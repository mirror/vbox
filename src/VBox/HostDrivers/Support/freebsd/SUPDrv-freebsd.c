/* $Id$ */
/** @file
 * VBoxDrv - FreeBSD specifics.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
/* Deal with conflicts first. */
#include <sys/param.h>
#undef PVM
#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/uio.h>

#include "SUPDRV.h"
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/spinlock.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/alloc.h>
#include <iprt/err.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxDrvFreeBSDModuleEvent(struct module *pMod, int enmEventType, void *pvArg);
static int VBoxDrvFreeBSDLoad(void);
static int VBoxDrvFreeBSDUnload(void);
static void VBoxDrvFreeBSDClone(void *pvArg, struct ucred *pCred, char *pachName, int cchName, struct cdev **ppDev);

static d_fdopen_t   VBoxDrvFreeBSDOpen;
static d_close_t    VBoxDrvFreeBSDClose;
static d_ioctl_t    VBoxDrvFreeBSDIOCtl;
static int          VBoxDrvFreeBSDIOCtlSlow(PSUPDRVSESSION pSession, u_long ulCmd, caddr_t pvData, struct thread *pTd);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Module info structure used by the kernel.
 */
static moduledata_t         g_VBoxDrvFreeBSDModule =
{
    "vboxdrv",
    VBoxDrvFreeBSDModuleEvent,
    NULL
};

/** Declare the module as a pseudo device. */
DECLARE_MODULE(vboxdrv,     g_VBoxDrvFreeBSDModule, SI_SUB_PSEUDO, SI_ORDER_ANY);

/**
 * The /dev/vboxdrv character device entry points.
 */
static struct cdevsw        g_VBoxDrvFreeBSDChrDevSW =
{
    .d_version =        D_VERSION,
    .d_flags =          D_PSEUDO | D_TRACKCLOSE,
    .d_fdopen =         VBoxDrvFreeBSDOpen,
    .d_close =          VBoxDrvFreeBSDClose,
    .d_ioctl =          VBoxDrvFreeBSDIOCtl,
    .d_name =           "vboxdrv"
};

/** List of cloned device. Managed by the kernel. */
static struct clonedevs    *g_pVBoxDrvFreeBSDClones;
/** The dev_clone event handler tag. */
static eventhandler_tag     g_VBoxDrvFreeBSDEHTag;

/** The device extention. */
static SUPDRVDEVEXT         g_VBoxDrvFreeBSDDevExt;




/**
 * Module event handler.
 *
 * @param   pMod            The module structure.
 * @param   enmEventType    The event type (modeventtype_t).
 * @param   pvArg           Module argument. NULL.
 *
 * @return  0 on success, errno.h status code on failure.
 */
static int VBoxDrvFreeBSDModuleEvent(struct module *pMod, int enmEventType, void *pvArg)
{
    int rc;
    switch (enmEventType)
    {
        case MOD_LOAD:
            rc = VBoxDrvFreeBSDLoad();
            break;

        case MOD_UNLOAD:
            rc = VBoxDrvFreeBSDUnload();
            break;

        case MOD_SHUTDOWN:
        case MOD_QUIESCE:
        default:
            return EOPNOTSUPP;
    }

    if (RT_SUCCESS(rc))
        return 0;
    return RTErrConvertToErrno(rc);
}


static int VBoxDrvFreeBSDLoad(void)
{
    dprintf(("VBoxDrvFreeBSDLoad:\n"));

    /*
     * Initialize the runtime.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize the device extension.
         */
        rc = supdrvInitDevExt(&g_VBoxDrvFreeBSDDevExt);
        if (RT_SUCCESS(rc))
        {
            /*
             * Configure device cloning.
             */
            clone_setup(&g_pVBoxDrvFreeBSDClones);
            g_VBoxDrvFreeBSDEHTag = EVENTHANDLER_REGISTER(dev_clone, VBoxDrvFreeBSDClone, 0, 1000);
            if (g_VBoxDrvFreeBSDEHTag)
            {
                dprintf(("VBoxDrvFreeBSDLoad: returns successfully\n"));
                return VINF_SUCCESS;
            }

            printf("vboxdrv: EVENTHANDLER_REGISTER(dev_clone,,,) failed\n");
            clone_cleanup(&g_pVBoxDrvFreeBSDClones);
            rc = SUPDRV_ERR_ALREADY_LOADED;
            supdrvDeleteDevExt(&g_VBoxDrvFreeBSDDevExt);
        }
        else
            printf("vboxdrv: supdrvInitDevExt failed, rc=%d\n", rc);
        RTR0Term();
    }
    else
        printf("vboxdrv: RTR0Init failed, rc=%d\n", rc);
    return rc;
}

