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
 */

#include "MachineDebuggerImpl.h"
#include "ConsoleImpl.h"
#include "Logging.h"

#include <VBox/em.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/vm.h>
#include <VBox/tm.h>
#include <VBox/err.h>
#include <VBox/hwaccm.h>

//
// defines
//


//
// globals
//


//
// constructor / destructor
//

HRESULT MachineDebugger::FinalConstruct()
{
    mParent = NULL;
    return S_OK;
}

void MachineDebugger::FinalRelease()
{
    if (isReady())
        uninit();
}

//
// public methods
//

/**
 * Initializes the machine debugger object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
HRESULT MachineDebugger::init(Console *parent)
{
    LogFlow(("MachineDebugger::init(): isReady=%d\n", isReady()));

    ComAssertRet (parent, E_INVALIDARG);

    AutoLock lock(this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    singlestepQueued = ~0;
    recompileUserQueued = ~0;
    recompileSupervisorQueued = ~0;
    patmEnabledQueued = ~0;
    csamEnabledQueued = ~0;
    mLogEnabledQueued = ~0;
    mVirtualTimeRateQueued = ~0;
    fFlushMode = false;
    setReady(true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void MachineDebugger::uninit()
{
    LogFlow(("MachineDebugger::uninit(): isReady=%d\n", isReady()));

    AutoLock lock(this);
    AssertReturn (isReady(), (void) 0);

    setReady (false);
}

/**
 * Returns the current singlestepping flag.
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(Singlestep)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    /** @todo */
    return E_NOTIMPL;
}

/**
 * Sets the singlestepping flag.
 *
 * @returns COM status code
 * @param enable new singlestepping flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(Singlestep)(BOOL enable)
{
    AutoLock lock(this);
    CHECK_READY();
    /** @todo */
    return E_NOTIMPL;
}

/**
 * Resets VM statistics.
 *
 * @returns COM status code.
 */
STDMETHODIMP MachineDebugger::ResetStats()
{
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        STAMR3Reset(pVM, NULL);
    return S_OK;
}

/**
 * Dumps VM statistics to the log.
 *
 * @returns COM status code.
 */
STDMETHODIMP MachineDebugger::DumpStats()
{
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        STAMR3Dump(pVM, NULL);
    return S_OK;
}

/**
 * Get the VM statistics in an XML format.
 *
 * @returns COM status code.
 * @param   aPattern            The selection pattern. A bit similar to filename globbing.
 * @param   aWithDescriptions   Whether to include the descriptions.
 * @param   aStats              The XML document containing the statistics.
 */
STDMETHODIMP MachineDebugger::GetStats(INPTR BSTR aPattern, BOOL aWithDescriptions, BSTR *aStats)
{
    Console::SafeVMPtrQuiet pVM (mParent);
    if (!pVM.isOk())
        return E_FAIL;

    char *pszSnapshot;
    int vrc = STAMR3Snapshot(pVM, Utf8Str(aPattern).raw(), &pszSnapshot, NULL, aWithDescriptions);
    if (RT_FAILURE(vrc))
        return vrc == VERR_NO_MEMORY ? E_OUTOFMEMORY : E_FAIL;

    /** @todo this is horribly inefficient! And it's kinda difficult to tell whether it failed...
     * Must use UTF-8 or ASCII here and completely avoid these two extra copy operations.
     * Until that's done, this method is kind of useless for debugger statistics GUI because
     * of the amount statistics in a debug build. */
    Bstr(pszSnapshot).cloneTo(aStats);

    return S_OK;
}

/**
 * Returns the current recompile user mode code flag.
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(RecompileUser)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        *enabled = !EMIsRawRing3Enabled(pVM.raw());
    else
        *enabled = false;
    return S_OK;
}

/**
 * Sets the recompile user mode code flag.
 *
 * @returns COM status
 * @param   enable new user mode code recompile flag.
 */
