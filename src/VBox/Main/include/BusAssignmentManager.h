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
#include "VirtualBoxBase.h"

struct PciBusAddress
{
    int  iBus;
    int  iDevice;
    int  iFn;

    PciBusAddress()
    {
        clear();
    }

    PciBusAddress(int bus, int device, int fn)
        : iBus(bus), iDevice(device), iFn(fn)
    {
    }

    PciBusAddress& clear()
    {
        iBus = iDevice = iFn = -1;
        return *this;
    }

    bool operator<(const PciBusAddress &a) const
    {
        if (iBus < a.iBus)
            return true;
        
        if (iBus > a.iBus)
            return false;

        if (iDevice < a.iDevice)
            return true;
        
        if (iDevice > a.iDevice)
            return false;

        if (iFn < a.iFn)
            return true;
        
        if (iFn > a.iFn)
            return false;

        return false;
    }

    bool operator==(const PciBusAddress &a) const
    {
        return     (iBus == a.iBus)
                && (iDevice == a.iDevice)
                && (iFn == a.iFn);
    }

    bool operator!=(const PciBusAddress &a) const
    {
        return     (iBus != a.iBus)
                || (iDevice != a.iDevice)
                || (iFn  != a.iFn);
    }

    bool valid() const
    {
        return (iBus != -1) && (iDevice != -1) && (iFn != -1);
    }
};

class BusAssignmentManager
{
private:
    struct State;
    State* pState;

    BusAssignmentManager();
    virtual ~BusAssignmentManager();

    static BusAssignmentManager* pInstance;

public:
    static BusAssignmentManager* getInstance(ChipsetType_T chipsetType);
    virtual void AddRef();
    virtual void Release();

    virtual HRESULT assignPciDevice(const char* pszDevName, PCFGMNODE pCfg, PciBusAddress& Address, bool fAddressRequired = false);
    virtual HRESULT assignPciDevice(const char* pszDevName, PCFGMNODE pCfg)
    {
        PciBusAddress Address;
        return assignPciDevice(pszDevName, pCfg, Address, false);
    }
};

#endif //  __BusAssignmentManager_h
