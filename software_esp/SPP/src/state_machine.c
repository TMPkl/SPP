#include <stdlib.h>  
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"  
#include "freertos/task.h"
#include "esp_log.h"
#include "process.h"
#include "esp_now.h"
#include "esp_now_receiver.h"
#include "mqueue.h"
#include "state_machine.h"
#include "mac_manager.h"
#include "disposition.h"
#include "lamportTS.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void set_state(process_local_t *proc, process_state_t new_state) {
    const char *state_names[] = {
        "KACUJE",
        "WYSYLAM_REQ",
        "JESTEM_W_KOLEJCE",
        "UMAWIAM_IMPREZE",
        "IMPREZA"
    };
    ESP_LOGI(my_id, "[LT:%llu] Zmieniam stan: %s -> %s", 
             proc->lamport_ts, state_names[proc->state], 
             state_names[new_state]);
    proc->state = new_state;
}

void handle_kacuje(process_local_t *proc) {

    if(!proc->ready_to_kac){
        return; // Czekaj aż wszyscy będą gotowi do kacowanie to jest ta bariera
    }

    const char *contrib[] = {"SEPIA", "ALKOHOL", "ZAGRYCHA"};
    
    
    // Inkrementuj liczbę imprez w których braliśmy udział
    proc->my_deficits.num_parties++;

    // Reset tylko zmiennych sterujących cyklem — kolejki NIE resetujemy tutaj.
    // REQ-i od innych urządzeń mogły już trafić do kolejki podczas czekania na barierę;
    // ich usunięcie zepsułoby porządek Lamporta. Kolejka czyszczona jest przez
    // mqueue_remove_participants po zakończeniu imprezy.
    proc->ack_count = 0;
    proc->request_sent = false;
    memset(proc->participants, 0, sizeof(proc->participants));
    proc->is_organizer = false;
    proc->hello_count = 0;
    proc->hello_sent = false;

    uint32_t delay = rand()  % MAX_KACOWANIE_MS;
    ESP_LOGI(my_id, "[LT:%llu] KACUJE:  czekam %lu ms", 
             proc->lamport_ts, delay);
    vTaskDelay(pdMS_TO_TICKS(delay));

        set_state(proc, WYSYLAM_REQ);
    }

void handle_wysylam_req(process_local_t *proc) {
    // Wysyślij REQ tylko raz
    if (proc->request_sent) {
        return;  // Już wysłano, czekamy na ACK
    }
    
    espnow_msg_t msg = {
        .header = {
            .type = MSG_REQ,
            .from = proc->my_id,
            .ts   = ++proc->lamport_ts,
        }
    };

    ESP_LOGI(my_id, "[LT:%llu] WYSYLAM_REQ: Wysyłam prośbę do wszystkich", proc->lamport_ts);
    for (int i = 0; i < 256; i++) {
        esp_now_peer_info_t *peer = get_peer_by_id(i);
        if (peer == NULL) continue;
        esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg_header_t));
    }
    
    proc->request_sent = true;  // Zaznacz że wysłano
}

void handle_jestem_w_kolejce(process_local_t *proc) {
    int my_pos = -1;
    for (int i = 0; i < proc->queue_size; i++) {
        if (proc->queue[i].mac_address == proc->my_id) {
            my_pos = i;
            break;
        }
    }

    if (my_pos == -1) return;

    ESP_LOGI(my_id, "[LT:%llu] JESTEM_W_KOLEJCE: Pozycja %d/%d", 
             proc->lamport_ts, my_pos + 1, proc->queue_size);

    if ((my_pos + 1) % CIRCLE_SIZE == 0) {
        proc->is_organizer = true;
        ESP_LOGI(my_id, "[LT:%llu] JESTEM ORGANIZATOREM! Wysyłam WELCOME", proc->lamport_ts);

        for (int i = 0; i < CIRCLE_SIZE; i++) {
            proc->participants[i] = proc->queue[my_pos - (CIRCLE_SIZE - 1) + i].mac_address;
        }

        espnow_msg_t msg = {
            .header = {
                .type = MSG_WELCOME,
                .from = proc->my_id,
                .ts   = ++proc->lamport_ts,
            },
            .payload.welcome = {
                .my_deficit = proc->my_deficits,
            }
        };
        memcpy(msg.payload.welcome.participants, proc->participants, sizeof(proc->participants));

        for (int i = 0; i < CIRCLE_SIZE - 1; i++) {
            esp_now_peer_info_t *peer = get_peer_by_id(proc->participants[i]);
            if (peer == NULL) continue;
            esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg));
        }

        set_state(proc, UMAWIAM_IMPREZE);
    }
}

