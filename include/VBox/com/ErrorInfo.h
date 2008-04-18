/** @file
 * MS COM / XPCOM Abstraction Layer:
 * ErrorInfo class declaration
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

#ifndef ___VBox_com_ErrorInfo_h
#define ___VBox_com_ErrorInfo_h

#include "VBox/com/ptr.h"
#include "VBox/com/string.h"
#include "VBox/com/Guid.h"
#include "VBox/com/assert.h"

#include <iprt/memory> // for auto_copy_ptr

struct IProgress;
struct IVirtualBoxErrorInfo;

namespace com
{

/**
 *  The ErrorInfo class provides a convenient way to retrieve error
 *  information set by the most recent interface method, that was invoked on
 *  the current thread and returned an unsuccessful result code.
 *
 *  Once the instance of this class is created, the error information for
 *  the current thread is cleared.
 *
 *  There is no sence to use instances of this class after the last
 *  invoked interface method returns a success.
 *
 *  The class usage pattern is as follows:
 *  <code>
 *      IFoo *foo;
 *      ...
 *      HRESULT rc = foo->SomeMethod();
 *      if (FAILED (rc)) {
 *          ErrorInfo info (foo);
 *          if (info.isFullAvailable()) {
 *              printf ("error message = %ls\n", info.getText().raw());
 *          }
 *      }
 *  </code>
 *
 *  This class fetches error information using the IErrorInfo interface on
 *  Win32 (MS COM) or the nsIException interface on other platforms (XPCOM),
 *  or the extended IVirtualBoxErrorInfo interface when when it is available
 *  (i.e. a given IErrorInfo or nsIException instance implements it).
 *  Currently, IVirtualBoxErrorInfo is only available for VirtualBox components.
 *
 *  ErrorInfo::isFullAvailable() and ErrorInfo::isBasicAvailable() determine
 *  what level of error information is available. If #isBasicAvailable()
 *  returns true, it means that only IErrorInfo or nsIException is available as
 *  the source of information (depending on the platform), but not
 *  IVirtualBoxErrorInfo. If #isFullAvailable() returns true, it means that all
 *  three interfaces are available. If both methods return false, no error info
 *  is available at all.
 *
 *  Here is a table of correspondence between this class methods and
 *  and IErrorInfo/nsIException/IVirtualBoxErrorInfo attributes/methods:
 *
 *  ErrorInfo       IErrorInfo      nsIException    IVirtualBoxErrorInfo
 *  --------------------------------------------------------------------
 *  getResultCode   --              result          resultCode
 *  getIID          GetGUID         --              interfaceID
 *  getComponent    GetSource       --              component
 *  getText         GetDescription  message         text
 *
 *  '--' means that this interface does not provide the corresponding portion
 *  of information, therefore it is useless to query it if only
 *  #isBasicAvailable() returns true. As it can be seen, the amount of
 *  information provided at the basic level, depends on the platform
 *  (MS COM or XPCOM).
 */
class ErrorInfo
{
public:

