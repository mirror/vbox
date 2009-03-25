/** @file
 *
 * Shared Clipboard:
 * Linux host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <vector>

#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/alloc.h>
#include <iprt/asm.h>        /* For atomic operations */
#include <iprt/assert.h>
#include <iprt/env.h>
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
#include <X11/StringDefs.h>

#ifdef RT_OS_SOLARIS
#include <tsol/label.h>
#endif

/** Do we want to test Utf16 by disabling other text formats? */
static bool g_testUtf16 = false;
/** Do we want to test Utf8 by disabling other text formats? */
static bool g_testUtf8 = false;
/** Do we want to test compount text by disabling other text formats? */
static bool g_testCText = false;
/** Are we currently debugging the clipboard code? */
static bool g_debugClipboard = false;

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

/** Does X11 or VBox currently own the clipboard? */
enum g_eOwner { NONE = 0, X11, VB };

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
    /** X11 atom refering to the selection: PRIMARY */
    Atom atomPrimary;
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
    volatile enum g_eOwner eOwner;

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
    /** Who (if anyone) is currently waiting for data?  Used for sanity checks
     *  when data arrives. */
    volatile uint32_t waiter;
    /** This mutex is grabbed during any critical operations on the clipboard
     * which might clash with others. */
    RTSEMMUTEX clipboardMutex;

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

/* Are we actually connected to the X11 servicer? */
static bool g_fHaveX11;

/**
 * Reset the contents of the buffer used to pass clipboard data from VBox to X11.
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
 * Send a request to VBox to transfer the contents of its clipboard to X11.
 *
 * @param  pCtx      Pointer to the host clipboard structure
 * @param  u32Format The format in which the data should be transfered
 * @thread clipboard X11 event thread
 * @note   called by vboxClipboardConvert*
 */
static int vboxClipboardReadDataFromVBox (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format)
{
    volatile VBOXCLIPBOARDCLIENTDATA *pClient = pCtx->pClient;

    LogFlowFunc(("u32Format=%02X\n", u32Format));
    if (pClient == NULL)
    {
        /* This can legitimately happen if we disconnect during a request for
         * data from X11. */
        LogFunc(("host requested guest clipboard data after guest had disconnected.\n"));
        pCtx->guestFormats = 0;
        pCtx->waiter = NONE;
        return VERR_TIMEOUT;
    }
    /* Assert that no other transfer is in process (requests are serialised)
     * and that the last transfer cleaned up properly. */
    AssertLogRelReturn(   pClient->data.pv == NULL
                       && pClient->data.cb == 0
                       && pClient->data.u32Format == 0,
                       VERR_WRONG_ORDER
                      );
    /* No one else (X11 or VBox) should currently be waiting.  The first because
     * requests from X11 are serialised and the second because VBox previously
     * grabbed the clipboard, so it should not be waiting for data from us. */
    AssertLogRelReturn (ASMAtomicCmpXchgU32(&pCtx->waiter, X11, NONE), VERR_DEADLOCK);
    /* Request data from VBox */
    vboxSvcClipboardReportMsg(pCtx->pClient,
                              VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA,
                              u32Format);
    /* Which will signal us when it is ready.  We use a timeout here because
     * we can't be sure that the guest will behave correctly. */
    int rc = RTSemEventWait(pCtx->waitForData, CLIPBOARDTIMEOUT);
    if (rc == VERR_TIMEOUT)
        rc = VINF_SUCCESS;  /* Timeout handling follows. */
    /* Now we have a potential race between the HGCM thread delivering the data
     * and our setting waiter to NONE to say that we are no longer waiting for
     * it.  We solve this as follows: both of these operations are done under
     * the clipboard mutex.  The HGCM thread will only deliver the data if we
     * are still waiting after it acquires the mutex.  After we release the
     * mutex, we finally do our check to see whether the data was delivered. */
    RTSemMutexRequest(g_ctx.clipboardMutex, RT_INDEFINITE_WAIT);
    pCtx->waiter = NONE;
    RTSemMutexRelease(g_ctx.clipboardMutex);
    AssertLogRelRCSuccess(rc);
    if (RT_FAILURE(rc))
    {
        /* I believe this should not happen.  Wait until the assertions arrive
         * to prove the contrary. */
        vboxClipboardEmptyGuestBuffer();
        pCtx->guestFormats = 0;
        return rc;
    }
    if (pClient->data.pv == NULL)
        return VERR_TIMEOUT;
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

    LogFlowFunc (("converting Utf-16 to Utf-16LE.  cwSrcLen=%d, cb=%d, pu16SrcText+1=%.*ls\n",
                   cwSrcLen, cb, cwSrcLen - 1, pu16SrcText + 1));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    /* How long will the converted text be? */
    int rc = vboxClipboardUtf16GetWinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_SUCCESS(rc) && (cb < cwDestLen * 2))
    {
        /* Not enough buffer space provided - report the amount needed. */
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        rc = VERR_BUFFER_OVERFLOW;
    }
    /* Convert the text. */
    if (RT_SUCCESS(rc))
        rc = vboxClipboardUtf16LinToWin(pu16SrcText, cwSrcLen, pu16DestText, cb / 2);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc (("converted string is %.*ls\n", cwDestLen, pu16DestText));
        *pcbActual = cwDestLen * 2;
    }
    /* We need to do this whether we succeed or fail. */
    XtFree(reinterpret_cast<char *>(pValue));
    RTSemEventSignal(g_ctx.waitForData);
    LogFlowFunc(("Returning.  Status is %Rrc\n", rc));
}

