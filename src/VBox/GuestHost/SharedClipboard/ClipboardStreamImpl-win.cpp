/* $Id$ */
/** @file
 * ClipboardStreamImpl-win.cpp - Shared Clipboard IStream object implementation (guest and host side).
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/GuestHost/SharedClipboard-win.h>

#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/thread.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-win.h>
#include <strsafe.h>

#include <VBox/log.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Static variables                                                                                                             *
*********************************************************************************************************************************/



VBoxClipboardWinStreamImpl::VBoxClipboardWinStreamImpl(VBoxClipboardWinDataObject *pParent,
                                                       PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uObjIdx)
    : m_pParent(pParent)
    , m_lRefCount(1)
    , m_pURITransfer(pTransfer)
    , m_uObjIdx(uObjIdx)
{
    AssertPtr(m_pURITransfer);

    LogFunc(("m_uObjIdx=%RU64\n", uObjIdx));
}

VBoxClipboardWinStreamImpl::~VBoxClipboardWinStreamImpl(void)
{
    LogFlowThisFuncEnter();
}

/*
 * IUnknown methods.
 */

STDMETHODIMP VBoxClipboardWinStreamImpl::QueryInterface(REFIID iid, void **ppvObject)
{
    AssertPtrReturn(ppvObject, E_INVALIDARG);

    if (iid == IID_IUnknown)
    {
        LogFlowFunc(("IID_IUnknown\n"));
        *ppvObject = (IUnknown *)(ISequentialStream *)this;
    }
    else if (iid == IID_ISequentialStream)
    {
        LogFlowFunc(("IID_ISequentialStream\n"));
        *ppvObject = (ISequentialStream *)this;
    }
    else if (iid == IID_IStream)
    {
        LogFlowFunc(("IID_IStream\n"));
        *ppvObject = (IStream *)this;
    }
    else
    {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) VBoxClipboardWinStreamImpl::AddRef(void)
{
    LONG lCount = InterlockedIncrement(&m_lRefCount);
    LogFlowFunc(("lCount=%RI32\n", lCount));
    return lCount;
}

STDMETHODIMP_(ULONG) VBoxClipboardWinStreamImpl::Release(void)
{
    LONG lCount = InterlockedDecrement(&m_lRefCount);
    LogFlowFunc(("lCount=%RI32\n", m_lRefCount));
    if (lCount == 0)
    {
        if (m_pParent)
            m_pParent->OnTransferComplete();

        delete this;
        return 0;
    }

    return lCount;
}

/*
 * IStream methods.
 */

STDMETHODIMP VBoxClipboardWinStreamImpl::Clone(IStream** ppStream)
{
    RT_NOREF(ppStream);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Commit(DWORD dwFrags)
{
    RT_NOREF(dwFrags);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::CopyTo(IStream *pDestStream, ULARGE_INTEGER nBytesToCopy, ULARGE_INTEGER *nBytesRead,
                                                ULARGE_INTEGER *nBytesWritten)
{
    RT_NOREF(pDestStream, nBytesToCopy, nBytesRead, nBytesWritten);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::LockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes,DWORD dwFlags)
{
    RT_NOREF(nStart, nBytes, dwFlags);

    LogFlowThisFuncEnter();
    return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Read(void *pvBuffer, ULONG nBytesToRead, ULONG *nBytesRead)
{
    LogFlowThisFuncEnter();

    AssertPtr(m_pURITransfer->State.ObjCtx.pObjInfo);

    const uint64_t cbSize      = m_pURITransfer->State.ObjCtx.pObjInfo->cbObject;
          uint64_t cbProcessed = m_pURITransfer->State.ObjCtx.cbProcessed;

    if (cbProcessed == cbSize)
    {
        /* There can be 0-byte files. */
        AssertMsg(cbSize == 0, ("Object is complete -- can't read from it anymore\n"));
        if (nBytesRead)
            *nBytesRead = 0; /** @todo If the file size is 0, already return at least 1 byte, else the whole operation will fail. */
        return S_OK; /* Don't report any failures back to Windows. */
    }

    const uint32_t cbToRead = RT_MIN(cbSize - cbProcessed, nBytesToRead);
          uint32_t cbRead   = 0;

    int rc = VINF_SUCCESS;

    if (cbToRead)
    {
        rc = m_pURITransfer->ProviderIface.pfnObjRead(&m_pURITransfer->ProviderCtx, m_pURITransfer->State.ObjCtx.uHandle,
                                                      pvBuffer, cbToRead, 0 /* fFlags */, &cbRead);
        if (RT_SUCCESS(rc))
        {
            cbProcessed += cbRead;
            Assert(cbProcessed <= cbSize);

            if (cbProcessed == cbSize)
                m_pParent->OnTransferComplete();

            m_pURITransfer->State.ObjCtx.cbProcessed = cbProcessed;
        }
    }

    if (nBytesRead)
        *nBytesRead = (ULONG)cbRead;

    LogFlowThisFunc(("rc=%Rrc, cbSize=%RU64, cbProcessed=%RU64 -> cbToRead=%zu, cbRead=%zu\n",
                     rc, cbSize, cbProcessed, cbToRead, cbRead));
    return RT_SUCCESS(rc) ? S_OK : E_FAIL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Revert(void)
{
    LogFlowThisFuncEnter();
    return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Seek(LARGE_INTEGER nMove, DWORD dwOrigin, ULARGE_INTEGER* nNewPos)
{
    RT_NOREF(nMove, dwOrigin, nNewPos);

    LogFlowThisFuncEnter();
    return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::SetSize(ULARGE_INTEGER nNewSize)
{
    RT_NOREF(nNewSize);

    LogFlowThisFuncEnter();
    return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Stat(STATSTG *pStatStg, DWORD dwFlags)
{
    HRESULT hr = S_OK;

    if (pStatStg)
    {
        const SharedClipboardURIObject *pObj = SharedClipboardURITransferGetObject(m_pURITransfer, m_uObjIdx);

        RT_BZERO(pStatStg, sizeof(STATSTG));

        switch (dwFlags)
        {
            case STATFLAG_NONAME:
                pStatStg->pwcsName = NULL;
                break;

            case STATFLAG_DEFAULT:
            {
                int rc2 = RTStrToUtf16(pObj->GetDestPathAbs().c_str(), &pStatStg->pwcsName);
                if (RT_FAILURE(rc2))
                    hr = E_FAIL;
                break;
            }

            default:
                hr = STG_E_INVALIDFLAG;
                break;
        }

        if (SUCCEEDED(hr))
        {
            pStatStg->type              = STGTY_STREAM;
            pStatStg->grfMode           = STGM_READ;
            pStatStg->grfLocksSupported = 0;
            pStatStg->cbSize.QuadPart   = pObj->GetSize();
        }
    }
    else
       hr = STG_E_INVALIDPOINTER;

    LogFlowThisFunc(("hr=%Rhrc\n", hr));
    return hr;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::UnlockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags)
{
    RT_NOREF(nStart, nBytes, dwFlags);

    LogFlowThisFuncEnter();
    return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Write(const void *pvBuffer, ULONG nBytesToRead, ULONG *nBytesRead)
{
    RT_NOREF(pvBuffer, nBytesToRead, nBytesRead);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

/*
 * Own stuff.
 */

/**
 * Factory to create our own IStream implementation.
 *
 * @returns HRESULT
 * @param   pParent             Pointer to the parent data object.
 * @param   pTransfer           Pointer to URI transfer object to use.
 * @param   uObjIdx             Index of object to handle within the given URI transfer object.
 * @param   ppStream            Where to return the created stream object on success.
 */
/* static */
HRESULT VBoxClipboardWinStreamImpl::Create(VBoxClipboardWinDataObject *pParent,
                                           PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uObjIdx, IStream **ppStream)
{
    AssertPtrReturn(pTransfer, E_POINTER);

    VBoxClipboardWinStreamImpl *pStream = new VBoxClipboardWinStreamImpl(pParent, pTransfer, uObjIdx);
    if (pStream)
    {
        pStream->AddRef();

        *ppStream = pStream;
        return S_OK;
    }

    return E_FAIL;
}

