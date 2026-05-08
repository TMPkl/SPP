#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "process.h"
#include "esp_now.h"
#include "esp_now_receiver.h"
#include "mqueue.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// State machine handlers
void handle_kacuje(process_local_t *proc);
void handle_wysylam_req(process_local_t *proc);
void handle_jestem_w_kolejce(process_local_t *proc);
void handle_umawiam_impreze(process_local_t *proc);
void handle_impreza(process_local_t *proc);

// Main state machine functions
void tick(process_local_t *proc);
void on_message(process_local_t *proc, espnow_msg_t *msg);

#endif // STATE_MACHINE_H
