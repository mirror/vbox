/* $Id$ */
/** @file
 * VBoxServicePropCache - Guest property cache.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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


/** Internal functions, not for public use. */
PVBOXSERVICEVEPROPCACHEENTRY vboxServicePropCacheFindInternal(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t uFlags);
PVBOXSERVICEVEPROPCACHEENTRY vboxServicePropCacheInsertEntryInternal(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName);


/** @todo Docs */
PVBOXSERVICEVEPROPCACHEENTRY vboxServicePropCacheFindInternal(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t uFlags)
{
    AssertPtrReturn(pCache, NULL);
    AssertPtrReturn(pszName, NULL);

    /** @todo This is a O(n) lookup, maybe improve this later to O(1) using a
     *        map.
     *  r=bird: Use a string space (RTstrSpace*). That is O(log n) in its current
     *        implementation (AVL tree). However, this is not important at the
     *        moment. */
    PVBOXSERVICEVEPROPCACHEENTRY pNodeIt, pNode = NULL;
    if (RT_SUCCESS(RTCritSectEnter(&pCache->CritSect)))
    {
        RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
        {
            if (strcmp(pNodeIt->pszName, pszName) == 0)
            {
                pNode = pNodeIt;
                break;
            }
        }
        RTCritSectLeave(&pCache->CritSect);
    }
    return pNode;
}


/** @todo Docs */
PVBOXSERVICEVEPROPCACHEENTRY vboxServicePropCacheInsertEntryInternal(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName)
{
    AssertPtrReturn(pCache, NULL);
    AssertPtrReturn(pszName, NULL);

    PVBOXSERVICEVEPROPCACHEENTRY pNode = (PVBOXSERVICEVEPROPCACHEENTRY)RTMemAlloc(sizeof(VBOXSERVICEVEPROPCACHEENTRY));
    if (pNode)
    {
        pNode->pszName = RTStrDup(pszName);
        if (!pNode->pszName)
        {
            RTMemFree(pNode);
            return NULL;
        }
        pNode->pszValue = NULL;
        pNode->fFlags = 0;
        pNode->pszValueReset = NULL;

        int rc = RTCritSectEnter(&pCache->CritSect);
        if (RT_SUCCESS(rc))
        {
            /*rc =*/ RTListAppend(&pCache->NodeHead, &pNode->NodeSucc);
            rc = RTCritSectLeave(&pCache->CritSect);
        }
    }
    return pNode;
}


/** @todo Docs */
int vboxServicePropCacheWritePropF(uint32_t u32ClientId, const char *pszName, uint32_t fFlags, const char *pszValueFormat, ...)
{
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    int rc;
    if (pszValueFormat != NULL)
    {
        va_list va;
        va_start(va, pszValueFormat);

        char *pszValue;
        if (RTStrAPrintfV(&pszValue, pszValueFormat, va) >= 0)
        {
            if (fFlags & VBOXSERVICEPROPCACHEFLAG_TRANSIENT)
            {
                /*
                 * Because a value can be temporary we have to make sure it also
                 * gets deleted when the property cache did not have the chance to
                 * gracefully clean it up (due to a hard VM reset etc), so set this
                 * guest property using the TRANSRESET flag..
                 */
                rc = VbglR3GuestPropWrite(u32ClientId, pszName, pszValue, "TRANSRESET");
                if (rc == VERR_PARSE_ERROR)
                {
                    /* Host does not support the "TRANSRESET" flag, so only
                     * use the "TRANSIENT" flag -- better than nothing :-). */
                    rc = VbglR3GuestPropWrite(u32ClientId, pszName, pszValue, "TRANSIENT");
                    /** @todo r=bird: Remember that the host doesn't support
                     * this. */
                }
            }
            else
                rc = VbglR3GuestPropWriteValue(u32ClientId, pszName, pszValue /* No transient flags set */);
            RTStrFree(pszValue);
        }
        else
            rc = VERR_NO_MEMORY;
        va_end(va);
    }
    else
    {
        rc = VbglR3GuestPropWriteValue(u32ClientId, pszName, NULL);
    }
    return rc;
}


