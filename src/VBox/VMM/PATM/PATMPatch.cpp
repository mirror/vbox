/* $Id$ */
/** @file
 * PATMPatch - Dynamic Guest OS Instruction patches
 *
 * NOTE: CSAM assumes patch memory is never reused!!
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
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/patm.h>
#include <VBox/stam.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/pdm.h>
#include <VBox/trpm.h>
#include <VBox/param.h>
#include <iprt/avl.h>
#include "PATMInternal.h"
#include <VBox/vm.h>
#include <VBox/csam.h>

#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>

#include <stdlib.h>
#include <stdio.h>
#include "PATMA.h"
#include "PATMPatch.h"

/* internal structure for passing more information about call fixups to patmPatchGenCode */
typedef struct
{
    RTGCPTR32   pTargetGC;
    RTGCPTR32   pCurInstrGC;
    RTGCPTR32   pNextInstrGC;
    RTGCPTR32   pReturnGC;
} PATMCALLINFO, *PPATMCALLINFO;

int patmPatchAddReloc32(PVM pVM, PPATCHINFO pPatch, uint8_t *pRelocHC, uint32_t uType, RTGCPTR32 pSource, RTGCPTR32 pDest)
{
    PRELOCREC pRec;

    Assert(uType == FIXUP_ABSOLUTE || ((uType == FIXUP_REL_JMPTOPATCH || uType == FIXUP_REL_JMPTOGUEST) && pSource && pDest));

    LogFlow(("patmPatchAddReloc32 type=%d pRelocGC=%VGv source=%VGv dest=%VGv\n", uType, pRelocHC - pVM->patm.s.pPatchMemGC + pVM->patm.s.pPatchMemGC , pSource, pDest));

    pRec = (PRELOCREC)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(*pRec));
    Assert(pRec);
    pRec->Core.Key  = (AVLPVKEY)pRelocHC;
    pRec->pRelocPos = pRelocHC; /* @todo redundant. */
    pRec->pSource   = pSource;
    pRec->pDest     = pDest;
    pRec->uType     = uType;

    bool ret = RTAvlPVInsert(&pPatch->FixupTree, &pRec->Core);
    Assert(ret); NOREF(ret);
    pPatch->nrFixups++;

    return VINF_SUCCESS;
}

int patmPatchAddJump(PVM pVM, PPATCHINFO pPatch, uint8_t *pJumpHC, uint32_t offset, RTGCPTR32 pTargetGC, uint32_t opcode)
{
    PJUMPREC pRec;

    pRec = (PJUMPREC)MMR3HeapAllocZ(pVM, MM_TAG_PATM_PATCH, sizeof(*pRec));
    Assert(pRec);

    pRec->Core.Key  = (AVLPVKEY)pJumpHC;
    pRec->pJumpHC   = pJumpHC; /* @todo redundant. */
    pRec->offDispl  = offset;
    pRec->pTargetGC = pTargetGC;
    pRec->opcode    = opcode;

    bool ret = RTAvlPVInsert(&pPatch->JumpTree, &pRec->Core);
    Assert(ret); NOREF(ret);
    pPatch->nrJumpRecs++;

    return VINF_SUCCESS;
}

#define PATCHGEN_PROLOG_NODEF(pVM, pPatch)                                      \
    pPB = PATCHCODE_PTR_HC(pPatch) + pPatch->uCurPatchOffset;            \
                                                                               \
    if (pPB + 256 >= pVM->patm.s.pPatchMemHC + pVM->patm.s.cbPatchMem)          \
    {                                                                          \
        pVM->patm.s.fOutOfMemory = true; \
        Assert(pPB + 256 >= pVM->patm.s.pPatchMemHC + pVM->patm.s.cbPatchMem); \
        return VERR_NO_MEMORY; \
    }

#define PATCHGEN_PROLOG(pVM, pPatch)                                      \
    uint8_t *pPB;                                                         \
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);


#define PATCHGEN_EPILOG(pPatch, size) \
    Assert(size <= 640);              \
    pPatch->uCurPatchOffset += size;


static uint32_t patmPatchGenCode(PVM pVM, PPATCHINFO pPatch, uint8_t *pPB, PPATCHASMRECORD pAsmRecord, RCPTRTYPE(uint8_t *) pReturnAddrGC, bool fGenJump,
                                 PPATMCALLINFO pCallInfo = 0)
{
    uint32_t i, j;

    Assert(fGenJump == false || pReturnAddrGC);
    Assert(fGenJump == false || pAsmRecord->offJump);
    Assert(pAsmRecord && pAsmRecord->size > sizeof(pAsmRecord->uReloc[0]));

    // Copy the code block
    memcpy(pPB, pAsmRecord->pFunction, pAsmRecord->size);

    // Process all fixups
    for (j=0,i=0;i<pAsmRecord->nrRelocs*2; i+=2)
    {
        for (;j<pAsmRecord->size;j++)
        {
            if (*(uint32_t*)&pPB[j] == pAsmRecord->uReloc[i])
            {
                RCPTRTYPE(uint32_t *) dest;

#ifdef VBOX_STRICT
                if (pAsmRecord->uReloc[i] == PATM_FIXUP)
                    Assert(pAsmRecord->uReloc[i+1] != 0);
                else
                    Assert(pAsmRecord->uReloc[i+1] == 0);
#endif

                switch (pAsmRecord->uReloc[i])
                {
                case PATM_VMFLAGS:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, uVMFlags);
                    break;

                case PATM_PENDINGACTION:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, uPendingAction);
                    break;

                case PATM_FIXUP:
                    /* Offset in uReloc[i+1] is from the base of the function. */
                    dest = (RTGCUINTPTR32)pVM->patm.s.pPatchMemGC + pAsmRecord->uReloc[i+1] + (RTGCUINTPTR32)(pPB - pVM->patm.s.pPatchMemHC);
                    break;
#ifdef VBOX_WITH_STATISTICS
                case PATM_ALLPATCHCALLS:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, uPatchCalls);
                    break;

                case PATM_IRETEFLAGS:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, uIretEFlags);
                    break;

                case PATM_IRETCS:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, uIretCS);
                    break;

                case PATM_IRETEIP:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, uIretEIP);
                    break;

                case PATM_PERPATCHCALLS:
                    dest = patmPatchQueryStatAddress(pVM, pPatch);
                    break;
