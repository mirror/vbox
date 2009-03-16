/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * SupportErrorInfo* class family declarations
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_com_SupportErrorInfo_h
#define ___VBox_com_SupportErrorInfo_h

#include "VBox/com/defs.h"
#include "VBox/com/string.h"

#include <iprt/cdefs.h>

#include <stdarg.h>

#if !defined (VBOX_WITH_XPCOM)
interface IVirtualBoxErrorInfo;
#else
class IVirtualBoxErrorInfo;
#endif

namespace com
{

/**
 * The MultiResult class is a com::FWResult enhancement that also acts as a
 * switch to turn on multi-error mode for SupportErrorInfo::setError() and
 * SupportErrorInfo::setWarning() calls.
 *
 * When an instance of this class is created, multi-error mode is turned on
 * for the current thread and the turn-on counter is increased by one. In
 * multi-error mode, a call to setError() or setWarning() does not
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
 * @note This class is intended to be instantiated on the stack, therefore
 *       You cannot create them using new(). Although it is possible to copy
 *       instances of MultiResult or return them by value, please never do
 *       that as it is breaks the class semantics (and will assert);
 */
class MultiResult : public FWResult
{
public:

    /**
     * @copydoc FWResult::FWResult().
     */
    MultiResult (HRESULT aRC = E_FAIL) : FWResult (aRC) { incCounter(); }

    MultiResult (const MultiResult &aThat) : FWResult (aThat)
    {
        /* We need this copy constructor only for GCC that wants to have
         * it in case of expressions like |MultiResult rc = E_FAIL;|. But
         * we assert since the optimizer should actually avoid the
         * temporary and call the other constructor directly instead. */
        AssertFailed();
    }

    ~MultiResult() { decCounter(); }

    MultiResult &operator= (HRESULT aRC)
    {
        FWResult::operator= (aRC);
        return *this;
    }

    MultiResult &operator= (const MultiResult & /* aThat */)
    {
        /* We need this copy constructor only for GCC that wants to have
         * it in case of expressions like |MultiResult rc = E_FAIL;|. But
         * we assert since the optimizer should actually avoid the
         * temporary and call the other constructor directly instead. */
        AssertFailed();
        return *this;
    }

private:

    DECLARE_CLS_NEW_DELETE_NOOP (MultiResult)

    static void incCounter();
    static void decCounter();

    static RTTLS sCounter;

    friend class SupportErrorInfoBase;
    friend class MultiResultRef;
};

/**
 * The MultiResultRef class is equivalent to MultiResult except that it takes
 * a reference to the existing HRESULT variable instead of maintaining its own
 * one.
 */
class MultiResultRef
{
public:

    MultiResultRef (HRESULT &aRC) : mRC (aRC) { MultiResult::incCounter(); }

    ~MultiResultRef() { MultiResult::decCounter(); }

    MultiResultRef &operator= (HRESULT aRC)
    {
        /* Copied from FWResult */
        if ((FAILED (aRC) && !FAILED (mRC)) ||
            (mRC == S_OK && aRC != S_OK))
            mRC = aRC;

        return *this;
    }

    operator HRESULT() const { return mRC; }

    HRESULT *operator&() { return &mRC; }

private:

    DECLARE_CLS_NEW_DELETE_NOOP (MultiResultRef)

    HRESULT &mRC;
};

/**
 * The SupportErrorInfoBase template class provides basic error info support.
 *
 * Basic error info support includes a group of setError() methods to set
 * extended error information on the current thread. This support does not
 * include all necessary implementation details (for example, implementation of
 * the ISupportErrorInfo interface on MS COM) to make the error info support
 * fully functional in a target component. These details are provided by the
 * SupportErrorInfoDerived class.
 *
 * This way, this class is intended to be directly inherited only by
 * intermediate component base classes that will be then inherited by final
 * component classes through the SupportErrorInfoDerived template class. In
 * all other cases, the SupportErrorInfoImpl class should be used as a base for
 * final component classes instead.
 */
class SupportErrorInfoBase
{
    static HRESULT setErrorInternal (HRESULT aResultCode, const GUID *aIID,
                                     const char *aComponent, const char *aText,
                                     bool aWarning,
                                     IVirtualBoxErrorInfo *aInfo = NULL);

protected:

