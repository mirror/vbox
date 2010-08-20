/* $Id$ */
/** @file
 * FTM - Fault Tolerance Manager
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_FTM
#include "FTMInternal.h"
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/ssm.h>
#include <VBox/log.h>
#include <VBox/pgm.h>

#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/tcp.h>
#include <iprt/semaphore.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const char g_szWelcome[] = "VirtualBox-Fault-Tolerance-Sync-1.0\n";

/**
 * Initializes the FTM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) FTMR3Init(PVM pVM)
{
    /*
     * Assert alignment and sizes.
     */
    AssertCompile(sizeof(pVM->ftm.s) <= sizeof(pVM->ftm.padding));
    AssertCompileMemberAlignment(FTM, CritSect, sizeof(uintptr_t));

    /** @todo saved state for master nodes! */
    pVM->ftm.s.pszAddress               = NULL;
    pVM->ftm.s.pszPassword              = NULL;
    pVM->fFaultTolerantMaster           = false;
    pVM->ftm.s.fIsStandbyNode           = false;
    pVM->ftm.s.standby.hServer          = NIL_RTTCPSERVER;
    pVM->ftm.s.master.hShutdownEvent    = NIL_RTSEMEVENT;
    pVM->ftm.s.hSocket                  = NIL_RTSOCKET;

    /*
     * Initialize the PGM critical section.
     */
    int rc = PDMR3CritSectInit(pVM, &pVM->ftm.s.CritSect, RT_SRC_POS, "FTM");
    AssertRCReturn(rc, rc);

    STAM_REL_REG(pVM, &pVM->ftm.s.StatReceivedMem,               STAMTYPE_COUNTER, "/FT/Received/Mem",                   STAMUNIT_BYTES, "The amount of memory pages that was received.");
    STAM_REL_REG(pVM, &pVM->ftm.s.StatReceivedState,             STAMTYPE_COUNTER, "/FT/Received/State",                 STAMUNIT_BYTES, "The amount of state information that was received.");
    STAM_REL_REG(pVM, &pVM->ftm.s.StatSentMem,                   STAMTYPE_COUNTER, "/FT/Sent/Mem",                       STAMUNIT_BYTES, "The amount of memory pages that was sent.");
    STAM_REL_REG(pVM, &pVM->ftm.s.StatSentState,                 STAMTYPE_COUNTER, "/FT/Sent/State",                     STAMUNIT_BYTES, "The amount of state information that was sent.");

    return VINF_SUCCESS;
}

/**
 * Terminates the FTM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM itself is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) FTMR3Term(PVM pVM)
{
    if (pVM->ftm.s.pszAddress)
        RTMemFree(pVM->ftm.s.pszAddress);
    if (pVM->ftm.s.pszPassword)
        RTMemFree(pVM->ftm.s.pszPassword);
    if (pVM->ftm.s.hSocket != NIL_RTSOCKET)
        RTTcpClientClose(pVM->ftm.s.hSocket);
    if (pVM->ftm.s.standby.hServer)
        RTTcpServerDestroy(pVM->ftm.s.standby.hServer);
    if (pVM->ftm.s.master.hShutdownEvent != NIL_RTSEMEVENT)
        RTSemEventDestroy(pVM->ftm.s.master.hShutdownEvent);

    PDMR3CritSectDelete(&pVM->ftm.s.CritSect);
    return VINF_SUCCESS;
}


static int ftmR3TcpWriteACK(PVM pVM)
{
    int rc = RTTcpWrite(pVM->ftm.s.hSocket, "ACK\n", sizeof("ACK\n") - 1);
    if (RT_FAILURE(rc))
    {
        LogRel(("FTSync: RTTcpWrite(,ACK,) -> %Rrc\n", rc));
    }
    return rc;
}


static int ftmR3TcpWriteNACK(PVM pVM, int32_t rc2, const char *pszMsgText = NULL)
{
    char    szMsg[256];
    size_t  cch;
    if (pszMsgText && *pszMsgText)
    {
        cch = RTStrPrintf(szMsg, sizeof(szMsg), "NACK=%d;%s\n", rc2, pszMsgText);
        for (size_t off = 6; off + 1 < cch; off++)
            if (szMsg[off] == '\n')
                szMsg[off] = '\r';
    }
    else
        cch = RTStrPrintf(szMsg, sizeof(szMsg), "NACK=%d\n", rc2);
    int rc = RTTcpWrite(pVM->ftm.s.hSocket, szMsg, cch);
    if (RT_FAILURE(rc))
        LogRel(("FTSync: RTTcpWrite(,%s,%zu) -> %Rrc\n", szMsg, cch, rc));
    return rc;
}

/**
 * Reads a string from the socket.
 *
 * @returns VBox status code.
 *
 * @param   pState      The teleporter state structure.
 * @param   pszBuf      The output buffer.
 * @param   cchBuf      The size of the output buffer.
 *
 */