#endif
                case PATM_STACKPTR:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, Psp);
                    break;

                /* The first part of our PATM stack is used to store offsets of patch return addresses; the 2nd
                 * part to store the original return addresses.
                 */
                case PATM_STACKBASE:
                    dest = pVM->patm.s.pGCStackGC;
                    break;

                case PATM_STACKBASE_GUEST:
                    dest = pVM->patm.s.pGCStackGC + PATM_STACK_SIZE;
                    break;

                case PATM_RETURNADDR:   /* absolute guest address; no fixup required */
                    Assert(pCallInfo && pAsmRecord->uReloc[i] >= PATM_NO_FIXUP);
                    dest = pCallInfo->pReturnGC;
                    break;

                case PATM_PATCHNEXTBLOCK:  /* relative address of instruction following this block */
                    Assert(pCallInfo && pAsmRecord->uReloc[i] >= PATM_NO_FIXUP);

                    /** @note hardcoded assumption that we must return to the instruction following this block */
                    dest = (uintptr_t)pPB - (uintptr_t)pVM->patm.s.pPatchMemHC + pAsmRecord->size;
                    break;

                case PATM_CALLTARGET:   /* relative to patch address; no fixup requird */
                    Assert(pCallInfo && pAsmRecord->uReloc[i] >= PATM_NO_FIXUP);

                    /* Address must be filled in later. (see patmr3SetBranchTargets)  */
                    patmPatchAddJump(pVM, pPatch, &pPB[j-1], 1, pCallInfo->pTargetGC, OP_CALL);
                    dest = PATM_ILLEGAL_DESTINATION;
                    break;

                case PATM_PATCHBASE:    /* Patch GC base address */
                    dest = pVM->patm.s.pPatchMemGC;
                    break;

                case PATM_CPUID_STD_PTR:
                    dest = CPUMGetGuestCpuIdStdGCPtr(pVM);
                    break;

                case PATM_CPUID_EXT_PTR:
                    dest = CPUMGetGuestCpuIdExtGCPtr(pVM);
                    break;

                case PATM_CPUID_CENTAUR_PTR:
                    dest = CPUMGetGuestCpuIdCentaurGCPtr(pVM);
                    break;

                case PATM_CPUID_DEF_PTR:
                    dest = CPUMGetGuestCpuIdDefGCPtr(pVM);
                    break;

                case PATM_CPUID_STD_MAX:
                    dest = CPUMGetGuestCpuIdStdMax(pVM);
                    break;

                case PATM_CPUID_EXT_MAX:
                    dest = CPUMGetGuestCpuIdExtMax(pVM);
                    break;

                case PATM_CPUID_CENTAUR_MAX:
                    dest = CPUMGetGuestCpuIdCentaurMax(pVM);
                    break;

                case PATM_INTERRUPTFLAG:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, fPIF);
                    break;

                case PATM_INHIBITIRQADDR:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, GCPtrInhibitInterrupts);
                    break;

                case PATM_NEXTINSTRADDR:
                    Assert(pCallInfo);
                    /* pNextInstrGC can be 0 if several instructions, that inhibit irqs, follow each other */
                    dest = pCallInfo->pNextInstrGC;
                    break;

                case PATM_CURINSTRADDR:
                    Assert(pCallInfo);
                    dest = pCallInfo->pCurInstrGC;
                    break;

                case PATM_VM_FORCEDACTIONS:
                    dest = pVM->pVMGC + RT_OFFSETOF(VM, fForcedActions);
                    break;

                case PATM_TEMP_EAX:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, Restore.uEAX);
                    break;
                case PATM_TEMP_ECX:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, Restore.uECX);
                    break;
                case PATM_TEMP_EDI:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, Restore.uEDI);
                    break;
                case PATM_TEMP_EFLAGS:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, Restore.eFlags);
                    break;
                case PATM_TEMP_RESTORE_FLAGS:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, Restore.uFlags);
                    break;
                case PATM_CALL_PATCH_TARGET_ADDR:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, GCCallPatchTargetAddr);
                    break;
                case PATM_CALL_RETURN_ADDR:
                    dest = pVM->patm.s.pGCStateGC + RT_OFFSETOF(PATMGCSTATE, GCCallReturnAddr);
                    break;

                /* Relative address of global patm lookup and call function. */
                case PATM_LOOKUP_AND_CALL_FUNCTION:
                {
                    RTGCPTR32 pInstrAfterCall = pVM->patm.s.pPatchMemGC + (RTGCUINTPTR32)(&pPB[j] + sizeof(RTGCPTR32) - pVM->patm.s.pPatchMemHC);
                    Assert(pVM->patm.s.pfnHelperCallGC);
                    Assert(sizeof(uint32_t) == sizeof(RTGCPTR32));

                    /* Relative value is target minus address of instruction after the actual call instruction. */
                    dest = pVM->patm.s.pfnHelperCallGC - pInstrAfterCall;
                    break;
                }

                case PATM_RETURN_FUNCTION:
                {
                    RTGCPTR32 pInstrAfterCall = pVM->patm.s.pPatchMemGC + (RTGCUINTPTR32)(&pPB[j] + sizeof(RTGCPTR32) - pVM->patm.s.pPatchMemHC);
                    Assert(pVM->patm.s.pfnHelperRetGC);
                    Assert(sizeof(uint32_t) == sizeof(RTGCPTR32));

                    /* Relative value is target minus address of instruction after the actual call instruction. */
                    dest = pVM->patm.s.pfnHelperRetGC - pInstrAfterCall;
                    break;
                }

                case PATM_IRET_FUNCTION:
                {
                    RTGCPTR32 pInstrAfterCall = pVM->patm.s.pPatchMemGC + (RTGCUINTPTR32)(&pPB[j] + sizeof(RTGCPTR32) - pVM->patm.s.pPatchMemHC);
                    Assert(pVM->patm.s.pfnHelperIretGC);
                    Assert(sizeof(uint32_t) == sizeof(RTGCPTR32));

                    /* Relative value is target minus address of instruction after the actual call instruction. */
                    dest = pVM->patm.s.pfnHelperIretGC - pInstrAfterCall;
                    break;
                }

                case PATM_LOOKUP_AND_JUMP_FUNCTION:
                {
                    RTGCPTR32 pInstrAfterCall = pVM->patm.s.pPatchMemGC + (RTGCUINTPTR32)(&pPB[j] + sizeof(RTGCPTR32) - pVM->patm.s.pPatchMemHC);
                    Assert(pVM->patm.s.pfnHelperJumpGC);
                    Assert(sizeof(uint32_t) == sizeof(RTGCPTR32));

                    /* Relative value is target minus address of instruction after the actual call instruction. */
                    dest = pVM->patm.s.pfnHelperJumpGC - pInstrAfterCall;
                    break;
                }

                default:
                    dest = PATM_ILLEGAL_DESTINATION;
                    AssertRelease(0);
                    break;
                }

                *(RTGCPTR32 *)&pPB[j] = dest;
                if (pAsmRecord->uReloc[i] < PATM_NO_FIXUP)
                {
                    patmPatchAddReloc32(pVM, pPatch, &pPB[j], FIXUP_ABSOLUTE);
                }
                break;
            }
        }
        Assert(j < pAsmRecord->size);
    }
    Assert(pAsmRecord->uReloc[i] == 0xffffffff);

    /* Add the jump back to guest code (if required) */
    if (fGenJump)
    {
        int32_t displ = pReturnAddrGC - (PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset + pAsmRecord->offJump - 1 + SIZEOF_NEARJUMP32);

        /* Add lookup record for patch to guest address translation */
        Assert(pPB[pAsmRecord->offJump - 1] == 0xE9);
        patmr3AddP2GLookupRecord(pVM, pPatch, &pPB[pAsmRecord->offJump - 1], pReturnAddrGC, PATM_LOOKUP_PATCH2GUEST);

        *(uint32_t *)&pPB[pAsmRecord->offJump] = displ;
        patmPatchAddReloc32(pVM, pPatch, &pPB[pAsmRecord->offJump], FIXUP_REL_JMPTOGUEST,
                        PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset + pAsmRecord->offJump - 1 + SIZEOF_NEARJUMP32,
                        pReturnAddrGC);
    }

    // Calculate the right size of this patch block
    if ((fGenJump && pAsmRecord->offJump) || (!fGenJump && !pAsmRecord->offJump))
    {
        return pAsmRecord->size;
    }
    else {
        // if a jump instruction is present and we don't want one, then subtract SIZEOF_NEARJUMP32
        return pAsmRecord->size - SIZEOF_NEARJUMP32;
    }
}