/**
 * Creates a property cache.
 *
 * @returns IPRT status code.
 * @param   pCache          Pointer to the cache.
 * @param   uClientId       The HGCM handle of to the guest property service.
 */
int VBoxServicePropCacheCreate(PVBOXSERVICEVEPROPCACHE pCache, uint32_t uClientId)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    /** @todo Prevent init the cache twice!
     *  r=bird: Use a magic. */
    RTListInit(&pCache->NodeHead);
    pCache->uClientID = uClientId;
    return RTCritSectInit(&pCache->CritSect);
}


/**
 * Updates a cache entry without submitting any changes to the host.
 * This is handy for defining default values/flags.
 *
 * @returns VBox status code.
 *
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   fFlags          The property flags to set.
 * @param   pszValueReset   The property reset value.
 */
int VBoxServicePropCacheUpdateEntry(PVBOXSERVICEVEPROPCACHE pCache,
                                    const char *pszName, uint32_t fFlags, const char *pszValueReset)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    PVBOXSERVICEVEPROPCACHEENTRY pNode = vboxServicePropCacheFindInternal(pCache, pszName, 0);
    if (pNode == NULL)
        pNode = vboxServicePropCacheInsertEntryInternal(pCache, pszName);

    int rc;
    if (pNode != NULL)
    {
        rc = RTCritSectEnter(&pCache->CritSect);
        if (RT_SUCCESS(rc))
        {
            pNode->fFlags = fFlags;
            if (pszValueReset)
            {
                if (pNode->pszValueReset)
                    RTStrFree(pNode->pszValueReset);
                pNode->pszValueReset = RTStrDup(pszValueReset);
                AssertPtr(pNode->pszValueReset);
            }
            rc = RTCritSectLeave(&pCache->CritSect);
        }
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Updates the local guest property cache and writes it to HGCM if outdated.
 *
 * @returns VBox status code.
 *
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VBoxServicePropCacheUpdate(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValueFormat, ...)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    Assert(pCache->uClientID);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

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

    PVBOXSERVICEVEPROPCACHEENTRY pNode = vboxServicePropCacheFindInternal(pCache, pszName, 0);

    /* Lock the cache. */
    int rc = RTCritSectEnter(&pCache->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pNode == NULL)
            pNode = vboxServicePropCacheInsertEntryInternal(pCache, pszName);

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
                rc = vboxServicePropCacheWritePropF(pCache->uClientID, pNode->pszName, pNode->fFlags, pszValue);
                VBoxServiceVerbose(4, "PropCache %p: Written \"%s\"=\"%s\" (flags: %x), rc=%Rrc\n",
                                   pCache, pNode->pszName, pszValue, pNode->fFlags, rc);
                RTStrFree(pNode->pszValue);
                pNode->pszValue = RTStrDup(pszValue);
                if (!pNode->pszValue)
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VINF_NO_CHANGE; /* No update needed. */
        }
        else
        {
            /* No value specified. Deletion (or no action required). */
            if (pNode->pszValue) /* Did we have a value before? Then the value needs to be deleted. */
            {
                /* Delete property (but do not remove from cache) if not deleted yet. */
                RTStrFree(pNode->pszValue);
                pNode->pszValue = NULL;
                rc = vboxServicePropCacheWritePropF(pCache->uClientID, pNode->pszName,
                                                    0, /* Flags */ NULL /* Value */);
                VBoxServiceVerbose(4, "PropCache %p: Deleted \"%s\"=\"%s\" (flags: %x), rc=%Rrc\n",
                                   pCache, pNode->pszName, pNode->pszValue, pNode->fFlags, rc);
            }
            else
                rc = VINF_NO_CHANGE; /* No update needed. */
        }

        /* Release cache. */
        RTCritSectLeave(&pCache->CritSect);
    }

    /* Delete temp stuff. */
    RTStrFree(pszValue);
    return rc;
}


