/* $Id$ */
/** @file
 * IPRT - Environment, Generic.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <iprt/env.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/string.h>
#include <iprt/sort.h>
#include <iprt/err.h>
#include "internal/magics.h"

#include <stdlib.h>
#if !defined(RT_OS_WINDOWS)
# include <unistd.h>
#endif
#ifdef RT_OS_DARWIN
# include <crt_externs.h>
#endif
#if defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_NETBSD) || defined(RT_OS_OPENBSD)
RT_C_DECLS_BEGIN
extern char **environ;
RT_C_DECLS_END
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The allocation granularity of the RTENVINTERNAL::papszEnv memory. */
#define RTENV_GROW_SIZE     16

/** Macro that unlocks the specified environment block. */
#define RTENV_LOCK(pEnvInt)     do { } while (0)
/** Macro that unlocks the specified environment block. */
#define RTENV_UNLOCK(pEnvInt)   do { } while (0)

/** @def RTENV_HAVE_WENVIRON
 * Indicates that we have a _wenviron variable with UTF-16 strings that we
 * better use instead of the current-cp strings in environ. */
#if defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
# define RTENV_HAVE_WENVIRON 1
#endif

/** @def RTENV_IMPLEMENTS_UTF8_DEFAULT_ENV_API
 * Indicates the RTEnv*Utf8 APIs are implemented. */
#if defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
# define RTENV_IMPLEMENTS_UTF8_DEFAULT_ENV_API 1
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The internal representation of a (non-default) environment.
 */
typedef struct RTENVINTERNAL
{
    /** Magic value . */
    uint32_t    u32Magic;
    /** Number of variables in the array.
     * This does not include the terminating NULL entry. */
    size_t      cVars;
    /** Capacity (allocated size) of the array.
     * This includes space for the terminating NULL element (for compatibility
     * with the C library), so that c <= cCapacity - 1. */
    size_t      cAllocated;
    /** Array of environment variables. */
    char      **papszEnv;
    /** Array of environment variables in the process CP.
     * This get (re-)constructed when RTEnvGetExecEnvP method is called. */
    char      **papszEnvOtherCP;

    /** The compare function we're using. */
    DECLCALLBACKMEMBER(int, pfnCompare)(const char *psz1, const char *psz2, size_t cchMax);
} RTENVINTERNAL, *PRTENVINTERNAL;


/**
 * Internal worker that resolves the pointer to the default
 * process environment. (environ)
 *
 * @returns Pointer to the default environment.
 *          This may be NULL.
 */
static const char * const *rtEnvDefault(void)
{
#ifdef RT_OS_DARWIN
    return *(_NSGetEnviron());
#else
    return environ;
#endif
}


/**
 * Internal worker that creates an environment handle with a specified capacity.
 *
 * @returns IPRT status code.
 * @param   ppIntEnv        Where to store the result.
 * @param   cAllocated      The initial array size.
 * @param   fCaseSensitive  Whether the environment block is case sensitive or
 *                          not.
 */
static int rtEnvCreate(PRTENVINTERNAL *ppIntEnv, size_t cAllocated, bool fCaseSensitive)
{
    /*
     * Allocate environment handle.
     */
    PRTENVINTERNAL pIntEnv = (PRTENVINTERNAL)RTMemAlloc(sizeof(*pIntEnv));
    if (pIntEnv)
    {
        /*
         * Pre-allocate the variable array.
         */
        pIntEnv->u32Magic = RTENV_MAGIC;
        pIntEnv->pfnCompare = fCaseSensitive ? RTStrNCmp : RTStrNICmp;
        pIntEnv->papszEnvOtherCP = NULL;
        pIntEnv->cVars = 0;
        pIntEnv->cAllocated = RT_ALIGN_Z(RT_MAX(cAllocated, RTENV_GROW_SIZE), RTENV_GROW_SIZE);
        pIntEnv->papszEnv = (char **)RTMemAllocZ(sizeof(pIntEnv->papszEnv[0]) * pIntEnv->cAllocated);
        if (pIntEnv->papszEnv)
        {
            *ppIntEnv = pIntEnv;
            return VINF_SUCCESS;
        }

        RTMemFree(pIntEnv);
    }

    return VERR_NO_MEMORY;
}