/* Read bytes and check for overwritten instructions. */
static int patmPatchReadBytes(PVM pVM, uint8_t *pDest, RTGCPTR32 pSrc, uint32_t cb)
{
    int rc = PGMPhysReadGCPtr(pVM, pDest, pSrc, cb);
    AssertRCReturn(rc, rc);
    /*
     * Could be patched already; make sure this is checked!
     */
    for (uint32_t i=0;i<cb;i++)
    {
        uint8_t temp;

        int rc2 = PATMR3QueryOpcode(pVM, pSrc+i, &temp);
        if (VBOX_SUCCESS(rc2))
        {
            pDest[i] = temp;
        }
        else
            break;  /* no more */
    }
    return VINF_SUCCESS;
}

int patmPatchGenDuplicate(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pCurInstrGC)
{
    int rc = VINF_SUCCESS;
    PATCHGEN_PROLOG(pVM, pPatch);

    rc = patmPatchReadBytes(pVM, pPB, pCurInstrGC, pCpu->opsize);
    AssertRC(rc);
    PATCHGEN_EPILOG(pPatch, pCpu->opsize);
    return rc;
}

int patmPatchGenIret(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pCurInstrGC, bool fSizeOverride)
{
    uint32_t size;
    PATMCALLINFO callInfo;

    PATCHGEN_PROLOG(pVM, pPatch);

    AssertMsg(fSizeOverride == false, ("operand size override!!\n"));

    callInfo.pCurInstrGC = pCurInstrGC;

    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMIretRecord, 0, false, &callInfo);

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

int patmPatchGenCli(PVM pVM, PPATCHINFO pPatch)
{
    uint32_t size;
    PATCHGEN_PROLOG(pVM, pPatch);

    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMCliRecord, 0, false);

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

/*
 * Generate an STI patch
 */
int patmPatchGenSti(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pCurInstrGC, RTGCPTR32 pNextInstrGC)
{
    PATMCALLINFO callInfo;
    uint32_t     size;

    Log(("patmPatchGenSti at %VGv; next %VGv\n", pCurInstrGC, pNextInstrGC));
    PATCHGEN_PROLOG(pVM, pPatch);
    callInfo.pNextInstrGC = pNextInstrGC;
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMStiRecord, 0, false, &callInfo);
    PATCHGEN_EPILOG(pPatch, size);

    return VINF_SUCCESS;
}


int patmPatchGenPopf(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pReturnAddrGC, bool fSizeOverride, bool fGenJumpBack)
{
    uint32_t        size;
    PATMCALLINFO    callInfo;

    PATCHGEN_PROLOG(pVM, pPatch);

    callInfo.pNextInstrGC = pReturnAddrGC;

    Log(("patmPatchGenPopf at %VGv\n", pReturnAddrGC));

    /* Note: keep IOPL in mind when changing any of this!! (see comments in PATMA.asm, PATMPopf32Replacement) */
    if (fSizeOverride == true)
    {
        Log(("operand size override!!\n"));
        size = patmPatchGenCode(pVM, pPatch, pPB, (fGenJumpBack) ? &PATMPopf16Record : &PATMPopf16Record_NoExit , pReturnAddrGC, fGenJumpBack, &callInfo);
    }
    else
    {
        size = patmPatchGenCode(pVM, pPatch, pPB, (fGenJumpBack) ? &PATMPopf32Record : &PATMPopf32Record_NoExit, pReturnAddrGC, fGenJumpBack, &callInfo);
    }

    PATCHGEN_EPILOG(pPatch, size);
    STAM_COUNTER_INC(&pVM->patm.s.StatGenPopf);
    return VINF_SUCCESS;
}

