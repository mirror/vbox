
/* $Id$ */
/** @file
 * VirtualBox Main - Guest file handling.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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
#include "GuestFileImpl.h"
#include "GuestSessionImpl.h"
#include "GuestCtrlImplPrivate.h"
#include "ConsoleImpl.h"

#include "Global.h"
#include "AutoCaller.h"

#include <iprt/file.h>
#include <VBox/com/array.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestFile)

HRESULT GuestFile::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestFile::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestFile::init(Console *pConsole, GuestSession *pSession, ULONG uFileID, const GuestFileOpenInfo &openInfo)
{
    LogFlowThisFunc(("pConsole=%p, pSession=%p, uFileID=%RU32, strPath=%s\n",
                     pConsole, pSession, uFileID, openInfo.mFileName.c_str()));

    AssertPtrReturn(pConsole, VERR_INVALID_POINTER);
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

    int vrc = bindToSession(pConsole, pSession, uFileID /* Object ID */);
    if (RT_SUCCESS(vrc))
    {
        mData.mInitialSize = 0;

        /* Confirm a successful initialization when it's the case. */
        autoInitSpan.setSucceeded();
        return vrc;
    }

    autoInitSpan.setFailed();
    return vrc;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestFile::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

#ifdef VBOX_WITH_GUEST_CONTROL
    /*
     * Cancel + remove all callbacks + waiters.
     * Note: Deleting them is the job of the caller!
     */
    callbackRemoveAll();
#endif

    LogFlowThisFuncLeave();
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestFile::COMGETTER(CreationMode)(ULONG *aCreationMode)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aCreationMode);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCreationMode = mData.mOpenInfo.mCreationMode;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

/** @todo For 4.3: Change ULONG* to BSTR* ?*/
STDMETHODIMP GuestFile::COMGETTER(Disposition)(ULONG *aDisposition)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aDisposition);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aDisposition = getDispositionFromString(mData.mOpenInfo.mDisposition);

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::COMGETTER(FileName)(BSTR *aFileName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aFileName);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mOpenInfo.mFileName.cloneTo(aFileName);

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::COMGETTER(InitialSize)(LONG64 *aInitialSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aInitialSize);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aInitialSize = mData.mInitialSize;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::COMGETTER(Offset)(LONG64 *aOffset)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aOffset);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOffset = mData.mOffCurrent;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

/** @todo For 4.3: Change ULONG* to BSTR* ?*/
STDMETHODIMP GuestFile::COMGETTER(OpenMode)(ULONG *aOpenMode)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aOpenMode);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOpenMode = getOpenModeFromString(mData.mOpenInfo.mOpenMode);

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

// private methods
/////////////////////////////////////////////////////////////////////////////

int GuestFile::callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
{
#ifdef DEBUG
    LogFlowThisFunc(("strName=%s, uContextID=%RU32, uFunction=%RU32, pSvcCb=%p\n",
                     mData.mOpenInfo.mFileName.c_str(), pCbCtx->uContextID, pCbCtx->uFunction, pSvcCb));
#endif
    AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Get the optional callback associated to this context ID.
     * The callback may not be around anymore if just kept locally by the caller when
     * doing the actual HGCM sending stuff. */
    GuestCtrlCallback *pCallback = NULL;
    GuestCtrlCallbacks::const_iterator it
        = mData.mCallbacks.find(VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(pCbCtx->uContextID));
    if (it != mData.mCallbacks.end())
    {
        pCallback = it->second;
        AssertPtr(pCallback);
#ifdef DEBUG
        LogFlowThisFunc(("pCallback=%p, CID=%RU32, Count=%RU32\n",
                         pCallback, pCbCtx->uContextID, VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(pCbCtx->uContextID)));
#endif
    }

    int vrc;
    switch (pCbCtx->uFunction)
    {
        case GUEST_DISCONNECTED:
            vrc = onGuestDisconnected(pCbCtx, pCallback, pSvcCb); /* Affects all callbacks. */
            break;

        case GUEST_FILE_NOTIFY:
            vrc = onFileNotify(pCbCtx, pCallback, pSvcCb);
            break;

        default:
            /* Silently ignore not implemented functions. */
            vrc = VERR_NOT_SUPPORTED;
            break;
    }

#ifdef DEBUG
    LogFlowFuncLeaveRC(vrc);
#endif
    return vrc;
}

/* static */
uint32_t GuestFile::getDispositionFromString(const Utf8Str &strDisposition)
{
    return 0; /** @todo Implement me! */
}

/* static */
uint32_t GuestFile::getOpenModeFromString(const Utf8Str &strOpenMode)
{
    uint32_t uOpenMode = 0;

    const char *pc = strOpenMode.c_str();
    while (*pc != '\0')
    {
        switch (*pc++)
        {
            case 'r':
                uOpenMode |= RTFILE_O_READ;
                break;

            case 'w':
                uOpenMode |= RTFILE_O_WRITE;
                break;

            default:
                /* Silently skip unknown values. */
                break;
        }
    }

    return uOpenMode;
}

