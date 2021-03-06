/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National  Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Nathan Lindquist <nallink01@gmail.com>
 *          Clay Kauzlaric <clay.kauzlaric@yahoo.com>
 *          Gino Wang <sihengwang2019@u.northwestern.edu>
 *          Jin Han <jinhan2019@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __VIRTIO_PCI
#define __VIRTIO_PCI

#include <dev/virtqueue.h>
#include <nautilus/nautilus.h>

#define MAX_VIRTQS 4
#define VIRTIO_MSI_NO_VECTOR 0xffff

// Virtio spec as of 4/18
enum virtio_pci_dev_type {
    VIRTIO_PCI_UNKNOWN = -1,
    VIRTIO_PCI_INVALID = 0,
    VIRTIO_PCI_NET,
    VIRTIO_PCI_BLOCK,
    VIRTIO_PCI_CONSOLE,
    VIRTIO_PCI_ENTROPY,
    VIRTIO_PCI_BALLOON,
    VIRTIO_PCI_IOMEM,
    VIRTIO_PCI_RPMSG,
    VIRTIO_PCI_SCSI_HOST,
    VIRTIO_PCI_9P,
    VIRTIO_PCI_WIFI,
    VIRTIO_PCI_RPROC_SERIAL,
    VIRTIO_PCI_CAIF,
    VIRTIO_PCI_FANCIER_BALLOON,
    VIRTIO_PCI_GPU,
    VIRTIO_PCI_TIMER,
    VIRTIO_PCI_INPUT,
};

enum virtio_pci_int_type {
    VIRTIO_PCI_LEGACY = 0,
    VIRTIO_PCI_MSI_X
};

enum virtio_pci_register_access_method_type { NONE=-1, MEMORY=0, IO};


// A wrapper for management of virtqs
struct virtio_pci_virtq {
    uint64_t size_bytes;
    // pointer to unaligned address supplied by nautilus
    // for memory management later (free)
    uint8_t *data ;
    // aligned start of vring within data
    uint8_t *aligned_data;

    // compatability with support code supplied in virtio docs
    // the pointers here go into aligned_data
    struct virtq vq;
    // for processing respones 
    uint16_t last_seen_used;

    // descriptor allocator
    uint16_t nfree;
    uint16_t head;
    spinlock_t lock;
};

// Generic info for a PCI_device
struct virtio_pci_dev {
    // our type
    enum virtio_pci_dev_type type;
    enum virtio_pci_int_type itype;
    
    // our internal state, set by specific driver
    void      *state;

    // how to destroy this device (set by specific driver, frees state)
    void      (*teardown)(struct virtio_pci_dev *);
    
    // we are a PCI device
    struct pci_dev *pci_dev;

    // we will be put on a list of all virtio devices
    struct list_head virtio_node;

    // the following is for legacy interrupts
    // we will try to use MSI-X first
    uint8_t   pci_intr;  // number on bus
    uint8_t   intr_vec;  // number we will see

    // The following hide the details of the PCI BARs, since
    // we only have one block of registers
    enum virtio_pci_register_access_method_type method;
    
    // Where registers are mapped into the I/O address space
    uint16_t  ioport_start;
    uint16_t  ioport_end;  

    // Where registers are mapped into the physical memory address space
    uint64_t  mem_start;
    uint64_t  mem_end;

    // Virtqs
    uint8_t num_virtqs;
    struct virtio_pci_virtq virtq[MAX_VIRTQS];

    // Feature bits (offered and accepted)
    uint32_t feat_offered;
    uint32_t feat_accepted;
};


int virtio_pci_init(struct naut_info * naut);
int virtio_pci_deinit();

int virtio_pci_virtqueue_init(struct virtio_pci_dev *dev);
int virtio_pci_virtqueue_deinit(struct virtio_pci_dev *dev);

int virtio_pci_ack_device(struct virtio_pci_dev *dev);
int virtio_pci_read_features(struct virtio_pci_dev *dev);
int virtio_pci_write_features(struct virtio_pci_dev *dev, uint32_t features);
int virtio_pci_start_device(struct virtio_pci_dev *dev);

// allocate a single descriptor
int virtio_pci_desc_alloc(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t *desc_idx);
// allocate a chain of descriptors and chain them together
// here desc_idx is an array of size count
int virtio_pci_desc_chain_alloc(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t *desc_idx, uint16_t count);
// free a single descriptor 
int virtio_pci_desc_free(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t desc_idx);
// free a chain descriptor starting with the given descriptor
int virtio_pci_desc_chain_free(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t desc_idx);

static inline uint32_t virtio_pci_read_regl(struct virtio_pci_dev *dev, uint32_t offset)
{
    uint32_t result;
    if (dev->method==MEMORY) {
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movl (%1), %0" : "=r"(result) : "r"(addr) : "memory");
    } else {
        result = inl(dev->ioport_start+offset);
    }
    return result;
}

static inline uint16_t virtio_pci_read_regw(struct virtio_pci_dev *dev, uint32_t offset)
{
    uint16_t result;
    if (dev->method==MEMORY) {
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movw (%1), %0" : "=r"(result) : "r"(addr) : "memory");
    } else {
        result = inw(dev->ioport_start+offset);
    }
    return result;
}

static inline uint8_t virtio_pci_read_regb(struct virtio_pci_dev *dev, uint32_t offset)
{
    uint8_t result;
    if (dev->method==MEMORY) {
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movb (%1), %0" : "=r"(result) : "r"(addr) : "memory");
    } else {
        result = inb(dev->ioport_start+offset);
    }
    return result;
}

static inline void virtio_pci_write_regl(struct virtio_pci_dev *dev, uint32_t offset, uint32_t data)
{
    if (dev->method==MEMORY) { 
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movl %1, (%0)" : "=r"(addr): "r"(data) : "memory");
    } else {
        outl(data,dev->ioport_start+offset);
    }
}

static inline void virtio_pci_write_regw(struct virtio_pci_dev *dev, uint32_t offset, uint16_t data)
{
    if (dev->method==MEMORY) { 
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movw %1, (%0)" : "=r"(addr): "r"(data) : "memory");
    } else {
        outw(data,dev->ioport_start+offset);
    }
}

static inline void virtio_pci_write_regb(struct virtio_pci_dev *dev, uint32_t offset, uint8_t data)
{
    if (dev->method==MEMORY) { 
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movb %1, (%0)" : "=r"(addr): "r"(data) : "memory");
    } else {
        outb(data,dev->ioport_start+offset);
    }
}


// common register offsets for legacy interface
#define DEVICE_FEATURES 0x0    // 4 byte
#define DRIVER_FEATURES 0x4    // 4 byte
#define QUEUE_ADDR      0x8    // 4 byte - page address
#define QUEUE_SIZE      0xc    // 2 byte
#define QUEUE_SEL       0xe    // 2 byte
#define QUEUE_NOTIFY    0x10   // 2 byte
#define DEVICE_STATUS   0x12   // 1 byte
#define ISR_STATUS      0x13   // 1 byte
#define DEVICE_REGS_START_LEGACY 0x14 // if no MSI-X
#define CONFIG_VEC      0x14   // 2 byte  if MSI-X
#define QUEUE_VEC       0x16   // 2 byte  if MSI-X
#define DEVICE_REGS_START_MSI_X    0x18   // device registers start if MSI-X


static inline uint32_t virtio_pci_device_regs_start(struct virtio_pci_dev *v)
{
    return v->itype==VIRTIO_PCI_MSI_X ? DEVICE_REGS_START_MSI_X : DEVICE_REGS_START_LEGACY;
}


#endif