int patmPatchGenPushf(PVM pVM, PPATCHINFO pPatch, bool fSizeOverride)
{
    uint32_t size;
    PATCHGEN_PROLOG(pVM, pPatch);

    if (fSizeOverride == true)
    {
        Log(("operand size override!!\n"));
        size = patmPatchGenCode(pVM, pPatch, pPB, &PATMPushf16Record, 0, false);
    }
    else
    {
        size = patmPatchGenCode(pVM, pPatch, pPB, &PATMPushf32Record, 0, false);
    }

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

int patmPatchGenPushCS(PVM pVM, PPATCHINFO pPatch)
{
    uint32_t size;
    PATCHGEN_PROLOG(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMPushCSRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

int patmPatchGenLoop(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pTargetGC, uint32_t opcode, bool fSizeOverride)
{
    uint32_t size = 0;
    PPATCHASMRECORD pPatchAsmRec;

    PATCHGEN_PROLOG(pVM, pPatch);

    switch (opcode)
    {
    case OP_LOOP:
        pPatchAsmRec = &PATMLoopRecord;
        break;
    case OP_LOOPNE:
        pPatchAsmRec = &PATMLoopNZRecord;
        break;
    case OP_LOOPE:
        pPatchAsmRec = &PATMLoopZRecord;
        break;
    case OP_JECXZ:
        pPatchAsmRec = &PATMJEcxRecord;
        break;
    default:
        AssertMsgFailed(("PatchGenLoop: invalid opcode %d\n", opcode));
        return VERR_INVALID_PARAMETER;
    }
    Assert(pPatchAsmRec->offSizeOverride && pPatchAsmRec->offRelJump);

    Log(("PatchGenLoop %d jump %d to %08x offrel=%d\n", opcode, pPatch->nrJumpRecs, pTargetGC, pPatchAsmRec->offRelJump));

    // Generate the patch code
    size = patmPatchGenCode(pVM, pPatch, pPB, pPatchAsmRec, 0, false);

    if (fSizeOverride)
    {
        pPB[pPatchAsmRec->offSizeOverride] = 0x66;  // ecx -> cx or vice versa
    }

    *(RTGCPTR32 *)&pPB[pPatchAsmRec->offRelJump] = 0xDEADBEEF;

    patmPatchAddJump(pVM, pPatch, &pPB[pPatchAsmRec->offRelJump - 1], 1, pTargetGC, opcode);

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

int patmPatchGenRelJump(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pTargetGC, uint32_t opcode, bool fSizeOverride)
{
    uint32_t offset = 0;
    PATCHGEN_PROLOG(pVM, pPatch);

    // internal relative jumps from patch code to patch code; no relocation record required

    Assert(PATMIsPatchGCAddr(pVM, pTargetGC) == false);

    switch (opcode)
    {
    case OP_JO:
        pPB[1] = 0x80;
        break;
    case OP_JNO:
        pPB[1] = 0x81;
        break;
    case OP_JC:
        pPB[1] = 0x82;
        break;
    case OP_JNC:
        pPB[1] = 0x83;
        break;
    case OP_JE:
        pPB[1] = 0x84;
        break;
    case OP_JNE:
        pPB[1] = 0x85;
        break;
    case OP_JBE:
        pPB[1] = 0x86;
        break;
    case OP_JNBE:
        pPB[1] = 0x87;
        break;
    case OP_JS:
        pPB[1] = 0x88;
        break;
    case OP_JNS:
        pPB[1] = 0x89;
        break;
    case OP_JP:
        pPB[1] = 0x8A;
        break;
    case OP_JNP:
        pPB[1] = 0x8B;
        break;
    case OP_JL:
        pPB[1] = 0x8C;
        break;
    case OP_JNL:
        pPB[1] = 0x8D;
        break;
    case OP_JLE:
        pPB[1] = 0x8E;
        break;
    case OP_JNLE:
        pPB[1] = 0x8F;
        break;

    case OP_JMP:
        /* If interrupted here, then jump to the target instruction. Used by PATM.cpp for jumping to known instructions. */
        /* Add lookup record for patch to guest address translation */
        patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pTargetGC, PATM_LOOKUP_PATCH2GUEST);

        pPB[0] = 0xE9;
        break;

    case OP_JECXZ:
    case OP_LOOP:
    case OP_LOOPNE:
    case OP_LOOPE:
        return patmPatchGenLoop(pVM, pPatch, pTargetGC, opcode, fSizeOverride);

    default:
        AssertMsg(0, ("Invalid jump opcode %d\n", opcode));
        return VERR_PATCHING_REFUSED;
    }
    if (opcode != OP_JMP)
    {
         pPB[0] = 0xF;
         offset += 2;
    }
    else offset++;

    *(RTGCPTR32 *)&pPB[offset] = 0xDEADBEEF;

    patmPatchAddJump(pVM, pPatch, pPB, offset, pTargetGC, opcode);

    offset += sizeof(RTGCPTR32);

    PATCHGEN_EPILOG(pPatch, offset);
    return VINF_SUCCESS;
}

/*
 * Rewrite call to dynamic or currently unknown function (on-demand patching of function)
 */
int patmPatchGenCall(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR32 pCurInstrGC, RTGCPTR32 pTargetGC, bool fIndirect)
{
    PATMCALLINFO        callInfo;
    uint32_t            offset;
    uint32_t            i, size;
    int                 rc;

    /** @note Don't check for IF=1 here. The ret instruction will do this.  */
    /** @note It's dangerous to do this for 'normal' patches. the jump target might be inside the generated patch jump. (seen this!) */

    /* 1: Clear PATM interrupt flag on entry. */
    rc = patmPatchGenClearPIF(pVM, pPatch, pCurInstrGC);
    if (rc == VERR_NO_MEMORY)
        return rc;
    AssertRCReturn(rc, rc);

    PATCHGEN_PROLOG(pVM, pPatch);
    /* 2: We must push the target address onto the stack before appending the indirect call code. */

    if (fIndirect)
    {
        Log(("patmPatchGenIndirectCall\n"));
        Assert(pCpu->param1.size == 4);
        Assert(OP_PARM_VTYPE(pCpu->pCurInstr->param1) != OP_PARM_J);

        /* We push it onto the stack here, so the guest's context isn't ruined when this happens to cause
         * a page fault. The assembly code restores the stack afterwards.
         */
        offset = 0;
        /* include prefix byte to make sure we don't use the incorrect selector register. */
        if (pCpu->prefix & PREFIX_SEG)
            pPB[offset++] = DISQuerySegPrefixByte(pCpu);
        pPB[offset++] = 0xFF;              // push r/m32
        pPB[offset++] = MAKE_MODRM(pCpu->ModRM.Bits.Mod, 6 /* group 5 */, pCpu->ModRM.Bits.Rm);
        i = 2;  /* standard offset of modrm bytes */
        if (pCpu->prefix & PREFIX_OPSIZE)
            i++;    //skip operand prefix
        if (pCpu->prefix & PREFIX_SEG)
            i++;    //skip segment prefix

        rc = patmPatchReadBytes(pVM, &pPB[offset], (RTGCPTR32)((RTGCUINTPTR32)pCurInstrGC + i), pCpu->opsize - i);
        AssertRCReturn(rc, rc);
        offset += (pCpu->opsize - i);
    }
    else
    {
        AssertMsg(PATMIsPatchGCAddr(pVM, pTargetGC) == false, ("Target is already a patch address (%VGv)?!?\n", pTargetGC));
        Assert(pTargetGC);
        Assert(OP_PARM_VTYPE(pCpu->pCurInstr->param1) == OP_PARM_J);

        /** @todo wasting memory as the complex search is overkill and we need only one lookup slot... */

        /* Relative call to patch code (patch to patch -> no fixup). */
        Log(("PatchGenCall from %VGv (next=%VGv) to %VGv\n", pCurInstrGC, pCurInstrGC + pCpu->opsize, pTargetGC));

        /* We push it onto the stack here, so the guest's context isn't ruined when this happens to cause
         * a page fault. The assembly code restores the stack afterwards.
         */
        offset = 0;
        pPB[offset++] = 0x68;              // push %Iv
        *(RTGCPTR32 *)&pPB[offset] = pTargetGC;
        offset += sizeof(RTGCPTR32);
    }

    /* align this block properly to make sure the jump table will not be misaligned. */
    size = (RTHCUINTPTR)&pPB[offset] & 3;
    if (size)
        size = 4 - size;

    for (i=0;i<size;i++)
    {
        pPB[offset++] = 0x90;   /* nop */
    }
    PATCHGEN_EPILOG(pPatch, offset);

    /* 3: Generate code to lookup address in our local cache; call hypervisor PATM code if it can't be located. */
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    callInfo.pReturnGC      = pCurInstrGC + pCpu->opsize;
    callInfo.pTargetGC      = (fIndirect) ? 0xDEADBEEF : pTargetGC;
    size = patmPatchGenCode(pVM, pPatch, pPB, (fIndirect) ? &PATMCallIndirectRecord : &PATMCallRecord, 0, false, &callInfo);
    PATCHGEN_EPILOG(pPatch, size);

    /* Need to set PATM_INTERRUPTFLAG after the patched ret returns here. */
    rc = patmPatchGenSetPIF(pVM, pPatch, pCurInstrGC);
    if (rc == VERR_NO_MEMORY)
        return rc;
    AssertRCReturn(rc, rc);

    STAM_COUNTER_INC(&pVM->patm.s.StatGenCall);
    return VINF_SUCCESS;
}

/**
 * Generate indirect jump to unknown destination
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pCpu        Disassembly state
 * @param   pCurInstrGC Current instruction address
 */
int patmPatchGenJump(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR32 pCurInstrGC)
{
    PATMCALLINFO        callInfo;
    uint32_t            offset;
    uint32_t            i, size;
    int                 rc;

    /* 1: Clear PATM interrupt flag on entry. */
    rc = patmPatchGenClearPIF(pVM, pPatch, pCurInstrGC);
    if (rc == VERR_NO_MEMORY)
        return rc;
    AssertRCReturn(rc, rc);

    PATCHGEN_PROLOG(pVM, pPatch);
    /* 2: We must push the target address onto the stack before appending the indirect call code. */

    Log(("patmPatchGenIndirectJump\n"));
    Assert(pCpu->param1.size == 4);
    Assert(OP_PARM_VTYPE(pCpu->pCurInstr->param1) != OP_PARM_J);

    /* We push it onto the stack here, so the guest's context isn't ruined when this happens to cause
     * a page fault. The assembly code restores the stack afterwards.
     */
    offset = 0;
    /* include prefix byte to make sure we don't use the incorrect selector register. */
    if (pCpu->prefix & PREFIX_SEG)
        pPB[offset++] = DISQuerySegPrefixByte(pCpu);

    pPB[offset++] = 0xFF;              // push r/m32
    pPB[offset++] = MAKE_MODRM(pCpu->ModRM.Bits.Mod, 6 /* group 5 */, pCpu->ModRM.Bits.Rm);
    i = 2;  /* standard offset of modrm bytes */
    if (pCpu->prefix & PREFIX_OPSIZE)
        i++;    //skip operand prefix
    if (pCpu->prefix & PREFIX_SEG)
        i++;    //skip segment prefix

    rc = patmPatchReadBytes(pVM, &pPB[offset], (RTGCPTR32)((RTGCUINTPTR32)pCurInstrGC + i), pCpu->opsize - i);
    AssertRCReturn(rc, rc);
    offset += (pCpu->opsize - i);

    /* align this block properly to make sure the jump table will not be misaligned. */
    size = (RTHCUINTPTR)&pPB[offset] & 3;
    if (size)
        size = 4 - size;

    for (i=0;i<size;i++)
    {
        pPB[offset++] = 0x90;   /* nop */
    }
    PATCHGEN_EPILOG(pPatch, offset);

    /* 3: Generate code to lookup address in our local cache; call hypervisor PATM code if it can't be located. */
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    callInfo.pReturnGC      = pCurInstrGC + pCpu->opsize;
    callInfo.pTargetGC      = 0xDEADBEEF;
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMJumpIndirectRecord, 0, false, &callInfo);
    PATCHGEN_EPILOG(pPatch, size);

    STAM_COUNTER_INC(&pVM->patm.s.StatGenJump);
    return VINF_SUCCESS;
}

/**
 * Generate return instruction
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pCpu        Disassembly struct
 * @param   pCurInstrGC Current instruction pointer
 *
 */
int patmPatchGenRet(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pCurInstrGC)
{
    int size = 0, rc;
    RTGCPTR32 pPatchRetInstrGC;

    /* Remember start of this patch for below. */
    pPatchRetInstrGC = PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset;

    Log(("patmPatchGenRet %VGv\n", pCurInstrGC));

    /** @note optimization: multiple identical ret instruction in a single patch can share a single patched ret. */
    if (    pPatch->pTempInfo->pPatchRetInstrGC
        &&  pPatch->pTempInfo->uPatchRetParam1 == (uint32_t)pCpu->param1.parval) /* nr of bytes popped off the stack should be identical of course! */
    {
        Assert(pCpu->pCurInstr->opcode == OP_RETN);
        STAM_COUNTER_INC(&pVM->patm.s.StatGenRetReused);

        return patmPatchGenPatchJump(pVM, pPatch, pCurInstrGC, pPatch->pTempInfo->pPatchRetInstrGC);
    }

    /* Jump back to the original instruction if IF is set again. */
    Assert(!PATMFindActivePatchByEntrypoint(pVM, pCurInstrGC));
    rc = patmPatchGenCheckIF(pVM, pPatch, pCurInstrGC);
    AssertRCReturn(rc, rc);

    /* align this block properly to make sure the jump table will not be misaligned. */
    PATCHGEN_PROLOG(pVM, pPatch);
    size = (RTHCUINTPTR)pPB & 3;
    if (size)
        size = 4 - size;

    for (int i=0;i<size;i++)
        pPB[i] = 0x90;   /* nop */
    PATCHGEN_EPILOG(pPatch, size);

    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMRetRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    STAM_COUNTER_INC(&pVM->patm.s.StatGenRet);
    /* Duplicate the ret or ret n instruction; it will use the PATM return address */
    rc = patmPatchGenDuplicate(pVM, pPatch, pCpu, pCurInstrGC);

    if (rc == VINF_SUCCESS)
    {
        pPatch->pTempInfo->pPatchRetInstrGC = pPatchRetInstrGC;
        pPatch->pTempInfo->uPatchRetParam1  = pCpu->param1.parval;
    }
    return rc;
}

/**
 * Generate all global patm functions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 *
 */
int patmPatchGenGlobalFunctions(PVM pVM, PPATCHINFO pPatch)
{
    int size = 0;

    pVM->patm.s.pfnHelperCallGC = PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset;
    PATCHGEN_PROLOG(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMLookupAndCallRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    /* Round to next 8 byte boundary. */
    pPatch->uCurPatchOffset = RT_ALIGN_32(pPatch->uCurPatchOffset, 8);

    pVM->patm.s.pfnHelperRetGC = PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset;
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMRetFunctionRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    /* Round to next 8 byte boundary. */
    pPatch->uCurPatchOffset = RT_ALIGN_32(pPatch->uCurPatchOffset, 8);

    pVM->patm.s.pfnHelperJumpGC = PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset;
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMLookupAndJumpRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    /* Round to next 8 byte boundary. */
    pPatch->uCurPatchOffset = RT_ALIGN_32(pPatch->uCurPatchOffset, 8);

    pVM->patm.s.pfnHelperIretGC = PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset;
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMIretFunctionRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    Log(("pfnHelperCallGC %VGv\n", pVM->patm.s.pfnHelperCallGC));
    Log(("pfnHelperRetGC  %VGv\n", pVM->patm.s.pfnHelperRetGC));
    Log(("pfnHelperJumpGC %VGv\n", pVM->patm.s.pfnHelperJumpGC));
    Log(("pfnHelperIretGC  %VGv\n", pVM->patm.s.pfnHelperIretGC));

    return VINF_SUCCESS;
}

/**
 * Generate illegal instruction (int 3)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 *
 */
int patmPatchGenIllegalInstr(PVM pVM, PPATCHINFO pPatch)
{
    PATCHGEN_PROLOG(pVM, pPatch);

    pPB[0] = 0xCC;

    PATCHGEN_EPILOG(pPatch, 1);
    return VINF_SUCCESS;
}

/**
 * Check virtual IF flag and jump back to original guest code if set
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pCurInstrGC Guest context pointer to the current instruction
 *
 */
int patmPatchGenCheckIF(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pCurInstrGC)
{
    uint32_t size;

    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pCurInstrGC, PATM_LOOKUP_PATCH2GUEST);

    /* Generate code to check for IF=1 before executing the call to the duplicated function. */
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMCheckIFRecord, pCurInstrGC, true);

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

/**
 * Set PATM interrupt flag
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pInstrGC    Corresponding guest instruction
 *
 */
int patmPatchGenSetPIF(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pInstrGC)
{
    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pInstrGC, PATM_LOOKUP_PATCH2GUEST);

    int size = patmPatchGenCode(pVM, pPatch, pPB, &PATMSetPIFRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

/**
 * Clear PATM interrupt flag
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pInstrGC    Corresponding guest instruction
 *
 */
int patmPatchGenClearPIF(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pInstrGC)
{
    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pInstrGC, PATM_LOOKUP_PATCH2GUEST);

    int size = patmPatchGenCode(pVM, pPatch, pPB, &PATMClearPIFRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}


/**
 * Clear PATM inhibit irq flag
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure
 * @param   pNextInstrGC    Next guest instruction
 */
int patmPatchGenClearInhibitIRQ(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pNextInstrGC)
{
    int          size;
    PATMCALLINFO callInfo;

    PATCHGEN_PROLOG(pVM, pPatch);

    Assert((pPatch->flags & (PATMFL_GENERATE_JUMPTOGUEST|PATMFL_DUPLICATE_FUNCTION)) != (PATMFL_GENERATE_JUMPTOGUEST|PATMFL_DUPLICATE_FUNCTION));

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pNextInstrGC, PATM_LOOKUP_PATCH2GUEST);

    callInfo.pNextInstrGC = pNextInstrGC;

    if (pPatch->flags & PATMFL_DUPLICATE_FUNCTION)
        size = patmPatchGenCode(pVM, pPatch, pPB, &PATMClearInhibitIRQContIF0Record, 0, false, &callInfo);
    else
        size = patmPatchGenCode(pVM, pPatch, pPB, &PATMClearInhibitIRQFaultIF0Record, 0, false, &callInfo);

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

/**
 * Generate an interrupt handler entrypoint
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pIntHandlerGC IDT handler address
 *
 ** @todo must check if virtual IF is already cleared on entry!!!!!!!!!!!!!!!!!!!!!!!
 */
int patmPatchGenIntEntry(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pIntHandlerGC)
{
    uint32_t size;
    int rc = VINF_SUCCESS;

    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pIntHandlerGC, PATM_LOOKUP_PATCH2GUEST);

    /* Generate entrypoint for the interrupt handler (correcting CS in the interrupt stack frame) */
    size = patmPatchGenCode(pVM, pPatch, pPB,
                            (pPatch->flags & PATMFL_INTHANDLER_WITH_ERRORCODE) ? &PATMIntEntryRecordErrorCode : &PATMIntEntryRecord,
                            0, false);

    PATCHGEN_EPILOG(pPatch, size);

    // Interrupt gates set IF to 0
    rc = patmPatchGenCli(pVM, pPatch);
    AssertRCReturn(rc, rc);

    return rc;
}

/**
 * Generate a trap handler entrypoint
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pTrapHandlerGC  IDT handler address
 */
int patmPatchGenTrapEntry(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pTrapHandlerGC)
{
    uint32_t size;

    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pTrapHandlerGC, PATM_LOOKUP_PATCH2GUEST);

    /* Generate entrypoint for the trap handler (correcting CS in the interrupt stack frame) */
    size = patmPatchGenCode(pVM, pPatch, pPB,
                            (pPatch->flags & PATMFL_TRAPHANDLER_WITH_ERRORCODE) ? &PATMTrapEntryRecordErrorCode : &PATMTrapEntryRecord,
                            pTrapHandlerGC, true);
    PATCHGEN_EPILOG(pPatch, size);

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_STATISTICS
int patmPatchGenStats(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pInstrGC)
{
    uint32_t size;

    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for stats code -> guest handler. */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pInstrGC, PATM_LOOKUP_PATCH2GUEST);

    /* Generate code to keep calling statistics for this patch */
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMStatsRecord, pInstrGC, false);
    PATCHGEN_EPILOG(pPatch, size);

    return VINF_SUCCESS;
}
#endif

