/** @file
 *
 * vboxadd -- VirtualBox Guest Additions for Linux
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "the-linux-kernel.h"
#include "version-generated.h"

/* #define IRQ_DEBUG */
// #define IOCTL_DEBUG
#ifdef IOCTL_DEBUG
# define IOCTL_ENTRY(name, arg) \
do { \
    Log(("IOCTL_ENTRY: %s, 0x%x\n", (name), (arg))); \
} while(0)
# define IOCTL_EXIT(name, arg) \
do { \
    Log(("IOCTL_EXIT: %s, 0x%x\n", (name), (arg))); \
} while(0)
#else
# define IOCTL_ENTRY(name, arg) do { } while(0)
# define IOCTL_EXIT(name, arg) do { } while(0)
#endif
#ifdef IOCTL_LOG_DEBUG
# define IOCTL_LOG_ENTRY(arg) \
do { \
    Log(("IOCTL_ENTRY: Log, 0x%x\n", (arg))); \
} while(0)
# define IOCTL_LOG_EXIT(arg) \
do { \
    Log(("IOCTL_EXIT: Log, 0x%x\n", (arg))); \
} while(0)
#else
# define IOCTL_LOG_ENTRY(arg) do { } while(0)
# define IOCTL_LOG_EXIT(arg) do { } while(0)
#endif
#ifdef IOCTL_VMM_DEBUG
# define IOCTL_VMM_ENTRY(arg) \
do { \
    Log(("IOCTL_ENTRY: VMMDevReq, 0x%x\n", (arg))); \
} while(0)
# define IOCTL_VMM_EXIT(arg) \
do { \
    Log(("IOCTL_EXIT: VMMDevReq, 0x%x\n", (arg))); \
} while(0)
#else
# define IOCTL_VMM_ENTRY(arg) do { } while(0)
# define IOCTL_VMM_EXIT(arg) do { } while(0)
#endif

#include "vboxmod.h"
#include "waitcompat.h"

#include <VBox/log.h>
#include <VBox/VBoxDev.h>
#include <iprt/asm.h>
#include <iprt/assert.h>

#define xstr(s) str(s)
#define str(s) #s

MODULE_DESCRIPTION("VirtualBox Guest Additions for Linux Module");
MODULE_AUTHOR("Sun Microsystems, Inc.");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (interface " xstr(VMMDEV_VERSION) ")");
#endif

/* This is called by our assert macros to find out whether we want
   to insert a breakpoint after the assertion. In kernel modules we
   do not of course. */
RTDECL(bool)    RTAssertDoBreakpoint(void)
{
    return false;
}
EXPORT_SYMBOL(RTAssertDoBreakpoint);

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

#define MAX_HGCM_CONNECTIONS 1024

/**
 * Structure for keeping track of HGCM connections owned by user space processes, so that
 * we can close the connection if a process does not clean up properly (for example if it
 * was terminated too abruptly).
 */
/* We just define a fixed number of these so far.  This can be changed if it ever becomes
   a problem. */
static struct {
        /** Open file structure that this connection handle is associated with */
        struct file *filp;
        /** HGCM connection ID */
        uint32_t client_id;
} hgcm_connections[MAX_HGCM_CONNECTIONS] = { { 0 } };

/**
 * Register an HGCM connection as being connected with a given file descriptor, so that it
 * will be closed automatically when that file descriptor is.
 *
 * @returns 0 on success or Linux kernel error number
 * @param clientID the client ID of the HGCM connection
 * @param filep    the file structure that the connection is to be associated with
 */
static int vboxadd_register_hgcm_connection(uint32_t client_id, struct file *filp)
{
        int i;
        bool found = false;

        for (i = 0; i < MAX_HGCM_CONNECTIONS; ++i) {
                Assert(hgcm_connections[i].client_id != client_id);
        }
        for (i = 0; (i < MAX_HGCM_CONNECTIONS) && (false == found); ++i) {
                if (ASMAtomicCmpXchgU32(&hgcm_connections[i].client_id, client_id, 0)) {
                        hgcm_connections[i].filp = filp;
                        found = true;
                }
        }
        return found ? 0 : -ENFILE;  /* Any ideas for a better error code? */
}

