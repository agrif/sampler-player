#include <stddef.h>
#include "player.h"
#include "sys/alt_irq.h"
#include "priv/alt_file.h"

static void player_handle_irq(void* context
#ifndef ALT_ENHANCED_INTERRUPT_API_PRESENT
                              , alt_u32 id
#endif
    ) {
    player_state* s = context;
    s->csr[0] &= ~PLAYER_CSR_IRQ_MSK;
    s->interrupts++;
}

void player_initialize(player_state* s) {
    if (s->irq != ALT_IRQ_NOT_CONNECTED) {
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
        alt_ic_isr_register(s->irq_controller_id, s->irq, player_handle_irq, s, NULL);
#else
        alt_irq_register(s->irq, s, player_handle_irq);
#endif
    }

    // register with the HAL
    alt_dev_reg(&(s->dev));
}

player_state* player_open(const char* name) {
    player_state* s = (player_state*)alt_find_dev(name, &alt_dev_list);
    return s;
}
