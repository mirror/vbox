/* $Id$ */
/** @file
 * IPRT - Debug Module Interpreter.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/strcache.h>
#include <iprt/string.h>
#include "internal/dbgmod.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Debug info interpreter registration record. */
typedef struct RTDBGMODREGDBG
{
    /** Pointer to the next record. */
    struct RTDBGMODREGDBG  *pNext;
    /** Pointer to the virtual function table for the interpreter.  */
    PCRTDBGMODVTDBG         pVt;
    /** Usage counter.  */
    uint32_t volatile       cUsers;
} RTDBGMODREGDBG;
typedef RTDBGMODREGDBG *PRTDBGMODREGDBG;

/** Image interpreter registration record. */
typedef struct RTDBGMODREGIMG
{
    /** Pointer to the next record. */
    struct RTDBGMODREGIMG  *pNext;
    /** Pointer to the virtual function table for the interpreter.  */
    PCRTDBGMODVTIMG         pVt;
    /** Usage counter.  */
    uint32_t volatile       cUsers;
} RTDBGMODREGIMG;
typedef RTDBGMODREGIMG *PRTDBGMODREGIMG;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a debug module handle and returns rc if not valid. */
#define RTDBGMOD_VALID_RETURN_RC(pDbgMod, rc) \
    do { \
        AssertPtrReturn((pDbgMod), (rc)); \
        AssertReturn((pDbgMod)->u32Magic == RTDBGMOD_MAGIC, (rc)); \
        AssertReturn((pDbgMod)->cRefs > 0, (rc)); \
    } while (0)

/** Locks the debug module. */
#define RTDBGMOD_LOCK(pDbgMod) \
    do { \
        int rcLock = RTCritSectEnter(&(pDbgMod)->CritSect); \
        AssertRC(rcLock); \
    } while (0)

/** Unlocks the debug module. */
#define RTDBGMOD_UNLOCK(pDbgMod) \
    do { \
        int rcLock = RTCritSectLeave(&(pDbgMod)->CritSect); \
        AssertRC(rcLock); \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Init once object for lazy registration of the built-in image and debug
 * info interpreters. */
static RTONCE           g_rtDbgModOnce = RTONCE_INITIALIZER;
/** Read/Write semaphore protecting the list of registered interpreters.  */
static RTSEMRW          g_hDbgModRWSem = NIL_RTSEMRW;
/** List of registered image interpreters.  */
static PRTDBGMODREGIMG  g_pImgHead;
/** List of registered debug infor interpreters.  */
static PRTDBGMODREGDBG  g_pDbgHead;
/** String cache for the debug info interpreters.
 * RTSTRCACHE is thread safe. */
DECLHIDDEN(RTSTRCACHE)  g_hDbgModStrCache = NIL_RTSTRCACHE;



/**
 * Cleanup debug info interpreter globals.
 *
 * @param   enmReason           The cause of the termination.
 * @param   iStatus             The meaning of this depends on enmReason.
 * @param   pvUser              User argument, unused.
 */
static DECLCALLBACK(void) rtDbgModTermCallback(RTTERMREASON enmReason, int32_t iStatus, void *pvUser)
{
    NOREF(iStatus); NOREF(pvUser);
    if (enmReason == RTTERMREASON_UNLOAD)
    {
        RTSemRWDestroy(g_hDbgModRWSem);
        g_hDbgModRWSem = NIL_RTSEMRW;

        RTStrCacheDestroy(g_hDbgModStrCache);
        g_hDbgModStrCache = NIL_RTSTRCACHE;

        PRTDBGMODREGDBG pDbg = g_pDbgHead;
        g_pDbgHead = NULL;
        while (pDbg)
        {
            PRTDBGMODREGDBG pNext = pDbg->pNext;
            AssertMsg(pDbg->cUsers == 0, ("%#x %s\n", pDbg->cUsers, pDbg->pVt->pszName));
            RTMemFree(pDbg);
            pDbg = pNext;
        }

        PRTDBGMODREGIMG pImg = g_pImgHead;
        g_pImgHead = NULL;
        while (pImg)
        {
            PRTDBGMODREGIMG pNext = pImg->pNext;
            AssertMsg(pImg->cUsers == 0, ("%#x %s\n", pImg->cUsers, pImg->pVt->pszName));
            RTMemFree(pImg);
            pImg = pNext;
        }
    }
}


/**
 * Internal worker for register a debug interpreter.
 *
 * Called while owning the write lock or when locking isn't required.
 *
 * @returns IPRT status code.
 * @retval  VERR_NO_MEMORY
 * @retval  VERR_ALREADY_EXISTS
 *
 * @param   pVt                 The virtual function table of the debug
 *                              module interpreter.
 */
static int rtDbgModDebugInterpreterRegister(PCRTDBGMODVTDBG pVt)
{
    /*
     * Search or duplicate registration.
     */
    PRTDBGMODREGDBG pPrev = NULL;
    for (PRTDBGMODREGDBG pCur = g_pDbgHead; pCur; pCur = pCur->pNext)
    {
        if (pCur->pVt == pVt)
            return VERR_ALREADY_EXISTS;
        if (!strcmp(pCur->pVt->pszName, pVt->pszName))
            return VERR_ALREADY_EXISTS;
        pPrev = pCur;
    }

    /*
     * Create a new record and add it to the end of the list.
     */
    PRTDBGMODREGDBG pReg = (PRTDBGMODREGDBG)RTMemAlloc(sizeof(*pReg));
    if (!pReg)
        return VERR_NO_MEMORY;
    pReg->pVt    = pVt;
    pReg->cUsers = 0;
    pReg->pNext  = NULL;
    if (pPrev)
        pPrev->pNext = pReg;
    else
        g_pDbgHead   = pReg;
    return VINF_SUCCESS;
}


/**
 * Internal worker for register a image interpreter.
 *
 * Called while owning the write lock or when locking isn't required.
 *
 * @returns IPRT status code.
 * @retval  VERR_NO_MEMORY
 * @retval  VERR_ALREADY_EXISTS
 *
 * @param   pVt                 The virtual function table of the image
 *                              interpreter.
 */
static int rtDbgModImageInterpreterRegister(PCRTDBGMODVTIMG pVt)
{
    /*
     * Search or duplicate registration.
     */
    PRTDBGMODREGIMG pPrev = NULL;
    for (PRTDBGMODREGIMG pCur = g_pImgHead; pCur; pCur = pCur->pNext)
    {
        if (pCur->pVt == pVt)
            return VERR_ALREADY_EXISTS;
        if (!strcmp(pCur->pVt->pszName, pVt->pszName))
            return VERR_ALREADY_EXISTS;
        pPrev = pCur;
    }

    /*
     * Create a new record and add it to the end of the list.
     */
    PRTDBGMODREGIMG pReg = (PRTDBGMODREGIMG)RTMemAlloc(sizeof(*pReg));
    if (!pReg)
        return VERR_NO_MEMORY;
    pReg->pVt    = pVt;
    pReg->cUsers = 0;
    pReg->pNext  = NULL;
    if (pPrev)
        pPrev->pNext = pReg;
    else
        g_pImgHead   = pReg;
    return VINF_SUCCESS;
}


/**
 * Do-once callback that initializes the read/write semaphore and registers
 * the built-in interpreters.
 *
 * @returns IPRT status code.
 * @param   pvUser      NULL.
 */
static DECLCALLBACK(int) rtDbgModInitOnce(void *pvUser)
{
    NOREF(pvUser);

    /*
     * Create the semaphore and string cache.
     */
    int rc = RTSemRWCreate(&g_hDbgModRWSem);
    AssertRCReturn(rc, rc);

    rc = RTStrCacheCreate(&g_hDbgModStrCache, "RTDBGMOD");
    if (RT_SUCCESS(rc))
    {
        /*
         * Register the interpreters.
         */
        rc = rtDbgModDebugInterpreterRegister(&g_rtDbgModVtDbgNm);
        if (RT_SUCCESS(rc))
            rc = rtDbgModDebugInterpreterRegister(&g_rtDbgModVtDbgDwarf);
        if (RT_SUCCESS(rc))
            rc = rtDbgModImageInterpreterRegister(&g_rtDbgModVtImgLdr);
        if (RT_SUCCESS(rc))
        {
            /*
             * Finally, register the IPRT cleanup callback.
             */
            rc = RTTermRegisterCallback(rtDbgModTermCallback, NULL);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;

            /* bail out: use the termination callback. */
        }
    }
    else
        g_hDbgModStrCache = NIL_RTSTRCACHE;
    rtDbgModTermCallback(RTTERMREASON_UNLOAD, 0, NULL);
    return rc;
}


DECLINLINE(int) rtDbgModLazyInit(void)
{
    return RTOnce(&g_rtDbgModOnce, rtDbgModInitOnce, NULL);
}


/**
 * Creates a module based on the default debug info container.
 *
 * This can be used to manually load a module and its symbol. The primary user
 * group is the debug info interpreters, which use this API to create an
 * efficient debug info container behind the scenes and forward all queries to
 * it once the info has been loaded.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgMod        Where to return the module handle.
 * @param   pszName         The name of the module (mandatory).
 * @param   cbSeg           The size of initial segment. If zero, segments will
 *                          have to be added manually using RTDbgModSegmentAdd.
 * @param   fFlags          Flags reserved for future extensions, MBZ for now.
 */
RTDECL(int) RTDbgModCreate(PRTDBGMOD phDbgMod, const char *pszName, RTUINTPTR cbSeg, uint32_t fFlags)
{
    /*
     * Input validation and lazy initialization.
     */
    AssertPtrReturn(phDbgMod, VERR_INVALID_POINTER);
    *phDbgMod = NIL_RTDBGMOD;
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);

    int rc = rtDbgModLazyInit();
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate a new module instance.
     */
    PRTDBGMODINT pDbgMod = (PRTDBGMODINT)RTMemAllocZ(sizeof(*pDbgMod));
    if (!pDbgMod)
        return VERR_NO_MEMORY;
    pDbgMod->u32Magic = RTDBGMOD_MAGIC;
    pDbgMod->cRefs = 1;
    rc = RTCritSectInit(&pDbgMod->CritSect);
    if (RT_SUCCESS(rc))
    {
        pDbgMod->pszName = RTStrCacheEnter(g_hDbgModStrCache, pszName);
        if (pDbgMod->pszName)
        {
            rc = rtDbgModContainerCreate(pDbgMod, cbSeg);
            if (RT_SUCCESS(rc))
            {
                *phDbgMod = pDbgMod;
                return rc;
            }
            RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszName);
        }
        RTCritSectDelete(&pDbgMod->CritSect);
    }

    RTMemFree(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModCreate);


