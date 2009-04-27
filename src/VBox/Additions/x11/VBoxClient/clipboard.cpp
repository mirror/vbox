/** $Id$ */
/** @file
 * Guest Additions - X11 Shared Clipboard.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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

/****************************************************************************
*   Header Files                                                            *
****************************************************************************/
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/X.h>
#include <vector>

#include "VBoxClient.h"
#include "clipboard.h"

/** The formats which we support in the guest. These can be deactivated in order to test specific code paths. */
#define USE_UTF16
#define USE_UTF8
#define USE_CTEXT

/****************************************************************************
*   Global Variables                                                        *
****************************************************************************/
/** The different clipboard formats which we support. */
enum g_eClipboardFormat
{
    INVALID = 0,
    TARGETS,
    CTEXT,
    UTF8,
    UTF16
};

/** The X11 clipboard uses several names for the same format. This structure maps an X11 name to a format. */
typedef struct
{
    Atom atom;
    g_eClipboardFormat format;
    unsigned hostFormat;
} VBOXCLIPBOARDFORMAT;

/** Does the host or the guest currently own the clipboard? */
enum g_eClipboardOwner
{
    NONE = 0,
    HOST,
    GUEST
};

/**
 * Global clipboard context information.
 */
typedef struct
{
    /** The Xt application context structure */
    XtAppContext appContext;

    /** We have a separate thread to wait for Window and Clipboard events */
    RTTHREAD thread;
    /** The Xt widget which we use as our clipboard client.  It is never made visible. */
    Widget widget;

    /** X11 atom refering to the clipboard: CLIPBOARD */
    Atom atomClipboard;
    /** X11 atom refering to the selection: PRIMARY */
    Atom atomPrimary;
    /** X11 atom refering to the clipboard: TARGETS */
    Atom atomTargets;
    /** X11 atom refering to the clipboard: MULTIPLE */
    Atom atomMultiple;
    /** X11 atom refering to the clipboard: TIMESTAMP */
    Atom atomTimestamp;
    /** X11 atom refering to the clipboard utf16 text format: text/plain;charset=ISO-10646-UCS-2 */
    Atom atomUtf16;
    /** X11 atom refering to the clipboard utf8 text format: UTF8_STRING */
    Atom atomUtf8;
    /** X11 atom refering to the native X11 clipboard text format: COMPOUND_TEXT */
    Atom atomCText;

    /** A list of the X11 formats which we support, mapped to our identifier for them, in the order
        we prefer to have them in. */
    std::vector<VBOXCLIPBOARDFORMAT> formatList;

    /** Does the host or the guest currently own the clipboard? */
    volatile enum g_eClipboardOwner eOwner;

    /** What is the best text format the guest has to offer?  INVALID for none. */
    g_eClipboardFormat guestTextFormat;
    /** Atom corresponding to the guest text format */
    Atom atomGuestTextFormat;
    /** What is the best bitmap format the guest has to offer?  INVALID for none. */
    g_eClipboardFormat guestBitmapFormat;
    /** Atom corresponding to the guest Bitmap format */
    Atom atomGuestBitmapFormat;
    /** What formats does the host have on offer? */
    int hostFormats;
    /** Windows caches the clipboard data it receives.  Since we have no way of knowing whether
        that data is still valid, we always send a "data changed" message after a successful
        transfer to invalidate the cache. */
    bool notifyHost;

    /** Since the clipboard data moves asynchronously, we use an event semaphore to wait for it. */
    RTSEMEVENT terminating;

    /** Format which we are reading from the guest clipboard (valid during a request for the
        guest clipboard) */
    g_eClipboardFormat requestGuestFormat;
    /** The guest buffer to write guest clipboard data to (valid during a request for the host
        clipboard) */
    void *requestBuffer;
    /** The size of the host buffer to write guest clipboard data to (valid during a request for
        the guest clipboard) */
    unsigned requestBufferSize;
    /** The size of the guest clipboard data written to the host buffer (valid during a request
        for the guest clipboard) */
    uint32_t *requestActualSize;

    /** Client ID for the clipboard subsystem */
    uint32_t client;
} VBOXCLIPBOARDCONTEXT;

/** Only one client is supported. There seems to be no need for more clients. */
static VBOXCLIPBOARDCONTEXT g_ctx;


/**
 * Transfer clipboard data from the guest to the host.
 *
 * @returns VBox result code
 * @param   u32Format The format of the data being sent
 * @param   pv        Pointer to the data being sent
 * @param   cb        Size of the data being sent in bytes
 */
static int vboxClipboardSendData(uint32_t u32Format, void *pv, uint32_t cb)
{
    int rc;
    LogFlowFunc(("u32Format=%d, pv=%p, cb=%d\n", u32Format, pv, cb));
    rc = VbglR3ClipboardWriteData(g_ctx.client, u32Format, pv, cb);
    LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}


/**
 * Get clipboard data from the host.
 *
 * @returns VBox result code
 * @param   u32Format The format of the data being requested
 * @retval  ppv       On success and if pcb > 0, this will point to a buffer
 *                    to be freed with RTMemFree containing the data read.
 * @retval  pcb       On success, this contains the number of bytes of data
 *                    returned
 */
