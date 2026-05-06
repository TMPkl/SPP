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

#include "mac_manager.h"
#include "secrets.h"
#include "config.h"
#include "log_redirect.h"
#include "wifi_manager.h"
#include "esp_now_receiver.h"


#define BLINK_GPIO 48



void app_main(void) {
    wifi_init_sta();
    
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
    
    esp_now_receiver_init();  // to jest aby wg zainicjalizaowac now 
    esp_now_add_all_peers(); // to jest aby dodać wszystkie urządzenia z peerów 
    
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

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