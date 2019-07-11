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

#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/uri.h>
#include <iprt/utf16.h>

#include <iprt/errcore.h>
#include <VBox/log.h>

/** @todo Also handle Unicode entries.
 *        !!! WARNING: Buggy, doesn't work yet (some memory corruption / garbage in the file name descriptions) !!! */
//#define VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT 0

VBoxClipboardWinDataObject::VBoxClipboardWinDataObject(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                                       LPFORMATETC pFormatEtc, LPSTGMEDIUM pStgMed, ULONG cFormats)
    : m_enmStatus(Uninitialized)
    , m_lRefCount(0)
    , m_cFormats(0)
    , m_pTransfer(pTransfer)
    , m_pStream(NULL)
    , m_uObjIdx(0)
{
    AssertPtr(m_pTransfer);

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
        unsigned uIdx = 0;

        LogFlowFunc(("Registering CFSTR_FILEDESCRIPTORA ...\n"));
        m_cfFileDescriptorA = RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA);
        registerFormat(&m_pFormatEtc[uIdx++], m_cfFileDescriptorA);
#ifdef VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT
        LogFlowFunc(("Registering CFSTR_FILEDESCRIPTORW ...\n"));
        m_cfFileDescriptorW = RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
        registerFormat(&m_pFormatEtc[uIdx++], m_cfFileDescriptorW);
#endif

        /* IStream interface, implemented in ClipboardStreamImpl-win.cpp. */
        LogFlowFunc(("Registering CFSTR_FILECONTENTS ...\n"));
        m_cfFileContents = RegisterClipboardFormat(CFSTR_FILECONTENTS);
        registerFormat(&m_pFormatEtc[uIdx++], m_cfFileContents, TYMED_ISTREAM, 0 /* lIndex */);

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

        int rc2 = RTSemEventCreate(&m_EventListComplete);
        AssertRC(rc2);
        rc2 = RTSemEventCreate(&m_EventTransferComplete);
        AssertRC(rc2);
    }

    LogFlowFunc(("cAllFormats=%RU32, hr=%Rhrc\n", cAllFormats, hr));
}

