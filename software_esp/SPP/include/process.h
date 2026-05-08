#include <stdint.h>
#include <string.h>
#include "deficits.h"
#include "messages.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES       10    
#define QUEUE_SIZE          MAX_PROCESSES
#define PARTY_DURATION_MS 5000
#define MAX_KACOWANIE_MS 10000

typedef enum {
    KACUJE,   
    WYSYLAM_REQ, 
    JESTEM_W_KOLEJCE,   
    UMAWIAM_IMPREZE,          
    IMPREZA
} process_state_t;


typedef struct __attribute__((packed)) {
    char mac_address;   
    uint64_t lamport_ts;        
} queue_entry_t;

typedef enum { SEPIA, ALKOHOL, ZAGRYCHA } contribution_t;

typedef struct {
    uint64_t lamport_ts;  
    char my_id;

    
    queue_entry_t queue[QUEUE_SIZE]; 
    uint8_t queue_size;  
     
    uint8_t ack_count;  
    
    uint8_t hello_count;
    
    uint8_t participants[CIRCLE_SIZE];
    bool is_organizer;
    deficit_t my_deficits;  
    contribution_t what_i_bring;   
    deficit_t received_deficits[CIRCLE_SIZE];

    SemaphoreHandle_t rel_semaphore;
    process_state_t state;  
    
} process_local_t;

#endif // PROCESS_H