static int vboxClipboardReadHostData(uint32_t u32Format, void **ppv, uint32_t *pcb)
{
    int rc = VINF_SUCCESS;
    uint32_t cb = 1024;
    void *pv = RTMemAlloc(cb);

    *ppv = 0;
    LogFlowFunc(("u32Format=%u\n", u32Format));
    if (RT_UNLIKELY(!pv))
        rc = VERR_NO_MEMORY;
    if (RT_SUCCESS(rc))
        rc = VbglR3ClipboardReadData(g_ctx.client, u32Format, pv, cb, pcb);
    if (RT_SUCCESS(rc) && (rc != VINF_BUFFER_OVERFLOW))
        *ppv = pv;
    /* A return value of VINF_BUFFER_OVERFLOW tells us to try again with a
     * larger buffer.  The size of the buffer needed is placed in *pcb.
     * So we start all over again. */
    if (rc == VINF_BUFFER_OVERFLOW)
    {
        cb = *pcb;
        RTMemFree(pv);
        pv = RTMemAlloc(cb);
        if (RT_UNLIKELY(!pv))
            rc = VERR_NO_MEMORY;
        if (RT_SUCCESS(rc))
            rc = VbglR3ClipboardReadData(g_ctx.client, u32Format, pv, cb, pcb);
        if (RT_SUCCESS(rc) && (rc != VINF_BUFFER_OVERFLOW))
            *ppv = pv;
    }
    /* Catch other errors. This also catches the case in which the buffer was
     * too small a second time, possibly because the clipboard contents
     * changed half-way through the operation.  Since we can't say whether or
     * not this is actually an error, we just return size 0.
     */
    if (RT_FAILURE(rc) || (VINF_BUFFER_OVERFLOW == rc))
    {
        *pcb = 0;
        if (pv != NULL)
            RTMemFree(pv);
    }
    LogFlowFunc(("returning %Rrc\n", rc));
    if (RT_SUCCESS(rc))
        LogFlow(("    *pcb=%d\n", *pcb));
    return rc;
}


/**
 * Convert a Utf16 text with Linux EOLs to zero-terminated Utf16-LE with Windows EOLs, allocating
 * memory for the converted text.  Does no checking for validity.
 *
 * @returns VBox status code
 *
 * @param   pu16Src   Source Utf16 text to convert
 * @param   cwSrc     Size of the source text in 16 bit words
 * @retval  ppu16Dest Where to store the pointer to the converted text.  Only valid on success
 *                    and if pcwDest is greater than 0.
 * @retval  pcwDest   Size of the converted text in 16 bit words, including the trailing null
 *                    if present
 */
static int vboxClipboardUtf16LinToWin(PRTUTF16 pu16Src, size_t cwSrc, PRTUTF16 *ppu16Dest,
                                      size_t *pcwDest)
{
    enum { LINEFEED = 0xa, CARRIAGERETURN = 0xd };
    PRTUTF16 pu16Dest;
    size_t cwDest, i, j;
    LogFlowFunc(("pu16Src=%.*ls, cwSrc=%u\n", cwSrc, pu16Src, cwSrc));
    if (cwSrc == 0)
    {
        *ppu16Dest = 0;
        *pcwDest = 0;
        LogFlowFunc(("*ppu16Dest=0, *pcwDest=0, rc=VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
    AssertReturn(VALID_PTR(pu16Src), VERR_INVALID_PARAMETER);
    cwDest = 0;
    for (i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (pu16Src[i] == LINEFEED)
            ++cwDest;
        if (pu16Src[i] == 0)
            break;
    }
    /* Leave space for a trailing null */
    ++cwDest;
    pu16Dest = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwDest * 2));
    if (pu16Dest == 0)
    {
        LogFlowFunc(("rc=VERR_NO_MEMORY\n"));
        return VERR_NO_MEMORY;
    }
    for (i = (pu16Src[0] == 0xfeff ? 1 : 0), j = 0; i < cwSrc; ++i, ++j)
    {
        if (pu16Src[i] == LINEFEED)
        {
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
        }
        if (pu16Src[i] == 0)
            break;
        pu16Dest[j] = pu16Src[i];
    }
    /* The trailing null */
    pu16Dest[j] = 0;
    *ppu16Dest = pu16Dest;
    *pcwDest = cwDest;
    LogFlowFunc(("*ppu16Dest=%p, *pcwDest=%d, rc=VINF_SUCCESS\n", pu16Dest, cwDest));
    return VINF_SUCCESS;
}


/**
 * Convert the UTF-16 text returned from the guest X11 clipboard to UTF-16LE with Windows EOLs
 * and send it to the host.
 *
 * @param pValue      Source UTF-16 text
 * @param cwSourceLen Length in 16-bit words of the source text
 */
static void vboxClipboardGetUtf16(XtPointer pValue, size_t cwSourceLen)
{
    size_t cwDestLen;
    PRTUTF16 pu16DestText;
    PRTUTF16 pu16SourceText = reinterpret_cast<PRTUTF16>(pValue);
    int rc;

    LogFlowFunc(("converting Utf-16 to Utf-16LE.  Original is %.*ls\n",
              cwSourceLen - 1, pu16SourceText + 1));
    rc = vboxClipboardUtf16LinToWin(pu16SourceText, cwSourceLen, &pu16DestText, &cwDestLen);
    if (rc != VINF_SUCCESS)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    LogFlow(("vboxClipboardGetUtf16: converted string is %.*ls\n", cwDestLen, pu16DestText));
    vboxClipboardSendData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                            reinterpret_cast<void *>(pu16DestText), cwDestLen * 2);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    XtFree(reinterpret_cast<char *>(pValue));
    LogFlowFunc(("returning\n"));
}


/**
 * Convert the UTF-8 text returned from the guest X11 clipboard to UTF-16LE with Windows EOLs
 * and send it to the host.
 *
 * @param pValue      Source UTF-8 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 */
static void vboxClipboardGetUtf8(XtPointer pValue, size_t cbSourceLen)
{
    size_t cwSourceLen, cwDestLen;
    char *pu8SourceText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16SourceText = 0, pu16DestText;
    int rc;

    LogFlowFunc(("\n"));
    LogFlow(("vboxClipboardGetUtf8: converting Utf-8 to Utf-16LE. Original is %.*s\n", cbSourceLen,
           pu8SourceText));
    /* First convert the UTF8 to UTF16 */
    rc = RTStrToUtf16Ex(pu8SourceText, cbSourceLen, &pu16SourceText, 0, &cwSourceLen);
    if (rc != VINF_SUCCESS)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    rc = vboxClipboardUtf16LinToWin(pu16SourceText, cwSourceLen, &pu16DestText, &cwDestLen);
    if (rc != VINF_SUCCESS)
    {
        RTMemFree(reinterpret_cast<void *>(pu16SourceText));
        XtFree(reinterpret_cast<char *>(pValue));
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    LogFlow(("vboxClipboardGetUtf8: converted string is %.*ls\n", cwDestLen, pu16DestText));
    vboxClipboardSendData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                            reinterpret_cast<void *>(pu16DestText), cwDestLen * 2);
    RTMemFree(reinterpret_cast<void *>(pu16SourceText));
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    XtFree(reinterpret_cast<char *>(pValue));
    LogFlowFunc(("returning\n"));
}


