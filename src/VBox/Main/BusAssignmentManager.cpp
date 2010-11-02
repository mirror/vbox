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
#include "BusAssignmentManager.h"

#include <iprt/asm.h>

#include <VBox/cfgm.h>

#include <map>
#include <vector>

struct BusAssignmentManager::State
{
    struct PciDeviceRecord
    {
        char szDevName[16];

        PciDeviceRecord(const char* pszName)
        {
            ::strncpy(szDevName, pszName, sizeof(szDevName));
        }

        bool operator<(const PciDeviceRecord &a) const
        {
            return ::strcmp(szDevName, a.szDevName) < 0;
        }

        bool operator==(const PciDeviceRecord &a) const
        {
            return ::strcmp(szDevName, a.szDevName) == 0;
        }
    };

    typedef std::map <PciBusAddress,PciDeviceRecord > PciMap;
    typedef std::vector<PciBusAddress>                PciAddrList;
    typedef std::map <PciDeviceRecord,PciAddrList >   ReversePciMap;

    volatile int32_t cRefCnt;
    ChipsetType_T    mChipsetType;
    PciMap           mPciMap;
    ReversePciMap    mReversePciMap;

    State()
        : cRefCnt(1), mChipsetType(ChipsetType_Null)
    {}
    ~State()
    {}

    HRESULT init(ChipsetType_T chipsetType);

    HRESULT record(const char* pszName, PciBusAddress& Address);
    HRESULT autoAssign(const char* pszName, PciBusAddress& Address);
    bool    checkAvailable(PciBusAddress& Address);
    bool    findPciAddress(const char* pszDevName, int iInstance, PciBusAddress& Address);
};

HRESULT BusAssignmentManager::State::init(ChipsetType_T chipsetType)
{
    mChipsetType = chipsetType;
    return S_OK;
}


HRESULT BusAssignmentManager::State::record(const char* pszName, PciBusAddress& Address)
{
    PciDeviceRecord devRec(pszName);

    /* Remember address -> device mapping */
    mPciMap.insert(PciMap::value_type(Address, devRec));

    ReversePciMap::iterator it = mReversePciMap.find(devRec);
    if (it == mReversePciMap.end())
    {
        mReversePciMap.insert(ReversePciMap::value_type(devRec, PciAddrList()));
        it = mReversePciMap.find(devRec);
    }

    /* Remember device name -> addresses mapping */
    it->second.push_back(Address);

    return S_OK;
}

bool    BusAssignmentManager::State::findPciAddress(const char* pszDevName, int iInstance, PciBusAddress& Address)
{
    PciDeviceRecord devRec(pszDevName);

    ReversePciMap::iterator it = mReversePciMap.find(devRec);
    if (it == mReversePciMap.end())
        return false;

    if (iInstance >= (int)it->second.size())
        return false;

    Address = it->second[iInstance];
    return true;
}

HRESULT BusAssignmentManager::State::autoAssign(const char* pszName, PciBusAddress& Address)
{
    // unimplemented yet
    Assert(false);
    return S_OK;
}

bool BusAssignmentManager::State::checkAvailable(PciBusAddress& Address)
{
    PciMap::const_iterator it = mPciMap.find(Address);

    return (it == mPciMap.end());
}

BusAssignmentManager::BusAssignmentManager()
    : pState(NULL)
{
    pState = new State();
    Assert(pState);
}

BusAssignmentManager::~BusAssignmentManager()
{
    if (pState)
    {
        delete pState;
        pState = NULL;
    }
}


BusAssignmentManager* BusAssignmentManager::pInstance = NULL;

BusAssignmentManager* BusAssignmentManager::getInstance(ChipsetType_T chipsetType)
{
    if (pInstance == NULL)
    {
        pInstance = new BusAssignmentManager();
        pInstance->pState->init(chipsetType);
        Assert(pInstance);
        return pInstance;
    }

    pInstance->AddRef();
    return pInstance;
}

void BusAssignmentManager::AddRef()
{
    ASMAtomicIncS32(&pState->cRefCnt);
}
void BusAssignmentManager::Release()
{
    if (ASMAtomicDecS32(&pState->cRefCnt) == 1)
        delete this;
}

DECLINLINE(HRESULT) InsertConfigInteger(PCFGMNODE pCfg,  const char* pszName, uint64_t u64)
{
    int vrc = CFGMR3InsertInteger(pCfg, pszName, u64);
    if (RT_FAILURE(vrc))
        return E_INVALIDARG;

    return S_OK;
}

HRESULT BusAssignmentManager::assignPciDevice(const char* pszDevName, PCFGMNODE pCfg,
                                              PciBusAddress& Address,    bool fAddressRequired)
{
    HRESULT rc = S_OK;

    if (!Address.valid())
        rc = pState->autoAssign(pszDevName, Address);
    else
    {
        bool fAvailable = pState->checkAvailable(Address);

        if (!fAvailable)
        {
            if (fAddressRequired)
                return E_ACCESSDENIED;
            else
                rc = pState->autoAssign(pszDevName, Address);
        }
    }

    if (FAILED(rc))
        return rc;

    Assert(Address.valid());

    rc = pState->record(pszDevName, Address);
    if (FAILED(rc))
        return rc;

    rc = InsertConfigInteger(pCfg, "PCIBusNo",             Address.iBus);
    if (FAILED(rc))
        return rc;
    rc = InsertConfigInteger(pCfg, "PCIDeviceNo",          Address.iDevice);
    if (FAILED(rc))
        return rc;
    rc = InsertConfigInteger(pCfg, "PCIFunctionNo",        Address.iFn);
    if (FAILED(rc))
        return rc;

    return S_OK;
}


bool BusAssignmentManager::findPciAddress(const char* pszDevName, int iInstance, PciBusAddress& Address)
{
    return pState->findPciAddress(pszDevName, iInstance, Address);
}
