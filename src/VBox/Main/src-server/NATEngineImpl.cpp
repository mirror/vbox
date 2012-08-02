/* $Id$ */
/** @file
 * Implementation of INATEngine in VBoxSVC.
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

#include "NATEngineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"
#include "MachineImpl.h"
#include "GuestOSTypeImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>


// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

NATEngine::NATEngine():mParent(NULL), mAdapter(NULL){}
NATEngine::~NATEngine(){}

HRESULT NATEngine::FinalConstruct()
{
    return S_OK;
}

HRESULT NATEngine::init(Machine *aParent, INetworkAdapter *aAdapter)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    autoInitSpan.setSucceeded();
    m_fModified = false;
    mData.allocate();
    mData->mNetwork.setNull();
    mData->mBindIP.setNull();
    unconst(mParent) = aParent;
    unconst(mAdapter) = aAdapter;
    return S_OK;
}

HRESULT NATEngine::init(Machine *aParent, INetworkAdapter *aAdapter, NATEngine *aThat)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    Log(("init that:%p this:%p\n", aThat, this));

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

    mData.share(aThat->mData);
    NATRuleMap::iterator it;
    mNATRules.clear();
    for (it = aThat->mNATRules.begin(); it != aThat->mNATRules.end(); ++it)
    {
        mNATRules.insert(std::make_pair(it->first, it->second));
    }
    unconst(mParent) = aParent;
    unconst(mAdapter) = aAdapter;
    unconst(mPeer) = aThat;
    autoInitSpan.setSucceeded();
    return S_OK;
}

HRESULT NATEngine::initCopy(Machine *aParent, INetworkAdapter *aAdapter, NATEngine *aThat)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    Log(("initCopy that:%p this:%p\n", aThat, this));

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

    mData.attachCopy(aThat->mData);
    NATRuleMap::iterator it;
    mNATRules.clear();
    for (it = aThat->mNATRules.begin(); it != aThat->mNATRules.end(); ++it)
    {
        mNATRules.insert(std::make_pair(it->first, it->second));
    }
    unconst(mAdapter) = aAdapter;
    unconst(mParent) = aParent;
    autoInitSpan.setSucceeded();
    return BaseFinalConstruct();
}


void NATEngine::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

void NATEngine::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mNATRules.clear();
    mData.free();
    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

bool NATEngine::isModified()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    bool fModified = m_fModified;
    return fModified;
}

bool NATEngine::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    bool fChanged = m_fModified;

    if (m_fModified)
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        mData.rollback();
    }
    m_fModified = false;
    return fChanged;
}

void NATEngine::commit()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(mPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);
    if (m_fModified)
    {
        mData.commit();
        if (mPeer)
        {
            mPeer->mData.attach(mData);
            mPeer->mNATRules.clear();
            NATRuleMap::iterator it;
            for (it = mNATRules.begin(); it != mNATRules.end(); ++it)
            {
                mPeer->mNATRules.insert(std::make_pair(it->first, it->second));
            }
        }
    }
    m_fModified = false;
}

STDMETHODIMP
NATEngine::GetNetworkSettings(ULONG *aMtu, ULONG *aSockSnd, ULONG *aSockRcv, ULONG *aTcpWndSnd, ULONG *aTcpWndRcv)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (aMtu)
        *aMtu = mData->mMtu;
    if (aSockSnd)
        *aSockSnd = mData->mSockSnd;
    if (aSockRcv)
         *aSockRcv = mData->mSockRcv;
    if (aTcpWndSnd)
         *aTcpWndSnd = mData->mTcpSnd;
    if (aTcpWndRcv)
         *aTcpWndRcv = mData->mTcpRcv;

    return S_OK;
}

STDMETHODIMP
NATEngine::SetNetworkSettings(ULONG aMtu, ULONG aSockSnd, ULONG aSockRcv, ULONG aTcpWndSnd, ULONG aTcpWndRcv)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (   aMtu || aSockSnd || aSockRcv
        || aTcpWndSnd || aTcpWndRcv)
    {
        mData.backup();
        m_fModified = true;
    }
    if (aMtu)
        mData->mMtu = aMtu;
    if (aSockSnd)
        mData->mSockSnd = aSockSnd;
    if (aSockRcv)
        mData->mSockRcv = aSockSnd;
    if (aTcpWndSnd)
        mData->mTcpSnd = aTcpWndSnd;
    if (aTcpWndRcv)
        mData->mTcpRcv = aTcpWndRcv;

    if (m_fModified)
        mParent->setModified(Machine::IsModified_NetworkAdapters);
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(Redirects)(ComSafeArrayOut(BSTR , aNatRules))
{
    CheckComArgOutSafeArrayPointerValid(aNatRules);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);


    SafeArray<BSTR> sf(mNATRules.size());
    size_t i = 0;
    NATRuleMap::const_iterator it;
    for (it = mNATRules.begin();
         it != mNATRules.end(); ++it, ++i)
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
    sf.detachTo(ComSafeArrayOutArg(aNatRules));
    return S_OK;
}


STDMETHODIMP
NATEngine::AddRedirect(IN_BSTR aName, NATProtocol_T aProto, IN_BSTR aBindIp, USHORT aHostPort, IN_BSTR aGuestIP, USHORT aGuestPort)
{

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str name = aName;
    settings::NATRule r;
    const char *proto;
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
        name = Utf8StrFmt("%s_%d_%d", proto, aHostPort, aGuestPort);

    NATRuleMap::iterator it;
    for (it = mNATRules.begin(); it != mNATRules.end(); ++it)
    {
        r = it->second;
        if (it->first == name)
            return setError(E_INVALIDARG,
                            tr("A NAT rule of this name already exists"));
        if (   r.strHostIP == Utf8Str(aBindIp)
            && r.u16HostPort == aHostPort
            && r.proto == aProto)
            return setError(E_INVALIDARG,
                            tr("A NAT rule for this host port and this host IP already exists"));
    }

    r.strName = name.c_str();
    r.proto = aProto;
    r.strHostIP = aBindIp;
    r.u16HostPort = aHostPort;
    r.strGuestIP = aGuestIP;
    r.u16GuestPort = aGuestPort;
    mNATRules.insert(std::make_pair(name, r));
    mParent->setModified(Machine::IsModified_NetworkAdapters);
    m_fModified = true;

    ULONG ulSlot;
    mAdapter->COMGETTER(Slot)(&ulSlot);

    alock.release();
    mParent->onNATRedirectRuleChange(ulSlot, FALSE, Bstr(name).raw(), aProto, Bstr(r.strHostIP).raw(), r.u16HostPort, Bstr(r.strGuestIP).raw(), r.u16GuestPort);
    return S_OK;
}

STDMETHODIMP
NATEngine::RemoveRedirect(IN_BSTR aName)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    NATRuleMap::iterator it = mNATRules.find(aName);
    if (it == mNATRules.end())
        return E_INVALIDARG;
    mData.backup();
    settings::NATRule r = it->second;
    Utf8Str strHostIP = r.strHostIP;
    Utf8Str strGuestIP = r.strGuestIP;
    NATProtocol_T proto = r.proto;
    uint16_t u16HostPort = r.u16HostPort;
    uint16_t u16GuestPort = r.u16GuestPort;
    ULONG ulSlot;
    mAdapter->COMGETTER(Slot)(&ulSlot);

    mNATRules.erase(it);
    mParent->setModified(Machine::IsModified_NetworkAdapters);
    m_fModified = true;
    alock.release();
    mParent->onNATRedirectRuleChange(ulSlot, TRUE, aName, proto, Bstr(strHostIP).raw(), u16HostPort, Bstr(strGuestIP).raw(), u16GuestPort);
    return S_OK;
}

HRESULT NATEngine::loadSettings(const settings::NAT &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = S_OK;
    mData->mNetwork = data.strNetwork;
    mData->mBindIP = data.strBindIP;
    mData->mMtu = data.u32Mtu;
    mData->mSockSnd = data.u32SockSnd;
    mData->mTcpRcv = data.u32TcpRcv;
    mData->mTcpSnd = data.u32TcpSnd;
    /* TFTP */
    mData->mTFTPPrefix = data.strTFTPPrefix;
    mData->mTFTPBootFile = data.strTFTPBootFile;
    mData->mTFTPNextServer = data.strTFTPNextServer;
    /* DNS */
    mData->mDNSPassDomain = data.fDNSPassDomain;
    mData->mDNSProxy = data.fDNSProxy;
    mData->mDNSUseHostResolver = data.fDNSUseHostResolver;
    /* Alias */
    mData->mAliasMode  = (data.fAliasUseSamePorts ? NATAliasMode_AliasUseSamePorts : 0);
    mData->mAliasMode |= (data.fAliasLog          ? NATAliasMode_AliasLog          : 0);
    mData->mAliasMode |= (data.fAliasProxyOnly    ? NATAliasMode_AliasProxyOnly    : 0);
    /* port forwarding */
    mNATRules.clear();
    for (settings::NATRuleList::const_iterator it = data.llRules.begin();
        it != data.llRules.end(); ++it)
    {
        mNATRules.insert(std::make_pair(it->strName, *it));
    }
    m_fModified = false;
    return rc;
}