/**
 * Convert the UTF-8 text returned from the X11 clipboard to UTF-16LE with Windows EOLS.
 * We are reading the X11 clipboard to make it available to VBox.
 *
 * @param pValue      Source UTF-8 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by pv
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @thread clipboard X11 event thread
 * @note   called by vboxClipboardGetDataFromX11
 */
static void vboxClipboardGetUtf8FromX11(XtPointer pValue, unsigned cbSrcLen,
                                        void *pv, unsigned cb,
                                        uint32_t *pcbActual)
{
    size_t cwSrcLen, cwDestLen;
    char *pu8SrcText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16SrcText = NULL;
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);

    LogFlowFunc (("converting Utf-8 to Utf-16LE.  cbSrcLen=%d, cb=%d, pu8SrcText=%.*s\n",
                   cbSrcLen, cb, cbSrcLen, pu8SrcText));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    /* First convert the UTF8 to UTF16 */
    int rc = RTStrToUtf16Ex(pu8SrcText, cbSrcLen, &pu16SrcText, 0, &cwSrcLen);
    /* Check how much longer will the converted text will be. */
    if (RT_SUCCESS(rc))
        rc = vboxClipboardUtf16GetWinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_SUCCESS(rc) && (cb < cwDestLen * 2))
    {
        /* Not enough buffer space provided - report the amount needed. */
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        rc = VERR_BUFFER_OVERFLOW;
    }
    /* Convert the text. */
    if (RT_SUCCESS(rc))
        rc = vboxClipboardUtf16LinToWin(pu16SrcText, cwSrcLen, pu16DestText, cb / 2);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc (("converted string is %.*ls.\n", cwDestLen, pu16DestText));
        *pcbActual = cwDestLen * 2;
    }
    XtFree(reinterpret_cast<char *>(pValue));
    RTUtf16Free(pu16SrcText);
    RTSemEventSignal(g_ctx.waitForData);
    LogFlowFunc(("Returning.  Status is %Rrc", rc));
}

/**
 * Convert the COMPOUND_TEXT text returned from the X11 clipboard to UTF-16LE with Windows
 * EOLS.  We are reading the X11 clipboard to make it available to VBox.
 *
 * @param pValue      Source COMPOUND_TEXT text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by pv
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @thread clipboard X11 event thread
 * @note   called by vboxClipboardGetDataFromX11
 */
static void vboxClipboardGetCTextFromX11(XtPointer pValue, unsigned cbSrcLen,
                                         void *pv, unsigned cb,
                                         uint32_t *pcbActual)
{
    size_t cwSrcLen, cwDestLen;
    char **ppu8SrcText = NULL;
    PRTUTF16 pu16SrcText = NULL;
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    XTextProperty property;
    int rc = VINF_SUCCESS;
    int cProps;

    LogFlowFunc (("converting COMPOUND TEXT to Utf-16LE.  cbSrcLen=%d, cb=%d, pu8SrcText=%.*s\n",
                   cbSrcLen, cb, cbSrcLen, reinterpret_cast<char *>(pValue)));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    /* First convert the compound text to Utf8 */
    property.value = reinterpret_cast<unsigned char *>(pValue);
    property.encoding = g_ctx.atomCText;
    property.format = 8;
    property.nitems = cbSrcLen;
#ifdef RT_OS_SOLARIS
    int xrc = XmbTextPropertyToTextList(XtDisplay(g_ctx.widget), &property, &ppu8SrcText, &cProps);
#else
    int xrc = Xutf8TextPropertyToTextList(XtDisplay(g_ctx.widget), &property, &ppu8SrcText, &cProps);
#endif
    XtFree(reinterpret_cast<char *>(pValue));
    if (xrc < 0)
        switch(xrc)
        {
        case XNoMemory:
            rc = VERR_NO_MEMORY;
            break;
        case XLocaleNotSupported:
        case XConverterNotFound:
            rc = VERR_NOT_SUPPORTED;
            break;
        default:
            rc = VERR_UNRESOLVED_ERROR;
        }
    /* Now convert the UTF8 to UTF16 */
    if (RT_SUCCESS(rc))
        rc = RTStrToUtf16Ex(*ppu8SrcText, cbSrcLen, &pu16SrcText, 0, &cwSrcLen);
    /* Check how much longer will the converted text will be. */
    if (RT_SUCCESS(rc))
        rc = vboxClipboardUtf16GetWinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_SUCCESS(rc) && (cb < cwDestLen * 2))
    {
        /* Not enough buffer space provided - report the amount needed. */
        LogFlowFunc (("guest buffer too small: size %d bytes, needed %d.  Returning.\n",
                       cb, cwDestLen * 2));
        *pcbActual = cwDestLen * 2;
        rc = VERR_BUFFER_OVERFLOW;
    }
    /* Convert the text. */
    if (RT_SUCCESS(rc))
        rc = vboxClipboardUtf16LinToWin(pu16SrcText, cwSrcLen, pu16DestText, cb / 2);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc (("converted string is %.*ls\n", cwDestLen, pu16DestText));
        *pcbActual = cwDestLen * 2;
    }
    if (ppu8SrcText != NULL)
        XFreeStringList(ppu8SrcText);
    RTUtf16Free(pu16SrcText);
    LogFlowFunc(("Returning.  Status is %Rrc\n", rc));
    RTSemEventSignal(g_ctx.waitForData);
}

