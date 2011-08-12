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

    LogFlowFunc(("Destroying callback with CID=%u ...\n", uContextID));

    /* Notify callback (if necessary). */
    int rc = callbackNotifyEx(uContextID, VERR_CANCELLED,
                              Guest::tr("VM is shutting down, canceling uncompleted guest requests ..."));
    AssertRC(rc);

    CallbackMapIter it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        if (it->second.pvData)
        {
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
    AssertPtrReturn(ppvData, VERR_INVALID_PARAMETER);
    /* pcbData is optional. */

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CallbackMapIterConst it = mCallbackMap.find(uContextID);
    if (it != mCallbackMap.end())
    {
        if (pEnmType)
            *pEnmType = it->second.mType;

        void *pvData = RTMemAlloc(it->second.cbData);
        AssertPtrReturn(pvData, VERR_NO_MEMORY);
        memcpy(pvData, it->second.pvData, it->second.cbData);
        *ppvData = pvData;

        if (pcbData)
            *pcbData = it->second.cbData;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
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

    int rc = VINF_SUCCESS;
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
            RT_BZERO(pData, sizeof(PCALLBACKDATAEXECINSTATUS));
            pCallback->cbData = sizeof(PCALLBACKDATAEXECINSTATUS);
            pCallback->pvData = pData;
            break;
        }

        default:
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        /* Init/set common stuff. */
        pCallback->mType  = enmType;
        pCallback->pProgress = pProgress;
    }

    return rc;
}

