/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag and drop source.
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_GUEST_DND //LOG_GROUP_MAIN_GUESTDNDSOURCE
#include "LoggingNew.h"

#include "GuestImpl.h"
#include "GuestDnDSourceImpl.h"
#include "GuestDnDPrivate.h"
#include "ConsoleImpl.h"

#include "Global.h"
#include "AutoCaller.h"
#include "ThreadTask.h"

#include <iprt/asm.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/uri.h>

#include <iprt/cpp/utils.h> /* For unconst(). */

#include <VBox/com/array.h>


/**
 * Base class for a source task.
 */
class GuestDnDSourceTask : public ThreadTask
{
public:

    GuestDnDSourceTask(GuestDnDSource *pSource)
        : ThreadTask("GenericGuestDnDSourceTask")
        , mSource(pSource)
        , mRC(VINF_SUCCESS) { }

    virtual ~GuestDnDSourceTask(void) { }

    int getRC(void) const { return mRC; }
    bool isOk(void) const { return RT_SUCCESS(mRC); }

protected:

    const ComObjPtr<GuestDnDSource>     mSource;
    int                                 mRC;
};

/**
 * Task structure for receiving data from a source using
 * a worker thread.
 */
class GuestDnDRecvDataTask : public GuestDnDSourceTask
{
public:

    GuestDnDRecvDataTask(GuestDnDSource *pSource, GuestDnDRecvCtx *pCtx)
        : GuestDnDSourceTask(pSource)
        , mpCtx(pCtx)
    {
        m_strTaskName = "dndSrcRcvData";
    }

    void handler()
    {
        LogFlowThisFunc(("\n"));

        const ComObjPtr<GuestDnDSource> pThis(mSource);
        Assert(!pThis.isNull());

        AutoCaller autoCaller(pThis);
        if (FAILED(autoCaller.rc()))
            return;

        int vrc = pThis->i_receiveData(mpCtx, RT_INDEFINITE_WAIT /* msTimeout */);
        if (RT_FAILURE(vrc)) /* In case we missed some error handling within i_receiveData(). */
        {
            if (vrc != VERR_CANCELLED)
                LogRel(("DnD: Receiving data from guest failed with %Rrc\n", vrc));

            /* Make sure to fire a cancel request to the guest side in case something went wrong. */
            pThis->sendCancel();
        }
    }

    virtual ~GuestDnDRecvDataTask(void) { }

protected:

    /** Pointer to receive data context. */
    GuestDnDRecvCtx *mpCtx;
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
    mData.mcbBlockSize = DND_DEFAULT_CHUNK_SIZE; /** @todo Make this configurable. */

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

    *aSupported = GuestDnDBase::i_isFormatSupported(aFormat) ? TRUE : FALSE;

    return S_OK;
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

    aFormats = GuestDnDBase::i_getFormats();

    return S_OK;
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
    GuestDnDBase::getProtocolVersion(&m_DataBase.uProtocolVersion);

    /* Default is ignoring the action. */
    if (aDefaultAction)
        *aDefaultAction = DnDAction_Ignore;

    HRESULT hr = S_OK;

    GuestDnDMsg Msg;
    Msg.setType(HOST_DND_GH_REQ_PENDING);
    if (m_DataBase.uProtocolVersion >= 3)
        Msg.appendUInt32(0); /** @todo ContextID not used yet. */
    Msg.appendUInt32(uScreenId);