    /**
     *  Constructs a new, "interfaceless" ErrorInfo instance that takes
     *  the error information possibly set on the current thread by an
     *  interface method of some COM component or by the COM subsystem.
     *
     *  This constructor is useful, for example, after an unsuccessful attempt
     *  to instantiate (create) a component, so there is no any valid interface
     *  pointer available.
     */
    explicit ErrorInfo()
        : mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK)
        { init(); }

    /**
     *  Constructs a new, "interfaceless" ErrorInfo instance that takes
     *  the error information possibly set on the current thread by an
     *  interface method of the given interface pointer.

     *  If the given interface does not support providing error information or,
     *  for some reason didn't set any error information, both
     *  #isFullAvailable() and #isBasicAvailable() will return |false|.
     *
     *  @param aPtr pointer to the interface whose method returned an
     *              error
     */
    template <class I> ErrorInfo (I *aPtr)
        : mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK)
        { init (aPtr, COM_IIDOF(I)); }

    /**
     *  Constructs a new ErrorInfo instance from the smart interface pointer.
     *  See template <class I> ErrorInfo (I *aPtr) for details
     *
     *  @param aPtr smart pointer to the interface whose method returned
     *              an error
     */
    template <class I> ErrorInfo (const ComPtr <I> &aPtr)
        : mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK)
        { init (static_cast <I*> (aPtr), COM_IIDOF(I)); }

    /** Specialization for the IVirtualBoxErrorInfo smart pointer */
    ErrorInfo (const ComPtr <IVirtualBoxErrorInfo> &aPtr)
        : mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK)
        { init (aPtr); }

    /**
     *  Constructs a new ErrorInfo instance from the IVirtualBoxErrorInfo
     *  interface pointer. If this pointer is not NULL, both #isFullAvailable()
     *  and #isBasicAvailable() will return |true|.
     *
     *  @param aInfo    pointer to the IVirtualBoxErrorInfo interface that
     *                  holds error info to be fetched by this instance
     */
    ErrorInfo (IVirtualBoxErrorInfo *aInfo)
        : mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK)
        { init (aInfo); }

    virtual ~ErrorInfo();

    /**
     *  Returns whether basic error info is actually available for the current
     *  thread. If the instance was created from an interface pointer that
     *  supports basic error info and successfully provided it, or if it is an
     *  "interfaceless" instance and there is some error info for the current
     *  thread, the returned value will be true.
     *
     *  See the class description for details about the basic error info level.
     *
     *  The appropriate methods of this class provide meaningful info only when
     *  this method returns true (otherwise they simply return NULL-like values).
     */
    bool isBasicAvailable() const { return mIsBasicAvailable; }

    /**
     *  Returns whether full error info is actually available for the current
     *  thread. If the instance was created from an interface pointer that
     *  supports full error info and successfully provided it, or if it is an
     *  "interfaceless" instance and there is some error info for the current
     *  thread, the returned value will be true.
     *
     *  See the class description for details about the full error info level.
     *
     *  The appropriate methods of this class provide meaningful info only when
     *  this method returns true (otherwise they simply return NULL-like values).
     */
    bool isFullAvailable() const { return mIsFullAvailable; }

    /** 
     *  Returns @c true if both isBasicAvailable() and isFullAvailable() are
     *  @c false. 
     */
    bool isNull() const { return !mIsBasicAvailable && !mIsFullAvailable; }

    /**
     *  Returns the COM result code of the failed operation.
     */
    HRESULT getResultCode() const { return mResultCode; }

    /**
     *  Returns the IID of the interface that defined the error.
     */
    const Guid &getInterfaceID() const { return mInterfaceID; }

    /**
     *  Returns the name of the component that generated the error.
     */
    const Bstr &getComponent() const { return mComponent; }

    /**
     *  Returns the textual description of the error.
     */
    const Bstr &getText() const { return mText; }

    /**
     *  Returns the next error information object or @c NULL if there is none.
     */
    const ErrorInfo *getNext() const { return mNext.get(); }

    /**
     *  Returns the name of the interface that defined the error
     */
    const Bstr &getInterfaceName() const { return mInterfaceName; }

    /**
     *  Returns the IID of the interface that returned the error.
     *
     *  This method returns a non-null IID only if the instance was created
     *  using #template <class I> ErrorInfo (I *i) or
     *  template <class I> ErrorInfo (const ComPtr <I> &i) constructor.
     */
    const Guid &getCalleeIID() const { return mCalleeIID; }

    /**
     *  Returns the name of the interface that returned the error
     *
     *  This method returns a non-null name only if the instance was created
     *  using #template <class I> ErrorInfo (I *i) or
     *  template <class I> ErrorInfo (const ComPtr <I> &i) constructor.
     */
    const Bstr &getCalleeName() const { return mCalleeName; }

    /**
     *  Prints error information stored in this instance to the console.
     *  Intended mainly for debugging and for simple command-line tools.
     *
     *  @param aPrefix  optional prefix
     */
    void print (const char *aPrefix = NULL);

    /**
     *  Resets all collected error information. #isNull() will
     *  return @c true after this method is called.
     */
    void setNull()
    {
        mIsBasicAvailable = false;
        mIsFullAvailable = false;

        mResultCode = S_OK;
        mInterfaceID.clear();
        mComponent.setNull();
        mText.setNull();
        mNext.reset();
        mInterfaceName.setNull();
        mCalleeIID.clear();
        mCalleeName.setNull();
        mErrorInfo.setNull();
    }

