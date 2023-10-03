#ifndef REQUEST_DATA_H
#define REQUEST_DATA_H

#include <string>
#include <vector>

#define URL_SOURCE_REQUEST_STANDARD_ERROR_CODE -1
#define URL_SOURCE_REQUEST_BENIGN_ERROR_CODE -2
#define URL_SOURCE_REQUEST_PARSING_ERROR_CODE -3

struct url_source_request_data {
	std::string url;
	std::string url_or_file;
	std::string method;
	std::string body;
	std::string obs_text_source;
	bool obs_text_source_skip_if_empty;
	bool obs_text_source_skip_if_same;
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
		last_obs_text_source_value = std::string("");
		headers = {};
		output_type = std::string("text");
		output_json_path = std::string("");
		output_json_pointer = std::string("");
		output_xpath = std::string("");
		output_xquery = std::string("");
		output_regex = std::string("");
		output_regex_flags = std::string("");
		output_regex_group = std::string("0");
	}

	// Copy constructor should duplicate all data
	url_source_request_data(const url_source_request_data &other)
	{
		url = std::string(other.url);
		url_or_file = std::string(other.url_or_file);
		method = std::string(other.method);
		body = std::string(other.body);
		obs_text_source = std::string(other.obs_text_source);
		obs_text_source_skip_if_empty = other.obs_text_source_skip_if_empty;
		obs_text_source_skip_if_same = other.obs_text_source_skip_if_same;
		last_obs_text_source_value = std::string(other.last_obs_text_source_value);
		headers = std::vector<std::pair<std::string, std::string>>(other.headers);
		output_type = std::string(other.output_type);
		output_json_path = std::string(other.output_json_path);
		output_json_pointer = std::string(other.output_json_pointer);
		output_xpath = std::string(other.output_xpath);
		output_xquery = std::string(other.output_xquery);
		output_regex = std::string(other.output_regex);
		output_regex_flags = std::string(other.output_regex_flags);
		output_regex_group = std::string(other.output_regex_group);
	}

	// Copy assignment operator should duplicate all data
	url_source_request_data &operator=(const url_source_request_data &other)
	{
		url = std::string(other.url);
		url_or_file = std::string(other.url_or_file);
		method = std::string(other.method);
		body = std::string(other.body);
		obs_text_source = std::string(other.obs_text_source);
		obs_text_source_skip_if_empty = other.obs_text_source_skip_if_empty;
		obs_text_source_skip_if_same = other.obs_text_source_skip_if_same;
		last_obs_text_source_value = std::string(other.last_obs_text_source_value);
		headers = std::vector<std::pair<std::string, std::string>>(other.headers);
		output_type = std::string(other.output_type);
		output_json_path = std::string(other.output_json_path);
		output_json_pointer = std::string(other.output_json_pointer);
		output_xpath = std::string(other.output_xpath);
		output_xquery = std::string(other.output_xquery);
		output_regex = std::string(other.output_regex);
		output_regex_flags = std::string(other.output_regex_flags);
		output_regex_group = std::string(other.output_regex_group);
		return *this;
	}
};

struct request_data_handler_response {
	std::string body;
	std::string content_type;
	std::vector<std::string> body_parts_parsed;
	std::vector<std::pair<std::string, std::string>> headers;
	int status_code;
	std::string status_message;
	std::string error_message;
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
