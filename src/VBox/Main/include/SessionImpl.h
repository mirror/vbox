/** @file
 *
 * VBox Client Session COM Class definition
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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

#ifndef ____H_SESSIONIMPL
#define ____H_SESSIONIMPL

#include "VirtualBoxBase.h"
#include "ConsoleImpl.h"

#ifdef __WIN__
#include "win32/resource.h"
#endif

/** @def VBOX_WITH_SYS_V_IPC_SESSION_WATCHER
 *  Use SYS V IPC for watching a session.
 *  This is defined in the Makefile since it's also used by MachineImpl.h/cpp.
 *
 *  @todo Dmitry, feel free to completely change this (and/or write a better description).
 *        (The same goes for the other darwin changes.)
 */
#ifdef __DOXYGEN__
# define VBOX_WITH_SYS_V_IPC_SESSION_WATCHER
#endif

class ATL_NO_VTABLE Session :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <Session, ISession>,
    public VirtualBoxSupportTranslation <Session>,
#ifdef __WIN__
    public IDispatchImpl<ISession, &IID_ISession, &LIBID_VirtualBox,
                         kTypeLibraryMajorVersion, kTypeLibraryMinorVersion>,
    public CComCoClass<Session, &CLSID_Session>,
#else
    public ISession,
#endif
    public IInternalSessionControl
{
public:

    DECLARE_CLASSFACTORY()

    DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)
    DECLARE_NOT_AGGREGATABLE(Session)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Session)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(IInternalSessionControl)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ISession)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers only for internal purposes
    HRESULT init();
    void uninit (bool aFinalRelease);

    // ISession properties
    STDMETHOD(COMGETTER(State)) (SessionState_T *aState);
    STDMETHOD(COMGETTER(Type)) (SessionType_T *aType);
    STDMETHOD(COMGETTER(Machine)) (IMachine **aMachine);
    STDMETHOD(COMGETTER(Console)) (IConsole **aConsole);

    // ISession methods
    STDMETHOD(Close)();

    // IInternalSessionControl methods
    STDMETHOD(GetPID) (ULONG *aPid);
    STDMETHOD(GetRemoteConsole) (IConsole **aConsole);
    STDMETHOD(AssignMachine) (IMachine *aMachine);
    STDMETHOD(AssignRemoteMachine) (IMachine *aMachine, IConsole *aConsole);
    STDMETHOD(UpdateMachineState) (MachineState_T aMachineState);
    STDMETHOD(Uninitialize)();
    STDMETHOD(OnDVDDriveChange)();
    STDMETHOD(OnFloppyDriveChange)();
    STDMETHOD(OnNetworkAdapterChange)(INetworkAdapter *networkAdapter);
    STDMETHOD(OnVRDPServerChange)();
    STDMETHOD(OnUSBControllerChange)();
    STDMETHOD(OnUSBDeviceAttach) (IUSBDevice *aDevice);
    STDMETHOD(OnUSBDeviceDetach) (INPTR GUIDPARAM aId);
    STDMETHOD(OnShowWindow) (BOOL aCheck, BOOL *aCanShow, ULONG64 *aWinId);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Session"; }

private:

    HRESULT close (bool aFinalRelease, bool aFromServer);
    HRESULT grabIPCSemaphore();
    void releaseIPCSemaphore();

    SessionState_T mState;
    SessionType_T mType;

    ComPtr <IInternalMachineControl> mControl;

    ComObjPtr <Console> mConsole;

    ComPtr <IMachine> mRemoteMachine;
    ComPtr <IConsole> mRemoteConsole;

    ComPtr <IVirtualBox> mVirtualBox;

    // the interprocess semaphore handle (id) for the opened machine
#if defined(__WIN__)
    HANDLE mIPCSem;
    HANDLE mIPCThreadSem;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    int mIPCSem;
#else
# error "PORTME"
#endif
};

#endif // ____H_SESSIONIMPL
