/* $Id$ */

/** @file
 *
 * PCI attachment information implmentation.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

class ATL_NO_VTABLE PciAddress :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IPciAddress)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(PciAddress, IPciAddress)

    DECLARE_NOT_AGGREGATABLE(PciAddress)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PciAddress)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IPciAddress)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    PciAddress() { }
    ~PciAddress() { }

    // public initializer/uninitializer for internal purposes only
    HRESULT init(LONG aAddess);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    // IPciAddress properties
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

class ATL_NO_VTABLE PciDeviceAttachment :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IPciDeviceAttachment)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(PciDeviceAttachment, IPciDeviceAttachment)

    DECLARE_NOT_AGGREGATABLE(PciDeviceAttachment)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PciDeviceAttachment)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IPciDeviceAttachment)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    PciDeviceAttachment() { }
    ~PciDeviceAttachment() { }

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *     aParent,
                 const Bstr    &aName,
                 LONG          aHostAddess,
                 LONG          aGuestAddress,
                 BOOL          fPhysical);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    // IPciDeviceAttachment properties
    STDMETHOD(COMGETTER(Name))(BSTR * aName);
    STDMETHOD(COMGETTER(IsPhysicalDevice))(BOOL * aPhysical);
    STDMETHOD(COMGETTER(HostAddress))(LONG  * hostAddress);
    STDMETHOD(COMGETTER(GuestAddress))(LONG * guestAddress);

private:
    struct Data;
    Data *m;
};

#endif // ____H_PCIDEVICEATTACHMENTIMPL
