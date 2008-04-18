/** $Id$ */
/** @file
 * VirtualBox Support Library - Windows NT specific parts.
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
#include <Windows.h>

#include <VBox/sup.h>
#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include "SUPLibInternal.h"
#include "SUPDRVIOC.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The support service name. */
#define SERVICE_NAME    "VBoxDrv"
/** Win32 Device name. */
#define DEVICE_NAME     "\\\\.\\VBoxDrv"
/** NT Device name. */
#define DEVICE_NAME_NT   L"\\Device\\VBoxDrv"
/** Win32 Symlink name. */
#define DEVICE_NAME_DOS  L"\\DosDevices\\VBoxDrv"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Handle to the open device. */
static HANDLE   g_hDevice = INVALID_HANDLE_VALUE;
/** Flags whether or not we started the service. */
static bool     g_fStartedService = false;
/** Pointer to the area of memory we reserve for SUPPageAlloc(). */
static void    *g_pvReserved = NULL;
/** The number of bytes we reserved for SUPPageAlloc(). */
static size_t   g_cbReserved = 0;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int suplibOsCreateService(void);
static int suplibOsUpdateService(void);
static int suplibOsDeleteService(void);
static int suplibOsStartService(void);
static int suplibOsStopService(void);
static int suplibConvertWin32Err(int);


/**
 * Initialize the OS specific part of the library.
 * On Win32 this involves:
 *      - registering the device driver
 *      - start device driver.
 *      - open driver.
 *
 * @returns 0 on success.
 * @returns current -1 on failure but this must be changed to proper error codes.
 * @param   cbReserve       The number of bytes to reserver for contiguous virtual allocations.
 */
int     suplibOsInit(size_t cbReserve)
{
    /*
     * Check if already initialized.
     */
    if (g_hDevice != INVALID_HANDLE_VALUE)
        return 0;

    /*
     * Try open the device.
     */
    g_hDevice = CreateFile(DEVICE_NAME,
                           GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                           NULL);
    if (g_hDevice == INVALID_HANDLE_VALUE)
    {
        /*
         * Try start the service and retry opening it.
         */
        suplibOsStartService();
        g_hDevice = CreateFile(DEVICE_NAME,
                               GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                               NULL);
        if (g_hDevice == INVALID_HANDLE_VALUE)
        {
            int rc = GetLastError();
            switch (rc)
            {
                /** @todo someone must test what is actually returned. */
                case ERROR_DEV_NOT_EXIST:
                case ERROR_DEVICE_NOT_CONNECTED:
                case ERROR_BAD_DEVICE:
                case ERROR_DEVICE_REMOVED:
                case ERROR_DEVICE_NOT_AVAILABLE:
                    return VERR_VM_DRIVER_LOAD_ERROR;
                case ERROR_PATH_NOT_FOUND:
                case ERROR_FILE_NOT_FOUND:
                    return VERR_VM_DRIVER_NOT_INSTALLED;
                case ERROR_ACCESS_DENIED:
                case ERROR_SHARING_VIOLATION:
                    return VERR_VM_DRIVER_NOT_ACCESSIBLE;
                default:
                    return VERR_VM_DRIVER_OPEN_ERROR;
            }

            return -1 /** @todo define proper error codes for suplibOsInit failure. */;
        }
    }

    /*
     * Check driver version.
     */
    /** @todo implement driver version checking. */

#if 0 /* obsolete code and restricts our virtual address space for the new allocation method */
    /*
     * Reserve memory.
     */
    if (cbReserve != 0)
    {
/** 1 1/2 GB - (a bit more than) current VBox max. */
#define SUPLIB_MAX_RESERVE (_1G + _1M*512)
        /*
         * Find the right size to reserve.
         */
        if (    cbReserve == ~(size_t)0
            ||  cbReserve > SUPLIB_MAX_RESERVE)
            cbReserve = SUPLIB_MAX_RESERVE;
        char szVar[64] = {0};
        if (GetEnvironmentVariable("VBOX_RESERVE_MEM_LIMIT", szVar, sizeof(szVar) - 1))
        {
            uint64_t cb;
            char    *pszNext;
            int rc = RTStrToUInt64Ex(szVar, &pszNext, 0, &cb);
            if (VBOX_SUCCESS(rc))
            {
                switch (*pszNext)
                {
                    case 'K':
                    case 'k':
                        cb *= _1K;
                        pszNext++;
                        break;
                    case 'M':
                    case 'm':
                        cb *= _1M;
                        pszNext++;
                        break;
                    case 'G':
                    case 'g':
                        cb *= _1G;
                        pszNext++;
                        break;
                    case '\0':
                        break;
                }
                if (*pszNext == 'b' || *pszNext == 'B')
                    pszNext++;
                if (!pszNext)
                    cbReserve = RT_MIN(SUPLIB_MAX_RESERVE, cb);
            }
        }

        /*
         * Try reserve virtual address space, lowering the requirements in by _1M chunks.
         * Make sure it's possible to get at least 3 chunks of 16MBs extra after the reservation.
         */
        for (cbReserve = RT_ALIGN_Z(cbReserve, _1M); cbReserve >= _1M * 64; cbReserve -= _1M)
        {
            void *pv = VirtualAlloc(NULL, cbReserve, MEM_RESERVE, PAGE_NOACCESS);
            if (pv)
            {
                void *pv1 = VirtualAlloc(NULL, _1M * 16, MEM_RESERVE, PAGE_NOACCESS);
                void *pv2 = VirtualAlloc(NULL, _1M * 16, MEM_RESERVE, PAGE_NOACCESS);
                void *pv3 = VirtualAlloc(NULL, _1M * 16, MEM_RESERVE, PAGE_NOACCESS);
                if (pv1)
                    VirtualFree(pv1, 0, MEM_RELEASE);
                if (pv2)
                    VirtualFree(pv2, 0, MEM_RELEASE);
                if (pv3)
                    VirtualFree(pv3, 0, MEM_RELEASE);
                const int cFailures = !pv1 + !pv2 + !pv3;
                if (!cFailures)
                {
                    g_pvReserved = pv;
                    g_cbReserved = cbReserve;
#if 0 /* too early, no logging. */
                    Log(("suplibOsInit: Reserved %zu bytes at %p\n", cbReserve, g_pvReserved));
#endif
                    break;
                }

                cbReserve -= cFailures > 2 ? _1M * 16 : _1M;
            }
            else
                cbReserve -= _1M;
        }
        /* ignore errors */
    }
#endif /* end of obsolete memory reservation hack */

    /*
     * We're done.
     */
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
    return suplibOsCreateService();
}


