/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Linux host:
 * Linux host driver code
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "SUPDRV.h"
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# include <linux/jiffies.h>
#endif
#include <asm/mman.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#ifdef CONFIG_DEVFS_FS
# include <linux/devfs_fs_kernel.h>
#endif
#ifdef CONFIG_VBOXDRV_AS_MISC
# include <linux/miscdevice.h>
#endif
#ifdef CONFIG_X86_LOCAL_APIC
# include <asm/apic.h>
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#  include <asm/nmi.h>
# endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) && !defined(page_to_pfn)
# define page_to_pfn(page) ((page) - mem_map)
#endif

/* devfs defines */
#if defined(CONFIG_DEVFS_FS) && !defined(CONFIG_VBOXDRV_AS_MISC)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)

#  define VBOX_REGISTER_DEVFS()                         \
({                                                      \
    void *rc = NULL;                                    \
    if (devfs_mk_cdev(MKDEV(DEVICE_MAJOR, 0),           \
                  S_IFCHR | S_IRUGO | S_IWUGO,          \
                  DEVICE_NAME) == 0)                    \
        rc = (void *)' '; /* return not NULL */         \
    rc;                                                 \
 })

#  define VBOX_UNREGISTER_DEVFS(handle)                 \
    devfs_remove(DEVICE_NAME);

# else /* < 2.6.0 */

#  define VBOX_REGISTER_DEVFS()                         \
    devfs_register(NULL, DEVICE_NAME, DEVFS_FL_DEFAULT, \
                   DEVICE_MAJOR, 0,                     \
                   S_IFCHR | S_IRUGO | S_IWUGO,         \
                   &gFileOpsVBoxDrv, NULL)

#  define VBOX_UNREGISTER_DEVFS(handle)                 \
    if (handle != NULL)                                 \
        devfs_unregister(handle)

# endif /* < 2.6.0 */
#endif /* CONFIG_DEV_FS && !CONFIG_VBOXDEV_AS_MISC */

#ifndef CONFIG_VBOXDRV_AS_MISC
# if defined(CONFIG_DEVFS_FS) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 0)
#  define VBOX_REGISTER_DEVICE(a,b,c)     devfs_register_chrdev(a,b,c)
#  define VBOX_UNREGISTER_DEVICE(a,b)     devfs_unregister_chrdev(a,b)
# else
#  define VBOX_REGISTER_DEVICE(a,b,c)     register_chrdev(a,b,c)
#  define VBOX_UNREGISTER_DEVICE(a,b)     unregister_chrdev(a,b)
# endif
#endif /* !CONFIG_VBOXDRV_AS_MISC */


#ifdef CONFIG_X86_HIGH_ENTRY
# error "CONFIG_X86_HIGH_ENTRY is not supported by VBoxDrv at this time."
#endif

/*
 * This sucks soooo badly on x86! Why don't they export __PAGE_KERNEL_EXEC so PAGE_KERNEL_EXEC would be usable?
 */
#if defined(__AMD64__)
# define MY_PAGE_KERNEL_EXEC    PAGE_KERNEL_EXEC
#elif defined(PAGE_KERNEL_EXEC) && defined(CONFIG_X86_PAE)
# define MY_PAGE_KERNEL_EXEC    __pgprot(cpu_has_pge ? _PAGE_KERNEL_EXEC | _PAGE_GLOBAL : _PAGE_KERNEL_EXEC)
#else
# define MY_PAGE_KERNEL_EXEC    PAGE_KERNEL
#endif

/*
 * The redhat hack section.
 *  - The current hacks are for 2.4.21-15.EL only.
 */
#ifndef NO_REDHAT_HACKS
/* accounting. */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  ifdef VM_ACCOUNT
#   define MY_DO_MUNMAP(a,b,c) do_munmap(a, b, c, 0) /* should it be 1 or 0? */
#  endif
# endif

/* backported remap_page_range. */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  include <asm/tlb.h>
#  ifdef tlb_vma /* probably not good enough... */
#   define HAVE_26_STYLE_REMAP_PAGE_RANGE 1
#  endif
# endif

# ifndef __AMD64__
/* In 2.6.9-22.ELsmp we have to call change_page_attr() twice when changing
 * the page attributes from PAGE_KERNEL to something else, because there appears
 * to be a bug in one of the many patches that redhat applied.
 * It should be safe to do this on less buggy linux kernels too. ;-)
 */
#  define MY_CHANGE_PAGE_ATTR(pPages, cPages, prot) \
    do { \
        if (pgprot_val(prot) != pgprot_val(PAGE_KERNEL)) \
            change_page_attr(pPages, cPages, prot); \
        change_page_attr(pPages, cPages, prot); \
    } while (0)
# endif
#endif /* !NO_REDHAT_HACKS */


#ifndef MY_DO_MUNMAP
# define MY_DO_MUNMAP(a,b,c) do_munmap(a, b, c)
#endif

#ifndef MY_CHANGE_PAGE_ATTR
# ifdef __AMD64__ /** @todo This is a cheap hack, but it'll get around that 'else BUG();' in __change_page_attr().  */
#  define MY_CHANGE_PAGE_ATTR(pPages, cPages, prot) \
    do { \
        change_page_attr(pPages, cPages, PAGE_KERNEL_NOCACHE); \
        change_page_attr(pPages, cPages, prot); \
    } while (0)
# else
#  define MY_CHANGE_PAGE_ATTR(pPages, cPages, prot) change_page_attr(pPages, cPages, prot)
# endif
#endif


/** @def ONE_MSEC_IN_JIFFIES
 * The number of jiffies that make up 1 millisecond. This is only actually used
 * when HZ is > 1000. */
