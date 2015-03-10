/* $Id$ */
/** @file
 * INATNetwork implementation.
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

#include <string>
#include "NetworkServiceRunner.h"
#include "DHCPServerImpl.h"
#include "NATNetworkImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/asm.h>
#include <iprt/cpp/utils.h>
#include <iprt/cidr.h>
#include <iprt/net.h>
#include <VBox/com/array.h>
#include <VBox/com/ptr.h>
#include <VBox/settings.h>

#include "EventImpl.h"

#include "VirtualBoxImpl.h"
#include <algorithm>
#include <list>

#ifndef RT_OS_WINDOWS
# include <netinet/in.h>
#else
# define IN_LOOPBACKNET 127
#endif


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////
struct NATNetwork::Data
{
    Data()
      : fEnabled(TRUE)
      , fIPv6Enabled(FALSE)
      , fAdvertiseDefaultIPv6Route(FALSE)
      , fNeedDhcpServer(TRUE)
      , u32LoopbackIp6(0)
      , offGateway(0)
      , offDhcp(0)
    {
        IPv4Gateway.setNull();
        IPv4NetworkCidr.setNull();
        IPv6Prefix.setNull();
        IPv4DhcpServer.setNull();
        IPv4NetworkMask.setNull();
        IPv4DhcpServerLowerIp.setNull();
        IPv4DhcpServerUpperIp.setNull();
    }
    virtual ~Data(){}
    const ComObjPtr<EventSource> pEventSource;
#ifdef VBOX_WITH_NAT_SERVICE
    NATNetworkServiceRunner NATRunner;
    ComObjPtr<IDHCPServer> dhcpServer;
#endif
    com::Utf8Str IPv4Gateway;
    com::Utf8Str IPv4NetworkCidr;
    com::Utf8Str IPv4NetworkMask;
    com::Utf8Str IPv4DhcpServer;
    com::Utf8Str IPv4DhcpServerLowerIp;
    com::Utf8Str IPv4DhcpServerUpperIp;
    BOOL fEnabled;
    BOOL fIPv6Enabled;
    com::Utf8Str IPv6Prefix;
    BOOL fAdvertiseDefaultIPv6Route;
    BOOL fNeedDhcpServer;
    NATRuleMap mapName2PortForwardRule4;
    NATRuleMap mapName2PortForwardRule6;
    settings::NATLoopbackOffsetList maNATLoopbackOffsetList;
    uint32_t u32LoopbackIp6;
    uint32_t offGateway;
    uint32_t offDhcp;
};


NATNetwork::NATNetwork()
    : mVirtualBox(NULL)
{
}


NATNetwork::~NATNetwork()
{
}


HRESULT NATNetwork::FinalConstruct()
{
    return BaseFinalConstruct();
}


void NATNetwork::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}


void NATNetwork::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
    delete m;
    m = NULL;
    unconst(mVirtualBox) = NULL;
}

HRESULT NATNetwork::init(VirtualBox *aVirtualBox, com::Utf8Str aName)
{
    AssertReturn(!aName.isEmpty(), E_INVALIDARG);

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;
    unconst(mName) = aName;
    m = new Data();
    m->offGateway = 1;
    m->IPv4NetworkCidr = "10.0.2.0/24";
    m->IPv6Prefix = "fe80::/64";

    settings::NATHostLoopbackOffset off;
    off.strLoopbackHostAddress = "127.0.0.1";
    off.u32Offset = (uint32_t)2;
    m->maNATLoopbackOffsetList.push_back(off);

    i_recalculateIpv4AddressAssignments();

    HRESULT hrc = unconst(m->pEventSource).createObject();
    if (FAILED(hrc)) throw hrc;

    hrc = m->pEventSource->init();
    if (FAILED(hrc)) throw hrc;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


HRESULT NATNetwork::init(VirtualBox *aVirtualBox,
                         const settings::NATNetwork &data)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;

    unconst(mName) = data.strNetworkName;
    m = new Data();
    m->IPv4NetworkCidr = data.strNetwork;
    m->fEnabled = data.fEnabled;
    m->fAdvertiseDefaultIPv6Route = data.fAdvertiseDefaultIPv6Route;
    m->fNeedDhcpServer = data.fNeedDhcpServer;
    m->fIPv6Enabled = data.fIPv6;

    m->u32LoopbackIp6 = data.u32HostLoopback6Offset;

    m->maNATLoopbackOffsetList.clear();
    m->maNATLoopbackOffsetList.assign(data.llHostLoopbackOffsetList.begin(),
                               data.llHostLoopbackOffsetList.end());

    i_recalculateIpv4AddressAssignments();

    /* IPv4 port-forward rules */
    m->mapName2PortForwardRule4.clear();
    for (settings::NATRuleList::const_iterator it = data.llPortForwardRules4.begin();
        it != data.llPortForwardRules4.end(); ++it)
    {
        m->mapName2PortForwardRule4.insert(std::make_pair(it->strName.c_str(), *it));
    }

    /* IPv6 port-forward rules */
    m->mapName2PortForwardRule6.clear();
    for (settings::NATRuleList::const_iterator it = data.llPortForwardRules6.begin();
        it != data.llPortForwardRules6.end(); ++it)
    {
        m->mapName2PortForwardRule6.insert(std::make_pair(it->strName, *it));
    }

    HRESULT hrc = unconst(m->pEventSource).createObject();
    if (FAILED(hrc)) throw hrc;

    hrc = m->pEventSource->init();
    if (FAILED(hrc)) throw hrc;

    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT NATNetwork::i_saveSettings(settings::NATNetwork &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.strNetworkName = mName;
    data.strNetwork = m->IPv4NetworkCidr;
    data.fEnabled = RT_BOOL(m->fEnabled);
    data.fAdvertiseDefaultIPv6Route = RT_BOOL(m->fAdvertiseDefaultIPv6Route);
    data.fNeedDhcpServer = RT_BOOL(m->fNeedDhcpServer);
    data.fIPv6 = RT_BOOL(m->fIPv6Enabled);
    data.strIPv6Prefix = m->IPv6Prefix;

    /* saving ipv4 port-forward Rules*/
    data.llPortForwardRules4.clear();
    for (NATRuleMap::iterator it = m->mapName2PortForwardRule4.begin();
         it != m->mapName2PortForwardRule4.end(); ++it)
        data.llPortForwardRules4.push_back(it->second);

    /* saving ipv6 port-forward Rules*/
    data.llPortForwardRules6.clear();
    for (NATRuleMap::iterator it = m->mapName2PortForwardRule6.begin();
         it != m->mapName2PortForwardRule6.end(); ++it)
        data.llPortForwardRules6.push_back(it->second);

    data.u32HostLoopback6Offset = m->u32LoopbackIp6;

    data.llHostLoopbackOffsetList.clear();
    data.llHostLoopbackOffsetList.assign(m->maNATLoopbackOffsetList.begin(),
                                         m->maNATLoopbackOffsetList.end());

    mVirtualBox->i_onNATNetworkSetting(Bstr(mName).raw(),
                                       data.fEnabled ? TRUE : FALSE,
                                       Bstr(m->IPv4NetworkCidr).raw(),
                                       Bstr(m->IPv4Gateway).raw(),
                                       data.fAdvertiseDefaultIPv6Route ? TRUE : FALSE,
                                       data.fNeedDhcpServer ? TRUE : FALSE);

    /* Notify listerners listening on this network only */
    fireNATNetworkSettingEvent(m->pEventSource,
                               Bstr(mName).raw(),
                               data.fEnabled ? TRUE : FALSE,
                               Bstr(m->IPv4NetworkCidr).raw(),
                               Bstr(m->IPv4Gateway).raw(),
                               data.fAdvertiseDefaultIPv6Route ? TRUE : FALSE,
                               data.fNeedDhcpServer ? TRUE : FALSE);

    return S_OK;
}

