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
#include "url-source-data.h"
#include "url-source-thread.h"
#include "url-source-callbacks.h"
#include "obs-source-util.h"

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

const char *url_source_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "URL Source";
}

void url_source_destroy(void *data)
{
	struct url_source_data *usd = reinterpret_cast<struct url_source_data *>(data);

	stop_and_join_curl_thread(usd);

	if (usd->output_source_name) {
		bfree(usd->output_source_name);
		usd->output_source_name = nullptr;
	}

	if (usd->output_source) {
		obs_weak_source_release(usd->output_source);
		usd->output_source = nullptr;
	}

	if (usd->output_source_mutex) {
		delete usd->output_source_mutex;
		usd->output_source_mutex = nullptr;
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

	usd->request_data.source_name = std::string(obs_source_get_name(source));
	usd->update_timer_ms = (uint32_t)obs_data_get_int(settings, "update_timer");
	usd->run_while_not_visible = obs_data_get_bool(settings, "run_while_not_visible");
	usd->output_is_image_url = obs_data_get_bool(settings, "is_image_url");
	usd->css_props = std::string(obs_data_get_string(settings, "css_props"));
	usd->output_text_template = std::string(obs_data_get_string(settings, "template"));
	usd->send_to_stream = obs_data_get_bool(settings, "send_to_stream");
	usd->unhide_output_source = obs_data_get_bool(settings, "unhide_output_source");

	// initialize the mutex
	usd->output_source_mutex = new std::mutex();
	usd->curl_mutex = new std::mutex();
	usd->curl_thread_cv = new std::condition_variable();
	usd->output_source_name = bstrdup(obs_data_get_string(settings, "text_sources"));
	usd->output_source = nullptr;

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
	usd->render_width = (uint32_t)obs_data_get_int(settings, "render_width");
	usd->unhide_output_source = obs_data_get_bool(settings, "unhide_output_source");

	// update the text source
	const char *new_text_source_name = obs_data_get_string(settings, "text_sources");
	obs_weak_source_t *old_weak_text_source = NULL;

	if (!is_valid_output_source_name(new_text_source_name)) {
		// new selected text source is not valid, release the old one
		if (usd->output_source) {
			std::lock_guard<std::mutex> lock(*usd->output_source_mutex);
			old_weak_text_source = usd->output_source;
			usd->output_source = nullptr;
		}
		if (usd->output_source_name) {
			bfree(usd->output_source_name);
			usd->output_source_name = nullptr;
		}
	} else {
		// new selected text source is valid, check if it's different from the old one
		if (usd->output_source_name == nullptr ||
		    strcmp(new_text_source_name, usd->output_source_name) != 0) {
			// new text source is different from the old one, release the old one
			if (usd->output_source) {
				std::lock_guard<std::mutex> lock(*usd->output_source_mutex);
				old_weak_text_source = usd->output_source;
				usd->output_source = nullptr;
			}
			usd->output_source_name = bstrdup(new_text_source_name);
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
	obs_data_set_default_bool(s, "unhide_output_source", false);

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

	// Default Render Width
	obs_data_set_default_int(s, "render_width", 640);
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
	// add all text and media sources to the list
	auto source_id = obs_source_get_id(source);
	if (strcmp(source_id, "text_ft2_source_v2") != 0 &&
	    strcmp(source_id, "text_gdiplus_v2") != 0 && strcmp(source_id, "ffmpeg_source") != 0) {
		return true;
	}

	obs_property_t *sources = (obs_property_t *)list_property;
	const char *name = obs_source_get_name(source);
	std::string name_with_prefix;
	// add a prefix to the name to indicate the source type
	if (strcmp(source_id, "text_ft2_source_v2") == 0 ||
	    strcmp(source_id, "text_gdiplus_v2") == 0) {
		name_with_prefix = std::string("(Text) ").append(name);
	} else if (strcmp(source_id, "image_source") == 0) {
		name_with_prefix = std::string("(Image) ").append(name);
	} else if (strcmp(source_id, "ffmpeg_source") == 0) {
		name_with_prefix = std::string("(Media) ").append(name);
	}
	obs_property_list_add_string(sources, name_with_prefix.c_str(), name);
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

	obs_property_t *sources = obs_properties_add_list(ppts, "text_sources", "Output source",
							  OBS_COMBO_TYPE_LIST,
							  OBS_COMBO_FORMAT_STRING);
	// Add a "none" option
	obs_property_list_add_string(sources, "None / Internal rendering", "none");
	// Add the sources
	obs_enum_sources(add_sources_to_list, sources);

	// Checkbox for unhiding output source on update
	obs_properties_add_bool(ppts, "unhide_output_source", "Unhide output source on update");

	// add callback for source selection change to change visibility of unhide option
	obs_property_set_modified_callback(sources, [](obs_properties_t *props,
						       obs_property_t *property,
						       obs_data_t *settings) {
		UNUSED_PARAMETER(property);
		const char *selected_source_name = obs_data_get_string(settings, "text_sources");
		obs_property_t *unhide_output_source =
			obs_properties_get(props, "unhide_output_source");
		if (is_valid_output_source_name(selected_source_name)) {
			obs_property_set_visible(unhide_output_source, true);
		} else {
			obs_property_set_visible(unhide_output_source, false);
		}
		return true;
	});

	obs_properties_add_bool(ppts, "send_to_stream",
				"Send output to current stream as captions");

	// Is Image URL boolean checkbox
	obs_properties_add_bool(ppts, "is_image_url", "Output is image URL (fetch and show image)");

	// CSS properties for styling the text
	obs_properties_add_text(ppts, "css_props", "CSS Properties", OBS_TEXT_MULTILINE);

	// Output template
	obs_property_t *template_prop =
		obs_properties_add_text(ppts, "template", "Output Template", OBS_TEXT_MULTILINE);
	obs_property_set_long_description(
		template_prop,
		"Template processed by inja engine\n"
		"Use {{output}} for text representation of a single object/string\n"
		"Use {{outputN}}, where N is index of item starting at 0, for text "
		"representation of parts of an array (see Test Request button)\n"
		"Use {{body}} variable for unparsed object/array representation of the "
		"entire response");

	obs_properties_add_int(ppts, "render_width", "Render Width (px)", 100, 10000, 1);

	// Add a informative text about the plugin
	obs_properties_add_text(
		ppts, "info",
		QString(PLUGIN_INFO_TEMPLATE).arg(PLUGIN_VERSION).toStdString().c_str(),
		OBS_TEXT_INFO);

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