#if HZ <= 1000
# define ONE_MSEC_IN_JIFFIES       0
#elif !(HZ % 1000)
# define ONE_MSEC_IN_JIFFIES       (HZ / 1000)
#else
# define ONE_MSEC_IN_JIFFIES       ((HZ + 999) / 1000)
# error "HZ is not a multiple of 1000, the GIP stuff won't work right!"
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/**
 * Device extention & session data association structure.
 */
static SUPDRVDEVEXT     g_DevExt;

/** Timer structure for the GIP update. */
static struct timer_list g_GipTimer;
/** Pointer to the page structure for the GIP. */
struct page            *g_pGipPage;

/** Registered devfs device handle. */
#if defined(CONFIG_DEVFS_FS) && !defined(CONFIG_VBOXDRV_AS_MISC)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static void            *g_hDevFsVBoxDrv = NULL;
# else
static devfs_handle_t   g_hDevFsVBoxDrv = NULL;
# endif
#endif

#ifndef CONFIG_VBOXDRV_AS_MISC
/** Module major number */
#define DEVICE_MAJOR   234
/** Saved major device number */
static int 	        g_iModuleMajor;
#endif /* !CONFIG_VBOXDRV_AS_MISC */

/** The module name. */
#define DEVICE_NAME    "vboxdrv"




/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int      VBoxSupDrvInit(void);
static void     VBoxSupDrvUnload(void);
static int      VBoxSupDrvCreate(struct inode *pInode, struct file *pFilp);
static int      VBoxSupDrvClose(struct inode *pInode, struct file *pFilp);
static int      VBoxSupDrvDeviceControl(struct inode *pInode, struct file *pFilp,
                                        unsigned int IOCmd, unsigned long IOArg);
static void    *VBoxSupDrvMapUser(struct page **papPages, unsigned cPages, unsigned fProt, pgprot_t pgFlags);
static int      VBoxSupDrvInitGip(PSUPDRVDEVEXT pDevExt);
static int      VBoxSupDrvTermGip(PSUPDRVDEVEXT pDevExt);
static void     VBoxSupGipTimer(unsigned long ulUser);
static int      VBoxSupDrvOrder(unsigned long size);
static int      VBoxSupDrvErr2LinuxErr(int);


/** The file_operations structure. */
static struct file_operations gFileOpsVBoxDrv =
{
	owner:		THIS_MODULE,
	open:		VBoxSupDrvCreate,
	release:	VBoxSupDrvClose,
	ioctl:		VBoxSupDrvDeviceControl,
};

#ifdef CONFIG_VBOXDRV_AS_MISC
/** The miscdevice structure. */
static struct miscdevice gMiscDevice =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       DEVICE_NAME,
    fops:       &gFileOpsVBoxDrv,
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) && \
     LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name: DEVICE_NAME,
# endif
};
#endif


/**
 * Initialize module.
 *
 * @returns appropritate status code.
 */
static int __init VBoxSupDrvInit(void)
{
    int                 rc;

    dprintf(("VBoxDrv::ModuleInit\n"));

#ifdef CONFIG_X86_LOCAL_APIC
    /*
     * If an NMI occurs while we are inside the world switcher the macine will crash.
     * The Linux NMI watchdog generates periodic NMIs increasing a counter which is
     * compared with another counter increased in the timer interrupt handler. Therefore
     * we don't allow to setup an NMI watchdog.
     */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    /*
     * First test: NMI actiated? Works only works with Linux 2.6 -- 2.4 does not export
     *             the nmi_watchdog variable.
     */
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
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
    atomic_set(&nmi_active, -1);
    printk(KERN_INFO DEVICE_NAME ": Trying to deactivate NMI watchdog permanently...\n");

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
# endif /* >= 2.6.0 */

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
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
                printk(KERN_ERR DEVICE_NAME
                ": NMI watchdog either active or at least initialized. Please disable the NMI\n"
                                DEVICE_NAME
                ": watchdog by specifying 'nmi_watchdog=0' at kernel command line.\n");
                return -EINVAL;
# else /* < 2.6.19 */
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
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
    printk(KERN_INFO DEVICE_NAME ": Successfully done.\n");
# endif /* >= 2.6.19 */
#endif /* CONFIG_X86_LOCAL_APIC */

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
    rc = VBOX_REGISTER_DEVICE((dev_t)g_iModuleMajor, DEVICE_NAME, &gFileOpsVBoxDrv);
    if (rc < 0)
    {
	dprintf(("VBOX_REGISTER_DEVICE failed with rc=%#x!\n", rc));
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

#ifdef CONFIG_DEVFS_FS
    /*
     * Register a device entry
     */
    g_hDevFsVBoxDrv = VBOX_REGISTER_DEVFS();
    if (g_hDevFsVBoxDrv == NULL)
    {
	dprintf(("devfs_register failed!\n"));
        rc = -EINVAL;
    }
#endif
#endif /* !CONFIG_VBOXDRV_AS_MISC */
    if (!rc)
    {
        /*
         * Initialize the device extension.
         */
        rc = supdrvInitDevExt(&g_DevExt);
        if (!rc)
        {
            /*
             * Create the GIP page.
             */
            rc = VBoxSupDrvInitGip(&g_DevExt);
            if (!rc)
            {
                dprintf(("VBoxDrv::ModuleInit returning %#x\n", rc));
                return rc;
            }
            supdrvDeleteDevExt(&g_DevExt);
        }
        else
            rc = -EINVAL;

        /*
         * Failed, cleanup and return the error code.
         */
#if defined(CONFIG_DEVFS_FS) && !defined(CONFIG_VBOXDRV_AS_MISC)
        VBOX_UNREGISTER_DEVFS(g_hDevFsVBoxDrv);
#endif
    }
#ifdef CONFIG_VBOXDRV_AS_MISC
    misc_deregister(&gMiscDevice);
    dprintf(("VBoxDrv::ModuleInit returning %#x (minor:%d)\n", rc, gMiscDevice.minor));
#else
    VBOX_UNREGISTER_DEVICE(g_iModuleMajor, DEVICE_NAME);
    dprintf(("VBoxDrv::ModuleInit returning %#x (major:%d)\n", rc, g_iModuleMajor));
#endif
    return rc;
}


/**
 * Unload the module.
 */
static void __exit VBoxSupDrvUnload(void)
{
    int                 rc;
    dprintf(("VBoxSupDrvUnload\n"));

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
#ifdef CONFIG_DEVFS_FS
    /*
     * Unregister a device entry
     */
    VBOX_UNREGISTER_DEVFS(g_hDevFsVBoxDrv);
#endif // devfs
    rc = VBOX_UNREGISTER_DEVICE(g_iModuleMajor, DEVICE_NAME);
    if (rc < 0)
    {
        dprintf(("unregister_chrdev failed with rc=%#x (major:%d)\n", rc, g_iModuleMajor));
    }
#endif /* !CONFIG_VBOXDRV_AS_MISC */

    /*
     * Destroy GIP and delete the device extension.
     */
    VBoxSupDrvTermGip(&g_DevExt);
    supdrvDeleteDevExt(&g_DevExt);
}


/**
 * Device open. Called on open /dev/vboxdrv
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxSupDrvCreate(struct inode *pInode, struct file *pFilp)
{
    int                 rc;
    PSUPDRVSESSION      pSession;
    dprintf(("VBoxSupDrvCreate: pFilp=%p\n", pFilp));

    /*
     * Call common code for the rest.
     */
    rc = supdrvCreateSession(&g_DevExt, (PSUPDRVSESSION *)&pSession);
    if (!rc)
    {
        pSession->Uid       = current->euid;
        pSession->Gid       = current->egid;
        pSession->Process   = (RTPROCESS)current->tgid;
    }

    dprintf(("VBoxSupDrvCreate: g_DevExt=%p pSession=%p rc=%d\n", &g_DevExt, pSession, rc));
    pFilp->private_data = pSession;

    return VBoxSupDrvErr2LinuxErr(rc);
}


