/* $Id$ */
/** @file
 * VirtualBox Support Driver - Darwin Specific Code.
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
/*
 * Deal with conflicts first.
 * PVM - BSD mess, that FreeBSD has correct a long time ago.
 * iprt/types.h before sys/param.h - prevents UINT32_C and friends.
 */
#include <iprt/types.h>
#include <sys/param.h>
#undef PVM

#include <IOKit/IOLib.h> /* Assert as function */

#include "../SUPDrvInternal.h"
#include <VBox/version.h>
#include <iprt/asm.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/process.h>
#include <iprt/alloc.h>
#include <iprt/power.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <mach/kmod.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#ifdef VBOX_WITH_HOST_VMX
RT_C_DECLS_BEGIN
# include <i386/vmx.h>
RT_C_DECLS_END
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** The module name. */
#define DEVICE_NAME    "vboxdrv"



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
static kern_return_t    VBoxDrvDarwinStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    VBoxDrvDarwinStop(struct kmod_info *pKModInfo, void *pvData);

static int              VBoxDrvDarwinOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int              VBoxDrvDarwinClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int              VBoxDrvDarwinIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess);
static int              VBoxDrvDarwinIOCtlSlow(PSUPDRVSESSION pSession, u_long iCmd, caddr_t pData, struct proc *pProcess);

static int              VBoxDrvDarwinErr2DarwinErr(int rc);

static IOReturn         VBoxDrvDarwinSleepHandler(void *pvTarget, void *pvRefCon, UInt32 uMessageType, IOService *pProvider, void *pvMessageArgument, vm_size_t argSize);
RT_C_DECLS_END


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The service class.
 * This is just a formality really.
 */
class org_virtualbox_SupDrv : public IOService
{
    OSDeclareDefaultStructors(org_virtualbox_SupDrv);

public:
    virtual bool init(OSDictionary *pDictionary = 0);
    virtual void free(void);
    virtual bool start(IOService *pProvider);
    virtual void stop(IOService *pProvider);
    virtual IOService *probe(IOService *pProvider, SInt32 *pi32Score);
    virtual bool terminate(IOOptionBits fOptions);
};

OSDefineMetaClassAndStructors(org_virtualbox_SupDrv, IOService);


/**
 * An attempt at getting that clientDied() notification.
 * I don't think it'll work as I cannot figure out where/what creates the correct
 * port right.
 */
class org_virtualbox_SupDrvClient : public IOUserClient
{
    OSDeclareDefaultStructors(org_virtualbox_SupDrvClient);

private:
    PSUPDRVSESSION          m_pSession;     /**< The session. */
    task_t                  m_Task;         /**< The client task. */
    org_virtualbox_SupDrv  *m_pProvider;    /**< The service provider. */

public:
    virtual bool initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type);
    virtual bool start(IOService *pProvider);
    static  void sessionClose(RTPROCESS Process);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    virtual bool terminate(IOOptionBits fOptions = 0);
    virtual bool finalize(IOOptionBits fOptions);
    virtual void stop(IOService *pProvider);
};

OSDefineMetaClassAndStructors(org_virtualbox_SupDrvClient, IOUserClient);



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Declare the module stuff.
 */
RT_C_DECLS_BEGIN
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VBoxDrv, VBOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain = VBoxDrvDarwinStart;
DECLHIDDEN(kmod_stop_func_t *)  _antimain = VBoxDrvDarwinStop;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
RT_C_DECLS_END


/**
 * Device extention & session data association structure.
 */
static SUPDRVDEVEXT     g_DevExt;

/**
 * The character device switch table for the driver.
 */
static struct cdevsw    g_DevCW =
{
    /** @todo g++ doesn't like this syntax - it worked with gcc before renaming to .cpp. */
    /*.d_open  = */VBoxDrvDarwinOpen,
    /*.d_close = */VBoxDrvDarwinClose,
    /*.d_read  = */eno_rdwrt,
    /*.d_write = */eno_rdwrt,
    /*.d_ioctl = */VBoxDrvDarwinIOCtl,
    /*.d_stop  = */eno_stop,
    /*.d_reset = */eno_reset,
    /*.d_ttys  = */NULL,
    /*.d_select= */eno_select,
    /*.d_mmap  = */eno_mmap,
    /*.d_strategy = */eno_strat,
    /*.d_getc  = */eno_getc,
    /*.d_putc  = */eno_putc,
    /*.d_type  = */0
};

