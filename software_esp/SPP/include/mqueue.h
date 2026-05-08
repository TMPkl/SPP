#include <stdint.h>
#ifndef MQUEUE_H
#define MQUEUE_H

#include "process.h"

void mqueue_insert(process_local_t *proc, queue_entry_t *entry);
void mqueue_remove_participants(process_local_t *proc, uint8_t *participants, uint8_t count);

#endif // MQUEUE_H