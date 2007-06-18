/** @file
 *
 * vboxadd -- VirtualBox Guest Additions for Linux
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#include "the-linux-kernel.h"
#include "version-generated.h"

/* #define IRQ_DEBUG */

#include "vboxmod.h"
#include "waitcompat.h"
#include <VBox/log.h>

#define VERSION "0.5"

MODULE_DESCRIPTION("VirtualBox Guest Additions for Linux Module");
MODULE_AUTHOR("innotek GmbH");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING);
#endif

/* Runtime assert implementation for Linux ring 0 */
RTDECL(void) AssertMsg1(const char *pszExpr, unsigned uLine,
                        const char *pszFile, const char *pszFunction)
{
    elog("!!Assertion Failed!!\n"
         "Expression: %s\n"
         "Location  : %s(%d) %s\n",
         pszExpr, pszFile, uLine, pszFunction);
    Log(("!!Assertion Failed!!\n"
         "Expression: %s\n"
         "Location  : %s(%d) %s\n",
         pszExpr, pszFile, uLine, pszFunction));
}

/* Runtime assert implementation for Linux ring 0 */
RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{
    va_list ap;
    char    msg[256];

    va_start(ap, pszFormat);
    vsnprintf(msg, sizeof(msg) - 1, pszFormat, ap);
    msg[sizeof(msg) - 1] = '\0';
    elog("%s", msg);
    Log(("%s", msg));
    va_end(ap);
}

#if 0  /* We now have real backdoor logging */
/* Backdoor logging function, needed by the runtime */
RTDECL(size_t) RTLogBackdoorPrintf (const char *pszFormat, ...)
{
    va_list ap;
    char    msg[256];
    size_t n;

    va_start(ap, pszFormat);
    n = vsnprintf(msg, sizeof(msg) - 1, pszFormat, ap);
    msg[sizeof(msg) - 1] = '\0';
    printk ("%s", msg);
    va_end(ap);
    return n;
}
#endif

/** device extension structure (we only support one device instance) */
static VBoxDevice *vboxDev = NULL;
/** our file node major id (set dynamically) */
#ifdef CONFIG_VBOXADD_MAJOR
static unsigned int vbox_major = CONFIG_VBOXADD_MAJOR;
#else
static unsigned int vbox_major = 0;
#endif

DECLVBGL (void *) vboxadd_cmc_open (void)
{
    return vboxDev;
}

DECLVBGL (void) vboxadd_cmc_close (void *opaque)
{
    (void) opaque;
}

EXPORT_SYMBOL (vboxadd_cmc_open);
EXPORT_SYMBOL (vboxadd_cmc_close);

/**
 * File open handler
 *
 */
static int vboxadd_open(struct inode *inode, struct file *filp)
{
    /* no checks required */
    return 0;
}

/**
 * File close handler
 *
 */
static int vboxadd_release(struct inode *inode, struct file * filp)
{
    /* no action required */
    return 0;
}

/**
 * Wait for event
 *
 */
static void
vboxadd_wait_for_event_helper (VBoxDevice *dev, long timeout,
                               uint32_t in_mask, uint32_t * out_mask)
{
    BUG ();
}

static void
vboxadd_wait_for_event (VBoxGuestWaitEventInfo * info)
{
    long timeout;

    timeout = msecs_to_jiffies (info->u32TimeoutIn);
    vboxadd_wait_for_event_helper (vboxDev, timeout,
                                   info->u32EventMaskIn,
                                   &info->u32EventFlagsOut);
}


/**
 * IOCTL handler
 *
 */
