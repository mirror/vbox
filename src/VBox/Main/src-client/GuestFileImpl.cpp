
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
#include "VirtualBoxErrorInfoImpl.h"

#include "Global.h"
#include "AutoCaller.h"
#include "VBoxEvents.h"

#include <iprt/cpp/utils.h> /* For unconst(). */
#include <iprt/file.h>

#include <VBox/com/array.h>
#include <VBox/com/listeners.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>


/**
 * Internal listener class to serve events in an
 * active manner, e.g. without polling delays.
 */
class GuestFileListener
{
public:

    GuestFileListener(void)
    {
    }

    HRESULT init(GuestFile *pFile)
    {
        mFile = pFile;
        return S_OK;
    }

    void uninit(void)
    {
        mFile.setNull();
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnGuestFileStateChanged:
            case VBoxEventType_OnGuestFileOffsetChanged:
            case VBoxEventType_OnGuestFileRead:
            case VBoxEventType_OnGuestFileWrite:
            {
                Assert(!mFile.isNull());
                int rc2 = mFile->signalWaitEvents(aType, aEvent);
#ifdef DEBUG_andy
                LogFlowFunc(("Signalling events of type=%ld, file=%p resulted in rc=%Rrc\n",
                             aType, mFile, rc2));
#endif
                break;
            }

            default:
                AssertMsgFailed(("Unhandled event %ld\n", aType));
                break;
        }

        return S_OK;
    }

private:

    ComObjPtr<GuestFile> mFile;
};
typedef ListenerImpl<GuestFileListener, GuestFile*> GuestFileListenerImpl;

VBOX_LISTENER_DECLARE(GuestFileListenerImpl)

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

/**
 * Initializes a file object but does *not* open the file on the guest
 * yet. This is done in the dedidcated openFile call.
 *
 * @return  IPRT status code.
 * @param   pConsole                Pointer to console object.
 * @param   pSession                Pointer to session object.
 * @param   uFileID                 Host-based file ID (part of the context ID).
 * @param   openInfo                File opening information.
 */
