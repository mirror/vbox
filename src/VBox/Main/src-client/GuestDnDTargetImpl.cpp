/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop target.
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
#define LOG_GROUP LOG_GROUP_GUEST_DND //LOG_GROUP_MAIN_GUESTDNDTARGET
#include "LoggingNew.h"

#include "GuestImpl.h"
#include "GuestDnDTargetImpl.h"
#include "ConsoleImpl.h"

#include "Global.h"
#include "AutoCaller.h"
#include "ThreadTask.h"

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


/**
 * Base class for a target task.
 */
class GuestDnDTargetTask : public ThreadTask
{
public:

    GuestDnDTargetTask(GuestDnDTarget *pTarget)
        : ThreadTask("GenericGuestDnDTargetTask")
        , mTarget(pTarget)
        , mRC(VINF_SUCCESS) { }

    virtual ~GuestDnDTargetTask(void) { }

    int getRC(void) const { return mRC; }
    bool isOk(void) const { return RT_SUCCESS(mRC); }

protected:

    const ComObjPtr<GuestDnDTarget>     mTarget;
    int                                 mRC;
};

/**
 * Task structure for sending data to a target using
 * a worker thread.
 */
class GuestDnDSendDataTask : public GuestDnDTargetTask
{
public:

    GuestDnDSendDataTask(GuestDnDTarget *pTarget, GuestDnDSendCtx *pCtx)
        : GuestDnDTargetTask(pTarget),
          mpCtx(pCtx)
    {
        m_strTaskName = "dndTgtSndData";
    }

    void handler()
    {
        const ComObjPtr<GuestDnDTarget> pThis(mTarget);
        Assert(!pThis.isNull());

        AutoCaller autoCaller(pThis);
        if (FAILED(autoCaller.rc()))
            return;

        int vrc = pThis->i_sendData(mpCtx, RT_INDEFINITE_WAIT /* msTimeout */);
        if (RT_FAILURE(vrc)) /* In case we missed some error handling within i_sendData(). */
        {
            if (vrc != VERR_CANCELLED)
                LogRel(("DnD: Sending data to guest failed with %Rrc\n", vrc));

            /* Make sure to fire a cancel request to the guest side in case something went wrong. */
            pThis->sendCancel();
        }
    }

    virtual ~GuestDnDSendDataTask(void) { }

protected:

    /** Pointer to send data context. */
    GuestDnDSendCtx *mpCtx;
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
    mData.mcbBlockSize = DND_DEFAULT_CHUNK_SIZE; /** @todo Make this configurable. */

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

    aFormats = GuestDnDBase::i_getFormats();

    return S_OK;
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
                              const GuestDnDMIMEList          &aFormats,
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
    GuestDnDBase::getProtocolVersion(&m_DataBase.uProtocolVersion);

    /* Default action is ignoring. */
    DnDAction_T resAction = DnDAction_Ignore;

