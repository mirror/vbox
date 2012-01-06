/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_MACHINEDEBUGGER
#define ____H_MACHINEDEBUGGER

#include "VirtualBoxBase.h"
#include <iprt/log.h>

class Console;

class ATL_NO_VTABLE MachineDebugger :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IMachineDebugger)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (MachineDebugger, IMachineDebugger)

    DECLARE_NOT_AGGREGATABLE (MachineDebugger)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(MachineDebugger)
        VBOX_DEFAULT_INTERFACE_ENTRIES (IMachineDebugger)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (MachineDebugger)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *aParent);
    void uninit();

    // IMachineDebugger properties
    STDMETHOD(COMGETTER(SingleStep))(BOOL *a_pfEnabled);
    STDMETHOD(COMSETTER(SingleStep))(BOOL a_fEnable);
    STDMETHOD(COMGETTER(RecompileUser))(BOOL *a_pfEnabled);
    STDMETHOD(COMSETTER(RecompileUser))(BOOL a_fEnable);
    STDMETHOD(COMGETTER(RecompileSupervisor))(BOOL *a_pfEnabled);
    STDMETHOD(COMSETTER(RecompileSupervisor))(BOOL a_fEnable);
    STDMETHOD(COMGETTER(PATMEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMSETTER(PATMEnabled))(BOOL a_fEnable);
    STDMETHOD(COMGETTER(CSAMEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMSETTER(CSAMEnabled))(BOOL a_fEnable);
    STDMETHOD(COMGETTER(LogEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMSETTER(LogEnabled))(BOOL a_fEnable);
    STDMETHOD(COMGETTER(LogDbgFlags))(BSTR *a_pbstrSettings);
    STDMETHOD(COMGETTER(LogDbgGroups))(BSTR *a_pbstrSettings);
    STDMETHOD(COMGETTER(LogDbgDestinations))(BSTR *a_pbstrSettings);
    STDMETHOD(COMGETTER(LogRelFlags))(BSTR *a_pbstrSettings);
    STDMETHOD(COMGETTER(LogRelGroups))(BSTR *a_pbstrSettings);
    STDMETHOD(COMGETTER(LogRelDestinations))(BSTR *a_pbstrSettings);
    STDMETHOD(COMGETTER(HWVirtExEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMGETTER(HWVirtExNestedPagingEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMGETTER(HWVirtExVPIDEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMGETTER(PAEEnabled))(BOOL *a_pfEnabled);
    STDMETHOD(COMGETTER(OSName))(BSTR *a_pbstrName);
    STDMETHOD(COMGETTER(OSVersion))(BSTR *a_pbstrVersion);
    STDMETHOD(COMGETTER(VirtualTimeRate))(ULONG *a_puPct);
    STDMETHOD(COMSETTER(VirtualTimeRate))(ULONG a_uPct);
    STDMETHOD(COMGETTER(VM))(LONG64 *a_u64Vm);

    // IMachineDebugger methods
    STDMETHOD(DumpGuestCore)(IN_BSTR a_bstrFilename, IN_BSTR a_bstrCompression);
    STDMETHOD(DumpHostProcessCore)(IN_BSTR a_bstrFilename, IN_BSTR a_bstrCompression);
    STDMETHOD(Info)(IN_BSTR a_bstrName, IN_BSTR a_bstrArgs, BSTR *a_bstrInfo);
    STDMETHOD(InjectNMI)();
    STDMETHOD(ModifyLogFlags)(IN_BSTR a_bstrSettings);
    STDMETHOD(ModifyLogGroups)(IN_BSTR a_bstrSettings);
    STDMETHOD(ModifyLogDestinations)(IN_BSTR a_bstrSettings);
    STDMETHOD(ReadPhysicalMemory)(LONG64 a_Address, ULONG a_cbRead, ComSafeArrayOut(BYTE, a_abData));
    STDMETHOD(WritePhysicalMemory)(LONG64 a_Address, ULONG a_cbRead, ComSafeArrayIn(BYTE, a_abData));
    STDMETHOD(ReadVirtualMemory)(ULONG a_idCpu, LONG64 a_Address, ULONG a_cbRead, ComSafeArrayOut(BYTE, a_abData));
    STDMETHOD(WriteVirtualMemory)(ULONG a_idCpu, LONG64 a_Address, ULONG a_cbRead, ComSafeArrayIn(BYTE, a_abData));
    STDMETHOD(DetectOS)(BSTR *a_pbstrName);
    STDMETHOD(GetRegister)(ULONG a_idCpu, IN_BSTR a_bstrName, BSTR *a_pbstrValue);
    STDMETHOD(GetRegisters)(ULONG a_idCpu, ComSafeArrayOut(BSTR, a_bstrNames), ComSafeArrayOut(BSTR, a_bstrValues));
    STDMETHOD(SetRegister)(ULONG a_idCpu, IN_BSTR a_bstrName, IN_BSTR a_bstrValue);
    STDMETHOD(SetRegisters)(ULONG a_idCpu, ComSafeArrayIn(IN_BSTR, a_bstrNames), ComSafeArrayIn(IN_BSTR, a_bstrValues));
    STDMETHOD(DumpGuestStack)(ULONG a_idCpu, BSTR *a_pbstrStack);
    STDMETHOD(ResetStats)(IN_BSTR aPattern);
    STDMETHOD(DumpStats)(IN_BSTR aPattern);
    STDMETHOD(GetStats)(IN_BSTR aPattern, BOOL aWithDescriptions, BSTR *aStats);


    // "public-private methods"
    void flushQueuedSettings();

private:
    // private methods
    bool queueSettings() const;

    /** RTLogGetFlags, RTLogGetGroupSettings and RTLogGetDestinations function. */
    typedef DECLCALLBACK(int) FNLOGGETSTR(PRTLOGGER, char *, size_t);
    /** Function pointer.  */
    typedef FNLOGGETSTR *PFNLOGGETSTR;
    HRESULT logStringProps(PRTLOGGER pLogger, PFNLOGGETSTR pfnLogGetStr, const char *pszLogGetStr, BSTR *a_bstrSettings);

    Console * const mParent;
    /** @name Flags whether settings have been queued because they could not be sent
     *        to the VM (not up yet, etc.)
     * @{ */
    int mSingleStepQueued;
    int mRecompileUserQueued;
    int mRecompileSupervisorQueued;
    int mPatmEnabledQueued;
    int mCsamEnabledQueued;
    int mLogEnabledQueued;
    uint32_t mVirtualTimeRateQueued;
    bool mFlushMode;
    /** @}  */
};

#endif /* !____H_MACHINEDEBUGGER */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
