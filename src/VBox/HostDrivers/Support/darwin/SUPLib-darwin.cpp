/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Darwin host:
 * Darwin implementations for support library
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
#include <iprt/err.h>
#include <iprt/string.h>
#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** BSD Device name. */
#define DEVICE_NAME     "/dev/vboxdrv"
/** The IOClass key of the service (see SUPDrv-darwin.cpp / Info.plist). */
#define IOCLASS_NAME    "org_virtualbox_SupDrv"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Handle to the open device. */
static int              g_hDevice = -1;
/** The IOMasterPort. */
static mach_port_t      g_MasterPort = 0;
/** The current service connection. */
static io_connect_t     g_Connection = NULL;


int suplibOsInit(size_t cbReserve)
{
    /*
     * Check if already initialized.
     */
    if (g_hDevice >= 0)
        return VINF_SUCCESS;

    /*
     * Open the IOKit client first - The first step is finding the service.
     */
    mach_port_t MasterPort;
    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &MasterPort);
    if (kr != kIOReturnSuccess)
    {
        LogRel(("IOMasterPort -> %d\n", kr));
        return VERR_GENERAL_FAILURE;
    }

    CFDictionaryRef ClassToMatch = IOServiceMatching(IOCLASS_NAME);
    if (!ClassToMatch)
    {
        LogRel(("IOServiceMatching(\"%s\") failed.\n", IOCLASS_NAME));
        return VERR_GENERAL_FAILURE;
    }

    /* Create an io_iterator_t for all instances of our drivers class that exist in the IORegistry. */
    io_iterator_t Iterator;
    kr = IOServiceGetMatchingServices(g_MasterPort, ClassToMatch, &Iterator);
    if (kr != kIOReturnSuccess)
    {
        LogRel(("IOServiceGetMatchingServices returned %d\n", kr));
        return VERR_GENERAL_FAILURE;
    }

    /* Get the first item in the iterator and release it. */
    io_service_t ServiceObject = IOIteratorNext(Iterator);
    IOObjectRelease(Iterator);
    if (!ServiceObject)
    {
        LogRel(("Couldn't find any matches.\n"));
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Open the service.
     * This will cause the user client class in SUPDrv-darwin.cpp to be instantiated.
     */
    kr = IOServiceOpen(ServiceObject, mach_task_self(), 0, &g_Connection);
    IOObjectRelease(ServiceObject);
    if (kr != kIOReturnSuccess)
    {
        LogRel(("IOServiceOpen returned %d\n", kr));
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Now, try open the BSD device.
     */
    g_hDevice = open(DEVICE_NAME, O_RDWR, 0);
    if (g_hDevice < 0)
    {
        int rc = errno;
        LogRel(("Failed to open \"%s\", errno=%d\n", rc));
        kr = IOServiceClose(g_Connection);
        if (kr != kIOReturnSuccess)
            LogRel(("Warning: IOServiceClose(%p) returned %d\n", g_Connection, kr));
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

    /*
     * Close the connection to the IOService and destroy the connection handle.
     */
    if (g_Connection)
    {
        kern_return_t kr = IOServiceClose(g_Connection);
        if (kr != kIOReturnSuccess)
        {
            LogRel(("Warning: IOServiceClose(%p) returned %d\n", g_Connection, kr));
            AssertFailed();
        }
        g_Connection = NULL;
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
    *ppvPages = valloc(cPages << PAGE_SHIFT);
    if (*ppvPages)
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
int suplibOsPageFree(void *pvPages, size_t /* cPages */)
{
    free(pvPages);
    return VINF_SUCCESS;
}

