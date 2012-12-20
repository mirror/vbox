/* $Rev$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Linux specifics.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "../SUPDrvInternal.h"
#include "the-linux-kernel.h"
#include "version-generated.h"
#include "product-generated.h"

#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <VBox/err.h>
#include <iprt/mem.h>
#include <VBox/log.h>
#include <iprt/mp.h>

/** @todo figure out the exact version number */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
# include <iprt/power.h>
# define VBOX_WITH_SUSPEND_NOTIFICATION
#endif

#include <linux/sched.h>
#ifdef CONFIG_DEVFS_FS
# include <linux/devfs_fs_kernel.h>
#endif
#ifdef CONFIG_VBOXDRV_AS_MISC
# include <linux/miscdevice.h>
#endif
#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
# include <linux/platform_device.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* check kernel version */
# ifndef SUPDRV_AGNOSTIC
#  if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#   error Unsupported kernel version!
#  endif
# endif

/* devfs defines */
#if defined(CONFIG_DEVFS_FS) && !defined(CONFIG_VBOXDRV_AS_MISC)
# ifdef VBOX_WITH_HARDENING
#  define VBOX_DEV_FMASK     (S_IWUSR | S_IRUSR)
# else
#  define VBOX_DEV_FMASK     (S_IRUGO | S_IWUGO)
# endif
#endif /* CONFIG_DEV_FS && !CONFIG_VBOXDEV_AS_MISC */

#ifdef CONFIG_X86_HIGH_ENTRY
# error "CONFIG_X86_HIGH_ENTRY is not supported by VBoxDrv at this time."
#endif

/* to include the version number of VirtualBox into kernel backtraces */
#define VBoxDrvLinuxVersion RT_CONCAT3(RT_CONCAT(VBOX_VERSION_MAJOR, _), \
                                       RT_CONCAT(VBOX_VERSION_MINOR, _), \
                                       VBOX_VERSION_BUILD)
#define VBoxDrvLinuxIOCtl RT_CONCAT(VBoxDrvLinuxIOCtl_,VBoxDrvLinuxVersion)

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  VBoxDrvLinuxInit(void);
static void VBoxDrvLinuxUnload(void);
static int  VBoxDrvLinuxCreateSys(struct inode *pInode, struct file *pFilp);
static int  VBoxDrvLinuxCreateUsr(struct inode *pInode, struct file *pFilp);
static int  VBoxDrvLinuxClose(struct inode *pInode, struct file *pFilp);
#ifdef HAVE_UNLOCKED_IOCTL
static long VBoxDrvLinuxIOCtl(struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
#else
static int  VBoxDrvLinuxIOCtl(struct inode *pInode, struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
#endif
static int  VBoxDrvLinuxIOCtlSlow(struct file *pFilp, unsigned int uCmd, unsigned long ulArg, PSUPDRVSESSION pSession);
static int  VBoxDrvLinuxErr2LinuxErr(int);
#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
static int  VBoxDrvProbe(struct platform_device *pDev);
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
static int  VBoxDrvSuspend(struct device *pDev);
static int  VBoxDrvResume(struct device *pDev);
# else
static int  VBoxDrvSuspend(struct platform_device *pDev, pm_message_t State);
static int  VBoxDrvResume(struct platform_device *pDev);
# endif
static void VBoxDevRelease(struct device *pDev);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Device extention & session data association structure.
 */
static SUPDRVDEVEXT         g_DevExt;

#ifndef CONFIG_VBOXDRV_AS_MISC
/** Module major number for vboxdrv. */
#define DEVICE_MAJOR_SYS    234
/** Saved major device number for vboxdrv. */
static int                  g_iModuleMajorSys;
/** Module major number for vboxdrvu. */
#define DEVICE_MAJOR_USR    235
/** Saved major device number for vboxdrvu. */
static int                  g_iModuleMajorUsr;
#endif /* !CONFIG_VBOXDRV_AS_MISC */

/** Module parameter.
 * Not prefixed because the name is used by macros and the end of this file. */
static int force_async_tsc = 0;

/** The system device name. */
#define DEVICE_NAME_SYS     "vboxdrvu"
/** The user device name. */
#define DEVICE_NAME_USR     "vboxdrv"

#if defined(RT_ARCH_AMD64) && !defined(CONFIG_DEBUG_SET_MODULE_RONX)
/**
 * Memory for the executable memory heap (in IPRT).
 */
extern uint8_t g_abExecMemory[1572864]; /* 1.5 MB */
__asm__(".section execmemory, \"awx\", @progbits\n\t"
        ".align 32\n\t"
        ".globl g_abExecMemory\n"
        "g_abExecMemory:\n\t"
        ".zero 1572864\n\t"
        ".type g_abExecMemory, @object\n\t"
        ".size g_abExecMemory, 1572864\n\t"
        ".text\n\t");
#endif

/** The file_operations structure. */
static struct file_operations gFileOpsVBoxDrvSys =
{
    owner:      THIS_MODULE,
    open:       VBoxDrvLinuxCreateSys,
    release:    VBoxDrvLinuxClose,
#ifdef HAVE_UNLOCKED_IOCTL
    unlocked_ioctl: VBoxDrvLinuxIOCtl,
#else
    ioctl:      VBoxDrvLinuxIOCtl,
#endif
};

/** The file_operations structure. */
static struct file_operations gFileOpsVBoxDrvUsr =
{
    owner:      THIS_MODULE,
    open:       VBoxDrvLinuxCreateUsr,
    release:    VBoxDrvLinuxClose,
#ifdef HAVE_UNLOCKED_IOCTL
    unlocked_ioctl: VBoxDrvLinuxIOCtl,
#else
    ioctl:      VBoxDrvLinuxIOCtl,
#endif
};

#ifdef CONFIG_VBOXDRV_AS_MISC
/** The miscdevice structure for vboxdrv. */
static struct miscdevice gMiscDeviceSys =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       DEVICE_NAME_SYS,
    fops:       &gFileOpsVBoxDrvSys,
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name: DEVICE_NAME_SYS,
# endif
};
/** The miscdevice structure for vboxdrvu. */
static struct miscdevice gMiscDeviceUsr =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       DEVICE_NAME_USR,
    fops:       &gFileOpsVBoxDrvUsr,
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name: DEVICE_NAME_USR,
# endif
};
#endif


