#ifndef PARSERS_H
#define PARSERS_H

#include "request-data.h"

struct request_data_handler_response parse_json(struct request_data_handler_response response,
						const url_source_request_data *request_data);

struct request_data_handler_response
parse_json_pointer(struct request_data_handler_response response,
		   const url_source_request_data *request_data);

struct request_data_handler_response parse_json_path(struct request_data_handler_response response,
						     const url_source_request_data *request_data);

struct request_data_handler_response parse_regex(struct request_data_handler_response response,
						 const url_source_request_data *request_data);

struct request_data_handler_response parse_xml(struct request_data_handler_response response,
					       const url_source_request_data *request_data);

struct request_data_handler_response parse_html(struct request_data_handler_response response,
						const url_source_request_data *request_data);

struct request_data_handler_response parse_image_data(struct request_data_handler_response response,
						      const url_source_request_data *request_data);

struct request_data_handler_response parse_audio_data(struct request_data_handler_response response,
						      const url_source_request_data *request_data);

struct request_data_handler_response
parse_xml_by_xquery(struct request_data_handler_response response,
		    const url_source_request_data *request_data);

struct request_data_handler_response parse_key_value(struct request_data_handler_response response,
						     const url_source_request_data *request_data);

#endif // PARSERS_H