void handle_umawiam_impreze(process_local_t *proc) {
    if (proc->hello_sent) return;

    espnow_msg_t msg = {
        .header = {
            .type = MSG_HELLO,
            .from = proc->my_id,
            .ts   = ++proc->lamport_ts,
        },
        .payload.hello = {
            .my_deficit = proc->my_deficits,
        }
    };

    ESP_LOGI(my_id, "[LT:%llu] UMAWIAM_IMPREZE: Wysyłam HELLO", proc->lamport_ts);
    for (int i = 0; i < CIRCLE_SIZE; i++) {
        esp_now_peer_info_t *peer = get_peer_by_id(proc->participants[i]);
        if (peer == NULL) continue;  // pomija siebie (brak w peers_table)
        esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg));
    }
    proc->hello_sent = true;
}

void handle_impreza(process_local_t *proc) {
    ESP_LOGI(my_id, "[LT:%llu] IMPREZA trwa %d ms...", proc->lamport_ts, PARTY_DURATION_MS);
    vTaskDelay(pdMS_TO_TICKS(PARTY_DURATION_MS));

    if (proc->is_organizer) {
        espnow_msg_t msg = {
            .header = {
                .type = MSG_REL,
                .from = proc->my_id,
                .ts   = ++proc->lamport_ts,
            }
        };
        memcpy(msg.payload.rel.participants, proc->participants, sizeof(proc->participants));

        ESP_LOGI(my_id, "[LT:%llu] IMPREZA skończona! Wysyłam REL jako organizator", proc->lamport_ts);
        for (int i = 0; i < 256; i++) {
            esp_now_peer_info_t *peer = get_peer_by_id(i);
            if (peer == NULL) continue;
            esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg));
        }

        set_state(proc, KACUJE);
    }  else {
        ESP_LOGI(my_id, "[LT:%llu] IMPREZA skończona! Czekam na REL od organizatora", proc->lamport_ts);
        xSemaphoreTake(proc->rel_semaphore, portMAX_DELAY);
        ESP_LOGI(my_id, "[LT:%llu] Otrzymałem REL, wracam do KACUJE", proc->lamport_ts);
    }
}