#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
static struct dev_pm_ops gPlatformPMOps =
{
    .suspend = VBoxDrvSuspend,  /* before entering deep sleep */
    .resume  = VBoxDrvResume,   /* after wakeup from deep sleep */
    .freeze  = VBoxDrvSuspend,  /* before creating hibernation image */
    .restore = VBoxDrvResume,   /* after waking up from hibernation */
};
# endif

static struct platform_driver gPlatformDriver =
{
    .probe = VBoxDrvProbe,
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
    .suspend = VBoxDrvSuspend,
    .resume  = VBoxDrvResume,
# endif
    /** @todo .shutdown? */
    .driver =
    {
        .name = "vboxdrv",
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
        .pm = &gPlatformPMOps,
# endif
    }
};

static struct platform_device gPlatformDevice =
{
    .name = "vboxdrv",
    .dev =
    {
        .release = VBoxDevRelease
    }
};
#endif /* VBOX_WITH_SUSPEND_NOTIFICATION */


DECLINLINE(RTUID) vboxdrvLinuxUid(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
    return current->cred->uid;
#else
    return current->uid;
#endif
}

DECLINLINE(RTGID) vboxdrvLinuxGid(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
    return current->cred->gid;
#else
    return current->gid;
#endif
}

DECLINLINE(RTUID) vboxdrvLinuxEuid(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
    return current->cred->euid;
#else
    return current->euid;
#endif
}

