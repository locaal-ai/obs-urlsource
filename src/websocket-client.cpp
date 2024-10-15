
#pragma warning(disable : 4267)

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>

#include <thread>
#include <functional>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "request-data.h"
#include "websocket-client.h"

typedef websocketpp::client<websocketpp::config::asio> ws_client;

struct WebSocketClientWrapper {
	ws_client client;
	websocketpp::connection_hdl connection;
	std::thread asio_thread;
	std::string last_received_message;

	WebSocketClientWrapper()
	{
		client.init_asio();
		client.set_message_handler(std::bind(&WebSocketClientWrapper::on_message, this,
						     std::placeholders::_1, std::placeholders::_2));
	}

	~WebSocketClientWrapper()
	{
		if (asio_thread.joinable()) {
			client.stop();
			asio_thread.join();
		}
	}

	void on_message(websocketpp::connection_hdl, ws_client::message_ptr msg)
	{
		last_received_message = msg->get_payload();
	}

	bool connect(const std::string &uri)
	{
		websocketpp::lib::error_code ec;
		ws_client::connection_ptr con = client.get_connection(uri, ec);
		if (ec) {
			return false;
		}

		client.connect(con);
		connection = con->get_handle();

		asio_thread = std::thread([this]() { client.run(); });
		return true;
	}

	bool send(const std::string &message)
	{
		websocketpp::lib::error_code ec;
		client.send(connection, message, websocketpp::frame::opcode::text, ec);
		return !ec;
	}
};

std::string create_message_body(url_source_request_data *request_data)
{
	// Implement this function based on your requirements
	return request_data->body;
}

struct request_data_handler_response
websocket_request_handler(url_source_request_data *request_data)
{
	request_data_handler_response response;

	if (!request_data->is_websocket) {
		response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
		response.error_message = "Not a WebSocket request";
		return response;
	}

	if (!request_data->ws_connected) {
		if (!request_data->ws_client_wrapper) {
			request_data->ws_client_wrapper = new WebSocketClientWrapper();
		}

		if (!request_data->ws_client_wrapper->connect(request_data->url)) {
			response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
			response.error_message = "Could not create WebSocket connection";
			return response;
		}

		request_data->ws_connected = true;
	}

	try {
		nlohmann::json json; // json object or variables for inja
		inja::Environment env;
		prepare_inja_env(&env, request_data, response, json);

		if (response.status_code != URL_SOURCE_REQUEST_SUCCESS) {
			return response;
		}

		// Assume you have a method to create the message body
		std::string message = env.render(request_data->body, json);

		if (!request_data->ws_client_wrapper->send(message)) {
			throw std::runtime_error("Failed to send WebSocket message");
		}

		// Wait for the response (you might want to implement a timeout mechanism)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		response.body = request_data->ws_client_wrapper->last_received_message;
		response.status_code = URL_SOURCE_REQUEST_SUCCESS;
	} catch (const std::exception &e) {
		response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
		response.error_message =
			"Error handling WebSocket request: " + std::string(e.what());
	}

	return response;
}
