/* $Id$ */
/** @file
 * VirtualBox Main - Guest session handling.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_MAIN_GUESTSESSION
#include "LoggingNew.h"

#include "GuestImpl.h"
#ifndef VBOX_WITH_GUEST_CONTROL
# error "VBOX_WITH_GUEST_CONTROL must defined in this file"
#endif
#include "GuestSessionImpl.h"
#include "GuestSessionImplTasks.h"
#include "GuestCtrlImplPrivate.h"
#include "VirtualBoxErrorInfoImpl.h"

#include "Global.h"
#include "AutoCaller.h"
#include "ProgressImpl.h"
#include "VBoxEvents.h"
#include "VMMDev.h"
#include "ThreadTask.h"

#include <memory> /* For auto_ptr. */

#include <iprt/cpp/utils.h> /* For unconst(). */
#include <iprt/env.h>
#include <iprt/file.h> /* For CopyTo/From. */
#include <iprt/path.h>

#include <VBox/com/array.h>
#include <VBox/com/listeners.h>
#include <VBox/version.h>


/**
 * Base class representing an internal
 * asynchronous session task.
 */
class GuestSessionTaskInternal : public ThreadTask
{
public:

    GuestSessionTaskInternal(GuestSession *pSession)
        : ThreadTask("GenericGuestSessionTaskInternal")
        , mSession(pSession)
        , mRC(VINF_SUCCESS) { }

    virtual ~GuestSessionTaskInternal(void) { }

    int rc(void) const { return mRC; }
    bool isOk(void) const { return RT_SUCCESS(mRC); }
    const ComObjPtr<GuestSession> &Session(void) const { return mSession; }

protected:

    const ComObjPtr<GuestSession>    mSession;
    int                              mRC;
};

/**
 * Class for asynchronously opening a guest session.
 */
class GuestSessionTaskInternalOpen : public GuestSessionTaskInternal
{
public:

    GuestSessionTaskInternalOpen(GuestSession *pSession)
        : GuestSessionTaskInternal(pSession)
    {
        m_strTaskName = "gctlSesStart";
    }

    void handler()
    {
        GuestSession::i_startSessionThreadTask(this);
    }
};

/**
 * Internal listener class to serve events in an
 * active manner, e.g. without polling delays.
 */
class GuestSessionListener
{
public:

    GuestSessionListener(void)
    {
    }

    virtual ~GuestSessionListener(void)
    {
    }

    HRESULT init(GuestSession *pSession)
    {
        AssertPtrReturn(pSession, E_POINTER);
        mSession = pSession;
        return S_OK;
    }

    void uninit(void)
    {
        mSession = NULL;
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnGuestSessionStateChanged:
            {
                AssertPtrReturn(mSession, E_POINTER);
                int rc2 = mSession->signalWaitEvent(aType, aEvent);
                RT_NOREF(rc2);
#ifdef DEBUG_andy
                LogFlowFunc(("Signalling events of type=%RU32, session=%p resulted in rc=%Rrc\n",
                             aType, mSession, rc2));
#endif
                break;
            }

            default:
                AssertMsgFailed(("Unhandled event %RU32\n", aType));
                break;
        }

        return S_OK;
    }

private:

    GuestSession *mSession;
};
typedef ListenerImpl<GuestSessionListener, GuestSession*> GuestSessionListenerImpl;

VBOX_LISTENER_DECLARE(GuestSessionListenerImpl)

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestSession)

HRESULT GuestSession::FinalConstruct(void)
{
    LogFlowThisFuncEnter();
    return BaseFinalConstruct();
}

void GuestSession::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes a guest session but does *not* open in on the guest side
 * yet. This needs to be done via the openSession() / openSessionAsync calls.
 *
 * @return  IPRT status code.
 ** @todo Docs!
 */
int GuestSession::init(Guest *pGuest, const GuestSessionStartupInfo &ssInfo,
                       const GuestCredentials &guestCreds)
{
    LogFlowThisFunc(("pGuest=%p, ssInfo=%p, guestCreds=%p\n",
                      pGuest, &ssInfo, &guestCreds));

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

    AssertPtrReturn(pGuest, VERR_INVALID_POINTER);

    /*
     * Initialize our data members from the input.
     */
    mParent = pGuest;

    /* Copy over startup info. */
    /** @todo Use an overloaded copy operator. Later. */
    mData.mSession.mID = ssInfo.mID;
    mData.mSession.mIsInternal = ssInfo.mIsInternal;
    mData.mSession.mName = ssInfo.mName;
    mData.mSession.mOpenFlags = ssInfo.mOpenFlags;
    mData.mSession.mOpenTimeoutMS = ssInfo.mOpenTimeoutMS;

    /* Copy over session credentials. */
    /** @todo Use an overloaded copy operator. Later. */
    mData.mCredentials.mUser = guestCreds.mUser;
    mData.mCredentials.mPassword = guestCreds.mPassword;
    mData.mCredentials.mDomain = guestCreds.mDomain;

    /* Initialize the remainder of the data. */
    mData.mRC = VINF_SUCCESS;
    mData.mStatus = GuestSessionStatus_Undefined;
    mData.mpBaseEnvironment = NULL;

    /*
     * Register an object for the session itself to clearly
     * distinguish callbacks which are for this session directly, or for
     * objects (like files, directories, ...) which are bound to this session.
     */
    int rc = i_objectRegister(SESSIONOBJECTTYPE_SESSION, &mData.mObjectID);
    if (RT_SUCCESS(rc))
    {
        rc = mData.mEnvironmentChanges.initChangeRecord();
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&mWaitEventCritSect);
            AssertRC(rc);
        }
    }

    if (RT_SUCCESS(rc))
        rc = i_determineProtocolVersion();

    if (RT_SUCCESS(rc))
    {
        /*
         * <Replace this if you figure out what the code is doing.>
         */
        HRESULT hr = unconst(mEventSource).createObject();
        if (SUCCEEDED(hr))
            hr = mEventSource->init();
        if (SUCCEEDED(hr))
        {
            try
            {
                GuestSessionListener *pListener = new GuestSessionListener();
                ComObjPtr<GuestSessionListenerImpl> thisListener;
                hr = thisListener.createObject();
                if (SUCCEEDED(hr))
                    hr = thisListener->init(pListener, this); /* thisListener takes ownership of pListener. */
                if (SUCCEEDED(hr))
                {
                    com::SafeArray <VBoxEventType_T> eventTypes;
                    eventTypes.push_back(VBoxEventType_OnGuestSessionStateChanged);
                    hr = mEventSource->RegisterListener(thisListener,
                                                        ComSafeArrayAsInParam(eventTypes),
                                                        TRUE /* Active listener */);
                    if (SUCCEEDED(hr))
                    {
                        mLocalListener = thisListener;

                        /*
                         * Mark this object as operational and return success.
                         */
                        autoInitSpan.setSucceeded();
                        LogFlowThisFunc(("mName=%s mID=%RU32 mIsInternal=%RTbool rc=VINF_SUCCESS\n",
                                         mData.mSession.mName.c_str(), mData.mSession.mID, mData.mSession.mIsInternal));
                        return VINF_SUCCESS;
                    }
                }
            }
            catch (std::bad_alloc &)
            {
                hr = E_OUTOFMEMORY;
            }
        }
        rc = Global::vboxStatusCodeFromCOM(hr);
    }

    autoInitSpan.setFailed();
    LogThisFunc(("Failed! mName=%s mID=%RU32 mIsInternal=%RTbool => rc=%Rrc\n",
                 mData.mSession.mName.c_str(), mData.mSession.mID, mData.mSession.mIsInternal, rc));
    return rc;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestSession::uninit(void)
{
    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("Closing directories (%zu total)\n",
                     mData.mDirectories.size()));
    for (SessionDirectories::iterator itDirs = mData.mDirectories.begin();
         itDirs != mData.mDirectories.end(); ++itDirs)
    {
        itDirs->second->i_onRemove();
        itDirs->second->uninit();
    }
    mData.mDirectories.clear();

    LogFlowThisFunc(("Closing files (%zu total)\n",
                     mData.mFiles.size()));
    for (SessionFiles::iterator itFiles = mData.mFiles.begin();
         itFiles != mData.mFiles.end(); ++itFiles)
    {
        itFiles->second->i_onRemove();
        itFiles->second->uninit();
    }
    mData.mFiles.clear();

    LogFlowThisFunc(("Closing processes (%zu total)\n",
                     mData.mProcesses.size()));
    for (SessionProcesses::iterator itProcs = mData.mProcesses.begin();
         itProcs != mData.mProcesses.end(); ++itProcs)
    {
        itProcs->second->i_onRemove();
        itProcs->second->uninit();
    }
    mData.mProcesses.clear();

    /* Unregister the session's object ID. */
    i_objectUnregister(mData.mObjectID);

    mData.mObjects.clear();
    mData.mObjectsFree.clear();

    mData.mEnvironmentChanges.reset();

    if (mData.mpBaseEnvironment)
    {
        mData.mpBaseEnvironment->releaseConst();
        mData.mpBaseEnvironment = NULL;
    }

    /* Unitialize our local listener. */
    mLocalListener.setNull();

    baseUninit();

    LogFlowFuncLeave();
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestSession::getUser(com::Utf8Str &aUser)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aUser = mData.mCredentials.mUser;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getDomain(com::Utf8Str &aDomain)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aDomain = mData.mCredentials.mDomain;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getName(com::Utf8Str &aName)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aName = mData.mSession.mName;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getId(ULONG *aId)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aId = mData.mSession.mID;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getStatus(GuestSessionStatus_T *aStatus)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aStatus = mData.mStatus;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getTimeout(ULONG *aTimeout)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeout = mData.mTimeout;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::setTimeout(ULONG aTimeout)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mTimeout = aTimeout;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getProtocolVersion(ULONG *aProtocolVersion)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aProtocolVersion = mData.mProtocolVersion;

    LogFlowThisFuncLeave();
    return S_OK;
}

HRESULT GuestSession::getEnvironmentChanges(std::vector<com::Utf8Str> &aEnvironmentChanges)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    int vrc = mData.mEnvironmentChanges.queryPutEnvArray(&aEnvironmentChanges);

    LogFlowFuncLeaveRC(vrc);
    return Global::vboxStatusCodeToCOM(vrc);
}

HRESULT GuestSession::setEnvironmentChanges(const std::vector<com::Utf8Str> &aEnvironmentChanges)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mEnvironmentChanges.reset();
    int vrc = mData.mEnvironmentChanges.applyPutEnvArray(aEnvironmentChanges);

    LogFlowFuncLeaveRC(vrc);
    return Global::vboxStatusCodeToCOM(vrc);
}

HRESULT GuestSession::getEnvironmentBase(std::vector<com::Utf8Str> &aEnvironmentBase)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT hrc;
    if (mData.mpBaseEnvironment)
    {
        int vrc = mData.mpBaseEnvironment->queryPutEnvArray(&aEnvironmentBase);
        hrc = Global::vboxStatusCodeToCOM(vrc);
    }
    else if (mData.mProtocolVersion < 99999)
        hrc = setError(VBOX_E_NOT_SUPPORTED, tr("The base environment feature is not supported by the guest additions"));
    else
        hrc = setError(VBOX_E_INVALID_OBJECT_STATE, tr("The base environment has not yet been reported by the guest"));

    LogFlowFuncLeave();
    return hrc;
}

HRESULT GuestSession::getProcesses(std::vector<ComPtr<IGuestProcess> > &aProcesses)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aProcesses.resize(mData.mProcesses.size());
    size_t i = 0;
    for(SessionProcesses::iterator it  = mData.mProcesses.begin();
                                   it != mData.mProcesses.end();
                                   ++it, ++i)
    {
        it->second.queryInterfaceTo(aProcesses[i].asOutParam());
    }

    LogFlowFunc(("mProcesses=%zu\n", aProcesses.size()));
    return S_OK;
}

HRESULT GuestSession::getPathStyle(PathStyle_T *aPathStyle)
{
    *aPathStyle = i_getPathStyle();
    return S_OK;
}

HRESULT GuestSession::getCurrentDirectory(com::Utf8Str &aCurrentDirectory)
{
    RT_NOREF(aCurrentDirectory);
    ReturnComNotImplemented();
}

HRESULT GuestSession::setCurrentDirectory(const com::Utf8Str &aCurrentDirectory)
{
    RT_NOREF(aCurrentDirectory);
    ReturnComNotImplemented();
}

HRESULT GuestSession::getUserHome(com::Utf8Str &aUserHome)
{
    HRESULT hr = i_isReadyExternal();
    if (FAILED(hr))
        return hr;

    int rcGuest;
    int vrc = i_pathUserHome(aUserHome, &rcGuest);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
            {
                switch (rcGuest)
                {
                    case VERR_NOT_SUPPORTED:
                        hr = setError(VBOX_E_IPRT_ERROR, tr("Getting the user's home path is not supported by installed Guest Additions"));
                        break;

                    default:
                        hr = setError(VBOX_E_IPRT_ERROR, tr("Getting the user's home path failed on the guest: %Rrc"), rcGuest);
                        break;
                }
                break;
            }

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Getting the user's home path failed: %Rrc"), vrc);
                break;
        }
    }

    return hr;
}

HRESULT GuestSession::getUserDocuments(com::Utf8Str &aUserDocuments)
{
    HRESULT hr = i_isReadyExternal();
    if (FAILED(hr))
        return hr;

    int rcGuest;
    int vrc = i_pathUserDocuments(aUserDocuments, &rcGuest);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
            {
                switch (rcGuest)
                {
                    case VERR_NOT_SUPPORTED:
                        hr = setError(VBOX_E_IPRT_ERROR, tr("Getting the user's documents path is not supported by installed Guest Additions"));
                        break;

                    default:
                        hr = setError(VBOX_E_IPRT_ERROR, tr("Getting the user's documents path failed on the guest: %Rrc"), rcGuest);
                        break;
                }
                break;
            }

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Getting the user's documents path failed: %Rrc"), vrc);
                break;
        }
    }

    return hr;
}

HRESULT GuestSession::getDirectories(std::vector<ComPtr<IGuestDirectory> > &aDirectories)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aDirectories.resize(mData.mDirectories.size());
    size_t i = 0;
    for(SessionDirectories::iterator it  = mData.mDirectories.begin();
                                     it != mData.mDirectories.end();
                                     ++it, ++i)
    {
        it->second.queryInterfaceTo(aDirectories[i].asOutParam());
    }

    LogFlowFunc(("mDirectories=%zu\n", aDirectories.size()));
    return S_OK;
}

HRESULT GuestSession::getFiles(std::vector<ComPtr<IGuestFile> > &aFiles)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aFiles.resize(mData.mFiles.size());
    size_t i = 0;
    for(SessionFiles::iterator it = mData.mFiles.begin(); it != mData.mFiles.end(); ++it, ++i)
        it->second.queryInterfaceTo(aFiles[i].asOutParam());

    LogFlowFunc(("mDirectories=%zu\n", aFiles.size()));

    return S_OK;
}

HRESULT GuestSession::getEventSource(ComPtr<IEventSource> &aEventSource)
{
    LogFlowThisFuncEnter();

    // no need to lock - lifetime constant
    mEventSource.queryInterfaceTo(aEventSource.asOutParam());

    LogFlowThisFuncLeave();
    return S_OK;
}

// private methods
///////////////////////////////////////////////////////////////////////////////

