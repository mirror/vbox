/* $Id$ */
/** @file
 * Implementation of INATEngine in VBoxSVC.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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
#include <VBox/com/array.h>

struct NATEngineData
{
    NATEngineData() : mMtu(0),
             mSockRcv(0),
             mSockSnd(0),
             mTcpRcv(0),
             mTcpSnd(0),
             mDNSPassDomain(TRUE),
             mDNSProxy(FALSE),
             mDNSUseHostResolver(FALSE),
             mAliasMode(0)
    {}

    com::Utf8Str mNetwork;
    com::Utf8Str mBindIP;
    uint32_t mMtu;
    uint32_t mSockRcv;
    uint32_t mSockSnd;
    uint32_t mTcpRcv;
    uint32_t mTcpSnd;
    /* TFTP service */
    Utf8Str mTFTPPrefix;
    Utf8Str mTFTPBootFile;
    Utf8Str mTFTPNextServer;
    /* DNS service */
    BOOL mDNSPassDomain;
    BOOL mDNSProxy;
    BOOL mDNSUseHostResolver;
    /* Alias service */
    ULONG mAliasMode;
};

struct NATEngine::Data
{
    Backupable<NATEngineData> m;
};


// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

NATEngine::NATEngine():mData(NULL), mParent(NULL), mAdapter(NULL) {}
NATEngine::~NATEngine(){}

HRESULT NATEngine::FinalConstruct()
{
    return BaseFinalConstruct();
}

void NATEngine::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}


HRESULT NATEngine::init(Machine *aParent, INetworkAdapter *aAdapter)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    autoInitSpan.setSucceeded();
    m_fModified = false;
    mData = new Data();
    mData->m.allocate();
    mData->m->mNetwork.setNull();
    mData->m->mBindIP.setNull();
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

    mData = new Data();
    mData->m.share(aThat->mData->m);
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

    mData = new Data();
    mData->m.attachCopy(aThat->mData->m);
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


void NATEngine::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mNATRules.clear();
    mData->m.free();
    delete mData;
    mData = NULL;
    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

bool NATEngine::i_isModified()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    bool fModified = m_fModified;
    return fModified;
}

bool NATEngine::i_rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    bool fChanged = m_fModified;

    if (m_fModified)
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        mData->m.rollback();
    }
    m_fModified = false;
    return fChanged;
}

void NATEngine::i_commit()
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
        mData->m.commit();
        if (mPeer)
        {
            mPeer->mData->m.attach(mData->m);
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

HRESULT NATEngine::getNetworkSettings(ULONG *aMtu, ULONG *aSockSnd, ULONG *aSockRcv, ULONG *aTcpWndSnd, ULONG *aTcpWndRcv)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (aMtu)
        *aMtu = mData->m->mMtu;
    if (aSockSnd)
        *aSockSnd = mData->m->mSockSnd;
    if (aSockRcv)
        *aSockRcv = mData->m->mSockRcv;
    if (aTcpWndSnd)
        *aTcpWndSnd = mData->m->mTcpSnd;
    if (aTcpWndRcv)
        *aTcpWndRcv = mData->m->mTcpRcv;

    return S_OK;
}

HRESULT NATEngine::setNetworkSettings(ULONG aMtu, ULONG aSockSnd, ULONG aSockRcv, ULONG aTcpWndSnd, ULONG aTcpWndRcv)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (   aMtu || aSockSnd || aSockRcv
        || aTcpWndSnd || aTcpWndRcv)
    {
        mData->m.backup();
        m_fModified = true;
    }
    if (aMtu)
        mData->m->mMtu = aMtu;
    if (aSockSnd)
        mData->m->mSockSnd = aSockSnd;
    if (aSockRcv)
        mData->m->mSockRcv = aSockSnd;
    if (aTcpWndSnd)
        mData->m->mTcpSnd = aTcpWndSnd;
    if (aTcpWndRcv)
        mData->m->mTcpRcv = aTcpWndRcv;

    if (m_fModified)
        mParent->setModified(Machine::IsModified_NetworkAdapters);
    return S_OK;
}


HRESULT NATEngine::getRedirects(std::vector<com::Utf8Str> &aRedirects)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aRedirects.resize(mNATRules.size());
    size_t i = 0;
    NATRuleMap::const_iterator it;
    for (it = mNATRules.begin(); it != mNATRules.end(); ++it, ++i)
    {
        settings::NATRule r = it->second;
        aRedirects[i] = Utf8StrFmt("%s,%d,%s,%d,%s,%d",
                                   r.strName.c_str(),
                                   r.proto,
                                   r.strHostIP.c_str(),
                                   r.u16HostPort,
                                   r.strGuestIP.c_str(),
                                   r.u16GuestPort);
    }
    return S_OK;
}

