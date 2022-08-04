/** @file
 * IPRT / No-CRT - Our minimal stdlib.h.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_nocrt_stdlib_h
#define IPRT_INCLUDED_nocrt_stdlib_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/nocrt/limits.h>

RT_C_DECLS_BEGIN

#define EXIT_SUCCESS    RTEXITCODE_SUCCESS
#define EXIT_FAILURE    RTEXITCODE_FAILURE


typedef void FNRTNOCRTATEXITCALLBACK(void) /*RT_NOEXCEPT*/;
typedef FNRTNOCRTATEXITCALLBACK *PFNRTNOCRTATEXITCALLBACK;
#if defined(_MSC_VER) && defined(RT_WITHOUT_NOCRT_WRAPPERS) /* Clashes with compiler internal prototype or smth. */
int          rtnocrt_atexit(PFNRTNOCRTATEXITCALLBACK) RT_NOEXCEPT;
# define     atexit rtnocrt_atexit
#else
int          RT_NOCRT(atexit)(PFNRTNOCRTATEXITCALLBACK) RT_NOEXCEPT;
#endif

#if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
# define atexit     RT_NOCRT(atexit)
#endif


#ifdef IPRT_NO_CRT_FOR_3RD_PARTY
/*
 * Only for external libraries and such.
 */

DECLINLINE(void *) RT_NOCRT(malloc)(size_t cb)
{
    return RTMemAlloc(cb);
}

DECLINLINE(void *) RT_NOCRT(calloc)(size_t cItems, size_t cbItem)
{
    return RTMemAllocZ(cItems * cbItem); /* caller responsible for overflow issues. */
}

DECLINLINE(void *) RT_NOCRT(realloc)(void *pvOld, size_t cbNew)
{
    return RTMemRealloc(pvOld, cbNew);
}

DECLINLINE(void) RT_NOCRT(free)(void *pv)
{
    RTMemFree(pv);
}

DECLINLINE(const char *) RT_NOCRT(getenv)(const char *pszVar)
{
    return RTEnvGet(pszVar);
}

int         RT_NOCRT(abs)(int);
long        RT_NOCRT(labs)(long);
long        RT_NOCRT(labs)(long);
long long   RT_NOCRT(llabs)(long long);
int         RT_NOCRT(rand)(void);
void        RT_NOCRT(srand)(unsigned);
long        RT_NOCRT(strtol)(const char *psz, char **ppszNext, int iBase);
long long   RT_NOCRT(strtoll)(const char *psz, char **ppszNext, int iBase);
unsigned long RT_NOCRT(strtoul)(const char *psz, char **ppszNext, int iBase);
unsigned long long RT_NOCRT(strtoull)(const char *psz, char **ppszNext, int iBase);
int         RT_NOCRT(atoi)(const char *psz);
double      RT_NOCRT(strtod)(const char *psz, char **ppszNext);
double      RT_NOCRT(atof)(const char *psz);
void       *RT_NOCRT(bsearch)(const void *pvKey, const void *pvBase, size_t cEntries, size_t cbEntry,
                              int (*pfnCompare)(const void *pv1, const void *pv2));
void        RT_NOCRT(qsort)(void *pvBase, size_t cEntries, size_t cbEntry,
                            int (*pfnCompare)(const void *pv1, const void *pv2));
void        RT_NOCRT(qsort_r)(void *pvBase, size_t cEntries, size_t cbEntry,
                              int (*pfnCompare)(const void *pv1, const void *pv2, void *pvUser), void *pvUser);

/* Map exit & abort onto fatal assert. */
DECL_NO_RETURN(DECLINLINE(void)) RT_NOCRT(exit)(int iExitCode) { AssertMsgFailed(("exit: iExitCode=%d\n", iExitCode)); }
DECL_NO_RETURN(DECLINLINE(void)) RT_NOCRT(abort)(void) { AssertMsgFailed(("abort\n")); }

/*
 * Underscored versions:
 */
DECLINLINE(void *) RT_NOCRT(_malloc)(size_t cb)
{
    return RTMemAlloc(cb);
}

DECLINLINE(void *) RT_NOCRT(_calloc)(size_t cItems, size_t cbItem)
{
    return RTMemAllocZ(cItems * cbItem); /* caller responsible for overflow issues. */
}

