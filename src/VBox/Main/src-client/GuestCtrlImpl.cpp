/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Guest
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include "GuestSessionImpl.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#include "VMMDev.h"

#include "AutoCaller.h"

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

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>


// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_WITH_GUEST_CONTROL
/**
 * Static callback function for receiving updates on guest control commands
 * from the guest. Acts as a dispatcher for the actual class instance.
 *
 * @returns VBox status code.
 *
 * @todo
 *
 */
/* static */
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
    LogFlowFunc(("pvExtension=%p, u32Function=%RU32, pvParms=%p, cbParms=%RU32\n",
                 pvExtension, u32Function, pvParms, cbParms));
    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest *>(pvExtension);
    Assert(!pGuest.isNull());

    /*
     * For guest control 2.0 using the legacy commands we need to do the following here:
     * - Get the callback header to access the context ID
     * - Get the context ID of the callback
     * - Extract the session ID out of the context ID
     * - Dispatch the whole stuff to the appropriate session (if still exists)
     */

    PCALLBACKHEADER pHeader = (PCALLBACKHEADER)pvParms;
    AssertPtr(pHeader);

#ifdef DEBUG
    LogFlowFunc(("CID=%RU32, uSession=%RU32, uObject=%RU32, uCount=%RU32\n",
                 pHeader->u32ContextID,
                 VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHeader->u32ContextID),
                 VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pHeader->u32ContextID),
                 VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(pHeader->u32ContextID)));
#endif

    bool fDispatch = true;
#ifdef DEBUG
    /*
     * Pre-check: If we got a status message with an error and VERR_TOO_MUCH_DATA
     *            it means that that guest could not handle the entire message
     *            because of its exceeding size. This should not happen on daily
     *            use but testcases might try this. It then makes no sense to dispatch
     *            this further because we don't have a valid context ID.
     */
    if (u32Function == GUEST_EXEC_SEND_STATUS)
    {
        PCALLBACKDATAEXECSTATUS pCallbackData = reinterpret_cast<PCALLBACKDATAEXECSTATUS>(pvParms);
        AssertPtr(pCallbackData);
        AssertReturn(sizeof(CALLBACKDATAEXECSTATUS) == cbParms, VERR_INVALID_PARAMETER);
        AssertReturn(CALLBACKDATAMAGIC_EXEC_STATUS == pCallbackData->hdr.u32Magic, VERR_INVALID_PARAMETER);

        if (   pCallbackData->u32Status == PROC_STS_ERROR
            && ((int)pCallbackData->u32Flags)  == VERR_TOO_MUCH_DATA)
        {
            LogFlowFunc(("Requested command with too much data, skipping dispatching ...\n"));

            Assert(pCallbackData->u32PID == 0);
            fDispatch = false;
        }
    }
#endif
    int rc = VINF_SUCCESS;
    if (fDispatch)
    {
        rc = pGuest->dispatchToSession(pHeader->u32ContextID, u32Function, pvParms, cbParms);
        if (RT_SUCCESS(rc))
            return rc;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_GUEST_CONTROL */

STDMETHODIMP Guest::UpdateGuestAdditions(IN_BSTR aSource, ComSafeArrayIn(AdditionsUpdateFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Validate flags. */
    uint32_t fFlags = AdditionsUpdateFlag_None;
    if (aFlags)
    {
        com::SafeArray<CopyFileFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];
    }

    if (fFlags)
    {
        if (!(fFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly))
            return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), aFlags);
    }

    HRESULT hr = S_OK;

    /* Create an anonymous session. This is required to run the Guest Additions
     * update process with administrative rights. */
    ComObjPtr<GuestSession> pSession;
    int rc = sessionCreate("" /* User */, "" /* Password */, "" /* Domain */,
                           "Updating Guest Additions" /* Name */, pSession);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_MAX_PROCS_REACHED:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Maximum number of guest sessions (%ld) reached"),
                              VBOX_GUESTCTRL_MAX_SESSIONS);
                break;

            /** @todo Add more errors here. */

           default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Could not create guest session: %Rrc"), rc);
                break;
        }
    }
    else
    {
        Assert(!pSession.isNull());
        rc = pSession->queryInfo();
        if (RT_FAILURE(rc))
        {
            hr = setError(VBOX_E_IPRT_ERROR, tr("Could not query guest session information: %Rrc"), rc);
        }
        else
        {
            ComObjPtr<Progress> pProgress;
            SessionTaskUpdateAdditions *pTask = new SessionTaskUpdateAdditions(pSession /* GuestSession */,
                                                                               Utf8Str(aSource), fFlags);
            AssertPtrReturn(pTask, VERR_NO_MEMORY);
            rc = pSession->startTaskAsync(tr("Updating Guest Additions"), pTask, pProgress);
            if (RT_SUCCESS(rc))
            {
                /* Return progress to the caller. */
                hr = pProgress.queryInterfaceTo(aProgress);
            }
            else
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Starting task for updating Guest Additions on the guest failed: %Rrc"), rc);
        }
    }
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

// private methods
/////////////////////////////////////////////////////////////////////////////

int Guest::dispatchToSession(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData)
{
    LogFlowFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint32_t uSessionID = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uContextID);
#ifdef DEBUG
    LogFlowFunc(("uSessionID=%RU32 (%RU32 total)\n",
                 uSessionID, mData.mGuestSessions.size()));