/** Major device number. */
static int              g_iMajorDeviceNo = -1;
/** Registered devfs device handle. */
static void            *g_hDevFsDevice = NULL;

/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK       g_Spinlock = NIL_RTSPINLOCK;
/** Hash table */
static PSUPDRVSESSION   g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(pid)     ((pid) % RT_ELEMENTS(g_apSessionHashTab))
/** The number of open sessions. */
static int32_t volatile g_cSessions = 0;
/** The notifier handle for the sleep callback handler. */
static IONotifier *g_pSleepNotifier = NULL;



/**
 * Start the kernel module.
 */
static kern_return_t    VBoxDrvDarwinStart(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;
#ifdef DEBUG
    printf("VBoxDrvDarwinStart\n");
#endif

    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize the device extension.
         */
        rc = supdrvInitDevExt(&g_DevExt, sizeof(SUPDRVSESSION));
        if (RT_SUCCESS(rc))
        {
            /*
             * Initialize the session hash table.
             */
            memset(g_apSessionHashTab, 0, sizeof(g_apSessionHashTab)); /* paranoia */
            rc = RTSpinlockCreate(&g_Spinlock);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Registering ourselves as a character device.
                 */
                g_iMajorDeviceNo = cdevsw_add(-1, &g_DevCW);
                if (g_iMajorDeviceNo >= 0)
                {
#ifdef VBOX_WITH_HARDENING
                    g_hDevFsDevice = devfs_make_node(makedev(g_iMajorDeviceNo, 0), DEVFS_CHAR,
                                                     UID_ROOT, GID_WHEEL, 0600, DEVICE_NAME);
#else
                    g_hDevFsDevice = devfs_make_node(makedev(g_iMajorDeviceNo, 0), DEVFS_CHAR,
                                                     UID_ROOT, GID_WHEEL, 0666, DEVICE_NAME);
#endif
                    if (g_hDevFsDevice)
                    {
                        LogRel(("VBoxDrv: version " VBOX_VERSION_STRING " r%d; IOCtl version %#x; IDC version %#x; dev major=%d\n",
                                VBOX_SVN_REV, SUPDRV_IOC_VERSION, SUPDRV_IDC_VERSION, g_iMajorDeviceNo));

                        /* Register a sleep/wakeup notification callback */
                        g_pSleepNotifier = registerPrioritySleepWakeInterest(&VBoxDrvDarwinSleepHandler, &g_DevExt, NULL);
                        if (g_pSleepNotifier == NULL)
                            LogRel(("VBoxDrv: register for sleep/wakeup events failed\n"));

                        return KMOD_RETURN_SUCCESS;
                    }

                    LogRel(("VBoxDrv: devfs_make_node(makedev(%d,0),,,,%s) failed\n", g_iMajorDeviceNo, DEVICE_NAME));
                    cdevsw_remove(g_iMajorDeviceNo, &g_DevCW);
                    g_iMajorDeviceNo = -1;
                }
                else
                    LogRel(("VBoxDrv: cdevsw_add failed (%d)\n", g_iMajorDeviceNo));
                RTSpinlockDestroy(g_Spinlock);
                g_Spinlock = NIL_RTSPINLOCK;
            }
            else
                LogRel(("VBoxDrv: RTSpinlockCreate failed (rc=%d)\n", rc));
            supdrvDeleteDevExt(&g_DevExt);
        }
        else
            printf("VBoxDrv: failed to initialize device extension (rc=%d)\n", rc);
        RTR0TermForced();
    }
    else
        printf("VBoxDrv: failed to initialize IPRT (rc=%d)\n", rc);

    memset(&g_DevExt, 0, sizeof(g_DevExt));
    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t    VBoxDrvDarwinStop(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;
    LogFlow(("VBoxDrvDarwinStop\n"));

    /** @todo I've got a nagging feeling that we'll have to keep track of users and refuse
     * unloading if we're busy. Investigate and implement this! */

    /*
     * Undo the work done during start (in reverse order).
     */
    if (g_pSleepNotifier)
    {
        g_pSleepNotifier->remove();
        g_pSleepNotifier = NULL;
    }

    devfs_remove(g_hDevFsDevice);
    g_hDevFsDevice = NULL;

    rc = cdevsw_remove(g_iMajorDeviceNo, &g_DevCW);
    Assert(rc == g_iMajorDeviceNo);
    g_iMajorDeviceNo = -1;

    supdrvDeleteDevExt(&g_DevExt);

    rc = RTSpinlockDestroy(g_Spinlock);
    AssertRC(rc);
    g_Spinlock = NIL_RTSPINLOCK;

    RTR0TermForced();

    memset(&g_DevExt, 0, sizeof(g_DevExt));
#ifdef DEBUG
    printf("VBoxDrvDarwinStop - done\n");
#endif
    return KMOD_RETURN_SUCCESS;
}