/**
 * Installs anything required by the support library.
 *
 * @returns 0 on success.
 * @returns error code on failure.
 */
int suplibOsUninstall(void)
{
    int rc = suplibOsStopService();
    if (!rc)
        rc = suplibOsDeleteService();
    return rc;
}


/**
 * Creates the service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int suplibOsCreateService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    SC_HANDLE   hSMgrCreate = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgrCreate, ("OpenSCManager(,,create) failed rc=%d\n", LastError));
    if (hSMgrCreate)
    {
        char szDriver[RTPATH_MAX];
        int rc = RTPathProgram(szDriver, sizeof(szDriver) - sizeof("\\VBoxDrv.sys"));
        if (VBOX_SUCCESS(rc))
        {
            strcat(szDriver, "\\VBoxDrv.sys");
            SC_HANDLE hService = CreateService(hSMgrCreate,
                                               SERVICE_NAME,
                                               "VBox Support Driver",
                                               SERVICE_QUERY_STATUS,
                                               SERVICE_KERNEL_DRIVER,
                                               SERVICE_DEMAND_START,
                                               SERVICE_ERROR_NORMAL,
                                               szDriver,
                                               NULL, NULL, NULL, NULL, NULL);
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsg(hService, ("CreateService failed! LastError=%Rwa szDriver=%s\n", LastError, szDriver));
            CloseServiceHandle(hService);
            CloseServiceHandle(hSMgrCreate);
            return hService ? 0 : -1;
        }
        CloseServiceHandle(hSMgrCreate);
        return rc;
    }
    return -1;
}

/**
 * Stops a possibly running service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int suplibOsStopService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_STOP | SERVICE_QUERY_STATUS);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgr, ("OpenSCManager(,,delete) failed rc=%d\n", LastError));
    if (hSMgr)
    {
        SC_HANDLE hService = OpenService(hSMgr, SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (hService)
        {
            /*
             * Stop the service.
             */
            SERVICE_STATUS  Status;
            QueryServiceStatus(hService, &Status);
            if (Status.dwCurrentState == SERVICE_STOPPED)
                rc = 0;
            else if (ControlService(hService, SERVICE_CONTROL_STOP, &Status))
            {
                int iWait = 100;
                while (Status.dwCurrentState == SERVICE_STOP_PENDING && iWait-- > 0)
                {
                    Sleep(100);
                    QueryServiceStatus(hService, &Status);
                }
                if (Status.dwCurrentState == SERVICE_STOPPED)
                    rc = 0;
                else
                   AssertMsgFailed(("Failed to stop service. status=%d\n", Status.dwCurrentState));
            }
            else
            {
                DWORD LastError = GetLastError(); NOREF(LastError);
                AssertMsgFailed(("ControlService failed with LastError=%Rwa. status=%d\n", LastError, Status.dwCurrentState));
            }
            CloseServiceHandle(hService);
        }
        else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
            rc = 0;
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("OpenService failed LastError=%Rwa\n", LastError));
        }
        CloseServiceHandle(hSMgr);
    }
    return rc;
}