/**
 * Close device.
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxSupDrvClose(struct inode *pInode, struct file *pFilp)
{
    dprintf(("VBoxSupDrvClose: pFilp=%p private_data=%p\n", pFilp, pFilp->private_data));
    supdrvCloseSession(&g_DevExt, (PSUPDRVSESSION)pFilp->private_data);
    pFilp->private_data = NULL;
    return 0;
}


/**
 * Device I/O Control entry point.
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 * @param   IOCmd       The function specified to ioctl().
 * @param   IOArg       The argument specified to ioctl().
 */
static int VBoxSupDrvDeviceControl(struct inode *pInode, struct file *pFilp,
                                   unsigned int IOCmd, unsigned long IOArg)
{
    int                 rc;
    SUPDRVIOCTLDATA	Args;
    void               *pvBuf = NULL;
    int                 cbBuf = 0;
    unsigned            cbOut = 0;

    dprintf2(("VBoxSupDrvDeviceControl: pFilp=%p IOCmd=%x IOArg=%p\n", pFilp, IOCmd, (void *)IOArg));

    /*
     * Copy ioctl data structure from user space.
     */
    if (_IOC_SIZE(IOCmd) != sizeof(SUPDRVIOCTLDATA))
    {
        dprintf(("VBoxSupDrvDeviceControl: incorrect input length! cbArgs=%d\n", _IOC_SIZE(IOCmd)));
        return -EINVAL;
    }
    if (copy_from_user(&Args, (void *)IOArg, _IOC_SIZE(IOCmd)))
    {
        dprintf(("VBoxSupDrvDeviceControl: copy_from_user(&Args) failed.\n"));
        return -EFAULT;
    }

    /*
     * Allocate and copy user space input data buffer to kernel space.
     */
    if (Args.cbIn > 0 || Args.cbOut > 0)
    {
        cbBuf = max(Args.cbIn, Args.cbOut);
        pvBuf = vmalloc(cbBuf);
        if (pvBuf == NULL)
        {
            dprintf(("VBoxSupDrvDeviceControl: failed to allocate buffer of %d bytes.\n", cbBuf));
            return -ENOMEM;
        }

        if (copy_from_user(pvBuf, (void *)Args.pvIn, Args.cbIn))
        {
            dprintf(("VBoxSupDrvDeviceControl: copy_from_user(pvBuf) failed.\n"));
            vfree(pvBuf);
            return -EFAULT;
        }
    }

    /*
     * Process the IOCtl.
     */
    rc = supdrvIOCtl(IOCmd, &g_DevExt, (PSUPDRVSESSION)pFilp->private_data,
                     pvBuf, Args.cbIn, pvBuf, Args.cbOut, &cbOut);

    /*
     * Copy ioctl data and output buffer back to user space.
     */
    if (rc)
    {
        dprintf(("VBoxSupDrvDeviceControl: pFilp=%p IOCmd=%x IOArg=%p failed, rc=%d (linux rc=%d)\n",
                 pFilp, IOCmd, (void *)IOArg, rc, VBoxSupDrvErr2LinuxErr(rc)));
        rc = VBoxSupDrvErr2LinuxErr(rc);
    }
    else if (cbOut > 0)
    {
        if (pvBuf != NULL && cbOut <= cbBuf)
        {
            if (copy_to_user((void *)Args.pvOut, pvBuf, cbOut))
            {
                dprintf(("copy_to_user failed.\n"));
                rc = -EFAULT;
            }
        }
        else
        {
            dprintf(("WHAT!?! supdrvIOCtl messed up! cbOut=%d cbBuf=%d pvBuf=%p\n", cbOut, cbBuf, pvBuf));
            rc = -EPERM;
        }
    }

    if (pvBuf)
        vfree(pvBuf);

    dprintf2(("VBoxSupDrvDeviceControl: returns %d\n", rc));
    return rc;
}


