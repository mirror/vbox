/* $Id$ */
/** @file
 * VirtualBox Main - Guest directory handling.
 */

/*
 * Copyright (C) 2012-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN_GUESTDIRECTORY
#include "LoggingNew.h"

#ifndef VBOX_WITH_GUEST_CONTROL
# error "VBOX_WITH_GUEST_CONTROL must defined in this file"
#endif
#include "GuestImpl.h"
#include "GuestDirectoryImpl.h"
#include "GuestSessionImpl.h"
#include "GuestCtrlImplPrivate.h"
#include "VirtualBoxErrorInfoImpl.h"

#include "Global.h"
#include "AutoCaller.h"
#include "VBoxEvents.h"

#include <VBox/com/array.h>
#include <VBox/com/listeners.h>
#include <VBox/AssertGuest.h>


/**
 * Internal listener class to serve events in an
 * active manner, e.g. without polling delays.
 */
class GuestDirectoryListener
{
public:

    GuestDirectoryListener(void)
    {
    }

    virtual ~GuestDirectoryListener()
    {
    }

    HRESULT init(GuestDirectory *pDir)
    {
        AssertPtrReturn(pDir, E_POINTER);
        mDir = pDir;
        return S_OK;
    }

    void uninit(void)
    {
        mDir = NULL;
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnGuestDirectoryStateChanged:
                RT_FALL_THROUGH();
            case VBoxEventType_OnGuestDirectoryRead:
            {
                AssertPtrReturn(mDir, E_POINTER);
                int vrc2 = mDir->signalWaitEvent(aType, aEvent);
                RT_NOREF(vrc2);
#ifdef DEBUG_andy
                LogFlowFunc(("Signalling events of type=%RU32, dir=%p resulted in vrc=%Rrc\n",
                             aType, mDir, vrc2));
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

    /** Weak pointer to the guest directory object to listen for. */
    GuestDirectory *mDir;
};
typedef ListenerImpl<GuestDirectoryListener, GuestDirectory *> GuestDirectoryListenerImpl;

VBOX_LISTENER_DECLARE(GuestDirectoryListenerImpl)

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDirectory)

HRESULT GuestDirectory::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestDirectory::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestDirectory::init(Console *pConsole, GuestSession *pSession, ULONG aObjectID, const GuestDirectoryOpenInfo &openInfo)
{
    LogFlowThisFunc(("pConsole=%p, pSession=%p, aObjectID=%RU32, strPath=%s, enmFilter=%#x, fFlags=%x\n",
                     pConsole, pSession, aObjectID, openInfo.mPath.c_str(), openInfo.menmFilter, openInfo.mFlags));

    AssertPtrReturn(pConsole, VERR_INVALID_POINTER);
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

    int vrc = bindToSession(pConsole, pSession, aObjectID);
    if (RT_SUCCESS(vrc))
    {
        mSession  = pSession;
        mObjectID = aObjectID;

        mData.mOpenInfo  = openInfo;
        mData.mStatus    = DirectoryStatus_Undefined;
        mData.mLastError = VINF_SUCCESS;

        unconst(mEventSource).createObject();
        HRESULT hr = mEventSource->init();
        if (FAILED(hr))
            vrc = VERR_COM_UNEXPECTED;
    }

    if (RT_SUCCESS(vrc))
    {
        try
        {
            GuestDirectoryListener *pListener = new GuestDirectoryListener();
            ComObjPtr<GuestDirectoryListenerImpl> thisListener;
            HRESULT hr = thisListener.createObject();
            if (SUCCEEDED(hr))
                hr = thisListener->init(pListener, this);

            if (SUCCEEDED(hr))
            {
                com::SafeArray <VBoxEventType_T> eventTypes;
                eventTypes.push_back(VBoxEventType_OnGuestDirectoryStateChanged);
                eventTypes.push_back(VBoxEventType_OnGuestDirectoryRead);
                hr = mEventSource->RegisterListener(thisListener,
                                                    ComSafeArrayAsInParam(eventTypes),
                                                    TRUE /* Active listener */);
                if (SUCCEEDED(hr))
                {
                    vrc = baseInit();
                    if (RT_SUCCESS(vrc))
                    {
                        mLocalListener = thisListener;
                    }
                }
                else
                    vrc = VERR_COM_UNEXPECTED;
            }
            else
                vrc = VERR_COM_UNEXPECTED;
        }
        catch(std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }
    }

    /* Confirm a successful initialization when it's the case. */
    if (RT_SUCCESS(vrc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed();

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestDirectory::uninit(void)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    LogFlowThisFuncLeave();
}

// implementation of private wrapped getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDirectory::getDirectoryName(com::Utf8Str &aDirectoryName)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aDirectoryName = mData.mOpenInfo.mPath;

    return S_OK;
}

HRESULT GuestDirectory::getEventSource(ComPtr<IEventSource> &aEventSource)
{
    /* No need to lock - lifetime constant. */
    mEventSource.queryInterfaceTo(aEventSource.asOutParam());

    return S_OK;
}

HRESULT GuestDirectory::getFilter(com::Utf8Str &aFilter)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aFilter = mData.mOpenInfo.mFilter;

    return S_OK;
}

HRESULT GuestDirectory::getId(ULONG *aId)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aId = mObjectID;