int GuestFile::init(Console *pConsole, GuestSession *pSession,
                    ULONG uFileID, const GuestFileOpenInfo &openInfo)
{
    LogFlowThisFunc(("pConsole=%p, pSession=%p, uFileID=%RU32, strPath=%s\n",
                     pConsole, pSession, uFileID, openInfo.mFileName.c_str()));

    AssertPtrReturn(pConsole, VERR_INVALID_POINTER);
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

#ifndef VBOX_WITH_GUEST_CONTROL
    autoInitSpan.setSucceeded();
    return VINF_SUCCESS;
#else
    int vrc = bindToSession(pConsole, pSession, uFileID /* Object ID */);
    if (RT_SUCCESS(vrc))
    {
        mSession = pSession;

        mData.mID = uFileID;
        mData.mInitialSize = 0;
        mData.mStatus = FileStatus_Undefined;
        mData.mOpenInfo = openInfo;

        unconst(mEventSource).createObject();
        HRESULT hr = mEventSource->init(static_cast<IGuestFile*>(this));
        if (FAILED(hr))
            vrc = VERR_COM_UNEXPECTED;
    }

    if (RT_SUCCESS(vrc))
    {
        try
        {
            GuestFileListener *pListener = new GuestFileListener();
            ComObjPtr<GuestFileListenerImpl> thisListener;
            HRESULT hr = thisListener.createObject();
            if (SUCCEEDED(hr))
                hr = thisListener->init(pListener, this);

            if (SUCCEEDED(hr))
            {
                com::SafeArray <VBoxEventType_T> eventTypes;
                eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);
                eventTypes.push_back(VBoxEventType_OnGuestFileOffsetChanged);
                eventTypes.push_back(VBoxEventType_OnGuestFileRead);
                eventTypes.push_back(VBoxEventType_OnGuestFileWrite);
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

    if (RT_SUCCESS(vrc))
    {
        /* Confirm a successful initialization when it's the case. */
        autoInitSpan.setSucceeded();
    }
    else
        autoInitSpan.setFailed();

    LogFlowFuncLeaveRC(vrc);
    return vrc;
#endif /* VBOX_WITH_GUEST_CONTROL */
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
    baseUninit();

    mEventSource->UnregisterListener(mLocalListener);
    unconst(mEventSource).setNull();
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

STDMETHODIMP GuestFile::COMGETTER(Disposition)(BSTR *aDisposition)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aDisposition);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mOpenInfo.mDisposition.cloneTo(aDisposition);

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::COMGETTER(EventSource)(IEventSource ** aEventSource)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    CheckComArgOutPointerValid(aEventSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* No need to lock - lifetime constant. */
    mEventSource.queryInterfaceTo(aEventSource);

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

STDMETHODIMP GuestFile::COMGETTER(Id)(ULONG *aID)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aID);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aID = mData.mID;

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

STDMETHODIMP GuestFile::COMGETTER(OpenMode)(BSTR *aOpenMode)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    CheckComArgOutPointerValid(aOpenMode);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mOpenInfo.mOpenMode.cloneTo(aOpenMode);

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::COMGETTER(Status)(FileStatus_T *aStatus)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aStatus = mData.mStatus;

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

    int vrc;
    switch (pCbCtx->uFunction)
    {
        case GUEST_DISCONNECTED:
            vrc = onGuestDisconnected(pCbCtx, pSvcCb);
            break;

        case GUEST_FILE_NOTIFY:
            vrc = onFileNotify(pCbCtx, pSvcCb);
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

int GuestFile::closeFile(int *pGuestRc)
{
    LogFlowThisFunc(("strFile=%s\n", mData.mOpenInfo.mFileName.c_str()));

    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mID /* Guest file ID */);

    vrc = sendCommand(HOST_FILE_CLOSE, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForStatusChange(pEvent, 30 * 1000 /* Timeout in ms */,
                                  NULL /* FileStatus */, pGuestRc);
    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/* static */
Utf8Str GuestFile::guestErrorToString(int guestRc)
{
    Utf8Str strError;

    /** @todo pData->u32Flags: int vs. uint32 -- IPRT errors are *negative* !!! */
    switch (guestRc)
    {
        case VERR_ALREADY_EXISTS:
            strError += Utf8StrFmt(tr("File already exists"));
            break;

        case VERR_FILE_NOT_FOUND:
            strError += Utf8StrFmt(tr("File not found"));
            break;

        case VERR_NET_HOST_NOT_FOUND:
            strError += Utf8StrFmt(tr("Host name not found"));
            break;

        case VERR_SHARING_VIOLATION:
            strError += Utf8StrFmt(tr("Sharing violation"));
            break;

        default:
            strError += Utf8StrFmt("%Rrc", guestRc);
            break;
    }

    return strError;
}

int GuestFile::onFileNotify(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);

    LogFlowThisFuncEnter();

    if (pSvcCbData->mParms < 3)
        return VERR_INVALID_PARAMETER;

    int vrc = VINF_SUCCESS;

    int idx = 1; /* Current parameter index. */
    CALLBACKDATA_FILE_NOTIFY dataCb;
    /* pSvcCb->mpaParms[0] always contains the context ID. */
    pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.uType);
    pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.rc);

    FileStatus_T fileStatus = FileStatus_Undefined;
    int guestRc = (int)dataCb.rc; /* uint32_t vs. int. */

    LogFlowFunc(("uType=%RU32, guestRc=%Rrc\n",
                 dataCb.uType, guestRc));

    if (RT_FAILURE(guestRc))
    {
        int rc2 = setFileStatus(FileStatus_Error, guestRc);
        AssertRC(rc2);

        return VINF_SUCCESS; /* Report to the guest. */
    }

    switch (dataCb.uType)
    {
        case GUEST_FILE_NOTIFYTYPE_ERROR:
        {
            int rc2 = setFileStatus(FileStatus_Error, guestRc);
            AssertRC(rc2);
            break;
        }

        case GUEST_FILE_NOTIFYTYPE_OPEN:
        {
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.u.open.uHandle);

                {
                    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
                    AssertMsg(mData.mID == VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pCbCtx->uContextID),
                              ("File ID %RU32 does not match context ID %RU32\n", mData.mID,
                               VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pCbCtx->uContextID)));

                    /* Set the initial offset. On the guest the whole opening operation
                     * would fail if an initial seek isn't possible. */
                    mData.mOffCurrent = mData.mOpenInfo.mInitialOffset;
                }

                /* Set the process status. */
                int rc2 = setFileStatus(FileStatus_Open, guestRc);
                if (RT_SUCCESS(vrc))
                    vrc = rc2;
            }
            else
                vrc = VERR_NOT_SUPPORTED;

            break;
        }

        case GUEST_FILE_NOTIFYTYPE_CLOSE:
        {
            int rc2 = setFileStatus(FileStatus_Closed, guestRc);
            AssertRC(rc2);

            break;
        }

        case GUEST_FILE_NOTIFYTYPE_READ:
        {
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getPointer(&dataCb.u.read.pvData,
                                                       &dataCb.u.read.cbData);
                uint32_t cbRead = dataCb.u.read.cbData;
                if (cbRead)
                {
                    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

                    mData.mOffCurrent += cbRead;

                    alock.release();

                    com::SafeArray<BYTE> data((size_t)cbRead);
                    data.initFrom((BYTE*)dataCb.u.read.pvData, cbRead);

                    fireGuestFileReadEvent(mEventSource, mSession, this, mData.mOffCurrent,
                                           cbRead, ComSafeArrayAsInParam(data));
                }
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;
        }

        case GUEST_FILE_NOTIFYTYPE_WRITE:
        {
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.u.write.cbWritten);

                AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

                mData.mOffCurrent += dataCb.u.write.cbWritten;
                uint64_t uOffCurrent = mData.mOffCurrent;

                alock.release();

                if (dataCb.u.write.cbWritten)
                    fireGuestFileWriteEvent(mEventSource, mSession, this, uOffCurrent,
                                            dataCb.u.write.cbWritten);
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;
        }

        case GUEST_FILE_NOTIFYTYPE_SEEK:
        {
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt64(&dataCb.u.seek.uOffActual);

                AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

                mData.mOffCurrent = dataCb.u.seek.uOffActual;
                uint64_t uOffCurrent = mData.mOffCurrent;

                alock.release();

                if (dataCb.u.seek.uOffActual)
                    fireGuestFileOffsetChangedEvent(mEventSource, mSession, this,
                                                    uOffCurrent, 0 /* Processed */);
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;
        }

        case GUEST_FILE_NOTIFYTYPE_TELL:
        {
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt64(&dataCb.u.tell.uOffActual);

                AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

                if (mData.mOffCurrent != dataCb.u.tell.uOffActual)
                {
                    mData.mOffCurrent = dataCb.u.tell.uOffActual;
                    uint64_t uOffCurrent = mData.mOffCurrent;

                    alock.release();

                    fireGuestFileOffsetChangedEvent(mEventSource, mSession, this,
                                                    uOffCurrent, 0 /* Processed */);
                }
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;
        }

        default:
            vrc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowThisFunc(("uType=%RU32, guestRc=%Rrc\n",
                     dataCb.uType, dataCb.rc));

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);

    int vrc = setFileStatus(FileStatus_Down, VINF_SUCCESS);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::openFile(uint32_t uTimeoutMS, int *pGuestRc)
{
    LogFlowThisFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("strFile=%s, strOpenMode=%s, strDisposition=%s, uCreationMode=%RU32, uOffset=%RU64\n",
                     mData.mOpenInfo.mFileName.c_str(), mData.mOpenInfo.mOpenMode.c_str(),
                     mData.mOpenInfo.mDisposition.c_str(), mData.mOpenInfo.mCreationMode, mData.mOpenInfo.mInitialOffset));
    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[8];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setPointer((void*)mData.mOpenInfo.mFileName.c_str(),
                            (ULONG)mData.mOpenInfo.mFileName.length() + 1);
    paParms[i++].setPointer((void*)mData.mOpenInfo.mOpenMode.c_str(),
                            (ULONG)mData.mOpenInfo.mOpenMode.length() + 1);
    paParms[i++].setPointer((void*)mData.mOpenInfo.mDisposition.c_str(),
                            (ULONG)mData.mOpenInfo.mDisposition.length() + 1);
    paParms[i++].setPointer((void*)mData.mOpenInfo.mSharingMode.c_str(),
                            (ULONG)mData.mOpenInfo.mSharingMode.length() + 1);
    paParms[i++].setUInt32(mData.mOpenInfo.mCreationMode);
    paParms[i++].setUInt64(mData.mOpenInfo.mInitialOffset);

    alock.release(); /* Drop read lock before sending. */

    vrc = sendCommand(HOST_FILE_OPEN, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForStatusChange(pEvent, uTimeoutMS,
                                  NULL /* FileStatus */, pGuestRc);

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::readData(uint32_t uSize, uint32_t uTimeoutMS,
                        void* pvData, uint32_t cbData, uint32_t* pcbRead)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("uSize=%RU32, uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uSize, uTimeoutMS, pvData, cbData));
    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);
        eventTypes.push_back(VBoxEventType_OnGuestFileRead);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt32(uSize /* Size (in bytes) to read */);

    uint32_t cbRead;
    vrc = sendCommand(HOST_FILE_READ, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForRead(pEvent, uTimeoutMS, pvData, cbData, &cbRead);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("cbRead=%RU32\n", cbRead));

        if (pcbRead)
            *pcbRead = cbRead;
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::readDataAt(uint64_t uOffset, uint32_t uSize, uint32_t uTimeoutMS,
                          void* pvData, size_t cbData, size_t* pcbRead)
{
    LogFlowThisFunc(("uOffset=%RU64, uSize=%RU32, uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uOffset, uSize, uTimeoutMS, pvData, cbData));
    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);
        eventTypes.push_back(VBoxEventType_OnGuestFileRead);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt64(uOffset /* Offset (in bytes) to start reading */);
    paParms[i++].setUInt32(uSize /* Size (in bytes) to read */);

    uint32_t cbRead;
    vrc = sendCommand(HOST_FILE_READ_AT, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForRead(pEvent, uTimeoutMS, pvData, cbData, &cbRead);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("cbRead=%RU32\n", cbRead));

        if (pcbRead)
            *pcbRead = cbRead;
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::seekAt(uint64_t uOffset, GUEST_FILE_SEEKTYPE eSeekType,
                      uint32_t uTimeoutMS, uint64_t *puOffset)
{
    LogFlowThisFunc(("uOffset=%RU64, uTimeoutMS=%RU32\n",
                     uOffset, uTimeoutMS));
    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);
        eventTypes.push_back(VBoxEventType_OnGuestFileOffsetChanged);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt32(eSeekType /* Seek method */);
    paParms[i++].setUInt64(uOffset /* Offset (in bytes) to start reading */);

    vrc = sendCommand(HOST_FILE_SEEK, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForOffsetChange(pEvent, uTimeoutMS, puOffset);

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/* static */
HRESULT GuestFile::setErrorExternal(VirtualBoxBase *pInterface, int guestRc)
{
    AssertPtr(pInterface);
    AssertMsg(RT_FAILURE(guestRc), ("Guest rc does not indicate a failure when setting error\n"));

    return pInterface->setError(VBOX_E_IPRT_ERROR, GuestFile::guestErrorToString(guestRc).c_str());
}

int GuestFile::setFileStatus(FileStatus_T fileStatus, int fileRc)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("oldStatus=%ld, newStatus=%ld, fileRc=%Rrc\n",
                     mData.mStatus, fileStatus, fileRc));

#ifdef VBOX_STRICT
    if (fileStatus == FileStatus_Error)
    {
        AssertMsg(RT_FAILURE(fileRc), ("Guest rc must be an error (%Rrc)\n", fileRc));
    }
    else
        AssertMsg(RT_SUCCESS(fileRc), ("Guest rc must not be an error (%Rrc)\n", fileRc));
#endif

    if (mData.mStatus != fileStatus)
    {
        mData.mStatus    = fileStatus;
        mData.mLastError = fileRc;

        ComObjPtr<VirtualBoxErrorInfo> errorInfo;
        HRESULT hr = errorInfo.createObject();
        ComAssertComRC(hr);
        if (RT_FAILURE(fileRc))
        {
            hr = errorInfo->initEx(VBOX_E_IPRT_ERROR, fileRc,
                                   COM_IIDOF(IGuestFile), getComponentName(),
                                   guestErrorToString(fileRc));
            ComAssertComRC(hr);
        }

        alock.release(); /* Release lock before firing off event. */

        fireGuestFileStateChangedEvent(mEventSource, mSession,
                                       this, fileStatus, errorInfo);
    }

    return VINF_SUCCESS;
}

int GuestFile::waitForOffsetChange(GuestWaitEvent *pEvent,
                                   uint32_t uTimeoutMS, uint64_t *puOffset)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    VBoxEventType_T evtType;
    ComPtr<IEvent> pIEvent;
    int vrc = waitForEvent(pEvent, uTimeoutMS,
                           &evtType, pIEvent.asOutParam());
    if (RT_SUCCESS(vrc))
    {
        if (evtType == VBoxEventType_OnGuestFileOffsetChanged)
        {
            if (puOffset)
            {
                ComPtr<IGuestFileOffsetChangedEvent> pFileEvent = pIEvent;
                Assert(!pFileEvent.isNull());

                HRESULT hr = pFileEvent->COMGETTER(Offset)((LONG64*)puOffset);
                ComAssertComRC(hr);
            }
        }
        else
            vrc = VWRN_GSTCTL_OBJECTSTATE_CHANGED;
    }

    return vrc;
}

int GuestFile::waitForRead(GuestWaitEvent *pEvent,
                           uint32_t uTimeoutMS, void *pvData, size_t cbData, uint32_t *pcbRead)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    VBoxEventType_T evtType;
    ComPtr<IEvent> pIEvent;
    int vrc = waitForEvent(pEvent, uTimeoutMS,
                           &evtType, pIEvent.asOutParam());
    if (RT_SUCCESS(vrc))
    {
        if (evtType == VBoxEventType_OnGuestFileRead)
        {
            ComPtr<IGuestFileReadEvent> pFileEvent = pIEvent;
            Assert(!pFileEvent.isNull());

            HRESULT hr;
            if (pvData)
            {
                com::SafeArray <BYTE> data;
                hr = pFileEvent->COMGETTER(Data)(ComSafeArrayAsOutParam(data));
                ComAssertComRC(hr);
                size_t cbRead = data.size();
                if (   cbRead
                    && cbRead <= cbData)
                {
                    memcpy(pvData, data.raw(), data.size());
                }
                else
                    vrc = VERR_BUFFER_OVERFLOW;
            }
            if (pcbRead)
            {
                hr = pFileEvent->COMGETTER(Processed)((ULONG*)pcbRead);
                ComAssertComRC(hr);
            }
        }
        else
            vrc = VWRN_GSTCTL_OBJECTSTATE_CHANGED;
    }

    return vrc;
}

