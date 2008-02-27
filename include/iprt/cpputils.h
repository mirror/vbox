/** @file
 * innotek Portable Runtime - C++ Utilities (useful templates, defines and such).
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

#ifndef ___iprt_cpputils_h
#define ___iprt_cpputils_h

#include <iprt/assert.h>

#include <memory>

/** @defgroup grp_rt_cpputils   C++ Utilities
 * @ingroup grp_rt
 * @{
 */

/**
 * Shortcut to |const_cast<C &>()| that automatically derives the correct
 * type (class) for the const_cast template's argument from its own argument.
 * Can be used to temporarily cancel the |const| modifier on the left-hand side
 * of assignment expressions, like this:
 * @code
 *      const Class that;
 *      ...
 *      unconst (that) = some_value;
 * @endcode
 */
template <class C>
inline C &unconst (const C &that) { return const_cast <C &> (that); }


/**
 * Shortcut to |const_cast<C *>()| that automatically derives the correct
 * type (class) for the const_cast template's argument from its own argument.
 * Can be used to temporarily cancel the |const| modifier on the left-hand side
 * of assignment expressions, like this:
 * @code
 *      const Class *that;
 *      ...
 *      unconst (that) = some_value;
 * @endcode
 */
template <class C>
inline C *unconst (const C *that) { return const_cast <C *> (that); }


/**
 * Extensions to the std namespace.
 */
namespace stdx
{

/* forward */
template <class> class auto_ref_ptr;

/**
 * Base class for objects willing to support smart reference counting using
 * the auto_ref_ptr template.
 *
 * When a class that wants to be used with the auto_ref_ptr template it simply
 * declares the auto_ref class among its public base classes -- there is no
 * need to implement any additional methods.
 */
class auto_ref
{
protected:

    auto_ref() : mRefs (0) {}

    /** Increases the reference counter and returns it */
    size_t ref() { return ++ mRefs; }

    /** Decreases the reference counter and returns it */
    size_t unref() { Assert (mRefs > 0); return -- mRefs; }

private:

    size_t mRefs;

    template <class> friend class auto_ref_ptr;
};

/**
 * The auto_ref_ptr template manages pointers to objects that support
 * reference counting by implementing auto_ref or a similar interface.
 *
 * Pointer management includes the following key points:
 *
 *   1) Automatic increment of the object's reference counter when the given
 *      auto_ref_ptr instance starts managing a pointer to this object.
 *
 *   2) Automatic decrement of the reference counter when the given
 *      auto_ref_ptr instance is destroyed, or before it is assigned a pointer
 *      to a new object.
 *
 *   3) Automatic deletion of the managed object whenever its reference
 *      counter reaches zero after a decrement.
 *
 *   4) Providing the dereference operator-> that gives direct access to the
 *      managed pointer.
 *
 * The object class to manage must provide ref() and unref() methods that have
 * the same syntax and symantics as defined in the auto_ref class.
 *
 * @param C     Class to manage.
 */
template <class C>
class auto_ref_ptr
{
public:

    /**
     * Creates a null instance that does not manage anything.
     */
    auto_ref_ptr() : m (NULL) {}

    /**
     * Creates an instance that starts managing the given pointer. The
     * reference counter of the object pointed to by @a a is incremented by
     * one.
     *
     * @param a Pointer to manage.
     */
    auto_ref_ptr (C* a) : m (a) { if (m) m->ref(); }

    /**
     * Creates an instance that starts managing a pointer managed by the given
     * instance. The reference counter of the object managed by @a that is
     * incremented by one.
     *
     * @param that Instance to take a pointer to manage from.
     */
    auto_ref_ptr (const auto_ref_ptr &that) : m (that.m) { if (m) m->ref(); }

    ~auto_ref_ptr() { do_unref(); }

    /**
     * Assigns the given pointer to this instance and starts managing it. The
     * reference counter of the object pointed to by @a a is incremented by
     * one. The reference counter of the object previously managed by this
     * instance is decremented by one.
     *
     * @param a Pointer to assign.
     */
    auto_ref_ptr &operator= (C *a) { do_reref (a); return *this; }

    /**
     * Assigns a pointer managed by the given instance to this instance and
     * starts managing it. The reference counter of the object managed by @a
     * that is incremented by one. The reference counter of the object
     * previously managed by this instance is decremented by one.
     *
     * @param that Instance which pointer to reference.
     */
    auto_ref_ptr &operator= (const auto_ref_ptr &that) { do_reref (that.m); return *this; }

    /**
     * Returns @c true if this instance is @c null and false otherwise.
     */
    bool is_null() const { return m == NULL; }

    /**
     * Dereferences the instance by returning the managed pointer.
     * Asserts that the managed pointer is not @c NULL.
     */
    C *operator-> () const { AssertMsg (m, ("Managed pointer is NULL!\n")); return m; }

    /**
     * Returns the managed pointer or @c NULL if this instance is @c null.
     */
    C *raw() const { return m; }

    /**
     * Compares this auto_ref_ptr instance with another instance and returns
     * @c true if both instances manage the same or @c NULL pointer.
     *
     * Note that this method compares pointer values only, it doesn't try to
     * compare objects themselves. Doing otherwise would a) break the common
     * 'pointer to something' comparison semantics auto_ref_ptr tries to
     * follow and b) require to define the comparison operator in the managed
     * class which is not always possible. You may analyze pointed objects
     * yourself if you need more precise comparison.
     *
     * @param that Instance to compare this instance with.
     */
    bool operator== (const auto_ref_ptr &that) const
    {
        return m == that.m;
    }

protected:

    void do_reref (C *a)
    {
        /* be aware of self assignment */
        if (a)
            a->ref();
        if (m)
        {
            size_t refs = m->unref();
            if (refs == 0)
            {
                refs = 1; /* stabilize */
                delete m;
            }
        }
        m = a;
    }

    void do_unref() { do_reref (NULL); }

    C *m;
};

/**
 * The exception_trap_base class is an abstract base class for all
 * exception_trap template instantiations.
 *
 * Pointer variables of this class are used to store a pointer any object of
 * any class instantiated from the exception_trap template, or in other words
 * to store a full copy of any exception wrapped into the exception_trap instance
 * allocated on the heap.
 *
 * See the exception_trap template for more info.
 */
class exception_trap_base
{
public:

    virtual void rethrow() = 0;
};

/**
 * The exception_trap template acts like a wrapper for the given exception
 * class that stores a full copy of the exception and therefore allows to
 * rethrow it preserving the actual type information about the exception
 * class.
 *
 * This functionality is useful in situations where it is necessary to catch a
 * (known) number of exception classes and pass the caught exception instance
 * to an upper level using a regular variable (rather than the exception
 * unwinding mechanism itself) *and* preserve all information about the type
 * (class) of the caight exception so that it may be rethrown on the upper
 * level unchanged.
 *
 * Usage pattern:
 * @code
    using namespace std;
    using namespace stdx;

    auto_ptr <exception_trap_base> trapped;

    int callback();

    int safe_callback()
    {
      try
      {
        // callback may throw a set of exceptions but we don't want it to start
        // unwinding the stack right now

        return callback();
      }
      catch (const MyException &err) { trapped = new_exception_trap (err); }
      catch (const MyException2 &err) { trapped = new_exception_trap (err); }
      catch (...) { trapped = new_exception_trap (logic_error()); }

      return -1;
    }

    void bar()
    {
      // call a funciton from some C library that supports callbacks but knows
      // nothing about exceptions so throwing one from a callback will leave
      // the library in an undetermined state

      do_something_with_callback (safe_callback());

      // check if we have got an exeption from callback() and rethrow it now
      // when we are not in the C library any more
      if (trapped.get() != NULL)
        trapped->rethrow();
    }
 * @endcode
 *
 * @param T Exception class to wrap.
 */
template <typename T>
class exception_trap : public exception_trap_base
{
public:

    exception_trap (const T &aTrapped) : trapped (aTrapped) {}
    void rethrow() { throw trapped; }

    T trapped;
};

/**
 * Convenience function that allocates a new exception_trap instance on the
 * heap by automatically deducing the exception_trap template argument from
 * the type of the exception passed in @a aTrapped.
 *
 * The following two lines of code inside the catch block are equivalent:
 *
 * @code
    using namespace std;
    using namespace stdx;
    catch (const MyException &err)
    {
      auto_ptr <exception_trap_base> t1 = new exception_trap <MyException> (err);
      auto_ptr <exception_trap_base> t2 = new_exception_trap (err);
    }
 * @endcode
 *
 * @param aTrapped Exception to put to the allocated trap.
 *
 * @return Allocated exception_trap object.
 */
template <typename T>
static exception_trap <T> *
new_exception_trap (const T &aTrapped)
{
    return new exception_trap <T> (aTrapped);
}

/**
 * Enhancement of std::auto_ptr @<char@> intended to take pointers to char
 * buffers allocated using new[].
 *
 * This differs from std::auto_ptr @<char@> so that it overloads some methods to
 * uses delete[] instead of delete to delete the owned data in order to
 * conform to the C++ standard (and avoid valgrind complaints).
 *
 * Note that you should not use instances of this class where pointers or
 * references to objects of std::auto_ptr @<char@> are expeced. Despite the fact
 * the classes are related, the base is not polymorphic (in particular,
 * neither the destructor nor the reset() method are virtual). It means that when
 * acessing instances of this class through the base pointer, overloaded
 * methods won't be called.
 */
class char_auto_ptr : public std::auto_ptr <char>
{
public:

    explicit char_auto_ptr (char *a = 0) throw()
        : std::auto_ptr <char> (a) {}

    /* Note: we use unconst brute force below because the non-const version
     * of the copy constructor won't accept temporary const objects
     * (e.g. function return values) in GCC. std::auto_ptr has the same
     * "problem" but it seems overcome it using #pragma GCC system_header
     * which doesn't work here. */
    char_auto_ptr (const char_auto_ptr &that) throw()
        : std::auto_ptr <char> (unconst (that).release()) {}

    ~char_auto_ptr() { delete[] (release()); }

    char_auto_ptr &operator= (char_auto_ptr &that) throw()
    {
        std::auto_ptr <char>::operator= (that);
        return *this;
    }

    void reset (char *a) throw()
    {
        if (a != get())
        {
            delete[] (release());
            std::auto_ptr <char>::reset (a);
        }
    }
};

} /* namespace stdx */

/** @} */

#endif

