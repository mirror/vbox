/* $Id$ */
/** @file
 * VMM DBGF - Debugger Facility, Breakpoint Management.
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include <VBox/rem.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/mm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
static DECLCALLBACK(int) dbgfR3BpSetReg(PVM pVM, PCDBGFADDRESS pAddress, uint64_t *piHitTrigger, uint64_t *piHitDisable,
                                        uint8_t u8Type, uint8_t cb, PRTUINT piBp);
static DECLCALLBACK(int) dbgfR3BpSetInt3(PVM pVM, PCDBGFADDRESS pAddress, uint64_t *piHitTrigger, uint64_t *piHitDisable, PRTUINT piBp);
static DECLCALLBACK(int) dbgfR3BpSetREM(PVM pVM, PCDBGFADDRESS pAddress, uint64_t *piHitTrigger, uint64_t *piHitDisable, PRTUINT piBp);
static DECLCALLBACK(int) dbgfR3BpClear(PVM pVM, RTUINT iBp);
static DECLCALLBACK(int) dbgfR3BpEnable(PVM pVM, RTUINT iBp);
static DECLCALLBACK(int) dbgfR3BpDisable(PVM pVM, RTUINT iBp);
static DECLCALLBACK(int) dbgfR3BpEnum(PVM pVM, PFNDBGFBPENUM pfnCallback, void *pvUser);
static int dbgfR3BpRegArm(PVM pVM, PDBGFBP pBp);
static int dbgfR3BpRegDisarm(PVM pVM, PDBGFBP pBp);
static int dbgfR3BpInt3Arm(PVM pVM, PDBGFBP pBp);
static int dbgfR3BpInt3Disarm(PVM pVM, PDBGFBP pBp);
__END_DECLS


/**
 * Initialize the breakpoint stuff.
 *
 * @returns VINF_SUCCESS
 * @param   pVM     The VM handle.
 */
int dbgfR3BpInit(PVM pVM)
{
    /*
     * Init structures.
     */
    unsigned i;
    for (i = 0; i < ELEMENTS(pVM->dbgf.s.aHwBreakpoints); i++)
    {
        pVM->dbgf.s.aHwBreakpoints[i].iBp = i;
        pVM->dbgf.s.aHwBreakpoints[i].enmType = DBGFBPTYPE_FREE;
        pVM->dbgf.s.aHwBreakpoints[i].u.Reg.iReg = i;
    }

    for (i = 0; i < ELEMENTS(pVM->dbgf.s.aBreakpoints); i++)
    {
        pVM->dbgf.s.aBreakpoints[i].iBp = i + ELEMENTS(pVM->dbgf.s.aHwBreakpoints);
        pVM->dbgf.s.aBreakpoints[i].enmType = DBGFBPTYPE_FREE;
    }

    /*
     * Register saved state.
     */
    /** @todo */

    return VINF_SUCCESS;
}



/**
 * Allocate a breakpoint.
 *
 * @returns Pointer to the allocated breakpoint.
 * @returns NULL if we're out of breakpoints.
 * @param   pVM     The VM handle.
 * @param   enmType The type to allocate.
 */
static PDBGFBP dbgfR3BpAlloc(PVM pVM, DBGFBPTYPE enmType)
{
    /*
     * Determin which array to search.
     */
    unsigned cBps;
    PRTUINT  pcBpsCur;
    PDBGFBP  paBps;
    switch (enmType)
    {
        case DBGFBPTYPE_REG:
            cBps = ELEMENTS(pVM->dbgf.s.aHwBreakpoints);
            paBps = &pVM->dbgf.s.aHwBreakpoints[0];
            pcBpsCur = &pVM->dbgf.s.cHwBreakpoints;
            break;

        case DBGFBPTYPE_INT3:
        case DBGFBPTYPE_REM:
            cBps = ELEMENTS(pVM->dbgf.s.aBreakpoints);
            paBps = &pVM->dbgf.s.aBreakpoints[0];
            pcBpsCur = &pVM->dbgf.s.cBreakpoints;
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return NULL;
    }

    /*
     * Search.
     */
    for (unsigned iBp = 0; iBp < cBps; iBp++)
        if (paBps[iBp].enmType == DBGFBPTYPE_FREE)
        {
            ++*pcBpsCur;
            paBps[iBp].cHits   = 0;
            paBps[iBp].enmType = enmType;
            return &paBps[iBp];
        }

    LogFlow(("dbgfR3BpAlloc: returns NULL - we're out of breakpoint slots! %u/%u\n", *pcBpsCur, cBps));
    return NULL;
}


