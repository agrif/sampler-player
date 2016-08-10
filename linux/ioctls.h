/*
 * this file defines the various ioctl command numbers
 * it is intended to be copied around and re-used
 */

#include <linux/ioctl.h>

/* selected arbitrarily from those not taken in ioctl-number.txt */
#define OSUQL_SP_IOC_MAGIC 0x9d

#define OSUQL_SP_GET_ENABLED _IO(OSUQL_SP_IOC_MAGIC, 0)
#define OSUQL_SP_SET_ENABLED _IO(OSUQL_SP_IOC_MAGIC, 1)

#define OSUQL_SP_GET_DONE    _IO(OSUQL_SP_IOC_MAGIC, 2)

#define OSUQL_SP_IOC_MAX 3
