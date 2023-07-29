#include "request-data.h"
#include "plugin-support.h"

#include <curl/curl.h>
#include <cstddef>
#include <string>
#include <util/base.h>
#include <nlohmann/json.hpp>

static const std::string USER_AGENT =
	std::string(PLUGIN_NAME) + "/" + std::string(PLUGIN_VERSION);

std::size_t writeFunctionStdString(void *ptr, std::size_t size, size_t nmemb,
				   std::string *data)
{
	data->append(static_cast<char *>(ptr), size * nmemb);
	return size * nmemb;
}

struct request_data_handler_response
request_data_handler(url_source_request_data *request_data)
{
	// Build the request with libcurl
	CURL *curl = curl_easy_init();
	if (!curl) {
		obs_log(LOG_INFO, "Failed to initialize curl");
		// Return an error response
		struct request_data_handler_response responseFail;
		responseFail.error_message = "Failed to initialize curl";
		responseFail.status_code = -1;
		return responseFail;
	}
	CURLcode code;
	curl_easy_setopt(curl, CURLOPT_URL, request_data->url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunctionStdString);

	if (request_data->headers.size() > 0) {
		// Add request headers
		struct curl_slist *headers = NULL;
		for (auto header : request_data->headers) {
			std::string header_string =
				header.first + ": " + header.second;
			headers = curl_slist_append(headers,
						    header_string.c_str());
		}
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	std::string responseBody;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

	// Send the request
	code = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (code != CURLE_OK) {
		obs_log(LOG_INFO, "Failed to send request: %s",
			curl_easy_strerror(code));
		// Return an error response
		struct request_data_handler_response responseFail;
		responseFail.error_message = curl_easy_strerror(code);
		responseFail.status_code = -1;
		return responseFail;
	}

	struct request_data_handler_response response;
	response.body = responseBody;
	response.status_code = 200;

	// Parse the response
	if (request_data->output_type == "JSON") {
		// Parse the response as JSON
		nlohmann::json json;
		try {
			json = nlohmann::json::parse(responseBody);
		} catch (nlohmann::json::parse_error &e) {
			obs_log(LOG_INFO, "Failed to parse JSON response: %s",
				e.what());
			// Return an error response
			struct request_data_handler_response responseFail;
			responseFail.error_message = e.what();
			responseFail.status_code = -1;
			return responseFail;
		}
		// Get the output value
		if (request_data->output_json_path != "") {
			try {
				response.body_parsed =
					json.at(nlohmann::json::json_pointer(
							request_data
								->output_json_path))
						.get<std::string>();
			} catch (nlohmann::json::exception &e) {
				obs_log(LOG_INFO,
					"Failed to get JSON value: %s",
					e.what());
				// Return an error response
				struct request_data_handler_response
					responseFail;
				responseFail.error_message = e.what();
				responseFail.status_code = -1;
				return responseFail;
			}
		} else {
			// Return the whole JSON object
			response.body_parsed = json.dump();
		}
	}

	// Return the response
	return response;
}

std::string serialize_request_data(url_source_request_data *request_data)
{
	obs_log(LOG_INFO, "Serializing request data");
	// Serialize the request data to a string using JSON
	nlohmann::json json;
	json["url"] = request_data->url;
	json["method"] = request_data->method;
	json["body"] = request_data->body;
	json["output_type"] = request_data->output_type;
	json["output_json_path"] = request_data->output_json_path;
	json["output_xpath"] = request_data->output_xpath;
	json["output_regex"] = request_data->output_regex;
	json["output_regex_flags"] = request_data->output_regex_flags;
	json["output_regex_group"] = request_data->output_regex_group;

	nlohmann::json headers_json;
	for (auto header : request_data->headers) {
		headers_json[header.first] = header.second;
	}
	json["headers"] = headers_json;

	return json.dump();
}

url_source_request_data
unserialize_request_data(std::string serialized_request_data)
{
	obs_log(LOG_INFO, "Unserializing request data");
	// Unserialize the request data from a string using JSON
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(serialized_request_data);
	} catch (nlohmann::json::parse_error &e) {
		obs_log(LOG_INFO, "Failed to parse JSON request data: %s",
			e.what());
		// Return an empty request data object
		url_source_request_data request_data;
		return request_data;
	}

	url_source_request_data request_data;
	request_data.url = json["url"].get<std::string>();
	request_data.method = json["method"].get<std::string>();
	request_data.body = json["body"].get<std::string>();
	request_data.output_type = json["output_type"].get<std::string>();
	request_data.output_json_path =
		json["output_json_path"].get<std::string>();
	request_data.output_xpath = json["output_xpath"].get<std::string>();
	request_data.output_regex = json["output_regex"].get<std::string>();
	request_data.output_regex_flags =
		json["output_regex_flags"].get<std::string>();
	request_data.output_regex_group =
		json["output_regex_group"].get<std::string>();

	nlohmann::json headers_json = json["headers"];
	for (auto header : headers_json.items()) {
		request_data.headers.push_back(std::make_pair(
			header.key(), header.value().get<std::string>()));
	}

	return request_data;
}