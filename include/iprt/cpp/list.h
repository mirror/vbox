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
#include <iprt/string.h> /* for memcpy */

#include <new> /* For std::bad_alloc */

namespace iprt
{

/** @defgroup grp_rt_cpp_list   C++ List support
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
 * If bigger types are used (e.g. RTCString) the internal array is an array of
 * pointers to the objects.
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
 * Please note that the return type of some of the getter methods are slightly
 * different depending on the list type. Native types return the item by value,
 * items with a size greater than sizeof(void*) by reference. As native types
 * saved directly in the internal array, returning a reference to them (and
 * saving them in a reference as well) would make them invalid (or pointing to
 * a wrong item) when the list is changed in the meanwhile. Returning a
 * reference for bigger types isn't problematic and makes sure we get out the
 * best speed of the list. The one exception to this rule is the index
 * operator[]. This operator always return a reference to make it possible to
 * use it as a lvalue. Its your responsibility to make sure the list isn't
 * changed when using the value as reference returned by this operator.
 *
 * The list class is reentrant. For a thread-safe variant see mtlist.
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
 * The guard definition.
 */
template <bool G>
class ListGuard;

/**
 * The default guard which does nothing.
 */
template <>
class ListGuard<false>
{
public:
    inline void enterRead() const {}
    inline void leaveRead() const {}
    inline void enterWrite() {}
    inline void leaveWrite() {}
};

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
template <class T, typename ITYPE, bool MT>
class ListBase
{
    /**
     * Defines the return type of most of the getter methods. If the internal
     * used type is a pointer, we return a reference. If not we return by
     * value.
     */
    typedef typename if_ptr<ITYPE, T&, T>::result GET_RTYPE;
    typedef typename if_ptr<ITYPE, const T&, const T>::result GET_CRTYPE;

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
    ListBase(const ListBase<T, ITYPE, MT>& other)
      : m_pArray(0)
      , m_cSize(0)
      , m_cCapacity(0)
    {
        realloc_grow(other.m_cSize);
        ListHelper<T, ITYPE>::copyTo(m_pArray, other.m_pArray, 0, other.m_cSize);
        m_cSize = other.m_cSize;
    }

