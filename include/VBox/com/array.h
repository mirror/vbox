/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Safe array helper class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_com_array_h
#define ___VBox_com_array_h

#include <VBox/com/ptr.h>

/** @defgroup   grp_COM_arrays    COM/XPCOM Arrays
 * @{
 *
 * The COM/XPCOM array support layer provides a cross-platform way to pass
 * arrays to and from COM interface methods and consists of the com::SafeArray
 * template and a set of ComSafeArray* macros part of which is defined in
 * VBox/com/defs.h.
 *
 * This layer works with interface attributes and method parameters that have
 * the 'safearray="yes"' attribute in the XIDL definition:
 * @code

    <interface name="ISomething" ...>

      <method name="testArrays">
        <param name="inArr" type="long" dir="in" safearray="yes"/>
        <param name="outArr" type="long" dir="out" safearray="yes"/>
        <param name="retArr" type="long" dir="return" safearray="yes"/>
      </method>

    </interface>

 * @endcode
 *
 * Methods generated from this and similar definitions are implemented in
 * component classes using the following declarations:
 * @code

    STDMETHOD(TestArrays) (ComSafeArrayIn (LONG, aIn),
                           ComSafeArrayOut (LONG, aOut),
                           ComSafeArrayOut (LONG, aRet));

 * @endcode
 *
 * And the following function bodies:
 * @code

    STDMETHODIMP Component::TestArrays (ComSafeArrayIn (LONG, aIn),
                                        ComSafeArrayOut (LONG, aOut),
                                        ComSafeArrayOut (LONG, aRet))
    {
        if (ComSafeArrayInIsNull (aIn))
            return E_INVALIDARG;
        if (ComSafeArrayOutIsNull (aOut))
            return E_POINTER;
        if (ComSafeArrayOutIsNull (aRet))
            return E_POINTER;

        // Use SafeArray to access the input array parameter

        com::SafeArray <LONG> in (ComSafeArrayInArg (aIn));

        for (size_t i = 0; i < in.size(); ++ i)
            LogFlow (("*** in[%u]=%d\n", i, in [i]));

        // Use SafeArray to create the return array (the same technique is used
        // for output array paramters)

        SafeArray <LONG> ret (in.size() * 2);
        for (size_t i = 0; i < in.size(); ++ i)
        {
            ret [i] = in [i];
            ret [i + in.size()] = in [i] * 10;
        }

        ret.detachTo (ComSafeArrayOutArg (aRet));

        return S_OK;
    }

 * @endcode
 *
 * Such methods can be called from the client code using the following pattern:
 * @code

    ComPtr <ISomething> component;

    // ...

    com::SafeArray <LONG> in (3);
    in [0] = -1;
    in [1] = -2;
    in [2] = -3;

    com::SafeArray <LONG> out;
    com::SafeArray <LONG> ret;

    HRESULT rc = component->TestArrays (ComSafeArrayAsInParam (in),
                                        ComSafeArrayAsOutParam (out),
                                        ComSafeArrayAsOutParam (ret));

    if (SUCCEEDED (rc))
        for (size_t i = 0; i < ret.size(); ++ i)
            printf ("*** ret[%u]=%d\n", i, ret [i]);

 * @endcode
 *
 * For interoperability with standard C++ containers, there is a template
 * constructor that takes such a container as argument and performs a deep copy
 * of its contents. This can be used in method implementations like this:
 * @code

    STDMETHODIMP Component::COMGETTER(Values) (ComSafeArrayOut (int, aValues))
    {
        // ... assume there is a |std::list <int> mValues| data member

        com::SafeArray <int> values (mValues);
        values.detachTo (ComSafeArrayOutArg (aValues));

        return S_OK;
    }

 * @endcode
 *
 * The current implementation of the SafeArray layer supports all types normally
 * allowed in XIDL as array element types (including 'wstring' and 'uuid').
 * However, 'pointer-to-...' types (e.g. 'long *', 'wstring *') are not
 * supported and therefore cannot be used as element types.
 *
 * Arrays of interface pointers are also supported but they require to use a
 * special SafeArray implementation, com::SafeIfacePointer, which takes the
 * interface class name as a template argument (e.g. com::SafeIfacePointer
 * <IUnknown>). This implementation functions identically to com::SafeArray.
 */