/**
 * Initializes any OS specific object creator fields.
 */
void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
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
bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}


/**
 * Compute order. Some functions allocate 2^order pages.
 *
 * @returns order.
 * @param   cPages      Number of pages.
 */
static int VBoxSupDrvOrder(unsigned long cPages)
{
    int             iOrder;
    unsigned long   cTmp;

    for (iOrder = 0, cTmp = cPages; cTmp >>= 1; ++iOrder)
        ;
    if (cPages & ~(1 << iOrder))
        ++iOrder;

    return iOrder;
}


/**
 * OS Specific code for locking down memory.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Pointer to memory.
 *                      This is not linked in anywhere.
 * @param   paPages     Array which should be filled with the address of the physical pages.
 *
 * @remark  See sgl_map_user_pages() for an example of an similar function.
 */
int  VBOXCALL   supdrvOSLockMemOne(PSUPDRVMEMREF pMem, PSUPPAGE paPages)
{
    int 	rc;
    struct page **papPages;
    unsigned    iPage;
    unsigned    cPages = pMem->cb >> PAGE_SHIFT;
    unsigned long pv = (unsigned long)pMem->pvR3;

    /*
     * Allocate page pointer array.
     */
    papPages = vmalloc(cPages * sizeof(*papPages));
    if (!papPages)
        return SUPDRV_ERR_NO_MEMORY;

    /*
     * Get user pages.
     */
    down_read(&current->mm->mmap_sem);
    rc = get_user_pages(current,                /* Task for fault acounting. */
                        current->mm,            /* Whose pages. */
                        (unsigned long)pv,      /* Where from. */
                        cPages,                 /* How many pages. */
                        1,                      /* Write to memory. */
                        0,                      /* force. */
                        papPages,               /* Page array. */
                        NULL);                  /* vmas */
    if (rc != cPages)
    {
        up_read(&current->mm->mmap_sem);
        dprintf(("supdrvOSLockMemOne: get_user_pages failed. rc=%d\n", rc));
        return SUPDRV_ERR_LOCK_FAILED;
    }

    for (iPage = 0; iPage < cPages; iPage++)
        flush_dcache_page(papPages[iPage]);
    up_read(&current->mm->mmap_sem);

    pMem->u.locked.papPages = papPages;
    pMem->u.locked.cPages = cPages;

    /*
     * Get addresses.
     */
    for (iPage = 0; iPage < cPages; iPage++)
    {
        paPages[iPage].Phys = page_to_phys(papPages[iPage]);
        paPages[iPage].uReserved = 0;
    }

    dprintf2(("supdrvOSLockMemOne: pvR3=%p cb=%d papPages=%p\n",
              pMem->pvR3, pMem->cb, pMem->u.locked.papPages));
    return 0;
}


/**
 * Unlocks the memory pointed to by pv.
 *
 * @param   pv  Memory to unlock.
 * @param   cb  Size of the memory (debug).
 *
 * @remark  See sgl_unmap_user_pages() for an example of an similar function.
 */
void VBOXCALL supdrvOSUnlockMemOne(PSUPDRVMEMREF pMem)
{
    unsigned    iPage;
    dprintf2(("supdrvOSUnlockMemOne: pvR3=%p cb=%d papPages=%p\n",
              pMem->pvR3, pMem->cb, pMem->u.locked.papPages));

    /*
     * Loop thru the pages and release them.
     */
    for (iPage = 0; iPage < pMem->u.locked.cPages; iPage++)
    {
        if (!PageReserved(pMem->u.locked.papPages[iPage]))
            SetPageDirty(pMem->u.locked.papPages[iPage]);
        page_cache_release(pMem->u.locked.papPages[iPage]);
    }

    /* free the page array */
    vfree(pMem->u.locked.papPages);
    pMem->u.locked.cPages = 0;
}


/**
 * OS Specific code for allocating page aligned memory with continuous fixed
 * physical paged backing.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Memory reference record of the memory to be allocated.
 *                      (This is not linked in anywhere.)
 * @param   ppvR0       Where to store the virtual address of the ring-0 mapping. (optional)
 * @param   ppvR3       Where to store the virtual address of the ring-3 mapping.
 * @param   pHCPhys     Where to store the physical address.
 */
