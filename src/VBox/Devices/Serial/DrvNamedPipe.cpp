/** @file
 *
 * VBox stream devices:
 * Named pipe stream
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_NAMEDPIPE
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>

#include "Builtins.h"

#ifdef RT_OS_WINDOWS
#include <windows.h>
#else /* !RT_OS_WINDOWS */
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif /* !RT_OS_WINDOWS */

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Converts a pointer to DRVNAMEDPIPE::IMedia to a PDRVNAMEDPIPE. */
#define PDMISTREAM_2_DRVNAMEDPIPE(pInterface) ( (PDRVNAMEDPIPE)((uintptr_t)pInterface - RT_OFFSETOF(DRVNAMEDPIPE, IStream)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Named pipe driver instance data.
 */
typedef struct DRVNAMEDPIPE
{
    /** The stream interface. */
    PDMISTREAM          IStream;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to the named pipe file name. (Freed by MM) */
    char                *pszLocation;
    /** Flag whether VirtualBox represents the server or client side. */
    bool                fIsServer;
#ifdef RT_OS_WINDOWS
    /* File handle of the named pipe. */
    HANDLE              NamedPipe;
    /* Overlapped structure for writes. */
    OVERLAPPED          OverlappedWrite;
    /* Overlapped structure for reads. */
    OVERLAPPED          OverlappedRead;
    /* Listen thread wakeup semaphore */
    RTSEMEVENT          ListenSem;
#else /* !RT_OS_WINDOWS */
    /** Socket handle of the local socket for server. */
    RTSOCKET            LocalSocketServer;
    /** Socket handle of the local socket. */
    RTSOCKET            LocalSocket;
#endif /* !RT_OS_WINDOWS */
    /** Thread for listening for new connections. */
    RTTHREAD            ListenThread;
    /** Flag to signal listening thread to shut down. */
    bool                fShutdown;
} DRVNAMEDPIPE, *PDRVNAMEDPIPE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


/** @copydoc PDMISTREAM::pfnRead */
static DECLCALLBACK(int) drvNamedPipeRead(PPDMISTREAM pInterface, void *pvBuf, size_t *cbRead)
{
    int rc = VINF_SUCCESS;
    PDRVNAMEDPIPE pData = PDMISTREAM_2_DRVNAMEDPIPE(pInterface);
    LogFlow(("%s: pvBuf=%p cbRead=%#x (%s)\n", __FUNCTION__, pvBuf, cbRead, pData->pszLocation));

    Assert(pvBuf);
#ifdef RT_OS_WINDOWS
    if (pData->NamedPipe != INVALID_HANDLE_VALUE)
    {
        DWORD cbReallyRead;
        pData->OverlappedRead.Offset     = 0;
        pData->OverlappedRead.OffsetHigh = 0;
        if (!ReadFile(pData->NamedPipe, pvBuf, *cbRead, &cbReallyRead, &pData->OverlappedRead))
        {
            DWORD uError = GetLastError();

            if (   uError == ERROR_PIPE_LISTENING
                || uError == ERROR_PIPE_NOT_CONNECTED)
            {
                /* No connection yet/anymore */
                cbReallyRead = 0;

                /* wait a bit or else we'll be called right back. */
                RTThreadSleep(100);
            }
            else
            {
                if (uError == ERROR_IO_PENDING)
                {
                    uError = 0;

                    /* Wait for incoming bytes. */
                    if (GetOverlappedResult(pData->NamedPipe, &pData->OverlappedRead, &cbReallyRead, TRUE) == FALSE)
                        uError = GetLastError();
                }

                rc = RTErrConvertFromWin32(uError);
                Log(("drvNamedPipeRead: ReadFile returned %d (%Vrc)\n", uError, rc));
            }
        }

        if (VBOX_FAILURE(rc))
        {
            Log(("drvNamedPipeRead: FileRead returned %Vrc fShutdown=%d\n", rc, pData->fShutdown));
            if (    !pData->fShutdown
                &&  (   rc == VERR_EOF
                     || rc == VERR_BROKEN_PIPE
                    )
               )

            {
                FlushFileBuffers(pData->NamedPipe);
                DisconnectNamedPipe(pData->NamedPipe);
                if (!pData->fIsServer)
                {
                    CloseHandle(pData->NamedPipe);
                    pData->NamedPipe = INVALID_HANDLE_VALUE;
                }
                /* pretend success */
                rc = VINF_SUCCESS;
            }
            cbReallyRead = 0;
        }
        *cbRead = (size_t)cbReallyRead;
    }
#else /* !RT_OS_WINDOWS */
    if (pData->LocalSocket != NIL_RTSOCKET)
    {
        ssize_t cbReallyRead;
        cbReallyRead = recv(pData->LocalSocket, pvBuf, *cbRead, 0);
        if (cbReallyRead == 0)
        {
            RTSOCKET tmp = pData->LocalSocket;
            pData->LocalSocket = NIL_RTSOCKET;
            close(tmp);
        }
        else if (cbReallyRead == -1)
        {
            cbReallyRead = 0;
            rc = RTErrConvertFromErrno(errno);
        }
        *cbRead = cbReallyRead;
    }
#endif /* !RT_OS_WINDOWS */
    else
    {
        RTThreadSleep(100);
        *cbRead = 0;
    }

    LogFlow(("%s: cbRead=%d returns %Vrc\n", __FUNCTION__, *cbRead, rc));
    return rc;
}


