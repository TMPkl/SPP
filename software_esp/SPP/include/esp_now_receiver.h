#ifndef ESP_NOW_RECEIVER_H
#define ESP_NOW_RECEIVER_H

#include "esp_err.h"

typedef struct {
    uint8_t sender_id[6];
    uint8_t content[250];
    uint8_t content_len;
} esp_now_message_t;

esp_err_t esp_now_receiver_init(void);

void esp_now_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);

esp_err_t esp_now_add_all_peers(void);

#endif // ESP_NOW_RECEIVER_H
