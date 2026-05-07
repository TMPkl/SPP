#include <stdint.h>
#include <string.h>

#ifndef DEFICITS_H
#define DEFICITS_H

typedef struct __attribute__((packed)) {
    uint8_t sep;
    uint8_t alko;     
    uint8_t zagrycha; 
} deficit_t;

#endif // DEFICITS_H