/**
 * Convert the Latin1 text returned from the X11 clipboard to UTF-16LE with Windows EOLS
 * and place it in the global g_pcClipboardText variable.  We are reading the X11 clipboard to
 * make it available to VBox.
 *
 * @param pValue      Source Latin1 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by cb
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @thread clipboard X11 event thread
 * @note   called by vboxClipboardGetDataFromX11
 */
static void vboxClipboardGetLatin1FromX11(XtPointer pValue, unsigned cbSourceLen, void *pv, unsigned cb,
                                   uint32_t *pcbActual)
{
    unsigned cwDestLen = cbSourceLen + 1;
    char *pu8SourceText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    int rc = VINF_SUCCESS;

    LogFlowFunc (("converting Latin1 to Utf-16LE.  Original is %.*s\n",
                  cbSourceLen, pu8SourceText));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    for (unsigned i = 0; i < cbSourceLen; i++)
        if (pu8SourceText[i] == LINEFEED)
            ++cwDestLen;
    if (cb < cwDestLen * 2)
    {
        /* Not enough buffer space provided - report the amount needed. */
        LogFlowFunc (("guest buffer too small: size %d bytes\n", cb));
        *pcbActual = cwDestLen * 2;
        rc = VERR_BUFFER_OVERFLOW;
    }
    if (RT_SUCCESS(rc))
    {
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
        LogFlowFunc (("converted text is %.*ls\n", cwDestLen, pu16DestText));
    }
    XtFree(reinterpret_cast<char *>(pValue));
    RTSemEventSignal(g_ctx.waitForData);
    LogFlowFunc(("Returning.  Status is %Rrc\n", rc));
}

/**
 * Convert the clipboard text from the current format to Utf-16 with Windows line breaks.
 * We are reading the X11 clipboard to make it available to VBox.
 * @thread  clipboard X11 event thread
 * @note    Callback for XtGetSelectionValue, called from vboxClipboardReadData
 */
static void vboxClipboardGetDataFromX11(Widget, XtPointer pClientData,
                                        Atom * /* selection */, Atom *atomType,
                                        XtPointer pValue,
                                        long unsigned int *pcLen,
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
    RTSemMutexRequest(g_ctx.clipboardMutex, RT_INDEFINITE_WAIT);
    if (reinterpret_cast<VBOXCLIPBOARDCLIENTDATA *>(pClientData) != g_ctx.pClient)
    {
        /* If the client is no longer connected, just return. */
        XtFree(reinterpret_cast<char *>(pValue));
        LogFlowFunc(("client is no longer connected, returning\n"));
        RTSemMutexRelease(g_ctx.clipboardMutex);
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
        vboxClipboardGetCTextFromX11(pValue, cTextLen, g_ctx.requestBuffer, g_ctx.requestBufferSize,
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
            vboxClipboardGetUtf8FromX11(pValue, cTextLen, g_ctx.requestBuffer, g_ctx.requestBufferSize,
                                 g_ctx.requestActualSize);
            break;
        }
        else
        {
            vboxClipboardGetLatin1FromX11(pValue, cTextLen, g_ctx.requestBuffer, g_ctx.requestBufferSize,
                                   g_ctx.requestActualSize);
            break;
        }
    }
    default:
        LogFunc (("bad target format\n"));
        XtFree(reinterpret_cast<char *>(pValue));
        RTSemMutexRelease(g_ctx.clipboardMutex);
        return;
    }
    g_ctx.notifyGuest = true;
    RTSemMutexRelease(g_ctx.clipboardMutex);
}

/**
 * Find out what targets the current X11 clipboard holder can handle.  We are
 * reading the X11 clipboard to make it available to VBox.
 * @thread  clipboard X11 event thread
 * @note    Callback for XtGetSelectionValue, called from vboxClipboardPollX11ForTargets
 */