int VBOXCALL supdrvOSContAllocOne(PSUPDRVMEMREF pMem, void **ppvR0, void **ppvR3, PRTHCPHYS pHCPhys)
{
    struct page *paPages;
    unsigned    iPage;
    unsigned    cbAligned = RT_ALIGN(pMem->cb, PAGE_SIZE);
    unsigned    cPages = cbAligned >> PAGE_SHIFT;
    unsigned    cOrder = VBoxSupDrvOrder(cPages);
    unsigned long ulAddr;
    dma_addr_t  HCPhys;
    int         rc = 0;
    pgprot_t    pgFlags;
    pgprot_val(pgFlags) = _PAGE_PRESENT | _PAGE_RW | _PAGE_USER;

    Assert(ppvR3);
    Assert(pHCPhys);

    /*
     * Allocate page pointer array.
     */
#ifdef __AMD64__ /** @todo check out if there is a correct way of getting memory below 4GB (physically). */
    paPages = alloc_pages(GFP_DMA, cOrder);
#else
    paPages = alloc_pages(GFP_USER, cOrder);
#endif
    if (!paPages)
        return SUPDRV_ERR_NO_MEMORY;

    /*
     * Lock the pages.
     */
    for (iPage = 0; iPage < cPages; iPage++)
    {
        SetPageReserved(&paPages[iPage]);
        if (!PageHighMem(&paPages[iPage]) && pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
            MY_CHANGE_PAGE_ATTR(&paPages[iPage], 1, MY_PAGE_KERNEL_EXEC);
#ifdef DEBUG
        if (iPage + 1 < cPages && (page_to_phys((&paPages[iPage])) + 0x1000) != page_to_phys((&paPages[iPage + 1])))
        {
            dprintf(("supdrvOSContAllocOne: Pages are not continuous!!!! iPage=%d phys=%llx physnext=%llx\n",
                     iPage, (long long)page_to_phys((&paPages[iPage])), (long long)page_to_phys((&paPages[iPage + 1]))));
            BUG();
        }
#endif
    }
    HCPhys = page_to_phys(paPages);

    /*
     * Allocate user space mapping and put the physical pages into it.
     */
    down_write(&current->mm->mmap_sem);
    ulAddr = do_mmap(NULL, 0, cbAligned, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANONYMOUS, 0);
    if (!(ulAddr & ~PAGE_MASK))
    {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) && !defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
        int rc2 = remap_page_range(ulAddr, HCPhys, cbAligned, pgFlags);
#else
        int rc2 = 0;
        struct vm_area_struct *vma = find_vma(current->mm, ulAddr);
        if (vma)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
            rc2 = remap_page_range(vma, ulAddr, HCPhys, cbAligned, pgFlags);
#else
            rc2 = remap_pfn_range(vma, ulAddr, HCPhys >> PAGE_SHIFT, cbAligned, pgFlags);
#endif
        else
        {
            rc = SUPDRV_ERR_NO_MEMORY;
            dprintf(("supdrvOSContAllocOne: no vma found for ulAddr=%#lx!\n", ulAddr));
        }
#endif
        if (rc2)
        {
            rc = SUPDRV_ERR_NO_MEMORY;
            dprintf(("supdrvOSContAllocOne: remap_page_range failed rc2=%d\n", rc2));
        }
    }
    else
    {
        dprintf(("supdrvOSContAllocOne: do_mmap failed ulAddr=%#lx\n", ulAddr));
        rc = SUPDRV_ERR_NO_MEMORY;
    }
    up_write(&current->mm->mmap_sem);   /* not quite sure when to give this up. */

    /*
     * Success?
     */
    if (!rc)
    {
        *pHCPhys = HCPhys;
        *ppvR3 = (void *)ulAddr;
        if (ppvR0)
            *ppvR0 = (void *)ulAddr;
        pMem->pvR3              = (void *)ulAddr;
        pMem->pvR0              = NULL;
        pMem->u.cont.paPages    = paPages;
        pMem->u.cont.cPages     = cPages;
        pMem->cb                = cbAligned;

        dprintf2(("supdrvOSContAllocOne: pvR0=%p pvR3=%p cb=%d paPages=%p *pHCPhys=%lx *ppvR0=*ppvR3=%p\n",
                  pMem->pvR0, pMem->pvR3, pMem->cb, paPages, (unsigned long)*pHCPhys, *ppvR3));
        global_flush_tlb();
        return 0;
    }

    /*
     * Failure, cleanup and be gone.
     */
    down_write(&current->mm->mmap_sem);
    if (ulAddr & ~PAGE_MASK)
        MY_DO_MUNMAP(current->mm, ulAddr, pMem->cb);
    for (iPage = 0; iPage < cPages; iPage++)
    {
        ClearPageReserved(&paPages[iPage]);
        if (!PageHighMem(&paPages[iPage]) && pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
            MY_CHANGE_PAGE_ATTR(&paPages[iPage], 1, PAGE_KERNEL);
    }
    up_write(&current->mm->mmap_sem);   /* check when we can leave this. */
    __free_pages(paPages, cOrder);

    global_flush_tlb();
    return rc;
}


/**
 * Frees contiguous memory.
 *
 * @param   pMem    Memory reference record of the memory to be freed.
 */
void VBOXCALL supdrvOSContFreeOne(PSUPDRVMEMREF pMem)
{
    unsigned    iPage;

    dprintf2(("supdrvOSContFreeOne: pvR0=%p pvR3=%p cb=%d paPages=%p\n",
              pMem->pvR0, pMem->pvR3, pMem->cb, pMem->u.cont.paPages));

    /*
     * do_exit() destroys the mm before closing files.
     * I really hope it cleans up our stuff properly...
     */
    if (current->mm)
    {
        down_write(&current->mm->mmap_sem);
        MY_DO_MUNMAP(current->mm, (unsigned long)pMem->pvR3, pMem->cb);
        up_write(&current->mm->mmap_sem);   /* check when we can leave this. */
    }

    /*
     * Change page attributes freeing the pages.
     */
    for (iPage = 0; iPage < pMem->u.cont.cPages; iPage++)
    {
        ClearPageReserved(&pMem->u.cont.paPages[iPage]);
        if (!PageHighMem(&pMem->u.cont.paPages[iPage]) && pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
            MY_CHANGE_PAGE_ATTR(&pMem->u.cont.paPages[iPage], 1, PAGE_KERNEL);
    }
    __free_pages(pMem->u.cont.paPages, VBoxSupDrvOrder(pMem->u.cont.cPages));

    pMem->u.cont.cPages = 0;
}


/**
 * Allocates memory which mapped into both kernel and user space.
 * The returned memory is page aligned and so is the allocation.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Memory reference record of the memory to be allocated.
 *                      (This is not linked in anywhere.)
 * @param   ppvR0       Where to store the address of the Ring-0 mapping.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 */
int  VBOXCALL   supdrvOSMemAllocOne(PSUPDRVMEMREF pMem, void **ppvR0, void **ppvR3)
{
    const unsigned  cbAligned = RT_ALIGN(pMem->cb, PAGE_SIZE);
    const unsigned  cPages = cbAligned >> PAGE_SHIFT;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 22)
    unsigned        cOrder = VBoxSupDrvOrder(cPages);
    struct page    *paPages;
#endif
    struct page   **papPages;
    unsigned        iPage;
    pgprot_t        pgFlags;
    pgprot_val(pgFlags) = _PAGE_PRESENT | _PAGE_RW | _PAGE_USER;

    /*
     * Allocate array with page pointers.
     */
    pMem->u.mem.cPages = 0;
    pMem->u.mem.papPages = papPages = kmalloc(sizeof(papPages[0]) * cPages, GFP_KERNEL);
    if (!papPages)
        return SUPDRV_ERR_NO_MEMORY;

    /*
     * Allocate the pages.
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
    for (iPage = 0; iPage < cPages; iPage++)
    {
        papPages[iPage] = alloc_page(GFP_HIGHUSER);
        if (!papPages[iPage])
        {
            pMem->u.mem.cPages = iPage;
            supdrvOSMemFreeOne(pMem);
            return SUPDRV_ERR_NO_MEMORY;
        }
    }

#else /* < 2.4.22 */
    paPages = alloc_pages(GFP_USER, cOrder);
    if (!paPages)
    {
        supdrvOSMemFreeOne(pMem);
        return SUPDRV_ERR_NO_MEMORY;
    }
    for (iPage = 0; iPage < cPages; iPage++)
    {
        papPages[iPage] = &paPages[iPage];
        if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
            MY_CHANGE_PAGE_ATTR(papPages[iPage], 1, MY_PAGE_KERNEL_EXEC);
        if (PageHighMem(papPages[iPage]))
            BUG();
    }
#endif
    pMem->u.mem.cPages = cPages;

    /*
     * Reserve the pages.
     */
    for (iPage = 0; iPage < cPages; iPage++)
        SetPageReserved(papPages[iPage]);

    /*
     * Create the Ring-0 mapping.
     */
    if (ppvR0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
# ifdef VM_MAP
        *ppvR0 = pMem->pvR0 = vmap(papPages, cPages, VM_MAP, pgFlags);
# else
        *ppvR0 = pMem->pvR0 = vmap(papPages, cPages, VM_ALLOC, pgFlags);
# endif
#else
        *ppvR0 = pMem->pvR0 = phys_to_virt(page_to_phys(papPages[0]));
#endif
    }
    if (pMem->pvR0 || !ppvR0)
    {
        /*
         * Create the ring3 mapping.
         */
        if (ppvR3)
            *ppvR3 = pMem->pvR3 = VBoxSupDrvMapUser(papPages, cPages, PROT_READ | PROT_WRITE | PROT_EXEC, pgFlags);
        if (pMem->pvR3 || !ppvR3)
            return 0;
        dprintf(("supdrvOSMemAllocOne: failed to map into r3! cPages=%u\n", cPages));
    }
    else
        dprintf(("supdrvOSMemAllocOne: failed to map into r0! cPages=%u\n", cPages));

    supdrvOSMemFreeOne(pMem);
    return SUPDRV_ERR_NO_MEMORY;
}


/**
 * Get the physical addresses of the pages in the allocation.
 * This is called while inside bundle the spinlock.
 *
 * @param   pMem        Memory reference record of the memory.
 * @param   paPages     Where to store the page addresses.
 */
void VBOXCALL   supdrvOSMemGetPages(PSUPDRVMEMREF pMem, PSUPPAGE paPages)
{
    unsigned iPage;
    for (iPage = 0; iPage < pMem->u.mem.cPages; iPage++)
    {
        paPages[iPage].Phys = page_to_phys(pMem->u.mem.papPages[iPage]);
        paPages[iPage].uReserved = 0;
    }
}


/**
 * Frees memory allocated by supdrvOSMemAllocOne().
 *
 * @param   pMem        Memory reference record of the memory to be free.
 */
void VBOXCALL   supdrvOSMemFreeOne(PSUPDRVMEMREF pMem)
{
    dprintf2(("supdrvOSMemFreeOne: pvR0=%p pvR3=%p cb=%d cPages=%d papPages=%p\n",
              pMem->pvR0, pMem->pvR3, pMem->cb, pMem->u.mem.cPages, pMem->u.mem.papPages));

    /*
     * Unmap the user mapping (if any).
     * do_exit() destroys the mm before closing files.
     */
    if (pMem->pvR3 && current->mm)
    {
        down_write(&current->mm->mmap_sem);
        MY_DO_MUNMAP(current->mm, (unsigned long)pMem->pvR3, RT_ALIGN(pMem->cb, PAGE_SIZE));
        up_write(&current->mm->mmap_sem);   /* check when we can leave this. */
    }
    pMem->pvR3 = NULL;

    /*
     * Unmap the kernel mapping (if any).
     */
    if (pMem->pvR0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
        vunmap(pMem->pvR0);
#endif
        pMem->pvR0 = NULL;
    }

    /*
     * Free the physical pages.
     */
    if (pMem->u.mem.papPages)
    {
        struct page   **papPages = pMem->u.mem.papPages;
        const unsigned  cPages   = pMem->u.mem.cPages;
        unsigned        iPage;

        /* Restore the page flags. */
        for (iPage = 0; iPage < cPages; iPage++)
        {
            ClearPageReserved(papPages[iPage]);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 22)
            if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
                MY_CHANGE_PAGE_ATTR(papPages[iPage], 1, PAGE_KERNEL);
#endif
        }

        /* Free the pages. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
        for (iPage = 0; iPage < pMem->u.cont.cPages; iPage++)
            __free_page(papPages[iPage]);
#else
        if (cPages > 0)
            __free_pages(papPages[0], VBoxSupDrvOrder(cPages));
#endif
        /* Free the page pointer array. */
        kfree(papPages);
        pMem->u.mem.papPages = NULL;
    }
    pMem->u.mem.cPages = 0;
}


