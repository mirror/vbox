/** @file
 *
 * VBox host drivers - Ring-0 support drivers - OS/2 host:
 * OS/2 implementations for support library
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** OS/2 Device name. */
#define DEVICE_NAME     "/dev/$vboxdrv"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Handle to the open device. */
static int      g_hDevice = -1;
/** Flags whether or not we've loaded the kernel module. */
static bool     g_fLoadedModule = false;


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
    if (g_hDevice >= 0)
        return 0;

#if 0
    /*
     * Try open the device.
     */
    g_hDevice = open(DEVICE_NAME, O_RDWR, 0);
    if (g_hDevice < 0)
    {
        /*
         * Try load the device.
         */
        //todo suplibOsLoadKernelModule();
        g_hDevice = open(DEVICE_NAME, O_RDWR, 0);
        if (g_hDevice < 0)
            return RTErrConvertFromErrno(errno);
    }

    /*
     * Check driver version.
     */
    /** @todo implement driver version checking. */

    /*
     * We're done.
     */
    NOREF(cbReserve);
    return 0;
#else
    NOREF(cbReserve);
    return VERR_NOT_IMPLEMENTED;
#endif
}


int     suplibOsTerm(void)
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

    /*
     * If we started the service we might consider stopping it too.
     *
     * Since this won't work unless the the process starting it is the
     * last user we might wanna skip this...
     */
    if (g_fLoadedModule)
    {
        //todo kernel module unloading.
        //suplibOsStopService();
        //g_fStartedService = false;
    }

    return 0;
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
int     suplibOsIOCtl(unsigned uFunction, void *pvIn, size_t cbIn, void *pvOut, size_t cbOut)
{
#if 0
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
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Allocate a number of zero-filled pages in user space.
 *
 * @returns VBox status code.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvPages    Where to return the base pointer.
 */
int     suplibOsPageAlloc(size_t cPages, void **ppvPages)
{
    if (!posix_memalign(ppvPages, PAGE_SIZE, cPages << PAGE_SHIFT))
    {
        memset(*ppvPages, 0, cPages << PAGE_SHIFT);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


/**
 * Frees pages allocated by suplibOsPageAlloc().
 *
 * @returns VBox status code.
 * @param   pvPages     Pointer to pages.
 */
int     suplibOsPageFree(void *pvPages)
{
    free(pvPages);
    return VINF_SUCCESS;
}