bool Guest::callbackIsCanceled(uint32_t uContextID)
{
    AssertReturn(uContextID, true);

    Progress *pProgress = NULL;
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

    Progress *pProgress = NULL;
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

    Progress *pProgress = NULL;
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
 * TODO
 *
 * @return  IPRT status code.
 * @param   uContextID
 * @param   iRC
 * @param   pszMessage
 */
int Guest::callbackNotifyEx(uint32_t uContextID, int iRC, const char *pszMessage)
{
    AssertReturn(uContextID, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Notifying callback with CID=%u, iRC=%d, pszMsg=%s ...\n",
                 uContextID, iRC, pszMessage ? pszMessage : "<No message given>"));

    Progress *pProgress = NULL;
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
            /*
             * To get waitForCompletion completed (unblocked) we have to notify it if necessary (only
             * cancel won't work!). This could happen if the client thread (e.g. VBoxService, thread of a spawned process)
             * is disconnecting without having the chance to sending a status message before, so we
             * have to abort here to make sure the host never hangs/gets stuck while waiting for the
             * progress object to become signalled.
             */
            if (   RT_SUCCESS(iRC)
                && !pszMessage)
            {
                hRC = pProgress->notifyComplete(S_OK);
            }
            else
            {
                AssertPtrReturn(pszMessage, VERR_INVALID_PARAMETER);
                hRC = pProgress->notifyComplete(VBOX_E_IPRT_ERROR /* Must not be S_OK. */,
                                                COM_IIDOF(IGuest),
                                                Guest::getStaticComponentName(),
                                                pszMessage);
            }
        }
        ComAssertComRC(hRC);

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
    Progress *pProgress = NULL;
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
    LogFlowFunc(("pvExtension = %p, u32Function = %d, pvParms = %p, cbParms = %d\n",
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

        PCALLBACKDATAEXECSTATUS pCallbackData =
            (PCALLBACKDATAEXECSTATUS)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            pCallbackData->u32PID = pData->u32PID;
            pCallbackData->u32Status = pData->u32Status;
            pCallbackData->u32Flags = pData->u32Flags;
            /** @todo Copy void* buffer contents? */
        }
        else
            AssertReleaseMsgFailed(("Process status (PID=%u, CID=%u) does not have allocated callback data!\n",
                                    pData->u32PID, uContextID));
    }

    int vrc = VINF_SUCCESS;
    Utf8Str errMsg;

    /* Was progress canceled before? */
    bool fCbCanceled = callbackIsCanceled(uContextID);
    if (!fCbCanceled)
    {
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
                break;

            case PROC_STS_TES: /* Terminated through signal. */
                LogRel(("Guest process (PID %u) terminated through signal with exit code = %u\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process terminated via signal with status '%u'"),
                                    pData->u32Flags);
                break;

            case PROC_STS_TOK:
                LogRel(("Guest process (PID %u) timed out and was killed\n", pData->u32PID)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and was killed"));
                break;

            case PROC_STS_TOA:
                LogRel(("Guest process (PID %u) timed out and could not be killed\n", pData->u32PID)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process timed out and could not be killed"));
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
                    errMsg = Utf8StrFmt(Guest::tr("Process killed because system is shutting down"));
                break;

            case PROC_STS_ERROR:
                LogRel(("Guest process (PID %u) could not be started because of rc=%Rrc\n",
                        pData->u32PID, pData->u32Flags)); /** @todo Add process name */
                errMsg = Utf8StrFmt(Guest::tr("Process execution failed with rc=%Rrc"), pData->u32Flags);
                break;

            default:
                vrc = VERR_INVALID_PARAMETER;
                break;
        }

        /* Handle process map. */
        /** @todo What happens on/deal with PID reuse? */
        /** @todo How to deal with multiple updates at once? */
        if (pData->u32PID)
        {
            VBOXGUESTCTRL_PROCESS process;
            vrc = processGetByPID(pData->u32PID, &process);
            if (vrc == VERR_NOT_FOUND)
            {
                /* Not found, add to map. */
                vrc = processAdd(pData->u32PID,
                                 (ExecuteProcessStatus_T)pData->u32Status,
                                 pData->u32Flags /* Contains exit code. */,
                                 0 /*Flags. */);
                AssertRC(vrc);
            }
            else if (RT_SUCCESS(vrc))
            {
                /* Process found, update process map. */
                vrc = processSetStatus(pData->u32PID,
                                       (ExecuteProcessStatus_T)pData->u32Status,
                                       pData->u32Flags /* Contains exit code. */,
                                       0 /*Flags. */);
                AssertRC(vrc);
            }
            else
                AssertReleaseMsgFailed(("Process was neither found nor absent!?\n"));
        }
    }
    else
        errMsg = Utf8StrFmt(Guest::tr("Process execution canceled"));

    if (!callbackIsComplete(uContextID))
    {
        if (   errMsg.length()
            || fCbCanceled) /* If canceled we have to report E_FAIL! */
        {
            /* Notify all callbacks which are still waiting on something
             * which is related to the current PID. */
            if (pData->u32PID)
            {
                vrc = callbackNotifyAllForPID(pData->u32PID, VERR_GENERAL_FAILURE, errMsg.c_str());
                if (RT_FAILURE(vrc))
                    LogFlowFunc(("Failed to notify other callbacks for PID=%u\n",
                                 pData->u32PID));
            }

            /* Let the caller know what went wrong ... */
            int rc2 = callbackNotifyEx(uContextID, VERR_GENERAL_FAILURE, errMsg.c_str());
            if (RT_FAILURE(rc2))
            {
                LogFlowFunc(("Failed to notify callback CID=%u for PID=%u\n",
                             uContextID, pData->u32PID));

                if (RT_SUCCESS(vrc))
                    vrc = rc2;
            }
            LogFlowFunc(("Process (CID=%u, status=%u) reported error: %s\n",
                         uContextID, pData->u32Status, errMsg.c_str()));
        }
    }
    LogFlowFunc(("Returned with rc=%Rrc\n", vrc));
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
        else
            AssertReleaseMsgFailed(("Process output status (PID=%u, CID=%u) does not have allocated callback data!\n",
                                    pData->u32PID, uContextID));
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

        PCALLBACKDATAEXECINSTATUS pCallbackData =
            (PCALLBACKDATAEXECINSTATUS)callbackGetUserDataMutableRaw(uContextID, NULL /* cbData */);
        if (pCallbackData)
        {
            /* Save bytes processed. */
            pCallbackData->cbProcessed = pData->cbProcessed;
            pCallbackData->u32Status = pData->u32Status;
            pCallbackData->u32Flags = pData->u32Flags;
            pCallbackData->u32PID = pData->u32PID;
        }
        else
            AssertReleaseMsgFailed(("Process input status (PID=%u, CID=%u) does not have allocated callback data!\n",
                                    pData->u32PID, uContextID));
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

    return callbackNotifyEx(uContextID, S_OK,
                            Guest::tr("Client disconnected"));
}

