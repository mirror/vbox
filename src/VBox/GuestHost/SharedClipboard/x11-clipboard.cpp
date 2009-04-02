/** @file
 *
 * Shared Clipboard:
 * X11 backend code.
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

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD

#include <vector>

#ifdef RT_OS_SOLARIS
#include <tsol/label.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xproto.h>
#include <X11/StringDefs.h>

#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include <VBox/log.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>

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

/** The X11 clipboard uses several names for the same format.  This
 * structure maps an X11 name to a format. */
typedef struct {
    Atom atom;
    g_eClipboardFormats format;
    unsigned guestFormat;
} VBOXCLIPBOARDFORMAT;

/** Global context information used by the X11 clipboard backend */
struct _VBOXCLIPBOARDCONTEXTX11
{
    /** Opaque data structure describing the front-end. */
    VBOXCLIPBOARDCONTEXT *pFrontend;
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

    /** Does VBox or X11 currently own the clipboard? */
    volatile enum g_eOwner eOwner;

    /** What is the best text format X11 has to offer?  INVALID for none. */
    g_eClipboardFormats X11TextFormat;
    /** Atom corresponding to the X11 text format */
    Atom atomX11TextFormat;
    /** What is the best bitmap format X11 has to offer?  INVALID for none. */
    g_eClipboardFormats X11BitmapFormat;
    /** Atom corresponding to the X11 Bitmap format */
    Atom atomX11BitmapFormat;
    /** What formats does VBox have on offer? */
    int vboxFormats;
    /** Windows hosts and guests cache the clipboard data they receive.
     * Since we have no way of knowing whether their cache is still valid,
     * we always send a "data changed" message after a successful transfer
     * to invalidate it. */
    bool notifyVBox;

    /** Since the clipboard data moves asynchronously, we use an event
     * semaphore to wait for it.  When a function issues a request for
     * clipboard data it must wait for this semaphore, which is triggered
     * when the data arrives. */
    RTSEMEVENT waitForData;
};

/* Only one client is supported. There seems to be no need for more clients. 
 */
static VBOXCLIPBOARDCONTEXTX11 g_ctxX11;
static VBOXCLIPBOARDCONTEXTX11 *g_pCtx;

/* Are we actually connected to the X server? */
static bool g_fHaveX11;

/**
 * Convert the UTF-16 text obtained from the X11 clipboard to UTF-16LE with
 * Windows EOLs, place it in the buffer supplied and signal that data has
 * arrived.
 *
 * @param pValue      Source UTF-16 text
 * @param cwSourceLen Length in 16-bit words of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by cb
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @note  X11 backend code, called from the Xt callback when we wish to read
 *        the X11 clipboard.
 */
static void vboxClipboardGetUtf16(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                  XtPointer pValue, unsigned cwSrcLen,
                                  void *pv, unsigned cb,
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
    RTSemEventSignal(pCtx->waitForData);
    LogFlowFunc(("Returning.  Status is %Rrc\n", rc));
}

/**
 * Convert the UTF-8 text obtained from the X11 clipboard to UTF-16LE with
 * Windows EOLs, place it in the buffer supplied and signal that data has
 * arrived.
 *
 * @param pValue      Source UTF-8 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by pv
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @note  X11 backend code, called from the Xt callback when we wish to read
 *        the X11 clipboard.
 */
static void vboxClipboardGetUtf8FromX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                        XtPointer pValue, unsigned cbSrcLen,
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
    RTSemEventSignal(pCtx->waitForData);
    LogFlowFunc(("Returning.  Status is %Rrc", rc));
}

/**
 * Convert the COMPOUND_TEXT obtained from the X11 clipboard to UTF-16LE with
 * Windows EOLs, place it in the buffer supplied and signal that data has
 * arrived.
 *
 * @param pValue      Source COMPOUND_TEXT
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by pv
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @note  X11 backend code, called from the Xt callback when we wish to read
 *        the X11 clipboard.
 */
static void vboxClipboardGetCTextFromX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                         XtPointer pValue, unsigned cbSrcLen,
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
    property.encoding = pCtx->atomCText;
    property.format = 8;
    property.nitems = cbSrcLen;
#ifdef RT_OS_SOLARIS
    int xrc = XmbTextPropertyToTextList(XtDisplay(pCtx->widget), &property,
                                        &ppu8SrcText, &cProps);
#else
    int xrc = Xutf8TextPropertyToTextList(XtDisplay(pCtx->widget),
                                          &property, &ppu8SrcText, &cProps);
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
    RTSemEventSignal(pCtx->waitForData);
}

/**
 * Convert the Latin1 text obtained from the X11 clipboard to UTF-16LE with
 * Windows EOLs, place it in the buffer supplied and signal that data has
 * arrived.
 *
 * @param pValue      Source Latin1 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 * @param pv          Where to store the converted data
 * @param cb          Length in bytes of the buffer pointed to by cb
 * @param pcbActual   Where to store the size of the converted data
 * @param pClient     Pointer to the client context structure
 * @note  X11 backend code, called from the Xt callback when we wish to read
 *        the X11 clipboard.
 */
