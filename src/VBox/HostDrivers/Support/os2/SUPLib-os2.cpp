/** @file
 *
 * VBox host drivers - Ring-0 support drivers - OS/2 host:
 * OS/2 implementations for support library
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define INCL_BASE
#define INCL_ERRORS
#include <os2.h>
#undef RT_MAX

#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** OS/2 Device name. */
#define DEVICE_NAME     "/dev/vboxdrv$"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Handle to the open device. */
static HFILE    g_hDevice = (HFILE)-1;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


/**
 * Initialize the OS specific part of the library.
 * On Linux this involves:
 *      - loading the module.
 *      - open driver.
 *
 * @returns 0 on success.
 * @returns current -1 on failure but this must be changed to proper error codes.
 * @param   cbReserve   Ignored on OS/2.
 */
int     suplibOsInit(size_t cbReserve)
{
    /*
     * Check if already initialized.
     */
    if (g_hDevice != (HFILE)-1)
        return 0;

    /*
     * Try open the device.
     */
    ULONG ulAction = 0;
    HFILE hDevice = (HFILE)-1;
    APIRET rc = DosOpen((PCSZ)DEVICE_NAME,
                        &hDevice,
                        &ulAction,
                        0,
                        FILE_NORMAL,
                        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                        OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
                        NULL);
    if (rc)
        return RTErrConvertFromOS2(rc);
    g_hDevice = hDevice;

    NOREF(cbReserve);
    return VINF_SUCCESS;
}


int     suplibOsTerm(void)
{
    /*
     * Check if we're initited at all.
     */
    if (g_hDevice != (HFILE)-1)
    {
        APIRET rc = DosClose(g_hDevice);
        AssertMsg(rc == NO_ERROR, ("%d\n", rc)); NOREF(rc);
        g_hDevice = (HFILE)-1;
    }

    return 0;
}


int suplibOsInstall(void)
{
    /** @remark OS/2: Not supported */
    return VERR_NOT_SUPPORTED;
}


int suplibOsUninstall(void)
{
    /** @remark OS/2: Not supported */
    return VERR_NOT_SUPPORTED;
}


int suplibOsIOCtl(unsigned uFunction, void *pvIn, size_t cbIn, void *pvOut, size_t cbOut)
{
    AssertMsg(g_hDevice != (HFILE)-1, ("SUPLIB not initiated successfully!\n"));

    SUPDRVIOCTLDATA Args;
    Args.pvIn = pvIn;
    Args.cbIn = cbIn;
    Args.pvOut = pvOut;
    Args.cbOut = cbOut;
    Args.rc = VERR_INTERNAL_ERROR;

    ULONG cbReturned = sizeof(Args);
    int rc = DosDevIOCtl(g_hDevice, SUP_CTL_CATEGORY, uFunction,
                         &Args, sizeof(Args), &cbReturned,
                         NULL, 0, NULL);
    if (RT_LIKELY(rc == NO_ERROR))
        rc = Args.rc;
    else
        rc = RTErrConvertFromOS2(rc);
    return rc;
}


#ifdef VBOX_WITHOUT_IDT_PATCHING
int suplibOSIOCtlFast(unsigned uFunction)
{
    int32_t rcRet = VERR_INTERNAL_ERROR;
    ULONG cbRet = sizeof(rcRet);
    int rc = DosDevIOCtl(g_hDevice, SUP_CTL_CATEGORY_FAST, uFunction,
                         NULL, 0, NULL,
                         &rcRet, sizeof(rcRet), &cbRet);
    if (RT_LIKELY(rc == NO_ERROR))
        rc = rcRet;
    else
        rc = RTErrConvertFromOS2(rc);
    return rc;
}
#endif


int suplibOsPageAlloc(size_t cPages, void **ppvPages)
{
    *ppvPages = NULL;
    int rc = DosAllocMem(ppvPages, cPages << PAGE_SHIFT, PAG_READ | PAG_WRITE | PAG_EXECUTE | PAG_COMMIT | OBJ_ANY);
    if (rc == ERROR_INVALID_PARAMETER)
        rc = DosAllocMem(ppvPages, cPages << PAGE_SHIFT, PAG_READ | PAG_WRITE | PAG_EXECUTE | PAG_COMMIT | OBJ_ANY);
    if (!rc)
        rc = VINF_SUCCESS;
    else
        rc = RTErrConvertFromOS2(rc);
    return rc;
}


int suplibOsPageFree(void *pvPages, size_t /* cPages */)
{
    if (pvPages)
    {
        int rc = DosFreeMem(pvPages);
        Assert(!rc);
    }
    return VINF_SUCCESS;
}

