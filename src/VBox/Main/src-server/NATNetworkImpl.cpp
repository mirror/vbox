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

#include "DHCPServerRunner.h"
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
#include "NATNetworkServiceRunner.h"
#include "VirtualBoxImpl.h"


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

struct NATNetwork::Data
{
    Data() : 
      
      fEnabled(FALSE),
      fIPv6Enabled(FALSE),
      fAdvertiseDefaultIPv6Route(FALSE),
      fNeedDhcpServer(FALSE)
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
    Bstr IPv4Gateway;
    Bstr IPv4NetworkCidr;
    Bstr IPv4NetworkMask;
    Bstr IPv4DhcpServer;
    Bstr IPv4DhcpServerLowerIp;
    Bstr IPv4DhcpServerUpperIp;
    BOOL fEnabled;
    BOOL fIPv6Enabled;
    Bstr IPv6Prefix;
    BOOL fAdvertiseDefaultIPv6Route;
    BOOL fNeedDhcpServer;
    NATRuleMap mapName2PortForwardRule4;
    NATRuleMap mapName2PortForwardRule6;
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
    uninit ();

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

HRESULT NATNetwork::init(VirtualBox *aVirtualBox, IN_BSTR aName)
{
    AssertReturn(aName != NULL, E_INVALIDARG);

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;

    unconst(mName) = aName;
    m = new Data();
    m->IPv4Gateway = "10.0.2.2";
    m->IPv4NetworkCidr = "10.0.2.0/24";
    m->IPv6Prefix = "fe80::/64";
    m->fEnabled = FALSE;
    RecalculateIpv4AddressAssignments();
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
    RecalculateIpv4AddressAssignments();
    /* IPv4 port-forward rules */
    m->mapName2PortForwardRule4.clear();
    for (settings::NATRuleList::const_iterator it = data.llPortForwardRules4.begin();
        it != data.llPortForwardRules4.end(); ++it)
    {
        m->mapName2PortForwardRule4.insert(std::make_pair(it->strName, *it));
    }

    /* IPv6 port-forward rules */
    m->mapName2PortForwardRule6.clear();
    for (settings::NATRuleList::const_iterator it = data.llPortForwardRules6.begin();
        it != data.llPortForwardRules6.end(); ++it)
    {
        m->mapName2PortForwardRule6.insert(std::make_pair(it->strName, *it));
    }
   
    autoInitSpan.setSucceeded();

    return S_OK;
}

#ifdef NAT_XML_SERIALIZATION
HRESULT NATNetwork::saveSettings(settings::NATNetwork &data)
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
      data.llPortForwardRules4.push_back(it->second);
    /* XXX: should we do here a copy of params */
    /* XXX: should we unlock here? */
    mVirtualBox->onNATNetworkSetting(mName.raw(), 
                                     data.fEnabled ? TRUE : FALSE,
                                     m->IPv4NetworkCidr.raw(),
                                     m->IPv4Gateway.raw(), 
                                     data.fAdvertiseDefaultIPv6Route ? TRUE : FALSE,
                                     data.fNeedDhcpServer ? TRUE : FALSE);
    return S_OK;
}
#endif

STDMETHODIMP NATNetwork::COMGETTER(EventSource)(IEventSource ** aEventSource)
{
    CheckComArgOutPointerValid(aEventSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* event source is const, no need to lock */
    m->pEventSource.queryInterfaceTo(aEventSource);

    return S_OK;
}

STDMETHODIMP NATNetwork::COMGETTER(NetworkName) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(NetworkName) (IN_BSTR aName)
{
    CheckComArgOutPointerValid(aName);

    HRESULT rc = S_OK;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    unconst(mName) = aName;

    alock.release();
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    return rc;
}


STDMETHODIMP NATNetwork::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aEnabled = m->fEnabled;
    RecalculateIpv4AddressAssignments();

    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    HRESULT rc = S_OK;
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->fEnabled = aEnabled;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    return rc;
}

STDMETHODIMP NATNetwork::COMGETTER(Gateway) (BSTR *aIPv4Gateway)
{
    CheckComArgOutPointerValid(aIPv4Gateway);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m->IPv4Gateway.cloneTo(aIPv4Gateway);

    return S_OK;
}