static void vboxClipboardGetLatin1FromX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                          XtPointer pValue,
                                          unsigned cbSourceLen, void *pv,
                                          unsigned cb, uint32_t *pcbActual)
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
    RTSemEventSignal(pCtx->waitForData);
    LogFlowFunc(("Returning.  Status is %Rrc\n", rc));
}

/**
 * Convert the text obtained from the X11 clipboard to UTF-16LE with Windows
 * EOLs, place it in the buffer supplied and signal that data has arrived.
 * @note  X11 backend code, callback for XtGetSelectionValue, for use when
 *        the X11 clipboard contains a text format we understand.
 */
static void vboxClipboardGetDataFromX11(Widget, XtPointer pClientData,
                                        Atom * /* selection */,
                                        Atom *atomType,
                                        XtPointer pValue,
                                        long unsigned int *pcLen,
                                        int *piFormat)
{
    VBOXCLIPBOARDREQUEST *pRequest
        = reinterpret_cast<VBOXCLIPBOARDREQUEST *>(pClientData);
    VBOXCLIPBOARDCONTEXTX11 *pCtx = pRequest->pCtx;
    LogFlowFunc(("pClientData=%p, *pcLen=%lu, *piFormat=%d\n", pClientData,
                 *pcLen, *piFormat));
    LogFlowFunc(("pCtx->X11TextFormat=%d, pRequest->cb=%d\n",
                 pCtx->X11TextFormat, pRequest->cb));
    unsigned cTextLen = (*pcLen) * (*piFormat) / 8;
    /* The X Toolkit may have failed to get the clipboard selection for us. */
    if (*atomType == XT_CONVERT_FAIL)
        return;
    /* The clipboard selection may have changed before we could get it. */
    if (NULL == pValue)
        return;
    /* In which format is the clipboard data? */
    switch (pCtx->X11TextFormat)
    {
    case UTF16:
        vboxClipboardGetUtf16(pCtx, pValue, cTextLen / 2, pRequest->pv,
                              pRequest->cb, pRequest->pcbActual);
        break;
    case CTEXT:
        vboxClipboardGetCTextFromX11(pCtx, pValue, cTextLen, pRequest->pv,
                                     pRequest->cb, pRequest->pcbActual);
        break;
    case UTF8:
    {
        /* If we are given broken Utf-8, we treat it as Latin1.  Is this acceptable? */
        size_t cStringLen;
        char *pu8SourceText = reinterpret_cast<char *>(pValue);

        if ((pCtx->X11TextFormat == UTF8)
            && (RTStrUniLenEx(pu8SourceText, *pcLen, &cStringLen) == VINF_SUCCESS))
        {
            vboxClipboardGetUtf8FromX11(pCtx, pValue, cTextLen, pRequest->pv,
                                     pRequest->cb, pRequest->pcbActual);
            break;
        }
        else
        {
            vboxClipboardGetLatin1FromX11(pCtx, pValue, cTextLen,
                                          pRequest->pv, pRequest->cb,
                                          pRequest->pcbActual);
            break;
        }
    }
    default:
        LogFunc (("bad target format\n"));
        XtFree(reinterpret_cast<char *>(pValue));
        return;
    }
    pCtx->notifyVBox = true;
}

/**
 * Notify the host clipboard about the data formats we support, based on the
 * "targets" (available data formats) information obtained from the X11
 * clipboard.
 * @note  X11 backend code, callback for XtGetSelectionValue, called when we
 *        poll for available targets.
 */
static void vboxClipboardGetTargetsFromX11(Widget,
                                           XtPointer pClientData,
                                           Atom * /* selection */,
                                           Atom *atomType,
                                           XtPointer pValue,
                                           long unsigned int *pcLen,
                                           int *piFormat)
{
    VBOXCLIPBOARDCONTEXTX11 *pCtx =
            reinterpret_cast<VBOXCLIPBOARDCONTEXTX11 *>(pClientData);
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

    for (unsigned i = 0; i < cAtoms; ++i)
    {
        for (unsigned j = 0; j != pCtx->formatList.size(); ++j)
            if (pCtx->formatList[j].atom == atomTargets[i])
            {
                if (eBestTarget < pCtx->formatList[j].format)
                {
                    eBestTarget = pCtx->formatList[j].format;
                    atomBestTarget = pCtx->formatList[j].atom;
                }
                break;
            }
        if (g_debugClipboard)
        {
            char *szAtomName = XGetAtomName(XtDisplay(pCtx->widget),
                                            atomTargets[i]);
            if (szAtomName != 0)
            {
                Log2 (("%s: the host offers target %s\n", __PRETTY_FUNCTION__,
                       szAtomName));
                XFree(szAtomName);
            }
        }
    }
    pCtx->atomX11TextFormat = atomBestTarget;
    if ((eBestTarget != pCtx->X11TextFormat) || (pCtx->notifyVBox == true))
    {
        uint32_t u32Formats = 0;
        if (g_debugClipboard)
        {
            if (atomBestTarget != None)
            {
                char *szAtomName = XGetAtomName(XtDisplay(pCtx->widget),
                                                atomBestTarget);
                Log2 (("%s: switching to host text target %s.  Available targets are:\n",
                       __PRETTY_FUNCTION__, szAtomName));
                XFree(szAtomName);
            }
            else
                Log2(("%s: no supported host text target found.  Available targets are:\n",
                      __PRETTY_FUNCTION__));
            for (unsigned i = 0; i < cAtoms; ++i)
            {
                char *szAtomName = XGetAtomName(XtDisplay(pCtx->widget),
                                                atomTargets[i]);
                if (szAtomName != 0)
                {
                    Log2 (("%s:     %s\n", __PRETTY_FUNCTION__, szAtomName));
                    XFree(szAtomName);
                }
            }
        }
        pCtx->X11TextFormat = eBestTarget;
        if (eBestTarget != INVALID)
            u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        VBoxX11ClipboardReportX11Formats(pCtx->pFrontend, u32Formats);
        pCtx->notifyVBox = false;
    }
    XtFree(reinterpret_cast<char *>(pValue));
}

