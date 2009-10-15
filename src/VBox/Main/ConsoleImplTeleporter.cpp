/* $Id$ */
/** @file
 * VBox Console COM Class implementation, The Teleporter Part.
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
 * Base class for the teleporter state.
 *
 * These classes are used as advanced structs, not as proper classes.
 */
class TeleporterState
{
public:
    ComPtr<Console>     mptrConsole;
    PVM                 mpVM;
    ComObjPtr<Progress> mptrProgress;
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

    TeleporterState(Console *pConsole, PVM pVM, Progress *pProgress, bool fIsSource)
        : mptrConsole(pConsole)
        , mpVM(pVM)
        , mptrProgress(pProgress)
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
 * Teleporter state used by the source side.
 */
class TeleporterStateSrc : public TeleporterState
{
public:
    Utf8Str             mstrHostname;
    uint32_t            muPort;

    TeleporterStateSrc(Console *pConsole, PVM pVM, Progress *pProgress)
        : TeleporterState(pConsole, pVM, pProgress, true /*fIsSource*/)
        , muPort(UINT32_MAX)
    {
    }
};


/**
 * Teleporter state used by the destiation side.
 */
class TeleporterStateTrg : public TeleporterState
{
public:
    IMachine           *mpMachine;
    PRTTCPSERVER        mhServer;
    PRTTIMERLR          mphTimerLR;
    int                 mRc;

