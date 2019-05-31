/* $Id$ */
/** @file
 * ClipboardDataObjectImpl-win.cpp - Shared Clipboard IDataObject implementation.
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
#include <VBox/GuestHost/SharedClipboard-uri.h>

/** !!! HACK ALERT !!! Dynamically resolve functions! */
#ifdef _WIN32_IE
#undef _WIN32_IE
#define _WIN32_IE 0x0501
#endif

#include <iprt/win/windows.h>
#include <iprt/win/shlobj.h>
#include <iprt/win/shlwapi.h>

#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/uri.h>
#include <iprt/utf16.h>

#include <iprt/errcore.h>
#include <VBox/log.h>

/** Also handle Unicode entries. */
#define VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT 1

VBoxClipboardWinDataObject::VBoxClipboardWinDataObject(SharedClipboardProvider *pProvider,
                                                       LPFORMATETC pFormatEtc, LPSTGMEDIUM pStgMed, ULONG cFormats)
    : m_enmStatus(Uninitialized)
    , m_lRefCount(0)
    , m_cFormats(0)
    , m_pProvider(pProvider)
    , m_pStream(NULL)
    , m_uObjIdx(0)
{
    AssertPtr(pProvider);

    HRESULT hr;

    ULONG cFixedFormats = 2; /* CFSTR_FILEDESCRIPTORA + CFSTR_FILECONTENTS */
#ifdef VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT
    cFixedFormats++; /* CFSTR_FILEDESCRIPTORW */
#endif
    const ULONG cAllFormats   = cFormats + cFixedFormats;

    try
    {
        m_pFormatEtc = new FORMATETC[cAllFormats];
        RT_BZERO(m_pFormatEtc, sizeof(FORMATETC) * cAllFormats);
        m_pStgMedium = new STGMEDIUM[cAllFormats];
        RT_BZERO(m_pStgMedium, sizeof(STGMEDIUM) * cAllFormats);

        /** @todo Do we need CFSTR_FILENAME / CFSTR_SHELLIDLIST here? */

        /*
         * Register fixed formats.
         */

        LogFlowFunc(("Registering CFSTR_FILEDESCRIPTORA ...\n"));
        registerFormat(&m_pFormatEtc[FormatIndex_FileDescriptorA],
                       RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA));
#ifdef VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT
        LogFlowFunc(("Registering CFSTR_FILEDESCRIPTORW ...\n"));
        registerFormat(&m_pFormatEtc[FormatIndex_FileDescriptorW],
                       RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW));
#endif
        /* IStream interface, implemented in ClipboardStreamImpl-win.cpp. */
        LogFlowFunc(("Registering CFSTR_FILECONTENTS ...\n"));
        registerFormat(&m_pFormatEtc[FormatIndex_FileContents],
                       RegisterClipboardFormat(CFSTR_FILECONTENTS),
                       TYMED_ISTREAM, 0 /* lIndex */);

        /*
         * Registration of dynamic formats needed?
         */
        LogFlowFunc(("%RU32 dynamic formats\n", cFormats));
        if (cFormats)
        {
            AssertPtr(pFormatEtc);
            AssertPtr(pStgMed);

            for (ULONG i = 0; i < cFormats; i++)
            {
                LogFlowFunc(("Format %RU32: cfFormat=%RI16, tyMed=%RU32, dwAspect=%RU32\n",
                             i, pFormatEtc[i].cfFormat, pFormatEtc[i].tymed, pFormatEtc[i].dwAspect));
                m_pFormatEtc[cFixedFormats + i] = pFormatEtc[i];
                m_pStgMedium[cFixedFormats + i] = pStgMed[i];
            }
        }

        hr = S_OK;
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        m_cFormats  = cAllFormats;
        m_enmStatus = Initialized;

        m_pProvider->AddRef();
    }

    LogFlowFunc(("cAllFormats=%RU32, hr=%Rhrc\n", cAllFormats, hr));
}

VBoxClipboardWinDataObject::~VBoxClipboardWinDataObject(void)
{
    if (m_pProvider)
        m_pProvider->Release();

    if (m_pStream)
        m_pStream->Release();

    if (m_pFormatEtc)
        delete[] m_pFormatEtc;

    if (m_pStgMedium)
        delete[] m_pStgMedium;

    LogFlowFunc(("mRefCount=%RI32\n", m_lRefCount));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) VBoxClipboardWinDataObject::AddRef(void)
{
    LONG lCount = InterlockedIncrement(&m_lRefCount);
    LogFlowFunc(("lCount=%RI32\n", lCount));
    return lCount;
}

