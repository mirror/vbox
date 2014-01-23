/* $Id$ */
/** @file
 * VBoxDnDTarget.cpp - IDropTarget implementation.
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
#include <windows.h>
#include <new> /* For bad_alloc. */

#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxDnD.h"

#include "VBox/HostServices/DragAndDropSvc.h"



VBoxDnDDropTarget::VBoxDnDDropTarget(VBoxDnDWnd *pParent)
    : mRefCount(1),
      mpWndParent(pParent),
      mClientID(UINT32_MAX),
      mdwCurEffect(0),
      mpDataObject(NULL),
      mfHasDropData(false)
{
    int rc = VbglR3DnDConnect(&mClientID);

    LogFlowFunc(("clientID=%RU32, rc=%Rrc\n",
                 mClientID, rc));
}

VBoxDnDDropTarget::~VBoxDnDDropTarget(void)
{
    int rc = VbglR3DnDDisconnect(mClientID);

    LogFlowFunc(("rc=%Rrc, mRefCount=%RI32\n", rc, mRefCount));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) VBoxDnDDropTarget::AddRef(void)
{
    return InterlockedIncrement(&mRefCount);
}

STDMETHODIMP_(ULONG) VBoxDnDDropTarget::Release(void)
{
    LONG lCount = InterlockedDecrement(&mRefCount);
    if (lCount == 0)
    {
        delete this;
        return 0;
    }

    return lCount;
}

STDMETHODIMP VBoxDnDDropTarget::QueryInterface(REFIID iid, void **ppvObject)
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
 * IDropTarget methods.
 */

STDMETHODIMP VBoxDnDDropTarget::DragEnter(IDataObject *pDataObject, DWORD grfKeyState,
                                          POINTL pt, DWORD *pdwEffect)
{
    AssertPtrReturn(pDataObject, E_INVALIDARG);
    AssertPtrReturn(pdwEffect, E_INVALIDARG);

    LogFlowFunc(("mfHasDropData=%RTbool, pDataObject=0x%p, grfKeyState=0x%x, x=%ld, y=%ld, dwEffect=%RU32\n",
                 mfHasDropData, pDataObject, grfKeyState, pt.x, pt.y, *pdwEffect));

    FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

    /* We only handle CF_HDROP in an HGLOBAL medium at the moment. */
    HRESULT hr = pDataObject->QueryGetData(&fmtetc);
    mfHasDropData = hr == S_OK;

    if (mfHasDropData)
    {
        /* Which drop effect we're going to use? */
        /* Note: pt is not used since we don't need to differentiate within our
         *       proxy window. */
        *pdwEffect = VBoxDnDDropTarget::GetDropEffect(grfKeyState, *pdwEffect);
    }
    else
    {
        /* No or incompatible data -- so no drop effect required. */
        *pdwEffect = DROPEFFECT_NONE;
    }

    LogFlowFunc(("Returning mfHasDropData=%RTbool, pdwEffect=%ld, hr=%Rhrc\n",
                 mfHasDropData, *pdwEffect, hr));
    return hr;
}

STDMETHODIMP VBoxDnDDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    AssertPtrReturn(pdwEffect, E_INVALIDARG);

#ifdef DEBUG_andy
    LogFlowFunc(("mfHasDropData=%RTbool, grfKeyState=0x%x, x=%ld, y=%ld\n",
                 mfHasDropData, grfKeyState, pt.x, pt.y));
#endif

    if (mfHasDropData)
    {
        /* Note: pt is not used since we don't need to differentiate within our
         *       proxy window. */
        *pdwEffect = VBoxDnDDropTarget::GetDropEffect(grfKeyState, *pdwEffect);
    }
    else
    {
        *pdwEffect = DROPEFFECT_NONE;
    }

#ifdef DEBUG_andy
    LogFlowFunc(("Returning *pdwEffect=%ld\n", *pdwEffect));
#endif
    return S_OK;
}

STDMETHODIMP VBoxDnDDropTarget::DragLeave(void)
{
#ifdef DEBUG_andy
    LogFlowFunc(("mfHasDropData=%RTbool\n", mfHasDropData));
#endif

    mpWndParent->hide();

    return S_OK;
}

STDMETHODIMP VBoxDnDDropTarget::Drop(IDataObject *pDataObject,
                                     DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    AssertPtrReturn(pDataObject, E_INVALIDARG);
    AssertPtrReturn(pdwEffect, E_INVALIDARG);

#ifdef DEBUG
    LogFlowFunc(("mfHasDropData=%RTbool, pDataObject=0x%p, grfKeyState=0x%x, x=%ld, y=%ld\n",
                 mfHasDropData, pDataObject, grfKeyState, pt.x, pt.y));
#endif
    bool fCanDrop = false;
    if (mfHasDropData)
    {
        FORMATETC fmtEtc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stgMed;

        AssertPtr(pDataObject);
        if (   (pDataObject->QueryGetData(&fmtEtc)     == S_OK)
            && (pDataObject->GetData(&fmtEtc, &stgMed) == S_OK))
        {
            PVOID pvData = GlobalLock(stgMed.hGlobal);
#ifdef DEBUG
            if (fmtEtc.cfFormat == CF_TEXT)
                LogFlowFunc(("pvData=%s\n", (char*)pvData));
#endif
            GlobalUnlock(stgMed.hGlobal);

            /* Release storage medium again. */
            ReleaseStgMedium(&stgMed);

            fCanDrop = true;
        }
    }

    if (fCanDrop)
    {
        /* Note: pt is not used since we don't need to differentiate within our
         *       proxy window. */
        *pdwEffect = VBoxDnDDropTarget::GetDropEffect(grfKeyState, *pdwEffect);
    }
    else
        *pdwEffect = DROPEFFECT_NONE;

    LogFlowFunc(("Returning with mfHasDropData=%RTbool, fCanDrop=%RTbool, *pdwEffect=%RI32\n",
                 mfHasDropData, fCanDrop, *pdwEffect));
    return S_OK;
}

/* static */
DWORD VBoxDnDDropTarget::GetDropEffect(DWORD grfKeyState, DWORD dwAllowedEffects)
{
    DWORD dwEffect = DROPEFFECT_NONE;

    if(grfKeyState & MK_CONTROL)
        dwEffect = dwAllowedEffects & DROPEFFECT_COPY;
    else if(grfKeyState & MK_SHIFT)
        dwEffect = dwAllowedEffects & DROPEFFECT_MOVE;

    /* If there still was no drop effect assigned, check for the handed-in
     * allowed effects and assign one of them.
     *
     * Note: A move action has precendence over a copy action! */
    if (dwEffect == DROPEFFECT_NONE)
    {
        if (dwAllowedEffects & DROPEFFECT_COPY)
            dwEffect = DROPEFFECT_COPY;
        if (dwAllowedEffects & DROPEFFECT_MOVE)
            dwEffect = DROPEFFECT_MOVE;
    }

#ifdef DEBUG_andy
    LogFlowFunc(("grfKeyState=0x%x, dwAllowedEffects=0x%x, dwEffect=0x%x\n",
                 grfKeyState, dwAllowedEffects, dwEffect));
#endif
    return dwEffect;
}