RTDECL(int) RTDbgModCreateDeferred(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName,
                                   RTUINTPTR cb, uint32_t fFlags)
{
    NOREF(phDbgMod); NOREF(pszFilename); NOREF(pszName); NOREF(cb); NOREF(fFlags);
    return VERR_NOT_IMPLEMENTED;
}
RT_EXPORT_SYMBOL(RTDbgModCreateDeferred);


RTDECL(int) RTDbgModCreateFromImage(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName, uint32_t fFlags)
{
    /*
     * Input validation and lazy initialization.
     */
    AssertPtrReturn(phDbgMod, VERR_INVALID_POINTER);
    *phDbgMod = NIL_RTDBGMOD;
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);

    int rc = rtDbgModLazyInit();
    if (RT_FAILURE(rc))
        return rc;

    if (!pszName)
        pszName = RTPathFilename(pszFilename);

    /*
     * Allocate a new module instance.
     */
    PRTDBGMODINT pDbgMod = (PRTDBGMODINT)RTMemAllocZ(sizeof(*pDbgMod));
    if (!pDbgMod)
        return VERR_NO_MEMORY;
    pDbgMod->u32Magic = RTDBGMOD_MAGIC;
    pDbgMod->cRefs = 1;
    rc = RTCritSectInit(&pDbgMod->CritSect);
    if (RT_SUCCESS(rc))
    {
        pDbgMod->pszName = RTStrCacheEnter(g_hDbgModStrCache, pszName);
        if (pDbgMod->pszName)
        {
            pDbgMod->pszImgFile = RTStrCacheEnter(g_hDbgModStrCache, pszFilename);
            if (pDbgMod->pszImgFile)
            {
                /*
                 * Find an image reader which groks the file.
                 */
                rc = RTSemRWRequestRead(g_hDbgModRWSem, RT_INDEFINITE_WAIT);
                if (RT_SUCCESS(rc))
                {
                    rc = VERR_DBG_NO_MATCHING_INTERPRETER;
                    PRTDBGMODREGIMG pImg;
                    for (pImg = g_pImgHead; pImg; pImg = pImg->pNext)
                    {
                        pDbgMod->pImgVt    = pImg->pVt;
                        pDbgMod->pvImgPriv = NULL;
                        rc = pImg->pVt->pfnTryOpen(pDbgMod);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Find a debug info interpreter.
                             */
                            rc = VERR_DBG_NO_MATCHING_INTERPRETER;
                            for (PRTDBGMODREGDBG pDbg = g_pDbgHead; pDbg; pDbg = pDbg->pNext)
                            {
                                pDbgMod->pDbgVt = pDbg->pVt;
                                pDbgMod->pvDbgPriv = NULL;
                                rc = pDbg->pVt->pfnTryOpen(pDbgMod);
                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                     * That's it!
                                     */
                                    ASMAtomicIncU32(&pDbg->cUsers);
                                    ASMAtomicIncU32(&pImg->cUsers);
                                    RTSemRWReleaseRead(g_hDbgModRWSem);

                                    *phDbgMod = pDbgMod;
                                    return rc;
                                }
                            }

                            /*
                             * Image detected, but found no debug info we were
                             * able to understand.
                             */
                            /** @todo Fall back on exported symbols! */
                            pDbgMod->pImgVt->pfnClose(pDbgMod);
                            break;
                        }
                    }

                    /*
                     * Could it be a file containing raw debug info?
                     */
                    if (!pImg)
                    {
                        pDbgMod->pImgVt     = NULL;
                        pDbgMod->pvImgPriv  = NULL;
                        pDbgMod->pszDbgFile = pDbgMod->pszImgFile;
                        pDbgMod->pszImgFile = NULL;

                        for (PRTDBGMODREGDBG pDbg = g_pDbgHead; pDbg; pDbg = pDbg->pNext)
                        {
                            pDbgMod->pDbgVt = pDbg->pVt;
                            pDbgMod->pvDbgPriv = NULL;
                            rc = pDbg->pVt->pfnTryOpen(pDbgMod);
                            if (RT_SUCCESS(rc))
                            {
                                /*
                                 * That's it!
                                 */
                                ASMAtomicIncU32(&pDbg->cUsers);
                                RTSemRWReleaseRead(g_hDbgModRWSem);

                                *phDbgMod = pDbgMod;
                                return rc;
                            }
                        }

                        pDbgMod->pszImgFile = pDbgMod->pszDbgFile;
                        pDbgMod->pszDbgFile = NULL;
                    }

                    /* bail out */
                    RTSemRWReleaseRead(g_hDbgModRWSem);
                }
                RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszName);
            }
            RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszImgFile);
        }
        RTCritSectDelete(&pDbgMod->CritSect);
    }

    RTMemFree(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModCreateFromImage);


