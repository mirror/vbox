/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Common definitions
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

#ifndef ___VBox_com_defs_h
#define ___VBox_com_defs_h

/*
 * Include iprt/types.h now to make sure iprt get to stdint.h first,
 * otherwise a system/xpcom header might beat us and we'll be without
 * the macros that are optional in C++.
 */
#include <iprt/types.h>

#if !defined (VBOX_WITH_XPCOM)

#if defined (RT_OS_WINDOWS)

// Windows COM
/////////////////////////////////////////////////////////////////////////////

#include <objbase.h>
#ifndef VBOX_COM_NO_ATL
#include <atlbase.h>
#endif

#define NS_DECL_ISUPPORTS
#define NS_IMPL_ISUPPORTS1_CI(a, b)

/* these are XPCOM only, one for every interface implemented */
#define NS_DECL_ISUPPORTS
#define NS_DECL_IVIRTUALBOX
#define NS_DECL_IMACHINECOLLECTION
#define NS_DECL_IMACHINE

/* input pointer argument to method */
#define INPTR

/* makes the name of the getter interface function (n must be capitalized) */
#define COMGETTER(n)    get_##n
/* makes the name of the setter interface function (n must be capitalized) */
#define COMSETTER(n)    put_##n

/* a type for an input GUID parameter in the interface method declaration */
#define GUIDPARAM           GUID
/* a type for an output GUID parameter in the interface method declaration */
#define GUIDPARAMOUT        GUID*

/**
 *  Returns the const reference to the IID (i.e., |const GUID &|) of the given
 *  interface.
 *
 *  @param i    interface class
 */
#define COM_IIDOF(I) _ATL_IIDOF (I)

#else // defined (RT_OS_WINDOWS)

#error "VBOX_WITH_XPCOM is not defined!"

#endif // defined (RT_OS_WINDOWS)

#else // !defined (VBOX_WITH_XPCOM)

// XPCOM
/////////////////////////////////////////////////////////////////////////////

#if defined (RT_OS_OS2)

/* Make sure OS/2 Toolkit headers are pulled in to have
 * BOOL/ULONG/etc. typedefs already defined in order to be able to redefine
 * them using #define. */
#define INCL_BASE
#define INCL_PM
#include <os2.h>

/* OS/2 Toolkit defines TRUE and FALSE */
#undef FALSE
#undef TRUE

#endif // defined (RT_OS_OS2)

#if defined (RT_OS_DARWIN)
  /* CFBase.h defines these*/
# undef FALSE
# undef TRUE
#endif  /* RT_OS_DARWIN */

#include <nsID.h>

#define ATL_NO_VTABLE
#define DECLARE_CLASSFACTORY(a)
#define DECLARE_CLASSFACTORY_SINGLETON(a)
#define DECLARE_REGISTRY_RESOURCEID(a)
#define DECLARE_NOT_AGGREGATABLE(a)
#define DECLARE_PROTECT_FINAL_CONSTRUCT(a)
#define BEGIN_COM_MAP(a)
#define COM_INTERFACE_ENTRY(a)
#define COM_INTERFACE_ENTRY2(a,b)
#define END_COM_MAP(a)

#define HRESULT nsresult
#define SUCCEEDED NS_SUCCEEDED
#define FAILED NS_FAILED
#define NS_NULL nsnull

#define IUnknown nsISupports

#define BOOL    PRBool
#define BYTE    PRUint8
#define SHORT   PRInt16
#define USHORT  PRUint16
#define LONG    PRInt32
#define ULONG   PRUint32
#define LONG64  PRInt64
#define ULONG64 PRUint64

#define BSTR    PRUnichar *
#define LPBSTR  BSTR *
#define OLECHAR wchar_t

#define FALSE PR_FALSE
#define TRUE PR_TRUE

/* makes the name of the getter interface function (n must be capitalized) */
#define COMGETTER(n)    Get##n
/* makes the name of the setter interface function (n must be capitalized) */
#define COMSETTER(n)    Set##n

/* a type to define a raw GUID variable (better to use the Guid class) */
#define GUID                nsID
/* a type for an input GUID parameter in the interface method declaration */
#define GUIDPARAM           nsID &
/* a type for an output GUID parameter in the interface method declaration */
#define GUIDPARAMOUT        nsID **

/* CLSID and IID for compatibility with Win32 */
typedef nsCID   CLSID;
typedef nsIID   IID;

