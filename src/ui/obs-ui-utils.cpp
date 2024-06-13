
#include "obs-ui-utils.h"

#include <obs.h>
#include <obs-module.h>

#include <QComboBox>

// add_sources_to_list is a helper function that adds all text and media sources to the list
bool add_sources_to_combobox(void *list_property, obs_source_t *source)
{
	// add all text and media sources to the list
	auto source_id = obs_source_get_id(source);
	if (strcmp(source_id, "text_ft2_source_v2") != 0 &&
	    strcmp(source_id, "text_gdiplus_v2") != 0 && strcmp(source_id, "ffmpeg_source") != 0 &&
	    strcmp(source_id, "image_source") != 0) {
		return true;
	}

	QComboBox *sources = static_cast<QComboBox *>(list_property);
	const char *name = obs_source_get_name(source);
	std::string name_with_prefix;
	// add a prefix to the name to indicate the source type
	if (strcmp(source_id, "text_ft2_source_v2") == 0 ||
	    strcmp(source_id, "text_gdiplus_v2") == 0) {
		name_with_prefix = std::string("(Text) ").append(name);
	} else if (strcmp(source_id, "image_source") == 0) {
		name_with_prefix = std::string("(Image) ").append(name);
	} else if (strcmp(source_id, "ffmpeg_source") == 0) {
		name_with_prefix = std::string("(Media) ").append(name);
	}
	sources->addItem(name_with_prefix.c_str());
	return true;
}
