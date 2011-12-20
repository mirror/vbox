/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Guest
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "GuestImpl.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
#endif
#include <iprt/cpp/utils.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/isofs.h>
#include <iprt/list.h>
#include <iprt/path.h>
#include <VBox/vmm/pgm.h>

#include <memory>

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_WITH_GUEST_CONTROL
/**
 * Appends environment variables to the environment block.
 *
 * Each var=value pair is separated by the null character ('\\0').  The whole
 * block will be stored in one blob and disassembled on the guest side later to
 * fit into the HGCM param structure.
 *
 * @returns VBox status code.
 *
 * @param   pszEnvVar       The environment variable=value to append to the
 *                          environment block.
 * @param   ppvList         This is actually a pointer to a char pointer
 *                          variable which keeps track of the environment block
 *                          that we're constructing.
 * @param   pcbList         Pointer to the variable holding the current size of
 *                          the environment block.  (List is a misnomer, go
 *                          ahead a be confused.)
 * @param   pcEnvVars       Pointer to the variable holding count of variables
 *                          stored in the environment block.
 */
int Guest::prepareExecuteEnv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnvVars)
{
    int rc = VINF_SUCCESS;
    uint32_t cchEnv = strlen(pszEnv); Assert(cchEnv >= 2);
    if (*ppvList)
    {
        uint32_t cbNewLen = *pcbList + cchEnv + 1; /* Include zero termination. */
        char *pvTmp = (char *)RTMemRealloc(*ppvList, cbNewLen);
        if (pvTmp == NULL)
            rc = VERR_NO_MEMORY;
        else
        {
            memcpy(pvTmp + *pcbList, pszEnv, cchEnv);
            pvTmp[cbNewLen - 1] = '\0'; /* Add zero termination. */
            *ppvList = (void **)pvTmp;
        }
    }
    else
    {
        char *pszTmp;
        if (RTStrAPrintf(&pszTmp, "%s", pszEnv) >= 0)
        {
            *ppvList = (void **)pszTmp;
            /* Reset counters. */
            *pcEnvVars = 0;
            *pcbList = 0;
        }
    }
    if (RT_SUCCESS(rc))
    {
        *pcbList += cchEnv + 1; /* Include zero termination. */
        *pcEnvVars += 1;        /* Increase env variable count. */
    }
    return rc;
}

/**
 * Adds a callback with a user provided data block and an optional progress object
 * to the callback map. A callback is identified by a unique context ID which is used
 * to identify a callback from the guest side.
 *
 * @return  IPRT status code.
 * @param   pCallback
 * @param   puContextID
 */
int Guest::callbackAdd(const PVBOXGUESTCTRL_CALLBACK pCallback, uint32_t *puContextID)
{
    AssertPtrReturn(pCallback, VERR_INVALID_PARAMETER);
    /* puContextID is optional. */

    int rc;

    /* Create a new context ID and assign it. */
    uint32_t uNewContextID = 0;
    for (;;)
    {
        /* Create a new context ID ... */
        uNewContextID = ASMAtomicIncU32(&mNextContextID);
        if (uNewContextID == UINT32_MAX)
            ASMAtomicUoWriteU32(&mNextContextID, 1000);
        /* Is the context ID already used?  Try next ID ... */
        if (!callbackExists(uNewContextID))
        {
            /* Callback with context ID was not found. This means
             * we can use this context ID for our new callback we want
             * to add below. */
            rc = VINF_SUCCESS;
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Add callback with new context ID to our callback map. */
        mCallbackMap[uNewContextID] = *pCallback;
        Assert(mCallbackMap.size());

        /* Report back new context ID. */
        if (puContextID)
            *puContextID = uNewContextID;
    }

    return rc;
}

/**
 * Does not do locking!
 *
 * @param   uContextID
 */
void Guest::callbackDestroy(uint32_t uContextID)
{
    AssertReturnVoid(uContextID);

    CallbackMapIter it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        LogFlowFunc(("Callback with CID=%u found\n", uContextID));
        if (it->second.pvData)
        {
            LogFlowFunc(("Destroying callback with CID=%u ...\n", uContextID));

            callbackFreeUserData(it->second.pvData);
            it->second.cbData = 0;
        }

        /* Remove callback context (not used anymore). */
        mCallbackMap.erase(it);
    }
}

bool Guest::callbackExists(uint32_t uContextID)
{
    AssertReturn(uContextID, false);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIter it = mCallbackMap.find(uContextID);
    return (it == mCallbackMap.end()) ? false : true;
}

void Guest::callbackFreeUserData(void *pvData)
{
    if (pvData)
    {
        RTMemFree(pvData);
        pvData = NULL;
    }
}

int Guest::callbackGetUserData(uint32_t uContextID, eVBoxGuestCtrlCallbackType *pEnmType,
                               void **ppvData, size_t *pcbData)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);
    /* pEnmType is optional. */
    /* ppvData is optional. */
    /* pcbData is optional. */

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIterConst it = mCallbackMap.find(uContextID);
    if (it == mCallbackMap.end())
        return VERR_NOT_FOUND;

    if (pEnmType)
        *pEnmType = it->second.mType;

    if (   ppvData
        && it->second.cbData)
    {
        void *pvData = RTMemAlloc(it->second.cbData);
        AssertPtrReturn(pvData, VERR_NO_MEMORY);
        memcpy(pvData, it->second.pvData, it->second.cbData);
        *ppvData = pvData;
    }

    if (pcbData)
        *pcbData = it->second.cbData;

    return VINF_SUCCESS;
}

/* Does not do locking! Caller has to take care of it because the caller needs to
 * modify the data ...*/
void* Guest::callbackGetUserDataMutableRaw(uint32_t uContextID, size_t *pcbData)
{
    AssertReturn(uContextID, NULL);
    /* pcbData is optional. */

    CallbackMapIterConst it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        if (pcbData)
            *pcbData = it->second.cbData;
        return it->second.pvData;
    }

    return NULL;
}

int Guest::callbackInit(PVBOXGUESTCTRL_CALLBACK pCallback, eVBoxGuestCtrlCallbackType enmType,
                        ComPtr<Progress> pProgress)
{
    AssertPtrReturn(pCallback, VERR_INVALID_POINTER);
    /* Everything else is optional. */

    int vrc = VINF_SUCCESS;
    switch (enmType)
    {
        case VBOXGUESTCTRLCALLBACKTYPE_EXEC_START:
        {
            PCALLBACKDATAEXECSTATUS pData = (PCALLBACKDATAEXECSTATUS)RTMemAlloc(sizeof(CALLBACKDATAEXECSTATUS));
            AssertPtrReturn(pData, VERR_NO_MEMORY);
            RT_BZERO(pData, sizeof(CALLBACKDATAEXECSTATUS));
            pCallback->cbData = sizeof(CALLBACKDATAEXECSTATUS);
            pCallback->pvData = pData;
            break;
        }

        case VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT:
        {
            PCALLBACKDATAEXECOUT pData = (PCALLBACKDATAEXECOUT)RTMemAlloc(sizeof(CALLBACKDATAEXECOUT));
            AssertPtrReturn(pData, VERR_NO_MEMORY);
            RT_BZERO(pData, sizeof(CALLBACKDATAEXECOUT));
            pCallback->cbData = sizeof(CALLBACKDATAEXECOUT);
            pCallback->pvData = pData;
            break;
        }

        case VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS:
        {
            PCALLBACKDATAEXECINSTATUS pData = (PCALLBACKDATAEXECINSTATUS)RTMemAlloc(sizeof(CALLBACKDATAEXECINSTATUS));
            AssertPtrReturn(pData, VERR_NO_MEMORY);
            RT_BZERO(pData, sizeof(CALLBACKDATAEXECINSTATUS));
            pCallback->cbData = sizeof(CALLBACKDATAEXECINSTATUS);
            pCallback->pvData = pData;
            break;
        }

        default:
            vrc = VERR_INVALID_PARAMETER;
            break;
    }

    if (RT_SUCCESS(vrc))
    {
        /* Init/set common stuff. */
        pCallback->mType  = enmType;
        pCallback->pProgress = pProgress;
    }

    return vrc;
}

bool Guest::callbackIsCanceled(uint32_t uContextID)
{
    AssertReturn(uContextID, true);

    ComPtr<IProgress> pProgress;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

    if (pProgress)
    {
        BOOL fCanceled = FALSE;
        HRESULT hRC = pProgress->COMGETTER(Canceled)(&fCanceled);
        if (   SUCCEEDED(hRC)
            && !fCanceled)
        {
            return false;
        }
    }

    return true; /* No progress / error means canceled. */
}