    /**
     * Returns an interface ID that is to be used in short setError() variants
     * to specify the interface that has defined the error. Must be implemented
     * in subclasses.
     */
    virtual const GUID &mainInterfaceID() const = 0;

    /**
     * Returns an component name (in UTF8) that is to be used in short
     * setError() variants to specify the interface that has defined the error.
     * Must be implemented in subclasses.
     */
    virtual const char *componentName() const = 0;

    /**
     * Sets the error information for the current thread.
     *
     * When the error information is set, it can be retrieved by a caller of an
     * interface method using the respective methods that return an IErrorInfo
     * object in MS COM (nsIException object in XPCOM) set for the current
     * thread. This object can also be or queried for the platform-independent
     * IVirtualBoxErrorInfo interface that provides extended error information
     * (only for components from the VirtualBox COM library). Alternatively, the
     * platform-independent ErrorInfo class can be used to retrieve error info
     * in a convenient way.
     *
     * It is assumed that the interface method that uses this function returns
     * an non S_OK result code to the caller (otherwise, there is no reason
     * for the caller to check for error info after method invocation).
     *
     * Here is a table of correspondence between this method's arguments and
     * IErrorInfo/nsIException/IVirtualBoxErrorInfo attributes/methods:
     *
     * <pre>
     * argument    IErrorInfo      nsIException    IVirtualBoxErrorInfo
     * ----------------------------------------------------------------
     * resultCode  --              result          resultCode
     * iid         GetGUID         --              interfaceID
     * component   GetSource       --              component
     * text        GetDescription  message         text
     * </pre>
     *
     * Note that this is a generic method. There are more convenient overloaded
     * versions that automatically substitute some arguments taking their
     * values from the template parameters. See #setError (HRESULT, const char
     * *, ...) for an example.
     *
     * It is also possible to turn on the multi-error mode so that setting a new
     * error information does not destroy the previous error (if any) but makes
     * it accessible using the IVirtualBoxErrorInfo::next attribute. See
     * MultiResult for more information.
     *
     * @param  aResultCode  Result (error) code, must not be S_OK.
     * @param  aIID         IID of the interface that defines the error.
     * @param  aComponent   Name of the component that sets the error (UTF8).
     * @param  aText        Error message in UTF8 (must not be NULL).
     *
     * @return @a aResultCode argument, for convenience. If an error occurs
     *         while setting error info itself, that error is returned instead
     *         of the @a aResultCode argument.
     */
    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const char *aComponent, const char *aText)
    {
        return setErrorInternal (aResultCode, &aIID, aComponent, aText,
                                 false /* aWarning */);
    }

