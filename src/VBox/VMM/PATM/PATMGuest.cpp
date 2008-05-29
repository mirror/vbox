/* $Id$ */
/** @file
 * PATMGuest - Guest OS Patching Manager (non-generic)
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
#include <iprt/string.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>

/*
 * ntdll!KiFastSystemCall:
 * 7c90eb8b 8bd4             mov     edx,esp
 * 7c90eb8d 0f34             sysenter
 * 7c90eb8f 90               nop
 * 7c90eb90 90               nop
 * 7c90eb91 90               nop
 * 7c90eb92 90               nop
 * 7c90eb93 90               nop
 * ntdll!KiFastSystemCallRet:
 * 7c90eb94 c3               ret
 *
 * ntdll!KiIntSystemCall:
 * 7c90eba5 8d542408         lea     edx,[esp+0x8]
 * 7c90eba9 cd2e             int     2e
 * 7c90ebab c3               ret
 *
 */
static uint8_t uFnKiFastSystemCall[7] = {0x8b, 0xd4, 0x0f, 0x34, 0x90, 0x90, 0x90};
static uint8_t uFnKiIntSystemCall[7]  = {0x8d, 0x54, 0x24, 0x08, 0xcd, 0x2e, 0xc3};

/*
 * OpenBSD 3.7 & 3.8:
 *
 * D0101B6D:  push CS                       [0E]
 * D0101B6E:  push ESI                      [56]
 * D0101B6F:  cli                           [FA]
 */
static uint8_t uFnOpenBSDHandlerPrefix1[3] = { 0x0E, 0x56, 0xFA };
/*
 * OpenBSD 3.9 & 4.0
 *
 * D0101BD1:  push CS                       [0E]
 * D0101BD2:  push ESI                      [56]
 * D0101BD3:  push 0x00                     [6A 00]
 * D0101BD4:  push 0x03                     [6A 03]
 */
static uint8_t uFnOpenBSDHandlerPrefix2[6] = { 0x0E, 0x56, 0x6A, 0x00, 0x6A, 0x03 };


/**
 * Check Windows XP sysenter heuristics and install patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    GC Instruction pointer for sysenter
 * @param   pPatchRec   Patch structure
 *
 */
int PATMPatchSysenterXP(PVM pVM, RTGCPTR32 pInstrGC, PPATMPATCHREC pPatchRec)
{
    PPATCHINFO pPatch = &pPatchRec->patch;
    uint8_t   uTemp[16];
    RTGCPTR32   lpfnKiFastSystemCall, lpfnKiIntSystemCall = 0; /* (initializing it to shut up warning.) */
    int       rc, i;

    Assert(sizeof(uTemp) > sizeof(uFnKiIntSystemCall));
    Assert(sizeof(uTemp) > sizeof(uFnKiFastSystemCall));

    /* Guest OS specific patch; check heuristics first */

    /* check the epilog of KiFastSystemCall */
    lpfnKiFastSystemCall = pInstrGC - 2;
    rc = PGMPhysReadGCPtr(pVM, uTemp, lpfnKiFastSystemCall, sizeof(uFnKiFastSystemCall));
    if (    VBOX_FAILURE(rc)
        ||  memcmp(uFnKiFastSystemCall, uTemp, sizeof(uFnKiFastSystemCall)))
    {
        return VERR_PATCHING_REFUSED;
    }

    /* Now search for KiIntSystemCall */
    for (i=0;i<64;i++)
    {
        rc = PGMPhysReadGCPtr(pVM, uTemp, pInstrGC + i, sizeof(uFnKiIntSystemCall));
        if(VBOX_FAILURE(rc))
        {
            break;
        }
        if(!memcmp(uFnKiIntSystemCall, uTemp, sizeof(uFnKiIntSystemCall)))
        {
            lpfnKiIntSystemCall = pInstrGC + i;
            /* Found it! */
            break;
        }
    }
    if (i == 64)
    {
        Log(("KiIntSystemCall not found!!\n"));
        return VERR_PATCHING_REFUSED;
    }

    if (PAGE_ADDRESS(lpfnKiFastSystemCall) != PAGE_ADDRESS(lpfnKiIntSystemCall))
    {
        Log(("KiFastSystemCall and KiIntSystemCall not in the same page!!\n"));
        return VERR_PATCHING_REFUSED;
    }

    // make a copy of the guest code bytes that will be overwritten
    rc = PGMPhysReadGCPtr(pVM, pPatch->aPrivInstr, pPatch->pPrivInstrGC, SIZEOF_NEARJUMP32);
    AssertRC(rc);

    /* Now we simply jump from the fast version to the 'old and slow' system call */
    uTemp[0] = 0xE9;
    *(RTGCPTR32 *)&uTemp[1] = lpfnKiIntSystemCall - (pInstrGC + SIZEOF_NEARJUMP32);
    rc = PGMPhysWriteGCPtrDirty(pVM, pInstrGC, uTemp, SIZEOF_NEARJUMP32);
    if (VBOX_FAILURE(rc))
    {
        Log(("MMR3PhysWriteGCVirt failed with rc=%d!!\n", rc));
        return VERR_PATCHING_REFUSED;
    }

#ifdef DEBUG
    Log(("Sysenter Patch code ----------------------------------------------------------\n"));
    patmr3DisasmCodeStream(pVM, pInstrGC, pInstrGC, patmr3DisasmCallback, pPatch);
    Log(("Sysenter Patch code ends -----------------------------------------------------\n"));
#endif

    pPatch->uState = PATCH_ENABLED;
    return VINF_SUCCESS;
}