bool Guest::callbackIsComplete(uint32_t uContextID)
{
    AssertReturn(uContextID, true);

    ComPtr<IProgress> pProgress;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

    if (pProgress)
    {
        BOOL fCompleted = FALSE;
        HRESULT hRC = pProgress->COMGETTER(Completed)(&fCompleted);
        if (   SUCCEEDED(hRC)
            && fCompleted)
        {
            return true;
        }
    }

    return false;
}

int Guest::callbackMoveForward(uint32_t uContextID, const char *pszMessage)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszMessage, VERR_INVALID_PARAMETER);

    ComPtr<IProgress> pProgress;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

    if (pProgress)
    {
        HRESULT hr = pProgress->SetNextOperation(Bstr(pszMessage).raw(), 1 /* Weight */);
        if (FAILED(hr))
            return VERR_CANCELLED;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

/**
 * Notifies a specified callback about its final status.
 *
 * @return  IPRT status code.
 * @param   uContextID
 * @param   iRC
 * @param   pszMessage
 */
int Guest::callbackNotifyEx(uint32_t uContextID, int iRC, const char *pszMessage)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);
    if (RT_FAILURE(iRC))
        AssertReturn(pszMessage, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Checking whether callback (CID=%u) needs notification iRC=%Rrc, pszMsg=%s\n",
                 uContextID, iRC, pszMessage ? pszMessage : "<No message given>"));

    ComObjPtr<Progress> pProgress;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
    }

#if 0
    BOOL fCanceled = FALSE;
    HRESULT hRC = pProgress->COMGETTER(Canceled)(&fCanceled);
    if (   SUCCEEDED(hRC)
        && fCanceled)
    {
        /* If progress already canceled do nothing here. */
        return VINF_SUCCESS;
    }
#endif

    if (pProgress)
    {
        /*
         * Assume we didn't complete to make sure we clean up even if the
         * following call fails.
         */
        BOOL fCompleted = FALSE;
        HRESULT hRC = pProgress->COMGETTER(Completed)(&fCompleted);
        if (   SUCCEEDED(hRC)
            && !fCompleted)
        {
            LogFlowFunc(("Notifying callback with CID=%u, iRC=%Rrc, pszMsg=%s\n",
                         uContextID, iRC, pszMessage ? pszMessage : "<No message given>"));

            /*
             * To get waitForCompletion completed (unblocked) we have to notify it if necessary (only
             * cancel won't work!). This could happen if the client thread (e.g. VBoxService, thread of a spawned process)
             * is disconnecting without having the chance to sending a status message before, so we
             * have to abort here to make sure the host never hangs/gets stuck while waiting for the
             * progress object to become signalled.
             */
            if (RT_SUCCESS(iRC))
            {
                hRC = pProgress->notifyComplete(S_OK);
            }
            else
            {

                hRC = pProgress->notifyComplete(VBOX_E_IPRT_ERROR /* Must not be S_OK. */,
                                                COM_IIDOF(IGuest),
                                                Guest::getStaticComponentName(),
                                                pszMessage);
            }

            LogFlowFunc(("Notified callback with CID=%u returned %Rhrc (0x%x)\n",
                         uContextID, hRC, hRC));
        }
        else
            LogFlowFunc(("Callback with CID=%u already notified\n", uContextID));

        /*
         * Do *not* NULL pProgress here, because waiting function like executeProcess()
         * will still rely on this object for checking whether they have to give up!
         */
    }
    /* If pProgress is not found (anymore) that's fine.
     * Might be destroyed already. */
    return S_OK;
}

/**
 * TODO
 *
 * @return  IPRT status code.
 * @param   uPID
 * @param   iRC
 * @param   pszMessage
 */
int Guest::callbackNotifyAllForPID(uint32_t uPID, int iRC, const char *pszMessage)
{
    AssertReturn(uPID, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIter it;
    for (it = mCallbackMap.begin(); it != mCallbackMap.end(); it++)
    {
        switch (it->second.mType)
        {
            case VBOXGUESTCTRLCALLBACKTYPE_EXEC_START:
                break;

            /* When waiting for process output while the process is destroyed,
             * make sure we also destroy the actual waiting operation (internal progress object)
             * in order to not block the caller. */
            case VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT:
            {
                PCALLBACKDATAEXECOUT pItData = (PCALLBACKDATAEXECOUT)it->second.pvData;
                AssertPtr(pItData);
                if (pItData->u32PID == uPID)
                    vrc = callbackNotifyEx(it->first, iRC, pszMessage);
                break;
            }

            /* When waiting for injecting process input while the process is destroyed,
             * make sure we also destroy the actual waiting operation (internal progress object)
             * in order to not block the caller. */
            case VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS:
            {
                PCALLBACKDATAEXECINSTATUS pItData = (PCALLBACKDATAEXECINSTATUS)it->second.pvData;
                AssertPtr(pItData);
                if (pItData->u32PID == uPID)
                    vrc = callbackNotifyEx(it->first, iRC, pszMessage);
                break;
            }

            default:
                vrc = VERR_INVALID_PARAMETER;
                AssertMsgFailed(("Unknown callback type %d, iRC=%d, message=%s\n",
                                 it->second.mType, iRC, pszMessage ? pszMessage : "<No message given>"));
                break;
        }

        if (RT_FAILURE(vrc))
            break;
    }

    return vrc;
}

int Guest::callbackNotifyComplete(uint32_t uContextID)
{
    return callbackNotifyEx(uContextID, S_OK, NULL /* No message */);
}

/**
 * Waits for a callback (using its context ID) to complete.
 *
 * @return  IPRT status code.
 * @param   uContextID              Context ID to wait for.
 * @param   lStage                  Stage to wait for. Specify -1 if no staging is present/required.
 *                                  Specifying a stage is only needed if there's a multi operation progress
 *                                  object to wait for.
 * @param   lTimeout                Timeout (in ms) to wait for.
 */
int Guest::callbackWaitForCompletion(uint32_t uContextID, LONG lStage, LONG lTimeout)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);

    /*
     * Wait for the HGCM low level callback until the process
     * has been started (or something went wrong). This is necessary to
     * get the PID.
     */

    int vrc = VINF_SUCCESS;
    ComPtr<IProgress> pProgress;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        CallbackMapIterConst it = mCallbackMap.find(uContextID);
        if (it != mCallbackMap.end())
            pProgress = it->second.pProgress;
        else
            vrc = VERR_NOT_FOUND;
    }

    if (RT_SUCCESS(vrc))
    {
        LogFlowFunc(("Waiting for callback completion (CID=%u, Stage=%RI32, timeout=%RI32ms) ...\n",
                     uContextID, lStage, lTimeout));
        HRESULT rc;
        if (lStage < 0)
            rc = pProgress->WaitForCompletion(lTimeout);
        else
            rc = pProgress->WaitForOperationCompletion((ULONG)lStage, lTimeout);
        if (SUCCEEDED(rc))
        {
            if (!callbackIsComplete(uContextID))
                vrc = callbackIsCanceled(uContextID)
                    ? VERR_CANCELLED : VINF_SUCCESS;
        }
        else
            vrc = VERR_TIMEOUT;
    }

    LogFlowFunc(("Callback (CID=%u) completed with rc=%Rrc\n",
                 uContextID, vrc));
    return vrc;
}

/**
 * Static callback function for receiving updates on guest control commands
 * from the guest. Acts as a dispatcher for the actual class instance.
 *
 * @returns VBox status code.
 *
 * @todo
 *
 */
DECLCALLBACK(int) Guest::notifyCtrlDispatcher(void    *pvExtension,
                                              uint32_t u32Function,
                                              void    *pvParms,
                                              uint32_t cbParms)
{
    using namespace guestControl;

    /*
     * No locking, as this is purely a notification which does not make any
     * changes to the object state.
     */
#ifdef DEBUG_andy
    LogFlowFunc(("pvExtension=%p, u32Function=%d, pvParms=%p, cbParms=%d\n",
                 pvExtension, u32Function, pvParms, cbParms));
#endif
    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest *>(pvExtension);

    int rc = VINF_SUCCESS;
    switch (u32Function)
    {
        case GUEST_DISCONNECTED:
        {
            //LogFlowFunc(("GUEST_DISCONNECTED\n"));

            PCALLBACKDATACLIENTDISCONNECTED pCBData = reinterpret_cast<PCALLBACKDATACLIENTDISCONNECTED>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATACLIENTDISCONNECTED) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_CLIENT_DISCONNECTED == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlClientDisconnected(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_STATUS:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_STATUS\n"));

            PCALLBACKDATAEXECSTATUS pCBData = reinterpret_cast<PCALLBACKDATAEXECSTATUS>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECSTATUS) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_EXEC_STATUS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecStatus(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_OUTPUT:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_OUTPUT\n"));

            PCALLBACKDATAEXECOUT pCBData = reinterpret_cast<PCALLBACKDATAEXECOUT>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECOUT) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_EXEC_OUT == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecOut(u32Function, pCBData);
            break;
        }

        case GUEST_EXEC_SEND_INPUT_STATUS:
        {
            //LogFlowFunc(("GUEST_EXEC_SEND_INPUT_STATUS\n"));

            PCALLBACKDATAEXECINSTATUS pCBData = reinterpret_cast<PCALLBACKDATAEXECINSTATUS>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(CALLBACKDATAEXECINSTATUS) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CALLBACKDATAMAGIC_EXEC_IN_STATUS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuest->notifyCtrlExecInStatus(u32Function, pCBData);
            break;
        }

        default:
            AssertMsgFailed(("Unknown guest control notification received, u32Function=%u\n", u32Function));
            rc = VERR_INVALID_PARAMETER;
            break;
    }
    return rc;
}

