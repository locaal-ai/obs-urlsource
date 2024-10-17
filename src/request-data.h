#ifndef REQUEST_DATA_H
#define REQUEST_DATA_H

#include <string>
#include <vector>
#include <map>
#include <chrono>

#include <nlohmann/json.hpp>

#include "mapping-data.h"

#define URL_SOURCE_REQUEST_STANDARD_ERROR_CODE -1
#define URL_SOURCE_REQUEST_BENIGN_ERROR_CODE -2
#define URL_SOURCE_REQUEST_PARSING_ERROR_CODE -3
#define URL_SOURCE_REQUEST_SUCCESS 0

#define URL_SOURCE_AGG_TARGET_NONE -1
#define URL_SOURCE_AGG_TARGET_EMPTY 0
#define URL_SOURCE_AGG_TARGET_1MIN 1
#define URL_SOURCE_AGG_TARGET_2MIN 2
#define URL_SOURCE_AGG_TARGET_5MIN 3
#define URL_SOURCE_AGG_TARGET_10MIN 4
#define URL_SOURCE_AGG_TARGET_30s 5

inline std::string url_source_agg_target_to_string(int agg_target)
{
	switch (agg_target) {
	case URL_SOURCE_AGG_TARGET_1MIN:
		return "for 1m";
	case URL_SOURCE_AGG_TARGET_2MIN:
		return "for 2m";
	case URL_SOURCE_AGG_TARGET_5MIN:
		return "for 5m";
	case URL_SOURCE_AGG_TARGET_10MIN:
		return "for 10m";
	case URL_SOURCE_AGG_TARGET_30s:
		return "for 30s";
	default:
		return "to Empty";
	}
}

inline uint64_t url_source_agg_target_to_nanoseconds(int agg_target)
{
	switch (agg_target) {
	case URL_SOURCE_AGG_TARGET_1MIN:
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1))
			.count();
	case URL_SOURCE_AGG_TARGET_2MIN:
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(2))
			.count();
	case URL_SOURCE_AGG_TARGET_5MIN:
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(5))
			.count();
	case URL_SOURCE_AGG_TARGET_10MIN:
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			       std::chrono::minutes(10))
			.count();
	case URL_SOURCE_AGG_TARGET_30s:
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			       std::chrono::seconds(30))
			.count();
	default:
		return 0;
	}
}

inline int url_source_agg_target_string_to_enum(const std::string &agg_target)
{
	if (agg_target == "for 1m") {
		return URL_SOURCE_AGG_TARGET_1MIN;
	} else if (agg_target == "for 2m") {
		return URL_SOURCE_AGG_TARGET_2MIN;
	} else if (agg_target == "for 5m") {
		return URL_SOURCE_AGG_TARGET_5MIN;
	} else if (agg_target == "for 10m") {
		return URL_SOURCE_AGG_TARGET_10MIN;
	} else if (agg_target == "for 30s") {
		return URL_SOURCE_AGG_TARGET_30s;
	} else {
		return URL_SOURCE_AGG_TARGET_EMPTY;
	}
}

struct WebSocketClientWrapper; // Forward declaration

struct url_source_request_data {
	std::string source_name;
	std::string url;
	std::string url_or_file;
	std::string method;
	bool fail_on_http_error;
	std::string body;

	inputs_data inputs;

	uint64_t sequence_number;
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
	std::string kv_delimiter;

	// WebSocket-specific fields
	bool is_websocket;
	WebSocketClientWrapper *ws_client_wrapper;
	bool ws_connected;

	// default constructor
	url_source_request_data()
	{
		source_name = std::string("");
		url = std::string("");
		url_or_file = std::string("url");
		method = std::string("GET");
		fail_on_http_error = false;
		body = std::string("");
		inputs = {};
		sequence_number = 0;
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
		post_process_regex = std::string("");
		post_process_regex_is_replace = false;
		post_process_regex_replace = std::string("");
		kv_delimiter = std::string("=");
		ws_client_wrapper = nullptr;
	}
};

struct request_data_handler_response {
	std::string body;
	std::vector<uint8_t> body_bytes;
	nlohmann::json body_json;
	std::string content_type;
	std::vector<std::string> body_parts_parsed;
	std::map<std::string, std::string> key_value_pairs;
	std::map<std::string, std::string> headers;
	int status_code = URL_SOURCE_REQUEST_SUCCESS;
	long http_status_code;
	std::string status_message;
	std::string error_message;
	std::string request_url;
	std::string request_body;
};

namespace inja {
class Environment;
}

void prepare_inja_env(inja::Environment *env, url_source_request_data *request_data,
		      request_data_handler_response &response, nlohmann::json &json);

struct request_data_handler_response request_data_handler(url_source_request_data *request_data);

std::string serialize_request_data(url_source_request_data *request_data);

url_source_request_data unserialize_request_data(std::string serialized_request_data);

// Fetch image from url and get bytes
std::vector<uint8_t> fetch_image(std::string url, std::string &out_mime_type);

// encode bytes to base64
std::string base64_encode(const std::vector<uint8_t> &bytes);

inline uint64_t get_time_ns(void)
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		       std::chrono::system_clock::now().time_since_epoch())
		.count();
}

#endif // REQUEST_DATA_H
