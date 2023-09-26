#include "request-data.h"
#include "plugin-support.h"

#include <curl/curl.h>
#include <cstddef>
#include <string>
#include <regex>
#include <fstream>
#include <util/base.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

static const std::string USER_AGENT = std::string(PLUGIN_NAME) + "/" + std::string(PLUGIN_VERSION);

std::size_t writeFunctionStdString(void *ptr, std::size_t size, size_t nmemb, std::string *data)
{
	data->append(static_cast<char *>(ptr), size * nmemb);
	return size * nmemb;
}

std::size_t writeFunctionUint8Vector(void *ptr, std::size_t size, size_t nmemb,
				     std::vector<uint8_t> *data)
{
	data->insert(data->end(), static_cast<uint8_t *>(ptr),
		     static_cast<uint8_t *>(ptr) + size * nmemb);
	return size * nmemb;
}

struct request_data_handler_response parse_regex(struct request_data_handler_response response,
						 url_source_request_data *request_data)
{
	if (request_data->output_regex == "") {
		// Return the whole response body
		response.body_parsed = response.body;
	} else {
		// Parse the response as a regex
		std::regex regex(request_data->output_regex,
				 std::regex_constants::ECMAScript | std::regex_constants::optimize);
		std::smatch match;
		if (std::regex_search(response.body, match, regex)) {
			if (match.size() > 1) {
				response.body_parsed = match[1].str();
			} else {
				response.body_parsed = match[0].str();
			}
		} else {
			obs_log(LOG_INFO, "Failed to match regex");
			// Return an error response
			struct request_data_handler_response responseFail;
			responseFail.error_message = "Failed to match regex";
			responseFail.status_code = -1;
			return responseFail;
		}
	}
	return response;
}

struct request_data_handler_response parse_xml(struct request_data_handler_response response,
					       url_source_request_data *request_data)
{
	// Parse the response as XML using pugixml
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(response.body.c_str());
	if (!result) {
		obs_log(LOG_INFO, "Failed to parse XML response: %s", result.description());
		// Return an error response
		struct request_data_handler_response responseFail;
		responseFail.error_message = result.description();
		responseFail.status_code = -1;
		return responseFail;
	}
	// Get the output value
	if (request_data->output_xpath != "") {
		pugi::xpath_node_set nodes = doc.select_nodes(request_data->output_xpath.c_str());
		if (nodes.size() > 0) {
			response.body_parsed = nodes[0].node().text().get();
		} else {
			obs_log(LOG_INFO, "Failed to get XML value");
			// Return an error response
			struct request_data_handler_response responseFail;
			responseFail.error_message = "Failed to get XML value";
			responseFail.status_code = -1;
			return responseFail;
		}
	} else {
		// Return the whole XML object
		response.body_parsed = response.body;
	}
	return response;
}

struct request_data_handler_response parse_json(struct request_data_handler_response response,
						url_source_request_data *request_data)
{
	// Parse the response as JSON
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(response.body);
	} catch (nlohmann::json::parse_error &e) {
		obs_log(LOG_INFO, "Failed to parse JSON response: %s", e.what());
		// Return an error response
		struct request_data_handler_response responseFail;
		responseFail.error_message = e.what();
		responseFail.status_code = -1;
		return responseFail;
	}
	// Get the output value
	if (request_data->output_json_path != "") {
		try {
			const auto value = json.at(nlohmann::json::json_pointer(
							   request_data->output_json_path))
						   .get<nlohmann::json>();
			if (value.is_string()) {
				response.body_parsed = value.get<std::string>();
			} else {
				response.body_parsed = value.dump();
			}
			// remove potential prefix and postfix quotes, conversion from string
			if (response.body_parsed.size() > 1 &&
			    response.body_parsed.front() == '"' &&
			    response.body_parsed.back() == '"') {
				response.body_parsed = response.body_parsed.substr(
					1, response.body_parsed.size() - 2);
			}
		} catch (nlohmann::json::exception &e) {
			obs_log(LOG_INFO, "Failed to get JSON value: %s", e.what());
			// Return an error response
			struct request_data_handler_response responseFail;
			responseFail.error_message = e.what();
			responseFail.status_code = -1;
			return responseFail;
		}
	} else {
		// Return the whole JSON object
		response.body_parsed = json.dump();
	}
	return response;
}

