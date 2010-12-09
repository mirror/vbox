/* $Id$ */

/** @file
 *
 * VirtualBox bus slots assignment manager
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
#ifndef __BusAssignmentManager_h
#define __BusAssignmentManager_h

#include "VBox/types.h"
#include "VBox/pci.h"
#include "VirtualBoxBase.h"

class BusAssignmentManager
{
private:
    struct State;
    State* pState;

    BusAssignmentManager();
    virtual ~BusAssignmentManager();

public:
    static BusAssignmentManager* createInstance(ChipsetType_T chipsetType);
    virtual void AddRef();
    virtual void Release();

    virtual HRESULT assignPciDevice(const char* pszDevName, PCFGMNODE pCfg, PciBusAddress& Address, bool fAddressRequired = false);
    virtual HRESULT assignPciDevice(const char* pszDevName, PCFGMNODE pCfg)
    {
        PciBusAddress Address;
        return assignPciDevice(pszDevName, pCfg, Address, false);
    }
    virtual bool findPciAddress(const char* pszDevName, int iInstance, PciBusAddress& Address);
    virtual bool hasPciDevice(const char* pszDevName, int iInstance)
    {
        PciBusAddress Address;
        return findPciAddress(pszDevName, iInstance, Address);
    }
    virtual void listAttachedPciDevices(ComSafeArrayOut(IPciDeviceAttachment*, aAttached));
};

#endif //  __BusAssignmentManager_h