/**
 * Device open. Called on open /dev/vboxdrv
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxDrvDarwinOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
#ifdef DEBUG_DARWIN_GIP
    char szName[128];
    szName[0] = '\0';
    proc_name(proc_pid(pProcess), szName, sizeof(szName));
    Log(("VBoxDrvDarwinOpen: pid=%d '%s'\n", proc_pid(pProcess), szName));
#endif

    /*
     * Find the session created by org_virtualbox_SupDrvClient, fail
     * if no such session, and mark it as opened. We set the uid & gid
     * here too, since that is more straight forward at this point.
     */
    int             rc = VINF_SUCCESS;
    PSUPDRVSESSION  pSession = NULL;
    kauth_cred_t    pCred = kauth_cred_proc_ref(pProcess);
    if (pCred)
    {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
        RTUID           Uid = kauth_cred_getruid(pCred);
        RTGID           Gid = kauth_cred_getrgid(pCred);
#else
        RTUID           Uid = pCred->cr_ruid;
        RTGID           Gid = pCred->cr_rgid;
#endif
        RTPROCESS       Process = RTProcSelf();
        unsigned        iHash = SESSION_HASH(Process);
        RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);

        pSession = g_apSessionHashTab[iHash];
        if (pSession && pSession->Process != Process)
        {
            do pSession = pSession->pNextHash;
            while (pSession && pSession->Process != Process);
        }
        if (pSession)
        {
            if (!pSession->fOpened)
            {
                pSession->fOpened = true;
                pSession->Uid = Uid;
                pSession->Gid = Gid;
            }
            else
                rc = VERR_ALREADY_LOADED;
        }
        else
            rc = VERR_GENERAL_FAILURE;

        RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
        kauth_cred_unref(&pCred);
#else  /* 10.4 */
        /* The 10.4u SDK headers and 10.4.11 kernel source have inconsistent definitions
           of kauth_cred_unref(), so use the other (now deprecated) API for releasing it. */
        kauth_cred_rele(pCred);
#endif /* 10.4 */
    }
    else
        rc = VERR_INVALID_PARAMETER;

#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("VBoxDrvDarwinOpen: pid=%d '%s' pSession=%p rc=%d\n", proc_pid(pProcess), szName, pSession, rc));
#else
    Log(("VBoxDrvDarwinOpen: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, proc_pid(pProcess)));
#endif
    return VBoxDrvDarwinErr2DarwinErr(rc);
}


/**
 * Close device.
 */
static int VBoxDrvDarwinClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    Log(("VBoxDrvDarwinClose: pid=%d\n", (int)RTProcSelf()));
    Assert(proc_pid(pProcess) == (int)RTProcSelf());

    /*
     * Hand the session closing to org_virtualbox_SupDrvClient.
     */
    org_virtualbox_SupDrvClient::sessionClose(RTProcSelf());
    return 0;
}


/**
 * Device I/O Control entry point.
 *
 * @returns Darwin for slow IOCtls and VBox status code for the fast ones.
 * @param   Dev         The device number (major+minor).
 * @param   iCmd        The IOCtl command.
 * @param   pData       Pointer to the data (if any it's a SUPDRVIOCTLDATA (kernel copy)).
 * @param   fFlags      Flag saying we're a character device (like we didn't know already).
 * @param   pProcess    The process issuing this request.
 */
