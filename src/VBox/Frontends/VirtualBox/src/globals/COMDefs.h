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
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __COMDefs_h__
#define __COMDefs_h__

/** @defgroup   grp_QT_COM  Qt-COM Support Layer
 * @{
 *
 * The Qt-COM support layer provides a set of definitions and smart classes for
 * writing simple, clean and platform-independent code to access COM/XPCOM
 * components through exposed COM interfaces. This layer is based on the
 * COM/XPCOM Abstraction Layer library (the VBoxCOM glue library defined in
 * include/VBox/com and implemented in src/VBox/Main/glue).
 *
 * ...
 *
 * @defgroup   grp_QT_COM_arrays    Arrays
 * @{
 *
 * COM/XPCOM arrays are mapped to QVector objects. QVector templates declared
 * with a type that corresponds to the COM type of elements in the array using
 * normal Qt-COM type mapping rules. Here is a code example that demonstrates
 * how to call interface methods that take and return arrays (this example is
 * based on examples given in @ref grp_COM_arrays):
 * @code

    CSomething component;

    // ...

    QVector <LONG> in (3);
    in [0] = -1;
    in [1] = -2;
    in [2] = -3;

    QVector <LONG> out;
    QVector <LONG> ret;

    ret = component.TestArrays (in, out);

    for (size_t i = 0; i < ret.size(); ++ i)
        LogFlow (("*** ret[%u]=%d\n", i, ret [i]));

 * @endcode
 * @}
 */

/* Both VBox/com/assert.h and qglobal.h contain a definition of ASSERT.
 * Either of them can be already included here, so try to shut them up.  */
#undef ASSERT

#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/assert.h>

#undef ASSERT

/* Qt includes */
#include <QString>
#include <QUuid>
#include <QVector>
#include <QMetaType>

#include <iprt/memory> // for auto_copy_ptr

/*
 * Additional COM / XPCOM defines and includes
 */

#if !defined (VBOX_WITH_XPCOM)

#else /* !defined (VBOX_WITH_XPCOM) */

#include <nsXPCOM.h>
#include <nsMemory.h>
#include <nsIComponentManager.h>

class XPCOMEventQSocketListener;

#endif /* !defined (VBOX_WITH_XPCOM) */


/* VirtualBox interfaces declarations */
#if !defined (VBOX_WITH_XPCOM)
    #include <VirtualBox.h>
#else /* !defined (VBOX_WITH_XPCOM) */
    #include <VirtualBox_XPCOM.h>
#endif /* !defined (VBOX_WITH_XPCOM) */

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

    const COMErrorInfo *next() const { return mNext.get(); }

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

    cppx::auto_copy_ptr <COMErrorInfo> mNext;

    QString mInterfaceName;
    QUuid mCalleeIID;
    QString mCalleeName;

    friend class COMBaseWithEI;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Base COM class the CInterface template and all wrapper classes are derived
 * from. Provides common functionality for all COM wrappers.
 */
class COMBase
{
public:

    static HRESULT InitializeCOM();
    static HRESULT CleanupCOM();

    /**
     * Returns the result code of the last interface method called by the
     * wrapper instance or the result of CInterface::createInstance()
     * operation.
     */
    HRESULT lastRC() const { return mRC; }

#if !defined (VBOX_WITH_XPCOM)

    /** Converts a GUID value to QUuid */
    static QUuid ToQUuid (const GUID &id)
    {
        return QUuid (id.Data1, id.Data2, id.Data3,
                      id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3],
                      id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    }

#else /* !defined (VBOX_WITH_XPCOM) */

    /** Converts a GUID value to QUuid */
    static QUuid ToQUuid (const nsID &id)
    {
        return QUuid (id.m0, id.m1, id.m2,
                      id.m3[0], id.m3[1], id.m3[2], id.m3[3],
                      id.m3[4], id.m3[5], id.m3[6], id.m3[7]);
    }

#endif /* !defined (VBOX_WITH_XPCOM) */

    /* Arrays of arbitrary types */

