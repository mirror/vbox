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

#ifndef ___iprt_autores_h
#define ___iprt_autores_h

#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/assert.h>


/**
 * A simple class used to prevent copying and assignment.
 *
 * Inherit from this class in order to prevent automatic generation
 * of the copy constructor and assignment operator in your class.
 */
class RTCNonCopyable
{
protected:
    RTCNonCopyable() {}
    ~RTCNonCopyable() {}
private:
    RTCNonCopyable(RTCNonCopyable const &);
    RTCNonCopyable const &operator=(RTCNonCopyable const &);
};


/**
 * A callable class template which returns the correct value against which an
 * IPRT type must be compared to see if it is invalid.
 *
 * @warning This template *must* be specialised for the types it is to work with.
 */
template <class T>
inline T RTAutoResNil(void)
{
    AssertFatalMsgFailed(("Unspecialized template!\n"));
    return (T)0;
}


/**
 * A function template which calls the correct destructor for an IPRT type.
 *
 * @warning This template *must* be specialised for the types it is to work with.
 */
template <class T>
inline void RTAutoResDestruct(T aHandle)
{
    AssertFatalMsgFailed(("Unspecialized template!\n"));
    NOREF(aHandle);
}


/**
 * An auto pointer-type class for resources which take a C-style destructor
 * (RTMemFree() or equivalent).
 *
 * The idea of this class is to manage resources which the current code is
 * responsible for freeing.  By wrapping the resource in an RTAutoRes, you
 * ensure that the resource will be freed when you leave the scope in which
 * the RTAutoRes is defined, unless you explicitly release the resource.
 *
 * A typical use case is when a function is allocating a number of resources.
 * If any single allocation fails then all other resources must be freed.  If
 * all allocations succeed, then the resources should be returned to the
 * caller.  By placing all allocated resources in RTAutoRes containers, you
 * ensure that they will be freed on failure, and only have to take care of
 * releasing them when you return them.
 *
 * @param   T           The type of the resource.
 * @param   Destruct    The function to be used to free the resource.
 *                      This is *not* optional, the default is there for
 *                      working around compiler issues (?).
 * @param   NilRes      The function returning the NIL value for T. Required.
 *                      This is *not* optional, the default is there for
 *                      working around compiler issues (?).
 *
 * @note    The class can not be initialised directly using assignment, due
 *          to the lack of a copy constructor. This is intentional.
 */
template <class T, void Destruct(T) = RTAutoResDestruct<T>, T NilRes(void) = RTAutoResNil<T> >
class RTAutoRes
    : public RTCNonCopyable
{
protected:
    /** The resource handle. */
    T m_hRes;

public:
    /**
     * Constructor
     *
     * @param   a_hRes      The handle to resource to manage. Defaults to NIL.
     */
    RTAutoRes(T a_hRes = NilRes())
        : m_hRes(a_hRes)
    {
    }

    /**
     * Destructor.
     *
     * This destroys any resource currently managed by the object.
     */
    ~RTAutoRes()
    {
        if (m_hRes != NilRes())
            Destruct(m_hRes);
    }

    /**
     * Assignment from a value.
     *
     * This destroys any resource currently managed by the object
     * before taking on the new one.
     *
     * @param   a_hRes      The handle to the new resource.
     */
    RTAutoRes &operator=(T a_hRes)
    {
        if (m_hRes != NilRes())
            Destruct(m_hRes);
        m_hRes = a_hRes;
        return *this;
    }

    /**
     * Checks if the resource handle is NIL or not.
     */
    bool operator!()
    {
        return m_hRes == NilRes();
    }

    /**
     * Give up ownership the current resource, handing it to the caller.
     *
     * @returns The current resource handle.
     *
     * @note    Nothing happens to the resource when the object goes out of scope.
     */
    T release(void)
    {
        T Tmp = m_hRes;
        m_hRes = NilRes();
        return Tmp;
    }

    /**
     * Deletes the current resources.
     *
     * @param   a_hRes      Handle to a new resource to manage. Defaults to NIL.
     */
    void reset(T a_hRes = NilRes())
    {
        if (a_hRes != m_hRes)
        {
            if (m_hRes != NilRes())
                Destruct(m_hRes);
            m_hRes = a_hRes;
        }
    }

    /**
     * Get the raw resource handle.
     *
     * Typically used passing the handle to some IPRT function while
     * the object remains in scope.
     *
     * @returns The raw resource handle.
     */
    T get(void)
    {
        return m_hRes;
    }
};

#endif

