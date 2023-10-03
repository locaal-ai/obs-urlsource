#ifndef PARSERS_ERRORS_H
#define PARSERS_ERRORS_H

#include "request-data.h"

struct request_data_handler_response make_fail_parse_response(const std::string &error_message);

#endif // PARSERS_ERRORS_H