/**
 * Convert the compound text returned from the guest X11 clipboard to UTF-16LE with Windows EOLs
 * and send it to the host.
 *
 * @param pValue      Source compound text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 */
static void vboxClipboardGetCText(XtPointer pValue, size_t cbSourceLen)
{
    size_t cwSourceLen, cwDestLen;
    char **ppu8SourceText = 0;
    PRTUTF16 pu16SourceText = 0, pu16DestText;
    XTextProperty property;
    int rc, cProps;

    LogFlowFunc(("\n"));
    LogFlow(("vboxClipboardGetCText: converting compound text to Utf-16LE. Original is %.*s\n",
           cbSourceLen, pValue));
    /* Quick fix for 2.2. */
    if (cbSourceLen == 0)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        vboxClipboardSendData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                              NULL, 0);        
    }
    /* First convert the compound text to Utf8 */
    property.value = reinterpret_cast<unsigned char *>(pValue);
    property.encoding = g_ctx.atomCText;
    property.format = 8;
    property.nitems = cbSourceLen;
#ifdef RT_OS_SOLARIS
    rc = XmbTextPropertyToTextList(XtDisplay(g_ctx.widget), &property, &ppu8SourceText, &cProps);
#else
    rc = Xutf8TextPropertyToTextList(XtDisplay(g_ctx.widget), &property, &ppu8SourceText, &cProps);
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
        XFreeStringList(ppu8SourceText);
        LogRel(("vboxClipboardGetCText: Xutf8TextPropertyToTextList failed.  Reason: %s\n", pcReason));
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    /* Next convert the UTF8 to UTF16 */
    rc = RTStrToUtf16Ex(*ppu8SourceText, cbSourceLen, &pu16SourceText, 0, &cwSourceLen);
    XFreeStringList(ppu8SourceText);
    if (rc != VINF_SUCCESS)
    {
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    rc = vboxClipboardUtf16LinToWin(pu16SourceText, cwSourceLen, &pu16DestText, &cwDestLen);
    RTMemFree(reinterpret_cast<void *>(pu16SourceText));
    if (rc != VINF_SUCCESS)
    {
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    LogFlow(("vboxClipboardGetCText: converted string is %.*ls\n", cwDestLen, pu16DestText));
    vboxClipboardSendData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                            reinterpret_cast<void *>(pu16DestText), cwDestLen * 2);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    LogFlowFunc(("returning\n"));
}


/**
 * Convert the Latin1 text returned from the guest X11 clipboard to UTF-16LE with Windows EOLs
 * and send it to the host.
 *
 * @param pValue      Source Latin1 text
 * @param cbSourceLen Length in 8-bit bytes of the source text
 */
static void vboxClipboardGetLatin1(XtPointer pValue, size_t cbSourceLen)
{
    enum { LINEFEED = 0xa, CARRIAGERETURN = 0xd };
    /* Leave space for an additional null character at the end of the destination text. */
    size_t cwDestLen = cbSourceLen + 1, cwDestPos;
    char *pu8SourceText = reinterpret_cast<char *>(pValue);
    PRTUTF16 pu16DestText;

    LogFlowFunc(("converting Latin1 to Utf-16LE.  Original is %.*s\n", cbSourceLen,
                 pu8SourceText));
    /* Find the size of the destination text */
    for (size_t i = 0; i < cbSourceLen; i++)
    {
        if (pu8SourceText[i] == LINEFEED)
            ++cwDestLen;
    }
    pu16DestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwDestLen * 2));
    if (pu16DestText == 0)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        LogFlow(("vboxClipboardGetLatin1: failed to allocate %d bytes!\n", cwDestLen * 2));
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    /* Copy the original X clipboard string to the destination, replacing Linux EOLs with
       Windows ones */
    cwDestPos = 0;
    for (size_t i = 0; i < cbSourceLen; ++i, ++cwDestPos)
    {
        if (pu8SourceText[i] == LINEFEED)
        {
            pu16DestText[cwDestPos] = CARRIAGERETURN;
            ++cwDestPos;
        }
        /* latin1 < utf-16LE */
        pu16DestText[cwDestPos] = pu8SourceText[i];
        if (pu8SourceText[i] == 0)
            break;
    }
    pu16DestText[cwDestPos] = 0;
    LogFlow(("vboxClipboardGetLatin1: converted text is %.*ls\n", cwDestPos, pu16DestText));
    vboxClipboardSendData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                            reinterpret_cast<void *>(pu16DestText), cwDestPos * 2);
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    XtFree(reinterpret_cast<char *>(pValue));
    LogFlowFunc(("returning\n"));
}


/** Convert the clipboard text from the current format to Utf-16 with Windows line breaks.
    We are reading the guest clipboard to make it available to the host. */
static void vboxClipboardGetProc(Widget, XtPointer /* pClientData */, Atom * /* selection */,
                                 Atom *atomType, XtPointer pValue, long unsigned int *pcLen,
                                 int *piFormat)
{
    LogFlowFunc(("*pcLen=%lu, *piFormat=%d, requested target format: %d, g_ctx.requestBufferSize=%d\n",
                 *pcLen, *piFormat, g_ctx.requestGuestFormat, g_ctx.requestBufferSize));
    /* The X Toolkit may have failed to get the clipboard selection for us. */
    if (*atomType == XT_CONVERT_FAIL)
    {
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("Xt failed to convert the data.  Sending empty data and returning\n"));
        return;
    }
    if (NULL == pValue)
    {
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("The clipboard contents changed while we were requesting them.  Sending empty data and returning\n"));
        return;
    }
    /* In which format did we request the clipboard data? */
    unsigned cTextLen = (*pcLen) * (*piFormat) / 8;
    switch (g_ctx.requestGuestFormat)
    {
        case UTF16:
            vboxClipboardGetUtf16(pValue, cTextLen / 2);
            break;
        case CTEXT:
            vboxClipboardGetCText(pValue, cTextLen);
            break;
        case UTF8:
        {
            /* If we are given broken Utf-8, we treat it as Latin1.  Is this acceptable? */
            size_t cStringLen;
            char *pu8SourceText = reinterpret_cast<char *>(pValue);

            if (RTStrUniLenEx(pu8SourceText, *pcLen, &cStringLen) == VINF_SUCCESS)
                vboxClipboardGetUtf8(pValue, cTextLen);
            else
                vboxClipboardGetLatin1(pValue, cTextLen);
            break;
        }
        default:
        {
            XtFree(reinterpret_cast<char *>(pValue));
            Log(("vboxClipboardGetProc: bad target format\n"));
            vboxClipboardSendData(0, NULL, 0);
            LogFlowFunc(("sending empty data and returning\n"));
            return;
        }
    }
    g_ctx.notifyHost = true;
    LogFlowFunc(("returning\n"));
}


