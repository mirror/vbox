/* $Id$ */
/** @file
 * Implementation of INATEngine in VBoxSVC.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

NATEngine::NATEngine():mParent(NULL){}
NATEngine::~NATEngine(){}

HRESULT NATEngine::FinalConstruct()
{
    return S_OK;
}

HRESULT NATEngine::init(Machine *aParent)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    autoInitSpan.setSucceeded();
    m_fModified = false;
    mData.allocate();
    mData->mNetwork.setNull();
    mData->mBindIP.setNull();
    unconst(mParent) = aParent;
    return S_OK;
}

HRESULT NATEngine::init(Machine *aParent, NATEngine *aThat)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    Log(("init that:%p this:%p\n", aThat, this));

    AutoCaller thatCaller (aThat);
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
    unconst(mPeer) = aThat;
    autoInitSpan.setSucceeded();
    return S_OK;
}

HRESULT NATEngine::initCopy (Machine *aParent, NATEngine *aThat)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    Log(("initCopy that:%p this:%p\n", aThat, this));

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

    mData.attachCopy(aThat->mData);
    NATRuleMap::iterator it;
    mNATRules.clear();
    for (it = aThat->mNATRules.begin(); it != aThat->mNATRules.end(); ++it)
    {
        mNATRules.insert(std::make_pair(it->first, it->second));
    }
    unconst(mParent) = aParent;
    autoInitSpan.setSucceeded();
    return S_OK;
}


void NATEngine::FinalRelease()
{
    uninit();    
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
    AssertComRCReturn (autoCaller.rc(), false);

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
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);
    if (m_fModified)
    {
        mData.commit();
        if (mPeer)
        {
            mPeer->mData.attach (mData);
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
         *aSockSnd = mData->mSockRcv;
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
NATEngine::COMGETTER(Redirects) (ComSafeArrayOut (BSTR , aNatRules))
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
        Utf8Str utf = Utf8StrFmt("%s,%d,%s,%d,%s,%d", r.strName.raw(), r.u32Proto, 
                        r.strHostIP.raw(), r.u16HostPort, r.strGuestIP.raw(), r.u16GuestPort);
        utf.cloneTo(&sf[i]);
    }
    sf.detachTo(ComSafeArrayOutArg(aNatRules));
    return S_OK;
}


STDMETHODIMP 
NATEngine::AddRedirect(IN_BSTR aName, PRUint32 aProto, IN_BSTR aBindIp, PRUint16 aHostPort, IN_BSTR aGuestIP, PRUint16 aGuestPort)
{

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str name = aName; 
    settings::NATRule r;
    if (name.isEmpty())
    {
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
        name = Utf8StrFmt("%s_%d_%d", proto, aHostPort, aGuestPort);
    }
    r.strName = name.raw();
    r.u32Proto = aProto;
    r.strHostIP = aBindIp;
    r.u16HostPort = aHostPort;
    r.strGuestIP = aGuestIP;
    r.u16GuestPort = aGuestPort;
    mNATRules.insert(std::make_pair(name, r));
    mParent->setModified(Machine::IsModified_NetworkAdapters);
    m_fModified = true;
    return S_OK;
}

STDMETHODIMP 
NATEngine::RemoveRedirect(IN_BSTR aName)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str rule;
    NATRuleMap::iterator it = mNATRules.find(aName); 
    if (it == mNATRules.end())
        return E_INVALIDARG;
    mData.backup();
    mNATRules.erase(it);
    mParent->setModified(Machine::IsModified_NetworkAdapters);
    m_fModified = true;
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
    mData->mTftpPrefix = data.strTftpPrefix;
    mData->mTftpBootFile = data.strTftpBootFile;
    mData->mTftpNextServer = data.strTftpNextServer;
    /* DNS */
    mData->mDnsPassDomain = data.fDnsPassDomain;
    mData->mDnsProxy = data.fDnsProxy;
    mData->mDnsUseHostResolver = data.fDnsUseHostResolver;
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
    data.strTftpPrefix = mData->mTftpPrefix;
    data.strTftpBootFile = mData->mTftpBootFile;
    data.strTftpNextServer = mData->mTftpNextServer;
    /* DNS */
    data.fDnsPassDomain = mData->mDnsPassDomain;
    data.fDnsProxy = mData->mDnsProxy;
    data.fDnsUseHostResolver = mData->mDnsUseHostResolver;

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
    AssertComRCReturnRC (autoCaller.rc());
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
        Log(("Getter (this:%p) Network: %s\n", this, mData->mNetwork.raw()));
    }
    return S_OK;
}

