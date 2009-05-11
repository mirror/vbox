/* $Id$ */
/** @file
 * IPRT - Debug Address Space.
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
#include <iprt/dbg.h>
#include <iprt/asm.h>
#include <iprt/avl.h>
#include <iprt/string.h>

#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/param.h>
//#include "internal/dbg.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to a module table entry. */
typedef struct RTDBGASMOD *PRTDBGASMOD;
/** Pointer to an address space mapping node. */
typedef struct RTDBGASMAP *PRTDBGASMAP;
/** Pointer to a name head. */
typedef struct RTDBGASNAME *PRTDBGASNAME;

/**
 * Module entry.
 */
typedef struct RTDBGASMOD
{
    /** Node core, the module handle is the key. */
    AVLPVNODECORE       Core;
    /** Pointer to the first mapping of the module or a segment within it. */
    RTDBGASMAP         *pMapHead;
    /** Pointer to the next module with an identical name. */
    PRTDBGASMOD         pNextName;
} RTDBGASMOD;

/**
 * An address space mapping, either of a full module or a segment.
 */
typedef struct RTDBGASMAP
{
    /** The AVL node core. Contains the address range. */
    AVLRUINTPTRNODECORE Core;
    /** Pointer to the next mapping of the module. */
    PRTDBGASMAP         pNext;
    /** Pointer to the module. */
    PRTDBGASMOD         pMod;
    /** Which segment in the module.
     * This is NIL_RTDBGSEGIDX when the entire module is mapped. */
    RTDBGSEGIDX         iSeg;
} RTDBGASMAP;

/**
 * Name in the address space.
 */
typedef struct RTDBGASNAME
{
    /** The string space node core.*/
    RTSTRSPACECORE      StrCore;
    /** The list of nodes  */
    PRTDBGASMOD         pHead;
} RTDBGASNAME;

/**
 * Debug address space instance.
 */
typedef struct RTDBGASINT
{
    /** Magic value (RTDBGAS_MAGIC). */
    uint32_t            u32Magic;
    /** Number of modules in the module address space. */
    uint32_t            cModules;
    /** Pointer to the module table.
     * The valid array length is given by cModules. */
    PRTDBGASMOD         paModules;
    /** AVL tree translating module handles to module entries. */
    AVLPVTREE           ModTree;
    /** AVL tree mapping addresses to modules. */
    AVLRUINTPTRTREE     MapTree;
    /** Names of the modules in the name space. */
    RTSTRSPACE          NameSpace;
    /** The first address the AS. */
    RTUINTPTR           FirstAddr;
    /** The last address in the AS. */
    RTUINTPTR           LastAddr;
    /** The name of the address space. (variable length) */
    char                szName[1];
} RTDBGASINT;
/** Pointer to an a debug address space instance. */
typedef RTDBGASINT *PRTDBGASINT;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a context handle and returns rc if not valid. */
#define RTDBGAS_VALID_RETURN_RC(pDbgAs, rc) \
    do { \
        AssertPtrReturn((pDbgAs), (rc)); \
        AssertReturn((pDbgAs)->u32Magic == RTDBGAS_MAGIC, (rc)); \
    } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtDbgAsModuleUnlinkMod(PRTDBGASINT pDbgAs, PRTDBGASMOD pMod);


/**
 * Creates an empty address space.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgAs         Where to store the address space handle on success.
 * @param   FirstAddr       The first address in the address space.
 * @param   LastAddr        The last address in the address space.
 * @param   pszName         The name of the address space.
 */
