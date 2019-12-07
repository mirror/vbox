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

#include <iprt/types.h>
#include <iprt/list.h>

/**
 * Shared Clipboard transfer direction.
 */
typedef enum SHCLTRANSFERDIR
{
    /** Unknown transfer directory. */
    SHCLTRANSFERDIR_UNKNOWN = 0,
    /** Read transfer (from source). */
    SHCLTRANSFERDIR_FROM_REMOTE,
    /** Write transfer (to target). */
    SHCLTRANSFERDIR_TO_REMOTE,
    /** The usual 32-bit hack. */
    SHCLTRANSFERDIR_32BIT_HACK = 0x7fffffff
} SHCLTRANSFERDIR;
/** Pointer to a shared clipboard transfer direction. */
typedef SHCLTRANSFERDIR *PSHCLTRANSFERDIR;


/** A single Shared Clipboard format. */
typedef uint32_t SHCLFORMAT;
/** Pointer to a single Shared Clipboard format. */
typedef SHCLFORMAT *PSHCLFORMAT;

/** Bit map of Shared Clipboard formats. */
typedef uint32_t SHCLFORMATS;
/** Pointer to a bit map of Shared Clipboard formats. */
typedef SHCLFORMATS *PSHCLFORMATS;

/** @name VBOX_SHCL_FMT_XXX - Data formats (flags) for Shared Clipboard.
 * @todo r=bird: Wrong header, belongs in the host service!
 * @{
 */
/** No format set. */
#define VBOX_SHCL_FMT_NONE          0
/** Shared Clipboard format is an Unicode text. */
#define VBOX_SHCL_FMT_UNICODETEXT   RT_BIT(0)
/** Shared Clipboard format is bitmap (BMP / DIB). */
#define VBOX_SHCL_FMT_BITMAP        RT_BIT(1)
/** Shared Clipboard format is HTML. */
#define VBOX_SHCL_FMT_HTML          RT_BIT(2)
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/** Shared Clipboard format is a transfer list. */
# define VBOX_SHCL_FMT_URI_LIST     RT_BIT(3)
#endif
/** @}  */

/**
 * Generic Shared Clipboard data block.
 */
typedef struct SHCLDATABLOCK
{
    /** Clipboard format this data block represents. */
    SHCLFORMAT  uFormat;
    /** Size (in bytes) of actual data block. */
    uint32_t    cbData;
    /** Pointer to actual data block. */
    void       *pvData;
} SHCLDATABLOCK;
/** Pointer to a generic shared clipboard data block. */
typedef SHCLDATABLOCK *PSHCLDATABLOCK;

/**
 * Shared Clipboard data read request.
 */
typedef struct SHCLDATAREQ
{
    /** In which format the data needs to be sent. */
    SHCLFORMAT uFmt;
    /** Read flags; currently unused. */
    uint32_t   fFlags;
    /** Maximum data (in byte) can be sent. */
    uint32_t   cbSize;
} SHCLDATAREQ;
/** Pointer to a shared clipboard data request. */
typedef SHCLDATAREQ *PSHCLDATAREQ;

/**
 * Shared Clipboard formats specification.
 */
typedef struct SHCLFORMATDATA
{
    /** Available format(s) as bit map. */
    SHCLFORMATS Formats;
    /** Formats flags. Currently unused. */
    uint32_t    fFlags;
} SHCLFORMATDATA;
/** Pointer to a shared clipboard formats specification. */
typedef SHCLFORMATDATA *PSHCLFORMATDATA;

/**
 * Shared Clipboard event payload (optional).
 */
typedef struct SHCLEVENTPAYLOAD
{
    /** Payload ID; currently unused. */
    uint32_t uID;
    /** Size (in bytes) of actual payload data. */
    uint32_t cbData;
    /** Pointer to actual payload data. */
    void    *pvData;
} SHCLEVENTPAYLOAD;
/** Pointer to a shared clipboard event payload. */
typedef SHCLEVENTPAYLOAD *PSHCLEVENTPAYLOAD;

/** A shared clipboard event source ID. */
typedef uint16_t SHCLEVENTSOURCEID;
/** Pointer to a shared clipboard event source ID. */
typedef SHCLEVENTSOURCEID *PSHCLEVENTSOURCEID;

/** A shared clipboard session ID. */
typedef uint16_t        SHCLSESSIONID;
/** Pointer to a shared clipboard session ID. */
typedef SHCLSESSIONID  *PSHCLSESSIONID;
/** NIL shared clipboard session ID. */
#define NIL_SHCLSESSIONID                        UINT16_MAX

/** A shared clipboard transfer ID. */
typedef uint16_t        SHCLTRANSFERID;
/** Pointer to a shared clipboard transfer ID. */
typedef SHCLTRANSFERID *PSHCLTRANSFERID;
/** NIL shared clipboardtransfer ID. */
#define NIL_SHCLTRANSFERID                       UINT16_MAX

/** A shared clipboard event ID. */
typedef uint32_t        SHCLEVENTID;
/** Pointer to a shared clipboard event source ID. */
typedef SHCLEVENTID    *PSHCLEVENTID;
/** NIL shared clipboard event ID. */
#define NIL_SHCLEVENTID                          UINT32_MAX