static int VBoxDrvFreeBSDUnload(void)
{
    dprintf(("VBoxDrvFreeBSDUnload:\n"));

    /** @todo verify that FreeBSD does reference counting. */

    /*
     * Reserve what we did in VBoxDrvFreeBSDInit.
     */
    clone_cleanup(&g_pVBoxDrvFreeBSDClones);

    supdrvDeleteDevExt(&g_VBoxDrvFreeBSDDevExt);

    RTR0Term();

    memset(&g_VBoxDrvFreeBSDDevExt, 0, sizeof(g_VBoxDrvFreeBSDDevExt));

    dprintf(("VBoxDrvFreeBSDUnload: returns\n"));
    return VINF_SUCCESS;
}


/**
 * DEVFS event handler.
 */
static void VBoxDrvFreeBSDClone(void *pvArg, struct ucred *pCred, char *pszName, int cchName, struct cdev **ppDev)
{
    int iUnit;
    int rc;

    dprintf(("VBoxDrvFreeBSDClone: pszName=%s ppDev=%p\n", pszName, ppDev));

    /*
     * One device node per user, si_drv1 points to the session.
     * /dev/vboxdrv<N> where N = {0...255}.
     */
    if (!ppDev)
        return;
    if (dev_stdclone(pszName, NULL, "vboxdrv", &iUnit) != 1)
        return;
    if (iUnit >= 256 || iUnit < 0)
    {
        dprintf(("VBoxDrvFreeBSDClone: iUnit=%d >= 256 - rejected\n", iUnit));
        return;
    }

    dprintf(("VBoxDrvFreeBSDClone: pszName=%s iUnit=%d\n", pszName, iUnit));

    rc = clone_create(&g_pVBoxDrvFreeBSDClones, &g_VBoxDrvFreeBSDChrDevSW, &iUnit, ppDev, 0);
    dprintf(("VBoxDrvFreeBSDClone: clone_create -> %d; iUnit=%d\n", rc, iUnit));
    if (rc)
    {
        *ppDev = make_dev(&g_VBoxDrvFreeBSDChrDevSW,
                          unit2minor(iUnit),
                          UID_ROOT,
                          GID_WHEEL,
                          0666,
                          "vboxdrv%d", iUnit);
        if (*ppDev)
        {
            dev_ref(*ppDev);
            (*ppDev)->si_flags |= SI_CHEAPCLONE;
            dprintf(("VBoxDrvFreeBSDClone: Created *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
                     *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
            (*ppDev)->si_drv1 = (*ppDev)->si_drv2 = NULL;
        }
        else
            OSDBGPRINT(("VBoxDrvFreeBSDClone: make_dev iUnit=%d failed\n", iUnit));
    }
    else
        dprintf(("VBoxDrvFreeBSDClone: Existing *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
                 *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
}



/**
 *
 * @returns 0 on success, errno on failure.
 *          EBUSY if the device is used by someone else.
 * @param   pDev    The device node.
 * @param   fOpen   The open flags.
 * @param   pTd     The thread.
 * @param   pFd     The file descriptor. FreeBSD 7.0 and later.
 * @param   iFd     The file descriptor index(?). Pre FreeBSD 7.0.
 */
#if FreeBSD >= 700 /* figure when and how to check properly */
static int VBoxDrvFreeBSDOpen(struct cdev *pDev, int fOpen, struct thread *pTd, struct file *pFd)
#else
static int VBoxDrvFreeBSDOpen(struct cdev *pDev, int fOpen, struct thread *pTd, int iFd)
#endif
{
    PSUPDRVSESSION pSession;
    int rc;

    dprintf(("VBoxDrvFreeBSDOpen: fOpen=%#x iUnit=%d\n", fOpen, minor2unit(minor(pDev))));

    /*
     * Let's be a bit picky about the flags...
     */
    if (fOpen != (FREAD|FWRITE /*=O_RDWR*/))
    {
        dprintf(("VBoxDrvFreeBSDOpen: fOpen=%#x expected %#x\n", fOpen, O_RDWR));
        return EINVAL;
    }

    /*
     * Try grab it (we don't grab the giant, remember).
     */
    if (!ASMAtomicCmpXchgPtr(&pDev->si_drv1, (void *)0x42, NULL))
        return EBUSY;

    /*
     * Create a new session.
     */
    rc = supdrvCreateSession(&g_VBoxDrvFreeBSDDevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        pSession->Uid = 0;
        pSession->Gid = 0;
        pSession->Process = RTProcSelf();
        pSession->R0Process = RTR0ProcHandleSelf();
        if (ASMAtomicCmpXchgPtr(&pDev->si_drv1, pSession, (void *)0x42))
            return 0;

        OSDBGPRINT(("VBoxDrvFreeBSDOpen: si_drv1=%p, expected 0x42!\n", pDev->si_drv1));
        supdrvCloseSession(&g_VBoxDrvFreeBSDDevExt, pSession);
    }

    return RTErrConvertToErrno(rc);
}


/**
 * Close a file device previously opened by VBoxDrvFreeBSDOpen
 *
 * @returns 0 on success.
 * @param   pDev        The device.
 * @param   fFile       The file descriptor flags.
 * @param   DevType     The device type (CHR.
 * @param   pTd         The calling thread.
 */
static int VBoxDrvFreeBSDClose(struct cdev *pDev, int fFile, int DevType, struct thread *pTd)
{
    PSUPDRVSESSION pSession = (PSUPDRVSESSION)pDev->si_drv1;
    dprintf(("VBoxDrvFreeBSDClose: fFile=%#x iUnit=%d pSession=%p\n", fFile, minor2unit(minor(pDev)), pSession));

    /*
     * Close the session if it's still hanging on to the device...
     */
    if (VALID_PTR(pSession))
    {
        supdrvCloseSession(&g_VBoxDrvFreeBSDDevExt, pSession);
        if (!ASMAtomicCmpXchgPtr(&pDev->si_drv1, NULL, pSession))
            OSDBGPRINT(("VBoxDrvFreeBSDClose: si_drv1=%p expected %p!\n", pDev->si_drv1, pSession));
    }
    else
        OSDBGPRINT(("VBoxDrvFreeBSDClose: si_drv1=%p!\n", pSession));
    return 0;
}


/**
 * I/O control request.
 *
 * @returns depends...
 * @param   pDev        The device.
 * @param   ulCmd       The command.
 * @param   pvData      Pointer to the data.
 * @param   fFile       The file descriptor flags.
 * @param   pTd         The calling thread.
 */
static int VBoxDrvFreeBSDIOCtl(struct cdev *pDev, u_long ulCmd, caddr_t pvData, int fFile, struct thread *pTd)
{
    /*
     * Validate the input.
     */
    PSUPDRVSESSION pSession = (PSUPDRVSESSION)pDev->si_drv1;
    if (RT_UNLIKELY(!VALID_PTR(pSession)))
        return EINVAL;

    /*
     * Deal with the fast ioctl path first.
     */
    if (    ulCmd == SUP_IOCTL_FAST_DO_RAW_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_NOP)
        return supdrvIOCtlFast(ulCmd, &g_VBoxDrvFreeBSDDevExt, pSession);

    return VBoxDrvFreeBSDIOCtlSlow(pSession, ulCmd, pvData, pTd);
}


/**
 * Deal with the 'slow' I/O control requests.
 *
 * @returns 0 on success, appropriate errno on failure.
 * @param   pSession    The session.
 * @param   ulCmd       The command.
 * @param   pvData      The request data.
 * @param   pTd         The calling thread.
 */
static int VBoxDrvFreeBSDIOCtlSlow(PSUPDRVSESSION pSession, u_long ulCmd, caddr_t pvData, struct thread *pTd)
{
    PSUPREQHDR  pHdr;
    uint32_t    cbReq = IOCPARM_LEN(ulCmd);
    void       *pvUser = NULL;

    /*
     * Buffered request?
     */
    if ((IOC_DIRMASK & ulCmd) == IOC_INOUT)
    {
        pHdr = (PSUPREQHDR)pvData;
        if (RT_UNLIKELY(cbReq < sizeof(*pHdr)))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: cbReq=%#x < %#x; ulCmd=%#lx\n", cbReq, (int)sizeof(*pHdr), ulCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY((pHdr->fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: bad magic fFlags=%#x; ulCmd=%#lx\n", pHdr->fFlags, ulCmd));
            return EINVAL;
        }
        if (RT_UNLIKELY(    RT_MAX(pHdr->cbIn, pHdr->cbOut) != cbReq
                        ||  pHdr->cbIn < sizeof(*pHdr)
                        ||  pHdr->cbOut < sizeof(*pHdr)))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: max(%#x,%#x) != %#x; ulCmd=%#lx\n", pHdr->cbIn, pHdr->cbOut, cbReq, ulCmd));
            return EINVAL;
        }
    }
    /*
     * Big unbuffered request?
     */
    else if ((IOC_DIRMASK & ulCmd) == IOC_VOID && !cbReq)
    {
        /*
         * Read the header, validate it and figure out how much that needs to be buffered.
         */
        SUPREQHDR Hdr;
        pvUser = *(void **)pvData;
        int rc = copyin(pvUser, &Hdr, sizeof(Hdr));
        if (RT_UNLIKELY(rc))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: copyin(%p,Hdr,) -> %#x; ulCmd=%#lx\n", pvUser, rc, ulCmd));
            return rc;
        }
        if (RT_UNLIKELY((Hdr.fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: bad magic fFlags=%#x; ulCmd=%#lx\n", Hdr.fFlags, ulCmd));
            return EINVAL;
        }
        cbReq = RT_MAX(Hdr.cbIn, Hdr.cbOut);
        if (RT_UNLIKELY(    Hdr.cbIn < sizeof(Hdr)
                        ||  Hdr.cbOut < sizeof(Hdr)
                        ||  cbReq > _1M*16))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: max(%#x,%#x); ulCmd=%#lx\n", Hdr.cbIn, Hdr.cbOut, ulCmd));
            return EINVAL;
        }

        /*
         * Allocate buffer and copy in the data.
         */
        pHdr = (PSUPREQHDR)RTMemTmpAlloc(cbReq);
        if (RT_UNLIKELY(!pHdr))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: failed to allocate buffer of %d bytes; ulCmd=%#lx\n", cbReq, ulCmd));
            return ENOMEM;
        }
        rc = copyin(pvUser, pHdr, Hdr.cbIn);
        if (RT_UNLIKELY(rc))
        {
            OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: copyin(%p,%p,%#x) -> %#x; ulCmd=%#lx\n",
                        pvUser, pHdr, Hdr.cbIn, rc, ulCmd));
            RTMemTmpFree(pHdr);
            return rc;
        }
    }
    else
    {
        dprintf(("VBoxDrvFreeBSDIOCtlSlow: huh? cbReq=%#x ulCmd=%#lx\n", cbReq, ulCmd));
        return EINVAL;
    }

    /*
     * Process the IOCtl.
     */
    int rc = supdrvIOCtl(ulCmd, &g_VBoxDrvFreeBSDDevExt, pSession, pHdr);
    if (RT_LIKELY(!rc))
    {
        /*
         * If unbuffered, copy back the result before returning.
         */
        if (pvUser)
        {
            uint32_t cbOut = pHdr->cbOut;
            if (cbOut > cbReq)
            {
                OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: too much output! %#x > %#x; uCmd=%#lx!\n", cbOut, cbReq, ulCmd));
                cbOut = cbReq;
            }
            rc = copyout(pHdr, pvUser, cbOut);
            if (RT_UNLIKELY(rc))
                OSDBGPRINT(("VBoxDrvFreeBSDIOCtlSlow: copyout(%p,%p,%#x) -> %d; uCmd=%#lx!\n", pHdr, pvUser, cbOut, rc, ulCmd));

            /* cleanup */
            RTMemTmpFree(pHdr);
        }
        dprintf(("VBoxDrvFreeBSDIOCtlSlow: returns %d / %d ulCmd=%lx\n", 0, pHdr->rc, ulCmd));
    }
    else
    {
        /*
         * The request failed, just clean up.
         */
        if (pvUser)
            RTMemTmpFree(pHdr);

        dprintf(("VBoxDrvFreeBSDIOCtlSlow: ulCmd=%lx pData=%p failed, rc=%d\n", ulCmd, pvData, rc));
        rc = EINVAL;
    }

    return rc;
}



void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
{
    NOREF(pObj);
    NOREF(pSession);
}


bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}

/**
 * Executes a callback handler on a specific cpu or all cpus
 *
 * @returns IPRT status code.
 * @param   pSession    The session.
 * @param   pfnCallback Callback handler
 * @param   pvUser      The first user argument.
 * @param   uCpu        Cpu id or SUPDRVEXECCALLBACK_CPU_ALL for all cpus
 */
int  VBOXCALL   supdrvOSExecuteCallback(PSUPDRVSESSION pSession, PFNSUPDRVEXECCALLBACK pfnCallback, void *pvUser, unsigned uCpu)
{
    NOREF(pSession);
    NOREF(pfnCallback);
    NOREF(pvUser);
    NOREF(uCpu);
    /** @todo */
    return VERR_NOT_IMPLEMENTED;
}

SUPR0DECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list va;
    char szMsg[256];
    int cch;

    va_start(va, pszFormat);
    cch = RTStrPrintfV(szMsg, sizeof(szMsg), pszFormat, va);
    va_end(va);

    printf("%s", szMsg);

    return cch;
}

