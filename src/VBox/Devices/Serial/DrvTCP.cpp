/* $Id$ */
/** @file
 * TCP socket driver implementing the IStream interface.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation.
 *
 * This file was contributed by Alexey Eromenko (derived from DrvNamedPipe)
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
#define LOG_GROUP LOG_GROUP_DRV_TCP
#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <stdlib.h>

#include "VBoxDD.h"

#ifdef RT_OS_WINDOWS
# include <iprt/win/ws2tcpip.h>
#else /* !RT_OS_WINDOWS */
# include <errno.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# ifndef SHUT_RDWR /* OS/2 */
#  define SHUT_RDWR 3
# endif
#endif /* !RT_OS_WINDOWS */

#ifndef SHUT_RDWR
# ifdef SD_BOTH
#  define SHUT_RDWR SD_BOTH
# else
#  define SHUT_RDWR 2
# endif
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Converts a pointer to DRVTCP::IMedia to a PDRVTCP. */
#define PDMISTREAM_2_DRVTCP(pInterface) ( (PDRVTCP)((uintptr_t)pInterface - RT_OFFSETOF(DRVTCP, IStream)) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * TCP driver instance data.
 *
 * @implements PDMISTREAM
 */
typedef struct DRVTCP
{
    /** The stream interface. */
    PDMISTREAM          IStream;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to the TCP server address:port or port only. (Freed by MM) */
    char                *pszLocation;
    /** Flag whether VirtualBox represents the server or client side. */
    bool                fIsServer;

    /** Socket handle of the TCP socket for server. */
    int                 TCPServer;
    /** Socket handle of the TCP socket connection. */
    int                 TCPConnection;

    /** Thread for listening for new connections. */
    RTTHREAD            ListenThread;
    /** Flag to signal listening thread to shut down. */
    bool volatile       fShutdown;
} DRVTCP, *PDRVTCP;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/** @interface_method_impl{PDMISTREAM,pfnRead} */
static DECLCALLBACK(int) drvTCPRead(PPDMISTREAM pInterface, void *pvBuf, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    PDRVTCP pThis = PDMISTREAM_2_DRVTCP(pInterface);
    LogFlow(("%s: pvBuf=%p *pcbRead=%#x (%s)\n", __FUNCTION__, pvBuf, *pcbRead, pThis->pszLocation));

    Assert(pvBuf);

    if (pThis->TCPConnection != -1)
    {
        ssize_t cbReallyRead;
        unsigned cbBuf = (unsigned)*pcbRead;
        cbReallyRead = recv(pThis->TCPConnection, (char *)pvBuf, cbBuf, 0);
        if (cbReallyRead == 0)
        {
            int tmp = pThis->TCPConnection;
            pThis->TCPConnection = -1;
#ifdef RT_OS_WINDOWS
            closesocket(tmp);
#else
            close(tmp);
#endif
        }
        else if (cbReallyRead == -1)
        {
            cbReallyRead = 0;
            rc = RTErrConvertFromErrno(errno);
        }
        *pcbRead = cbReallyRead;
    }
    else
    {
        RTThreadSleep(100);
        *pcbRead = 0;
    }

    LogFlow(("%s: *pcbRead=%zu returns %Rrc\n", __FUNCTION__, *pcbRead, rc));
    return rc;
}


/** @interface_method_impl{PDMISTREAM,pfnWrite} */
static DECLCALLBACK(int) drvTCPWrite(PPDMISTREAM pInterface, const void *pvBuf, size_t *pcbWrite)
{
    int rc = VINF_SUCCESS;
    PDRVTCP pThis = PDMISTREAM_2_DRVTCP(pInterface);
    LogFlow(("%s: pvBuf=%p *pcbWrite=%#x (%s)\n", __FUNCTION__, pvBuf, *pcbWrite, pThis->pszLocation));

    Assert(pvBuf);
    if (pThis->TCPConnection != -1)
    {
        ssize_t cbWritten;
        unsigned cbBuf = (unsigned)*pcbWrite;
        cbWritten = send(pThis->TCPConnection, (const char *)pvBuf, cbBuf, 0);
        if (cbWritten == 0)
        {
            int tmp = pThis->TCPConnection;
            pThis->TCPConnection = -1;
#ifdef RT_OS_WINDOWS
            closesocket(tmp);
#else
            close(tmp);
#endif
        }
        else if (cbWritten == -1)
        {
            cbWritten = 0;
            rc = RTErrConvertFromErrno(errno);
        }
        *pcbWrite = cbWritten;
    }

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvTCPQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTCP         pThis   = PDMINS_2_DATA(pDrvIns, PDRVTCP);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISTREAM, &pThis->IStream);
    return NULL;
}


/* -=-=-=-=- listen thread -=-=-=-=- */

/**
 * Receive thread loop.
 *
 * @returns VINF_SUCCESS
 * @param   hThreadSelf Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvTCPListenLoop(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf);
    PDRVTCP pThis = (PDRVTCP)pvUser;
    int     rc;

    while (RT_LIKELY(!pThis->fShutdown))
    {
        if (listen(pThis->TCPServer, 0) == -1)
        {
            rc = RTErrConvertFromErrno(errno);
            LogRel(("DrvTCP%d: listen failed, rc=%Rrc\n", pThis->pDrvIns->iInstance, rc));
            break;
        }
        int s = accept(pThis->TCPServer, NULL, NULL);
        if (s == -1)
        {
            rc = RTErrConvertFromErrno(errno);
            LogRel(("DrvTCP%d: accept failed, rc=%Rrc\n", pThis->pDrvIns->iInstance, rc));
            break;
        }
        if (pThis->TCPConnection != -1)
        {
            LogRel(("DrvTCP%d: only single connection supported\n", pThis->pDrvIns->iInstance));
#ifdef RT_OS_WINDOWS
            closesocket(s);
#else
            close(s);
#endif
        }
        else
            pThis->TCPConnection = s;
    }

    return VINF_SUCCESS;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/**
 * Common worker for drvTCPPowerOff and drvTCPDestructor.
 *
 * @param   pThis               The instance data.
 */
