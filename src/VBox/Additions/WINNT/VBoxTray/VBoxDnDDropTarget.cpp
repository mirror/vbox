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
      mpvData(NULL),
      mcbData(0),
      hEventDrop(NIL_RTSEMEVENT)
{
    int rc = VbglR3DnDConnect(&mClientID);
    if (RT_SUCCESS(rc))
        rc = RTSemEventCreate(&hEventDrop);

    LogFlowFunc(("clientID=%RU32, rc=%Rrc\n",
                 mClientID, rc));
}

VBoxDnDDropTarget::~VBoxDnDDropTarget(void)
{
    reset();

    int rc2 = VbglR3DnDDisconnect(mClientID);
    AssertRC(rc2);
    rc2 = RTSemEventDestroy(hEventDrop);
    AssertRC(rc2);

    LogFlowFunc(("rc=%Rrc, mRefCount=%RI32\n", rc2, mRefCount));
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

    LogFlowFunc(("pDataObject=0x%p, grfKeyState=0x%x, x=%ld, y=%ld, dwEffect=%RU32\n",
                 pDataObject, grfKeyState, pt.x, pt.y, *pdwEffect));

    reset();

    /* Try different formats. CF_HDROP is the most common one, so start
     * with this. */
    /** @todo At the moment we only support TYMED_HGLOBAL. */
    FORMATETC fmtEtc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    HRESULT hr = pDataObject->QueryGetData(&fmtEtc);
    if (hr != S_OK)
    {
        LogFlowFunc(("CF_HDROP not supported, hr=%Rhrc\n", hr));

        /* So we couldn't retrieve the data in CF_HDROP format; try with
         * CF_TEXT format now. Rest stays the same. */
        fmtEtc.cfFormat = CF_TEXT;
        hr = pDataObject->QueryGetData(&fmtEtc);
        if (hr != S_OK)
        {
            LogFlowFunc(("CF_TEXT not supported, hr=%Rhrc\n", hr));
            fmtEtc.cfFormat = 0; /* Mark it to not supported. */
        }
    }

    /* Did we find a format that we support? */
    if (fmtEtc.cfFormat)
    {
        LogFlowFunc(("Found supported format %RI16\n", fmtEtc.cfFormat));

        /* Make a copy of the FORMATETC structure so that we later can
         * use this for comparrison and stuff. */
        /** Note: The DVTARGETDEVICE member only is a shallow copy! */
        memcpy(&mFormatEtc, &fmtEtc, sizeof(FORMATETC));

        /* Which drop effect we're going to use? */
        /* Note: pt is not used since we don't need to differentiate within our
         *       proxy window. */
        *pdwEffect = VBoxDnDDropTarget::GetDropEffect(grfKeyState, *pdwEffect);
    }
    else
    {
        /* No or incompatible data -- so no drop effect required. */
        *pdwEffect = DROPEFFECT_NONE;

        switch (hr)
        {
            case ERROR_INVALID_FUNCTION:
            {
                LogRel(("DnD: Drag'n drop format is not supported by VBoxTray\n"));

                /* Enumerate supported source formats. This shouldn't happen too often
                 * on day to day use, but still keep it in here. */
                IEnumFORMATETC *pEnumFormats;
                HRESULT hr2 = pDataObject->EnumFormatEtc(DATADIR_GET, &pEnumFormats);
                if (SUCCEEDED(hr2))
                {
                    LogRel(("DnD: The following formats were offered to us:\n"));

                    FORMATETC curFormatEtc;
                    while (pEnumFormats->Next(1, &curFormatEtc,
                                              NULL /* pceltFetched */) == S_OK)
                	{
                        WCHAR wszCfName[128]; /* 128 chars should be enough, rest will be truncated. */
                        hr2 = GetClipboardFormatNameW(curFormatEtc.cfFormat, wszCfName,
                                                      sizeof(wszCfName) / sizeof(WCHAR));
                        LogRel(("\tcfFormat=%RI16, tyMed=%RI32, dwAspect=%RI32, strCustomName=%ls, hr=%Rhrc\n",
                                curFormatEtc.cfFormat,
                                curFormatEtc.tymed,
                                curFormatEtc.dwAspect,
                                wszCfName, hr2));
                    }

                    pEnumFormats->Release();
            	}

                break;
            }

            default:
                break;
        }
    }

    LogFlowFunc(("Returning cfFormat=%RI16, pdwEffect=%ld, hr=%Rhrc\n",
                 fmtEtc.cfFormat, *pdwEffect, hr));
    return hr;
}

STDMETHODIMP VBoxDnDDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    AssertPtrReturn(pdwEffect, E_INVALIDARG);

