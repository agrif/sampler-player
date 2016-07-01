#ifndef __PLAYER_REGS_H__
#define __PLAYER_REGS_H__

#include <io.h>

#define PLAYER_CSR_REG 0
#define IOADDR_PLAYER_CSR(base) \
    __IO_CALC_ADDRESS_NATIVE(base, PLAYER_CSR_REG)
#define IORD_PLAYER_CSR(base) \
    IORD(base, PLAYER_CSR_REG)
#define IOWR_PLAYER_CSR(base, data) \
    IOWR(base, PLAYER_CSR_REG, data)

#define PLAYER_CSR_ENABLED_MSK  (0x1)
#define PLAYER_CSR_ENABLED_OFST (0)
#define PLAYER_CSR_DONE_MSK     (0x2)
#define PLAYER_CSR_DONE_OFST    (1)
#define PLAYER_CSR_IRQ_MSK      (0x4)
#define PLAYER_CSR_IRQ_OFST     (2)

#endif /* __PLAYER_REGS_H__ */
