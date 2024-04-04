
#include "url-source-thread.h"
#include "url-source-callbacks.h"
#include "request-data.h"
#include "plugin-support.h"
#include "obs-source-util.h"
#include "ui/text-render-helper.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <inja/inja.hpp>

void curl_loop(struct url_source_data *usd)
{
	obs_log(LOG_INFO, "Starting URL Source thread, update timer: %d", usd->update_timer_ms);
	uint64_t cur_time = get_time_ns();
	uint64_t start_time = cur_time;

	usd->frame.format = VIDEO_FORMAT_BGRA;

	inja::Environment env;

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

			if (usd->request_data.output_type == "Audio (data)") {
				if (!is_valid_output_source_name(usd->output_source_name)) {
					obs_log(LOG_ERROR,
						"Must select an output source for audio output");
				} else {
					setAudioCallback(response.body, usd);
				}
			} else {
				// prepare the text from the template
				std::string text = usd->output_text_template;
				// if the template is empty use the response body
				if (text.empty()) {
					text = response.body_parts_parsed[0];
				} else {
					// if output is image or image-URL - fetch the image and convert it to base64
					if (usd->output_is_image_url ||
					    usd->request_data.output_type == "Image (data)") {
						std::vector<uint8_t> image_data;
						std::string mime_type = "image/png";
						if (usd->request_data.output_type ==
						    "Image (data)") {
							// if the output type is image data - use the response body bytes
							image_data = response.body_bytes;
							// get the mime type from the response headers if available
							if (response.headers.find("content-type") !=
							    response.headers.end()) {
								mime_type =
									response.headers
										["content-type"];
							}
						} else {
							text = renderOutputTemplate(
								env, text,
								response.body_parts_parsed,
								response.body_json);
							// use fetch_image to get the image
							image_data = fetch_image(text.c_str(),
										 mime_type);
						}
						// convert the image to base64
						const std::string base64_image =
							base64_encode(image_data);
						// build an image tag with the base64 image
						text = "<img src=\"data:" + mime_type + ";base64," +
						       base64_image + "\" />";
					} else {
						text = renderOutputTemplate(
							env, text, response.body_parts_parsed,
							response.body_json);
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

				if (is_valid_output_source_name(usd->output_source_name)) {
					// If an output source is selected - use it for rendering
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
					uint32_t width = usd->render_width;
					uint32_t height = 0;

					// render the text with QTextDocument
					render_text_with_qtextdocument(
						text, width, height, &renderBuffer, usd->css_props);
					// Update the frame
					usd->frame.data[0] = renderBuffer;
					usd->frame.linesize[0] = width * 4;
					usd->frame.width = width;
					usd->frame.height = height;

					// Send the frame
					obs_source_output_video(usd->source, &usd->frame);
				} // end if not text source
			}         // end if not audio
		}                 // end if request success

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