    return S_OK;
}

HRESULT GuestDirectory::getStatus(DirectoryStatus_T *aStatus)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aStatus = mData.mStatus;

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Entry point for guest side directory callbacks.
 *
 * @returns VBox status code.
 * @param   pCbCtx              Host callback context.
 * @param   pSvcCb              Host callback data.
 */
int GuestDirectory::i_callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strPath=%s, uContextID=%RU32, uMessage=%RU32, pSvcCb=%p\n",
                     mData.mOpenInfo.mPath.c_str(), pCbCtx->uContextID, pCbCtx->uMessage, pSvcCb));

    int vrc;
    switch (pCbCtx->uMessage)
    {
        case GUEST_MSG_DISCONNECTED:
            /** @todo vrc = i_onGuestDisconnected(pCbCtx, pSvcCb); */
            vrc = VINF_SUCCESS; /// @todo To be implemented
            break;

        case GUEST_MSG_DIR_NOTIFY:
        {
            vrc = i_onDirNotify(pCbCtx, pSvcCb);
            break;
        }

        default:
            /* Silently ignore not implemented functions. */
            vrc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Opens the directory on the guest side.
 *
 * @return VBox status code.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 */
int GuestDirectory::i_open(int *pvrcGuest)
{
    int vrc;
#ifdef VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS
    if ((mSession->i_getParent()->i_getGuestControlFeatures0() & VBOX_GUESTCTRL_GF_0_TOOLBOX_AS_CMDS))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        GuestWaitEvent *pEvent = NULL;
        GuestEventTypes eventTypes;
        try
        {
            eventTypes.push_back(VBoxEventType_OnGuestDirectoryStateChanged);

            vrc = registerWaitEvent(eventTypes, &pEvent);
        }
        catch (std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(vrc))
            return vrc;

        /* Prepare HGCM call. */
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        HGCMSvcSetU32(&paParms[i++], pEvent->ContextID());
        HGCMSvcSetStr(&paParms[i++], mData.mOpenInfo.mPath.c_str());
        HGCMSvcSetU32(&paParms[i++], mData.mOpenInfo.menmFilter);
        HGCMSvcSetU32(&paParms[i++], mData.mOpenInfo.mFlags);

        alock.release(); /* Drop lock before sending. */

        vrc = sendMessage(HOST_MSG_DIR_OPEN, i, paParms);
        if (RT_SUCCESS(vrc))
            vrc = i_waitForStatusChange(pEvent, 30 * 1000, NULL /* FileStatus */, pvrcGuest);
    }
    else
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS */
    {
        vrc = i_openViaToolbox(pvrcGuest);
    }

    return vrc;
}

#ifdef VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT
/**
 * Opens the directory on the guest side (legacy version).
 *
 * @returns VBox status code.
 * @param   pvrcGuest           Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 *
 * @note This uses an own guest process via the built-in toolbox in VBoxSerivce.
 */
int GuestDirectory::i_openViaToolbox(int *pvrcGuest)
{
    /* Start the directory process on the guest. */
    GuestProcessStartupInfo procInfo;
    procInfo.mName.printf(tr("Opening directory \"%s\""), mData.mOpenInfo.mPath.c_str());
    procInfo.mTimeoutMS = 5 * 60 * 1000; /* 5 minutes timeout. */
    procInfo.mFlags     = ProcessCreateFlag_WaitForStdOut;
    procInfo.mExecutable= Utf8Str(VBOXSERVICE_TOOL_LS);

    procInfo.mArguments.push_back(procInfo.mExecutable);
    procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
    /* We want the long output format which contains all the object details. */
    procInfo.mArguments.push_back(Utf8Str("-l"));
# if 0 /* Flags are not supported yet. */
    if (uFlags & DirectoryOpenFlag_NoSymlinks)
        procInfo.mArguments.push_back(Utf8Str("--nosymlinks")); /** @todo What does GNU here? */
# endif
    /** @todo Recursion support? */
    procInfo.mArguments.push_back(mData.mOpenInfo.mPath); /* The directory we want to open. */

    /*
     * Start the process synchronously and keep it around so that we can use
     * it later in subsequent read() calls.
     */
    int vrc = mData.mProcessTool.init(mSession, procInfo, false /*fAsync*/, NULL /*pvrcGuest*/);
    if (RT_SUCCESS(vrc))
    {
        /* As we need to know if the directory we were about to open exists and and is accessible,
         * do the first read here in order to return a meaningful status here. */
        int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;
        vrc = i_readInternal(mData.mObjData, &vrcGuest);
        if (RT_FAILURE(vrc))
        {
            /*
             * We need to actively terminate our process tool in case of an error here,
             * as this otherwise would be done on (directory) object destruction implicitly.
             * This in turn then will run into a timeout, as the directory object won't be
             * around anymore at that time. Ugly, but that's how it is for the moment.
             */
            /* ignore rc */ mData.mProcessTool.terminate(30 * RT_MS_1SEC, NULL /* pvrcGuest */);
        }

        if (pvrcGuest)
            *pvrcGuest = vrcGuest;
    }

    return vrc;
}
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT */

/**
 * Called when the guest side notifies the host of a directory event.
 *
 * @returns VBox status code.
 * @param   pCbCtx              Host callback context.
 * @param   pSvcCbData          Host callback data.
 */
int GuestDirectory::i_onDirNotify(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData)
{
#ifndef VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS
    RT_NOREF(pCbCtx, pSvcCbData);
    return VERR_NOT_SUPPORTED;
#else
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);

    LogFlowThisFuncEnter();

    if (pSvcCbData->mParms < 3)
        return VERR_INVALID_PARAMETER;

    int idx = 1; /* Current parameter index. */
    CALLBACKDATA_DIR_NOTIFY dataCb;
    RT_ZERO(dataCb);
    /* pSvcCb->mpaParms[0] always contains the context ID. */
    HGCMSvcGetU32(&pSvcCbData->mpaParms[idx++], &dataCb.uType);
    HGCMSvcGetU32(&pSvcCbData->mpaParms[idx++], &dataCb.rc);

    int vrcGuest = (int)dataCb.rc; /* uint32_t vs. int. */

    LogFlowThisFunc(("uType=%RU32, vrcGuest=%Rrc\n", dataCb.uType, vrcGuest));

    if (RT_FAILURE(vrcGuest))
    {
        /** @todo Set status? */

        /* Ignore return code, as the event to signal might not be there (anymore). */
        signalWaitEventInternal(pCbCtx, vrcGuest, NULL /* pPayload */);
        return VINF_SUCCESS; /* Report to the guest. */
    }

    int vrc = VERR_NOT_SUPPORTED; /* Play safe by default. */

    ComObjPtr<VirtualBoxErrorInfo> errorInfo;
    HRESULT hrc = errorInfo.createObject();
    ComAssertComRCRet(hrc, VERR_COM_UNEXPECTED);
    if (RT_FAILURE(vrcGuest))
    {
        hrc = errorInfo->initEx(VBOX_E_GSTCTL_GUEST_ERROR, vrcGuest,
                                COM_IIDOF(IGuestDirectory), getComponentName(),
                                i_guestErrorToString(vrcGuest, mData.mOpenInfo.mPath.c_str()));
        ComAssertComRCRet(hrc, VERR_COM_UNEXPECTED);
    }

    switch (dataCb.uType)
    {
        case GUEST_DIR_NOTIFYTYPE_ERROR:
        {
            vrc = i_setStatus(DirectoryStatus_Error, vrcGuest);
            break;
        }

        case GUEST_DIR_NOTIFYTYPE_OPEN:
        {
            AssertBreakStmt(pSvcCbData->mParms >= 4, vrc = VERR_INVALID_PARAMETER);
            vrc = HGCMSvcGetU32(&pSvcCbData->mpaParms[idx++], &dataCb.u.open.uHandle /* Guest native file handle */);
            AssertRCBreak(vrc);
            vrc = i_setStatus(DirectoryStatus_Open, vrcGuest);
            break;
        }

        case GUEST_DIR_NOTIFYTYPE_CLOSE:
        {
            vrc = i_setStatus(DirectoryStatus_Close, vrcGuest);
            break;
        }

        case GUEST_DIR_NOTIFYTYPE_READ:
        {
            ASSERT_GUEST_MSG_STMT_BREAK(pSvcCbData->mParms == 7, ("mParms=%u\n", pSvcCbData->mParms),
                                        vrc = VERR_WRONG_PARAMETER_COUNT);
            ASSERT_GUEST_MSG_STMT_BREAK(pSvcCbData->mpaParms[idx].type == VBOX_HGCM_SVC_PARM_PTR,
                                        ("type=%u\n", pSvcCbData->mpaParms[idx].type),
                                        vrc = VERR_WRONG_PARAMETER_TYPE);
            PGSTCTLDIRENTRYEX pEntry;
            uint32_t          cbEntry;
            vrc = HGCMSvcGetPv(&pSvcCbData->mpaParms[idx++], (void **)&pEntry, &cbEntry);
            AssertRCBreak(vrc);
            AssertBreakStmt(   cbEntry >= sizeof(GSTCTLDIRENTRYEX)
                            && cbEntry <= GSTCTL_DIRENTRY_MAX_SIZE, VERR_INVALID_PARAMETER);
            dataCb.u.read.pEntry  = (PGSTCTLDIRENTRYEX)RTMemDup(pEntry, cbEntry);
            AssertPtrBreakStmt(dataCb.u.read.pEntry, vrc = VERR_NO_MEMORY);
            dataCb.u.read.cbEntry = cbEntry;

            char    *pszUser;
            uint32_t cbUser;
            vrc = HGCMSvcGetStr(&pSvcCbData->mpaParms[idx++], &pszUser, &cbUser);
            AssertRCBreak(vrc);
            dataCb.u.read.pszUser = RTStrDup(pszUser);
            AssertPtrBreakStmt(dataCb.u.read.pszUser, vrc = VERR_NO_MEMORY);
            dataCb.u.read.cbUser  = cbUser;

            char    *pszGroups;
            uint32_t cbGroups;
            vrc = HGCMSvcGetStr(&pSvcCbData->mpaParms[idx++], &pszGroups, &cbGroups);
            AssertRCBreak(vrc);
            dataCb.u.read.pszGroups = RTStrDup(pszGroups);
            AssertPtrBreakStmt(dataCb.u.read.pszGroups, vrc = VERR_NO_MEMORY);
            dataCb.u.read.cbGroups  = cbGroups;

            /** @todo ACLs not implemented yet. */

            GuestFsObjData fsObjData(dataCb.u.read.pEntry->szName);
            vrc = fsObjData.FromGuestFsObjInfo(&dataCb.u.read.pEntry->Info);
            AssertRCBreak(vrc);
            ComObjPtr<GuestFsObjInfo> ptrFsObjInfo;
            hrc = ptrFsObjInfo.createObject();
            ComAssertComRCBreak(hrc, vrc = VERR_COM_UNEXPECTED);
            vrc = ptrFsObjInfo->init(fsObjData);
            AssertRCBreak(vrc);

            ::FireGuestDirectoryReadEvent(mEventSource, mSession, this,
                                          dataCb.u.read.pEntry->szName, ptrFsObjInfo, dataCb.u.read.pszUser, dataCb.u.read.pszGroups);
            break;
        }

        case GUEST_DIR_NOTIFYTYPE_REWIND:
        {
            /* Note: This does not change the overall status of the directory (i.e. open). */
            ::FireGuestDirectoryStateChangedEvent(mEventSource, mSession, this, DirectoryStatus_Rewind, errorInfo);
            break;
        }

        default:
            AssertFailed();
            break;
    }

    try
    {
        if (RT_SUCCESS(vrc))
        {
            GuestWaitEventPayload payload(dataCb.uType, &dataCb, sizeof(dataCb));

            /* Ignore return code, as the event to signal might not be there (anymore). */
            signalWaitEventInternal(pCbCtx, vrcGuest, &payload);
        }
        else /* OOM situation, wrong HGCM parameters or smth. not expected. */
        {
            /* Ignore return code, as the event to signal might not be there (anymore). */
            signalWaitEventInternalEx(pCbCtx, vrc, 0 /* guestRc */, NULL /* pPayload */);
        }
    }
    catch (int vrcEx) /* Thrown by GuestWaitEventPayload constructor. */
    {
        /* Also try to signal the waiter, to let it know of the OOM situation.
         * Ignore return code, as the event to signal might not be there (anymore). */
        signalWaitEventInternalEx(pCbCtx, vrcEx, 0 /* guestRc */, NULL /* pPayload */);
        vrc = vrcEx;
    }

    LogFlowThisFunc(("uType=%RU32, rcGuest=%Rrc, vrc=%Rrc\n", dataCb.uType, vrcGuest, vrc));
    return vrc;
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS */
}

