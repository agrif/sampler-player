#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include "sampler-player.h"

static struct of_device_id of_match[] = {
    { .compatible = "osuql,player-1.0",  .data = (void*)TYPE_PLAYER  },
    { .compatible = "osuql,sampler-1.0", .data = (void*)TYPE_SAMPLER },
    {}
};

// registration tables for device numbers
static u8 sampler_nums[MAX_DEVICES];
static u8 player_nums[MAX_DEVICES];

MODULE_DEVICE_TABLE(of, of_match);

#define STRUCT_ATTRIBUTE(name, ...)                                     \
    static ssize_t name##_show(struct device* dev, struct device_attribute* attr, char* buf) { \
        struct sp_device* sp = dev_to_sp(dev);                          \
        return scnprintf(buf, PAGE_SIZE, __VA_ARGS__);                  \
    }                                                                   \
    static DEVICE_ATTR(name, S_IRUGO, name##_show, NULL);
#define CSR_ATTRIBUTE(name, write, mask)                                \
    static ssize_t name##_show(struct device* dev, struct device_attribute* attr, char* buf) { \
        return scnprintf(buf, PAGE_SIZE, "%i\n", (ioread8(dev_to_sp(dev)->csr) & mask) ? 1 : 0); \
    }                                                                   \
    static ssize_t name##_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) { \
        struct sp_device* sp = dev_to_sp(dev);                          \
        u8 input, csr;                                                  \
        int ret = kstrtou8(buf, 10, &input);                            \
        if (ret < 0)                                                    \
            return ret;                                                 \
        csr = ioread8(sp->csr);                                         \
        if (input) {                                                    \
            iowrite8(csr | mask, sp->csr);                              \
        } else  {                                                       \
            iowrite8(csr & (~mask), sp->csr);                           \
        }                                                               \
        return count;                                                   \
    }                                                                   \
    static DEVICE_ATTR(name, S_IRUGO | (write ? S_IWUSR : 0), name##_show, name##_store);
#include "attributes.h"

static irqreturn_t handle_interrupt(int irq, void* cookie) {
    struct sp_device* sp = cookie;
    u8 csr = ioread8(sp->csr);
    csr &= ~CSR_IRQ;
    iowrite8(csr, sp->csr);
    sp->interrupts++;
    return IRQ_HANDLED;
}

static int remove(struct platform_device* dev) {
    struct sp_device* sp;

    sp = dev_to_sp(&(dev->dev));
    if (sp) {
        osuql_sp_remove_block(sp);

        // this is safe to call on files that don't exist, thankfully
#define ATTRIBUTE(name) device_remove_file(&dev->dev, &dev_attr_##name);
#include "attributes.h"

        // unregister irq if we've registered it
        if (sp->irq)
            free_irq(sp->irq, sp);
        
        // unmap our memory
        if (sp->buffer)
            iounmap(sp->buffer);
        if (sp->csr)
            iounmap(sp->csr);

        // release our hold on it
        if (sp->buffer_requested)
            release_mem_region(sp->buffer_res->start, resource_size(sp->buffer_res));
        if (sp->csr_requested)
            release_mem_region(sp->csr_res->start, resource_size(sp->csr_res));

        if (sp->number != MAX_DEVICES) {
            u8* nums = BY_TYPE(sp->type, sampler_nums, player_nums);
            nums[sp->number] = 0;
        }

        if (sp->registered) {
            printk(KERN_INFO "%s%i: Unregistered device.\n", BY_TYPE(sp->type, SAMPLER_DEV, PLAYER_DEV), sp->number);
        }
    }
    
    return 0;
}

static int probe(struct platform_device* dev) {
    const struct of_device_id* of_id;
    u8* numtable;
    const void* ptr;
    int ret;
    struct sp_device* sp = NULL;

    // get our match data
    of_id = of_match_node(of_match, dev->dev.of_node);
    if (!of_id)
        return -EINVAL;

    // create a home for our information
    sp = kmalloc(sizeof(struct sp_device), GFP_KERNEL);
    if (!sp)
        return -EINVAL;
    memset(sp, 0, sizeof(struct sp_device));
    dev_set_drvdata(&dev->dev, sp);
    sp->number = MAX_DEVICES;
    sp->dev = &dev->dev;

    // set up our type
    sp->type = (enum sp_type)(of_id->data);
    numtable = BY_TYPE(sp->type, sampler_nums, player_nums);

    // find and register a number
    sp->number = 0;
    for (sp->number = 0; sp->number < MAX_DEVICES; sp->number++) {
        if (!numtable[sp->number]) {
            numtable[sp->number] = 1;
            break;
        }
    }
    if (sp->number == MAX_DEVICES) {
        remove(dev);
        return -EINVAL;
    }

    // read in required metadata
    ptr = of_get_property(dev->dev.of_node, "sample-width", NULL);
    if (!ptr) {
        remove(dev);
        return -EINVAL;
    } else {
        sp->sample_width = be32_to_cpup(ptr);
    }

    ptr = of_get_property(dev->dev.of_node, "sample-bits", NULL);
    if (!ptr) {
        remove(dev);
        return -EINVAL;
    } else {
        sp->sample_bits = be32_to_cpup(ptr);
        sp->sample_length = 1 << sp->sample_bits;
    }

    ptr = of_get_property(dev->dev.of_node, "time-bits", NULL);
    if (!ptr) {
        remove(dev);
        return -EINVAL;
    } else {
        sp->time_bits = be32_to_cpup(ptr);
        sp->time_length = 1 << sp->time_bits;
        sp->bits = sp->time_bits + sp->sample_bits;
        sp->length = sp->time_length * sp->sample_length;
    }

    // get resource data for buffer / csr
    sp->buffer_res = platform_get_resource_byname(dev, IORESOURCE_MEM, "buffer");
    sp->csr_res = platform_get_resource_byname(dev, IORESOURCE_MEM, "csr");
    if (!sp->buffer_res || !sp->csr_res) {
        remove(dev);
        return -EINVAL;
    }

    // request memory regions
    sp->buffer_requested = request_mem_region(sp->buffer_res->start, resource_size(sp->buffer_res), DRIVER_NAME);
    sp->csr_requested = request_mem_region(sp->csr_res->start, resource_size(sp->csr_res), DRIVER_NAME);
    if (!sp->buffer_requested || !sp->csr_requested) {
        remove(dev);
        return -EINVAL;
    }

    // map them into our memory
    sp->buffer = ioremap(sp->buffer_res->start, resource_size(sp->buffer_res));
    sp->csr = ioremap(sp->csr_res->start, resource_size(sp->csr_res));
    if (!sp->buffer || !sp->csr) {
        remove(dev);
        return -EINVAL;
    }

    // find and register our irq
    sp->irq = irq_of_parse_and_map(dev->dev.of_node, 0);
    if (sp->irq) {
        // the irq is, actually, optional. since it does very little.
        ret = request_irq(sp->irq, handle_interrupt, 0, DRIVER_NAME, sp);
        if (ret < 0) {
            remove(dev);
            return ret;
        }
    }

    // register our attributes
#define ATTRIBUTE(name)                                             \
    ret = device_create_file(&dev->dev, &dev_attr_##name);          \
    if (ret < 0) {                                                  \
        remove(dev);                                                \
        return ret;                                                 \
    }
#include "attributes.h"

    // create our block device
    ret = osuql_sp_init_block(sp);
    if (ret < 0) {
        remove(dev);
        return ret;
    }

    printk(KERN_INFO "%s%i: Registered device.\n", BY_TYPE(sp->type, SAMPLER_DEV, PLAYER_DEV), sp->number);
    sp->registered = 1;

    return 0;
}

struct platform_driver osuql_sp_platform_driver = {
    .probe = probe,
    .remove = remove,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match,
    },
};

