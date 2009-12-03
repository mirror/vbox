/** @file
 *
 * List of COM objects
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ____H_OBJECTSLIST
#define ____H_OBJECTSLIST

#include <list>
#include <VBox/com/ptr.h>

/**
 * Implements a "flat" objects list with a lock. Since each such list
 * has its own lock it is not a good idea to implement trees with this.
 *
 * ObjectList<T> is designed to behave as if it were a std::list of
 * COM pointers of class T; in other words,
 * ObjectList<Medium> behaves like std::list< ComObjPtr<Medium> > but
 * it's less typing. Iterators, front(), size(), begin() and end()
 * are implemented.
 *
 * In addition it automatically includes an RWLockHandle which can be
 * accessed with getLockHandle().
 *
 * If you need the raw std::list for some reason you can access it with
 * getList().
 *
 * The destructor automatically calls uninit() on every contained
 * COM object. If this is not desired, clear the member list before
 * deleting the list object.
 */
template<typename T>
class ObjectsList
{
public:
    typedef ComObjPtr<T, ComStrongRef> MyType;
    typedef std::list<MyType> MyList;

    typedef typename MyList::iterator iterator;
    typedef typename MyList::const_iterator const_iterator;
        // typename is necessary to de-ambiguate "::iterator" in templates; see
        // the "this might hurt your head" part in
        // http://www.parashift.com/c++-faq-lite/templates.html#faq-35.18

    ObjectsList()
    { }

    ~ObjectsList()
    {
        uninitAll();
    }

    /**
     * Returns the lock handle which protects this list, for use with
     * AutoReadLock or AutoWriteLock.
     */
    RWLockHandle& getLockHandle()
    {
        return m_lock;
    }

    /**
     * Calls m_ll.push_back(p) with locking.
     * @param p
     */
    void addChild(MyType p)
    {
        AutoWriteLock al(m_lock);
        m_ll.push_back(p);
    }

    /**
     * Calls m_ll.remove(p) with locking. Does NOT call uninit()
     * on the contained object.
     * @param p
     */
    void removeChild(MyType p)
    {
        AutoWriteLock al(m_lock);
        m_ll.remove(p);
    }

    /**
     * Appends all objects from another list to the member list.
     * Locks the other list for reading but does not lock "this"
     * (because it might be on the caller's stack and needs no
     * locking).
     * @param ll
     */
    void appendOtherList(ObjectsList<T> &ll)
    {
        AutoReadLock alr(ll.getLockHandle());
        for (const_iterator it = ll.begin();
             it != ll.end();
             ++it)
        {
            m_ll.push_back(*it);
        }
    }

    /**
     * Returns the no. of objects on the list (std::list compatibility)
     * with locking.
     */
    size_t size()
    {
        AutoReadLock al(m_lock);
        return m_ll.size();
    }

    /**
     * Returns the first object on the list (std::list compatibility)
     * with locking.
     */
    MyType front()
    {
        AutoReadLock al(m_lock);
        return m_ll.front();
    }

    /**
     * Returns a raw pointer to the member list of objects.
     * Does not lock!
     * @return
     */
    MyList& getList()
    {
        return m_ll;
    }

    /**
     * Returns the begin iterator from the list (std::list compatibility).
     * Does not lock!
     * @return
     */
    iterator begin()
    {
        return m_ll.begin();
    }

    /**
     * Returns the end iterator from the list (std::list compatibility).
     * Does not lock!
     */
    iterator end()
    {
        return m_ll.end();
    }

    /**
     * Calls uninit() on every COM object on the list and then
     * clears the list, with locking.
     */
    void uninitAll()
    {
        AutoWriteLock al(m_lock);
        for (iterator it = m_ll.begin();
             it != m_ll.end();
             ++it)
        {
            MyType &q = *it;
            q->uninit();
        }
        m_ll.clear();
    }

private:
    MyList          m_ll;
    RWLockHandle    m_lock;
};

#endif