static int ftmR3TcpReadLine(PVM pVM, char *pszBuf, size_t cchBuf)
{
    char       *pszStart = pszBuf;
    RTSOCKET    Sock     = pVM->ftm.s.hSocket;

    AssertReturn(cchBuf > 1, VERR_INTERNAL_ERROR);
    *pszBuf = '\0';

    /* dead simple approach. */
    for (;;)
    {
        char ch;
        int rc = RTTcpRead(Sock, &ch, sizeof(ch), NULL);
        if (RT_FAILURE(rc))
        {
            LogRel(("FTSync: RTTcpRead -> %Rrc while reading string ('%s')\n", rc, pszStart));
            return rc;
        }
        if (    ch == '\n'
            ||  ch == '\0')
            return VINF_SUCCESS;
        if (cchBuf <= 1)
        {
            LogRel(("FTSync: String buffer overflow: '%s'\n", pszStart));
            return VERR_BUFFER_OVERFLOW;
        }
        *pszBuf++ = ch;
        *pszBuf = '\0';
        cchBuf--;
    }
}

/**
 * Reads an ACK or NACK.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM to operate on.
 * @param   pszWhich            Which ACK is this this?
 * @param   pszNAckMsg          Optional NACK message.
 */
static int ftmR3TcpReadACK(PVM pVM, const char *pszWhich, const char *pszNAckMsg /*= NULL*/)
{
    char szMsg[256];
    int rc = ftmR3TcpReadLine(pVM, szMsg, sizeof(szMsg));
    if (RT_FAILURE(rc))
        return rc;

    if (!strcmp(szMsg, "ACK"))
        return VINF_SUCCESS;

    if (!strncmp(szMsg, "NACK=", sizeof("NACK=") - 1))
    {
        char *pszMsgText = strchr(szMsg, ';');
        if (pszMsgText)
            *pszMsgText++ = '\0';

        int32_t vrc2;
        rc = RTStrToInt32Full(&szMsg[sizeof("NACK=") - 1], 10, &vrc2);
        if (rc == VINF_SUCCESS)
        {
            /*
             * Well formed NACK, transform it into an error.
             */
            if (pszNAckMsg)
            {
                LogRel(("FTSync: %s: NACK=%Rrc (%d)\n", pszWhich, vrc2, vrc2));
                return VERR_INTERNAL_ERROR;
            }

            if (pszMsgText)
            {
                pszMsgText = RTStrStrip(pszMsgText);
                for (size_t off = 0; pszMsgText[off]; off++)
                    if (pszMsgText[off] == '\r')
                        pszMsgText[off] = '\n';

                LogRel(("FTSync: %s: NACK=%Rrc (%d) - '%s'\n", pszWhich, vrc2, vrc2, pszMsgText));
            }
            return VERR_INTERNAL_ERROR_2;
        }

        if (pszMsgText)
            pszMsgText[-1] = ';';
    }
    return VERR_INTERNAL_ERROR_3;
}

/**
 * Thread function which starts syncing process for this master VM
 *
 * @param   Thread      The thread id.
 * @param   pvUser      Not used
 * @return  VINF_SUCCESS (ignored).
 *
 * @note Locks the Console object for writing.
 */
