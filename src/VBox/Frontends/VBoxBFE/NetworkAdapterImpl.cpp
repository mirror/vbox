/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of NetworkAdapter class
 *
 * This is adapted from frontends/VirtualBox/NetworkAdapter.cpp.
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



#include <VBox/err.h>
#include <iprt/assert.h>

#include <NetworkAdapterImpl.h>
#include <COMDefs.h>
#include <ConsoleImpl.h>

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
    //    CHECK_READY();

    //    mData->mHostInterface.cloneTo(hostInterface);
    mData.mHostInterface.cloneTo(hostInterface);

    return S_OK;
}

NetworkAdapter::NetworkAdapter()
{
  RTCritSectInit(&mCritSec);
}


NetworkAdapter::~NetworkAdapter()
{
}

int
NetworkAdapter::init (Console *parent, ULONG slot)
{
  mParent       = parent;
  mData.mSlot   = slot;
  return S_OK;
}


STDMETHODIMP
NetworkAdapter::COMGETTER(Slot)(ULONG *slot)
{
    if (!slot)
        return E_POINTER;

    AutoLock alock (this);
    //    CHECK_READY();

    *slot = mData.mSlot;
    return S_OK;
}


STDMETHODIMP
NetworkAdapter::COMGETTER(Enabled)(bool *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoLock alock (this);
    //    CHECK_READY();

    *enabled = mData.mEnabled;
    return S_OK;
}


STDMETHODIMP
NetworkAdapter::COMSETTER(Enabled)(BOOL enabled)
{
    AutoLock alock(this);
    //    CHECK_READY();

    //    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData.mEnabled != enabled)
    {
      //        mData.backup();
        mData.mEnabled = enabled;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange (this);
    }

    return S_OK;
}

STDMETHODIMP
NetworkAdapter::COMGETTER(MACAddress)(BSTR *macAddress)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}

STDMETHODIMP
NetworkAdapter::COMSETTER(MACAddress)(INPTR BSTR macAddress)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}


STDMETHODIMP
NetworkAdapter::COMSETTER(HostInterface)(INPTR BSTR hostInterface)
{
#ifdef RT_OS_LINUX
    // empty strings are not allowed as path names
    if (hostInterface && !(*hostInterface))
        return E_INVALIDARG;
#endif


    AutoLock alock(this);
    //    CHECK_READY();

    //    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData.mHostInterface != hostInterface)
    {
      //        mData.backup();
        mData.mHostInterface = hostInterface;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;
}



STDMETHODIMP
NetworkAdapter::COMGETTER(TAPFileDescriptor)(LONG *tapFileDescriptor)
{
    if (!tapFileDescriptor)
        return E_POINTER;

    AutoLock alock(this);
    //    CHECK_READY();

    *tapFileDescriptor = mData.mTAPFD;

    return S_OK;

}

STDMETHODIMP
NetworkAdapter::COMSETTER(TAPFileDescriptor)(LONG tapFileDescriptor)
{
    /*
     * Validate input.
     */
    RTFILE tapFD = tapFileDescriptor;
    if (tapFD != NIL_RTFILE && (LONG)tapFD != tapFileDescriptor)
    {
        AssertMsgFailed(("Invalid file descriptor: %ld.\n", tapFileDescriptor));

        //    setError  VirtualBoxSupportErrorInfoImplBase which
        //       is a parent class of NetworAdapter in the COM flavored version
        //    return setError (E_INVALIDARG,
        //        tr ("Invalid file descriptor: %ld"), tapFileDescriptor);

        return S_OK;

    }

    AutoLock alock(this);
    //    CHECK_READY();

    //    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData.mTAPFD != (RTFILE) tapFileDescriptor)
    {
      //        mData.backup();
        mData.mTAPFD = tapFileDescriptor;

        /* notify parent */
        alock.unlock();
        mParent->onNetworkAdapterChange(this);
    }

    return S_OK;

}

STDMETHODIMP
NetworkAdapter::COMGETTER(TAPSetupApplication)(BSTR *tapSetupApplication)
{
    if (!tapSetupApplication)
        return E_POINTER;
    AutoLock alock(this);
    //    CHECK_READY();

    /* we don't have to be in TAP mode to support this call */
    mData.mTAPSetupApplication.cloneTo(tapSetupApplication);

    return S_OK;

}

STDMETHODIMP
NetworkAdapter::COMSETTER(TAPSetupApplication)(INPTR BSTR tapSetupApplication)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}

STDMETHODIMP
NetworkAdapter::COMGETTER(TAPTerminateApplication)(BSTR *tapTerminateApplication)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}

STDMETHODIMP
NetworkAdapter::COMSETTER(TAPTerminateApplication)(INPTR BSTR tapTerminateApplication)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}

STDMETHODIMP
NetworkAdapter::COMGETTER(InternalNetwork)(BSTR *internalNetwork)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::COMSETTER(InternalNetwork)(INPTR BSTR internalNetwork)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::COMGETTER(CableConnected)(BOOL *connected)
{
    if (!connected)
        return E_POINTER;

    AutoLock alock(this);
    //    CHECK_READY();

    *connected = mData.mCableConnected;
    return S_OK;

}
STDMETHODIMP
NetworkAdapter::COMSETTER(CableConnected)(BOOL connected)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::COMGETTER(TraceEnabled)(BOOL *enabled)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::COMSETTER(TraceEnabled)(BOOL enabled)
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}

    // INetworkAdapter methods
STDMETHODIMP
NetworkAdapter::AttachToNAT()
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::AttachToHostInterface()
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::AttachToInternalNetwork()
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}
STDMETHODIMP
NetworkAdapter::Detach()
{
  AssertMsg(0,("Not implemented yet\n"));
  return 0;
}

void
NetworkAdapter::detach()
{
  AssertMsg(0,("Not implemented yet\n"));
}

void
NetworkAdapter::generateMACAddress()
{
  AssertMsg(0,("Not implemented yet\n"));
}