/**
 * This timer callback is called every 200ms to check the contents of the X11
 * clipboard.
 * @note  X11 backend code, callback for XtAppAddTimeOut, recursively
 *        re-armed.
 * @todo  Use the XFIXES extension to check for new clipboard data when
 *        available.
 */
static void vboxClipboardPollX11ForTargets(XtPointer pUserData,
                                           XtIntervalId * /* hTimerId */)
{
    VBOXCLIPBOARDCONTEXTX11 *pCtx =
            reinterpret_cast<VBOXCLIPBOARDCONTEXTX11 *>(pUserData);
    Log3 (("%s: called\n", __PRETTY_FUNCTION__));
    /* Get the current clipboard contents */
    if (pCtx->eOwner == X11)
    {
        Log3 (("%s: requesting the targets that the host clipboard offers\n",
               __PRETTY_FUNCTION__));
        XtGetSelectionValue(pCtx->widget, pCtx->atomClipboard,
                            pCtx->atomTargets,
                            vboxClipboardGetTargetsFromX11, pCtx,
                            CurrentTime);
    }
    /* Re-arm our timer */
    XtAppAddTimeOut(pCtx->appContext, 200 /* ms */,
                    vboxClipboardPollX11ForTargets, pCtx);
}

/** We store information about the target formats we can handle in a global
 * vector for internal use.
 * @note  X11 backend code.
 */
static void vboxClipboardAddFormat(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                   const char *pszName,
                                   g_eClipboardFormats eFormat,
                                   unsigned guestFormat)
{
    VBOXCLIPBOARDFORMAT sFormat;
    /* Get an atom from the X server for that target format */
    Atom atomFormat = XInternAtom(XtDisplay(pCtx->widget), pszName, false);
    sFormat.atom   = atomFormat;
    sFormat.format = eFormat;
    sFormat.guestFormat = guestFormat;
    pCtx->formatList.push_back(sFormat);
    LogFlow (("vboxClipboardAddFormat: added format %s (%d)\n", pszName, eFormat));
}

/**
 * The main loop of our clipboard reader.
 * @note  X11 backend code.
 */
static int vboxClipboardThread(RTTHREAD self, void *pvUser)
{
    LogRel(("Shared clipboard: starting host clipboard thread\n"));

    VBOXCLIPBOARDCONTEXTX11 *pCtx =
            reinterpret_cast<VBOXCLIPBOARDCONTEXTX11 *>(pvUser);
    /* Set up a timer to poll the host clipboard */
    XtAppAddTimeOut(pCtx->appContext, 200 /* ms */,
                    vboxClipboardPollX11ForTargets, pCtx);

    XtAppMainLoop(pCtx->appContext);
    pCtx->formatList.clear();
    LogRel(("Shared clipboard: host clipboard thread terminated successfully\n"));
    return VINF_SUCCESS;
}

/** X11 specific initialisation for the shared clipboard.
 * @note  X11 backend code.
 */