RTDECL(int) RTDbgModCreateFromMap(PRTDBGMOD phDbgMod, const char *pszFilename, const char *pszName,
                                  RTUINTPTR uSubtrahend, uint32_t fFlags)
{
    /*
     * Input validation and lazy initialization.
     */
    AssertPtrReturn(phDbgMod, VERR_INVALID_POINTER);
    *phDbgMod = NIL_RTDBGMOD;
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER);
    AssertReturn(uSubtrahend == 0, VERR_NOT_IMPLEMENTED); /** @todo implement uSubtrahend. */

    int rc = rtDbgModLazyInit();
    if (RT_FAILURE(rc))
        return rc;

    if (!pszName)
        pszName = RTPathFilename(pszFilename);

    /*
     * Allocate a new module instance.
     */
    PRTDBGMODINT pDbgMod = (PRTDBGMODINT)RTMemAllocZ(sizeof(*pDbgMod));
    if (!pDbgMod)
        return VERR_NO_MEMORY;
    pDbgMod->u32Magic = RTDBGMOD_MAGIC;
    pDbgMod->cRefs = 1;
    rc = RTCritSectInit(&pDbgMod->CritSect);
    if (RT_SUCCESS(rc))
    {
        pDbgMod->pszName = RTStrCacheEnter(g_hDbgModStrCache, pszName);
        if (pDbgMod->pszName)
        {
            pDbgMod->pszDbgFile = RTStrCacheEnter(g_hDbgModStrCache, pszFilename);
            if (pDbgMod->pszDbgFile)
            {
                /*
                 * Try the map file readers.
                 */
                rc = RTSemRWRequestRead(g_hDbgModRWSem, RT_INDEFINITE_WAIT);
                if (RT_SUCCESS(rc))
                {
                    rc = VERR_DBG_NO_MATCHING_INTERPRETER;
                    for (PRTDBGMODREGDBG pCur = g_pDbgHead; pCur; pCur = pCur->pNext)
                    {
                        if (pCur->pVt->fSupports & RT_DBGTYPE_MAP)
                        {
                            pDbgMod->pDbgVt = pCur->pVt;
                            pDbgMod->pvDbgPriv = NULL;
                            rc = pCur->pVt->pfnTryOpen(pDbgMod);
                            if (RT_SUCCESS(rc))
                            {
                                ASMAtomicIncU32(&pCur->cUsers);
                                RTSemRWReleaseRead(g_hDbgModRWSem);

                                *phDbgMod = pDbgMod;
                                return rc;
                            }
                        }
                    }

                    /* bail out */
                    RTSemRWReleaseRead(g_hDbgModRWSem);
                }
                RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszName);
            }
            RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszDbgFile);
        }
        RTCritSectDelete(&pDbgMod->CritSect);
    }

    RTMemFree(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModCreateFromMap);


/**
 * Destroys an module after the reference count has reached zero.
 *
 * @param   pDbgMod     The module instance.
 */
static void rtDbgModDestroy(PRTDBGMODINT pDbgMod)
{
    /*
     * Close the debug info interpreter first, then the image interpret.
     */
    RTCritSectEnter(&pDbgMod->CritSect); /* paranoia  */

    if (pDbgMod->pDbgVt)
    {
        pDbgMod->pDbgVt->pfnClose(pDbgMod);
        pDbgMod->pDbgVt = NULL;
        pDbgMod->pvDbgPriv = NULL;
    }

    if (pDbgMod->pImgVt)
    {
        pDbgMod->pImgVt->pfnClose(pDbgMod);
        pDbgMod->pImgVt = NULL;
        pDbgMod->pvImgPriv = NULL;
    }

    /*
     * Free the resources.
     */
    ASMAtomicWriteU32(&pDbgMod->u32Magic, ~RTDBGMOD_MAGIC);
    RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszName);
    RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszImgFile);
    RTStrCacheRelease(g_hDbgModStrCache, pDbgMod->pszDbgFile);
    RTCritSectLeave(&pDbgMod->CritSect); /* paranoia  */
    RTCritSectDelete(&pDbgMod->CritSect);
    RTMemFree(pDbgMod);
}


