/** @file
 * IPRT - C++ Memory Resource Management.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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

#ifndef ___iprt_cpp_mem_h
#define ___iprt_cpp_mem_h

#include <iprt/cpp/autores.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h> /* for memset */

/** @defgroup grp_rt_cpp_autores_mem   C++ Memory Resource Management
 * @ingroup grp_rt_cpp_autores
 * @{
 */

/**
 * Template function wrapping RTMemFree to get the correct a_fnDestruct
 * signature for RTCAutoRes.
 *
 * We can't use a more complex template here, because the g++ on RHEL 3
 * chokes on it with an internal compiler error.
 *
 * @tparam  T           The data type that's being managed.
 * @param   aMem        Pointer to the memory that should be free.
 */
template <class T>
inline void RTCMemAutoDestructor(T *aMem) RT_NO_THROW
{
    RTMemFree(aMem);
}


/**
 * RTCMemAutoPtr allocator which uses RTMemTmpAlloc().
 *
 * @returns Allocated memory on success, NULL on failure.
 * @param   pvOld       What to reallocate, shall always be NULL.
 * @param   cbNew       The amount of memory to allocate (in bytes).
 */
inline void *RTCMemTmpAutoAllocator(void *pvOld, size_t cbNew) RT_NO_THROW
{
    AssertReturn(!pvOld, NULL);
    return RTMemTmpAlloc(cbNew);
}


/**
 * Template function wrapping RTMemTmpFree to get the correct a_fnDestruct
 * signature for RTCAutoRes.
 *
 * We can't use a more complex template here, because the g++ on RHEL 3
 * chokes on it with an internal compiler error.
 *
 * @tparam  T           The data type that's being managed.
 * @param   aMem        Pointer to the memory that should be free.
 */
template <class T>
inline void RTCMemTmpAutoDestructor(T *aMem) RT_NO_THROW
{
    RTMemTmpFree(aMem);
}


/**
 * Template function wrapping RTMemEfFree to get the correct a_fnDestruct
 * signature for RTCAutoRes.
 *
 * We can't use a more complex template here, because the g++ on RHEL 3
 * chokes on it with an internal compiler error.
 *
 * @tparam  T           The data type that's being managed.
 * @param   aMem        Pointer to the memory that should be free.
 */
template <class T>
inline void RTCMemEfAutoFree(T *aMem) RT_NO_THROW
{
    RTMemEfFreeNP(aMem);
}


/**
 * Template function wrapping NULL to get the correct NilRes signature
 * for RTCAutoRes.
 *
 * @tparam  T           The data type that's being managed.
 * @returns NULL with the right type.
 */
template <class T>
inline T *RTCMemAutoNil(void) RT_NO_THROW
{
    return (T *)(NULL);
}


/**
 * An auto pointer-type template class for managing memory allocating
 * via C APIs like RTMem (the default).
 *
 * The main purpose of this class is to automatically free memory that
 * isn't explicitly used (release()'ed) when the object goes out of scope.
 *
 * As an additional service it can also make the allocations and
 * reallocations for you if you like, but it can also take of memory
 * you hand it.
 *
 * @tparam  T               The data type to manage allocations for.
 * @tparam  a_fnDestruct    The function to be used to free the resource.
 *                          This will default to RTMemFree.
 * @tparam  a_fnAllocator   The function to be used to allocate or reallocate
 *                          the managed memory.
 *                          This is standard realloc() like stuff, so it's
 *                          possible to support simple allocation without
 *                          actually having to support reallocating memory if
 *                          that's a problem. This will default to
 *                          RTMemRealloc.
 */
template <class T,
          void a_fnDestruct(T *) = RTCMemAutoDestructor<T>,
# if defined(RTMEM_WRAP_TO_EF_APIS) && !defined(RTMEM_NO_WRAP_TO_EF_APIS)
          void *a_fnAllocator(void *, size_t, const char *) = RTMemEfReallocNP