/**
 * Tell the host that new clipboard formats are available.
 *
 * @returns VBox status code.
 * @param u32Formats      The formats to advertise
 */
static int vboxClipboardReportFormats(uint32_t u32Formats)
{
    int rc;
    LogFlowFunc(("u32Formats=%d\n", u32Formats));
    rc = VbglR3ClipboardReportFormats(g_ctx.client, u32Formats);
    LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}


/** Callback to handle a reply to a request for the targets the current clipboard holder can
    handle.  We are reading the guest clipboard to make it available to the host. */
static void vboxClipboardTargetsProc(Widget, XtPointer, Atom * /* selection */, Atom *atomType,
                                     XtPointer pValue, long unsigned int *pcLen, int *piFormat)
{
    static int cCalls = 0;
    Atom *atomTargets = reinterpret_cast<Atom *>(pValue);
    /* The number of format atoms the clipboard holder is offering us */
    unsigned cAtoms = *pcLen;
    /* The best clipboard format we have found so far */
    g_eClipboardFormat eBestTarget = INVALID;
    /* The atom corresponding to our best clipboard format found */
    Atom atomBestTarget = None;

    if ((cCalls % 10) == 0)
        Log3(("vboxClipboardTargetsProc called, cAtoms=%d\n", cAtoms));
    if (*atomType == XT_CONVERT_FAIL)
    {
        Log(("vboxClipboardTargetsProc: reading clipboard from guest, X toolkit failed to convert the selection\n"));
        LogFlowFunc(("returning\n"));
        return;
    }
    /* Run through the atoms offered to us to see if we recognise them.  If we find the atom for
       a "better" format than the best we have found so far, we remember it as our new "best"
       format. */
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
            if ((cCalls % 10) == 0)
                Log3(("vboxClipboardTargetsProc: the guest offers target %s\n", szAtomName));
            XFree(szAtomName);
        }
        else
        {
            if ((cCalls % 10) == 0)
                Log3(("vboxClipboardTargetsProc: the guest returned a bad atom: %d\n",
                       atomTargets[i]));
        }
#endif
    }
    g_ctx.atomGuestTextFormat = atomBestTarget;
    /* If the available formats as seen by the host have changed, or if we suspect that
       the host has cached the clipboard data (which can change without our noticing it),
       then tell the host that new clipboard contents are available. */
    if ((eBestTarget != g_ctx.guestTextFormat) || (g_ctx.notifyHost == true))
    {
        uint32_t u32Formats = 0;
#ifdef DEBUG
        if (atomBestTarget != None)
        {
            char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomBestTarget);
            LogFlow(("vboxClipboardTargetsProc: switching to guest text target %s.  Available targets are:\n", szAtomName));
            XFree(szAtomName);
        }
        else
            LogFlow(("vboxClipboardTargetsProc: no supported host text target found.  Available targets are:\n"));

        for (unsigned i = 0; i < cAtoms; ++i)
        {
            char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), atomTargets[i]);
            if (szAtomName != 0)
            {
                LogFlow(("vboxClipboardTargetsProc:     %s\n", szAtomName));
                XFree(szAtomName);
            }
        }
#endif
        g_ctx.guestTextFormat = eBestTarget;
        if (eBestTarget != INVALID)
            u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        vboxClipboardReportFormats(u32Formats);
        g_ctx.notifyHost = false;
    }
    XtFree(reinterpret_cast<char *>(pValue));
    ++cCalls;
}


/**
 * This callback is called every 200ms to check the contents of the guest clipboard.
 */
static void vboxClipboardTimerProc(XtPointer /* pUserData */, XtIntervalId * /* hTimerId */)
{
    static int cCalls = 0;
    /* Get the current clipboard contents */
    if (g_ctx.eOwner == GUEST)
    {
        if ((cCalls % 10) == 0)
            Log3(("vboxClipboardTimerProc: requesting the targets that the guest clipboard offers\n"));
        XtGetSelectionValue(g_ctx.widget, g_ctx.atomClipboard, g_ctx.atomTargets,
                            vboxClipboardTargetsProc, 0, CurrentTime);
    }
    /* Re-arm our timer */
    XtAppAddTimeOut(g_ctx.appContext, 200 /* ms */, vboxClipboardTimerProc, 0);
    ++cCalls;
}


/**
 * Satisfy a request from the guest for available clipboard targets.
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

    LogFlowFunc(("\n"));
    LogFlow(("vboxClipboardConvertTargets: uListSize=%u\n", uListSize));
    for (unsigned i = 0; i < uListSize; ++i)
    {
        if (   ((g_ctx.hostFormats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT) != 0)
            && (g_ctx.formatList[i].hostFormat == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT))
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
            LogFlow(("vboxClipboardConvertTargets: returning target %s\n", szAtomName));
            XFree(szAtomName);
        }
        else
            Log(("vboxClipboardConvertTargets: invalid atom %d in the list!\n", atomTargets[i]));
    }
#endif
    *atomTypeReturn = XA_ATOM;
    *pValReturn = reinterpret_cast<XtPointer>(atomTargets);
    *pcLenReturn = cTargets + 3;
    *piFormatReturn = 32;
    LogFlowFunc(("returning true\n"));
    return true;
}


/**
 * Get the size of the buffer needed to hold a zero-terminated Utf16 string with Linux EOLs
 * converted from a Utf16 string with Windows EOLs.
 *
 * @returns The size of the buffer needed in bytes
 *
 * @param   pu16Src  The source Utf16 string
 * @param   cwSrc    The length in 16 bit words of the source string
 */