RTDECL(int) RTEnvCreate(PRTENV pEnv)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    return rtEnvCreate(pEnv, RTENV_GROW_SIZE, false /*fCaseSensitive*/);
}
RT_EXPORT_SYMBOL(RTEnvCreate);


RTDECL(int) RTEnvDestroy(RTENV Env)
{
    /*
     * Ignore NIL_RTENV and validate input.
     */
    if (    Env == NIL_RTENV
        ||  Env == RTENV_DEFAULT)
        return VINF_SUCCESS;

    PRTENVINTERNAL pIntEnv = Env;
    AssertPtrReturn(pIntEnv, VERR_INVALID_HANDLE);
    AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the cleanup.
     */
    RTENV_LOCK(pIntEnv);
    pIntEnv->u32Magic++;
    size_t iVar = pIntEnv->cVars;
    while (iVar-- > 0)
        RTStrFree(pIntEnv->papszEnv[iVar]);
    RTMemFree(pIntEnv->papszEnv);
    pIntEnv->papszEnv = NULL;

    if (pIntEnv->papszEnvOtherCP)
    {
        for (iVar = 0; pIntEnv->papszEnvOtherCP[iVar]; iVar++)
        {
            RTStrFree(pIntEnv->papszEnvOtherCP[iVar]);
            pIntEnv->papszEnvOtherCP[iVar] = NULL;
        }
        RTMemFree(pIntEnv->papszEnvOtherCP);
        pIntEnv->papszEnvOtherCP = NULL;
    }

    RTENV_UNLOCK(pIntEnv);
    /*RTCritSectDelete(&pIntEnv->CritSect) */
    RTMemFree(pIntEnv);

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTEnvDestroy);


RTDECL(int) RTEnvClone(PRTENV pEnv, RTENV EnvToClone)
{
    /*
     * Validate input and figure out how many variable to clone and where to get them.
     */
    bool fCaseSensitive = true;
    size_t cVars;
    const char * const *papszEnv;
#ifdef RTENV_HAVE_WENVIRON
    PCRTUTF16 const * papwszEnv;
#endif
    PRTENVINTERNAL pIntEnvToClone;
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    if (EnvToClone == RTENV_DEFAULT)
    {
        cVars = 0;
        pIntEnvToClone = NULL;
#ifdef RTENV_HAVE_WENVIRON
        papszEnv  = NULL;
        papwszEnv = (PCRTUTF16 * const)_wenviron;
        if (!papwszEnv)
        {
            _wgetenv(L"Path"); /* Force the CRT to initalize it. */
            papwszEnv = (PCRTUTF16 * const)_wenviron;
        }
        if (papwszEnv)
            while (papwszEnv[cVars])
                cVars++;
#else
        papszEnv = rtEnvDefault();
        if (papszEnv)
            while (papszEnv[cVars])
                cVars++;
#endif

#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
        /* DOS systems was case insensitive.  A prime example is the 'Path'
           variable on windows which turns into the 'PATH' variable. */
        fCaseSensitive = false;
#endif
    }
    else
    {
        pIntEnvToClone = EnvToClone;
        AssertPtrReturn(pIntEnvToClone, VERR_INVALID_HANDLE);
        AssertReturn(pIntEnvToClone->u32Magic == RTENV_MAGIC, VERR_INVALID_HANDLE);
        RTENV_LOCK(pIntEnvToClone);

        papszEnv = pIntEnvToClone->papszEnv;
        cVars = pIntEnvToClone->cVars;
    }

    /*
     * Create the duplicate.
     */
    PRTENVINTERNAL pIntEnv;
    int rc = rtEnvCreate(&pIntEnv, cVars + 1 /* NULL */, fCaseSensitive);
    if (RT_SUCCESS(rc))
    {
        pIntEnv->cVars = cVars;
        pIntEnv->papszEnv[pIntEnv->cVars] = NULL;
        if (EnvToClone == RTENV_DEFAULT)
        {
            /* ASSUMES the default environment is in the current codepage. */
            size_t  iDst = 0;
            for (size_t iSrc = 0; iSrc < cVars; iSrc++)
            {
#ifdef RTENV_HAVE_WENVIRON
                int rc2 = RTUtf16ToUtf8(papwszEnv[iSrc], &pIntEnv->papszEnv[iDst]);
#else
                int rc2 = RTStrCurrentCPToUtf8(&pIntEnv->papszEnv[iDst], papszEnv[iSrc]);
#endif
                if (RT_SUCCESS(rc2))
                    iDst++;
                else if (rc2 == VERR_NO_TRANSLATION)
                    rc = VWRN_ENV_NOT_FULLY_TRANSLATED;
                else
                {
                    pIntEnv->cVars = iDst;
                    RTEnvDestroy(pIntEnv);
                    return rc2;
                }
            }
            pIntEnv->cVars = iDst;
        }
        else
        {
            for (size_t iVar = 0; iVar < cVars; iVar++)
            {
                char *pszVar = RTStrDup(papszEnv[iVar]);
                if (RT_UNLIKELY(!pszVar))
                {
                    RTENV_UNLOCK(pIntEnvToClone);

                    pIntEnv->cVars = iVar;
                    RTEnvDestroy(pIntEnv);
                    return VERR_NO_STR_MEMORY;
                }
                pIntEnv->papszEnv[iVar] = pszVar;
            }
        }

        /* done */
        *pEnv = pIntEnv;
    }

    if (pIntEnvToClone)
        RTENV_UNLOCK(pIntEnvToClone);
    return rc;
}
RT_EXPORT_SYMBOL(RTEnvClone);


