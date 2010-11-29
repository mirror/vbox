/* $Id$ */
/** @file
 * IPRT - Manifest, the core.
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
#include "internal/iprt.h"
#include <iprt/manifest.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/md5.h>
#include <iprt/sha.h>
#include <iprt/string.h>
#include <iprt/vfs.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Manifest attribute.
 *
 * Used both for entries and manifest attributes.
 */
typedef struct RTMANIFESTATTR
{
    /** The string space core (szName). */
    RTSTRSPACECORE      StrCore;
    /** The property value. */
    char               *pszValue;
    /** The attribute type if applicable, RTMANIFEST_ATTR_UNKNOWN if not. */
    uint32_t            fType;
    /** The normalized property name that StrCore::pszString points at. */
    char                szName[1];
} RTMANIFESTATTR;
/** Pointer to a manifest attribute. */
typedef RTMANIFESTATTR *PRTMANIFESTATTR;


/**
 * Manifest entry.
 */
typedef struct RTMANIFESTENTRY
{
    /** The string space core (szName). */
    RTSTRSPACECORE      StrCore;
    /** The entry attributes (hashes, checksums, size, etc) -
     *  RTMANIFESTATTR. */
    RTSTRSPACE          Attributes;
    /** The normalized entry name that StrCore::pszString points at. */
    char                szName[1];
} RTMANIFESTENTRY;
/** Pointer to a manifest entry. */
typedef RTMANIFESTENTRY *PRTMANIFESTENTRY;


/**
 * Manifest handle data.
 */
typedef struct RTMANIFESTINT
{
    /** Magic value (RTMANIFEST_MAGIC). */
    uint32_t            u32Magic;
    /** The number of references to this manifest. */
    uint32_t volatile   cRefs;
    /** Manifest attributes - RTMANIFESTATTR. */
    RTSTRSPACE          Attributes;
    /** String space of the entries covered by this manifest -
     *  RTMANIFESTENTRY. */
    RTSTRSPACE          Entries;
} RTMANIFESTINT;

/** The value of RTMANIFESTINT::u32Magic. */
#define RTMANIFEST_MAGIC    UINT32_C(0x99998866)


/**
 * Creates an empty manifest.
 *
 * @returns IPRT status code.
 * @param   fFlags              Flags, MBZ.
 * @param   phManifest          Where to return the handle to the manifest.
 */
RTDECL(int) RTManifestCreate(uint32_t fFlags, PRTMANIFEST phManifest)
{
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertPtr(phManifest);

    RTMANIFESTINT *pThis = (RTMANIFESTINT *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic     = RTMANIFEST_MAGIC;
    pThis->cRefs        = 1;
    pThis->Attributes   = NULL;
    pThis->Entries      = NULL;

    *phManifest = pThis;
    return VINF_SUCCESS;
}

/**
 * Retains a reference to the manifest handle.
 *
 * @returns The new reference count, UINT32_MAX if the handle is invalid.
 * @param   hManifest           The handle to retain.
 */
RTDECL(uint32_t) RTManifestRetain(RTMANIFEST hManifest)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1 && cRefs < _1M);

    return cRefs;
}


/**
 * @callback_method_impl{FNRTSTRSPACECALLBACK, Destroys RTMANIFESTATTR.}
 */
static DECLCALLBACK(int) rtManifestDestroyAttribute(PRTSTRSPACECORE pStr, void *pvUser)
{
    PRTMANIFESTATTR pAttr = RT_FROM_MEMBER(pStr, RTMANIFESTATTR, StrCore);
    RTStrFree(pAttr->pszValue);
    pAttr->pszValue = NULL;
    RTMemFree(pAttr);
    return 0;
}


/**
 * @callback_method_impl{FNRTSTRSPACECALLBACK, Destroys RTMANIFESTENTRY.}
 */
static DECLCALLBACK(int) rtManifestDestroyEntry(PRTSTRSPACECORE pStr, void *pvUser)
{
    PRTMANIFESTENTRY pEntry = RT_FROM_MEMBER(pStr, RTMANIFESTENTRY, StrCore);
    RTStrSpaceDestroy(&pEntry->Attributes, rtManifestDestroyAttribute, pvUser);
    RTMemFree(pEntry);
    return 0;
}


