/* $Id$ */
/** @file
 * Support Library - Solaris Specific Back-End.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Solaris device link. */
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
     * Try to open the device.
     */
    g_hDevice = open(DEVICE_NAME, O_RDWR, 0);
    if (g_hDevice < 0)
    {
        int rc;
        switch (errno)
        {
            case ENODEV:    rc = VERR_VM_DRIVER_LOAD_ERROR; break;
            case EPERM:
            case EACCES:    rc = VERR_VM_DRIVER_NOT_ACCESSIBLE; break;
            case ENOENT:    rc = VERR_VM_DRIVER_NOT_INSTALLED; break;
            default:        rc = VERR_VM_DRIVER_OPEN_ERROR; break;
        }
        LogRel(("Failed to open \"%s\", errno=%d, rc=%Vrc\n", DEVICE_NAME, errno, rc));
        return rc;
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
     * Avoid unused parameter warning
     */
    NOREF(cbReserve);

    return VINF_SUCCESS;
}


int suplibOsTerm(void)
{
    /*
     * Check if we're initialized
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
    if (RT_LIKELY(ioctl(g_hDevice, uFunction, pvReq) >= 0))
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
    *ppvPages = mmap(NULL, cPages * PAGE_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
    if (*ppvPages != (void *)-1)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


int suplibOsPageFree(void *pvPages, size_t cPages)
{
    munmap(pvPages, cPages * PAGE_SIZE);
    return VINF_SUCCESS;
}