int GuestFile::waitForStatusChange(GuestWaitEvent *pEvent, uint32_t uTimeoutMS,
                                   FileStatus_T *pFileStatus, int *pGuestRc)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);
    /* pFileStatus is optional. */

    VBoxEventType_T evtType;
    ComPtr<IEvent> pIEvent;
    int vrc = waitForEvent(pEvent, uTimeoutMS,
                           &evtType, pIEvent.asOutParam());
    if (RT_SUCCESS(vrc))
    {
        Assert(evtType == VBoxEventType_OnGuestFileStateChanged);
        ComPtr<IGuestFileStateChangedEvent> pFileEvent = pIEvent;
        Assert(!pFileEvent.isNull());

        HRESULT hr;
        if (pFileStatus)
        {
            hr = pFileEvent->COMGETTER(Status)(pFileStatus);
            ComAssertComRC(hr);
        }

        ComPtr<IVirtualBoxErrorInfo> errorInfo;
        hr = pFileEvent->COMGETTER(Error)(errorInfo.asOutParam());
        ComAssertComRC(hr);

        LONG lGuestRc;
        hr = errorInfo->COMGETTER(ResultDetail)(&lGuestRc);
        ComAssertComRC(hr);

        LogFlowThisFunc(("resultDetail=%RI32 (rc=%Rrc)\n",
                         lGuestRc, lGuestRc));

        if (RT_FAILURE((int)lGuestRc))
            vrc = VERR_GSTCTL_GUEST_ERROR;

        if (pGuestRc)
            *pGuestRc = (int)lGuestRc;
    }

    return vrc;
}

