
#include "obs-source-util.h"
#include "plugin-support.h"

#include <obs-module.h>

#include <QBuffer>
#include <QImage>

#include <nlohmann/json.hpp>

void init_source_render_data(source_render_data *tf)
{
	tf->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	tf->stagesurface = nullptr;
}

void destroy_source_render_data(source_render_data *tf)
{
	if (tf->texrender) {
		gs_texrender_destroy(tf->texrender);
		tf->texrender = nullptr;
	}
	if (tf->stagesurface) {
		gs_stagesurface_destroy(tf->stagesurface);
		tf->stagesurface = nullptr;
	}
}

/**
  * @brief Get RGBA from the stage surface
  *
  * @param tf  The filter data
  * @param width  The width of the stage surface (output)
  * @param height  The height of the stage surface (output)
  * @param scale  Scale the output by this factor
  * @return The RGBA buffer (4 bytes per pixel) or an empty vector if there was an error
*/
std::vector<uint8_t> get_rgba_from_source_render(obs_source_t *source, source_render_data *tf,
						 uint32_t &width, uint32_t &height, float scale)
{
	if (!obs_source_enabled(source)) {
		obs_log(LOG_ERROR, "Source is not enabled");
		return std::vector<uint8_t>();
	}

	width = obs_source_get_base_width(source);
	height = obs_source_get_base_height(source);
	if (width == 0 || height == 0) {
		obs_log(LOG_ERROR, "Width or height is 0");
		return std::vector<uint8_t>();
	}
	// scale the width and height
	width = (uint32_t)((float)width * scale);
	height = (uint32_t)((float)height * scale);

	// enter graphics context
	obs_enter_graphics();

	gs_texrender_reset(tf->texrender);
	if (!gs_texrender_begin(tf->texrender, width, height)) {
		obs_log(LOG_ERROR, "Could not begin texrender");
		return std::vector<uint8_t>();
	}
	struct vec4 background;
	vec4_zero(&background);
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f,
		 100.0f);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	obs_source_video_render(source);
	gs_blend_state_pop();
	gs_texrender_end(tf->texrender);

	if (tf->stagesurface) {
		uint32_t stagesurf_width = gs_stagesurface_get_width(tf->stagesurface);
		uint32_t stagesurf_height = gs_stagesurface_get_height(tf->stagesurface);
		if (stagesurf_width != width || stagesurf_height != height) {
			gs_stagesurface_destroy(tf->stagesurface);
			tf->stagesurface = nullptr;
		}
	}
	if (!tf->stagesurface) {
		tf->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
	}
	gs_stage_texture(tf->stagesurface, gs_texrender_get_texture(tf->texrender));
	uint8_t *video_data;
	uint32_t linesize;
	if (!gs_stagesurface_map(tf->stagesurface, &video_data, &linesize)) {
		obs_log(LOG_ERROR, "Cannot map stage surface");
		return std::vector<uint8_t>();
	}
	obs_log(LOG_INFO, "linesize: %d, width: %d, height: %d", linesize, width, height);
	if (linesize != width * 4) {
		obs_log(LOG_WARNING, "linesize %d != width %d * 4", linesize, width);
	}
	std::vector<uint8_t> rgba(width * height * 4);
	for (uint32_t i = 0; i < height; i++) {
		memcpy(rgba.data() + i * width * 4, video_data + i * linesize, width * 4);
	}

	gs_stagesurface_unmap(tf->stagesurface);

	// leave graphics context
	obs_leave_graphics();

	return rgba;
}

std::string convert_rgba_buffer_to_png_base64(const std::vector<uint8_t> &rgba, uint32_t width,
					      uint32_t height)
{
	// use Qt to convert the RGBA buffer to an encoded image buffer
	QImage image(rgba.data(), width, height, QImage::Format_RGBA8888);
	QByteArray ba;
	QBuffer buffer(&ba);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "PNG");
	buffer.close();

	// convert the encoded image buffer to a base64 string
	QByteArray base64 = ba.toBase64();
	std::string base64_str = base64.toStdString();

	// json string escape
	nlohmann::json j(base64_str);
	std::string escaped = j.dump();
	// remove the quotes
	escaped = escaped.substr(1, escaped.size() - 2);

	return escaped;
}

std::string get_source_name_without_prefix(const std::string &source_name)
{
	if (source_name.size() > 0 && source_name[0] == '(') {
		size_t end = source_name.find(')');
		if (end != std::string::npos && end + 2 < source_name.size()) {
			return source_name.substr(end + 2);
		}
	}
	return source_name;
}