HRESULT NATEngine::addRedirect(const com::Utf8Str &aName, NATProtocol_T aProto, const com::Utf8Str &aHostIP,
                               USHORT aHostPort, const com::Utf8Str &aGuestIP, USHORT aGuestPort)
{
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
        if (   r.strHostIP == aHostIP
            && r.u16HostPort == aHostPort
            && r.proto == aProto)
            return setError(E_INVALIDARG,
                            tr("A NAT rule for this host port and this host IP already exists"));
    }

    r.strName = name.c_str();
    r.proto = aProto;
    r.strHostIP = aHostIP;
    r.u16HostPort = aHostPort;
    r.strGuestIP = aGuestIP;
    r.u16GuestPort = aGuestPort;
    mNATRules.insert(std::make_pair(name, r));
    mParent->setModified(Machine::IsModified_NetworkAdapters);
    m_fModified = true;

    ULONG ulSlot;
    mAdapter->COMGETTER(Slot)(&ulSlot);

    alock.release();
    mParent->onNATRedirectRuleChange(ulSlot, FALSE, Bstr(name).raw(), aProto, Bstr(r.strHostIP).raw(),
                                     r.u16HostPort, Bstr(r.strGuestIP).raw(), r.u16GuestPort);
    return S_OK;
}

HRESULT NATEngine::removeRedirect(const com::Utf8Str &aName)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    NATRuleMap::iterator it = mNATRules.find(aName);
    if (it == mNATRules.end())
        return E_INVALIDARG;
    mData->m.backup();
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
    mData->m.commit();
    alock.release();
    mParent->onNATRedirectRuleChange(ulSlot, TRUE, Bstr(aName).raw(), proto, Bstr(strHostIP).raw(),
                                     u16HostPort, Bstr(strGuestIP).raw(), u16GuestPort);
    return S_OK;
}

HRESULT NATEngine::i_loadSettings(const settings::NAT &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = S_OK;
    mData->m->mNetwork = data.strNetwork;
    mData->m->mBindIP = data.strBindIP;
    mData->m->mMtu = data.u32Mtu;
    mData->m->mSockSnd = data.u32SockSnd;
    mData->m->mTcpRcv = data.u32TcpRcv;
    mData->m->mTcpSnd = data.u32TcpSnd;
    /* TFTP */
    mData->m->mTFTPPrefix = data.strTFTPPrefix;
    mData->m->mTFTPBootFile = data.strTFTPBootFile;
    mData->m->mTFTPNextServer = data.strTFTPNextServer;
    /* DNS */
    mData->m->mDNSPassDomain = data.fDNSPassDomain;
    mData->m->mDNSProxy = data.fDNSProxy;
    mData->m->mDNSUseHostResolver = data.fDNSUseHostResolver;
    /* Alias */
    mData->m->mAliasMode  = (data.fAliasUseSamePorts ? NATAliasMode_AliasUseSamePorts : 0);
    mData->m->mAliasMode |= (data.fAliasLog          ? NATAliasMode_AliasLog          : 0);
    mData->m->mAliasMode |= (data.fAliasProxyOnly    ? NATAliasMode_AliasProxyOnly    : 0);
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


HRESULT NATEngine::i_saveSettings(settings::NAT &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = S_OK;
    data.strNetwork = mData->m->mNetwork;
    data.strBindIP = mData->m->mBindIP;
    data.u32Mtu = mData->m->mMtu;
    data.u32SockRcv = mData->m->mSockRcv;
    data.u32SockSnd = mData->m->mSockSnd;
    data.u32TcpRcv = mData->m->mTcpRcv;
    data.u32TcpSnd = mData->m->mTcpSnd;
    /* TFTP */
    data.strTFTPPrefix = mData->m->mTFTPPrefix;
    data.strTFTPBootFile = mData->m->mTFTPBootFile;
    data.strTFTPNextServer = mData->m->mTFTPNextServer;
    /* DNS */
    data.fDNSPassDomain = !!mData->m->mDNSPassDomain;
    data.fDNSProxy = !!mData->m->mDNSProxy;
    data.fDNSUseHostResolver = !!mData->m->mDNSUseHostResolver;
    /* Alias */
    data.fAliasLog = !!(mData->m->mAliasMode & NATAliasMode_AliasLog);
    data.fAliasProxyOnly = !!(mData->m->mAliasMode & NATAliasMode_AliasProxyOnly);
    data.fAliasUseSamePorts = !!(mData->m->mAliasMode & NATAliasMode_AliasUseSamePorts);

    for (NATRuleMap::iterator it = mNATRules.begin();
        it != mNATRules.end(); ++it)
        data.llRules.push_back(it->second);
    m_fModified = false;
    return rc;
}

HRESULT NATEngine::setNetwork(const com::Utf8Str &aNetwork)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->m->mNetwork) != aNetwork)
    {
        mData->m.backup();
        mData->m->mNetwork = aNetwork;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}


