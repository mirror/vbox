/** @file
 * IPRT - C++ Representational State Transfer (REST) Array Template Class.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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

#ifndef ___iprt_cpp_restarray_h
#define ___iprt_cpp_restarray_h

#include <iprt/cpp/restbase.h>


/** @defgroup grp_rt_cpp_restarray  C++ Representational State Transfer (REST) Array Template Class.
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Abstract base class for the RTCRestArray template.
 */
class RT_DECL_CLASS RTCRestArrayBase : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestArrayBase();
    /** Destructor. */
    virtual ~RTCRestArrayBase();

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /**
     * Clear the content of the map.
     */
    void clear();

    /**
     * Check if an list contains any items.
     *
     * @return   True if there is more than zero items, false otherwise.
     */
    bool isEmpty() const
    {
        return m_cElements == 0;
    }

    /**
     * Gets the number of entries in the map.
     */
    size_t size() const
    {
        return m_cElements;
    }

    /**
     * Returns the base object pointer at a given index.
     *
     * @returns The base object at @a a_idx, NULL if out of range.
     * @param   a_idx           The array index.
     */
    RTCRestObjectBase *atBase(size_t a_idx)
    {
        if (a_idx < m_cElements)
            return m_papElements[a_idx];
        return NULL;
    }

    /**
     * Returns the const base object pointer at a given index.
     *
     * @returns The base object at @a a_idx, NULL if out of range.
     * @param   a_idx           The array index.
     */
    RTCRestObjectBase const *atBase(size_t a_idx) const
    {
        if (a_idx < m_cElements)
            return m_papElements[a_idx];
        return NULL;
    }

    /**
     * Removes the element at @a a_idx.
     * @returns true if @a a_idx is valid, false if out of range.
     * @param   a_idx       The index of the element to remove.
     *                      The value ~(size_t)0 is an alias for the final element.
     */
    bool removeAt(size_t a_idx);

    /**
     * Makes sure the array can hold at the given number of entries.
     *
     * @returns VINF_SUCCESS or VERR_NO_MEMORY.
     * @param   a_cEnsureCapacity   The number of elements to ensure capacity to hold.
     */
    int ensureCapacity(size_t a_cEnsureCapacity);


protected:
    /** The array. */
    RTCRestObjectBase **m_papElements;
    /** Number of elements in the array. */
    size_t              m_cElements;
    /** The number of elements m_papElements can hold.
     * The difference between m_cCapacity and m_cElements are all NULLs. */
    size_t              m_cCapacity;

    /**
     * Wrapper around the value constructor.
     *
     * @returns Pointer to new value object on success, NULL if out of memory.
     */
    virtual RTCRestObjectBase *createValue(void) = 0;

    /**
     * Wrapper around the value copy constructor.
     *
     * @returns Pointer to copy on success, NULL if out of memory.
     * @param   a_pSrc      The value to copy.
     */
    virtual RTCRestObjectBase *createValueCopy(RTCRestObjectBase const *a_pSrc) = 0;

    /**
     * Worker for the copy constructor and the assignment operator.
     *
     * This will use createEntryCopy to do the copying.
     *
     * @returns VINF_SUCCESS on success, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_rThat     The array to copy.  Caller makes 100% sure the it has
     *                      the same type as the destination.
     * @param   a_fThrow    Whether to throw error.
     */
    int copyArrayWorker(RTCRestArrayBase const &a_rThat, bool a_fThrow);

    /**
     * Worker for performing inserts.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_idx           Where to insert it.  The value ~(size_t)0 is an alias for m_cElements.
     * @param   a_pValue        The value to insert.  Ownership is transferred to the map on success.
     * @param   a_fReplace      Whether to replace existing entry rather than insert.
     */
    int insertWorker(size_t a_idx, RTCRestObjectBase *a_pValue, bool a_fReplace);

    /**
     * Worker for performing inserts.
     *
     * @returns VINF_SUCCESS or VWRN_ALREADY_EXISTS on success.
     *          VERR_ALREADY_EXISTS, VERR_NO_MEMORY or VERR_NO_STR_MEMORY on failure.
     * @param   a_idx           Where to insert it.  The value ~(size_t)0 is an alias for m_cElements.
     * @param   a_rValue        The value to copy into the map.
     * @param   a_fReplace      Whether to replace existing key-value pair with matching key.
     */
    int insertCopyWorker(size_t a_idx, RTCRestObjectBase const &a_rValue, bool a_fReplace);

private:
    /** Copy constructor on this class should never be used. */
    RTCRestArrayBase(RTCRestArrayBase const &a_rThat);
    /** Copy assignment operator on this class should never be used. */
    RTCRestArrayBase &operator=(RTCRestArrayBase const &a_rThat);
};



/**
 * Limited array class.
 */