RTDECL(int) RTEnvPutEx(RTENV Env, const char *pszVarEqualValue)
{
    int rc;
    AssertPtrReturn(pszVarEqualValue, VERR_INVALID_POINTER);
    const char *pszEq = strchr(pszVarEqualValue, '=');
    if (!pszEq)
        rc = RTEnvUnsetEx(Env, pszVarEqualValue);
    else
    {
        /*
         * Make a copy of the variable name so we can terminate it
         * properly and then pass the request on to RTEnvSetEx.
         */
        const char *pszValue = pszEq + 1;

        size_t cchVar = pszEq - pszVarEqualValue;
        Assert(cchVar < 1024);
        char *pszVar = (char *)alloca(cchVar + 1);
        memcpy(pszVar, pszVarEqualValue, cchVar);
        pszVar[cchVar] = '\0';

        rc = RTEnvSetEx(Env, pszVar, pszValue);
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTEnvPutEx);


RTDECL(int) RTEnvSetEx(RTENV Env, const char *pszVar, const char *pszValue)
{
    AssertPtrReturn(pszVar, VERR_INVALID_POINTER);
    AssertReturn(*pszVar, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszValue, VERR_INVALID_POINTER);

    int rc;
    if (Env == RTENV_DEFAULT)
    {
#ifdef RT_OS_WINDOWS
        rc = RTEnvSetUtf8(pszVar, pszValue);
#else
        /*
         * Since RTEnvPut isn't UTF-8 clean and actually expects the strings
         * to be in the current code page (codeset), we'll do the necessary
         * conversions here.
         */
        char *pszVarOtherCP;
        rc = RTStrUtf8ToCurrentCP(&pszVarOtherCP, pszVar);
        if (RT_SUCCESS(rc))
        {
            char *pszValueOtherCP;
            rc = RTStrUtf8ToCurrentCP(&pszValueOtherCP, pszValue);
            if (RT_SUCCESS(rc))
            {
                rc = RTEnvSet(pszVarOtherCP, pszValueOtherCP);
                RTStrFree(pszValueOtherCP);
            }
            RTStrFree(pszVarOtherCP);
        }
#endif
    }
    else
    {
        PRTENVINTERNAL pIntEnv = Env;
        AssertPtrReturn(pIntEnv, VERR_INVALID_HANDLE);
        AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, VERR_INVALID_HANDLE);

        /*
         * Create the variable string.
         */
        const size_t cchVar = strlen(pszVar);
        const size_t cchValue = strlen(pszValue);
        char *pszEntry = (char *)RTMemAlloc(cchVar + cchValue + 2);
        if (pszEntry)
        {
            memcpy(pszEntry, pszVar, cchVar);
            pszEntry[cchVar] = '=';
            memcpy(&pszEntry[cchVar + 1], pszValue, cchValue + 1);

            RTENV_LOCK(pIntEnv);

            /*
             * Find the location of the variable. (iVar = cVars if new)
             */
            rc = VINF_SUCCESS;
            size_t iVar;
            for (iVar = 0; iVar < pIntEnv->cVars; iVar++)
                if (    !pIntEnv->pfnCompare(pIntEnv->papszEnv[iVar], pszVar, cchVar)
                    &&  pIntEnv->papszEnv[iVar][cchVar] == '=')
                    break;
            if (iVar < pIntEnv->cVars)
            {
                /*
                 * Replace the current entry. Simple.
                 */
                RTMemFree(pIntEnv->papszEnv[iVar]);
                pIntEnv->papszEnv[iVar] = pszEntry;
            }
            else
            {
                /*
                 * Adding a new variable. Resize the array if required
                 * and then insert the new value at the end.
                 */
                if (pIntEnv->cVars + 2 > pIntEnv->cAllocated)
                {
                    void *pvNew = RTMemRealloc(pIntEnv->papszEnv, sizeof(char *) * (pIntEnv->cAllocated + RTENV_GROW_SIZE));
                    if (!pvNew)
                        rc = VERR_NO_MEMORY;
                    else
                    {
                        pIntEnv->papszEnv = (char **)pvNew;
                        pIntEnv->cAllocated += RTENV_GROW_SIZE;
                        for (size_t iNewVar = pIntEnv->cVars; iNewVar < pIntEnv->cAllocated; iNewVar++)
                            pIntEnv->papszEnv[iNewVar] = NULL;
                    }
                }
                if (RT_SUCCESS(rc))
                {
                    pIntEnv->papszEnv[iVar] = pszEntry;
                    pIntEnv->papszEnv[iVar + 1] = NULL; /* this isn't really necessary, but doesn't hurt. */
                    pIntEnv->cVars++;
                    Assert(pIntEnv->cVars == iVar + 1);
                }
            }

            RTENV_UNLOCK(pIntEnv);

            if (RT_FAILURE(rc))
                RTMemFree(pszEntry);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTEnvSetEx);


RTDECL(int) RTEnvUnsetEx(RTENV Env, const char *pszVar)
{
    AssertPtrReturn(pszVar, VERR_INVALID_POINTER);
    AssertReturn(*pszVar, VERR_INVALID_PARAMETER);

    int rc;
    if (Env == RTENV_DEFAULT)
    {
#ifdef RTENV_IMPLEMENTS_UTF8_DEFAULT_ENV_API
        rc = RTEnvUnsetUtf8(pszVar);
#else
        /*
         * Since RTEnvUnset isn't UTF-8 clean and actually expects the strings
         * to be in the current code page (codeset), we'll do the necessary
         * conversions here.
         */
        char *pszVarOtherCP;
        rc = RTStrUtf8ToCurrentCP(&pszVarOtherCP, pszVar);
        if (RT_SUCCESS(rc))
        {
            rc = RTEnvUnset(pszVarOtherCP);
            RTStrFree(pszVarOtherCP);
        }
#endif
    }
    else
    {
        PRTENVINTERNAL pIntEnv = Env;
        AssertPtrReturn(pIntEnv, VERR_INVALID_HANDLE);
        AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, VERR_INVALID_HANDLE);

        RTENV_LOCK(pIntEnv);

        /*
         * Remove all variable by the given name.
         */
        rc = VINF_ENV_VAR_NOT_FOUND;
        const size_t cchVar = strlen(pszVar);
        size_t iVar;
        for (iVar = 0; iVar < pIntEnv->cVars; iVar++)
            if (    !pIntEnv->pfnCompare(pIntEnv->papszEnv[iVar], pszVar, cchVar)
                &&  pIntEnv->papszEnv[iVar][cchVar] == '=')
            {
                RTMemFree(pIntEnv->papszEnv[iVar]);
                pIntEnv->cVars--;
                if (pIntEnv->cVars > 0)
                    pIntEnv->papszEnv[iVar] = pIntEnv->papszEnv[pIntEnv->cVars];
                pIntEnv->papszEnv[pIntEnv->cVars] = NULL;
                rc = VINF_SUCCESS;
                /* no break, there could be more. */
            }

        RTENV_UNLOCK(pIntEnv);
    }
    return rc;

}
RT_EXPORT_SYMBOL(RTEnvUnsetEx);


RTDECL(int) RTEnvGetEx(RTENV Env, const char *pszVar, char *pszValue, size_t cbValue, size_t *pcchActual)
{
    AssertPtrReturn(pszVar, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszValue, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pcchActual, VERR_INVALID_POINTER);
    AssertReturn(pcchActual || (pszValue && cbValue), VERR_INVALID_PARAMETER);

    if (pcchActual)
        *pcchActual = 0;
    int rc;
    if (Env == RTENV_DEFAULT)
    {
#ifdef RTENV_IMPLEMENTS_UTF8_DEFAULT_ENV_API
        rc = RTEnvGetUtf8(pszVar, pszValue, cbValue, pcchActual);
#else
        /*
         * Since RTEnvGet isn't UTF-8 clean and actually expects the strings
         * to be in the current code page (codeset), we'll do the necessary
         * conversions here.
         */
        char *pszVarOtherCP;
        rc = RTStrUtf8ToCurrentCP(&pszVarOtherCP, pszVar);
        if (RT_SUCCESS(rc))
        {
            const char *pszValueOtherCP = RTEnvGet(pszVarOtherCP);
            RTStrFree(pszVarOtherCP);
            if (pszValueOtherCP)
            {
                char *pszValueUtf8;
                rc = RTStrCurrentCPToUtf8(&pszValueUtf8, pszValueOtherCP);
                if (RT_SUCCESS(rc))
                {
                    rc = VINF_SUCCESS;
                    size_t cch = strlen(pszValueUtf8);
                    if (pcchActual)
                        *pcchActual = cch;
                    if (pszValue && cbValue)
                    {
                        if (cch < cbValue)
                            memcpy(pszValue, pszValueUtf8, cch + 1);
                        else
                            rc = VERR_BUFFER_OVERFLOW;
                    }
                    RTStrFree(pszValueUtf8);
                }
            }
            else
                rc = VERR_ENV_VAR_NOT_FOUND;
        }
#endif
    }
    else
    {
        PRTENVINTERNAL pIntEnv = Env;
        AssertPtrReturn(pIntEnv, VERR_INVALID_HANDLE);
        AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, VERR_INVALID_HANDLE);

        RTENV_LOCK(pIntEnv);

        /*
         * Locate the first variable and return it to the caller.
         */
        rc = VERR_ENV_VAR_NOT_FOUND;
        const size_t cchVar = strlen(pszVar);
        size_t iVar;
        for (iVar = 0; iVar < pIntEnv->cVars; iVar++)
            if (    !pIntEnv->pfnCompare(pIntEnv->papszEnv[iVar], pszVar, cchVar)
                &&  pIntEnv->papszEnv[iVar][cchVar] == '=')
            {
                rc = VINF_SUCCESS;
                const char *pszValueOrg = pIntEnv->papszEnv[iVar] + cchVar + 1;
                size_t cch = strlen(pszValueOrg);
                if (pcchActual)
                    *pcchActual = cch;
                if (pszValue && cbValue)
                {
                    if (cch < cbValue)
                        memcpy(pszValue, pszValueOrg, cch + 1);
                    else
                        rc = VERR_BUFFER_OVERFLOW;
                }
                break;
            }

        RTENV_UNLOCK(pIntEnv);
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTEnvGetEx);


RTDECL(bool) RTEnvExistEx(RTENV Env, const char *pszVar)
{
    AssertPtrReturn(pszVar, false);

    bool fExists = false;
    if (Env == RTENV_DEFAULT)
    {
#ifdef RTENV_IMPLEMENTS_UTF8_DEFAULT_ENV_API
        fExists = RTEnvExistsUtf8(pszVar);
#else
        /*
         * Since RTEnvExist isn't UTF-8 clean and actually expects the strings
         * to be in the current code page (codeset), we'll do the necessary
         * conversions here.
         */
        char *pszVarOtherCP;
        int rc = RTStrUtf8ToCurrentCP(&pszVarOtherCP, pszVar);
        if (RT_SUCCESS(rc))
        {
            fExists = RTEnvExist(pszVarOtherCP);
            RTStrFree(pszVarOtherCP);
        }
#endif
    }
    else
    {
        PRTENVINTERNAL pIntEnv = Env;
        AssertPtrReturn(pIntEnv, false);
        AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, false);

        RTENV_LOCK(pIntEnv);

        /*
         * Simple search.
         */
        const size_t cchVar = strlen(pszVar);
        for (size_t iVar = 0; iVar < pIntEnv->cVars; iVar++)
            if (    !pIntEnv->pfnCompare(pIntEnv->papszEnv[iVar], pszVar, cchVar)
                &&  pIntEnv->papszEnv[iVar][cchVar] == '=')
            {
                fExists = true;
                break;
            }

        RTENV_UNLOCK(pIntEnv);
    }
    return fExists;
}
RT_EXPORT_SYMBOL(RTEnvExistEx);


