/* $Revision$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common OS agnostic.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_SUP_DRV
#define SUPDRV_AGNOSTIC
#include "SUPDrvInternal.h"

/** @todo trim this down. */
#include <iprt/param.h>
#include <iprt/alloc.h>
#include <iprt/cpuset.h>
#include <iprt/handletable.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/err.h>


/**
 * Destructor for objects created by SUPSemEventCreate.
 *
 * @param   pvObj               The object handle.
 * @param   pvUser1             The IPRT event handle.
 * @param   pvUser2             NULL.
 */
static DECLCALLBACK(void) supR0SemEventDestructor(void *pvObj, void *pvUser1, void *pvUser2)
{
    Assert(pvUser2 == NULL);
    NOREF(pvObj);
    RTSemEventDestroy((RTSEMEVENT)pvUser1);
}


SUPDECL(int) SUPSemEventCreate(PSUPDRVSESSION pSession, PSUPSEMEVENT phEvent)
{
    int         rc;
    RTSEMEVENT  hEventReal;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phEvent, VERR_INVALID_POINTER);

    /*
     * Create the event semaphore object.
     */
    rc = RTSemEventCreate(&hEventReal);
    if (RT_SUCCESS(rc))
    {
        void *pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_SEM_EVENT, supR0SemEventDestructor, hEventReal, NULL);
        if (pvObj)
        {
            uint32_t h32;
            rc = RTHandleTableAllocWithCtx(pSession->hHandleTable, pvObj, SUPDRV_HANDLE_CTX_EVENT, &h32);
            if (RT_SUCCESS(rc))
            {
                *phEvent = (SUPSEMEVENT)(uintptr_t)h32;
                return VINF_SUCCESS;
            }
            SUPR0ObjRelease(pvObj, pSession);
        }
        else
            RTSemEventDestroy(hEventReal);
    }
    return rc;
}


SUPDECL(int) SUPSemEventClose(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    if (hEvent == NIL_SUPSEMEVENT)
        return VINF_SUCCESS;
    h32 = (uint32_t)(uintptr_t)hEvent;
    if (h32 != (uintptr_t)hEvent)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    pObj = (PSUPDRVOBJ)RTHandleTableFreeWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    Assert(pObj->cUsage >= 2);
    SUPR0ObjRelease(pObj, pSession);        /* The free call above. */
    return SUPR0ObjRelease(pObj, pSession); /* The handle table reference. */
}


SUPDECL(int) SUPSemEventSignal(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEvent;
    if (h32 != (uintptr_t)hEvent)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventSignal((RTSEMEVENT)pObj->pvUser1);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}


SUPDECL(int) SUPSemEventWait(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint32_t cMillies)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEvent;
    if (h32 != (uintptr_t)hEvent)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventWait((RTSEMEVENT)pObj->pvUser1, cMillies);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}


SUPDECL(int) SUPSemEventWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENT hEvent, uint32_t cMillies)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEvent;
    if (h32 != (uintptr_t)hEvent)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventWaitNoResume((RTSEMEVENT)pObj->pvUser1, cMillies);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}


/**
 * Destructor for objects created by SUPSemEventMultiCreate.
 *
 * @param   pvObj               The object handle.
 * @param   pvUser1             The IPRT event handle.
 * @param   pvUser2             NULL.
 */
static DECLCALLBACK(void) supR0SemEventMultiDestructor(void *pvObj, void *pvUser1, void *pvUser2)
{
    Assert(pvUser2 == NULL);
    NOREF(pvObj);
    RTSemEventMultiDestroy((RTSEMEVENTMULTI)pvUser1);
}


SUPDECL(int) SUPSemEventMultiCreate(PSUPDRVSESSION pSession, PSUPSEMEVENTMULTI phEventMulti)
{
    int             rc;
    RTSEMEVENTMULTI hEventMultReal;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phEventMulti, VERR_INVALID_POINTER);

    /*
     * Create the event semaphore object.
     */
    rc = RTSemEventMultiCreate(&hEventMultReal);
    if (RT_SUCCESS(rc))
    {
        void *pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_SEM_EVENT_MULTI, supR0SemEventMultiDestructor, hEventMultReal, NULL);
        if (pvObj)
        {
            uint32_t h32;
            rc = RTHandleTableAllocWithCtx(pSession->hHandleTable, pvObj, SUPDRV_HANDLE_CTX_EVENT_MULTI, &h32);
            if (RT_SUCCESS(rc))
            {
                *phEventMulti = (SUPSEMEVENTMULTI)(uintptr_t)h32;
                return VINF_SUCCESS;
            }
            SUPR0ObjRelease(pvObj, pSession);
        }
        else
            RTSemEventMultiDestroy(hEventMultReal);
    }
    return rc;
}


SUPDECL(int) SUPSemEventMultiClose(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti)
{
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    if (hEventMulti == NIL_SUPSEMEVENTMULTI)
        return VINF_SUCCESS;
    h32 = (uint32_t)(uintptr_t)hEventMulti;
    if (h32 != (uintptr_t)hEventMulti)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    pObj = (PSUPDRVOBJ)RTHandleTableFreeWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT_MULTI);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    Assert(pObj->cUsage >= 2);
    SUPR0ObjRelease(pObj, pSession);        /* The free call above. */
    return SUPR0ObjRelease(pObj, pSession); /* The handle table reference. */
}


SUPDECL(int) SUPSemEventMultiSignal(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEventMulti;
    if (h32 != (uintptr_t)hEventMulti)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT_MULTI);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventMultiSignal((RTSEMEVENTMULTI)pObj->pvUser1);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}


SUPDECL(int) SUPSemEventMultiReset(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEventMulti;
    if (h32 != (uintptr_t)hEventMulti)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT_MULTI);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventMultiReset((RTSEMEVENTMULTI)pObj->pvUser1);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}


SUPDECL(int) SUPSemEventMultiWait(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint32_t cMillies)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEventMulti;
    if (h32 != (uintptr_t)hEventMulti)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT_MULTI);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventMultiWait((RTSEMEVENTMULTI)pObj->pvUser1, cMillies);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}


SUPDECL(int) SUPSemEventMultiWaitNoResume(PSUPDRVSESSION pSession, SUPSEMEVENTMULTI hEventMulti, uint32_t cMillies)
{
    int         rc;
    uint32_t    h32;
    PSUPDRVOBJ  pObj;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    h32 = (uint32_t)(uintptr_t)hEventMulti;
    if (h32 != (uintptr_t)hEventMulti)
        return VERR_INVALID_HANDLE;
    pObj = (PSUPDRVOBJ)RTHandleTableLookupWithCtx(pSession->hHandleTable, h32, SUPDRV_HANDLE_CTX_EVENT_MULTI);
    if (!pObj)
        return VERR_INVALID_HANDLE;

    /*
     * Do the job.
     */
    rc = RTSemEventMultiWaitNoResume((RTSEMEVENTMULTI)pObj->pvUser1, cMillies);

    SUPR0ObjRelease(pObj, pSession);
    return rc;
}