/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init VBoxDrvLinuxInit(void)
{
    int       rc;

    /*
     * Check for synchronous/asynchronous TSC mode.
     */
    printk(KERN_DEBUG "vboxdrv: Found %u processor cores.\n", (unsigned)RTMpGetOnlineCount());
#ifdef CONFIG_VBOXDRV_AS_MISC
    rc = misc_register(&gMiscDeviceSys);
    if (rc)
    {
        printk(KERN_ERR "vboxdrv: Can't register system misc device! rc=%d\n", rc);
        return rc;
    }
    rc = misc_register(&gMiscDeviceUsr);
    if (rc)
    {
        printk(KERN_ERR "vboxdrv: Can't register user misc device! rc=%d\n", rc);
        misc_deregister(&gMiscDeviceSys);
        return rc;
    }
#else  /* !CONFIG_VBOXDRV_AS_MISC */
    /*
     * Register character devices and save the returned major numbers.
     */
    /* /dev/vboxdrv */
    g_iModuleMajorSys = DEVICE_MAJOR_SYS;
    rc = register_chrdev((dev_t)g_iModuleMajorSys, DEVICE_NAME_SYS, &gFileOpsVBoxDrvSys);
    if (rc < 0)
    {
        Log(("register_chrdev() failed with rc=%#x for vboxdrv!\n", rc));
        return rc;
    }
    if (DEVICE_MAJOR_SYS != 0)
        g_iModuleMajorSys = DEVICE_MAJOR_SYS;
    else
        g_iModuleMajorSys = rc;

    /* /dev/vboxdrvu */
    /** @todo Use a minor number of this bugger (not sure if this code is used
     *        though, so not bothering right now.) */
    g_iModuleMajorUsr = DEVICE_MAJOR_USR;
    rc = register_chrdev((dev_t)g_iModuleMajorUsr, DEVICE_NAME_USR, &gFileOpsVBoxDrvUsr);
    if (rc < 0)
    {
        Log(("register_chrdev() failed with rc=%#x for vboxdrv!\n", rc));
        return rc;
    }
    if (DEVICE_MAJOR_USR != 0)
        g_iModuleMajorUsr = DEVICE_MAJOR_USR;
    else
        g_iModuleMajorUsr = rc;
    rc = 0;

# ifdef CONFIG_DEVFS_FS
    /*
     * Register a device entry
     */
    if (   devfs_mk_cdev(MKDEV(DEVICE_MAJOR_SYS, 0), S_IFCHR | VBOX_DEV_FMASK, DEVICE_NAME_SYS) != 0
        || devfs_mk_cdev(MKDEV(DEVICE_MAJOR_USR, 0), S_IFCHR | VBOX_DEV_FMASK, DEVICE_NAME_USR) != 0)
    {
        Log(("devfs_register failed!\n"));
        rc = -EINVAL;
    }
# endif
#endif /* !CONFIG_VBOXDRV_AS_MISC */
    if (!rc)
    {
        /*
         * Initialize the runtime.
         * On AMD64 we'll have to donate the high rwx memory block to the exec allocator.
         */
        rc = RTR0Init(0);
        if (RT_SUCCESS(rc))
        {
#if defined(RT_ARCH_AMD64) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
            rc = RTR0MemExecDonate(&g_abExecMemory[0], sizeof(g_abExecMemory));
            printk(KERN_DEBUG "VBoxDrv: dbg - g_abExecMemory=%p\n", (void *)&g_abExecMemory[0]);
#endif
            Log(("VBoxDrv::ModuleInit\n"));

            /*
             * Initialize the device extension.
             */
            if (RT_SUCCESS(rc))
                rc = supdrvInitDevExt(&g_DevExt, sizeof(SUPDRVSESSION));
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
                rc = platform_driver_register(&gPlatformDriver);
                if (rc == 0)
                {
                    rc = platform_device_register(&gPlatformDevice);
                    if (rc == 0)
#endif
                    {
                        printk(KERN_INFO "vboxdrv: TSC mode is %s, kernel timer mode is 'normal'.\n",
                               g_DevExt.pGip->u32Mode == SUPGIPMODE_SYNC_TSC ? "'synchronous'" : "'asynchronous'");
                        LogFlow(("VBoxDrv::ModuleInit returning %#x\n", rc));
                        printk(KERN_DEBUG "vboxdrv: Successfully loaded version "
                                VBOX_VERSION_STRING " (interface " RT_XSTR(SUPDRV_IOC_VERSION) ").\n");
                        return rc;
                    }
#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
                    else
                        platform_driver_unregister(&gPlatformDriver);
                }
#endif
            }

            rc = -EINVAL;
            RTR0TermForced();
        }
        else
            rc = -EINVAL;

        /*
         * Failed, cleanup and return the error code.
         */
#if defined(CONFIG_DEVFS_FS) && !defined(CONFIG_VBOXDRV_AS_MISC)
        devfs_remove(DEVICE_NAME_SYS);
        devfs_remove(DEVICE_NAME_USR);
#endif
    }
