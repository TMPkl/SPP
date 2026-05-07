#ifndef QUEUE_H
#define QUEUE_H

#include "process.h"

void queue_insert(process_local_t *proc, queue_entry_t *entry);
void queue_remove_participants(process_local_t *proc, uint8_t *participants, uint8_t count);

#endif // QUEUE_H