RTDECL(int) RTDbgAsCreate(PRTDBGAS phDbgAs, RTUINTPTR FirstAddr, RTUINTPTR LastAddr, const char *pszName)
{
    /*
     * Input validation.
     */
    AssertPtrReturn(phDbgAs, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(FirstAddr < LastAddr, VERR_INVALID_PARAMETER);

    /*
     * Allocate memory for the instance data.
     */
    size_t cchName = strlen(pszName);
    PRTDBGASINT pDbgAs = (PRTDBGASINT)RTMemAlloc(RT_OFFSETOF(RTDBGASINT, szName[cchName + 1]));
    if (!pDbgAs)
        return VERR_NO_MEMORY;

    /* initalize it. */
    pDbgAs->u32Magic    = RTDBGAS_MAGIC;
    pDbgAs->cModules    = 0;
    pDbgAs->paModules   = NULL;
    pDbgAs->ModTree     = NULL;
    pDbgAs->MapTree     = NULL;
    pDbgAs->NameSpace   = NULL;
    pDbgAs->FirstAddr   = FirstAddr;
    pDbgAs->LastAddr    = LastAddr;
    memcpy(pDbgAs->szName, pszName, cchName + 1);

    *phDbgAs = pDbgAs;
    return VINF_SUCCESS;
}


/**
 * Variant of RTDbgAsCreate that takes a name format string.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgAs         Where to store the address space handle on success.
 * @param   FirstAddr       The first address in the address space.
 * @param   LastAddr        The last address in the address space.
 * @param   pszNameFmt      The name format of the address space.
 * @param   va              Format arguments.
 */
RTDECL(int) RTDbgAsCreateV(PRTDBGAS phDbgAs, RTUINTPTR FirstAddr, RTUINTPTR LastAddr, const char *pszNameFmt, va_list va)
{
    AssertPtrReturn(pszNameFmt, VERR_INVALID_POINTER);

    char *pszName;
    RTStrAPrintfV(&pszName, pszNameFmt, va);
    if (!pszName)
        return VERR_NO_MEMORY;

    int rc = RTDbgAsCreate(phDbgAs, FirstAddr, LastAddr, pszName);

    RTStrFree(pszName);
    return rc;
}


/**
 * Variant of RTDbgAsCreate that takes a name format string.
 *
 * @returns IPRT status code.
 *
 * @param   phDbgAs         Where to store the address space handle on success.
 * @param   FirstAddr       The first address in the address space.
 * @param   LastAddr        The last address in the address space.
 * @param   pszNameFmt      The name format of the address space.
 * @param   ...             Format arguments.
 */
RTDECL(int) RTDbgAsCreateF(PRTDBGAS phDbgAs, RTUINTPTR FirstAddr, RTUINTPTR LastAddr, const char *pszNameFmt, ...)
{
    va_list va;
    va_start(va, pszNameFmt);
    int rc = RTDbgAsCreateV(phDbgAs, FirstAddr, LastAddr, pszNameFmt, va);
    va_end(va);
    return rc;
}


/**
 * Callback used by RTDbgAsDestroy to free all mapping nodes.
 *
 * @returns 0
 * @param   pNode           The map node.
 * @param   pvUser          NULL.
 */
static DECLCALLBACK(int) rtDbgAsDestroyMapCallback(PAVLRUINTPTRNODECORE pNode, void *pvUser)
{
    RTMemFree(pNode);
    NOREF(pvUser);
    return 0;
}


/**
 * Callback used by RTDbgAsDestroy to free all name space nodes.
 *
 * @returns 0
 * @param   pStr            The name node.
 * @param   pvUser          NULL.
 */
static DECLCALLBACK(int) rtDbgAsDestroyNameCallback(PRTSTRSPACECORE pStr, void *pvUser)
{
    RTMemFree(pStr);
    NOREF(pvUser);
    return 0;
}


/**
 * Destroys the address space.
 *
 * This means unlinking all the modules it currently contains, potentially
 * causing some or all of them to be destroyed as they are managed by
 * reference counting.
 *
 * @returns IPRT status code.
 *
 * @param   hDbgAs          The address space handle. A NIL handle will
 *                          be quietly ignored.
 */
RTDECL(int) RTDbgAsDestroy(RTDBGAS hDbgAs)
{
    /*
     * Validate input.
     */
    if (hDbgAs == NIL_RTDBGAS)
        return VINF_SUCCESS;
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);

    /*
     * Mark the address space invalid and release all the modules.
     */
    ASMAtomicWriteU32(&pDbgAs->u32Magic, ~RTDBGAS_MAGIC);

    RTAvlrUIntPtrDestroy(&pDbgAs->MapTree, rtDbgAsDestroyMapCallback, NULL);
    RTStrSpaceDestroy(&pDbgAs->NameSpace, rtDbgAsDestroyNameCallback, NULL);

    uint32_t i = pDbgAs->cModules;
    while (i-- > 0)
    {
        RTDbgModRelease((RTDBGMOD)pDbgAs->paModules[i].Core.Key);
        pDbgAs->paModules[i].Core.Key = NIL_RTDBGMOD;
    }
    RTMemFree(pDbgAs->paModules);
    pDbgAs->paModules = NULL;

    RTMemFree(pDbgAs);

    return VINF_SUCCESS;
}