#ifdef CONFIG_VBOXDRV_AS_MISC
    misc_deregister(&gMiscDeviceSys);
    misc_deregister(&gMiscDeviceUsr);
    Log(("VBoxDrv::ModuleInit returning %#x (minor:%d & %d)\n", rc, gMiscDeviceSys.minor, gMiscDeviceUsr.minor));
#else
    unregister_chrdev(g_iModuleMajorUsr, DEVICE_NAME_USR);
    unregister_chrdev(g_iModuleMajorSys, DEVICE_NAME_SYS);
    Log(("VBoxDrv::ModuleInit returning %#x (major:%d & %d)\n", rc, g_iModuleMajorSys, g_iModuleMajorUsr));
#endif
    return rc;
}


/**
 * Unload the module.
 */
static void __exit VBoxDrvLinuxUnload(void)
{
    int                 rc;
    Log(("VBoxDrvLinuxUnload\n"));
    NOREF(rc);

#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
    platform_device_unregister(&gPlatformDevice);
    platform_driver_unregister(&gPlatformDriver);
#endif

    /*
     * I Don't think it's possible to unload a driver which processes have
     * opened, at least we'll blindly assume that here.
     */
#ifdef CONFIG_VBOXDRV_AS_MISC
    rc = misc_deregister(&gMiscDeviceUsr);
    if (rc < 0)
    {
        Log(("misc_deregister failed with rc=%#x on vboxdrvu\n", rc));
    }
    rc = misc_deregister(&gMiscDeviceSys);
    if (rc < 0)
    {
        Log(("misc_deregister failed with rc=%#x on vboxdrv\n", rc));
    }
#else  /* !CONFIG_VBOXDRV_AS_MISC */
# ifdef CONFIG_DEVFS_FS
    /*
     * Unregister a device entry
     */
    devfs_remove(DEVICE_NAME_USR);
    devfs_remove(DEVICE_NAME_SYS);
# endif /* devfs */
    unregister_chrdev(g_iModuleMajorUsr, DEVICE_NAME_USR);
    unregister_chrdev(g_iModuleMajorSys, DEVICE_NAME_SYS);
#endif /* !CONFIG_VBOXDRV_AS_MISC */

    /*
     * Destroy GIP, delete the device extension and terminate IPRT.
     */
    supdrvDeleteDevExt(&g_DevExt);
    RTR0TermForced();
}


/**
 * Common open code.
 *
 * @param   pInode          Pointer to inode info structure.
 * @param   pFilp           Associated file pointer.
 * @param   fUnrestricted   Indicates which device node which was opened.
 */
static int vboxdrvLinuxCreateCommon(struct inode *pInode, struct file *pFilp, bool fUnrestricted)
{
    int                 rc;
    PSUPDRVSESSION      pSession;
    Log(("VBoxDrvLinuxCreate: pFilp=%p pid=%d/%d %s\n", pFilp, RTProcSelf(), current->pid, current->comm));

#ifdef VBOX_WITH_HARDENING
    /*
     * Only root is allowed to access the device, enforce it!
     */
    if (vboxdrvLinuxEuid() != 0 /* root */ )
    {
        Log(("VBoxDrvLinuxCreate: euid=%d, expected 0 (root)\n", vboxdrvLinuxEuid()));
        return -EPERM;
    }
#endif /* VBOX_WITH_HARDENING */

    /*
     * Call common code for the rest.
     */
    rc = supdrvCreateSession(&g_DevExt, true /* fUser */, fUnrestricted, &pSession);
    if (!rc)
    {
        pSession->Uid = vboxdrvLinuxUid();
        pSession->Gid = vboxdrvLinuxGid();
    }

    pFilp->private_data = pSession;

    Log(("VBoxDrvLinuxCreate: g_DevExt=%p pSession=%p rc=%d/%d (pid=%d/%d %s)\n",
         &g_DevExt, pSession, rc, VBoxDrvLinuxErr2LinuxErr(rc),
         RTProcSelf(), current->pid, current->comm));
    return VBoxDrvLinuxErr2LinuxErr(rc);
}


/** /dev/vboxdrv.  */
static int VBoxDrvLinuxCreateSys(struct inode *pInode, struct file *pFilp)
{
    return vboxdrvLinuxCreateCommon(pInode, pFilp, true);
}


