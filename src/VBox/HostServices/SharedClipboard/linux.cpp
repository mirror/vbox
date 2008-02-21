/** @file
 *
 * Shared Clipboard:
 * Linux host.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define USE_UTF16
#define USE_UTF8
#define USE_CTEXT

#include <vector>

#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/alloc.h>
#include <iprt/asm.h>        /* For atomic operations */
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "VBoxClipboard.h"
#include "clipboard-helper.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xproto.h>

/** The different clipboard formats which we support. */
enum g_eClipboardFormats
{
    INVALID = 0,
    TARGETS,
    CTEXT,
    UTF8,
    UTF16
};

/** The X11 clipboard uses several names for the same format.  This structure maps an X11
    name to a format. */
typedef struct {
    Atom atom;
    g_eClipboardFormats format;
    unsigned guestFormat;
} VBOXCLIPBOARDFORMAT;

/** Does the host or the guest currently own the clipboard? */
enum g_eClipboardOwner { NONE = 0, HOST, GUEST };

typedef struct {
    /** BMP file type marker - must always contain 'BM' */
    uint16_t bfType;
    /** The size of the BMP file in bytes (the MS docs say that this is not reliable) */
    uint32_t bfSize;
    /** Reserved, must always be zero */
    uint16_t bfReserved1;
    /** Reserved, must always be zero */
    uint16_t bfReserved2;
    /** Offset from the beginning of this header to the actual image bits */
} VBOXBITMAPFILEHEADER;

/** Global clipboard context information */
struct _VBOXCLIPBOARDCONTEXT
{
    /** The X Toolkit application context structure */
    XtAppContext appContext;

    /** We have a separate thread to wait for Window and Clipboard events */
    RTTHREAD thread;
    /** The X Toolkit widget which we use as our clipboard client.  It is never made visible. */
    Widget widget;

    /** X11 atom refering to the clipboard: CLIPBOARD */
    Atom atomClipboard;
    /** X11 atom refering to the clipboard targets: TARGETS */
    Atom atomTargets;
    /** X11 atom refering to the clipboard multiple target: MULTIPLE */
    Atom atomMultiple;
    /** X11 atom refering to the clipboard timestamp target: TIMESTAMP */
    Atom atomTimestamp;
    /** X11 atom refering to the clipboard utf16 text format: text/plain;charset=ISO-10646-UCS-2 */
    Atom atomUtf16;
    /** X11 atom refering to the clipboard utf8 text format: UTF8_STRING */
    Atom atomUtf8;
    /** X11 atom refering to the clipboard compound text format: COMPOUND_TEXT */
    Atom atomCText;

    /** A list of the X11 formats which we support, mapped to our identifier for them, in the
        order we prefer to have them in. */
    std::vector<VBOXCLIPBOARDFORMAT> formatList;

    /** Does the host or the guest currently own the clipboard? */
    volatile enum g_eClipboardOwner eOwner;

    /** What is the best text format the host has to offer?  INVALID for none. */
    g_eClipboardFormats hostTextFormat;
    /** Atom corresponding to the host text format */
    Atom atomHostTextFormat;
    /** What is the best bitmap format the host has to offer?  INVALID for none. */
    g_eClipboardFormats hostBitmapFormat;
    /** Atom corresponding to the host Bitmap format */
    Atom atomHostBitmapFormat;
    /** What formats does the guest have on offer? */
    int guestFormats;
    /** Windows caches the clipboard data it receives.  Since we have no way of knowing whether
        that data is still valid, we always send a "data changed" message after a successful
        transfer to invalidate the cache. */
    bool notifyGuest;

    /** Since the clipboard data moves asynchronously, we use an event semaphore to wait for
        it.  When a function issues a request for clipboard data it must wait for this
        semaphore, which is triggered when the data arrives. */
    RTSEMEVENT waitForData;
    /** And because it would not do for the guest to be waiting for the host while the host
        is waiting for the guest, we set a flag and assert horribly if we spot a deadlock. */
    uint32_t waiter;
    /** This mutex is held while an asynchronous operation completes (i.e. the host clipboard is
        being queried) to make sure that the clipboard is not disconnected during that time.  It
        is also grabbed when the clipboard client disconnects.  When an asynchronous operation
        starts completing, it checks that the same client is still connected immediately after
        grabbing the mutex. */
    RTSEMMUTEX asyncMutex;

    /** Format which we are reading from the host clipboard (valid during a request for the
        host clipboard) */
    g_eClipboardFormats requestHostFormat;
    /** The guest buffer to write host clipboard data to (valid during a request for the host
        clipboard) */
    void *requestBuffer;
    /** The size of the guest buffer to write host clipboard data to (valid during a request for
        the host clipboard) */
    unsigned requestBufferSize;
    /** The size of the host clipboard data written to the guest buffer (valid during a request
        for the host clipboard) */
    uint32_t *requestActualSize;

    /** Pointer to the client data structure */
    VBOXCLIPBOARDCLIENTDATA *pClient;
};

/* Only one client is supported. There seems to be no need for more clients. */
static VBOXCLIPBOARDCONTEXT g_ctx;

/**
 * Send a request to the guest to transfer the contents of its clipboard to the host.
 *
 * @param pCtx      Pointer to the host clipboard structure
 * @param u32Format The format in which the data should be transfered
 */