    int rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
    if (RT_SUCCESS(rc))
    {
        GuestDnDResponse *pResp = GuestDnDInst()->response();
        AssertPtr(pResp);

        bool fFetchResult = true;

        rc = pResp->waitForGuestResponse(100 /* Timeout in ms */);
        if (RT_FAILURE(rc))
            fFetchResult = false;

        if (   fFetchResult
            && isDnDIgnoreAction(pResp->getActionDefault()))
            fFetchResult = false;

        /* Fetch the default action to use. */
        if (fFetchResult)
        {
            /*
             * In the GuestDnDSource case the source formats are from the guest,
             * as GuestDnDSource acts as a target for the guest. The host always
             * dictates what's supported and what's not, so filter out all formats
             * which are not supported by the host.
             */
            GuestDnDMIMEList lstFiltered  = GuestDnD::toFilteredFormatList(m_lstFmtSupported, pResp->formats());
            if (lstFiltered.size())
            {
                LogRel3(("DnD: Host offered the following formats:\n"));
                for (size_t i = 0; i < lstFiltered.size(); i++)
                    LogRel3(("DnD:\tFormat #%zu: %s\n", i, lstFiltered.at(i).c_str()));

                aFormats            = lstFiltered;
                aAllowedActions     = GuestDnD::toMainActions(pResp->getActionsAllowed());
                if (aDefaultAction)
                    *aDefaultAction = GuestDnD::toMainAction(pResp->getActionDefault());

                /* Apply the (filtered) formats list. */
                m_lstFmtOffered     = lstFiltered;
            }
            else
                LogRel2(("DnD: Negotiation of formats between guest and host failed, drag and drop to host not possible\n"));
        }

        LogFlowFunc(("fFetchResult=%RTbool, lstActionsAllowed=0x%x\n", fFetchResult, pResp->getActionsAllowed()));
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

    LogFunc(("aFormat=%s, aAction=%RU32\n", aFormat.c_str(), aAction));

    /* Input validation. */
    if (RT_UNLIKELY((aFormat.c_str()) == NULL || *(aFormat.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No drop format specified"));

    /* Is the specified format in our list of (left over) offered formats? */
    if (!GuestDnD::isFormatInFormatList(aFormat, m_lstFmtOffered))
        return setError(E_INVALIDARG, tr("Specified format '%s' is not supported"), aFormat.c_str());

    /* Check that the given action is supported by us. */
    VBOXDNDACTION dndAction = GuestDnD::toHGCMAction(aAction);
    if (isDnDIgnoreAction(dndAction)) /* If there is no usable action, ignore this request. */
        return S_OK;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Check if this object still is in a pending state and bail out if so. */
    if (m_fIsPending)
        return setError(E_FAIL, tr("Current drop operation to host still in progress"));

    /* Reset our internal state. */
    i_reset();

    /* At the moment we only support one transfer at a time. */
    if (GuestDnDInst()->getSourceCount())
        return setError(E_INVALIDARG, tr("Another drag and drop operation to the host already is in progress"));

    /* Reset progress object. */
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    AssertPtr(pResp);
    HRESULT hr = pResp->resetProgress(m_pGuest);
    if (FAILED(hr))
        return hr;

    GuestDnDRecvDataTask *pTask = NULL;

    try
    {
        mData.mRecvCtx.pSource       = this;
        mData.mRecvCtx.pResp         = pResp;
        mData.mRecvCtx.enmAction     = dndAction;
        mData.mRecvCtx.strFmtReq     = aFormat;
        mData.mRecvCtx.lstFmtOffered = m_lstFmtOffered;

        LogRel2(("DnD: Requesting data from guest in format '%s'\n", aFormat.c_str()));

        pTask = new GuestDnDRecvDataTask(this, &mData.mRecvCtx);
        if (!pTask->isOk())
        {
            delete pTask;
            LogRel2(("DnD: Receive data task failed to initialize\n"));
            throw hr = E_FAIL;
        }

        /* Drop write lock before creating thread. */
        alock.release();

        /* This function delete pTask in case of exceptions,
         * so there is no need in the call of delete operator. */
        hr = pTask->createThreadWithType(RTTHREADTYPE_MAIN_WORKER);
        pTask = NULL;  /* Note: pTask is now owned by the worker thread. */
    }
    catch (std::bad_alloc &)
    {
        hr = setError(E_OUTOFMEMORY);
    }
    catch (...)
    {
        LogRel2(("DnD: Could not create thread for data receiving task\n"));
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        /* Register ourselves at the DnD manager. */
        GuestDnDInst()->registerSource(this);

        hr = pResp->queryProgressTo(aProgress.asOutParam());
        ComAssertComRC(hr);

    }
    else
        hr = setError(hr, tr("Starting thread for GuestDnDSource failed (%Rhrc)"), hr);

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDSource::receiveData(std::vector<BYTE> &aData)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Don't allow receiving the actual data until our current transfer is complete. */
    if (m_fIsPending)
        return setError(E_FAIL, tr("Current drop operation to host still in progress"));

    HRESULT hr = S_OK;

    try
    {
        GuestDnDRecvCtx *pCtx = &mData.mRecvCtx;
        if (DnDMIMENeedsDropDir(pCtx->strFmtRecv.c_str(), pCtx->strFmtRecv.length()))
        {
            PDNDDROPPEDFILES pDF = &pCtx->Transfer.DroppedFiles;

            const char *pcszDropDirAbs = DnDDroppedFilesGetDirAbs(pDF);
            AssertPtr(pcszDropDirAbs);

            LogRel2(("DnD: Using drop directory '%s', got %RU64 root entries\n",
                     pcszDropDirAbs, DnDTransferListGetRootCount(&pCtx->Transfer.List)));

            /* We return the data as "text/uri-list" MIME data here. */
            char  *pszBuf = NULL;
            size_t cbBuf  = 0;
            int rc = DnDTransferListGetRootsEx(&pCtx->Transfer.List, DNDTRANSFERLISTFMT_URI,
                                               pcszDropDirAbs, DND_PATH_SEPARATOR, &pszBuf, &cbBuf);
            if (RT_SUCCESS(rc))
            {
                Assert(cbBuf);
                AssertPtr(pszBuf);

                aData.resize(cbBuf);
                memcpy(&aData.front(), pszBuf, cbBuf);
                RTStrFree(pszBuf);
            }
            else
                LogRel(("DnD: Unable to build source root list, rc=%Rrc\n", rc));
        }
        else /* Raw data. */
        {
            if (pCtx->Meta.cbData)
            {
                /* Copy the data into a safe array of bytes. */
                aData.resize(pCtx->Meta.cbData);
                memcpy(&aData.front(), pCtx->Meta.pvData, pCtx->Meta.cbData);
            }
            else
                aData.resize(0);
        }
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
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

        case VERR_DISK_FULL:
            strError += Utf8StrFmt(tr("Host disk ran out of space (disk is full)."));
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

void GuestDnDSource::i_reset(void)
{
    LogFlowThisFunc(("\n"));

    mData.mRecvCtx.reset();

    m_fIsPending = false;

    /* Unregister ourselves from the DnD manager. */
    GuestDnDInst()->unregisterSource(this);
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH

/**
 * Handles receiving a send data header from the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Receive context to use.
 * @param   pDataHdr            Pointer to send data header from the guest.
 */
int GuestDnDSource::i_onReceiveDataHdr(GuestDnDRecvCtx *pCtx, PVBOXDNDSNDDATAHDR pDataHdr)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    LogRel2(("DnD: Receiving %RU64 bytes total data (%RU32 bytes meta data, %RU64 objects) from guest ...\n",
             pDataHdr->cbTotal, pDataHdr->cbMeta, pDataHdr->cObjects));

    AssertReturn(pDataHdr->cbTotal >= pDataHdr->cbMeta, VERR_INVALID_PARAMETER);

    pCtx->Meta.cbAnnounced = pDataHdr->cbMeta;
    pCtx->cbExtra          = pDataHdr->cbTotal - pDataHdr->cbMeta;

    Assert(pCtx->Transfer.cObjToProcess == 0); /* Sanity. */
    Assert(pCtx->Transfer.cObjProcessed == 0);

    pCtx->Transfer.reset();

    pCtx->Transfer.cObjToProcess = pDataHdr->cObjects;

    /** @todo Handle compression type. */
    /** @todo Handle checksum type. */

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Handles receiving a send data block from the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Receive context to use.
 * @param   pSndData            Pointer to send data block from the guest.
 */
int GuestDnDSource::i_onReceiveData(GuestDnDRecvCtx *pCtx, PVBOXDNDSNDDATA pSndData)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pSndData, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    try
    {
        GuestDnDTransferRecvData *pTransfer = &pCtx->Transfer;

        size_t  cbData;
        void   *pvData;
        size_t  cbTotalAnnounced;
        size_t  cbMetaAnnounced;

        if (m_DataBase.uProtocolVersion < 3)
        {
            cbData  = pSndData->u.v1.cbData;
            pvData  = pSndData->u.v1.pvData;

            /* Sends the total data size to receive for every data chunk. */
            cbTotalAnnounced = pSndData->u.v1.cbTotalSize;

            /* Meta data size always is cbData, meaning there cannot be an
             * extended data chunk transfer by sending further data. */
            cbMetaAnnounced  = cbData;
        }
        else
        {
            cbData  = pSndData->u.v3.cbData;
            pvData  = pSndData->u.v3.pvData;

            /* Note: Data sizes get initialized in i_onReceiveDataHdr().
             *       So just use the set values here. */
            cbTotalAnnounced = pCtx->getTotalAnnounced();
            cbMetaAnnounced  = pCtx->Meta.cbAnnounced;
        }

        if (cbData > cbTotalAnnounced)
        {
            AssertMsgFailed(("Incoming data size invalid: cbData=%zu, cbTotal=%zu\n", cbData, cbTotalAnnounced));
            rc = VERR_INVALID_PARAMETER;
        }
        else if (   cbTotalAnnounced == 0
                 || cbTotalAnnounced  < cbMetaAnnounced)
        {
            AssertMsgFailed(("cbTotal (%zu) is smaller than cbMeta (%zu)\n", cbTotalAnnounced, cbMetaAnnounced));
            rc = VERR_INVALID_PARAMETER;
        }

        if (RT_FAILURE(rc))
            return rc;

        AssertReturn(cbData <= mData.mcbBlockSize, VERR_BUFFER_OVERFLOW);

        const size_t cbMetaRecv = pCtx->Meta.add(pvData, cbData);
        AssertReturn(cbMetaRecv <= pCtx->Meta.cbData, VERR_BUFFER_OVERFLOW);

        LogFlowThisFunc(("cbData=%zu, cbMetaRecv=%zu, cbMetaAnnounced=%zu, cbTotalAnnounced=%zu\n",
                         cbData, cbMetaRecv, cbMetaAnnounced, cbTotalAnnounced));

        LogRel2(("DnD: %RU8%% of meta data complete (%zu/%zu bytes)\n",
                 (uint8_t)(cbMetaRecv * 100 / RT_MAX(cbMetaAnnounced, 1)), cbMetaRecv, cbMetaAnnounced));

        /*
         * (Meta) Data transfer complete?
         */
        if (cbMetaAnnounced == cbMetaRecv)
        {
            LogRel2(("DnD: Receiving meta data complete\n"));

            if (DnDMIMENeedsDropDir(pCtx->strFmtRecv.c_str(), pCtx->strFmtRecv.length()))
            {
                rc = DnDTransferListInitEx(&pTransfer->List,
                                           DnDDroppedFilesGetDirAbs(&pTransfer->DroppedFiles), DNDTRANSFERLISTFMT_NATIVE);
                if (RT_SUCCESS(rc))
                    rc = DnDTransferListAppendRootsFromBuffer(&pTransfer->List, DNDTRANSFERLISTFMT_URI,
                                                              (const char *)pCtx->Meta.pvData, pCtx->Meta.cbData, DND_PATH_SEPARATOR,
                                                              DNDTRANSFERLIST_FLAGS_NONE);
                /* Validation. */
                if (RT_SUCCESS(rc))
                {
                    uint64_t cRoots = DnDTransferListGetRootCount(&pTransfer->List);

                    LogRel2(("DnD: Received %RU64 root entries from guest\n", cRoots));

                    if (   cRoots == 0
                        || cRoots > pTransfer->cObjToProcess)
                    {
                        LogRel(("DnD: Number of root entries invalid / mismatch: Got %RU64, expected %RU64\n",
                                cRoots, pTransfer->cObjToProcess));
                        rc = VERR_INVALID_PARAMETER;
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    /* Update our process with the data we already received. */
                    rc = updateProgress(pCtx, pCtx->pResp, cbMetaAnnounced);
                    AssertRC(rc);
                }

                if (RT_FAILURE(rc))
                    LogRel(("DnD: Error building root entry list, rc=%Rrc\n", rc));
            }
            else /* Raw data. */
            {
                rc = updateProgress(pCtx, pCtx->pResp, cbData);
                AssertRC(rc);
            }

            if (RT_FAILURE(rc))
                LogRel(("DnD: Error receiving meta data, rc=%Rrc\n", rc));
        }
    }
    catch (std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_onReceiveDir(GuestDnDRecvCtx *pCtx, const char *pszPath, uint32_t cbPath, uint32_t fMode)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath,     VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x\n", pszPath, cbPath, fMode));

    const PDNDTRANSFEROBJECT pObj = &pCtx->Transfer.ObjCur;
    const PDNDDROPPEDFILES   pDF  = &pCtx->Transfer.DroppedFiles;

    int rc = DnDTransferObjectInitEx(pObj, DNDTRANSFEROBJTYPE_DIRECTORY,
                                     DnDDroppedFilesGetDirAbs(pDF), pszPath);
    if (RT_SUCCESS(rc))
    {
        const char *pcszPathAbs = DnDTransferObjectGetSourcePath(pObj);
        AssertPtr(pcszPathAbs);

        rc = RTDirCreateFullPath(pcszPathAbs, fMode);
        if (RT_SUCCESS(rc))
        {
            pCtx->Transfer.cObjProcessed++;
            if (pCtx->Transfer.cObjProcessed <= pCtx->Transfer.cObjToProcess)
            {
                rc = DnDDroppedFilesAddDir(pDF, pcszPathAbs);
            }
            else
                rc = VERR_TOO_MUCH_DATA;

            DnDTransferObjectDestroy(pObj);

            if (RT_FAILURE(rc))
                LogRel2(("DnD: Created guest directory '%s' on host\n", pcszPathAbs));
        }
        else
            LogRel(("DnD: Error creating guest directory '%s' on host, rc=%Rrc\n", pcszPathAbs, rc));
    }

    if (RT_FAILURE(rc))
        LogRel(("DnD: Receiving guest directory '%s' failed with rc=%Rrc\n", pszPath, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Receives a file header from the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Receive context to use.
 * @param   pszPath             File path of file to use.
 * @param   cbPath              Size (in bytes, including terminator) of file path.
 * @param   cbSize              File size (in bytes) to receive.
 * @param   fMode               File mode to use.
 * @param   fFlags              Additional receive flags; not used yet.
 */
int GuestDnDSource::i_onReceiveFileHdr(GuestDnDRecvCtx *pCtx, const char *pszPath, uint32_t cbPath,
                                       uint64_t cbSize, uint32_t fMode, uint32_t fFlags)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath,     VERR_INVALID_PARAMETER);
    AssertReturn(fMode,      VERR_INVALID_PARAMETER);
    /* fFlags are optional. */

    RT_NOREF(fFlags);

    LogFlowFunc(("pszPath=%s, cbPath=%RU32, cbSize=%RU64, fMode=0x%x, fFlags=0x%x\n", pszPath, cbPath, cbSize, fMode, fFlags));

    AssertMsgReturn(cbSize <= pCtx->cbExtra,
                    ("File size (%RU64) exceeds extra size to transfer (%RU64)\n", cbSize, pCtx->cbExtra), VERR_INVALID_PARAMETER);
    AssertMsgReturn(   pCtx->isComplete() == false
                    && pCtx->Transfer.cObjToProcess,
                    ("Data transfer already complete, bailing out\n"), VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    do
    {
        const PDNDTRANSFEROBJECT pObj = &pCtx->Transfer.ObjCur;

        if (    DnDTransferObjectIsOpen(pObj)
            && !DnDTransferObjectIsComplete(pObj))
        {
            AssertMsgFailed(("Object '%s' not complete yet\n", DnDTransferObjectGetSourcePath(pObj)));
            rc = VERR_WRONG_ORDER;
            break;
        }

        const PDNDDROPPEDFILES pDF = &pCtx->Transfer.DroppedFiles;

        rc = DnDTransferObjectInitEx(pObj, DNDTRANSFEROBJTYPE_FILE, DnDDroppedFilesGetDirAbs(pDF), pszPath);
        AssertRCBreak(rc);

        const char *pcszSource = DnDTransferObjectGetSourcePath(pObj);
        AssertPtrBreakStmt(pcszSource, VERR_INVALID_POINTER);

        /** @todo Add sparse file support based on fFlags? (Use Open(..., fFlags | SPARSE). */
        rc = DnDTransferObjectOpen(pObj, RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE,
                                   (fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRUSR | RTFS_UNIX_IWUSR, DNDTRANSFEROBJECT_FLAGS_NONE);
        if (RT_FAILURE(rc))
        {
            LogRel(("DnD: Error opening/creating guest file '%s' on host, rc=%Rrc\n", pcszSource, rc));
            break;
        }

        /* Note: Protocol v1 does not send any file sizes, so always 0. */
        if (m_DataBase.uProtocolVersion >= 2)
            rc = DnDTransferObjectSetSize(pObj, cbSize);

        /** @todo Unescape path before printing. */
        LogRel2(("DnD: Transferring guest file '%s' to host (%RU64 bytes, mode %#x)\n",
                 pcszSource, DnDTransferObjectGetSize(pObj), DnDTransferObjectGetMode(pObj)));

        /** @todo Set progress object title to current file being transferred? */

        if (DnDTransferObjectIsComplete(pObj)) /* 0-byte file? We're done already. */
        {
            LogRel2(("DnD: Transferring guest file '%s' (0 bytes) to host complete\n", pcszSource));

            pCtx->Transfer.cObjProcessed++;
            if (pCtx->Transfer.cObjProcessed <= pCtx->Transfer.cObjToProcess)
            {
                /* Add for having a proper rollback. */
                rc = DnDDroppedFilesAddFile(pDF, pcszSource);
            }
            else
                rc = VERR_TOO_MUCH_DATA;

            DnDTransferObjectDestroy(pObj);
        }

    } while (0);

    if (RT_FAILURE(rc))
        LogRel(("DnD: Error receiving guest file header, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_onReceiveFileData(GuestDnDRecvCtx *pCtx, const void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    LogFlowFunc(("pvData=%p, cbData=%RU32, cbBlockSize=%RU32\n", pvData, cbData, mData.mcbBlockSize));

    /*
     * Sanity checking.
     */
    if (cbData > mData.mcbBlockSize)
        return VERR_INVALID_PARAMETER;

    do
    {
        const PDNDTRANSFEROBJECT pObj = &pCtx->Transfer.ObjCur;

        const char *pcszSource = DnDTransferObjectGetSourcePath(pObj);
        AssertPtrBreakStmt(pcszSource, VERR_INVALID_POINTER);

        AssertMsgReturn(DnDTransferObjectIsOpen(pObj),
                        ("Object '%s' not open (anymore)\n", pcszSource), VERR_WRONG_ORDER);
        AssertMsgReturn(DnDTransferObjectIsComplete(pObj) == false,
                        ("Object '%s' already marked as complete\n", pcszSource), VERR_WRONG_ORDER);

        uint32_t cbWritten;
        rc = DnDTransferObjectWrite(pObj, pvData, cbData, &cbWritten);
        if (RT_FAILURE(rc))
            LogRel(("DnD: Error writing guest file data for '%s', rc=%Rrc\n", pcszSource, rc));

        Assert(cbWritten <= cbData);
        if (cbWritten < cbData)
        {
            LogRel(("DnD: Only written %RU32 of %RU32 bytes of guest file '%s' -- disk full?\n",
                    cbWritten, cbData, pcszSource));
            rc = VERR_IO_GEN_FAILURE; /** @todo Find a better rc. */
            break;
        }

        rc = updateProgress(pCtx, pCtx->pResp, cbWritten);
        AssertRCBreak(rc);

        if (DnDTransferObjectIsComplete(pObj))
        {
            LogRel2(("DnD: Transferring guest file '%s' to host complete\n", pcszSource));

            pCtx->Transfer.cObjProcessed++;
            if (pCtx->Transfer.cObjProcessed > pCtx->Transfer.cObjToProcess)
                rc = VERR_TOO_MUCH_DATA;

            DnDTransferObjectDestroy(pObj);
        }

    } while (0);

    if (RT_FAILURE(rc))
        LogRel(("DnD: Error receiving guest file data, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

/**
 * @returns VBox status code that the caller ignores. Not sure if that's
 *          intentional or not.
 */
int GuestDnDSource::i_receiveData(GuestDnDRecvCtx *pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    /* Sanity. */
    AssertMsgReturn(pCtx->enmAction,
                    ("Action to perform is none when it shouldn't\n"), VERR_INVALID_PARAMETER);
    AssertMsgReturn(pCtx->strFmtReq.isNotEmpty(),
                    ("Requested format from host is empty when it shouldn't\n"), VERR_INVALID_PARAMETER);

    /*
     * Do we need to receive a different format than initially requested?
     *
     * For example, receiving a file link as "text/plain" requires still to receive
     * the file from the guest as "text/uri-list" first, then pointing to
     * the file path on the host in the "text/plain" data returned.
     */

    bool fFoundFormat = true; /* Whether we've found a common format between host + guest. */

    LogFlowFunc(("strFmtReq=%s, strFmtRecv=%s, enmAction=0x%x\n",
                 pCtx->strFmtReq.c_str(), pCtx->strFmtRecv.c_str(), pCtx->enmAction));

    /* Plain text wanted? */
    if (   pCtx->strFmtReq.equalsIgnoreCase("text/plain")
        || pCtx->strFmtReq.equalsIgnoreCase("text/plain;charset=utf-8"))
    {
        /* Did the guest offer a file? Receive a file instead. */
        if (GuestDnD::isFormatInFormatList("text/uri-list", pCtx->lstFmtOffered))
            pCtx->strFmtRecv = "text/uri-list";
        /* Guest only offers (plain) text. */
        else
            pCtx->strFmtRecv = "text/plain;charset=utf-8";

        /** @todo Add more conversions here. */
    }
    /* File(s) wanted? */
    else if (pCtx->strFmtReq.equalsIgnoreCase("text/uri-list"))
    {
        /* Does the guest support sending files? */
        if (GuestDnD::isFormatInFormatList("text/uri-list", pCtx->lstFmtOffered))
            pCtx->strFmtRecv = "text/uri-list";
        else /* Bail out. */
            fFoundFormat = false;
    }

    int rc = VINF_SUCCESS;

    if (fFoundFormat)
    {
        if (!pCtx->strFmtRecv.equals(pCtx->strFmtReq))
            LogRel2(("DnD: Requested data in format '%s', receiving in intermediate format '%s' now\n",
                     pCtx->strFmtReq.c_str(), pCtx->strFmtRecv.c_str()));

        /*
         * Call the appropriate receive handler based on the data format to handle.
         */
        bool fURIData = DnDMIMENeedsDropDir(pCtx->strFmtRecv.c_str(), pCtx->strFmtRecv.length());
        if (fURIData)
        {
            rc = i_receiveTransferData(pCtx, msTimeout);
        }
        else
        {
            rc = i_receiveRawData(pCtx, msTimeout);
        }
    }
    else /* Just inform the user (if verbose release logging is enabled). */
    {
        LogRel(("DnD: The guest does not support format '%s':\n", pCtx->strFmtReq.c_str()));
        LogRel(("DnD: Guest offered the following formats:\n"));
        for (size_t i = 0; i < pCtx->lstFmtOffered.size(); i++)
            LogRel(("DnD:\tFormat #%zu: %s\n", i, pCtx->lstFmtOffered.at(i).c_str()));

        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("DnD: Receiving data from guest failed with %Rrc\n", rc));

        /* Reset state. */
        i_reset();
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_receiveRawData(GuestDnDRecvCtx *pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc;

    LogFlowFuncEnter();

    GuestDnDResponse *pResp = pCtx->pResp;
    AssertPtr(pCtx->pResp);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

#define REGISTER_CALLBACK(x) \
    do {                                                            \
        rc = pResp->setCallback(x, i_receiveRawDataCallback, pCtx); \
        if (RT_FAILURE(rc))                                         \
            return rc;                                              \
    } while (0)

#define UNREGISTER_CALLBACK(x)                                      \
    do {                                                            \
        int rc2 = pResp->setCallback(x, NULL);                      \
        AssertRC(rc2);                                              \
    } while (0)

    /*
     * Register callbacks.
     */
    REGISTER_CALLBACK(GUEST_DND_CONNECT);
    REGISTER_CALLBACK(GUEST_DND_DISCONNECT);
    REGISTER_CALLBACK(GUEST_DND_GH_EVT_ERROR);
    if (m_DataBase.uProtocolVersion >= 3)
        REGISTER_CALLBACK(GUEST_DND_GH_SND_DATA_HDR);
    REGISTER_CALLBACK(GUEST_DND_GH_SND_DATA);

    do
    {
        /*
         * Receive the raw data.
         */
        GuestDnDMsg Msg;
        Msg.setType(HOST_DND_GH_EVT_DROPPED);
        if (m_DataBase.uProtocolVersion >= 3)
            Msg.appendUInt32(0); /** @todo ContextID not used yet. */
        Msg.appendPointer((void*)pCtx->strFmtRecv.c_str(), (uint32_t)pCtx->strFmtRecv.length() + 1);
        Msg.appendUInt32((uint32_t)pCtx->strFmtRecv.length() + 1);
        Msg.appendUInt32(pCtx->enmAction);

        /* Make the initial call to the guest by telling that we initiated the "dropped" event on
         * the host and therefore now waiting for the actual raw data. */
        rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            rc = waitForEvent(&pCtx->EventCallback, pCtx->pResp, msTimeout);
            if (RT_SUCCESS(rc))
                rc = pCtx->pResp->setProgress(100, DND_PROGRESS_COMPLETE, VINF_SUCCESS);
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    UNREGISTER_CALLBACK(GUEST_DND_CONNECT);
    UNREGISTER_CALLBACK(GUEST_DND_DISCONNECT);
    UNREGISTER_CALLBACK(GUEST_DND_GH_EVT_ERROR);
    if (m_DataBase.uProtocolVersion >= 3)
        UNREGISTER_CALLBACK(GUEST_DND_GH_SND_DATA_HDR);
    UNREGISTER_CALLBACK(GUEST_DND_GH_SND_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    if (RT_FAILURE(rc))
    {
        if (rc == VERR_CANCELLED) /* Transfer was cancelled by the host. */
        {
            /*
             * Now that we've cleaned up tell the guest side to cancel.
             * This does not imply we're waiting for the guest to react, as the
             * host side never must depend on anything from the guest.
             */
            int rc2 = sendCancel();
            AssertRC(rc2);

            rc2 = pCtx->pResp->setProgress(100, DND_PROGRESS_CANCELLED);
            AssertRC(rc2);
        }
        else if (rc != VERR_GSTDND_GUEST_ERROR) /* Guest-side error are already handled in the callback. */
        {
            int rc2 = pCtx->pResp->setProgress(100, DND_PROGRESS_ERROR,
                                               rc, GuestDnDSource::i_hostErrorToString(rc));
            AssertRC(rc2);
        }

        rc = VINF_SUCCESS; /* The error was handled by the setProgress() calls above. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDSource::i_receiveTransferData(GuestDnDRecvCtx *pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc;

    LogFlowFuncEnter();

    GuestDnDResponse *pResp = pCtx->pResp;
    AssertPtr(pCtx->pResp);

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

#define REGISTER_CALLBACK(x)                                             \
    do {                                                                 \
        rc = pResp->setCallback(x, i_receiveTransferDataCallback, pCtx); \
        if (RT_FAILURE(rc))                                              \
            return rc;                                                   \
    } while (0)

#define UNREGISTER_CALLBACK(x)                                      \
    do {                                                            \
        int rc2 = pResp->setCallback(x, NULL);                      \
        AssertRC(rc2);                                              \
    } while (0)

    /*
     * Register callbacks.
     */
    /* Guest callbacks. */
    REGISTER_CALLBACK(GUEST_DND_CONNECT);
    REGISTER_CALLBACK(GUEST_DND_DISCONNECT);
    REGISTER_CALLBACK(GUEST_DND_GH_EVT_ERROR);
    if (m_DataBase.uProtocolVersion >= 3)
        REGISTER_CALLBACK(GUEST_DND_GH_SND_DATA_HDR);
    REGISTER_CALLBACK(GUEST_DND_GH_SND_DATA);
    REGISTER_CALLBACK(GUEST_DND_GH_SND_DIR);
    if (m_DataBase.uProtocolVersion >= 2)
        REGISTER_CALLBACK(GUEST_DND_GH_SND_FILE_HDR);
    REGISTER_CALLBACK(GUEST_DND_GH_SND_FILE_DATA);

    const PDNDDROPPEDFILES pDF = &pCtx->Transfer.DroppedFiles;

    do
    {
        rc = DnDDroppedFilesOpenTemp(pDF, 0 /* fFlags */);
        if (RT_FAILURE(rc))
        {
            LogRel(("DnD: Opening dropped files directory '%s' on the host failed with rc=%Rrc\n",
                    DnDDroppedFilesGetDirAbs(pDF), rc));
            break;
        }

        /*
         * Receive the transfer list.
         */
        GuestDnDMsg Msg;
        Msg.setType(HOST_DND_GH_EVT_DROPPED);
        if (m_DataBase.uProtocolVersion >= 3)
            Msg.appendUInt32(0); /** @todo ContextID not used yet. */
        Msg.appendPointer((void*)pCtx->strFmtRecv.c_str(), (uint32_t)pCtx->strFmtRecv.length() + 1);
        Msg.appendUInt32((uint32_t)pCtx->strFmtRecv.length() + 1);
        Msg.appendUInt32(pCtx->enmAction);

        /* Make the initial call to the guest by telling that we initiated the "dropped" event on
         * the host and therefore now waiting for the actual URI data. */
        rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("Waiting ...\n"));

            rc = waitForEvent(&pCtx->EventCallback, pCtx->pResp, msTimeout);
            if (RT_SUCCESS(rc))
                rc = pCtx->pResp->setProgress(100, DND_PROGRESS_COMPLETE, VINF_SUCCESS);

            LogFlowFunc(("Waiting ended with rc=%Rrc\n", rc));
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    UNREGISTER_CALLBACK(GUEST_DND_CONNECT);
    UNREGISTER_CALLBACK(GUEST_DND_DISCONNECT);
    UNREGISTER_CALLBACK(GUEST_DND_GH_EVT_ERROR);
    UNREGISTER_CALLBACK(GUEST_DND_GH_SND_DATA_HDR);
    UNREGISTER_CALLBACK(GUEST_DND_GH_SND_DATA);
    UNREGISTER_CALLBACK(GUEST_DND_GH_SND_DIR);
    UNREGISTER_CALLBACK(GUEST_DND_GH_SND_FILE_HDR);
    UNREGISTER_CALLBACK(GUEST_DND_GH_SND_FILE_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    if (RT_FAILURE(rc))
    {
        int rc2 = DnDDroppedFilesRollback(pDF);
        if (RT_FAILURE(rc2))
            LogRel(("DnD: Deleting left over temporary files failed (%Rrc), please remove directory '%s' manually\n",
                    rc2, DnDDroppedFilesGetDirAbs(pDF)));

        if (rc == VERR_CANCELLED)
        {
            /*
             * Now that we've cleaned up tell the guest side to cancel.
             * This does not imply we're waiting for the guest to react, as the
             * host side never must depend on anything from the guest.
             */
            rc2 = sendCancel();
            AssertRC(rc2);

            rc2 = pCtx->pResp->setProgress(100, DND_PROGRESS_CANCELLED);
            AssertRC(rc2);
        }
        else if (rc != VERR_GSTDND_GUEST_ERROR) /* Guest-side error are already handled in the callback. */
        {
            rc2 = pCtx->pResp->setProgress(100, DND_PROGRESS_ERROR,
                                           rc, GuestDnDSource::i_hostErrorToString(rc));
            AssertRC(rc2);
        }

        rc = VINF_SUCCESS; /* The error was handled by the setProgress() calls above. */
    }

    DnDDroppedFilesClose(pDF);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
DECLCALLBACK(int) GuestDnDSource::i_receiveRawDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    GuestDnDRecvCtx *pCtx = (GuestDnDRecvCtx *)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDSource *pThis = pCtx->pSource;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("pThis=%p, uMsg=%RU32\n", pThis, uMsg));

    int rc = VINF_SUCCESS;

    int rcCallback = VINF_SUCCESS; /* rc for the callback. */
    bool fNotify   = false;

    switch (uMsg)
    {
        case GUEST_DND_CONNECT:
            /* Nothing to do here (yet). */
            break;

        case GUEST_DND_DISCONNECT:
            rc = VERR_CANCELLED;
            break;

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case GUEST_DND_GH_SND_DATA_HDR:
        {
            PVBOXDNDCBSNDDATAHDRDATA pCBData = reinterpret_cast<PVBOXDNDCBSNDDATAHDRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDDATAHDRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_DATA_HDR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveDataHdr(pCtx, &pCBData->data);
            break;
        }
        case GUEST_DND_GH_SND_DATA:
        {
            PVBOXDNDCBSNDDATADATA pCBData = reinterpret_cast<PVBOXDNDCBSNDDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_DATA == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveData(pCtx, &pCBData->data);
            break;
        }
        case GUEST_DND_GH_EVT_ERROR:
        {
            PVBOXDNDCBEVTERRORDATA pCBData = reinterpret_cast<PVBOXDNDCBEVTERRORDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBEVTERRORDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_EVT_ERROR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            pCtx->pResp->reset();

            if (RT_SUCCESS(pCBData->rc))
            {
                AssertMsgFailed(("Received guest error with no error code set\n"));
                pCBData->rc = VERR_GENERAL_FAILURE; /* Make sure some error is set. */
            }
            else if (pCBData->rc == VERR_WRONG_ORDER)
            {
                rc = pCtx->pResp->setProgress(100, DND_PROGRESS_CANCELLED);
            }
            else
                rc = pCtx->pResp->setProgress(100, DND_PROGRESS_ERROR, pCBData->rc,
                                              GuestDnDSource::i_guestErrorToString(pCBData->rc));

            LogRel3(("DnD: Guest reported data transfer error: %Rrc\n", pCBData->rc));

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
                LogRel2(("DnD: Data transfer to host complete\n"));
                break;

            case VERR_CANCELLED:
                LogRel2(("DnD: Data transfer to host canceled\n"));
                break;

            default:
                LogRel(("DnD: Error %Rrc occurred, aborting data transfer to host\n", rc));
                break;
        }

        /* Unregister this callback. */
        AssertPtr(pCtx->pResp);
        int rc2 = pCtx->pResp->setCallback(uMsg, NULL /* PFNGUESTDNDCALLBACK */);
        AssertRC(rc2);
    }

    /* All data processed? */
    if (pCtx->isComplete())
        fNotify = true;

    LogFlowFunc(("cbProcessed=%RU64, cbExtra=%RU64, fNotify=%RTbool, rcCallback=%Rrc, rc=%Rrc\n",
                 pCtx->cbProcessed, pCtx->cbExtra, fNotify, rcCallback, rc));

    if (fNotify)
    {
        int rc2 = pCtx->EventCallback.Notify(rcCallback);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc; /* Tell the guest. */
}

/* static */
DECLCALLBACK(int) GuestDnDSource::i_receiveTransferDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    GuestDnDRecvCtx *pCtx = (GuestDnDRecvCtx *)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDSource *pThis = pCtx->pSource;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("pThis=%p, uMsg=%RU32\n", pThis, uMsg));

    int rc = VINF_SUCCESS;

    int rcCallback = VINF_SUCCESS; /* rc for the callback. */
    bool fNotify = false;

    switch (uMsg)
    {
        case GUEST_DND_CONNECT:
            /* Nothing to do here (yet). */
            break;

        case GUEST_DND_DISCONNECT:
            rc = VERR_CANCELLED;
            break;

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case GUEST_DND_GH_SND_DATA_HDR:
        {
            PVBOXDNDCBSNDDATAHDRDATA pCBData = reinterpret_cast<PVBOXDNDCBSNDDATAHDRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDDATAHDRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_DATA_HDR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveDataHdr(pCtx, &pCBData->data);
            break;
        }
        case GUEST_DND_GH_SND_DATA:
        {
            PVBOXDNDCBSNDDATADATA pCBData = reinterpret_cast<PVBOXDNDCBSNDDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_DATA == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveData(pCtx, &pCBData->data);
            break;
        }
        case GUEST_DND_GH_SND_DIR:
        {
            PVBOXDNDCBSNDDIRDATA pCBData = reinterpret_cast<PVBOXDNDCBSNDDIRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDDIRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_DIR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveDir(pCtx, pCBData->pszPath, pCBData->cbPath, pCBData->fMode);
            break;
        }
        case GUEST_DND_GH_SND_FILE_HDR:
        {
            PVBOXDNDCBSNDFILEHDRDATA pCBData = reinterpret_cast<PVBOXDNDCBSNDFILEHDRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDFILEHDRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_FILE_HDR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            rc = pThis->i_onReceiveFileHdr(pCtx, pCBData->pszFilePath, pCBData->cbFilePath,
                                           pCBData->cbSize, pCBData->fMode, pCBData->fFlags);
            break;
        }
        case GUEST_DND_GH_SND_FILE_DATA:
        {
            PVBOXDNDCBSNDFILEDATADATA pCBData = reinterpret_cast<PVBOXDNDCBSNDFILEDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBSNDFILEDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_SND_FILE_DATA == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            if (pThis->m_DataBase.uProtocolVersion <= 1)
            {
                /**
                 * Notes for protocol v1 (< VBox 5.0):
                 * - Every time this command is being sent it includes the file header,
                 *   so just process both calls here.
                 * - There was no information whatsoever about the total file size; the old code only
                 *   appended data to the desired file. So just pass 0 as cbSize.
                 */
                rc = pThis->i_onReceiveFileHdr(pCtx, pCBData->u.v1.pszFilePath, pCBData->u.v1.cbFilePath,
                                               0 /* cbSize */, pCBData->u.v1.fMode, 0 /* fFlags */);
                if (RT_SUCCESS(rc))
                    rc = pThis->i_onReceiveFileData(pCtx, pCBData->pvData, pCBData->cbData);
            }
            else /* Protocol v2 and up. */
                rc = pThis->i_onReceiveFileData(pCtx, pCBData->pvData, pCBData->cbData);
            break;
        }
        case GUEST_DND_GH_EVT_ERROR:
        {
            PVBOXDNDCBEVTERRORDATA pCBData = reinterpret_cast<PVBOXDNDCBEVTERRORDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBEVTERRORDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_EVT_ERROR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            pCtx->pResp->reset();

            if (RT_SUCCESS(pCBData->rc))
            {
                AssertMsgFailed(("Received guest error with no error code set\n"));
                pCBData->rc = VERR_GENERAL_FAILURE; /* Make sure some error is set. */
            }
            else if (pCBData->rc == VERR_WRONG_ORDER)
            {
                rc = pCtx->pResp->setProgress(100, DND_PROGRESS_CANCELLED);
            }
            else
                rc = pCtx->pResp->setProgress(100, DND_PROGRESS_ERROR, pCBData->rc,
                                              GuestDnDSource::i_guestErrorToString(pCBData->rc));

            LogRel3(("DnD: Guest reported file transfer error: %Rrc\n", pCBData->rc));

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
                LogRel2(("DnD: File transfer to host complete\n"));
                break;

            case VERR_CANCELLED:
                LogRel2(("DnD: File transfer to host canceled\n"));
                break;

            default:
                LogRel(("DnD: Error %Rrc occurred, aborting file transfer to host\n", rc));
                break;
        }

        /* Unregister this callback. */
        AssertPtr(pCtx->pResp);
        int rc2 = pCtx->pResp->setCallback(uMsg, NULL /* PFNGUESTDNDCALLBACK */);
        AssertRC(rc2);
    }

    /* All data processed? */
    if (   pCtx->Transfer.isComplete()
        && pCtx->isComplete())
    {
        fNotify = true;
    }

    LogFlowFunc(("cbProcessed=%RU64, cbExtra=%RU64, fNotify=%RTbool, rcCallback=%Rrc, rc=%Rrc\n",
                 pCtx->cbProcessed, pCtx->cbExtra, fNotify, rcCallback, rc));

    if (fNotify)
    {
        int rc2 = pCtx->EventCallback.Notify(rcCallback);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc; /* Tell the guest. */
}

