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
#include "Global.h"
#include "Logging.h"
#include "ProgressImpl.h"

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
 * Base class for the migration state.
 *
 * These classes are used as advanced structs, not as proper classes.
 */
class MigrationState
{
public:
    ComPtr<Console>     mptrConsole;
    PVM                 mpVM;
    Utf8Str             mstrPassword;
    bool const          mfIsSource;

    /** @name stream stuff
     * @{  */
    RTSOCKET            mhSocket;
    uint64_t            moffStream;
    uint32_t            mcbReadBlock;
    bool volatile       mfStopReading;
    bool volatile       mfEndOfStream;
    bool volatile       mfIOError;
    /** @} */

    MigrationState(Console *pConsole, PVM pVM, bool fIsSource)
        : mptrConsole(pConsole)
        , mpVM(pVM)
        , mfIsSource(fIsSource)
        , mhSocket(NIL_RTSOCKET)
        , moffStream(UINT64_MAX / 2)
        , mcbReadBlock(0)
        , mfStopReading(false)
        , mfEndOfStream(false)
        , mfIOError(false)
    {
    }
};


/**
 * Migration state used by the source side.
 */
class MigrationStateSrc : public MigrationState
{
public:
    ComPtr<Progress>    mptrProgress;
    Utf8Str             mstrHostname;
    uint32_t            muPort;

    MigrationStateSrc(Console *pConsole, PVM pVM)
        : MigrationState(pConsole, pVM, true /*fIsSource*/)
        , muPort(UINT32_MAX)
    {
    }
};


/**
 * Migration state used by the destiation side.
 */
class MigrationStateDst : public MigrationState
{
public:
    IMachine           *mpMachine;
    void               *mpvVMCallbackTask;
    PRTTCPSERVER        mhServer;
    PRTTIMERLR          mphTimerLR;
    int                 mRc;

    MigrationStateDst(Console *pConsole, PVM pVM, IMachine *pMachine, PRTTIMERLR phTimerLR)
        : MigrationState(pConsole, pVM, false /*fIsSource*/)
        , mpMachine(pMachine)
        , mpvVMCallbackTask(NULL)
        , mhServer(NULL)
        , mphTimerLR(phTimerLR)
        , mRc(VINF_SUCCESS)
    {
    }
};


/**
 * TCP stream header.
 *
 * This is an extra layer for fixing the problem with figuring out when the SSM
 * stream ends.
 */
typedef struct MIGRATIONTCPHDR
{
    /** Magic value. */
    uint32_t    u32Magic;
    /** The size of the data block following this header.
     * 0 indicates the end of the stream. */
    uint32_t    cb;
} MIGRATIONTCPHDR;
/** Magic value for MIGRATIONTCPHDR::u32Magic. (Egberto Gismonti Amin) */
#define MIGRATIONTCPHDR_MAGIC       UINT32_C(0x19471205)
/** The max block size. */
#define MIGRATIONTCPHDR_MAX_SIZE    UINT32_C(0x00fffff8)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const char g_szWelcome[] = "VirtualBox-LiveMigration-1.0\n";


/**
 * Reads a string from the socket.
 *
 * @returns VBox status code.
 *
 * @param   pState      The live migration state structure.
 * @param   pszBuf      The output buffer.
 * @param   cchBuf      The size of the output buffer.
 *
 */
