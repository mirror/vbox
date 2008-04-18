/* $Id$ */
/** @file
 * innotek Portable Runtime - Unique String Spaces.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/string.h>
#include <iprt/assert.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/*
 * AVL configuration.
 */
#define KAVL_FN(a)                  rtstrspace##a
#define KAVL_MAX_STACK              27  /* Up to 2^24 nodes. */
#define KAVL_EQUAL_ALLOWED          1
#define KAVLNODECORE                RTSTRSPACECORE
#define PKAVLNODECORE               PRTSTRSPACECORE
#define PPKAVLNODECORE              PPRTSTRSPACECORE
#define KAVLKEY                     uint32_t
#define PKAVLKEY                    uint32_t *

#define PKAVLCALLBACK               PFNRTSTRSPACECALLBACK

/*
 * AVL Compare macros
 */
#define KAVL_G(key1, key2)          (key1 >  key2)
#define KAVL_E(key1, key2)          (key1 == key2)
#define KAVL_NE(key1, key2)         (key1 != key2)


/*
 * Include the code.
 */
#define SSToDS(ptr) ptr
#define KMAX RT_MAX
#define kASSERT Assert
#include "../table/avl_Base.cpp.h"
#include "../table/avl_Get.cpp.h"
#include "../table/avl_DoWithAll.cpp.h"
#include "../table/avl_Destroy.cpp.h"



/* sdbm:
   This algorithm was created for sdbm (a public-domain reimplementation of
   ndbm) database library. it was found to do well in scrambling bits,
   causing better distribution of the keys and fewer splits. it also happens
   to be a good general hashing function with good distribution. the actual
   function is hash(i) = hash(i - 1) * 65599 + str[i]; what is included below
   is the faster version used in gawk. [there is even a faster, duff-device
   version] the magic constant 65599 was picked out of thin air while
   experimenting with different constants, and turns out to be a prime.
   this is one of the algorithms used in berkeley db (see sleepycat) and
   elsewhere. */
inline uint32_t sdbm(const char *str, size_t *pcch)
{
    uint8_t *pu8 = (uint8_t *)str;
    uint32_t hash = 0;
    int c;

    while ((c = *pu8++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    *pcch = (uintptr_t)pu8 - (uintptr_t)str;
    return hash;
}


/**
 * Inserts a string into a unique string space.
 *
 * @returns true on success.
 * @returns false if the string collieded with an existing string.
 * @param   pStrSpace       The space to insert it into.
 * @param   pStr            The string node.
 */
RTDECL(bool) RTStrSpaceInsert(PRTSTRSPACE pStrSpace, PRTSTRSPACECORE pStr)
{
    pStr->Key = sdbm(pStr->pszString, &pStr->cchString);
    PRTSTRSPACECORE pMatch = KAVL_FN(Get)(pStrSpace, pStr->Key);
    if (!pMatch)
        return KAVL_FN(Insert)(pStrSpace, pStr);

    /* Check for clashes. */
    for (PRTSTRSPACECORE pCur = pMatch; pCur; pCur = pCur->pList)
        if (    pCur->cchString == pStr->cchString
            &&  !memcmp(pCur->pszString, pStr->pszString, pStr->cchString))
            return false;
    pStr->pList = pMatch->pList;
    pMatch->pList = pStr;
    return false;
}


/**
 * Removes a string from a unique string space.
 *
 * @returns Pointer to the removed string node.
 * @returns NULL if the string was not found in the string space.
 * @param   pStrSpace       The space to insert it into.
 * @param   pszString       The string to remove.
 */
RTDECL(PRTSTRSPACECORE) RTStrSpaceRemove(PRTSTRSPACE pStrSpace, const char *pszString)
{
    size_t  cchString;
    KAVLKEY Key = sdbm(pszString, &cchString);
    PRTSTRSPACECORE pCur = KAVL_FN(Get)(pStrSpace, Key);
    if (!pCur)
        return NULL;

    /* find the right one. */
    PRTSTRSPACECORE pPrev = NULL;
    for (; pCur; pPrev = pCur, pCur = pCur->pList)
        if (    pCur->cchString == cchString
            && !memcmp(pCur->pszString, pszString, cchString))
            break;
    if (pCur)
    {
        if (pPrev)
            /* simple, it's in the linked list. */
            pPrev->pList = pCur->pList;
        else
        {
            /* in the tree. remove and reinsert list head. */
            PRTSTRSPACECORE pInsert = pCur->pList;
            pCur->pList = NULL;
            pCur = KAVL_FN(Remove)(pStrSpace, Key);
            Assert(pCur);
            if (pInsert)
            {
                PRTSTRSPACECORE pList = pInsert->pList;
                bool fRc = KAVL_FN(Insert)(pStrSpace, pInsert);
                Assert(fRc); NOREF(fRc);
                pInsert->pList = pList;
            }
        }
    }

    return pCur;
}


/**
 * Gets a string from a unique string space.
 *
 * @returns Pointer to the string node.
 * @returns NULL if the string was not found in the string space.
 * @param   pStrSpace       The space to insert it into.
 * @param   pszString       The string to get.
 */
RTDECL(PRTSTRSPACECORE) RTStrSpaceGet(PRTSTRSPACE pStrSpace, const char *pszString)
{
    size_t  cchString;
    KAVLKEY Key = sdbm(pszString, &cchString);
    PRTSTRSPACECORE pCur = KAVL_FN(Get)(pStrSpace, Key);
    if (!pCur)
        return NULL;

    /* Linear search. */
    for (; pCur; pCur = pCur->pList)
        if (    pCur->cchString == cchString
            && !memcmp(pCur->pszString, pszString, cchString))
            return pCur;
    return NULL;
}



/**
 * Enumerates the string space.
 * The caller supplies a callback which will be called for each of
 * the string nodes.
 *
 * @returns 0 or what ever non-zero return value pfnCallback returned
 *          when aborting the destruction.
 * @param   pStrSpace       The space to insert it into.
 * @param   pfnCallback     The callback.
 * @param   pvUser          The user argument.
 */
RTDECL(int) RTStrSpaceEnumerate(PRTSTRSPACE pStrSpace, PFNRTSTRSPACECALLBACK pfnCallback, void *pvUser)
{
    return KAVL_FN(DoWithAll)(pStrSpace, true, pfnCallback, pvUser);
}


/**
 * Destroys the string space.
 * The caller supplies a callback which will be called for each of
 * the string nodes in for freeing their memory and other resources.
 *
 * @returns 0 or what ever non-zero return value pfnCallback returned
 *          when aborting the destruction.
 * @param   pStrSpace       The space to insert it into.
 * @param   pfnCallback     The callback.
 * @param   pvUser          The user argument.
 */
RTDECL(int) RTStrSpaceDestroy(PRTSTRSPACE pStrSpace, PFNRTSTRSPACECALLBACK pfnCallback, void *pvUser)
{
    return KAVL_FN(Destroy)(pStrSpace, pfnCallback, pvUser);
}

