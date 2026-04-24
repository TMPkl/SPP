#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "mac_manager.h"

#define BLINK_GPIO 48

int flaga = 0;

void app_main(void) {
    printf("Starting WiFi with unique MAC...\n");
    // Inicjalizacja NVS (wymagane dla WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    // Inicjalizacja event loop
    esp_event_loop_create_default();
    
    // Inicjalizacja WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    // (Opcjonalne, tylko na produkcji) - Przykładowy zapis adresu do NVS
    // Normalnie uruchamiasz to tylko raz, na etapie flashowania urządzenia.
    
    uint8_t factory_mac[6] = {0xA0, 0x11, 0x84, 0xAA, 0x2C, 0x03};
    save_mac_to_nvs(factory_mac);
    
    // Ustaw unikalny MAC z NVS (jeśli istnieje) PRZED esp_wifi_start()
    load_and_set_mac_from_nvs();
    
    // Konfiguracja WiFi STA mode
    esp_wifi_set_mode(WIFI_MODE_STA);
    
     wifi_config_t wifi_config = {
         .sta = {
             .ssid = "karol",
             .password = "511585031",
         },
     };
    
     esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
     esp_wifi_start();
     esp_wifi_connect();
    
     printf("WiFi initialized with unique MAC\n");
    
    // Tutaj Twój kod dla "poets and poetry circles"
    
        gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    uint8_t mac[6];
    
    // Pobierz aktualnie ustawiony MAC interfejsu
    esp_wifi_get_mac(WIFI_IF_STA, mac);


    while(1) {
        /* Blink off (output low) */
        printf("Turning off the LED\n");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        printf("Turning on the LED\n");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
            printf("Current MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("Flaga: %d\n", flaga);
    }
    }