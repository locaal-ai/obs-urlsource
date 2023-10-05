#include "plugin-support.h"
#include "errors.h"

#include <obs-module.h>

struct request_data_handler_response make_fail_parse_response(const std::string &error_message)
{
	obs_log(LOG_INFO, "Failed to parse response: %s", error_message.c_str());
	// Build an error response
	struct request_data_handler_response responseFail;
	responseFail.error_message = error_message;
	responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
	return responseFail;
}
