/** @file
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
/* #define IOCTL_DEBUG */
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
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

#define xstr(s) str(s)
#define str(s) #s

MODULE_DESCRIPTION("VirtualBox Guest Additions for Linux Module");
MODULE_AUTHOR("Sun Microsystems, Inc.");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (interface " xstr(VMMDEV_VERSION) ")");
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


#define MAX_HGCM_CONNECTIONS 1024

/**
 * Structure for keeping track of HGCM connections owned by user space processes, so that
 * we can close the connection if a process does not clean up properly (for example if it
 * was terminated too abruptly).
 */
/* We just define a fixed number of these so far.  This can be changed if it ever becomes
   a problem. */
static struct
{
    /** Open file structure that this connection handle is associated with */
    struct file *filp;
    /** HGCM connection ID */
    uint32_t client_id;
} hgcm_connections[MAX_HGCM_CONNECTIONS] 
=
{
    { 0 }
};


/**
 * This function converts a VBox result code into a Linux error number.
 * Note that we return 0 (success) for all informational values, as Linux
 * has no such concept.
 */
static int vboxadd_convert_result(int vbox_err)
{
    if (   vbox_err > -1000
        && vbox_err < 1000)
        return RTErrConvertToErrno(vbox_err);
    switch (vbox_err)
    {
        case VERR_HGCM_SERVICE_NOT_FOUND:      return ESRCH;
        case VINF_HGCM_CLIENT_REJECTED:        return 0;
        case VERR_HGCM_INVALID_CMD_ADDRESS:    return EFAULT;
        case VINF_HGCM_ASYNC_EXECUTE:          return 0;
        case VERR_HGCM_INTERNAL:               return EPROTO;
        case VERR_HGCM_INVALID_CLIENT_ID:      return EINVAL;
        case VINF_HGCM_SAVE_STATE:             return 0;
        /* No reason to return this to a guest */
        // case VERR_HGCM_SERVICE_EXISTS:         return EEXIST;
    }
    AssertMsgFailedReturn(("Unhandled error code %Rrc\n", vbox_err), EPROTO);
}

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

    for (i = 0; i < MAX_HGCM_CONNECTIONS; ++i)
    {
        Assert(hgcm_connections[i].client_id != client_id);
    }
    for (i = 0; (i < MAX_HGCM_CONNECTIONS) && (false == found); ++i)
    {
        if (ASMAtomicCmpXchgU32(&hgcm_connections[i].client_id, client_id, 0))
        {
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

    for (i = 0; (i < MAX_HGCM_CONNECTIONS) && (false == found); ++i)
    {
        if (hgcm_connections[i].client_id == client_id)
        {
            hgcm_connections[i].filp = NULL;
            hgcm_connections[i].client_id = 0;
            found = true;
        }
    }
    for (i = 0; i < MAX_HGCM_CONNECTIONS; ++i)
    {
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

    for (i = 0; i < MAX_HGCM_CONNECTIONS; ++i)
    {
        if (hgcm_connections[i].filp == filp)
        {
            VBoxGuestHGCMDisconnectInfo infoDisconnect;
            infoDisconnect.u32ClientID = hgcm_connections[i].client_id;
            vboxadd_cmc_call(vboxDev, VBOXGUEST_IOCTL_HGCM_DISCONNECT,
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

static void
vboxadd_wait_for_event (VBoxGuestWaitEventInfo *info)
{
    long timeleft;
    uint32_t cInterruptions = vboxDev->u32GuestInterruptions;
    uint32_t in_mask = info->u32EventMaskIn;

    info->u32Result = VBOXGUEST_WAITEVENT_OK;
    if (RT_INDEFINITE_WAIT != info->u32TimeoutIn)
    {
        timeleft = wait_event_interruptible_timeout
                           (vboxDev->eventq,
                               (vboxDev->u32Events & in_mask)
                            || (vboxDev->u32GuestInterruptions != cInterruptions),
                            msecs_to_jiffies (info->u32TimeoutIn)
                           );
        if (vboxDev->u32GuestInterruptions != cInterruptions)
        {
            info->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
        }
        if (timeleft < 0)
        {
            info->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
        }
        if (timeleft == 0)
        {
            info->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        }
    }
    else
    {
        if (wait_event_interruptible(vboxDev->eventq,
                                       (vboxDev->u32Events & in_mask)
                                         || (vboxDev->u32GuestInterruptions != cInterruptions)
                    )
           )
        {
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

    if (copy_from_user (&info, ptr, sizeof (info)))
    {
        LogRelFunc (("VBOXGUEST_IOCTL_WAITEVENT: can not get event info\n"));
        rc = -EFAULT;
    }

    if (0 == rc)
    {
        vboxadd_wait_for_event (&info);

        if (copy_to_user (ptr, &info, sizeof (info)))
        {
            LogRelFunc (("VBOXGUEST_IOCTL_WAITEVENT: can not put out_mask\n"));
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
    int rc = 0;

    if (copy_from_user ((void *)&info, (void *)userspace_info,
                            sizeof (info)) != 0)
    {
        LogFunc (("VBOXGUEST_IOCTL_HGCM_CONNECT: can not get connection info\n"));
        rc = -EFAULT;
    }
    info.u32ClientID = 0;
    if (rc >= 0)
    {
        int vrc = vboxadd_cmc_call(vboxDev, VBOXGUEST_IOCTL_HGCM_CONNECT,
                                    &info);
        rc = RT_FAILURE(vrc) ? -vboxadd_convert_result(vrc)
                             : -vboxadd_convert_result(info.result);
        if (rc < 0)
            LogFunc(("hgcm connection failed.  internal ioctl result %Rrc, hgcm result %Rrc\n",
                      vrc, info.result));
        if (rc >= 0 && info.result < 0)
            rc = info.result;
    }
    if (rc >= 0)
    {
        /* Register that the connection is associated with this file pointer. */
        LogFunc(("Connected, client ID %u\n", info.u32ClientID));
        rc = vboxadd_register_hgcm_connection(info.u32ClientID, filp);
        if (rc < 0)
            LogFunc(("failed to register the HGCM connection\n"));
    }
    if (   rc >= 0
        && copy_to_user ((void *)userspace_info, (void *)&info,
                             sizeof(info)) != 0)
    {
        LogFunc (("failed to return the connection structure\n"));
        rc = -EFAULT;
    }
    if (rc < 0 && (info.u32ClientID != 0))
        /* Unregister again, as we didn't get as far as informing userspace. */
        vboxadd_unregister_hgcm_connection_no_close(info.u32ClientID);
    if (rc < 0 && info.u32ClientID != 0)
    {
        /* Disconnect the hgcm connection again, as we told userspace it failed. */
        VBoxGuestHGCMDisconnectInfo infoDisconnect;
        infoDisconnect.u32ClientID = info.u32ClientID;
        vboxadd_cmc_call(vboxDev, VBOXGUEST_IOCTL_HGCM_DISCONNECT,
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
    int rc = 0, vrc = VINF_SUCCESS;

    VBoxGuestHGCMDisconnectInfo info;
    if (copy_from_user((void *)&info, (void *)userspace_info, sizeof (info)) != 0)
    {
        LogRelFunc (("VBOXGUEST_IOCTL_HGCM_DISCONNECT: can not get info\n"));
        rc = -EFAULT;
    }
    if (rc >= 0)
    {
        vrc = vboxadd_cmc_call(vboxDev, VBOXGUEST_IOCTL_HGCM_DISCONNECT, &info);
        rc = -vboxadd_convert_result(vrc);
        if (rc < 0)
            LogFunc(("HGCM disconnect failed, error %Rrc\n", vrc));
    }
    if (   rc >= 0
        && copy_to_user((void *)userspace_info, (void *)&info, sizeof(info)) != 0)
    {
        LogRelFunc (("VBOXGUEST_IOCTL_HGCM_DISCONNECT: failed to return the connection structure\n"));
        rc = -EFAULT;
    }
    return rc;
}

/** Bounce buffer structure for hcgm guest-host data copies. */
typedef struct hgcm_bounce_buffer
{
    /** Kernel memory address. */
    void *pKernel;
    /** User memory address. */
    void *pUser;
    /** Buffer size. */
    size_t cb;
} hgcm_bounce_buffer;

/** Create a bounce buffer in kernel space for user space memory. */
static int vboxadd_hgcm_alloc_buffer(hgcm_bounce_buffer **ppBuf, void *pUser,
                                     size_t cb, bool copy)
{
    hgcm_bounce_buffer *pBuf = NULL;
    void *pKernel = NULL;
    int rc = 0;

    AssertPtrReturn(ppBuf, -EINVAL);
    /* Empty buffers are allowed, but then the user pointer should be NULL */
    AssertReturn(((cb > 0) && VALID_PTR(pUser)) || (pUser == NULL), -EINVAL);

    pBuf = RTMemAlloc(sizeof(*pBuf));
    if (pBuf == NULL)
        rc = -ENOMEM;
    if (rc >= 0)
    {
        if (cb > 0)
        {
            pKernel = RTMemAlloc(cb);
            if (pKernel == NULL)
                rc = -ENOMEM;
            if (   rc >= 0
                && copy
                && copy_from_user(pKernel, pUser, cb) != 0)
                rc = -EFAULT;
        }
        else
            /* We definitely don't want to copy anything from an empty
             * buffer. */
            pKernel = NULL;
    }
    if (rc >= 0)
    {
        pBuf->pKernel = pKernel;
        pBuf->pUser = pUser;
        pBuf->cb = cb;
        *ppBuf = pBuf;
    }
    else
    {
        RTMemFree(pBuf);
        RTMemFree(pKernel);
        LogFunc(("failed, returning %d\n", rc));
    }
    return rc;
}

/** Free a kernel space bounce buffer for user space memory. */
static int vboxadd_hgcm_free_buffer(hgcm_bounce_buffer *pBuf, bool copy)
{
    int rc = 0;
    AssertPtrReturn(pBuf, -EINVAL);
    if ((pBuf->cb > 0)
        && copy
        && copy_to_user(pBuf->pUser, pBuf->pKernel, pBuf->cb) != 0)
        rc = -EFAULT;
    RTMemFree(pBuf->pKernel);  /* We want to do this whatever the outcome. */
    RTMemFree(pBuf);
    if (rc < 0)
        LogFunc(("failed, returning %d\n", rc));
    return rc;
}

/** Lock down R3 memory as needed for the HGCM call.  Copied from
 * HGCMInternal.cpp and SysHlp.cpp */
static int vboxadd_buffer_hgcm_parms(void **ppvCtx, VBoxGuestHGCMCallInfo *pCallInfo)
{
    uint32_t cbParms = pCallInfo->cParms * sizeof (HGCMFunctionParameter);
    int rc = 0;
    unsigned iParm;
    HGCMFunctionParameter *pParm;
    memset (ppvCtx, 0, sizeof(void *) * pCallInfo->cParms);
    if (cbParms)
    {
        /* Lock user buffers. */
        pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);

        for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
        {
            switch (pParm->type)
            {
            case VMMDevHGCMParmType_LinAddr_Locked_In:
                pParm->type = VMMDevHGCMParmType_LinAddr_In;
                break;
            case VMMDevHGCMParmType_LinAddr_Locked_Out:
                pParm->type = VMMDevHGCMParmType_LinAddr_Out;
                break;
            case VMMDevHGCMParmType_LinAddr_Locked:
                pParm->type = VMMDevHGCMParmType_LinAddr;
                break;

            case VMMDevHGCMParmType_LinAddr_In:
            case VMMDevHGCMParmType_LinAddr_Out:
            case VMMDevHGCMParmType_LinAddr:
            {
                void *pv = (void *) pParm->u.Pointer.u.linearAddr;
                uint32_t u32Size = pParm->u.Pointer.size;
                hgcm_bounce_buffer *MemObj = NULL;
                rc = vboxadd_hgcm_alloc_buffer(&MemObj, pv, u32Size,
                         pParm->type != VMMDevHGCMParmType_LinAddr_Out /* copy */);
                if (rc >= 0)
                {
                    ppvCtx[iParm] = MemObj;
                    pParm->u.Pointer.u.linearAddr = (uintptr_t)MemObj->pKernel;
                }
                else
                    ppvCtx[iParm] = NULL;
                break;
            }
            default:
                /* make gcc happy */
                break;
            }
            if (rc < 0)
                break;
        }
    }
    return rc;
}

/** Unlock R3 memory after the HGCM call.  Copied from HGCMInternal.cpp and
 * SysHlp.cpp */
static int vboxadd_unbuffer_hgcm_parms(void **ppvCtx, VBoxGuestHGCMCallInfo *pCallInfo)
{
    int rc = 0;
    unsigned iParm;
    /* Unlock user buffers. */
    HGCMFunctionParameter *pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);

    for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
    {
        if (   pParm->type == VMMDevHGCMParmType_LinAddr_In
            || pParm->type == VMMDevHGCMParmType_LinAddr_Out
            || pParm->type == VMMDevHGCMParmType_LinAddr)
        {
            if (ppvCtx[iParm] != NULL)
            {
                hgcm_bounce_buffer *MemObj = (hgcm_bounce_buffer *)ppvCtx[iParm];
                int rc2 = vboxadd_hgcm_free_buffer(MemObj,
                                  pParm->type != VMMDevHGCMParmType_LinAddr_In /* copy */);
                if (rc >= 0 && rc2 < 0)
                    rc = rc2;  /* Report the first error. */
            }
        }
        else
        {
            if (ppvCtx[iParm] != NULL)
            {
                AssertFailed();
                rc = -EOVERFLOW;  /* Something unlikely to turn up elsewhere so
                                   * we can see where it's coming from. */
            }
        }
    }
    return rc;
}

/**
 * IOCTL handler.  Make an HGCM call.
 *
 * @returns 0 on success, or a Linux kernel errno value
 * @param  userspace_info userspace pointer to the hgcm connection information
 *                        (VBoxGuestHGCMConnectInfo structure).  This will be
 *                        updated on success.
 * @param  u32Size        the size of the userspace structure
 */
static int vboxadd_hgcm_call(unsigned long userspace_info, uint32_t u32Size)
{
    VBoxGuestHGCMCallInfo *pInfo = NULL;
    void *apvCtx[VBOX_HGCM_MAX_PARMS];
    unsigned haveParms = 0;
    int rc = 0;

    pInfo = kmalloc(u32Size, GFP_KERNEL);
    if (pInfo == NULL)
        rc = -ENOMEM;
    if (   rc >= 0
        &&  0 != copy_from_user ((void *)pInfo, (void *)userspace_info, u32Size))
    {
        LogRelFunc (("can not get info from user space\n"));
        rc = -EFAULT;
    }
    if (   rc >= 0
        && sizeof(*pInfo) + pInfo->cParms * sizeof(HGCMFunctionParameter) != u32Size)
    {
        LogRelFunc (("bad parameter size, structure says %d, ioctl says %d\n",
                    sizeof(*pInfo) + pInfo->cParms * sizeof(HGCMFunctionParameter),
                    u32Size));
        rc = -EINVAL;
    }
    if (rc >= 0)
    {
        haveParms = 1;
        rc = vboxadd_buffer_hgcm_parms(apvCtx, pInfo);
    }
    if (rc >= 0)
    {
        int vrc;
        vrc = vboxadd_cmc_call(vboxDev,
                               VBOXGUEST_IOCTL_HGCM_CALL(u32Size), pInfo);
        rc = -vboxadd_convert_result(vrc);
        if (rc < 0)
            LogFunc(("HGCM call failed, error %Rrc\n", vrc));
        if (   rc >= 0
            && copy_to_user ((void *)userspace_info, (void *)pInfo,
                                     u32Size))
        {
            LogRelFunc (("failed to return the information to user space\n"));
            rc = -EFAULT;
        }
    }
    if (haveParms)
    {
        int rc2 = vboxadd_unbuffer_hgcm_parms(apvCtx, pInfo);
        if (rc >= 0 && rc2 < 0)
            rc = rc2;
    }
    if (pInfo != NULL)
        kfree(pInfo);
    return rc;
}

/**
 * IOCTL handler.  Make an HGCM call with timeout.
 *
 * @returns 0 on success, or a Linux kernel errno value
 * @param  userspace_info userspace pointer to the hgcm connection information
 *                        (VBoxGuestHGCMConnectInfo structure).  This will be
 *                        updated on success.
 * @param  u32Size        the size of the userspace structure
 */
static int vboxadd_hgcm_call_timed(unsigned long userspace_info,
                                   uint32_t u32Size)
{
    VBoxGuestHGCMCallInfoTimed *pInfo = NULL;
    void *apvCtx[VBOX_HGCM_MAX_PARMS];
    unsigned haveParms = 0;
    int rc = 0;

    pInfo = kmalloc(u32Size, GFP_KERNEL);
    if (pInfo == NULL)
        rc = -ENOMEM;
    if (   rc >= 0
        &&  0 != copy_from_user ((void *)pInfo, (void *)userspace_info, u32Size))
    {
        LogRelFunc (("can not get info from user space\n"));
        rc = -EFAULT;
    }
    if (   rc >= 0
        && sizeof(*pInfo) + pInfo->info.cParms * sizeof(HGCMFunctionParameter) != u32Size)
    {
        LogRelFunc (("bad parameter size, structure says %d, ioctl says %d\n",
                    sizeof(*pInfo) + pInfo->info.cParms * sizeof(HGCMFunctionParameter),
                    u32Size));
        rc = -EINVAL;
    }
    if (rc >= 0)
    {
        haveParms = 1;
        rc = vboxadd_buffer_hgcm_parms(apvCtx, &pInfo->info);
    }
    if (rc >= 0)
    {
        int vrc;
        pInfo->fInterruptible = true;  /* User space may not do uninterruptible waits */
        vrc = vboxadd_cmc_call(vboxDev,
                               VBOXGUEST_IOCTL_HGCM_CALL_TIMED(u32Size), pInfo);
        rc = -vboxadd_convert_result(vrc);
        if (rc < 0)
            LogFunc(("HGCM call failed, error %Rrc", vrc));
        if (   rc >= 0
            && copy_to_user ((void *)userspace_info, (void *)pInfo, u32Size))
        {
            LogRelFunc (("failed to return the information to user space\n"));
            rc = -EFAULT;
        }
    }
    if (haveParms)
    {
        int rc2 = vboxadd_unbuffer_hgcm_parms(apvCtx, &pInfo->info);
        if (rc >= 0 && rc2 < 0)
            rc = rc2;
    }
    if (pInfo != NULL)
        kfree(pInfo);
    return rc;
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
 * IOCTL handler for vboxadd
 */
static int vboxadd_ioctl(struct inode *inode, struct file *filp,
                         unsigned int cmd, unsigned long arg)
{
    int rc = 0;

    /* Deal with variable size ioctls first. */
    if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_LOG(0))
        == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        char *pszMessage;

        IOCTL_LOG_ENTRY(arg);
        pszMessage = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
        if (NULL == pszMessage)
        {
            LogRelFunc(("VBOXGUEST_IOCTL_LOG: cannot allocate %d bytes of memory!\n",
                         _IOC_SIZE(cmd)));
            rc = -ENOMEM;
        }
        if (   (0 == rc)
            && copy_from_user(pszMessage, (void*)arg, _IOC_SIZE(cmd)))
        {
            LogRelFunc(("VBOXGUEST_IOCTL_LOG: copy_from_user failed!\n"));
            rc = -EFAULT;
        }
        if (0 == rc)
        {
            Log(("%.*s", _IOC_SIZE(cmd), pszMessage));
        }
        if (NULL != pszMessage)
        {
            kfree(pszMessage);
        }
        IOCTL_LOG_EXIT(arg);
    }
    else if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_VMMREQUEST(0))
             == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        VMMDevRequestHeader reqHeader;
        VMMDevRequestHeader *reqFull = NULL;
        size_t cbRequestSize;
        size_t cbVanillaRequestSize;

        IOCTL_VMM_ENTRY(arg);
        if (copy_from_user(&reqHeader, (void*)arg, sizeof(reqHeader)))
        {
            LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: copy_from_user failed for vmm request!\n"));
            rc = -EFAULT;
        }
        if (0 == rc)
        {
            /* get the request size */
            cbVanillaRequestSize = vmmdevGetRequestSize(reqHeader.requestType);
            if (!cbVanillaRequestSize)
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: invalid request type: %d\n",
                        reqHeader.requestType));
                rc = -EINVAL;
            }
        }
        if (0 == rc)
        {
            cbRequestSize = reqHeader.size;
            if (cbRequestSize < cbVanillaRequestSize)
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: invalid request size: %d min: %d type: %d\n",
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
            if (RT_FAILURE(rc))
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: could not allocate request structure! rc = %d\n", rc));
                rc = -EFAULT;
            }
        }
        if (0 == rc)
        {
            /* now get the full request */
            if (copy_from_user(reqFull, (void*)arg, cbRequestSize))
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: failed to fetch full request from user space!\n"));
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
            if (RT_FAILURE(rrc) || RT_FAILURE(reqFull->rc))
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: request execution failed!\n"));
                rc = RT_FAILURE(rrc) ? -RTErrConvertToErrno(rrc)
                                       : -RTErrConvertToErrno(reqFull->rc);
            }
            else
            {
                /* success, copy the result data to user space */
                if (copy_to_user((void*)arg, (void*)reqFull, cbRequestSize))
                {
                    LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: error copying request result to user space!\n"));
                    rc = -EFAULT;
                }
            }
        }
        if (NULL != reqFull)
            VbglGRFree(reqFull);
        IOCTL_VMM_EXIT(arg);
    }
    else if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL(0))
             == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        /* Do the HGCM call using the Vbgl bits */
        IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CALL", arg);
        rc = vboxadd_hgcm_call(arg, _IOC_SIZE(cmd));
        IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CALL", arg);
    }
    else if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMED(0))
             == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        /* Do the HGCM call using the Vbgl bits */
        IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CALL_TIMED", arg);
        rc = vboxadd_hgcm_call_timed(arg, _IOC_SIZE(cmd));
        IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CALL_TIMED", arg);
    }
    else
    {
        switch (cmd)
        {
            case VBOXGUEST_IOCTL_WAITEVENT:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_WAITEVENT", arg);
                rc = vboxadd_wait_event((void *) arg);
                IOCTL_EXIT("VBOXGUEST_IOCTL_WAITEVENT", arg);
                break;
            case VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS", arg);
                ++vboxDev->u32GuestInterruptions;
                IOCTL_EXIT("VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS", arg);
                break;
            case VBOXGUEST_IOCTL_HGCM_CONNECT:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CONNECT", arg);
                rc = vboxadd_hgcm_connect(filp, arg);
                IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CONNECT", arg);
                break;
            case VBOXGUEST_IOCTL_HGCM_DISCONNECT:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_DISCONNECT", arg);
                vboxadd_hgcm_disconnect(filp, arg);
                IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_DISCONNECT", arg);
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

/**
 * IOCTL handler for vboxuser
 * @todo currently this is just a copy of vboxadd_ioctl.  We should
 *       decide if we wish to restrict this.  If we do, we should remove
 *       the more general ioctls (HGCM call, VMM device request) and
 *       replace them with specific ones.  If not, then we should just
 *       make vboxadd world readable and writable or something.
 */
static int vboxuser_ioctl(struct inode *inode, struct file *filp,
                          unsigned int cmd, unsigned long arg)
{
    int rc = 0;

    /* Deal with variable size ioctls first. */
#ifdef DEBUG  /* Only allow random user applications to spam the log in 
               * debug additions builds */
    if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_LOG(0))
        == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        char *pszMessage;

        IOCTL_LOG_ENTRY(arg);
        pszMessage = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
        if (NULL == pszMessage)
        {
            LogRelFunc(("VBOXGUEST_IOCTL_LOG: cannot allocate %d bytes of memory!\n",
                         _IOC_SIZE(cmd)));
            rc = -ENOMEM;
        }
        if (   (0 == rc)
            && copy_from_user(pszMessage, (void*)arg, _IOC_SIZE(cmd)))
        {
            LogRelFunc(("VBOXGUEST_IOCTL_LOG: copy_from_user failed!\n"));
            rc = -EFAULT;
        }
        if (0 == rc)
        {
            Log(("%.*s", _IOC_SIZE(cmd), pszMessage));
        }
        if (NULL != pszMessage)
        {
            kfree(pszMessage);
        }
        IOCTL_LOG_EXIT(arg);
    }
    else
#endif
    if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_VMMREQUEST(0))
             == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        VMMDevRequestHeader reqHeader;
        VMMDevRequestHeader *reqFull = NULL;
        size_t cbRequestSize;
        size_t cbVanillaRequestSize;

        IOCTL_VMM_ENTRY(arg);
        if (copy_from_user(&reqHeader, (void*)arg, sizeof(reqHeader)))
        {
            LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: copy_from_user failed for vmm request!\n"));
            rc = -EFAULT;
        }
        if (0 == rc)
        {
            /* get the request size */
            cbVanillaRequestSize = vmmdevGetRequestSize(reqHeader.requestType);
            if (!cbVanillaRequestSize)
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: invalid request type: %d\n",
                        reqHeader.requestType));
                rc = -EINVAL;
            }
        }
        if (0 == rc)
        {
            cbRequestSize = reqHeader.size;
            if (cbRequestSize < cbVanillaRequestSize)
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: invalid request size: %d min: %d type: %d\n",
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
            if (RT_FAILURE(rc))
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: could not allocate request structure! rc = %d\n", rc));
                rc = -EFAULT;
            }
        }
        if (0 == rc)
        {
            /* now get the full request */
            if (copy_from_user(reqFull, (void*)arg, cbRequestSize))
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: failed to fetch full request from user space!\n"));
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
            if (RT_FAILURE(rrc) || RT_FAILURE(reqFull->rc))
            {
                LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: request execution failed!\n"));
                rc = RT_FAILURE(rrc) ? -RTErrConvertToErrno(rrc)
                                       : -RTErrConvertToErrno(reqFull->rc);
            }
            else
            {
                /* success, copy the result data to user space */
                if (copy_to_user((void*)arg, (void*)reqFull, cbRequestSize))
                {
                    LogRelFunc(("VBOXGUEST_IOCTL_VMMREQUEST: error copying request result to user space!\n"));
                    rc = -EFAULT;
                }
            }
        }
        if (NULL != reqFull)
            VbglGRFree(reqFull);
        IOCTL_VMM_EXIT(arg);
    }
    else if (    VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL(0))
         == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        /* Do the HGCM call using the Vbgl bits */
        IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CALL", arg);
        rc = vboxadd_hgcm_call(arg, _IOC_SIZE(cmd));
        IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CALL", arg);
    }
    else if (   VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMED(0))
             == VBOXGUEST_IOCTL_STRIP_SIZE(cmd))
    {
        /* Do the HGCM call using the Vbgl bits */
        IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CALL_TIMED", arg);
        rc = vboxadd_hgcm_call_timed(arg, _IOC_SIZE(cmd));
        IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CALL_TIMED", arg);
    }
    else
    {
        switch (cmd)
        {
            case VBOXGUEST_IOCTL_WAITEVENT:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_WAITEVENT", arg);
                rc = vboxadd_wait_event((void *) arg);
                IOCTL_EXIT("VBOXGUEST_IOCTL_WAITEVENT", arg);
                break;
            case VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS", arg);
                ++vboxDev->u32GuestInterruptions;
                IOCTL_EXIT("VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS", arg);
                break;
            case VBOXGUEST_IOCTL_HGCM_CONNECT:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_CONNECT", arg);
                rc = vboxadd_hgcm_connect(filp, arg);
                IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_CONNECT", arg);
                break;
            case VBOXGUEST_IOCTL_HGCM_DISCONNECT:
                IOCTL_ENTRY("VBOXGUEST_IOCTL_HGCM_DISCONNECT", arg);
                vboxadd_hgcm_disconnect(filp, arg);
                IOCTL_EXIT("VBOXGUEST_IOCTL_HGCM_DISCONNECT", arg);
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

/**
 * Poll function.  This returns "ready to read" if the guest is in absolute
 * mouse pointer mode and the pointer position has changed since the last
 * poll.
 */
unsigned int
vboxadd_poll (struct file *file, poll_table *wait)
{
    int result = 0;
    poll_wait(file, &vboxDev->eventq, wait);
    if (vboxDev->u32Events & VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
        result = (POLLIN | POLLRDNORM);
    vboxDev->u32Events &= ~VMMDEV_EVENT_MOUSE_POSITION_CHANGED;
    return result;
}

/** Asynchronous notification activation method. */
static int
vboxadd_fasync(int fd, struct file *file, int mode)
{
    return fasync_helper(fd, file, mode, &vboxDev->async_queue);
}

/**
 * Dummy read function - we only supply this because we implement poll and
 * fasync.
 */
static ssize_t
vboxadd_read (struct file *file, char *buf, size_t count, loff_t *loff)
{
    if (0 == count || *loff != 0)
    {
        return -EINVAL;
    }
    buf[0] = 0;
    return 1;
}

/**
 * File close handler for vboxadd.  Clean up any HGCM connections associated
 * with the open file which might still be open.
 */
static int vboxadd_release(struct inode *inode, struct file * filp)
{
    vboxadd_unregister_all_hgcm_connections(filp);
    /* Deactivate our asynchronous queue. */
    vboxadd_fasync(-1, filp, 0);
    return 0;
}

/**
 * File close handler for vboxuser.  Clean up any HGCM connections associated
 * with the open file which might still be open.
 */
static int vboxuser_release(struct inode *inode, struct file * filp)
{
    vboxadd_unregister_all_hgcm_connections(filp);
    return 0;
}

/** file operations for the vboxadd device */
static struct file_operations vboxadd_fops =
{
    .owner   = THIS_MODULE,
    .open    = vboxadd_open,
    .ioctl   = vboxadd_ioctl,
    .poll    = vboxadd_poll,
    .fasync  = vboxadd_fasync,
    .read    = vboxadd_read,
    .release = vboxadd_release,
    .llseek  = no_llseek
};

/** Miscellaneous device allocation for vboxadd */
static struct miscdevice gMiscVBoxAdd =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       VBOXADD_NAME,
    fops:       &vboxadd_fops
};

/** file operations for the vboxuser device */
static struct file_operations vboxuser_fops =
{
    .owner   = THIS_MODULE,
    .open    = vboxadd_open,
    .ioctl   = vboxuser_ioctl,
    .release = vboxuser_release,
    .llseek  = no_llseek
};

/** Miscellaneous device allocation for vboxuser */
static struct miscdevice gMiscVBoxUser =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       VBOXUSER_NAME,
    fops:       &vboxuser_fops
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
        if (RT_SUCCESS(rcVBox) && RT_SUCCESS(vboxDev->irqAckRequest->header.rc))
        {
            if (RT_LIKELY (vboxDev->irqAckRequest->events))
            {
                vboxDev->u32Events |= vboxDev->irqAckRequest->events;
                if (  vboxDev->irqAckRequest->events
                    & VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
                    kill_fasync(&vboxDev->async_queue, SIGIO, POLL_IN);
                wake_up (&vboxDev->eventq);
            }
        }
        else
        {
            /* impossible... */
            LogRelFunc(("IRQ was not acknowledged! rc = %Rrc, header.rc = %Rrc\n",
                        rcVBox, vboxDev->irqAckRequest->header.rc));
            BUG ();
        }

        /* it was ours! */
        fIRQTaken = 1;
    }
#ifdef IRQ_DEBUG
    else
    {
        /* we might be attached to a shared interrupt together with another device. */
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
    if (RT_FAILURE(rcVBox))
    {
        LogRelFunc(("failed to allocate hypervisor info structure! rc = %Rrc\n", rcVBox));
        goto bail_out;
    }
    /* query the hypervisor information */
    rcVBox = VbglGRPerform(&req->header);
    if (RT_SUCCESS(rcVBox) && RT_SUCCESS(req->header.rc))
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
                req->hypervisorStart    = (VMMDEVHYPPTR32)(uintptr_t)RT_ALIGN_P(hypervisorArea, 0x400000);
                req->header.requestType = VMMDevReq_SetHypervisorInfo;
                req->header.rc          = VERR_GENERAL_FAILURE;
                rcVBox = VbglGRPerform(&req->header);
                if (RT_SUCCESS(rcVBox) && RT_SUCCESS(req->header.rc))
                {
                    /* store mapping for future unmapping */
                    vboxDev->hypervisorStart = hypervisorArea;
                    vboxDev->hypervisorSize  = hypervisorSize;
                }
                else
                {
                    LogRelFunc(("failed to set hypervisor region! rc = %Rrc, header.rc = %Rrc\n",
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
        LogRelFunc(("failed to query hypervisor info! rc = %Rrc, header.rc = %Rrc\n",
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
    if (RT_FAILURE(rcVBox))
    {
        LogRelFunc(("failed to allocate hypervisor info structure! rc = %Rrc\n", rcVBox));
        goto bail_out;
    }
    /* reset the hypervisor information */
    req->hypervisorStart = 0;
    req->hypervisorSize  = 0;
    rcVBox = VbglGRPerform(&req->header);
    if (RT_SUCCESS(rcVBox) && RT_SUCCESS(req->header.rc))
    {
        /* now we can free the associated IO space mapping */
        iounmap(vboxDev->hypervisorStart);
        vboxDev->hypervisorStart = 0;
    }
    else
    {
        LogRelFunc(("failed to reset hypervisor info! rc = %Rrc, header.rc = %Rrc\n",
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
        {
            /* Unregister notifications when the host absolute pointer
             * position changes. */
            VBoxGuestFilterMaskInfo info;
            info.u32OrMask = 0;
            info.u32NotMask = VMMDEV_EVENT_MOUSE_POSITION_CHANGED;
            vboxadd_control_filter_mask(&info);
        }
        /* Detach from IRQ before cleaning up! */
        if (vboxDev->irq)
            free_irq(vboxDev->irq, vboxDev);
        if (vboxDev->hypervisorStart)
            vboxadd_free_hypervisor();
        if (vboxDev->irqAckRequest)
        {
            VbglGRFree(&vboxDev->irqAckRequest->header);
            VbglTerminate();
        }
        if (vboxDev->pVMMDevMemory)
            iounmap(vboxDev->pVMMDevMemory);
        if (vboxDev->vmmdevmem)
            release_mem_region(vboxDev->vmmdevmem, vboxDev->vmmdevmem_size);
        kfree(vboxDev);
        vboxDev = NULL;
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#define PCI_DEV_GET(v,d,p) pci_get_device(v,d,p)
#define PCI_DEV_PUT(x) pci_dev_put(x)
#else
#define PCI_DEV_GET(v,d,p) pci_find_device(v,d,p)
#define PCI_DEV_PUT(x)     do {} while(0)
#endif

/**
 * Module initialization
 *
 */
static __init int init(void)
{
    int rc = 0, rcVBox = VINF_SUCCESS;
    bool fHaveVBoxAdd = false, fHaveVBoxUser = false, fHaveGuestLib = false;
    struct pci_dev *pcidev = NULL;

    rcVBox = vboxadd_cmc_init();
    if (RT_FAILURE(rcVBox))
    {
        printk (KERN_ERR "vboxadd: could not init cmc, VBox error code %d.\n", rcVBox);
        rc = -RTErrConvertToErrno(rcVBox);
    }

    /* Detect PCI device */
    if (!rc)
    {
        pcidev = PCI_DEV_GET(VMMDEV_VENDORID, VMMDEV_DEVICEID, pcidev);
        if (!pcidev)
        {
            printk(KERN_ERR "vboxadd: VirtualBox Guest PCI device not found.\n");
            rc = -ENODEV;
        }
    }

    if (!rc)
    {
        rc = pci_enable_device (pcidev);
        if (rc)
            LogRel(("vboxadd: could not enable device: %d\n", rc));
    }
    if (!rc)
        LogRel(("Starting VirtualBox version %s Guest Additions\n",
                VBOX_VERSION_STRING));

    /* Register vboxadd */
    if (!rc && vbox_major > 0)  /* Register as a character device in this case */
    {
        rc = register_chrdev(vbox_major, VBOXADD_NAME, &vboxadd_fops);
        if (rc)  /* As we pass a non-zero major, rc should be zero on success. */
            LogRel(("vboxadd: register_chrdev failed: vbox_major: %d, err = %d\n",
                    vbox_major, rc));
    }
    else if (!rc)  /* Register as a miscellaneous device otherwise */
    {
        rc = misc_register(&gMiscVBoxAdd);
        if (rc)
            LogRel(("vboxadd: misc_register failed for %s (rc=%d)\n",
                    VBOXADD_NAME, rc));
    }
    if (!rc)
        fHaveVBoxAdd = true;

    /* Register our user session device */
    if (!rc)
    {
        rc = misc_register(&gMiscVBoxUser);
        if (rc)
            LogRel(("vboxadd: misc_register failed for %s (rc=%d)\n",
                    VBOXUSER_NAME, rc));
    }
    if (!rc)
        fHaveVBoxUser = true;

    /* allocate and initialize device extension */
    if (!rc)
    {
        vboxDev = kmalloc(sizeof(*vboxDev), GFP_KERNEL);
        if (vboxDev)
            memset(vboxDev, 0, sizeof(*vboxDev));
        else
        {
            LogRel(("vboxadd: could not allocate private device structure\n"));
            rc = -ENOMEM;
        }
    }

    if (!rc)
    {
        /* get the IO port region */
        vboxDev->io_port = pci_resource_start(pcidev, 0);

        /* get the memory region */
        vboxDev->vmmdevmem = pci_resource_start(pcidev, 1);
        vboxDev->vmmdevmem_size = pci_resource_len(pcidev, 1);

        /* all resources found? */
        if (!vboxDev->io_port || !vboxDev->vmmdevmem || !vboxDev->vmmdevmem_size)
        {
            LogRel(("vboxadd: did not find expected hardware resources\n"));
            rc = -ENXIO;
        }
    }

    /* request ownership of adapter memory */
    if (!rc && !request_mem_region(vboxDev->vmmdevmem, vboxDev->vmmdevmem_size,
                                   VBOXADD_NAME))
    {
        LogRel(("vboxadd: failed to obtain adapter memory\n"));
        rc = -EBUSY;
    }

    /* map adapter memory into kernel address space and check version */
    if (!rc)
    {
        vboxDev->pVMMDevMemory = (VMMDevMemory *) ioremap(vboxDev->vmmdevmem,
                                                      vboxDev->vmmdevmem_size);
        if (!vboxDev->pVMMDevMemory)
        {
            LogRel(("vboxadd: ioremap failed\n"));
            rc = -ENOMEM;
        }
    }

    if (!rc && (vboxDev->pVMMDevMemory->u32Version != VMMDEV_MEMORY_VERSION))
    {
        LogRel(("vboxadd: invalid VMM device memory version! (got 0x%x, expected 0x%x)\n",
                   vboxDev->pVMMDevMemory->u32Version, VMMDEV_MEMORY_VERSION));
        rc = -ENXIO;
    }

    /* initialize ring 0 guest library */
    if (!rc)
    {
        rcVBox = VbglInit(vboxDev->io_port, vboxDev->pVMMDevMemory);
        if (RT_FAILURE(rcVBox))
        {
            LogRel(("vboxadd: could not initialize VBGL subsystem: %Rrc\n",
                    rcVBox));
            rc = -RTErrConvertToErrno(rcVBox);
        }
    }
    if (!rc)
        fHaveGuestLib = true;

    /* report guest information to host, this must be done as the very first request */
    if (!rc)
    {
        VMMDevReportGuestInfo *infoReq = NULL;

        rcVBox = VbglGRAlloc((VMMDevRequestHeader**)&infoReq,
                             sizeof(VMMDevReportGuestInfo), VMMDevReq_ReportGuestInfo);
        if (RT_FAILURE(rcVBox))
        {
            LogRel(("vboxadd: could not allocate request structure: %Rrc\n", rcVBox));
            rc = -RTErrConvertToErrno(rcVBox);
        }
        /* report guest version to host, the VMMDev requires that to be done
         * before any other VMMDev operations. */
        if (infoReq)
        {
            infoReq->guestInfo.additionsVersion = VMMDEV_VERSION;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 0)
            infoReq->guestInfo.osType = VBOXOSTYPE_Linux26;
#else
            infoReq->guestInfo.osType = VBOXOSTYPE_Linux24;
#endif
            rcVBox = VbglGRPerform(&infoReq->header);
        }
        if (   infoReq 
            && (   RT_FAILURE(rcVBox) 
                || RT_FAILURE(infoReq->header.rc)))
        {
            LogRel(("vboxadd: error reporting guest information to host: %Rrc, header: %Rrc\n",
                        rcVBox, infoReq->header.rc));
            rc = RT_FAILURE(rcVBox) ? -RTErrConvertToErrno(rcVBox)
                                    : -RTErrConvertToErrno(infoReq->header.rc);
        }
        if (infoReq)
            VbglGRFree(&infoReq->header);
    }

    /* Unset the graphics capability until/unless X is loaded. */
    /** @todo check the error code once we bump the additions version.
              For now we ignore it for compatibility with older hosts. */
    if (!rc)
    {
        VMMDevReqGuestCapabilities2 *vmmreqGuestCaps;

        rcVBox = VbglGRAlloc((VMMDevRequestHeader**)&vmmreqGuestCaps,
                              sizeof(VMMDevReqGuestCapabilities2),
                              VMMDevReq_SetGuestCapabilities);
        if (RT_FAILURE(rcVBox))
        {
            LogRel(("vboxadd: could not allocate request structure: %Rrc\n",
                    rcVBox));
            rc = -RTErrConvertToErrno(rcVBox);
        }
        else
        {
            vmmreqGuestCaps->u32OrMask = 0;
            vmmreqGuestCaps->u32NotMask = VMMDEV_GUEST_SUPPORTS_GRAPHICS;
            rcVBox = VbglGRPerform(&vmmreqGuestCaps->header);
            if (RT_FAILURE(rcVBox))
            {
                LogRel(("vboxadd: could not allocate request structure: %Rrc\n",
                        rcVBox));
                rc = -RTErrConvertToErrno(rcVBox);
            }
            VbglGRFree(&vmmreqGuestCaps->header);
        }
    }

    /* perform hypervisor address space reservation */
    if (!rc && vboxadd_reserve_hypervisor())
    {
        /* we just ignore the error, no address window reservation, non fatal */
    }

    /* allocate a VMM request structure for use in the ISR */
    if (!rc)
    {
        rcVBox = VbglGRAlloc((VMMDevRequestHeader**)&vboxDev->irqAckRequest,
                             sizeof(VMMDevEvents), VMMDevReq_AcknowledgeEvents);
        if (RT_FAILURE(rcVBox))
        {
            LogRel(("vboxadd: could not allocate request structure: %Rrc\n",
                    rcVBox));
            rc = -RTErrConvertToErrno(rcVBox);
        }
    }

    /* get ISR */
    if (!rc)
    {
        rc = request_irq(pcidev->irq, vboxadd_irq_handler,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
                          IRQF_SHARED,
#else
                          SA_SHIRQ,
#endif
                          VBOXADD_NAME, vboxDev);
        if (rc)
            LogRel(("vboxadd: could not request IRQ %d, err: %d\n", pcidev->irq, rc));
        else
            vboxDev->irq = pcidev->irq;
    }

    if (!rc)
    {
        VBoxGuestFilterMaskInfo info;

        init_waitqueue_head (&vboxDev->eventq);
        /* Register for notification when the host absolute pointer position
         * changes. */
        info.u32OrMask = VMMDEV_EVENT_MOUSE_POSITION_CHANGED;
        info.u32NotMask = 0;
        rcVBox = vboxadd_control_filter_mask(&info);
        if (RT_FAILURE(rcVBox))
        {
            LogRel(("vboxadd: failed to register for VMMDEV_EVENT_MOUSE_POSITION_CHANGED events: %Rrc\n",
                    rcVBox));
            rc = -RTErrConvertToErrno(rcVBox);
        }
    }

    if (!rc)
    {
        /* some useful information for the user but don't show this on the console */
        LogRel(("VirtualBox device settings: major %d, IRQ %d, "
                "I/O port 0x%x, MMIO at 0x%x (size 0x%x), "
                "hypervisor window at 0x%p (size 0x%x)\n",
                vbox_major, vboxDev->irq, vboxDev->io_port,
                vboxDev->vmmdevmem, vboxDev->vmmdevmem_size,
                vboxDev->hypervisorStart, vboxDev->hypervisorSize));
        printk(KERN_DEBUG "vboxadd: Successfully loaded version "
                VBOX_VERSION_STRING " (interface " xstr(VMMDEV_VERSION) ")\n");
    }
    else  /* Clean up on failure */
    {
        if (fHaveGuestLib)
            VbglTerminate();
        if (vboxDev)
            free_resources();
        if (fHaveVBoxUser)
            misc_deregister(&gMiscVBoxUser);
        if (fHaveVBoxAdd && vbox_major > 0)
            unregister_chrdev(vbox_major, VBOXADD_NAME);
        else if (fHaveVBoxAdd)
            misc_deregister(&gMiscVBoxAdd);
    }

    /* We always release this.  Presumably because we no longer need to do
     * anything with the device structure. */
    if (pcidev)
        PCI_DEV_PUT(pcidev);

    return rc;
}

/**
 * Module termination
 *
 */
static __exit void fini(void)
{
    misc_deregister(&gMiscVBoxUser);
    if (vbox_major > 0)
        unregister_chrdev(vbox_major, VBOXADD_NAME);
    else
        misc_deregister(&gMiscVBoxAdd);
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



/*
 * Local Variables:
 * c-mode: bsd
 * indent-tabs-mode: nil
 * c-plusplus: evil
 * End:
 */
