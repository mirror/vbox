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


/** A single Shared Clipboard format (VBOX_SHCL_FMT_XXX). */
typedef uint32_t SHCLFORMAT;
/** Pointer to a single Shared Clipboard format (VBOX_SHCL_FMT_XXX). */
typedef SHCLFORMAT *PSHCLFORMAT;

/** Bit map (flags) of Shared Clipboard formats (VBOX_SHCL_FMT_XXX). */
typedef uint32_t SHCLFORMATS;
/** Pointer to a bit map of Shared Clipboard formats (VBOX_SHCL_FMT_XXX). */
typedef SHCLFORMATS *PSHCLFORMATS;


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
 * @todo r=bird: Pointless as we don't have any fFlags defined, so, unless
 *       someone can give me a plausible scenario where we will need flags here,
 *       this structure will be eliminated.
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

/**
 * Shared Clipboard event.
 */
typedef struct SHCLEVENT
{
    /** List node. */
    RTLISTNODE          Node;
    /** The event's ID, for self-reference. */
    SHCLEVENTID         idEvent;
    /** Event semaphore for signalling the event. */
    RTSEMEVENTMULTI     hEvtMulSem;
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
    SHCLEVENTID       idNextEvent;
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
SHCLEVENTID ShClEventIdGenerateAndRegister(PSHCLEVENTSOURCE pSource);
SHCLEVENTID ShClEventIDGenerate(PSHCLEVENTSOURCE pSource);
SHCLEVENTID ShClEventGetLast(PSHCLEVENTSOURCE pSource);
/*int ShClEventRegister(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);*/
int ShClEventUnregister(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);
int ShClEventWait(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent, RTMSINTERVAL uTimeoutMs, PSHCLEVENTPAYLOAD *ppPayload);
int ShClEventSignal(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent, PSHCLEVENTPAYLOAD pPayload);
void ShClEventPayloadDetach(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);
/** @} */

/**
 * Shared Clipboard transfer source type.
 * @note Part of saved state!
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

