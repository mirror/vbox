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

/* Note: to automatically run regression tests on the shared clipboard, set
 * the make variable VBOX_RUN_X11_CLIPBOARD_TEST=1 while building.  If you
 * often make changes to the clipboard code, setting this variable in
 * LocalConfig.kmk will cause the tests to be run every time the code is
 * changed. */

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD

#include <errno.h>

#include <unistd.h>

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

/** The different clipboard formats which we support. */
enum CLIPFORMAT
{
    INVALID = 0,
    TARGETS,
    CTEXT,
    UTF8
};

/** The table mapping X11 names to data formats and to the corresponding
 * VBox clipboard formats (currently only Unicode) */
static struct _CLIPFORMATTABLE
{
    /** The X11 atom name of the format (several names can match one format)
     */
    const char *pcszAtom;
    /** The format corresponding to the name */
    CLIPFORMAT  enmFormat;
    /** The corresponding VBox clipboard format */
    uint32_t    u32VBoxFormat;
} g_aFormats[] =
{
    { "UTF8_STRING", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT },
    { "text/plain;charset=UTF-8", UTF8,
      VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT },
    { "text/plain;charset=utf-8", UTF8,
      VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT },
    { "STRING", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT },
    { "TEXT", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT },
    { "text/plain", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT },
    { "COMPOUND_TEXT", CTEXT, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT }
};

/** Global context information used by the X11 clipboard backend */
struct _CLIPBACKEND
{
    /** Opaque data structure describing the front-end. */
    VBOXCLIPBOARDCONTEXT *pFrontend;
    /** The X Toolkit application context structure */
    XtAppContext appContext;

    /** We have a separate thread to wait for Window and Clipboard events */
    RTTHREAD thread;
    /** The X Toolkit widget which we use as our clipboard client.  It is never made visible. */
    Widget widget;

    /** Does VBox currently own the clipboard?  If so, we don't need to poll
     * X11 for supported formats. */
    bool fOwnsClipboard;

    /** What is the best text format X11 has to offer?  INVALID for none. */
    CLIPFORMAT X11TextFormat;
    /** Atom corresponding to the X11 text format */
    Atom atomX11TextFormat;
    /** What is the best bitmap format X11 has to offer?  INVALID for none.
     */
    CLIPFORMAT X11BitmapFormat;
    /** Atom corresponding to the X11 Bitmap format */
    Atom atomX11BitmapFormat;
    /** What formats does VBox have on offer? */
    uint32_t vboxFormats;
    /** Windows hosts and guests cache the clipboard data they receive.
     * Since we have no way of knowing whether their cache is still valid,
     * we always send a "data changed" message after a successful transfer
     * to invalidate it. */
    bool notifyVBox;
    /** Cache of the last unicode data that we received */
    void *pvUnicodeCache;
    /** Size of the unicode data in the cache */
    uint32_t cbUnicodeCache;
    /** When we wish the clipboard to exit, we have to wake up the event
     * loop.  We do this by writing into a pipe.  This end of the pipe is
     * the end that another thread can write to. */
    int wakeupPipeWrite;
    /** The reader end of the pipe */
    int wakeupPipeRead;
};

/** The number of simultaneous instances we support.  For all normal purposes
 * we should never need more than one.  For the testcase it is convenient to
 * have a second instance that the first can interact with in order to have
 * a more controlled environment. */
enum { CLIPBOARD_NUM_CONTEXTS = 20 };

/** Array of structures for mapping Xt widgets to context pointers.  We
 * need this because the widget clipboard callbacks do not pass user data. */
static struct {
    /** The widget we want to associate the context with */
    Widget widget;
    /** The context associated with the widget */
    CLIPBACKEND *pCtx;
} g_contexts[CLIPBOARD_NUM_CONTEXTS];

/** Register a new X11 clipboard context. */
static int vboxClipboardAddContext(CLIPBACKEND *pCtx)
{
    bool found = false;
    AssertReturn(pCtx != NULL, VERR_INVALID_PARAMETER);
    Widget widget = pCtx->widget;
    AssertReturn(widget != NULL, VERR_INVALID_PARAMETER);
    for (unsigned i = 0; i < RT_ELEMENTS(g_contexts); ++i)
    {
        AssertReturn(   (g_contexts[i].widget != widget)
                     && (g_contexts[i].pCtx != pCtx), VERR_WRONG_ORDER);
        if (g_contexts[i].widget == NULL && !found)
        {
            AssertReturn(g_contexts[i].pCtx == NULL, VERR_INTERNAL_ERROR);
            g_contexts[i].widget = widget;
            g_contexts[i].pCtx = pCtx;
            found = true;
        }
    }
    return found ? VINF_SUCCESS : VERR_OUT_OF_RESOURCES;
}

/** Unregister an X11 clipboard context. */
static void vboxClipboardRemoveContext(CLIPBACKEND *pCtx)
{
    bool found = false;
    AssertReturnVoid(pCtx != NULL);
    Widget widget = pCtx->widget;
    AssertReturnVoid(widget != NULL);
    for (unsigned i = 0; i < RT_ELEMENTS(g_contexts); ++i)
    {
        Assert(!found || g_contexts[i].widget != widget);
        if (g_contexts[i].widget == widget)
        {
            Assert(g_contexts[i].pCtx != NULL);
            g_contexts[i].widget = NULL;
            g_contexts[i].pCtx = NULL;
            found = true;
        }
    }
}

/** Find an X11 clipboard context. */
static CLIPBACKEND *vboxClipboardFindContext(Widget widget)
{
    AssertReturn(widget != NULL, NULL);
    for (unsigned i = 0; i < RT_ELEMENTS(g_contexts); ++i)
    {
        if (g_contexts[i].widget == widget)
        {
            Assert(g_contexts[i].pCtx != NULL);
            return g_contexts[i].pCtx;
        }
    }
    return NULL;
}

/** Convert an atom name string to an X11 atom, looking it up in a cache
 * before asking the server */
static Atom clipGetAtom(Widget widget, const char *pszName)
{
    AssertPtrReturn(pszName, None);
    Atom retval = None;
    XrmValue nameVal, atomVal;
    nameVal.addr = (char *) pszName;
    nameVal.size = strlen(pszName);
    atomVal.size = sizeof(Atom);
    atomVal.addr = (char *) &retval;
    XtConvertAndStore(widget, XtRString, &nameVal, XtRAtom, &atomVal);
    return retval;
}

/* Are we actually connected to the X server? */
static bool g_fHaveX11;