static DECLCALLBACK(int) ftmR3MasterThread(RTTHREAD Thread, void *pvUser)
{
    int rc  = VINF_SUCCESS;
    PVM pVM = (PVM)pvUser;

    for (;;)
    {
        /*
         * Try connect to the standby machine.
         */
        rc = RTTcpClientConnect(pVM->ftm.s.pszAddress, pVM->ftm.s.uPort, &pVM->ftm.s.hSocket);
        if (RT_SUCCESS(rc))
        {
            /* Disable Nagle. */
            rc = RTTcpSetSendCoalescing(pVM->ftm.s.hSocket, false /*fEnable*/);
            AssertRC(rc);

            /* Read and check the welcome message. */
            char szLine[RT_MAX(128, sizeof(g_szWelcome))];
            RT_ZERO(szLine);
            rc = RTTcpRead(pVM->ftm.s.hSocket, szLine, sizeof(g_szWelcome) - 1, NULL);
            if (    RT_SUCCESS(rc)
                &&  !strcmp(szLine, g_szWelcome))
            {
                /* password */
                rc = RTTcpWrite(pVM->ftm.s.hSocket, pVM->ftm.s.pszPassword, strlen(pVM->ftm.s.pszPassword));
                if (RT_SUCCESS(rc))
                {
                    /* ACK */
                    rc = ftmR3TcpReadACK(pVM, "password", "Invalid password");
                    if (RT_SUCCESS(rc))
                    {
                        /** todo: verify VM config. */
                        break;
                    }
                }
            }
            rc = RTTcpClientClose(pVM->ftm.s.hSocket);
            AssertRC(rc);
            pVM->ftm.s.hSocket = NIL_RTSOCKET;
        }
        rc = RTSemEventWait(pVM->ftm.s.master.hShutdownEvent, 1000 /* 1 second */);
        if (rc != VERR_TIMEOUT)
            return VINF_SUCCESS;    /* told to quit */            
    }

    /* Successfully initialized the connection to the standby node.
     * Start the sync process.
     */

    for (;;)
    {
        if (!pVM->ftm.s.fCheckpointingActive)
        {
            rc = PDMCritSectEnter(&pVM->ftm.s.CritSect, VERR_SEM_BUSY);
            AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc));

            /* sync the changed memory with the standby node. */

            PDMCritSectLeave(&pVM->ftm.s.CritSect);
        }
        rc = RTSemEventWait(pVM->ftm.s.master.hShutdownEvent, pVM->ftm.s.uInterval);
        if (rc != VERR_TIMEOUT)
            break;    /* told to quit */            
    }
    return rc;
}

/**
 * Listen for incoming traffic destined for the standby VM.
 *
 * @copydoc FNRTTCPSERVE
 *
 * @returns VINF_SUCCESS or VERR_TCP_SERVER_STOP.
 */
static DECLCALLBACK(int) ftmR3StandbyServeConnection(RTSOCKET Sock, void *pvUser)
{
    PVM pVM = (PVM)pvUser;

    pVM->ftm.s.hSocket = Sock;

    /*
     * Disable Nagle.
     */
    int rc = RTTcpSetSendCoalescing(Sock, false /*fEnable*/);
    AssertRC(rc);

    /* Send the welcome message to the master node. */
    rc = RTTcpWrite(Sock, g_szWelcome, sizeof(g_szWelcome) - 1);
    if (RT_FAILURE(rc))
    {
        LogRel(("Teleporter: Failed to write welcome message: %Rrc\n", rc));
        return VINF_SUCCESS;
    }

    /*
     * Password.
     */
    const char *pszPassword = pVM->ftm.s.pszPassword;
    unsigned    off = 0;
    while (pszPassword[off])
    {
        char ch;
        rc = RTTcpRead(Sock, &ch, sizeof(ch), NULL);
        if (    RT_FAILURE(rc)
            ||  pszPassword[off] != ch)
        {
            if (RT_FAILURE(rc))
                LogRel(("FTSync: Password read failure (off=%u): %Rrc\n", off, rc));
            else
                LogRel(("FTSync: Invalid password (off=%u)\n", off));
            ftmR3TcpWriteNACK(pVM, VERR_AUTHENTICATION_FAILURE);
            return VINF_SUCCESS;
        }
        off++;
    }
    rc = ftmR3TcpWriteACK(pVM);
    if (RT_FAILURE(rc))
        return VINF_SUCCESS;

    /** todo: verify VM config. */

    /*
     * Stop the server.
     *
     * Note! After this point we must return VERR_TCP_SERVER_STOP, while prior
     *       to it we must not return that value!
     */
    RTTcpServerShutdown(pVM->ftm.s.standby.hServer);

    /*
     * Command processing loop.
     */
    bool fDone = false;
    for (;;)
    {
        char szCmd[128];
        rc = ftmR3TcpReadLine(pVM, szCmd, sizeof(szCmd));
        if (RT_FAILURE(rc))
            break;

        if (!strcmp(szCmd, "mem-sync"))
        {
        }
        else
        if (!strcmp(szCmd, "heartbeat"))
        {
        }
        else
        if (!strcmp(szCmd, "checkpoint"))
        {
        }
        if (RT_FAILURE(rc))
            break;
    }
    LogFlowFunc(("returns mRc=%Rrc\n", rc));
    return VERR_TCP_SERVER_STOP;
}

