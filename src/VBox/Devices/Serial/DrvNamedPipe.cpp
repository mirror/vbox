/* $Id$ */
/** @file
 * Named pipe / local socket stream driver.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_NAMEDPIPE
#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
#else /* !RT_OS_WINDOWS */
# include <errno.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# ifndef SHUT_RDWR /* OS/2 */
#  define SHUT_RDWR 3
# endif
#endif /* !RT_OS_WINDOWS */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Converts a pointer to DRVNAMEDPIPE::IMedia to a PDRVNAMEDPIPE. */
#define PDMISTREAM_2_DRVNAMEDPIPE(pInterface) ( (PDRVNAMEDPIPE)((uintptr_t)pInterface - RT_OFFSETOF(DRVNAMEDPIPE, IStream)) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Named pipe driver instance data.
 *
 * @implements PDMISTREAM
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
    /** File handle of the named pipe. */
    HANDLE              NamedPipe;
    /** Overlapped structure for writes. */
    OVERLAPPED          OverlappedWrite;
    /** Overlapped structure for reads. */
    OVERLAPPED          OverlappedRead;
    /** Listen thread wakeup semaphore */
    RTSEMEVENTMULTI     ListenSem;
#else /* !RT_OS_WINDOWS */
    /** Socket handle of the local socket for server. */
    int                 LocalSocketServer;
    /** Socket handle of the local socket. */
    int                 LocalSocket;
#endif /* !RT_OS_WINDOWS */
    /** Thread for listening for new connections. */
    RTTHREAD            ListenThread;
    /** Flag to signal listening thread to shut down. */
    bool volatile       fShutdown;
} DRVNAMEDPIPE, *PDRVNAMEDPIPE;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/** @copydoc PDMISTREAM::pfnRead */
static DECLCALLBACK(int) drvNamedPipeRead(PPDMISTREAM pInterface, void *pvBuf, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    PDRVNAMEDPIPE pThis = PDMISTREAM_2_DRVNAMEDPIPE(pInterface);
    LogFlow(("%s: pvBuf=%p *pcbRead=%#x (%s)\n", __FUNCTION__, pvBuf, *pcbRead, pThis->pszLocation));

    Assert(pvBuf);
#ifdef RT_OS_WINDOWS
    if (pThis->NamedPipe != INVALID_HANDLE_VALUE)
    {
        DWORD cbReallyRead;
        pThis->OverlappedRead.Offset     = 0;
        pThis->OverlappedRead.OffsetHigh = 0;
        if (!ReadFile(pThis->NamedPipe, pvBuf, (DWORD)*pcbRead, &cbReallyRead, &pThis->OverlappedRead))
        {
            DWORD uError = GetLastError();

            if (uError == ERROR_IO_PENDING)
            {
                uError = 0;

                /* Wait for incoming bytes. */
                if (GetOverlappedResult(pThis->NamedPipe, &pThis->OverlappedRead, &cbReallyRead, TRUE) == FALSE)
                    uError = GetLastError();
            }

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
                rc = RTErrConvertFromWin32(uError);
                Log(("drvNamedPipeRead: ReadFile returned %d (%Rrc)\n", uError, rc));
            }
        }

        if (RT_FAILURE(rc))
        {
            Log(("drvNamedPipeRead: FileRead returned %Rrc fShutdown=%d\n", rc, pThis->fShutdown));
            if (    !pThis->fShutdown
                &&  (   rc == VERR_EOF
                     || rc == VERR_BROKEN_PIPE
                    )
               )
            {
                FlushFileBuffers(pThis->NamedPipe);
                DisconnectNamedPipe(pThis->NamedPipe);
                if (!pThis->fIsServer)
                {
                    CloseHandle(pThis->NamedPipe);
                    pThis->NamedPipe = INVALID_HANDLE_VALUE;
                }
                /* pretend success */
                rc = VINF_SUCCESS;
            }
            cbReallyRead = 0;
        }
        *pcbRead = (size_t)cbReallyRead;
    }