# else
          void *a_fnAllocator(void *, size_t, const char *) = RTMemReallocTag
# endif
          >
class RTCMemAutoPtr
    : public RTCAutoRes<T *, a_fnDestruct, RTCMemAutoNil<T> >
{
public:
    /**
     * Constructor.
     *
     * @param   aPtr    Memory pointer to manage. Defaults to NULL.
     */
    RTCMemAutoPtr(T *aPtr = NULL)
        : RTCAutoRes<T *, a_fnDestruct, RTCMemAutoNil<T> >(aPtr)
    {
    }

    /**
     * Constructor that allocates memory.
     *
     * @param   a_cElements The number of elements (of the data type) to allocate.
     * @param   a_fZeroed   Whether the memory should be memset with zeros after
     *                      the allocation. Defaults to false.
     */
    RTCMemAutoPtr(size_t a_cElements, bool a_fZeroed = false)
        : RTCAutoRes<T *, a_fnDestruct, RTCMemAutoNil<T> >((T *)a_fnAllocator(NULL, a_cElements * sizeof(T), RTMEM_TAG))
    {
        if (a_fZeroed && RT_LIKELY(this->get() != NULL))
            memset(this->get(), '\0', a_cElements * sizeof(T));
    }

    /**
     * Free current memory and start managing aPtr.
     *
     * @param   aPtr    Memory pointer to manage.
     */
    RTCMemAutoPtr &operator=(T *aPtr)
    {
        this->RTCAutoRes<T *, a_fnDestruct, RTCMemAutoNil<T> >::operator=(aPtr);
        return *this;
    }

    /**
     * Dereference with * operator.
     */
    T &operator*()
    {
         return *this->get();
    }

    /**
     * Dereference with -> operator.
     */
    T *operator->()
    {
        return this->get();
    }

    /**
     * Accessed with the subscript operator ([]).
     *
     * @returns Reference to the element.
     * @param   a_i     The element to access.
     */
    T &operator[](size_t a_i)
    {
        return this->get()[a_i];
    }

    /**
     * Allocates memory and start manage it.
     *
     * Any previously managed memory will be freed before making
     * the new allocation.
     *
     * @returns Success indicator.
     * @retval  true if the new allocation succeeds.
     * @retval  false on failure, no memory is associated with the object.
     *
     * @param   a_cElements The number of elements (of the data type) to allocate.
     *                      This defaults to 1.
     * @param   a_fZeroed   Whether the memory should be memset with zeros after
     *                      the allocation. Defaults to false.
     */
    bool alloc(size_t a_cElements = 1, bool a_fZeroed = false)
    {
        this->reset(NULL);
        T *pNewMem = (T *)a_fnAllocator(NULL, a_cElements * sizeof(T), RTMEM_TAG);
        if (a_fZeroed && RT_LIKELY(pNewMem != NULL))
            memset(pNewMem, '\0', a_cElements * sizeof(T));
        this->reset(pNewMem);
        return pNewMem != NULL;
    }

    /**
     * Reallocate or allocates the memory resource.
     *
     * Free the old value if allocation fails.
     *
     * The content of any additional memory that was allocated is
     * undefined when using the default allocator.
     *
     * @returns Success indicator.
     * @retval  true if the new allocation succeeds.
     * @retval  false on failure, no memory is associated with the object.
     *
     * @param   a_cElements The new number of elements (of the data type) to
     *                      allocate. The size of the allocation is the number of
     *                      elements times the size of the data type - this is
     *                      currently what's passed down to the a_fnAllocator.
     *                      This defaults to 1.
     */
    bool realloc(size_t a_cElements = 1)
    {
        T *aNewValue = (T *)a_fnAllocator(this->get(), a_cElements * sizeof(T), RTMEM_TAG);
        if (RT_LIKELY(aNewValue != NULL))
            this->release();
        /* We want this both if aNewValue is non-NULL and if it is NULL. */
        this->reset(aNewValue);
        return aNewValue != NULL;
    }
};

/** @}  */

#endif