static int migrationTcpReadLine(MigrationState *pState, char *pszBuf, size_t cchBuf)
{
    char       *pszStart = pszBuf;
    RTSOCKET    Sock     = pState->mhSocket;

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
 * Reads an ACK or NACK.
 *
 * @returns S_OK on ACK, E_FAIL+setError() on failure or NACK.
 * @param   pState              The live migration source state.
 * @param   pszNAckMsg          Optional NACK message.
 *
 * @remarks the setError laziness forces this to be a Console member.
 */
HRESULT
Console::migrationSrcReadACK(MigrationStateSrc *pState, const char *pszNAckMsg /*= NULL*/)
{
    char szMsg[128];
    int vrc = migrationTcpReadLine(pState, szMsg, sizeof(szMsg));
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed reading ACK: %Rrc"), vrc);
    if (strcmp(szMsg, "ACK"))
    {
        if (!strncmp(szMsg, "NACK=", sizeof("NACK=") - 1))
        {
            int32_t vrc2;
            vrc = RTStrToInt32Full(&szMsg[sizeof("NACK=") - 1], 10, &vrc2);
            if (vrc == VINF_SUCCESS)
            {
                if (pszNAckMsg)
                {
                    LogRel(("Migration: NACK=%Rrc (%d)\n", vrc2, vrc2));
                    return setError(E_FAIL, pszNAckMsg);
                }
                return setError(E_FAIL, "NACK - %Rrc (%d)", vrc2, vrc2);
            }
        }
        return setError(E_FAIL, tr("Expected ACK or NACK, got '%s'"), szMsg);
    }
    return S_OK;
}


/**
 * Submitts a command to the destination and waits for the ACK.
 *
 * @returns S_OK on ACKed command, E_FAIL+setError() on failure.
 *
 * @param   pState              The live migration source state.
 * @param   pszCommand          The command.
 *
 * @remarks the setError laziness forces this to be a Console member.
 */
HRESULT
Console::migrationSrcSubmitCommand(MigrationStateSrc *pState, const char *pszCommand)
{
    size_t cchCommand = strlen(pszCommand);
    int vrc = RTTcpWrite(pState->mhSocket, pszCommand, cchCommand);
    if (RT_SUCCESS(vrc))
        vrc = RTTcpWrite(pState->mhSocket, "\n", sizeof("\n") - 1);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed writing command '%s': %Rrc"), pszCommand, vrc);
    return migrationSrcReadACK(pState);
}


/**
 * @copydoc SSMSTRMOPS::pfnWrite
 */
static DECLCALLBACK(int) migrationTcpOpWrite(void *pvUser, uint64_t offStream, const void *pvBuf, size_t cbToWrite)
{
    MigrationState *pState = (MigrationState *)pvUser;

    AssertReturn(cbToWrite > 0, VINF_SUCCESS);
    AssertReturn(pState->mfIsSource, VERR_INVALID_HANDLE);

    for (;;)
    {
        /* Write block header. */
        MIGRATIONTCPHDR Hdr;
        Hdr.u32Magic = MIGRATIONTCPHDR_MAGIC;
        Hdr.cb       = RT_MIN(cbToWrite, MIGRATIONTCPHDR_MAX_SIZE);
        int rc = RTTcpWrite(pState->mhSocket, &Hdr, sizeof(Hdr));
        if (RT_FAILURE(rc))
        {
            LogRel(("Migration/TCP: Header write error: %Rrc\n", rc));
            return rc;
        }

        /* Write the data. */
        rc = RTTcpWrite(pState->mhSocket, pvBuf, Hdr.cb);
        if (RT_FAILURE(rc))
        {
            LogRel(("Migration/TCP: Data write error: %Rrc (cb=%#x)\n", rc, Hdr.cb));
            return rc;
        }
        pState->moffStream += Hdr.cb;
        if (Hdr.cb == cbToWrite)
            return VINF_SUCCESS;

        /* advance */
        cbToWrite -= Hdr.cb;
        pvBuf = (uint8_t const *)pvBuf + Hdr.cb;
    }
}


/**
 * Selects and poll for close condition.
 *
 * We can use a relatively high poll timeout here since it's only used to get
 * us out of error paths.  In the normal cause of events, we'll get a
 * end-of-stream header.
 *
 * @returns VBox status code.
 *
 * @param   pState          The migration state data.
 */
static int migrationTcpReadSelect(MigrationState *pState)
{
    int rc;
    do
    {
        rc = RTTcpSelectOne(pState->mhSocket, 1000);
        if (RT_FAILURE(rc) && rc != VERR_TIMEOUT)
        {
            pState->mfIOError = true;
            LogRel(("Migration/TCP: Header select error: %Rrc\n", rc));
            break;
        }
        if (pState->mfStopReading)
        {
            rc = VERR_EOF;
            break;
        }
    } while (rc == VERR_TIMEOUT);
    return rc;
}


/**
 * @copydoc SSMSTRMOPS::pfnRead
 */
static DECLCALLBACK(int) migrationTcpOpRead(void *pvUser, uint64_t offStream, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    MigrationState *pState = (MigrationState *)pvUser;
    AssertReturn(!pState->mfIsSource, VERR_INVALID_HANDLE);

    for (;;)
    {
        int rc;

        /*
         * Check for various conditions and may have been signalled.
         */
        if (pState->mfEndOfStream)
            return VERR_EOF;
        if (pState->mfStopReading)
            return VERR_EOF;
        if (pState->mfIOError)
            return VERR_IO_GEN_FAILURE;

        /*
         * If there is no more data in the current block, read the next
         * block header.
         */
        if (!pState->mcbReadBlock)
        {
            rc = migrationTcpReadSelect(pState);
            if (RT_FAILURE(rc))
                return rc;
            MIGRATIONTCPHDR Hdr;
            rc = RTTcpRead(pState->mhSocket, &Hdr, sizeof(Hdr), NULL);
            if (RT_FAILURE(rc))
            {
                pState->mfIOError = true;
                LogRel(("Migration/TCP: Header read error: %Rrc\n", rc));
                return rc;
            }
            if (   Hdr.u32Magic != MIGRATIONTCPHDR_MAGIC
                || Hdr.cb > MIGRATIONTCPHDR_MAX_SIZE)
            {
                pState->mfIOError = true;
                LogRel(("Migration/TCP: Invalid block: u32Magic=%#x cb=%#x\n", Hdr.u32Magic, Hdr.cb));
                return VERR_IO_GEN_FAILURE;
            }

            pState->mcbReadBlock = Hdr.cb;
            if (!Hdr.cb)
            {
                pState->mfEndOfStream = true;
                return VERR_EOF;
            }

            if (pState->mfStopReading)
                return VERR_EOF;
        }

        /*
         * Read more data.
         */
        rc = migrationTcpReadSelect(pState);
        if (RT_FAILURE(rc))
            return rc;
        size_t cb = RT_MIN(pState->mcbReadBlock, cbToRead);
        rc = RTTcpRead(pState->mhSocket, pvBuf, cb, pcbRead);
        if (RT_FAILURE(rc))
        {
            pState->mfIOError = true;
            LogRel(("Migration/TCP: Data read error: %Rrc (cb=%#x)\n", rc, cb));
            return rc;
        }
        if (pcbRead)
        {
            pState->moffStream   += *pcbRead;
            pState->mcbReadBlock -= *pcbRead;
            return VINF_SUCCESS;
        }
        pState->moffStream   += cb;
        pState->mcbReadBlock -= cb;
        if (cbToRead == cb)
            return VINF_SUCCESS;

        /* Advance to the next block. */
        cbToRead -= cb;
        pvBuf = (uint8_t *)pvBuf + cb;
    }
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
    MigrationState *pState = (MigrationState *)pvUser;
    return pState->moffStream;
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
    MigrationState *pState = (MigrationState *)pvUser;

    if (pState->mfIsSource)
    {
        MIGRATIONTCPHDR EofHdr = { MIGRATIONTCPHDR_MAGIC, 0 };
        int rc = RTTcpWrite(pState->mhSocket, &EofHdr, sizeof(EofHdr));
        if (RT_FAILURE(rc))
        {
            LogRel(("Migration/TCP: EOF Header write error: %Rrc\n", rc));
            return rc;
        }
    }
    else
    {
        ASMAtomicWriteBool(&pState->mfStopReading, true);
        RTTcpFlush(pState->mhSocket);
    }

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
 * Do the live migration.
 *
 * @returns VBox status code.
 * @param   pState              The migration state.
 */
HRESULT
Console::migrationSrc(MigrationStateSrc *pState)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /*
     * Wait for Console::Migrate to change the state.
     */
    { AutoWriteLock autoLock(); }

    BOOL fCanceled = TRUE;
    HRESULT hrc = pState->mptrProgress->COMGETTER(Canceled)(&fCanceled);
    if (FAILED(hrc))
        return hrc;
    if (fCanceled)
        return setError(E_FAIL, tr("canceled"));

    /*
     * Try connect to the destination machine.
     * (Note. The caller cleans up mhSocket, so we can return without worries.)
     */
    int vrc = RTTcpClientConnect(pState->mstrHostname.c_str(), pState->muPort, &pState->mhSocket);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to connect to port %u on '%s': %Rrc"),
                        pState->muPort, pState->mstrHostname.c_str(), vrc);

    /* Read and check the welcome message. */
    char szLine[RT_MAX(128, sizeof(g_szWelcome))];
    vrc = RTTcpRead(pState->mhSocket, szLine, sizeof(g_szWelcome) - 1, NULL);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to read welcome message: %Rrc"), vrc);
    if (strcmp(szLine, g_szWelcome))
        return setError(E_FAIL, tr("Unexpected welcome '%s'"), szLine);

    /* password */
    pState->mstrPassword.append('\n');
    vrc = RTTcpWrite(pState->mhSocket, pState->mstrPassword.c_str(), pState->mstrPassword.length());
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to send password: %Rrc"), vrc);

    /* ACK */
    hrc = migrationSrcReadACK(pState, tr("Invalid password"));
    if (FAILED(hrc))
        return hrc;

    /*
     * Do compatability checks of the VM config and the host hardware.
     */
    /** @todo later */

    /*
     * Start loading the state.
     */
    hrc = migrationSrcSubmitCommand(pState, "load");
    if (FAILED(hrc))
        return hrc;

    void *pvUser = static_cast<void *>(static_cast<MigrationState *>(pState));
    vrc = VMR3Migrate(pState->mpVM, &g_migrationTcpOps, pvUser, NULL/** @todo progress*/, pvUser);
    if (vrc)
        return setError(E_FAIL, tr("VMR3Migrate -> %Rrc"), vrc);

    hrc = migrationSrcReadACK(pState);
    if (FAILED(hrc))
        return hrc;

    /*
     * State fun? Automatic power off?
     */

    return S_OK;
}


