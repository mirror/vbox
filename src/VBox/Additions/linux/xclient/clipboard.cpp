/** @file
 *
 * Shared Clipboard:
 * Linux guest.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/* The formats which we support in the guest.  These can be deactivated in order to test specific
   code paths. */
#define USE_UTF16
#define USE_UTF8
#define USE_LATIN1

#define LOG_GROUP LOG_GROUP_HGCM

#include <vector>
#include <iostream>

using std::cout;
using std::endl;

#include <VBox/VBoxGuest.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/alloc.h>
#include <iprt/asm.h>        /* For atomic operations */
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <VBox/log.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

// #include "VBoxClipboard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/X.h>

#define VBOX_INIT_CALL(__a, __b, __c) do {                                                             \
    (__a)->hdr.result      = VINF_SUCCESS;                                                             \
    (__a)->hdr.u32ClientID = (__c);                                                       \
    (__a)->hdr.u32Function = (__b);                                                                    \
    (__a)->hdr.cParms      = (sizeof (*(__a)) - sizeof ((__a)->hdr)) / sizeof (HGCMFunctionParameter); \
} while (0)

/** The different clipboard formats which we support. */
enum g_eClipboardFormats
{
    INVALID = 0,
    TARGETS,
    LATIN1,
    UTF8,
    UTF16
};

/** The X11 clipboard uses several names for the same format.  This structure maps an X11
    name to a format. */
