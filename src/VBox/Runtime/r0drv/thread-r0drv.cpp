/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Threads, Ring-0 Driver, Common Bits.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
#define LOG_GROUP RTLOGGROUP_THREAD
#include "r0drv/thread-r0drv.h"
#include <iprt/thread.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/err.h>
#include <iprt/string.h>


/* In ring-0 RTTHREAD == RTNATIVETHREAD. */

RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)RTThreadSelf();
}

RTDECL(RTNATIVETHREAD) RTThreadGetNative(RTTHREAD Thread)
{
    return (RTNATIVETHREAD)Thread;
}

RTDECL(RTTHREAD) RTThreadFromNative(RTNATIVETHREAD NativeThread)
{
    return (RTTHREAD)NativeThread;
}


/**
 * Checks that the give thread type is valid for a ring-0 context.
 *
 * @returns true if it's valid, otherwise false is returned.
 * @param   enmType   The thread type.
 */
static bool rtThreadIsTypeValid(RTTHREADTYPE enmType)
{
    switch (enmType)
    {
        case RTTHREADTYPE_INFREQUENT_POLLER:
        case RTTHREADTYPE_EMULATION:
        case RTTHREADTYPE_DEFAULT:
        case RTTHREADTYPE_MSG_PUMP:
        case RTTHREADTYPE_IO:
        case RTTHREADTYPE_TIMER:
            return true;
        default:
            return false;
    }
}


RTDECL(int) RTThreadSetType(RTTHREAD Thread, RTTHREADTYPE enmType)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(RTThreadSelf() == Thread, ("%RTthrd != %RTthrd\n", RTThreadSelf(), Thread), VERR_INVALID_HANDLE);
    AssertMsgReturn(rtThreadIsTypeValid(enmType), ("enmType=%d\n", enmType), VERR_INVALID_PARAMETER);

    /*
     * Call the native function to do the actual job.
     */
    return rtThreadNativeSetPriority(Thread, enmType);
}


/**
 * Common thread wrapper function.
 *
 * This will adjust the priority and invoke the IPRT thread function.
 *
 * @param pArgs         Pointer to the argument package.
 * @param NativeThread  The native thread handle.
 */
int rtThreadMain(PRTR0THREADARGS pArgs, RTNATIVETHREAD NativeThread)
{
    RTR0THREADARGS Args = *pArgs;
    RTMemTmpFree(pArgs);

    /*
     * Set thread priority and invoke the thread function.
     */
    int rc = rtThreadNativeSetPriority((RTTHREAD)NativeThread, Args.enmType);
    AssertRC(rc);
    return Args.pfnThread((RTTHREAD)NativeThread, Args.pvUser);
}


RTDECL(int) RTThreadCreate(PRTTHREAD pThread, PFNRTTHREAD pfnThread, void *pvUser, size_t cbStack,
                           RTTHREADTYPE enmType, unsigned fFlags, const char *pszName)
{
    LogFlow(("RTThreadCreate: pThread=%p pfnThread=%p pvUser=%p cbStack=%#x enmType=%d fFlags=%#x pszName=%p:{%s}\n",
             pThread, pfnThread, pvUser, cbStack, enmType, fFlags, pszName, pszName));

    /*
     * Validate input.
     */
    AssertPtrNullReturn(pThread, VERR_INVALID_POINTER);
    AssertPtrReturn(pfnThread, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertMsgReturn(strlen(pszName) < 16, ("%.*64s", pszName), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fFlags & ~RTTHREADFLAGS_MASK), ("fFlags=%#x\n", fFlags), VERR_INVALID_PARAMETER);
    AssertMsgReturn(cbStack == 0, ("cbStack=%#x - only default (0) is allowed for Ring-0!\n", cbStack), VERR_INVALID_PARAMETER);

    /*
     * Allocate thread argument.
     */
    PRTR0THREADARGS pArgs = (PRTR0THREADARGS)RTMemTmpAlloc(sizeof(*pArgs));
    if (!pArgs)
        return VERR_NO_TMP_MEMORY;

    pArgs->pvUser = pvUser;
    pArgs->pfnThread = pfnThread;
    pArgs->enmType = enmType;
    RTNATIVETHREAD NativeThread;
    int rc = rtThreadNativeCreate(pArgs, &NativeThread);
    if (RT_SUCCESS(rc))
    {
        Log(("RTThreadCreate: Created thread %RTnthrd %s\n", NativeThread, pszName));
        if (pThread)
            *pThread = (RTTHREAD)NativeThread;
        return VINF_SUCCESS;
    }
    RTMemTmpFree(pArgs);
    LogFlow(("RTThreadCreate: Failed to create thread, rc=%Rrc\n", rc));
    AssertRC(rc);
    return rc;
}