/**
 * Retains another reference to the module.
 *
 * @returns New reference count, UINT32_MAX on invalid handle (asserted).
 *
 * @param   hDbgMod         The module handle.
 *
 * @remarks Will not take any locks.
 */
RTDECL(uint32_t) RTDbgModRetain(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, UINT32_MAX);
    return ASMAtomicIncU32(&pDbgMod->cRefs);
}
RT_EXPORT_SYMBOL(RTDbgModRetain);


/**
 * Release a reference to the module.
 *
 * When the reference count reaches zero, the module is destroyed.
 *
 * @returns New reference count, UINT32_MAX on invalid handle (asserted).
 *
 * @param   hDbgMod         The module handle. The NIL handle is quietly ignored
 *                          and 0 is returned.
 *
 * @remarks Will not take any locks.
 */
RTDECL(uint32_t) RTDbgModRelease(RTDBGMOD hDbgMod)
{
    if (hDbgMod == NIL_RTDBGMOD)
        return 0;
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pDbgMod->cRefs);
    if (!cRefs)
        rtDbgModDestroy(pDbgMod);
    return cRefs;
}
RT_EXPORT_SYMBOL(RTDbgModRelease);


/**
 * Gets the module name.
 *
 * @returns Pointer to a read only string containing the name.
 *
 * @param   hDbgMod         The module handle.
 */
RTDECL(const char *) RTDbgModName(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, NULL);
    return pDbgMod->pszName;
}
RT_EXPORT_SYMBOL(RTDbgModName);


/**
 * Converts an image relative address to a segment:offset address.
 *
 * @returns Segment index on success.
 *          NIL_RTDBGSEGIDX is returned if the module handle or the RVA are
 *          invalid.
 *
 * @param   hDbgMod         The module handle.
 * @param   uRva            The image relative address to convert.
 * @param   poffSeg         Where to return the segment offset. Optional.
 */
RTDECL(RTDBGSEGIDX) RTDbgModRvaToSegOff(RTDBGMOD hDbgMod, RTUINTPTR uRva, PRTUINTPTR poffSeg)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, NIL_RTDBGSEGIDX);
    RTDBGMOD_LOCK(pDbgMod);

    RTDBGSEGIDX iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, uRva, poffSeg);

    RTDBGMOD_UNLOCK(pDbgMod);
    return iSeg;
}
RT_EXPORT_SYMBOL(RTDbgModRvaToSegOff);


/**
 * Image size when mapped if segments are mapped adjacently.
 *
 * For ELF, PE, and Mach-O images this is (usually) a natural query, for LX and
 * NE and such it's a bit odder and the answer may not make much sense for them.
 *
 * @returns Image mapped size.
 *          RTUINTPTR_MAX is returned if the handle is invalid.
 *
 * @param   hDbgMod         The module handle.
 */
RTDECL(RTUINTPTR) RTDbgModImageSize(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, RTUINTPTR_MAX);
    RTDBGMOD_LOCK(pDbgMod);

    RTUINTPTR cbImage = pDbgMod->pDbgVt->pfnImageSize(pDbgMod);

    RTDBGMOD_UNLOCK(pDbgMod);
    return cbImage;
}
RT_EXPORT_SYMBOL(RTDbgModImageSize);


/**
 * Gets the module tag value if any.
 *
 * @returns The tag. 0 if hDbgMod is invalid.
 *
 * @param   hDbgMod         The module handle.
 */
RTDECL(uint64_t) RTDbgModGetTag(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, 0);
    return pDbgMod->uTag;
}
RT_EXPORT_SYMBOL(RTDbgModGetTag);


/**
 * Tags or untags the module.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 *
 * @param   hDbgMod         The module handle.
 * @param   uTag            The tag value.  The convention is that 0 is no tag
 *                          and any other value means it's tagged.  It's adviced
 *                          to use some kind of unique number like an address
 *                          (global or string cache for instance) to avoid
 *                          collisions with other users
 */
RTDECL(int) RTDbgModSetTag(RTDBGMOD hDbgMod, uint64_t uTag)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    RTDBGMOD_LOCK(pDbgMod);

    pDbgMod->uTag = uTag;

    RTDBGMOD_UNLOCK(pDbgMod);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTDbgModSetTag);


/**
 * Adds a segment to the module. Optional feature.
 *
 * This method is intended used for manually constructing debug info for a
 * module. The main usage is from other debug info interpreters that want to
 * avoid writing a debug info database and instead uses the standard container
 * behind the scenes.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if this feature isn't support by the debug info
 *          interpreter. This is a common return code.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_ADDRESS_WRAP if uRva+cb wraps around.
 * @retval  VERR_DBG_SEGMENT_NAME_OUT_OF_RANGE if pszName is too short or long.
 * @retval  VERR_INVALID_PARAMETER if fFlags contains undefined flags.
 * @retval  VERR_DBG_SPECIAL_SEGMENT if *piSeg is a special segment.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if *piSeg doesn't meet expectations.
 *
 * @param   hDbgMod             The module handle.
 * @param   uRva                The image relative address of the segment.
 * @param   cb                  The size of the segment.
 * @param   pszName             The segment name. Does not normally need to be
 *                              unique, although this is somewhat up to the
 *                              debug interpreter to decide.
 * @param   fFlags              Segment flags. Reserved for future used, MBZ.
 * @param   piSeg               The segment index or NIL_RTDBGSEGIDX on input.
 *                              The assigned segment index on successful return.
 *                              Optional.
 */
RTDECL(int) RTDbgModSegmentAdd(RTDBGMOD hDbgMod, RTUINTPTR uRva, RTUINTPTR cb, const char *pszName,
                               uint32_t fFlags, PRTDBGSEGIDX piSeg)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertMsgReturn(uRva + cb >= uRva, ("uRva=%RTptr cb=%RTptr\n", uRva, cb), VERR_DBG_ADDRESS_WRAP);
    Assert(*pszName);
    size_t cchName = strlen(pszName);
    AssertReturn(cchName > 0, VERR_DBG_SEGMENT_NAME_OUT_OF_RANGE);
    AssertReturn(cchName < RTDBG_SEGMENT_NAME_LENGTH, VERR_DBG_SEGMENT_NAME_OUT_OF_RANGE);
    AssertMsgReturn(!fFlags, ("%#x\n", fFlags), VERR_INVALID_PARAMETER);
    AssertPtrNull(piSeg);
    AssertMsgReturn(!piSeg || *piSeg == NIL_RTDBGSEGIDX || *piSeg <= RTDBGSEGIDX_LAST, ("%#x\n", *piSeg), VERR_DBG_SPECIAL_SEGMENT);

    /*
     * Do the deed.
     */
    RTDBGMOD_LOCK(pDbgMod);
    int rc = pDbgMod->pDbgVt->pfnSegmentAdd(pDbgMod, uRva, cb, pszName, cchName, fFlags, piSeg);
    RTDBGMOD_UNLOCK(pDbgMod);

    return rc;

}
RT_EXPORT_SYMBOL(RTDbgModSegmentAdd);


