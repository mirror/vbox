
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
        mData.mID = 0;
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
            vrc = onGuestDisconnected(pCbCtx, pSvcCb, pCallback); /* Affects all callbacks. */
            break;

        case GUEST_FILE_NOTIFY:
            vrc = onFileNotify(pCbCtx, pSvcCb, pCallback);
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

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setUInt32(mData.mID /* Guest file ID */);

    int guestRc;
    int vrc = sendFileCommand(HOST_FILE_CLOSE, i, paParms, 30 * 1000 /* 30s timeout */,
                              &guestRc, NULL /* ppCallback */);
    if (pGuestRc)
        *pGuestRc = guestRc;

    LogFlowFuncLeaveRC(vrc);
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

/* static */
Utf8Str GuestFile::guestErrorToString(int guestRc)
{
    Utf8Str strError;

    /** @todo pData->u32Flags: int vs. uint32 -- IPRT errors are *negative* !!! */
    switch (guestRc)
    {
        case VERR_INVALID_VM_HANDLE:
            strError += Utf8StrFmt(tr("VMM device is not available (is the VM running?)"));
            break;

        case VERR_HGCM_SERVICE_NOT_FOUND:
            strError += Utf8StrFmt(tr("The guest execution service is not available"));
            break;

        case VERR_TIMEOUT:
            strError += Utf8StrFmt(tr("The guest did not respond within time"));
            break;

        case VERR_CANCELLED:
            strError += Utf8StrFmt(tr("The session operation was canceled"));
            break;

        case VERR_MAX_PROCS_REACHED:
            strError += Utf8StrFmt(tr("Maximum number of concurrent guest files has been reached"));
            break;

        case VERR_NOT_EQUAL: /** @todo Imprecise to the user; can mean anything and all. */
            strError += Utf8StrFmt(tr("Unable to retrieve requested information"));
            break;

        case VERR_NOT_FOUND:
            strError += Utf8StrFmt(tr("The guest execution service is not ready (yet)"));
            break;

        default:
            strError += Utf8StrFmt("%Rrc", guestRc);
            break;
    }

    return strError;
}

