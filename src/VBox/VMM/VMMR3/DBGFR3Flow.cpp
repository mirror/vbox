/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Control Flow Graph Interface (CFG).
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_dbgf_cfg    DBGFR3Flow - Control Flow Graph Interface
 *
 * The control flow graph interface provides an API to disassemble
 * guest code providing the result in a control flow graph.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vmm/mm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/param.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/sort.h>
#include <iprt/strcache.h>

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Internal control flow graph state.
 */
typedef struct DBGFFLOWINT
{
    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** Internal reference counter for basic blocks. */
    uint32_t volatile       cRefsBb;
    /** List of all basic blocks. */
    RTLISTANCHOR            LstFlowBb;
    /** Number of basic blocks in this control flow graph. */
    uint32_t                cBbs;
    /** The lowest addres of a basic block. */
    DBGFADDRESS             AddrLowest;
    /** The highest address of a basic block. */
    DBGFADDRESS             AddrHighest;
    /** String cache for disassembled instructions. */
    RTSTRCACHE              hStrCacheInstr;
} DBGFFLOWINT;
/** Pointer to an internal control flow graph state. */
typedef DBGFFLOWINT *PDBGFFLOWINT;

/**
 * Instruction record
 */
typedef struct DBGFFLOWBBINSTR
{
    /** Instruction address. */
    DBGFADDRESS             AddrInstr;
    /** Size of instruction. */
    uint32_t                cbInstr;
    /** Disassembled instruction string. */
    const char              *pszInstr;
} DBGFFLOWBBINSTR;
/** Pointer to an instruction record. */
typedef DBGFFLOWBBINSTR *PDBGFFLOWBBINSTR;

/**
 * Internal control flow graph basic block state.
 */
typedef struct DBGFFLOWBBINT
{
    /** Node for the list of all basic blocks. */
    RTLISTNODE               NdFlowBb;
    /** The control flow graph the basic block belongs to. */
    PDBGFFLOWINT             pFlow;
    /** Reference counter. */
    uint32_t volatile        cRefs;
    /** Basic block end type. */
    DBGFFLOWBBENDTYPE        enmEndType;
    /** Start address of this basic block. */
    DBGFADDRESS              AddrStart;
    /** End address of this basic block. */
    DBGFADDRESS              AddrEnd;
    /** Address of the block succeeding.
     *  This is valid for conditional jumps
     * (the other target is referenced by AddrEnd+1) and
     * unconditional jumps (not ret, iret, etc.) except
     * if we can't infer the jump target (jmp *eax for example). */
    DBGFADDRESS              AddrTarget;
    /** Last status error code if DBGF_FLOW_BB_F_INCOMPLETE_ERR is set. */
    int                      rcError;
    /** Error message if DBGF_FLOW_BB_F_INCOMPLETE_ERR is set. */
    char                     *pszErr;
    /** Flags for this basic block. */
    uint32_t                 fFlags;
    /** Number of instructions in this basic block. */
    uint32_t                 cInstr;
    /** Maximum number of instruction records for this basic block. */
    uint32_t                 cInstrMax;
    /** Instruction records, variable in size. */
    DBGFFLOWBBINSTR          aInstr[1];
} DBGFFLOWBBINT;
/** Pointer to an internal control flow graph basic block state. */
typedef DBGFFLOWBBINT *PDBGFFLOWBBINT;

/**
 * Control flow graph iterator state.
 */
typedef struct DBGFFLOWITINT
{
    /** Pointer to the control flow graph (holding a reference). */
    PDBGFFLOWINT             pFlow;
    /** Next basic block to return. */
    uint32_t                 idxBbNext;
    /** Array of basic blocks sorted by the specified order - variable in size. */
    PDBGFFLOWBBINT           apBb[1];
} DBGFFLOWITINT;
/** Pointer to the internal control flow graph iterator state. */
typedef DBGFFLOWITINT *PDBGFFLOWITINT;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static uint32_t dbgfR3FlowBbReleaseInt(PDBGFFLOWBBINT pFlowBb, bool fMayDestroyFlow);

/**
 * Creates a new basic block.
 *
 * @returns Pointer to the basic block on success or NULL if out of memory.
 * @param   pThis               The control flow graph.
 * @param   pAddrStart          The start of the basic block.
 * @param   cInstrMax           Maximum number of instructions this block can hold initially.
 */
static PDBGFFLOWBBINT dbgfR3FlowBbCreate(PDBGFFLOWINT pThis, PDBGFADDRESS pAddrStart, uint32_t cInstrMax)
{
    PDBGFFLOWBBINT pFlowBb = (PDBGFFLOWBBINT)RTMemAllocZ(RT_OFFSETOF(DBGFFLOWBBINT, aInstr[cInstrMax]));
    if (RT_LIKELY(pFlowBb))
    {
        RTListInit(&pFlowBb->NdFlowBb);
        pFlowBb->cRefs      = 1;
        pFlowBb->enmEndType = DBGFFLOWBBENDTYPE_INVALID;
        pFlowBb->pFlow       = pThis;
        pFlowBb->fFlags     = DBGF_FLOW_BB_F_EMPTY;
        pFlowBb->AddrStart  = *pAddrStart;
        pFlowBb->AddrEnd    = *pAddrStart;
        pFlowBb->rcError    = VINF_SUCCESS;
        pFlowBb->pszErr     = NULL;
        pFlowBb->cInstr     = 0;
        pFlowBb->cInstrMax  = cInstrMax;
        ASMAtomicIncU32(&pThis->cRefsBb);
    }

    return pFlowBb;
}


/**
 * Destroys a control flow graph.
 *
 * @returns nothing.
 * @param   pThis               The control flow graph to destroy.
 */