int GuestFile::onFileNotify(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, GuestCtrlCallback *pCallback, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData)
{
    AssertPtrReturn(pCallback, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);

    if (pSvcCbData->mParms < 3)
        return VERR_INVALID_PARAMETER;

    uint32_t uType;
    void *pvData; uint32_t cbData;
    /* pSvcCb->mpaParms[0] always contains the context ID. */
    pSvcCbData->mpaParms[1].getUInt32(&uType);
    pSvcCbData->mpaParms[2].getPointer(&pvData, &cbData);

    LogFlowThisFunc(("strName=%s, uType=%RU32, pvData=%p, cbData=%RU32, pCallback=%p\n",
                     mData.mOpenInfo.mFileName.c_str(), uType, pvData, cbData, pCallback));

    /* Signal callback in every case (if available). */
    int vrc = VINF_SUCCESS;
    if (pCallback)
    {
        vrc = pCallback->SetData(pvData, cbData);

        int rc2 = pCallback->Signal();
        if (RT_SUCCESS(vrc))
            vrc = rc2;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, GuestCtrlCallback *pCallback, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData)
{
    AssertPtrReturn(pCallback, VERR_INVALID_POINTER);

    LogFlowThisFunc(("strFile=%s, pCallback=%p\n", mData.mOpenInfo.mFileName.c_str(), pCallback));

    /* First, signal callback in every case. */
    if (pCallback)
        pCallback->Signal();

    /** @todo More on onGuestDisconnected? */
    int vrc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::openFile(int *pGuestRc)
{
    LogFlowThisFunc(("strFile=%s, strOpenMode=%s, strDisposition=%s, uCreationMode=%RU32\n",
                     mData.mOpenInfo.mFileName.c_str(), mData.mOpenInfo.mOpenMode.c_str(),
                     mData.mOpenInfo.mDisposition.c_str(), mData.mOpenInfo.mCreationMode));

    /* Wait until the caller function (if kicked off by a thread)
     * has returned and continue operation. */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(mData.mSession);
    uint32_t uProtocol = mData.mSession->getProtocolVersion();
    if (uProtocol < 2)
        return VERR_NOT_SUPPORTED;

    int vrc = VINF_SUCCESS;
    uint32_t uContextID = 0;

    GuestCtrlCallback *pCallbackOpen;
    try
    {
        pCallbackOpen = new GuestCtrlCallback();
    }
    catch(std::bad_alloc &)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(vrc))
    {
        /* Create callback and add it to the map. */
        vrc = pCallbackOpen->Init(CALLBACKTYPE_FILE_OPEN);
        if (RT_SUCCESS(vrc))
            vrc = callbackAdd(pCallbackOpen, &uContextID);
    }

    if (RT_SUCCESS(vrc))
    {
        GuestSession *pSession = mData.mSession;
        AssertPtr(pSession);

        const GuestCredentials &sessionCreds = pSession->getCredentials();

        if (RT_SUCCESS(vrc))
        {
            /* Prepare HGCM call. */
            VBOXHGCMSVCPARM paParms[8];
            int i = 0;
            paParms[i++].setUInt32(uContextID);
            paParms[i++].setPointer((void*)mData.mOpenInfo.mFileName.c_str(),
                                    (ULONG)mData.mOpenInfo.mFileName.length() + 1);
            paParms[i++].setPointer((void*)mData.mOpenInfo.mOpenMode.c_str(),
                                    (ULONG)mData.mOpenInfo.mOpenMode.length() + 1);
            paParms[i++].setPointer((void*)mData.mOpenInfo.mDisposition.c_str(),
                                    (ULONG)mData.mOpenInfo.mDisposition.length() + 1);
            paParms[i++].setUInt32(mData.mOpenInfo.mCreationMode);
            paParms[i++].setUInt64(mData.mOpenInfo.mInitialOffset);

            /* Note: Don't hold the write lock in here. */
            vrc = sendCommand(HOST_FILE_OPEN, i, paParms);
        }

        /* Drop the write lock again before waiting. */
        alock.release();

        if (RT_SUCCESS(vrc))
        {
            /*
             * Let's wait for the process being started.
             * Note: Be sure not keeping a AutoRead/WriteLock here.
             */
            LogFlowThisFunc(("Waiting for callback (30s) ...\n"));
            vrc = pCallbackOpen->Wait(30 *  1000 /* Wait 30s max. */);
            if (RT_SUCCESS(vrc)) /* Wait was successful, check for supplied information. */
            {
                int guestRc = pCallbackOpen->GetResultCode();
                if (RT_SUCCESS(guestRc))
                {

                }

                if (pGuestRc)
                    *pGuestRc = guestRc;
                LogFlowThisFunc(("Callback returned rc=%Rrc\n", guestRc));
            }
            else
                vrc = VERR_TIMEOUT;
        }

        AutoWriteLock awlock(this COMMA_LOCKVAL_SRC_POS);

        AssertPtr(pCallbackOpen);
        int rc2 = callbackRemove(uContextID);
        if (RT_SUCCESS(vrc))
            vrc = rc2;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestFile::Close(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AssertPtr(mData.mSession);
    int rc = mData.mSession->fileRemoveFromList(this);

    /*
     * Release autocaller before calling uninit.
     */
    autoCaller.release();

    uninit();

    LogFlowFuncLeaveRC(rc);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::QueryInfo(IFsObjInfo **aInfo)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::Read(ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::ReadAt(LONG64 aOffset, ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::Seek(LONG64 aOffset, FileSeekType_T aType)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::SetACL(IN_BSTR aACL)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::Write(ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::WriteAt(LONG64 aOffset, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

