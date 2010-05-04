/* $Id$ */
/** @file
 * VBoxServicePropCache - Guest property cache.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServicePropCache.h"


/**
 * Initializes the property cache.
 *
 * @returns IPRT status code.
 * @param   pCache          Pointer to the cache.
 * @param   uClientId       The HGCM handle of to the guest property service.
 */
int VBoxServicePropCacheInit(PVBOXSERVICEVEPROPCACHE pCache, uint32_t uClientId)
{
    AssertPtr(pCache);
    /** @todo Prevent init the cache twice!
     *  r=bird: Use a magic, or/and abstract the whole cache by rename this function
     *  VBoxServicePropCacheCreate(). */
    RTListInit(&pCache->Node);
    pCache->uClientID = uClientId;
    return RTSemMutexCreate(&pCache->Mutex);
}


/** @todo Docs
 * @todo this looks internal to me, nobody should need to access the
 *       structures directly here. */
PVBOXSERVICEVEPROPCACHEENTRY VBoxServicePropCacheFind(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t uFlags)
{
    AssertPtr(pCache);
    AssertPtr(pszName);
    /** @todo This is a O(n) lookup, maybe improve this later to O(1) using a
     *        map.
     *  r=bird: Use a string space (RTstrSpace*). That is O(log n) in its current
     *        implementation (AVL tree). However, this is not important at the
     *        moment. */
    PVBOXSERVICEVEPROPCACHEENTRY pNodeIt, pNode = NULL;
    if (RT_SUCCESS(RTSemMutexRequest(pCache->Mutex, RT_INDEFINITE_WAIT)))
    {
        RTListForEach(&pCache->Node, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, Node)
        {
            if (strcmp(pNodeIt->pszName, pszName) == 0)
            {
                pNode = pNodeIt;
                break;
            }
        }
        RTSemMutexRelease(pCache->Mutex);
    }
    return pNode;
}


/** @todo Docs */
int VBoxServicePropCacheUpdateEntry(PVBOXSERVICEVEPROPCACHE pCache,
                                    const char *pszName, uint32_t fFlags, const char *pszValueReset)
{
    AssertPtr(pCache);
    AssertPtr(pszName);
    PVBOXSERVICEVEPROPCACHEENTRY pNode = VBoxServicePropCacheFind(pCache, pszName, 0);
    int rc;
    if (pNode != NULL)
    {
        rc = RTSemMutexRequest(pCache->Mutex, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
        {
            pNode->fFlags = fFlags;
            if (pszValueReset)
            {
                if (pszValueReset)
                    RTStrFree(pNode->pszValueReset);
                pNode->pszValueReset = RTStrDup(pszValueReset);
            }
            rc = RTSemMutexRelease(pCache->Mutex);
        }
    }
    else
        rc = VERR_NOT_FOUND;
    return rc;
}


/**
 * Updates the local guest property cache and writes it to HGCM if outdated.
 *
 * @returns VBox status code. Errors will be logged.
 *
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VBoxServicePropCacheUpdate(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValueFormat, ...)
{
    char *pszValue = NULL;
    int rc;
    if (pszValueFormat)
    {
        va_list va;
        va_start(va, pszValueFormat);
        RTStrAPrintfV(&pszValue, pszValueFormat, va);
        va_end(va);
    }
    rc = VBoxServicePropCacheUpdateEx(pCache, pszName, 0 /* Not used */, NULL /* Not used */, pszValue);
    if (pszValue)
        RTStrFree(pszValue);
    return rc;
}