STDMETHODIMP NATNetwork::COMGETTER(Network) (BSTR *aIPv4NetworkCidr)
{
    CheckComArgOutPointerValid(aIPv4NetworkCidr);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    m->IPv4NetworkCidr.cloneTo(aIPv4NetworkCidr);
    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(Network) (IN_BSTR aIPv4NetworkCidr)
{
    CheckComArgOutPointerValid(aIPv4NetworkCidr);

    HRESULT rc = S_OK;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    /* silently ignore network cidr update */
    if (m->mapName2PortForwardRule4.empty())
    {

        unconst(m->IPv4NetworkCidr) = Bstr(aIPv4NetworkCidr);
        RecalculateIpv4AddressAssignments();
        alock.release();
#ifdef NAT_XML_SERIALIZATION
        AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
        rc = mVirtualBox->saveSettings();
#endif
    }
    return rc;
}

STDMETHODIMP NATNetwork::COMGETTER(IPv6Enabled)(BOOL *aAdvertiseDefaultIPv6Route)
{
    CheckComArgOutPointerValid(aAdvertiseDefaultIPv6Route);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aAdvertiseDefaultIPv6Route = m->fAdvertiseDefaultIPv6Route;

    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(IPv6Enabled)(BOOL aAdvertiseDefaultIPv6Route)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    HRESULT rc = S_OK;
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->fAdvertiseDefaultIPv6Route = aAdvertiseDefaultIPv6Route;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    return rc;
}

STDMETHODIMP NATNetwork::COMGETTER(IPv6Prefix) (BSTR *aIPv6Prefix)
{
    CheckComArgOutPointerValid(aIPv6Prefix);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(IPv6Prefix) (IN_BSTR aIPv6Prefix)
{
    CheckComArgOutPointerValid(aIPv6Prefix);

    HRESULT rc = S_OK;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    /* silently ignore network cidr update */
    if (m->mapName2PortForwardRule6.empty())
    {

        unconst(m->IPv6Prefix) = Bstr(aIPv6Prefix);
        /* @todo: do we need recalcualtion ? */
        alock.release();
#ifdef NAT_XML_SERIALIZATION
        AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
        rc = mVirtualBox->saveSettings();
#endif
    }
    return rc;
}

STDMETHODIMP NATNetwork::COMGETTER(AdvertiseDefaultIPv6RouteEnabled)(BOOL *aAdvertiseDefaultIPv6Route)
{
    CheckComArgOutPointerValid(aAdvertiseDefaultIPv6Route);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aAdvertiseDefaultIPv6Route = m->fAdvertiseDefaultIPv6Route;

    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(AdvertiseDefaultIPv6RouteEnabled)(BOOL aAdvertiseDefaultIPv6Route)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    HRESULT rc = S_OK;
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->fAdvertiseDefaultIPv6Route = aAdvertiseDefaultIPv6Route;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    return rc;
}

STDMETHODIMP NATNetwork::COMGETTER(NeedDhcpServer)(BOOL *aNeedDhcpServer)
{
    CheckComArgOutPointerValid(aNeedDhcpServer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aNeedDhcpServer = m->fNeedDhcpServer;

    return S_OK;
}

STDMETHODIMP NATNetwork::COMSETTER(NeedDhcpServer)(BOOL aNeedDhcpServer)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    HRESULT rc = S_OK;
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->fNeedDhcpServer = aNeedDhcpServer;
    RecalculateIpv4AddressAssignments();
    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    return rc;
}

STDMETHODIMP NATNetwork::COMGETTER(PortForwardRules4)(ComSafeArrayOut(BSTR, aPortForwardRules4))
{
    CheckComArgOutSafeArrayPointerValid(aPortForwardRules4);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    /* aPortForwardRules4Size appears from ComSafeArrayOut */
    GetPortForwardRulesFromMap(aPortForwardRules4Size, 
                                  aPortForwardRules4, 
                                  m->mapName2PortForwardRule4);
    alock.release();
    return S_OK;
}

STDMETHODIMP NATNetwork::COMGETTER(PortForwardRules6)(ComSafeArrayOut(BSTR, aPortForwardRules6))
{
    CheckComArgOutSafeArrayPointerValid(aPortForwardRules6);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    GetPortForwardRulesFromMap(aPortForwardRules6Size, aPortForwardRules6, m->mapName2PortForwardRule6);
    return S_OK;
}

STDMETHODIMP NATNetwork::AddPortForwardRule(BOOL aIsIpv6, 
                                            IN_BSTR aPortForwardRuleName, 
                                            NATProtocol_T aProto,
                                            IN_BSTR aHostIp,
                                            USHORT aHostPort,
                                            IN_BSTR aGuestIp,
                                            USHORT aGuestPort)
{
    int rc = S_OK;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

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
      name = Utf8StrFmt("%s_%s:%d_%s:%d", proto.c_str(), 
                        Utf8Str(aHostIp).c_str(), aHostPort, 
                        Utf8Str(aGuestIp).c_str(), aGuestPort);

    NATRuleMap::iterator it;
    for (it = mapRules.begin(); it != mapRules.end(); ++it)
    {
        r = it->second;
        if (it->first == name)
            return setError(E_INVALIDARG,
                            tr("A NAT rule of this name already exists"));
        if (   r.strHostIP == Utf8Str(aHostIp)
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

    alock.release();
    mVirtualBox->onNATNetworkPortForward(mName.raw(), TRUE, aIsIpv6, 
                                         aPortForwardRuleName, aProto, 
                                         aHostIp, aHostPort, 
                                         aGuestIp, aGuestPort);
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    /* @todo: fire the event */
    return rc;
}

STDMETHODIMP NATNetwork::RemovePortForwardRule(BOOL aIsIpv6, IN_BSTR aPortForwardRuleName)
{
    int rc = S_OK;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    NATRuleMap& mapRules = aIsIpv6 ? m->mapName2PortForwardRule6 : m->mapName2PortForwardRule4;
    NATRuleMap::iterator it = mapRules.find(aPortForwardRuleName);
    if (it == mapRules.end())
        return E_INVALIDARG;
   
    mapRules.erase(it);
    
    alock.release();
    /* we need only name here, it supposed to be uniq within IP version protocols */
    mVirtualBox->onNATNetworkPortForward(mName.raw(), FALSE, aIsIpv6, 
                                         aPortForwardRuleName, NATProtocol_TCP, 
                                         Bstr().raw(), 0, 
                                         Bstr().raw(), 0);
#ifdef NAT_XML_SERIALIZATION
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    rc = mVirtualBox->saveSettings();
#endif
    /* @todo: fire the event */
    return rc;
}


STDMETHODIMP NATNetwork::Start(IN_BSTR aTrunkType)
{
#ifdef VBOX_WITH_NAT_SERVICE
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (!m->fEnabled) return S_OK;
    m->NATRunner.setOption(NATSCCFG_NAME, mName, true);
    m->NATRunner.setOption(NATSCCFG_TRUNKTYPE, Utf8Str(aTrunkType), true);
    m->NATRunner.setOption(NATSCCFG_IPADDRESS, m->IPv4Gateway, true);
    m->NATRunner.setOption(NATSCCFG_NETMASK, m->IPv4NetworkMask, true);

    if (m->fNeedDhcpServer)
    {
        /**
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
        int rc = mVirtualBox->FindDHCPServerByNetworkName(mName.raw(), 
                                                      m->dhcpServer.asOutParam());
        switch (rc) 
        {
            case E_INVALIDARG:
                /* server haven't beeen found let create it then */
                rc = mVirtualBox->CreateDHCPServer(mName.raw(), 
                                                   m->dhcpServer.asOutParam());
                if (FAILED(rc))
                  return E_FAIL;
                /* breakthrough */
            case S_OK:
            {
                LogFunc(("gateway: %s, dhcpserver:%s, dhcplowerip:%s, dhcpupperip:%s\n", 
                         Utf8Str(m->IPv4Gateway.raw()).c_str(), 
                         Utf8Str(m->IPv4DhcpServer.raw()).c_str(),
                         Utf8Str(m->IPv4DhcpServerLowerIp.raw()).c_str(), 
                         Utf8Str(m->IPv4DhcpServerUpperIp.raw()).c_str()));
                         

                rc = m->dhcpServer->SetEnabled(true);
                BSTR dhcpip = NULL;
                BSTR netmask = NULL;
                BSTR lowerip = NULL;
                BSTR upperip = NULL;
                m->IPv4DhcpServer.cloneTo(&dhcpip);
                m->IPv4NetworkMask.cloneTo(&netmask);
                
                m->IPv4DhcpServerLowerIp.cloneTo(&lowerip); 
                m->IPv4DhcpServerUpperIp.cloneTo(&upperip);
                rc = m->dhcpServer->SetConfiguration(dhcpip, 
                                                     netmask,
                                                     lowerip,
                                                     upperip);
            break;
            }
            
        default:
            return E_FAIL;
        }

        rc = m->dhcpServer->Start(mName.raw(), Bstr("").raw(), aTrunkType);
        if (FAILED(rc))
        {
            m->dhcpServer.setNull();
            return E_FAIL;
        }
    }
    
    if (RT_SUCCESS(m->NATRunner.start()))
    {
        mVirtualBox->onNATNetworkStartStop(mName.raw(), TRUE);
        return S_OK;
    }
    else return E_FAIL;
#else
    NOREF(aTrunkType);
    return E_NOTIMPL;
#endif
}

STDMETHODIMP NATNetwork::Stop()
{
#ifdef VBOX_WITH_NAT_SERVICE
    if (RT_SUCCESS(m->NATRunner.stop()))
    {
        mVirtualBox->onNATNetworkStartStop(mName.raw(), FALSE);
        return S_OK;
    }
    else return E_FAIL;
        
#else
    return E_NOTIMPL;
#endif
}

void NATNetwork::GetPortForwardRulesFromMap(ComSafeArrayOut(BSTR, aPortForwardRules), NATRuleMap& aRules)
{
    com::SafeArray<BSTR> sf(aRules.size());
    size_t i = 0;
    NATRuleMap::const_iterator it;
    for (it = aRules.begin();
         it != aRules.end(); ++it, ++i)
      {
        settings::NATRule r = it->second;
        BstrFmt bstr("%s,%d,%s,%d,%s,%d",
                     r.strName.c_str(),
                     r.proto,
                     r.strHostIP.c_str(),
                     r.u16HostPort,
                     r.strGuestIP.c_str(),
                     r.u16GuestPort);
        bstr.detachTo(&sf[i]);
    }
    sf.detachTo(ComSafeArrayOutArg(aPortForwardRules));
}

int NATNetwork::RecalculateIpv4AddressAssignments()
{
    /**
     * We assume that port-forwarding rules set is empty!
     * possible scenarious on change of CIDR we clean up (1) pfs 
     * or (2) rewrite all rules to new network.
     */
    AssertReturn(m->mapName2PortForwardRule4.empty(), VERR_INTERNAL_ERROR);
    RTNETADDRIPV4 network, netmask, gateway;
    char aszGatewayIp[16], aszNetmask[16];
    RT_ZERO(aszNetmask);
    int rc = RTCidrStrToIPv4(Utf8Str(m->IPv4NetworkCidr.raw()).c_str(), 
                             &network, 
                             &netmask);
    AssertRCReturn(rc, rc);
  
    /* I don't remember the reason CIDR calcualted in host */
    gateway.u = network.u;

    gateway.u += 1;
    gateway.u = RT_H2N_U32(gateway.u);
    RTStrPrintf(aszGatewayIp, 16, "%RTnaipv4", gateway);
    m->IPv4Gateway = RTStrDup(aszGatewayIp);
    if (m->fNeedDhcpServer)
    {
        RTNETADDRIPV4 dhcpserver,
          dhcplowerip,
          dhcpupperip;
        char aszNetwork[16], 
          aszDhcpIp[16],
          aszDhcpLowerIp[16],
          aszDhcpUpperIp[16];
        RT_ZERO(aszNetwork);

        RT_ZERO(aszDhcpIp);
        RT_ZERO(aszDhcpLowerIp);
        RT_ZERO(aszDhcpUpperIp);

        dhcpserver.u = network.u;
        dhcpserver.u += 2;
        
        
        /* XXX: adding more services should change the math here */
        dhcplowerip.u = RT_H2N_U32(dhcpserver.u + 1);
        dhcpupperip.u = RT_H2N_U32((network.u | (~netmask.u)) -1);
        dhcpserver.u = RT_H2N_U32(dhcpserver.u);
        network.u = RT_H2N_U32(network.u);
               
        RTStrPrintf(aszNetwork, 16, "%RTnaipv4", network);
        RTStrPrintf(aszDhcpLowerIp, 16, "%RTnaipv4", dhcplowerip);
        RTStrPrintf(aszDhcpUpperIp, 16, "%RTnaipv4", dhcpupperip);
        RTStrPrintf(aszDhcpIp, 16, "%RTnaipv4", dhcpserver);

        m->IPv4DhcpServer = aszDhcpIp;
        m->IPv4DhcpServerLowerIp = aszDhcpLowerIp;
        m->IPv4DhcpServerUpperIp = aszDhcpUpperIp;
        LogFunc(("network: %RTnaipv4, dhcpserver:%RTnaipv4, dhcplowerip:%RTnaipv4, dhcpupperip:%RTnaipv4\n", network, dhcpserver, dhcplowerip, dhcpupperip));
    }
    /* we need IPv4NetworkMask for NAT's gw service start */
    netmask.u = RT_H2N_U32(netmask.u);
    RTStrPrintf(aszNetmask, 16, "%RTnaipv4", netmask);
    m->IPv4NetworkMask = aszNetmask;
    LogFlowFunc(("getaway:%RTnaipv4, netmask:%RTnaipv4\n", gateway, netmask));
    return VINF_SUCCESS;
}
