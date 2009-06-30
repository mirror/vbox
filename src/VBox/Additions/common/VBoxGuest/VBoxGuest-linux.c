/* $Rev$ */
/** @file
 * VBoxGuest - Linux specifics.
 *
 * Note. Unfortunately, the difference between this and SUPDrv-linux.c is
 *       a little bit too big to be helpful.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 * Some lines of code to disable the local APIC on x86_64 machines taken
 * from a Mandriva patch by Gwenole Beauchesne <gbeauchesne@mandriva.com>.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "VBoxGuestInternal.h"
#include "the-linux-kernel.h"
#include <linux/miscdevice.h>
#include "version-generated.h"

#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <VBox/log.h>
#include <iprt/mp.h>
#include <iprt/mem.h>


#define xstr(s) str(s)
#define str(s) #s

/** The device name. */
#define DEVICE_NAME             "vboxguest"
/** The device name for the device node open to everyone.. */
#define DEVICE_NAME_USER        "vboxuser"


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
# define PCI_DEV_GET(v,d,p)     pci_get_device(v,d,p)
# define PCI_DEV_PUT(x)         pci_dev_put(x)
#else
# define PCI_DEV_GET(v,d,p)     pci_find_device(v,d,p)
# define PCI_DEV_PUT(x)         do {} while(0)
#endif

/* 2.4.x compatability macros that may or may not be defined. */
#ifndef IRQ_RETVAL
# define irqreturn_t            void
# define IRQ_RETVAL(n)
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  vboxguestLinuxModInit(void);
static void vboxguestLinuxModExit(void);
static int  vboxguestLinuxOpen(struct inode *pInode, struct file *pFilp);
static int  vboxguestLinuxRelease(struct inode *pInode, struct file *pFilp);
#ifdef HAVE_UNLOCKED_IOCTL
static long vboxguestLinuxIOCtl(struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
#else
static int  vboxguestLinuxIOCtl(struct inode *pInode, struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Device extention & session data association structure.
 */
static VBOXGUESTDEVEXT          g_DevExt;
/** The PCI device. */
static struct pci_dev          *g_pPciDev;
/** The base of the I/O port range. */
static RTIOPORT                 g_IOPortBase;
/** The base of the MMIO range. */
static RTHCPHYS                 g_MMIOPhysAddr = NIL_RTHCPHYS;
/** The size of the MMIO range as seen by PCI. */
static uint32_t                 g_cbMMIO;
/** The pointer to the mapping of the MMIO range. */
static void                    *g_pvMMIOBase;

/** Our file node major id.
 * Either set dynamically at run time or statically at compile time. */
#ifdef CONFIG_VBOXADD_MAJOR
static unsigned int             g_iModuleMajor = CONFIG_VBOXADD_MAJOR;
#else
static unsigned int             g_iModuleMajor = 0;
#endif


/** The file_operations structure. */
static struct file_operations   g_FileOps =
{
    owner:          THIS_MODULE,
    open:           vboxguestLinuxOpen,
    release:        vboxguestLinuxRelease,
#ifdef HAVE_UNLOCKED_IOCTL
    unlocked_ioctl: vboxguestLinuxIOCtl,
#else
    ioctl:          vboxguestLinuxIOCtl,
#endif
};

/** The miscdevice structure. */
static struct miscdevice        g_MiscDevice =
{
    minor:          MISC_DYNAMIC_MINOR,
    name:           DEVICE_NAME,
    fops:           &g_FileOps,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name:     DEVICE_NAME,
#endif
};

/** The file_operations structure for the user device.
 * @remarks For the time being we'll be using the same implementation as
 *          /dev/vboxguest here. */
static struct file_operations   g_FileOpsUser =
{
    owner:          THIS_MODULE,
    open:           vboxguestLinuxOpen,
    release:        vboxguestLinuxRelease,
#ifdef HAVE_UNLOCKED_IOCTL
    unlocked_ioctl: vboxguestLinuxIOCtl,
#else
    ioctl:          vboxguestLinuxIOCtl,
#endif
};

/** The miscdevice structure for the user device. */
static struct miscdevice        g_MiscDeviceUser =
{
    minor:          MISC_DYNAMIC_MINOR,
    name:           DEVICE_NAME_USER,
    fops:           &g_FileOpsUser,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name:     DEVICE_NAME_USER,
#endif
};


/** PCI hotplug structure. */
static const struct pci_device_id __devinitdata g_VBoxGuestPciId[] =
{
    {
        vendor:     VMMDEV_VENDORID,
        device:     VMMDEV_DEVICEID
    },
    {
        /* empty entry */
    }
};
MODULE_DEVICE_TABLE(pci, g_VBoxGuestPciId);


/**
 * Converts a VBox status code to a linux error code.
 *
 * @returns corresponding negative linux error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int vboxguestLinuxConvertToNegErrno(int rc)
{
    if (   rc > -1000
        && rc < 1000)
        return -RTErrConvertToErrno(rc);
    switch (rc)
    {
        case VERR_HGCM_SERVICE_NOT_FOUND:      return -ESRCH;
        case VINF_HGCM_CLIENT_REJECTED:        return 0;
        case VERR_HGCM_INVALID_CMD_ADDRESS:    return -EFAULT;
        case VINF_HGCM_ASYNC_EXECUTE:          return 0;
        case VERR_HGCM_INTERNAL:               return -EPROTO;
        case VERR_HGCM_INVALID_CLIENT_ID:      return -EINVAL;
        case VINF_HGCM_SAVE_STATE:             return 0;
        /* No reason to return this to a guest */
        // case VERR_HGCM_SERVICE_EXISTS:         return -EEXIST;
        default:
            AssertMsgFailed(("Unhandled error code %Rrc\n", rc));
            return -EPROTO;
    }
}