/** @copydoc PDMISTREAM::pfnWrite */
static DECLCALLBACK(int) drvNamedPipeWrite(PPDMISTREAM pInterface, const void *pvBuf, size_t *cbWrite)
{
    int rc = VINF_SUCCESS;
    PDRVNAMEDPIPE pData = PDMISTREAM_2_DRVNAMEDPIPE(pInterface);
    LogFlow(("%s: pvBuf=%p cbWrite=%#x (%s)\n", __FUNCTION__, pvBuf, cbWrite, pData->pszLocation));

    Assert(pvBuf);
#ifdef RT_OS_WINDOWS
    if (pData->NamedPipe != INVALID_HANDLE_VALUE)
    {
        unsigned cbWritten;
        pData->OverlappedWrite.Offset     = 0;
        pData->OverlappedWrite.OffsetHigh = 0;
        if (!WriteFile(pData->NamedPipe, pvBuf, *cbWrite, NULL, &pData->OverlappedWrite))
        {
            DWORD uError = GetLastError();

            if (   uError == ERROR_PIPE_LISTENING
                || uError == ERROR_PIPE_NOT_CONNECTED)
            {
                /* No connection yet/anymore; just discard the write. */
                cbWritten = *cbWrite;
            }
            else
            if (uError != ERROR_IO_PENDING)
            {
                rc = RTErrConvertFromWin32(uError);
                Log(("drvNamedPipeWrite: WriteFile returned %d (%Vrc)\n", uError, rc));
            }
            else
            {
                /* Wait for the write to complete. */
                if (GetOverlappedResult(pData->NamedPipe, &pData->OverlappedWrite, (DWORD *)&cbWritten, TRUE) == FALSE)
                    uError = GetLastError();
            }
        }
        else
            cbWritten = *cbWrite;

        if (VBOX_FAILURE(rc))
        {
            if (    rc == VERR_EOF
                ||  rc == VERR_BROKEN_PIPE)
            {
                FlushFileBuffers(pData->NamedPipe);
                DisconnectNamedPipe(pData->NamedPipe);
                if (!pData->fIsServer)
                {
                    CloseHandle(pData->NamedPipe);
                    pData->NamedPipe = INVALID_HANDLE_VALUE;
                }
                /* pretend success */
                rc = VINF_SUCCESS;
            }
            cbWritten = 0;
        }
        *cbWrite = cbWritten;
    }
#else /* !RT_OS_WINDOWS */
    if (pData->LocalSocket != NIL_RTSOCKET)
    {
        ssize_t cbWritten;
        cbWritten = send(pData->LocalSocket, pvBuf, *cbWrite, 0);
        if (cbWritten == 0)
        {
            RTSOCKET tmp = pData->LocalSocket;
            pData->LocalSocket = NIL_RTSOCKET;
            close(tmp);
        }
        else if (cbWritten == -1)
        {
            cbWritten = 0;
            rc = RTErrConvertFromErrno(errno);
        }
        *cbWrite = cbWritten;
    }
#endif /* !RT_OS_WINDOWS */

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvNamedPipeQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PDRVNAMEDPIPE pDrv = PDMINS2DATA(pDrvIns, PDRVNAMEDPIPE);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_STREAM:
            return &pDrv->IStream;
        default:
            return NULL;
    }
}


/* -=-=-=-=- listen thread -=-=-=-=- */