/**
 * Gets the number of segments in the module.
 *
 * This is can be used to determine the range which can be passed to
 * RTDbgModSegmentByIndex and derivatives.
 *
 * @returns The segment relative address.
 *          NIL_RTDBGSEGIDX if the handle is invalid.
 *
 * @param   hDbgMod         The module handle.
 */
RTDECL(RTDBGSEGIDX) RTDbgModSegmentCount(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, NIL_RTDBGSEGIDX);
    RTDBGMOD_LOCK(pDbgMod);

    RTDBGSEGIDX cSegs = pDbgMod->pDbgVt->pfnSegmentCount(pDbgMod);

    RTDBGMOD_UNLOCK(pDbgMod);
    return cSegs;
}
RT_EXPORT_SYMBOL(RTDbgModSegmentCount);


/**
 * Query information about a segment.
 *
 * This can be used together with RTDbgModSegmentCount to enumerate segments.
 * The index starts a 0 and stops one below RTDbgModSegmentCount.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if iSeg is too high.
 * @retval  VERR_DBG_SPECIAL_SEGMENT if iSeg indicates a special segment.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 *
 * @param   hDbgMod         The module handle.
 * @param   iSeg            The segment index. No special segments.
 * @param   pSegInfo        Where to return the segment info. The
 *                          RTDBGSEGMENT::Address member will be set to
 *                          RTUINTPTR_MAX or the load address used at link time.
 */
RTDECL(int) RTDbgModSegmentByIndex(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, PRTDBGSEGMENT pSegInfo)
{
    AssertMsgReturn(iSeg <= RTDBGSEGIDX_LAST, ("%#x\n", iSeg), VERR_DBG_SPECIAL_SEGMENT);
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    RTDBGMOD_LOCK(pDbgMod);

    int rc = pDbgMod->pDbgVt->pfnSegmentByIndex(pDbgMod, iSeg, pSegInfo);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSegmentByIndex);


/**
 * Gets the size of a segment.
 *
 * This is a just a wrapper around RTDbgModSegmentByIndex.
 *
 * @returns The segment size.
 *          RTUINTPTR_MAX is returned if either the handle and segment index are
 *          invalid.
 *
 * @param   hDbgMod         The module handle.
 * @param   iSeg            The segment index. RTDBGSEGIDX_ABS is not allowed.
 *                          If RTDBGSEGIDX_RVA is used, the functions returns
 *                          the same value as RTDbgModImageSize.
 */
RTDECL(RTUINTPTR) RTDbgModSegmentSize(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg)
{
    if (iSeg == RTDBGSEGIDX_RVA)
        return RTDbgModImageSize(hDbgMod);
    RTDBGSEGMENT SegInfo;
    int rc = RTDbgModSegmentByIndex(hDbgMod, iSeg, &SegInfo);
    return RT_SUCCESS(rc) ? SegInfo.cb : RTUINTPTR_MAX;
}
RT_EXPORT_SYMBOL(RTDbgModSegmentSize);


/**
 * Gets the image relative address of a segment.
 *
 * This is a just a wrapper around RTDbgModSegmentByIndex.
 *
 * @returns The segment relative address.
 *          RTUINTPTR_MAX is returned if either the handle and segment index are
 *          invalid.
 *
 * @param   hDbgMod         The module handle.
 * @param   iSeg            The segment index. No special segment indexes
 *                          allowed (asserted).
 */
RTDECL(RTUINTPTR) RTDbgModSegmentRva(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg)
{
    RTDBGSEGMENT SegInfo;
    int rc = RTDbgModSegmentByIndex(hDbgMod, iSeg, &SegInfo);
    return RT_SUCCESS(rc) ? SegInfo.uRva : RTUINTPTR_MAX;
}
RT_EXPORT_SYMBOL(RTDbgModSegmentRva);


/**
 * Adds a line number to the module.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if the module interpret doesn't support adding
 *          custom symbols. This is a common place occurrence.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE if the symbol name is too long or
 *          short.
 * @retval  VERR_DBG_INVALID_RVA if an image relative address is specified and
 *          it's not inside any of the segments defined by the module.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if the segment index isn't valid.
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET if the segment offset is beyond the
 *          end of the segment.
 * @retval  VERR_DBG_ADDRESS_WRAP if off+cb wraps around.
 * @retval  VERR_INVALID_PARAMETER if the symbol flags sets undefined bits.
 *
 * @param   hDbgMod         The module handle.
 * @param   pszSymbol       The symbol name.
 * @param   iSeg            The segment index.
 * @param   off             The segment offset.
 * @param   cb              The size of the symbol. Can be zero, although this
 *                          may depend somewhat on the debug interpreter.
 * @param   fFlags          Symbol flags. Reserved for the future, MBZ.
 * @param   piOrdinal       Where to return the symbol ordinal on success. If
 *                          the interpreter doesn't do ordinals, this will be set to
 *                          UINT32_MAX. Optional.
 */
