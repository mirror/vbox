/* $Rev$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Linux specifics.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include "../SUPDrvInternal.h"
#include "the-linux-kernel.h"
#include "version-generated.h"

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
#ifdef CONFIG_X86_LOCAL_APIC
# include <asm/apic.h>
# include <asm/nmi.h>
#endif
#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
# include <linux/platform_device.h>
#endif

#include <iprt/mem.h>


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

#ifdef CONFIG_X86_LOCAL_APIC

/* If an NMI occurs while we are inside the world switcher the machine will
 * crash. The Linux NMI watchdog generates periodic NMIs increasing a counter
 * which is compared with another counter increased in the timer interrupt
 * handler. We disable the NMI watchdog.
 *
 * - Linux >= 2.6.21: The watchdog is disabled by default on i386 and x86_64.
 * - Linux <  2.6.21: The watchdog is normally enabled by default on x86_64
 *                    and disabled on i386.
 */
# if defined(RT_ARCH_AMD64)
#  if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 21) && !defined(VBOX_REDHAT_KABI)
#   define DO_DISABLE_NMI 1
#  endif
# endif

# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
extern int nmi_active;
#  define nmi_atomic_read(P)    *(P)
#  define nmi_atomic_set(P, V)  *(P) = (V)
#  define nmi_atomic_dec(P)     nmi_atomic_set(P, 0)
# else
#  define nmi_atomic_read(P)    atomic_read(P)
#  define nmi_atomic_set(P, V)  atomic_set(P, V)
#  define nmi_atomic_dec(P)     atomic_dec(P)
# endif

# ifndef X86_FEATURE_ARCH_PERFMON
#  define X86_FEATURE_ARCH_PERFMON (3*32+9) /* Intel Architectural PerfMon */
# endif
# ifndef MSR_ARCH_PERFMON_EVENTSEL0
#  define MSR_ARCH_PERFMON_EVENTSEL0 0x186
# endif
# ifndef ARCH_PERFMON_UNHALTED_CORE_CYCLES_PRESENT
#  define ARCH_PERFMON_UNHALTED_CORE_CYCLES_PRESENT (1 << 0)
# endif

#endif /* CONFIG_X86_LOCAL_APIC */


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Device extention & session data association structure.
 */
static SUPDRVDEVEXT         g_DevExt;

#ifndef CONFIG_VBOXDRV_AS_MISC
/** Module major number */
#define DEVICE_MAJOR        234
/** Saved major device number */
static int                  g_iModuleMajor;
#endif /* !CONFIG_VBOXDRV_AS_MISC */

/** Module parameter.
 * Not prefixed because the name is used by macros and the end of this file. */
static int force_async_tsc = 0;

/** The module name. */
#define DEVICE_NAME         "vboxdrv"

