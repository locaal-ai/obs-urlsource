#include "url-source.h"

struct obs_source_info url_source = {
	.id = "url_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.get_name = url_source_name,
	.create = url_source_create,
	.destroy = url_source_destroy,
	.get_defaults = url_source_defaults,
	.get_properties = url_source_properties,
	.update = url_source_update,
	.activate = url_source_activate,
	.deactivate = url_source_deactivate,
};