RTDECL(int) RTDbgModSymbolAdd(RTDBGMOD hDbgMod, const char *pszSymbol, RTDBGSEGIDX iSeg, RTUINTPTR off,
                              RTUINTPTR cb, uint32_t fFlags, uint32_t *piOrdinal)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtr(pszSymbol);
    size_t cchSymbol = strlen(pszSymbol);
    AssertReturn(cchSymbol, VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE);
    AssertReturn(cchSymbol < RTDBG_SYMBOL_NAME_LENGTH, VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE);
    AssertMsgReturn(   iSeg <= RTDBGSEGIDX_LAST
                    || (    iSeg >= RTDBGSEGIDX_SPECIAL_FIRST
                        &&  iSeg <= RTDBGSEGIDX_SPECIAL_LAST),
                    ("%#x\n", iSeg),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertMsgReturn(off + cb >= off, ("off=%RTptr cb=%RTptr\n", off, cb), VERR_DBG_ADDRESS_WRAP);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER); /* currently reserved. */

    RTDBGMOD_LOCK(pDbgMod);

    /*
     * Convert RVAs.
     */
    if (iSeg == RTDBGSEGIDX_RVA)
    {
        iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, off, &off);
        if (iSeg == NIL_RTDBGSEGIDX)
        {
            RTDBGMOD_UNLOCK(pDbgMod);
            return VERR_DBG_INVALID_RVA;
        }
    }

    /*
     * Get down to business.
     */
    int rc = pDbgMod->pDbgVt->pfnSymbolAdd(pDbgMod, pszSymbol, cchSymbol, iSeg, off, cb, fFlags, piOrdinal);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolAdd);


/**
 * Gets the symbol count.
 *
 * This can be used together wtih RTDbgModSymbolByOrdinal or
 * RTDbgModSymbolByOrdinalA to enumerate all the symbols.
 *
 * @returns The number of symbols in the module.
 *          UINT32_MAX is returned if the module handle is invalid or some other
 *          error occurs.
 *
 * @param   hDbgMod             The module handle.
 */
RTDECL(uint32_t) RTDbgModSymbolCount(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, UINT32_MAX);
    RTDBGMOD_LOCK(pDbgMod);

    uint32_t cSymbols = pDbgMod->pDbgVt->pfnSymbolCount(pDbgMod);

    RTDBGMOD_UNLOCK(pDbgMod);
    return cSymbols;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolCount);


/**
 * Queries symbol information by ordinal number.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if there is no symbol at the given number.
 * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_NOT_SUPPORTED if lookup by ordinal is not supported.
 *
 * @param   hDbgMod             The module handle.
 * @param   iOrdinal            The symbol ordinal number. 0-based. The highest
 *                              number is RTDbgModSymbolCount() - 1.
 * @param   pSymInfo            Where to store the symbol information.
 */
RTDECL(int) RTDbgModSymbolByOrdinal(RTDBGMOD hDbgMod, uint32_t iOrdinal, PRTDBGSYMBOL pSymInfo)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    RTDBGMOD_LOCK(pDbgMod);

    int rc = pDbgMod->pDbgVt->pfnSymbolByOrdinal(pDbgMod, iOrdinal, pSymInfo);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolByOrdinal);


/**
 * Queries symbol information by ordinal number.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
 * @retval  VERR_NOT_SUPPORTED if lookup by ordinal is not supported.
 * @retval  VERR_SYMBOL_NOT_FOUND if there is no symbol at the given number.
 * @retval  VERR_NO_MEMORY if RTDbgSymbolAlloc fails.
 *
 * @param   hDbgMod             The module handle.
 * @param   iOrdinal            The symbol ordinal number. 0-based. The highest
 *                              number is RTDbgModSymbolCount() - 1.
 * @param   ppSymInfo           Where to store the pointer to the returned
 *                              symbol information. Always set. Free with
 *                              RTDbgSymbolFree.
 */
RTDECL(int) RTDbgModSymbolByOrdinalA(RTDBGMOD hDbgMod, uint32_t iOrdinal, PRTDBGSYMBOL *ppSymInfo)
{
    AssertPtr(ppSymInfo);
    *ppSymInfo = NULL;

    PRTDBGSYMBOL pSymInfo = RTDbgSymbolAlloc();
    if (!pSymInfo)
        return VERR_NO_MEMORY;

    int rc = RTDbgModSymbolByOrdinal(hDbgMod, iOrdinal, pSymInfo);

    if (RT_SUCCESS(rc))
        *ppSymInfo = pSymInfo;
    else
        RTDbgSymbolFree(pSymInfo);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolByOrdinalA);


/**
 * Queries symbol information by address.
 *
 * The returned symbol is what the debug info interpreter considers the symbol
 * most applicable to the specified address. This usually means a symbol with an
 * address equal or lower than the requested.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
 * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_INVALID_RVA if an image relative address is specified and
 *          it's not inside any of the segments defined by the module.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if the segment index isn't valid.
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET if the segment offset is beyond the
 *          end of the segment.
 * @retval  VERR_INVALID_PARAMETER if incorrect flags.
 *
 * @param   hDbgMod             The module handle.
 * @param   iSeg                The segment number.
 * @param   off                 The offset into the segment.
 * @param   fFlags              Symbol search flags, see RTDBGSYMADDR_FLAGS_XXX.
 * @param   poffDisp            Where to store the distance between the
 *                              specified address and the returned symbol.
 *                              Optional.
 * @param   pSymInfo            Where to store the symbol information.
 */
RTDECL(int) RTDbgModSymbolByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t fFlags,
                                 PRTINTPTR poffDisp, PRTDBGSYMBOL pSymInfo)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtrNull(poffDisp);
    AssertPtr(pSymInfo);
    AssertReturn(!(fFlags & ~RTDBGSYMADDR_FLAGS_VALID_MASK), VERR_INVALID_PARAMETER);

    RTDBGMOD_LOCK(pDbgMod);

    /*
     * Convert RVAs.
     */
    if (iSeg == RTDBGSEGIDX_RVA)
    {
        iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, off, &off);
        if (iSeg == NIL_RTDBGSEGIDX)
        {
            RTDBGMOD_UNLOCK(pDbgMod);
            return VERR_DBG_INVALID_RVA;
        }
    }

    /*
     * Get down to business.
     */
    int rc = pDbgMod->pDbgVt->pfnSymbolByAddr(pDbgMod, iSeg, off, fFlags, poffDisp, pSymInfo);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolByAddr);


/**
 * Queries symbol information by address.
 *
 * The returned symbol is what the debug info interpreter considers the symbol
 * most applicable to the specified address. This usually means a symbol with an
 * address equal or lower than the requested.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
 * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_INVALID_RVA if an image relative address is specified and
 *          it's not inside any of the segments defined by the module.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if the segment index isn't valid.
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET if the segment offset is beyond the
 *          end of the segment.
 * @retval  VERR_NO_MEMORY if RTDbgSymbolAlloc fails.
 * @retval  VERR_INVALID_PARAMETER if incorrect flags.
 *
 * @param   hDbgMod             The module handle.
 * @param   iSeg                The segment index.
 * @param   off                 The offset into the segment.
 * @param   fFlags              Symbol search flags, see RTDBGSYMADDR_FLAGS_XXX.
 * @param   poffDisp            Where to store the distance between the
 *                              specified address and the returned symbol. Optional.
 * @param   ppSymInfo           Where to store the pointer to the returned
 *                              symbol information. Always set. Free with
 *                              RTDbgSymbolFree.
 */
