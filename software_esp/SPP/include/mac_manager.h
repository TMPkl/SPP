#ifndef MAC_MANAGER_H
#define MAC_MANAGER_H

#include <stdint.h>

extern int flaga;

/**
 * @brief Jednorazowa funkcja np. na produkcję do zapisywania MAC
 * @param mac Wskaźnik do tablicy 6 bajtów z adresem MAC
 */
void save_mac_to_nvs(const uint8_t *mac);

/**
 * @brief Funkcja wczytująca MAC z pamięci NVS i ustawiająca go dla interfejsu WiFi
 */
void load_and_set_mac_from_nvs(void);

#endif // MAC_MANAGER_H