static void vboxClipboardGetTargetsFromX11(Widget, XtPointer pClientData,
                                           Atom * /* selection */,
                                           Atom *atomType,
                                           XtPointer pValue,
                                           long unsigned int *pcLen,
                                           int *piFormat)
{
    Atom *atomTargets = reinterpret_cast<Atom *>(pValue);
    unsigned cAtoms = *pcLen;
    g_eClipboardFormats eBestTarget = INVALID;
    Atom atomBestTarget = None;

    Log3 (("%s: called\n", __PRETTY_FUNCTION__));
    if (*atomType == XT_CONVERT_FAIL)
    {
        LogFunc (("reading clipboard from host, X toolkit failed to convert the selection\n"));
        return;
    }
    /* We grab this mutex whenever an asynchronous clipboard operation completes and while
       disconnecting a client from the clipboard to stop these operations colliding. */
    RTSemMutexRequest(g_ctx.clipboardMutex, RT_INDEFINITE_WAIT);
    if (reinterpret_cast<VBOXCLIPBOARDCLIENTDATA *>(pClientData) != g_ctx.pClient)
    {
        /* If the client is no longer connected, just return. */
        LogFlowFunc(("client is no longer connected, returning\n"));
        RTSemMutexRelease(g_ctx.clipboardMutex);
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
        if (g_debugClipboard)
        {
            char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
            if (szAtomName != 0)
            {
                Log2 (("%s: the host offers target %s\n", __PRETTY_FUNCTION__,
                       szAtomName));
                XFree(szAtomName);
            }
        }
    }
    g_ctx.atomHostTextFormat = atomBestTarget;
    if ((eBestTarget != g_ctx.hostTextFormat) || (g_ctx.notifyGuest == true))
    {
        uint32_t u32Formats = 0;
        if (g_debugClipboard)
        {
            if (atomBestTarget != None)
            {
                char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomBestTarget);
                Log2 (("%s: switching to host text target %s.  Available targets are:\n",
                       __PRETTY_FUNCTION__, szAtomName));
                XFree(szAtomName);
            }
            else
                Log2(("%s: no supported host text target found.  Available targets are:\n",
                      __PRETTY_FUNCTION__));
            for (unsigned i = 0; i < cAtoms; ++i)
            {
                char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
                if (szAtomName != 0)
                {
                    Log2 (("%s:     %s\n", __PRETTY_FUNCTION__, szAtomName));
                    XFree(szAtomName);
                }
            }
        }
        g_ctx.hostTextFormat = eBestTarget;
        if (eBestTarget != INVALID)
            u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        vboxSvcClipboardReportMsg (g_ctx.pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS,
                                   u32Formats);
        g_ctx.notifyGuest = false;
    }
    XtFree(reinterpret_cast<char *>(pValue));
    RTSemMutexRelease(g_ctx.clipboardMutex);
}

/**
 * This callback is called every 200ms to check the contents of the X11 clipboard.
 * @thread  clipboard X11 event thread
 * @note    Callback for XtAppAddTimeOut, called from vboxClipboardThread and
 *          recursively retriggered
 */