    /* Check & convert the drag & drop actions. */
    VBOXDNDACTION     dndActionDefault     = 0;
    VBOXDNDACTIONLIST dndActionListAllowed = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &dndActionDefault,
                            aAllowedActions, &dndActionListAllowed);

    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(dndActionDefault))
        return S_OK;

    /*
     * Make a flat data string out of the supported format list.
     * In the GuestDnDTarget case the source formats are from the host,
     * as GuestDnDTarget acts as a source for the guest.
     */
    Utf8Str strFormats       = GuestDnD::toFormatString(GuestDnD::toFilteredFormatList(m_lstFmtSupported, aFormats));
    if (strFormats.isEmpty())
        return setError(E_INVALIDARG, tr("No or not supported format(s) specified"));
    const uint32_t cbFormats = (uint32_t)strFormats.length() + 1; /* Include terminating zero. */

    LogRel2(("DnD: Offered formats to guest:\n"));
    RTCList<RTCString> lstFormats = strFormats.split(DND_PATH_SEPARATOR);
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
        Msg.setType(HOST_DND_HG_EVT_ENTER);
        if (m_DataBase.uProtocolVersion >= 3)
            Msg.appendUInt32(0); /** @todo ContextID not used yet. */
        Msg.appendUInt32(aScreenId);
        Msg.appendUInt32(aX);
        Msg.appendUInt32(aY);
        Msg.appendUInt32(dndActionDefault);
        Msg.appendUInt32(dndActionListAllowed);
        Msg.appendPointer((void *)strFormats.c_str(), cbFormats);
        Msg.appendUInt32(cbFormats);

        rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->getActionDefault());
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
    VBOXDNDACTION     dndActionDefault     = 0;
    VBOXDNDACTIONLIST dndActionListAllowed = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &dndActionDefault,
                            aAllowedActions, &dndActionListAllowed);

    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(dndActionDefault))
        return S_OK;

    /*
     * Make a flat data string out of the supported format list.
     * In the GuestDnDTarget case the source formats are from the host,
     * as GuestDnDTarget acts as a source for the guest.
     */
    Utf8Str strFormats       = GuestDnD::toFormatString(GuestDnD::toFilteredFormatList(m_lstFmtSupported, aFormats));
    if (strFormats.isEmpty())
        return setError(E_INVALIDARG, tr("No or not supported format(s) specified"));
    const uint32_t cbFormats = (uint32_t)strFormats.length() + 1; /* Include terminating zero. */

    HRESULT hr = S_OK;

    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        GuestDnDMsg Msg;
        Msg.setType(HOST_DND_HG_EVT_MOVE);
        if (m_DataBase.uProtocolVersion >= 3)
            Msg.appendUInt32(0); /** @todo ContextID not used yet. */
        Msg.appendUInt32(aScreenId);
        Msg.appendUInt32(aX);
        Msg.appendUInt32(aY);
        Msg.appendUInt32(dndActionDefault);
        Msg.appendUInt32(dndActionListAllowed);
        Msg.appendPointer((void *)strFormats.c_str(), cbFormats);
        Msg.appendUInt32(cbFormats);

        rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->getActionDefault());
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
    RT_NOREF(uScreenId);
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GuestDnDMsg Msg;
    Msg.setType(HOST_DND_HG_EVT_LEAVE);
    if (m_DataBase.uProtocolVersion >= 3)
        Msg.appendUInt32(0); /** @todo ContextID not used yet. */

    int rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
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
    DnDAction_T resAct = DnDAction_Ignore;
    Utf8Str     resFmt;

    /* Check & convert the drag & drop actions to HGCM codes. */
    VBOXDNDACTION     dndActionDefault     = VBOX_DND_ACTION_IGNORE;
    VBOXDNDACTIONLIST dndActionListAllowed = 0;
    GuestDnD::toHGCMActions(aDefaultAction,  &dndActionDefault,
                            aAllowedActions, &dndActionListAllowed);

    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(dndActionDefault))
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
    Utf8Str strFormats       = GuestDnD::toFormatString(GuestDnD::toFilteredFormatList(m_lstFmtSupported, aFormats));
    if (strFormats.isEmpty())
        return setError(E_INVALIDARG, tr("No or not supported format(s) specified"));
    const uint32_t cbFormats = (uint32_t)strFormats.length() + 1; /* Include terminating zero. */

    /* Adjust the coordinates in a multi-monitor setup. */
    HRESULT hr = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (SUCCEEDED(hr))
    {
        GuestDnDMsg Msg;
        Msg.setType(HOST_DND_HG_EVT_DROPPED);
        if (m_DataBase.uProtocolVersion >= 3)
            Msg.appendUInt32(0); /** @todo ContextID not used yet. */
        Msg.appendUInt32(aScreenId);
        Msg.appendUInt32(aX);
        Msg.appendUInt32(aY);
        Msg.appendUInt32(dndActionDefault);
        Msg.appendUInt32(dndActionListAllowed);
        Msg.appendPointer((void*)strFormats.c_str(), cbFormats);
        Msg.appendUInt32(cbFormats);

        int vrc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(vrc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
            {
                resAct = GuestDnD::toMainAction(pResp->getActionDefault());

                GuestDnDMIMEList lstFormats = pResp->formats();
                if (lstFormats.size() == 1) /* Exactly one format to use specified? */
                {
                    resFmt = lstFormats.at(0);
                }
                else
                    /** @todo r=bird: This isn't an IPRT error, is it?   */
                    hr = setError(VBOX_E_IPRT_ERROR, tr("Guest returned invalid drop formats (%zu formats)"), lstFormats.size());
            }
            else
                hr = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Waiting for response of dropped event failed (%Rrc)"), vrc);
        }
        else
            hr = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Sending dropped event to guest failed (%Rrc)"), vrc);
    }
    else
        hr = setError(hr, tr("Retrieving drop coordinates failed"));

    LogFlowFunc(("resFmt=%s, resAct=%RU32, vrc=%Rhrc\n", resFmt.c_str(), resAct, hr));

    if (SUCCEEDED(hr))
    {
        aFormat = resFmt;
        if (aResultAction)
            *aResultAction = resAct;
    }

    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

