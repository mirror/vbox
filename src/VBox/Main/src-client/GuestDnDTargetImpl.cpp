/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop target.
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
#include "GuestDnDTargetImpl.h"
#include "ConsoleImpl.h"

#include "Global.h"
#include "AutoCaller.h"

#include <algorithm>        /* For std::find(). */

#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/uri.h>
#include <iprt/cpp/utils.h> /* For unconst(). */

#include <VBox/com/array.h>

#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/Service.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


/**
 * Base class for a target task.
 */
class GuestDnDTargetTask
{
public:

    GuestDnDTargetTask(GuestDnDTarget *pTarget)
        : mTarget(pTarget),
          mRC(VINF_SUCCESS) { }

    virtual ~GuestDnDTargetTask(void) { }

    int getRC(void) const { return mRC; }
    bool isOk(void) const { return RT_SUCCESS(mRC); }
    const ComObjPtr<GuestDnDTarget> &getTarget(void) const { return mTarget; }

protected:

    const ComObjPtr<GuestDnDTarget>     mTarget;
    int                                 mRC;
};

/**
 * Task structure for sending data to a target using
 * a worker thread.
 */
class SendDataTask : public GuestDnDTargetTask
{
public:

    SendDataTask(GuestDnDTarget *pTarget, PSENDDATACTX pCtx)
        : GuestDnDTargetTask(pTarget),
          mpCtx(pCtx) { }

    virtual ~SendDataTask(void)
    {
        if (mpCtx)
        {
            delete mpCtx;
            mpCtx = NULL;
        }
    }


    PSENDDATACTX getCtx(void) { return mpCtx; }

protected:

    /** Pointer to send data context. */
    PSENDDATACTX mpCtx;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDnDTarget)