HRESULT NATNetwork::getEventSource(ComPtr<IEventSource> &aEventSource)
{
    /* event source is const, no need to lock */
    m->pEventSource.queryInterfaceTo(aEventSource.asOutParam());
    return S_OK;
}

HRESULT NATNetwork::getNetworkName(com::Utf8Str &aNetworkName)
{
    aNetworkName = mName;
    return S_OK;
}

HRESULT NATNetwork::setNetworkName(const com::Utf8Str &aNetworkName)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (aNetworkName == mName)
            return S_OK;

        unconst(mName) = aNetworkName;
    }
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);

    return S_OK;
}

HRESULT NATNetwork::getEnabled(BOOL *aEnabled)
{
    *aEnabled = m->fEnabled;

    i_recalculateIpv4AddressAssignments();
    return S_OK;
}

HRESULT NATNetwork::setEnabled(const BOOL aEnabled)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (aEnabled == m->fEnabled)
            return S_OK;
        m->fEnabled = aEnabled;
    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);
    return S_OK;
}

HRESULT NATNetwork::getGateway(com::Utf8Str &aIPv4Gateway)
{
    aIPv4Gateway = m->IPv4Gateway;
    return S_OK;
}

HRESULT NATNetwork::getNetwork(com::Utf8Str &aNetwork)
{
    aNetwork = m->IPv4NetworkCidr;
    return S_OK;
}