/**
 * Maps a range of pages into user space.
 *
 * @returns Pointer to the user space mapping on success.
 * @returns NULL on failure.
 * @param   papPages    Array of the pages to map.
 * @param   cPages      Number of pages to map.
 * @param   fProt       The mapping protection.
 * @param   pgFlags     The page level protection.
 */
static void *VBoxSupDrvMapUser(struct page **papPages, unsigned cPages, unsigned fProt, pgprot_t pgFlags)
{
    int             rc = SUPDRV_ERR_NO_MEMORY;
    unsigned long   ulAddr;

    /*
     * Allocate user space mapping.
     */
    down_write(&current->mm->mmap_sem);
    ulAddr = do_mmap(NULL, 0, cPages * PAGE_SIZE, fProt, MAP_SHARED | MAP_ANONYMOUS, 0);
    if (!(ulAddr & ~PAGE_MASK))
    {
        /*
         * Map page by page into the mmap area.
         * This is generic, paranoid and not very efficient.
         */
        int             rc = 0;
        unsigned long   ulAddrCur = ulAddr;
        unsigned        iPage;
        for (iPage = 0; iPage < cPages; iPage++, ulAddrCur += PAGE_SIZE)
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
            struct vm_area_struct *vma = find_vma(current->mm, ulAddrCur);
            if (!vma)
                break;
#endif

#if   LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
            rc = remap_pfn_range(vma, ulAddrCur, page_to_pfn(papPages[iPage]), PAGE_SIZE, pgFlags);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
            rc = remap_page_range(vma, ulAddrCur, page_to_phys(papPages[iPage]), PAGE_SIZE, pgFlags);
#else /* 2.4 */
            rc = remap_page_range(ulAddrCur, page_to_phys(papPages[iPage]), PAGE_SIZE, pgFlags);
#endif
            if (rc)
                break;
        }

        /*
         * Successful?
         */
        if (iPage >= cPages)
        {
            up_write(&current->mm->mmap_sem);
            return (void *)ulAddr;
        }

        /* no, cleanup! */
        if (rc)
            dprintf(("VBoxSupDrvMapUser: remap_[page|pfn]_range failed! rc=%d\n", rc));
        else
            dprintf(("VBoxSupDrvMapUser: find_vma failed!\n"));

        MY_DO_MUNMAP(current->mm, ulAddr, cPages * PAGE_SIZE);
    }
    else
    {
        dprintf(("supdrvOSContAllocOne: do_mmap failed ulAddr=%#lx\n", ulAddr));
        rc = SUPDRV_ERR_NO_MEMORY;
    }
    up_write(&current->mm->mmap_sem);

    return NULL;
}


