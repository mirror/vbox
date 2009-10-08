/* $Id$ */
/** @file
 * VBox Console COM Class implementation, The Live Migration Part.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "ConsoleImpl.h"
#include "Logging.h"

#include <iprt/err.h>
#include <iprt/rand.h>
#include <iprt/tcp.h>
#include <iprt/timer.h>

#include <VBox/vmapi.h>
#include <VBox/ssm.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/com/string.h>



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Argument package for Console::migrationServeConnection.
 */
typedef struct MIGRATIONSTATE
{
    Console    *pConsole;
    IMachine   *pMachine;
    PVM         pVM;
    const char *pszPassword;
    void       *pvVMCallbackTask;
    RTSOCKET    hSocket;
    uint64_t    offStream;
    int         rc;
} MIGRATIONSTATE;
typedef MIGRATIONSTATE *PMIGRATIONSTATE;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const char g_szWelcome[] = "VirtualBox-LiveMigration-1.0\n";


/**
 * @copydoc SSMSTRMOPS::pfnWrite
 */
static DECLCALLBACK(int) migrationTcpOpWrite(void *pvUser, uint64_t offStream, const void *pvBuf, size_t cbToWrite)
{
    PMIGRATIONSTATE pState = (PMIGRATIONSTATE)pvUser;
    int rc = RTTcpWrite(pState->hSocket, pvBuf, cbToWrite);
    if (RT_SUCCESS(rc))
    {
        pState->offStream += cbToWrite;
        return VINF_SUCCESS;
    }
    return rc;
}


/**
 * @copydoc SSMSTRMOPS::pfnRead
 */
static DECLCALLBACK(int) migrationTcpOpRead(void *pvUser, uint64_t offStream, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PMIGRATIONSTATE pState = (PMIGRATIONSTATE)pvUser;
    int rc = RTTcpRead(pState->hSocket, pvBuf, cbToRead, pcbRead);
    if (RT_SUCCESS(rc))
    {
        pState->offStream += pcbRead ? *pcbRead : cbToRead;
        return VINF_SUCCESS;
    }
    return rc;
}


/**
 * @copydoc SSMSTRMOPS::pfnSeek
 */
static DECLCALLBACK(int) migrationTcpOpSeek(void *pvUser, int64_t offSeek, unsigned uMethod, uint64_t *poffActual)
{
    return VERR_NOT_SUPPORTED;
}


/**
 * @copydoc SSMSTRMOPS::pfnTell
 */
static DECLCALLBACK(uint64_t) migrationTcpOpTell(void *pvUser)
{
    PMIGRATIONSTATE pState = (PMIGRATIONSTATE)pvUser;
    return pState->offStream;
}


/**
 * @copydoc SSMSTRMOPS::pfnSize
 */
static DECLCALLBACK(int) migrationTcpOpSize(void *pvUser, uint64_t *pcb)
{
    return VERR_NOT_SUPPORTED;
}


/**
 * @copydoc SSMSTRMOPS::pfnClose
 */
static DECLCALLBACK(int) migrationTcpOpClose(void *pvUser)
{
    return VINF_SUCCESS;
}


/**
 * Method table for a TCP based stream.
 */
static SSMSTRMOPS const g_migrationTcpOps =
{
    SSMSTRMOPS_VERSION,
    migrationTcpOpWrite,
    migrationTcpOpRead,
    migrationTcpOpSeek,
    migrationTcpOpTell,
    migrationTcpOpSize,
    migrationTcpOpClose,
    SSMSTRMOPS_VERSION
};


/**
 * @copydoc FNRTTIMERLR
 */
static DECLCALLBACK(void) migrationTimeout(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick)
{
    /* This is harmless for any open connections. */
    RTTcpServerShutdown((PRTTCPSERVER)pvUser);
}


/**
 * Start live migration to the specified target.
 *
 * @returns COM status code.
 *
 * @param   aHostname   The name of the target host.
 * @param   aPort       The TCP port number.
 * @param   aPassword   The password.
 * @param   aProgress   Where to return the progress object.
 */
STDMETHODIMP
Console::Migrate(IN_BSTR aHostname, ULONG aPort, IN_BSTR aPassword, IProgress **aProgress)
{
    /*
     * Validate parameters.
     */

    /*
     * Try change the state, create a progress object and spawn a worker thread.
     */

    return E_FAIL;
}


/**
 * Creates a TCP server that listens for the source machine and passes control
 * over to Console::migrationServeConnection().
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle
 * @param   pMachine            The IMachine for the virtual machine.
 * @param   pvVMCallbackTask    The callback task pointer for
 *                              stateProgressCallback().
 */
