#ifndef ESP_NOW_RECEIVER_H
#define ESP_NOW_RECEIVER_H

#include "esp_err.h"
#include "esp_now.h"

#define MAX_PEERS 10

typedef struct {
    uint8_t sender_id[6];
    uint8_t content[250];
    uint8_t content_len;
} esp_now_message_t;

extern esp_now_peer_info_t peers_table[MAX_PEERS];

esp_err_t esp_now_receiver_init(void);

void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len);

esp_err_t esp_now_add_all_peers(void);

esp_now_peer_info_t* get_peer_by_id(uint8_t device_id);

#endif // ESP_NOW_RECEIVER_H