static void drvTCPShutdownListener(PDRVTCP pThis)
{
    /*
     * Signal shutdown of the listener thread.
     */
    pThis->fShutdown = true;
    if (    pThis->fIsServer
        &&  pThis->TCPServer != -1)
    {
        int rc = shutdown(pThis->TCPServer, SHUT_RDWR);
        AssertRC(rc == 0); NOREF(rc);

#ifdef RT_OS_WINDOWS
        rc = closesocket(pThis->TCPServer);
#else
        rc = close(pThis->TCPServer);
#endif
        AssertRC(rc == 0);
        pThis->TCPServer = -1;
    }
}


/**
 * Power off a TCP socket stream driver instance.
 *
 * This does most of the destruction work, to avoid ordering dependencies.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvTCPPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVTCP pThis = PDMINS_2_DATA(pDrvIns, PDRVTCP);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));

    drvTCPShutdownListener(pThis);
}


/**
 * Destruct a TCP socket stream driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvTCPDestruct(PPDMDRVINS pDrvIns)
{
    PDRVTCP pThis = PDMINS_2_DATA(pDrvIns, PDRVTCP);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    drvTCPShutdownListener(pThis);

    /*
     * While the thread exits, clean up as much as we can.
     */

    Assert(pThis->TCPServer == -1);
    if (pThis->TCPConnection != -1)
    {
        int rc = shutdown(pThis->TCPConnection, SHUT_RDWR);
        AssertRC(rc == 0); NOREF(rc);

#ifdef RT_OS_WINDOWS
        rc = closesocket(pThis->TCPConnection);
#else
        rc = close(pThis->TCPConnection);
#endif
        Assert(rc == 0);
        pThis->TCPConnection = -1;
    }
    if (   pThis->fIsServer
        && pThis->pszLocation)
        RTFileDelete(pThis->pszLocation);


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
            LogRel(("DrvTCP%d: listen thread did not terminate (%Rrc)\n", pDrvIns->iInstance, rc));
    }

}


/**
 * Construct a TCP socket stream driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvTCPConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVTCP pThis = PDMINS_2_DATA(pDrvIns, PDRVTCP);

#ifdef RT_OS_WINDOWS
    {
        WSADATA wsaData;
        int err;

        err = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (err != 0)
        {
            LogRel(("DrvTCP: Failed to initialize Winsock, error %d\n", err));
            /* XXX: let socket creation fail below */
        }
    }
#endif

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->pszLocation                  = NULL;
    pThis->fIsServer                    = false;

    pThis->TCPServer                    = -1;
    pThis->TCPConnection                = -1;

    pThis->ListenThread                 = NIL_RTTHREAD;
    pThis->fShutdown                    = false;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvTCPQueryInterface;
    /* IStream */
    pThis->IStream.pfnRead              = drvTCPRead;
    pThis->IStream.pfnWrite             = drvTCPWrite;

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
     * Create/Open the socket.
     */
    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (s == -1)
        return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                   N_("DrvTCP#%d failed to create socket"), pDrvIns->iInstance);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    if (pThis->fIsServer)
    {
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(/*PORT*/ atoi(pThis->pszLocation));

        /* Bind address to the telnet socket. */
        pThis->TCPServer = s;
        RTFileDelete(pThis->pszLocation);
        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                       N_("DrvTCP#%d failed to bind to socket %s"),
                                       pDrvIns->iInstance, pThis->pszLocation);
        rc = RTThreadCreate(&pThis->ListenThread, drvTCPListenLoop, (void *)pThis, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "DrvTCPStream");
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, rc,  RT_SRC_POS,
                                       N_("DrvTCP#%d failed to create listening thread"), pDrvIns->iInstance);
    }
    else
    {
        char *token;
        const char *delim = ":";
        struct hostent *server;
        token = strtok(pThis->pszLocation, delim);
        if(token) {
            server = gethostbyname(token);
            memmove((char *)&addr.sin_addr.s_addr,
                    (char *)server->h_addr,
                     server->h_length);
        }
        token = strtok(NULL, delim);
        if(token) {
            addr.sin_port = htons(/*PORT*/ atoi(token));
        }

        /* Connect to the telnet socket. */
        pThis->TCPConnection = s;
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            return PDMDrvHlpVMSetError(pDrvIns, RTErrConvertFromErrno(errno), RT_SRC_POS,
                                       N_("DrvTCP#%d failed to connect to socket %s"),
                                       pDrvIns->iInstance, pThis->pszLocation);
    }

    LogRel(("DrvTCP: %s, %s\n", pThis->pszLocation, pThis->fIsServer ? "server" : "client"));
    return VINF_SUCCESS;
}


/**
 * TCP stream driver registration record.
 */
const PDMDRVREG g_DrvTCP =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "TCP",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "TCP serial stream driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVTCP),
    /* pfnConstruct */
    drvTCPConstruct,
    /* pfnDestruct */
    drvTCPDestruct,
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
    drvTCPPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