HRESULT NATNetwork::setNetwork(const com::Utf8Str &aIPv4NetworkCidr)
{
    {

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (aIPv4NetworkCidr == m->IPv4NetworkCidr)
            return S_OK;

        /* silently ignore network cidr update for now.
         * todo: keep internally guest address of port forward rule
         * as offset from network id.
         */
        if (!m->mapName2PortForwardRule4.empty())
            return S_OK;


        unconst(m->IPv4NetworkCidr) = aIPv4NetworkCidr;
        i_recalculateIpv4AddressAssignments();
    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);
    return S_OK;
}


HRESULT NATNetwork::getIPv6Enabled(BOOL *aIPv6Enabled)
{
    *aIPv6Enabled = m->fIPv6Enabled;

    return S_OK;
}


HRESULT NATNetwork::setIPv6Enabled(const BOOL aIPv6Enabled)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (aIPv6Enabled == m->fIPv6Enabled)
            return S_OK;

        m->fIPv6Enabled = aIPv6Enabled;
    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);

    return S_OK;
}


HRESULT NATNetwork::getIPv6Prefix(com::Utf8Str &aIPv6Prefix)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aIPv6Prefix = m->IPv6Prefix;
    return S_OK;
}

HRESULT NATNetwork::setIPv6Prefix(const com::Utf8Str &aIPv6Prefix)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (aIPv6Prefix == m->IPv6Prefix)
            return S_OK;

        /* silently ignore network IPv6 prefix update.
         * todo: see similar todo in NATNetwork::COMSETTER(Network)(IN_BSTR)
         */
        if (!m->mapName2PortForwardRule6.empty())
            return S_OK;

        unconst(m->IPv6Prefix) = Bstr(aIPv6Prefix);
    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);

    return S_OK;
}


HRESULT NATNetwork::getAdvertiseDefaultIPv6RouteEnabled(BOOL *aAdvertiseDefaultIPv6Route)
{
    *aAdvertiseDefaultIPv6Route = m->fAdvertiseDefaultIPv6Route;

    return S_OK;
}


HRESULT NATNetwork::setAdvertiseDefaultIPv6RouteEnabled(const BOOL aAdvertiseDefaultIPv6Route)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (aAdvertiseDefaultIPv6Route == m->fAdvertiseDefaultIPv6Route)
            return S_OK;

        m->fAdvertiseDefaultIPv6Route = aAdvertiseDefaultIPv6Route;

    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);

    return S_OK;
}


HRESULT NATNetwork::getNeedDhcpServer(BOOL *aNeedDhcpServer)
{
    *aNeedDhcpServer = m->fNeedDhcpServer;

    return S_OK;
}

HRESULT NATNetwork::setNeedDhcpServer(const BOOL aNeedDhcpServer)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (aNeedDhcpServer == m->fNeedDhcpServer)
            return S_OK;

        m->fNeedDhcpServer = aNeedDhcpServer;

        i_recalculateIpv4AddressAssignments();

    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->i_saveSettings();
    ComAssertComRCRetRC(rc);

    return S_OK;
}

HRESULT NATNetwork::getLocalMappings(std::vector<com::Utf8Str> &aLocalMappings)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aLocalMappings.resize(m->maNATLoopbackOffsetList.size());
    size_t i = 0;
    for (settings::NATLoopbackOffsetList::const_iterator it = m->maNATLoopbackOffsetList.begin();
         it != m->maNATLoopbackOffsetList.end(); ++it, ++i)
    {
        aLocalMappings[i] = Utf8StrFmt("%s=%d",
                            (*it).strLoopbackHostAddress.c_str(),
                            (*it).u32Offset);
    }

    return S_OK;
}

