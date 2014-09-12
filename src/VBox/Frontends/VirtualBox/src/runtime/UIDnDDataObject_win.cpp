/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDDrag class implementation. This class implements the
 * IDataObject interface.
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

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

#include <windows.h>
#include <new> /* For bad_alloc. */
#include <shlobj.h>

#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/uri.h>

#include <QStringList>

#include "UIDnDDataObject_win.h"
#include "UIDnDEnumFormat_win.h"


UIDnDDataObject::UIDnDDataObject(CSession &session,
                                 CDnDSource &dndSource,
                                 const QStringList &lstFormats,
                                 QWidget *pParent)
    : mSession(session),
      mDnDSource(dndSource),
      mpParent(pParent),
      mStatus(Uninitialized),
      mRefCount(1),
      mcFormats(0),
      mpFormatEtc(NULL),
      mpStgMedium(NULL),
      mpvData(NULL),
      mcbData(0)
{
    HRESULT hr;

    ULONG cMaxFormats = 16; /* Maximum number of registered formats. */
    ULONG cRegisteredFormats = 0;

    try
    {
        mpFormatEtc = new FORMATETC[cMaxFormats];
        RT_BZERO(mpFormatEtc, sizeof(FORMATETC) * cMaxFormats);
        mpStgMedium = new STGMEDIUM[cMaxFormats];
        RT_BZERO(mpStgMedium, sizeof(STGMEDIUM) * cMaxFormats);

        for (int i = 0;
             (   i < lstFormats.size()
              && i < cMaxFormats); i++)
        {
            const QString &strFormat = lstFormats.at(i);
            if (mlstFormats.contains(strFormat))
                continue;

            /* URI data ("text/uri-list"). */
            if (strFormat.contains("text/uri-list", Qt::CaseInsensitive))
            {
                RegisterFormat(&mpFormatEtc[cRegisteredFormats], CF_TEXT);
                mpStgMedium[cRegisteredFormats++].tymed = TYMED_HGLOBAL;
                RegisterFormat(&mpFormatEtc[cRegisteredFormats], CF_UNICODETEXT);
                mpStgMedium[cRegisteredFormats++].tymed = TYMED_HGLOBAL;
                RegisterFormat(&mpFormatEtc[cRegisteredFormats], CF_HDROP);
                mpStgMedium[cRegisteredFormats++].tymed = TYMED_HGLOBAL;

                mlstFormats << strFormat;
            }
            /* Plain text ("text/plain"). */
            else if (strFormat.contains("text/plain", Qt::CaseInsensitive))
            {
                RegisterFormat(&mpFormatEtc[cRegisteredFormats], CF_TEXT);
                mpStgMedium[cRegisteredFormats++].tymed = TYMED_HGLOBAL;
                RegisterFormat(&mpFormatEtc[cRegisteredFormats], CF_UNICODETEXT);
                mpStgMedium[cRegisteredFormats++].tymed = TYMED_HGLOBAL;

                mlstFormats << strFormat;
            }
        }

        LogFlowFunc(("Total registered native formats: %RU32 (for %d formats from guest)\n",
                     cRegisteredFormats, lstFormats.size()));
        hr = S_OK;
    }
    catch (std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        int rc2 = RTSemEventCreate(&mSemEvent);
        AssertRC(rc2);

        /*
         * Other (not so common) formats.
         */
#if 0
        /* IStream. */
        RegisterFormat(&mpFormatEtc[cFormats++],
                       RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR));
        RegisterFormat(&mpFormatEtc[cFormats++],
                       RegisterClipboardFormat(CFSTR_FILECONTENTS),
                       TYMED_ISTREAM, 0 /* lIndex */);

        /* Required for e.g. Windows Media Player. */
        RegisterFormat(&mpFormatEtc[cFormats++],
                       RegisterClipboardFormat(CFSTR_FILENAME));
        RegisterFormat(&mpFormatEtc[cFormats++],
                       RegisterClipboardFormat(CFSTR_FILENAMEW));
        RegisterFormat(&mpFormatEtc[cFormats++],
                       RegisterClipboardFormat(CFSTR_SHELLIDLIST));
        RegisterFormat(&mpFormatEtc[cFormats++],
                       RegisterClipboardFormat(CFSTR_SHELLIDLISTOFFSET));
#endif
        mcFormats = cRegisteredFormats;

        /* Put the object in the "dropped" state already. We don't need
         * to handle the other states here at the moment. */
        mStatus = Dropped;
    }

    LogFlowFunc(("hr=%Rhrc\n", hr));
}

