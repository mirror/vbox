/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag and drop source.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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
#include "GuestImpl.h"
#include "GuestDnDSourceImpl.h"
#include "GuestDnDPrivate.h"
#include "ConsoleImpl.h"

#include "Global.h"
#include "AutoCaller.h"

#include <iprt/asm.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/uri.h>

#include <iprt/cpp/utils.h> /* For unconst(). */

#include <VBox/com/array.h>
#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/DragAndDropSvc.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

/**
 * Base class for a source task.
 */
class GuestDnDSourceTask
{
public:

    GuestDnDSourceTask(GuestDnDSource *pSource)
        : mSource(pSource),
          mRC(VINF_SUCCESS) { }

    virtual ~GuestDnDSourceTask(void) { }

    int getRC(void) const { return mRC; }
    bool isOk(void) const { return RT_SUCCESS(mRC); }
    const ComObjPtr<GuestDnDSource> &getSource(void) const { return mSource; }

protected:

    const ComObjPtr<GuestDnDSource>     mSource;
    int                                 mRC;
};

/**
 * Task structure for receiving data from a source using
 * a worker thread.
 */
class RecvDataTask : public GuestDnDSourceTask
{
public:

    RecvDataTask(GuestDnDSource *pSource, PRECVDATACTX pCtx)
        : GuestDnDSourceTask(pSource)
        , mpCtx(pCtx) { }

    virtual ~RecvDataTask(void) { }

    PRECVDATACTX getCtx(void) { return mpCtx; }

protected:

    /** Pointer to receive data context. */
    PRECVDATACTX mpCtx;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDnDSource)

