/* $Id$ */
/** @file
 * IPRT - Internal RTReq header.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

#ifndef ___internal_req_h
#define ___internal_req_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/**
 * Internal queue instance.
 */
typedef struct RTREQQUEUEINT
{
    /** Magic value (RTREQQUEUE_MAGIC). */
    uint32_t                u32Magic;
    /** Set if busy (pending or processing requests). */
    bool volatile           fBusy;
    /** Head of the request queue. Atomic. */
    volatile PRTREQ         pReqs;
    /** The last index used during alloc/free. */
    volatile uint32_t       iReqFree;
    /** Number of free request packets. */
    volatile uint32_t       cReqFree;
    /** Array of pointers to lists of free request packets. Atomic. */
    volatile PRTREQ         apReqFree[9];
    /** Requester event sem.
     * The request can use this event semaphore to wait/poll for new requests.
     */
    RTSEMEVENT              EventSem;
} RTREQQUEUEINT;

/** Pointer to an internal queue instance. */
typedef RTREQQUEUEINT *PRTREQQUEUEINT;



DECLHIDDEN(int) rtReqProcessOne(PRTREQ pReq);

RT_C_DECLS_END

#endif