/**
 * Patch OpenBSD interrupt handler prefix
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on
 * @param   pCpu        Disassembly state of instruction.
 * @param   pInstrGC    GC Instruction pointer for instruction
 * @param   pInstrHC    GC Instruction pointer for instruction
 * @param   pPatchRec   Patch structure
 *
 */
int PATMPatchOpenBSDHandlerPrefix(PVM pVM, PDISCPUSTATE pCpu, RTGCPTR32 pInstrGC, uint8_t *pInstrHC, PPATMPATCHREC pPatchRec)
{
    uint8_t   uTemp[16];
    int       rc;

    Assert(sizeof(uTemp) > RT_MAX(sizeof(uFnOpenBSDHandlerPrefix1), sizeof(uFnOpenBSDHandlerPrefix2)));

    /* Guest OS specific patch; check heuristics first */

    rc = PGMPhysReadGCPtr(pVM, uTemp, pInstrGC, RT_MAX(sizeof(uFnOpenBSDHandlerPrefix1), sizeof(uFnOpenBSDHandlerPrefix2)));
    if (    VBOX_FAILURE(rc) 
        || (    memcmp(uFnOpenBSDHandlerPrefix1, uTemp, sizeof(uFnOpenBSDHandlerPrefix1))
            &&  memcmp(uFnOpenBSDHandlerPrefix2, uTemp, sizeof(uFnOpenBSDHandlerPrefix2))))
    {
        return VERR_PATCHING_REFUSED;
    }
    /* Found it; patch the push cs */
    pPatchRec->patch.flags &= ~(PATMFL_GUEST_SPECIFIC);  /* prevent a breakpoint from being triggered */
    return PATMR3PatchInstrInt3(pVM, pInstrGC, pInstrHC, pCpu, &pPatchRec->patch);
}

/**
 * Install guest OS specific patch
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on
 * @param   pCpu        Disassembly state of instruction.
 * @param   pInstrGC    GC Instruction pointer for instruction
 * @param   pInstrHC    GC Instruction pointer for instruction
 * @param   pCallerGC   GC address of caller; CODE32_UNKNOWN_CALLER if unknown
 * @param   pPatchRec   Patch structure
 *
 */
int PATMInstallGuestSpecificPatch(PVM pVM, PDISCPUSTATE pCpu, RTGCPTR32 pInstrGC, uint8_t *pInstrHC, PPATMPATCHREC pPatchRec)
{
    int rc;

    /** @todo might have to check if the patch crosses a page boundary. Currently not necessary, but that might change in the future!! */
    switch (pCpu->pCurInstr->opcode)
    {
    case OP_SYSENTER:
        pPatchRec->patch.flags |= PATMFL_SYSENTER_XP | PATMFL_USER_MODE | PATMFL_GUEST_SPECIFIC;

        rc = PATMPatchSysenterXP(pVM, pInstrGC, pPatchRec);
        if (VBOX_FAILURE(rc))
        {
            return VERR_PATCHING_REFUSED;
        }
        return VINF_SUCCESS;

    case OP_PUSH:
        /* OpenBSD guest specific patch for the following code block:
         *
         *  pushf
         *  push cs     <- dangerous because of DPL 0 tests
         *  push esi
         *  cli
         */
        if (pCpu->pCurInstr->param1 == OP_PARM_REG_CS)
            return PATMPatchOpenBSDHandlerPrefix(pVM, pCpu, pInstrGC, pInstrHC, pPatchRec);

        return VERR_PATCHING_REFUSED;

    default:
        AssertMsgFailed(("PATMInstallGuestSpecificPatch: unknown opcode %d\n", pCpu->pCurInstr->opcode));
        return VERR_PATCHING_REFUSED;
    }
    return VERR_PATCHING_REFUSED;
}

