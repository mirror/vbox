/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Thread Management, Ring-0 Driver.
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

#ifndef __r0drv_thread_r0drv_h_
#define __r0drv_thread_r0drv_h_

#include <iprt/thread.h>

__BEGIN_DECLS

/**
 * Argument package for a ring-0 thread.
 */
typedef struct RTR0THREADARGS
{
    /** The thread function. */
    PFNRTTHREAD     pfnThread;
    /** The thread function argument. */
    void           *pvUser;
    /** The thread type. */
    RTTHREADTYPE    enmType;
} RTR0THREADARGS, *PRTR0THREADARGS;



int rtThreadMain(RTNATIVETHREAD Self, PRTR0THREADARGS pArgs);


/**
 * Do the actual thread creation.
 *
 * @returns IPRT status code.
 *          On failure, no thread has been created.
 * @param   pArgs           The argument package.
 * @param   pNativeThread   Where to return the native thread handle.
 */
int rtThreadNativeCreate(PRTR0THREADARGS pArgs, PRTNATIVETHREAD pNativeThread);

/**
 * Do the actual thread priority change.
 *
 * @returns IPRT status code.
 * @param   Thread      The thread which priority should be changed.
 *                      This is currently restricted to the current thread.
 * @param   enmType     The new thread priority type (valid).
 */
int rtThreadNativeSetPriority(RTTHREAD Thread, RTTHREADTYPE enmType);


__END_DECLS

#endif