#if defined (VBOX_WITH_XPCOM)
#include <nsMemory.h>
#endif

#include "VBox/com/defs.h"
#include "VBox/com/assert.h"

#include "iprt/cpputils.h"

#if defined (VBOX_WITH_XPCOM)

/**
 * Wraps the given com::SafeArray instance to generate an expression that is
 * suitable for passing it to functions that take input safearray parameters
 * declared using the ComSafeArrayIn marco.
 *
 * @param aArray    com::SafeArray instance to pass as an input parameter.
 */
#define ComSafeArrayAsInParam(aArray)   \
    (aArray).size(), (aArray).__asInParam_Arr (aArray.raw())

/**
 * Wraps the given com::SafeArray instance to generate an expression that is
 * suitable for passing it to functions that take output safearray parameters
 * declared using the ComSafeArrayOut marco.
 *
 * @param aArray    com::SafeArray instance to pass as an output parameter.
 */
#define ComSafeArrayAsOutParam(aArray)  \
    (aArray).__asOutParam_Size(), (aArray).__asOutParam_Arr()

#else /* defined (VBOX_WITH_XPCOM) */

#define ComSafeArrayAsInParam(aArray)   (aArray).__asInParam()

#define ComSafeArrayAsOutParam(aArray)  (aArray).__asOutParam()

#endif /* defined (VBOX_WITH_XPCOM) */

/**
 *
 */
namespace com
{

#if defined (VBOX_WITH_XPCOM)

////////////////////////////////////////////////////////////////////////////////

/**
 * Contains various helper constants for SafeArray.
 */
template <typename T>
struct SafeArrayTraits
{
    static void Init (T &aElem) { aElem = 0; }
    static void Uninit (T &aElem) { aElem = 0; }
    static void Copy (const T &aFrom, T &aTo) { aTo = aFrom; }

    /* Magic to workaround strict rules of par. 4.4.4 of the C++ standard (that
     * in particular forbid casts of 'char **' to 'const char **'). Then initial
     * reason for this magic is that XPIDL declares input strings
     * (char/PRUnichar pointers) as const but doesn't do so for pointers to
     * arrays. */
    static T *__asInParam_Arr (T *aArr) { return aArr; }
    static T *__asInParam_Arr (const T *aArr) { return const_cast <T *> (aArr); }
};

template <typename T>
struct SafeArrayTraits <T *>
{
    // Arbitrary pointers are not supported
};

template<>
struct SafeArrayTraits <PRUnichar *>
{
    static void Init (PRUnichar * &aElem) { aElem = NULL; }
    static void Uninit (PRUnichar * &aElem)
    {
        if (aElem)
        {
            ::SysFreeString (aElem);
            aElem = NULL;
        }
    }

    static void Copy (const PRUnichar * aFrom, PRUnichar * &aTo)
    {
        AssertCompile (sizeof (PRUnichar) == sizeof (OLECHAR));
        aTo = aFrom ? ::SysAllocString ((const OLECHAR *) aFrom) : NULL;
    }

    /* Magic to workaround strict rules of par. 4.4.4 of the C++ standard */
    static const PRUnichar **__asInParam_Arr (PRUnichar **aArr)
    {
        return const_cast <const PRUnichar **> (aArr);
    }
    static const PRUnichar **__asInParam_Arr (const PRUnichar **aArr) { return aArr; }
};

#else /* defined (VBOX_WITH_XPCOM) */

////////////////////////////////////////////////////////////////////////////////

/**
 * Contains various helper constants for SafeArray.
 */
template <typename T>
struct SafeArrayTraits
{
    // Arbitrary types are not supported
};

template<>
struct SafeArrayTraits <LONG>
{
    static VARTYPE VarType() { return VT_I4; }
    static void Copy (LONG aFrom, LONG &aTo) { aTo = aFrom; }
};

template<>
struct SafeArrayTraits <ULONG>
{
    static VARTYPE VarType() { return VT_UI4; }
    static void Copy (ULONG aFrom, ULONG &aTo) { aTo = aFrom; }
};

template<>
struct SafeArrayTraits <BSTR>
{
    static VARTYPE VarType() { return VT_BSTR; }