/* Function for handling the execution start/termination notification. */
/* Callback can be called several times. */
int Guest::notifyCtrlExecStatus(uint32_t                u32Function,
                                PCALLBACKDATAEXECSTATUS pData)
{
    AssertReturn(u32Function, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    /* Scope write locks as much as possible. */
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        LogFlowFunc(("Execution status (CID=%u, pData=0x%p)\n",
                     uContextID, pData));

        PCALLBACKDATAEXECSTATUS pCallbackData =
            (PCALLBACKDATAEXECSTATUS)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            pCallbackData->u32PID = pData->u32PID;
            pCallbackData->u32Status = pData->u32Status;
            pCallbackData->u32Flags = pData->u32Flags;
            /** @todo Copy void* buffer contents? */
        }
        /* If pCallbackData is NULL this might be an old request for which no user data
         * might exist anymore. */
    }

    int vrc = VINF_SUCCESS; /* Function result. */
    int rcCallback = VINF_SUCCESS; /* Callback result. */
    Utf8Str errMsg;

    /* Was progress canceled before? */
    bool fCanceled = callbackIsCanceled(uContextID);
    if (!fCanceled)
    {
        /* Handle process map. This needs to be done first in order to have a valid
         * map in case some callback gets notified a bit below. */

        /* Note: PIDs never get removed here in case the guest process signalled its
         *       end; instead the next call of GetProcessStatus() will remove the PID
         *       from the process map after we got the process' final (exit) status.
         *       See waitpid() for an example. */
        if (pData->u32PID) /* Only add/change a process if it has a valid PID (>0). */
        {
            switch (pData->u32Status)
            {
                /* Interprete u32Flags as the guest process' exit code. */
                case PROC_STS_TES:
                case PROC_STS_TOK:
                    vrc = processSetStatus(pData->u32PID,
                                           (ExecuteProcessStatus_T)pData->u32Status,
                                           pData->u32Flags /* Exit code. */, 0 /* Flags. */);
                    break;
                /* Just reach through flags. */
                default:
                    vrc = processSetStatus(pData->u32PID,
                                           (ExecuteProcessStatus_T)pData->u32Status,
                                           0 /* Exit code. */, pData->u32Flags);
                    break;
            }
        }

        /* Do progress handling. */
        switch (pData->u32Status)
        {
            case PROC_STS_STARTED:
                vrc = callbackMoveForward(uContextID, Guest::tr("Waiting for process to exit ..."));
                LogRel(("Guest process (PID %u) started\n", pData->u32PID)); /** @todo Add process name */
                break;

            case PROC_STS_TEN: /* Terminated normally. */
                vrc = callbackNotifyComplete(uContextID);
                LogRel(("Guest process (PID %u) exited normally\n", pData->u32PID)); /** @todo Add process name */
                break;

            case PROC_STS_TEA: /* Terminated abnormally. */
                LogRel(("Guest process (PID %u) terminated abnormally with exit code = %u\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated abnormally with status '%u'"),
                                    pData->u32Flags);
                rcCallback = VERR_GENERAL_FAILURE; /** @todo */
                break;

            case PROC_STS_TES: /* Terminated through signal. */
                LogRel(("Guest process (PID %u) terminated through signal with exit code = %u\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated via signal with status '%u'"),
                                    pData->u32Flags);
                rcCallback = VERR_GENERAL_FAILURE; /** @todo */
                break;

            case PROC_STS_TOK:
                LogRel(("Guest process (PID %u) timed out and was killed\n", pData->u32PID)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and was killed"));
                rcCallback = VERR_TIMEOUT;
                break;

            case PROC_STS_TOA:
                LogRel(("Guest process (PID %u) timed out and could not be killed\n", pData->u32PID)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and could not be killed"));
                rcCallback = VERR_TIMEOUT;
                break;

            case PROC_STS_DWN:
                LogRel(("Guest process (PID %u) killed because system is shutting down\n", pData->u32PID)); /** @todo Add process name */
                /*
                 * If u32Flags has ExecuteProcessFlag_IgnoreOrphanedProcesses set, we don't report an error to
                 * our progress object. This is helpful for waiters which rely on the success of our progress object
                 * even if the executed process was killed because the system/VBoxService is shutting down.
                 *
                 * In this case u32Flags contains the actual execution flags reached in via Guest::ExecuteProcess().
                 */
                if (pData->u32Flags & ExecuteProcessFlag_IgnoreOrphanedProcesses)
                {
                    vrc = callbackNotifyComplete(uContextID);
                }
                else
                {
                    errMsg = Utf8StrFmt(Guest::tr("Process killed because system is shutting down"));
                    rcCallback = VERR_CANCELLED;
                }
                break;

            case PROC_STS_ERROR:
                if (pData->u32PID)
                {
                    LogRel(("Guest process (PID %u) could not be started because of rc=%Rrc\n",
                            pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                }
                else
                {
                    switch (pData->u32Flags)
                    {
                        case VERR_MAX_PROCS_REACHED:
                            LogRel(("Guest process could not be started because maximum number of parallel guest processes has been reached\n"));
                            break;

                        default:
                            LogRel(("Guest process could not be started because of rc=%Rrc\n",
                                    pData->u32Flags));
                    }

                }
                errMsg = Utf8StrFmt(Guest::tr("Process execution failed with rc=%Rrc"), pData->u32Flags);
                rcCallback = pData->u32Flags; /* Report back rc. */
                break;

            default:
                vrc = VERR_INVALID_PARAMETER;
                break;
        }
    }
    else
    {
        errMsg = Utf8StrFmt(Guest::tr("Process execution canceled"));
        rcCallback = VERR_CANCELLED;
    }

    /* Do we need to handle the callback error? */
    if (RT_FAILURE(rcCallback))
    {
        AssertMsg(!errMsg.isEmpty(), ("Error message must not be empty!\n"));

        /* Notify all callbacks which are still waiting on something
         * which is related to the current PID. */
        if (pData->u32PID)
        {
            int rc2 = callbackNotifyAllForPID(pData->u32PID, rcCallback, errMsg.c_str());
            if (RT_FAILURE(rc2))
            {
                LogFlowFunc(("Failed to notify other callbacks for PID=%u\n",
                             pData->u32PID));
                if (RT_SUCCESS(vrc))
                    vrc = rc2;
            }
        }

        /* Let the caller know what went wrong ... */
        int rc2 = callbackNotifyEx(uContextID, rcCallback, errMsg.c_str());
        if (RT_FAILURE(rc2))
        {
            LogFlowFunc(("Failed to notify callback CID=%u for PID=%u\n",
                         uContextID, pData->u32PID));
            if (RT_SUCCESS(vrc))
                vrc = rc2;
        }
        LogFlowFunc(("Process (CID=%u, status=%u) reported: %s\n",
                     uContextID, pData->u32Status, errMsg.c_str()));
    }
    LogFlowFunc(("Returned with rc=%Rrc, rcCallback=%Rrc\n",
                 vrc, rcCallback));
    return vrc;
}

/* Function for handling the execution output notification. */
int Guest::notifyCtrlExecOut(uint32_t             u32Function,
                             PCALLBACKDATAEXECOUT pData)
{
    AssertReturn(u32Function, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    /* Scope write locks as much as possible. */
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        LogFlowFunc(("Output status (CID=%u, pData=0x%p)\n",
                     uContextID, pData));

        PCALLBACKDATAEXECOUT pCallbackData =
            (PCALLBACKDATAEXECOUT)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            pCallbackData->u32PID = pData->u32PID;
            pCallbackData->u32HandleId = pData->u32HandleId;
            pCallbackData->u32Flags = pData->u32Flags;

            /* Make sure we really got something! */
            if (   pData->cbData
                && pData->pvData)
            {
                callbackFreeUserData(pCallbackData->pvData);

                /* Allocate data buffer and copy it */
                pCallbackData->pvData = RTMemAlloc(pData->cbData);
                pCallbackData->cbData = pData->cbData;

                AssertReturn(pCallbackData->pvData, VERR_NO_MEMORY);
                memcpy(pCallbackData->pvData, pData->pvData, pData->cbData);
            }
            else /* Nothing received ... */
            {
                pCallbackData->pvData = NULL;
                pCallbackData->cbData = 0;
            }
        }
        /* If pCallbackData is NULL this might be an old request for which no user data
         * might exist anymore. */
    }

    int vrc;
    if (callbackIsCanceled(pData->u32PID))
    {
        vrc = callbackNotifyEx(uContextID, VERR_CANCELLED,
                               Guest::tr("The output operation was canceled"));
    }
    else
        vrc = callbackNotifyComplete(uContextID);

    return vrc;
}

/* Function for handling the execution input status notification. */
int Guest::notifyCtrlExecInStatus(uint32_t                  u32Function,
                                  PCALLBACKDATAEXECINSTATUS pData)
{
    AssertReturn(u32Function, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    /* Scope write locks as much as possible. */
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        LogFlowFunc(("Input status (CID=%u, pData=0x%p)\n",
                     uContextID, pData));

        PCALLBACKDATAEXECINSTATUS pCallbackData =
            (PCALLBACKDATAEXECINSTATUS)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            /* Save bytes processed. */
            pCallbackData->cbProcessed = pData->cbProcessed;
            pCallbackData->u32Status   = pData->u32Status;
            pCallbackData->u32Flags    = pData->u32Flags;
            pCallbackData->u32PID      = pData->u32PID;
        }
        /* If pCallbackData is NULL this might be an old request for which no user data
         * might exist anymore. */
    }

    return callbackNotifyComplete(uContextID);
}

int Guest::notifyCtrlClientDisconnected(uint32_t                        u32Function,
                                        PCALLBACKDATACLIENTDISCONNECTED pData)
{
    /* u32Function is 0. */
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);

    uint32_t uContextID = pData->hdr.u32ContextID;
    Assert(uContextID);

    LogFlowFunc(("Client disconnected (CID=%u)\n,", uContextID));

    return callbackNotifyEx(uContextID, VERR_CANCELLED,
                            Guest::tr("Client disconnected"));
}