#ifdef RT_ARCH_AMD64
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


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  VBoxDrvLinuxInit(void);
static void VBoxDrvLinuxUnload(void);
static int  VBoxDrvLinuxCreate(struct inode *pInode, struct file *pFilp);
static int  VBoxDrvLinuxClose(struct inode *pInode, struct file *pFilp);
#ifdef HAVE_UNLOCKED_IOCTL
static long VBoxDrvLinuxIOCtl(struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
#else
static int  VBoxDrvLinuxIOCtl(struct inode *pInode, struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
#endif
static int  VBoxDrvLinuxIOCtlSlow(struct file *pFilp, unsigned int uCmd, unsigned long ulArg);
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

/** The file_operations structure. */
static struct file_operations gFileOpsVBoxDrv =
{
    owner:      THIS_MODULE,
    open:       VBoxDrvLinuxCreate,
    release:    VBoxDrvLinuxClose,
#ifdef HAVE_UNLOCKED_IOCTL
    unlocked_ioctl: VBoxDrvLinuxIOCtl,
#else
    ioctl:      VBoxDrvLinuxIOCtl,
#endif
};

#ifdef CONFIG_VBOXDRV_AS_MISC
/** The miscdevice structure. */
static struct miscdevice gMiscDevice =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       DEVICE_NAME,
    fops:       &gFileOpsVBoxDrv,
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name: DEVICE_NAME,
# endif
};
#endif


#ifdef VBOX_WITH_SUSPEND_NOTIFICATION
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
static struct dev_pm_ops gPlatformPMOps =
{
    .suspend = VBoxDrvSuspend,
    .resume = VBoxDrvResume,
};
# endif

static struct platform_driver gPlatformDriver =
{
    .probe = VBoxDrvProbe,
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
    .suspend = VBoxDrvSuspend,
    .resume = VBoxDrvResume,
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


#ifdef CONFIG_X86_LOCAL_APIC
# ifdef DO_DISABLE_NMI
/** Stop AMD NMI watchdog (x86_64 only). */
static int vboxdrvStopK7Watchdog(void)
{
    wrmsr(MSR_K7_EVNTSEL0, 0, 0);
    return 1;
}

/** Stop Intel P4 NMI watchdog (x86_64 only). */
static int vboxdrvStopP4Watchdog(void)
{
    wrmsr(MSR_P4_IQ_CCCR0,  0, 0);
    wrmsr(MSR_P4_IQ_CCCR1,  0, 0);
    wrmsr(MSR_P4_CRU_ESCR0, 0, 0);
    return 1;
}

/** The new method of detecting the event counter */
static int vboxdrvStopIntelArchWatchdog(void)
{
    unsigned ebx;

    ebx = cpuid_ebx(10);
    if (!(ebx & ARCH_PERFMON_UNHALTED_CORE_CYCLES_PRESENT))
        wrmsr(MSR_ARCH_PERFMON_EVENTSEL0, 0, 0);
    return 1;
}

/** Stop NMI watchdog. */
static void vboxdrvStopApicNmiWatchdog(void *unused)
{
    int stopped = 0;

    /* only support LOCAL and IO APICs for now */
    if ((nmi_watchdog != NMI_LOCAL_APIC) &&
        (nmi_watchdog != NMI_IO_APIC))
        return;

    if (nmi_watchdog == NMI_LOCAL_APIC)
    {
        switch (boot_cpu_data.x86_vendor)
        {
        case X86_VENDOR_AMD:
            if (strstr(boot_cpu_data.x86_model_id, "Screwdriver"))
               return;
            stopped = vboxdrvStopK7Watchdog();
            break;
        case X86_VENDOR_INTEL:
            if (cpu_has(&boot_cpu_data, X86_FEATURE_ARCH_PERFMON))
            {
                stopped = vboxdrvStopIntelArchWatchdog();
                break;
            }
            stopped = vboxdrvStopP4Watchdog();
            break;
        default:
            return;
        }
    }

    if (stopped)
        nmi_atomic_dec(&nmi_active);
}

/** Disable LAPIC NMI watchdog. */
static void DisableLapicNmiWatchdog(void)
{
    BUG_ON(nmi_watchdog != NMI_LOCAL_APIC);

    if (nmi_atomic_read(&nmi_active) <= 0)
        return;

    on_each_cpu(vboxdrvStopApicNmiWatchdog, NULL, 1, 1);

    BUG_ON(nmi_atomic_read(&nmi_active) != 0);

    /* tell do_nmi() and others that we're not active any more */
    nmi_watchdog = NMI_NONE;
}

/** Shutdown NMI. */
static void vboxdrvNmiCpuShutdown(void * dummy)
{
    unsigned int vERR, vPC;

    vPC = apic_read(APIC_LVTPC);

    if ((GET_APIC_DELIVERY_MODE(vPC) == APIC_MODE_NMI) && !(vPC & APIC_LVT_MASKED))
    {
        vERR = apic_read(APIC_LVTERR);
        apic_write(APIC_LVTERR, vERR | APIC_LVT_MASKED);
        apic_write(APIC_LVTPC,  vPC  | APIC_LVT_MASKED);
        apic_write(APIC_LVTERR, vERR);
    }
}

static void vboxdrvNmiShutdown(void)
{
    on_each_cpu(vboxdrvNmiCpuShutdown, NULL, 0, 1);
}
# endif /* DO_DISABLE_NMI */
#endif /* CONFIG_X86_LOCAL_APIC */


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

    dprintf(("VBoxDrv::ModuleInit\n"));

#ifdef CONFIG_X86_LOCAL_APIC
    /*
     * If an NMI occurs while we are inside the world switcher the macine will crash.
     * The Linux NMI watchdog generates periodic NMIs increasing a counter which is
     * compared with another counter increased in the timer interrupt handler. Therefore
     * we don't allow to setup an NMI watchdog.
     */
# if !defined(VBOX_REDHAT_KABI)
    /*
     * First test: NMI actiated? Works only works with Linux 2.6 -- 2.4 does not export
     *             the nmi_watchdog variable.
     */
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) || defined CONFIG_X86_64
#   ifdef DO_DISABLE_NMI
    if (nmi_atomic_read(&nmi_active) > 0)
    {
        printk(KERN_DEBUG DEVICE_NAME ": Trying to deactivate the NMI watchdog...\n");

        switch (nmi_watchdog)
        {
            case NMI_LOCAL_APIC:
                DisableLapicNmiWatchdog();
                break;
            case NMI_NONE:
                nmi_atomic_dec(&nmi_active);
                break;
        }

        if (nmi_atomic_read(&nmi_active) == 0)
        {
            vboxdrvNmiShutdown();
            printk(KERN_DEBUG DEVICE_NAME ": Successfully done.\n");
        }
        else
            printk(KERN_DEBUG DEVICE_NAME ": Failed!\n");
    }
#   endif /* DO_DISABLE_NMI */

    /*
     * Permanent IO_APIC mode active? No way to handle this!
     */
    if (nmi_watchdog == NMI_IO_APIC)
    {
        printk(KERN_ERR DEVICE_NAME
               ": NMI watchdog in IO_APIC mode active -- refused to load the kernel module!\n"
                        DEVICE_NAME
               ": Please disable the NMI watchdog by specifying 'nmi_watchdog=0' at kernel\n"
                        DEVICE_NAME
               ": command line.\n");
        return -EINVAL;
    }

    /*
     * See arch/i386/kernel/nmi.c on >= 2.6.19: -1 means it can never enabled again
     */
    nmi_atomic_set(&nmi_active, -1);
    printk(KERN_DEBUG DEVICE_NAME ": Trying to deactivate the NMI watchdog permanently...\n");

    /*
     * Now fall through and see if it actually was enabled before. If so, fail
     * as we cannot deactivate it cleanly from here.
     */
#  else /* < 2.6.19 */
    /*
     * Older 2.6 kernels: nmi_watchdog is not initalized by default
     */
    if (nmi_watchdog != NMI_NONE)
        goto nmi_activated;
#  endif
# endif /* >= 2.6.0 && !defined(VBOX_REDHAT_KABI) */

    /*
     * Second test: Interrupt generated by performance counter not masked and can
     *              generate an NMI. Works also with Linux 2.4.
     */
    {
        unsigned int v, ver, maxlvt;

        v   = apic_read(APIC_LVR);
        ver = GET_APIC_VERSION(v);
        /* 82489DXs do not report # of LVT entries. */
        maxlvt = APIC_INTEGRATED(ver) ? GET_APIC_MAXLVT(v) : 2;
        if (maxlvt >= 4)
        {
            /* Read status of performance counter IRQ vector */
            v = apic_read(APIC_LVTPC);

            /* performance counter generates NMI and is not masked? */
            if ((GET_APIC_DELIVERY_MODE(v) == APIC_MODE_NMI) && !(v & APIC_LVT_MASKED))
            {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31) && defined(CONFIG_PERF_COUNTERS)
                /* 2.6.31+: The performance counter framework will initialize the LVTPC
                 * vector as NMI. We can't disable the framework but the kernel loader
                 * script will do 'echo 2 > /proc/sys/kernel/perf_counter_paranoid'
                 * which hopefilly prevents any usage of hardware performance counters
                 * and therefore triggering of NMIs. */
                printk(KERN_ERR DEVICE_NAME
                       ": Warning: 2.6.31+ kernel detected. Most likely the hwardware performance\n"
                                DEVICE_NAME
                       ": counter framework which can generate NMIs is active. You have to prevent\n"
                                DEVICE_NAME
                       ": the usage of hardware performance counters by\n"
                                DEVICE_NAME
                       ":   echo 2 > /proc/sys/kernel/perf_counter_paranoid\n");
                /* We can't do more here :-( */
                goto no_error;
# endif

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) || defined CONFIG_X86_64
                printk(KERN_ERR DEVICE_NAME
                ": NMI watchdog either active or at least initialized. Please disable the NMI\n"
                                DEVICE_NAME
                ": watchdog by specifying 'nmi_watchdog=0' at kernel command line.\n");
                return -EINVAL;
# else /* < 2.6.19 */
#  if !defined(VBOX_REDHAT_KABI)
nmi_activated:
#  endif
                printk(KERN_ERR DEVICE_NAME
                ": NMI watchdog active -- refused to load the kernel module! Please disable\n"
                                DEVICE_NAME
                ": the NMI watchdog by specifying 'nmi_watchdog=0' at kernel command line.\n");
                return -EINVAL;
# endif /* >= 2.6.19 */
            }
        }
    }
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
    printk(KERN_DEBUG DEVICE_NAME ": Successfully done.\n");
