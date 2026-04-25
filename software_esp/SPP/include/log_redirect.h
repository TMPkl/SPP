#ifndef LOG_REDIRECT_H
#define LOG_REDIRECT_H

#include "esp_err.h"

void init_tcp_logger();
esp_err_t tcp_logger_connect(const char *ingest_url, const char *ingest_token);

#endif // LOG_REDIRECT_H