VBoxClipboardWinDataObject::~VBoxClipboardWinDataObject(void)
{
    RTSemEventDestroy(m_EventListComplete);
    RTSemEventDestroy(m_EventTransferComplete);

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

/* static */

/**
 * Thread for reading URI data.
 * The data object needs the (high level, root) URI listing at the time of ::GetData(), so we need
 * to block and wait until we have this data (via this thread) and continue.
 *
 * @returns VBox status code.
 * @param   ThreadSelf          Thread handle. Unused at the moment.
 * @param   pvUser              Pointer to user-provided data. Of type VBoxClipboardWinDataObject.
 */
DECLCALLBACK(int) VBoxClipboardWinDataObject::readThread(RTTHREAD ThreadSelf, void *pvUser)
{
    RT_NOREF(ThreadSelf);

    LogFlowFuncEnter();

    VBoxClipboardWinDataObject *pThis = (VBoxClipboardWinDataObject *)pvUser;

    PSHAREDCLIPBOARDURITRANSFER pTransfer = pThis->m_pTransfer;
    AssertPtr(pTransfer);

    pTransfer->Thread.fStarted = true;

    RTThreadUserSignal(RTThreadSelf());

    LogRel2(("Shared Clipboard: Calculating transfer ....\n"));

    int rc = SharedClipboardURITransferOpen(pTransfer);
    if (RT_SUCCESS(rc))
    {
        uint32_t cRoots;
        char    *pszRoots;
        rc = SharedClipboardURILTransferGetRoots(pTransfer, &pszRoots, &cRoots);
        if (RT_SUCCESS(rc))
        {
            SharedClipboardURIListRootEntries lstRoots = RTCString(pszRoots, strlen(pszRoots)).split("\r\n");
            Assert(lstRoots.size() == cRoots);

            LogFlowFunc(("cRoots=%zu\n", lstRoots.size()));

            for (uint32_t i = 0; i < lstRoots.size(); i++)
            {
                VBOXCLIPBOARDLISTOPENPARMS openParmsList;
                rc = SharedClipboardURIListOpenParmsInit(&openParmsList);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("pszRoot=%s\n", lstRoots[i].c_str()));

                    rc = RTStrCopy(openParmsList.pszPath, openParmsList.cbPath, lstRoots[i].c_str());
                    if (RT_FAILURE(rc))
                        break;

                    SHAREDCLIPBOARDLISTHANDLE hList;
                    rc = SharedClipboardURITransferListOpen(pTransfer, &openParmsList, &hList);
                    if (RT_SUCCESS(rc))
                    {
                        LogFlowFunc(("hList=%RU64\n", hList));

                        VBOXCLIPBOARDLISTHDR hdrList;
                        rc = SharedClipboardURITransferListGetHeader(pTransfer, hList, &hdrList);
                        if (RT_SUCCESS(rc))
                        {
                            LogFlowFunc(("cTotalObjects=%RU64, cbTotalSize=%RU64\n\n",
                                         hdrList.cTotalObjects, hdrList.cbTotalSize));

                            for (uint64_t i = 0; i < hdrList.cTotalObjects; i++)
                            {
                                VBOXCLIPBOARDLISTENTRY entryList;
                                rc = SharedClipboardURITransferListRead(pTransfer, hList, &entryList);
                                if (RT_SUCCESS(rc))
                                {
                                    PSHAREDCLIPBOARDFSOBJINFO pObjInfo = (PSHAREDCLIPBOARDFSOBJINFO)entryList.pvInfo;
                                    Assert(entryList.cbInfo == sizeof(SHAREDCLIPBOARDFSOBJINFO));

                                    LogFlowFunc(("\t%s (%RU64 bytes)\n", entryList.pszName, pObjInfo->cbObject));

                                    FSOBJENTRY objEntry = { entryList.pszName, *pObjInfo };
                                    pThis->m_lstRootEntries.push_back(objEntry); /** @todo Can this throw? */
                                }
                                else
                                    break;

                                if (pTransfer->Thread.fStop)
                                    break;
                            }
                        }

                        SharedClipboardURITransferListClose(pTransfer, hList);
                    }

                    SharedClipboardURIListOpenParmsDestroy(&openParmsList);
                }

                if (RT_FAILURE(rc))
                    break;
            }

            if (RT_SUCCESS(rc))
            {
                LogRel2(("Shared Clipboard: Calculation complete, starting transfer ...\n"));

                /*
                 * Signal the "list complete" event so that this data object can return (valid) data via ::GetData().
                 * This in turn then will create IStream instances (by the OS) for each file system object to handle.
                 */
                int rc2 = RTSemEventSignal(pThis->m_EventListComplete);
                AssertRC(rc2);

                LogFlowFunc(("Waiting for transfer to complete ...\n"));

                /* Transferring stuff can take a while, so don't use any timeout here. */
                rc2 = RTSemEventWait(pThis->m_EventTransferComplete, RT_INDEFINITE_WAIT);
                AssertRC(rc2);
            }

            RTStrFree(pszRoots);
        }

        SharedClipboardURITransferClose(pTransfer);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a FILEGROUPDESCRIPTOR object from a given URI transfer and stores the result into an HGLOBAL object.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI transfer to create file grou desciprtor for.
 * @param   fUnicode            Whether the FILEGROUPDESCRIPTOR object shall contain Unicode data or not.
 * @param   phGlobal            Where to store the allocated HGLOBAL object on success.
 */
int VBoxClipboardWinDataObject::createFileGroupDescriptorFromTransfer(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                                                      bool fUnicode, HGLOBAL *phGlobal)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(phGlobal,  VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    const size_t cbFileGroupDescriptor = fUnicode ? sizeof(FILEGROUPDESCRIPTORW) : sizeof(FILEGROUPDESCRIPTORA);
    const size_t cbFileDescriptor = fUnicode ? sizeof(FILEDESCRIPTORW) : sizeof(FILEDESCRIPTORA);

    const UINT   cItems = (UINT)m_lstRootEntries.size(); /** UINT vs. size_t. */
    if (!cItems)
        return VERR_NOT_FOUND;
          UINT   curIdx = 0; /* Current index of the handled file group descriptor (FGD). */

    const size_t cbFGD  = cbFileGroupDescriptor + (cbFileDescriptor * (cItems - 1));

    LogFunc(("fUnicode=%RTbool, cItems=%u, cbFileDescriptor=%zu\n", fUnicode, cItems, cbFileDescriptor));

    /* FILEGROUPDESCRIPTORA / FILEGROUPDESCRIPTOR matches except the cFileName member (TCHAR vs. WCHAR). */
    FILEGROUPDESCRIPTOR *pFGD = (FILEGROUPDESCRIPTOR *)RTMemAllocZ(cbFGD);
    if (!pFGD)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    pFGD->cItems = cItems;

    char *pszFileSpec = NULL;

    FsObjEntryList::const_iterator itRoot = m_lstRootEntries.begin();
    while (itRoot != m_lstRootEntries.end())
    {
        FILEDESCRIPTOR *pFD = &pFGD->fgd[curIdx];
        RT_BZERO(pFD, cbFileDescriptor);

        const char *pszFile = itRoot->strPath.c_str();
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

                LogFlowFunc(("pFD->cFileNameW=%ls\n", pFD->cFileName));
            }
        }
        else
        {
            rc = RTStrCopy(pFD->cFileName, sizeof(pFD->cFileName), pszFileSpec);
            LogFlowFunc(("pFD->cFileNameA=%s\n", pFD->cFileName));
        }

        RTStrFree(pszFileSpec);
        pszFileSpec = NULL;

        if (RT_FAILURE(rc))
            break;

        pFD->dwFlags          = FD_PROGRESSUI | FD_ATTRIBUTES;
        if (fUnicode) /** @todo Only >= Vista. */
            pFD->dwFlags     |= FD_UNICODE;
        pFD->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

        const SHAREDCLIPBOARDFSOBJINFO *pObjInfo = &itRoot->objInfo;

        if (RTFS_IS_DIRECTORY(pObjInfo->Attr.fMode))
        {
            pFD->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }
        else if (RTFS_IS_FILE(pObjInfo->Attr.fMode))
        {
            pFD->dwFlags |= FD_FILESIZE;

            const uint64_t cbObjSize = pObjInfo->cbObject;

            pFD->nFileSizeHigh = RT_HI_U32(cbObjSize);
            pFD->nFileSizeLow  = RT_LO_U32(cbObjSize);
        }
        else if (RTFS_IS_SYMLINK(pObjInfo->Attr.fMode))
        {
            /** @todo Implement. */
        }
#if 0 /** @todo Implement this. */
        pFD->dwFlags = FD_ATTRIBUTES | FD_CREATETIME | FD_ACCESSTIME | FD_WRITESTIME | FD_FILESIZE;
        pFD->dwFileAttributes =
        pFD->ftCreationTime   =
        pFD->ftLastAccessTime =
        pFD->ftLastWriteTime  =
#endif
        ++curIdx;
        ++itRoot;
    }

    if (pszFileSpec)
        RTStrFree(pszFileSpec);

    if (RT_SUCCESS(rc))
        rc = copyToHGlobal(pFGD, cbFGD, GMEM_MOVEABLE, phGlobal);

    RTMemFree(pFGD);

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

    LogFlowFunc(("lIndex=%RI32\n", pFormatEtc->lindex));

    /*
     * Initialize default values.
     */
    RT_BZERO(pMedium, sizeof(STGMEDIUM));

    HRESULT hr = DV_E_FORMATETC; /* Play safe. */

    if (   pFormatEtc->cfFormat == m_cfFileDescriptorA
#ifdef VBOX_CLIPBOARD_WITH_UNICODE_SUPPORT
        || pFormatEtc->cfFormat == m_cfFileDescriptorW
#endif
       )
    {
        const bool fUnicode = pFormatEtc->cfFormat == m_cfFileDescriptorW;

        LogFlowFunc(("FormatIndex_FileDescriptor%s\n", fUnicode ? "W" : "A"));

        int rc;

        /* The caller can call GetData() several times, so make sure we don't do the same transfer multiple times. */
        if (SharedClipboardURITransferGetStatus(m_pTransfer) == SHAREDCLIPBOARDURITRANSFERSTATUS_NONE)
        {
            rc = SharedClipboardURITransferPrepare(m_pTransfer);
            if (RT_SUCCESS(rc))
            {
                /* Start the transfer asynchronously in a separate thread. */
                rc = SharedClipboardURITransferRun(m_pTransfer, &VBoxClipboardWinDataObject::readThread, this);
                if (RT_SUCCESS(rc))
                {
                    /* Don't block for too long here, as this also will screw other apps running on the OS. */
                    LogFunc(("Waiting for listing to arrive ...\n"));
                    rc = RTSemEventWait(m_EventListComplete, 10 * 1000 /* 10s timeout */);
                    if (RT_SUCCESS(rc))
                    {
                        LogFunc(("Listing complete\n"));


                    }
                }
            }
        }
        else
            rc = VINF_SUCCESS;

        if (RT_SUCCESS(rc))
        {
            HGLOBAL hGlobal;
            rc = createFileGroupDescriptorFromTransfer(m_pTransfer, fUnicode, &hGlobal);
            if (RT_SUCCESS(rc))
            {
                pMedium->tymed   = TYMED_HGLOBAL;
                pMedium->hGlobal = hGlobal;
                /* Note: hGlobal now is being owned by pMedium / the caller. */

                hr = S_OK;
            }
        }

        if (RT_FAILURE(rc))
            LogRel(("Shared Clipboard: Data object unable to get data, rc=%Rrc\n", rc));
    }

    if (pFormatEtc->cfFormat == m_cfFileContents)
    {
        if (   pFormatEtc->lindex >= 0
            && pFormatEtc->lindex < m_lstRootEntries.size())
        {
            m_uObjIdx = pFormatEtc->lindex; /* lIndex of FormatEtc contains the actual index to the object being handled. */

            LogFlowFunc(("FormatIndex_FileContents: m_uObjIdx=%u\n", m_uObjIdx));

            FSOBJENTRY &fsObjEntry = m_lstRootEntries.at(m_uObjIdx);

            LogRel2(("Shared Clipboard: Receiving file '%s' ...\n", fsObjEntry.strPath.c_str()));

            /* Hand-in the provider so that our IStream implementation can continue working with it. */
            hr = VBoxClipboardWinStreamImpl::Create(this /* pParent */, m_pTransfer,
                                                    fsObjEntry.strPath.c_str()/* File name */, &fsObjEntry.objInfo /* PSHAREDCLIPBOARDFSOBJINFO */,
                                                    &m_pStream);
            if (SUCCEEDED(hr))
            {
                /* Hand over the stream to the caller. */
                pMedium->tymed = TYMED_ISTREAM;
                pMedium->pstm  = m_pStream;
            }
        }
    }

    /* Error handling; at least return some basic data. */
    if (FAILED(hr))
    {
        LogFunc(("Failed; copying medium ...\n"));

        //pMedium->tymed          = pThisFormat->tymed;
        //pMedium->pUnkForRelease = NULL;
    }

    if (hr == DV_E_FORMATETC)
        LogRel(("Shared Clipboard: Error handling format\n"));

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