no_error:
# endif /* >= 2.6.19 */
#endif /* CONFIG_X86_LOCAL_APIC */

    /*
     * Check for synchronous/asynchronous TSC mode.
     */
    printk(KERN_DEBUG DEVICE_NAME ": Found %u processor cores.\n", (unsigned)RTMpGetOnlineCount());
#ifdef CONFIG_VBOXDRV_AS_MISC
    rc = misc_register(&gMiscDevice);
    if (rc)
    {
        printk(KERN_ERR DEVICE_NAME ": Can't register misc device! rc=%d\n", rc);
        return rc;
    }
#else  /* !CONFIG_VBOXDRV_AS_MISC */
    /*
     * Register character device.
     */
    g_iModuleMajor = DEVICE_MAJOR;
    rc = register_chrdev((dev_t)g_iModuleMajor, DEVICE_NAME, &gFileOpsVBoxDrv);
    if (rc < 0)
    {
        dprintf(("register_chrdev() failed with rc=%#x!\n", rc));
        return rc;
    }

    /*
     * Save returned module major number
     */
    if (DEVICE_MAJOR != 0)
        g_iModuleMajor = DEVICE_MAJOR;
    else
        g_iModuleMajor = rc;
    rc = 0;

# ifdef CONFIG_DEVFS_FS
    /*
     * Register a device entry
     */
    if (devfs_mk_cdev(MKDEV(DEVICE_MAJOR, 0), S_IFCHR | VBOX_DEV_FMASK, DEVICE_NAME) != 0)
    {
        dprintf(("devfs_register failed!\n"));
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
#ifdef RT_ARCH_AMD64
            rc = RTR0MemExecDonate(&g_abExecMemory[0], sizeof(g_abExecMemory));
            printk("VBoxDrv: dbg - g_abExecMemory=%p\n", (void *)&g_abExecMemory[0]);
#endif
            /*
             * Initialize the device extension.
             */
            if (RT_SUCCESS(rc))
                rc = supdrvInitDevExt(&g_DevExt);
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
                        printk(KERN_INFO DEVICE_NAME ": TSC mode is %s, kernel timer mode is "
#ifdef VBOX_HRTIMER
                               "'high-res'"
#else
                               "'normal'"
#endif
                               ".\n",
                               g_DevExt.pGip->u32Mode == SUPGIPMODE_SYNC_TSC ? "'synchronous'" : "'asynchronous'");
                        LogFlow(("VBoxDrv::ModuleInit returning %#x\n", rc));
                        printk(KERN_DEBUG DEVICE_NAME ": Successfully loaded version "
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
            RTR0Term();
        }
        else
            rc = -EINVAL;

        /*
         * Failed, cleanup and return the error code.
         */
#if defined(CONFIG_DEVFS_FS) && !defined(CONFIG_VBOXDRV_AS_MISC)
        devfs_remove(DEVICE_NAME);
#endif
    }
