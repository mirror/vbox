/** @file
 * IPRT - Generic List Class.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___iprt_cpp_list_h
#define ___iprt_cpp_list_h

#include <iprt/cpp/meta.h>
#include <iprt/mem.h>

#include <new> /* For std::bad_alloc */

namespace iprt
{

/**
 * @defgroup grp_rt_cpp_list   C++ List support
 * @ingroup grp_rt_cpp
 *
 * @brief  Generic C++ list class support.
 *
 * This list classes manage any amount of data in a fast and easy to use way.
 * They have no dependencies on STL, only on generic memory management methods
 * of IRPT. This allows list handling in situations where the use of STL
 * container classes is forbidden.
 *
 * Not all of the functionality of STL container classes is implemented. There
 * are no iterators or any other high level access/modifier methods (e.g.
 * std::algorithms).
 *
 * The implementation is array based which allows fast access to the items.
 * Appending items is usually also fast, cause the internal array is
 * preallocated. To minimize the memory overhead, native types (that is
 * everything smaller then the size of void*) are directly saved in the array.
 * If bigger types are used (e.g. iprt::MiniString) the internal array is an
 * array of pointers to the objects.
 *
 * The size of the internal array will usually not shrink, but grow
 * automatically. Only certain methods, like list::clear or the "=" operator
 * will reset any previously allocated memory. You can call list::setCapacity
 * for manual adjustment. If the size of an new list will be known, calling the
 * constructor with the necessary capacity will speed up the insertion of the
 * new items.
 *
 * For the full public interface these list classes offer see ListBase.
 *
 * There are some requirements for the types used which follow:
 * -# They need a default and a copy constructor.
 * -# If the type is some complex class (that is, having a constructor which
 *    allocates members on the heap) it has to be greater than sizeof(void*) to
 *    be used correctly. If this is not the case you can manually overwrite the
 *    list behavior. Just add T* as a second parameter to the list template if
 *    your class is called T. Another possibility is to specialize the list for
 *    your target class. See below for more information.
 *
 * The native types like int, bool, ptr, ..., are meeting this criteria, so
 * they are save to use.
 *
 * Implementation details:
 * It is possible to specialize any type. This might be necessary to get the
 * best speed out of the list. Examples are the 64-bit types, which use the
 * native (no pointers) implementation even on a 32-bit host. Consult the
 * source code for more details.
 *
 * Current specialized implementations:
 * - int64_t: iprt::list<int64_t>
 * - uint64_t: iprt::list<uint64_t>
 *
 * @{
 */

/**
 * General helper template for managing native values in ListBase.
 */
template <typename T1, typename T2>
class ListHelper
{
public:
    static inline void      set(T2 *p, size_t i, const T1 &v) { p[i] = v; }
    static inline T1 &      at(T2 *p, size_t i) { return p[i]; }
    static inline void      copyTo(T2 *p, T2 *const p1 , size_t iTo, size_t cSize)
    {
        if (cSize > 0)
            memcpy(&p[iTo], &p1[0], sizeof(T1) * cSize);
    }
    static inline void      erase(T2 *p, size_t /* i */) { /* Nothing to do here. */ }
    static inline void      eraseRange(T2 * /* p */, size_t /* cFrom */, size_t /* cSize */) { /* Nothing to do here. */ }
};

/**
 * Specialized helper template for managing pointer values in ListBase.
 */
template <typename T1>
class ListHelper<T1, T1*>
{
public:
    static inline void      set(T1 **p, size_t i, const T1 &v) { p[i] = new T1(v); }
    static inline T1 &      at(T1 **p, size_t i) { return *p[i]; }
    static inline void      copyTo(T1 **p, T1 **const p1 , size_t iTo, size_t cSize)
    {
        for (size_t i = 0; i < cSize; ++i)
            p[iTo + i] = new T1(*p1[i]);
    }
    static inline void      erase(T1 **p, size_t i) { delete p[i]; }
    static inline void      eraseRange(T1 **p, size_t cFrom, size_t cSize)
    {
        for (size_t i = cFrom; i < cFrom + cSize; ++i)
            delete p[i];
    }
};

/**
 * This is the base class for all other list classes. It implements the
 * necessary list functionality in a type independent way and offers the public
 * list interface to the user.
 */
template <class T, typename TYPE>
class ListBase
{
public:
    /**
     * Creates a new list.
     *
     * This preallocates @a cCapacity elements within the list.
     *
     * @param   cCapacitiy   The initial capacity the list has.
     * @throws  std::bad_alloc
     */
    ListBase(size_t cCapacity = DefaultCapacity)
      : m_pArray(0)
      , m_cSize(0)
      , m_cCapacity(0)
    {
        realloc_grow(cCapacity);
    }