static int vboxadd_ioctl(struct inode *inode, struct file *filp,
                         unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
        case IOCTL_VBOXGUEST_WAITEVENT:
        {
            VBoxGuestWaitEventInfo info;
            char *ptr = (void *) arg;

            if (copy_from_user (&info, ptr, sizeof (info)))
            {
                printk (KERN_ERR "vboxadd_ioctl: can not get event info\n");
                return -EFAULT;
            }

            vboxadd_wait_for_event (&info);

            ptr += offsetof (VBoxGuestWaitEventInfo, u32EventFlagsOut);
            if (put_user (info.u32EventFlagsOut, ptr))
            {
                printk (KERN_ERR "vboxadd_ioctl: can not put out_mask\n");
                return -EFAULT;
            }
            return 0;
        }

        case IOCTL_VBOXGUEST_VMMREQUEST:
        {
            VMMDevRequestHeader reqHeader;
            VMMDevRequestHeader *reqFull = NULL;
            size_t cbRequestSize;
            size_t cbVanillaRequestSize;
            int rc;

            if (_IOC_SIZE(cmd) != sizeof(VMMDevRequestHeader))
            {
                printk(KERN_ERR "vboxadd_ioctl: invalid VMM request structure size: %d\n",
                       _IOC_SIZE(cmd));
                return -EINVAL;
            }
            if (copy_from_user(&reqHeader, (void*)arg, _IOC_SIZE(cmd)))
            {
                printk(KERN_ERR "vboxadd_ioctl: copy_from_user failed for vmm request!\n");
                return -EFAULT;
            }
            /* get the request size */
            cbVanillaRequestSize = vmmdevGetRequestSize(reqHeader.requestType);
            if (!cbVanillaRequestSize)
            {
                printk(KERN_ERR "vboxadd_ioctl: invalid request type: %d\n",
                       reqHeader.requestType);
                return -EINVAL;
            }

            cbRequestSize = reqHeader.size;
            if (cbRequestSize < cbVanillaRequestSize)
            {
                printk(KERN_ERR
                       "vboxadd_ioctl: invalid request size: %d min: %d type: %d\n",
                       cbRequestSize,
                       cbVanillaRequestSize,
                       reqHeader.requestType);
                return -EINVAL;
            }
            /* request storage for the full request */
            rc = VbglGRAlloc(&reqFull, cbRequestSize, reqHeader.requestType);
            if (VBOX_FAILURE(rc))
            {
                printk(KERN_ERR
                       "vboxadd_ioctl: could not allocate request structure! rc = %d\n", rc);
                return -EFAULT;
            }
            /* now get the full request */
            if (copy_from_user(reqFull, (void*)arg, cbRequestSize))
            {
                printk(KERN_ERR
                       "vboxadd_ioctl: failed to fetch full request from user space!\n");
                VbglGRFree(reqFull);
                return -EFAULT;
            }

            /* now issue the request */
            rc = VbglGRPerform(reqFull);

            /* asynchronous processing? */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                VMMDevHGCMRequestHeader *reqHGCM = (VMMDevHGCMRequestHeader*)reqFull;
                wait_event (vboxDev->eventq, reqHGCM->fu32Flags & VBOX_HGCM_REQ_DONE);
                rc = reqFull->rc;
            }

            /* failed? */
            if (VBOX_FAILURE(rc) || VBOX_FAILURE(reqFull->rc))
            {
                printk(KERN_ERR "vboxadd_ioctl: request execution failed!\n");
                VbglGRFree(reqFull);
                return -EFAULT;
            }
            else
            {
                /* success, copy the result data to user space */
                if (copy_to_user((void*)arg, (void*)reqFull, cbRequestSize))
                {
                    printk(KERN_ERR
                           "vboxadd_ioctl: error copying request result to user space!\n");
                    VbglGRFree(reqFull);
                    return -EFAULT;
                }
            }
            VbglGRFree(reqFull);
            break;
        }

        case IOCTL_VBOXGUEST_HGCM_CALL:
        {
            /* This IOCTL allows the guest to make an HGCM call from user space.  The
               OS-independant part of the Guest Additions already contain code for making an
               HGCM call from the guest, but this code assumes that the call is made from the
               kernel's address space.  So before calling it, we have to copy all parameters
               to the HGCM call from user space to kernel space and reconstruct the structures
               passed to the call (which include pointers to other memory) inside the kernel's
               address space. */
            return vbox_ioctl_hgcm_call(arg, vboxDev);
        }

        case IOCTL_VBOXGUEST_CLIPBOARD_CONNECT:
        {
            static uint32_t u32ClientID = 0;
            VMMDevHGCMDisconnect *reqDisconnect = NULL;
            VMMDevHGCMConnect *reqConnect = NULL;
            size_t cbRequestSize;
            int rc;

            /* First, disconnect any old client. */
            if (u32ClientID != 0)
            {
                /* get the request size */
                cbRequestSize = vmmdevGetRequestSize(VMMDevReq_HGCMDisconnect);
                /* request storage for the request */
                rc = VbglGRAlloc((VMMDevRequestHeader **) &reqDisconnect, cbRequestSize,
                                 VMMDevReq_HGCMDisconnect);
                if (VBOX_FAILURE(rc))
                {
                    printk(KERN_ERR
                          "vboxadd_ioctl: could not allocate request structure! rc = %d\n", rc);
                    return -EFAULT;
                }
                /* now get the full request */
                vmmdevInitRequest(&reqDisconnect->header.header, VMMDevReq_HGCMDisconnect);
                reqDisconnect->u32ClientID = u32ClientID;

                /* now issue the request */
                rc = VbglGRPerform(&reqDisconnect->header.header);

                /* asynchronous processing? */
                if (rc == VINF_HGCM_ASYNC_EXECUTE)
                {
                    VMMDevHGCMRequestHeader *reqHGCM = &reqDisconnect->header;
                    wait_event (vboxDev->eventq, reqHGCM->fu32Flags & VBOX_HGCM_REQ_DONE);
                    rc = reqHGCM->header.rc;
                }

                /* failed? */
                if (VBOX_FAILURE(rc) || VBOX_FAILURE(reqDisconnect->header.header.rc))
                {
                    printk(KERN_ERR "vboxadd_ioctl: request execution failed!\n");
                    VbglGRFree(&reqDisconnect->header.header);
                    return -EFAULT;
                }
            }

            /* And connect... */
            /* get the request size */
            cbRequestSize = vmmdevGetRequestSize(VMMDevReq_HGCMConnect);
            /* request storage for the request */
            rc = VbglGRAlloc((VMMDevRequestHeader **) &reqConnect, cbRequestSize, VMMDevReq_HGCMConnect);
            if (VBOX_FAILURE(rc))
            {
                printk(KERN_ERR
                       "vboxadd_ioctl: could not allocate request structure! rc = %d\n", rc);
                return -EFAULT;
            }
            /* now get the full request */
            vmmdevInitRequest((VMMDevRequestHeader*)reqConnect, VMMDevReq_HGCMConnect);
            reqConnect->loc.type = VMMDevHGCMLoc_LocalHost_Existing;
            strcpy (reqConnect->loc.u.host.achName, "VBoxSharedClipboard");

            /* now issue the request */
            rc = VbglGRPerform(&reqConnect->header.header);

            /* asynchronous processing? */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                VMMDevHGCMRequestHeader *reqHGCM = &reqConnect->header;
                wait_event (vboxDev->eventq, reqHGCM->fu32Flags & VBOX_HGCM_REQ_DONE);
                rc = reqHGCM->header.rc;
            }

            /* failed? */
            if (VBOX_FAILURE(rc) || VBOX_FAILURE(reqConnect->header.header.rc))
            {
                printk(KERN_ERR "vboxadd_ioctl: request execution failed!\n");
                VbglGRFree(&reqConnect->header.header);
                return -EFAULT;
            }
            else
            {
                /* success, copy the result data to user space */
                u32ClientID = reqConnect->u32ClientID;
                if (copy_to_user((void*)arg, (void*)&(reqConnect->u32ClientID), sizeof(uint32_t)))
                {
                    printk(KERN_ERR
                           "vboxadd_ioctl: error copying request result to user space!\n");
                    VbglGRFree(&reqConnect->header.header);
                    return -EFAULT;
                }
            }
            VbglGRFree(&reqConnect->header.header);
            break;
        }

        default:
        {
            elog("vboxadd_ioctl: unknown command: %x, IOCTL_VBOXGUEST_HGCM_CALL is %x\n", cmd,
                 IOCTL_VBOXGUEST_HGCM_CALL);
            Log(("vboxadd_ioctl: unknown command: %x, IOCTL_VBOXGUEST_HGCM_CALL is %x\n", cmd,
                 IOCTL_VBOXGUEST_HGCM_CALL));
            return -EINVAL;
        }
    }
    return 0;
}

