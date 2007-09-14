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


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxDrvFreeBSDModuleEvent(struct module *pMod, int enmEventType, void *pvArg);
static int VBoxDrvFreeBSDLoad(void);
static int VBoxDrvFreeBSDUnload(void);
static d_fdopen_t   VBoxDrvFreeBSDOpen;
static d_close_t    VBoxDrvFreeBSDClose;
static d_ioctl_t    VBoxDrvFreeBSDIOCtl;
static int VBoxDrvFreeBsdErr2Native(int rc);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Module info structure used by the kernel.
 */
static moduledata_t g_VBoxDrvFreeBSDModule =
{
    "vboxdrv",
    VBoxDrvFreeBSDModuleEvent,
    NULL
};

/** Declare the module as a pseudo device. */
DECLARE_MODULE(vboxdrv, g_VBoxDrvFreeBSDModule, SI_SUB_PSEUDO, SI_ORDER_ANY);

/**
 * The /dev/vboxdrv character device entry points.
 */
static struct cdevsw        g_VBoxDrvFreeBSDChrDevSW =
{
    .d_version =        D_VERSION,
    .d_flags =          D_TRACKCLOSE,
    .d_fdopen =         VBoxDrvFreeBSDOpen,
    .d_close =          VBoxDrvFreeBSDClose,
    .d_ioctl =          VBoxDrvFreeBSDIOCtl,
    .d_name =           "vboxdrv"
};

/** The make_dev result. */
static struct cdev         *g_pVBoxDrvFreeBSDChrDev;

/** The device extention. */
static SUPDRVDEVEXT         g_DevExt;

/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Hash table */
static PSUPDRVSESSION       g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) 	((sfn) % RT_ELEMENTS(g_apSessionHashTab))




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
    return VBoxDrvFreeBsdErr2Native(rc);
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
        rc = supdrvInitDevExt(&g_DevExt);
        if (RT_SUCCESS(rc))
        {
            /*
             * Initialize the session hash table.
             */
            rc = RTSpinlockCreate(&g_Spinlock);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Create our device node.
                 */
                /** @todo find a way to fix this 0666 permission issue. Perhaps by defining some vboxusers group with a fixed gid? */
                g_pVBoxDrvFreeBSDChrDev = make_dev(&g_VBoxDrvFreeBSDChrDevSW,
                                                   0,
                                                   UID_ROOT,
                                                   GID_WHEEL,
                                                   0666,
                                                   "vboxdrv");
                if (g_pVBoxDrvFreeBSDChrDev)
                {
                    dprintf(("VBoxDrvFreeBSDLoad: returns successfully\n"));
                    return VINF_SUCCESS;
                }

                printf("vboxdrv: make_dev failed\n");
                rc = SUPDRV_ERR_ALREADY_LOADED;
                RTSpinlockDestroy(g_Spinlock);
                g_Spinlock = NIL_RTSPINLOCK;
            }
            else
                printf("vboxdrv: RTSpinlockCreate failed, rc=%d\n", rc);
            supdrvDeleteDevExt(&g_DevExt);
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
    int rc;
    dprintf(("VBoxDrvFreeBSDUnload:\n"));

    /** @todo verify that FreeBSD does reference counting. */

    /*
     * Reserve what we did in VBoxDrvFreeBSDInit.
     */
    if (g_pVBoxDrvFreeBSDChrDev)
    {
        destroy_dev(g_pVBoxDrvFreeBSDChrDev);
        g_pVBoxDrvFreeBSDChrDev = NULL;
    }

    supdrvDeleteDevExt(&g_DevExt);

    rc = RTSpinlockDestroy(g_Spinlock);
    AssertRC(rc);
    g_Spinlock = NIL_RTSPINLOCK;

    RTR0Term();

    memset(&g_DevExt, 0, sizeof(g_DevExt));

    dprintf(("VBoxDrvFreeBSDUnload: returns\n"));
    return VINF_SUCCESS;
}


static int VBoxDrvFreeBSDOpen(struct cdev *dev, int oflags, struct thread *td, int fdidx)
{
    dprintf(("VBoxDrvFreeBSDOpen:\n"));
    return EOPNOTSUPP;
}


static int VBoxDrvFreeBSDClose(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
    dprintf(("VBoxDrvFreeBSDClose:\n"));
    return EBADF;
}


static int VBoxDrvFreeBSDIOCtl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
    dprintf(("VBoxDrvFreeBSDIOCtl:\n"));
    return EINVAL;
}


/**
 * Converts an supdrv error code to a FreeBSD error code.
 *
 * @returns corresponding FreeBSD error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int VBoxDrvFreeBsdErr2Native(int rc)
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