/**
 * Initializes the GIP.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static int VBoxSupDrvInitGip(PSUPDRVDEVEXT pDevExt)
{
    struct page *pPage;
    dma_addr_t  HCPhys;
    PSUPGLOBALINFOPAGE pGip;
    dprintf(("VBoxSupDrvInitGip:\n"));

    /*
     * Allocate the page.
     */
    pPage = alloc_pages(GFP_USER, 0);
    if (!pPage)
    {
        dprintf(("VBoxSupDrvInitGip: failed to allocate the GIP page\n"));
        return -ENOMEM;
    }

    /*
     * Lock the page.
     */
    SetPageReserved(pPage);
    g_pGipPage = pPage;

    /*
     * Call common initialization routine.
     */
    HCPhys = page_to_phys(pPage);
    pGip = (PSUPGLOBALINFOPAGE)page_address(pPage);
    pDevExt->ulLastJiffies  = jiffies;
#ifdef TICK_NSEC
    pDevExt->u64LastMonotime = (uint64_t)pDevExt->ulLastJiffies * TICK_NSEC;
    dprintf(("VBoxSupDrvInitGIP: TICK_NSEC=%ld HZ=%d jiffies=%ld now=%lld\n",
             TICK_NSEC, HZ, pDevExt->ulLastJiffies, pDevExt->u64LastMonotime));
#else
    pDevExt->u64LastMonotime = (uint64_t)pDevExt->ulLastJiffies * (1000000 / HZ);
    dprintf(("VBoxSupDrvInitGIP: TICK_NSEC=%d HZ=%d jiffies=%ld now=%lld\n",
             (int)(1000000 / HZ), HZ, pDevExt->ulLastJiffies, pDevExt->u64LastMonotime));
#endif
    supdrvGipInit(pDevExt, pGip, HCPhys, pDevExt->u64LastMonotime,
                  HZ <= 1000 ? HZ : 1000);

    /*
     * Initialize the timer.
     */
    init_timer(&g_GipTimer);
    g_GipTimer.data = (unsigned long)pDevExt;
    g_GipTimer.function = VBoxSupGipTimer;
    g_GipTimer.expires = jiffies;

    return 0;
}


/**
 * Terminates the GIP.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static int VBoxSupDrvTermGip(PSUPDRVDEVEXT pDevExt)
{
    struct page *pPage;
    PSUPGLOBALINFOPAGE pGip;
    dprintf(("VBoxSupDrvTermGip:\n"));

    /*
     * Delete the timer if it's pending.
     */
    if (timer_pending(&g_GipTimer))
        del_timer(&g_GipTimer);

    /*
     * Uninitialize the content.
     */
    pGip = pDevExt->pGip;
    pDevExt->pGip = NULL;
    if (pGip)
        supdrvGipTerm(pGip);

    /*
     * Free the page.
     */
    pPage = g_pGipPage;
    g_pGipPage = NULL;
    if (pPage)
    {
        ClearPageReserved(pPage);
        __free_pages(pPage, 0);
    }

    return 0;
}

/**
 * Timer callback function.
 * The ulUser parameter is the device extension pointer.
 */
