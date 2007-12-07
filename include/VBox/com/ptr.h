/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Smart COM pointer classes declaration
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

#ifndef ___VBox_com_ptr_h
#define ___VBox_com_ptr_h

#if !defined (VBOX_WITH_XPCOM)

#include <atlbase.h>

#ifndef _ATL_IIDOF
# define _ATL_IIDOF(c) __uuidof(c)
#endif 

#else /* !defined (VBOX_WITH_XPCOM) */

#include <nsXPCOM.h>
#include <nsIComponentManager.h>
#include <nsCOMPtr.h>
#include <ipcIService.h>
#include <nsIServiceManagerUtils.h>
#include <ipcCID.h>
#include <ipcIDConnectService.h>

// official XPCOM headers don't define it yet
#define IPC_DCONNECTSERVICE_CONTRACTID \
    "@mozilla.org/ipc/dconnect-service;1"

#endif /* !defined (VBOX_WITH_XPCOM) */

#include <VBox/com/defs.h>
#include <VBox/com/assert.h>

/**
 *  Strong referencing operators. Used as a second argument to ComPtr<>/ComObjPtr<>.
 */
template <class C>
class ComStrongRef
{
protected:
    static void addref (C *p) { p->AddRef(); }
    static void release (C *p) { p->Release(); }
};

/**
 *  Weak referencing operators. Used as a second argument to ComPtr<>/ComObjPtr<>.
 */
template <class C>
class ComWeakRef
{
protected:
    static void addref (C *p) {}
    static void release (C *p) {}
};

/**
 *  Base template for smart COM pointers. Not intended to be used directly.
 */
template <class C, template <class> class RefOps = ComStrongRef>
class ComPtrBase : protected RefOps <C>
{
public:

    // a special template to disable AddRef()/Release()
    template <class I>
    class NoAddRefRelease : public I {
        private:
#if !defined (VBOX_WITH_XPCOM)
            STDMETHOD_(ULONG, AddRef)() = 0;
            STDMETHOD_(ULONG, Release)() = 0;
#else /* !defined (VBOX_WITH_XPCOM) */
            NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
            NS_IMETHOD_(nsrefcnt) Release(void) = 0;
#endif /* !defined (VBOX_WITH_XPCOM) */
    };

protected:

    ComPtrBase () : p (NULL) {}
    ComPtrBase (const ComPtrBase &that) : p (that.p) { addref(); }
    ComPtrBase (C *that_p) : p (that_p) { addref(); }

    ~ComPtrBase() { release(); }

    ComPtrBase &operator= (const ComPtrBase &that) {
        safe_assign (that.p);
        return *this;
    }
    ComPtrBase &operator= (C *that_p) {
        safe_assign (that_p);
        return *this;
    }

public:

    void setNull() {
        release();
        p = NULL;
    }

    bool isNull() const {
        return (p == NULL);
    }
    bool operator! () const { return isNull(); }

    bool operator< (C* that_p) const { return p < that_p; }
    bool operator== (C* that_p) const { return p == that_p; }

    template <class I>
    bool equalsTo (I *i) const {
        IUnknown *this_unk = NULL, *that_unk = NULL;
        if (i)
            i->QueryInterface (COM_IIDOF (IUnknown), (void**) &that_unk);
        if (p)
            p->QueryInterface (COM_IIDOF (IUnknown), (void**) &this_unk);
        bool equal = this_unk == that_unk;
        if (that_unk)
            that_unk->Release();
        if (this_unk)
            this_unk->Release();
        return equal;
    }

    template <class OC>
    bool equalsTo (const ComPtrBase <OC> &oc) const {
        return equalsTo ((OC *) oc);
    }

    /** Intended to pass instances as in parameters to interface methods */
    operator C* () const { return p; }

    /**
     *  Derefereces the instance (redirects the -> operator to the managed
     *  pointer).
     */
    NoAddRefRelease <C> *operator-> () const {
        AssertMsg (p, ("Managed pointer must not be null\n"));
        return (NoAddRefRelease <C> *) p;
    }

    template <class I>
    HRESULT queryInterfaceTo (I **pp) const {
        if (pp) {
            if (p) {
                return p->QueryInterface (COM_IIDOF (I), (void**) pp);
            } else {
                *pp = NULL;
                return S_OK;
            }
        } else {
            return E_INVALIDARG;
        }
    }