#else /* !RT_OS_WINDOWS */
    if (pThis->LocalSocket != -1)
    {
        ssize_t cbReallyRead;
        cbReallyRead = recv(pThis->LocalSocket, pvBuf, *pcbRead, 0);
        if (cbReallyRead == 0)
        {
            int tmp = pThis->LocalSocket;
            pThis->LocalSocket = -1;
            close(tmp);
        }
        else if (cbReallyRead == -1)
        {
            cbReallyRead = 0;
            rc = RTErrConvertFromErrno(errno);
        }
        *pcbRead = cbReallyRead;
    }
#endif /* !RT_OS_WINDOWS */
    else
    {
        RTThreadSleep(100);
        *pcbRead = 0;
    }

    LogFlow(("%s: *pcbRead=%zu returns %Rrc\n", __FUNCTION__, *pcbRead, rc));
    return rc;
}


/** @copydoc PDMISTREAM::pfnWrite */
static DECLCALLBACK(int) drvNamedPipeWrite(PPDMISTREAM pInterface, const void *pvBuf, size_t *pcbWrite)
{
    int rc = VINF_SUCCESS;
    PDRVNAMEDPIPE pThis = PDMISTREAM_2_DRVNAMEDPIPE(pInterface);
    LogFlow(("%s: pvBuf=%p *pcbWrite=%#x (%s)\n", __FUNCTION__, pvBuf, *pcbWrite, pThis->pszLocation));

    Assert(pvBuf);
#ifdef RT_OS_WINDOWS
    if (pThis->NamedPipe != INVALID_HANDLE_VALUE)
    {
        DWORD cbWritten = (DWORD)*pcbWrite;
        pThis->OverlappedWrite.Offset     = 0;
        pThis->OverlappedWrite.OffsetHigh = 0;
        if (!WriteFile(pThis->NamedPipe, pvBuf, cbWritten, NULL, &pThis->OverlappedWrite))
        {
            DWORD uError = GetLastError();

            if (   uError == ERROR_PIPE_LISTENING
                || uError == ERROR_PIPE_NOT_CONNECTED)
            {
                /* No connection yet/anymore; just discard the write (pretending everything was written). */;
            }
            else if (uError != ERROR_IO_PENDING)
            {
                rc = RTErrConvertFromWin32(uError);
                Log(("drvNamedPipeWrite: WriteFile returned %d (%Rrc)\n", uError, rc));
                cbWritten = 0;
            }
            else
            {
                /* Wait for the write to complete. */
                if (GetOverlappedResult(pThis->NamedPipe, &pThis->OverlappedWrite, &cbWritten, TRUE /*bWait*/) == FALSE)
                    rc = RTErrConvertFromWin32(uError = GetLastError());
            }
        }

        if (RT_FAILURE(rc))
        {
            /** @todo WriteFile(pipe) has been observed to return  ERROR_NO_DATA
             *        (VERR_NO_DATA) instead of ERROR_BROKEN_PIPE, when the pipe is
             *        disconnected. */
            if (    rc == VERR_EOF
                ||  rc == VERR_BROKEN_PIPE)
            {
                FlushFileBuffers(pThis->NamedPipe);
                DisconnectNamedPipe(pThis->NamedPipe);
                if (!pThis->fIsServer)
                {
                    CloseHandle(pThis->NamedPipe);
                    pThis->NamedPipe = INVALID_HANDLE_VALUE;
                }
                /* pretend success */
                rc = VINF_SUCCESS;
            }
            cbWritten = 0;
        }
        *pcbWrite = cbWritten;
    }
#else /* !RT_OS_WINDOWS */
    if (pThis->LocalSocket != -1)
    {
        ssize_t cbWritten;
        cbWritten = send(pThis->LocalSocket, pvBuf, *pcbWrite, 0);
        if (cbWritten == 0)
        {
            int tmp = pThis->LocalSocket;
            pThis->LocalSocket = -1;
            close(tmp);
        }
        else if (cbWritten == -1)
        {
            cbWritten = 0;
            rc = RTErrConvertFromErrno(errno);
        }
        *pcbWrite = cbWritten;
    }
#endif /* !RT_OS_WINDOWS */

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvNamedPipeQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNAMEDPIPE   pThis   = PDMINS_2_DATA(pDrvIns, PDRVNAMEDPIPE);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISTREAM, &pThis->IStream);
    return NULL;
}


