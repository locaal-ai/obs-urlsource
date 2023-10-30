#include "request-data.h"
#include "plugin-support.h"

#include <tidy.h>
#include <tidybuffio.h>
#include <pugixml.hpp>

#include <obs-module.h>

struct request_data_handler_response parse_html(struct request_data_handler_response response,
					       const url_source_request_data *request_data) {
  // Parse the response as HTML using tidy
  TidyBuffer output = {0};
  TidyBuffer errbuf = {0};
  TidyDoc tdoc = tidyCreate();
  tidyOptSetBool( tdoc, TidyXhtmlOut, yes );
  // tidyOptSetBool(tdoc, TidyShowWarnings, no);
  // tidyOptSetBool(tdoc, TidyShowErrors, no);
  // tidyOptSetBool(tdoc, TidyQuiet, yes);
  // tidyOptSetBool(tdoc, TidyNumEntities, yes);
  // tidyOptSetBool(tdoc, TidyShowInfo, no);
  // tidyOptSetBool(tdoc, TidyShowMarkup, no);
  // tidyOptSetBool(tdoc, TidyBodyOnly, yes);
  // tidyOptSetBool(tdoc, TidyMakeClean, yes);
  tidyOptSetBool(tdoc, TidyDropEmptyParas, yes);
  tidyOptSetBool(tdoc, TidyDropEmptyElems, yes);
  // tidyOptSetBool(tdoc, TidyWrapScriptlets, yes);

  tidySetErrorBuffer(tdoc, &errbuf);
  tidyParseString(tdoc, response.body.c_str());
  tidyCleanAndRepair(tdoc);
  if (tidyRunDiagnostics(tdoc) < 0) {
    obs_log(LOG_INFO, "Failed to parse HTML response: %s", errbuf.bp);
    // Return an error response
    struct request_data_handler_response responseFail;
    responseFail.error_message = (const char *)errbuf.bp;
    responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
    // release tidy buffers
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);
    return responseFail;
  }
  tidyOptSetBool(tdoc, TidyForceOutput, yes);
  if (tidySaveBuffer(tdoc, &output) < 0) {
    obs_log(LOG_INFO, "Failed to parse HTML response: %s", errbuf.bp);
    // Return an error response
    struct request_data_handler_response responseFail;
    responseFail.error_message = (const char *)errbuf.bp;
    responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
    // release tidy buffers
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);
    return responseFail;
  }

  // check for errors
  if (errbuf.bp != NULL && errbuf.size > 0) {
    obs_log(LOG_INFO, "Failed to parse HTML response: %s", errbuf.bp);
    // Return an error response
    struct request_data_handler_response responseFail;
    responseFail.error_message = (const char *)errbuf.bp;
    responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
    // release tidy buffers
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);
    return responseFail;
  }

  if (output.bp == NULL || output.size == 0) {
    obs_log(LOG_INFO, "Failed to parse HTML response: empty output");
    // Return an error response
    struct request_data_handler_response responseFail;
    responseFail.error_message = "Failed to parse HTML response: empty output";
    responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
    // release tidy buffers
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    tidyRelease(tdoc);
    return responseFail;
  }

  // use pugixml to parse the output
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string((const char *)output.bp);
  if (!result) {
    obs_log(LOG_INFO, "Failed to parse HTML response: %s", result.description());
    // Return an error response
    struct request_data_handler_response responseFail;
    responseFail.error_message = result.description();
    responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
    return responseFail;
  }
  std::string parsed_output = "";
  // Get the output value
  if (request_data->output_xpath != "") {
    pugi::xpath_node_set nodes = doc.select_nodes(request_data->output_xpath.c_str());
    if (nodes.size() > 0) {
      parsed_output = nodes[0].node().text().get();
    } else {
      obs_log(LOG_INFO, "Failed to get HTML value");
      // Return an error response
      struct request_data_handler_response responseFail;
      responseFail.error_message = "Failed to get HTML value";
      responseFail.status_code = URL_SOURCE_REQUEST_PARSING_ERROR_CODE;
      return responseFail;
    }
  } else {
    // Return the whole HTML object
    parsed_output = response.body;
  }
  response.body_parts_parsed.push_back(parsed_output);

  // free tidy buffers
  tidyBufFree(&output);
  tidyBufFree(&errbuf);
  tidyRelease(tdoc);

  return response;
}
