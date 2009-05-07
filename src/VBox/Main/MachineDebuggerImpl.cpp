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

#include "MachineDebuggerImpl.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "Logging.h"

#include <VBox/em.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/vm.h>
#include <VBox/tm.h>
#include <VBox/err.h>
#include <VBox/hwaccm.h>

// defines
/////////////////////////////////////////////////////////////////////////////


// globals
/////////////////////////////////////////////////////////////////////////////


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (MachineDebugger)

HRESULT MachineDebugger::FinalConstruct()
{
    unconst (mParent) = NULL;
    return S_OK;
}

void MachineDebugger::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the machine debugger object.
 *
 * @returns COM result indicator
 * @param aParent handle of our parent object
 */
HRESULT MachineDebugger::init (Console *aParent)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;

    mSinglestepQueued = ~0;
    mRecompileUserQueued = ~0;
    mRecompileSupervisorQueued = ~0;
    mPatmEnabledQueued = ~0;
    mCsamEnabledQueued = ~0;
    mLogEnabledQueued = ~0;
    mVirtualTimeRateQueued = ~0;
    mFlushMode = false;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void MachineDebugger::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (mParent).setNull();
    mFlushMode = false;
}

// IMachineDebugger properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the current singlestepping flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(Singlestep) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /** @todo */
    ReturnComNotImplemented();
}

/**
 * Sets the singlestepping flag.
 *
 * @returns COM status code
 * @param aEnable new singlestepping flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(Singlestep) (BOOL aEnable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /** @todo */
    ReturnComNotImplemented();
}

/**
 * Returns the current recompile user mode code flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(RecompileUser) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = !EMIsRawRing3Enabled (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Sets the recompile user mode code flag.
 *
 * @returns COM status
 * @param   aEnable new user mode code recompile flag.
 */
