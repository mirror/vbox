/* $Id$ */
/** @file
 * VBoxDnDEnumFormat.cpp - IEnumFORMATETC ("Format et cetera") implementation.
 */

/*
 * Copyright (C) 2013-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/win/windows.h>
#include <new> /* For bad_alloc. */

#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxDnD.h"

#ifdef DEBUG
# define LOG_ENABLED
# define LOG_GROUP LOG_GROUP_DEFAULT
#endif
#include <VBox/log.h>



VBoxDnDEnumFormatEtc::VBoxDnDEnumFormatEtc(LPFORMATETC pFormatEtc, ULONG cFormats)
    : m_cRefs(1)
    , m_uIdxCur(0)
    , m_cFormats(0)
    , m_paFormatEtc(NULL)

{
    LogFlowFunc(("pFormatEtc=%p, cFormats=%RU32\n", pFormatEtc, cFormats));
    /** @todo r=bird: Use an init() function! */
    m_paFormatEtc = (LPFORMATETC)RTMemAllocZ(sizeof(*m_paFormatEtc) * cFormats);
    if (m_paFormatEtc)
    {
        for (ULONG i = 0; i < cFormats; i++)
        {
            LogFlowFunc(("Format %RU32: cfFormat=%RI16, sFormat=%s, tyMed=%RU32, dwAspect=%RU32\n",
                         i, pFormatEtc[i].cfFormat, VBoxDnDDataObject::ClipboardFormatToString(pFormatEtc[i].cfFormat),
                         pFormatEtc[i].tymed, pFormatEtc[i].dwAspect));
            VBoxDnDEnumFormatEtc::CopyFormat(&m_paFormatEtc[i], &pFormatEtc[i]);
        }
        m_cFormats = cFormats;
    }
    else
        Log(("Failed to allocate memory for %u formats!\n"));
}

VBoxDnDEnumFormatEtc::~VBoxDnDEnumFormatEtc(void)
{
    if (m_paFormatEtc)
    {
        for (ULONG i = 0; i < m_cFormats; i++)
            if (m_paFormatEtc[i].ptd)
            {
                CoTaskMemFree(m_paFormatEtc[i].ptd);
                m_paFormatEtc[i].ptd = NULL;
            }

        RTMemFree(m_paFormatEtc);
        m_paFormatEtc = NULL;
    }

    LogFlowFunc(("m_lRefCount=%RI32\n", m_cRefs));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) VBoxDnDEnumFormatEtc::AddRef(void)
{
    return InterlockedIncrement(&m_cRefs);
}

STDMETHODIMP_(ULONG) VBoxDnDEnumFormatEtc::Release(void)
{
    LONG lCount = InterlockedDecrement(&m_cRefs);
    if (lCount == 0)
    {
        delete this;
        return 0;
    }

    return lCount;
}

STDMETHODIMP VBoxDnDEnumFormatEtc::QueryInterface(REFIID iid, void **ppvObject)
{
    if (   iid == IID_IEnumFORMATETC
        || iid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
}

STDMETHODIMP VBoxDnDEnumFormatEtc::Next(ULONG cFormats, LPFORMATETC pFormatEtc, ULONG *pcFetched)
{
    ULONG ulCopied  = 0;

    if(cFormats == 0 || pFormatEtc == 0)
        return E_INVALIDARG;

    while (   m_uIdxCur < m_cFormats
           && ulCopied < cFormats)
    {
        VBoxDnDEnumFormatEtc::CopyFormat(&pFormatEtc[ulCopied],
                                         &m_paFormatEtc[m_uIdxCur]);
        ulCopied++;
        m_uIdxCur++;
    }

    if (pcFetched)
        *pcFetched = ulCopied;

    return (ulCopied == cFormats) ? S_OK : S_FALSE;
}

STDMETHODIMP VBoxDnDEnumFormatEtc::Skip(ULONG cFormats)
{
    m_uIdxCur += cFormats;
    return (m_uIdxCur <= m_cFormats) ? S_OK : S_FALSE;
}

STDMETHODIMP VBoxDnDEnumFormatEtc::Reset(void)
{
    m_uIdxCur = 0;
    return S_OK;
}

STDMETHODIMP VBoxDnDEnumFormatEtc::Clone(IEnumFORMATETC **ppEnumFormatEtc)
{
    HRESULT hrc = CreateEnumFormatEtc(m_cFormats, m_paFormatEtc, ppEnumFormatEtc);

    if (hrc == S_OK)
        ((VBoxDnDEnumFormatEtc *) *ppEnumFormatEtc)->m_uIdxCur = m_uIdxCur;

    return hrc;
}

/* static */
void VBoxDnDEnumFormatEtc::CopyFormat(LPFORMATETC pDest, LPFORMATETC pSource)
{
    AssertPtrReturnVoid(pDest);
    AssertPtrReturnVoid(pSource);

    *pDest = *pSource;

    if (pSource->ptd)
    {
        pDest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
        *(pDest->ptd) = *(pSource->ptd);
    }
}

/* static */
HRESULT VBoxDnDEnumFormatEtc::CreateEnumFormatEtc(UINT nNumFormats, LPFORMATETC pFormatEtc, IEnumFORMATETC **ppEnumFormatEtc)
{
    AssertReturn(nNumFormats, E_INVALIDARG);
    AssertPtrReturn(pFormatEtc, E_INVALIDARG);
    AssertPtrReturn(ppEnumFormatEtc, E_INVALIDARG);

#ifdef RT_EXCEPTIONS_ENABLED
    try { *ppEnumFormatEtc = new VBoxDnDEnumFormatEtc(pFormatEtc, nNumFormats); }
    catch (std::bad_alloc &)
#else
    *ppEnumFormatEtc = new VBoxDnDEnumFormatEtc(pFormatEtc, nNumFormats);
    if (!*ppEnumFormatEtc)
#endif
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