/**
 * Initiates a data transfer from the host to the guest.
 *
 * The source is the host, whereas the target is the guest.
 *
 * @return  HRESULT
 * @param   aScreenId           Screen ID where this data transfer was initiated from.
 * @param   aFormat             Format of data to send. MIME-style.
 * @param   aData               Actual data to send.
 * @param   aProgress           Where to return the progress object on success.
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

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Check if this object still is in a pending state and bail out if so. */
    if (m_fIsPending)
        return setError(E_FAIL, tr("Current drop operation to guest still in progress"));

    /* Reset our internal state. */
    i_reset();

    /* At the moment we only support one transfer at a time. */
    if (GuestDnDInst()->getTargetCount())
        return setError(E_INVALIDARG, tr("Another drag and drop operation to the guest already is in progress"));

    /* Reset progress object. */
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    AssertPtr(pResp);
    HRESULT hr = pResp->resetProgress(m_pGuest);
    if (FAILED(hr))
        return hr;

    GuestDnDSendDataTask *pTask = NULL;

    try
    {
        mData.mSendCtx.reset();

        mData.mSendCtx.pTarget   = this;
        mData.mSendCtx.pResp     = pResp;
        mData.mSendCtx.uScreenID = aScreenId;

        mData.mSendCtx.Meta.strFmt = aFormat;
        mData.mSendCtx.Meta.add(aData);

        pTask = new GuestDnDSendDataTask(this, &mData.mSendCtx);
        if (!pTask->isOk())
        {
            delete pTask;
            LogRel(("DnD: Could not create SendDataTask object\n"));
            throw hr = E_FAIL;
        }

        /* This function delete pTask in case of exceptions,
         * so there is no need in the call of delete operator. */
        hr = pTask->createThreadWithType(RTTHREADTYPE_MAIN_WORKER);
        pTask = NULL; /* Note: pTask is now owned by the worker thread. */
    }
    catch (std::bad_alloc &)
    {
        hr = setError(E_OUTOFMEMORY);
    }
    catch (...)
    {
        LogRel(("DnD: Could not create thread for data sending task\n"));
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        /* Register ourselves at the DnD manager. */
        GuestDnDInst()->registerTarget(this);

        /* Return progress to caller. */
        hr = pResp->queryProgressTo(aProgress.asOutParam());
        ComAssertComRC(hr);
    }
    else
        hr = setError(hr, tr("Starting thread for GuestDnDTarget failed (%Rhrc)"), hr);

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
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

void GuestDnDTarget::i_reset(void)
{
    LogFlowThisFunc(("\n"));

    mData.mSendCtx.reset();

    m_fIsPending = false;

    /* Unregister ourselves from the DnD manager. */
    GuestDnDInst()->unregisterTarget(this);
}

/**
 * Main function for sending DnD host data to the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Send context to use.
 * @param   msTimeout           Timeout (in ms) to wait for getting the data sent.
 */
