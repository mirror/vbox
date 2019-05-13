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



VBoxClipboardWinStreamImpl::VBoxClipboardWinStreamImpl(void)
    : m_lRefCount(1)
{

}

VBoxClipboardWinStreamImpl::~VBoxClipboardWinStreamImpl(void)
{
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

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::CopyTo(IStream* pDestStream, ULARGE_INTEGER nBytesToCopy, ULARGE_INTEGER* nBytesRead,
                                                ULARGE_INTEGER* nBytesWritten)
{
    RT_NOREF(pDestStream, nBytesToCopy, nBytesRead, nBytesWritten);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::LockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes,DWORD dwFlags)
{
    RT_NOREF(nStart, nBytes, dwFlags);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

static ULONG cbFileSize = _1M;
static ULONG cbSizeRead = 0;

STDMETHODIMP VBoxClipboardWinStreamImpl::Read(void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead)
{
    /* If the file size is 0, already return at least 1 byte, else the whole operation will fail. */

    ULONG cbToRead = RT_MIN(cbFileSize - cbSizeRead,  _4K /* nBytesToRead */);

    if (cbToRead > nBytesToRead)
        cbToRead = nBytesToRead;

    LogFlowFunc(("pvBuffer=%p, nBytesToRead=%u -> cbSizeRead=%u, cbToRead=%u\n", pvBuffer, nBytesToRead, cbSizeRead, cbToRead));

    if (cbToRead)
    {
        memset(pvBuffer, cbToRead, 0x65);
        cbSizeRead += cbToRead;
    }

    if (nBytesRead)
        *nBytesRead = cbToRead;

    if (cbSizeRead == cbFileSize)
        cbSizeRead = 0;

    return S_OK;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Revert(void)
{
    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Seek(LARGE_INTEGER nMove, DWORD dwOrigin, ULARGE_INTEGER* nNewPos)
{
    RT_NOREF(nMove, dwOrigin, nNewPos);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::SetSize(ULARGE_INTEGER nNewSize)
{
    RT_NOREF(nNewSize);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Stat(STATSTG* statstg, DWORD dwFlags)
{
    RT_NOREF(statstg, dwFlags);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::UnlockRegion(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags)
{
    RT_NOREF(nStart, nBytes, dwFlags);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinStreamImpl::Write(const void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead)
{
    RT_NOREF(pvBuffer, nBytesToRead, nBytesRead);

    LogFlowFuncEnter();
    return E_NOTIMPL;
}

/*
 * Own stuff.
 */

/* static */
HRESULT VBoxClipboardWinStreamImpl::Create(IStream **ppStream)
{
    VBoxClipboardWinStreamImpl *pStream = new VBoxClipboardWinStreamImpl();
    if (pStream)
    {
        pStream->AddRef();

        *ppStream = pStream;
        return S_OK;
    }

    return E_FAIL;
}