/**
 * Gets the name of an address space.
 *
 * @returns read only address space name.
 *          NULL if hDbgAs is invalid.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(const char *) RTDbgAsName(RTDBGAS hDbgAs)
{
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, NULL);
    return pDbgAs->szName;
}


/**
 * Gets the first address in an address space.
 *
 * @returns The address.
 *          0 if hDbgAs is invalid.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(RTUINTPTR) RTDbgAsFirstAddr(RTDBGAS hDbgAs)
{
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, 0);
    return pDbgAs->FirstAddr;
}


/**
 * Gets the last address in an address space.
 *
 * @returns The address.
 *          0 if hDbgAs is invalid.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(RTUINTPTR) RTDbgAsLastAddr(RTDBGAS hDbgAs)
{
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, 0);
    return pDbgAs->LastAddr;
}

/**
 * Gets the number of modules in the address space.
 *
 * This can be used together with RTDbgAsModuleByIndex
 * to enumerate the modules.
 *
 * @returns The number of modules.
 *
 * @param   hDbgAs          The address space handle.
 */
RTDECL(uint32_t)    RTDbgAsModuleCount(RTDBGAS hDbgAs)
{
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, 0);
    return pDbgAs->cModules;
}


/**
 * Common worker for RTDbgAsModuleLink and RTDbgAsModuleLinkSeg.
 *
 * @returns IPRT status.
 * @param   pDbgAs          Pointer to the address space instance data.
 * @param   hDbgMod         The module to link.
 * @param   iSeg            The segment to link or NIL if all.
 * @param   Addr            The address we're linking it at.
 * @param   cb              The size of what we're linking.
 * @param   pszName         The name of the module.
 */