/**
 * Deletes the service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int suplibOsDeleteService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgr, ("OpenSCManager(,,delete) failed rc=%d\n", LastError));
    if (hSMgr)
    {
        SC_HANDLE hService = OpenService(hSMgr, SERVICE_NAME, DELETE);
        if (hService)
        {
            /*
             * Delete the service.
             */
            if (DeleteService(hService))
                rc = 0;
            else
            {
                DWORD LastError = GetLastError(); NOREF(LastError);
                AssertMsgFailed(("DeleteService failed LastError=%Rwa\n", LastError));
            }
            CloseServiceHandle(hService);
        }
        else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
            rc = 0;
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("OpenService failed LastError=%Rwa\n", LastError));
        }
        CloseServiceHandle(hSMgr);
    }
    return rc;
}

#if 0
/**
 * Creates the service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int suplibOsUpdateService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgr, ("OpenSCManager(,,delete) failed LastError=%Rwa\n", LastError));
    if (hSMgr)
    {
        SC_HANDLE hService = OpenService(hSMgr, SERVICE_NAME, SERVICE_CHANGE_CONFIG);
        if (hService)
        {
            char szDriver[RTPATH_MAX];
            int rc = RTPathProgram(szDriver, sizeof(szDriver) - sizeof("\\VBoxDrv.sys"));
            if (VBOX_SUCCESS(rc))
            {
                strcat(szDriver, "\\VBoxDrv.sys");

                SC_LOCK hLock = LockServiceDatabase(hSMgr);
                if (ChangeServiceConfig(hService,
                                        SERVICE_KERNEL_DRIVER,
                                        SERVICE_DEMAND_START,
                                        SERVICE_ERROR_NORMAL,
                                        szDriver,
                                        NULL, NULL, NULL, NULL, NULL, NULL))
                {

                    UnlockServiceDatabase(hLock);
                    CloseServiceHandle(hService);
                    CloseServiceHandle(hSMgr);
                    return 0;
                }
                else
                {
                    DWORD LastError = GetLastError(); NOREF(LastError);
                    AssertMsgFailed(("ChangeServiceConfig failed LastError=%Rwa\n", LastError));
                }
            }
            UnlockServiceDatabase(hLock);
            CloseServiceHandle(hService);
        }
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("OpenService failed LastError=%Rwa\n", LastError));
        }
        CloseServiceHandle(hSMgr);
    }
    return -1;
}
#endif

/**
 * Attempts to start the service, creating it if necessary.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 * @param   fRetry  Indicates retry call.
 */