HRESULT GuestDnDTarget::FinalConstruct(void)
{
    /* Set the maximum block size our guests can handle to 64K. This always has
     * been hardcoded until now. */
    /* Note: Never ever rely on information from the guest; the host dictates what and
     *       how to do something, so try to negogiate a sensible value here later. */
    mData.mcbBlockSize = _64K; /** @todo Make this configurable. */

    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestDnDTarget::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestDnDTarget::init(const ComObjPtr<Guest>& pGuest)
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
void GuestDnDTarget::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

// implementation of wrapped IDnDBase methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDTarget::isFormatSupported(const com::Utf8Str &aFormat, BOOL *aSupported)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_isFormatSupported(aFormat, aSupported);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::getFormats(GuestDnDMIMEList &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_getFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::addFormats(const GuestDnDMIMEList &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_addFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::removeFormats(const GuestDnDMIMEList &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_removeFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::getProtocolVersion(ULONG *aProtocolVersion)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_getProtocolVersion(aProtocolVersion);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

// implementation of wrapped IDnDTarget methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDTarget::enter(ULONG aScreenId, ULONG aX, ULONG aY,
                              DnDAction_T                      aDefaultAction,
                              const std::vector<DnDAction_T>  &aAllowedActions,
                              const GuestDnDMIMEList           &aFormats,
                              DnDAction_T                     *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */
    if (aDefaultAction == DnDAction_Ignore)
        return setError(E_INVALIDARG, tr("No default action specified"));
    if (!aAllowedActions.size())
        return setError(E_INVALIDARG, tr("Number of allowed actions is empty"));
    if (!aFormats.size())
        return setError(E_INVALIDARG, tr("Number of supported formats is empty"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Determine guest DnD protocol to use. */
    GuestDnDBase::getProtocolVersion(&mDataBase.mProtocolVersion);

    /* Default action is ignoring. */
    DnDAction_T resAction = DnDAction_Ignore;

    /* Check & convert the drag & drop actions */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &uDefAction,
                            aAllowedActions, &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /*
     * Make a flat data string out of the supported format list.
     * In the GuestDnDTarget case the source formats are from the host,
     * as GuestDnDTarget acts as a source for the guest.
     */
    Utf8Str strFormats = GuestDnD::toFormatString(GuestDnD::toFilteredFormatList(m_lstFmtSupported, aFormats));
    if (strFormats.isEmpty())
        return setError(E_INVALIDARG, tr("No or not supported format(s) specified"));

    LogRel2(("DnD: Offered formats to guest:\n"));
    RTCList<RTCString> lstFormats = strFormats.split("\r\n");
    for (size_t i = 0; i < lstFormats.size(); i++)
        LogRel2(("DnD: \t%s\n", lstFormats[i].c_str()));

    /* Save the formats offered to the guest. This is needed to later
     * decide what to do with the data when sending stuff to the guest. */
    m_lstFmtOffered = aFormats;
    Assert(m_lstFmtOffered.size());

    HRESULT hr = S_OK;

    /* Adjust the coordinates in a multi-monitor setup. */
    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_HG_EVT_ENTER);
        Msg.setNextUInt32(aScreenId);
        Msg.setNextUInt32(aX);
        Msg.setNextUInt32(aY);
        Msg.setNextUInt32(uDefAction);
        Msg.setNextUInt32(uAllowedActions);
        Msg.setNextPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        Msg.setNextUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->defAction());
        }
    }

    if (RT_FAILURE(rc))
        hr = VBOX_E_IPRT_ERROR;

    if (SUCCEEDED(hr))
    {
        if (aResultAction)
            *aResultAction = resAction;
    }

    LogFlowFunc(("hr=%Rhrc, resAction=%ld\n", hr, resAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::move(ULONG aScreenId, ULONG aX, ULONG aY,
                             DnDAction_T                      aDefaultAction,
                             const std::vector<DnDAction_T>  &aAllowedActions,
                             const GuestDnDMIMEList          &aFormats,
                             DnDAction_T                     *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Default action is ignoring. */
    DnDAction_T resAction = DnDAction_Ignore;

    /* Check & convert the drag & drop actions. */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &uDefAction,
                            aAllowedActions, &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /*
     * Make a flat data string out of the supported format list.
     * In the GuestDnDTarget case the source formats are from the host,
     * as GuestDnDTarget acts as a source for the guest.
     */
    Utf8Str strFormats = GuestDnD::toFormatString(GuestDnD::toFilteredFormatList(m_lstFmtSupported, aFormats));
    if (strFormats.isEmpty())
        return setError(E_INVALIDARG, tr("No or not supported format(s) specified"));

    HRESULT hr = S_OK;

    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_HG_EVT_MOVE);
        Msg.setNextUInt32(aScreenId);
        Msg.setNextUInt32(aX);
        Msg.setNextUInt32(aY);
        Msg.setNextUInt32(uDefAction);
        Msg.setNextUInt32(uAllowedActions);
        Msg.setNextPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        Msg.setNextUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->defAction());
        }
    }

    if (RT_FAILURE(rc))
        hr = VBOX_E_IPRT_ERROR;

    if (SUCCEEDED(hr))
    {
        if (aResultAction)
            *aResultAction = resAction;
    }

    LogFlowFunc(("hr=%Rhrc, *pResultAction=%ld\n", hr, resAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::leave(ULONG uScreenId)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;
    int rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_LEAVE,
                                      0 /* cParms */, NULL /* paParms */);
    if (RT_SUCCESS(rc))
    {
        GuestDnDResponse *pResp = GuestDnDInst()->response();
        if (pResp)
            pResp->waitForGuestResponse();
    }

    if (RT_FAILURE(rc))
        hr = VBOX_E_IPRT_ERROR;

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::drop(ULONG aScreenId, ULONG aX, ULONG aY,
                             DnDAction_T                      aDefaultAction,
                             const std::vector<DnDAction_T>  &aAllowedActions,
                             const GuestDnDMIMEList          &aFormats,
                             com::Utf8Str                    &aFormat,
                             DnDAction_T                     *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    if (aDefaultAction == DnDAction_Ignore)
        return setError(E_INVALIDARG, tr("Invalid default action specified"));
    if (!aAllowedActions.size())
        return setError(E_INVALIDARG, tr("Invalid allowed actions specified"));
    if (!aFormats.size())
        return setError(E_INVALIDARG, tr("No drop format(s) specified"));
    /* aResultAction is optional. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Default action is ignoring. */
    DnDAction_T resAction    = DnDAction_Ignore;

    /* Check & convert the drag & drop actions to HGCM codes. */
    uint32_t uDefAction      = DND_IGNORE_ACTION;
    uint32_t uAllowedActions = 0;
    GuestDnD::toHGCMActions(aDefaultAction,  &uDefAction,
                            aAllowedActions, &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
    {
        aFormat = "";
        if (aResultAction)
            *aResultAction = DnDAction_Ignore;
        return S_OK;
    }

    /*
     * Make a flat data string out of the supported format list.
     * In the GuestDnDTarget case the source formats are from the host,
     * as GuestDnDTarget acts as a source for the guest.
     */
    Utf8Str strFormats = GuestDnD::toFormatString(GuestDnD::toFilteredFormatList(m_lstFmtSupported, aFormats));
    if (strFormats.isEmpty())
        return setError(E_INVALIDARG, tr("No or not supported format(s) specified"));

    /* Adjust the coordinates in a multi-monitor setup. */
    HRESULT hr = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (SUCCEEDED(hr))
    {
        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_HG_EVT_DROPPED);
        Msg.setNextUInt32(aScreenId);
        Msg.setNextUInt32(aX);
        Msg.setNextUInt32(aY);
        Msg.setNextUInt32(uDefAction);
        Msg.setNextUInt32(uAllowedActions);
        Msg.setNextPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        Msg.setNextUInt32(strFormats.length() + 1);

        int rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            AssertPtr(pResp);

            rc = pResp->waitForGuestResponse();
            if (RT_SUCCESS(rc))
            {
                resAction = GuestDnD::toMainAction(pResp->defAction());

                GuestDnDMIMEList lstFormats = pResp->formats();
                if (lstFormats.size() == 1) /* Exactly one format to use specified? */
                {
                    aFormat = lstFormats.at(0);
                    LogFlowFunc(("resFormat=%s, resAction=%RU32\n", aFormat.c_str(), pResp->defAction()));
                }
                else
                    hr = setError(VBOX_E_IPRT_ERROR, tr("Guest returned invalid drop formats (%zu formats)"), lstFormats.size());
            }
            else
                hr = setError(VBOX_E_IPRT_ERROR, tr("Waiting for response of dropped event failed (%Rrc)"), rc);
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR, tr("Sending dropped event to guest failed (%Rrc)"), rc);
    }
    else
        hr = setError(hr, tr("Retrieving drop coordinates failed"));

    if (SUCCEEDED(hr))
    {
        if (aResultAction)
            *aResultAction = resAction;
    }

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

/* static */
DECLCALLBACK(int) GuestDnDTarget::i_sendDataThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFunc(("pvUser=%p\n", pvUser));

    SendDataTask *pTask = (SendDataTask *)pvUser;
    AssertPtrReturn(pTask, VERR_INVALID_POINTER);

    const ComObjPtr<GuestDnDTarget> pTarget(pTask->getTarget());
    Assert(!pTarget.isNull());

    int rc;

    AutoCaller autoCaller(pTarget);
    if (SUCCEEDED(autoCaller.rc()))
    {
        rc = pTarget->i_sendData(pTask->getCtx(), RT_INDEFINITE_WAIT /* msTimeout */);
        /* Nothing to do here anymore. */
    }
    else
        rc = VERR_COM_INVALID_OBJECT_STATE;

    ASMAtomicWriteBool(&pTarget->mDataBase.mfTransferIsPending, false);

    if (pTask)
        delete pTask;

    LogFlowFunc(("pTarget=%p returning rc=%Rrc\n", (GuestDnDTarget *)pTarget, rc));
    return rc;
}