    TeleporterStateTrg(Console *pConsole, PVM pVM, Progress *pProgress,
                       IMachine *pMachine, PRTTIMERLR phTimerLR)
        : TeleporterState(pConsole, pVM, pProgress, false /*fIsSource*/)
        , mpMachine(pMachine)
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
typedef struct TELEPORTERTCPHDR
{
    /** Magic value. */
    uint32_t    u32Magic;
    /** The size of the data block following this header.
     * 0 indicates the end of the stream. */
    uint32_t    cb;
} TELEPORTERTCPHDR;
/** Magic value for TELEPORTERTCPHDR::u32Magic. (Egberto Gismonti Amin) */
#define TELEPORTERTCPHDR_MAGIC       UINT32_C(0x19471205)
/** The max block size. */
#define TELEPORTERTCPHDR_MAX_SIZE    UINT32_C(0x00fffff8)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const char g_szWelcome[] = "VirtualBox-Teleporter-1.0\n";


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
static int teleporterTcpReadLine(TeleporterState *pState, char *pszBuf, size_t cchBuf)
{
    char       *pszStart = pszBuf;
    RTSOCKET    Sock     = pState->mhSocket;

    AssertReturn(cchBuf > 1, VERR_INTERNAL_ERROR);
    *pszBuf = '\0';

    /* dead simple approach. */
    for (;;)
    {
        char ch;
        int rc = RTTcpRead(Sock, &ch, sizeof(ch), NULL);
        if (RT_FAILURE(rc))
        {
            LogRel(("Teleporter: RTTcpRead -> %Rrc while reading string ('%s')\n", rc, pszStart));
            return rc;
        }
        if (    ch == '\n'
            ||  ch == '\0')
            return VINF_SUCCESS;
        if (cchBuf <= 1)
        {
            LogRel(("Teleporter: String buffer overflow: '%s'\n", pszStart));
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
 * @returns S_OK on ACK, E_FAIL+setError() on failure or NACK.
 * @param   pState              The teleporter source state.
 * @param   pszWhich            Which ACK is this this?
 * @param   pszNAckMsg          Optional NACK message.
 *
 * @remarks the setError laziness forces this to be a Console member.
 */
HRESULT
Console::teleporterSrcReadACK(TeleporterStateSrc *pState, const char *pszWhich,
                             const char *pszNAckMsg /*= NULL*/)
{
    char szMsg[128];
    int vrc = teleporterTcpReadLine(pState, szMsg, sizeof(szMsg));
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed reading ACK(%s): %Rrc"), pszWhich, vrc);
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
                    LogRel(("Teleporter: %s: NACK=%Rrc (%d)\n", pszWhich, vrc2, vrc2));
                    return setError(E_FAIL, pszNAckMsg);
                }
                return setError(E_FAIL, "NACK(%s) - %Rrc (%d)", pszWhich, vrc2, vrc2);
            }
        }
        return setError(E_FAIL, tr("%s: Expected ACK or NACK, got '%s'"), pszWhich, szMsg);
    }
    return S_OK;
}


/**
 * Submitts a command to the destination and waits for the ACK.
 *
 * @returns S_OK on ACKed command, E_FAIL+setError() on failure.
 *
 * @param   pState              The teleporter source state.
 * @param   pszCommand          The command.
 *
 * @remarks the setError laziness forces this to be a Console member.
 */
HRESULT
Console::teleporterSrcSubmitCommand(TeleporterStateSrc *pState, const char *pszCommand)
{
    size_t cchCommand = strlen(pszCommand);
    int vrc = RTTcpWrite(pState->mhSocket, pszCommand, cchCommand);
    if (RT_SUCCESS(vrc))
        vrc = RTTcpWrite(pState->mhSocket, "\n", sizeof("\n") - 1);
    if (RT_SUCCESS(vrc))
        vrc = RTTcpFlush(pState->mhSocket);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed writing command '%s': %Rrc"), pszCommand, vrc);
    return teleporterSrcReadACK(pState, pszCommand);
}


/**
 * @copydoc SSMSTRMOPS::pfnWrite
 */
static DECLCALLBACK(int) teleporterTcpOpWrite(void *pvUser, uint64_t offStream, const void *pvBuf, size_t cbToWrite)
{
    TeleporterState *pState = (TeleporterState *)pvUser;

    AssertReturn(cbToWrite > 0, VINF_SUCCESS);
    AssertReturn(pState->mfIsSource, VERR_INVALID_HANDLE);

    for (;;)
    {
        /* Write block header. */
        TELEPORTERTCPHDR Hdr;
        Hdr.u32Magic = TELEPORTERTCPHDR_MAGIC;
        Hdr.cb       = RT_MIN(cbToWrite, TELEPORTERTCPHDR_MAX_SIZE);
        int rc = RTTcpWrite(pState->mhSocket, &Hdr, sizeof(Hdr));
        if (RT_FAILURE(rc))
        {
            LogRel(("Teleporter/TCP: Header write error: %Rrc\n", rc));
            return rc;
        }

        /* Write the data. */
        rc = RTTcpWrite(pState->mhSocket, pvBuf, Hdr.cb);
        if (RT_FAILURE(rc))
        {
            LogRel(("Teleporter/TCP: Data write error: %Rrc (cb=%#x)\n", rc, Hdr.cb));
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
 * @param   pState          The teleporter state data.
 */
static int teleporterTcpReadSelect(TeleporterState *pState)
{
    int rc;
    do
    {
        rc = RTTcpSelectOne(pState->mhSocket, 1000);
        if (RT_FAILURE(rc) && rc != VERR_TIMEOUT)
        {
            pState->mfIOError = true;
            LogRel(("Teleporter/TCP: Header select error: %Rrc\n", rc));
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
static DECLCALLBACK(int) teleporterTcpOpRead(void *pvUser, uint64_t offStream, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    TeleporterState *pState = (TeleporterState *)pvUser;
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
            rc = teleporterTcpReadSelect(pState);
            if (RT_FAILURE(rc))
                return rc;
            TELEPORTERTCPHDR Hdr;
            rc = RTTcpRead(pState->mhSocket, &Hdr, sizeof(Hdr), NULL);
            if (RT_FAILURE(rc))
            {
                pState->mfIOError = true;
                LogRel(("Teleporter/TCP: Header read error: %Rrc\n", rc));
                return rc;
            }
            if (   Hdr.u32Magic != TELEPORTERTCPHDR_MAGIC
                || Hdr.cb > TELEPORTERTCPHDR_MAX_SIZE)
            {
                pState->mfIOError = true;
                LogRel(("Teleporter/TCP: Invalid block: u32Magic=%#x cb=%#x\n", Hdr.u32Magic, Hdr.cb));
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
        rc = teleporterTcpReadSelect(pState);
        if (RT_FAILURE(rc))
            return rc;
        size_t cb = RT_MIN(pState->mcbReadBlock, cbToRead);
        rc = RTTcpRead(pState->mhSocket, pvBuf, cb, pcbRead);
        if (RT_FAILURE(rc))
        {
            pState->mfIOError = true;
            LogRel(("Teleporter/TCP: Data read error: %Rrc (cb=%#x)\n", rc, cb));
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
static DECLCALLBACK(int) teleporterTcpOpSeek(void *pvUser, int64_t offSeek, unsigned uMethod, uint64_t *poffActual)
{
    return VERR_NOT_SUPPORTED;
}


/**
 * @copydoc SSMSTRMOPS::pfnTell
 */
static DECLCALLBACK(uint64_t) teleporterTcpOpTell(void *pvUser)
{
    TeleporterState *pState = (TeleporterState *)pvUser;
    return pState->moffStream;
}


/**
 * @copydoc SSMSTRMOPS::pfnSize
 */
static DECLCALLBACK(int) teleporterTcpOpSize(void *pvUser, uint64_t *pcb)
{
    return VERR_NOT_SUPPORTED;
}


/**
 * @copydoc SSMSTRMOPS::pfnClose
 */
static DECLCALLBACK(int) teleporterTcpOpClose(void *pvUser)
{
    TeleporterState *pState = (TeleporterState *)pvUser;

    if (pState->mfIsSource)
    {
        TELEPORTERTCPHDR EofHdr = { TELEPORTERTCPHDR_MAGIC, 0 };
        int rc = RTTcpWrite(pState->mhSocket, &EofHdr, sizeof(EofHdr));
        if (RT_SUCCESS(rc))
            rc = RTTcpFlush(pState->mhSocket);
        if (RT_FAILURE(rc))
        {
            LogRel(("Teleporter/TCP: EOF Header write error: %Rrc\n", rc));
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
static SSMSTRMOPS const g_teleporterTcpOps =
{
    SSMSTRMOPS_VERSION,
    teleporterTcpOpWrite,
    teleporterTcpOpRead,
    teleporterTcpOpSeek,
    teleporterTcpOpTell,
    teleporterTcpOpSize,
    teleporterTcpOpClose,
    SSMSTRMOPS_VERSION
};


/**
 * Progress cancelation callback.
 */
static void teleporterProgressCancelCallback(void *pvUser)
{
    TeleporterState *pState = (TeleporterState *)pvUser;
    SSMR3Cancel(pState->mpVM);
    if (!pState->mfIsSource)
    {
        TeleporterStateTrg *pStateTrg = (TeleporterStateTrg *)pState;
        RTTcpServerShutdown(pStateTrg->mhServer);
    }
}

/**
 * @copydoc PFNVMPROGRESS
 */
static DECLCALLBACK(int) teleporterProgressCallback(PVM pVM, unsigned uPercent, void *pvUser)
{
    TeleporterState *pState = (TeleporterState *)pvUser;
    if (pState->mptrProgress)
    {
        HRESULT hrc = pState->mptrProgress->SetCurrentOperationProgress(uPercent);
        if (FAILED(hrc))
        {
            /* check if the failure was caused by cancellation. */
            BOOL fCanceled;
            hrc = pState->mptrProgress->COMGETTER(Canceled)(&fCanceled);
            if (SUCCEEDED(hrc) && fCanceled)
            {
                SSMR3Cancel(pState->mpVM);
                return VERR_SSM_CANCELLED;
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * @copydoc FNRTTIMERLR
 */
static DECLCALLBACK(void) teleporterDstTimeout(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick)
{
    /* This is harmless for any open connections. */
    RTTcpServerShutdown((PRTTCPSERVER)pvUser);
}


/**
 * Do the teleporter.
 *
 * @returns VBox status code.
 * @param   pState              The teleporter state.
 */
HRESULT
Console::teleporterSrc(TeleporterStateSrc *pState)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /*
     * Wait for Console::Teleport to change the state.
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
    RT_ZERO(szLine);
    vrc = RTTcpRead(pState->mhSocket, szLine, sizeof(g_szWelcome) - 1, NULL);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to read welcome message: %Rrc"), vrc);
    if (strcmp(szLine, g_szWelcome))
        return setError(E_FAIL, tr("Unexpected welcome %.*Rhxs"), sizeof(g_szWelcome) - 1, szLine);

    /* password */
    pState->mstrPassword.append('\n');
    vrc = RTTcpWrite(pState->mhSocket, pState->mstrPassword.c_str(), pState->mstrPassword.length());
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to send password: %Rrc"), vrc);

    /* ACK */
    hrc = teleporterSrcReadACK(pState, "password", tr("Invalid password"));
    if (FAILED(hrc))
        return hrc;

    /*
     * Do compatability checks of the VM config and the host hardware.
     */
    /** @todo later
     * Update: As much as possible will be taken care of by the first snapshot
     *         pass. */

    /*
     * Start loading the state.
     */
    hrc = teleporterSrcSubmitCommand(pState, "load");
    if (FAILED(hrc))
        return hrc;

    void *pvUser = static_cast<void *>(static_cast<TeleporterState *>(pState));
    vrc = VMR3Teleport(pState->mpVM, &g_teleporterTcpOps, pvUser, teleporterProgressCallback, pvUser);
    if (vrc)
        return setError(E_FAIL, tr("VMR3Teleport -> %Rrc"), vrc);

    hrc = teleporterSrcReadACK(pState, "load-complete");
    if (FAILED(hrc))
        return hrc;

    /*
     * State fun? Automatic power off?
     */
    hrc = teleporterSrcSubmitCommand(pState, "done");
    if (FAILED(hrc))
        return hrc;

    return S_OK;
}


/**
 * Static thread method wrapper.
 *
 * @returns VINF_SUCCESS (ignored).
 * @param   hThread             The thread.
 * @param   pvUser              Pointer to a TeleporterStateSrc instance.
 */
/*static*/ DECLCALLBACK(int)
Console::teleporterSrcThreadWrapper(RTTHREAD hThread, void *pvUser)
{
    TeleporterStateSrc *pState = (TeleporterStateSrc *)pvUser;

    AutoVMCaller autoVMCaller(pState->mptrConsole);
    HRESULT hrc = autoVMCaller.rc();

    if (SUCCEEDED(hrc))
        hrc = pState->mptrConsole->teleporterSrc(pState);

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
                    /** @todo somehow make the VMM report back external pause even on error. */
                    autoLock.leave();
                    VMR3Resume(pState->mpVM);
                    break;
            }
        }
    }

    if (pState->mhSocket != NIL_RTSOCKET)
    {
        RTTcpClientClose(pState->mhSocket);
        pState->mhSocket = NIL_RTSOCKET;
    }

    pState->mptrProgress->setCancelCallback(NULL, NULL);
    delete pState;

    return VINF_SUCCESS;
}


/**
 * Start teleporter to the specified target.
 *
 * @returns COM status code.
 *
 * @param   aHostname   The name of the target host.
 * @param   aPort       The TCP port number.
 * @param   aPassword   The password.
 * @param   aProgress   Where to return the progress object.
 */
STDMETHODIMP
Console::Teleport(IN_BSTR aHostname, ULONG aPort, IN_BSTR aPassword, IProgress **aProgress)
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
    LogFlowThisFunc(("Initiating TELEPORTER request...\n"));

    ComObjPtr<Progress> ptrProgress;
    HRESULT hrc = ptrProgress.createObject();
    CheckComRCReturnRC(hrc);
    hrc = ptrProgress->init(static_cast<IConsole *>(this),
                                        Bstr(tr("Teleporter")),
                                        TRUE /*aCancelable*/);
    CheckComRCReturnRC(hrc);

    TeleporterStateSrc *pState = new TeleporterStateSrc(this, mpVM, ptrProgress);
    pState->mstrPassword = aPassword;
    pState->mstrHostname = aHostname;
    pState->muPort       = aPort;

    void *pvUser = static_cast<void *>(static_cast<TeleporterState *>(pState));
    ptrProgress->setCancelCallback(teleporterProgressCancelCallback, pvUser);

    int vrc = RTThreadCreate(NULL, Console::teleporterSrcThreadWrapper, (void *)pState, 0 /*cbStack*/,
                             RTTHREADTYPE_EMULATION, 0 /*fFlags*/, "Teleport");
    if (RT_SUCCESS(vrc))
    {
        hrc = setMachineState(MachineState_Saving);
        if (SUCCEEDED(hrc))
            ptrProgress.queryInterfaceTo(aProgress);
        else
            ptrProgress->Cancel();
    }
    else
    {
        ptrProgress->setCancelCallback(NULL, NULL);
        delete pState;
        hrc = setError(E_FAIL, tr("RTThreadCreate -> %Rrc"), vrc);
    }

    return hrc;
}


/**
 * Creates a TCP server that listens for the source machine and passes control
 * over to Console::teleporterTrgServeConnection().
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle
 * @param   pMachine            The IMachine for the virtual machine.
 * @param   fStartPaused        Whether to start it in the Paused (true) or
 *                              Running (false) state,
 * @param   pProgress           Pointer to the progress object.
 */
int
Console::teleporterTrg(PVM pVM, IMachine *pMachine, bool fStartPaused, Progress *pProgress)
{
    /*
     * Get the config.
     */
    ULONG uPort;
    HRESULT hrc = pMachine->COMGETTER(TeleporterPort)(&uPort);
    if (FAILED(hrc))
        return VERR_GENERAL_FAILURE;
    ULONG const uPortOrg = uPort;

    Bstr bstrAddress;
    hrc = pMachine->COMGETTER(TeleporterAddress)(bstrAddress.asOutParam());
    if (FAILED(hrc))
        return VERR_GENERAL_FAILURE;
    Utf8Str strAddress(bstrAddress);
    const char *pszAddress = strAddress.isEmpty() ? NULL : strAddress.c_str();

    Bstr bstrPassword;
    hrc = pMachine->COMGETTER(TeleporterPassword)(bstrPassword.asOutParam());
    if (FAILED(hrc))
        return VERR_GENERAL_FAILURE;
    Utf8Str strPassword(bstrPassword);
    strPassword.append('\n');           /* To simplify password checking. */

    /*
     * Create the TCP server.
     */
    int vrc;
    PRTTCPSERVER hServer;
    if (uPort)
        vrc = RTTcpServerCreateEx(pszAddress, uPort, &hServer);
    else
    {
        for (int cTries = 10240; cTries > 0; cTries--)
        {
            uPort = RTRandU32Ex(cTries >= 8192 ? 49152 : 1024, 65534);
            vrc = RTTcpServerCreateEx(pszAddress, uPort, &hServer);
            if (vrc != VERR_NET_ADDRESS_IN_USE)
                break;
        }
        if (RT_SUCCESS(vrc))
        {
            HRESULT hrc = pMachine->COMSETTER(TeleporterPort)(uPort);
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
    vrc = RTTimerLRCreateEx(&hTimerLR, 0 /*ns*/, RTTIMER_FLAGS_CPU_ANY, teleporterDstTimeout, hServer);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTTimerLRStart(hTimerLR, 5*60*UINT64_C(1000000000) /*ns*/);
        if (RT_SUCCESS(vrc))
        {
            /*
             * Do the job, when it returns we're done.
             */
            TeleporterStateTrg State(this, pVM, pProgress, pMachine, &hTimerLR);
            State.mstrPassword      = strPassword;
            State.mhServer          = hServer;

            void *pvUser = static_cast<void *>(static_cast<TeleporterState *>(&State));
            pProgress->setCancelCallback(teleporterProgressCancelCallback, pvUser);

            vrc = RTTcpServerListen(hServer, Console::teleporterTrgServeConnection, &State);
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
                LogRel(("Teleporter: RTTcpServerListen -> %Rrc\n", vrc));
            }

            pProgress->setCancelCallback(NULL, NULL);
        }

        RTTimerLRDestroy(hTimerLR);
    }
    RTTcpServerDestroy(hServer);

    /*
     * If we change TeleporterPort above, set it back to it's original
     * value before returning.
     */
    if (uPortOrg != uPort)
        pMachine->COMSETTER(TeleporterPort)(uPortOrg);

    return vrc;
}


static int teleporterTcpWriteACK(TeleporterStateTrg *pState)
{
    int rc = RTTcpWrite(pState->mhSocket, "ACK\n", sizeof("ACK\n") - 1);
    if (RT_FAILURE(rc))
        LogRel(("Teleporter: RTTcpWrite(,ACK,) -> %Rrc\n", rc));
    RTTcpFlush(pState->mhSocket);
    return rc;
}


static int teleporterTcpWriteNACK(TeleporterStateTrg *pState, int32_t rc2)
{
    char    szMsg[64];
    size_t  cch = RTStrPrintf(szMsg, sizeof(szMsg), "NACK=%d\n", rc2);
    int rc = RTTcpWrite(pState->mhSocket, szMsg, cch);
    if (RT_FAILURE(rc))
        LogRel(("Teleporter: RTTcpWrite(,%s,%zu) -> %Rrc\n", szMsg, cch, rc));
    RTTcpFlush(pState->mhSocket);
    return rc;
}


/**
 * @copydoc FNRTTCPSERVE
 */
/*static*/ DECLCALLBACK(int)
Console::teleporterTrgServeConnection(RTSOCKET Sock, void *pvUser)
{
    TeleporterStateTrg *pState = (TeleporterStateTrg *)pvUser;
    pState->mhSocket = Sock;

    /*
     * Say hello.
     */
    int vrc = RTTcpWrite(Sock, g_szWelcome, sizeof(g_szWelcome) - 1);
    if (RT_FAILURE(vrc))
    {
        LogRel(("Teleporter: Failed to write welcome message: %Rrc\n", vrc));
        return VINF_SUCCESS;
    }

    /*
     * Password (includes '\n', see teleporterTrg).  If it's right, tell
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
                LogRel(("Teleporter: Password read failure (off=%u): %Rrc\n", off, vrc));
            else
                LogRel(("Teleporter: Invalid password (off=%u)\n", off));
            teleporterTcpWriteNACK(pState, VERR_AUTHENTICATION_FAILURE);
            return VINF_SUCCESS;
        }
        off++;
    }
    vrc = teleporterTcpWriteACK(pState);
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
        vrc = teleporterTcpReadLine(pState, szCmd, sizeof(szCmd));
        if (RT_FAILURE(vrc))
            break;

        if (!strcmp(szCmd, "load"))
        {
            vrc = teleporterTcpWriteACK(pState);
            if (RT_FAILURE(vrc))
                break;

            pState->moffStream = 0;
            void *pvUser = static_cast<void *>(static_cast<TeleporterState *>(pState));
            vrc = VMR3LoadFromStream(pState->mpVM, &g_teleporterTcpOps, pvUser,
                                     teleporterProgressCallback, pvUser);
            if (RT_FAILURE(vrc))
            {
                LogRel(("Teleporter: VMR3LoadFromStream -> %Rrc\n", vrc));
                teleporterTcpWriteNACK(pState, vrc);
                break;
            }

            /* The EOS might not have been read, make sure it is. */
            pState->mfStopReading = false;
            size_t cbRead;
            vrc = teleporterTcpOpRead(pvUser, pState->moffStream, szCmd, 1, &cbRead);
            if (vrc != VERR_EOF)
            {
                LogRel(("Teleporter: Draining teleporterTcpOpRead -> %Rrc\n", vrc));
                teleporterTcpWriteNACK(pState, vrc);
                break;
            }

            vrc = teleporterTcpWriteACK(pState);
            if (RT_FAILURE(vrc))
                break;
        }
        /** @todo implement config verification and hardware compatability checks. Or
         *        maybe leave part of these to the saved state machinery?
         * Update: We're doing as much as possible in the first SSM pass. */
        else if (!strcmp(szCmd, "done"))
        {
            vrc = teleporterTcpWriteACK(pState);
            break;
        }
        else
        {
            LogRel(("Teleporter: Unknown command '%s' (%.*Rhxs)\n", szCmd, strlen(szCmd), szCmd));
            vrc = VERR_NOT_IMPLEMENTED;
            teleporterTcpWriteNACK(pState, vrc);
            break;
        }
    }

    pState->mRc = vrc;
    pState->mhSocket = NIL_RTSOCKET;
    LogFlowFunc(("returns mRc=%Rrc\n", vrc));
    return VERR_TCP_SERVER_STOP;
}