/** /dev/vboxdrvu.  */
static int VBoxDrvLinuxCreateUsr(struct inode *pInode, struct file *pFilp)
{
    return vboxdrvLinuxCreateCommon(pInode, pFilp, false);
}


/**
 * Close device.
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxDrvLinuxClose(struct inode *pInode, struct file *pFilp)
{
    Log(("VBoxDrvLinuxClose: pFilp=%p pSession=%p pid=%d/%d %s\n",
         pFilp, pFilp->private_data, RTProcSelf(), current->pid, current->comm));
    supdrvCloseSession(&g_DevExt, (PSUPDRVSESSION)pFilp->private_data);
    pFilp->private_data = NULL;
    return 0;
}


#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
/**
 * Dummy device release function. We have to provide this function,
 * otherwise the kernel will complain.
 *
 * @param   pDev        Pointer to the platform device.
 */
static void VBoxDevRelease(struct device *pDev)
{
}

/**
 * Dummy probe function.
 *
 * @param   pDev        Pointer to the platform device.
 */
static int VBoxDrvProbe(struct platform_device *pDev)
{
    return 0;
}

/**
 * Suspend callback.
 * @param   pDev        Pointer to the platform device.
 * @param   State       message type, see Documentation/power/devices.txt.
 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
static int VBoxDrvSuspend(struct device *pDev)
# else
static int VBoxDrvSuspend(struct platform_device *pDev, pm_message_t State)
# endif
{
    RTPowerSignalEvent(RTPOWEREVENT_SUSPEND);
    return 0;
}

/**
 * Resume callback.
 *
 * @param   pDev        Pointer to the platform device.
 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
static int VBoxDrvResume(struct device *pDev)
# else
static int VBoxDrvResume(struct platform_device *pDev)
# endif
{
    RTPowerSignalEvent(RTPOWEREVENT_RESUME);
    return 0;
}
#endif /* VBOX_WITH_SUSPEND_NOTIFICATION */


/**
 * Device I/O Control entry point.
 *
 * @param   pFilp       Associated file pointer.
 * @param   uCmd        The function specified to ioctl().
 * @param   ulArg       The argument specified to ioctl().
 */
#ifdef HAVE_UNLOCKED_IOCTL
static long VBoxDrvLinuxIOCtl(struct file *pFilp, unsigned int uCmd, unsigned long ulArg)
#else
static int VBoxDrvLinuxIOCtl(struct inode *pInode, struct file *pFilp, unsigned int uCmd, unsigned long ulArg)
#endif
{
    PSUPDRVSESSION pSession = (PSUPDRVSESSION)pFilp->private_data;

    /*
     * Deal with the two high-speed IOCtl that takes it's arguments from
     * the session and iCmd, and only returns a VBox status code.
     */
#ifdef HAVE_UNLOCKED_IOCTL
    if (RT_LIKELY(   (   uCmd == SUP_IOCTL_FAST_DO_RAW_RUN
                      || uCmd == SUP_IOCTL_FAST_DO_HM_RUN
                      || uCmd == SUP_IOCTL_FAST_DO_NOP)
                  && pSession->fUnrestricted == true))
        return supdrvIOCtlFast(uCmd, ulArg, &g_DevExt, pSession);
    return VBoxDrvLinuxIOCtlSlow(pFilp, uCmd, ulArg, pSession);

#else   /* !HAVE_UNLOCKED_IOCTL */

    int rc;
    unlock_kernel();
    if (RT_LIKELY(   (   uCmd == SUP_IOCTL_FAST_DO_RAW_RUN
                      || uCmd == SUP_IOCTL_FAST_DO_HM_RUN
                      || uCmd == SUP_IOCTL_FAST_DO_NOP)
                  && pSession->fUnrestricted == true))
        rc = supdrvIOCtlFast(uCmd, ulArg, &g_DevExt, pSession);
    else
        rc = VBoxDrvLinuxIOCtlSlow(pFilp, uCmd, ulArg, pSession);
    lock_kernel();
    return rc;
#endif  /* !HAVE_UNLOCKED_IOCTL */
}


/**
 * Device I/O Control entry point.
 *
 * @param   pFilp       Associated file pointer.
 * @param   uCmd        The function specified to ioctl().
 * @param   ulArg       The argument specified to ioctl().
 * @param   pSession    The session instance.
 */