protected:

    ErrorInfo (bool aDummy)
        : mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK)
        {}

    void init (bool aKeepObj = false);
    void init (IUnknown *aUnk, const GUID &aIID, bool aKeepObj = false);
    void init (IVirtualBoxErrorInfo *aInfo);

    bool mIsBasicAvailable : 1;
    bool mIsFullAvailable : 1;

    HRESULT mResultCode;
    Guid mInterfaceID;
    Bstr mComponent;
    Bstr mText;

    cppx::auto_copy_ptr <ErrorInfo> mNext;

    Bstr mInterfaceName;
    Guid mCalleeIID;
    Bstr mCalleeName;

    ComPtr <IUnknown> mErrorInfo;
};

/**
 *  A convenience subclass of ErrorInfo that, given an IProgress interface
 *  pointer, reads its errorInfo attribute and uses the returned
 *  IVirtualBoxErrorInfo instance to construct itself.
 */
class ProgressErrorInfo : public ErrorInfo
{
public:

    /**
     *  Constructs a new instance by fetchig error information from the
     *  IProgress interface pointer. If the progress object is not NULL,
     *  its completed attribute is true, resultCode represents a failure,
     *  and the errorInfo attribute returns a valid IVirtualBoxErrorInfo pointer,
     *  both #isFullAvailable() and #isBasicAvailable() will return true.
     *
     *  @param  progress    the progress object representing a failed operation
     */
    ProgressErrorInfo (IProgress *progress);
};

/**
 *  A convenience subclass of ErrorInfo that allows to preserve the current
 *  error info. Instances of this class fetch an error info object set on the
 *  current thread and keep a reference to it, which allows to restore it
 *  later using the #restore() method. This is useful to preserve error
 *  information returned by some method for the duration of making another COM
 *  call that may set its own error info and overwrite the existing
 *  one. Preserving and restoring error information makes sense when some
 *  method wants to return error information set by other call as its own
 *  error information while it still needs to make another call before return.
 *
 *  Instead of calling #restore() explicitly you may let the object destructor
 *  do it for you, if you correctly limit the object's lifeime.
 *
 *  The usage pattern is:
 *  <code>
 *      rc = foo->method();
 *      if (FAILED (rc))
 *      {
 *           ErrorInfoKeeper eik;
 *           ...
 *           // bar may return error info as well
 *           bar->method();
 *           ...
 *           // no need to call #restore() explicitly here because the eik's
 *           // destructor will restore error info fetched after the failed
 *           // call to foo before returning to the caller
 *           return rc;
 *      }
 *  </code>
 */
class ErrorInfoKeeper : public ErrorInfo
{
public:

    /**
     *  Constructs a new instance that will fetch the current error info if
     *  @a aIsNull is @c false (by default) or remain uninitialized (null)
     *  otherwise.
     *
     *  @param aIsNull  @true to prevent fetching error info and leave
     *                  the instance uninitialized.
     */
    ErrorInfoKeeper (bool aIsNull = false)
        : ErrorInfo (false), mForgot (false)
    {
        if (!aIsNull)
            init (true /* aKeepObj */);
    }

    /**
     *  Destroys this instance and automatically calls #restore() which will
     *  either restore error info fetched by the constructor or do nothing
     *  if #forget() was called before destruction. */
    ~ErrorInfoKeeper() { if (!mForgot) restore(); }

    /** 
     *  Tries to (re-)fetch error info set on the current thread.  On success,
     *  the previous error information, if any, will be overwritten with the
     *  new error information. On failure, or if there is no error information
     *  available, this instance will be reset to null.
     */
    void fetch()
    {
        setNull();
        init (true /* aKeepObj */);
    }

    /**
     *  Restores error info fetched by the constructor and forgets it
     *  afterwards.
     *
     *  @return COM result of the restore operation.
     */
    HRESULT restore();

    /**
     *  Forgets error info fetched by the constructor to prevent it from
     *  being restored by #restore() or by the destructor.
     */
    void forget() { mForgot = true; }

    /**
     *  Forgets error info fetched by the constructor to prevent it from
     *  being restored by #restore() or by the destructor, and returns the
     *  stored error info object to the caller.
     */
    ComPtr <IUnknown> takeError() { mForgot = true; return mErrorInfo; }

private:

    bool mForgot : 1;
};

} /* namespace com */

#endif