int Guest::processAdd(uint32_t u32PID, ExecuteProcessStatus_T enmStatus,
                      uint32_t uExitCode, uint32_t uFlags)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIterConst it = mGuestProcessMap.find(u32PID);
    if (it == mGuestProcessMap.end())
    {
        VBOXGUESTCTRL_PROCESS process;

        process.mStatus = enmStatus;
        process.mExitCode = uExitCode;
        process.mFlags = uFlags;

        mGuestProcessMap[u32PID] = process;

        return VINF_SUCCESS;
    }

    return VERR_ALREADY_EXISTS;
}

int Guest::processGetByPID(uint32_t u32PID, PVBOXGUESTCTRL_PROCESS pProcess)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pProcess, VERR_INVALID_PARAMETER);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIterConst it = mGuestProcessMap.find(u32PID);
    if (it != mGuestProcessMap.end())
    {
        pProcess->mStatus = it->second.mStatus;
        pProcess->mExitCode = it->second.mExitCode;
        pProcess->mFlags = it->second.mFlags;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}

int Guest::processSetStatus(uint32_t u32PID, ExecuteProcessStatus_T enmStatus, uint32_t uExitCode, uint32_t uFlags)
{
    AssertReturn(u32PID, VERR_INVALID_PARAMETER);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestProcessMapIter it = mGuestProcessMap.find(u32PID);
    if (it != mGuestProcessMap.end())
    {
        it->second.mStatus = enmStatus;
        it->second.mExitCode = uExitCode;
        it->second.mFlags = uFlags;

        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
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
 * @param   aProgress               Pointer which receives the tool's progress object. Optional.
 * @param   aPID                    Pointer which receives the tool's PID. Optional.
 */
HRESULT Guest::executeAndWaitForTool(IN_BSTR aTool, IN_BSTR aDescription,
                                     ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                     IN_BSTR aUsername, IN_BSTR aPassword,
                                     IProgress **aProgress, ULONG *aPID)
{
    ComPtr<IProgress> progressTool;
    ULONG uPID;

    HRESULT rc = ExecuteProcess(aTool,
                                ExecuteProcessFlag_Hidden,
                                ComSafeArrayInArg(aArguments),
                                ComSafeArrayInArg(aEnvironment),
                                aUsername, aPassword,
                                5 * 1000 /* Wait 5s for getting the process started. */,
                                &uPID, progressTool.asOutParam());
    if (SUCCEEDED(rc))
    {
        /* Wait for process to exit ... */
        rc = progressTool->WaitForCompletion(-1);
        if (FAILED(rc)) return rc;

        BOOL fCompleted = FALSE;
        BOOL fCanceled = FALSE;
        progressTool->COMGETTER(Completed)(&fCompleted);
        if (!fCompleted)
            progressTool->COMGETTER(Canceled)(&fCanceled);

        if (fCompleted)
        {
            ExecuteProcessStatus_T retStatus;
            ULONG uRetExitCode, uRetFlags;
            if (SUCCEEDED(rc))
            {
                rc = GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                if (SUCCEEDED(rc))
                {
                    if (uRetExitCode != 0) /* Not equal 0 means some error occured. */
                    {
                        /** @todo IPRT exit code to string! */
                        rc = setError(VBOX_E_IPRT_ERROR,
                                      tr("%s: Error %u occured"),
                                      Utf8Str(aDescription).c_str(), uRetExitCode);
                    }
                    else /* Return code 0, success. */
                    {
                        if (aProgress)
                        {
                            /* Return the progress to the caller. */
                            progressTool.queryInterfaceTo(aProgress);
                        }
                        if (aPID)
                            *aPID = uPID;
                    }
                }
            }
        }
        else if (fCanceled)
        {
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("%s was aborted"), aDescription);
        }
        else
            AssertReleaseMsgFailed(("%s: Operation neither completed nor canceled!?\n",
                                    Utf8Str(aDescription).c_str()));
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
HRESULT Guest::executeStreamQueryFsObjInfo(IN_BSTR aObjName,
                                           GuestProcessStreamBlock &streamBlock,
                                           PRTFSOBJINFO pObjInfo,
                                           RTFSOBJATTRADD enmAddAttribs)
{
    HRESULT rc = S_OK;
    Utf8Str Utf8ObjName(aObjName);

    int64_t iVal;
    int vrc = streamBlock.GetInt64Ex("st_size", &iVal);
    if (RT_SUCCESS(vrc))
        pObjInfo->cbObject = iVal;
    else
        rc = setError(VBOX_E_IPRT_ERROR,
                      tr("Unable to retrieve size for \"%s\" (%Rrc)"),
                      Utf8ObjName.c_str(), vrc);
    /** @todo Add more stuff! */
    return rc;
}

/**
 * Tries to drain the guest's output (from stdout) and fill it into
 * a guest process stream object for later usage.
 *
 * @return  IPRT status code.
 * @param   aPID                    PID of process to get the output from.
 * @param   stream                  Reference to guest process stream to fill.
 */
int Guest::executeStreamDrain(ULONG aPID, GuestProcessStream &stream)
{
    AssertReturn(aPID, VERR_INVALID_PARAMETER);

    /** @todo Should we try to drain the stream harder? */

    int rc = VINF_SUCCESS;
    for (;;)
    {
        SafeArray<BYTE> aOutputData;
        HRESULT hr = GetProcessOutput(aPID, ProcessOutputFlag_None /* Stdout */,
                                      10 * 1000 /* Timeout in ms */,
                                      _64K, ComSafeArrayAsOutParam(aOutputData));
        if (   SUCCEEDED(hr)
            && aOutputData.size())
        {
            rc = stream.AddData(aOutputData.raw(), aOutputData.size());
            if (RT_UNLIKELY(RT_FAILURE(rc)))
                break;
        }
        else /* No more output and/or error! */
            break;
    }

    return rc;
}

/**
 * Tries to get the next upcoming value block from a started guest process
 * by first draining its output and then processing the received guest stream.
 *
 * @return  IPRT status code.
 * @param   aPID                    PID of process to get/parse the output from.
 * @param   stream                  Reference to process stream object to use.
 * @param   streamBlock             Reference that receives the next stream block data.
 *
 */
int Guest::executeStreamGetNextBlock(ULONG aPID, GuestProcessStream &stream,
                                     GuestProcessStreamBlock &streamBlock)
{
    int rc = executeStreamDrain(aPID, stream);
    if (RT_SUCCESS(rc))
    {
        do
        {
            rc = stream.ParseBlock(streamBlock);
            if (streamBlock.GetCount())
                break; /* We got a block, bail out! */
        } while (RT_SUCCESS(rc));

        /* In case this was the last block, VERR_NO_DATA is returned.
         * Overwrite this to get a proper return value for the last block. */
        if(    streamBlock.GetCount()
            && rc == VERR_NO_DATA)
        {
            rc = VINF_SUCCESS;
        }
    }

    return rc;
}

/**
 * Gets output from a formerly started guest process, tries to parse all of its guest
 * stream (as long as data is available) and returns a stream object which can contain
 * multiple stream blocks (which in turn then can contain key=value pairs).
 *
 * @return  HRESULT
 * @param   aPID                    PID of process to get/parse the output from.
 * @param   streamObjects           Reference to a guest stream object structure for
 *                                  storing the parsed data.
 */
HRESULT Guest::executeStreamParse(ULONG aPID, GuestCtrlStreamObjects &streamObjects)
{
    GuestProcessStream guestStream;
    HRESULT hr = executeStreamDrain(aPID, guestStream);
    if (SUCCEEDED(hr))
    {
        for (;;)
        {
            /* Try to parse the stream output we gathered until now. If we still need more
             * data the parsing routine will tell us and we just do another poll round. */
            GuestProcessStreamBlock curBlock;
            int vrc = guestStream.ParseBlock(curBlock);
            if (RT_SUCCESS(vrc))
            {
                if (curBlock.GetCount())
                {
                    streamObjects.push_back(curBlock);
                }
                else
                    break; /* No more data. */
            }
            else /* Everything else would be an error! */
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Error while parsing guest output (%Rrc)"), vrc);
        }
    }

    /** @todo Add check if there now are any sream objects at all! */

    return hr;
}

/**
 * Does busy waiting on a formerly started guest process.
 *
 * @return  HRESULT
 * @param   uPID                    PID of guest process to wait for.
 * @param   uTimeoutMS              Waiting timeout (in ms). Specify 0 for an infinite timeout.
 * @param   pRetStatus              Pointer which receives current process status after the change.
 *                                  Optional.
 * @param   puRetExitCode           Pointer which receives the final exit code in case of guest process
 *                                  termination. Optional.
 */
HRESULT Guest::executeWaitForStatusChange(ULONG uPID, ULONG uTimeoutMS,
                                          ExecuteProcessStatus_T *pRetStatus, ULONG *puRetExitCode)
{
    if (uTimeoutMS == 0)
        uTimeoutMS = UINT32_MAX;

    uint64_t u64StartMS = RTTimeMilliTS();

    HRESULT hRC;
    ULONG uExitCode, uRetFlags;
    ExecuteProcessStatus_T curStatus;
    hRC = GetProcessStatus(uPID, &uExitCode, &uRetFlags, &curStatus);
    if (FAILED(hRC))
        return hRC;

    do
    {
        if (   uTimeoutMS != UINT32_MAX
            && RTTimeMilliTS() - u64StartMS > uTimeoutMS)
        {
            hRC = setError(VBOX_E_IPRT_ERROR,
                           tr("The process (PID %u) did not change its status within time (%ums)"),
                           uPID, uTimeoutMS);
            break;
        }
        hRC = GetProcessStatus(uPID, &uExitCode, &uRetFlags, &curStatus);
        if (FAILED(hRC))
            break;
        RTThreadSleep(100);
    } while(*pRetStatus == curStatus);

    if (SUCCEEDED(hRC))
    {
        if (pRetStatus)
            *pRetStatus = curStatus;
        if (puRetExitCode)
            *puRetExitCode = uExitCode;
    }
    return hRC;
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
    if (aFlags !=  ExecuteProcessFlag_None)
    {
        if (   !(aFlags & ExecuteProcessFlag_IgnoreOrphanedProcesses)
            && !(aFlags & ExecuteProcessFlag_WaitForProcessStartOnly)
            && !(aFlags & ExecuteProcessFlag_Hidden)
            && !(aFlags & ExecuteProcessFlag_NoProfile))
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
                LogFlowFunc(("Waiting for HGCM callback (timeout=%dms) ...\n", aTimeoutMS));

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
        int vrc = processGetByPID(aPID, &process);
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

        if (SUCCEEDED(rc))
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
                    }
                }
            }

            if (RT_SUCCESS(vrc))
            {
                LogFlowFunc(("Waiting for HGCM callback ...\n"));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                PCALLBACKDATAEXECINSTATUS pExecStatusIn = NULL;

                 /*
                 * Wait for the first stage (=0) to complete (that is starting the process).
                 */
                vrc = callbackWaitForCompletion(uContextID, 0 /* Stage */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
                    vrc = callbackGetUserData(uContextID, NULL /* We know the type. */,
                                              (void**)&pExecStatusIn, NULL /* Don't need the size. */);
                    if (RT_SUCCESS(vrc))
                    {
                        switch (pExecStatusIn->u32Status)
                        {
                            case INPUT_STS_WRITTEN:
                                *aBytesWritten = pExecStatusIn->cbProcessed;
                                break;

                            default:
                                rc = setError(VBOX_E_IPRT_ERROR,
                                              tr("Client error %u while processing input data"), pExecStatusIn->u32Status);
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
            else
                rc = handleErrorHGCM(vrc);

            if (SUCCEEDED(rc))
            {
                /* Nothing to do here yet. */
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
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetByPID(aPID, &process);
        if (RT_FAILURE(vrc))
            rc = setError(VBOX_E_IPRT_ERROR,
                          Guest::tr("Cannot get output from non-existent guest process (PID %u)"), aPID);

        if (SUCCEEDED(rc))
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
                LogFlowFunc(("Waiting for HGCM callback (timeout=%dms) ...\n", aTimeoutMS));

                /*
                 * Wait for the HGCM low level callback until the process
                 * has been started (or something went wrong). This is necessary to
                 * get the PID.
                 */

                PCALLBACKDATAEXECOUT pExecOut = NULL;

                /*
                 * Wait for the first output callback notification to arrive.
                 */
                vrc = callbackWaitForCompletion(uContextID, -1 /* No staging */, aTimeoutMS);
                if (RT_SUCCESS(vrc))
                {
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
                                           tr("Unable to retrieve process output data"));
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
    CheckComArgNotNull(aExitCode);
    CheckComArgNotNull(aFlags);
    CheckComArgNotNull(aStatus);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        VBOXGUESTCTRL_PROCESS process;
        int vrc = processGetByPID(aPID, &process);
        if (RT_SUCCESS(vrc))
        {
            *aExitCode = process.mExitCode;
            *aFlags = process.mFlags;
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

