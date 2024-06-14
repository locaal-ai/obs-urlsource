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

struct input_data {
	std::string name;
	std::string source;
	bool no_empty;
	bool no_same;
	bool aggregate;
	int agg_method;
	std::string resize_method;
	std::string last_obs_text_source_value;
	std::string aggregate_to_empty_buffer;
	uint64_t agg_buffer_begin_ts;
};

typedef std::vector<input_data> inputs_data;

std::string serialize_input_mapping_data(const inputs_data &data);
inputs_data deserialize_input_mapping_data(const std::string &data);

#endif // MAPPING_DATA_H