HRESULT NATEngine::getNetwork(com::Utf8Str &aNetwork)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->m->mNetwork.isEmpty())
    {
        aNetwork = mData->m->mNetwork;
        Log(("Getter (this:%p) Network: %s\n", this, mData->m->mNetwork.c_str()));
    }
    return S_OK;
}

HRESULT NATEngine::setHostIP(const com::Utf8Str &aHostIP)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->m->mBindIP) != aHostIP)
    {
        mData->m.backup();
        mData->m->mBindIP = aHostIP;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

HRESULT NATEngine::getHostIP(com::Utf8Str &aBindIP)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->m->mBindIP.isEmpty())
        aBindIP = mData->m->mBindIP;
    return S_OK;
}

HRESULT NATEngine::setTFTPPrefix(const com::Utf8Str &aTFTPPrefix)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->m->mTFTPPrefix) != aTFTPPrefix)
    {
        mData->m.backup();
        mData->m->mTFTPPrefix = aTFTPPrefix;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}


HRESULT NATEngine::getTFTPPrefix(com::Utf8Str &aTFTPPrefix)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->m->mTFTPPrefix.isEmpty())
    {
        aTFTPPrefix = mData->m->mTFTPPrefix;
        Log(("Getter (this:%p) TFTPPrefix: %s\n", this, mData->m->mTFTPPrefix.c_str()));
    }
    return S_OK;
}

HRESULT NATEngine::setTFTPBootFile(const com::Utf8Str &aTFTPBootFile)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->m->mTFTPBootFile) != aTFTPBootFile)
    {
        mData->m.backup();
        mData->m->mTFTPBootFile = aTFTPBootFile;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}


HRESULT NATEngine::getTFTPBootFile(com::Utf8Str &aTFTPBootFile)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->m->mTFTPBootFile.isEmpty())
    {
        aTFTPBootFile = mData->m->mTFTPBootFile;
        Log(("Getter (this:%p) BootFile: %s\n", this, mData->m->mTFTPBootFile.c_str()));
    }
    return S_OK;
}


HRESULT NATEngine::setTFTPNextServer(const com::Utf8Str &aTFTPNextServer)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (Bstr(mData->m->mTFTPNextServer) != aTFTPNextServer)
    {
        mData->m.backup();
        mData->m->mTFTPNextServer = aTFTPNextServer;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

HRESULT NATEngine::getTFTPNextServer(com::Utf8Str &aTFTPNextServer)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->m->mTFTPNextServer.isEmpty())
    {
        aTFTPNextServer =  mData->m->mTFTPNextServer;
        Log(("Getter (this:%p) NextServer: %s\n", this, mData->m->mTFTPNextServer.c_str()));
    }
    return S_OK;
}

/* DNS */
HRESULT NATEngine::setDNSPassDomain(BOOL aDNSPassDomain)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->m->mDNSPassDomain != aDNSPassDomain)
    {
        mData->m.backup();
        mData->m->mDNSPassDomain = aDNSPassDomain;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

HRESULT NATEngine::getDNSPassDomain(BOOL *aDNSPassDomain)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSPassDomain = mData->m->mDNSPassDomain;
    return S_OK;
}


HRESULT NATEngine::setDNSProxy(BOOL aDNSProxy)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->m->mDNSProxy != aDNSProxy)
    {
        mData->m.backup();
        mData->m->mDNSProxy = aDNSProxy;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

HRESULT NATEngine::getDNSProxy(BOOL *aDNSProxy)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSProxy = mData->m->mDNSProxy;
    return S_OK;
}


HRESULT NATEngine::getDNSUseHostResolver(BOOL *aDNSUseHostResolver)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDNSUseHostResolver = mData->m->mDNSUseHostResolver;
    return S_OK;
}


HRESULT NATEngine::setDNSUseHostResolver(BOOL aDNSUseHostResolver)
{
    if (mData->m->mDNSUseHostResolver != aDNSUseHostResolver)
    {
        mData->m.backup();
        mData->m->mDNSUseHostResolver = aDNSUseHostResolver;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

HRESULT NATEngine::setAliasMode(ULONG aAliasMode)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->m->mAliasMode != aAliasMode)
    {
        mData->m.backup();
        mData->m->mAliasMode = aAliasMode;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

HRESULT NATEngine::getAliasMode(ULONG *aAliasMode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aAliasMode = mData->m->mAliasMode;
    return S_OK;
}