/** Maximum number of concurrent Shared Clipboard client sessions a VM can have. */
#define VBOX_SHCL_MAX_SESSIONS                   (UINT16_MAX - 1)
/** Maximum number of concurrent Shared Clipboard transfers a single client can have. */
#define VBOX_SHCL_MAX_TRANSFERS                  (UINT16_MAX - 1)
/** Maximum number of events a single Shared Clipboard transfer can have. */
#define VBOX_SHCL_MAX_EVENTS                     (UINT32_MAX - 1)

/**
 * Creates a context ID out of a client ID, a transfer ID and an event ID (count).
 */
#define VBOX_SHCL_CONTEXTID_MAKE(a_idSession, a_idTransfer, a_idEvent) \
    (  ((uint64_t)((a_idSession)  & 0xffff) << 48) \
     | ((uint64_t)((a_idTransfer) & 0xffff) << 32) \
     | ((uint32_t) (a_idEvent)) \
    )
/** Creates a context ID out of a session ID. */
#define VBOX_SHCL_CONTEXTID_MAKE_SESSION(a_idSession)    VBOX_SHCL_CONTEXTID_MAKE(a_idSession, 0, 0)
/** Gets the session ID out of a context ID. */
#define VBOX_SHCL_CONTEXTID_GET_SESSION(a_idContext)     ( (uint16_t)(((a_idContext) >> 48) & UINT16_MAX) )
/** Gets the transfer ID out of a context ID. */
#define VBOX_SHCL_CONTEXTID_GET_TRANSFER(a_idContext)    ( (uint16_t)(((a_idContext) >> 32) & UINT16_MAX) )
/** Gets the transfer event out of a context ID. */
#define VBOX_SHCL_CONTEXTID_GET_EVENT(a_idContext)       ( (uint32_t)( (a_idContext)        & UINT32_MAX) )

/**
 * Shared Clipboard event.
 */
typedef struct SHCLEVENT
{
    /** List node. */
    RTLISTNODE          Node;
    /** The event's ID, for self-reference. */
    SHCLEVENTID         uID;
    /** Event semaphore for signalling the event. */
    RTSEMEVENT          hEventSem;
    /** Payload to this event, optional (NULL). */
    PSHCLEVENTPAYLOAD   pPayload;
} SHCLEVENT;
/** Pointer to a shared clipboard event. */
typedef SHCLEVENT *PSHCLEVENT;

/**
 * Shared Clipboard event source.
 *
 * Each event source maintains an own counter for events, so that it can be used
 * in different contexts.
 */
typedef struct SHCLEVENTSOURCE
{
    /** The event source ID. */
    SHCLEVENTSOURCEID uID;
    /** Next upcoming event ID. */
    SHCLEVENTID       uEventIDNext;
    /** List of events (PSHCLEVENT). */
    RTLISTANCHOR      lstEvents;
} SHCLEVENTSOURCE;
/** Pointer to a shared clipboard event source. */
typedef SHCLEVENTSOURCE *PSHCLEVENTSOURCE;

/** @name Shared Clipboard data payload functions.
 *  @{
 */
int ShClPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData, PSHCLEVENTPAYLOAD *ppPayload);
void ShClPayloadFree(PSHCLEVENTPAYLOAD pPayload);
/** @} */

/** @name Shared Clipboard event source functions.
 *  @{
 */
int ShClEventSourceCreate(PSHCLEVENTSOURCE pSource, SHCLEVENTSOURCEID idEvtSrc);
void ShClEventSourceDestroy(PSHCLEVENTSOURCE pSource);
void ShClEventSourceReset(PSHCLEVENTSOURCE pSource);
/** @} */

/** @name Shared Clipboard event functions.
 *  @{
 */
SHCLEVENTID ShClEventIDGenerate(PSHCLEVENTSOURCE pSource);
SHCLEVENTID ShClEventGetLast(PSHCLEVENTSOURCE pSource);
int ShClEventRegister(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);
int ShClEventUnregister(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);
int ShClEventWait(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent, RTMSINTERVAL uTimeoutMs, PSHCLEVENTPAYLOAD *ppPayload);
int ShClEventSignal(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent, PSHCLEVENTPAYLOAD pPayload);
void ShClEventPayloadDetach(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);
/** @} */

/**
 * Shared Clipboard transfer source type.
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
    SHCLSOURCE_32BIT_HACK = 0x7fffffff
} SHCLSOURCE;

/** Opaque data structure for the X11/VBox frontend/glue code.
 * @{ */
struct SHCLCONTEXT;
typedef struct SHCLCONTEXT SHCLCONTEXT;
/** @} */
/** Pointer to opaque data structure the X11/VBox frontend/glue code. */
typedef SHCLCONTEXT *PSHCLCONTEXT;

/** Opaque request structure for X11 clipboard data.
 * @{ */
struct CLIPREADCBREQ;
typedef struct CLIPREADCBREQ CLIPREADCBREQ;
/** @} */

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_h */