#ifdef DEBUG
static ssize_t
vboxadd_read (struct file *file, char *buf, size_t count, loff_t *loff)
{
    if (count != 8 || *loff != 0)
    {
        return -EINVAL;
    }
    *(uint32_t *) buf = vboxDev->pVMMDevMemory->V.V1_04.fHaveEvents;
    *(uint32_t *) (buf + 4) = vboxDev->u32Events;
    *loff += 8;
    return 8;
}
#endif

/** strategy handlers (file operations) */
static struct file_operations vbox_fops =
{
    .owner   = THIS_MODULE,
    .open    = vboxadd_open,
    .release = vboxadd_release,
    .ioctl   = vboxadd_ioctl,
#ifdef DEBUG
    .read    = vboxadd_read,
#endif
    .llseek  = no_llseek
};

#ifndef IRQ_RETVAL
/* interrupt handlers in 2.4 kernels don't return anything */
# define irqreturn_t void
# define IRQ_RETVAL(n)
#endif

/**
 * vboxadd_irq_handler
 *
 * Interrupt handler
 *
 * @returns scsi error code
 * @param irq                   Irq number
 * @param dev_id                Irq handler parameter
 * @param regs                  Regs
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static irqreturn_t vboxadd_irq_handler(int irq, void *dev_id)
#else
static irqreturn_t vboxadd_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
    int fIRQTaken = 0;
    int rcVBox;

#ifdef IRQ_DEBUG
    printk ("%s: vboxDev->pVMMDevMemory=%p vboxDev->pVMMDevMemory->fHaveEvents=%d\n",
            __func__, vboxDev->pVMMDevMemory, vboxDev->pVMMDevMemory->fHaveEvents);
#endif

    /* check if IRQ was asserted by VBox */
    if (vboxDev->pVMMDevMemory->V.V1_04.fHaveEvents != 0)
    {
#ifdef IRQ_DEBUG
        printk(KERN_INFO "vboxadd: got IRQ with event mask 0x%x\n",
               vboxDev->pVMMDevMemory->u32HostEvents);
#endif

        /* make a copy of the event mask */
        rcVBox = VbglGRPerform (&vboxDev->irqAckRequest->header);
        if (VBOX_SUCCESS(rcVBox) && VBOX_SUCCESS(vboxDev->irqAckRequest->header.rc))
        {
            if (likely (vboxDev->irqAckRequest->events))
            {
                vboxDev->u32Events |= vboxDev->irqAckRequest->events;
                wake_up (&vboxDev->eventq);
            }
        }
        else
        {
            /* impossible... */
            printk(KERN_ERR
                   "vboxadd: failed acknowledging IRQ! rc = %x, header.rc = %d\n",
                   rcVBox, vboxDev->irqAckRequest->header.rc);
            BUG ();
        }

        /* it was ours! */
        fIRQTaken = 1;
    }