/**
 * Initiates a data transfer from the host to the guest. The source is the host whereas the target is the
 * guest in this case.
 *
 * @return  HRESULT
 * @param   aScreenId
 * @param   aFormat
 * @param   aData
 * @param   aProgress
 */
HRESULT GuestDnDTarget::sendData(ULONG aScreenId, const com::Utf8Str &aFormat, const std::vector<BYTE> &aData,
                                 ComPtr<IProgress> &aProgress)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Input validation. */
    if (RT_UNLIKELY((aFormat.c_str()) == NULL || *(aFormat.c_str()) == '\0'))
        return setError(E_INVALIDARG, tr("No data format specified"));
    if (RT_UNLIKELY(!aData.size()))
        return setError(E_INVALIDARG, tr("No data to send specified"));

    /* Note: At the moment we only support one transfer at a time. */
    if (ASMAtomicReadBool(&mDataBase.mfTransferIsPending))
        return setError(E_INVALIDARG, tr("Another send operation already is in progress"));

    ASMAtomicWriteBool(&mDataBase.mfTransferIsPending, true);

    /* Dito. */
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    AssertPtr(pResp);

    HRESULT hr = pResp->resetProgress(m_pGuest);
    if (FAILED(hr))
        return hr;

    try
    {
        PSENDDATACTX pSendCtx = new SENDDATACTX;
        RT_BZERO(pSendCtx, sizeof(SENDDATACTX));

        pSendCtx->mpTarget      = this;
        pSendCtx->mpResp        = pResp;
        pSendCtx->mScreenID     = aScreenId;
        pSendCtx->mFmtReq       = aFormat;
        pSendCtx->mData.vecData = aData;

        SendDataTask *pTask = new SendDataTask(this, pSendCtx);
        AssertReturn(pTask->isOk(), pTask->getRC());

        LogFlowFunc(("Starting thread ...\n"));

        int rc = RTThreadCreate(NULL, GuestDnDTarget::i_sendDataThread,
                                (void *)pTask, 0, RTTHREADTYPE_MAIN_WORKER, 0, "dndTgtSndData");
        if (RT_SUCCESS(rc))
        {
            hr = pResp->queryProgressTo(aProgress.asOutParam());
            ComAssertComRC(hr);

            /* Note: pTask is now owned by the worker thread. */
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR, tr("Starting thread failed (%Rrc)"), rc);

        if (RT_FAILURE(rc))
            delete pSendCtx;
    }
    catch(std::bad_alloc &)
    {
        hr = setError(E_OUTOFMEMORY);
    }

    /* Note: mDataBase.mfTransferIsPending will be set to false again by i_sendDataThread. */

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

