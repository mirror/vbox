/* $Id$ */
/** @file
 * IPRT - Debugging Configuration.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_DBG
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * String list entry.
 */
typedef struct RTDBGCFGSTR
{
    /** List entry. */
    RTLISTNODE  ListEntry;
    /** Domain specific flags. */
    uint16_t    fFlags;
    /** The length of the string. */
    uint16_t    cch;
    /** The string. */
    char        sz[1];
} RTDBGCFGSTR;
/** Pointer to a string list entry. */
typedef RTDBGCFGSTR *PRTDBGCFGSTR;


/**
 * Log callback.
 *
 * @param   hDbgCfg         The debug config instance.
 * @param   iLevel          The message level.
 * @param   pszMsg          The message.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNRTDBGCFGLOG(RTDBGCFG hDbgCfg, uint32_t iLevel, const char *pszMsg, void *pvUser);
/** Pointer to a log callback. */
typedef FNRTDBGCFGLOG *PFNRTDBGCFGLOG;

/**
 * Configuration instance.
 */
typedef struct RTDBGCFGINT
{
    /** The magic value (RTDBGCFG_MAGIC). */
    uint32_t            u32Magic;
    /** Reference counter. */
    uint32_t volatile   cRefs;
    /** Flags, see RTDBGCFG_FLAGS_XXX. */
    uint64_t            fFlags;

    /** List of paths to search for debug files and executable images. */
    RTLISTANCHOR        PathList;
    /** List of debug file suffixes. */
    RTLISTANCHOR        SuffixList;
    /** List of paths to search for source files. */
    RTLISTANCHOR        SrcPathList;

#ifdef RT_OS_WINDOWS
    /** The _NT_ALT_SYMBOL_PATH and _NT_SYMBOL_PATH combined. */
    RTLISTANCHOR        NtSymbolPathList;
    /** The _NT_EXECUTABLE_PATH. */
    RTLISTANCHOR        NtExecutablePathList;
    /** The _NT_SOURCE_PATH. */
    RTLISTANCHOR        NtSourcePath;
#endif

    /** Log callback function. */
    PFNRTDBGCFGLOG      pfnLogCallback;
    /** User argument to pass to the log callback. */
    void               *pvLogUser;

    /** Critical section protecting the instance data. */
    RTCRITSECTRW        CritSect;
} *PRTDBGCFGINT;

/**
 * Mnemonics map entry for a 64-bit unsigned property value.
 */
typedef struct RTDBGCFGU64MNEMONIC
{
    /** The flags to set or clear. */
    uint64_t    fFlags;
    /** The mnemonic. */
    const char *pszMnemonic;
    /** The length of the mnemonic. */
    uint8_t     cchMnemonic;
    /** If @c true, the bits in fFlags will be set, if @c false they will be
     *  cleared. */
    bool        fSet;
} RTDBGCFGU64MNEMONIC;
/** Pointer to a read only mnemonic map entry for a uint64_t property. */
typedef RTDBGCFGU64MNEMONIC const *PCRTDBGCFGU64MNEMONIC;


/** @name Open flags.
 * @{ */
