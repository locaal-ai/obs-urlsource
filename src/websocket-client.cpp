
#pragma warning(disable : 4267)

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "request-data.h"
#include "websocket-client.h"
#include "plugin-support.h"

#include <util/base.h>

typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

struct WebSocketClientWrapper {
	ws_client client;
	websocketpp::connection_hdl connection;
	std::unique_ptr<std::thread> asio_thread;
	std::string last_received_message;
	std::atomic<bool> is_connected{false};
	std::mutex mutex;
	std::condition_variable cv;

	WebSocketClientWrapper()
	{
		try {
			client.clear_access_channels(websocketpp::log::alevel::all);
			client.clear_error_channels(websocketpp::log::elevel::all);

			client.init_asio();
			client.start_perpetual();

			client.set_message_handler(std::bind(&WebSocketClientWrapper::on_message,
							     this, std::placeholders::_1,
							     std::placeholders::_2));

			client.set_open_handler(std::bind(&WebSocketClientWrapper::on_open, this,
							  std::placeholders::_1));

			client.set_close_handler(std::bind(&WebSocketClientWrapper::on_close, this,
							   std::placeholders::_1));

			asio_thread = std::make_unique<std::thread>(&ws_client::run, &client);
		} catch (const std::exception &e) {
			// Log the error or handle it appropriately
			throw std::runtime_error("Failed to initialize WebSocket client: " +
						 std::string(e.what()));
		}
	}

	~WebSocketClientWrapper()
	{
		if (is_connected.load()) {
			close();
		}
		client.stop_perpetual();
		if (asio_thread && asio_thread->joinable()) {
			asio_thread->join();
		}
	}

	void on_message(websocketpp::connection_hdl, ws_client::message_ptr msg)
	{
		std::lock_guard<std::mutex> lock(mutex);
		last_received_message = msg->get_payload();
		cv.notify_one();
	}

	void on_open(websocketpp::connection_hdl hdl)
	{
		connection = hdl;
		is_connected.store(true);
		cv.notify_one();
	}

	void on_close(websocketpp::connection_hdl)
	{
		is_connected.store(false);
		cv.notify_one();
	}

	bool connect(const std::string &uri)
	{
		websocketpp::lib::error_code ec;
		ws_client::connection_ptr con = client.get_connection(uri, ec);
		if (ec) {
			return false;
		}

		client.connect(con);

		std::unique_lock<std::mutex> lock(mutex);
		return cv.wait_for(lock, std::chrono::seconds(5),
				   [this] { return is_connected.load(); });
	}

	bool send(const std::string &message)
	{
		if (!is_connected.load()) {
			return false;
		}

		websocketpp::lib::error_code ec;
		client.send(connection, message, websocketpp::frame::opcode::text, ec);
		return !ec;
	}

	bool receive(std::string &message, std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (cv.wait_for(lock, timeout, [this] { return !last_received_message.empty(); })) {
			message = std::move(last_received_message);
			last_received_message.clear();
			return true;
		}
		return false;
	}

	void close()
	{
		if (is_connected.load()) {
			websocketpp::lib::error_code ec;
			client.close(connection, websocketpp::close::status::normal,
				     "Closing connection", ec);
			if (ec) {
				// Handle error
				obs_log(LOG_WARNING, "Failed to close WebSocket connection: %s",
					ec.message().c_str());
			}
		}
	}
};

struct request_data_handler_response
websocket_request_handler(url_source_request_data *request_data)
{
	request_data_handler_response response;

	try {
		if (!request_data->ws_client_wrapper) {
			request_data->ws_client_wrapper = new WebSocketClientWrapper();
		}

		if (!request_data->ws_connected) {
			if (!request_data->ws_client_wrapper->connect(request_data->url)) {
				throw std::runtime_error("Could not create WebSocket connection");
			}
			request_data->ws_connected = true;
		}

		nlohmann::json json; // json object or variables for inja
		inja::Environment env;
		prepare_inja_env(&env, request_data, response, json);

		if (response.status_code != URL_SOURCE_REQUEST_SUCCESS) {
			return response;
		}

		std::string message = env.render(request_data->body, json);

		if (!request_data->ws_client_wrapper->send(message)) {
			throw std::runtime_error("Failed to send WebSocket message");
		}

		std::string received_message;
		if (request_data->ws_client_wrapper->receive(received_message,
							     std::chrono::milliseconds(5000))) {
			response.body = std::move(received_message);
			response.status_code = URL_SOURCE_REQUEST_SUCCESS;
		} else {
			throw std::runtime_error("Timeout waiting for WebSocket response");
		}
	} catch (const std::exception &e) {
		response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
		response.error_message =
			"Error handling WebSocket request: " + std::string(e.what());
	}

	return response;
}
