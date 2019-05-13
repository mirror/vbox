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

#include <VBox/err.h>
#include <VBox/log.h>

VBoxClipboardWinDataObject::VBoxClipboardWinDataObject(LPFORMATETC pFormatEtc, LPSTGMEDIUM pStgMed, ULONG cFormats)
    : mStatus(Uninitialized)
    , mRefCount(0)
    , mcFormats(0)
    , muClientID(0)
{
    HRESULT hr;

    const ULONG cFixedFormats = 3; /* CFSTR_FILEDESCRIPTORA + CFSTR_FILEDESCRIPTORW + CFSTR_FILECONTENTS */
    const ULONG cAllFormats   = cFormats + cFixedFormats;

    try
    {
        mpFormatEtc = new FORMATETC[cAllFormats];
        RT_BZERO(mpFormatEtc, sizeof(FORMATETC) * cAllFormats);
        mpStgMedium = new STGMEDIUM[cAllFormats];
        RT_BZERO(mpStgMedium, sizeof(STGMEDIUM) * cAllFormats);

        /** @todo Do we need CFSTR_FILENAME / CFSTR_SHELLIDLIST here? */

        /*
         * Register fixed formats.
         */

        /* IStream interface, implemented in ClipboardStreamImpl-win.cpp. */
        registerFormat(&mpFormatEtc[FormatIndex_FileDescriptorA],
                       RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA));
        registerFormat(&mpFormatEtc[FormatIndex_FileDescriptorW],
                       RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW));
        registerFormat(&mpFormatEtc[FormatIndex_FileContents],
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
                mpFormatEtc[cFixedFormats + i] = pFormatEtc[i];
                mpStgMedium[cFixedFormats + i] = pStgMed[i];
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
        mcFormats = cAllFormats;
        mStatus   = Initialized;
    }

    LogFlowFunc(("cAllFormats=%RU32, hr=%Rhrc\n", cAllFormats, hr));
}

VBoxClipboardWinDataObject::~VBoxClipboardWinDataObject(void)
{
    if (mpStream)
        mpStream->Release();

    if (mpFormatEtc)
        delete[] mpFormatEtc;

    if (mpStgMedium)
        delete[] mpStgMedium;

    LogFlowFunc(("mRefCount=%RI32\n", mRefCount));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) VBoxClipboardWinDataObject::AddRef(void)
{
    LONG lCount = InterlockedIncrement(&mRefCount);
    LogFlowFunc(("lCount=%RI32\n", lCount));
    return lCount;
}

