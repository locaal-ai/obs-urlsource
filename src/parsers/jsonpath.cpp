#include "request-data.h"
#include "errors.h"

#include <jsoncons/basic_json.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <nlohmann/json.hpp>
#include <util/c99defs.h>

struct request_data_handler_response parse_json(struct request_data_handler_response response,
						const url_source_request_data *request_data)
{
	UNUSED_PARAMETER(request_data);
	try {
		// Parse JSON only once and store in both formats
		auto json_cons = jsoncons::json::parse(response.body);
		response.body_json = nlohmann::json::parse(response.body);
		return response;
	} catch (const jsoncons::json_exception &e) {
		return make_fail_parse_response(e.what());
	} catch (const nlohmann::json::exception &e) {
		return make_fail_parse_response(e.what());
	}
}

struct request_data_handler_response parse_json_path(struct request_data_handler_response response,
						     const url_source_request_data *request_data)
{
	try {
		auto json = jsoncons::json::parse(response.body);
		response.body_json = nlohmann::json::parse(response.body);

		if (!request_data->output_json_path.empty()) {
			// Create and evaluate JSONPath expression
			auto value = jsoncons::jsonpath::json_query(json,
								    request_data->output_json_path);

			if (value.is_array()) {
				response.body_parts_parsed.reserve(value.size());
				for (const auto &item : value.array_range()) {
					response.body_parts_parsed.push_back(
						item.as<std::string>());
				}
			} else {
				response.body_parts_parsed.push_back(value.as<std::string>());
			}
		} else {
			response.body_parts_parsed.push_back(json.as<std::string>());
		}

		return response;

	} catch (const jsoncons::jsonpath::jsonpath_error &e) {
		return make_fail_parse_response(std::string("JSONPath error: ") + e.what());
	} catch (const std::exception &e) {
		return make_fail_parse_response(std::string("JSON parse error: ") + e.what());
	}
}