#ifdef IRQ_DEBUG
    else
    {
        printk ("vboxadd: stale IRQ mem=%p events=%d devevents=%#x\n",
                vboxDev->pVMMDevMemory,
                vboxDev->pVMMDevMemory->fHaveEvents,
                vboxDev->u32Events);
    }
#endif
    /* it was ours */
    return IRQ_RETVAL(fIRQTaken);
}

/**
 * Helper function to reserve a fixed kernel address space window
 * and tell the VMM that it can safely put its hypervisor there.
 * This function might fail which is not a critical error.
 */
static int vboxadd_reserve_hypervisor(void)
{
    VMMDevReqHypervisorInfo *req = NULL;
    int rcVBox;

    /* allocate request structure */
    rcVBox = VbglGRAlloc(
        (VMMDevRequestHeader**)&req,
        sizeof(VMMDevReqHypervisorInfo),
        VMMDevReq_GetHypervisorInfo
        );
    if (VBOX_FAILURE(rcVBox))
    {
        printk(KERN_ERR "vboxadd: failed to allocate hypervisor info structure! rc = %d\n",
               rcVBox);
        goto bail_out;
    }
    /* query the hypervisor information */
    rcVBox = VbglGRPerform(&req->header);
    if (VBOX_SUCCESS(rcVBox) && VBOX_SUCCESS(req->header.rc))
    {
        /* are we supposed to make a reservation? */
        if (req->hypervisorSize)
        {
            /** @todo repeat this several times until we get an address the host likes */

            void *hypervisorArea;
            /* reserve another 4MB because the start needs to be 4MB aligned */
            uint32_t hypervisorSize = req->hypervisorSize + 0x400000;
            /* perform a fictive IO space mapping */
            hypervisorArea = ioremap(HYPERVISOR_PHYSICAL_START, hypervisorSize);
            if (hypervisorArea)
            {
                /* communicate result to VMM, align at 4MB */
                req->hypervisorStart    = (vmmDevHypPtr)ALIGNP(hypervisorArea, 0x400000);
                req->header.requestType = VMMDevReq_SetHypervisorInfo;
                req->header.rc          = VERR_GENERAL_FAILURE;
                rcVBox = VbglGRPerform(&req->header);
                if (VBOX_SUCCESS(rcVBox) && VBOX_SUCCESS(req->header.rc))
                {
                    /* store mapping for future unmapping */
                    vboxDev->hypervisorStart = hypervisorArea;
                    vboxDev->hypervisorSize  = hypervisorSize;
                }
                else
                {
                    printk(KERN_ERR "vboxadd: failed to set hypervisor region! "
                           "rc = %d, header.rc = %d\n",
                           rcVBox, req->header.rc);
                    goto bail_out;
                }
            }
            else
            {
                printk(KERN_ERR "vboxadd: failed to allocate 0x%x bytes of IO space\n",
                       hypervisorSize);
                goto bail_out;
            }
        }
    }
    else
    {
        printk(KERN_ERR "vboxadd: failed to query hypervisor info! rc = %d, header.rc = %d\n",
               rcVBox, req->header.rc);
        goto bail_out;
    }
    /* successful return */
    VbglGRFree(&req->header);
    return 0;
bail_out:
    /* error return */
    if (req)
        VbglGRFree(&req->header);
    return 1;
}