static int vboxClipboardWriteUtf16LE(CLIPBACKEND *pCtx,
                                     PRTUTF16 pu16SrcText,
                                     size_t cwSrcLen,
                                     void *pv, unsigned cb,
                                     uint32_t *pcbActual)
{
    size_t cwDestLen;
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    int rc = VINF_SUCCESS;
    /* Check how much longer will the converted text will be. */
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
    return rc;
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
static int vboxClipboardGetUtf8FromX11(CLIPBACKEND *pCtx,
                                       XtPointer pValue, unsigned cbSrcLen,
                                       void *pv, unsigned cb,
                                       uint32_t *pcbActual)
{
    size_t cwSrcLen;
    char *pu8SrcText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16SrcText = NULL;

    LogFlowFunc (("converting Utf-8 to Utf-16LE.  cbSrcLen=%d, cb=%d, pu8SrcText=%.*s\n",
                   cbSrcLen, cb, cbSrcLen, pu8SrcText));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    /* First convert the UTF8 to UTF16 */
    int rc = RTStrToUtf16Ex(pu8SrcText, cbSrcLen, &pu16SrcText, 0, &cwSrcLen);
    if (RT_SUCCESS(rc))
        rc = vboxClipboardWriteUtf16LE(pCtx, pu16SrcText, cwSrcLen + 1,
                                       pv, cb, pcbActual);
    XtFree(reinterpret_cast<char *>(pValue));
    RTUtf16Free(pu16SrcText);
    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
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
static int vboxClipboardGetCTextFromX11(CLIPBACKEND *pCtx,
                                        XtPointer pValue, unsigned cbSrcLen,
                                        void *pv, unsigned cb,
                                        uint32_t *pcbActual)
{
    size_t cwSrcLen;
    char **ppu8SrcText = NULL;
    PRTUTF16 pu16SrcText = NULL;
    XTextProperty property;
    int rc = VINF_SUCCESS;
    int cProps;

    LogFlowFunc (("converting COMPOUND TEXT to Utf-16LE.  cbSrcLen=%d, cb=%d, pu8SrcText=%.*s\n",
                   cbSrcLen, cb, cbSrcLen, reinterpret_cast<char *>(pValue)));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    /** @todo quick fix for 2.2, do this properly. */
    if (cbSrcLen == 0)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        if (cb < 2)
            return VERR_BUFFER_OVERFLOW;
        *(PRTUTF16) pv = 0;
        *pcbActual = 2;
        return VINF_SUCCESS;
    }
    /* First convert the compound text to Utf8 */
    property.value = reinterpret_cast<unsigned char *>(pValue);
    property.encoding = clipGetAtom(pCtx->widget, "COMPOUND_TEXT");
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
    if (RT_SUCCESS(rc))
        rc = vboxClipboardWriteUtf16LE(pCtx, pu16SrcText, cwSrcLen + 1,
                                       pv, cb, pcbActual);
    if (ppu8SrcText != NULL)
        XFreeStringList(ppu8SrcText);
    RTUtf16Free(pu16SrcText);
    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
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
static int vboxClipboardGetLatin1FromX11(CLIPBACKEND *pCtx,
                                         XtPointer pValue,
                                         unsigned cbSourceLen, void *pv,
                                         unsigned cb, uint32_t *pcbActual)
{
    char *pu8SourceText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16DestText = reinterpret_cast<PRTUTF16>(pv);
    int rc = VINF_SUCCESS;

    LogFlowFunc (("converting Latin1 to Utf-16LE.  Original is %.*s\n",
                  cbSourceLen, pu8SourceText));
    *pcbActual = 0;  /* Only set this to the right value on success. */
    unsigned cwDestLen = 0;
    for (unsigned i = 0; i < cbSourceLen && pu8SourceText[i] != '\0'; i++)
    {
        ++cwDestLen;
        if (pu8SourceText[i] == LINEFEED)
            ++cwDestLen;
    }
    /* Leave space for the terminator */
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
    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
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
    CLIPBACKEND *pCtx = pRequest->pCtx;
    if (pCtx->fOwnsClipboard == true)
    {
        /* We don't want to request data from ourselves! */
        pRequest->rc = VERR_NO_DATA;
        RTSemEventSignal(pRequest->finished);
        return;
    }
    pRequest->rc = VINF_SUCCESS;
    LogFlowFunc(("pClientData=%p, *pcLen=%lu, *piFormat=%d\n", pClientData,
                 *pcLen, *piFormat));
    LogFlowFunc(("pCtx->X11TextFormat=%d, pRequest->cb=%d\n",
                 pCtx->X11TextFormat, pRequest->cb));
    unsigned cTextLen = (*pcLen) * (*piFormat) / 8;
    /* The X Toolkit may have failed to get the clipboard selection for us. */
    if (*atomType == XT_CONVERT_FAIL) /* timeout */
    {
        pRequest->rc = VERR_TIMEOUT;
        RTSemEventSignal(pRequest->finished);
        return;
    }
    /* The clipboard selection may have changed before we could get it. */
    if (NULL == pValue)
    {
        pRequest->rc = VERR_NO_DATA;
        RTSemEventSignal(pRequest->finished);
        return;
    }
    /* In which format is the clipboard data? */
    switch (pCtx->X11TextFormat)
    {
    case CTEXT:
        pRequest->rc = vboxClipboardGetCTextFromX11(pCtx, pValue, cTextLen,
                                                    pRequest->pv,
                                                    pRequest->cb,
                                                    pRequest->pcbActual);
        RTSemEventSignal(pRequest->finished);
        break;
    case UTF8:
    {
        /* If we are given broken Utf-8, we treat it as Latin1.  Is this acceptable? */
        size_t cStringLen;
        char *pu8SourceText = reinterpret_cast<char *>(pValue);

        if ((pCtx->X11TextFormat == UTF8)
            && (RTStrUniLenEx(pu8SourceText, *pcLen, &cStringLen) == VINF_SUCCESS))
        {
            pRequest->rc = vboxClipboardGetUtf8FromX11(pCtx, pValue,
                                                       cTextLen,
                                                       pRequest->pv,
                                                       pRequest->cb,
                                                       pRequest->pcbActual);
            RTSemEventSignal(pRequest->finished);
            break;
        }
        else
        {
            pRequest->rc = vboxClipboardGetLatin1FromX11(pCtx, pValue,
                                                         cTextLen,
                                                         pRequest->pv,
                                                         pRequest->cb,
                                                        pRequest->pcbActual);
            RTSemEventSignal(pRequest->finished);
            break;
        }
    }
    default:
        LogFunc (("bad target format\n"));
        XtFree(reinterpret_cast<char *>(pValue));
        pRequest->rc = VERR_INVALID_PARAMETER;
        RTSemEventSignal(pRequest->finished);
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
    CLIPBACKEND *pCtx =
            reinterpret_cast<CLIPBACKEND *>(pClientData);
    Atom *atomTargets = reinterpret_cast<Atom *>(pValue);
    unsigned cAtoms = *pcLen;
    CLIPFORMAT enmBestTarget = INVALID;
    Atom atomBestTarget = None;

    Log3 (("%s: called\n", __PRETTY_FUNCTION__));
    if (   (*atomType == XT_CONVERT_FAIL) /* timeout */
        || (pCtx->fOwnsClipboard == true) /* VBox currently owns the
                                           * clipboard */
       )
    {
        pCtx->atomX11TextFormat = None;
        pCtx->X11TextFormat = INVALID;
        return;
    }

    for (unsigned i = 0; i < cAtoms; ++i)
    {
        for (unsigned j = 0; j < RT_ELEMENTS(g_aFormats); ++j)
        {
            Atom formatAtom = clipGetAtom(pCtx->widget,
                                          g_aFormats[j].pcszAtom);
            if (atomTargets[i] == formatAtom)
            {
                if (   enmBestTarget < g_aFormats[j].enmFormat)
                {
                    enmBestTarget = g_aFormats[j].enmFormat;
                    atomBestTarget = formatAtom;
                }
                break;
            }
        }
    }
    pCtx->atomX11TextFormat = atomBestTarget;
    if ((enmBestTarget != pCtx->X11TextFormat) || (pCtx->notifyVBox == true))
    {
        uint32_t u32Formats = 0;
        pCtx->X11TextFormat = enmBestTarget;
        if (enmBestTarget != INVALID)
            u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        VBoxX11ClipboardReportX11Formats(pCtx->pFrontend, u32Formats);
        pCtx->notifyVBox = false;
    }
    XtFree(reinterpret_cast<char *>(pValue));
}

enum { TIMER_FREQ = 200 /* ms */ };

static void vboxClipboardPollX11ForTargets(XtPointer pUserData,
                                           XtIntervalId * /* hTimerId */);
static void clipSchedulePoller(CLIPBACKEND *pCtx,
                               XtTimerCallbackProc proc);

#ifndef TESTCASE
void clipSchedulePoller(CLIPBACKEND *pCtx,
                        XtTimerCallbackProc proc)
{
    XtAppAddTimeOut(pCtx->appContext, TIMER_FREQ, proc, pCtx);
}
#endif

/**
 * This timer callback is called every 200ms to check the contents of the X11
 * clipboard.
 * @note  X11 backend code, callback for XtAppAddTimeOut, recursively
 *        re-armed.
 * @todo  Use the XFIXES extension to check for new clipboard data when
 *        available.
 */
void vboxClipboardPollX11ForTargets(XtPointer pUserData,
                                    XtIntervalId * /* hTimerId */)
{
    CLIPBACKEND *pCtx =
            reinterpret_cast<CLIPBACKEND *>(pUserData);
    Log3 (("%s: called\n", __PRETTY_FUNCTION__));
    /* Get the current clipboard contents if we don't own it ourselves */
    if (!pCtx->fOwnsClipboard)
    {
        Log3 (("%s: requesting the targets that the host clipboard offers\n",
               __PRETTY_FUNCTION__));
        XtGetSelectionValue(pCtx->widget, clipGetAtom(pCtx->widget, "CLIPBOARD"),
                            clipGetAtom(pCtx->widget, "TARGETS"),
                            vboxClipboardGetTargetsFromX11, pCtx,
                            CurrentTime);
    }
    /* Re-arm our timer */
    clipSchedulePoller(pCtx, vboxClipboardPollX11ForTargets);
}

#ifndef TESTCASE
/**
 * The main loop of our clipboard reader.
 * @note  X11 backend code.
 */
static int vboxClipboardThread(RTTHREAD self, void *pvUser)
{
    LogRel(("Shared clipboard: starting host clipboard thread\n"));

    CLIPBACKEND *pCtx =
            reinterpret_cast<CLIPBACKEND *>(pvUser);
    while (XtAppGetExitFlag(pCtx->appContext) == FALSE)
        XtAppProcessEvent(pCtx->appContext, XtIMAll);
    LogRel(("Shared clipboard: host clipboard thread terminated successfully\n"));
    return VINF_SUCCESS;
}
#endif

/** X11 specific uninitialisation for the shared clipboard.
 * @note  X11 backend code.
 */
static void vboxClipboardUninitX11(CLIPBACKEND *pCtx)
{
    AssertPtrReturnVoid(pCtx);
    if (pCtx->widget)
    {
        /* Valid widget + invalid appcontext = bug.  But don't return yet. */
        AssertPtr(pCtx->appContext);
        vboxClipboardRemoveContext(pCtx);
        XtDestroyWidget(pCtx->widget);
    }
    pCtx->widget = NULL;
    if (pCtx->appContext)
        XtDestroyApplicationContext(pCtx->appContext);
    pCtx->appContext = NULL;
    if (pCtx->wakeupPipeRead != 0)
        close(pCtx->wakeupPipeRead);
    if (pCtx->wakeupPipeWrite != 0)
        close(pCtx->wakeupPipeWrite);
    pCtx->wakeupPipeRead = 0;
    pCtx->wakeupPipeWrite = 0;
}

/** Worker function for stopping the clipboard which runs on the event
 * thread. */
static void vboxClipboardStopWorker(XtPointer pUserData, int * /* source */,
                                    XtInputId * /* id */)
{
    
    CLIPBACKEND *pCtx = (CLIPBACKEND *)pUserData;

    /* This might mean that we are getting stopped twice. */
    Assert(pCtx->widget != NULL);

    /* Set the termination flag to tell the Xt event loop to exit.  We
     * reiterate that any outstanding requests from the X11 event loop to
     * the VBox part *must* have returned before we do this. */
    XtAppSetExitFlag(pCtx->appContext);
    pCtx->fOwnsClipboard = false;
    pCtx->X11TextFormat = INVALID;
    pCtx->X11BitmapFormat = INVALID;
}

/** X11 specific initialisation for the shared clipboard.
 * @note  X11 backend code.
 */
static int vboxClipboardInitX11 (CLIPBACKEND *pCtx)
{
    /* Create a window and make it a clipboard viewer. */
    int cArgc = 0;
    char *pcArgv = 0;
    int rc = VINF_SUCCESS;
    Display *pDisplay;

    /* Make sure we are thread safe */
    XtToolkitThreadInitialize();
    /* Set up the Clipbard application context and main window.  We call all these functions
       directly instead of calling XtOpenApplication() so that we can fail gracefully if we
       can't get an X11 display. */
    XtToolkitInitialize();
    pCtx->appContext = XtCreateApplicationContext();
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
        else
            rc = vboxClipboardAddContext(pCtx);
    }
    if (RT_SUCCESS(rc))
    {
        XtSetMappedWhenManaged(pCtx->widget, false);
        XtRealizeWidget(pCtx->widget);
        /* Set up a timer to poll the host clipboard */
        clipSchedulePoller(pCtx, vboxClipboardPollX11ForTargets);
    }
    /* Create the pipes */
    int pipes[2];
    if (!pipe(pipes))
    {
        pCtx->wakeupPipeRead = pipes[0];
        pCtx->wakeupPipeWrite = pipes[1];
        if (!XtAppAddInput(pCtx->appContext, pCtx->wakeupPipeRead,
                           (XtPointer) XtInputReadMask,
                           vboxClipboardStopWorker, (XtPointer) pCtx))
            rc = VERR_NO_MEMORY;  /* What failure means is not doc'ed. */
    }
    else
        rc = RTErrConvertFromErrno(errno);
    if (RT_FAILURE(rc))
        vboxClipboardUninitX11(pCtx);
    return rc;
}

/**
 * Construct the X11 backend of the shared clipboard.
 * @note  X11 backend code
 */
CLIPBACKEND *VBoxX11ClipboardConstructX11
                                 (VBOXCLIPBOARDCONTEXT *pFrontend)
{
    int rc;

    CLIPBACKEND *pCtx = (CLIPBACKEND *)
                    RTMemAllocZ(sizeof(CLIPBACKEND));
    if (pCtx && !RTEnvGet("DISPLAY"))
    {
        /*
         * If we don't find the DISPLAY environment variable we assume that
         * we are not connected to an X11 server. Don't actually try to do
         * this then, just fail silently and report success on every call.
         * This is important for VBoxHeadless.
         */
        LogRelFunc(("X11 DISPLAY variable not set -- disabling shared clipboard\n"));
        g_fHaveX11 = false;
        return pCtx;
    }

    g_fHaveX11 = true;

    LogRel(("Initializing X11 clipboard backend\n"));
    if (pCtx)
        pCtx->pFrontend = pFrontend;
    return pCtx;
}

/**
 * Destruct the shared clipboard X11 backend.
 * @note  X11 backend code
 */
void VBoxX11ClipboardDestructX11(CLIPBACKEND *pCtx)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;

    /* We set this to NULL when the event thread exits.  It really should
     * have exited at this point, when we are about to unload the code from
     * memory. */
    Assert(pCtx->widget == NULL);
}

/**
 * Announce to the X11 backend that we are ready to start.
 */
int VBoxX11ClipboardStartX11(CLIPBACKEND *pCtx)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("\n"));
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    rc = vboxClipboardInitX11(pCtx);
#ifndef TESTCASE
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadCreate(&pCtx->thread, vboxClipboardThread, pCtx, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
        if (RT_FAILURE(rc))
            LogRel(("Failed to initialise the shared clipboard X11 backend.\n"));
    }