int rtDbgAsModuleLinkCommon(PRTDBGASINT pDbgAs, RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg,
                            RTUINTPTR Addr, RTUINTPTR cb, const char *pszName)
{
    /*
     * Check that the requested space is undisputed.
     */
   PRTDBGASMAP pAdjMod = (PRTDBGASMAP)RTAvlrUIntPtrGetBestFit(&pDbgAs->MapTree, Addr, false /* fAbove */);
   if (     pAdjMod
       &&   pAdjMod->Core.KeyLast >= Addr)
       return VERR_ADDRESS_CONFLICT;
   pAdjMod = (PRTDBGASMAP)RTAvlrUIntPtrGetBestFit(&pDbgAs->MapTree, Addr, true /* fAbove */);
   if (     pAdjMod
       &&   pAdjMod->Core.Key >= Addr + cb - 1)
       return VERR_ADDRESS_CONFLICT;

    /*
     * First, create or find the module table entry.
     */
    PRTDBGASMOD pMod = (PRTDBGASMOD)RTAvlPVGet(&pDbgAs->ModTree, hDbgMod);
    if (!pMod)
    {
        /*
         * Ok, we need a new entry. Grow the table if necessary.
         */
        if (!(pDbgAs->cModules % 32))
        {
            void *pvNew = RTMemRealloc(pDbgAs->paModules, sizeof(pDbgAs->paModules[0]) * (pDbgAs->cModules + 32));
            if (!pvNew)
                return VERR_NO_MEMORY;
            pDbgAs->paModules = (PRTDBGASMOD)pvNew;
        }
        pMod = &pDbgAs->paModules[pDbgAs->cModules];
        pDbgAs->cModules++;

        pMod->Core.Key  = hDbgMod;
        pMod->pMapHead  = NULL;
        pMod->pNextName = NULL;
        if (!RTAvlPVInsert(&pDbgAs->ModTree, &pMod->Core))
        {
            AssertFailed();
            pDbgAs->cModules--;
            return VERR_INTERNAL_ERROR;
        }
        RTDbgModRetain(hDbgMod);

        /*
         * Add it to the name space.
         */
        PRTDBGASNAME pName = (PRTDBGASNAME)RTStrSpaceGet(&pDbgAs->NameSpace, pszName);
        if (!pName)
        {
            size_t cchName = strlen(pszName);
            pName = (PRTDBGASNAME)RTMemAlloc(sizeof(*pName) + cchName + 1);
            if (!pName)
            {
                pDbgAs->cModules--;
                RTDbgModRelease(hDbgMod);
                return VERR_NO_MEMORY;
            }
            pName->StrCore.cchString = cchName;
            pName->StrCore.pszString = (char *)memcpy(pName + 1, pszName, cchName + 1);
            pName->pHead = pMod;
        }
        else
        {
            /* quick, but unfair. */
            pMod->pNextName = pName->pHead;
            pName->pHead = pMod;
        }
    }

    /*
     * Create a mapping node.
     */
    int rc;
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTMemAlloc(sizeof(*pMap));
    if (pMap)
    {
        pMap->Core.Key      = Addr;
        pMap->Core.KeyLast  = Addr + cb - 1;
        pMap->pMod          = pMod;
        pMap->iSeg          = iSeg;
        if (RTAvlrUIntPtrInsert(&pDbgAs->MapTree, &pMap->Core))
        {
            PRTDBGASMAP *pp = &pMod->pMapHead;
            while (*pp && (*pp)->Core.Key < Addr)
                pp = &(*pp)->pNext;
            pMap->pNext = *pp;
            *pp = pMap;
            return VINF_SUCCESS;
        }

        AssertFailed();
        RTMemFree(pMap);
        rc = VERR_ADDRESS_CONFLICT;
    }
    else
        rc = VERR_NO_MEMORY;

    /*
     * Unlink the module if this was the only mapping.
     */
    if (!pMod->pMapHead)
        rtDbgAsModuleUnlinkMod(pDbgAs, pMod);
    return rc;
}




/**
 * Links a module into the address space at the give address.
 *
 * The size of the mapping is determined using RTDbgModImageSize().
 *
 * @returns IPRT status code.
 * @retval  VERR_OUT_OF_RANGE if the specified address will put the module
 *          outside the address space.
 * @retval  VERR_ADDRESS_CONFLICT if the mapping clashes with existing mappings.
 *
 * @param   hDbgAs          The address space handle.
 * @param   hDbgMod         The module handle of the module to be linked in.
 * @param   ImageAddr       The address to link the module at.
 */
RTDECL(int) RTDbgAsModuleLink(RTDBGAS hDbgAs, RTDBGMOD hDbgMod, RTUINTPTR ImageAddr)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    const char *pszName = RTDbgModName(hDbgMod);
    if (!pszName)
        return VERR_INVALID_HANDLE;
    RTUINTPTR cb = RTDbgModImageSize(hDbgMod);
    if (!cb)
        return VERR_OUT_OF_RANGE;
    if (    ImageAddr           < pDbgAs->FirstAddr
        ||  ImageAddr           > pDbgAs->LastAddr
        ||  ImageAddr + cb - 1  < pDbgAs->FirstAddr
        ||  ImageAddr + cb - 1  > pDbgAs->LastAddr
        ||  ImageAddr + cb - 1  < ImageAddr)
        return VERR_OUT_OF_RANGE;

    /*
     * Invoke worker common with RTDbgAsModuleLinkSeg.
     */
    return rtDbgAsModuleLinkCommon(pDbgAs, hDbgMod, NIL_RTDBGSEGIDX, ImageAddr, cb, pszName);
}