RTDECL(char const * const *) RTEnvGetExecEnvP(RTENV Env)
{
    const char * const *papszRet;
    if (Env == RTENV_DEFAULT)
    {
        /** @todo fix this API it's fundamentally wrong! */
        papszRet = rtEnvDefault();
        if (!papszRet)
        {
            static const char * const s_papszDummy[2] = { NULL, NULL };
            papszRet = &s_papszDummy[0];
        }
    }
    else
    {
        PRTENVINTERNAL pIntEnv = Env;
        AssertPtrReturn(pIntEnv, NULL);
        AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, NULL);

        RTENV_LOCK(pIntEnv);

        /*
         * Free any old envp.
         */
        if (pIntEnv->papszEnvOtherCP)
        {
            for (size_t iVar = 0; pIntEnv->papszEnvOtherCP[iVar]; iVar++)
            {
                RTStrFree(pIntEnv->papszEnvOtherCP[iVar]);
                pIntEnv->papszEnvOtherCP[iVar] = NULL;
            }
            RTMemFree(pIntEnv->papszEnvOtherCP);
            pIntEnv->papszEnvOtherCP = NULL;
        }

        /*
         * Construct a new envp with the strings in the process code set.
         */
        char **papsz;
        papszRet = pIntEnv->papszEnvOtherCP = papsz = (char **)RTMemAlloc(sizeof(char *) * (pIntEnv->cVars + 1));
        if (papsz)
        {
            papsz[pIntEnv->cVars] = NULL;
            for (size_t iVar = 0; iVar < pIntEnv->cVars; iVar++)
            {
                int rc = RTStrUtf8ToCurrentCP(&papsz[iVar], pIntEnv->papszEnv[iVar]);
                if (RT_FAILURE(rc))
                {
                    /* RTEnvDestroy / we cleans up later. */
                    papsz[iVar] = NULL;
                    AssertRC(rc);
                    papszRet = NULL;
                    break;
                }
            }
        }

        RTENV_UNLOCK(pIntEnv);
    }
    return papszRet;
}
RT_EXPORT_SYMBOL(RTEnvGetExecEnvP);


