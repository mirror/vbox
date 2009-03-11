/** @file
 *
 * VirtualBox COM base classes definition
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#ifndef ____H_VIRTUALBOXBASEIMPL
#define ____H_VIRTUALBOXBASEIMPL

#include <iprt/cdefs.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>

#include <list>
#include <map>

#include "VBox/com/ErrorInfo.h"

#include "VBox/com/VirtualBox.h"

// avoid including VBox/settings.h and VBox/xml.h;
// only declare the classes
namespace settings
{
class XmlTreeBackend;
class TreeBackend;
class Key;
}

namespace xml
{
class File;
}

#include "AutoLock.h"

using namespace com;
using namespace util;

#if !defined (VBOX_WITH_XPCOM)

#include <atlcom.h>

/* use a special version of the singleton class factory,
 * see KB811591 in msdn for more info. */

#undef DECLARE_CLASSFACTORY_SINGLETON
#define DECLARE_CLASSFACTORY_SINGLETON(obj) DECLARE_CLASSFACTORY_EX(CMyComClassFactorySingleton<obj>)

template <class T>
class CMyComClassFactorySingleton : public CComClassFactory
{
public:
    CMyComClassFactorySingleton() : m_hrCreate(S_OK){}
    virtual ~CMyComClassFactorySingleton(){}
    // IClassFactory
    STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter, REFIID riid, void** ppvObj)
    {
        HRESULT hRes = E_POINTER;
        if (ppvObj != NULL)
        {
            *ppvObj = NULL;
            // Aggregation is not supported in singleton objects.
            ATLASSERT(pUnkOuter == NULL);
            if (pUnkOuter != NULL)
                hRes = CLASS_E_NOAGGREGATION;
            else
            {
                if (m_hrCreate == S_OK && m_spObj == NULL)
                {
                    Lock();
                    __try
                    {
                        // Fix:  The following If statement was moved inside the __try statement.
                        // Did another thread arrive here first?
                        if (m_hrCreate == S_OK && m_spObj == NULL)
                        {
                            // lock the module to indicate activity
                            // (necessary for the monitor shutdown thread to correctly
                            // terminate the module in case when CreateInstance() fails)
                            _pAtlModule->Lock();
                            CComObjectCached<T> *p;
                            m_hrCreate = CComObjectCached<T>::CreateInstance(&p);
                            if (SUCCEEDED(m_hrCreate))
                            {
                                m_hrCreate = p->QueryInterface(IID_IUnknown, (void**)&m_spObj);
                                if (FAILED(m_hrCreate))
                                {
                                    delete p;
                                }
                            }
                            _pAtlModule->Unlock();
                        }
                    }
                    __finally
                    {
                        Unlock();
                    }
                }
                if (m_hrCreate == S_OK)
                {
                    hRes = m_spObj->QueryInterface(riid, ppvObj);
                }
                else
                {
                    hRes = m_hrCreate;
                }
            }
        }
        return hRes;
    }
    HRESULT m_hrCreate;
    CComPtr<IUnknown> m_spObj;
};

#endif /* !defined (VBOX_WITH_XPCOM) */

// macros
////////////////////////////////////////////////////////////////////////////////

/**
 *  Special version of the Assert macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  In the debug build, this macro is equivalent to Assert.
 *  In the release build, this macro uses |setError (E_FAIL, ...)| to set the
 *  error info from the asserted expression.
 *
 *  @see VirtualBoxSupportErrorInfoImpl::setError
 *
 *  @param   expr    Expression which should be true.
 */