static int vboxClipboardReadDataFromClient (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format)
{
    VBOXCLIPBOARDCLIENTDATA *pClient = pCtx->pClient;

    LogFlowFunc(("u32Format=%02X\n", u32Format));
    if (pClient == 0)
    {
        Log(("vboxClipboardReadDataFromClient: host requested guest clipboard data after guest had disconnected.\n"));
        pCtx->guestFormats = 0;
        pCtx->waiter = 0;
        return VERR_TIMEOUT;
    }
    if (!(   pCtx->pClient->data.pv == NULL
          && pCtx->pClient->data.cb == 0
          && pCtx->pClient->data.u32Format == 0))
    {
        LogRel(("vboxClipboardReadDataFromClient: a guest to host clipboard transfer has been requested, but another is in progress, or has not cleaned up properly.\n"));
        AssertMsgFailed(("A guest to host clipboard transfer has been requested, but another is in progress, or has not cleaned up properly.\n"));
    }

    /* Only one of the guest and the host should be waiting at any one time */
    if (RT_FAILURE(ASMAtomicCmpXchgU32(&pCtx->waiter, 1, 0)))
    {
        LogRel(("vboxClipboardReadDataFromClient: deadlock situation - the host and the guest are both waiting for data from the other."));
        return VERR_DEADLOCK;
    }
    /* Request data from the guest */
    vboxSvcClipboardReportMsg (pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, u32Format);
    /* Which will signal us when it is ready. */
    if (RTSemEventWait(pCtx->waitForData, CLIPBOARDTIMEOUT) != VINF_SUCCESS)
    {
        LogRel (("vboxClipboardReadDataFromClient: vboxSvcClipboardReportMsg failed to complete within %d milliseconds\n", CLIPBOARDTIMEOUT));
        pCtx->guestFormats = 0;
        pCtx->waiter = 0;
        return VERR_TIMEOUT;
    }
    pCtx->waiter = 0;
    LogFlowFunc(("wait completed.  Returning.\n"));
    return VINF_SUCCESS;
}

/**
 * Convert the UTF-16 text returned from the X11 clipboard to UTF-16LE with Windows EOLs
 * and place it in the global g_pcClipboardText variable.  We are reading the host clipboard to
 * make it available to the guest.
 *
 * @param pValue      Source UTF-16 text
 * @param cwSourceLen Length in 16-bit words of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by cb
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 */
static void vboxClipboardGetUtf16(XtPointer pValue, unsigned cwSrcLen, void *pv, unsigned cb,
                                  uint32_t *pcbActual)
{
    size_t cwDestLen;
    PRTUTF16 pu16SrcText = reinterpret_cast<PRTUTF16>(pValue);
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    int rc;

    LogFlowFunc (("converting Utf-16 to Utf-16LE.  cwSrcLen=%d, cb=%d, pu16SrcText+1=%.*ls\n",
                   cwSrcLen, cb, cwSrcLen - 1, pu16SrcText + 1));
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetWinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        XtFree(reinterpret_cast<char *>(pValue));
        LogRel(("vboxClipboardGetUtf16: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Vrc.  Abandoning.\n", rc));
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        AssertReturnVoid(RT_SUCCESS(rc));
    }
    if (cb < cwDestLen * 2)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        /* Report the amount of buffer space needed for the transfer */
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16LinToWin(pu16SrcText, cwSrcLen, pu16DestText, cb / 2);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetUtf16: clipboard conversion failed.  vboxClipboardUtf16LinToWin returned %Vrc.  Abandoning.\n", rc));
        XtFree(reinterpret_cast<char *>(pValue));
        *pcbActual = 0;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    LogFlowFunc (("converted string is %.*ls. Returning.\n", cwDestLen, pu16DestText));
    *pcbActual = cwDestLen * 2;
    XtFree(reinterpret_cast<char *>(pValue));
    RTSemEventSignal(g_ctx.waitForData);
}

/**
 * Convert the UTF-8 text returned from the X11 clipboard to UTF-16LE with Windows EOLS.
 * We are reading the host clipboard to make it available to the guest.
 *
 * @param pValue      Source UTF-8 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by pv
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 */
static void vboxClipboardGetUtf8(XtPointer pValue, unsigned cbSrcLen, void *pv, unsigned cb,
                                 uint32_t *pcbActual)
{
    size_t cwSrcLen, cwDestLen;
    char *pu8SrcText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16SrcText;
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    int rc;

    LogFlowFunc (("converting Utf-8 to Utf-16LE.  cbSrcLen=%d, cb=%d, pu8SrcText=%.*s\n",
                   cbSrcLen, cb, cbSrcLen, pu8SrcText));
    /* First convert the UTF8 to UTF16 */
    rc = RTStrToUtf16Ex(pu8SrcText, cbSrcLen, &pu16SrcText, 0, &cwSrcLen);
    XtFree(reinterpret_cast<char *>(pValue));
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetUtf8: clipboard conversion failed.  RTStrToUtf16Ex returned %Vrc.  Abandoning.\n", rc));
        *pcbActual = 0;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    /* Check how much longer will the converted text will be. */
    rc = vboxClipboardUtf16GetWinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetUtf8: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Vrc.  Abandoning.\n", rc));
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        RTUtf16Free(pu16SrcText);
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        AssertReturnVoid(RT_SUCCESS(rc));
    }
    if (cb < cwDestLen * 2)
    {
        RTUtf16Free(pu16SrcText);
        /* Report the amount of buffer space needed for the transfer */
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16LinToWin(pu16SrcText, cwSrcLen, pu16DestText, cb / 2);
    RTUtf16Free(pu16SrcText);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetUtf8: clipboard conversion failed.  vboxClipboardUtf16LinToWin returned %Vrc.  Abandoning.\n", rc));
        *pcbActual = 0;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    LogFlowFunc (("converted string is %.*ls. Returning.\n", cwDestLen, pu16DestText));
    *pcbActual = cwDestLen * 2;
    RTSemEventSignal(g_ctx.waitForData);
}

