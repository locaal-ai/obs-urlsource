#ifndef URL_SOURCE_CALLBACKS_H
#define URL_SOURCE_CALLBACKS_H

#include <obs-module.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <inja/inja.hpp>

std::string renderOutputTemplate(inja::Environment &env, const std::string &input,
				 const std::vector<std::string> &outputs,
				 const nlohmann::json &body);

void setAudioCallback(const std::string &str, struct url_source_data *usd);
void setTextCallback(const std::string &str, struct url_source_data *usd);

#endif