static void vboxClipboardPollX11ForTargets(XtPointer /* pUserData */, XtIntervalId * /* hTimerId */)
{
    Log3 (("%s: called\n", __PRETTY_FUNCTION__));
    /* Get the current clipboard contents */
    if (g_ctx.eOwner == X11 && g_ctx.pClient != 0)
    {
        Log3 (("%s: requesting the targets that the host clipboard offers\n",
               __PRETTY_FUNCTION__));
        XtGetSelectionValue(g_ctx.widget, g_ctx.atomClipboard, g_ctx.atomTargets,
                            vboxClipboardGetTargetsFromX11, reinterpret_cast<XtPointer>(g_ctx.pClient),
                            CurrentTime);
    }
    /* Re-arm our timer */
    XtAppAddTimeOut(g_ctx.appContext, 200 /* ms */, vboxClipboardPollX11ForTargets, 0);
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
 * @thread  clipboard X11 event thread
 */
static int vboxClipboardThread(RTTHREAD self, void * /* pvUser */)
{
    LogRel(("Shared clipboard: starting host clipboard thread\n"));

    /* Set up a timer to poll the host clipboard */
    XtAppAddTimeOut(g_ctx.appContext, 200 /* ms */, vboxClipboardPollX11ForTargets, 0);

    XtAppMainLoop(g_ctx.appContext);
    g_ctx.formatList.clear();
    LogRel(("Shared clipboard: host clipboard thread terminated successfully\n"));
    return VINF_SUCCESS;
}

/** X11 specific initialisation for the shared clipboard. */
int vboxClipboardInitX11 (void)
{
    /* Create a window and make it a clipboard viewer. */
    int cArgc = 0;
    char *pcArgv = 0;
    int rc = VINF_SUCCESS;
    // static String szFallbackResources[] = { (char*)"*.width: 1", (char*)"*.height: 1", NULL };
    Display *pDisplay;

    /* Make sure we are thread safe */
    XtToolkitThreadInitialize();
    /* Set up the Clipbard application context and main window.  We call all these functions
       directly instead of calling XtOpenApplication() so that we can fail gracefully if we
       can't get an X11 display. */
    XtToolkitInitialize();
    g_ctx.appContext = XtCreateApplicationContext();
    // XtAppSetFallbackResources(g_ctx.appContext, szFallbackResources);
    pDisplay = XtOpenDisplay(g_ctx.appContext, 0, 0, "VBoxClipboard", 0, 0, &cArgc, &pcArgv);
    if (NULL == pDisplay)
    {
        LogRel(("Shared clipboard: failed to connect to the host clipboard - the window system may not be running.\n"));
        rc = VERR_NOT_SUPPORTED;
    }
    if (RT_SUCCESS(rc))
    {
        g_ctx.widget = XtVaAppCreateShell(0, "VBoxClipboard", applicationShellWidgetClass, pDisplay,
                                          XtNwidth, 1, XtNheight, 1, NULL);
        if (NULL == g_ctx.widget)
        {
            LogRel(("Shared clipboard: failed to construct the X11 window for the host clipboard manager.\n"));
            rc = VERR_NO_MEMORY;
        }
    }
    if (RT_SUCCESS(rc))
    {
        XtSetMappedWhenManaged(g_ctx.widget, false);
        XtRealizeWidget(g_ctx.widget);

        /* Get hold of the atoms which we need */
        g_ctx.atomClipboard = XInternAtom(XtDisplay(g_ctx.widget), "CLIPBOARD", false /* only_if_exists */);
        g_ctx.atomPrimary   = XInternAtom(XtDisplay(g_ctx.widget), "PRIMARY",   false);
        g_ctx.atomTargets   = XInternAtom(XtDisplay(g_ctx.widget), "TARGETS",   false);
        g_ctx.atomMultiple  = XInternAtom(XtDisplay(g_ctx.widget), "MULTIPLE",  false);
        g_ctx.atomTimestamp = XInternAtom(XtDisplay(g_ctx.widget), "TIMESTAMP", false);
        g_ctx.atomUtf16     = XInternAtom(XtDisplay(g_ctx.widget),
                                          "text/plain;charset=ISO-10646-UCS-2", false);
        g_ctx.atomUtf8      = XInternAtom(XtDisplay(g_ctx.widget), "UTF_STRING", false);
        /* And build up the vector of supported formats */
        g_ctx.atomCText     = XInternAtom(XtDisplay(g_ctx.widget), "COMPOUND_TEXT", false);
        /* And build up the vector of supported formats */
        if (!g_testUtf8 && !g_testCText)
            vboxClipboardAddFormat("text/plain;charset=ISO-10646-UCS-2", UTF16,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
        if (!g_testUtf16 && !g_testCText)
        {
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
}
        if (!g_testUtf16 && !g_testUtf8)
            vboxClipboardAddFormat("COMPOUND_TEXT", CTEXT,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    }
    return rc;
}

/**
 * Initialise the host side of the shared clipboard.
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
int vboxClipboardInit (void)
{
    int rc;

    if (!RTEnvGet("DISPLAY"))
    {
        /*
         * If we don't find the DISPLAY environment variable we assume that we are not
         * connected to an X11 server. Don't actually try to do this then, just fail
         * silently and report success on every call. This is important for VBoxHeadless.
         */
        LogRelFunc(("no X11 detected -- host clipboard disabled\n"));
        g_fHaveX11 = false;
        return VINF_SUCCESS;
    }

    if (RTEnvGet("VBOX_CBTEST_UTF16"))
    {
        g_testUtf16 = true;
        LogRel(("Host clipboard: testing Utf16\n"));
    }
    else if (RTEnvGet("VBOX_CBTEST_UTF8"))
    {
        g_testUtf8 = true;
        LogRel(("Host clipboard: testing Utf8\n"));
    }
    else if (RTEnvGet("VBOX_CBTEST_CTEXT"))
    {
        g_testCText = true;
        LogRel(("Host clipboard: testing compound text\n"));
    }
    else if (RTEnvGet("VBOX_CBDEBUG"))
    {
        g_debugClipboard = true;
        LogRel(("Host clipboard: enabling additional debugging output\n"));
    }

    g_fHaveX11 = true;

    LogRel(("Initializing host clipboard service\n"));
    RTSemEventCreate(&g_ctx.waitForData);
    RTSemMutexCreate(&g_ctx.clipboardMutex);
    rc = vboxClipboardInitX11();
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadCreate(&g_ctx.thread, vboxClipboardThread, 0, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
        if (RT_FAILURE(rc))
            LogRel(("Failed to start the host shared clipboard thread.\n"));
    }
    if (RT_FAILURE(rc))
    {
        RTSemEventDestroy(g_ctx.waitForData);
        RTSemMutexDestroy(g_ctx.clipboardMutex);
    }
    return rc;
}

/**
 * Terminate the host side of the shared clipboard.
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
void vboxClipboardDestroy (void)
{
    int rc, rcThread;
    unsigned count = 0;
    XEvent ev;

    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;

    LogRel(("vboxClipboardDestroy: shutting down host clipboard\n"));

    /* Drop the reference to the client, in case it is still there.  This will
     * cause any outstanding clipboard data requests from X11 to fail
     * immediately. */
    g_ctx.pClient = NULL;
    if (g_ctx.eOwner == VB)
        /* X11 may be waiting for data from VBox.  At this point it is no
         * longer going to arrive, and we must release it to allow the event
         * loop to terminate.  In this case the buffer where VBox would have
         * written the clipboard data will still be empty and we will just
         * return "no data" to X11.  Any subsequent attempts to get the data
         * from VBox will fail immediately as the client reference is gone. */
        RTSemEventSignal(g_ctx.waitForData);
    /* Set the termination flag.  This has been observed to block if it was set
     * during a request for clipboard data coming from X11, so only we do it
     * after releasing any such requests. */
    XtAppSetExitFlag(g_ctx.appContext);
    /* Wake up the event loop */
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.format = 8;
    XSendEvent(XtDisplay(g_ctx.widget), XtWindow(g_ctx.widget), false, 0, &ev);
    XFlush(XtDisplay(g_ctx.widget));
    do
    {
        rc = RTThreadWait(g_ctx.thread, 1000, &rcThread);
        ++count;
        Assert(RT_SUCCESS(rc) || ((VERR_TIMEOUT == rc) && (count != 5)));
    } while ((VERR_TIMEOUT == rc) && (count < 300));
    if (RT_SUCCESS(rc))
    {
        /*
         * No one should be waiting on this by now.  Justification:
         *  - Case 1: VBox is waiting for data from X11:
         *      Not possible, as it would be waiting on this thread.
         *  - Case 2: X11 is waiting for data from VBox:
         *      Not possible, as we checked that the X11 event thread exited
         *      successfully.
         */
        RTSemEventDestroy(g_ctx.waitForData);
        RTSemMutexDestroy(g_ctx.clipboardMutex);
        AssertRC(rcThread);
    }
    else
        LogRel(("vboxClipboardDestroy: rc=%Rrc\n", rc));
    XtCloseDisplay(XtDisplay(g_ctx.widget));
    LogFlowFunc(("returning.\n"));
}

