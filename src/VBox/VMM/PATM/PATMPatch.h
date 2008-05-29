/* $Id$ */
/** @file
 * PATMPatch - Internal header file.
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
#ifndef ___PATMPATCH_H
#define ___PATMPATCH_H

int patmPatchAddReloc32(PVM pVM, PPATCHINFO pPatch, uint8_t *pRelocHC, uint32_t uType, RTGCPTR pSource = 0, RTGCPTR pDest = 0);
int patmPatchAddJump(PVM pVM, PPATCHINFO pPatch, uint8_t *pJumpHC, uint32_t offset, RTGCPTR pTargetGC, uint32_t opcode);

int patmPatchGenCpuid(PVM pVM, PPATCHINFO pPatch, RTGCPTR pCurInstrGC);
int patmPatchGenSxDT(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR pCurInstrGC);
int patmPatchGenSldtStr(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR pCurInstrGC);
int patmPatchGenMovControl(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu);
int patmPatchGenMovDebug(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu);
int patmPatchGenMovFromSS(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR pCurInstrGC);
int patmPatchGenRelJump(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pTargetGC, uint32_t opcode, bool fSizeOverride);
int patmPatchGenLoop(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pTargetGC, uint32_t opcode, bool fSizeOverride);
int patmPatchGenPushf(PVM pVM, PPATCHINFO pPatch, bool fSizeOverride);
int patmPatchGenPopf(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pReturnAddrGC, bool fSizeOverride, bool fGenJumpBack);
int patmPatchGenSti(PVM pVM, PPATCHINFO pPatch, RTGCPTR pCurInstrGC, RTGCPTR pNextInstrGC);

int patmPatchGenCli(PVM pVM, PPATCHINFO pPatch);
int patmPatchGenIret(PVM pVM, PPATCHINFO pPatch, RTGCPTR pCurInstrGC, bool fSizeOverride);
int patmPatchGenDuplicate(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pCurInstrGC);
int patmPatchGenPushCS(PVM pVM, PPATCHINFO pPatch);

int patmPatchGenStats(PVM pVM, PPATCHINFO pPatch, RTGCPTR pInstrGC);

int patmPatchGenCall(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR pInstrGC, RTGCPTR pTargetGC, bool fIndirect);
int patmPatchGenRet(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RCPTRTYPE(uint8_t *) pCurInstrGC);

int patmPatchGenPatchJump(PVM pVM, PPATCHINFO pPatch, RTGCPTR pCurInstrGC, RCPTRTYPE(uint8_t *) pPatchAddrGC, bool fAddLookupRecord = true);

/**
 * Generate indirect jump to unknown destination
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pCpu        Disassembly state
 * @param   pCurInstrGC Current instruction address
 */
int patmPatchGenJump(PVM pVM, PPATCHINFO pPatch, DISCPUSTATE *pCpu, RTGCPTR pCurInstrGC);

/**
 * Generate a trap handler entrypoint
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pTrapHandlerGC  IDT handler address
 */
int patmPatchGenTrapEntry(PVM pVM, PPATCHINFO pPatch, RTGCPTR pTrapHandlerGC);

/**
 * Generate an interrupt handler entrypoint
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pIntHandlerGC IDT handler address
 */
int patmPatchGenIntEntry(PVM pVM, PPATCHINFO pPatch, RTGCPTR pIntHandlerGC);

/**
 * Generate the jump from guest to patch code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch record
 * @param   pTargetGC   Guest target jump
 * @param   fClearInhibitIRQs   Clear inhibit irq flag
 */
int patmPatchGenJumpToGuest(PVM pVM, PPATCHINFO pPatch, RCPTRTYPE(uint8_t *) pReturnAddrGC, bool fClearInhibitIRQs = false);

/**
 * Generate illegal instruction (int 3)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 *
 */
int patmPatchGenIllegalInstr(PVM pVM, PPATCHINFO pPatch);

/**
 * Set PATM interrupt flag
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pInstrGC    Corresponding guest instruction
 *
 */
int patmPatchGenSetPIF(PVM pVM, PPATCHINFO pPatch, RTGCPTR pInstrGC);

/**
 * Clear PATM interrupt flag
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pInstrGC    Corresponding guest instruction
 *
 */
int patmPatchGenClearPIF(PVM pVM, PPATCHINFO pPatch, RTGCPTR pInstrGC);

/**
 * Clear PATM inhibit irq flag
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   pPatch          Patch structure
 * @param   pNextInstrGC    Next guest instruction
 */
int patmPatchGenClearInhibitIRQ(PVM pVM, PPATCHINFO pPatch, RTGCPTR pNextInstrGC);

/**
 * Check virtual IF flag and jump back to original guest code if set
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 * @param   pCurInstrGC Guest context pointer to the current instruction
 *
 */
int patmPatchGenCheckIF(PVM pVM, PPATCHINFO pPatch, RTGCPTR pCurInstrGC);

/**
 * Generate all global patm functions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPatch      Patch structure
 *
 */
int patmPatchGenGlobalFunctions(PVM pVM, PPATCHINFO pPatch);

#endif