/**
 * Releases a reference to the manifest handle.
 *
 * @returns The new reference count, 0 if free. UINT32_MAX is returned if the
 *          handle is invalid.
 * @param   hManifest           The handle to release.
 *                              NIL is quietly ignored (returns 0).
 */
RTDECL(uint32_t) RTManifestRelease(RTMANIFEST hManifest)
{
    RTMANIFESTINT *pThis = hManifest;
    if (pThis == NIL_RTMANIFEST)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < _1M);
    if (!cRefs)
    {
        ASMAtomicWriteU32(&pThis->u32Magic, ~RTMANIFEST_MAGIC);
        RTStrSpaceDestroy(&pThis->Attributes, rtManifestDestroyAttribute, pThis);
        RTStrSpaceDestroy(&pThis->Entries,    rtManifestDestroyEntry,     pThis);
        RTMemFree(pThis);
    }

    return cRefs;
}


/**
 * Creates a duplicate of the specified manifest.
 *
 * @returns IPRT status code
 * @param   hManifestSrc        The manifest to clone.
 * @param   phManifestDst       Where to store the handle to the duplicate.
 */
RTDECL(int) RTManifestDup(RTMANIFEST hManifestSrc, PRTMANIFEST phManifestDst)
{
    RTMANIFESTINT *pThis = hManifestSrc;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(phManifestDst);


    /** @todo implement cloning. */

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTManifestEqualsEx(RTMANIFEST hManifest1, RTMANIFEST hManifest2, const char * const *papszIgnoreEntries,
                               const char * const *papszIgnoreAttr, char *pszEntry, size_t cbEntry)
{
    /*
     * Validate input.
     */
    AssertPtrNullReturn(pszEntry, VERR_INVALID_POINTER);
    if (pszEntry && cbEntry)
        *pszEntry = '\0';
    RTMANIFESTINT *pThis1 = hManifest1;
    RTMANIFESTINT *pThis2 = hManifest1;
    if (pThis1 != NIL_RTMANIFEST)
    {
        AssertPtrReturn(pThis1, VERR_INVALID_HANDLE);
        AssertReturn(pThis1->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    }
    if (pThis2 != NIL_RTMANIFEST)
    {
        AssertPtrReturn(pThis2, VERR_INVALID_HANDLE);
        AssertReturn(pThis2->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    }

    /*
     * The simple cases.
     */
    if (pThis1 == pThis2)
        return VINF_SUCCESS;
    if (pThis1 == NIL_RTMANIFEST || pThis2 == NIL_RTMANIFEST)
        return VERR_NOT_EQUAL;

    /*
     *
     */


    /** @todo implement comparing. */

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTManifestEquals(RTMANIFEST hManifest1, RTMANIFEST hManifest2)
{
    return RTManifestEqualsEx(hManifest1, hManifest2, NULL /*papszIgnoreEntries*/, NULL /*papszIgnoreAttrs*/, NULL, 0);
}


/**
 * Worker common to RTManifestSetAttr and RTManifestEntrySetAttr.
 *
 * @returns IPRT status code.
 * @param   pAttributes         The attribute container.
 * @param   pszAttr             The name of the attribute to add.
 * @param   pszValue            The value string.
 * @param   fType               The attribute type type.
 */
static int rtManifestSetAttrWorker(PRTSTRSPACE pAttributes, const char *pszAttr, const char *pszValue, uint32_t fType)
{
    char *pszValueCopy;
    int rc = RTStrDupEx(&pszValueCopy, pszValue);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Does the attribute exist already?
     */
    AssertCompileMemberOffset(RTMANIFESTATTR, StrCore, 0);
    PRTMANIFESTATTR pAttr = (PRTMANIFESTATTR)RTStrSpaceGet(pAttributes, pszAttr);
    if (pAttr)
    {
        RTStrFree(pAttr->pszValue);
        pAttr->pszValue = pszValueCopy;
        pAttr->fType    = fType;
    }
    else
    {
        size_t          cbName = strlen(pszAttr) + 1;
        pAttr = (PRTMANIFESTATTR)RTMemAllocVar(RT_OFFSETOF(RTMANIFESTATTR, szName[cbName]));
        if (!pAttr)
        {
            RTStrFree(pszValueCopy);
            return VERR_NO_MEMORY;
        }
        memcpy(pAttr->szName, pszAttr, cbName);
        pAttr->StrCore.pszString = pAttr->szName;
        pAttr->StrCore.cchString = cbName - 1;
        pAttr->pszValue = pszValueCopy;
        pAttr->fType    = fType;
        if (RT_UNLIKELY(!RTStrSpaceInsert(pAttributes, &pAttr->StrCore)))
        {
            AssertFailed();
            RTStrFree(pszValueCopy);
            RTMemFree(pAttr);
            return VERR_INTERNAL_ERROR_4;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Sets a manifest attribute.
 *
 * @returns IPRT status code.
 * @param   hManifest           The manifest handle.
 * @param   pszAttr             The attribute name.  If this already exists,
 *                              its value will be replaced.
 * @param   pszValue            The value string.
 * @param   fType               The attribute type, pass
 *                              RTMANIFEST_ATTR_UNKNOWN if not known.
 */
RTDECL(int) RTManifestSetAttr(RTMANIFEST hManifest, const char *pszAttr, const char *pszValue, uint32_t fType)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pszAttr);
    AssertPtr(pszValue);
    AssertReturn(RT_IS_POWER_OF_TWO(fType) && fType < RTMANIFEST_ATTR_END, VERR_INVALID_PARAMETER);

    return rtManifestSetAttrWorker(&pThis->Attributes, pszAttr, pszValue, fType);
}


/**
 * Worker common to RTManifestUnsetAttr and RTManifestEntryUnsetAttr.
 *
 * @returns IPRT status code.
 * @param   pAttributes         The attribute container.
 * @param   pszAttr             The name of the attribute to remove.
 */
static int rtManifestUnsetAttrWorker(PRTSTRSPACE pAttributes, const char *pszAttr)
{
    PRTSTRSPACECORE pStrCore = RTStrSpaceRemove(pAttributes, pszAttr);
    if (!pStrCore)
        return VWRN_NOT_FOUND;
    rtManifestDestroyAttribute(pStrCore, NULL);
    return VINF_SUCCESS;
}


/**
 * Unsets (removes) a manifest attribute if it exists.
 *
 * @returns IPRT status code.
 * @retval  VWRN_NOT_FOUND if not found.
 *
 * @param   hManifest           The manifest handle.
 * @param   pszAttr             The attribute name.
 */
RTDECL(int) RTManifestUnsetAttr(RTMANIFEST hManifest, const char *pszAttr)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pszAttr);

    return rtManifestUnsetAttrWorker(&pThis->Attributes, pszAttr);
}


/**
 * Validates the name entry.
 *
 * @returns IPRT status code.
 * @param   pszEntry            The entry name to validate.
 * @param   pfNeedNormalization Where to return whether it needs normalization
 *                              or not.  Optional.
 * @param   pcchEntry           Where to return the length.  Optional.
 */
static int rtManifestValidateNameEntry(const char *pszEntry, bool *pfNeedNormalization, size_t *pcchEntry)
{
    int         rc;
    bool        fNeedNormalization = false;
    const char *pszCur             = pszEntry;

    for (;;)
    {
        RTUNICP uc;
        rc = RTStrGetCpEx(&pszCur, &uc);
        if (RT_FAILURE(rc))
            break;
        if (!uc)
            break;
        if (uc == '\\')
            fNeedNormalization = true;
        else if (uc < 32 || uc == ':' || uc == '(' || uc == ')')
        {
            rc = VERR_INVALID_NAME;
            break;
        }
    }

    if (pfNeedNormalization)
        *pfNeedNormalization = fNeedNormalization;
    if (pcchEntry)
        *pcchEntry = pszCur - pszEntry - 1;
    return rc;
}


/**
 * Normalizes a entry name.
 *
 * @param   pszEntry            The entry name to normalize.
 */
static void rtManifestNormalizeEntry(char *pszEntry)
{
    char  ch;
    while ((ch = *pszEntry))
    {
        if (ch == '\\')
            *pszEntry = '/';
        pszEntry++;
    }
}


/**
 * Gets an entry.
 *
 * @returns IPRT status code.
 * @param   pThis               The manifest to work with.
 * @param   pszEntry            The entry name.
 * @param   fNeedNormalization  Whether rtManifestValidateNameEntry said it
 *                              needed normalization.
 * @param   cchEntry            The length of the name.
 * @param   ppEntry             Where to return the entry pointer on success.
 */
static int rtManifestGetEntry(RTMANIFESTINT *pThis, const char *pszEntry, bool fNeedNormalization, size_t cchEntry,
                              PRTMANIFESTENTRY *ppEntry)
{
    PRTMANIFESTENTRY pEntry;

    AssertCompileMemberOffset(RTMANIFESTATTR, StrCore, 0);
    if (!fNeedNormalization)
        pEntry = (PRTMANIFESTENTRY)RTStrSpaceGet(&pThis->Entries, pszEntry);
    else
    {
        char *pszCopy = (char *)RTMemTmpAlloc(cchEntry + 1);
        if (RT_UNLIKELY(!pszCopy))
            return VERR_NO_TMP_MEMORY;
        memcpy(pszCopy, pszEntry, cchEntry + 1);
        rtManifestNormalizeEntry(pszCopy);

        pEntry = (PRTMANIFESTENTRY)RTStrSpaceGet(&pThis->Entries, pszCopy);
        RTMemTmpFree(pszCopy);
    }

    *ppEntry = pEntry;
    return pEntry ? VINF_SUCCESS : VERR_NOT_FOUND;
}


/**
 * Sets an attribute of a manifest entry.
 *
 * @returns IPRT status code.
 * @param   hManifest           The manifest handle.
 * @param   pszEntry            The entry name.  This will automatically be
 *                              added if there was no previous call to
 *                              RTManifestEntryAdd for this name.  See
 *                              RTManifestEntryAdd for the entry name rules.
 * @param   pszAttr             The attribute name.  If this already exists,
 *                              its value will be replaced.
 * @param   pszValue            The value string.
 * @param   fType               The attribute type, pass
 *                              RTMANIFEST_ATTR_UNKNOWN if not known.
 */
RTDECL(int) RTManifestEntrySetAttr(RTMANIFEST hManifest, const char *pszEntry, const char *pszAttr,
                                   const char *pszValue, uint32_t fType)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pszEntry);
    AssertPtr(pszAttr);
    AssertPtr(pszValue);
    AssertReturn(RT_IS_POWER_OF_TWO(fType) && fType < RTMANIFEST_ATTR_END, VERR_INVALID_PARAMETER);

    bool    fNeedNormalization;
    size_t  cchEntry;
    int rc = rtManifestValidateNameEntry(pszEntry, &fNeedNormalization, &cchEntry);
    AssertRCReturn(rc, rc);

    /*
     * Resolve the entry, adding one if necessary.
     */
    PRTMANIFESTENTRY pEntry;
    rc = rtManifestGetEntry(pThis, pszEntry, fNeedNormalization, cchEntry, &pEntry);
    if (rc == VERR_NOT_FOUND)
    {
        pEntry = (PRTMANIFESTENTRY)RTMemAlloc(RT_OFFSETOF(RTMANIFESTENTRY, szName[cchEntry + 1]));
        if (!pEntry)
            return VERR_NO_MEMORY;

        pEntry->StrCore.cchString = cchEntry;
        pEntry->StrCore.pszString = pEntry->szName;
        pEntry->Attributes = NULL;
        memcpy(pEntry->szName, pszEntry, cchEntry + 1);
        if (fNeedNormalization)
            rtManifestNormalizeEntry(pEntry->szName);

        if (!RTStrSpaceInsert(&pThis->Entries, &pEntry->StrCore))
        {
            RTMemFree(pEntry);
            return VERR_INTERNAL_ERROR_4;
        }
    }
    else if (RT_FAILURE(rc))
        return rc;

    return rtManifestSetAttrWorker(&pEntry->Attributes, pszAttr, pszValue, fType);
}


/**
 * Unsets (removes) an attribute of a manifest entry if they both exist.
 *
 * @returns IPRT status code.
 * @retval  VWRN_NOT_FOUND if not found.
 *
 * @param   hManifest           The manifest handle.
 * @param   pszEntry            The entry name.
 * @param   pszAttr             The attribute name.
 */
RTDECL(int) RTManifestEntryUnsetAttr(RTMANIFEST hManifest, const char *pszEntry, const char *pszAttr)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pszEntry);
    AssertPtr(pszAttr);

    bool    fNeedNormalization;
    size_t  cchEntry;
    int rc = rtManifestValidateNameEntry(pszEntry, &fNeedNormalization, &cchEntry);
    AssertRCReturn(rc, rc);

    /*
     * Resolve the entry and hand it over to the worker.
     */
    PRTMANIFESTENTRY pEntry;
    rc = rtManifestGetEntry(pThis, pszEntry, fNeedNormalization, cchEntry, &pEntry);
    if (RT_SUCCESS(rc))
        rc = rtManifestUnsetAttrWorker(&pEntry->Attributes, pszAttr);
    return rc;
}