    /**
     * Creates a copy of another list.
     *
     * The other list will be fully copied and the capacity will be the same as
     * the size if the other list.
     *
     * @param   other          The list to copy.
     * @throws  std::bad_alloc
     */
    ListBase(const ListBase<T, TYPE>& other)
      : m_pArray(0)
      , m_cSize(0)
      , m_cCapacity(0)
    {
        realloc_grow(other.m_cSize);
        ListHelper<T, list_type>::copyTo(m_pArray, other.m_pArray, 0, other.m_cSize);
        m_cSize = other.m_cSize;
    }

    /**
     * Destructor.
     */
    ~ListBase()
    {
        ListHelper<T, list_type>::eraseRange(m_pArray, 0, m_cSize);
        if (m_pArray)
            RTMemFree(m_pArray);
    }

    /**
     * Sets a new capacity within the list.
     *
     * If the new capacity is bigger than the old size, it will be simply
     * preallocated more space for the new items. If the new capacity is
     * smaller than the previous size, items at the end of the list will be
     * deleted.
     *
     * @param   cCapacity   The new capacity within the list.
     * @throws  std::bad_alloc
     */
    void setCapacity(size_t cCapacity) { realloc(cCapacity); }

    /**
     * Return the current capacity of the list.
     *
     * @return   The actual capacity.
     */
    size_t capacity() const { return m_cCapacity; }

    /**
     * Check if an list contains any items.
     *
     * @return   True if there is more than zero items, false otherwise.
     */
    bool isEmpty() const { return m_cSize == 0; }

    /**
     * Return the current count of elements within the list.
     *
     * @return   The current element count.
     */
    size_t size() const { return m_cSize; }

    /**
     * Inserts an item to the list at position @a i.
     *
     * @param   i     The position of the new item.
     * @param   val   The new item.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, TYPE> &insert(size_t i, const T &val)
    {
        if (m_cSize == m_cCapacity)
            realloc_grow(m_cCapacity + DefaultCapacity);
        memmove(&m_pArray[i + 1], &m_pArray[i], (m_cSize - i) * sizeof(list_type));
        ListHelper<T, list_type>::set(m_pArray, i, val);
        ++m_cSize;

        return *this;
    }

    /**
     * Prepend an item to the list.
     *
     * @param   val   The new item.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, TYPE> &prepend(const T &val)
    {
        return insert(0, val);
    }

    /**
     * Prepend a list of type T to the list.
     *
     * @param   other   The list to prepend.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, TYPE> &prepend(const ListBase<T, TYPE> &other)
    {
        if (m_cCapacity - m_cSize < other.m_cSize)
            realloc_grow(m_cCapacity + (other.m_cSize - (m_cCapacity - m_cSize)));
        memmove(&m_pArray[other.m_cSize], &m_pArray[0], m_cSize * sizeof(list_type));
        ListHelper<T, list_type>::copyTo(m_pArray, other.m_pArray, 0, other.m_cSize);
        m_cSize += other.m_cSize;

        return *this;
    }

    /**
     * Append an item to the list.
     *
     * @param   val   The new item.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, TYPE> &append(const T &val)
    {
        if (m_cSize == m_cCapacity)
            realloc_grow(m_cCapacity + DefaultCapacity);
        ListHelper<T, list_type>::set(m_pArray, m_cSize, val);
        ++m_cSize;

        return *this;
    }

    /**
     * Append a list of type T to the list.
     *
     * @param   other   The list to append.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, TYPE> &append(const ListBase<T, TYPE> &other)
    {
        if (m_cCapacity - m_cSize < other.m_cSize)
            realloc_grow(m_cCapacity + (other.m_cSize - (m_cCapacity - m_cSize)));
        ListHelper<T, list_type>::copyTo(m_pArray, other.m_pArray, m_cSize, other.m_cSize);
        m_cSize += other.m_cSize;

        return *this;
    }

    /**
     * Copy the items of the other list into this list. All previous items of
     * this list are deleted.
     *
     * @param   other   The list to copy.
     * @return  a reference to this list.
     */
    ListBase<T, TYPE> &operator=(const ListBase<T, TYPE>& other)
    {
        /* Prevent self assignment */
        if (this == &other) return *this;
        /* Values cleanup */
        ListHelper<T, list_type>::eraseRange(m_pArray, 0, m_cSize);
        /* Copy */
        if (other.m_cSize != m_cCapacity)
            realloc_grow(other.m_cSize);
        m_cSize = other.m_cSize;
        ListHelper<T, list_type>::copyTo(m_pArray, other.m_pArray, 0, other.m_cSize);

        return *this;
    }