#ifdef CONFIG_VBOXDRV_AS_MISC
    misc_deregister(&gMiscDevice);
    dprintf(("VBoxDrv::ModuleInit returning %#x (minor:%d)\n", rc, gMiscDevice.minor));
#else
    unregister_chrdev(g_iModuleMajor, DEVICE_NAME);
    dprintf(("VBoxDrv::ModuleInit returning %#x (major:%d)\n", rc, g_iModuleMajor));
#endif
    return rc;
}


/**
 * Unload the module.
 */
static void __exit VBoxDrvLinuxUnload(void)
{
    int                 rc;
    dprintf(("VBoxDrvLinuxUnload\n"));
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
    rc = misc_deregister(&gMiscDevice);
    if (rc < 0)
    {
        dprintf(("misc_deregister failed with rc=%#x\n", rc));
    }
#else  /* !CONFIG_VBOXDRV_AS_MISC */
# ifdef CONFIG_DEVFS_FS
    /*
     * Unregister a device entry
     */
    devfs_remove(DEVICE_NAME);
# endif /* devfs */
    unregister_chrdev(g_iModuleMajor, DEVICE_NAME);
#endif /* !CONFIG_VBOXDRV_AS_MISC */

    /*
     * Destroy GIP, delete the device extension and terminate IPRT.
     */
    supdrvDeleteDevExt(&g_DevExt);
    RTR0Term();
}