    /** Intended to pass instances as out parameters to interface methods */
    C **asOutParam() {
        setNull();
        return &p;
    }

private:

    void addref() {
        if (p)
            RefOps <C>::addref (p);
    }
    void release() {
        if (p)
            RefOps <C>::release (p);
    }

    void safe_assign (C *that_p) {
        // be aware of self-assignment
        if (that_p)
            RefOps <C>::addref (that_p);
        release();
        p = that_p;
    }

    C *p;
};

/**
 *  Smart COM pointer wrapper that automatically manages refcounting of
 *  interface pointers.
 *
 *  @param I    COM interface class
 */
template <class I, template <class> class RefOps = ComStrongRef>
class ComPtr : public ComPtrBase <I, RefOps>
{
    typedef ComPtrBase <I, RefOps> Base;

public:

    ComPtr () : Base() {}
    ComPtr (const ComPtr &that) : Base (that) {}
    ComPtr &operator= (const ComPtr &that) {
        Base::operator= (that);
        return *this;
    }

    template <class OI>
    ComPtr (OI *that_p) : Base () { operator= (that_p); }
    // specialization for I
    ComPtr (I *that_p) : Base (that_p) {}

    template <class OC>
    ComPtr (const ComPtr <OC, RefOps> &oc) : Base () { operator= ((OC *) oc); }

    template <class OI>
    ComPtr &operator= (OI *that_p) {
        if (that_p)
            that_p->QueryInterface (COM_IIDOF (I), (void **) Base::asOutParam());
        else
            Base::setNull();
        return *this;
    }
    // specialization for I
    ComPtr &operator= (I *that_p) {
        Base::operator= (that_p);
        return *this;
    }

    template <class OC>
    ComPtr &operator= (const ComPtr <OC, RefOps> &oc) {
        return operator= ((OC *) oc);
    }

    /**
     *  Createas an in-process object of the given class ID and starts to
     *  manage a reference to the created object in case of success.
     */
    HRESULT createInprocObject (const CLSID &clsid)
    {
        HRESULT rc;
        I *obj = NULL;
#if !defined (VBOX_WITH_XPCOM)
        rc = CoCreateInstance (clsid, NULL, CLSCTX_INPROC_SERVER, _ATL_IIDOF (I),
                               (void **) &obj);
#else /* !defined (VBOX_WITH_XPCOM) */
        nsCOMPtr <nsIComponentManager> manager;
        rc = NS_GetComponentManager (getter_AddRefs (manager));
        if (SUCCEEDED (rc))
            rc = manager->CreateInstance (clsid, nsnull, NS_GET_IID (I),
                                          (void **) &obj);
#endif /* !defined (VBOX_WITH_XPCOM) */
        *this = obj;
        if (SUCCEEDED (rc))
            obj->Release();
        return rc;
    }

    /**
     *  Createas a local (out-of-process) object of the given class ID and starts
     *  to manage a reference to the created object in case of success.
     *
     *  Note: In XPCOM, the out-of-process functionality is currently emulated
     *  through in-process wrapper objects (that start a dedicated process and
     *  redirect all object requests to that process). For this reason, this
     *  method is fully equivalent to #createInprocObject() for now.
     */
    HRESULT createLocalObject (const CLSID &clsid)
    {
#if !defined (VBOX_WITH_XPCOM)
        HRESULT rc;
        I *obj = NULL;
        rc = CoCreateInstance (clsid, NULL, CLSCTX_LOCAL_SERVER, _ATL_IIDOF (I),
                               (void **) &obj);
        *this = obj;
        if (SUCCEEDED (rc))
            obj->Release();
        return rc;
#else /* !defined (VBOX_WITH_XPCOM) */
        return createInprocObject (clsid);
#endif /* !defined (VBOX_WITH_XPCOM) */
    }

#ifdef VBOX_WITH_XPCOM
    /**
     *  Createas an object of the given class ID on the specified server and
     *  starts to manage a reference to the created object in case of success.
     *
     *  @param serverName   Name of the server to create an object within.
     */
    HRESULT createObjectOnServer (const CLSID &clsid, const char *serverName)
    {
        HRESULT rc;
        I *obj = NULL;
        nsCOMPtr <ipcIService> ipcServ = do_GetService (IPC_SERVICE_CONTRACTID, &rc);
        if (SUCCEEDED (rc))
        {
            PRUint32 serverID = 0;
            rc = ipcServ->ResolveClientName (serverName, &serverID);
            if (SUCCEEDED (rc)) {
                nsCOMPtr <ipcIDConnectService> dconServ =
                    do_GetService (IPC_DCONNECTSERVICE_CONTRACTID, &rc);
                if (SUCCEEDED (rc))
                    rc = dconServ->CreateInstance (serverID, clsid, NS_GET_IID (I),
                                                   (void **) &obj);
            }
        }
        *this = obj;
        if (SUCCEEDED (rc))
            obj->Release();
        return rc;
    }
#endif
};