/**
 * Converts a given guest directory error to a string.
 *
 * @returns Error string.
 * @param   vrcGuest            Guest directory error to return string for.
 * @param   pcszWhat            Hint of what was involved when the error occurred.
 */
/* static */
Utf8Str GuestDirectory::i_guestErrorToString(int vrcGuest, const char *pcszWhat)
{
    AssertPtrReturn(pcszWhat, "");

#define CASE_MSG(a_iRc, ...) \
        case a_iRc: strErr.printf(__VA_ARGS__); break;

    Utf8Str strErr;
    switch (vrcGuest)
    {
        CASE_MSG(VERR_ACCESS_DENIED,  tr("Access to guest directory \"%s\" is denied"), pcszWhat);
        CASE_MSG(VERR_ALREADY_EXISTS, tr("Guest directory \"%s\" already exists"), pcszWhat);
        CASE_MSG(VERR_CANT_CREATE,    tr("Guest directory \"%s\" cannot be created"), pcszWhat);
        CASE_MSG(VERR_DIR_NOT_EMPTY,  tr("Guest directory \"%s\" is not empty"), pcszWhat);
        default:
            strErr.printf(tr("Error %Rrc for guest directory \"%s\" occurred\n"), vrcGuest, pcszWhat);
            break;
    }

#undef CASE_MSG

    return strErr;
}

/**
 * @copydoc GuestObject::i_onUnregister
 */
