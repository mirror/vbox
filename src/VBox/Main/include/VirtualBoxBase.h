/** @file
 *
 * VirtualBox COM base classes definition
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "VBox/com/string.h"
#include "VBox/com/Guid.h"
#include "VBox/com/ptr.h"
#include "VBox/com/ErrorInfo.h"

#include "VBox/com/VirtualBox.h"

#include <VBox/settings.h>

#include "AutoLock.h"

using namespace com;
using namespace util;

#include <iprt/cdefs.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>

#include <list>
#include <map>

#if !defined (VBOX_WITH_XPCOM)

#include <atlcom.h>

//  use a special version of the singleton class factory,
//  see KB811591 in msdn for more info.

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

#endif // !defined (VBOX_WITH_XPCOM)

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
#define ComAssertRC(vrc)    ComAssertMsgRC (vrc, ("%Vra", vrc))
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
#define ComAssertMsgRC(vrc, msg)    ComAssertMsg (VBOX_SUCCESS (vrc), msg)
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
#define ComAssertComRC(rc)  ComAssertMsg (SUCCEEDED (rc), ("COM RC = %Rhrc (0x%08X)\n", rc, rc))
#endif


/** Special version of ComAssert that returns ret if expr fails */
#define ComAssertRet(expr, ret)             \
    do { ComAssert (expr); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertMsg that returns ret if expr fails */
#define ComAssertMsgRet(expr, a, ret)       \
    do { ComAssertMsg (expr, a); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertRC that returns ret if vrc does not succeed */
#define ComAssertRCRet(vrc, ret)            \
    do { ComAssertRC (vrc); if (!VBOX_SUCCESS (vrc)) return (ret); } while (0)
/** Special version of ComAssertMsgRC that returns ret if vrc does not succeed */
#define ComAssertMsgRCRet(vrc, msg, ret)    \
    do { ComAssertMsgRC (vrc, msg); if (!VBOX_SUCCESS (vrc)) return (ret); } while (0)
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


/** Special version of ComAssert that evaulates eval and breaks if expr fails */
#define ComAssertBreak(expr, eval)                \
    if (1) { ComAssert (expr); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsg that evaulates eval and breaks if expr fails */
#define ComAssertMsgBreak(expr, a, eval)          \
    if (1)  { ComAssertMsg (expr, a); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertRC that evaulates eval and breaks if vrc does not succeed */
#define ComAssertRCBreak(vrc, eval)               \
    if (1)  { ComAssertRC (vrc); if (!VBOX_SUCCESS (vrc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsgRC that evaulates eval and breaks if vrc does not succeed */
#define ComAssertMsgRCBreak(vrc, msg, eval)       \
    if (1)  { ComAssertMsgRC (vrc, msg); if (!VBOX_SUCCESS (vrc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertFailed that evaulates eval and breaks */
#define ComAssertFailedBreak(eval)                \
    if (1)  { ComAssertFailed(); { eval; break; } } else do {} while (0)
/** Special version of ComAssertMsgFailed that evaulates eval and breaks */
#define ComAssertMsgFailedBreak(msg, eval)        \
    if (1)  { ComAssertMsgFailed (msg); { eval; break; } } else do {} while (0)
/** Special version of ComAssertComRC that evaulates eval and breaks if rc does not succeed */
#define ComAssertComRCBreak(rc, eval)             \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { eval; break; } } else do {} while (0)
/** Special version of ComAssertComRC that just breaks if rc does not succeed */
#define ComAssertComRCBreakRC(rc)                 \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { break; } } else do {} while (0)


/** Special version of ComAssert that evaulates eval and throws it if expr fails */
#define ComAssertThrow(expr, eval)                \
    if (1) { ComAssert (expr); if (!(expr)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertMsg that evaulates eval and throws it if expr fails */
#define ComAssertMsgThrow(expr, a, eval)          \
    if (1)  { ComAssertMsg (expr, a); if (!(expr)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertRC that evaulates eval and throws it if vrc does not succeed */
#define ComAssertRCThrow(vrc, eval)               \
    if (1)  { ComAssertRC (vrc); if (!VBOX_SUCCESS (vrc)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertMsgRC that evaulates eval and throws it if vrc does not succeed */
#define ComAssertMsgRCThrow(vrc, msg, eval)       \
    if (1)  { ComAssertMsgRC (vrc, msg); if (!VBOX_SUCCESS (vrc)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertFailed that evaulates eval and throws it */
#define ComAssertFailedThrow(eval)                \
    if (1)  { ComAssertFailed(); { throw (eval); } } else do {} while (0)
/** Special version of ComAssertMsgFailed that evaulates eval and throws it */
#define ComAssertMsgFailedThrow(msg, eval)        \
    if (1)  { ComAssertMsgFailed (msg); { throw (eval); } } else do {} while (0)
/** Special version of ComAssertComRC that evaulates eval and throws it if rc does not succeed */
#define ComAssertComRCThrow(rc, eval)             \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { throw (eval); } } else do {} while (0)
/** Special version of ComAssertComRC that just throws rc if rc does not succeed */
#define ComAssertComRCThrowRC(rc)                 \
    if (1)  { ComAssertComRC (rc); if (!SUCCEEDED (rc)) { throw rc; } } else do {} while (0)


/// @todo (dmik) remove after we switch to VirtualBoxBaseNEXT completely
/**
 *  Checks whether this object is ready or not. Objects are typically ready
 *  after they are successfully created by their parent objects and become
 *  not ready when the respective parent itsef becomes not ready or gets
 *  destroyed while a reference to the child is still held by the caller
 *  (which prevents it from destruction).
 *
 *  When this object is not ready, the macro sets error info and returns
 *  E_UNEXPECTED (the translatable error message is defined in null context).
 *  Otherwise, the macro does nothing.
 *
 *  This macro <b>must</b> be used at the beginning of all interface methods
 *  (right after entering the class lock) in classes derived from both
 *  VirtualBoxBase and VirtualBoxSupportErrorInfoImpl.
 */
#define CHECK_READY() \
    do { \
        if (!isReady()) \
            return setError (E_UNEXPECTED, tr ("The object is not ready")); \
    } while (0)

/**
 *  Declares an empty construtor and destructor for the given class.
 *  This is useful to prevent the compiler from generating the default
 *  ctor and dtor, which in turn allows to use forward class statements
 *  (instead of including their header files) when declaring data members of
 *  non-fundamental types with constructors (which are always called implicitly
 *  by constructors and by the destructor of the class).
 *
 *  This macro is to be palced within (the public section of) the class
 *  declaration. Its counterpart, DEFINE_EMPTY_CTOR_DTOR, must be placed
 *  somewhere in one of the translation units (usually .cpp source files).
 *
 *  @param      cls     class to declare a ctor and dtor for
 */
#define DECLARE_EMPTY_CTOR_DTOR(cls) cls(); ~cls();

/**
 *  Defines an empty construtor and destructor for the given class.
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

class ATL_NO_VTABLE VirtualBoxBaseNEXT_base
#if !defined (VBOX_WITH_XPCOM)
    : public CComObjectRootEx <CComMultiThreadModel>
#else
    : public CComObjectRootEx
#endif
    , public Lockable
{
public:

    enum State { NotReady, Ready, InInit, InUninit, InitFailed, Limited };

protected:

    VirtualBoxBaseNEXT_base();
    virtual ~VirtualBoxBaseNEXT_base();

public:

    // util::Lockable interface
    virtual RWLockHandle *lockHandle() const;

    /**
     *  Virtual unintialization method.
     *  Must be called by all implementations (COM classes) when the last
     *  reference to the object is released, before calling the destructor.
     *  Also, this method is called automatically by the uninit() method of the
     *  parent of this object, when this object is a dependent child of a class
     *  derived from VirtualBoxBaseWithChildren (@sa
     *  VirtualBoxBaseWithChildren::addDependentChild).
     */
    virtual void uninit() {}

    virtual HRESULT addCaller (State *aState = NULL, bool aLimited = false);
    virtual void releaseCaller();

    /**
     *  Adds a limited caller. This method is equivalent to doing
     *  <tt>addCaller (aState, true)</tt>, but it is preferred because
     *  provides better self-descriptiveness. See #addCaller() for more info.
     */
    HRESULT addLimitedCaller (State *aState = NULL)
    {
        return addCaller (aState, true /* aLimited */);
    }

    /**
     *  Smart class that automatically increases the number of callers of the
     *  given VirtualBoxBase object when an instance is constructed and decreases
     *  it back when the created instance goes out of scope (i.e. gets destroyed).
     *
     *  If #rc() returns a failure after the instance creation, it means that
     *  the managed VirtualBoxBase object is not Ready, or in any other invalid
     *  state, so that the caller must not use the object and can return this
     *  failed result code to the upper level.
     *
     *  See VirtualBoxBase::addCaller(), VirtualBoxBase::addLimitedCaller() and
     *  VirtualBoxBase::releaseCaller() for more details about object callers.
     *
     *  @param aLimited |false| if this template should use
     *                  VirtualiBoxBase::addCaller() calls to add callers, or
     *                  |true| if VirtualiBoxBase::addLimitedCaller() should be
     *                  used.
     *
     *  @note It is preferrable to use the AutoCaller and AutoLimitedCaller
     *        classes than specify the @a aLimited argument, for better
     *        self-descriptiveness.
     */
    template <bool aLimited>
    class AutoCallerBase
    {
    public:

        /**
         *  Increases the number of callers of the given object
         *  by calling VirtualBoxBase::addCaller().
         *
         *  @param aObj     Object to add a caller to. If NULL, this
         *                  instance is effectively turned to no-op (where
         *                  rc() will return S_OK and state() will be
         *                  NotReady).
         */
        AutoCallerBase (VirtualBoxBaseNEXT_base *aObj)
            : mObj (aObj)
            , mRC (S_OK)
            , mState (NotReady)
        {
            if (mObj)
                mRC =  mObj->addCaller (&mState, aLimited);
        }

        /**
         *  If the number of callers was successfully increased,
         *  decreases it using VirtualBoxBase::releaseCaller(), otherwise
         *  does nothing.
         */
        ~AutoCallerBase()
        {
            if (mObj && SUCCEEDED (mRC))
                mObj->releaseCaller();
        }

        /**
         *  Stores the result code returned by VirtualBoxBase::addCaller()
         *  after instance creation or after the last #add() call. A successful
         *  result code means the number of callers was successfully increased.
         */
        HRESULT rc() const { return mRC; }

        /**
         *  Returns |true| if |SUCCEEDED (rc())| is |true|, for convenience.
         *  |true| means the number of callers was successfully increased.
         */
        bool isOk() const { return SUCCEEDED (mRC); }

        /**
         *  Stores the object state returned by VirtualBoxBase::addCaller()
         *  after instance creation or after the last #add() call.
         */
        State state() const { return mState; }

        /**
         *  Temporarily decreases the number of callers of the managed object.
         *  May only be called if #isOk() returns |true|. Note that #rc() will
         *  return E_FAIL after this method succeeds.
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
         *  Restores the number of callers decreased by #release(). May only
         *  be called after #release().
         */
        void add()
        {
            Assert (!SUCCEEDED (mRC));
            if (mObj && !SUCCEEDED (mRC))
                mRC = mObj->addCaller (&mState, aLimited);
        }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoCallerBase)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoCallerBase)

        VirtualBoxBaseNEXT_base *mObj;
        HRESULT mRC;
        State mState;
    };

    /**
     *  Smart class that automatically increases the number of normal
     *  (non-limited) callers of the given VirtualBoxBase object when an
     *  instance is constructed and decreases it back when the created instance
     *  goes out of scope (i.e. gets destroyed).
     *
     *  A typical usage pattern to declare a normal method of some object
     *  (i.e. a method that is valid only when the object provides its
     *  full functionality) is:
     *  <code>
     *  STDMETHODIMP Component::Foo()
     *  {
     *      AutoCaller autoCaller (this);
     *      CheckComRCReturnRC (autoCaller.rc());
     *      ...
     *  </code>
     *
     *  Using this class is equivalent to using the AutoCallerBase template
     *  with the @a aLimited argument set to |false|, but this class is
     *  preferred because provides better self-descriptiveness.
     *
     *  See AutoCallerBase for more information about auto caller functionality.
     */
    typedef AutoCallerBase <false> AutoCaller;

    /**
     *  Smart class that automatically increases the number of limited callers
     *  of the given VirtualBoxBase object when an instance is constructed and
     *  decreases it back when the created instance goes out of scope (i.e.
     *  gets destroyed).
     *
     *  A typical usage pattern to declare a limited method of some object
     *  (i.e. a method that is valid even if the object doesn't provide its
     *  full functionality) is:
     *  <code>
     *  STDMETHODIMP Component::Bar()
     *  {
     *      AutoLimitedCaller autoCaller (this);
     *      CheckComRCReturnRC (autoCaller.rc());
     *      ...
     *  </code>
     *
     *  Using this class is equivalent to using the AutoCallerBase template
     *  with the @a aLimited argument set to |true|, but this class is
     *  preferred because provides better self-descriptiveness.
     *
     *  See AutoCallerBase for more information about auto caller functionality.
     */
    typedef AutoCallerBase <true> AutoLimitedCaller;

protected:

    /**
     *  Smart class to enclose the state transition NotReady->InInit->Ready.
     *
     *  Instances must be created at the beginning of init() methods of
     *  VirtualBoxBase subclasses as a stack-based variable using |this| pointer
     *  as the argument. When this variable is created it automatically places
     *  the object to the InInit state.
     *
     *  When the created variable goes out of scope (i.e. gets destroyed),
     *  depending on the success status of this initialization span, it either
     *  places the object to the Ready state or calls the object's
     *  VirtualBoxBase::uninit() method which is supposed to place the object
     *  back to the NotReady state using the AutoUninitSpan class.
     *
     *  The initial success status of the initialization span is determined by
     *  the @a aSuccess argument of the AutoInitSpan constructor (|false| by
     *  default). Inside the initialization span, the success status can be set
     *  to |true| using #setSucceeded() or to |false| using #setFailed(). Please
     *  don't forget to set the correct success status before letting the
     *  AutoInitSpan variable go out of scope (for example, by performing an
     *  early return from the init() method)!
     *
     *  Note that if an instance of this class gets constructed when the
     *  object is in the state other than NotReady, #isOk() returns |false| and
     *  methods of this class do nothing: the state transition is not performed.
     *
     *  A typical usage pattern is:
     *  <code>
     *  HRESULT Component::init()
     *  {
     *      AutoInitSpan autoInitSpan (this);
     *      AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);
     *      ...
     *      if (FAILED (rc))
     *          return rc;
     *      ...
     *      if (SUCCEEDED (rc))
     *          autoInitSpan.setSucceeded();
     *      return rc;
     *  }
     *  </code>
     *
     *  @note Never create instances of this class outside init() methods of
     *  VirtualBoxBase subclasses and never pass anything other than |this| as
     *  the argument to the constructor!
     */
    class AutoInitSpan
    {
    public:

        enum Status { Failed = 0x0, Succeeded = 0x1, Limited = 0x2 };

        AutoInitSpan (VirtualBoxBaseNEXT_base *aObj, Status aStatus = Failed);
        ~AutoInitSpan();

        /**
         *  Returns |true| if this instance has been created at the right moment
         *  (when the object was in the NotReady state) and |false| otherwise.
         */
        bool isOk() const { return mOk; }

        /**
         *  Sets the initialization status to Succeeded to indicates successful
         *  initialization. The AutoInitSpan destructor will place the managed
         *  VirtualBoxBase object to the Ready state.
         */
        void setSucceeded() { mStatus = Succeeded; }

        /**
         *  Sets the initialization status to Succeeded to indicate limited
         *  (partly successful) initialization. The AutoInitSpan destructor will
         *  place the managed VirtualBoxBase object to the Limited state.
         */
        void setLimited() { mStatus = Limited; }

        /**
         *  Sets the initialization status to Failure to indicates failed
         *  initialization. The AutoInitSpan destructor will place the managed
         *  VirtualBoxBase object to the InitFailed state and will automatically
         *  call its uninit() method which is supposed to place the object back
         *  to the NotReady state using AutoUninitSpan.
         */
        void setFailed() { mStatus = Failed; }

        /** Returns the current initialization status. */
        Status status() { return mStatus; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoInitSpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoInitSpan)

        VirtualBoxBaseNEXT_base *mObj;
        Status mStatus : 3; // must be at least total number of bits + 1 (sign)
        bool mOk : 1;
    };

    /**
     *  Smart class to enclose the state transition Limited->InInit->Ready.
     *
     *  Instances must be created at the beginning of methods of VirtualBoxBase
     *  subclasses that try to re-initialize the object to bring it to the
     *  Ready state (full functionality) after partial initialization
     *  (limited functionality)>, as a stack-based variable using |this| pointer
     *  as the argument. When this variable is created it automatically places
     *  the object to the InInit state.
     *
     *  When the created variable goes out of scope (i.e. gets destroyed),
     *  depending on the success status of this initialization span, it either
     *  places the object to the Ready state or brings it back to the Limited
     *  state.
     *
     *  The initial success status of the re-initialization span is |false|.
     *  In order to make it successful, #setSucceeded() must be called before
     *  the instance is destroyed.
     *
     *  Note that if an instance of this class gets constructed when the
     *  object is in the state other than Limited, #isOk() returns |false| and
     *  methods of this class do nothing: the state transition is not performed.
     *
     *  A typical usage pattern is:
     *  <code>
     *  HRESULT Component::reinit()
     *  {
     *      AutoReadySpan autoReadySpan (this);
     *      AssertReturn (autoReadySpan.isOk(), E_UNEXPECTED);
     *      ...
     *      if (FAILED (rc))
     *          return rc;
     *      ...
     *      if (SUCCEEDED (rc))
     *          autoReadySpan.setSucceeded();
     *      return rc;
     *  }
     *  </code>
     *
     *  @note Never create instances of this class outside re-initialization
     *  methods of VirtualBoxBase subclasses and never pass anything other than
     *  |this| as the argument to the constructor!
     */
    class AutoReadySpan
    {
    public:

        AutoReadySpan (VirtualBoxBaseNEXT_base *aObj);
        ~AutoReadySpan();

        /**
         *  Returns |true| if this instance has been created at the right moment
         *  (when the object was in the Limited state) and |false| otherwise.
         */
        bool isOk() const { return mOk; }

        /**
         *  Sets the re-initialization status to Succeeded to indicates
         *  successful re-initialization. The AutoReadySpan destructor will
         *  place the managed VirtualBoxBase object to the Ready state.
         */
        void setSucceeded() { mSucceeded = true; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoReadySpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoReadySpan)

        VirtualBoxBaseNEXT_base *mObj;
        bool mSucceeded : 1;
        bool mOk : 1;
    };

    /**
     *  Smart class to enclose the state transition Ready->InUnnit->NotReady or
     *  InitFailed->InUnnit->NotReady.
     *
     *  Must be created at the beginning of uninit() methods of VirtualBoxBase
     *  subclasses as a stack-based variable using |this| pointer as the argument.
     *  When this variable is created it automatically places the object to the
     *  InUninit state, unless it is already in the NotReady state as indicated
     *  by #uninitDone() returning |true|. In the latter case, the uninit()
     *  method must immediately return because there should be nothing to
     *  uninitialize.
     *
     *  When this variable goes out of scope (i.e. gets destroyed), it places
     *  the object to the NotReady state.
     *
     *  A typical usage pattern is:
     *  <code>
     *  void Component::uninit()
     *  {
     *      AutoUninitSpan autoUninitSpan (this);
     *      if (autoUninitSpan.uninitDone())
     *          retrun;
     *      ...
     *  </code>
     *
     *  @note Never create instances of this class outside uninit() methods and
     *  never pass anything other than |this| as the argument to the constructor!
     */
    class AutoUninitSpan
    {
    public:

        AutoUninitSpan (VirtualBoxBaseNEXT_base *aObj);
        ~AutoUninitSpan();

        /** |true| when uninit() is called as a result of init() failure */
        bool initFailed() { return mInitFailed; }

        /** |true| when uninit() has already been called (so the object is NotReady) */
        bool uninitDone() { return mUninitDone; }

    private:

        DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoUninitSpan)
        DECLARE_CLS_NEW_DELETE_NOOP (AutoUninitSpan)

        VirtualBoxBaseNEXT_base *mObj;
        bool mInitFailed : 1;
        bool mUninitDone : 1;
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
    /** Semaphore posted when the number of callers drops to zero */
    RTSEMEVENT mZeroCallersSem;
    /** Semaphore posted when the object goes from InInit some other state */
    RTSEMEVENTMULTI mInitDoneSem;
    /** Number of threads waiting for mInitDoneSem */
    unsigned mInitDoneSemUsers;

    /** Protects access to state related data members */
    WriteLockHandle mStateLock;

    /** User-level object lock for subclasses */
    mutable RWLockHandle *mObjectLock;
};

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
    virtual HRESULT addCaller (VirtualBoxBaseNEXT_base::State *aState = NULL, \
                               bool aLimited = false) \
    { \
        VirtualBoxBaseNEXT_base::State state; \
        HRESULT rc = VirtualBoxBaseNEXT_base::addCaller (&state, aLimited); \
        if (FAILED (rc)) \
        { \
            if (state == VirtualBoxBaseNEXT_base::Limited) \
                rc = setError (rc, tr ("The object functonality is limited")); \
            else \
                rc = setError (rc, tr ("The object is not ready")); \
        } \
        if (aState) \
            *aState = state; \
        return rc; \
    } \

////////////////////////////////////////////////////////////////////////////////

/// @todo (dmik) remove after we switch to VirtualBoxBaseNEXT completely
class ATL_NO_VTABLE VirtualBoxBase : public VirtualBoxBaseNEXT_base
//#if !defined (VBOX_WITH_XPCOM)
//    : public CComObjectRootEx<CComMultiThreadModel>
//#else
//    : public CComObjectRootEx
//#endif
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
     *  a class dervived from VirtualBoxBaseWithChildren (@sa
     *  VirtualBoxBaseWithChildren::addDependentChild). In this case, this
     *  method's impelemtation must call setReady (false),
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
 *  the new locking scheme (VirtualBoxBaseNEXT_base).
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

/** Helper for VirtualBoxSupportTranslation */
class VirtualBoxSupportTranslationBase
{
protected:
    static bool cutClassNameFrom__PRETTY_FUNCTION__ (char *prettyFunctionName);
};

/**
 *  This template implements the NLS string translation support for the
 *  given class by providing a #tr() function.
 *
 *  @param C    class that needs to support the string translation
 *
 *  @note
 *      Every class that wants to use the #tr() function in its own methods must
 *      inherit from this template, regardless of whether its base class (if any)
 *      inherits from it or not! Otherwise, the translation service will not
 *      work correctly. However, the declaration of the resulting class must
 *      contain the VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(<ClassName>) macro
 *      if one of its base classes also inherits from this template (to resolve
 *      the ambiguity of the #tr() function).
 */
template <class C>
class VirtualBoxSupportTranslation : virtual protected VirtualBoxSupportTranslationBase
{
public:

    /**
     *  Translates the given text string according to the currently installed
     *  translation table and current context, which is determined by the
     *  class name. See VirtualBoxBase::translate() for more info.
     *
     *  @param sourceText   the string to translate
     *  @param comment      the comment to the string (NULL means no comment)
     *
     *  @return
     *      the translated version of the source string in UTF-8 encoding,
     *      or the source string itself if the translation is not found in
     *      the current context.
     */
    inline static const char *tr (const char *sourceText, const char *comment = 0)
    {
        return VirtualBoxBase::translate (getClassName(), sourceText, comment);
    }

protected:

    static const char *getClassName()
    {
        static char fn [sizeof (__PRETTY_FUNCTION__) + 1];
        if (!className)
        {
            strcpy (fn, __PRETTY_FUNCTION__);
            cutClassNameFrom__PRETTY_FUNCTION__ (fn);
            className = fn;
        }
        return className;
    }

private:

    static const char *className;
};

template <class C>
const char *VirtualBoxSupportTranslation <C>::className = NULL;

/**
 *  This macro must be invoked inside the public section of the declaration of
 *  the class inherited from the VirtualBoxSupportTranslation template, in case
 *  when one of its other base classes also inherits from that template. This is
 *  necessary to resolve the ambiguity of the #tr() function.
 *
 *  @param C    class that inherits from the VirtualBoxSupportTranslation template
 *              more than once (through its other base clases)
 */
#define VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(C) \
    inline static const char *tr (const char *sourceText, const char *comment = 0) \
    { \
        return VirtualBoxSupportTranslation <C>::tr (sourceText, comment); \
    }

/**
 *  A dummy macro that is used to shut down Qt's lupdate tool warnings
 *  in some situations. This macro needs to be present inside (better at the
 *  very beginning) of the declaration of the class that inherits from
 *  VirtualBoxSupportTranslation template, to make lupdate happy.
 */
#define Q_OBJECT

////////////////////////////////////////////////////////////////////////////////

/**
 *  Helper for the VirtualBoxSupportErrorInfoImpl template.
 */
class VirtualBoxSupportErrorInfoImplBase
{
    static HRESULT setErrorInternal (HRESULT aResultCode, const GUID &aIID,
                                     const Bstr &aComponent, const Bstr &aText,
                                     bool aWarning);

protected:

    /**
     * The MultiResult class is a com::LWResult enhancement that also acts as a
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
     * calling party, the preceeding error is what the
     * IVirtualBoxErrorInfo::next attribute of the first error points to, and so
     * on, upto the first error or warning occured which is the last in the
     * chain. See IVirtualBoxErrorInfo documentation for more info.
     *
     * When the instance of the MultiResult class goes out of scope and gets
     * destroyed, it automatically decreases the turn-on counter by one. If
     * the counter drops to zero, multi-error mode for the current thread is
     * turned off and the thread switches back to single-error mode where every
     * next error or warning object overwrites the previous one.
     *
     * Note that the caller of a COM methid uses a non-S_OK result code to
     * decide if the method has returned an error (negative codes) or a warning
     * (positive non-zero codes) and will query extended error info only in
     * these two cases. However, since multi-error mode implies that the method
     * doesn't return control return to the caller immediately after the first
     * error or warning but continues its execution, the functionality provided
     * by the base com::LWResult class becomes very useful because it allows to
     * preseve the error or the warning result code even if it is later assigned
     * a S_OK value multiple times. See com::LWResult for details.
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
     *       that as it is breaks the class semantics (and will assert);
     */
    class MultiResult : public com::LWResult
    {
    public:

        /**
         * @see com::LWResult::LWResult().
         */
        MultiResult (HRESULT aRC = E_FAIL) : LWResult (aRC) { init(); }

        MultiResult (const MultiResult &aThat) : LWResult (aThat)
        {
            /* We need this copy constructor only for GCC that wants to have
             * it in case of expressions like |MultiResult rc = E_FAIL;|. But
             * we assert since the optimizer should actually avoid the
             * temporary and call the other constructor directly istead. */
            AssertFailed();
            init();
        }

        ~MultiResult();

        MultiResult &operator= (HRESULT aRC)
        {
            com::LWResult::operator= (aRC);
            return *this;
        }

        MultiResult &operator= (const MultiResult &aThat)
        {
            /* We need this copy constructor only for GCC that wants to have
             * it in case of expressions like |MultiResult rc = E_FAIL;|. But
             * we assert since the optimizer should actually avoid the
             * temporary and call the other constructor directly istead. */
            AssertFailed();
            com::LWResult::operator= (aThat);
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
                                    const Bstr &aText)
    {
        return setErrorInternal (aResultCode, aIID, aComponent, aText,
                                 false /* aWarning */);
    }

    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const Bstr &aComponent,
                               const Bstr &aText)
    {
        return setErrorInternal (aResultCode, aIID, aComponent, aText,
                                 true /* aWarning */);
    }

    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const Bstr &aComponent,
                             const char *aText, va_list aArgs)
    {
        return setErrorInternal (aResultCode, aIID, aComponent,
                                 Utf8StrFmtVA (aText, aArgs),
                                 false /* aWarning */);
    }

    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const Bstr &aComponent,
                               const char *aText, va_list aArgs)
    {
        return setErrorInternal (aResultCode, aIID, aComponent,
                                 Utf8StrFmtVA (aText, aArgs),
                                 true /* aWarning */);
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
     *  @param  aIID        IID of the intrface that defines the error
     *  @param  aComponent  name of the component that generates the error
     *  @param  aText       error message (must not be null), an RTStrPrintf-like
     *                      format string in UTF-8 encoding
     *  @param  ...         list of arguments for the format string
     *
     *  @return
     *      the error argument, for convenience, If an error occures while
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
            (aResultCode, aIID, aComponent, aText, args);
        va_end (args);
        return rc;
    }

    /**
     *  This method is the same as #setError() except that it makes sure @a
     *  aResultCode doesn't have the error severty bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereofe you can
     *  use ordinary E_XXX result code constancs, for convenience. However, this
     *  behavior may be non-stanrard on some COM platforms.
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
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, args);
        va_end (args);
        return rc;
    }

    /**
     *  This method is the same as #setError() except that it makes sure @a
     *  aResultCode doesn't have the error severty bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereofe you can
     *  use ordinary E_XXX result code constancs, for convenience. However, this
     *  behavior may be non-stanrard on some COM platforms.
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
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText, aArgs);
        return rc;
    }

    /**
     *  This method is the same as #setErrorV() except that it makes sure @a
     *  aResultCode doesn't have the error severty bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereofe you can
     *  use ordinary E_XXX result code constancs, for convenience. However, this
     *  behavior may be non-stanrard on some COM platforms.
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
     *  This method is preferred iy you have a ready (translated and formatted)
     *  Bstr string, because it omits an extra conversion Utf8Str -> Bstr.
     *
     *  See #setError (HRESULT, const GUID &, const wchar_t *, const char *text, ...)
     *  and #setError (HRESULT, const char *, ...)  for details.
     */
    static HRESULT setErrorBstr (HRESULT aResultCode, const Bstr &aText)
    {
        HRESULT rc = VirtualBoxSupportErrorInfoImplBase::setError
            (aResultCode, COM_IIDOF(I), C::getComponentName(), aText);
        return rc;
    }

    /**
     *  This method is the same as #setErrorBstr() except that it makes sure @a
     *  aResultCode doesn't have the error severty bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereofe you can
     *  use ordinary E_XXX result code constancs, for convenience. However, this
     *  behavior may be non-stanrard on some COM platforms.
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
            (aResultCode, aIID, C::getComponentName(), aText, args);
        va_end (args);
        return rc;
    }

    /**
     *  This method is the same as #setError() except that it makes sure @a
     *  aResultCode doesn't have the error severty bit (31) set when passed
     *  down to the created IVirtualBoxErrorInfo object.
     *
     *  The error severity bit is always cleared by this call, thereofe you can
     *  use ordinary E_XXX result code constancs, for convenience. However, this
     *  behavior may be non-stanrard on some COM platforms.
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

private:

};

////////////////////////////////////////////////////////////////////////////////

/**
 *  Base class to track VirtualBoxBase chlidren of the component.
 *
 *  This class is a preferrable VirtualBoxBase replacement for components
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
        Assert (!child->isWriteLockOnCurrentThread());
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
 *
 * Base class to track VirtualBoxBaseNEXT chlidren of the component.
 *
 * This class is a preferrable VirtualBoxBase replacement for components that
 * operate with collections of child components. It gives two useful
 * possibilities:
 *
 * <ol><li>
 *      Given an IUnknown instance, it's possible to quickly determine
 *      whether this instance represents a child object created by the given
 *      component, and if so, get a valid VirtualBoxBase pointer to the child
 *      object. The returned pointer can be then safely casted to the
 *      actual class of the child object (to get access to its "internal"
 *      non-interface methods) provided that no other child components implement
 *      the same initial interface IUnknown is queried from.
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
 *      When a child object it is uninitialized, it calls
 *      #removeDependentChild() to unregister itself. Since the child's
 *      uninitialization may originate both from this method and from the child
 *      itself calling its uninit() on another thread at the same time, please
 *      make sure that #removeDependentChild() is called:
 *      <ul><li>
 *          after the child has successfully entered AutoUninitSpan -- to make
 *          sure this method is called only once for the given child object
 *          transitioning from Ready to NotReady. A failure to do so will at
 *          least likely cause an assertion ("Failed to remove the child from
 *          the map").
 *      </li><li>
 *          outside the child object's lock -- to avoid guaranteed deadlocks
 *          caused by different lock order: (child_lock, map_lock) in uninit()
 *          and (map_lock, child_lock) in this method.
 *      </li></ul>
 * </li></ol>
 *
 * Note that children added by #addDependentChild() are <b>weakly</b> referenced
 * (i.e. AddRef() is not called), so when a child object is deleted externally
 * (because it's reference count goes to zero), it will automatically remove
 * itself from the map of dependent children provided that it follows the rules
 * described here.
 *
 * @note Once again: because of weak referencing, deadlocks and assertions are
 *       very likely if #addDependentChild() or #removeDependentChild() are used
 *       incorrectly (called at inappropriate times). Check the above rules once
 *       more.
 *
 * @todo This is a VirtualBoxBaseWithChildren equivalent that uses the
 *       VirtualBoxBaseNEXT implementation. Will completely supercede
 *       VirtualBoxBaseWithChildren after the old VirtualBoxBase implementation
 *       has gone.
 */
class VirtualBoxBaseWithChildrenNEXT : public VirtualBoxBaseNEXT
{
public:

    VirtualBoxBaseWithChildrenNEXT()
        : mUninitDoneSem (NIL_RTSEMEVENT), mChildrenLeft (0)
    {}

    virtual ~VirtualBoxBaseWithChildrenNEXT()
    {}

    /**
     * Adds the given child to the map of dependent children.
     *
     * Typically called from the child's init() method, from within the
     * AutoInitSpan scope.  Otherwise, VirtualBoxBase::AutoCaller must be
     * used on @a aChild to make sure it is not uninitialized during this
     * method's call.
     *
     * @param aChild    Child object to add (must inherit VirtualBoxBase AND
     *                  implement some interface).
     */
    template <class C>
    void addDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild);
        doAddDependentChild (ComPtr <IUnknown> (aChild), aChild);
    }

    /**
     * Removes the given child from the map of dependent children.
     *
     * Make sure this method is called after the child has successfully entered
     * AutoUninitSpan and outside the child lock.
     *
     * If called not from within the AutoUninitSpan scope,
     * VirtualBoxBase::AutoCaller must be used on @a aChild to make sure it is
     * not uninitialized during this method's call.
     *
     * @param aChild    Child object to remove (must inherit VirtualBoxBase AND
     *                  implement some interface).
     */
    template <class C>
    void removeDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild);
        Assert (!aChild->isWriteLockOnCurrentThread());
        doRemoveDependentChild (ComPtr <IUnknown> (aChild));
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

    RTSEMEVENT mUninitDoneSem;
    size_t mChildrenLeft;

    /* Protects all the fields above */
    RWLockHandle mMapLock;
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
 *  For this reason, strict rules of calling #removeDependentChild() don't
 *  apply to instances of this class -- it can be called anywhere in the
 *  child's uninit() implementation.
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
     *  returned by #dependentChildren() using AutoWriteLock:
     *  <code>
     *      AutoWriteLock alock (dependentChildrenLock());
     *  </code>
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
 * Base class to track component's chlidren of the particular type.
 *
 * This class is similar to VirtualBoxBaseWithChildren, with the exception that
 * all children must be of the same type. For this reason, it's not necessary to
 * use a map to store children, so a list is used instead.
 *
 * As opposed to VirtualBoxBaseWithChildren, children added by
 * #addDependentChild() are <b>strongly</b> referenced, so that they cannot be
 * externally deleted until #removeDependentChild() is called. For this
 * reason, strict rules of calling #removeDependentChild() don't apply to
 * instances of this class -- it can be called anywhere in the child's uninit()
 * implementation.
 *
 * @param C Type of child objects (must inherit VirtualBoxBase AND implementsome
 *          interface).
 *
 * @todo This is a VirtualBoxBaseWithChildren equivalent that uses the
 *       VirtualBoxBaseNEXT implementation. Will completely supercede
 *       VirtualBoxBaseWithChildren after the old VirtualBoxBase implementation
 *       has gone.
 */
template <class C>
class VirtualBoxBaseWithTypedChildrenNEXT : public VirtualBoxBaseNEXT
{
public:

    typedef std::list <ComObjPtr <C> > DependentChildren;

    VirtualBoxBaseWithTypedChildrenNEXT() : mInUninit (false) {}

    virtual ~VirtualBoxBaseWithTypedChildrenNEXT() {}

    /**
     * Adds the given child to the list of dependent children.
     *
     * VirtualBoxBase::AutoCaller must be used on @a aChild to make sure it is
     * not uninitialized during this method's call.
     *
     *  @param aChild   Child object to add (must inherit VirtualBoxBase AND
     *                  implement some interface).
     */
    void addDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild);

        AutoWriteLock alock (mMapLock);
        if (mInUninit)
            return;

        mDependentChildren.push_back (aChild);
    }

    /**
     * Removes the given child from the list of dependent children.
     *
     * VirtualBoxBase::AutoCaller must be used on @a aChild to make sure it is
     * not uninitialized during this method's call.
     *
     *  @param aChild   the child object to remove (must inherit VirtualBoxBase
     *                  AND implement some interface).
     */
    void removeDependentChild (C *aChild)
    {
        AssertReturnVoid (aChild);

        AutoWriteLock alock (mMapLock);
        if (mInUninit)
            return;

        mDependentChildren.remove (aChild);
    }

protected:

    /**
     * Returns an internal lock handle used to lock the list of children
     * returned by #dependentChildren(). This lock is to be used by
     * AutoWriteLock as follows:
     * <code>
     *      AutoWriteLock alock (dependentChildrenLock());
     * </code>
     */
    RWLockHandle *dependentChildrenLock() const { return &mMapLock; }

    /**
     * Returns the read-only list of all dependent children.
     *
     * @note Access the returned list (iterate, get size etc.) only after doing
     *       AutoWriteLock alock (dependentChildrenLock())!
     */
    const DependentChildren &dependentChildren() const { return mDependentChildren; }

    void uninitDependentChildren();

    /**
     * Removes (detaches) all dependent children registered with
     * #addDependentChild(), without uninitializing them.
     *
     * @note This method must be called from under the main object's lock.
     */
    void removeDependentChildren()
    {
        /// @todo why?..
        AssertReturnVoid (isWriteLockOnCurrentThread());

        AutoWriteLock alock (mMapLock);
        mDependentChildren.clear();
    }

private:

    DependentChildren mDependentChildren;

    bool mInUninit;

    /* Protects the two fields above */
    mutable RWLockHandle mMapLock;
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

#if defined VBOX_MAIN_SETTINGS_ADDONS

/**
 * Settinsg API additions.
 */
namespace settings
{

/// @todo once string data in Bstr and Utf8Str is auto_ref_ptr, enable the
/// code below

#if 0

/** Specialization of FromString for Bstr. */
template<> com::Bstr FromString <com::Bstr> (const char *aValue);

#endif

/** Specialization of ToString for Bstr. */
template<> stdx::char_auto_ptr
ToString <com::Bstr> (const com::Bstr &aValue, unsigned int aExtra);

/** Specialization of FromString for Guid. */
template<> com::Guid FromString <com::Guid> (const char *aValue);

/** Specialization of ToString for Guid. */
template<> stdx::char_auto_ptr
ToString <com::Guid> (const com::Guid &aValue, unsigned int aExtra);

} /* namespace settings */

#endif /* VBOX_MAIN_SETTINGS_ADDONS */

#endif // ____H_VIRTUALBOXBASEIMPL