/**
 * Convert the COMPOUND_TEXT text returned from the X11 clipboard to UTF-16LE with Windows
 * EOLS.  We are reading the host clipboard to make it available to the guest.
 *
 * @param pValue      Source COMPOUND_TEXT text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by pv
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 */
static void vboxClipboardGetCText(XtPointer pValue, unsigned cbSrcLen, void *pv, unsigned cb,
                                  uint32_t *pcbActual)
{
    size_t cwSrcLen, cwDestLen;
    char **ppu8SrcText;
    PRTUTF16 pu16SrcText;
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    XTextProperty property;
    int rc, cProps;

    LogFlowFunc (("converting COMPOUND TEXT to Utf-16LE.  cbSrcLen=%d, cb=%d, pu8SrcText=%.*s\n",
                   cbSrcLen, cb, cbSrcLen, reinterpret_cast<char *>(pValue)));
    /* First convert the compound text to Utf8 */
    property.value = reinterpret_cast<unsigned char *>(pValue);
    property.encoding = g_ctx.atomCText;
    property.format = 8;
    property.nitems = cbSrcLen;
#ifdef RT_OS_SOLARIS
    rc = XmbTextPropertyToTextList(XtDisplay(g_ctx.widget), &property, &ppu8SrcText, &cProps);
#else
    rc = Xutf8TextPropertyToTextList(XtDisplay(g_ctx.widget), &property, &ppu8SrcText, &cProps);
#endif
    XtFree(reinterpret_cast<char *>(pValue));
    if (rc < 0)
    {
        const char *pcReason;
        switch(rc)
        {
        case XNoMemory:
            pcReason = "out of memory";
            break;
        case XLocaleNotSupported:
            pcReason = "locale (Utf8) not supported";
            break;
        case XConverterNotFound:
            pcReason = "converter not found";
            break;
        default:
            pcReason = "unknown error";
        }
        XFreeStringList(ppu8SrcText);
        LogRel(("vboxClipboardGetCText: Xutf8TextPropertyToTextList failed.  Reason: %s\n",
                pcReason));
        *pcbActual = 0;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    /* Now convert the UTF8 to UTF16 */
    rc = RTStrToUtf16Ex(*ppu8SrcText, cbSrcLen, &pu16SrcText, 0, &cwSrcLen);
    XFreeStringList(ppu8SrcText);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetCText: clipboard conversion failed.  RTStrToUtf16Ex returned %Vrc.  Abandoning.\n", rc));
        *pcbActual = 0;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    /* Check how much longer will the converted text will be. */
    rc = vboxClipboardUtf16GetWinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetCText: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Vrc.  Abandoning.\n", rc));
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        RTUtf16Free(pu16SrcText);
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        AssertReturnVoid(RT_SUCCESS(rc));
    }
    if (cb < cwDestLen * 2)
    {
        RTUtf16Free(pu16SrcText);
        /* Report the amount of buffer space needed for the transfer */
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16LinToWin(pu16SrcText, cwSrcLen, pu16DestText, cb / 2);
    RTUtf16Free(pu16SrcText);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardGetCText: clipboard conversion failed.  vboxClipboardUtf16LinToWin returned %Vrc.  Abandoning.\n", rc));
        *pcbActual = 0;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    LogFlowFunc (("converted string is %.*ls. Returning.\n", cwDestLen, pu16DestText));
    *pcbActual = cwDestLen * 2;
    RTSemEventSignal(g_ctx.waitForData);
}

/**
 * Convert the Latin1 text returned from the X11 clipboard to UTF-16LE with Windows EOLS
 * and place it in the global g_pcClipboardText variable.  We are reading the host clipboard to
 * make it available to the guest.
 *
 * @param pValue      Source Latin1 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by cb
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 */
static void vboxClipboardGetLatin1(XtPointer pValue, unsigned cbSourceLen, void *pv, unsigned cb,
                                   uint32_t *pcbActual)
{
    unsigned cwDestLen = cbSourceLen + 1;
    char *pu8SourceText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);

    LogFlow (("vboxClipboardGetLatin1: converting Latin1 to Utf-16LE.  Original is %.*s\n",
              cbSourceLen, pu8SourceText));
    for (unsigned i = 0; i < cbSourceLen; i++)
        if (pu8SourceText[i] == LINEFEED)
            ++cwDestLen;
    if (cb < cwDestLen * 2)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        /* Report the amount of buffer space needed for the transfer */
        Log2 (("vboxClipboardGetLatin1: guest buffer too small: size %d bytes\n", cb));
        *pcbActual = cwDestLen * 2;
        RTSemEventSignal(g_ctx.waitForData);
        return;
    }
    for (unsigned i = 0, j = 0; i < cbSourceLen; ++i, ++j)
        if (pu8SourceText[i] != LINEFEED)
            pu16DestText[j] = pu8SourceText[i];  /* latin1 < utf-16LE */
        else
        {
            pu16DestText[j] = CARRIAGERETURN;
            ++j;
            pu16DestText[j] = LINEFEED;
        }
    pu16DestText[cwDestLen - 1] = 0;
    *pcbActual = cwDestLen * 2;
    Log2 (("vboxClipboardGetLatin1: converted text is %.*ls\n", cwDestLen, pu16DestText));
    XtFree(reinterpret_cast<char *>(pValue));
    RTSemEventSignal(g_ctx.waitForData);
}

/** Convert the clipboard text from the current format to Utf-16 with Windows line breaks.
    We are reading the host clipboard to make it available to the guest. */
static void vboxClipboardGetProc(Widget, XtPointer pClientData, Atom * /* selection */,
                                 Atom *atomType, XtPointer pValue, long unsigned int *pcLen,
                                 int *piFormat)
{
    LogFlowFunc(("pClientData=%p, *pcLen=%lu, *piFormat=%d\n", pClientData, *pcLen, *piFormat));
    LogFlowFunc(("g_ctx.requestHostFormat=%d, g_ctx.requestBufferSize=%d\n",
                 g_ctx.requestHostFormat, g_ctx.requestBufferSize));
    unsigned cTextLen = (*pcLen) * (*piFormat) / 8;
    /* The X Toolkit may have failed to get the clipboard selection for us. */
    if (*atomType == XT_CONVERT_FAIL)
        return;
    /* The clipboard selection may have changed before we could get it. */
    if (NULL == pValue)
        return;
    /* We grab this mutex whenever an asynchronous clipboard operation completes and while
       disconnecting a client from the clipboard to stop these operations colliding. */
    RTSemMutexRequest(g_ctx.asyncMutex, RT_INDEFINITE_WAIT);
    if (reinterpret_cast<VBOXCLIPBOARDCLIENTDATA *>(pClientData) != g_ctx.pClient)
    {
        /* If the client is no longer connected, just return. */
        XtFree(reinterpret_cast<char *>(pValue));
        LogFlowFunc(("client is no longer connected, returning\n"));
        RTSemMutexRelease(g_ctx.asyncMutex);
        return;
    }

    /* In which format did we request the clipboard data? */
    switch (g_ctx.requestHostFormat)
    {
    case UTF16:
        vboxClipboardGetUtf16(pValue, cTextLen / 2, g_ctx.requestBuffer, g_ctx.requestBufferSize,
                              g_ctx.requestActualSize);
        break;
    case CTEXT:
        vboxClipboardGetCText(pValue, cTextLen, g_ctx.requestBuffer, g_ctx.requestBufferSize,
                              g_ctx.requestActualSize);
        break;
    case UTF8:
    {
        /* If we are given broken Utf-8, we treat it as Latin1.  Is this acceptable? */
        size_t cStringLen;
        char *pu8SourceText = reinterpret_cast<char *>(pValue);

        if ((g_ctx.requestHostFormat == UTF8)
            && (RTStrUniLenEx(pu8SourceText, *pcLen, &cStringLen) == VINF_SUCCESS))
        {
            vboxClipboardGetUtf8(pValue, cTextLen, g_ctx.requestBuffer, g_ctx.requestBufferSize,
                                 g_ctx.requestActualSize);
            break;
        }
        else
        {
            vboxClipboardGetLatin1(pValue, cTextLen, g_ctx.requestBuffer, g_ctx.requestBufferSize,
                                   g_ctx.requestActualSize);
            break;
        }
    }
    default:
        Log (("vboxClipboardGetProc: bad target format\n"));
        XtFree(reinterpret_cast<char *>(pValue));
        RTSemMutexRelease(g_ctx.asyncMutex);
        return;
    }
    g_ctx.notifyGuest = true;
    RTSemMutexRelease(g_ctx.asyncMutex);
}

