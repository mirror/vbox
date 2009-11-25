/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_MEDIUMATTACHMENTIMPL
#define ____H_MEDIUMATTACHMENTIMPL

#include "VirtualBoxBase.h"

#include "MediumImpl.h"

class Machine;
class Medium;

class ATL_NO_VTABLE MediumAttachment :
    public VirtualBoxBase,
    public com::SupportErrorInfoImpl<MediumAttachment, IMediumAttachment>,
    public VirtualBoxSupportTranslation<MediumAttachment>,
    VBOX_SCRIPTABLE_IMPL(IMediumAttachment)
{
public:

    DECLARE_NOT_AGGREGATABLE(MediumAttachment)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(MediumAttachment)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IMediumAttachment)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(MediumAttachment)

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent,
                 Medium *aMedium,
                 const Bstr &aControllerName,
                 LONG aPort,
                 LONG aDevice,
                 DeviceType_T aType,
                 bool aImplicit = false);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    bool rollback();
    void commit();

    // IMediumAttachment properties
    STDMETHOD(COMGETTER(Medium))(IMedium **aMedium);
    STDMETHOD(COMGETTER(Controller))(BSTR *aController);
    STDMETHOD(COMGETTER(Port))(LONG *aPort);
    STDMETHOD(COMGETTER(Device))(LONG *aDevice);
    STDMETHOD(COMGETTER(Type))(DeviceType_T *aType);
    STDMETHOD(COMGETTER(Passthrough))(BOOL *aPassthrough);

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    bool isImplicit() const { return m->implicit; }
    void setImplicit(bool aImplicit) { m->implicit = aImplicit; }

    const ComObjPtr<Medium> &medium() const { return m->medium; }
    Bstr controllerName() const { return m->controllerName; }
    LONG port() const { return m->port; }
    LONG device() const { return m->device; }
    DeviceType_T type() const { return m->type; }
    bool passthrough() const { AutoReadLock lock(this); return m->passthrough; }

    bool matches(CBSTR aControllerName, LONG aPort, LONG aDevice)
    {
        return (    aControllerName == m->controllerName
                 && aPort == m->port
                 && aDevice == m->device);
    }

    /** Must be called from under this object's write lock. */
    void updateMedium(const ComObjPtr<Medium> &aMedium, bool aImplicit)
    {
        m.backup();
        m->medium = aMedium;
        m->implicit = aImplicit;
    }

    /** Must be called from under this object's write lock. */
    void updatePassthrough(bool aPassthrough)
    {
        m.backup();
        m->passthrough = aPassthrough;
    }

    /** Get a unique and somewhat descriptive name for logging. */
    const char *logName(void) const { return mLogName.c_str(); }

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "MediumAttachment"; }

private:

    /** Reference to Machine object, for checking mutable state. */
    const ComObjPtr<Machine, ComWeakRef> mParent;
    /* later: const ComObjPtr<MediumAttachment> mPeer; */

    struct Data
    {
        Data() : port(0), device(0), type(DeviceType_Null),
                 passthrough(false), implicit(false) {}

        bool operator== (const Data &that) const
        {
            return   this == &that
                   || (passthrough == that.passthrough);
        }

        ComObjPtr<Medium> medium;
        /* Since MediumAttachment is not a first class citizen when it
         * comes to managing settings, having a reference to the storage
         * controller will not work - when settings are changed it will point
         * to the old, uninitialized instance. Changing this requires
         * substantial changes to MediumImpl.cpp. */
        const Bstr controllerName;
        const LONG port;
        const LONG device;
        const DeviceType_T type;
        bool passthrough : 1;
        bool implicit : 1;
    };

    Backupable<Data> m;

    Utf8Str mLogName;                   /**< For logging purposes */
};

#endif // ____H_MEDIUMATTACHMENTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
