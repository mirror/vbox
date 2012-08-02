/* $Id$ */

/** @file
 *
 * PCI attachment information implmentation.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_PCIDEVICEATTACHMENTIMPL
#define ____H_PCIDEVICEATTACHMENTIMPL

#include "VirtualBoxBase.h"
#include <VBox/settings.h>

class ATL_NO_VTABLE PCIAddress :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IPCIAddress)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(PCIAddress, IPCIAddress)

    DECLARE_NOT_AGGREGATABLE(PCIAddress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PCIAddress)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IPCIAddress)
    END_COM_MAP()

    PCIAddress() { }
    ~PCIAddress() { }

    // public initializer/uninitializer for internal purposes only
    HRESULT init(LONG aAddess);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    // IPCIAddress properties
    STDMETHOD(COMGETTER(Bus))(SHORT *aBus)
    {
        *aBus = mBus;
        return S_OK;
    }
    STDMETHOD(COMSETTER(Bus))(SHORT aBus)
    {
        mBus = aBus;
        return S_OK;
    }
    STDMETHOD(COMGETTER(Device))(SHORT *aDevice)
    {
        *aDevice = mDevice;
        return S_OK;
    }
    STDMETHOD(COMSETTER(Device))(SHORT aDevice)
    {
        mDevice = aDevice;
        return S_OK;
    }

    STDMETHOD(COMGETTER(DevFunction))(SHORT *aDevFunction)
    {
        *aDevFunction = mFn;
        return S_OK;
    }
    STDMETHOD(COMSETTER(DevFunction))(SHORT aDevFunction)
    {
        mFn = aDevFunction;
        return S_OK;
    }

private:
    SHORT mBus, mDevice, mFn;
};

class ATL_NO_VTABLE PCIDeviceAttachment :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IPCIDeviceAttachment)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(PCIDeviceAttachment, IPCIDeviceAttachment)

    DECLARE_NOT_AGGREGATABLE(PCIDeviceAttachment)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PCIDeviceAttachment)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IPCIDeviceAttachment)
    END_COM_MAP()

    PCIDeviceAttachment() { }
    ~PCIDeviceAttachment() { }

    // public initializer/uninitializer for internal purposes only
    HRESULT init(IMachine *    aParent,
                 const Bstr    &aName,
                 LONG          aHostAddess,
                 LONG          aGuestAddress,
                 BOOL          fPhysical);

    void uninit();

    // settings
    HRESULT loadSettings(IMachine * aParent,
                         const settings::HostPCIDeviceAttachment& aHpda);
    HRESULT saveSettings(settings::HostPCIDeviceAttachment &data);

    HRESULT FinalConstruct();
    void FinalRelease();

    // IPCIDeviceAttachment properties
    STDMETHOD(COMGETTER(Name))(BSTR * aName);
    STDMETHOD(COMGETTER(IsPhysicalDevice))(BOOL * aPhysical);
    STDMETHOD(COMGETTER(HostAddress))(LONG  * hostAddress);
    STDMETHOD(COMGETTER(GuestAddress))(LONG * guestAddress);

private:
    struct Data;
    Data*  m;
};

#endif // ____H_PCIDEVICEATTACHMENTIMPL
