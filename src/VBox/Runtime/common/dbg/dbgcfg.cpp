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
#include <iprt/dbg.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** @name Flags for path search strings
 * @{ */
#define RTDBGCCFG_PATH_SRV      UINT16_C(0x0001)
#define RTDBGCCFG_PATH_CACHE    UINT16_C(0x0002)
/** @}*/

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

        if (enmOp != RTDBGCFGOP_REMOVE)
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
            if (fPaths)
            {
                if (!strncmp(pchPath, RT_STR_TUPLE("srv*")))
                    pNew->fFlags |= RTDBGCCFG_PATH_SRV;
                else if (!strncmp(pchPath, RT_STR_TUPLE("cache*")))
                    pNew->fFlags |= RTDBGCCFG_PATH_CACHE;
            }
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
            void           *pvAttr;
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

