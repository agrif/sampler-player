#include <stddef.h>
#include "sampler.h"
#include "sys/alt_irq.h"
#include "priv/alt_file.h"

static void sampler_handle_irq(void* context
#ifndef ALT_ENHANCED_INTERRUPT_API_PRESENT
                               , alt_u32 id
#endif
    ) {
    sampler_state* s = context;
    s->csr[0] &= ~SAMPLER_CSR_IRQ_MSK;
    s->interrupts++;
}

void sampler_initialize(sampler_state* s) {
    if (s->irq != ALT_IRQ_NOT_CONNECTED) {
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
        alt_ic_isr_register(s->irq_controller_id, s->irq, sampler_handle_irq, s, NULL);
#else
        alt_irq_register(s->irq, s, sampler_handle_irq);
#endif
        
    }
}

sampler_state* sampler_open(const char* name) {
    sampler_state* s = alt_find_dev(name, &alt_dev_list);
    return s;
}