/**
 * Unregister an HGCM connection associated with a given file descriptor without closing
 * the connection.
 *
 * @returns 0 on success or Linux kernel error number
 * @param clientID the client ID of the HGCM connection
 */
static int vboxadd_unregister_hgcm_connection_no_close(uint32_t client_id)
{
        int i;
        bool found = false;

        for (i = 0; (i < MAX_HGCM_CONNECTIONS) && (false == found); ++i) {
                if (hgcm_connections[i].client_id == client_id) {
                        hgcm_connections[i].filp = NULL;
                        hgcm_connections[i].client_id = 0;
                        found = true;
                }
        }
        for (i = 0; i < MAX_HGCM_CONNECTIONS; ++i) {
                Assert(hgcm_connections[i].client_id != client_id);
        }
        return found ? 0 : -ENOENT;
}

/**
 * Unregister all HGCM connections associated with a given file descriptor, closing
 * the connections in the process.  This should be called when a file descriptor is
 * closed.
 *
 * @returns 0 on success or Linux kernel error number
 * @param clientID the client ID of the HGCM connection
 */
static int vboxadd_unregister_all_hgcm_connections(struct file *filp)
{
        int i;

        for (i = 0; i < MAX_HGCM_CONNECTIONS; ++i) {
                if (hgcm_connections[i].filp == filp) {
                        VBoxGuestHGCMDisconnectInfo infoDisconnect;
                        infoDisconnect.u32ClientID = hgcm_connections[i].client_id;
                        vboxadd_cmc_call(vboxDev, IOCTL_VBOXGUEST_HGCM_DISCONNECT,
                                         &infoDisconnect);
                        hgcm_connections[i].filp = NULL;
                        hgcm_connections[i].client_id = 0;
                }
        }
        return 0;
}


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
 * File close handler.  Clean up any HGCM connections associated with the open file
 * which might still be open.
 */
static int vboxadd_release(struct inode *inode, struct file * filp)
{
        vboxadd_unregister_all_hgcm_connections(filp);
        return 0;
}

static void
vboxadd_wait_for_event (VBoxGuestWaitEventInfo *info)
{
    long timeleft;
    uint32_t cInterruptions = vboxDev->u32GuestInterruptions;
    uint32_t in_mask = info->u32EventMaskIn;

    info->u32Result = VBOXGUEST_WAITEVENT_OK;
    if (RT_INDEFINITE_WAIT != info->u32TimeoutIn) {
            timeleft = wait_event_interruptible_timeout
                           (vboxDev->eventq,
                               (vboxDev->u32Events & in_mask)
                            || (vboxDev->u32GuestInterruptions != cInterruptions),
                            msecs_to_jiffies (info->u32TimeoutIn)
                           );
            if (vboxDev->u32GuestInterruptions != cInterruptions) {
                    info->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
            }
            if (timeleft < 0) {
                    info->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
            }
            if (timeleft == 0) {
                    info->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
            }
    }
    else {
            if (wait_event_interruptible(vboxDev->eventq,
                                            (vboxDev->u32Events & in_mask)
                                         || (vboxDev->u32GuestInterruptions != cInterruptions)
                                        )
               ) {
                    info->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
            }
    }
    info->u32EventFlagsOut = vboxDev->u32Events & in_mask;
    vboxDev->u32Events &= ~in_mask;
}

/**
 * IOCtl handler - wait for an event from the host.
 *
 * @returns Linux kernel return code
 * @param ptr User space pointer to a structure describing the event
 */
static int vboxadd_wait_event(void *ptr)
{
        int rc = 0;
        VBoxGuestWaitEventInfo info;

        if (copy_from_user (&info, ptr, sizeof (info))) {
                LogRelFunc (("IOCTL_VBOXGUEST_WAITEVENT: can not get event info\n"));
                rc = -EFAULT;
        }

        if (0 == rc) {
                vboxadd_wait_for_event (&info);

                if (copy_to_user (ptr, &info, sizeof (info))) {
                        LogRelFunc (("IOCTL_VBOXGUEST_WAITEVENT: can not put out_mask\n"));
                        rc = -EFAULT;
                }
        }
        return 0;
}