    static void Copy (BSTR aFrom, BSTR &aTo)
    {
        aTo = aFrom ? ::SysAllocString ((const OLECHAR *) aFrom) : NULL;
    }
};

#endif /* defined (VBOX_WITH_XPCOM) */

////////////////////////////////////////////////////////////////////////////////

/**
 * The SafeArray class represents the safe array type used in COM to pass arrays
 * to/from interface methods.
 *
 * This helper class hides all MSCOM/XPCOM specific implementation details and,
 * together with ComSafeArrayIn, ComSafeArrayOut and ComSafeArrayRet macros,
 * provides a platform-neutral way to handle safe arrays in the method
 * implementation.
 *
 * When an instance of this class is destroyed, it automatically frees all
 * resources occupied by individual elements of the array as well as by the
 * array itself. However, when the value of an element is manually changed
 * using #operator[] or by acessing array data through the #raw() pointer, it is
 * the caller's responsibility to free resources occupied by the previous
 * element's value.
 *
 * Also, objects of this class do not support copy and assignment operations and
 * therefore cannot be returned from functions by value. In other words, this
 * class is just a temporary storage for handling interface method calls and not
 * intended to be used to store arrays as data members and such -- you should
 * use normal list/vector classes for that.
 *
 * @note The current implementation supports only one-dimentional arrays.
 *
 * @note This class is not thread-safe.
 */
template  <typename T, class Traits = SafeArrayTraits <T> >
class SafeArray : protected Traits
{
public:

    /**
     * Creates a null array.
     */
    SafeArray() {}

    /**
     * Creates a new array of the given size. All elements of the newly created
     * array initialized with null values.
     *
     * @param aSize     Initial number of elements in the array. Must be greater
     *                  than 0.
     *
     * @note If this object remains null after construction it means that there
     *       was not enough memory for creating an array of the requested size.
     *       The constructor will also assert in this case.
     */
    SafeArray (size_t aSize) { reset (aSize); }

    /**
     * Weakly attaches this instance to the existing array passed in a method
     * parameter declared using the ComSafeArrayIn macro. When using this call,
     * always wrap the parameter name in the ComSafeArrayOutArg macro call like
     * this:
     * <pre>
     *  SafeArray safeArray (ComSafeArrayInArg (aArg));
     * </pre>
     *
     * Note that this constructor doesn't take the ownership of the array. In
     * particular, it means that operations that operate on the ownership (e.g.
     * #detachTo()) are forbidden and will assert.
     *
     * @param aArg  Input method parameter to attach to.
     */
    SafeArray (ComSafeArrayIn (T, aArg))
    {
#if defined (VBOX_WITH_XPCOM)

        AssertReturnVoid (aArg != NULL);

        m.size = aArgSize;
        m.arr = aArg;
        m.isWeak = true;

#else /* defined (VBOX_WITH_XPCOM) */

        AssertReturnVoid (aArg != NULL);
        SAFEARRAY *arg = *aArg;

        if (arg)
        {
            AssertReturnVoid (arg->cDims == 1);

            VARTYPE vt;
            HRESULT rc = SafeArrayGetVartype (arg, &vt);
            AssertComRCReturnVoid (rc);
            AssertMsgReturnVoid (vt == VarType(),
                                 ("Expected vartype %d, got %d.\n",
                                  VarType(), vt));
        }

        m.arr = arg;
        m.isWeak = true;

        AssertReturnVoid (accessRaw() != NULL);

#endif /* defined (VBOX_WITH_XPCOM) */
    }

    /**
     * Creates a deep copy of the goven standard C++ container.
     *
     * @param aCntr Container object to copy.
     *
     * @param C     Standard C++ container template class (normally deduced from
     *              @c aCntr).
     */
    template <template <class> class C>
    SafeArray (const C <T> & aCntr)
    {
        reset (aCntr.size());
        AssertReturnVoid (!isNull());

        int i = 0;
        for (typename C <T>::const_iterator it = aCntr.begin();
             it != aCntr.end(); ++ it, ++ i)
#if defined (VBOX_WITH_XPCOM)
            Copy (*it, m.arr [i]);
#else
            Copy (*it, m.raw [i]);
#endif
    }

    /**
     * Destroys this instance after calling #setNull() to release allocated
     * resources. See #setNull() for more details.
     */
    virtual ~SafeArray() { setNull(); }

