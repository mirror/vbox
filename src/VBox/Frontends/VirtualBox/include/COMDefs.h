/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Various COM definitions and COM wrapper class declarations
 *
 * This header is used in conjunction with the header generated from
 * XIDL expressed interface definitions to provide cross-platform Qt-based
 * interface wrapper classes.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __COMDefs_h__
#define __COMDefs_h__

#include <qglobal.h>
#include <qstring.h>
#include <quuid.h>

/*
 * common COM / XPCOM includes and defines
 */

#if defined(Q_OS_WIN32)

    #include <objbase.h>
    /* for _ATL_IIDOF */
    #include <atldef.h>

    #include <VBox/types.h>

    /* these are XPCOM only */
    #define NS_DECL_ISUPPORTS

    /* makes interface getter/setter names (n must be capitalized) */
    #define COMGETTER(n)    get_##n
    #define COMSETTER(n)    put_##n

    #define IN_BSTRPARAM    BSTR
    #define IN_GUIDPARAM    GUID

    /* const reference to IID of the interface */
    #define COM_IIDOF(I)    _ATL_IIDOF (I)

#else

    #include <VBox/types.h>

    #include <nsMemory.h>
    #include <nsIComponentManager.h>
    #include <ipcIDConnectService.h>

    class nsIComponentManager;
    class nsIEventQueue;
    class ipcIDConnectService;

    typedef nsCID   CLSID;
    typedef nsIID   IID;

    class XPCOMEventQSocketListener;

    #define STDMETHOD(a) NS_IMETHOD a
    #define STDMETHODIMP NS_IMETHODIMP

    #define HRESULT     nsresult
    #define SUCCEEDED   NS_SUCCEEDED
    #define FAILED      NS_FAILED

    /// @todo error code mappings
    #define S_OK            NS_OK
    #define E_UNEXPECTED    (HRESULT)0x8000FFFFL
    #define E_NOTIMPL       (HRESULT)0x80004001L
    #define E_OUTOFMEMORY   (HRESULT)0x8007000EL
    #define E_INVALIDARG    (HRESULT)0x80070057L
    #define E_NOINTERFACE   (HRESULT)0x80004002L
    #define E_POINTER       (HRESULT)0x80004003L
    #define E_HANDLE        (HRESULT)0x80070006L
    #define E_ABORT         (HRESULT)0x80004004L
    #define E_FAIL          (HRESULT)0x80004005L
    #define E_ACCESSDENIED  (HRESULT)0x80070005L

    #define IUnknown    nsISupports

    #define BOOL        PRBool
    #define BYTE        PRUint8
    #define SHORT       PRInt16
    #define USHORT      PRUint16
    #define LONG        PRInt32
    #define ULONG       PRUint32
    #define LONG64      PRInt64
    #define ULONG64     PRUint64

    #define BSTR        PRUnichar*
    #define LPBSTR      BSTR*
    #define OLECHAR     wchar_t
    #define GUID        nsID

    #define IN_BSTRPARAM    const BSTR
    #define IN_GUIDPARAM    const nsID &

    /* makes interface getter/setter names (n must be capitalized) */
    #define COMGETTER(n)    Get##n
    #define COMSETTER(n)    Set##n

    /* const reference to IID of the interface */
    #define COM_IIDOF(I)    NS_GET_IID (I)

    /* helper functions (defined in the Runtime3 library) */
    extern "C" {
        BSTR SysAllocString (const OLECHAR* sz);
        BSTR SysAllocStringByteLen (char *psz, unsigned int len);
        BSTR SysAllocStringLen (const OLECHAR *pch, unsigned int cch);
        void SysFreeString (BSTR bstr);
        int SysReAllocString (BSTR *pbstr, const OLECHAR *psz);
        int SysReAllocStringLen (BSTR *pbstr, const OLECHAR *psz, unsigned int cch);
        unsigned int SysStringByteLen (BSTR bstr);
        unsigned int SysStringLen (BSTR bstr);
    }

#endif

/* VirtualBox interfaces declarations */
#if defined(Q_OS_WIN32)
    #include <VirtualBox.h>
#else
    #include <VirtualBox_XPCOM.h>
#endif

#include "VBoxDefs.h"

/////////////////////////////////////////////////////////////////////////////

class CVirtualBoxErrorInfo;

/** Represents extended error information */
class COMErrorInfo
{
public:

    COMErrorInfo()
        : mIsNull (true)
        , mIsBasicAvailable (false), mIsFullAvailable (false)
        , mResultCode (S_OK) {}

    COMErrorInfo (const CVirtualBoxErrorInfo &info) { init (info); }