HRESULT GuestDnDSource::FinalConstruct(void)
{
    /*
     * Set the maximum block size this source can handle to 64K. This always has
     * been hardcoded until now.
     *
     * Note: Never ever rely on information from the guest; the host dictates what and
     *       how to do something, so try to negogiate a sensible value here later.
     */
    mData.mcbBlockSize = _64K; /** @todo Make this configurable. */

    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestDnDSource::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestDnDSource::init(const ComObjPtr<Guest>& pGuest)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(m_pGuest) = pGuest;

    /* Confirm a successful initialization when it's the case. */
    autoInitSpan.setSucceeded();

    return VINF_SUCCESS;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestDnDSource::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

// implementation of wrapped IDnDBase methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDSource::isFormatSupported(const com::Utf8Str &aFormat, BOOL *aSupported)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_isFormatSupported(aFormat, aSupported);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::getFormats(GuestDnDMIMEList &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_getFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::addFormats(const GuestDnDMIMEList &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_addFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::removeFormats(const GuestDnDMIMEList &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_removeFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::getProtocolVersion(ULONG *aProtocolVersion)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_getProtocolVersion(aProtocolVersion);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

// implementation of wrapped IDnDSource methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDSource::dragIsPending(ULONG uScreenId, GuestDnDMIMEList &aFormats,
                                      std::vector<DnDAction_T> &aAllowedActions, DnDAction_T *aDefaultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* aDefaultAction is optional. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Determine guest DnD protocol to use. */
    GuestDnDBase::getProtocolVersion(&mDataBase.mProtocolVersion);

    /* Default is ignoring the action. */
    DnDAction_T defaultAction = DnDAction_Ignore;

    HRESULT hr = S_OK;

    GuestDnDMsg Msg;
    Msg.setType(DragAndDropSvc::HOST_DND_GH_REQ_PENDING);
    Msg.setNextUInt32(uScreenId);

    int rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
    if (RT_SUCCESS(rc))
    {
        GuestDnDResponse *pResp = GuestDnDInst()->response();
        AssertPtr(pResp);

        bool fFetchResult = true;

        rc = pResp->waitForGuestResponse(5000 /* Timeout in ms */);
        if (RT_FAILURE(rc))
            fFetchResult = false;

        if (   fFetchResult
            && isDnDIgnoreAction(pResp->defAction()))
            fFetchResult = false;

        /* Fetch the default action to use. */
        if (fFetchResult)
        {
            defaultAction   = GuestDnD::toMainAction(pResp->defAction());
            aAllowedActions = GuestDnD::toMainActions(pResp->allActions());

            /*
             * In the GuestDnDSource case the source formats are from the guest,
             * as GuestDnDSource acts as a target for the guest. The host always
             * dictates what's supported and what's not, so filter out all formats
             * which are not supported by the host.
             */
            aFormats        = GuestDnD::toFilteredFormatList(m_lstFmtSupported, pResp->formats());

            /* Save the (filtered) formats. */
            m_lstFmtOffered = aFormats;

            if (m_lstFmtOffered.size())
            {
                LogRelMax(3, ("DnD: Offered formats:\n"));
                for (size_t i = 0; i < m_lstFmtOffered.size(); i++)
                    LogRelMax(3, ("DnD:\tFormat #%zu: %s\n", i, m_lstFmtOffered.at(i).c_str()));
            }
            else
                LogRelMax(3, ("DnD: No compatible format between guest and host found, drag and drop to host not possible\n"));
        }

        LogFlowFunc(("fFetchResult=%RTbool, defaultAction=0x%x, allActions=0x%x\n",
                     fFetchResult, defaultAction, pResp->allActions()));

        if (aDefaultAction)
            *aDefaultAction = defaultAction;
    }

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::drop(const com::Utf8Str &aFormat, DnDAction_T aAction, ComPtr<IProgress> &aProgress)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Input validation. */
    if (RT_UNLIKELY((aFormat.c_str()) == NULL || *(aFormat.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No drop format specified"));

    /* Is the specified format in our list of (left over) offered formats? */
    if (!GuestDnD::isFormatInFormatList(aFormat, m_lstFmtOffered))
        return setError(E_INVALIDARG, tr("Specified format '%s' is not supported"), aFormat.c_str());

    uint32_t uAction = GuestDnD::toHGCMAction(aAction);
    if (isDnDIgnoreAction(uAction)) /* If there is no usable action, ignore this request. */
        return S_OK;

    /* Note: At the moment we only support one transfer at a time. */
    if (ASMAtomicReadBool(&mDataBase.mfTransferIsPending))
        return setError(E_INVALIDARG, tr("Another drop operation already is in progress"));

    /* Gets reset when the thread is finished. */
    ASMAtomicWriteBool(&mDataBase.mfTransferIsPending, true);

    /* Dito. */
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    AssertPtr(pResp);

    HRESULT hr = pResp->resetProgress(m_pGuest);
    if (FAILED(hr))
        return hr;

    try
    {
        mData.mRecvCtx.mIsActive   = false;
        mData.mRecvCtx.mpSource    = this;
        mData.mRecvCtx.mpResp      = pResp;
        mData.mRecvCtx.mFmtReq     = aFormat;
        mData.mRecvCtx.mFmtOffered = m_lstFmtOffered;

        LogRel2(("DnD: Requesting data from guest in format: %s\n", aFormat.c_str()));

        RecvDataTask *pTask = new RecvDataTask(this, &mData.mRecvCtx);
        AssertReturn(pTask->isOk(), pTask->getRC());

        LogFlowFunc(("Starting thread ...\n"));

        int rc = RTThreadCreate(NULL, GuestDnDSource::i_receiveDataThread,
                                (void *)pTask, 0, RTTHREADTYPE_MAIN_WORKER, 0, "dndSrcRcvData");
        if (RT_SUCCESS(rc))
        {
            hr = pResp->queryProgressTo(aProgress.asOutParam());
            ComAssertComRC(hr);

            /* Note: pTask is now owned by the worker thread. */
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR, tr("Starting thread failed (%Rrc)"), rc);
    }
    catch(std::bad_alloc &)
    {
        hr = setError(E_OUTOFMEMORY);
    }

    /* Note: mDataBase.mfTransferIsPending will be set to false again by i_receiveDataThread. */

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::receiveData(std::vector<BYTE> &aData)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Don't allow receiving the actual data until our transfer
     * actually is complete. */
    if (ASMAtomicReadBool(&mDataBase.mfTransferIsPending))
        return setError(E_INVALIDARG, tr("Current drop operation still in progress"));

    PRECVDATACTX pCtx = &mData.mRecvCtx;

    if (pCtx->mData.vecData.empty())
    {
        aData.resize(0);
        return S_OK;
    }

    HRESULT hr = S_OK;
    size_t cbData;

    try
    {
        bool fHasURIList = DnDMIMENeedsDropDir(pCtx->mFmtRecv.c_str(), pCtx->mFmtRecv.length());
        if (fHasURIList)
        {
            Utf8Str strURIs = pCtx->mURI.lstURI.RootToString(RTCString(DnDDirDroppedFilesGetDirAbs(&pCtx->mURI.mDropDir)));
            cbData = strURIs.length();

            LogFlowFunc(("Found %zu root URIs (%zu bytes)\n", pCtx->mURI.lstURI.RootCount(), cbData));

            aData.resize(cbData + 1 /* Include termination */);
            memcpy(&aData.front(), strURIs.c_str(), cbData);
        }
        else
        {
            cbData = pCtx->mData.vecData.size();

            /* Copy the data into a safe array of bytes. */
            aData.resize(cbData);
            memcpy(&aData.front(), &pCtx->mData.vecData[0], cbData);
        }
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    LogFlowFunc(("Returning cbData=%zu, hr=%Rhrc\n", cbData, hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

// implementation of internal methods.
/////////////////////////////////////////////////////////////////////////////

/* static */
Utf8Str GuestDnDSource::i_guestErrorToString(int guestRc)
{
    Utf8Str strError;

    switch (guestRc)
    {
        case VERR_ACCESS_DENIED:
            strError += Utf8StrFmt(tr("For one or more guest files or directories selected for transferring to the host your guest "
                                      "user does not have the appropriate access rights for. Please make sure that all selected "
                                      "elements can be accessed and that your guest user has the appropriate rights"));
            break;

        case VERR_NOT_FOUND:
            /* Should not happen due to file locking on the guest, but anyway ... */
            strError += Utf8StrFmt(tr("One or more guest files or directories selected for transferring to the host were not"
                                      "found on the guest anymore. This can be the case if the guest files were moved and/or"
                                      "altered while the drag and drop operation was in progress"));
            break;

        case VERR_SHARING_VIOLATION:
            strError += Utf8StrFmt(tr("One or more guest files or directories selected for transferring to the host were locked. "
                                      "Please make sure that all selected elements can be accessed and that your guest user has "
                                      "the appropriate rights"));
            break;

        case VERR_TIMEOUT:
            strError += Utf8StrFmt(tr("The guest was not able to retrieve the drag and drop data within time"));
            break;

        default:
            strError += Utf8StrFmt(tr("Drag and drop error from guest (%Rrc)"), guestRc);
            break;
    }

    return strError;
}

/* static */
Utf8Str GuestDnDSource::i_hostErrorToString(int hostRc)
{
    Utf8Str strError;

    switch (hostRc)
    {
        case VERR_ACCESS_DENIED:
            strError += Utf8StrFmt(tr("For one or more host files or directories selected for transferring to the guest your host "
                                      "user does not have the appropriate access rights for. Please make sure that all selected "
                                      "elements can be accessed and that your host user has the appropriate rights."));
            break;

        case VERR_NOT_FOUND:
            /* Should not happen due to file locking on the host, but anyway ... */
            strError += Utf8StrFmt(tr("One or more host files or directories selected for transferring to the host were not"
                                      "found on the host anymore. This can be the case if the host files were moved and/or"
                                      "altered while the drag and drop operation was in progress."));
            break;

        case VERR_SHARING_VIOLATION:
            strError += Utf8StrFmt(tr("One or more host files or directories selected for transferring to the guest were locked. "
                                      "Please make sure that all selected elements can be accessed and that your host user has "
                                      "the appropriate rights."));
            break;

        default:
            strError += Utf8StrFmt(tr("Drag and drop error from host (%Rrc)"), hostRc);
            break;
    }

    return strError;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
int GuestDnDSource::i_onReceiveData(PRECVDATACTX pCtx, const void *pvData, uint32_t cbData, uint64_t cbTotalSize)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,   VERR_INVALID_POINTER);
    AssertReturn(cbData,      VERR_INVALID_PARAMETER);
    AssertReturn(cbTotalSize, VERR_INVALID_PARAMETER);

    LogFlowFunc(("cbData=%RU32, cbTotalSize=%RU64\n", cbData, cbTotalSize));

    int rc = VINF_SUCCESS;

    try
    {
        if (   cbData > cbTotalSize
            || cbData > mData.mcbBlockSize)
        {
            LogFlowFunc(("Data sizes invalid: cbData=%RU32, cbTotalSize=%RU64\n", cbData, cbTotalSize));
            rc = VERR_INVALID_PARAMETER;
        }
        else if (cbData < pCtx->mData.vecData.size())
        {
            AssertMsgFailed(("New size (%RU64) is smaller than current size (%zu)\n", cbTotalSize, pCtx->mData.vecData.size()));
            rc = VERR_INVALID_PARAMETER;
        }

        if (RT_SUCCESS(rc))
        {
            pCtx->mData.vecData.insert(pCtx->mData.vecData.begin(), (BYTE *)pvData, (BYTE *)pvData + cbData);

            LogFlowFunc(("vecDataSize=%zu, cbData=%RU32, cbTotalSize=%RU64\n", pCtx->mData.vecData.size(), cbData, cbTotalSize));

            /* Data transfer complete? */
            Assert(cbData <= pCtx->mData.vecData.size());
            if (cbData == pCtx->mData.vecData.size())
            {
                bool fHasURIList = DnDMIMENeedsDropDir(pCtx->mFmtRecv.c_str(), pCtx->mFmtRecv.length());
                LogFlowFunc(("fHasURIList=%RTbool, cbTotalSize=%RU32\n", fHasURIList, cbTotalSize));
                if (fHasURIList)
                {
                    /* Try parsing the data as URI list. */
                    rc = pCtx->mURI.lstURI.RootFromURIData(&pCtx->mData.vecData[0], pCtx->mData.vecData.size(), 0 /* uFlags */);
                    if (RT_SUCCESS(rc))
                    {
                        /* Reset processed bytes. */
                        pCtx->mData.cbProcessed = 0;

                        /*
                         * Assign new total size which also includes all file data to receive
                         * from the guest.
                         */
                        pCtx->mData.cbToProcess = cbTotalSize;

                        /* Update our process with the data we already received.
                         * Note: The total size will consist of the meta data (in vecData) and
                         *       the actual accumulated file/directory data from the guest. */
                        rc = i_updateProcess(pCtx, (uint64_t)pCtx->mData.vecData.size());

                        LogFlowFunc(("URI data => cbProcessed=%RU64, cbToProcess=%RU64, rc=%Rrc\n",
                                     pCtx->mData.cbProcessed, pCtx->mData.cbToProcess, rc));
                    }
                }
            }
        }
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_onReceiveDir(PRECVDATACTX pCtx, const char *pszPath, uint32_t cbPath, uint32_t fMode)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath,     VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x\n", pszPath, cbPath, fMode));

    int rc;
    char *pszDir = RTPathJoinA(DnDDirDroppedFilesGetDirAbs(&pCtx->mURI.mDropDir), pszPath);
    if (pszDir)
    {
        rc = RTDirCreateFullPath(pszDir, fMode);
        if (RT_FAILURE(rc))
            LogRel2(("DnD: Error creating guest directory '%s' on the host, rc=%Rrc\n", pszDir, rc));

        RTStrFree(pszDir);
    }
    else
         rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_onReceiveFileHdr(PRECVDATACTX pCtx, const char *pszPath, uint32_t cbPath,
                                       uint64_t cbSize, uint32_t fMode, uint32_t fFlags)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath,     VERR_INVALID_PARAMETER);
    AssertReturn(fMode,      VERR_INVALID_PARAMETER);
    /* fFlags are optional. */

    LogFlowFunc(("pszPath=%s, cbPath=%RU32, cbSize=%RU64, fMode=0x%x, fFlags=0x%x\n", pszPath, cbPath, cbSize, fMode, fFlags));

    int rc = VINF_SUCCESS;

    do
    {
        if (    pCtx->mURI.objURI.IsOpen()
            && !pCtx->mURI.objURI.IsComplete())
        {
            AssertMsgFailed(("Object '%s' not complete yet\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_WRONG_ORDER;
            break;
        }

        if (pCtx->mURI.objURI.IsOpen()) /* File already opened? */
        {
            AssertMsgFailed(("Current opened object is '%s', close this first\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_WRONG_ORDER;
            break;
        }

        if (cbSize > pCtx->mData.cbToProcess)
        {
            AssertMsgFailed(("File size (%RU64) exceeds total size to transfer (%RU64)\n", cbSize, pCtx->mData.cbToProcess));
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        char pszPathAbs[RTPATH_MAX];
        rc = RTPathJoin(pszPathAbs, sizeof(pszPathAbs), DnDDirDroppedFilesGetDirAbs(&pCtx->mURI.mDropDir), pszPath);
        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Warning: Rebasing current file failed with rc=%Rrc\n", rc));
            break;
        }

        rc = DnDPathSanitize(pszPathAbs, sizeof(pszPathAbs));
        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Warning: Rebasing current file failed with rc=%Rrc\n", rc));
            break;
        }

        LogFunc(("Rebased to: %s\n", pszPathAbs));

        /** @todo Add sparse file support based on fFlags? (Use Open(..., fFlags | SPARSE). */
        rc = pCtx->mURI.objURI.OpenEx(pszPathAbs, DnDURIObject::File, DnDURIObject::Target,
                                      RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE,
                                      (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRUSR | RTFS_UNIX_IWUSR);
        if (RT_SUCCESS(rc))
        {
            /* Note: Protocol v1 does not send any file sizes, so always 0. */
            if (mDataBase.mProtocolVersion >= 2)
                rc = pCtx->mURI.objURI.SetSize(cbSize);

            /** @todo Unescpae path before printing. */
            LogRel2(("DnD: Transferring guest file to host: %s (%RU64 bytes, mode 0x%x)\n",
                     pCtx->mURI.objURI.GetDestPath().c_str(), pCtx->mURI.objURI.GetSize(), pCtx->mURI.objURI.GetMode()));

            /** @todo Set progress object title to current file being transferred? */

            if (!cbSize) /* 0-byte file? Close again. */
                pCtx->mURI.objURI.Close();
        }
        else
        {
            LogRel2(("DnD: Error opening/creating guest file '%s' on host, rc=%Rrc\n",
                     pCtx->mURI.objURI.GetDestPath().c_str(), rc));
            break;
        }

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_onReceiveFileData(PRECVDATACTX pCtx, const void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    do
    {
        if (pCtx->mURI.objURI.IsComplete())
        {
            LogFlowFunc(("Warning: Object '%s' already completed\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_WRONG_ORDER;
            break;
        }

        if (!pCtx->mURI.objURI.IsOpen()) /* File opened on host? */
        {
            LogFlowFunc(("Warning: Object '%s' not opened\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_WRONG_ORDER;
            break;
        }

        uint32_t cbWritten;
        rc = pCtx->mURI.objURI.Write(pvData, cbData, &cbWritten);
        if (RT_SUCCESS(rc))
        {
            Assert(cbWritten <= cbData);
            if (cbWritten < cbData)
            {
                /** @todo What to do when the host's disk is full? */
                rc = VERR_DISK_FULL;
            }

            if (RT_SUCCESS(rc))
                rc = i_updateProcess(pCtx, cbWritten);
        }

        if (RT_SUCCESS(rc))
        {
            if (pCtx->mURI.objURI.IsComplete())
            {
                /** @todo Sanitize path. */
                LogRel2(("DnD: File transfer to host complete: %s\n", pCtx->mURI.objURI.GetDestPath().c_str()));
                rc = VINF_EOF;

                /* Prepare URI object for next use. */
                pCtx->mURI.objURI.Reset();
            }
        }
        else
        {
            /** @todo What to do when the host's disk is full? */
            LogRel(("DnD: Error writing guest file to host to '%s': %Rrc\n", pCtx->mURI.objURI.GetDestPath().c_str(), rc));
        }

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

int GuestDnDSource::i_receiveData(PRECVDATACTX pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pCtx->mpResp);

    /* Is this context already in receiving state? */
    if (ASMAtomicReadBool(&pCtx->mIsActive))
        return VERR_WRONG_ORDER;

    ASMAtomicWriteBool(&pCtx->mIsActive, true);

    int rc = pCtx->mCallback.Reset();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Reset any old data.
     */
    pCtx->mData.Reset();
    pCtx->mURI.Reset();
    pResp->reset();

    /*
     * Do we need to receive a different format than initially requested?
     *
     * For example, receiving a file link as "text/plain"  requires still to receive
     * the file from the guest as "text/uri-list" first, then pointing to
     * the file path on the host in the "text/plain" data returned.
     */

    /* Plain text needed? */
    if (pCtx->mFmtReq.equalsIgnoreCase("text/plain"))
    {
        /* Did the guest offer a file? Receive a file instead. */
        if (GuestDnD::isFormatInFormatList("text/uri-list", pCtx->mFmtOffered))
            pCtx->mFmtRecv = "text/uri-list";

        /** @todo Add more conversions here. */
    }

    if (pCtx->mFmtRecv.isEmpty())
        pCtx->mFmtRecv = pCtx->mFmtReq;

    if (!pCtx->mFmtRecv.equals(pCtx->mFmtReq))
        LogRel3(("DnD: Requested data in format '%s', receiving in intermediate format '%s' now\n",
                 pCtx->mFmtReq.c_str(), pCtx->mFmtRecv.c_str()));

    /*
     * Call the appropriate receive handler based on the data format to handle.
     */
    bool fHasURIList = DnDMIMENeedsDropDir(pCtx->mFmtRecv.c_str(), pCtx->mFmtRecv.length());
    LogFlowFunc(("strFormatReq=%s, strFormatRecv=%s, uAction=0x%x, fHasURIList=%RTbool\n",
                 pCtx->mFmtReq.c_str(), pCtx->mFmtRecv.c_str(), pCtx->mAction, fHasURIList));

    if (fHasURIList)
    {
        rc = i_receiveURIData(pCtx, msTimeout);
    }
    else
    {
        rc = i_receiveRawData(pCtx, msTimeout);
    }

    ASMAtomicWriteBool(&pCtx->mIsActive, false);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
DECLCALLBACK(int) GuestDnDSource::i_receiveDataThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFunc(("pvUser=%p\n", pvUser));

    RecvDataTask *pTask = (RecvDataTask *)pvUser;
    AssertPtrReturn(pTask, VERR_INVALID_POINTER);

    const ComObjPtr<GuestDnDSource> pSource(pTask->getSource());
    Assert(!pSource.isNull());

    int rc;

    AutoCaller autoCaller(pSource);
    if (SUCCEEDED(autoCaller.rc()))
    {
        rc = pSource->i_receiveData(pTask->getCtx(), RT_INDEFINITE_WAIT /* msTimeout */);
    }
    else
        rc = VERR_COM_INVALID_OBJECT_STATE;

    ASMAtomicWriteBool(&pSource->mDataBase.mfTransferIsPending, false);

    if (pTask)
        delete pTask;

    LogFlowFunc(("pSource=%p returning rc=%Rrc\n", (GuestDnDSource *)pSource, rc));
    return rc;
}

int GuestDnDSource::i_receiveRawData(PRECVDATACTX pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc;

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pCtx->mpResp);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

#define REGISTER_CALLBACK(x) \
    rc = pResp->setCallback(x, i_receiveRawDataCallback, pCtx); \
    if (RT_FAILURE(rc)) \
        return rc;

#define UNREGISTER_CALLBACK(x) \
    rc = pCtx->mpResp->setCallback(x, NULL); \
    AssertRC(rc);

    /*
     * Register callbacks.
     */
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);

    do
    {
        /*
         * Receive the raw data.
         */
        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_GH_EVT_DROPPED);
        Msg.setNextPointer((void*)pCtx->mFmtRecv.c_str(), (uint32_t)pCtx->mFmtRecv.length() + 1);
        Msg.setNextUInt32((uint32_t)pCtx->mFmtRecv.length() + 1);
        Msg.setNextUInt32(pCtx->mAction);

        /* Make the initial call to the guest by telling that we initiated the "dropped" event on
         * the host and therefore now waiting for the actual raw data. */
        rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            rc = waitForEvent(msTimeout, pCtx->mCallback, pCtx->mpResp);
            if (RT_SUCCESS(rc))
                rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_COMPLETE, VINF_SUCCESS);
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_CANCELLED)
        {
            int rc2 = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_CANCELLED, VINF_SUCCESS);
            AssertRC(rc2);

            rc2 = sendCancel();
            AssertRC(rc2);
        }
        else if (rc != VERR_GSTDND_GUEST_ERROR) /* Guest-side error are already handled in the callback. */
        {
            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR,
                                           rc, GuestDnDSource::i_hostErrorToString(rc));
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_receiveURIData(PRECVDATACTX pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc;

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pCtx->mpResp);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

#define REGISTER_CALLBACK(x)                                    \
    rc = pResp->setCallback(x, i_receiveURIDataCallback, pCtx); \
    if (RT_FAILURE(rc))                                         \
        return rc;

#define UNREGISTER_CALLBACK(x)                                  \
    {                                                           \
        int rc2 = pResp->setCallback(x, NULL);                  \
        AssertRC(rc2);                                          \
    }

    /*
     * Register callbacks.
     */
    /* Guest callbacks. */
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DIR);
    if (mDataBase.mProtocolVersion >= 2)
        REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_HDR);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_DATA);

    do
    {
        rc = DnDDirDroppedFilesCreateAndOpenTemp(&pCtx->mURI.mDropDir);
        if (RT_FAILURE(rc))
            break;
        LogFlowFunc(("rc=%Rrc, strDropDir=%s\n", rc, DnDDirDroppedFilesGetDirAbs(&pCtx->mURI.mDropDir)));
        if (RT_FAILURE(rc))
            break;

        /*
         * Receive the URI list.
         */
        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_GH_EVT_DROPPED);
        Msg.setNextPointer((void*)pCtx->mFmtRecv.c_str(), (uint32_t)pCtx->mFmtRecv.length() + 1);
        Msg.setNextUInt32((uint32_t)pCtx->mFmtRecv.length() + 1);
        Msg.setNextUInt32(pCtx->mAction);

        /* Make the initial call to the guest by telling that we initiated the "dropped" event on
         * the host and therefore now waiting for the actual URI data. */
        rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("Waiting ...\n"));

            rc = waitForEvent(msTimeout, pCtx->mCallback, pCtx->mpResp);
            if (RT_SUCCESS(rc))
                rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_COMPLETE, VINF_SUCCESS);

            LogFlowFunc(("Waiting ended with rc=%Rrc\n", rc));
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DIR);
    if (mDataBase.mProtocolVersion >= 2)
        UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_HDR);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    int rc2;

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_CANCELLED)
        {
            rc2 = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_CANCELLED, VINF_SUCCESS);
            AssertRC(rc2);

            rc2 = sendCancel();
            AssertRC(rc2);
        }
        else if (rc != VERR_GSTDND_GUEST_ERROR) /* Guest-side error are already handled in the callback. */
        {
            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR,
                                           rc, GuestDnDSource::i_hostErrorToString(rc));
        }
    }

    if (RT_FAILURE(rc))
    {
        rc2 = DnDDirDroppedFilesRollback(&pCtx->mURI.mDropDir); /** @todo Inform user on rollback failure? */
        LogFlowFunc(("Rolling back ended with rc=%Rrc\n", rc2));
    }

    rc2 = DnDDirDroppedFilesClose(&pCtx->mURI.mDropDir, RT_FAILURE(rc) ? true : false /* fRemove */);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
DECLCALLBACK(int) GuestDnDSource::i_receiveRawDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    PRECVDATACTX pCtx = (PRECVDATACTX)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDSource *pThis = pCtx->mpSource;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("pThis=%p, uMsg=%RU32\n", pThis, uMsg));

    int rc = VINF_SUCCESS;

    int rcCallback = VINF_SUCCESS; /* rc for the callback. */
    bool fNotify = false;

    switch (uMsg)
    {
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case DragAndDropSvc::GUEST_DND_GH_SND_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBSNDDATADATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveData(pCtx, pCBData->pvData, pCBData->cbData, pCBData->cbTotalSize);
            break;
        }
        case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
        {
            DragAndDropSvc::PVBOXDNDCBEVTERRORDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBEVTERRORDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBEVTERRORDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_EVT_ERROR == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            pCtx->mpResp->reset();

            if (RT_SUCCESS(pCBData->rc))
                pCBData->rc = VERR_GENERAL_FAILURE; /* Make sure some error is set. */

            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, pCBData->rc,
                                           GuestDnDSource::i_guestErrorToString(pCBData->rc));
            if (RT_SUCCESS(rc))
                rcCallback = VERR_GSTDND_GUEST_ERROR;
            break;
        }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_FAILURE(rc))
    {
        int rc2 = pCtx->mCallback.Notify(rc);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc; /* Tell the guest. */
}