HRESULT NATNetwork::addLocalMapping(const com::Utf8Str &aHostId, LONG aOffset)
{
    RTNETADDRIPV4 addr, net, mask;

    int rc = RTNetStrToIPv4Addr(Utf8Str(aHostId).c_str(), &addr);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    /* check against 127/8 */
    if ((RT_N2H_U32(addr.u) >> IN_CLASSA_NSHIFT) != IN_LOOPBACKNET)
        return E_INVALIDARG;

    /* check against networkid vs network mask */
    rc = RTCidrStrToIPv4(Utf8Str(m->IPv4NetworkCidr).c_str(), &net, &mask);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    if (((net.u + aOffset) & mask.u) != net.u)
        return E_INVALIDARG;

    settings::NATLoopbackOffsetList::iterator it;

    it = std::find(m->maNATLoopbackOffsetList.begin(),
                   m->maNATLoopbackOffsetList.end(),
                   aHostId);
    if (it != m->maNATLoopbackOffsetList.end())
    {
        if (aOffset == 0) /* erase */
            m->maNATLoopbackOffsetList.erase(it, it);
        else /* modify */
        {
            settings::NATLoopbackOffsetList::iterator it1;
            it1 = std::find(m->maNATLoopbackOffsetList.begin(),
                           m->maNATLoopbackOffsetList.end(),
                           (uint32_t)aOffset);
            if (it1 != m->maNATLoopbackOffsetList.end())
                return E_INVALIDARG; /* this offset is already registered. */

            (*it).u32Offset = aOffset;
        }

        AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
        return mVirtualBox->i_saveSettings();
    }

    /* injection */
    it = std::find(m->maNATLoopbackOffsetList.begin(),
                   m->maNATLoopbackOffsetList.end(),
                   (uint32_t)aOffset);

    if (it != m->maNATLoopbackOffsetList.end())
        return E_INVALIDARG; /* offset is already registered. */

    settings::NATHostLoopbackOffset off;
    off.strLoopbackHostAddress = aHostId;
    off.u32Offset = (uint32_t)aOffset;
    m->maNATLoopbackOffsetList.push_back(off);

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->i_saveSettings();
}


HRESULT NATNetwork::getLoopbackIp6(LONG *aLoopbackIp6)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aLoopbackIp6 = m->u32LoopbackIp6;
    return S_OK;
}


HRESULT NATNetwork::setLoopbackIp6(LONG aLoopbackIp6)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (aLoopbackIp6 < 0)
            return E_INVALIDARG;

        if (static_cast<uint32_t>(aLoopbackIp6) == m->u32LoopbackIp6)
            return S_OK;

        m->u32LoopbackIp6 = aLoopbackIp6;
    }

    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->i_saveSettings();
}


HRESULT NATNetwork::getPortForwardRules4(std::vector<com::Utf8Str> &aPortForwardRules4)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    i_getPortForwardRulesFromMap(aPortForwardRules4,
                                 m->mapName2PortForwardRule4);
    return S_OK;
}

HRESULT NATNetwork::getPortForwardRules6(std::vector<com::Utf8Str> &aPortForwardRules6)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    i_getPortForwardRulesFromMap(aPortForwardRules6,
                                 m->mapName2PortForwardRule6);
    return S_OK;
}