/**
 * Links a segment into the address space at the give address.
 *
 * The size of the mapping is determined using RTDbgModSegmentSize().
 *
 * @returns IPRT status code.
 * @retval  VERR_OUT_OF_RANGE if the specified address will put the module
 *          outside the address space.
 * @retval  VERR_ADDRESS_CONFLICT if the mapping clashes with existing mappings.
 *
 * @param   hDbgAs          The address space handle.
 * @param   hDbgMod         The module handle.
 * @param   iSeg            The segment number (0-based) of the segment to be
 *                          linked in.
 * @param   SegAddr         The address to link the segment at.
 */
RTDECL(int) RTDbgAsModuleLinkSeg(RTDBGAS hDbgAs, RTDBGMOD hDbgMod, RTDBGSEGIDX iSeg, RTUINTPTR SegAddr)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    const char *pszName = RTDbgModName(hDbgMod);
    if (!pszName)
        return VERR_INVALID_HANDLE;
    RTUINTPTR cb = RTDbgModSegmentSize(hDbgMod, iSeg);
    if (!cb)
        return VERR_OUT_OF_RANGE;
    if (    SegAddr           < pDbgAs->FirstAddr
        ||  SegAddr           > pDbgAs->LastAddr
        ||  SegAddr + cb - 1  < pDbgAs->FirstAddr
        ||  SegAddr + cb - 1  > pDbgAs->LastAddr
        ||  SegAddr + cb - 1  < SegAddr)
        return VERR_OUT_OF_RANGE;

    /*
     * Invoke worker common with RTDbgAsModuleLinkSeg.
     */
    return rtDbgAsModuleLinkCommon(pDbgAs, hDbgMod, iSeg, SegAddr, cb, pszName);
}


/**
 * Worker for RTDbgAsModuleUnlink, RTDbgAsModuleUnlinkByAddr and rtDbgAsModuleLinkCommon.
 *
 * @param   pDbgAs          Pointer to the address space instance data.
 * @param   pMod            The module to unlink.
 */
static void rtDbgAsModuleUnlinkMod(PRTDBGASINT pDbgAs, PRTDBGASMOD pMod)
{
    Assert(!pMod->pMapHead);

    /*
     * Unlink it from the name.
     */
    PRTDBGASNAME pName = (PRTDBGASNAME)RTStrSpaceGet(&pDbgAs->NameSpace, RTDbgModName((RTDBGMOD)pMod->Core.Key));
    AssertReturnVoid(pName);

    if (pName->pHead == pMod)
        pName->pHead = pMod->pNextName;
    else
        for (PRTDBGASMOD pCur = pName->pHead; pCur; pCur = pCur->pNextName)
            if (pCur->pNextName == pMod)
            {
                pCur->pNextName = pMod->pNextName;
                break;
            }
    pMod->pNextName = NULL;

    /*
     * Free the name if this was the last reference to it.
     */
    if (!pName->pHead)
    {
        pName = (PRTDBGASNAME)RTStrSpaceRemove(&pDbgAs->NameSpace, pName->StrCore.pszString);
        Assert(pName);
        RTMemFree(pName);
    }

    /*
     * Remove it from the module handle tree.
     */
    PAVLPVNODECORE pNode = RTAvlPVRemove(&pDbgAs->ModTree, pMod->Core.Key);
    Assert(pNode == &pMod->Core);

    /*
     * Remove it from the module table by replacing it by the last entry.
     */
    uint32_t iMod = pMod - &pDbgAs->paModules[0];
    Assert(iMod < pDbgAs->cModules);
    pDbgAs->cModules--;
    if (iMod <= pDbgAs->cModules)
        pDbgAs->paModules[iMod] = pDbgAs->paModules[pDbgAs->cModules];
}