static int VBoxDrvDarwinIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess)
{
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = proc_pid(pProcess);
    const unsigned      iHash = SESSION_HASH(Process);
    PSUPDRVSESSION      pSession;

    /*
     * Find the session.
     */
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession && pSession->Process != Process)
    {
        do pSession = pSession->pNextHash;
        while (pSession && pSession->Process != Process);
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        OSDBGPRINT(("VBoxDrvDarwinIOCtl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d iCmd=%#lx\n",
                    (int)Process, iCmd));
        return EINVAL;
    }

    /*
     * Deal with the two high-speed IOCtl that takes it's arguments from
     * the session and iCmd, and only returns a VBox status code.
     */
    if (    iCmd == SUP_IOCTL_FAST_DO_RAW_RUN
        ||  iCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
        ||  iCmd == SUP_IOCTL_FAST_DO_NOP)
        return supdrvIOCtlFast(iCmd, *(uint32_t *)pData, &g_DevExt, pSession);
    return VBoxDrvDarwinIOCtlSlow(pSession, iCmd, pData, pProcess);
}


/**
 * Worker for VBoxDrvDarwinIOCtl that takes the slow IOCtl functions.
 *
 * @returns Darwin errno.
 *
 * @param pSession  The session.
 * @param iCmd      The IOCtl command.
 * @param pData     Pointer to the kernel copy of the SUPDRVIOCTLDATA buffer.
 * @param pProcess  The calling process.
 */
static int VBoxDrvDarwinIOCtlSlow(PSUPDRVSESSION pSession, u_long iCmd, caddr_t pData, struct proc *pProcess)
{
    LogFlow(("VBoxDrvDarwinIOCtlSlow: pSession=%p iCmd=%p pData=%p pProcess=%p\n", pSession, iCmd, pData, pProcess));


    /*
     * Buffered or unbuffered?
     */
    PSUPREQHDR pHdr;
    user_addr_t pUser = 0;
    void *pvPageBuf = NULL;
    uint32_t cbReq = IOCPARM_LEN(iCmd);
    if ((IOC_DIRMASK & iCmd) == IOC_INOUT)
    {
        pHdr = (PSUPREQHDR)pData;
        if (RT_UNLIKELY(cbReq < sizeof(*pHdr)))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: cbReq=%#x < %#x; iCmd=%#lx\n", cbReq, (int)sizeof(*pHdr), iCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY((pHdr->fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: bad magic fFlags=%#x; iCmd=%#lx\n", pHdr->fFlags, iCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY(    RT_MAX(pHdr->cbIn, pHdr->cbOut) != cbReq
                        ||  pHdr->cbIn < sizeof(*pHdr)
                        ||  pHdr->cbOut < sizeof(*pHdr)))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: max(%#x,%#x) != %#x; iCmd=%#lx\n", pHdr->cbIn, pHdr->cbOut, cbReq, iCmd));
            return EINVAL;
        }
    }
    else if ((IOC_DIRMASK & iCmd) == IOC_VOID && !cbReq)
    {
        /*
         * Get the header and figure out how much we're gonna have to read.
         */
        SUPREQHDR Hdr;
        pUser = (user_addr_t)*(void **)pData;
        int rc = copyin(pUser, &Hdr, sizeof(Hdr));
        if (RT_UNLIKELY(rc))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: copyin(%llx,Hdr,) -> %#x; iCmd=%#lx\n", (unsigned long long)pUser, rc, iCmd));
            return rc;
        }
        if (RT_UNLIKELY((Hdr.fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: bad magic fFlags=%#x; iCmd=%#lx\n", Hdr.fFlags, iCmd));
            return EINVAL;
        }
        cbReq = RT_MAX(Hdr.cbIn, Hdr.cbOut);
        if (RT_UNLIKELY(    Hdr.cbIn < sizeof(Hdr)
                        ||  Hdr.cbOut < sizeof(Hdr)
                        ||  cbReq > _1M*16))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: max(%#x,%#x); iCmd=%#lx\n", Hdr.cbIn, Hdr.cbOut, iCmd));
            return EINVAL;
        }

        /*
         * Allocate buffer and copy in the data.
         */
        pHdr = (PSUPREQHDR)RTMemTmpAlloc(cbReq);
        if (!pHdr)
            pvPageBuf = pHdr = (PSUPREQHDR)IOMallocAligned(RT_ALIGN_Z(cbReq, PAGE_SIZE), 8);
        if (RT_UNLIKELY(!pHdr))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: failed to allocate buffer of %d bytes; iCmd=%#lx\n", cbReq, iCmd));
            return ENOMEM;
        }
        rc = copyin(pUser, pHdr, Hdr.cbIn);
        if (RT_UNLIKELY(rc))
        {
            OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: copyin(%llx,%p,%#x) -> %#x; iCmd=%#lx\n",
                        (unsigned long long)pUser, pHdr, Hdr.cbIn, rc, iCmd));
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, RT_ALIGN_Z(cbReq, PAGE_SIZE));
            else
                RTMemTmpFree(pHdr);
            return rc;
        }
    }
    else
    {
        Log(("VBoxDrvDarwinIOCtlSlow: huh? cbReq=%#x iCmd=%#lx\n", cbReq, iCmd));
        return EINVAL;
    }

    /*
     * Process the IOCtl.
     */
    int rc = supdrvIOCtl(iCmd, &g_DevExt, pSession, pHdr);
    if (RT_LIKELY(!rc))
    {
        /*
         * If not buffered, copy back the buffer before returning.
         */
        if (pUser)
        {
            uint32_t cbOut = pHdr->cbOut;
            if (cbOut > cbReq)
            {
                OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: too much output! %#x > %#x; uCmd=%#lx!\n", cbOut, cbReq, iCmd));
                cbOut = cbReq;
            }
            rc = copyout(pHdr, pUser, cbOut);
            if (RT_UNLIKELY(rc))
                OSDBGPRINT(("VBoxDrvDarwinIOCtlSlow: copyout(%p,%llx,%#x) -> %d; uCmd=%#lx!\n",
                            pHdr, (unsigned long long)pUser, cbOut, rc, iCmd));

            /* cleanup */
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, RT_ALIGN_Z(cbReq, PAGE_SIZE));
            else
                RTMemTmpFree(pHdr);
        }
    }
    else
    {
        /*
         * The request failed, just clean up.
         */
        if (pUser)
        {
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, RT_ALIGN_Z(cbReq, PAGE_SIZE));
            else
                RTMemTmpFree(pHdr);
        }

        Log(("VBoxDrvDarwinIOCtlSlow: pid=%d iCmd=%lx pData=%p failed, rc=%d\n", proc_pid(pProcess), iCmd, (void *)pData, rc));
        rc = EINVAL;
    }

    Log2(("VBoxDrvDarwinIOCtlSlow: returns %d\n", rc));
    return rc;
}