/**
 * RTSort callback for comparing two environment variables.
 *
 * @returns -1, 0, 1. See PFNRTSORTCMP.
 * @param   pvElement1          Variable 1.
 * @param   pvElement2          Variable 2.
 * @param   pvUser              Ignored.
 */
DECLCALLBACK(int) rtEnvSortCompare(const void *pvElement1, const void *pvElement2, void *pvUser)
{
    NOREF(pvUser);
    int iDiff = strcmp((const char *)pvElement1, (const char *)pvElement2);
    if (iDiff < 0)
        iDiff = -1;
    else if (iDiff > 0)
        iDiff = 1;
    return iDiff;
}


RTDECL(int) RTEnvQueryUtf16Block(RTENV hEnv, PRTUTF16 *ppwszzBlock)
{
    RTENV           hClone  = NIL_RTENV;
    PRTENVINTERNAL  pIntEnv;
    int             rc;

    /*
     * Validate / simplify input.
     */
    if (hEnv == RTENV_DEFAULT)
    {
        rc = RTEnvClone(&hClone, RTENV_DEFAULT);
        if (RT_FAILURE(rc))
            return rc;
        pIntEnv = hClone;
    }
    else
    {
        pIntEnv = hEnv;
        AssertPtrReturn(pIntEnv, VERR_INVALID_HANDLE);
        AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, VERR_INVALID_HANDLE);
        rc = VINF_SUCCESS;
    }

    RTENV_LOCK(pIntEnv);

    /*
     * Sort it first.
     */
    RTSortApvShell((void **)pIntEnv->papszEnv, pIntEnv->cVars, rtEnvSortCompare, pIntEnv);

    /*
     * Calculate the size.
     */
    size_t cwc;
    size_t cwcTotal = 2;
    for (size_t iVar = 0; iVar < pIntEnv->cVars; iVar++)
    {
        rc = RTStrCalcUtf16LenEx(pIntEnv->papszEnv[iVar], RTSTR_MAX, &cwc);
        AssertRCBreak(rc);
        cwcTotal += cwc + 1;
    }

    PRTUTF16 pwszzBlock = NULL;
    if (RT_SUCCESS(rc))
    {
        /*
         * Perform the conversion.
         */
        PRTUTF16 pwszz = pwszzBlock = (PRTUTF16)RTMemAlloc(cwcTotal * sizeof(RTUTF16));
        if (pwszz)
        {
            size_t cwcLeft = cwcTotal;
            for (size_t iVar = 0; iVar < pIntEnv->cVars; iVar++)
            {
                rc = RTStrToUtf16Ex(pIntEnv->papszEnv[iVar], RTSTR_MAX,
                                    &pwszz, cwcTotal - (pwszz - pwszzBlock), &cwc);
                AssertRCBreak(rc);
                pwszz   += cwc + 1;
                cwcLeft -= cwc + 1;
                AssertBreakStmt(cwcLeft >= 2, rc = VERR_INTERNAL_ERROR_3);
            }
            AssertStmt(cwcLeft == 2 || RT_FAILURE(rc), rc = VERR_INTERNAL_ERROR_2);
            if (RT_SUCCESS(rc))
            {
                pwszz[0] = '\0';
                pwszz[1] = '\0';
            }
            else
            {
                RTMemFree(pwszzBlock);
                pwszzBlock = NULL;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    RTENV_UNLOCK(pIntEnv);

    if (hClone != NIL_RTENV)
        RTEnvDestroy(hClone);
    if (RT_SUCCESS(rc))
        *ppwszzBlock = pwszzBlock;
    return rc;
}
RT_EXPORT_SYMBOL(RTEnvGetExecEnvP);


RTDECL(void) RTEnvFreeUtf16Block(PRTUTF16 pwszzBlock)
{
    RTMemFree(pwszzBlock);
}
RT_EXPORT_SYMBOL(RTEnvFreeUtf16Block);


RTDECL(uint32_t) RTEnvCountEx(RTENV hEnv)
{
    PRTENVINTERNAL pIntEnv = hEnv;
    AssertPtrReturn(pIntEnv, UINT32_MAX);
    AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, UINT32_MAX);

    RTENV_LOCK(pIntEnv);
    uint32_t cVars = (uint32_t)pIntEnv->cVars;
    RTENV_UNLOCK(pIntEnv);

    return cVars;
}
RT_EXPORT_SYMBOL(RTEnvCountEx);


RTDECL(uint32_t) RTEnvGetByIndexEx(RTENV hEnv, uint32_t iVar, char *pszVar, size_t cbVar, char *pszValue, size_t cbValue)
{
    PRTENVINTERNAL pIntEnv = hEnv;
    AssertPtrReturn(pIntEnv, UINT32_MAX);
    AssertReturn(pIntEnv->u32Magic == RTENV_MAGIC, UINT32_MAX);
    if (cbVar)
        AssertPtrReturn(pszVar, VERR_INVALID_POINTER);
    if (cbValue)
        AssertPtrReturn(pszValue, VERR_INVALID_POINTER);

    RTENV_LOCK(pIntEnv);

    int rc;
    if (iVar < pIntEnv->cVars)
    {
        const char *pszSrcVar   = pIntEnv->papszEnv[iVar];
        const char *pszSrcValue = strchr(pszSrcVar, '=');
        bool        fHasEqual   = pszSrcValue != NULL;
        if (pszSrcValue)
            pszSrcValue++;
        else
            pszSrcValue = strchr(pszSrcVar, '\0');
        rc = VINF_SUCCESS;
        if (cbVar)
            rc = RTStrCopyEx(pszVar, cbVar, pszSrcVar, pszSrcValue - pszSrcVar - fHasEqual);
        if (cbValue)
        {
            int rc2 = RTStrCopy(pszValue, cbValue, pszSrcValue);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }
    }
    else
        rc = VERR_ENV_VAR_NOT_FOUND;

    RTENV_UNLOCK(pIntEnv);

    return rc;
}
RT_EXPORT_SYMBOL(RTEnvGetByIndexEx);