STDMETHODIMP_(ULONG) VBoxClipboardWinDataObject::Release(void)
{
    LONG lCount = InterlockedDecrement(&mRefCount);
    LogFlowFunc(("lCount=%RI32\n", mRefCount));
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

int VBoxClipboardWinDataObject::createFileGroupDescriptor(const SharedClipboardURIList &URIList, HGLOBAL *phGlobal)
{
//    AssertReturn(URIList.GetRootCount(), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phGlobal, VERR_INVALID_POINTER);

    int rc;

    const size_t cItems = 2;  URIList.GetRootCount();
    const size_t cbFGD  = sizeof(FILEGROUPDESCRIPTOR) + sizeof(FILEDESCRIPTOR) * (cItems - 1);

    LogFunc(("cItmes=%zu\n", cItems));

    FILEGROUPDESCRIPTOR *pFGD = (FILEGROUPDESCRIPTOR *)RTMemAlloc(cbFGD);
    if (pFGD)
    {
        pFGD->cItems = (UINT)cItems;


        FILEDESCRIPTOR *pFD = &pFGD->fgd[0];
        RT_BZERO(pFD, sizeof(FILEDESCRIPTOR));

        RTStrPrintf(pFD->cFileName, sizeof(pFD->cFileName), "barbaz.txt\n");

    #if 1
        pFD->dwFlags          = FD_ATTRIBUTES | FD_FILESIZE | FD_PROGRESSUI;
        pFD->dwFileAttributes = FILE_ATTRIBUTE_NORMAL; // FILE_ATTRIBUTE_DIRECTORY;

        uint64_t cbSize = _1M;

        pFD->nFileSizeHigh    = RT_HI_U32(cbSize);
        pFD->nFileSizeLow     = RT_LO_U32(cbSize);
    #else
        pFD->dwFlags = FD_ATTRIBUTES | FD_CREATETIME | FD_ACCESSTIME | FD_WRITESTIME | FD_FILESIZE;
        pFD->dwFileAttributes =
        pFD->ftCreationTime   =
        pFD->ftLastAccessTime =
        pFD->ftLastWriteTime  =
        pFD->nFileSizeHigh    =
        pFD->nFileSizeLow     =
    #endif



        pFD = &pFGD->fgd[1];
        RT_BZERO(pFD, sizeof(FILEDESCRIPTOR));

        RTStrPrintf(pFD->cFileName, sizeof(pFD->cFileName), "barbaz_dir\n");

    #if 1
        pFD->dwFlags          = FD_ATTRIBUTES | FD_PROGRESSUI;
        pFD->dwFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY;
    #else
        pFD->dwFlags = FD_ATTRIBUTES | FD_CREATETIME | FD_ACCESSTIME | FD_WRITESTIME | FD_FILESIZE;
        pFD->dwFileAttributes =
        pFD->ftCreationTime   =
        pFD->ftLastAccessTime =
        pFD->ftLastWriteTime  =
        pFD->nFileSizeHigh    =
        pFD->nFileSizeLow     =
    #endif


        rc = copyToHGlobal(pFGD, cbFGD, GMEM_MOVEABLE, phGlobal);
    }
    else
        rc = VERR_NO_MEMORY;

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
    if (lIndex >= mcFormats) /* Paranoia. */
        return DV_E_LINDEX;

    LPFORMATETC pThisFormat = &mpFormatEtc[lIndex];
    AssertPtr(pThisFormat);

    LPSTGMEDIUM pThisMedium = &mpStgMedium[lIndex];
    AssertPtr(pThisMedium);

    LogFlowFunc(("Using pThisFormat=%p, pThisMedium=%p\n", pThisFormat, pThisMedium));

    HRESULT hr = DV_E_FORMATETC; /* Play safe. */

    LogRel3(("Clipboard: cfFormat=%RI16, sFormat=%s, tyMed=%RU32, dwAspect=%RU32 -> lIndex=%u\n",
             pThisFormat->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(pFormatEtc->cfFormat),
             pThisFormat->tymed, pThisFormat->dwAspect, lIndex));

    /*
     * Initialize default values.
     */
    pMedium->tymed          = pThisFormat->tymed;
    pMedium->pUnkForRelease = NULL; /* Caller is responsible for deleting the data. */

    switch (lIndex)
    {
        case FormatIndex_FileDescriptorA:
        {
            LogFlowFunc(("FormatIndex_FileDescriptorA\n"));

            SharedClipboardURIList mURIList;
           // mURIList.AppendURIPath()

            HGLOBAL hGlobal;
            int rc = createFileGroupDescriptor(mURIList, &hGlobal);
            if (RT_SUCCESS(rc))
            {
                pMedium->tymed   = TYMED_HGLOBAL;
                pMedium->hGlobal = hGlobal;

                hr = S_OK;
            }
            break;
        }

        case FormatIndex_FileDescriptorW:
            LogFlowFunc(("FormatIndex_FileDescriptorW\n"));
            break;

        case FormatIndex_FileContents:
        {
            LogFlowFunc(("FormatIndex_FileContents\n"));

            hr = VBoxClipboardWinStreamImpl::Create(&mpStream);
            if (SUCCEEDED(hr))
            {
                /* Hand over the stream to the caller. */
                pMedium->tymed          = TYMED_ISTREAM;
                pMedium->pstm           = mpStream;
            }

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
    LogFlowFunc(("dwDirection=%RI32, mcFormats=%RI32, mpFormatEtc=%p\n", dwDirection, mcFormats, mpFormatEtc));

    HRESULT hr;
    if (dwDirection == DATADIR_GET)
        hr = VBoxClipboardWinEnumFormatEtc::CreateEnumFormatEtc(mcFormats, mpFormatEtc, ppEnumFormatEtc);
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

int VBoxClipboardWinDataObject::Init(uint32_t idClient)
{
    muClientID = idClient;

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

    for (ULONG i = 0; i < mcFormats; i++)
    {
        if(    (pFormatEtc->tymed & mpFormatEtc[i].tymed)
            && pFormatEtc->cfFormat == mpFormatEtc[i].cfFormat
            && pFormatEtc->dwAspect == mpFormatEtc[i].dwAspect)
        {
            LogRel3(("Clipboard: Format found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32, ulIndex=%RU32\n",
                      pFormatEtc->tymed, pFormatEtc->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(mpFormatEtc[i].cfFormat),
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

