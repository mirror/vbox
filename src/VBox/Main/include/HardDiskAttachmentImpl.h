/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_HARDDISKATTACHMENTIMPL
#define ____H_HARDDISKATTACHMENTIMPL

#include "VirtualBoxBase.h"

#include "HardDiskImpl.h"

class ATL_NO_VTABLE HardDiskAttachment :
    public VirtualBoxBaseNEXT,
    public com::SupportErrorInfoImpl<HardDiskAttachment, IHardDiskAttachment>,
    public VirtualBoxSupportTranslation<HardDiskAttachment>,
    VBOX_SCRIPTABLE_IMPL(IHardDiskAttachment)
{
public:

    /** Equality predicate for stdc++. */
    struct EqualsTo
        : public std::unary_function <ComObjPtr<HardDiskAttachment>, bool>
    {
        explicit EqualsTo (CBSTR aController, LONG aPort, LONG aDevice)
            : controller(aController), port (aPort), device (aDevice) {}

        bool operator() (const argument_type &aThat) const
        {
            return aThat->controller() == controller && aThat->port() == port &&
                   aThat->device() == device;
        }

        const Bstr controller;
        const LONG port;
        const LONG device;
    };

    /** Hard disk reference predicate for stdc++. */
    struct RefersTo
        : public std::unary_function< ComObjPtr<HardDiskAttachment>, bool>
    {
        explicit RefersTo (HardDisk *aHardDisk) : hardDisk (aHardDisk) {}

        bool operator() (const argument_type &aThat) const
        {
            return aThat->hardDisk().equalsTo (hardDisk);
        }

        const ComObjPtr <HardDisk> hardDisk;
    };

    DECLARE_NOT_AGGREGATABLE(HardDiskAttachment)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (HardDiskAttachment)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDiskAttachment)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(HardDisk *aHD, CBSTR aController, LONG aPort,
                 LONG aDevice, bool aImplicit = false);
    void uninit();

    // IHardDiskAttachment properties
    STDMETHOD(COMGETTER(HardDisk))   (IHardDisk **aHardDisk);
    STDMETHOD(COMGETTER(Controller)) (BSTR *aController);
    STDMETHOD(COMGETTER(Port))       (LONG *aPort);
    STDMETHOD(COMGETTER(Device))     (LONG *aDevice);

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    bool isImplicit() const { return m.implicit; }
    void setImplicit (bool aImplicit) { m.implicit = aImplicit; }

    const ComObjPtr<HardDisk> &hardDisk() const { return m.hardDisk; }
    Bstr controller() const { return m.controller; }
    LONG port()   const { return m.port; }
    LONG device() const { return m.device; }

    /** Must be called from under this object's write lock.  */
    void updateHardDisk (const ComObjPtr<HardDisk> &aHardDisk, bool aImplicit)
    {
        m.hardDisk = aHardDisk;
        m.implicit = aImplicit;
    }

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "HardDiskAttachment"; }

private:

    struct Data
    {
        Data() : port (0), device (0)
               , implicit (false) {}

        /// @todo NEWMEDIA shouldn't it be constant too? It'd be nice to get
        /// rid of locks at all in this simple readonly structure-like interface
        ComObjPtr<HardDisk> hardDisk;
        const Bstr controller;
        const LONG port;
        const LONG device;

        bool implicit : 1;
    };

    Data m;
};

#endif // ____H_HARDDISKATTACHMENTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