/**
 * Get a breakpoint give by breakpoint id.
 *
 * @returns Pointer to the allocated breakpoint.
 * @returns NULL if the breakpoint is invalid.
 * @param   pVM     The VM handle.
 * @param   iBp     The breakpoint id.
 */
static PDBGFBP dbgfR3BpGet(PVM pVM, RTUINT iBp)
{
    /* Find it. */
    PDBGFBP pBp;
    if (iBp < ELEMENTS(pVM->dbgf.s.aHwBreakpoints))
        pBp = &pVM->dbgf.s.aHwBreakpoints[iBp];
    else
    {
        iBp -= ELEMENTS(pVM->dbgf.s.aHwBreakpoints);
        if (iBp >= ELEMENTS(pVM->dbgf.s.aBreakpoints))
            return NULL;
        pBp = &pVM->dbgf.s.aBreakpoints[iBp];
    }

    /* check if it's valid. */
    switch (pBp->enmType)
    {
        case DBGFBPTYPE_FREE:
            return NULL;

        case DBGFBPTYPE_REG:
        case DBGFBPTYPE_INT3:
        case DBGFBPTYPE_REM:
            break;

        default:
            AssertMsgFailed(("Invalid enmType=%d!\n", pBp->enmType));
            return NULL;
    }

    return pBp;
}


/**
 * Get a breakpoint give by address.
 *
 * @returns Pointer to the allocated breakpoint.
 * @returns NULL if the breakpoint is invalid.
 * @param   pVM     The VM handle.
 * @param   enmType The breakpoint type.
 * @param   GCPtr   The breakpoint address.
 */
static PDBGFBP dbgfR3BpGetByAddr(PVM pVM, DBGFBPTYPE enmType, RTGCUINTPTR GCPtr)
{
    /*
     * Determin which array to search.
     */
    unsigned cBps;
    PDBGFBP  paBps;
    switch (enmType)
    {
        case DBGFBPTYPE_REG:
            cBps = ELEMENTS(pVM->dbgf.s.aHwBreakpoints);
            paBps = &pVM->dbgf.s.aHwBreakpoints[0];
            break;

        case DBGFBPTYPE_INT3:
        case DBGFBPTYPE_REM:
            cBps = ELEMENTS(pVM->dbgf.s.aBreakpoints);
            paBps = &pVM->dbgf.s.aBreakpoints[0];
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return NULL;
    }

    /*
     * Search.
     */
    for (unsigned iBp = 0; iBp < cBps; iBp++)
    {
        if (    paBps[iBp].enmType == enmType
            &&  paBps[iBp].GCPtr == GCPtr)
            return &paBps[iBp];
    }

    return NULL;
}


/**
 * Frees a breakpoint.
 *
 * @param   pVM     The VM handle.
 * @param   pBp     The breakpoint to free.
 */
static void dbgfR3BpFree(PVM pVM, PDBGFBP pBp)
{
    switch (pBp->enmType)
    {
        case DBGFBPTYPE_FREE:
            AssertMsgFailed(("Already freed!\n"));
            return;

        case DBGFBPTYPE_REG:
            Assert(pVM->dbgf.s.cHwBreakpoints > 0);
            pVM->dbgf.s.cHwBreakpoints--;
            break;

        case DBGFBPTYPE_INT3:
        case DBGFBPTYPE_REM:
            Assert(pVM->dbgf.s.cBreakpoints > 0);
            pVM->dbgf.s.cBreakpoints--;
            break;

        default:
            AssertMsgFailed(("Invalid enmType=%d!\n", pBp->enmType));
            return;

    }
    pBp->enmType = DBGFBPTYPE_FREE;
}


/**
 * Sets a breakpoint (int 3 based).
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pAddress    The address of the breakpoint.
 * @param   iHitTrigger The hit count at which the breakpoint start triggering.
 *                      Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable The hit count which disables the breakpoint.
 *                      Use ~(uint64_t) if it's never gonna be disabled.
 * @param   piBp        Where to store the breakpoint id. (optional)
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3BpSet(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpSetInt3, 5, pVM, pAddress, &iHitTrigger, &iHitDisable, piBp);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpSet: returns %Vrc\n", rc));
    return rc;
}


/**
 * Sets a breakpoint (int 3 based).
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pAddress        The address of the breakpoint.
 * @param   piHitTrigger    The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   piHitDisable    The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   piBp            Where to store the breakpoint id. (optional)
 * @thread  Any thread.
 */