/**
 * Helper function to free the hypervisor address window
 *
 */
static int vboxadd_free_hypervisor(void)
{
    VMMDevReqHypervisorInfo *req = NULL;
    int rcVBox;

    /* allocate request structure */
    rcVBox = VbglGRAlloc(
        (VMMDevRequestHeader**)&req,
        sizeof(VMMDevReqHypervisorInfo),
        VMMDevReq_SetHypervisorInfo
        );
    if (VBOX_FAILURE(rcVBox))
    {
        printk(KERN_ERR
               "vboxadd: failed to allocate hypervisor info structure! rc = %d\n", rcVBox);
        goto bail_out;
    }
    /* reset the hypervisor information */
    req->hypervisorStart = 0;
    req->hypervisorSize  = 0;
    rcVBox = VbglGRPerform(&req->header);
    if (VBOX_SUCCESS(rcVBox) && VBOX_SUCCESS(req->header.rc))
    {
        /* now we can free the associated IO space mapping */
        iounmap(vboxDev->hypervisorStart);
        vboxDev->hypervisorStart = 0;
    }
    else
    {
        printk(KERN_ERR "vboxadd: failed to reset hypervisor info! rc = %d, header.rc = %d\n",
               rcVBox, req->header.rc);
        goto bail_out;
    }
    return 0;

 bail_out:
    if (req)
        VbglGRFree(&req->header);
    return 1;
}

/**
 * Helper to free resources
 *
 */
