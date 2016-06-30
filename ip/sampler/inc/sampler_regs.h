#ifndef __SAMPLER_REGS_H__
#define __SAMPLER_REGS_H__

#include <io.h>

#define SAMPLER_CSR_REG 0
#define IOADDR_SAMPLER_CSR(base) \
    __IO_CALC_ADDRESS_NATIVE(base, SAMPLER_CSR_REG)
#define IORD_SAMPLER_CSR(base) \
    IORD(base, SAMPLER_CSR_REG)
#define IOWR_SAMPLER_CSR(base, data) \
    IOWR(base, SAMPLER_CSR_REG, data)

#define SAMPLER_CSR_ENABLED_MSK  (0x1)
#define SAMPLER_CSR_ENABLED_OFST (0)
#define SAMPLER_CSR_DONE_MSK     (0x2)
#define SAMPLER_CSR_DONE_OFST    (1)
#define SAMPLER_CSR_IRQ_MSK      (0x4)
#define SAMPLER_CSR_IRQ_OFST     (2)

#endif /* __SAMPLER_REGS_H__ */