/**
 * Updates all cache values which are matching the specified path.
 *
 * @returns VBox status code.
 *
 * @param   pCache          The property cache.
 * @param   pszValue        The value to set.  A NULL will delete the value.
 * @param   fFlags          Flags to set.
 * @param   pszPathFormat   The path format string.  May not be null and has
 *                          to be an absolute path.
 * @param   ...             Format arguments.
 */
int VBoxServicePropCacheUpdateByPath(PVBOXSERVICEVEPROPCACHE pCache, const char *pszValue, uint32_t fFlags, const char *pszPathFormat, ...)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPathFormat, VERR_INVALID_POINTER);

    int rc = VERR_NOT_FOUND;
    PVBOXSERVICEVEPROPCACHEENTRY pNodeIt = NULL;
    if (RT_SUCCESS(RTCritSectEnter(&pCache->CritSect)))
    {
        /*
         * Format the value first.
         */
        char *pszPath = NULL;
        va_list va;
        va_start(va, pszPathFormat);
        RTStrAPrintfV(&pszPath, pszPathFormat, va);
        va_end(va);
        if (!pszPath)
        {
            rc = VERR_NO_STR_MEMORY;
        }
        else
        {
            /* Iterate through all nodes and compare their paths. */
            RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
            {
                if (RTStrStr(pNodeIt->pszName, pszPath) == pNodeIt->pszName)
                {
                    /** @todo Use some internal function to update the node directly, this is slow atm. */
                    rc = VBoxServicePropCacheUpdate(pCache, pNodeIt->pszName, pszValue);
                }
                if (RT_FAILURE(rc))
                    break;
            }
            RTStrFree(pszPath);
        }
        RTCritSectLeave(&pCache->CritSect);
    }
    return rc;
}


/**
 * Flushes the cache by writing every item regardless of its state.
 *
 * @param   pCache          The property cache.
 */
int VBoxServicePropCacheFlush(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    PVBOXSERVICEVEPROPCACHEENTRY pNodeIt = NULL;
    if (RT_SUCCESS(RTCritSectEnter(&pCache->CritSect)))
    {
        RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
        {
            rc = vboxServicePropCacheWritePropF(pCache->uClientID, pNodeIt->pszName,
                                                pNodeIt->fFlags, pNodeIt->pszValue);
            if (RT_FAILURE(rc))
                break;
        }
        RTCritSectLeave(&pCache->CritSect);
    }
    return rc;
}


/**
 * Reset all temporary properties and destroy the cache.
 *
 * @param   pCache          The property cache.
 */
void VBoxServicePropCacheDestroy(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtrReturnVoid(pCache);
    Assert(pCache->uClientID);

    /* Lock the cache. */
    int rc = RTCritSectEnter(&pCache->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNode = RTListGetFirst(&pCache->NodeHead, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc);
        while (pNode)
        {
            PVBOXSERVICEVEPROPCACHEENTRY pNext = RTListNodeIsLast(&pCache->NodeHead, &pNode->NodeSucc)
                                                                  ? NULL :
                                                                    RTListNodeGetNext(&pNode->NodeSucc,
                                                                                      VBOXSERVICEVEPROPCACHEENTRY, NodeSucc);
            RTListNodeRemove(&pNode->NodeSucc);

            if (pNode->fFlags & VBOXSERVICEPROPCACHEFLAG_TEMPORARY)
            {
                rc = vboxServicePropCacheWritePropF(pCache->uClientID, pNode->pszName,
                                                    pNode->fFlags, pNode->pszValueReset);
            }

            AssertPtr(pNode->pszName);
            RTStrFree(pNode->pszName);
            RTStrFree(pNode->pszValue);
            RTStrFree(pNode->pszValueReset);
            pNode->fFlags = 0;

            RTMemFree(pNode);

            pNode = pNext;
        }
        RTCritSectLeave(&pCache->CritSect);
    }

    /* Destroy critical section. */
    RTCritSectDelete(&pCache->CritSect);
}

