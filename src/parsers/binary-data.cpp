#include "errors.h"
#include "request-data.h"
#include "plugin-support.h"
#include "string-util.h"

#include <obs-module.h>

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

std::string save_to_temp_file(const std::vector<uint8_t> &data, const std::string &extension)
{
	// check if the config folder exists if it doesn't exist, create it.
	char *config_path = obs_module_config_path("");
	if (!std::filesystem::exists(config_path)) {
		if (!std::filesystem::create_directory(config_path)) {
			obs_log(LOG_ERROR, "Failed to create config directory %s", config_path);
			bfree(config_path);
			return "";
		}
	}
	bfree(config_path);

	// append the extension to the file name
	std::string file_name = "temp." + extension;
	char *temp_file_path = obs_module_config_path(file_name.c_str());

	obs_log(LOG_INFO, "save to temp file %s, %d bytes", temp_file_path, data.size());

	std::ofstream temp_file(temp_file_path, std::ios::binary);
	bfree(temp_file_path);
	temp_file.write((const char *)data.data(), data.size());
	temp_file.close();
	return std::string(temp_file_path);
}

struct request_data_handler_response parse_image_data(struct request_data_handler_response response,
						      const url_source_request_data *request_data)
{
    UNUSED_PARAMETER(request_data);

	// find the image type from the content type on the response.headers map
	std::string content_type = response.headers["content-type"];
	std::string image_type = content_type.substr(content_type.find("/") + 1);

	// if the image type is not supported, return an error
	if (image_type != "png" && image_type != "jpg" && image_type != "jpeg" &&
	    image_type != "gif") {
		return make_fail_parse_response("Unsupported image type: " + image_type);
	}

	// save the image to a temporary file
	std::string temp_file_path = save_to_temp_file(response.body_bytes, image_type);
	response.body = temp_file_path;

	return response;
}

struct request_data_handler_response parse_audio_data(struct request_data_handler_response response,
						      const url_source_request_data *request_data)
{
    UNUSED_PARAMETER(request_data);

	// find the audio type from the content type on the response.headers map
	std::string content_type = response.headers["content-type"];
	std::string audio_type = content_type.substr(content_type.find("/") + 1);
	audio_type = trim(audio_type);

	// if the audio type is not supported, return an error
	if (!(audio_type == "mp3" || audio_type == "mpeg" || audio_type == "wav" ||
	      audio_type == "ogg" || audio_type == "flac" || audio_type == "aac")) {
		return make_fail_parse_response("Unsupported audio type: " + audio_type);
	}

	// save the audio to a temporary file
	std::string temp_file_path = save_to_temp_file(response.body_bytes, audio_type);
	response.body = temp_file_path;

	return response;
}
