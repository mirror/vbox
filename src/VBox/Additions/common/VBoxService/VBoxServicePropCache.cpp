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

#ifdef VBOX_WITH_GUEST_PROPS

/** @todo Docs */
int VBoxServicePropCacheInit(PVBOXSERVICEVEPROPCACHE pCache, uint32_t u32ClientId)
{
    AssertPtr(pCache);
    RTListInit(&pCache->Node);
    pCache->uClientID = u32ClientId;
    return VINF_SUCCESS;
}


/** @todo Docs */
PVBOXSERVICEVEPROPCACHEENTRY VBoxServicePropCacheFind(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t uFlags)
{
    AssertPtr(pCache);
    AssertPtr(pszName);
    /* This is a O(n) lookup, maybe improve this later to O(1) using a map. */
    PVBOXSERVICEVEPROPCACHEENTRY pNode;
    RTListForEach(&pCache->Node, pNode, VBOXSERVICEVEPROPCACHEENTRY, Node)
    {
        if (strcmp(pNode->pszName, pszName) == 0)
            return pNode;
    }
    return NULL;
}


/** @todo Docs */
int VBoxServicePropCacheUpdateEntry(PVBOXSERVICEVEPROPCACHE pCache, 
                                    const char *pszName, uint32_t u32Flags, const char *pszValueReset)
{
    AssertPtr(pCache);
    AssertPtr(pszName);
    PVBOXSERVICEVEPROPCACHEENTRY pNode = VBoxServicePropCacheFind(pCache, pszName, 0);
    if (pNode != NULL)
    {
        pNode->uFlags = u32Flags;
        if (pszValueReset)
        {
            if (pszValueReset)
                RTStrFree(pNode->pszValueReset);
            pNode->pszValueReset = RTStrDup(pszValueReset);
        }
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}


/**
 * Updates the local guest property cache and writes it to HGCM if outdated.
 *
 * @returns VBox status code. Errors will be logged.
 *
 * @param   u32ClientId     The HGCM client ID for the guest property session.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VBoxServicePropCacheUpdate(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValueFormat, ...)
{
    va_list va;
    va_start(va, pszValueFormat);
    char *pszValue;
    int rc = RTStrAPrintfV(&pszValue, pszValueFormat, va);
    if (RT_SUCCESS(rc))
    {
        rc = VBoxServicePropCacheUpdateEx(pCache, pszName, 0 /* Not used */, NULL /* Not used */, pszValue);
        RTStrFree(pszValue);
    }
    va_end(va);
    return rc;
}


/**
 * Updates the local guest property cache and writes it to HGCM if outdated.
 *
 * @returns VBox status code. Errors will be logged.
 *
 * @param   u32ClientId     The HGCM client ID for the guest property session.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VBoxServicePropCacheUpdateEx(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t u32Flags, const char *pszValueReset, const char *pszValueFormat, ...)
{
    AssertPtr(pCache);
    Assert(pCache->uClientID);
    AssertPtr(pszName);
    AssertPtr(pszValueFormat);

    int rc = VINF_SUCCESS;
    char *pszValue = NULL;
    if (pszValueFormat)
    {
        va_list va;
        va_start(va, pszValueFormat);
        rc = RTStrAPrintfV(&pszValue, pszValueFormat, va);
        va_end(va);
    }

    PVBOXSERVICEVEPROPCACHEENTRY pNode = VBoxServicePropCacheFind(pCache, pszName, 0);
    if (pNode != NULL)
    {            
        if (pszValue)
        {
            bool fUpdate = false;
            if (pNode->uFlags & VBOXSERVICEPROPCACHEFLAG_ALWAYS_UPDATE)
                fUpdate = true;
            else if (pNode->pszValue && strcmp(pNode->pszValue, pszValue) != 0)
                fUpdate = true;

            if (fUpdate)
            {
                /* Write the update. */
                rc = VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, pszValue);
                if (pNode->pszValue)
                    RTStrFree(pNode->pszValue);
                pNode->pszValue = RTStrDup(pszValue);
                if (pszValueReset)
                {
                    if (pNode->pszValueReset)
                        RTStrFree(pNode->pszValueReset);
                    pNode->pszValueReset = RTStrDup(pszValueReset);
                }
                if (u32Flags)
                    pNode->uFlags = u32Flags;
            }
            else
                rc = VINF_ALREADY_INITIALIZED; /* No update needed. */
        }
        else if (pNode->pszValue)
        {
            /* Delete property (but do not remove from cache) if not deleted yet. */
            VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, NULL);
            RTStrFree(pNode->pszValue);
            pNode->pszValue = NULL;
        }
    }
    else /* Node not found; add it and update HGCM service. */
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNewNode = (PVBOXSERVICEVEPROPCACHEENTRY)RTMemAlloc(sizeof(VBOXSERVICEVEPROPCACHEENTRY));
        pNewNode->pszName = RTStrDup(pszName);
        if (pszValue)
            pNewNode->pszValue = RTStrDup(pszValue);
        else
            pNewNode->pszValue = NULL;
         if (pszValueReset)
            pNewNode->pszValueReset = RTStrDup(pszValueReset);
        else
            pNewNode->pszValueReset = NULL;
        pNewNode->uFlags = u32Flags;
        /*rc =*/ RTListAppend(&pCache->Node, &pNewNode->Node);
        if (RT_SUCCESS(rc))
            rc = VERR_NOT_FOUND;
    }

    if (pszValue)
        RTStrFree(pszValue);   
    return rc;
}


/** @todo Docs */
void VBoxServicePropCacheDestroy(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtr(pCache);
    Assert(pCache->uClientID);
    PVBOXSERVICEVEPROPCACHEENTRY pNode;
    RTListForEach(&pCache->Node, pNode, VBOXSERVICEVEPROPCACHEENTRY, Node)
    {
        if (pNode->uFlags & VBOXSERVICEPROPCACHEFLAG_TEMPORARY == 0)
        {
            if (pNode->pszValueReset) /* Write reset value? */
            {
                /* rc = */VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, pNode->pszValueReset);
            }
            else /* Delete value. */
                /* rc = */VBoxServiceWritePropF(pCache->uClientID, pNode->pszName, NULL);
        }

        AssertPtr(pNode->pszName);
        RTStrFree(pNode->pszName);
        if (pNode->pszValue)
            RTStrFree(pNode->pszValue);
        if (pNode->pszValueReset)
            RTStrFree(pNode->pszValueReset);
        pNode->uFlags = 0;
    }

    pNode = RTListNodeGetFirst(&pCache->Node, VBOXSERVICEVEPROPCACHEENTRY, Node);
    while (pNode)
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNext = RTListNodeGetNext(&pNode->Node, VBOXSERVICEVEPROPCACHEENTRY, Node);
        RTListNodeRemove(&pNode->Node);
        if (pNext && RTListNodeIsLast(&pCache->Node, &pNext->Node))
            break;
        pNode = pNext;
    }
}
#endif /* VBOX_WITH_GUEST_PROPS */
