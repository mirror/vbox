/* $Id$ */
/** @file
 * ClipboardStreamImpl-win.cpp - Shared Clipboard IStream object implementation (for CF_HDROP).
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



VBoxClipboardWinStreamImpl::VBoxClipboardWinStreamImpl(SharedClipboardProvider *pProvider, SharedClipboardURIObject *pURIObj)
    : m_lRefCount(1)
    , m_pProvider(pProvider)
    , m_pURIObj(pURIObj)
{
    AssertPtr(m_pProvider);
    AssertPtr(m_pURIObj);

    LogFunc(("szSrcPath=%s, cbSize=%RU64\n", m_pURIObj->GetSourcePathAbs().c_str(), m_pURIObj->GetSize()));

    m_pProvider->AddRef();
}

VBoxClipboardWinStreamImpl::~VBoxClipboardWinStreamImpl(void)
{
    LogFlowThisFuncEnter();
    m_pProvider->Release();
}

/*
 * IUnknown methods.
 */

STDMETHODIMP VBoxClipboardWinStreamImpl::QueryInterface(REFIID iid, void ** ppvObject)
{
    AssertPtrReturn(ppvObject, E_INVALIDARG);

    if (   iid == IID_IStream
        || iid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
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
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Read(void *pvBuffer, ULONG nBytesToRead, ULONG *nBytesRead)
{
   LogFlowThisFuncEnter();

    AssertPtr(m_pURIObj);
    if (m_pURIObj->IsComplete())
    {
        /* There can be 0-byte files. */
        AssertMsg(m_pURIObj->GetSize() == 0, ("Object is complete -- can't read from it anymore\n"));
        if (nBytesRead)
            *nBytesRead = 0; /** @todo If the file size is 0, already return at least 1 byte, else the whole operation will fail. */
        return S_OK; /* Don't report any failures back to Windows. */
    }

    const uint64_t cbSize      = m_pURIObj->GetSize();
    const uint64_t cbProcessed = m_pURIObj->GetProcessed();

    const size_t cbToRead = RT_MIN(cbSize - cbProcessed, nBytesToRead);
          size_t cbRead   = 0;

    int rc = VINF_SUCCESS;

    if (cbToRead)
        rc = m_pProvider->ReadData(pvBuffer, cbToRead, &cbRead);

    if (nBytesRead)
        *nBytesRead = (ULONG)cbRead;

    LogFlowThisFunc(("rc=%Rrc, cbSize=%RU64, cbProcessed=%RU64 -> cbToRead=%zu, cbRead=%zu\n",
                     rc, cbSize, cbProcessed, cbToRead, cbRead));
    return RT_SUCCESS(rc) ? S_OK : E_FAIL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Revert(void)
{
    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Seek(LARGE_INTEGER nMove, DWORD dwOrigin, ULARGE_INTEGER* nNewPos)
{
    RT_NOREF(nMove, dwOrigin, nNewPos);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::SetSize(ULARGE_INTEGER nNewSize)
{
    RT_NOREF(nNewSize);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Stat(STATSTG *statstg, DWORD dwFlags)
{
    RT_NOREF(statstg, dwFlags);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::UnlockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags)
{
    RT_NOREF(nStart, nBytes, dwFlags);

    LogFlowThisFuncEnter();
    return E_NOTIMPL;
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
 * @param   pProvider           Pointer to Shared Clipboard provider to use.
 * @param   pURIObj             Pointer to URI object to handle.
 * @param   ppStream            Where to return the created stream object on success.
 */
/* static */
HRESULT VBoxClipboardWinStreamImpl::Create(SharedClipboardProvider *pProvider, SharedClipboardURIObject *pURIObj,
                                           IStream **ppStream)
{
    AssertPtrReturn(pProvider, E_POINTER);

    VBoxClipboardWinStreamImpl *pStream = new VBoxClipboardWinStreamImpl(pProvider, pURIObj);
    if (pStream)
    {
        pStream->AddRef();

        *ppStream = pStream;
        return S_OK;
    }

    return E_FAIL;
}

