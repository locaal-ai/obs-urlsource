#ifndef URL_SOURCE_H
#define URL_SOURCE_H

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

obs_properties_t *url_source_properties(void *data);
void url_source_defaults(obs_data_t *s);
void *url_source_create(obs_data_t *settings, obs_source_t *source);
void url_source_destroy(void *data);
void url_source_update(void *data, obs_data_t *settings);
const char *url_source_name(void *unused);
void url_source_activate(void *data);
void url_source_deactivate(void *data);

#ifdef __cplusplus
}
#endif

#endif /* URL_SOURCE_H */