static int VBoxDrvLinuxIOCtlSlow(struct file *pFilp, unsigned int uCmd, unsigned long ulArg, PSUPDRVSESSION pSession)
{
    int                 rc;
    SUPREQHDR           Hdr;
    PSUPREQHDR          pHdr;
    uint32_t            cbBuf;

    Log6(("VBoxDrvLinuxIOCtl: pFilp=%p uCmd=%#x ulArg=%p pid=%d/%d\n", pFilp, uCmd, (void *)ulArg, RTProcSelf(), current->pid));

    /*
     * Read the header.
     */
    if (RT_UNLIKELY(copy_from_user(&Hdr, (void *)ulArg, sizeof(Hdr))))
    {
        Log(("VBoxDrvLinuxIOCtl: copy_from_user(,%#lx,) failed; uCmd=%#x.\n", ulArg, uCmd));
        return -EFAULT;
    }
    if (RT_UNLIKELY((Hdr.fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
    {
        Log(("VBoxDrvLinuxIOCtl: bad header magic %#x; uCmd=%#x\n", Hdr.fFlags & SUPREQHDR_FLAGS_MAGIC_MASK, uCmd));
        return -EINVAL;
    }

    /*
     * Buffer the request.
     */
    cbBuf = RT_MAX(Hdr.cbIn, Hdr.cbOut);
    if (RT_UNLIKELY(cbBuf > _1M*16))
    {
        Log(("VBoxDrvLinuxIOCtl: too big cbBuf=%#x; uCmd=%#x\n", cbBuf, uCmd));
        return -E2BIG;
    }
    if (RT_UNLIKELY(cbBuf != _IOC_SIZE(uCmd) && _IOC_SIZE(uCmd)))
    {
        Log(("VBoxDrvLinuxIOCtl: bad ioctl cbBuf=%#x _IOC_SIZE=%#x; uCmd=%#x.\n", cbBuf, _IOC_SIZE(uCmd), uCmd));
        return -EINVAL;
    }
    pHdr = RTMemAlloc(cbBuf);
    if (RT_UNLIKELY(!pHdr))
    {
        OSDBGPRINT(("VBoxDrvLinuxIOCtl: failed to allocate buffer of %d bytes for uCmd=%#x.\n", cbBuf, uCmd));
        return -ENOMEM;
    }
    if (RT_UNLIKELY(copy_from_user(pHdr, (void *)ulArg, Hdr.cbIn)))
    {
        Log(("VBoxDrvLinuxIOCtl: copy_from_user(,%#lx, %#x) failed; uCmd=%#x.\n", ulArg, Hdr.cbIn, uCmd));
        RTMemFree(pHdr);
        return -EFAULT;
    }

    /*
     * Process the IOCtl.
     */
    rc = supdrvIOCtl(uCmd, &g_DevExt, pSession, pHdr);

    /*
     * Copy ioctl data and output buffer back to user space.
     */
    if (RT_LIKELY(!rc))
    {
        uint32_t cbOut = pHdr->cbOut;
        if (RT_UNLIKELY(cbOut > cbBuf))
        {
            OSDBGPRINT(("VBoxDrvLinuxIOCtl: too much output! %#x > %#x; uCmd=%#x!\n", cbOut, cbBuf, uCmd));
            cbOut = cbBuf;
        }
        if (RT_UNLIKELY(copy_to_user((void *)ulArg, pHdr, cbOut)))
        {
            /* this is really bad! */
            OSDBGPRINT(("VBoxDrvLinuxIOCtl: copy_to_user(%#lx,,%#x); uCmd=%#x!\n", ulArg, cbOut, uCmd));
            rc = -EFAULT;
        }
    }
    else
    {
        Log(("VBoxDrvLinuxIOCtl: pFilp=%p uCmd=%#x ulArg=%p failed, rc=%d\n", pFilp, uCmd, (void *)ulArg, rc));
        rc = -EINVAL;
    }
    RTMemFree(pHdr);

    Log6(("VBoxDrvLinuxIOCtl: returns %d (pid=%d/%d)\n", rc, RTProcSelf(), current->pid));
    return rc;
}


/**
 * The SUPDRV IDC entry point.
 *
 * @returns VBox status code, see supdrvIDC.
 * @param   iReq        The request code.
 * @param   pReq        The request.
 */
int VBOXCALL SUPDrvLinuxIDC(uint32_t uReq, PSUPDRVIDCREQHDR pReq)
{
    PSUPDRVSESSION  pSession;

    /*
     * Some quick validations.
     */
    if (RT_UNLIKELY(!VALID_PTR(pReq)))
        return VERR_INVALID_POINTER;

    pSession = pReq->pSession;
    if (pSession)
    {
        if (RT_UNLIKELY(!VALID_PTR(pSession)))
            return VERR_INVALID_PARAMETER;
        if (RT_UNLIKELY(pSession->pDevExt != &g_DevExt))
            return VERR_INVALID_PARAMETER;
    }
    else if (RT_UNLIKELY(uReq != SUPDRV_IDC_REQ_CONNECT))
        return VERR_INVALID_PARAMETER;

    /*
     * Do the job.
     */
    return supdrvIDC(uReq, &g_DevExt, pSession, pReq);
}

EXPORT_SYMBOL(SUPDrvLinuxIDC);


/**
 * Initializes any OS specific object creator fields.
 */
void VBOXCALL supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
{
    NOREF(pObj);
    NOREF(pSession);
}


/**
 * Checks if the session can access the object.
 *
 * @returns true if a decision has been made.
 * @returns false if the default access policy should be applied.
 *
 * @param   pObj        The object in question.
 * @param   pSession    The session wanting to access the object.
 * @param   pszObjName  The object name, can be NULL.
 * @param   prc         Where to store the result when returning true.
 */
bool VBOXCALL supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}


bool VBOXCALL supdrvOSGetForcedAsyncTscMode(PSUPDRVDEVEXT pDevExt)
{
    return force_async_tsc != 0;
}


int  VBOXCALL   supdrvOSLdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const char *pszFilename)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pszFilename);
    return VERR_NOT_SUPPORTED;
}


