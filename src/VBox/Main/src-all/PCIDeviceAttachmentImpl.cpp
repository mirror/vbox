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

#include "PCIDeviceAttachmentImpl.h"
#include "AutoCaller.h"
#include "Global.h"
#include "Logging.h"

struct PCIDeviceAttachment::Data
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
DEFINE_EMPTY_CTOR_DTOR(PCIDeviceAttachment)

HRESULT PCIDeviceAttachment::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void PCIDeviceAttachment::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////
HRESULT PCIDeviceAttachment::init(IMachine      *aParent,
                                  const Bstr   &aDevName,
                                  LONG          aHostAddress,
                                  LONG          aGuestAddress,
                                  BOOL          fPhysical)
{
    (void)aParent;
    m = new Data(aDevName, aHostAddress, aGuestAddress, fPhysical);

    return m != NULL ? S_OK : E_FAIL;
}

HRESULT PCIDeviceAttachment::i_loadSettings(IMachine *aParent,
                                            const settings::HostPCIDeviceAttachment &hpda)
{
    Bstr bname(hpda.strDeviceName);
    return init(aParent, bname,  hpda.uHostAddress, hpda.uGuestAddress, TRUE);
}


HRESULT PCIDeviceAttachment::i_saveSettings(settings::HostPCIDeviceAttachment &data)
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
void PCIDeviceAttachment::uninit()
{
    if (m)
    {
        delete m;
        m = NULL;
    }
}

// IPCIDeviceAttachment properties
/////////////////////////////////////////////////////////////////////////////
HRESULT PCIDeviceAttachment::getName(com::Utf8Str &aName)
{
    aName = m->DevName;
    return S_OK;
}

HRESULT PCIDeviceAttachment::getIsPhysicalDevice(BOOL *aIsPhysicalDevice)
{
    *aIsPhysicalDevice = m->fPhysical;
    return S_OK;
}

HRESULT PCIDeviceAttachment::getHostAddress(LONG *aHostAddress)
{
    *aHostAddress = m->HostAddress;
    return S_OK;
}
HRESULT PCIDeviceAttachment::getGuestAddress(LONG *aGuestAddress)
{
    *aGuestAddress = m->GuestAddress;
    return S_OK;
}
