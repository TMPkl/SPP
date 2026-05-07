#include <stdint.h>
#include <string.h>
#include "deficits.h"
#include "messages.h"

#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES       10    
#define QUEUE_SIZE          MAX_PROCESSES

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
    
    uint8_t participants[CIRCLE_SIZE];
    bool is_organizer;
    deficit_t my_deficits;  
    contribution_t what_i_bring;   

    process_state_t state;  
    
} process_local_t;

#endif // PROCESS_H