#ifndef __SAMPLER_PLAYER_H_INCLUDED__
#define __SAMPLER_PLAYER_H_INCLUDED__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/genhd.h>

#define DRIVER_NAME "sampler-player"
#define SAMPLER_DEV "sampler"
#define PLAYER_DEV "player"

struct sp_device;

extern struct platform_driver osuql_sp_platform_driver;
extern int osuql_sp_major_num;

extern int osuql_sp_init_block(struct sp_device*);
extern void osuql_sp_remove_block(struct sp_device*);

#define CSR_ENABLED 0x1
#define CSR_DONE    0x2
#define CSR_IRQ     0x4

enum sp_type {
    TYPE_SAMPLER,
    TYPE_PLAYER,
};

#define BY_TYPE(ty, sampler, player) ((ty) == TYPE_SAMPLER ? (sampler) : (player))

// this must fit inside the type used for sp_device.number
#define MAX_DEVICES 255

// how many minor numbers each device can have (for partitions)
#define MINORS 1

// getting between our three major representations
// dev (struct device*), disk (struct gendisk*), and sp (struct sp_device*)
#define dev_to_sp(device) ((struct sp_device*)dev_get_drvdata(device))
#define sp_to_dev(sp) ((sp)->dev)
#define disk_to_sp(disk) ((struct sp_device*)(disk)->private_data)
#define sp_to_disk(sp) ((sp)->gd)

struct sp_device {
    //
    // set by driver.c:
    //

    struct device* dev;
    enum sp_type type;
    u8 number;
    u8 registered;
    
    struct resource* buffer_res;
    struct resource* csr_res;

    void* buffer_requested;
    void* csr_requested;

    void* buffer;
    void* csr;

    unsigned int irq;
    unsigned int interrupts;

    // see "attributes.h" for exposing these via sysfs

    // how many bits count as 1 sample
    u32 sample_width;

    // how many bits are used to address within each sample
    u8 sample_bits;
    // how many bytes each sample occupies, accounting for alignment
    u32 sample_length;

    // how many bits are used to address each timeslice
    u8 time_bits;
    // how many timeslices exist
    u32 time_length;

    // how many bits are used for addressing all memory
    u8 bits;
    // how many bytes are contained in this memory
    u32 length;

    //
    // set by block.c:
    //

    spinlock_t lock;
    struct request_queue* queue;
    struct gendisk* gd;
};

#endif /* __SAMPLER_PLAYER_H_INCLUDED__ */