    template <typename QT, typename CT>
    static void ToSafeArray (const QVector <QT> &aVec, com::SafeArray <CT> &aArr)
    {
        aArr.reset (aVec.size());
        for (int i = 0; i < aVec.size(); ++i)
            aArr [i] = static_cast<CT> (aVec.at (i));
    }

    template <typename CT, typename QT>
    static void FromSafeArray (const com::SafeArray <CT> &aArr, QVector <QT> &aVec)
    {
        aVec.resize (static_cast<int> (aArr.size()));
        for (int i = 0; i < aVec.size(); ++i)
            aVec [i] = static_cast<QT> (aArr [i]);
    }

    template <typename QT, typename CT>
    static void ToSafeArray (const QVector <QT *> &aVec, com::SafeArray <CT *> &aArr)
    {
        Q_UNUSED (aVec);
        Q_UNUSED (aArr);
        AssertMsgFailedReturnVoid (("No conversion!\n"));
    }

    template <typename CT, typename QT>
    static void FromSafeArray (const com::SafeArray <CT *> &aArr, QVector <QT *> &aVec)
    {
        Q_UNUSED (aArr);
        Q_UNUSED (aVec);
        AssertMsgFailedReturnVoid (("No conversion!\n"));
    }

    /* Arrays of equal types */

    template <typename T>
    static void ToSafeArray (const QVector <T> &aVec, com::SafeArray <T> &aArr)
    {
        aArr.reset (aVec.size());
        for (int i = 0; i < aVec.size(); ++i)
            aArr [i] = aVec.at (i);
    }

    template <typename T>
    static void FromSafeArray (const com::SafeArray <T> &aArr, QVector <T> &aVec)
    {
        aVec.resize (static_cast<int> (aArr.size()));
        for (int i = 0; i < aVec.size(); ++i)
            aVec [i] = aArr [i];
    }

    /* Arrays of strings */

    static void ToSafeArray (const QVector <QString> &aVec,
                             com::SafeArray <BSTR> &aArr);
    static void FromSafeArray (const com::SafeArray <BSTR> &aArr,
                               QVector <QString> &aVec);

    /* Arrays of GUID */

    static void ToSafeArray (const QVector <QUuid> &aVec,
                             com::SafeGUIDArray &aArr);
    static void FromSafeArray (const com::SafeGUIDArray &aArr,
                               QVector <QUuid> &aVec);

    /* Arrays of enums. Does a cast similar to what ENUMOut does. */

    template <typename QE, typename CE>
    static void ToSafeArray (const QVector <QE> &aVec,
                             com::SafeIfaceArray <CE> &aArr)
    {
        aArr.reset (static_cast <int> (aVec.size()));
        for (int i = 0; i < aVec.size(); ++i)
            aArr [i] = static_cast <CE> (aVec.at (i));
    }

    template <typename CE, typename QE>
    static void FromSafeArray (const com::SafeIfaceArray <CE> &aArr,
                               QVector <QE> &aVec)
    {
        aVec.resize (static_cast <int> (aArr.size()));
        for (int i = 0; i < aVec.size(); ++i)
            aVec [i] = static_cast <QE> (aArr [i]);
    }

    /* Arrays of interface pointers. Note: we need a separate pair of names
     * only because the MSVC8 template matching algorithm is poor and tries to
     * instantiate a com::SafeIfaceArray <BSTR> (!!!) template otherwise for
     * *no* reason and fails. Note that it's also not possible to choose the
     * correct function by specifying template arguments explicitly because then
     * it starts to try to instantiate the com::SafeArray <I> template for
     * *no* reason again and fails too. Definitely, broken. Works in GCC like a
     * charm. */

    template <class CI, class I>
    static void ToSafeIfaceArray (const QVector <CI> &aVec,
                                  com::SafeIfaceArray <I> &aArr)
    {
        aArr.reset (static_cast<int> (aVec.size()));
        for (int i = 0; i < aVec.size(); ++i)
        {
            aArr [i] = aVec.at (i).raw();
            if (aArr [i])
                aArr [i]->AddRef();
        }
    }