/**
 * Gets guest process information. Removes the process from the map
 * after the process was marked as exited/terminated.
 *
 * @return  IPRT status code.
 * @param   u32PID
 * @param   pProcess
 */
int Guest::processGetStatus(uint32_t u32PID, PVBOXGUESTCTRL_PROCESS pProcess)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIter it = mGuestProcessMap.find(u32PID);
    if (it != mGuestProcessMap.end())
    {
        if (pProcess)
        {
            pProcess->mStatus   = it->second.mStatus;
            pProcess->mExitCode = it->second.mExitCode;
            pProcess->mFlags    = it->second.mFlags;
        }

        /* If the is marked as stopped/terminated
         * remove it from the map. */
        if (it->second.mStatus != ExecuteProcessStatus_Started)
            mGuestProcessMap.erase(it);

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

int Guest::processSetStatus(uint32_t u32PID, ExecuteProcessStatus_T enmStatus, uint32_t uExitCode, uint32_t uFlags)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIter it = mGuestProcessMap.find(u32PID);
    if (it != mGuestProcessMap.end())
    {
        it->second.mStatus = enmStatus;
        it->second.mExitCode = uExitCode;
        it->second.mFlags = uFlags;
    }
    else
    {
        VBOXGUESTCTRL_PROCESS process;

        process.mStatus = enmStatus;
        process.mExitCode = uExitCode;
        process.mFlags = uFlags;

        mGuestProcessMap[u32PID] = process;
    }

    return VINF_SUCCESS;
}

HRESULT Guest::handleErrorCompletion(int rc)
{
    HRESULT hRC;
    if (rc == VERR_NOT_FOUND)
        hRC = setErrorNoLog(VBOX_E_VM_ERROR,
                            tr("VMM device is not available (is the VM running?)"));
    else if (rc == VERR_CANCELLED)
        hRC = setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("Process execution has been canceled"));
    else if (rc == VERR_TIMEOUT)
        hRC= setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("The guest did not respond within time"));
    else
        hRC = setErrorNoLog(E_UNEXPECTED,
                            tr("Waiting for completion failed with error %Rrc"), rc);
    return hRC;
}

HRESULT Guest::handleErrorHGCM(int rc)
{
    HRESULT hRC;
    if (rc == VERR_INVALID_VM_HANDLE)
        hRC = setErrorNoLog(VBOX_E_VM_ERROR,
                            tr("VMM device is not available (is the VM running?)"));
    else if (rc == VERR_NOT_FOUND)
        hRC = setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("The guest execution service is not ready (yet)"));
    else if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        hRC= setErrorNoLog(VBOX_E_IPRT_ERROR,
                            tr("The guest execution service is not available"));
    else /* HGCM call went wrong. */
        hRC = setErrorNoLog(E_UNEXPECTED,
                            tr("The HGCM call failed with error %Rrc"), rc);
    return hRC;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::ExecuteProcess(IN_BSTR aCommand, ULONG aFlags,
                                   ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                   IN_BSTR aUsername, IN_BSTR aPassword,
                                   ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress)
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgStrNotEmptyOrNull(aCommand);
    CheckComArgOutPointerValid(aPID);
    CheckComArgOutPointerValid(aProgress);

    /* Do not allow anonymous executions (with system rights). */
    if (RT_UNLIKELY((aUsername) == NULL || *(aUsername) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));

    LogRel(("Executing guest process \"%s\" as user \"%s\" ...\n",
            Utf8Str(aCommand).c_str(), Utf8Str(aUsername).c_str()));

    return executeProcessInternal(aCommand, aFlags, ComSafeArrayInArg(aArguments),
                                  ComSafeArrayInArg(aEnvironment),
                                  aUsername, aPassword, aTimeoutMS, aPID, aProgress, NULL /* rc */);
#endif
}

#ifdef VBOX_WITH_GUEST_CONTROL
/**
 * Executes and waits for an internal tool (that is, a tool which is integrated into
 * VBoxService, beginning with "vbox_" (e.g. "vbox_ls")) to finish its operation.
 *
 * @return  HRESULT
 * @param   aTool                   Name of tool to execute.
 * @param   aDescription            Friendly description of the operation.
 * @param   aFlags                  Execution flags.
 * @param   aUsername               Username to execute tool under.
 * @param   aPassword               The user's password.
 * @param   uFlagsToAdd             ExecuteProcessFlag flags to add to the execution operation.
 * @param   aProgress               Pointer which receives the tool's progress object. Optional.
 * @param   aPID                    Pointer which receives the tool's PID. Optional.
 */
HRESULT Guest::executeAndWaitForTool(IN_BSTR aTool, IN_BSTR aDescription,
                                     ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                     IN_BSTR aUsername, IN_BSTR aPassword,
                                     ULONG uFlagsToAdd,
                                     GuestCtrlStreamObjects *pObjStdOut, GuestCtrlStreamObjects *pObjStdErr,
                                     IProgress **aProgress, ULONG *aPID)
{
    ComPtr<IProgress> progressTool;
    ULONG uPID;

    ULONG uFlags =   ExecuteProcessFlag_Hidden
                   | ExecuteProcessFlag_WaitForProcessStartOnly;
    if (uFlagsToAdd)
        uFlags |= uFlagsToAdd;

    bool fWaitForOutput = false;
    if (   (   (uFlags & ExecuteProcessFlag_WaitForStdOut)
            && pObjStdOut)
        || (   (uFlags & ExecuteProcessFlag_WaitForStdErr)
            && pObjStdErr))
    {
        fWaitForOutput = true;
    }

    HRESULT rc = ExecuteProcess(aTool,
                                uFlags,
                                ComSafeArrayInArg(aArguments),
                                ComSafeArrayInArg(aEnvironment),
                                aUsername, aPassword,
                                0 /* No timeout. */,
                                &uPID, progressTool.asOutParam());
    if (   SUCCEEDED(rc)
        && fWaitForOutput)
    {
        BOOL fCompleted;
        while (   SUCCEEDED(progressTool->COMGETTER(Completed)(&fCompleted))
               && !fCompleted)
        {
            BOOL fCanceled;
            rc = progressTool->COMGETTER(Canceled)(&fCanceled);
            AssertComRC(rc);
            if (fCanceled)
            {
                rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                   tr("%s was cancelled"), Utf8Str(aDescription).c_str());
                break;
            }

            if (   (uFlags & ExecuteProcessFlag_WaitForStdOut)
                && pObjStdOut)
            {
                rc = executeStreamParse(uPID, ProcessOutputFlag_None /* StdOut */, *pObjStdOut);
            }

            if (   (uFlags & ExecuteProcessFlag_WaitForStdErr)
                && pObjStdErr)
            {
                rc = executeStreamParse(uPID, ProcessOutputFlag_StdErr, *pObjStdErr);
            }

            if (FAILED(rc))
                break;
        }
    }

    if (SUCCEEDED(rc))
    {
        if (aProgress)
        {
            /* Return the progress to the caller. */
            progressTool.queryInterfaceTo(aProgress);
        }

        if (aPID)
            *aPID = uPID;
    }

    return rc;
}