/**
 * The SUPDRV IDC entry point.
 *
 * @returns VBox status code, see supdrvIDC.
 * @param   iReq        The request code.
 * @param   pReq        The request.
 */
int VBOXCALL SUPDrvDarwinIDC(uint32_t uReq, PSUPDRVIDCREQHDR pReq)
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
 * Callback for blah blah blah.
 */
IOReturn VBoxDrvDarwinSleepHandler(void * /* pvTarget */, void *pvRefCon, UInt32 uMessageType, IOService * /* pProvider */, void * /* pvMessageArgument */, vm_size_t /* argSize */)
{
    LogFlow(("VBoxDrv: Got sleep/wake notice. Message type was %X\n", (uint)uMessageType));

    if (uMessageType == kIOMessageSystemWillSleep)
        RTPowerSignalEvent(RTPOWEREVENT_SUSPEND);
    else if (uMessageType == kIOMessageSystemHasPoweredOn)
        RTPowerSignalEvent(RTPOWEREVENT_RESUME);

    acknowledgeSleepWakeNotification(pvRefCon);

    return 0;
}


/**
 * Enables or disables VT-x using kernel functions.
 *
 * @returns VBox status code. VERR_NOT_SUPPORTED has a special meaning.
 * @param   fEnable     Whether to enable or disable.
 */
