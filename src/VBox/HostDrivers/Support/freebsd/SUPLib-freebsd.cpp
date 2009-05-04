/* $Id: $ */
/** @file
 * VirtualBox Support Library - FreeBSD specific parts.
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
#ifdef IN_SUP_HARDENED_R3
# undef DEBUG /* Warning: disables RT_STRICT */
# define LOG_DISABLED
  /** @todo RTLOGREL_DISABLED */
# include <iprt/log.h>
# undef LogRelIt
# define LogRelIt(pvInst, fFlags, iGroup, fmtargs) do { } while (0)
#endif

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
#include "../SUPLibInternal.h"
#include "../SUPDrvIOC.h"

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** FreeBSD base device name. */
#define DEVICE_NAME     "/dev/vboxdrv"



int suplibOsInit(PSUPLIBDATA pThis, bool fPreInited)
{
    /*
     * Nothing to do if pre-inited.
     */
    if (fPreInited)
        return VINF_SUCCESS;

    /*
     * Try open the BSD device.
     */
    int hDevice = -1;
    char szDevice[sizeof(DEVICE_NAME) + 16];
    for (unsigned iUnit = 0; iUnit < 1024; iUnit++)
    {
        errno = 0;
        snprintf(szDevice, sizeof(szDevice), DEVICE_NAME "%d", iUnit);
        hDevice = open(szDevice, O_RDWR, 0);
        if (hDevice >= 0 || errno != EBUSY)
            break;
    }
    if (hDevice < 0)
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
        LogRel(("Failed to open \"%s\", errno=%d, rc=%Rrc\n", szDevice, errno, rc));
        return rc;
    }

    /*
     * Mark the file handle close on exec.
     */
    if (fcntl(hDevice, F_SETFD, FD_CLOEXEC) != 0)
    {
#ifdef IN_SUP_HARDENED_R3
        int rc = VERR_INTERNAL_ERROR;
#else
        int err = errno;
        int rc = RTErrConvertFromErrno(err);
        LogRel(("suplibOSInit: setting FD_CLOEXEC failed, errno=%d (%Rrc)\n", err, rc));
#endif
        close(hDevice);
        return rc;
    }

    /*
     * We're done.
     */
    pThis->hDevice = hDevice;
    return VINF_SUCCESS;
}


#ifndef IN_SUP_HARDENED_R3

int suplibOsTerm(PSUPLIBDATA pThis)
{
    /*
     * Check if we're initited at all.
     */
    if (pThis->hDevice != NIL_RTFILE)
    {
        if (close(pThis->hDevice))
            AssertFailed();
        pThis->hDevice = NIL_RTFILE;
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


int suplibOsIOCtl(PSUPLIBDATA pThis, uintptr_t uFunction, void *pvReq, size_t cbReq)
{
    if (RT_LIKELY(ioctl(pThis->hDevice, uFunction, pvReq) >= 0))
	return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


int suplibOsIOCtlFast(PSUPLIBDATA pThis, uintptr_t uFunction, uintptr_t idCpu)
{
    int rc = ioctl(pThis->hDevice, uFunction, idCpu);
    if (rc == -1)
        rc = errno;
    return rc;
}


int suplibOsPageAlloc(PSUPLIBDATA pThis, size_t cPages, void **ppvPages)
{
    NOREF(pThis);
    *ppvPages = RTMemPageAllocZ(cPages << PAGE_SHIFT);
    if (*ppvPages)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


int suplibOsPageFree(PSUPLIBDATA pThis, void *pvPages, size_t /* cPages */)
{
    NOREF(pThis);
    RTMemPageFree(pvPages);
    return VINF_SUCCESS;
}

#endif /* !IN_SUP_HARDENED_R3 */