int GuestDnDTarget::i_cancelOperation(void)
{
    /** @todo Check for pending cancel requests. */

#if 0 /** @todo Later. */
    /* Cancel any outstanding waits for guest responses first. */
    if (pResp)
        pResp->notifyAboutGuestResponse();
#endif

    LogFlowFunc(("Cancelling operation, telling guest ...\n"));
    return GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_CANCEL, 0 /* cParms */, NULL /*paParms*/);
}

/* static */
Utf8Str GuestDnDTarget::i_guestErrorToString(int guestRc)
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
            strError += Utf8StrFmt(tr("The guest was not able to process the drag and drop data within time"));
            break;

        default:
            strError += Utf8StrFmt(tr("Drag and drop error from guest (%Rrc)"), guestRc);
            break;
    }

    return strError;
}

/* static */
Utf8Str GuestDnDTarget::i_hostErrorToString(int hostRc)
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

int GuestDnDTarget::i_sendData(PSENDDATACTX pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc;

    ASMAtomicWriteBool(&pCtx->mIsActive, true);

    /* Clear all remaining outgoing messages. */
    mDataBase.mListOutgoing.clear();

    /**
     * Do we need to build up a file tree?
     * Note: The decision whether we need to build up a file tree and sending
     *       actual file data only depends on the actual formats offered by this target.
     *       If the guest does not want an URI list ("text/uri-list") but text ("TEXT" and
     *       friends) instead, still send the data over to the guest -- the file as such still
     *       is needed on the guest in this case, as the guest then just wants a simple path
     *       instead of an URI list (pointing to a file on the guest itself).
     *
     ** @todo Support more than one format; add a format<->function handler concept. Later. */
    bool fHasURIList = std::find(m_lstFmtOffered.begin(),
                                 m_lstFmtOffered.end(), "text/uri-list") != m_lstFmtOffered.end();
    if (fHasURIList)
    {
        rc = i_sendURIData(pCtx, msTimeout);
    }
    else
    {
        rc = i_sendRawData(pCtx, msTimeout);
    }

    ASMAtomicWriteBool(&pCtx->mIsActive, false);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendDirectory(PSENDDATACTX pCtx, GuestDnDURIObjCtx *pObjCtx, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg,    VERR_INVALID_POINTER);

    DnDURIObject *pObj = pObjCtx->pObjURI;
    AssertPtr(pObj);

    RTCString strPath = pObj->GetDestPath();
    if (strPath.isEmpty())
        return VERR_INVALID_PARAMETER;
    if (strPath.length() >= RTPATH_MAX) /* Note: Maximum is RTPATH_MAX on guest side. */
        return VERR_BUFFER_OVERFLOW;

    LogFlowFunc(("Sending directory \"%s\" using protocol v%RU32 ...\n", strPath.c_str(), mDataBase.mProtocolVersion));

    pMsg->setType(DragAndDropSvc::HOST_DND_HG_SND_DIR);
    pMsg->setNextString(strPath.c_str());                  /* path */
    pMsg->setNextUInt32((uint32_t)(strPath.length() + 1)); /* path length - note: Maximum is RTPATH_MAX on guest side. */
    pMsg->setNextUInt32(pObj->GetMode());                  /* mode */

    return VINF_SUCCESS;
}