/**
 * Updates the local guest property cache and writes it to HGCM if outdated.
 *
 * @returns VBox status code. Errors will be logged.
 *
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VBoxServicePropCacheUpdateEx(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags,
                                 const char *pszValueReset, const char *pszValueFormat, ...)
{
    AssertPtr(pCache);
    Assert(pCache->uClientID);
    AssertPtr(pszName);

    /*
     * Format the value first.
     */
    char *pszValue = NULL;
    if (pszValueFormat)
    {
        va_list va;
        va_start(va, pszValueFormat);
        RTStrAPrintfV(&pszValue, pszValueFormat, va);
        va_end(va);
        if (!pszValue)
            return VERR_NO_STR_MEMORY;
    }

    PVBOXSERVICEVEPROPCACHEENTRY pNode = VBoxServicePropCacheFind(pCache, pszName, 0);

    /* Lock the cache. */
    int rc = RTSemMutexRequest(pCache->Mutex, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {

        if (pNode == NULL)
        {
            pNode = (PVBOXSERVICEVEPROPCACHEENTRY)RTMemAlloc(sizeof(VBOXSERVICEVEPROPCACHEENTRY));
            if (RT_UNLIKELY(!pNode))
            {
                RTSemMutexRelease(pCache->Mutex);
                return VERR_NO_MEMORY;
            }

            pNode->pszName = RTStrDup(pszName);
            pNode->pszValue = NULL;
            pNode->fFlags = 0;
            pNode->pszValueReset = NULL;

            /*rc =*/ RTListAppend(&pCache->Node, &pNode->Node);
        }

        AssertPtr(pNode);
        if (pszValue) /* Do we have a value to check for? */
        {
            bool fUpdate = false;
            /* Always update this property, no matter what? */
            if (pNode->fFlags & VBOXSERVICEPROPCACHEFLAG_ALWAYS_UPDATE)
                fUpdate = true;
            /* Did the value change so we have to update? */
            else if (pNode->pszValue && strcmp(pNode->pszValue, pszValue) != 0)
                fUpdate = true;
            /* No value stored at the moment but we have a value now? */
            else if (pNode->pszValue == NULL)
                fUpdate = true;

            if (fUpdate)
            {
                /* Write the update. */
                rc = VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, pszValue);
                RTStrFree(pNode->pszValue);
                pNode->pszValue = RTStrDup(pszValue);
            }
            else
                /** @todo r=bird: Add a VINF_NO_CHANGE status code to iprt/err.h and use it. */
                rc = VINF_ALREADY_INITIALIZED; /* No update needed. */
        }
        else
        {
            /* No value specified. Deletion (or no action required). */
            if (pNode->pszValue) /* Did we have a value before? Then the value needs to be deleted. */
            {
                /* Delete property (but do not remove from cache) if not deleted yet. */
                RTStrFree(pNode->pszValue);
                pNode->pszValue = NULL;
                rc = VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, NULL);
            }
            else
                /** @todo r=bird: Use VINF_NO_CHANGE. */
                rc = VINF_ALREADY_INITIALIZED; /* No update needed. */
        }

        /* Update rest of the fields. */
        if (pszValueReset)
        {
            if (pNode->pszValueReset)
                RTStrFree(pNode->pszValueReset);
            pNode->pszValueReset = RTStrDup(pszValueReset);
        }
        if (fFlags)
            pNode->fFlags = fFlags;

        /* Release cache. */
        int rc2 = RTSemMutexRelease(pCache->Mutex);
        if (RT_SUCCESS(rc))
            rc2 = rc;
    }

    /* Delete temp stuff. */
    RTStrFree(pszValue);

    return rc;
}


/**
 * Reset all temporary properties and destroy the cache.
 *
 * @param   pCache          The property cache.
 */
void VBoxServicePropCacheDestroy(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtr(pCache);
    Assert(pCache->uClientID);
    PVBOXSERVICEVEPROPCACHEENTRY pNode;
    RTListForEach(&pCache->Node, pNode, VBOXSERVICEVEPROPCACHEENTRY, Node)
    {
        if ((pNode->fFlags & VBOXSERVICEPROPCACHEFLAG_TEMPORARY) == 0)
            VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, pNode->pszValueReset);

        AssertPtr(pNode->pszName);
        RTStrFree(pNode->pszName);
        RTStrFree(pNode->pszValue);
        RTStrFree(pNode->pszValueReset);
        pNode->fFlags = 0;
    }

    pNode = RTListNodeGetFirst(&pCache->Node, VBOXSERVICEVEPROPCACHEENTRY, Node);
    while (pNode)
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNext = RTListNodeGetNext(&pNode->Node, VBOXSERVICEVEPROPCACHEENTRY, Node);
        RTListNodeRemove(&pNode->Node);
        /** @todo r=bird: hrm. missing RTMemFree(pNode)? Why don't you just combine the
         *        two loops? */

        if (pNext && RTListNodeIsLast(&pCache->Node, &pNext->Node))
            break;
        pNode = pNext;
    }

    /* Destroy mutex. */
    RTSemMutexDestroy(pCache->Mutex);
}

