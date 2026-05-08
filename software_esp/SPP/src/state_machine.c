#include <stdlib.h>  
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"  
#include "freertos/task.h"
#include "process.h"
#include "esp_now.h"
#include "esp_now_receiver.h"
#include "mqueue.h"
#include "state_machine.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void handle_kacuje(process_local_t *proc) {
    switch (proc->what_i_bring) {
        case SEPIA:    proc->my_deficits.sep++;      break;
        case ALKOHOL:  proc->my_deficits.alko++;     break;
        case ZAGRYCHA: proc->my_deficits.zagrycha++; break;
    }

    proc->ack_count = 0;
    proc->queue_size = 0;
    memset(proc->queue, 0, sizeof(proc->queue));
    memset(proc->participants, 0, sizeof(proc->participants));
    proc->is_organizer = false;
    proc->hello_count = 0;

    uint32_t delay = rand() % MAX_KACOWANIE_MS;
    vTaskDelay(pdMS_TO_TICKS(delay));

    proc->state = WYSYLAM_REQ;
}

void handle_wysylam_req(process_local_t *proc) {
    espnow_msg_t msg = {
        .header = {
            .type = MSG_REQ,
            .from = proc->my_id,
            .ts   = ++proc->lamport_ts,
        }
    };

    for (uint8_t i = 0; i < MAX_PEERS; i++) {
        esp_now_peer_info_t *peer = get_peer_by_id(i);
        if (peer == NULL) continue;
        esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg_header_t));
    }
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

    if ((my_pos + 1) % CIRCLE_SIZE == 0) {
        proc->is_organizer = true;

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

        proc->state = UMAWIAM_IMPREZE;
    }
}

void handle_umawiam_impreze(process_local_t *proc) {
    if (!proc->is_organizer) {
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

        for (int i = 0; i < CIRCLE_SIZE; i++) {
            esp_now_peer_info_t *peer = get_peer_by_id(proc->participants[i]);
            if (peer == NULL) continue;
            esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg));
        }
    }
}

void handle_impreza(process_local_t *proc) {
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

        for (uint8_t i = 0; i < MAX_PEERS; i++) {
            esp_now_peer_info_t *peer = get_peer_by_id(i);
            if (peer == NULL) continue;
            esp_now_send(peer->peer_addr, (uint8_t *)&msg, sizeof(msg));
        }

        proc->state = KACUJE;
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
}

void on_message(process_local_t *proc, espnow_msg_t *msg) {
    proc->lamport_ts = MAX(proc->lamport_ts, msg->header.ts) + 1;

    if (msg->header.type == MSG_REQ) {
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

    switch (proc->state) {
        case WYSYLAM_REQ:
            if (msg->header.type == MSG_ACK) {
                proc->ack_count++;
                if (proc->ack_count == MAX_PEERS - 1) {
                    queue_entry_t entry = {
                        .mac_address = proc->my_id,
                        .lamport_ts  = proc->lamport_ts,
                    };
                    //mqueue_insert(proc, &entry);
                    proc->state = JESTEM_W_KOLEJCE;
                }
            }
            break;

        case JESTEM_W_KOLEJCE:
            if (msg->header.type == MSG_WELCOME) {
                memcpy(proc->participants, msg->payload.welcome.participants, sizeof(proc->participants));
                for (int i = 0; i < CIRCLE_SIZE; i++) {
                    if (proc->participants[i] == msg->header.from) {
                        proc->received_deficits[i] = msg->payload.welcome.my_deficit;
                        break;
                    }
                }
                proc->state = UMAWIAM_IMPREZE;
            }
            break;

        case UMAWIAM_IMPREZE:
            if (msg->header.type == MSG_HELLO) {
                for (int i = 0; i < CIRCLE_SIZE; i++) {
                    if (proc->participants[i] == msg->header.from) {
                        proc->received_deficits[i] = msg->payload.hello.my_deficit;
                        break;
                    }
                }
                proc->hello_count++;
                if (proc->hello_count == CIRCLE_SIZE - 1) {
                    for (int i = 0; i < CIRCLE_SIZE; i++) {
                        if (proc->participants[i] == proc->my_id) {
                            proc->received_deficits[i] = proc->my_deficits;
                            break;
                        }
                    }
                    // assign_contributions(proc); to be implemented
                    proc->state = IMPREZA;
                }
            }
            break;

            case IMPREZA:
                if (msg->header.type == MSG_REL) {
                    //mqueue_remove_participants(proc, msg->payload.rel.participants, CIRCLE_SIZE);
                    proc->state = KACUJE;
                }
                break;

        default:
            break;
    }
}