    /**
     * Same as #setError() except that it makes sure that aResultCode doesn't
     * have the error severity bit (31) set when passed down to the created
     * IVirtualBoxErrorInfo object.
     *
     * The error severity bit is always cleared by this call, thereof you can
     * use ordinary E_XXX result code constants, for convenience. However, this
     * behavior may be non-standard on some COM platforms.
     */
    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const char *aComponent, const char *aText)
    {
        return setErrorInternal (aResultCode, &aIID, aComponent, aText,
                                 true /* aWarning */);
    }

    /**
     * Same as #setError (HRESULT, const GUID &, const char *, const char *) but
     * interprets the @a aText argument as a RTPrintf-like format string and the
     * @a aArgs argument as an argument list for this format string.
     */
    static HRESULT setErrorV (HRESULT aResultCode, const GUID &aIID,
                              const char *aComponent, const char *aText,
                              va_list aArgs)
    {
        return setErrorInternal (aResultCode, &aIID, aComponent,
                                 Utf8StrFmtVA (aText, aArgs),
                                 false /* aWarning */);
    }

    /**
     * Same as #setWarning (HRESULT, const GUID &, const char *, const char *)
     * but interprets the @a aText argument as a RTPrintf-like format string and
     * the @a aArgs argument as an argument list for this format string.
     */
    static HRESULT setWarningV (HRESULT aResultCode, const GUID &aIID,
                                const char *aComponent, const char *aText,
                                va_list aArgs)
    {
        return setErrorInternal (aResultCode, &aIID, aComponent,
                                 Utf8StrFmtVA (aText, aArgs),
                                 true /* aWarning */);
    }

    /**
     * Same as #setError (HRESULT, const GUID &, const char *, const char *) but
     * interprets the @a aText argument as a RTPrintf-like format string and
     * takes a variable list of arguments for this format string.
     */
    static HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                             const char *aComponent, const char *aText,
                             ...);

    /**
     * Same as #setWarning (HRESULT, const GUID &, const char *, const char *)
     * but interprets the @a aText argument as a RTPrintf-like format string and
     * takes a variable list of arguments for this format string.
     */
    static HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                               const char *aComponent, const char *aText,
                               ...);

    /**
     * Sets the given error info object on the current thread.
     *
     * Note that In multi-error mode (see MultiResult), the existing error info
     * object (if any) will be preserved by attaching it to the tail of the
     * error chain of the given aInfo object.
     *
     * @param aInfo     Error info object to set (must not be NULL).
     */
    static HRESULT setErrorInfo (IVirtualBoxErrorInfo *aInfo)
    {
        AssertReturn (aInfo != NULL, E_FAIL);
        return setErrorInternal (0, NULL, NULL, NULL, false, aInfo);
    }

    /**
     * Same as #setError (HRESULT, const GUID &, const char *, const char *,
     * ...) but uses the return value of the mainInterfaceID() method as an @a
     * aIID argument and the return value of the componentName() method as a @a
     * aComponent argument.
     *
     * This method is the most common (and convenient) way to set error
     * information from within a component method. The typical usage pattern is:
     * <code>
     *     return setError (E_FAIL, "Terrible Error");
     * </code>
     * or
     * <code>
     *     HRESULT rc = setError (E_FAIL, "Terrible Error");
     *     ...
     *     return rc;
     * </code>
     */
    HRESULT setError (HRESULT aResultCode, const char *aText, ...);

    /**
     * Same as #setWarning (HRESULT, const GUID &, const char *, const char *,
     * ...) but uses the return value of the mainInterfaceID() method as an @a
     * aIID argument and the return value of the componentName() method as a @a
     * aComponent argument.
     *
     * This method is the most common (and convenient) way to set warning
     * information from within a component method. The typical usage pattern is:
     * <code>
     *     return setWarning (E_FAIL, "Dangerous warning");
     * </code>
     * or
     * <code>
     *     HRESULT rc = setWarning (E_FAIL, "Dangerous warning");
     *     ...
     *     return rc;
     * </code>
     */
    HRESULT setWarning (HRESULT aResultCode, const char *aText, ...);

    /**
     * Same as #setError (HRESULT, const char *, ...) but takes a va_list
     * argument instead of a variable argument list.
     */
    HRESULT setErrorV (HRESULT aResultCode, const char *aText, va_list aArgs)
    {
        return setError (aResultCode, mainInterfaceID(), componentName(),
                         aText, aArgs);
    }

    /**
     * Same as #setWarning (HRESULT, const char *, ...) but takes a va_list
     * argument instead of a variable argument list.
     */
    HRESULT setWarningV (HRESULT aResultCode, const char *aText, va_list aArgs)
    {
        return setWarning (aResultCode, mainInterfaceID(), componentName(),
                           aText, aArgs);
    }

    /**
     * Same as #setError (HRESULT, const char *, ...) but allows to specify the
     * interface ID manually.
     */
    HRESULT setError (HRESULT aResultCode, const GUID &aIID,
                      const char *aText, ...);

    /**
     * Same as #setWarning (HRESULT, const char *, ...) but allows to specify
     * the interface ID manually.
     */
    HRESULT setWarning (HRESULT aResultCode, const GUID &aIID,
                        const char *aText, ...);
};

////////////////////////////////////////////////////////////////////////////////