int
Console::migrationLoadRemote(PVM pVM, IMachine *pMachine, void *pvVMCallbackTask)
{
    /*
     * Get the config.
     */
    ULONG uPort;
    HRESULT hrc = pMachine->COMGETTER(LiveMigrationPort)(&uPort);
    if (FAILED(hrc))
        return VERR_GENERAL_FAILURE;

    Bstr bstrPassword;
    hrc = pMachine->COMGETTER(LiveMigrationPassword)(bstrPassword.asOutParam());
    if (FAILED(hrc))
        return VERR_GENERAL_FAILURE;
    Utf8Str strPassword(bstrPassword);
    strPassword.append('\n');           /* always ends with a newline. */

    Utf8Str strBind("");
    /** @todo Add a bind address property. */
    const char *pszBindAddress = strBind.isEmpty() ? NULL : strBind.c_str();

    /*
     * Create the TCP server.
     */
    int rc;
    PRTTCPSERVER hServer;
    if (uPort)
        rc = RTTcpServerCreateEx(pszBindAddress, uPort, &hServer);
    else
    {
        for (int cTries = 10240; cTries > 0; cTries--)
        {
            uPort = RTRandU32Ex(cTries >= 8192 ? 49152 : 1024, 65534);
            rc = RTTcpServerCreateEx(pszBindAddress, uPort, &hServer);
            if (rc != VERR_NET_ADDRESS_IN_USE)
                break;
        }
        if (RT_SUCCESS(rc))
        {
            HRESULT hrc = pMachine->COMSETTER(LiveMigrationPort)(uPort);
            if (FAILED(hrc))
            {
                RTTcpServerDestroy(hServer);
                return VERR_GENERAL_FAILURE;
            }
        }
    }
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create a one-shot timer for timing out after 5 mins.
     */
    RTTIMERLR hTimerLR;
    rc = RTTimerLRCreateEx(&hTimerLR, 0 /*ns*/, RTTIMER_FLAGS_CPU_ANY, migrationTimeout, hServer);
    if (RT_SUCCESS(rc))
    {
        rc = RTTimerLRStart(hTimerLR, 5*60*UINT64_C(1000000000) /*ns*/);
        if (RT_SUCCESS(rc))
        {
            /*
             * Do the job, when it returns we're done.
             */
            MIGRATIONSTATE State;
            State.pConsole      = this;
            State.pMachine      = pMachine;
            State.pVM           = pVM;
            State.pszPassword   = strPassword.c_str();
            State.hSocket       = NIL_RTSOCKET;
            State.offStream     = UINT64_MAX / 2;
            State.rc            = VINF_SUCCESS;

            rc = RTTcpServerListen(hServer, Console::migrationServeConnection, &State);
            if (rc == VERR_TCP_SERVER_STOP)
                rc = State.rc;
            if (RT_FAILURE(rc))
                LogRel(("Migration: RTTcpServerListen -> %Rrc\n", rc));
        }

        RTTimerLRDestroy(hTimerLR);
    }
    RTTcpServerDestroy(hServer);

    return rc;
}


/**
 * Reads a string from the socket.
 *
 * @returns VBox status code.
 *
 * @param   Sock        The socket.
 * @param   pszBuf      The output buffer.
 * @param   cchBuf      The size of the output buffer.
 *
 */
static int migrationReadLine(RTSOCKET Sock, char *pszBuf, size_t cchBuf)
{
    char *pszStart = pszBuf;
    AssertReturn(cchBuf > 1, VERR_INTERNAL_ERROR);

    /* dead simple (stupid) approach. */
    for (;;)
    {
        char ch;
        int rc = RTTcpRead(Sock, &ch, sizeof(ch), NULL);
        if (RT_FAILURE(rc))
        {
            LogRel(("Migration: RTTcpRead -> %Rrc while reading string ('%s')\n", rc, pszStart));
            return rc;
        }
        if (    ch == '\n'
            ||  ch == '\0')
            return VINF_SUCCESS;
        if (cchBuf <= 1)
        {
            LogRel(("Migration: String buffer overflow: '%s'\n", pszStart));
            return VERR_BUFFER_OVERFLOW;
        }
        *pszBuf++ = ch;
        *pszBuf   = '\0';
        cchBuf--;
    }
}


/**
 * @copydoc FNRTTCPSERVE
 * VERR_TCP_SERVER_STOP
 */
/*static*/ DECLCALLBACK(int)
Console::migrationServeConnection(RTSOCKET Sock, void *pvUser)
{
    PMIGRATIONSTATE pState   = (PMIGRATIONSTATE)pvUser;
    Console        *pConsole = pState->pConsole;
    IMachine       *pMachine = pState->pMachine;

    /*
     * Say hello.
     */
    int rc = RTTcpWrite(Sock, g_szWelcome, sizeof(g_szWelcome) - 1);
    if (RT_FAILURE(rc))
    {
        LogRel(("Migration: Failed to write welcome message: %Rrc\n", rc));
        return VINF_SUCCESS;
    }

    /*
     * Password (includes '\n', see migrationLoadRemote).  If it's right, tell
     * the TCP server to stop listening (frees up host resources and makes sure
     * this is the last connection attempt).
     */
    const char *pszPassword = pState->pszPassword;
    unsigned    off = 0;
    while (pszPassword[off])
    {
        char ch;
        rc = RTTcpRead(Sock, &ch, sizeof(ch), NULL);
        if (RT_FAILURE(rc))
            break;
        if (pszPassword[off] != ch)
        {
            LogRel(("Migration: Invalid password (off=%u)\n", off));
            return VINF_SUCCESS;
        }
        off++;
    }

    RTTcpServerShutdown((PRTTCPSERVER)pvUser);

    /*
     * Command processing loop.
     */
    pState->hSocket = Sock;
    for (;;)
    {
        char szCmd[128];
        rc = migrationReadLine(Sock, szCmd, sizeof(szCmd));
        if (RT_FAILURE(rc))
            break;

        if (!strcmp(szCmd, "load"))
        {
            pState->offStream = 0;
            rc = VMR3LoadFromStream(pState->pVM, &g_migrationTcpOps, pState,
                                    Console::stateProgressCallback, pState->pvVMCallbackTask);
            if (RT_FAILURE(rc))
            {
                LogRel(("Migration: VMR3LoadFromStream -> %Rrc\n", rc));
                break;
            }
        }
        /** @todo implement config verification and hardware compatability checks. Or
         *        maybe leave part of these to the saved state machinery? */
        else if (!strcmp(szCmd, "done"))
            break;
        else
        {
            LogRel(("Migration: Unknown command '%s'\n", szCmd));
            break;
        }
    }
    pState->hSocket = NIL_RTSOCKET;

    return VERR_TCP_SERVER_STOP;
}

