/** @file
 * VBox host drivers - Ring-0 support drivers - Darwin host:
 * Darwin driver C code
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
/* Deal with conflicts first.
 * (This is mess inherited from BSD. The *BSDs has clean this up long ago.) */
#include <sys/param.h>
#undef PVM
#include <IOKit/IOLib.h> /* Assert as function */

#include "SUPDRV.h"
#include <VBox/version.h>
#include <iprt/types.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/process.h>
#include <iprt/alloc.h>

#include <mach/kmod.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserclient.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** The module name. */
#define DEVICE_NAME    "vboxdrv"



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
static kern_return_t    VBoxSupDrvStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    VBoxSupDrvStop(struct kmod_info *pKModInfo, void *pvData);

static int              VBoxSupDrvOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int              VBoxSupDrvClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess);
static int              VBoxSupDrvIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess);
static int              VBoxSupDrvIOCtlSlow(PSUPDRVSESSION pSession, u_long iCmd, caddr_t pData, struct proc *pProcess);

static int              VBoxSupDrvErr2DarwinErr(int rc);
__END_DECLS


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The service class.
 * This is just a formality really.
 */
class org_virtualbox_SupDrv : public IOService
{
    OSDeclareDefaultStructors(org_virtualbox_SupDrv)

public:
    virtual bool init(OSDictionary *pDictionary = 0);
    virtual void free(void);
    virtual bool start(IOService *pProvider);
    virtual void stop(IOService *pProvider);
    virtual IOService *probe(IOService *pProvider, SInt32 *pi32Score);
};

OSDefineMetaClassAndStructors(org_virtualbox_SupDrv, IOService)


/**
 * An attempt at getting that clientDied() notification.
 * I don't think it'll work as I cannot figure out where/what creates the correct
 * port right.
 */
class org_virtualbox_SupDrvClient : public IOUserClient
{
    OSDeclareDefaultStructors(org_virtualbox_SupDrvClient)

private:
    PSUPDRVSESSION          m_pSession;     /**< The session. */
    task_t                  m_Task;         /**< The client task. */
    org_virtualbox_SupDrv  *m_pProvider;    /**< The service provider. */

public:
    virtual bool initWithTask(task_t OwningTask, void *pvSecurityId, UInt32 u32Type);
    virtual bool start(IOService *pProvider);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    virtual bool terminate(IOOptionBits fOptions = 0);
    virtual bool finalize(IOOptionBits fOptions);
    virtual void stop(IOService *pProvider);
};

OSDefineMetaClassAndStructors(org_virtualbox_SupDrvClient, IOUserClient)



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Declare the module stuff.
 */
__BEGIN_DECLS
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);
__private_extern__ kmod_start_func_t *_realmain;
__private_extern__ kmod_stop_func_t  *_antimain;
__private_extern__ int                _kext_apple_cc;

KMOD_EXPLICIT_DECL(VBoxDrv, VBOX_VERSION_STRING, _start, _stop)
kmod_start_func_t *_realmain = VBoxSupDrvStart;
kmod_stop_func_t  *_antimain = VBoxSupDrvStop;
int                _kext_apple_cc = __APPLE_CC__;
__END_DECLS


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
    /*.d_open  = */VBoxSupDrvOpen,
    /*.d_close = */VBoxSupDrvClose,
    /*.d_read  = */eno_rdwrt,
    /*.d_write = */eno_rdwrt,
    /*.d_ioctl = */VBoxSupDrvIOCtl,
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


/**
 * Start the kernel module.
 */