UIDnDDataObject::~UIDnDDataObject(void)
{
    if (mpFormatEtc)
        delete[] mpFormatEtc;

    if (mpStgMedium)
        delete[] mpStgMedium;

    if (mpvData)
        RTMemFree(mpvData);

    LogFlowFunc(("mRefCount=%RI32\n", mRefCount));
}

/*
 * IUnknown methods.
 */

STDMETHODIMP_(ULONG) UIDnDDataObject::AddRef(void)
{
    return InterlockedIncrement(&mRefCount);
}

STDMETHODIMP_(ULONG) UIDnDDataObject::Release(void)
{
    LONG lCount = InterlockedDecrement(&mRefCount);
    if (lCount == 0)
    {
        delete this;
        return 0;
    }

    return lCount;
}

STDMETHODIMP UIDnDDataObject::QueryInterface(REFIID iid, void **ppvObject)
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
 * Retrieves the data stored in this object and store the result in
 * pMedium.
 *
 * @return  IPRT status code.
 * @return  HRESULT
 * @param   pFormatEtc
 * @param   pMedium
 */
STDMETHODIMP UIDnDDataObject::GetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium)
{
    AssertPtrReturn(pFormatEtc, DV_E_FORMATETC);
    AssertPtrReturn(pMedium, DV_E_FORMATETC);

#ifdef VBOX_DND_DEBUG_FORMATS
    LogFlowFunc(("pFormatEtc=%p, pMedium=%p\n", pFormatEtc, pMedium));
#endif

    ULONG lIndex;
    if (!LookupFormatEtc(pFormatEtc, &lIndex)) /* Format supported? */
        return DV_E_FORMATETC;
    if (lIndex >= mcFormats) /* Paranoia. */
        return DV_E_FORMATETC;

    FORMATETC *pThisFormat = &mpFormatEtc[lIndex];
    AssertPtr(pThisFormat);

    STGMEDIUM *pThisMedium = &mpStgMedium[lIndex];
    AssertPtr(pThisMedium);

    HRESULT hr = DV_E_FORMATETC;

    LogFlowFunc(("mStatus=%ld\n", mStatus));
    if (mStatus == Dropping)
    {
        LogFlowFunc(("Waiting for event ...\n"));
        int rc2 = RTSemEventWait(mSemEvent, RT_INDEFINITE_WAIT);
        LogFlowFunc(("rc=%Rrc, mStatus=%ld\n", rc2, mStatus));
    }

    if (mStatus == Dropped)
    {
        LogFlowFunc(("cfFormat=%RI16, sFormat=%s, tyMed=%RU32, dwAspect=%RU32\n",
                     pThisFormat->cfFormat, UIDnDDataObject::ClipboardFormatToString(pFormatEtc->cfFormat),
                     pThisFormat->tymed, pThisFormat->dwAspect));
        LogFlowFunc(("Got strFormat=%s, pvData=%p, cbData=%RU32\n",
                     mstrFormat.toAscii().constData(), mpvData, mcbData));

        QVariant::Type vaType;
        QString strMIMEType;
        if (    (pFormatEtc->tymed & TYMED_HGLOBAL)
             && (pFormatEtc->dwAspect == DVASPECT_CONTENT)
             && (   pFormatEtc->cfFormat == CF_TEXT
                 || pFormatEtc->cfFormat == CF_UNICODETEXT)
           )
        {
            strMIMEType = "text/plain"; /** @todo Indicate UTF8 encoding? */
            vaType = QVariant::String;
        }
        else if (   (pFormatEtc->tymed & TYMED_HGLOBAL)
                 && (pFormatEtc->dwAspect == DVASPECT_CONTENT)
                 && (pFormatEtc->cfFormat == CF_HDROP))
        {
            strMIMEType = "text/uri-list";
            vaType = QVariant::StringList;
        }
#if 0 /* More formats; not needed right now. */
        else if (   (pFormatEtc->tymed & TYMED_ISTREAM)
                && (pFormatEtc->dwAspect == DVASPECT_CONTENT)
                && (pFormatEtc->cfFormat == CF_FILECONTENTS))
        {

        }
        else if  (   (pFormatEtc->tymed & TYMED_HGLOBAL)
                  && (pFormatEtc->dwAspect == DVASPECT_CONTENT)
                  && (pFormatEtc->cfFormat == CF_FILEDESCRIPTOR))
        {

        }
        else if (   (pFormatEtc->tymed & TYMED_HGLOBAL)
                 && (pFormatEtc->cfFormat == CF_PREFERREDDROPEFFECT))
        {
            HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, sizeof(DWORD));
            DWORD *pdwEffect = (DWORD *)GlobalLock(hData);
            AssertPtr(pdwEffect);
            *pdwEffect = DROPEFFECT_COPY;
            GlobalUnlock(hData);

            pMedium->hGlobal = hData;
            pMedium->tymed = TYMED_HGLOBAL;
        }
#endif
        LogFlowFunc(("strMIMEType=%s, vaType=%ld\n",
                     strMIMEType.toAscii().constData(), vaType));

        int rc;
        if (!mVaData.isValid())
        {
            rc = UIDnDDrag::RetrieveData(mSession,
                                         mDnDSource,
                                         /** @todo Support other actions. */
                                         Qt::CopyAction,
                                         strMIMEType, vaType, mVaData,
                                         mpParent);
        }
        else
            rc = VINF_SUCCESS; /* Data already retrieved. */

        if (RT_SUCCESS(rc))
        {
            if (   strMIMEType.startsWith("text/uri-list")
                       /* One item. */
                && (   mVaData.canConvert(QVariant::String)
                       /* Multiple items. */
                    || mVaData.canConvert(QVariant::StringList))
               )
            {
                QStringList lstFilesURI = mVaData.toStringList();
                QStringList lstFiles;
                for (size_t i = 0; i < lstFilesURI.size(); i++)
                {
                    /* Extract path from URI. */
                    char *pszPath = RTUriPath(lstFilesURI.at(i).toAscii().constData());
                    if (   pszPath
                        && strlen(pszPath) > 1)
                    {
                        pszPath++; /** @todo Skip first '/' (part of URI). Correct? */
                        pszPath = RTPathChangeToDosSlashes(pszPath, false /* fForce */);
                        lstFiles.append(pszPath);
                    }
                }

                size_t cFiles = lstFiles.size();
                Assert(cFiles);
#ifdef DEBUG
                LogFlowFunc(("Files (%zu)\n", cFiles));
                for (size_t i = 0; i < cFiles; i++)
                    LogFlowFunc(("\tFile: %s\n", lstFiles.at(i).toAscii().constData()));
#endif
                size_t cchFiles = 0; /* Number of ASCII characters. */
                for (size_t i = 0; i < cFiles; i++)
                {
                    cchFiles += strlen(lstFiles.at(i).toAscii().constData());
                    cchFiles += 1; /* Terminating '\0'. */
                }

                size_t cbBuf = sizeof(DROPFILES) + ((cchFiles + 1) * sizeof(RTUTF16));
                DROPFILES *pDropFiles = (DROPFILES *)RTMemAllocZ(cbBuf);
                if (pDropFiles)
                {
                    pDropFiles->pFiles = sizeof(DROPFILES);
                    pDropFiles->fWide = 1; /* We use unicode. Always. */

                    uint8_t *pCurFile = (uint8_t *)pDropFiles + pDropFiles->pFiles;
                    AssertPtr(pCurFile);

                    for (size_t i = 0; i < cFiles; i++)
                    {
                        size_t cchCurFile;
                        PRTUTF16 pwszFile;
                        rc = RTStrToUtf16(lstFiles.at(i).toAscii().constData(), &pwszFile);
                        if (RT_SUCCESS(rc))
                        {
                            cchCurFile = RTUtf16Len(pwszFile);
                            Assert(cchCurFile);
                            memcpy(pCurFile, pwszFile, cchCurFile * sizeof(RTUTF16));
                            RTUtf16Free(pwszFile);
                        }
                        else
                            break;

                        pCurFile += cchCurFile * sizeof(RTUTF16);

                        /* Terminate current file name. */
                        *pCurFile = L'\0';
                        pCurFile += sizeof(RTUTF16);
                    }

                    if (RT_SUCCESS(rc))
                    {
                        *pCurFile = L'\0'; /* Final list terminator. */

                        pMedium->tymed = TYMED_HGLOBAL;
                        pMedium->pUnkForRelease = NULL;
                        pMedium->hGlobal = GlobalAlloc(  GMEM_ZEROINIT
                                                       | GMEM_MOVEABLE
                                                       | GMEM_DDESHARE, cbBuf);
                        if (pMedium->hGlobal)
                        {
                            LPVOID pvMem = GlobalLock(pMedium->hGlobal);
                            if (pvMem)
                            {
                                memcpy(pvMem, pDropFiles, cbBuf);
                                GlobalUnlock(pMedium->hGlobal);

                                hr = S_OK;
                            }
                            else
                                rc = VERR_ACCESS_DENIED;
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }

                    RTMemFree(pDropFiles);
                }
            }
            else if (   strMIMEType.startsWith("text/plain")
                     && mVaData.canConvert(QVariant::String))
            {
                bool fUnicode = pFormatEtc->cfFormat == CF_UNICODETEXT;
                int cbCh = fUnicode
                         ? sizeof(WCHAR) : sizeof(char);

                QString strText = mVaData.toString();
                size_t cbSrc = strText.length() * cbCh;
                Assert(cbSrc);
                LPCVOID pvSrc = fUnicode
                              ? (void *)strText.unicode()
                              : (void *)strText.toAscii().constData();
                AssertPtr(pvSrc);

                LogFlowFunc(("pvSrc=0x%p, cbSrc=%zu, cbch=%d, fUnicode=%RTbool\n",
                             pvSrc, cbSrc, cbCh, fUnicode));

                pMedium->tymed = TYMED_HGLOBAL;
                pMedium->pUnkForRelease = NULL;
                pMedium->hGlobal = GlobalAlloc(  GMEM_ZEROINIT
                                               | GMEM_MOVEABLE
                                               | GMEM_DDESHARE,
                                               cbSrc);
                if (pMedium->hGlobal)
                {
                    LPVOID pvDst = GlobalLock(pMedium->hGlobal);
                    if (pvDst)
                    {
                        memcpy(pvDst, pvSrc, cbSrc);
                        GlobalUnlock(pMedium->hGlobal);
                    }
                    else
                        rc = VERR_ACCESS_DENIED;

                    hr = S_OK;
                }
                else
                    hr  = VERR_NO_MEMORY;
            }
            else
                LogFlowFunc(("MIME type=%s not supported\n",
                             strMIMEType.toAscii().constData()));

            LogFlowFunc(("Handling formats ended with rc=%Rrc\n", rc));
        }
    }

    /*
     * Fallback in error case.
     */
    if (FAILED(hr))
    {
        LogFlowFunc(("Error hr=%Rhrc while handling data, copying raw medium data ...\n", hr));

        switch (pThisMedium->tymed)
        {

        case TYMED_HGLOBAL:
            pMedium->hGlobal = (HGLOBAL)OleDuplicateData(pThisMedium->hGlobal,
                                                         pThisFormat->cfFormat,
                                                         0 /* Flags */);
            break;

        default:
            break;
        }

        pMedium->tymed          = pFormatEtc->tymed;
        pMedium->pUnkForRelease = NULL;
    }

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
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
STDMETHODIMP UIDnDDataObject::GetDataHere(FORMATETC *pFormatEtc, STGMEDIUM *pMedium)
{
    LogFlowFunc(("\n"));
    return DATA_E_FORMATETC;
}

/**
 * Query if this objects supports a specific format.
 *
 * @return  IPRT status code.
 * @return  HRESULT
 * @param   pFormatEtc
 */
STDMETHODIMP UIDnDDataObject::QueryGetData(FORMATETC *pFormatEtc)
{
    return (LookupFormatEtc(pFormatEtc, NULL /* puIndex */)) ? S_OK : DV_E_FORMATETC;
}

STDMETHODIMP UIDnDDataObject::GetCanonicalFormatEtc(FORMATETC *pFormatEct, FORMATETC *pFormatEtcOut)
{
    LogFlowFunc(("\n"));

    /* Set this to NULL in any case. */
    pFormatEtcOut->ptd = NULL;
    return E_NOTIMPL;
}

STDMETHODIMP UIDnDDataObject::SetData(FORMATETC *pFormatEtc, STGMEDIUM *pMedium, BOOL fRelease)
{
    return E_NOTIMPL;
}

STDMETHODIMP UIDnDDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc)
{
    LogFlowFunc(("dwDirection=%RI32, mcFormats=%RI32, mpFormatEtc=%p\n",
                 dwDirection, mcFormats, mpFormatEtc));

    HRESULT hr;
    if (dwDirection == DATADIR_GET)
    {
        hr = UIDnDEnumFormatEtc::CreateEnumFormatEtc(mcFormats, mpFormatEtc, ppEnumFormatEtc);
    }
    else
        hr = E_NOTIMPL;

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
}