HRESULT Guest::executeProcessResult(const char *pszCommand, const char *pszUser, ULONG ulTimeout,
                                    PCALLBACKDATAEXECSTATUS pExecStatus, ULONG *puPID)
{
    AssertPtrReturn(pExecStatus, E_INVALIDARG);
    AssertPtrReturn(puPID, E_INVALIDARG);

    HRESULT rc = S_OK;

    /* Did we get some status? */
    switch (pExecStatus->u32Status)
    {
        case PROC_STS_STARTED:
            /* Process is (still) running; get PID. */
            *puPID = pExecStatus->u32PID;
            break;

        /* In any other case the process either already
         * terminated or something else went wrong, so no PID ... */
        case PROC_STS_TEN: /* Terminated normally. */
        case PROC_STS_TEA: /* Terminated abnormally. */
        case PROC_STS_TES: /* Terminated through signal. */
        case PROC_STS_TOK:
        case PROC_STS_TOA:
        case PROC_STS_DWN:
            /*
             * Process (already) ended, but we want to get the
             * PID anyway to retrieve the output in a later call.
             */
            *puPID = pExecStatus->u32PID;
            break;

        case PROC_STS_ERROR:
            {
                int vrc = pExecStatus->u32Flags; /* u32Flags member contains IPRT error code. */
                if (vrc == VERR_FILE_NOT_FOUND) /* This is the most likely error. */
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The file '%s' was not found on guest"), pszCommand);
                else if (vrc == VERR_PATH_NOT_FOUND)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The path to file '%s' was not found on guest"), pszCommand);
                else if (vrc == VERR_BAD_EXE_FORMAT)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The file '%s' is not an executable format on guest"), pszCommand);
                else if (vrc == VERR_AUTHENTICATION_FAILURE)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The specified user '%s' was not able to logon on guest"), pszUser);
                else if (vrc == VERR_TIMEOUT)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The guest did not respond within time (%ums)"), ulTimeout);
                else if (vrc == VERR_CANCELLED)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("The execution operation was canceled"));
                else if (vrc == VERR_PERMISSION_DENIED)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("Invalid user/password credentials"));
                else if (vrc == VERR_MAX_PROCS_REACHED)
                    rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                       tr("Concurrent guest process limit is reached"));
                else
                {
                    if (pExecStatus && pExecStatus->u32Status == PROC_STS_ERROR)
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Process could not be started: %Rrc"), pExecStatus->u32Flags);
                    else
                        rc = setErrorNoLog(E_UNEXPECTED,
                                           tr("The service call failed with error %Rrc"), vrc);
                }
            }
            break;

        case PROC_STS_UNDEFINED: /* . */
            rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                               tr("The operation did not complete within time"));
            break;

        default:
            AssertReleaseMsgFailed(("Process (PID %u) reported back an undefined state!\n",
                                    pExecStatus->u32PID));
            rc = E_UNEXPECTED;
            break;
    }

    return rc;
}

/**
 * TODO
 *
 * @return  HRESULT
 * @param   aObjName
 * @param   pStreamBlock
 * @param   pObjInfo
 * @param   enmAddAttribs
 */
int Guest::executeStreamQueryFsObjInfo(IN_BSTR aObjName,
                                       GuestProcessStreamBlock &streamBlock,
                                       PRTFSOBJINFO pObjInfo,
                                       RTFSOBJATTRADD enmAddAttribs)
{
    Utf8Str Utf8ObjName(aObjName);
    int64_t iVal;
    int rc = streamBlock.GetInt64Ex("st_size", &iVal);
    if (RT_SUCCESS(rc))
        pObjInfo->cbObject = iVal;
    /** @todo Add more stuff! */
    return rc;
}

/**
 * Tries to drain the guest's output and fill it into
 * a guest process stream object for later usage.
 *
 * @todo    What's about specifying stderr?
 * @return  IPRT status code.
 * @param   aPID                    PID of process to get the output from.
 * @param   aFlags                  Which stream to drain (stdout or stderr).
 * @param   stream                  Reference to guest process stream to fill.
 */