/**
 * IOCTL handler.  Initiate an HGCM connection for a user space application.  If the connection
 * succeeds, it will be associated with the file structure used to open it, so that it will be
 * automatically shut down again if the file descriptor is closed.
 *
 * @returns 0 on success, or a Linux kernel errno value
 * @param  filp           the file structure with which the application opened the driver
 * @param  userspace_info userspace pointer to the hgcm connection information
 *                        (VBoxGuestHGCMConnectInfo structure)
 * @retval userspace_info userspace pointer to the hgcm connection information
 */
static int vboxadd_hgcm_connect(struct file *filp, unsigned long userspace_info)
{
        VBoxGuestHGCMConnectInfo info;
        VBoxGuestHGCMDisconnectInfo infoDisconnect;
        int rc = 0, rcVBox;

        if (0 != copy_from_user ((void *)&info, (void *)userspace_info, sizeof (info))) {
                LogRelFunc (("IOCTL_VBOXGUEST_HGCM_CONNECT: can not get connection info\n"));
                return -EFAULT;
        }
        rcVBox = vboxadd_cmc_call(vboxDev, IOCTL_VBOXGUEST_HGCM_CONNECT, &info);
        if (RT_FAILURE(rcVBox) || (RT_FAILURE(info.result))) {
                LogRelFunc(("IOCTL_VBOXGUEST_HGCM_CONNECT: hgcm connection failed.  internal ioctl result %Vrc, hgcm result %Vrc\n", rcVBox, info.result));
                rc = RT_FAILURE(rcVBox) ?   -RTErrConvertToErrno(rcVBox)
                                          : -RTErrConvertToErrno(info.result);
        } else {
                /* Register that the connection is associated with this file pointer. */
                LogRelFunc(("Connected, client ID %u\n", info.u32ClientID));
                rc = vboxadd_register_hgcm_connection(info.u32ClientID, filp);
                if (0 != rc) {
                        LogRelFunc(("IOCTL_VBOXGUEST_HGCM_CONNECT: failed to register the HGCM connection\n"));
                } else {
                        if (copy_to_user ((void *)userspace_info, (void *)&info,
                                          sizeof(info))) {
                                LogRelFunc (("IOCTL_VBOXGUEST_HGCM_CONNECT: failed to return the connection structure\n"));
                                rc = -EFAULT;
                        } else {
                                return 0;
                        }
                        /* Unregister again, as we didn't get as far as informing userspace. */
                        vboxadd_unregister_hgcm_connection_no_close(info.u32ClientID);
                }
                /* And disconnect the hgcm connection again, as we told userspace it failed. */
                infoDisconnect.u32ClientID = info.u32ClientID;
                vboxadd_cmc_call(vboxDev, IOCTL_VBOXGUEST_HGCM_DISCONNECT,
                                  &infoDisconnect);
        }
        return rc;
}

/**
 * IOCTL handler.  Disconnect a specific HGCM connection.
 *
 * @returns 0 on success, or a Linux kernel errno value
 * @param  filp           the file structure with which the application opened the driver
 * @param  userspace_info userspace pointer to the hgcm connection information
 *                        (VBoxGuestHGCMConnectInfo structure)
 * @retval userspace_info userspace pointer to the hgcm connection information
 */
static int vboxadd_hgcm_disconnect(struct file *filp, unsigned long userspace_info)
{
        VBoxGuestHGCMDisconnectInfo info;
        if (0 != copy_from_user ((void *)&info, (void *)userspace_info, sizeof (info))) {
                LogRelFunc (("IOCTL_VBOXGUEST_HGCM_DISCONNECT: can not get info\n"));
                return -EFAULT;
        }
        LogRelFunc(("client ID %u\n", info.u32ClientID));
        vboxadd_cmc_call(vboxDev, IOCTL_VBOXGUEST_HGCM_DISCONNECT, &info);
        if (copy_to_user ((void *)userspace_info, (void *)&info, sizeof(info))) {
                LogRelFunc (("IOCTL_VBOXGUEST_HGCM_DISCONNECT: failed to return the connection structure\n"));
                return -EFAULT;
        }
        return 0;
}

