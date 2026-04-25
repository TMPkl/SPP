#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "log_redirect.h"
#include "mac_manager.h"

#define LOGGER_QUEUE_LEN 32
#define LOGGER_LINE_MAX 256
#define LOGGER_TASK_STACK_SIZE 12288

typedef struct {
    char line[LOGGER_LINE_MAX];
} logger_msg_t;

static bool logger_ready = false;
static bool suppress_capture = false;
static char ingest_url[256];
static char ingest_token[128];
static char escaped_id_buf[48];
static char escaped_line_buf[512];
static char payload_buf[640];
static QueueHandle_t logger_queue = NULL;
static TaskHandle_t logger_task_handle = NULL;

static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 2 < dst_size; ++i) {
        char c = src[i];
        if (c == '\\' || c == '"') {
            if (j + 2 >= dst_size) {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = c;
            continue;
        }
        if (c == '\n') {
            if (j + 2 >= dst_size) {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = 'n';
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if ((unsigned char)c < 0x20) {
            continue;
        }
        dst[j++] = c;
    }
    dst[j] = '\0';
}

static void post_log_line(const char *line)
{
    if (!logger_ready || line == NULL || line[0] == '\0') {
        return;
    }

    json_escape(my_id, escaped_id_buf, sizeof(escaped_id_buf));
    json_escape(line, escaped_line_buf, sizeof(escaped_line_buf));

    int payload_len = snprintf(payload_buf, sizeof(payload_buf),
                               "{\"id\":\"%s\",\"line\":\"%s\"}",
                               escaped_id_buf, escaped_line_buf);
    if (payload_len <= 0 || payload_len >= (int)sizeof(payload_buf)) {
        return;
    }

    esp_http_client_config_t config = {
        .url = ingest_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    suppress_capture = true;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        suppress_capture = false;
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Log-Token", ingest_token);
    esp_http_client_set_post_field(client, payload_buf, payload_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 400) {
            // Server rejected the payload; skip this line and continue logging.
        }
    }

    esp_http_client_cleanup(client);
    suppress_capture = false;
}

static bool normalize_log_line(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    bool has_content = false;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; ++i) {
        char c = src[i];
        if (c == '\r' || c == '\n') {
            continue;
        }
        dst[j++] = c;
        if (c != ' ' && c != '\t') {
            has_content = true;
        }
    }

    while (j > 0 && (dst[j - 1] == ' ' || dst[j - 1] == '\t')) {
        --j;
    }

    dst[j] = '\0';
    return has_content && j > 0;
}

static void enqueue_log_line(const char *raw_line)
{
    if (!logger_ready || suppress_capture || logger_queue == NULL) {
        return;
    }

    logger_msg_t msg;
    if (!normalize_log_line(raw_line, msg.line, sizeof(msg.line))) {
        return;
    }

    (void)xQueueSend(logger_queue, &msg, 0);
}

static void logger_http_task(void *arg)
{
    (void)arg;
    logger_msg_t msg;

    while (1) {
        if (xQueueReceive(logger_queue, &msg, portMAX_DELAY) == pdTRUE) {
            post_log_line(msg.line);
        }
    }
}

static int tcp_vprintf(const char *fmt, va_list args)
{
    char buffer[LOGGER_LINE_MAX];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        enqueue_log_line(buffer);
    }

    // Dodatkowo wypisz na standardowe wyjście (UART)
    vprintf(fmt, args_copy);
    va_end(args_copy);

    return len;
}

void init_tcp_logger()
{
    if (logger_queue == NULL) {
        logger_queue = xQueueCreate(LOGGER_QUEUE_LEN, sizeof(logger_msg_t));
    }

    if (logger_queue != NULL && logger_task_handle == NULL) {
        xTaskCreate(logger_http_task, "log_http", LOGGER_TASK_STACK_SIZE, NULL, 5, &logger_task_handle);
    }

    esp_log_set_vprintf(tcp_vprintf);
}

esp_err_t tcp_logger_connect(const char *url, const char *token)
{
    if (url == NULL || token == NULL || url[0] == '\0' || token[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (logger_queue == NULL || logger_task_handle == NULL) {
        return ESP_FAIL;
    }

    snprintf(ingest_url, sizeof(ingest_url), "%s", url);
    snprintf(ingest_token, sizeof(ingest_token), "%s", token);
    logger_ready = true;

    return ESP_OK;
}