int GuestFile::waitForWrite(GuestWaitEvent *pEvent,
                            uint32_t uTimeoutMS, uint32_t *pcbWritten)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    VBoxEventType_T evtType;
    ComPtr<IEvent> pIEvent;
    int vrc = waitForEvent(pEvent, uTimeoutMS,
                           &evtType, pIEvent.asOutParam());
    if (RT_SUCCESS(vrc))
    {
        if (evtType == VBoxEventType_OnGuestFileWrite)
        {
            if (pcbWritten)
            {
                ComPtr<IGuestFileWriteEvent> pFileEvent = pIEvent;
                Assert(!pFileEvent.isNull());

                HRESULT hr = pFileEvent->COMGETTER(Processed)((ULONG*)pcbWritten);
                ComAssertComRC(hr);
            }
        }
        else
            vrc = VWRN_GSTCTL_OBJECTSTATE_CHANGED;
    }

    return vrc;
}

int GuestFile::writeData(uint32_t uTimeoutMS, void *pvData, uint32_t cbData,
                         uint32_t *pcbWritten)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uTimeoutMS, pvData, cbData));
    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);
        eventTypes.push_back(VBoxEventType_OnGuestFileWrite);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[8];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt32(cbData /* Size (in bytes) to write */);
    paParms[i++].setPointer(pvData, cbData);

    uint32_t cbWritten;
    vrc = sendCommand(HOST_FILE_WRITE, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForWrite(pEvent, uTimeoutMS, &cbWritten);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("cbWritten=%RU32\n", cbWritten));

        if (cbWritten)
            *pcbWritten = cbWritten;
    }

    unregisterWaitEvent(pEvent);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::writeDataAt(uint64_t uOffset, uint32_t uTimeoutMS,
                           void *pvData, uint32_t cbData, uint32_t *pcbWritten)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("uOffset=%RU64, uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uOffset, uTimeoutMS, pvData, cbData));
    int vrc;

    GuestWaitEvent *pEvent = NULL;
    std::list < VBoxEventType_T > eventTypes;
    try
    {
        eventTypes.push_back(VBoxEventType_OnGuestFileStateChanged);
        eventTypes.push_back(VBoxEventType_OnGuestFileWrite);

        vrc = registerWaitEvent(eventTypes, &pEvent);
    }
    catch (std::bad_alloc)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return vrc;

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[8];
    int i = 0;
    paParms[i++].setUInt32(pEvent->ContextID());
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt64(uOffset /* Offset where to starting writing */);
    paParms[i++].setUInt32(cbData /* Size (in bytes) to write */);
    paParms[i++].setPointer(pvData, cbData);

    uint32_t cbWritten;
    vrc = sendCommand(HOST_FILE_WRITE_AT, i, paParms);
    if (RT_SUCCESS(vrc))
        vrc = waitForWrite(pEvent, uTimeoutMS, &cbWritten);

    if (RT_SUCCESS(vrc))
    {
        LogFlowThisFunc(("cbWritten=%RU32\n", cbWritten));

        if (cbWritten)
            *pcbWritten = cbWritten;
    }

    unregisterWaitEvent(pEvent);

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

    /* Close file on guest. */
    int guestRc;
    int rc = closeFile(&guestRc);
    /* On failure don't return here, instead do all the cleanup
     * work first and then return an error. */

    AssertPtr(mSession);
    int rc2 = mSession->fileRemoveFromList(this);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_GSTCTL_GUEST_ERROR)
            return GuestFile::setErrorExternal(this, guestRc);

        return setError(VBOX_E_IPRT_ERROR,
                        tr("Closing guest file failed with %Rrc\n"), rc);
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
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
    if (aToRead == 0)
        return setError(E_INVALIDARG, tr("The size to read is zero"));
    CheckComArgOutSafeArrayPointerValid(aData);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    com::SafeArray<BYTE> data((size_t)aToRead);
    Assert(data.size() >= aToRead);

    HRESULT hr = S_OK;

    uint32_t cbRead;
    int vrc = readData(aToRead, aTimeoutMS,
                       data.raw(), aToRead, &cbRead);
    if (RT_SUCCESS(vrc))
    {
        if (data.size() != cbRead)
            data.resize(cbRead);
        data.detachTo(ComSafeArrayOutArg(aData));
    }
    else
    {
        switch (vrc)
        {
            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Reading from file \"%s\" failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), vrc);
                break;
        }
    }

    LogFlowFuncLeaveRC(vrc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::ReadAt(LONG64 aOffset, ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    if (aToRead == 0)
        return setError(E_INVALIDARG, tr("The size to read is zero"));
    CheckComArgOutSafeArrayPointerValid(aData);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    com::SafeArray<BYTE> data((size_t)aToRead);
    Assert(data.size() >= aToRead);

    HRESULT hr = S_OK;

    size_t cbRead;
    int vrc = readDataAt(aOffset, aToRead, aTimeoutMS,
                         data.raw(), aToRead, &cbRead);
    if (RT_SUCCESS(vrc))
    {
        if (data.size() != cbRead)
            data.resize(cbRead);
        data.detachTo(ComSafeArrayOutArg(aData));
    }
    else
    {
        switch (vrc)
        {
            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Reading from file \"%s\" (at offset %RU64) failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), aOffset, vrc);
                break;
        }
    }

    LogFlowFuncLeaveRC(vrc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::Seek(LONG64 aOffset, FileSeekType_T aType)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GUEST_FILE_SEEKTYPE eSeekType;
    switch (aType)
    {
        case FileSeekType_Set:
            eSeekType = GUEST_FILE_SEEKTYPE_BEGIN;
            break;

        case FileSeekType_Current:
            eSeekType = GUEST_FILE_SEEKTYPE_CURRENT;
            break;

        default:
            return setError(E_INVALIDARG, tr("Invalid seek type specified"));
            break;
    }

    int vrc = seekAt(aOffset, eSeekType,
                     30 * 1000 /* 30s timeout */, NULL /* puOffset */);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Seeking file \"%s\" (to offset %RU64) failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), aOffset, vrc);
                break;
        }
    }

    LogFlowFuncLeaveRC(vrc);
    return hr;
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
    LogFlowThisFuncEnter();

    CheckComArgSafeArrayNotNull(aData);
    CheckComArgOutPointerValid(aWritten);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    com::SafeArray<BYTE> data(ComSafeArrayInArg(aData));
    int vrc = writeData(aTimeoutMS, data.raw(), (uint32_t)data.size(),
                        (uint32_t*)aWritten);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Writing %zubytes to file \"%s\" failed: %Rrc"),
                              data.size(), mData.mOpenInfo.mFileName.c_str(), vrc);
                break;
        }
    }

    LogFlowFuncLeaveRC(vrc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestFile::WriteAt(LONG64 aOffset, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgSafeArrayNotNull(aData);
    CheckComArgOutPointerValid(aWritten);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    com::SafeArray<BYTE> data(ComSafeArrayInArg(aData));
    int vrc = writeData(aTimeoutMS, data.raw(), (uint32_t)data.size(),
                         (uint32_t*)aWritten);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Writing %zubytes to file \"%s\" (at offset %RU64) failed: %Rrc"),
                              data.size(), mData.mOpenInfo.mFileName.c_str(), aOffset, vrc);
                break;
        }
    }

    LogFlowFuncLeaveRC(vrc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