#ifdef DEBUG_andy
    LogFlowFunc(("cfFormat=%RI16, grfKeyState=0x%x, x=%ld, y=%ld\n",
                 mFormatEtc.cfFormat, grfKeyState, pt.x, pt.y));
#endif

    if (mFormatEtc.cfFormat)
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
    LogFlowFunc(("cfFormat=%RI16\n", mFormatEtc.cfFormat));
#endif

    if (mpWndParent)
        mpWndParent->hide();

    return S_OK;
}

STDMETHODIMP VBoxDnDDropTarget::Drop(IDataObject *pDataObject,
                                     DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    AssertPtrReturn(pDataObject, E_INVALIDARG);
    AssertPtrReturn(pdwEffect, E_INVALIDARG);

#ifdef DEBUG
    LogFlowFunc(("mFormatEtc.cfFormat=%RI16, pDataObject=0x%p, grfKeyState=0x%x, x=%ld, y=%ld\n",
                 mFormatEtc.cfFormat, pDataObject, grfKeyState, pt.x, pt.y));
#endif
    HRESULT hr = S_OK;

    bool fCanDrop = false;
    if (mFormatEtc.cfFormat) /* Did we get a supported format yet? */
    {
        /* Make sure the data object's data format is still the same
         * as we got it in DragEnter(). */
        hr = pDataObject->QueryGetData(&mFormatEtc);
        AssertMsg(SUCCEEDED(hr),
                  ("Data format changed between DragEnter() and Drop(), cfFormat=%RI16, hr=%Rhrc\n",
                  mFormatEtc.cfFormat, hr));
    }

    if (SUCCEEDED(hr))
    {
        STGMEDIUM stgMed;
        hr = pDataObject->GetData(&mFormatEtc, &stgMed);
        if (SUCCEEDED(hr))
        {
            /*
             * First stage: Prepare the access to the storage medium.
             *              For now we only support HGLOBAL stuff.
             */
            PVOID pvData = NULL; /** @todo Put this in an own union? */

            switch (mFormatEtc.tymed)
            {
                case TYMED_HGLOBAL:
                    pvData = GlobalLock(stgMed.hGlobal);
                    if (!pvData)
                    {
                        LogFlowFunc(("Locking HGLOBAL storage failed with %Rrc\n",
                                     RTErrConvertFromWin32(GetLastError())));
                        hr = ERROR_INVALID_HANDLE;
                    }
                    break;

                default:
                    AssertMsgFailed(("Storage medium type %RI32 supported\n",
                                     mFormatEtc.tymed));
                    hr = ERROR_NOT_SUPPORTED;
                    break;
            }

            if (SUCCEEDED(hr))
            {
                /* Second stage: Do the actual copying of the data object's data,
                                 based on the storage medium type. */
                switch (mFormatEtc.cfFormat)
                {
                    case CF_TEXT:
                    {
                        LogFlowFunc(("pvData=%s\n", (char*)pvData));

                        uint32_t cbSize = strlen((char*)pvData); /** @todo Evil hack, fix this! */
                        mpvData = RTMemDup(pvData, cbSize);
                        mcbData = cbSize;
                        break;
                    }

                    case CF_HDROP:
                        break;

                    default:
                        AssertMsgFailed(("Format of type %RI16 supported\n",
                                         mFormatEtc.cfFormat));
                        hr = ERROR_NOT_SUPPORTED;
                        break;
                }
            }

            /*
             * Third stage: Release access to the storage medium again.
             */
            switch (mFormatEtc.tymed)
            {
                case TYMED_HGLOBAL:
                    GlobalUnlock(stgMed.hGlobal);
                    break;

                default:
                    AssertMsgFailed(("Storage medium type %RI32 supported\n",
                                     mFormatEtc.tymed));
                    hr = ERROR_NOT_SUPPORTED;
                    break;
            }

            /* Release storage medium again. */
            ReleaseStgMedium(&stgMed);

            if (SUCCEEDED(hr))
            {
                RTSemEventSignal(hEventDrop);
                fCanDrop = true;
            }
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

    if (mpWndParent)
        mpWndParent->hide();

    LogFlowFunc(("Returning with mFormatEtc.cfFormat=%RI16, fCanDrop=%RTbool, *pdwEffect=%RI32\n",
                 mFormatEtc.cfFormat, fCanDrop, *pdwEffect));
    return hr;
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

void VBoxDnDDropTarget::reset(void)
{
    LogFlowFuncEnter();

    if (mpvData)
    {
        RTMemFree(mpvData);
        mpvData = NULL;
    }

    mcbData = 0;
    RT_ZERO(mFormatEtc);
}

int VBoxDnDDropTarget::WaitForDrop(RTMSINTERVAL msTimeout)
{
    LogFlowFunc(("msTimeout=%RU32\n", msTimeout));
    int rc = RTSemEventWait(hEventDrop, msTimeout);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

