#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "log_redirect.h"
#include "mac_manager.h"

#define LOGGER_QUEUE_LEN 256
#define LOGGER_LINE_MAX 256
#define LOGGER_TASK_STACK_SIZE 8192

typedef struct {
    char line[LOGGER_LINE_MAX];
} logger_msg_t;

static bool logger_ready = false;
static bool suppress_capture = false;
static char ingest_url[128];
static char ingest_token[64];
static uint8_t payload_buf[256];
static QueueHandle_t logger_queue = NULL;
static TaskHandle_t logger_task_handle = NULL;

static uint8_t hash_id(const char *id)
{
    size_t len = strlen(id);
    if (len >= 2) {
        char hex_str[3];
        hex_str[0] = id[len-2];
        hex_str[1] = id[len-1];
        hex_str[2] = '\0';
        return (uint8_t)strtol(hex_str, NULL, 16);
    }
    return 0;
}

static void post_log_line(const char *line)
{
    if (!logger_ready || line == NULL || line[0] == '\0') {
        return;
    }

    size_t line_len = strlen(line);
    if (line_len > 254) {
        line_len = 254;
    }

    payload_buf[0] = hash_id(my_id);
    payload_buf[1] = (uint8_t)line_len;
    memcpy(&payload_buf[2], line, line_len);
    int payload_len = 2 + line_len;

    esp_http_client_config_t config = {
        .url = ingest_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 2000,  // 2 sekundy dla lokalnej sieci
        .keep_alive_enable = true,
    };

    // Tylko krótki moment suppress żeby init nie zalogował
    suppress_capture = true;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    suppress_capture = false;  // Wznów zbieranie logów ASAP
    
    if (client == NULL) {
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_header(client, "X-Log-Token", ingest_token);
    esp_http_client_set_post_field(client, (const char *)payload_buf, payload_len);

    // Perform bez retry - szybko, nie traci logów
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

static void enqueue_log_line(const char *raw_line)
{
    if (!logger_ready || suppress_capture || logger_queue == NULL || raw_line == NULL) {
        return;
    }

    logger_msg_t msg;
    size_t j = 0;
    bool has_content = false;

    for (size_t i = 0; raw_line[i] != '\0' && j + 1 < sizeof(msg.line); ++i) {
        char c = raw_line[i];
        if (c == '\r' || c == '\n') continue;
        msg.line[j++] = c;
        if (c != ' ' && c != '\t') has_content = true;
    }

    while (j > 0 && (msg.line[j - 1] == ' ' || msg.line[j - 1] == '\t')) --j;
    
    if (!has_content || j == 0) return;
    
    msg.line[j] = '\0';
    // Czekaj (max 100ms) zamiast tracić logi
    if (xQueueSend(logger_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW("LOG_REDIR", "Logger queue full, dropped: %s", raw_line);
    }
}

static void logger_http_task(void *arg)
{
    (void)arg;
    logger_msg_t msg;
    uint32_t sent = 0;

    while (1) {
        if (xQueueReceive(logger_queue, &msg, portMAX_DELAY) == pdTRUE) {
            post_log_line(msg.line);
            sent++;
            // Co 100 wysłanych logów wyślij podsumowanie
            if (sent % 100 == 0) {
                ESP_LOGI("LOG_REDIR", "Total sent: %lu", sent);
            }
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