static DECLCALLBACK(int) dbgfR3BpSetInt3(PVM pVM, PCDBGFADDRESS pAddress, uint64_t *piHitTrigger, uint64_t *piHitDisable, PRTUINT piBp)
{
    /*
     * Validate input.
     */
    if (!DBGFR3AddrIsValid(pVM, pAddress))
        return VERR_INVALID_PARAMETER;
    if (*piHitTrigger > *piHitDisable)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(!piBp || VALID_PTR(piBp), ("piBp=%p\n", piBp), VERR_INVALID_POINTER);
    if (piBp)
        *piBp = ~0;

    /*
     * Check if the breakpoint already exists.
     */
    PDBGFBP pBp = dbgfR3BpGetByAddr(pVM, DBGFBPTYPE_INT3, pAddress->FlatPtr);
    if (pBp)
    {
        int rc = VINF_SUCCESS;
        if (!pBp->fEnabled)
            rc = dbgfR3BpInt3Arm(pVM, pBp);
        if (VBOX_SUCCESS(rc))
        {
            rc = VINF_DBGF_BP_ALREADY_EXIST;
            if (piBp)
                *piBp = pBp->iBp;
        }
        return rc;
    }

    /*
     * Allocate and initialize the bp.
     */
    pBp = dbgfR3BpAlloc(pVM, DBGFBPTYPE_INT3);
    if (!pBp)
        return VERR_DBGF_NO_MORE_BP_SLOTS;
    pBp->GCPtr       = pAddress->FlatPtr;
    pBp->iHitTrigger = *piHitTrigger;
    pBp->iHitDisable = *piHitDisable;
    pBp->fEnabled    = true;

    /*
     * Now ask REM to set the breakpoint.
     */
    int rc = dbgfR3BpInt3Arm(pVM, pBp);
    if (VBOX_SUCCESS(rc))
    {
        if (piBp)
            *piBp = pBp->iBp;
    }
    else
        dbgfR3BpFree(pVM, pBp);

    return rc;
}


/**
 * Arms an int 3 breakpoint.
 * This is used to implement both DBGFR3BpSetReg() and DBGFR3BpEnable().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pBp         The breakpoint.
 */
static int dbgfR3BpInt3Arm(PVM pVM, PDBGFBP pBp)
{
    /** @todo should actually use physical address here! */
    /*
     * Save current byte and write int3 instruction.
     */
    int rc = MMR3ReadGCVirt(pVM, &pBp->u.Int3.bOrg, pBp->GCPtr, 1);
    if (VBOX_SUCCESS(rc))
    {
        static const uint8_t s_bInt3 = 0xcc;
        rc = MMR3WriteGCVirt(pVM, pBp->GCPtr, &s_bInt3, 1);
    }
    return rc;
}


/**
 * Disarms an int 3 breakpoint.
 * This is used to implement both DBGFR3BpClear() and DBGFR3BpDisable().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pBp         The breakpoint.
 */
static int dbgfR3BpInt3Disarm(PVM pVM, PDBGFBP pBp)
{
    /*
     * Check that the current byte is the int3 instruction, and restore the original one.
     * We currently ignore invalid bytes.
     */
    uint8_t bCurrent;
    int rc = MMR3ReadGCVirt(pVM, &bCurrent, pBp->GCPtr, 1);
    if (bCurrent == 0xcc)
        rc = MMR3WriteGCVirt(pVM, pBp->GCPtr, &pBp->u.Int3.bOrg, 1);
    return rc;
}


