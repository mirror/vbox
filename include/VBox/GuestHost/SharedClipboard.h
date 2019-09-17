/** @file
 * Shared Clipboard - Common guest and host Code.
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
typedef uint32_t SHCLFORMAT;
/** Pointer to a single Shared Clipboard format. */
typedef SHCLFORMAT *PSHCLFORMAT;

/** Bit map of Shared Clipboard formats. */
typedef uint32_t SHCLFORMATS;
/** Pointer to a bit map of Shared Clipboard formats. */
typedef SHCLFORMATS *PSHCLFORMATS;

/**
 * Supported data formats for Shared Clipboard. Bit mask.
 */
/** No format set. */
#define VBOX_SHARED_CLIPBOARD_FMT_NONE          0
/** Shared Clipboard format is an Unicode text. */
#define VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT   RT_BIT(0)
/** Shared Clipboard format is bitmap (BMP / DIB). */
#define VBOX_SHARED_CLIPBOARD_FMT_BITMAP        RT_BIT(1)
/** Shared Clipboard format is HTML. */
#define VBOX_SHARED_CLIPBOARD_FMT_HTML          RT_BIT(2)
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/** Shared Clipboard format is an URI list. */
#define VBOX_SHARED_CLIPBOARD_FMT_URI_LIST      RT_BIT(3)
#endif

/**
 * Structure for keeping a generic Shared Clipboard data block.
 */
typedef struct _SHCLDATABLOCK
{
    /** Clipboard format this data block represents. */
    SHCLFORMAT  uFormat;
    /** Pointer to actual data block. */
    void       *pvData;
    /** Size (in bytes) of actual data block. */
    uint32_t    cbData;
} SHCLDATABLOCK, *PSHCLDATABLOCK;

/**
 * Structure for keeping a Shared Clipboard data read request.
 */
typedef struct _SHCLDATAREQ
{
    /** In which format the data needs to be sent. */
    SHCLFORMAT uFmt;
    /** Read flags; currently unused. */
    uint32_t   fFlags;
    /** Maximum data (in byte) can be sent. */
    uint32_t   cbSize;
} SHCLDATAREQ, *PSHCLDATAREQ;

/**
 * Structure for keeping Shared Clipboard formats specifications.
 */
typedef struct _SHCLFORMATDATA
{
    /** Available format(s) as bit map. */
    SHCLFORMATS uFormats;
    /** Formats flags. Currently unused. */
    uint32_t    fFlags;
} SHCLFORMATDATA, *PSHCLFORMATDATA;

/**
 * Structure for an (optional) Shared Clipboard event payload.
 */
typedef struct _SHCLEVENTPAYLOAD
{
    /** Payload ID; currently unused. */
    uint32_t uID;
    /** Pointer to actual payload data. */
    void    *pvData;
    /** Size (in bytes) of actual payload data. */
    uint32_t cbData;
} SHCLEVENTPAYLOAD, *PSHCLEVENTPAYLOAD;

/** Defines an event source ID. */
typedef uint16_t SHCLEVENTSOURCEID;
/** Defines a pointer to a event source ID. */
typedef SHCLEVENTSOURCEID *PSHCLEVENTSOURCEID;

/** Defines an event ID. */
typedef uint16_t SHCLEVENTID;
/** Defines a pointer to a event source ID. */
typedef SHCLEVENTID *PSHCLEVENTID;

/** Maximum number of concurrent Shared Clipboard client sessions a VM can have. */
#define VBOX_SHARED_CLIPBOARD_MAX_SESSIONS                   32
/** Maximum number of concurrent Shared Clipboard transfers a single
 *  client can have. */
#define VBOX_SHARED_CLIPBOARD_MAX_TRANSFERS                  _2K
/** Maximum number of events a single Shared Clipboard transfer can have. */
#define VBOX_SHARED_CLIPBOARD_MAX_EVENTS                     _64K

/**
 * Creates a context ID out of a client ID, a transfer ID and a count.
 */
#define VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(uSessionID, uTransferID, uEventID) \
    (  (uint32_t)((uSessionID)  &   0x1f) << 27 \
     | (uint32_t)((uTransferID) &  0x7ff) << 16 \
     | (uint32_t)((uEventID)    & 0xffff)       \
    )
/** Creates a context ID out of a session ID. */
#define VBOX__SHARED_CLIPBOARD_CONTEXTID_MAKE_SESSION(uSessionID) \
    ((uint32_t)((uSessionID) & 0x1f) << 27)
/** Gets the session ID out of a context ID. */
#define VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_SESSION(uContextID) \
    (((uContextID) >> 27) & 0x1f)
