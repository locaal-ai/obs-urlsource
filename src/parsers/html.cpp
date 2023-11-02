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
	(void)lxb_html_serialize_deep_cb(node, serializer_callback, &str);
	((std::vector<std::string> *)data)->push_back(str);
	return LXB_STATUS_OK;
}

lxb_status_t find_with_selectors(const std::string &slctrs, lxb_html_document_t *document,
				 std::vector<std::string> &found)
{
	/* Create CSS parser. */
	lxb_css_parser_t *parser;
	lxb_css_selector_list_t *list;
	lxb_status_t status;
	lxb_dom_node_t *body;
	lxb_selectors_t *selectors;

	parser = lxb_css_parser_create();
	status = lxb_css_parser_init(parser, NULL);
	if (status != LXB_STATUS_OK) {
		obs_log(LOG_ERROR, "Failed to setup CSS parser");
		return EXIT_FAILURE;
	}

	/* Selectors. */
	selectors = lxb_selectors_create();
	status = lxb_selectors_init(selectors);
	if (status != LXB_STATUS_OK) {
		obs_log(LOG_ERROR, "Failed to setup Selectors");
		return EXIT_FAILURE;
	}

	/* Parse and get the log. */

	list = lxb_css_selectors_parse(parser, (const lxb_char_t *)slctrs.c_str(), slctrs.length());
	if (parser->status != LXB_STATUS_OK) {
		obs_log(LOG_ERROR, "Failed to parse CSS selectors");
		return EXIT_FAILURE;
	}

	/* Selector List Serialization. */

	// std::string selectors_str;
	// (void) lxb_css_selector_serialize_list_chain(list, serializer_callback, &selectors_str);
	// printf("Selectors: %s\n", selectors_str.c_str());

	/* Find HTML nodes by CSS Selectors. */

	body = lxb_dom_interface_node(lxb_html_document_body_element(document));

	status = lxb_selectors_find(selectors, body, list, find_callback, &found);
	if (status != LXB_STATUS_OK) {
		obs_log(LOG_ERROR, "Failed to find HTML nodes by CSS Selectors");
		return EXIT_FAILURE;
	}

	/* Destroy Selectors object. */
	(void)lxb_selectors_destroy(selectors, true);

	/* Destroy resources for CSS Parser. */
	(void)lxb_css_parser_destroy(parser, true);

	/* Destroy all object for all CSS Selector List. */
	lxb_css_selector_list_destroy_memory(list);

	return LXB_STATUS_OK;
}

struct request_data_handler_response parse_html(struct request_data_handler_response response,
						const url_source_request_data *request_data)
{

	lxb_status_t status;
	lxb_html_document_t *document;

	document = lxb_html_document_create();
	if (document == NULL) {
		return make_fail_parse_response("Failed to setup HTML parser");
	}

	status = lxb_html_document_parse(document, (const lxb_char_t *)response.body.c_str(),
					 response.body.length());
	if (status != LXB_STATUS_OK) {
		return make_fail_parse_response("Failed to parse HTML");
	}

	std::string parsed_output = response.body;
	// Get the output value
	if (request_data->output_cssselector != "") {
		std::vector<std::string> found;
		if (find_with_selectors(request_data->output_cssselector, document, found) !=
		    LXB_STATUS_OK) {
			return make_fail_parse_response("Failed to find element with CSS selector");
		} else {
			if (found.size() > 0) {
				std::copy(found.begin(), found.end(),
					  std::back_inserter(response.body_parts_parsed));
			}
		}
	} else {
		// Return the whole HTML object
		response.body_parts_parsed.push_back(parsed_output);
	}

	return response;
}