int GuestFile::onFileNotify(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData,
                            GuestCtrlCallback *pCallback)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);
    /* pCallback is optional. */

    if (pSvcCbData->mParms < 3)
        return VERR_INVALID_PARAMETER;

    int vrc = VINF_SUCCESS;

    int idx = 0; /* Current parameter index. */
    CALLBACKDATA_FILE_NOTIFY dataCb;
    /* pSvcCb->mpaParms[0] always contains the context ID. */
    pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.uType);
    pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.rc);

    switch (dataCb.uType)
    {
        case GUEST_FILE_NOTIFYTYPE_ERROR:
            /* No extra data. */
            break;

        case GUEST_FILE_NOTIFYTYPE_OPEN:
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.u.open.uHandle);

                AssertMsg(mData.mID == 0, ("File ID already set to %RU32\n", mData.mID));
                mData.mID = dataCb.u.open.uHandle;
                AssertMsg(mData.mID == VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pCbCtx->uContextID),
                          ("File ID %RU32 does not match context ID %RU32\n", mData.mID,
                           VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pCbCtx->uContextID)));
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;

        case GUEST_FILE_NOTIFYTYPE_CLOSE:
            /* No extra data. */
            break;

        case GUEST_FILE_NOTIFYTYPE_READ:
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getPointer(&dataCb.u.read.pvData,
                                                       &dataCb.u.read.cbData);

                mData.mOffCurrent += dataCb.u.read.cbData;
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;

        case GUEST_FILE_NOTIFYTYPE_WRITE:
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt32(&dataCb.u.write.cbWritten);

                mData.mOffCurrent += dataCb.u.write.cbWritten;
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;

        case GUEST_FILE_NOTIFYTYPE_SEEK:
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt64(&dataCb.u.seek.uOffActual);

                mData.mOffCurrent = dataCb.u.seek.uOffActual;
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;

        case GUEST_FILE_NOTIFYTYPE_TELL:
            if (pSvcCbData->mParms == 4)
            {
                pSvcCbData->mpaParms[idx++].getUInt64(&dataCb.u.tell.uOffActual);

                mData.mOffCurrent = dataCb.u.tell.uOffActual;
            }
            else
                vrc = VERR_NOT_SUPPORTED;
            break;

        default:
            vrc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowThisFunc(("strName=%s, uType=%RU32, rc=%Rrc, pCallback=%p\n",
                     mData.mOpenInfo.mFileName.c_str(), dataCb.uType, dataCb.rc, pCallback));

    int guestRc = (int)dataCb.rc; /* uint32_t vs. int. */
    if (RT_SUCCESS(vrc))
    {
        /* Nothing to do here yet. */
    }
    else if (vrc == VERR_NOT_SUPPORTED)
    {
        /* Also let the callback know. */
        guestRc = VERR_NOT_SUPPORTED;
    }

    /* Signal callback in every case (if available). */
    if (pCallback)
    {
        int rc2 = pCallback->SetData(&dataCb, sizeof(dataCb));
        if (RT_SUCCESS(vrc))
            vrc = rc2;
        rc2 = pCallback->Signal(guestRc);
        if (RT_SUCCESS(vrc))
            vrc = rc2;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData,
                                   GuestCtrlCallback *pCallback)
{
    AssertPtrReturn(pCbCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCbData, VERR_INVALID_POINTER);
    /* pCallback is optional. */

    LogFlowThisFunc(("strFile=%s, pCallback=%p\n",
                     mData.mOpenInfo.mFileName.c_str(), pCallback));

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

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[8];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setPointer((void*)mData.mOpenInfo.mFileName.c_str(),
                            (ULONG)mData.mOpenInfo.mFileName.length() + 1);
    paParms[i++].setPointer((void*)mData.mOpenInfo.mOpenMode.c_str(),
                            (ULONG)mData.mOpenInfo.mOpenMode.length() + 1);
    paParms[i++].setPointer((void*)mData.mOpenInfo.mDisposition.c_str(),
                            (ULONG)mData.mOpenInfo.mDisposition.length() + 1);
    paParms[i++].setUInt32(mData.mOpenInfo.mCreationMode);
    paParms[i++].setUInt64(mData.mOpenInfo.mInitialOffset);

    int vrc = sendFileCommand(HOST_FILE_OPEN, i, paParms, 30 * 1000 /* 30s timeout */,
                              pGuestRc, NULL /* ppCallback */);

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::readData(uint32_t uSize, uint32_t uTimeoutMS, void *pvData, size_t cbData,
                        size_t *pcbRead, int *pGuestRc)
{
    LogFlowThisFunc(("uSize=%RU32, uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uSize, uTimeoutMS, pvData, cbData));

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt32(uSize /* Size (in bytes) to read */);

    GuestCtrlCallback *pCallback = NULL; int guestRc;
    int vrc = sendFileCommand(HOST_FILE_READ, i, paParms, uTimeoutMS,
                              &guestRc, &pCallback);

    if (RT_SUCCESS(vrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        Assert(pCallback->GetDataSize() == sizeof(CALLBACKDATA_FILE_NOTIFY));
        PCALLBACKDATA_FILE_NOTIFY pData = (PCALLBACKDATA_FILE_NOTIFY)pCallback->GetDataRaw();
        AssertPtr(pData);
        Assert(pData->uType == GUEST_FILE_NOTIFYTYPE_READ);

        size_t cbRead = pData->u.read.cbData;
        if (cbRead)
        {
            Assert(cbData >= cbRead);
            memcpy(pvData, pData->u.read.pvData, cbRead);
        }

        LogFlowThisFunc(("cbRead=%RU32\n", cbRead));

        if (pcbRead)
            *pcbRead = cbRead;
    }

    callbackDelete(pCallback);

    if (pGuestRc)
        *pGuestRc = guestRc;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::readDataAt(uint64_t uOffset, uint32_t uSize, uint32_t uTimeoutMS,
                          void *pvData, size_t cbData,
                          size_t *pcbRead, int *pGuestRc)
{
    LogFlowThisFunc(("uOffset=%RU64, uSize=%RU32, uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uOffset, uSize, uTimeoutMS, pvData, cbData));

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt64(uOffset /* Offset (in bytes) to start reading */);
    paParms[i++].setUInt32(uSize /* Size (in bytes) to read */);

    GuestCtrlCallback *pCallback = NULL; int guestRc;
    int vrc = sendFileCommand(HOST_FILE_READ_AT, i, paParms, uTimeoutMS,
                              &guestRc, &pCallback);

    if (RT_SUCCESS(vrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        Assert(pCallback->GetDataSize() == sizeof(CALLBACKDATA_FILE_NOTIFY));
        PCALLBACKDATA_FILE_NOTIFY pData = (PCALLBACKDATA_FILE_NOTIFY)pCallback->GetDataRaw();
        AssertPtr(pData);
        Assert(pData->uType == GUEST_FILE_NOTIFYTYPE_READ);

        size_t cbRead = pData->u.read.cbData;
        if (cbRead)
        {
            Assert(cbData >= cbRead);
            memcpy(pvData, pData->u.read.pvData, cbRead);
        }

        LogFlowThisFunc(("cbRead=%RU32\n", cbRead));

        if (pcbRead)
            *pcbRead = cbRead;
    }

    callbackDelete(pCallback);

    if (pGuestRc)
        *pGuestRc = guestRc;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::seekAt(uint64_t uOffset, GUEST_FILE_SEEKTYPE eSeekType,
                      uint32_t uTimeoutMS, int *pGuestRc)
{
    LogFlowThisFunc(("uOffset=%RU64, uTimeoutMS=%RU32\n",
                     uOffset, uTimeoutMS));

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt32(eSeekType /* Seek method */);
    paParms[i++].setUInt64(uOffset /* Offset (in bytes) to start reading */);

    int guestRc;
    int vrc = sendFileCommand(HOST_FILE_SEEK, i, paParms, uTimeoutMS,
                              &guestRc, NULL /* ppCallback */);
    if (pGuestRc)
        *pGuestRc = guestRc;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

/**
 * Handles the common parts of sending a file command to the guest.
 * If ppCallback is returned it must be removed via callbackRemove()
 * by the caller in any case.
 *
 * @return  IPRT status code.
 * @param   uFunction               HGCM function of command to send.
 * @param   uParms                  Number of HGCM parameters to send.
 *                                  At least one parameter must be present.
 * @param   paParms                 Array of HGCM parameters to send.
 *                                  Index [0] must not be used and will be
 *                                  filled out by the function.
 * @param   uTimeoutMS              Timeout (in ms).
 * @param   pGuestRc                Guest result. Optional.
 * @param   ppCallback              Pointer which will receive the callback for
 *                                  further processing by the caller. Must
 *                                  be deleted with callbackDelete() when done. Optional.
 */
int GuestFile::sendFileCommand(uint32_t uFunction, uint32_t uParms, PVBOXHGCMSVCPARM paParms,
                               uint32_t uTimeoutMS, int *pGuestRc, GuestCtrlCallback **ppCallback)
{
    AssertReturn(uParms, VERR_INVALID_PARAMETER);
    AssertPtrReturn(paParms, VERR_INVALID_POINTER);
    /** pGuestRc is optional. */
    /** ppCallback is optional. */

    LogFlowThisFunc(("strFile=%s, uFunction=%RU32, uParms=%RU32\n",
                     mData.mOpenInfo.mFileName.c_str(), uFunction, uParms));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertPtr(mData.mSession);
    uint32_t uProtocol = mData.mSession->getProtocolVersion();
    if (uProtocol < 2)
        return VERR_NOT_SUPPORTED;

    int vrc = VINF_SUCCESS;
    uint32_t uContextID = 0;

    GuestCtrlCallback *pCallback;
    try
    {
        pCallback = new GuestCtrlCallback();
    }
    catch(std::bad_alloc &)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(vrc))
    {
        /* Create callback and add it to the map. */
        vrc = pCallback->Init(CALLBACKTYPE_FILE_NOTIFY);
        if (RT_SUCCESS(vrc))
            vrc = callbackAdd(pCallback, &uContextID);
    }

    if (RT_SUCCESS(vrc))
    {
        /* Assign context ID. */
        paParms[0].setUInt32(uContextID);

        GuestSession *pSession = mData.mSession;
        AssertPtr(pSession);

        alock.release(); /* Drop the write lock again. */

        /* Note: Don't hold the write lock in here. */
        vrc = sendCommand(uFunction, uParms, paParms);

        if (RT_SUCCESS(vrc))
        {
            /*
             * Let's wait for the process being started.
             * Note: Be sure not keeping a AutoRead/WriteLock here.
             */
            LogFlowThisFunc(("Waiting for callback (%RU32ms) ...\n",
                             uTimeoutMS));
            vrc = pCallback->Wait(uTimeoutMS);
            if (RT_SUCCESS(vrc)) /* Wait was successful, check for supplied information. */
            {
                int guestRc = pCallback->GetResultCode();
                if (RT_SUCCESS(guestRc))
                {
                    /* Nothing to do here yet. */
                }
                else
                    vrc = VERR_GSTCTL_GUEST_ERROR;

                if (pGuestRc)
                    *pGuestRc = guestRc;
                LogFlowThisFunc(("Callback returned rc=%Rrc\n", guestRc));
            }
        }

        alock.acquire(); /* Get write lock again. */

        AssertPtr(pCallback);
        int rc2 = callbackRemove(uContextID);
        if (RT_SUCCESS(vrc))
            vrc = rc2;

        if (ppCallback)
        {
            /* Return callback to the caller which then will be
             * responsible for removing it. Don't forget to lock write
             * access before using this callback then! */
            *ppCallback = pCallback;
        }
        else
        {
            delete pCallback;
        }
    }

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

int GuestFile::writeData(uint32_t uTimeoutMS, void *pvData, size_t cbData,
                         uint32_t *pcbWritten, int *pGuestRc)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uTimeoutMS, pvData, cbData));

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt32(cbData /* Size (in bytes) to write */);
    paParms[i++].setPointer(pvData, cbData);

    GuestCtrlCallback *pCallback = NULL; int guestRc;
    int vrc = sendFileCommand(HOST_FILE_WRITE, i, paParms, uTimeoutMS,
                              &guestRc, &pCallback);

    if (RT_SUCCESS(vrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        Assert(pCallback->GetDataSize() == sizeof(CALLBACKDATA_FILE_NOTIFY));
        PCALLBACKDATA_FILE_NOTIFY pData = (PCALLBACKDATA_FILE_NOTIFY)pCallback->GetDataRaw();
        AssertPtr(pData);
        Assert(pData->uType == GUEST_FILE_NOTIFYTYPE_WRITE);

        size_t cbWritten = pData->u.write.cbWritten;
        LogFlowThisFunc(("cbWritten=%RU32\n", cbWritten));

        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    callbackDelete(pCallback);

    if (pGuestRc)
        *pGuestRc = guestRc;

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestFile::writeDataAt(uint64_t uOffset, uint32_t uTimeoutMS,
                           void *pvData, size_t cbData,
                           uint32_t *pcbWritten, int *pGuestRc)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("uOffset=%RU64, uTimeoutMS=%RU32, pvData=%p, cbData=%zu\n",
                     uOffset, uTimeoutMS, pvData, cbData));

    /* Prepare HGCM call. */
    VBOXHGCMSVCPARM paParms[4];
    int i = 1; /* Context ID will be set in sendFileComannd(). */
    paParms[i++].setUInt32(mData.mID /* File handle */);
    paParms[i++].setUInt64(uOffset /* Offset where to starting writing */);
    paParms[i++].setUInt32(cbData /* Size (in bytes) to write */);
    paParms[i++].setPointer(pvData, cbData);

    GuestCtrlCallback *pCallback = NULL; int guestRc;
    int vrc = sendFileCommand(HOST_FILE_WRITE_AT, i, paParms, uTimeoutMS,
                              &guestRc, &pCallback);

    if (RT_SUCCESS(vrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        Assert(pCallback->GetDataSize() == sizeof(CALLBACKDATA_FILE_NOTIFY));
        PCALLBACKDATA_FILE_NOTIFY pData = (PCALLBACKDATA_FILE_NOTIFY)pCallback->GetDataRaw();
        AssertPtr(pData);
        Assert(pData->uType == GUEST_FILE_NOTIFYTYPE_WRITE);

        size_t cbWritten = pData->u.write.cbWritten;
        LogFlowThisFunc(("cbWritten=%RU32\n", cbWritten));

        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    callbackDelete(pCallback);

    if (pGuestRc)
        *pGuestRc = guestRc;

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

    AssertPtr(mData.mSession);
    int rc2 = mData.mSession->fileRemoveFromList(this);
    if (RT_SUCCESS(rc))
        rc = rc2;

    /*
     * Release autocaller before calling uninit.
     */
    autoCaller.release();

    uninit();

    LogFlowFuncLeaveRC(rc);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_GSTCTL_GUEST_ERROR)
            return GuestFile::setErrorExternal(this, guestRc);

        return setError(VBOX_E_IPRT_ERROR,
                        tr("Closing guest file failed with %Rrc\n"), rc);
    }

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

    size_t cbRead; int guestRc;
    int vrc = readData(aToRead, aTimeoutMS,
                       data.raw(), aToRead, &cbRead, &guestRc);
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
            case VERR_GSTCTL_GUEST_ERROR:
                hr = GuestFile::setErrorExternal(this, guestRc);
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Reading from file \"%s\" failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), vrc);
                break;
        }
    }

    LogFlowThisFunc(("rc=%Rrc, cbRead=%RU64\n", vrc, cbRead));

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

    size_t cbRead; int guestRc;
    int vrc = readDataAt(aOffset, aToRead, aTimeoutMS,
                         data.raw(), aToRead, &cbRead, &guestRc);
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
            case VERR_GSTCTL_GUEST_ERROR:
                hr = GuestFile::setErrorExternal(this, guestRc);
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Reading from file \"%s\" (at offset %RU64) failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), aOffset, vrc);
                break;
        }
    }

    LogFlowThisFunc(("rc=%Rrc, cbRead=%RU64\n", vrc, cbRead));

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

    int guestRc;
    int vrc = seekAt(aOffset, eSeekType,
                     30 * 1000 /* 30s timeout */, &guestRc);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                hr = GuestFile::setErrorExternal(this, guestRc);
                break;

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

    CheckComArgOutPointerValid(aWritten);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    com::SafeArray<BYTE> data(ComSafeArrayInArg(aData)); int guestRc;
    int vrc = writeData(aTimeoutMS, data.raw(), data.size(), (uint32_t*)aWritten, &guestRc);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                hr = GuestFile::setErrorExternal(this, guestRc);
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Writing to file \"%s\" failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), vrc);
                break;
        }
    }

    LogFlowThisFunc(("rc=%Rrc, aWritten=%RU32\n", vrc, aWritten));

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

    CheckComArgOutPointerValid(aWritten);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    com::SafeArray<BYTE> data(ComSafeArrayInArg(aData)); int guestRc;
    int vrc = writeData(aTimeoutMS, data.raw(), data.size(), (uint32_t*)aWritten, &guestRc);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GSTCTL_GUEST_ERROR:
                hr = GuestFile::setErrorExternal(this, guestRc);
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR,
                              tr("Writing to file \"%s\" (at offset %RU64) failed: %Rrc"),
                              mData.mOpenInfo.mFileName.c_str(), aOffset, vrc);
                break;
        }
    }

    LogFlowThisFunc(("rc=%Rrc, aWritten=%RU32\n", vrc, aWritten));

    LogFlowFuncLeaveRC(vrc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