/**
 * The SupportErrorInfoDerived template class implements the remaining parts
 * of error info support in addition to SupportErrorInfoBase.
 *
 * These parts include the ISupportErrorInfo implementation on the MS COM
 * platform and implementations of mandatory SupportErrorInfoBase virtual
 * methods.
 *
 * On MS COM, the @a C template argument must declare a COM interface map using
 * BEGIN_COM_MAP / END_COM_MAP macros and this map must contain a
 * COM_INTERFACE_ENTRY(ISupportErrorInfo) definition. All interface entries that
 * follow it will be considered to support IErrorInfo, i.e. the
 * InterfaceSupportsErrorInfo() implementation will return S_OK for the
 * corresponding IIDs.
 *
 * On all platforms, the @a C template argument must be a subclass of
 * SupportErrorInfoBase and also define the following method: <tt>public static
 * const char *ComponentName()</tt> that will be used as a value returned by the
 * SupportErrorInfoBase::componentName() implementation.
 *
 * If SupportErrorInfoBase is used as a base for an intermediate component base
 * class FooBase then the final component FooFinal that inherits FooBase should
 * use this template class as follows:
 * <code>
 *     class FooFinal : public SupportErrorInfoDerived <FooBase, FooFinal, IFoo>
 *     {
 *         ...
 *     };
 * </code>
 *
 * Note that if you don not use intermediate component base classes, you should
 * use the SupportErrorInfoImpl class as a base for your component instead.
 *
 * @param B     Intermediate component base derived from SupportErrorInfoBase.
 * @param C     Component class that implements one or more COM interfaces.
 * @param I     Default interface for the component (for short #setError()
 *              versions).
 */
template <class B, class C, class I>
class ATL_NO_VTABLE SupportErrorInfoDerived : public B
#if !defined (VBOX_WITH_XPCOM)
    , public ISupportErrorInfo
#endif
{
public:

#if !defined (VBOX_WITH_XPCOM)
    STDMETHOD(InterfaceSupportsErrorInfo) (REFIID aIID)
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
                /* skip the COM map entries until ISupportErrorInfo is found */
                bISupportErrorInfoFound =
                    InlineIsEqualGUID (*(pEntries->piid), IID_ISupportErrorInfo);
            }
            else
            {
                /* look for the requested interface in the rest of the com map */
                bSupports = InlineIsEqualGUID (*(pEntries->piid), aIID);
            }
            pEntries++;
        }

        Assert (bISupportErrorInfoFound);

        return bSupports ? S_OK : S_FALSE;
    }
#endif /* !defined (VBOX_WITH_XPCOM) */

protected:

    virtual const GUID &mainInterfaceID() const { return COM_IIDOF (I); }

    virtual const char *componentName() const { return C::ComponentName(); }
};

////////////////////////////////////////////////////////////////////////////////

/**
 * The SupportErrorInfoImpl template class provides complete error info support
 * for COM component classes.
 *
 * Complete error info support includes what both SupportErrorInfoBase and
 * SupportErrorInfoDerived provide, e.g. a variety of setError() methods to
 * set extended error information from within a component's method
 * implementation and all necessary additional interface implementations (see
 * descriptions of these classes for details).
 *
 * To add error info support to a Foo component that implements a IFoo
 * interface, use the following pattern:
 * <code>
 *     class Foo : public SupportErrorInfoImpl <Foo, IFoo>
 *     {
 *     public:
 *
 *         ...
 *
 *         static const char *ComponentName() const { return "Foo"; }
 *     };
 * </code>
 *
 * Note that your component class (the @a C template argument) must provide the
 * ComponentName() implementation as shown above.
 *
 * @param C Component class that implements one or more COM interfaces.
 * @param I Default interface for the component (for short #setError()
 *          versions).
 */
template <class C, class I>
class ATL_NO_VTABLE SupportErrorInfoImpl
    : public SupportErrorInfoDerived <SupportErrorInfoBase, C, I>
{
};

} /* namespace com */

#endif /* ___VBox_com_SupportErrorInfo_h */