int GuestDirectory::i_onUnregister(void)
{
    LogFlowThisFuncEnter();

    int vrc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * @copydoc GuestObject::i_onSessionStatusChange
 */
int GuestDirectory::i_onSessionStatusChange(GuestSessionStatus_T enmSessionStatus)
{
    RT_NOREF(enmSessionStatus);

    LogFlowThisFuncEnter();

    int vrc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Closes this guest directory and removes it from the
 * guest session's directory list.
 *
 * @return VBox status code.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 */
int GuestDirectory::i_close(int *pvrcGuest)
{
    int vrc;
#ifdef VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS
    if (mSession->i_getParent()->i_getGuestControlFeatures0() & VBOX_GUESTCTRL_GF_0_TOOLBOX_AS_CMDS)
    {
        GuestWaitEvent *pEvent = NULL;
        GuestEventTypes eventTypes;
        try
        {
            eventTypes.push_back(VBoxEventType_OnGuestDirectoryStateChanged);

            vrc = registerWaitEvent(eventTypes, &pEvent);
        }
        catch (std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(vrc))
            return vrc;

        /* Prepare HGCM call. */
        VBOXHGCMSVCPARM paParms[2];
        int i = 0;
        HGCMSvcSetU32(&paParms[i++], pEvent->ContextID());
        HGCMSvcSetU32(&paParms[i++], mObjectID /* Guest directory handle */);

        vrc = sendMessage(HOST_MSG_DIR_CLOSE, i, paParms);
        if (RT_SUCCESS(vrc))
        {
            vrc = pEvent->Wait(30 * 1000);
            if (RT_SUCCESS(vrc))
            {
                // Nothing to do here.
            }
            else if (pEvent->HasGuestError() && pvrcGuest)
                *pvrcGuest = pEvent->GuestResult();
        }
    }
    else
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS */
    {
        vrc = i_closeViaToolbox(pvrcGuest);
    }

    AssertPtr(mSession);
    int vrc2 = mSession->i_directoryUnregister(this);
    if (RT_SUCCESS(vrc))
        vrc = vrc2;

    LogFlowThisFunc(("Returning vrc=%Rrc\n", vrc));
    return vrc;
}

#ifdef VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT
/**
 * Closes this guest directory and removes it from the guest session's directory list (legacy version).
 *
 * @return VBox status code.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 *
 * @note This uses an own guest process via the built-in toolbox in VBoxSerivce.
 */
int GuestDirectory::i_closeViaToolbox(int *pvrcGuest)
{
    return mData.mProcessTool.terminate(30 * 1000 /* 30s timeout */, pvrcGuest);
}
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT */

/**
 * Reads the next directory entry, internal version.
 *
 * @return VBox status code. Will return VERR_NO_MORE_FILES if no more entries are available.
 * @param  objData              Where to store the read directory entry as internal object data.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 */
int GuestDirectory::i_readInternal(GuestFsObjData &objData, int *pvrcGuest)
{
    AssertPtrReturn(pvrcGuest, VERR_INVALID_POINTER);

    int vrc;
    int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;

#ifdef VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS
    if (mSession->i_getParent()->i_getGuestControlFeatures0() & VBOX_GUESTCTRL_GF_0_TOOLBOX_AS_CMDS)
    {
        GuestWaitEvent *pEvent = NULL;
        GuestEventTypes eventTypes;
        try
        {
            vrc = registerWaitEvent(eventTypes, &pEvent);
        }
        catch (std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(vrc))
            return vrc;

        /* Prepare HGCM call. */
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        HGCMSvcSetU32(&paParms[i++], pEvent->ContextID());
        HGCMSvcSetU32(&paParms[i++], mObjectID /* Guest directory handle */);
        HGCMSvcSetU32(&paParms[i++], GSTCTL_DIRENTRY_MAX_SIZE);
        HGCMSvcSetU32(&paParms[i++], GSTCTLFSOBJATTRADD_UNIX /* Implicit */);
        HGCMSvcSetU32(&paParms[i++], GSTCTL_PATH_F_ON_LINK);

        vrc = sendMessage(HOST_MSG_DIR_READ, i, paParms);
        if (RT_SUCCESS(vrc))
        {
            vrc = pEvent->Wait(30 * 1000);
            if (RT_SUCCESS(vrc))
            {
                PCALLBACKDATA_DIR_NOTIFY const pDirNotify = (PCALLBACKDATA_DIR_NOTIFY)pEvent->Payload().Raw();
                AssertPtrReturn(pDirNotify, VERR_INVALID_POINTER);
                vrcGuest = (int)pDirNotify->rc;
                if (RT_SUCCESS(vrcGuest))
                {
                    AssertReturn(pDirNotify->uType == GUEST_DIR_NOTIFYTYPE_READ, VERR_INVALID_PARAMETER);
                    AssertPtrReturn(pDirNotify->u.read.pEntry, VERR_INVALID_POINTER);
                    objData.Init(pDirNotify->u.read.pEntry->szName);
                    vrc = objData.FromGuestFsObjInfo(&pDirNotify->u.read.pEntry->Info,
                                                     pDirNotify->u.read.pszUser, pDirNotify->u.read.pszGroups);
                    RTMemFree(pDirNotify->u.read.pEntry);
                    RTStrFree(pDirNotify->u.read.pszUser);
                    RTStrFree(pDirNotify->u.read.pszGroups);
                }
                else
                {
                    if (pvrcGuest)
                        *pvrcGuest = vrcGuest;
                    vrc = VERR_GSTCTL_GUEST_ERROR;
                }
            }
            else if (pEvent->HasGuestError() && pvrcGuest)
                *pvrcGuest = pEvent->GuestResult();
        }
    }
    else
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS */
    {
        vrc = i_readInternalViaToolbox(objData, pvrcGuest);
    }

    LogFlowThisFunc(("Returning vrc=%Rrc\n", vrc));
    return vrc;
}

#ifdef VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT
/**
 * Reads the next directory entry, internal version (legacy version).
 *
 * @return VBox status code. Will return VERR_NO_MORE_FILES if no more entries are available.
 * @param  objData              Where to store the read directory entry as internal object data.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 *
 * @note This uses an own guest process via the built-in toolbox in VBoxSerivce.
 */
int GuestDirectory::i_readInternalViaToolbox(GuestFsObjData &objData, int *pvrcGuest)
{
    GuestToolboxStreamBlock curBlock;
    int vrc = mData.mProcessTool.waitEx(GUESTPROCESSTOOL_WAIT_FLAG_STDOUT_BLOCK, &curBlock, pvrcGuest);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Note: The guest process can still be around to serve the next
         *       upcoming stream block next time.
         */
        if (!mData.mProcessTool.isRunning())
            vrc = mData.mProcessTool.getTerminationStatus(); /* Tool process is not running (anymore). Check termination status. */

        if (RT_SUCCESS(vrc))
        {
            if (curBlock.GetCount()) /* Did we get content? */
            {
                if (curBlock.GetString("name"))
                {
                    vrc = objData.FromToolboxLs(curBlock, true /* fLong */);
                }
                else
                    vrc = VERR_PATH_NOT_FOUND;
            }
            else
            {
                /* Nothing to read anymore. Tell the caller. */
                vrc = VERR_NO_MORE_FILES;
            }
        }
    }

    return vrc;
}
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT */

/**
 * Reads the next directory entry.
 *
 * @return VBox status code. Will return VERR_NO_MORE_FILES if no more entries are available.
 * @param  fsObjInfo            Where to store the read directory entry.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 */
int GuestDirectory::i_read(ComObjPtr<GuestFsObjInfo> &fsObjInfo, int *pvrcGuest)
{
    AssertPtrReturn(pvrcGuest, VERR_INVALID_POINTER);

    /* Create the FS info object. */
    HRESULT hr = fsObjInfo.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    int vrc;

    /* If we have a valid object data cache, read from it. */
    if (mData.mObjData.mName.isNotEmpty())
    {
        vrc = fsObjInfo->init(mData.mObjData);
        if (RT_SUCCESS(vrc))
        {
            mData.mObjData.mName = ""; /* Mark the object data as being empty (beacon). */
        }
    }
    else /* Otherwise ask the guest for the next object data. */
    {

        GuestFsObjData objData;
        vrc = i_readInternal(objData, pvrcGuest);
        if (RT_SUCCESS(vrc))
            vrc = fsObjInfo->init(objData);
    }

    LogFlowThisFunc(("Returning vrc=%Rrc\n", vrc));
    return vrc;
}

/**
 * Rewinds the directory reading.
 *
 * @returns VBox status code.
 * @retval  VERR_GSTCTL_GUEST_ERROR when an error from the guest side has been received.
 * @param  uTimeoutMS           Timeout (in ms) to wait.
 * @param  pvrcGuest            Where to store the guest result code in case VERR_GSTCTL_GUEST_ERROR is returned.
 */
int GuestDirectory::i_rewind(uint32_t uTimeoutMS, int *pvrcGuest)
{
    RT_NOREF(pvrcGuest);
#ifndef VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS
    RT_NOREF(uTimeoutMS, pvrcGuest);
#else
    /* Only available for Guest Additions 7.1+. */
    if (mSession->i_getParent()->i_getGuestControlFeatures0() & VBOX_GUESTCTRL_GF_0_TOOLBOX_AS_CMDS)
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        int vrc;

        GuestWaitEvent *pEvent = NULL;
        GuestEventTypes eventTypes;
        try
        {
            eventTypes.push_back(VBoxEventType_OnGuestDirectoryStateChanged);
            vrc = registerWaitEvent(eventTypes, &pEvent);
        }
        catch (std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(vrc))
            return vrc;

        /* Prepare HGCM call. */
        VBOXHGCMSVCPARM paParms[4];
        int i = 0;
        HGCMSvcSetU32(&paParms[i++], pEvent->ContextID());
        HGCMSvcSetU32(&paParms[i++], mObjectID /* Directory handle */);

        alock.release(); /* Drop lock before sending. */

        vrc = sendMessage(HOST_MSG_DIR_REWIND, i, paParms);
        if (RT_SUCCESS(vrc))
        {
            VBoxEventType_T evtType;
            ComPtr<IEvent> pIEvent;
            vrc = waitForEvent(pEvent, uTimeoutMS, &evtType, pIEvent.asOutParam());
            if (RT_SUCCESS(vrc))
            {
                if (evtType == VBoxEventType_OnGuestDirectoryStateChanged)
                {
                    ComPtr<IGuestDirectoryStateChangedEvent> pEvt = pIEvent;
                    Assert(!pEvt.isNull());
                }
                else
                    vrc = VWRN_GSTCTL_OBJECTSTATE_CHANGED;
            }
            else if (pEvent->HasGuestError()) /* Return guest vrc if available. */
                vrc = pEvent->GuestResult();
        }

        unregisterWaitEvent(pEvent);
        return vrc;
    }
#endif /* VBOX_WITH_GSTCTL_TOOLBOX_AS_CMDS */

    return VERR_NOT_SUPPORTED;
}