    /**
     * Returns @c true if this instance represents a null array.
     */
    bool isNull() const { return m.arr == NULL; }

    /**
     * Resets this instance to null and, if this instance is not a weak one,
     * releases any resources ocuppied by the array data.
     *
     * @note This method destroys (cleans up) all elements of the array using
     *       the corresponding cleanup routine for the element type before the
     *       array itself is destroyed.
     */
    virtual void setNull() { m.uninit(); }

    /**
     * Returns @c true if this instance is weak. A weak instance doesn't own the
     * array data and therefore operations manipulating the ownership (e.g.
     * #detachTo()) are forbidden and will assert.
     */
    bool isWeak() const { return m.isWeak; }

    /** Number of elements in the array. */
    size_t size() const
    {
#if defined (VBOX_WITH_XPCOM)
        if (m.arr)
            return m.size;
        return 0;
#else
        if (m.arr)
            return m.arr->rgsabound [0].cElements;
        return 0;
#endif
    }

    /**
     * Resizes the array preserving its contents when possible. If the new size
     * is bigger than the old size, new elements are initialized with null
     * values. If the new size is smaller than the old size, the contents of the
     * array above the new size is lost.
     *
     * @param aNewSize  New number of elements in the array.
     * @return          @c true on success and false if there is not enough
     *                  memory for resizing.
     */
    virtual bool resize (size_t aNewSize)
    {
        /// @todo Implement me!
        NOREF (aNewSize);
        AssertFailedReturn (false);
    }

    /**
     * Reinitializes this instance by preallocating space for the given number
     * of elements. The previous array contents is lost.
     *
     * @param aNewSize  New number of elements in the array.
     * @return          @c true on success and false if there is not enough
     *                  memory for resizing.
     */
    virtual bool reset (size_t aNewSize)
    {
        m.uninit();

#if defined (VBOX_WITH_XPCOM)

        /* Note: for zero-sized arrays, we use the size of 1 because whether
         * malloc(0) returns a null pointer or not (which is used in isNull())
         * is implementation-dependent according to the C standard. */

        m.arr = (T *) nsMemory::Alloc (RT_MAX (aNewSize, 1) * sizeof (T));
        AssertReturn (m.arr != NULL, false);

        m.size = aNewSize;

        for (size_t i = 0; i < m.size; ++ i)
            Init (m.arr [i]);

#else

        SAFEARRAYBOUND bound = { aNewSize, 0 };
        m.arr = SafeArrayCreate (VarType(), 1, &bound);
        AssertReturn (m.arr != NULL, false);

        AssertReturn (accessRaw() != NULL, false);

#endif
        return true;
    }

    /**
     * Returns a pointer to the raw array data. Use this raw pointer with care
     * as no type or bound checking is done for you in this case.
     *
     * @note This method returns @c NULL when this instance is null.
     * @see #operator[]
     */
    T *raw()
    {
#if defined (VBOX_WITH_XPCOM)
        return m.arr;
#else
        return accessRaw();
#endif
    }

    /**
     * Const version of #raw().
     */
    const T *raw() const
    {
#if defined (VBOX_WITH_XPCOM)
        return m.arr;
#else
        return accessRaw();
#endif
    }

    /**
     * Array access operator that returns an array element by reference. A bit
     * safer than #raw(): asserts and returns an invalid reference if this
     * instance is null or if the index is out of bounds.
     *
     * @note For weak instances, this call will succeed but the beiavior of
     *       changing the contents of an element of the weak array instance is
     *       undefined and may lead to a program crash on some platforms.
     */
    T &operator[] (size_t aIdx)
    {
        AssertReturn (m.arr != NULL,  *((T *) NULL));
        AssertReturn (aIdx < size(), *((T *) NULL));
#if defined (VBOX_WITH_XPCOM)
        return m.arr [aIdx];
#else

        AssertReturn (accessRaw() != NULL,  *((T *) NULL));
        return m.raw [aIdx];
#endif
    }

    /**
     * Const version of #operator[] that returns an array element by value.
     */
    const T operator[] (size_t aIdx) const
    {
        AssertReturn (m.arr != NULL,  *((T *) NULL));
        AssertReturn (aIdx < size(), *((T *) NULL));
#if defined (VBOX_WITH_XPCOM)
        return m.arr [aIdx];
#else
        AssertReturn (unconst (this)->accessRaw() != NULL,  *((T *) NULL));
        return m.raw [aIdx];
#endif
    }

