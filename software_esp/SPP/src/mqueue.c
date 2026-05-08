#include <stdint.h>
#include "process.h"
#include <string.h>
#include "mqueue.h"

void mqueue_insert(process_local_t *proc, queue_entry_t *entry) {
    // znajdź miejsce do wstawienia (sortuj po ts, potem mac)
    int insert_pos = proc->queue_size;
    for (int i = 0; i < proc->queue_size; i++) {
        if (entry->lamport_ts < proc->queue[i].lamport_ts ||
           (entry->lamport_ts == proc->queue[i].lamport_ts && 
            entry->mac_address < proc->queue[i].mac_address)) {
            insert_pos = i;
            break;
        }
    }

    // przesuń elementy w prawo
    for (int i = proc->queue_size; i > insert_pos; i--) {
        proc->queue[i] = proc->queue[i - 1];
    }

    // wstaw
    proc->queue[insert_pos] = *entry;
    proc->queue_size++;
}

void mqueue_remove_participants(process_local_t *proc, uint8_t *participants, uint8_t count) {
    for (int p = 0; p < count; p++) {
        for (int i = 0; i < proc->queue_size; i++) {
            if (proc->queue[i].mac_address == participants[p]) {
                // przesuń elementy w lewo
                for (int j = i; j < proc->queue_size - 1; j++) {
                    proc->queue[j] = proc->queue[j + 1];
                }
                proc->queue_size--;
                break;
            }
        }
    }
}