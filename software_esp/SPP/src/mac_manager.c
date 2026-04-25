#include "mac_manager.h"
#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

char my_id[16] = "UNKNOWN";
// Zmienna używana w głównym kodzie
// Zostaje ona zadeklarowana jako 'extern' w pliku nagłówkowym
// a jej fizyczne umieszczenie zachodzi w pliku main.c

static void update_device_id_from_mac(const uint8_t *mac) {
    snprintf(my_id, sizeof(my_id), "ESP-%02X%02X", mac[4], mac[5]);
}

void save_mac_to_nvs(const uint8_t *mac) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(my_handle, "custom_mac", mac, 6);
    if (err == ESP_OK) {
        err = nvs_commit(my_handle);
        if (err == ESP_OK) {
            printf("Saved custom MAC to NVS: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        }

        update_device_id_from_mac(mac);
    
        
    } else {
        printf("Failed to save MAC to NVS: %s\n", esp_err_to_name(err));
    }
    nvs_close(my_handle);
}

void load_and_set_mac_from_nvs(void) {
    nvs_handle_t my_handle;
    esp_err_t err;
    uint8_t mac[6];
    size_t required_size = sizeof(mac);

    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        printf("NVS error: %s (No custom MAC configured yet?)\n", esp_err_to_name(err));
        return;
    }

    err = nvs_get_blob(my_handle, "custom_mac", mac, &required_size);
    if (err == ESP_OK && required_size == 6) {
        esp_wifi_set_mac(WIFI_IF_STA, mac);
        printf("Loaded custom MAC from NVS: %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        update_device_id_from_mac(mac);
    } else {
        printf("Custom MAC not found in NVS (%s).\n", esp_err_to_name(err));
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
            update_device_id_from_mac(mac);
        }
    }
    nvs_close(my_handle);
}