#endif
    int rc;
    GuestSessions::const_iterator itSession
        = mData.mGuestSessions.find(uSessionID);
    if (itSession != mData.mGuestSessions.end())
    {
        ComObjPtr<GuestSession> pSession(itSession->second);
        Assert(!pSession.isNull());

        alock.release();
        rc = pSession->dispatchToProcess(uContextID, uFunction, pvData, cbData);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int Guest::sessionRemove(GuestSession *pSession)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_NOT_FOUND;

    LogFlowFunc(("Closing session (ID=%RU32) ...\n", pSession->getId()));

    for (GuestSessions::iterator itSessions = mData.mGuestSessions.begin();
         itSessions != mData.mGuestSessions.end(); ++itSessions)
    {
        if (pSession == itSessions->second)
        {
            LogFlowFunc(("Removing session (pSession=%p, ID=%RU32) (now total %ld sessions)\n",
                         itSessions->second, itSessions->second->getId(), mData.mGuestSessions.size() - 1));

            mData.mGuestSessions.erase(itSessions);

            rc = VINF_SUCCESS;
            break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int Guest::sessionCreate(const Utf8Str &strUser, const Utf8Str &strPassword, const Utf8Str &strDomain,
                         const Utf8Str &strSessionName, ComObjPtr<GuestSession> &pGuestSession)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_MAX_PROCS_REACHED;
    if (mData.mGuestSessions.size() >= VBOX_GUESTCTRL_MAX_SESSIONS)
        return rc;

    try
    {
        /* Create a new session ID and assign it. */
        uint32_t uNewSessionID = 0;
        uint32_t uTries = 0;

        for (;;)
        {
            /* Is the context ID already used? */
            if (!sessionExists(uNewSessionID))
            {
                rc = VINF_SUCCESS;
                break;
            }
            uNewSessionID++;
            if (uNewSessionID >= VBOX_GUESTCTRL_MAX_SESSIONS)
                uNewSessionID = 0;

            if (++uTries == UINT32_MAX)
                break; /* Don't try too hard. */
        }
        if (RT_FAILURE(rc)) throw rc;

        /* Create the session object. */
        HRESULT hr = pGuestSession.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        rc = pGuestSession->init(this, uNewSessionID,
                                 strUser, strPassword, strDomain, strSessionName);
        if (RT_FAILURE(rc)) throw rc;

        mData.mGuestSessions[uNewSessionID] = pGuestSession;

        LogFlowFunc(("Added new session (pSession=%p, ID=%RU32), now %ld sessions total\n",
                     pGuestSession, uNewSessionID, mData.mGuestSessions.size()));
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    return rc;
}

inline bool Guest::sessionExists(uint32_t uSessionID)
{
    GuestSessions::const_iterator itSessions = mData.mGuestSessions.find(uSessionID);
    return (itSessions == mData.mGuestSessions.end()) ? false : true;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Guest::CreateSession(IN_BSTR aUser, IN_BSTR aPassword, IN_BSTR aDomain, IN_BSTR aSessionName, IGuestSession **aGuestSession)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */

    LogFlowFuncEnter();

    /* Do not allow anonymous sessions (with system rights) with official API. */
    if (RT_UNLIKELY((aUser) == NULL || *(aUser) == '\0'))
        return setError(E_INVALIDARG, tr("No user name specified"));
    CheckComArgOutPointerValid(aGuestSession);
    /* Rest is optional. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    ComObjPtr<GuestSession> pSession;
    int rc = sessionCreate(aUser, aPassword, aDomain, aSessionName, pSession);
    if (RT_SUCCESS(rc))
    {
        /* Return guest session to the caller. */
        HRESULT hr2 = pSession.queryInterfaceTo(aGuestSession);
        if (FAILED(hr2))
            rc = VERR_COM_OBJECT_NOT_FOUND;

        if (RT_SUCCESS(rc))
            rc = pSession->queryInfo();
    }

    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_MAX_PROCS_REACHED:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Maximum number of guest sessions (%ld) reached"),
                              VBOX_GUESTCTRL_MAX_SESSIONS);
                break;

            /** @todo Add more errors here. */

           default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Could not create guest session, rc=%Rrc"), rc);
                break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP Guest::FindSession(IN_BSTR aSessionName, ComSafeArrayOut(IGuestSession *, aSessions))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */

    CheckComArgOutSafeArrayPointerValid(aSessions);

    LogFlowFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strName(aSessionName);
    std::list < ComObjPtr<GuestSession> > listSessions;

    GuestSessions::const_iterator itSessions = mData.mGuestSessions.begin();
    while (itSessions != mData.mGuestSessions.end())
    {
        if (strName.contains(itSessions->second->getName())) /** @todo Use a (simple) pattern match (IPRT?). */
            listSessions.push_back(itSessions->second);
        itSessions++;
    }

    LogFlowFunc(("Sessions with \"%ls\" = %RU32\n",
                 aSessionName, listSessions.size()));

    if (listSessions.size())
    {
        SafeIfaceArray<IGuestSession> sessionIfacs(listSessions);
        sessionIfacs.detachTo(ComSafeArrayOutArg(aSessions));

        return S_OK;
    }

    return setErrorNoLog(VBOX_E_OBJECT_NOT_FOUND,
                         tr("Could not find sessions with name '%ls'"),
                         aSessionName);
#endif /* VBOX_WITH_GUEST_CONTROL */
}