    template <class I, class CI>
    static void FromSafeIfaceArray (const com::SafeIfaceArray <I> &aArr,
                                    QVector <CI> &aVec)
    {
        aVec.resize (static_cast<int> (aArr.size()));
        for (int i = 0; i < aVec.size(); ++i)
            aVec [i].attach (aArr [i]);
    }

protected:

    /* no arbitrary instance creations */
    COMBase() : mRC (S_OK) {};

#if defined (VBOX_WITH_XPCOM)
    static XPCOMEventQSocketListener *sSocketListener;
#endif

    /** Adapter to pass QString as input BSTR params */
    class BSTRIn
    {
    public:

        BSTRIn (const QString &s) : bstr (SysAllocString ((const OLECHAR *)
            (s.isNull() ? 0 : s.utf16()))) {}

        ~BSTRIn()
        {
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

        ~BSTROut()
        {
            if (bstr) {
                str = QString::fromUtf16 (bstr);
                SysFreeString (bstr);
            }
        }

        operator BSTR *() { return &bstr; }

    private:

        QString &str;
        BSTR bstr;
    };

    /**
     * Adapter to pass K* enums as output COM enum params (*_T).
     *
     * @param QE    K* enum.
     * @param CE    COM enum.
     */
    template <typename QE, typename CE>
    class ENUMOut
    {
    public:

        ENUMOut (QE &e) : qe (e), ce ((CE) 0) {}
        ~ENUMOut() { qe = (QE) ce; }
        operator CE *() { return &ce; }

    private:

        QE &qe;
        CE ce;
    };

#if !defined (VBOX_WITH_XPCOM)

    /** Adapter to pass QUuid as input GUID params */
    static GUID GUIDIn (const QUuid &uuid) { return uuid; }

    /** Adapter to pass QUuid as output GUID params */
    class GUIDOut
    {
    public:

        GUIDOut (QUuid &id) : uuid (id)
        {
            ::memset (&guid, 0, sizeof (GUID));
        }

        ~GUIDOut()
        {
            uuid = QUuid (
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        }

        operator GUID *() { return &guid; }

    private:

        QUuid &uuid;
        GUID guid;
    };

#else /* !defined (VBOX_WITH_XPCOM) */

    /** Adapter to pass QUuid as input GUID params */
    static const nsID &GUIDIn (const QUuid &uuid)
    {
        return *(const nsID *) &uuid;
    }

    /** Adapter to pass QUuid as output GUID params */
    class GUIDOut
    {
    public:

        GUIDOut (QUuid &id) : uuid (id), nsid (0) {}

        ~GUIDOut()
        {
            if (nsid)
            {
                uuid = QUuid (
                    nsid->m0, nsid->m1, nsid->m2,
                    nsid->m3[0], nsid->m3[1], nsid->m3[2], nsid->m3[3],
                    nsid->m3[4], nsid->m3[5], nsid->m3[6], nsid->m3[7]);
                nsMemory::Free (nsid);
            }
        }

        operator nsID **() { return &nsid; }

    private:

        QUuid &uuid;
        nsID *nsid;
    };

#endif /* !defined (VBOX_WITH_XPCOM) */

    static void addref (IUnknown *aIface) { if (aIface) aIface->AddRef(); }
    static void release (IUnknown *aIface) { if (aIface) aIface->Release(); }

protected:

    mutable HRESULT mRC;

    friend class COMErrorInfo;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Alternative base class for the CInterface template that adds the errorInfo()
 * method for providing extended error info about unsuccessful invocation of the
 * last called interface method.
 */
class COMBaseWithEI : public COMBase
{
public:

    /**
     * Returns error info set by the last unsuccessfully invoked interface
     * method. Returned error info is useful only if CInterface::lastRC()
     * represents a failure or a warning (i.e. CInterface::isReallyOk() is
     * false).
     */
    const COMErrorInfo &errorInfo() const { return mErrInfo; }

protected:

    /* no arbitrary instance creation */
    COMBaseWithEI() : COMBase () {};

    void setErrorInfo (const COMErrorInfo &aErrInfo) { mErrInfo = aErrInfo; }

    void fetchErrorInfo (IUnknown *aCallee, const GUID *aCalleeIID) const
    {
        mErrInfo.fetchFromCurrentThread (aCallee, aCalleeIID);
    }