/**
 * Device open. Called on open /dev/vboxdrv
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxDrvLinuxCreate(struct inode *pInode, struct file *pFilp)
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
    rc = supdrvCreateSession(&g_DevExt, true /* fUser */, (PSUPDRVSESSION *)&pSession);
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
    /*
     * Deal with the two high-speed IOCtl that takes it's arguments from
     * the session and iCmd, and only returns a VBox status code.
     */
#ifdef HAVE_UNLOCKED_IOCTL
    if (RT_LIKELY(   uCmd == SUP_IOCTL_FAST_DO_RAW_RUN
                  || uCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
                  || uCmd == SUP_IOCTL_FAST_DO_NOP))
        return supdrvIOCtlFast(uCmd, ulArg, &g_DevExt, (PSUPDRVSESSION)pFilp->private_data);
    return VBoxDrvLinuxIOCtlSlow(pFilp, uCmd, ulArg);

#else   /* !HAVE_UNLOCKED_IOCTL */

    int rc;
    unlock_kernel();
    if (RT_LIKELY(   uCmd == SUP_IOCTL_FAST_DO_RAW_RUN
                  || uCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
                  || uCmd == SUP_IOCTL_FAST_DO_NOP))
        rc = supdrvIOCtlFast(uCmd, ulArg, &g_DevExt, (PSUPDRVSESSION)pFilp->private_data);
    else
        rc = VBoxDrvLinuxIOCtlSlow(pFilp, uCmd, ulArg);
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
 */
static int VBoxDrvLinuxIOCtlSlow(struct file *pFilp, unsigned int uCmd, unsigned long ulArg)
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
    rc = supdrvIOCtl(uCmd, &g_DevExt, (PSUPDRVSESSION)pFilp->private_data, pHdr);

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


/**
 * Check if the host kernel supports VT-x or not.
 *
 * Older Linux kernels clear the VMXE bit in the CR4 register (function
 * tlb_flush_all()) leading to a host kernel panic.
 *
 * @returns VBox error code
 */
int VBOXCALL supdrvOSQueryVTxSupport(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
    return VINF_SUCCESS;
#else
    return VERR_SUPDRV_KERNEL_TOO_OLD_FOR_VTX;
#endif
}


/**
 * Converts a supdrv error code to an linux error code.
 *
 * @returns corresponding linux error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int VBoxDrvLinuxErr2LinuxErr(int rc)
{
    switch (rc)
    {
        case 0:                             return 0;
        case SUPDRV_ERR_GENERAL_FAILURE:    return -EACCES;
        case SUPDRV_ERR_INVALID_PARAM:      return -EINVAL;
        case SUPDRV_ERR_INVALID_MAGIC:      return -EILSEQ;
        case SUPDRV_ERR_INVALID_HANDLE:     return -ENXIO;
        case SUPDRV_ERR_INVALID_POINTER:    return -EFAULT;
        case SUPDRV_ERR_LOCK_FAILED:        return -ENOLCK;
        case SUPDRV_ERR_ALREADY_LOADED:     return -EEXIST;
        case SUPDRV_ERR_PERMISSION_DENIED:  return -EPERM;
        case SUPDRV_ERR_VERSION_MISMATCH:   return -ENOSYS;
        case SUPDRV_ERR_IDT_FAILED:         return -1000;
    }

    return -EPERM;
}


RTDECL(int) SUPR0Printf(const char *pszFormat, ...)
{
#if 1
    va_list args;
    char    szMsg[512];

    va_start(args, pszFormat);
    vsnprintf(szMsg, sizeof(szMsg) - 1, pszFormat, args);
    szMsg[sizeof(szMsg) - 1] = '\0';
    printk("%s", szMsg);
    va_end(args);
#else
    /* forward to printf - needs some more GCC hacking to fix ebp... */
    __asm__ __volatile__ ("mov %0, %esp\n\t"
                          "jmp %1\n\t",
                          :: "r" ((uintptr_t)&pszFormat - 4),
                             "m" (printk));
#endif
    return 0;
}

module_init(VBoxDrvLinuxInit);
module_exit(VBoxDrvLinuxUnload);

MODULE_AUTHOR("Sun Microsystems, Inc.");
MODULE_DESCRIPTION("VirtualBox Support Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (" RT_XSTR(SUPDRV_IOC_VERSION) ")");
#endif

module_param(force_async_tsc, int, 0444);
MODULE_PARM_DESC(force_async_tsc, "force the asynchronous TSC mode");