DECLINLINE(void *) RT_NOCRT(_realloc)(void *pvOld, size_t cbNew)
{
    return RTMemRealloc(pvOld, cbNew);
}

DECLINLINE(void) RT_NOCRT(_free)(void *pv)
{
    RTMemFree(pv);
}

DECLINLINE(const char *) RT_NOCRT(_getenv)(const char *pszVar)
{
    return RTEnvGet(pszVar);
}

int         RT_NOCRT(_abs)(int);
long        RT_NOCRT(_labs)(long);
long        RT_NOCRT(_labs)(long);
long long   RT_NOCRT(_llabs)(long long);
int         RT_NOCRT(_rand)(void);
void        RT_NOCRT(_srand)(unsigned);
long        RT_NOCRT(_strtol)(const char *psz, char **ppszNext, int iBase);
long long   RT_NOCRT(_strtoll)(const char *psz, char **ppszNext, int iBase);
unsigned long RT_NOCRT(_strtoul)(const char *psz, char **ppszNext, int iBase);
unsigned long long RT_NOCRT(_strtoull)(const char *psz, char **ppszNext, int iBase);
int         RT_NOCRT(_atoi)(const char *psz);
double      RT_NOCRT(_strtod)(const char *psz, char **ppszNext);
double      RT_NOCRT(_atof)(const char *psz);
void       *RT_NOCRT(_bsearch)(const void *pvKey, const void *pvBase, size_t cEntries, size_t cbEntry,
                               int (*pfnCompare)(const void *pv1, const void *pv2));
void        RT_NOCRT(_qsort)(void *pvBase, size_t cEntries, size_t cbEntry,
                             int (*pfnCompare)(const void *pv1, const void *pv2));
void        RT_NOCRT(_qsort_r)(void *pvBase, size_t cEntries, size_t cbEntry,
                               int (*pfnCompare)(const void *pv1, const void *pv2, void *pvUser), void *pvUser);

/* Map exit & abort onto fatal assert. */
DECL_NO_RETURN(DECLINLINE(void)) RT_NOCRT(_exit)(int iExitCode) { AssertMsgFailed(("_exit: iExitCode=%d\n", iExitCode)); }
DECL_NO_RETURN(DECLINLINE(void)) RT_NOCRT(_abort)(void) { AssertMsgFailed(("_abort\n")); }

/* Some windows CRT error control functions we totally ignore (only underscored): */
# define _set_error_mode(a_Mode)                    (0)
# define _set_abort_behavior(a_fFlags, a_fMask)     (0)

/*
 * No-CRT aliases.
 */
# if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
#  define malloc        RT_NOCRT(malloc)
#  define calloc        RT_NOCRT(calloc)
#  define realloc       RT_NOCRT(realloc)
#  define free          RT_NOCRT(free)
#  define getenv        RT_NOCRT(getenv)
#  define bsearch       RT_NOCRT(bsearch)
#  define exit          RT_NOCRT(exit)
#  define abort         RT_NOCRT(abort)
#  define rand          RT_NOCRT(rand)
#  define srand         RT_NOCRT(srand)
#  define strtol        RT_NOCRT(strtol)
#  define strtoll       RT_NOCRT(strtoll)
#  define strtoul       RT_NOCRT(strtoul)
#  define strtoull      RT_NOCRT(strtoull)
#  define atoi          RT_NOCRT(atoi)
#  define strtod        RT_NOCRT(strtod)
#  define atof          RT_NOCRT(atof)

#  define _malloc       RT_NOCRT(malloc)
#  define _calloc       RT_NOCRT(calloc)
#  define _realloc      RT_NOCRT(realloc)
#  define _free         RT_NOCRT(free)
#  define _getenv       RT_NOCRT(getenv)
#  define _bsearch      RT_NOCRT(bsearch)
#  define _exit         RT_NOCRT(exit)
#  define _abort        RT_NOCRT(abort)
#  define _strtol       RT_NOCRT(strtol)
#  define _strtoll      RT_NOCRT(strtoll)
#  define _strtoul      RT_NOCRT(strtoul)
#  define _strtoull     RT_NOCRT(strtoull)
#  define _atoi         RT_NOCRT(atoi)
#  define _strtod       RT_NOCRT(strtod)
#  define _atof         RT_NOCRT(atof)
#  endif

#endif /* IPRT_NO_CRT_FOR_3RD_PARTY */


RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_nocrt_stdlib_h */