/* -=-=-=-=- listen thread -=-=-=-=- */

/**
 * Receive thread loop.
 *
 * @returns 0 on success.
 * @param   hThreadSelf Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvNamedPipeListenLoop(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf);
    PDRVNAMEDPIPE   pThis = (PDRVNAMEDPIPE)pvUser;
    int             rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    HANDLE          NamedPipe = pThis->NamedPipe;
    HANDLE          hEvent = CreateEvent(NULL, TRUE, FALSE, 0);
#endif

    while (RT_LIKELY(!pThis->fShutdown))
    {
#ifdef RT_OS_WINDOWS
        OVERLAPPED overlapped;

        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = hEvent;

        BOOL fConnected = ConnectNamedPipe(NamedPipe, &overlapped);
        if (    !fConnected
            &&  !pThis->fShutdown)
        {
            DWORD hrc = GetLastError();

            if (hrc == ERROR_IO_PENDING)
            {
                DWORD dummy;

                hrc = 0;
                if (GetOverlappedResult(pThis->NamedPipe, &overlapped, &dummy, TRUE) == FALSE)
                    hrc = GetLastError();

            }

            if (pThis->fShutdown)
                break;

            if (hrc == ERROR_PIPE_CONNECTED)
            {
                RTSemEventMultiWait(pThis->ListenSem, 250);
            }
            else if (hrc != ERROR_SUCCESS)
            {
                rc = RTErrConvertFromWin32(hrc);
                LogRel(("NamedPipe%d: ConnectNamedPipe failed, rc=%Rrc\n", pThis->pDrvIns->iInstance, rc));
                break;
            }
        }
#else /* !RT_OS_WINDOWS */
        if (listen(pThis->LocalSocketServer, 0) == -1)
        {
            rc = RTErrConvertFromErrno(errno);
            LogRel(("NamedPipe%d: listen failed, rc=%Rrc\n", pThis->pDrvIns->iInstance, rc));
            break;
        }
        int s = accept(pThis->LocalSocketServer, NULL, NULL);
        if (s == -1)
        {
            rc = RTErrConvertFromErrno(errno);
            LogRel(("NamedPipe%d: accept failed, rc=%Rrc\n", pThis->pDrvIns->iInstance, rc));
            break;
        }
        if (pThis->LocalSocket != -1)
        {
            LogRel(("NamedPipe%d: only single connection supported\n", pThis->pDrvIns->iInstance));
            close(s);
        }
        else
            pThis->LocalSocket = s;

#endif /* !RT_OS_WINDOWS */
    }

#ifdef RT_OS_WINDOWS
    CloseHandle(hEvent);
#endif
    return VINF_SUCCESS;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/**
 * Common worker for drvNamedPipePowerOff and drvNamedPipeDestructor.
 *
 * @param   pThis               The instance data.
 */
static void drvNamedPipeShutdownListener(PDRVNAMEDPIPE pThis)
{
    /*
     * Signal shutdown of the listener thread.
     */
    pThis->fShutdown = true;
#ifdef RT_OS_WINDOWS
    if (    pThis->fIsServer
        &&  pThis->NamedPipe != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(pThis->NamedPipe);
        DisconnectNamedPipe(pThis->NamedPipe);

        BOOL fRc = CloseHandle(pThis->NamedPipe);
        Assert(fRc); NOREF(fRc);
        pThis->NamedPipe = INVALID_HANDLE_VALUE;

        /* Wake up listen thread */
        if (pThis->ListenSem != NIL_RTSEMEVENT)
            RTSemEventMultiSignal(pThis->ListenSem);
    }
#else
    if (    pThis->fIsServer
        &&  pThis->LocalSocketServer != -1)
    {
        int rc = shutdown(pThis->LocalSocketServer, SHUT_RDWR);
        AssertRC(rc == 0); NOREF(rc);

        rc = close(pThis->LocalSocketServer);
        AssertRC(rc == 0);
        pThis->LocalSocketServer = -1;
    }
#endif
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
    PDRVNAMEDPIPE pThis = PDMINS_2_DATA(pDrvIns, PDRVNAMEDPIPE);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));

    drvNamedPipeShutdownListener(pThis);
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
    PDRVNAMEDPIPE pThis = PDMINS_2_DATA(pDrvIns, PDRVNAMEDPIPE);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    drvNamedPipeShutdownListener(pThis);

    /*
     * While the thread exits, clean up as much as we can.
     */