/**
 * Connect a guest the shared clipboard.
 *
 * @param   pClient Structure containing context information about the guest system
 * @returns RT status code
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
int vboxClipboardConnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    LogFlow(("vboxClipboardConnect\n"));

    /* Only one client is supported for now */
    AssertLogRelReturn(g_ctx.pClient == 0, VERR_NOT_SUPPORTED);

    pClient->pCtx = &g_ctx;
    pClient->pCtx->pClient = pClient;
    g_ctx.eOwner = X11;
    g_ctx.notifyGuest = true;
    return VINF_SUCCESS;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called
 * after a save and restore of the guest.
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    /* On a Linux host, the guest should never synchronise/cache its clipboard contents, as
       we have no way of reliably telling when the host clipboard data changes.  So instead
       of synchronising, we tell the guest to empty its clipboard, and we set the cached
       flag so that we report formats to the guest next time we poll for them. */
    vboxSvcClipboardReportMsg (g_ctx.pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, 0);
    g_ctx.notifyGuest = true;

    return VINF_SUCCESS;
}

/**
 * Shut down the shared clipboard service and "disconnect" the guest.
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;

    LogFlow(("vboxClipboardDisconnect\n"));

    RTSemMutexRequest(g_ctx.clipboardMutex, RT_INDEFINITE_WAIT);
    g_ctx.pClient = NULL;
    g_ctx.eOwner = NONE;
    g_ctx.hostTextFormat = INVALID;
    g_ctx.hostBitmapFormat = INVALID;
    RTSemMutexRelease(g_ctx.clipboardMutex);
}

/**
 * Satisfy a request from X11 for clipboard targets supported by VBox.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param  atomTypeReturn The type of the data we are returning
 * @param  pValReturn     A pointer to the data we are returning.  This should be to memory
 *                        allocated by XtMalloc, which will be freed by the toolkit later
 * @param  pcLenReturn    The length of the data we are returning
 * @param  piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 * @thread  clipboard X11 event thread
 * @note    called by vboxClipboardConvertForX11
 */