/* static */
DECLCALLBACK(int) GuestDnDSource::i_receiveURIDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    PRECVDATACTX pCtx = (PRECVDATACTX)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDSource *pThis = pCtx->mpSource;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("pThis=%p, uMsg=%RU32\n", pThis, uMsg));

    int rc = VINF_SUCCESS;

    int rcCallback = VINF_SUCCESS; /* rc for the callback. */
    bool fNotify = false;

    switch (uMsg)
    {
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case DragAndDropSvc::GUEST_DND_GH_SND_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBSNDDATADATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveData(pCtx, pCBData->pvData, pCBData->cbData, pCBData->cbTotalSize);
            break;
        }
        case DragAndDropSvc::GUEST_DND_GH_SND_DIR:
        {
            DragAndDropSvc::PVBOXDNDCBSNDDIRDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDDIRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDDIRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_DIR == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveDir(pCtx, pCBData->pszPath, pCBData->cbPath, pCBData->fMode);
            break;
        }
        case DragAndDropSvc::GUEST_DND_GH_SND_FILE_HDR:
        {
            DragAndDropSvc::PVBOXDNDCBSNDFILEHDRDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDFILEHDRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDFILEHDRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_FILE_HDR == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveFileHdr(pCtx, pCBData->pszFilePath, pCBData->cbFilePath,
                                           pCBData->cbSize, pCBData->fMode, pCBData->fFlags);
            break;
        }
        case DragAndDropSvc::GUEST_DND_GH_SND_FILE_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBSNDFILEDATADATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDFILEDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDFILEDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_FILE_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            if (pThis->mDataBase.mProtocolVersion <= 1)
            {
                /**
                 * Notes for protocol v1 (< VBox 5.0):
                 * - Every time this command is being sent it includes the file header,
                 *   so just process both calls here.
                 * - There was no information whatsoever about the total file size; the old code only
                 *   appended data to the desired file. So just pass 0 as cbSize.
                 */
                rc = pThis->i_onReceiveFileHdr(pCtx,
                                               pCBData->u.v1.pszFilePath, pCBData->u.v1.cbFilePath,
                                               0 /* cbSize */, pCBData->u.v1.fMode, 0 /* fFlags */);
                if (RT_SUCCESS(rc))
                    rc = pThis->i_onReceiveFileData(pCtx, pCBData->pvData, pCBData->cbData);
            }
            else /* Protocol v2 and up. */
                rc = pThis->i_onReceiveFileData(pCtx, pCBData->pvData, pCBData->cbData);
            break;
        }
        case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
        {
            DragAndDropSvc::PVBOXDNDCBEVTERRORDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBEVTERRORDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBEVTERRORDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_EVT_ERROR == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            pCtx->mpResp->reset();

            if (RT_SUCCESS(pCBData->rc))
                pCBData->rc = VERR_GENERAL_FAILURE; /* Make sure some error is set. */

            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, pCBData->rc,
                                           GuestDnDSource::i_guestErrorToString(pCBData->rc));
            if (RT_SUCCESS(rc))
                rcCallback = VERR_GSTDND_GUEST_ERROR;
            break;
        }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (   RT_FAILURE(rc)
        || RT_FAILURE(rcCallback))
    {
        fNotify = true;
        if (RT_SUCCESS(rcCallback))
            rcCallback = rc;
    }

    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_NO_DATA:
                LogRel2(("DnD: Transfer to host complete\n"));
                break;

            case VERR_CANCELLED:
                LogRel2(("DnD: Transfer to host canceled\n"));
                break;

            default:
                LogRel(("DnD: Error %Rrc occurred, aborting transfer to host\n", rc));
                break;
        }

        /* Unregister this callback. */
        AssertPtr(pCtx->mpResp);
        int rc2 = pCtx->mpResp->setCallback(uMsg, NULL /* PFNGUESTDNDCALLBACK */);
        AssertRC(rc2);
    }

    /* All URI data processed? */
    if (pCtx->mData.cbProcessed >= pCtx->mData.cbToProcess)
    {
        Assert(pCtx->mData.cbProcessed == pCtx->mData.cbToProcess);
        fNotify = true;
    }

    LogFlowFunc(("cbProcessed=%RU64, cbToProcess=%RU64, fNotify=%RTbool, rcCallback=%Rrc, rc=%Rrc\n",
                 pCtx->mData.cbProcessed, pCtx->mData.cbToProcess, fNotify, rcCallback, rc));

    if (fNotify)
    {
        int rc2 = pCtx->mCallback.Notify(rcCallback);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc; /* Tell the guest. */
}

int GuestDnDSource::i_updateProcess(PRECVDATACTX pCtx, uint64_t cbDataAdd)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("cbProcessed=%RU64 (+ %RU64 = %RU64), cbToProcess=%RU64\n",
                 pCtx->mData.cbProcessed, cbDataAdd, pCtx->mData.cbProcessed + cbDataAdd, pCtx->mData.cbToProcess));

    pCtx->mData.cbProcessed += cbDataAdd;
    Assert(pCtx->mData.cbProcessed <= pCtx->mData.cbToProcess);

    int64_t cbTotal  = pCtx->mData.cbToProcess;
    uint8_t uPercent = pCtx->mData.cbProcessed * 100 / (cbTotal ? cbTotal : 1);

    int rc = pCtx->mpResp->setProgress(uPercent,
                                         uPercent >= 100
                                       ? DragAndDropSvc::DND_PROGRESS_COMPLETE
                                       : DragAndDropSvc::DND_PROGRESS_RUNNING);
    return rc;
}
