/** @file
 *
 * vboxadd -- VirtualBox Guest Additions for Linux
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef VBOXMOD_H
#define VBOXMOD_H

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>
#include <iprt/asm.h>

typedef struct VBoxDevice VBoxDevice;
struct VBoxDevice
{
    /** the device name */
    char name[128];
    /** file node minor code */
    unsigned minor;
    /** IRQ number */
    unsigned irq;
    /** first IO port */
    unsigned short io_port;
    /** physical address of device memory */
    uint32_t vmmdevmem;
    /** size of adapter memory */
    size_t vmmdevmem_size;

    /** kernel space mapping of the adapter memory */
    VMMDevMemory *pVMMDevMemory;
    /** current pending events mask */
    uint32_t u32Events;
    /** request structure to acknowledge events in ISR */
    VMMDevEvents *irqAckRequest;
    /** start of the hypervisor window */
    void *hypervisorStart;
    /** size of the hypervisor window in bytes */
    uint32_t hypervisorSize;
    /** event synchronization */
    wait_queue_head_t eventq;
};

extern int vboxadd_verbosity;

#define wlog(...) printk (KERN_WARNING "vboxadd: " __VA_ARGS__)
#define ilog(...) printk (KERN_INFO "vboxadd: " __VA_ARGS__)
#define dlog(...) printk (KERN_DEBUG "vboxadd: " __VA_ARGS__)
#define elog(...) printk (KERN_ERR "vboxadd: " __VA_ARGS__)

#define vlog(n, ...) \
if (n >= vboxadd_verbosity) printk (KERN_DEBUG "vboxadd: " __VA_ARGS__)

extern int vboxadd_cmc_init (void);
extern void vboxadd_cmc_fini (void);

#endif /* !VBOXMOD_H */