/**
 * Sets the current internal directory object status.
 *
 * @returns VBox status code.
 * @param   enmStatus           New directory status to set.
 * @param   vrcDir              New result code to set.
 *
 * @note    Takes the write lock.
 */
int GuestDirectory::i_setStatus(DirectoryStatus_T enmStatus, int vrcDir)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("oldStatus=%RU32, newStatus=%RU32, vrcDir=%Rrc\n", mData.mStatus, enmStatus, vrcDir));

#ifdef VBOX_STRICT
    if (enmStatus == DirectoryStatus_Error)
        AssertMsg(RT_FAILURE(vrcDir), ("Guest vrc must be an error (%Rrc)\n", vrcDir));
    else
        AssertMsg(RT_SUCCESS(vrcDir), ("Guest vrc must not be an error (%Rrc)\n", vrcDir));
#endif

    if (mData.mStatus != enmStatus)
    {
        mData.mStatus    = enmStatus;
        mData.mLastError = vrcDir;

        ComObjPtr<VirtualBoxErrorInfo> errorInfo;
        HRESULT hrc = errorInfo.createObject();
        ComAssertComRCRet(hrc, VERR_COM_UNEXPECTED);
        if (RT_FAILURE(vrcDir))
        {
            hrc = errorInfo->initEx(VBOX_E_GSTCTL_GUEST_ERROR, vrcDir,
                                    COM_IIDOF(IGuestDirectory), getComponentName(),
                                    i_guestErrorToString(vrcDir, mData.mOpenInfo.mPath.c_str()));
            ComAssertComRCRet(hrc, VERR_COM_UNEXPECTED);
        }
        /* Note: On vrcDir success, errorInfo is set to S_OK and also sent via the event below. */

        alock.release(); /* Release lock before firing off event. */

        ::FireGuestDirectoryStateChangedEvent(mEventSource, mSession, this, mData.mStatus, errorInfo);
    }

    return VINF_SUCCESS;
}

