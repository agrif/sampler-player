#include <linux/blkdev.h>
#include <linux/fs.h>

#include "sampler-player.h"
#include "ioctls.h"

#define KERNEL_SECTOR_SIZE 512

int osuql_sp_major_num = 0;

static void memcpy_fromio_word(void* to, const volatile void __iomem* from, size_t count) {
    u32* t = to;
    while (count) {
        count--;
        *t = readl(from);
        t++;
        from += sizeof(u32);
    }
}

static void memcpy_toio_word(volatile void __iomem* to, const void* from, size_t count) {
    const u32* f = from;
    while (count) {
        count--;
        writel(*f, to);
        f++;
        to += sizeof(u32);
    }
}

static void request(struct request_queue* q) {
    struct request* req;
    struct sp_device* sp;
    bool chunks_left = true;
    size_t start, size;
    
    while ((req = blk_fetch_request(q)) != NULL) {
        // skip non-fs requests
        if (req->cmd_type != REQ_TYPE_FS) {
            __blk_end_request_all(req, -EIO);
            continue;
        }

        while (chunks_left) {
            sp = disk_to_sp(req->rq_disk);
            start = blk_rq_pos(req) * KERNEL_SECTOR_SIZE;
            size = blk_rq_cur_sectors(req) * KERNEL_SECTOR_SIZE;
            
            if (start + size > sp->length) {
                // end of device, should never happen
                chunks_left = __blk_end_request_cur(req, -EIO);
                continue;
            }

            // request buffer is in req->buffer
            // device buffer is in sp->buffer
            if (rq_data_dir(req)) {
                // write
                memcpy_toio_word(sp->buffer + start, req->buffer, size / sizeof(u32));
            } else {
                // read
                memcpy_fromio_word(req->buffer, sp->buffer + start, size / sizeof(u32));
            }
            
            chunks_left = __blk_end_request_cur(req, 0);
        }
    }
}

static int ioctl(struct block_device* blk, fmode_t mode, unsigned int cmd, unsigned long arg) {
    int err = 0;
    u8 csr;
    struct sp_device* sp = disk_to_sp(blk->bd_disk);

    // only handle known commands
    if (_IOC_TYPE(cmd) != OSUQL_SP_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) >= OSUQL_SP_IOC_MAX)
        return -ENOTTY;

    // check readability for pointer arguments
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    switch (cmd) {
        
    case OSUQL_SP_GET_ENABLED:
        return (ioread8(sp->csr) & CSR_ENABLED) ? 1 : 0;
    case OSUQL_SP_SET_ENABLED:
        csr = ioread8(sp->csr);
        if (arg) {
            iowrite8(csr | CSR_ENABLED, sp->csr);
        } else {
            iowrite8(csr & (~CSR_ENABLED), sp->csr);
        }
        return 0;

    case OSUQL_SP_GET_DONE:
        return (ioread8(sp->csr) & CSR_DONE) ? 1 : 0;

    default:
        return -ENOTTY;
    }
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
    sp->gd->private_data = sp;
    sp->gd->driverfs_dev = sp->dev;
    snprintf(sp->gd->disk_name, 32, "%s%i", BY_TYPE(sp->type, SAMPLER_DEV, PLAYER_DEV), sp->number);
    set_capacity(sp->gd, (sp->length + KERNEL_SECTOR_SIZE - 1) / KERNEL_SECTOR_SIZE);
    add_disk(sp->gd);
    
    return 0;
}

