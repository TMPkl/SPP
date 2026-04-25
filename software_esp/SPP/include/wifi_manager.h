#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

/**
 * @brief Inicjalizuje Wi-Fi w trybie Station (STA), łączy z punktem dostępowym (AP)
 * i konfiguruje obsługę zdarzeń.
 *
 * Ta funkcja wykonuje następujące kroki:
 * 1. Inicjalizuje NVS (Non-Volatile Storage).
 * 2. Inicjalizuje stos TCP/IP i pętlę zdarzeń.
 * 3. Tworzy domyślny interfejs Wi-Fi STA.
 * 4. Konfiguruje i inicjalizuje Wi-Fi.
 * 5. Rejestruje handlery zdarzeń dla Wi-Fi i IP.
 * 6. Ustawia konfigurację (SSID, hasło) i uruchamia Wi-Fi.
 */
void wifi_init_sta(void);

#endif // WIFI_MANAGER_H