/**
 * Waits for a guest directory status change.
 *
 * @note Similar code in GuestFile::i_waitForStatusChange().
 *
 * @returns VBox status code.
 * @retval  VERR_GSTCTL_GUEST_ERROR when an error from the guest side has been received.
 * @param   pEvent              Guest wait event to wait for.
 * @param   uTimeoutMS          Timeout (in ms) to wait.
 * @param   penmStatus          Where to return the directoy status on success.
 * @param   prcGuest            Where to return the guest error when VERR_GSTCTL_GUEST_ERROR was returned.
 */
int GuestDirectory::i_waitForStatusChange(GuestWaitEvent *pEvent, uint32_t uTimeoutMS,
                                          DirectoryStatus_T *penmStatus, int *prcGuest)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);
    /* penmStatus is optional. */

    VBoxEventType_T evtType;
    ComPtr<IEvent>  pIEvent;
    int vrc = waitForEvent(pEvent, uTimeoutMS, &evtType, pIEvent.asOutParam());
    if (RT_SUCCESS(vrc))
    {
        AssertReturn(evtType == VBoxEventType_OnGuestDirectoryStateChanged, VERR_WRONG_TYPE);
        ComPtr<IGuestDirectoryStateChangedEvent> pDirectoryEvent = pIEvent;
        AssertReturn(!pDirectoryEvent.isNull(), VERR_COM_UNEXPECTED);

        HRESULT hr;
        if (penmStatus)
        {
            hr = pDirectoryEvent->COMGETTER(Status)(penmStatus);
            ComAssertComRC(hr);
        }

        ComPtr<IVirtualBoxErrorInfo> errorInfo;
        hr = pDirectoryEvent->COMGETTER(Error)(errorInfo.asOutParam());
        ComAssertComRC(hr);

        LONG lGuestRc;
        hr = errorInfo->COMGETTER(ResultDetail)(&lGuestRc);
        ComAssertComRC(hr);

        LogFlowThisFunc(("resultDetail=%RI32 (%Rrc)\n", lGuestRc, lGuestRc));

        if (RT_FAILURE((int)lGuestRc))
            vrc = VERR_GSTCTL_GUEST_ERROR;

        if (prcGuest)
            *prcGuest = (int)lGuestRc;
    }
    /* waitForEvent may also return VERR_GSTCTL_GUEST_ERROR like we do above, so make prcGuest is set. */
    /** @todo Also see todo in GuestFile::i_waitForStatusChange(). */
    else if (vrc == VERR_GSTCTL_GUEST_ERROR && prcGuest)
        *prcGuest = pEvent->GuestResult();
    Assert(vrc != VERR_GSTCTL_GUEST_ERROR || !prcGuest || *prcGuest != (int)0xcccccccc);

    return vrc;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////