static void dbgfR3FlowDestroy(PDBGFFLOWINT pThis)
{
    /* Defer destruction if there are still basic blocks referencing us. */
    PDBGFFLOWBBINT pFlowBb = NULL;
    PDBGFFLOWBBINT pFlowBbNext = NULL;
    RTListForEachSafe(&pThis->LstFlowBb, pFlowBb, pFlowBbNext, DBGFFLOWBBINT, NdFlowBb)
    {
        dbgfR3FlowBbReleaseInt(pFlowBb, false /*fMayDestroyFlow*/);
    }

    Assert(!pThis->cRefs);
    if (!pThis->cRefsBb)
    {
        RTStrCacheDestroy(pThis->hStrCacheInstr);
        RTMemFree(pThis);
    }
}


/**
 * Destroys a basic block.
 *
 * @returns nothing.
 * @param   pFlowBb              The basic block to destroy.
 * @param   fMayDestroyFlow      Flag whether the control flow graph container
 *                               should be destroyed when there is nothing referencing it.
 */
static void dbgfR3FlowBbDestroy(PDBGFFLOWBBINT pFlowBb, bool fMayDestroyFlow)
{
    PDBGFFLOWINT pThis = pFlowBb->pFlow;

    RTListNodeRemove(&pFlowBb->NdFlowBb);
    pThis->cBbs--;
    for (uint32_t idxInstr = 0; idxInstr < pFlowBb->cInstr; idxInstr++)
        RTStrCacheRelease(pThis->hStrCacheInstr, pFlowBb->aInstr[idxInstr].pszInstr);
    uint32_t cRefsBb = ASMAtomicDecU32(&pThis->cRefsBb);
    RTMemFree(pFlowBb);

    if (!cRefsBb && !pThis->cRefs && fMayDestroyFlow)
        dbgfR3FlowDestroy(pThis);
}


/**
 * Internal basic block release worker.
 *
 * @returns New reference count of the released basic block, on 0
 *          it is destroyed.
 * @param   pFlowBb              The basic block to release.
 * @param   fMayDestroyFlow      Flag whether the control flow graph container
 *                               should be destroyed when there is nothing referencing it.
 */
static uint32_t dbgfR3FlowBbReleaseInt(PDBGFFLOWBBINT pFlowBb, bool fMayDestroyFlow)
{
    uint32_t cRefs = ASMAtomicDecU32(&pFlowBb->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p %d\n", cRefs, pFlowBb, pFlowBb->enmEndType));
    if (cRefs == 0)
        dbgfR3FlowBbDestroy(pFlowBb, fMayDestroyFlow);
    return cRefs;
}


/**
 * Links the given basic block into the control flow graph.
 *
 * @returns nothing.
 * @param   pThis               The control flow graph to link into.
 * @param   pFlowBb              The basic block to link.
 */
DECLINLINE(void) dbgfR3FlowLink(PDBGFFLOWINT pThis, PDBGFFLOWBBINT pFlowBb)
{
    RTListAppend(&pThis->LstFlowBb, &pFlowBb->NdFlowBb);
    pThis->cBbs++;
}


/**
 * Returns the first unpopulated basic block of the given control flow graph.
 *
 * @returns The first unpopulated control flow graph or NULL if not found.
 * @param   pThis               The control flow graph.
 */
DECLINLINE(PDBGFFLOWBBINT) dbgfR3FlowGetUnpopulatedBb(PDBGFFLOWINT pThis)
{
    PDBGFFLOWBBINT pFlowBb = NULL;
    RTListForEach(&pThis->LstFlowBb, pFlowBb, DBGFFLOWBBINT, NdFlowBb)
    {
        if (pFlowBb->fFlags & DBGF_FLOW_BB_F_EMPTY)
            return pFlowBb;
    }

    return NULL;
}


/**
 * Resolves the jump target address if possible from the given instruction address
 * and instruction parameter.
 *
 * @returns VBox status code.
 * @param   pUVM                The usermode VM handle.
 * @param   idCpu               CPU id for resolving the address.
 * @param   pDisParam           The parmeter from the disassembler.
 * @param   pAddrInstr          The instruction address.
 * @param   cbInstr             Size of instruction in bytes.
 * @param   fRelJmp             Flag whether this is a reltive jump.
 * @param   pAddrJmpTarget      Where to store the address to the jump target on success.
 */
static int dbgfR3FlowQueryJmpTarget(PUVM pUVM, VMCPUID idCpu, PDISOPPARAM pDisParam, PDBGFADDRESS pAddrInstr,
                                   uint32_t cbInstr, bool fRelJmp, PDBGFADDRESS pAddrJmpTarget)
{
    int rc = VINF_SUCCESS;

    /* Relative jumps are always from the beginning of the next instruction. */
    *pAddrJmpTarget = *pAddrInstr;
    DBGFR3AddrAdd(pAddrJmpTarget, cbInstr);

    if (fRelJmp)
    {
        RTGCINTPTR iRel = 0;
        if (pDisParam->fUse & DISUSE_IMMEDIATE8_REL)
            iRel = (int8_t)pDisParam->uValue;
        else if (pDisParam->fUse & DISUSE_IMMEDIATE16_REL)
            iRel = (int16_t)pDisParam->uValue;
        else if (pDisParam->fUse & DISUSE_IMMEDIATE32_REL)
            iRel = (int32_t)pDisParam->uValue;
        else if (pDisParam->fUse & DISUSE_IMMEDIATE64_REL)
            iRel = (int64_t)pDisParam->uValue;
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);

        if (iRel < 0)
            DBGFR3AddrSub(pAddrJmpTarget, -iRel);
        else
            DBGFR3AddrAdd(pAddrJmpTarget, iRel);
    }
    else
    {
        if (pDisParam->fUse & (DISUSE_IMMEDIATE8 | DISUSE_IMMEDIATE16 | DISUSE_IMMEDIATE32 | DISUSE_IMMEDIATE64))
        {
            if (DBGFADDRESS_IS_FLAT(pAddrInstr))
                DBGFR3AddrFromFlat(pUVM, pAddrJmpTarget, pDisParam->uValue);
            else
                DBGFR3AddrFromSelOff(pUVM, idCpu, pAddrJmpTarget, pAddrInstr->Sel, pDisParam->uValue);
        }
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
    }

    return rc;
}


