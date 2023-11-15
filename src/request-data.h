#ifndef REQUEST_DATA_H
#define REQUEST_DATA_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#define URL_SOURCE_REQUEST_STANDARD_ERROR_CODE -1
#define URL_SOURCE_REQUEST_BENIGN_ERROR_CODE -2
#define URL_SOURCE_REQUEST_PARSING_ERROR_CODE -3
#define URL_SOURCE_REQUEST_SUCCESS 0

struct url_source_request_data {
	std::string url;
	std::string url_or_file;
	std::string method;
	std::string body;
	std::string obs_text_source;
	bool obs_text_source_skip_if_empty;
	bool obs_text_source_skip_if_same;
	bool aggregate_to_empty;
	std::string aggregate_to_empty_buffer;
	std::string last_obs_text_source_value;
	// SSL options
	std::string ssl_client_cert_file;
	std::string ssl_client_key_file;
	std::string ssl_client_key_pass;
	bool ssl_verify_peer;
	// Request headers
	std::vector<std::pair<std::string, std::string>> headers;
	// Output parsing options
	std::string output_type;
	std::string output_json_path;
	std::string output_json_pointer;
	std::string output_xpath;
	std::string output_xquery;
	std::string output_regex;
	std::string output_regex_flags;
	std::string output_regex_group;
	std::string output_cssselector;
	// post process options
	std::string post_process_regex;
	bool post_process_regex_is_replace;
	std::string post_process_regex_replace;

	// default constructor
	url_source_request_data()
	{
		url = std::string("");
		url_or_file = std::string("url");
		method = std::string("GET");
		body = std::string("");
		obs_text_source = std::string("");
		obs_text_source_skip_if_empty = false;
		obs_text_source_skip_if_same = false;
		aggregate_to_empty = false;
		aggregate_to_empty_buffer = std::string("");
		last_obs_text_source_value = std::string("");
		ssl_verify_peer = false;
		headers = {};
		output_type = std::string("text");
		output_json_path = std::string("");
		output_json_pointer = std::string("");
		output_xpath = std::string("");
		output_xquery = std::string("");
		output_regex = std::string("");
		output_regex_flags = std::string("");
		output_regex_group = std::string("0");
		output_cssselector = std::string("");
	}
};

struct request_data_handler_response {
	std::string body;
	std::vector<uint8_t> body_bytes;
	nlohmann::json body_json;
	std::string content_type;
	std::vector<std::string> body_parts_parsed;
	std::map<std::string, std::string> headers;
	int status_code;
	std::string status_message;
	std::string error_message;
	std::string request_url;
	std::string request_body;
};

struct request_data_handler_response request_data_handler(url_source_request_data *request_data);

std::string serialize_request_data(url_source_request_data *request_data);

url_source_request_data unserialize_request_data(std::string serialized_request_data);

// Fetch image from url and get bytes
std::vector<uint8_t> fetch_image(std::string url);

// encode bytes to base64
std::string base64_encode(const std::vector<uint8_t> &bytes);

#endif // REQUEST_DATA_H
