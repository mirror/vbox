/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "NetworkServiceRunner.h"
#include "DHCPServerImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/cpp/utils.h>

#include <VBox/com/array.h>
#include <VBox/settings.h>

#include "VirtualBoxImpl.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DHCPServer::DHCPServer()
    : mVirtualBox(NULL)
{
}


DHCPServer::~DHCPServer()
{
}


HRESULT DHCPServer::FinalConstruct()
{
    return BaseFinalConstruct();
}


void DHCPServer::FinalRelease()
{
    uninit ();

    BaseFinalRelease();
}


void DHCPServer::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mVirtualBox) = NULL;
}


HRESULT DHCPServer::init(VirtualBox *aVirtualBox, IN_BSTR aName)
{
    AssertReturn(aName != NULL, E_INVALIDARG);

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;

    unconst(mName) = aName;
    m.IPAddress = "0.0.0.0";
    m.GlobalDhcpOptions.insert(DhcpOptValuePair(DhcpOpt_SubnetMask, Bstr("0.0.0.0")));
    m.enabled = FALSE;

    m.lowerIP = "0.0.0.0";
    m.upperIP = "0.0.0.0";

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


HRESULT DHCPServer::init(VirtualBox *aVirtualBox,
                         const settings::DHCPServer &data)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;

    unconst(mName) = data.strNetworkName;
    m.IPAddress = data.strIPAddress;
    m.enabled = data.fEnabled;
    m.lowerIP = data.strIPLower;
    m.upperIP = data.strIPUpper;

    m.GlobalDhcpOptions.clear();
    m.GlobalDhcpOptions.insert(data.GlobalDhcpOptions.begin(),
                               data.GlobalDhcpOptions.end());

    m.VmSlot2Options.clear();
    m.VmSlot2Options.insert(data.VmSlot2OptionsM.begin(),
                            data.VmSlot2OptionsM.end());

    autoInitSpan.setSucceeded();

    return S_OK;
}


HRESULT DHCPServer::saveSettings(settings::DHCPServer &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.strNetworkName = mName;
    data.strIPAddress = m.IPAddress;

    data.fEnabled = !!m.enabled;
    data.strIPLower = m.lowerIP;
    data.strIPUpper = m.upperIP;

    data.GlobalDhcpOptions.clear();
    data.GlobalDhcpOptions.insert(m.GlobalDhcpOptions.begin(),
                                  m.GlobalDhcpOptions.end());

    data.VmSlot2OptionsM.clear();
    data.VmSlot2OptionsM.insert(m.VmSlot2Options.begin(),
                                m.VmSlot2Options.end());

    return S_OK;
}


STDMETHODIMP DHCPServer::COMGETTER(NetworkName) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mName.cloneTo(aName);

    return S_OK;
}


STDMETHODIMP DHCPServer::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aEnabled = m.enabled;

    return S_OK;
}


STDMETHODIMP DHCPServer::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m.enabled = aEnabled;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}


STDMETHODIMP DHCPServer::COMGETTER(IPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.IPAddress.cloneTo(aIPAddress);

    return S_OK;
}


STDMETHODIMP DHCPServer::COMGETTER(NetworkMask) (BSTR *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.GlobalDhcpOptions[DhcpOpt_SubnetMask].cloneTo(aNetworkMask);

    return S_OK;
}


STDMETHODIMP DHCPServer::COMGETTER(LowerIP) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.lowerIP.cloneTo(aIPAddress);

    return S_OK;
}


STDMETHODIMP DHCPServer::COMGETTER(UpperIP) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.upperIP.cloneTo(aIPAddress);

    return S_OK;
}


STDMETHODIMP DHCPServer::SetConfiguration (IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aLowerIP, IN_BSTR aUpperIP)
{
    AssertReturn(aIPAddress != NULL, E_INVALIDARG);
    AssertReturn(aNetworkMask != NULL, E_INVALIDARG);
    AssertReturn(aLowerIP != NULL, E_INVALIDARG);
    AssertReturn(aUpperIP != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m.IPAddress = aIPAddress;
    m.GlobalDhcpOptions[DhcpOpt_SubnetMask] = aNetworkMask;

    m.lowerIP = aLowerIP;
    m.upperIP = aUpperIP;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->saveSettings();
}


STDMETHODIMP DHCPServer::AddGlobalOption(DhcpOpt_T aOption, IN_BSTR aValue)
{
    CheckComArgStr(aValue);
    /* store global option */
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m.GlobalDhcpOptions.insert(
      DhcpOptValuePair(aOption, Utf8Str(aValue)));

    alock.release();

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->saveSettings();
}


STDMETHODIMP DHCPServer::COMGETTER(GlobalOptions)(ComSafeArrayOut(BSTR, aValue))
{
    CheckComArgOutSafeArrayPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeArray<BSTR> sf(m.GlobalDhcpOptions.size());
    int i = 0;

    for (DhcpOptIterator it = m.GlobalDhcpOptions.begin();
         it != m.GlobalDhcpOptions.end(); ++it)
    {
        Bstr(Utf8StrFmt("%d:%s", (*it).first, (*it).second.c_str())).detachTo(&sf[i]);
        i++;
    }

    sf.detachTo(ComSafeArrayOutArg(aValue));

    return S_OK;
}


STDMETHODIMP DHCPServer::COMGETTER(VmConfigs)(ComSafeArrayOut(BSTR, aValue))
{
    CheckComArgOutSafeArrayPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeArray<BSTR> sf(m.VmSlot2Options.size());
    VmSlot2OptionsIterator it = m.VmSlot2Options.begin();
    int i = 0;
    for (;it != m.VmSlot2Options.end(); ++it)
    {
        Bstr(Utf8StrFmt("[%s]:%d",
                        it->first.VmName.c_str(),
                        it->first.Slot)).detachTo(&sf[i]);
        i++;
    }

    sf.detachTo(ComSafeArrayOutArg(aValue));

    return S_OK;
}


STDMETHODIMP DHCPServer::AddVmSlotOption(IN_BSTR aVmName, LONG aSlot, DhcpOpt_T aOption, IN_BSTR aValue)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m.VmSlot2Options[settings::VmNameSlotKey(
          com::Utf8Str(aVmName),
          aSlot)][aOption] = com::Utf8Str(aValue);


    alock.release();

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->saveSettings();
}


