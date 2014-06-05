/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop target.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#include "GuestDnDTargetImpl.h"

#include "Global.h"
#include "AutoCaller.h"

#include <algorithm> /* For std::find(). */
#include <iprt/cpp/utils.h> /* For unconst(). */

#include <VBox/com/array.h>
#include <VBox/HostServices/DragAndDropSvc.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDnDTarget)

HRESULT GuestDnDTarget::FinalConstruct(void)
{
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

HRESULT GuestDnDTarget::isFormatSupported(const com::Utf8Str &aFormat,
                                          BOOL *aSupported)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    return GuestDnDBase::isFormatSupported(aFormat, aSupported);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::getFormats(std::vector<com::Utf8Str> &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    return GuestDnDBase::getFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::addFormats(const std::vector<com::Utf8Str> &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    return GuestDnDBase::addFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::removeFormats(const std::vector<com::Utf8Str> &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    return GuestDnDBase::removeFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

// implementation of wrapped IDnDTarget methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDTarget::enter(ULONG aScreenId, ULONG aX, ULONG aY,
                              DnDAction_T aDefaultAction,
                              const std::vector<DnDAction_T> &aAllowedActions,
                              const std::vector<com::Utf8Str> &aFormats,
                              DnDAction_T *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
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

    /* Make a flat data string out of the supported format list. */
    Utf8Str strFormats = GuestDnD::toFormatString(m_strFormats, aFormats);
    /* If there is no valid supported format, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    /* Adjust the coordinates in a multi-monitor setup. */
    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        paParms[i++].setUInt32(aScreenId);
        paParms[i++].setUInt32(aX);
        paParms[i++].setUInt32(aY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_ENTER,
                                      i, paParms);
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->defAction());
        }
    }

    if (aResultAction)
        *aResultAction = resAction;

    LogFlowFunc(("hr=%Rhrc, resAction=%ld\n", hr, resAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::move(ULONG aScreenId, ULONG aX, ULONG aY,
                             DnDAction_T aDefaultAction,
                             const std::vector<DnDAction_T> &aAllowedActions,
                             const std::vector<com::Utf8Str> &aFormats,
                             DnDAction_T *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
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

    /* Make a flat data string out of the supported format list. */
    RTCString strFormats = GuestDnD::toFormatString(m_strFormats, aFormats);
    /* If there is no valid supported format, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        paParms[i++].setUInt32(aScreenId);
        paParms[i++].setUInt32(aX);
        paParms[i++].setUInt32(aY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_MOVE,
                                      i, paParms);
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->defAction());
        }
    }

    if (aResultAction)
        *aResultAction = resAction;

    LogFlowFunc(("hr=%Rhrc, *pResultAction=%ld\n", hr, resAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::leave(ULONG uScreenId)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
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

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::drop(ULONG aScreenId, ULONG aX, ULONG aY,
                             DnDAction_T aDefaultAction,
                             const std::vector<DnDAction_T> &aAllowedActions,
                             const std::vector<com::Utf8Str> &aFormats,
                             com::Utf8Str &aFormat, DnDAction_T *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */

    /* Everything else is optional. */

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

    /* Make a flat data string out of the supported format list. */
    Utf8Str strFormats = GuestDnD::toFormatString(m_strFormats, aFormats);
    /* If there is no valid supported format, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    /* Adjust the coordinates in a multi-monitor setup. */
    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        paParms[i++].setUInt32(aScreenId);
        paParms[i++].setUInt32(aX);
        paParms[i++].setUInt32(aY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_DROPPED,
                                      i, paParms);
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
            {
                resAction = GuestDnD::toMainAction(pResp->defAction());
                aFormat = pResp->format();

                LogFlowFunc(("resFormat=%s, resAction=%RU32\n",
                             pResp->format().c_str(), pResp->defAction()));
            }
        }
    }

    if (aResultAction)
        *aResultAction = resAction;

    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::sendData(ULONG aScreenId,
                                 const com::Utf8Str &aFormat,
                                 const std::vector<BYTE> &aData,
                                 ComPtr<IProgress> &aProgress)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP) || !defined(VBOX_WITH_DRAG_AND_DROP_GH)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation */


    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    VBOXHGCMSVCPARM paParms[8];
    int i = 0;
    paParms[i++].setUInt32(aScreenId);
    paParms[i++].setPointer((void *)aFormat.c_str(), (uint32_t)aFormat.length() + 1);
    paParms[i++].setUInt32((uint32_t)aFormat.length() + 1);
    paParms[i++].setPointer((void *)aData.front(), (uint32_t)aData.size());
    paParms[i++].setUInt32((uint32_t)aData.size());

    GuestDnDResponse *pResp = GuestDnDInst()->response();
    if (pResp)
    {
        /* Reset any old progress status. */
        pResp->resetProgress(m_pGuest);

        /* Note: The actual data transfer of files/directoies is performed by the
         *       DnD host service. */
        int rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_SND_DATA,
                                          i, paParms);
        if (RT_SUCCESS(rc))
        {
            /* Query the progress object to the caller. */
            if (aProgress)
                pResp->queryProgressTo(aProgress.asOutParam());
        }
    }

    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

