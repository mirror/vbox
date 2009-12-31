/* $Id$ */
/** @file
 * IPRT - Internal RTLockValidator header.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#ifndef ___iprt_internal_lockvalidator_h
#define ___iprt_internal_lockvalidator_h

#include <iprt/types.h>
#include <iprt/lockvalidator.h>
#include "internal/lockvalidator.h"

RT_C_DECLS_BEGIN

/**
 * Record union for simplifying internal processing.
 */
typedef union RTLOCKVALIDATORRECUNION
{
    RTLOCKVALIDATORRECCORE      Core;
    RTLOCKVALIDATORREC          Excl;
    RTLOCKVALIDATORSHARED       Shared;
    RTLOCKVALIDATORSHAREDONE    SharedOne;
} RTLOCKVALIDATORRECUNION;


/**
 * Per thread data for the lock validator.
 *
 * This is part of the RTTHREADINT structure.
 */
typedef struct RTLOCKVALIDATORPERTHREAD
{
    /** What we're blocking on. */
    PRTLOCKVALIDATORRECUNION volatile   pRec;
    /** Where we are blocking. */
    RTLOCKVALIDATORSRCPOS               SrcPos;
    /** Number of registered write locks, mutexes and critsects that this thread owns. */
    int32_t volatile                    cWriteLocks;
    /** Number of registered read locks that this thread owns, nesting included. */
    int32_t volatile                    cReadLocks;
} RTLOCKVALIDATORPERTHREAD;


DECLHIDDEN(void)    rtLockValidatorSerializeDestructEnter(void);
DECLHIDDEN(void)    rtLockValidatorSerializeDestructLeave(void);

RT_C_DECLS_END

#endif