/**
 * Checks whether both addresses are equal.
 *
 * @returns true if both addresses point to the same location, false otherwise.
 * @param   pAddr1              First address.
 * @param   pAddr2              Second address.
 */
static bool dbgfR3FlowBbAddrEqual(PDBGFADDRESS pAddr1, PDBGFADDRESS pAddr2)
{
    return    pAddr1->Sel == pAddr2->Sel
           && pAddr1->off == pAddr2->off;
}


/**
 * Checks whether the first given address is lower than the second one.
 *
 * @returns true if both addresses point to the same location, false otherwise.
 * @param   pAddr1              First address.
 * @param   pAddr2              Second address.
 */
static bool dbgfR3FlowBbAddrLower(PDBGFADDRESS pAddr1, PDBGFADDRESS pAddr2)
{
    return    pAddr1->Sel == pAddr2->Sel
           && pAddr1->off < pAddr2->off;
}


/**
 * Checks whether the given basic block and address intersect.
 *
 * @returns true if they intersect, false otherwise.
 * @param   pFlowBb              The basic block to check.
 * @param   pAddr               The address to check for.
 */
static bool dbgfR3FlowBbAddrIntersect(PDBGFFLOWBBINT pFlowBb, PDBGFADDRESS pAddr)
{
    return    (pFlowBb->AddrStart.Sel == pAddr->Sel)
           && (pFlowBb->AddrStart.off <= pAddr->off)
           && (pFlowBb->AddrEnd.off >= pAddr->off);
}


/**
 * Checks whether the given control flow graph contains a basic block
 * with the given start address.
 *
 * @returns true if there is a basic block with the start address, false otherwise.
 * @param   pThis               The control flow graph.
 * @param   pAddr               The address to check for.
 */
static bool dbgfR3FlowHasBbWithStartAddr(PDBGFFLOWINT pThis, PDBGFADDRESS pAddr)
{
    PDBGFFLOWBBINT pFlowBb = NULL;
    RTListForEach(&pThis->LstFlowBb, pFlowBb, DBGFFLOWBBINT, NdFlowBb)
    {
        if (dbgfR3FlowBbAddrEqual(&pFlowBb->AddrStart, pAddr))
            return true;
    }
    return false;
}

/**
 * Splits a given basic block into two at the given address.
 *
 * @returns VBox status code.
 * @param   pThis               The control flow graph.
 * @param   pFlowBb              The basic block to split.
 * @param   pAddr               The address to split at.
 */
