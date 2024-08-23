
#include "mapping-data.h"
#include <nlohmann/json.hpp>

std::string serialize_output_mapping_data(const output_mapping_data &data)
{
	nlohmann::json j;
	for (const auto &mapping : data.mappings) {
		nlohmann::json j_mapping;
		j_mapping["name"] = mapping.name;
		j_mapping["output_source"] = mapping.output_source;
		j_mapping["template_string"] = mapping.template_string;
		j_mapping["css_props"] = mapping.css_props;
		j_mapping["unhide_output_source"] = mapping.unhide_output_source;
		j_mapping["file_path"] = mapping.file_path;
		j.push_back(j_mapping);
	}
	return j.dump();
}

output_mapping_data deserialize_output_mapping_data(const std::string &data)
{
	output_mapping_data result;
	nlohmann::json j = nlohmann::json::parse(data);
	for (const auto &j_mapping : j) {
		output_mapping mapping;
		mapping.name = j_mapping.value("name", "");
		mapping.output_source = j_mapping.value("output_source", "");
		mapping.template_string = j_mapping.value("template_string", "");
		mapping.css_props = j_mapping.value("css_props", "");
		mapping.unhide_output_source = j_mapping.value("unhide_output_source", false);
		mapping.file_path = j_mapping.value("file_path", "");
		result.mappings.push_back(mapping);
	}
	return result;
}

nlohmann::json serialize_input_mapping_data(const inputs_data &data)
{
	nlohmann::json j;
	for (const auto &input : data) {
		nlohmann::json j_input;
		j_input["source"] = input.source;
		j_input["no_empty"] = input.no_empty;
		j_input["no_same"] = input.no_same;
		j_input["aggregate"] = input.aggregate;
		j_input["agg_method"] = input.agg_method;
		j_input["resize_method"] = input.resize_method;
		j.push_back(j_input);
	}
	return j;
}

inputs_data deserialize_input_mapping_data(const std::string &data)
{
	inputs_data result;
	nlohmann::json j = nlohmann::json::parse(data);
	for (const auto &j_input : j) {
		input_data input;
		input.source = j_input.value("source", "");
		input.no_empty = j_input.value("no_empty", false);
		input.no_same = j_input.value("no_same", false);
		input.aggregate = j_input.value("aggregate", false);
		input.agg_method = j_input.value("agg_method", -1);
		input.resize_method = j_input.value("resize_method", "");
		result.push_back(input);
	}
	return result;
}
