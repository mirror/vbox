/* $Id$ */
/** @file
 * VBox Debugger GUI - Base class.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


#ifndef ___Debugger_VBoxDbgBase_h
#define ___Debugger_VBoxDbgBase_h


#include <VBox/stam.h>
#include <VBox/vmapi.h>
#include <VBox/dbg.h>
#include <iprt/thread.h>
#ifdef VBOXDBG_USE_QT4
# include <QString>
#else
# include <qstring.h>
#endif


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


    /**
     * Checks if the VM is OK for normal operations.
     * @returns true if ok, false if not.
     */
    bool isVMOk() const
    {
        return m_pVM != NULL;
    }

    /**
     * Checks if the current thread is the GUI thread or not.
     * @return true/false accordingly.
     */
    bool isGUIThread() const
    {
        return m_hGUIThread == RTThreadNativeSelf();
    }

    /** @name Operations
     * @{ */
    /**
     * Wrapper for STAMR3Reset().
     */
    int stamReset(const QString &rPat);
    /**
     * Wrapper for STAMR3Enum().
     */
    int stamEnum(const QString &rPat, PFNSTAMR3ENUM pfnEnum, void *pvUser);
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
    /** The handle of the GUI thread. */
    RTNATIVETHREAD m_hGUIThread;
};


#endif

