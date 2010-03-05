/* $Id$ */

/** @file
 *
 * VirtualBox medium object lock collections
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef ____H_MEDIUMLOCK
#define ____H_MEDIUMLOCK

/* interface definitions */
#include "VBox/com/VirtualBox.h"

#include <iprt/types.h>

/**
 * Single entry for medium lock lists. Has a medium object reference,
 * information about what kind of lock should be taken, and if it is
 * locked right now.
 */
class MediumLock
{
public:
    /**
     * Default medium lock constructor.
     */
    MediumLock();

    /**
     * Default medium lock destructor.
     */
    ~MediumLock();

    /**
     * Create a new medium lock description
     *
     * @param aMedium       Reference to medium object
     * @param aLockWrite    @c true means a write lock should be taken
     */
    MediumLock(ComObjPtr<Medium> aMedium, bool aLockWrite);

    /**
     * Acquire a medium lock.
     */
    int Lock();

    /**
     * Release a medium lock.
     */
    int Unlock();

private:
    ComObjPtr<Medium> mMedium;
    bool mLockWrite;
};


/**
 * Medium lock list. Meant for storing the ordered locking information
 * for a single medium chain.
 */
class MediumLockList
{
public:

    /**
     * Default medium lock list constructor.
     */
    MediumLockList();

    /**
     * Default medium lock list destructor.
     */
    ~MediumLockList();

    /**
     * Add a new medium lock declaration to the end of the list.
     *
     * @note May be only used in unlocked state.
     *
     * @return VBox status code.
     * @param aMedium       Reference to medium object
     * @param aLockWrite    @c true means a write lock should be taken
     */
    int Append(ComObjPtr<Medium> aMedium, bool aLockWrite);

    /**
     * Add a new medium lock declaration to the beginning of the list.
     *
     * @note May be only used in unlocked state.
     *
     * @return VBox status code.
     * @param aMedium       Reference to medium object
     * @param aLockWrite    @c true means a write lock should be taken
     */
    int Prepend(ComObjPtr<Medium> aMedium, bool aLockWrite);

    /**
     * Update a medium lock declaration.
     *
     * @note May be only used in unlocked state.
     *
     * @return VBox status code.
     * @param aMedium       Reference to medium object
     * @param aLockWrite    @c true means a write lock should be taken
     */
    int Update(ComObjPtr<Medium> aMedium, bool aLockWrite);

    /**
     * Remove a medium lock declaration.
     *
     * @note May be only used in unlocked state.
     *
     * @return VBox status code.
     * @param aMedium       Reference to medium object
     */
    int Remove(ComObjPtr<Medium> aMedium);

    /**
     * Acquire all medium locks "atomically", i.e. all or nothing.
     */
    int Lock();

    /**
     * Release all medium locks.
     */
    int Unlock();

private:
    std::list<MediumLock> mMediumLocks;
    bool mIsLocked;
}

/**
 * Medium lock list map. Meant for storing a collection of lock lists.
 * The usual use case is creating such a map when locking all medium chains
 * belonging to one VM, however that's not the limit. Be creative.
 */
class MediumLockListMap
{
public:

    /**
     * Default medium lock list map constructor.
     */
    MediumLockListMap();

    /**
     * Default medium lock list destructor.
     */
    ~MediumLockListMap();

private:
    std::map<ComObjPtr<Medium>, MediumLockList *> mMediumLocks;
}

#endif /* !____H_MEDIUMLOCK */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
