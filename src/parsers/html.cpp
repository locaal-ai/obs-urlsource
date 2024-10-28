#include "request-data.h"
#include "plugin-support.h"
#include "errors.h"

#include <lexbor/html/parser.h>
#include <lexbor/html/html.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>

#include <obs-module.h>

lxb_inline lxb_status_t serializer_callback(const lxb_char_t *data, size_t len, void *ctx)
{
	((std::string *)ctx)->append((const char *)data, len);
	return LXB_STATUS_OK;
}

lxb_status_t find_callback(lxb_dom_node_t *node, lxb_css_selector_specificity_t spec, void *data)
{
	UNUSED_PARAMETER(spec);
	std::string str;
	lxb_status_t status = lxb_html_serialize_deep_cb(node, serializer_callback, &str);
	if (status == LXB_STATUS_OK) {
		((std::vector<std::string> *)data)->push_back(str);
	}
	return status;
}

lxb_status_t find_with_selectors(const std::string &slctrs, lxb_html_document_t *document,
				 std::vector<std::string> &found)
{
	lxb_css_parser_t *parser = nullptr;
	lxb_css_selector_list_t *list = nullptr;
	lxb_selectors_t *selectors = nullptr;
	lxb_status_t status = LXB_STATUS_ERROR;

	do {
		parser = lxb_css_parser_create();
		if (!parser) {
			obs_log(LOG_ERROR, "Failed to create CSS parser");
			break;
		}

		status = lxb_css_parser_init(parser, nullptr);
		if (status != LXB_STATUS_OK) {
			obs_log(LOG_ERROR, "Failed to init CSS parser");
			break;
		}

		selectors = lxb_selectors_create();
		if (!selectors) {
			obs_log(LOG_ERROR, "Failed to create selectors");
			break;
		}

		status = lxb_selectors_init(selectors);
		if (status != LXB_STATUS_OK) {
			obs_log(LOG_ERROR, "Failed to init selectors");
			break;
		}

		list = lxb_css_selectors_parse(parser, (const lxb_char_t *)slctrs.c_str(),
					       slctrs.length());
		if (!list || parser->status != LXB_STATUS_OK) {
			obs_log(LOG_ERROR, "Failed to parse CSS selectors");
			break;
		}

		lxb_dom_node_t *body =
			lxb_dom_interface_node(lxb_html_document_body_element(document));
		if (!body) {
			obs_log(LOG_ERROR, "Failed to get document body");
			break;
		}

		status = lxb_selectors_find(selectors, body, list, find_callback, &found);
		if (status != LXB_STATUS_OK) {
			obs_log(LOG_ERROR, "Failed to find nodes by CSS Selectors");
			break;
		}

	} while (0);

	// Cleanup
	if (list) {
		lxb_css_selector_list_destroy_memory(list);
	}
	if (selectors) {
		lxb_selectors_destroy(selectors, true);
	}
	if (parser) {
		lxb_css_parser_destroy(parser, true);
	}

	return status;
}

struct request_data_handler_response parse_html(struct request_data_handler_response response,
						const url_source_request_data *request_data)
{
	lxb_html_document_t *document = nullptr;

	try {
		document = lxb_html_document_create();
		if (!document) {
			return make_fail_parse_response("Failed to create HTML document");
		}

		lxb_status_t status =
			lxb_html_document_parse(document, (const lxb_char_t *)response.body.c_str(),
						response.body.length());

		if (status != LXB_STATUS_OK) {
			lxb_html_document_destroy(document);
			return make_fail_parse_response("Failed to parse HTML");
		}

		if (!request_data->output_cssselector.empty()) {
			std::vector<std::string> found;
			status = find_with_selectors(request_data->output_cssselector, document,
						     found);

			if (status != LXB_STATUS_OK) {
				lxb_html_document_destroy(document);
				return make_fail_parse_response(
					"Failed to find element with CSS selector");
			}

			response.body_parts_parsed = std::move(found);
		} else {
			response.body_parts_parsed.push_back(response.body);
		}

		lxb_html_document_destroy(document);
		return response;

	} catch (const std::exception &e) {
		if (document) {
			lxb_html_document_destroy(document);
		}
		return make_fail_parse_response(std::string("HTML parsing exception: ") + e.what());
	}
}