/**
 * Worker for RTDbgAsModuleUnlink and RTDbgAsModuleUnlinkByAddr.
 *
 * @param   pDbgAs          Pointer to the address space instance data.
 * @param   pMap            The map to unlink and free.
 */
static void rtDbgAsModuleUnlinkMap(PRTDBGASINT pDbgAs, PRTDBGASMAP pMap)
{
    /* remove from the tree */
    PAVLRUINTPTRNODECORE pNode = RTAvlrUIntPtrRemove(&pDbgAs->MapTree, pMap->Core.Key);
    Assert(pNode);

    /* unlink */
    PRTDBGASMOD pMod = pMap->pMod;
    if (pMod->pMapHead)
        pMod->pMapHead = pMap->pNext;
    else
    {
        bool        fFound = false;
        for (PRTDBGASMAP pCur = pMod->pMapHead; pCur; pCur = pCur->pNext)
            if (pCur->pNext == pMap)
            {
                pCur->pNext = pMap->pNext;
                fFound = true;
                break;
            }
        Assert(fFound);
    }

    /* free it */
    pMap->Core.Key = pMap->Core.KeyLast = 0;
    pMap->pNext = NULL;
    pMap->pMod = NULL;
    RTMemFree(pMap);
}


/**
 * Unlinks all the mappings of a module from the address space.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if the module wasn't found.
 *
 * @param   hDbgAs          The address space handle.
 * @param   hDbgMod         The module handle of the module to be unlinked.
 */
RTDECL(int) RTDbgAsModuleUnlink(RTDBGAS hDbgAs, RTDBGMOD hDbgMod)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    if (hDbgMod == NIL_RTDBGMOD)
        return VINF_SUCCESS;

    PRTDBGASMOD pMod = (PRTDBGASMOD)RTAvlPVGet(&pDbgAs->ModTree, hDbgMod);
    if (!pMod)
        return VERR_NOT_FOUND;

    /*
     * Unmap all but
     */
    while (pMod->pMapHead)
        rtDbgAsModuleUnlinkMap(pDbgAs, pMod->pMapHead);
    rtDbgAsModuleUnlinkMod(pDbgAs, pMod);
    return VINF_SUCCESS;
}


/**
 * Unlinks the mapping at the specified address.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if no module or segment is mapped at that address.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address within the mapping to be unlinked.
 */
RTDECL(int) RTDbgAsModuleUnlinkByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Unlink it from the address space.
     * Unlink the module as well if it's the last mapping it has.
     */
    PRTDBGASMOD pMod = pMap->pMod;
    rtDbgAsModuleUnlinkMap(pDbgAs, pMap);
    if (!pMod->pMapHead)
        rtDbgAsModuleUnlinkMod(pDbgAs, pMod);
    return VINF_SUCCESS;
}


/**
 * Get a the handle of a module in the address space by is index.
 *
 * @returns A retained handle to the specified module. The caller must release
 *          the returned reference.
 *          NIL_RTDBGMOD if invalid index or handle.
 *
 * @param   hDbgAs          The address space handle.
 * @param   iModule         The index of the module to get.
 *
 * @remarks The module indexes may change after calls to RTDbgAsModuleLink,
 *          RTDbgAsModuleLinkSeg, RTDbgAsModuleUnlink and
 *          RTDbgAsModuleUnlinkByAddr.
 */
RTDECL(RTDBGMOD) RTDbgAsModuleByIndex(RTDBGAS hDbgAs, uint32_t iModule)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, NIL_RTDBGMOD);
    if (iModule >= pDbgAs->cModules)
        return NIL_RTDBGMOD;

    /*
     * Get, retain and return it.
     */
    RTDBGMOD hMod = (RTDBGMOD)pDbgAs->paModules[iModule].Core.Key;
    RTDbgModRetain(hMod);
    return hMod;
}


