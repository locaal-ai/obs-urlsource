
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
		result.mappings.push_back(mapping);
	}
	return result;
}