int GuestDnDTarget::i_sendFile(PSENDDATACTX pCtx, GuestDnDURIObjCtx *pObjCtx, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg,    VERR_INVALID_POINTER);

    DnDURIObject *pObj = pObjCtx->pObjURI;
    AssertPtr(pObj);

    RTCString strPathSrc = pObj->GetSourcePath();
    if (strPathSrc.isEmpty())
        return VERR_INVALID_PARAMETER;

    int rc = VINF_SUCCESS;

    LogFlowFunc(("Sending file with %RU32 bytes buffer, using protocol v%RU32 ...\n",
                  mData.mcbBlockSize, mDataBase.mProtocolVersion));
    LogFlowFunc(("strPathSrc=%s, fIsOpen=%RTbool, cbSize=%RU64\n", strPathSrc.c_str(), pObj->IsOpen(), pObj->GetSize()));

    if (!pObj->IsOpen())
    {
        LogRel2(("DnD: Opening host file for transferring to guest: %s\n", strPathSrc.c_str()));
        rc = pObj->OpenEx(strPathSrc, DnDURIObject::File, DnDURIObject::Source,
                          RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, 0 /* fFlags */);
        if (RT_FAILURE(rc))
            LogRel(("DnD: Error opening host file '%s', rc=%Rrc\n", strPathSrc.c_str(), rc));
    }

    bool fSendData = false;
    if (RT_SUCCESS(rc))
    {
        if (mDataBase.mProtocolVersion >= 2)
        {
            if (!pObjCtx->fHeaderSent)
            {
                /*
                 * Since protocol v2 the file header and the actual file contents are
                 * separate messages, so send the file header first.
                 * The just registered callback will be called by the guest afterwards.
                 */
                pMsg->setType(DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR);
                pMsg->setNextUInt32(0);                                            /* uContextID */
                rc = pMsg->setNextString(pObj->GetDestPath().c_str());             /* pvName */
                AssertRC(rc);
                pMsg->setNextUInt32((uint32_t)(pObj->GetDestPath().length() + 1)); /* cbName */
                pMsg->setNextUInt32(0);                                            /* uFlags */
                pMsg->setNextUInt32(pObj->GetMode());                              /* fMode */
                pMsg->setNextUInt64(pObj->GetSize());                              /* uSize */

                LogFlowFunc(("Sending file header ...\n"));
                LogRel2(("DnD: Transferring host file to guest: %s (%RU64 bytes, mode 0x%x)\n",
                         strPathSrc.c_str(), pObj->GetSize(), pObj->GetMode()));

                /** @todo Set progress object title to current file being transferred? */

                pObjCtx->fHeaderSent = true;
            }
            else
            {
                /* File header was sent, so only send the actual file data. */
                fSendData = true;
            }
        }
        else /* Protocol v1. */
        {
            /* Always send the file data, every time. */
            fSendData = true;
        }
    }

    if (   RT_SUCCESS(rc)
        && fSendData)
    {
        rc = i_sendFileData(pCtx, pObjCtx, pMsg);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendFileData(PSENDDATACTX pCtx, GuestDnDURIObjCtx *pObjCtx, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg,    VERR_INVALID_POINTER);

    DnDURIObject *pObj = pObjCtx->pObjURI;
    AssertPtr(pObj);

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pResp);

    /** @todo Don't allow concurrent reads per context! */

    /*
     * Start sending stuff.
     */

    /* Set the message type. */
    pMsg->setType(DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA);

    /* Protocol version 1 sends the file path *every* time with a new file chunk.
     * In protocol version 2 we only do this once with HOST_DND_HG_SND_FILE_HDR. */
    if (mDataBase.mProtocolVersion <= 1)
    {
        pMsg->setNextString(pObj->GetDestPath().c_str());                  /* pvName */
        pMsg->setNextUInt32((uint32_t)(pObj->GetDestPath().length() + 1)); /* cbName */
    }
    else
    {
        /* Protocol version 2 also sends the context ID. Currently unused. */
        pMsg->setNextUInt32(0);                                            /* context ID */
    }

    uint32_t cbRead = 0;

    int rc = pObj->Read(pCtx->mURI.GetBufferMutable(), pCtx->mURI.GetBufferSize(), &cbRead);
    if (RT_SUCCESS(rc))
    {
        pCtx->mData.cbProcessed += cbRead;
        LogFlowFunc(("cbBufSize=%zu, cbRead=%RU32, cbProcessed=%RU64, rc=%Rrc\n",
                     pCtx->mURI.GetBufferSize(), cbRead, pCtx->mData.cbProcessed, rc));

        if (mDataBase.mProtocolVersion <= 1)
        {
            pMsg->setNextPointer(pCtx->mURI.GetBufferMutable(), cbRead);   /* pvData */
            pMsg->setNextUInt32(cbRead);                                   /* cbData */
            pMsg->setNextUInt32(pObj->GetMode());                          /* fMode */
        }
        else
        {
            pMsg->setNextPointer(pCtx->mURI.GetBufferMutable(), cbRead);   /* pvData */
            pMsg->setNextUInt32(cbRead);                                   /* cbData */
        }

        if (pObj->IsComplete()) /* Done reading? */
        {
            LogRel2(("DnD: File transfer to guest complete: %s\n", pObj->GetSourcePath().c_str()));
            LogFlowFunc(("File '%s' complete\n", pObj->GetSourcePath().c_str()));

            /* DnDURIObject::Read() returns VINF_EOF when finished reading the entire fire,
             * but we don't want this here -- so just override this with VINF_SUCCESS. */
            rc = VINF_SUCCESS;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
DECLCALLBACK(int) GuestDnDTarget::i_sendURIDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    PSENDDATACTX pCtx = (PSENDDATACTX)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDTarget *pThis = pCtx->mpTarget;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("pThis=%p, uMsg=%RU32\n", pThis, uMsg));

    int rc      = VINF_SUCCESS; /* Will be reported back to guest. */
    int rcGuest;                /* Contains error code from guest in case of VERR_GSTDND_GUEST_ERROR. */
    bool fNotify = false;

    switch (uMsg)
    {
        case DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG:
        {
            DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSG pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSG>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGGETNEXTHOSTMSG) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            try
            {
                GuestDnDMsg *pMsg = new GuestDnDMsg();

                rc = pThis->i_sendURIDataLoop(pCtx, pMsg);
                if (rc == VINF_EOF) /* Transfer complete? */
                {
                    LogFlowFunc(("Last URI item processed, bailing out\n"));
                }
                else if (RT_SUCCESS(rc))
                {
                    rc = pThis->msgQueueAdd(pMsg);
                    if (RT_SUCCESS(rc)) /* Return message type & required parameter count to the guest. */
                    {
                        LogFlowFunc(("GUEST_DND_GET_NEXT_HOST_MSG -> %RU32 (%RU32 params)\n", pMsg->getType(), pMsg->getCount()));
                        pCBData->uMsg   = pMsg->getType();
                        pCBData->cParms = pMsg->getCount();
                    }
                }

                if (   RT_FAILURE(rc)
                    || rc == VINF_EOF) /* Transfer complete? */
                {
                    delete pMsg;
                    pMsg = NULL;
                }
            }
            catch(std::bad_alloc & /*e*/)
            {
                rc = VERR_NO_MEMORY;
            }
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
            {
                AssertMsgFailed(("Guest has sent an error event but did not specify an actual error code\n"));
                pCBData->rc = VERR_GENERAL_FAILURE; /* Make sure some error is set. */
            }

            rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, pCBData->rc,
                                           GuestDnDTarget::i_guestErrorToString(pCBData->rc));
            if (RT_SUCCESS(rc))
            {
                rc      = VERR_GSTDND_GUEST_ERROR;
                rcGuest = pCBData->rc;
            }
            break;
        }
        case DragAndDropSvc::HOST_DND_HG_SND_DIR:
        case DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR:
        case DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSGDATA pCBData
                = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSGDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGGETNEXTHOSTMSGDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            LogFlowFunc(("pCBData->uMsg=%RU32, paParms=%p, cParms=%RU32\n", pCBData->uMsg, pCBData->paParms, pCBData->cParms));

            GuestDnDMsg *pMsg = pThis->msgQueueGetNext();
            if (pMsg)
            {
                /*
                 * Sanity checks.
                 */
                if (   pCBData->uMsg    != uMsg
                    || pCBData->paParms == NULL
                    || pCBData->cParms  != pMsg->getCount())
                {
                    /* Start over. */
                    pThis->msgQueueClear();

                    rc = VERR_INVALID_PARAMETER;
                }

                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("Returning uMsg=%RU32\n", uMsg));
                    rc = HGCM::Message::copyParms(pMsg->getCount(), pMsg->getParms(), pCBData->paParms);
                    if (RT_SUCCESS(rc))
                    {
                        pCBData->cParms = pMsg->getCount();
                        pThis->msgQueueRemoveNext();
                    }
                    else
                        LogFlowFunc(("Copying parameters failed with rc=%Rrc\n", rc));
                }
            }
            else
                rc = VERR_NO_DATA;

            LogFlowFunc(("Processing next message ended with rc=%Rrc\n", rc));
            break;
        }
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    int rcToGuest = VINF_SUCCESS; /* Status which will be sent back to the guest. */

    /*
     * Resolve errors.
     */
    switch (rc)
    {
        case VINF_SUCCESS:
            break;

        case VINF_EOF:
        {
            LogRel2(("DnD: Transfer to guest complete\n"));

            /* Complete operation on host side. */
            fNotify = true;

            /* The guest expects VERR_NO_DATA if the transfer is complete. */
            rcToGuest = VERR_NO_DATA;
            break;
        }

        case VERR_GSTDND_GUEST_ERROR:
        {
            LogRel(("DnD: Guest reported error %Rrc, aborting transfer to guest\n", rcGuest));
            break;
        }

        case VERR_CANCELLED:
        {
            LogRel2(("DnD: Transfer to guest canceled\n"));
            rcToGuest = VERR_CANCELLED; /* Also cancel on guest side. */
            break;
        }

        default:
        {
            LogRel(("DnD: Host error %Rrc occurred, aborting transfer to guest\n", rc));
            rcToGuest = VERR_CANCELLED; /* Also cancel on guest side. */
            break;
        }
    }

    if (RT_FAILURE(rc))
    {
        /* Unregister this callback. */
        AssertPtr(pCtx->mpResp);
        int rc2 = pCtx->mpResp->setCallback(uMsg, NULL /* PFNGUESTDNDCALLBACK */);
        AssertRC(rc2);

        /* Let the waiter(s) know. */
        fNotify = true;
    }

    LogFlowFunc(("fNotify=%RTbool, rc=%Rrc, rcToGuest=%Rrc\n", fNotify, rc, rcToGuest));

    if (fNotify)
    {
        int rc2 = pCtx->mCallback.Notify(rc); /** @todo Also pass guest error back? */
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rcToGuest; /* Tell the guest. */
}