/**
 * Receive thread loop.
 *
 * @returns 0 on success.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvNamedPipeListenLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVNAMEDPIPE   pData = (PDRVNAMEDPIPE)pvUser;
    int             rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    HANDLE          NamedPipe = pData->NamedPipe;
    HANDLE          hEvent = CreateEvent(NULL, TRUE, FALSE, 0);
#endif

    while (RT_LIKELY(!pData->fShutdown))
    {
#ifdef RT_OS_WINDOWS
        OVERLAPPED overlapped;

        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = hEvent;

        BOOL fConnected = ConnectNamedPipe(NamedPipe, &overlapped);
        if (    !fConnected
            &&  !pData->fShutdown)
        {
            DWORD hrc = GetLastError();

            if (hrc == ERROR_IO_PENDING)
            {
                DWORD dummy;

                hrc = 0;
                if (GetOverlappedResult(pData->NamedPipe, &overlapped, &dummy, TRUE) == FALSE)
                    hrc = GetLastError();

            }

            if (pData->fShutdown)
                break;

            if (hrc == ERROR_PIPE_CONNECTED)
            {
                RTSemEventWait(pData->ListenSem, 250);
            }
            else
            if (hrc != ERROR_SUCCESS)
            {
                rc = RTErrConvertFromWin32(hrc);
                LogRel(("NamedPipe%d: ConnectNamedPipe failed, rc=%Vrc\n", pData->pDrvIns->iInstance, rc));
                break;
            }
        }
#else /* !RT_OS_WINDOWS */
        if (listen(pData->LocalSocketServer, 0) == -1)
        {
            rc = RTErrConvertFromErrno(errno);
            LogRel(("NamedPipe%d: listen failed, rc=%Vrc\n", pData->pDrvIns->iInstance, rc));
            break;
        }
        int s = accept(pData->LocalSocketServer, NULL, NULL);
        if (s == -1)
        {
            rc = RTErrConvertFromErrno(errno);
            LogRel(("NamedPipe%d: accept failed, rc=%Vrc\n", pData->pDrvIns->iInstance, rc));
            break;
        }
        else
        {
            if (pData->LocalSocket != NIL_RTSOCKET)
            {
                LogRel(("NamedPipe%d: only single connection supported\n", pData->pDrvIns->iInstance));
                close(s);
            }
            else
                pData->LocalSocket = s;
        }
#endif /* !RT_OS_WINDOWS */
    }

#ifdef RT_OS_WINDOWS
    CloseHandle(hEvent);
#endif
    pData->ListenThread = NIL_RTTHREAD;
    return VINF_SUCCESS;
}


/**
 * Construct a named pipe stream driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvNamedPipeConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    int rc;
    PDRVNAMEDPIPE pData = PDMINS2DATA(pDrvIns, PDRVNAMEDPIPE);

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->pszLocation                  = NULL;
    pData->fIsServer                    = false;
#ifdef RT_OS_WINDOWS
    pData->NamedPipe                    = INVALID_HANDLE_VALUE;
#else /* !RT_OS_WINDOWS */
    pData->LocalSocketServer            = NIL_RTSOCKET;
    pData->LocalSocket                  = NIL_RTSOCKET;
#endif /* !RT_OS_WINDOWS */
    pData->ListenThread                 = NIL_RTTHREAD;
    pData->fShutdown                    = false;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNamedPipeQueryInterface;
    /* IStream */
    pData->IStream.pfnRead              = drvNamedPipeRead;
    pData->IStream.pfnWrite             = drvNamedPipeWrite;

    /*
     * Read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Location\0IsServer\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    char *pszLocation;
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Location", &pszLocation);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query \"Location\" resulted in %Vrc.\n", rc));
        return rc;
    }
    pData->pszLocation = pszLocation;

    bool fIsServer;
    rc = CFGMR3QueryBool(pCfgHandle, "IsServer", &fIsServer);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query \"IsServer\" resulted in %Vrc.\n", rc));
        goto out;
    }
    pData->fIsServer = fIsServer;

#ifdef RT_OS_WINDOWS
    if (fIsServer)
    {
        HANDLE hPipe = CreateNamedPipe(pData->pszLocation, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 32, 32, 10000, NULL);
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            rc = RTErrConvertFromWin32(GetLastError());
            LogRel(("NamedPipe%d: CreateNamedPipe failed rc=%Vrc\n", pData->pDrvIns->iInstance));
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NamedPipe#%d failed to create named pipe %s"), pDrvIns->iInstance, pszLocation);
        }
        pData->NamedPipe = hPipe;

        rc = RTThreadCreate(&pData->ListenThread, drvNamedPipeListenLoop, (void *)pData, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "NamedPipe");
        if VBOX_FAILURE(rc)
            return PDMDrvHlpVMSetError(pDrvIns, rc,  RT_SRC_POS, N_("NamedPipe#%d failed to create listening thread"), pDrvIns->iInstance);

        rc = RTSemEventCreate(&pData->ListenSem);
        AssertRC(rc);
    }
    else
    {
        /* Connect to the named pipe. */
        HANDLE hPipe = CreateFile(pData->pszLocation, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            rc = RTErrConvertFromWin32(GetLastError());
            LogRel(("NamedPipe%d: CreateFile failed rc=%Vrc\n", pData->pDrvIns->iInstance));
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NamedPipe#%d failed to connect to named pipe %s"), pDrvIns->iInstance, pszLocation);
        }
        pData->NamedPipe = hPipe;
    }

    memset(&pData->OverlappedWrite, 0, sizeof(pData->OverlappedWrite));
    memset(&pData->OverlappedRead, 0, sizeof(pData->OverlappedRead));
    pData->OverlappedWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    pData->OverlappedRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