void VBOXCALL   supdrvOSLdrNotifyOpened(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    NOREF(pDevExt); NOREF(pImage);
}


int  VBOXCALL   supdrvOSLdrValidatePointer(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, void *pv, const uint8_t *pbImageBits)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pv); NOREF(pbImageBits);
    return VERR_NOT_SUPPORTED;
}


int  VBOXCALL   supdrvOSLdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const uint8_t *pbImageBits, PSUPLDRLOAD pReq)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pbImageBits); NOREF(pReq);
    return VERR_NOT_SUPPORTED;
}


void VBOXCALL   supdrvOSLdrUnload(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    NOREF(pDevExt); NOREF(pImage);
}


/**
 * Converts a supdrv error code to an linux error code.
 *
 * @returns corresponding linux error code.
 * @param   rc          IPRT status code.
 */
static int VBoxDrvLinuxErr2LinuxErr(int rc)
{
    switch (rc)
    {
        case VINF_SUCCESS:              return 0;
        case VERR_GENERAL_FAILURE:      return -EACCES;
        case VERR_INVALID_PARAMETER:    return -EINVAL;
        case VERR_INVALID_MAGIC:        return -EILSEQ;
        case VERR_INVALID_HANDLE:       return -ENXIO;
        case VERR_INVALID_POINTER:      return -EFAULT;
        case VERR_LOCK_FAILED:          return -ENOLCK;
        case VERR_ALREADY_LOADED:       return -EEXIST;
        case VERR_PERMISSION_DENIED:    return -EPERM;
        case VERR_VERSION_MISMATCH:     return -ENOSYS;
        case VERR_IDT_FAILED:           return -1000;
    }

    return -EPERM;
}


RTDECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list va;
    char    szMsg[512];

    va_start(va, pszFormat);
    RTStrPrintfV(szMsg, sizeof(szMsg) - 1, pszFormat, va);
    va_end(va);
    szMsg[sizeof(szMsg) - 1] = '\0';

    printk("%s", szMsg);
    return 0;
}

module_init(VBoxDrvLinuxInit);
module_exit(VBoxDrvLinuxUnload);

MODULE_AUTHOR(VBOX_VENDOR);
MODULE_DESCRIPTION(VBOX_PRODUCT " Support Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (" RT_XSTR(SUPDRV_IOC_VERSION) ")");
#endif

module_param(force_async_tsc, int, 0444);
MODULE_PARM_DESC(force_async_tsc, "force the asynchronous TSC mode");

