/** @file
 *
 * VBox Client callback COM Class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include "VirtualBoxCallbackImpl.h"
#include "Logging.h"

HRESULT VirtualBoxCallback::FinalConstruct()
{
    return S_OK;
}

void VirtualBoxCallback::FinalRelease()
{
}

// public initializers/uninitializers only for internal purposes
HRESULT VirtualBoxCallback::init()
{
    return S_OK;
}

void VirtualBoxCallback::uninit(bool aFinalRelease)
{
}

// ILocalOwner methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBoxCallback::SetLocalObject(IUnknown *aCallback)
{
    if (aCallback == NULL)
    {
        mVBoxCallback.setNull();
        mConsoleCallback.setNull();
        return S_OK;
    }

    if (!VALID_PTR (aCallback))
        return E_POINTER;

    mVBoxCallback = aCallback;
    mConsoleCallback = aCallback;

    // or some other error code?
    AssertReturn(!!mVBoxCallback || !!mConsoleCallback, E_FAIL);

    return S_OK;
}

// IVirtualBoxCallback methods
/////////////////////////////////////////////////////////////////////////////
STDMETHODIMP VirtualBoxCallback::OnMachineStateChange(IN_BSTR machineId, MachineState_T state)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnMachineStateChange(machineId, state);
}

STDMETHODIMP VirtualBoxCallback::OnMachineDataChange(IN_BSTR machineId)
{
    AutoReadLock alock(this);

    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnMachineDataChange(machineId);
}


STDMETHODIMP VirtualBoxCallback::OnExtraDataCanChange(IN_BSTR machineId, IN_BSTR key, IN_BSTR value,
                                           BSTR *error, BOOL *changeAllowed)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnExtraDataCanChange(machineId, key, value, error, changeAllowed);
}

STDMETHODIMP VirtualBoxCallback::OnExtraDataChange(IN_BSTR machineId, IN_BSTR key, IN_BSTR value)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnExtraDataChange(machineId, key, value);
}

STDMETHODIMP VirtualBoxCallback::OnMediaRegistered(IN_BSTR mediaId, DeviceType_T mediaType,
                              BOOL registered)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnMediaRegistered(mediaId, mediaType, registered);
}


STDMETHODIMP VirtualBoxCallback::OnMachineRegistered(IN_BSTR aMachineId, BOOL registered)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnMachineRegistered(aMachineId, registered);
}

STDMETHODIMP VirtualBoxCallback::OnSessionStateChange(IN_BSTR aMachineId, SessionState_T state)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnSessionStateChange(aMachineId, state);
}

STDMETHODIMP VirtualBoxCallback::OnSnapshotTaken(IN_BSTR aMachineId, IN_BSTR aSnapshotId)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnSnapshotTaken(aMachineId, aSnapshotId);
}

STDMETHODIMP VirtualBoxCallback::OnSnapshotDiscarded(IN_BSTR aMachineId, IN_BSTR aSnapshotId)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnSnapshotDiscarded(aMachineId, aSnapshotId);
}

STDMETHODIMP VirtualBoxCallback::OnSnapshotChange(IN_BSTR aMachineId, IN_BSTR aSnapshotId)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnSnapshotChange(aMachineId, aSnapshotId);
}

STDMETHODIMP VirtualBoxCallback::OnGuestPropertyChange(IN_BSTR aMachineId, IN_BSTR key, IN_BSTR value, IN_BSTR flags)
{
    if (mVBoxCallback.isNull())
        return S_OK;

    return mVBoxCallback->OnGuestPropertyChange(aMachineId, key, value, flags);
}


STDMETHODIMP VirtualBoxCallback::OnMousePointerShapeChange(BOOL visible, BOOL alpha, ULONG xHot, ULONG yHot,
                                         ULONG width, ULONG height, BYTE *shape)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnMousePointerShapeChange(visible, alpha, xHot, yHot, width, height, shape);
}


STDMETHODIMP VirtualBoxCallback::OnMouseCapabilityChange(BOOL supportsAbsolute, BOOL needsHostCursor)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnMouseCapabilityChange(supportsAbsolute, needsHostCursor);
}

STDMETHODIMP  VirtualBoxCallback::OnKeyboardLedsChange(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnKeyboardLedsChange(fNumLock, fCapsLock, fScrollLock);
}

STDMETHODIMP VirtualBoxCallback::OnStateChange(MachineState_T machineState)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnStateChange(machineState);
}

STDMETHODIMP VirtualBoxCallback::OnAdditionsStateChange()
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnAdditionsStateChange();
}

STDMETHODIMP VirtualBoxCallback::OnDVDDriveChange()
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnDVDDriveChange();
}

STDMETHODIMP VirtualBoxCallback::OnFloppyDriveChange()
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnFloppyDriveChange();
}

STDMETHODIMP VirtualBoxCallback::OnNetworkAdapterChange(INetworkAdapter *aNetworkAdapter)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnNetworkAdapterChange(aNetworkAdapter);
}

STDMETHODIMP VirtualBoxCallback::OnSerialPortChange(ISerialPort *aSerialPort)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnSerialPortChange(aSerialPort);
}

STDMETHODIMP VirtualBoxCallback::OnParallelPortChange(IParallelPort *aParallelPort)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnParallelPortChange(aParallelPort);
}
STDMETHODIMP VirtualBoxCallback::OnVRDPServerChange()
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnVRDPServerChange();
}

STDMETHODIMP VirtualBoxCallback::OnUSBControllerChange()
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnUSBControllerChange();
}

STDMETHODIMP VirtualBoxCallback::OnUSBDeviceStateChange(IUSBDevice *aDevice,
                                                        BOOL aAttached,
                                                        IVirtualBoxErrorInfo *aError)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnUSBDeviceStateChange(aDevice, aAttached, aError);
}

STDMETHODIMP VirtualBoxCallback::OnSharedFolderChange(Scope_T aScope)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnSharedFolderChange(aScope);
}

STDMETHODIMP VirtualBoxCallback::OnStorageControllerChange()
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnStorageControllerChange();
}

STDMETHODIMP VirtualBoxCallback::OnRuntimeError(BOOL fFatal, IN_BSTR id, IN_BSTR message)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnRuntimeError(fFatal, id, message);
}

STDMETHODIMP VirtualBoxCallback::OnCanShowWindow(BOOL *canShow)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnCanShowWindow(canShow);
}

STDMETHODIMP VirtualBoxCallback::OnShowWindow(ULONG64 *winId)
{
    if (mConsoleCallback.isNull())
        return S_OK;

    return mConsoleCallback->OnShowWindow(winId);
}