/**
 *  Specialization of ComPtr<> for IUnknown to guarantee identity
 *  by always doing QueryInterface() when constructing or assigning from
 *  another interface pointer disregarding its type.
 */
template <template <class> class RefOps>
class ComPtr <IUnknown, RefOps> : public ComPtrBase <IUnknown, RefOps>
{
    typedef ComPtrBase <IUnknown, RefOps> Base;

public:

    ComPtr () : Base() {}
    ComPtr (const ComPtr &that) : Base (that) {}
    ComPtr &operator= (const ComPtr &that) {
        Base::operator= (that);
        return *this;
    }

    template <class OI>
    ComPtr (OI *that_p) : Base () { operator= (that_p); }

    template <class OC>
    ComPtr (const ComPtr <OC, RefOps> &oc) : Base () { operator= ((OC *) oc); }

    template <class OI>
    ComPtr &operator= (OI *that_p) {
        if (that_p)
            that_p->QueryInterface (COM_IIDOF (IUnknown), (void **) Base::asOutParam());
        else
            Base::setNull();
        return *this;
    }

    template <class OC>
    ComPtr &operator= (const ComPtr <OC, RefOps> &oc) {
        return operator= ((OC *) oc);
    }
};

/**
 *  Smart COM pointer wrapper that automatically manages refcounting of
 *  pointers to interface implementation classes created on the component's
 *  (i.e. the server's) side. Differs from ComPtr by providing additional
 *  platform independent operations for creating new class instances.
 *
 *  @param C    class that implements some COM interface
 */
template <class C, template <class> class RefOps = ComStrongRef>
class ComObjPtr : public ComPtrBase <C, RefOps>
{
    typedef ComPtrBase <C, RefOps> Base;

public:

    ComObjPtr () : Base() {}
    ComObjPtr (const ComObjPtr &that) : Base (that) {}
    ComObjPtr (C *that_p) : Base (that_p) {}
    ComObjPtr &operator= (const ComObjPtr &that) {
        Base::operator= (that);
        return *this;
    }
    ComObjPtr &operator= (C *that_p) {
        Base::operator= (that_p);
        return *this;
    }

    /**
     *  Creates a new server-side object of the given component class and
     *  immediately starts to manage a pointer to the created object (the
     *  previous pointer, if any, is of course released when appropriate).
     *
     *  @note This method should be used with care on weakly referenced
     *  smart pointers because it leaves the newly created object completely
     *  unreferenced (i.e., with reference count equal to zero),
     *
     *  @note Win32: when VBOX_COM_OUTOFPROC_MODULE is defined, the created
     *  object doesn't increase the lock count of the server module, as it
     *  does otherwise.
     */
    HRESULT createObject() {
        HRESULT rc;
#if !defined (VBOX_WITH_XPCOM)
#   ifdef VBOX_COM_OUTOFPROC_MODULE
        CComObjectNoLock <C> *obj = new CComObjectNoLock <C>();
        if (obj) {
            obj->InternalFinalConstructAddRef();
            rc = obj->FinalConstruct();
            obj->InternalFinalConstructRelease();
        } else {
            rc = E_OUTOFMEMORY;
        }
#   else
        CComObject <C> *obj = NULL;
        rc = CComObject <C>::CreateInstance (&obj);
#   endif
#else /* !defined (VBOX_WITH_XPCOM) */
        CComObject <C> *obj = new CComObject <C>();
        if (obj) {
            rc = obj->FinalConstruct();
        } else {
            rc = E_OUTOFMEMORY;
        }
#endif /* !defined (VBOX_WITH_XPCOM) */
        *this = obj;
        return rc;
    }
};

#endif