    /**
     * Creates a copy of this array and stores it in a method parameter declared
     * using the ComSafeArrayOut macro. When using this call, always wrap the
     * parameter name in the ComSafeArrayOutArg macro call like this:
     * <pre>
     *  safeArray.cloneTo (ComSafeArrayOutArg (aArg));
     * </pre>
     *
     * @note It is assumed that the ownership of the returned copy is
     * transferred to the caller of the method and he is responsible to free the
     * array data when it is no more necessary.
     *
     * @param aArg  Output method parameter to clone to.
     */
    virtual const SafeArray &cloneTo (ComSafeArrayOut (T, aArg)) const
    {
        /// @todo Implement me!
#if defined (VBOX_WITH_XPCOM)
        NOREF (aArgSize);
        NOREF (aArg);
#else
        NOREF (aArg);
#endif
        AssertFailedReturn (*this);
    }

    /**
     * Transfers the ownership of this array's data to a method parameter
     * declared using the ComSafeArrayOut macro and makes this array a null
     * array. When using this call, always wrap the parameter name in the
     * ComSafeArrayOutArg macro call like this:
     * <pre>
     *  safeArray.detachTo (ComSafeArrayOutArg (aArg));
     * </pre>
     *
     * @note Since the ownership of the array data is transferred to the
     * caller of the method, he is responsible to free the array data when it is
     * no more necessary.
     *
     * @param aArg  Output method parameter to detach to.
     */
    virtual SafeArray &detachTo (ComSafeArrayOut (T, aArg))
    {
        AssertReturn (m.isWeak == false, *this);

#if defined (VBOX_WITH_XPCOM)

        AssertReturn (aArgSize != NULL, *this);
        AssertReturn (aArg != NULL, *this);

        *aArgSize = m.size;
        *aArg = m.arr;

        m.isWeak = false;
        m.size = 0;
        m.arr = NULL;

#else /* defined (VBOX_WITH_XPCOM) */

        AssertReturn (aArg != NULL, *this);
        *aArg = m.arr;

        if (m.raw)
        {
            HRESULT rc = SafeArrayUnaccessData (m.arr);
            AssertComRCReturn (rc, *this);
            m.raw = NULL;
        }

        m.isWeak = false;
        m.arr = NULL;

#endif /* defined (VBOX_WITH_XPCOM) */

        return *this;
    }

    // public methods for internal purposes only

#if defined (VBOX_WITH_XPCOM)

    /** Internal funciton. Never call it directly. */
    PRUint32 *__asOutParam_Size() { setNull(); return &m.size; }

    /** Internal funciton. Never call it directly. */
    T **__asOutParam_Arr() { Assert (isNull()); return &m.arr; }

#else /* defined (VBOX_WITH_XPCOM) */

    /** Internal funciton. Never call it directly. */
    SAFEARRAY ** __asInParam() { return &m.arr; }

    /** Internal funciton. Never call it directly. */
    SAFEARRAY ** __asOutParam() { setNull(); return &m.arr; }

#endif /* defined (VBOX_WITH_XPCOM) */

    static const SafeArray Null;

protected:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(SafeArray)

#if defined (VBOX_WITH_XPCOM)
#else /* defined (VBOX_WITH_XPCOM) */

    /** Requests access to the raw data pointer. */
    T *accessRaw()
    {
        if (m.arr && m.raw == NULL)
        {
            HRESULT rc = SafeArrayAccessData (m.arr, (void HUGEP **) &m.raw);
            AssertComRCReturn (rc, NULL);
        }
        return m.raw;
    }

#endif /* defined (VBOX_WITH_XPCOM) */

    struct Data
    {
        Data()
            : isWeak (false)
#if defined (VBOX_WITH_XPCOM)
            , size (0), arr (NULL)
#else
            , arr (NULL), raw (NULL)
#endif
        {}

        ~Data() { uninit(); }