static int dbgfR3FlowBbSplit(PDBGFFLOWINT pThis, PDBGFFLOWBBINT pFlowBb, PDBGFADDRESS pAddr)
{
    int rc = VINF_SUCCESS;
    uint32_t idxInstrSplit;

    /* If the block is empty it will get populated later so there is nothing to split,
     * same if the start address equals. */
    if (   pFlowBb->fFlags & DBGF_FLOW_BB_F_EMPTY
        || dbgfR3FlowBbAddrEqual(&pFlowBb->AddrStart, pAddr))
        return VINF_SUCCESS;

    /* Find the instruction to split at. */
    for (idxInstrSplit = 1; idxInstrSplit < pFlowBb->cInstr; idxInstrSplit++)
        if (dbgfR3FlowBbAddrEqual(&pFlowBb->aInstr[idxInstrSplit].AddrInstr, pAddr))
            break;

    Assert(idxInstrSplit > 0);

    /*
     * Given address might not be on instruction boundary, this is not supported
     * so far and results in an error.
     */
    if (idxInstrSplit < pFlowBb->cInstr)
    {
        /* Create new basic block. */
        uint32_t cInstrNew = pFlowBb->cInstr - idxInstrSplit;
        PDBGFFLOWBBINT pFlowBbNew = dbgfR3FlowBbCreate(pThis, &pFlowBb->aInstr[idxInstrSplit].AddrInstr,
                                                    cInstrNew);
        if (pFlowBbNew)
        {
            /* Move instructions over. */
            pFlowBbNew->cInstr     = cInstrNew;
            pFlowBbNew->AddrEnd    = pFlowBb->AddrEnd;
            pFlowBbNew->enmEndType = pFlowBb->enmEndType;
            pFlowBbNew->fFlags     = pFlowBb->fFlags & ~DBGF_FLOW_BB_F_ENTRY;

            /* Move any error to the new basic block and clear them in the old basic block. */
            pFlowBbNew->rcError    = pFlowBb->rcError;
            pFlowBbNew->pszErr     = pFlowBb->pszErr;
            pFlowBb->rcError       = VINF_SUCCESS;
            pFlowBb->pszErr        = NULL;
            pFlowBb->fFlags       &= ~DBGF_FLOW_BB_F_INCOMPLETE_ERR;

            memcpy(&pFlowBbNew->aInstr[0], &pFlowBb->aInstr[idxInstrSplit], cInstrNew * sizeof(DBGFFLOWBBINSTR));
            pFlowBb->cInstr     = idxInstrSplit;
            pFlowBb->enmEndType = DBGFFLOWBBENDTYPE_UNCOND;
            pFlowBb->AddrEnd    = pFlowBb->aInstr[idxInstrSplit-1].AddrInstr;
            pFlowBb->AddrTarget = pFlowBbNew->AddrStart;
            DBGFR3AddrAdd(&pFlowBb->AddrEnd, pFlowBb->aInstr[idxInstrSplit-1].cbInstr - 1);
            RT_BZERO(&pFlowBb->aInstr[idxInstrSplit], cInstrNew * sizeof(DBGFFLOWBBINSTR));

            dbgfR3FlowLink(pThis, pFlowBbNew);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        AssertFailedStmt(rc = VERR_INVALID_STATE); /** @todo: Proper status code. */

    return rc;
}


/**
 * Makes sure there is an successor at the given address splitting already existing
 * basic blocks if they intersect.
 *
 * @returns VBox status code.
 * @param   pThis               The control flow graph.
 * @param   pAddrSucc           The guest address the new successor should start at.
 */
static int dbgfR3FlowBbSuccessorAdd(PDBGFFLOWINT pThis, PDBGFADDRESS pAddrSucc)
{
    PDBGFFLOWBBINT pFlowBb = NULL;
    RTListForEach(&pThis->LstFlowBb, pFlowBb, DBGFFLOWBBINT, NdFlowBb)
    {
        /*
         * The basic block must be split if it intersects with the given address
         * and the start address does not equal the given one.
         */
        if (dbgfR3FlowBbAddrIntersect(pFlowBb, pAddrSucc))
            return dbgfR3FlowBbSplit(pThis, pFlowBb, pAddrSucc);
    }

    int rc = VINF_SUCCESS;
    pFlowBb = dbgfR3FlowBbCreate(pThis, pAddrSucc, 10);
    if (pFlowBb)
        dbgfR3FlowLink(pThis, pFlowBb);
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Sets the given error status for the basic block.
 *
 * @returns nothing.
 * @param   pFlowBb              The basic block causing the error.
 * @param   rcError             The error to set.
 * @param   pszFmt              Format string of the error description.
 * @param   ...                 Arguments for the format string.
 */
static void dbgfR3FlowBbSetError(PDBGFFLOWBBINT pFlowBb, int rcError, const char *pszFmt, ...)
{
    va_list va;
    va_start(va, pszFmt);

    Assert(!(pFlowBb->fFlags & DBGF_FLOW_BB_F_INCOMPLETE_ERR));
    pFlowBb->fFlags |= DBGF_FLOW_BB_F_INCOMPLETE_ERR;
    pFlowBb->fFlags &= ~DBGF_FLOW_BB_F_EMPTY;
    pFlowBb->rcError = rcError;
    pFlowBb->pszErr = RTStrAPrintf2V(pszFmt, va);
    va_end(va);
}


/**
 * Processes and fills one basic block.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   idCpu               CPU id for disassembling.
 * @param   pThis               The control flow graph to populate.
 * @param   pFlowBb              The basic block to fill.
 * @param   cbDisasmMax         The maximum amount to disassemble.
 * @param   fFlags              Combination of DBGF_DISAS_FLAGS_*.
 */
static int dbgfR3FlowBbProcess(PUVM pUVM, VMCPUID idCpu, PDBGFFLOWINT pThis, PDBGFFLOWBBINT pFlowBb,
                              uint32_t cbDisasmMax, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    uint32_t cbDisasmLeft = cbDisasmMax ? cbDisasmMax : UINT32_MAX;
    DBGFADDRESS AddrDisasm = pFlowBb->AddrEnd;

    Assert(pFlowBb->fFlags & DBGF_FLOW_BB_F_EMPTY);

    /*
     * Disassemble instruction by instruction until we get a conditional or
     * unconditional jump or some sort of return.
     */
    while (   cbDisasmLeft
           && RT_SUCCESS(rc))
    {
        DBGFDISSTATE DisState;
        char szOutput[_4K];

        /*
         * Before disassembling we have to check whether the address belongs
         * to another basic block and stop here.
         */
        if (   !(pFlowBb->fFlags & DBGF_FLOW_BB_F_EMPTY)
            && dbgfR3FlowHasBbWithStartAddr(pThis, &AddrDisasm))
        {
            pFlowBb->AddrTarget = AddrDisasm;
            pFlowBb->enmEndType = DBGFFLOWBBENDTYPE_UNCOND;
            break;
        }

        pFlowBb->fFlags &= ~DBGF_FLOW_BB_F_EMPTY;

        rc = dbgfR3DisasInstrStateEx(pUVM, idCpu, &AddrDisasm, fFlags,
                                     &szOutput[0], sizeof(szOutput), &DisState);
        if (RT_SUCCESS(rc))
        {
            cbDisasmLeft -= DisState.cbInstr;

            if (pFlowBb->cInstr == pFlowBb->cInstrMax)
            {
                /* Reallocate. */
                RTListNodeRemove(&pFlowBb->NdFlowBb);
                PDBGFFLOWBBINT pFlowBbNew = (PDBGFFLOWBBINT)RTMemRealloc(pFlowBb, RT_OFFSETOF(DBGFFLOWBBINT, aInstr[pFlowBb->cInstrMax + 10]));
                if (pFlowBbNew)
                {
                    pFlowBbNew->cInstrMax += 10;
                    pFlowBb = pFlowBbNew;
                }
                else
                    rc = VERR_NO_MEMORY;
                RTListAppend(&pThis->LstFlowBb, &pFlowBb->NdFlowBb);
            }

            if (RT_SUCCESS(rc))
            {
                PDBGFFLOWBBINSTR pInstr = &pFlowBb->aInstr[pFlowBb->cInstr];

                pInstr->AddrInstr = AddrDisasm;
                pInstr->cbInstr   = DisState.cbInstr;
                pInstr->pszInstr  = RTStrCacheEnter(pThis->hStrCacheInstr, &szOutput[0]);
                pFlowBb->cInstr++;

                pFlowBb->AddrEnd = AddrDisasm;
                DBGFR3AddrAdd(&pFlowBb->AddrEnd, pInstr->cbInstr - 1);
                DBGFR3AddrAdd(&AddrDisasm, pInstr->cbInstr);

                /*
                 * Check control flow instructions and create new basic blocks
                 * marking the current one as complete.
                 */
                if (DisState.pCurInstr->fOpType & DISOPTYPE_CONTROLFLOW)
                {
                    uint16_t uOpc = DisState.pCurInstr->uOpcode;

                    if (   uOpc == OP_RETN || uOpc == OP_RETF || uOpc == OP_IRET
                        || uOpc == OP_SYSEXIT || uOpc == OP_SYSRET)
                        pFlowBb->enmEndType = DBGFFLOWBBENDTYPE_EXIT;
                    else if (uOpc == OP_JMP)
                    {
                        Assert(DisState.pCurInstr->fOpType & DISOPTYPE_UNCOND_CONTROLFLOW);
                        pFlowBb->enmEndType = DBGFFLOWBBENDTYPE_UNCOND_JMP;

                        /* Create one new basic block with the jump target address. */
                        rc = dbgfR3FlowQueryJmpTarget(pUVM, idCpu, &DisState.Param1, &pInstr->AddrInstr, pInstr->cbInstr,
                                                     RT_BOOL(DisState.pCurInstr->fOpType & DISOPTYPE_RELATIVE_CONTROLFLOW),
                                                     &pFlowBb->AddrTarget);
                        if (RT_SUCCESS(rc))
                            rc = dbgfR3FlowBbSuccessorAdd(pThis, &pFlowBb->AddrTarget);
                    }
                    else if (uOpc != OP_CALL)
                    {
                        Assert(DisState.pCurInstr->fOpType & DISOPTYPE_COND_CONTROLFLOW);
                        pFlowBb->enmEndType = DBGFFLOWBBENDTYPE_COND;

                        /*
                         * Create two new basic blocks, one with the jump target address
                         * and one starting after the current instruction.
                         */
                        rc = dbgfR3FlowBbSuccessorAdd(pThis, &AddrDisasm);
                        if (RT_SUCCESS(rc))
                        {
                            rc = dbgfR3FlowQueryJmpTarget(pUVM, idCpu, &DisState.Param1, &pInstr->AddrInstr, pInstr->cbInstr, 
                                                         RT_BOOL(DisState.pCurInstr->fOpType & DISOPTYPE_RELATIVE_CONTROLFLOW),
                                                         &pFlowBb->AddrTarget);
                            if (RT_SUCCESS(rc))
                                rc = dbgfR3FlowBbSuccessorAdd(pThis, &pFlowBb->AddrTarget);
                        }
                    }

                    if (RT_FAILURE(rc))
                        dbgfR3FlowBbSetError(pFlowBb, rc, "Adding successor blocks failed with %Rrc", rc);

                    /* Quit disassembling. */
                    if (   uOpc != OP_CALL
                        || RT_FAILURE(rc))
                        break;
                }
            }
            else
                dbgfR3FlowBbSetError(pFlowBb, rc, "Increasing basic block failed with %Rrc", rc);
        }
        else
            dbgfR3FlowBbSetError(pFlowBb, rc, "Disassembling the instruction failed with %Rrc", rc);
    }

    return VINF_SUCCESS;
}

/**
 * Populate all empty basic blocks.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   idCpu               CPU id for disassembling.
 * @param   pThis               The control flow graph to populate.
 * @param   pAddrStart          The start address to disassemble at.
 * @param   cbDisasmMax         The maximum amount to disassemble.
 * @param   fFlags              Combination of DBGF_DISAS_FLAGS_*.
 */
static int dbgfR3FlowPopulate(PUVM pUVM, VMCPUID idCpu, PDBGFFLOWINT pThis, PDBGFADDRESS pAddrStart,
                             uint32_t cbDisasmMax, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    PDBGFFLOWBBINT pFlowBb = dbgfR3FlowGetUnpopulatedBb(pThis);
    DBGFADDRESS AddrEnd = *pAddrStart;
    DBGFR3AddrAdd(&AddrEnd, cbDisasmMax);

    while (VALID_PTR(pFlowBb))
    {
        rc = dbgfR3FlowBbProcess(pUVM, idCpu, pThis, pFlowBb, cbDisasmMax, fFlags);
        if (RT_FAILURE(rc))
            break;

        pFlowBb = dbgfR3FlowGetUnpopulatedBb(pThis);
    }

    return rc;
}

/**
 * Creates a new control flow graph from the given start address.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   idCpu               CPU id for disassembling.
 * @param   pAddressStart       Where to start creating the control flow graph.
 * @param   cbDisasmMax         Limit the amount of bytes to disassemble, 0 for no limit.
 * @param   fFlags              Combination of DBGF_DISAS_FLAGS_*.
 * @param   phFlow              Where to store the handle to the control flow graph on success.
 */
VMMR3DECL(int) DBGFR3FlowCreate(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddressStart, uint32_t cbDisasmMax,
                               uint32_t fFlags, PDBGFFLOW phFlow)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pUVM->cCpus, VERR_INVALID_CPU_ID);
    AssertPtrReturn(pAddressStart, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~DBGF_DISAS_FLAGS_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & DBGF_DISAS_FLAGS_MODE_MASK) <= DBGF_DISAS_FLAGS_64BIT_MODE, VERR_INVALID_PARAMETER);

    /* Create the control flow graph container. */
    int rc = VINF_SUCCESS;
    PDBGFFLOWINT pThis = (PDBGFFLOWINT)RTMemAllocZ(sizeof(DBGFFLOWINT));
    if (RT_LIKELY(pThis))
    {
        rc = RTStrCacheCreate(&pThis->hStrCacheInstr, "DBGFFLOW");
        if (RT_SUCCESS(rc))
        {
            pThis->cRefs   = 1;
            pThis->cRefsBb = 0;
            pThis->cBbs    = 0;
            RTListInit(&pThis->LstFlowBb);
            /* Create the entry basic block and start the work. */

            PDBGFFLOWBBINT pFlowBb = dbgfR3FlowBbCreate(pThis, pAddressStart, 10);
            if (RT_LIKELY(pFlowBb))
            {
                pFlowBb->fFlags |= DBGF_FLOW_BB_F_ENTRY;
                dbgfR3FlowLink(pThis, pFlowBb);
                rc = dbgfR3FlowPopulate(pUVM, idCpu, pThis, pAddressStart, cbDisasmMax, fFlags);
                if (RT_SUCCESS(rc))
                {
                    *phFlow = pThis;
                    return VINF_SUCCESS;
                }
            }
            else
                rc = VERR_NO_MEMORY;
        }

        ASMAtomicDecU32(&pThis->cRefs);
        dbgfR3FlowDestroy(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Retains the control flow graph handle.
 *
 * @returns Current reference count.
 * @param   hFlow                The control flow graph handle to retain.
 */
VMMR3DECL(uint32_t) DBGFR3FlowRetain(DBGFFLOW hFlow)
{
    PDBGFFLOWINT pThis = hFlow;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


/**
 * Releases the control flow graph handle.
 *
 * @returns Current reference count, on 0 the control flow graph will be destroyed.
 * @param   hFlow                The control flow graph handle to release.
 */
VMMR3DECL(uint32_t) DBGFR3FlowRelease(DBGFFLOW hFlow)
{
    PDBGFFLOWINT pThis = hFlow;
    if (!pThis)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        dbgfR3FlowDestroy(pThis);
    return cRefs;
}


/**
 * Queries the basic block denoting the entry point into the control flow graph.
 *
 * @returns VBox status code.
 * @param   hFlow                The control flow graph handle.
 * @param   phFlowBb             Where to store the basic block handle on success.
 */
VMMR3DECL(int) DBGFR3FlowQueryStartBb(DBGFFLOW hFlow, PDBGFFLOWBB phFlowBb)
{
    PDBGFFLOWINT pThis = hFlow;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    PDBGFFLOWBBINT pFlowBb = NULL;
    RTListForEach(&pThis->LstFlowBb, pFlowBb, DBGFFLOWBBINT, NdFlowBb)
    {
        if (pFlowBb->fFlags & DBGF_FLOW_BB_F_ENTRY)
        {
            *phFlowBb = pFlowBb;
            return VINF_SUCCESS;
        }
    }

    AssertFailed(); /* Should never get here. */
    return VERR_INTERNAL_ERROR;
}


/**
 * Queries a basic block in the given control flow graph which covers the given
 * address.
 *
 * @returns VBox status code.
 * @retval  VERR_NOT_FOUND if there is no basic block intersecting with the address.
 * @param   hFlow                The control flow graph handle.
 * @param   pAddr               The address to look for.
 * @param   phFlowBb             Where to store the basic block handle on success.
 */
VMMR3DECL(int) DBGFR3FlowQueryBbByAddress(DBGFFLOW hFlow, PDBGFADDRESS pAddr, PDBGFFLOWBB phFlowBb)
{
    PDBGFFLOWINT pThis = hFlow;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(phFlowBb, VERR_INVALID_POINTER);

    PDBGFFLOWBBINT pFlowBb = NULL;
    RTListForEach(&pThis->LstFlowBb, pFlowBb, DBGFFLOWBBINT, NdFlowBb)
    {
        if (dbgfR3FlowBbAddrIntersect(pFlowBb, pAddr))
        {
            DBGFR3FlowBbRetain(pFlowBb);
            *phFlowBb = pFlowBb;
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}


/**
 * Returns the number of basic blcoks inside the control flow graph.
 *
 * @returns Number of basic blocks.
 * @param   hFlow                The control flow graph handle.
 */
VMMR3DECL(uint32_t) DBGFR3FlowGetBbCount(DBGFFLOW hFlow)
{
    PDBGFFLOWINT pThis = hFlow;
    AssertPtrReturn(pThis, 0);

    return pThis->cBbs;
}


/**
 * Retains the basic block handle.
 *
 * @returns Current reference count.
 * @param   hFlowBb              The basic block handle to retain.
 */
VMMR3DECL(uint32_t) DBGFR3FlowBbRetain(DBGFFLOWBB hFlowBb)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pFlowBb->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p %d\n", cRefs, pFlowBb, pFlowBb->enmEndType));
    return cRefs;
}


/**
 * Releases the basic block handle.
 *
 * @returns Current reference count, on 0 the basic block will be destroyed.
 * @param   hFlowBb              The basic block handle to release.
 */
VMMR3DECL(uint32_t) DBGFR3FlowBbRelease(DBGFFLOWBB hFlowBb)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    if (!pFlowBb)
        return 0;

    return dbgfR3FlowBbReleaseInt(pFlowBb, true /* fMayDestroyFlow */);
}


/**
 * Returns the start address of the basic block.
 *
 * @returns Pointer to DBGF adress containing the start address of the basic block.
 * @param   hFlowBb              The basic block handle.
 * @param   pAddrStart          Where to store the start address of the basic block.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3FlowBbGetStartAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrStart)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, NULL);
    AssertPtrReturn(pAddrStart, NULL);

    *pAddrStart = pFlowBb->AddrStart;
    return pAddrStart;
}


/**
 * Returns the end address of the basic block (inclusive).
 *
 * @returns Pointer to DBGF adress containing the end address of the basic block.
 * @param   hFlowBb              The basic block handle.
 * @param   pAddrEnd            Where to store the end address of the basic block.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3FlowBbGetEndAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrEnd)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, NULL);
    AssertPtrReturn(pAddrEnd, NULL);

    *pAddrEnd = pFlowBb->AddrEnd;
    return pAddrEnd;
}


/**
 * Returns the address the last instruction in the basic block branches to.
 *
 * @returns Pointer to DBGF adress containing the branch address of the basic block.
 * @param   hFlowBb              The basic block handle.
 * @param   pAddrTarget         Where to store the branch address of the basic block.
 *
 * @note This is only valid for unconditional or conditional branches and will assert
 *       for every other basic block type.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3FlowBbGetBranchAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrTarget)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, NULL);
    AssertPtrReturn(pAddrTarget, NULL);
    AssertReturn(   pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_UNCOND_JMP
                 || pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_COND,
                 NULL);

    *pAddrTarget = pFlowBb->AddrTarget;
    return pAddrTarget;
}


/**
 * Returns the address of the next block following this one in the instruction stream.
 * (usually end address + 1).
 *
 * @returns Pointer to DBGF adress containing the following address of the basic block.
 * @param   hFlowBb              The basic block handle.
 * @param   pAddrFollow         Where to store the following address of the basic block.
 *
 * @note This is only valid for conditional branches and if the last instruction in the
 *       given basic block doesn't change the control flow but the blocks were split
 *       because the successor is referenced by multiple other blocks as an entry point.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3FlowBbGetFollowingAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrFollow)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, NULL);
    AssertPtrReturn(pAddrFollow, NULL);
    AssertReturn(   pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_UNCOND
                 || pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_COND,
                 NULL);

    *pAddrFollow = pFlowBb->AddrEnd;
    DBGFR3AddrAdd(pAddrFollow, 1);
    return pAddrFollow;
}


/**
 * Returns the type of the last instruction in the basic block.
 *
 * @returns Last instruction type.
 * @param   hFlowBb              The basic block handle.
 */
VMMR3DECL(DBGFFLOWBBENDTYPE) DBGFR3FlowBbGetType(DBGFFLOWBB hFlowBb)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, DBGFFLOWBBENDTYPE_INVALID);

    return pFlowBb->enmEndType;
}


/**
 * Get the number of instructions contained in the basic block.
 *
 * @returns Number of instructions in the basic block.
 * @param   hFlowBb              The basic block handle.
 */
VMMR3DECL(uint32_t) DBGFR3FlowBbGetInstrCount(DBGFFLOWBB hFlowBb)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, 0);

    return pFlowBb->cInstr;
}


