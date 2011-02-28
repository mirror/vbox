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

#include "PciDeviceAttachmentImpl.h"
#include "AutoCaller.h"
#include "Global.h"
#include "Logging.h"

struct PciDeviceAttachment::Data
{
    Data(const Bstr    &aDevName,
         LONG          aHostAddress,
         LONG          aGuestAddress,
         BOOL          afPhysical)
        : HostAddress(aHostAddress), GuestAddress(aGuestAddress),
          fPhysical(afPhysical)
    {
        DevName = aDevName;
    }

    Bstr             DevName;
    LONG             HostAddress;
    LONG             GuestAddress;
    BOOL             fPhysical;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT PciDeviceAttachment::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void PciDeviceAttachment::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////
HRESULT PciDeviceAttachment::init(IMachine      *aParent,
                                  const Bstr   &aDevName,
                                  LONG          aHostAddress,
                                  LONG          aGuestAddress,
                                  BOOL          fPhysical)
{
    (void)aParent;
    m = new Data(aDevName, aHostAddress, aGuestAddress, fPhysical);

    return m != NULL ? S_OK : E_FAIL;
}

HRESULT PciDeviceAttachment::loadSettings(IMachine *aParent,
                                          const settings::HostPciDeviceAttachment &hpda)
{
    Bstr bname(hpda.strDeviceName);
    return init(aParent, bname,  hpda.uHostAddress, hpda.uGuestAddress, TRUE);
}


HRESULT PciDeviceAttachment::saveSettings(settings::HostPciDeviceAttachment &data)
{
    Assert(m);
    data.uHostAddress = m->HostAddress;
    data.uGuestAddress = m->GuestAddress;
    data.strDeviceName = m->DevName;

    return S_OK;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void PciDeviceAttachment::uninit()
{
    if (m)
    {
        delete m;
        m = NULL;
    }
}

// IPciDeviceAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP PciDeviceAttachment::COMGETTER(Name)(BSTR * aName)
{
    CheckComArgOutPointerValid(aName);
    m->DevName.cloneTo(aName);
    return S_OK;
}

STDMETHODIMP PciDeviceAttachment::COMGETTER(IsPhysicalDevice)(BOOL * aPhysical)
{
    CheckComArgOutPointerValid(aPhysical);
    *aPhysical = m->fPhysical;
    return S_OK;
}

STDMETHODIMP PciDeviceAttachment::COMGETTER(HostAddress)(LONG * aHostAddress)
{
    *aHostAddress = m->HostAddress;
    return S_OK;
}

STDMETHODIMP PciDeviceAttachment::COMGETTER(GuestAddress)(LONG * aGuestAddress)
{
    *aGuestAddress = m->GuestAddress;
    return S_OK;
}

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(PciDeviceAttachment)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(PciDeviceAttachment, IPciDeviceAttachment)
#endif