/**
 * Does the PCI detection and init of the device.
 *
 * @returns 0 on success, negated errno on failure.
 */
static int __init vboxguestLinuxInitPci(void)
{
    struct pci_dev *pPciDev;
    int             rc;

    pPciDev = PCI_DEV_GET(VMMDEV_VENDORID, VMMDEV_DEVICEID, NULL);
    if (pPciDev)
    {
        rc = pci_enable_device(pPciDev);
        if (rc >= 0)
        {
            /* I/O Ports are mandatory, the MMIO bit is not. */
            if (pci_resource_start(pPciDev, 0) != 0)
            {
                /*
                 * Map the register address space.
                 */
                g_MMIOPhysAddr = pci_resource_start(pPciDev, 1);
                g_cbMMIO       = pci_resource_len(pPciDev, 1);
                g_IOPortBase   = pci_resource_start(pPciDev, 0);
                if (request_mem_region(g_MMIOPhysAddr, g_cbMMIO, DEVICE_NAME) != NULL)
                {
                    g_pvMMIOBase = ioremap(g_MMIOPhysAddr, g_cbMMIO);
                    if (g_DevExt.pVMMDevMemory)
                    {
                        /** @todo why aren't we requesting ownership of the I/O ports as well? */
                        g_pPciDev = pPciDev;
                        return 0;
                    }

                    /* failure cleanup path */
                    LogRel((DEVICE_NAME ": ioremap failed\n"));
                    rc = -ENOMEM;
                    release_mem_region(g_MMIOPhysAddr, g_cbMMIO);
                }
                else
                {
                    LogRel((DEVICE_NAME ": failed to obtain adapter memory\n"));
                    rc = -EBUSY;
                }
                g_MMIOPhysAddr = NIL_RTHCPHYS;
                g_cbMMIO       = 0;
                g_IOPortBase   = 0;
            }
            else
            {
                LogRel((DEVICE_NAME ": did not find expected hardware resources\n"));
                rc = -ENXIO;
            }
            pci_disable_device(pPciDev);
        }
        else
            LogRel((DEVICE_NAME ": could not enable device: %d\n", rc));
        PCI_DEV_PUT(pPciDev);
    }
    else
    {
        printk(KERN_ERR DEVICE_NAME ": VirtualBox Guest PCI device not found.\n");
        rc = -ENODEV;
    }
    return rc;
}


/**
 * Clean up the usage of the PCI device.
 */