struct request_data_handler_response request_data_handler(url_source_request_data *request_data)
{
	struct request_data_handler_response response;

	if (request_data->url_or_file == "file") {
		// This is a file request
		// Read the file
		std::ifstream file(request_data->url);
		if (!file.is_open()) {
			obs_log(LOG_INFO, "Failed to open file");
			// Return an error response
			response.error_message = "Failed to open file";
			response.status_code = -1;
			return response;
		}
		std::string responseBody((std::istreambuf_iterator<char>(file)),
					 std::istreambuf_iterator<char>());
		file.close();

		response.body = responseBody;
		response.status_code = 200;
	} else {
		// This is a URL request
		// Check if the URL is empty
		if (request_data->url == "") {
			obs_log(LOG_INFO, "URL is empty");
			// Return an error response
			response.error_message = "URL is empty";
			response.status_code = -1;
			return response;
		}
		// Build the request with libcurl
		CURL *curl = curl_easy_init();
		if (!curl) {
			obs_log(LOG_INFO, "Failed to initialize curl");
			// Return an error response
			response.error_message = "Failed to initialize curl";
			response.status_code = -1;
			return response;
		}
		CURLcode code;
		curl_easy_setopt(curl, CURLOPT_URL, request_data->url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunctionStdString);

		if (request_data->headers.size() > 0) {
			// Add request headers
			struct curl_slist *headers = NULL;
			for (auto header : request_data->headers) {
				std::string header_string = header.first + ": " + header.second;
				headers = curl_slist_append(headers, header_string.c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		}

		if (request_data->method == "POST") {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_data->body.c_str());
		} else if (request_data->method == "GET") {
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		}

		// SSL options
		if (request_data->ssl_client_cert_file != "") {
			curl_easy_setopt(curl, CURLOPT_SSLCERT, request_data->ssl_client_cert_file.c_str());
		}
		if (request_data->ssl_client_key_file != "") {
			curl_easy_setopt(curl, CURLOPT_SSLKEY, request_data->ssl_client_key_file.c_str());
		}
		if (request_data->ssl_client_key_pass != "") {
			curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD,
					request_data->ssl_client_key_pass.c_str());
		}
		if (!request_data->ssl_verify_peer) {
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		}

		std::string responseBody;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

		// Send the request
		code = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (code != CURLE_OK) {
			obs_log(LOG_INFO, "Failed to send request: %s", curl_easy_strerror(code));
			// Return an error response
			response.error_message = curl_easy_strerror(code);
			response.status_code = -1;
			return response;
		}

		response.body = responseBody;
		response.status_code = 200;
	}

	// Parse the response
	if (request_data->output_type == "JSON") {
		response = parse_json(response, request_data);
	} else if (request_data->output_type == "XML" || request_data->output_type == "HTML") {
		response = parse_xml(response, request_data);
	} else if (request_data->output_type == "Text") {
		response = parse_regex(response, request_data);
	} else {
		obs_log(LOG_INFO, "Invalid output type");
		// Return an error response
		struct request_data_handler_response responseFail;
		responseFail.error_message = "Invalid output type";
		responseFail.status_code = -1;
		return responseFail;
	}

	// Return the response
	return response;
}