HRESULT NATNetwork::addPortForwardRule(BOOL aIsIpv6,
                                       const com::Utf8Str &aPortForwardRuleName,
                                       NATProtocol_T aProto,
                                       const com::Utf8Str &aHostIp,
                                       USHORT aHostPort,
                                       const com::Utf8Str &aGuestIp,
                                       USHORT aGuestPort)
{
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        Utf8Str name = aPortForwardRuleName;
        Utf8Str proto;
        settings::NATRule r;
        NATRuleMap& mapRules = aIsIpv6 ? m->mapName2PortForwardRule6 : m->mapName2PortForwardRule4;
        switch (aProto)
        {
            case NATProtocol_TCP:
                proto = "tcp";
                break;
            case NATProtocol_UDP:
                proto = "udp";
                break;
            default:
                return E_INVALIDARG;
        }
        if (name.isEmpty())
            name = Utf8StrFmt("%s_[%s]%%%d_[%s]%%%d", proto.c_str(),
                              aHostIp.c_str(), aHostPort,
                              aGuestIp.c_str(), aGuestPort);

        for (NATRuleMap::iterator it = mapRules.begin(); it != mapRules.end(); ++it)
        {
            r = it->second;
            if (it->first == name)
                return setError(E_INVALIDARG,
                                tr("A NAT rule of this name already exists"));
            if (   r.strHostIP == aHostIp
                   && r.u16HostPort == aHostPort
                   && r.proto == aProto)
                return setError(E_INVALIDARG,
                                tr("A NAT rule for this host port and this host IP already exists"));
        }

        r.strName = name.c_str();
        r.proto = aProto;
        r.strHostIP = aHostIp;
        r.u16HostPort = aHostPort;
        r.strGuestIP = aGuestIp;
        r.u16GuestPort = aGuestPort;
        mapRules.insert(std::make_pair(name, r));
    }
    {
        AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
        HRESULT rc = mVirtualBox->i_saveSettings();
        ComAssertComRCRetRC(rc);
    }

    mVirtualBox->i_onNATNetworkPortForward(Bstr(mName).raw(), TRUE, aIsIpv6,
                                           Bstr(aPortForwardRuleName).raw(), aProto,
                                           Bstr(aHostIp).raw(), aHostPort,
                                           Bstr(aGuestIp).raw(), aGuestPort);

    /* Notify listerners listening on this network only */
    fireNATNetworkPortForwardEvent(m->pEventSource, Bstr(mName).raw(), TRUE,
                                   aIsIpv6, Bstr(aPortForwardRuleName).raw(), aProto,
                                   Bstr(aHostIp).raw(), aHostPort,
                                   Bstr(aGuestIp).raw(), aGuestPort);

    return S_OK;
}

HRESULT NATNetwork::removePortForwardRule(BOOL aIsIpv6, const com::Utf8Str &aPortForwardRuleName)
{
    Utf8Str strHostIP;
    Utf8Str strGuestIP;
    uint16_t u16HostPort;
    uint16_t u16GuestPort;
    NATProtocol_T proto;

    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        NATRuleMap& mapRules = aIsIpv6 ? m->mapName2PortForwardRule6 : m->mapName2PortForwardRule4;
        NATRuleMap::iterator it = mapRules.find(aPortForwardRuleName);

        if (it == mapRules.end())
            return E_INVALIDARG;

        strHostIP = it->second.strHostIP;
        strGuestIP = it->second.strGuestIP;
        u16HostPort = it->second.u16HostPort;
        u16GuestPort = it->second.u16GuestPort;
        proto = it->second.proto;

        mapRules.erase(it);
    }

    {
        AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
        HRESULT rc = mVirtualBox->i_saveSettings();
        ComAssertComRCRetRC(rc);
    }

    mVirtualBox->i_onNATNetworkPortForward(Bstr(mName).raw(), FALSE, aIsIpv6,
                                           Bstr(aPortForwardRuleName).raw(), proto,
                                           Bstr(strHostIP).raw(), u16HostPort,
                                           Bstr(strGuestIP).raw(), u16GuestPort);

    /* Notify listerners listening on this network only */
    fireNATNetworkPortForwardEvent(m->pEventSource, Bstr(mName).raw(), FALSE,
                                   aIsIpv6, Bstr(aPortForwardRuleName).raw(), proto,
                                   Bstr(strHostIP).raw(), u16HostPort,
                                   Bstr(strGuestIP).raw(), u16GuestPort);
    return S_OK;
}


