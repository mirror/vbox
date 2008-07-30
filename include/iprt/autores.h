/** @file
 * IPRT - C++ Extensions: resource lifetime management
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ___iprt_autores___
# define ___iprt_autores___

#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/cpputils.h>
#include <VBox/log.h>

/**
 * A callable class template which returns the correct value against which an
 * IPRT type must be compared to see if it is invalid.
 * @note this template must be specialised for the types it is to work with.
 */
template <class T>
inline T RTResNul(void)
{ AssertLogRelMsgFailedReturn (("Unspecialized template!\n"), (T) 0); }

/**
 * A function template which calls the correct destructor for an IPRT type.
 * @note this template must be specialised for the types it is to work with.
 */
template <class T> inline void RTResDestruct(T aHandle)
{ AssertLogRelMsgFailedReturn (("Unspecialized template!\n"), (T) 0); }

/** 
 * An auto pointer-type class for resources which take a C-style destructor
 * (RTMemFree() or equivalent).
 *
 * The idea of this class is to manage resources which the current code is
 * responsible for freeing.  By wrapping the resource in an RTAutoRes, you
 * ensure that the resource will be freed when you leave the scope in which
 * the RTAutoRes is defined, unless you explicitly release the resource again.
 *
 * A typical use case is when a function is allocating a number of resources.
 * If any single allocation fails then all other resources must be freed.  If
 * all allocations succeed, then the resources should be returned to the
 * caller.  By placing all allocated resources in RTAutoRes containers, you
 * ensure that they will be freed on failure, and only have to take care of
 * releasing them when you return them.
 * @param T         the type of the resource
 * @param Destruct  the function to be used to free the resource
 * @note The class can not be initialised directly using assignment, due to
 *       the lack of a copy constructor.
 */
template <class T, void Destruct(T) = RTResDestruct<T>, T NulRes(void) = RTResNul<T> >
class RTAutoRes : public stdx::non_copyable
{
private:
    /** The actual resource value */
    T mValue;
public:
    /** Constructor */
    RTAutoRes(T aValue = NulRes()) { mValue = aValue; }

    /** Destructor */
    ~RTAutoRes() { if (mValue != NulRes()) Destruct(mValue); }

    /** Assignment from a value. */
    RTAutoRes& operator=(T aValue)
    {
        if (mValue != NulRes())
        {
            Destruct(mValue);
        }
        mValue = aValue;
        return *this;
    }

    bool operator!() { return (NulRes() == mValue); }

    /** release method to get the pointer's value and "reset" the pointer. */
    T release(void) { T aTmp = mValue; mValue = NulRes(); return aTmp; }

    /** reset the pointer value to zero or to another pointer. */
    void reset(T aValue = NulRes()) { if (aValue != mValue)
    { Destruct(mValue); mValue = aValue; } }

    /** Accessing the value inside. */
    T get(void) { return mValue; }
};

/** 
 * Inline wrapper around RTMemFree to correct the signature.  We can't use a
 * more complex template here, because the G++ on RHEL 3 chokes on it with an
 * internal compiler error.
 */
template <class T>
void RTMemAutoFree(T *pMem) { RTMemFree(pMem); }

template <class T>
T *RTMemAutoNul(void) { return (T *)(NULL); }

/** 
 * An auto pointer-type class for structures allocated using C-like APIs.
 *
 * The idea of this class is to manage resources which the current code is
 * responsible for freeing.  By wrapping the resource in an RTAutoRes, you
 * ensure that the resource will be freed when you leave the scope in which
 * the RTAutoRes is defined, unless you explicitly release the resource again.
 *
 * A typical use case is when a function is allocating a number of resources.
 * If any single allocation fails then all other resources must be freed.  If
 * all allocations succeed, then the resources should be returned to the
 * caller.  By placing all allocated resources in RTAutoRes containers, you
 * ensure that they will be freed on failure, and only have to take care of
 * releasing them when you return them.
 * @param T         the type of the resource
 * @param Destruct  the function to be used to free the resource
 */
template <class T, void Destruct(T *) = RTMemAutoFree<T>,
          void *Reallocator(void *, size_t) = RTMemRealloc >
class RTMemAutoPtr : public RTAutoRes <T *, Destruct, RTMemAutoNul<T> >
{
public:
    /** Constructor */
    RTMemAutoPtr(T *aValue = NULL) : RTAutoRes <T *, Destruct, RTMemAutoNul<T> >(aValue) {}

    RTMemAutoPtr& operator=(T *aValue)
    { this->RTAutoRes <T *, Destruct, RTMemAutoNul<T> >::operator=(aValue); return *this; }

    /** Dereference with * operator. */
    T &operator*() { return *this->get(); }

    /** Dereference with -> operator. */
    T* operator->() { return this->get(); }

    /** Dereference with [] operator. */
    T &operator[](size_t i) { return this->get()[i]; }

    /**
     * Reallocate the resource value.  Free the old value if allocation fails.
     * @returns true if the new allocation succeeds, false otherwise
     * @param   Reallocator  the function to be used for reallocating the
     *                       resource.  It should have equivalent semantics to
     *                       C realloc.
     * @note We can overload this member for other reallocator signatures as
     *       needed.
     */
    bool realloc(size_t cchNewSize)
    {
        T *aNewValue = reinterpret_cast<T *>(Reallocator(this->get(), cchNewSize));
        if (aNewValue != NULL)
            this->release();
        /* We want this both if aNewValue is non-NULL and if it is NULL. */
        this->reset(aNewValue);
        return (aNewValue != NULL);
    }
};

#endif /* ___iprt_autores___ not defined */