int VBOXCALL supdrvOSEnableVTx(bool fEnable)
{
/* Zarking amateurish Apple engineering!
   host_vmxon is actually buggy and may panic multicore machines. Reason, it
   uses a simple lock which will disable preemption of the cpu/thread trying
   to acquire it.  Then it allocate wired memory in the kernel map for each
   of the cpus in the system. If anyone else tries to mess around in the
   kernel map on another CPU while this is going on, there is a fair chance
   that it might cause the host_vmxon thread to block and hence panic since
   preemption is disabled. Argh! */
#if 0 /*def VBOX_WITH_HOST_VMX*/
    int rc;
    if (fEnable)
    {
        rc = host_vmxon(false /* exclusive */);
        if (rc == 0 /* all ok */)
            rc = VINF_SUCCESS;
        else if (rc == 1 /* unsupported */)
            rc = VERR_VMX_NO_VMX;
        else if (rc == 2 /* exclusive user */)
            rc = VERR_VMX_IN_VMX_ROOT_MODE;
        else /* shouldn't happen, but just in case. */
        {
            LogRel(("host_vmxon returned %d\n", rc));
            rc = VERR_UNRESOLVED_ERROR;
        }
    }
    else
    {
        host_vmxoff();
        rc = VINF_SUCCESS;
    }
    return rc;
#else
    return VERR_NOT_SUPPORTED;
#endif
}


bool VBOXCALL supdrvOSGetForcedAsyncTscMode(PSUPDRVDEVEXT pDevExt)
{
    NOREF(pDevExt);
    return false;
}


int  VBOXCALL   supdrvOSLdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const char *pszFilename)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pszFilename);
    return VERR_NOT_SUPPORTED;
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
 * Converts an IPRT error code to a darwin error code.
 *
 * @returns corresponding darwin error code.
 * @param   rc      IPRT status code.
 */
static int VBoxDrvDarwinErr2DarwinErr(int rc)
{
    switch (rc)
    {
        case VINF_SUCCESS:              return 0;
        case VERR_GENERAL_FAILURE:      return EACCES;
        case VERR_INVALID_PARAMETER:    return EINVAL;
        case VERR_INVALID_MAGIC:        return EILSEQ;
        case VERR_INVALID_HANDLE:       return ENXIO;
        case VERR_INVALID_POINTER:      return EFAULT;
        case VERR_LOCK_FAILED:          return ENOLCK;
        case VERR_ALREADY_LOADED:       return EEXIST;
        case VERR_PERMISSION_DENIED:    return EPERM;
        case VERR_VERSION_MISMATCH:     return ENOSYS;
    }

    return EPERM;
}


RTDECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list     va;
    char        szMsg[512];

    va_start(va, pszFormat);
    RTStrPrintfV(szMsg, sizeof(szMsg) - 1, pszFormat, va);
    va_end(va);
    szMsg[sizeof(szMsg) - 1] = '\0';

    printf("%s", szMsg);
    return 0;
}


/*
 *
 * org_virtualbox_SupDrv
 *
 */


/**
 * Initialize the object.
 */
bool org_virtualbox_SupDrv::init(OSDictionary *pDictionary)
{
    LogFlow(("org_virtualbox_SupDrv::init([%p], %p)\n", this, pDictionary));
    if (IOService::init(pDictionary))
    {
        /* init members. */
        return true;
    }
    return false;
}


/**
 * Free the object.
 */
void org_virtualbox_SupDrv::free(void)
{
    LogFlow(("IOService::free([%p])\n", this));
    IOService::free();
}


/**
 * Check if it's ok to start this service.
 * It's always ok by us, so it's up to IOService to decide really.
 */
IOService *org_virtualbox_SupDrv::probe(IOService *pProvider, SInt32 *pi32Score)
{
    LogFlow(("org_virtualbox_SupDrv::probe([%p])\n", this));
    return IOService::probe(pProvider, pi32Score);
}


/**
 * Start this service.
 */
bool org_virtualbox_SupDrv::start(IOService *pProvider)
{
    LogFlow(("org_virtualbox_SupDrv::start([%p])\n", this));

    if (IOService::start(pProvider))
    {
        /* register the service. */
        registerService();
        return true;
    }
    return false;
}


/**
 * Stop this service.
 */
void org_virtualbox_SupDrv::stop(IOService *pProvider)
{
    LogFlow(("org_virtualbox_SupDrv::stop([%p], %p)\n", this, pProvider));
    IOService::stop(pProvider);
}