HRESULT  NATNetwork::start(const com::Utf8Str &aTrunkType)
{
#ifdef VBOX_WITH_NAT_SERVICE
    if (!m->fEnabled) return S_OK;

    m->NATRunner.setOption(NetworkServiceRunner::kNsrKeyNetwork, Utf8Str(mName).c_str());
    m->NATRunner.setOption(NetworkServiceRunner::kNsrKeyTrunkType, Utf8Str(aTrunkType).c_str());
    m->NATRunner.setOption(NetworkServiceRunner::kNsrIpAddress, Utf8Str(m->IPv4Gateway).c_str());
    m->NATRunner.setOption(NetworkServiceRunner::kNsrIpNetmask, Utf8Str(m->IPv4NetworkMask).c_str());

    /* No portforwarding rules from command-line, all will be fetched via API */

    if (m->fNeedDhcpServer)
    {
        /*
         * Just to as idea... via API (on creation user pass the cidr of network and)
         * and we calculate it's addreses (mutable?).
         */

        /*
         * Configuration and running DHCP server:
         * 1. find server first createDHCPServer
         * 2. if return status is E_INVALARG => server already exists just find and start.
         * 3. if return status neither E_INVALRG nor S_OK => return E_FAIL
         * 4. if return status S_OK proceed to DHCP server configuration
         * 5. call setConfiguration() and pass all required parameters
         * 6. start dhcp server.
         */
        HRESULT hrc = mVirtualBox->FindDHCPServerByNetworkName(Bstr(mName).raw(),
                                                               m->dhcpServer.asOutParam());
        switch (hrc)
        {
            case E_INVALIDARG:
                /* server haven't beeen found let create it then */
                hrc = mVirtualBox->CreateDHCPServer(Bstr(mName).raw(),
                                                   m->dhcpServer.asOutParam());
                if (FAILED(hrc))
                  return E_FAIL;
                /* breakthrough */

            {
                LogFunc(("gateway: %s, dhcpserver:%s, dhcplowerip:%s, dhcpupperip:%s\n",
                         m->IPv4Gateway.c_str(),
                         m->IPv4DhcpServer.c_str(),
                         m->IPv4DhcpServerLowerIp.c_str(),
                         m->IPv4DhcpServerUpperIp.c_str()));

                hrc = m->dhcpServer->COMSETTER(Enabled)(true);

                BSTR dhcpip = NULL;
                BSTR netmask = NULL;
                BSTR lowerip = NULL;
                BSTR upperip = NULL;

                m->IPv4DhcpServer.cloneTo(&dhcpip);
                m->IPv4NetworkMask.cloneTo(&netmask);
                m->IPv4DhcpServerLowerIp.cloneTo(&lowerip);
                m->IPv4DhcpServerUpperIp.cloneTo(&upperip);
                hrc = m->dhcpServer->SetConfiguration(dhcpip,
                                                     netmask,
                                                     lowerip,
                                                     upperip);
            }
            case S_OK:
                break;

            default:
                return E_FAIL;
        }

        /* XXX: AddGlobalOption(DhcpOpt_Router,) - enables attachement of DhcpServer to Main. */
        m->dhcpServer->AddGlobalOption(DhcpOpt_Router, Bstr(m->IPv4Gateway).raw());

        hrc = m->dhcpServer->Start(Bstr(mName).raw(), Bstr("").raw(), Bstr(aTrunkType).raw());
        if (FAILED(hrc))
        {
            m->dhcpServer.setNull();
            return E_FAIL;
        }
    }

    if (RT_SUCCESS(m->NATRunner.start(false /* KillProcOnStop */)))
    {
        mVirtualBox->i_onNATNetworkStartStop(Bstr(mName).raw(), TRUE);
        return S_OK;
    }
    /** @todo missing setError()! */
    return E_FAIL;
#else
    NOREF(aTrunkType);
    ReturnComNotImplemented();
#endif
}

HRESULT NATNetwork::stop()
{
#ifdef VBOX_WITH_NAT_SERVICE
    mVirtualBox->i_onNATNetworkStartStop(Bstr(mName).raw(), FALSE);

    if (!m->dhcpServer.isNull())
        m->dhcpServer->Stop();

    if (RT_SUCCESS(m->NATRunner.stop()))
        return S_OK;

    /** @todo missing setError()! */
    return E_FAIL;
#else
    ReturnComNotImplemented();
#endif
}


void NATNetwork::i_getPortForwardRulesFromMap(std::vector<com::Utf8Str> &aPortForwardRules, NATRuleMap& aRules)
{
    aPortForwardRules.resize(aRules.size());
    size_t i = 0;
    for (NATRuleMap::const_iterator it = aRules.begin();
         it != aRules.end(); ++it, ++i)
      {
        settings::NATRule r = it->second;
        aPortForwardRules[i] =  Utf8StrFmt("%s:%s:[%s]:%d:[%s]:%d",
                                           r.strName.c_str(),
                                           (r.proto == NATProtocol_TCP? "tcp" : "udp"),
                                           r.strHostIP.c_str(),
                                           r.u16HostPort,
                                           r.strGuestIP.c_str(),
                                           r.u16GuestPort);
    }
}


