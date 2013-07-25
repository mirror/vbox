/* $Id$ */

/** @file
 *
 * VBox USBDeviceFilters COM Class declaration.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_USBDEVICEFILTERSIMPL
#define ____H_USBDEVICEFILTERSIMPL

#include "VirtualBoxBase.h"

class HostUSBDevice;
class USBDeviceFilter;

namespace settings
{
    struct USB;
}

class ATL_NO_VTABLE USBDeviceFilters :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IUSBDeviceFilters)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(USBDeviceFilters, IUSBDeviceFilters)

    DECLARE_NOT_AGGREGATABLE(USBDeviceFilters)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(USBDeviceFilters)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IUSBDeviceFilters)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(USBDeviceFilters)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, USBDeviceFilters *aThat);
    HRESULT initCopy(Machine *aParent, USBDeviceFilters *aThat);
    void uninit();

    // IUSBDeviceFilters attributes
    STDMETHOD(COMGETTER(DeviceFilters))(ComSafeArrayOut(IUSBDeviceFilter *, aDevicesFilters));

    // IUSBDeviceFilters methods
    STDMETHOD(CreateDeviceFilter)(IN_BSTR aName, IUSBDeviceFilter **aFilter);
    STDMETHOD(InsertDeviceFilter)(ULONG aPosition, IUSBDeviceFilter *aFilter);
    STDMETHOD(RemoveDeviceFilter)(ULONG aPosition, IUSBDeviceFilter **aFilter);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::USB &data);
    HRESULT saveSettings(settings::USB &data);

    void rollback();
    void commit();
    void copyFrom(USBDeviceFilters *aThat);

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

#endif //!____H_USBDEVICEFILTERSIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
