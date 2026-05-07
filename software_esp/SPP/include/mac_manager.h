#ifndef MAC_MANAGER_H
#define MAC_MANAGER_H

#include <stdint.h>

extern char my_id[7];

void save_mac_to_nvs(const uint8_t *mac);

void load_and_set_mac_from_nvs(void);

void get_current_mac(uint8_t *mac);

#endif // MAC_MANAGER_H