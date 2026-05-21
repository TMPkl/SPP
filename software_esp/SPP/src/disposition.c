#include <stdlib.h>  
#include <string.h>
#include <stdbool.h>
#include "process.h"
#include "state_machine.h"
#include "deficits.h"
#include "esp_log.h"
#include "mac_manager.h"

void disposition(process_local_t *proc) {
    if (proc == NULL) return;
    
    bool assigned[CIRCLE_SIZE] = {false};

    contribution_t result[CIRCLE_SIZE] = {0};
    
    const char *contrib[] = {"SEPIA", "ALKOHOL", "ZAGRYCHA"};
    const char *resource_names[] = {"SEP", "ALKOHOL", "ZAGRYCHA"};
    
    ESP_LOGI(my_id, "[DISPOSITION] Rozpoczynam podział zasobów dla %d uczestników", CIRCLE_SIZE);
    
    for (int slot = 0; slot < CIRCLE_SIZE; slot++) {
        int res_type = slot % 3;
        
        int selected_idx = -1;
        uint32_t min_deficit = UINT32_MAX;  
        uint8_t selected_id = UINT8_MAX;    
        
        for (int i = 0; i < CIRCLE_SIZE; i++) {
            if (assigned[i]) continue; 
            
            uint8_t zasoby = 0;
            uint8_t num_parties = proc->received_deficits[i].num_parties;
            uint32_t deficit = 0;
            
            if (res_type == 0) zasoby = proc->received_deficits[i].sep;
            else if (res_type == 1) zasoby = proc->received_deficits[i].alko;
            else if (res_type == 2) zasoby = proc->received_deficits[i].zagrycha;

            if (num_parties > 0) {
                deficit = (zasoby * 1000) / num_parties;
            } else {
                deficit = 1000; 
            }
            
            uint8_t participant_id = proc->participants[i];
            
            if (deficit < min_deficit || 
                (deficit == min_deficit && participant_id < selected_id)) {
                min_deficit = deficit;
                selected_idx = i;
                selected_id = participant_id;
            }
        }
        
        if (selected_idx != -1) {
            assigned[selected_idx] = true;
            result[selected_idx] = (contribution_t)res_type;
        }
    }
    
    for (int i = 0; i < CIRCLE_SIZE; i++) {
        if (proc->participants[i] == proc->my_id) {
            proc->what_i_bring = result[i];
            ESP_LOGI(my_id, "[DISPOSITION] Ja przynoszę: %s", contrib[result[i]]);
            break;
        }
    }
    
    ESP_LOGI(my_id, "[DISPOSITION] Podział zasobów zakończony!");
}