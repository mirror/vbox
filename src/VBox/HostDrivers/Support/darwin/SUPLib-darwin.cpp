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
        LogRel(("SUP: Couldn't find any matches. The kernel module is probably not loaded.\n"));
        return VERR_VM_DRIVER_NOT_INSTALLED;
    }

    /*
     * Open the service.
     * This will cause the user client class in SUPDrv-darwin.cpp to be instantiated.
     */
    kr = IOServiceOpen(ServiceObject, mach_task_self(), 0, &g_Connection);
    IOObjectRelease(ServiceObject);
    if (kr != kIOReturnSuccess)
    {
        LogRel(("SUP: IOServiceOpen returned %d. Driver open failed.\n", kr));
        return VERR_VM_DRIVER_OPEN_ERROR;
    }

    /*
     * Now, try open the BSD device.
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
        LogRel(("SUP: Failed to open \"%s\", errno=%d, rc=%Vrc\n", DEVICE_NAME, errno, rc));

        kr = IOServiceClose(g_Connection);
        if (kr != kIOReturnSuccess)
            LogRel(("Warning: IOServiceClose(%p) returned %d\n", g_Connection, kr));
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
    *ppvPages = valloc(cPages << PAGE_SHIFT);
    if (*ppvPages)
    {
        memset(*ppvPages, 0, cPages << PAGE_SHIFT);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


int suplibOsPageFree(void *pvPages, size_t /* cPages */)
{
    free(pvPages);
    return VINF_SUCCESS;
}