/**
 * IOCtl handler.  Control the interrupt filter mask to specify which VMMDev interrupts
 * we know how to handle.
 *
 * @returns iprt status code
 * @param pInfo kernel space pointer to the filter mask change info
 */
static int vboxadd_control_filter_mask(VBoxGuestFilterMaskInfo *pInfo)
{
    VMMDevCtlGuestFilterMask *pReq = NULL;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_CtlGuestFilterMask);

    LogFlow(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: request received, u32OrMask=0x%x, u32NotMask=0x%x\n", pInfo->u32OrMask, pInfo->u32NotMask));
    if (RT_FAILURE(rc))
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: failed to allocate %u (%#x) bytes to cache the request. rc=%d!!\n", sizeof(*pReq), sizeof(*pReq), rc));
    else
    {
        pReq->u32OrMask = pInfo->u32OrMask;
        pReq->u32NotMask = pInfo->u32NotMask;
        rc = VbglGRPerform(&pReq->header);
    }
    if (RT_FAILURE(rc))
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: VbglGRPerform failed, rc=%Rrc!\n", rc));
    else if (RT_FAILURE(pReq->header.rc))
    {
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: The request failed; VMMDev rc=%Rrc!\n", pReq->header.rc));
        rc = pReq->header.rc;
    }
    if (pReq)
        VbglGRFree(&pReq->header);
    return rc;
}

/**
 * IOCTL handler
 *
 */
static int vboxadd_ioctl(struct inode *inode, struct file *filp,
                         unsigned int cmd, unsigned long arg)
{
        int rc = 0;

        /* Deal with variable size ioctls first. */
        if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_LOG(0))
            == VBOXGUEST_IOCTL_STRIP_SIZE(cmd)) {
                char *pszMessage;

                IOCTL_LOG_ENTRY(arg);
                pszMessage = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
                if (NULL == pszMessage) {
                        LogRelFunc(("VBOXGUEST_IOCTL_LOG: cannot allocate %d bytes of memory!\n",
                                    _IOC_SIZE(cmd)));
                        rc = -ENOMEM;
                }
                if (   (0 == rc)
                    && copy_from_user(pszMessage, (void*)arg, _IOC_SIZE(cmd))) {
                        LogRelFunc(("VBOXGUEST_IOCTL_LOG: copy_from_user failed!\n"));
                        rc = -EFAULT;
                }
                if (0 == rc) {
                    Log(("%.*s", _IOC_SIZE(cmd), pszMessage));
                }
                if (NULL != pszMessage) {
                    kfree(pszMessage);
                }
                IOCTL_LOG_EXIT(arg);
        }
        else if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_VMMREQUEST(0))
            == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))  {
            VMMDevRequestHeader reqHeader;
            VMMDevRequestHeader *reqFull = NULL;
            size_t cbRequestSize;
            size_t cbVanillaRequestSize;

            IOCTL_VMM_ENTRY(arg);
            if (copy_from_user(&reqHeader, (void*)arg, sizeof(reqHeader)))
            {
                LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: copy_from_user failed for vmm request!\n"));
                rc = -EFAULT;
            }
            if (0 == rc)
            {
                /* get the request size */
                cbVanillaRequestSize = vmmdevGetRequestSize(reqHeader.requestType);
                if (!cbVanillaRequestSize)
                {
                    LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: invalid request type: %d\n",
                            reqHeader.requestType));
                    rc = -EINVAL;
                }
            }
            if (0 == rc)
            {
                cbRequestSize = reqHeader.size;
                if (cbRequestSize < cbVanillaRequestSize)
                {
                    LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: invalid request size: %d min: %d type: %d\n",
                            cbRequestSize,
                            cbVanillaRequestSize,
                            reqHeader.requestType));
                    rc = -EINVAL;
                }
            }
            if (0 == rc)
            {
                /* request storage for the full request */
                rc = VbglGRAlloc(&reqFull, cbRequestSize, reqHeader.requestType);
                if (VBOX_FAILURE(rc))
                {
                    LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: could not allocate request structure! rc = %d\n", rc));
                    rc = -EFAULT;
                }
            }
            if (0 == rc)
            {
                /* now get the full request */
                if (copy_from_user(reqFull, (void*)arg, cbRequestSize))
                {
                    LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: failed to fetch full request from user space!\n"));
                    rc = -EFAULT;
                }
            }

            /* now issue the request */
            if (0 == rc)
            {
                int rrc = VbglGRPerform(reqFull);

                /* asynchronous processing? */
                if (rrc == VINF_HGCM_ASYNC_EXECUTE)
                {
                    VMMDevHGCMRequestHeader *reqHGCM = (VMMDevHGCMRequestHeader*)reqFull;
                    wait_event_interruptible (vboxDev->eventq, reqHGCM->fu32Flags & VBOX_HGCM_REQ_DONE);
                    rrc = reqFull->rc;
                }

                /* failed? */
                if (VBOX_FAILURE(rrc) || VBOX_FAILURE(reqFull->rc))
                {
                    LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: request execution failed!\n"));
                    rc = VBOX_FAILURE(rrc) ? -RTErrConvertToErrno(rrc)
                                           : -RTErrConvertToErrno(reqFull->rc);
                }
                else
                {
                    /* success, copy the result data to user space */
                    if (copy_to_user((void*)arg, (void*)reqFull, cbRequestSize))
                    {
                        LogRelFunc(("IOCTL_VBOXGUEST_VMMREQUEST: error copying request result to user space!\n"));
                        rc = -EFAULT;
                    }
                }
            }
            if (NULL != reqFull)
                VbglGRFree(reqFull);
            IOCTL_VMM_EXIT(arg);
        }
        else if (   (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL(0))
                == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
            || (cmd == IOCTL_VBOXGUEST_HGCM_CALL))
        {
        /* This IOCTL allows the guest to make an HGCM call from user space.  The
           OS-independant part of the Guest Additions already contain code for making an
           HGCM call from the guest, but this code assumes that the call is made from the
           kernel's address space.  So before calling it, we have to copy all parameters
           to the HGCM call from user space to kernel space and reconstruct the structures
           passed to the call (which include pointers to other memory) inside the kernel's
           address space. */
                IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CALL", arg);
                rc = vbox_ioctl_hgcm_call(arg, vboxDev);
                IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CALL", arg);
        }
        else
        {
            switch (cmd) {
            case IOCTL_VBOXGUEST_WAITEVENT:
                    IOCTL_ENTRY("IOCTL_VBOXGUEST_WAITEVENT", arg);
                    rc = vboxadd_wait_event((void *) arg);
                    IOCTL_EXIT("IOCTL_VBOXGUEST_WAITEVENT", arg);
                    break;
            case VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS:
                    IOCTL_ENTRY("VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS", arg);
                    ++vboxDev->u32GuestInterruptions;
                    IOCTL_EXIT("VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS", arg);
                    break;
            case IOCTL_VBOXGUEST_HGCM_CONNECT:
                    IOCTL_ENTRY("IOCTL_VBOXGUEST_HGCM_CONNECT", arg);
                    rc = vboxadd_hgcm_connect(filp, arg);
                    IOCTL_EXIT("IOCTL_VBOXGUEST_HGCM_CONNECT", arg);
                    break;
            case IOCTL_VBOXGUEST_HGCM_DISCONNECT:
                    IOCTL_ENTRY("IOCTL_VBOXGUEST_HGCM_DISCONNECT", arg);
                    vboxadd_hgcm_disconnect(filp, arg);
                    IOCTL_EXIT("IOCTL_VBOXGUEST_HGCM_DISCONNECT", arg);
                    break;
            case VBOXGUEST_IOCTL_CTL_FILTER_MASK:
            {
                    VBoxGuestFilterMaskInfo info;
                    IOCTL_ENTRY("VBOXGUEST_IOCTL_CTL_FILTER_MASK", arg);
                    if (copy_from_user((void*)&info, (void*)arg, sizeof(info)))
                    {
                        LogRelFunc(("VBOXGUEST_IOCTL_CTL_FILTER_MASK: error getting parameters from user space!\n"));
                        rc = -EFAULT;
                        break;
                    }
                    rc = -RTErrConvertToErrno(vboxadd_control_filter_mask(&info));
                    IOCTL_EXIT("VBOXGUEST_IOCTL_CTL_FILTER_MASK", arg);
                    break;
            }
            default:
                    LogRelFunc(("unknown command: %x\n", cmd));
                    rc = -EINVAL;
                    break;
            }
        }
        return rc;
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
    Log(("vboxadd IRQ_DEBUG: vboxDev->pVMMDevMemory=%p vboxDev->pVMMDevMemory->fHaveEvents=%d\n",
          vboxDev->pVMMDevMemory, vboxDev->pVMMDevMemory->V.V1_04.fHaveEvents));
