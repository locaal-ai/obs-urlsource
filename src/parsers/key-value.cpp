#include "errors.h"
#include "request-data.h"

#include <obs-module.h>

#include <sstream>
#include <string>

struct request_data_handler_response parse_key_value(struct request_data_handler_response response,
						     const url_source_request_data *request_data)
{

	UNUSED_PARAMETER(request_data);

	// Create a string stream from the body
	std::istringstream body_stream(response.body);
	std::string line;

	// Iterate through each line of the body
	while (std::getline(body_stream, line)) {
		// Skip empty lines
		if (line.empty())
			continue;

		// Split the line by the delimiter
		// look for the first occurrence of the delimiter from the beginning of the line
		size_t delimiter_pos = line.find(request_data->kv_delimiter);
		if (delimiter_pos != std::string::npos) {
			std::string key = line.substr(0, delimiter_pos);
			std::string value = line.substr(delimiter_pos + 1);
			response.key_value_pairs[key] = value;
			response.body_parts_parsed.push_back(line);
		}
	}

	return response;
}