    /**
     * Destructor.
     */
    ~ListBase()
    {
        ListHelper<T, ITYPE>::eraseRange(m_pArray, 0, m_cSize);
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
    void setCapacity(size_t cCapacity)
    {
        m_guard.enterWrite();
        realloc(cCapacity);
        m_guard.leaveWrite();
    }

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
    ListBase<T, ITYPE, MT> &insert(size_t i, const T &val)
    {
        m_guard.enterWrite();
        if (m_cSize == m_cCapacity)
            realloc_grow(m_cCapacity + DefaultCapacity);
        memmove(&m_pArray[i + 1], &m_pArray[i], (m_cSize - i) * sizeof(ITYPE));
        ListHelper<T, ITYPE>::set(m_pArray, i, val);
        ++m_cSize;
        m_guard.leaveWrite();

        return *this;
    }

    /**
     * Prepend an item to the list.
     *
     * @param   val   The new item.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, ITYPE, MT> &prepend(const T &val)
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
    ListBase<T, ITYPE, MT> &prepend(const ListBase<T, ITYPE, MT> &other)
    {
        m_guard.enterWrite();
        if (m_cCapacity - m_cSize < other.m_cSize)
            realloc_grow(m_cCapacity + (other.m_cSize - (m_cCapacity - m_cSize)));
        memmove(&m_pArray[other.m_cSize], &m_pArray[0], m_cSize * sizeof(ITYPE));
        ListHelper<T, ITYPE>::copyTo(m_pArray, other.m_pArray, 0, other.m_cSize);
        m_cSize += other.m_cSize;
        m_guard.leaveWrite();

        return *this;
    }

    /**
     * Append an item to the list.
     *
     * @param   val   The new item.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, ITYPE, MT> &append(const T &val)
    {
        m_guard.enterWrite();
        if (m_cSize == m_cCapacity)
            realloc_grow(m_cCapacity + DefaultCapacity);
        ListHelper<T, ITYPE>::set(m_pArray, m_cSize, val);
        ++m_cSize;
        m_guard.leaveWrite();

        return *this;
    }

    /**
     * Append a list of type T to the list.
     *
     * @param   other   The list to append.
     * @return  a reference to this list.
     * @throws  std::bad_alloc
     */
    ListBase<T, ITYPE, MT> &append(const ListBase<T, ITYPE, MT> &other)
    {
        m_guard.enterWrite();
        if (m_cCapacity - m_cSize < other.m_cSize)
            realloc_grow(m_cCapacity + (other.m_cSize - (m_cCapacity - m_cSize)));
        ListHelper<T, ITYPE>::copyTo(m_pArray, other.m_pArray, m_cSize, other.m_cSize);
        m_cSize += other.m_cSize;
        m_guard.leaveWrite();

        return *this;
    }

    /**
     * Copy the items of the other list into this list. All previous items of
     * this list are deleted.
     *
     * @param   other   The list to copy.
     * @return  a reference to this list.
     */
    ListBase<T, ITYPE, MT> &operator=(const ListBase<T, ITYPE, MT>& other)
    {
        /* Prevent self assignment */
        if (this == &other)
            return *this;

        m_guard.enterWrite();
        /* Values cleanup */
        ListHelper<T, ITYPE>::eraseRange(m_pArray, 0, m_cSize);

        /* Copy */
        if (other.m_cSize != m_cCapacity)
            realloc_grow(other.m_cSize);
        m_cSize = other.m_cSize;
        ListHelper<T, ITYPE>::copyTo(m_pArray, other.m_pArray, 0, other.m_cSize);
        m_guard.leaveWrite();

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
    ListBase<T, ITYPE, MT> &replace(size_t i, const T &val)
    {
        m_guard.enterWrite();
        ListHelper<T, ITYPE>::erase(m_pArray, i);
        ListHelper<T, ITYPE>::set(m_pArray, i, val);
        m_guard.leaveWrite();

        return *this;
    }

    /**
     * Return the first item as constant object.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The first item.
     */
    GET_CRTYPE first() const
    {
        m_guard.enterRead();
        GET_CRTYPE res = ListHelper<T, ITYPE>::at(m_pArray, 0);
        m_guard.leaveRead();
        return res;
    }

    /**
     * Return the first item.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The first item.
     */
    GET_RTYPE first()
    {
        m_guard.enterRead();
        GET_RTYPE res = ListHelper<T, ITYPE>::at(m_pArray, 0);
        m_guard.leaveRead();
        return res;
    }

    /**
     * Return the last item as constant object.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The last item.
     */
    GET_CRTYPE last() const
    {
        m_guard.enterRead();
        GET_CRTYPE res = ListHelper<T, ITYPE>::at(m_pArray, m_cSize - 1);
        m_guard.leaveRead();
        return res;
    }

    /**
     * Return the last item.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @return   The last item.
     */
    GET_RTYPE last()
    {
        m_guard.enterRead();
        GET_RTYPE res = ListHelper<T, ITYPE>::at(m_pArray, m_cSize - 1);
        m_guard.leaveRead();
        return res;
    }

    /**
     * Return the item at position @a i as constant object.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i     The position of the item to return.
     * @return  The item at position @a i.
     */
    GET_CRTYPE at(size_t i) const
    {
        m_guard.enterRead();
        GET_CRTYPE res = ListHelper<T, ITYPE>::at(m_pArray, i);
        m_guard.leaveRead();
        return res;
    }

    /**
     * Return the item at position @a i.
     *
     * @note No boundary checks are done. Make sure @a i is equal or greater zero
     *       and smaller than list::size.
     *
     * @param   i     The position of the item to return.
     * @return   The item at position @a i.
     */
    GET_RTYPE at(size_t i)
    {
        m_guard.enterRead();
        GET_RTYPE res = ListHelper<T, ITYPE>::at(m_pArray, i);
        m_guard.leaveRead();
        return res;
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
        m_guard.enterRead();
        T &res = ListHelper<T, ITYPE>::at(m_pArray, i);
        m_guard.leaveRead();
        return res;
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
        m_guard.enterRead();
        if (i >= m_cSize)
        {
            m_guard.leaveRead();
            return T();
        }
        T res = ListHelper<T, ITYPE>::at(m_pArray, i);
        m_guard.leaveRead();
        return res;
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
        m_guard.enterRead();
        if (i >= m_cSize)
        {
            m_guard.leaveRead();
            return defaultVal;
        }
        T res = ListHelper<T, ITYPE>::at(m_pArray, i);
        m_guard.leaveRead();
        return res;
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
        m_guard.enterWrite();
        ListHelper<T, ITYPE>::erase(m_pArray, i);
        /* Not last element? */
        if (i < m_cSize - 1)
            memmove(&m_pArray[i], &m_pArray[i + 1], (m_cSize - i - 1) * sizeof(ITYPE));
        --m_cSize;
        m_guard.leaveWrite();
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
        m_guard.enterWrite();
        ListHelper<T, ITYPE>::eraseRange(m_pArray, iFrom, iTo - iFrom);
        /* Not last elements? */
        if (m_cSize - iTo > 0)
            memmove(&m_pArray[iFrom], &m_pArray[iTo], (m_cSize - iTo) * sizeof(ITYPE));
        m_cSize -= iTo - iFrom;
        m_guard.leaveWrite();
    }

    /**
     * Delete all items in the list.
     */
    void clear()
    {
        m_guard.enterWrite();
        /* Values cleanup */
        ListHelper<T, ITYPE>::eraseRange(m_pArray, 0, m_cSize);
        if (m_cSize != DefaultCapacity)
            realloc_grow(DefaultCapacity);
        m_cSize = 0;
        m_guard.leaveWrite();
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
            ListHelper<T, ITYPE>::eraseRange(m_pArray, cNewSize, m_cSize - cNewSize);
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
            m_pArray = static_cast<ITYPE*>(RTMemRealloc(m_pArray, sizeof(ITYPE) * cNewSize));
            if (!m_pArray)
            {
                /** @todo you leak memory. */
                m_cCapacity = 0;
                m_cSize = 0;
#ifdef RT_EXCEPTIONS_ENABLED
                throw std::bad_alloc();
#endif
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
        m_pArray = static_cast<ITYPE*>(RTMemRealloc(m_pArray, sizeof(ITYPE) * cNewSize));
        if (!m_pArray)
        {
            /** @todo you leak memory. */
            m_cCapacity = 0;
            m_cSize = 0;
#ifdef RT_EXCEPTIONS_ENABLED
            throw std::bad_alloc();
#endif
        }
    }

    /** The internal list array. */
    ITYPE *m_pArray;
    /** The current count of items in use. */
    size_t m_cSize;
    /** The current capacity of the internal array. */
    size_t m_cCapacity;
    /** The guard used to serialize the access to the items. */
    ListGuard<MT> m_guard;
};

template <class T, typename ITYPE, bool MT>
const size_t ListBase<T, ITYPE, MT>::DefaultCapacity = 10;

/**
 * Template class which automatically determines the type of list to use.
 *
 * @see ListBase
 */
template <class T, typename ITYPE = typename if_<(sizeof(T) > sizeof(void*)), T*, T>::result>
class list : public ListBase<T, ITYPE, false> {};

/**
 * Specialized class for using the native type list for unsigned 64-bit
 * values even on a 32-bit host.
 *
 * @see ListBase
 */
template <>
class list<uint64_t>: public ListBase<uint64_t, uint64_t, false> {};

/**
 * Specialized class for using the native type list for signed 64-bit
 * values even on a 32-bit host.
 *
 * @see ListBase
 */
template <>
class list<int64_t>: public ListBase<int64_t, int64_t, false> {};

/** @} */

} /* namespace iprt */

#endif /* !___iprt_cpp_list_h */