#ifdef RT_OS_WINDOWS
    if (pThis->NamedPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(pThis->NamedPipe);
        pThis->NamedPipe = INVALID_HANDLE_VALUE;
    }
    if (pThis->OverlappedRead.hEvent != NULL)
    {
        CloseHandle(pThis->OverlappedRead.hEvent);
        pThis->OverlappedRead.hEvent = NULL;
    }
    if (pThis->OverlappedWrite.hEvent != NULL)
    {
        CloseHandle(pThis->OverlappedWrite.hEvent);
        pThis->OverlappedWrite.hEvent = NULL;
    }
#else /* !RT_OS_WINDOWS */
    Assert(pThis->LocalSocketServer == -1);
    if (pThis->LocalSocket != -1)
    {
        int rc = shutdown(pThis->LocalSocket, SHUT_RDWR);
        AssertRC(rc == 0); NOREF(rc);

        rc = close(pThis->LocalSocket);
        Assert(rc == 0);
        pThis->LocalSocket = -1;
    }
    if (   pThis->fIsServer
        && pThis->pszLocation)
        RTFileDelete(pThis->pszLocation);
#endif /* !RT_OS_WINDOWS */

    MMR3HeapFree(pThis->pszLocation);
    pThis->pszLocation = NULL;

    /*
     * Wait for the thread.
     */
    if (pThis->ListenThread != NIL_RTTHREAD)
    {
        int rc = RTThreadWait(pThis->ListenThread, 30000, NULL);
        if (RT_SUCCESS(rc))
            pThis->ListenThread = NIL_RTTHREAD;
        else
            LogRel(("NamedPipe%d: listen thread did not terminate (%Rrc)\n", pDrvIns->iInstance, rc));
    }

    /*
     * The last bits of cleanup.
     */
#ifdef RT_OS_WINDOWS
    if (pThis->ListenSem != NIL_RTSEMEVENT)
    {
        RTSemEventMultiDestroy(pThis->ListenSem);
        pThis->ListenSem = NIL_RTSEMEVENT;
    }
#endif
}


/**
 * Construct a named pipe stream driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvNamedPipeConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVNAMEDPIPE pThis = PDMINS_2_DATA(pDrvIns, PDRVNAMEDPIPE);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->pszLocation                  = NULL;
    pThis->fIsServer                    = false;
#ifdef RT_OS_WINDOWS
    pThis->NamedPipe                    = INVALID_HANDLE_VALUE;
    pThis->ListenSem                    = NIL_RTSEMEVENTMULTI;
    pThis->OverlappedWrite.hEvent       = NULL;
    pThis->OverlappedRead.hEvent        = NULL;
#else /* !RT_OS_WINDOWS */
    pThis->LocalSocketServer            = -1;
    pThis->LocalSocket                  = -1;
#endif /* !RT_OS_WINDOWS */
    pThis->ListenThread                 = NIL_RTTHREAD;
    pThis->fShutdown                    = false;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNamedPipeQueryInterface;
    /* IStream */
    pThis->IStream.pfnRead              = drvNamedPipeRead;
    pThis->IStream.pfnWrite             = drvNamedPipeWrite;

    /*
     * Validate and read the configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "Location|IsServer", "");

    int rc = CFGMR3QueryStringAlloc(pCfg, "Location", &pThis->pszLocation);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"Location\" resulted in %Rrc"), rc);
    rc = CFGMR3QueryBool(pCfg, "IsServer", &pThis->fIsServer);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"IsServer\" resulted in %Rrc"), rc);

    /*
     * Create/Open the pipe.
     */
