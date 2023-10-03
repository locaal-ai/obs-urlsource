
#include "request-data.h"
#include "plugin-support.h"

#include <regex>
#include <obs-module.h>

struct request_data_handler_response parse_regex(struct request_data_handler_response response,
						 const url_source_request_data *request_data)
{
	std::string parsed_output = "";
	if (request_data->output_regex == "") {
		// Return the whole response body
		parsed_output = response.body;
	} else {
		// Parse the response as a regex
		std::regex regex(request_data->output_regex,
				 std::regex_constants::ECMAScript | std::regex_constants::optimize);
		std::smatch match;
		if (std::regex_search(response.body, match, regex)) {
			if (match.size() > 1) {
				parsed_output = match[1].str();
			} else {
				parsed_output = match[0].str();
			}
		} else {
			obs_log(LOG_INFO, "Failed to match regex");
			// Return an error response
			struct request_data_handler_response responseFail;
			responseFail.error_message = "Failed to match regex";
			responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
			return responseFail;
		}
	}
	response.body_parts_parsed.push_back(parsed_output);
	return response;
}