STDMETHODIMP VBoxClipboardWinDataObject::EndOperation(HRESULT hResult, IBindCtx *pbcReserved, DWORD dwEffects)
{
     RT_NOREF(hResult, pbcReserved, dwEffects);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::GetAsyncMode(BOOL *pfIsOpAsync)
{
     RT_NOREF(pfIsOpAsync);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::InOperation(BOOL *pfInAsyncOp)
{
     RT_NOREF(pfInAsyncOp);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::SetAsyncMode(BOOL fDoOpAsync)
{
     RT_NOREF(fDoOpAsync);
     return E_NOTIMPL;
}

STDMETHODIMP VBoxClipboardWinDataObject::StartOperation(IBindCtx *pbcReserved)
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

void VBoxClipboardWinDataObject::OnTransferComplete(int rc /* = VINF_SUCESS */)
{
    RT_NOREF(rc);

    LogFlowFunc(("m_uObjIdx=%RU32 (total: %zu)\n", m_uObjIdx, m_lstRootEntries.size()));

    const bool fComplete = m_uObjIdx == m_lstRootEntries.size() - 1 /* Object index is zero-based */;
    if (fComplete)
    {
        int rc2 = RTSemEventSignal(m_EventTransferComplete);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
}

void VBoxClipboardWinDataObject::OnTransferCanceled(void)
{
    LogFlowFuncEnter();

    int rc2 = RTSemEventSignal(m_EventTransferComplete);
    AssertRC(rc2);

    LogFlowFuncLeave();
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
            LogRel3(("Shared Clipboard: Format found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32, ulIndex=%RU32\n",
                      pFormatEtc->tymed, pFormatEtc->cfFormat, VBoxClipboardWinDataObject::ClipboardFormatToString(m_pFormatEtc[i].cfFormat),
                      pFormatEtc->dwAspect, i));
            if (puIndex)
                *puIndex = i;
            return true;
        }
    }

    LogRel3(("Shared Clipboard: Format NOT found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32\n",
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

