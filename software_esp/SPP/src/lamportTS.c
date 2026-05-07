#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lamportTS.h"

uint64_t lamport_clock = 0; 
portMUX_TYPE lamport_mux = portMUX_INITIALIZER_UNLOCKED; // ciekawe czy to zadziała jako globalny mutex dla lamporta

void lamport_increment() {
    taskENTER_CRITICAL(&lamport_mux);
    lamport_clock++;
    taskEXIT_CRITICAL(&lamport_mux);
}

void lamport_receive(uint64_t received_ts) {
    taskENTER_CRITICAL(&lamport_mux);
    if (received_ts > lamport_clock) {
        lamport_clock = received_ts;
    } 
    lamport_clock++;
    taskEXIT_CRITICAL(&lamport_mux);
}

uint64_t lamport_get_time() {
    taskENTER_CRITICAL(&lamport_mux);
    uint64_t ts = lamport_clock;
    taskEXIT_CRITICAL(&lamport_mux);
    return ts;
}