void tick(process_local_t *proc) {
    switch (proc->state) {
        case KACUJE:            handle_kacuje(proc);          break;
        case WYSYLAM_REQ:       handle_wysylam_req(proc);     break;
        case JESTEM_W_KOLEJCE:  handle_jestem_w_kolejce(proc);break;
        case UMAWIAM_IMPREZE:   handle_umawiam_impreze(proc); break;
        case IMPREZA:           handle_impreza(proc);         break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // Mały delay, aby nie zająć całego CPU
}

void on_message(process_local_t *proc, espnow_msg_t *msg) {
    proc->lamport_ts = MAX(proc->lamport_ts, msg->header.ts) + 1;

    // ========== OBSŁUGA MSG_READY - BARIERA ==========
    if (msg->header.type == MSG_READY) {
        proc->ready_count++;
        ESP_LOGI(my_id, "[LT:%llu] Otrzymano MSG_READY od %d, ready_count=%d/%d", 
                 proc->lamport_ts, msg->header.from, proc->ready_count, MAX_PEERS - 1);
        
        // Bariera: czekamy na N-1 (wszyscy oprócz siebie)
        if (proc->ready_count >= MAX_PEERS - 1 && !proc->ready_to_kac) {
            proc->ready_to_kac = true;
            ESP_LOGI(my_id, "[LT:%llu] ★★★ BARIERA OSIĄGNIĘTA! ★★★ Wszystkie %d urządzenia gotowe, BEGIN!", 
                    proc->lamport_ts, MAX_PEERS);
        }
        return;
    }

    // ========== EARLY BARRIER RELEASE ==========
    // Jeśli otrzymujemy JAKĄKOLWIEK inną wiadomość, ktoś przeszedł przez barierę
    if (!proc->ready_to_kac) {
        ESP_LOGI(my_id, "[LT:%llu] Otrzymano %d, zdejmuję barierę (ktoś już zaczął)", 
                 proc->lamport_ts, msg->header.type);
        proc->ready_to_kac = true;
    }

    // ========== OBSŁUGA MSG_REQ ==========
    if (msg->header.type == MSG_REQ) {
        ESP_LOGI(my_id, "[LT:%llu] Otrzymano MSG_REQ od %d, queue_size=%d", 
                 proc->lamport_ts, msg->header.from, proc->queue_size + 1);
        
        queue_entry_t entry = {
            .mac_address = msg->header.from,
            .lamport_ts  = msg->header.ts,
        };
        mqueue_insert(proc, &entry);

        espnow_msg_t ack = {
            .header = {
                .type = MSG_ACK,
                .from = proc->my_id,
                .ts   = ++proc->lamport_ts,
            }
        };
        esp_now_peer_info_t *peer = get_peer_by_id(msg->header.from);
        if (peer != NULL) {
            esp_now_send(peer->peer_addr, (uint8_t *)&ack, sizeof(msg_header_t));
        }
        return;
    }

    // ========== OBSŁUGA SPECYFICZNA DLA STANU ==========
    switch (proc->state) {
        case WYSYLAM_REQ:
            if (msg->header.type == MSG_ACK) {
                proc->ack_count++;
                ESP_LOGI(my_id, "[LT:%llu] Otrzymano MSG_ACK od %d (%u/%d)", 
                         proc->lamport_ts, msg->header.from, proc->ack_count, MAX_PEERS - 1);
                
                if (proc->ack_count == MAX_PEERS - 1) {
                    queue_entry_t entry = {
                        .mac_address = proc->my_id,
                        .lamport_ts  = proc->lamport_ts,
                    };
                    mqueue_insert(proc, &entry);
                    set_state(proc, JESTEM_W_KOLEJCE);
                }
            }
            break;

        case JESTEM_W_KOLEJCE:
            if (msg->header.type == MSG_WELCOME) {
                ESP_LOGI(my_id, "[LT:%llu] Otrzymano MSG_WELCOME od %d (organizator)", 
                         proc->lamport_ts, msg->header.from);
                
                memcpy(proc->participants, msg->payload.welcome.participants, sizeof(proc->participants));
                for (int i = 0; i < CIRCLE_SIZE; i++) {
                    if (proc->participants[i] == msg->header.from) {
                        proc->received_deficits[i] = msg->payload.welcome.my_deficit;
                        break;
                    }
                }
                set_state(proc, UMAWIAM_IMPREZE);
            }
            break;

        case UMAWIAM_IMPREZE:
            if (msg->header.type == MSG_HELLO) {
                proc->hello_count++;
                ESP_LOGI(my_id, "[LT:%llu] Otrzymano MSG_HELLO od %d (%u/%u)", 
                         proc->lamport_ts, msg->header.from, proc->hello_count, CIRCLE_SIZE - 1);
                
                for (int i = 0; i < CIRCLE_SIZE; i++) {
                    if (proc->participants[i] == msg->header.from) {
                        proc->received_deficits[i] = msg->payload.hello.my_deficit;
                        break;
                    }
                }
                
                if (proc->hello_count == CIRCLE_SIZE - 1) {
                    for (int i = 0; i < CIRCLE_SIZE; i++) {
                        if (proc->participants[i] == proc->my_id) {
                            proc->received_deficits[i] = proc->my_deficits;
                            break;
                        }
                    }
                    ESP_LOGI(my_id, "[LT:%llu] Wszyscy wysłali HELLO! Obliczam podział zasobów...", proc->lamport_ts);
                    disposition(proc);
                    ESP_LOGI(my_id, "[LT:%llu] Przechodzę do IMPREZA", proc->lamport_ts);
                    set_state(proc, IMPREZA);
                }
            }
            break;

        case IMPREZA:
            if (msg->header.type == MSG_REL) {
                ESP_LOGI(my_id, "[LT:%llu] Otrzymano MSG_REL od organizatora %d, impreza się kończy", 
                         proc->lamport_ts, msg->header.from);
                
                mqueue_remove_participants(proc, msg->payload.rel.participants, CIRCLE_SIZE);
                set_state(proc, KACUJE);
                xSemaphoreGive(proc->rel_semaphore); 
            }
            break;

        default:
            break;
    }
}