    /* the default copy ctor and assignment op are ok */

    bool isNull() const { return mIsNull; }

    bool isBasicAvailable() const { return mIsBasicAvailable; }
    bool isFullAvailable() const { return mIsFullAvailable; }

    HRESULT resultCode() const { return mResultCode; }
    QUuid interfaceID() const { return mInterfaceID; }
    QString component() const { return mComponent; }
    QString text() const { return mText; }

    QString interfaceName() const { return mInterfaceName; }
    QUuid calleeIID() const { return mCalleeIID; }
    QString calleeName() const { return mCalleeName; }

private:

    void init (const CVirtualBoxErrorInfo &info);
    void fetchFromCurrentThread (IUnknown *callee, const GUID *calleeIID);

    static QString getInterfaceNameFromIID (const QUuid &id);

    bool mIsNull : 1;
    bool mIsBasicAvailable : 1;
    bool mIsFullAvailable : 1;

    HRESULT mResultCode;
    QUuid mInterfaceID;
    QString mComponent;
    QString mText;

    QString mInterfaceName;
    QUuid mCalleeIID;
    QString mCalleeName;

    friend class COMBaseWithEI;
};

/////////////////////////////////////////////////////////////////////////////

/**
 *  Base COM class the CInterface template and all wrapper classes are derived
 *  from. Provides common functionality for all COM wrappers.
 */
class COMBase
{
public:

    static HRESULT initializeCOM();
    static HRESULT cleanupCOM();

    /** Converts a GUID value to QUuid */
#if defined (Q_OS_WIN32)
    static QUuid toQUuid (const GUID &id) {
        return QUuid (id.Data1, id.Data2, id.Data3,
                      id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3],
                      id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    }
#else
    static QUuid toQUuid (const nsID &id) {
        return QUuid (id.m0, id.m1, id.m2,
                      id.m3[0], id.m3[1], id.m3[2], id.m3[3],
                      id.m3[4], id.m3[5], id.m3[6], id.m3[7]);
    }
#endif

    /**
     *  Returns the result code of the last interface method called
     *  by the wrapper instance or the result of CInterface::createInstance()
     *  operation.
     */
    HRESULT lastRC() const { return rc; }

    /**
     *  Returns error info set by the last unsuccessfully invoked interface
     *  method. Returned error info is useful only if CInterface::lastRC()
     *  represents a failure (i.e. CInterface::isOk() is false).
     */
    virtual COMErrorInfo errorInfo() const { return COMErrorInfo(); }

protected:

    /* no arbitrary instance creations */
    COMBase() : rc (S_OK) {};

#if !defined (Q_OS_WIN32)
    static nsIComponentManager *componentManager;
    static nsIEventQueue* eventQ;
    static ipcIDConnectService *dconnectService;
    static PRUint32 vboxServerID;

    static XPCOMEventQSocketListener *socketListener;
#endif

    /** Adapter to pass QString as input BSTR params */
    class BSTRIn
    {
    public:
        BSTRIn (const QString &s) : bstr (SysAllocString ((const OLECHAR *) s.ucs2())) {}
        ~BSTRIn() {
            if (bstr)
                SysFreeString (bstr);
        }
        operator BSTR() const { return bstr; }

    private:
        BSTR bstr;
    };

    /** Adapter to pass QString as output BSTR params */
    class BSTROut
    {
    public:
        BSTROut (QString &s) : str (s), bstr (0) {}
        ~BSTROut() {
            if (bstr) {
                str = QString::fromUcs2 (bstr);
                SysFreeString (bstr);
            }
        }
        operator BSTR *() { return &bstr; }

    private:
        QString &str;
        BSTR bstr;
    };

    /** Adapter to pass CEnums enums as output VirtualBox enum params (*_T) */
    template <typename CE, typename VE>
    class ENUMOut
    {
    public:
        ENUMOut (CE &e) : ce (e), ve ((VE) 0) {}
        ~ENUMOut() { ce = (CE) ve; }
        operator VE *() { return &ve; }

    private:
        CE &ce;
        VE ve;
    };

#if defined (Q_OS_WIN32)

    /** Adapter to pass QUuid as input GUID params */
    GUID GUIDIn (const QUuid &uuid) const { return uuid; }

    /** Adapter to pass QUuid as output GUID params */
    class GUIDOut
    {
    public:
        GUIDOut (QUuid &id) : uuid (id) {
            ::memset (&guid, 0, sizeof (GUID));
        }
        ~GUIDOut() {
            uuid = QUuid (
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
            );
        }
        operator GUID *() { return &guid; }

    private:
        QUuid &uuid;
        GUID guid;
    };

#else