static kern_return_t    VBoxSupDrvStart(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;
    dprintf(("VBoxSupDrvStart\n"));

    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize the device extension.
         */
        rc = supdrvInitDevExt(&g_DevExt);
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
                    /** @todo the UID, GID and mode mask should be configurable! This isn't very secure... */
                    g_hDevFsDevice = devfs_make_node(makedev(g_iMajorDeviceNo, 0), DEVFS_CHAR,
                                                     UID_ROOT, GID_WHEEL, 0666, DEVICE_NAME);
                    if (g_hDevFsDevice)
                    {
                        OSDBGPRINT(("VBoxDrv: Successfully started. (major=%d)\n", g_iMajorDeviceNo));
                        return KMOD_RETURN_SUCCESS;
                    }

                    OSDBGPRINT(("VBoxDrv: devfs_make_node(makedev(%d,0),,,,%s) failed\n",
                                g_iMajorDeviceNo, DEVICE_NAME));
                    cdevsw_remove(g_iMajorDeviceNo, &g_DevCW);
                    g_iMajorDeviceNo = -1;
                }
                else
                    OSDBGPRINT(("VBoxDrv: cdevsw_add failed (%d)\n", g_iMajorDeviceNo));
                RTSpinlockDestroy(g_Spinlock);
                g_Spinlock = NIL_RTSPINLOCK;
            }
            else
                OSDBGPRINT(("VBoxDrv: RTSpinlockCreate failed (rc=%d)\n", rc));
            supdrvDeleteDevExt(&g_DevExt);
        }
        else
            OSDBGPRINT(("VBoxDrv: failed to initialize device extension (rc=%d)\n", rc));
        RTR0Term();
    }
    else
        OSDBGPRINT(("VBoxDrv: failed to initialize IPRT (rc=%d)\n", rc));

    memset(&g_DevExt, 0, sizeof(g_DevExt));
    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t    VBoxSupDrvStop(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;
    dprintf(("VBoxSupDrvStop\n"));

    /** @todo I've got a nagging feeling that we'll have to keep track of users and refuse
     * unloading if we're busy. Investigate and implement this! */

    /*
     * Undo the work done during start (in reverse order).
     */
    devfs_remove(g_hDevFsDevice);
    g_hDevFsDevice = NULL;

    rc = cdevsw_remove(g_iMajorDeviceNo, &g_DevCW);
    Assert(rc == g_iMajorDeviceNo);
    g_iMajorDeviceNo = -1;

    rc = supdrvDeleteDevExt(&g_DevExt);
    AssertRC(rc);

    rc = RTSpinlockDestroy(g_Spinlock);
    AssertRC(rc);
    g_Spinlock = NIL_RTSPINLOCK;

    RTR0Term();

    memset(&g_DevExt, 0, sizeof(g_DevExt));
    dprintf(("VBoxSupDrvStop - done\n"));
    return KMOD_RETURN_SUCCESS;
}


/**
 * Device open. Called on open /dev/vboxdrv
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxSupDrvOpen(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    int                 rc;
    PSUPDRVSESSION      pSession;
#ifdef DEBUG_DARWIN_GIP
    char szName[128];
    szName[0] = '\0';
    proc_name(proc_pid(pProcess), szName, sizeof(szName));
    dprintf(("VBoxSupDrvOpen: pid=%d '%s'\n", proc_pid(pProcess), szName));
#endif

    /*
     * Create a new session.
     */
    rc = supdrvCreateSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
        unsigned        iHash;
        struct ucred   *pCred = proc_ucred(pProcess);
        if (pCred)
        {
            pSession->Uid = pCred->cr_uid;
            pSession->Gid = pCred->cr_gid;
        }
        pSession->Process = RTProcSelf();
        pSession->R0Process = RTR0ProcHandleSelf();

        /*
         * Insert it into the hash table.
         */
        iHash = SESSION_HASH(pSession->Process);
        RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
        pSession->pNextHash = g_apSessionHashTab[iHash];
        g_apSessionHashTab[iHash] = pSession;
        RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    }