/**
 * Static thread method wrapper.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   hThread             The thread.
 * @param   pvUser              Pointer to a MigrationStateSrc instance.
 */
/*static*/ DECLCALLBACK(int)
Console::migrationSrcThreadWrapper(RTTHREAD hThread, void *pvUser)
{
    MigrationStateSrc *pState = (MigrationStateSrc *)pvUser;

    HRESULT hrc = pState->mptrConsole->migrationSrc(pState);
    pState->mptrProgress->notifyComplete(hrc);

    AutoWriteLock autoLock(pState->mptrConsole);
    if (pState->mptrConsole->mMachineState == MachineState_Saving)
    {
        VMSTATE enmVMState = VMR3GetState(pState->mpVM);
        if (SUCCEEDED(hrc))
        {
            if (enmVMState == VMSTATE_SUSPENDED)
                pState->mptrConsole->setMachineState(MachineState_Paused);
        }
        else
        {
            switch (enmVMState)
            {
                case VMSTATE_RUNNING:
                case VMSTATE_RUNNING_LS:
                case VMSTATE_DEBUGGING:
                case VMSTATE_DEBUGGING_LS:
                case VMSTATE_POWERING_OFF:
                case VMSTATE_POWERING_OFF_LS:
                case VMSTATE_RESETTING:
                case VMSTATE_RESETTING_LS:
                    pState->mptrConsole->setMachineState(MachineState_Running);
                    break;
                case VMSTATE_GURU_MEDITATION:
                case VMSTATE_GURU_MEDITATION_LS:
                    pState->mptrConsole->setMachineState(MachineState_Stuck);
                    break;
                default:
                    AssertMsgFailed(("%s\n", VMR3GetStateName(enmVMState)));
                case VMSTATE_SUSPENDED:
                case VMSTATE_SUSPENDED_LS:
                case VMSTATE_SUSPENDING:
                case VMSTATE_SUSPENDING_LS:
                case VMSTATE_SUSPENDING_EXT_LS:
                    pState->mptrConsole->setMachineState(MachineState_Paused);
                    break;
            }
        }
    }

    if (pState->mhSocket != NIL_RTSOCKET)
    {
        RTTcpClientClose(pState->mhSocket);
        pState->mhSocket = NIL_RTSOCKET;
    }
    delete pState;

    return VINF_SUCCESS;
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
     * Validate parameters, check+hold object status, write lock the object
     * and validate the state.
     */
    CheckComArgOutPointerValid(aProgress);
    CheckComArgStrNotEmptyOrNull(aHostname);
    CheckComArgNotNull(aHostname);
    CheckComArgExprMsg(aPort, aPort > 0 && aPort <= 65535, ("is %u", aPort));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock autoLock(this);
    LogFlowThisFunc(("mMachineState=%d\n", mMachineState));

    switch (mMachineState)
    {
        case MachineState_Running:
        case MachineState_Paused:
            break;

        default:
            return setError(VBOX_E_INVALID_VM_STATE,
                tr("Invalid machine state: %s (must be Running, Paused or Stuck)"),
                Global::stringifyMachineState(mMachineState));
    }


    /*
     * Create a progress object, spawn a worker thread and change the state.
     * Note! The thread won't start working until we release the lock.
     */
    LogFlowThisFunc(("Initiating LIVE MIGRATION request...\n"));

    ComObjPtr<Progress> ptrMigrateProgress;
    HRESULT hrc = ptrMigrateProgress.createObject();
    CheckComRCReturnRC(hrc);
    hrc = ptrMigrateProgress->init(static_cast<IConsole *>(this),
                                   Bstr(tr("Live Migration")),
                                   TRUE /*aCancelable*/);
    CheckComRCReturnRC(hrc);

    MigrationStateSrc *pState = new MigrationStateSrc(this, mpVM);
    pState->mstrPassword = aPassword;
    pState->mstrHostname = aHostname;
    pState->muPort       = aPort;
    pState->mptrProgress = ptrMigrateProgress;

    int vrc = RTThreadCreate(NULL, Console::migrationSrcThreadWrapper, (void *)pState, 0 /*cbStack*/,
                             RTTHREADTYPE_EMULATION, 0 /*fFlags*/, "Migrate");
    if (RT_SUCCESS(vrc))
    {
        hrc = setMachineState(MachineState_Saving);
        if (SUCCEEDED(hrc))
            ptrMigrateProgress.queryInterfaceTo(aProgress);
        else
            ptrMigrateProgress->Cancel();
    }
    else
    {
        delete pState;
        hrc = setError(E_FAIL, tr("RTThreadCreate -> %Rrc"), vrc);
    }

    return hrc;
}


