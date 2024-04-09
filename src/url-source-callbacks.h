#ifndef URL_SOURCE_CALLBACKS_H
#define URL_SOURCE_CALLBACKS_H

#include <obs-module.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <inja/inja.hpp>

#include "request-data.h"

void output_with_mapping(const request_data_handler_response &response,
			 struct url_source_data *usd);

#endif
