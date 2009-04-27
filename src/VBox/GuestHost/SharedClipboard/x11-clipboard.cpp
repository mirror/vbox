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

/** Do we want to test Utf8 by disabling other text formats? */
static bool g_testUtf8 = false;
/** Do we want to test compount text by disabling other text formats? */
static bool g_testCText = false;
/** Are we currently debugging the clipboard code? */
static bool g_debugClipboard = false;

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
    CLIPFORMAT enmFormat;
    /** The corresponding VBox clipboard format */
    uint32_t   u32VBoxFormat;
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
    VBOXCLIPBOARDCONTEXTX11 *pCtx;
} g_contexts[CLIPBOARD_NUM_CONTEXTS];

/** Register a new X11 clipboard context. */
static int vboxClipboardAddContext(VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
static void vboxClipboardRemoveContext(VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
static VBOXCLIPBOARDCONTEXTX11 *vboxClipboardFindContext(Widget widget)
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

static int vboxClipboardWriteUtf16LE(VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
static int vboxClipboardGetUtf8FromX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
        rc = vboxClipboardWriteUtf16LE(pCtx, pu16SrcText, cwSrcLen,
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
static int vboxClipboardGetCTextFromX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
        rc = vboxClipboardWriteUtf16LE(pCtx, pu16SrcText, cwSrcLen,
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
static int vboxClipboardGetLatin1FromX11(VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
    VBOXCLIPBOARDCONTEXTX11 *pCtx = pRequest->pCtx;
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
    VBOXCLIPBOARDCONTEXTX11 *pCtx =
            reinterpret_cast<VBOXCLIPBOARDCONTEXTX11 *>(pClientData);
    Atom *atomTargets = reinterpret_cast<Atom *>(pValue);
    unsigned cAtoms = *pcLen;
    CLIPFORMAT enmBestTarget = INVALID;
    CLIPFORMAT enmRequiredTarget = INVALID;
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

    /* Debugging stuff */
    if (g_testUtf8)
        enmRequiredTarget = UTF8;
    else if (g_testCText)
        enmRequiredTarget = CTEXT;

    for (unsigned i = 0; i < cAtoms; ++i)
    {
        for (unsigned j = 0; j < RT_ELEMENTS(g_aFormats); ++j)
        {
            Atom formatAtom = clipGetAtom(pCtx->widget,
                                          g_aFormats[j].pcszAtom);
            if (atomTargets[i] == formatAtom)
            {
                if (   enmBestTarget < g_aFormats[j].enmFormat
                    /* debugging stuff */
                    && (   enmRequiredTarget == INVALID
                        || enmRequiredTarget == g_aFormats[j].enmFormat))
                {
                    enmBestTarget = g_aFormats[j].enmFormat;
                    atomBestTarget = formatAtom;
                }
                break;
            }
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
    if ((enmBestTarget != pCtx->X11TextFormat) || (pCtx->notifyVBox == true))
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
        pCtx->X11TextFormat = enmBestTarget;
        if (enmBestTarget != INVALID)
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
    XtAppAddTimeOut(pCtx->appContext, 200 /* ms */,
                    vboxClipboardPollX11ForTargets, pCtx);
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

    while (XtAppGetExitFlag(pCtx->appContext) == FALSE)
        XtAppProcessEvent(pCtx->appContext, XtIMAll);
    LogRel(("Shared clipboard: host clipboard thread terminated successfully\n"));
    return VINF_SUCCESS;
}

/** X11 specific uninitialisation for the shared clipboard.
 * @note  X11 backend code.
 */
static void vboxClipboardUninitX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
    
    VBOXCLIPBOARDCONTEXTX11 *pCtx = (VBOXCLIPBOARDCONTEXTX11 *)pUserData;

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
static int vboxClipboardInitX11 (VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
    }
    /* Create the pipes */
    int pipes[2];
    if (!pipe(pipes))
    {
        pCtx->wakeupPipeRead = pipes[0];
        pCtx->wakeupPipeWrite = pipes[1];
        XtAppAddInput(pCtx->appContext, pCtx->wakeupPipeRead,
                      (XtPointer) XtInputReadMask, vboxClipboardStopWorker,
                      (XtPointer) pCtx);
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
VBOXCLIPBOARDCONTEXTX11 *VBoxX11ClipboardConstructX11
                                 (VBOXCLIPBOARDCONTEXT *pFrontend)
{
    int rc;

    VBOXCLIPBOARDCONTEXTX11 *pCtx = (VBOXCLIPBOARDCONTEXTX11 *)
                    RTMemAllocZ(sizeof(VBOXCLIPBOARDCONTEXTX11));
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

    if (RTEnvGet("VBOX_CBTEST_UTF8"))
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
    if (pCtx)
        pCtx->pFrontend = pFrontend;
    return pCtx;
}

/**
 * Destruct the shared clipboard X11 backend.
 * @note  X11 backend code
 */
void VBoxX11ClipboardDestructX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
int VBoxX11ClipboardStartX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("\n"));
    /*
     * Immediately return if we are not connected to the host X server.
     */
    if (!g_fHaveX11)
        return VINF_SUCCESS;

    rc = vboxClipboardInitX11(pCtx);
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadCreate(&pCtx->thread, vboxClipboardThread, pCtx, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
        if (RT_FAILURE(rc))
            LogRel(("Failed to initialise the shared clipboard X11 backend.\n"));
    }
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
int VBoxX11ClipboardStopX11(VBOXCLIPBOARDCONTEXTX11 *pCtx)
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
static Boolean vboxClipboardConvertTargetsForX11(VBOXCLIPBOARDCONTEXTX11
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

/** This is a wrapper around VBoxX11ClipboardReadVBoxData that will cache the
 * data returned.  This is unfortunately necessary, because if the other side
 * of the shared clipboard is also an X11 system, it may send a format
 * announcement message every time its clipboard is read, for reasons that
 * are explained elsewhere.  Unfortunately, some applications on our side
 * like to read the clipboard several times in short succession in different
 * formats.  This can fail if it collides with a format announcement message.
 * @todo any ideas about how to do this better are welcome.
 */
static int vboxClipboardReadVBoxData (VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
static Boolean vboxClipboardConvertToUtf8ForX11(VBOXCLIPBOARDCONTEXTX11
                                                                      *pCtx,
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
    *atomTypeReturn = clipGetAtom(pCtx->widget, "UTF8_STRING");
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
    *pcLenReturn = property.nitems;
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
    VBOXCLIPBOARDCONTEXTX11 *pCtx = vboxClipboardFindContext(widget);

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
static void vboxClipboardReturnToX11(Widget widget, Atom *)
{
    VBOXCLIPBOARDCONTEXTX11 *pCtx = vboxClipboardFindContext(widget);
    LogFlowFunc (("called, giving X11 clipboard ownership\n"));
    /* These should be set to the right values as soon as we start polling */
    pCtx->X11TextFormat = INVALID;
    pCtx->X11BitmapFormat = INVALID;
    pCtx->fOwnsClipboard = false;
    pCtx->notifyVBox = true;
}

/** Structure used to pass information about formats that VBox supports */
typedef struct _VBOXCLIPBOARDFORMATS
{
    /** Context information for the X11 clipboard */
    VBOXCLIPBOARDCONTEXTX11 *pCtx;
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
    VBOXCLIPBOARDCONTEXTX11 *pCtx = pFormats->pCtx;
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
void VBoxX11ClipboardAnnounceVBoxFormat(VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
        XtAppAddTimeOut(pCtx->appContext, 0, vboxClipboardAnnounceWorker,
                        (XtPointer) pFormats);
    }
}

/** Worker function for VBoxX11ClipboardReadX11Data which runs on the event
 * thread. */
static void vboxClipboardReadX11Worker(XtPointer pUserData,
                                       XtIntervalId * /* interval */)
{
    VBOXCLIPBOARDREQUEST *pRequest = (VBOXCLIPBOARDREQUEST *)pUserData;
    VBOXCLIPBOARDCONTEXTX11 *pCtx = pRequest->pCtx;
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
int VBoxX11ClipboardReadX11Data(VBOXCLIPBOARDCONTEXTX11 *pCtx,
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
        XtAppAddTimeOut(pCtx->appContext, 0, vboxClipboardReadX11Worker,
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

enum { MAX_ATTEMPTS = 10 };

static VBOXCLIPBOARDCONTEXTX11 *g_pCtxTestX11;
static VBOXCLIPBOARDCONTEXTX11 *g_pCtxTestVBox;

/** Quick Utf16 string class, initialised from a Utf8 string */
class testUtf16String
{
    /** Stores the Utf16 data.  NULL if something went wrong. */
    PRTUTF16 mData;
    /** Stores the size in bytes of the data.  0 if something went wrong. */
    size_t mSize;
public:
    /** Constructor
     * @param aString the Utf8 representation of the string data
     */
    testUtf16String(const char *aString)
    {
        int rc = RTStrToUtf16(aString, &mData);
        if (RT_FAILURE(rc))
        {
            mData = NULL;
            mSize = 0;
            return;
        }
        mSize = RTUtf16Len(mData) * 2 + 2;
    }
    /** Destructor */
    ~testUtf16String()
    {
        RTUtf16Free(mData);
    }
    /** Getter for the data */
    PCRTUTF16 getData() { return mData; }
    /** Getter for the data size */
    size_t getSize() { return mSize; }
};

/** Parameters for a test run. */
static struct testData
{
    /** The string data we are offering. */
    testUtf16String data;
    /** The size of the buffer to receive the request data */
    uint32_t cchBuffer;
    /** The format we request the data in */
    PCRTUTF16 getData() { return data.getData(); }
    /** Getter for the Utf16 string data size in bytes */
    size_t getSize() { return data.getSize(); }
    /** Is the test expected to produce a buffer overflow? */
    bool overflow() { return cchBuffer < getSize(); }
} g_sTestData[] =
{
    { "Hello world\r\n", 256 },
    { "Goodbye world", 28 },
    { "", 2 },
    /* This should produce a buffer overflow */
    { "Goodbye world", 27 },
};

/** Which line of the table above are we currently testing? */
size_t g_testNum = 0;
enum { MAX_TESTS = RT_ELEMENTS(g_sTestData) };
/** Are we doing a timeout test?  Ugly, but whatever. */
static bool g_testTimeout = false;

/** @todo the current code can't test the following conditions:
 *         * the X11 clipboard offers non-Utf8 text
 *         * the X11 clipboard offers invalid Utf8 text
 */
/** @todo stress test our context store code?  Probably not important... */

int VBoxX11ClipboardReadVBoxData(VBOXCLIPBOARDCONTEXT *pCtx,
                                 uint32_t u32Format, void **ppv,
                                 uint32_t *pcb)
{
    LogFlowFunc(("pCtx = %p\n", pCtx));
    /* This should only ever be called for the second "test" clipboard. */
    AssertReturn(pCtx == (VBOXCLIPBOARDCONTEXT *)g_pCtxTestX11
                         /* Magic cookie */, VERR_WRONG_ORDER);
    /* Timeout test hack */
    if (g_testTimeout)
    {
        RTThreadSleep(200);
        /* Try to return data after we have been timed out. */
        *ppv = RTMemDup((const void *) "Scribblings", sizeof("Scribblings"));
        *pcb = sizeof("Scribblings");
        LogFlowFunc(("sleep finished, returning\n"));
        return VINF_SUCCESS;
    }
    /* Sanity. */
    AssertReturn(g_testNum < MAX_TESTS, VERR_WRONG_ORDER);
    testData *pData = &g_sTestData[g_testNum];
    AssertPtrReturn(pData->getData(), VERR_NO_MEMORY);
    AssertReturn(pData->getSize(), VERR_WRONG_ORDER);
    void *retval = RTMemDup((const void *) pData->getData(),
                            pData->getSize());
    AssertPtrReturn(retval, VERR_NO_MEMORY);
    *ppv = (void *) retval;
    *pcb = pData->getSize();
    LogFlowFunc(("returning\n"));
    return VINF_SUCCESS;
}

/** As long as our test is running, we grab back the clipboard if X11 ever
 * tries to take it away.  I hope we don't end up with a tug of war... */
void VBoxX11ClipboardReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx,
                                      uint32_t u32Formats)
{
    LogFlowFunc(("pCtx = %p\n", pCtx));
    if (pCtx == (VBOXCLIPBOARDCONTEXT *)g_pCtxTestX11) /* Magic cookie */
        VBoxX11ClipboardAnnounceVBoxFormat(g_pCtxTestVBox,
                        VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
}

/** Initial tests that can be done while the clipboard contents are still
 * invalid.
 * @returns boolean success value
 * @note    prints status information to stdout
 */
bool testInvalid(void)
{
    bool fSuccess = true;
    char pc[256];
    uint32_t cbActual;
    RTPrintf("tstClipboardX11:  TESTING a request for an invalid data format\n");
    int rc = VBoxX11ClipboardReadX11Data(g_pCtxTestX11, 0xffff, (void *) pc,
                                         sizeof(pc), &cbActual);
    RTPrintf("Returned %Rrc - %s\n", rc,
             rc == VERR_NOT_IMPLEMENTED ? "SUCCESS" : "FAILURE");
    if (rc != VERR_NOT_IMPLEMENTED)
        fSuccess = false;
    RTPrintf("tstClipboardX11:  TESTING a request for data from an empty clipboard\n");
    VBoxX11ClipboardAnnounceVBoxFormat(g_pCtxTestX11,
                                      VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    VBoxX11ClipboardAnnounceVBoxFormat(g_pCtxTestX11,
                                       0);
    rc = VBoxX11ClipboardReadX11Data(g_pCtxTestX11,
                                     VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                     (void *) pc, sizeof(pc), &cbActual);
    RTPrintf("Returned %Rrc - %s\n", rc,
             rc == VERR_NO_DATA ? "SUCCESS" : "FAILURE");
    if (rc != VERR_NO_DATA)
        fSuccess = false;
    return fSuccess;
}

/** Tests an entry in the table above.
 * @returns boolean success value
 * @note    prints status information to stdout
 */
bool testEntry(testData *pData)
{
    bool fSuccess = false;
    char pc[256];
    uint32_t cbActual;
    for (int i = 0; (i < MAX_ATTEMPTS) && !fSuccess; ++i)
    {
        VBoxX11ClipboardAnnounceVBoxFormat(g_pCtxTestVBox,
                                      VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
        int rc = VBoxX11ClipboardReadX11Data(g_pCtxTestX11,
                        VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                             (void *) pc, sizeof(pc),
                                             &cbActual);
        AssertRCReturn(rc, 1);
        /* Did we expect and get an overflow? */
        if (   RT_SUCCESS(rc)
            && (cbActual == pData->getSize())
            && pData->overflow())
            fSuccess = true;
        /* Did we expect a string and get it? */
        else if (   RT_SUCCESS(rc)
                 && (memcmp(pc, pData->getData(),
                            pData->getSize()) == 0))
            fSuccess = true;
        else
            RTThreadSleep(50);
        if (fSuccess)
            RTPrintf("text %ls, %sretval %Rrc - SUCCESS\n",
                     pData->getData(),
                     pData->overflow() ? "buffer overflow as expected, "
                                       : "", rc);
        else
            RTPrintf("text %ls, retval %Rrc, attempt %d of %d\n",
                     pData->getData(), rc, i + 1,
                     MAX_ATTEMPTS);
    }
    if (!fSuccess)
        RTPrintf("FAILURE.  Last string obtained was %.*lS\n",
                 RT_MIN(cbActual / 2, 20), pc);
    return fSuccess;
}

int main()
{
    int rc = VINF_SUCCESS;
    int status = 0;
    g_debugClipboard = true;
    /* We can't test anything without an X session, so just return success
     * in that case. */
    if (!RTEnvGet("DISPLAY"))
        return 0;
    RTR3Init();
    g_pCtxTestX11 = VBoxX11ClipboardConstructX11(NULL);
    g_pCtxTestVBox =
        VBoxX11ClipboardConstructX11((VBOXCLIPBOARDCONTEXT *)g_pCtxTestX11);
    rc = VBoxX11ClipboardStartX11(g_pCtxTestVBox);
    AssertRCReturn(rc, 1);
    bool fSuccess = false;
    /* Test a request for an invalid data format and data from an empty
     * clipboard */
    for (unsigned i = 0; (i < MAX_ATTEMPTS) && !fSuccess; ++i)
    {
        rc = VBoxX11ClipboardStartX11(g_pCtxTestX11);
        AssertRCReturn(rc, 1);
        fSuccess = testInvalid();
        VBoxX11ClipboardStopX11(g_pCtxTestX11);
        if (!fSuccess)
        {
            RTPrintf("attempt %d of %d\n", i + 1, MAX_ATTEMPTS + 1);
            RTThreadSleep(50);
        }
    }
    if (!fSuccess)
        status = 1;
    rc = VBoxX11ClipboardStartX11(g_pCtxTestX11);
    AssertRCReturn(rc, 1);
    /* Claim the clipboard and make sure we get it */
    VBoxX11ClipboardAnnounceVBoxFormat(g_pCtxTestVBox,
                    VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    for (unsigned i = 0;
            i < 255
         && g_pCtxTestX11->X11TextFormat == INVALID;
         ++i)
        RTThreadSleep(50);
    AssertReturn(g_pCtxTestX11->X11TextFormat != INVALID, 1);
    /* Do general purpose clipboard tests */
    for (int i = 0; i < 2; ++i)
    {
        g_testUtf8 = (i == 0);
        g_testCText = (i == 1);
        RTPrintf("tstClipboardX11: TESTING sending and receiving of %s\n",
                 i == 0 ? "Utf-8" : "COMPOUND TEXT");
        for (g_testNum = 0; g_testNum < MAX_TESTS; ++g_testNum)
        {
            if (!testEntry(&g_sTestData[g_testNum]))
                status = 1;
        }
    }
    /* Finally test timeouts. */
    XtAppSetSelectionTimeout(g_pCtxTestX11->appContext, 10);
    RTPrintf("tstClipboardX11: TESTING the clipboard timeout\n");
    rc = VINF_SUCCESS;
    g_testTimeout = true;
    for (unsigned i = 0; (i < MAX_ATTEMPTS) && (rc != VERR_TIMEOUT); ++i)
    {
        char pc[256];
        uint32_t cbActual;
        VBoxX11ClipboardAnnounceVBoxFormat(g_pCtxTestVBox,
                                      VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
        rc = VBoxX11ClipboardReadX11Data(g_pCtxTestX11,
                    VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                         (void *) pc, sizeof(pc), &cbActual);
        AssertMsg(   RT_SUCCESS(rc)
                  || (rc == VERR_TIMEOUT)
                  || (rc == VERR_NO_DATA),
                  ("rc = %Rrc\n", rc));
        if (rc == VERR_TIMEOUT)
            RTPrintf("SUCCESS\n");
        else
        {
            RTPrintf("Attempt %d of %d\n", i + 1, MAX_ATTEMPTS);
            RTThreadSleep(50);
        }
    }
    if (rc != VERR_TIMEOUT)
        status = 1;
    rc = VBoxX11ClipboardStopX11(g_pCtxTestX11);
    AssertRCReturn(rc, 1);
    rc = VBoxX11ClipboardStopX11(g_pCtxTestVBox);
    AssertRCReturn(rc, 1);
    VBoxX11ClipboardDestructX11(g_pCtxTestX11);
    VBoxX11ClipboardDestructX11(g_pCtxTestVBox);
    return status;
}

#endif /* TESTCASE defined */
