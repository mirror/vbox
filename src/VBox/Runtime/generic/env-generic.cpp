/* $Id$ */
/** @file
 * innotek Portable Runtime - Environment, Generic.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/env.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/err.h>

#if defined(RT_OS_WINDOWS)
# include <stdlib.h>
#else
# include <unistd.h>
#endif

#include <string.h>

struct RTENVINTERNAL
{
    /* Array of environment variables. */
    char **apszEnv;
    /* Count of variables in the array. */
    size_t cCount;
    /* Capacity (real size) of the array. This includes space for the
     * terminating NULL element (for compatibility with the C library), so
     * that cCount <= cCapacity - 1. */
    size_t cCapacity;
};

#define RTENV_GROW_SIZE 16

static int rtEnvCreate(struct RTENVINTERNAL **ppIntEnv, size_t cCapacity)
{
    int rc;

    /*
     * Allocate environment handle.
     */
    struct RTENVINTERNAL *pIntEnv = (struct RTENVINTERNAL *)RTMemAlloc(sizeof(struct RTENVINTERNAL));
    if (pIntEnv)
    {
        cCapacity = (cCapacity + RTENV_GROW_SIZE - 1)
                    / RTENV_GROW_SIZE * RTENV_GROW_SIZE;
        /*
         * Pre-allocate the variable array.
         */
        pIntEnv->cCount = 0;
        pIntEnv->cCapacity = cCapacity;
        pIntEnv->apszEnv = (char **)RTMemAlloc(sizeof(pIntEnv->apszEnv[0]) * pIntEnv->cCapacity);
        if (pIntEnv->apszEnv)
        {
            /* add terminating NULL */
            pIntEnv->apszEnv[0] = NULL;
            *ppIntEnv = pIntEnv;
            return VINF_SUCCESS;
        }

        rc = VERR_NO_MEMORY;
        RTMemFree(pIntEnv);
    }    
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Creates an empty environment block.
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   pEnv        Where to store the handle of the environment block.
 */
RTDECL(int) RTEnvCreate(PRTENV pEnv)
{
    if (pEnv == NULL)
        return VERR_INVALID_POINTER;

    return rtEnvCreate(pEnv, RTENV_GROW_SIZE);
}

/**
 * Destroys an environment block.
 *
 * @returns IPRT status code.
 *
 * @param   Env     Handle of the environment block.
 */
RTDECL(int) RTEnvDestroy(RTENV Env)
{
    struct RTENVINTERNAL *pIntEnv = Env;

    if (pIntEnv == NULL)
        return VERR_INVALID_HANDLE;

    for (size_t i = 0; i < pIntEnv->cCount; ++i)
    {
        RTStrFree(pIntEnv->apszEnv[i]);
    }

    RTMemFree(pIntEnv->apszEnv);
    RTMemFree(pIntEnv);
    return VINF_SUCCESS;
}

/**
 * Creates an environment block and fill it with variables from the given
 * environment array.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pEnv        Where to store the handle of the environment block.
 * @param   apszEnv     Pointer to the NULL-terminated array of environment
 *                      variables. If NULL, the current process' environment
 *                      will be cloned.
 */
RTDECL(int) RTEnvClone(PRTENV pEnv, char const *const *apszEnv)
{
    if (apszEnv == NULL)
        apszEnv = environ;

    /* count the number of varialbes to clone */
    size_t cEnv = 0;
    for (; apszEnv[cEnv]; ++cEnv) {}

    struct RTENVINTERNAL *pIntEnv;

    int rc = rtEnvCreate(&pIntEnv, cEnv);
    if (RT_FAILURE(rc))
        return rc;

    for (size_t i = 0; i < cEnv; ++i)
    {
        char *pszVar = RTStrDup(environ[i]);
        if (pszVar == NULL)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pIntEnv->apszEnv[i] = pszVar;
        ++pIntEnv->cCount;
    }

    if (RT_SUCCESS(rc))
    {
        /* add terminating NULL */
        pIntEnv->apszEnv[pIntEnv->cCount] = NULL;
        *pEnv = pIntEnv;
        return VINF_SUCCESS;
    }

    RTEnvDestroy(pIntEnv);
    return rc;
}

static int rtEnvRemoveVars(struct RTENVINTERNAL *pIntEnv, size_t iFrom, size_t cVars)
{
    AssertReturn (iFrom < pIntEnv->cCount, VERR_GENERAL_FAILURE);
    AssertReturn (cVars <= pIntEnv->cCount, VERR_GENERAL_FAILURE);
    AssertReturn (cVars > 0, VERR_GENERAL_FAILURE);

    /* free variables */
    size_t iTo = iFrom + cVars - 1;
    for (size_t i = iFrom; i <= iTo; ++i)
        RTStrFree(pIntEnv->apszEnv[i]);

    /* remove the hole */
    size_t cToMove = pIntEnv->cCount - iTo - 1;
    if (cToMove)
        memcpy(&pIntEnv->apszEnv[iFrom], &pIntEnv->apszEnv[iTo + 1],
               sizeof(pIntEnv->apszEnv[0]) * cToMove);

    pIntEnv->cCount -= cVars;

    /// @todo resize pIntEnv->apszEnv to a multiply of RTENV_GROW_SIZE to keep
    /// it compact

    /* add terminating NULL */
    pIntEnv->apszEnv[pIntEnv->cCount] = NULL;
    return VINF_SUCCESS;
}

static int rtEnvInsertVars(struct RTENVINTERNAL *pIntEnv, size_t iAt, size_t cVars)
{
    AssertReturn (iAt <= pIntEnv->cCount, VERR_GENERAL_FAILURE);

    int rc;

    size_t cCapacity = (pIntEnv->cCount + cVars + RTENV_GROW_SIZE - 1)
                       / RTENV_GROW_SIZE * RTENV_GROW_SIZE;
    bool needAlloc = cCapacity != pIntEnv->cCapacity;

    /* allocate a new variable array if needed */
    char **apszEnv = needAlloc ? (char **)RTMemAlloc(sizeof(apszEnv[0]) * cCapacity)
                               : pIntEnv->apszEnv;
    if (apszEnv)
    {
        /* copy old variables */
        if (needAlloc && iAt)
            memcpy (apszEnv, pIntEnv->apszEnv, sizeof(apszEnv[0]) * iAt);
        if (iAt < pIntEnv->cCount)
            memcpy (&apszEnv[iAt + cVars], &pIntEnv->apszEnv[iAt],
                    sizeof(apszEnv[0]) * pIntEnv->cCount - iAt);
        /* initialize new variables with NULL */
        memset(&apszEnv[iAt], 0, sizeof(apszEnv[0]) * cVars);
        /* replace the array */
        if (needAlloc)
        {
            RTMemFree(pIntEnv->apszEnv);
            pIntEnv->apszEnv = apszEnv;
            pIntEnv->cCapacity = cCapacity;
        }
        pIntEnv->cCount += cVars;
        /* add terminating NULL */
        pIntEnv->apszEnv[pIntEnv->cCount] = NULL;
        return VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

static int rtEnvSetEx(struct RTENVINTERNAL *pIntEnv,
                      const char *pszVar, size_t cchVar,
                      const char *pszValue)
{
    AssertReturn (pszVar != NULL, VERR_GENERAL_FAILURE);

    size_t i = 0;
    for (; i < pIntEnv->cCount; ++i)
    {
        if ((cchVar == 0 && !*pszVar) ||
            strncmp(pIntEnv->apszEnv[i], pszVar, cchVar) == 0)
        {
            if (pszValue == NULL)
                return rtEnvRemoveVars(pIntEnv, i, 1);

            break;
        }
    }

    /* allocate a new variable */
    size_t cchNew = cchVar + 1 + strlen(pszValue) + 1;
    char *pszNew = (char *)RTMemAlloc(cchNew);
    if (pszNew == NULL)
        return VERR_NO_MEMORY;
    memcpy(pszNew, pszVar, cchVar);
    pszNew[cchVar] = '=';
    strcpy(pszNew + cchVar + 1, pszValue);

    if (i < pIntEnv->cCount)
    {
        /* replace the old variable */
        RTStrFree(pIntEnv->apszEnv[i]);
        pIntEnv->apszEnv[i] = pszNew;
        return VINF_SUCCESS;
    }

    /* nothing to do to remove a non-existent variable */
    if (pszValue == NULL)
        return VINF_SUCCESS;

    /* insert the new variable */
    int rc = rtEnvInsertVars(pIntEnv, i, 1);
    if (RT_SUCCESS(rc))
    {
        pIntEnv->apszEnv[i] = pszNew;
        return VINF_SUCCESS;
    }

    RTStrFree(pszNew);
    return rc;
}

/**
 * Puts a 'variable=value' string into the environment.
 *
 * The supplied string must be in the current process' codepage.
 * This function makes a copy of the supplied string.
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   Env                 Handle of the environment block.
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is 
 *                              omitted, the variable is removed from the environment.
 */
RTDECL(int) RTEnvPutEx(RTENV Env, const char *pszVarEqualValue)
{
    struct RTENVINTERNAL *pIntEnv = Env;

    if (pIntEnv == NULL)
        return VERR_INVALID_HANDLE;

    if (pszVarEqualValue == NULL)
        return VERR_INVALID_POINTER;

    const char *pszEq = strchr(pszVarEqualValue, '=');
    return rtEnvSetEx(pIntEnv, pszVarEqualValue,
                      pszEq ? pszEq - pszVarEqualValue
                            : strlen (pszVarEqualValue),
                      pszEq ? pszEq + 1 : NULL);
}

/**
 * Returns a raw pointer to the array of environment variables of the given
 * environment block where every variable is a string in format
 * 'variable=value'.
 *
 * All returned strings are in the current process' codepage.
 * 
 * @returns Pointer to the raw array of environment variables.
 * @returns NULL if Env is NULL or invalid.
 * 
 * @param   Env                 Handle of the environment block.
 */
RTDECL(char const *const *) RTEnvGetArray(RTENV Env)
{
    struct RTENVINTERNAL *pIntEnv = Env;

    if (pIntEnv == NULL)
        return NULL;

    return pIntEnv->apszEnv;
}
