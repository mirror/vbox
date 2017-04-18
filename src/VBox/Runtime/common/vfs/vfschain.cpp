/* $Id$ */
/** @file
 * IPRT - Virtual File System, Chains.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static PCRTVFSCHAINELEMENTREG rtVfsChainFindProviderLocked(const char *pszProvider);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Init the critical section once. */
static RTONCE       g_rtVfsChainElementInitOnce = RTONCE_INITIALIZER;
/** Critical section protecting g_rtVfsChainElementProviderList. */
static RTCRITSECTRW g_rtVfsChainElementCritSect;
/** List of VFS chain element providers (RTVFSCHAINELEMENTREG). */
static RTLISTANCHOR g_rtVfsChainElementProviderList;



RTDECL(int) RTVfsChainValidateOpenFileOrIoStream(PRTVFSCHAINSPEC pSpec, PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError)
{
    if (pElement->cArgs < 1)
        return VERR_VFS_CHAIN_AT_LEAST_ONE_ARG;
    if (pElement->cArgs > 4)
        return VERR_VFS_CHAIN_AT_MOST_FOUR_ARGS;
    if (!*pElement->paArgs[0].psz)
        return VERR_VFS_CHAIN_EMPTY_ARG;

    /*
     * Calculate the flags, storing them in the first argument.
     */
    const char *pszAccess = pElement->cArgs >= 2 ? pElement->paArgs[1].psz : "";
    if (!*pszAccess)
        pszAccess = (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_READWRITE ? "rw"
                  : (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_READ      ? "r"
                  : (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_WRITE     ? "w"
                  :                                                                   "rw";

    const char *pszDisp = pElement->cArgs >= 3 ? pElement->paArgs[2].psz : "";
    if (!*pszDisp)
        pszDisp = strchr(pszAccess, 'w') != NULL ? "open-create" : "open";

    const char *pszSharing = pElement->cArgs >= 4 ? pElement->paArgs[3].psz : "";

    int rc = RTFileModeToFlagsEx(pszAccess, pszDisp, pszSharing, &pElement->uProvider);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    /*
     * Now try figure out which argument offended us.
     */
    AssertReturn(pElement->cArgs > 1, VERR_VFS_CHAIN_IPE);
    if (   pElement->cArgs == 2
        || RT_FAILURE(RTFileModeToFlagsEx(pszAccess, "open-create", "", &pElement->uProvider)))
        *poffError = pElement->paArgs[1].offSpec;
    else if (   pElement->cArgs == 3
             || RT_FAILURE(RTFileModeToFlagsEx(pszAccess, pszDisp, "", &pElement->uProvider)))
        *poffError = pElement->paArgs[2].offSpec;
    else
        *poffError = pElement->paArgs[3].offSpec;
    return VERR_VFS_CHAIN_INVALID_ARGUMENT;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainOpen_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                 PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError)
{
    RT_NOREF(pProviderReg);

    /*
     * Basic checks.
     */
    if (   pElement->enmType != RTVFSOBJTYPE_DIR
        && pElement->enmType != RTVFSOBJTYPE_FILE
        && pElement->enmType != RTVFSOBJTYPE_IO_STREAM)
        return VERR_VFS_CHAIN_ONLY_FILE_OR_IOS_OR_DIR;
    if (   pElement->enmTypeIn != RTVFSOBJTYPE_DIR
        && pElement->enmTypeIn != RTVFSOBJTYPE_FS_STREAM
        && pElement->enmTypeIn != RTVFSOBJTYPE_VFS)
    {
        if (pElement->enmTypeIn == RTVFSOBJTYPE_INVALID)
        {
            /*
             * First element: Ttransform into 'stdfile' or 'stddir' if registered.
             */
            const char            *pszNewProvider = pElement->enmType == RTVFSOBJTYPE_DIR ? "stddir" : "stdfile";
            PCRTVFSCHAINELEMENTREG pNewProvider   = rtVfsChainFindProviderLocked(pszNewProvider);
            if (pNewProvider)
            {
                pElement->pProvider = pNewProvider;
                return pNewProvider->pfnValidate(pNewProvider, pSpec, pElement, poffError);
            }
            return VERR_VFS_CHAIN_CANNOT_BE_FIRST_ELEMENT;
        }
        return VERR_VFS_CHAIN_TAKES_DIR_OR_FSS_OR_VFS;
    }

    /*
     * Make common cause with 'stdfile' if we're opening a file or I/O stream.
     * If the input is a FSS, we have to make sure it's a read-only operation.
     */
    if (   pElement->enmType != RTVFSOBJTYPE_FILE
        && pElement->enmType != RTVFSOBJTYPE_IO_STREAM)
    {
        int rc = RTVfsChainValidateOpenFileOrIoStream(pSpec, pElement, poffError);
        if (RT_SUCCESS(rc))
        {
            if (pElement->enmTypeIn != RTVFSOBJTYPE_FS_STREAM)
                return VINF_SUCCESS;
            if (   !(pElement->uProvider & RTFILE_O_WRITE)
                &&  (pElement->uProvider & RTFILE_O_ACTION_MASK) ==  RTFILE_O_OPEN)
                return VINF_SUCCESS;
            *poffError = pElement->cArgs > 1 ? pElement->paArgs[1].offSpec : pElement->offSpec;
            return VERR_VFS_CHAIN_INVALID_ARGUMENT;
        }
    }

    /*
     * Directory checks.  Path argument only, optional. If not given the root directory of a VFS or the
     */
    if (pElement->cArgs > 1)
        return VERR_VFS_CHAIN_AT_MOST_ONE_ARG;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainOpen_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                    PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                    PRTVFSOBJ phVfsObj, uint32_t *poffError)
{
    RT_NOREF(pProviderReg, pSpec, pElement, poffError);
    AssertReturn(hPrevVfsObj != NIL_RTVFSOBJ, VERR_VFS_CHAIN_IPE);

    /*
     * File system stream: Seek thru the stream looking for the object to open.
     */
    RTVFSFSSTREAM hVfsFssIn = RTVfsObjToFsStream(hPrevVfsObj);
    if (hVfsFssIn != NIL_RTVFSFSSTREAM)
    {
        return VERR_NOT_IMPLEMENTED;
    }

    /*
     * VFS: Use RTVfsFileOpen or RTVfsDirOpen.
     */
    RTVFS hVfsIn = RTVfsObjToVfs(hPrevVfsObj);
    if (hVfsIn != NIL_RTVFS)
    {
        if (   pElement->enmType == RTVFSOBJTYPE_FILE
            || pElement->enmType == RTVFSOBJTYPE_IO_STREAM)
        {
            RTVFSFILE hVfsFile = NIL_RTVFSFILE;
            int rc = RTVfsFileOpen(hVfsIn, pElement->paArgs[0].psz, pElement->uProvider, &hVfsFile);
            if (RT_SUCCESS(rc))
            {
                *phVfsObj = RTVfsObjFromFile(hVfsFile);
                RTVfsFileRelease(hVfsFile);
                if (*phVfsObj != NIL_RTVFSOBJ)
                    return VINF_SUCCESS;
                rc = VERR_VFS_CHAIN_CAST_FAILED;
            }
            return rc;
        }
        if (pElement->enmType == RTVFSOBJTYPE_DIR)
        {
            RTVFSDIR hVfsDir = NIL_RTVFSDIR;
            int rc = RTVfsDirOpen(hVfsIn, pElement->paArgs[0].psz, (uint32_t)pElement->uProvider, &hVfsDir);
            if (RT_SUCCESS(rc))
            {
                *phVfsObj = RTVfsObjFromDir(hVfsDir);
                RTVfsDirRelease(hVfsDir);
                if (*phVfsObj != NIL_RTVFSOBJ)
                    return VINF_SUCCESS;
                rc = VERR_VFS_CHAIN_CAST_FAILED;
            }
            return rc;
        }
        return VERR_VFS_CHAIN_IPE;
    }

    /*
     * Directory: Similar to above, just relative to a directory.
     */
    RTVFSDIR hVfsDirIn = RTVfsObjToDir(hPrevVfsObj);
    if (hVfsDirIn != NIL_RTVFSDIR)
    {
        if (   pElement->enmType == RTVFSOBJTYPE_FILE
            || pElement->enmType == RTVFSOBJTYPE_IO_STREAM)
        {
            RTVFSFILE hVfsFile = NIL_RTVFSFILE;
            int rc = RTVfsDirOpenFile(hVfsDirIn, pElement->paArgs[0].psz, pElement->uProvider, &hVfsFile);
            if (RT_SUCCESS(rc))
            {
                *phVfsObj = RTVfsObjFromFile(hVfsFile);
                RTVfsFileRelease(hVfsFile);
                if (*phVfsObj != NIL_RTVFSOBJ)
                    return VINF_SUCCESS;
                rc = VERR_VFS_CHAIN_CAST_FAILED;
            }
            return rc;
        }
        if (pElement->enmType == RTVFSOBJTYPE_DIR)
        {
            RTVFSDIR hVfsDir = NIL_RTVFSDIR;
            int rc = RTVfsDirOpenDir(hVfsDirIn, pElement->paArgs[0].psz, pElement->uProvider, &hVfsDir);
            if (RT_SUCCESS(rc))
            {
                *phVfsObj = RTVfsObjFromDir(hVfsDir);
                RTVfsDirRelease(hVfsDir);
                if (*phVfsObj != NIL_RTVFSOBJ)
                    return VINF_SUCCESS;
                rc = VERR_VFS_CHAIN_CAST_FAILED;
            }
            return rc;
        }
        return VERR_VFS_CHAIN_IPE;
    }

    AssertFailed();
    return VERR_VFS_CHAIN_CAST_FAILED;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtVfsChainOpen_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                         PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                         PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pElement, pReuseSpec, pReuseElement);
    return false;
}


/** VFS chain element 'gunzip'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainGunzipReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "open",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Generic VFS open, that can open files (or I/O stream) and directories in a VFS, directory or file system stream.\n"
                                "If used as the first element in a chain, it will work like 'stdfile' or 'stddir' and work on the real file system.\n"
                                "First argument is the filename or directory path.\n"
                                "Second argument is access mode, files only, optional: r, w, rw.\n"
                                "Third argument is open disposition, files only, optional: create, create-replace, open, open-create, open-append, open-truncate.\n"
                                "Forth argument is file sharing, files only, optional: nr, nw, nrw, d.",
    /* pfnValidate = */         rtVfsChainOpen_Validate,
    /* pfnInstantiate = */      rtVfsChainOpen_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainOpen_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainGunzipReg, rtVfsChainGunzipReg);




/**
 * Initializes the globals via RTOnce.
 *
 * @returns IPRT status code
 * @param   pvUser              Unused, ignored.
 */
static DECLCALLBACK(int) rtVfsChainElementRegisterInit(void *pvUser)
{
    NOREF(pvUser);
    if (!g_rtVfsChainElementProviderList.pNext)
        RTListInit(&g_rtVfsChainElementProviderList);
    int rc = RTCritSectRwInit(&g_rtVfsChainElementCritSect);
    if (RT_SUCCESS(rc))
    {
    }
    return rc;
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
    AssertPtrReturn(pRegRec->pszName,               VERR_INVALID_POINTER);
    AssertPtrReturn(pRegRec->pfnValidate,           VERR_INVALID_POINTER);
    AssertPtrReturn(pRegRec->pfnInstantiate,        VERR_INVALID_POINTER);
    AssertPtrReturn(pRegRec->pfnCanReuseElement,    VERR_INVALID_POINTER);

    /*
     * Init and take the lock.
     */
    if (!fFromCtor)
    {
        rc = RTOnce(&g_rtVfsChainElementInitOnce, rtVfsChainElementRegisterInit, NULL);
        if (RT_FAILURE(rc))
            return rc;
        rc = RTCritSectRwEnterExcl(&g_rtVfsChainElementCritSect);
        if (RT_FAILURE(rc))
            return rc;
    }
    else if (!g_rtVfsChainElementProviderList.pNext)
        RTListInit(&g_rtVfsChainElementProviderList);

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
        RTCritSectRwLeaveExcl(&g_rtVfsChainElementCritSect);
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
        pSpec->fOpenFile      = 0;
        pSpec->fOpenDir       = 0;
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
static PRTVFSCHAINELEMSPEC rtVfsChainSpecAddElement(PRTVFSCHAINSPEC pSpec, uint16_t offSpec, int *prc)
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
    pElement->enmTypeIn   = iElement ? pSpec->paElements[iElement - 1].enmType : RTVFSOBJTYPE_INVALID;
    pElement->enmType     = RTVFSOBJTYPE_INVALID;
    pElement->offSpec     = offSpec;
    pElement->cchSpec     = 0;
    pElement->cArgs       = 0;
    pElement->paArgs      = NULL;
    pElement->pProvider   = NULL;
    pElement->hVfsObj     = NIL_RTVFSOBJ;

    pSpec->cElements = iElement + 1;
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
static int rtVfsChainSpecElementAddArg(PRTVFSCHAINELEMSPEC pElement, const char *psz, size_t cch, uint16_t offSpec)
{
    uint32_t iArg = pElement->cArgs;
    if ((iArg % 32) == 0)
    {
        PRTVFSCHAINELEMENTARG paNew = (PRTVFSCHAINELEMENTARG)RTMemTmpAlloc((iArg + 32) * sizeof(paNew[0]));
        if (!paNew)
            return VERR_NO_TMP_MEMORY;
        memcpy(paNew, pElement->paArgs, iArg * sizeof(paNew[0]));
        RTMemTmpFree(pElement->paArgs);
        pElement->paArgs = paNew;
    }

    int rc = VINF_SUCCESS;
    pElement->paArgs[iArg].psz     = rtVfsChainSpecDupStrN(psz, cch, &rc);
    pElement->paArgs[iArg].offSpec = offSpec;
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
            RTMemTmpFree(pSpec->paElements[i].paArgs[iArg].psz);
        RTMemTmpFree(pSpec->paElements[i].paArgs);
        RTMemTmpFree(pSpec->paElements[i].pszProvider);
        if (pSpec->paElements[i].hVfsObj != NIL_RTVFSOBJ)
        {
            RTVfsObjRelease(pSpec->paElements[i].hVfsObj);
            pSpec->paElements[i].hVfsObj = NIL_RTVFSOBJ;
        }
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


RTDECL(int) RTVfsChainSpecParse(const char *pszSpec, uint32_t fFlags, RTVFSOBJTYPE enmDesiredType,
                                PRTVFSCHAINSPEC *ppSpec, const char **ppszError)
{
    AssertPtrNullReturn(ppszError, VERR_INVALID_POINTER);
    if (ppszError)
        *ppszError = NULL;
    AssertPtrReturn(ppSpec, VERR_INVALID_POINTER);
    *ppSpec = NULL;
    AssertPtrReturn(pszSpec, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~RTVFSCHAIN_PF_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(enmDesiredType > RTVFSOBJTYPE_INVALID && enmDesiredType < RTVFSOBJTYPE_END, VERR_INVALID_PARAMETER);

    /*
     * Check the start of the specification and allocate an empty return spec.
     */
    if (strncmp(pszSpec, RTVFSCHAIN_SPEC_PREFIX, sizeof(RTVFSCHAIN_SPEC_PREFIX) - 1))
        return VERR_VFS_CHAIN_NO_PREFIX;
    const char *pszSrc = RTStrStripL(pszSpec + sizeof(RTVFSCHAIN_SPEC_PREFIX) - 1);
    if (!*pszSrc)
        return VERR_VFS_CHAIN_EMPTY;

    PRTVFSCHAINSPEC pSpec = rtVfsChainSpecAlloc();
    if (!pSpec)
        return VERR_NO_TMP_MEMORY;
    pSpec->enmDesiredType = enmDesiredType;

    /*
     * Parse the spec one element at a time.
     */
    int rc = VINF_SUCCESS;
    while (*pszSrc && RT_SUCCESS(rc))
    {
        /*
         * Digest element separator, except for the first element.
         */
        if (*pszSrc == '|' || *pszSrc == ':')
        {
            if (pSpec->cElements != 0)
                pszSrc = RTStrStripL(pszSrc + 1);
            else
            {
                rc = VERR_VFS_CHAIN_LEADING_SEPARATOR;
                break;
            }
        }
        else if (pSpec->cElements != 0)
        {
            rc = VERR_VFS_CHAIN_EXPECTED_SEPARATOR;
            break;
        }

        /*
         * Ok, there should be an element here so add one to the return struct.
         */
        PRTVFSCHAINELEMSPEC pElement = rtVfsChainSpecAddElement(pSpec, (uint16_t)(pszSrc - pszSpec), &rc);
        if (!pElement)
            break;

        /*
         * First up is the VFS object type followed by a parentheses,
         * or this could be the trailing action.
         */
        size_t cch;
        if (strncmp(pszSrc, "base", cch = 4) == 0)
            pElement->enmType = RTVFSOBJTYPE_BASE;
        else if (strncmp(pszSrc, "vfs",  cch = 3) == 0)
            pElement->enmType = RTVFSOBJTYPE_VFS;
        else if (strncmp(pszSrc, "fss",  cch = 3) == 0)
            pElement->enmType = RTVFSOBJTYPE_FS_STREAM;
        else if (strncmp(pszSrc, "ios",  cch = 3) == 0)
            pElement->enmType = RTVFSOBJTYPE_IO_STREAM;
        else if (strncmp(pszSrc, "dir",  cch = 3) == 0)
            pElement->enmType = RTVFSOBJTYPE_DIR;
        else if (strncmp(pszSrc, "file", cch = 4) == 0)
            pElement->enmType = RTVFSOBJTYPE_FILE;
        else if (strncmp(pszSrc, "sym",  cch = 3) == 0)
            pElement->enmType = RTVFSOBJTYPE_SYMLINK;
        else
        {
            if (*pszSrc == '\0')
                rc = VERR_VFS_CHAIN_TRAILING_SEPARATOR;
            else
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
            rc = rtVfsChainSpecElementAddArg(pElement, pszSrc, cch, (uint16_t)(pszSrc - pszSpec));
            if (RT_FAILURE(rc))
                break;
            pszSrc += cch;
        }
        if (RT_FAILURE(rc))
            break;

        /* Must end with a right parentheses. */
        if (*pszSrc != ')')
        {
            rc = VERR_VFS_CHAIN_EXPECTED_RIGHT_PARENTHESES;
            break;
        }
        pElement->cchSpec = (uint16_t)(pszSrc - pszSpec) - pElement->offSpec + 1;

        pszSrc = RTStrStripL(pszSrc + 1);
    }

#if 1
    RTAssertMsg2("dbg: cElements=%d rc=%Rrc\n", pSpec->cElements, rc);
    for (uint32_t i = 0; i < pSpec->cElements; i++)
    {
        uint32_t const cArgs = pSpec->paElements[i].cArgs;
        RTAssertMsg2("dbg: #%u: enmTypeIn=%d enmType=%d cArgs=%d",
                     i, pSpec->paElements[i].enmTypeIn, pSpec->paElements[i].enmType, cArgs);
        for (uint32_t j = 0; j < cArgs; j++)
            RTAssertMsg2(j == 0 ? (cArgs > 1 ? " [%s" : " [%s]") : j + 1 < cArgs ? ", %s" : ", %s]",
                         pSpec->paElements[i].paArgs[j].psz);
        //RTAssertMsg2(" offSpec=%d cchSpec=%d", pSpec->paElements[i].offSpec, pSpec->paElements[i].cchSpec);
        RTAssertMsg2(" spec: %.*s\n", pSpec->paElements[i].cchSpec, &pszSpec[pSpec->paElements[i].offSpec]);
    }
#endif

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


/**
 * Looks up @a pszProvider among the registered providers.
 *
 * @returns Pointer to registration record if found, NULL if not.
 * @param   pszProvider         The provider.
 */
static PCRTVFSCHAINELEMENTREG rtVfsChainFindProviderLocked(const char *pszProvider)
{
    PCRTVFSCHAINELEMENTREG pIterator;
    RTListForEach(&g_rtVfsChainElementProviderList, pIterator, RTVFSCHAINELEMENTREG, ListEntry)
    {
        if (strcmp(pIterator->pszName, pszProvider) == 0)
            return pIterator;
    }
    return NULL;
}


/**
 * Does reusable object type matching.
 *
 * @returns true if the types matches, false if not.
 * @param   pElement        The target element specification.
 * @param   pReuseElement   The source element specification.
 */
static bool rtVfsChainMatchReusableType(PRTVFSCHAINELEMSPEC pElement, PRTVFSCHAINELEMSPEC pReuseElement)
{
    if (pElement->enmType == pReuseElement->enmType)
        return true;

    /* File objects can always be cast to I/O streams.  */
    if (   pElement->enmType == RTVFSOBJTYPE_IO_STREAM
        && pReuseElement->enmType == RTVFSOBJTYPE_FILE)
        return true;

    /* I/O stream objects may be file objects. */
    if (   pElement->enmType == RTVFSOBJTYPE_FILE
        && pReuseElement->enmType == RTVFSOBJTYPE_IO_STREAM)
    {
        RTVFSFILE hVfsFile = RTVfsObjToFile(pReuseElement->hVfsObj);
        if (hVfsFile != NIL_RTVFSFILE)
        {
            RTVfsFileRelease(hVfsFile);
            return true;
        }
    }
    return false;
}


RTDECL(int) RTVfsChainSpecCheckAndSetup(PRTVFSCHAINSPEC pSpec, PCRTVFSCHAINSPEC pReuseSpec,
                                        PRTVFSOBJ phVfsObj, uint32_t *poffError)
{
    AssertPtrReturn(poffError, VERR_INVALID_POINTER);
    *poffError = 0;
    AssertPtrReturn(phVfsObj, VERR_INVALID_POINTER);
    *phVfsObj = NIL_RTVFSOBJ;
    AssertPtrReturn(pSpec, VERR_INVALID_POINTER);

    /*
     * Enter the critical section after making sure it has been initialized.
     */
    int rc = RTOnce(&g_rtVfsChainElementInitOnce, rtVfsChainElementRegisterInit, NULL);
    if (RT_SUCCESS(rc))
        rc = RTCritSectRwEnterShared(&g_rtVfsChainElementCritSect);
    if (RT_SUCCESS(rc))
    {
        /*
         * Resolve and check each element first.
         */
        for (uint32_t i = 0; i < pSpec->cElements; i++)
        {
            PRTVFSCHAINELEMSPEC const pElement = &pSpec->paElements[i];
            *poffError = pElement->offSpec;
            pElement->pProvider = rtVfsChainFindProviderLocked(pElement->pszProvider);
            if (pElement->pProvider)
            {
                rc = pElement->pProvider->pfnValidate(pElement->pProvider, pSpec, pElement, poffError);
                if (RT_SUCCESS(rc))
                    continue;
            }
            else
                rc = VERR_VFS_CHAIN_PROVIDER_NOT_FOUND;
            break;
        }

        /*
         * Check that the desired type is compatible with the last element.
         */
        if (RT_SUCCESS(rc))
        {
            if (pSpec->cElements > 0) /* paranoia */
            {
                PRTVFSCHAINELEMSPEC const pLast = &pSpec->paElements[pSpec->cElements - 1];
                if (   pLast->enmType == pSpec->enmDesiredType
                    || (   pLast->enmType == RTVFSOBJTYPE_FILE
                        && pSpec->enmDesiredType == RTVFSOBJTYPE_IO_STREAM) )
                    rc = VINF_SUCCESS;
                else
                {
                    *poffError = pLast->offSpec;
                    rc = VERR_VFS_CHAIN_FINAL_TYPE_MISMATCH;
                }
            }
            else
                rc = VERR_VFS_CHAIN_EMPTY;
        }

        if (RT_SUCCESS(rc))
        {
            /*
             * Try construct the chain.
             */
            RTVFSOBJ hPrevVfsObj = NIL_RTVFSOBJ; /* No extra reference, kept in chain structure. */
            for (uint32_t i = 0; i < pSpec->cElements; i++)
            {
                PRTVFSCHAINELEMSPEC const pElement = &pSpec->paElements[i];
                *poffError = pElement->offSpec;

                /*
                 * Try reuse the VFS objects at the start of the passed in reuse chain.
                 */
                if (!pReuseSpec)
                { /* likely */ }
                else
                {
                    if (i < pReuseSpec->cElements)
                    {
                        PRTVFSCHAINELEMSPEC const pReuseElement = &pReuseSpec->paElements[i];
                        if (pReuseElement->hVfsObj != NIL_RTVFSOBJ)
                        {
                            if (strcmp(pElement->pszProvider, pReuseElement->pszProvider) == 0)
                            {
                                if (rtVfsChainMatchReusableType(pElement, pReuseElement))
                                {
                                    if (pElement->pProvider->pfnCanReuseElement(pElement->pProvider, pSpec, pElement,
                                                                                pReuseSpec, pReuseElement))
                                    {
                                        uint32_t cRefs = RTVfsObjRetain(pReuseElement->hVfsObj);
                                        if (cRefs != UINT32_MAX)
                                        {
                                            pElement->hVfsObj = hPrevVfsObj = pReuseElement->hVfsObj;
                                            continue;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    pReuseSpec = NULL;
                }

                /*
                 * Instantiate a new VFS object.
                 */
                RTVFSOBJ hVfsObj = NIL_RTVFSOBJ;
                rc = pElement->pProvider->pfnInstantiate(pElement->pProvider, pSpec, pElement, hPrevVfsObj, &hVfsObj, poffError);
                if (RT_FAILURE(rc))
                    break;
                pElement->hVfsObj = hVfsObj;
                hPrevVfsObj = hVfsObj;
            }

            /*
             * Add another reference to the final object and return.
             */
            if (RT_SUCCESS(rc))
            {
                uint32_t cRefs = RTVfsObjRetain(hPrevVfsObj);
                AssertStmt(cRefs != UINT32_MAX, rc = VERR_VFS_CHAIN_IPE);
                *phVfsObj = hPrevVfsObj;
            }
        }

        int rc2 = RTCritSectRwLeaveShared(&g_rtVfsChainElementCritSect);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
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
        RTCritSectRwEnterExcl(&g_rtVfsChainElementCritSect);
    else if (!g_rtVfsChainElementProviderList.pNext)
        RTListInit(&g_rtVfsChainElementProviderList);

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
        RTCritSectRwLeaveExcl(&g_rtVfsChainElementCritSect);
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
        rc = RTVfsChainSpecParse(pszSpec,  0 /*fFlags*/, RTVFSOBJTYPE_FILE, &pSpec, ppszError);
        if (RT_SUCCESS(rc))
        {
            pSpec->fOpenFile = fOpen;

            uint32_t offError = 0;
            RTVFSOBJ hVfsObj  = NIL_RTVFSOBJ;
            rc = RTVfsChainSpecCheckAndSetup(pSpec, NULL /*pReuseSpec*/, &hVfsObj, &offError);
            if (RT_SUCCESS(rc))
            {
                *phVfsFile = RTVfsObjToFile(hVfsObj);
                if (*phVfsFile)
                    rc = VINF_SUCCESS;
                else
                    rc = VERR_VFS_CHAIN_CAST_FAILED;
                RTVfsObjRelease(hVfsObj);
            }
            else if (ppszError)
                *ppszError = &pszSpec[offError];

            RTVfsChainSpecFree(pSpec);
        }
    }
    return rc;
}


RTDECL(int) RTVfsChainOpenIoStream(const char *pszSpec, uint64_t fOpen, PRTVFSIOSTREAM phVfsIos, const char **ppszError)
{
    if (ppszError)
        *ppszError = NULL;

    AssertPtrReturn(pszSpec, VERR_INVALID_POINTER);
    AssertReturn(*pszSpec != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsIos, VERR_INVALID_POINTER);

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
    /*
     * Do the chain thing.
     */
    else
    {
        PRTVFSCHAINSPEC pSpec;
        rc = RTVfsChainSpecParse(pszSpec, 0 /*fFlags*/, RTVFSOBJTYPE_IO_STREAM, &pSpec, ppszError);
        if (RT_SUCCESS(rc))
        {
            pSpec->fOpenFile = fOpen;

            uint32_t offError = 0;
            RTVFSOBJ hVfsObj  = NIL_RTVFSOBJ;
            rc = RTVfsChainSpecCheckAndSetup(pSpec, NULL /*pReuseSpec*/, &hVfsObj, &offError);
            if (RT_SUCCESS(rc))
            {
                *phVfsIos = RTVfsObjToIoStream(hVfsObj);
                if (*phVfsIos)
                    rc = VINF_SUCCESS;
                else
                    rc = VERR_VFS_CHAIN_CAST_FAILED;
                RTVfsObjRelease(hVfsObj);
            }
            else if (ppszError)
                *ppszError = &pszSpec[offError];

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