#endif
    if (RT_SUCCESS(rc))
    {
        pCtx->fOwnsClipboard = false;
        pCtx->notifyVBox = true;
    }
    return rc;
}

/** String written to the wakeup pipe. */
#define WAKE_UP_STRING      "WakeUp!"
/** Length of the string written. */
#define WAKE_UP_STRING_LEN  ( sizeof(WAKE_UP_STRING) - 1 )

/**
 * Shut down the shared clipboard X11 backend.
 * @note  X11 backend code
 * @note  Any requests from this object to get clipboard data from VBox
 *        *must* have completed or aborted before we are called, as
 *        otherwise the X11 event loop will still be waiting for the request
 *        to return and will not be able to terminate.
 */
int VBoxX11ClipboardStopX11(CLIPBACKEND *pCtx)
{
    int rc, rcThread;
    unsigned count = 0;
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    LogRelFunc(("stopping the shared clipboard X11 backend\n"));
    /* Write to the "stop" pipe */
    rc = write(pCtx->wakeupPipeWrite, WAKE_UP_STRING, WAKE_UP_STRING_LEN);
    do
    {
        rc = RTThreadWait(pCtx->thread, 1000, &rcThread);
        ++count;
        Assert(RT_SUCCESS(rc) || ((VERR_TIMEOUT == rc) && (count != 5)));
    } while ((VERR_TIMEOUT == rc) && (count < 300));
    if (RT_SUCCESS(rc))
        AssertRC(rcThread);
    else
        LogRelFunc(("rc=%Rrc\n", rc));
    vboxClipboardUninitX11(pCtx);
    LogFlowFunc(("returning %Rrc.\n", rc));
    return rc;
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
static Boolean vboxClipboardConvertTargetsForX11(CLIPBACKEND
                                                                      *pCtx,
                                                 Atom *atomTypeReturn,
                                                 XtPointer *pValReturn,
                                                 unsigned long *pcLenReturn,
                                                 int *piFormatReturn)
{
    unsigned uListSize = RT_ELEMENTS(g_aFormats);
    Atom *atomTargets = reinterpret_cast<Atom *>(XtMalloc((uListSize + 3) * sizeof(Atom)));
    unsigned cTargets = 0;

    LogFlowFunc (("called\n"));
    for (unsigned i = 0; i < uListSize; ++i)
    {
        if (   (pCtx->vboxFormats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
            && (   g_aFormats[i].u32VBoxFormat
                == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT))
        {
            atomTargets[cTargets] = clipGetAtom(pCtx->widget,
                                                g_aFormats[i].pcszAtom);
            ++cTargets;
        }
    }
    atomTargets[cTargets] = clipGetAtom(pCtx->widget, "TARGETS");
    atomTargets[cTargets + 1] = clipGetAtom(pCtx->widget, "MULTIPLE");
    atomTargets[cTargets + 2] = clipGetAtom(pCtx->widget, "TIMESTAMP");
    *atomTypeReturn = XA_ATOM;
    *pValReturn = reinterpret_cast<XtPointer>(atomTargets);
    *pcLenReturn = cTargets + 3;
    *piFormatReturn = 32;
    return true;
}

/** This is a wrapper around VBoxX11ClipboardReadVBoxData that will cache the
 * data returned.  This is unfortunately necessary, because if the other side
 * of the shared clipboard is also an X11 system, it may send a format
 * announcement message every time its clipboard is read, for reasons that
 * are explained elsewhere.  Unfortunately, some applications on our side
 * like to read the clipboard several times in short succession in different
 * formats.  This can fail if it collides with a format announcement message.
 * @todo any ideas about how to do this better are welcome.
 */
static int vboxClipboardReadVBoxData (CLIPBACKEND *pCtx,
                               uint32_t u32Format, void **ppv,
                               uint32_t *pcb)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pCtx=%p, u32Format=%02X, ppv=%p, pcb=%p\n", pCtx,
                 u32Format, ppv, pcb));
    if (u32Format == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        if (pCtx->pvUnicodeCache == NULL)
            rc = VBoxX11ClipboardReadVBoxData(pCtx->pFrontend, u32Format,
                                              &pCtx->pvUnicodeCache,
                                              &pCtx->cbUnicodeCache);
        if (RT_SUCCESS(rc))
        {
            *ppv = RTMemDup(pCtx->pvUnicodeCache, pCtx->cbUnicodeCache);
            *pcb = pCtx->cbUnicodeCache;
            if (*ppv == NULL)
                rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VBoxX11ClipboardReadVBoxData(pCtx->pFrontend, u32Format,
                                          ppv, pcb);
    LogFlowFunc(("returning %Rrc\n", rc));
    if (RT_SUCCESS(rc))
        LogFlowFunc(("*ppv=%.*ls, *pcb=%u\n", *pcb, *ppv, *pcb));
    return rc;
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
static Boolean vboxClipboardConvertToUtf8ForX11(CLIPBACKEND
                                                                      *pCtx,
                                                Atom *atomTarget,
                                                Atom *atomTypeReturn,
                                                XtPointer *pValReturn,
                                                unsigned long *pcLenReturn,
                                                int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    char *pu8DestText;
    void *pvVBox = NULL;
    uint32_t cbVBox = 0;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = vboxClipboardReadVBoxData(pCtx,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   &pvVBox, &cbVBox);
    if ((rc != VINF_SUCCESS) || (cbVBox == 0))
    {
        /* If vboxClipboardReadVBoxData fails then we may be terminating */
        LogRelFunc (("vboxClipboardReadVBoxData returned %Rrc%s\n", rc,
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
    *atomTypeReturn = *atomTarget;
    *pValReturn = reinterpret_cast<XtPointer>(pu8DestText);
    *pcLenReturn = cbDestLen + 1;
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
static Boolean vboxClipboardConvertToCTextForX11(CLIPBACKEND
                                                                      *pCtx,
                                                 Atom *atomTypeReturn,
                                                 XtPointer *pValReturn,
                                                 unsigned long *pcLenReturn,
                                                 int *piFormatReturn)
{
    PRTUTF16 pu16SrcText, pu16DestText;
    void *pvVBox = NULL;
    uint32_t cbVBox = 0;
    char *pu8DestText = 0;
    size_t cwSrcLen, cwDestLen, cbDestLen;
    XTextProperty property;
    int rc;

    LogFlowFunc (("called\n"));
    /* Read the clipboard data from the guest. */
    rc = vboxClipboardReadVBoxData(pCtx,
                                   VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   &pvVBox, &cbVBox);
    if ((rc != VINF_SUCCESS) || (cbVBox == 0))
    {
        /* If vboxClipboardReadVBoxData fails then we may be terminating */
        LogRelFunc (("vboxClipboardReadVBoxData returned %Rrc%s\n", rc,
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
    *pcLenReturn = property.nitems + 1;
    *piFormatReturn = property.format;
    return true;
}

/**
 * Return VBox's clipboard data for an X11 client.
 * @note  X11 backend code, callback for XtOwnSelection
 */
static Boolean vboxClipboardConvertForX11(Widget widget, Atom *atomSelection,
                                          Atom *atomTarget,
                                          Atom *atomTypeReturn,
                                          XtPointer *pValReturn,
                                          unsigned long *pcLenReturn,
                                          int *piFormatReturn)
{
    CLIPFORMAT enmFormat = INVALID;
    CLIPBACKEND *pCtx = vboxClipboardFindContext(widget);

    LogFlowFunc(("\n"));
    /* Drop requests that we receive too late. */
    if (!pCtx->fOwnsClipboard)
        return false;
    if (   (*atomSelection != clipGetAtom(pCtx->widget, "CLIPBOARD"))
        && (*atomSelection != clipGetAtom(pCtx->widget, "PRIMARY"))
       )
    {
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    if (*atomTarget == clipGetAtom(pCtx->widget, "TARGETS"))
    {
        enmFormat = TARGETS;
    }
    else
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aFormats); ++i)
        {
            if (*atomTarget == clipGetAtom(pCtx->widget,
                                           g_aFormats[i].pcszAtom))
            {
                enmFormat = g_aFormats[i].enmFormat;
                break;
            }
        }
    }
    switch (enmFormat)
    {
    case TARGETS:
        return vboxClipboardConvertTargetsForX11(pCtx, atomTypeReturn,
                                                 pValReturn, pcLenReturn,
                                                 piFormatReturn);
    case UTF8:
        return vboxClipboardConvertToUtf8ForX11(pCtx, atomTarget,
                                                atomTypeReturn,
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
static void vboxClipboardReturnToX11(Widget widget, Atom *)
{
    CLIPBACKEND *pCtx = vboxClipboardFindContext(widget);
    LogFlowFunc (("called, giving X11 clipboard ownership\n"));
    /* These should be set to the right values as soon as we start polling */
    pCtx->X11TextFormat = INVALID;
    pCtx->X11BitmapFormat = INVALID;
    pCtx->fOwnsClipboard = false;
    pCtx->notifyVBox = true;
}

static void clipSchedule(XtAppContext app_context, XtTimerCallbackProc proc,
                         XtPointer client_data);
#ifndef TESTCASE
void clipSchedule(XtAppContext app_context, XtTimerCallbackProc proc,
                  XtPointer client_data)
{
    XtAppAddTimeOut(app_context, 0, proc, client_data);
}
#endif

/** Structure used to pass information about formats that VBox supports */
typedef struct _VBOXCLIPBOARDFORMATS
{
    /** Context information for the X11 clipboard */
    CLIPBACKEND *pCtx;
    /** Formats supported by VBox */
    uint32_t formats;
} VBOXCLIPBOARDFORMATS;

/** Worker function for VBoxX11ClipboardAnnounceVBoxFormat which runs on the
 * event thread. */
static void vboxClipboardAnnounceWorker(XtPointer pUserData,
                                        XtIntervalId * /* interval */)
{
    /* Extract and free the user data */
    VBOXCLIPBOARDFORMATS *pFormats = (VBOXCLIPBOARDFORMATS *)pUserData;
    CLIPBACKEND *pCtx = pFormats->pCtx;
    uint32_t u32Formats = pFormats->formats;
    RTMemFree(pFormats);
    LogFlowFunc (("u32Formats=%d\n", u32Formats));
    pCtx->vboxFormats = u32Formats;
    /* Invalidate the clipboard cache */
    if (pCtx->pvUnicodeCache != NULL)
    {
        RTMemFree(pCtx->pvUnicodeCache);
        pCtx->pvUnicodeCache = NULL;
    }
    if (u32Formats == 0)
    {
        /* This is just an automatism, not a genuine anouncement */
        XtDisownSelection(pCtx->widget, clipGetAtom(pCtx->widget, "CLIPBOARD"), CurrentTime);
        pCtx->fOwnsClipboard = false;
        LogFlowFunc(("returning\n"));
        return;
    }
    Log2 (("%s: giving the guest clipboard ownership\n", __PRETTY_FUNCTION__));
    if (XtOwnSelection(pCtx->widget, clipGetAtom(pCtx->widget, "CLIPBOARD"), CurrentTime,
                       vboxClipboardConvertForX11, vboxClipboardReturnToX11,
                       0) == True)
    {
        pCtx->fOwnsClipboard = true;
        /* Grab the middle-button paste selection too. */
        XtOwnSelection(pCtx->widget, clipGetAtom(pCtx->widget, "PRIMARY"), CurrentTime,
                       vboxClipboardConvertForX11, NULL, 0);
    }
    else
    {
        /* Another X11 client claimed the clipboard just after us, so let it
         * go again. */
        Log2 (("%s: returning clipboard ownership to the X11\n",
               __PRETTY_FUNCTION__));
        /* VBox thinks it currently owns the clipboard, so we must notify it
         * as soon as we know what formats X11 has to offer. */
        pCtx->notifyVBox = true;
        pCtx->fOwnsClipboard = false;
    }
    LogFlowFunc(("returning\n"));
}

/**
 * VBox is taking possession of the shared clipboard.
 *
 * @param u32Formats Clipboard formats the guest is offering
 * @note  X11 backend code
 */
void VBoxX11ClipboardAnnounceVBoxFormat(CLIPBACKEND *pCtx,
                                        uint32_t u32Formats)
{
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return;
    /* This must be freed by the worker callback */
    VBOXCLIPBOARDFORMATS *pFormats =
        (VBOXCLIPBOARDFORMATS *) RTMemAlloc(sizeof(VBOXCLIPBOARDFORMATS));
    if (pFormats != NULL)  /* if it is we will soon have other problems */
    {
        pFormats->pCtx = pCtx;
        pFormats->formats = u32Formats;
        clipSchedule(pCtx->appContext, vboxClipboardAnnounceWorker,
                     (XtPointer) pFormats);
    }
}

/** Worker function for VBoxX11ClipboardReadX11Data which runs on the event
 * thread. */
static void vboxClipboardReadX11Worker(XtPointer pUserData,
                                       XtIntervalId * /* interval */)
{
    VBOXCLIPBOARDREQUEST *pRequest = (VBOXCLIPBOARDREQUEST *)pUserData;
    CLIPBACKEND *pCtx = pRequest->pCtx;
    LogFlowFunc (("u32Format = %d, cb = %d\n", pRequest->format,
                  pRequest->cb));

    int rc = VINF_SUCCESS;
    /* Set this to start with, just in case */
    *pRequest->pcbActual = 0;
    /* Do not continue if we already own the clipboard */
    if (pCtx->fOwnsClipboard == true)
        rc = VERR_NO_DATA;  /** @todo should this be VINF? */
    else
    {
        /*
         * VBox wants to read data in the given format.
         */
        if (pRequest->format == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
        {
            if (pCtx->atomX11TextFormat == None)
                /* VBox thinks we have data and we don't */
                rc = VERR_NO_DATA;
            else
                /* Send out a request for the data to the current clipboard
                 * owner */
                XtGetSelectionValue(pCtx->widget, clipGetAtom(pCtx->widget, "CLIPBOARD"),
                                    pCtx->atomX11TextFormat,
                                    vboxClipboardGetDataFromX11,
                                    reinterpret_cast<XtPointer>(pRequest),
                                    CurrentTime);
        }
        else
            rc = VERR_NOT_IMPLEMENTED;
    }
    if (RT_FAILURE(rc))
    {
        pRequest->rc = rc;
        /* The clipboard callback was never scheduled, so we must signal
         * that the request processing is finished ourselves. */
        RTSemEventSignal(pRequest->finished);
    }
    LogFlowFunc(("status %Rrc\n", rc));
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
int VBoxX11ClipboardReadX11Data(CLIPBACKEND *pCtx,
                                uint32_t u32Format, void *pv, uint32_t cb,
                                uint32_t *pcbActual)
{
    /* Initially set the size of the data read to zero in case we fail
     * somewhere or X11 is not available. */
    *pcbActual = 0;

    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    VBOXCLIPBOARDREQUEST request;
    request.rc = VERR_WRONG_ORDER;
    request.pv = pv;
    request.cb = cb;
    request.pcbActual = pcbActual;
    request.format = u32Format;
    request.pCtx = pCtx;
    /* The worker function will signal this when it has finished. */
    int rc = RTSemEventCreate(&request.finished);
    if (RT_SUCCESS(rc))
    {
        /* We use this to schedule a worker function on the event thread. */
        clipSchedule(pCtx->appContext, vboxClipboardReadX11Worker,
                     (XtPointer) &request);
        rc = RTSemEventWait(request.finished, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
            rc = request.rc;
        RTSemEventDestroy(request.finished);
    }
    return rc;
}

#ifdef TESTCASE

#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <poll.h>

#define TEST_NAME "tstClipboardX11"
#define TEST_WIDGET (Widget)0xffff

/* Our X11 clipboard target poller */
static XtTimerCallbackProc g_pfnPoller = NULL;
/* User data for the poller function. */
static XtPointer g_pPollerData = NULL;

/* For the testcase, we install the poller function in a global variable
 * which is called when the testcase updates the X11 targets. */
void clipSchedulePoller(CLIPBACKEND *pCtx,
                        XtTimerCallbackProc proc)
{
    g_pfnPoller = proc;
    g_pPollerData = (XtPointer)pCtx;
}

static bool clipPollTargets()
{
    if (!g_pfnPoller)
        return false;
    g_pfnPoller(g_pPollerData, NULL);
    return true;
}

/* For the purpose of the test case, we just execute the procedure to be
 * scheduled, as we are running single threaded. */
void clipSchedule(XtAppContext app_context, XtTimerCallbackProc proc,
                  XtPointer client_data)
{
    proc(client_data, NULL);
}

void XtFree(char *ptr)
{ RTMemFree((void *) ptr); }

/* The data in the simulated VBox clipboard */
static int g_vboxDataRC = VINF_SUCCESS;
static void *g_vboxDatapv = NULL;
static uint32_t g_vboxDatacb = 0;

/* Set empty data in the simulated VBox clipboard. */
static void clipEmptyVBox(CLIPBACKEND *pCtx, int retval)
{
    g_vboxDataRC = retval;
    g_vboxDatapv = NULL;
    g_vboxDatacb = 0;
    VBoxX11ClipboardAnnounceVBoxFormat(pCtx, 0);
}

/* Set the data in the simulated VBox clipboard. */
static int clipSetVBoxUtf16(CLIPBACKEND *pCtx, int retval,
                            const char *pcszData, size_t cb)
{
    PRTUTF16 pwszData = NULL;
    size_t cwData = 0;
    int rc = RTStrToUtf16Ex(pcszData, RTSTR_MAX, &pwszData, 0, &cwData);
    if (RT_FAILURE(rc))
        return rc;
    AssertReturn(cb <= cwData * 2 + 2, VERR_BUFFER_OVERFLOW);
    void *pv = RTMemDup(pwszData, cb);
    RTUtf16Free(pwszData);
    if (pv == NULL)
        return VERR_NO_MEMORY;
    if (g_vboxDatapv)
        RTMemFree(g_vboxDatapv);
    g_vboxDataRC = retval;
    g_vboxDatapv = pv;
    g_vboxDatacb = cb;
    VBoxX11ClipboardAnnounceVBoxFormat(pCtx,
                                       VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    return VINF_SUCCESS;
}

/* Return the data in the simulated VBox clipboard. */
int VBoxX11ClipboardReadVBoxData(VBOXCLIPBOARDCONTEXT *pCtx,
                                 uint32_t u32Format, void **ppv,
                                 uint32_t *pcb)
{
    *pcb = g_vboxDatacb;
    if (g_vboxDatapv != NULL)
    {
        void *pv = RTMemDup(g_vboxDatapv, g_vboxDatacb);
        *ppv = pv;
        return pv != NULL ? g_vboxDataRC : VERR_NO_MEMORY;
    }
    *ppv = NULL;
    return g_vboxDataRC;
}

Display *XtDisplay(Widget w)
{ return (Display *) 0xffff; }

int XmbTextListToTextProperty(Display *display, char **list, int count,
                              XICCEncodingStyle style,
                              XTextProperty *text_prop_return)
{
    /* We don't fully reimplement this API for obvious reasons. */
    AssertReturn(count == 1, XLocaleNotSupported);
    AssertReturn(style == XCompoundTextStyle, XLocaleNotSupported);
    /* We simplify the conversion by only accepting ASCII. */
    for (unsigned i = 0; (*list)[i] != 0; ++i)
        AssertReturn(((*list)[i] & 0x80) == 0, XLocaleNotSupported);
    text_prop_return->value =
            (unsigned char*)RTMemDup(*list, strlen(*list) + 1);
    text_prop_return->encoding = clipGetAtom(NULL, "COMPOUND_TEXT");
    text_prop_return->format = 8;
    text_prop_return->nitems = strlen(*list);
    return 0;
}

int Xutf8TextListToTextProperty(Display *display, char **list, int count,
                                XICCEncodingStyle style,
                                XTextProperty *text_prop_return)
{
    return XmbTextListToTextProperty(display, list, count, style,
                                     text_prop_return);
}

int XmbTextPropertyToTextList(Display *display,
                              const XTextProperty *text_prop,
                              char ***list_return, int *count_return)
{
    int rc = 0;
    if (text_prop->nitems == 0)
    {
        *list_return = NULL;
        *count_return = 0;
        return 0;
    }
    /* Only accept simple ASCII properties */
    for (unsigned i = 0; i < text_prop->nitems; ++i)
        AssertReturn(!(text_prop->value[i] & 0x80), XConverterNotFound);
    char **ppList = (char **)RTMemAlloc(sizeof(char *));
    char *pValue = (char *)RTMemDup(text_prop->value, text_prop->nitems + 1);
    if (ppList)
        *ppList = pValue;
    if (!ppList || !pValue)
    {
        RTMemFree(ppList);
        RTMemFree(pValue);
        rc = XNoMemory;
    }
    else
    {
        /* NULL-terminate the string */
        pValue[text_prop->nitems] = '\0';
        *count_return = 1;
        *list_return = ppList;
    }
    return rc;
}

int Xutf8TextPropertyToTextList(Display *display,
                                const XTextProperty *text_prop,
                                char ***list_return, int *count_return)
{
    return XmbTextPropertyToTextList(display, text_prop, list_return,
                                     count_return);
}

void XtAppSetExitFlag(XtAppContext app_context) {}

void XtDestroyWidget(Widget w) {}

XtAppContext XtCreateApplicationContext(void) { return (XtAppContext)0xffff; }

void XtDestroyApplicationContext(XtAppContext app_context) {}

void XtToolkitInitialize(void) {}

Boolean XtToolkitThreadInitialize(void) { return True; }

Display *XtOpenDisplay(XtAppContext app_context,
                       _Xconst _XtString display_string,
                       _Xconst _XtString application_name,
                       _Xconst _XtString application_class,
                       XrmOptionDescRec *options, Cardinal num_options,
                       int *argc, char **argv)
{ return (Display *)0xffff; }

Widget XtVaAppCreateShell(_Xconst _XtString application_name,
                          _Xconst _XtString application_class,
                          WidgetClass widget_class, Display *display, ...)
{ return TEST_WIDGET; }

void XtSetMappedWhenManaged(Widget widget, _XtBoolean mapped_when_managed) {}

void XtRealizeWidget(Widget widget) {}

XtInputId XtAppAddInput(XtAppContext app_context, int source,
                        XtPointer condition, XtInputCallbackProc proc,
                        XtPointer closure)
{ return 0xffff; }

/* Atoms we need other than the formats we support. */
static const char *g_apszSupAtoms[] =
{
    "PRIMARY", "CLIPBOARD", "TARGETS", "MULTIPLE", "TIMESTAMP"
};

/* This just looks for the atom names in a couple of tables and returns an
 * index with an offset added. */
Boolean XtConvertAndStore(Widget widget, _Xconst _XtString from_type,
                          XrmValue* from, _Xconst _XtString to_type,
                          XrmValue* to_in_out)
{
    Boolean rc = False;
    /* What we support is: */
    AssertReturn(from_type == XtRString, False);
    AssertReturn(to_type == XtRAtom, False);
    for (unsigned i = 0; i < RT_ELEMENTS(g_aFormats); ++i)
        if (!strcmp(from->addr, g_aFormats[i].pcszAtom))
        {
            *(Atom *)(to_in_out->addr) = (Atom) (i + 0x1000);
            rc = True;
        }
    for (unsigned i = 0; i < RT_ELEMENTS(g_apszSupAtoms); ++i)
        if (!strcmp(from->addr, g_apszSupAtoms[i]))
        {
            *(Atom *)(to_in_out->addr) = (Atom) (i + 0x2000);
            rc = True;
        }
    Assert(rc == True);  /* Have we missed any atoms? */
    return rc;
}

/* The current values of the X selection, which will be returned to the
 * XtGetSelectionValue callback. */
static Atom g_selTarget = 0;
static Atom g_selType = 0;
static const void *g_pSelData = NULL;
static unsigned long g_cSelData = 0;
static int g_selFormat = 0;

void XtGetSelectionValue(Widget widget, Atom selection, Atom target,
                         XtSelectionCallbackProc callback,
                         XtPointer closure, Time time)
{
    unsigned long count = 0;
    int format = 0;
    Atom type = XA_STRING;
    if (   (   selection != clipGetAtom(NULL, "PRIMARY")
            && selection != clipGetAtom(NULL, "CLIPBOARD")
            && selection != clipGetAtom(NULL, "TARGETS"))
        || (   target != g_selTarget
            && target != clipGetAtom(NULL, "TARGETS")))
    {
        /* Otherwise this is probably a caller error. */
        Assert(target != g_selTarget);
        callback(NULL, closure, &selection, &type, NULL, &count, &format);
                /* Could not convert to target. */
        return;
    }
    XtPointer pValue = NULL;
    if (target == clipGetAtom(NULL, "TARGETS"))
    {
        pValue = (XtPointer) RTMemDup(&g_selTarget, sizeof(g_selTarget));
        type = XA_ATOM;
        count = 1;
        format = 32;
    }
    else
    {
        pValue = (XtPointer) g_pSelData ? RTMemDup(g_pSelData, g_cSelData)
                                        : NULL;
        type = g_selType;
        count = g_pSelData ? g_cSelData : 0;
        format = g_selFormat;
    }
    if (!pValue)
    {
        count = 0;
        format = 0;
    }
    callback(NULL, closure, &selection, &type, pValue,
             &count, &format);
}

/* The formats currently on offer from X11 via the shared clipboard */
static uint32_t g_fX11Formats = 0;

void VBoxX11ClipboardReportX11Formats(VBOXCLIPBOARDCONTEXT* pCtx,
                                      uint32_t u32Formats)
{
    g_fX11Formats = u32Formats;
}

static uint32_t clipQueryFormats()
{
    return g_fX11Formats;
}

/* Does our clipboard code currently own the selection? */
static bool g_ownsSel = false;
/* The procedure that is called when we should convert the selection to a
 * given format. */
static XtConvertSelectionProc g_pfnSelConvert = NULL;
/* The procedure which is called when we lose the selection. */
static XtLoseSelectionProc g_pfnSelLose = NULL;
/* The procedure which is called when the selection transfer has completed. */
static XtSelectionDoneProc g_pfnSelDone = NULL;

Boolean XtOwnSelection(Widget widget, Atom selection, Time time,
                       XtConvertSelectionProc convert,
                       XtLoseSelectionProc lose,
                       XtSelectionDoneProc done)
{
    if (selection != clipGetAtom(NULL, "CLIPBOARD"))
        return True;  /* We don't really care about this. */
    g_ownsSel = true;  /* Always succeed. */
    g_pfnSelConvert = convert;
    g_pfnSelLose = lose;
    g_pfnSelDone = done;
    return True;
}

void XtDisownSelection(Widget widget, Atom selection, Time time)
{
    g_ownsSel = false;
    g_pfnSelConvert = NULL;
    g_pfnSelLose = NULL;
    g_pfnSelDone = NULL;
}

/* Request the shared clipboard to convert its data to a given format. */
static bool clipConvertSelection(const char *pcszTarget, Atom *type,
                                 XtPointer *value, unsigned long *length,
                                 int *format)
{
    Atom target = clipGetAtom(NULL, pcszTarget);
    if (target == 0)
        return false;
    /* Initialise all return values in case we make a quick exit. */
    *type = XA_STRING;
    *value = NULL;
    *length = 0;
    *format = 0;
    if (!g_ownsSel)
        return false;
    if (!g_pfnSelConvert)
        return false;
    Atom clipAtom = clipGetAtom(NULL, "CLIPBOARD");
    if (!g_pfnSelConvert(TEST_WIDGET, &clipAtom, &target, type,
                         value, length, format))
        return false;
    if (g_pfnSelDone)
        g_pfnSelDone(TEST_WIDGET, &clipAtom, &target);
    return true;
}

/* Set the current X selection data */
static void clipSetSelectionValues(const char *pcszTarget, Atom type,
                                   const void *data,
                                   unsigned long count, int format)
{
    Atom clipAtom = clipGetAtom(NULL, "CLIPBOARD");
    g_selTarget = clipGetAtom(NULL, pcszTarget);
    g_selType = type;
    g_pSelData = data;
    g_cSelData = count;
    g_selFormat = format;
    if (g_pfnSelLose)
        g_pfnSelLose(TEST_WIDGET, &clipAtom);
    g_ownsSel = false;
    g_fX11Formats = 0;
}

char *XtMalloc(Cardinal size) { return (char *) RTMemAlloc(size); }

char *XGetAtomName(Display *display, Atom atom)
{
    AssertReturn((unsigned)atom < RT_ELEMENTS(g_aFormats) + 1, NULL);
    const char *pcszName = NULL;
    if (atom < 0x1000)
        return NULL;
    else if (0x1000 <= atom && atom < 0x2000)
    {
        unsigned index = atom - 0x1000;
        AssertReturn(index < RT_ELEMENTS(g_aFormats), NULL);
        pcszName = g_aFormats[index].pcszAtom;
    }
    else
    {
        unsigned index = atom - 0x2000;
        AssertReturn(index < RT_ELEMENTS(g_apszSupAtoms), NULL);
        pcszName = g_apszSupAtoms[index];
    }
    return (char *)RTMemDup(pcszName, sizeof(pcszName) + 1);
}

int XFree(void *data)
{
    RTMemFree(data);
    return 0;
}

void XFreeStringList(char **list)
{
    if (list)
        RTMemFree(*list);
    RTMemFree(list);
}

const char XtStrings [] = "";
_WidgetClassRec* applicationShellWidgetClass;
const char XtShellStrings [] = "";

#define MAX_BUF_SIZE 256

static bool testStringFromX11(CLIPBACKEND *pCtx, uint32_t cbBuf,
                              const char *pcszExp, int rcExp)
{
    bool retval = false;
    AssertReturn(cbBuf <= MAX_BUF_SIZE, false);
    if (!clipPollTargets())
        RTPrintf("Failed to poll for targets\n");
    else if (clipQueryFormats() != VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
        RTPrintf("Wrong targets reported: %02X\n", clipQueryFormats());
    else
    {
        char pc[MAX_BUF_SIZE];
        uint32_t cbActual;
        int rc = VBoxX11ClipboardReadX11Data(pCtx,
                                     VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                     (void *) pc, cbBuf, &cbActual);
        if (rc != rcExp)
            RTPrintf("Wrong return code, expected %Rrc, got %Rrc\n", rcExp,
                     rc);
        else if (RT_FAILURE(rcExp))
            retval = true;
        else
        {
            RTUTF16 wcExp[MAX_BUF_SIZE / 2];
            RTUTF16 *pwcExp = wcExp;
            size_t cwc = 0;
            rc = RTStrToUtf16Ex(pcszExp, RTSTR_MAX, &pwcExp,
                                RT_ELEMENTS(wcExp), &cwc);
            size_t cbExp = cwc * 2 + 2;
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                if (cbActual != cbExp)
                {
                    RTPrintf("Returned string is the wrong size, string \"%.*ls\", size %u\n",
                             RT_MIN(MAX_BUF_SIZE, cbActual), pc, cbActual);
                    RTPrintf("Expected \"%s\", size %u\n", pcszExp,
                             cbExp);
                }
                else
                {
                    if (memcmp(pc, wcExp, cbExp) == 0)
                        retval = true;
                    else
                        RTPrintf("Returned string \"%.*ls\" does not match expected string \"%s\"\n",
                                 MAX_BUF_SIZE, pc, pcszExp);
                }
            }
        }
    }
    if (!retval)
        RTPrintf("Expected: string \"%s\", rc %Rrc (buffer size %u)\n",
                 pcszExp, rcExp, cbBuf);
    return retval;
}

static bool testLatin1FromX11(CLIPBACKEND *pCtx, uint32_t cbBuf,
                              const char *pcszExp, int rcExp)
{
    bool retval = false;
    AssertReturn(cbBuf <= MAX_BUF_SIZE, false);
    if (!clipPollTargets())
        RTPrintf("Failed to poll for targets\n");
    else if (clipQueryFormats() != VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
        RTPrintf("Wrong targets reported: %02X\n", clipQueryFormats());
    else
    {
        char pc[MAX_BUF_SIZE];
        uint32_t cbActual;
        int rc = VBoxX11ClipboardReadX11Data(pCtx,
                                     VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                     (void *) pc, cbBuf, &cbActual);
        if (rc != rcExp)
            RTPrintf("Wrong return code, expected %Rrc, got %Rrc\n", rcExp,
                     rc);
        else if (RT_FAILURE(rcExp))
            retval = true;
        else
        {
            RTUTF16 wcExp[MAX_BUF_SIZE / 2];
            RTUTF16 *pwcExp = wcExp;
            size_t cwc;
            for (cwc = 0; cwc == 0 || pcszExp[cwc - 1] != '\0'; ++cwc)
                wcExp[cwc] = pcszExp[cwc];
            size_t cbExp = cwc * 2;
            if (cbActual != cbExp)
            {
                RTPrintf("Returned string is the wrong size, string \"%.*ls\", size %u\n",
                         RT_MIN(MAX_BUF_SIZE, cbActual), pc, cbActual);
                RTPrintf("Expected \"%s\", size %u\n", pcszExp,
                         cbExp);
            }
            else
            {
                if (memcmp(pc, wcExp, cbExp) == 0)
                    retval = true;
                else
                    RTPrintf("Returned string \"%.*ls\" does not match expected string \"%s\"\n",
                             MAX_BUF_SIZE, pc, pcszExp);
            }
        }
    }
    if (!retval)
        RTPrintf("Expected: string \"%s\", rc %Rrc (buffer size %u)\n",
                 pcszExp, rcExp, cbBuf);
    return retval;
}

static bool testStringFromVBox(CLIPBACKEND *pCtx,
                               const char *pcszTarget, Atom typeExp,
                               const void *valueExp, unsigned long lenExp,
                               int formatExp)
{
    bool retval = false;
    Atom type;
    XtPointer value = NULL;
    unsigned long length;
    int format;
    if (clipConvertSelection(pcszTarget, &type, &value, &length, &format))
    {
        if (   type != typeExp
            || length != lenExp
            || format != formatExp
            || memcmp((const void *) value, (const void *)valueExp,
                      lenExp))
        {
            RTPrintf("Bad data: type %d, (expected %d), length %u, (%u), format %d (%d),\n",
                     type, typeExp, length, lenExp, format, formatExp);
            RTPrintf("value \"%.*s\" (\"%.*s\")", RT_MIN(length, 20), value,
                     RT_MIN(lenExp, 20), valueExp);
        }
        else
            retval = true;
    }
    else
        RTPrintf("Conversion failed\n");
    XtFree((char *)value);
    if (!retval)
        RTPrintf("Conversion to %s, expected \"%s\"\n", pcszTarget, valueExp);
    return retval;
}

static bool testStringFromVBoxFailed(CLIPBACKEND *pCtx,
                                     const char *pcszTarget)
{
    bool retval = false;
    Atom type;
    XtPointer value = NULL;
    unsigned long length;
    int format;
    if (!clipConvertSelection(pcszTarget, &type, &value, &length, &format))
        retval = true;
    XtFree((char *)value);
    if (!retval)
    {
        RTPrintf("Conversion to target %s, should have failed but didn't\n",
                 pcszTarget);
        RTPrintf("Returned type %d, length %u, format %d, value \"%.*s\"\n",
                 type, length, format, RT_MIN(length, 20), value);
    }
    return retval;
}

int main()
{
    RTR3Init();
    CLIPBACKEND *pCtx = VBoxX11ClipboardConstructX11(NULL);
    unsigned cErrs = 0;
    char pc[MAX_BUF_SIZE];
    uint32_t cbActual;
    int rc = VBoxX11ClipboardStartX11(pCtx);
    AssertRCReturn(rc, 1);

    /*** Utf-8 from X11 ***/
    RTPrintf(TEST_NAME ": TESTING reading Utf-8 from X11\n");
    /* Simple test */
    clipSetSelectionValues("UTF8_STRING", XA_STRING, "hello world",
                           sizeof("hello world"), 8);
    if (!testStringFromX11(pCtx, 256, "hello world", VINF_SUCCESS))
        ++cErrs;
    /* Receiving buffer of the exact size needed */
    if (!testStringFromX11(pCtx, sizeof("hello world") * 2, "hello world",
                           VINF_SUCCESS))
        ++cErrs;
    /* Buffer one too small */
    if (!testStringFromX11(pCtx, sizeof("hello world") * 2 - 1, "hello world",
                           VERR_BUFFER_OVERFLOW))
        ++cErrs;
    /* Zero-size buffer */
    if (!testStringFromX11(pCtx, 0, "hello world", VERR_BUFFER_OVERFLOW))
        ++cErrs;
    /* With an embedded carriage return */
    clipSetSelectionValues("text/plain;charset=UTF-8", XA_STRING,
                           "hello\nworld", sizeof("hello\nworld"), 8);
    if (!testStringFromX11(pCtx, sizeof("hello\r\nworld") * 2,
                           "hello\r\nworld", VINF_SUCCESS))
        ++cErrs;
    /* An empty string */
    clipSetSelectionValues("text/plain;charset=utf-8", XA_STRING, "",
                           sizeof(""), 8);
    if (!testStringFromX11(pCtx, sizeof("") * 2, "", VINF_SUCCESS))
        ++cErrs;
    /* With an embedded Utf-8 character. */
    clipSetSelectionValues("STRING", XA_STRING,
                           "100\xE2\x82\xAC" /* 100 Euro */,
                           sizeof("100\xE2\x82\xAC"), 8);
    if (!testStringFromX11(pCtx, sizeof("100\xE2\x82\xAC") * 2,
                           "100\xE2\x82\xAC", VINF_SUCCESS))
        ++cErrs;
    /* A non-zero-terminated string */
    clipSetSelectionValues("TEXT", XA_STRING,
                           "hello world", sizeof("hello world") - 2, 8);
    if (!testStringFromX11(pCtx, sizeof("hello world") * 2 - 2,
                           "hello worl", VINF_SUCCESS))
        ++cErrs;

    /*** COMPOUND TEXT from X11 ***/
    RTPrintf(TEST_NAME ": TESTING reading compound text from X11\n");
    /* Simple test */
    clipSetSelectionValues("COMPOUND_TEXT", XA_STRING, "hello world",
                           sizeof("hello world"), 8);
    if (!testStringFromX11(pCtx, 256, "hello world", VINF_SUCCESS))
        ++cErrs;
    /* Receiving buffer of the exact size needed */
    if (!testStringFromX11(pCtx, sizeof("hello world") * 2, "hello world",
                           VINF_SUCCESS))
        ++cErrs;
    /* Buffer one too small */
    if (!testStringFromX11(pCtx, sizeof("hello world") * 2 - 1, "hello world",
                           VERR_BUFFER_OVERFLOW))
        ++cErrs;
    /* Zero-size buffer */
    if (!testStringFromX11(pCtx, 0, "hello world", VERR_BUFFER_OVERFLOW))
        ++cErrs;
    /* With an embedded carriage return */
    clipSetSelectionValues("COMPOUND_TEXT", XA_STRING, "hello\nworld",
                           sizeof("hello\nworld"), 8);
    if (!testStringFromX11(pCtx, sizeof("hello\r\nworld") * 2,
                           "hello\r\nworld", VINF_SUCCESS))
        ++cErrs;
    /* An empty string */
    clipSetSelectionValues("COMPOUND_TEXT", XA_STRING, "",
                           sizeof(""), 8);
    if (!testStringFromX11(pCtx, sizeof("") * 2, "", VINF_SUCCESS))
        ++cErrs;
    /* A non-zero-terminated string */
    clipSetSelectionValues("COMPOUND_TEXT", XA_STRING,
                           "hello world", sizeof("hello world") - 2, 8);
    if (!testStringFromX11(pCtx, sizeof("hello world") * 2 - 2,
                           "hello worl", VINF_SUCCESS))
        ++cErrs;

    /*** Latin1 from X11 ***/
    RTPrintf(TEST_NAME ": TESTING reading Latin1 from X11\n");
    /* Simple test */
    clipSetSelectionValues("STRING", XA_STRING, "Georges Dupr\xEA",
                           sizeof("Georges Dupr\xEA"), 8);
    if (!testLatin1FromX11(pCtx, 256, "Georges Dupr\xEA", VINF_SUCCESS))
        ++cErrs;
    /* Receiving buffer of the exact size needed */
    if (!testLatin1FromX11(pCtx, sizeof("Georges Dupr\xEA") * 2,
                           "Georges Dupr\xEA", VINF_SUCCESS))
        ++cErrs;
    /* Buffer one too small */
    if (!testLatin1FromX11(pCtx, sizeof("Georges Dupr\xEA") * 2 - 1,
                           "Georges Dupr\xEA", VERR_BUFFER_OVERFLOW))
        ++cErrs;
    /* Zero-size buffer */
    if (!testLatin1FromX11(pCtx, 0, "Georges Dupr\xEA", VERR_BUFFER_OVERFLOW))
        ++cErrs;
    /* With an embedded carriage return */
    clipSetSelectionValues("TEXT", XA_STRING, "Georges\nDupr\xEA",
                           sizeof("Georges\nDupr\xEA"), 8);
    if (!testLatin1FromX11(pCtx, sizeof("Georges\r\nDupr\xEA") * 2,
                           "Georges\r\nDupr\xEA", VINF_SUCCESS))
        ++cErrs;
    /* A non-zero-terminated string */
    clipSetSelectionValues("text/plain", XA_STRING,
                           "Georges Dupr\xEA!",
                           sizeof("Georges Dupr\xEA!") - 2, 8);
    if (!testLatin1FromX11(pCtx, sizeof("Georges Dupr\xEA!") * 2 - 2,
                           "Georges Dupr\xEA", VINF_SUCCESS))
        ++cErrs;


    /*** Timeout from X11 ***/
    RTPrintf(TEST_NAME ": TESTING X11 timeout\n");
    clipSetSelectionValues("UTF8_STRING", XT_CONVERT_FAIL, "hello world",
                           sizeof("hello world"), 8);
    if (!testStringFromX11(pCtx, 256, "hello world", VERR_TIMEOUT))
        ++cErrs;

    /*** No data in X11 clipboard ***/
    RTPrintf(TEST_NAME ": TESTING a data request from an empty X11 clipboard\n");
    clipSetSelectionValues("UTF8_STRING", XA_STRING, NULL,
                           0, 8);
    rc = VBoxX11ClipboardReadX11Data(pCtx,
                                     VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                     (void *) pc, sizeof(pc), &cbActual);
    if (rc != VERR_NO_DATA)
    {
        RTPrintf("Returned %Rrc instead of VERR_NO_DATA\n", rc);
        ++cErrs;
    }

    /*** request for an invalid VBox format from X11 ***/
    RTPrintf(TEST_NAME ": TESTING a request for an invalid host format from X11\n");
    rc = VBoxX11ClipboardReadX11Data(pCtx, 0xffff, (void *) pc,
                                     sizeof(pc), &cbActual);
    if (rc != VERR_NOT_IMPLEMENTED)
    {
        RTPrintf("Returned %Rrc instead of VERR_NOT_IMPLEMENTED\n", rc);
        ++cErrs;
    }

    /*** Utf-8 from VBox ***/
    RTPrintf(TEST_NAME ": TESTING reading Utf-8 from VBox\n");
    /* Simple test */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "hello world",
                     sizeof("hello world") * 2);
    if (!testStringFromVBox(pCtx, "UTF8_STRING",
                            clipGetAtom(NULL, "UTF8_STRING"),
                            "hello world", sizeof("hello world"), 8))
        ++cErrs;
    /* With an embedded carriage return */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "hello\r\nworld",
                     sizeof("hello\r\nworld") * 2);
    if (!testStringFromVBox(pCtx, "text/plain;charset=UTF-8",
                            clipGetAtom(NULL, "text/plain;charset=UTF-8"),
                            "hello\nworld", sizeof("hello\nworld"), 8))
        ++cErrs;
    /* An empty string */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "", 2);
    if (!testStringFromVBox(pCtx, "text/plain;charset=utf-8",
                            clipGetAtom(NULL, "text/plain;charset=utf-8"),
                            "", sizeof(""), 8))
        ++cErrs;
    /* With an embedded Utf-8 character. */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "100\xE2\x82\xAC" /* 100 Euro */,
                     10);
    if (!testStringFromVBox(pCtx, "STRING",
                            clipGetAtom(NULL, "STRING"),
                            "100\xE2\x82\xAC", sizeof("100\xE2\x82\xAC"), 8))
        ++cErrs;
    /* A non-zero-terminated string */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "hello world",
                     sizeof("hello world") * 2 - 4);
    if (!testStringFromVBox(pCtx, "TEXT",
                            clipGetAtom(NULL, "TEXT"),
                            "hello worl", sizeof("hello worl"), 8))
        ++cErrs;

    /*** COMPOUND TEXT from VBox ***/
    RTPrintf(TEST_NAME ": TESTING reading COMPOUND TEXT from VBox\n");
    /* Simple test */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "hello world",
                     sizeof("hello world") * 2);
    if (!testStringFromVBox(pCtx, "COMPOUND_TEXT",
                            clipGetAtom(NULL, "COMPOUND_TEXT"),
                            "hello world", sizeof("hello world"), 8))
        ++cErrs;
    /* With an embedded carriage return */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "hello\r\nworld",
                     sizeof("hello\r\nworld") * 2);
    if (!testStringFromVBox(pCtx, "COMPOUND_TEXT",
                            clipGetAtom(NULL, "COMPOUND_TEXT"),
                            "hello\nworld", sizeof("hello\nworld"), 8))
        ++cErrs;
    /* An empty string */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "", 2);
    if (!testStringFromVBox(pCtx, "COMPOUND_TEXT",
                            clipGetAtom(NULL, "COMPOUND_TEXT"),
                            "", sizeof(""), 8))
        ++cErrs;
    /* A non-zero-terminated string */
    clipSetVBoxUtf16(pCtx, VINF_SUCCESS, "hello world",
                     sizeof("hello world") * 2 - 4);
    if (!testStringFromVBox(pCtx, "COMPOUND_TEXT",
                            clipGetAtom(NULL, "COMPOUND_TEXT"),
                            "hello worl", sizeof("hello worl"), 8))
        ++cErrs;

    /*** Timeout from VBox ***/
    RTPrintf(TEST_NAME ": TESTING reading from VBox with timeout\n");
    clipEmptyVBox(pCtx, VERR_TIMEOUT);
    if (!testStringFromVBoxFailed(pCtx, "UTF8_STRING"))
        ++cErrs;

    /*** No data in VBox clipboard ***/
    RTPrintf(TEST_NAME ": TESTING reading from VBox with no data\n");
    clipEmptyVBox(pCtx, VINF_SUCCESS);
    if (!testStringFromVBoxFailed(pCtx, "UTF8_STRING"))
        ++cErrs;
    if (cErrs > 0)
        RTPrintf("Failed with %u error(s)\n", cErrs);
    return cErrs > 0 ? 1 : 0;
}

#endif

#ifdef SMOKETEST

/* This is a simple test case that just starts a copy of the X11 clipboard
 * backend, checks the X11 clipboard and exits.  If ever needed I will add an
 * interactive mode in which the user can read and copy to the clipboard from
 * the command line. */

#include <iprt/initterm.h>
#include <iprt/stream.h>

#define TEST_NAME "tstClipboardX11Smoke"

int VBoxX11ClipboardReadVBoxData(VBOXCLIPBOARDCONTEXT *pCtx,
                                 uint32_t u32Format, void **ppv,
                                 uint32_t *pcb)
{
    return VERR_NO_DATA;
}

void VBoxX11ClipboardReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx,
                                      uint32_t u32Formats)
{}

int main()
{
    int rc = VINF_SUCCESS;
    RTR3Init();
    /* We can't test anything without an X session, so just return success
     * in that case. */
    if (!RTEnvGet("DISPLAY"))
    {
        RTPrintf(TEST_NAME ": X11 not available, not running test\n");
        return 0;
    }
    RTPrintf(TEST_NAME ": TESTING\n");
    CLIPBACKEND *pCtx = VBoxX11ClipboardConstructX11(NULL);
    AssertReturn(pCtx, 1);
    rc = VBoxX11ClipboardStartX11(pCtx);
    AssertRCReturn(rc, 1);
    /* Give the clipboard time to synchronise. */
    RTThreadSleep(500);
    rc = VBoxX11ClipboardStopX11(pCtx);
    AssertRCReturn(rc, 1);
    VBoxX11ClipboardDestructX11(pCtx);
    return 0;
}

#endif /* SMOKETEST defined */