int GuestDnDTarget::i_sendURIData(PSENDDATACTX pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtr(pCtx->mpResp);

#define URI_DATA_IS_VALID_BREAK(x) \
    if (!x) \
    { \
        LogFlowFunc(("Invalid URI data value for \"" #x "\"\n")); \
        rc = VERR_INVALID_PARAMETER; \
        break; \
    }

#define REGISTER_CALLBACK(x)                                        \
    rc = pCtx->mpResp->setCallback(x, i_sendURIDataCallback, pCtx); \
    if (RT_FAILURE(rc))                                             \
        return rc;

#define UNREGISTER_CALLBACK(x)                        \
    {                                                 \
        int rc2 = pCtx->mpResp->setCallback(x, NULL); \
        AssertRC(rc2);                                \
    }

    int rc = pCtx->mURI.Init(mData.mcbBlockSize);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCtx->mCallback.Reset();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register callbacks.
     */
    /* Guest callbacks. */
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG);
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    /* Host callbacks. */
    REGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_DIR);
    if (mDataBase.mProtocolVersion >= 2)
        REGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR);
    REGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA);

    do
    {
        /*
         * Extract URI list from byte data.
         */
        DnDURIList &lstURI = pCtx->mURI.lstURI; /* Use the URI list from the context. */

        const char *pszList = (const char *)&pCtx->mData.vecData.front();
        URI_DATA_IS_VALID_BREAK(pszList);

        uint32_t cbList = pCtx->mData.vecData.size();
        URI_DATA_IS_VALID_BREAK(cbList);

        RTCList<RTCString> lstURIOrg = RTCString(pszList, cbList).split("\r\n");
        URI_DATA_IS_VALID_BREAK(!lstURIOrg.isEmpty());

        /* Note: All files to be transferred will be kept open during the entire DnD
         *       operation, also to keep the accounting right. */
        rc = lstURI.AppendURIPathsFromList(lstURIOrg, DNDURILIST_FLAGS_KEEP_OPEN);
        if (RT_SUCCESS(rc))
            LogFlowFunc(("URI root objects: %zu, total bytes (raw data to transfer): %zu\n",
                         lstURI.RootCount(), lstURI.TotalBytes()));
        else
            break;

        pCtx->mData.cbProcessed = 0;
        pCtx->mData.cbToProcess = lstURI.TotalBytes();

        /*
         * The first message always is the meta info for the data. The meta
         * info *only* contains the root elements of an URI list.
         *
         * After the meta data we generate the messages required to send the data itself.
         */
        Assert(!lstURI.IsEmpty());
        RTCString strData = lstURI.RootToString().c_str();
        size_t    cbData  = strData.length() + 1; /* Include terminating zero. */

        GuestDnDMsg MsgSndData;
        MsgSndData.setType(DragAndDropSvc::HOST_DND_HG_SND_DATA);
        MsgSndData.setNextUInt32(pCtx->mScreenID);
        MsgSndData.setNextPointer((void *)pCtx->mFmtReq.c_str(), (uint32_t)pCtx->mFmtReq.length() + 1);
        MsgSndData.setNextUInt32((uint32_t)pCtx->mFmtReq.length() + 1);
        MsgSndData.setNextPointer((void*)strData.c_str(), (uint32_t)cbData);
        MsgSndData.setNextUInt32((uint32_t)cbData);

        rc = GuestDnDInst()->hostCall(MsgSndData.getType(), MsgSndData.getCount(), MsgSndData.getParms());
        if (RT_SUCCESS(rc))
        {
            rc = waitForEvent(msTimeout, pCtx->mCallback, pCtx->mpResp);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_CANCELLED)
                    rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_CANCELLED, VINF_SUCCESS);
                else if (rc != VERR_GSTDND_GUEST_ERROR) /* Guest-side error are already handled in the callback. */
                    rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, rc,
                                                   GuestDnDTarget::i_hostErrorToString(rc));
            }
            else
                rc = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_COMPLETE, VINF_SUCCESS);
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    /* Guest callbacks. */
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG);
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GH_EVT_ERROR);
    /* Host callbacks. */
    UNREGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_DIR);
    if (mDataBase.mProtocolVersion >= 2)
        UNREGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR);
    UNREGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    /*
     * Now that we've cleaned up tell the guest side to cancel.
     * This does not imply we're waiting for the guest to react, as the
     * host side never must depend on anything from the guest.
     */
    if (rc == VERR_CANCELLED)
    {
        int rc2 = sendCancel();
        AssertRC(rc2);
    }

