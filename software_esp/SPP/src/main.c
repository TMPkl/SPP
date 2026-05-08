#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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
#include "lamportTS.h"
#include "state_machine.h"
#include "process.h"    

#define BLINK_GPIO 48

static process_local_t proc;
QueueHandle_t msg_queue;

void message_task(void *arg) {
    esp_now_message_t raw;
    while (1) {
        if (xQueueReceive(msg_queue, &raw, portMAX_DELAY)) {
            espnow_msg_t *msg = (espnow_msg_t *)raw.content;
            on_message(&proc, msg);
        }
    }
}

void main_task(void *arg) {
    while (1) {
        tick(&proc);
    }
}

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

    esp_now_receiver_init();
    esp_now_add_all_peers();

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // inicjalizacja procesu
    memset(&proc, 0, sizeof(proc));
    proc.my_id = (uint8_t)strtol((const char*)&my_id[6], NULL, 16);
    proc.state = KACUJE;

    // kolejka wiadomości
    msg_queue = xQueueCreate(10, sizeof(esp_now_message_t));

    xTaskCreate(message_task, "msg_task",  4096, NULL, 5, NULL);
    xTaskCreate(main_task,    "main_task", 4096, NULL, 4, NULL);
}



//typedef struct healthcheck_message_t {
//      uint8_t sender_id[2];
//      uint8_t content[250];
//      uint8_t content_len;
//     } healthcheck_message_t;

// void app_main(void) {
//     wifi_init_sta();
    
//     init_tcp_logger();
    
//     #if USEZLOTA
//     while (tcp_logger_connect(ZLOTA_LOG_INGEST_URL, LOG_INGEST_TOKEN) != ESP_OK) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
//     #else
//     while (tcp_logger_connect(DEBUG_LOG_INGEST_URL, DEBUG_LOG_INGEST_TOKEN) != ESP_OK) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
//     #endif
    
//     printf("WiFi initialized.\n");
    
//     esp_now_receiver_init();  // to jest aby wg zainicjalizaowac now 
//     esp_now_add_all_peers(); // to jest aby dodać wszystkie urządzenia z peerów 
    
//     gpio_reset_pin(BLINK_GPIO);
//     gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    
// //// koniec incjalizowania wszystkiego

//     // Lamport Clock Test
//     uint8_t my_device_id = (uint8_t)strtol((const char*)&my_id[6], NULL, 16);
    
//     // Routing: 1 -> 2 -> 10 -> 1
//     uint8_t next_device_id = (my_device_id == 1) ? 2 : (my_device_id == 2) ? 10 : 1;
    
//     ESP_LOGI(my_id, "Lamport Clock Test started. Device ID: %d, Sending to: %d", my_device_id, next_device_id);

//     int send_counter = 0;
    
//     while(1) {
//         // Send test message every 5 seconds
//         healthcheck_message_t health_msg;
//         snprintf((char *)health_msg.content, sizeof(health_msg.content), 
//                  "From device %d - Message #%d", my_device_id, send_counter);
        
//         health_msg.sender_id[0] = (int)my_id[7];
//         health_msg.sender_id[1] = (int)my_id[8];
//         health_msg.content_len = strlen((char *)health_msg.content);

//         esp_now_peer_info_t* peer = get_peer_by_id(next_device_id);

//         if (peer != NULL) {
//             lamport_increment();
//             uint64_t ts = lamport_get_time();
//             esp_err_t result = esp_now_send(peer->peer_addr, (uint8_t *)&health_msg, sizeof(healthcheck_message_t));
//             if (result == ESP_OK) {
//                 ESP_LOGI(my_id, "Message #%d sent to device %d", send_counter, next_device_id);
//             } else {
//                 ESP_LOGE(my_id, "Failed to send to device %d: %s", next_device_id, esp_err_to_name(result));
//             }
//         }
        
//         send_counter++;
        
//         // Blink LED every 5 seconds
//         gpio_set_level(BLINK_GPIO, 0);
//         vTaskDelay(5000 / portTICK_PERIOD_MS);
//         gpio_set_level(BLINK_GPIO, 1);
//         vTaskDelay(500 / portTICK_PERIOD_MS);
//     }
// }