/**
 * Get flags for the given basic block.
 *
 * @returns Combination of DBGF_FLOW_BB_F_*
 * @param   hFlowBb              The basic block handle.
 */
VMMR3DECL(uint32_t) DBGFR3FlowBbGetFlags(DBGFFLOWBB hFlowBb)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, 0);

    return pFlowBb->fFlags;
}


/**
 * Returns the error status and message if the given basic block has an error.
 *
 * @returns VBox status code of the error for the basic block.
 * @param   hFlowBb              The basic block handle.
 * @param   ppszErr             Where to store the pointer to the error message - optional.
 */
VMMR3DECL(int) DBGFR3FlowBbQueryError(DBGFFLOWBB hFlowBb, const char **ppszErr)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, VERR_INVALID_HANDLE);

    if (ppszErr)
        *ppszErr = pFlowBb->pszErr;

    return pFlowBb->rcError;
}


/**
 * Store the disassembled instruction as a string in the given output buffer.
 *
 * @returns VBox status code.
 * @param   hFlowBb              The basic block handle.
 * @param   idxInstr            The instruction to query.
 * @param   pAddrInstr          Where to store the guest instruction address on success, optional.
 * @param   pcbInstr            Where to store the instruction size on success, optional.
 * @param   ppszInstr           Where to store the pointer to the disassembled instruction string, optional.
 */