/**
 * Debug register moves to or from general purpose registers
 * mov GPR, DRx
 * mov DRx, GPR
 *
 * @todo: if we ever want to support hardware debug registers natively, then
 *        this will need to be changed!
 */
int patmPatchGenMovDebug(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu)
{
    int rc = VINF_SUCCESS;
    int reg, mod, rm, dbgreg;
    uint32_t offset;

    PATCHGEN_PROLOG(pVM, pPatch);

    mod = 0;            //effective address (only)
    rm  = 5;            //disp32
    if (pCpu->pCurInstr->param1 == OP_PARM_Dd)
    {
        Assert(0);  // You not come here. Illegal!

        // mov DRx, GPR
        pPB[0] = 0x89;      //mov disp32, GPR
        Assert(pCpu->param1.flags & USE_REG_DBG);
        Assert(pCpu->param2.flags & USE_REG_GEN32);

        dbgreg = pCpu->param1.base.reg_dbg;
        reg    = pCpu->param2.base.reg_gen;
    }
    else
    {
        // mov GPR, DRx
        Assert(pCpu->param1.flags & USE_REG_GEN32);
        Assert(pCpu->param2.flags & USE_REG_DBG);

        pPB[0] = 0x8B;      // mov GPR, disp32
        reg    = pCpu->param1.base.reg_gen;
        dbgreg = pCpu->param2.base.reg_dbg;
    }

    pPB[1] = MAKE_MODRM(mod, reg, rm);

    /// @todo: make this an array in the context structure
    switch (dbgreg)
    {
    case USE_REG_DR0:
        offset = RT_OFFSETOF(CPUMCTX, dr0);
        break;
    case USE_REG_DR1:
        offset = RT_OFFSETOF(CPUMCTX, dr1);
        break;
    case USE_REG_DR2:
        offset = RT_OFFSETOF(CPUMCTX, dr2);
        break;
    case USE_REG_DR3:
        offset = RT_OFFSETOF(CPUMCTX, dr3);
        break;
    case USE_REG_DR4:
        offset = RT_OFFSETOF(CPUMCTX, dr4);
        break;
    case USE_REG_DR5:
        offset = RT_OFFSETOF(CPUMCTX, dr5);
        break;
    case USE_REG_DR6:
        offset = RT_OFFSETOF(CPUMCTX, dr6);
        break;
    case USE_REG_DR7:
        offset = RT_OFFSETOF(CPUMCTX, dr7);
        break;
    default: /* Shut up compiler warning. */
        AssertFailed();
        offset = 0;
        break;
    }
    *(RTGCPTR32 *)&pPB[2] = pVM->patm.s.pCPUMCtxGC + offset;
    patmPatchAddReloc32(pVM, pPatch, &pPB[2], FIXUP_ABSOLUTE);

    PATCHGEN_EPILOG(pPatch, 2 + sizeof(RTGCPTR32));
    return rc;
}

