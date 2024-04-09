#ifndef MAPPING_DATA_H
#define MAPPING_DATA_H

#include <string>
#include <vector>

const std::string none_internal_rendering = "None / Internal rendering";

struct output_mapping {
	std::string name;
	std::string output_source;
	std::string template_string;
	std::string css_props;
	bool unhide_output_source = false;
};

struct output_mapping_data {
	std::vector<output_mapping> mappings;
};

std::string serialize_output_mapping_data(const output_mapping_data &data);
output_mapping_data deserialize_output_mapping_data(const std::string &data);

#endif // MAPPING_DATA_H
