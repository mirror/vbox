/** @file
 * Shared Clipboard - Common Guest and Host Code.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>

/** A single Shared Clipboard format. */
typedef uint32_t VBOXCLIPBOARDFORMAT;
/** Pointer to a single Shared Clipboard format. */
typedef VBOXCLIPBOARDFORMAT *PVBOXCLIPBOARDFORMAT;

/** Bit map of Shared Clipboard formats. */
typedef uint32_t VBOXCLIPBOARDFORMATS;
/** Pointer to a bit map of Shared Clipboard formats. */
typedef VBOXCLIPBOARDFORMATS *PVBOXCLIPBOARDFORMATS;

/**
 * Supported data formats for Shared Clipboard. Bit mask.
 */
/** No format set. */
#define VBOX_SHARED_CLIPBOARD_FMT_NONE          0
/** Shared Clipboard format is an Unicode text. */
#define VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT   UINT32_C(0x01)
/** Shared Clipboard format is bitmap (BMP / DIB). */
#define VBOX_SHARED_CLIPBOARD_FMT_BITMAP        UINT32_C(0x02)
/** Shared Clipboard format is HTML. */
#define VBOX_SHARED_CLIPBOARD_FMT_HTML          UINT32_C(0x04)
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/** Shared Clipboard format is an URI list. */
#define VBOX_SHARED_CLIPBOARD_FMT_URI_LIST      UINT32_C(0x08)
#endif

/**
 * The host messages for the guest.
 */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT                   1
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA              2
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS         3

/** Initiates a new transfer (read / write) on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START     4
/** Open an URI list on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN          5
/** Closes a formerly opened URI list on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE         6
/** Reads a list header from the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ      7
/** Writes a list header to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE     8
/** Reads a list entry from the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ    9
/** Writes a list entry to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE   10
/** Open an URI object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_OPEN           11
/** Closes a formerly opened URI object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_CLOSE          12
/** Reads from an object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_READ           13
/** Writes to an object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE          14
/** Indicates that the host has canceled a transfer. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_CANCEL             15
/** Indicates that the an unrecoverable error on the host occurred . */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ERROR              16

/**
 * Enumeration to specify the Shared Clipboard URI source type.
 */
typedef enum SHAREDCLIPBOARDSOURCE
{
    /** Invalid source type. */
    SHAREDCLIPBOARDSOURCE_INVALID = 0,
    /** Source is local. */
    SHAREDCLIPBOARDSOURCE_LOCAL,
    /** Source is remote. */
    SHAREDCLIPBOARDSOURCE_REMOTE,
    /** The usual 32-bit hack. */
    SHAREDCLIPBOARDSOURCE_32Bit_Hack = 0x7fffffff
} SHAREDCLIPBOARDSOURCE;

enum
{
    /** The number of milliseconds before the clipboard times out. */
#ifndef TESTCASE
    CLIPBOARD_TIMEOUT = 5000
#else
    CLIPBOARD_TIMEOUT = 1
#endif
};

/** Opaque data structure for the X11/VBox frontend/glue code. */
struct _VBOXCLIPBOARDCONTEXT;
typedef struct _VBOXCLIPBOARDCONTEXT VBOXCLIPBOARDCONTEXT;
typedef struct _VBOXCLIPBOARDCONTEXT *PVBOXCLIPBOARDCONTEXT;

/** Opaque data structure for the X11/VBox backend code. */
struct _CLIPBACKEND;
typedef struct _CLIPBACKEND CLIPBACKEND;

/** Opaque request structure for clipboard data.
 * @todo All use of single and double underscore prefixes is banned! */
struct _CLIPREADCBREQ;
typedef struct _CLIPREADCBREQ CLIPREADCBREQ;

/* APIs exported by the X11 backend */
extern CLIPBACKEND *ClipConstructX11(VBOXCLIPBOARDCONTEXT *pFrontend, bool fHeadless);
extern void ClipDestructX11(CLIPBACKEND *pBackend);
#ifdef __cplusplus
extern int ClipStartX11(CLIPBACKEND *pBackend, bool grab = false);
#else
extern int ClipStartX11(CLIPBACKEND *pBackend, bool grab);
#endif
extern int ClipStopX11(CLIPBACKEND *pBackend);
extern int ClipAnnounceFormatToX11(CLIPBACKEND *pBackend,
                                   VBOXCLIPBOARDFORMATS vboxFormats);
extern int ClipRequestDataFromX11(CLIPBACKEND *pBackend, VBOXCLIPBOARDFORMATS vboxFormat,
                                  CLIPREADCBREQ *pReq);

/* APIs exported by the X11/VBox frontend */
extern int ClipRequestDataForX11(VBOXCLIPBOARDCONTEXT *pCtx,
                                 uint32_t u32Format, void **ppv,
                                 uint32_t *pcb);
extern void ClipReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx,
                                 uint32_t u32Formats);
extern void ClipCompleteDataRequestFromX11(VBOXCLIPBOARDCONTEXT *pCtx, int rc,
                                           CLIPREADCBREQ *pReq, void *pv,
                                           uint32_t cb);
#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_h */