#endif

    /* check if IRQ was asserted by VBox */
    if (vboxDev->pVMMDevMemory->V.V1_04.fHaveEvents != 0)
    {
#ifdef IRQ_DEBUG
        Log(("vboxadd IRQ_DEBUG: got IRQ with event mask 0x%x\n",
               vboxDev->irqAckRequest->events));
#endif

        /* make a copy of the event mask */
        rcVBox = VbglGRPerform (&vboxDev->irqAckRequest->header);
        if (VBOX_SUCCESS(rcVBox) && VBOX_SUCCESS(vboxDev->irqAckRequest->header.rc))
        {
            if (RT_LIKELY (vboxDev->irqAckRequest->events))
            {
                vboxDev->u32Events |= vboxDev->irqAckRequest->events;
                wake_up (&vboxDev->eventq);
            }
        }
        else
        {
            /* impossible... */
            LogRelFunc(("IRQ was not acknowledged! rc = %Vrc, header.rc = %Vrc\n",
                        rcVBox, vboxDev->irqAckRequest->header.rc));
            BUG ();
        }

        /* it was ours! */
        fIRQTaken = 1;
    }
#ifdef IRQ_DEBUG
    else
    {
        Log(("vboxadd IRQ_DEBUG: stale IRQ mem=%p events=%d devevents=%#x\n",
             vboxDev->pVMMDevMemory,
             vboxDev->pVMMDevMemory->V.V1_04.fHaveEvents,
             vboxDev->u32Events));
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
        LogRelFunc(("failed to allocate hypervisor info structure! rc = %Vrc\n", rcVBox));
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
                req->hypervisorStart    = (vmmDevHypPtr)RT_ALIGN_P(hypervisorArea, 0x400000);
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
                    LogRelFunc(("failed to set hypervisor region! rc = %Vrc, header.rc = %Vrc\n",
                                rcVBox, req->header.rc));
                    goto bail_out;
                }
            }
            else
            {
                LogRelFunc(("failed to allocate 0x%x bytes of IO space\n", hypervisorSize));
                goto bail_out;
            }
        }
    }
    else
    {
        LogRelFunc(("failed to query hypervisor info! rc = %Vrc, header.rc = %Vrc\n",
                    rcVBox, req->header.rc));
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
        LogRelFunc(("failed to allocate hypervisor info structure! rc = %Vrc\n", rcVBox));
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
        LogRelFunc(("failed to reset hypervisor info! rc = %Vrc, header.rc = %Vrc\n",
                    rcVBox, req->header.rc));
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#define PCI_DEV_GET(v,d,p) pci_get_device(v,d,p)
#define PCI_DEV_PUT(x) pci_dev_put(x)
#else
#define PCI_DEV_GET(v,d,p) pci_find_device(v,d,p)
#define PCI_DEV_PUT(x)
#endif

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

    if (vboxadd_cmc_init ())
    {
        printk (KERN_ERR "vboxadd: could not init cmc.\n");
        return -ENODEV;
    }

    /*
     * Detect PCI device
     */
    pcidev = PCI_DEV_GET(VMMDEV_VENDORID, VMMDEV_DEVICEID, pcidev);
    if (!pcidev)
    {
        printk(KERN_ERR "vboxadd: VirtualBox PCI device not found.\n");
        return -ENODEV;
    }

    err = pci_enable_device (pcidev);
    if (err)
    {
        Log(("vboxadd: could not enable device: %d\n", err));
        PCI_DEV_PUT(pcidev);
        return -ENODEV;
    }

    LogRel(("Starting VirtualBox version %s Guest Additions\n",
            VBOX_VERSION_STRING));
    /* register a character device */
    err = register_chrdev(vbox_major, "vboxadd", &vbox_fops);
    if (err < 0 || ((vbox_major & err) || (!vbox_major && !err)))
    {
        LogRelFunc(("register_chrdev failed: vbox_major: %d, err = %d\n",
                     vbox_major, err));
        PCI_DEV_PUT(pcidev);
        return -ENODEV;
    }
    /* if no major code was set, take the return value */
    if (!vbox_major)
        vbox_major = err;

    /* allocate and initialize device extension */
    vboxDev = kmalloc(sizeof(*vboxDev), GFP_KERNEL);
    if (!vboxDev)
    {
        LogRelFunc(("cannot allocate device!\n"));
        err = -ENOMEM;
        goto fail;
    }
    memset(vboxDev, 0, sizeof(*vboxDev));
    snprintf(vboxDev->name, sizeof(vboxDev->name), "vboxadd");

    /* get the IO port region */
    vboxDev->io_port = pci_resource_start(pcidev, 0);

    /* get the memory region */
    vboxDev->vmmdevmem = pci_resource_start(pcidev, 1);
    vboxDev->vmmdevmem_size = pci_resource_len(pcidev, 1);

    /* all resources found? */
    if (!vboxDev->io_port || !vboxDev->vmmdevmem || !vboxDev->vmmdevmem_size)
    {
        LogRelFunc(("did not find expected hardware resources!\n"));
        err = -ENXIO;
        goto fail;
    }

    /* request ownership of adapter memory */
    if (request_mem_region(vboxDev->vmmdevmem, vboxDev->vmmdevmem_size, "vboxadd") == 0)
    {
        LogRelFunc(("failed to request adapter memory!\n"));
        err = -ENXIO;
        goto fail;
    }

    /* map adapter memory into kernel address space and check version */
    vboxDev->pVMMDevMemory = (VMMDevMemory *) ioremap(vboxDev->vmmdevmem,
                                                      vboxDev->vmmdevmem_size);
    if (!vboxDev->pVMMDevMemory)
    {
        LogRelFunc(("ioremap failed\n"));
        err = -ENOMEM;
        goto fail;
    }

    if (vboxDev->pVMMDevMemory->u32Version != VMMDEV_MEMORY_VERSION)
    {
        LogRelFunc(("invalid VMM device memory version! (got 0x%x, expected 0x%x)\n",
                    vboxDev->pVMMDevMemory->u32Version, VMMDEV_MEMORY_VERSION));
        err = -ENXIO;
        goto fail;
    }

    /* initialize VBGL subsystem */
    rcVBox = VbglInit(vboxDev->io_port, vboxDev->pVMMDevMemory);
    if (VBOX_FAILURE(rcVBox))
    {
        LogRelFunc(("could not initialize VBGL subsystem! rc = %Vrc\n", rcVBox));
        err = -ENXIO;
        goto fail;
    }

    /* report guest information to host, this must be done as the very first request */
    rcVBox = VbglGRAlloc((VMMDevRequestHeader**)&infoReq,
                         sizeof(VMMDevReportGuestInfo), VMMDevReq_ReportGuestInfo);
    if (VBOX_FAILURE(rcVBox))
    {
        LogRelFunc(("could not allocate request structure! rc = %Vrc\n", rcVBox));
        err = -ENOMEM;
        goto fail;
    }

    /* report guest version to host, the VMMDev requires that to be done first */
    infoReq->guestInfo.additionsVersion = VMMDEV_VERSION;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 0)
    infoReq->guestInfo.osType = VBOXOSTYPE_Linux26;