template<class ElementType> class RTCRestArray : public RTCRestArrayBase
{
public:
    /** Default constructor - empty array. */
    RTCRestArray()
        : RTCRestArrayBase()
    {
    }

    /** Destructor. */
    ~RTCRestArray()
    {
    }

    /** Copy constructor. */
    RTCRestArray(RTCRestArray const &a_rThat)
        : RTCRestArrayBase()
    {
        copyArrayWorker(a_rThat, true /*fThrow*/);
    }

    /** Copy assignment operator. */
    RTCRestArray &operator=(RTCRestArray const &a_rThat)
    {
        copyArrayWorker(a_rThat, true /*fThrow*/);
        return *this;
    }

    /** Safe copy assignment method. */
    int assignCopy(RTCRestArray const &a_rThat)
    {
        return copyArrayWorker(a_rThat, false /*fThrow*/);
    }

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void)
    {
        return new (std::nothrow) RTCRestArray<ElementType>();
    }

    /** Factory method for elements. */
    static DECLCALLBACK(RTCRestObjectBase *) createElementInstance(void)
    {
        return new (std::nothrow) ElementType();
    }


    /**
     * Insert the given object at the specified index.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_INVALID_POINTER, VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_idx           The insertion index.  ~(size_t)0 is an alias for the end.
     * @param   a_pThat         The object to insert.  The array takes ownership of the object on success.
     */
    int insert(size_t a_idx, ElementType *a_pThat)
    {
        return insertWorker(a_idx, a_pThat, false /*a_fReplace*/);
    }

    /**
     * Insert a copy of the object at the specified index.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_idx           The insertion index.  ~(size_t)0 is an alias for the end.
     * @param   a_rThat         The object to insert a copy of.
     */
    int insertCopy(size_t a_idx, ElementType const &a_rThat)
    {
        return insertCopyWorker(a_idx, a_rThat, false /*a_fReplace*/);
    }

    /**
     * Appends the given object to the array.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_INVALID_POINTER, VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_pThat         The object to insert.  The array takes ownership of the object on success.
     */
    int append(ElementType *a_pThat)
    {
        return insertWorker(~(size_t)0, a_pThat, false /*a_fReplace*/);
    }

    /**
     * Appends a copy of the object at the specified index.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_rThat         The object to insert a copy of.
     */
    int appendCopy(ElementType const &a_rThat)
    {
        return insertCopyWorker(~(size_t)0, a_rThat, false /*a_fReplace*/);
    }

    /**
     * Prepends the given object to the array.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_INVALID_POINTER, VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_pThat         The object to insert.  The array takes ownership of the object on success.
     */
    int prepend(ElementType *a_pThat)
    {
        return insertWorker(0, a_pThat, false /*a_fReplace*/);
    }

    /**
     * Prepends a copy of the object at the specified index.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_rThat         The object to insert a copy of.
     */
    int prependCopy(ElementType const &a_rThat)
    {
        return insertCopyWorker(0, a_rThat, false /*a_fReplace*/);
    }

    /**
     * Insert the given object at the specified index.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_INVALID_POINTER, VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_idx           The index of the existing object to replace.
     * @param   a_pThat         The replacement object.  The array takes ownership of the object on success.
     */
    int replace(size_t a_idx, ElementType *a_pThat)
    {
        return insertWorker(a_idx, a_pThat, true /*a_fReplace*/);
    }

    /**
     * Insert a copy of the object at the specified index.
     *
     * @returns VINF_SUCCESS on success.
     *          VERR_NO_MEMORY, VERR_NO_STR_MEMORY or VERR_OUT_OF_RANGE on failure.
     * @param   a_idx           The index of the existing object to replace.
     * @param   a_rThat         The object to insert a copy of.
     */
    int replaceCopy(size_t a_idx, ElementType const &a_rThat)
    {
        return insertCopyWorker(a_idx, a_rThat, true /*a_fReplace*/);
    }

    /**
     * Returns the object at a given index.
     *
     * @returns The object at @a a_idx, NULL if out of range.
     * @param   a_idx           The array index.
     */
    ElementType *at(size_t a_idx)
    {
        if (a_idx < m_cElements)
            return (ElementType *)m_papElements[a_idx];
        return NULL;
    }

    /**
     * Returns the object at a given index, const variant.
     *
     * @returns The object at @a a_idx, NULL if out of range.
     * @param   a_idx           The array index.
     */
    ElementType const *at(size_t a_idx) const
    {
        if (a_idx < m_cElements)
            return (ElementType const *)m_papElements[a_idx];
        return NULL;
    }

    /**
     * Returns the first object in the array.
     * @returns The first object, NULL if empty.
     */
    ElementType *first()
    {
        return at(0);
    }

    /**
     * Returns the first object in the array, const variant.
     * @returns The first object, NULL if empty.
     */
    ElementType const *first() const
    {
        return at(0);
    }

    /**
     * Returns the last object in the array.
     * @returns The last object, NULL if empty.
     */
    ElementType *last()
    {
        return at(m_cElements - 1);
    }

    /**
     * Returns the last object in the array, const variant.
     * @returns The last object, NULL if empty.
     */
    ElementType const *last() const
    {
        return at(m_cElements - 1);
    }


protected:
    virtual RTCRestObjectBase *createValue(void) RT_OVERRIDE
    {
        return new (std::nothrow) ElementType();
    }

    virtual RTCRestObjectBase *createValueCopy(RTCRestObjectBase const *a_pSrc) RT_OVERRIDE
    {
        ElementType *pCopy = new (std::nothrow) ElementType();
        if (pCopy)
        {
            int rc = pCopy->assignCopy(*(ElementType const *)a_pSrc);
            if (RT_SUCCESS(rc))
                return pCopy;
            delete pCopy;
        }
        return NULL;
    }
};


/** @} */

#endif

