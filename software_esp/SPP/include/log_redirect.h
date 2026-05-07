#ifndef LOG_REDIRECT_H
#define LOG_REDIRECT_H

#include "esp_err.h"
#include "esp_log.h"
#include "lamportTS.h"

void init_tcp_logger();
esp_err_t tcp_logger_connect(const char *ingest_url, const char *ingest_token);

// Custom logging macros with Lamport Timestamp
#undef ESP_LOGI
#undef ESP_LOGW
#undef ESP_LOGE
#undef ESP_LOGD

#define ESP_LOGI(tag, fmt, ...) \
    esp_log_write(ESP_LOG_INFO, tag, "[%llu] " fmt "\n", lamport_get_time(), ##__VA_ARGS__)

#define ESP_LOGW(tag, fmt, ...) \
    esp_log_write(ESP_LOG_WARN, tag, "[%llu] " fmt "\n", lamport_get_time(), ##__VA_ARGS__)

#define ESP_LOGE(tag, fmt, ...) \
    esp_log_write(ESP_LOG_ERROR, tag, "[%llu] " fmt "\n", lamport_get_time(), ##__VA_ARGS__)

#define ESP_LOGD(tag, fmt, ...) \
    esp_log_write(ESP_LOG_DEBUG, tag, "[%llu] " fmt "\n", lamport_get_time(), ##__VA_ARGS__)

#endif // LOG_REDIRECT_H