STDMETHODIMP_(ULONG) VBoxClipboardWinDataObject::Release(void)
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

STDMETHODIMP VBoxClipboardWinDataObject::QueryInterface(REFIID iid, void **ppvObject)
{
    AssertPtrReturn(ppvObject, E_INVALIDARG);

    if (   iid == IID_IDataObject
        || iid == IID_IUnknown)
    {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = 0;
    return E_NOINTERFACE;
}

/**
 * Copies a chunk of data into a HGLOBAL object.
 *
 * @returns VBox status code.
 * @param   pvData              Data to copy.
 * @param   cbData              Size (in bytes) to copy.
 * @param   fFlags              GlobalAlloc flags, used for allocating the HGLOBAL block.
 * @param   phGlobal            Where to store the allocated HGLOBAL object.
 */
int VBoxClipboardWinDataObject::copyToHGlobal(const void *pvData, size_t cbData, UINT fFlags, HGLOBAL *phGlobal)
{
    AssertPtrReturn(phGlobal, VERR_INVALID_POINTER);

    HGLOBAL hGlobal = GlobalAlloc(fFlags, cbData);
    if (!hGlobal)
        return VERR_NO_MEMORY;

    void *pvAlloc = GlobalLock(hGlobal);
    if (pvAlloc)
    {
        CopyMemory(pvAlloc, pvData, cbData);
        GlobalUnlock(hGlobal);

        *phGlobal = hGlobal;

        return VINF_SUCCESS;
    }

    GlobalFree(hGlobal);
    return VERR_ACCESS_DENIED;
}

/**
 * Creates a FILEGROUPDESCRIPTOR object from a given Shared Clipboard URI list and stores
 * the result into an HGLOBAL object.
 *
 * @returns VBox status code.
 * @param   URIList             URI list to create object for.
 * @param   fUnicode            Whether the FILEGROUPDESCRIPTOR object shall contain Unicode data or not.
 * @param   phGlobal            Where to store the allocated HGLOBAL object on success.
 */
int VBoxClipboardWinDataObject::createFileGroupDescriptorFromURIList(const SharedClipboardURIList &URIList,
                                                                     bool fUnicode, HGLOBAL *phGlobal)
{
    AssertReturn(URIList.GetRootCount(), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phGlobal,            VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    const size_t cbFileGroupDescriptor = fUnicode ? sizeof(FILEGROUPDESCRIPTORW) : sizeof(FILEGROUPDESCRIPTORA);
    const size_t cbFileDescriptor = fUnicode ? sizeof(FILEDESCRIPTORW) : sizeof(FILEDESCRIPTORA);

    const UINT   cItems = (UINT)URIList.GetRootCount(); /** @todo UINT vs. uint64_t */
    const size_t cbFGD  = cbFileGroupDescriptor + (cbFileDescriptor * (cItems - 1));

    LogFunc(("fUnicode=%RTbool, cItems=%u, cbFileDescriptor=%zu\n", fUnicode, cItems, cbFileDescriptor));

    /* FILEGROUPDESCRIPTORA / FILEGROUPDESCRIPTOR matches except the cFileName member (TCHAR vs. WCHAR). */
    FILEGROUPDESCRIPTOR *pFGD = (FILEGROUPDESCRIPTOR *)RTMemAlloc(cbFGD);
    if (!pFGD)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    pFGD->cItems = cItems;

    char *pszFileSpec = NULL;

    for (UINT i = 0; i < cItems; i++)
    {
        FILEDESCRIPTOR *pFD = &pFGD->fgd[i];
        RT_BZERO(pFD, cbFileDescriptor);

        const SharedClipboardURIObject *pObj = URIList.At(i);
        AssertPtr(pObj);
        const char *pszFile = pObj->GetSourcePathAbs().c_str();
        AssertPtr(pszFile);

        pszFileSpec = RTStrDup(pszFile);
        AssertBreakStmt(pszFileSpec != NULL, rc = VERR_NO_MEMORY);

        if (fUnicode)
        {
            PRTUTF16 pwszFileSpec;
            rc = RTStrToUtf16(pszFileSpec, &pwszFileSpec);
            if (RT_SUCCESS(rc))
            {
                rc = RTUtf16CopyEx((PRTUTF16 )pFD->cFileName, sizeof(pFD->cFileName) / sizeof(WCHAR),
                                   pwszFileSpec, RTUtf16Len(pwszFileSpec));
                RTUtf16Free(pwszFileSpec);
            }
        }
        else
            rc = RTStrCopy(pFD->cFileName, sizeof(pFD->cFileName), pszFileSpec);

        RTStrFree(pszFileSpec);
        pszFileSpec = NULL;

        if (RT_FAILURE(rc))
            break;

        pFD->dwFlags          = FD_PROGRESSUI | FD_ATTRIBUTES;
        if (fUnicode) /** @todo Only >= Vista. */
            pFD->dwFlags     |= FD_UNICODE;
        pFD->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

        switch (pObj->GetType())
        {
            case SharedClipboardURIObject::Type_Directory:
                pFD->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

                LogFunc(("pszDir=%s\n", pszFile));
                break;

            case SharedClipboardURIObject::Type_File:
            {
                pFD->dwFlags |= FD_FILESIZE;

                const uint64_t cbObjSize = pObj->GetSize();

                pFD->nFileSizeHigh = RT_HI_U32(cbObjSize);
                pFD->nFileSizeLow  = RT_LO_U32(cbObjSize);

                LogFunc(("pszFile=%s, cbObjSize=%RU64\n", pszFile, cbObjSize));
                break;
            }

            default:
                AssertFailed();
                break;
        }
#if 0
        pFD->dwFlags = FD_ATTRIBUTES | FD_CREATETIME | FD_ACCESSTIME | FD_WRITESTIME | FD_FILESIZE; /** @todo Implement this. */
        pFD->dwFileAttributes =
        pFD->ftCreationTime   =
        pFD->ftLastAccessTime =
        pFD->ftLastWriteTime  =
#endif
    }

    if (pszFileSpec)
        RTStrFree(pszFileSpec);

    if (RT_SUCCESS(rc))
    {
        rc = copyToHGlobal(pFGD, cbFGD, GMEM_MOVEABLE, phGlobal);
    }
    else
    {
        RTMemFree(pFGD);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Retrieves the data stored in this object and store the result in
 * pMedium.
 *
 * @return  IPRT status code.
 * @return  HRESULT
 * @param   pFormatEtc
 * @param   pMedium
 */
STDMETHODIMP VBoxClipboardWinDataObject::GetData(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium)
{
    AssertPtrReturn(pFormatEtc, DV_E_FORMATETC);
    AssertPtrReturn(pMedium, DV_E_FORMATETC);

    LogFlowFuncEnter();

    ULONG lIndex;
    if (!lookupFormatEtc(pFormatEtc, &lIndex)) /* Format supported? */
        return DV_E_FORMATETC;
    if (lIndex >= m_cFormats) /* Paranoia. */
        return DV_E_LINDEX;

    LPFORMATETC pThisFormat = &m_pFormatEtc[lIndex];
    AssertPtr(pThisFormat);

    LPSTGMEDIUM pThisMedium = &m_pStgMedium[lIndex];
    AssertPtr(pThisMedium);

    LogFlowFunc(("Using pThisFormat=%p, pThisMedium=%p\n", pThisFormat, pThisMedium));

    HRESULT hr = DV_E_FORMATETC; /* Play safe. */

    LogRel2(("Clipboard: cfFormat=%RI16, sFormat=%s, tyMed=%RU32, dwAspect=%RU32 -> lIndex=%u\n",
             pThisFormat->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(pFormatEtc->cfFormat),
             pThisFormat->tymed, pThisFormat->dwAspect, lIndex));

    /*
     * Initialize default values.
     */
    pMedium->tymed          = pThisFormat->tymed;
    pMedium->pUnkForRelease = NULL; /* Caller is responsible for deleting the data. */

    switch (lIndex)
    {
        case FormatIndex_FileDescriptorA: /* ANSI */
#ifdef VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT
            RT_FALL_THROUGH();
        case FormatIndex_FileDescriptorW: /* Unicode */
#endif
        {
            const bool fUnicode = lIndex == FormatIndex_FileDescriptorW;

            LogFlowFunc(("FormatIndex_FileDescriptor%s\n", fUnicode ? "W" : "A"));

            int rc = m_pProvider->ReadMetaData(); /** @todo Do this asynchronously some time earlier? */
            if (   RT_SUCCESS(rc)
                && !m_pProvider->GetURIList().IsEmpty())
            {
                HGLOBAL hGlobal;
                rc = createFileGroupDescriptorFromURIList(m_pProvider->GetURIList(), fUnicode, &hGlobal);
                if (RT_SUCCESS(rc))
                {
                    pMedium->tymed   = TYMED_HGLOBAL;
                    pMedium->hGlobal = hGlobal;
                    /* Note: hGlobal now is being owned by pMedium / the caller. */

                    hr = S_OK;
                }
            }
            break;
        }

        case FormatIndex_FileContents:
        {
            LogFlowFunc(("FormatIndex_FileContents: m_uObjIdx=%u\n", m_uObjIdx));

            SharedClipboardURIObject *pURIObj = m_pProvider->GetURIList().At(m_uObjIdx);
            if (pURIObj)
            {
                /* Hand-in the provider so that our IStream implementation can continue working with it. */
                hr = VBoxClipboardWinStreamImpl::Create(m_pProvider, pURIObj, &m_pStream);
                if (SUCCEEDED(hr))
                {
                    /* Hand over the stream to the caller. */
                    pMedium->tymed = TYMED_ISTREAM;
                    pMedium->pstm  = m_pStream;

                    /* Handle next object. */
                    m_uObjIdx++;
                }
            }
            else
                LogFunc(("No object with index %u found, skipping\n", m_uObjIdx));
            break;
        }

        default:
            break;
    }

    /* Error handling; at least return some basic data. */
    if (FAILED(hr))
    {
        LogFunc(("Failed; copying medium ...\n"));

        pMedium->tymed          = pThisFormat->tymed;
        pMedium->pUnkForRelease = NULL;
    }

    if (hr == DV_E_FORMATETC)
        LogRel(("Clipboard: Error handling format\n"));

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
}

/**
 * Only required for IStream / IStorage interfaces.
 *
 * @return  IPRT status code.
 * @return  HRESULT
 * @param   pFormatEtc
 * @param   pMedium
 */
STDMETHODIMP VBoxClipboardWinDataObject::GetDataHere(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium)
{
    RT_NOREF(pFormatEtc, pMedium);
    LogFlowFunc(("\n"));
    return E_NOTIMPL;
}

/**
 * Query if this objects supports a specific format.
 *
 * @return  IPRT status code.
 * @return  HRESULT
 * @param   pFormatEtc
 */
STDMETHODIMP VBoxClipboardWinDataObject::QueryGetData(LPFORMATETC pFormatEtc)
{
    LogFlowFunc(("\n"));
    return (lookupFormatEtc(pFormatEtc, NULL /* puIndex */)) ? S_OK : DV_E_FORMATETC;
}

STDMETHODIMP VBoxClipboardWinDataObject::GetCanonicalFormatEtc(LPFORMATETC pFormatEtc, LPFORMATETC pFormatEtcOut)
{
    RT_NOREF(pFormatEtc);
    LogFlowFunc(("\n"));

    /* Set this to NULL in any case. */
    pFormatEtcOut->ptd = NULL;
    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::SetData(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium, BOOL fRelease)
{
    RT_NOREF(pFormatEtc, pMedium, fRelease);
    LogFlowFunc(("\n"));

    return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc)
{
    LogFlowFunc(("dwDirection=%RI32, mcFormats=%RI32, mpFormatEtc=%p\n", dwDirection, m_cFormats, m_pFormatEtc));

    HRESULT hr;
    if (dwDirection == DATADIR_GET)
        hr = VBoxClipboardWinEnumFormatEtc::CreateEnumFormatEtc(m_cFormats, m_pFormatEtc, ppEnumFormatEtc);
    else
        hr = E_NOTIMPL;

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
}

STDMETHODIMP VBoxClipboardWinDataObject::DAdvise(LPFORMATETC pFormatEtc, DWORD fAdvise, IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
    RT_NOREF(pFormatEtc, fAdvise, pAdvSink, pdwConnection);
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP VBoxClipboardWinDataObject::DUnadvise(DWORD dwConnection)
{
    RT_NOREF(dwConnection);
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP VBoxClipboardWinDataObject::EnumDAdvise(IEnumSTATDATA **ppEnumAdvise)
{
    RT_NOREF(ppEnumAdvise);
    return OLE_E_ADVISENOTSUPPORTED;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_WIN_ASYNC
/*
 * IDataObjectAsyncCapability methods.
 */

STDMETHODIMP VBoxClipboardWinDataObject::EndOperation(HRESULT hResult, IBindCtx* pbcReserved, DWORD dwEffects)
{
     RT_NOREF(hResult, pbcReserved, dwEffects);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::GetAsyncMode(BOOL* pfIsOpAsync)
{
     RT_NOREF(pfIsOpAsync);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::InOperation(BOOL* pfInAsyncOp)
{
     RT_NOREF(pfInAsyncOp);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::SetAsyncMode(BOOL fDoOpAsync)
{
     RT_NOREF(fDoOpAsync);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::StartOperation(IBindCtx* pbcReserved)
{
     RT_NOREF(pbcReserved);
     return E_NOTIMPL;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_WIN_ASYNC */

/*
 * Own stuff.
 */

int VBoxClipboardWinDataObject::Init(void)
{
    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/* static */
const char* VBoxClipboardWinDataObject::ClipboardFormatToString(CLIPFORMAT fmt)
{
#if 0
    char szFormat[128];
    if (GetClipboardFormatName(fmt, szFormat, sizeof(szFormat)))
        LogFlowFunc(("wFormat=%RI16, szName=%s\n", fmt, szFormat));
#endif

    switch (fmt)
    {

    case 1:
        return "CF_TEXT";
    case 2:
        return "CF_BITMAP";
    case 3:
        return "CF_METAFILEPICT";
    case 4:
        return "CF_SYLK";
    case 5:
        return "CF_DIF";
    case 6:
        return "CF_TIFF";
    case 7:
        return "CF_OEMTEXT";
    case 8:
        return "CF_DIB";
    case 9:
        return "CF_PALETTE";
    case 10:
        return "CF_PENDATA";
    case 11:
        return "CF_RIFF";
    case 12:
        return "CF_WAVE";
    case 13:
        return "CF_UNICODETEXT";
    case 14:
        return "CF_ENHMETAFILE";
    case 15:
        return "CF_HDROP";
    case 16:
        return "CF_LOCALE";
    case 17:
        return "CF_DIBV5";
    case 18:
        return "CF_MAX";
    case 49158:
        return "FileName";
    case 49159:
        return "FileNameW";
    case 49161:
        return "DATAOBJECT";
    case 49171:
        return "Ole Private Data";
    case 49314:
        return "Shell Object Offsets";
    case 49316:
        return "File Contents";
    case 49317:
        return "File Group Descriptor";
    case 49323:
        return "Preferred Drop Effect";
    case 49380:
        return "Shell Object Offsets";
    case 49382:
        return "FileContents";
    case 49383:
        return "FileGroupDescriptor";
    case 49389:
        return "Preferred DropEffect";
    case 49268:
        return "Shell IDList Array";
    case 49619:
        return "RenPrivateFileAttachments";
    default:
        break;
    }

    return "unknown";
}

bool VBoxClipboardWinDataObject::lookupFormatEtc(LPFORMATETC pFormatEtc, ULONG *puIndex)
{
    AssertReturn(pFormatEtc, false);
    /* puIndex is optional. */

    for (ULONG i = 0; i < m_cFormats; i++)
    {
        if(    (pFormatEtc->tymed & m_pFormatEtc[i].tymed)
            && pFormatEtc->cfFormat == m_pFormatEtc[i].cfFormat)
            /* Note: Do *not* compare dwAspect here, as this can be dynamic, depending on how the object should be represented. */
            //&& pFormatEtc->dwAspect == m_pFormatEtc[i].dwAspect)
        {
            LogRel3(("Clipboard: Format found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32, ulIndex=%RU32\n",
                      pFormatEtc->tymed, pFormatEtc->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(m_pFormatEtc[i].cfFormat),
                      pFormatEtc->dwAspect, i));
            if (puIndex)
                *puIndex = i;
            return true;
        }
    }

    LogRel3(("Clipboard: Format NOT found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32\n",
             pFormatEtc->tymed, pFormatEtc->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(pFormatEtc->cfFormat),
             pFormatEtc->dwAspect));

    return false;
}

void VBoxClipboardWinDataObject::registerFormat(LPFORMATETC pFormatEtc, CLIPFORMAT clipFormat,
                                                TYMED tyMed, LONG lIndex, DWORD dwAspect,
                                                DVTARGETDEVICE *pTargetDevice)
{
    AssertPtr(pFormatEtc);

    pFormatEtc->cfFormat = clipFormat;
    pFormatEtc->tymed    = tyMed;
    pFormatEtc->lindex   = lIndex;
    pFormatEtc->dwAspect = dwAspect;
    pFormatEtc->ptd      = pTargetDevice;

    LogFlowFunc(("Registered format=%ld, sFormat=%s\n",
                 pFormatEtc->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(pFormatEtc->cfFormat)));
}

