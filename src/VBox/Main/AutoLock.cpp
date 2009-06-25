/** @file
 *
 * AutoWriteLock/AutoReadLock: smart R/W semaphore wrappers
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "AutoLock.h"

#include "Logging.h"

#include <iprt/string.h>


namespace util
{

RWLockHandle::RWLockHandle()
{
    int vrc = RTSemRWCreate (&mSemRW);
    AssertRC (vrc);
}


RWLockHandle::~RWLockHandle()
{
    RTSemRWDestroy (mSemRW);
}


bool RWLockHandle::isWriteLockOnCurrentThread() const
{
    return RTSemRWIsWriteOwner (mSemRW);
}


void RWLockHandle::lockWrite()
{
    int vrc = RTSemRWRequestWrite (mSemRW, RT_INDEFINITE_WAIT);
    AssertRC (vrc);
}


void RWLockHandle::unlockWrite()
{
    int vrc = RTSemRWReleaseWrite (mSemRW);
    AssertRC (vrc);
}


void RWLockHandle::lockRead()
{
    int vrc = RTSemRWRequestRead (mSemRW, RT_INDEFINITE_WAIT);
    AssertRC (vrc);
}


void RWLockHandle::unlockRead()
{
    int vrc = RTSemRWReleaseRead (mSemRW);
    AssertRC (vrc);
}


uint32_t RWLockHandle::writeLockLevel() const
{
    return RTSemRWGetWriteRecursion (mSemRW);
}

} /* namespace util */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