static int vboxClipboardUtf16GetLinSize(PRTUTF16 pu16Src, size_t cwSrc)
{
    enum { LINEFEED = 0xa, CARRIAGERETURN = 0xd };
    size_t cwDest;

    LogFlowFunc(("pu16Src=%.*ls, cwSrc=%u\n", cwSrc, pu16Src, cwSrc));
    AssertReturn(pu16Src != 0, VERR_INVALID_PARAMETER);
    /* We only take little endian Utf16 */
    AssertReturn(pu16Src[0] != 0xfffe, VERR_INVALID_PARAMETER);
    if (cwSrc == 0)
    {
        LogFlowFunc(("returning 0\n"));
        return 0;
    }
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    if (pu16Src[0] == 0xfeff)
        cwDest = 0;
    else
        cwDest = 1;
    for (size_t i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (   (i + 1 < cwSrc)
            && (pu16Src[i] == CARRIAGERETURN)
            && (pu16Src[i + 1] == LINEFEED))
            ++i;
        if (pu16Src[i] == 0)
            break;
    }
    /* The terminating null */
    ++cwDest;
    LogFlowFunc(("returning %d\n", cwDest * 2));
    return cwDest * 2;
}


/**
 * Convert Utf16-LE text with Windows EOLs to Utf16 with Linux EOLs.  This function does not
 * verify that the Utf16 is valid.
 *
 * @returns VBox status code
 *
 * @param   pu16Src       Text to convert
 * @param   cwSrc         Size of the source text in 16 bit words
 * @param   pu16Dest      The buffer to store the converted text to
 * @param   cwDest        The size of the buffer for the destination text
 */
