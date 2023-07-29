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

#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <graphics/graphics.h>
#include <obs-module.h>
#include <obs-frontend-api.h>

struct url_source_data {
	obs_source_t *source;
	struct url_source_request_data request_data;
	struct request_data_handler_response response;
	uint32_t update_timer_ms;

	// Use std for thread and mutex
	std::thread curl_thread;
	bool curl_thread_run = false;
};

std::mutex curl_mutex;

static const char *url_source_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "URL Source";
}

static void url_source_destroy(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);

	{
		std::lock_guard<std::mutex> lock(curl_mutex);
		usd->curl_thread_run = false;
	}

	// join the thread
	usd->curl_thread.join();

	bfree(usd);
}

void curl_loop(struct url_source_data *usd)
{
	obs_log(LOG_INFO, "Starting URL Source thread");
	uint64_t cur_time = os_gettime_ns();
	uint64_t start_time = cur_time;

	struct obs_source_frame frame = {};

	while (true) {
		{
			std::lock_guard<std::mutex> lock(curl_mutex);
			if (!usd->curl_thread_run) {
				break;
			}
		}
		// Send the request
		struct request_data_handler_response response =
			request_data_handler(&(usd->request_data));
		if (response.status_code != 200) {
			obs_log(LOG_INFO, "Failed to send request: %s",
				response.error_message.c_str());
		} else {
			frame.timestamp = cur_time - start_time;

			uint32_t width = 0;
			uint32_t height = 0;
			uint8_t *renderBuffer = nullptr;
			render_text_with_qtextdocument(response.body_parsed, width, height,
						       &renderBuffer);
			// Update the frame
			frame.data[0] = renderBuffer;
			frame.linesize[0] = width * 4;
			frame.width = width;
			frame.height = height;
			frame.format = VIDEO_FORMAT_BGRX;

			std::lock_guard<std::mutex> lock(curl_mutex);
			// Send the frame
			obs_source_output_video(usd->source, &frame);

			// Free the render buffer
			bfree(renderBuffer);
		}

		// Sleep for n ms as per the update timer
		std::this_thread::sleep_for(std::chrono::milliseconds(usd->update_timer_ms));
	}
	obs_log(LOG_INFO, "Stopping URL Source thread");
}

static void *url_source_create(obs_data_t *settings, obs_source_t *source)
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
		// serialize request data
		std::string serialized_request_data = serialize_request_data(&(usd->request_data));
		// Save on settings
		obs_data_set_string(settings, "request_data", serialized_request_data.c_str());
	} else {
		// Unserialize request data
		usd->request_data = unserialize_request_data(serialized_request_data);
	}

	// start the thread
	usd->curl_thread_run = true;
	usd->curl_thread = std::thread(curl_loop, usd);

	return usd;
}

static void url_source_update(void *data, obs_data_t *settings)
{
	obs_log(LOG_INFO, "Updating URL Source");
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);
	// Update the request data from the settings
	usd->update_timer_ms = (uint32_t)obs_data_get_int(settings, "update_timer");

	// serialize request data
	std::string serialized_request_data = serialize_request_data(&(usd->request_data));
	// Save on settings
	obs_data_set_string(settings, "request_data", serialized_request_data.c_str());
}

static void url_source_defaults(obs_data_t *s)
{
	// Default URL input string
	obs_data_set_default_string(s, "url", "https://catfact.ninja/fact");
	// Default update timer setting in milliseconds
	obs_data_set_default_int(s, "update_timer", 100);
}

static obs_properties_t *url_source_properties(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);

	obs_properties_t *ppts = obs_properties_create();
	// URL input string
	obs_property_t *urlprop = obs_properties_add_text(ppts, "url", "URL", OBS_TEXT_DEFAULT);
	// Disable the URL input since it's setup via the Request Builder dialog
	obs_property_set_enabled(urlprop, false);

	obs_properties_add_button2(
		ppts, "button", "Setup Request",
		[](obs_properties_t *, obs_property_t *, void *button_data) {
			struct url_source_data *button_usd =
				reinterpret_cast<struct url_source_data *>(button_data);
			// Open the Request Builder dialog
			RequestBuilder *builder = new RequestBuilder(
				&(button_usd->request_data),
				[&button_usd]() {
					// Update the request data from the settings
					obs_data_t *settings =
						obs_source_get_settings(button_usd->source);
					// serialize request data
					std::string serialized_request_data =
						serialize_request_data(&(button_usd->request_data));
					// Save on settings
					obs_data_set_string(settings, "request_data",
							    serialized_request_data.c_str());
					// Update the URL input string
					obs_data_set_string(settings, "url",
							    button_usd->request_data.url.c_str());
				},
				(QWidget *)obs_frontend_get_main_window());
			builder->show();
			return true;
		},
		usd);

	// Update timer setting in milliseconds
	obs_properties_add_int(ppts, "update_timer", "Update Timer (ms)", 100, 10000, 100);

	return ppts;
}

static uint32_t url_source_size(void *data)
{
	UNUSED_PARAMETER(data);
	return 32;
}

struct obs_source_info url_source {
	.id = "url_source", .type = OBS_SOURCE_TYPE_INPUT, .output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.get_name = url_source_name, .create = url_source_create, .destroy = url_source_destroy,
	.get_defaults = url_source_defaults, .get_properties = url_source_properties,
	.update = url_source_update
};