HRESULT NATEngine::saveSettings(settings::NAT &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = S_OK;
    data.strNetwork = mData->mNetwork;
    data.strBindIP = mData->mBindIP;
    data.u32Mtu = mData->mMtu;
    data.u32SockRcv = mData->mSockRcv;
    data.u32SockSnd = mData->mSockSnd;
    data.u32TcpRcv = mData->mTcpRcv;
    data.u32TcpSnd = mData->mTcpSnd;
    /* TFTP */
    data.strTFTPPrefix = mData->mTFTPPrefix;
    data.strTFTPBootFile = mData->mTFTPBootFile;
    data.strTFTPNextServer = mData->mTFTPNextServer;
    /* DNS */
    data.fDNSPassDomain = !!mData->mDNSPassDomain;
    data.fDNSProxy = !!mData->mDNSProxy;
    data.fDNSUseHostResolver = !!mData->mDNSUseHostResolver;
    /* Alias */
    data.fAliasLog = !!(mData->mAliasMode & NATAliasMode_AliasLog);
    data.fAliasProxyOnly = !!(mData->mAliasMode & NATAliasMode_AliasProxyOnly);
    data.fAliasUseSamePorts = !!(mData->mAliasMode & NATAliasMode_AliasUseSamePorts);

    for (NATRuleMap::iterator it = mNATRules.begin();
        it != mNATRules.end(); ++it)
        data.llRules.push_back(it->second);
    m_fModified = false;
    return rc;
}


