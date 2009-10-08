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
typedef struct MIGRATIONSERVEARGS
{
    Console    *pConsole;
    IMachine   *pMachine;
    PVM         pVM;
    const char *pszPassword;
    RTTIMERLR   hTimer;
} MIGRATIONSERVEARGS;
typedef MIGRATIONSERVEARGS *PMIGRATIONSERVEARGS;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const char g_szWelcome[] = "VirtualBox-LiveMigration-1.0\n";



/**
 * @copydoc FNRTTIMERLR
 */
static DECLCALLBACK(void) migrationTimeout(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick)
{
    /* This is harmless for any open connections. */
    RTTcpServerShutdown((PRTTCPSERVER)pvUser);
}


STDMETHODIMP
Console::Migrate(IN_BSTR aHostname, ULONG aPort, IN_BSTR aPassword, IProgress **aProgress)
{
    return E_FAIL;
}


/**
 * Creates a TCP server that listens for the source machine and passes control
 * over to Console::migrationServeConnection().
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle
 * @param   pMachine            The IMachine for the virtual machine.
 */
int
Console::migrationLoadRemote(PVM pVM, IMachine *pMachine)
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
     * Create a timer for timing out after 5 mins.
     */
    RTTIMERLR hTimer;
    rc = RTTimerLRCreateEx(&hTimer, 0, 0, migrationTimeout, hServer);
    if (RT_SUCCESS(rc))
    {
        rc = RTTimerLRStart(hTimer, 5*60*UINT64_C(1000000000) /*ns*/);
        if (RT_SUCCESS(rc))
        {
            /*
             * Do the job, when it returns we're done.
             */
            MIGRATIONSERVEARGS Args;
            Args.pConsole    = this;
            Args.pMachine    = pMachine;
            Args.pVM         = pVM;
            Args.pszPassword = strPassword.c_str();
            Args.hTimer      = hTimer;
            rc = RTTcpServerListen(hServer, Console::migrationServeConnection, &Args);
        }

        RTTimerLRDestroy(hTimer);
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
    PMIGRATIONSERVEARGS pArgs       = (PMIGRATIONSERVEARGS)pvUser;
    Console            *pConsole    = pArgs->pConsole;
    IMachine           *pMachine    = pArgs->pMachine;
    PVM                 pVM         = pArgs->pVM;
    const char         *pszPassword = pArgs->pszPassword;

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
    unsigned off = 0;
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
    for (;;)
    {
        char szCmd[128];
        rc = migrationReadLine(Sock, szCmd, sizeof(szCmd));
        if (RT_FAILURE(rc))
            break;

        if (!strcmp(szCmd, "state"))
        {
            /* restore the state. */
        }
        else if (!strcmp(szCmd, "done"))
            break;
        else
        {
            LogRel(("Migration: Unknown command '%s'\n", szCmd));
            break;
        }
    }

    return VERR_TCP_SERVER_STOP;
}