int GuestDnDTarget::i_sendData(GuestDnDSendCtx *pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    /* Don't allow receiving the actual data until our current transfer is complete. */
    if (m_fIsPending)
        return setError(E_FAIL, tr("Current drop operation to guest still in progress"));

    /* Clear all remaining outgoing messages. */
    m_DataBase.lstMsgOut.clear();

    /**
     * Do we need to build up a file tree?
     * Note: The decision whether we need to build up a file tree and sending
     *       actual file data only depends on the actual formats offered by this target.
     *       If the guest does not want a transfer list ("text/uri-list") but text ("TEXT" and
     *       friends) instead, still send the data over to the guest -- the file as such still
     *       is needed on the guest in this case, as the guest then just wants a simple path
     *       instead of a transfer list (pointing to a file on the guest itself).
     *
     ** @todo Support more than one format; add a format<->function handler concept. Later. */
    int rc;
    const bool fHasURIList = std::find(m_lstFmtOffered.begin(),
                                       m_lstFmtOffered.end(), "text/uri-list") != m_lstFmtOffered.end();
    if (fHasURIList)
    {
        rc = i_sendTransferData(pCtx, msTimeout);
    }
    else
    {
        rc = i_sendRawData(pCtx, msTimeout);
    }

    if (RT_FAILURE(rc))
        LogRel(("DnD: Sending data to guest failed with %Rrc\n", rc));

    /* Reset state. */
    i_reset();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sends the common meta data body to the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Send context to use.
 */
int GuestDnDTarget::i_sendMetaDataBody(GuestDnDSendCtx *pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    uint8_t *pvData = (uint8_t *)pCtx->Meta.pvData;
    size_t   cbData = pCtx->Meta.cbData;

    int rc = VINF_SUCCESS;

    const size_t  cbFmt   = pCtx->Meta.strFmt.length() + 1; /* Include terminator. */
    const char   *pcszFmt = pCtx->Meta.strFmt.c_str();

    LogFlowFunc(("uProto=%u, szFmt=%s, cbFmt=%RU32, cbData=%zu\n", m_DataBase.uProtocolVersion, pcszFmt, cbFmt, cbData));

    LogRel2(("DnD: Sending meta data to guest as '%s' (%zu bytes)\n", pcszFmt, cbData));

#ifdef DEBUG
    RTCList<RTCString> lstFilesURI = RTCString((char *)pvData, cbData).split(DND_PATH_SEPARATOR);
    LogFlowFunc(("lstFilesURI=%zu\n", lstFilesURI.size()));
    for (size_t i = 0; i < lstFilesURI.size(); i++)
        LogFlowFunc(("\t%s\n", lstFilesURI.at(i).c_str()));
#endif

    uint8_t *pvChunk = pvData;
    size_t   cbChunk = RT_MIN(mData.mcbBlockSize, cbData);
    while (cbData)
    {
        GuestDnDMsg Msg;
        Msg.setType(HOST_DND_HG_SND_DATA);

        if (m_DataBase.uProtocolVersion < 3)
        {
            Msg.appendUInt32(pCtx->uScreenID);                                 /* uScreenId */
            Msg.appendPointer(unconst(pcszFmt), (uint32_t)cbFmt);              /* pvFormat */
            Msg.appendUInt32((uint32_t)cbFmt);                                 /* cbFormat */
            Msg.appendPointer(pvChunk, (uint32_t)cbChunk);                     /* pvData */
            /* Fill in the current data block size to send.
             * Note: Only supports uint32_t. */
            Msg.appendUInt32((uint32_t)cbChunk);                               /* cbData */
        }
        else
        {
            Msg.appendUInt32(0); /** @todo ContextID not used yet. */
            Msg.appendPointer(pvChunk, (uint32_t)cbChunk);                     /* pvData */
            Msg.appendUInt32((uint32_t)cbChunk);                               /* cbData */
            Msg.appendPointer(NULL, 0);                                        /** @todo pvChecksum; not used yet. */
            Msg.appendUInt32(0);                                               /** @todo cbChecksum; not used yet. */
        }

        rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_FAILURE(rc))
            break;

        pvChunk += cbChunk;
        AssertBreakStmt(cbData >= cbChunk, VERR_BUFFER_UNDERFLOW);
        cbData  -= cbChunk;
    }

    if (RT_SUCCESS(rc))
    {
        rc = updateProgress(pCtx, pCtx->pResp, (uint32_t)pCtx->Meta.cbData);
        AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sends the common meta data header to the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Send context to use.
 */
int GuestDnDTarget::i_sendMetaDataHeader(GuestDnDSendCtx *pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    if (m_DataBase.uProtocolVersion < 3) /* Protocol < v3 did not support this, skip. */
        return VINF_SUCCESS;

    GuestDnDMsg Msg;
    Msg.setType(HOST_DND_HG_SND_DATA_HDR);

    LogRel2(("DnD: Sending meta data header to guest (%RU64 bytes total data, %RU32 bytes meta data, %RU64 objects)\n",
             pCtx->getTotalAnnounced(), pCtx->Meta.cbData, pCtx->Transfer.cObjToProcess));

    Msg.appendUInt32(0);                                                /** @todo uContext; not used yet. */
    Msg.appendUInt32(0);                                                /** @todo uFlags; not used yet. */
    Msg.appendUInt32(pCtx->uScreenID);                                  /* uScreen */
    Msg.appendUInt64(pCtx->getTotalAnnounced());                        /* cbTotal */
    Msg.appendUInt32((uint32_t)pCtx->Meta.cbData);                      /* cbMeta*/
    Msg.appendPointer(unconst(pCtx->Meta.strFmt.c_str()), (uint32_t)pCtx->Meta.strFmt.length() + 1); /* pvMetaFmt */
    Msg.appendUInt32((uint32_t)pCtx->Meta.strFmt.length() + 1);                                      /* cbMetaFmt */
    Msg.appendUInt64(pCtx->Transfer.cObjToProcess);                     /* cObjects */
    Msg.appendUInt32(0);                                                /** @todo enmCompression; not used yet. */
    Msg.appendUInt32(0);                                                /** @todo enmChecksumType; not used yet. */
    Msg.appendPointer(NULL, 0);                                         /** @todo pvChecksum; not used yet. */
    Msg.appendUInt32(0);                                                /** @todo cbChecksum; not used yet. */

    int rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendDirectory(GuestDnDSendCtx *pCtx, PDNDTRANSFEROBJECT pObj, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg, VERR_INVALID_POINTER);

    const char *pcszDstPath = DnDTransferObjectGetDestPath(pObj);
    AssertPtrReturn(pcszDstPath, VERR_INVALID_POINTER);
    const size_t cchPath = RTStrNLen(pcszDstPath, RTPATH_MAX); /* Note: Maximum is RTPATH_MAX on guest side. */
    AssertReturn(cchPath, VERR_INVALID_PARAMETER);

    LogRel2(("DnD: Transferring host directory '%s' to guest\n", DnDTransferObjectGetSourcePath(pObj)));

    pMsg->setType(HOST_DND_HG_SND_DIR);
    if (m_DataBase.uProtocolVersion >= 3)
        pMsg->appendUInt32(0); /** @todo ContextID not used yet. */
    pMsg->appendString(pcszDstPath);                    /* path */
    pMsg->appendUInt32((uint32_t)(cchPath + 1));        /* path length, including terminator. */
    pMsg->appendUInt32(DnDTransferObjectGetMode(pObj)); /* mode */

    return VINF_SUCCESS;
}

/**
 * Sends a transfer file to the guest.
 *
 * @returns VBox status code.
 * @param   pCtx
 * @param   pObj
 * @param   pMsg
 */
int GuestDnDTarget::i_sendFile(GuestDnDSendCtx *pCtx,
                               PDNDTRANSFEROBJECT pObj, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pObj, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg, VERR_INVALID_POINTER);

    const char *pcszSrcPath = DnDTransferObjectGetSourcePath(pObj);
    AssertPtrReturn(pcszSrcPath, VERR_INVALID_POINTER);
    const char *pcszDstPath = DnDTransferObjectGetDestPath(pObj);
    AssertPtrReturn(pcszDstPath, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    if (!DnDTransferObjectIsOpen(pObj))
    {
        LogRel2(("DnD: Opening host file '%s' for transferring to guest\n", pcszSrcPath));

        rc = DnDTransferObjectOpen(pObj, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, 0 /* fMode */,
                                   DNDTRANSFEROBJECT_FLAGS_NONE);
        if (RT_FAILURE(rc))
            LogRel(("DnD: Opening host file '%s' failed, rc=%Rrc\n", pcszSrcPath, rc));
    }

    if (RT_FAILURE(rc))
        return rc;

    bool fSendData = false;
    if (RT_SUCCESS(rc))
    {
        if (m_DataBase.uProtocolVersion >= 2)
        {
            if (!(pCtx->Transfer.fObjState & DND_OBJ_STATE_HAS_HDR))
            {
                const size_t  cchDstPath = RTStrNLen(pcszDstPath, RTPATH_MAX);
                const size_t  cbSize     = DnDTransferObjectGetSize(pObj);
                const RTFMODE fMode      = DnDTransferObjectGetMode(pObj);

                /*
                 * Since protocol v2 the file header and the actual file contents are
                 * separate messages, so send the file header first.
                 * The just registered callback will be called by the guest afterwards.
                 */
                pMsg->setType(HOST_DND_HG_SND_FILE_HDR);
                pMsg->appendUInt32(0); /** @todo ContextID not used yet. */
                pMsg->appendString(pcszDstPath);                    /* pvName */
                pMsg->appendUInt32((uint32_t)(cchDstPath + 1));     /* cbName */
                pMsg->appendUInt32(0);                              /* uFlags */
                pMsg->appendUInt32(fMode);                          /* fMode */
                pMsg->appendUInt64(cbSize);                         /* uSize */

                LogRel2(("DnD: Transferring host file '%s' to guest (as '%s', %zu bytes, mode %#x)\n",
                         pcszSrcPath, pcszDstPath, cbSize, fMode));

                /** @todo Set progress object title to current file being transferred? */

                /* Update object state to reflect that we have sent the file header. */
                pCtx->Transfer.fObjState |= DND_OBJ_STATE_HAS_HDR;
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
        rc = i_sendFileData(pCtx, pObj, pMsg);
    }

    if (RT_FAILURE(rc))
        LogRel(("DnD: Sending host file '%s' to guest failed, rc=%Rrc\n", pcszSrcPath, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendFileData(GuestDnDSendCtx *pCtx,
                                   PDNDTRANSFEROBJECT pObj, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pObj, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg, VERR_INVALID_POINTER);

    AssertPtrReturn(pCtx->pResp, VERR_WRONG_ORDER);

    /** @todo Don't allow concurrent reads per context! */

    /* Set the message type. */
    pMsg->setType(HOST_DND_HG_SND_FILE_DATA);

    const char *pcszSrcPath = DnDTransferObjectGetSourcePath(pObj);
    const char *pcszDstPath = DnDTransferObjectGetDestPath(pObj);

    /* Protocol version 1 sends the file path *every* time with a new file chunk.
     * In protocol version 2 we only do this once with HOST_DND_HG_SND_FILE_HDR. */
    if (m_DataBase.uProtocolVersion <= 1)
    {
        const size_t cchDstPath = RTStrNLen(pcszDstPath, RTPATH_MAX);

        pMsg->appendString(pcszDstPath);              /* pvName */
        pMsg->appendUInt32((uint32_t)cchDstPath + 1); /* cbName */
    }
    else if (m_DataBase.uProtocolVersion >= 2)
    {
        pMsg->appendUInt32(0);                        /** @todo ContextID not used yet. */
    }

    void *pvBuf  = pCtx->Transfer.pvScratchBuf;
    AssertPtr(pvBuf);
    size_t cbBuf = pCtx->Transfer.cbScratchBuf;
    Assert(cbBuf);

    uint32_t cbRead;

    int rc = DnDTransferObjectRead(pObj, pvBuf, cbBuf, &cbRead);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("cbBufe=%zu, cbRead=%RU32\n", cbBuf, cbRead));

        if (m_DataBase.uProtocolVersion <= 1)
        {
            pMsg->appendPointer(pvBuf, cbRead);                            /* pvData */
            pMsg->appendUInt32(cbRead);                                    /* cbData */
            pMsg->appendUInt32(DnDTransferObjectGetMode(pObj));            /* fMode */
        }
        else /* Protocol v2 and up. */
        {
            pMsg->appendPointer(pvBuf, cbRead);                            /* pvData */
            pMsg->appendUInt32(cbRead);                                    /* cbData */

            if (m_DataBase.uProtocolVersion >= 3)
            {
                /** @todo Calculate checksum. */
                pMsg->appendPointer(NULL, 0);                              /* pvChecksum */
                pMsg->appendUInt32(0);                                     /* cbChecksum */
            }
        }

        int rc2 = updateProgress(pCtx, pCtx->pResp, (uint32_t)cbRead);
        AssertRC(rc2);

        /* DnDTransferObjectRead() will return VINF_EOF if reading is complete. */
        if (rc == VINF_EOF)
            rc = VINF_SUCCESS;

        if (DnDTransferObjectIsComplete(pObj)) /* Done reading? */
            LogRel2(("DnD: Transferring host file '%s' to guest complete\n", pcszSrcPath));
    }
    else
        LogRel(("DnD: Reading from host file '%s' failed, rc=%Rrc\n", pcszSrcPath, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
DECLCALLBACK(int) GuestDnDTarget::i_sendTransferDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    GuestDnDSendCtx *pCtx = (GuestDnDSendCtx *)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDTarget *pThis = pCtx->pTarget;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    /* At the moment we only have one transfer list per transfer. */
    PDNDTRANSFERLIST pList = &pCtx->Transfer.List;

    LogFlowFunc(("pThis=%p, pList=%p, uMsg=%RU32\n", pThis, pList, uMsg));

    int  rc      = VINF_SUCCESS;
    int  rcGuest = VINF_SUCCESS; /* Contains error code from guest in case of VERR_GSTDND_GUEST_ERROR. */
    bool fNotify = false;

    switch (uMsg)
    {
        case GUEST_DND_CONNECT:
            /* Nothing to do here (yet). */
            break;

        case GUEST_DND_DISCONNECT:
            rc = VERR_CANCELLED;
            break;

        case GUEST_DND_GET_NEXT_HOST_MSG:
        {
            PVBOXDNDCBHGGETNEXTHOSTMSG pCBData = reinterpret_cast<PVBOXDNDCBHGGETNEXTHOSTMSG>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBHGGETNEXTHOSTMSG) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            try
            {
                GuestDnDMsg *pMsg = new GuestDnDMsg();

                rc = pThis->i_sendTransferListObject(pCtx, pList, pMsg);
                if (rc == VINF_EOF) /* Transfer complete? */
                {
                    LogFlowFunc(("Last transfer item processed, bailing out\n"));
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
        case GUEST_DND_GH_EVT_ERROR:
        {
            PVBOXDNDCBEVTERRORDATA pCBData = reinterpret_cast<PVBOXDNDCBEVTERRORDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBEVTERRORDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(CB_MAGIC_DND_GH_EVT_ERROR == pCBData->hdr.uMagic, VERR_INVALID_PARAMETER);

            pCtx->pResp->reset();

            if (RT_SUCCESS(pCBData->rc))
            {
                AssertMsgFailed(("Guest has sent an error event but did not specify an actual error code\n"));
                pCBData->rc = VERR_GENERAL_FAILURE; /* Make sure some error is set. */
            }

            rc = pCtx->pResp->setProgress(100, DND_PROGRESS_ERROR, pCBData->rc,
                                          GuestDnDTarget::i_guestErrorToString(pCBData->rc));
            if (RT_SUCCESS(rc))
            {
                rc      = VERR_GSTDND_GUEST_ERROR;
                rcGuest = pCBData->rc;
            }
            break;
        }
        case HOST_DND_HG_SND_DIR:
        case HOST_DND_HG_SND_FILE_HDR:
        case HOST_DND_HG_SND_FILE_DATA:
        {
            PVBOXDNDCBHGGETNEXTHOSTMSGDATA pCBData
                = reinterpret_cast<PVBOXDNDCBHGGETNEXTHOSTMSGDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(VBOXDNDCBHGGETNEXTHOSTMSGDATA) == cbParms, VERR_INVALID_PARAMETER);

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
                    LogFlowFunc(("Current message does not match:\n"));
                    LogFlowFunc(("\tCallback: uMsg=%RU32, cParms=%RU32, paParms=%p\n",
                                 pCBData->uMsg, pCBData->cParms, pCBData->paParms));
                    LogFlowFunc(("\t    Next: uMsg=%RU32, cParms=%RU32\n", pMsg->getType(), pMsg->getCount()));

                    /* Start over. */
                    pThis->msgQueueClear();

                    rc = VERR_INVALID_PARAMETER;
                }

                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("Returning uMsg=%RU32\n", uMsg));
                    rc = HGCM::Message::CopyParms(pCBData->paParms, pCBData->cParms, pMsg->getParms(), pMsg->getCount(),
                                                  false /* fDeepCopy */);
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
        AssertPtr(pCtx->pResp);
        int rc2 = pCtx->pResp->setCallback(uMsg, NULL /* PFNGUESTDNDCALLBACK */);
        AssertRC(rc2);

        /* Let the waiter(s) know. */
        fNotify = true;
    }

    LogFlowFunc(("fNotify=%RTbool, rc=%Rrc, rcToGuest=%Rrc\n", fNotify, rc, rcToGuest));

    if (fNotify)
    {
        int rc2 = pCtx->EventCallback.Notify(rc); /** @todo Also pass guest error back? */
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rcToGuest; /* Tell the guest. */
}

/**
 * Main function for sending the actual transfer data (i.e. files + directories) to the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Send context to use.
 * @param   msTimeout           Timeout (in ms) to use for getting the data sent.
 */
int GuestDnDTarget::i_sendTransferData(GuestDnDSendCtx *pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtr(pCtx->pResp);

#define REGISTER_CALLBACK(x)                                                 \
    do {                                                                     \
        rc = pCtx->pResp->setCallback(x, i_sendTransferDataCallback, pCtx); \
        if (RT_FAILURE(rc))                                                  \
            return rc;                                                       \
    } while (0)

#define UNREGISTER_CALLBACK(x)                        \
    do {                                              \
        int rc2 = pCtx->pResp->setCallback(x, NULL); \
        AssertRC(rc2);                                \
    } while (0)

    int rc = pCtx->Transfer.init(mData.mcbBlockSize);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCtx->EventCallback.Reset();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register callbacks.
     */
    /* Guest callbacks. */
    REGISTER_CALLBACK(GUEST_DND_CONNECT);
    REGISTER_CALLBACK(GUEST_DND_DISCONNECT);
    REGISTER_CALLBACK(GUEST_DND_GET_NEXT_HOST_MSG);
    REGISTER_CALLBACK(GUEST_DND_GH_EVT_ERROR);
    /* Host callbacks. */
    REGISTER_CALLBACK(HOST_DND_HG_SND_DIR);
    if (m_DataBase.uProtocolVersion >= 2)
        REGISTER_CALLBACK(HOST_DND_HG_SND_FILE_HDR);
    REGISTER_CALLBACK(HOST_DND_HG_SND_FILE_DATA);

    do
    {
        /*
         * Extract transfer list from current meta data.
         */
        rc = DnDTransferListAppendPathsFromBuffer(&pCtx->Transfer.List, DNDTRANSFERLISTFMT_URI,
                                                  (const char *)pCtx->Meta.pvData, pCtx->Meta.cbData, DND_PATH_SEPARATOR,
                                                  DNDTRANSFERLIST_FLAGS_RECURSIVE);
        if (RT_FAILURE(rc))
            break;

        /*
         * Update internal state to reflect everything we need to work with it.
         */
        pCtx->cbExtra                = DnDTransferListObjTotalBytes(&pCtx->Transfer.List);
        /* cbExtra can be 0, if all files are of 0 bytes size. */
        pCtx->Transfer.cObjToProcess = DnDTransferListObjCount(&pCtx->Transfer.List);
        AssertBreakStmt(pCtx->Transfer.cObjToProcess, rc = VERR_INVALID_PARAMETER);

        /* Update the meta data to have the current root transfer entries in the right shape. */
        if (DnDMIMEHasFileURLs(pCtx->Meta.strFmt.c_str(), RTSTR_MAX))
        {
            /* Save original format we're still going to use after updating the actual meta data. */
            Utf8Str strFmt = pCtx->Meta.strFmt;

            /* Reset stale data. */
            pCtx->Meta.reset();

            void  *pvData;
            size_t cbData;
#ifdef DEBUG
            rc = DnDTransferListGetRootsEx(&pCtx->Transfer.List, DNDTRANSFERLISTFMT_URI, "" /* pcszPathBase */,
                                           "\n" /* pcszSeparator */, (char **)&pvData, &cbData);
            AssertRCReturn(rc, rc);
            LogFlowFunc(("URI data:\n%s", (char *)pvData));
            RTMemFree(pvData);
            cbData = 0;
#endif
            rc = DnDTransferListGetRoots(&pCtx->Transfer.List, DNDTRANSFERLISTFMT_URI,
                                         (char **)&pvData, &cbData);
            AssertRCReturn(rc, rc);

            /* pCtx->Meta now owns the allocated data. */
            pCtx->Meta.strFmt      = strFmt;
            pCtx->Meta.pvData      = pvData;
            pCtx->Meta.cbData      = cbData;
            pCtx->Meta.cbAllocated = cbData;
            pCtx->Meta.cbAnnounced = cbData;
        }

        /*
         * The first message always is the data header. The meta data itself then follows
         * and *only* contains the root elements of a transfer list.
         *
         * After the meta data we generate the messages required to send the
         * file/directory data itself.
         *
         * Note: Protocol < v3 use the first data message to tell what's being sent.
         */

        /*
         * Send the data header first.
         */
        if (m_DataBase.uProtocolVersion >= 3)
            rc = i_sendMetaDataHeader(pCtx);

        /*
         * Send the (meta) data body.
         */
        if (RT_SUCCESS(rc))
            rc = i_sendMetaDataBody(pCtx);

        if (RT_SUCCESS(rc))
        {
            rc = waitForEvent(&pCtx->EventCallback, pCtx->pResp, msTimeout);
            if (RT_SUCCESS(rc))
                pCtx->pResp->setProgress(100, DND_PROGRESS_COMPLETE, VINF_SUCCESS);
        }

    } while (0);

    /*
     * Unregister callbacks.
     */
    /* Guest callbacks. */
    UNREGISTER_CALLBACK(GUEST_DND_CONNECT);
    UNREGISTER_CALLBACK(GUEST_DND_DISCONNECT);
    UNREGISTER_CALLBACK(GUEST_DND_GET_NEXT_HOST_MSG);
    UNREGISTER_CALLBACK(GUEST_DND_GH_EVT_ERROR);
    /* Host callbacks. */
    UNREGISTER_CALLBACK(HOST_DND_HG_SND_DIR);
    if (m_DataBase.uProtocolVersion >= 2)
        UNREGISTER_CALLBACK(HOST_DND_HG_SND_FILE_HDR);
    UNREGISTER_CALLBACK(HOST_DND_HG_SND_FILE_DATA);

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

            LogRel2(("DnD: Sending transfer data to guest cancelled by user\n"));

            rc2 = pCtx->pResp->setProgress(100, DND_PROGRESS_CANCELLED, VINF_SUCCESS);
            AssertRC(rc2);
        }
        else if (rc != VERR_GSTDND_GUEST_ERROR) /* Guest-side error are already handled in the callback. */
        {
            LogRel(("DnD: Sending transfer data to guest failed with rc=%Rrc\n", rc));
            int rc2 = pCtx->pResp->setProgress(100, DND_PROGRESS_ERROR, rc,
                                               GuestDnDTarget::i_hostErrorToString(rc));
            AssertRC(rc2);
        }

        rc = VINF_SUCCESS; /* The error was handled by the setProgress() calls above. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sends the next object of a transfer list to the guest.
 *
 * @returns VBox status code. VINF_EOF if the transfer list is complete.
 * @param   pCtx                Send context to use.
 * @param   pList               Transfer list to use.
 * @param   pMsg                Message to store send data into.
 */
int GuestDnDTarget::i_sendTransferListObject(GuestDnDSendCtx *pCtx, PDNDTRANSFERLIST pList, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg, VERR_INVALID_POINTER);

    int rc = updateProgress(pCtx, pCtx->pResp);
    AssertRCReturn(rc, rc);

    PDNDTRANSFEROBJECT pObj = DnDTransferListObjGetFirst(pList);
    if (!pObj) /* Transfer complete? */
        return VINF_EOF;

    switch (DnDTransferObjectGetType(pObj))
    {
        case DNDTRANSFEROBJTYPE_DIRECTORY:
            rc = i_sendDirectory(pCtx, pObj, pMsg);
            break;

        case DNDTRANSFEROBJTYPE_FILE:
            rc = i_sendFile(pCtx, pObj, pMsg);
            break;

        default:
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
            break;
    }

    if (   RT_SUCCESS(rc)
        && DnDTransferObjectIsComplete(pObj))
    {
        DnDTransferListObjRemove(pList, pObj);
        pObj = NULL;

        AssertReturn(pCtx->Transfer.cObjProcessed + 1 <= pCtx->Transfer.cObjToProcess, VERR_WRONG_ORDER);
        pCtx->Transfer.cObjProcessed++;

        pCtx->Transfer.fObjState = DND_OBJ_STATE_NONE;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function for sending raw data (e.g. text, RTF, ...) to the guest.
 *
 * @returns VBox status code.
 * @param   pCtx                Send context to use.
 * @param   msTimeout           Timeout (in ms) to use for getting the data sent.
 */
int GuestDnDTarget::i_sendRawData(GuestDnDSendCtx *pCtx, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    NOREF(msTimeout);

    /** @todo At the moment we only allow sending up to 64K raw data.
     *        For protocol v1+v2: Fix this by using HOST_DND_HG_SND_MORE_DATA.
     *        For protocol v3   : Send another HOST_DND_HG_SND_DATA message. */
    if (!pCtx->Meta.cbData)
        return VINF_SUCCESS;

    int rc = i_sendMetaDataHeader(pCtx);
    if (RT_SUCCESS(rc))
        rc = i_sendMetaDataBody(pCtx);

    int rc2;
    if (RT_FAILURE(rc))
    {
        LogRel(("DnD: Sending raw data to guest failed with rc=%Rrc\n", rc));
        rc2 = pCtx->pResp->setProgress(100 /* Percent */, DND_PROGRESS_ERROR, rc,
                                       GuestDnDTarget::i_hostErrorToString(rc));
    }
    else
        rc2 = pCtx->pResp->setProgress(100 /* Percent */, DND_PROGRESS_COMPLETE, rc);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Cancels sending DnD data.
 *
 * @returns VBox status code.
 * @param   aVeto               Whether cancelling was vetoed or not.
 *                              Not implemented yet.
 */
HRESULT GuestDnDTarget::cancel(BOOL *aVeto)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    LogRel2(("DnD: Sending cancelling request to the guest ...\n"));

    int rc = GuestDnDBase::sendCancel();

    if (aVeto)
        *aVeto = FALSE; /** @todo Implement vetoing. */

    HRESULT hr = RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