/* OLE error codes */
#define S_OK                NS_OK
#define E_UNEXPECTED        NS_ERROR_UNEXPECTED
#define E_NOTIMPL           NS_ERROR_NOT_IMPLEMENTED
#define E_OUTOFMEMORY       NS_ERROR_OUT_OF_MEMORY
#define E_INVALIDARG        NS_ERROR_INVALID_ARG
#define E_NOINTERFACE       NS_ERROR_NO_INTERFACE
#define E_POINTER           NS_ERROR_NULL_POINTER
#define E_ABORT             NS_ERROR_ABORT
#define E_FAIL              NS_ERROR_FAILURE
/* Note: a better analog for E_ACCESSDENIED would probably be
 * NS_ERROR_NOT_AVAILABLE, but we want binary compatibility for now. */
#define E_ACCESSDENIED      ((nsresult) 0x80070005L)

#define STDMETHOD(a) NS_IMETHOD a
#define STDMETHODIMP NS_IMETHODIMP

#define COM_IIDOF(I) NS_GET_IID (I)

/* two very simple ATL emulator classes to provide
 * FinalConstruct()/FinalRelease() functionality on Linux */

class CComObjectRootEx
{
public:
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() {}
};

template <class Base> class CComObject : public Base
{
public:
    virtual ~CComObject() { this->FinalRelease(); }
};

/* input pointer argument to method */
#define INPTR const

/* helper functions */
extern "C"
{
BSTR SysAllocString (const OLECHAR* sz);
BSTR SysAllocStringByteLen (char *psz, unsigned int len);
BSTR SysAllocStringLen (const OLECHAR *pch, unsigned int cch);
void SysFreeString (BSTR bstr);
int SysReAllocString (BSTR *pbstr, const OLECHAR *psz);
int SysReAllocStringLen (BSTR *pbstr, const OLECHAR *psz, unsigned int cch);
unsigned int SysStringByteLen (BSTR bstr);
unsigned int SysStringLen (BSTR bstr);
}

/**
 *  'Constructor' for the component class.
 *  This constructor, as opposed to NS_GENERIC_FACTORY_CONSTRUCTOR,
 *  assumes that the component class is derived from the CComObjectRootEx<>
 *  template, so it calls FinalConstruct() right after object creation
 *  and ensures that FinalRelease() will be called right before destruction.
 *  The result from FinalConstruct() is returned to the caller.
 */
#define NS_GENERIC_FACTORY_CONSTRUCTOR_WITH_RC(_InstanceClass)                \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    CComObject <_InstanceClass> *inst = new CComObject <_InstanceClass>();    \
    if (NULL == inst) {                                                       \
        rv = NS_ERROR_OUT_OF_MEMORY;                                          \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    NS_ADDREF(inst); /* protect FinalConstruct() */                           \
    rv = inst->FinalConstruct();                                              \
    if (NS_SUCCEEDED(rv))                                                     \
        rv = inst->QueryInterface(aIID, aResult);                             \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}

/**
 *  'Constructor' that uses an existing getter function that gets a singleton.
 *  The getter function must have the following prototype:
 *      nsresult _GetterProc (_InstanceClass **inst)
 *  This constructor, as opposed to NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR,
 *  lets the getter function return a result code that is passed back to the
 *  caller that tries to instantiate the object.
 *  NOTE: assumes that getter does an AddRef - so additional AddRef is not done.
 */
#define NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR_WITH_RC(_InstanceClass, _GetterProc) \
static NS_IMETHODIMP                                                          \
_InstanceClass##Constructor(nsISupports *aOuter, REFNSIID aIID,               \
                            void **aResult)                                   \
{                                                                             \
    nsresult rv;                                                              \
                                                                              \
    _InstanceClass * inst;                                                    \
                                                                              \
    *aResult = NULL;                                                          \
    if (NULL != aOuter) {                                                     \
        rv = NS_ERROR_NO_AGGREGATION;                                         \
        return rv;                                                            \
    }                                                                         \
                                                                              \
    rv = _GetterProc(&inst);                                                  \
    if (NS_FAILED(rv))                                                        \
        return rv;                                                            \
                                                                              \
    /* sanity check */                                                        \
    if (NULL == inst)                                                         \
        return NS_ERROR_OUT_OF_MEMORY;                                        \
                                                                              \
    /* NS_ADDREF(inst); */                                                    \
    if (NS_SUCCEEDED(rv)) {                                                   \
        rv = inst->QueryInterface(aIID, aResult);                             \
    }                                                                         \
    NS_RELEASE(inst);                                                         \
                                                                              \
    return rv;                                                                \
}

#endif // !defined (RT_OS_WINDOWS)

/**
 *  Declares a whar_t string literal from the argument.
 *  Necessary to overcome MSC / GCC differences.
 *  @param s    expression to stringify
 */
#if defined (_MSC_VER)
#   define WSTR_LITERAL(s)  L#s
#elif defined (__GNUC__)
#   define WSTR_LITERAL(s)  L""#s
#else
#   error "Unsupported compiler!"
#endif

#endif