std::string serialize_request_data(url_source_request_data *request_data)
{
	// Serialize the request data to a string using JSON
	nlohmann::json json;
	json["url"] = request_data->url;
	json["url_or_file"] = request_data->url_or_file;
	json["method"] = request_data->method;
	json["body"] = request_data->body;
	// SSL options
	json["ssl_client_cert_file"] = request_data->ssl_client_cert_file;
	json["ssl_client_key_file"] = request_data->ssl_client_key_file;
	json["ssl_client_key_pass"] = request_data->ssl_client_key_pass;
	// Output parsing options
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

url_source_request_data unserialize_request_data(std::string serialized_request_data)
{
	// Unserialize the request data from a string using JSON
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(serialized_request_data);
	} catch (nlohmann::json::parse_error &e) {
		obs_log(LOG_INFO, "Failed to parse JSON request data: %s", e.what());
		// Return an empty request data object
		url_source_request_data request_data;
		return request_data;
	}

	url_source_request_data request_data;
	request_data.url = json["url"].get<std::string>();
	if (json.contains("url_or_file")) { // backwards compatibility
		request_data.url_or_file = json["url_or_file"].get<std::string>();
	} else {
		request_data.url_or_file = "url";
	}
	request_data.method = json["method"].get<std::string>();
	request_data.body = json["body"].get<std::string>();
	// SSL options
	if (json.contains("ssl_client_cert_file")) {
		request_data.ssl_client_cert_file = json["ssl_client_cert_file"].get<std::string>();
	}
	if (json.contains("ssl_client_key_file")) {
		request_data.ssl_client_key_file = json["ssl_client_key_file"].get<std::string>();
	}
	if (json.contains("ssl_client_key_pass")) {
		request_data.ssl_client_key_pass = json["ssl_client_key_pass"].get<std::string>();
	}
	// Output parsing options
	request_data.output_type = json["output_type"].get<std::string>();
	request_data.output_json_path = json["output_json_path"].get<std::string>();
	request_data.output_xpath = json["output_xpath"].get<std::string>();
	request_data.output_regex = json["output_regex"].get<std::string>();
	request_data.output_regex_flags = json["output_regex_flags"].get<std::string>();
	request_data.output_regex_group = json["output_regex_group"].get<std::string>();

	nlohmann::json headers_json = json["headers"];
	for (auto header : headers_json.items()) {
		request_data.headers.push_back(
			std::make_pair(header.key(), header.value().get<std::string>()));
	}

	return request_data;
}

// Fetch image from url and get bytes
std::vector<uint8_t> fetch_image(std::string url)
{
	// Build the request with libcurl
	CURL *curl = curl_easy_init();
	if (!curl) {
		obs_log(LOG_INFO, "Failed to initialize curl");
		// Return an error response
		std::vector<uint8_t> responseFail;
		return responseFail;
	}
	CURLcode code;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunctionUint8Vector);

	std::vector<uint8_t> responseBody;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

	// Send the request
	code = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (code != CURLE_OK) {
		obs_log(LOG_INFO, "Failed to send request: %s", curl_easy_strerror(code));
		// Return an error response
		std::vector<uint8_t> responseFail;
		return responseFail;
	}

	return responseBody;
}

// BASE64_CHARS
static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz"
					"0123456789+/";

// encode bytes to base64
std::string base64_encode(const std::vector<uint8_t> &bytes)
{
	std::string encoded;
	encoded.resize(((bytes.size() + 2) / 3) * 4);
	auto it = encoded.begin();
	for (size_t i = 0; i < bytes.size(); i += 3) {
		size_t group = 0;
		group |= (size_t)bytes[i] << 16;
		if (i + 1 < bytes.size()) {
			group |= (size_t)bytes[i + 1] << 8;
		}
		if (i + 2 < bytes.size()) {
			group |= (size_t)bytes[i + 2];
		}
		for (size_t j = 0; j < 4; j++) {
			if (i * 8 / 6 + j < encoded.size()) {
				*it++ = BASE64_CHARS[(group >> 6 * (3 - j)) & 0x3f];
			} else {
				*it++ = '=';
			}
		}
	}
	return encoded;
}