/*
 * Control register moves to or from general purpose registers
 * mov GPR, CRx
 * mov CRx, GPR
 */
int patmPatchGenMovControl(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu)
{
    int rc = VINF_SUCCESS;
    int reg, mod, rm, ctrlreg;
    uint32_t offset;

    PATCHGEN_PROLOG(pVM, pPatch);

    mod = 0;            //effective address (only)
    rm  = 5;            //disp32
    if (pCpu->pCurInstr->param1 == OP_PARM_Cd)
    {
        Assert(0);  // You not come here. Illegal!

        // mov CRx, GPR
        pPB[0] = 0x89;      //mov disp32, GPR
        ctrlreg = pCpu->param1.base.reg_ctrl;
        reg     = pCpu->param2.base.reg_gen;
        Assert(pCpu->param1.flags & USE_REG_CR);
        Assert(pCpu->param2.flags & USE_REG_GEN32);
    }
    else
    {
        // mov GPR, DRx
        Assert(pCpu->param1.flags & USE_REG_GEN32);
        Assert(pCpu->param2.flags & USE_REG_CR);

        pPB[0]  = 0x8B;      // mov GPR, disp32
        reg     = pCpu->param1.base.reg_gen;
        ctrlreg = pCpu->param2.base.reg_ctrl;
    }

    pPB[1] = MAKE_MODRM(mod, reg, rm);

    /// @todo: make this an array in the context structure
    switch (ctrlreg)
    {
    case USE_REG_CR0:
        offset = RT_OFFSETOF(CPUMCTX, cr0);
        break;
    case USE_REG_CR2:
        offset = RT_OFFSETOF(CPUMCTX, cr2);
        break;
    case USE_REG_CR3:
        offset = RT_OFFSETOF(CPUMCTX, cr3);
        break;
    case USE_REG_CR4:
        offset = RT_OFFSETOF(CPUMCTX, cr4);
        break;
    default: /* Shut up compiler warning. */
        AssertFailed();
        offset = 0;
        break;
    }
    *(RTGCPTR32 *)&pPB[2] = pVM->patm.s.pCPUMCtxGC + offset;
    patmPatchAddReloc32(pVM, pPatch, &pPB[2], FIXUP_ABSOLUTE);

    PATCHGEN_EPILOG(pPatch, 2 + sizeof(RTGCPTR32));
    return rc;
}