    /**
     * Replace an item in the list.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i     The position of the item to replace.
     * @param   val   The new value.
     * @return  a reference to this list.
     */
    ListBase<T, TYPE> &replace(size_t i, const T &val)
    {
        ListHelper<T, list_type>::erase(m_pArray, i);
        ListHelper<T, list_type>::set(m_pArray, i, val);

        return *this;
    }

    /**
     * Return the first item as constant reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The first item.
     */
    const T &first() const
    {
        return ListHelper<T, list_type>::at(m_pArray, 0);
    }

    /**
     * Return the first item as mutable reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The first item.
     */
    T &first()
    {
        return ListHelper<T, list_type>::at(m_pArray, 0);
    }

    /**
     * Return the last item as constant reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The last item.
     */
    const T &last() const
    {
        return ListHelper<T, list_type>::at(m_pArray, m_cSize - 1);
    }

    /**
     * Return the last item as mutable reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The last item.
     */
    T &last()
    {
        return ListHelper<T, list_type>::at(m_pArray, m_cSize - 1);
    }

    /**
     * Return the item at position @a i as constant reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i     The position of the item to return.
     * @return  The item at position @a i.
     */
    const T &at(size_t i) const
    {
        return ListHelper<T, list_type>::at(m_pArray, i);
    }

    /**
     * Return the item at position @a i as mutable reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i     The position of the item to return.
     * @return   The item at position @a i.
     */
    T &at(size_t i)
    {
        return ListHelper<T, list_type>::at(m_pArray, i);
    }

    /**
     * Return the item at position @a i as mutable reference.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i     The position of the item to return.
     * @return   The item at position @a i.
     */
    T &operator[](size_t i)
    {
        return ListHelper<T, list_type>::at(m_pArray, i);
    }

    /**
     * Return the item at position @a i. If @a i isn't valid within the list a
     * default value is returned.
     *
     * @param   i              The position of the item to return.
     * @return  The item at position @a i.
     */
    T value(size_t i) const
    {
        if (i >= m_cSize)
            return T();
        return ListHelper<T, list_type>::at(m_pArray, i);
    }

    /**
     * Return the item at position @a i. If @a i isn't valid within the list
     * @a defaultVal is returned.
     *
     * @param   i              The position of the item to return.
     * @param   defaultVal     The value to return in case @a i is invalid.
     * @return  The item at position @a i.
     */
    T value(size_t i, const T &defaultVal) const
    {
        if (i >= m_cSize)
            return defaultVal;
        return ListHelper<T, list_type>::at(m_pArray, i);
    }

    /**
     * Remove the item at position @a i.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i   The position of the item to remove.
     */
    void removeAt(size_t i)
    {
        ListHelper<T, list_type>::erase(m_pArray, i);
        /* Not last element? */
        if (i < m_cSize - 1)
            memmove(&m_pArray[i], &m_pArray[i + 1], (m_cSize - i - 1) * sizeof(list_type));
        --m_cSize;
    }