        void uninit()
        {
#if defined (VBOX_WITH_XPCOM)

            if (arr)
            {
                if (!isWeak)
                {
                    for (size_t i = 0; i < size; ++ i)
                        Uninit (arr [i]);

                    nsMemory::Free ((void *) arr);

                    isWeak = false;
                }
                arr = NULL;
            }

#else /* defined (VBOX_WITH_XPCOM) */

            if (arr)
            {
                if (raw)
                {
                    SafeArrayUnaccessData (arr);
                    raw = NULL;
                }

                if (!isWeak)
                {
                    HRESULT rc = SafeArrayDestroy (arr);
                    AssertComRCReturnVoid (rc);

                    isWeak = false;
                }
                arr = NULL;
            }

#endif /* defined (VBOX_WITH_XPCOM) */
        }

        bool isWeak : 1;

#if defined (VBOX_WITH_XPCOM)
        PRUint32 size;
        T *arr;
#else
        SAFEARRAY *arr;
        T *raw;
#endif
    };

    Data m;
};

////////////////////////////////////////////////////////////////////////////////

#if defined (VBOX_WITH_XPCOM)

template <class I>
struct SafeIfaceArrayTraits
{
    static void Init (I * &aElem) { aElem = NULL; }
    static void Uninit (I * &aElem)
    {
        if (aElem)
        {
            aElem->Release();
            aElem = NULL;
        }
    }

    static void Copy (I * aFrom, I * &aTo)
    {
        if (aFrom != NULL)
        {
            aTo = aFrom;
            aTo->AddRef();
        }
        else
            aTo = NULL;
    }

    /* Magic to workaround strict rules of par. 4.4.4 of the C++ standard. */
    static I **__asInParam_Arr (I **aArr) { return aArr; }
    static I **__asInParam_Arr (const I **aArr) { return const_cast <I **> (aArr); }
};

#else /* defined (VBOX_WITH_XPCOM) */

template <class I>
struct SafeIfaceArrayTraits
{
    static VARTYPE VarType() { return VT_UNKNOWN; }

    static void Copy (I * aFrom, I * &aTo)
    {
        if (aFrom != NULL)
        {
            aTo = aFrom;
            aTo->AddRef();
        }
        else
            aTo = NULL;
    }
};

#endif /* defined (VBOX_WITH_XPCOM) */

////////////////////////////////////////////////////////////////////////////////

/**
 * Version of com::SafeArray for arrays of interface pointers.
 *
 * Except that it manages arrays of interface pointers, the usage of this class
 * is identical to com::SafeArray.
 *
 * @param I     Interface class (no asterisk).
 */
template <class I>
class SafeIfaceArray : public SafeArray <I *, SafeIfaceArrayTraits <I> >
{
public:

    typedef SafeArray <I *, SafeIfaceArrayTraits <I> > Base;

    /**
     * Creates a null array.
     */
    SafeIfaceArray() {}

    /**
     * Creates a new array of the given size. All elements of the newly created
     * array initialized with null values.
     *
     * @param aSize     Initial number of elements in the array. Must be greater
     *                  than 0.
     *
     * @note If this object remains null after construction it means that there
     *       was not enough memory for creating an array of the requested size.
     *       The constructor will also assert in this case.
     */
    SafeIfaceArray (size_t aSize) { reset (aSize); }

    /**
     * Weakly attaches this instance to the existing array passed in a method
     * parameter declared using the ComSafeArrayIn macro. When using this call,
     * always wrap the parameter name in the ComSafeArrayOutArg macro call like
     * this:
     * <pre>
     *  SafeArray safeArray (ComSafeArrayInArg (aArg));
     * </pre>
     *
     * Note that this constructor doesn't take the ownership of the array. In
     * particular, it means that operations that operate on the ownership (e.g.
     * #detachTo()) are forbidden and will assert.
     *
     * @param aArg  Input method parameter to attach to.
     */
    SafeIfaceArray (ComSafeArrayIn (I *, aArg))
    {
#if defined (VBOX_WITH_XPCOM)

        AssertReturnVoid (aArg != NULL);

        Base::m.size = aArgSize;
        Base::m.arr = aArg;
        Base::m.isWeak = true;

#else /* defined (VBOX_WITH_XPCOM) */

        AssertReturnVoid (aArg != NULL);
        SAFEARRAY *arg = *aArg;

        if (arg)
        {
            AssertReturnVoid (arg->cDims == 1);

            VARTYPE vt;
            HRESULT rc = SafeArrayGetVartype (arg, &vt);
            AssertComRCReturnVoid (rc);
            AssertMsgReturnVoid (vt == VT_UNKNOWN,
                                 ("Expected vartype VT_UNKNOWN, got %d.\n",
                                  VarType(), vt));
            GUID guid;
            rc = SafeArrayGetIID (arg, &guid);
            AssertComRCReturnVoid (rc);
            AssertMsgReturnVoid (InlineIsEqualGUID (_ATL_IIDOF (I), guid),
                                 ("Expected IID {%Vuuid}, got {%Vuuid}.\n",
                                  &_ATL_IIDOF (I), &guid));
        }

        m.arr = arg;
        m.isWeak = true;

        AssertReturnVoid (accessRaw() != NULL);

#endif /* defined (VBOX_WITH_XPCOM) */
    }