STDMETHODIMP 
NATEngine::COMSETTER(HostIP) (CBSTR aBindIP)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
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
STDMETHODIMP NATEngine::COMGETTER(HostIP) (BSTR *aBindIP)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mBindIP.isEmpty())
        mData->mBindIP.cloneTo(aBindIP);
    return S_OK;
}


STDMETHODIMP
NATEngine::COMSETTER(TftpPrefix)(IN_BSTR aTftpPrefix)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); 
    if (Bstr(mData->mTftpPrefix) != aTftpPrefix)
    {
        mData.backup();
        mData->mTftpPrefix = aTftpPrefix;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(TftpPrefix)(BSTR *aTftpPrefix)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mTftpPrefix.isEmpty())
    {
        mData->mTftpPrefix.cloneTo(aTftpPrefix);
        Log(("Getter (this:%p) TftpPrefix: %s\n", this, mData->mTftpPrefix.raw()));
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMSETTER(TftpBootFile)(IN_BSTR aTftpBootFile)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); 
    if (Bstr(mData->mTftpBootFile) != aTftpBootFile)
    {
        mData.backup();
        mData->mTftpBootFile = aTftpBootFile;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(TftpBootFile)(BSTR *aTftpBootFile)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mTftpBootFile.isEmpty())
    {
        mData->mTftpBootFile.cloneTo(aTftpBootFile);
        Log(("Getter (this:%p) BootFile: %s\n", this, mData->mTftpBootFile.raw()));
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMSETTER(TftpNextServer)(IN_BSTR aTftpNextServer)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); 
    if (Bstr(mData->mTftpNextServer) != aTftpNextServer)
    {
        mData.backup();
        mData->mTftpNextServer = aTftpNextServer;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}

STDMETHODIMP
NATEngine::COMGETTER(TftpNextServer)(BSTR *aTftpNextServer)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    if (!mData->mTftpNextServer.isEmpty())
    {
        mData->mTftpNextServer.cloneTo(aTftpNextServer);
        Log(("Getter (this:%p) NextServer: %s\n", this, mData->mTftpNextServer.raw()));
    }
    return S_OK;
}
/* DNS */
STDMETHODIMP
NATEngine::COMSETTER(DnsPassDomain) (BOOL aDnsPassDomain)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); 
    
    if (mData->mDnsPassDomain != aDnsPassDomain)
    {
        mData.backup();
        mData->mDnsPassDomain = aDnsPassDomain;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}
STDMETHODIMP
NATEngine::COMGETTER(DnsPassDomain)(BOOL *aDnsPassDomain)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDnsPassDomain = mData->mDnsPassDomain;
    return S_OK;
}
STDMETHODIMP
NATEngine::COMSETTER(DnsProxy)(BOOL aDnsProxy)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); 

    if (mData->mDnsProxy != aDnsProxy)
    {
        mData.backup();
        mData->mDnsProxy = aDnsProxy;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}
STDMETHODIMP
NATEngine::COMGETTER(DnsProxy)(BOOL *aDnsProxy)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aDnsProxy = mData->mDnsProxy;
    return S_OK;
}
STDMETHODIMP
NATEngine::COMGETTER(DnsUseHostResolver)(BOOL *aDnsUseHostResolver)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC (autoCaller.rc());
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); 
    *aDnsUseHostResolver = mData->mDnsUseHostResolver;
    return S_OK;
}
STDMETHODIMP
NATEngine::COMSETTER(DnsUseHostResolver)(BOOL aDnsUseHostResolver)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mDnsUseHostResolver != aDnsUseHostResolver)
    {
        mData.backup();
        mData->mDnsUseHostResolver = aDnsUseHostResolver;
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        m_fModified = true;
    }
    return S_OK;
}
