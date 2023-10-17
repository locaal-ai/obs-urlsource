#include "request-data.h"
#include "errors.h"

#include <nlohmann/json.hpp>

struct request_data_handler_response
parse_json_pointer(struct request_data_handler_response response,
		   const url_source_request_data *request_data)
{
	// Parse the response as JSON
	nlohmann::json json;
	try {
		json = nlohmann::json::parse(response.body);
	} catch (nlohmann::json::parse_error &e) {
		return make_fail_parse_response(e.what());
	}
	response.body_json = json;
	std::string parsed_output = "";
	// Get the output value
	if (request_data->output_json_pointer != "") {
		try {
			const auto value = json.at(nlohmann::json::json_pointer(
							   request_data->output_json_pointer))
						   .get<nlohmann::json>();
			if (value.is_string()) {
				parsed_output = value.get<std::string>();
			} else {
				parsed_output = value.dump();
			}
			// remove potential prefix and postfix quotes, conversion from string
			if (parsed_output.size() > 1 && parsed_output.front() == '"' &&
			    parsed_output.back() == '"') {
				parsed_output = parsed_output.substr(1, parsed_output.size() - 2);
			}
		} catch (nlohmann::json::exception &e) {
			return make_fail_parse_response(e.what());
		}
	} else {
		// Return the whole JSON object
		parsed_output = json.dump();
	}
	response.body_parts_parsed.push_back(parsed_output);
	return response;
}