/**
 * Queries mapping module information by handle.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if no mapping was found at the specified address.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            Address within the mapping of the module or segment.
 * @param   phMod           Where to the return the retained module handle.
 *                          Optional.
 * @param   pAddr           Where to return the base address of the mapping.
 *                          Optional.
 * @param   piSeg           Where to return the segment index. This is set to
 *                          NIL if the entire module is mapped as a single
 *                          mapping. Optional.
 */
RTDECL(int) RTDbgAsModuleByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTDBGMOD phMod, PRTUINTPTR pAddr, PRTDBGSEGIDX piSeg)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Set up the return values.
     */
    if (phMod)
    {
        RTDBGMOD hMod = (RTDBGMOD)pMap->pMod->Core.Key;
        RTDbgModRetain(hMod);
        *phMod = hMod;
    }
    if (pAddr)
        *pAddr = pMap->Core.Key;
    if (piSeg)
        *piSeg = pMap->iSeg;
    return VINF_SUCCESS;
}


/**
 * Queries mapping module information by name.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if no mapping was found at the specified address.
 * @retval  VERR_OUT_OF_RANGE if the name index was out of range.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszName         The module name.
 * @param   iName           There can be more than one module by the same name
 *                          in an address space. This argument indicates which
 *                          is ment. (0 based)
 * @param   phMod           Where to the return the retained module handle.
 */
RTDECL(int) RTDbgAsModuleByName(RTDBGAS hDbgAs, const char *pszName, uint32_t iName, PRTDBGMOD phMod)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    AssertPtrReturn(phMod, VERR_INVALID_POINTER);
    PRTDBGASNAME pName = (PRTDBGASNAME)RTStrSpaceGet(&pDbgAs->NameSpace, pszName);
    if (!pName)
        return VERR_NOT_FOUND;

    PRTDBGASMOD pMod = pName->pHead;
    while (iName-- > 0)
    {
        pMod = pMod->pNextName;
        if (!pMod)
            return VERR_OUT_OF_RANGE;
    }

    /*
     * Get, retain and return it.
     */
    RTDBGMOD hMod = (RTDBGMOD)pMod->Core.Key;
    RTDbgModRetain(hMod);
    *phMod = hMod;
    return VINF_SUCCESS;
}


/**
 * Adds a symbol to a module in the address space.
 *
 * @returns IPRT status code. See RTDbgModSymbolAdd for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if no module was found at the specified address.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszSymbol       The symbol name.
 * @param   Addr            The address of the symbol.
 * @param   cb              The size of the symbol.
 */
RTDECL(int) RTDbgAsSymbolAdd(RTDBGAS hDbgAs, const char *pszSymbol, RTUINTPTR Addr, uint32_t cb)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Forward the call.
     */
    RTDBGMOD hMod = (RTDBGMOD)pMap->pMod->Core.Key;
    return RTDbgModSymbolAdd(hMod, pszSymbol, pMap->iSeg, Addr - pMap->Core.Key, cb);
}


/**
 * Query a symbol by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddr for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the symbol
 *                          and address. Optional.
 * @param   pSymbol         Where to return the symbol info.
 */
RTDECL(int) RTDbgAsSymbolByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGSYMBOL pSymbol)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Forward the call.
     */
    RTDBGMOD hMod = (RTDBGMOD)pMap->pMod->Core.Key;
    return RTDbgModSymbolByAddr(hMod, pMap->iSeg, Addr - pMap->Core.Key, poffDisp, pSymbol);
}


/**
 * Query a symbol by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddrA for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the symbol
 *                          and address. Optional.
 * @param   ppSymbol        Where to return the pointer to the allocated
 *                          symbol info. Always set. Free with RTDbgSymbolFree.
 */
RTDECL(int) RTDbgAsSymbolByAddrA(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGSYMBOL *ppSymbol)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Forward the call.
     */
    RTDBGMOD hMod = (RTDBGMOD)pMap->pMod->Core.Key;
    return RTDbgModSymbolByAddrA(hMod, pMap->iSeg, Addr - pMap->Core.Key, poffDisp, ppSymbol);
}