#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("VBoxSupDrvOpen: pid=%d '%s' pSession=%p rc=%d\n", proc_pid(pProcess), szName, pSession, rc));
#else
    dprintf(("VBoxSupDrvOpen: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, proc_pid(pProcess)));
#endif
    return VBoxSupDrvErr2DarwinErr(rc);
}


/**
 * Close device.
 */
static int VBoxSupDrvClose(dev_t Dev, int fFlags, int fDevType, struct proc *pProcess)
{
    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS Process = proc_pid(pProcess);
    const unsigned  iHash = SESSION_HASH(Process);
    PSUPDRVSESSION  pSession;

    dprintf(("VBoxSupDrvClose: pid=%d\n", (int)Process));

    /*
     * Remove from the hash table.
     */
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession)
    {
        if (pSession->Process == Process)
        {
            g_apSessionHashTab[iHash] = pSession->pNextHash;
            pSession->pNextHash = NULL;
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
        OSDBGPRINT(("VBoxSupDrvClose: WHAT?!? pSession == NULL! This must be a mistake... pid=%d (close)\n", 
                    (int)Process));
        return EINVAL;
    }

    /*
     * Close the session.
     */
    supdrvCloseSession(&g_DevExt, pSession);
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
static int VBoxSupDrvIOCtl(dev_t Dev, u_long iCmd, caddr_t pData, int fFlags, struct proc *pProcess)
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
        OSDBGPRINT(("VBoxSupDrvIOCtl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d iCmd=%#x\n", 
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
        return supdrvIOCtlFast(iCmd, &g_DevExt, pSession);
    return VBoxSupDrvIOCtlSlow(pSession, iCmd, pData, pProcess);
}


/**
 * Worker for VBoxSupDrvIOCtl that takes the slow IOCtl functions.
 *
 * @returns Darwin errno.
 *
 * @param pSession  The session.
 * @param iCmd      The IOCtl command.
 * @param pData     Pointer to the kernel copy of the SUPDRVIOCTLDATA buffer.
 * @param pProcess  The calling process.
 */
static int VBoxSupDrvIOCtlSlow(PSUPDRVSESSION pSession, u_long iCmd, caddr_t pData, struct proc *pProcess)
{
    int                 rc;
    void               *pvPageBuf = NULL;
    void               *pvBuf = NULL;
    unsigned long       cbBuf = 0;
    unsigned            cbOut = 0;
    PSUPDRVIOCTLDATA    pArgs = (PSUPDRVIOCTLDATA)pData;
    dprintf(("VBoxSupDrvIOCtlSlow: pSession=%p iCmd=%p pData=%p pProcess=%p\n", pSession, iCmd, pData, pProcess));

    /*
     * Copy ioctl data structure from user space.
     */
    if (IOCPARM_LEN(iCmd) != sizeof(SUPDRVIOCTLDATA))
    {
        dprintf(("VBoxSupDrvIOCtlSlow: incorrect input length! cbArgs=%d\n", IOCPARM_LEN(iCmd)));
        return EINVAL;
    }

    /*
     * Allocate and copy user space input data buffer to kernel space.
     */
    if (pArgs->cbIn > 0 || pArgs->cbOut > 0)
    {
        cbBuf = max(pArgs->cbIn, pArgs->cbOut);
        pvBuf = RTMemTmpAlloc(cbBuf);
        if (pvBuf == NULL)
            pvPageBuf = pvBuf = IOMallocAligned(cbBuf, 8);
        if (pvBuf == NULL)
        {
            dprintf(("VBoxSupDrvIOCtlSlow: failed to allocate buffer of %d bytes.\n", cbBuf));
            return ENOMEM;
        }
        rc = copyin((const user_addr_t)pArgs->pvIn, pvBuf, pArgs->cbIn);
        if (rc)
        {
            dprintf(("VBoxSupDrvIOCtlSlow: copyin(%p,,%d) failed.\n", pArgs->pvIn, cbBuf));
            if (pvPageBuf)
                IOFreeAligned(pvPageBuf, cbBuf);
            else
                RTMemTmpFree(pvBuf);
            return rc;
        }
    }

    /*
     * Process the IOCtl.
     */
    rc = supdrvIOCtl(iCmd, &g_DevExt, pSession,
                     pvBuf, pArgs->cbIn, pvBuf, pArgs->cbOut, &cbOut);

    /*
     * Copy ioctl data and output buffer back to user space.
     */
    if (rc)
    {
        dprintf(("VBoxSupDrvIOCtlSlow: pid=%d iCmd=%x pData=%p failed, rc=%d (darwin rc=%d)\n",
                 proc_pid(pProcess), iCmd, (void *)pData, rc, VBoxSupDrvErr2DarwinErr(rc)));
        rc = VBoxSupDrvErr2DarwinErr(rc);
    }
    else if (cbOut > 0)
    {
        if (pvBuf != NULL && cbOut <= cbBuf)
        {
            int rc2 = copyout(pvBuf, (user_addr_t)pArgs->pvOut, cbOut);
            if (rc2)
            {
                dprintf(("VBoxSupDrvIOCtlSlow: copyout(,%p,%d) failed.\n", pArgs->pvOut, cbBuf));
                rc = rc2;
            }
        }
        else
        {
            dprintf(("WHAT!?! supdrvIOCtl messed up! cbOut=%d cbBuf=%d pvBuf=%p\n", cbOut, cbBuf, pvBuf));
            rc = EPERM;
        }
    }

    if (pvPageBuf)
        IOFreeAligned(pvPageBuf, cbBuf);
    else if (pvBuf)
        RTMemTmpFree(pvBuf);

    dprintf2(("VBoxSupDrvIOCtlSlow: returns %d\n", rc));
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
 * Converts a supdrv error code to a darwin error code.
 *
 * @returns corresponding darwin error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int VBoxSupDrvErr2DarwinErr(int rc)
{
    switch (rc)
    {
        case 0:                             return 0;
        case SUPDRV_ERR_GENERAL_FAILURE:    return EACCES;
        case SUPDRV_ERR_INVALID_PARAM:      return EINVAL;
        case SUPDRV_ERR_INVALID_MAGIC:      return EILSEQ;
        case SUPDRV_ERR_INVALID_HANDLE:     return ENXIO;
        case SUPDRV_ERR_INVALID_POINTER:    return EFAULT;
        case SUPDRV_ERR_LOCK_FAILED:        return ENOLCK;
        case SUPDRV_ERR_ALREADY_LOADED:     return EEXIST;
        case SUPDRV_ERR_PERMISSION_DENIED:  return EPERM;
        case SUPDRV_ERR_VERSION_MISMATCH:   return ENOSYS;
    }

    return EPERM;
}


/** @todo move this to assembly where a simple "jmp printf" will to the trick. */
RTDECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list     args;
    char        szMsg[512];

    va_start(args, pszFormat);
    vsnprintf(szMsg, sizeof(szMsg) - 1, pszFormat, args);
    va_end(args);

    szMsg[sizeof(szMsg) - 1] = '\0';
    printf("%s", szMsg);
    return 0;
}


/** Runtime assert implementation for Darwin Ring-0. */
RTDECL(void) AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    printf("!!Assertion Failed!!\n"
             "Expression: %s\n"
             "Location  : %s(%d) %s\n",
             pszExpr, pszFile, uLine, pszFunction);
}


/** Runtime assert implementation for the Darwin Ring-0 driver.
 * @todo this one needs fixing! */
RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{   /* forwarder. */
    va_list     ap;
    char        msg[256];

    va_start(ap, pszFormat);
    vsnprintf(msg, sizeof(msg) - 1, pszFormat, ap);
    msg[sizeof(msg) - 1] = '\0';
    printf("%s", msg);
    va_end(ap);
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
    dprintf(("org_virtualbox_SupDrv::init([%p], %p)\n", this, pDictionary));
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
    dprintf(("IOService::free([%p])\n", this));
    IOService::free();
}


/**
 * Check if it's ok to start this service.
 * It's always ok by us, so it's up to IOService to decide really.
 */
IOService *org_virtualbox_SupDrv::probe(IOService *pProvider, SInt32 *pi32Score)
{
    dprintf(("org_virtualbox_SupDrv::probe([%p])\n", this));
    return IOService::probe(pProvider, pi32Score);
}


/**
 * Start this service.
 */
bool org_virtualbox_SupDrv::start(IOService *pProvider)
{
    dprintf(("org_virtualbox_SupDrv::start([%p])\n", this));

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
    dprintf(("org_virtualbox_SupDrv::stop([%p], %p)\n", this, pProvider));
    IOService::stop(pProvider);
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
    dprintf(("org_virtualbox_SupDrvClient::initWithTask([%p], %#x, %p, %#x)\n", this, OwningTask, pvSecurityId, u32Type));

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
    dprintf(("org_virtualbox_SupDrvClient::start([%p], %p)\n", this, pProvider));
    if (IOUserClient::start(pProvider))
    {
        m_pProvider = OSDynamicCast(org_virtualbox_SupDrv, pProvider);
        if (m_pProvider)
        {
            /* this is where we could create the section. */
            return true;
        }
        dprintf(("org_virtualbox_SupDrvClient::start: %p isn't org_virtualbox_SupDrv\n", pProvider));
    }
    return false;
}


/**
 * Client exits normally.
 */
IOReturn org_virtualbox_SupDrvClient::clientClose(void)
{
    dprintf(("org_virtualbox_SupDrvClient::clientClose([%p])\n", this));

    m_pProvider = NULL;
    terminate();

    return kIOReturnSuccess;
}


/**
 * The client exits abnormally / forgets to do cleanups.
 */
IOReturn org_virtualbox_SupDrvClient::clientDied(void)
{
    dprintf(("org_virtualbox_SupDrvClient::clientDied([%p]) m_Task=%p R0Process=%p Process=%d\n",
             this, m_Task, RTR0ProcHandleSelf(), RTProcSelf()));

    /*
     * Do early session cleanup (if there is a session) so
     * we avoid hanging in vm_map_remove().
     */
    const RTR0PROCESS   R0Process = (RTR0PROCESS)m_Task;
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    for (unsigned i = 0; i < RT_ELEMENTS(g_apSessionHashTab); i++)
    {
        for (PSUPDRVSESSION pSession = g_apSessionHashTab[i]; pSession; pSession = pSession->pNextHash)
        {
            dprintf2(("pSession=%p R0Process=%p (=? %p)\n", pSession, pSession->R0Process, R0Process));
            if (pSession->R0Process == R0Process)
            {
                /*
                 * It is safe to leave the spinlock here; the session shouldn't be able
                 * to go away while we're cleaning it up, changes to pNextHash will not
                 * harm us, and new sessions can't possibly be added for this process.
                 */
                RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
                supdrvCleanupSession(&g_DevExt, pSession);
                RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
            }
        }
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);

    /* IOUserClient::clientDied() calls close... */
    return IOUserClient::clientDied();
}


/**
 * Terminate the service (initiate the destruction).
 */
bool org_virtualbox_SupDrvClient::terminate(IOOptionBits fOptions)
{
    dprintf(("org_virtualbox_SupDrvClient::terminate([%p], %#x)\n", this, fOptions));
    return IOUserClient::terminate(fOptions);
}


/**
 * The final stage of the client service destruction.
 */
bool org_virtualbox_SupDrvClient::finalize(IOOptionBits fOptions)
{
    dprintf(("org_virtualbox_SupDrvClient::finalize([%p], %#x)\n", this, fOptions));
    return IOUserClient::finalize(fOptions);
}


/**
 * Stop the client service.
 */
void org_virtualbox_SupDrvClient::stop(IOService *pProvider)
{
    dprintf(("org_virtualbox_SupDrvClient::stop([%p])\n", this));
    IOUserClient::stop(pProvider);
}

