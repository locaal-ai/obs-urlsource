#include "request-data.h"
#include "plugin-support.h"
#include "errors.h"

#include <pugixml.hpp>
#include <obs-module.h>

struct request_data_handler_response parse_xml(struct request_data_handler_response response,
					       const url_source_request_data *request_data)
{
	pugi::xml_document doc;

	try {
		// Use load_buffer instead of load_string for better performance with known size
		pugi::xml_parse_result result =
			doc.load_buffer(response.body.c_str(), response.body.size(),
					pugi::parse_default, pugi::encoding_utf8);

		if (!result) {
			obs_log(LOG_INFO, "Failed to parse XML response: %s", result.description());
			return make_fail_parse_response(result.description());
		}

		if (doc.empty()) {
			return make_fail_parse_response("Empty XML document");
		}

		if (!request_data->output_xpath.empty()) {
			obs_log(LOG_INFO, "Parsing XML with XPath: %s",
				request_data->output_xpath.c_str());

			// Compile XPath expression once for better performance
			pugi::xpath_query query(request_data->output_xpath.c_str());
			pugi::xpath_node_set nodes = query.evaluate_node_set(doc);

			if (nodes.empty()) {
				return make_fail_parse_response("XPath query returned no results");
			}

			// Get all matching nodes
			for (const auto &node : nodes) {
				response.body_parts_parsed.push_back(node.node().text().get());
			}
		} else {
			// Return the whole XML object
			response.body_parts_parsed.push_back(response.body);
		}

		return response;

	} catch (const pugi::xpath_exception &e) {
		obs_log(LOG_INFO, "XPath evaluation failed: %s", e.what());
		return make_fail_parse_response(e.what());
	} catch (const std::exception &e) {
		obs_log(LOG_INFO, "XML parsing failed: %s", e.what());
		return make_fail_parse_response(e.what());
	}
}

struct request_data_handler_response
parse_xml_by_xquery(struct request_data_handler_response response,
		    const url_source_request_data *request_data)
{
	pugi::xml_document doc;

	try {
		pugi::xml_parse_result result =
			doc.load_buffer(response.body.c_str(), response.body.size(),
					pugi::parse_default, pugi::encoding_utf8);

		if (!result) {
			return make_fail_parse_response(result.description());
		}

		if (!request_data->output_xquery.empty()) {
			pugi::xpath_query query(request_data->output_xquery.c_str());
			std::string result = query.evaluate_string(doc);
			response.body_parts_parsed.push_back(std::move(result));
		} else {
			response.body_parts_parsed.push_back(response.body);
		}

		return response;

	} catch (const pugi::xpath_exception &e) {
		return make_fail_parse_response(e.what());
	} catch (const std::exception &e) {
		return make_fail_parse_response(e.what());
	}
}