static void free_resources(void)
{
    if (vboxDev)
    {
        if (vboxDev->hypervisorStart)
        {
            vboxadd_free_hypervisor();
        }
        if (vboxDev->irqAckRequest)
        {
            VbglGRFree(&vboxDev->irqAckRequest->header);
            VbglTerminate();
        }
        if (vboxDev->pVMMDevMemory)
            iounmap(vboxDev->pVMMDevMemory);
        if (vboxDev->vmmdevmem)
            release_mem_region(vboxDev->vmmdevmem, vboxDev->vmmdevmem_size);
        if (vboxDev->irq)
            free_irq(vboxDev->irq, vboxDev);
        kfree(vboxDev);
        vboxDev = NULL;
    }
}

/**
 * Module initialization
 *
 */
static __init int init(void)
{
    int err;
    int rcVBox;
    struct pci_dev *pcidev = NULL;
    VMMDevReportGuestInfo *infoReq = NULL;

    printk(KERN_INFO "vboxadd: initializing. Version %s\n", VERSION);

    if (vboxadd_cmc_init ())
    {
        printk (KERN_ERR "vboxadd: could not init cmc.\n");
        return -ENODEV;
    }

    /*
     * Detect PCI device
     */
    pcidev = pci_find_device(VMMDEV_VENDORID, VMMDEV_DEVICEID, pcidev);
    if (!pcidev)
    {
        printk(KERN_ERR "vboxadd: VirtualBox PCI device not found.\n");
        return -ENODEV;
    }

    err = pci_enable_device (pcidev);
    if (err)
    {
        printk (KERN_ERR "vboxadd: could not enable device: %d\n", err);
        return -ENODEV;
    }

    /* register a character device */
    err = register_chrdev(vbox_major, "vboxadd", &vbox_fops);
    if (err < 0 || ((vbox_major & err) || (!vbox_major && !err)))
    {
        printk(KERN_ERR "vboxadd: register_chrdev failed: vbox_major: %d, err = %d\n",
               vbox_major, err);
        return -ENODEV;
    }
    /* if no major code was set, take the return value */
    if (!vbox_major)
        vbox_major = err;

    /* allocate and initialize device extension */
    vboxDev = kmalloc(sizeof(*vboxDev), GFP_KERNEL);
    if (!vboxDev)
    {
        printk(KERN_ERR "vboxadd: cannot allocate device!\n");
        err = -ENOMEM;
        goto fail;
    }
    memset(vboxDev, 0, sizeof(*vboxDev));
    snprintf(vboxDev->name, sizeof(vboxDev->name), "vboxadd");

    /* get the IO port region */
    vboxDev->io_port = pci_resource_start(pcidev, 0);

    /* get the memory region */
    vboxDev->vmmdevmem = pci_resource_start(pcidev, 1);
    /* and size */
    vboxDev->vmmdevmem_size = pci_resource_len(pcidev, 1);

    /* all resources found? */
    if (!vboxDev->io_port || !vboxDev->vmmdevmem || !vboxDev->vmmdevmem_size)
    {
        printk(KERN_ERR "vboxadd: did not find expected hardware resources!\n");
        goto fail;
    }

    /* request ownership of adapter memory */
    if (request_mem_region(vboxDev->vmmdevmem, vboxDev->vmmdevmem_size, "vboxadd") == 0)
    {
        printk(KERN_ERR "vboxadd: failed to request adapter memory!\n");
        goto fail;
    }

    /* map adapter memory into kernel address space and check version */
    vboxDev->pVMMDevMemory = (VMMDevMemory *) ioremap(
        vboxDev->vmmdevmem,
        vboxDev->vmmdevmem_size
        );
    if (!vboxDev->pVMMDevMemory)
    {
        printk (KERN_ERR "vboxadd: ioremap failed\n");
        goto fail;
    }

    if (vboxDev->pVMMDevMemory->u32Version != VMMDEV_MEMORY_VERSION)
    {
        printk(KERN_ERR
               "vboxadd: invalid VMM device memory version! (got 0x%x, expected 0x%x)\n",
               vboxDev->pVMMDevMemory->u32Version, VMMDEV_MEMORY_VERSION);
        goto fail;
    }

    /* initialize VBGL subsystem */
    rcVBox = VbglInit(vboxDev->io_port, vboxDev->pVMMDevMemory);
    if (VBOX_FAILURE(rcVBox))
    {
        printk(KERN_ERR "vboxadd: could not initialize VBGL subsystem! rc = %d\n", rcVBox);
        goto fail;
    }

    /* report guest information to host, this must be done as the very first request */
    rcVBox = VbglGRAlloc(
        (VMMDevRequestHeader**)&infoReq,
        sizeof(VMMDevReportGuestInfo),
        VMMDevReq_ReportGuestInfo
        );
    if (VBOX_FAILURE(rcVBox))
    {
        printk(KERN_ERR "vboxadd: could not allocate request structure! rc = %d\n", rcVBox);
        goto fail;
    }

    /* report guest version to host, the VMMDev requires that to be done first */
    infoReq->guestInfo.additionsVersion = VMMDEV_VERSION;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 0)
    infoReq->guestInfo.osType = OSTypeLinux26;
