/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Linux host:
 * Linux implementations for driver support library
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
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <VBox/sup.h>
#include <VBox/types.h>
#include <VBox/log.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Unix Device name. */
#define DEVICE_NAME     "/dev/vboxdrv"

/* define MADV_DONTFORK if it's missing from the system headers. */
#ifndef MADV_DONTFORK
# define MADV_DONTFORK  10
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Handle to the open device. */
static int      g_hDevice = -1;
/** Flags whether or not we've loaded the kernel module. */
static bool     g_fLoadedModule = false;
/** Indicates whether madvise(,,MADV_DONTFORK) works.  */
static bool     g_fSysMadviseWorks = false;


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
 * @param   cbReserved  Ignored on linux.
 */
int     suplibOsInit(size_t cbReserve)
{
    /*
     * Check if already initialized.
     */
    if (g_hDevice >= 0)
        return 0;

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
        {
            int rc;
            switch (errno)
            {
                case ENXIO: /* see man 2 open, ENODEV is actually a kernel bug */
                case ENODEV:    rc = VERR_VM_DRIVER_LOAD_ERROR; break;
                case EPERM:
                case EACCES:    rc = VERR_VM_DRIVER_NOT_ACCESSIBLE; break;
                case ENOENT:    rc = VERR_VM_DRIVER_NOT_INSTALLED; break;
                default:        rc = VERR_VM_DRIVER_OPEN_ERROR; break;
            }
            LogRel(("Failed to open \"%s\", errno=%d, rc=%Vrc\n", DEVICE_NAME, errno, rc));
            return rc;
        }
    }

    /*
     * Mark the file handle close on exec.
     */
    if (fcntl(g_hDevice, F_SETFD, FD_CLOEXEC) == -1)
    {
        close(g_hDevice);
        g_hDevice = -1;
        return RTErrConvertFromErrno(errno);
    }

    /*
     * Check if madvise works.
     */
    void *pv = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pv == MAP_FAILED)
        return VERR_NO_MEMORY;
    g_fSysMadviseWorks = (0 == madvise(pv, PAGE_SIZE, MADV_DONTFORK));
    munmap(pv, PAGE_SIZE);

    /*
     * We're done.
     */
    NOREF(cbReserve);
    return 0;
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
    // nothing to do on Linux
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
    // nothing to do on Linux
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Send a I/O Control request to the device.
 *
 * @returns 0 on success.
 * @returns VBOX error code on failure.
 * @param   uFunction   IO Control function.
 * @param   pvReq       The request buffer.
 * @param   cbReq       The size of the request buffer.
 */
int suplibOsIOCtl(uintptr_t uFunction, void *pvReq, size_t cbReq)
{
    AssertMsg(g_hDevice != -1, ("SUPLIB not initiated successfully!\n"));

    /*
     * Issue device iocontrol.
     */
    if (RT_LIKELY(ioctl(g_hDevice, uFunction, pvReq) >= 0))
	return VINF_SUCCESS;

    /* This is the reverse operation of the one found in SUPDrv-linux.c */
    switch (errno)
    {
        case EACCES: return VERR_GENERAL_FAILURE;
        case EINVAL: return VERR_INVALID_PARAMETER;
        case EILSEQ: return VERR_INVALID_MAGIC;
        case ENXIO:  return VERR_INVALID_HANDLE;
        case EFAULT: return VERR_INVALID_POINTER;
        case ENOLCK: return VERR_LOCK_FAILED;
        case EEXIST: return VERR_ALREADY_LOADED;
        case EPERM:  return VERR_PERMISSION_DENIED;
        case ENOSYS: return VERR_VERSION_MISMATCH;
        case 1000:   return VERR_IDT_FAILED;
    }

    return RTErrConvertFromErrno(errno);
}


/**
 * Fast I/O Control path, no buffers.
 *
 * @returns VBox status code.
 * @param   uFunction   The operation.
 */
int suplibOsIOCtlFast(uintptr_t uFunction)
{
    int rc = ioctl(g_hDevice, uFunction, NULL);
    if (rc == -1)
        rc = -errno;
    return rc;
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
    size_t cbMmap = (g_fSysMadviseWorks ? cPages : cPages + 2) << PAGE_SHIFT;
    char *pvPages = (char *)mmap(NULL, cbMmap, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pvPages == MAP_FAILED)
	return VERR_NO_MEMORY;

    if (g_fSysMadviseWorks)
    {
	/*
	 * It is not fatal if we fail here but a forked child (e.g. the ALSA sound server)
	 * could crash. Linux < 2.6.16 does not implement madvise(MADV_DONTFORK) but the
	 * kernel seems to split bigger VMAs and that is all that we want -- later we set the
	 * VM_DONTCOPY attribute in supdrvOSLockMemOne().
	 */
	if (madvise (pvPages, cbMmap, MADV_DONTFORK))
	    LogRel(("SUPLib: madvise %p-%p failed\n", pvPages, cbMmap));
	*ppvPages = pvPages;
    }
    else
    {
	/*
	 * madvise(MADV_DONTFORK) is not available (most probably Linux 2.4). Enclose any
	 * mmapped region by two unmapped pages to guarantee that there is exactly one VM
	 * area struct of the very same size as the mmap area.
	 */
	mprotect(pvPages,                      PAGE_SIZE, PROT_NONE);
	mprotect(pvPages + cbMmap - PAGE_SIZE, PAGE_SIZE, PROT_NONE);
	*ppvPages = pvPages + PAGE_SIZE;
    }
    memset(*ppvPages, 0, cPages << PAGE_SHIFT);
    return VINF_SUCCESS;
}


/**
 * Frees pages allocated by suplibOsPageAlloc().
 *
 * @returns VBox status code.
 * @param   pvPages     Pointer to pages.
 */
int     suplibOsPageFree(void *pvPages, size_t cPages)
{
    munmap(pvPages, cPages << PAGE_SHIFT);
    return VINF_SUCCESS;
}
