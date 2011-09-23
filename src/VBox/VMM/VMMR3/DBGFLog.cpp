/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Log Manager.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgfR3LogModifyGroups(PVM pVM, const char *pszGroupSettings);
static DECLCALLBACK(int) dbgfR3LogModifyFlags(PVM pVM, const char *pszFlagSettings);
static DECLCALLBACK(int) dbgfR3LogModifyDestinations(PVM pVM, const char *pszDestSettings);


/**
 * Changes the logger group settings.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszGroupSettings    The group settings string. (VBOX_LOG)
 */
VMMR3DECL(int) DBGFR3LogModifyGroups(PVM pVM, const char *pszGroupSettings)
{
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertPtrReturn(pszGroupSettings, VERR_INVALID_POINTER);

    return VMR3ReqPriorityCallWait(pVM, VMCPUID_ANY, (PFNRT)dbgfR3LogModifyGroups, 2, pVM, pszGroupSettings);
}


/**
 * EMT worker for DBGFR3LogModifyGroups.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszGroupSettings    The group settings string. (VBOX_LOG)
 */
static DECLCALLBACK(int) dbgfR3LogModifyGroups(PVM pVM, const char *pszGroupSettings)
{
    int rc = RTLogGroupSettings(NULL, pszGroupSettings);
    if (RT_SUCCESS(rc))
        rc = VMMR3UpdateLoggers(pVM);
    return rc;
}


/**
 * Changes the logger flag settings.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszFlagSettings     The group settings string. (VBOX_LOG_FLAGS)
 */
VMMR3DECL(int) DBGFR3LogModifyFlags(PVM pVM, const char *pszFlagSettings)
{
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFlagSettings, VERR_INVALID_POINTER);

    return VMR3ReqPriorityCallWait(pVM, VMCPUID_ANY, (PFNRT)dbgfR3LogModifyFlags, 2, pVM, pszFlagSettings);
}


/**
 * EMT worker for DBGFR3LogModifyFlags.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszFlagSettings     The group settings string. (VBOX_LOG_FLAGS)
 */
static DECLCALLBACK(int) dbgfR3LogModifyFlags(PVM pVM, const char *pszFlagSettings)
{
    int rc = RTLogFlags(NULL, pszFlagSettings);
    if (RT_SUCCESS(rc))
        rc = VMMR3UpdateLoggers(pVM);
    return rc;
}


/**
 * Changes the logger destination settings.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszDestSettings     The destination settings string. (VBOX_LOG_DEST)
 */
VMMR3DECL(int) DBGFR3LogModifyDestinations(PVM pVM, const char *pszDestSettings)
{
    AssertReturn(VALID_PTR(pVM), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszDestSettings), VERR_INVALID_POINTER);

    return VMR3ReqPriorityCallWait(pVM, VMCPUID_ANY, (PFNRT)dbgfR3LogModifyDestinations, 2, pVM, pszDestSettings);
}


/**
 * EMT worker for DBGFR3LogModifyFlags.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszDestSettings     The destination settings string. (VBOX_LOG_DEST)
 */
static DECLCALLBACK(int) dbgfR3LogModifyDestinations(PVM pVM, const char *pszDestSettings)
{
    int rc = RTLogDestinations(NULL, pszDestSettings);
    if (RT_SUCCESS(rc))
        rc = VMMR3UpdateLoggers(pVM);
    return rc;
}