/**
 * Sets a register breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pAddress    The address of the breakpoint.
 * @param   iHitTrigger The hit count at which the breakpoint start triggering.
 *                      Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable The hit count which disables the breakpoint.
 *                      Use ~(uint64_t) if it's never gonna be disabled.
 * @param   fType       The access type (one of the X86_DR7_RW_* defines).
 * @param   cb          The access size - 1,2,4 or 8 (the latter is AMD64 long mode only.
 *                      Must be 1 if fType is X86_DR7_RW_EO.
 * @param   piBp        Where to store the breakpoint id. (optional)
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3BpSetReg(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                               uint8_t fType, uint8_t cb, PRTUINT piBp)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpSetReg, 7, pVM, pAddress, &iHitTrigger, &iHitDisable, fType, cb, piBp);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpSetReg: returns %Vrc\n", rc));
    return rc;

}


/**
 * Sets a register breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pAddress        The address of the breakpoint.
 * @param   piHitTrigger    The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   piHitDisable    The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   fType           The access type (one of the X86_DR7_RW_* defines).
 * @param   cb              The access size - 1,2,4 or 8 (the latter is AMD64 long mode only.
 *                          Must be 1 if fType is X86_DR7_RW_EO.
 * @param   piBp            Where to store the breakpoint id. (optional)
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpSetReg(PVM pVM, PCDBGFADDRESS pAddress, uint64_t *piHitTrigger, uint64_t *piHitDisable,
                                        uint8_t fType, uint8_t cb, PRTUINT piBp)
{
    /*
     * Validate input.
     */
    if (!DBGFR3AddrIsValid(pVM, pAddress))
        return VERR_INVALID_PARAMETER;
    if (*piHitTrigger > *piHitDisable)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(!piBp || VALID_PTR(piBp), ("piBp=%p\n", piBp), VERR_INVALID_POINTER);
    if (piBp)
        *piBp = ~0;
    switch (fType)
    {
        case X86_DR7_RW_EO:
            if (cb == 1)
                break;
            AssertMsgFailed(("fType=%#x cb=%d != 1\n", fType, cb));
            return VERR_INVALID_PARAMETER;
        case X86_DR7_RW_IO:
        case X86_DR7_RW_RW:
        case X86_DR7_RW_WO:
            break;
        default:
            AssertMsgFailed(("fType=%#x\n", fType));
            return VERR_INVALID_PARAMETER;
    }
    switch (cb)
    {
        case 1:
        case 2:
        case 4:
            break;
        default:
            AssertMsgFailed(("cb=%#x\n", cb));
            return VERR_INVALID_PARAMETER;
    }

    /*
     * Check if the breakpoint already exists.
     */
    PDBGFBP pBp = dbgfR3BpGetByAddr(pVM, DBGFBPTYPE_REG, pAddress->FlatPtr);
    if (    pBp
        &&  pBp->u.Reg.cb == cb
        &&  pBp->u.Reg.fType == fType)
    {
        int rc = VINF_SUCCESS;
        if (!pBp->fEnabled)
            rc = dbgfR3BpRegArm(pVM, pBp);
        if (VBOX_SUCCESS(rc))
        {
            rc = VINF_DBGF_BP_ALREADY_EXIST;
            if (piBp)
                *piBp = pBp->iBp;
        }
        return rc;
    }

    /*
     * Allocate and initialize the bp.
     */
    pBp = dbgfR3BpAlloc(pVM, DBGFBPTYPE_REG);
    if (!pBp)
        return VERR_DBGF_NO_MORE_BP_SLOTS;
    pBp->GCPtr       = pAddress->FlatPtr;
    pBp->iHitTrigger = *piHitTrigger;
    pBp->iHitDisable = *piHitDisable;
    pBp->fEnabled    = true;
    Assert(pBp->iBp == pBp->u.Reg.iReg);
    pBp->u.Reg.fType = fType;
    pBp->u.Reg.cb    = cb;

    /*
     * Arm the breakpoint.
     */
    int rc = dbgfR3BpRegArm(pVM, pBp);
    if (VBOX_SUCCESS(rc))
    {
        if (piBp)
            *piBp = pBp->iBp;
    }
    else
        dbgfR3BpFree(pVM, pBp);

    return rc;
}


/**
 * Arms a debug register breakpoint.
 * This is used to implement both DBGFR3BpSetReg() and DBGFR3BpEnable().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pBp         The breakpoint.
 */
static int dbgfR3BpRegArm(PVM pVM, PDBGFBP pBp)
{
    Assert(pBp->fEnabled);
    return CPUMRecalcHyperDRx(pVM);
}


/**
 * Disarms a debug register breakpoint.
 * This is used to implement both DBGFR3BpClear() and DBGFR3BpDisable().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pBp         The breakpoint.
 */
static int dbgfR3BpRegDisarm(PVM pVM, PDBGFBP pBp)
{
    Assert(!pBp->fEnabled);
    return CPUMRecalcHyperDRx(pVM);
}