/*
 * mov GPR, SS
 */
int patmPatchGenMovFromSS(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR32 pCurInstrGC)
{
    uint32_t size, offset;

    Log(("patmPatchGenMovFromSS %VGv\n", pCurInstrGC));

    Assert(pPatch->flags & PATMFL_CODE32);

    PATCHGEN_PROLOG(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMClearPIFRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    /* push ss */
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    offset = 0;
    if (pCpu->prefix & PREFIX_OPSIZE)
        pPB[offset++] = 0x66;       /* size override -> 16 bits push */
    pPB[offset++] = 0x16;
    PATCHGEN_EPILOG(pPatch, offset);

    /* checks and corrects RPL of pushed ss*/
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMMovFromSSRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    /* pop general purpose register */
    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    offset = 0;
    if (pCpu->prefix & PREFIX_OPSIZE)
        pPB[offset++] = 0x66; /* size override -> 16 bits pop */
    pPB[offset++] = 0x58 + pCpu->param1.base.reg_gen;
    PATCHGEN_EPILOG(pPatch, offset);


    PATCHGEN_PROLOG_NODEF(pVM, pPatch);
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMSetPIFRecord, 0, false);
    PATCHGEN_EPILOG(pPatch, size);

    return VINF_SUCCESS;
}


/**
 * Generate an sldt or str patch instruction
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pCpu        Disassembly state
 * @param   pCurInstrGC Guest instruction address
 */
int patmPatchGenSldtStr(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR32 pCurInstrGC)
{
    // sldt %Ew
    int rc = VINF_SUCCESS;
    uint32_t offset = 0;
    uint32_t i;

    /** @todo segment prefix (untested) */
    Assert(pCpu->prefix == PREFIX_NONE || pCpu->prefix == PREFIX_OPSIZE);

    PATCHGEN_PROLOG(pVM, pPatch);

    if (pCpu->param1.flags == USE_REG_GEN32 || pCpu->param1.flags == USE_REG_GEN16)
    {
        /* Register operand */
        // 8B 15 [32 bits addr]   mov edx, CPUMCTX.tr/ldtr

        if (pCpu->prefix == PREFIX_OPSIZE)
            pPB[offset++] = 0x66;

        pPB[offset++] = 0x8B;              // mov       destreg, CPUMCTX.tr/ldtr
        /* Modify REG part according to destination of original instruction */
        pPB[offset++] = MAKE_MODRM(0, pCpu->param1.base.reg_gen, 5);
        if (pCpu->pCurInstr->opcode == OP_STR)
        {
            *(RTGCPTR32 *)&pPB[offset] = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, tr);
        }
        else
        {
            *(RTGCPTR32 *)&pPB[offset] = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, ldtr);
        }
        patmPatchAddReloc32(pVM, pPatch, &pPB[offset], FIXUP_ABSOLUTE);
        offset += sizeof(RTGCPTR32);
    }
    else
    {
        /* Memory operand */
        //50                   push        eax
        //52                   push        edx
        //8D 15 48 7C 42 00    lea         edx, dword ptr [dest]
        //66 A1 48 7C 42 00    mov         ax, CPUMCTX.tr/ldtr
        //66 89 02             mov         word ptr [edx],ax
        //5A                   pop         edx
        //58                   pop         eax

        pPB[offset++] = 0x50;              // push      eax
        pPB[offset++] = 0x52;              // push      edx

        if (pCpu->prefix == PREFIX_SEG)
        {
            pPB[offset++] = DISQuerySegPrefixByte(pCpu);
        }
        pPB[offset++] = 0x8D;              // lea       edx, dword ptr [dest]
        // duplicate and modify modrm byte and additional bytes if present (e.g. direct address)
        pPB[offset++] = MAKE_MODRM(pCpu->ModRM.Bits.Mod, USE_REG_EDX, pCpu->ModRM.Bits.Rm);

        i = 3;  /* standard offset of modrm bytes */
        if (pCpu->prefix == PREFIX_OPSIZE)
            i++;    //skip operand prefix
        if (pCpu->prefix == PREFIX_SEG)
            i++;    //skip segment prefix

        rc = patmPatchReadBytes(pVM, &pPB[offset], (RTGCPTR32)((RTGCUINTPTR32)pCurInstrGC + i), pCpu->opsize - i);
        AssertRCReturn(rc, rc);
        offset += (pCpu->opsize - i);

        pPB[offset++] = 0x66;              // mov       ax, CPUMCTX.tr/ldtr
        pPB[offset++] = 0xA1;
        if (pCpu->pCurInstr->opcode == OP_STR)
        {
            *(RTGCPTR32 *)&pPB[offset] = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, tr);
        }
        else
        {
            *(RTGCPTR32 *)&pPB[offset] = pVM->patm.s.pCPUMCtxGC + RT_OFFSETOF(CPUMCTX, ldtr);
        }
        patmPatchAddReloc32(pVM, pPatch, &pPB[offset], FIXUP_ABSOLUTE);
        offset += sizeof(RTGCPTR32);

        pPB[offset++] = 0x66;              // mov       word ptr [edx],ax
        pPB[offset++] = 0x89;
        pPB[offset++] = 0x02;

        pPB[offset++] = 0x5A;              // pop       edx
        pPB[offset++] = 0x58;              // pop       eax
    }

    PATCHGEN_EPILOG(pPatch, offset);

    return rc;
}