STDMETHODIMP DHCPServer::RemoveVmSlotOptions(IN_BSTR aVmName, LONG aSlot)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    DhcpOptionMap& map = findOptMapByVmNameSlot(com::Utf8Str(aVmName), aSlot);

    map.clear();

    alock.release();

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->saveSettings();
}


/**
 * this is mapping (vm, slot)
 */
STDMETHODIMP DHCPServer::GetVmSlotOptions(IN_BSTR aVmName,
                                          LONG aSlot,
                                          ComSafeArrayOut(BSTR, aValues))
{
    CheckComArgOutSafeArrayPointerValid(aValues);
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    DhcpOptionMap& map = findOptMapByVmNameSlot(com::Utf8Str(aVmName), aSlot);

    SafeArray<BSTR> sf(map.size());
    int i = 0;

    for (DhcpOptIterator it = map.begin();
         it != map.end(); ++it)
    {
        Bstr(Utf8StrFmt("%d:%s", (*it).first, (*it).second.c_str())).detachTo(&sf[i]);
        i++;
    }

    sf.detachTo(ComSafeArrayOutArg(aValues));

    return S_OK;
}


STDMETHODIMP DHCPServer::GetMacOptions(IN_BSTR aMAC, ComSafeArrayOut(BSTR, aValues))
{
    CheckComArgOutSafeArrayPointerValid(aValues);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT hrc;

    ComPtr<IMachine> machine;
    ComPtr<INetworkAdapter> nic;

    VmSlot2OptionsIterator it;
    for(it = m.VmSlot2Options.begin();
        it != m.VmSlot2Options.end();
        ++it)
    {

        alock.release();
        hrc = mVirtualBox->FindMachine(Bstr(it->first.VmName).raw(), machine.asOutParam());
        alock.acquire();

        if (FAILED(hrc))
            continue;

        alock.release();
        hrc = machine->GetNetworkAdapter(it->first.Slot, nic.asOutParam());
        alock.acquire();

        if (FAILED(hrc))
            continue;

        com::Bstr mac;

        alock.release();
        hrc = nic->COMGETTER(MACAddress)(mac.asOutParam());
        alock.acquire();

        if (FAILED(hrc)) /* no MAC address ??? */
            break;

        if (!RTStrICmp(com::Utf8Str(mac).c_str(), com::Utf8Str(aMAC).c_str()))
            return GetVmSlotOptions(Bstr(it->first.VmName).raw(),
                                    it->first.Slot,
                                    ComSafeArrayOutArg(aValues));
    } /* end of for */

    return hrc;
}


STDMETHODIMP DHCPServer::COMGETTER(EventSource)(IEventSource **aEventSource)
{
    ReturnComNotImplemented();
}


STDMETHODIMP DHCPServer::Start(IN_BSTR aNetworkName, IN_BSTR aTrunkName, IN_BSTR aTrunkType)
{
    /* Silently ignore attempts to run disabled servers. */
    if (!m.enabled)
        return S_OK;

    /* Commmon Network Settings */
    m.dhcp.setOption(NETCFG_NETNAME, Utf8Str(aNetworkName), true);

    Bstr tmp(aTrunkName);

    if (!tmp.isEmpty())
        m.dhcp.setOption(NETCFG_TRUNKNAME, Utf8Str(tmp), true);
    m.dhcp.setOption(NETCFG_TRUNKTYPE, Utf8Str(aTrunkType), true);

    /* XXX: should this MAC default initialization moved to NetworkServiceRunner? */
    char strMAC[32];
    Guid guid;
    guid.create();
    RTStrPrintf (strMAC, sizeof(strMAC), "08:00:27:%02X:%02X:%02X",
                 guid.raw()->au8[0],
                 guid.raw()->au8[1],
                 guid.raw()->au8[2]);
    m.dhcp.setOption(NETCFG_MACADDRESS, strMAC, true);
    m.dhcp.setOption(NETCFG_IPADDRESS,  Utf8Str(m.IPAddress), true);
    m.dhcp.setOption(NETCFG_NETMASK, Utf8Str(m.GlobalDhcpOptions[DhcpOpt_SubnetMask]), true);

    /* XXX: This parameters Dhcp Server will fetch via API */
    return RT_FAILURE(m.dhcp.start()) ? E_FAIL : S_OK;
    //m.dhcp.detachFromServer(); /* need to do this to avoid server shutdown on runner destruction */
}


STDMETHODIMP DHCPServer::Stop (void)
{
    return RT_FAILURE(m.dhcp.stop()) ? E_FAIL : S_OK;
}


DhcpOptionMap& DHCPServer::findOptMapByVmNameSlot(const com::Utf8Str& aVmName,
                                                  LONG aSlot)
{
    return m.VmSlot2Options[settings::VmNameSlotKey(
          com::Utf8Str(aVmName),
          aSlot)];
}