int GuestSession::i_closeSession(uint32_t uFlags, uint32_t uTimeoutMS, int *prcGuest)
{
    AssertPtrReturn(prcGuest, VERR_INVALID_POINTER);

    LogFlowThisFunc(("uFlags=%x, uTimeoutMS=%RU32\n", uFlags, uTimeoutMS));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Guest Additions < 4.3 don't support closing dedicated
       guest sessions, skip. */
    if (mData.mProtocolVersion < 2)
    {
        LogFlowThisFunc(("Installed Guest Additions don't support closing dedicated sessions, skipping\n"));
        return VINF_SUCCESS;
    }

    /** @todo uFlags validation. */

    if (mData.mStatus != GuestSessionStatus_Started)
    {
        LogFlowThisFunc(("Session ID=%RU32 not started (anymore), status now is: %RU32\n",
                         mData.mSession.mID, mData.mStatus));
        return VINF_SUCCESS;
    }

    int vrc;

    GuestWaitEvent *pEvent = NULL;
    GuestEventTypes eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestSessionStateChanged);

        vrc = registerWaitEventEx(mData.mSession.mID, mData.mObjectID, eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    LogFlowThisFunc(("Sending closing request to guest session ID=%RU32, uFlags=%x\n",
                     mData.mSession.mID, uFlags));

    VBOXHGCMSVCPARM paParms[4];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(uFlags);

    alock.release(); /* Drop the write lock before waiting. */

    vrc = i_sendCommand(HOST_SESSION_CLOSE, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = i_waitForStatusChange(pEvent, GuestSessionWaitForFlag_Terminate, uTimeoutMS,
                                    NULL /* Session status */, prcGuest);

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Internal worker function for public APIs that handle copying elements from
 * guest to the host.
 *
 * @return HRESULT
 * @param  SourceSet            Source set specifying what to copy.
 * @param  strDestination       Destination path on the host. Host path style.
 * @param  pProgress            Progress object returned to the caller.
 */
HRESULT GuestSession::i_copyFromGuest(const GuestSessionFsSourceSet &SourceSet,
                                      const com::Utf8Str &strDestination, ComPtr<IProgress> &pProgress)
{
    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    /* Validate stuff. */
    if (RT_UNLIKELY(SourceSet.size() == 0 || *(SourceSet[0].strSource.c_str()) == '\0')) /* At least one source must be present. */
        return setError(E_INVALIDARG, tr("No source(s) specified"));
    if (RT_UNLIKELY((strDestination.c_str()) == NULL || *(strDestination.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No destination specified"));

    GuestSessionFsSourceSet::const_iterator itSource = SourceSet.begin();
    while (itSource != SourceSet.end())
    {
        if (itSource->enmType == FsObjType_Directory)
        {
            if (itSource->Type.Dir.fCopyFlags)
            {
                if (!(itSource->Type.Dir.fCopyFlags & DirectoryCopyFlag_CopyIntoExisting))
                    return setError(E_INVALIDARG, tr("Invalid / not (yet) implemented directory copy flags specified"));
            }
        }
        else if (itSource->enmType == FsObjType_File)
        {
            if (itSource->Type.File.fCopyFlags)
            {
                if (   !(itSource->Type.File.fCopyFlags & FileCopyFlag_NoReplace)
                    && !(itSource->Type.File.fCopyFlags & FileCopyFlag_FollowLinks)
                    && !(itSource->Type.File.fCopyFlags & FileCopyFlag_Update))
                {
                    return setError(E_NOTIMPL, tr("Invalid / not (yet) implemented file copy flag(s) specified"));
                }
            }
        }
        else
            return setError(E_NOTIMPL, tr("Source type %RU32 not implemented"), itSource->enmType);

        itSource++;
    }

    try
    {
        GuestSessionTaskCopyFrom *pTask = NULL;
        ComObjPtr<Progress> pProgressObj;
        try
        {
            pTask = new GuestSessionTaskCopyFrom(this /* GuestSession */, SourceSet, strDestination);
        }
        catch(...)
        {
            hrc = setError(VBOX_E_IPRT_ERROR, tr("Failed to create SessionTaskCopyFrom object"));
            throw;
        }

        hrc = pTask->Init(Utf8StrFmt(tr("Copying to \"%s\" on the host"), strDestination.c_str()));
        if (FAILED(hrc))
        {
            delete pTask;
            hrc = setError(VBOX_E_IPRT_ERROR,
                           tr("Creating progress object for SessionTaskCopyFrom object failed"));
            throw hrc;
        }

        hrc = pTask->createThreadWithType(RTTHREADTYPE_MAIN_HEAVY_WORKER);

        if (SUCCEEDED(hrc))
        {
            /* Return progress to the caller. */
            pProgressObj = pTask->GetProgressObject();
            hrc = pProgressObj.queryInterfaceTo(pProgress.asOutParam());
        }
        else
            hrc = setError(VBOX_E_IPRT_ERROR,
                           tr("Starting thread for copying from guest to \"%s\" on the host failed"), strDestination.c_str());

    }
    catch(std::bad_alloc &)
    {
        hrc = E_OUTOFMEMORY;
    }
    catch(HRESULT eHR)
    {
        hrc = eHR;
        LogFlowThisFunc(("Exception was caught in the function\n"));
    }

    return hrc;
}

/**
 * Internal worker function for public APIs that handle copying elements from
 * host to the guest.
 *
 * @return HRESULT
 * @param  SourceSet            Source set specifying what to copy.
 * @param  strDestination       Destination path on the guest. Guest path style.
 * @param  pProgress            Progress object returned to the caller.
 */
HRESULT GuestSession::i_copyToGuest(const GuestSessionFsSourceSet &SourceSet,
                                    const com::Utf8Str &strDestination, ComPtr<IProgress> &pProgress)
{
    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    /* Validate stuff. */
    if (RT_UNLIKELY(SourceSet.size() == 0 || *(SourceSet[0].strSource.c_str()) == '\0')) /* At least one source must be present. */
        return setError(E_INVALIDARG, tr("No source(s) specified"));
    if (RT_UNLIKELY((strDestination.c_str()) == NULL || *(strDestination.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No destination specified"));

    GuestSessionFsSourceSet::const_iterator itSource = SourceSet.begin();
    while (itSource != SourceSet.end())
    {
        if (itSource->enmType == FsObjType_Directory)
        {
            if (itSource->Type.Dir.fCopyFlags)
            {
                if (!(itSource->Type.Dir.fCopyFlags & DirectoryCopyFlag_CopyIntoExisting))
                    return setError(E_INVALIDARG, tr("Invalid / not (yet) implemented directory copy flags specified"));
            }
        }
        else if (itSource->enmType == FsObjType_File)
        {
            if (itSource->Type.File.fCopyFlags)
            {
                if (   !(itSource->Type.File.fCopyFlags & FileCopyFlag_NoReplace)
                    && !(itSource->Type.File.fCopyFlags & FileCopyFlag_FollowLinks)
                    && !(itSource->Type.File.fCopyFlags & FileCopyFlag_Update))
                {
                    return setError(E_NOTIMPL, tr("Invalid / not (yet) implemented file copy flag(s) specified"));
                }
            }
        }
        else
            return setError(E_NOTIMPL, tr("Source type %RU32 not implemented"), itSource->enmType);

        itSource++;
    }

    try
    {
        GuestSessionTaskCopyTo *pTask = NULL;
        ComObjPtr<Progress> pProgressObj;
        try
        {
            pTask = new GuestSessionTaskCopyTo(this /* GuestSession */, SourceSet, strDestination);
        }
        catch(...)
        {
            hrc = setError(VBOX_E_IPRT_ERROR, tr("Failed to create SessionTaskCopyTo object"));
            throw;
        }

        hrc = pTask->Init(Utf8StrFmt(tr("Copying to \"%s\" on the guest"), strDestination.c_str()));
        if (FAILED(hrc))
        {
            delete pTask;
            hrc = setError(VBOX_E_IPRT_ERROR,
                           tr("Creating progress object for SessionTaskCopyTo object failed"));
            throw hrc;
        }

        hrc = pTask->createThreadWithType(RTTHREADTYPE_MAIN_HEAVY_WORKER);

        if (SUCCEEDED(hrc))
        {
            /* Return progress to the caller. */
            pProgress = pTask->GetProgressObject();
            hrc = pProgressObj.queryInterfaceTo(pProgress.asOutParam());
        }
        else
            hrc = setError(VBOX_E_IPRT_ERROR,
                           tr("Starting thread for copying from host to \"%s\" on the guest failed"), strDestination.c_str());

    }
    catch(std::bad_alloc &)
    {
        hrc = E_OUTOFMEMORY;
    }
    catch(HRESULT eHR)
    {
        hrc = eHR;
        LogFlowThisFunc(("Exception was caught in the function\n"));
    }

    return hrc;
}

int GuestSession::i_directoryCreate(const Utf8Str &strPath, uint32_t uMode,
                                    uint32_t uFlags, int *prcGuest)
{
    AssertPtrReturn(prcGuest, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strPath=%s, uMode=%x, uFlags=%x\n", strPath.c_str(), uMode, uFlags));

    int vrc = VINF_SUCCESS;

    GuestProcessStartupInfo procInfo;
    procInfo.mFlags      = ProcessCreateFlag_Hidden;
    procInfo.mExecutable = Utf8Str(VBOXSERVICE_TOOL_MKDIR);

    try
    {
        procInfo.mArguments.push_back(procInfo.mExecutable); /* Set argv0. */

        /* Construct arguments. */
        if (uFlags)
        {
            if (uFlags & DirectoryCreateFlag_Parents)
                procInfo.mArguments.push_back(Utf8Str("--parents")); /* We also want to create the parent directories. */
            else
                vrc = VERR_INVALID_PARAMETER;
        }

        if (   RT_SUCCESS(vrc)
            && uMode)
        {
            procInfo.mArguments.push_back(Utf8Str("--mode")); /* Set the creation mode. */

            char szMode[16];
            if (RTStrPrintf(szMode, sizeof(szMode), "%o", uMode))
            {
                procInfo.mArguments.push_back(Utf8Str(szMode));
            }
            else
                vrc = VERR_BUFFER_OVERFLOW;
        }

        procInfo.mArguments.push_back("--");    /* '--version' is a valid directory name. */
        procInfo.mArguments.push_back(strPath); /* The directory we want to create. */
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(vrc))
        vrc = GuestProcessTool::run(this, procInfo, prcGuest);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

inline bool GuestSession::i_directoryExists(uint32_t uDirID, ComObjPtr<GuestDirectory> *pDir)
{
    SessionDirectories::const_iterator it = mData.mDirectories.find(uDirID);
    if (it != mData.mDirectories.end())
    {
        if (pDir)
            *pDir = it->second;
        return true;
    }
    return false;
}

int GuestSession::i_directoryQueryInfo(const Utf8Str &strPath, bool fFollowSymlinks,
                                       GuestFsObjData &objData, int *prcGuest)
{
    AssertPtrReturn(prcGuest, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strPath=%s, fFollowSymlinks=%RTbool\n", strPath.c_str(), fFollowSymlinks));

    int vrc = i_fsQueryInfo(strPath, fFollowSymlinks, objData, prcGuest);
    if (RT_SUCCESS(vrc))
    {
        vrc = objData.mType == FsObjType_Directory
            ? VINF_SUCCESS : VERR_NOT_A_DIRECTORY;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Unregisters a directory object from a session.
 *
 * @return VBox status code. VERR_NOT_FOUND if the directory is not registered (anymore).
 * @param  pDirectory           Directory object to unregister from session.
 */
int GuestSession::i_directoryUnregister(GuestDirectory *pDirectory)
{
    AssertPtrReturn(pDirectory, VERR_INVALID_POINTER);

    LogFlowThisFunc(("pDirectory=%p\n", pDirectory));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    const uint32_t uObjectID = pDirectory->getObjectID();

    LogFlowFunc(("Removing directory (objectID=%RU32) ...\n", uObjectID));

    int rc = i_objectUnregister(uObjectID);
    if (RT_FAILURE(rc))
        return rc;

    SessionDirectories::iterator itDirs = mData.mDirectories.find(uObjectID);
    AssertReturn(itDirs != mData.mDirectories.end(), VERR_NOT_FOUND);

    /* Make sure to consume the pointer before the one of the iterator gets released. */
    ComObjPtr<GuestDirectory> pDirConsumed = pDirectory;

    LogFlowFunc(("Removing directory ID=%RU32 (session %RU32, now total %zu directories)\n",
                 uObjectID, mData.mSession.mID, mData.mDirectories.size()));

    rc = pDirConsumed->i_onRemove();
    AssertRCReturn(rc, rc);

    mData.mDirectories.erase(itDirs);

    alock.release(); /* Release lock before firing off event. */

//    fireGuestDirectoryRegisteredEvent(mEventSource, this /* Session */, pDirConsumed, false /* Process unregistered */);

    pDirConsumed.setNull();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::i_directoryRemove(const Utf8Str &strPath, uint32_t uFlags, int *prcGuest)
{
    AssertReturn(!(uFlags & ~DIRREMOVE_FLAG_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertPtrReturn(prcGuest, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strPath=%s, uFlags=0x%x\n", strPath.c_str(), uFlags));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestWaitEvent *pEvent = NULL;
    int vrc = registerWaitEvent(mData.mSession.mID, mData.mObjectID, &pEvent);
    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[8];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setPointer((void*)strPath.c_str(),
                            (ULONG)strPath.length() + 1);
    paParms[i++].setUInt32(uFlags);

    alock.release(); /* Drop write lock before sending. */

    vrc = i_sendCommand(HOST_DIR_REMOVE, i, paParms);
    if (RT_SUCCESS(vrc))
    {
        vrc = pEvent->Wait(30 * 1000);
        if (   vrc == VERR_GSTCTL_GUEST_ERROR
            && prcGuest)
            *prcGuest = pEvent->GuestResult();
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::i_fsCreateTemp(const Utf8Str &strTemplate, const Utf8Str &strPath, bool fDirectory, Utf8Str &strName,
                                 int *prcGuest)
{
    AssertPtrReturn(prcGuest, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strTemplate=%s, strPath=%s, fDirectory=%RTbool\n",
                     strTemplate.c_str(), strPath.c_str(), fDirectory));

    GuestProcessStartupInfo procInfo;
    procInfo.mFlags = ProcessCreateFlag_WaitForStdOut;
    try
    {
        procInfo.mExecutable = Utf8Str(VBOXSERVICE_TOOL_MKTEMP);
        procInfo.mArguments.push_back(procInfo.mExecutable); /* Set argv0. */
        procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
        if (fDirectory)
            procInfo.mArguments.push_back(Utf8Str("-d"));
        if (strPath.length()) /* Otherwise use /tmp or equivalent. */
        {
            procInfo.mArguments.push_back(Utf8Str("-t"));
            procInfo.mArguments.push_back(strPath);
        }
        procInfo.mArguments.push_back("--"); /* strTemplate could be '--help'. */
        procInfo.mArguments.push_back(strTemplate);
    }
    catch (std::bad_alloc)
    {
        Log(("Out of memory!\n"));
        return VERR_NO_MEMORY;
    }

    /** @todo Use an internal HGCM command for this operation, since
     *        we now can run in a user-dedicated session. */
    int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    GuestCtrlStreamObjects stdOut;
    int vrc = GuestProcessTool::runEx(this, procInfo, &stdOut, 1 /* cStrmOutObjects */, &vrcGuest);
    if (!GuestProcess::i_isGuestError(vrc))
    {
        GuestFsObjData objData;
        if (!stdOut.empty())
        {
            vrc = objData.FromMkTemp(stdOut.at(0));
            if (RT_FAILURE(vrc))
            {
                vrcGuest = vrc;
                if (prcGuest)
                    *prcGuest = vrc;
                vrc = VERR_GSTCTL_GUEST_ERROR;
            }
        }
        else
            vrc = VERR_BROKEN_PIPE;

        if (RT_SUCCESS(vrc))
            strName = objData.mName;
    }
    else if (prcGuest)
        *prcGuest = vrcGuest;

    LogFlowThisFunc(("Returning vrc=%Rrc, vrcGuest=%Rrc\n", vrc, vrcGuest));
    return vrc;
}

int GuestSession::i_directoryOpen(const GuestDirectoryOpenInfo &openInfo,
                                  ComObjPtr<GuestDirectory> &pDirectory, int *prcGuest)
{
    AssertPtrReturn(prcGuest, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strPath=%s, strPath=%s, uFlags=%x\n",
                     openInfo.mPath.c_str(), openInfo.mFilter.c_str(), openInfo.mFlags));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Register a new object ID. */
    uint32_t uObjectID;
    int rc = i_objectRegister(SESSIONOBJECTTYPE_DIRECTORY, &uObjectID);
    if (RT_FAILURE(rc))
        return rc;

    /* Create the directory object. */
    HRESULT hr = pDirectory.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    Console *pConsole = mParent->i_getConsole();
    AssertPtr(pConsole);

    int vrc = pDirectory->init(pConsole, this /* Parent */, uObjectID, openInfo);
    if (RT_FAILURE(vrc))
        return vrc;

    /*
     * Since this is a synchronous guest call we have to
     * register the file object first, releasing the session's
     * lock and then proceed with the actual opening command
     * -- otherwise the file's opening callback would hang
     * because the session's lock still is in place.
     */
    try
    {
        /* Add the created directory to our map. */
        mData.mDirectories[uObjectID] = pDirectory;

        LogFlowFunc(("Added new guest directory \"%s\" (Session: %RU32) (now total %zu directories)\n",
                     openInfo.mPath.c_str(), mData.mSession.mID, mData.mDirectories.size()));

        alock.release(); /* Release lock before firing off event. */

        /** @todo Fire off a VBoxEventType_OnGuestDirectoryRegistered event? */
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        /* Nothing further to do here yet. */
        if (prcGuest)
            *prcGuest = VINF_SUCCESS;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Dispatches a host callback to its corresponding object.
 *
 * @return VBox status code. VERR_NOT_FOUND if no corresponding object was found.
 * @param  pCtxCb               Host callback context.
 * @param  pSvcCb               Service callback data.
 */
int GuestSession::i_dispatchToObject(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
{
    LogFlowFunc(("pCtxCb=%p, pSvcCb=%p\n", pCtxCb, pSvcCb));

    AssertPtrReturn(pCtxCb, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    const uint32_t uObjectID = VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pCtxCb->uContextID);

    int rc = VERR_NOT_FOUND;

    SessionObjects::const_iterator itObjs = mData.mObjects.find(uObjectID);

    if (itObjs == mData.mObjects.end())
        return rc;

    /* Set protocol version so that pSvcCb can be interpreted right. */
    pCtxCb->uProtocol = mData.mProtocolVersion;

    switch (itObjs->second.enmType)
    {
        case SESSIONOBJECTTYPE_ANONYMOUS:
            rc = VERR_NOT_SUPPORTED;
            break;

        case SESSIONOBJECTTYPE_SESSION:
        {
            alock.release();

            rc = i_dispatchToThis(pCtxCb, pSvcCb);
            break;
        }
        case SESSIONOBJECTTYPE_DIRECTORY:
        {
            SessionDirectories::const_iterator itDir = mData.mDirectories.find(uObjectID);
            if (itDir != mData.mDirectories.end())
            {
                ComObjPtr<GuestDirectory> pDirectory(itDir->second);
                Assert(!pDirectory.isNull());

                alock.release();

                rc = pDirectory->i_callbackDispatcher(pCtxCb, pSvcCb);
            }
            break;
        }
        case SESSIONOBJECTTYPE_FILE:
        {
            SessionFiles::const_iterator itFile = mData.mFiles.find(uObjectID);
            if (itFile != mData.mFiles.end())
            {
                ComObjPtr<GuestFile> pFile(itFile->second);
                Assert(!pFile.isNull());

                alock.release();

                rc = pFile->i_callbackDispatcher(pCtxCb, pSvcCb);
            }
            break;
        }
        case SESSIONOBJECTTYPE_PROCESS:
        {
            SessionProcesses::const_iterator itProc = mData.mProcesses.find(uObjectID);
            if (itProc != mData.mProcesses.end())
            {
                ComObjPtr<GuestProcess> pProcess(itProc->second);
                Assert(!pProcess.isNull());

                alock.release();

                rc = pProcess->i_callbackDispatcher(pCtxCb, pSvcCb);
            }
            break;
        }
        default:
            AssertFailed();
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::i_dispatchToThis(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("sessionID=%RU32, CID=%RU32, uFunction=%RU32, pSvcCb=%p\n",
                     mData.mSession.mID, pCbCtx->uContextID, pCbCtx->uFunction, pSvcCb));
    int rc;
    switch (pCbCtx->uFunction)
    {
        case GUEST_DISCONNECTED:
            /** @todo Handle closing all guest objects. */
            rc = VERR_INTERNAL_ERROR;
            break;

        case GUEST_SESSION_NOTIFY: /* Guest Additions >= 4.3.0. */
        {
            rc = i_onSessionStatusChange(pCbCtx, pSvcCb);
            break;
        }

        default:
            rc = dispatchGeneric(pCbCtx, pSvcCb);
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

inline bool GuestSession::i_fileExists(uint32_t uFileID, ComObjPtr<GuestFile> *pFile)
{
    SessionFiles::const_iterator it = mData.mFiles.find(uFileID);
    if (it != mData.mFiles.end())
    {
        if (pFile)
            *pFile = it->second;
        return true;
    }
    return false;
}

/**
 * Unregisters a file object from a session.
 *
 * @return VBox status code. VERR_NOT_FOUND if the file is not registered (anymore).
 * @param  pFile                File object to unregister from session.
 */
int GuestSession::i_fileUnregister(GuestFile *pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);

    LogFlowThisFunc(("pFile=%p\n", pFile));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    const uint32_t uObjectID = pFile->getObjectID();

    LogFlowFunc(("Removing file (objectID=%RU32) ...\n", uObjectID));

    int rc = i_objectUnregister(uObjectID);
    if (RT_FAILURE(rc))
        return rc;

    SessionFiles::iterator itFiles = mData.mFiles.find(uObjectID);
    AssertReturn(itFiles != mData.mFiles.end(), VERR_NOT_FOUND);

    /* Make sure to consume the pointer before the one of the iterator gets released. */
    ComObjPtr<GuestFile> pFileConsumed = pFile;

    LogFlowFunc(("Removing file ID=%RU32 (session %RU32, now total %zu files)\n",
                 pFileConsumed->getObjectID(), mData.mSession.mID, mData.mFiles.size()));

    rc = pFileConsumed->i_onRemove();
    AssertRCReturn(rc, rc);

    mData.mFiles.erase(itFiles);

    alock.release(); /* Release lock before firing off event. */

    fireGuestFileRegisteredEvent(mEventSource, this, pFileConsumed, false /* Unregistered */);

    pFileConsumed.setNull();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::i_fileRemove(const Utf8Str &strPath, int *prcGuest)
{
    LogFlowThisFunc(("strPath=%s\n", strPath.c_str()));

    int vrc = VINF_SUCCESS;

    GuestProcessStartupInfo procInfo;
    GuestProcessStream      streamOut;

    procInfo.mFlags      = ProcessCreateFlag_WaitForStdOut;
    procInfo.mExecutable = Utf8Str(VBOXSERVICE_TOOL_RM);

    try
    {
        procInfo.mArguments.push_back(procInfo.mExecutable); /* Set argv0. */
        procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
        procInfo.mArguments.push_back("--"); /* strPath could be '--help', which is a valid filename. */
        procInfo.mArguments.push_back(strPath); /* The file we want to remove. */
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(vrc))
        vrc = GuestProcessTool::run(this, procInfo, prcGuest);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::i_fileOpenEx(const com::Utf8Str &aPath, FileAccessMode_T aAccessMode, FileOpenAction_T aOpenAction,
                               FileSharingMode_T aSharingMode, ULONG aCreationMode, const std::vector<FileOpenExFlag_T> &aFlags,
                               ComObjPtr<GuestFile> &pFile, int *prcGuest)
{
    GuestFileOpenInfo openInfo;
    RT_ZERO(openInfo);

    openInfo.mFileName     = aPath;
    openInfo.mCreationMode = aCreationMode;
    openInfo.mAccessMode   = aAccessMode;
    openInfo.mOpenAction   = aOpenAction;
    openInfo.mSharingMode  = aSharingMode;

    /* Combine and validate flags. */
    uint32_t fOpenEx = 0;
    for (size_t i = 0; i < aFlags.size(); i++)
        fOpenEx = aFlags[i];
    if (fOpenEx)
        return VERR_INVALID_PARAMETER; /* FileOpenExFlag not implemented yet. */
    openInfo.mfOpenEx = fOpenEx;

    return i_fileOpen(openInfo, pFile, prcGuest);
}

int GuestSession::i_fileOpen(const GuestFileOpenInfo &openInfo, ComObjPtr<GuestFile> &pFile, int *prcGuest)
{
    LogFlowThisFunc(("strFile=%s, enmAccessMode=0x%x, enmOpenAction=0x%x, uCreationMode=%RU32, mfOpenEx=%RU32\n",
                     openInfo.mFileName.c_str(), openInfo.mAccessMode, openInfo.mOpenAction, openInfo.mCreationMode,
                     openInfo.mfOpenEx));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Guest Additions < 4.3 don't support handling guest files, skip. */
    if (mData.mProtocolVersion < 2)
    {
        if (prcGuest)
            *prcGuest = VERR_NOT_SUPPORTED;
        return VERR_GSTCTL_GUEST_ERROR;
    }

    /* Register a new object ID. */
    uint32_t uObjectID;
    int rc = i_objectRegister(SESSIONOBJECTTYPE_FILE, &uObjectID);
    if (RT_FAILURE(rc))
        return rc;

    /* Create the directory object. */
    HRESULT hr = pFile.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    Console *pConsole = mParent->i_getConsole();
    AssertPtr(pConsole);

    rc = pFile->init(pConsole, this /* GuestSession */, uObjectID, openInfo);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Since this is a synchronous guest call we have to
     * register the file object first, releasing the session's
     * lock and then proceed with the actual opening command
     * -- otherwise the file's opening callback would hang
     * because the session's lock still is in place.
     */
    try
    {
        /* Add the created file to our vector. */
        mData.mFiles[uObjectID] = pFile;

        LogFlowFunc(("Added new guest file \"%s\" (Session: %RU32) (now total %zu files)\n",
                     openInfo.mFileName.c_str(), mData.mSession.mID, mData.mFiles.size()));

        alock.release(); /* Release lock before firing off event. */

        fireGuestFileRegisteredEvent(mEventSource, this, pFile, true /* Registered */);
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        int rcGuest;
        rc = pFile->i_openFile(30 * 1000 /* 30s timeout */, &rcGuest);
        if (   rc == VERR_GSTCTL_GUEST_ERROR
            && prcGuest)
        {
            *prcGuest = rcGuest;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::i_fileQueryInfo(const Utf8Str &strPath, bool fFollowSymlinks, GuestFsObjData &objData, int *prcGuest)
{
    LogFlowThisFunc(("strPath=%s fFollowSymlinks=%RTbool\n", strPath.c_str(), fFollowSymlinks));

    int vrc = i_fsQueryInfo(strPath, fFollowSymlinks, objData, prcGuest);
    if (RT_SUCCESS(vrc))
    {
        vrc = objData.mType == FsObjType_File
            ? VINF_SUCCESS : VERR_NOT_A_FILE;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::i_fileQuerySize(const Utf8Str &strPath, bool fFollowSymlinks, int64_t *pllSize, int *prcGuest)
{
    AssertPtrReturn(pllSize, VERR_INVALID_POINTER);

    GuestFsObjData objData;
    int vrc = i_fileQueryInfo(strPath, fFollowSymlinks, objData, prcGuest);
    if (RT_SUCCESS(vrc))
        *pllSize = objData.mObjectSize;

    return vrc;
}

/**
 * Queries information of a file system object (file, directory, ...).
 *
 * @return  IPRT status code.
 * @param   strPath             Path to file system object to query information for.
 * @param   fFollowSymlinks     Whether to follow symbolic links or not.
 * @param   objData             Where to return the file system object data, if found.
 * @param   prcGuest            Guest rc, when returning VERR_GSTCTL_GUEST_ERROR.
 *                              Any other return code indicates some host side error.
 */
int GuestSession::i_fsQueryInfo(const Utf8Str &strPath, bool fFollowSymlinks, GuestFsObjData &objData, int *prcGuest)
{
    LogFlowThisFunc(("strPath=%s\n", strPath.c_str()));

    /** @todo Merge this with IGuestFile::queryInfo(). */
    GuestProcessStartupInfo procInfo;
    procInfo.mFlags      = ProcessCreateFlag_WaitForStdOut;
    try
    {
        procInfo.mExecutable = Utf8Str(VBOXSERVICE_TOOL_STAT);
        procInfo.mArguments.push_back(procInfo.mExecutable); /* Set argv0. */
        procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
        if (fFollowSymlinks)
            procInfo.mArguments.push_back(Utf8Str("-L"));
        procInfo.mArguments.push_back("--"); /* strPath could be '--help', which is a valid filename. */
        procInfo.mArguments.push_back(strPath);
    }
    catch (std::bad_alloc)
    {
        Log(("Out of memory!\n"));
        return VERR_NO_MEMORY;
    }

    int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    GuestCtrlStreamObjects stdOut;
    int vrc = GuestProcessTool::runEx(this, procInfo,
                                        &stdOut, 1 /* cStrmOutObjects */,
                                        &vrcGuest);
    if (!GuestProcess::i_isGuestError(vrc))
    {
        if (!stdOut.empty())
        {
            vrc = objData.FromStat(stdOut.at(0));
            if (RT_FAILURE(vrc))
            {
                vrcGuest = vrc;
                if (prcGuest)
                    *prcGuest = vrc;
                vrc = VERR_GSTCTL_GUEST_ERROR;
            }
        }
        else
            vrc = VERR_BROKEN_PIPE;
    }
    else if (prcGuest)
        *prcGuest = vrcGuest;

    LogFlowThisFunc(("Returning vrc=%Rrc, vrcGuest=%Rrc\n", vrc, vrcGuest));
    return vrc;
}

const GuestCredentials& GuestSession::i_getCredentials(void)
{
    return mData.mCredentials;
}

Utf8Str GuestSession::i_getName(void)
{
    return mData.mSession.mName;
}

/* static */
Utf8Str GuestSession::i_guestErrorToString(int rcGuest)
{
    Utf8Str strError;

    /** @todo pData->u32Flags: int vs. uint32 -- IPRT errors are *negative* !!! */
    switch (rcGuest)
    {
        case VERR_INVALID_VM_HANDLE:
            strError += Utf8StrFmt(tr("VMM device is not available (is the VM running?)"));
            break;

        case VERR_HGCM_SERVICE_NOT_FOUND:
            strError += Utf8StrFmt(tr("The guest execution service is not available"));
            break;

        case VERR_ACCOUNT_RESTRICTED:
            strError += Utf8StrFmt(tr("The specified user account on the guest is restricted and can't be used to logon"));
            break;

        case VERR_AUTHENTICATION_FAILURE:
            strError += Utf8StrFmt(tr("The specified user was not able to logon on guest"));
            break;

        case VERR_TIMEOUT:
            strError += Utf8StrFmt(tr("The guest did not respond within time"));
            break;

        case VERR_CANCELLED:
            strError += Utf8StrFmt(tr("The session operation was canceled"));
            break;

        case VERR_PERMISSION_DENIED: /** @todo r=bird: This is probably completely and utterly misleading. VERR_AUTHENTICATION_FAILURE could have this message. */
            strError += Utf8StrFmt(tr("Invalid user/password credentials"));
            break;

        case VERR_GSTCTL_MAX_OBJECTS_REACHED:
            strError += Utf8StrFmt(tr("Maximum number of concurrent guest processes has been reached"));
            break;

        case VERR_NOT_FOUND:
            strError += Utf8StrFmt(tr("The guest execution service is not ready (yet)"));
            break;

        default:
            strError += Utf8StrFmt("%Rrc", rcGuest);
            break;
    }

    return strError;
}

/**
 * Checks if this session is ready state where it can handle
 * all session-bound actions (like guest processes, guest files).
 * Only used by official API methods. Will set an external
 * error when not ready.
 */
HRESULT GuestSession::i_isReadyExternal(void)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Be a bit more informative. */
    if (mData.mStatus != GuestSessionStatus_Started)
        return setError(E_UNEXPECTED, tr("Session is not in started state"));

    return S_OK;
}

/**
 * Called by IGuest right before this session gets removed from
 * the public session list.
 */
int GuestSession::i_onRemove(void)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int vrc = VINF_SUCCESS;

    /*
     * Note: The event source stuff holds references to this object,
     *       so make sure that this is cleaned up *before* calling uninit.
     */
    if (!mEventSource.isNull())
    {
        mEventSource->UnregisterListener(mLocalListener);

        mLocalListener.setNull();
        unconst(mEventSource).setNull();
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/** No locking! */
int GuestSession::i_onSessionStatusChange(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    /* pCallback is optional. */
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);

    if (pSvcCbData->mParms < 3)
        return VERR_INVALID_PARAMETER;

    CALLBACKDATA_SESSION_NOTIFY dataCb;
    /* pSvcCb->mpaParms[0] always contains the context ID. */
    int vrc = pSvcCbData->mpaParms[1].getUInt32(&dataCb.uType);
    AssertRCReturn(vrc, vrc);
    vrc = pSvcCbData->mpaParms[2].getUInt32(&dataCb.uResult);
    AssertRCReturn(vrc, vrc);

    LogFlowThisFunc(("ID=%RU32, uType=%RU32, rcGuest=%Rrc\n",
                     mData.mSession.mID, dataCb.uType, dataCb.uResult));

    GuestSessionStatus_T sessionStatus = GuestSessionStatus_Undefined;

    int rcGuest = dataCb.uResult; /** @todo uint32_t vs. int. */
    switch (dataCb.uType)
    {
        case GUEST_SESSION_NOTIFYTYPE_ERROR:
            sessionStatus = GuestSessionStatus_Error;
            break;

        case GUEST_SESSION_NOTIFYTYPE_STARTED:
            sessionStatus = GuestSessionStatus_Started;
#if 0 /** @todo If we get some environment stuff along with this kind notification: */
            const char *pszzEnvBlock = ...;
            uint32_t    cbEnvBlock   = ...;
            if (!mData.mpBaseEnvironment)
            {
                GuestEnvironment *pBaseEnv;
                try { pBaseEnv = new GuestEnvironment(); } catch (std::bad_alloc &) { pBaseEnv = NULL; }
                if (pBaseEnv)
                {
                    int vrc = pBaseEnv->initNormal();
                    if (RT_SUCCESS(vrc))
                        vrc = pBaseEnv->copyUtf8Block(pszzEnvBlock, cbEnvBlock);
                    if (RT_SUCCESS(vrc))
                        mData.mpBaseEnvironment = pBaseEnv;
                    else
                        pBaseEnv->release();
                }
            }
#endif
            break;

        case GUEST_SESSION_NOTIFYTYPE_TEN:
        case GUEST_SESSION_NOTIFYTYPE_TES:
        case GUEST_SESSION_NOTIFYTYPE_TEA:
            sessionStatus = GuestSessionStatus_Terminated;
            break;

        case GUEST_SESSION_NOTIFYTYPE_TOK:
            sessionStatus = GuestSessionStatus_TimedOutKilled;
            break;

        case GUEST_SESSION_NOTIFYTYPE_TOA:
            sessionStatus = GuestSessionStatus_TimedOutAbnormally;
            break;

        case GUEST_SESSION_NOTIFYTYPE_DWN:
            sessionStatus = GuestSessionStatus_Down;
            break;

        case GUEST_SESSION_NOTIFYTYPE_UNDEFINED:
        default:
            vrc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(vrc))
    {
        if (RT_FAILURE(rcGuest))
            sessionStatus = GuestSessionStatus_Error;
    }

    /* Set the session status. */
    if (RT_SUCCESS(vrc))
        vrc = i_setSessionStatus(sessionStatus, rcGuest);

    LogFlowThisFunc(("ID=%RU32, rcGuest=%Rrc\n", mData.mSession.mID, rcGuest));

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

PathStyle_T GuestSession::i_getPathStyle(void)
{
    PathStyle_T enmPathStyle;

    VBOXOSTYPE enmOsType = mParent->i_getGuestOSType();
    if (enmOsType < VBOXOSTYPE_DOS)
    {
        LogFlowFunc(("returns PathStyle_Unknown\n"));
        enmPathStyle = PathStyle_Unknown;
    }
    else if (enmOsType < VBOXOSTYPE_Linux)
    {
        LogFlowFunc(("returns PathStyle_DOS\n"));
        enmPathStyle = PathStyle_DOS;
    }
    else
    {
        LogFlowFunc(("returns PathStyle_UNIX\n"));
        enmPathStyle = PathStyle_UNIX;
    }

    return enmPathStyle;
}

int GuestSession::i_startSession(int *prcGuest)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("mID=%RU32, mName=%s, uProtocolVersion=%RU32, openFlags=%x, openTimeoutMS=%RU32\n",
                     mData.mSession.mID, mData.mSession.mName.c_str(), mData.mProtocolVersion,
                     mData.mSession.mOpenFlags, mData.mSession.mOpenTimeoutMS));

    /* Guest Additions < 4.3 don't support opening dedicated
       guest sessions. Simply return success here. */
    if (mData.mProtocolVersion < 2)
    {
        mData.mStatus = GuestSessionStatus_Started;

        LogFlowThisFunc(("Installed Guest Additions don't support opening dedicated sessions, skipping\n"));
        return VINF_SUCCESS;
    }

    if (mData.mStatus != GuestSessionStatus_Undefined)
        return VINF_SUCCESS;

    /** @todo mData.mSession.uFlags validation. */

    /* Set current session status. */
    mData.mStatus = GuestSessionStatus_Starting;
    mData.mRC = VINF_SUCCESS; /* Clear previous error, if any. */

    int vrc;

    GuestWaitEvent *pEvent = NULL;
    GuestEventTypes eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestSessionStateChanged);

        vrc = registerWaitEventEx(mData.mSession.mID, mData.mObjectID, eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    VBOXHGCMSVCPARM paParms[8];

    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mProtocolVersion);
    paParms[i++].setPointer((void*)mData.mCredentials.mUser.c_str(),
                            (ULONG)mData.mCredentials.mUser.length() + 1);
    paParms[i++].setPointer((void*)mData.mCredentials.mPassword.c_str(),
                            (ULONG)mData.mCredentials.mPassword.length() + 1);
    paParms[i++].setPointer((void*)mData.mCredentials.mDomain.c_str(),
                            (ULONG)mData.mCredentials.mDomain.length() + 1);
    paParms[i++].setUInt32(mData.mSession.mOpenFlags);

    alock.release(); /* Drop write lock before sending. */

    vrc = i_sendCommand(HOST_SESSION_CREATE, i, paParms);
    if (RT_SUCCESS(vrc))
    {
        vrc = i_waitForStatusChange(pEvent, GuestSessionWaitForFlag_Start,
                                    30 * 1000 /* 30s timeout */,
                                    NULL /* Session status */, prcGuest);
    }
    else
    {
        /*
         * Unable to start guest session - update its current state.
         * Since there is no (official API) way to recover a failed guest session
         * this also marks the end state. Internally just calling this
         * same function again will work though.
         */
        mData.mStatus = GuestSessionStatus_Error;
        mData.mRC = vrc;
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::i_startSessionAsync(void)
{
    LogFlowThisFuncEnter();

    int vrc;
    GuestSessionTaskInternalOpen* pTask = NULL;
    try
    {
        pTask = new GuestSessionTaskInternalOpen(this);
        if (!pTask->isOk())
        {
            delete pTask;
            LogFlow(("GuestSession: Could not create GuestSessionTaskInternalOpen object \n"));
            throw VERR_MEMOBJ_INIT_FAILED;
        }

        /* Asynchronously open the session on the guest by kicking off a
         * worker thread. */
        //this function delete pTask in case of exceptions, so there is no need in the call of delete operator
        HRESULT hrc = pTask->createThread();
        vrc = Global::vboxStatusCodeFromCOM(hrc);
    }
    catch(std::bad_alloc &)
    {
        vrc = VERR_NO_MEMORY;
    }
    catch(int eVRC)
    {
        vrc = eVRC;
        LogFlow(("GuestSession: Could not create thread for GuestSessionTaskInternalOpen task %Rrc\n", vrc));
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/* static */
void GuestSession::i_startSessionThreadTask(GuestSessionTaskInternalOpen *pTask)
{
    LogFlowFunc(("pTask=%p\n", pTask));
    AssertPtr(pTask);

    const ComObjPtr<GuestSession> pSession(pTask->Session());
    Assert(!pSession.isNull());

    AutoCaller autoCaller(pSession);
    if (FAILED(autoCaller.rc()))
        return;

    int vrc = pSession->i_startSession(NULL /* Guest rc, ignored */);
/** @todo
 *
 * r=bird: Is it okay to ignore @a vrc here?
 *
 */

    /* Nothing to do here anymore. */

    LogFlowFuncLeaveRC(vrc);
    NOREF(vrc);
}

/**
 * Registers an object to a session.
 *
 * @return VBox status code.
 * @param  enmType              Session object type to register.
 * @param  puObjectID           Returns registered object ID on success. Optional.
 */
int GuestSession::i_objectRegister(SESSIONOBJECTTYPE enmType, uint32_t *puObjectID)
{
    return i_objectRegisterEx(enmType, 0 /* fFlags */, puObjectID);
}

/**
 * Registers an object to a session, extended version.
 *
 * @return VBox status code. VERR_GSTCTL_MAX_OBJECTS_REACHED if the maximum of concurrent objects is reached.
 * @param  enmType              Session object type to register.
 * @param  fFlags               Registration flags. Not used yet and must be 0.
 * @param  puObjectID           Returns registered object ID on success. Optional.
 */
int GuestSession::i_objectRegisterEx(SESSIONOBJECTTYPE enmType, uint32_t fFlags, uint32_t *puObjectID)
{
    RT_NOREF(fFlags);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VINF_SUCCESS;

    uint32_t uObjectID = 0; /* Shut up MSVC. */

    if (mData.mObjects.size() < VBOX_GUESTCTRL_MAX_OBJECTS - 1 /* Minus 1 for the session itself */)
    {
        SessionObjects::reverse_iterator itRend = mData.mObjects.rbegin();
        if (itRend != mData.mObjects.rend())
            uObjectID = itRend->first + 1; /* Last key plus 1. */
        else
            uObjectID = 0;
    }
    else
    {
        /* Utilize our "free list" to get the next free object ID in the map. */
        if (mData.mObjectsFree.size())
        {
            /* Always re-use the oldest object ID to avoid trouble. */
            uObjectID = mData.mObjectsFree.front();
            mData.mObjectsFree.pop_front();
        }
        else
            rc = VERR_GSTCTL_MAX_OBJECTS_REACHED;
    }

    Log2Func(("enmType=%RU32, fFlags=%RU32 -> uObjectID=%RU32 (%zu objects, %zu on free list), rc=%Rrc\n",
              enmType, fFlags, uObjectID, mData.mObjects.size(), mData.mObjectsFree.size(), rc));

    if (RT_SUCCESS(rc))
    {
        mData.mObjects[uObjectID].enmType     = enmType;
        mData.mObjects[uObjectID].tsCreatedMs = RTTimeMilliTS();

        if (puObjectID)
            *puObjectID = uObjectID;
    }

    return rc;
}

/**
 * Unregisters a formerly registered object from a session.
 *
 * @return VBox status code. VERR_NOT_FOUND if object to unregister was not found.
 * @param  uObjectID            Object ID to unregister.
 */
int GuestSession::i_objectUnregister(uint32_t uObjectID)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    SessionObjects::const_iterator itObj = mData.mObjects.find(uObjectID);
    if (itObj != mData.mObjects.end())
    {
        Log2Func(("uObjectID=%RU32 (now %zu objects in free list)\n", uObjectID, mData.mObjectsFree.size()));

        /* Note: Do not remove object from object list (mData.mObjects) here, as we continue operating
         *       on the free list (mData.mObjectsFree) if we reached the object list's maximum. */

        mData.mObjectsFree.push_back(uObjectID);
        Assert(mData.mObjectsFree.size() <= VBOX_GUESTCTRL_MAX_OBJECTS);
        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_NOT_FOUND;
}

int GuestSession::i_pathRename(const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags, int *prcGuest)
{
    AssertReturn(!(uFlags & ~PATHRENAME_FLAG_VALID_MASK), VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("strSource=%s, strDest=%s, uFlags=0x%x\n",
                     strSource.c_str(), strDest.c_str(), uFlags));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    GuestWaitEvent *pEvent = NULL;
    int vrc = registerWaitEvent(mData.mSession.mID, mData.mObjectID, &pEvent);
    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[8];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setPointer((void*)strSource.c_str(),
                            (ULONG)strSource.length() + 1);
    paParms[i++].setPointer((void*)strDest.c_str(),
                            (ULONG)strDest.length() + 1);
    paParms[i++].setUInt32(uFlags);

    alock.release(); /* Drop write lock before sending. */

    vrc = i_sendCommand(HOST_PATH_RENAME, i, paParms);
    if (RT_SUCCESS(vrc))
    {
        vrc = pEvent->Wait(30 * 1000);
        if (   vrc == VERR_GSTCTL_GUEST_ERROR
            && prcGuest)
            *prcGuest = pEvent->GuestResult();
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Returns the user's absolute documents path, if any.
 *
 * @return VBox status code.
 * @param  strPath              Where to store the user's document path.
 * @param  prcGuest             Guest rc, when returning VERR_GSTCTL_GUEST_ERROR.
 *                              Any other return code indicates some host side error.
 */
int GuestSession::i_pathUserDocuments(Utf8Str &strPath, int *prcGuest)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Cache the user's document path? */

    GuestWaitEvent *pEvent = NULL;
    int vrc = registerWaitEvent(mData.mSession.mID, mData.mObjectID, &pEvent);
    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[2];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());

    alock.release(); /* Drop write lock before sending. */

    vrc = i_sendCommand(HOST_PATH_USER_DOCUMENTS, i, paParms);
    if (RT_SUCCESS(vrc))
    {
        vrc = pEvent->Wait(30 * 1000);
        if (RT_SUCCESS(vrc))
        {
            strPath = pEvent->Payload().ToString();
        }
        else
        {
            if (vrc == VERR_GSTCTL_GUEST_ERROR)
            {
                if (prcGuest)
                    *prcGuest = pEvent->GuestResult();
            }
        }
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Returns the user's absolute home path, if any.
 *
 * @return VBox status code.
 * @param  strPath              Where to store the user's home path.
 * @param  prcGuest             Guest rc, when returning VERR_GSTCTL_GUEST_ERROR.
 *                              Any other return code indicates some host side error.
 */
int GuestSession::i_pathUserHome(Utf8Str &strPath, int *prcGuest)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo Cache the user's home path? */

    GuestWaitEvent *pEvent = NULL;
    int vrc = registerWaitEvent(mData.mSession.mID, mData.mObjectID, &pEvent);
    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[2];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());

    alock.release(); /* Drop write lock before sending. */

    vrc = i_sendCommand(HOST_PATH_USER_HOME, i, paParms);
    if (RT_SUCCESS(vrc))
    {
        vrc = pEvent->Wait(30 * 1000);
        if (RT_SUCCESS(vrc))
        {
            strPath = pEvent->Payload().ToString();
        }
        else
        {
            if (vrc == VERR_GSTCTL_GUEST_ERROR)
            {
                if (prcGuest)
                    *prcGuest = pEvent->GuestResult();
            }
        }
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Unregisters a process object from a session.
 *
 * @return VBox status code. VERR_NOT_FOUND if the process is not registered (anymore).
 * @param  pProcess             Process object to unregister from session.
 */
int GuestSession::i_processUnregister(GuestProcess *pProcess)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    LogFlowThisFunc(("pProcess=%p\n", pProcess));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    const uint32_t uObjectID = pProcess->getObjectID();

    LogFlowFunc(("Removing process (objectID=%RU32) ...\n", uObjectID));

    int rc = i_objectUnregister(uObjectID);
    if (RT_FAILURE(rc))
        return rc;

    SessionProcesses::iterator itProcs = mData.mProcesses.find(uObjectID);
    AssertReturn(itProcs != mData.mProcesses.end(), VERR_NOT_FOUND);

    /* Make sure to consume the pointer before the one of the iterator gets released. */
    ComObjPtr<GuestProcess> pProc = pProcess;

    ULONG uPID;
    HRESULT hr = pProc->COMGETTER(PID)(&uPID);
    ComAssertComRC(hr);

    LogFlowFunc(("Removing process ID=%RU32 (session %RU32, guest PID %RU32, now total %zu processes)\n",
                 uObjectID, mData.mSession.mID, uPID, mData.mProcesses.size()));

    rc = pProcess->i_onRemove();
    AssertRCReturn(rc, rc);

    mData.mProcesses.erase(itProcs);

    alock.release(); /* Release lock before firing off event. */

    fireGuestProcessRegisteredEvent(mEventSource, this /* Session */, pProc, uPID, false /* Process unregistered */);

    pProc.setNull();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates but does *not* start the process yet.
 *
 * See GuestProcess::startProcess() or GuestProcess::startProcessAsync() for
 * starting the process.
 *
 * @return  IPRT status code.
 * @param   procInfo
 * @param   pProcess
 */
int GuestSession::i_processCreateEx(GuestProcessStartupInfo &procInfo, ComObjPtr<GuestProcess> &pProcess)
{
    LogFlowFunc(("mExe=%s, mFlags=%x, mTimeoutMS=%RU32\n",
                 procInfo.mExecutable.c_str(), procInfo.mFlags, procInfo.mTimeoutMS));
#ifdef DEBUG
    if (procInfo.mArguments.size())
    {
        LogFlowFunc(("Arguments:"));
        ProcessArguments::const_iterator it = procInfo.mArguments.begin();
        while (it != procInfo.mArguments.end())
        {
            LogFlow((" %s", (*it).c_str()));
            ++it;
        }
        LogFlow(("\n"));
    }
#endif

    /* Validate flags. */
    if (procInfo.mFlags)
    {
        if (   !(procInfo.mFlags & ProcessCreateFlag_IgnoreOrphanedProcesses)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForProcessStartOnly)
            && !(procInfo.mFlags & ProcessCreateFlag_Hidden)
            && !(procInfo.mFlags & ProcessCreateFlag_Profile)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForStdOut)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForStdErr))
        {
            return VERR_INVALID_PARAMETER;
        }
    }

    if (   (procInfo.mFlags & ProcessCreateFlag_WaitForProcessStartOnly)
        && (   (procInfo.mFlags & ProcessCreateFlag_WaitForStdOut)
            || (procInfo.mFlags & ProcessCreateFlag_WaitForStdErr)
           )
       )
    {
        return VERR_INVALID_PARAMETER;
    }

    /* Adjust timeout.
     * If set to 0, we define an infinite timeout (unlimited process run time). */
    if (procInfo.mTimeoutMS == 0)
        procInfo.mTimeoutMS = UINT32_MAX;

    /** @todo Implement process priority + affinity. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Register a new object ID. */
    uint32_t uObjectID;
    int rc = i_objectRegister(SESSIONOBJECTTYPE_PROCESS, &uObjectID);
    if (RT_FAILURE(rc))
        return rc;

    /* Create the process object. */
    HRESULT hr = pProcess.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    rc = pProcess->init(mParent->i_getConsole() /* Console */, this /* Session */, uObjectID,
                        procInfo, mData.mpBaseEnvironment);
    if (RT_FAILURE(rc))
        return rc;

    /* Add the created process to our map. */
    try
    {
        mData.mProcesses[uObjectID] = pProcess;

        LogFlowFunc(("Added new process (Session: %RU32) with process ID=%RU32 (now total %zu processes)\n",
                     mData.mSession.mID, uObjectID, mData.mProcesses.size()));

        alock.release(); /* Release lock before firing off event. */

        fireGuestProcessRegisteredEvent(mEventSource, this /* Session */, pProcess, 0 /* PID */, true /* Process registered */);
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

inline bool GuestSession::i_processExists(uint32_t uProcessID, ComObjPtr<GuestProcess> *pProcess)
{
    SessionProcesses::const_iterator it = mData.mProcesses.find(uProcessID);
    if (it != mData.mProcesses.end())
    {
        if (pProcess)
            *pProcess = it->second;
        return true;
    }
    return false;
}

inline int GuestSession::i_processGetByPID(ULONG uPID, ComObjPtr<GuestProcess> *pProcess)
{
    AssertReturn(uPID, false);
    /* pProcess is optional. */

    SessionProcesses::iterator itProcs = mData.mProcesses.begin();
    for (; itProcs != mData.mProcesses.end(); ++itProcs)
    {
        ComObjPtr<GuestProcess> pCurProc = itProcs->second;
        AutoCaller procCaller(pCurProc);
        if (procCaller.rc())
            return VERR_COM_INVALID_OBJECT_STATE;

        ULONG uCurPID;
        HRESULT hr = pCurProc->COMGETTER(PID)(&uCurPID);
        ComAssertComRC(hr);

        if (uCurPID == uPID)
        {
            if (pProcess)
                *pProcess = pCurProc;
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::i_sendCommand(uint32_t uFunction,
                                uint32_t uParms, PVBOXHGCMSVCPARM paParms)
{
    LogFlowThisFuncEnter();

#ifndef VBOX_GUESTCTRL_TEST_CASE
    ComObjPtr<Console> pConsole = mParent->i_getConsole();
    Assert(!pConsole.isNull());

    /* Forward the information to the VMM device. */
    VMMDev *pVMMDev = pConsole->i_getVMMDev();
    AssertPtr(pVMMDev);

    LogFlowThisFunc(("uFunction=%RU32, uParms=%RU32\n", uFunction, uParms));
    int vrc = pVMMDev->hgcmHostCall(HGCMSERVICE_NAME, uFunction, uParms, paParms);
    if (RT_FAILURE(vrc))
    {
        /** @todo What to do here? */
    }
#else
    /* Not needed within testcases. */
    int vrc = VINF_SUCCESS;
#endif
    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/* static */
HRESULT GuestSession::i_setErrorExternal(VirtualBoxBase *pInterface, int rcGuest)
{
    AssertPtr(pInterface);
    AssertMsg(RT_FAILURE(rcGuest), ("Guest rc does not indicate a failure when setting error\n"));

    return pInterface->setError(VBOX_E_IPRT_ERROR, GuestSession::i_guestErrorToString(rcGuest).c_str());
}

/* Does not do locking; caller is responsible for that! */
int GuestSession::i_setSessionStatus(GuestSessionStatus_T sessionStatus, int sessionRc)
{
    LogFlowThisFunc(("oldStatus=%RU32, newStatus=%RU32, sessionRc=%Rrc\n",
                     mData.mStatus, sessionStatus, sessionRc));

    if (sessionStatus == GuestSessionStatus_Error)
    {
        AssertMsg(RT_FAILURE(sessionRc), ("Guest rc must be an error (%Rrc)\n", sessionRc));
        /* Do not allow overwriting an already set error. If this happens
         * this means we forgot some error checking/locking somewhere. */
        AssertMsg(RT_SUCCESS(mData.mRC), ("Guest rc already set (to %Rrc)\n", mData.mRC));
    }
    else
        AssertMsg(RT_SUCCESS(sessionRc), ("Guest rc must not be an error (%Rrc)\n", sessionRc));

    if (mData.mStatus != sessionStatus)
    {
        mData.mStatus = sessionStatus;
        mData.mRC     = sessionRc;

        ComObjPtr<VirtualBoxErrorInfo> errorInfo;
        HRESULT hr = errorInfo.createObject();
        ComAssertComRC(hr);
        int rc2 = errorInfo->initEx(VBOX_E_IPRT_ERROR, sessionRc,
                                    COM_IIDOF(IGuestSession), getComponentName(),
                                    i_guestErrorToString(sessionRc));
        AssertRC(rc2);

        fireGuestSessionStateChangedEvent(mEventSource, this,
                                          mData.mSession.mID, sessionStatus, errorInfo);
    }

    return VINF_SUCCESS;
}

int GuestSession::i_signalWaiters(GuestSessionWaitResult_T enmWaitResult, int rc /*= VINF_SUCCESS */)
{
    RT_NOREF(enmWaitResult, rc);

    /*LogFlowThisFunc(("enmWaitResult=%d, rc=%Rrc, mWaitCount=%RU32, mWaitEvent=%p\n",
                     enmWaitResult, rc, mData.mWaitCount, mData.mWaitEvent));*/

    /* Note: No write locking here -- already done in the caller. */

    int vrc = VINF_SUCCESS;
    /*if (mData.mWaitEvent)
        vrc = mData.mWaitEvent->Signal(enmWaitResult, rc);*/
    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Determines the protocol version (sets mData.mProtocolVersion).
 *
 * This is called from the init method prior to to establishing a guest
 * session.
 *
 * @return  IPRT status code.
 */
int GuestSession::i_determineProtocolVersion(void)
{
    /*
     * We currently do this based on the reported guest additions version,
     * ASSUMING that VBoxService and VBoxDrv are at the same version.
     */
    ComObjPtr<Guest> pGuest = mParent;
    AssertReturn(!pGuest.isNull(), VERR_NOT_SUPPORTED);
    uint32_t uGaVersion = pGuest->i_getAdditionsVersion();

    /* Everyone supports version one, if they support anything at all. */
    mData.mProtocolVersion = 1;

    /* Guest control 2.0 was introduced with 4.3.0. */
    if (uGaVersion >= VBOX_FULL_VERSION_MAKE(4,3,0))
        mData.mProtocolVersion = 2; /* Guest control 2.0. */

    LogFlowThisFunc(("uGaVersion=%u.%u.%u => mProtocolVersion=%u\n",
                     VBOX_FULL_VERSION_GET_MAJOR(uGaVersion), VBOX_FULL_VERSION_GET_MINOR(uGaVersion),
                     VBOX_FULL_VERSION_GET_BUILD(uGaVersion), mData.mProtocolVersion));

    /*
     * Inform the user about outdated guest additions (VM release log).
     */
    if (mData.mProtocolVersion < 2)
        LogRelMax(3, (tr("Warning: Guest Additions v%u.%u.%u only supports the older guest control protocol version %u.\n"
                         "         Please upgrade GAs to the current version to get full guest control capabilities.\n"),
                      VBOX_FULL_VERSION_GET_MAJOR(uGaVersion), VBOX_FULL_VERSION_GET_MINOR(uGaVersion),
                      VBOX_FULL_VERSION_GET_BUILD(uGaVersion), mData.mProtocolVersion));

    return VINF_SUCCESS;
}

int GuestSession::i_waitFor(uint32_t fWaitFlags, ULONG uTimeoutMS, GuestSessionWaitResult_T &waitResult, int *prcGuest)
{
    LogFlowThisFuncEnter();

    AssertReturn(fWaitFlags, VERR_INVALID_PARAMETER);

    /*LogFlowThisFunc(("fWaitFlags=0x%x, uTimeoutMS=%RU32, mStatus=%RU32, mWaitCount=%RU32, mWaitEvent=%p, prcGuest=%p\n",
                     fWaitFlags, uTimeoutMS, mData.mStatus, mData.mWaitCount, mData.mWaitEvent, prcGuest));*/

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Did some error occur before? Then skip waiting and return. */
    if (mData.mStatus == GuestSessionStatus_Error)
    {
        waitResult = GuestSessionWaitResult_Error;
        AssertMsg(RT_FAILURE(mData.mRC), ("No error rc (%Rrc) set when guest session indicated an error\n", mData.mRC));
        if (prcGuest)
            *prcGuest = mData.mRC; /* Return last set error. */
        return VERR_GSTCTL_GUEST_ERROR;
    }

    /* Guest Additions < 4.3 don't support session handling, skip. */
    if (mData.mProtocolVersion < 2)
    {
        waitResult = GuestSessionWaitResult_WaitFlagNotSupported;

        LogFlowThisFunc(("Installed Guest Additions don't support waiting for dedicated sessions, skipping\n"));
        return VINF_SUCCESS;
    }

    waitResult = GuestSessionWaitResult_None;
    if (fWaitFlags & GuestSessionWaitForFlag_Terminate)
    {
        switch (mData.mStatus)
        {
            case GuestSessionStatus_Terminated:
            case GuestSessionStatus_Down:
                waitResult = GuestSessionWaitResult_Terminate;
                break;

            case GuestSessionStatus_TimedOutKilled:
            case GuestSessionStatus_TimedOutAbnormally:
                waitResult = GuestSessionWaitResult_Timeout;
                break;

            case GuestSessionStatus_Error:
                /* Handled above. */
                break;

            case GuestSessionStatus_Started:
                waitResult = GuestSessionWaitResult_Start;
                break;

            case GuestSessionStatus_Undefined:
            case GuestSessionStatus_Starting:
                /* Do the waiting below. */
                break;

            default:
                AssertMsgFailed(("Unhandled session status %RU32\n", mData.mStatus));
                return VERR_NOT_IMPLEMENTED;
        }
    }
    else if (fWaitFlags & GuestSessionWaitForFlag_Start)
    {
        switch (mData.mStatus)
        {
            case GuestSessionStatus_Started:
            case GuestSessionStatus_Terminating:
            case GuestSessionStatus_Terminated:
            case GuestSessionStatus_Down:
                waitResult = GuestSessionWaitResult_Start;
                break;

            case GuestSessionStatus_Error:
                waitResult = GuestSessionWaitResult_Error;
                break;

            case GuestSessionStatus_TimedOutKilled:
            case GuestSessionStatus_TimedOutAbnormally:
                waitResult = GuestSessionWaitResult_Timeout;
                break;

            case GuestSessionStatus_Undefined:
            case GuestSessionStatus_Starting:
                /* Do the waiting below. */
                break;

            default:
                AssertMsgFailed(("Unhandled session status %RU32\n", mData.mStatus));
                return VERR_NOT_IMPLEMENTED;
        }
    }

    LogFlowThisFunc(("sessionStatus=%RU32, sessionRc=%Rrc, waitResult=%RU32\n",
                     mData.mStatus, mData.mRC, waitResult));

    /* No waiting needed? Return immediately using the last set error. */
    if (waitResult != GuestSessionWaitResult_None)
    {
        if (prcGuest)
            *prcGuest = mData.mRC; /* Return last set error (if any). */
        return RT_SUCCESS(mData.mRC) ? VINF_SUCCESS : VERR_GSTCTL_GUEST_ERROR;
    }

    int vrc;

    GuestWaitEvent *pEvent = NULL;
    GuestEventTypes eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestSessionStateChanged);

        vrc = registerWaitEventEx(mData.mSession.mID, mData.mObjectID, eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    alock.release(); /* Release lock before waiting. */

    GuestSessionStatus_T sessionStatus;
    vrc = i_waitForStatusChange(pEvent, fWaitFlags,
                                uTimeoutMS, &sessionStatus, prcGuest);
    if (RT_SUCCESS(vrc))
    {
        switch (sessionStatus)
        {
            case GuestSessionStatus_Started:
                waitResult = GuestSessionWaitResult_Start;
                break;

            case GuestSessionStatus_Terminated:
                waitResult = GuestSessionWaitResult_Terminate;
                break;

            case GuestSessionStatus_TimedOutKilled:
            case GuestSessionStatus_TimedOutAbnormally:
                waitResult = GuestSessionWaitResult_Timeout;
                break;

            case GuestSessionStatus_Down:
                waitResult = GuestSessionWaitResult_Terminate;
                break;

            case GuestSessionStatus_Error:
                waitResult = GuestSessionWaitResult_Error;
                break;

            default:
                waitResult = GuestSessionWaitResult_Status;
                break;
        }
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::i_waitForStatusChange(GuestWaitEvent *pEvent, uint32_t fWaitFlags, uint32_t uTimeoutMS,
                                        GuestSessionStatus_T *pSessionStatus, int *prcGuest)
{
    RT_NOREF(fWaitFlags);
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    VBoxEventType_T evtType;
    ComPtr<IEvent> pIEvent;
    int vrc = waitForEvent(pEvent, uTimeoutMS,
                           &evtType, pIEvent.asOutParam());
    if (RT_SUCCESS(vrc))
    {
        Assert(evtType == VBoxEventType_OnGuestSessionStateChanged);

        ComPtr<IGuestSessionStateChangedEvent> pChangedEvent = pIEvent;
        Assert(!pChangedEvent.isNull());

        GuestSessionStatus_T sessionStatus;
        pChangedEvent->COMGETTER(Status)(&sessionStatus);
        if (pSessionStatus)
            *pSessionStatus = sessionStatus;

        ComPtr<IVirtualBoxErrorInfo> errorInfo;
        HRESULT hr = pChangedEvent->COMGETTER(Error)(errorInfo.asOutParam());
        ComAssertComRC(hr);

        LONG lGuestRc;
        hr = errorInfo->COMGETTER(ResultDetail)(&lGuestRc);
        ComAssertComRC(hr);
        if (RT_FAILURE((int)lGuestRc))
            vrc = VERR_GSTCTL_GUEST_ERROR;
        if (prcGuest)
            *prcGuest = (int)lGuestRc;

        LogFlowThisFunc(("Status changed event for session ID=%RU32, new status is: %RU32 (%Rrc)\n",
                         mData.mSession.mID, sessionStatus,
                         RT_SUCCESS((int)lGuestRc) ? VINF_SUCCESS : (int)lGuestRc));
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestSession::close()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    LogFlowThisFuncEnter();

    /* Note: Don't check if the session is ready via i_isReadyExternal() here;
     *       the session (already) could be in a stopped / aborted state. */

    /* Close session on guest. */
    int rcGuest = VINF_SUCCESS;
    int rc = i_closeSession(0 /* Flags */, 30 * 1000 /* Timeout */, &rcGuest);
    /* On failure don't return here, instead do all the cleanup
     * work first and then return an error. */

    /* Remove ourselves from the session list. */
    AssertPtr(mParent);
    int rc2 = mParent->i_sessionRemove(mData.mSession.mID);
    if (rc2 == VERR_NOT_FOUND) /* Not finding the session anymore isn't critical. */
        rc2 = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowThisFunc(("Returning rc=%Rrc, rcGuest=%Rrc\n", rc, rcGuest));

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_GSTCTL_GUEST_ERROR)
            return GuestSession::i_setErrorExternal(this, rcGuest);

        return setError(VBOX_E_IPRT_ERROR,
                        tr("Closing guest session failed with %Rrc"), rc);
    }

    return S_OK;
}

HRESULT GuestSession::fileCopy(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                               const std::vector<FileCopyFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aSource, aDestination, aFlags, aProgress);
    ReturnComNotImplemented();
}

HRESULT GuestSession::fileCopyFromGuest(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                                        const std::vector<FileCopyFlag_T> &aFlags,
                                        ComPtr<IProgress> &aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = FileCopyFlag_None;
    if (aFlags.size())
    {
        for (size_t i = 0; i < aFlags.size(); i++)
            fFlags |= aFlags[i];
    }

    GuestSessionFsSourceSet SourceSet;

    GuestSessionFsSourceSpec source;
    source.strSource            = aSource;
    source.enmType              = FsObjType_File;
    source.enmPathStyle         = i_getPathStyle();
    source.Type.File.fCopyFlags = (FileCopyFlag_T)fFlags;

    SourceSet.push_back(source);

    return i_copyFromGuest(SourceSet, aDestination, aProgress);
}

HRESULT GuestSession::fileCopyToGuest(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                                      const std::vector<FileCopyFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = FileCopyFlag_None;
    if (aFlags.size())
    {
        for (size_t i = 0; i < aFlags.size(); i++)
            fFlags |= aFlags[i];
    }

    GuestSessionFsSourceSet SourceSet;

    GuestSessionFsSourceSpec source;
    source.strSource            = aSource;
    source.enmType              = FsObjType_File;
    source.enmPathStyle         = i_getPathStyle();
    source.Type.File.fCopyFlags = (FileCopyFlag_T)fFlags;

    SourceSet.push_back(source);

    return i_copyToGuest(SourceSet, aDestination, aProgress);
}

HRESULT GuestSession::copyFromGuest(const std::vector<com::Utf8Str> &aSources, const std::vector<com::Utf8Str> &aFilters,
                                    const std::vector<FsObjType_T> &aTypes, const std::vector<GuestCopyFlag_T> &aFlags,
                                    const com::Utf8Str &aDestination, ComPtr<IProgress> &aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    const size_t cSources = aSources.size();
    if (   aFilters.size() != cSources
        || aTypes.size()   != cSources
        || aFlags.size()   != cSources)
    {
        return setError(E_INVALIDARG, tr("Parameter array sizes don't match to the number of sources specified"));
    }

    GuestSessionFsSourceSet SourceSet;

    std::vector<com::Utf8Str>::const_iterator itSource  = aSources.begin();
    std::vector<com::Utf8Str>::const_iterator itFilter  = aFilters.begin();
    std::vector<FsObjType_T>::const_iterator itType     = aTypes.begin();
    std::vector<GuestCopyFlag_T>::const_iterator itFlag = aFlags.begin();

    while (itSource != aSources.end())
    {
        GuestSessionFsSourceSpec source;
        source.strSource    = *itSource;
        source.strFilter    = *itFilter;
        source.enmType      = *itType;
        source.enmPathStyle = i_getPathStyle();

        if (*itType == FsObjType_Directory)
        {
            source.Type.Dir.fCopyFlags = (DirectoryCopyFlag_T)*itFlag;
            source.Type.Dir.fRecursive = true; /* Implicit. */
        }
        else
        {
            source.Type.File.fCopyFlags = (FileCopyFlag_T)*itFlag;
        }

        SourceSet.push_back(source);

        ++itSource;
        ++itFilter;
        ++itType;
        ++itFlag;
    }

    return i_copyFromGuest(SourceSet, aDestination, aProgress);
}

HRESULT GuestSession::copyToGuest(const std::vector<com::Utf8Str> &aSources, const std::vector<com::Utf8Str> &aFilters,
                                  const std::vector<FsObjType_T> &aTypes, const std::vector<GuestCopyFlag_T> &aFlags,
                                  const com::Utf8Str &aDestination, ComPtr<IProgress> &aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    const size_t cSources = aSources.size();
    if (   aFilters.size() != cSources
        || aTypes.size()   != cSources
        || aFlags.size()   != cSources)
    {
        return setError(E_INVALIDARG, tr("Parameter array sizes don't match to the number of sources specified"));
    }

    GuestSessionFsSourceSet SourceSet;

    std::vector<com::Utf8Str>::const_iterator itSource  = aSources.begin();
    std::vector<com::Utf8Str>::const_iterator itFilter  = aFilters.begin();
    std::vector<FsObjType_T>::const_iterator itType     = aTypes.begin();
    std::vector<GuestCopyFlag_T>::const_iterator itFlag = aFlags.begin();

    while (itSource != aSources.end())
    {

        GuestSessionFsSourceSpec source;
        source.strSource    = *itSource;
        source.strFilter    = *itFilter;
        source.enmType      = *itType;
        source.enmPathStyle = i_getPathStyle();

        if (*itType == FsObjType_Directory)
        {
            source.Type.Dir.fCopyFlags = (DirectoryCopyFlag_T)*itFlag;
            source.Type.Dir.fRecursive = true; /* Implicit. */
        }
        else
            source.Type.File.fCopyFlags = (FileCopyFlag_T)*itFlag;

        SourceSet.push_back(source);

        ++itSource;
        ++itFilter;
        ++itType;
        ++itFlag;
    }

    return i_copyToGuest(SourceSet, aDestination, aProgress);
}

HRESULT GuestSession::directoryCopy(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                                    const std::vector<DirectoryCopyFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aSource, aDestination, aFlags, aProgress);
    ReturnComNotImplemented();
}

HRESULT GuestSession::directoryCopyFromGuest(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                                             const std::vector<DirectoryCopyFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = DirectoryCopyFlag_None;
    if (aFlags.size())
    {
        for (size_t i = 0; i < aFlags.size(); i++)
            fFlags |= aFlags[i];
    }

    GuestSessionFsSourceSet SourceSet;

    GuestSessionFsSourceSpec source;
    source.strSource            = aSource;
    source.enmType              = FsObjType_Directory;
    source.enmPathStyle         = i_getPathStyle();
    source.Type.Dir.fCopyFlags  = (DirectoryCopyFlag_T)fFlags;
    source.Type.Dir.fRecursive  = true; /* Implicit. */

    SourceSet.push_back(source);

    return i_copyFromGuest(SourceSet, aDestination, aProgress);
}

HRESULT GuestSession::directoryCopyToGuest(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                                           const std::vector<DirectoryCopyFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = DirectoryCopyFlag_None;
    if (aFlags.size())
    {
        for (size_t i = 0; i < aFlags.size(); i++)
            fFlags |= aFlags[i];
    }

    GuestSessionFsSourceSet SourceSet;

    GuestSessionFsSourceSpec source;
    source.strSource           = aSource;
    source.enmType             = FsObjType_Directory;
    source.enmPathStyle        = i_getPathStyle();
    source.Type.Dir.fCopyFlags = (DirectoryCopyFlag_T)fFlags;
    source.Type.Dir.fRecursive = true; /* Implicit. */

    SourceSet.push_back(source);

    return i_copyToGuest(SourceSet, aDestination, aProgress);
}

HRESULT GuestSession::directoryCreate(const com::Utf8Str &aPath, ULONG aMode,
                                      const std::vector<DirectoryCreateFlag_T> &aFlags)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to create specified"));

    uint32_t fFlags = DirectoryCreateFlag_None;
    if (aFlags.size())
    {
        for (size_t i = 0; i < aFlags.size(); i++)
            fFlags |= aFlags[i];

        if (fFlags)
            if (!(fFlags & DirectoryCreateFlag_Parents))
                return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), fFlags);
    }

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    ComObjPtr <GuestDirectory> pDirectory; int rcGuest;
    int rc = i_directoryCreate(aPath, (uint32_t)aMode, fFlags, &rcGuest);
    if (RT_FAILURE(rc))
    {
        if (GuestProcess::i_isGuestError(rc))
        {
            hrc = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: %s"),
                                                 GuestDirectory::i_guestErrorToString(rcGuest).c_str());
        }
        else
        {
            switch (rc)
            {
                case VERR_INVALID_PARAMETER:
                    hrc = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Invalid parameters given"));
                    break;

                case VERR_BROKEN_PIPE:
                    hrc = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Unexpectedly aborted"));
                    break;

                default:
                    hrc = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: %Rrc"), rc);
                    break;
            }
        }
    }

    return hrc;
}

HRESULT GuestSession::directoryCreateTemp(const com::Utf8Str &aTemplateName, ULONG aMode, const com::Utf8Str &aPath,
                                          BOOL aSecure, com::Utf8Str &aDirectory)
{
    RT_NOREF(aMode, aSecure);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aTemplateName.c_str()) == NULL || *(aTemplateName.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No template specified"));
    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No directory name specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    int rcGuest;
    int rc = i_fsCreateTemp(aTemplateName, aPath, true /* Directory */, aDirectory, &rcGuest);
    if (!RT_SUCCESS(rc))
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                hrc = GuestProcess::i_setErrorExternal(this, rcGuest);
                break;

            default:
               hrc = setError(VBOX_E_IPRT_ERROR, tr("Temporary directory creation \"%s\" with template \"%s\" failed: %Rrc"),
                              aPath.c_str(), aTemplateName.c_str(), rc);
               break;
        }
    }

    return hrc;
}

HRESULT GuestSession::directoryExists(const com::Utf8Str &aPath, BOOL aFollowSymlinks, BOOL *aExists)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to check existence for specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    GuestFsObjData objData; int rcGuest;
    int rc = i_directoryQueryInfo(aPath, aFollowSymlinks != FALSE, objData, &rcGuest);
    if (RT_SUCCESS(rc))
        *aExists = objData.mType == FsObjType_Directory;
    else
    {
        switch (rc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
            {
                switch (rcGuest)
                {
                    case VERR_PATH_NOT_FOUND:
                        *aExists = FALSE;
                        break;
                    default:
                        hrc = setError(VBOX_E_IPRT_ERROR, tr("Querying directory existence \"%s\" failed: %s"),
                                                             aPath.c_str(), GuestProcess::i_guestErrorToString(rcGuest).c_str());
                        break;
                }
                break;
            }

            default:
               hrc = setError(VBOX_E_IPRT_ERROR, tr("Querying directory existence \"%s\" failed: %Rrc"),
                                                    aPath.c_str(), rc);
               break;
        }
    }

    return hrc;
}

HRESULT GuestSession::directoryOpen(const com::Utf8Str &aPath, const com::Utf8Str &aFilter,
                                    const std::vector<DirectoryOpenFlag_T> &aFlags, ComPtr<IGuestDirectory> &aDirectory)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to open specified"));
    if (RT_UNLIKELY((aFilter.c_str()) != NULL && *(aFilter.c_str()) != '\0'))
        return setError(E_INVALIDARG, tr("Directory filters are not implemented yet"));

    uint32_t fFlags = DirectoryOpenFlag_None;
    if (aFlags.size())
    {
        for (size_t i = 0; i < aFlags.size(); i++)
            fFlags |= aFlags[i];

        if (fFlags)
            return setError(E_INVALIDARG, tr("Open flags (%#x) not implemented yet"), fFlags);
    }

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    GuestDirectoryOpenInfo openInfo;
    openInfo.mPath = aPath;
    openInfo.mFilter = aFilter;
    openInfo.mFlags = fFlags;

    ComObjPtr <GuestDirectory> pDirectory; int rcGuest;
    int rc = i_directoryOpen(openInfo, pDirectory, &rcGuest);
    if (RT_SUCCESS(rc))
    {
        /* Return directory object to the caller. */
        hrc = pDirectory.queryInterfaceTo(aDirectory.asOutParam());
    }
    else
    {
        switch (rc)
        {
            case VERR_INVALID_PARAMETER:
               hrc = setError(VBOX_E_IPRT_ERROR, tr("Opening directory \"%s\" failed; invalid parameters given"),
                                                    aPath.c_str());
               break;

            case VERR_GSTCTL_GUEST_ERROR:
                hrc = GuestDirectory::i_setErrorExternal(this, rcGuest);
                break;

            default:
               hrc = setError(VBOX_E_IPRT_ERROR, tr("Opening directory \"%s\" failed: %Rrc"),
                                                    aPath.c_str(),rc);
               break;
        }
    }

    return hrc;
}

HRESULT GuestSession::directoryRemove(const com::Utf8Str &aPath)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to remove specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    /* No flags; only remove the directory when empty. */
    uint32_t uFlags = 0;

    int rcGuest;
    int vrc = i_directoryRemove(aPath, uFlags, &rcGuest);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_NOT_SUPPORTED:
                hrc = setError(VBOX_E_IPRT_ERROR,
                               tr("Handling removing guest directories not supported by installed Guest Additions"));
                break;

            case VERR_GSTCTL_GUEST_ERROR:
                hrc = GuestDirectory::i_setErrorExternal(this, rcGuest);
                break;

            default:
                hrc = setError(VBOX_E_IPRT_ERROR, tr("Removing guest directory \"%s\" failed: %Rrc"),
                                                     aPath.c_str(), vrc);
                break;
        }
    }

    return hrc;
}

HRESULT GuestSession::directoryRemoveRecursive(const com::Utf8Str &aPath, const std::vector<DirectoryRemoveRecFlag_T> &aFlags,
                                               ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aFlags);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to remove recursively specified"));

/** @todo r=bird: Must check that the flags matches the hardcoded behavior
 *        further down!! */

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    ComObjPtr<Progress> pProgress;
    hrc = pProgress.createObject();
    if (SUCCEEDED(hrc))
        hrc = pProgress->init(static_cast<IGuestSession *>(this),
                              Bstr(tr("Removing guest directory")).raw(),
                              TRUE /*aCancelable*/);
    if (FAILED(hrc))
        return hrc;

    /* Note: At the moment we don't supply progress information while
     *       deleting a guest directory recursively. So just complete
     *       the progress object right now. */
     /** @todo Implement progress reporting on guest directory deletion! */
    hrc = pProgress->i_notifyComplete(S_OK);
    if (FAILED(hrc))
        return hrc;

    /* Remove the directory + all its contents. */
    uint32_t uFlags = DIRREMOVE_FLAG_RECURSIVE
                    | DIRREMOVE_FLAG_CONTENT_AND_DIR;
    int rcGuest;
    int vrc = i_directoryRemove(aPath, uFlags, &rcGuest);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_NOT_SUPPORTED:
                hrc = setError(VBOX_E_IPRT_ERROR,
                              tr("Handling removing guest directories recursively not supported by installed Guest Additions"));
                break;

            case VERR_GSTCTL_GUEST_ERROR:
                hrc = GuestFile::i_setErrorExternal(this, rcGuest);
                break;

            default:
                hrc = setError(VBOX_E_IPRT_ERROR, tr("Recursively removing guest directory \"%s\" failed: %Rrc"),
                                                     aPath.c_str(), vrc);
                break;
        }
    }
    else
    {
        pProgress.queryInterfaceTo(aProgress.asOutParam());
    }

    return hrc;
}

HRESULT GuestSession::environmentScheduleSet(const com::Utf8Str &aName, const com::Utf8Str &aValue)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hrc;
    if (RT_LIKELY(aName.isNotEmpty()))
    {
        if (RT_LIKELY(strchr(aName.c_str(), '=') == NULL))
        {
            LogFlowThisFuncEnter();

            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            int vrc = mData.mEnvironmentChanges.setVariable(aName, aValue);
            if (RT_SUCCESS(vrc))
                hrc = S_OK;
            else
                hrc = setErrorVrc(vrc);
        }
        else
            hrc = setError(E_INVALIDARG, tr("The equal char is not allowed in environment variable names"));
    }
    else
        hrc = setError(E_INVALIDARG, tr("No variable name specified"));

    LogFlowThisFuncLeave();
    return hrc;
}

HRESULT GuestSession::environmentScheduleUnset(const com::Utf8Str &aName)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hrc;
    if (RT_LIKELY(aName.isNotEmpty()))
    {
        if (RT_LIKELY(strchr(aName.c_str(), '=') == NULL))
        {
            LogFlowThisFuncEnter();

            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            int vrc = mData.mEnvironmentChanges.unsetVariable(aName);
            if (RT_SUCCESS(vrc))
                hrc = S_OK;
            else
                hrc = setErrorVrc(vrc);
        }
        else
            hrc = setError(E_INVALIDARG, tr("The equal char is not allowed in environment variable names"));
    }
    else
        hrc = setError(E_INVALIDARG, tr("No variable name specified"));

    LogFlowThisFuncLeave();
    return hrc;
}

HRESULT GuestSession::environmentGetBaseVariable(const com::Utf8Str &aName, com::Utf8Str &aValue)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hrc;
    if (RT_LIKELY(aName.isNotEmpty()))
    {
        if (RT_LIKELY(strchr(aName.c_str(), '=') == NULL))
        {
            LogFlowThisFuncEnter();

            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
            if (mData.mpBaseEnvironment)
            {
                int vrc = mData.mpBaseEnvironment->getVariable(aName, &aValue);
                if (RT_SUCCESS(vrc))
                    hrc = S_OK;
                else
                    hrc = setErrorVrc(vrc);
            }
            else if (mData.mProtocolVersion < 99999)
                hrc = setError(VBOX_E_NOT_SUPPORTED, tr("The base environment feature is not supported by the guest additions"));
            else
                hrc = setError(VBOX_E_INVALID_OBJECT_STATE, tr("The base environment has not yet been reported by the guest"));
        }
        else
            hrc = setError(E_INVALIDARG, tr("The equal char is not allowed in environment variable names"));
    }
    else
        hrc = setError(E_INVALIDARG, tr("No variable name specified"));

    LogFlowThisFuncLeave();
    return hrc;
}

HRESULT GuestSession::environmentDoesBaseVariableExist(const com::Utf8Str &aName, BOOL *aExists)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aExists = FALSE;

    HRESULT hrc;
    if (RT_LIKELY(aName.isNotEmpty()))
    {
        if (RT_LIKELY(strchr(aName.c_str(), '=') == NULL))
        {
            LogFlowThisFuncEnter();

            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
            if (mData.mpBaseEnvironment)
            {
                hrc = S_OK;
                *aExists = mData.mpBaseEnvironment->doesVariableExist(aName);
            }
            else if (mData.mProtocolVersion < 99999)
                hrc = setError(VBOX_E_NOT_SUPPORTED, tr("The base environment feature is not supported by the guest additions"));
            else
                hrc = setError(VBOX_E_INVALID_OBJECT_STATE, tr("The base environment has not yet been reported by the guest"));
        }
        else
            hrc = setError(E_INVALIDARG, tr("The equal char is not allowed in environment variable names"));
    }
    else
        hrc = setError(E_INVALIDARG, tr("No variable name specified"));

    LogFlowThisFuncLeave();
    return hrc;
}

HRESULT GuestSession::fileCreateTemp(const com::Utf8Str &aTemplateName, ULONG aMode, const com::Utf8Str &aPath, BOOL aSecure,
                                     ComPtr<IGuestFile> &aFile)
{
    RT_NOREF(aTemplateName, aMode, aPath, aSecure, aFile);
    ReturnComNotImplemented();
}

HRESULT GuestSession::fileExists(const com::Utf8Str &aPath, BOOL aFollowSymlinks, BOOL *aExists)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
    {
        *aExists = FALSE;
        return S_OK;
    }

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    GuestFsObjData objData; int rcGuest;
    int vrc = i_fileQueryInfo(aPath, aFollowSymlinks != FALSE, objData, &rcGuest);
    if (RT_SUCCESS(vrc))
    {
        *aExists = TRUE;
        return S_OK;
    }

    switch (vrc)
    {
        case VERR_GSTCTL_GUEST_ERROR:
            hrc = GuestProcess::i_setErrorExternal(this, rcGuest);
            break;

/** @todo r=bird: what about VERR_PATH_NOT_FOUND and VERR_FILE_NOT_FOUND?
 *        Where does that get converted to *aExists = FALSE? */
        case VERR_NOT_A_FILE:
            *aExists = FALSE;
            break;

        default:
            hrc = setError(VBOX_E_IPRT_ERROR, tr("Querying file information for \"%s\" failed: %Rrc"),
                                                 aPath.c_str(), vrc);
            break;
    }

    return hrc;
}

HRESULT GuestSession::fileOpen(const com::Utf8Str &aPath, FileAccessMode_T aAccessMode, FileOpenAction_T aOpenAction,
                               ULONG aCreationMode, ComPtr<IGuestFile> &aFile)
{
    LogFlowThisFuncEnter();

    const std::vector<FileOpenExFlag_T> EmptyFlags;
    return fileOpenEx(aPath, aAccessMode, aOpenAction, FileSharingMode_All, aCreationMode, EmptyFlags, aFile);
}

HRESULT GuestSession::fileOpenEx(const com::Utf8Str &aPath, FileAccessMode_T aAccessMode, FileOpenAction_T aOpenAction,
                                 FileSharingMode_T aSharingMode, ULONG aCreationMode,
                                 const std::vector<FileOpenExFlag_T> &aFlags, ComPtr<IGuestFile> &aFile)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY((aPath.c_str()) == NULL || *(aPath.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No file to open specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFuncEnter();

    GuestFileOpenInfo openInfo;
    openInfo.mFileName = aPath;
    openInfo.mCreationMode = aCreationMode;

    /* Validate aAccessMode. */
    switch (aAccessMode)
    {
        case (FileAccessMode_T)FileAccessMode_ReadOnly:
            RT_FALL_THRU();
        case (FileAccessMode_T)FileAccessMode_WriteOnly:
            RT_FALL_THRU();
        case (FileAccessMode_T)FileAccessMode_ReadWrite:
            openInfo.mAccessMode = aAccessMode;
            break;
        case (FileAccessMode_T)FileAccessMode_AppendOnly:
            RT_FALL_THRU();
        case (FileAccessMode_T)FileAccessMode_AppendRead:
            return setError(E_NOTIMPL, tr("Append access modes are not yet implemented"));
        default:
            return setError(E_INVALIDARG, tr("Unknown FileAccessMode value %u (%#x)"), aAccessMode, aAccessMode);
    }

    /* Validate aOpenAction to the old format. */
    switch (aOpenAction)
    {
        case (FileOpenAction_T)FileOpenAction_OpenExisting:
            RT_FALL_THRU();
        case (FileOpenAction_T)FileOpenAction_OpenOrCreate:
            RT_FALL_THRU();
        case (FileOpenAction_T)FileOpenAction_CreateNew:
            RT_FALL_THRU();
        case (FileOpenAction_T)FileOpenAction_CreateOrReplace:
            RT_FALL_THRU();
        case (FileOpenAction_T)FileOpenAction_OpenExistingTruncated:
            RT_FALL_THRU();
        case (FileOpenAction_T)FileOpenAction_AppendOrCreate:
            openInfo.mOpenAction = aOpenAction;
            break;
        default:
            return setError(E_INVALIDARG, tr("Unknown FileOpenAction value %u (%#x)"), aAccessMode, aAccessMode);
    }

    /* Validate aSharingMode. */
    switch (aSharingMode)
    {
        case (FileSharingMode_T)FileSharingMode_All:
            openInfo.mSharingMode = aSharingMode;
            break;
        case (FileSharingMode_T)FileSharingMode_Read:
        case (FileSharingMode_T)FileSharingMode_Write:
        case (FileSharingMode_T)FileSharingMode_ReadWrite:
        case (FileSharingMode_T)FileSharingMode_Delete:
        case (FileSharingMode_T)FileSharingMode_ReadDelete:
        case (FileSharingMode_T)FileSharingMode_WriteDelete:
            return setError(E_NOTIMPL, tr("Only FileSharingMode_All is currently implemented"));

        default:
            return setError(E_INVALIDARG, tr("Unknown FileOpenAction value %u (%#x)"), aAccessMode, aAccessMode);
    }

    /* Combine and validate flags. */
    uint32_t fOpenEx = 0;
    for (size_t i = 0; i < aFlags.size(); i++)
        fOpenEx = aFlags[i];
    if (fOpenEx)
        return setError(E_INVALIDARG, tr("Unsupported FileOpenExFlag value(s) in aFlags (%#x)"), fOpenEx);
    openInfo.mfOpenEx = fOpenEx;

    ComObjPtr <GuestFile> pFile;
    int rcGuest;
    int vrc = i_fileOpenEx(aPath, aAccessMode, aOpenAction, aSharingMode, aCreationMode, aFlags, pFile, &rcGuest);
    if (RT_SUCCESS(vrc))
        /* Return directory object to the caller. */
        hrc = pFile.queryInterfaceTo(aFile.asOutParam());
    else
    {
        switch (vrc)
        {
            case VERR_NOT_SUPPORTED:
                hrc = setError(VBOX_E_IPRT_ERROR,
                              tr("Handling guest files not supported by installed Guest Additions"));
                break;

            case VERR_GSTCTL_GUEST_ERROR:
                hrc = GuestFile::i_setErrorExternal(this, rcGuest);
                break;

            default:
                hrc = setError(VBOX_E_IPRT_ERROR, tr("Opening guest file \"%s\" failed: %Rrc"),
                              aPath.c_str(), vrc);
                break;
        }
    }

    return hrc;
}

HRESULT GuestSession::fileQuerySize(const com::Utf8Str &aPath, BOOL aFollowSymlinks, LONG64 *aSize)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (aPath.isEmpty())
        return setError(E_INVALIDARG, tr("No path specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    int64_t llSize; int rcGuest;
    int vrc = i_fileQuerySize(aPath, aFollowSymlinks != FALSE,  &llSize, &rcGuest);
    if (RT_SUCCESS(vrc))
    {
        *aSize = llSize;
    }
    else
    {
        if (GuestProcess::i_isGuestError(vrc))
        {
            hrc = GuestProcess::i_setErrorExternal(this, rcGuest);
        }
        else
            hrc = setError(VBOX_E_IPRT_ERROR, tr("Querying file size failed: %Rrc"), vrc);
    }

    return hrc;
}

HRESULT GuestSession::fsObjExists(const com::Utf8Str &aPath, BOOL aFollowSymlinks, BOOL *aExists)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (aPath.isEmpty())
        return setError(E_INVALIDARG, tr("No path specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFunc(("aPath=%s, aFollowSymlinks=%RTbool\n", aPath.c_str(), RT_BOOL(aFollowSymlinks)));

    *aExists = false;

    GuestFsObjData objData;
    int rcGuest;
    int vrc = i_fsQueryInfo(aPath, aFollowSymlinks != FALSE, objData, &rcGuest);
    if (RT_SUCCESS(vrc))
    {
        *aExists = TRUE;
    }
    else
    {
        if (GuestProcess::i_isGuestError(vrc))
        {
            if (   rcGuest == VERR_NOT_A_FILE
                || rcGuest == VERR_PATH_NOT_FOUND
                || rcGuest == VERR_FILE_NOT_FOUND
                || rcGuest == VERR_INVALID_NAME)
            {
                hrc = S_OK; /* Ignore these vrc values. */
            }
            else
                hrc = GuestProcess::i_setErrorExternal(this, rcGuest);
        }
        else
            hrc = setErrorVrc(vrc, tr("Querying file information for \"%s\" failed: %Rrc"), aPath.c_str(), vrc);
    }

    return hrc;
}

HRESULT GuestSession::fsObjQueryInfo(const com::Utf8Str &aPath, BOOL aFollowSymlinks, ComPtr<IGuestFsObjInfo> &aInfo)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (aPath.isEmpty())
        return setError(E_INVALIDARG, tr("No path specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFunc(("aPath=%s, aFollowSymlinks=%RTbool\n", aPath.c_str(), RT_BOOL(aFollowSymlinks)));

    GuestFsObjData Info; int rcGuest;
    int vrc = i_fsQueryInfo(aPath, aFollowSymlinks != FALSE, Info, &rcGuest);
    if (RT_SUCCESS(vrc))
    {
        ComObjPtr<GuestFsObjInfo> ptrFsObjInfo;
        hrc = ptrFsObjInfo.createObject();
        if (SUCCEEDED(hrc))
        {
            vrc = ptrFsObjInfo->init(Info);
            if (RT_SUCCESS(vrc))
                hrc = ptrFsObjInfo.queryInterfaceTo(aInfo.asOutParam());
            else
                hrc = setErrorVrc(vrc);
        }
    }
    else
    {
        if (GuestProcess::i_isGuestError(vrc))
        {
            hrc = GuestProcess::i_setErrorExternal(this, rcGuest);
        }
        else
            hrc = setErrorVrc(vrc, tr("Querying file information for \"%s\" failed: %Rrc"), aPath.c_str(), vrc);
    }

    return hrc;
}

HRESULT GuestSession::fsObjRemove(const com::Utf8Str &aPath)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY(aPath.isEmpty()))
        return setError(E_INVALIDARG, tr("No path specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    LogFlowThisFunc(("aPath=%s\n", aPath.c_str()));

    int rcGuest;
    int vrc = i_fileRemove(aPath, &rcGuest);
    if (RT_FAILURE(vrc))
    {
        if (GuestProcess::i_isGuestError(vrc))
        {
            hrc = GuestProcess::i_setErrorExternal(this, rcGuest);
        }
        else
            hrc = setError(VBOX_E_IPRT_ERROR, tr("Removing file \"%s\" failed: %Rrc"), aPath.c_str(), vrc);
    }

    return hrc;
}

HRESULT GuestSession::fsObjRename(const com::Utf8Str &aSource,
                                  const com::Utf8Str &aDestination,
                                  const std::vector<FsObjRenameFlag_T> &aFlags)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (RT_UNLIKELY(aSource.isEmpty()))
        return setError(E_INVALIDARG, tr("No source path specified"));

    if (RT_UNLIKELY(aDestination.isEmpty()))
        return setError(E_INVALIDARG, tr("No destination path specified"));

    HRESULT hrc = i_isReadyExternal();
    if (FAILED(hrc))
        return hrc;

    /* Combine, validate and convert flags. */
    uint32_t fApiFlags = 0;
    for (size_t i = 0; i < aFlags.size(); i++)
        fApiFlags |= aFlags[i];
    if (fApiFlags & ~((uint32_t)FsObjRenameFlag_NoReplace | (uint32_t)FsObjRenameFlag_Replace))
        return setError(E_INVALIDARG, tr("Unknown rename flag: %#x"), fApiFlags);

    LogFlowThisFunc(("aSource=%s, aDestination=%s\n", aSource.c_str(), aDestination.c_str()));

    AssertCompile(FsObjRenameFlag_NoReplace == 0);
    AssertCompile(FsObjRenameFlag_Replace != 0);
    uint32_t fBackend;
    if ((fApiFlags & ((uint32_t)FsObjRenameFlag_NoReplace | (uint32_t)FsObjRenameFlag_Replace)) == FsObjRenameFlag_Replace)
        fBackend = PATHRENAME_FLAG_REPLACE;
    else
        fBackend = PATHRENAME_FLAG_NO_REPLACE;

    /* Call worker to do the job. */
    int rcGuest;
    int vrc = i_pathRename(aSource, aDestination, fBackend, &rcGuest);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_NOT_SUPPORTED:
                hrc = setError(VBOX_E_IPRT_ERROR,
                              tr("Handling renaming guest directories not supported by installed Guest Additions"));
                break;

            case VERR_GSTCTL_GUEST_ERROR:
                hrc = setError(VBOX_E_IPRT_ERROR,
                              tr("Renaming guest directory failed: %Rrc"), rcGuest);
                break;

            default:
                hrc = setError(VBOX_E_IPRT_ERROR, tr("Renaming guest directory \"%s\" failed: %Rrc"),
                                                     aSource.c_str(), vrc);
                break;
        }
    }

    return hrc;
}

HRESULT GuestSession::fsObjMove(const com::Utf8Str &aSource, const com::Utf8Str &aDestination,
                                const std::vector<FsObjMoveFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aSource, aDestination, aFlags, aProgress);
    ReturnComNotImplemented();
}

HRESULT GuestSession::fsObjSetACL(const com::Utf8Str &aPath, BOOL aFollowSymlinks, const com::Utf8Str &aAcl, ULONG aMode)
{
    RT_NOREF(aPath, aFollowSymlinks, aAcl, aMode);
    ReturnComNotImplemented();
}


HRESULT GuestSession::processCreate(const com::Utf8Str &aExecutable, const std::vector<com::Utf8Str> &aArguments,
                                    const std::vector<com::Utf8Str> &aEnvironment,
                                    const std::vector<ProcessCreateFlag_T> &aFlags,
                                    ULONG aTimeoutMS, ComPtr<IGuestProcess> &aGuestProcess)
{
    LogFlowThisFuncEnter();

    std::vector<LONG> affinityIgnored;
    return processCreateEx(aExecutable, aArguments, aEnvironment, aFlags, aTimeoutMS, ProcessPriority_Default,
                           affinityIgnored, aGuestProcess);
}

HRESULT GuestSession::processCreateEx(const com::Utf8Str &aExecutable, const std::vector<com::Utf8Str> &aArguments,
                                      const std::vector<com::Utf8Str> &aEnvironment,
                                      const std::vector<ProcessCreateFlag_T> &aFlags, ULONG aTimeoutMS,
                                      ProcessPriority_T aPriority, const std::vector<LONG> &aAffinity,
                                      ComPtr<IGuestProcess> &aGuestProcess)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = i_isReadyExternal();
    if (FAILED(hr))
        return hr;

    /** @todo r=bird: Check input better? aPriority is passed on to the guest
     * without any validation.  Flags not existing in this vbox version are
     * ignored, potentially doing something entirely different than what the
     * caller had in mind. */

    /*
     * Must have an executable to execute.  If none is given, we try use the
     * zero'th argument.
     */
    const char *pszExecutable = aExecutable.c_str();
    if (RT_UNLIKELY(pszExecutable == NULL || *pszExecutable == '\0'))
    {
        if (aArguments.size() > 0)
            pszExecutable = aArguments[0].c_str();
        if (pszExecutable == NULL || *pszExecutable == '\0')
            return setError(E_INVALIDARG, tr("No command to execute specified"));
    }

    LogFlowThisFuncEnter();

    /*
     * Build the process startup info.
     */
    GuestProcessStartupInfo procInfo;

    /* Executable and arguments. */
    procInfo.mExecutable = pszExecutable;
    if (aArguments.size())
        for (size_t i = 0; i < aArguments.size(); i++)
            procInfo.mArguments.push_back(aArguments[i]);

    /* Combine the environment changes associated with the ones passed in by
       the caller, giving priority to the latter.  The changes are putenv style
       and will be applied to the standard environment for the guest user. */
    int vrc = procInfo.mEnvironmentChanges.copy(mData.mEnvironmentChanges);
    if (RT_SUCCESS(vrc))
        vrc = procInfo.mEnvironmentChanges.applyPutEnvArray(aEnvironment);
    if (RT_SUCCESS(vrc))
    {
        /* Convert the flag array into a mask. */
        if (aFlags.size())
            for (size_t i = 0; i < aFlags.size(); i++)
                procInfo.mFlags |= aFlags[i];

        procInfo.mTimeoutMS = aTimeoutMS;

        /** @todo use RTCPUSET instead of archaic 64-bit variables! */
        if (aAffinity.size())
            for (size_t i = 0; i < aAffinity.size(); i++)
                if (aAffinity[i])
                    procInfo.mAffinity |= (uint64_t)1 << i;

        procInfo.mPriority = aPriority;

        /*
         * Create a guest process object.
         */
        ComObjPtr<GuestProcess> pProcess;
        vrc = i_processCreateEx(procInfo, pProcess);
        if (RT_SUCCESS(vrc))
        {
            ComPtr<IGuestProcess> pIProcess;
            hr = pProcess.queryInterfaceTo(pIProcess.asOutParam());
            if (SUCCEEDED(hr))
            {
                /*
                 * Start the process.
                 */
                vrc = pProcess->i_startProcessAsync();
                if (RT_SUCCESS(vrc))
                {
                    aGuestProcess = pIProcess;

                    LogFlowFuncLeaveRC(vrc);
                    return S_OK;
                }

                hr = setErrorVrc(vrc, tr("Failed to start guest process: %Rrc"), vrc);
            }
        }
        else if (vrc == VERR_GSTCTL_MAX_OBJECTS_REACHED)
            hr = setErrorVrc(vrc, tr("Maximum number of concurrent guest processes per session (%u) reached"),
                             VBOX_GUESTCTRL_MAX_OBJECTS);
        else
            hr = setErrorVrc(vrc, tr("Failed to create guest process object: %Rrc"), vrc);
    }
    else
        hr = setErrorVrc(vrc, tr("Failed to set up the environment: %Rrc"), vrc);

    LogFlowFuncLeaveRC(vrc);
    return hr;
}

HRESULT GuestSession::processGet(ULONG aPid, ComPtr<IGuestProcess> &aGuestProcess)

{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (aPid == 0)
        return setError(E_INVALIDARG, tr("No valid process ID (PID) specified"));

    LogFlowThisFunc(("PID=%RU32\n", aPid));

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    ComObjPtr<GuestProcess> pProcess;
    int rc = i_processGetByPID(aPid, &pProcess);
    if (RT_FAILURE(rc))
        hr = setError(E_INVALIDARG, tr("No process with PID %RU32 found"), aPid);

    /* This will set (*aProcess) to NULL if pProgress is NULL. */
    HRESULT hr2 = pProcess.queryInterfaceTo(aGuestProcess.asOutParam());
    if (SUCCEEDED(hr))
        hr = hr2;

    LogFlowThisFunc(("aProcess=%p, hr=%Rhrc\n", (IGuestProcess*)aGuestProcess, hr));
    return hr;
}

HRESULT GuestSession::symlinkCreate(const com::Utf8Str &aSource, const com::Utf8Str &aTarget, SymlinkType_T aType)
{
    RT_NOREF(aSource, aTarget, aType);
    ReturnComNotImplemented();
}

HRESULT GuestSession::symlinkExists(const com::Utf8Str &aSymlink, BOOL *aExists)

{
    RT_NOREF(aSymlink, aExists);
    ReturnComNotImplemented();
}

HRESULT GuestSession::symlinkRead(const com::Utf8Str &aSymlink, const std::vector<SymlinkReadFlag_T> &aFlags,
                                  com::Utf8Str &aTarget)
{
    RT_NOREF(aSymlink, aFlags, aTarget);
    ReturnComNotImplemented();
}

HRESULT GuestSession::waitFor(ULONG aWaitFor, ULONG aTimeoutMS, GuestSessionWaitResult_T *aReason)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Note: No call to i_isReadyExternal() needed here, as the session might not has been started (yet). */

    LogFlowThisFuncEnter();

    HRESULT hrc = S_OK;

    /*
     * Note: Do not hold any locks here while waiting!
     */
    int rcGuest; GuestSessionWaitResult_T waitResult;
    int vrc = i_waitFor(aWaitFor, aTimeoutMS, waitResult, &rcGuest);
    if (RT_SUCCESS(vrc))
        *aReason = waitResult;
    else
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                hrc = GuestSession::i_setErrorExternal(this, rcGuest);
                break;

            case VERR_TIMEOUT:
                *aReason = GuestSessionWaitResult_Timeout;
                break;

            default:
            {
                const char *pszSessionName = mData.mSession.mName.c_str();
                hrc = setError(VBOX_E_IPRT_ERROR,
                               tr("Waiting for guest session \"%s\" failed: %Rrc"),
                                  pszSessionName ? pszSessionName : tr("Unnamed"), vrc);
                break;
            }
        }
    }

    LogFlowFuncLeaveRC(vrc);
    return hrc;
}

HRESULT GuestSession::waitForArray(const std::vector<GuestSessionWaitForFlag_T> &aWaitFor, ULONG aTimeoutMS,
                                   GuestSessionWaitResult_T *aReason)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Note: No call to i_isReadyExternal() needed here, as the session might not has been started (yet). */

    LogFlowThisFuncEnter();

    /*
     * Note: Do not hold any locks here while waiting!
     */
    uint32_t fWaitFor = GuestSessionWaitForFlag_None;
    for (size_t i = 0; i < aWaitFor.size(); i++)
        fWaitFor |= aWaitFor[i];

    return WaitFor(fWaitFor, aTimeoutMS, aReason);
}