    mutable COMErrorInfo mErrInfo;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Simple class that encapsulates the result code and COMErrorInfo.
 */
class COMResult
{
public:

    COMResult() : mRC (S_OK) {}

    /**
     * Queries the current result code from the given component.
     */
    explicit COMResult (const COMBase &aComponent)
        : mRC (aComponent.lastRC()) {}

    /**
     * Queries the current result code and error info from the given component.
     */
    COMResult (const COMBaseWithEI &aComponent)
        : mRC (aComponent.lastRC())
        , mErrInfo (aComponent.errorInfo()) {}

    /**
     * Queries the current result code from the given component.
     */
    COMResult &operator= (const COMBase &aComponent)
    {
        mRC = aComponent.lastRC();
        return *this;
    }

    /**
     * Queries the current result code and error info from the given component.
     */
    COMResult &operator= (const COMBaseWithEI &aComponent)
    {
        mRC = aComponent.lastRC();
        mErrInfo = aComponent.errorInfo();
        return *this;
    }

    bool isNull() const { return mErrInfo.isNull(); }

    /**
     * Returns @c true if the result code represents success (with or without
     * warnings).
     */
    bool isOk() const { return SUCCEEDED (mRC); }

    /**
     * Returns @c true if the result code represents success with one or more
     * warnings.
     */
    bool isWarning() const { return SUCCEEDED_WARNING (mRC); }

    /**
     * Returns @c true if the result code represents success with no warnings.
     */
    bool isReallyOk() const { return mRC == S_OK; }

    COMErrorInfo errorInfo() const { return mErrInfo; }
    HRESULT rc() const { return mRC; }

private:

    HRESULT mRC;
    COMErrorInfo mErrInfo;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Wrapper template class for all interfaces.
 *
 * All interface methods named as they are in the original, i.e. starting
 * with the capital letter. All utility non-interface methods are named
 * starting with the small letter. Utility methods should be not normally
 * called by the end-user client application.
 *
 * @param  I    Interface class (i.e. IUnknown/nsISupports derivant).
 * @param  B    Base class, either COMBase (by default) or COMBaseWithEI.
 */
template <class I, class B = COMBase>
class CInterface : public B
{
public:

    typedef B Base;
    typedef I Iface;

    // constructors & destructor

    CInterface() : mIface (NULL) {}

    CInterface (const CInterface &that) : B (that), mIface (that.mIface)
    {
        addref (mIface);
    }

    CInterface (I *aIface) : mIface (aIface) { addref (mIface); }

    virtual ~CInterface() { release (mIface); }

    // utility methods

    void createInstance (const CLSID &aClsId)
    {
        AssertMsg (!mIface, ("Instance is already non-NULL\n"));
        if (!mIface)
        {
#if !defined (VBOX_WITH_XPCOM)

            B::mRC = CoCreateInstance (aClsId, NULL, CLSCTX_ALL,
                                       _ATL_IIDOF (I), (void **) &mIface);

#else /* !defined (VBOX_WITH_XPCOM) */

            nsCOMPtr <nsIComponentManager> manager;
            B::mRC = NS_GetComponentManager (getter_AddRefs (manager));
            if (SUCCEEDED (B::mRC))
                B::mRC = manager->CreateInstance (aClsId, nsnull, NS_GET_IID (I),
                                                  (void **) &mIface);

#endif /* !defined (VBOX_WITH_XPCOM) */

            /* fetch error info, but don't assert if it's missing -- many other
             * reasons can lead to an error (w/o providing error info), not only
             * the instance initialization code (that should always provide it) */
            B::fetchErrorInfo (NULL, NULL);
        }
    }

    /**
     * Attaches to the given foreign interface pointer by querying the own
     * interface on it. The operation may fail.
     */
    template <class OI>
    void attach (OI *aIface)
    {
        /* be aware of self assignment */
        addref (aIface);
        release (mIface);
        if (aIface)
        {
            mIface = NULL;
#if !defined (VBOX_WITH_XPCOM)
            B::mRC = aIface->QueryInterface (_ATL_IIDOF (I), (void **) &mIface);
#else /* !defined (VBOX_WITH_XPCOM) */
            B::mRC = aIface->QueryInterface (NS_GET_IID (I), (void **) &mIface);
#endif /* !defined (VBOX_WITH_XPCOM) */
        }
        else
        {
            mIface = NULL;
            B::mRC = S_OK;
        }
    };