static void __exit vboxguestLinuxTermPci(void)
{
    struct pci_dev *pPciDev = g_pPciDev;
    g_pPciDev = NULL;
    if (pPciDev)
    {
        iounmap(g_pvMMIOBase);
        g_pvMMIOBase = NULL;

        release_mem_region(g_MMIOPhysAddr, g_cbMMIO);
        g_MMIOPhysAddr = NIL_RTHCPHYS;
        g_cbMMIO = 0;

        pci_disable_device(pPciDev);
    }
}


/**
 * Interrupt service routine.
 *
 * @returns In 2.4 it returns void.
 *          In 2.6 we indicate whether we've handled the IRQ or not.
 *
 * @param   iIrq            The IRQ number.
 * @param   pvDevId         The device ID, a pointer to g_DevExt.
 * @param   pvRegs          Register set. Removed in 2.6.19.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static irqreturn_t vboxguestLinuxISR(int iIrrq, void *pvDevId)
#else
static irqreturn_t vboxguestLinuxISR(int iIrrq, void *pvDevId, struct pt_regs *pRegs)
#endif
{
    bool fTaken = VBoxGuestCommonISR(&g_DevExt);
    /** @todo  if (vboxDev->irqAckRequest->events &
     *             VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
     *             kill_fasync(&vboxDev->async_queue, SIGIO, POLL_IN);
     */
    return IRQ_RETVAL(fTaken);
}


/**
 * Registers the ISR.
 */
static int __init vboxguestLinuxInitISR(void)
{
    int rc;

    rc = request_irq(g_pPciDev->irq,
                     vboxguestLinuxISR,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
                     IRQF_SHARED,
#else
                     SA_SHIRQ,
#endif
                     DEVICE_NAME,
                     &g_DevExt);
    if (rc)
    {
        LogRel((DEVICE_NAME ": could not request IRQ %d: err=%d\n", g_pPciDev->irq, rc));
        return rc;
    }
    return 0;
}


/**
 * Deregisters the ISR.
 */
static void __exit vboxguestLinuxTermISR(void)
{
    free_irq(g_pPciDev->irq, &g_DevExt);
}


/**
 * Creates the device nodes.
 *
 * @returns 0 on success, negated errno on failure.
 */
static int __init vboxguestLinuxInitDeviceNodes(void)
{
    int rc;

    /*
     * The full feature device node.
     */
    if (g_iModuleMajor > 0)
    {
        rc = register_chrdev(g_iModuleMajor, DEVICE_NAME, &g_FileOps);
        if (rc < 0)
        {
            LogRel((DEVICE_NAME ": register_chrdev failed: g_iModuleMajor: %d, rc: %d\n", g_iModuleMajor, rc));
            return rc;
        }
    }
    else
    {
        rc = misc_register(&g_MiscDevice);
        if (rc)
        {
            LogRel((DEVICE_NAME ": misc_register failed for %s (rc=%d)\n", DEVICE_NAME, rc));
            return rc;
        }
    }

    /*
     * The device node intended to be accessible by all users.
     */
    rc = misc_register(&g_MiscDeviceUser);
    if (rc)
    {
        LogRel((DEVICE_NAME ": misc_register failed for %s (rc=%d)\n", DEVICE_NAME_USER, rc));
        if (g_iModuleMajor > 0)
            unregister_chrdev(g_iModuleMajor, DEVICE_NAME);
        else
            misc_deregister(&g_MiscDevice);
        return rc;
    }

    return 0;
}


/**
 * Deregisters the device nodes.
 */
static void __exit vboxguestLinuxTermDeviceNodes(void)
{
    if (g_iModuleMajor > 0)
        unregister_chrdev(g_iModuleMajor, DEVICE_NAME);
    else
        misc_deregister(&g_MiscDevice);
    misc_deregister(&g_MiscDeviceUser);
}