int suplibOsStartService(void)
{
    /*
     * Check if the driver service is there.
     */
    SC_HANDLE hSMgr = OpenSCManager(NULL, NULL, SERVICE_QUERY_STATUS | SERVICE_START);
    if (hSMgr == NULL)
    {
        AssertMsgFailed(("couldn't open service manager in SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS mode!\n"));
        return -1;
    }

    /*
     * Try open our service to check it's status.
     */
    SC_HANDLE hService = OpenService(hSMgr, SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_START);
    if (!hService)
    {
        /*
         * Create the service.
         */
        int rc = suplibOsCreateService();
        if (rc)
            return rc;

        /*
         * Try open the service.
         */
        hService = OpenService(hSMgr, SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_START);
    }

    /*
     * Check if open and on demand create succeeded.
     */
    int rc = -1;
    if (hService)
    {

        /*
         * Query service status to see if we need to start it or not.
         */
        SERVICE_STATUS  Status;
        BOOL fRc = QueryServiceStatus(hService, &Status);
        Assert(fRc);
        if (    Status.dwCurrentState != SERVICE_RUNNING
            &&  Status.dwCurrentState != SERVICE_START_PENDING)
        {
            /*
             * Start it.
             */
            fRc = StartService(hService, 0, NULL);
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsg(fRc, ("StartService failed with LastError=%Rwa\n", LastError));
        }

        /*
         * Wait for the service to finish starting.
         * We'll wait for 10 seconds then we'll give up.
         */
        QueryServiceStatus(hService, &Status);
        if (Status.dwCurrentState == SERVICE_START_PENDING)
        {
            int iWait;
            for (iWait = 100; iWait > 0 && Status.dwCurrentState == SERVICE_START_PENDING; iWait--)
            {
                Sleep(100);
                QueryServiceStatus(hService, &Status);
            }
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsg(Status.dwCurrentState != SERVICE_RUNNING,
                      ("Failed to start. LastError=%Rwa iWait=%d status=%d\n",
                       LastError, iWait, Status.dwCurrentState));
        }

        if (Status.dwCurrentState == SERVICE_RUNNING)
            rc = 0;

        /*
         * Close open handles.
         */
        CloseServiceHandle(hService);
    }
    else
    {
        DWORD LastError = GetLastError(); NOREF(LastError);
        AssertMsgFailed(("OpenService failed! LastError=%Rwa\n", LastError));
    }
    if (!CloseServiceHandle(hSMgr))
        AssertFailed();

    return rc;
}


int     suplibOsTerm(void)
{
    /*
     * Check if we're initited at all.
     */
    if (g_hDevice != INVALID_HANDLE_VALUE)
    {
        if (!CloseHandle(g_hDevice))
            AssertFailed();
        g_hDevice = INVALID_HANDLE_VALUE;
    }

    /*
     * If we started the service we might consider stopping it too.
     *
     * Since this won't work unless the the process starting it is the
     * last user we might wanna skip this...
     */
    if (g_fStartedService)
    {
        suplibOsStopService();
        g_fStartedService = false;
    }

    return 0;
}


/**
 * Send a I/O Control request to the device.
 *
 * @returns 0 on success.
 * @returns VBOX error code on failure.
 * @param   uFunction   IO Control function.
 * @param   pvIn        The request buffer.
 * @param   cbReq       The size of the request buffer.
 */
int suplibOsIOCtl(uintptr_t uFunction, void *pvReq, size_t cbReq)
{
    AssertMsg(g_hDevice != INVALID_HANDLE_VALUE, ("SUPLIB not initiated successfully!\n"));

    /*
     * Issue the device I/O control.
     */
    PSUPREQHDR pHdr = (PSUPREQHDR)pvReq;
    Assert(cbReq == RT_MAX(pHdr->cbIn, pHdr->cbOut));
    DWORD cbReturned = (ULONG)pHdr->cbOut;
    if (DeviceIoControl(g_hDevice, uFunction, pvReq, pHdr->cbIn, pvReq, cbReturned, &cbReturned, NULL))
        return 0;

    return suplibConvertWin32Err(GetLastError());
}


int suplibOsIOCtlFast(uintptr_t uFunction)
{
    /*
     * Issue device I/O control.
     */
    int rc = VERR_INTERNAL_ERROR;
    DWORD cbReturned = (ULONG)sizeof(rc);
    if (DeviceIoControl(g_hDevice, uFunction, NULL, 0, &rc, (DWORD)sizeof(rc), &cbReturned, NULL))
        return rc;
    return suplibConvertWin32Err(GetLastError());
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
    if (g_pvReserved)
    {
        if (VirtualFree(g_pvReserved, 0, MEM_RELEASE))
            Log(("suplibOsPageAlloc: Freed %zu bytes of reserved memory at %p.\n", g_cbReserved, g_pvReserved));
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("LastError=%Rwa g_pvReserved=%p\n", LastError, g_pvReserved));
        }
        g_pvReserved = NULL;
    }

    *ppvPages = VirtualAlloc(NULL, (size_t)cPages << PAGE_SHIFT, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (*ppvPages)
        return VINF_SUCCESS;
    return suplibConvertWin32Err(GetLastError());
}


