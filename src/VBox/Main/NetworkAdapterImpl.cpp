/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "NetworkAdapterImpl.h"
#include "Logging.h"
#include "MachineImpl.h"
#include "GuestOSTypeImpl.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

#include <VBox/err.h>
#include <VBox/settings.h>

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (NetworkAdapter)

HRESULT NetworkAdapter::FinalConstruct()
{
    return S_OK;
}

void NetworkAdapter::FinalRelease()
{
    uninit ();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the network adapter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT NetworkAdapter::init (Machine *aParent, ULONG aSlot)
{
    LogFlowThisFunc (("aParent=%p, aSlot=%d\n", aParent, aSlot));

    ComAssertRet (aParent, E_INVALIDARG);
    ComAssertRet (aSlot < SchemaDefs::NetworkAdapterCount, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    /* initialize data */
    mData->mSlot = aSlot;

    /* default to Am79C973 */
    mData->mAdapterType = NetworkAdapterType_Am79C973;

    /* generate the MAC address early to guarantee it is the same both after
     * changing some other property (i.e. after mData.backup()) and after the
     * subsequent mData.rollback(). */
    generateMACAddress();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the network adapter object given another network adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT NetworkAdapter::init (Machine *aParent, NetworkAdapter *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    unconst (mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReadLock thatLock (aThat);
    mData.share (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT NetworkAdapter::initCopy (Machine *aParent, NetworkAdapter *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReadLock thatLock (aThat);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void NetworkAdapter::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// INetworkAdapter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP NetworkAdapter::COMGETTER(AdapterType) (NetworkAdapterType_T *aAdapterType)
{
    CheckComArgOutPointerValid(aAdapterType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aAdapterType = mData->mAdapterType;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(AdapterType) (NetworkAdapterType_T aAdapterType)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    /* make sure the value is allowed */
    switch (aAdapterType)
    {
        case NetworkAdapterType_Am79C970A:
        case NetworkAdapterType_Am79C973:
#ifdef VBOX_WITH_E1000
        case NetworkAdapterType_I82540EM:
        case NetworkAdapterType_I82543GC:
#endif
            break;
        default:
            return setError (E_FAIL,
                tr("Invalid network adapter type '%d'"),
                aAdapterType);
    }

    if (mData->mAdapterType != aAdapterType)
    {
        mData.backup();
        mData->mAdapterType = aAdapterType;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(Slot) (ULONG *aSlot)
{
    CheckComArgOutPointerValid(aSlot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSlot = mData->mSlot;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(MACAddress)(BSTR *aMACAddress)
{
    CheckComArgOutPointerValid(aMACAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    ComAssertRet (!!mData->mMACAddress, E_FAIL);

    mData->mMACAddress.cloneTo (aMACAddress);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(MACAddress)(IN_BSTR aMACAddress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    /*
     * Are we supposed to generate a MAC?
     */
    if (!aMACAddress)
    {
        mData.backup();

        generateMACAddress();
        emitChangeEvent = true;
    }
    else
    {
        if (mData->mMACAddress != aMACAddress)
        {
            /*
             * Verify given MAC address
             */
            Utf8Str macAddressUtf = aMACAddress;
            char *macAddressStr = macAddressUtf.mutableRaw();
            int i = 0;
            while ((i < 13) && macAddressStr && *macAddressStr && (rc == S_OK))
            {
                char c = *macAddressStr;
                /* canonicalize hex digits to capital letters */
                if (c >= 'a' && c <= 'f')
                {
                    /** @todo the runtime lacks an ascii lower/upper conv */
                    c &= 0xdf;
                    *macAddressStr = c;
                }
                /* we only accept capital letters */
                if (((c < '0') || (c > '9')) &&
                    ((c < 'A') || (c > 'F')))
                    rc = setError(E_INVALIDARG, tr("Invalid MAC address format"));
                /* the second digit must have even value for unicast addresses */
                if ((i == 1) && (!!(c & 1) == (c >= '0' && c <= '9')))
                    rc = setError(E_INVALIDARG, tr("Invalid MAC address format"));

                macAddressStr++;
                i++;
            }
            /* we must have parsed exactly 12 characters */
            if (i != 12)
                rc = setError(E_INVALIDARG, tr("Invalid MAC address format"));

            if (SUCCEEDED (rc))
            {
                mData.backup();

                mData->mMACAddress = macAddressUtf;
                emitChangeEvent = true;
            }
        }
    }

    if (emitChangeEvent)
    {
        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return rc;
}

STDMETHODIMP NetworkAdapter::COMGETTER(AttachmentType)(
    NetworkAttachmentType_T *aAttachmentType)
{
    CheckComArgOutPointerValid(aAttachmentType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aAttachmentType = mData->mAttachmentType;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(HostInterface)(BSTR *aHostInterface)
{
    CheckComArgOutPointerValid(aHostInterface);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mHostInterface.cloneTo (aHostInterface);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(HostInterface)(IN_BSTR aHostInterface)
{
    /** @todo Validate input string length. r=dmik: do it in XML schema?*/

    /* we don't allow null strings for the host interface (because the @name
     * attribute of <HostInterface> must be always present but can be empty). */
    CheckComArgNotNull (aHostInterface);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mHostInterface != aHostInterface)
    {
        mData.backup();
        mData->mHostInterface = aHostInterface;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(InternalNetwork) (BSTR *aInternalNetwork)
{
    CheckComArgOutPointerValid(aInternalNetwork);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mInternalNetwork.cloneTo (aInternalNetwork);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(InternalNetwork) (IN_BSTR aInternalNetwork)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mInternalNetwork != aInternalNetwork)
    {
        /* if an empty/null string is to be set, internal networking must be
         * turned off */
        if (   (aInternalNetwork == NULL || *aInternalNetwork == '\0')
            && mData->mAttachmentType == NetworkAttachmentType_Internal)
        {
            return setError (E_FAIL,
                tr ("Empty or null internal network name is not valid"));
        }

        mData.backup();
        mData->mInternalNetwork = aInternalNetwork;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(NATNetwork) (BSTR *aNATNetwork)
{
    CheckComArgOutPointerValid(aNATNetwork);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mNATNetwork.cloneTo (aNATNetwork);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(NATNetwork) (IN_BSTR aNATNetwork)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mNATNetwork != aNATNetwork)
    {
        mData.backup();
        mData->mNATNetwork = aNATNetwork;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(CableConnected) (BOOL *aConnected)
{
    CheckComArgOutPointerValid(aConnected);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aConnected = mData->mCableConnected;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(CableConnected) (BOOL aConnected)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (aConnected != mData->mCableConnected)
    {
        mData.backup();
        mData->mCableConnected = aConnected;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(LineSpeed) (ULONG *aSpeed)
{
    CheckComArgOutPointerValid(aSpeed);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSpeed = mData->mLineSpeed;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(LineSpeed) (ULONG aSpeed)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (aSpeed != mData->mLineSpeed)
    {
        mData.backup();
        mData->mLineSpeed = aSpeed;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(TraceEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aEnabled = mData->mTraceEnabled;
    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(TraceEnabled) (BOOL aEnabled)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (aEnabled != mData->mTraceEnabled)
    {
        mData.backup();
        mData->mTraceEnabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(TraceFile) (BSTR *aTraceFile)
{
    CheckComArgOutPointerValid(aTraceFile);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mTraceFile.cloneTo (aTraceFile);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(TraceFile) (IN_BSTR aTraceFile)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mTraceFile != aTraceFile)
    {
        mData.backup();
        mData->mTraceFile = aTraceFile;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

// INetworkAdapter methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP NetworkAdapter::AttachToNAT()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mAttachmentType != NetworkAttachmentType_NAT)
    {
        mData.backup();

        detach();

        mData->mAttachmentType = NetworkAttachmentType_NAT;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToBridgedInterface()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    /* don't do anything if we're already host interface attached */
    if (mData->mAttachmentType != NetworkAttachmentType_Bridged)
    {
        mData.backup();

        /* first detach the current attachment */
        detach();

        mData->mAttachmentType = NetworkAttachmentType_Bridged;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToInternalNetwork()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    /* don't do anything if we're already internal network attached */
    if (mData->mAttachmentType != NetworkAttachmentType_Internal)
    {
        mData.backup();

        /* first detach the current attachment */
        detach();

        /* there must an internal network name */
        if (mData->mInternalNetwork.isEmpty())
        {
            LogRel (("Internal network name not defined, "
                     "setting to default \"intnet\"\n"));
            mData->mInternalNetwork = "intnet";
        }

        mData->mAttachmentType = NetworkAttachmentType_Internal;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToHostOnlyInterface()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    /* don't do anything if we're already host interface attached */
    if (mData->mAttachmentType != NetworkAttachmentType_HostOnly)
    {
        mData.backup();

        /* first detach the current attachment */
        detach();

        mData->mAttachmentType = NetworkAttachmentType_HostOnly;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::Detach()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (mData->mAttachmentType != NetworkAttachmentType_Null)
    {
        mData.backup();

        detach();

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given adapter node.
 *  May be called once right after this object creation.
 *
 *  @param aAdapterNode <Adapter> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT NetworkAdapter::loadSettings (const settings::Key &aAdapterNode)
{
    using namespace settings;

    AssertReturn (!aAdapterNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    HRESULT rc = S_OK;

    /* type (optional, defaults to Am79C970A) */
    const char *adapterType = aAdapterNode.stringValue ("type");

    if (strcmp (adapterType, "Am79C970A") == 0)
        mData->mAdapterType = NetworkAdapterType_Am79C970A;
    else if (strcmp (adapterType, "Am79C973") == 0)
        mData->mAdapterType = NetworkAdapterType_Am79C973;
    else if (strcmp (adapterType, "82540EM") == 0)
        mData->mAdapterType = NetworkAdapterType_I82540EM;
    else if (strcmp (adapterType, "82543GC") == 0)
        mData->mAdapterType = NetworkAdapterType_I82543GC;
    else
        ComAssertMsgFailedRet (("Invalid adapter type '%s'", adapterType),
                               E_FAIL);

    /* enabled (required) */
    mData->mEnabled = aAdapterNode.value <bool> ("enabled");
    /* MAC address (can be null) */
    rc = COMSETTER(MACAddress) (Bstr (aAdapterNode.stringValue ("MACAddress")));
    CheckComRCReturnRC (rc);
    /* cable (required) */
    mData->mCableConnected = aAdapterNode.value <bool> ("cable");
    /* line speed (defaults to 100 Mbps) */
    mData->mLineSpeed = aAdapterNode.value <ULONG> ("speed");
    /* tracing (defaults to false) */
    mData->mTraceEnabled = aAdapterNode.value <bool> ("trace");
    mData->mTraceFile = aAdapterNode.stringValue ("tracefile");

    /* One of NAT, HostInerface, Internal or nothing */
    Key attachmentNode;

    if (!(attachmentNode = aAdapterNode.findKey ("NAT")).isNull())
    {
        /* NAT */

        /* optional */
        mData->mNATNetwork = attachmentNode.stringValue ("network");

        rc = AttachToNAT();
        CheckComRCReturnRC (rc);
    }
    else
    if (!(attachmentNode = aAdapterNode.findKey ("HostInterface")).isNull() /* backwards compatibility */
            || !(attachmentNode = aAdapterNode.findKey ("BridgedInterface")).isNull())
    {
        /* Host Interface Networking */

        Bstr name = attachmentNode.stringValue ("name");
        /* name can be empty, but not null */
        ComAssertRet (!name.isNull(), E_FAIL);

        rc = COMSETTER(HostInterface) (name);
        CheckComRCReturnRC (rc);

        rc = AttachToBridgedInterface();
        CheckComRCReturnRC (rc);
    }
    else
    if (!(attachmentNode = aAdapterNode.findKey ("InternalNetwork")).isNull())
    {
        /* Internal Networking */

        /* required */
        mData->mInternalNetwork = attachmentNode.stringValue ("name");
        Assert (!mData->mInternalNetwork.isNull());

        rc = AttachToInternalNetwork();
        CheckComRCReturnRC (rc);
    }
    else
    if (!(attachmentNode = aAdapterNode.findKey ("HostOnlyInterface")).isNull())
    {
#if defined(VBOX_WITH_NETFLT)
        Bstr name = attachmentNode.stringValue ("name");
        /* name can be empty, but not null */
        ComAssertRet (!name.isNull(), E_FAIL);

        rc = COMSETTER(HostInterface) (name);
        CheckComRCReturnRC (rc);
#endif

        /* Host Interface Networking */
        rc = AttachToHostOnlyInterface();
        CheckComRCReturnRC (rc);
    }
    else
    {
        /* Adapter has no children */
        rc = Detach();
        CheckComRCReturnRC (rc);
    }

    return S_OK;
}

/**
 *  Saves settings to the given adapter node.
 *
 *  Note that the given Adapter node is comletely empty on input.
 *
 *  @param aAdapterNode <Adapter> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT NetworkAdapter::saveSettings (settings::Key &aAdapterNode)
{
    using namespace settings;

    AssertReturn (!aAdapterNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    aAdapterNode.setValue <bool> ("enabled", !!mData->mEnabled);
    aAdapterNode.setValue <Bstr> ("MACAddress", mData->mMACAddress);
    aAdapterNode.setValue <bool> ("cable", !!mData->mCableConnected);

    aAdapterNode.setValue <ULONG> ("speed", mData->mLineSpeed);

    if (mData->mTraceEnabled)
        aAdapterNode.setValue <bool> ("trace", true);

    aAdapterNode.setValueOr <Bstr> ("tracefile", mData->mTraceFile, Bstr::Null);

    const char *typeStr = NULL;
    switch (mData->mAdapterType)
    {
        case NetworkAdapterType_Am79C970A:
            typeStr = "Am79C970A";
            break;
        case NetworkAdapterType_Am79C973:
            typeStr = "Am79C973";
            break;
        case NetworkAdapterType_I82540EM:
            typeStr = "82540EM";
            break;
        case NetworkAdapterType_I82543GC:
            typeStr = "82543GC";
            break;
        default:
            ComAssertMsgFailedRet (("Invalid network adapter type: %d",
                                    mData->mAdapterType),
                                   E_FAIL);
    }
    aAdapterNode.setStringValue ("type", typeStr);

    switch (mData->mAttachmentType)
    {
        case NetworkAttachmentType_Null:
        {
            /* do nothing -- empty content */
            break;
        }
        case NetworkAttachmentType_NAT:
        {
            Key attachmentNode = aAdapterNode.createKey ("NAT");
            if (!mData->mNATNetwork.isEmpty())
                attachmentNode.setValue <Bstr> ("network",
                                                mData->mNATNetwork);
            break;
        }
        case NetworkAttachmentType_Bridged:
        {
            Key attachmentNode = aAdapterNode.createKey ("BridgedInterface");
            Assert (!mData->mHostInterface.isNull());
            attachmentNode.setValue <Bstr> ("name", mData->mHostInterface);
            break;
        }
        case NetworkAttachmentType_Internal:
        {
            Key attachmentNode = aAdapterNode.createKey ("InternalNetwork");
            Assert (!mData->mInternalNetwork.isEmpty());
            attachmentNode.setValue <Bstr> ("name", mData->mInternalNetwork);
            break;
        }
        case NetworkAttachmentType_HostOnly:
        {
            Key attachmentNode = aAdapterNode.createKey ("HostOnlyInterface");
#if defined(VBOX_WITH_NETFLT)
            Assert (!mData->mHostInterface.isNull());
            attachmentNode.setValue <Bstr> ("name", mData->mHostInterface);
#endif
            break;
        }
        default:
        {
            ComAssertFailedRet (E_FAIL);
        }
    }

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
bool NetworkAdapter::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoWriteLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void NetworkAdapter::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock (mPeer, this);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void NetworkAdapter::copyFrom (NetworkAdapter *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoMultiLock2 alock (aThat->rlock(), this->wlock());

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

void NetworkAdapter::applyDefaults (GuestOSType *aOsType)
{
    AssertReturnVoid (aOsType != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    bool e1000enabled = false;
#ifdef VBOX_WITH_E1000
    e1000enabled = true;
#endif // VBOX_WITH_E1000

    NetworkAdapterType_T defaultType = aOsType->networkAdapterType();

    /* Set default network adapter for this OS type */
    if (defaultType == NetworkAdapterType_I82540EM ||
        defaultType == NetworkAdapterType_I82543GC)
    {
        if (e1000enabled) mData->mAdapterType = defaultType;
    }
    else mData->mAdapterType = defaultType;

    /* Enable and connect the first one adapter to the NAT */
    if (mData->mSlot == 0)
    {
        mData->mEnabled = true;
        mData->mAttachmentType = NetworkAttachmentType_NAT;
        mData->mCableConnected = true;
    }
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Worker routine for detach handling. No locking, no notifications.

 *  @note Must be called from under the object's write lock.
 */
void NetworkAdapter::detach()
{
    AssertReturnVoid (isWriteLockOnCurrentThread());

    switch (mData->mAttachmentType)
    {
        case NetworkAttachmentType_Null:
        {
            /* nothing to do here */
            break;
        }
        case NetworkAttachmentType_NAT:
        {
            break;
        }
        case NetworkAttachmentType_Bridged:
        {
            /* reset handle and device name */
            mData->mHostInterface = "";
            break;
        }
        case NetworkAttachmentType_Internal:
        {
            mData->mInternalNetwork.setNull();
            break;
        }
        case NetworkAttachmentType_HostOnly:
        {
#if defined(VBOX_WITH_NETFLT)
            /* reset handle and device name */
            mData->mHostInterface = "";
#endif
            break;
        }
    }

    mData->mAttachmentType = NetworkAttachmentType_Null;
}

/**
 *  Generates a new unique MAC address based on our vendor ID and
 *  parts of a GUID.
 *
 *  @note Must be called from under the object's write lock or within the init
 *  span.
 */
void NetworkAdapter::generateMACAddress()
{
    /*
     * Our strategy is as follows: the first three bytes are our fixed
     * vendor ID (080027). The remaining 3 bytes will be taken from the
     * start of a GUID. This is a fairly safe algorithm.
     */
    char strMAC[13];
    Guid guid;
    guid.create();
    RTStrPrintf (strMAC, sizeof(strMAC), "080027%02X%02X%02X",
                 guid.ptr()->au8[0], guid.ptr()->au8[1], guid.ptr()->au8[2]);
    LogFlowThisFunc (("generated MAC: '%s'\n", strMAC));
    mData->mMACAddress = strMAC;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