#else /* !RT_OS_WINDOWS */
    int s;
    struct sockaddr_un addr;

    if ((s = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS, N_("NamedPipe#%d failed to create local socket"), pDrvIns->iInstance);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, pszLocation, sizeof(addr.sun_path)-1);

    if (fIsServer)
    {
        /* Bind address to the local socket. */
        RTFileDelete(pszLocation);
        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS, N_("NamedPipe#%d failed to bind to local socket %s"), pDrvIns->iInstance, pszLocation);
        rc = RTThreadCreate(&pData->ListenThread, drvNamedPipeListenLoop, (void *)pData, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "NamedPipe");
        if VBOX_FAILURE(rc)
            return PDMDrvHlpVMSetError(pDrvIns, rc,  RT_SRC_POS, N_("NamedPipe#%d failed to create listening thread"), pDrvIns->iInstance);
        pData->LocalSocketServer = s;
    }
    else
    {
        /* Connect to the local socket. */
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS, N_("NamedPipe#%d failed to connect to local socket %s"), pDrvIns->iInstance, pszLocation);
        pData->LocalSocket = s;
    }
#endif /* !RT_OS_WINDOWS */

out:
    if (VBOX_FAILURE(rc))
    {
        if (pszLocation)
            MMR3HeapFree(pszLocation);
    }
    else
    {
        LogFlow(("drvNamedPipeConstruct: location %s isServer %d\n", pszLocation, fIsServer));
        LogRel(("NamedPipe: location %s, %s\n", pszLocation, fIsServer ? "server" : "client"));
    }
    return rc;
}


/**
 * Destruct a named pipe stream driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvNamedPipeDestruct(PPDMDRVINS pDrvIns)
{
    PDRVNAMEDPIPE pData = PDMINS2DATA(pDrvIns, PDRVNAMEDPIPE);
    LogFlow(("%s: %s\n", __FUNCTION__, pData->pszLocation));

    if (pData->ListenThread)
    {
        RTThreadWait(pData->ListenThread, 250, NULL);
        if (pData->ListenThread != NIL_RTTHREAD)
            LogRel(("NamedPipe%d: listen thread did not terminate\n", pDrvIns->iInstance));
    }

    if (pData->pszLocation)
        MMR3HeapFree(pData->pszLocation);
}


/**
 * Power off a named pipe stream driver instance.
 *
 * This does most of the destruction work, to avoid ordering dependencies.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvNamedPipePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVNAMEDPIPE pData = PDMINS2DATA(pDrvIns, PDRVNAMEDPIPE);
    LogFlow(("%s: %s\n", __FUNCTION__, pData->pszLocation));

    pData->fShutdown = true;

#ifdef RT_OS_WINDOWS
    if (pData->NamedPipe != INVALID_HANDLE_VALUE)
    {
        if (pData->fIsServer) 
        {
            FlushFileBuffers(pData->NamedPipe);
            DisconnectNamedPipe(pData->NamedPipe);
        }

        CloseHandle(pData->NamedPipe);
        pData->NamedPipe = INVALID_HANDLE_VALUE;
        CloseHandle(pData->OverlappedRead.hEvent);
        CloseHandle(pData->OverlappedWrite.hEvent);
    }
    if (pData->fIsServer)
    {
        /* Wake up listen thread */
        RTSemEventSignal(pData->ListenSem);
        RTSemEventDestroy(pData->ListenSem);
    }
#else /* !RT_OS_WINDOWS */
    if (pData->fIsServer)
    {
        if (pData->LocalSocketServer != NIL_RTSOCKET)
            close(pData->LocalSocketServer);
        if (pData->pszLocation)
            RTFileDelete(pData->pszLocation);
    }
    else
    {
        if (pData->LocalSocket != NIL_RTSOCKET)
            close(pData->LocalSocket);
    }
#endif /* !RT_OS_WINDOWS */
}


/**
 * Named pipe driver registration record.
 */
const PDMDRVREG g_DrvNamedPipe =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "NamedPipe",
    /* pszDescription */
    "Named Pipe stream driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVNAMEDPIPE),
    /* pfnConstruct */
    drvNamedPipeConstruct,
    /* pfnDestruct */
    drvNamedPipeDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvNamedPipePowerOff,
};
