/* $Id$ */
/** @file
 * IPRT - Common semaphore wait code, Linux.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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

#ifndef IPRT_INCLUDED_SRC_r3_linux_semwait_linux_h
#define IPRT_INCLUDED_SRC_r3_linux_semwait_linux_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/* With 2.6.17 futex.h has become C++ unfriendly, so define the bits we need. */
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_WAIT_BITSET 9 /**< @since 2.6.25 - uses absolute timeout. */


/**
 * Wrapper for the futex syscall.
 */
DECLINLINE(long) sys_futex(uint32_t volatile *uaddr, int op, int val, struct timespec *utime, int32_t *uaddr2, int val3)
{
    errno = 0;
    long rc = syscall(__NR_futex, uaddr, op, val, utime, uaddr2, val3);
    if (rc < 0)
    {
        Assert(rc == -1);
        rc = -errno;
    }
    return rc;
}


DECL_NO_INLINE(static, void) rtSemLinuxCheckForFutexWaitBitSetSlow(int volatile *pfCanUseWaitBitSet)
{
    uint32_t uTestVar = UINT32_MAX;
    long rc = sys_futex(&uTestVar, FUTEX_WAIT_BITSET, UINT32_C(0xf0f0f0f0), NULL, NULL, UINT32_MAX);
    *pfCanUseWaitBitSet = rc == -EAGAIN;
    AssertMsg(rc == -ENOSYS || rc == -EAGAIN, ("%d\n", rc));
}


DECLINLINE(void) rtSemLinuxCheckForFutexWaitBitSet(int volatile *pfCanUseWaitBitSet)
{
    if (*pfCanUseWaitBitSet != -1)
    { /* likely */ }
    else
        rtSemLinuxCheckForFutexWaitBitSetSlow(pfCanUseWaitBitSet);
}

#endif /* !IPRT_INCLUDED_SRC_r3_linux_semwait_linux_h */