STDMETHODIMP MachineDebugger::COMSETTER(RecompileUser)(BOOL enable)
{
    LogFlowThisFunc (("enable=%d\n", enable));

    AutoLock lock(this);
    CHECK_READY();

    if (!fFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State)(&machineState);
        if (machineState != MachineState_Running)
        {
            // queue the request
            recompileUserQueued = enable;
            return S_OK;
        }
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    PVMREQ pReq;
    EMRAWMODE rawModeFlag = enable ? EMRAW_RING3_DISABLE : EMRAW_RING3_ENABLE;
    int rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)EMR3RawSetMode, 2, pVM.raw(), rawModeFlag);
    if (VBOX_SUCCESS(rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    if (VBOX_SUCCESS(rcVBox))
        return S_OK;

    AssertMsgFailed(("Could not set raw mode flags to %d, rcVBox = %Vrc\n",
                     rawModeFlag, rcVBox));
    return E_FAIL;
}

/**
 * Returns the current recompile supervisor code flag.
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(RecompileSupervisor)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        *enabled = !EMIsRawRing0Enabled(pVM.raw());
    else
        *enabled = false;
    return S_OK;
}

/**
 * Sets the new recompile supervisor code flag.
 *
 * @returns COM status code
 * @param   enable new recompile supervisor code flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(RecompileSupervisor)(BOOL enable)
{
    LogFlowThisFunc (("enable=%d\n", enable));

    AutoLock lock(this);
    CHECK_READY();

    if (!fFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State)(&machineState);
        if (machineState != MachineState_Running)
        {
            // queue the request
            recompileSupervisorQueued = enable;
            return S_OK;
        }
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    PVMREQ pReq;
    EMRAWMODE rawModeFlag = enable ? EMRAW_RING0_DISABLE : EMRAW_RING0_ENABLE;
    int rcVBox = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT,
                             (PFNRT)EMR3RawSetMode, 2, pVM.raw(), rawModeFlag);
    if (VBOX_SUCCESS(rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree(pReq);
    }

    if (VBOX_SUCCESS(rcVBox))
        return S_OK;

    AssertMsgFailed(("Could not set raw mode flags to %d, rcVBox = %Vrc\n",
                     rawModeFlag, rcVBox));
    return E_FAIL;
}

/**
 * Returns the current patch manager enabled flag.
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(PATMEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        *enabled = PATMIsEnabled(pVM.raw());
    else
        *enabled = false;
    return S_OK;
}

/**
 * Set the new patch manager enabled flag.
 *
 * @returns COM status code
 * @param   new patch manager enabled flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(PATMEnabled)(BOOL enable)
{
    AutoLock lock(this);
    CHECK_READY();
    LogFlowThisFunc (("enable=%d\n", enable));

    if (!fFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State)(&machineState);
        if (machineState != MachineState_Running)
        {
            // queue the request
            patmEnabledQueued = enable;
            return S_OK;
        }
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    PATMR3AllowPatching(pVM, enable);
    return E_NOTIMPL;
}

/**
 * Returns the current code scanner enabled flag.
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(CSAMEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        *enabled = CSAMIsEnabled(pVM.raw());
    else
        *enabled = false;
    return S_OK;
}

/**
 * Sets the new code scanner enabled flag.
 *
 * @returns COM status code
 * @param   enable new code scanner enabled flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(CSAMEnabled)(BOOL enable)
{
    AutoLock lock(this);
    CHECK_READY();
    LogFlowThisFunc (("enable=%d\n", enable));

    if (!fFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State)(&machineState);
        if (machineState != MachineState_Running)
        {
            // queue the request
            csamEnabledQueued = enable;
            return S_OK;
        }
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    int vrc;
    if (enable)
        vrc = CSAMEnableScanning(pVM);
    else
        vrc = CSAMDisableScanning(pVM);
    if (VBOX_FAILURE(vrc))
    {
        /** @todo handle error case */
    }
    return S_OK;
}

/**
 * Returns the log enabled / disabled status.
 *
 * @returns COM status code
 * @param   aEnabled     address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(LogEnabled)(BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;
    AutoLock alock(this);
    CHECK_READY();
    PRTLOGGER pLogInstance = RTLogDefaultInstance();
    *aEnabled = pLogInstance && !(pLogInstance->fFlags & RTLOGFLAGS_DISABLED);
    return S_OK;
}

/**
 * Enables or disables logging.
 *
 * @returns COM status code
 * @param   aEnabled    The new code log state.
 */
