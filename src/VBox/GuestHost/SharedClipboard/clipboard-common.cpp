/* $Id$ */
/** @file
 * Shared Clipboard: Some helper function for converting between the various eol.
 */

/*
 * Includes contributions from François Revol
 *
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/path.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/GuestHost/clipboard-helper.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif


#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static int sharedClipboardURITransferThreadCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer);
static int sharedClipboardURITransferThreadDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs);
static int sharedClipboardURITransferReadThread(RTTHREAD hThread, void *pvUser);
static int sharedClipboardURITransferWriteThread(RTTHREAD hThread, void *pvUser);
static PSHAREDCLIPBOARDURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx);
#endif


/** @todo use const where appropriate; delinuxify the code (*Lin* -> *Host*); use AssertLogRel*. */

int vboxClipboardUtf16GetWinSize(PRTUTF16 pwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest, i;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    AssertLogRelMsgReturn(pwszSrc != NULL, ("vboxClipboardUtf16GetWinSize: received a null Utf16 string, returning VERR_INVALID_PARAMETER\n"), VERR_INVALID_PARAMETER);
    if (cwSrc == 0)
    {
        *pcwDest = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
/** @todo convert the remainder of the Assert stuff to AssertLogRel. */
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16GetWinSize: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    cwDest = 0;
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    for (i = (pwszSrc[0] == UTF16LEMARKER ? 1 : 0); i < cwSrc; ++i, ++cwDest)
    {
        /* Check for a single line feed */
        if (pwszSrc[i] == LINEFEED)
            ++cwDest;
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        if (pwszSrc[i] == CARRIAGERETURN)
            ++cwDest;
#endif
        if (pwszSrc[i] == 0)
        {
            /* Don't count this, as we do so below. */
            break;
        }
    }
    /* Count the terminating null byte. */
    ++cwDest;
    LogFlowFunc(("returning VINF_SUCCESS, %d 16bit words\n", cwDest));
    *pcwDest = cwDest;
    return VINF_SUCCESS;
}

int vboxClipboardUtf16LinToWin(PRTUTF16 pwszSrc, size_t cwSrc, PRTUTF16 pu16Dest,
                               size_t cwDest)
{
    size_t i, j;
    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    if (!VALID_PTR(pwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("vboxClipboardUtf16LinToWin: received an invalid pointer, pwszSrc=%p, pu16Dest=%p, returning VERR_INVALID_PARAMETER\n", pwszSrc, pu16Dest));
        AssertReturn(VALID_PTR(pwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        if (cwDest == 0)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        pu16Dest[0] = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16LinToWin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Don't copy the endian marker. */
    for (i = (pwszSrc[0] == UTF16LEMARKER ? 1 : 0), j = 0; i < cwSrc; ++i, ++j)
    {
        /* Don't copy the null byte, as we add it below. */
        if (pwszSrc[i] == 0)
            break;
        if (j == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (pwszSrc[i] == LINEFEED)
        {
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
        }
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        else if (pwszSrc[i] == CARRIAGERETURN)
        {
            /* set cr */
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
            /* add the lf */
            pu16Dest[j] = LINEFEED;
            continue;
        }
#endif
        pu16Dest[j] = pwszSrc[i];
    }
    /* Add the trailing null. */
    if (j == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[j] = 0;
    LogFlowFunc(("rc=VINF_SUCCESS, pu16Dest=%ls\n", pu16Dest));
    return VINF_SUCCESS;
}

int vboxClipboardUtf16GetLinSize(PRTUTF16 pwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    if (!VALID_PTR(pwszSrc))
    {
        LogRel(("vboxClipboardUtf16GetLinSize: received an invalid Utf16 string %p.  Returning VERR_INVALID_PARAMETER.\n", pwszSrc));
        AssertReturn(VALID_PTR(pwszSrc), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        LogFlowFunc(("empty source string, returning VINF_SUCCESS\n"));
        *pcwDest = 0;
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16GetLinSize: received a big endian Utf16 string.  Returning VERR_INVALID_PARAMETER.\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    if (pwszSrc[0] == UTF16LEMARKER)
        cwDest = 0;
    else
        cwDest = 1;
    for (size_t i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (   (i + 1 < cwSrc)
            && (pwszSrc[i] == CARRIAGERETURN)
            && (pwszSrc[i + 1] == LINEFEED))
        {
            ++i;
        }
        if (pwszSrc[i] == 0)
        {
            break;
        }
    }
    /* Terminating zero */
    ++cwDest;
    LogFlowFunc(("returning %d\n", cwDest));
    *pcwDest = cwDest;
    return VINF_SUCCESS;
}

int vboxClipboardUtf16WinToLin(PRTUTF16 pwszSrc, size_t cwSrc, PRTUTF16 pu16Dest,
                               size_t cwDest)
{
    size_t cwDestPos;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u, pu16Dest=%p, cwDest=%u\n",
                 cwSrc, pwszSrc, cwSrc, pu16Dest, cwDest));
    /* A buffer of size 0 may not be an error, but it is not a good idea either. */
    Assert(cwDest > 0);
    if (!VALID_PTR(pwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("vboxClipboardUtf16WinToLin: received an invalid ptr, pwszSrc=%p, pu16Dest=%p, returning VERR_INVALID_PARAMETER\n", pwszSrc, pu16Dest));
        AssertReturn(VALID_PTR(pwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16WinToLin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertMsgFailedReturn(("received a big endian string\n"), VERR_INVALID_PARAMETER);
    }
    if (cwDest == 0)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    if (cwSrc == 0)
    {
        pu16Dest[0] = 0;
        LogFlowFunc(("received empty string.  Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
    /* Prepend the Utf16 byte order marker if it is missing. */
    if (pwszSrc[0] == UTF16LEMARKER)
    {
        cwDestPos = 0;
    }
    else
    {
        pu16Dest[0] = UTF16LEMARKER;
        cwDestPos = 1;
    }
    for (size_t i = 0; i < cwSrc; ++i, ++cwDestPos)
    {
        if (pwszSrc[i] == 0)
        {
            break;
        }
        if (cwDestPos == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (   (i + 1 < cwSrc)
            && (pwszSrc[i] == CARRIAGERETURN)
            && (pwszSrc[i + 1] == LINEFEED))
        {
            ++i;
        }
        pu16Dest[cwDestPos] = pwszSrc[i];
    }
    /* Terminating zero */
    if (cwDestPos == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[cwDestPos] = 0;
    LogFlowFunc(("set string %ls.  Returning\n", pu16Dest + 1));
    return VINF_SUCCESS;
}

int vboxClipboardDibToBmp(const void *pvSrc, size_t cbSrc, void **ppvDest, size_t *pcbDest)
{
    size_t        cb            = sizeof(BMFILEHEADER) + cbSrc;
    PBMFILEHEADER pFileHeader   = NULL;
    void         *pvDest        = NULL;
    size_t        offPixel      = 0;

    AssertPtrReturn(pvSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);

    PBMINFOHEADER pBitmapInfoHeader = (PBMINFOHEADER)pvSrc;
    /** @todo Support all the many versions of the DIB headers. */
    if (   cbSrc < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->u32Size) < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->u32Size) != sizeof(BMINFOHEADER))
    {
        Log(("vboxClipboardDibToBmp: invalid or unsupported bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    offPixel = sizeof(BMFILEHEADER)
                + RT_LE2H_U32(pBitmapInfoHeader->u32Size)
                + RT_LE2H_U32(pBitmapInfoHeader->u32ClrUsed) * sizeof(uint32_t);
    if (cbSrc < offPixel)
    {
        Log(("vboxClipboardDibToBmp: invalid bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    pvDest = RTMemAlloc(cb);
    if (!pvDest)
    {
        Log(("writeToPasteboard: cannot allocate memory for bitmap.\n"));
        return VERR_NO_MEMORY;
    }

    pFileHeader = (PBMFILEHEADER)pvDest;
    pFileHeader->u16Type        = BITMAPHEADERMAGIC;
    pFileHeader->u32Size        = (uint32_t)RT_H2LE_U32(cb);
    pFileHeader->u16Reserved1   = pFileHeader->u16Reserved2 = 0;
    pFileHeader->u32OffBits     = (uint32_t)RT_H2LE_U32(offPixel);
    memcpy((uint8_t *)pvDest + sizeof(BMFILEHEADER), pvSrc, cbSrc);
    *ppvDest = pvDest;
    *pcbDest = cb;
    return VINF_SUCCESS;
}

int vboxClipboardBmpGetDib(const void *pvSrc, size_t cbSrc, const void **ppvDest, size_t *pcbDest)
{
    AssertPtrReturn(pvSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);

    PBMFILEHEADER pFileHeader = (PBMFILEHEADER)pvSrc;
    if (   cbSrc < sizeof(BMFILEHEADER)
        || pFileHeader->u16Type != BITMAPHEADERMAGIC
        || RT_LE2H_U32(pFileHeader->u32Size) != cbSrc)
    {
        Log(("vboxClipboardBmpGetDib: invalid bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    *ppvDest = ((uint8_t *)pvSrc) + sizeof(BMFILEHEADER);
    *pcbDest = cbSrc - sizeof(BMFILEHEADER);
    return VINF_SUCCESS;
}

#ifdef VBOX_STRICT
int vboxClipboardDbgDumpHtml(const char *pszSrc, size_t cbSrc)
{
    size_t cchIgnored = 0;
    int rc = RTStrNLenEx(pszSrc, cbSrc, &cchIgnored);
    if (RT_SUCCESS(rc))
    {
        char *pszBuf = (char *)RTMemAllocZ(cbSrc + 1);
        if (pszBuf)
        {
            rc = RTStrCopy(pszBuf, cbSrc + 1, (const char *)pszSrc);
            if (RT_SUCCESS(rc))
            {
                for (size_t i = 0; i < cbSrc; ++i)
                    if (pszBuf[i] == '\n' || pszBuf[i] == '\r')
                        pszBuf[i] = ' ';
            }
            else
                LogFunc(("Error in copying string\n"));
            LogFunc(("Removed \\r\\n: %s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}
#endif

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Returns the size (in bytes) of the announced meta data.
 *
 * @returns Announced meta data size in bytes.
 * @param   pDataHdr            Data header struct to get announced meta data size for.
 */
uint32_t SharedClipboardURIDataHdrGetMetaDataSize(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, 0);

    return pDataHdr->cbMeta;
}

/**
 * Initializes an URI data header struct.
 *
 * @returns VBox status code.
 * @param   pDataHdr            Data header struct to initialize.
 */
int SharedClipboardURIDataHdrInit(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    SharedClipboardURIDataHdrReset(pDataHdr);

    return VINF_SUCCESS;
}

/**
 * Destroys an URI data header struct.
 *
 * @param   pDataHdr            Data header struct to destroy.
 */
void SharedClipboardURIDataHdrDestroy(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (!pDataHdr)
        return;

    if (pDataHdr->pvMetaFmt)
    {
        Assert(pDataHdr->cbMetaFmt);

        PSHAREDCLIPBOARDMETADATAFMT pMetaDatafmt = (PSHAREDCLIPBOARDMETADATAFMT)pDataHdr->pvMetaFmt;
        AssertPtr(pMetaDatafmt);
        if (pMetaDatafmt->pvFmt)
        {
            Assert(pMetaDatafmt->cbFmt);
            RTMemFree(pMetaDatafmt->pvFmt);
        }

        RTMemFree(pDataHdr->pvMetaFmt);
        pDataHdr->pvMetaFmt = NULL;
        pDataHdr->cbMetaFmt = 0;
    }

    if (pDataHdr->pvChecksum)
    {
        Assert(pDataHdr->cbChecksum);

        RTMemFree(pDataHdr->pvChecksum);
        pDataHdr->pvChecksum = NULL;
        pDataHdr->cbChecksum = 0;
    }
}

/**
 * Resets an URI data header struct.
 *
 * @returns VBox status code.
 * @param   pDataHdr            Data header struct to reset.
 */
void SharedClipboardURIDataHdrReset(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturnVoid(pDataHdr);

    LogFlowFuncEnter();

    RT_BZERO(pDataHdr, sizeof(VBOXCLIPBOARDDATAHDR));
}

/**
 * Returns whether a given clipboard data header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDataHdr            Clipboard data header to validate.
 */
bool SharedClipboardURIDataHdrIsValid(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return true; /** @todo Implement this. */
}

/**
 * Returns whether a given clipboard data chunk is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDataChunk          Clipboard data chunk to validate.
 */
bool SharedClipboardURIDataChunkIsValid(PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    RT_NOREF(pDataChunk);
    return true; /** @todo Implement this. */
}

/**
 * Returns whether given clipboard directory data is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDirData            Clipboard directory data to validate.
 */
bool SharedClipboardURIDirDataIsValid(PVBOXCLIPBOARDDIRDATA pDirData)
{
    if (   !pDirData->cbPath
        || pDirData->cbPath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pDirData->pszPath))
        return false;

    return true;
}

/**
 * Returns whether a given clipboard file header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pFileHdr            Clipboard file header to validate.
 * @param   pDataHdr            Data header to use for validation.
 */
bool SharedClipboardURIFileHdrIsValid(PVBOXCLIPBOARDFILEHDR pFileHdr, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (   !pFileHdr->cbFilePath
        || pFileHdr->cbFilePath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pFileHdr->pszFilePath))
        return false;

    if (pFileHdr->cbSize > pDataHdr->cbTotal)
        return false;

    return true;
}

/**
 * Returns whether given clipboard file data is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pFileData           Clipboard file data to validate.
 * @param   pDataHdr            Data header to use for validation.
 */
bool SharedClipboardURIFileDataIsValid(PVBOXCLIPBOARDFILEDATA pFileData, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pFileData, pDataHdr);
    return true;
}

/**
 * Initializes an URI object context.
 *
 * @returns VBox status code.
 * @param   pObjCtx             URI object context to initialize.
 */
int SharedClipboardURIObjCtxInit(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    pObjCtx->pObj = NULL;

    return VINF_SUCCESS;
}

/**
 * Uninitializes an URI object context.
 *
 * @param   pObjCtx             URI object context to uninitialize.
 */
void SharedClipboardURIObjCtxUninit(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturnVoid(pObjCtx);

    LogFlowFuncEnter();

    if (pObjCtx->pObj)
    {
        pObjCtx->pObj->Close();
        delete pObjCtx->pObj;
    }

    pObjCtx->pObj = NULL;
}

/**
 * Returns the URI object context's URI object.
 *
 * @returns Pointer to the URI object context's URI object.
 * @param   pObjCtx             URI object context to return the URI object for.
 */
SharedClipboardURIObject *SharedClipboardURIObjCtxGetObj(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, NULL);
    return pObjCtx->pObj;
}

/**
 * Returns if an URI object context is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pObjCtx             URI object context to check.
 */
bool SharedClipboardURIObjCtxIsValid(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    return (   pObjCtx
            && pObjCtx->pObj
            && pObjCtx->pObj->IsComplete() == false
            && pObjCtx->pObj->IsOpen());
}

/**
 * Initializes an URI clipboard transfer struct.
 *
 * @returns VBox status code.
 * @param   enmDir              Transfer direction.
 * @param   pCtx                Shared Clipboard provider creation context to use.
 * @param   ppTransfer          Where to return the created URI transfer struct.
 *                              Must be destroyed by SharedClipboardURITransferDestroy().
 */
int SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR enmDir, PSHAREDCLIPBOARDPROVIDERCREATIONCTX pCtx,
                                     PSHAREDCLIPBOARDURITRANSFER *ppTransfer)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(ppTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PSHAREDCLIPBOARDURITRANSFER pTransfer = (PSHAREDCLIPBOARDURITRANSFER)RTMemAlloc(sizeof(SHAREDCLIPBOARDURITRANSFER));
    if (!pTransfer)
        return VERR_NO_MEMORY;

    pTransfer->State.enmDir = enmDir;

    int rc = SharedClipboardURIDataHdrInit(&pTransfer->State.Header);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardMetaDataInit(&pTransfer->State.Meta);
        if (RT_SUCCESS(rc))
        {
            pTransfer->pArea     = NULL; /* Will be created later if needed. */
            pTransfer->pProvider = SharedClipboardProvider::Create(pCtx);
            if (pTransfer->pProvider)
            {
                pTransfer->Thread.hThread    = NIL_RTTHREAD;
                pTransfer->Thread.fCancelled = false;
                pTransfer->Thread.fStarted   = false;

                pTransfer->pvUser = NULL;
                pTransfer->cbUser = 0;

                *ppTransfer = pTransfer;
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys an URI clipboard transfer context struct.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard transfer to destroy.
 */
int SharedClipboardURITransferDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    if (!pTransfer)
        return VINF_SUCCESS;

    LogFlowFuncEnter();

    int rc = sharedClipboardURITransferThreadDestroy(pTransfer, 30 * 1000 /* Timeout in ms */);
    if (RT_FAILURE(rc))
        return rc;

    SharedClipboardURIDataHdrDestroy(&pTransfer->State.Header);
    SharedClipboardMetaDataDestroy(&pTransfer->State.Meta);

    if (pTransfer->pProvider)
    {
        delete pTransfer->pProvider;
        pTransfer->pProvider = NULL;
    }

    RTMemFree(pTransfer);
    pTransfer = NULL;

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

/**
 * Resets an clipboard URI transfer.
 *
 * @param   pTransfer           URI clipboard transfer to reset.
 */
void SharedClipboardURITransferReset(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    LogFlowFuncEnter();

    /** @todo Anything else to do here? */

    if (pTransfer->pProvider)
        pTransfer->pProvider->Reset();

    pTransfer->URIList.Clear();
}

/**
 * Returns the clipboard area for a clipboard URI transfer.
 *
 * @returns Current clipboard area, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return clipboard area for.
 */
SharedClipboardArea *SharedClipboardURITransferGetArea(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    return pTransfer->pArea;
}

/**
 * Returns the current URI object for a clipboard URI transfer.
 *
 * @returns Current URI object, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return current URI object for.
 */
const SharedClipboardURIObject *SharedClipboardURITransferGetCurrentObject(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    return pTransfer->URIList.First();
}

/**
 * Returns the provider for a clipboard URI transfer.
 *
 * @returns Current provider, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return provider for.
 */
SharedClipboardProvider *SharedClipboardURITransferGetProvider(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    return pTransfer->pProvider;
}

/**
 * Returns the URI list for a clipboard URI transfer.
 *
 * @returns Pointer to URI list.
 * @param   pTransfer           URI clipboard transfer to return URI list for.
 */
SharedClipboardURIList *SharedClipboardURITransferGetList(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    return &pTransfer->URIList;
}

/**
 * Returns the current URI object for a clipboard URI transfer.
 *
 * @returns Pointer to URI object.
 * @param   pTransfer           URI clipboard transfer to return URI object for.
 */
SharedClipboardURIObject *SharedClipboardURITransferGetObject(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uIdx)
{
    return pTransfer->URIList.At(uIdx);
}

/**
 * Runs (starts) an URI transfer, either in synchronous or asynchronous (threaded) mode.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to run.
 * @param   fAsync              Whether to run the transfer synchronously or asynchronously.
 */
int SharedClipboardURITransferRun(PSHAREDCLIPBOARDURITRANSFER pTransfer, bool fAsync)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("fAsync=%RTbool\n", fAsync));

    if (fAsync)
    {
        rc = sharedClipboardURITransferThreadCreate(pTransfer);
    }
    else
    {
        if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ)
            rc = SharedClipboardURITransferRead(pTransfer);
        else if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_WRITE)
            rc = SharedClipboardURITransferWrite(pTransfer);
        else
            rc = VERR_NOT_IMPLEMENTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets or unsets the callback table to be used for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to set callbacks for.
 * @param   pCallbacks          Pointer to callback table to set. Specify NULL to unset existing callbacks.
 */
void SharedClipboardURITransferSetCallbacks(PSHAREDCLIPBOARDURITRANSFER pTransfer, PSHAREDCLIPBOARDURITRANSFERCALLBACKS pCallbacks)
{
    AssertPtrReturnVoid(pTransfer);
    /* pCallbacks might be NULL to unset callbacks. */

    LogFlowFunc(("pCallbacks=%p\n", pCallbacks));

    if (pCallbacks)
    {
        pTransfer->Callbacks = *pCallbacks;
    }
    else
        RT_ZERO(pTransfer->Callbacks);
}

/**
 * Creates a thread for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to create thread for.
 */
static int sharedClipboardURITransferThreadCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    PFNRTTHREAD pfnRTThread = NULL;

    if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ)
        pfnRTThread = sharedClipboardURITransferReadThread;
    else if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_WRITE)
        pfnRTThread = sharedClipboardURITransferWriteThread;

    AssertPtrReturn(pfnRTThread, VERR_NOT_SUPPORTED);

    /* Spawn a worker thread, so that we don't block the window thread for too long. */
    int rc = RTThreadCreate(&pTransfer->Thread.hThread, pfnRTThread,
                            pTransfer /* pvUser */, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "shclp");
    if (RT_SUCCESS(rc))
    {
        int rc2 = RTThreadUserWait(pTransfer->Thread.hThread, 30 * 1000 /* Timeout in ms */);
        AssertRC(rc2);

        if (!pTransfer->Thread.fStarted) /* Did the thread fail to start? */
            rc = VERR_GENERAL_FAILURE; /** @todo Find a better rc. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys a thread of a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to destroy thread for.
 * @param   uTimeoutMs          Timeout (in ms) to wait for thread creation.
 */
static int sharedClipboardURITransferThreadDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    if (pTransfer->Thread.hThread == NIL_RTTHREAD)
        return VINF_SUCCESS;

    int rcThread = VERR_WRONG_ORDER;
    int rc = RTThreadWait(pTransfer->Thread.hThread, uTimeoutMs, &rcThread);

    LogFlowFunc(("Waiting for thread resulted in %Rrc (thread exited with %Rrc)\n", rc, rcThread));

    return rc;
}

/**
 * Reads all URI objects using the connected provider.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to read objects for.
 */
int SharedClipboardURITransferRead(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = SharedClipboardURITransferMetaDataRead(pTransfer, NULL /* pcbRead */);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardURITransferWriteObjects(pTransfer);
        if (RT_SUCCESS(rc))
        {
            if (pTransfer->Callbacks.pfnTransferComplete)
            {
                SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
                pTransfer->Callbacks.pfnTransferComplete(&callbackData, rc);
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pTransfer->Callbacks.pfnTransferError)
        {
            SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
            pTransfer->Callbacks.pfnTransferError(&callbackData, rc);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Thread for transferring (reading) URI objects from source to the target.
 * For target to source transfers we utilize our own IDataObject / IStream implementations.
 *
 * @returns VBox status code.
 * @param   hThread             Thread handle.
 * @param   pvUser              User arguments; is PSHAREDCLIPBOARDURITRANSFER.
 */
static int sharedClipboardURITransferReadThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* At the moment we only support one transfer at a time. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer = (PSHAREDCLIPBOARDURITRANSFER)pvUser;
    AssertPtr(pTransfer->pProvider);

    int rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        pTransfer->Thread.fStarted = true;

    int rc2 = RTThreadUserSignal(hThread);
    const bool fSignalled = RT_SUCCESS(rc2);

    if (RT_SUCCESS(rc))
        rc = SharedClipboardURITransferRead(pTransfer);

    if (!fSignalled)
    {
        rc2 = RTThreadUserSignal(hThread);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds meta data for a clipboard URI transfer, internal version.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to set meta data for.
 * @param   pvMeta              Pointer to meta data buffer.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 */
static int sharedClipboardURITransferMetaDataAddInternal(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                                         const void *pvMeta, uint32_t cbMeta)
{
    LogFlowFunc(("pvMeta=%p, cbMeta=%RU32\n", pvMeta, cbMeta));

    int rc = SharedClipboardMetaDataAdd(&pTransfer->State.Meta, pvMeta, cbMeta);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds meta data for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to set meta data for.
 * @param   pvMeta              Pointer to meta data buffer.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 */
int SharedClipboardURITransferMetaDataAdd(PSHAREDCLIPBOARDURITRANSFER pTransfer, const void *pvMeta, uint32_t cbMeta)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    int rc = sharedClipboardURITransferMetaDataAddInternal(pTransfer, pvMeta, cbMeta);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Marks the meta data as being complete and initializes everything needed for the transfer to begin.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to mark meta data complete for.
 */
int SharedClipboardURITransferMetaDataComplete(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = pTransfer->URIList.SetFromURIData(SharedClipboardMetaDataRaw(&pTransfer->State.Meta),
                                               SharedClipboardMetaDataGetUsed(&pTransfer->State.Meta),
                                               SHAREDCLIPBOARDURILIST_FLAGS_NONE);
    if (RT_SUCCESS(rc))
    {
        pTransfer->State.Header.cbTotal  = pTransfer->URIList.GetTotalBytes();
        pTransfer->State.Header.cbMeta   = (uint32_t)SharedClipboardMetaDataGetUsed(&pTransfer->State.Meta);
        pTransfer->State.Header.cObjects = pTransfer->URIList.GetTotalCount();

        PSHAREDCLIPBOARDMETADATAFMT pMetaDataFmt = (PSHAREDCLIPBOARDMETADATAFMT)RTMemAllocZ(sizeof(SHAREDCLIPBOARDMETADATAFMT));
        if (pMetaDataFmt)
        {
            char *pszFmt = NULL;
            rc = RTStrAAppend(&pszFmt, "VBoxShClURIList");
            if (RT_SUCCESS(rc))
            {
                pMetaDataFmt->uVer  = 1;
                pMetaDataFmt->pvFmt = pszFmt;
                pMetaDataFmt->cbFmt = (uint32_t)strlen(pszFmt) + 1 /* Include terminating zero */;

                pTransfer->State.Header.pvMetaFmt = pMetaDataFmt;
                pTransfer->State.Header.cbMetaFmt = sizeof(PSHAREDCLIPBOARDMETADATAFMT) + pMetaDataFmt->cbFmt;
            }
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            /** @todo Add checksum support. */

            if (!SharedClipboardURIDataHdrIsValid(&pTransfer->State.Header))
                rc = VERR_INVALID_PARAMETER;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads meta for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to read meta data for.
 * @param   pcbRead             How much meta data (in bytes) was read on success.
 */
int SharedClipboardURITransferMetaDataRead(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t *pcbRead)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    /* Destroy any former meta data. */
    SharedClipboardMetaDataDestroy(&pTransfer->State.Meta);

    uint32_t cbReadTotal = 0;

    int rc = pTransfer->pProvider->ReadDataHdr(&pTransfer->State.Header);
    if (RT_SUCCESS(rc))
    {
        uint32_t cbMeta = _4K; /** @todo Improve. */
        void    *pvMeta = RTMemAlloc(cbMeta);

        if (pvMeta)
        {
            uint32_t cbMetaToRead = pTransfer->State.Header.cbMeta;
            while (cbMetaToRead)
            {
                uint32_t cbMetaRead;
                rc = pTransfer->pProvider->ReadDataChunk(&pTransfer->State.Header, pvMeta, cbMeta, &cbMetaRead);
                if (RT_SUCCESS(rc))
                    rc = sharedClipboardURITransferMetaDataAddInternal(pTransfer, pvMeta, cbMeta);

                if (RT_FAILURE(rc))
                    break;

                Assert(cbMetaToRead >= cbMetaRead);
                cbMetaToRead -= cbMetaRead;

                cbReadTotal += cbReadTotal;
            }

            RTMemFree(pvMeta);

            if (RT_SUCCESS(rc))
            {
                if (pcbRead)
                    *pcbRead = cbReadTotal;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes the actual meta data.
 *
 * @returns IPRT status code.
 * @param   pTransfer           Transfer to write meta data for.
 * @param   pcbWritten          How much bytes were written on success. Optional.
 */
int SharedClipboardURITransferMetaDataWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t *pcbWritten)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    AssertPtr(pTransfer->pProvider);

    uint32_t cbWrittenTotal = 0;

    int rc = pTransfer->pProvider->WriteDataHdr(&pTransfer->State.Header);
    if (RT_SUCCESS(rc))
    {
        /* Sanity. */
        Assert(pTransfer->State.Header.cbMeta == pTransfer->State.Meta.cbUsed);

        uint32_t cbMetaToWrite = pTransfer->State.Header.cbMeta;
        while (cbMetaToWrite)
        {
            uint32_t cbMetaWritten;
            rc = pTransfer->pProvider->WriteDataChunk(&pTransfer->State.Header, (uint8_t *)pTransfer->State.Meta.pvMeta + cbWrittenTotal,
                                                      cbMetaToWrite, &cbMetaWritten, 0 /* fFlags */);
            if (RT_SUCCESS(rc))
            {
                cbWrittenTotal += cbMetaWritten;
                Assert(cbWrittenTotal <= pTransfer->State.Header.cbMeta);
            }
        }

        if (RT_SUCCESS(rc))
        {
            if (pcbWritten)
                *pcbWritten = cbWrittenTotal;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes all URI objects using the connected provider.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to write objects for.
 */
int SharedClipboardURITransferWriteObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    AssertPtr(pTransfer->pProvider);

    while (!pTransfer->URIList.IsEmpty())
    {
        SharedClipboardURIObject *pObj = pTransfer->URIList.First();
        AssertPtrBreakStmt(pObj, rc = VERR_INVALID_POINTER);

        switch (pObj->GetType())
        {
            case SharedClipboardURIObject::Type_Directory:
            {
                VBOXCLIPBOARDDIRDATA dirData;
                RT_ZERO(dirData);

                dirData.pszPath = RTStrDup(pObj->GetDestPathAbs().c_str());
                dirData.cbPath  = (uint32_t)strlen(dirData.pszPath);

                rc = pTransfer->pProvider->WriteDirectory(&dirData);
                break;
            }

            case SharedClipboardURIObject::Type_File:
            {
                VBOXCLIPBOARDFILEHDR fileHdr;
                RT_ZERO(fileHdr);

                fileHdr.pszFilePath = RTStrDup(pObj->GetDestPathAbs().c_str());
                fileHdr.cbFilePath  = (uint32_t)strlen(fileHdr.pszFilePath);
                fileHdr.cbSize      = pObj->GetSize();
                fileHdr.fFlags      = 0;
                fileHdr.fMode       = pObj->GetMode();

                rc = pTransfer->pProvider->WriteFileHdr(&fileHdr);
                if (RT_FAILURE(rc))
                    break;

                uint32_t cbData = _4K; /** @todo Improve. */
                void    *pvData = RTMemAlloc(cbData);

                while (!pObj->IsComplete())
                {
                    VBOXCLIPBOARDFILEDATA fileData;
                    RT_ZERO(fileData);

                    uint32_t cbRead;
                    rc = pObj->Read(pvData, cbData, &cbRead);
                    if (RT_SUCCESS(rc))
                    {
                        fileData.pvData = pvData;
                        fileData.cbData = cbRead;

                        uint32_t cbWritten;
                        rc = pTransfer->pProvider->WriteFileData(&fileData, &cbWritten);
                    }

                    if (RT_FAILURE(rc))
                        break;
                }
                break;
            }

            default:
                AssertFailed();
                break;
        }

        if (RT_FAILURE(rc))
            break;

        /* Only remove current object on success. */
        pTransfer->URIList.RemoveFirst();
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Thread for transferring (writing) URI objects from source to the target.
 * For target to source transfers we utilize our own IDataObject / IStream implementations.
 *
 * @returns VBox status code.
 * @param   hThread             Thread handle.
 * @param   pvUser              User arguments; is PSHAREDCLIPBOARDURITRANSFER.
 */
static int sharedClipboardURITransferWriteThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* At the moment we only support one transfer at a time. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer = (PSHAREDCLIPBOARDURITRANSFER)pvUser;
    AssertPtr(pTransfer->pProvider);

    int rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        pTransfer->Thread.fStarted = true;

    int rc2 = RTThreadUserSignal(hThread);
    const bool fSignalled = RT_SUCCESS(rc2);

    if (RT_SUCCESS(rc))
        rc = SharedClipboardURITransferWrite(pTransfer);

    if (!fSignalled)
    {
        rc2 = RTThreadUserSignal(hThread);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function to write a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to write.
 */
int SharedClipboardURITransferWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = SharedClipboardURITransferMetaDataWrite(pTransfer, NULL /* pcbWritten */);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardURITransferWriteObjects(pTransfer);
        if (RT_SUCCESS(rc))
        {
            if (pTransfer->Callbacks.pfnTransferComplete)
            {
                SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
                pTransfer->Callbacks.pfnTransferComplete(&callbackData, rc);
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pTransfer->Callbacks.pfnTransferError)
        {
            SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
            pTransfer->Callbacks.pfnTransferError(&callbackData, rc);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Initializes a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to initialize.
 */
int SharedClipboardURICtxInit(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = RTCritSectInit(&pURI->CritSect);
    if (RT_SUCCESS(rc))
    {
        RTListInit(&pURI->List);

        pURI->cTransfers    = 0;
        pURI->cMaxTransfers = 1; /* For now we only support one transfer per client at a time. */

#ifdef DEBUG_andy
        pURI->cMaxTransfers = UINT32_MAX;
#endif

        SharedClipboardURICtxReset(pURI);
    }

    return VINF_SUCCESS;
}

/**
 * Destroys an URI clipboard information context struct.
 *
 * @param   pURI                URI clipboard context to destroy.
 */
void SharedClipboardURICtxDestroy(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    RTCritSectDelete(&pURI->CritSect);

    PSHAREDCLIPBOARDURITRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pURI->List, pTransfer, pTransferNext, SHAREDCLIPBOARDURITRANSFER, Node)
    {
        SharedClipboardURITransferDestroy(pTransfer);
        RTListNodeRemove(&pTransfer->Node);
    }

    LogFlowFuncEnter();
}

/**
 * Resets an clipboard URI transfer.
 *
 * @param   pURI                URI clipboard context to reset.
 */
void SharedClipboardURICtxReset(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFuncEnter();

    PSHAREDCLIPBOARDURITRANSFER pTransfer;
    RTListForEach(&pURI->List, pTransfer, SHAREDCLIPBOARDURITRANSFER, Node)
        SharedClipboardURITransferReset(pTransfer);
}

/**
 * Adds a new URI transfer to an clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to add transfer to.
 * @param   pTransfer           Pointer to URI clipboard transfer to add.
 */
int SharedClipboardURICtxTransferAdd(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pURI,      VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    if (pURI->cTransfers == pURI->cMaxTransfers)
        return VERR_SHCLPB_MAX_TRANSFERS_REACHED;

    RTListAppend(&pURI->List, &pTransfer->Node);
    pURI->cTransfers++;

    return VINF_SUCCESS;
}

/**
 * Removes an URI transfer from a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to remove transfer from.
 * @param   pTransfer           Pointer to URI clipboard transfer to remove.
 */
int SharedClipboardURICtxTransferRemove(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pURI,      VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Sanity. */
    AssertReturn(pURI->cTransfers, VERR_WRONG_ORDER);

    int rc = SharedClipboardURITransferDestroy(pTransfer);
    if (RT_SUCCESS(rc))
    {

        RTListNodeRemove(&pTransfer->Node);
        Assert(pURI->cTransfers);
        pURI->cTransfers--;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns a specific URI transfer, internal version.
 *
 * @returns URI transfer, or NULL if not found.
 * @param   pURI                URI clipboard context to return transfer for.
 * @param   uIdx                Index of the transfer to return.
 */
static PSHAREDCLIPBOARDURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx)
{
    AssertReturn(uIdx == 0, NULL); /* Only one transfer allowed at the moment. */
    return RTListGetFirst(&pURI->List, SHAREDCLIPBOARDURITRANSFER, Node);
}

/**
 * Returns a specific URI transfer.
 *
 * @returns URI transfer, or NULL if not found.
 * @param   pURI                URI clipboard context to return transfer for.
 * @param   uIdx                Index of the transfer to return.
 */
PSHAREDCLIPBOARDURITRANSFER SharedClipboardURICtxGetTransfer(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx)
{
    return sharedClipboardURICtxGetTransferInternal(pURI, uIdx);
}

/**
 * Returns the number of active URI transfers.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to return number for.
 */
uint32_t SharedClipboardURICtxGetActiveTransfers(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, 0);
    return pURI->cTransfers;
}

/**
 * Returns whether the maximum of concurrent transfers of a specific URI context has been reached or not.
 *
 * @returns \c if maximum has been reached, \c false if not.
 * @param   pURI                URI clipboard context to determine value for.
 */
bool SharedClipboardURICtxMaximumTransfersReached(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, true);

    LogFlowFunc(("cTransfers=%RU32\n", pURI->cTransfers));

    Assert(pURI->cTransfers <= pURI->cMaxTransfers);
    return pURI->cTransfers == pURI->cMaxTransfers;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

