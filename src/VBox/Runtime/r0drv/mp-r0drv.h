/* $Id$ */
/** @file
 * innotek Portable Runtime - Multiprocessor, Ring-0 Driver, Internal Header.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___r0drv_mp_r0drv_h
#define ___r0drv_mp_r0drv_h

#include <iprt/mp.h>

/**
 * MP callback
 *
 * @param   idCpu       CPU id
 * @param   pvUser1     The first user argument.
 * @param   pvUser2     The second user argument.
 */
typedef DECLCALLBACK(void) FNMPWORKER(RTCPUID idCpu, void *pvUser1, void *pvUser2);
/** Pointer to a FNMPWORKER(). */
typedef FNMPWORKER *PFNMPWORKER;

/**
 * RTMpOn* argument packet used by the host specific callback 
 * wrapper functions.
 */
typedef struct RTMPARGS
{
    PFNMPWORKER pfnWorker;
    void       *pvUser1;
    void       *pvUser2;
    RTCPUID     idCpu;
    uint32_t volatile cHits;
} RTMPARGS;
/** Pointer to a RTMpOn* argument packet. */
typedef RTMPARGS *PRTMPARGS;

#endif
