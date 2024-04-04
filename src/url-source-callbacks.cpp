
#include "url-source-callbacks.h"
#include "url-source-data.h"
#include "obs-source-util.h"
#include "plugin-support.h"

#include <obs-module.h>

#include <mutex>

void acquire_weak_output_source_ref(struct url_source_data *usd)
{
	if (!is_valid_output_source_name(usd->output_source_name)) {
		obs_log(LOG_ERROR, "output_source_name is invalid");
		// text source is not selected
		return;
	}

	if (!usd->output_source_mutex) {
		obs_log(LOG_ERROR, "output_source_mutex is null");
		return;
	}

	std::lock_guard<std::mutex> lock(*usd->output_source_mutex);

	// acquire a weak ref to the new text source
	obs_source_t *source = obs_get_source_by_name(usd->output_source_name);
	if (source) {
		usd->output_source = obs_source_get_weak_source(source);
		obs_source_release(source);
		if (!usd->output_source) {
			obs_log(LOG_ERROR, "failed to get weak source for text source %s",
				usd->output_source_name);
		}
	} else {
		obs_log(LOG_ERROR, "text source '%s' not found", usd->output_source_name);
	}
}

void setTextCallback(const std::string &str, struct url_source_data *usd)
{
	if (!usd->output_source_mutex) {
		obs_log(LOG_ERROR, "output_source_mutex is null");
		return;
	}

	if (!usd->output_source) {
		// attempt to acquire a weak ref to the text source if it's yet available
		acquire_weak_output_source_ref(usd);
	}

	std::lock_guard<std::mutex> lock(*usd->output_source_mutex);

	obs_weak_source_t *text_source = usd->output_source;
	if (!text_source) {
		obs_log(LOG_ERROR, "text_source is null");
		return;
	}
	auto target = obs_weak_source_get_source(text_source);
	if (!target) {
		obs_log(LOG_ERROR, "text_source target is null");
		return;
	}
	auto target_settings = obs_source_get_settings(target);
	if (strcmp(obs_source_get_id(target), "ffmpeg_source") == 0) {
		// if the target source is a media source - set the input field to the text and disable the local file
		obs_data_set_bool(target_settings, "is_local_file", false);
		obs_data_set_bool(target_settings, "clear_on_media_end", true);
		obs_data_set_string(target_settings, "local_file", "");
		obs_data_set_string(target_settings, "input", str.c_str());
		obs_data_set_bool(target_settings, "looping", false);
	} else {
		// if the target source is a text source - set the text field
		obs_data_set_string(target_settings, "text", str.c_str());
	}
	obs_source_update(target, target_settings);
	if (usd->unhide_output_source) {
		// unhide the output source
		obs_source_set_enabled(target, true);
		obs_enum_scenes(
			[](void *target_ptr, obs_source_t *scene_source) -> bool {
				obs_scene_t *scene = obs_scene_from_source(scene_source);
				if (scene == nullptr) {
					return true;
				}
				obs_sceneitem_t *scene_item = obs_scene_sceneitem_from_source(
					scene, (obs_source_t *)target_ptr);
				if (scene_item == nullptr) {
					return true;
				}
				obs_sceneitem_set_visible(scene_item, true);
				return false; // stop enumerating
			},
			target);
	}
	obs_data_release(target_settings);
	obs_source_release(target);
};

void setAudioCallback(const std::string &str, struct url_source_data *usd)
{
	if (!usd->output_source_mutex) {
		obs_log(LOG_ERROR, "output_source_mutex is null");
		return;
	}

	if (!usd->output_source) {
		// attempt to acquire a weak ref to the output source if it's yet available
		acquire_weak_output_source_ref(usd);
	}

	std::lock_guard<std::mutex> lock(*usd->output_source_mutex);

	obs_weak_source_t *media_source = usd->output_source;
	if (!media_source) {
		obs_log(LOG_ERROR, "output_source is null");
		return;
	}
	auto target = obs_weak_source_get_source(media_source);
	if (!target) {
		obs_log(LOG_ERROR, "output_source target is null");
		return;
	}
	// assert the source is a media source
	if (strcmp(obs_source_get_id(target), "ffmpeg_source") != 0) {
		obs_source_release(target);
		obs_log(LOG_ERROR, "output_source is not a media source");
		return;
	}
	obs_data_t *media_settings = obs_source_get_settings(target);
	obs_data_set_bool(media_settings, "is_local_file", true);
	obs_data_set_bool(media_settings, "clear_on_media_end", true);
	obs_data_set_string(media_settings, "local_file", str.c_str());
	obs_data_set_bool(media_settings, "looping", false);
	obs_source_update(target, media_settings);
	obs_data_release(media_settings);

	obs_source_media_restart(target);
	obs_source_release(target);
};

std::string renderOutputTemplate(inja::Environment &env, const std::string &input,
				 const std::vector<std::string> &outputs,
				 const nlohmann::json &body)
try {
	// Use Inja to render the template
	nlohmann::json data;
	if (outputs.size() > 1) {
		for (size_t i = 0; i < outputs.size(); i++) {
			data["output" + std::to_string(i)] = outputs[i];
		}
		// in "output" add an array of all the outputs
		data["output"] = outputs;
	} else {
		data["output"] = outputs[0];
	}
	data["body"] = body;
	return env.render(input, data);
} catch (std::exception &e) {
	obs_log(LOG_ERROR, "Failed to parse template: %s", e.what());
	return "";
}