/**
 * Termination request.
 *
 * @return  true if we're ok with shutting down now, false if we're not.
 * @param   fOptions        Flags.
 */
bool org_virtualbox_SupDrv::terminate(IOOptionBits fOptions)
{
    bool fRc;
    LogFlow(("org_virtualbox_SupDrv::terminate: reference_count=%d g_cSessions=%d (fOptions=%#x)\n",
             KMOD_INFO_NAME.reference_count, ASMAtomicUoReadS32(&g_cSessions), fOptions));
    if (    KMOD_INFO_NAME.reference_count != 0
        ||  ASMAtomicUoReadS32(&g_cSessions))
        fRc = false;
    else
        fRc = IOService::terminate(fOptions);
    LogFlow(("org_virtualbox_SupDrv::terminate: returns %d\n", fRc));
    return fRc;
}


/*
 *
 * org_virtualbox_SupDrvClient
 *
 */


/**
 * Initializer called when the client opens the service.
 */
bool org_virtualbox_SupDrvClient::initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type)
{
    LogFlow(("org_virtualbox_SupDrvClient::initWithTask([%p], %#x, %p, %#x) (cur pid=%d proc=%p)\n",
             this, OwningTask, pvSecurityId, u32Type, RTProcSelf(), RTR0ProcHandleSelf()));
    AssertMsg((RTR0PROCESS)OwningTask == RTR0ProcHandleSelf(), ("%p %p\n", OwningTask, RTR0ProcHandleSelf()));

    if (!OwningTask)
        return false;
    if (IOUserClient::initWithTask(OwningTask, pvSecurityId , u32Type))
    {
        m_Task = OwningTask;
        m_pSession = NULL;
        m_pProvider = NULL;
        return true;
    }
    return false;
}


/**
 * Start the client service.
 */
bool org_virtualbox_SupDrvClient::start(IOService *pProvider)
{
    LogFlow(("org_virtualbox_SupDrvClient::start([%p], %p) (cur pid=%d proc=%p)\n",
             this, pProvider, RTProcSelf(), RTR0ProcHandleSelf() ));
    AssertMsgReturn((RTR0PROCESS)m_Task == RTR0ProcHandleSelf(),
                    ("%p %p\n", m_Task, RTR0ProcHandleSelf()),
                    false);

    if (IOUserClient::start(pProvider))
    {
        m_pProvider = OSDynamicCast(org_virtualbox_SupDrv, pProvider);
        if (m_pProvider)
        {
            Assert(!m_pSession);

            /*
             * Create a new session.
             */
            int rc = supdrvCreateSession(&g_DevExt, true /* fUser */, &m_pSession);
            if (RT_SUCCESS(rc))
            {
                m_pSession->fOpened = false;
                /* The Uid and Gid fields are set on open. */

                /*
                 * Insert it into the hash table, checking that there isn't
                 * already one for this process first.
                 */
                unsigned iHash = SESSION_HASH(m_pSession->Process);
                RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
                RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);

                PSUPDRVSESSION pCur = g_apSessionHashTab[iHash];
                if (pCur && pCur->Process != m_pSession->Process)
                {
                    do pCur = pCur->pNextHash;
                    while (pCur && pCur->Process != m_pSession->Process);
                }
                if (!pCur)
                {
                    m_pSession->pNextHash = g_apSessionHashTab[iHash];
                    g_apSessionHashTab[iHash] = m_pSession;
                    m_pSession->pvSupDrvClient = this;
                    ASMAtomicIncS32(&g_cSessions);
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_ALREADY_LOADED;

                RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
                if (RT_SUCCESS(rc))
                {
                    Log(("org_virtualbox_SupDrvClient::start: created session %p for pid %d\n", m_pSession, (int)RTProcSelf()));
                    return true;
                }

                LogFlow(("org_virtualbox_SupDrvClient::start: already got a session for this process (%p)\n", pCur));
                supdrvCloseSession(&g_DevExt, m_pSession);
            }

            m_pSession = NULL;
            LogFlow(("org_virtualbox_SupDrvClient::start: rc=%Rrc from supdrvCreateSession\n", rc));
        }
        else
            LogFlow(("org_virtualbox_SupDrvClient::start: %p isn't org_virtualbox_SupDrv\n", pProvider));
    }
    return false;
}


