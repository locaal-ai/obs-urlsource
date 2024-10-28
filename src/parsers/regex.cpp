#include "request-data.h"
#include "plugin-support.h"
#include "errors.h"

#include <regex>
#include <obs-module.h>

struct request_data_handler_response parse_regex(struct request_data_handler_response response,
						 const url_source_request_data *request_data)
{
	try {
		if (request_data->output_regex.empty()) {
			response.body_parts_parsed.push_back(response.body);
			return response;
		}

		// Cache compiled regex patterns for better performance
		static thread_local std::unordered_map<std::string, std::regex> regex_cache;

		auto &regex = regex_cache[request_data->output_regex];
		if (regex_cache.find(request_data->output_regex) == regex_cache.end()) {
			regex = std::regex(request_data->output_regex,
					   std::regex_constants::ECMAScript |
						   std::regex_constants::optimize);
		}

		std::smatch match;
		if (std::regex_search(response.body, match, regex)) {
			// Get the appropriate capture group
			size_t group = match.size() > 1 ? 1 : 0;
			response.body_parts_parsed.push_back(match[group].str());
			return response;
		}

		return make_fail_parse_response("No regex match found");

	} catch (const std::regex_error &e) {
		return make_fail_parse_response(std::string("Regex error: ") + e.what());
	} catch (const std::exception &e) {
		return make_fail_parse_response(std::string("Parse error: ") + e.what());
	}
}
