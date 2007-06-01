/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "NetworkAdapterImpl.h"
#include "Logging.h"
#include "MachineImpl.h"

#include <VBox/err.h>
#include <iprt/string.h>

// defines
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT NetworkAdapter::FinalConstruct()
{
    return S_OK;
}

void NetworkAdapter::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the network adapter object.
 *
 * @param   parent  handle of our parent object
 * @return  COM result indicator
 */
HRESULT NetworkAdapter::init (Machine *parent, ULONG slot)
{
    LogFlowMember (("NetworkAdapter::init (%p): slot=%d\n", parent, slot));

    ComAssertRet (parent, E_INVALIDARG);
    ComAssertRet (slot < SchemaDefs::NetworkAdapterCount, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    // mPeer is left null

    mData.allocate();

    // initialize data
    mData->mSlot = slot;

    // default to Am79C973
    mData->mAdapterType = NetworkAdapterType_NetworkAdapterAm79C973;

    // generate the MAC address early to guarantee it is the same both after
    // changing some other property (i.e. after mData.backup()) and after the
    // subsequent mData.rollback().
    generateMACAddress();

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the network adapter object given another network adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT NetworkAdapter::init (Machine *parent, NetworkAdapter *that)
{
    LogFlowMember (("NetworkAdapter::init (%p, %p)\n", parent, that));

    ComAssertRet (parent && that, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    mPeer = that;

    AutoLock thatlock (that);
    mData.share (that->mData);

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT NetworkAdapter::initCopy (Machine *parent, NetworkAdapter *that)
{
    LogFlowMember (("NetworkAdapter::initCopy (%p, %p)\n", parent, that));

    ComAssertRet (parent && that, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    // mPeer is left null

    AutoLock thatlock (that);
    mData.attachCopy (that->mData);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void NetworkAdapter::uninit()
{
    LogFlowMember (("INetworkAdapter::uninit()\n"));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    mData.free();

    mPeer.setNull();
    mParent.setNull();

    setReady (false);
}

// INetworkAdapter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP NetworkAdapter::COMGETTER(AdapterType) (NetworkAdapterType_T *adapterType)
{
    if (!adapterType)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *adapterType = mData->mAdapterType;
    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(AdapterType) (NetworkAdapterType_T adapterType)
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    /* make sure the value is allowed */
    switch (adapterType)
    {
        case NetworkAdapterType_NetworkAdapterAm79C970A:
        case NetworkAdapterType_NetworkAdapterAm79C973:
            break;
        default:
            return setError(E_FAIL, tr("Invalid network adapter type '%d'"), adapterType);
    }

    if (mData->mAdapterType != adapterType)
    {
        mData.backup();
        mData->mAdapterType = adapterType;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(Slot) (ULONG *slot)
{
    if (!slot)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *slot = mData->mSlot;
    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(Enabled) (BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *enabled = mData->mEnabled;
    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(Enabled) (BOOL enabled)
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mEnabled != enabled)
    {
        mData.backup();
        mData->mEnabled = enabled;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

/**
 * Returns the MAC address string
 *
 * @returns COM status code
 * @param macAddress address of result variable
 */
STDMETHODIMP NetworkAdapter::COMGETTER(MACAddress)(BSTR *macAddress)
{
    if (!macAddress)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    ComAssertRet (!!mData->mMACAddress, E_FAIL);

    mData->mMACAddress.cloneTo (macAddress);

    return S_OK;
}

/**
 * Sets the MAC address
 *
 * @returns COM status code
 * @param   macAddress 12-digit hexadecimal MAC address string with
 *                     capital letters. Can be NULL to generate a MAC
 */
STDMETHODIMP NetworkAdapter::COMSETTER(MACAddress)(INPTR BSTR macAddress)
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    /*
     * Are we supposed to generate a MAC?
     */
    if (!macAddress)
    {
        mData.backup();
        generateMACAddress();
        emitChangeEvent = true;
    }
    else
    {
        if (mData->mMACAddress != macAddress)
        {
            /*
             * Verify given MAC address
             */
            Utf8Str macAddressUtf = macAddress;
            char *macAddressStr = (char*)macAddressUtf.raw();
            int i = 0;
            while ((i < 12) && macAddressStr && (rc == S_OK))
            {
                char c = *macAddressStr;
                /* we only accept capital letters */
                if (((c < '0') || (c > '9')) &&
                    ((c < 'A') || (c > 'F')))
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
                mData->mMACAddress = macAddress;
                emitChangeEvent = true;
            }
        }
    }

    if (emitChangeEvent)
    {
        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange (this);
    }

    return rc;
}

/**
 * Returns the attachment type
 *
 * @returns COM status code
 * @param   attachmentType address of result variable
 */
STDMETHODIMP NetworkAdapter::COMGETTER(AttachmentType)(NetworkAttachmentType_T *attachmentType)
{
    if (!attachmentType)
        return E_POINTER;
    AutoLock alock(this);
    CHECK_READY();

    *attachmentType = mData->mAttachmentType;

    return S_OK;
}

/**
 * Returns the host interface the adapter is attached to
 *
 * @returns COM status code
 * @param   hostInterface address of result string
 */
STDMETHODIMP NetworkAdapter::COMGETTER(HostInterface)(BSTR *hostInterface)
{
    if (!hostInterface)
        return E_POINTER;
    AutoLock alock(this);
    CHECK_READY();

    mData->mHostInterface.cloneTo(hostInterface);

    return S_OK;
}

/**
 * Sets the host interface device name.
 *
 * @returns COM status code
 * @param   hostInterface name of the host interface device in use
 */
STDMETHODIMP NetworkAdapter::COMSETTER(HostInterface)(INPTR BSTR hostInterface)
{
    /** @todo Validate input string length. r=dmik: do it in XML schema?*/

#ifdef __WIN__
    // we don't allow null strings for the host interface on Win32
    // (because the @name attribute of <HostInerface> must be always present,
    // but can be empty).
    if (!hostInterface)
        return E_INVALIDARG;
#endif
#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
    // empty strings are not allowed as path names
    if (hostInterface && !(*hostInterface))
        return E_INVALIDARG;
#endif

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mHostInterface != hostInterface)
    {
        mData.backup();
        mData->mHostInterface = hostInterface;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
/**
 * Returns the TAP file descriptor the adapter is attached to
 *
 * @returns COM status code
 * @param   tapFileDescriptor address of result string
 */
STDMETHODIMP NetworkAdapter::COMGETTER(TAPFileDescriptor)(LONG *tapFileDescriptor)
{
    if (!tapFileDescriptor)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *tapFileDescriptor = mData->mTAPFD;

    return S_OK;
}

/**
 * Sets the TAP file descriptor the adapter is attached to
 *
 * @returns COM status code
 * @param   tapFileDescriptor file descriptor of existing TAP interface
 */
STDMETHODIMP NetworkAdapter::COMSETTER(TAPFileDescriptor)(LONG tapFileDescriptor)
{
    /*
     * Validate input.
     */
    RTFILE tapFD = tapFileDescriptor;
    if (tapFD != NIL_RTFILE && (LONG)tapFD != tapFileDescriptor)
    {
        AssertMsgFailed(("Invalid file descriptor: %ld.\n", tapFileDescriptor));
        return setError (E_INVALIDARG,
            tr ("Invalid file descriptor: %ld"), tapFileDescriptor);
    }

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mTAPFD != (RTFILE) tapFileDescriptor)
    {
        mData.backup();
        mData->mTAPFD = tapFileDescriptor;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}


/**
 * Returns the current TAP setup application path
 *
 * @returns COM status code
 * @param   tapSetupApplication address of result buffer
 */
STDMETHODIMP NetworkAdapter::COMGETTER(TAPSetupApplication)(BSTR *tapSetupApplication)
{
    if (!tapSetupApplication)
        return E_POINTER;
    AutoLock alock(this);
    CHECK_READY();

    /* we don't have to be in TAP mode to support this call */
    mData->mTAPSetupApplication.cloneTo(tapSetupApplication);

    return S_OK;
}

/**
 * Stores a new TAP setup application
 *
 * @returns COM status code
 * @param   tapSetupApplication new TAP setup application path
 */
STDMETHODIMP NetworkAdapter::COMSETTER(TAPSetupApplication)(INPTR BSTR tapSetupApplication)
{
    // empty strings are not allowed as path names
    if (tapSetupApplication && !(*tapSetupApplication))
        return E_INVALIDARG;

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mTAPSetupApplication != tapSetupApplication)
    {
        mData.backup();
        mData->mTAPSetupApplication = tapSetupApplication;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

/**
 * Returns the current TAP terminate application path
 *
 * @returns COM status code
 * @param   tapTerminateApplication address of result buffer
 */
STDMETHODIMP NetworkAdapter::COMGETTER(TAPTerminateApplication)(BSTR *tapTerminateApplication)
{
    if (!tapTerminateApplication)
        return E_POINTER;
    AutoLock alock(this);
    CHECK_READY();

    /* we don't have to be in TAP mode to support this call */
    mData->mTAPTerminateApplication.cloneTo(tapTerminateApplication);

    return S_OK;
}

/**
 * Stores a new TAP terminate application
 *
 * @returns COM status code
 * @param   tapTerminateApplication new TAP terminate application path
 */
STDMETHODIMP NetworkAdapter::COMSETTER(TAPTerminateApplication)(INPTR BSTR tapTerminateApplication)
{
    // empty strings are not allowed as path names
    if (tapTerminateApplication && !(*tapTerminateApplication))
        return E_INVALIDARG;

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mTAPTerminateApplication != tapTerminateApplication)
    {
        mData.backup();
        mData->mTAPTerminateApplication = tapTerminateApplication;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

#endif /* VBOX_WITH_UNIXY_TAP_NETWORKING */

/**
 * Returns the internal network the adapter is attached to
 *
 * @returns COM status code
 * @param   internalNetwork address of result variable
 */
STDMETHODIMP NetworkAdapter::COMGETTER(InternalNetwork)(BSTR *internalNetwork)
{
    // we don't allow null strings
    if (!internalNetwork)
        return E_POINTER;
    AutoLock alock(this);
    CHECK_READY();

    mData->mInternalNetwork.cloneTo(internalNetwork);

    return S_OK;
}

/**
 * Sets the internal network for attachment.
 *
 * @returns COM status code
 * @param   internalNetwork internal network name
 */
STDMETHODIMP NetworkAdapter::COMSETTER(InternalNetwork)(INPTR BSTR internalNetwork)
{
    if (!internalNetwork)
        return E_INVALIDARG;

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mInternalNetwork != internalNetwork)
    {
        /* if an empty string is to be set, internal networking must be turned off */
        if (   (internalNetwork == Bstr(""))
            && (mData->mAttachmentType = NetworkAttachmentType_InternalNetworkAttachment))
        {
            return setError(E_FAIL, tr("A valid internal network name must be set"));
        }

        mData.backup();
        mData->mInternalNetwork = internalNetwork;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}


/**
 * Return the current cable status
 *
 * @returns COM status code
 * @param   connected address of result variable
 */
STDMETHODIMP NetworkAdapter::COMGETTER(CableConnected)(BOOL *connected)
{
    if (!connected)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *connected = mData->mCableConnected;
    return S_OK;
}

/**
 * Set the cable status flag.
 *
 * @returns COM status code
 * @param   connected new trace flag
 */
STDMETHODIMP NetworkAdapter::COMSETTER(CableConnected)(BOOL connected)
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (connected != mData->mCableConnected)
    {
        mData.backup();
        mData->mCableConnected = connected;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

/**
 * Return the current trace status
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP NetworkAdapter::COMGETTER(TraceEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    *enabled = mData->mTraceEnabled;
    return S_OK;
}

/**
 * Set the trace flag.
 *
 * @returns COM status code
 * @param   enabled new trace flag
 */
STDMETHODIMP NetworkAdapter::COMSETTER(TraceEnabled)(BOOL enabled)
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (enabled != mData->mTraceEnabled)
    {
        mData.backup();
        mData->mTraceEnabled = enabled;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

/**
 * Return the current trace file name
 *
 * @returns COM status code
 * @param   address where to store result
 */
STDMETHODIMP NetworkAdapter::COMGETTER(TraceFile)(BSTR *traceFile)
{
    if (!traceFile)
        return E_POINTER;

    AutoLock alock(this);
    CHECK_READY();

    mData->mTraceFile.cloneTo(traceFile);
    return S_OK;
}

/**
 * Set the trace file name
 *
 * @returns COM status code
 * @param   New trace file name
 */
STDMETHODIMP NetworkAdapter::COMSETTER(TraceFile)(INPTR BSTR traceFile)
{
    if (!traceFile)
        return E_INVALIDARG;

    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY(mParent);

    mData.backup();
    mData->mTraceFile = traceFile;

    /* notify parent */
    alock.unlock();
    mParent->onNetworkAdapterChange(this);

    return S_OK;
}

// INetworkAdapter methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Attach the network card to the NAT interface.
 *
 * @returns COM status code
 */
STDMETHODIMP NetworkAdapter::AttachToNAT()
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mAttachmentType != NetworkAttachmentType_NATNetworkAttachment)
    {
        mData.backup();
        detach();
        mData->mAttachmentType = NetworkAttachmentType_NATNetworkAttachment;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

/**
 * Attach the network card to the defined host interface.
 *
 * @returns COM status code
 */
STDMETHODIMP NetworkAdapter::AttachToHostInterface()
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    /* don't do anything if we're already host interface attached */
    if (mData->mAttachmentType != NetworkAttachmentType_HostInterfaceNetworkAttachment)
    {
        mData.backup();
        /* first detach the current attachment */
        if (mData->mAttachmentType != NetworkAttachmentType_NoNetworkAttachment)
            detach();
        mData->mAttachmentType = NetworkAttachmentType_HostInterfaceNetworkAttachment;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

/**
 * Attach the network card to the defined internal network.
 *
 * @returns COM status code
 */
STDMETHODIMP NetworkAdapter::AttachToInternalNetwork()
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    /* don't do anything if we're already internal network attached */
    if (mData->mAttachmentType != NetworkAttachmentType_InternalNetworkAttachment)
    {
        /* there must an internal network name */
        if (   !mData->mInternalNetwork
            || (mData->mInternalNetwork == Bstr("")))
        {
            LogRel(("Internal network name not defined, setting to default \"intnet\"\n"));
            HRESULT rc = COMSETTER(InternalNetwork)(Bstr("intnet"));
            if (FAILED(rc))
                return rc;
        }

        mData.backup();
        /* first detach the current attachment */
        detach();
        mData->mAttachmentType = NetworkAttachmentType_InternalNetworkAttachment;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

/**
 * Detach the network card from its interface
 *
 * @returns COM status code
 */
STDMETHODIMP NetworkAdapter::Detach()
{
    AutoLock alock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mAttachmentType != NetworkAttachmentType_NoNetworkAttachment)
    {
        mData.backup();
        detach();

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

bool NetworkAdapter::rollback()
{
    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        // we need to check all data to see whether anything will be changed
        // after rollback
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

void NetworkAdapter::commit()
{
    AutoLock alock (this);
    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            AutoLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }
}

void NetworkAdapter::copyFrom (NetworkAdapter *aThat)
{
    AutoLock alock (this);

    // this will back up current data
    mData.assignCopy (aThat->mData);
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Worker routine for detach handling. No locking, no notifications.
 */
void NetworkAdapter::detach()
{
    switch (mData->mAttachmentType)
    {
        case NetworkAttachmentType_NoNetworkAttachment:
        {
            /* nothing to do here */
            break;
        }
        case NetworkAttachmentType_NATNetworkAttachment:
        {
            break;
        }
        case NetworkAttachmentType_HostInterfaceNetworkAttachment:
        {
            /* reset handle and device name */
#ifdef __WIN__
            mData->mHostInterface = "";
#endif
#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
            mData->mHostInterface.setNull();
            mData->mTAPFD = NIL_RTFILE;
#endif
            break;
        }
        case NetworkAttachmentType_InternalNetworkAttachment:
        {
            mData->mInternalNetwork.setNull();
            break;
        }
    }

    mData->mAttachmentType = NetworkAttachmentType_NoNetworkAttachment;
}

/**
 * Generates a new unique MAC address based on our vendor ID and
 * parts of a GUID
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
    RTStrPrintf(strMAC, sizeof(strMAC), "080027%02X%02X%02X", guid.ptr()->au8[0], guid.ptr()->au8[1], guid.ptr()->au8[2]);
    LogFlowThisFunc (("Generated '%s'\n", strMAC));
    mData->mMACAddress = strMAC;
}