#else
    infoReq->guestInfo.osType = OSTypeLinux24;
#endif
    rcVBox = VbglGRPerform(&infoReq->header);
    if (VBOX_FAILURE(rcVBox) || VBOX_FAILURE(infoReq->header.rc))
    {
        printk(KERN_ERR
               "vboxadd: error reporting guest info to host! rc = %d, header.rc = %d\n",
               rcVBox, infoReq->header.rc);
        VbglGRFree(&infoReq->header);
        goto fail;
    }
    VbglGRFree(&infoReq->header);

    /* perform hypervisor address space reservation */
    if (vboxadd_reserve_hypervisor())
    {
        /* we just ignore the error, no address window reservation, non fatal */
    }

    /* allocate a VMM request structure for use in the ISR */
    rcVBox = VbglGRAlloc(
        (VMMDevRequestHeader**)&vboxDev->irqAckRequest,
        sizeof(VMMDevEvents),
        VMMDevReq_AcknowledgeEvents
        );
    if (VBOX_FAILURE(rcVBox))
    {
        printk(KERN_ERR "vboxadd: could not allocate request structure! rc = %d\n", rcVBox);
        goto fail;
    }

    /* get ISR */
    err = request_irq(pcidev->irq, vboxadd_irq_handler, SA_SHIRQ, "vboxadd", vboxDev);
    if (err)
    {
        printk(KERN_ERR "vboxadd: Could not request IRQ %d, err: %d\n", pcidev->irq, err);
        goto fail;
    }
    vboxDev->irq = pcidev->irq;

    init_waitqueue_head (&vboxDev->eventq);

    /* some useful information for the user */
    printk(KERN_INFO
           "vboxadd: major code: %d, "
           "using irq %d, "
           "io port 0x%x, memory at 0x%x (size %d bytes), "
           "hypervisor window at 0x%p (size 0x%x bytes)\n",
           vbox_major, vboxDev->irq, vboxDev->io_port,
           vboxDev->vmmdevmem, vboxDev->vmmdevmem_size,
           vboxDev->hypervisorStart, vboxDev->hypervisorSize);

    /* successful return */
    return 0;
fail:
    free_resources();
    unregister_chrdev(vbox_major, "vboxadd");
    return err;
}

/**
 * Module termination
 *
 */
static __exit void fini(void)
{
    printk(KERN_DEBUG "vboxadd: unloading...\n");

    unregister_chrdev(vbox_major, "vboxadd");
    free_resources();
    vboxadd_cmc_fini ();
    printk(KERN_DEBUG "vboxadd: unloaded\n");
}

module_init(init);
module_exit(fini);

/* PCI hotplug structure */
static const struct pci_device_id __devinitdata vmmdev_pci_id[] =
{
    {
        .vendor = VMMDEV_VENDORID,
        .device = VMMDEV_DEVICEID
    },
    {
        /* empty entry */
    }
};
MODULE_DEVICE_TABLE(pci, vmmdev_pci_id);

int __gxx_personality_v0 = 0xdeadbeef;

/*
 * Local Variables:
 * c-mode: bsd
 * indent-tabs-mode: nil
 * c-plusplus: evil
 * End:
 */