static int vboxClipboardUtf16WinToLin(PRTUTF16 pu16Src, size_t cwSrc, PRTUTF16 pu16Dest,
                                      size_t cwDest)
{
    enum { LINEFEED = 0xa, CARRIAGERETURN = 0xd };
    size_t cwDestPos;

    LogFlowFunc(("pu16Src=%.*ls, cwSrc=%u, pu16Dest=%p, cwDest=%u\n",
                  cwSrc, pu16Src, cwSrc, pu16Dest, cwDest));
    /* A buffer of size 0 may not be an error, but it is not a good idea either. */
    Assert(cwDest > 0);
    AssertReturn(VALID_PTR(pu16Src), VERR_INVALID_PARAMETER);
    /* We only take little endian Utf16 */
    AssertReturn(pu16Src[0] != 0xfffe, VERR_INVALID_PARAMETER);
    if (cwSrc == 0)
    {
        if (cwDest != 0)
            pu16Dest[0] = 0;
        LogFlowFunc(("set empty string.  Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
    /* Prepend the Utf16 byte order marker if it is missing. */
    if (pu16Src[0] == 0xfeff)
        cwDestPos = 0;
    else
    {
        if (cwDest == 0)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        pu16Dest[0] = 0xfeff;
        cwDestPos = 1;
    }
    for (size_t i = 0; i < cwSrc; ++i, ++cwDestPos)
    {
        if (cwDestPos == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (   (i + 1 < cwSrc)
            && (pu16Src[i] == CARRIAGERETURN)
            && (pu16Src[i + 1] == LINEFEED))
            ++i;

        pu16Dest[cwDestPos] = pu16Src[i];
        if (pu16Src[i] == 0)
            break;
    }
    if (cwDestPos == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[cwDestPos] = 0;
    LogFlowFunc(("set string %ls.  Returning\n", pu16Dest + 1));
    return VINF_SUCCESS;
}


/**
 * Satisfy a request from the guest to convert the clipboard text to Utf16.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param atomTypeReturn The type of the data we are returning
 * @param pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @param pcLenReturn    The length of the data we are returning
 * @param piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertUtf16(Atom *atomTypeReturn, XtPointer *pValReturn,
                                         unsigned long *pcLenReturn, int *piFormatReturn)
{
    PRTUTF16 pu16GuestText, pu16HostText;
    unsigned cbHostText, cwHostText, cwGuestText;
    int rc;

    LogFlowFunc(("\n"));
    rc = vboxClipboardReadHostData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   reinterpret_cast<void **>(&pu16HostText), &cbHostText);
    if ((rc != VINF_SUCCESS) || cbHostText == 0)
    {
        LogFlow(("vboxClipboardConvertUtf16: vboxClipboardReadHostData returned %Rrc, %d bytes of data\n", rc, cbHostText));
        g_ctx.hostFormats = 0;
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    cwHostText = cbHostText / 2;
    cwGuestText = vboxClipboardUtf16GetLinSize(pu16HostText, cwHostText);
    pu16GuestText = reinterpret_cast<PRTUTF16>(XtMalloc(cwGuestText * 2));
    if (pu16GuestText == 0)
    {
        RTMemFree(reinterpret_cast<void *>(pu16HostText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    rc = vboxClipboardUtf16WinToLin(pu16HostText, cwHostText, pu16GuestText, cwGuestText);
    if (rc != VINF_SUCCESS)
    {
        LogFlow(("vboxClipboardConvertUtf16: vboxClipboardUtf16WinToLin returned %Rrc\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16HostText));
        XtFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    LogFlow(("vboxClipboardConvertUtf16: returning Unicode, original text is %.*ls\n", cwHostText, pu16HostText));
    LogFlow(("vboxClipboardConvertUtf16: converted text is %.*ls\n", cwGuestText - 1, pu16GuestText + 1));
    RTMemFree(reinterpret_cast<void *>(pu16HostText));
    *atomTypeReturn = g_ctx.atomUtf16;
    *pValReturn = reinterpret_cast<XtPointer>(pu16GuestText);
    *pcLenReturn = cwGuestText * 2;
    *piFormatReturn = 8;
    LogFlowFunc(("rc = true\n"));
    return true;
}


/**
 * Satisfy a request from the guest to convert the clipboard text to Utf8.
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
    PRTUTF16 pu16GuestText, pu16HostText;
    char *pcGuestText;
    unsigned cbHostText, cwHostText, cwGuestText, cbGuestTextBuffer;
    size_t cbGuestTextActual;
    int rc;

    LogFlowFunc(("\n"));
    /* Get the host Utf16 data and convert it to Linux Utf16. */
    rc = vboxClipboardReadHostData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   reinterpret_cast<void **>(&pu16HostText), &cbHostText);
    if ((rc != VINF_SUCCESS) || cbHostText == 0)
    {
        LogFlow(("vboxClipboardConvertUtf16: vboxClipboardReadHostData returned %Rrc, %d bytes of data\n", rc, cbHostText));
        g_ctx.hostFormats = 0;
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    cwHostText = cbHostText / 2;
    LogFlow(("vboxClipboardConvertUtf8: original text is %.*ls\n", cwHostText, pu16HostText));
    cwGuestText = vboxClipboardUtf16GetLinSize(pu16HostText, cwHostText);
    pu16GuestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwGuestText * 2));
    if (pu16GuestText == 0)
    {
        RTMemFree(reinterpret_cast<char *>(pu16HostText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    rc = vboxClipboardUtf16WinToLin(pu16HostText, cwHostText, pu16GuestText, cwGuestText);
    RTMemFree(reinterpret_cast<char *>(pu16HostText));
    if (rc != VINF_SUCCESS)
    {
        LogFlow(("vboxClipboardConvertUtf16: vboxClipboardUtf16WinToLin returned %Rrc\n", rc));
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    /* Now convert the Utf16 Linux text to Utf8 */
    cbGuestTextBuffer = cwGuestText * 3;  /* Should always be enough. */
    pcGuestText = XtMalloc(cbGuestTextBuffer);
    if (pcGuestText == 0)
    {
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    /* Our runtime can't cope with endian markers. */
    cbGuestTextActual = 0;
    rc = RTUtf16ToUtf8Ex(pu16GuestText + 1, cwGuestText - 1, &pcGuestText, cbGuestTextBuffer, &cbGuestTextActual);
    RTMemFree(reinterpret_cast<char *>(pu16GuestText));
    if (rc != VINF_SUCCESS)
    {
        XtFree(pcGuestText);
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    LogFlow(("vboxClipboardConvertUtf8: converted text is %.*s\n", cbGuestTextActual, pcGuestText));
    *atomTypeReturn = g_ctx.atomUtf8;
    *pValReturn = reinterpret_cast<XtPointer>(pcGuestText);
    *pcLenReturn = (unsigned long)cbGuestTextActual;
    *piFormatReturn = 8;
    LogFlowFunc(("rc = true\n"));
    return true;
}


/**
 * Satisfy a request from the guest to convert the clipboard text to compound text.
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
    PRTUTF16 pu16GuestText, pu16HostText;
    char *pcUtf8Text;
    unsigned cbHostText, cwHostText, cwGuestText, cbUtf8Text;
    XTextProperty property;
    int rc;

    LogFlowFunc(("\n"));
    /* Get the host Utf16 data and convert it to Linux Utf16. */
    rc = vboxClipboardReadHostData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   reinterpret_cast<void **>(&pu16HostText), &cbHostText);
    if ((rc != VINF_SUCCESS) || cbHostText == 0)
    {
        LogFlow(("vboxClipboardConvertCText: vboxClipboardReadHostData returned %Rrc, %d bytes of data\n", rc, cbHostText));
        g_ctx.hostFormats = 0;
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    LogFlow(("vboxClipboardConvertCText: original text is %.*ls\n", cwHostText, pu16HostText));
    cwHostText = cbHostText / 2;
    cwGuestText = vboxClipboardUtf16GetLinSize(pu16HostText, cwHostText);
    pu16GuestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwGuestText * 2));
    if (pu16GuestText == 0)
    {
        Log(("vboxClipboardConvertCText: out of memory\n"));
        RTMemFree(reinterpret_cast<char *>(pu16HostText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    rc = vboxClipboardUtf16WinToLin(pu16HostText, cwHostText, pu16GuestText, cwGuestText);
    RTMemFree(reinterpret_cast<char *>(pu16HostText));
    if (rc != VINF_SUCCESS)
    {
        LogFlow(("vboxClipboardConvertUtf16: vboxClipboardUtf16WinToLin returned %Rrc\n", rc));
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    /* Now convert the Utf16 Linux text to Utf8 */
    cbUtf8Text = cwGuestText * 3;  /* Should always be enough. */
    pcUtf8Text = reinterpret_cast<char *>(RTMemAlloc(cbUtf8Text));
    if (pcUtf8Text == 0)
    {
        Log(("vboxClipboardConvertCText: out of memory\n"));
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    /* Our runtime can't cope with endian markers. */
    rc = RTUtf16ToUtf8Ex(pu16GuestText + 1, cwGuestText - 1, &pcUtf8Text, cbUtf8Text, 0);
    RTMemFree(reinterpret_cast<char *>(pu16GuestText));
    if (rc != VINF_SUCCESS)
    {
        Log(("vboxClipboardConvertCText: RTUtf16ToUtf8Ex failed: rc=%Rrc\n", rc));
        XtFree(pcUtf8Text);
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    /* And finally (!) convert the Utf8 text to compound text. */
#ifdef RT_OS_SOLARIS
    rc = XmbTextListToTextProperty(XtDisplay(g_ctx.widget), &pcUtf8Text, 1,
                                     XCompoundTextStyle, &property);
#else
    rc = Xutf8TextListToTextProperty(XtDisplay(g_ctx.widget), &pcUtf8Text, 1,
                                     XCompoundTextStyle, &property);
#endif
    RTMemFree(pcUtf8Text);
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
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    LogFlow(("vboxClipboardConvertCText: converted text is %s\n", property.value));
    *atomTypeReturn = property.encoding;
    *pValReturn = reinterpret_cast<XtPointer>(property.value);
    *pcLenReturn = property.nitems;
    *piFormatReturn = property.format;
    LogFlowFunc(("rc = true\n"));
    return true;
}


/**
 * Callback to convert the hosts clipboard data for an application on the guest.  Called by the
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
    g_eClipboardFormat eFormat = INVALID;
    int rc;

    LogFlowFunc(("\n"));
    if (   (*atomSelection != g_ctx.atomClipboard)
        && (*atomSelection != g_ctx.atomPrimary)
       )
    {
        LogFlowFunc(("rc = false\n"));
        return false;
    }
#ifdef DEBUG
    char *szAtomName = XGetAtomName(XtDisplay(g_ctx.widget), *atomTarget);
    if (szAtomName != 0)
    {
        LogFlow(("vboxClipboardConvertProc: request for format %s\n", szAtomName));
        XFree(szAtomName);
    }
    else
        Log(("vboxClipboardConvertProc: request for invalid target atom %d!\n", *atomTarget));
#endif
    if (*atomTarget == g_ctx.atomTargets)
        eFormat = TARGETS;
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
            rc = vboxClipboardConvertTargets(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
            LogFlowFunc(("TARGETS rc=%d\n", rc));
            return rc;
        case UTF16:
            rc = vboxClipboardConvertUtf16(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
            LogFlowFunc(("UTF16 rc=%d\n", rc));
            return rc;
        case UTF8:
            rc = vboxClipboardConvertUtf8(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
            LogFlowFunc(("UTF8 rc=%d\n", rc));
            return rc;
        case CTEXT:
            rc = vboxClipboardConvertCText(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
            LogFlowFunc(("CTEXT rc=%d\n", rc));
            return rc;
        default:
            Log(("vboxClipboardConvertProc: bad format\n"));
            LogFlowFunc(("rc = false\n"));
            return false;
    }
}


static void vboxClipboardLoseProc(Widget, Atom *)
{
    LogFlowFunc(("giving the guest clipboard ownership\n"));
    g_ctx.eOwner = GUEST;
    g_ctx.notifyHost = true;
    LogFlowFunc(("returning\n"));
}


/**
 * The host is taking possession of the shared clipboard.  Called by the HGCM clipboard
 * subsystem.
 *
 * @param u32Formats Clipboard formats the guest is offering
 */
void vboxClipboardFormatAnnounce (uint32_t u32Formats)
{
    g_ctx.hostFormats = u32Formats;
    LogFlowFunc(("u32Formats = %d\n", u32Formats));
    if (u32Formats == 0)
    {
        /* This is just an automatism, not a genuine announcement */
        LogFlowFunc(("returning\n"));
        return;
    }
    LogFlow(("vboxClipboardFormatAnnounce: giving the host clipboard ownership\n"));
    g_ctx.eOwner = HOST;
    g_ctx.guestTextFormat = INVALID;
    g_ctx.guestBitmapFormat = INVALID;
    if (XtOwnSelection(g_ctx.widget, g_ctx.atomClipboard, CurrentTime,
                       vboxClipboardConvertProc, vboxClipboardLoseProc, 0)
                       == True)
        XtOwnSelection(g_ctx.widget, g_ctx.atomPrimary, CurrentTime,
                       vboxClipboardConvertProc, NULL, 0);
    else
    {
        LogFlow(("vboxClipboardFormatAnnounce: returning clipboard ownership to the guest\n"));
        g_ctx.notifyHost = true;
        g_ctx.eOwner = GUEST;
    }
    LogFlowFunc(("returning\n"));
}


/**
 * Called when the host wants to read the guest clipboard.
 *
 * @param u32Format The format that the host would like to receive the data in
 */
int vboxClipboardReadGuestData (uint32_t u32Format)
{
    LogFlowFunc(("u32Format = %d\n", u32Format));

    /*
     * The guest wants to read data in the given format.
     */
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        if (g_ctx.guestTextFormat == INVALID)
        {
            /* No data available. */
            vboxClipboardSendData(0, NULL, 0);
            LogFlowFunc(("sent empty data, returned VINF_SUCCESS\n"));
            return VINF_SUCCESS;
        }
        g_ctx.requestGuestFormat = g_ctx.guestTextFormat;
        /* Send out a request for the data to the current clipboard owner */
        XtGetSelectionValue(g_ctx.widget, g_ctx.atomClipboard, g_ctx.atomGuestTextFormat,
                            vboxClipboardGetProc, 0, CurrentTime);
        /* When the data arrives, the vboxClipboardGetProc callback will be called.  The
           callback will signal the event semaphore when it has processed the data for us. */
    }
    else
    {
        vboxClipboardSendData(0, NULL, 0);
        LogFlowFunc(("sent empty data, returned VERR_NOT_IMPLEMENTED\n"));
        return VERR_NOT_IMPLEMENTED;
    }
    LogFlowFunc(("returned VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/** Terminate the guest side of the shared clipboard. */
void vboxClipboardDestroy (void)
{
    LogFlowFunc(("\n"));

    /* Set the termination flag. */
    XtAppSetExitFlag(g_ctx.appContext);
    LogFlowFunc(("returning\n"));
}


/**
 * The main loop of our clipboard reader.
 */
static int vboxClipboardThread(RTTHREAD /* ThreadSelf */, void * /* pvUser */)
{
    int rc;
    LogFlowFunc(("Starting clipboard thread\n"));

    for (;;)
    {
        uint32_t Msg;
        uint32_t fFormats;
        rc = VbglR3ClipboardGetHostMsg(g_ctx.client, &Msg, &fFormats);
        if (RT_SUCCESS(rc))
        {
            switch (Msg)
            {
                case VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS:
                {
                    /* The host has announced available clipboard formats.
                     * Save the information so that it is available for
                     * future requests from guest applications.
                     */
                    LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS fFormats=%x\n", fFormats));
                    vboxClipboardFormatAnnounce(fFormats);
                    break;
                }

                case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
                {
                    /* The host needs data in the specified format. */
                    LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA fFormats=%x\n", fFormats));
                    vboxClipboardReadGuestData(fFormats);
                    break;
                }

                case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
                {
                    /* The host is terminating. */
                    LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT\n"));
                    vboxClipboardDestroy();
                    rc = VERR_INTERRUPTED;
                    break;
                }

                default:
                    Log(("Unsupported message from host!!!\n"));
            }
        }
        if (rc == VERR_INTERRUPTED)
        {
            /* Wait for termination event. */
            RTSemEventWait(g_ctx.terminating, RT_INDEFINITE_WAIT);
            break;
        }
        if (RT_FAILURE(rc))
        {
            /* Wait a bit before retrying. */
            if (RTSemEventWait(g_ctx.terminating, 1000) == VINF_SUCCESS)
                break;
            continue;
        }

        LogFlow(("processed host event rc = %d\n", rc));
    }
    LogFlowFunc(("rc=%d\n", rc));
    return rc;
}


/**
 * Disconnect the guest clipboard from the host.
 */
void vboxClipboardDisconnect(void)
{
#if 0
    VMMDevHGCMDisconnect request;
#endif
    LogFlowFunc(("\n"));

    AssertReturn(g_ctx.client != 0, (void) 0);
    VbglR3ClipboardDisconnect(g_ctx.client);
    LogFlowFunc(("returning\n"));
}

/**
 * Connect the guest clipboard to the host.
 *
 * @returns VBox status code
 */
int vboxClipboardConnect(void)
{
    int rc;
    LogFlowFunc(("\n"));

    /* Only one client is supported for now */
    AssertReturn(g_ctx.client == 0, VERR_NOT_SUPPORTED);

    /* Initialise the termination semaphore. */
    RTSemEventCreate(&g_ctx.terminating);

    /* Initialise threading in X11 and in Xt. */
    if (!XInitThreads() || !XtToolkitThreadInitialize())
    {
        LogRel(("VBoxClient: error initialising threads in X11, exiting.\n"));
        return VERR_NOT_SUPPORTED;
    }

    rc = VbglR3ClipboardConnect(&g_ctx.client);
    if (RT_FAILURE(rc))
    {
        LogRel(("Error connecting to host. rc=%Rrc\n", rc));
        return rc;
    }
    if (!g_ctx.client)
    {
        LogRel(("Invalid client ID of 0\n"));
        return VERR_NOT_SUPPORTED;
    }
    /* Assume that if the guest clipboard already contains data then the
     * user put it there for a reason. */
    g_ctx.eOwner = GUEST;
    LogFlowFunc(("g_ctx.client=%u rc=%Rrc\n", g_ctx.client, rc));
    return rc;
}


/** We store information about the target formats we can handle in a global vector for internal
    use. */
static void vboxClipboardAddFormat(const char *pszName, g_eClipboardFormat eFormat, unsigned hostFormat)
{
    VBOXCLIPBOARDFORMAT sFormat;
    LogFlowFunc(("pszName=%s, eFormat=%d, hostFormat=%u\n", pszName, eFormat, hostFormat));
    /* Get an atom from the X server for that target format */
    Atom atomFormat = XInternAtom(XtDisplay(g_ctx.widget), pszName, false);
    LogFlow(("vboxClipboardAddFormat: atomFormat=%d\n", atomFormat));
    if (atomFormat == 0)
    {
        LogFlowFunc(("atomFormat=0!  returning...\n"));
        return;
    }
    sFormat.atom   = atomFormat;
    sFormat.format = eFormat;
    sFormat.hostFormat = hostFormat;
    g_ctx.formatList.push_back(sFormat);
    LogFlowFunc(("returning\n"));
}


/** Create the X11 window which we use to interact with the guest clipboard */
static int vboxClipboardCreateWindow(void)
{
    /* Create a window and make it a clipboard viewer. */
    int cArgc = 0;
    char *pcArgv = 0;
    String szFallbackResources[] = { (char*)"*.width: 1", (char*)"*.height: 1", 0 };

    /* Set up the Clipbard application context and main window. */
    g_ctx.widget = XtOpenApplication(&g_ctx.appContext, "VBoxClipboard", 0, 0, &cArgc, &pcArgv,
                                  szFallbackResources, applicationShellWidgetClass, 0, 0);
    AssertMsgReturn(g_ctx.widget != 0, ("Failed to construct the X clipboard window\n"),
                    VERR_ACCESS_DENIED);
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
    g_ctx.atomCText     = XInternAtom(XtDisplay(g_ctx.widget), "COMPOUND_TEXT", false);
    /* And build up the vector of supported formats */
#ifdef USE_UTF16
    vboxClipboardAddFormat("text/plain;charset=ISO-10646-UCS-2", UTF16, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif
#ifdef USE_UTF8
    vboxClipboardAddFormat("UTF8_STRING", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain;charset=UTF-8", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain;charset=utf-8", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("STRING", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("TEXT", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain", UTF8, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif
#ifdef USE_CTEXT
    vboxClipboardAddFormat("COMPOUND_TEXT", CTEXT, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif
    return VINF_SUCCESS;
}

/** Initialise the guest side of the shared clipboard. */
int vboxClipboardMain(void)
{
    int rc;
    LogFlowFunc(("\n"));

    rc = vboxClipboardCreateWindow();
    if (RT_FAILURE(rc))
        return rc;

    rc = RTThreadCreate(&g_ctx.thread, vboxClipboardThread, 0, 0, RTTHREADTYPE_IO,
                        RTTHREADFLAGS_WAITABLE, "SHCLIP");
    AssertRCReturn(rc, rc);
    /* Set up a timer to poll the host clipboard */
    XtAppAddTimeOut(g_ctx.appContext, 200 /* ms */, vboxClipboardTimerProc, 0);

    XtAppMainLoop(g_ctx.appContext);
    g_ctx.formatList.clear();
    XtDestroyApplicationContext(g_ctx.appContext);
    /* Set the termination signal. */
    RTSemEventSignal(g_ctx.terminating);
    LogFlowFunc(("returning %d\n", rc));
    return rc;
}

class ClipboardService : public VBoxClient::Service
{
public:
    virtual const char *getPidFilePath()
    {
        return ".vboxclient-clipboard.pid";
    }
    virtual int run()
    {
        int rc = vboxClipboardConnect();
        if (RT_SUCCESS(rc))
            rc = vboxClipboardMain();
        return rc;
    }
    virtual void cleanup()
    {
        /* Nothing to do. */
    }
};

VBoxClient::Service *VBoxClient::GetClipboardService()
{
    return new ClipboardService;
}