STDMETHODIMP
NATEngine::COMSETTER(Network)(IN_BSTR aNetwork)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->mNetwork) != aNetwork)
    {
        mData.backup();
        mData->mNetwork = aNetwork;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(Network)(BSTR *aNetwork)
{
    CheckComArgNotNull(aNetwork);
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mNetwork.isEmpty())
    {
        mData->mNetwork.cloneTo(aNetwork);
        Log(("Getter (this:%p) Network: %s\n", this, mData->mNetwork.c_str()));
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMSETTER(HostIP)(IN_BSTR aBindIP)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->mBindIP) != aBindIP)
    {
        mData.backup();
        mData->mBindIP = aBindIP;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}
STDMETHODIMP NATEngine::COMGETTER(HostIP)(BSTR *aBindIP)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mBindIP.isEmpty())
        mData->mBindIP.cloneTo(aBindIP);
    return S_OK;
}


STDMETHODIMP
NATEngine::COMSETTER(TFTPPrefix)(IN_BSTR aTFTPPrefix)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->mTFTPPrefix) != aTFTPPrefix)
    {
        mData.backup();
        mData->mTFTPPrefix = aTFTPPrefix;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(TFTPPrefix)(BSTR *aTFTPPrefix)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mTFTPPrefix.isEmpty())
    {
        mData->mTFTPPrefix.cloneTo(aTFTPPrefix);
        Log(("Getter (this:%p) TFTPPrefix: %s\n", this, mData->mTFTPPrefix.c_str()));
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMSETTER(TFTPBootFile)(IN_BSTR aTFTPBootFile)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->mTFTPBootFile) != aTFTPBootFile)
    {
        mData.backup();
        mData->mTFTPBootFile = aTFTPBootFile;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(TFTPBootFile)(BSTR *aTFTPBootFile)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mTFTPBootFile.isEmpty())
    {
        mData->mTFTPBootFile.cloneTo(aTFTPBootFile);
        Log(("Getter (this:%p) BootFile: %s\n", this, mData->mTFTPBootFile.c_str()));
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMSETTER(TFTPNextServer)(IN_BSTR aTFTPNextServer)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->mTFTPNextServer) != aTFTPNextServer)
    {
        mData.backup();
        mData->mTFTPNextServer = aTFTPNextServer;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(TFTPNextServer)(BSTR *aTFTPNextServer)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mTFTPNextServer.isEmpty())
    {
        mData->mTFTPNextServer.cloneTo(aTFTPNextServer);
        Log(("Getter (this:%p) NextServer: %s\n", this, mData->mTFTPNextServer.c_str()));
    }
    return S_OK;
}
/* DNS */
STDMETHODIMP
NATEngine::COMSETTER(DNSPassDomain) (BOOL aDNSPassDomain)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mDNSPassDomain != aDNSPassDomain)
    {
        mData.backup();
        mData->mDNSPassDomain = aDNSPassDomain;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}
STDMETHODIMP
NATEngine::COMGETTER(DNSPassDomain)(BOOL *aDNSPassDomain)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSPassDomain = mData->mDNSPassDomain;
    return S_OK;
}
STDMETHODIMP
NATEngine::COMSETTER(DNSProxy)(BOOL aDNSProxy)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mDNSProxy != aDNSProxy)
    {
        mData.backup();
        mData->mDNSProxy = aDNSProxy;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}
STDMETHODIMP
NATEngine::COMGETTER(DNSProxy)(BOOL *aDNSProxy)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSProxy = mData->mDNSProxy;
    return S_OK;
}
STDMETHODIMP
NATEngine::COMGETTER(DNSUseHostResolver)(BOOL *aDNSUseHostResolver)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSUseHostResolver = mData->mDNSUseHostResolver;
    return S_OK;
}
STDMETHODIMP
NATEngine::COMSETTER(DNSUseHostResolver)(BOOL aDNSUseHostResolver)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mDNSUseHostResolver != aDNSUseHostResolver)
    {
        mData.backup();
        mData->mDNSUseHostResolver = aDNSUseHostResolver;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP NATEngine::COMSETTER(AliasMode)(ULONG aAliasMode)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mAliasMode != aAliasMode)
    {
        mData.backup();
        mData->mAliasMode = aAliasMode;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP NATEngine::COMGETTER(AliasMode)(ULONG *aAliasMode)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aAliasMode = mData->mAliasMode;
    return S_OK;
}

