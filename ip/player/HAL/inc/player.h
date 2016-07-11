#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "player_regs.h"
#include "alt_types.h"
#include "sys/alt_dev.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct player_state_s {
    alt_dev dev;
    
    volatile alt_u32* buffer;
    alt_u32 buffer_size;
    volatile alt_u32* csr;
    alt_u32 csr_size;
    alt_u8 irq;
    alt_u8 irq_controller_id;
    volatile unsigned int interrupts;
    alt_u8 width_bits;
    alt_u8 width_words;
    alt_u8 sample_bits;
    alt_u8 time_bits;
    alt_u32 time_length;
} player_state;

#define PLAYER_INSTANCE(name, state)            \
    player_state state = {                      \
        { ALT_LLIST_ENTRY, "/dev/" #state },    \
        (alt_u32*) name##_BUFFER_BASE,          \
        name##_BUFFER_SPAN / sizeof(alt_u32),   \
        (alt_u32*) name##_CSR_BASE,             \
        name##_CSR_SPAN / sizeof(alt_u32),      \
        name##_CSR_IRQ,                         \
        name##_CSR_IRQ_INTERRUPT_CONTROLLER_ID, \
        0,                                      \
        name##_BUFFER_WIDTH,                    \
        (name##_BUFFER_WIDTH + 31) / 32,        \
        name##_BUFFER_SAMPLE_BITS,              \
        name##_BUFFER_TIME_BITS,                \
        1 << name##_BUFFER_TIME_BITS,           \
    }
#define PLAYER_INIT(name, state) \
        player_initialize(&state)

extern void player_initialize(player_state* s);

extern player_state* player_open(const char* name);

static inline int player_is_done(player_state* s) {
    return s->csr[0] & PLAYER_CSR_DONE_MSK;
}

static inline void player_wait_done(player_state* s) {
    while (!player_is_done(s));
}

static inline int player_is_enabled(player_state* s) {
    return s->csr[0] & PLAYER_CSR_ENABLED_MSK;
}

static inline void player_set_enabled(player_state* s, int enabled) {
    if (enabled) {
        s->csr[0] |= PLAYER_CSR_ENABLED_MSK;
    } else {
        s->csr[0] &= ~PLAYER_CSR_ENABLED_MSK;
    }
}

static inline volatile alt_u32* player_get_time(player_state* s, alt_u32 time) {
    return &(s->buffer[time << (s->sample_bits - 2)]);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYER_H__ */