static int vboxClipboardInitX11 (VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
    pCtx->appContext = XtCreateApplicationContext();
    // XtAppSetFallbackResources(pCtx->appContext, szFallbackResources);
    pDisplay = XtOpenDisplay(pCtx->appContext, 0, 0, "VBoxClipboard", 0, 0, &cArgc, &pcArgv);
    if (NULL == pDisplay)
    {
        LogRel(("Shared clipboard: failed to connect to the host clipboard - the window system may not be running.\n"));
        rc = VERR_NOT_SUPPORTED;
    }
    if (RT_SUCCESS(rc))
    {
        pCtx->widget = XtVaAppCreateShell(0, "VBoxClipboard", applicationShellWidgetClass, pDisplay,
                                          XtNwidth, 1, XtNheight, 1, NULL);
        if (NULL == pCtx->widget)
        {
            LogRel(("Shared clipboard: failed to construct the X11 window for the host clipboard manager.\n"));
            rc = VERR_NO_MEMORY;
        }
    }
    if (RT_SUCCESS(rc))
    {
        XtSetMappedWhenManaged(pCtx->widget, false);
        XtRealizeWidget(pCtx->widget);

        /* Get hold of the atoms which we need */
        pCtx->atomClipboard = XInternAtom(XtDisplay(pCtx->widget), "CLIPBOARD", false /* only_if_exists */);
        pCtx->atomPrimary   = XInternAtom(XtDisplay(pCtx->widget), "PRIMARY",   false);
        pCtx->atomTargets   = XInternAtom(XtDisplay(pCtx->widget), "TARGETS",   false);
        pCtx->atomMultiple  = XInternAtom(XtDisplay(pCtx->widget), "MULTIPLE",  false);
        pCtx->atomTimestamp = XInternAtom(XtDisplay(pCtx->widget), "TIMESTAMP", false);
        pCtx->atomUtf16     = XInternAtom(XtDisplay(pCtx->widget),
                                          "text/plain;charset=ISO-10646-UCS-2", false);
        pCtx->atomUtf8      = XInternAtom(XtDisplay(pCtx->widget), "UTF_STRING", false);
        /* And build up the vector of supported formats */
        pCtx->atomCText     = XInternAtom(XtDisplay(pCtx->widget), "COMPOUND_TEXT", false);
        /* And build up the vector of supported formats */
        if (!g_testUtf8 && !g_testCText)
            vboxClipboardAddFormat(pCtx,
                                   "text/plain;charset=ISO-10646-UCS-2",
                                   UTF16,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
        if (!g_testUtf16 && !g_testCText)
        {
            vboxClipboardAddFormat(pCtx, "UTF8_STRING", UTF8,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
            vboxClipboardAddFormat(pCtx, "text/plain;charset=UTF-8", UTF8,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
            vboxClipboardAddFormat(pCtx, "text/plain;charset=utf-8", UTF8,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
            vboxClipboardAddFormat(pCtx, "STRING", UTF8,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
            vboxClipboardAddFormat(pCtx, "TEXT", UTF8,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
            vboxClipboardAddFormat(pCtx, "text/plain", UTF8,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
}
        if (!g_testUtf16 && !g_testUtf8)
            vboxClipboardAddFormat(pCtx, "COMPOUND_TEXT", CTEXT,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    }
    return rc;
}

/**
 * Initialise the X11 backend of the shared clipboard.
 * @note  X11 backend code
 */
int VBoxX11ClipboardInitX11(VBOXCLIPBOARDCONTEXT *pFrontend,
                            VBOXCLIPBOARDCONTEXTX11 **ppBackend)
{
    int rc;

    VBOXCLIPBOARDCONTEXTX11 *pCtx = &g_ctxX11;
    /** @todo we still only support one backend at a time, because the X
     * toolkit intrinsics don't support user data in XtOwnSelection.  Not
     * a big problem, but not clean either. */
    AssertReturn(g_pCtx == NULL, VERR_NOT_SUPPORTED);
    g_pCtx = &g_ctxX11;
    if (!RTEnvGet("DISPLAY"))
    {
        /*
         * If we don't find the DISPLAY environment variable we assume that we are not
         * connected to an X11 server. Don't actually try to do this then, just fail
         * silently and report success on every call. This is important for VBoxHeadless.
         */
        LogRelFunc(("X11 DISPLAY variable not set -- disabling shared clipboard\n"));
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

    LogRel(("Initializing X11 clipboard backend\n"));
    pCtx->pFrontend = pFrontend;
    RTSemEventCreate(&pCtx->waitForData);
    rc = vboxClipboardInitX11(pCtx);
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadCreate(&pCtx->thread, vboxClipboardThread, pCtx, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
        if (RT_FAILURE(rc))
            LogRel(("Failed to initialise the shared clipboard X11 backend.\n"));
    }
    if (RT_FAILURE(rc))
        RTSemEventDestroy(pCtx->waitForData);
    *ppBackend = pCtx;
    return rc;
}

/**
 * Terminate the shared clipboard X11 backend.
 * @note  X11 backend code
 */
int VBoxX11ClipboardTermX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
{
    int rc, rcThread;
    unsigned count = 0;
    XEvent ev;

    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    LogRelFunc(("shutting down the shared clipboard X11 backend\n"));

    /* Set the termination flag.  This has been observed to block if it was set
     * during a request for clipboard data coming from X11, so only we do it
     * after releasing any such requests. */
    XtAppSetExitFlag(pCtx->appContext);
    /* Wake up the event loop */
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.format = 8;
    XSendEvent(XtDisplay(pCtx->widget), XtWindow(pCtx->widget), false, 0, &ev);
    XFlush(XtDisplay(pCtx->widget));
    do
    {
        rc = RTThreadWait(pCtx->thread, 1000, &rcThread);
        ++count;
        Assert(RT_SUCCESS(rc) || ((VERR_TIMEOUT == rc) && (count != 5)));
    } while ((VERR_TIMEOUT == rc) && (count < 300));
    if (RT_SUCCESS(rc))
    {
        /* We can safely destroy this now, as only this thread ever waits
         * for it. */
        RTSemEventDestroy(pCtx->waitForData);
        AssertRC(rcThread);
    }
    else
        LogRel(("vboxClipboardDestroy: rc=%Rrc\n", rc));
    XtCloseDisplay(XtDisplay(pCtx->widget));
    LogFlowFunc(("returning %Rrc.\n", rc));
    return rc;
}

/**
 * Announce to the X11 backend that we are ready to start.
 * @param  owner who is the initial clipboard owner
 */
int VBoxX11ClipboardStartX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                             enum g_eOwner owner)
{
    LogFlowFunc(("\n"));
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    pCtx->eOwner = owner;
    if (owner == X11)
        pCtx->notifyVBox = true;
    else
    {
        /** @todo Check whether the guest gets a format announcement at
          *       startup. */
        VBoxX11ClipboardAnnounceVBoxFormat(pCtx, 0);
    }
    return VINF_SUCCESS;
}

/**
 * Called when the VBox may have fallen out of sync with the backend.
 * @note  X11 backend code
 */
void VBoxX11ClipboardRequestSyncX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;
    pCtx->notifyVBox = true;
}

/**
 * Shut down the shared clipboard X11 backend.
 * @note  X11 backend code
 */
void VBoxX11ClipboardStopX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;

    pCtx->eOwner = NONE;
    pCtx->X11TextFormat = INVALID;
    pCtx->X11BitmapFormat = INVALID;
}

/**
 * Satisfy a request from X11 for clipboard targets supported by VBox.
 *
 * @returns true if we successfully convert the data to the format
 *          requested, false otherwise.
 *
 * @param  atomTypeReturn The type of the data we are returning
 * @param  pValReturn     A pointer to the data we are returning.  This
 *                        should be set to memory allocated by XtMalloc,
 *                        which will be freed later by the Xt toolkit.
 * @param  pcLenReturn    The length of the data we are returning
 * @param  piFormatReturn The format (8bit, 16bit, 32bit) of the data we are
 *                        returning
 * @note  X11 backend code, called by the XtOwnSelection callback.
 */
static Boolean vboxClipboardConvertTargetsForX11(VBOXCLIPBOARDCONTEXTX11
                                                                      *pCtx,
                                                 Atom *atomTypeReturn,
                                                 XtPointer *pValReturn,
                                                 unsigned long *pcLenReturn,
                                                 int *piFormatReturn)
{
    unsigned uListSize = pCtx->formatList.size();
    Atom *atomTargets = reinterpret_cast<Atom *>(XtMalloc((uListSize + 3) * sizeof(Atom)));
    unsigned cTargets = 0;

    LogFlowFunc (("called\n"));
    for (unsigned i = 0; i < uListSize; ++i)
    {
        if (   ((pCtx->vboxFormats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT) != 0)
            && (   pCtx->formatList[i].guestFormat
                == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT))
        {
            atomTargets[cTargets] = pCtx->formatList[i].atom;
            ++cTargets;
        }
    }
    atomTargets[cTargets] = pCtx->atomTargets;
    atomTargets[cTargets + 1] = pCtx->atomMultiple;
    atomTargets[cTargets + 2] = pCtx->atomTimestamp;
    if (g_debugClipboard)
    {
        for (unsigned i = 0; i < cTargets + 3; i++)
        {
            char *szAtomName = XGetAtomName(XtDisplay(pCtx->widget), atomTargets[i]);
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
 * Satisfy a request from X11 to convert the clipboard text to Utf16.  We
 * return non-zero terminated text.
 * @todo that works, but it is bad.  Change it to return zero-terminated
 *       text.
 *
 * @returns true if we successfully convert the data to the format
 * requested, false otherwise.
 *
 * @param  atomTypeReturn  Where to store the atom for the type of the data
 *                         we are returning
 * @param  pValReturn      Where to store the pointer to the data we are
 *                         returning.  This should be to memory allocated by
 *                         XtMalloc, which will be freed by the Xt toolkit
 *                         later.
 * @param  pcLenReturn     Where to store the length of the data we are
 *                         returning
 * @param  piFormatReturn  Where to store the bit width (8, 16, 32) of the
 *                         data we are returning
 * @note  X11 backend code, called by the callback for XtOwnSelection.
 */
static Boolean vboxClipboardConvertUtf16(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                         Atom *atomTypeReturn,
                                         XtPointer *pValReturn,
                                         unsigned long *pcLenReturn,
                                         int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    void *pvVBox;
    uint32_t cbVBox;
    size_t cwSrcLen, cwDestLen;
    int rc;

    LogFlowFunc (("called\n"));
    rc = VBoxX11ClipboardReadVBoxData(pCtx->pFrontend, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT, &pvVBox, &cbVBox);
    if ((RT_FAILURE(rc)) || (cbVBox == 0))
    {
        /* If VBoxX11ClipboardReadVBoxData fails then pClient may be invalid */
        LogRelFunc (("VBoxX11ClipboardReadVBoxData returned %Rrc%s\n", rc,
                    RT_SUCCESS(rc) ? ", cbVBox == 0" :  ""));
        RTMemFree(pvVBox);
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(pvVBox);
    cwSrcLen = cbVBox / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf16: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(pvVBox);
        AssertRCReturn(rc, false);
    }
    if (cwDestLen == 0)
    {
        LogFlowFunc(("received empty clipboard data from the guest, returning false.\n"));
        RTMemFree(pvVBox);
        return false;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(XtMalloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        LogRel(("vboxClipboardConvertUtf16: failed to allocate %d bytes\n", cwDestLen * 2));
        RTMemFree(pvVBox);
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxClipboardConvertUtf16: clipboard conversion failed.  vboxClipboardUtf16WinToLin returned %Rrc.  Abandoning.\n", rc));
        XtFree(reinterpret_cast<char *>(pu16DestText));
        RTMemFree(pvVBox);
        return false;
    }
    LogFlowFunc (("converted string is %.*ls. Returning.\n", cwDestLen, pu16DestText));
    RTMemFree(pvVBox);
    *atomTypeReturn = pCtx->atomUtf16;
    *pValReturn = reinterpret_cast<XtPointer>(pu16DestText);
    *pcLenReturn = cwDestLen;
    *piFormatReturn = 16;
    return true;
}

/**
 * Satisfy a request from X11 to convert the clipboard text to Utf8.  We
 * return non-zero terminated text.
 * @todo that works, but it is bad.  Change it to return zero-terminated
 *       text.
 *
 * @returns true if we successfully convert the data to the format
 * requested, false otherwise.
 *
 * @param  atomTypeReturn  Where to store the atom for the type of the data
 *                         we are returning
 * @param  pValReturn      Where to store the pointer to the data we are
 *                         returning.  This should be to memory allocated by
 *                         XtMalloc, which will be freed by the Xt toolkit
 *                         later.
 * @param  pcLenReturn     Where to store the length of the data we are
 *                         returning
 * @param  piFormatReturn  Where to store the bit width (8, 16, 32) of the
 *                         data we are returning
 * @note  X11 backend code, called by the callback for XtOwnSelection.
 */
static Boolean vboxClipboardConvertToUtf8ForX11(VBOXCLIPBOARDCONTEXTX11
                                                                      *pCtx,
                                                Atom *atomTypeReturn,
                                                XtPointer *pValReturn,
                                                unsigned long *pcLenReturn,
                                                int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    char *pu8DestText;
    void *pvVBox;
    uint32_t cbVBox;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = VBoxX11ClipboardReadVBoxData(pCtx->pFrontend, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT, &pvVBox, &cbVBox);
    if ((rc != VINF_SUCCESS) || (cbVBox == 0))
    {
        /* If VBoxX11ClipboardReadVBoxData fails then pClient may be invalid */
        LogRelFunc (("VBoxX11ClipboardReadVBoxData returned %Rrc%s\n", rc,
                     RT_SUCCESS(rc) ? ", cbVBox == 0" :  ""));
        RTMemFree(pvVBox);
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(pvVBox);
    cwSrcLen = cbVBox / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(pvVBox);
        AssertRCReturn(rc, false);
    }
    if (cwDestLen == 0)
    {
        LogFlowFunc(("received empty clipboard data from the guest, returning false.\n"));
        RTMemFree(pvVBox);
        return false;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        LogRelFunc (("failed to allocate %d bytes\n", cwDestLen * 2));
        RTMemFree(pvVBox);
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        RTMemFree(pvVBox);
        return false;
    }
    /* Allocate enough space, as RTUtf16ToUtf8Ex may fail if the
       space is too tightly calculated. */
    pu8DestText = XtMalloc(cwDestLen * 4);
    if (pu8DestText == 0)
    {
        LogRelFunc (("failed to allocate %d bytes\n", cwDestLen * 4));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        RTMemFree(pvVBox);
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
        RTMemFree(pvVBox);
        return false;
    }
    LogFlowFunc (("converted string is %.*s. Returning.\n", cbDestLen, pu8DestText));
    RTMemFree(pvVBox);
    *atomTypeReturn = pCtx->atomUtf8;
    *pValReturn = reinterpret_cast<XtPointer>(pu8DestText);
    *pcLenReturn = cbDestLen;
    *piFormatReturn = 8;
    return true;
}

/**
 * Satisfy a request from X11 to convert the clipboard text to
 * COMPOUND_TEXT.  We return non-zero terminated text.
 * @todo that works, but it is bad.  Change it to return zero-terminated
 *       text.
 *
 * @returns true if we successfully convert the data to the format
 * requested, false otherwise.
 *
 * @param  atomTypeReturn  Where to store the atom for the type of the data
 *                         we are returning
 * @param  pValReturn      Where to store the pointer to the data we are
 *                         returning.  This should be to memory allocated by
 *                         XtMalloc, which will be freed by the Xt toolkit
 *                         later.
 * @param  pcLenReturn     Where to store the length of the data we are
 *                         returning
 * @param  piFormatReturn  Where to store the bit width (8, 16, 32) of the
 *                         data we are returning
 * @note  X11 backend code, called by the callback for XtOwnSelection.
 */
static Boolean vboxClipboardConvertToCTextForX11(VBOXCLIPBOARDCONTEXTX11
                                                                      *pCtx,
                                                 Atom *atomTypeReturn,
                                                 XtPointer *pValReturn,
                                                 unsigned long *pcLenReturn,
                                                 int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    void *pvVBox;
    uint32_t cbVBox;
    char *pu8DestText = 0;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    XTextProperty property;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = VBoxX11ClipboardReadVBoxData(pCtx->pFrontend, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT, &pvVBox, &cbVBox);
    if ((rc != VINF_SUCCESS) || (cbVBox == 0))
    {
        /* If VBoxX11ClipboardReadVBoxData fails then pClient may be invalid */
        LogRelFunc (("VBoxX11ClipboardReadVBoxData returned %Rrc%s\n", rc,
                      RT_SUCCESS(rc) ? ", cbVBox == 0" :  ""));
        RTMemFree(pvVBox);
        return false;
    }
    pu16SrcText = reinterpret_cast<PRTUTF16>(pvVBox);
    cwSrcLen = cbVBox / 2;
    /* How long will the converted text be? */
    rc = vboxClipboardUtf16GetLinSize(pu16SrcText, cwSrcLen, &cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(pvVBox);
        AssertRCReturn(rc, false);
    }
    if (cwDestLen == 0)
    {
        LogFlowFunc(("received empty clipboard data from the guest, returning false.\n"));
        RTMemFree(pvVBox);
        return false;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        LogRelFunc (("failed to allocate %d bytes\n", cwDestLen * 2));
        RTMemFree(pvVBox);
        return false;
    }
    /* Convert the text. */
    rc = vboxClipboardUtf16WinToLin(pu16SrcText, cwSrcLen, pu16DestText, cwDestLen);
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16DestText));
        RTMemFree(pvVBox);
        return false;
    }
    /* Convert the Utf16 string to Utf8. */
    rc = RTUtf16ToUtf8Ex(pu16DestText + 1, cwDestLen - 1, &pu8DestText, 0, &cbDestLen);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    if (RT_FAILURE(rc))
    {
        LogRelFunc (("clipboard conversion failed.  RTUtf16ToUtf8Ex() returned %Rrc.  Abandoning.\n", rc));
        RTMemFree(pvVBox);
        return false;
    }
    /* And finally (!) convert the Utf8 text to compound text. */
#ifdef RT_OS_SOLARIS
    rc = XmbTextListToTextProperty(XtDisplay(pCtx->widget), &pu8DestText, 1,
                                     XCompoundTextStyle, &property);
#else
    rc = Xutf8TextListToTextProperty(XtDisplay(pCtx->widget), &pu8DestText, 1,
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
        RTMemFree(pvVBox);
        return false;
    }
    LogFlowFunc (("converted string is %s. Returning.\n", property.value));
    RTMemFree(pvVBox);
    *atomTypeReturn = property.encoding;
    *pValReturn = reinterpret_cast<XtPointer>(property.value);
    *pcLenReturn = property.nitems;
    *piFormatReturn = property.format;
    return true;
}

/**
 * Return VBox's clipboard data for an X11 client.
 * @note  X11 backend code, callback for XtOwnSelection
 */
static Boolean vboxClipboardConvertForX11(Widget, Atom *atomSelection,
                                          Atom *atomTarget,
                                          Atom *atomTypeReturn,
                                          XtPointer *pValReturn,
                                          unsigned long *pcLenReturn,
                                          int *piFormatReturn)
{
    g_eClipboardFormats eFormat = INVALID;
    /** @todo find a better way around the lack of user data. */
    VBOXCLIPBOARDCONTEXTX11 *pCtx = g_pCtx;

    LogFlowFunc(("\n"));
    /* Drop requests that we receive too late. */
    if (pCtx->eOwner != VB)
        return false;
    if (   (*atomSelection != pCtx->atomClipboard)
        && (*atomSelection != pCtx->atomPrimary)
       )
    {
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    if (g_debugClipboard)
    {
        char *szAtomName = XGetAtomName(XtDisplay(pCtx->widget), *atomTarget);
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
    if (*atomTarget == pCtx->atomTargets)
    {
        eFormat = TARGETS;
    }
    else
    {
        for (unsigned i = 0; i != pCtx->formatList.size(); ++i)
        {
            if (pCtx->formatList[i].atom == *atomTarget)
            {
                eFormat = pCtx->formatList[i].format;
                break;
            }
        }
    }
    switch (eFormat)
    {
    case TARGETS:
        return vboxClipboardConvertTargetsForX11(pCtx, atomTypeReturn,
                                                 pValReturn, pcLenReturn,
                                                 piFormatReturn);
    case UTF16:
        return vboxClipboardConvertUtf16(pCtx, atomTypeReturn, pValReturn,
                                         pcLenReturn, piFormatReturn);
    case UTF8:
        return vboxClipboardConvertToUtf8ForX11(pCtx, atomTypeReturn,
                                                pValReturn, pcLenReturn,
                                                piFormatReturn);
    case CTEXT:
        return vboxClipboardConvertToCTextForX11(pCtx, atomTypeReturn,
                                                 pValReturn, pcLenReturn,
                                                 piFormatReturn);
    default:
        LogFunc (("bad format\n"));
        return false;
    }
}

/**
 * This is called by the X toolkit intrinsics to let us know that another
 * X11 client has taken the clipboard.  In this case we notify VBox that
 * we want ownership of the clipboard.
 * @note  X11 backend code, callback for XtOwnSelection
 */
static void vboxClipboardReturnToX11(Widget, Atom *)
{
    /** @todo find a better way around the lack of user data */
    VBOXCLIPBOARDCONTEXTX11 *pCtx = g_pCtx;
    LogFlowFunc (("called, giving VBox clipboard ownership\n"));
    pCtx->eOwner = X11;
    pCtx->notifyVBox = true;
}

/**
 * VBox is taking possession of the shared clipboard.
 *
 * @param u32Formats Clipboard formats the guest is offering
 * @note  X11 backend code
 */
void VBoxX11ClipboardAnnounceVBoxFormat(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                        uint32_t u32Formats)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;

    pCtx->vboxFormats = u32Formats;
    LogFlowFunc (("u32Formats=%d\n", u32Formats));
    if (u32Formats == 0)
    {
        /* This is just an automatism, not a genuine anouncement */
        LogFlowFunc(("returning\n"));
        return;
    }
    if (pCtx->eOwner == VB)
    {
        /* We already own the clipboard, so no need to grab it, especially as that can lead
           to races due to the asynchronous nature of the X11 clipboard.  This event may also
           have been sent out by the guest to invalidate the Windows clipboard cache. */
        LogFlowFunc(("returning\n"));
        return;
    }
    Log2 (("%s: giving the guest clipboard ownership\n", __PRETTY_FUNCTION__));
    pCtx->eOwner = VB;
    pCtx->X11TextFormat = INVALID;
    pCtx->X11BitmapFormat = INVALID;
    if (XtOwnSelection(pCtx->widget, pCtx->atomClipboard, CurrentTime,
                       vboxClipboardConvertForX11, vboxClipboardReturnToX11,
                       0) != True)
    {
        Log2 (("%s: returning clipboard ownership to the host\n", __PRETTY_FUNCTION__));
        /* We set this so that the guest gets notified when we take the clipboard, even if no
          guest formats are found which we understand. */
        pCtx->notifyVBox = true;
        pCtx->eOwner = X11;
    }
    XtOwnSelection(pCtx->widget, pCtx->atomPrimary, CurrentTime, vboxClipboardConvertForX11,
                   NULL, 0);
    LogFlowFunc(("returning\n"));

}

/**
 * Called when VBox wants to read the X11 clipboard.
 *
 * @param  pClient   Context information about the guest VM
 * @param  u32Format The format that the guest would like to receive the data in
 * @param  pv        Where to write the data to
 * @param  cb        The size of the buffer to write the data to
 * @param  pcbActual Where to write the actual size of the written data
 * @note   X11 backend code
 */
int VBoxX11ClipboardReadX11Data(VBOXCLIPBOARDCONTEXTX11 *pCtx,
                                uint32_t u32Format,
                                VBOXCLIPBOARDREQUEST *pRequest)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
    {
        /* no data available */
        *pRequest->pcbActual = 0;
        return VINF_SUCCESS;
    }

    LogFlowFunc (("u32Format = %d, cb = %d\n", u32Format, pRequest->cb));

    /*
     * The guest wants to read data in the given format.
     */
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        if (pCtx->X11TextFormat == INVALID)
        {
            /* No data available. */
            *pRequest->pcbActual = 0;
            return VERR_NO_DATA;  /* The guest thinks we have data and we don't */
        }
        /* Initially set the size of the data read to zero in case we fail
         * somewhere. */
        *pRequest->pcbActual = 0;
        /* Send out a request for the data to the current clipboard owner */
        XtGetSelectionValue(pCtx->widget, pCtx->atomClipboard,
                            pCtx->atomX11TextFormat,
                            vboxClipboardGetDataFromX11,
                            reinterpret_cast<XtPointer>(pRequest),
                            CurrentTime);
        /* When the data arrives, the vboxClipboardGetDataFromX11 callback will be called.  The
           callback will signal the event semaphore when it has processed the data for us. */

        int rc = RTSemEventWait(pCtx->waitForData, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        return VERR_NOT_IMPLEMENTED;
    }
    return VINF_SUCCESS;
}