static void     VBoxSupGipTimer(unsigned long ulUser)
{
    PSUPDRVDEVEXT pDevExt  = (PSUPDRVDEVEXT)ulUser;
    unsigned long ulNow    = jiffies;
    unsigned long ulDiff   = ulNow - pDevExt->ulLastJiffies;
    pDevExt->ulLastJiffies = ulNow;
#ifdef TICK_NSEC
    pDevExt->u64LastMonotime += ulDiff * TICK_NSEC;
#else
    pDevExt->u64LastMonotime += ulDiff * (1000000 / HZ);
#endif
    supdrvGipUpdate(pDevExt->pGip, pDevExt->u64LastMonotime);
    mod_timer(&g_GipTimer, jiffies + (HZ <= 1000 ? 0 : ONE_MSEC_IN_JIFFIES));
}


/**
 * Maps the GIP into user space.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data.
 */
int VBOXCALL supdrvOSGipMap(PSUPDRVDEVEXT pDevExt, PCSUPGLOBALINFOPAGE *ppGip)
{
    int             rc = 0;
    unsigned long   ulAddr;
    unsigned long   HCPhys = pDevExt->HCPhysGip;
    pgprot_t        pgFlags;
    pgprot_val(pgFlags) = _PAGE_PRESENT | _PAGE_USER;
    dprintf2(("supdrvOSGipMap: ppGip=%p\n", ppGip));

    /*
     * Allocate user space mapping and put the physical pages into it.
     */
    down_write(&current->mm->mmap_sem);
    ulAddr = do_mmap(NULL, 0, PAGE_SIZE, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0);
    if (!(ulAddr & ~PAGE_MASK))
    {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) && !defined(HAVE_26_STYLE_REMAP_PAGE_RANGE)
        int rc2 = remap_page_range(ulAddr, HCPhys, PAGE_SIZE, pgFlags);
#else
        int rc2 = 0;
        struct vm_area_struct *vma = find_vma(current->mm, ulAddr);
        if (vma)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
            rc2 = remap_page_range(vma, ulAddr, HCPhys, PAGE_SIZE, pgFlags);
#else
            rc2 = remap_pfn_range(vma, ulAddr, HCPhys >> PAGE_SHIFT, PAGE_SIZE, pgFlags);
#endif
        else
        {
            rc = SUPDRV_ERR_NO_MEMORY;
            dprintf(("supdrvOSGipMap: no vma found for ulAddr=%#lx!\n", ulAddr));
        }
#endif
        if (rc2)
        {
            rc = SUPDRV_ERR_NO_MEMORY;
            dprintf(("supdrvOSGipMap: remap_page_range failed rc2=%d\n", rc2));
        }
    }
    else
    {
        dprintf(("supdrvOSGipMap: do_mmap failed ulAddr=%#lx\n", ulAddr));
        rc = SUPDRV_ERR_NO_MEMORY;
    }
    up_write(&current->mm->mmap_sem);   /* not quite sure when to give this up. */

    /*
     * Success?
     */
    if (!rc)
    {
        *ppGip = (PCSUPGLOBALINFOPAGE)ulAddr;
        dprintf2(("supdrvOSGipMap: ppGip=%p\n", *ppGip));
        return 0;
    }

    /*
     * Failure, cleanup and be gone.
     */
    if (ulAddr & ~PAGE_MASK)
    {
        down_write(&current->mm->mmap_sem);
        MY_DO_MUNMAP(current->mm, ulAddr, PAGE_SIZE);
        up_write(&current->mm->mmap_sem);
    }

    dprintf2(("supdrvOSGipMap: returns %d\n", rc));
    return rc;
}


/**
 * Maps the GIP into user space.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data.
 */
int VBOXCALL supdrvOSGipUnmap(PSUPDRVDEVEXT pDevExt, PCSUPGLOBALINFOPAGE pGip)
{
    dprintf2(("supdrvOSGipUnmap: pGip=%p\n", pGip));
    if (current->mm)
    {
        down_write(&current->mm->mmap_sem);
        MY_DO_MUNMAP(current->mm, (unsigned long)pGip, PAGE_SIZE);
        up_write(&current->mm->mmap_sem);
    }
    dprintf2(("supdrvOSGipUnmap: returns 0\n"));
    return 0;
}


/**
 * Resumes the GIP updating.
 *
 * @param   pDevExt     Instance data.
 */
void  VBOXCALL  supdrvOSGipResume(PSUPDRVDEVEXT pDevExt)
{
    dprintf2(("supdrvOSGipResume:\n"));
    mod_timer(&g_GipTimer, jiffies);
}


/**
 * Suspends the GIP updating.
 *
 * @param   pDevExt     Instance data.
 */
void  VBOXCALL  supdrvOSGipSuspend(PSUPDRVDEVEXT pDevExt)
{
    dprintf2(("supdrvOSGipSuspend:\n"));
    if (timer_pending(&g_GipTimer))
        del_timer(&g_GipTimer);
}


/**
 * Converts a supdrv error code to an linux error code.
 *
 * @returns corresponding linux error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int     VBoxSupDrvErr2LinuxErr(int rc)
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
    }

    return -EPERM;
}


RTDECL(int) SUPR0Printf(const char *pszFormat, ...)
{
#if 1
    va_list	 args;
    char	 szMsg[512];

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


/** Runtime assert implementation for Linux Ring-0. */
RTDECL(void) AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    printk("!!Assertion Failed!!\n"
             "Expression: %s\n"
             "Location  : %s(%d) %s\n",
             pszExpr, pszFile, uLine, pszFunction);
}


/** Runtime assert implementation for Linux Ring-0. */
RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{   /* forwarder. */
    va_list	 ap;
    char	 msg[256];

    va_start(ap, pszFormat);
    vsnprintf(msg, sizeof(msg) - 1, pszFormat, ap);
    msg[sizeof(msg) - 1] = '\0';
    printk("%s", msg);
    va_end(ap);
}


/* GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xcccccccc;


module_init(VBoxSupDrvInit);
module_exit(VBoxSupDrvUnload);

MODULE_AUTHOR("InnoTek Systemberatung GmbH");
MODULE_DESCRIPTION("VirtualBox Support Driver");
MODULE_LICENSE("GPL");
