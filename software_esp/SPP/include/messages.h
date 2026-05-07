#include <stdint.h>
#include <string.h>

#ifndef MESSAGES_H
#define MESSAGES_H


#define CIRCLE_SIZE   3


// typy wiadomości 
typedef enum __attribute__((packed)) {
    MSG_REQ     = 0x01,
    MSG_ACK     = 0x02,
    MSG_REL     = 0x03,
    MSG_WELCOME = 0x04,
    MSG_HELLO   = 0x05,
} msg_type_t;

//te podstawowe dane do wszystkich wiadomości
typedef struct __attribute__((packed)) {
    msg_type_t type;
    uint8_t    from;
    uint64_t   ts;
} msg_header_t;


// REL(from, ts, participants) 
typedef struct __attribute__((packed)) {
    uint8_t participants[CIRCLE_SIZE];
} payload_rel_t;

// WELCOME(from, ts, deficits, participants)
typedef struct __attribute__((packed)) {
    int16_t deficits[3];
    uint8_t participants[CIRCLE_SIZE];
} payload_welcome_t;


typedef struct __attribute__((packed)) {
    uint8_t sep;
    uint8_t alko;      /// ile razy co przyniosłem
    uint8_t zagrycha; 
} deficit_t;

// HELLO(from, ts, deficits)
typedef struct __attribute__((packed)) {
    int16_t deficits[3];
    deficit_t my_deficit;
} payload_hello_t;


// razem wszyskot 
typedef struct __attribute__((packed)) {
    msg_header_t header;
    union {
        payload_rel_t     rel;
        payload_welcome_t welcome;
        payload_hello_t   hello;
        // REQ i ACK - sam nagłówek, brak payloadu
    } payload;
} espnow_msg_t;


#endif // MESSAGES_H