/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init vboxguestLinuxModInit(void)
{
    int rc;

    /*
     * Initialize IPRT first.
     */
    rc = RTR0Init(0);
    if (RT_FAILURE(rc))
    {
        Log((DEVICE_NAME ": RTR0Init failed.\n"));
        return -EINVAL;
    }

    /*
     * Locate and initialize the PCI device.
     */
    rc = vboxguestLinuxInitPci();
    if (rc >= 0)
    {
        /*
         * Register the interrupt service routine for it.
         */
        rc = vboxguestLinuxInitISR();
        if (rc >= 0)
        {
            /*
             * Call the common device extension initializer.
             */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) && defined(RT_ARCH_X86)
            VBOXOSTYPE enmOSType = VBOXOSTYPE_Linux26;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) && defined(RT_ARCH_AMD64)
            VBOXOSTYPE enmOSType = VBOXOSTYPE_Linux26_x64;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0) && defined(RT_ARCH_X86)
            VBOXOSTYPE enmOSType = VBOXOSTYPE_Linux24;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0) && defined(RT_ARCH_AMD64)
            VBOXOSTYPE enmOSType = VBOXOSTYPE_Linux24_x64;
#else
# warning "huh? which arch + version is this?"
            VBOXOSTYPE enmOsType = VBOXOSTYPE_Linux;
#endif
            rc = VBoxGuestInitDevExt(&g_DevExt,
                                     g_IOPortBase,
                                     g_pvMMIOBase,
                                     g_cbMMIO,
                                     enmOSType);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Finally, create the device nodes.
                 */
                rc = vboxguestLinuxInitDeviceNodes();
                if (rc >= 0)
                {
                    /* some useful information for the user but don't show this on the console */
                    LogRel(("VirtualBox device settings: major %d, IRQ %d, I/O port 0x%x, MMIO at 0x%x (size 0x%x)\n",
                            g_iModuleMajor, g_pPciDev->irq, g_IOPortBase, g_MMIOPhysAddr, g_cbMMIO));
                    printk(KERN_DEBUG DEVICE_NAME ": Successfully loaded version "
                            VBOX_VERSION_STRING " (interface " xstr(VMMDEV_VERSION) ")\n");
                    return rc;
                }

                /* bail out */
                VBoxGuestDeleteDevExt(&g_DevExt);
            }
            else
            {
                LogRel(( DEVICE_NAME ": VBoxGuestInitDevExt failed with rc=%Rrc\n", rc));
                rc = RTErrConvertFromErrno(rc);
            }
            vboxguestLinuxTermISR();
        }
        vboxguestLinuxTermPci();
    }
    RTR0Term();
    return rc;
}


/**
 * Unload the module.
 */
static void __exit vboxguestLinuxModExit(void)
{
    /*
     * Inverse order of init.
     */
    vboxguestLinuxTermDeviceNodes();
    VBoxGuestDeleteDevExt(&g_DevExt);
    vboxguestLinuxTermISR();
    vboxguestLinuxTermPci();
    RTR0Term();
}


/**
 * Device open. Called on open /dev/vboxdrv
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int vboxguestLinuxOpen(struct inode *pInode, struct file *pFilp)
{
    int                 rc;
    PVBOXGUESTSESSION   pSession;
    Log((DEVICE_NAME ": pFilp=%p pid=%d/%d %s\n", pFilp, RTProcSelf(), current->pid, current->comm));

    /*
     * Call common code to create the user session. Associate it with
     * the file so we can access it in the other methods.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
        pFilp->private_data = pSession;

    Log(("vboxguestLinuxOpen: g_DevExt=%p pSession=%p rc=%d/%d (pid=%d/%d %s)\n",
         &g_DevExt, pSession, rc, vboxguestLinuxConvertToNegErrno(rc),
         RTProcSelf(), current->pid, current->comm));
    return vboxguestLinuxConvertToNegErrno(rc);
}


/**
 * Close device.
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int vboxguestLinuxRelease(struct inode *pInode, struct file *pFilp)
{
    Log(("vboxguestLinuxRelease: pFilp=%p pSession=%p pid=%d/%d %s\n",
         pFilp, pFilp->private_data, RTProcSelf(), current->pid, current->comm));

    VBoxGuestCloseSession(&g_DevExt, (PVBOXGUESTSESSION)pFilp->private_data);
    pFilp->private_data = NULL;
    return 0;
}


/**
 * Device I/O Control entry point.
 *
 * @param   pFilp       Associated file pointer.
 * @param   uCmd        The function specified to ioctl().
 * @param   ulArg       The argument specified to ioctl().
 */
