/** @file
 * VBox Client Session COM Class definition
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_SESSIONIMPL
#define ____H_SESSIONIMPL

#include "VirtualBoxBase.h"
#include "ConsoleImpl.h"

#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif

#ifdef RT_OS_WINDOWS
[threading(free)]
#endif
class ATL_NO_VTABLE Session :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(ISession),
    VBOX_SCRIPTABLE_IMPL(IInternalSessionControl)
#ifdef RT_OS_WINDOWS
    , public CComCoClass<Session, &CLSID_Session>
#endif
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Session, ISession)

    DECLARE_CLASSFACTORY()

    DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)
    DECLARE_NOT_AGGREGATABLE(Session)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Session)
        VBOX_DEFAULT_INTERFACE_ENTRIES(ISession)
        COM_INTERFACE_ENTRY2(IDispatch, IInternalSessionControl)
        COM_INTERFACE_ENTRY(IInternalSessionControl)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers only for internal purposes
    HRESULT init();
    void uninit();

    // ISession properties
    STDMETHOD(COMGETTER(State))(SessionState_T *aState);
    STDMETHOD(COMGETTER(Type))(SessionType_T *aType);
    STDMETHOD(COMGETTER(Machine))(IMachine **aMachine);
    STDMETHOD(COMGETTER(Console))(IConsole **aConsole);

    // ISession methods
    STDMETHOD(UnlockMachine)();

    // IInternalSessionControl methods
    STDMETHOD(GetPID)(ULONG *aPid);
    STDMETHOD(GetRemoteConsole)(IConsole **aConsole);
#ifndef VBOX_WITH_GENERIC_SESSION_WATCHER
    STDMETHOD(AssignMachine)(IMachine *aMachine, LockType_T aLockType, IN_BSTR aTokenId);
#else /* VBOX_WITH_GENERIC_SESSION_WATCHER */
    STDMETHOD(AssignMachine)(IMachine *aMachine, LockType_T aLockType, IToken *aToken);
#endif /* VBOX_WITH_GENERIC_SESSION_WATCHER */
    STDMETHOD(AssignRemoteMachine)(IMachine *aMachine, IConsole *aConsole);
    STDMETHOD(UpdateMachineState)(MachineState_T aMachineState);
    STDMETHOD(Uninitialize)();
    STDMETHOD(OnNetworkAdapterChange)(INetworkAdapter *networkAdapter, BOOL changeAdapter);
    STDMETHOD(OnSerialPortChange)(ISerialPort *serialPort);
    STDMETHOD(OnParallelPortChange)(IParallelPort *parallelPort);
    STDMETHOD(OnStorageControllerChange)();
    STDMETHOD(OnMediumChange)(IMediumAttachment *aMediumAttachment, BOOL aForce);
    STDMETHOD(OnCPUChange)(ULONG aCPU, BOOL aRemove);
    STDMETHOD(OnCPUExecutionCapChange)(ULONG aExecutionCap);
    STDMETHOD(OnVRDEServerChange)(BOOL aRestart);
    STDMETHOD(OnVideoCaptureChange)();
    STDMETHOD(OnUSBControllerChange)();
    STDMETHOD(OnSharedFolderChange)(BOOL aGlobal);
    STDMETHOD(OnClipboardModeChange)(ClipboardMode_T aClipboardMode);
    STDMETHOD(OnDragAndDropModeChange)(DragAndDropMode_T aDragAndDropMode);
    STDMETHOD(OnUSBDeviceAttach)(IUSBDevice *aDevice, IVirtualBoxErrorInfo *aError, ULONG aMaskedIfs);
    STDMETHOD(OnUSBDeviceDetach)(IN_BSTR aId, IVirtualBoxErrorInfo *aError);
    STDMETHOD(OnShowWindow)(BOOL aCheck, BOOL *aCanShow, LONG64 *aWinId);
    STDMETHOD(OnBandwidthGroupChange)(IBandwidthGroup *aBandwidthGroup);
    STDMETHOD(OnStorageDeviceChange)(IMediumAttachment *aMediumAttachment, BOOL aRemove, BOOL aSilent);
    STDMETHOD(AccessGuestProperty)(IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags,
                                   BOOL aIsSetter, BSTR *aRetValue, LONG64 *aRetTimestamp, BSTR *aRetFlags);
    STDMETHOD(EnumerateGuestProperties)(IN_BSTR aPatterns,
                                        ComSafeArrayOut(BSTR, aNames),
                                        ComSafeArrayOut(BSTR, aValues),
                                        ComSafeArrayOut(LONG64, aTimestamps),
                                        ComSafeArrayOut(BSTR, aFlags));
    STDMETHOD(OnlineMergeMedium)(IMediumAttachment *aMediumAttachment,
                                 ULONG aSourceIdx, ULONG aTargetIdx,
                                 IProgress *aProgress);
    STDMETHOD(EnableVMMStatistics)(BOOL aEnable);
    STDMETHOD(PauseWithReason)(Reason_T aReason);
    STDMETHOD(ResumeWithReason)(Reason_T aReason);
    STDMETHOD(SaveStateWithReason)(Reason_T aReason, IProgress **aProgress);

private:

    HRESULT unlockMachine(bool aFinalRelease, bool aFromServer);

    SessionState_T mState;
    SessionType_T mType;

    ComPtr<IInternalMachineControl> mControl;

#ifndef VBOX_COM_INPROC_API_CLIENT
    ComObjPtr<Console> mConsole;
#endif

    ComPtr<IMachine> mRemoteMachine;
    ComPtr<IConsole> mRemoteConsole;

    ComPtr<IVirtualBox> mVirtualBox;

    class ClientTokenHolder;

    ClientTokenHolder *mClientTokenHolder;
};

#endif // !____H_SESSIONIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
