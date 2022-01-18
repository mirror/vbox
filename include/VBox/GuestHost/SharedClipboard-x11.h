/** @file
 * Shared Clipboard - Common X11 code.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_x11_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_x11_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <X11/Intrinsic.h>

#include <iprt/thread.h>

#include <VBox/GuestHost/SharedClipboard.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include <VBox/GuestHost/SharedClipboard-transfers.h>
#endif

/**
 * The maximum number of simultaneous connections to shared clipboard service.
 * This constant limits amount of GUEST -> HOST connections to shared clipboard host service
 * for X11 host only. Once amount of connections reaches this number, all the
 * further attempts to CONNECT will be dropped on an early stage. Possibility to connect
 * is available again after one of existing connections is closed by DISCONNECT call.
 */
#define VBOX_SHARED_CLIPBOARD_X11_CONNECTIONS_MAX   (20)

/** Enables the Xt busy / update handling. */
#define VBOX_WITH_SHARED_CLIPBOARD_XT_BUSY      1

/**
 * Enumeration for all clipboard formats which we support on X11.
 */
typedef enum _SHCLX11FMT
{
    SHCLX11FMT_INVALID = 0,
    SHCLX11FMT_TARGETS,
    SHCLX11FMT_TEXT,  /* Treat this as UTF-8, but it may really be ascii */
    SHCLX11FMT_UTF8,
    SHCLX11FMT_BMP,
    SHCLX11FMT_HTML
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    , SHCLX11FMT_URI_LIST
#endif
} SHCLX11FMT;

/**
 * The table maps X11 names to data formats
 * and to the corresponding VBox clipboard formats.
 */
typedef struct SHCLX11FMTTABLE
{
    /** The X11 atom name of the format (several names can match one format). */
    const char     *pcszAtom;
    /** The format corresponding to the name. */
    SHCLX11FMT      enmFmtX11;
    /** The corresponding VBox clipboard format. */
    SHCLFORMAT      uFmtVBox;
} SHCLX11FMTTABLE;

#define NIL_CLIPX11FORMAT   0

/** Defines an index of the X11 clipboad format table. */
typedef unsigned SHCLX11FMTIDX;

/**
 * Structure for maintaining a Shared Clipboard context on X11 platforms.
 */
typedef struct _SHCLX11CTX
{
    /** Opaque data structure describing the front-end. */
    PSHCLCONTEXT pFrontend;
    /** Is an X server actually available? */
    bool fHaveX11;
    /** The X Toolkit application context structure. */
    XtAppContext pAppContext;

    /** We have a separate thread to wait for window and clipboard events. */
    RTTHREAD Thread;
    /** Flag indicating that the thread is in a started state. */
    bool fThreadStarted;

    /** The X Toolkit widget which we use as our clipboard client.  It is never made visible. */
    Widget pWidget;

    /** Should we try to grab the clipboard on startup? */
    bool fGrabClipboardOnStart;

    /** The best text format X11 has to offer, as an index into the formats table. */
    SHCLX11FMTIDX idxFmtText;
    /** The best bitmap format X11 has to offer, as an index into the formats table. */
    SHCLX11FMTIDX idxFmtBmp;
    /** The best HTML format X11 has to offer, as an index into the formats table. */
    SHCLX11FMTIDX idxFmtHTML;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /** The best HTML format X11 has to offer, as an index into the formats table. */
    SHCLX11FMTIDX   idxFmtURI;
# ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS_HTTP
    /** HTTP transfer context data. */
    SHCLHTTPCONTEXT HttpCtx;
# endif
#endif
    /** What kind of formats does VBox have to offer? */
    SHCLFORMATS vboxFormats;
    /** Cache of the last unicode data that we received. */
    void *pvUnicodeCache;
    /** Size of the unicode data in the cache. */
    uint32_t cbUnicodeCache;
    /** When we wish the clipboard to exit, we have to wake up the event
     * loop.  We do this by writing into a pipe.  This end of the pipe is
     * the end that another thread can write to. */
    int wakeupPipeWrite;
    /** The reader end of the pipe. */
    int wakeupPipeRead;
    /** A pointer to the XFixesSelectSelectionInput function. */
    void (*fixesSelectInput)(Display *, Window, Atom, unsigned long);
    /** The first XFixes event number. */
    int fixesEventBase;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_XT_BUSY
    /** XtGetSelectionValue on some versions of libXt isn't re-entrant
     * so block overlapping requests on this flag. */
    bool fXtBusy;
    /** If a request is blocked on the previous flag, set this flag to request
     * an update later - the first callback should check and clear this flag
     * before processing the callback event. */
    bool fXtNeedsUpdate;
#endif
} SHCLX11CTX, *PSHCLX11CTX;

/** @name Shared Clipboard APIs for X11.
 * @{
 */
int ShClX11Init(PSHCLX11CTX pCtx, PSHCLCONTEXT pParent, bool fHeadless);
void ShClX11Destroy(PSHCLX11CTX pCtx);
int ShClX11ThreadStart(PSHCLX11CTX pCtx, bool grab);
int ShClX11ThreadStop(PSHCLX11CTX pCtx);
int ShClX11ReportFormatsToX11(PSHCLX11CTX pCtx, SHCLFORMATS vboxFormats);
int ShClX11ReadDataFromX11(PSHCLX11CTX pCtx, SHCLFORMATS vboxFormat, CLIPREADCBREQ *pReq);
/** @} */

/** @name Shared Clipboard callbacks which have to be implemented by tools using the X11
 *        clipboard, e.g. VBoxClient (on guest side) or the X11 host service backend.
 * @{
 */
/**
 * Callback for reporting supported formats of current clipboard data from X11 to VBox.
 *
 * @note   Runs in Xt event thread.
 *
 * @param  pCtx                 Opaque context pointer for the glue code.
 * @param  fFormats             The formats available.
 */
DECLCALLBACK(void) ShClX11ReportFormatsCallback(PSHCLCONTEXT pCtx, SHCLFORMATS fFormats);

/**
 * Callback for requesting clipboard data for X11.
 * The function will be invoked for every single target the clipboard requests.
 *
 * @note Runs in Xt event thread.
 *
 * @returns VBox status code. VERR_NO_DATA if no data available.
 * @param   pCtx                Pointer to the host clipboard structure.
 * @param   uFmt                The format in which the data should be transferred
 *                              (VBOX_SHCL_FMT_XXX).
 * @param   ppv                 Returns an allocated buffer with data read from the guest on success.
 *                              Needs to be free'd with RTMemFree() by the caller.
 * @param   pcb                 Returns the amount of data read (in bytes) on success.
 */
DECLCALLBACK(int) ShClX11RequestDataCallback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, uint32_t *pcb);

/**
 * Callback for reporting that clipboard data from X11 is available.
 *
 * @param  pCtx                 Our context information.
 * @param  rcCompletion         The completion status of the request.
 * @param  pReq                 The request structure that we passed in when we started
 *                              the request.  We RTMemFree() this in this function.
 * @param  pv                   The clipboard data returned from X11 if the request succeeded (see @a rcCompletion).
 * @param  cb                   The size of the data in @a pv.
 */
DECLCALLBACK(void) ShClX11ReportDataCallback(PSHCLCONTEXT pCtx, int rcCompletion,
                                             CLIPREADCBREQ *pReq, void *pv, uint32_t cb);
/** @} */

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_x11_h */

