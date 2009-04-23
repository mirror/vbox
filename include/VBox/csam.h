/** @file
 * CSAM - Guest OS Code Scanning and Analyis Manager.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_csam_h
#define ___VBox_csam_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>


/** @defgroup grp_csam      The Code Scanning and Analysis API
 * @{
 */

/**
 * CSAM monitoring tag
 * For use with CSAMR3MonitorPage
 */
typedef enum CSAMTAG
{
    CSAM_TAG_INVALID = 0,
    CSAM_TAG_REM,
    CSAM_TAG_PATM,
    CSAM_TAG_CSAM,
    CSAM_TAG_32BIT_HACK = 0x7fffffff
} CSAMTAG;


__BEGIN_DECLS


/**
 * Check if this page needs to be analysed by CSAM.
 *
 * This function should only be called for supervisor pages and
 * only when CSAM is enabled. Leaving these selection criteria
 * to the caller simplifies the interface (PTE passing).
 *
 * Note the the page has not yet been synced, so the TLB trick
 * (which wasn't ever active anyway) cannot be applied.
 *
 * @returns true if the page should be marked not present because
 *          CSAM want need to scan it.
 * @returns false if the page was already scanned.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer of page table entry
 */
VMMDECL(bool) CSAMDoesPageNeedScanning(PVM pVM, RTRCPTR GCPtr);

/**
 * Check if this page was previously scanned by CSAM
 *
 * @returns true -> scanned, false -> not scanned
 * @param   pVM         The VM to operate on.
 * @param   pPage       GC page address
 */
VMMDECL(bool) CSAMIsPageScanned(PVM pVM, RTRCPTR pPage);

/**
 * Mark a page as scanned/not scanned
 *
 * @note: we always mark it as scanned, even if we haven't completely done so
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pPage       GC page address (not necessarily aligned)
 * @param   fScanned    Mark as scanned or not scanned
 *
 */
VMMDECL(int) CSAMMarkPage(PVM pVM, RTRCPTR pPage, bool fScanned);


/**
 * Remember a possible code page for later inspection
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer of page
 */
VMMDECL(void) CSAMMarkPossibleCodePage(PVM pVM, RTRCPTR GCPtr);

/**
 * Query CSAM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
#define CSAMIsEnabled(pVM) (pVM->fCSAMEnabled && EMIsRawRing0Enabled(pVM))

/**
 * Turn on code scanning
 *
 * @returns VBox status code. (trap handled or not)
 * @param   pVM         The VM to operate on.
 */
VMMDECL(int) CSAMEnableScanning(PVM pVM);

/**
 * Turn off code scanning
 *
 * @returns VBox status code. (trap handled or not)
 * @param   pVM         The VM to operate on.
 */
VMMDECL(int) CSAMDisableScanning(PVM pVM);


/**
 * Check if this page needs to be analysed by CSAM
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 * @param   pvFault     Fault address
 */
VMMDECL(int) CSAMExecFault(PVM pVM, RTRCPTR pvFault);

/**
 * Check if we've scanned this instruction before. If true, then we can emulate
 * it instead of returning to ring 3.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       GC pointer of page table entry
 */
VMMDECL(bool) CSAMIsKnownDangerousInstr(PVM pVM, RTRCPTR GCPtr);


#ifdef IN_RING3
/** @defgroup grp_csam_r3      The Code Scanning and Analysis API
 * @ingroup grp_csam
 * @{
 */

/**
 * Query CSAM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CSAMR3IsEnabled(PVM pVM);

/**
 * Initializes the csam.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CSAMR3Init(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The csam will update the addresses used by the switcher.
 *
 * @param   pVM      The VM.
 * @param   offDelta Relocation delta.
 */
VMMR3DECL(void) CSAMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Terminates the csam.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CSAMR3Term(PVM pVM);

/**
 * CSAM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM     The VM which is reset.
 */
VMMR3DECL(int) CSAMR3Reset(PVM pVM);


/**
 * Notify CSAM of a page flush
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 */
VMMR3DECL(int) CSAMR3FlushPage(PVM pVM, RTRCPTR addr);

/**
 * Remove a CSAM monitored page. Use with care!
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 */
VMMR3DECL(int) CSAMR3RemovePage(PVM pVM, RTRCPTR addr);

/**
 * Scan and analyse code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtxCore    CPU context
 * @param   pInstrGC    Instruction pointer
 */
VMMR3DECL(int) CSAMR3CheckCodeEx(PVM pVM, PCPUMCTXCORE pCtxCore, RTRCPTR pInstrGC);

/**
 * Scan and analyse code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Instruction pointer (0:32 virtual address)
 */
VMMR3DECL(int) CSAMR3CheckCode(PVM pVM, RTRCPTR pInstrGC);

/**
 * Mark an instruction in a page as scanned/not scanned
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Instruction pointer
 * @param   opsize      Instruction size
 * @param   fScanned    Mark as scanned or not
 */
VMMR3DECL(int) CSAMR3MarkCode(PVM pVM, RTRCPTR pInstr, uint32_t opsize, bool fScanned);

/**
 * Perform any pending actions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR3DECL(int) CSAMR3DoPendingAction(PVM pVM, PVMCPU pVCpu);

/**
 * Monitors a code page (if not already monitored)
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   pPageAddrGC The page to monitor
 * @param   enmTag      Monitor tag
 */
VMMR3DECL(int) CSAMR3MonitorPage(PVM pVM, RTRCPTR pPageAddrGC, CSAMTAG enmTag);

/**
 * Unmonitors a code page
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   pPageAddrGC The page to monitor
 * @param   enmTag      Monitor tag
 */
VMMR3DECL(int) CSAMR3UnmonitorPage(PVM pVM, RTRCPTR pPageAddrGC, CSAMTAG enmTag);

/**
 * Analyse interrupt and trap gates
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   iGate       Start gate
 * @param   cGates      Number of gates to check
 */
VMMR3DECL(int) CSAMR3CheckGates(PVM pVM, uint32_t iGate, uint32_t cGates);

/**
 * Record previous call instruction addresses
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCPtrCall   Call address
 */
VMMR3DECL(int) CSAMR3RecordCallAddress(PVM pVM, RTRCPTR GCPtrCall);

/** @} */
#endif


/** @} */
__END_DECLS

#endif