int NATNetwork::i_findFirstAvailableOffset(ADDRESSLOOKUPTYPE addrType, uint32_t *poff)
{
    RTNETADDRIPV4 network, netmask;

    int rc = RTCidrStrToIPv4(m->IPv4NetworkCidr.c_str(),
                             &network,
                             &netmask);
    AssertRCReturn(rc, rc);

    uint32_t off;
    for (off = 1; off < ~netmask.u; ++off)
    {
        bool skip = false;
        for (settings::NATLoopbackOffsetList::iterator it = m->maNATLoopbackOffsetList.begin();
             it != m->maNATLoopbackOffsetList.end();
             ++it)
        {
            if ((*it).u32Offset == off)
            {
                skip = true;
                break;
            }

        }

        if (skip)
            continue;

        if (off == m->offGateway)
        {
            if (addrType == ADDR_GATEWAY)
                break;
            else
                continue;
        }

        if (off == m->offDhcp)
        {
            if (addrType == ADDR_DHCP)
                break;
            else
                continue;
        }

        if (!skip)
            break;
    }

    if (poff)
        *poff = off;

    return VINF_SUCCESS;
}

int NATNetwork::i_recalculateIpv4AddressAssignments()
{
    RTNETADDRIPV4 network, netmask;
    int rc = RTCidrStrToIPv4(m->IPv4NetworkCidr.c_str(),
                             &network,
                             &netmask);
    AssertRCReturn(rc, rc);

    i_findFirstAvailableOffset(ADDR_GATEWAY, &m->offGateway);
    if (m->fNeedDhcpServer)
        i_findFirstAvailableOffset(ADDR_DHCP, &m->offDhcp);

    /* I don't remember the reason CIDR calculated on the host. */
    RTNETADDRIPV4 gateway = network;
    gateway.u += m->offGateway;
    gateway.u = RT_H2N_U32(gateway.u);
    char szTmpIp[16];
    RTStrPrintf(szTmpIp, sizeof(szTmpIp), "%RTnaipv4", gateway);
    m->IPv4Gateway = szTmpIp;

    if (m->fNeedDhcpServer)
    {
        RTNETADDRIPV4 dhcpserver = network;
        dhcpserver.u += m->offDhcp;

        /* XXX: adding more services should change the math here */
        RTNETADDRIPV4 dhcplowerip = network;
        uint32_t offDhcpLowerIp;
        i_findFirstAvailableOffset(ADDR_DHCPLOWERIP, &offDhcpLowerIp);
        dhcplowerip.u = RT_H2N_U32(dhcplowerip.u + offDhcpLowerIp);

        RTNETADDRIPV4 dhcpupperip;
        dhcpupperip.u = RT_H2N_U32((network.u | ~netmask.u) - 1);

        dhcpserver.u = RT_H2N_U32(dhcpserver.u);
        network.u = RT_H2N_U32(network.u);

        RTStrPrintf(szTmpIp, sizeof(szTmpIp), "%RTnaipv4", dhcpserver);
        m->IPv4DhcpServer = szTmpIp;
        RTStrPrintf(szTmpIp, sizeof(szTmpIp), "%RTnaipv4", dhcplowerip);
        m->IPv4DhcpServerLowerIp = szTmpIp;
        RTStrPrintf(szTmpIp, sizeof(szTmpIp), "%RTnaipv4", dhcpupperip);
        m->IPv4DhcpServerUpperIp = szTmpIp;

        LogFunc(("network:%RTnaipv4, dhcpserver:%RTnaipv4, dhcplowerip:%RTnaipv4, dhcpupperip:%RTnaipv4\n",
                 network, dhcpserver, dhcplowerip, dhcpupperip));
    }

    /* we need IPv4NetworkMask for NAT's gw service start */
    netmask.u = RT_H2N_U32(netmask.u);
    RTStrPrintf(szTmpIp, 16, "%RTnaipv4", netmask);
    m->IPv4NetworkMask = szTmpIp;

    LogFlowFunc(("getaway:%RTnaipv4, netmask:%RTnaipv4\n", gateway, netmask));
    return VINF_SUCCESS;
}
