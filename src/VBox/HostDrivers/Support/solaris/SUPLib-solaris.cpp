/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Solaris host:
 * Solaris implementations for support library
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
#include <stdlib.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
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
        int rc = errno;
        LogRel(("Failed to open \"%s\", errno=%d\n", rc));
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


/**
 * Installs anything required by the support library.
 *
 * @returns 0 on success.
 * @returns error code on failure.
 */
int suplibOsInstall(void)
{
//    int rc = mknod(DEVICE_NAME, S_IFCHR, );

    return VERR_NOT_IMPLEMENTED;
}

/**
 * Installs anything required by the support library.
 *
 * @returns 0 on success.
 * @returns error code on failure.
 */
int suplibOsUninstall(void)
{
//    int rc = unlink(DEVICE_NAME);

    return VERR_NOT_IMPLEMENTED;
}

/**
 * Send a I/O Control request to the device.
 *
 * @returns 0 on success.
 * @returns VBOX error code on failure.
 * @param   uFunction   IO Control function.
 * @param   pvIn        Input data buffer.
 * @param   cbIn        Size of input data.
 * @param   pvOut       Output data buffer.
 * @param   cbOut       Size of output data.
 */
int suplibOsIOCtl(unsigned uFunction, void *pvIn, size_t cbIn, void *pvOut, size_t cbOut)
{
    AssertMsg(g_hDevice != -1, ("SUPLIB not initiated successfully!\n"));
    /*
     * Issue device iocontrol.
     */
    SUPDRVIOCTLDATA Args;
    Args.pvIn = pvIn;
    Args.cbIn = cbIn;
    Args.pvOut = pvOut;
    Args.cbOut = cbOut;

    if (ioctl(g_hDevice, uFunction, &Args) >= 0)
	return 0;
    /* This is the reverse operation of the one found in SUPDrv-linux.c */
    switch (errno)
    {
        case EACCES: return VERR_GENERAL_FAILURE;
        case EINVAL: return VERR_INVALID_PARAMETER;
        case ENOSYS: return VERR_INVALID_MAGIC;
        case ENXIO:  return VERR_INVALID_HANDLE;
        case EFAULT: return VERR_INVALID_POINTER;
        case ENOLCK: return VERR_LOCK_FAILED;
        case EEXIST: return VERR_ALREADY_LOADED;
    }

    return RTErrConvertFromErrno(errno);
}


#ifdef VBOX_WITHOUT_IDT_PATCHING
int suplibOSIOCtlFast(unsigned uFunction)
{
    int rc = ioctl(g_hDevice, uFunction, NULL);
    if (rc == -1)
        rc = errno;
    return rc;
}
#endif


/**
 * Allocate a number of zero-filled pages in user space.
 *
 * @returns VBox status code.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvPages    Where to return the base pointer.
 */
int suplibOsPageAlloc(size_t cPages, void **ppvPages)
{
    *ppvPages = RTMemPageAllocZ(cPages << PAGE_SHIFT);
    if (*ppvPages)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


/**
 * Frees pages allocated by suplibOsPageAlloc().
 *
 * @returns VBox status code.
 * @param   pvPages     Pointer to pages.
 */
int suplibOsPageFree(void *pvPages, size_t /* cPages */)
{
    RTMemPageFree(pvPages);
    return VINF_SUCCESS;
}