/** The operative system mask.  The values are RT_OPSYS_XXX. */
#define RTDBGCFG_O_OPSYS_MASK           UINT32_C(0x000000ff)
/** Whether to make a recursive search. */
#define RTDBGCFG_O_RECURSIVE            RT_BIT_32(27)
/** We're looking for a separate debug file. */
#define RTDBGCFG_O_EXT_DEBUG_FILE       RT_BIT_32(28)
/** We're looking for an executable image. */
#define RTDBGCFG_O_EXECUTABLE_IMAGE     RT_BIT_32(29)
/** The file search should be done in an case insensitive fashion. */
#define RTDBGCFG_O_CASE_INSENSITIVE     RT_BIT_32(30)
/** Use Windbg style symbol servers when encountered in the path. */
#define RTDBGCFG_O_SYMSRV               RT_BIT_32(31)
/** @} */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a debug module handle and returns rc if not valid. */
#define RTDBGCFG_VALID_RETURN_RC(pThis, rc) \
    do { \
        AssertPtrReturn((pThis), (rc)); \
        AssertReturn((pThis)->u32Magic == RTDBGCFG_MAGIC, (rc)); \
        AssertReturn((pThis)->cRefs > 0, (rc)); \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Mnemonics map for RTDBGCFGPROP_FLAGS. */
static const RTDBGCFGU64MNEMONIC g_aDbgCfgFlags[] =
{
    {   RTDBGCFG_FLAGS_DEFERRED,                RT_STR_TUPLE("deferred"),       true  },
    {   RTDBGCFG_FLAGS_DEFERRED,                RT_STR_TUPLE("nodeferred"),     false },
    {   RTDBGCFG_FLAGS_NO_SYM_SRV,              RT_STR_TUPLE("symsrv"),         false },
    {   RTDBGCFG_FLAGS_NO_SYM_SRV,              RT_STR_TUPLE("nosymsrv"),       true  },
    {   RTDBGCFG_FLAGS_NO_SYSTEM_PATHS,         RT_STR_TUPLE("syspaths"),       false },
    {   RTDBGCFG_FLAGS_NO_SYSTEM_PATHS,         RT_STR_TUPLE("nosyspaths"),     true  },
    {   RTDBGCFG_FLAGS_NO_RECURSIV_SEARCH,      RT_STR_TUPLE("rec"),            false },
    {   RTDBGCFG_FLAGS_NO_RECURSIV_SEARCH,      RT_STR_TUPLE("norec"),          true  },
    {   RTDBGCFG_FLAGS_NO_RECURSIV_SRC_SEARCH,  RT_STR_TUPLE("recsrc"),         false },
    {   RTDBGCFG_FLAGS_NO_RECURSIV_SRC_SEARCH,  RT_STR_TUPLE("norecsrc"),       true  },
    {   0,                                      NULL, 0,                        false }
};



/**
 * Runtime logging, level 1.
 *
 * @param   pThis               The debug config instance data.
 * @param   pszFormat           The message format string.
 * @param   ...                 Arguments references in the format string.
 */
static void rtDbgCfgLog1(PRTDBGCFGINT pThis, const char *pszFormat, ...)
{
    if (LogIsEnabled() || (pThis && pThis->pfnLogCallback))
    {
        va_list va;
        va_start(va, pszFormat);
        char *pszMsg = RTStrAPrintf2V(pszFormat, va);
        va_end(va);

        Log(("RTDbgCfg: %s", pszMsg));
        if (pThis && pThis->pfnLogCallback)
            pThis->pfnLogCallback(pThis, 1, pszMsg, pThis->pvLogUser);
        RTStrFree(pszMsg);
    }
}


/**
 * Runtime logging, level 2.
 *
 * @param   pThis               The debug config instance data.
 * @param   pszFormat           The message format string.
 * @param   ...                 Arguments references in the format string.
 */
static void rtDbgCfgLog2(PRTDBGCFGINT pThis, const char *pszFormat, ...)
{
    if (LogIs2Enabled() || (pThis && pThis->pfnLogCallback))
    {
        va_list va;
        va_start(va, pszFormat);
        char *pszMsg = RTStrAPrintf2V(pszFormat, va);
        va_end(va);

        Log(("RTDbgCfg: %s", pszMsg));
        if (pThis && pThis->pfnLogCallback)
            pThis->pfnLogCallback(pThis, 2, pszMsg, pThis->pvLogUser);
        RTStrFree(pszMsg);
    }
}


/**
 * Checks if the file system at the given path is case insensitive or not.
 *
 * @returns true / false
 * @param   pszPath             The path to query about.
 */
static int rtDbgCfgIsFsCaseInsensitive(const char *pszPath)
{
    RTFSPROPERTIES Props;
    int rc = RTFsQueryProperties(pszPath, &Props);
    if (RT_FAILURE(rc))
        return RT_OPSYS == RT_OPSYS_DARWIN
            || RT_OPSYS == RT_OPSYS_DOS
            || RT_OPSYS == RT_OPSYS_OS2
            || RT_OPSYS == RT_OPSYS_NT
            || RT_OPSYS == RT_OPSYS_WINDOWS;
    return !Props.fCaseSensitive;
}


/**
 * Worker that does case sensitive file/dir searching.
 *
 * @returns true / false.
 * @param   pszPath         The path buffer containing an existing directory.
 *                          RTPATH_MAX in size.  On success, this will contain
 *                          the combined path with @a pszName case correct.
 * @param   offLastComp     The offset of the last component (for chopping it
 *                          off).
 * @param   pszName         What we're looking for.
 * @param   enmType         What kind of thing we're looking for.
 */
static bool rtDbgCfgIsXxxxAndFixCaseWorker(char *pszPath, size_t offLastComp, const char *pszName,
                                           RTDIRENTRYTYPE enmType)
{
    /** @todo IPRT should generalize this so we can use host specific tricks to
     *        speed it up. */

    /* Return straight away if the name isn't case foldable. */
    if (!RTStrIsCaseFoldable(pszName))
        return false;

    /*
     * Open the directory and check each entry in it.
     */
    pszPath[offLastComp] = '\0';
    PRTDIR pDir;
    int rc = RTDirOpen(&pDir, pszPath);
    if (RT_SUCCESS(rc))
        return false;

    for (;;)
    {
        /* Read the next entry. */
        union
        {
            RTDIRENTRY  Entry;
            uint8_t     ab[_4K];
        } u;
        size_t cbBuf = sizeof(u);
        rc = RTDirRead(pDir, &u.Entry, &cbBuf);
        if (RT_FAILURE(rc))
            break;

        if (   !RTStrICmp(pszName, u.Entry.szName)
            && (   u.Entry.enmType == enmType
                || u.Entry.enmType == RTDIRENTRYTYPE_UNKNOWN
                || u.Entry.enmType == RTDIRENTRYTYPE_SYMLINK) )
        {
            pszPath[offLastComp] = '\0';
            rc = RTPathAppend(pszPath, RTPATH_MAX, u.Entry.szName);
            if (   u.Entry.enmType != enmType
                && RT_SUCCESS(rc))
                RTDirQueryUnknownType(pszPath, true /*fFollowSymlinks*/, &u.Entry.enmType);

            if (   u.Entry.enmType == enmType
                || RT_FAILURE(rc))
            {
                RTDirClose(pDir);
                if (RT_FAILURE(rc))
                {
                    pszPath[offLastComp] = '\0';
                    return false;
                }
                return true;
            }
        }
    }

    RTDirClose(pDir);
    pszPath[offLastComp] = '\0';

    return false;
}


/**
 * Appends @a pszSubDir to @a pszPath and check whether it exists and is a
 * directory.
 *
 * If @a fCaseInsensitive is set, we will do a case insensitive search for a
 * matching sub directory.
 *
 * @returns true / false
 * @param   pszPath             The path buffer containing an existing
 *                              directory.  RTPATH_MAX in size.
 * @param   pszSubDir           The sub directory to append.
 * @param   fCaseInsensitive    Whether case insensitive searching is required.
 */
static bool rtDbgCfgIsDirAndFixCase(char *pszPath, const char *pszSubDir, bool fCaseInsensitive)
{
    /* Save the length of the input path so we can restore it in the case
       insensitive branch further down. */
    size_t const cchPath = strlen(pszPath);

    /*
     * Append the sub directory and check if we got a hit.
     */
    int rc = RTPathAppend(pszPath, RTPATH_MAX, pszSubDir);
    if (RT_FAILURE(rc))
        return false;

    if (RTDirExists(pszPath))
        return true;

    /*
     * Do case insensitive lookup if requested.
     */
    if (fCaseInsensitive)
        return rtDbgCfgIsXxxxAndFixCaseWorker(pszPath, cchPath, pszSubDir, RTDIRENTRYTYPE_DIRECTORY);
    return false;
}


/**
 * Appends @a pszFilename to @a pszPath and check whether it exists and is a
 * directory.
 *
 * If @a fCaseInsensitive is set, we will do a case insensitive search for a
 * matching filename.
 *
 * @returns true / false
 * @param   pszPath             The path buffer containing an existing
 *                              directory.  RTPATH_MAX in size.
 * @param   pszFilename         The file name to append.
 * @param   fCaseInsensitive    Whether case insensitive searching is required.
 */
static bool rtDbgCfgIsFileAndFixCase(char *pszPath, const char *pszFilename, bool fCaseInsensitive)
{
    /* Save the length of the input path so we can restore it in the case
       insensitive branch further down. */
    size_t cchPath = strlen(pszPath);

    /*
     * Append the filename and check if we got a hit.
     */
    int rc = RTPathAppend(pszPath, RTPATH_MAX, pszFilename);
    if (RT_FAILURE(rc))
        return false;

    if (RTFileExists(pszPath))
        return true;

    /*
     * Do case insensitive lookup if requested.
     */
    if (fCaseInsensitive)
        return rtDbgCfgIsXxxxAndFixCaseWorker(pszPath, cchPath, pszFilename, RTDIRENTRYTYPE_FILE);
    return false;
}


static int rtDbgCfgTryOpenDir(PRTDBGCFGINT pThis, char *pszPath, PRTPATHSPLIT pSplitFn, uint32_t fFlags,
                              PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    int rcRet = VWRN_NOT_FOUND;
    int rc2;

    /* If the directory doesn't exist, just quit immediately.
       Note! Our case insensitivity doesn't extend to the search dirs themselfs,
             only to the bits under neath them. */
    if (!RTDirExists(pszPath))
    {
        rtDbgCfgLog2(pThis, "Dir does not exist: '%s'\n", pszPath);
        return rcRet;
    }

    /* Figure out whether we have to do a case sensitive search or not.
       Note! As a simplification, we don't ask for case settings in each
             directory under the user specified path, we assume the file
             systems that mounted there have compatible settings. Faster
             that way. */
    bool const fCaseInsensitive = (fFlags & RTDBGCFG_O_CASE_INSENSITIVE)
                               && rtDbgCfgIsFsCaseInsensitive(pszPath);

    size_t const cchPath = strlen(pszPath);

    /*
     * Look for the file with less and less of the original path given.
     */
    for (unsigned i = RTPATH_PROP_HAS_ROOT_SPEC(pSplitFn->fProps); i < pSplitFn->cComps; i++)
    {
        pszPath[cchPath] = '\0';

        rc2 = VINF_SUCCESS;
        for (unsigned j = i; j < pSplitFn->cComps - 1U && RT_SUCCESS(rc2); j++)
            if (!rtDbgCfgIsDirAndFixCase(pszPath, pSplitFn->apszComps[i], fCaseInsensitive))
                rc2 = VERR_FILE_NOT_FOUND;

        if (RT_SUCCESS(rc2))
        {
            if (rtDbgCfgIsFileAndFixCase(pszPath, pSplitFn->apszComps[pSplitFn->cComps - 1], fCaseInsensitive))
            {
                rtDbgCfgLog1(pThis, "Trying '%s'...\n", pszPath);
                rc2 = pfnCallback(pThis, pszPath, pvUser1, pvUser2);
                if (rc2 == VINF_CALLBACK_RETURN || rc2 == VERR_CALLBACK_RETURN)
                {
                    if (rc2 == VINF_CALLBACK_RETURN)
                        rtDbgCfgLog1(pThis, "Found '%s'.", pszPath);
                    else
                        rtDbgCfgLog1(pThis, "Error opening '%s'.\n", pszPath);
                    return rc2;
                }
                rtDbgCfgLog1(pThis, "Error %Rrc opening '%s'.\n", rc2, pszPath);
                if (RT_FAILURE(rc2) && RT_SUCCESS_NP(rcRet))
                    rcRet = rc2;
            }
        }
    }

    /*
     * Do a recursive search if requested.
     */
    if (   (fFlags & RTDBGCFG_O_RECURSIVE)
        && !(pThis->fFlags & RTDBGCFG_FLAGS_NO_RECURSIV_SEARCH) )
    {
        /** @todo Recursive searching will be done later. */
    }

    return rcRet;
}


static int rtDbgCfgTryDownloadAndOpen(PRTDBGCFGINT pThis, const char *pszServer,
                                      char *pszPath, const char *pszCacheSubDir, PRTPATHSPLIT pSplitFn,
                                      uint32_t fFlags, PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    if (pThis->fFlags & RTDBGCFG_FLAGS_NO_SYM_SRV)
        return VWRN_NOT_FOUND;


    return VERR_NOT_IMPLEMENTED;
}


static int rtDbgCfgCopyFileToCache(PRTDBGCFGINT pThis, char const *pszSrc, const char *pchCache, size_t cchCache,
                                   const char *pszCacheSubDir, PRTPATHSPLIT pSplitFn)
{
    /** @todo copy to cache */
    return VINF_SUCCESS;
}


static int rtDbgCfgTryOpenCache(PRTDBGCFGINT pThis, char *pszPath, const char *pszCacheSubDir, PRTPATHSPLIT pSplitFn,
                                uint32_t fFlags, PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    /*
     * If the cache doesn't exist, fail right away.
     */
    if (!pszCacheSubDir || !*pszCacheSubDir)
        return VWRN_NOT_FOUND;
    if (!RTDirExists(pszPath))
    {
        rtDbgCfgLog2(pThis, "Cache does not exist: '%s'\n", pszPath);
        return VWRN_NOT_FOUND;
    }

    size_t cchPath = strlen(pszPath);

    /*
     * Carefully construct the cache path with case insensitivity in mind.
     */
    bool const fCaseInsensitive = (fFlags & RTDBGCFG_O_CASE_INSENSITIVE)
                               && rtDbgCfgIsFsCaseInsensitive(pszPath);

    if (!rtDbgCfgIsDirAndFixCase(pszPath, pSplitFn->apszComps[pSplitFn->cComps - 1], fCaseInsensitive))
        return VWRN_NOT_FOUND;

    if (!rtDbgCfgIsDirAndFixCase(pszPath, pszCacheSubDir, fCaseInsensitive))
        return VWRN_NOT_FOUND;

    if (!rtDbgCfgIsFileAndFixCase(pszPath, pSplitFn->apszComps[pSplitFn->cComps - 1], fCaseInsensitive))
        return VWRN_NOT_FOUND;

    rtDbgCfgLog1(pThis, "Trying '%s'...\n", pszPath);
    int rc2 = pfnCallback(pThis, pszPath, pvUser1, pvUser2);
    if (rc2 == VINF_CALLBACK_RETURN)
        rtDbgCfgLog1(pThis, "Found '%s'.", pszPath);
    else if (rc2 == VERR_CALLBACK_RETURN)
        rtDbgCfgLog1(pThis, "Error opening '%s'.\n", pszPath);
    else
        rtDbgCfgLog1(pThis, "Error %Rrc opening '%s'.\n", rc2, pszPath);
    return rc2;
}


static int rtDbgCfgTryOpenList(PRTDBGCFGINT pThis, PRTLISTANCHOR pList, PRTPATHSPLIT pSplitFn, const char *pszCacheSubDir,
                               uint32_t fFlags, char *pszPath, PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    int rcRet = VWRN_NOT_FOUND;
    int rc2;

    const char *pchCache = NULL;
    size_t      cchCache = 0;

    PRTDBGCFGSTR pCur;
    RTListForEach(pList, pCur, RTDBGCFGSTR, ListEntry)
    {
        size_t      cchDir = pCur->cch;
        const char *pszDir = pCur->sz;
        rtDbgCfgLog2(pThis, "Path list entry: '%s'\n", pszDir);

        /* This is very simplistic, but we have a unreasonably large path
           buffer, so it'll work just fine and simplify things greatly below. */
        if (cchDir >= RTPATH_MAX - 8U)
        {
            if (RT_SUCCESS_NP(rcRet))
                rcRet = VERR_FILENAME_TOO_LONG;
            continue;
        }

        /*
         * Process the path according to it's type.
         */
        if (!strncmp(pszDir, RT_STR_TUPLE("srv*")))
        {
            /*
             * Symbol server.
             */
            pszDir += sizeof("srv*") - 1;
            cchDir -= sizeof("srv*") - 1;
            bool        fSearchCache = false;
            const char *pszServer = (const char *)memchr(pszDir, '*', cchDir);
            if (!pszServer)
                pszServer = pszDir;
            else if (pszServer == pszDir)
                continue;
            {
                fSearchCache = true;
                pchCache = pszDir;
                cchCache = pszServer - pszDir;
                pszServer++;
            }

            /* We don't have any default cache directory, so skip if the cache is missing. */
            if (cchCache == 0)
                continue;

            /* Search the cache first (if we haven't already done so). */
            if (fSearchCache)
            {
                memcpy(pszPath, pchCache, cchCache);
                pszPath[cchCache] = '\0';
                rc2 = rtDbgCfgTryOpenCache(pThis, pszPath, pszCacheSubDir, pSplitFn, fFlags,
                                           pfnCallback, pvUser1, pvUser2);
                if (rc2 == VINF_CALLBACK_RETURN || rc2 == VERR_CALLBACK_RETURN)
                    return rc2;
            }

            /* Try downloading the file. */
            memcpy(pszPath, pchCache, cchCache);
            pszPath[cchCache] = '\0';
            rc2 = rtDbgCfgTryDownloadAndOpen(pThis, pszServer, pszPath, pszCacheSubDir, pSplitFn, fFlags,
                                             pfnCallback, pvUser1, pvUser2);
            if (rc2 == VINF_CALLBACK_RETURN || rc2 == VERR_CALLBACK_RETURN)
                return rc2;
        }
        else if (!strncmp(pszDir, RT_STR_TUPLE("cache*")))
        {
            /*
             * Cache directory.
             */
            pszDir += sizeof("cache*") - 1;
            cchDir -= sizeof("cache*") - 1;
            if (!cchDir)
                continue;
            pchCache = pszDir;
            cchCache = cchDir;

            memcpy(pszPath, pchCache, cchCache);
            pszPath[cchCache] = '\0';
            rc2 = rtDbgCfgTryOpenCache(pThis, pszPath, pszCacheSubDir, pSplitFn, fFlags,
                                       pfnCallback, pvUser1, pvUser2);
            if (rc2 == VINF_CALLBACK_RETURN || rc2 == VERR_CALLBACK_RETURN)
                return rc2;
        }
        else
        {
            /*
             * Normal directory. Check for our own 'rec*' and 'norec*' prefix
             * flags governing recursive searching.
             */
            uint32_t fFlagsDir = fFlags;
            if (!strncmp(pszDir, RT_STR_TUPLE("rec*")))
            {
                pszDir += sizeof("rec*") - 1;
                cchDir -= sizeof("rec*") - 1;
                fFlagsDir |= RTDBGCFG_O_RECURSIVE;
            }
            else if (!strncmp(pszDir, RT_STR_TUPLE("norec*")))
            {
                pszDir += sizeof("norec*") - 1;
                cchDir -= sizeof("norec*") - 1;
                fFlagsDir &= ~RTDBGCFG_O_RECURSIVE;
            }

            /* Copy the path into the buffer and do the searching. */
            memcpy(pszPath, pszDir, cchDir);
            pszPath[cchDir] = '\0';

            rc2 = rtDbgCfgTryOpenDir(pThis, pszPath, pSplitFn, fFlagsDir, pfnCallback, pvUser1, pvUser2);
            if (rc2 == VINF_CALLBACK_RETURN || rc2 == VERR_CALLBACK_RETURN)
            {
                if (   rc2 == VINF_CALLBACK_RETURN
                    && cchCache > 0)
                    rtDbgCfgCopyFileToCache(pThis, pszPath, pchCache, cchCache, pszCacheSubDir, pSplitFn);
                return rc2;
            }
        }
    }

    return rcRet;
}


/**
 * Common worker routine for Image and debug info opening.
 *
 * This will not search using for suffixes.
 *
 * @returns IPRT status code.
 * @param   hDbgCfg         The debugging configuration handle.  NIL_RTDBGCFG is
 *                          accepted, but the result is that no paths will be
 *                          searched beyond the given and the current directory.
 * @param   pszFilename     The filename to search for.  This may or may not
 *                          include a full or partial path.
 * @param   pszCacheSubDir  The cache subdirectory to look in.
 * @param   fFlags          Flags and hints.
 * @param   pfnCallback     The open callback routine.
 * @param   pvUser1         User parameter 1.
 * @param   pvUser2         User parameter 2.
 */
static int rtDbgCfgOpenWithSubDir(RTDBGCFG hDbgCfg, const char *pszFilename, const char *pszCacheSubDir,
                                  uint32_t fFlags, PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    int rcRet = VINF_SUCCESS;
    int rc2;

    /*
     * Do a little validating first.
     */
    PRTDBGCFGINT pThis = hDbgCfg;
    if (pThis != NIL_RTDBGCFG)
        RTDBGCFG_VALID_RETURN_RC(pThis, VERR_INVALID_HANDLE);
    else
        pThis = NULL;
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(pszCacheSubDir, VERR_INVALID_POINTER);
    AssertPtrReturn(pfnCallback, VERR_INVALID_POINTER);

    /*
     * Do some guessing as to the way we should parse the filename and whether
     * it's case exact or not.
     */
    bool fDosPath = strchr(pszFilename, ':')  != NULL
                 || strchr(pszFilename, '\\') != NULL
                 || RT_OPSYS_USES_DOS_PATHS(fFlags & RTDBGCFG_O_OPSYS_MASK)
                 || (fFlags & RTDBGCFG_O_CASE_INSENSITIVE);
    if (fDosPath)
        fFlags |= RTDBGCFG_O_CASE_INSENSITIVE;

    rtDbgCfgLog2(pThis, "Looking for '%s' w/ cache subdir '%s' and %#x flags\n", pszFilename, pszCacheSubDir, fFlags);

    PRTPATHSPLIT pSplitFn;
    rc2 = RTPathSplitA(pszFilename, &pSplitFn, fDosPath ? RTPATH_STR_F_STYLE_DOS : RTPATH_STR_F_STYLE_UNIX);
    if (RT_FAILURE(rc2))
        return rc2;

    /*
     * Try the stored file name first if it has a kind of absolute path.
     */
    char szPath[RTPATH_MAX];
    if (RTPATH_PROP_HAS_ROOT_SPEC(pSplitFn->fProps))
    {
        rc2 = RTPathSplitReassemble(pSplitFn, RTPATH_STR_F_STYLE_HOST, szPath, sizeof(szPath));
        if (RT_SUCCESS(rc2) && RTFileExists(szPath))
            rc2 = pfnCallback(pThis, pszFilename, pvUser1, pvUser2);
    }
    if (   rc2 != VINF_CALLBACK_RETURN
        && rc2 != VERR_CALLBACK_RETURN)
    {
        /*
         * Try the current directory (will take cover relative paths
         * skipped above).
         */
        rc2 = RTPathGetCurrent(szPath, sizeof(szPath));
        if (RT_FAILURE(rc2))
            strcpy(szPath, ".");
        rc2 = rtDbgCfgTryOpenDir(pThis, szPath, pSplitFn, fFlags, pfnCallback, pvUser1, pvUser2);
        if (RT_FAILURE(rc2) && RT_SUCCESS_NP(rcRet))
            rcRet = rc2;

        if (   rc2 != VINF_CALLBACK_RETURN
            && rc2 != VERR_CALLBACK_RETURN
            && pThis)
        {
            rc2 = RTCritSectRwEnterShared(&pThis->CritSect);
            if (RT_SUCCESS(rc2))
            {
                /*
                 * Run the applicable lists.
                 */
                rc2 = rtDbgCfgTryOpenList(pThis, &pThis->PathList, pSplitFn, pszCacheSubDir, fFlags, szPath,
                                          pfnCallback, pvUser1, pvUser2);
                if (RT_FAILURE(rc2) && RT_SUCCESS_NP(rcRet))
                    rcRet = rc2;

#ifdef RT_OS_WINDOWS
                if (   rc2 != VINF_CALLBACK_RETURN
                    && rc2 != VERR_CALLBACK_RETURN
                    && (fFlags & RTDBGCFG_O_EXECUTABLE_IMAGE)
                    && !(pThis->fFlags & RTDBGCFG_FLAGS_NO_SYSTEM_PATHS) )
                {
                    rc2 = rtDbgCfgTryOpenList(pThis, &pThis->NtExecutablePathList, pSplitFn, pszCacheSubDir, fFlags, szPath,
                                              pfnCallback, pvUser1, pvUser2);
                    if (RT_FAILURE(rc2) && RT_SUCCESS_NP(rcRet))
                        rcRet = rc2;
                }

                if (   rc2 != VINF_CALLBACK_RETURN
                    && rc2 != VERR_CALLBACK_RETURN
                    && !(pThis->fFlags & RTDBGCFG_FLAGS_NO_SYSTEM_PATHS) )
                {
                    rc2 = rtDbgCfgTryOpenList(pThis, &pThis->NtSymbolPathList, pSplitFn, pszCacheSubDir, fFlags, szPath,
                                              pfnCallback, pvUser1, pvUser2);
                    if (RT_FAILURE(rc2) && RT_SUCCESS_NP(rcRet))
                        rcRet = rc2;
                }
#endif
                RTCritSectRwLeaveShared(&pThis->CritSect);
            }
            else if (RT_SUCCESS(rcRet))
                rcRet = rc2;
        }
    }

    RTPathSplitFree(pSplitFn);
    if (   rc2 == VINF_CALLBACK_RETURN
        || rc2 == VERR_CALLBACK_RETURN)
        rcRet = rc2;
    else if (RT_SUCCESS(rcRet))
        rcRet = VERR_NOT_FOUND;
    return rcRet;
}


RTDECL(int) RTDbgCfgOpenPeImage(RTDBGCFG hDbgCfg, const char *pszFilename, uint32_t cbImage, uint32_t uTimestamp,
                                PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    char szSubDir[32];
    RTStrPrintf(szSubDir, sizeof(szSubDir), "%08X%X", uTimestamp, cbImage);
    return rtDbgCfgOpenWithSubDir(hDbgCfg, pszFilename, szSubDir,
                                  RT_OPSYS_WINDOWS /* approx */ | RTDBGCFG_O_SYMSRV | RTDBGCFG_O_CASE_INSENSITIVE
                                  | RTDBGCFG_O_EXECUTABLE_IMAGE,
                                  pfnCallback, pvUser1, pvUser2);
}


RTDECL(int) RTDbgCfgOpenPdb70(RTDBGCFG hDbgCfg, const char *pszFilename, PCRTUUID pUuid, uint32_t uAge,
                              PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    char szSubDir[64];
    if (!pUuid)
        szSubDir[0] = '\0';
    else
    {
        /* Stringify the UUID and remove the dashes. */
        int rc2 = RTUuidToStr(pUuid, szSubDir, sizeof(szSubDir));
        AssertRCReturn(rc2, rc2);

        char *pszSrc = szSubDir;
        char *pszDst = szSubDir;
        char ch;
        while ((ch = *pszSrc++))
            if (ch != '-')
                *pszDst++ = ch;
            else if (RT_C_IS_LOWER(ch))
                *pszDst++ = RT_C_TO_UPPER(ch);

        RTStrPrintf(pszDst, &szSubDir[sizeof(szSubDir)] - pszDst, "%X", uAge);
    }

    return rtDbgCfgOpenWithSubDir(hDbgCfg, pszFilename, szSubDir,
                                  RT_OPSYS_WINDOWS /* approx */ | RTDBGCFG_O_SYMSRV | RTDBGCFG_O_CASE_INSENSITIVE
                                  | RTDBGCFG_O_EXT_DEBUG_FILE,
                                  pfnCallback, pvUser1, pvUser2);
}


RTDECL(int) RTDbgCfgOpenPdb20(RTDBGCFG hDbgCfg, const char *pszFilename, uint32_t cbImage, uint32_t uTimestamp, uint32_t uAge,
                              PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    /** @todo test this! */
    char szSubDir[32];
    RTStrPrintf(szSubDir, sizeof(szSubDir), "%08X%X", uTimestamp, uAge);
    return rtDbgCfgOpenWithSubDir(hDbgCfg, pszFilename, szSubDir,
                                  RT_OPSYS_WINDOWS /* approx */ | RTDBGCFG_O_SYMSRV | RTDBGCFG_O_CASE_INSENSITIVE
                                  | RTDBGCFG_O_EXT_DEBUG_FILE,
                                  pfnCallback, pvUser1, pvUser2);
}


RTDECL(int) RTDbgCfgOpenDbg(RTDBGCFG hDbgCfg, const char *pszFilename, uint32_t cbImage, uint32_t uTimestamp,
                            PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    char szSubDir[32];
    RTStrPrintf(szSubDir, sizeof(szSubDir), "%08X%X", uTimestamp, cbImage);
    return rtDbgCfgOpenWithSubDir(hDbgCfg, pszFilename, szSubDir,
                                  RT_OPSYS_WINDOWS /* approx */ | RTDBGCFG_O_SYMSRV | RTDBGCFG_O_CASE_INSENSITIVE
                                  | RTDBGCFG_O_EXT_DEBUG_FILE,
                                  pfnCallback, pvUser1, pvUser2);
}


RTDECL(int) RTDbgCfgOpenDwo(RTDBGCFG hDbgCfg, const char *pszFilename, uint32_t uCrc32,
                            PFNDBGCFGOPEN pfnCallback, void *pvUser1, void *pvUser2)
{
    char szSubDir[32];
    RTStrPrintf(szSubDir, sizeof(szSubDir), "%08x", uCrc32);
    return rtDbgCfgOpenWithSubDir(hDbgCfg, pszFilename, szSubDir,
                                  RT_OPSYS_UNKNOWN | RTDBGCFG_O_EXT_DEBUG_FILE,
                                  pfnCallback, pvUser1, pvUser2);
}




/**
 * Frees a string list.
 *
 * @param   pList               The list to free.
 */
static void rtDbgCfgFreeStrList(PRTLISTANCHOR pList)
{
    PRTDBGCFGSTR pCur;
    PRTDBGCFGSTR pNext;
    RTListForEachSafe(pList, pCur, pNext, RTDBGCFGSTR, ListEntry)
    {
        RTListNodeRemove(&pCur->ListEntry);
        RTMemFree(pCur);
    }
}


/**
 * Make changes to a string list, given a semicolon separated input string.
 *
 * @returns VINF_SUCCESS, VERR_FILENAME_TOO_LONG, VERR_NO_MEMORY
 * @param   pThis               The config instance.
 * @param   enmOp               The change operation.
 * @param   pszValue            The input strings separated by semicolon.
 * @param   fPaths              Indicates that this is a path list and that we
 *                              should look for srv and cache prefixes.
 * @param   pList               The string list anchor.
 */
static int rtDbgCfgChangeStringList(PRTDBGCFGINT pThis, RTDBGCFGOP enmOp, const char *pszValue, bool fPaths,
                                    PRTLISTANCHOR pList)
{
    if (enmOp == RTDBGCFGOP_SET)
        rtDbgCfgFreeStrList(pList);

    while (*pszValue)
    {
        /* Skip separators. */
        while (*pszValue == ';')
            pszValue++;
        if (!*pszValue)
            break;

        /* Find the end of this path. */
        const char *pchPath = pszValue++;
        char ch;
        while ((ch = *pszValue) && ch != ';')
            pszValue++;
        size_t cchPath = pszValue - pchPath;
        if (cchPath >= UINT16_MAX)
            return VERR_FILENAME_TOO_LONG;

        if (enmOp == RTDBGCFGOP_REMOVE)
        {
            /*
             * Remove all occurences.
             */
            PRTDBGCFGSTR pCur;
            PRTDBGCFGSTR pNext;
            RTListForEachSafe(pList, pCur, pNext, RTDBGCFGSTR, ListEntry)
            {
                if (   pCur->cch == cchPath
                    && !memcmp(pCur->sz, pchPath, cchPath))
                {
                    RTListNodeRemove(&pCur->ListEntry);
                    RTMemFree(pCur);
                }
            }
        }
        else
        {
            /*
             * We're adding a new one.
             */
            PRTDBGCFGSTR pNew = (PRTDBGCFGSTR)RTMemAlloc(RT_OFFSETOF(RTDBGCFGSTR, sz[cchPath + 1]));
            if (!pNew)
                return VERR_NO_MEMORY;
            pNew->cch = (uint16_t)cchPath;
            pNew->fFlags = 0;
            memcpy(pNew->sz, pchPath, cchPath);
            pNew->sz[cchPath] = '\0';

            if (enmOp == RTDBGCFGOP_PREPEND)
                RTListPrepend(pList, &pNew->ListEntry);
            else
                RTListAppend(pList, &pNew->ListEntry);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Make changes to a 64-bit value
 *
 * @returns VINF_SUCCESS, VERR_DBG_CFG_INVALID_VALUE.
 * @param   pThis               The config instance.
 * @param   enmOp               The change operation.
 * @param   pszValue            The input value.
 * @param   pszMnemonics        The mnemonics map for this value.
 * @param   puValue             The value to change.
 */
static int rtDbgCfgChangeStringU64(PRTDBGCFGINT pThis, RTDBGCFGOP enmOp, const char *pszValue,
                                   PCRTDBGCFGU64MNEMONIC paMnemonics, uint64_t *puValue)
{
    uint64_t    uNew = enmOp == RTDBGCFGOP_SET ? 0 : *puValue;

    char        ch;
    while ((ch = *pszValue))
    {
        /* skip whitespace and separators */
        while (RT_C_IS_SPACE(ch) || RT_C_IS_CNTRL(ch) || ch == ';' || ch == ':')
            ch = *++pszValue;
        if (!ch)
            break;

        if (RT_C_IS_DIGIT(ch))
        {
            uint64_t uTmp;
            int rc = RTStrToUInt64Ex(pszValue, (char **)&pszValue, 0, &uTmp);
            if (RT_FAILURE(rc) || rc == VWRN_NUMBER_TOO_BIG)
                return VERR_DBG_CFG_INVALID_VALUE;

            if (enmOp != RTDBGCFGOP_REMOVE)
                uNew |= uTmp;
            else
                uNew &= ~uTmp;
        }
        else
        {
            /* A mnemonic, find the end of it. */
            const char *pszMnemonic = pszValue - 1;
            do
                ch = *++pszValue;
            while (ch && !RT_C_IS_SPACE(ch) && !RT_C_IS_CNTRL(ch) && ch != ';' && ch != ':');
            size_t cchMnemonic = pszValue - pszMnemonic;

            /* Look it up in the map and apply it. */
            unsigned i = 0;
            while (paMnemonics[i].pszMnemonic)
            {
                if (   cchMnemonic == paMnemonics[i].cchMnemonic
                    && !memcmp(pszMnemonic, paMnemonics[i].pszMnemonic, cchMnemonic))
                {
                    if (paMnemonics[i].fSet ? enmOp != RTDBGCFGOP_REMOVE : enmOp == RTDBGCFGOP_REMOVE)
                        uNew |= paMnemonics[i].fFlags;
                    else
                        uNew &= ~paMnemonics[i].fFlags;
                   break;
                }
                i++;
            }

            if (!paMnemonics[i].pszMnemonic)
                return VERR_DBG_CFG_INVALID_VALUE;
        }
    }

    *puValue = uNew;
    return VINF_SUCCESS;
}


RTDECL(int) RTDbgCfgChangeString(RTDBGCFG hDbgCfg, RTDBGCFGPROP enmProp, RTDBGCFGOP enmOp, const char *pszValue)
{
    PRTDBGCFGINT pThis = hDbgCfg;
    RTDBGCFG_VALID_RETURN_RC(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmProp > RTDBGCFGPROP_INVALID && enmProp < RTDBGCFGPROP_END, VERR_INVALID_PARAMETER);
    AssertReturn(enmOp   > RTDBGCFGOP_INVALID   && enmOp   < RTDBGCFGOP_END,   VERR_INVALID_PARAMETER);
    if (!pszValue)
        pszValue = "";
    else
        AssertPtrReturn(pszValue, VERR_INVALID_POINTER);

    int rc = RTCritSectRwEnterExcl(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        switch (enmProp)
        {
            case RTDBGCFGPROP_FLAGS:
                rc = rtDbgCfgChangeStringU64(pThis, enmOp, pszValue, g_aDbgCfgFlags, &pThis->fFlags);
                break;
            case RTDBGCFGPROP_PATH:
                rc = rtDbgCfgChangeStringList(pThis, enmOp, pszValue, true, &pThis->PathList);
                break;
            case RTDBGCFGPROP_SUFFIXES:
                rc = rtDbgCfgChangeStringList(pThis, enmOp, pszValue, false, &pThis->SuffixList);
                break;
            case RTDBGCFGPROP_SRC_PATH:
                rc = rtDbgCfgChangeStringList(pThis, enmOp, pszValue, true, &pThis->SrcPathList);
                break;
            default:
                AssertFailed();
                rc = VERR_INTERNAL_ERROR_3;
        }

        RTCritSectRwLeaveExcl(&pThis->CritSect);
    }

    return rc;
}


RTDECL(int) RTDbgCfgChangeUInt(RTDBGCFG hDbgCfg, RTDBGCFGPROP enmProp, RTDBGCFGOP enmOp, uint64_t uValue)
{
    PRTDBGCFGINT pThis = hDbgCfg;
    RTDBGCFG_VALID_RETURN_RC(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmProp > RTDBGCFGPROP_INVALID && enmProp < RTDBGCFGPROP_END, VERR_INVALID_PARAMETER);
    AssertReturn(enmOp   > RTDBGCFGOP_INVALID   && enmOp   < RTDBGCFGOP_END,   VERR_INVALID_PARAMETER);

    int rc = RTCritSectRwEnterExcl(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        uint64_t *puValue = NULL;
        switch (enmProp)
        {
            case RTDBGCFGPROP_FLAGS:
                puValue = &pThis->fFlags;
                break;
            default:
                rc = VERR_DBG_CFG_NOT_UINT_PROP;
        }
        if (RT_SUCCESS(rc))
        {
            switch (enmOp)
            {
                case RTDBGCFGOP_SET:
                    *puValue = uValue;
                    break;
                case RTDBGCFGOP_APPEND:
                case RTDBGCFGOP_PREPEND:
                    *puValue |= uValue;
                    break;
                case RTDBGCFGOP_REMOVE:
                    *puValue &= ~uValue;
                    break;
                default:
                    AssertFailed();
                    rc = VERR_INTERNAL_ERROR_2;
            }
        }

        RTCritSectRwLeaveExcl(&pThis->CritSect);
    }

    return rc;
}


/**
 * Querys a string list as a single string (semicolon separators).
 *
 * @returns VINF_SUCCESS, VERR_BUFFER_OVERFLOW.
 * @param   pThis               The config instance.
 * @param   pList               The string list anchor.
 * @param   pszValue            The output buffer.
 * @param   cbValue             The size of the output buffer.
 */
static int rtDbgCfgQueryStringList(RTDBGCFG hDbgCfg, PRTLISTANCHOR pList,
                                   char *pszValue, size_t cbValue)
{
    /*
     * Check the length first.
     */
    size_t cbReq = 1;
    PRTDBGCFGSTR pCur;
    RTListForEach(pList, pCur, RTDBGCFGSTR, ListEntry)
        cbReq += pCur->cch + 1;
    if (cbReq > cbValue)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Construct the string list in the buffer.
     */
    char *psz = pszValue;
    RTListForEach(pList, pCur, RTDBGCFGSTR, ListEntry)
    {
        if (psz != pszValue)
            *psz++ = ';';
        memcpy(psz, pCur->sz, pCur->cch);
        psz += pCur->cch;
    }
    *psz = '\0';

    return VINF_SUCCESS;
}


/**
 * Querys the string value of a 64-bit unsigned int.
 *
 * @returns VINF_SUCCESS, VERR_BUFFER_OVERFLOW.
 * @param   pThis               The config instance.
 * @param   uValue              The value to query.
 * @param   pszMnemonics        The mnemonics map for this value.
 * @param   pszValue            The output buffer.
 * @param   cbValue             The size of the output buffer.
 */
static int rtDbgCfgQueryStringU64(RTDBGCFG hDbgCfg, uint64_t uValue, PCRTDBGCFGU64MNEMONIC paMnemonics,
                                  char *pszValue, size_t cbValue)
{
    /*
     * If no mnemonics, just return the hex value.
     */
    if (!paMnemonics || paMnemonics[0].pszMnemonic)
    {
        char szTmp[64];
        size_t cch = RTStrPrintf(szTmp, sizeof(szTmp), "%#x", uValue);
        if (cch + 1 > cbValue)
            return VERR_BUFFER_OVERFLOW;
        memcpy(pszValue, szTmp, cbValue);
        return VINF_SUCCESS;
    }

    /*
     * Check that there is sufficient buffer space first.
     */
    size_t cbReq = 1;
    for (unsigned i = 0; paMnemonics[i].pszMnemonic; i++)
        if (  paMnemonics[i].fSet
            ? (paMnemonics[i].fFlags & uValue)
            : !(paMnemonics[i].fFlags & uValue))
            cbReq += (cbReq != 1) + paMnemonics[i].cchMnemonic;
    if (cbReq > cbValue)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Construct the string.
     */
    char *psz = pszValue;
    for (unsigned i = 0; paMnemonics[i].pszMnemonic; i++)
        if (  paMnemonics[i].fSet
            ? (paMnemonics[i].fFlags & uValue)
            : !(paMnemonics[i].fFlags & uValue))
        {
            if (psz != pszValue)
                *psz++ = ' ';
            memcpy(psz, paMnemonics[i].pszMnemonic, paMnemonics[i].cchMnemonic);
            psz += paMnemonics[i].cchMnemonic;
        }
    *psz = '\0';
    return VINF_SUCCESS;
}


RTDECL(int) RTDbgCfgQueryString(RTDBGCFG hDbgCfg, RTDBGCFGPROP enmProp, char *pszValue, size_t cbValue)
{
    PRTDBGCFGINT pThis = hDbgCfg;
    RTDBGCFG_VALID_RETURN_RC(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmProp > RTDBGCFGPROP_INVALID && enmProp < RTDBGCFGPROP_END, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszValue, VERR_INVALID_POINTER);

    int rc = RTCritSectRwEnterShared(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        switch (enmProp)
        {
            case RTDBGCFGPROP_FLAGS:
                rc = rtDbgCfgQueryStringU64(pThis, pThis->fFlags, g_aDbgCfgFlags, pszValue, cbValue);
                break;
            case RTDBGCFGPROP_PATH:
                rc = rtDbgCfgQueryStringList(pThis, &pThis->PathList, pszValue, cbValue);
                break;
            case RTDBGCFGPROP_SUFFIXES:
                rc = rtDbgCfgQueryStringList(pThis, &pThis->SuffixList, pszValue, cbValue);
                break;
            case RTDBGCFGPROP_SRC_PATH:
                rc = rtDbgCfgQueryStringList(pThis, &pThis->SrcPathList, pszValue, cbValue);
                break;
            default:
                AssertFailed();
                rc = VERR_INTERNAL_ERROR_3;
        }

        RTCritSectRwLeaveShared(&pThis->CritSect);
    }

    return rc;
}


RTDECL(int) RTDbgCfgQueryUInt(RTDBGCFG hDbgCfg, RTDBGCFGPROP enmProp, uint64_t *puValue)
{
    PRTDBGCFGINT pThis = hDbgCfg;
    RTDBGCFG_VALID_RETURN_RC(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmProp > RTDBGCFGPROP_INVALID && enmProp < RTDBGCFGPROP_END, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puValue, VERR_INVALID_POINTER);

    int rc = RTCritSectRwEnterShared(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        switch (enmProp)
        {
            case RTDBGCFGPROP_FLAGS:
                *puValue = pThis->fFlags;
                break;
            default:
                rc = VERR_DBG_CFG_NOT_UINT_PROP;
        }

        RTCritSectRwLeaveShared(&pThis->CritSect);
    }

    return rc;
}

RTDECL(uint32_t) RTDbgCfgRetain(RTDBGCFG hDbgCfg)
{
    PRTDBGCFGINT pThis = hDbgCfg;
    RTDBGCFG_VALID_RETURN_RC(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    return cRefs;
}


RTDECL(uint32_t) RTDbgCfgRelease(RTDBGCFG hDbgCfg)
{
    if (hDbgCfg == NIL_RTDBGCFG)
        return 0;

    PRTDBGCFGINT pThis = hDbgCfg;
    RTDBGCFG_VALID_RETURN_RC(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (!cRefs)
    {
        /*
         * Last reference - free all memory.
         */
        ASMAtomicWriteU32(&pThis->u32Magic, ~RTDBGCFG_MAGIC);
        rtDbgCfgFreeStrList(&pThis->PathList);
        rtDbgCfgFreeStrList(&pThis->SuffixList);
        rtDbgCfgFreeStrList(&pThis->SrcPathList);
#ifdef RT_OS_WINDOWS
        rtDbgCfgFreeStrList(&pThis->NtSymbolPathList);
        rtDbgCfgFreeStrList(&pThis->NtExecutablePathList);
        rtDbgCfgFreeStrList(&pThis->NtSourcePath);
#endif
        RTCritSectRwDelete(&pThis->CritSect);
        RTMemFree(pThis);
    }
    else
        Assert(cRefs < UINT32_MAX / 2);
    return cRefs;
}


RTDECL(int) RTDbgCfgCreate(PRTDBGCFG phDbgCfg, const char *pszEnvVarPrefix)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(phDbgCfg, VERR_INVALID_POINTER);
    if (pszEnvVarPrefix)
    {
        AssertPtrReturn(pszEnvVarPrefix, VERR_INVALID_POINTER);
        AssertReturn(*pszEnvVarPrefix, VERR_INVALID_PARAMETER);
    }

    /*
     * Allocate and initialize a new instance.
     */
    PRTDBGCFGINT pThis = (PRTDBGCFGINT)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic   = RTDBGCFG_MAGIC;
    pThis->cRefs      = 1;
    RTListInit(&pThis->PathList);
    RTListInit(&pThis->SuffixList);
    RTListInit(&pThis->SrcPathList);
#ifdef RT_OS_WINDOWS
    RTListInit(&pThis->NtSymbolPathList);
    RTListInit(&pThis->NtExecutablePathList);
    RTListInit(&pThis->NtSourcePath);
#endif

    int rc = RTCritSectRwInit(&pThis->CritSect);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pThis);
        return rc;
    }

    /*
     * Read configurtion from the environment if requested to do so.
     */
    if (pszEnvVarPrefix)
    {
        static struct
        {
            RTDBGCFGPROP    enmProp;
            const char     *pszVar;
        } const s_aProps[] =
        {
            { RTDBGCFGPROP_FLAGS,       "FLAGS"    },
            { RTDBGCFGPROP_PATH,        "PATH"     },
            { RTDBGCFGPROP_SUFFIXES,    "SUFFIXES" },
            { RTDBGCFGPROP_SRC_PATH,    "SRC_PATH" },
        };
        const size_t cbEnvVar = 256;
        const size_t cbEnvVal = 65536 - cbEnvVar;
        char        *pszEnvVar = (char *)RTMemTmpAlloc(cbEnvVar + cbEnvVal);
        if (pszEnvVar)
        {
            char *pszEnvVal = pszEnvVar + cbEnvVar;
            for (unsigned i = 0; i < RT_ELEMENTS(s_aProps); i++)
            {
                size_t cchEnvVar = RTStrPrintf(pszEnvVar, cbEnvVar, "%s_%s", pszEnvVarPrefix, s_aProps[i].pszVar);
                if (cchEnvVar >= cbEnvVar - 1)
                {
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }

                rc = RTEnvGetEx(RTENV_DEFAULT, pszEnvVar, pszEnvVal, cbEnvVal, NULL);
                if (RT_SUCCESS(rc))
                {
                    rc = RTDbgCfgChangeString(pThis, s_aProps[i].enmProp, RTDBGCFGOP_SET, pszEnvVal);
                    if (RT_FAILURE(rc))
                        break;
                }
                else if (rc != VERR_ENV_VAR_NOT_FOUND)
                    break;
                else
                    rc = VINF_SUCCESS;
            }

            /*
             * Pick up system specific search paths.
             */
            if (RT_SUCCESS(rc))
            {
                struct
                {
                    PRTLISTANCHOR   pList;
                    const char     *pszVar;
                    char            chSep;
                } aNativePaths[] =
                {
#ifdef RT_OS_WINDOWS
                    { &pThis->NtExecutablePathList, "_NT_EXECUTABLE_PATH",  ';' },
                    { &pThis->NtSymbolPathList,     "_NT_ALT_SYMBOL_PATH",  ';' },
                    { &pThis->NtSymbolPathList,     "_NT_SYMBOL_PATH",      ';' },
                    { &pThis->NtSourcePath,         "_NT_SOURCE_PATH",      ';' },
#endif
                    { NULL, NULL, 0 }
                };
                for (unsigned i = 0; aNativePaths[i].pList; i++)
                {
                    Assert(aNativePaths[i].chSep == ';'); /* fix when needed */
                    rc = RTEnvGetEx(RTENV_DEFAULT, aNativePaths[i].pszVar, pszEnvVal, cbEnvVal, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rtDbgCfgChangeStringList(pThis, RTDBGCFGOP_APPEND, pszEnvVal, true, aNativePaths[i].pList);
                        if (RT_FAILURE(rc))
                            break;
                    }
                    else if (rc != VERR_ENV_VAR_NOT_FOUND)
                        break;
                    else
                        rc = VINF_SUCCESS;
                }
            }
            RTMemTmpFree(pszEnvVar);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
        if (RT_FAILURE(rc))
        {
            /*
             * Error, bail out.
             */
            RTDbgCfgRelease(pThis);
            return rc;
        }
    }

    /*
     * Returns successfully.
     */
    *phDbgCfg = pThis;

    return VINF_SUCCESS;
}

