/** @file
 *
 * VBox Debugger GUI - Base class.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef __VBoxDbgBase_h__
#define __VBoxDbgBase_h__


#include <VBox/stam.h>
#include <VBox/vmapi.h>
#include <VBox/dbg.h>


/**
 * VBox Debugger GUI Base Class.
 *
 * The purpose of this class is to hide the VM handle, abstract VM
 * operations, and finally to make sure the GUI won't crash when
 * the VM dies.
 */
class VBoxDbgBase
{
public:
    /**
     * Construct the object.
     *
     * @param   pVM     The VM handle.
     */
    VBoxDbgBase(PVM pVM);

    /**
     * Destructor.
     */
    virtual ~VBoxDbgBase();


protected:
    /**
     * Checks if the VM is OK for normal operations.
     * @returns true if ok, false if not.
     */
    bool isVMOk() const
    {
        return m_pVM != NULL;
    }

    /** @name Operations
     * @{ */
    /**
     * Wrapper for STAMR3Reset().
     */
    int stamReset(const char *pszPat);
    /**
     * Wrapper for STAMR3Enum().
     */
    int stamEnum(const char *pszPat, PFNSTAMR3ENUM pfnEnum, void *pvUser);
    /**
     * Wrapper for DBGCCreate().
     */
    int dbgcCreate(PDBGCBACK pBack, unsigned fFlags);
    /** @} */


protected:
    /** @name Signals
     * @{ */
    /**
     * Called when the VM has been terminated.
     */
    virtual void sigTerminated();
    /** @} */


private:
    /**
     * VM state callback function.
     *
     * You are not allowed to call any function which changes the VM state from a
     * state callback, except VMR3Destroy().
     *
     * @param   pVM         The VM handle.
     * @param   enmState    The new state.
     * @param   enmOldState The old state.
     * @param   pvUser      The user argument.
     */
    static DECLCALLBACK(void) atStateChange(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser);

private:
    /** The VM handle. */
    PVM m_pVM;
};


#endif

