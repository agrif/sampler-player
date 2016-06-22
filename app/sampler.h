#ifndef __SAMPLER_H_INCLUDED__
#define __SAMPLER_H_INCLUDED__

#include "alt_types.h"
#include "sys/alt_irq.h"

typedef struct {
    volatile alt_u32* read;
    alt_u32 read_span;
    volatile alt_u32* csr;
    alt_u32 csr_span;
    alt_u8 irq;
    alt_u8 irq_controller_id;
    volatile unsigned int finished_reads;
} Sampler;

#define INIT_SAMPLER(p, BASE) do {                                      \
        (p)->read = (alt_u32*) BASE ## _READ_BASE;                      \
        (p)->read_span = BASE ## _READ_SPAN / sizeof((p)->read[0]);     \
        (p)->csr = (alt_u8*) BASE ## _CSR_BASE;                         \
        (p)->csr_span = BASE ## _CSR_SPAN / sizeof((p)->csr[0]);        \
        (p)->irq = BASE ## _CSR_IRQ;                                    \
        (p)->irq_controller_id = BASE ## _CSR_IRQ_INTERRUPT_CONTROLLER_ID; \
        (p)->finished_reads = 0;                                            \
        sampler_initialize(p);                                          \
    } while(0)

enum SamplerCSR {
    SAMPLER_ENABLED = 0x01,
    SAMPLER_DONE = 0x02,
};

static void sampler_handle_irq(void* context);
static inline void sampler_initialize(Sampler* s) {
    if (s->irq >= 0) {
        alt_ic_isr_register(s->irq_controller_id, s->irq, sampler_handle_irq, s, NULL);
    }
}

static inline int sampler_is_done(Sampler* s) {
    return s->csr[0] & SAMPLER_DONE;
}

static inline void sampler_wait_done(Sampler* s) {
    while (!sampler_is_done(s));
}

static inline int sampler_is_enabled(Sampler* s) {
    return s->csr[0] & SAMPLER_ENABLED;
}

static inline void sampler_set_enabled(Sampler* s, int enabled) {
    if (enabled) {
        s->csr[0] |= SAMPLER_ENABLED;
    } else {
        s->csr[0] &= ~SAMPLER_ENABLED;
    }
}

static void sampler_handle_irq(void* context) {
    Sampler* s = (Sampler*)context;
    // ought to be done already, but this will clear the interrupt
    while (!sampler_is_done(s));
    s->finished_reads++;
}

#endif /* __SAMPLER_H_INCLUDED__ */
