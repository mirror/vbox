/** @file
 *
 * SELM - Internal header file.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __SELMInternal_h__
#define __SELMInternal_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/stam.h>
#include <VBox/cpum.h>


#if !defined(IN_SELM_R3) && !defined(IN_SELM_R0) && !defined(IN_SELM_GC)
# error "Not in SELM! This is an internal header!"
#endif

/** @defgroup grp_selm_int   Internals
 * @ingroup grp_selm
 * @internal
 * @{
 */

/** Number of GDTs we need for internal use */
#define MAX_NEEDED_HYPERVISOR_GDTS      5

/** The number of GDTS allocated for our GDT. (full size) */
#define SELM_GDT_ELEMENTS               8192

/**
 * Converts a SELM pointer into a VM pointer.
 * @returns Pointer to the VM structure the SELM is part of.
 * @param   pSELM   Pointer to SELM instance data.
 */
#define SELM2VM(pSELM)  ( (PVM)((char*)pSELM - pSELM->offVM) )



/**
 * SELM Data (part of VM)
 */
typedef struct SELM
{
    /** Offset to the VM structure.
     * See SELM2VM(). */
    RTINT                   offVM;

    /** The Flat CS selector used by the VMM inside the GC. */
    RTSEL                   SelCS;
    /** The Flat DS selector used by the VMM inside the GC. */
    RTSEL                   SelDS;
    /** The 64-bit mode CS selector used by the VMM inside the GC. */
    RTSEL                   SelCS64;
    /** The TSS selector used by the VMM inside the GC. */
    RTSEL                   SelTSS;
    /** The TSS selector for taking trap 08 (\#DF). */
    RTSEL                   SelTSSTrap08;

    /** Pointer to the GCs - HC Ptr.
     * This size is governed by SELM_GDT_ELEMENTS. */
    HCPTRTYPE(PVBOXDESC)    paGdtHC;
    /** Pointer to the GCs - GC Ptr.
     * This is not initialized until the first relocation because it's used to
     * check if the shadow GDT virtual handler requires deregistration. */
    GCPTRTYPE(PVBOXDESC)    paGdtGC;
    /** Current (last) Guest's GDTR. */
    VBOXGDTR                GuestGdtr;
    /** The current (last) effective Guest GDT size. */
    RTUINT                  cbEffGuestGdtLimit;

    /** HC Pointer to the LDT shadow area placed in Hypervisor memory arena. */
    HCPTRTYPE(void *)       HCPtrLdt;
    /** GC Pointer to the LDT shadow area placed in Hypervisor memory arena. */
    GCPTRTYPE(void *)       GCPtrLdt;
    /** GC Pointer to the current Guest's LDT. */
    RTGCPTR                 GCPtrGuestLdt;
    /** Current LDT limit, both Guest and Shadow. */
    RTUINT                  cbLdtLimit;
    /** Current LDT offset relative to pvLdt*. */
    RTUINT                  offLdtHyper;

#if HC_ARCH_BITS == 32
    /** TSS alignment padding. */
    RTUINT                  auPadding[2];
#endif
    /** TSS. (This is 16 byte aligned!) */
    VBOXTSS                 Tss;
    /** @todo I/O bitmap & interrupt redirection table. */

    /** TSS for trap 08 (\#DF). */
    VBOXTSS                 TssTrap08;
    /** TSS for trap 0a (\#TS). */
    VBOXTSS                 TssTrap0a;

    /** GC Pointer to the TSS shadow area (Tss) placed in Hypervisor memory arena. */
    RTGCPTR                 GCPtrTss;
    /** GC Pointer to the current Guest's TSS. */
    RTGCPTR                 GCPtrGuestTss;
    /** The size of the guest TSS. */
    RTUINT                  cbGuestTss;
    /** Set if it's a 32-bit TSS. */
    bool                    fGuestTss32Bit;
    /** The size of the Guest's TSS part we're monitoring. */
    RTUINT                  cbMonitoredGuestTss;
    /** GC shadow TSS selector */
    RTSEL                   GCSelTss;

    /** Indicates that the Guest GDT access handler have been registered. */
    RTUINT                  fGDTRangeRegistered; /** @todo r=bird: use bool when we mean bool. Just keep in mind that it's a 1 byte byte. */

    /** Indicates whether LDT/GDT/TSS monitoring and syncing is disabled. */
    RTUINT                  fDisableMonitoring;

    /** SELMR3UpdateFromCPUM() profiling. */
    STAMPROFILE             StatUpdateFromCPUM;
    /** SELMR3SyncTSS() profiling. */
    STAMPROFILE             StatTSSSync;

    /** GC: The number of handled write to the Guest's GDT. */
    STAMCOUNTER             StatGCWriteGuestGDTHandled;
    /** GC: The number of unhandled write to the Guest's GDT. */
    STAMCOUNTER             StatGCWriteGuestGDTUnhandled;
    /** GC: The number of times write to Guest's LDT was detected. */
    STAMCOUNTER             StatGCWriteGuestLDT;
    /** GC: The number of handled write to the Guest's TSS. */
    STAMCOUNTER             StatGCWriteGuestTSSHandled;
    /** GC: The number of handled write to the Guest's TSS where we detected a change. */
    STAMCOUNTER             StatGCWriteGuestTSSHandledChanged;
    /** GC: The number of unhandled write to the Guest's TSS. */
    STAMCOUNTER             StatGCWriteGuestTSSUnhandled;
} SELM, *PSELM;

__BEGIN_DECLS

SELMGCDECL(int) selmgcGuestGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);
SELMGCDECL(int) selmgcGuestLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);
SELMGCDECL(int) selmgcGuestTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);

SELMGCDECL(int) selmgcShadowGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);
SELMGCDECL(int) selmgcShadowLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);
SELMGCDECL(int) selmgcShadowTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange);

__END_DECLS

#ifdef IN_RING3

#endif

/** @} */

#endif
