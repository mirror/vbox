/* $Id$ */
/** @file
 * SELM - Internal header file.
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

#ifndef ___SELMInternal_h
#define ___SELMInternal_h

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

/** The number of GDTS allocated for our GDT. (full size) */
#define SELM_GDT_ELEMENTS                   8192

/** aHyperSel index to retrieve hypervisor selectors */
/** The Flat CS selector used by the VMM inside the GC. */
#define SELM_HYPER_SEL_CS                   0
/** The Flat DS selector used by the VMM inside the GC. */
#define SELM_HYPER_SEL_DS                   1
/** The 64-bit mode CS selector used by the VMM inside the GC. */
#define SELM_HYPER_SEL_CS64                 2
/** The TSS selector used by the VMM inside the GC. */
#define SELM_HYPER_SEL_TSS                  3
/** The TSS selector for taking trap 08 (\#DF). */
#define SELM_HYPER_SEL_TSS_TRAP08           4
/** Number of GDTs we need for internal use */
#define SELM_HYPER_SEL_MAX                  (SELM_HYPER_SEL_TSS_TRAP08 + 1)


/** Default GDT selectors we use for the hypervisor. */
#define SELM_HYPER_DEFAULT_SEL_CS           ((SELM_GDT_ELEMENTS - 0x1) << 3)
#define SELM_HYPER_DEFAULT_SEL_DS           ((SELM_GDT_ELEMENTS - 0x2) << 3)
#define SELM_HYPER_DEFAULT_SEL_CS64         ((SELM_GDT_ELEMENTS - 0x3) << 3)
#define SELM_HYPER_DEFAULT_SEL_TSS          ((SELM_GDT_ELEMENTS - 0x4) << 3)
#define SELM_HYPER_DEFAULT_SEL_TSS_TRAP08   ((SELM_GDT_ELEMENTS - 0x5) << 3)
/** The lowest value default we use. */
#define SELM_HYPER_DEFAULT_BASE             SELM_HYPER_DEFAULT_SEL_TSS_TRAP08

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

    /* Flat CS, DS, 64 bit mode CS, TSS & trap 8 TSS. */
    RTSEL                   aHyperSel[SELM_HYPER_SEL_MAX];

    /** Pointer to the GCs - HC Ptr.
     * This size is governed by SELM_GDT_ELEMENTS. */
    R3R0PTRTYPE(PVBOXDESC)  paGdtHC;
    /** Pointer to the GCs - GC Ptr.
     * This is not initialized until the first relocation because it's used to
     * check if the shadow GDT virtual handler requires deregistration. */
    RCPTRTYPE(PVBOXDESC)    paGdtGC;
    /** Current (last) Guest's GDTR. */
    VBOXGDTR                GuestGdtr;
    /** The current (last) effective Guest GDT size. */
    RTUINT                  cbEffGuestGdtLimit;

    /** HC Pointer to the LDT shadow area placed in Hypervisor memory arena. */
    R3PTRTYPE(void *)       HCPtrLdt;
    /** GC Pointer to the LDT shadow area placed in Hypervisor memory arena. */
    RCPTRTYPE(void *)       GCPtrLdt;
    /** GC Pointer to the current Guest's LDT. */
    RTGCPTR                 GCPtrGuestLdt;
    /** Current LDT limit, both Guest and Shadow. */
    RTUINT                  cbLdtLimit;
    /** Current LDT offset relative to pvLdt*. */
    RTUINT                  offLdtHyper;

#if HC_ARCH_BITS == 32 || GC_ARCH_BITS == 64
    /** TSS alignment padding. */
    RTUINT                  auPadding[2];
#endif
    /** TSS. (This is 16 byte aligned!)
      * @todo I/O bitmap & interrupt redirection table? */
    VBOXTSS                 Tss;

    /** TSS for trap 08 (\#DF). */
    VBOXTSS                 TssTrap08;

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
    bool                    fGDTRangeRegistered;

    /** Indicates whether LDT/GDT/TSS monitoring and syncing is disabled. */
    bool                    fDisableMonitoring;

    /** Indicates whether the TSS stack selector & base address need to be refreshed.  */
    bool                    fSyncTSSRing0Stack;
    /** alignment . */
    RTUINT                  uPadding2;

    /** SELMR3UpdateFromCPUM() profiling. */
    STAMPROFILE             StatUpdateFromCPUM;
    /** SELMR3SyncTSS() profiling. */
    STAMPROFILE             StatTSSSync;

    /** GC: The number of handled writes to the Guest's GDT. */
    STAMCOUNTER             StatGCWriteGuestGDTHandled;
    /** GC: The number of unhandled write to the Guest's GDT. */
    STAMCOUNTER             StatGCWriteGuestGDTUnhandled;
    /** GC: The number of times writes to Guest's LDT was detected. */
    STAMCOUNTER             StatGCWriteGuestLDT;
    /** GC: The number of handled writes to the Guest's TSS. */
    STAMCOUNTER             StatGCWriteGuestTSSHandled;
    /** GC: The number of handled writes to the Guest's TSS where we detected a change. */
    STAMCOUNTER             StatGCWriteGuestTSSHandledChanged;
    /** GC: The number of handled redir writes to the Guest's TSS where we detected a change. */
    STAMCOUNTER             StatGCWriteGuestTSSRedir;
    /** GC: The number of unhandled writes to the Guest's TSS. */
    STAMCOUNTER             StatGCWriteGuestTSSUnhandled;
    /** The number of times we had to relocate our hypervisor selectors. */
    STAMCOUNTER             StatHyperSelsChanged;
    /** The number of times we had find free hypervisor selectors. */
    STAMCOUNTER             StatScanForHyperSels;
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