STDMETHODIMP MachineDebugger::COMSETTER(RecompileUser) (BOOL aEnable)
{
    LogFlowThisFunc (("enable=%d\n", aEnable));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (queueSettings())
    {
        // queue the request
        mRecompileUserQueued = aEnable;
        return S_OK;
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    PVMREQ pReq;
    EMRAWMODE rawModeFlag = aEnable ? EMRAW_RING3_DISABLE : EMRAW_RING3_ENABLE;
    int rcVBox = VMR3ReqCall (pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT,
                              (PFNRT)EMR3RawSetMode, 2, pVM.raw(), rawModeFlag);
    if (RT_SUCCESS (rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree (pReq);
    }

    if (RT_SUCCESS (rcVBox))
        return S_OK;

    AssertMsgFailed (("Could not set raw mode flags to %d, rcVBox = %Rrc\n",
                      rawModeFlag, rcVBox));
    return E_FAIL;
}

/**
 * Returns the current recompile supervisor code flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(RecompileSupervisor) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = !EMIsRawRing0Enabled (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Sets the new recompile supervisor code flag.
 *
 * @returns COM status code
 * @param   aEnable new recompile supervisor code flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(RecompileSupervisor) (BOOL aEnable)
{
    LogFlowThisFunc (("enable=%d\n", aEnable));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (queueSettings())
    {
        // queue the request
        mRecompileSupervisorQueued = aEnable;
        return S_OK;
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    PVMREQ pReq;
    EMRAWMODE rawModeFlag = aEnable ? EMRAW_RING0_DISABLE : EMRAW_RING0_ENABLE;
    int rcVBox = VMR3ReqCall (pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT,
                              (PFNRT)EMR3RawSetMode, 2, pVM.raw(), rawModeFlag);
    if (RT_SUCCESS (rcVBox))
    {
        rcVBox = pReq->iStatus;
        VMR3ReqFree (pReq);
    }

    if (RT_SUCCESS (rcVBox))
        return S_OK;

    AssertMsgFailed (("Could not set raw mode flags to %d, rcVBox = %Rrc\n",
                      rawModeFlag, rcVBox));
    return E_FAIL;
}

/**
 * Returns the current patch manager enabled flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(PATMEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = PATMIsEnabled (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Set the new patch manager enabled flag.
 *
 * @returns COM status code
 * @param   aEnable new patch manager enabled flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(PATMEnabled) (BOOL aEnable)
{
    LogFlowThisFunc (("enable=%d\n", aEnable));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (queueSettings())
    {
        // queue the request
        mPatmEnabledQueued = aEnable;
        return S_OK;
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    PATMR3AllowPatching (pVM, aEnable);

    return S_OK;
}

/**
 * Returns the current code scanner enabled flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(CSAMEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = CSAMIsEnabled (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Sets the new code scanner enabled flag.
 *
 * @returns COM status code
 * @param   aEnable new code scanner enabled flag
 */
STDMETHODIMP MachineDebugger::COMSETTER(CSAMEnabled) (BOOL aEnable)
{
    LogFlowThisFunc (("enable=%d\n", aEnable));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (queueSettings())
    {
        // queue the request
        mCsamEnabledQueued = aEnable;
        return S_OK;
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    int vrc;
    if (aEnable)
        vrc = CSAMEnableScanning (pVM);
    else
        vrc = CSAMDisableScanning (pVM);

    if (RT_FAILURE (vrc))
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
STDMETHODIMP MachineDebugger::COMGETTER(LogEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

#ifdef LOG_ENABLED
    AutoReadLock alock (this);

    const PRTLOGGER pLogInstance = RTLogDefaultInstance();
    *aEnabled = pLogInstance && !(pLogInstance->fFlags & RTLOGFLAGS_DISABLED);
#else
    *aEnabled = false;
#endif

    return S_OK;
}

/**
 * Enables or disables logging.
 *
 * @returns COM status code
 * @param   aEnabled    The new code log state.
 */
STDMETHODIMP MachineDebugger::COMSETTER(LogEnabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%d\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (queueSettings())
    {
        // queue the request
        mLogEnabledQueued = aEnabled;
        return S_OK;
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

#ifdef LOG_ENABLED
    int vrc = DBGFR3LogModifyFlags (pVM, aEnabled ? "enabled" : "disabled");
    if (RT_FAILURE (vrc))
    {
        /** @todo handle error code. */
    }
#endif

    return S_OK;
}

/**
 * Returns the current hardware virtualization flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(HWVirtExEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = HWACCMIsEnabled (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Returns the current nested paging flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(HWVirtExNestedPagingEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = HWACCMR3IsNestedPagingActive (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Returns the current VPID flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(HWVirtExVPIDEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aEnabled = HWACCMR3IsVPIDActive (pVM.raw());
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Returns the current PAE flag.
 *
 * @returns COM status code
 * @param   aEnabled address of result variable
 */
STDMETHODIMP MachineDebugger::COMGETTER(PAEEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
    {
        uint64_t cr4 = CPUMGetGuestCR4 (VMMGetCpu0(pVM.raw()));
        *aEnabled = !!(cr4 & X86_CR4_PAE);
    }
    else
        *aEnabled = false;

    return S_OK;
}

/**
 * Returns the current virtual time rate.
 *
 * @returns COM status code.
 * @param   aPct     Where to store the rate.
 */
STDMETHODIMP MachineDebugger::COMGETTER(VirtualTimeRate) (ULONG *aPct)
{
    CheckComArgOutPointerValid(aPct);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtrQuiet pVM (mParent);

    if (pVM.isOk())
        *aPct = TMGetWarpDrive (pVM);
    else
        *aPct = 100;

    return S_OK;
}

/**
 * Returns the current virtual time rate.
 *
 * @returns COM status code.
 * @param   aPct     Where to store the rate.
 */
STDMETHODIMP MachineDebugger::COMSETTER(VirtualTimeRate) (ULONG aPct)
{
    if (aPct < 2 || aPct > 20000)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (queueSettings())
    {
        // queue the request
        mVirtualTimeRateQueued = aPct;
        return S_OK;
    }

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    int vrc = TMR3SetWarpDrive (pVM, aPct);
    if (RT_FAILURE (vrc))
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
 * @param   aVm      Where to store the vm handle.
 *                  Since there is no uintptr_t in COM, we're using the max integer.
 *                  (No, ULONG is not pointer sized!)
 */
STDMETHODIMP MachineDebugger::COMGETTER(VM) (ULONG64 *aVm)
{
    CheckComArgOutPointerValid(aVm);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    *aVm = (uintptr_t)pVM.raw();

    /*
     *  Note: pVM protection provided by SafeVMPtr is no more effective
     *  after we return from this method.
     */

    return S_OK;
}

// IMachineDebugger methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Resets VM statistics.
 *
 * @returns COM status code.
 * @param   aPattern            The selection pattern. A bit similar to filename globbing.
 */
STDMETHODIMP MachineDebugger::ResetStats (IN_BSTR aPattern)
{
    Console::SafeVMPtrQuiet pVM (mParent);

    if (!pVM.isOk())
        return E_FAIL;

    STAMR3Reset (pVM, Utf8Str (aPattern).raw());

    return S_OK;
}

/**
 * Dumps VM statistics to the log.
 *
 * @returns COM status code.
 * @param   aPattern            The selection pattern. A bit similar to filename globbing.
 */
STDMETHODIMP MachineDebugger::DumpStats (IN_BSTR aPattern)
{
    Console::SafeVMPtrQuiet pVM (mParent);

    if (!pVM.isOk())
        return E_FAIL;

    STAMR3Dump (pVM, Utf8Str (aPattern).raw());

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
STDMETHODIMP MachineDebugger::GetStats (IN_BSTR aPattern, BOOL aWithDescriptions, BSTR *aStats)
{
    Console::SafeVMPtrQuiet pVM (mParent);

    if (!pVM.isOk())
        return E_FAIL;

    char *pszSnapshot;
    int vrc = STAMR3Snapshot (pVM, Utf8Str (aPattern).raw(), &pszSnapshot, NULL,
                              !!aWithDescriptions);
    if (RT_FAILURE (vrc))
        return vrc == VERR_NO_MEMORY ? E_OUTOFMEMORY : E_FAIL;

    /** @todo this is horribly inefficient! And it's kinda difficult to tell whether it failed...
     * Must use UTF-8 or ASCII here and completely avoid these two extra copy operations.
     * Until that's done, this method is kind of useless for debugger statistics GUI because
     * of the amount statistics in a debug build. */
    Bstr (pszSnapshot).cloneTo (aStats);

    return S_OK;
}

/**
 * Set the new patch manager enabled flag.
 *
 * @returns COM status code
 * @param   new patch manager enabled flag
 */
STDMETHODIMP MachineDebugger::InjectNMI()
{
    LogFlowThisFunc ((""));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    Console::SafeVMPtr pVM (mParent);
    CheckComRCReturnRC (pVM.rc());

    HWACCMR3InjectNMI(pVM);

    return S_OK;
}


// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void MachineDebugger::flushQueuedSettings()
{
    mFlushMode = true;
    if (mSinglestepQueued != ~0)
    {
        COMSETTER(Singlestep) (mSinglestepQueued);
        mSinglestepQueued = ~0;
    }
    if (mRecompileUserQueued != ~0)
    {
        COMSETTER(RecompileUser) (mRecompileUserQueued);
        mRecompileUserQueued = ~0;
    }
    if (mRecompileSupervisorQueued != ~0)
    {
        COMSETTER(RecompileSupervisor) (mRecompileSupervisorQueued);
        mRecompileSupervisorQueued = ~0;
    }
    if (mPatmEnabledQueued != ~0)
    {
        COMSETTER(PATMEnabled) (mPatmEnabledQueued);
        mPatmEnabledQueued = ~0;
    }
    if (mCsamEnabledQueued != ~0)
    {
        COMSETTER(CSAMEnabled) (mCsamEnabledQueued);
        mCsamEnabledQueued = ~0;
    }
    if (mLogEnabledQueued != ~0)
    {
        COMSETTER(LogEnabled) (mLogEnabledQueued);
        mLogEnabledQueued = ~0;
    }
    if (mVirtualTimeRateQueued != ~(uint32_t)0)
    {
        COMSETTER(VirtualTimeRate) (mVirtualTimeRateQueued);
        mVirtualTimeRateQueued = ~0;
    }
    mFlushMode = false;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

bool MachineDebugger::queueSettings() const
{
    if (!mFlushMode)
    {
        // check if the machine is running
        MachineState_T machineState;
        mParent->COMGETTER(State) (&machineState);
        if (!Global::IsActive (machineState))
            // queue the request
            return true;
    }
    return false;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
