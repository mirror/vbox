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
#include <iprt/list.h>
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
 * Structure for keeping a generic Shared Clipboard data block.
 */
typedef struct _SHAREDCLIPBOARDDATABLOCK
{
    /** Clipboard format this data block represents. */
    VBOXCLIPBOARDFORMAT  uFormat;
    /** Pointer to actual data block. */
    void                *pvData;
    /** Size (in bytes) of actual data block. */
    uint32_t             cbData;
} SHAREDCLIPBOARDDATABLOCK, *PSHAREDCLIPBOARDDATABLOCK;

/**
 * Structure for keeping a Shared Clipboard data read request.
 */
typedef struct _SHAREDCLIPBOARDDATAREQ
{
    /** In which format the data needs to be sent. */
    VBOXCLIPBOARDFORMAT uFmt;
    /** Read flags; currently unused. */
    uint32_t            fFlags;
    /** Maximum data (in byte) can be sent. */
    uint32_t            cbSize;
} SHAREDCLIPBOARDDATAREQ, *PSHAREDCLIPBOARDDATAREQ;

/**
 * Structure for keeping Shared Clipboard formats specifications.
 */
typedef struct _SHAREDCLIPBOARDFORMATDATA
{
    /** Available format(s) as bit map. */
    VBOXCLIPBOARDFORMATS uFormats;
    /** Formats flags. Currently unused. */
    uint32_t             fFlags;
} SHAREDCLIPBOARDFORMATDATA, *PSHAREDCLIPBOARDFORMATDATA;

/**
 * Structure for an (optional) Shared Clipboard event payload.
 */
typedef struct _SHAREDCLIPBOARDEVENTPAYLOAD
{
    /** Payload ID; currently unused. */
    uint32_t uID;
    /** Pointer to actual payload data. */
    void    *pvData;
    /** Size (in bytes) of actual payload data. */
    uint32_t cbData;
} SHAREDCLIPBOARDEVENTPAYLOAD, *PSHAREDCLIPBOARDEVENTPAYLOAD;

/**
 * Structure for maintaining a Shared Clipboard event.
 */
typedef struct _SHAREDCLIPBOARDEVENT
{
    /** List node. */
    RTLISTNODE                   Node;
    /** The event's ID, for self-reference. */
    uint16_t                     uID;
    /** Event semaphore for signalling the event. */
    RTSEMEVENT                   hEventSem;
    /** Payload to this event. Optional and can be NULL. */
    PSHAREDCLIPBOARDEVENTPAYLOAD pPayload;
} SHAREDCLIPBOARDEVENT, *PSHAREDCLIPBOARDEVENT;

/**
 * Structure for maintaining a Shared Clipboard event source.
 *
 * Each event source maintains an own counter for events, so that
 * it can be used in different contexts.
 */
typedef struct _SHAREDCLIPBOARDEVENTSOURCE
{
    /** The event source' ID. */
    uint16_t                 uID;
    /** Next upcoming event ID.
     *  0 is reserved for invalid event IDs. */
    uint16_t                 uEventIDNext;
    /** List of events (PSHAREDCLIPBOARDEVENT). */
    RTLISTANCHOR             lstEvents;
} SHAREDCLIPBOARDEVENTSOURCE, *PSHAREDCLIPBOARDEVENTSOURCE;

int SharedClipboardPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData,
                                PSHAREDCLIPBOARDEVENTPAYLOAD *ppPayload);
void SharedClipboardPayloadFree(PSHAREDCLIPBOARDEVENTPAYLOAD pPayload);

int SharedClipboardEventSourceCreate(PSHAREDCLIPBOARDEVENTSOURCE pSource, uint16_t uID);
void SharedClipboardEventSourceDestroy(PSHAREDCLIPBOARDEVENTSOURCE pSource);

uint16_t SharedClipboardEventIDGenerate(PSHAREDCLIPBOARDEVENTSOURCE pSource);
int SharedClipboardEventRegister(PSHAREDCLIPBOARDEVENTSOURCE pSource, uint16_t uID);
int SharedClipboardEventUnregister(PSHAREDCLIPBOARDEVENTSOURCE pSource, uint16_t uID);
int SharedClipboardEventWait(PSHAREDCLIPBOARDEVENTSOURCE pSource, uint16_t uID, RTMSINTERVAL uTimeoutMs,
                             PSHAREDCLIPBOARDEVENTPAYLOAD* ppPayload);
int SharedClipboardEventSignal(PSHAREDCLIPBOARDEVENTSOURCE pSource, uint16_t uID, PSHAREDCLIPBOARDEVENTPAYLOAD pPayload);
void SharedClipboardEventPayloadDetach(PSHAREDCLIPBOARDEVENTSOURCE pSource, uint16_t uID);

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