RTDECL(int) RTDbgModSymbolByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t fFlags,
                                  PRTINTPTR poffDisp, PRTDBGSYMBOL *ppSymInfo)
{
    AssertPtr(ppSymInfo);
    *ppSymInfo = NULL;

    PRTDBGSYMBOL pSymInfo = RTDbgSymbolAlloc();
    if (!pSymInfo)
        return VERR_NO_MEMORY;

    int rc = RTDbgModSymbolByAddr(hDbgMod, iSeg, off, fFlags, poffDisp, pSymInfo);

    if (RT_SUCCESS(rc))
        *ppSymInfo = pSymInfo;
    else
        RTDbgSymbolFree(pSymInfo);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolByAddrA);


/**
 * Queries symbol information by symbol name.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
 * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
 * @retval  VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE if the symbol name is too long or
 *          short.
 *
 * @param   hDbgMod             The module handle.
 * @param   pszSymbol           The symbol name.
 * @param   pSymInfo            Where to store the symbol information.
 */
RTDECL(int) RTDbgModSymbolByName(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL pSymInfo)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtr(pszSymbol);
    size_t cchSymbol = strlen(pszSymbol);
    AssertReturn(cchSymbol, VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE);
    AssertReturn(cchSymbol < RTDBG_SYMBOL_NAME_LENGTH, VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE);
    AssertPtr(pSymInfo);

    /*
     * Make the query.
     */
    RTDBGMOD_LOCK(pDbgMod);
    int rc = pDbgMod->pDbgVt->pfnSymbolByName(pDbgMod, pszSymbol, cchSymbol, pSymInfo);
    RTDBGMOD_UNLOCK(pDbgMod);

    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolByName);


/**
 * Queries symbol information by symbol name.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_SYMBOLS if there aren't any symbols.
 * @retval  VERR_SYMBOL_NOT_FOUND if no suitable symbol was found.
 * @retval  VERR_DBG_SYMBOL_NAME_OUT_OF_RANGE if the symbol name is too long or
 *          short.
 * @retval  VERR_NO_MEMORY if RTDbgSymbolAlloc fails.
 *
 * @param   hDbgMod             The module handle.
 * @param   pszSymbol           The symbol name.
 * @param   ppSymInfo           Where to store the pointer to the returned
 *                              symbol information. Always set. Free with
 *                              RTDbgSymbolFree.
 */
RTDECL(int) RTDbgModSymbolByNameA(RTDBGMOD hDbgMod, const char *pszSymbol, PRTDBGSYMBOL *ppSymInfo)
{
    AssertPtr(ppSymInfo);
    *ppSymInfo = NULL;

    PRTDBGSYMBOL pSymInfo = RTDbgSymbolAlloc();
    if (!pSymInfo)
        return VERR_NO_MEMORY;

    int rc = RTDbgModSymbolByName(hDbgMod, pszSymbol, pSymInfo);

    if (RT_SUCCESS(rc))
        *ppSymInfo = pSymInfo;
    else
        RTDbgSymbolFree(pSymInfo);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModSymbolByNameA);


/**
 * Adds a line number to the module.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if the module interpret doesn't support adding
 *          custom symbols. This should be consider a normal response.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_FILE_NAME_OUT_OF_RANGE if the file name is too longer or
 *          empty.
 * @retval  VERR_DBG_INVALID_RVA if an image relative address is specified and
 *          it's not inside any of the segments defined by the module.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if the segment index isn't valid.
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET if the segment offset is beyond the
 *          end of the segment.
 * @retval  VERR_INVALID_PARAMETER if the line number flags sets undefined bits.
 *
 * @param   hDbgMod             The module handle.
 * @param   pszFile             The file name.
 * @param   uLineNo             The line number.
 * @param   iSeg                The segment index.
 * @param   off                 The segment offset.
 * @param   piOrdinal           Where to return the line number ordinal on
 *                              success. If  the interpreter doesn't do ordinals,
 *                              this will be set to UINT32_MAX. Optional.
 */
RTDECL(int) RTDbgModLineAdd(RTDBGMOD hDbgMod, const char *pszFile, uint32_t uLineNo,
                            RTDBGSEGIDX iSeg, RTUINTPTR off, uint32_t *piOrdinal)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtr(pszFile);
    size_t cchFile = strlen(pszFile);
    AssertReturn(cchFile, VERR_DBG_FILE_NAME_OUT_OF_RANGE);
    AssertReturn(cchFile < RTDBG_FILE_NAME_LENGTH, VERR_DBG_FILE_NAME_OUT_OF_RANGE);
    AssertMsgReturn(   iSeg <= RTDBGSEGIDX_LAST
                    || iSeg == RTDBGSEGIDX_RVA,
                    ("%#x\n", iSeg),
                    VERR_DBG_INVALID_SEGMENT_INDEX);
    AssertReturn(uLineNo > 0 && uLineNo < UINT32_MAX, VERR_INVALID_PARAMETER);

    RTDBGMOD_LOCK(pDbgMod);

    /*
     * Convert RVAs.
     */
    if (iSeg == RTDBGSEGIDX_RVA)
    {
        iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, off, &off);
        if (iSeg == NIL_RTDBGSEGIDX)
        {
            RTDBGMOD_UNLOCK(pDbgMod);
            return VERR_DBG_INVALID_RVA;
        }
    }

    /*
     * Get down to business.
     */
    int rc = pDbgMod->pDbgVt->pfnLineAdd(pDbgMod, pszFile, cchFile, uLineNo, iSeg, off, piOrdinal);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModLineAdd);


/**
 * Gets the line number count.
 *
 * This can be used together wtih RTDbgModLineByOrdinal or RTDbgModSymbolByLineA
 * to enumerate all the line number information.
 *
 * @returns The number of line numbers in the module.
 *          UINT32_MAX is returned if the module handle is invalid or some other
 *          error occurs.
 *
 * @param   hDbgMod             The module handle.
 */
RTDECL(uint32_t) RTDbgModLineCount(RTDBGMOD hDbgMod)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, UINT32_MAX);
    RTDBGMOD_LOCK(pDbgMod);

    uint32_t cLineNumbers = pDbgMod->pDbgVt->pfnLineCount(pDbgMod);

    RTDBGMOD_UNLOCK(pDbgMod);
    return cLineNumbers;
}
RT_EXPORT_SYMBOL(RTDbgModLineCount);