    /**
     * Creates a deep copy of the given standard C++ container that stores
     * interface pointers as objects of the ComPtr <I> class.
     *
     * @param aCntr Container object to copy.
     *
     * @param C     Standard C++ container template class (normally deduced from
     *              @c aCntr).
     * @param A     Standard C++ allocator class (deduced from @c aCntr).
     * @param OI    Argument to the ComPtr template (deduced from @c aCntr).
     */
    template <template <typename, typename> class C, class A, class OI>
    SafeIfaceArray (const C <ComPtr <OI>, A> & aCntr)
    {
        typedef C <ComPtr <OI>, A> List;

        reset (aCntr.size());
        AssertReturnVoid (!Base::isNull());

        int i = 0;
        for (typename List::const_iterator it = aCntr.begin();
             it != aCntr.end(); ++ it, ++ i)
#if defined (VBOX_WITH_XPCOM)
            Copy (*it, Base::m.arr [i]);
#else
            Copy (*it, Base::m.raw [i]);
#endif
    }

    /**
     * Creates a deep copy of the given standard C++ container that stores
     * interface pointers as objects of the ComObjPtr <I> class.
     *
     * @param aCntr Container object to copy.
     *
     * @param C     Standard C++ container template class (normally deduced from
     *              @c aCntr).
     * @param A     Standard C++ allocator class (deduced from @c aCntr).
     * @param OI    Argument to the ComObjPtr template (deduced from @c aCntr).
     */
    template <template <typename, typename> class C, class A, class OI>
    SafeIfaceArray (const C <ComObjPtr <OI>, A> & aCntr)
    {
        typedef C <ComObjPtr <OI>, A> List;

        reset (aCntr.size());
        AssertReturnVoid (!Base::isNull());

        int i = 0;
        for (typename List::const_iterator it = aCntr.begin();
             it != aCntr.end(); ++ it, ++ i)
#if defined (VBOX_WITH_XPCOM)
            Copy (*it, Base::m.arr [i]);
#else
            Copy (*it, Base::m.raw [i]);
#endif
    }

    /**
     * Reinitializes this instance by preallocating space for the given number
     * of elements. The previous array contents is lost.
     *
     * @param aNewSize  New number of elements in the array.
     * @return          @c true on success and false if there is not enough
     *                  memory for resizing.
     */
    virtual bool reset (size_t aNewSize)
    {
        Base::m.uninit();

#if defined (VBOX_WITH_XPCOM)

        /* Note: for zero-sized arrays, we use the size of 1 because whether
         * malloc(0) returns a null pointer or not (which is used in isNull())
         * is implementation-dependent according to the C standard. */

        Base::m.arr = (I **) nsMemory::Alloc (RT_MAX (aNewSize, 1) * sizeof (I *));
        AssertReturn (Base::m.arr != NULL, false);

        Base::m.size = aNewSize;

        for (size_t i = 0; i < Base::m.size; ++ i)
            Init (Base::m.arr [i]);

#else

        SAFEARRAYBOUND bound = { aNewSize, 0 };
        m.arr = SafeArrayCreateEx (VT_UNKNOWN, 1, &bound,
                                   (PVOID) &_ATL_IIDOF (I));
        AssertReturn (m.arr != NULL, false);

        AssertReturn (accessRaw() != NULL, false);

#endif
        return true;
    }
};

} /* namespace com */

/** @} */

#endif /* ___VBox_com_array_h */