/**
 * Query a symbol by name.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if not found.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszSymbol       The symbol name.
 * @param   pSymbol         Where to return the symbol info.
 */
RTDECL(int) RTDbgAsSymbolByName(RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL pSymbol)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszSymbol, VERR_INVALID_POINTER);
    AssertPtrReturn(pSymbol, VERR_INVALID_POINTER);

    /*
     * Iterate the modules, looking for the symbol.
     */
    for (uint32_t i = 0; i < pDbgAs->cModules; i++)
    {
        RTDBGMOD hMod = (RTDBGMOD)pDbgAs->paModules[i].Core.Key;
        int rc = RTDbgModSymbolByName(hMod, pszSymbol, pSymbol);
        if (RT_SUCCESS(rc))
            return rc;
    }
    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Query a symbol by name.
 *
 * @returns IPRT status code.
 * @retval  VERR_SYMBOL_NOT_FOUND if not found.
 *
 * @param   hDbgAs          The address space handle.
 * @param   pszSymbol       The symbol name.
 * @param   ppSymbol        Where to return the pointer to the allocated
 *                          symbol info. Always set. Free with RTDbgSymbolFree.
 */
RTDECL(int) RTDbgAsSymbolByNameA(RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL *ppSymbol)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(ppSymbol, VERR_INVALID_POINTER);
    *ppSymbol = NULL;
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszSymbol, VERR_INVALID_POINTER);

    /*
     * Iterate the modules, looking for the symbol.
     */
    for (uint32_t i = 0; i < pDbgAs->cModules; i++)
    {
        RTDBGMOD hMod = (RTDBGMOD)pDbgAs->paModules[i].Core.Key;
        int rc = RTDbgModSymbolByNameA(hMod, pszSymbol, ppSymbol);
        if (RT_SUCCESS(rc))
            return rc;
    }
    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Query a line number by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddrA for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the line
 *                          number and address.
 * @param   pLine           Where to return the line number information.
 */
RTDECL(int) RTDbgAsLineByAddr(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGLINE pLine)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Forward the call.
     */
    RTDBGMOD hMod = (RTDBGMOD)pMap->pMod->Core.Key;
    return RTDbgModLineByAddr(hMod, pMap->iSeg, Addr - pMap->Core.Key, poffDisp, pLine);
}


/**
 * Query a line number by address.
 *
 * @returns IPRT status code. See RTDbgModSymbolAddrA for more specific ones.
 * @retval  VERR_INVALID_HANDLE if hDbgAs is invalid.
 * @retval  VERR_NOT_FOUND if the address couldn't be mapped to a module.
 *
 * @param   hDbgAs          The address space handle.
 * @param   Addr            The address which closest symbol is requested.
 * @param   poffDisp        Where to return the distance between the line
 *                          number and address.
 * @param   ppLine          Where to return the pointer to the allocated line
 *                          number info. Always set. Free with RTDbgLineFree.
 */
RTDECL(int) RTDbgAsLineByAddrA(RTDBGAS hDbgAs, RTUINTPTR Addr, PRTINTPTR poffDisp, PRTDBGLINE *ppLine)
{
    /*
     * Validate input.
     */
    PRTDBGASINT pDbgAs = hDbgAs;
    RTDBGAS_VALID_RETURN_RC(pDbgAs, VERR_INVALID_HANDLE);
    PRTDBGASMAP pMap = (PRTDBGASMAP)RTAvlrUIntPtrRangeGet(&pDbgAs->MapTree, Addr);
    if (pMap)
        return VERR_NOT_FOUND;

    /*
     * Forward the call.
     */
    RTDBGMOD hMod = (RTDBGMOD)pMap->pMod->Core.Key;
    return RTDbgModLineByAddrA(hMod, pMap->iSeg, Addr - pMap->Core.Key, poffDisp, ppLine);
}

