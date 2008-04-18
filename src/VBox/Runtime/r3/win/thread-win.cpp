/* $Id$ */
/** @file
 * innotek Portable Runtime - Threads, Win32.
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
#define LOG_GROUP RTLOGGROUP_THREAD
#include <Windows.h>

#include <errno.h>
#include <process.h>

#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include "internal/thread.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The TLS index allocated for storing the RTTHREADINT pointer. */
static DWORD g_dwSelfTLS = TLS_OUT_OF_INDEXES;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static unsigned __stdcall rtThreadNativeMain(void *pvArgs);


int rtThreadNativeInit(void)
{
    g_dwSelfTLS = TlsAlloc();
    if (g_dwSelfTLS == TLS_OUT_OF_INDEXES)
        return VERR_NO_TLS_FOR_SELF;
    return VINF_SUCCESS;
}


void rtThreadNativeDetach(void)
{
    /*
     * Deal with alien threads.
     */
    PRTTHREADINT pThread = (PRTTHREADINT)TlsGetValue(g_dwSelfTLS);
    if (    pThread
        &&  (pThread->fIntFlags & RTTHREADINT_FLAGS_ALIEN))
    {
        rtThreadTerminate(pThread, 0);
        TlsSetValue(g_dwSelfTLS, NULL);
    }
}


int rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    if (!TlsSetValue(g_dwSelfTLS, pThread))
        return VERR_FAILED_TO_SET_SELF_TLS;
    return VINF_SUCCESS;
}


/**
 * Wrapper which unpacks the param stuff and calls thread function.
 */
static unsigned __stdcall rtThreadNativeMain(void *pvArgs)
{
    DWORD           dwThreadId = GetCurrentThreadId();
    PRTTHREADINT    pThread = (PRTTHREADINT)pvArgs;

    if (!TlsSetValue(g_dwSelfTLS, pThread))
        AssertReleaseMsgFailed(("failed to set self TLS. lasterr=%d thread '%s'\n", GetLastError(), pThread->szName));

    int rc = rtThreadMain(pThread, dwThreadId, &pThread->szName[0]);

    TlsSetValue(g_dwSelfTLS, NULL);
    _endthreadex(rc);
    return rc;
}


int rtThreadNativeCreate(PRTTHREADINT pThread, PRTNATIVETHREAD pNativeThread)
{
    AssertReturn(pThread->cbStack < ~(unsigned)0, VERR_INVALID_PARAMETER);

    /*
     * Create the thread.
     */
    pThread->hThread = (uintptr_t)INVALID_HANDLE_VALUE;
    unsigned    uThreadId = 0;
    uintptr_t   hThread = _beginthreadex(NULL, (unsigned)pThread->cbStack, rtThreadNativeMain, pThread, 0, &uThreadId);
    if (hThread != 0 && hThread != ~0U)
    {
        pThread->hThread = hThread;
        *pNativeThread = uThreadId;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    PRTTHREADINT pThread = (PRTTHREADINT)TlsGetValue(g_dwSelfTLS);
    /** @todo import alien threads ? */
    return pThread;
}


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)GetCurrentThreadId();
}


RTR3DECL(int)   RTThreadSleep(unsigned cMillies)
{
    LogFlow(("RTThreadSleep: cMillies=%d\n", cMillies));
    Sleep(cMillies);
    LogFlow(("RTThreadSleep: returning %Vrc (cMillies=%d)\n", VINF_SUCCESS, cMillies));
    return VINF_SUCCESS;
}


RTR3DECL(bool) RTThreadYield(void)
{
    uint64_t u64TS = ASMReadTSC();
    Sleep(0);
    u64TS = ASMReadTSC() - u64TS;
    bool fRc = u64TS > 1500;
    LogFlow(("RTThreadYield: returning %d (%llu ticks)\n", fRc, u64TS));
    return fRc;
}


#if 0 /* noone is using this ... */
/**
 * Returns the processor number the current thread was running on during this call
 *
 * @returns processor nr
 */
static int rtThreadGetCurrentProcessorNumber(void)
{
    static bool           fInitialized = false;
    static DWORD (WINAPI *pfnGetCurrentProcessorNumber)(void) = NULL;
    if (!fInitialized)
    {
        HMODULE hmodKernel32 = GetModuleHandle("KERNEL32.DLL");
        if (hmodKernel32)
            pfnGetCurrentProcessorNumber = (DWORD (WINAPI*)(void))GetProcAddress(hmodKernel32, "GetCurrentProcessorNumber");
        fInitialized = true;
    }
    if (pfnGetCurrentProcessorNumber)
        return pfnGetCurrentProcessorNumber();
    return -1;
}
#endif


RTR3DECL(int) RTThreadSetAffinity(uint64_t u64Mask)
{
    Assert((DWORD_PTR)u64Mask == u64Mask || u64Mask == ~(uint64_t)0);
    DWORD dwRet = SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)u64Mask);
    if (dwRet)
        return VINF_SUCCESS;

    int iLastError = GetLastError();
    AssertMsgFailed(("SetThreadAffinityMask failed, LastError=%d\n", iLastError));
    return RTErrConvertFromWin32(iLastError);
}


RTR3DECL(uint64_t) RTThreadGetAffinity(void)
{
    /*
     * Haven't found no query api, but the set api returns the old mask, so let's use that.
     */
    DWORD_PTR dwIgnored;
    DWORD_PTR dwProcAff = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &dwProcAff, &dwIgnored))
    {
        HANDLE hThread = GetCurrentThread();
        DWORD dwRet = SetThreadAffinityMask(hThread, dwProcAff);
        if (dwRet)
        {
            DWORD dwSet = SetThreadAffinityMask(hThread, dwRet);
            Assert(dwSet == dwProcAff); NOREF(dwRet);
            return dwRet;
        }
    }

    int iLastError = GetLastError();
    AssertMsgFailed(("SetThreadAffinityMask or GetProcessAffinityMask failed, LastError=%d\n", iLastError));
    return RTErrConvertFromWin32(iLastError);
}

