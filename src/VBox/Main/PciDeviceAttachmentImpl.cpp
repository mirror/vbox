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
#include "Logging.h"

struct PciDeviceAttachment::Data
{
    Data()
        : pMachine(NULL)
    { }

    Machine * const pMachine;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT PciDeviceAttachment::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return S_OK;
}

void PciDeviceAttachment::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////
HRESULT PciDeviceAttachment::init(Machine      *aParent,
                                  const Bstr   &aDevName,
                                  LONG          aHostAddess,
                                  LONG          aGuestAddress,
                                  BOOL          fPhysical)
{
    m = new Data();
    //unconst(m->pMachine) = aParent;

    return S_OK;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void PciDeviceAttachment::uninit()
{    
    //unconst(m->pMachine) = NULL;

    delete m;
    m = NULL;
}

// IPciDeviceAttachment properties
/////////////////////////////////////////////////////////////////////////////

#if 0
STDMETHODIMP PciDeviceAttachment::COMGETTER(Name)(BSTR *)
{
    return E_NOTIMPL;
}

STDMETHODIMP PciDeviceAttachment::COMGETTER(PhysicalDevice)(BOOL *)
{
    return E_NOTIMPL;
}

STDMETHODIMP PciDeviceAttachment::COMGETTER(HostAddress)(LONG *)
{
    return E_NOTIMPL;
}

STDMETHODIMP PciDeviceAttachment::COMGETTER(GuestAddress)(LONG *)
{
    return E_NOTIMPL;
}

#endif