int Guest::executeStreamDrain(ULONG aPID, ULONG aFlags, GuestProcessStream &stream)
{
    AssertReturn(aPID, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    for (;;)
    {
        SafeArray<BYTE> aData;
        HRESULT hr = getProcessOutputInternal(aPID, aFlags,
                                              0 /* Infinite timeout */,
                                              _64K, ComSafeArrayAsOutParam(aData), &rc);
        if (SUCCEEDED(hr))
        {
            if (aData.size())
            {
                rc = stream.AddData(aData.raw(), aData.size());
                if (RT_UNLIKELY(RT_FAILURE(rc)))
                    break;
            }

            continue; /* Try one more time. */
        }
        else /* No more output and/or error! */
        {
            if (rc == VERR_NOT_FOUND)
                rc = VINF_SUCCESS;
            break;
        }
    }

    return rc;
}

/**
 * Tries to retrieve the next stream block of a given stream and
 * drains the process output only as much as needed to get this next
 * stream block.
 *
 * @return  IPRT status code.
 * @param   ulPID
 * @param   ulFlags
 * @param   stream
 * @param   streamBlock
 */
int Guest::executeStreamGetNextBlock(ULONG ulPID,
                                     ULONG ulFlags,
                                     GuestProcessStream &stream,
                                     GuestProcessStreamBlock &streamBlock)
{
    AssertReturn(!streamBlock.GetCount(), VERR_INVALID_PARAMETER);

    bool fDrainStream = true;
    int rc;
    do
    {
        rc = stream.ParseBlock(streamBlock);
        if (RT_FAILURE(rc)) /* More data needed or empty buffer? */
        {
            if (fDrainStream)
            {
                SafeArray<BYTE> aData;
                HRESULT hr = getProcessOutputInternal(ulPID, ulFlags,
                                                      0 /* Infinite timeout */,
                                                      _64K, ComSafeArrayAsOutParam(aData), &rc);
                if (SUCCEEDED(hr))
                {
                    if (aData.size())
                    {
                        rc = stream.AddData(aData.raw(), aData.size());
                        if (RT_UNLIKELY(RT_FAILURE(rc)))
                            break;
                    }

                    continue; /* Try one more time. */
                }
                else
                {
                    /* No more output to drain from stream. */
                    if (rc == VERR_NOT_FOUND)
                    {
                        fDrainStream = false;
                        continue;
                    }

                    break;
                }
            }
            else
            {
                /* We haved drained the stream as much as we can and reached the
                 * end of our stream buffer -- that means that there simply is no
                 * stream block anymore, which is ok. */
                if (rc == VERR_NO_DATA)
                    rc = VINF_SUCCESS;
                break;
            }
        }
    }
    while (!streamBlock.GetCount());

    return rc;
}

/**
 * Tries to get the next upcoming value block from a started guest process
 * by first draining its output and then processing the received guest stream.
 *
 * @return  IPRT status code.
 * @param   ulPID                   PID of process to get/parse the output from.
 * @param   ulFlags                 ?
 * @param   stream                  Reference to process stream object to use.
 * @param   streamBlock             Reference that receives the next stream block data.
 *
 */
int Guest::executeStreamParseNextBlock(ULONG ulPID,
                                       ULONG ulFlags,
                                       GuestProcessStream &stream,
                                       GuestProcessStreamBlock &streamBlock)
{
    AssertReturn(!streamBlock.GetCount(), VERR_INVALID_PARAMETER);

    int rc;
    do
    {
        rc = stream.ParseBlock(streamBlock);
        if (RT_FAILURE(rc))
            break;
    }
    while (!streamBlock.GetCount());

    return rc;
}

/**
 * Gets output from a formerly started guest process, tries to parse all of its guest
 * stream (as long as data is available) and returns a stream object which can contain
 * multiple stream blocks (which in turn then can contain key=value pairs).
 *
 * @return  HRESULT
 * @param   ulPID                   PID of process to get/parse the output from.
 * @param   ulFlags                 ?
 * @param   streamObjects           Reference to a guest stream object structure for
 *                                  storing the parsed data.
 */
HRESULT Guest::executeStreamParse(ULONG ulPID, ULONG ulFlags, GuestCtrlStreamObjects &streamObjects)
{
    GuestProcessStream stream;
    int rc = executeStreamDrain(ulPID, ulFlags, stream);
    if (RT_SUCCESS(rc))
    {
        do
        {
            /* Try to parse the stream output we gathered until now. If we still need more
             * data the parsing routine will tell us and we just do another poll round. */
            GuestProcessStreamBlock curBlock;
            rc = executeStreamParseNextBlock(ulPID, ulFlags, stream, curBlock);
            if (RT_SUCCESS(rc))
                streamObjects.push_back(curBlock);
        } while (RT_SUCCESS(rc));

        if (rc == VERR_NO_DATA) /* End of data reached. */
            rc = VINF_SUCCESS;
    }

    if (RT_FAILURE(rc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Error while parsing guest output (%Rrc)"), rc);
    return rc;
}

/**
 * Waits for a fomerly started guest process to exit using its progress
 * object and returns its final status. Returns E_ABORT if guest process
 * was canceled.
 *
 * @return  IPRT status code.
 * @param   uPID                    PID of guest process to wait for.
 * @param   pProgress               Progress object to wait for.
 * @param   uTimeoutMS              Timeout (in ms) for waiting; use 0 for
 *                                  an indefinite timeout.
 * @param   pRetStatus              Pointer where to store the final process
 *                                  status. Optional.
 * @param   puRetExitCode           Pointer where to store the final process
 *                                  exit code. Optional.
 */
HRESULT Guest::executeWaitForExit(ULONG uPID, ComPtr<IProgress> pProgress, ULONG uTimeoutMS,
                                  ExecuteProcessStatus_T *pRetStatus, ULONG *puRetExitCode)
{
    HRESULT rc = S_OK;

    BOOL fCanceled = FALSE;
    if (   SUCCEEDED(pProgress->COMGETTER(Canceled(&fCanceled)))
        && fCanceled)
    {
        return E_ABORT;
    }

    BOOL fCompleted = FALSE;
    if (   SUCCEEDED(pProgress->COMGETTER(Completed(&fCompleted)))
        && !fCompleted)
    {
        rc = pProgress->WaitForCompletion(  !uTimeoutMS
                                          ? -1 /* No timeout */
                                          : uTimeoutMS);
        if (FAILED(rc))
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Waiting for guest process to end failed (%Rhrc)"),
                          rc);
    }

    if (SUCCEEDED(rc))
    {
        ULONG uExitCode, uRetFlags;
        ExecuteProcessStatus_T enmStatus;
        HRESULT hRC = GetProcessStatus(uPID, &uExitCode, &uRetFlags, &enmStatus);
        if (FAILED(hRC))
            return hRC;

        if (pRetStatus)
            *pRetStatus = enmStatus;
        if (puRetExitCode)
            *puRetExitCode = uExitCode;
        /** @todo Flags? */
    }

    return rc;
}

/**
 * Does the actual guest process execution, internal function.
 *
 * @return  HRESULT
 * @param   aCommand                Command line to execute.
 * @param   aFlags                  Execution flags.
 * @param   Username                Username to execute the process with.
 * @param   aPassword               The user's password.
 * @param   aTimeoutMS              Timeout (in ms) to wait for the execution operation.
 * @param   aPID                    Pointer that receives the guest process' PID.
 * @param   aProgress               Pointer that receives the guest process' progress object.
 * @param   pRC                     Pointer that receives the internal IPRT return code. Optional.
 */
HRESULT Guest::executeProcessInternal(IN_BSTR aCommand, ULONG aFlags,
                                      ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                      IN_BSTR aUsername, IN_BSTR aPassword,
                                      ULONG aTimeoutMS, ULONG *aPID, IProgress **aProgress, int *pRC)
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
    using namespace guestControl;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != ExecuteProcessFlag_None)
    {
        if (   !(aFlags & ExecuteProcessFlag_IgnoreOrphanedProcesses)
            && !(aFlags & ExecuteProcessFlag_WaitForProcessStartOnly)
            && !(aFlags & ExecuteProcessFlag_Hidden)
            && !(aFlags & ExecuteProcessFlag_NoProfile)
            && !(aFlags & ExecuteProcessFlag_WaitForStdOut)
            && !(aFlags & ExecuteProcessFlag_WaitForStdErr))
        {
            if (pRC)
                *pRC = VERR_INVALID_PARAMETER;
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    HRESULT rc = S_OK;

    try
    {
        /*
         * Create progress object.  Note that this is a multi operation
         * object to perform the following steps:
         * - Operation 1 (0): Create/start process.
         * - Operation 2 (1): Wait for process to exit.
         * If this progress completed successfully (S_OK), the process
         * started and exited normally. In any other case an error/exception
         * occurred.
         */
        ComObjPtr <Progress> pProgress;
        rc = pProgress.createObject();
        if (SUCCEEDED(rc))
        {
            rc = pProgress->init(static_cast<IGuest*>(this),
                                 Bstr(tr("Executing process")).raw(),
                                 TRUE,
                                 2,                                          /* Number of operations. */
                                 Bstr(tr("Starting process ...")).raw());    /* Description of first stage. */
        }
        ComAssertComRC(rc);

        /*
         * Prepare process execution.
         */
        int vrc = VINF_SUCCESS;
        Utf8Str Utf8Command(aCommand);

        /* Adjust timeout. If set to 0, we define
         * an infinite timeout. */
        if (aTimeoutMS == 0)
            aTimeoutMS = UINT32_MAX;

        /* Prepare arguments. */
        char **papszArgv = NULL;
        uint32_t uNumArgs = 0;
        if (aArguments)
        {
            com::SafeArray<IN_BSTR> args(ComSafeArrayInArg(aArguments));
            uNumArgs = args.size();
            papszArgv = (char**)RTMemAlloc(sizeof(char*) * (uNumArgs + 1));
            AssertReturn(papszArgv, E_OUTOFMEMORY);
            for (unsigned i = 0; RT_SUCCESS(vrc) && i < uNumArgs; i++)
                vrc = RTUtf16ToUtf8(args[i], &papszArgv[i]);
            papszArgv[uNumArgs] = NULL;
        }

        Utf8Str Utf8UserName(aUsername);
        Utf8Str Utf8Password(aPassword);
        if (RT_SUCCESS(vrc))
        {
            uint32_t uContextID = 0;

            char *pszArgs = NULL;
            if (uNumArgs > 0)
                vrc = RTGetOptArgvToString(&pszArgs, papszArgv, RTGETOPTARGV_CNV_QUOTE_MS_CRT);
            if (RT_SUCCESS(vrc))
            {
                uint32_t cbArgs = pszArgs ? strlen(pszArgs) + 1 : 0; /* Include terminating zero. */

                /* Prepare environment. */
                void *pvEnv = NULL;
                uint32_t uNumEnv = 0;
                uint32_t cbEnv = 0;
                if (aEnvironment)
                {
                    com::SafeArray<IN_BSTR> env(ComSafeArrayInArg(aEnvironment));

                    for (unsigned i = 0; i < env.size(); i++)
                    {
                        vrc = prepareExecuteEnv(Utf8Str(env[i]).c_str(), &pvEnv, &cbEnv, &uNumEnv);
                        if (RT_FAILURE(vrc))
                            break;
                    }
                }

                if (RT_SUCCESS(vrc))
                {
                    VBOXGUESTCTRL_CALLBACK callback;
                    vrc = callbackInit(&callback, VBOXGUESTCTRLCALLBACKTYPE_EXEC_START, pProgress);
                    if (RT_SUCCESS(vrc))
                    {
                        /* Allocate and assign payload. */
                        callback.cbData = sizeof(CALLBACKDATAEXECSTATUS);
                        PCALLBACKDATAEXECSTATUS pData = (PCALLBACKDATAEXECSTATUS)RTMemAlloc(callback.cbData);
                        AssertReturn(pData, E_OUTOFMEMORY);
                        RT_BZERO(pData, callback.cbData);
                        callback.pvData = pData;
                    }

                    if (RT_SUCCESS(vrc))
                        vrc = callbackAdd(&callback, &uContextID);

                    if (RT_SUCCESS(vrc))
                    {
                        VBOXHGCMSVCPARM paParms[15];
                        int i = 0;
                        paParms[i++].setUInt32(uContextID);
                        paParms[i++].setPointer((void*)Utf8Command.c_str(), (uint32_t)Utf8Command.length() + 1);
                        paParms[i++].setUInt32(aFlags);
                        paParms[i++].setUInt32(uNumArgs);
                        paParms[i++].setPointer((void*)pszArgs, cbArgs);
                        paParms[i++].setUInt32(uNumEnv);
                        paParms[i++].setUInt32(cbEnv);
                        paParms[i++].setPointer((void*)pvEnv, cbEnv);
                        paParms[i++].setPointer((void*)Utf8UserName.c_str(), (uint32_t)Utf8UserName.length() + 1);
                        paParms[i++].setPointer((void*)Utf8Password.c_str(), (uint32_t)Utf8Password.length() + 1);

                        /*
                         * If the WaitForProcessStartOnly flag is set, we only want to define and wait for a timeout
                         * until the process was started - the process itself then gets an infinite timeout for execution.
                         * This is handy when we want to start a process inside a worker thread within a certain timeout
                         * but let the started process perform lengthly operations then.
                         */
                        if (aFlags & ExecuteProcessFlag_WaitForProcessStartOnly)
                            paParms[i++].setUInt32(UINT32_MAX /* Infinite timeout */);
                        else
                            paParms[i++].setUInt32(aTimeoutMS);

                        VMMDev *pVMMDev = NULL;
                        {
                            /* Make sure mParent is valid, so set the read lock while using.
                             * Do not keep this lock while doing the actual call, because in the meanwhile
                             * another thread could request a write lock which would be a bad idea ... */
                            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                            /* Forward the information to the VMM device. */
                            AssertPtr(mParent);
                            pVMMDev = mParent->getVMMDev();
                        }

                        if (pVMMDev)
                        {
                            LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                            vrc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_CMD,
                                                       i, paParms);
                        }
                        else
                            vrc = VERR_INVALID_VM_HANDLE;
                    }
                    RTMemFree(pvEnv);
                }
                RTStrFree(pszArgs);
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback (timeout=%RI32ms) ...\n", aTimeoutMS));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                PCALLBACKDATAEXECSTATUS pExecStatus = NULL;

                /*
                 * Wait for the first stage (=0) to complete (that is starting the process).
                 */
                vrc = callbackWaitForCompletion(uContextID, 0 /* Stage */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecStatus, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        rc = executeProcessResult(Utf8Command.c_str(), Utf8UserName.c_str(), aTimeoutMS,
                                                  pExecStatus, aPID);
                        callbackFreeUserData(pExecStatus);
                    }
                    else
                    {
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Unable to retrieve process execution status data"));
                    }
                }
                else
                    rc = handleErrorCompletion(vrc);

                /*
                 * Do *not* remove the callback yet - we might wait with the IProgress object on something
                 * else (like end of process) ...
                 */
            }
            else
                rc = handleErrorHGCM(vrc);

            for (unsigned i = 0; i < uNumArgs; i++)
                RTMemFree(papszArgv[i]);
            RTMemFree(papszArgv);
        }

        if (SUCCEEDED(rc))
        {
            /* Return the progress to the caller. */
            pProgress.queryInterfaceTo(aProgress);
        }
        else
        {
            if (!pRC) /* Skip logging internal calls. */
                LogRel(("Executing guest process \"%s\" as user \"%s\" failed with %Rrc\n",
                        Utf8Command.c_str(), Utf8UserName.c_str(), vrc));
        }

        if (pRC)
            *pRC = vrc;
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::SetProcessInput(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, ComSafeArrayIn(BYTE, aData), ULONG *aBytesWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgExpr(aPID, aPID > 0);
    CheckComArgOutPointerValid(aBytesWritten);

    /* Validate flags. */
    if (aFlags)
    {
        if (!(aFlags & ProcessInputFlag_EndOfFile))
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetStatus(aPID, &process);
        if (RT_SUCCESS(vrc))
        {
            /* PID exists; check if process is still running. */
            if (process.mStatus != ExecuteProcessStatus_Started)
                rc = setError(VBOX_E_IPRT_ERROR,
                              Guest::tr("Cannot inject input to not running process (PID %u)"), aPID);
        }
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          Guest::tr("Cannot inject input to non-existent process (PID %u)"), aPID);

        if (RT_SUCCESS(vrc))
        {
            uint32_t uContextID = 0;

            /*
             * Create progress object.
             * This progress object, compared to the one in executeProgress() above,
             * is only single-stage local and is used to determine whether the operation
             * finished or got canceled.
             */
            ComObjPtr <Progress> pProgress;
            rc = pProgress.createObject();
            if (SUCCEEDED(rc))
            {
                rc = pProgress->init(static_cast<IGuest*>(this),
                                     Bstr(tr("Setting input for process")).raw(),
                                     TRUE /* Cancelable */);
            }
            if (FAILED(rc)) throw rc;
            ComAssert(!pProgress.isNull());

            /* Adjust timeout. */
            if (aTimeoutMS == 0)
                aTimeoutMS = UINT32_MAX;

            VBOXGUESTCTRL_CALLBACK callback;
            vrc = callbackInit(&callback, VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS, pProgress);
            if (RT_SUCCESS(vrc))
            {
                PCALLBACKDATAEXECINSTATUS pData = (PCALLBACKDATAEXECINSTATUS)callback.pvData;

                /* Save PID + output flags for later use. */
                pData->u32PID = aPID;
                pData->u32Flags = aFlags;
            }

            if (RT_SUCCESS(vrc))
                vrc = callbackAdd(&callback, &uContextID);

            if (RT_SUCCESS(vrc))
            {
                com::SafeArray<BYTE> sfaData(ComSafeArrayInArg(aData));
                uint32_t cbSize = sfaData.size();

                VBOXHGCMSVCPARM paParms[6];
                int i = 0;
                paParms[i++].setUInt32(uContextID);
                paParms[i++].setUInt32(aPID);
                paParms[i++].setUInt32(aFlags);
                paParms[i++].setPointer(sfaData.raw(), cbSize);
                paParms[i++].setUInt32(cbSize);

                {
                    VMMDev *pVMMDev = NULL;
                    {
                        /* Make sure mParent is valid, so set the read lock while using.
                         * Do not keep this lock while doing the actual call, because in the meanwhile
                         * another thread could request a write lock which would be a bad idea ... */
                        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                        /* Forward the information to the VMM device. */
                        AssertPtr(mParent);
                        pVMMDev = mParent->getVMMDev();
                    }

                    if (pVMMDev)
                    {
                        LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                        vrc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_SET_INPUT,
                                                   i, paParms);
                        if (RT_FAILURE(vrc))
                            rc = handleErrorHGCM(vrc);
                    }
                }
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback ...\n"));

                 /*
                 * Wait for getting back the input response from the guest.
                 */
                vrc = callbackWaitForCompletion(uContextID, -1 /* No staging required */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    PCALLBACKDATAEXECINSTATUS pExecStatusIn;
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecStatusIn, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        AssertPtr(pExecStatusIn);
                        switch (pExecStatusIn->u32Status)
                        {
                            case INPUT_STS_WRITTEN:
                                *aBytesWritten = pExecStatusIn->cbProcessed;
                                break;

                            case INPUT_STS_ERROR:
                                rc = setError(VBOX_E_IPRT_ERROR,
                                              tr("Client reported error %Rrc while processing input data"),
                                              pExecStatusIn->u32Flags);
                                break;

                            case INPUT_STS_TERMINATED:
                                rc = setError(VBOX_E_IPRT_ERROR,
                                              tr("Client terminated while processing input data"));
                                break;

                            case INPUT_STS_OVERFLOW:
                                rc = setError(VBOX_E_IPRT_ERROR,
                                              tr("Client reported buffer overflow while processing input data"));
                                break;

                            default:
                                /*AssertReleaseMsgFailed(("Client reported unknown input error, status=%u, flags=%u\n",
                                                        pExecStatusIn->u32Status, pExecStatusIn->u32Flags));*/
                                break;
                        }

                        callbackFreeUserData(pExecStatusIn);
                    }
                    else
                    {
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Unable to retrieve process input status data"));
                    }
                }
                else
                    rc = handleErrorCompletion(vrc);
            }

            /* The callback isn't needed anymore -- just was kept locally. */
            callbackDestroy(uContextID);

            /* Cleanup. */
            if (!pProgress.isNull())
                pProgress->uninit();
            pProgress.setNull();
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::GetProcessOutput(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS, LONG64 aSize, ComSafeArrayOut(BYTE, aData))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    return getProcessOutputInternal(aPID, aFlags, aTimeoutMS,
                                    aSize, ComSafeArrayOutArg(aData), NULL /* rc */);
#endif
}

HRESULT Guest::getProcessOutputInternal(ULONG aPID, ULONG aFlags, ULONG aTimeoutMS,
                                        LONG64 aSize, ComSafeArrayOut(BYTE, aData), int *pRC)
{
/** @todo r=bird: Eventually we should clean up all the timeout parameters
 *        in the API and have the same way of specifying infinite waits!  */
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */
    using namespace guestControl;

    CheckComArgExpr(aPID, aPID > 0);
    if (aSize < 0)
        return setError(E_INVALIDARG, tr("The size argument (%lld) is negative"), aSize);
    if (aSize == 0)
        return setError(E_INVALIDARG, tr("The size (%lld) is zero"), aSize);
    if (aFlags)
    {
        if (!(aFlags & ProcessOutputFlag_StdErr))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS proc;
        int vrc = processGetStatus(aPID, &proc);
        if (RT_FAILURE(vrc))
        {
            rc = setError(VBOX_E_IPRT_ERROR,
                          Guest::tr("Guest process (PID %u) does not exist"), aPID);
        }
        else
        {
            uint32_t uContextID = 0;

            /*
             * Create progress object.
             * This progress object, compared to the one in executeProgress() above,
             * is only single-stage local and is used to determine whether the operation
             * finished or got canceled.
             */
            ComObjPtr <Progress> pProgress;
            rc = pProgress.createObject();
            if (SUCCEEDED(rc))
            {
                rc = pProgress->init(static_cast<IGuest*>(this),
                                     Bstr(tr("Getting output for guest process")).raw(),
                                     TRUE /* Cancelable */);
            }
            if (FAILED(rc)) throw rc;
            ComAssert(!pProgress.isNull());

            /* Adjust timeout. */
            if (aTimeoutMS == 0)
                aTimeoutMS = UINT32_MAX;

            /* Set handle ID. */
            uint32_t uHandleID = OUTPUT_HANDLE_ID_STDOUT; /* Default */
            if (aFlags & ProcessOutputFlag_StdErr)
                uHandleID = OUTPUT_HANDLE_ID_STDERR;

            VBOXGUESTCTRL_CALLBACK callback;
            vrc = callbackInit(&callback, VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT, pProgress);
            if (RT_SUCCESS(vrc))
            {
                PCALLBACKDATAEXECOUT pData = (PCALLBACKDATAEXECOUT)callback.pvData;

                /* Save PID + output flags for later use. */
                pData->u32PID = aPID;
                pData->u32Flags = aFlags;
            }

            if (RT_SUCCESS(vrc))
                vrc = callbackAdd(&callback, &uContextID);

            if (RT_SUCCESS(vrc))
            {
                VBOXHGCMSVCPARM paParms[5];
                int i = 0;
                paParms[i++].setUInt32(uContextID);
                paParms[i++].setUInt32(aPID);
                paParms[i++].setUInt32(uHandleID);
                paParms[i++].setUInt32(0 /* Flags, none set yet */);

                VMMDev *pVMMDev = NULL;
                {
                    /* Make sure mParent is valid, so set the read lock while using.
                     * Do not keep this lock while doing the actual call, because in the meanwhile
                     * another thread could request a write lock which would be a bad idea ... */
                    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                    /* Forward the information to the VMM device. */
                    AssertPtr(mParent);
                    pVMMDev = mParent->getVMMDev();
                }

                if (pVMMDev)
                {
                    LogFlowFunc(("hgcmHostCall numParms=%d\n", i));
                    vrc = pVMMDev->hgcmHostCall("VBoxGuestControlSvc", HOST_EXEC_GET_OUTPUT,
                                               i, paParms);
                }
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback (timeout=%RI32ms) ...\n", aTimeoutMS));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                /*
                 * Wait for the first output callback notification to arrive.
                 */
                vrc = callbackWaitForCompletion(uContextID, -1 /* No staging required */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    PCALLBACKDATAEXECOUT pExecOut = NULL;
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecOut, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        com::SafeArray<BYTE> outputData((size_t)aSize);

                        if (pExecOut->cbData)
                        {
                            /* Do we need to resize the array? */
                            if (pExecOut->cbData > aSize)
                                outputData.resize(pExecOut->cbData);

                            /* Fill output in supplied out buffer. */
                            memcpy(outputData.raw(), pExecOut->pvData, pExecOut->cbData);
                            outputData.resize(pExecOut->cbData); /* Shrink to fit actual buffer size. */
                        }
                        else
                        {
                            /* No data within specified timeout available. */
                            outputData.resize(0);
                        }

                        /* Detach output buffer to output argument. */
                        outputData.detachTo(ComSafeArrayOutArg(aData));

                        callbackFreeUserData(pExecOut);
                    }
                    else
                    {
                        rc = setErrorNoLog(VBOX_E_IPRT_ERROR,
                                           tr("Unable to retrieve process output data (%Rrc)"), vrc);
                    }
                }
                else
                    rc = handleErrorCompletion(vrc);
            }
            else
                rc = handleErrorHGCM(vrc);

            /* The callback isn't needed anymore -- just was kept locally. */
            callbackDestroy(uContextID);

            /* Cleanup. */
            if (!pProgress.isNull())
                pProgress->uninit();
            pProgress.setNull();
        }

        if (pRC)
            *pRC = vrc;
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::GetProcessStatus(ULONG aPID, ULONG *aExitCode, ULONG *aFlags, ExecuteProcessStatus_T *aStatus)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else  /* VBOX_WITH_GUEST_CONTROL */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetStatus(aPID, &process);
        if (RT_SUCCESS(vrc))
        {
            if (aExitCode)
                *aExitCode = process.mExitCode;
            if (aFlags)
                *aFlags = process.mFlags;
            if (aStatus)
                *aStatus = process.mStatus;
        }
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Process (PID %u) not found!"), aPID);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }
    return rc;
#endif
}

STDMETHODIMP Guest::CopyFromGuest(IN_BSTR aSource, IN_BSTR aDest,
                                  IN_BSTR aUsername, IN_BSTR aPassword,
                                  ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgStrNotEmptyOrNull(aUsername);
    CheckComArgStrNotEmptyOrNull(aPassword);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != CopyFileFlag_None)
    {
        if (   !(aFlags & CopyFileFlag_Recursive)
            && !(aFlags & CopyFileFlag_Update)
            && !(aFlags & CopyFileFlag_FollowLinks))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        /* Create the progress object. */
        progress.createObject();

        rc = progress->init(static_cast<IGuest*>(this),
                            Bstr(tr("Copying file from guest to host")).raw(),
                            TRUE /* aCancelable */);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task. */
        GuestTask *pTask = new GuestTask(GuestTask::TaskType_CopyFileFromGuest, this, progress);
        AssertPtr(pTask);
        std::auto_ptr<GuestTask> task(pTask);

        /* Assign data - aSource is the source file on the host,
         * aDest reflects the full path on the guest. */
        task->strSource   = (Utf8Str(aSource));
        task->strDest     = (Utf8Str(aDest));
        task->strUserName = (Utf8Str(aUsername));
        task->strPassword = (Utf8Str(aPassword));
        task->uFlags      = aFlags;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
    {
        /* Return progress to the caller. */
        progress.queryInterfaceTo(aProgress);
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::CopyToGuest(IN_BSTR aSource, IN_BSTR aDest,
                                IN_BSTR aUsername, IN_BSTR aPassword,
                                ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgStrNotEmptyOrNull(aUsername);
    CheckComArgStrNotEmptyOrNull(aPassword);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags != CopyFileFlag_None)
    {
        if (   !(aFlags & CopyFileFlag_Recursive)
            && !(aFlags & CopyFileFlag_Update)
            && !(aFlags & CopyFileFlag_FollowLinks))
        {
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
        }
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        /* Create the progress object. */
        progress.createObject();

        rc = progress->init(static_cast<IGuest*>(this),
                            Bstr(tr("Copying file from host to guest")).raw(),
                            TRUE /* aCancelable */);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task. */
        GuestTask *pTask = new GuestTask(GuestTask::TaskType_CopyFileToGuest, this, progress);
        AssertPtr(pTask);
        std::auto_ptr<GuestTask> task(pTask);

        /* Assign data - aSource is the source file on the host,
         * aDest reflects the full path on the guest. */
        task->strSource   = (Utf8Str(aSource));
        task->strDest     = (Utf8Str(aDest));
        task->strUserName = (Utf8Str(aUsername));
        task->strPassword = (Utf8Str(aPassword));
        task->uFlags      = aFlags;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
    {
        /* Return progress to the caller. */
        progress.queryInterfaceTo(aProgress);
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::UpdateGuestAdditions(IN_BSTR aSource, ULONG aFlags, IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    if (aFlags)
    {
        if (!(aFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly))
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        /* Create the progress object. */
        progress.createObject();

        rc = progress->init(static_cast<IGuest*>(this),
                            Bstr(tr("Updating Guest Additions")).raw(),
                            TRUE /* aCancelable */);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task. */
        GuestTask *pTask = new GuestTask(GuestTask::TaskType_UpdateGuestAdditions, this, progress);
        AssertPtr(pTask);
        std::auto_ptr<GuestTask> task(pTask);

        /* Assign data - in that case aSource is the full path
         * to the Guest Additions .ISO we want to mount. */
        task->strSource = (Utf8Str(aSource));
        task->uFlags = aFlags;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success. */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
    {
        /* Return progress to the caller. */
        progress.queryInterfaceTo(aProgress);
    }
    return rc;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