/**
 * Common worker for clientClose and VBoxDrvDarwinClose.
 *
 * It will
 */
/* static */ void org_virtualbox_SupDrvClient::sessionClose(RTPROCESS Process)
{
    /*
     * Look for the session.
     */
    const unsigned  iHash = SESSION_HASH(Process);
    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    PSUPDRVSESSION  pSession = g_apSessionHashTab[iHash];
    if (pSession)
    {
        if (pSession->Process == Process)
        {
            g_apSessionHashTab[iHash] = pSession->pNextHash;
            pSession->pNextHash = NULL;
            ASMAtomicDecS32(&g_cSessions);
        }
        else
        {
            PSUPDRVSESSION pPrev = pSession;
            pSession = pSession->pNextHash;
            while (pSession)
            {
                if (pSession->Process == Process)
                {
                    pPrev->pNextHash = pSession->pNextHash;
                    pSession->pNextHash = NULL;
                    ASMAtomicDecS32(&g_cSessions);
                    break;
                }

                /* next */
                pPrev = pSession;
                pSession = pSession->pNextHash;
            }
        }
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        Log(("SupDrvClient::sessionClose: pSession == NULL, pid=%d; freed already?\n", (int)Process));
        return;
    }

    /*
     * Remove it from the client object.
     */
    org_virtualbox_SupDrvClient *pThis = (org_virtualbox_SupDrvClient *)pSession->pvSupDrvClient;
    pSession->pvSupDrvClient = NULL;
    if (pThis)
    {
        Assert(pThis->m_pSession == pSession);
        pThis->m_pSession = NULL;
    }

    /*
     * Close the session.
     */
    supdrvCloseSession(&g_DevExt, pSession);
}


/**
 * Client exits normally.
 */
IOReturn org_virtualbox_SupDrvClient::clientClose(void)
{
    LogFlow(("org_virtualbox_SupDrvClient::clientClose([%p]) (cur pid=%d proc=%p)\n", this, RTProcSelf(), RTR0ProcHandleSelf()));
    AssertMsg((RTR0PROCESS)m_Task == RTR0ProcHandleSelf(), ("%p %p\n", m_Task, RTR0ProcHandleSelf()));

    /*
     * Clean up the session if it's still around.
     *
     * We cannot rely 100% on close, and in the case of a dead client
     * we'll end up hanging inside vm_map_remove() if we postpone it.
     */
    if (m_pSession)
    {
        sessionClose(RTProcSelf());
        Assert(!m_pSession);
    }

    m_pProvider = NULL;
    terminate();

    return kIOReturnSuccess;
}


/**
 * The client exits abnormally / forgets to do cleanups. (logging)
 */
IOReturn org_virtualbox_SupDrvClient::clientDied(void)
{
    LogFlow(("org_virtualbox_SupDrvClient::clientDied([%p]) m_Task=%p R0Process=%p Process=%d\n",
             this, m_Task, RTR0ProcHandleSelf(), RTProcSelf()));

    /* IOUserClient::clientDied() calls clientClose, so we'll just do the work there. */
    return IOUserClient::clientDied();
}


/**
 * Terminate the service (initiate the destruction). (logging)
 */
bool org_virtualbox_SupDrvClient::terminate(IOOptionBits fOptions)
{
    LogFlow(("org_virtualbox_SupDrvClient::terminate([%p], %#x)\n", this, fOptions));
    return IOUserClient::terminate(fOptions);
}


/**
 * The final stage of the client service destruction. (logging)
 */
bool org_virtualbox_SupDrvClient::finalize(IOOptionBits fOptions)
{
    LogFlow(("org_virtualbox_SupDrvClient::finalize([%p], %#x)\n", this, fOptions));
    return IOUserClient::finalize(fOptions);
}


/**
 * Stop the client service. (logging)
 */
void org_virtualbox_SupDrvClient::stop(IOService *pProvider)
{
    LogFlow(("org_virtualbox_SupDrvClient::stop([%p])\n", this));
    IOUserClient::stop(pProvider);
}

