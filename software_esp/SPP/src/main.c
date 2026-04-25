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

#include "mac_manager.h"
#include "secrets.h"
#include "config.h"
#include "log_redirect.h"
#include "wifi_manager.h"


#define BLINK_GPIO 48



void app_main(void) {
    // Inicjalizacja Wi-Fi przy użyciu dedykowanego menedżera
    wifi_init_sta();
    
    // Inicjalizacja i konfiguracja loggera HTTPS
    init_tcp_logger();
    while (tcp_logger_connect(DEBUG_LOG_INGEST_URL, DEBUG_LOG_INGEST_TOKEN) != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    printf("WiFi initialized.\n");
    
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