    /** Adapter to pass QUuid as input GUID params */
    static const nsID &GUIDIn (const QUuid &uuid) {
        return *(const nsID *) &uuid;
    }

    /** Adapter to pass QUuid as output GUID params */
    class GUIDOut
    {
    public:
        GUIDOut (QUuid &id) : uuid (id), nsid (0) {}
        ~GUIDOut() {
            if (nsid) {
                uuid = QUuid (
                    nsid->m0, nsid->m1, nsid->m2,
                    nsid->m3[0], nsid->m3[1], nsid->m3[2], nsid->m3[3],
                    nsid->m3[4], nsid->m3[5], nsid->m3[6], nsid->m3[7]
                );
                nsMemory::Free (nsid);
            }
        }
        operator nsID **() { return &nsid; }

    private:
        QUuid &uuid;
        nsID *nsid;
    };

#endif

    void fetchErrorInfo (IUnknown * /*callee*/, const GUID * /*calleeIID*/) const {}

    mutable HRESULT rc;

    friend class COMErrorInfo;
};

/////////////////////////////////////////////////////////////////////////////

/**
 *  Alternative base class for the CInterface template that adds
 *  the errorInfo() method for providing extended error info about
 *  unsuccessful invocation of the last called interface method.
 */
class COMBaseWithEI : public COMBase
{
public:

    /**
     *  Returns error info set by the last unsuccessfully invoked interface
     *  method. Returned error info is useful only if CInterface::lastRC()
     *  represents a failure (i.e. CInterface::isOk() is false).
     */
    COMErrorInfo errorInfo() const { return errInfo; }

protected:

    /* no arbitrary instance creations */
    COMBaseWithEI() : COMBase () {};

    void fetchErrorInfo (IUnknown *callee, const GUID *calleeIID) const {
        errInfo.fetchFromCurrentThread (callee, calleeIID);
    }

    mutable COMErrorInfo errInfo;
};

/////////////////////////////////////////////////////////////////////////////

/**
 *  Simple class that encapsulates the result code and COMErrorInfo.
 */
class COMResult
{
public:

    COMResult() : mRC (S_OK) {}

    /** Queries the current result code and error info from the given component */
    COMResult (const COMBase &aComponent)
    {
        mErrInfo = aComponent.errorInfo();
        mRC = aComponent.lastRC();
    }

    /** Queries the current result code and error info from the given component */
    COMResult &operator= (const COMBase &aComponent)
    {
        mErrInfo = aComponent.errorInfo();
        mRC = aComponent.lastRC();
        return *this;
    }

    bool isNull() const { return mErrInfo.isNull(); }
    bool isOk() const { return mErrInfo.isNull() && SUCCEEDED (mRC); }

    COMErrorInfo errorInfo() const { return mErrInfo; }
    HRESULT rc() const { return mRC; }

private:

    COMErrorInfo mErrInfo;
    HRESULT mRC;
};

/////////////////////////////////////////////////////////////////////////////

class CUnknown;

/**
 *  Wrapper template class for all interfaces.
 *
 *  All interface methods named as they are in the original, i.e. starting
 *  with the capital letter. All utility non-interface methods are named
 *  starting with the small letter. Utility methods should be not normally
 *  called by the end-user client application.
 *
 *  @param  I   interface class (i.e. IUnknown/nsISupports derivant)
 *  @param  B   base class, either COMBase (by default) or COMBaseWithEI
 */
template <class I, class B = COMBase>
class CInterface : public B
{
public:

    typedef B Base;
    typedef I Iface;

    /* constructors & destructor */

    CInterface() : iface (NULL) {}

    CInterface (const CInterface &that) : B (that), iface (that.iface) {
        addref (iface);
    }

    CInterface (const CUnknown &that);

    CInterface (I *i) : iface (i) { addref (iface); }

    virtual ~CInterface() { release (iface); }

    /* utility methods */

    void createInstance (const CLSID &clsid) {
        AssertMsg (!iface, ("Instance is already non-NULL\n"));
        if (!iface)
        {
#if defined (Q_OS_WIN32)
            B::rc = CoCreateInstance (clsid, NULL, CLSCTX_ALL,
                                   _ATL_IIDOF (I), (void**) &iface);
#else
            /* first, try to create an instance within the in-proc server
             * (for compatibility with Win32) */
            B::rc = B::componentManager->CreateInstance (
                clsid, nsnull, NS_GET_IID (I), (void**) &iface
            );
            if (FAILED (B::rc) && B::dconnectService && B::vboxServerID) {
                /* now try the out-of-proc server if it exists */
                B::rc = B::dconnectService->CreateInstance (
                    B::vboxServerID, clsid,
                    NS_GET_IID (I), (void**) &iface
                );
            }
#endif
            /* fetch error info, but don't assert if it's missing -- many other
             * reasons can lead to an error (w/o providing error info), not only
             * the instance initialization code (that should always provide it) */
            B::fetchErrorInfo (NULL, NULL);
        }
    }

