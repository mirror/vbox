/** @file
 *
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/err.h>
#include <iprt/assert.h>
#include "VBoxDbgBase.h"



VBoxDbgBase::VBoxDbgBase(PVM pVM) : m_pVM(pVM)
{
    /*
     * Register
     */
    int rc = VMR3AtStateRegister(pVM, atStateChange, this);
    AssertRC(rc);
}

VBoxDbgBase::~VBoxDbgBase()
{
    /*
     * If the VM is still around.
     */
    if (m_pVM)
    {
        int rc = VMR3AtStateDeregister(m_pVM, atStateChange, this);
        AssertRC(rc);
        m_pVM = NULL;
    }
}

int VBoxDbgBase::stamReset(const char *pszPat)
{
    if (m_pVM)
        return STAMR3Reset(m_pVM, pszPat);
    return VERR_INVALID_HANDLE;
}

int VBoxDbgBase::stamEnum(const char *pszPat, PFNSTAMR3ENUM pfnEnum, void *pvUser)
{
    if (m_pVM)
        return STAMR3Enum(m_pVM, pszPat, pfnEnum, pvUser);
    return VERR_INVALID_HANDLE;
}

int VBoxDbgBase::dbgcCreate(PDBGCBACK pBack, unsigned fFlags)
{
    if (m_pVM)
        return DBGCCreate(m_pVM, pBack, fFlags);
    return VERR_INVALID_HANDLE;
}

/*static*/ DECLCALLBACK(void) VBoxDbgBase::atStateChange(PVM /*pVM*/, VMSTATE enmState, VMSTATE /*enmOldState*/, void *pvUser)
{
    VBoxDbgBase *pThis = (VBoxDbgBase *)pvUser;
    switch (enmState)
    {
        case VMSTATE_TERMINATED:
            pThis->sigTerminated();
            pThis->m_pVM = NULL;
            break;

        default:
            break;
    }
}

void VBoxDbgBase::sigTerminated()
{
}

