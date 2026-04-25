#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "mac_manager.h"
#include "secrets.h"
#include "config.h"
#include "wifi_manager.h"


#define BLINK_GPIO 48

int flaga = 0;

// Uwaga: Funkcja została napisana jako Task, żeby nie blokowała głównej pętli
void tcp_client_task(void *pvParameters) {
    // UWAGA: ZMIEŃ NA IP SWOJEGO KOMPUTERA/SERWERA W SIECI LOKALNEJ
    char host_ip[] = DEBUG_IP_ADDRESS; // IP Twojego serwera Go
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEBUG_SERIAL_REDIRECT_PORT); // Port Twojego serwera Go

    while (1) {
        // Czekaj 5 sekund pomiędzy pingami
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            printf("Nie udalo sie utworzyc socketu: errno %d\n", errno);
            continue;
        }

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
        if (err != 0) {
            printf("Socket nie polaczyl sie (serwer byc moze nie dziala): errno %d\n", errno);
            close(sock);
            continue;
        }
        printf("Polaczono z serwerem!\n");

        // 1. Serwer Go wymaga najpierw identyfikatora zakonczonego nowa linia \n
        const char *id_payload = "ESP_TEST_01\n";
        send(sock, id_payload, strlen(id_payload), 0);

        // 2. Po czym wysyłamy nasze właściwe wiadomości (pingi)
        const char *payload = "Ping - dzialam poprawnie!\n";
        send(sock, payload, strlen(payload), 0);

        // Zamedłknij jeśli chcemy rozłączyć od razu. Pętla będzie wznawiać.
        close(sock);
    }
    
    vTaskDelete(NULL);
}

void app_main(void) {
    // Inicjalizacja Wi-Fi przy użyciu dedykowanego menedżera
    wifi_init_sta();
    
    printf("WiFi initialized.\n");
    
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while(1) {
        /* Blink off (output low) */
        printf("Turning off the LED\n");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        printf("Turning on the LED\n");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}