    void attach (I *i) {
        /* be aware of self (from COM point of view) assignment */
        I *old_iface = iface;
        iface = i;
        addref (iface);
        release (old_iface);
        B::rc = S_OK;
    };

    void attachUnknown (IUnknown *i) {
        /* be aware of self (from COM point of view) assignment */
        I *old_iface = iface;
        iface = NULL;
        B::rc = S_OK;
        if (i)
#if defined (Q_OS_WIN32)
            B::rc = i->QueryInterface (_ATL_IIDOF (I), (void**) &iface);
#else
            B::rc = i->QueryInterface (NS_GET_IID (I), (void**) &iface);
#endif
        release (old_iface);
    };

    void detach() { release (iface); iface = NULL; }

    bool isNull() const { return iface == NULL; }

    bool isOk() const { return !isNull() && SUCCEEDED (B::rc); }

    /* utility operators */

    CInterface &operator= (const CInterface &that) {
        attach (that.iface);
        B::operator= (that);
        return *this;
    }

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** @todo bird: dmik, perhaps I missed something, but I didn't find anything equivalent
     * to this. feel free to fix &/| remove this hack. */
    I *getInterface() { return iface; }
#endif

    bool operator== (const CInterface &that) const { return iface == that.iface; }
    bool operator!= (const CInterface &that) const { return iface != that.iface; }

    CInterface &operator= (const CUnknown &that);

protected:

    static void addref (I *i) { if (i) i->AddRef(); }
    static void release (I *i) { if (i) i->Release(); }

    mutable I *iface;
};

/////////////////////////////////////////////////////////////////////////////

class CUnknown : public CInterface <IUnknown, COMBaseWithEI>
{
public:

    CUnknown() : CInterface <IUnknown, COMBaseWithEI> () {}

    template <class C>
    explicit CUnknown (const C &that) {
        iface = NULL;
        if (that.iface)
#if defined (Q_OS_WIN32)
            rc = that.iface->QueryInterface (_ATL_IIDOF (IUnknown), (void**) &iface);
#else
            rc = that.iface->QueryInterface (NS_GET_IID (IUnknown), (void**) &iface);
#endif
        if (SUCCEEDED (rc)) {
            rc = that.lastRC();
            errInfo = that.errorInfo();
        }
    }
    /* specialization for CUnknown */
    CUnknown (const CUnknown &that) : CInterface <IUnknown, COMBaseWithEI> () {
        iface = that.iface;
        addref (iface);
        COMBaseWithEI::operator= (that);
    }

    template <class C>
    CUnknown &operator= (const C &that) {
        /* be aware of self (from COM point of view) assignment */
        IUnknown *old_iface = iface;
        iface = NULL;
        rc = S_OK;
#if defined (Q_OS_WIN32)
        if (that.iface)
            rc = that.iface->QueryInterface (_ATL_IIDOF (IUnknown), (void**) &iface);
#else
        if (that.iface)
            rc = that.iface->QueryInterface (NS_GET_IID (IUnknown), (void**) &iface);
#endif
        if (SUCCEEDED (rc)) {
            rc = that.lastRC();
            errInfo = that.errorInfo();
        }
        release (old_iface);
        return *this;
    }
    /* specialization for CUnknown */
    CUnknown &operator= (const CUnknown &that) {
        attach (that.iface);
        COMBaseWithEI::operator= (that);
        return *this;
    }

    IUnknown *&ifaceRef() { return iface; };
    IUnknown *ifaceRef() const { return iface; };
};

/* inlined CInterface methods that use CUnknown */

template <class I, class B>
inline CInterface <I, B>::CInterface (const CUnknown &that)
    : iface (NULL)
{
    attachUnknown (that.ifaceRef());
    if (SUCCEEDED (B::rc))
        B::operator= ((B &) that);
}

template <class I, class B>
inline CInterface <I, B> &CInterface <I, B>::operator =(const CUnknown &that)
{
    attachUnknown (that.ifaceRef());
    if (SUCCEEDED (B::rc))
        B::operator= ((B &) that);
    return *this;
}

/////////////////////////////////////////////////////////////////////////////

/* include the generated header containing concrete wrapper definitions */
#include "COMWrappers.h"

#endif // __COMDefs_h__
