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


/** @page pg_dbgf_cfg    DBGFR3Cfg - Control Flow Graph Interface
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
typedef struct DBGFCFGINT
{
    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** Internal reference counter for basic blocks. */
    uint32_t volatile       cRefsBb;
    /** List of all basic blocks. */
    RTLISTANCHOR            LstCfgBb;
    /** Number of basic blocks in this control flow graph. */
    uint32_t                cBbs;
    /** The lowest addres of a basic block. */
    DBGFADDRESS             AddrLowest;
    /** The highest address of a basic block. */
    DBGFADDRESS             AddrHighest;
    /** String cache for disassembled instructions. */
    RTSTRCACHE              hStrCacheInstr;
} DBGFCFGINT;
/** Pointer to an internal control flow graph state. */
typedef DBGFCFGINT *PDBGFCFGINT;

/**
 * Instruction record
 */
typedef struct DBGFCFGBBINSTR
{
    /** Instruction address. */
    DBGFADDRESS             AddrInstr;
    /** Size of instruction. */
    uint32_t                cbInstr;
    /** Disassembled instruction string. */
    const char              *pszInstr;
} DBGFCFGBBINSTR;
/** Pointer to an instruction record. */
typedef DBGFCFGBBINSTR *PDBGFCFGBBINSTR;

/**
 * Internal control flow graph basic block state.
 */
typedef struct DBGFCFGBBINT
{
    /** Node for the list of all basic blocks. */
    RTLISTNODE              NdCfgBb;
    /** The control flow graph the basic block belongs to. */
    PDBGFCFGINT             pCfg;
    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** Basic block end type. */
    DBGFCFGBBENDTYPE        enmEndType;
    /** Start address of this basic block. */
    DBGFADDRESS             AddrStart;
    /** End address of this basic block. */
    DBGFADDRESS             AddrEnd;
    /** Address of the block succeeding.
     *  This is valid for conditional jumps
     * (the other target is referenced by AddrEnd+1) and
     * unconditional jumps (not ret, iret, etc.) except
     * if we can't infer the jump target (jmp *eax for example). */
    DBGFADDRESS             AddrTarget;
    /** Last status error code if DBGF_CFG_BB_F_INCOMPLETE_ERR is set. */
    int                     rcError;
    /** Error message if DBGF_CFG_BB_F_INCOMPLETE_ERR is set. */
    char                   *pszErr;
    /** Flags for this basic block. */
    uint32_t                fFlags;
    /** Number of instructions in this basic block. */
    uint32_t                cInstr;
    /** Maximum number of instruction records for this basic block. */
    uint32_t                cInstrMax;
    /** Instruction records, variable in size. */
    DBGFCFGBBINSTR          aInstr[1];
} DBGFCFGBBINT;
/** Pointer to an internal control flow graph basic block state. */
typedef DBGFCFGBBINT *PDBGFCFGBBINT;

/**
 * Dumper state for a basic block.
 */
typedef struct DBGFCFGDUMPBB
{
    /** The basic block referenced. */
    PDBGFCFGBBINT           pCfgBb;
    /** Width of the basic block in chars. */
    uint32_t                cchWidth;
    /** Height of the basic block in chars. */
    uint32_t                cchHeight;
    /** X coordinate of the start. */
    uint32_t                uStartX;
    /** Y coordinate of the start. */
    uint32_t                uStartY;
} DBGFCFGDUMPBB;
/** Pointer to a basic block dumper state. */
typedef DBGFCFGDUMPBB *PDBGFCFGDUMPBB;

/**
 * Dumper ASCII screen.
 */
typedef struct DBGFCFGDUMPSCREEN
{
    /** Width of the screen. */
    uint32_t                cchWidth;
    /** Height of the screen. */
    uint32_t                cchHeight;
    /** Extra amount of characters at the end of each line (usually temrinator). */
    uint32_t                cchStride;
    /** Pointer to the char buffer. */
    char                   *pszScreen;
} DBGFCFGDUMPSCREEN;
/** Pointer to a dumper ASCII screen. */
typedef DBGFCFGDUMPSCREEN *PDBGFCFGDUMPSCREEN;

/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static uint32_t dbgfR3CfgBbReleaseInt(PDBGFCFGBBINT pCfgBb, bool fMayDestroyCfg);

/**
 * Creates a new basic block.
 *
 * @returns Pointer to the basic block on success or NULL if out of memory.
 * @param   pThis               The control flow graph.
 * @param   pAddrStart          The start of the basic block.
 * @param   cInstrMax           Maximum number of instructions this block can hold initially.
 */
static PDBGFCFGBBINT dbgfR3CfgBbCreate(PDBGFCFGINT pThis, PDBGFADDRESS pAddrStart, uint32_t cInstrMax)
{
    PDBGFCFGBBINT pCfgBb = (PDBGFCFGBBINT)RTMemAllocZ(RT_OFFSETOF(DBGFCFGBBINT, aInstr[cInstrMax]));
    if (RT_LIKELY(pCfgBb))
    {
        RTListInit(&pCfgBb->NdCfgBb);
        pCfgBb->cRefs      = 1;
        pCfgBb->enmEndType = DBGFCFGBBENDTYPE_INVALID;
        pCfgBb->pCfg       = pThis;
        pCfgBb->fFlags     = DBGF_CFG_BB_F_EMPTY;
        pCfgBb->AddrStart  = *pAddrStart;
        pCfgBb->AddrEnd    = *pAddrStart;
        pCfgBb->rcError    = VINF_SUCCESS;
        pCfgBb->pszErr     = NULL;
        pCfgBb->cInstr     = 0;
        pCfgBb->cInstrMax  = cInstrMax;
        ASMAtomicIncU32(&pThis->cRefsBb);
    }

    return pCfgBb;
}


/**
 * Destroys a control flow graph.
 *
 * @returns nothing.
 * @param   pThis               The control flow graph to destroy.
 */