/**
 * Frees pages allocated by suplibOsPageAlloc().
 *
 * @returns VBox status code.
 * @param   pvPages     Pointer to pages.
 */
int     suplibOsPageFree(void *pvPages, size_t /* cPages */)
{
    if (VirtualFree(pvPages, 0, MEM_RELEASE))
        return VINF_SUCCESS;
    return suplibConvertWin32Err(GetLastError());
}


/**
 * Converts a supdrv error code to an nt status code.
 *
 * @returns corresponding SUPDRV_ERR_*.
 * @param   rc  Win32 error code.
 */
static int     suplibConvertWin32Err(int rc)
{
    /* Conversion program (link with ntdll.lib from ddk):
        #define _WIN32_WINNT 0x0501
        #include <windows.h>
        #include <ntstatus.h>
        #include <winternl.h>
        #include <stdio.h>

        int main()
        {
            #define CONVERT(a)  printf(#a " %#x -> %d\n", a, RtlNtStatusToDosError((a)))
            CONVERT(STATUS_SUCCESS);
            CONVERT(STATUS_NOT_SUPPORTED);
            CONVERT(STATUS_INVALID_PARAMETER);
            CONVERT(STATUS_UNKNOWN_REVISION);
            CONVERT(STATUS_INVALID_HANDLE);
            CONVERT(STATUS_INVALID_ADDRESS);
            CONVERT(STATUS_NOT_LOCKED);
            CONVERT(STATUS_IMAGE_ALREADY_LOADED);
            CONVERT(STATUS_ACCESS_DENIED);
            CONVERT(STATUS_REVISION_MISMATCH);

            return 0;
        }
     */

    switch (rc)
    {
        //case 0:                             return STATUS_SUCCESS;
        case 0:                             return VINF_SUCCESS;
        //case SUPDRV_ERR_GENERAL_FAILURE:    return STATUS_NOT_SUPPORTED;
        case ERROR_NOT_SUPPORTED:           return VERR_GENERAL_FAILURE;
        //case SUPDRV_ERR_INVALID_PARAM:      return STATUS_INVALID_PARAMETER;
        case ERROR_INVALID_PARAMETER:       return VERR_INVALID_PARAMETER;
        //case SUPDRV_ERR_INVALID_MAGIC:      return STATUS_ACCESS_DENIED;
        case ERROR_UNKNOWN_REVISION:        return VERR_INVALID_MAGIC;
        //case SUPDRV_ERR_INVALID_HANDLE:     return STATUS_INVALID_HANDLE;
        case ERROR_INVALID_HANDLE:          return VERR_INVALID_HANDLE;
        //case SUPDRV_ERR_INVALID_POINTER:    return STATUS_INVALID_ADDRESS;
        case ERROR_UNEXP_NET_ERR:           return VERR_INVALID_POINTER;
        //case SUPDRV_ERR_LOCK_FAILED:        return STATUS_NOT_LOCKED;
        case ERROR_NOT_LOCKED:              return VERR_LOCK_FAILED;
        //case SUPDRV_ERR_ALREADY_LOADED:     return STATUS_IMAGE_ALREADY_LOADED;
        case ERROR_SERVICE_ALREADY_RUNNING: return VERR_ALREADY_LOADED;
        //case SUPDRV_ERR_PERMISSION_DENIED:  return STATUS_ACCESS_DENIED;
        case ERROR_ACCESS_DENIED:           return VERR_PERMISSION_DENIED;
        //case SUPDRV_ERR_VERSION_MISMATCH:   return STATUS_REVISION_MISMATCH;
        case ERROR_REVISION_MISMATCH:       return VERR_VERSION_MISMATCH;
    }

    /* fall back on the default conversion. */
    return RTErrConvertFromWin32(rc);
}



