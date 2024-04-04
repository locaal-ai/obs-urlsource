#ifndef URL_SOURCE_DATA_H
#define URL_SOURCE_DATA_H

#include "request-data.h"

#include <obs-module.h>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>

struct url_source_data {
	obs_source_t *source;
	struct url_source_request_data request_data;
	struct request_data_handler_response response;
	uint32_t update_timer_ms = 1000;
	bool run_while_not_visible = false;
	std::string css_props;
	std::string output_text_template;
	bool output_is_image_url = false;
	struct obs_source_frame frame;
	bool send_to_stream = false;
	uint32_t render_width = 640;
	bool unhide_output_source = false;

	// Text source to output the text to
	obs_weak_source_t *output_source = nullptr;
	char *output_source_name = nullptr;
	std::mutex *output_source_mutex = nullptr;

	// Use std for thread and mutex
	std::mutex *curl_mutex = nullptr;
	std::thread curl_thread;
	std::condition_variable *curl_thread_cv = nullptr;
	bool curl_thread_run = false;
};

#endif
