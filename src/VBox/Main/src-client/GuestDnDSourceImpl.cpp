/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop source.
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
#include "GuestDnDSourceImpl.h"
#include "GuestDnDPrivate.h"

#include "Global.h"
#include "AutoCaller.h"

#include <iprt/cpp/utils.h> /* For unconst(). */

#include <VBox/com/array.h>
#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/DragAndDropSvc.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDnDSource)

HRESULT GuestDnDSource::FinalConstruct(void)
{
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

// implementation of wrapped private getters/setters for attributes
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

    /* Default is ignoring the action. */
    DnDAction_T defaultAction = DnDAction_Ignore;

    HRESULT hr = S_OK;

    VBOXHGCMSVCPARM paParms[1];
    int i = 0;
    paParms[i++].setUInt32(uScreenId);

    int rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_GH_REQ_PENDING,
                                      i, paParms);
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

                GuestDnD::toFormatVector(GuestDnDInst()->supportedFormats(),
                                         pResp->format(), aFormats);
                GuestDnD::toMainActions(pResp->allActions(), aAllowedActions);
            }
        }

        if (aDefaultAction)
            *aDefaultAction = defaultAction;
    }

    if (RT_FAILURE(rc))
        hr = setError(VBOX_E_IPRT_ERROR,
                      tr("Unable to retrieve pending status (%Rrc)\n"), rc);

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

    const char *pcszFormat = aFormat.c_str();
    bool fNeedsDropDir = DnDMIMENeedsDropDir(pcszFormat, strlen(pcszFormat));
    LogFlowFunc(("strFormat=%s, uAction=0x%x, fNeedsDropDir=%RTbool\n",
                 pcszFormat, uAction, fNeedsDropDir));

    GuestDnDResponse *pResp = GuestDnDInst()->response();
    if (pResp)
    {
        /* Reset any old data. */
        pResp->reset();
        pResp->resetProgress(m_pGuest);

        /* Set the format we are going to retrieve to have it around
         * when retrieving the data later. */
        pResp->setFormat(aFormat);

        if (fNeedsDropDir)
        {
            char szDropDir[RTPATH_MAX];
            int rc = DnDDirCreateDroppedFiles(szDropDir, sizeof(szDropDir));
            LogFlowFunc(("rc=%Rrc, szDropDir=%s\n", rc, szDropDir));
            if (RT_FAILURE(rc))
                return setError(VBOX_E_IPRT_ERROR,
                                tr("Unable to create the temporary drag'n drop directory \"%s\" (%Rrc)\n"),
                                szDropDir, rc);

            pResp->setDropDir(szDropDir);
        }

        VBOXHGCMSVCPARM paParms[4];
        int i = 0;
        paParms[i++].setPointer((void*)aFormat.c_str(), (uint32_t)aFormat.length() + 1);
        paParms[i++].setUInt32((uint32_t)aFormat.length() + 1);
        paParms[i++].setUInt32(uAction);

        int rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_GH_EVT_DROPPED,
                                          i, paParms);
        if (RT_SUCCESS(rc))
        {
            /* Query the progress object to the caller. */
            pResp->queryProgressTo(aProgress.asOutParam());
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR,
                          tr("Error signalling to drop data (%Rrc)\n"), rc);
    }

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

    GuestDnDResponse *pResp = GuestDnDInst()->response();
    if (pResp)
    {
        size_t cbData = pResp->size();
        if (cbData)
        {
            const void *pvData = pResp->data();
            AssertPtr(pvData);

            Utf8Str strFormat = pResp->format();
            LogFlowFunc(("strFormat=%s, cbData=%zu, pvData=0x%p\n",
                         strFormat.c_str(), cbData, pvData));

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

                        LogFlowFunc(("Found %zu root URIs (%zu bytes)\n",
                                     lstURI.RootCount(), cbURIs));

                        aData.resize(cbURIs + 1 /* Include termination */);
                        memcpy((void *)aData.front(), strURIs.c_str(), cbURIs);
                    }
                    else
                        hr = VBOX_E_IPRT_ERROR;
                }
                else
                {
                    /* Copy the data into a safe array of bytes. */
                    aData.resize(cbData);
                    memcpy((void *)aData.front(), pvData, cbData);
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

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

