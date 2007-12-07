/* $Id$ */
/** @file
 * SUPLib - Support Library, OS/2 backend.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define INCL_BASE
#define INCL_ERRORS
#include <os2.h>
#undef RT_MAX

#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
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
    {
        int vrc;
        switch (rc)
        {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:  vrc = VERR_VM_DRIVER_NOT_INSTALLED; break;
            default:                    vrc = VERR_VM_DRIVER_OPEN_ERROR; break;
        }
        LogRel(("Failed to open \"%s\", rc=%d, vrc=%Vrc\n", DEVICE_NAME, rc, vrc));
        return vrc;
    }
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


int suplibOsIOCtl(uintptr_t uFunction, void *pvReq, size_t cbReq)
{
    AssertMsg(g_hDevice != (HFILE)-1, ("SUPLIB not initiated successfully!\n"));

    ULONG cbReturned = sizeof(SUPREQHDR);
    int rc = DosDevIOCtl(g_hDevice, SUP_CTL_CATEGORY, uFunction,
                         pvReq, cbReturned, &cbReturned,
                         NULL, 0, NULL);
    if (RT_LIKELY(rc == NO_ERROR))
        return VINF_SUCCESS;
    return RTErrConvertFromOS2(rc);
}


int suplibOsIOCtlFast(uintptr_t uFunction)
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
        Assert(!rc); NOREF(rc);
    }
    return VINF_SUCCESS;
}

