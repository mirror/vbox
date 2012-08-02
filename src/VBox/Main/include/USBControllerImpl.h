/* $Id$ */

/** @file
 *
 * VBox USBController COM Class declaration.
 */

/*
 * Copyright (C) 2005-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_USBCONTROLLERIMPL
#define ____H_USBCONTROLLERIMPL

#include "VirtualBoxBase.h"

class HostUSBDevice;
class USBDeviceFilter;

namespace settings
{
    struct USBController;
}

class ATL_NO_VTABLE USBController :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IUSBController)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(USBController, IUSBController)

    DECLARE_NOT_AGGREGATABLE(USBController)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(USBController)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IUSBController)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(USBController)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, USBController *aThat);
    HRESULT initCopy(Machine *aParent, USBController *aThat);
    void uninit();

    // IUSBController properties
    STDMETHOD(COMGETTER(Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL aEnabled);
    STDMETHOD(COMGETTER(EnabledEHCI))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(EnabledEHCI))(BOOL aEnabled);
    STDMETHOD(COMGETTER(ProxyAvailable))(BOOL *aEnabled);
    STDMETHOD(COMGETTER(USBStandard))(USHORT *aUSBStandard);
    STDMETHOD(COMGETTER(DeviceFilters))(ComSafeArrayOut(IUSBDeviceFilter *, aDevicesFilters));

    // IUSBController methods
    STDMETHOD(CreateDeviceFilter)(IN_BSTR aName, IUSBDeviceFilter **aFilter);
    STDMETHOD(InsertDeviceFilter)(ULONG aPosition, IUSBDeviceFilter *aFilter);
    STDMETHOD(RemoveDeviceFilter)(ULONG aPosition, IUSBDeviceFilter **aFilter);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::USBController &data);
    HRESULT saveSettings(settings::USBController &data);

    void rollback();
    void commit();
    void copyFrom(USBController *aThat);

#ifdef VBOX_WITH_USB
    HRESULT onDeviceFilterChange(USBDeviceFilter *aFilter,
                                 BOOL aActiveChanged = FALSE);

    bool hasMatchingFilter(const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs);
    bool hasMatchingFilter(IUSBDevice *aUSBDevice, ULONG *aMaskedIfs);

    HRESULT notifyProxy(bool aInsertFilters);
#endif /* VBOX_WITH_USB */

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)
    Machine* getMachine();

private:

    void printList();

    struct Data;
    Data *m;
};

#endif //!____H_USBCONTROLLERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
