/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef ____H_MACHINEDEBUGGER
#define ____H_MACHINEDEBUGGER

#include "VirtualBoxBase.h"

class Console;

class ATL_NO_VTABLE MachineDebugger :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <MachineDebugger, IMachineDebugger>,
    public VirtualBoxSupportTranslation <MachineDebugger>,
    VBOX_SCRIPTABLE_IMPL(IMachineDebugger)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (MachineDebugger)

    DECLARE_NOT_AGGREGATABLE (MachineDebugger)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(MachineDebugger)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IMachineDebugger)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (MachineDebugger)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *aParent);
    void uninit();

    // IMachineDebugger properties
    STDMETHOD(COMGETTER(Singlestep)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(Singlestep)) (BOOL aEnable);
    STDMETHOD(COMGETTER(RecompileUser)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(RecompileUser)) (BOOL aEnable);
    STDMETHOD(COMGETTER(RecompileSupervisor)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(RecompileSupervisor)) (BOOL aEnable);
    STDMETHOD(COMGETTER(PATMEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(PATMEnabled)) (BOOL aEnable);
    STDMETHOD(COMGETTER(CSAMEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(CSAMEnabled)) (BOOL aEnable);
    STDMETHOD(COMGETTER(LogEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(LogEnabled)) (BOOL aEnable);
    STDMETHOD(COMGETTER(HWVirtExEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMGETTER(HWVirtExNestedPagingEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMGETTER(HWVirtExVPIDEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMGETTER(PAEEnabled)) (BOOL *aEnabled);
    STDMETHOD(COMGETTER(VirtualTimeRate)) (ULONG *aPct);
    STDMETHOD(COMSETTER(VirtualTimeRate)) (ULONG aPct);
    STDMETHOD(COMGETTER(VM)) (ULONG64 *aVm);
    STDMETHOD(InjectNMI)();

    // IMachineDebugger methods
    STDMETHOD(ResetStats (IN_BSTR aPattern));
    STDMETHOD(DumpStats (IN_BSTR aPattern));
    STDMETHOD(GetStats (IN_BSTR aPattern, BOOL aWithDescriptions, BSTR *aStats));


    // "public-private methods"
    void flushQueuedSettings();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"MachineDebugger"; }

private:
    // private methods
    bool queueSettings() const;

    const ComObjPtr <Console, ComWeakRef> mParent;
    // flags whether settings have been queued because
    // they could not be sent to the VM (not up yet, etc.)
    int mSinglestepQueued;
    int mRecompileUserQueued;
    int mRecompileSupervisorQueued;
    int mPatmEnabledQueued;
    int mCsamEnabledQueued;
    int mLogEnabledQueued;
    uint32_t mVirtualTimeRateQueued;
    bool mFlushMode;
};

#endif /* ____H_MACHINEDEBUGGER */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