/**
 * Sets a recompiler breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pAddress    The address of the breakpoint.
 * @param   iHitTrigger The hit count at which the breakpoint start triggering.
 *                      Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable The hit count which disables the breakpoint.
 *                      Use ~(uint64_t) if it's never gonna be disabled.
 * @param   piBp        Where to store the breakpoint id. (optional)
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3BpSetREM(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpSetREM, 5, pVM, pAddress, &iHitTrigger, &iHitDisable, piBp);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpSetREM: returns %Vrc\n", rc));
    return rc;
}


/**
 * EMT worker for DBGFR3BpSetREM().
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pAddress        The address of the breakpoint.
 * @param   piHitTrigger    The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   piHitDisable    The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   piBp            Where to store the breakpoint id. (optional)
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpSetREM(PVM pVM, PCDBGFADDRESS pAddress, uint64_t *piHitTrigger, uint64_t *piHitDisable, PRTUINT piBp)
{
    /*
     * Validate input.
     */
    if (!DBGFR3AddrIsValid(pVM, pAddress))
        return VERR_INVALID_PARAMETER;
    if (*piHitTrigger > *piHitDisable)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(!piBp || VALID_PTR(piBp), ("piBp=%p\n", piBp), VERR_INVALID_POINTER);
    if (piBp)
        *piBp = ~0;


    /*
     * Check if the breakpoint already exists.
     */
    PDBGFBP pBp = dbgfR3BpGetByAddr(pVM, DBGFBPTYPE_REM, pAddress->FlatPtr);
    if (pBp)
    {
        int rc = VINF_SUCCESS;
        if (!pBp->fEnabled)
            rc = REMR3BreakpointSet(pVM, pBp->GCPtr);
        if (VBOX_SUCCESS(rc))
        {
            rc = VINF_DBGF_BP_ALREADY_EXIST;
            if (piBp)
                *piBp = pBp->iBp;
        }
        return rc;
    }

    /*
     * Allocate and initialize the bp.
     */
    pBp = dbgfR3BpAlloc(pVM, DBGFBPTYPE_REM);
    if (!pBp)
        return VERR_DBGF_NO_MORE_BP_SLOTS;
    pBp->GCPtr       = pAddress->FlatPtr;
    pBp->iHitTrigger = *piHitTrigger;
    pBp->iHitDisable = *piHitDisable;
    pBp->fEnabled    = true;

    /*
     * Now ask REM to set the breakpoint.
     */
    int rc = REMR3BreakpointSet(pVM, pAddress->FlatPtr);
    if (VBOX_SUCCESS(rc))
    {
        if (piBp)
            *piBp = pBp->iBp;
    }
    else
        dbgfR3BpFree(pVM, pBp);

    return rc;
}


/**
 * Clears a breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   iBp         The id of the breakpoint which should be removed (cleared).
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3BpClear(PVM pVM, RTUINT iBp)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpClear, 2, pVM, iBp);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpClear: returns %Vrc\n", rc));
    return rc;
}


/**
 * EMT worker for DBGFR3BpClear().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   iBp         The id of the breakpoint which should be removed (cleared).
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpClear(PVM pVM, RTUINT iBp)
{
    /*
     * Validate input.
     */
    PDBGFBP pBp = dbgfR3BpGet(pVM, iBp);
    if (!pBp)
        return VERR_DBGF_BP_NOT_FOUND;

    /*
     * Disarm the breakpoint if it's enabled.
     */
    if (pBp->fEnabled)
    {
        pBp->fEnabled = false;
        int rc;
        switch (pBp->enmType)
        {
            case DBGFBPTYPE_REG:
                rc = dbgfR3BpRegDisarm(pVM, pBp);
                break;

            case DBGFBPTYPE_INT3:
                rc = dbgfR3BpInt3Disarm(pVM, pBp);
                break;

            case DBGFBPTYPE_REM:
                rc = REMR3BreakpointClear(pVM, pBp->GCPtr);
                break;

            default:
                AssertMsgFailed(("Invalid enmType=%d!\n", pBp->enmType));
                return VERR_INTERNAL_ERROR;
        }
        AssertRCReturn(rc, rc);
    }

    /*
     * Free the breakpoint.
     */
    dbgfR3BpFree(pVM, pBp);
    return VINF_SUCCESS;
}