#else
    infoReq->guestInfo.osType = VBOXOSTYPE_Linux24;
#endif
    rcVBox = VbglGRPerform(&infoReq->header);
    if (VBOX_FAILURE(rcVBox) || VBOX_FAILURE(infoReq->header.rc))
    {
        LogRelFunc(("error reporting guest info to host! rc = %Vrc, header.rc = %Vrc\n",
                    rcVBox, infoReq->header.rc));
        VbglGRFree(&infoReq->header);
        err = -ENXIO;
        goto fail;
    }
    VbglGRFree(&infoReq->header);

    /* Unset the graphics capability until/unless X is loaded. */
    /** @todo check the error code once we bump the additions version.
              For now we ignore it for compatibility with older hosts. */
    {
        VMMDevReqGuestCapabilities2 *vmmreqGuestCaps;

        
        rcVBox = VbglGRAlloc((VMMDevRequestHeader**)&vmmreqGuestCaps,
                              sizeof(VMMDevReqGuestCapabilities2),
                              VMMDevReq_SetGuestCapabilities);
        if (VBOX_FAILURE(rcVBox))
        {
            LogRelFunc(("could not allocate request structure! rc = %Vrc\n", rcVBox));
            err = -ENOMEM;
            goto fail;
        }
        vmmreqGuestCaps->u32OrMask = 0;
        vmmreqGuestCaps->u32NotMask = VMMDEV_GUEST_SUPPORTS_GRAPHICS;
        rcVBox = VbglGRPerform(&vmmreqGuestCaps->header);
        VbglGRFree(&vmmreqGuestCaps->header);
        if (RT_FAILURE(rcVBox))
        {
            err = -ENXIO;
            goto fail;
        }
    }

    /* perform hypervisor address space reservation */
    if (vboxadd_reserve_hypervisor())
    {
        /* we just ignore the error, no address window reservation, non fatal */
    }

    /* allocate a VMM request structure for use in the ISR */
    rcVBox = VbglGRAlloc((VMMDevRequestHeader**)&vboxDev->irqAckRequest,
                         sizeof(VMMDevEvents), VMMDevReq_AcknowledgeEvents);
    if (VBOX_FAILURE(rcVBox))
    {
        LogRelFunc(("could not allocate request structure! rc = %Vrc\n", rcVBox));
        err = -ENOMEM;
        goto fail;
    }

    /* get ISR */
    err = request_irq(pcidev->irq, vboxadd_irq_handler,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
                      IRQF_SHARED,
#else
                      SA_SHIRQ,
#endif
                      "vboxadd", vboxDev);
    if (err)
    {
        LogRelFunc(("could not request IRQ %d, err: %d\n", pcidev->irq, err));
        goto fail;
    }
    vboxDev->irq = pcidev->irq;

    init_waitqueue_head (&vboxDev->eventq);

    /* some useful information for the user but don't show this on the console */
    LogRel(("VirtualBox device settings: major %d, IRQ %d, "
                "I/O port 0x%x, MMIO at 0x%x (size 0x%x), "
                "hypervisor window at 0x%p (size 0x%x)\n",
                vbox_major, vboxDev->irq, vboxDev->io_port,
                vboxDev->vmmdevmem, vboxDev->vmmdevmem_size,
                vboxDev->hypervisorStart, vboxDev->hypervisorSize));
    Log(("Successfully loaded VirtualBox device version "
           VBOX_VERSION_STRING " (interface " xstr(VMMDEV_VERSION) ")\n"));

    /* successful return */
    PCI_DEV_PUT(pcidev);
    return 0;

fail:
    PCI_DEV_PUT(pcidev);
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
    unregister_chrdev(vbox_major, "vboxadd");
    free_resources();
    vboxadd_cmc_fini ();
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