/**
 * Adds a new entry to a manifest.
 *
 * The entry name rules:
 *     - The entry name can contain any character defined by unicode, except
 *       control characters, ':', '(' and ')'.  The exceptions are mainly there
 *       because of uncertainty around how various formats handles these.
 *     - It is considered case sensitive.
 *     - Forward (unix) and backward (dos) slashes are considered path
 *       separators and converted to forward slashes.
 *
 * @returns IPRT status code.
 * @retval  VWRN_ALREADY_EXISTS if the entry already exists.
 *
 * @param   hManifest           The manifest handle.
 * @param   pszEntry            The entry name (UTF-8).
 *
 * @remarks Some manifest formats will not be able to store an entry without
 *          any attributes.  So, this is just here in case it comes in handy
 *          when dealing with formats which can.
 */
RTDECL(int) RTManifestEntryAdd(RTMANIFEST hManifest, const char *pszEntry)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pszEntry);

    bool    fNeedNormalization;
    size_t  cchEntry;
    int rc = rtManifestValidateNameEntry(pszEntry, &fNeedNormalization, &cchEntry);
    AssertRCReturn(rc, rc);

    /*
     * Only add one if it does not already exist.
     */
    PRTMANIFESTENTRY pEntry;
    rc = rtManifestGetEntry(pThis, pszEntry, fNeedNormalization, cchEntry, &pEntry);
    if (rc == VERR_NOT_FOUND)
    {
        pEntry = (PRTMANIFESTENTRY)RTMemAlloc(RT_OFFSETOF(RTMANIFESTENTRY, szName[cchEntry + 1]));
        if (pEntry)
        {
            pEntry->StrCore.cchString = cchEntry;
            pEntry->StrCore.pszString = pEntry->szName;
            pEntry->Attributes = NULL;
            memcpy(pEntry->szName, pszEntry, cchEntry + 1);
            if (fNeedNormalization)
                rtManifestNormalizeEntry(pEntry->szName);

            if (RTStrSpaceInsert(&pThis->Entries, &pEntry->StrCore))
                rc = VINF_SUCCESS;
            else
            {
                RTMemFree(pEntry);
                rc = VERR_INTERNAL_ERROR_4;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (RT_SUCCESS(rc))
        rc = VWRN_ALREADY_EXISTS;

    return rc;
}


/**
 * Removes an entry.
 *
 * @returns IPRT status code.
 * @param   hManifest           The manifest handle.
 * @param   pszEntry            The entry name.
 */
RTDECL(int) RTManifestEntryRemove(RTMANIFEST hManifest, const char *pszEntry)
{
    RTMANIFESTINT *pThis = hManifest;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMANIFEST_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pszEntry);

    bool    fNeedNormalization;
    size_t  cchEntry;
    int rc = rtManifestValidateNameEntry(pszEntry, &fNeedNormalization, &cchEntry);
    AssertRCReturn(rc, rc);

    /*
     * Only add one if it does not already exist.
     */
    PRTMANIFESTENTRY pEntry;
    rc = rtManifestGetEntry(pThis, pszEntry, fNeedNormalization, cchEntry, &pEntry);
    if (RT_SUCCESS(rc))
    {
        PRTSTRSPACECORE pStrCore = RTStrSpaceRemove(&pThis->Entries, pEntry->StrCore.pszString);
        AssertReturn(pStrCore, VERR_INTERNAL_ERROR_3);
        rtManifestDestroyEntry(pStrCore, pThis);
    }

    return rc;
}


/**
 * Reads a line from a VFS I/O stream.
 *
 * @todo    Replace this with a buffered I/O stream layer.
 *
 * @returns IPRT status code.  VERR_EOF when trying to read beyond the stream
 *          end.
 * @param   hVfsIos             The I/O stream to read from.
 * @param   pszLine             Where to store what we've read.
 * @param   cbLine              The number of bytes to read.
 */
static int rtManifestReadLine(RTVFSIOSTREAM hVfsIos, char *pszLine, size_t cbLine)
{
    /* This is horribly slow right now, but it's not a biggy as the input is
       usually cached in memory somewhere... */
    *pszLine = '\0';
    while (cbLine > 1)
    {
        char ch;
        int rc = RTVfsIoStrmRead(hVfsIos, &ch, 1, true /*fBLocking*/, NULL);
        if (RT_FAILURE(rc))
            return rc == VERR_EOF ? VINF_EOF : rc;

        /* \r\n */
        if (ch == '\r')
        {
            if (cbLine <= 2)
            {
                pszLine[0] = ch;
                pszLine[1] = '\0';
                return VINF_BUFFER_OVERFLOW;
            }

            rc = RTVfsIoStrmRead(hVfsIos, &ch, 1, true /*fBLocking*/, NULL);
            if (RT_SUCCESS(rc) && ch == '\n')
                return VINF_SUCCESS;
            pszLine[0] = '\r';
            pszLine[1] = ch;
            pszLine[2] = '\0';
            if (RT_FAILURE(rc))
                return rc == VERR_EOF ? VINF_EOF : rc;
        }

        /* \n */
        if (ch == '\n')
            return VINF_SUCCESS;

        /* add character. */
        pszLine[0] = ch;
        pszLine[1] = '\0';

        /* advance */
        pszLine++;
        cbLine--;
    }

    return VINF_BUFFER_OVERFLOW;
}


RTDECL(int) RTManifestReadStandardEx(RTMANIFEST hManifest, RTVFSIOSTREAM hVfsIos, char *pszErr, size_t cbErr)
{
    if (pszErr && cbErr)
        *pszErr = '\0';

    uint32_t iLine = 0;
    for (;;)
    {
        /*
         * Read a line from the input stream.
         */
        iLine++;
        char szLine[RTPATH_MAX + RTSHA512_DIGEST_LEN + 32];
        int rc = rtManifestReadLine(hVfsIos, szLine, sizeof(szLine));
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_EOF)
                return VINF_SUCCESS;
            RTStrPrintf(pszErr, cbErr, "Error reading line #u: %Rrc", iLine, rc);
            return rc;
        }
        if (rc != VINF_SUCCESS)
        {
            RTStrPrintf(pszErr, cbErr, "Line number %u is too long", iLine);
            return VERR_OUT_OF_RANGE;
        }

        /*
         * Strip it and skip if empty.
         */
        char *psz = RTStrStrip(szLine);
        if (!*psz)
            continue;

        /*
         * Read the attribute name.
         */
        const char * const pszAttr = psz;
        do
            psz++;
        while (!RT_C_IS_BLANK(*psz) && *psz);
        if (*psz)
            *psz++ = '\0';

        /*
         * The entry name is enclosed in parenthesis and followed by a '='.
         */
        psz = RTStrStripL(psz);
        if (*psz != '(')
        {
            RTStrPrintf(pszErr, cbErr, "Expected '(' after %zu on line %u", psz - szLine, iLine);
            return VERR_PARSE_ERROR;
        }
        const char * const pszName = ++psz;
        while (*psz)
        {
            if (*psz == ')')
            {
                char *psz2 = RTStrStripL(psz + 1);
                if (*psz2 == '=')
                {
                    *psz = '\0';
                    psz = psz2;
                    break;
                }
            }
            psz++;
        }

        if (*psz != '=')
        {
            RTStrPrintf(pszErr, cbErr, "Expected ')=' at %zu on line %u", psz - szLine, iLine);
            return VERR_PARSE_ERROR;
        }

        /*
         * The value.
         */
        psz = RTStrStrip(psz + 1);
        const char * const pszValue = psz;
        if (!*psz)
        {
            RTStrPrintf(pszErr, cbErr, "Expected value at %zu on line %u", psz - szLine, iLine);
            return VERR_PARSE_ERROR;
        }

        /*
         * Detect attribute type and sanity check the value.
         */
        uint32_t fType = RTMANIFEST_ATTR_UNKNOWN;
        static const struct
        {
            const char *pszAttr;
            uint32_t    fType;
            unsigned    cBits;
            unsigned    uBase;
        } s_aDecAttrs[] =
        {
            { "SIZE", RTMANIFEST_ATTR_SIZE, 64,  10}
        };
        for (unsigned i = 0; i < RT_ELEMENTS(s_aDecAttrs); i++)
            if (!strcmp(s_aDecAttrs[i].pszAttr, pszAttr))
            {
                fType = s_aDecAttrs[i].fType;
                rc = RTStrToUInt64Full(pszValue, s_aDecAttrs[i].uBase, NULL);
                if (rc != VINF_SUCCESS)
                {
                    RTStrPrintf(pszErr, cbErr, "Malformed value ('%s') at %zu on line %u: %Rrc", pszValue, psz - szLine, iLine, rc);
                    return VERR_PARSE_ERROR;
                }
                break;
            }

        if (fType == RTMANIFEST_ATTR_UNKNOWN)
        {
            static const struct
            {
                const char *pszAttr;
                uint32_t    fType;
                unsigned    cchHex;
            } s_aHexAttrs[] =
            {
                { "MD5",        RTMANIFEST_ATTR_MD5,        RTMD5_DIGEST_LEN    },
                { "SHA1",       RTMANIFEST_ATTR_SHA1,       RTSHA1_DIGEST_LEN   },
                { "SHA256",     RTMANIFEST_ATTR_SHA256,     RTSHA256_DIGEST_LEN },
                { "SHA512",     RTMANIFEST_ATTR_SHA512,     RTSHA512_DIGEST_LEN }
            };
            for (unsigned i = 0; i < RT_ELEMENTS(s_aHexAttrs); i++)
                if (!strcmp(s_aHexAttrs[i].pszAttr, pszAttr))
                {
                    fType = s_aHexAttrs[i].fType;
                    for (unsigned off = 0; off < s_aHexAttrs[i].cchHex; off++)
                        if (!RT_C_IS_XDIGIT(pszAttr[off]))
                        {
                            RTStrPrintf(pszErr, cbErr, "Expected hex digit at %zu on line %u (value '%s', pos %u)",
                                        pszValue - szLine + off, iLine, pszValue, off);
                            return VERR_PARSE_ERROR;
                        }
                    break;
                }
        }

        /*
         * Finally, add it.
         */
        rc = RTManifestEntrySetAttr(hManifest, pszName, pszAttr, pszValue, fType);
        if (RT_FAILURE(rc))
        {
            RTStrPrintf(pszErr, cbErr, "RTManifestEntrySetAttr(,'%s','%s', '%s', %#x) failed on line %u: %Rrc",
                        pszName, pszAttr, pszValue, fType, iLine, rc);
            return rc;
        }
    }
}

RTDECL(int) RTManifestReadStandard(RTMANIFEST hManifest, RTVFSIOSTREAM hVfsIos)
{
    return RTManifestReadStandardEx(hManifest, hVfsIos, NULL, 0);
}


RTDECL(int) RTManifestWriteStandard(RTMANIFEST hManifest, RTVFSIOSTREAM hVfsIos)
{
    return VERR_NOT_IMPLEMENTED;
}

