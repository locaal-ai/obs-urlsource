#ifndef URL_SOURCE_THREAD_H
#define URL_SOURCE_THREAD_H

#include "url-source-data.h"

void curl_loop(struct url_source_data *usd);
void stop_and_join_curl_thread(struct url_source_data *usd);

#endif