    /** Specialization of attach() for our own interface I. Never fails. */
    void attach (I *aIface)
    {
        /* be aware of self assignment */
        addref (aIface);
        release (mIface);
        mIface = aIface;
        B::mRC = S_OK;
    };

    /** Detaches from the underlying interface pointer. */
    void detach() { release (mIface); mIface = NULL; }

    /** Returns @c true if not attached to any interface pointer. */
    bool isNull() const { return mIface == NULL; }

    /**
     * Returns @c true if the result code represents success (with or without
     * warnings).
     */
    bool isOk() const { return !isNull() && SUCCEEDED (B::mRC); }

    /**
     * Returns @c true if the result code represents success with one or more
     * warnings.
     */
    bool isWarning() const { return !isNull() && SUCCEEDED_WARNING (B::mRC); }

    /**
     * Returns @c true if the result code represents success with no warnings.
     */
    bool isReallyOk() const { return !isNull() && B::mRC == S_OK; }

    // utility operators

    CInterface &operator= (const CInterface &that)
    {
        attach (that.mIface);
        B::operator= (that);
        return *this;
    }

    CInterface &operator= (I *aIface)
    {
        attach (aIface);
        return *this;
    }

    /**
     * Returns the raw interface pointer. Not intended to be used for anything
     * else but in generated wrappers and for debugging. You've been warned.
     */
    I *raw() const { return mIface; }

    bool operator== (const CInterface &that) const { return mIface == that.mIface; }
    bool operator!= (const CInterface &that) const { return mIface != that.mIface; }

protected:

    mutable I *mIface;
};

/////////////////////////////////////////////////////////////////////////////

class CUnknown : public CInterface <IUnknown, COMBaseWithEI>
{
public:

    typedef CInterface <IUnknown, COMBaseWithEI> Base;

    CUnknown() {}

    /** Creates an instance given another CInterface-based instance. */
    template <class OI, class OB>
    explicit CUnknown (const CInterface <OI, OB> &that)
    {
        attach (that.mIface);
        if (SUCCEEDED (mRC))
        {
            /* preserve old error info if any */
            mRC = that.lastRC();
            setErrorInfo (that.errorInfo());
        }
    }

    /** Constructor specialization for IUnknown. */
    CUnknown (const CUnknown &that) : Base (that) {}

    /** Creates an instance given a foreign interface pointer. */
    template <class OI>
    explicit CUnknown (OI *aIface)
    {
        attach (aIface);
    }

    /** Constructor specialization for IUnknown. */
    explicit CUnknown (IUnknown *aIface) : Base (aIface) {}

    /** Assigns from another CInterface-based instance. */
    template <class OI, class OB>
    CUnknown &operator= (const CInterface <OI, OB> &that)
    {
        attach (that.mIface);
        if (SUCCEEDED (mRC))
        {
            /* preserve old error info if any */
            mRC = that.lastRC();
            setErrorInfo (that.errorInfo());
        }
        return *this;
    }

    /** Assignment specialization for CUnknown. */
    CUnknown &operator= (const CUnknown &that)
    {
        Base::operator= (that);
        return *this;
    }

    /** Assigns from a foreign interface pointer. */
    template <class OI>
    CUnknown &operator= (OI *aIface)
    {
        attach (aIface);
        return *this;
    }

    /** Assignment specialization for IUnknown. */
    CUnknown &operator= (IUnknown *aIface)
    {
        Base::operator= (aIface);
        return *this;
    }

    /* @internal Used in generated wrappers. Never use directly. */
    IUnknown *&rawRef() { return mIface; };
};

/////////////////////////////////////////////////////////////////////////////

/* include the generated header containing concrete wrapper definitions */
#include "COMWrappers.h"

/** @} */

#endif // __COMDefs_h__