/** Callback to handle a reply to a request for the targets the current clipboard holder can
    handle.  We are reading the host clipboard to make it available to the guest. */
static void vboxClipboardTargetsProc(Widget, XtPointer pClientData, Atom * /* selection */,
                                     Atom *atomType, XtPointer pValue, long unsigned int *pcLen,
                                     int *piFormat)
{
    Atom *atomTargets = reinterpret_cast<Atom *>(pValue);
    unsigned cAtoms = *pcLen;
    g_eClipboardFormats eBestTarget = INVALID;
    Atom atomBestTarget = None;

    Log3 (("vboxClipboardTargetsProc called\n"));
    if (*atomType == XT_CONVERT_FAIL)
    {
        Log (("vboxClipboardTargetsProc: reading clipboard from host, X toolkit failed to convert the selection\n"));
        return;
    }
    /* We grab this mutex whenever an asynchronous clipboard operation completes and while
       disconnecting a client from the clipboard to stop these operations colliding. */
    RTSemMutexRequest(g_ctx.asyncMutex, RT_INDEFINITE_WAIT);
    if (reinterpret_cast<VBOXCLIPBOARDCLIENTDATA *>(pClientData) != g_ctx.pClient)
    {
        /* If the client is no longer connected, just return. */
        LogFlowFunc(("client is no longer connected, returning\n"));
        RTSemMutexRelease(g_ctx.asyncMutex);
        return;
    }

    for (unsigned i = 0; i < cAtoms; ++i)
    {
        for (unsigned j = 0; j != g_ctx.formatList.size(); ++j)
            if (g_ctx.formatList[j].atom == atomTargets[i])
            {
                if (eBestTarget < g_ctx.formatList[j].format)
                {
                    eBestTarget = g_ctx.formatList[j].format;
                    atomBestTarget = g_ctx.formatList[j].atom;
                }
                break;
            }
#ifdef DEBUG
        char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
        if (szAtomName != 0)
        {
            Log3 (("vboxClipboardTargetsProc: the host offers target %s\n", szAtomName));
            XFree(szAtomName);
        }
#endif
    }
    g_ctx.atomHostTextFormat = atomBestTarget;
    if ((eBestTarget != g_ctx.hostTextFormat) || (g_ctx.notifyGuest == true))
    {
        uint32_t u32Formats = 0;
#ifdef DEBUG
        if (atomBestTarget != None)
        {
            char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomBestTarget);
            Log2 (("vboxClipboardTargetsProc: switching to host text target %s.  Available targets are:\n",
                   szAtomName));
            XFree(szAtomName);
        }
        else
        {
            Log2(("vboxClipboardTargetsProc: no supported host text target found.  Available targets are:\n"));
        }
        for (unsigned i = 0; i < cAtoms; ++i)
        {
            char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
            if (szAtomName != 0)
            {
                Log2 (("vboxClipboardTargetsProc:     %s\n", szAtomName));
                XFree(szAtomName);
            }
        }
#endif
        g_ctx.hostTextFormat = eBestTarget;
        if (eBestTarget != INVALID)
            u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        vboxSvcClipboardReportMsg (g_ctx.pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS,
                                   u32Formats);
        g_ctx.notifyGuest = false;
    }
    XtFree(reinterpret_cast<char *>(pValue));
    RTSemMutexRelease(g_ctx.asyncMutex);
}

/**
 * This callback is called every 200ms to check the contents of the host clipboard.
 */
static void vboxClipboardTimerProc(XtPointer /* pUserData */, XtIntervalId * /* hTimerId */)
{
    Log3 (("vboxClipboardTimerProc called\n"));
    /* Get the current clipboard contents */
    if (g_ctx.eOwner == HOST && g_ctx.pClient != 0)
    {
        Log3 (("vboxClipboardTimerProc: requesting the targets that the host clipboard offers\n"));
        XtGetSelectionValue(g_ctx.widget, g_ctx.atomClipboard, g_ctx.atomTargets,
                            vboxClipboardTargetsProc, reinterpret_cast<XtPointer>(g_ctx.pClient),
                            CurrentTime);
    }
    /* Re-arm our timer */
    XtAppAddTimeOut(g_ctx.appContext, 200 /* ms */, vboxClipboardTimerProc, 0);
}

/** We store information about the target formats we can handle in a global vector for internal
    use. */
static void vboxClipboardAddFormat(const char *pszName, g_eClipboardFormats eFormat,
                                   unsigned guestFormat)
{
    VBOXCLIPBOARDFORMAT sFormat;
    /* Get an atom from the X server for that target format */
    Atom atomFormat = XInternAtom(XtDisplay(g_ctx.widget), pszName, false);
    sFormat.atom   = atomFormat;
    sFormat.format = eFormat;
    sFormat.guestFormat = guestFormat;
    g_ctx.formatList.push_back(sFormat);
    LogFlow (("vboxClipboardAddFormat: added format %s (%d)\n", pszName, eFormat));
}