VMMR3DECL(int) DBGFR3FlowBbQueryInstr(DBGFFLOWBB hFlowBb, uint32_t idxInstr, PDBGFADDRESS pAddrInstr,
                                     uint32_t *pcbInstr, const char **ppszInstr)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, VERR_INVALID_POINTER);
    AssertReturn(idxInstr < pFlowBb->cInstr, VERR_INVALID_PARAMETER);

    if (pAddrInstr)
        *pAddrInstr = pFlowBb->aInstr[idxInstr].AddrInstr;
    if (pcbInstr)
        *pcbInstr = pFlowBb->aInstr[idxInstr].cbInstr;
    if (ppszInstr)
        *ppszInstr = pFlowBb->aInstr[idxInstr].pszInstr;

    return VINF_SUCCESS;
}


/**
 * Queries the successors of the basic block.
 *
 * @returns VBox status code.
 * @param   hFlowBb              The basic block handle.
 * @param   phFlowBbFollow       Where to store the handle to the basic block following
 *                              this one (optional).
 * @param   phFlowBbTarget       Where to store the handle to the basic block being the
 *                              branch target for this one (optional).
 */
VMMR3DECL(int) DBGFR3FlowBbQuerySuccessors(DBGFFLOWBB hFlowBb, PDBGFFLOWBB phFlowBbFollow, PDBGFFLOWBB phFlowBbTarget)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, VERR_INVALID_POINTER);

    if (   phFlowBbFollow
        && (   pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_UNCOND
            || pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_COND))
    {
        DBGFADDRESS AddrStart = pFlowBb->AddrEnd;
        DBGFR3AddrAdd(&AddrStart, 1);
        int rc = DBGFR3FlowQueryBbByAddress(pFlowBb->pFlow, &AddrStart, phFlowBbFollow);
        AssertRC(rc);
    }

    if (   phFlowBbTarget
        && (   pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_UNCOND_JMP
            || pFlowBb->enmEndType == DBGFFLOWBBENDTYPE_COND))
    {
        int rc = DBGFR3FlowQueryBbByAddress(pFlowBb->pFlow, &pFlowBb->AddrTarget, phFlowBbTarget);
        AssertRC(rc);
    }

    return VINF_SUCCESS;
}