/**
 * Generate an sgdt or sidt patch instruction
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pCpu        Disassembly state
 * @param   pCurInstrGC Guest instruction address
 */
int patmPatchGenSxDT(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR32 pCurInstrGC)
{
    int rc = VINF_SUCCESS;
    uint32_t offset = 0, offset_base, offset_limit;
    uint32_t i;

    /* @todo segment prefix (untested) */
    Assert(pCpu->prefix == PREFIX_NONE);

    // sgdt %Ms
    // sidt %Ms

    switch (pCpu->pCurInstr->opcode)
    {
    case OP_SGDT:
        offset_base  = RT_OFFSETOF(CPUMCTX, gdtr.pGdt);
        offset_limit = RT_OFFSETOF(CPUMCTX, gdtr.cbGdt);
        break;

    case OP_SIDT:
        offset_base  = RT_OFFSETOF(CPUMCTX, idtr.pIdt);
        offset_limit = RT_OFFSETOF(CPUMCTX, idtr.cbIdt);
        break;

    default:
        return VERR_INVALID_PARAMETER;
    }

//50                   push        eax
//52                   push        edx
//8D 15 48 7C 42 00    lea         edx, dword ptr [dest]
//66 A1 48 7C 42 00    mov         ax, CPUMCTX.gdtr.limit
//66 89 02             mov         word ptr [edx],ax
//A1 48 7C 42 00       mov         eax, CPUMCTX.gdtr.base
//89 42 02             mov         dword ptr [edx+2],eax
//5A                   pop         edx
//58                   pop         eax

    PATCHGEN_PROLOG(pVM, pPatch);
    pPB[offset++] = 0x50;              // push      eax
    pPB[offset++] = 0x52;              // push      edx

    if (pCpu->prefix == PREFIX_SEG)
    {
        pPB[offset++] = DISQuerySegPrefixByte(pCpu);
    }
    pPB[offset++] = 0x8D;              // lea       edx, dword ptr [dest]
    // duplicate and modify modrm byte and additional bytes if present (e.g. direct address)
    pPB[offset++] = MAKE_MODRM(pCpu->ModRM.Bits.Mod, USE_REG_EDX, pCpu->ModRM.Bits.Rm);

    i = 3;  /* standard offset of modrm bytes */
    if (pCpu->prefix == PREFIX_OPSIZE)
        i++;    //skip operand prefix
    if (pCpu->prefix == PREFIX_SEG)
        i++;    //skip segment prefix
    rc = patmPatchReadBytes(pVM, &pPB[offset], (RTGCPTR32)((RTGCUINTPTR32)pCurInstrGC + i), pCpu->opsize - i);
    AssertRCReturn(rc, rc);
    offset += (pCpu->opsize - i);

    pPB[offset++] = 0x66;              // mov       ax, CPUMCTX.gdtr.limit
    pPB[offset++] = 0xA1;
    *(RTGCPTR32 *)&pPB[offset] = pVM->patm.s.pCPUMCtxGC + offset_limit;
    patmPatchAddReloc32(pVM, pPatch, &pPB[offset], FIXUP_ABSOLUTE);
    offset += sizeof(RTGCPTR32);

    pPB[offset++] = 0x66;              // mov       word ptr [edx],ax
    pPB[offset++] = 0x89;
    pPB[offset++] = 0x02;

    pPB[offset++] = 0xA1;              // mov       eax, CPUMCTX.gdtr.base
    *(RTGCPTR32 *)&pPB[offset] = pVM->patm.s.pCPUMCtxGC + offset_base;
    patmPatchAddReloc32(pVM, pPatch, &pPB[offset], FIXUP_ABSOLUTE);
    offset += sizeof(RTGCPTR32);

    pPB[offset++] = 0x89;              // mov       dword ptr [edx+2],eax
    pPB[offset++] = 0x42;
    pPB[offset++] = 0x02;

    pPB[offset++] = 0x5A;              // pop       edx
    pPB[offset++] = 0x58;              // pop       eax

    PATCHGEN_EPILOG(pPatch, offset);

    return rc;
}

/**
 * Generate a cpuid patch instruction
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pCurInstrGC Guest instruction address
 */
int patmPatchGenCpuid(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pCurInstrGC)
{
    uint32_t size;
    PATCHGEN_PROLOG(pVM, pPatch);

    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMCpuidRecord, 0, false);

    PATCHGEN_EPILOG(pPatch, size);
    return VINF_SUCCESS;
}

/**
 * Generate the jump from guest to patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pTargetGC   Guest target jump
 * @param   fClearInhibitIRQs   Clear inhibit irq flag
 */
int patmPatchGenJumpToGuest(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pReturnAddrGC, bool fClearInhibitIRQs)
{
    int rc = VINF_SUCCESS;
    uint32_t size;

    if (fClearInhibitIRQs)
    {
        rc = patmPatchGenClearInhibitIRQ(pVM, pPatch, pReturnAddrGC);
        if (rc == VERR_NO_MEMORY)
            return rc;
        AssertRCReturn(rc, rc);
    }

    PATCHGEN_PROLOG(pVM, pPatch);

    /* Add lookup record for patch to guest address translation */
    patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pReturnAddrGC, PATM_LOOKUP_PATCH2GUEST);

    /* Generate code to jump to guest code if IF=1, else fault. */
    size = patmPatchGenCode(pVM, pPatch, pPB, &PATMJumpToGuest_IF1Record, pReturnAddrGC, true);
    PATCHGEN_EPILOG(pPatch, size);

    return rc;
}

/*
 * Relative jump from patch code to patch code (no fixup required)
 */
int patmPatchGenPatchJump(PVM pVM, PPATCHINFO pPatch, RTGCPTR32 pCurInstrGC, RCPTRTYPE(uint8_t *) pPatchAddrGC, bool fAddLookupRecord)
{
    int32_t displ;
    int     rc = VINF_SUCCESS;

    Assert(PATMIsPatchGCAddr(pVM, pPatchAddrGC));
    PATCHGEN_PROLOG(pVM, pPatch);

    if (fAddLookupRecord)
    {
        /* Add lookup record for patch to guest address translation */
        patmr3AddP2GLookupRecord(pVM, pPatch, pPB, pCurInstrGC, PATM_LOOKUP_PATCH2GUEST);
    }

    pPB[0] = 0xE9;  //JMP

    displ = pPatchAddrGC - (PATCHCODE_PTR_GC(pPatch) + pPatch->uCurPatchOffset + SIZEOF_NEARJUMP32);

     *(uint32_t *)&pPB[1] = displ;

    PATCHGEN_EPILOG(pPatch, SIZEOF_NEARJUMP32);

    return rc;
}
