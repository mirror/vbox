/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, R0 Driver parts.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_DRIVER
#include "PDMInternal.h"
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/gvmm.h>

#include <VBox/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>



/** @name Ring-0 Context Driver Helpers
 * @{
 */

/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetError} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetError(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetErrorV} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetErrorV(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetRuntimeError} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetRuntimeError(PPDMDRVINS pDrvIns, uint32_t fFlags, const char *pszErrorId,
                                                       const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetRuntimeErrorV} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetRuntimeErrorV(PPDMDRVINS pDrvIns, uint32_t fFlags, const char *pszErrorId,
                                                        const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnAssertEMT} */
static DECLCALLBACK(bool) pdmR0DrvHlp_AssertEMT(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (VM_IS_EMT(pDrvIns->Internal.s.pVMR0))
        return true;

    RTAssertMsg1Weak("AssertEMT", iLine, pszFile, pszFunction);
    RTAssertPanic();
    return false;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnAssertOther} */
static DECLCALLBACK(bool) pdmR0DrvHlp_AssertOther(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (!VM_IS_EMT(pDrvIns->Internal.s.pVMR0))
        return true;

    RTAssertMsg1Weak("AssertOther", iLine, pszFile, pszFunction);
    RTAssertPanic();
    return false;
}


/**
 * The Ring-0 Context Driver Helper Callbacks.
 */
extern DECLEXPORT(const PDMDRVHLPR0) g_pdmR0DrvHlp =
{
    PDM_DRVHLPRC_VERSION,
    pdmR0DrvHlp_VMSetError,
    pdmR0DrvHlp_VMSetErrorV,
    pdmR0DrvHlp_VMSetRuntimeError,
    pdmR0DrvHlp_VMSetRuntimeErrorV,
    pdmR0DrvHlp_AssertEMT,
    pdmR0DrvHlp_AssertOther,
    PDM_DRVHLPRC_VERSION
};

/** @} */



/**
 * PDMDrvHlpCallR0 helper.
 *
 * @returns See PFNPDMDRVREQHANDLERR0.
 * @param   pGVM    The global (ring-0) VM structure. (For validation.)
 * @param   pReq    Pointer to the request buffer.
 */
VMMR0_INT_DECL(int) PDMR0DriverCallReqHandler(PGVM pGVM, PPDMDRIVERCALLREQHANDLERREQ pReq)
{
    /*
     * Validate input and make the call.
     */
    int rc = GVMMR0ValidateGVM(pGVM);
    if (RT_SUCCESS(rc))
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);
        AssertMsgReturn(pReq->Hdr.cbReq == sizeof(*pReq), ("%#x != %#x\n", pReq->Hdr.cbReq, sizeof(*pReq)), VERR_INVALID_PARAMETER);

        PPDMDRVINS pDrvIns = pReq->pDrvInsR0;
        AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
        AssertReturn(pDrvIns->Internal.s.pVMR0 == pGVM, VERR_INVALID_PARAMETER);

        PFNPDMDRVREQHANDLERR0 pfnReqHandlerR0 = pDrvIns->Internal.s.pfnReqHandlerR0;
        AssertPtrReturn(pfnReqHandlerR0, VERR_INVALID_POINTER);

        rc = pfnReqHandlerR0(pDrvIns, pReq->uOperation, pReq->u64Arg);
    }
    return rc;
}