HRESULT GuestDirectory::close()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.hrc())) return autoCaller.hrc();

    LogFlowThisFuncEnter();

    HRESULT hrc = S_OK;

    int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    int vrc = i_close(&vrcGuest);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
            {
                GuestErrorInfo ge(GuestErrorInfo::Type_Directory, vrcGuest, mData.mOpenInfo.mPath.c_str());
                hrc = setErrorBoth(VBOX_E_IPRT_ERROR, vrcGuest, tr("Closing guest directory failed: %s"),
                                   GuestBase::getErrorAsString(ge).c_str());
                break;
            }
            case VERR_NOT_SUPPORTED:
                /* Silently skip old Guest Additions which do not support killing the
                 * the guest directory handling process. */
                break;

            default:
                hrc = setErrorBoth(VBOX_E_IPRT_ERROR, vrc,
                                   tr("Closing guest directory \"%s\" failed: %Rrc"), mData.mOpenInfo.mPath.c_str(), vrc);
                break;
        }
    }

    return hrc;
}

HRESULT GuestDirectory::read(ComPtr<IFsObjInfo> &aObjInfo)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.hrc())) return autoCaller.hrc();

    LogFlowThisFuncEnter();

    HRESULT hrc = S_OK;

    ComObjPtr<GuestFsObjInfo> fsObjInfo;
    int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    int vrc = i_read(fsObjInfo, &vrcGuest);
    if (RT_SUCCESS(vrc))
    {
        /* Return info object to the caller. */
        hrc = fsObjInfo.queryInterfaceTo(aObjInfo.asOutParam());
    }
    else
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
            {
                GuestErrorInfo ge(
#ifdef VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT
                                  GuestErrorInfo::Type_ToolLs
#else
                                  GuestErrorInfo::Type_Fs
#endif
                , vrcGuest, mData.mOpenInfo.mPath.c_str());
                hrc = setErrorBoth(VBOX_E_IPRT_ERROR, vrcGuest, tr("Reading guest directory failed: %s"),
                                   GuestBase::getErrorAsString(ge).c_str());
                break;
            }

#ifdef VBOX_WITH_GSTCTL_TOOLBOX_SUPPORT
            case VERR_GSTCTL_PROCESS_EXIT_CODE:
                hrc = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Reading guest directory \"%s\" failed: %Rrc"),
                                   mData.mOpenInfo.mPath.c_str(), mData.mProcessTool.getRc());
                break;
#endif
            case VERR_PATH_NOT_FOUND:
                hrc = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Reading guest directory \"%s\" failed: Path not found"),
                                   mData.mOpenInfo.mPath.c_str());
                break;

            case VERR_NO_MORE_FILES:
                /* See SDK reference. */
                hrc = setErrorBoth(VBOX_E_OBJECT_NOT_FOUND, vrc, tr("Reading guest directory \"%s\" failed: No more entries"),
                                   mData.mOpenInfo.mPath.c_str());
                break;

            default:
                hrc = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Reading guest directory \"%s\" returned unhandled error: %Rrc\n"),
                                   mData.mOpenInfo.mPath.c_str(), vrc);
                break;
        }
    }

    LogFlowThisFunc(("Returning hrc=%Rhrc / vrc=%Rrc\n", hrc, vrc));
    return hrc;
}

HRESULT GuestDirectory::rewind(void)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.hrc())) return autoCaller.hrc();

    int vrcGuest = VERR_IPE_UNINITIALIZED_STATUS;
    int vrc = i_rewind(30 * 1000 /* Timeout in ms */, &vrcGuest);
    if (RT_SUCCESS(vrc))
        return S_OK;

    GuestErrorInfo ge(GuestErrorInfo::Type_Directory, vrcGuest, mData.mOpenInfo.mPath.c_str());
    return setErrorBoth(VBOX_E_IPRT_ERROR, vrcGuest, tr("Rewinding guest directory failed: %s"),
                       GuestBase::getErrorAsString(ge).c_str());
}

