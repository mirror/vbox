/* $Id$ */
/** @file
 * VBoxDnDSource.cpp - IDropSource implementation.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <windows.h>
#include <new> /* For bad_alloc. */

#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxDnD.h"

#include "VBox/HostServices/DragAndDropSvc.h"



VBoxDnDDropSource::VBoxDnDDropSource(VBoxDnDWnd *pParent)
    : mRefCount(1),
      mpWndParent(pParent),
      mClientID(UINT32_MAX),
      mdwCurEffect(0),
      muCurAction(DND_IGNORE_ACTION)
{
    int rc = VbglR3DnDConnect(&mClientID);

    LogFlowFunc(("rc=%Rrc\n", rc));
}

VBoxDnDDropSource::~VBoxDnDDropSource(void)
{
    int rc = VbglR3DnDDisconnect(mClientID);

    LogFlowFunc(("rc=%Rrc, mRefCount=%RI32\n", rc, mRefCount));
}

/* static */
int VBoxDnDDropSource::CreateDropSource(VBoxDnDWnd *pParent, IDropSource **ppDropSource)
{
    AssertPtrReturn(pParent, VERR_INVALID_POINTER);
    AssertPtrReturn(ppDropSource, VERR_INVALID_POINTER);

    int rc;
    try
    {
        *ppDropSource = new VBoxDnDDropSource(pParent);
        rc = VINF_SUCCESS;
    }
    catch(std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) VBoxDnDDropSource::AddRef(void)
{
    return InterlockedIncrement(&mRefCount);
}

STDMETHODIMP_(ULONG) VBoxDnDDropSource::Release(void)
{
    LONG lCount = InterlockedDecrement(&mRefCount);
    if (lCount == 0)
    {
        delete this;
        return 0;
    }

    return lCount;
}

STDMETHODIMP VBoxDnDDropSource::QueryInterface(REFIID iid, void **ppvObject)
{
    AssertPtrReturn(ppvObject, E_INVALIDARG);

    if (   iid == IID_IDropSource
        || iid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
}

/*
 * IDropSource methods.
 */

/**
 * The system informs us about whether we should continue the drag'n drop
 * operation or not, depending on the sent key states.
 *
 * @return  HRESULT
 */
STDMETHODIMP VBoxDnDDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD dwKeyState)
{
#ifndef DEBUG_andy
    LogFlowFunc(("fEscapePressed=%RTbool, dwKeyState=0x%x, mdwCurEffect=%RI32, muCurAction=%RU32\n",
                 fEscapePressed, dwKeyState, mdwCurEffect, muCurAction));
#endif

    /* ESC pressed? Bail out. */
    if (fEscapePressed)
    {
        mdwCurEffect = 0;
        muCurAction = DND_IGNORE_ACTION;

        return DRAGDROP_S_CANCEL;
    }

    /* Left mouse button released? Start "drop" action. */
    if ((dwKeyState & MK_LBUTTON) == 0)
        return DRAGDROP_S_DROP;

    /* No change, just continue. */
    return S_OK;
}

/**
 * The drop target gives our source feedback about whether
 * it can handle our data or not.
 *
 * @return  HRESULT
 */
STDMETHODIMP VBoxDnDDropSource::GiveFeedback(DWORD dwEffect)
{
    uint32_t uAction = DND_IGNORE_ACTION;

#ifndef DEBUG_andy
    LogFlowFunc(("dwEffect=0x%x\n", dwEffect));
#endif
    if (dwEffect)
    {
        if (dwEffect & DROPEFFECT_COPY)
            uAction |= DND_COPY_ACTION;
        if (dwEffect & DROPEFFECT_MOVE)
            uAction |= DND_MOVE_ACTION;
        if (dwEffect & DROPEFFECT_LINK)
            uAction |= DND_LINK_ACTION;
    }

    mdwCurEffect = dwEffect;
    muCurAction = uAction;

    return DRAGDROP_S_USEDEFAULTCURSORS;
}