#ifdef HAVE_UNLOCKED_IOCTL
static long vboxguestLinuxIOCtl(struct file *pFilp, unsigned int uCmd, unsigned long ulArg)
#else
static int vboxguestLinuxIOCtl(struct inode *pInode, struct file *pFilp, unsigned int uCmd, unsigned long ulArg)
#endif
{
    PVBOXGUESTSESSION   pSession = (PVBOXGUESTSESSION)pFilp->private_data;
    uint32_t            cbData   = _IOC_SIZE(uCmd);
    void               *pvBufFree;
    void               *pvBuf;
    int                 rc;
    uint64_t            au64Buf[32/sizeof(uint64_t)];

    Log6(("vboxguestLinuxIOCtl: pFilp=%p uCmd=%#x ulArg=%p pid=%d/%d\n", pFilp, uCmd, (void *)ulArg, RTProcSelf(), current->pid));

    /*
     * Buffer the request.
     */
    if (cbData <= sizeof(au64Buf))
    {
        pvBufFree = NULL;
        pvBuf = &au64Buf[0];
    }
    else
    {
        pvBufFree = pvBuf = RTMemTmpAlloc(cbData);
        if (RT_UNLIKELY(!pvBuf))
        {
            LogRel((DEVICE_NAME "::IOCtl: RTMemTmpAlloc failed to alloc %u bytes.\n", cbData));
            return -ENOMEM;
        }
    }
    if (RT_LIKELY(copy_from_user(pvBuf, (void *)ulArg, cbData)))
    {
        /*
         * Process the IOCtl.
         */
        size_t cbDataReturned;
        rc = VBoxGuestCommonIOCtl(uCmd, &g_DevExt, pSession, pvBuf, cbData, &cbDataReturned);

        /*
         * Copy ioctl data and output buffer back to user space.
         */
        if (RT_LIKELY(!rc))
        {
            rc = 0;
            if (RT_UNLIKELY(cbDataReturned > cbData))
            {
                LogRel((DEVICE_NAME "::IOCtl: too much output data %u expected %u\n", cbDataReturned, cbData));
                cbDataReturned = cbData;
            }
            if (cbDataReturned > 0)
            {
                if (RT_UNLIKELY(copy_to_user((void *)ulArg, pvBuf, cbDataReturned)))
                {
                    LogRel((DEVICE_NAME "::IOCtl: copy_to_user failed; pvBuf=%p ulArg=%p cbDataReturned=%u uCmd=%d\n",
                            pvBuf, (void *)ulArg, cbDataReturned, uCmd, rc));
                    rc = -EFAULT;
                }
            }
        }
        else
        {
            Log(("vboxguestLinuxIOCtl: pFilp=%p uCmd=%#x ulArg=%p failed, rc=%d\n", pFilp, uCmd, (void *)ulArg, rc));
            rc = vboxguestLinuxConvertToNegErrno(rc);
        }
    }
    else
    {
        Log((DEVICE_NAME "::IOCtl: copy_from_user(,%#lx, %#x) failed; uCmd=%#x.\n", ulArg, cbData, uCmd));
        rc = -EFAULT;
    }
    if (pvBufFree)
        RTMemFree(pvBufFree);

    Log6(("vboxguestLinuxIOCtl: returns %d (pid=%d/%d)\n", rc, RTProcSelf(), current->pid));
    return rc;
}

/* Common code that depend on g_DevExt. */
#include "VBoxGuestIDC-unix.c.h"

EXPORT_SYMBOL(VBoxGuestIDCOpen);
EXPORT_SYMBOL(VBoxGuestIDCClose);
EXPORT_SYMBOL(VBoxGuestIDCCall);

module_init(vboxguestLinuxModInit);
module_exit(vboxguestLinuxModExit);

MODULE_AUTHOR("Sun Microsystems, Inc.");
MODULE_DESCRIPTION("VirtualBox Guest Additions for Linux Module");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (" xstr(SUPDRV_IOC_VERSION) ")");
#endif