/**
 * Creates a TCP server that listens for the source machine and passes control
 * over to Console::migrationDstServeConnection().
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle
 * @param   pMachine            The IMachine for the virtual machine.
 * @param   fStartPaused        Whether to start it in the Paused (true) or
 *                              Running (false) state,
 * @param   pvVMCallbackTask    The callback task pointer for
 *                              stateProgressCallback().
 */
int
Console::migrationDst(PVM pVM, IMachine *pMachine, bool fStartPaused, void *pvVMCallbackTask)
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
    strPassword.append('\n');           /* To simplify password checking. */

    Utf8Str strBind("");
    /** @todo Add a bind address property. */
    const char *pszBindAddress = strBind.isEmpty() ? NULL : strBind.c_str();


    /*
     * Create the TCP server.
     */
    int vrc;
    PRTTCPSERVER hServer;
    if (uPort)
        vrc = RTTcpServerCreateEx(pszBindAddress, uPort, &hServer);
    else
    {
        for (int cTries = 10240; cTries > 0; cTries--)
        {
            uPort = RTRandU32Ex(cTries >= 8192 ? 49152 : 1024, 65534);
            vrc = RTTcpServerCreateEx(pszBindAddress, uPort, &hServer);
            if (vrc != VERR_NET_ADDRESS_IN_USE)
                break;
        }
        if (RT_SUCCESS(vrc))
        {
            HRESULT hrc = pMachine->COMSETTER(LiveMigrationPort)(uPort);
            if (FAILED(hrc))
            {
                RTTcpServerDestroy(hServer);
                return VERR_GENERAL_FAILURE;
            }
        }
    }
    if (RT_FAILURE(vrc))
        return vrc;

    /*
     * Create a one-shot timer for timing out after 5 mins.
     */
    RTTIMERLR hTimerLR;
    vrc = RTTimerLRCreateEx(&hTimerLR, 0 /*ns*/, RTTIMER_FLAGS_CPU_ANY, migrationTimeout, hServer);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTTimerLRStart(hTimerLR, 5*60*UINT64_C(1000000000) /*ns*/);
        if (RT_SUCCESS(vrc))
        {
            /*
             * Do the job, when it returns we're done.
             */
            MigrationStateDst State(this, pVM, pMachine, &hTimerLR);
            State.mstrPassword      = strPassword;
            State.mhServer          = hServer;
            State.mpvVMCallbackTask = pvVMCallbackTask;

            vrc = RTTcpServerListen(hServer, Console::migrationDstServeConnection, &State);
            if (vrc == VERR_TCP_SERVER_STOP)
                vrc = State.mRc;
            if (RT_SUCCESS(vrc))
            {
                if (fStartPaused)
                    setMachineState(MachineState_Paused);
                else
                    vrc = VMR3Resume(pVM);
            }
            else
            {
                LogRel(("Migration: RTTcpServerListen -> %Rrc\n", vrc));
            }
        }

        RTTimerLRDestroy(hTimerLR);
    }
    RTTcpServerDestroy(hServer);

    return vrc;
}


