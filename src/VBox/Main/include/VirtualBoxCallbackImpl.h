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

#ifndef ____H_VIRTUALBOXCALLBACKIMPL
#define ____H_VIRTUALBOXCALLBACKIMPL

#include "VirtualBoxBase.h"
#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif

class ATL_NO_VTABLE CallbackWrapper :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<CallbackWrapper, IVirtualBoxCallback>,
    public VirtualBoxSupportTranslation<CallbackWrapper>,
    VBOX_SCRIPTABLE_IMPL(ILocalOwner),
    VBOX_SCRIPTABLE_IMPL(IConsoleCallback),
    VBOX_SCRIPTABLE_IMPL(IVirtualBoxCallback)
#ifdef RT_OS_WINDOWS
    , public CComCoClass<CallbackWrapper, &CLSID_CallbackWrapper>
#endif
{
public:

    DECLARE_CLASSFACTORY()

    DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)
    DECLARE_NOT_AGGREGATABLE(CallbackWrapper)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CallbackWrapper)
        COM_INTERFACE_ENTRY2(IDispatch, ILocalOwner)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ILocalOwner)
        COM_INTERFACE_ENTRY(IVirtualBoxCallback)
        COM_INTERFACE_ENTRY(IConsoleCallback)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers only for internal purposes
    HRESULT init();
    void uninit (bool aFinalRelease);

    // ILocalOwner methods
    STDMETHOD(SetLocalObject)(IUnknown *aLocalObject);

    // IVirtualBoxCallback methods
    STDMETHOD(OnMachineStateChange)(IN_BSTR machineId, MachineState_T state);
    STDMETHOD(OnMachineDataChange)(IN_BSTR machineId);
    STDMETHOD(OnExtraDataCanChange)(IN_BSTR machineId, IN_BSTR key, IN_BSTR value,
                                    BSTR *error, BOOL *changeAllowed);
    STDMETHOD(OnExtraDataChange)(IN_BSTR machineId, IN_BSTR key, IN_BSTR value);
    STDMETHOD(OnMediaRegistered) (IN_BSTR mediaId, DeviceType_T mediaType,
                                  BOOL registered);
    STDMETHOD(OnMachineRegistered)(IN_BSTR machineId, BOOL registered);
    STDMETHOD(OnSessionStateChange)(IN_BSTR machineId, SessionState_T state);
    STDMETHOD(OnSnapshotTaken) (IN_BSTR aMachineId, IN_BSTR aSnapshotId);
    STDMETHOD(OnSnapshotDiscarded) (IN_BSTR aMachineId, IN_BSTR aSnapshotId);
    STDMETHOD(OnSnapshotChange) (IN_BSTR aMachineId, IN_BSTR aSnapshotId);
    STDMETHOD(OnGuestPropertyChange)(IN_BSTR machineId, IN_BSTR key, IN_BSTR value, IN_BSTR flags);

    // IConsoleCallback
    STDMETHOD(OnMousePointerShapeChange)(BOOL visible, BOOL alpha, ULONG xHot, ULONG yHot,
                                         ULONG width, ULONG height, BYTE *shape);
    STDMETHOD(OnMouseCapabilityChange)(BOOL supportsAbsolute, BOOL needsHostCursor);
    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock);
    STDMETHOD(OnStateChange)(MachineState_T machineState);
    STDMETHOD(OnAdditionsStateChange)();
    STDMETHOD(OnDVDDriveChange)();
    STDMETHOD(OnFloppyDriveChange)();
    STDMETHOD(OnNetworkAdapterChange) (INetworkAdapter *aNetworkAdapter);
    STDMETHOD(OnSerialPortChange) (ISerialPort *aSerialPort);
    STDMETHOD(OnParallelPortChange) (IParallelPort *aParallelPort);
    STDMETHOD(OnVRDPServerChange)();
    STDMETHOD(OnUSBControllerChange)();
    STDMETHOD(OnUSBDeviceStateChange) (IUSBDevice *aDevice, BOOL aAttached,
                                       IVirtualBoxErrorInfo *aError);
    STDMETHOD(OnSharedFolderChange) (Scope_T aScope);
    STDMETHOD(OnStorageControllerChange) ();
    STDMETHOD(OnRuntimeError)(BOOL fFatal, IN_BSTR id, IN_BSTR message);
    STDMETHOD(OnCanShowWindow)(BOOL *canShow);
    STDMETHOD(OnShowWindow) (ULONG64 *winId);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"CallbackWrapper"; }

private:
    ComPtr<IVirtualBoxCallback> mVBoxCallback;
    ComPtr<IConsoleCallback>    mConsoleCallback;
};
#endif // ____H_VIRTUALBOXCALLBACKIMPL
