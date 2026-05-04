#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_now.h"
#include "esp_wifi.h"



#include "mac_manager.h"
#include "secrets.h"
#include "config.h"
#include "log_redirect.h"
#include "wifi_manager.h"
#include "messages.h"


#define BLINK_GPIO 48

// Callback funkcja do odbierania wiadomości
static void recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    Message *msg = (Message *)data;
    ESP_LOGI(my_id, "Received message from peer: id=%d, value=%d", msg->id, msg->value);
}

void app_main(void) {
    // Inicjalizacja Wi-Fi przy użyciu dedykowanego menedżera
    wifi_init_sta();
    
    // Inicjalizacja i konfiguracja loggera HTTPS
    init_tcp_logger();
    
    #if USEZLOTA
    while (tcp_logger_connect(ZLOTA_LOG_INGEST_URL, LOG_INGEST_TOKEN) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    #else
    while (tcp_logger_connect(DEBUG_LOG_INGEST_URL, DEBUG_LOG_INGEST_TOKEN) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    #endif
    
    printf("WiFi initialized.\n");
    
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);


    // przyklad uzycia ESPNOW, wysyla wiadomosc do urzadzenia o id 1 co 5 sekund

    Message msg;
    msg.id = 1;
    msg.value = 42;

    uint8_t peer_addr[6] = {0xA0, 0x11, 0x84, 0xAA, 0x2C, 0x01}; // adress jedynkii

     esp_now_init();
    // Rejestracja callback do odbierania wiadomości
    esp_now_register_recv_cb(recv_cb);
    esp_now_peer_info_t peerInfo;

    

    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, peer_addr, 6);

    peerInfo.channel = 0;   // tutaj jest podobno jakieś sranie z tymi kanałami, zobaczymy czy będzie działać 
    peerInfo.encrypt = false;

    esp_now_add_peer(&peerInfo);  //takie coś trza będezie zrobić dla każdego peera (9 razem)  wymyśleć fajny sposób aby każdy z tego samego kodu robił dla sobie peerów




    while(1) {
        /* Blink off (output low) */
        ESP_LOGI(my_id, "Turning off the LED");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        ESP_LOGI(my_id, "Turning on the LED");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}