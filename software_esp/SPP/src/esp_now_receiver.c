#include "esp_now_receiver.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "string.h"
#include "mac_manager.h"


static const char *TAG = "ESP_NOW_RECEIVER";

esp_now_peer_info_t peers_table[MAX_PEERS] = {};

esp_now_peer_info_t* get_peer_by_id(uint8_t device_id) {
    if (device_id >= MAX_PEERS) return NULL;
    return &peers_table[device_id];
}

void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    if (recv_info == NULL || data == NULL) {
        ESP_LOGE(TAG, "Błędny wskaźnik: recv_info=%p, data=%p", recv_info, data);
        return;
    }

    const uint8_t *mac_addr = recv_info->src_addr;
    int msg_len = (data_len > 250) ? 250 : data_len;

    ESP_LOGI(TAG, "Wiadomość od: %02X:%02X:%02X:%02X:%02X:%02X",mac_addr[0], mac_addr[1], mac_addr[2],mac_addr[3], mac_addr[4], mac_addr[5]);

    ESP_LOGI(TAG, "Zawartość- %d bajtów: %.*s",
             msg_len, msg_len, (const char *)data);

    if (data_len > 0) {
        char hex_buffer[251 * 3];
        int pos = 0;
        for (int i = 0; i < msg_len; i++) {
            pos += snprintf(hex_buffer + pos, sizeof(hex_buffer) - pos,"%02X ", data[i]);
        }
        ESP_LOGD(TAG, "Hex dump: %s", hex_buffer);
    }
}

esp_err_t esp_now_receiver_init(void) {
    esp_err_t ret;

    wifi_mode_t mode;
    ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Nie udało się pobrać trybu Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    if (mode == WIFI_MODE_AP) {
        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    } else if (mode == WIFI_MODE_NULL) {
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Nie udało się ustawić trybu Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Inicjalizacja ESP-NOW nieudana: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_now_register_recv_cb(esp_now_recv_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Rejestracja callback'u nieudana: %s", esp_err_to_name(ret));
        esp_now_deinit();
        return ret;
    }

    ESP_LOGI(TAG, "ESP-NOW inicjalizacja powodzona");
    return ESP_OK;
}

esp_err_t esp_now_add_all_peers(void) {
    uint8_t own_mac[6];
    get_current_mac(own_mac);
    
    uint8_t own_device_id = own_mac[5];
    ESP_LOGI(TAG, "Adding peers - own device ID: %02X (MAC: %02X:%02X:%02X:%02X:%02X:%02X)",own_device_id, own_mac[0], own_mac[1], own_mac[2], own_mac[3], own_mac[4], own_mac[5]);
    
    for (uint8_t device_id = 0; device_id < 10; device_id++) {
        if (device_id == own_device_id) {
            ESP_LOGI(TAG, "Skipping self (ID: %02X)", device_id);
            continue;
        }
        
        uint8_t peer_mac[6];
        memcpy(peer_mac, own_mac, 6);
        peer_mac[5] = device_id;
        
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, peer_mac, 6);
        peer.channel = 0;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        
        if (esp_now_add_peer(&peer) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add peer ID: %02X", device_id);
        } else {
            memcpy(&peers_table[device_id], &peer, sizeof(esp_now_peer_info_t));
            ESP_LOGI(TAG, "Added peer ID: %02X (MAC: %02X:%02X:%02X:%02X:%02X:%02X)",device_id, peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]);
        }
    }
    
    return ESP_OK;
}