/** Gets the transfer ID out of a context ID. */
#define VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(uContextID) \
    (((uContextID) >> 16) & 0x7ff)
/** Gets the transfer event out of a context ID. */
#define VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uContextID) \
    ((uContextID) & 0xffff)

/**
 * Structure for maintaining a Shared Clipboard event.
 */
typedef struct _SHCLEVENT
{
    /** List node. */
    RTLISTNODE        Node;
    /** The event's ID, for self-reference. */
    SHCLEVENTID       uID;
    /** Event semaphore for signalling the event. */
    RTSEMEVENT        hEventSem;
    /** Payload to this event. Optional and can be NULL. */
    PSHCLEVENTPAYLOAD pPayload;
} SHCLEVENT, *PSHCLEVENT;

/**
 * Structure for maintaining a Shared Clipboard event source.
 *
 * Each event source maintains an own counter for events, so that
 * it can be used in different contexts.
 */
typedef struct _SHCLEVENTSOURCE
{
    /** The event source' ID. */
    SHCLEVENTSOURCEID uID;
    /** Next upcoming event ID. */
    SHCLEVENTID       uEventIDNext;
    /** List of events (PSHCLEVENT). */
    RTLISTANCHOR      lstEvents;
} SHCLEVENTSOURCE, *PSHCLEVENTSOURCE;

int SharedClipboardPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData,
                                PSHCLEVENTPAYLOAD *ppPayload);
void SharedClipboardPayloadFree(PSHCLEVENTPAYLOAD pPayload);

int SharedClipboardEventSourceCreate(PSHCLEVENTSOURCE pSource, SHCLEVENTSOURCEID uID);
void SharedClipboardEventSourceDestroy(PSHCLEVENTSOURCE pSource);

SHCLEVENTID SharedClipboardEventIDGenerate(PSHCLEVENTSOURCE pSource);
SHCLEVENTID SharedClipboardEventGetLast(PSHCLEVENTSOURCE pSource);
int SharedClipboardEventRegister(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID);
int SharedClipboardEventUnregister(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID);
int SharedClipboardEventWait(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID, RTMSINTERVAL uTimeoutMs,
                             PSHCLEVENTPAYLOAD* ppPayload);
int SharedClipboardEventSignal(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID, PSHCLEVENTPAYLOAD pPayload);
void SharedClipboardEventPayloadDetach(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID);

/**
 * Enumeration to specify the Shared Clipboard URI source type.
 */
typedef enum SHCLSOURCE
{
    /** Invalid source type. */
    SHCLSOURCE_INVALID = 0,
    /** Source is local. */
    SHCLSOURCE_LOCAL,
    /** Source is remote. */
    SHCLSOURCE_REMOTE,
    /** The usual 32-bit hack. */
    SHCLSOURCE_32Bit_Hack = 0x7fffffff
} SHCLSOURCE;

/** Opaque data structure for the X11/VBox frontend/glue code. */
struct _SHCLCONTEXT;
typedef struct _SHCLCONTEXT SHCLCONTEXT;
typedef struct _SHCLCONTEXT *PSHCLCONTEXT;

/** Opaque data structure for the X11/VBox backend code. */
struct _CLIPBACKEND;
typedef struct _CLIPBACKEND CLIPBACKEND;

/** Opaque request structure for X11 clipboard data.
 * @todo All use of single and double underscore prefixes is banned! */
struct _CLIPREADCBREQ;
typedef struct _CLIPREADCBREQ CLIPREADCBREQ;

/* APIs exported by the X11 backend */
extern CLIPBACKEND *ClipConstructX11(SHCLCONTEXT *pFrontend, bool fHeadless);
extern void ClipDestructX11(CLIPBACKEND *pBackend);
extern int ClipStartX11(CLIPBACKEND *pBackend, bool grab);
extern int ClipStopX11(CLIPBACKEND *pBackend);
extern int ClipAnnounceFormatToX11(CLIPBACKEND *pBackend, SHCLFORMATS vboxFormats);
extern int ClipRequestDataFromX11(CLIPBACKEND *pBackend, SHCLFORMATS vboxFormat, CLIPREADCBREQ *pReq);

/* APIs exported by the X11/VBox frontend */
extern int ClipRequestDataForX11(SHCLCONTEXT *pCtx, uint32_t u32Format, void **ppv, uint32_t *pcb);
extern void ClipReportX11Formats(SHCLCONTEXT *pCtx, uint32_t u32Formats);
extern void ClipRequestFromX11CompleteCallback(SHCLCONTEXT *pCtx, int rc, CLIPREADCBREQ *pReq, void *pv, uint32_t cb);
#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_h */