typedef struct {
    Atom atom;
    g_eClipboardFormats format;
    unsigned hostFormat;
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
typedef struct
{
    /** The Xt application context structure */
    XtAppContext appContext;

    /** We have a separate thread to wait for Window and Clipboard events */
    RTTHREAD thread;
    /** The Xt widget which we use as our clipboard client.  It is never made visible. */
    Widget widget;
    /** The file descriptor for the VirtualBox device which we use for pushing requests
        to the host. */
    int sendDevice;
    /** The file descriptor for the VirtualBox device which we use for polling requests
        from the host. */
    int receiveDevice;

    /** X11 atom refering to the clipboard: CLIPBOARD */
    Atom atomClipboard;
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
    g_eClipboardFormats guestTextFormat;
    /** Atom corresponding to the guest text format */
    Atom atomGuestTextFormat;
    /** What is the best bitmap format the guest has to offer?  INVALID for none. */
    g_eClipboardFormats guestBitmapFormat;
    /** Atom corresponding to the guest Bitmap format */
    Atom atomGuestBitmapFormat;
    /** What formats does the host have on offer? */
    int hostFormats;
    /** Windows caches the clipboard data it receives.  Since we have no way of knowing whether
        that data is still valid, we always send a "data changed" message after a successful
        transfer to invalidate the cache. */
    bool notifyHost;

    /** Since the clipboard data moves asynchronously, we use an event semaphore to wait for
        it. */
    RTSEMEVENT terminating;
    /** Are we running as a daemon? */
    bool daemonise;

    /** Format which we are reading from the guest clipboard (valid during a request for the
        guest clipboard) */
    g_eClipboardFormats requestGuestFormat;
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

/* Only one client is supported. There seems to be no need for more clients. */
static VBOXCLIPBOARDCONTEXT g_ctx;

static void VBoxHGCMParmUInt32Set(HGCMFunctionParameter *pParm, uint32_t u32)
{
    LogFlowFunc(("pParm=%p, u32=%d\n", pParm, u32));
    pParm->type = VMMDevHGCMParmType_32bit;
    pParm->u.value32 = u32;
}

static int VBoxHGCMParmUInt32Get (HGCMFunctionParameter *pParm, uint32_t *pu32)
{
    LogFlowFunc(("pParm=%p, pu32=%p\n", pParm, pu32));
    if (pParm->type == VMMDevHGCMParmType_32bit)
    {
        *pu32 = pParm->u.value32;
        LogFlowFunc(("rc=VINF_SUCCESS, *pu32=%d\n", *pu32));
        return VINF_SUCCESS;
    }

    LogFlowFunc(("rc=VERR_INVALID_PARAMETER\n"));
    return VERR_INVALID_PARAMETER;
}

static void VBoxHGCMParmPtrSet (HGCMFunctionParameter *pParm, void *pv, uint32_t cb)
{
    LogFlowFunc(("pParm=%p, pv=%p, cb=%d\n", pParm, pv, cb));
    pParm->type                    = VMMDevHGCMParmType_LinAddr;
    pParm->u.Pointer.size          = cb;
    pParm->u.Pointer.u.linearAddr  = (vmmDevHypPtr)pv;
}

/**
 * Transfer clipboard data from the guest to the host
 *
 * @returns VBox result code
 * @param   u32Format The format of the data being sent
 * @param   pv        Pointer to the data being sent
 * @param   cb        Size of the data being sent in bytes
 */
static int vboxClipboardSendData (uint32_t u32Format, void *pv, uint32_t cb)
{
    VBoxClipboardWriteData parms;

    LogFlowFunc(("u32Format=%d, pv=%p, cb=%d\n", u32Format, pv, cb));
    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA, g_ctx.client);

    VBoxHGCMParmUInt32Set (&parms.format, u32Format);
    VBoxHGCMParmPtrSet (&parms.ptr, pv, cb);

    int rc = VERR_DEV_IO_ERROR;
    if (ioctl(g_ctx.sendDevice, IOCTL_VBOXGUEST_HGCM_CALL, &parms) == 0)
    {
        rc = parms.hdr.result;
    }

    LogFlowFunc(("rc=%Vrc\n", rc));
    return rc;
}

/**
 * Get clipboard data from the host
 *
 * @returns VBox result code
 * @param   u32Format The format of the data being requested
 * @retval  ppv       On success, this will point to a buffer to be freed with RTMemFree
 *                    containing the data read if pcb > 0.
 * @retval  pcb       On success, this contains the number of bytes of data returned
 */
static int vboxClipboardReadHostData (uint32_t u32Format, void **ppv, uint32_t *pcb)
{
    VBoxClipboardReadData parms;
    int rc;

    LogFlowFunc(("u32Format=%d, ppv=%p, *pcb=%d\n", u32Format, ppv, *pcb));
    /* Allocate a 1K buffer for receiving the clipboard data to start with.  If this is too small,
       the host will tell us what size of buffer we need, and we will try again with a buffer of
       that size. */
    uint32_t cb = 1024, u32Size;
    void *pv = RTMemAlloc(cb);
    if (pv == 0)
    {
        LogFlowFunc(("rc=VERR_NO_MEMORY\n"));
        return VERR_NO_MEMORY;
    }
    /* Set up the HGCM call structure and make the call to the host. */
    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_READ_DATA, g_ctx.client);
    VBoxHGCMParmUInt32Set (&parms.format, u32Format);
    VBoxHGCMParmPtrSet    (&parms.ptr, pv, cb);
    VBoxHGCMParmUInt32Set (&parms.size, 0);
    if (ioctl(g_ctx.sendDevice, IOCTL_VBOXGUEST_HGCM_CALL, (void*)&parms) < 0)
    {
        RTMemFree(pv);
        LogFlowFunc(("rc=VERR_DEV_IO_ERROR\n"));
        return VERR_DEV_IO_ERROR;
    }
    else if (parms.hdr.result != VINF_SUCCESS)
    {
        RTMemFree(pv);
        LogFlowFunc(("rc=%Vrc\n", parms.hdr.result));
        return parms.hdr.result;
    }
    /* Check whether the buffer we supplied was big enough. */
    rc = VBoxHGCMParmUInt32Get (&parms.size, &u32Size);
    if (rc != VINF_SUCCESS || u32Size == 0)
    {
        *pcb = u32Size;
        RTMemFree(pv);
        *ppv = 0;
        LogFlowFunc(("*pcb=%d, *ppv=%p, rc=%Vrc\n", *pcb, *ppv, rc));
        return rc;
    }
    if (u32Size <= cb)
    {
        /* Our initial 1024 byte buffer was big enough for the clipboard data. */
        *ppv = pv;
        *pcb = u32Size;
        LogFlowFunc(("*pcb=%d, *ppv=%p, rc=VINF_SUCCESS\n", *pcb, *ppv));
        return VINF_SUCCESS;
    }
    /* Else if u32Size > cb, we try again with a buffer of size u32Size. */
    cb = u32Size;
    RTMemFree(pv);
    pv = RTMemAlloc(cb);
    if (pv == 0)
    {
        LogFlowFunc(("rc=VERR_NO_MEMORY\n"));
        return VERR_NO_MEMORY;
    }
    /* Set up the HGCM call structure and make the call to the host. */
    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_READ_DATA, g_ctx.client);
    VBoxHGCMParmUInt32Set (&parms.format, u32Format);
    VBoxHGCMParmPtrSet    (&parms.ptr, pv, cb);
    VBoxHGCMParmUInt32Set (&parms.size, 0);
    if (ioctl(g_ctx.sendDevice, IOCTL_VBOXGUEST_HGCM_CALL, (void*)&parms) < 0)
    {
        RTMemFree(pv);
        LogFlowFunc(("rc=VERR_DEV_IO_ERROR\n"));
        return VERR_DEV_IO_ERROR;
    }
    else if (parms.hdr.result != VINF_SUCCESS)
    {
        RTMemFree(pv);
        LogFlowFunc(("rc=%Vrc\n", parms.hdr.result));
        return parms.hdr.result;
    }
    /* Check whether the buffer we supplied was big enough. */
    rc = VBoxHGCMParmUInt32Get (&parms.size, &u32Size);
    if (rc != VINF_SUCCESS || u32Size == 0)
    {
        *pcb = u32Size;
        RTMemFree(pv);
        *ppv = 0;
        LogFlowFunc(("*pcb=%d, *ppv=%p, rc=%Vrc\n", *pcb, *ppv, rc));
        return rc;
    }
    if (u32Size <= cb)
    {
        /* The buffer was big enough. */
        *ppv = pv;
        *pcb = cb;
        LogFlowFunc(("*pcb=%d, *ppv=%p, rc=VINF_SUCCESS\n", *pcb, *ppv));
        return VINF_SUCCESS;
    }
    /* The buffer was to small again.  Perhaps the clipboard contents changed half-way through
       the operation.  Since I can't say whether or not this is actually an error, we will just
       return size 0. */
    RTMemFree(pv);
    *pcb = 0;
    LogFlowFunc(("*pcb=0 rc=VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/**
 * Convert a Utf16 text with Linux EOLs to Utf16-LE with Windows EOLs, allocating memory
 * for the converted text.  Does no checking for validity.
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
    AssertReturn(pu16Src != 0, VERR_INVALID_PARAMETER);
    cwDest = 0;
    for (i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (pu16Src[i] == LINEFEED)
        {
            ++cwDest;
        }
    }
    /* Leave space for a trailing null in any case */
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
        vboxClipboardSendData (0, 0, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    Log2 (("vboxClipboardGetUtf16: converted string is %.*ls\n", cwDestLen, pu16DestText));
    vboxClipboardSendData (VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
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
    Log2 (("vboxClipboardGetUtf8: converting Utf-8 to Utf-16LE. Original is %.*s\n", cbSourceLen,
           pu8SourceText));
    /* First convert the UTF8 to UTF16 */
    rc = RTStrToUtf16Ex(pu8SourceText, cbSourceLen, &pu16SourceText, 0, &cwSourceLen);
    if (rc != VINF_SUCCESS)
    {
        XtFree(reinterpret_cast<char *>(pValue));
        vboxClipboardSendData (0, 0, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    rc = vboxClipboardUtf16LinToWin(pu16SourceText, cwSourceLen, &pu16DestText, &cwDestLen);
    if (rc != VINF_SUCCESS)
    {
        RTMemFree(reinterpret_cast<void *>(pu16SourceText));
        XtFree(reinterpret_cast<char *>(pValue));
        vboxClipboardSendData (0, 0, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    Log2 (("vboxClipboardGetUtf8: converted string is %.*ls\n", cwDestLen, pu16DestText));
    vboxClipboardSendData (VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                            reinterpret_cast<void *>(pu16DestText), cwDestLen * 2);
    RTMemFree(reinterpret_cast<void *>(pu16SourceText));
    RTMemFree(reinterpret_cast<void *>(pu16DestText));
    XtFree(reinterpret_cast<char *>(pValue));
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
        Log2 (("vboxClipboardGetLatin1: failed to allocate %d bytes!\n", cwDestLen * 2));
        vboxClipboardSendData (0, NULL, 0);
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
    Log2 (("vboxClipboardGetLatin1: converted text is %.*ls\n", cwDestPos, pu16DestText));
    vboxClipboardSendData (VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
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
    /* The X Toolkit may have failed to get the clipboard selection for us. */
    LogFlowFunc(("*pcLen=%lu, *piFormat=%d, requested target format: %d, g_ctx.requestBufferSize=%d\n",
                 *pcLen, *piFormat, g_ctx.requestGuestFormat, g_ctx.requestBufferSize));
    if (*atomType == XT_CONVERT_FAIL)
    {
        vboxClipboardSendData (0, NULL, 0);
        LogFlowFunc(("Xt failed to convert the data.  Sending empty data and returning\n"));
        return;
    }
    /* In which format did we request the clipboard data? */
    unsigned cTextLen = (*pcLen) * (*piFormat) / 8;
    switch (g_ctx.requestGuestFormat)
    {
    case UTF16:
        vboxClipboardGetUtf16(pValue, cTextLen / 2);
        break;
    case UTF8:
    case LATIN1:
    {
        /* If we are given broken Utf-8, we treat it as Latin1.  Is this acceptable? */
        size_t cStringLen;
        char *pu8SourceText = reinterpret_cast<char *>(pValue);

        if ((g_ctx.requestGuestFormat == UTF8)
            && (RTStrUniLenEx(pu8SourceText, *pcLen, &cStringLen) == VINF_SUCCESS))
        {
            vboxClipboardGetUtf8(pValue, cTextLen);
            break;
        }
        else
        {
            vboxClipboardGetLatin1(pValue, cTextLen);
            break;
        }
    }
    default:
        XtFree(reinterpret_cast<char *>(pValue));
        Log (("vboxClipboardGetProc: bad target format\n"));
        vboxClipboardSendData (0, NULL, 0);
        LogFlowFunc(("sending empty data and returning\n"));
        return;
    }
    g_ctx.notifyHost = true;
    LogFlowFunc(("returning\n"));
}

/**
 * Tell the host that new clipboard formats are available
 */
static int vboxClipboardReportFormats (uint32_t u32Formats)
{
    VBoxClipboardFormats parms;
    int rc = VERR_DEV_IO_ERROR;

    LogFlowFunc(("u32Formats=%d\n", u32Formats));
    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_FORMATS, g_ctx.client);

    VBoxHGCMParmUInt32Set (&parms.formats, u32Formats);

    if (ioctl(g_ctx.sendDevice, IOCTL_VBOXGUEST_HGCM_CALL, &parms) == 0)
    {
        rc = parms.hdr.result;
    }

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;
    }

    LogFlowFunc(("rc=%Vrc\n", rc));
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
    unsigned cAtoms = (*pcLen) * (*piFormat) / sizeof(Atom) / 8;
    /* The best clipboard format we have found so far */
    g_eClipboardFormats eBestTarget = INVALID;
    /* The atom corresponding to our best clipboard format found */
    Atom atomBestTarget = None;

    if ((cCalls % 10) == 0)
        Log3 (("vboxClipboardTargetsProc called, cAtoms=%d\n", cAtoms));
    if (*atomType == XT_CONVERT_FAIL)
    {
        Log (("vboxClipboardTargetsProc: reading clipboard from guest, X toolkit failed to convert the selection\n"));
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
                Log3 (("vboxClipboardTargetsProc: the guest offers target %s\n", szAtomName));
            XFree(szAtomName);
        }
        else
        {
            if ((cCalls % 10) == 0)
                Log3 (("vboxClipboardTargetsProc: the guest returned a bad atom: %d\n",
                       atomTargets[i]));
        }
#endif
    }
    XtFree(reinterpret_cast<char *>(pValue));
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
            Log2 (("vboxClipboardTargetsProc: switching to guest text target %s\n", szAtomName));
            XFree(szAtomName);
        }
        else
        {
            Log2(("vboxClipboardTargetsProc: no supported host text target found.\n"));
        }
#endif
        g_ctx.guestTextFormat = eBestTarget;
        if (eBestTarget != INVALID)
            u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        vboxClipboardReportFormats(u32Formats);
        g_ctx.notifyHost = false;
    }
    ++cCalls;
}

/**
 * This callback is called every 200ms to check the contents of the guest clipboard.
 */
static void vboxClipboardTimerProc(XtPointer /* pUserData */, XtIntervalId * /* hTimerId */)
{
    static int cCalls = 0;
    Log3 (("vboxClipboardTimerProc called\n"));
    /* Get the current clipboard contents */
    if (g_ctx.eOwner == GUEST)
    {
        if ((cCalls % 10) == 0)
            Log3 (("vboxClipboardTimerProc: requesting the targets that the guest clipboard offers\n"));
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
    Log2(("vboxClipboardConvertTargets: uListSize=%u\n", uListSize));
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
    *piFormatReturn = sizeof(Atom) * 8;
    LogFlowFunc(("returning true\n"));
    return true;
}

/**
 * Get the size of the buffer needed to hold a Utf16 string with Linux EOLs converted from
 * a Utf16 string with Windows EOLs.
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

    LogFlowFunc(("pu16Src=%.*ls, cwSrc=%u", cwSrc, pu16Src, cwSrc));
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
        {
            /* The terminating zero is included in the size */
            ++cwDest;
            break;
        }
    }
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
    AssertReturn(pu16Src != 0, VERR_INVALID_PARAMETER);
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
    {
        cwDestPos = 0;
    }
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
        {
            ++i;
        }
        pu16Dest[cwDestPos] = pu16Src[i];
        if (pu16Src[i] == 0)
        {
            break;
        }
    }
    LogFlowFunc(("set string %.*ls.  Returning\n", cwDestPos, pu16Dest));
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
        Log (("vboxClipboardConvertUtf16: vboxClipboardReadHostData returned %Vrc, %d bytes of data\n", rc, cbHostText));
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
        Log2(("vboxClipboardConvertUtf16: vboxClipboardUtf16WinToLin returned %Vrc\n", rc));
        RTMemFree(reinterpret_cast<void *>(pu16HostText));
        XtFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    Log2 (("vboxClipboardConvertUtf16: returning Unicode, original text is %.*ls\n",
           cwHostText, pu16HostText));
    Log2 (("vboxClipboardConvertUtf16: converted text is %.*ls\n", cwGuestText - 1,
           pu16GuestText + 1));
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
    unsigned cbHostText, cwHostText, cwGuestText, cbGuestText;
    int rc;

    LogFlowFunc(("\n"));
    /* Get the host Utf16 data and convert it to Linux Utf16. */
    rc = vboxClipboardReadHostData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   reinterpret_cast<void **>(&pu16HostText), &cbHostText);
    if ((rc != VINF_SUCCESS) || cbHostText == 0)
    {
        Log (("vboxClipboardConvertUtf16: vboxClipboardReadHostData returned %Vrc, %d bytes of data\n", rc, cbGuestText));
        g_ctx.hostFormats = 0;
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    cwHostText = cbHostText / 2;
    cwGuestText = vboxClipboardUtf16GetLinSize(pu16HostText, cwHostText);
    pu16GuestText = reinterpret_cast<PRTUTF16>(RTMemAlloc(cwGuestText * 2));
    if (pu16GuestText == 0)
    {
        RTMemFree(reinterpret_cast<char *>(pu16HostText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    rc = vboxClipboardUtf16WinToLin(pu16HostText, cwHostText, pu16GuestText, cwGuestText);
    if (rc != VINF_SUCCESS)
    {
        Log2(("vboxClipboardConvertUtf16: vboxClipboardUtf16WinToLin returned %Vrc\n", rc));
        RTMemFree(reinterpret_cast<char *>(pu16HostText));
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    /* Now convert the Utf16 Linux text to Utf8 */
    cbGuestText = cwGuestText * 3;  /* Should always be enough. */
    if (rc != VINF_SUCCESS)
    {
        RTMemFree(reinterpret_cast<char *>(pu16HostText));
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    pcGuestText = XtMalloc(cbGuestText);
    /* Our runtime can't cope with endian markers. */
    rc = RTUtf16ToUtf8Ex(pu16GuestText + 1, cwGuestText - 1, &pcGuestText, cbGuestText, 0);
    if (rc != VINF_SUCCESS)
    {
        RTMemFree(reinterpret_cast<char *>(pu16HostText));
        RTMemFree(reinterpret_cast<char *>(pu16GuestText));
        XtFree(pcGuestText);
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    Log2 (("vboxClipboardConvertUtf8: returning Utf-8, original text is %.*ls\n", cwHostText,
           pu16HostText));
    Log2 (("vboxClipboardConvertUtf8: converted text is %.*s\n", cbGuestText, pcGuestText));
    RTMemFree(reinterpret_cast<char *>(pu16HostText));
    RTMemFree(reinterpret_cast<char *>(pu16GuestText));
    *atomTypeReturn = g_ctx.atomUtf8;
    *pValReturn = reinterpret_cast<XtPointer>(pcGuestText);
    *pcLenReturn = cbGuestText;
    *piFormatReturn = 8;
    LogFlowFunc(("rc = true\n"));
    return true;
}

/**
 * Satisfy a request from the guest to convert the clipboard text to Latin1.
 *
 * @returns true if we successfully convert the data to the format requested, false otherwise.
 *
 * @param atomTypeReturn The type of the data we are returning
 * @param pValReturn     A pointer to the data we are returning.  This should be to memory
 *                       allocated by XtMalloc, which will be freed by the toolkit later
 * @param pcLenReturn    The length of the data we are returning
 * @param piFormatReturn The format (8bit, 16bit, 32bit) of the data we are returning
 */
static Boolean vboxClipboardConvertLatin1(Atom *atomTypeReturn, XtPointer *pValReturn,
                                          unsigned long *pcLenReturn, int *piFormatReturn)
{
    enum { LINEFEED = 0xa, CARRIAGERETURN = 0xd };
    PRTUTF16 pu16HostText;
    unsigned cbHostText, cwHostText, cbGuestPos = 0, cbGuestText;
    unsigned char *pcGuestText;
    int rc;

    LogFlowFunc(("\n"));
    /* Get the host UTF16 data */
    rc = vboxClipboardReadHostData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   reinterpret_cast<void **>(&pu16HostText), &cbHostText);
    if ((rc != VINF_SUCCESS) || cbHostText == 0)
    {
        Log (("vboxClipboardConvertUtf16: vboxClipboardReadHostData returned %Vrc, %d bytes of data\n", rc, cbGuestText));
        g_ctx.hostFormats = 0;
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    cwHostText = cbHostText / 2;
    cbGuestText = cwHostText;
    pcGuestText = reinterpret_cast<unsigned char *>(XtMalloc(cbGuestText));
    if (pcGuestText == 0)
    {
        RTMemFree(reinterpret_cast<void *>(pu16HostText));
        LogFlowFunc(("rc = false\n"));
        return false;
    }
    for (unsigned i = 0; i < cwHostText; ++i, ++cbGuestPos)
    {
        if (   (i + 1 < cwHostText)
            && (pu16HostText[i] == CARRIAGERETURN)
            && (pu16HostText[i + 1] == LINEFEED))
            ++i;
        if (pu16HostText[i] < 256)
            pcGuestText[cbGuestPos] = pu16HostText[i];
        else
            /* Any better ideas as to how to do this? */
            pcGuestText[cbGuestPos] = '.';
    }
    Log (("vboxClipboardConvertLatin1: returning Latin-1, original text is %.*ls\n", cwHostText,
          pu16HostText));
    Log (("vboxClipboardConvertLatin1: converted text is %.*s\n", cbGuestPos,
          pcGuestText));
    RTMemFree(reinterpret_cast<void *>(pu16HostText));
    *atomTypeReturn = XA_STRING;
    *pValReturn = reinterpret_cast<XtPointer>(pcGuestText);
    *pcLenReturn = cbGuestPos;
    *piFormatReturn = 8;
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
    g_eClipboardFormats eFormat = INVALID;
    int rc;

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
    if (*atomTarget == g_ctx.atomCText)
    {
        /* We do not support compound text conversion.  However, as we are required to do so by
           the X11 standards, we return Latin-1 if we are asked to do so anyway. */
        LogRel(("An application on your guest system has asked for clipboard data in COMPOUND_TEXT format.  Since VirtualBox does not support this format, international characters will get lost.  Please set up your guest applications to use Unicode!\n"));
        eFormat = LATIN1;
    }
    switch (eFormat)
    {
    case TARGETS:
        rc = vboxClipboardConvertTargets(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
        LogFlowFunc(("rc=%d\n", rc));
        return rc;
    case UTF16:
        rc = vboxClipboardConvertUtf16(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
        LogFlowFunc(("rc=%d\n", rc));
        return rc;
    case UTF8:
        rc = vboxClipboardConvertUtf8(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
        LogFlowFunc(("rc=%d\n", rc));
        return rc;
    case LATIN1:
        rc = vboxClipboardConvertLatin1(atomTypeReturn, pValReturn, pcLenReturn, piFormatReturn);
        LogFlowFunc(("rc=%d\n", rc));
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
 * @param u32Formats Clipboard formats the the guest is offering
 */
void vboxClipboardFormatAnnounce (uint32_t u32Formats)
{
    g_ctx.hostFormats = u32Formats;
    LogFlowFunc(("u32Formats = %d\n", u32Formats));
    if (u32Formats == 0)
    {
        /* This is just an automatism, not a genuine anouncement */
        LogFlowFunc(("returning\n"));
        return;
    }
    Log2 (("vboxClipboardFormatAnnounce: giving the host clipboard ownership\n"));
    g_ctx.eOwner = HOST;
    g_ctx.guestTextFormat = INVALID;
    g_ctx.guestBitmapFormat = INVALID;
    if (XtOwnSelection(g_ctx.widget, g_ctx.atomClipboard, CurrentTime, vboxClipboardConvertProc,
                       vboxClipboardLoseProc, 0) != True)
    {
        Log2 (("vboxClipboardFormatAnnounce: returning clipboard ownership to the guest\n"));
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
            vboxClipboardSendData (0, NULL, 0);
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
        vboxClipboardSendData (0, NULL, 0);
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
    int rc = VINF_SUCCESS;
    LogFlowFunc(("Starting clipboard thread\n"));

    for (;;)
    {
        VBoxClipboardGetHostMsg parms;
        VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG, g_ctx.client);

        VBoxHGCMParmUInt32Set (&parms.msg, 0);
        VBoxHGCMParmUInt32Set (&parms.formats, 0);
        if (ioctl(g_ctx.receiveDevice, IOCTL_VBOXGUEST_HGCM_CALL, (void*)&parms) < 0)
        {
            Log(("Failed to call the driver for host message.\n"));

            /* Wait a bit before retrying. */
            if (RTSemEventWait(g_ctx.terminating, 1000) == VINF_SUCCESS)
                break;
            continue;
        }
        rc = parms.hdr.result;
        if (VBOX_SUCCESS (rc))
        {
            uint32_t u32Msg;
            uint32_t u32Formats;

            rc = VBoxHGCMParmUInt32Get (&parms.msg, &u32Msg);

            if (VBOX_SUCCESS (rc))
            {
                rc = VBoxHGCMParmUInt32Get (&parms.formats, &u32Formats);

                if (VBOX_SUCCESS (rc))
                {
                    Log(("vboxClipboardHostEvent u32Msg %d, u32Formats %d\n", u32Msg, u32Formats));

                    switch (u32Msg)
                    {
                        case VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS:
                        {
                            /* The host has announced available clipboard formats.
                             * Save the information so that it is available for
                             * future requests from guest applications.
                             */
                            vboxClipboardFormatAnnounce(u32Formats);
                        } break;
                        case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
                        {
                            /* The host needs data in the specified format.
                             */
                            vboxClipboardReadGuestData(u32Formats);
                        } break;
                        case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
                        {
                            /* The host is terminating.
                             */
                            vboxClipboardDestroy();
                            rc = VERR_INTERRUPTED;
                        } break;
                        default:
                        {
                            Log(("Unsupported message from host!!!\n"));
                        }
                    }
                }
            }
        }
        if (rc == VERR_INTERRUPTED)
        {
            /* Wait for termination event. */
            RTSemEventWait(g_ctx.terminating, RT_INDEFINITE_WAIT);
            break;
        }
        if (VBOX_FAILURE (rc))
        {
            /* Wait a bit before retrying. */
            if (RTSemEventWait(g_ctx.terminating, 1000) == VINF_SUCCESS)
                break;
            continue;
        }

        Log(("processed host event rc = %d\n", rc));
    }
    LogFlowFunc(("rc=%d\n", rc));
    return rc;
}

/**
 * Disconnect the guest clipboard from the host.
 */
void vboxClipboardDisconnect (void)
{
#if 0
    VMMDevHGCMDisconnect request;
#endif
    LogFlowFunc(("\n"));

    AssertReturn(g_ctx.client != 0, (void) 0);
#if 0
    /* Currently, disconnecting is not needed, as the new "connect clipboard"
       ioctl in the Guest Additions kernel module disconnects the last
       connection made automatically.  The reason for this change was that
       currently only one clipboard connection is allowed, and that if the
       client holding that connection was terminated too abruptly, the
       information needed to disconnect that connection was lost.  If the
       subsystem is ever changed to allow several connections, this will have
       to be rethought. */
    vmmdevInitRequest((VMMDevRequestHeader*)&request, VMMDevReq_HGCMDisconnect);
    request.u32ClientID = g_ctx.client;
    ioctl(g_ctx.sendDevice, IOCTL_VBOXGUEST_VMMREQUEST, (void*)&request);
#endif
    LogFlowFunc(("returning\n"));
}

int vboxClipboardXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    LogFlowFunc(("\n"));
    if (pError->error_code == BadAtom)
    {
        /* This can be triggered in debug builds if a guest application passes a bad atom
           in its list of supported clipboard formats.  As such it is harmless. */
        LogFlowFunc(("ignoring BadAtom error and returning\n"));
        return 0;
    }
    vboxClipboardDisconnect();
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    if (g_ctx.daemonise == 0)
    {
        cout << "An X Window protocol error occurred: " << errorText << endl
            << "  Request code: " << int(pError->request_code) << endl
            << "  Minor code: " << int(pError->minor_code) << endl
            << "  Serial number of the failed request: " << int(pError->serial) << endl;
    }
    Log(("%s: an X Window protocol error occurred: %s.  Request code: %d, minor code: %d, serial number: %d\n",
         __PRETTY_FUNCTION__, pError->error_code, pError->request_code, pError->minor_code,
         pError->serial));
    LogFlowFunc(("exiting\n"));
    exit(1);
}


/**
  * Connect the guest clipboard to the host.
  *
  * @returns RT status code
  */
int vboxClipboardConnect (void)
{
    LogFlowFunc(("\n"));
    int rc;
    /* Only one client is supported for now */
    AssertReturn(g_ctx.client == 0, VERR_NOT_SUPPORTED);

    rc = ioctl(g_ctx.sendDevice, IOCTL_VBOXGUEST_CLIPBOARD_CONNECT, (void*)&g_ctx.client);
    if (rc >= 0)
    {
        if (g_ctx.client == 0)
        {
            cout << "We got an invalid client ID of 0!" << endl;
            return VERR_NOT_SUPPORTED;
        }
        g_ctx.eOwner = HOST;
    }
    else
    {
        Log(("Error connecting to host.  rc = %d (%s)\n", rc, strerror(-rc)));
        cout << "Unable to connect to the host system." << endl;
        LogFlowFunc(("returned VERR_NOT_SUPPORTED\n"));
        return VERR_NOT_SUPPORTED;
    }
    /* Set an X11 error handler, so that we don't die when we get BadAtom errors. */
    XSetErrorHandler(vboxClipboardXLibErrorHandler);
    LogFlowFunc(("returned VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/** We store information about the target formats we can handle in a global vector for internal
    use. */
static void vboxClipboardAddFormat(const char *pszName, g_eClipboardFormats eFormat,
                                   unsigned hostFormat)
{
    VBOXCLIPBOARDFORMAT sFormat;
    LogFlowFunc(("pszName=%s, eFormat=%d, hostFormat=%u\n", pszName, eFormat, hostFormat));
    /* Get an atom from the X server for that target format */
    Atom atomFormat = XInternAtom(XtDisplay(g_ctx.widget), pszName, false);
    Log2(("vboxClipboardAddFormat: atomFormat=%d\n", atomFormat));
    if (atomFormat == 0)
    {
        LogFlowFunc(("atomFormat=0!  returning...\n"));
        return;
    }
    sFormat.atom   = atomFormat;
    sFormat.format = eFormat;
    sFormat.hostFormat = hostFormat;
    g_ctx.formatList.push_back(sFormat);
    LogFlowFunc (("returning\n"));
}

/** Create the X11 window which we use to interact with the guest clipboard */
static int vboxClipboardCreateWindow(void)
{
    /* Create a window and make it a clipboard viewer. */
    int cArgc = 0;
    char *pcArgv = 0;
    String szFallbackResources[] = { "*.width: 1", "*.height: 1", 0 };

    /* Set up the Clipbard application context and main window. */
    g_ctx.widget = XtOpenApplication(&g_ctx.appContext, "VBoxClipboard", 0, 0, &cArgc, &pcArgv,
                                  szFallbackResources, applicationShellWidgetClass, 0, 0);
    AssertMsgReturn(g_ctx.widget != 0, ("Failed to construct the X clipboard window\n"),
                    VERR_ACCESS_DENIED);
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
#endif
#ifdef USE_LATIN1
    vboxClipboardAddFormat("STRING", LATIN1,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("TEXT", LATIN1,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
    vboxClipboardAddFormat("text/plain", LATIN1,
                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);
#endif
    return VINF_SUCCESS;
}

/** Initialise the guest side of the shared clipboard. */
int vboxClipboardMain (void)
{
    int rc;
    LogFlowFunc(("\n"));

    rc = vboxClipboardCreateWindow();
    if (VBOX_FAILURE(rc))
    {
        return rc;
    }

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


/**
 * Become a daemon process
 */
void vboxDaemonise(void)
{
    /* First fork and exit the parent process, so that we are sure we are not session leader. */
    if (fork() != 0)
    {
        exit(0);
    }
    /* Detach from the controlling terminal by creating our own session. */
    setsid();
    /* And change to the root directory to avoid holding the one we were started in open. */
    chdir("/");
    /* Close the standard files. */
    close(0);
    close(1);
    close(2);
}

int main(int argc, char *argv[])
{
    int rc;

    /* Parse our option(s) */
    g_ctx.daemonise = 1;
    while (1)
    {
        static struct option sOpts[] =
        {
            {"nodaemon", 0, 0, 'd'},
            {0, 0, 0, 0}
        };
        int cOpt = getopt_long(argc, argv, "", sOpts, 0);
        if (cOpt == -1)
        {
            if (optind < argc)
            {
                cout << "Unrecognized command line argument: " << argv[argc] << endl;
                exit(1);
            }
            break;
        }
        switch(cOpt)
        {
        case 'd':
            g_ctx.daemonise = 0;
            break;
        default:
            cout << "Unrecognized command line option: " << static_cast<char>(cOpt) << endl;
        case '?':
            exit(1);
        }
    }
    /* Initialise our runtime before all else. */
    RTR3Init(false);
    LogFlowFunc(("\n"));
    /* Initialise threading in Xt before we start any new threads. */
    XtToolkitThreadInitialize();
    /* Initialise the termination semaphore. */
    RTSemEventCreate(&g_ctx.terminating);
    /* Open a connection to the driver for sending requests. */
    g_ctx.sendDevice = open(VBOXGUEST_DEVICE_NAME, O_RDWR, 0);
    if (g_ctx.sendDevice < 0)
    {
        Log(("Error opening kernel module! errno = %d\n", errno));
        cout << "Failed to open the VirtualBox device in the guest." << endl;
        LogFlowFunc(("returning 1\n"));
        return 1;
    }
    /* Open a connection to the driver for polling for host requests. */
    g_ctx.receiveDevice = open(VBOXGUEST_DEVICE_NAME, O_RDWR, 0);
    if (g_ctx.receiveDevice < 0)
    {
        Log(("Error opening kernel module! rc = %d\n", errno));
        cout << "Failed to open the VirtualBox device in the guest" << endl;
        LogFlowFunc(("returning 1\n"));
        return 1;
    }
    /* Connect to the host clipboard. */
    rc = vboxClipboardConnect();
    if (rc != VINF_SUCCESS)
    {
        Log(("vboxClipboardConnect failed with rc = %d\n", rc));
        cout << "Failed to connect to the host clipboard." << endl;
        LogFlowFunc(("returning 1\n"));
        return 1;
    }
    if (g_ctx.daemonise == 1)
    {
        vboxDaemonise();
    }
    vboxClipboardMain();
    vboxClipboardDisconnect();
    LogFlowFunc(("returning 0\n"));
    return 0;
}
