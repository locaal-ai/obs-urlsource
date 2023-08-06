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
#include <util/threading.h>
#include <util/platform.h>
#include <graphics/graphics.h>
#include <obs-module.h>
#include <obs-frontend-api.h>

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
	std::string css_props;
	std::string output_text_template;

	// Use std for thread and mutex
	std::unique_ptr<std::mutex> curl_mutex;
	std::thread curl_thread;
	std::unique_ptr<std::condition_variable> curl_thread_cv;
	bool curl_thread_run = false;
};

const char *url_source_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "URL Source";
}

void stop_and_join_curl_thread(struct url_source_data *usd)
{
	{
		std::lock_guard<std::mutex> lock(*(usd->curl_mutex.get()));
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

	bfree(usd);
}

void curl_loop(struct url_source_data *usd)
{
	obs_log(LOG_INFO, "Starting URL Source thread, update timer: %d", usd->update_timer_ms);
	uint64_t cur_time = os_gettime_ns();
	uint64_t start_time = cur_time;

	struct obs_source_frame frame = {};

	while (true) {
		{
			std::lock_guard<std::mutex> lock(*(usd->curl_mutex.get()));
			if (!usd->curl_thread_run) {
				break;
			}
		}

		// time the request
		uint64_t request_start_time_ns = os_gettime_ns();

		// Send the request
		struct request_data_handler_response response =
			request_data_handler(&(usd->request_data));
		if (response.status_code != 200) {
			obs_log(LOG_INFO, "Failed to send request: %s",
				response.error_message.c_str());
		} else {
			cur_time = os_gettime_ns();
			frame.timestamp = cur_time - start_time;

			uint32_t width = 0;
			uint32_t height = 0;
			uint8_t *renderBuffer = nullptr;

			// prepare the text from the template
			std::string text = usd->output_text_template;
			// if the template is empty use the response body
			if (text.empty()) {
				text = response.body_parsed;
			} else {
				// attempt to replace {output} with the response body
				text = std::regex_replace(text, std::regex("\\{output\\}"),
							  response.body_parsed);
			}
			// render the text
			render_text_with_qtextdocument(text, width, height, &renderBuffer,
						       usd->css_props);
			// Update the frame
			frame.data[0] = renderBuffer;
			frame.linesize[0] = width * 4;
			frame.width = width;
			frame.height = height;
			frame.format = VIDEO_FORMAT_BGRA;

			// Send the frame
			obs_source_output_video(usd->source, &frame);

			// Free the render buffer
			bfree(renderBuffer);
		}

		// time the request
		const uint64_t request_end_time_ns = os_gettime_ns();
		const uint64_t request_time_ns = request_end_time_ns - request_start_time_ns;
		const int64_t sleep_time_ms =
			(int64_t)(usd->update_timer_ms) - (int64_t)(request_time_ns / 1000000);
		if (sleep_time_ms > 0) {
			std::unique_lock<std::mutex> lock(*(usd->curl_mutex.get()));
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
	struct url_source_data *usd =
		reinterpret_cast<struct url_source_data *>(bzalloc(sizeof(struct url_source_data)));
	usd->source = source;

	// get request data from settings
	std::string serialized_request_data = obs_data_get_string(settings, "request_data");
	if (serialized_request_data.empty()) {
		// Default request data
		usd->request_data.url = "https://catfact.ninja/fact";
		usd->request_data.method = "GET";
		usd->request_data.output_type = "json";
		usd->request_data.output_json_path = "fact";

		save_request_info_on_settings(settings, &(usd->request_data));
	} else {
		// Unserialize request data
		usd->request_data = unserialize_request_data(serialized_request_data);
	}

	usd->update_timer_ms = (uint32_t)obs_data_get_int(settings, "update_timer");
	usd->css_props = obs_data_get_string(settings, "css_props");
	usd->output_text_template = obs_data_get_string(settings, "template");

	// initialize the mutex
	usd->curl_mutex = std::unique_ptr<std::mutex>(new std::mutex());
	usd->curl_thread_cv =
		std::unique_ptr<std::condition_variable>(new std::condition_variable());

	if (obs_source_active(source) && obs_source_showing(source)) {
		obs_log(LOG_INFO, "url_source_create: source is active");
		// start the thread
		usd->curl_thread_run = true;
		usd->curl_thread = std::thread(curl_loop, usd);
	} else {
		obs_log(LOG_INFO, "url_source_create: source is inactive");
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
	usd->css_props = obs_data_get_string(settings, "css_props");
	usd->output_text_template = obs_data_get_string(settings, "template");

	save_request_info_on_settings(settings, &(usd->request_data));
}

void url_source_defaults(obs_data_t *s)
{
	// Default request data
	struct url_source_request_data request_data;
	request_data.url = "https://catfact.ninja/fact";
	request_data.method = "GET";
	request_data.output_type = "JSON";
	request_data.output_json_path = "fact";

	// serialize request data
	std::string serialized_request_data = serialize_request_data(&request_data);
	obs_data_set_default_string(s, "request_data", serialized_request_data.c_str());
	obs_data_set_default_string(s, "url", request_data.url.c_str());

	// Default update timer setting in milliseconds
	obs_data_set_default_int(s, "update_timer", 1000);

	// Default CSS properties
	obs_data_set_default_string(
		s, "css_props",
		"background-color: transparent;\ncolor: #FFFFFF;\nfont-size: 48px;");

	// Default Template
	obs_data_set_default_string(s, "template", "{output}");
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

obs_properties_t *url_source_properties(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);

	obs_properties_t *ppts = obs_properties_create();
	// URL input string
	obs_property_t *urlprop = obs_properties_add_text(ppts, "url", "URL", OBS_TEXT_DEFAULT);
	// Disable the URL input since it's setup via the Request Builder dialog
	obs_property_set_enabled(urlprop, false);

	obs_properties_add_button2(ppts, "button", "Setup Request", setup_request_button_click,
				   usd);

	// Update timer setting in milliseconds
	obs_properties_add_int(ppts, "update_timer", "Update Timer (ms)", 100, 10000, 100);

	// CSS properties for styling the text
	obs_properties_add_text(ppts, "css_props", "CSS Properties", OBS_TEXT_MULTILINE);

	// Output template
	obs_properties_add_text(ppts, "template", "Output Template", OBS_TEXT_DEFAULT);

	return ppts;
}

void url_source_activate(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);
	// Start the thread
	{
		std::lock_guard<std::mutex> lock(*(usd->curl_mutex.get()));
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
	// Stop the thread
	stop_and_join_curl_thread(usd);
}