#undef URI_DATA_IS_VALID_BREAK

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendURIDataLoop(PSENDDATACTX pCtx, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    DnDURIList &lstURI = pCtx->mURI.lstURI;

    uint64_t cbTotal = pCtx->mData.cbToProcess;
    uint8_t uPercent = pCtx->mData.cbProcessed * 100 / (cbTotal ? cbTotal : 1);

    LogFlowFunc(("%RU64 / %RU64 -- %RU8%%\n", pCtx->mData.cbProcessed, cbTotal, uPercent));

    bool fComplete = (uPercent >= 100) || lstURI.IsEmpty();

    if (pCtx->mpResp)
    {
        int rc2 = pCtx->mpResp->setProgress(uPercent,
                                              fComplete
                                            ? DragAndDropSvc::DND_PROGRESS_COMPLETE
                                            : DragAndDropSvc::DND_PROGRESS_RUNNING);
        AssertRC(rc2);
    }

    if (fComplete)
        return VINF_EOF;

    Assert(!lstURI.IsEmpty());
    DnDURIObject *pCurObj = lstURI.First();

    /* As we transfer all objects one after another at a time at the moment,
     * we only need one object context at the moment. */
    GuestDnDURIObjCtx *pObjCtx = &pCtx->mURI.objCtx;

    /* Assign the pointer of the current object to our context. */
    pObjCtx->pObjURI = pCurObj;

    uint32_t fMode = pCurObj->GetMode();
    LogFlowFunc(("Processing srcPath=%s, dstPath=%s, fMode=0x%x, cbSize=%RU32, fIsDir=%RTbool, fIsFile=%RTbool\n",
                 pCurObj->GetSourcePath().c_str(), pCurObj->GetDestPath().c_str(),
                 fMode, pCurObj->GetSize(),
                 RTFS_IS_DIRECTORY(fMode), RTFS_IS_FILE(fMode)));
    int rc;
    if (RTFS_IS_DIRECTORY(fMode))
    {
        rc = i_sendDirectory(pCtx, pObjCtx, pMsg);
    }
    else if (RTFS_IS_FILE(fMode))
    {
        rc = i_sendFile(pCtx, pObjCtx, pMsg);
    }
    else
    {
        AssertMsgFailed(("fMode=0x%x is not supported for srcPath=%s, dstPath=%s\n",
                         fMode, pCurObj->GetSourcePath().c_str(), pCurObj->GetDestPath().c_str()));
        rc = VERR_NOT_SUPPORTED;
    }

    bool fRemove = false; /* Remove current entry? */
    if (   pCurObj->IsComplete()
        || RT_FAILURE(rc))
    {
        fRemove = true;
    }

    if (fRemove)
    {
        LogFlowFunc(("Removing \"%s\" from list, rc=%Rrc\n", pCurObj->GetSourcePath().c_str(), rc));
        lstURI.RemoveFirst();
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendRawData(PSENDDATACTX pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    NOREF(msTimeout);

    GuestDnD *pInst = GuestDnDInst();
    AssertPtr(pInst);

    /** @todo At the moment we only allow sending up to 64K raw data. Fix this by
     *        using HOST_DND_HG_SND_MORE_DATA. */
    size_t cbDataTotal = pCtx->mData.vecData.size();
    if (   !cbDataTotal
        || cbDataTotal > _64K)
    {
        return VERR_INVALID_PARAMETER;
    }

    /* Just copy over the raw data. */
    GuestDnDMsg Msg;
    Msg.setType(DragAndDropSvc::HOST_DND_HG_SND_DATA);
    Msg.setNextUInt32(pCtx->mScreenID);
    Msg.setNextPointer((void *)pCtx->mFmtReq.c_str(), (uint32_t)pCtx->mFmtReq.length() + 1);
    Msg.setNextUInt32((uint32_t)pCtx->mFmtReq.length() + 1);
    Msg.setNextPointer((void*)&pCtx->mData.vecData.front(), (uint32_t)cbDataTotal);
    Msg.setNextUInt32(cbDataTotal);

    LogFlowFunc(("Transferring %zu total bytes of raw data ('%s')\n", cbDataTotal, pCtx->mFmtReq.c_str()));

    int rc2;

    int rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
    if (RT_FAILURE(rc))
        rc2 = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, rc,
                                        GuestDnDTarget::i_hostErrorToString(rc));
    else
        rc2 = pCtx->mpResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_COMPLETE, rc);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

HRESULT GuestDnDTarget::cancel(BOOL *aVeto)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    int rc = i_cancelOperation();

    if (aVeto)
        *aVeto = FALSE; /** @todo */

    HRESULT hr = RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