STDMETHODIMP MachineDebugger::COMSETTER(LogEnabled)(BOOL aEnabled)
{
    AutoLock alock(this);

    CHECK_READY();
    LogFlowThisFunc (("aEnabled=%d\n", aEnabled));

    if (!fFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State)(&machineState);
        if (machineState != MachineState_Running)
        {
            // queue the request
            mLogEnabledQueued = aEnabled;
            return S_OK;
        }
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    int vrc = DBGFR3LogModifyFlags(pVM, aEnabled ? "enabled" : "disabled");
    if (VBOX_FAILURE(vrc))
    {
        /** @todo handle error code. */
    }
    return S_OK;
}

/**
 * Returns the current hardware virtualization flag.
 *
 * @returns COM status code
 * @param   enabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(HWVirtExEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        *enabled = HWACCMIsEnabled(pVM.raw());
    else
        *enabled = false;
    return S_OK;
}

/**
 * Returns the current virtual time rate.
 *
 * @returns COM status code.
 * @param   pct     Where to store the rate.
 */
STDMETHODIMP MachineDebugger::COMGETTER(VirtualTimeRate)(ULONG *pct)
{
    if (!pct)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    Console::SafeVMPtrQuiet pVM (mParent);
    if (pVM.isOk())
        *pct = TMVirtualGetWarpDrive(pVM);
    else
        *pct = 100;
    return S_OK;
}

/**
 * Returns the current virtual time rate.
 *
 * @returns COM status code.
 * @param   pct     Where to store the rate.
 */
STDMETHODIMP MachineDebugger::COMSETTER(VirtualTimeRate)(ULONG pct)
{
    if (pct < 2 || pct > 20000)
        return E_INVALIDARG;

    AutoLock lock(this);
    CHECK_READY();

    if (!fFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State)(&machineState);
        if (machineState != MachineState_Running)
        {
            // queue the request
            mVirtualTimeRateQueued = pct;
            return S_OK;
        }
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    int vrc = TMVirtualSetWarpDrive(pVM, pct);
    if (VBOX_FAILURE(vrc))
    {
        /** @todo handle error code. */
    }
    return S_OK;
}

/**
 * Hack for getting the VM handle.
 * This is only temporary (promise) while prototyping the debugger.
 *
 * @returns COM status code
 * @param   vm      Where to store the vm handle.
 *                  Since there is no uintptr_t in COM, we're using the max integer.
 *                  (No, ULONG is not pointer sized!)
 */
STDMETHODIMP MachineDebugger::COMGETTER(VM)(ULONG64 *vm)
{
    if (!vm)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    *vm = (uintptr_t)pVM.raw();

    /*
     *  Note: pVM protection provided by SafeVMPtr is no more effective
     *  after we return from this method.
     */

    return S_OK;
}

//
// "public-private" methods
//
void MachineDebugger::flushQueuedSettings()
{
    fFlushMode = true;
    if (singlestepQueued != ~0)
    {
        COMSETTER(Singlestep)(singlestepQueued);
        singlestepQueued = ~0;
    }
    if (recompileUserQueued != ~0)
    {
        COMSETTER(RecompileUser)(recompileUserQueued);
        recompileUserQueued = ~0;
    }
    if (recompileSupervisorQueued != ~0)
    {
        COMSETTER(RecompileSupervisor)(recompileSupervisorQueued);
        recompileSupervisorQueued = ~0;
    }
    if (patmEnabledQueued != ~0)
    {
        COMSETTER(PATMEnabled)(patmEnabledQueued);
        patmEnabledQueued = ~0;
    }
    if (csamEnabledQueued != ~0)
    {
        COMSETTER(CSAMEnabled)(csamEnabledQueued);
        csamEnabledQueued = ~0;
    }
    if (mLogEnabledQueued != ~0)
    {
        COMSETTER(LogEnabled)(mLogEnabledQueued);
        mLogEnabledQueued = ~0;
    }
    if (mVirtualTimeRateQueued != ~(uint32_t)0)
    {
        COMSETTER(VirtualTimeRate)(mVirtualTimeRateQueued);
        mVirtualTimeRateQueued = ~0;
    }
    fFlushMode = false;
}

//
// private methods
//
