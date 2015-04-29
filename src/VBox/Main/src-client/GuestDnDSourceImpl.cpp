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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "GuestImpl.h"
#include "GuestDnDSourceImpl.h"
#include "GuestDnDPrivate.h"

#include "Global.h"
#include "AutoCaller.h"

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

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

    virtual ~RecvDataTask(void)
    {
        if (mpCtx)
        {
            delete mpCtx;
            mpCtx = NULL;
        }
    }

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
    /* Set the maximum block size this source can handle to 64K. This always has
     * been hardcoded until now. */
    /* Note: Never ever rely on information from the guest; the host dictates what and
     *       how to do something, so try to negogiate a sensible value here later. */
    m_cbBlockSize = _64K; /** @todo Make this configurable. */

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

HRESULT GuestDnDSource::getFormats(std::vector<com::Utf8Str> &aFormats)
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

HRESULT GuestDnDSource::addFormats(const std::vector<com::Utf8Str> &aFormats)
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

HRESULT GuestDnDSource::removeFormats(const std::vector<com::Utf8Str> &aFormats)
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

// implementation of wrapped IDnDTarget methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDSource::dragIsPending(ULONG uScreenId,
                                      std::vector<com::Utf8Str> &aFormats,
                                      std::vector<DnDAction_T> &aAllowedActions,
                                      DnDAction_T *aDefaultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Determine guest DnD protocol to use. */
    GuestDnDBase::getProtocolVersion(&mData.mProtocolVersion);

    /* Default is ignoring the action. */
    DnDAction_T defaultAction = DnDAction_Ignore;

    HRESULT hr = S_OK;

    VBOXHGCMSVCPARM paParms[1];
    int i = 0;
    paParms[i++].setUInt32(uScreenId);

    int rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_GH_REQ_PENDING, i, paParms);
    if (RT_SUCCESS(rc))
    {
        bool fFetchResult = true;
        GuestDnDResponse *pResp = GuestDnDInst()->response();
        if (pResp)
        {
            if (pResp->waitForGuestResponse() == VERR_TIMEOUT)
                fFetchResult = false;

            if (isDnDIgnoreAction(pResp->defAction()))
                fFetchResult = false;

            /* Fetch the default action to use. */
            if (fFetchResult)
            {
                defaultAction = GuestDnD::toMainAction(pResp->defAction());

                GuestDnD::toFormatVector(m_strFormats, pResp->format(), aFormats);
                GuestDnD::toMainActions(pResp->allActions(), aAllowedActions);
            }
        }

        if (aDefaultAction)
            *aDefaultAction = defaultAction;
    }

    if (RT_FAILURE(rc))
        hr = setError(VBOX_E_IPRT_ERROR,
                      tr("Error retrieving drag'n drop pending status (%Rrc)\n"), rc);

    LogFlowFunc(("hr=%Rhrc, defaultAction=0x%x\n", hr, defaultAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::drop(const com::Utf8Str &aFormat,
                             DnDAction_T aAction, ComPtr<IProgress> &aProgress)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */
    if (RT_UNLIKELY((aFormat.c_str()) == NULL || *(aFormat.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No drop format specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t uAction = GuestDnD::toHGCMAction(aAction);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uAction))
        return S_OK;

    HRESULT hr = S_OK;

    /* Note: At the moment we only support one response at a time. */
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    if (pResp)
    {
        pResp->resetProgress(m_pGuest);

        int rc;

        try
        {
            PRECVDATACTX pRecvCtx = new RECVDATACTX;
            RT_BZERO(pRecvCtx, sizeof(RECVDATACTX));

            pRecvCtx->mpSource = this;
            pRecvCtx->mpResp   = pResp;
            pRecvCtx->mFormat  = aFormat;

            RecvDataTask *pTask = new RecvDataTask(this, pRecvCtx);
            AssertReturn(pTask->isOk(), pTask->getRC());

            rc = RTThreadCreate(NULL, GuestDnDSource::i_receiveDataThread,
                                (void *)pTask, 0, RTTHREADTYPE_MAIN_WORKER, 0, "dndSrcRcvData");
            if (RT_SUCCESS(rc))
            {
                hr = pResp->queryProgressTo(aProgress.asOutParam());
                ComAssertComRC(hr);

                /* Note: pTask is now owned by the worker thread. */
            }
            else if (pRecvCtx)
                delete pRecvCtx;
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }

        /*if (RT_FAILURE(vrc)) @todo SetError(...) */
    }
    /** @todo SetError(...) */

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

    HRESULT hr = S_OK;

#if 0
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    if (pResp)
    {
        size_t cbData = pResp->size();
        if (cbData)
        {
            const void *pvData = pResp->data();
            AssertPtr(pvData);

            Utf8Str strFormat = pResp->format();
            LogFlowFunc(("strFormat=%s, cbData=%zu, pvData=0x%p\n", strFormat.c_str(), cbData, pvData));

            try
            {
                if (DnDMIMEHasFileURLs(strFormat.c_str(), strFormat.length()))
                {
                    LogFlowFunc(("strDropDir=%s\n", pResp->dropDir().c_str()));

                    DnDURIList lstURI;
                    int rc2 = lstURI.RootFromURIData(pvData, cbData, 0 /* fFlags */);
                    if (RT_SUCCESS(rc2))
                    {
                        Utf8Str strURIs = lstURI.RootToString(pResp->dropDir());
                        size_t cbURIs = strURIs.length();

                        LogFlowFunc(("Found %zu root URIs (%zu bytes)\n", lstURI.RootCount(), cbURIs));

                        aData.resize(cbURIs + 1 /* Include termination */);
                        memcpy(&aData.front(), strURIs.c_str(), cbURIs);
                    }
                    else
                        hr = VBOX_E_IPRT_ERROR;
                }
                else
                {
                    /* Copy the data into a safe array of bytes. */
                    aData.resize(cbData);
                    memcpy(&aData.front(), pvData, cbData);
                }
            }
            catch (std::bad_alloc &)
            {
                hr = E_OUTOFMEMORY;
            }
        }

        /* Delete the data. */
        pResp->reset();
    }
    else
        hr = VBOX_E_INVALID_OBJECT_STATE;
#endif

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

// implementation of internal methods.
/////////////////////////////////////////////////////////////////////////////

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
            || cbData > m_cbBlockSize)
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
                bool fHasURIList = DnDMIMENeedsDropDir(pCtx->mFormat.c_str(), pCtx->mFormat.length());
                LogFlowFunc(("fHasURIList=%RTbool, cbTotalSize=%RU32\n", fHasURIList, cbTotalSize));
                if (fHasURIList)
                {
                    /* Try parsing the data as URI list. */
                    rc = pCtx->mURI.lstURI.RootFromURIData(&pCtx->mData.vecData[0], pCtx->mData.vecData.size(), 0 /* uFlags */);
                    if (RT_SUCCESS(rc))
                    {
                        pCtx->mData.cbProcessed = 0;

                        /* Assign new total size which also includes all paths + file
                         * data to receive from the guest. */
                        pCtx->mData.cbToProcess = cbTotalSize;
                    }
                }
            }

            if (RT_SUCCESS(rc))
                rc = i_updateProcess(pCtx, cbData);
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

    LogFlowFunc(("pszPath=%s, cbPath=%zu, fMode=0x%x\n", pszPath, cbPath, fMode));

    int rc;
    char *pszDir = RTPathJoinA(pCtx->mURI.strDropDir.c_str(), pszPath);
    if (pszDir)
    {
        rc = RTDirCreateFullPath(pszDir, fMode);
        if (RT_FAILURE(rc))
            LogRel2(("DnD: Error creating guest directory \"%s\" on the host, rc=%Rrc\n", pszDir, rc));

        RTStrFree(pszDir);
    }
    else
         rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
        rc = i_updateProcess(pCtx, cbPath);

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
        if (!pCtx->mURI.objURI.IsComplete())
        {
            LogFlowFunc(("Warning: Object \"%s\" not complete yet\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        if (pCtx->mURI.objURI.IsOpen()) /* File already opened? */
        {
            LogFlowFunc(("Warning: Current opened object is \"%s\"\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_WRONG_ORDER;
            break;
        }

        char pszPathAbs[RTPATH_MAX];
        rc = RTPathJoin(pszPathAbs, sizeof(pszPathAbs), pCtx->mURI.strDropDir.c_str(), pszPath);
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
        /** @todo Add fMode to opening flags. */
        rc = pCtx->mURI.objURI.OpenEx(pszPathAbs, DnDURIObject::File, DnDURIObject::Target,
                                      RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            /** @todo Unescpae path before printing. */
            LogRel2(("DnD: Transferring file to host: %s\n", pCtx->mURI.objURI.GetDestPath().c_str()));

            /* Note: Protocol v1 does not send any file sizes, so always 0. */
            if (mData.mProtocolVersion >= 2)
                rc = pCtx->mURI.objURI.SetSize(cbSize);
        }
        else
        {
            LogRel2(("DnD: Error opening/creating guest file \"%s\" on host, rc=%Rrc\n",
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
            LogFlowFunc(("Warning: Object \"%s\" already completed\n", pCtx->mURI.objURI.GetDestPath().c_str()));
            rc = VERR_WRONG_ORDER;
            break;
        }

        if (!pCtx->mURI.objURI.IsOpen()) /* File opened on host? */
        {
            LogFlowFunc(("Warning: Object \"%s\" not opened\n", pCtx->mURI.objURI.GetDestPath().c_str()));
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
                /* Prepare URI object for next use. */
                pCtx->mURI.objURI.Reset();

                /** @todo Sanitize path. */
                LogRel2(("DnD: File transfer to host complete: %s\n", pCtx->mURI.objURI.GetDestPath().c_str()));
                rc = VINF_EOF;
            }
        }
        else
            LogRel(("DnD: Error: Can't write guest file to host to \"%s\": %Rrc\n", pCtx->mURI.objURI.GetDestPath().c_str(), rc));

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

int GuestDnDSource::i_receiveData(PRECVDATACTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pCtx->mpResp);

    ASMAtomicWriteBool(&pCtx->mIsActive, true);

    int rc = pCtx->mCallback.Reset();
    if (RT_FAILURE(rc))
        return rc;

    pCtx->mData.vecData.clear();
    pCtx->mData.cbToProcess = 0;
    pCtx->mData.cbProcessed = 0;

    do
    {
        /* Reset any old data. */
        pResp->reset();
        pResp->resetProgress(m_pGuest);

        /* Set the format we are going to retrieve to have it around
         * when retrieving the data later. */
        pResp->setFormat(pCtx->mFormat);

        bool fHasURIList = DnDMIMENeedsDropDir(pCtx->mFormat.c_str(), pCtx->mFormat.length());
        LogFlowFunc(("strFormat=%s, uAction=0x%x, fHasURIList=%RTbool\n", pCtx->mFormat.c_str(), pCtx->mAction, fHasURIList));

        if (fHasURIList)
        {
            rc = i_receiveURIData(pCtx);
        }
        else
        {
            rc = i_receiveRawData(pCtx);
        }

    } while (0);

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
        rc = pSource->i_receiveData(pTask->getCtx());
    }
    else
        rc = VERR_COM_INVALID_OBJECT_STATE;

    LogFlowFunc(("pSource=%p returning rc=%Rrc\n", (GuestDnDSource *)pSource, rc));

    if (pTask)
        delete pTask;
    return rc;
}

int GuestDnDSource::i_receiveRawData(PRECVDATACTX pCtx)
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
        Msg.setNextPointer((void*)pCtx->mFormat.c_str(), (uint32_t)pCtx->mFormat.length() + 1);
        Msg.setNextUInt32((uint32_t)pCtx->mFormat.length() + 1);
        Msg.setNextUInt32(pCtx->mAction);

        /* Make the initial call to the guest by telling that we initiated the "dropped" event on
         * the host and therefore now waiting for the actual raw data. */
        rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            /*
             * Wait until our callback i_receiveRawDataCallback triggered the
             * wait event.
             */
            LogFlowFunc(("Waiting for raw data callback ...\n"));
            rc = pCtx->mCallback.Wait(RT_INDEFINITE_WAIT);
            LogFlowFunc(("Raw callback done, rc=%Rrc\n", rc));
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_receiveURIData(PRECVDATACTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc;

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pCtx->mpResp);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

#define REGISTER_CALLBACK(x) \
    rc = pResp->setCallback(x, i_receiveURIDataCallback, pCtx); \
    if (RT_FAILURE(rc)) \
        return rc;

#define UNREGISTER_CALLBACK(x) \
    rc = pResp->setCallback(x, NULL); \
    AssertRC(rc);

    /*
     * Register callbacks.
     */
    /* Guest callbacks. */
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DIR);
    if (mData.mProtocolVersion >= 2)
        REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_HDR);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_DATA);

    do
    {
        char szDropDir[RTPATH_MAX];
        rc = DnDDirCreateDroppedFiles(szDropDir, sizeof(szDropDir));
        LogFlowFunc(("rc=%Rrc, szDropDir=%s\n", rc, szDropDir));
        if (RT_FAILURE(rc))
            break;

        pCtx->mURI.strDropDir = szDropDir; /** @todo Keep directory handle open? */

        /*
         * Receive the URI list.
         */
        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_GH_EVT_DROPPED);
        Msg.setNextPointer((void*)pCtx->mFormat.c_str(), (uint32_t)pCtx->mFormat.length() + 1);
        Msg.setNextUInt32((uint32_t)pCtx->mFormat.length() + 1);
        Msg.setNextUInt32(pCtx->mAction);

        /* Make the initial call to the guest by telling that we initiated the "dropped" event on
         * the host and therefore now waiting for the actual URI actual data. */
        rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            /*
             * Wait until our callback i_receiveURIDataCallback triggered the
             * wait event.
             */
            LogFlowFunc(("Waiting for URI callback ...\n"));
            rc = pCtx->mCallback.Wait(RT_INDEFINITE_WAIT);
            LogFlowFunc(("URI callback done, rc=%Rrc\n", rc));
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DATA);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_DIR);
    if (mData.mProtocolVersion >= 2)
        UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_HDR);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_SND_FILE_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Rolling back ...\n"));

        /* Rollback by removing any stuff created. */
        for (size_t i = 0; i < pCtx->mURI.lstFiles.size(); ++i)
            RTFileDelete(pCtx->mURI.lstFiles.at(i).c_str());
        for (size_t i = 0; i < pCtx->mURI.lstDirs.size(); ++i)
            RTDirRemove(pCtx->mURI.lstDirs.at(i).c_str());
    }

    /* Try removing (hopefully) empty drop directory in any case. */
    if (pCtx->mURI.strDropDir.isNotEmpty())
        RTDirRemove(pCtx->mURI.strDropDir.c_str());

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
            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, pCBData->rc);
            if (RT_SUCCESS(rc))
                rc = pCBData->rc;
            break;
        }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_FAILURE(rc))
        fNotify = true;

    if (fNotify)
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

            if (pThis->mData.mProtocolVersion <= 1)
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
            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, pCBData->rc);
            if (RT_SUCCESS(rc))
                rc = pCBData->rc;
            break;
        }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_FAILURE(rc))
        fNotify = true;

    /* All URI data processed? */
    if (pCtx->mData.cbProcessed >= pCtx->mData.cbToProcess)
    {
        Assert(pCtx->mData.cbProcessed == pCtx->mData.cbToProcess);
        fNotify = true;
    }

    LogFlowFunc(("cbProcessed=%RU64, cbToProcess=%RU64, fNotify=%RTbool\n",
                 pCtx->mData.cbProcessed, pCtx->mData.cbToProcess, fNotify));

    if (fNotify)
    {
        int rc2 = pCtx->mCallback.Notify(rc);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc; /* Tell the guest. */
}

int GuestDnDSource::i_updateProcess(PRECVDATACTX pCtx, uint32_t cbDataAdd)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

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

