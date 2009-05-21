/* $Id$ */
/** @file
 * VirtualBox Support Library - Semaphores, ring-3 implementation.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP
#include <VBox/sup.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>

#include "SUPLibInternal.h"
#include "SUPDrvIOC.h"


/**
 * Worker that makes a SUP_IOCTL_SEM_OP request.
 *
 * @returns VBox status code.
 * @param   pSession            The session handle.
 * @param   uType               The semaphore type.
 * @param   hSem                The semaphore handle.
 * @param   uOp                 The operation.
 * @param   cMillies            The timeout if applicable, otherwise 0.
 */
DECLINLINE(int) supSemOp(PSUPDRVSESSION pSession, uint32_t uType, uintptr_t hSem, uint32_t uOp, uint32_t cMillies)
{
    SUPSEMOP Req;
    Req.Hdr.u32Cookie           = g_u32Cookie;
    Req.Hdr.u32SessionCookie    = g_u32SessionCookie;
    Req.Hdr.cbIn                = SUP_IOCTL_SEM_OP_SIZE_IN;
    Req.Hdr.cbOut               = SUP_IOCTL_SEM_OP_SIZE_OUT;
    Req.Hdr.fFlags              = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc                  = VERR_INTERNAL_ERROR;
    Req.u.In.uType              = uType;
    Req.u.In.hSem               = (uint32_t)hSem;
    AssertReturn(Req.u.In.hSem == hSem, VERR_INVALID_HANDLE);
    Req.u.In.uOp                = uOp;
    Req.u.In.cMillies           = 0;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_SEM_OP, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;

    return rc;
}


SUPDECL(int) SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent)
{
    AssertPtrReturn(phEvent, VERR_INVALID_POINTER);

    SUPSEMCREATE Req;
    Req.Hdr.u32Cookie           = g_u32Cookie;
    Req.Hdr.u32SessionCookie    = g_u32SessionCookie;
    Req.Hdr.cbIn                = SUP_IOCTL_SEM_CREATE_SIZE_IN;
    Req.Hdr.cbOut               = SUP_IOCTL_SEM_CREATE_SIZE_OUT;
    Req.Hdr.fFlags              = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc                  = VERR_INTERNAL_ERROR;
    Req.u.In.uType              = SUP_SEM_TYPE_EVENT;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_SEM_CREATE, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
    {
        rc = Req.Hdr.rc;
        if (RT_SUCCESS(rc))
            *phEvent = (SUPSEMEVENT)(uintptr_t)Req.u.Out.hSem;
    }

    return rc;
}


SUPDECL(int) SUPSemEventClose(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
    if (hEvent == NIL_SUPSEMEVENT)
        return VINF_SUCCESS;
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT, (uintptr_t)hEvent, SUPSEMOP_CLOSE, 0);
}


SUPDECL(int) SUPSemEventSignal(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT, (uintptr_t)hEvent, SUPSEMOP_SIGNAL, 0);
}


SUPDECL(int) SUPSemEventWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint32_t cMillies)
{
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT, (uintptr_t)hEvent, SUPSEMOP_WAIT, cMillies);
}

SUPDECL(int) SUPSemEventMultiCreate(PSUPDRVSESSION pSession, PSUPSEMEVENTMULTI phEventMulti)
{
    AssertPtrReturn(phEventMulti, VERR_INVALID_POINTER);

    SUPSEMCREATE Req;
    Req.Hdr.u32Cookie           = g_u32Cookie;
    Req.Hdr.u32SessionCookie    = g_u32SessionCookie;
    Req.Hdr.cbIn                = SUP_IOCTL_SEM_CREATE_SIZE_IN;
    Req.Hdr.cbOut               = SUP_IOCTL_SEM_CREATE_SIZE_OUT;
    Req.Hdr.fFlags              = SUPREQHDR_FLAGS_DEFAULT;
    Req.Hdr.rc                  = VERR_INTERNAL_ERROR;
    Req.u.In.uType              = SUP_SEM_TYPE_EVENT_MULTI;
    int rc = suplibOsIOCtl(&g_supLibData, SUP_IOCTL_SEM_CREATE, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
    {
        rc = Req.Hdr.rc;
        if (RT_SUCCESS(rc))
            *phEventMulti = (SUPSEMEVENTMULTI)(uintptr_t)Req.u.Out.hSem;
    }

    return rc;
}


SUPDECL(int) SUPSemEventMultiClose(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti)
{
    if (hEventMulti == NIL_SUPSEMEVENTMULTI)
        return VINF_SUCCESS;
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT_MULTI, (uintptr_t)hEventMulti, SUPSEMOP_CLOSE, 0);
}


SUPDECL(int) SUPSemEventMultiSignal(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti)
{
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT_MULTI, (uintptr_t)hEventMulti, SUPSEMOP_SIGNAL, 0);
}


SUPDECL(int) SUPSemEventMultiReset(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti)
{
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT_MULTI, (uintptr_t)hEventMulti, SUPSEMOP_RESET, 0);
}


SUPDECL(int) SUPSemEventMultiWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint32_t cMillies)
{
    return supSemOp(pSession, SUP_SEM_TYPE_EVENT_MULTI, (uintptr_t)hEventMulti, SUPSEMOP_WAIT, cMillies);
}

