#ifndef REQUEST_DATA_H
#define REQUEST_DATA_H

#include <string>
#include <vector>

struct url_source_request_data {
	std::string url;
	std::string method;
	std::string body;
	// Request headers
	std::vector<std::pair<std::string, std::string>> headers;
	// Output parsing options
	std::string output_type;
	std::string output_json_path;
	std::string output_xpath;
	std::string output_regex;
	std::string output_regex_flags;
	std::string output_regex_group;
};

struct request_data_handler_response {
	std::string body;
	std::string content_type;
	std::string body_parsed;
	std::vector<std::pair<std::string, std::string>> headers;
	int status_code;
	std::string status_message;
	std::string error_message;
};

struct request_data_handler_response request_data_handler(url_source_request_data *request_data);

std::string serialize_request_data(url_source_request_data *request_data);

url_source_request_data unserialize_request_data(std::string serialized_request_data);

// Fetch image from url and get bytes
std::vector<uint8_t> fetch_image(std::string url);

// encode bytes to base64
std::string base64_encode(const std::vector<uint8_t> &bytes);

#endif // REQUEST_DATA_H
