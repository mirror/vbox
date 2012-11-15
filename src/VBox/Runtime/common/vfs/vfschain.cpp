/* $Id$ */
/** @file
 * IPRT - Virtual File System, Chains.
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
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include <iprt/asm.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include "internal/file.h"
#include "internal/magics.h"
//#include "internal/vfs.h"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Init the critical section once. */
static RTONCE       g_rtVfsChainElementInitOnce;
/** Critical section protecting g_rtVfsChainElementProviderList. */
static RTCRITSECT   g_rtVfsChainElementCritSect;
/** List of VFS chain element providers (RTVFSCHAINELEMENTREG). */
static RTLISTANCHOR g_rtVfsChainElementProviderList;


/**
 * Initializes the globals via RTOnce.
 *
 * @returns IPRT status code
 * @param   pvUser              Unused, ignored.
 */
static DECLCALLBACK(int) rtVfsChainElementRegisterInit(void *pvUser)
{
    NOREF(pvUser);
    return RTCritSectInit(&g_rtVfsChainElementCritSect);
}


RTDECL(int) RTVfsChainElementRegisterProvider(PRTVFSCHAINELEMENTREG pRegRec, bool fFromCtor)
{
    int rc;

    /*
     * Input validation.
     */
    AssertPtrReturn(pRegRec, VERR_INVALID_POINTER);
    AssertMsgReturn(pRegRec->uVersion   == RTVFSCHAINELEMENTREG_VERSION, ("%#x", pRegRec->uVersion),    VERR_INVALID_POINTER);
    AssertMsgReturn(pRegRec->uEndMarker == RTVFSCHAINELEMENTREG_VERSION, ("%#zx", pRegRec->uEndMarker), VERR_INVALID_POINTER);
    AssertReturn(pRegRec->fReserved == 0, VERR_INVALID_POINTER);
    AssertPtrReturn(pRegRec->pszName, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pRegRec->pfnOpenVfs,      VERR_INVALID_POINTER);
    AssertPtrNullReturn(pRegRec->pfnOpenDir,      VERR_INVALID_POINTER);
    AssertPtrNullReturn(pRegRec->pfnOpenFile,     VERR_INVALID_POINTER);
    AssertPtrNullReturn(pRegRec->pfnOpenIoStream, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pRegRec->pfnOpenFsStream, VERR_INVALID_POINTER);

    /*
     * Init and take the lock.
     */
    if (!fFromCtor)
    {
        rc = RTOnce(&g_rtVfsChainElementInitOnce, rtVfsChainElementRegisterInit, NULL);
        if (RT_FAILURE(rc))
            return rc;
        rc = RTCritSectEnter(&g_rtVfsChainElementCritSect);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Duplicate name?
     */
    rc = VINF_SUCCESS;
    PRTVFSCHAINELEMENTREG pIterator, pIterNext;
    RTListForEachSafe(&g_rtVfsChainElementProviderList, pIterator, pIterNext, RTVFSCHAINELEMENTREG, ListEntry)
    {
        if (!strcmp(pIterator->pszName, pRegRec->pszName))
        {
            AssertMsgFailed(("duplicate name '%s' old=%p new=%p\n",  pIterator->pszName, pIterator, pRegRec));
            rc = VERR_ALREADY_EXISTS;
            break;
        }
    }

    /*
     * If not, append the record to the list.
     */
    if (RT_SUCCESS(rc))
        RTListAppend(&g_rtVfsChainElementProviderList, &pRegRec->ListEntry);

    /*
     * Leave the lock and return.
     */
    if (!fFromCtor)
        RTCritSectLeave(&g_rtVfsChainElementCritSect);
    return rc;
}


/**
 * Allocates and initializes an empty spec
 *
 * @returns Pointer to the spec on success, NULL on failure.
 */
static PRTVFSCHAINSPEC rtVfsChainSpecAlloc(void)
{
    PRTVFSCHAINSPEC pSpec = (PRTVFSCHAINSPEC)RTMemTmpAlloc(sizeof(*pSpec));
    if (pSpec)
    {
        pSpec->iActionElement = UINT32_MAX;
        pSpec->cElements      = 0;
        pSpec->paElements     = NULL;
    }
    return pSpec;
}


/**
 * Duplicate a spec string.
 *
 * This differs from RTStrDupN in that it uses RTMemTmpAlloc instead of
 * RTMemAlloc.
 *
 * @returns String copy on success, NULL on failure.
 * @param   psz         The string to duplicate.
 * @param   cch         The number of bytes to duplicate.
 * @param   prc         The status code variable to set on failure. (Leeps the
 *                      code shorter. -lazy bird)
 */
DECLINLINE(char *) rtVfsChainSpecDupStrN(const char *psz, size_t cch, int *prc)
{
    char *pszCopy = (char *)RTMemTmpAlloc(cch + 1);
    if (pszCopy)
    {
        if (!memchr(psz, '\\', cch))
        {
            /* Plain string, copy it raw. */
            memcpy(pszCopy, psz, cch);
            pszCopy[cch] = '\0';
        }
        else
        {
            /* Has escape sequences, must unescape it. */
            char *pszDst = pszCopy;
            while (cch)
            {
                char ch = *psz++;
                if (ch == '\\')
                {
                    char ch2 = psz[2];
                    if (ch2 == '(' || ch2 == ')' || ch2 == '\\' || ch2 == ',')
                    {
                        psz++;
                        ch = ch2;
                    }
                }
                *pszDst++ = ch;
            }
            *pszDst = '\0';
        }
    }
    else
        *prc = VERR_NO_TMP_MEMORY;
    return pszCopy;
}


/**
 * Adds an empty element to the chain specification.
 *
 * The caller is responsible for filling it the element attributes.
 *
 * @returns Pointer to the new element on success, NULL on failure.  The
 *          pointer is only valid till the next call to this function.
 * @param   pSpec       The chain specification.
 * @param   prc         The status code variable to set on failure. (Leeps the
 *                      code shorter. -lazy bird)
 */
static PRTVFSCHAINELEMSPEC rtVfsChainSpecAddElement(PRTVFSCHAINSPEC pSpec, int *prc)
{
    AssertPtr(pSpec);

    /*
     * Resize the element table if necessary.
     */
    uint32_t const iElement = pSpec->cElements;
    if ((iElement % 32) == 0)
    {
        PRTVFSCHAINELEMSPEC paNew = (PRTVFSCHAINELEMSPEC)RTMemTmpAlloc((iElement + 32) * sizeof(paNew[0]));
        if (!paNew)
        {
            *prc = VERR_NO_TMP_MEMORY;
            return NULL;
        }

        memcpy(paNew, pSpec->paElements, iElement * sizeof(paNew[0]));
        RTMemTmpFree(pSpec->paElements);
        pSpec->paElements = paNew;
    }

    /*
     * Initialize and add the new element.
     */
    PRTVFSCHAINELEMSPEC pElement = &pSpec->paElements[iElement];
    pElement->pszProvider = NULL;
    pElement->enmTypeIn   = iElement ? pSpec->paElements[iElement - 1].enmTypeOut : RTVFSOBJTYPE_INVALID;
    pElement->enmTypeOut  = RTVFSOBJTYPE_INVALID;
    pElement->enmAction   = RTVFSCHAINACTION_INVALID;
    pElement->cArgs       = 0;
    pElement->papszArgs   = 0;

    pSpec->cElements  = iElement + 1;
    return pElement;
}


/**
 * Adds an argument to the element spec.
 *
 * @returns IPRT status code.
 * @param   pElement            The element.
 * @param   psz                 The start of the argument string.
 * @param   cch                 The length of the argument string, escape
 *                              sequences counted twice.
 */
static int rtVfsChainSpecElementAddArg(PRTVFSCHAINELEMSPEC pElement, const char *psz, size_t cch)
{
    uint32_t iArg = pElement->cArgs;
    if ((iArg % 32) == 0)
    {
        char **papszNew = (char **)RTMemTmpAlloc((iArg + 32 + 1) * sizeof(papszNew[0]));
        if (!papszNew)
            return VERR_NO_TMP_MEMORY;
        memcpy(papszNew, pElement->papszArgs, iArg * sizeof(papszNew[0]));
        RTMemTmpFree(pElement->papszArgs);
        pElement->papszArgs = papszNew;
    }

    int rc = VINF_SUCCESS;
    pElement->papszArgs[iArg] = rtVfsChainSpecDupStrN(psz, cch, &rc);
    pElement->papszArgs[iArg + 1] = NULL;
    pElement->cArgs = iArg + 1;
    return rc;
}


RTDECL(void)    RTVfsChainSpecFree(PRTVFSCHAINSPEC pSpec)
{
    if (!pSpec)
        return;

    uint32_t i = pSpec->cElements;
    while (i-- > 0)
    {
        uint32_t iArg = pSpec->paElements[i].cArgs;
        while (iArg-- > 0)
            RTMemTmpFree(pSpec->paElements[i].papszArgs[iArg]);
        RTMemTmpFree(pSpec->paElements[i].papszArgs);
        RTMemTmpFree(pSpec->paElements[i].pszProvider);
    }

    RTMemTmpFree(pSpec->paElements);
    pSpec->paElements = NULL;
    RTMemTmpFree(pSpec);
}


/**
 * Finds the end of the argument string.
 *
 * @returns The offset of the end character relative to @a psz.
 * @param   psz                 The argument string.
 */
static size_t rtVfsChainSpecFindArgEnd(const char *psz)
{
    char ch;
    size_t off = 0;
    while (  (ch = psz[off]) != '\0'
           && ch != ','
           && ch != ')'
           && ch != '(')
    {
        /* check for escape sequences. */
        if (   ch == '\\'
            && (psz[off+1] == '(' || psz[off+1] == ')' || psz[off+1] == '\\' || psz[off+1] == ','))
            off++;
        off++;
    }
    return off;
}

/**
 * Look for action.
 *
 * @returns Action.
 * @param   pszSpec             The current spec position.
 * @param   pcchAction          Where to return the length of the action
 *                              string.
 */
static RTVFSCHAINACTION rtVfsChainSpecEatAction(const char *pszSpec, size_t *pcchAction)
{
    switch (*pszSpec)
    {
        case '|':
            *pcchAction = 1;
            return RTVFSCHAINACTION_PASSIVE;
        case '>':
            *pcchAction = 1;
            return RTVFSCHAINACTION_PUSH;
        default:
            *pcchAction = 0;
            return RTVFSCHAINACTION_NONE;
    }
}


RTDECL(int)     RTVfsChainSpecParse(const char *pszSpec, uint32_t fFlags, RTVFSCHAINACTION enmLeadingAction,
                                    RTVFSCHAINACTION enmTrailingAction,
                                    PRTVFSCHAINSPEC *ppSpec, const char **ppszError)
{
    AssertPtrNullReturn(ppszError, VERR_INVALID_POINTER);
    if (ppszError)
        *ppszError = NULL;
    AssertPtrReturn(ppSpec, VERR_INVALID_POINTER);
    *ppSpec = NULL;
    AssertPtrReturn(pszSpec, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~RTVFSCHAIN_PF_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(enmLeadingAction > RTVFSCHAINACTION_INVALID && enmLeadingAction < RTVFSCHAINACTION_END, VERR_INVALID_PARAMETER);

    /*
     * Check the start of the specification and allocate an empty return spec.
     */
    if (strncmp(pszSpec, RTVFSCHAIN_SPEC_PREFIX, sizeof(RTVFSCHAIN_SPEC_PREFIX) - 1))
        return VERR_VFS_CHAIN_NO_PREFIX;
    pszSpec = RTStrStripL(pszSpec + sizeof(RTVFSCHAIN_SPEC_PREFIX) - 1);
    if (!*pszSpec)
        return VERR_VFS_CHAIN_EMPTY;

    PRTVFSCHAINSPEC pSpec = rtVfsChainSpecAlloc();
    if (!pSpec)
        return VERR_NO_TMP_MEMORY;

    /*
     * Parse the spec one element at a time.
     */
    int         rc     = VINF_SUCCESS;
    const char *pszSrc = pszSpec;
    while (*pszSrc && RT_SUCCESS(rc))
    {
        /*
         * Pipe or redirection action symbol, except maybe the first time.
         * The pipe symbol may occur at the end of the spec.
         */
        size_t           cch;
        RTVFSCHAINACTION enmAction = rtVfsChainSpecEatAction(pszSpec, &cch);
        if (enmAction != RTVFSCHAINACTION_NONE)
        {
            pszSrc = RTStrStripL(pszSrc + cch);
            if (!*pszSrc)
            {
                /* Fail if the caller does not approve of a trailing pipe (all
                   other actions non-trailing). */
                if (   enmAction != enmTrailingAction
                    && !(fFlags & RTVFSCHAIN_PF_TRAILING_ACTION_OPTIONAL))
                    rc = VERR_VFS_CHAIN_EXPECTED_ELEMENT;
                break;
            }

            /* There can only be one real action atm. */
            if (enmAction != RTVFSCHAINACTION_PASSIVE)
            {
                if (pSpec->iActionElement != UINT32_MAX)
                {
                    rc = VERR_VFS_CHAIN_MULTIPLE_ACTIONS;
                    break;
                }
                pSpec->iActionElement = pSpec->cElements;
            }
        }
        else if (pSpec->cElements > 0)
        {
            rc = VERR_VFS_CHAIN_EXPECTED_ACTION;
            break;
        }

        /* Check the leading action. */
        if (   pSpec->cElements == 0
            && enmAction != enmLeadingAction
            && !(fFlags & RTVFSCHAIN_PF_LEADING_ACTION_OPTIONAL))
        {
            rc = VERR_VFS_CHAIN_UNEXPECTED_ACTION_TYPE;
            break;
        }

        /*
         * Ok, there should be an element here so add one to the return struct.
         */
        PRTVFSCHAINELEMSPEC pElement = rtVfsChainSpecAddElement(pSpec, &rc);
        if (!pElement)
            break;
        pElement->enmAction = enmAction;

        /*
         * First up is the VFS object type followed by a parentheses.
         */
        if (strncmp(pszSrc, "base", cch = 4) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_BASE;
        else if (strncmp(pszSrc, "vfs",  cch = 3) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_VFS;
        else if (strncmp(pszSrc, "fss",  cch = 3) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_FS_STREAM;
        else if (strncmp(pszSrc, "ios",  cch = 3) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_IO_STREAM;
        else if (strncmp(pszSrc, "dir",  cch = 3) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_DIR;
        else if (strncmp(pszSrc, "file", cch = 4) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_FILE;
        else if (strncmp(pszSrc, "sym",  cch = 3) == 0)
            pElement->enmTypeOut = RTVFSOBJTYPE_SYMLINK;
        else
        {
            rc = VERR_VFS_CHAIN_UNKNOWN_TYPE;
            break;
        }
        pszSrc += cch;

        /* Check and skip the parentheses. */
        if (*pszSrc != '(')
        {
            rc = VERR_VFS_CHAIN_EXPECTED_LEFT_PARENTHESES;
            break;
        }
        pszSrc = RTStrStripL(pszSrc + 1);

        /*
         * The name of the element provider.
         */
        cch = rtVfsChainSpecFindArgEnd(pszSrc);
        if (!cch)
        {
            rc = VERR_VFS_CHAIN_EXPECTED_PROVIDER_NAME;
            break;
        }
        pElement->pszProvider = rtVfsChainSpecDupStrN(pszSrc, cch, &rc);
        if (!pElement->pszProvider)
            break;
        pszSrc += cch;

        /*
         * The arguments.
         */
        while (*pszSrc == ',')
        {
            pszSrc = RTStrStripL(pszSrc + 1);
            cch = rtVfsChainSpecFindArgEnd(pszSrc);
            rc = rtVfsChainSpecElementAddArg(pElement, pszSrc, cch);
            pszSrc += cch;
        }

        /* Must end with a right parentheses. */
        if (*pszSrc != ')')
        {
            rc = VERR_VFS_CHAIN_EXPECTED_RIGHT_PARENTHESES;
            break;
        }
        pszSrc = RTStrStripL(pszSrc + 1);
    }

    /*
     * Return the chain on success; Cleanup and set the error indicator on
     * failure.
     */
    if (RT_SUCCESS(rc))
        *ppSpec = pSpec;
    else
    {
        if (ppszError)
            *ppszError = pszSrc;
        RTVfsChainSpecFree(pSpec);
    }
    return rc;
}





RTDECL(int) RTVfsChainElementDeregisterProvider(PRTVFSCHAINELEMENTREG pRegRec, bool fFromDtor)
{
    /*
     * Fend off wildlife.
     */
    if (pRegRec == NULL)
        return VINF_SUCCESS;
    AssertPtrReturn(pRegRec, VERR_INVALID_POINTER);
    AssertMsgReturn(pRegRec->uVersion   == RTVFSCHAINELEMENTREG_VERSION, ("%#x", pRegRec->uVersion),    VERR_INVALID_POINTER);
    AssertMsgReturn(pRegRec->uEndMarker == RTVFSCHAINELEMENTREG_VERSION, ("%#zx", pRegRec->uEndMarker), VERR_INVALID_POINTER);
    AssertPtrReturn(pRegRec->pszName, VERR_INVALID_POINTER);

    /*
     * Take the lock if that's safe.
     */
    if (!fFromDtor)
        RTCritSectEnter(&g_rtVfsChainElementCritSect);

    /*
     * Ok, remove it.
     */
    int rc = VERR_NOT_FOUND;
    PRTVFSCHAINELEMENTREG pIterator, pIterNext;
    RTListForEachSafe(&g_rtVfsChainElementProviderList, pIterator, pIterNext, RTVFSCHAINELEMENTREG, ListEntry)
    {
        if (pIterator == pRegRec)
        {
            RTListNodeRemove(&pRegRec->ListEntry);
            rc = VINF_SUCCESS;
            break;
        }
    }

    /*
     * Leave the lock and return.
     */
    if (!fFromDtor)
        RTCritSectLeave(&g_rtVfsChainElementCritSect);
    return rc;
}


RTDECL(int) RTVfsChainOpenFile(const char *pszSpec, uint64_t fOpen, PRTVFSFILE phVfsFile, const char **ppszError)
{
    AssertPtrReturn(pszSpec, VERR_INVALID_POINTER);
    AssertReturn(*pszSpec != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsFile, VERR_INVALID_POINTER);
    if (ppszError)
        *ppszError = NULL;

    /*
     * If it's not a VFS chain spec, treat it as a file.
     */
    int rc;
    if (strncmp(pszSpec, RTVFSCHAIN_SPEC_PREFIX, sizeof(RTVFSCHAIN_SPEC_PREFIX) - 1))
    {
        RTFILE hFile;
        rc = RTFileOpen(&hFile, pszSpec, fOpen);
        if (RT_SUCCESS(rc))
        {
            RTVFSFILE hVfsFile;
            rc = RTVfsFileFromRTFile(hFile, fOpen, false /*fLeaveOpen*/, &hVfsFile);
            if (RT_SUCCESS(rc))
                *phVfsFile = hVfsFile;
            else
                RTFileClose(hFile);
        }
    }
    else
    {
        PRTVFSCHAINSPEC pSpec;
        rc = RTVfsChainSpecParse(pszSpec,
                                   RTVFSCHAIN_PF_NO_REAL_ACTION
                                 | RTVFSCHAIN_PF_LEADING_ACTION_OPTIONAL,
                                 RTVFSCHAINACTION_PASSIVE,
                                 RTVFSCHAINACTION_NONE,
                                 &pSpec,
                                 ppszError);
        if (RT_SUCCESS(rc))
        {
            /** @todo implement this when needed. */
            rc = VERR_NOT_IMPLEMENTED;
            RTVfsChainSpecFree(pSpec);
        }
    }
    return rc;
}


RTDECL(int) RTVfsChainOpenIoStream(const char *pszSpec, uint64_t fOpen, PRTVFSIOSTREAM phVfsIos, const char **ppszError)
{
    AssertPtrReturn(pszSpec, VERR_INVALID_POINTER);
    AssertReturn(*pszSpec != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsIos, VERR_INVALID_POINTER);
    if (ppszError)
        *ppszError = NULL;

    /*
     * If it's not a VFS chain spec, treat it as a file.
     */
    int rc;
    if (strncmp(pszSpec, RTVFSCHAIN_SPEC_PREFIX, sizeof(RTVFSCHAIN_SPEC_PREFIX) - 1))
    {
        RTFILE hFile;
        rc = RTFileOpen(&hFile, pszSpec, fOpen);
        if (RT_SUCCESS(rc))
        {
            RTVFSFILE hVfsFile;
            rc = RTVfsFileFromRTFile(hFile, fOpen, false /*fLeaveOpen*/, &hVfsFile);
            if (RT_SUCCESS(rc))
            {
                *phVfsIos = RTVfsFileToIoStream(hVfsFile);
                RTVfsFileRelease(hVfsFile);
            }
            else
                RTFileClose(hFile);
        }
    }
    else
    {
        PRTVFSCHAINSPEC pSpec;
        rc = RTVfsChainSpecParse(pszSpec,
                                   RTVFSCHAIN_PF_NO_REAL_ACTION
                                 | RTVFSCHAIN_PF_LEADING_ACTION_OPTIONAL,
                                 RTVFSCHAINACTION_PASSIVE,
                                 RTVFSCHAINACTION_NONE,
                                 &pSpec,
                                 ppszError);
        if (RT_SUCCESS(rc))
        {


            rc = VERR_NOT_IMPLEMENTED;
            RTVfsChainSpecFree(pSpec);
        }
    }
    return rc;
}



RTDECL(bool) RTVfsChainIsSpec(const char *pszSpec)
{
    return pszSpec
        && strcmp(pszSpec, RTVFSCHAIN_SPEC_PREFIX) == 0;
}