STDMETHODIMP UIDnDDataObject::DAdvise(FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP UIDnDDataObject::DUnadvise(DWORD dwConnection)
{
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP UIDnDDataObject::EnumDAdvise(IEnumSTATDATA **ppEnumAdvise)
{
    return OLE_E_ADVISENOTSUPPORTED;
}

/*
 * Own stuff.
 */

int UIDnDDataObject::Abort(void)
{
    LogFlowFunc(("Aborting ...\n"));
    mStatus = Aborted;
    return RTSemEventSignal(mSemEvent);
}

/* static */
const char* UIDnDDataObject::ClipboardFormatToString(CLIPFORMAT fmt)
{
    WCHAR wszFormat[128];
    if (GetClipboardFormatNameW(fmt, wszFormat, sizeof(wszFormat) / sizeof(WCHAR)))
        LogFlowFunc(("wFormat=%RI16, szName=%ls\n", fmt, wszFormat));

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

bool UIDnDDataObject::LookupFormatEtc(FORMATETC *pFormatEtc, ULONG *puIndex)
{
    AssertReturn(pFormatEtc, false);
    /* puIndex is optional. */

    for (ULONG i = 0; i < mcFormats; i++)
    {
        if(    (pFormatEtc->tymed & mpFormatEtc[i].tymed)
            && pFormatEtc->cfFormat == mpFormatEtc[i].cfFormat
            && pFormatEtc->dwAspect == mpFormatEtc[i].dwAspect)
        {
#ifdef VBOX_DND_DEBUG_FORMATS
            LogFlowFunc(("Format found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32, ulIndex=%RU32\n",
                         pFormatEtc->tymed, pFormatEtc->cfFormat, UIDnDDataObject::ClipboardFormatToString(mpFormatEtc[i].cfFormat),
                         pFormatEtc->dwAspect, i));
#endif
            if (puIndex)
                *puIndex = i;
            return true;
        }
    }

#ifdef VBOX_DND_DEBUG_FORMATS
    LogFlowFunc(("Format NOT found: tyMed=%RI32, cfFormat=%RI16, sFormats=%s, dwAspect=%RI32\n",
                 pFormatEtc->tymed, pFormatEtc->cfFormat, UIDnDDataObject::ClipboardFormatToString(pFormatEtc->cfFormat),
                 pFormatEtc->dwAspect));
#endif
    return false;
}

/* static */
HGLOBAL UIDnDDataObject::MemDup(HGLOBAL hMemSource)
{
    DWORD dwLen    = GlobalSize(hMemSource);
    AssertReturn(dwLen, NULL);
    PVOID pvSource = GlobalLock(hMemSource);
    if (pvSource)
    {
        PVOID pvDest = GlobalAlloc(GMEM_FIXED, dwLen);
        if (pvDest)
            memcpy(pvDest, pvSource, dwLen);

        GlobalUnlock(hMemSource);
        return pvDest;
    }

    return NULL;
}

void UIDnDDataObject::RegisterFormat(FORMATETC *pFormatEtc, CLIPFORMAT clipFormat,
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
                 pFormatEtc->cfFormat, UIDnDDataObject::ClipboardFormatToString(pFormatEtc->cfFormat)));
}

void UIDnDDataObject::SetStatus(Status status)
{
    LogFlowFunc(("Setting status to %ld\n", status));
    mStatus = status;
}

int UIDnDDataObject::Signal(const QString &strFormat,
                            const void *pvData, uint32_t cbData)
{
    LogFlowFunc(("Signalling ...\n"));

    int rc;

    mStatus = Dropped;
    mstrFormat = strFormat;
    if (cbData)
    {
        mpvData = RTMemAlloc(cbData);
        if (mpvData)
        {
            memcpy(mpvData, pvData, cbData);
            mcbData = cbData;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VINF_SUCCESS;

    if (RT_FAILURE(rc))
        mStatus = Aborted;

    /* Signal in any case. */
    int rc2 = RTSemEventSignal(mSemEvent);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