/**
 * Powers on the fault tolerant virtual machine.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to operate on.
 * @param   fMaster     FT master or standby
 * @param   uInterval   FT sync interval
 * @param   pszAddress  Standby VM address
 * @param   uPort       Standby VM port
 * @param   pszPassword FT password (NULL for none)
 *
 * @thread      Any thread.
 * @vmstate     Created
 * @vmstateto   PoweringOn+Running (master), PoweringOn+Running_FT (standby)
 */
VMMR3DECL(int) FTMR3PowerOn(PVM pVM, bool fMaster, unsigned uInterval, const char *pszAddress, unsigned uPort, const char *pszPassword)
{
    int rc = VINF_SUCCESS;

    VMSTATE enmVMState = VMR3GetState(pVM);
    AssertMsgReturn(enmVMState == VMSTATE_POWERING_ON,
                    ("%s\n", VMR3GetStateName(enmVMState)),
                    VERR_INTERNAL_ERROR_4);
    AssertReturn(pszAddress, VERR_INVALID_PARAMETER);

    if (pVM->ftm.s.uInterval)
        pVM->ftm.s.uInterval    = uInterval;
    else
        pVM->ftm.s.uInterval    = 50;   /* standard sync interval of 50ms */

    pVM->ftm.s.uPort            = uPort;
    pVM->ftm.s.pszAddress       = RTStrDup(pszAddress);
    if (pszPassword)
        pVM->ftm.s.pszPassword  = RTStrDup(pszPassword);
    if (fMaster)
    {
        rc = RTSemEventCreate(&pVM->ftm.s.master.hShutdownEvent);
        if (RT_FAILURE(rc))
            return rc;

        rc = RTThreadCreate(NULL, ftmR3MasterThread, pVM,
                            0, RTTHREADTYPE_IO /* higher than normal priority */, 0, "ftmR3MasterThread");
        if (RT_FAILURE(rc))
            return rc;

        pVM->fFaultTolerantMaster = true;
        if (PGMIsUsingLargePages(pVM))
        {
            /* Must disable large page usage as 2 MB pages are too big to write monitor. */
            LogRel(("FTSync: disabling large page usage.\n"));
            PGMSetLargePageUsage(pVM, false);
        }
        /** @todo might need to disable page fusion as well */

        return VMR3PowerOn(pVM);
    }
    else
    {
        /* standby */
        rc = RTTcpServerCreateEx(pszAddress, uPort, &pVM->ftm.s.standby.hServer);
        if (RT_FAILURE(rc))
            return rc;
        pVM->ftm.s.fIsStandbyNode = true;

        rc = RTTcpServerListen(pVM->ftm.s.standby.hServer, ftmR3StandbyServeConnection, pVM);
        /** @todo deal with the exit code to check if we should activate this standby VM. */

        RTTcpServerDestroy(pVM->ftm.s.standby.hServer);
        pVM->ftm.s.standby.hServer = NULL;
    }
    return rc;
}

/**
 * Powers off the fault tolerant virtual machine (standby).
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) FTMR3CancelStandby(PVM pVM)
{
    AssertReturn(!pVM->fFaultTolerantMaster, VERR_NOT_SUPPORTED);
    Assert(pVM->ftm.s.standby.hServer);

    return RTTcpServerShutdown(pVM->ftm.s.standby.hServer);
}


/**
 * Performs a full sync to the standby node
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) FTMR3SyncState(PVM pVM)
{
    if (!pVM->fFaultTolerantMaster)
        return VINF_SUCCESS;

    pVM->ftm.s.fCheckpointingActive = true;
    int rc = PDMCritSectEnter(&pVM->ftm.s.CritSect, VERR_SEM_BUSY);
    AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc));

    PDMCritSectLeave(&pVM->ftm.s.CritSect);
    pVM->ftm.s.fCheckpointingActive = false;

    return VERR_NOT_IMPLEMENTED;
}