/**
 * Returns the number of basic blocks referencing this basic block as a target.
 *
 * @returns Number of other basic blocks referencing this one.
 * @param   hFlowBb              The basic block handle.
 *
 * @note If the given basic block references itself (loop, etc.) this will be counted as well.
 */
VMMR3DECL(uint32_t) DBGFR3FlowBbGetRefBbCount(DBGFFLOWBB hFlowBb)
{
    PDBGFFLOWBBINT pFlowBb = hFlowBb;
    AssertPtrReturn(pFlowBb, 0);

    uint32_t cRefsBb = 0;
    PDBGFFLOWBBINT pFlowBbCur = NULL;
    RTListForEach(&pFlowBb->pFlow->LstFlowBb, pFlowBbCur, DBGFFLOWBBINT, NdFlowBb)
    {
        if (pFlowBbCur->fFlags & DBGF_FLOW_BB_F_INCOMPLETE_ERR)
            continue;

        if (   pFlowBbCur->enmEndType == DBGFFLOWBBENDTYPE_UNCOND
            || pFlowBbCur->enmEndType == DBGFFLOWBBENDTYPE_COND)
        {
            DBGFADDRESS AddrStart = pFlowBb->AddrEnd;
            DBGFR3AddrAdd(&AddrStart, 1);
            if (dbgfR3FlowBbAddrEqual(&pFlowBbCur->AddrStart, &AddrStart))
                cRefsBb++;
        }

        if (   (   pFlowBbCur->enmEndType == DBGFFLOWBBENDTYPE_UNCOND_JMP
                || pFlowBbCur->enmEndType == DBGFFLOWBBENDTYPE_COND)
            && dbgfR3FlowBbAddrEqual(&pFlowBbCur->AddrStart, &pFlowBb->AddrTarget))
            cRefsBb++;
    }
    return cRefsBb;
}