/**
 * Queries line number information by ordinal number.
 *
 * This can be used to enumerate the line numbers for the module. Use
 * RTDbgModLineCount() to figure the end of the ordinals.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_LINE_NUMBERS if there aren't any line numbers.
 * @retval  VERR_DBG_LINE_NOT_FOUND if there is no line number with that
 *          ordinal.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.

 * @param   hDbgMod             The module handle.
 * @param   iOrdinal            The line number ordinal number.
 * @param   pLineInfo           Where to store the information about the line
 *                              number.
 */
RTDECL(int) RTDbgModLineByOrdinal(RTDBGMOD hDbgMod, uint32_t iOrdinal, PRTDBGLINE pLineInfo)
{
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    RTDBGMOD_LOCK(pDbgMod);

    int rc = pDbgMod->pDbgVt->pfnLineByOrdinal(pDbgMod, iOrdinal, pLineInfo);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModLineByOrdinal);


/**
 * Queries line number information by ordinal number.
 *
 * This can be used to enumerate the line numbers for the module. Use
 * RTDbgModLineCount() to figure the end of the ordinals.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_LINE_NUMBERS if there aren't any line numbers.
 * @retval  VERR_DBG_LINE_NOT_FOUND if there is no line number with that
 *          ordinal.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_NO_MEMORY if RTDbgLineAlloc fails.
 *
 * @param   hDbgMod             The module handle.
 * @param   iOrdinal            The line number ordinal number.
 * @param   ppLineInfo          Where to store the pointer to the returned line
 *                              number information. Always set. Free with
 *                              RTDbgLineFree.
 */
RTDECL(int) RTDbgModLineByOrdinalA(RTDBGMOD hDbgMod, uint32_t iOrdinal, PRTDBGLINE *ppLineInfo)
{
    AssertPtr(ppLineInfo);
    *ppLineInfo = NULL;

    PRTDBGLINE pLineInfo = RTDbgLineAlloc();
    if (!pLineInfo)
        return VERR_NO_MEMORY;

    int rc = RTDbgModLineByOrdinal(hDbgMod, iOrdinal, pLineInfo);

    if (RT_SUCCESS(rc))
        *ppLineInfo = pLineInfo;
    else
        RTDbgLineFree(pLineInfo);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModLineByOrdinalA);


/**
 * Queries line number information by address.
 *
 * The returned line number is what the debug info interpreter considers the
 * one most applicable to the specified address. This usually means a line
 * number with an address equal or lower than the requested.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_LINE_NUMBERS if there aren't any line numbers.
 * @retval  VERR_DBG_LINE_NOT_FOUND if no suitable line number was found.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_INVALID_RVA if an image relative address is specified and
 *          it's not inside any of the segments defined by the module.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if the segment index isn't valid.
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET if the segment offset is beyond the
 *          end of the segment.
 *
 * @param   hDbgMod             The module handle.
 * @param   iSeg                The segment number.
 * @param   off                 The offset into the segment.
 * @param   poffDisp            Where to store the distance between the
 *                              specified address and the returned symbol.
 *                              Optional.
 * @param   pLineInfo           Where to store the line number information.
 */
RTDECL(int) RTDbgModLineByAddr(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE pLineInfo)
{
    /*
     * Validate input.
     */
    PRTDBGMODINT pDbgMod = hDbgMod;
    RTDBGMOD_VALID_RETURN_RC(pDbgMod, VERR_INVALID_HANDLE);
    AssertPtrNull(poffDisp);
    AssertPtr(pLineInfo);

    RTDBGMOD_LOCK(pDbgMod);

    /*
     * Convert RVAs.
     */
    if (iSeg == RTDBGSEGIDX_RVA)
    {
        iSeg = pDbgMod->pDbgVt->pfnRvaToSegOff(pDbgMod, off, &off);
        if (iSeg == NIL_RTDBGSEGIDX)
        {
            RTDBGMOD_UNLOCK(pDbgMod);
            return VERR_DBG_INVALID_RVA;
        }
    }

    int rc = pDbgMod->pDbgVt->pfnLineByAddr(pDbgMod, iSeg, off, poffDisp, pLineInfo);

    RTDBGMOD_UNLOCK(pDbgMod);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModLineByAddr);


/**
 * Queries line number information by address.
 *
 * The returned line number is what the debug info interpreter considers the
 * one most applicable to the specified address. This usually means a line
 * number with an address equal or lower than the requested.
 *
 * @returns IPRT status code.
 * @retval  VERR_DBG_NO_LINE_NUMBERS if there aren't any line numbers.
 * @retval  VERR_DBG_LINE_NOT_FOUND if no suitable line number was found.
 * @retval  VERR_INVALID_HANDLE if hDbgMod is invalid.
 * @retval  VERR_DBG_INVALID_RVA if an image relative address is specified and
 *          it's not inside any of the segments defined by the module.
 * @retval  VERR_DBG_INVALID_SEGMENT_INDEX if the segment index isn't valid.
 * @retval  VERR_DBG_INVALID_SEGMENT_OFFSET if the segment offset is beyond the
 *          end of the segment.
 * @retval  VERR_NO_MEMORY if RTDbgLineAlloc fails.
 *
 * @param   hDbgMod             The module handle.
 * @param   iSeg                The segment number.
 * @param   off                 The offset into the segment.
 * @param   poffDisp            Where to store the distance between the
 *                              specified address and the returned symbol.
 *                              Optional.
 * @param   ppLineInfo          Where to store the pointer to the returned line
 *                              number information. Always set. Free with
 *                              RTDbgLineFree.
 */
RTDECL(int) RTDbgModLineByAddrA(RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR off, PRTINTPTR poffDisp, PRTDBGLINE *ppLineInfo)
{
    AssertPtr(ppLineInfo);
    *ppLineInfo = NULL;

    PRTDBGLINE pLineInfo = RTDbgLineAlloc();
    if (!pLineInfo)
        return VERR_NO_MEMORY;

    int rc = RTDbgModLineByAddr(hDbgMod, iSeg, off, poffDisp, pLineInfo);

    if (RT_SUCCESS(rc))
        *ppLineInfo = pLineInfo;
    else
        RTDbgLineFree(pLineInfo);
    return rc;
}
RT_EXPORT_SYMBOL(RTDbgModLineByAddrA);

