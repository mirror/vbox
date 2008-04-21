/* $Id$ */
/** @file
 * IPRT - Environment, Generic.
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
#include <iprt/env.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/string.h>
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
__BEGIN_DECLS
extern char **environ;
__END_DECLS
#endif

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Macro that unlocks the specified environment block. */
#define RTENV_LOCK(pEnvInt)     do { } while (0)
/** Macro that unlocks the specified environment block. */
#define RTENV_UNLOCK(pEnvInt)   do { } while (0)


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
} RTENVINTERNAL, *PRTENVINTERNAL;

/** The allocation granularity of the RTENVINTERNAL::papszEnv memory. */
#define RTENV_GROW_SIZE     16


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
#elif defined(RT_OS_L4)  
    /* So far, our L4 libraries do not include environment support. */
    return NULL;
#else
    return environ;
#endif 
}


/**
 * Internal worker that creates an environment handle with a specified capacity.
 * 
 * @returns IPRT status code.
 * @param   ppIntEnv    Where to store the result.
 * @param   cAllocated  The initial array size.
 */
static int rtEnvCreate(PRTENVINTERNAL *ppIntEnv, size_t cAllocated)
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
    return rtEnvCreate(pEnv, RTENV_GROW_SIZE);
}


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


RTDECL(int) RTEnvClone(PRTENV pEnv, RTENV EnvToClone)
{
    /*
     * Validate input and figure out how many variable to clone and where to get them.
     */
    size_t cVars;
    const char * const *papszEnv;
    PRTENVINTERNAL pIntEnvToClone;
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    if (EnvToClone == RTENV_DEFAULT)
    {
        pIntEnvToClone = NULL;
        papszEnv = rtEnvDefault();
        cVars = 0;
        if (papszEnv)
            while (papszEnv[cVars])
                cVars++;
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
    int rc = rtEnvCreate(&pIntEnv, cVars + 1 /* NULL */);
    if (RT_SUCCESS(rc))
    {
        pIntEnv->cVars = cVars;
        pIntEnv->papszEnv[pIntEnv->cVars] = NULL;
        if (EnvToClone == RTENV_DEFAULT)
        {
            /* ASSUMES the default environment is in the current codepage. */
            for (size_t iVar = 0; iVar < cVars; iVar++)
            {
                int rc = RTStrCurrentCPToUtf8(&pIntEnv->papszEnv[iVar], papszEnv[iVar]);
                if (RT_FAILURE(rc))
                {
                    pIntEnv->cVars = iVar;
                    RTEnvDestroy(pIntEnv);
                    return rc;
                }
            }
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
                    return rc;
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


RTDECL(int) RTEnvSetEx(RTENV Env, const char *pszVar, const char *pszValue)
{
    AssertPtrReturn(pszVar, VERR_INVALID_POINTER);
    AssertReturn(*pszVar, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszValue, VERR_INVALID_POINTER);

    int rc;
    if (Env == RTENV_DEFAULT)
    {
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
                if (    !strncmp(pIntEnv->papszEnv[iVar], pszVar, cchVar)
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


RTDECL(int) RTEnvUnsetEx(RTENV Env, const char *pszVar)
{
    AssertPtrReturn(pszVar, VERR_INVALID_POINTER);
    AssertReturn(*pszVar, VERR_INVALID_PARAMETER);

    int rc;
    if (Env == RTENV_DEFAULT)
    {
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
            if (    !strncmp(pIntEnv->papszEnv[iVar], pszVar, cchVar)
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
                }
            }
            else
                rc = VERR_ENV_VAR_NOT_FOUND;
        }
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
            if (    !strncmp(pIntEnv->papszEnv[iVar], pszVar, cchVar)
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


RTDECL(bool) RTEnvExistEx(RTENV Env, const char *pszVar)
{
    AssertPtrReturn(pszVar, false);

    bool fExist = false;
    if (Env == RTENV_DEFAULT)
    {
        /* 
         * Since RTEnvExist isn't UTF-8 clean and actually expects the strings
         * to be in the current code page (codeset), we'll do the necessary 
         * conversions here.
         */
        char *pszVarOtherCP;
        int rc = RTStrUtf8ToCurrentCP(&pszVarOtherCP, pszVar);
        if (RT_SUCCESS(rc))
        {
            fExist = RTEnvExist(pszVarOtherCP);
            RTStrFree(pszVarOtherCP);
        }
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
            if (    !strncmp(pIntEnv->papszEnv[iVar], pszVar, cchVar)
                &&  pIntEnv->papszEnv[iVar][cchVar] == '=')
            {
                fExist = true;
                break;
            }

        RTENV_UNLOCK(pIntEnv);
    }
    return fExist;
}


RTDECL(char const * const *) RTEnvGetExecEnvP(RTENV Env)
{
    const char * const *papszRet;
    if (Env == RTENV_DEFAULT)
    {
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