/**
 * Enables a breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   iBp         The id of the breakpoint which should be enabled.
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3BpEnable(PVM pVM, RTUINT iBp)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpEnable, 2, pVM, iBp);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpEnable: returns %Vrc\n", rc));
    return rc;
}


/**
 * EMT worker for DBGFR3BpEnable().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   iBp         The id of the breakpoint which should be enabled.
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpEnable(PVM pVM, RTUINT iBp)
{
    /*
     * Validate input.
     */
    PDBGFBP pBp = dbgfR3BpGet(pVM, iBp);
    if (!pBp)
        return VERR_DBGF_BP_NOT_FOUND;

    /*
     * Already enabled?
     */
    if (pBp->fEnabled)
        return VINF_DBGF_BP_ALREADY_ENABLED;

    /*
     * Remove the breakpoint.
     */
    int rc;
    pBp->fEnabled = true;
    switch (pBp->enmType)
    {
        case DBGFBPTYPE_REG:
            rc = dbgfR3BpRegArm(pVM, pBp);
            break;

        case DBGFBPTYPE_INT3:
            rc = dbgfR3BpInt3Arm(pVM, pBp);
            break;

        case DBGFBPTYPE_REM:
            rc = REMR3BreakpointSet(pVM, pBp->GCPtr);
            break;

        default:
            AssertMsgFailed(("Invalid enmType=%d!\n", pBp->enmType));
            return VERR_INTERNAL_ERROR;
    }
    if (VBOX_FAILURE(rc))
        pBp->fEnabled = false;

    return rc;
}


/**
 * Disables a breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   iBp         The id of the breakpoint which should be disabled.
 * @thread  Any thread.
 */
DBGFR3DECL(int) DBGFR3BpDisable(PVM pVM, RTUINT iBp)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpDisable, 2, pVM, iBp);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpDisable: returns %Vrc\n", rc));
    return rc;
}


/**
 * EMT worker for DBGFR3BpDisable().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   iBp         The id of the breakpoint which should be disabled.
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpDisable(PVM pVM, RTUINT iBp)
{
    /*
     * Validate input.
     */
    PDBGFBP pBp = dbgfR3BpGet(pVM, iBp);
    if (!pBp)
        return VERR_DBGF_BP_NOT_FOUND;

    /*
     * Already enabled?
     */
    if (!pBp->fEnabled)
        return VINF_DBGF_BP_ALREADY_DISABLED;

    /*
     * Remove the breakpoint.
     */
    pBp->fEnabled = false;
    int rc;
    switch (pBp->enmType)
    {
        case DBGFBPTYPE_REG:
            rc = dbgfR3BpRegDisarm(pVM, pBp);
            break;

        case DBGFBPTYPE_INT3:
            rc = dbgfR3BpInt3Disarm(pVM, pBp);
            break;

        case DBGFBPTYPE_REM:
            rc = REMR3BreakpointClear(pVM, pBp->GCPtr);
            break;

        default:
            AssertMsgFailed(("Invalid enmType=%d!\n", pBp->enmType));
            return VERR_INTERNAL_ERROR;
    }

    return rc;
}


/**
 * Enumerate the breakpoints.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pfnCallback The callback function.
 * @param   pvUser      The user argument to pass to the callback.
 * @thread  Any thread but the callback will be called from EMT.
 */
DBGFR3DECL(int) DBGFR3BpEnum(PVM pVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    /*
     * This must be done in EMT.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3BpEnum, 3, pVM, pfnCallback, pvUser);
    if (VBOX_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    LogFlow(("DBGFR3BpClear: returns %Vrc\n", rc));
    return rc;
}


/**
 * EMT worker for DBGFR3BpEnum().
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pfnCallback The callback function.
 * @param   pvUser      The user argument to pass to the callback.
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpEnum(PVM pVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pfnCallback), ("pfnCallback=%p\n", pfnCallback), VERR_INVALID_POINTER);

    /*
     * Enumerate the hardware breakpoints.
     */
    unsigned i;
    for (i = 0; i < ELEMENTS(pVM->dbgf.s.aHwBreakpoints); i++)
        if (pVM->dbgf.s.aHwBreakpoints[i].enmType != DBGFBPTYPE_FREE)
        {
            int rc = pfnCallback(pVM, pvUser, &pVM->dbgf.s.aHwBreakpoints[i]);
            if (VBOX_FAILURE(rc))
                return rc;
        }

    /*
     * Enumerate the other breakpoints.
     */
    for (i = 0; i < ELEMENTS(pVM->dbgf.s.aBreakpoints); i++)
        if (pVM->dbgf.s.aBreakpoints[i].enmType != DBGFBPTYPE_FREE)
        {
            int rc = pfnCallback(pVM, pvUser, &pVM->dbgf.s.aBreakpoints[i]);
            if (VBOX_FAILURE(rc))
                return rc;
        }

    return VINF_SUCCESS;
}