#ifdef RT_OS_WINDOWS
    if (pThis->fIsServer)
    {
        pThis->NamedPipe = CreateNamedPipe(pThis->pszLocation,
                                           PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                           1,        /*nMaxInstances*/
                                           32,       /*nOutBufferSize*/
                                           32,       /*nOutBufferSize*/
                                           10000,    /*nDefaultTimeOut*/
                                           NULL);    /* lpSecurityAttributes*/
        if (pThis->NamedPipe == INVALID_HANDLE_VALUE)
        {
            rc = RTErrConvertFromWin32(GetLastError());
            LogRel(("NamedPipe%d: CreateNamedPipe failed rc=%Rrc\n", pThis->pDrvIns->iInstance));
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NamedPipe#%d failed to create named pipe %s"),
                                       pDrvIns->iInstance, pThis->pszLocation);
        }

        rc = RTSemEventMultiCreate(&pThis->ListenSem);
        AssertRCReturn(rc, rc);

        rc = RTThreadCreate(&pThis->ListenThread, drvNamedPipeListenLoop, (void *)pThis, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SerPipe");
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, rc,  RT_SRC_POS, N_("NamedPipe#%d failed to create listening thread"),
                                       pDrvIns->iInstance);

    }
    else
    {
        /* Connect to the named pipe. */
        pThis->NamedPipe = CreateFile(pThis->pszLocation, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                      OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (pThis->NamedPipe == INVALID_HANDLE_VALUE)
        {
            rc = RTErrConvertFromWin32(GetLastError());
            LogRel(("NamedPipe%d: CreateFile failed rc=%Rrc\n", pThis->pDrvIns->iInstance));
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NamedPipe#%d failed to connect to named pipe %s"),
                                       pDrvIns->iInstance, pThis->pszLocation);
        }
    }

    memset(&pThis->OverlappedWrite, 0, sizeof(pThis->OverlappedWrite));
    pThis->OverlappedWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    AssertReturn(pThis->OverlappedWrite.hEvent != NULL, VERR_OUT_OF_RESOURCES);

    memset(&pThis->OverlappedRead, 0, sizeof(pThis->OverlappedRead));
    pThis->OverlappedRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    AssertReturn(pThis->OverlappedRead.hEvent != NULL, VERR_OUT_OF_RESOURCES);

#else /* !RT_OS_WINDOWS */
    int s = socket(PF_UNIX, SOCK_STREAM, 0);
    if (s == -1)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                   N_("NamedPipe#%d failed to create local socket"), pDrvIns->iInstance);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, pThis->pszLocation, sizeof(addr.sun_path) - 1);

    if (pThis->fIsServer)
    {
        /* Bind address to the local socket. */
        pThis->LocalSocketServer = s;
        RTFileDelete(pThis->pszLocation);
        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                       N_("NamedPipe#%d failed to bind to local socket %s"),
                                       pDrvIns->iInstance, pThis->pszLocation);
        rc = RTThreadCreate(&pThis->ListenThread, drvNamedPipeListenLoop, (void *)pThis, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SerPipe");
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, rc,  RT_SRC_POS,
                                       N_("NamedPipe#%d failed to create listening thread"), pDrvIns->iInstance);
    }
    else
    {
        /* Connect to the local socket. */
        pThis->LocalSocket = s;
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                       N_("NamedPipe#%d failed to connect to local socket %s"),
                                       pDrvIns->iInstance, pThis->pszLocation);
    }
#endif /* !RT_OS_WINDOWS */

    LogRel(("NamedPipe: location %s, %s\n", pThis->pszLocation, pThis->fIsServer ? "server" : "client"));
    return VINF_SUCCESS;
}


/**
 * Named pipe driver registration record.
 */
const PDMDRVREG g_DrvNamedPipe =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "NamedPipe",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Named Pipe stream driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVNAMEDPIPE),
    /* pfnConstruct */
    drvNamedPipeConstruct,
    /* pfnDestruct */
    drvNamedPipeDestruct,
    /* pfnRelocate */
    NULL,
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
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvNamedPipePowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

