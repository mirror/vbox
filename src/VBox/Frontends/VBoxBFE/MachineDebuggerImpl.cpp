/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of MachineDebugger class
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
#else
# include <VBox/com/defs.h>
#endif
#include <VBox/vmm/em.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/semaphore.h>
#include <iprt/assert.h>

#include "VBoxBFE.h"
#include "MachineDebuggerImpl.h"

//
// defines
//


//
// globals
//


//
// public methods
//

/**
 * Initializes the machine debugger object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
MachineDebugger::MachineDebugger()
{
    singlestepQueued = ~0;
    recompileUserQueued = ~0;
    recompileSupervisorQueued = ~0;
    patmEnabledQueued = ~0;
    csamEnabledQueued = ~0;
    fFlushMode = false;
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
    /** @todo */
    return E_NOTIMPL;
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
    if (gpVM)
        *enabled = !EMIsRawRing3Enabled(gpVM);
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
    LogFlow(("MachineDebugger:: set user mode recompiler to %d\n", enable));

    if (!fFlushMode)
    {
        // check if the machine is running
        if (machineState != VMSTATE_RUNNING)
        {
            // queue the request
            recompileUserQueued = enable;
            return S_OK;
        }
    }
    if (!gpVM)
        return E_FAIL;

    int rcVBox = EMR3SetExecutionPolicy(gpVM, EMEXECPOLICY_RECOMPILE_RING3, enable);
    AssertRCReturn(rcVBox, E_FAIL);
    return S_OK;
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
    if (gpVM)
        *enabled = !EMIsRawRing0Enabled(gpVM);
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
    LogFlow(("MachineDebugger:: set supervisor mode recompiler to %d\n", enable));

    if (!fFlushMode)
    {
        // check if the machine is running
        if (machineState != VMSTATE_RUNNING)
        {
            // queue the request
            recompileSupervisorQueued = enable;
            return S_OK;
        }
    }
    if (!gpVM)
        return E_FAIL;

    int rcVBox = EMR3SetExecutionPolicy(gpVM, EMEXECPOLICY_RECOMPILE_RING0, enable);
    AssertRCReturn(rcVBox, E_FAIL);
    return S_OK;
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
    if (gpVM)
        *enabled = PATMIsEnabled(gpVM);
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
    LogFlow(("MachineDebugger::SetPATMEnabled: %d\n", enable));

    if (!fFlushMode)
    {
        // check if the machine is running
        if (machineState != VMSTATE_RUNNING)
        {
            // queue the request
            patmEnabledQueued = enable;
            return S_OK;
        }
    }

    if (!gpVM)
        return E_FAIL;

    PATMR3AllowPatching(gpVM, enable);
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
    if (gpVM)
        *enabled = CSAMIsEnabled(gpVM);
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
    LogFlow(("MachineDebugger:SetCSAMEnabled: %d\n", enable));

    if (!fFlushMode)
    {
        // check if the machine is running
        if (machineState != VMSTATE_RUNNING)
        {
            // queue the request
            csamEnabledQueued = enable;
            return S_OK;
        }
    }

    if (!gpVM)
        return E_FAIL;

    if (enable)
        CSAMEnableScanning(gpVM);
    else
        CSAMDisableScanning(gpVM);
    return E_NOTIMPL;
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
    fFlushMode = false;
}

//
// private methods
//
