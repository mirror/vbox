/** @file
 * SUPLib - FreeBSD Hosts,
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
#define LOG_GROUP LOG_GROUP_SUP
#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** BSD Device name. */
#define DEVICE_NAME     "/dev/vboxdrv"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Handle to the open device. */
static int              g_hDevice = -1;


int suplibOsInit(size_t cbReserve)
{
    /*
     * Check if already initialized.
     */
    if (g_hDevice >= 0)
        return VINF_SUCCESS;

    /*
     * Try open the BSD device.
     */
    g_hDevice = open(DEVICE_NAME, O_RDWR, 0);
    if (g_hDevice < 0)
    {
        int rc = errno;
        LogRel(("Failed to open \"%s\", errno=%d\n", rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Mark the file handle close on exec.
     */
    if (fcntl(g_hDevice, F_SETFD, FD_CLOEXEC) != 0)
    {
        int rc = errno;
        LogRel(("suplibOSInit: setting FD_CLOEXEC failed, errno=%d\n", rc));
        close(g_hDevice);
        g_hDevice = -1;
        return RTErrConvertFromErrno(rc);
    }

    /*
     * We're done.
     */
    NOREF(cbReserve);
    return VINF_SUCCESS;
}


int suplibOsTerm(void)
{
    /*
     * Check if we're initited at all.
     */
    if (g_hDevice >= 0)
    {
        if (close(g_hDevice))
            AssertFailed();
        g_hDevice = -1;
    }
    return VINF_SUCCESS;
}


int suplibOsInstall(void)
{
    return VERR_NOT_IMPLEMENTED;
}


int suplibOsUninstall(void)
{
    return VERR_NOT_IMPLEMENTED;
}


int suplibOsIOCtl(uintptr_t uFunction, void *pvReq, size_t cbReq)
{
    AssertMsg(g_hDevice != -1, ("SUPLIB not initiated successfully!\n"));

    if (RT_LIKELY(ioctl((g_hDevice, uFunction, pvReq) >= 0))
	return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


int suplibOsIOCtlFast(uintptr_t uFunction)
{
    int rc = ioctl(g_hDevice, uFunction, NULL);
    if (rc == -1)
        rc = errno;
    return rc;
}


int suplibOsPageAlloc(size_t cPages, void **ppvPages)
{
    *ppvPages = RTMemPageAllocZ(cPages << PAGE_SHIFT);
    if (*ppvPages)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


int suplibOsPageFree(void *pvPages, size_t /* cPages */)
{
    RTMemPageFree(pvPages);
    return VINF_SUCCESS;
}

