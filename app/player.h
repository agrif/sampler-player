#ifndef __PLAYER_H_INCLUDED__
#define __PLAYER_H_INCLUDED__

#include "alt_types.h"
#include "sys/alt_irq.h"

typedef struct {
    alt_u32* write;
    alt_u32 write_span;
    volatile alt_u32* csr;
    alt_u32 csr_span;
    alt_u8 irq;
    alt_u8 irq_controller_id;
    volatile unsigned int finished_writes;
} Player;

#define INIT_PLAYER(p, BASE) do {                                       \
        (p)->write = (alt_u32*) BASE ## _WRITE_BASE;                    \
        (p)->write_span = BASE ## _WRITE_SPAN / sizeof((p)->write[0]);  \
        (p)->csr = (alt_u8*) BASE ## _CSR_BASE;                         \
        (p)->csr_span = BASE ## _CSR_SPAN / sizeof((p)->csr[0]);        \
        (p)->irq = BASE ## _CSR_IRQ;                                    \
        (p)->irq_controller_id = BASE ## _CSR_IRQ_INTERRUPT_CONTROLLER_ID; \
        (p)->finished_writes = 0;                                       \
        player_initialize(p);                                           \
    } while(0)

enum PlayerCSR {
    PLAYER_ENABLED = 0x01,
    PLAYER_DONE = 0x02,
    PLAYER_IRQ = 0x04,
};

static void player_handle_irq(void* context);
static inline void player_initialize(Player* s) {
    if (s->irq >= 0) {
        alt_ic_isr_register(s->irq_controller_id, s->irq, player_handle_irq, s, NULL);
    }
}

static inline int player_is_done(Player* s) {
    return s->csr[0] & PLAYER_DONE;
}

static inline void player_wait_done(Player* s) {
    while (!player_is_done(s));
}

static inline int player_is_enabled(Player* s) {
    return s->csr[0] & PLAYER_ENABLED;
}

static inline void player_set_enabled(Player* s, int enabled) {
    if (enabled) {
        s->csr[0] |= PLAYER_ENABLED;
    } else {
        s->csr[0] &= ~PLAYER_ENABLED;
    }
}

static inline void player_clear_irq(Player* s) {
    s->csr[0] &= ~PLAYER_IRQ;
}

static void player_handle_irq(void* context) {
    Player* s = (Player*)context;
    player_clear_irq(s);
    s->finished_writes++;
}

#endif /* __PLAYER_H_INCLUDED__ */
