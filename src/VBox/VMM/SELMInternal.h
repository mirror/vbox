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
#define SELM2VM(pSELM)  ( (PVM)((char *)pSELM - pSELM->offVM) )



/**
 * SELM Data (part of VM)
 */
typedef struct SELM
{
    /** Offset to the VM structure.
     * See SELM2VM(). */
    RTINT                   offVM;

    /** Flat CS, DS, 64 bit mode CS, TSS & trap 8 TSS. */
    RTSEL                   aHyperSel[SELM_HYPER_SEL_MAX];

    /** Pointer to the GCs - R3 Ptr.
     * This size is governed by SELM_GDT_ELEMENTS. */
    R3PTRTYPE(PX86DESC)     paGdtR3;
    /** Pointer to the GCs - RC Ptr.
     * This is not initialized until the first relocation because it's used to
     * check if the shadow GDT virtual handler requires deregistration. */
    RCPTRTYPE(PX86DESC)     paGdtRC;
    /** Current (last) Guest's GDTR.
     * The pGdt member is set to RTRCPTR_MAX if we're not monitoring the guest GDT. */
    VBOXGDTR                GuestGdtr;
    /** The current (last) effective Guest GDT size. */
    RTUINT                  cbEffGuestGdtLimit;

    uint32_t                padding0;

    /** R3 pointer to the LDT shadow area in HMA. */
    R3PTRTYPE(void *)       pvLdtR3;
    /** RC pointer to the LDT shadow area in HMA. */
    RCPTRTYPE(void *)       pvLdtRC;
#if GC_ARCH_BITS == 64
    RTRCPTR                 padding1;
#endif
    /** The address of the guest LDT.
     * RTRCPTR_MAX if not monitored. */
    RTGCPTR                 GCPtrGuestLdt;
    /** Current LDT limit, both Guest and Shadow. */
    RTUINT                  cbLdtLimit;
    /** Current LDT offset relative to pvLdtR3/pvLdtRC. */
    RTUINT                  offLdtHyper;
#if HC_ARCH_BITS == 32 && GC_ARCH_BITS == 64
    uint32_t                padding2[2];
#endif
    /** TSS. (This is 16 byte aligned!)
      * @todo I/O bitmap & interrupt redirection table? */
    VBOXTSS                 Tss;

    /** TSS for trap 08 (\#DF). */
    VBOXTSS                 TssTrap08;

    /** Monitored shadow TSS address. */
    RCPTRTYPE(void *)       pvMonShwTssRC;
#if GC_ARCH_BITS == 64
    RTRCPTR                 padding3;
#endif
    /** GC Pointer to the current Guest's TSS.
     * RTRCPTR_MAX if not monitored. */
    RTGCPTR                 GCPtrGuestTss;
    /** The size of the guest TSS. */
    RTUINT                  cbGuestTss;
    /** Set if it's a 32-bit TSS. */
    bool                    fGuestTss32Bit;
    /** The size of the Guest's TSS part we're monitoring. */
    RTUINT                  cbMonitoredGuestTss;
    /** The guest TSS selector at last sync (part of monitoring).
     * Contains RTSEL_MAX if not set. */
    RTSEL                   GCSelTss;
    /** The last known offset of the I/O bitmap.
     * This is only used if we monitor the bitmap. */
    uint16_t                offGuestIoBitmap;

    /** Indicates that the Guest GDT access handler have been registered. */
    bool                    fGDTRangeRegistered;

    /** Indicates whether LDT/GDT/TSS monitoring and syncing is disabled. */
    bool                    fDisableMonitoring;

    /** Indicates whether the TSS stack selector & base address need to be refreshed.  */
    bool                    fSyncTSSRing0Stack;
    bool                    fPadding2[1+2];

    /** SELMR3UpdateFromCPUM() profiling. */
    STAMPROFILE             StatUpdateFromCPUM;
    /** SELMR3SyncTSS() profiling. */
    STAMPROFILE             StatTSSSync;

    /** GC: The number of handled writes to the Guest's GDT. */
    STAMCOUNTER             StatRCWriteGuestGDTHandled;
    /** GC: The number of unhandled write to the Guest's GDT. */
    STAMCOUNTER             StatRCWriteGuestGDTUnhandled;
    /** GC: The number of times writes to Guest's LDT was detected. */
    STAMCOUNTER             StatRCWriteGuestLDT;
    /** GC: The number of handled writes to the Guest's TSS. */
    STAMCOUNTER             StatRCWriteGuestTSSHandled;
    /** GC: The number of handled writes to the Guest's TSS where we detected a change. */
    STAMCOUNTER             StatRCWriteGuestTSSHandledChanged;
    /** GC: The number of handled redir writes to the Guest's TSS where we detected a change. */
    STAMCOUNTER             StatRCWriteGuestTSSRedir;
    /** GC: The number of unhandled writes to the Guest's TSS. */
    STAMCOUNTER             StatRCWriteGuestTSSUnhandled;
    /** The number of times we had to relocate our hypervisor selectors. */
    STAMCOUNTER             StatHyperSelsChanged;
    /** The number of times we had find free hypervisor selectors. */
    STAMCOUNTER             StatScanForHyperSels;
} SELM, *PSELM;

__BEGIN_DECLS

VMMRCDECL(int) selmRCGuestGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCGuestLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCGuestTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);

VMMRCDECL(int) selmRCShadowGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCShadowLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCShadowTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);

void           selmSetRing1Stack(PVM pVM, uint32_t ss, RTGCPTR32 esp);

__END_DECLS

/** @} */

#endif