static Boolean vboxClipboardConvertTargetsForX11(Atom *atomTypeReturn, XtPointer *pValReturn,
                                                 unsigned long *pcLenReturn, int *piFormatReturn)
{
    unsigned uListSize = g_ctx.formatList.size();
    Atom *atomTargets = reinterpret_cast<Atom *>(XtMalloc((uListSize + 3) * sizeof(Atom)));
    unsigned cTargets = 0;

    LogFlowFunc (("called\n"));
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
    if (g_debugClipboard)
    {
        for (unsigned i = 0; i < cTargets + 3; i++)
        {
            char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
            if (szAtomName != 0)
            {
                Log2 (("%s: returning target %s\n", __PRETTY_FUNCTION__,
                       szAtomName));
                XFree(szAtomName);
            }
            else
            {
                Log(("%s: invalid atom %d in the list!\n", __PRETTY_FUNCTION__,
                     atomTargets[i]));
            }
        }
    }
    *atomTypeReturn = XA_ATOM;
    *pValReturn = reinterpret_cast<XtPointer>(atomTargets);
    *pcLenReturn = cTargets + 3;
    *piFormatReturn = 32;
    return true;
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
    rc = vboxClipboardReadDataFromVBox(&g_ctx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    if ((RT_FAILURE(rc)) || (g_ctx.pClient->data.cb == 0))
    {
        /* If vboxClipboardReadDataFromVBox fails then pClient may be invalid */
        LogRelFunc (("vboxClipboardReadDataFromVBox returned %Rrc%s\n", rc,
                    RT_SUCCESS(rc) ? ", g_ctx.pClient->data.cb == 0" :  ""));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(g_ctx.pClient->data.pv);
    cwSrcLen = g_ctx.pClient->data.cb / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf16: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
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
        LogRel(("vboxClipboardConvertUtf16: failed to allocate %d bytes\n", cwDestLen * 2));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf16: clipboard conversion failed.  vboxClipboardUtf16WinToLin returned %Rrc.  Abandoning.\n", rc));
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
 * @param  atomTypeReturn The type of the data we are returning
 * @param  pValReturn     A pointer to the data we are returning.  This should be to memory
 *                        allocated by XtMalloc, which will be freed by the toolkit later
 * @param  pcLenReturn    The length of the data we are returning
 * @param  piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 * @thread  clipboard X11 event thread
 * @note    called by vboxClipboardConvertForX11
 */
static Boolean vboxClipboardConvertToUtf8ForX11(Atom *atomTypeReturn,
                                                XtPointer *pValReturn,
                                                unsigned long *pcLenReturn,
                                                int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    char *pu8DestText;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = vboxClipboardReadDataFromVBox(&g_ctx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    if ((rc != VINF_SUCCESS) || (g_ctx.pClient->data.cb == 0))
    {
        /* If vboxClipboardReadDataFromVBox fails then pClient may be invalid */
        LogRelFunc (("vboxClipboardReadDataFromVBox returned %Rrc%s\n", rc,
                     RT_SUCCESS(rc) ? ", g_ctx.pClient->data.cb == 0" :  ""));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(g_ctx.pClient->data.pv);
    cwSrcLen = g_ctx.pClient->data.cb / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
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
        LogRelFunc (("failed to allocate %d bytes\n", cwDestLen * 2));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Allocate enough space, as RTUtf16ToUtf8Ex may fail if the
       space is too tightly calculated. */
    pu8DestText = XtMalloc(cwDestLen * 4);
    if (pu8DestText == 0)
    {
        LogRelFunc (("failed to allocate %d bytes\n", cwDestLen * 4));
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
        LogRelFunc (("clipboard conversion failed.  RTUtf16ToUtf8Ex() returned %Rrc.  Abandoning.\n", rc));
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
 * @param  atomTypeReturn The type of the data we are returning
 * @param  pValReturn     A pointer to the data we are returning.  This should be to memory
 *                        allocated by XtMalloc, which will be freed by the toolkit later
 * @param  pcLenReturn    The length of the data we are returning
 * @param  piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 * @thread  clipboard X11 event thread
 * @note    called by vboxClipboardConvertForX11
 */
static Boolean vboxClipboardConvertToCTextForX11(Atom *atomTypeReturn,
                                                 XtPointer *pValReturn,
                                                 unsigned long *pcLenReturn,
                                                 int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    char *pu8DestText = 0;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    XTextProperty property;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = vboxClipboardReadDataFromVBox(&g_ctx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    if ((rc != VINF_SUCCESS) || (g_ctx.pClient->data.cb == 0))
    {
        /* If vboxClipboardReadDataFromVBox fails then pClient may be invalid */
        LogRelFunc (("vboxClipboardReadDataFromVBox returned %Rrc%s\n", rc,
                      RT_SUCCESS(rc) ? ", g_ctx.pClient->data.cb == 0" :  ""));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(g_ctx.pClient->data.pv);
    cwSrcLen = g_ctx.pClient->data.cb / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
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
        LogRelFunc (("failed to allocate %d bytes\n", cwDestLen * 2));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        vboxClipboardEmptyGuestBuffer();
        return false;
    }
    /* Convert the Utf16 string to Utf8. */
    rc = RTUtf16ToUtf8Ex(pu16DestText + 1, cwDestLen - 1, &pu8DestText, 0, &cbDestLen);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  RTUtf16ToUtf8Ex() returned %Rrc.  Abandoning.\n", rc));
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
        LogRelFunc (("Xutf8TextListToTextProperty failed.  Reason: %s\n",
                pcReason));
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
 * Callback to request VBox's clipboard data for an X11 client.  Called by the
 * X Toolkit.
 * @thread  clipboard X11 event thread
 * @note    callback for XtOwnSelection, called by vboxClipboardFormatAnnounce
 */
static Boolean vboxClipboardConvertForX11(Widget, Atom *atomSelection,
                                          Atom *atomTarget,
                                          Atom *atomTypeReturn,
                                          XtPointer *pValReturn,
                                          unsigned long *pcLenReturn,
                                          int *piFormatReturn)
{
    g_eClipboardFormats eFormat = INVALID;

    LogFlowFunc(("\n"));
    /* Drop requests that we receive too late. */
    if (g_ctx.eOwner != VB)
        return false;
    if (   (*atomSelection != g_ctx.atomClipboard)
        && (*atomSelection != g_ctx.atomPrimary)
       )
    {
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    if (g_debugClipboard)
    {
        char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), *atomTarget);
        if (szAtomName != 0)
        {
            Log2 (("%s: request for format %s\n", __PRETTY_FUNCTION__, szAtomName));
            XFree(szAtomName);
        }
        else
        {
            LogFunc (("request for invalid target atom %d!\n", *atomTarget));
        }
    }
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
        return vboxClipboardConvertTargetsForX11(atomTypeReturn, pValReturn,
                                                 pcLenReturn, piFormatReturn);
    case UTF16:
        return vboxClipboardConvertUtf16(atomTypeReturn, pValReturn, pcLenReturn,
                                         piFormatReturn);
    case UTF8:
        return vboxClipboardConvertToUtf8ForX11(atomTypeReturn, pValReturn,
                                                pcLenReturn, piFormatReturn);
    case CTEXT:
        return vboxClipboardConvertToCTextForX11(atomTypeReturn, pValReturn,
                                                 pcLenReturn, piFormatReturn);
    default:
        LogFunc (("bad format\n"));
        return false;
    }
}

/**
 * This is called by the X toolkit intrinsics to let us know that another
 * X11 client has taken the clipboard.
 * @note    callback for XtOwnSelection, called from vboxClipboardFormatAnnounce
 * @thread  clipboard X11 event thread
 */
static void vboxClipboardReturnToX11(Widget, Atom *)
{
    LogFlowFunc (("called, giving VBox clipboard ownership\n"));
    g_ctx.eOwner = X11;
    g_ctx.notifyGuest = true;
}

/**
 * VBox is taking possession of the shared clipboard.
 *
 * @param pClient    Context data for the guest system
 * @param u32Formats Clipboard formats the guest is offering
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Formats)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;

    pClient->pCtx->guestFormats = u32Formats;
    LogFlowFunc (("u32Formats=%d\n", u32Formats));
    if (u32Formats == 0)
    {
        /* This is just an automatism, not a genuine anouncement */
        LogFlowFunc(("returning\n"));
        return;
    }
    if (g_ctx.eOwner == VB)
    {
        /* We already own the clipboard, so no need to grab it, especially as that can lead
           to races due to the asynchronous nature of the X11 clipboard.  This event may also
           have been sent out by the guest to invalidate the Windows clipboard cache. */
        LogFlowFunc(("returning\n"));
        return;
    }
    Log2 (("%s: giving the guest clipboard ownership\n", __PRETTY_FUNCTION__));
    g_ctx.eOwner = VB;
    g_ctx.hostTextFormat = INVALID;
    g_ctx.hostBitmapFormat = INVALID;
    if (XtOwnSelection(g_ctx.widget, g_ctx.atomClipboard, CurrentTime, vboxClipboardConvertForX11,
                       vboxClipboardReturnToX11, 0) != True)
    {
        Log2 (("%s: returning clipboard ownership to the host\n", __PRETTY_FUNCTION__));
        /* We set this so that the guest gets notified when we take the clipboard, even if no
          guest formats are found which we understand. */
        g_ctx.notifyGuest = true;
        g_ctx.eOwner = X11;
    }
    XtOwnSelection(g_ctx.widget, g_ctx.atomPrimary, CurrentTime, vboxClipboardConvertForX11,
                   NULL, 0);
    LogFlowFunc(("returning\n"));

}

/**
 * Called when VBox wants to read the X11 clipboard.
 *
 * @param pClient   Context information about the guest VM
 * @param u32Format The format that the guest would like to receive the data in
 * @param pv        Where to write the data to
 * @param cb        The size of the buffer to write the data to
 * @param pcbActual Where to write the actual size of the written data
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
int vboxClipboardReadData (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Format, void *pv,
                           uint32_t cb, uint32_t *pcbActual)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
    {
        /* no data available */
        *pcbActual = 0;
        return VINF_SUCCESS;
    }

    LogFlowFunc (("u32Format = %d, cb = %d\n", u32Format, cb));

    /*
     * The guest wants to read data in the given format.
     */
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        if (g_ctx.hostTextFormat == INVALID)
        {
            /* No data available. */
            *pcbActual = 0;
            return VERR_NO_DATA;  /* The guest thinks we have data and we don't */
        }
        /* No one else (VBox or X11) should currently be waiting.  The first because
         * requests from VBox are serialised and the second because X11 previously
         * grabbed the clipboard, so it should not be waiting for data from us. */
        AssertLogRelReturn (ASMAtomicCmpXchgU32(&g_ctx.waiter, VB, NONE), VERR_DEADLOCK);
        g_ctx.requestHostFormat = g_ctx.hostTextFormat;
        g_ctx.requestBuffer = pv;
        g_ctx.requestBufferSize = cb;
        g_ctx.requestActualSize = pcbActual;
        /* Initially set the size of the data read to zero in case we fail
         * somewhere. */
        *pcbActual = 0;
        /* Send out a request for the data to the current clipboard owner */
        XtGetSelectionValue(g_ctx.widget, g_ctx.atomClipboard, g_ctx.atomHostTextFormat,
                            vboxClipboardGetDataFromX11, reinterpret_cast<XtPointer>(g_ctx.pClient),
                            CurrentTime);
        /* When the data arrives, the vboxClipboardGetDataFromX11 callback will be called.  The
           callback will signal the event semaphore when it has processed the data for us. */

        int rc = RTSemEventWait(g_ctx.waitForData, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
        {
            g_ctx.waiter = NONE;
            return rc;
        }
        g_ctx.waiter = NONE;
    }
    else
    {
        return VERR_NOT_IMPLEMENTED;
    }
    return VINF_SUCCESS;
}

/**
 * Called when we have requested data from VBox and that data has arrived.
 *
 * @param pClient   Context information about the guest VM
 * @param pv        Buffer to which the data was written
 * @param cb        The size of the data written
 * @param u32Format The format of the data written
 * @note    Called by the HGCM clipboard service
 * @thread  HGCM clipboard service thread
 */
void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv, uint32_t cb, uint32_t u32Format)
{
    if (!g_fHaveX11)
        return;

    LogFlowFunc (("called\n"));

    /* Assert that no other transfer is in process (requests are serialised)
     * or has not cleaned up properly. */
    AssertLogRelReturnVoid (   pClient->data.pv == NULL
                            && pClient->data.cb == 0
                            && pClient->data.u32Format == 0);

    /* Grab the mutex and check that X11 is still waiting for the data before
     * delivering it.  See the explanation in vboxClipboardReadDataFromVBox. */
    RTSemMutexRequest(g_ctx.clipboardMutex, RT_INDEFINITE_WAIT);
    if (g_ctx.waiter == X11 && cb > 0)
    {
        pClient->data.pv = RTMemAlloc (cb);

        if (pClient->data.pv)
        {
            memcpy (pClient->data.pv, pv, cb);
            pClient->data.cb = cb;
            pClient->data.u32Format = u32Format;
        }
    }
    RTSemMutexRelease(g_ctx.clipboardMutex);

    RTSemEventSignal(g_ctx.waitForData);
}