    /**
     * Remove a range of items from the list.
     *
     * @note No boundary checks are done. Make sure @a iFrom is equal or
     *       greater zero and smaller than list::size. @a iTo has to be
     *       greater than @a iFrom and equal or smaller than list::size.
     *
     * @param   iFrom   The start position of the items to remove.
     * @param   iTo     The end position of the items to remove (excluded).
     */
    void removeRange(size_t iFrom, size_t iTo)
    {
        ListHelper<T, list_type>::eraseRange(m_pArray, iFrom, iTo - iFrom);
        /* Not last elements? */
        if (m_cSize - iTo > 0)
            memmove(&m_pArray[iFrom], &m_pArray[iTo], (m_cSize - iTo) * sizeof(list_type));
        m_cSize -= iTo - iFrom;
    }

    /**
     * Delete all items in the list.
     */
    void clear()
    {
        /* Values cleanup */
        ListHelper<T, list_type>::eraseRange(m_pArray, 0, m_cSize);
        if (m_cSize != DefaultCapacity)
            realloc_grow(DefaultCapacity);
        m_cSize = 0;
    }

    /**
     * The default capacity of the list. This is also used as grow factor.
     */
    static const size_t DefaultCapacity;
private:

    /**
      * Generic realloc, which does some kind of boundary checking.
     */
    void realloc(size_t cNewSize)
    {
        /* Same size? */
        if (cNewSize == m_cCapacity)
            return;
        /* If we get smaller we have to delete some of the objects at the end
           of the list. */
        if (   cNewSize < m_cSize
            && m_pArray)
        {
            ListHelper<T, list_type>::eraseRange(m_pArray, cNewSize, m_cSize - cNewSize);
            m_cSize -= m_cSize - cNewSize;
        }
        /* If we get zero we delete the array it self. */
        if (   cNewSize == 0
            && m_pArray)
        {
            RTMemFree(m_pArray);
            m_pArray = 0;
        }
        m_cCapacity = cNewSize;
        /* Resize the array. */
        if (cNewSize > 0)
        {
            m_pArray = static_cast<list_type*>(RTMemRealloc(m_pArray, sizeof(list_type) * cNewSize));
            if (!m_pArray)
            {
                m_cCapacity = 0;
                m_cSize = 0;
#ifdef RT_EXCEPTIONS_ENABLED
                throw std::bad_alloc();
#endif /* RT_EXCEPTIONS_ENABLED */
            }
        }
    }

    /**
     * Special realloc method which require that the array will grow.
     *
     * @note No boundary checks are done!
     */
    void realloc_grow(size_t cNewSize)
    {
        /* Resize the array. */
        m_cCapacity = cNewSize;
        m_pArray = static_cast<list_type*>(RTMemRealloc(m_pArray, sizeof(list_type) * cNewSize));
        if (!m_pArray)
        {
            m_cCapacity = 0;
            m_cSize = 0;
#ifdef RT_EXCEPTIONS_ENABLED
            throw std::bad_alloc();
#endif /* RT_EXCEPTIONS_ENABLED */
        }
    }

    /**
     * Which type of list should be created. This depends on the size of T. If
     * T is a native type (int, bool, ptr, ...), the list will contain the
     * values itself. If the size is bigger than the size of a void*, the list
     * contains pointers to the values. This could be specialized like for the
     * 64-bit integer types.
     */
    typedef TYPE list_type;

    /** The internal list array. */
    list_type *m_pArray;
    /** The current count of items in use. */
    size_t m_cSize;
    /** The current capacity of the internal array. */
    size_t m_cCapacity;
};

template <class T, typename TYPE>
const size_t ListBase<T, TYPE>::DefaultCapacity = 10;

/**
 * Template class which automatically determines the type of list to use.
 *
 * @see ListBase
 */
template <class T, typename TYPE = typename if_<(sizeof(T) > sizeof(void*)), T*, T>::result>
class list: public ListBase<T, TYPE> {};

/**
 * Specialization class for using the native type list for unsigned 64-bit
 * values even on a 32-bit host.
 *
 * @see ListBase
 */
template <>
class list<uint64_t>: public ListBase<uint64_t, uint64_t> {};

/**
 * Specialization class for using the native type list for signed 64-bit
 * values even on a 32-bit host.
 *
 * @see ListBase
 */
template <>
class list<int64_t>: public ListBase<int64_t, int64_t> {};

/** @} */

} /* namespace iprt */

#endif /* ___iprt_cpp_list_h */