/**
 * The main loop of our clipboard reader.
 */
static int vboxClipboardThread(RTTHREAD self, void * /* pvUser */)
{
    /* Create a window and make it a clipboard viewer. */
    int cArgc = 0;
    char *pcArgv = 0;
    int rc = VINF_SUCCESS;
    String szFallbackResources[] = { (char*)"*.width: 1", (char*)"*.height: 1", 0 };
    Display *pDisplay;
    LogRel (("vboxClipboardThread: starting clipboard thread\n"));

    /* Make sure we are thread safe */
    XtToolkitThreadInitialize();
    /* Set up the Clipbard application context and main window.  We call all these functions
       directly instead of calling XtOpenApplication() so that we can fail gracefully if we
       can't get an X11 display. */
    XtToolkitInitialize();
    g_ctx.appContext = XtCreateApplicationContext();
    XtAppSetFallbackResources(g_ctx.appContext, szFallbackResources);
    pDisplay = XtOpenDisplay(g_ctx.appContext, 0, 0, "VBoxClipboard", 0, 0, &cArgc, &pcArgv);
    if (pDisplay == 0)
    {
        LogRel(("vboxClipboardThread: failed to connect to the host clipboard - the window system may not be running.\n"));
        return VERR_NOT_SUPPORTED;
    }
    g_ctx.widget = XtAppCreateShell(0, "VBoxClipboard", applicationShellWidgetClass, pDisplay,
                                    0, 0);
    if (g_ctx.widget == 0)
    {
        LogRel(("vboxClipboardThread: failed to construct the X11 window for the clipboard manager.\n"));
        AssertReturn(g_ctx.widget != 0, VERR_ACCESS_DENIED);
    }
    RTThreadUserSignal(self);
    XtSetMappedWhenManaged(g_ctx.widget, false);
    XtRealizeWidget(g_ctx.widget);

    /* Get hold of the atoms which we need */
    g_ctx.atomClipboard = XInternAtom(XtDisplay(g_ctx.widget), "CLIPBOARD", false /* only_if_exists */);
    g_ctx.atomTargets   = XInternAtom(XtDisplay(g_ctx.widget), "TARGETS",   false);
    g_ctx.atomMultiple  = XInternAtom(XtDisplay(g_ctx.widget), "MULTIPLE",  false);
    g_ctx.atomTimestamp = XInternAtom(XtDisplay(g_ctx.widget), "TIMESTAMP", false);
    g_ctx.atomUtf16     = XInternAtom(XtDisplay(g_ctx.widget),
                                      "text/plain;charset=ISO-10646-UCS-2", false);
    g_ctx.atomUtf8      = XInternAtom(XtDisplay(g_ctx.widget), "UTF_STRING", false);
    /* And build up the vector of supported formats */
    g_ctx.atomCText     = XInternAtom(XtDisplay(g_ctx.widget), "COMPOUND_TEXT", false);
    /* And build up the vector of supported formats */
#ifdef USE_UTF16
    vboxClipboardAddFormat("text/plain;charset=ISO-10646-UCS-2", UTF16,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif
#ifdef USE_UTF8
    vboxClipboardAddFormat("UTF8_STRING", UTF8,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain;charset=UTF-8", UTF8,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain;charset=utf-8", UTF8,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("STRING", UTF8,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("TEXT", UTF8,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain", UTF8,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif
#ifdef USE_CTEXT
    vboxClipboardAddFormat("COMPOUND_TEXT", CTEXT,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif

    /* Set up a timer to poll the host clipboard */
    XtAppAddTimeOut(g_ctx.appContext, 200 /* ms */, vboxClipboardTimerProc, 0);

    XtAppMainLoop(g_ctx.appContext);
    g_ctx.formatList.clear();
    RTSemEventDestroy(g_ctx.waitForData);
    RTSemMutexDestroy(g_ctx.asyncMutex);
    LogRel (("vboxClipboardThread: clipboard thread terminated successfully with return code %Vrc\n", rc));
    return rc;
}

/** Initialise the host side of the shared clipboard - called by the hgcm layer. */
int vboxClipboardInit (void)
{
    int rc;

    LogRel(("vboxClipboardInit: initializing host clipboard\n"));
    RTSemEventCreate(&g_ctx.waitForData);
    RTSemMutexCreate(&g_ctx.asyncMutex);
    rc = RTThreadCreate(&g_ctx.thread, vboxClipboardThread, 0, 0, RTTHREADTYPE_IO,
                        RTTHREADFLAGS_WAITABLE, "SHCLIP");
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardInit: failed to create the clipboard thread.\n"));
        AssertRCReturn(rc, rc);
    }
    return RTThreadUserWait(g_ctx.thread, 1000);
}

/** Terminate the host side of the shared clipboard - called by the hgcm layer. */
void vboxClipboardDestroy (void)
{
    LogRel(("vboxClipboardDestroy: shutting down host clipboard\n"));
    int rc, rcThread;
    XEvent ev;

    /* Set the termination flag. */
    XtAppSetExitFlag(g_ctx.appContext);
    /* Wake up the event loop */
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.format = 8;
    XSendEvent(XtDisplay(g_ctx.widget), XtWindow(g_ctx.widget), false, 0, &ev);
    XFlush(XtDisplay(g_ctx.widget));
    rc = RTThreadWait(g_ctx.thread, 2000, &rcThread);
    AssertRC(rc);
    AssertRC(rcThread);
    XtCloseDisplay(XtDisplay(g_ctx.widget));
    LogFlowFunc(("returning.\n"));
}

/**
  * Enable the shared clipboard - called by the hgcm clipboard subsystem.
  *
  * @param   pClient Structure containing context information about the guest system
  * @returns RT status code
  */
int vboxClipboardConnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    LogFlow(("vboxClipboardConnect\n"));

    /* Only one client is supported for now */
    if (g_ctx.pClient != 0)
    {
        LogRel(("vboxClipboardConnect: attempted to connect, but a client appears to be already running.\n"));
        AssertReturn(g_ctx.pClient == 0, VERR_NOT_SUPPORTED);
    }

    pClient->pCtx = &g_ctx;
    pClient->pCtx->pClient = pClient;
    g_ctx.eOwner = HOST;
    g_ctx.notifyGuest = true;
    return VINF_SUCCESS;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called by the HGCM layer
 * after a save and restore of the guest.
 */
int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    /* On a Linux host, the guest should never synchronise/cache its clipboard contents, as
       we have no way of reliably telling when the host clipboard data changes.  So instead
       of synchronising, we tell the guest to empty its clipboard, and we set the cached
       flag so that we report formats to the guest next time we poll for them. */
    vboxSvcClipboardReportMsg (g_ctx.pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, 0);
    g_ctx.notifyGuest = true;

    return VINF_SUCCESS;
}

/**
 * Shut down the shared clipboard subsystem and "disconnect" the guest.
 */
void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    LogFlow(("vboxClipboardDisconnect\n"));

    RTSemMutexRequest(g_ctx.asyncMutex, RT_INDEFINITE_WAIT);
    g_ctx.pClient = NULL;
    g_ctx.eOwner = NONE;
    g_ctx.hostTextFormat = INVALID;
    g_ctx.hostBitmapFormat = INVALID;
    RTSemMutexRelease(g_ctx.asyncMutex);
}

/**
 * Satisfy a request from the host for available clipboard targets.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param atomTypeReturn The type of the data we are returning
 * @param pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @param pcLenReturn    The length of the data we are returning
 * @param piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertTargets(Atom *atomTypeReturn, XtPointer *pValReturn,
                                           unsigned long *pcLenReturn, int *piFormatReturn)
{
    unsigned uListSize = g_ctx.formatList.size();
    Atom *atomTargets = reinterpret_cast<Atom *>(XtMalloc((uListSize + 3) * sizeof(Atom)));
    unsigned cTargets = 0;

    LogFlow (("vboxClipboardConvertTargets called\n"));
    for (unsigned i = 0; i < uListSize; ++i)
    {
        if (   ((g_ctx.guestFormats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT) != 0)
            && (g_ctx.formatList[i].guestFormat == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT))
        {
            atomTargets[cTargets] = g_ctx.formatList[i].atom;
            ++cTargets;
        }
    }
    atomTargets[cTargets] = g_ctx.atomTargets;
    atomTargets[cTargets + 1] = g_ctx.atomMultiple;
    atomTargets[cTargets + 2] = g_ctx.atomTimestamp;
#ifdef DEBUG
    for (unsigned i = 0; i < cTargets + 3; i++)
    {
        char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
        if (szAtomName != 0)
        {
            Log2 (("vboxClipboardConvertTargets: returning target %s\n", szAtomName));
            XFree(szAtomName);
        }
        else
        {
            Log(("vboxClipboardConvertTargets: invalid atom %d in the list!\n", atomTargets[i]));
        }
    }
#endif
    *atomTypeReturn = XA_ATOM;
    *pValReturn = reinterpret_cast<XtPointer>(atomTargets);
    *pcLenReturn = cTargets + 3;
    *piFormatReturn = 32;
    return true;
}

/**
 * Reset the contents of the buffer used to pass clipboard data from the guest to the host.
 * This must be done after every clipboard transfer.
 */
static void vboxClipboardEmptyGuestBuffer(void)
{
    if (g_ctx.pClient->data.pv != 0)
        RTMemFree(g_ctx.pClient->data.pv);
    g_ctx.pClient->data.pv = 0;
    g_ctx.pClient->data.cb = 0;
    g_ctx.pClient->data.u32Format = 0;
}

/**
 * Satisfy a request from the host to convert the clipboard text to Utf16.  We return non-zero
 * terminated text.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @retval atomTypeReturn The type of the data we are returning
 * @retval pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @retval pcLenReturn    The length of the data we are returning
 * @retval piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertUtf16(Atom *atomTypeReturn, XtPointer *pValReturn,
                                         unsigned long *pcLenReturn, int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    size_t cwSrcLen, cwDestLen;
    int rc;

    LogFlowFunc (("called\n"));
    rc = vboxClipboardReadDataFromClient(&g_ctx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    if ((rc != VINF_SUCCESS) || (g_ctx.pClient->data.cb == 0))
    {
        LogRel (("vboxClipboardConvertUtf16: vboxClipboardReadDataFromClient returned %Vrc, %d bytes of data\n", rc, g_ctx.pClient->data.cb));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(g_ctx.pClient->data.pv);
    cwSrcLen = g_ctx.pClient->data.cb / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf16: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Vrc.  Abandoning.\n", rc));
        vboxClipboardEmptyGuestBuffer();
        AssertRCReturn(rc, false);
    }
    if (cwDestLen == 0)
    {
        LogFlowFunc(("received empty clipboard data from the guest, returning false.\n"));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(XtMalloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        LogRel (("vboxClipboardConvertUtf16: failed to allocate %d bytes\n", cwDestLen * 2));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf16: clipboard conversion failed.  vboxClipboardUtf16WinToLin returned %Vrc.  Abandoning.\n", rc));
        XtFree(reinterpret_cast<char *>(pu16DestText));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    LogFlowFunc (("converted string is %.*ls. Returning.\n", cwDestLen, pu16DestText));
    vboxClipboardEmptyGuestBuffer();
    *atomTypeReturn = g_ctx.atomUtf16;
    *pValReturn = reinterpret_cast<XtPointer>(pu16DestText);
    *pcLenReturn = cwDestLen;
    *piFormatReturn = 16;
    return true;
}

/**
 * Satisfy a request from the host to convert the clipboard text to Utf8.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param atomTypeReturn The type of the data we are returning
 * @param pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @param pcLenReturn    The length of the data we are returning
 * @param piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertUtf8(Atom *atomTypeReturn, XtPointer *pValReturn,
                                        unsigned long *pcLenReturn, int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    char *pu8DestText;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = vboxClipboardReadDataFromClient(&g_ctx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    if ((rc != VINF_SUCCESS) || (g_ctx.pClient->data.cb == 0))
    {
        LogRel (("vboxClipboardConvertUtf8: vboxClipboardReadDataFromClient returned %Vrc, %d bytes of data\n", rc, g_ctx.pClient->data.cb));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(g_ctx.pClient->data.pv);
    cwSrcLen = g_ctx.pClient->data.cb / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf8: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Vrc.  Abandoning.\n", rc));
        vboxClipboardEmptyGuestBuffer();
        AssertRCReturn(rc, false);
    }
    if (cwDestLen == 0)
    {
        LogFlowFunc(("received empty clipboard data from the guest, returning false.\n"));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        LogRel (("vboxClipboardConvertUtf8: failed to allocate %d bytes\n", cwDestLen * 2));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf8: clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Vrc.  Abandoning.\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Allocate enough space, as RTUtf16ToUtf8Ex may fail if the
       space is too tightly calculated. */
    pu8DestText = XtMalloc(cwDestLen * 4);
    if (pu8DestText == 0)
    {
        LogRel (("vboxClipboardConvertUtf8: failed to allocate %d bytes\n", cwDestLen * 4));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the Utf16 string to Utf8. */
    rc = RTUtf16ToUtf8Ex(pu16DestText + 1, cwDestLen - 1, &pu8DestText, cwDestLen * 4,
                         &cbDestLen);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf8: clipboard conversion failed.  RTUtf16ToUtf8Ex() returned %Vrc.  Abandoning.\n", rc));
        XtFree(pu8DestText);
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    LogFlowFunc (("converted string is %.*s. Returning.\n", cbDestLen, pu8DestText));
    vboxClipboardEmptyGuestBuffer();
    *atomTypeReturn = g_ctx.atomUtf8;
    *pValReturn = reinterpret_cast<XtPointer>(pu8DestText);
    *pcLenReturn = cbDestLen;
    *piFormatReturn = 8;
    return true;
}

/**
 * Satisfy a request from the host to convert the clipboard text to COMPOUND_TEXT.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param atomTypeReturn The type of the data we are returning
 * @param pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @param pcLenReturn    The length of the data we are returning
 * @param piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertCText(Atom *atomTypeReturn, XtPointer *pValReturn,
                                         unsigned long *pcLenReturn, int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    char *pu8DestText = 0;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    XTextProperty property;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = vboxClipboardReadDataFromClient(&g_ctx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    if ((rc != VINF_SUCCESS) || (g_ctx.pClient->data.cb == 0))
    {
        LogRel (("vboxClipboardConvertCText: vboxClipboardReadDataFromClient returned %Vrc, %d bytes of data\n", rc, g_ctx.pClient->data.cb));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(g_ctx.pClient->data.pv);
    cwSrcLen = g_ctx.pClient->data.cb / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertCText: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Vrc.  Abandoning.\n", rc));
        vboxClipboardEmptyGuestBuffer();
        AssertRCReturn(rc, false);
    }
    if (cwDestLen == 0)
    {
        LogFlowFunc(("received empty clipboard data from the guest, returning false.\n"));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        LogRel (("vboxClipboardConvertCText: failed to allocate %d bytes\n", cwDestLen * 2));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertCText: clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Vrc.  Abandoning.\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the Utf16 string to Utf8. */
    rc = RTUtf16ToUtf8Ex(pu16DestText + 1, cwDestLen - 1, &pu8DestText, 0, &cbDestLen);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertCText: clipboard conversion failed.  RTUtf16ToUtf8Ex() returned %Vrc.  Abandoning.\n", rc));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* And finally (!) convert the Utf8 text to compound text. */
#ifdef RT_OS_SOLARIS
    rc = XmbTextListToTextProperty(XtDisplay(g_ctx.widget), &pu8DestText, 1,
                                     XCompoundTextStyle, &property);
#else
    rc = Xutf8TextListToTextProperty(XtDisplay(g_ctx.widget), &pu8DestText, 1,
                                     XCompoundTextStyle, &property);
#endif
    RTMemFree(pu8DestText);
    if (rc < 0)
    {
        const char *pcReason;
        switch(rc)
        {
        case XNoMemory:
            pcReason = "out of memory";
            break;
        case XLocaleNotSupported:
            pcReason = "locale (Utf8) not supported";
            break;
        case XConverterNotFound:
            pcReason = "converter not found";
            break;
        default:
            pcReason = "unknown error";
        }
        LogRel(("vboxClipboardConvertCText: Xutf8TextListToTextProperty failed.  Reason: %s\n",
                pcReason));
        XFree(property.value);
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    LogFlowFunc (("converted string is %s. Returning.\n", property.value));
    vboxClipboardEmptyGuestBuffer();
    *atomTypeReturn = property.encoding;
    *pValReturn = reinterpret_cast<XtPointer>(property.value);
    *pcLenReturn = property.nitems;
    *piFormatReturn = property.format;
    return true;
}

/**
 * Callback to convert the guests clipboard data for an application on the host.  Called by the
 * X Toolkit.
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param atomSelection  The selection which is being requested.  We only handle the clipboard.
 * @param atomTarget     The format we should convert the data to
 * @param atomTypeReturn The type of the data we are returning
 * @param pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @param pcLenReturn    The length of the data we are returning
 * @param piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertProc(Widget, Atom *atomSelection, Atom *atomTarget,
                                        Atom *atomTypeReturn, XtPointer *pValReturn,
                                        unsigned long *pcLenReturn, int *piFormatReturn)
{
    g_eClipboardFormats eFormat = INVALID;

    LogFlowFunc(("\n"));
    if (*atomSelection != g_ctx.atomClipboard)
    {
        LogFlowFunc(("rc = false\n"));
        return false;
    }
#ifdef DEBUG
    char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), *atomTarget);
    if (szAtomName != 0)
    {
        Log2 (("vboxClipboardConvertProc: request for format %s\n", szAtomName));
        XFree(szAtomName);
    }
    else
    {
        Log(("vboxClipboardConvertProc: request for invalid target atom %d!\n", *atomTarget));
    }
#endif
    if (*atomTarget == g_ctx.atomTargets)
    {
        eFormat = TARGETS;
    }
    else
    {
        for (unsigned i = 0; i != g_ctx.formatList.size(); ++i)
        {
            if (g_ctx.formatList[i].atom == *atomTarget)
            {
                eFormat = g_ctx.formatList[i].format;
                break;
            }
        }
    }
    switch (eFormat)
    {
    case TARGETS:
        return vboxClipboardConvertTargets(atomTypeReturn, pValReturn, pcLenReturn,
                                           piFormatReturn);
    case UTF16:
        return vboxClipboardConvertUtf16(atomTypeReturn, pValReturn, pcLenReturn,
                                         piFormatReturn);
    case UTF8:
        return vboxClipboardConvertUtf8(atomTypeReturn, pValReturn, pcLenReturn,
                                        piFormatReturn);
    case CTEXT:
        return vboxClipboardConvertCText(atomTypeReturn, pValReturn, pcLenReturn,
                                         piFormatReturn);
    default:
        Log(("vboxClipboardConvertProc: bad format\n"));
        return false;
    }
}

static void vboxClipboardLoseProc(Widget, Atom *)
{
    LogFlow (("vboxClipboardLoseProc: called, giving the host clipboard ownership\n"));
    g_ctx.eOwner = HOST;
    g_ctx.notifyGuest = true;
}

/**
 * The guest is taking possession of the shared clipboard.  Called by the HGCM clipboard
 * subsystem.
 *
 * @param pClient    Context data for the guest system
 * @param u32Formats Clipboard formats the the guest is offering
 */
void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Formats)
{
    pClient->pCtx->guestFormats = u32Formats;
    LogFlowFunc (("u32Formats=%d\n", u32Formats));
    if (u32Formats == 0)
    {
        /* This is just an automatism, not a genuine anouncement */
        LogFlowFunc(("returning\n"));
        return;
    }
    if (g_ctx.eOwner == GUEST)
    {
        /* We already own the clipboard, so no need to grab it, especially as that can lead
           to races due to the asynchronous nature of the X11 clipboard.  This event may also
           have been sent out by the guest to invalidate the Windows clipboard cache. */
        LogFlowFunc(("returning\n"));
        return;
    }
    Log2 (("vboxClipboardFormatAnnounce: giving the guest clipboard ownership\n"));
    g_ctx.eOwner = GUEST;
    g_ctx.hostTextFormat = INVALID;
    g_ctx.hostBitmapFormat = INVALID;
    if (XtOwnSelection(g_ctx.widget, g_ctx.atomClipboard, CurrentTime, vboxClipboardConvertProc,
                       vboxClipboardLoseProc, 0) != True)
    {
        Log2 (("vboxClipboardFormatAnnounce: returning clipboard ownership to the host\n"));
        /* We set this so that the guest gets notified when we take the clipboard, even if no
          guest formats are found which we understand. */
        g_ctx.notifyGuest = true;
        g_ctx.eOwner = HOST;
    }
    LogFlowFunc(("returning\n"));

}

/**
 * Called by the HGCM clipboard subsystem when the guest wants to read the host clipboard.
 *
 * @param pClient   Context information about the guest VM
 * @param u32Format The format that the guest would like to receive the data in
 * @param pv        Where to write the data to
 * @param cb        The size of the buffer to write the data to
 * @param pcbActual Where to write the actual size of the written data
 */
int vboxClipboardReadData (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Format, void *pv,
                           uint32_t cb, uint32_t *pcbActual)
{
    LogFlow(("vboxClipboardReadData: u32Format = %d, cb = %d\n", u32Format, cb));

    /*
     * The guest wants to read data in the given format.
     */
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        if (g_ctx.hostTextFormat == INVALID)
        {
            /* No data available. */
            *pcbActual = 0;
            return VINF_SUCCESS;
        }
        /* Only one of the host and the guest should ever be waiting. */
        if (RT_FAILURE(ASMAtomicCmpXchgU32(&g_ctx.waiter, 1, 0)))
        {
            LogRel(("vboxClipboardReadData: detected a deadlock situation - the host and the guest are waiting for each other.\n"));
            return VERR_DEADLOCK;
        }
        g_ctx.requestHostFormat = g_ctx.hostTextFormat;
        g_ctx.requestBuffer = pv;
        g_ctx.requestBufferSize = cb;
        g_ctx.requestActualSize = pcbActual;
        /* Send out a request for the data to the current clipboard owner */
        XtGetSelectionValue(g_ctx.widget, g_ctx.atomClipboard, g_ctx.atomHostTextFormat,
                            vboxClipboardGetProc, reinterpret_cast<XtPointer>(g_ctx.pClient),
                            CurrentTime);
        /* When the data arrives, the vboxClipboardGetProc callback will be called.  The
           callback will signal the event semaphore when it has processed the data for us. */
        if (RTSemEventWait(g_ctx.waitForData, CLIPBOARDTIMEOUT) != VINF_SUCCESS)
        {
            LogRel (("vboxClipboardReadDataFromClient: XtGetSelectionValue failed to complete within %d milliseconds\n", CLIPBOARDTIMEOUT));
            g_ctx.hostTextFormat = INVALID;
            g_ctx.hostBitmapFormat = INVALID;
            g_ctx.waiter = 0;
            return VERR_TIMEOUT;
        }
        g_ctx.waiter = 0;
    }
    else
    {
        return VERR_NOT_IMPLEMENTED;
    }
    return VINF_SUCCESS;
}

/**
 * Called by the HGCM clipboard subsystem when we have requested data and that data arrives.
 *
 * @param pClient   Context information about the guest VM
 * @param pv        Buffer to which the data was written
 * @param cb        The size of the data written
 * @param u32Format The format of the data written
 */
void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv, uint32_t cb, uint32_t u32Format)
{
    LogFlow(("vboxClipboardWriteData\n"));

    /*
     * The guest returns data that was requested in the WM_RENDERFORMAT handler.
     */
    if (!(   pClient->data.pv == NULL
          && pClient->data.cb == 0
          && pClient->data.u32Format == 0))
    {
        LogRel(("vboxClipboardWriteData: clipboard data has arrived from the guest, but another transfer is in process or has not cleaned up properly.\n"));
        AssertMsgFailed(("vboxClipboardWriteData: clipboard data has arrived from the guest, but another transfer is in process or has not cleaned up properly.\n"));
    }

    if (cb > 0)
    {
        pClient->data.pv = RTMemAlloc (cb);

        if (pClient->data.pv)
        {
            memcpy (pClient->data.pv, pv, cb);
            pClient->data.cb = cb;
            pClient->data.u32Format = u32Format;
        }
    }

    RTSemEventSignal(g_ctx.waitForData);
}
