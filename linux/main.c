#include <linux/init.h>

#include "sampler-player.h"

MODULE_AUTHOR("Aaron Griffith");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Sampler/Player Control");

static int __init initialize(void) {
    int ret;

    osuql_sp_major_num = register_blkdev(0, DRIVER_NAME);
    if (!osuql_sp_major_num) {
        printk(KERN_ERR DRIVER_NAME ": Unable to register major number.\n");
        return -ENOMEM;
    }

    ret = platform_driver_register(&osuql_sp_platform_driver);
    if (ret) {
        unregister_blkdev(osuql_sp_major_num, DRIVER_NAME);
        
        printk(KERN_ERR DRIVER_NAME ": Unable to register platform driver.\n");
        return ret;
    }
    
    return 0;
}

static void __exit deinitialize(void) {
    platform_driver_unregister(&osuql_sp_platform_driver);
    unregister_blkdev(osuql_sp_major_num, DRIVER_NAME);
}

module_init(initialize);
module_exit(deinitialize);
