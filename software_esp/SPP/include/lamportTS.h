#include <stdint.h>
#include <freertos/FreeRTOS.h>


#ifndef LAMPORTTS_H
#define LAMPORTTS_H

extern uint64_t lamport_clock;

void lamport_increment();
void lamport_receive(uint64_t received_ts);
uint64_t lamport_get_time();

#endif