/**
 * Returns the basic block handles referencing the given basic block.
 *
 * @returns VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the array can't hold all the basic blocks.
 * @param   hFlowBb              The basic block handle.
 * @param   paFlowBbRef          Pointer to the array containing the referencing basic block handles on success.
 * @param   cRef                Number of entries in the given array.
 */
VMMR3DECL(int) DBGFR3FlowBbGetRefBb(DBGFFLOWBB hFlowBb, PDBGFFLOWBB paFlowBbRef, uint32_t cRef)
{
    RT_NOREF3(hFlowBb, paFlowBbRef, cRef);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNRTSORTCMP}
 */
static DECLCALLBACK(int) dbgfR3FlowItSortCmp(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    PDBGFFLOWITORDER penmOrder = (PDBGFFLOWITORDER)pvUser;
    PDBGFFLOWBBINT pFlowBb1 = *(PDBGFFLOWBBINT *)pvElement1;
    PDBGFFLOWBBINT pFlowBb2 = *(PDBGFFLOWBBINT *)pvElement2;

    if (dbgfR3FlowBbAddrEqual(&pFlowBb1->AddrStart, &pFlowBb2->AddrStart))
        return 0;

    if (*penmOrder == DBGFFLOWITORDER_BY_ADDR_LOWEST_FIRST)
    {
        if (dbgfR3FlowBbAddrLower(&pFlowBb1->AddrStart, &pFlowBb2->AddrStart))
            return -1;
        else
            return 1;
    }
    else
    {
        if (dbgfR3FlowBbAddrLower(&pFlowBb1->AddrStart, &pFlowBb2->AddrStart))
            return 1;
        else
            return -1;
    }
}


/**
 * Creates a new iterator for the given control flow graph.
 *
 * @returns VBox status code.
 * @param   hFlow                The control flow graph handle.
 * @param   enmOrder            The order in which the basic blocks are enumerated.
 * @param   phFlowIt             Where to store the handle to the iterator on success.
 */
VMMR3DECL(int) DBGFR3FlowItCreate(DBGFFLOW hFlow, DBGFFLOWITORDER enmOrder, PDBGFFLOWIT phFlowIt)
{
    int rc = VINF_SUCCESS;
    PDBGFFLOWINT pFlow = hFlow;
    AssertPtrReturn(pFlow, VERR_INVALID_POINTER);
    AssertPtrReturn(phFlowIt, VERR_INVALID_POINTER);
    AssertReturn(enmOrder > DBGFFLOWITORDER_INVALID && enmOrder < DBGFFLOWITORDER_BREADTH_FIRST,
                 VERR_INVALID_PARAMETER);
    AssertReturn(enmOrder < DBGFFLOWITORDER_DEPTH_FRIST, VERR_NOT_IMPLEMENTED); /** @todo */

    PDBGFFLOWITINT pIt = (PDBGFFLOWITINT)RTMemAllocZ(RT_OFFSETOF(DBGFFLOWITINT, apBb[pFlow->cBbs]));
    if (RT_LIKELY(pIt))
    {
        DBGFR3FlowRetain(hFlow);
        pIt->pFlow      = pFlow;
        pIt->idxBbNext = 0;
        /* Fill the list and then sort. */
        PDBGFFLOWBBINT pFlowBb;
        uint32_t idxBb = 0;
        RTListForEach(&pFlow->LstFlowBb, pFlowBb, DBGFFLOWBBINT, NdFlowBb)
        {
            DBGFR3FlowBbRetain(pFlowBb);
            pIt->apBb[idxBb++] = pFlowBb;
        }

        /* Sort the blocks by address. */
        RTSortShell(&pIt->apBb[0], pFlow->cBbs, sizeof(PDBGFFLOWBBINT), dbgfR3FlowItSortCmp, &enmOrder);

        *phFlowIt = pIt;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Destroys a given control flow graph iterator.
 *
 * @returns nothing.
 * @param   hFlowIt              The control flow graph iterator handle.
 */
VMMR3DECL(void) DBGFR3FlowItDestroy(DBGFFLOWIT hFlowIt)
{
    PDBGFFLOWITINT pIt = hFlowIt;
    AssertPtrReturnVoid(pIt);

    for (unsigned i = 0; i < pIt->pFlow->cBbs; i++)
        DBGFR3FlowBbRelease(pIt->apBb[i]);

    DBGFR3FlowRelease(pIt->pFlow);
    RTMemFree(pIt);
}


/**
 * Returns the next basic block in the iterator or NULL if there is no
 * basic block left.
 *
 * @returns Handle to the next basic block in the iterator or NULL if the end
 *          was reached.
 * @param   hFlowIt              The iterator handle.
 *
 * @note If a valid handle is returned it must be release with DBGFR3FlowBbRelease()
 *       when not required anymore.
 */
VMMR3DECL(DBGFFLOWBB) DBGFR3FlowItNext(DBGFFLOWIT hFlowIt)
{
    PDBGFFLOWITINT pIt = hFlowIt;
    AssertPtrReturn(pIt, NULL);

    PDBGFFLOWBBINT pFlowBb = NULL;
    if (pIt->idxBbNext < pIt->pFlow->cBbs)
    {
        pFlowBb = pIt->apBb[pIt->idxBbNext++];
        DBGFR3FlowBbRetain(pFlowBb);
    }

    return pFlowBb;
}


/**
 * Resets the given iterator to the beginning.
 *
 * @returns VBox status code.
 * @param   hFlowIt              The iterator handle.
 */
VMMR3DECL(int) DBGFR3FlowItReset(DBGFFLOWIT hFlowIt)
{
    PDBGFFLOWITINT pIt = hFlowIt;
    AssertPtrReturn(pIt, VERR_INVALID_HANDLE);

    pIt->idxBbNext = 0;
    return VINF_SUCCESS;
}
