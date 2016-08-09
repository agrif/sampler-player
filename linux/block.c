#include <linux/blkdev.h>

#include "sampler-player.h"

#define KERNEL_SECTOR_SIZE 512

int osuql_sp_major_num = 0;

static void request(struct request_queue* q) {
    struct request* req;
    while ((req = blk_fetch_request(q)) != NULL) {
        // skip all for now
        printk(KERN_NOTICE DRIVER_NAME ": skipping request.\n");
        __blk_end_request_all(req, -EIO);
        continue;
    }
}

static int ioctl(struct block_device* blk, fmode_t mode, unsigned int cmd, unsigned long arg) {
    return -ENOTTY; // unknown command
}

static struct block_device_operations ops = {
    .owner = THIS_MODULE,
    .ioctl = ioctl,
};

void osuql_sp_remove_block(struct sp_device* sp) {
    if (sp) {
        if (sp->gd) {
            del_gendisk(sp->gd);
            put_disk(sp->gd);
        }
        if (sp->queue)
            blk_cleanup_queue(sp->queue);
    }
}

int osuql_sp_init_block(struct sp_device* sp) {
    spin_lock_init(&sp->lock);

    sp->queue = blk_init_queue(request, &sp->lock);
    if (!sp->queue)
        return -EINVAL;
    blk_queue_logical_block_size(sp->queue, sp->sample_length);

    sp->gd = alloc_disk(MINORS);
    if (!sp->gd)
        return -EINVAL;

    sp->gd->major = osuql_sp_major_num;
    sp->gd->first_minor = BY_TYPE(sp->type, 0, (MAX_DEVICES + 1) * MINORS);
    sp->gd->first_minor += MINORS * sp->number;
    sp->gd->fops = &ops;
    sp->gd->queue = sp->queue;
    sp->gd->driverfs_dev = sp->dev;
    snprintf(sp->gd->disk_name, 32, "%s%i", BY_TYPE(sp->type, SAMPLER_DEV, PLAYER_DEV), sp->number);
    set_capacity(sp->gd, (sp->length + KERNEL_SECTOR_SIZE - 1) / KERNEL_SECTOR_SIZE);
    add_disk(sp->gd);
    
    return 0;
}

