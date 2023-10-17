#include "request-data.h"
#include "errors.h"

#include <jsoncons/basic_json.hpp>
#include <jsoncons/json_parser.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <obs-module.h>
#include <nlohmann/json.hpp>

struct request_data_handler_response parse_json(struct request_data_handler_response response,
						const url_source_request_data *request_data)
{
	UNUSED_PARAMETER(request_data);

	// Parse the response as JSON
	jsoncons::json json;
	try {
		json = jsoncons::json::parse(response.body);
        response.body_json = nlohmann::json::parse(response.body);
	} catch (jsoncons::json_exception &e) {
		return make_fail_parse_response(e.what());
	} catch (nlohmann::json::parse_error &e) {
		return make_fail_parse_response(e.what());
	}
	// Return the whole JSON object
	response.body_parts_parsed.push_back(json.as_string());
	return response;
}

struct request_data_handler_response parse_json_path(struct request_data_handler_response response,
						     const url_source_request_data *request_data)
{

	// Parse the response as JSON
	jsoncons::json json;
	try {
		json = jsoncons::json::parse(response.body);
        response.body_json = nlohmann::json::parse(response.body);
	} catch (jsoncons::json_exception &e) {
		return make_fail_parse_response(e.what());
	} catch (nlohmann::json::parse_error &e) {
		return make_fail_parse_response(e.what());
	}
	std::vector<std::string> parsed_output = {};
	// Get the output value
	if (request_data->output_json_path != "") {
		try {
			const auto value = jsoncons::jsonpath::json_query(
				json, request_data->output_json_path);
			if (value.is_array()) {
				// extract array items as strings
				for (const auto &item : value.array_range()) {
					parsed_output.push_back(item.as_string());
				}
			} else {
				parsed_output.push_back(value.as_string());
			}
		} catch (jsoncons::json_exception &e) {
			return make_fail_parse_response(e.what());
		}
	} else {
		// Return the whole JSON object
		parsed_output.clear();
		parsed_output.push_back(json.as_string());
	}
	response.body_parts_parsed = parsed_output;
	return response;
}