static void dbgfR3CfgDestroy(PDBGFCFGINT pThis)
{
    /* Defer destruction if there are still basic blocks referencing us. */
    PDBGFCFGBBINT pCfgBb = NULL;
    PDBGFCFGBBINT pCfgBbNext = NULL;
    RTListForEachSafe(&pThis->LstCfgBb, pCfgBb, pCfgBbNext, DBGFCFGBBINT, NdCfgBb)
    {
        dbgfR3CfgBbReleaseInt(pCfgBb, false /*fMayDestroyCfg*/);
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
 * @param   pCfgBb              The basic block to destroy.
 * @param   fMayDestroyCfg      Flag whether the control flow graph container
 *                              should be destroyed when there is nothing referencing it.
 */
static void dbgfR3CfgBbDestroy(PDBGFCFGBBINT pCfgBb, bool fMayDestroyCfg)
{
    PDBGFCFGINT pThis = pCfgBb->pCfg;

    RTListNodeRemove(&pCfgBb->NdCfgBb);
    pThis->cBbs--;
    for (uint32_t idxInstr = 0; idxInstr < pCfgBb->cInstr; idxInstr++)
        RTStrCacheRelease(pThis->hStrCacheInstr, pCfgBb->aInstr[idxInstr].pszInstr);
    uint32_t cRefsBb = ASMAtomicDecU32(&pThis->cRefsBb);
    RTMemFree(pCfgBb);

    if (!cRefsBb && !pThis->cRefs && fMayDestroyCfg)
        dbgfR3CfgDestroy(pThis);
}


/**
 * Internal basic block release worker.
 *
 * @returns New reference count of the released basic block, on 0
 *          it is destroyed.
 * @param   pCfgBb              The basic block to release.
 * @param   fMayDestroyCfg      Flag whether the control flow graph container
 *                              should be destroyed when there is nothing referencing it.
 */
static uint32_t dbgfR3CfgBbReleaseInt(PDBGFCFGBBINT pCfgBb, bool fMayDestroyCfg)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCfgBb->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p %d\n", cRefs, pCfgBb, pCfgBb->enmEndType));
    if (cRefs == 0)
        dbgfR3CfgBbDestroy(pCfgBb, fMayDestroyCfg);
    return cRefs;
}


/**
 * Links the given basic block into the control flow graph.
 *
 * @returns nothing.
 * @param   pThis               The control flow graph to link into.
 * @param   pCfgBb              The basic block to link.
 */
DECLINLINE(void) dbgfR3CfgLink(PDBGFCFGINT pThis, PDBGFCFGBBINT pCfgBb)
{
    RTListAppend(&pThis->LstCfgBb, &pCfgBb->NdCfgBb);
    pThis->cBbs++;
}


/**
 * Returns the first unpopulated basic block of the given control flow graph.
 *
 * @returns The first unpopulated control flow graph or NULL if not found.
 * @param   pThis               The control flow graph.
 */
DECLINLINE(PDBGFCFGBBINT) dbgfR3CfgGetUnpopulatedBb(PDBGFCFGINT pThis)
{
    PDBGFCFGBBINT pCfgBb = NULL;
    RTListForEach(&pThis->LstCfgBb, pCfgBb, DBGFCFGBBINT, NdCfgBb)
    {
        if (pCfgBb->fFlags & DBGF_CFG_BB_F_EMPTY)
            return pCfgBb;
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
static int dbgfR3CfgQueryJmpTarget(PUVM pUVM, VMCPUID idCpu, PDISOPPARAM pDisParam, PDBGFADDRESS pAddrInstr,
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
static bool dbgfR3CfgBbAddrEqual(PDBGFADDRESS pAddr1, PDBGFADDRESS pAddr2)
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
static bool dbgfR3CfgBbAddrLower(PDBGFADDRESS pAddr1, PDBGFADDRESS pAddr2)
{
    return    pAddr1->Sel == pAddr2->Sel
           && pAddr1->off < pAddr2->off;
}


/**
 * Checks whether the given basic block and address intersect.
 *
 * @returns true if they intersect, false otherwise.
 * @param   pCfgBb              The basic block to check.
 * @param   pAddr               The address to check for.
 */
static bool dbgfR3CfgBbAddrIntersect(PDBGFCFGBBINT pCfgBb, PDBGFADDRESS pAddr)
{
    return    (pCfgBb->AddrStart.Sel == pAddr->Sel)
           && (pCfgBb->AddrStart.off <= pAddr->off)
           && (pCfgBb->AddrEnd.off >= pAddr->off);
}


/**
 * Checks whether the given control flow graph contains a basic block
 * with the given start address.
 *
 * @returns true if there is a basic block with the start address, false otherwise.
 * @param   pThis               The control flow graph.
 * @param   pAddr               The address to check for.
 */
static bool dbgfR3CfgHasBbWithStartAddr(PDBGFCFGINT pThis, PDBGFADDRESS pAddr)
{
    PDBGFCFGBBINT pCfgBb = NULL;
    RTListForEach(&pThis->LstCfgBb, pCfgBb, DBGFCFGBBINT, NdCfgBb)
    {
        if (dbgfR3CfgBbAddrEqual(&pCfgBb->AddrStart, pAddr))
            return true;
    }
    return false;
}

/**
 * Splits a given basic block into two at the given address.
 *
 * @returns VBox status code.
 * @param   pThis               The control flow graph.
 * @param   pCfgBb              The basic block to split.
 * @param   pAddr               The address to split at.
 */
static int dbgfR3CfgBbSplit(PDBGFCFGINT pThis, PDBGFCFGBBINT pCfgBb, PDBGFADDRESS pAddr)
{
    int rc = VINF_SUCCESS;
    uint32_t idxInstrSplit;

    /* If the block is empty it will get populated later so there is nothing to split,
     * same if the the start address equals. */
    if (   pCfgBb->fFlags & DBGF_CFG_BB_F_EMPTY
        || dbgfR3CfgBbAddrEqual(&pCfgBb->AddrStart, pAddr))
        return VINF_SUCCESS;

    /* Find the instruction to split at. */
    for (idxInstrSplit = 1; idxInstrSplit < pCfgBb->cInstr; idxInstrSplit++)
        if (dbgfR3CfgBbAddrEqual(&pCfgBb->aInstr[idxInstrSplit].AddrInstr, pAddr))
            break;

    Assert(idxInstrSplit > 0);

    /*
     * Given address might not be on instruction boundary, this is not supported
     * so far and results in an error.
     */
    if (idxInstrSplit < pCfgBb->cInstr)
    {
        /* Create new basic block. */
        uint32_t cInstrNew = pCfgBb->cInstr - idxInstrSplit;
        PDBGFCFGBBINT pCfgBbNew = dbgfR3CfgBbCreate(pThis, &pCfgBb->aInstr[idxInstrSplit].AddrInstr,
                                                    cInstrNew);
        if (pCfgBbNew)
        {
            /* Move instructions over. */
            pCfgBbNew->cInstr = cInstrNew;
            pCfgBbNew->AddrEnd = pCfgBb->AddrEnd;
            pCfgBbNew->enmEndType = pCfgBb->enmEndType;

            memcpy(&pCfgBbNew->aInstr[0], &pCfgBb->aInstr[idxInstrSplit], cInstrNew * sizeof(DBGFCFGBBINSTR));
            pCfgBb->cInstr     = idxInstrSplit;
            pCfgBb->enmEndType = DBGFCFGBBENDTYPE_UNCOND;
            pCfgBb->AddrEnd    = pCfgBb->aInstr[idxInstrSplit].AddrInstr;
            pCfgBb->AddrTarget = pCfgBbNew->AddrStart;
            DBGFR3AddrAdd(&pCfgBb->AddrEnd, pCfgBb->aInstr[idxInstrSplit].cbInstr - 1);
            RT_BZERO(&pCfgBb->aInstr[idxInstrSplit], cInstrNew * sizeof(DBGFCFGBBINSTR));

            dbgfR3CfgLink(pThis, pCfgBbNew);
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
static int dbgfR3CfgBbSuccessorAdd(PDBGFCFGINT pThis, PDBGFADDRESS pAddrSucc)
{
    PDBGFCFGBBINT pCfgBb = NULL;
    RTListForEach(&pThis->LstCfgBb, pCfgBb, DBGFCFGBBINT, NdCfgBb)
    {
        /*
         * The basic block must be split if it intersects with the given address
         * and the start address does not equal the given one.
         */
        if (dbgfR3CfgBbAddrIntersect(pCfgBb, pAddrSucc))
            return dbgfR3CfgBbSplit(pThis, pCfgBb, pAddrSucc);
    }

    int rc = VINF_SUCCESS;
    pCfgBb = dbgfR3CfgBbCreate(pThis, pAddrSucc, 10);
    if (pCfgBb)
        dbgfR3CfgLink(pThis, pCfgBb);
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Sets the given error status for the basic block.
 *
 * @returns nothing.
 * @param   pCfgBb              The basic block causing the error.
 * @param   rcError             The error to set.
 * @param   pszFmt              Format string of the error description.
 * @param   ...                 Arguments for the format string.
 */
static void dbgfR3CfgBbSetError(PDBGFCFGBBINT pCfgBb, int rcError, const char *pszFmt, ...)
{
    va_list va;
    va_start(va, pszFmt);

    Assert(!(pCfgBb->fFlags & DBGF_CFG_BB_F_INCOMPLETE_ERR));
    pCfgBb->fFlags |= DBGF_CFG_BB_F_INCOMPLETE_ERR;
    pCfgBb->fFlags &= ~DBGF_CFG_BB_F_EMPTY;
    pCfgBb->rcError = rcError;
    pCfgBb->pszErr = RTStrAPrintf2V(pszFmt, va);
    va_end(va);
}


/**
 * Processes and fills one basic block.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   idCpu               CPU id for disassembling.
 * @param   pThis               The control flow graph to populate.
 * @param   pCfgBb              The basic block to fill.
 * @param   cbDisasmMax         The maximum amount to disassemble.
 * @param   fFlags              Combination of DBGF_DISAS_FLAGS_*.
 */
static int dbgfR3CfgBbProcess(PUVM pUVM, VMCPUID idCpu, PDBGFCFGINT pThis, PDBGFCFGBBINT pCfgBb,
                              uint32_t cbDisasmMax, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    uint32_t cbDisasmLeft = cbDisasmMax ? cbDisasmMax : UINT32_MAX;
    DBGFADDRESS AddrDisasm = pCfgBb->AddrEnd;

    Assert(pCfgBb->fFlags & DBGF_CFG_BB_F_EMPTY);

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
        if (   !(pCfgBb->fFlags & DBGF_CFG_BB_F_EMPTY)
            && dbgfR3CfgHasBbWithStartAddr(pThis, &AddrDisasm))
        {
            pCfgBb->AddrTarget = AddrDisasm;
            pCfgBb->enmEndType = DBGFCFGBBENDTYPE_UNCOND;
            break;
        }

        rc = dbgfR3DisasInstrStateEx(pUVM, idCpu, &AddrDisasm, fFlags,
                                     &szOutput[0], sizeof(szOutput), &DisState);
        if (RT_SUCCESS(rc))
        {
            cbDisasmLeft -= DisState.cbInstr;

            if (pCfgBb->cInstr == pCfgBb->cInstrMax)
            {
                /* Reallocate. */
                RTListNodeRemove(&pCfgBb->NdCfgBb);
                PDBGFCFGBBINT pCfgBbNew = (PDBGFCFGBBINT)RTMemRealloc(pCfgBb, RT_OFFSETOF(DBGFCFGBBINT, aInstr[pCfgBb->cInstrMax + 10]));
                if (pCfgBbNew)
                {
                    pCfgBbNew->cInstrMax += 10;
                    pCfgBb = pCfgBbNew;
                }
                else
                    rc = VERR_NO_MEMORY;
                RTListAppend(&pThis->LstCfgBb, &pCfgBb->NdCfgBb);
            }

            if (RT_SUCCESS(rc))
            {
                PDBGFCFGBBINSTR pInstr = &pCfgBb->aInstr[pCfgBb->cInstr];

                pCfgBb->fFlags &= ~DBGF_CFG_BB_F_EMPTY;

                pInstr->AddrInstr = AddrDisasm;
                pInstr->cbInstr   = DisState.cbInstr;
                pInstr->pszInstr  = RTStrCacheEnter(pThis->hStrCacheInstr, &szOutput[0]);
                pCfgBb->cInstr++;

                pCfgBb->AddrEnd = AddrDisasm;
                DBGFR3AddrSub(&pCfgBb->AddrEnd, 1);
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
                        pCfgBb->enmEndType = DBGFCFGBBENDTYPE_EXIT;
                    else if (uOpc == OP_JMP)
                    {
                        Assert(DisState.pCurInstr->fOpType & DISOPTYPE_UNCOND_CONTROLFLOW);
                        pCfgBb->enmEndType = DBGFCFGBBENDTYPE_UNCOND_JMP;

                        /* Create one new basic block with the jump target address. */
                        rc = dbgfR3CfgQueryJmpTarget(pUVM, idCpu, &DisState.Param1, &pInstr->AddrInstr, pInstr->cbInstr,
                                                     RT_BOOL(DisState.pCurInstr->fOpType & DISOPTYPE_RELATIVE_CONTROLFLOW),
                                                     &pCfgBb->AddrTarget);
                        if (RT_SUCCESS(rc))
                            rc = dbgfR3CfgBbSuccessorAdd(pThis, &pCfgBb->AddrTarget);
                    }
                    else if (uOpc != OP_CALL)
                    {
                        Assert(DisState.pCurInstr->fOpType & DISOPTYPE_COND_CONTROLFLOW);
                        pCfgBb->enmEndType = DBGFCFGBBENDTYPE_COND;

                        /*
                         * Create two new basic blocks, one with the jump target address
                         * and one starting after the current instruction.
                         */
                        rc = dbgfR3CfgBbSuccessorAdd(pThis, &AddrDisasm);
                        if (RT_SUCCESS(rc))
                        {
                            rc = dbgfR3CfgQueryJmpTarget(pUVM, idCpu, &DisState.Param1, &pInstr->AddrInstr, pInstr->cbInstr, 
                                                         RT_BOOL(DisState.pCurInstr->fOpType & DISOPTYPE_RELATIVE_CONTROLFLOW),
                                                         &pCfgBb->AddrTarget);
                            if (RT_SUCCESS(rc))
                                rc = dbgfR3CfgBbSuccessorAdd(pThis, &pCfgBb->AddrTarget);
                        }
                    }

                    if (RT_FAILURE(rc))
                        dbgfR3CfgBbSetError(pCfgBb, rc, "Adding successor blocks failed with %Rrc", rc);

                    /* Quit disassembling. */
                    if (   uOpc != OP_CALL
                        || RT_FAILURE(rc))
                        break;
                }
            }
            else
                dbgfR3CfgBbSetError(pCfgBb, rc, "Increasing basic block failed with %Rrc", rc);
        }
        else
            dbgfR3CfgBbSetError(pCfgBb, rc, "Disassembling the instruction failed with %Rrc", rc);
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
static int dbgfR3CfgPopulate(PUVM pUVM, VMCPUID idCpu, PDBGFCFGINT pThis, PDBGFADDRESS pAddrStart,
                             uint32_t cbDisasmMax, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    PDBGFCFGBBINT pCfgBb = dbgfR3CfgGetUnpopulatedBb(pThis);
    DBGFADDRESS AddrEnd = *pAddrStart;
    DBGFR3AddrAdd(&AddrEnd, cbDisasmMax);

    while (VALID_PTR(pCfgBb))
    {
        rc = dbgfR3CfgBbProcess(pUVM, idCpu, pThis, pCfgBb, cbDisasmMax, fFlags);
        if (RT_FAILURE(rc))
            break;

        pCfgBb = dbgfR3CfgGetUnpopulatedBb(pThis);
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
 * @param   phCfg               Where to store the handle to the control flow graph on success.
 */
VMMR3DECL(int) DBGFR3CfgCreate(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddressStart, uint32_t cbDisasmMax,
                               uint32_t fFlags, PDBGFCFG phCfg)
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
    PDBGFCFGINT pThis = (PDBGFCFGINT)RTMemAllocZ(sizeof(DBGFCFGINT));
    if (RT_LIKELY(pThis))
    {
        rc = RTStrCacheCreate(&pThis->hStrCacheInstr, "DBGFCFG");
        if (RT_SUCCESS(rc))
        {
            pThis->cRefs   = 1;
            pThis->cRefsBb = 0;
            pThis->cBbs    = 0;
            RTListInit(&pThis->LstCfgBb);
            /* Create the entry basic block and start the work. */

            PDBGFCFGBBINT pCfgBb = dbgfR3CfgBbCreate(pThis, pAddressStart, 10);
            if (RT_LIKELY(pCfgBb))
            {
                pCfgBb->fFlags |= DBGF_CFG_BB_F_ENTRY;
                dbgfR3CfgLink(pThis, pCfgBb);
                rc = dbgfR3CfgPopulate(pUVM, idCpu, pThis, pAddressStart, cbDisasmMax, fFlags);
                if (RT_SUCCESS(rc))
                {
                    *phCfg = pThis;
                    return VINF_SUCCESS;
                }
            }
            else
                rc = VERR_NO_MEMORY;
        }

        ASMAtomicDecU32(&pThis->cRefs);
        dbgfR3CfgDestroy(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Retains the control flow graph handle.
 *
 * @returns Current reference count.
 * @param   hCfg                The control flow graph handle to retain.
 */
VMMR3DECL(uint32_t) DBGFR3CfgRetain(DBGFCFG hCfg)
{
    PDBGFCFGINT pThis = hCfg;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


/**
 * Releases the control flow graph handle.
 *
 * @returns Current reference count, on 0 the control flow graph will be destroyed.
 * @param   hCfg                The control flow graph handle to release.
 */
VMMR3DECL(uint32_t) DBGFR3CfgRelease(DBGFCFG hCfg)
{
    PDBGFCFGINT pThis = hCfg;
    if (!pThis)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        dbgfR3CfgDestroy(pThis);
    return cRefs;
}


/**
 * Queries the basic block denoting the entry point into the control flow graph.
 *
 * @returns VBox status code.
 * @param   hCfg                The control flow graph handle.
 * @param   phCfgBb             Where to store the basic block handle on success.
 */
VMMR3DECL(int) DBGFR3CfgQueryStartBb(DBGFCFG hCfg, PDBGFCFGBB phCfgBb)
{
    PDBGFCFGINT pThis = hCfg;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    PDBGFCFGBBINT pCfgBb = NULL;
    RTListForEach(&pThis->LstCfgBb, pCfgBb, DBGFCFGBBINT, NdCfgBb)
    {
        if (pCfgBb->fFlags & DBGF_CFG_BB_F_ENTRY)
        {
            *phCfgBb = pCfgBb;
            return VINF_SUCCESS;
        }
    }

    AssertFailed(); /* Should never get here. */
    return VERR_INTERNAL_ERROR;
}


/**
 * Returns the buffer starting at the given position.
 *
 * @returns Pointer to the ASCII buffer.
 * @param   pScreen             The screen.
 * @param   uX                  Horizontal position.
 * @param   uY                  Vertical position.
 */
static char *dbgfR3CfgDumpScreenGetBufferAtPos(PDBGFCFGDUMPSCREEN pScreen, uint32_t uX, uint32_t uY)
{
    AssertReturn(uX < pScreen->cchWidth && uY < pScreen->cchHeight, NULL);
    return pScreen->pszScreen + (pScreen->cchWidth + pScreen->cchStride) * uY + uX;
}


/**
 * Draws a single pixel to the screen at the given coordinates.
 *
 * @returns nothing.
 * @param   pScreen             The screen.
 * @param   uX                  X coordinate.
 * @param   uY                  Y coordinate.
 * @param   ch                  Character to draw.
 */
DECLINLINE(void) dbgfR3CfgDumpScreenDrawPixel(PDBGFCFGDUMPSCREEN pScreen, uint32_t uX, uint32_t uY, char ch)
{
    char *psz = dbgfR3CfgDumpScreenGetBufferAtPos(pScreen, uX, uY);
    AssertPtrReturnVoid(psz);
    AssertReturnVoid(*psz != '\0');
    *psz = ch;
}


/**
 * Draws a horizontal line at the given coordinates.
 *
 * @returns nothing.
 * @param   pScreen             The screen.
 * @param   uStartX             X position to start drawing.
 * @param   uEndX               X position to draw the line to (inclusive).
 * @param   uY                  Y position.
 * @param   ch                  The character to use for drawing.
 */
static void dbgfR3CfgDumpScreenDrawLineHorizontal(PDBGFCFGDUMPSCREEN pScreen, uint32_t uStartX, uint32_t uEndX,
                                                  uint32_t uY, char ch)
{
    char *psz = dbgfR3CfgDumpScreenGetBufferAtPos(pScreen, uStartX, uY);
    AssertPtrReturnVoid(psz);
    //AssertReturnVoid(psz[uEndX - uStartX + 1] != '\0'); /* Triggers during initialization. */

    memset(psz, ch, uEndX - uStartX + 1);
}


/**
 * Draws a vertical line at the given coordinates.
 *
 * @returns nothing.
 * @param   pScreen             The screen.
 * @param   uX                  X position to draw.
 * @param   uStartY             Y position to start drawing.
 * @param   uEndY               Y position to draw to (inclusive).
 * @param   ch                  The character to use for drawing.
 */
static void dbgfR3CfgDumpScreenDrawLineVertical(PDBGFCFGDUMPSCREEN pScreen, uint32_t uX, uint32_t uStartY,
                                                uint32_t uEndY, char ch)
{
    while (uStartY <= uEndY)
    {
        char *psz = dbgfR3CfgDumpScreenGetBufferAtPos(pScreen, uX, uStartY);
        AssertPtrReturnVoid(psz);
        *psz = ch;
        uStartY++;
    }
}


/**
 * Creates a new ASCII screen for layouting.
 *
 * @returns VBox status code.
 * @param   ppScreen            Where to store the screen instance on success.
 * @param   cchWidth            Width of the screen in characters.
 * @param   cchHeight           Height of the screen in characters.
 */
static int dbgfR3CfgDumpScreenCreate(PDBGFCFGDUMPSCREEN *ppScreen, uint32_t cchWidth, uint32_t cchHeight)
{
    int rc = VINF_SUCCESS;

    PDBGFCFGDUMPSCREEN pScreen = (PDBGFCFGDUMPSCREEN)RTMemAllocZ(sizeof(DBGFCFGDUMPSCREEN));
    if (pScreen)
    {
        pScreen->cchWidth  = cchWidth;
        pScreen->cchHeight = cchHeight;
        pScreen->cchStride = 1; /* Zero terminators after every line. */
        pScreen->pszScreen = RTStrAlloc((cchWidth + 1) * cchHeight * sizeof(char));
        if (pScreen->pszScreen)
        {
            memset(pScreen->pszScreen, 0, (cchWidth + 1) * cchHeight * sizeof(char));
            /* Initialize the screen with spaces. */
            for (uint32_t i = 0; i < cchHeight; i++)
                dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, 0, cchWidth, i, ' ');
            *ppScreen = pScreen;
        }
        else
            rc = VERR_NO_STR_MEMORY;

        if (RT_FAILURE(rc))
            RTMemFree(pScreen);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Destroys a given ASCII screen.
 *
 * @returns nothing.
 * @param   pScreen             The screen to destroy.
 */
static void dbgfR3CfgDumpScreenDestroy(PDBGFCFGDUMPSCREEN pScreen)
{
    RTStrFree(pScreen->pszScreen);
    RTMemFree(pScreen);
}


/**
 * Blits the entire screen using the dumper callback.
 *
 * @returns VBox status code.
 * @param   pScreen             The screen to blit.
 * @param   pfnDump             Dumper callback.
 * @param   pvUser              Opaque user data to pass to the dumper callback.
 */
static int dbgfR3CfgDumpScreenBlit(PDBGFCFGDUMPSCREEN pScreen, PFNDBGFR3CFGDUMP pfnDump, void *pvUser)
{
    int rc = VINF_SUCCESS;

    for (uint32_t iY = 0; iY < pScreen->cchHeight && RT_SUCCESS(rc); iY++)
    {
        /* Play safe and restore line endings. */
        char *psz = dbgfR3CfgDumpScreenGetBufferAtPos(pScreen, 0, iY);
        psz[pScreen->cchWidth] = '\0';
        rc = pfnDump(psz, pvUser);
    }

    return rc;
}


/**
 * Calculates the size required for the given basic block including the
 * border and spacing on the edges.
 *
 * @returns nothing.
 * @param   pCfgBb              The basic block.
 * @param   pDumpBb             The dumper state to fill in for the basic block.
 */
static void dbgfR3CfgDumpBbCalcSizes(PDBGFCFGBBINT pCfgBb, PDBGFCFGDUMPBB pDumpBb)
{
    pDumpBb->pCfgBb = pCfgBb;
    pDumpBb->cchHeight = pCfgBb->cInstr + 4; /* Include spacing and border top and bottom. */
    if (   RT_FAILURE(pCfgBb->rcError)
        && pCfgBb->pszErr)
    {
        pDumpBb->cchHeight++;
        pDumpBb->cchWidth = RT_MAX(pDumpBb->cchWidth, (uint32_t)strlen(pCfgBb->pszErr));
    }
    pDumpBb->cchWidth = 0;
    for (unsigned i = 0; i < pCfgBb->cInstr; i++)
        pDumpBb->cchWidth = RT_MAX(pDumpBb->cchWidth, (uint32_t)strlen(pCfgBb->aInstr[i].pszInstr));
    pDumpBb->cchWidth += 4; /* Include spacing and border left and right. */
}


/**
 * Dumps a top or bottom boundary line.
 *
 * @returns nothing.
 * @param   pScreen             The screen to draw to.
 * @param   uStartX             Where to start drawing the boundary.
 * @param   uStartY             Y coordinate.
 * @param   cchWidth            Width of the boundary.
 */
static void dbgfR3CfgDumpBbBoundary(PDBGFCFGDUMPSCREEN pScreen, uint32_t uStartX, uint32_t uStartY, uint32_t cchWidth)
{
    dbgfR3CfgDumpScreenDrawPixel(pScreen, uStartX, uStartY, '+');
    dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, uStartX + 1, uStartX + 1 + cchWidth - 2, uStartY, '-');
    dbgfR3CfgDumpScreenDrawPixel(pScreen, uStartX + cchWidth - 1, uStartY, '+');
}


/**
 * Dumps a spacing line between the top or bottom boundary and the actual disassembly.
 *
 * @returns nothing.
 * @param   pScreen             The screen to draw to.
 * @param   uStartX             Where to start drawing the spacing.
 * @param   uStartY             Y coordinate.
 * @param   cchWidth            Width of the spacing.
 */
static void dbgfR3CfgDumpBbSpacing(PDBGFCFGDUMPSCREEN pScreen, uint32_t uStartX, uint32_t uStartY, uint32_t cchWidth)
{
    dbgfR3CfgDumpScreenDrawPixel(pScreen, uStartX, uStartY, '|');
    dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, uStartX + 1, uStartX + 1 + cchWidth - 2, uStartY, ' ');
    dbgfR3CfgDumpScreenDrawPixel(pScreen, uStartX + cchWidth - 1, uStartY, '|');
}


/**
 * Writes a given text to the screen.
 *
 * @returns nothing.
 * @returns nothing.
 * @param   pScreen             The screen to draw to.
 * @param   uStartX             Where to start drawing the line.
 * @param   uStartY             Y coordinate.
 * @param   cchWidth            Maximum width of the text.
 * @param   pszText             The text to write.
 */
static void dbgfR3CfgDumpBbText(PDBGFCFGDUMPSCREEN pScreen, uint32_t uStartX, uint32_t uStartY,
                                uint32_t cchWidth, const char *pszText)
{
        char *psz = dbgfR3CfgDumpScreenGetBufferAtPos(pScreen, uStartX, uStartY);
        AssertPtrReturnVoid(psz);
        size_t cch = 0;
        size_t cchText = strlen(pszText);
        psz[cch++] = '|';
        psz[cch++] = ' ';
        int rc = RTStrCopyEx(&psz[cch], cchWidth, pszText, cchText);
        AssertRC(rc);

        /* Fill the rest with spaces. */
        cch += cchText;
        while (cch < cchWidth - 1)
            psz[cch++] = ' ';
        psz[cch] = '|';
}


/**
 * Dumps one basic block using the dumper callback.
 *
 * @returns nothing.
 * @param   pDumpBb             The basic block dump state to dump.
 * @param   pScreen             The screen to dump to.
 */
static void dbgfR3CfgDumpBb(PDBGFCFGDUMPBB pDumpBb, PDBGFCFGDUMPSCREEN pScreen)
{
    uint32_t uStartY = pDumpBb->uStartY;

    dbgfR3CfgDumpBbBoundary(pScreen, pDumpBb->uStartX, uStartY, pDumpBb->cchWidth);
    uStartY++;
    dbgfR3CfgDumpBbSpacing(pScreen, pDumpBb->uStartX, uStartY, pDumpBb->cchWidth);
    uStartY++;

    for (unsigned i = 0; i < pDumpBb->pCfgBb->cInstr; i++)
        dbgfR3CfgDumpBbText(pScreen, pDumpBb->uStartX, uStartY + i,
                            pDumpBb->cchWidth, pDumpBb->pCfgBb->aInstr[i].pszInstr);
    uStartY += pDumpBb->pCfgBb->cInstr;

    if (pDumpBb->pCfgBb->pszErr)
    {
        dbgfR3CfgDumpBbText(pScreen, pDumpBb->uStartX, uStartY, pDumpBb->cchWidth,
                            pDumpBb->pCfgBb->pszErr);
        uStartY++;
    }

    dbgfR3CfgDumpBbSpacing(pScreen, pDumpBb->uStartX, uStartY, pDumpBb->cchWidth);
    uStartY++;
    dbgfR3CfgDumpBbBoundary(pScreen, pDumpBb->uStartX, uStartY, pDumpBb->cchWidth);
    uStartY++;
}


/**
 * @callback_method_impl{FNRTSORTCMP}
 */
static DECLCALLBACK(int) dbgfR3CfgDumpSortCmp(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    RT_NOREF(pvUser);
    PDBGFCFGDUMPBB pCfgDumpBb1 = (PDBGFCFGDUMPBB)pvElement1;
    PDBGFCFGDUMPBB pCfgDumpBb2 = (PDBGFCFGDUMPBB)pvElement2;

    if (dbgfR3CfgBbAddrEqual(&pCfgDumpBb1->pCfgBb->AddrStart, &pCfgDumpBb2->pCfgBb->AddrStart))
        return 0;
    else if (dbgfR3CfgBbAddrLower(&pCfgDumpBb1->pCfgBb->AddrStart, &pCfgDumpBb2->pCfgBb->AddrStart))
        return -1;

    return 1;
}


/**
 * Dumps a given control flow graph as ASCII using the given dumper callback.
 *
 * @returns VBox status code.
 * @param   hCfg                The control flow graph handle.
 * @param   pfnDump             The dumper callback.
 * @param   pvUser              Opaque user data to pass to the dumper callback.
 */
VMMR3DECL(int) DBGFR3CfgDump(DBGFCFG hCfg, PFNDBGFR3CFGDUMP pfnDump, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PDBGFCFGINT pThis = hCfg;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pfnDump, VERR_INVALID_POINTER);

    PDBGFCFGDUMPBB paDumpBb = (PDBGFCFGDUMPBB)RTMemTmpAllocZ(pThis->cBbs * sizeof(DBGFCFGDUMPBB));
    if (paDumpBb)
    {
        /* Calculate the sizes of each basic block first. */
        PDBGFCFGBBINT pCfgBb = NULL;
        uint32_t idxDumpBb = 0;
        RTListForEach(&pThis->LstCfgBb, pCfgBb, DBGFCFGBBINT, NdCfgBb)
        {
            dbgfR3CfgDumpBbCalcSizes(pCfgBb, &paDumpBb[idxDumpBb]);
            idxDumpBb++;
        }

        /* Sort the blocks by address. */
        RTSortShell(paDumpBb, pThis->cBbs, sizeof(DBGFCFGDUMPBB), dbgfR3CfgDumpSortCmp, NULL);

        /* Calculate the ASCII screen dimensions and create one. */
        uint32_t cchWidth = 0;
        uint32_t cchLeftExtra = 5;
        uint32_t cchRightExtra = 5;
        uint32_t cchHeight = 0;
        for (unsigned i = 0; i < pThis->cBbs; i++)
        {
            PDBGFCFGDUMPBB pDumpBb = &paDumpBb[i];
            cchWidth = RT_MAX(cchWidth, pDumpBb->cchWidth);
            cchHeight += pDumpBb->cchHeight;
            switch (pDumpBb->pCfgBb->enmEndType)
            {
                case DBGFCFGBBENDTYPE_EXIT:
                case DBGFCFGBBENDTYPE_LAST_DISASSEMBLED:
                    break;
                case DBGFCFGBBENDTYPE_UNCOND_JMP:
                    if (   dbgfR3CfgBbAddrLower(&pDumpBb->pCfgBb->AddrTarget, &pDumpBb->pCfgBb->AddrStart)
                        || dbgfR3CfgBbAddrEqual(&pDumpBb->pCfgBb->AddrTarget, &pDumpBb->pCfgBb->AddrStart))
                        cchLeftExtra++;
                    else
                        cchRightExtra++;
                    break;
                case DBGFCFGBBENDTYPE_UNCOND:
                    cchHeight += 2; /* For the arrow down to the next basic block. */
                    break;
                case DBGFCFGBBENDTYPE_COND:
                    cchHeight += 2; /* For the arrow down to the next basic block. */
                    if (   dbgfR3CfgBbAddrLower(&pDumpBb->pCfgBb->AddrTarget, &pDumpBb->pCfgBb->AddrStart)
                        || dbgfR3CfgBbAddrEqual(&pDumpBb->pCfgBb->AddrTarget, &pDumpBb->pCfgBb->AddrStart))
                        cchLeftExtra++;
                    else
                        cchRightExtra++;
                    break;
                default:
                    AssertFailed();
            }
        }

        cchWidth += 2;

        PDBGFCFGDUMPSCREEN pScreen = NULL;
        rc = dbgfR3CfgDumpScreenCreate(&pScreen, cchWidth + cchLeftExtra + cchRightExtra, cchHeight);
        if (RT_SUCCESS(rc))
        {
            uint32_t uY = 0;

            /* Dump the basic blocks and connections to the immediate successor. */
            for (unsigned i = 0; i < pThis->cBbs; i++)
            {
                paDumpBb[i].uStartX = cchLeftExtra + (cchWidth - paDumpBb[i].cchWidth) / 2;
                paDumpBb[i].uStartY = uY;
                dbgfR3CfgDumpBb(&paDumpBb[i], pScreen);
                uY += paDumpBb[i].cchHeight;

                switch (paDumpBb[i].pCfgBb->enmEndType)
                {
                    case DBGFCFGBBENDTYPE_EXIT:
                    case DBGFCFGBBENDTYPE_LAST_DISASSEMBLED:
                    case DBGFCFGBBENDTYPE_UNCOND_JMP:
                        break;
                    case DBGFCFGBBENDTYPE_UNCOND:
                    case DBGFCFGBBENDTYPE_COND:
                        /* Draw the arrow down to the next block. */
                        dbgfR3CfgDumpScreenDrawPixel(pScreen, cchLeftExtra + cchWidth / 2, uY, '|');
                        uY++;
                        dbgfR3CfgDumpScreenDrawPixel(pScreen, cchLeftExtra + cchWidth / 2, uY, 'V');
                        uY++;
                        break;
                    default:
                        AssertFailed();
                }
            }

            /* Last pass, connect all remaining branches. */
            uint32_t uBackConns = 0;
            uint32_t uFwdConns = 0;
            for (unsigned i = 0; i < pThis->cBbs; i++)
            {
                PDBGFCFGDUMPBB pDumpBb = &paDumpBb[i];

                switch (pDumpBb->pCfgBb->enmEndType)
                {
                    case DBGFCFGBBENDTYPE_EXIT:
                    case DBGFCFGBBENDTYPE_LAST_DISASSEMBLED:
                    case DBGFCFGBBENDTYPE_UNCOND:
                        break;
                    case DBGFCFGBBENDTYPE_COND:
                    case DBGFCFGBBENDTYPE_UNCOND_JMP:
                    {
                        /* Find the target first to get the coordinates. */
                        PDBGFCFGDUMPBB pDumpBbTgt = NULL;
                        for (idxDumpBb = 0; idxDumpBb < pThis->cBbs; idxDumpBb++)
                        {
                            pDumpBbTgt = &paDumpBb[idxDumpBb];
                            if (dbgfR3CfgBbAddrEqual(&pDumpBb->pCfgBb->AddrTarget, &pDumpBbTgt->pCfgBb->AddrStart))
                                break;
                        }

                        /*
                         * Use the right side for targets with higher addresses,
                         * left when jumping backwards.
                         */
                        if (   dbgfR3CfgBbAddrLower(&pDumpBb->pCfgBb->AddrTarget, &pDumpBb->pCfgBb->AddrStart)
                            || dbgfR3CfgBbAddrEqual(&pDumpBb->pCfgBb->AddrTarget, &pDumpBb->pCfgBb->AddrStart))
                        {
                            /* Going backwards. */
                            uint32_t uXVerLine = /*cchLeftExtra - 1 -*/ uBackConns + 1;
                            uint32_t uYHorLine = pDumpBb->uStartY + pDumpBb->cchHeight - 1 - 2;
                            uBackConns++;

                            /* Draw the arrow pointing to the target block. */
                            dbgfR3CfgDumpScreenDrawPixel(pScreen, pDumpBbTgt->uStartX - 1, pDumpBbTgt->uStartY, '>');
                            /* Draw the horizontal line. */
                            dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, uXVerLine + 1, pDumpBbTgt->uStartX - 2,
                                                                  pDumpBbTgt->uStartY, '-');
                            dbgfR3CfgDumpScreenDrawPixel(pScreen, uXVerLine, pDumpBbTgt->uStartY, '+');
                            /* Draw the vertical line down to the source block. */
                            dbgfR3CfgDumpScreenDrawLineVertical(pScreen, uXVerLine, pDumpBbTgt->uStartY + 1, uYHorLine - 1, '|');
                            dbgfR3CfgDumpScreenDrawPixel(pScreen, uXVerLine, uYHorLine, '+');
                            /* Draw the horizontal connection between the source block and vertical part. */
                            dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, uXVerLine + 1, pDumpBb->uStartX - 1,
                                                                  uYHorLine, '-');

                        }
                        else
                        {
                            /* Going forward. */
                            uint32_t uXVerLine = cchWidth + cchLeftExtra + (cchRightExtra - uFwdConns) - 1;
                            uint32_t uYHorLine = pDumpBb->uStartY + pDumpBb->cchHeight - 1 - 2;
                            uFwdConns++;

                            /* Draw the horizontal line. */
                            dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, pDumpBb->uStartX + pDumpBb->cchWidth,
                                                                  uXVerLine - 1, uYHorLine, '-');
                            dbgfR3CfgDumpScreenDrawPixel(pScreen, uXVerLine, uYHorLine, '+');
                            /* Draw the vertical line down to the target block. */
                            dbgfR3CfgDumpScreenDrawLineVertical(pScreen, uXVerLine, uYHorLine + 1, pDumpBbTgt->uStartY - 1, '|');
                            /* Draw the horizontal connection between the target block and vertical part. */
                            dbgfR3CfgDumpScreenDrawLineHorizontal(pScreen, pDumpBbTgt->uStartX + pDumpBbTgt->cchWidth,
                                                                  uXVerLine, pDumpBbTgt->uStartY, '-');
                            dbgfR3CfgDumpScreenDrawPixel(pScreen, uXVerLine, pDumpBbTgt->uStartY, '+');
                            /* Draw the arrow pointing to the target block. */
                            dbgfR3CfgDumpScreenDrawPixel(pScreen, pDumpBbTgt->uStartX + pDumpBbTgt->cchWidth,
                                                         pDumpBbTgt->uStartY, '<');
                        }
                        break;
                    }
                    default:
                        AssertFailed();
                }
            }

            rc = dbgfR3CfgDumpScreenBlit(pScreen, pfnDump, pvUser);
            dbgfR3CfgDumpScreenDestroy(pScreen);
        }

        RTMemTmpFree(paDumpBb);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Retains the basic block handle.
 *
 * @returns Current reference count.
 * @param   hCfgBb              The basic block handle to retain.
 */
VMMR3DECL(uint32_t) DBGFR3CfgBbRetain(DBGFCFGBB hCfgBb)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pCfgBb->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p %d\n", cRefs, pCfgBb, pCfgBb->enmEndType));
    return cRefs;
}


/**
 * Releases the basic block handle.
 *
 * @returns Current reference count, on 0 the basic block will be destroyed.
 * @param   hCfgBb              The basic block handle to release.
 */
VMMR3DECL(uint32_t) DBGFR3CfgBbRelease(DBGFCFGBB hCfgBb)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    if (!pCfgBb)
        return 0;

    return dbgfR3CfgBbReleaseInt(pCfgBb, true /* fMayDestroyCfg */);
}


/**
 * Returns the start address of the basic block.
 *
 * @returns Pointer to DBGF adress containing the start address of the basic block.
 * @param   hCfgBb              The basic block handle.
 * @param   pAddrStart          Where to store the start address of the basic block.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3CfgBbGetStartAddress(DBGFCFGBB hCfgBb, PDBGFADDRESS pAddrStart)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, NULL);
    AssertPtrReturn(pAddrStart, NULL);

    *pAddrStart = pCfgBb->AddrStart;
    return pAddrStart;
}


/**
 * Returns the end address of the basic block (inclusive).
 *
 * @returns Pointer to DBGF adress containing the end address of the basic block.
 * @param   hCfgBb              The basic block handle.
 * @param   pAddrEnd            Where to store the end address of the basic block.
 */
VMMR3DECL(PDBGFADDRESS) DBGFR3CfgBbGetEndAddress(DBGFCFGBB hCfgBb, PDBGFADDRESS pAddrEnd)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, NULL);
    AssertPtrReturn(pAddrEnd, NULL);

    *pAddrEnd = pCfgBb->AddrEnd;
    return pAddrEnd;
}


/**
 * Returns the type of the last instruction in the basic block.
 *
 * @returns Last instruction type.
 * @param   hCfgBb              The basic block handle.
 */
VMMR3DECL(DBGFCFGBBENDTYPE) DBGFR3CfgBbGetType(DBGFCFGBB hCfgBb)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, DBGFCFGBBENDTYPE_INVALID);

    return pCfgBb->enmEndType;
}


/**
 * Get the number of instructions contained in the basic block.
 *
 * @returns Number of instructions in the basic block.
 * @param   hCfgBb              The basic block handle.
 */
VMMR3DECL(uint32_t) DBGFR3CfgBbGetInstrCount(DBGFCFGBB hCfgBb)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, 0);

    return pCfgBb->cInstr;
}


/**
 * Get flags for the given basic block.
 *
 * @returns Combination of DBGF_CFG_BB_F_*
 * @param   hCfgBb              The basic block handle.
 */
VMMR3DECL(uint32_t) DBGFR3CfgBbGetFlags(DBGFCFGBB hCfgBb)
{
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, 0);

    return pCfgBb->fFlags;
}


/**
 * Store the disassembled instruction as a string in the given output buffer.
 *
 * @returns VBox status code.
 * @retval VERR_BUFFER_OVERFLOW if the size of the output buffer can't hold the complete string.
 * @param   hCfgBb              The basic block handle.
 * @param   idxInstr            The instruction to query.
 * @param   pAddrInstr          Where to store the guest instruction address on success, optional.
 * @param   pcbInstr            Where to store the instruction size on success, optional.
 * @param   pszOutput           Where to store the disassembled instruction, optional.
 * @param   cbOutput            Size of the output buffer.
 */
VMMR3DECL(int) DBGFR3CfgBbQueryInstr(DBGFCFGBB hCfgBb, uint32_t idxInstr, PDBGFADDRESS pAddrInstr,
                                     uint32_t *pcbInstr, char *pszOutput, uint32_t cbOutput)
{
    int rc = VINF_SUCCESS;
    PDBGFCFGBBINT pCfgBb = hCfgBb;
    AssertPtrReturn(pCfgBb, VERR_INVALID_POINTER);
    AssertReturn(idxInstr < pCfgBb->cInstr, VERR_INVALID_PARAMETER);
    AssertReturn(   (VALID_PTR(pszOutput) && cbOutput > 0)
                 || (!pszOutput && !cbOutput), VERR_INVALID_PARAMETER);

    if (pAddrInstr)
        *pAddrInstr = pCfgBb->aInstr[idxInstr].AddrInstr;
    if (pcbInstr)
        *pcbInstr = pCfgBb->aInstr[idxInstr].cbInstr;
    if (cbOutput)
        rc = RTStrCopy(pszOutput, cbOutput, pCfgBb->aInstr[idxInstr].pszInstr);

    return rc;
}


/**
 * Queries the successors of the basic block.
 *
 * @returns VBox status code.
 * @param   hCfgBb              The basic block handle.
 * @param   pahCfgBbSucc        Where to store the handles to the basic blocks succeeding the given one.
 * @param   cSucc               Number of entries the handle array can hold.
 */
VMMR3DECL(int) DBGFR3CfgBbQuerySuccessors(DBGFCFGBB hCfgBb, PDBGFCFGBB pahCfgBbSucc, uint32_t cSucc)
{
    RT_NOREF3(hCfgBb, pahCfgBbSucc, cSucc);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Returns the number of basic blocks referencing this basic block as a target.
 *
 * @returns Number of other basic blocks referencing this one.
 * @param   hCfgBb              The basic block handle.
 */
VMMR3DECL(uint32_t) DBGFR3CfgBbGetRefBbCount(DBGFCFGBB hCfgBb)
{
    RT_NOREF1(hCfgBb);
    return 0;
}


/**
 * Returns the basic block handles referencing the given basic block.
 *
 * @returns VBox status code.
 * @param   hCfgBb              The basic block handle.
 * @param   paCfgBbRef          Pointer to the array containing the referencing basic block handles on success.
 * @param   cRef                Number of entries in the given array.
 */
VMMR3DECL(int) DBGFR3CfgBbGetRefBb(DBGFCFGBB hCfgBb, PDBGFCFGBB paCfgBbRef, uint32_t cRef)
{
    RT_NOREF3(hCfgBb, paCfgBbRef, cRef);
    return VERR_NOT_IMPLEMENTED;
}

