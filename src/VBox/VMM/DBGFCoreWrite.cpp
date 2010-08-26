/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Guest Core Dump.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <iprt/param.h>
#include <VBox/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>


/**
 * EMT worker function for DBGFR3CoreWrite.
 *
 * @param   pVM              The VM handle.
 * @param   idCpu            The target CPU ID.
 * @param   pszDumpPath      The full path of the file to dump into.
 *
 * @return VBox status code.
 */
static DECLCALLBACK(int) dbgfR3CoreWrite(PVM pVM, VMCPUID idCpu, const char *pszDumpPath)
{
    /*
     * Validate input.
     */
    Assert(idCpu == VMMGetCpuId(pVM));
    AssertReturn(pszDumpPath, VERR_INVALID_POINTER);

    /* @todo */

    return VINF_SUCCESS;
}


/**
 * Write core dump of the guest.
 *
 * @return VBox status code.
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   pszDumpPath         The path of the file to dump into, cannot be
 *                              NULL.
 */
VMMR3DECL(int) DBGFR3CoreWrite(PVM pVM, VMCPUID idCpu, const char *pszDumpPath)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
    AssertReturn(pszDumpPath, VERR_INVALID_HANDLE);

    /*
     * Pass the core write request down to EMT.
     */
    return VMR3ReqCallWaitU(pVM->pUVM, idCpu, (PFNRT)dbgfR3CoreWrite, 3, pVM, idCpu, pszDumpPath);
}

