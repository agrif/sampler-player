#ifndef __SAMPLER_H__
#define __SAMPLER_H__

#include "sampler_regs.h"
#include "alt_types.h"
#include "sys/alt_dev.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct sampler_state_s {
    alt_dev dev;
    
    volatile alt_u32* buffer;
    alt_u32 buffer_size;
    volatile alt_u32* csr;
    alt_u32 csr_size;
    alt_u8 irq;
    alt_u8 irq_controller_id;
    volatile unsigned int interrupts;
    alt_u8 width;
    alt_u8 sample_bits;
    alt_u8 time_bits;
} sampler_state;

#define SAMPLER_INSTANCE(name, state)           \
    sampler_state state = {                     \
        { ALT_LLIST_ENTRY, "/dev/" #state },    \
        (alt_u32*) name##_READ_BASE,            \
        name##_READ_SPAN / sizeof(alt_u32),     \
        (alt_u32*) name##_CSR_BASE,             \
        name##_CSR_SPAN / sizeof(alt_u32),      \
        name##_CSR_IRQ,                         \
        name##_CSR_IRQ_INTERRUPT_CONTROLLER_ID, \
        0,                                      \
        name##_READ_WIDTH,                      \
        name##_READ_SAMPLE_BITS,                \
        name##_READ_TIME_BITS,                  \
    }
#define SAMPLER_INIT(name, state)               \
    do  {                                       \
        sampler_initialize(&state);             \
        alt_dev_reg((alt_dev*)(&state));        \
    } while (0)

extern void sampler_initialize(sampler_state* s);

extern sampler_state* sampler_open(const char* name);

static inline int sampler_is_done(sampler_state* s) {
    return s->csr[0] & SAMPLER_CSR_DONE_MSK;
}

static inline void sampler_wait_done(sampler_state* s) {
    while (!sampler_is_done(s));
}

static inline int sampler_is_enabled(sampler_state* s) {
    return s->csr[0] & SAMPLER_CSR_ENABLED_MSK;
}

static inline void sampler_set_enabled(sampler_state* s, int enabled) {
    if (enabled) {
        s->csr[0] |= SAMPLER_CSR_ENABLED_MSK;
    } else {
        s->csr[0] &= ~SAMPLER_CSR_ENABLED_MSK;
    }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SAMPLER_H__ */