static int migrationTcpWriteACK(MigrationStateDst *pState)
{
    int rc = RTTcpWrite(pState->mhSocket, "ACK\n", sizeof("ACK\n") - 1);
    if (RT_FAILURE(rc))
        LogRel(("Migration: RTTcpWrite(,ACK,) -> %Rrc\n", rc));
    RTTcpFlush(pState->mhSocket);
    return rc;
}


static int migrationTcpWriteNACK(MigrationStateDst *pState, int32_t rc2)
{
    char    szMsg[64];
    size_t  cch = RTStrPrintf(szMsg, sizeof(szMsg), "NACK=%d\n", rc2);
    int rc = RTTcpWrite(pState->mhSocket, szMsg, cch);
    if (RT_FAILURE(rc))
        LogRel(("Migration: RTTcpWrite(,%s,%zu) -> %Rrc\n", szMsg, cch, rc));
    RTTcpFlush(pState->mhSocket);
    return rc;
}


/**
 * @copydoc FNRTTCPSERVE
 */
/*static*/ DECLCALLBACK(int)
Console::migrationDstServeConnection(RTSOCKET Sock, void *pvUser)
{
    MigrationStateDst *pState = (MigrationStateDst *)pvUser;
    pState->mhSocket = Sock;

    /*
     * Say hello.
     */
    int vrc = RTTcpWrite(Sock, g_szWelcome, sizeof(g_szWelcome) - 1);
    if (RT_FAILURE(vrc))
    {
        LogRel(("Migration: Failed to write welcome message: %Rrc\n", vrc));
        return VINF_SUCCESS;
    }

    /*
     * Password (includes '\n', see migrationDst).  If it's right, tell
     * the TCP server to stop listening (frees up host resources and makes sure
     * this is the last connection attempt).
     */
    const char *pszPassword = pState->mstrPassword.c_str();
    unsigned    off = 0;
    while (pszPassword[off])
    {
        char ch;
        vrc = RTTcpRead(Sock, &ch, sizeof(ch), NULL);
        if (    RT_FAILURE(vrc)
            ||  pszPassword[off] != ch)
        {
            if (RT_FAILURE(vrc))
                LogRel(("Migration: Password read failure (off=%u): %Rrc\n", off, vrc));
            else
                LogRel(("Migration: Invalid password (off=%u)\n", off));
            migrationTcpWriteNACK(pState, VERR_AUTHENTICATION_FAILURE);
            return VINF_SUCCESS;
        }
        off++;
    }
    vrc = migrationTcpWriteACK(pState);
    if (RT_FAILURE(vrc))
        return vrc;

    RTTcpServerShutdown(pState->mhServer);
    RTTimerLRDestroy(*pState->mphTimerLR);
    *pState->mphTimerLR = NIL_RTTIMERLR;

    /*
     * Command processing loop.
     */
    for (;;)
    {
        char szCmd[128];
        vrc = migrationTcpReadLine(pState, szCmd, sizeof(szCmd));
        if (RT_FAILURE(vrc))
            break;

        if (!strcmp(szCmd, "load"))
        {
            vrc = migrationTcpWriteACK(pState);
            if (RT_FAILURE(vrc))
                break;

            pState->moffStream = 0;
            void *pvUser = static_cast<void *>(static_cast<MigrationState *>(pState));
            vrc = VMR3LoadFromStream(pState->mpVM, &g_migrationTcpOps, pvUser,
                                     Console::stateProgressCallback, pState->mpvVMCallbackTask);
            if (RT_FAILURE(vrc))
            {
                LogRel(("Migration: VMR3LoadFromStream -> %Rrc\n", vrc));
                migrationTcpWriteNACK(pState, vrc);
                break;
            }

            vrc = migrationTcpWriteACK(pState);
            if (RT_FAILURE(vrc))
                break;
        }
        /** @todo implement config verification and hardware compatability checks. Or
         *        maybe leave part of these to the saved state machinery? */
        else if (!strcmp(szCmd, "done"))
        {
            vrc = migrationTcpWriteACK(pState);
            break;
        }
        else
        {
            LogRel(("Migration: Unknown command '%s'\n", szCmd));
            break;
        }
    }

    pState->mRc = vrc;
    pState->mhSocket = NIL_RTSOCKET;
    LogFlowFunc(("returns mRc=%Rrc\n", vrc));
    return VERR_TCP_SERVER_STOP;
}

