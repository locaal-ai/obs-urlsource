/*
URL Source
Copyright (C) 2023 Roy Shilkrot roy.shil@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "ui/RequestBuilder.h"
#include "ui/text-render-helper.h"
#include "request-data.h"
#include "plugin-support.h"
#include "url-source.h"

#include <stdlib.h>
#include <graphics/graphics.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <inja/inja.hpp>

#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <regex>

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

	// Text source to output the text to
	obs_weak_source_t *text_source = nullptr;
	char *text_source_name = nullptr;
	std::mutex *text_source_mutex = nullptr;

	// Use std for thread and mutex
	std::mutex *curl_mutex = nullptr;
	std::thread curl_thread;
	std::condition_variable *curl_thread_cv = nullptr;
	bool curl_thread_run = false;
};

inline uint64_t get_time_ns(void)
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		       std::chrono::system_clock::now().time_since_epoch())
		.count();
}

const char *url_source_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "URL Source";
}

void stop_and_join_curl_thread(struct url_source_data *usd)
{
	{
		std::lock_guard<std::mutex> lock(*usd->curl_mutex);
		if (!usd->curl_thread_run) {
			// Thread is already stopped
			return;
		}
		usd->curl_thread_run = false;
	}
	usd->curl_thread_cv->notify_all();
	if (usd->curl_thread.joinable()) {
		usd->curl_thread.join();
	}
}

void url_source_destroy(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);

	stop_and_join_curl_thread(usd);

	if (usd->text_source_name) {
		bfree(usd->text_source_name);
		usd->text_source_name = nullptr;
	}

	if (usd->text_source) {
		obs_weak_source_release(usd->text_source);
		usd->text_source = nullptr;
	}

	if (usd->text_source_mutex) {
		delete usd->text_source_mutex;
		usd->text_source_mutex = nullptr;
	}

	if (usd->curl_mutex) {
		delete usd->curl_mutex;
		usd->curl_mutex = nullptr;
	}

	if (usd->curl_thread_cv) {
		delete usd->curl_thread_cv;
		usd->curl_thread_cv = nullptr;
	}

	if (usd->frame.data[0] != nullptr) {
		bfree(usd->frame.data[0]);
		usd->frame.data[0] = nullptr;
	}

	bfree(usd);
}

void acquire_weak_text_source_ref(struct url_source_data *usd)
{
	if (!usd->text_source_name) {
		obs_log(LOG_ERROR, "text_source_name is null");
		return;
	}

	if (strcmp(usd->text_source_name, "none") == 0 ||
	    strcmp(usd->text_source_name, "(null)") == 0 ||
	    strcmp(usd->text_source_name, "") == 0) {
		// text source is not selected
		return;
	}

	if (!usd->text_source_mutex) {
		obs_log(LOG_ERROR, "text_source_mutex is null");
		return;
	}

	std::lock_guard<std::mutex> lock(*usd->text_source_mutex);

	// acquire a weak ref to the new text source
	obs_source_t *source = obs_get_source_by_name(usd->text_source_name);
	if (source) {
		usd->text_source = obs_source_get_weak_source(source);
		obs_source_release(source);
		if (!usd->text_source) {
			obs_log(LOG_ERROR, "failed to get weak source for text source %s",
				usd->text_source_name);
		}
	} else {
		obs_log(LOG_ERROR, "text source '%s' not found", usd->text_source_name);
	}
}

void setTextCallback(const std::string &str, struct url_source_data *usd)
{
	if (!usd->text_source_mutex) {
		obs_log(LOG_ERROR, "text_source_mutex is null");
		return;
	}

	if (!usd->text_source) {
		// attempt to acquire a weak ref to the text source if it's yet available
		acquire_weak_text_source_ref(usd);
	}

	std::lock_guard<std::mutex> lock(*usd->text_source_mutex);

	obs_weak_source_t *text_source = usd->text_source;
	if (!text_source) {
		obs_log(LOG_ERROR, "text_source is null");
		return;
	}
	auto target = obs_weak_source_get_source(text_source);
	if (!target) {
		obs_log(LOG_ERROR, "text_source target is null");
		return;
	}
	auto text_settings = obs_source_get_settings(target);
	obs_data_set_string(text_settings, "text", str.c_str());
	obs_source_update(target, text_settings);
	obs_source_release(target);
};

void curl_loop(struct url_source_data *usd)
{
	obs_log(LOG_INFO, "Starting URL Source thread, update timer: %d", usd->update_timer_ms);
	uint64_t cur_time = get_time_ns();
	uint64_t start_time = cur_time;

	usd->frame.format = VIDEO_FORMAT_BGRA;

	while (true) {
		{
			std::lock_guard<std::mutex> lock(*(usd->curl_mutex));
			if (!usd->curl_thread_run) {
				break;
			}
		}

		// time the request
		uint64_t request_start_time_ns = get_time_ns();

		// Send the request
		struct request_data_handler_response response =
			request_data_handler(&(usd->request_data));
		if (response.status_code != URL_SOURCE_REQUEST_SUCCESS) {
			if (response.status_code != URL_SOURCE_REQUEST_BENIGN_ERROR_CODE) {
				obs_log(LOG_INFO, "Failed to send request: %s",
					response.error_message.c_str());
			}
		} else {
			if (response.body_parts_parsed.empty()) {
				response.body_parts_parsed.push_back(response.body);
			}

			cur_time = get_time_ns();
			usd->frame.timestamp = cur_time - start_time;

			uint32_t width = 0;
			uint32_t height = 0;

			// prepare the text from the template
			std::string text = usd->output_text_template;
			// if the template is empty use the response body
			if (text.empty()) {
				text = response.body_parts_parsed[0];
			} else {
				// if output is image URL - fetch the image and convert it to base64
				if (usd->output_is_image_url) {
					// use fetch_image to get the image
					std::vector<uint8_t> image_data =
						fetch_image(response.body_parts_parsed[0]);
					// convert the image to base64
					const std::string base64_image = base64_encode(image_data);
					// build an image tag with the base64 image
					response.body_parts_parsed[0] =
						"<img src=\"data:image/png;base64," + base64_image +
						"\" />";
				}
				try {
					// Use Inja to render the template
					inja::Environment env;
					nlohmann::json data;
					if (response.body_parts_parsed.size() > 1) {
						for (size_t i = 0;
						     i < response.body_parts_parsed.size(); i++) {
							data["output" + std::to_string(i)] =
								response.body_parts_parsed[i];
						}
					} else {
						data["output"] = response.body_parts_parsed[0];
					}
					data["body"] = response.body_json;
					text = env.render(text, data);
				} catch (std::exception &e) {
					obs_log(LOG_ERROR, "Failed to parse template: %s",
						e.what());
				}
			}

			if (usd->send_to_stream && !usd->output_is_image_url) {
				// Send the output to the current stream as caption, if it's not an image and a stream is open
				obs_output_t *streaming_output =
					obs_frontend_get_streaming_output();
				if (streaming_output) {
					obs_output_output_caption_text1(streaming_output,
									text.c_str());
					obs_output_release(streaming_output);
				}
			}

			if (usd->frame.data[0] != nullptr) {
				// Free the old render buffer
				bfree(usd->frame.data[0]);
				usd->frame.data[0] = nullptr;
			}

			if (usd->text_source_name != nullptr &&
			    strcmp(usd->text_source_name, "none") != 0 &&
			    strcmp(usd->text_source_name, "(null)") != 0 &&
			    strcmp(usd->text_source_name, "") != 0) {
				// If a text source is selected - use it for rendering
				setTextCallback(text, usd);

				// Update the frame with an empty buffer of 1x1 pixels
				usd->frame.data[0] = (uint8_t *)bzalloc(4);
				usd->frame.linesize[0] = 4;
				usd->frame.width = 1;
				usd->frame.height = 1;

				// Send the frame
				obs_source_output_video(usd->source, &usd->frame);
			} else {
				uint8_t *renderBuffer = nullptr;
				// render the text with QTextDocument
				render_text_with_qtextdocument(text, width, height, &renderBuffer,
							       usd->css_props);
				// Update the frame
				usd->frame.data[0] = renderBuffer;
				usd->frame.linesize[0] = width * 4;
				usd->frame.width = width;
				usd->frame.height = height;

				// Send the frame
				obs_source_output_video(usd->source, &usd->frame);
			}
		}

		// time the request, calculate the remaining time and sleep
		const uint64_t request_end_time_ns = get_time_ns();
		const uint64_t request_time_ns = request_end_time_ns - request_start_time_ns;
		const int64_t sleep_time_ms =
			(int64_t)(usd->update_timer_ms) - (int64_t)(request_time_ns / 1000000);
		if (sleep_time_ms > 0) {
			std::unique_lock<std::mutex> lock(*(usd->curl_mutex));
			// Sleep for n ns as per the update timer for the remaining time
			usd->curl_thread_cv->wait_for(lock,
						      std::chrono::milliseconds(sleep_time_ms));
		}
	}
	obs_log(LOG_INFO, "Stopping URL Source thread");
}

void save_request_info_on_settings(obs_data_t *settings,
				   struct url_source_request_data *request_data)
{
	// serialize request data
	std::string serialized_request_data = serialize_request_data(request_data);
	// Save on settings
	obs_data_set_string(settings, "request_data", serialized_request_data.c_str());
	// Update the URL input string
	obs_data_set_string(settings, "url", request_data->url.c_str());
}

void *url_source_create(obs_data_t *settings, obs_source_t *source)
{
	void *p = bzalloc(sizeof(struct url_source_data));
	struct url_source_data *usd = new (p) url_source_data;
	usd->source = source;
	usd->request_data = url_source_request_data();

	usd->frame.data[0] = nullptr;

	// get request data from settings
	std::string serialized_request_data = obs_data_get_string(settings, "request_data");
	if (serialized_request_data.empty()) {
		// Default request data
		usd->request_data.url = std::string("https://catfact.ninja/fact");
		usd->request_data.url_or_file = std::string("url");
		usd->request_data.method = std::string("GET");
		usd->request_data.output_type = std::string("json");
		usd->request_data.output_json_path = std::string("/fact");

		save_request_info_on_settings(settings, &(usd->request_data));
	} else {
		// Unserialize request data
		usd->request_data = unserialize_request_data(serialized_request_data);
	}

	usd->update_timer_ms = (uint32_t)obs_data_get_int(settings, "update_timer");
	usd->run_while_not_visible = obs_data_get_bool(settings, "run_while_not_visible");
	usd->output_is_image_url = obs_data_get_bool(settings, "is_image_url");
	usd->css_props = std::string(obs_data_get_string(settings, "css_props"));
	usd->output_text_template = std::string(obs_data_get_string(settings, "template"));
	usd->send_to_stream = obs_data_get_bool(settings, "send_to_stream");

	// initialize the mutex
	usd->text_source_mutex = new std::mutex();
	usd->curl_mutex = new std::mutex();
	usd->curl_thread_cv = new std::condition_variable();
	usd->text_source_name = bstrdup(obs_data_get_string(settings, "text_sources"));
	usd->text_source = nullptr;

	if (obs_source_active(source) && obs_source_showing(source)) {
		// start the thread
		usd->curl_thread_run = true;
		std::thread new_curl_thread(curl_loop, usd);
		usd->curl_thread.swap(new_curl_thread);
	} else {
		// thread should not be running
		usd->curl_thread_run = false;
	}

	return usd;
}

void url_source_update(void *data, obs_data_t *settings)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);
	// Update the request data from the settings
	usd->update_timer_ms = (uint32_t)obs_data_get_int(settings, "update_timer");
	usd->run_while_not_visible = obs_data_get_bool(settings, "run_while_not_visible");
	usd->output_is_image_url = obs_data_get_bool(settings, "is_image_url");
	usd->css_props = obs_data_get_string(settings, "css_props");
	usd->output_text_template = obs_data_get_string(settings, "template");
	usd->send_to_stream = obs_data_get_bool(settings, "send_to_stream");

	// update the text source
	const char *new_text_source_name = obs_data_get_string(settings, "text_sources");
	obs_weak_source_t *old_weak_text_source = NULL;

	if (strcmp(new_text_source_name, "none") == 0 ||
	    strcmp(new_text_source_name, "(null)") == 0) {
		// new selected text source is not valid, release the old one
		if (usd->text_source) {
			std::lock_guard<std::mutex> lock(*usd->text_source_mutex);
			old_weak_text_source = usd->text_source;
			usd->text_source = nullptr;
		}
		if (usd->text_source_name) {
			bfree(usd->text_source_name);
			usd->text_source_name = nullptr;
		}
	} else {
		// new selected text source is valid, check if it's different from the old one
		if (usd->text_source_name == nullptr ||
		    strcmp(new_text_source_name, usd->text_source_name) != 0) {
			// new text source is different from the old one, release the old one
			if (usd->text_source) {
				std::lock_guard<std::mutex> lock(*usd->text_source_mutex);
				old_weak_text_source = usd->text_source;
				usd->text_source = nullptr;
			}
			usd->text_source_name = bstrdup(new_text_source_name);
		}
	}

	if (old_weak_text_source) {
		obs_weak_source_release(old_weak_text_source);
	}

	save_request_info_on_settings(settings, &(usd->request_data));
}

void url_source_defaults(obs_data_t *s)
{
	// Default request data
	struct url_source_request_data request_data;
	request_data.url = "https://catfact.ninja/fact";
	request_data.url_or_file = "url";
	request_data.method = "GET";
	request_data.output_type = "JSON";
	request_data.output_json_path = "/fact";

	// serialize request data
	std::string serialized_request_data = serialize_request_data(&request_data);
	obs_data_set_default_string(s, "request_data", serialized_request_data.c_str());
	obs_data_set_default_string(s, "url", request_data.url.c_str());

	obs_data_set_default_string(s, "text_sources", "none");

	obs_data_set_default_bool(s, "send_to_stream", false);

	// Default update timer setting in milliseconds
	obs_data_set_default_int(s, "update_timer", 1000);

	obs_data_set_default_bool(s, "run_while_not_visible", false);

	// Is Image URL default false
	obs_data_set_default_bool(s, "is_image_url", false);

	// Default CSS properties
	obs_data_set_default_string(
		s, "css_props",
		"background-color: transparent;\ncolor: #FFFFFF;\nfont-size: 48px;");

	// Default Template
	obs_data_set_default_string(s, "template", "{{output}}");
}

bool setup_request_button_click(obs_properties_t *, obs_property_t *, void *button_data)
{
	struct url_source_data *button_usd =
		reinterpret_cast<struct url_source_data *>(button_data);
	// Open the Request Builder dialog
	RequestBuilder *builder = new RequestBuilder(
		&(button_usd->request_data),
		[button_usd]() {
			// Update the request data from the settings
			obs_data_t *settings = obs_source_get_settings(button_usd->source);
			save_request_info_on_settings(settings, &(button_usd->request_data));
		},
		(QWidget *)obs_frontend_get_main_window());
	builder->show();
	return true;
}

bool add_sources_to_list(void *list_property, obs_source_t *source)
{
	auto source_id = obs_source_get_id(source);
	if (strcmp(source_id, "text_ft2_source_v2") != 0 &&
	    strcmp(source_id, "text_gdiplus_v2") != 0) {
		return true;
	}

	obs_property_t *sources = (obs_property_t *)list_property;
	const char *name = obs_source_get_name(source);
	obs_property_list_add_string(sources, name, name);
	return true;
}

obs_properties_t *url_source_properties(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);

	obs_properties_t *ppts = obs_properties_create();
	// URL input string
	obs_property_t *urlprop =
		obs_properties_add_text(ppts, "url", "URL / File", OBS_TEXT_DEFAULT);
	// Disable the URL input since it's setup via the Request Builder dialog
	obs_property_set_enabled(urlprop, false);

	// Add button to open the Request Builder dialog
	obs_properties_add_button2(ppts, "setup_request_button", "Setup Data Source",
				   setup_request_button_click, usd);

	// Update timer setting in milliseconds
	obs_properties_add_int(ppts, "update_timer", "Update Timer (ms)", 100, 1000000, 100);

	// Run timer while not visible
	obs_properties_add_bool(ppts, "run_while_not_visible", "Run while not visible?");

	obs_property_t *sources = obs_properties_add_list(ppts, "text_sources",
							  "Output text source", OBS_COMBO_TYPE_LIST,
							  OBS_COMBO_FORMAT_STRING);
	// Add a "none" option
	obs_property_list_add_string(sources, "None / Internal rendering", "none");
	// Add the sources
	obs_enum_sources(add_sources_to_list, sources);

	obs_properties_add_bool(ppts, "send_to_stream",
				"Send output to current stream as captions");

	// Is Image URL boolean checkbox
	obs_properties_add_bool(ppts, "is_image_url", "Output is image URL (fetch and show image)");

	// CSS properties for styling the text
	obs_properties_add_text(ppts, "css_props", "CSS Properties", OBS_TEXT_MULTILINE);

	// Output template
	obs_properties_add_text(ppts, "template", "Output Template", OBS_TEXT_MULTILINE);

	return ppts;
}

void url_source_activate(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);
	// Start the thread
	{
		std::lock_guard<std::mutex> lock(*(usd->curl_mutex));
		if (usd->curl_thread_run) {
			// Thread is already running
			return;
		}
		usd->curl_thread_run = true;
	}
	usd->curl_thread = std::thread(curl_loop, usd);
}

void url_source_deactivate(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);
	if (!usd->run_while_not_visible) {
		// Stop the thread
		stop_and_join_curl_thread(usd);
	}
}
