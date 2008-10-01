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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGG
#include <VBox/err.h>
#include <iprt/assert.h>
#include "VBoxDbgBase.h"



VBoxDbgBase::VBoxDbgBase(PVM pVM)
    : m_pVM(pVM), m_hGUIThread(RTThreadNativeSelf())
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
    /** @todo need to do some locking here?  */
    PVM pVM = (PVM)ASMAtomicXchgPtr((void * volatile *)&m_pVM, NULL);
    if (pVM)
    {
        int rc = VMR3AtStateDeregister(pVM, atStateChange, this);
        AssertRC(rc);
    }
}


int
VBoxDbgBase::stamReset(const QString &rPat)
{
#ifdef VBOXDBG_USE_QT4
    QByteArray Utf8Array = rPat.toUtf8();
    const char *pszPat = !rPat.isEmpty() ? Utf8Array.constData() : NULL;
#else
    const char *pszPat = !rPat.isEmpty() ? rPat : NULL;
#endif
    if (m_pVM)
        return STAMR3Reset(m_pVM, pszPat);
    return VERR_INVALID_HANDLE;
}


int
VBoxDbgBase::stamEnum(const QString &rPat, PFNSTAMR3ENUM pfnEnum, void *pvUser)
{
#ifdef VBOXDBG_USE_QT4
    QByteArray Utf8Array = rPat.toUtf8();
    const char *pszPat = !rPat.isEmpty() ? Utf8Array.constData() : NULL;
#else
    const char *pszPat = !rPat.isEmpty() ? rPat : NULL;
#endif
    if (m_pVM)
        return STAMR3Enum(m_pVM, pszPat, pfnEnum, pvUser);
    return VERR_INVALID_HANDLE;
}


int
VBoxDbgBase::dbgcCreate(PDBGCBACK pBack, unsigned fFlags)
{
    if (m_pVM)
        return DBGCCreate(m_pVM, pBack, fFlags);
    return VERR_INVALID_HANDLE;
}


/*static*/ DECLCALLBACK(void)
VBoxDbgBase::atStateChange(PVM pVM, VMSTATE enmState, VMSTATE /*enmOldState*/, void *pvUser)
{
    VBoxDbgBase *pThis = (VBoxDbgBase *)pvUser;
    switch (enmState)
    {
        case VMSTATE_TERMINATED:
            /** @todo need to do some locking here?  */
            if (ASMAtomicCmpXchgPtr((void * volatile *)&pThis->m_pVM, NULL, pVM))
                pThis->sigTerminated();
            break;

        default:
            break;
    }
}


void
VBoxDbgBase::sigTerminated()
{
}