#if defined (DEBUG)
#define ComAssert(expr)    Assert (expr)
#else
#define ComAssert(expr)    \
    do { \
        if (!(expr)) \
            setError (E_FAIL, "Assertion failed: [%s] at '%s' (%d) in %s.\n" \
                              "Please contact the product vendor!", \
                      #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    } while (0)
#endif

/**
 *  Special version of the AssertMsg macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param   expr    Expression which should be true.
 *  @param   a       printf argument list (in parenthesis).
 */
#if defined (DEBUG)
#define ComAssertMsg(expr, a)  AssertMsg (expr, a)
#else
#define ComAssertMsg(expr, a)  \
    do { \
        if (!(expr)) \
            setError (E_FAIL, "Assertion failed: [%s] at '%s' (%d) in %s.\n" \
                              "%s.\n" \
                              "Please contact the product vendor!", \
                      #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__, Utf8StrFmt a .raw()); \
    } while (0)
#endif

/**
 *  Special version of the AssertRC macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 * @param   vrc     VBox status code.
 */
#if defined (DEBUG)
#define ComAssertRC(vrc)    AssertRC (vrc)
#else
#define ComAssertRC(vrc)    ComAssertMsgRC (vrc, ("%Rra", vrc))
#endif

/**
 *  Special version of the AssertMsgRC macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param   vrc    VBox status code.
 *  @param   msg    printf argument list (in parenthesis).
 */
#if defined (DEBUG)
#define ComAssertMsgRC(vrc, msg)    AssertMsgRC (vrc, msg)
#else
#define ComAssertMsgRC(vrc, msg)    ComAssertMsg (RT_SUCCESS (vrc), msg)
#endif


/**
 *  Special version of the AssertFailed macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 */
#if defined (DEBUG)
#define ComAssertFailed()   AssertFailed()
#else
#define ComAssertFailed()   \
    do { \
        setError (E_FAIL, "Assertion failed at '%s' (%d) in %s.\n" \
                          "Please contact the product vendor!", \
                  __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    } while (0)
#endif

/**
 *  Special version of the AssertMsgFailed macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param   a   printf argument list (in parenthesis).
 */
#if defined (DEBUG)
#define ComAssertMsgFailed(a)   AssertMsgFailed(a)
#else
#define ComAssertMsgFailed(a)   \
    do { \
        setError (E_FAIL, "Assertion failed at '%s' (%d) in %s.\n" \
                          "%s.\n" \
                          "Please contact the product vendor!", \
                  __FILE__, __LINE__, __PRETTY_FUNCTION__, Utf8StrFmt a .raw()); \
    } while (0)
#endif

/**
 *  Special version of the ComAssertMsgFailed macro that additionally takes
 *  line number, file and function arguments to inject an assertion position
 *  that differs from the position where this macro is instantiated.
 *
 *  @param   a                  printf argument list (in parenthesis).
 *  @param   file, line, func   Line number (int), file and function (const char *).
 */
#if defined (DEBUG)
#define ComAssertMsgFailedPos(a, file, line, func)              \
    do {                                                        \
        AssertMsg1 ((const char *) 0, line, file, func);        \
        AssertMsg2 a;                                           \
        AssertBreakpoint();                                     \
    } while (0)
#else
#define ComAssertMsgFailedPos(a, file, line, func)              \
    do {                                                        \
        setError (E_FAIL,                                       \
                  "Assertion failed at '%s' (%d) in %s.\n"      \
                  "%s.\n"                                       \
                  "Please contact the product vendor!",         \
                  file, line, func, Utf8StrFmt a .raw());       \
    } while (0)
#endif

/**
 *  Special version of the AssertComRC macro to be used within VirtualBoxBase
 *  subclasses that also inherit the VirtualBoxSupportErrorInfoImpl template.
 *
 *  See ComAssert for more info.
 *
 *  @param rc   COM result code
 */
#if defined (DEBUG)
#define ComAssertComRC(rc)  AssertComRC (rc)
#else
#define ComAssertComRC(rc)  ComAssertMsg (SUCCEEDED (rc), ("COM RC = %Rhrc (0x%08X)", (rc), (rc)))
#endif


/** Special version of ComAssert that returns ret if expr fails */
#define ComAssertRet(expr, ret)             \
    do { ComAssert (expr); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertMsg that returns ret if expr fails */
#define ComAssertMsgRet(expr, a, ret)       \
    do { ComAssertMsg (expr, a); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertRC that returns ret if vrc does not succeed */
#define ComAssertRCRet(vrc, ret)            \
    do { ComAssertRC (vrc); if (!RT_SUCCESS (vrc)) return (ret); } while (0)
/** Special version of ComAssertMsgRC that returns ret if vrc does not succeed */
#define ComAssertMsgRCRet(vrc, msg, ret)    \
    do { ComAssertMsgRC (vrc, msg); if (!RT_SUCCESS (vrc)) return (ret); } while (0)
/** Special version of ComAssertFailed that returns ret */
#define ComAssertFailedRet(ret)             \
    do { ComAssertFailed(); return (ret); } while (0)
/** Special version of ComAssertMsgFailed that returns ret */
#define ComAssertMsgFailedRet(msg, ret)     \
    do { ComAssertMsgFailed (msg); return (ret); } while (0)
/** Special version of ComAssertComRC that returns ret if rc does not succeed */
#define ComAssertComRCRet(rc, ret)          \
    do { ComAssertComRC (rc); if (!SUCCEEDED (rc)) return (ret); } while (0)
/** Special version of ComAssertComRC that returns rc if rc does not succeed */
#define ComAssertComRCRetRC(rc)             \
    do { ComAssertComRC (rc); if (!SUCCEEDED (rc)) return (rc); } while (0)


/** Special version of ComAssert that evaluates eval and breaks if expr fails */
#define ComAssertBreak(expr, eval)                \
    if (1) { ComAssert (expr); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsg that evaluates eval and breaks if expr fails */
#define ComAssertMsgBreak(expr, a, eval)          \
    if (1)  { ComAssertMsg (expr, a); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertRC that evaluates eval and breaks if vrc does not succeed */
#define ComAssertRCBreak(vrc, eval)               \
    if (1)  { ComAssertRC (vrc); if (!RT_SUCCESS (vrc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsgRC that evaluates eval and breaks if vrc does not succeed */
#define ComAssertMsgRCBreak(vrc, msg, eval)       \
    if (1)  { ComAssertMsgRC (vrc, msg); if (!RT_SUCCESS (vrc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertFailed that evaluates eval and breaks */
#define ComAssertFailedBreak(eval)                \
    if (1)  { ComAssertFailed(); { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsgFailed that evaluates eval and breaks */
#define ComAssertMsgFailedBreak(msg, eval)        \
    if (1)  { ComAssertMsgFailed (msg); { eval; break; } } else do {} while (0)
/** Special version of ComAssertComRC that evaluates eval and breaks if rc does not succeed */
#define ComAssertComRCBreak(rc, eval)             \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertComRC that just breaks if rc does not succeed */
#define ComAssertComRCBreakRC(rc)                 \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { break; } } else do {} while (0)


/** Special version of ComAssert that evaluates eval and throws it if expr fails */
#define ComAssertThrow(expr, eval)                \
    if (1) { ComAssert (expr); if (!(expr)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertMsg that evaluates eval and throws it if expr fails */
#define ComAssertMsgThrow(expr, a, eval)          \
    if (1)  { ComAssertMsg (expr, a); if (!(expr)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertRC that evaluates eval and throws it if vrc does not succeed */
#define ComAssertRCThrow(vrc, eval)               \
    if (1)  { ComAssertRC (vrc); if (!RT_SUCCESS (vrc)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertMsgRC that evaluates eval and throws it if vrc does not succeed */
#define ComAssertMsgRCThrow(vrc, msg, eval)       \
    if (1)  { ComAssertMsgRC (vrc, msg); if (!RT_SUCCESS (vrc)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertFailed that evaluates eval and throws it */
#define ComAssertFailedThrow(eval)                \
    if (1)  { ComAssertFailed(); { throw (eval); } } else do {} while (0)
/** Special version of ComAssertMsgFailed that evaluates eval and throws it */
#define ComAssertMsgFailedThrow(msg, eval)        \
    if (1)  { ComAssertMsgFailed (msg); { throw (eval); } } else do {} while (0)
/** Special version of ComAssertComRC that evaluates eval and throws it if rc does not succeed */
#define ComAssertComRCThrow(rc, eval)             \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertComRC that just throws rc if rc does not succeed */
#define ComAssertComRCThrowRC(rc)                 \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { throw rc; } } else do {} while (0)

////////////////////////////////////////////////////////////////////////////////

/**
 * Checks that the pointer argument is not NULL and returns E_INVALIDARG +
 * extended error info on failure.
 * @param arg   Input pointer-type argument (strings, interface pointers...)
 */
#define CheckComArgNotNull(arg) \
    do { \
        if ((arg) == NULL) \
            return setError (E_INVALIDARG, tr ("Argument %s is NULL"), #arg); \
    } while (0)

/**
 * Checks that safe array argument is not NULL and returns E_INVALIDARG +
 * extended error info on failure.
 * @param arg   Input safe array argument (strings, interface pointers...)
 */
#define CheckComArgSafeArrayNotNull(arg) \
    do { \
        if (ComSafeArrayInIsNull (arg)) \
            return setError (E_INVALIDARG, tr ("Argument %s is NULL"), #arg); \
    } while (0)

/**
 * Checks that the string argument is not a NULL or empty string and returns
 * E_INVALIDARG + extended error info on failure.
 * @param arg   Input string argument (BSTR etc.).
 */
#define CheckComArgStrNotEmptyOrNull(arg) \
    do { \
        if ((arg) == NULL || *(arg) == '\0') \
            return setError (E_INVALIDARG, \
                tr ("Argument %s is emtpy or NULL"), #arg); \
    } while (0)

/**
 * Checks that the given expression (that must involve the argument) is true and
 * returns E_INVALIDARG + extended error info on failure.
 * @param arg   Argument.
 * @param expr  Expression to evaluate.
 */
#define CheckComArgExpr(arg, expr) \
    do { \
        if (!(expr)) \
            return setError (E_INVALIDARG, \
                tr ("Argument %s is invalid (must be %s)"), #arg, #expr); \
    } while (0)

/**
 * Checks that the given expression (that must involve the argument) is true and
 * returns E_INVALIDARG + extended error info on failure. The error message must
 * be customized.
 * @param arg   Argument.
 * @param expr  Expression to evaluate.
 * @param msg   Parenthesized printf-like expression (must start with a verb,
 *              like "must be one of...", "is not within...").
 */
#define CheckComArgExprMsg(arg, expr, msg) \
    do { \
        if (!(expr)) \
            return setError (E_INVALIDARG, tr ("Argument %s %s"), \
                             #arg, Utf8StrFmt msg .raw()); \
    } while (0)

/**
 * Checks that the given pointer to an output argument is valid and returns
 * E_POINTER + extended error info otherwise.
 * @param arg   Pointer argument.
 */
#define CheckComArgOutPointerValid(arg) \
    do { \
        if (!VALID_PTR (arg)) \
            return setError (E_POINTER, \
                tr ("Output argument %s points to invalid memory location (%p)"), \
                #arg, (void *) (arg)); \
    } while (0)

/**
 * Checks that the given pointer to an output safe array argument is valid and
 * returns E_POINTER + extended error info otherwise.
 * @param arg   Safe array argument.
 */
#define CheckComArgOutSafeArrayPointerValid(arg) \
    do { \
        if (ComSafeArrayOutIsNull (arg)) \
            return setError (E_POINTER, \
                tr ("Output argument %s points to invalid memory location (%p)"), \
                #arg, (void *) (arg)); \
    } while (0)

/**
 * Sets the extended error info and returns E_NOTIMPL.
 */
#define ReturnComNotImplemented() \
    do { \
        return setError (E_NOTIMPL, tr ("Method %s is not implemented"), __FUNCTION__); \
    } while (0)

////////////////////////////////////////////////////////////////////////////////

/// @todo (dmik) remove after we switch to VirtualBoxBaseNEXT completely
/**
 *  Checks whether this object is ready or not. Objects are typically ready
 *  after they are successfully created by their parent objects and become
 *  not ready when the respective parent itself becomes not ready or gets
 *  destroyed while a reference to the child is still held by the caller
 *  (which prevents it from destruction).
 *
 *  When this object is not ready, the macro sets error info and returns
 *  E_ACCESSDENIED (the translatable error message is defined in null context).
 *  Otherwise, the macro does nothing.
 *
 *  This macro <b>must</b> be used at the beginning of all interface methods
 *  (right after entering the class lock) in classes derived from both
 *  VirtualBoxBase and VirtualBoxSupportErrorInfoImpl.
 */
#define CHECK_READY() \
    do { \
        if (!isReady()) \
            return setError (E_ACCESSDENIED, tr ("The object is not ready")); \
    } while (0)

/**
 *  Declares an empty constructor and destructor for the given class.
 *  This is useful to prevent the compiler from generating the default
 *  ctor and dtor, which in turn allows to use forward class statements
 *  (instead of including their header files) when declaring data members of
 *  non-fundamental types with constructors (which are always called implicitly
 *  by constructors and by the destructor of the class).
 *
 *  This macro is to be placed within (the public section of) the class
 *  declaration. Its counterpart, DEFINE_EMPTY_CTOR_DTOR, must be placed
 *  somewhere in one of the translation units (usually .cpp source files).
 *
 *  @param      cls     class to declare a ctor and dtor for
 */
#define DECLARE_EMPTY_CTOR_DTOR(cls) cls(); ~cls();

/**
 *  Defines an empty constructor and destructor for the given class.
 *  See DECLARE_EMPTY_CTOR_DTOR for more info.
 */
#define DEFINE_EMPTY_CTOR_DTOR(cls) \
    cls::cls () {}; cls::~cls () {};

////////////////////////////////////////////////////////////////////////////////

namespace stdx
{
    /**
     *  A wrapper around the container that owns pointers it stores.
     *
     *  @note
     *      Ownership is recognized only when destructing the container!
     *      Pointers are not deleted when erased using erase() etc.
     *
     *  @param container
     *      class that meets Container requirements (for example, an instance of
     *      std::list<>, std::vector<> etc.). The given class must store
     *      pointers (for example, std::list <MyType *>).
     */
    template <typename container>
    class ptr_container : public container
    {
    public:
        ~ptr_container()
        {
            for (typename container::iterator it = container::begin();
                it != container::end();
                ++ it)
                delete (*it);
        }
    };
}

////////////////////////////////////////////////////////////////////////////////

/**
 * Abstract base class for all component classes implementing COM
 * interfaces of the VirtualBox COM library.
 *
 * Declares functionality that should be available in all components.
 *
 * Note that this class is always subclassed using the virtual keyword so
 * that only one instance of its VTBL and data is present in each derived class
 * even in case if VirtualBoxBaseProto appears more than once among base classes
 * of the particular component as a result of multiple inheritance.
 *
 * This makes it possible to have intermediate base classes used by several
 * components that implement some common interface functionality but still let
 * the final component classes choose what VirtualBoxBase variant it wants to
 * use.
 *
 * Among the basic functionality implemented by this class is the primary object
 * state that indicates if the object is ready to serve the calls, and if not,
 * what stage it is currently at. Here is the primary state diagram:
 *
 *              +-------------------------------------------------------+
 *              |                                                       |
 *              |         (InitFailed) -----------------------+         |
 *              |              ^                              |         |
 *              v              |                              v         |
 *  [*] ---> NotReady ----> (InInit) -----> Ready -----> (InUninit) ----+
 *                     ^       |      ^       |               ^
 *                     |       v      |       v               |
 *                     |    Limited   |  (MayUninit) --> (WillUninit)
 *                     |       |      |       |
 *                     +-------+      +-------+
 *
 * The object is fully operational only when its state is Ready. The Limited
 * state means that only some vital part of the object is operational, and it
 * requires some sort of reinitialization to become fully operational. The
 * NotReady state means the object is basically dead: it either was not yet
 * initialized after creation at all, or was uninitialized and is waiting to be
 * destroyed when the last reference to it is released. All other states are
 * transitional.
 *
 * The NotReady->InInit->Ready, NotReady->InInit->Limited and
 * NotReady->InInit->InitFailed transition is done by the AutoInitSpan smart
 * class.
 *
 * The Limited->InInit->Ready, Limited->InInit->Limited and
 * Limited->InInit->InitFailed transition is done by the AutoReinitSpan smart
 * class.
 *
 * The Ready->InUninit->NotReady, InitFailed->InUninit->NotReady and
 * WillUninit->InUninit->NotReady transitions are done by the AutoUninitSpan
 * smart class.
 *
 * The Ready->MayUninit->Ready and Ready->MayUninit->WillUninit transitions are
 * done by the AutoMayUninitSpan smart class.
 *
 * In order to maintain the primary state integrity and declared functionality
 * all subclasses must:
 *
 * 1) Use the above Auto*Span classes to perform state transitions. See the
 *    individual class descriptions for details.
 *
 * 2) All public methods of subclasses (i.e. all methods that can be called
 *    directly, not only from within other methods of the subclass) must have a
 *    standard prolog as described in the AutoCaller and AutoLimitedCaller
 *    documentation. Alternatively, they must use addCaller()/releaseCaller()
 *    directly (and therefore have both the prolog and the epilog), but this is
 *    not recommended.
 */
class ATL_NO_VTABLE VirtualBoxBaseProto : public Lockable
{
public:

    enum State { NotReady, Ready, InInit, InUninit, InitFailed, Limited,
                 MayUninit, WillUninit };

protected:

    VirtualBoxBaseProto();
    virtual ~VirtualBoxBaseProto();

public:

    // util::Lockable interface
    virtual RWLockHandle *lockHandle() const;

    /**
     * Unintialization method.
     *
     * Must be called by all final implementations (component classes) when the
     * last reference to the object is released, before calling the destructor.
     *
     * This method is also automatically called by the uninit() method of this
     * object's parent if this object is a dependent child of a class derived
     * from VirtualBoxBaseWithChildren (see
     * VirtualBoxBaseWithChildren::addDependentChild).
     *
     * @note Never call this method the AutoCaller scope or after the
     *       #addCaller() call not paired by #releaseCaller() because it is a
     *       guaranteed deadlock. See AutoUninitSpan for details.
     */
    virtual void uninit() {}

    virtual HRESULT addCaller (State *aState = NULL, bool aLimited = false);
    virtual void releaseCaller();

    /**
     * Adds a limited caller. This method is equivalent to doing
     * <tt>addCaller (aState, true)</tt>, but it is preferred because provides
     * better self-descriptiveness. See #addCaller() for more info.
     */
    HRESULT addLimitedCaller (State *aState = NULL)
    {
        return addCaller (aState, true /* aLimited */);
    }

    /**
     * Smart class that automatically increases the number of callers of the
     * given VirtualBoxBase object when an instance is constructed and decreases
     * it back when the created instance goes out of scope (i.e. gets destroyed).
     *
     * If #rc() returns a failure after the instance creation, it means that
     * the managed VirtualBoxBase object is not Ready, or in any other invalid
     * state, so that the caller must not use the object and can return this
     * failed result code to the upper level.
     *
     * See VirtualBoxBase::addCaller(), VirtualBoxBase::addLimitedCaller() and
     * VirtualBoxBase::releaseCaller() for more details about object callers.
     *
     * @param aLimited  |false| if this template should use
     *                  VirtualiBoxBase::addCaller() calls to add callers, or
     *                  |true| if VirtualiBoxBase::addLimitedCaller() should be
     *                  used.
     *
     * @note It is preferable to use the AutoCaller and AutoLimitedCaller
     *       classes than specify the @a aLimited argument, for better
     *       self-descriptiveness.
     */
    template <bool aLimited>
    class AutoCallerBase
    {
    public:

        /**
         * Increases the number of callers of the given object by calling
         * VirtualBoxBase::addCaller().
         *
         * @param aObj      Object to add a caller to. If NULL, this
         *                  instance is effectively turned to no-op (where
         *                  rc() will return S_OK and state() will be
         *                  NotReady).
         */
        AutoCallerBase (VirtualBoxBaseProto *aObj)
            : mObj (aObj)
            , mRC (S_OK)
            , mState (NotReady)
        {
            if (mObj)
                mRC =  mObj->addCaller (&mState, aLimited);
        }

        /**
         * If the number of callers was successfully increased, decreases it
         * using VirtualBoxBase::releaseCaller(), otherwise does nothing.
         */
        ~AutoCallerBase()
        {
            if (mObj && SUCCEEDED (mRC))
                mObj->releaseCaller();
        }

        /**
         * Stores the result code returned by VirtualBoxBase::addCaller() after
         * instance creation or after the last #add() call. A successful result
         * code means the number of callers was successfully increased.
         */
        HRESULT rc() const { return mRC; }

        /**
         * Returns |true| if |SUCCEEDED (rc())| is |true|, for convenience.
         * |true| means the number of callers was successfully increased.
         */
        bool isOk() const { return SUCCEEDED (mRC); }

        /**
         * Stores the object state returned by VirtualBoxBase::addCaller() after
         * instance creation or after the last #add() call.
         */
        State state() const { return mState; }

        /**
         * Temporarily decreases the number of callers of the managed object.
         * May only be called if #isOk() returns |true|. Note that #rc() will
         * return E_FAIL after this method succeeds.
         */
        void release()
        {
            Assert (SUCCEEDED (mRC));
            if (SUCCEEDED (mRC))
            {
                if (mObj)
                    mObj->releaseCaller();
                mRC = E_FAIL;
            }
        }

        /**
         * Restores the number of callers decreased by #release(). May only be
         * called after #release().
         */
        void add()
        {
            Assert (!SUCCEEDED (mRC));
            if (mObj && !SUCCEEDED (mRC))
                mRC = mObj->addCaller (&mState, aLimited);
        }

        /**
         * Attaches another object to this caller instance.
         * The previous object's caller is released before the new one is added.
         *
         * @param aObj  New object to attach, may be @c NULL.
         */
        void attach (VirtualBoxBaseProto *aObj)
        {
            /* detect simple self-reattachment */
            if (mObj != aObj)
            {
                if (mObj && SUCCEEDED (mRC))
                    release();
                mObj = aObj;
                add();
            }
        }

        /** Verbose equivalent to <tt>attach (NULL)</tt>. */
        void detach() { attach (NULL); }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoCallerBase)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoCallerBase)

        VirtualBoxBaseProto *mObj;
        HRESULT mRC;
        State mState;
    };

    /**
     * Smart class that automatically increases the number of normal
     * (non-limited) callers of the given VirtualBoxBase object when an instance
     * is constructed and decreases it back when the created instance goes out
     * of scope (i.e. gets destroyed).
     *
     * A typical usage pattern to declare a normal method of some object (i.e. a
     * method that is valid only when the object provides its full
     * functionality) is:
     * <code>
     * STDMETHODIMP Component::Foo()
     * {
     *     AutoCaller autoCaller (this);
     *     CheckComRCReturnRC (autoCaller.rc());
     *     ...
     * </code>
     *
     * Using this class is equivalent to using the AutoCallerBase template with
     * the @a aLimited argument set to |false|, but this class is preferred
     * because provides better self-descriptiveness.
     *
     * See AutoCallerBase for more information about auto caller functionality.
     */
    typedef AutoCallerBase <false> AutoCaller;

    /**
     * Smart class that automatically increases the number of limited callers of
     * the given VirtualBoxBase object when an instance is constructed and
     * decreases it back when the created instance goes out of scope (i.e. gets
     * destroyed).
     *
     * A typical usage pattern to declare a limited method of some object (i.e.
     * a method that is valid even if the object doesn't provide its full
     * functionality) is:
     * <code>
     * STDMETHODIMP Component::Bar()
     * {
     *     AutoLimitedCaller autoCaller (this);
     *     CheckComRCReturnRC (autoCaller.rc());
     *     ...
     * </code>
     *
     * Using this class is equivalent to using the AutoCallerBase template with
     * the @a aLimited argument set to |true|, but this class is preferred
     * because provides better self-descriptiveness.
     *
     * See AutoCallerBase for more information about auto caller functionality.
     */
    typedef AutoCallerBase <true> AutoLimitedCaller;

protected:

    /**
     * Smart class to enclose the state transition NotReady->InInit->Ready.
     *
     * The purpose of this span is to protect object initialization.
     *
     * Instances must be created as a stack-based variable taking |this| pointer
     * as the argument at the beginning of init() methods of VirtualBoxBase
     * subclasses. When this variable is created it automatically places the
     * object to the InInit state.
     *
     * When the created variable goes out of scope (i.e. gets destroyed) then,
     * depending on the result status of this initialization span, it either
     * places the object to Ready or Limited state or calls the object's
     * VirtualBoxBase::uninit() method which is supposed to place the object
     * back to the NotReady state using the AutoUninitSpan class.
     *
     * The initial result status of the initialization span is determined by the
     * @a aResult argument of the AutoInitSpan constructor (Result::Failed by
     * default). Inside the initialization span, the success status can be set
     * to Result::Succeeded using #setSucceeded(), to to Result::Limited using
     * #setLimited() or to Result::Failed using #setFailed(). Please don't
     * forget to set the correct success status before getting the AutoInitSpan
     * variable destroyed (for example, by performing an early return from
     * the init() method)!
     *
     * Note that if an instance of this class gets constructed when the object
     * is in the state other than NotReady, #isOk() returns |false| and methods
     * of this class do nothing: the state transition is not performed.
     *
     * A typical usage pattern is:
     * <code>
     * HRESULT Component::init()
     * {
     *     AutoInitSpan autoInitSpan (this);
     *     AssertReturn (autoInitSpan.isOk(), E_FAIL);
     *     ...
     *     if (FAILED (rc))
     *         return rc;
     *     ...
     *     if (SUCCEEDED (rc))
     *         autoInitSpan.setSucceeded();
     *     return rc;
     * }
     * </code>
     *
     * @note Never create instances of this class outside init() methods of
     *       VirtualBoxBase subclasses and never pass anything other than |this|
     *       as the argument to the constructor!
     */
    class AutoInitSpan
    {
    public:

        enum Result { Failed = 0x0, Succeeded = 0x1, Limited = 0x2 };

        AutoInitSpan (VirtualBoxBaseProto *aObj, Result aResult = Failed);
        ~AutoInitSpan();

        /**
         * Returns |true| if this instance has been created at the right moment
         * (when the object was in the NotReady state) and |false| otherwise.
         */
        bool isOk() const { return mOk; }

        /**
         * Sets the initialization status to Succeeded to indicates successful
         * initialization. The AutoInitSpan destructor will place the managed
         * VirtualBoxBase object to the Ready state.
         */
        void setSucceeded() { mResult = Succeeded; }

        /**
         * Sets the initialization status to Succeeded to indicate limited
         * (partly successful) initialization. The AutoInitSpan destructor will
         * place the managed VirtualBoxBase object to the Limited state.
         */
        void setLimited() { mResult = Limited; }

        /**
         * Sets the initialization status to Failure to indicates failed
         * initialization. The AutoInitSpan destructor will place the managed
         * VirtualBoxBase object to the InitFailed state and will automatically
         * call its uninit() method which is supposed to place the object back
         * to the NotReady state using AutoUninitSpan.
         */
        void setFailed() { mResult = Failed; }

        /** Returns the current initialization result. */
        Result result() { return mResult; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoInitSpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoInitSpan)

        VirtualBoxBaseProto *mObj;
        Result mResult : 3; // must be at least total number of bits + 1 (sign)
        bool mOk : 1;
    };

    /**
     * Smart class to enclose the state transition Limited->InInit->Ready.
     *
     * The purpose of this span is to protect object re-initialization.
     *
     * Instances must be created as a stack-based variable taking |this| pointer
     * as the argument at the beginning of methods of VirtualBoxBase
     * subclasses that try to re-initialize the object to bring it to the Ready
     * state (full functionality) after partial initialization (limited
     * functionality). When this variable is created, it automatically places
     * the object to the InInit state.
     *
     * When the created variable goes out of scope (i.e. gets destroyed),
     * depending on the success status of this initialization span, it either
     * places the object to the Ready state or brings it back to the Limited
     * state.
     *
     * The initial success status of the re-initialization span is |false|. In
     * order to make it successful, #setSucceeded() must be called before the
     * instance is destroyed.
     *
     * Note that if an instance of this class gets constructed when the object
     * is in the state other than Limited, #isOk() returns |false| and methods
     * of this class do nothing: the state transition is not performed.
     *
     * A typical usage pattern is:
     * <code>
     * HRESULT Component::reinit()
     * {
     *     AutoReinitSpan autoReinitSpan (this);
     *     AssertReturn (autoReinitSpan.isOk(), E_FAIL);
     *     ...
     *     if (FAILED (rc))
     *         return rc;
     *     ...
     *     if (SUCCEEDED (rc))
     *         autoReinitSpan.setSucceeded();
     *     return rc;
     * }
     * </code>
     *
     * @note Never create instances of this class outside re-initialization
     * methods of VirtualBoxBase subclasses and never pass anything other than
     * |this| as the argument to the constructor!
     */
    class AutoReinitSpan
    {
    public:

        AutoReinitSpan (VirtualBoxBaseProto *aObj);
        ~AutoReinitSpan();

        /**
         * Returns |true| if this instance has been created at the right moment
         * (when the object was in the Limited state) and |false| otherwise.
         */
        bool isOk() const { return mOk; }

        /**
         * Sets the re-initialization status to Succeeded to indicates
         * successful re-initialization. The AutoReinitSpan destructor will place
         * the managed VirtualBoxBase object to the Ready state.
         */
        void setSucceeded() { mSucceeded = true; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoReinitSpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoReinitSpan)

        VirtualBoxBaseProto *mObj;
        bool mSucceeded : 1;
        bool mOk : 1;
    };

    /**
     * Smart class to enclose the state transition Ready->InUnnit->NotReady,
     * InitFailed->InUnnit->NotReady or WillUninit->InUnnit->NotReady.
     *
     * The purpose of this span is to protect object uninitialization.
     *
     * Instances must be created as a stack-based variable taking |this| pointer
     * as the argument at the beginning of uninit() methods of VirtualBoxBase
     * subclasses. When this variable is created it automatically places the
     * object to the InUninit state, unless it is already in the NotReady state
     * as indicated by #uninitDone() returning |true|. In the latter case, the
     * uninit() method must immediately return because there should be nothing
     * to uninitialize.
     *
     * When this variable goes out of scope (i.e. gets destroyed), it places the
     * object to NotReady state.
     *
     * A typical usage pattern is:
     * <code>
     * void Component::uninit()
     * {
     *     AutoUninitSpan autoUninitSpan (this);
     *     if (autoUninitSpan.uninitDone())
     *         return;
     *     ...
     * }
     * </code>
     *
     * @note The constructor of this class blocks the current thread execution
     *       until the number of callers added to the object using #addCaller()
     *       or AutoCaller drops to zero. For this reason, it is forbidden to
     *       create instances of this class (or call uninit()) within the
     *       AutoCaller or #addCaller() scope because it is a guaranteed
     *       deadlock.
     *
     * @note Never create instances of this class outside uninit() methods and
     *       never pass anything other than |this| as the argument to the
     *       constructor!
     */
    class AutoUninitSpan
    {
    public:

        AutoUninitSpan (VirtualBoxBaseProto *aObj);
        ~AutoUninitSpan();

        /** |true| when uninit() is called as a result of init() failure */
        bool initFailed() { return mInitFailed; }

        /** |true| when uninit() has already been called (so the object is NotReady) */
        bool uninitDone() { return mUninitDone; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoUninitSpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoUninitSpan)

        VirtualBoxBaseProto *mObj;
        bool mInitFailed : 1;
        bool mUninitDone : 1;
    };

    /**
     * Smart class to enclose the state transition Ready->MayUninit->NotReady or
     * Ready->MayUninit->WillUninit.
     *
     * The purpose of this span is to safely check if unintialization is
     * possible at the given moment and seamlessly perform it if so.
     *
     * Instances must be created as a stack-based variable taking |this| pointer
     * as the argument at the beginning of methods of VirtualBoxBase
     * subclasses that want to uninitialize the object if a necessary set of
     * criteria is met and leave it Ready otherwise.
     *
     * When this variable is created it automatically places the object to the
     * MayUninit state if it is Ready, does nothing but returns |true| in
     * response to #alreadyInProgress() if it is already in MayUninit, or
     * returns a failure in response to #rc() in any other case. The example
     * below shows how the user must react in latter two cases.
     *
     * When this variable goes out of scope (i.e. gets destroyed), it places the
     * object back to Ready state unless #acceptUninit() is called in which case
     * the object is placed to WillUninit state and uninit() is immediately
     * called after that.
     *
     * A typical usage pattern is:
     * <code>
     * void Component::uninit()
     * {
     *     AutoMayUninitSpan mayUninitSpan (this);
     *     CheckComRCReturnRC (mayUninitSpan.rc());
     *     if (mayUninitSpan.alreadyInProgress())
     *          return S_OK;
     *     ...
     *     if (FAILED (rc))
     *         return rc; // will go back to Ready
     *     ...
     *     if (SUCCEEDED (rc))
     *         mayUninitSpan.acceptUninit(); // will call uninit()
     *     return rc;
     * }
     * </code>
     *
     * @note The constructor of this class blocks the current thread execution
     *       until the number of callers added to the object using #addCaller()
     *       or AutoCaller drops to zero. For this reason, it is forbidden to
     *       create instances of this class (or call uninit()) within the
     *       AutoCaller or #addCaller() scope because it is a guaranteed
     *       deadlock.
     */
    class AutoMayUninitSpan
    {
    public:

        AutoMayUninitSpan (VirtualBoxBaseProto *aObj);
        ~AutoMayUninitSpan();

        /**
         * Returns a failure if the AutoMayUninitSpan variable was constructed
         * at an improper time. If there is a failure, do nothing but return
         * it to the caller.
         */
        HRESULT rc() { return mRC; }

        /**
         * Returns |true| if AutoMayUninitSpan is already in progress on some
         * other thread. If it's the case, do nothing but return S_OK to
         * the caller.
         */
        bool alreadyInProgress() { return mAlreadyInProgress; }

        /*
         * Accepts uninitialization and causes the destructor to go to
         * WillUninit state and call uninit() afterwards.
         */
        void acceptUninit() { mAcceptUninit = true; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoMayUninitSpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoMayUninitSpan)

        VirtualBoxBaseProto *mObj;

        HRESULT mRC;
        bool mAlreadyInProgress : 1;
        bool mAcceptUninit : 1;
    };

    /**
     * Returns a lock handle used to protect the primary state fields (used by
     * #addCaller(), AutoInitSpan, AutoUninitSpan, etc.). Only intended to be
     * used for similar purposes in subclasses. WARNING: NO any other locks may
     * be requested while holding this lock!
     */
    WriteLockHandle *stateLockHandle() { return &mStateLock; }

private:

    void setState (State aState)
    {
        Assert (mState != aState);
        mState = aState;
        mStateChangeThread = RTThreadSelf();
    }

    /** Primary state of this object */
    State mState;
    /** Thread that caused the last state change */
    RTTHREAD mStateChangeThread;
    /** Total number of active calls to this object */
    unsigned mCallers;
    /** Posted when the number of callers drops to zero */
    RTSEMEVENT mZeroCallersSem;
    /** Posted when the object goes from InInit/InUninit to some other state */
    RTSEMEVENTMULTI mInitUninitSem;
    /** Number of threads waiting for mInitUninitDoneSem */
    unsigned mInitUninitWaiters;

    /** Protects access to state related data members */
    WriteLockHandle mStateLock;

    /** User-level object lock for subclasses */
    mutable RWLockHandle *mObjectLock;
};

////////////////////////////////////////////////////////////////////////////////

/**
 *  This macro adds the error info support to methods of the VirtualBoxBase
 *  class (by overriding them). Place it to the public section of the
 *  VirtualBoxBase subclass and the following methods will set the extended
 *  error info in case of failure instead of just returning the result code:
 *
 *  <ul>
 *      <li>VirtualBoxBase::addCaller()
 *  </ul>
 *
 *  @note The given VirtualBoxBase subclass must also inherit from both
 *  VirtualBoxSupportErrorInfoImpl and VirtualBoxSupportTranslation templates!
 *
 *  @param C    VirtualBoxBase subclass to add the error info support to
 */
#define VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(C) \
    virtual HRESULT addCaller (VirtualBoxBaseProto::State *aState = NULL, \
                               bool aLimited = false) \
    { \
        VirtualBoxBaseProto::State state; \
        HRESULT rc = VirtualBoxBaseProto::addCaller (&state, aLimited); \
        if (FAILED (rc)) \
        { \
            if (state == VirtualBoxBaseProto::Limited) \
                rc = setError (rc, tr ("The object functionality is limited")); \
            else \
                rc = setError (rc, tr ("The object is not ready")); \
        } \
        if (aState) \
            *aState = state; \
        return rc; \
    } \

////////////////////////////////////////////////////////////////////////////////

/// @todo (dmik) remove after we switch to VirtualBoxBaseNEXT completely
class ATL_NO_VTABLE VirtualBoxBase
    : virtual public VirtualBoxBaseProto
#if !defined (VBOX_WITH_XPCOM)
    , public CComObjectRootEx <CComMultiThreadModel>
#else
    , public CComObjectRootEx
#endif
{

public:
    VirtualBoxBase()
    {
        mReady = false;
    }
    virtual ~VirtualBoxBase()
    {
    }

    /**
     *  Virtual unintialization method. Called during parent object's
     *  uninitialization, if the given subclass instance is a dependent child of
     *  a class derived from VirtualBoxBaseWithChildren (@sa
     *  VirtualBoxBaseWithChildren::addDependentChild). In this case, this
     *  method's implementation must call setReady (false),
     */
    virtual void uninit() {}


    // sets the ready state of the object
    void setReady(bool isReady)
    {
        mReady = isReady;
    }
    // get the ready state of the object
    bool isReady()
    {
        return mReady;
    }

    static const char *translate (const char *context, const char *sourceText,
                                  const char *comment = 0);

private:

    // flag determining whether an object is ready
    // for usage, i.e. methods may be called
    bool mReady;
    // mutex semaphore to lock the object
};

/**
 *  Temporary class to disable deprecated methods of VirtualBoxBase.
 *  Can be used as a base for components that are completely switched to
 *  the new locking scheme (VirtualBoxBaseProto).
 *
 *  @todo remove after we switch to VirtualBoxBaseNEXT completely.
 */
class VirtualBoxBaseNEXT : public VirtualBoxBase
{
private:

    void lock();
    void unlock();
    void setReady (bool isReady);
    bool isReady();
};

////////////////////////////////////////////////////////////////////////////////

/** Helper for VirtualBoxSupportTranslation. */
class VirtualBoxSupportTranslationBase
{
protected:
    static bool cutClassNameFrom__PRETTY_FUNCTION__ (char *aPrettyFunctionName);
};

/**
 * The VirtualBoxSupportTranslation template implements the NLS string
 * translation support for the given class.
 *
 * Translation support is provided by the static #tr() function. This function,
 * given a string in UTF-8 encoding, looks up for a translation of the given
 * string by calling the VirtualBoxBase::translate() global function which
 * receives the name of the enclosing class ("context of translation") as the
 * additional argument and returns a translated string based on the currently
 * active language.
 *
 * @param C     Class that needs to support the string translation.
 *
 * @note Every class that wants to use the #tr() function in its own methods
 *       must inherit from this template, regardless of whether its base class
 *       (if any) inherits from it or not. Otherwise, the translation service
 *       will not work correctly. However, the declaration of the derived
 *       class must contain
 *       the <tt>COM_SUPPORTTRANSLATION_OVERRIDE (<ClassName>)</tt> macro if one
 *       of its base classes also inherits from this template (to resolve the
 *       ambiguity of the #tr() function).
 */
template <class C>
class VirtualBoxSupportTranslation : virtual protected VirtualBoxSupportTranslationBase
{
public:

    /**
     * Translates the given text string by calling VirtualBoxBase::translate()
     * and passing the name of the C class as the first argument ("context of
     * translation") See VirtualBoxBase::translate() for more info.
     *
     * @param aSourceText   String to translate.
     * @param aComment      Comment to the string to resolve possible
     *                      ambiguities (NULL means no comment).
     *
     * @return Translated version of the source string in UTF-8 encoding, or
     *      the source string itself if the translation is not found in the
     *      specified context.
     */
    inline static const char *tr (const char *aSourceText,
                                  const char *aComment = NULL)
    {
        return VirtualBoxBase::translate (className(), aSourceText, aComment);
    }

protected:

    static const char *className()
    {
        static char fn [sizeof (__PRETTY_FUNCTION__) + 1];
        if (!sClassName)
        {
            strcpy (fn, __PRETTY_FUNCTION__);
            cutClassNameFrom__PRETTY_FUNCTION__ (fn);
            sClassName = fn;
        }
        return sClassName;
    }

private:

    static const char *sClassName;
};

template <class C>
const char *VirtualBoxSupportTranslation <C>::sClassName = NULL;

/**
 * This macro must be invoked inside the public section of the declaration of
 * the class inherited from the VirtualBoxSupportTranslation template in case
 * if one of its other base classes also inherits from that template. This is
 * necessary to resolve the ambiguity of the #tr() function.
 *
 * @param C     Class that inherits the VirtualBoxSupportTranslation template
 *              more than once (through its other base clases).
 */
#define VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(C) \
    inline static const char *tr (const char *aSourceText, \
                                  const char *aComment = NULL) \
    { \
        return VirtualBoxSupportTranslation <C>::tr (aSourceText, aComment); \
    }

/**
 * Dummy macro that is used to shut down Qt's lupdate tool warnings in some
 * situations. This macro needs to be present inside (better at the very
 * beginning) of the declaration of the class that inherits from
 * VirtualBoxSupportTranslation template, to make lupdate happy.
 */
#define Q_OBJECT

////////////////////////////////////////////////////////////////////////////////

/**
 *  Helper for the VirtualBoxSupportErrorInfoImpl template.
 */
/// @todo switch to com::SupportErrorInfo* and remove
class VirtualBoxSupportErrorInfoImplBase
{
    static HRESULT setErrorInternal (HRESULT aResultCode, const GUID &aIID,
                                     const Bstr &aComponent, const Bstr &aText,
                                     bool aWarning, bool aLogIt);

protected:

    /**
     * The MultiResult class is a com::FWResult enhancement that also acts as a
     * switch to turn on multi-error mode for #setError() or #setWarning()
     * calls.
     *
     * When an instance of this class is created, multi-error mode is turned on
     * for the current thread and the turn-on counter is increased by one. In
     * multi-error mode, a call to #setError() or #setWarning() does not
     * overwrite the current error or warning info object possibly set on the
     * current thread by other method calls, but instead it stores this old
     * object in the IVirtualBoxErrorInfo::next attribute of the new error
     * object being set.
     *
     * This way, error/warning objects are stacked together and form a chain of
     * errors where the most recent error is the first one retrieved by the
     * calling party, the preceding error is what the
     * IVirtualBoxErrorInfo::next attribute of the first error points to, and so
     * on, up to the first error or warning occurred which is the last in the
     * chain. See IVirtualBoxErrorInfo documentation for more info.
     *
     * When the instance of the MultiResult class goes out of scope and gets
     * destroyed, it automatically decreases the turn-on counter by one. If
     * the counter drops to zero, multi-error mode for the current thread is
     * turned off and the thread switches back to single-error mode where every
     * next error or warning object overwrites the previous one.
     *
     * Note that the caller of a COM method uses a non-S_OK result code to
     * decide if the method has returned an error (negative codes) or a warning
     * (positive non-zero codes) and will query extended error info only in
     * these two cases. However, since multi-error mode implies that the method
     * doesn't return control return to the caller immediately after the first
     * error or warning but continues its execution, the functionality provided
     * by the base com::FWResult class becomes very useful because it allows to
     * preserve the error or the warning result code even if it is later assigned
     * a S_OK value multiple times. See com::FWResult for details.
     *
     * Here is the typical usage pattern:
     *  <code>

        HRESULT Bar::method()
        {
            // assume multi-errors are turned off here...

            if (something)
            {
                // Turn on multi-error mode and make sure severity is preserved
                MultiResult rc = foo->method1();

                // return on fatal error, but continue on warning or on success
                CheckComRCReturnRC (rc);

                rc = foo->method2();
                // no matter what result, stack it and continue

                // ...

                // return the last worst result code (it will be preserved even if
                // foo->method2() returns S_OK.
                return rc;
            }

            // multi-errors are turned off here again...

            return S_OK;
        }

     *  </code>
     *
     *
     * @note This class is intended to be instantiated on the stack, therefore
     *       You cannot create them using new(). Although it is possible to copy
     *       instances of MultiResult or return them by value, please never do
     *       that as it is breaks the class semantics (and will assert).
     */
    class MultiResult : public com::FWResult
    {
    public:

        /**
         * @copydoc com::FWResult::FWResult().
         */
        MultiResult (HRESULT aRC = E_FAIL) : FWResult (aRC) { init(); }

        MultiResult (const MultiResult &aThat) : FWResult (aThat)
        {
            /* We need this copy constructor only for GCC that wants to have
             * it in case of expressions like |MultiResult rc = E_FAIL;|. But
             * we assert since the optimizer should actually avoid the
             * temporary and call the other constructor directly instead. */
            AssertFailed();
            init();
        }

        ~MultiResult();

        MultiResult &operator= (HRESULT aRC)
        {
            com::FWResult::operator= (aRC);
            return *this;
        }

        MultiResult &operator= (const MultiResult &aThat)
        {
            /* We need this copy constructor only for GCC that wants to have
             * it in case of expressions like |MultiResult rc = E_FAIL;|. But
             * we assert since the optimizer should actually avoid the
             * temporary and call the other constructor directly instead. */
            AssertFailed();
            com::FWResult::operator= (aThat);
            return *this;
        }

    private:

        DECLARE_CLS_NEW_DELETE_NOOP (MultiResult)

        void init();

        static RTTLS sCounter;

        friend class VirtualBoxSupportErrorInfoImplBase;
    };

    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const Bstr &aComponent,
                             const Bstr &aText,
                             bool aLogIt = true)
    {
        return setErrorInternal (aResultCode, aIID, aComponent, aText,
                                 false /* aWarning */, aLogIt);
    }

    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const Bstr &aComponent,
                               const Bstr &aText)
    {
        return setErrorInternal (aResultCode, aIID, aComponent, aText,
                                 true /* aWarning */, true /* aLogIt */);
    }

    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const Bstr &aComponent,
                             const char *aText, va_list aArgs, bool aLogIt = true)
    {
        return setErrorInternal (aResultCode, aIID, aComponent,
                                 Utf8StrFmtVA (aText, aArgs),
                                 false /* aWarning */, aLogIt);
    }

    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const Bstr &aComponent,
                               const char *aText, va_list aArgs)
    {
        return setErrorInternal (aResultCode, aIID, aComponent,
                                 Utf8StrFmtVA (aText, aArgs),
                                 true /* aWarning */, true /* aLogIt */);
    }
};

/**
 *  This template implements ISupportErrorInfo for the given component class
 *  and provides the #setError() method to conveniently set the error information
 *  from within interface methods' implementations.
 *
 *  On Windows, the template argument must define a COM interface map using
 *  BEGIN_COM_MAP / END_COM_MAP macros and this map must contain a
 *  COM_INTERFACE_ENTRY(ISupportErrorInfo) definition. All interface entries
 *  that follow it will be considered to support IErrorInfo, i.e. the
 *  InterfaceSupportsErrorInfo() implementation will return S_OK for the
 *  corresponding IID.
 *
 *  On all platforms, the template argument must also define the following
 *  method: |public static const wchar_t *C::getComponentName()|. See
 *  #setError (HRESULT, const char *, ...) for a description on how it is
 *  used.
 *
 *  @param C
 *      component class that implements one or more COM interfaces
 *  @param I
 *      default interface for the component. This interface's IID is used
 *      by the shortest form of #setError, for convenience.
 */
/// @todo switch to com::SupportErrorInfo* and remove
template <class C, class I>
class ATL_NO_VTABLE VirtualBoxSupportErrorInfoImpl
    : protected VirtualBoxSupportErrorInfoImplBase
#if !defined (VBOX_WITH_XPCOM)
    , public ISupportErrorInfo
#else
#endif
{
public:

#if !defined (VBOX_WITH_XPCOM)
    STDMETHOD(InterfaceSupportsErrorInfo) (REFIID riid)
    {
        const _ATL_INTMAP_ENTRY* pEntries = C::_GetEntries();
        Assert (pEntries);
        if (!pEntries)
            return S_FALSE;

        BOOL bSupports = FALSE;
        BOOL bISupportErrorInfoFound = FALSE;

        while (pEntries->pFunc != NULL && !bSupports)
        {
            if (!bISupportErrorInfoFound)
            {
                // skip the com map entries until ISupportErrorInfo is found
                bISupportErrorInfoFound =
                    InlineIsEqualGUID (*(pEntries->piid), IID_ISupportErrorInfo);
            }
            else
            {
                // look for the requested interface in the rest of the com map
                bSupports = InlineIsEqualGUID (*(pEntries->piid), riid);
            }
            pEntries++;
        }

        Assert (bISupportErrorInfoFound);

        return bSupports ? S_OK : S_FALSE;
    }
#endif // !defined (VBOX_WITH_XPCOM)

protected:

    /**
     *  Sets the error information for the current thread.
     *  This information can be retrieved by a caller of an interface method
     *  using IErrorInfo on Windows or nsIException on Linux, or the cross-platform
     *  IVirtualBoxErrorInfo interface that provides extended error info (only
     *  for components from the VirtualBox COM library). Alternatively, the
     *  platform-independent class com::ErrorInfo (defined in VBox[XP]COM.lib)
     *  can be used to retrieve error info in a convenient way.
     *
     *  It is assumed that the interface method that uses this function returns
     *  an unsuccessful result code to the caller (otherwise, there is no reason
     *  for the caller to try to retrieve error info after method invocation).
     *
     *  Here is a table of correspondence between this method's arguments
     *  and IErrorInfo/nsIException/IVirtualBoxErrorInfo attributes/methods:
     *
     *  argument    IErrorInfo      nsIException    IVirtualBoxErrorInfo
     *  ----------------------------------------------------------------
     *  resultCode  --              result          resultCode
     *  iid         GetGUID         --              interfaceID
     *  component   GetSource       --              component
     *  text        GetDescription  message         text
     *
     *  This method is rarely needs to be used though. There are more convenient
     *  overloaded versions, that automatically substitute some arguments
     *  taking their values from the template parameters. See
     *  #setError (HRESULT, const char *, ...) for an example.
     *
     *  @param  aResultCode result (error) code, must not be S_OK
     *  @param  aIID        IID of the interface that defines the error
     *  @param  aComponent  name of the component that generates the error
     *  @param  aText       error message (must not be null), an RTStrPrintf-like
     *                      format string in UTF-8 encoding
     *  @param  ...         list of arguments for the format string
     *
     *  @return
     *      the error argument, for convenience, If an error occurs while
     *      creating error info itself, that error is returned instead of the
     *      error argument.
     */
    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const wchar_t *aComponent,
                             const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, aIID, aComponent, aText, args, true /* aLogIt */);
        va_end (args);
        return rc;
    }

    /**
     *  This method is the same as #setError() except that it makes sure @a
     *  aResultCode doesn't have the error severity bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereof you can
     *  use ordinary E_XXX result code constants, for convenience. However, this
     *  behavior may be non-standard on some COM platforms.
     */
    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const wchar_t *aComponent,
                               const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setWarning
            (aResultCode, aIID, aComponent, aText, args);
        va_end (args);
        return rc;
    }

    /**
     *  Sets the error information for the current thread.
     *  A convenience method that automatically sets the default interface
     *  ID (taken from the I template argument) and the component name
     *  (a value of C::getComponentName()).
     *
     *  See #setError (HRESULT, const GUID &, const wchar_t *, const char *text, ...)
     *  for details.
     *
     *  This method is the most common (and convenient) way  to set error
     *  information from within interface methods. A typical pattern of usage
     *  is looks like this:
     *
     *  <code>
     *      return setError (E_FAIL, "Terrible Error");
     *  </code>
     *  or
     *  <code>
     *      HRESULT rc = setError (E_FAIL, "Terrible Error");
     *      ...
     *      return rc;
     *  </code>
     */
    static HRESULT setError (HRESULT aResultCode, const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, args, true /* aLogIt */);
        va_end (args);
        return rc;
    }

    /**
     *  This method is the same as #setError() except that it makes sure @a
     *  aResultCode doesn't have the error severity bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereof you can
     *  use ordinary E_XXX result code constants, for convenience. However, this
     *  behavior may be non-standard on some COM platforms.
     */
    static HRESULT setWarning (HRESULT aResultCode, const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setWarning
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, args);
        va_end (args);
        return rc;
    }

    /**
     *  Sets the error information for the current thread, va_list variant.
     *  A convenience method that automatically sets the default interface
     *  ID (taken from the I template argument) and the component name
     *  (a value of C::getComponentName()).
     *
     *  See #setError (HRESULT, const GUID &, const wchar_t *, const char *text, ...)
     *  and #setError (HRESULT, const char *, ...)  for details.
     */
    static HRESULT setErrorV (HRESULT aResultCode, const char *aText,
                              va_list aArgs)
    {
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, aArgs, true /* aLogIt */);
        return rc;
    }

    /**
     *  This method is the same as #setErrorV() except that it makes sure @a
     *  aResultCode doesn't have the error severity bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereof you can
     *  use ordinary E_XXX result code constants, for convenience. However, this
     *  behavior may be non-standard on some COM platforms.
     */
    static HRESULT setWarningV (HRESULT aResultCode, const char *aText,
                                va_list aArgs)
    {
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setWarning
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, aArgs);
        return rc;
    }

    /**
     *  Sets the error information for the current thread, BStr variant.
     *  A convenience method that automatically sets the default interface
     *  ID (taken from the I template argument) and the component name
     *  (a value of C::getComponentName()).
     *
     *  This method is preferred if you have a ready (translated and formatted)
     *  Bstr string, because it omits an extra conversion Utf8Str -> Bstr.
     *
     *  See #setError (HRESULT, const GUID &, const wchar_t *, const char *text, ...)
     *  and #setError (HRESULT, const char *, ...)  for details.
     */
    static HRESULT setErrorBstr (HRESULT aResultCode, const Bstr &aText)
    {
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, true /* aLogIt */);
        return rc;
    }

    /**
     *  This method is the same as #setErrorBstr() except that it makes sure @a
     *  aResultCode doesn't have the error severity bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereof you can
     *  use ordinary E_XXX result code constants, for convenience. However, this
     *  behavior may be non-standard on some COM platforms.
     */
    static HRESULT setWarningBstr (HRESULT aResultCode, const Bstr &aText)
    {
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setWarning
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText);
        return rc;
    }

    /**
     *  Sets the error information for the current thread.
     *  A convenience method that automatically sets the component name
     *  (a value of C::getComponentName()), but allows to specify the interface
     *  id manually.
     *
     *  See #setError (HRESULT, const GUID &, const wchar_t *, const char *text, ...)
     *  for details.
     */
    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, aIID, C::getComponentName(), aText, args, true /* aLogIt */);
        va_end (args);
        return rc;
    }

    /**
     *  This method is the same as #setError() except that it makes sure @a
     *  aResultCode doesn't have the error severity bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereof you can
     *  use ordinary E_XXX result code constants, for convenience. However, this
     *  behavior may be non-standard on some COM platforms.
     */
    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setWarning
            (aResultCode, aIID, C::getComponentName(), aText, args);
        va_end (args);
        return rc;
    }

    /**
     *  Sets the error information for the current thread but doesn't put
     *  anything in the release log. This is very useful for avoiding
     *  harmless error from causing confusion.
     *
     *  It is otherwise identical to #setError (HRESULT, const char *text, ...).
     */
    static HRESULT setErrorNoLog (HRESULT aResultCode, const char *aText, ...)
    {
        va_list args;
        va_start (args, aText);
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, args, false /* aLogIt */);
        va_end (args);
        return rc;
    }

private:

};

////////////////////////////////////////////////////////////////////////////////

/**
 *  Base class to track VirtualBoxBase children of the component.
 *
 *  This class is a preferable VirtualBoxBase replacement for components
 *  that operate with collections of child components. It gives two useful
 *  possibilities:
 *
 *  <ol><li>
 *      Given an IUnknown instance, it's possible to quickly determine
 *      whether this instance represents a child object created by the given
 *      component, and if so, get a valid VirtualBoxBase pointer to the child
 *      object. The returned pointer can be then safely casted to the
 *      actual class of the child object (to get access to its "internal"
 *      non-interface methods) provided that no other child components implement
 *      the same initial interface IUnknown is queried from.
 *  </li><li>
 *      When the parent object uninitializes itself, it can easily unintialize
 *      all its VirtualBoxBase derived children (using their
 *      VirtualBoxBase::uninit() implementations). This is done simply by
 *      calling the #uninitDependentChildren() method.
 *  </li></ol>
 *
 *  In order to let the above work, the following must be done:
 *  <ol><li>
 *      When a child object is initialized, it calls #addDependentChild() of
 *      its parent to register itself within the list of dependent children.
 *  </li><li>
 *      When a child object it is uninitialized, it calls #removeDependentChild()
 *      to unregister itself. This must be done <b>after</b> the child has called
 *      setReady(false) to indicate it is no more valid, and <b>not</b> from under
 *      the child object's lock. Note also, that the first action the child's
 *      uninit() implementation must do is to check for readiness after acquiring
 *      the object's lock and return immediately if not ready.
 *  </li></ol>
 *
 *  Children added by #addDependentChild() are <b>weakly</b> referenced
 *  (i.e. AddRef() is not called), so when a child is externally destructed
 *  (i.e. its reference count goes to zero), it will automatically remove
 *  itself from a map of dependent children, provided that it follows the
 *  rules described here.
 *
 *  @note
 *  Because of weak referencing, deadlocks and assertions are very likely
 *  if #addDependentChild() or #removeDependentChild() are used incorrectly
 *  (called at inappropriate times). Check the above rules once more.
 *
 *  @deprecated Use VirtualBoxBaseWithChildrenNEXT for new classes.
 */
class VirtualBoxBaseWithChildren : public VirtualBoxBase
{
public:

    VirtualBoxBaseWithChildren()
        : mUninitDoneSem (NIL_RTSEMEVENT), mChildrenLeft (0)
    {}

    virtual ~VirtualBoxBaseWithChildren()
    {}

    /**
     *  Adds the given child to the map of dependent children.
     *  Intended to be called from the child's init() method,
     *  from under the child's lock.
     *
     *  @param C    the child object to add (must inherit VirtualBoxBase AND
     *              implement some interface)
     */
    template <class C>
    void addDependentChild (C *child)
    {
        AssertReturn (child, (void) 0);
        addDependentChild (child, child);
    }

    /**
     *  Removes the given child from the map of dependent children.
     *  Must be called <b>after<b> the child has called setReady(false), and
     *  <b>not</b> from under the child object's lock.
     *
     *  @param C    the child object to remove (must inherit VirtualBoxBase AND
     *              implement some interface)
     */
    template <class C>
    void removeDependentChild (C *child)
    {
        AssertReturn (child, (void) 0);
        /// @todo (r=dmik) the below check (and the relevant comment above)
        //  seems to be not necessary any more once we completely switch to
        //  the NEXT locking scheme. This requires altering removeDependentChild()
        //  and uninitDependentChildren() as well (due to the new state scheme,
        //  there is a separate mutex for state transition, so calling the
        //  child's uninit() from under the children map lock should not produce
        //  dead-locks any more).
        Assert (!child->isWriteLockOnCurrentThread() || child->lockHandle() == lockHandle());
        removeDependentChild (ComPtr <IUnknown> (child));
    }

protected:

    void uninitDependentChildren();

    VirtualBoxBase *getDependentChild (const ComPtr <IUnknown> &unk);

private:

    void addDependentChild (const ComPtr <IUnknown> &unk, VirtualBoxBase *child);
    void removeDependentChild (const ComPtr <IUnknown> &unk);

    typedef std::map <IUnknown *, VirtualBoxBase *> DependentChildren;
    DependentChildren mDependentChildren;

    WriteLockHandle mMapLock;

    RTSEMEVENT mUninitDoneSem;
    unsigned mChildrenLeft;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Base class to track VirtualBoxBaseNEXT chlidren of the component.
 *
 * This class is a preferrable VirtualBoxBase replacement for components that
 * operate with collections of child components. It gives two useful
 * possibilities:
 *
 * <ol><li>
 *      Given an IUnknown instance, it's possible to quickly determine
 *      whether this instance represents a child object that belongs to the
 *      given component, and if so, get a valid VirtualBoxBase pointer to the
 *      child object. The returned pointer can be then safely casted to the
 *      actual class of the child object (to get access to its "internal"
 *      non-interface methods) provided that no other child components implement
 *      the same original COM interface IUnknown is queried from.
 * </li><li>
 *      When the parent object uninitializes itself, it can easily unintialize
 *      all its VirtualBoxBase derived children (using their
 *      VirtualBoxBase::uninit() implementations). This is done simply by
 *      calling the #uninitDependentChildren() method.
 * </li></ol>
 *
 * In order to let the above work, the following must be done:
 * <ol><li>
 *      When a child object is initialized, it calls #addDependentChild() of
 *      its parent to register itself within the list of dependent children.
 * </li><li>
 *      When the child object it is uninitialized, it calls
 *      #removeDependentChild() to unregister itself.
 * </li></ol>
 *
 * Note that if the parent object does not call #uninitDependentChildren() when
 * it gets uninitialized, it must call uninit() methods of individual children
 * manually to disconnect them; a failure to do so will cause crashes in these
 * methods when children get destroyed. The same applies to children not calling
 * #removeDependentChild() when getting destroyed.
 *
 * Note that children added by #addDependentChild() are <b>weakly</b> referenced
 * (i.e. AddRef() is not called), so when a child object is deleted externally
 * (because it's reference count goes to zero), it will automatically remove
 * itself from the map of dependent children provided that it follows the rules
 * described here.
 *
 * Access to the child list is serialized using the #childrenLock() lock handle
 * (which defaults to the general object lock handle (see
 * VirtualBoxBase::lockHandle()). This lock is used by all add/remove methods of
 * this class so be aware of the need to preserve the {parent, child} lock order
 * when calling these methods.
 *
 * Read individual method descriptions to get further information.
 *
 * @todo This is a VirtualBoxBaseWithChildren equivalent that uses the
 *       VirtualBoxBaseNEXT implementation. Will completely supersede
 *       VirtualBoxBaseWithChildren after the old VirtualBoxBase implementation
 *       has gone.
 */
class VirtualBoxBaseWithChildrenNEXT : public VirtualBoxBaseNEXT
{
public:

    VirtualBoxBaseWithChildrenNEXT()
    {}

    virtual ~VirtualBoxBaseWithChildrenNEXT()
    {}

    /**
     * Lock handle to use when adding/removing child objects from the list of
     * children. It is guaranteed that no any other lock is requested in methods
     * of this class while holding this lock.
     *
     * @warning By default, this simply returns the general object's lock handle
     *          (see VirtualBoxBase::lockHandle()) which is sufficient for most
     *          cases.
     */
    virtual RWLockHandle *childrenLock() { return lockHandle(); }

    /**
     * Adds the given child to the list of dependent children.
     *
     * Usually gets called from the child's init() method.
     *
     * @note @a aChild (unless it is in InInit state) must be protected by
     *       VirtualBoxBase::AutoCaller to make sure it is not uninitialized on
     *       another thread during this method's call.
     *
     * @note When #childrenLock() is not overloaded (returns the general object
     *       lock) and this method is called from under the child's read or
     *       write lock, make sure the {parent, child} locking order is
     *       preserved by locking the callee (this object) for writing before
     *       the child's lock.
     *
     * @param aChild    Child object to add (must inherit VirtualBoxBase AND
     *                  implement some interface).
     *
     * @note Locks #childrenLock() for writing.
     */
    template <class C>
    void addDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild != NULL);
        doAddDependentChild (ComPtr <IUnknown> (aChild), aChild);
    }

    /**
     * Equivalent to template <class C> void addDependentChild (C *aChild)
     * but takes a ComObjPtr <C> argument.
     */
    template <class C>
    void addDependentChild (const ComObjPtr <C> &aChild)
    {
        AssertReturnVoid (!aChild.isNull());
        doAddDependentChild (ComPtr <IUnknown> (static_cast <C *> (aChild)), aChild);
    }

    /**
     * Removes the given child from the list of dependent children.
     *
     * Usually gets called from the child's uninit() method.
     *
     * Keep in mind that the called (parent) object may be no longer available
     * (i.e. may be deleted deleted) after this method returns, so you must not
     * call any other parent's methods after that!
     *
     * @note Locks #childrenLock() for writing.
     *
     * @note @a aChild (unless it is in InUninit state) must be protected by
     *       VirtualBoxBase::AutoCaller to make sure it is not uninitialized on
     *       another thread during this method's call.
     *
     * @note When #childrenLock() is not overloaded (returns the general object
     *       lock) and this method is called from under the child's read or
     *       write lock, make sure the {parent, child} locking order is
     *       preserved by locking the callee (this object) for writing before
     *       the child's lock. This is irrelevant when the method is called from
     *       under this object's VirtualBoxBaseProto::AutoUninitSpan (i.e. in
     *       InUninit state) since in this case no locking is done.
     *
     * @param aChild    Child object to remove.
     *
     * @note Locks #childrenLock() for writing.
     */
    template <class C>
    void removeDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild != NULL);
        doRemoveDependentChild (ComPtr <IUnknown> (aChild));
    }

    /**
     * Equivalent to template <class C> void removeDependentChild (C *aChild)
     * but takes a ComObjPtr <C> argument.
     */
    template <class C>
    void removeDependentChild (const ComObjPtr <C> &aChild)
    {
        AssertReturnVoid (!aChild.isNull());
        doRemoveDependentChild (ComPtr <IUnknown> (static_cast <C *> (aChild)));
    }

protected:

    void uninitDependentChildren();

    VirtualBoxBaseNEXT *getDependentChild (const ComPtr <IUnknown> &aUnk);

private:

    /// @todo temporarily reinterpret VirtualBoxBase * as VirtualBoxBaseNEXT *
    //  until ported HardDisk and Progress to the new scheme.
    void doAddDependentChild (IUnknown *aUnk, VirtualBoxBase *aChild)
    {
        doAddDependentChild (aUnk,
                             reinterpret_cast <VirtualBoxBaseNEXT *> (aChild));
    }

    void doAddDependentChild (IUnknown *aUnk, VirtualBoxBaseNEXT *aChild);
    void doRemoveDependentChild (IUnknown *aUnk);

    typedef std::map <IUnknown *, VirtualBoxBaseNEXT *> DependentChildren;
    DependentChildren mDependentChildren;
};

////////////////////////////////////////////////////////////////////////////////

/**
 *  Base class to track component's children of some particular type.
 *
 *  This class is similar to VirtualBoxBaseWithChildren, with the exception
 *  that all children must be of the same type. For this reason, it's not
 *  necessary to use a map to store children, so a list is used instead.
 *
 *  As opposed to VirtualBoxBaseWithChildren, children added by
 *  #addDependentChild() are <b>strongly</b> referenced, so that they cannot
 *  be externally destructed until #removeDependentChild() is called.
 *
 *  Also, this class doesn't have the
 *  VirtualBoxBaseWithChildrenNEXT::getDependentChild() method because it would
 *  be not fast for long lists.
 *
 *  @param C    type of child objects (must inherit VirtualBoxBase AND
 *              implement some interface)
 *
 *  @deprecated Use VirtualBoxBaseWithTypedChildrenNEXT for new classes.
 */
template <class C>
class VirtualBoxBaseWithTypedChildren : public VirtualBoxBase
{
public:

    typedef std::list <ComObjPtr <C> > DependentChildren;

    VirtualBoxBaseWithTypedChildren() : mInUninit (false) {}

    virtual ~VirtualBoxBaseWithTypedChildren() {}

    /**
     *  Adds the given child to the list of dependent children.
     *  Must be called from the child's init() method,
     *  from under the child's lock.
     *
     *  @param C    the child object to add (must inherit VirtualBoxBase AND
     *              implement some interface)
     */
    void addDependentChild (C *child)
    {
        AssertReturn (child, (void) 0);

        AutoWriteLock alock (mMapLock);
        if (mInUninit)
            return;

        mDependentChildren.push_back (child);
    }

    /**
     *  Removes the given child from the list of dependent children.
     *  Must be called from the child's uninit() method,
     *  under the child's lock.
     *
     *  @param C    the child object to remove (must inherit VirtualBoxBase AND
     *              implement some interface)
     */
    void removeDependentChild (C *child)
    {
        AssertReturn (child, (void) 0);

        AutoWriteLock alock (mMapLock);
        if (mInUninit)
            return;

        mDependentChildren.remove (child);
    }

protected:

    /**
     *  Returns an internal lock handle to lock the list of children
     *  returned by #dependentChildren() using AutoReadLock/AutoWriteLock:
     *  <code>
     *      AutoReadLock alock (dependentChildrenLock());
     *  </code>
     *
     *  This is necessary for example to access the list of children returned by
     *  #dependentChildren().
     */
    RWLockHandle *dependentChildrenLock() const { return &mMapLock; }

    /**
     *  Returns the read-only list of all dependent children.
     *  @note
     *      Access the returned list (iterate, get size etc.) only after
     *      doing |AutoWriteLock alock (dependentChildrenLock());|!
     */
    const DependentChildren &dependentChildren() const { return mDependentChildren; }

    /**
     *  Uninitializes all dependent children registered with #addDependentChild().
     *
     *  @note
     *      This method will call uninit() methods of children. If these methods
     *      access the parent object, uninitDependentChildren() must be called
     *      either at the beginning of the parent uninitialization sequence (when
     *      it is still operational) or after setReady(false) is called to
     *      indicate the parent is out of action.
     */
    void uninitDependentChildren()
    {
        AutoWriteLock alock (this);
        AutoWriteLock mapLock (mMapLock);

        if (mDependentChildren.size())
        {
            // set flag to ignore #removeDependentChild() called from child->uninit()
            mInUninit = true;

            // leave the locks to let children waiting for #removeDependentChild() run
            mapLock.leave();
            alock.leave();

            for (typename DependentChildren::iterator it = mDependentChildren.begin();
                it != mDependentChildren.end(); ++ it)
            {
                C *child = (*it);
                Assert (child);
                if (child)
                    child->uninit();
            }
            mDependentChildren.clear();

            alock.enter();
            mapLock.enter();

            mInUninit = false;
        }
    }

    /**
     *  Removes (detaches) all dependent children registered with
     *  #addDependentChild(), without uninitializing them.
     *
     *  @note This method must be called from under the main object's lock
     */
    void removeDependentChildren()
    {
        AutoWriteLock alock (mMapLock);
        mDependentChildren.clear();
    }

private:

    DependentChildren mDependentChildren;

    bool mInUninit;
    mutable RWLockHandle mMapLock;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Base class to track component's children of the particular type.
 *
 * This class is similar to VirtualBoxBaseWithChildrenNEXT with the exception
 * that all children must be of the same type. For this reason, it's not
 * necessary to use a map to store children -- a list is used instead.
 *
 * Also, as opposed to VirtualBoxBaseWithChildren, children added by
 * #addDependentChild() are <b>strongly</b> referenced, so that they cannot be
 * deleted (even by a third party) until #removeDependentChild() is called on
 * them. This also means that a failure to call #removeDependentChild() and
 * #uninitDependentChildren() at appropriate times as described in
 * VirtualBoxBaseWithChildrenNEXT may cause stuck references that won't be able
 * uninitialize themselves.
 *
 * See individual method descriptions for further information.
 *
 * @param C Type of child objects (must inherit VirtualBoxBase AND implement
 *          some interface).
 *
 * @todo This is a VirtualBoxBaseWithChildren equivalent that uses the
 *       VirtualBoxBaseNEXT implementation. Will completely supersede
 *       VirtualBoxBaseWithChildren after the old VirtualBoxBase implementation
 *       has gone.
 */
template <class C>
class VirtualBoxBaseWithTypedChildrenNEXT : public VirtualBoxBaseNEXT
{
public:

    typedef std::list <ComObjPtr <C> > DependentChildren;

    VirtualBoxBaseWithTypedChildrenNEXT() {}

    virtual ~VirtualBoxBaseWithTypedChildrenNEXT() {}

    /**
     * Lock handle to use when adding/removing child objects from the list of
     * children. It is guaranteed that no any other lock is requested in methods
     * of this class while holding this lock.
     *
     * @warning By default, this simply returns the general object's lock handle
     *          (see VirtualBoxBase::lockHandle()) which is sufficient for most
     *          cases.
     */
    virtual RWLockHandle *childrenLock() { return lockHandle(); }

    /**
     * Adds the given child to the list of dependent children.
     *
     * Usually gets called from the child's init() method.
     *
     * @note @a aChild (unless it is in InInit state) must be protected by
     *       VirtualBoxBase::AutoCaller to make sure it is not uninitialized on
     *       another thread during this method's call.
     *
     * @note When #childrenLock() is not overloaded (returns the general object
     *       lock) and this method is called from under the child's read or
     *       write lock, make sure the {parent, child} locking order is
     *       preserved by locking the callee (this object) for writing before
     *       the child's lock.
     *
     * @param aChild    Child object to add.
     *
     * @note Locks #childrenLock() for writing.
     */
    void addDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild != NULL);

        AutoCaller autoCaller (this);

        /* sanity */
        AssertReturnVoid (autoCaller.state() == InInit ||
                          autoCaller.state() == Ready ||
                          autoCaller.state() == Limited);

        AutoWriteLock chLock (childrenLock());
        mDependentChildren.push_back (aChild);
    }

    /**
     * Removes the given child from the list of dependent children.
     *
     * Usually gets called from the child's uninit() method.
     *
     * Keep in mind that the called (parent) object may be no longer available
     * (i.e. may be deleted deleted) after this method returns, so you must not
     * call any other parent's methods after that!
     *
     * @note @a aChild (unless it is in InUninit state) must be protected by
     *       VirtualBoxBase::AutoCaller to make sure it is not uninitialized on
     *       another thread during this method's call.
     *
     * @note When #childrenLock() is not overloaded (returns the general object
     *       lock) and this method is called from under the child's read or
     *       write lock, make sure the {parent, child} locking order is
     *       preserved by locking the callee (this object) for writing before
     *       the child's lock. This is irrelevant when the method is called from
     *       under this object's AutoUninitSpan (i.e. in InUninit state) since
     *       in this case no locking is done.
     *
     * @param aChild    Child object to remove.
     *
     * @note Locks #childrenLock() for writing.
     */
    void removeDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild);

        AutoCaller autoCaller (this);

        /* sanity */
        AssertReturnVoid (autoCaller.state() == InUninit ||
                          autoCaller.state() == InInit ||
                          autoCaller.state() == Ready ||
                          autoCaller.state() == Limited);

        /* return shortly; we are strongly referenced so the object won't get
         * deleted if it calls init() before uninitDependentChildren() does
         * and therefore the list will still contain a valid reference that will
         * be correctly processed by uninitDependentChildren() anyway */
        if (autoCaller.state() == InUninit)
            return;

        AutoWriteLock chLock (childrenLock());
        mDependentChildren.remove (aChild);
    }

protected:

    /**
     * Returns the read-only list of all dependent children.
     *
     * @note Access the returned list (iterate, get size etc.) only after making
     *       sure #childrenLock() is locked for reading or for writing!
     */
    const DependentChildren &dependentChildren() const { return mDependentChildren; }

    /**
     * Uninitializes all dependent children registered on this object with
     * #addDependentChild().
     *
     * Must be called from within the VirtualBoxBaseProto::AutoUninitSpan (i.e.
     * typically from this object's uninit() method) to uninitialize children
     * before this object goes out of service and becomes unusable.
     *
     * Note that this method will call uninit() methods of child objects. If
     * these methods need to call the parent object during uninitialization,
     * #uninitDependentChildren() must be called before the relevant part of the
     * parent is uninitialized: usually at the beginning of the parent
     * uninitialization sequence.
     *
     * @note May lock something through the called children.
     */
    void uninitDependentChildren()
    {
        AutoCaller autoCaller (this);

        /* We don't want to hold the childrenLock() write lock here (necessary
         * to protect mDependentChildren) when uninitializing children because
         * we want to avoid a possible deadlock where we could get stuck in
         * child->uninit() blocked by AutoUninitSpan waiting for the number of
         * child's callers to drop to zero (or for another AutoUninitSpan to
         * finish), while some other thread is stuck in our
         * removeDependentChild() method called for that child and waiting for
         * the childrenLock()'s write lock.
         *
         * The only safe place to not lock and keep accessing our data members
         * is the InUninit state (no active call to our object may exist on
         * another thread when we are in InUinint, provided that all such calls
         * use the AutoCaller class of course). InUinint is also used as a flag
         * by removeDependentChild() that prevents touching mDependentChildren
         * from outside. Therefore, we assert. Note that InInit is also fine
         * since no any object may access us by that time.
         */
        AssertReturnVoid (autoCaller.state() == InUninit ||
                          autoCaller.state() == InInit);

        if (mDependentChildren.size())
        {
            for (typename DependentChildren::iterator it = mDependentChildren.begin();
                 it != mDependentChildren.end(); ++ it)
            {
                C *child = (*it);
                Assert (child);

                /* Note that if child->uninit() happens to be called on another
                 * thread right before us and is not yet finished, the second
                 * uninit() call will wait until the first one has done so
                 * (thanks to AutoUninitSpan). */
                if (child)
                    child->uninit();
            }

            /* release all strong references we hold */
            mDependentChildren.clear();
        }
    }

    /**
     * Removes (detaches) all dependent children registered with
     * #addDependentChild(), without uninitializing them.
     *
     * @note @a |this| (unless it is in InUninit state) must be protected by
     *       VirtualBoxBase::AutoCaller to make sure it is not uninitialized on
     *       another thread during this method's call.
     *
     * @note Locks #childrenLock() for writing.
     */
    void removeDependentChildren()
    {
        AutoWriteLock chLock (childrenLock());
        mDependentChildren.clear();
    }

private:

    DependentChildren mDependentChildren;
};

////////////////////////////////////////////////////////////////////////////////

/// @todo (dmik) remove after we switch to VirtualBoxBaseNEXT completely
/**
 *  Simple template that manages data structure allocation/deallocation
 *  and supports data pointer sharing (the instance that shares the pointer is
 *  not responsible for memory deallocation as opposed to the instance that
 *  owns it).
 */
template <class D>
class Shareable
{
public:

    Shareable() : mData (NULL), mIsShared (FALSE) {}
    ~Shareable() { free(); }

    void allocate() { attach (new D); }

    virtual void free() {
        if (mData) {
            if (!mIsShared)
                delete mData;
            mData = NULL;
            mIsShared = false;
        }
    }

    void attach (D *data) {
        AssertMsg (data, ("new data must not be NULL"));
        if (data && mData != data) {
            if (mData && !mIsShared)
                delete mData;
            mData = data;
            mIsShared = false;
        }
    }

    void attach (Shareable &data) {
        AssertMsg (
            data.mData == mData || !data.mIsShared,
            ("new data must not be shared")
        );
        if (this != &data && !data.mIsShared) {
            attach (data.mData);
            data.mIsShared = true;
        }
    }

    void share (D *data) {
        AssertMsg (data, ("new data must not be NULL"));
        if (mData != data) {
            if (mData && !mIsShared)
                delete mData;
            mData = data;
            mIsShared = true;
        }
    }

    void share (const Shareable &data) { share (data.mData); }

    void attachCopy (const D *data) {
        AssertMsg (data, ("data to copy must not be NULL"));
        if (data)
            attach (new D (*data));
    }

    void attachCopy (const Shareable &data) {
        attachCopy (data.mData);
    }

    virtual D *detach() {
        D *d = mData;
        mData = NULL;
        mIsShared = false;
        return d;
    }

    D *data() const {
        return mData;
    }

    D *operator->() const {
        AssertMsg (mData, ("data must not be NULL"));
        return mData;
    }

    bool isNull() const { return mData == NULL; }
    bool operator!() const { return isNull(); }

    bool isShared() const { return mIsShared; }

protected:

    D *mData;
    bool mIsShared;
};

/// @todo (dmik) remove after we switch to VirtualBoxBaseNEXT completely
/**
 *  Simple template that enhances Shareable<> and supports data
 *  backup/rollback/commit (using the copy constructor of the managed data
 *  structure).
 */
template <class D>
class Backupable : public Shareable <D>
{
public:

    Backupable() : Shareable <D> (), mBackupData (NULL) {}

    void free()
    {
        AssertMsg (this->mData || !mBackupData, ("backup must be NULL if data is NULL"));
        rollback();
        Shareable <D>::free();
    }

    D *detach()
    {
        AssertMsg (this->mData || !mBackupData, ("backup must be NULL if data is NULL"));
        rollback();
        return Shareable <D>::detach();
    }

    void share (const Backupable &data)
    {
        AssertMsg (!data.isBackedUp(), ("data to share must not be backed up"));
        if (!data.isBackedUp())
            Shareable <D>::share (data.mData);
    }

    /**
     *  Stores the current data pointer in the backup area, allocates new data
     *  using the copy constructor on current data and makes new data active.
     */
    void backup()
    {
        AssertMsg (this->mData, ("data must not be NULL"));
        if (this->mData && !mBackupData)
        {
            mBackupData = this->mData;
            this->mData = new D (*mBackupData);
        }
    }

    /**
     *  Deletes new data created by #backup() and restores previous data pointer
     *  stored in the backup area, making it active again.
     */
    void rollback()
    {
        if (this->mData && mBackupData)
        {
            delete this->mData;
            this->mData = mBackupData;
            mBackupData = NULL;
        }
    }

    /**
     *  Commits current changes by deleting backed up data and clearing up the
     *  backup area. The new data pointer created by #backup() remains active
     *  and becomes the only managed pointer.
     *
     *  This method is much faster than #commitCopy() (just a single pointer
     *  assignment operation), but makes the previous data pointer invalid
     *  (because it is freed). For this reason, this method must not be
     *  used if it's possible that data managed by this instance is shared with
     *  some other Shareable instance. See #commitCopy().
     */
    void commit()
    {
        if (this->mData && mBackupData)
        {
            if (!this->mIsShared)
                delete mBackupData;
            mBackupData = NULL;
            this->mIsShared = false;
        }
    }

    /**
     *  Commits current changes by assigning new data to the previous data
     *  pointer stored in the backup area using the assignment operator.
     *  New data is deleted, the backup area is cleared and the previous data
     *  pointer becomes active and the only managed pointer.
     *
     *  This method is slower than #commit(), but it keeps the previous data
     *  pointer valid (i.e. new data is copied to the same memory location).
     *  For that reason it's safe to use this method on instances that share
     *  managed data with other Shareable instances.
     */
    void commitCopy()
    {
        if (this->mData && mBackupData)
        {
            *mBackupData = *(this->mData);
            delete this->mData;
            this->mData = mBackupData;
            mBackupData = NULL;
        }
    }

    void assignCopy (const D *data)
    {
        AssertMsg (this->mData, ("data must not be NULL"));
        AssertMsg (data, ("data to copy must not be NULL"));
        if (this->mData && data)
        {
            if (!mBackupData)
            {
                mBackupData = this->mData;
                this->mData = new D (*data);
            }
            else
                *this->mData = *data;
        }
    }

    void assignCopy (const Backupable &data)
    {
        assignCopy (data.mData);
    }

    bool isBackedUp() const
    {
        return mBackupData != NULL;
    }

    bool hasActualChanges() const
    {
        AssertMsg (this->mData, ("data must not be NULL"));
        return this->mData != NULL && mBackupData != NULL &&
               !(*this->mData == *mBackupData);
    }

    D *backedUpData() const
    {
        return mBackupData;
    }

protected:

    D *mBackupData;
};

#endif // ____H_VIRTUALBOXBASEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
