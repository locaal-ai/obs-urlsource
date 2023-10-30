#include <inja/inja.hpp>

#include "request-data.h"
#include "plugin-support.h"
#include "parsers/parsers.h"

#include <cstddef>
#include <string>
#include <fstream>
#include <regex>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <util/base.h>
#include <obs-module.h>

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
			response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
			return response;
		}
		std::string responseBody((std::istreambuf_iterator<char>(file)),
					 std::istreambuf_iterator<char>());
		file.close();

		response.body = responseBody;
		response.status_code = URL_SOURCE_REQUEST_SUCCESS;
	} else {
		// This is a URL request
		// Check if the URL is empty
		if (request_data->url == "") {
			obs_log(LOG_INFO, "URL is empty");
			// Return an error response
			response.error_message = "URL is empty";
			response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
			return response;
		}
		// Build the request with libcurl
		CURL *curl = curl_easy_init();
		if (!curl) {
			obs_log(LOG_INFO, "Failed to initialize curl");
			// Return an error response
			response.error_message = "Failed to initialize curl";
			response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
			return response;
		}
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

		nlohmann::json json; // json object or variables for inja

		// If dynamic data source is set, replace the {input} placeholder with the source text
		if (request_data->obs_text_source != "") {
			// Get text from OBS text source
			obs_data_t *sourceSettings = obs_source_get_settings(
				obs_get_source_by_name(request_data->obs_text_source.c_str()));
			const char *text = obs_data_get_string(sourceSettings, "text");
			obs_data_release(sourceSettings);
			if (request_data->obs_text_source_skip_if_empty) {
				if (text == NULL || text[0] == '\0') {
					// Return an error response
					response.error_message =
						"OBS text source is empty, skipping was requested";
					response.status_code = URL_SOURCE_REQUEST_BENIGN_ERROR_CODE;
					return response;
				}
			}
			if (request_data->obs_text_source_skip_if_same) {
				if (request_data->last_obs_text_source_value == text) {
					// Return an error response
					response.error_message =
						"OBS text source value is the same as last time, skipping was requested";
					response.status_code = URL_SOURCE_REQUEST_BENIGN_ERROR_CODE;
					return response;
				}
				request_data->last_obs_text_source_value = text;
			}
			json["input"] = text;
		}

		// Replace the {input} placeholder with the source text
		inja::Environment env;
		// Add an inja callback for time formatting
		env.add_callback("strftime", 1, [](inja::Arguments &args) {
			std::string format = args.at(0)->get<std::string>();
			std::time_t t = std::time(nullptr);
			std::tm *tm = std::localtime(&t);
			char buffer[256];
			std::strftime(buffer, sizeof(buffer), format.c_str(), tm);
			return std::string(buffer);
		});
		// add a callback for escaping strings in the querystring
		env.add_callback("urlencode", 1, [&curl](inja::Arguments &args) {
			std::string input = args.at(0)->get<std::string>();
			char *escaped = curl_easy_escape(curl, input.c_str(), 0);
			input = std::string(escaped);
			curl_free(escaped);
			return input;
		});

		// Replace the {input} placeholder in the querystring as well
		std::string url = request_data->url;
		try {
			url = env.render(url, json);
		} catch (std::exception &e) {
			obs_log(LOG_WARNING, "Failed to render URL template: %s", e.what());
		}
		response.request_url = url;

		if (request_data->url != url) { // If the url was changed
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		}

		// this is needed here, out of the `if` scope below
		std::string request_body_allocated;

		if (request_data->method == "POST") {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			try {
				request_body_allocated = env.render(request_data->body, json);
			} catch (std::exception &e) {
				obs_log(LOG_WARNING, "Failed to render Body template: %s",
					e.what());
			}
			response.request_body = request_body_allocated;
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body_allocated.c_str());
		} else if (request_data->method == "GET") {
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		}

		// SSL options
		if (request_data->ssl_client_cert_file != "") {
			curl_easy_setopt(curl, CURLOPT_SSLCERT,
					 request_data->ssl_client_cert_file.c_str());
		}
		if (request_data->ssl_client_key_file != "") {
			curl_easy_setopt(curl, CURLOPT_SSLKEY,
					 request_data->ssl_client_key_file.c_str());
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
		CURLcode code = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (code != CURLE_OK) {
			obs_log(LOG_INFO, "Failed to send request: %s", curl_easy_strerror(code));
			obs_log(LOG_INFO, "Request URL: %s", url.c_str());
			// Return an error response
			response.error_message = curl_easy_strerror(code);
			response.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
			return response;
		}

		response.body = responseBody;
		response.status_code = URL_SOURCE_REQUEST_SUCCESS;
	}

	// Parse the response
	if (request_data->output_type == "JSON") {
		if (request_data->output_json_path != "") {
			response = parse_json_path(response, request_data);
		} else if (request_data->output_json_pointer != "") {
			response = parse_json_pointer(response, request_data);
		} else {
			// attempt to parse as json and return the whole object
			response = parse_json(response, request_data);
		}
	} else if (request_data->output_type == "XML (XPath)") {
		response = parse_xml(response, request_data);
	} else if (request_data->output_type == "XML (XQuery)") {
		response = parse_xml_by_xquery(response, request_data);
	} else if (request_data->output_type == "HTML") {
		response = parse_html(response, request_data);
	} else if (request_data->output_type == "Text") {
		response = parse_regex(response, request_data);
	} else {
		obs_log(LOG_INFO, "Invalid output type");
		// Return an error response
		struct request_data_handler_response responseFail;
		responseFail.error_message = "Invalid output type";
		responseFail.status_code = URL_SOURCE_REQUEST_STANDARD_ERROR_CODE;
		return responseFail;
	}

	// If output regex is set - use it to format the output in response.body_parts_parsed
	if (!request_data->post_process_regex.empty()) {
		try {
			std::regex regex(request_data->post_process_regex);
			// for each part of the response body - apply the regex
			for (size_t i = 0; i < response.body_parts_parsed.size(); i++) {
				if (request_data->post_process_regex_is_replace) {
					// replace the whole string with the regex replace string
					response.body_parts_parsed[i] = std::regex_replace(
						response.body_parts_parsed[i], regex,
						request_data->post_process_regex_replace);
				} else {
					std::smatch match;
					if (std::regex_search(response.body_parts_parsed[i], match,
							      regex)) {
						if (match.size() > 1) {
							// replace the whole string with the first capture group
							response.body_parts_parsed[i] = match[1];
						}
					}
				}
			}
		} catch (std::regex_error &e) {
			obs_log(LOG_ERROR, "Failed to parse output_regex: %s", e.what());
		}
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
	json["obs_text_source"] = request_data->obs_text_source;
	json["obs_text_source_skip_if_empty"] = request_data->obs_text_source_skip_if_empty;
	json["obs_text_source_skip_if_same"] = request_data->obs_text_source_skip_if_same;
	// SSL options
	json["ssl_client_cert_file"] = request_data->ssl_client_cert_file;
	json["ssl_client_key_file"] = request_data->ssl_client_key_file;
	json["ssl_client_key_pass"] = request_data->ssl_client_key_pass;
	// Output parsing options
	json["output_type"] = request_data->output_type;
	json["output_json_path"] = request_data->output_json_path;
	json["output_json_pointer"] = request_data->output_json_pointer;
	json["output_xpath"] = request_data->output_xpath;
	json["output_xquery"] = request_data->output_xquery;
	json["output_regex"] = request_data->output_regex;
	json["output_regex_flags"] = request_data->output_regex_flags;
	json["output_regex_group"] = request_data->output_regex_group;
	// postprocess options
	json["post_process_regex"] = request_data->post_process_regex;
	json["post_process_regex_is_replace"] = request_data->post_process_regex_is_replace;
	json["post_process_regex_replace"] = request_data->post_process_regex_replace;

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
	// throughout this function we use .contains() to check if a key exists to avoid
	// exceptions
	url_source_request_data request_data;
	try {
		nlohmann::json json;
		json = nlohmann::json::parse(serialized_request_data);

		request_data.url = json["url"].get<std::string>();
		if (json.contains("url_or_file")) {
			request_data.url_or_file = json["url_or_file"].get<std::string>();
		} else {
			request_data.url_or_file = "url";
		}
		request_data.method = json["method"].get<std::string>();
		request_data.body = json["body"].get<std::string>();
		if (json.contains("obs_text_source")) {
			request_data.obs_text_source = json["obs_text_source"].get<std::string>();
		} else {
			request_data.obs_text_source = "";
		}
		if (json.contains("obs_text_source_skip_if_empty")) {
			request_data.obs_text_source_skip_if_empty =
				json["obs_text_source_skip_if_empty"].get<bool>();
		} else {
			request_data.obs_text_source_skip_if_empty = false;
		}
		if (json.contains("obs_text_source_skip_if_same")) {
			request_data.obs_text_source_skip_if_same =
				json["obs_text_source_skip_if_same"].get<bool>();
		} else {
			request_data.obs_text_source_skip_if_same = false;
		}

		// SSL options
		if (json.contains("ssl_client_cert_file")) {
			request_data.ssl_client_cert_file =
				json["ssl_client_cert_file"].get<std::string>();
		}
		if (json.contains("ssl_client_key_file")) {
			request_data.ssl_client_key_file =
				json["ssl_client_key_file"].get<std::string>();
		}
		if (json.contains("ssl_client_key_pass")) {
			request_data.ssl_client_key_pass =
				json["ssl_client_key_pass"].get<std::string>();
		}
		// Output parsing options
		request_data.output_type = json["output_type"].get<std::string>();
		if (json.contains("output_json_path")) {
			request_data.output_json_path = json["output_json_path"].get<std::string>();
			if (request_data.output_json_path != "" &&
			    request_data.output_json_path[0] == '/') {
				obs_log(LOG_WARNING,
					"JSON pointer detected in JSON Path. Translating to JSON "
					"path.");
				// this is a json pointer - translate by adding a "$" prefix and replacing all
				// "/"s with "."
				request_data.output_json_path =
					"$." + request_data.output_json_path.substr(1);
				std::replace(request_data.output_json_path.begin(),
					     request_data.output_json_path.end(), '/', '.');
			}
		}
		if (json.contains("output_json_pointer")) {
			request_data.output_json_pointer =
				json["output_json_pointer"].get<std::string>();
			if (request_data.output_json_pointer != "" &&
			    request_data.output_json_pointer[0] == '$') {
				obs_log(LOG_WARNING,
					"JSON path detected in JSON Pointer. Translating to JSON "
					"pointer.");
				// this is a json path - translate by replacing all "."s with "/"s
				std::replace(request_data.output_json_pointer.begin(),
					     request_data.output_json_pointer.end(), '.', '/');
				request_data.output_json_pointer =
					"/" + request_data.output_json_pointer;
			}
		}
		request_data.output_xpath = json["output_xpath"].get<std::string>();
		request_data.output_xquery = json["output_xquery"].get<std::string>();
		request_data.output_regex = json["output_regex"].get<std::string>();
		request_data.output_regex_flags = json["output_regex_flags"].get<std::string>();
		request_data.output_regex_group = json["output_regex_group"].get<std::string>();

		// postprocess options
		if (json.contains("post_process_regex")) {
			request_data.post_process_regex =
				json["post_process_regex"].get<std::string>();
		}
		if (json.contains("post_process_regex_is_replace")) {
			request_data.post_process_regex_is_replace =
				json["post_process_regex_is_replace"].get<bool>();
		}
		if (json.contains("post_process_regex_replace")) {
			request_data.post_process_regex_replace =
				json["post_process_regex_replace"].get<std::string>();
		}

		nlohmann::json headers_json = json["headers"];
		for (auto header : headers_json.items()) {
			request_data.headers.push_back(
				std::make_pair(header.key(), header.value().get<std::string>()));
		}

	} catch (const std::exception &e) {
		obs_log(LOG_WARNING,
			"Failed to parse JSON request data. Saved request "
			"properties are reset. Error: %s",
			e.what());
		// Return an empty request data object
		return request_data;
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
