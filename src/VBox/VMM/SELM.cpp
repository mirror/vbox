/* $Id$ */
/** @file
 * SELM - The Selector Manager.
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

/** @page pg_selm   SELM - The Selector Manager
 *
 * SELM takes care of GDT, LDT and TSS shadowing in raw-mode, and the injection
 * of a few hyper selector for the raw-mode context.  In the hardware assisted
 * virtualization mode its only task is to decode entries in the guest GDT or
 * LDT once in a while.
 *
 * @see grp_selm
 *
 *
 * @section seg_selm_shadowing   Shadowing
 *
 * SELMR3UpdateFromCPUM() and SELMR3SyncTSS() does the bulk synchronization
 * work.  The three structures (GDT, LDT, TSS) are all shadowed wholesale atm.
 * The idea is to do it in a more on-demand fashion when we get time.  There
 * also a whole bunch of issues with the current synchronization of all three
 * tables, see notes and todos in the code.
 *
 * When the guest makes changes to the GDT we will try update the shadow copy
 * without involving SELMR3UpdateFromCPUM(), see selmGCSyncGDTEntry().
 *
 * When the guest make LDT changes we'll trigger a full resync of the LDT
 * (SELMR3UpdateFromCPUM()), which, needless to say, isn't optimal.
 *
 * The TSS shadowing is limited to the fields we need to care about, namely SS0
 * and ESP0.  The Patch Manager makes use of these.  We monitor updates to the
 * guest TSS and will try keep our SS0 and ESP0 copies up to date this way
 * rather than go the SELMR3SyncTSS() route.
 *
 * When in raw-mode SELM also injects a few extra GDT selectors which are used
 * by the raw-mode (hyper) context.  These start their life at the high end of
 * the table and will be relocated when the guest tries to make use of them...
 * Well, that was that idea at least, only the code isn't quite there yet which
 * is why we have trouble with guests which actually have a full sized GDT.
 *
 * So, the summary of the current GDT, LDT and TSS shadowing is that there is a
 * lot of relatively simple and enjoyable work to be done, see @bugref{3267}.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SELM
#include <VBox/selm.h>
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/pgm.h>
#include <VBox/trpm.h>
#include <VBox/dbgf.h>
#include "SELMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/string.h>


/**
 * Enable or disable tracking of Guest's GDT/LDT/TSS.
 * @{
 */
#define SELM_TRACK_GUEST_GDT_CHANGES
#define SELM_TRACK_GUEST_LDT_CHANGES
#define SELM_TRACK_GUEST_TSS_CHANGES
/** @} */

/**
 * Enable or disable tracking of Shadow GDT/LDT/TSS.
 * @{
 */
#define SELM_TRACK_SHADOW_GDT_CHANGES
#define SELM_TRACK_SHADOW_LDT_CHANGES
#define SELM_TRACK_SHADOW_TSS_CHANGES
/** @} */


/** SELM saved state version. */
#define SELM_SAVED_STATE_VERSION    5


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  selmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  selmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(int)  selmR3LoadDone(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  selmR3GuestGDTWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
static DECLCALLBACK(int)  selmR3GuestLDTWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
static DECLCALLBACK(int)  selmR3GuestTSSWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
static DECLCALLBACK(void) selmR3InfoGdt(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) selmR3InfoGdtGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) selmR3InfoLdt(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) selmR3InfoLdtGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
//static DECLCALLBACK(void) selmR3InfoTss(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
//static DECLCALLBACK(void) selmR3InfoTssGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);



/**
 * Initializes the SELM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) SELMR3Init(PVM pVM)
{
    LogFlow(("SELMR3Init\n"));

    /*
     * Assert alignment and sizes.
     * (The TSS block requires contiguous back.)
     */
    AssertCompile(sizeof(pVM->selm.s) <= sizeof(pVM->selm.padding));    AssertRelease(sizeof(pVM->selm.s) <= sizeof(pVM->selm.padding));
    AssertCompileMemberAlignment(VM, selm.s, 32);                       AssertRelease(!(RT_OFFSETOF(VM, selm.s) & 31));
#if 0 /* doesn't work */
    AssertCompile((RT_OFFSETOF(VM, selm.s.Tss)       & PAGE_OFFSET_MASK) <= PAGE_SIZE - sizeof(pVM->selm.s.Tss));
    AssertCompile((RT_OFFSETOF(VM, selm.s.TssTrap08) & PAGE_OFFSET_MASK) <= PAGE_SIZE - sizeof(pVM->selm.s.TssTrap08));
#endif
    AssertRelease((RT_OFFSETOF(VM, selm.s.Tss)       & PAGE_OFFSET_MASK) <= PAGE_SIZE - sizeof(pVM->selm.s.Tss));
    AssertRelease((RT_OFFSETOF(VM, selm.s.TssTrap08) & PAGE_OFFSET_MASK) <= PAGE_SIZE - sizeof(pVM->selm.s.TssTrap08));
    AssertRelease(sizeof(pVM->selm.s.Tss.IntRedirBitmap) == 0x20);

    /*
     * Init the structure.
     */
    pVM->selm.s.offVM                                = RT_OFFSETOF(VM, selm);
    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS]         = (SELM_GDT_ELEMENTS - 0x1) << 3;
    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]         = (SELM_GDT_ELEMENTS - 0x2) << 3;
    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64]       = (SELM_GDT_ELEMENTS - 0x3) << 3;
    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]        = (SELM_GDT_ELEMENTS - 0x4) << 3;
    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] = (SELM_GDT_ELEMENTS - 0x5) << 3;

    /*
     * Allocate GDT table.
     */
    int rc = MMR3HyperAllocOnceNoRel(pVM, sizeof(pVM->selm.s.paGdtR3[0]) * SELM_GDT_ELEMENTS,
                                     PAGE_SIZE, MM_TAG_SELM, (void **)&pVM->selm.s.paGdtR3);
    AssertRCReturn(rc, rc);

    /*
     * Allocate LDT area.
     */
    rc = MMR3HyperAllocOnceNoRel(pVM, _64K + PAGE_SIZE, PAGE_SIZE, MM_TAG_SELM, &pVM->selm.s.pvLdtR3);
    AssertRCReturn(rc, rc);

    /*
     * Init Guest's and Shadow GDT, LDT, TSS changes control variables.
     */
    pVM->selm.s.cbEffGuestGdtLimit = 0;
    pVM->selm.s.GuestGdtr.pGdt     = RTRCPTR_MAX;
    pVM->selm.s.GCPtrGuestLdt      = RTRCPTR_MAX;
    pVM->selm.s.GCPtrGuestTss      = RTRCPTR_MAX;

    pVM->selm.s.paGdtRC            = NIL_RTRCPTR; /* Must be set in SELMR3Relocate because of monitoring. */
    pVM->selm.s.pvLdtRC            = RTRCPTR_MAX;
    pVM->selm.s.pvMonShwTssRC      = RTRCPTR_MAX;
    pVM->selm.s.GCSelTss           = RTSEL_MAX;

    pVM->selm.s.fDisableMonitoring = false;
    pVM->selm.s.fSyncTSSRing0Stack = false;

    /* The I/O bitmap starts right after the virtual interrupt redirection bitmap. Outside the TSS on purpose; the CPU will not check it
     * for I/O operations. */
    pVM->selm.s.Tss.offIoBitmap = sizeof(VBOXTSS);
    /* bit set to 1 means no redirection */
    memset(pVM->selm.s.Tss.IntRedirBitmap, 0xff, sizeof(pVM->selm.s.Tss.IntRedirBitmap));

    /*
     * Register the saved state data unit.
     */
    rc = SSMR3RegisterInternal(pVM, "selm", 1, SELM_SAVED_STATE_VERSION, sizeof(SELM),
                               NULL, selmR3Save, NULL,
                               NULL, selmR3Load, selmR3LoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Statistics.
     */
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestGDTHandled,     STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/GDTInt",  STAMUNIT_OCCURENCES,     "The number of handled writes to the Guest GDT.");
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestGDTUnhandled,   STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/GDTEmu",  STAMUNIT_OCCURENCES,     "The number of unhandled writes to the Guest GDT.");
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestLDT,            STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/LDT",     STAMUNIT_OCCURENCES,     "The number of writes to the Guest LDT was detected.");
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestTSSHandled,     STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/TSSInt",  STAMUNIT_OCCURENCES,     "The number of handled writes to the Guest TSS.");
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestTSSRedir,       STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/TSSRedir",STAMUNIT_OCCURENCES,     "The number of handled redir bitmap writes to the Guest TSS.");
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestTSSHandledChanged,STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/TSSIntChg", STAMUNIT_OCCURENCES, "The number of handled writes to the Guest TSS where the R0 stack changed.");
    STAM_REG(pVM, &pVM->selm.s.StatRCWriteGuestTSSUnhandled,   STAMTYPE_COUNTER, "/SELM/GC/Write/Guest/TSSEmu",  STAMUNIT_OCCURENCES,     "The number of unhandled writes to the Guest TSS.");
    STAM_REG(pVM, &pVM->selm.s.StatTSSSync,                    STAMTYPE_PROFILE, "/PROF/SELM/TSSSync",           STAMUNIT_TICKS_PER_CALL, "Profiling of the SELMR3SyncTSS() body.");
    STAM_REG(pVM, &pVM->selm.s.StatUpdateFromCPUM,             STAMTYPE_PROFILE, "/PROF/SELM/UpdateFromCPUM",    STAMUNIT_TICKS_PER_CALL, "Profiling of the SELMR3UpdateFromCPUM() body.");

    STAM_REG(pVM, &pVM->selm.s.StatHyperSelsChanged,           STAMTYPE_COUNTER, "/SELM/HyperSels/Changed",      STAMUNIT_OCCURENCES,     "The number of times we had to relocate our hypervisor selectors.");
    STAM_REG(pVM, &pVM->selm.s.StatScanForHyperSels,           STAMTYPE_COUNTER, "/SELM/HyperSels/Scan",         STAMUNIT_OCCURENCES,     "The number of times we had find free hypervisor selectors.");

    /*
     * Default action when entering raw mode for the first time
     */
    PVMCPU pVCpu = &pVM->aCpus[0];  /* raw mode implies on VCPU */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_GDT);
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_LDT);

    /*
     * Register info handlers.
     */
    DBGFR3InfoRegisterInternal(pVM, "gdt",      "Displays the shadow GDT. No arguments.",   &selmR3InfoGdt);
    DBGFR3InfoRegisterInternal(pVM, "gdtguest", "Displays the guest GDT. No arguments.",    &selmR3InfoGdtGuest);
    DBGFR3InfoRegisterInternal(pVM, "ldt",      "Displays the shadow LDT. No arguments.",   &selmR3InfoLdt);
    DBGFR3InfoRegisterInternal(pVM, "ldtguest", "Displays the guest LDT. No arguments.",    &selmR3InfoLdtGuest);
    //DBGFR3InfoRegisterInternal(pVM, "tss",      "Displays the shadow TSS. No arguments.",   &selmR3InfoTss);
    //DBGFR3InfoRegisterInternal(pVM, "tssguest", "Displays the guest TSS. No arguments.",    &selmR3InfoTssGuest);

    return rc;
}


/**
 * Finalizes HMA page attributes.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int) SELMR3InitFinalize(PVM pVM)
{
    /** @cfgm{/DoubleFault,bool,false}
     * Enables catching of double faults in the raw-mode context VMM code.  This can
     * be used when the tripple faults or hangs occure and one suspect an unhandled
     * double fault.  This is not enabled by default because it means making the
     * hyper selectors writeable for all supervisor code, including the guest's.
     * The double fault is a task switch and thus requires write access to the GDT
     * of the TSS (to set it busy), to the old TSS (to store state), and to the Trap
     * 8 TSS for the back link.
     */
    bool f;
#if defined(DEBUG_bird)
    int rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "DoubleFault", &f, true);
#else
    int rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "DoubleFault", &f, false);
#endif
    AssertLogRelRCReturn(rc, rc);
    if (f)
    {
        PX86DESC paGdt = pVM->selm.s.paGdtR3;
        rc = PGMMapSetPage(pVM, MMHyperR3ToRC(pVM, &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] >> 3]), sizeof(paGdt[0]),
                           X86_PTE_RW | X86_PTE_P | X86_PTE_A | X86_PTE_D);
        AssertRC(rc);
        rc = PGMMapSetPage(pVM, MMHyperR3ToRC(pVM, &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS] >> 3]), sizeof(paGdt[0]),
                           X86_PTE_RW | X86_PTE_P | X86_PTE_A | X86_PTE_D);
        AssertRC(rc);
        rc = PGMMapSetPage(pVM, VM_RC_ADDR(pVM, &pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]), sizeof(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]),
                           X86_PTE_RW | X86_PTE_P | X86_PTE_A | X86_PTE_D);
        AssertRC(rc);
        rc = PGMMapSetPage(pVM, VM_RC_ADDR(pVM, &pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08]), sizeof(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08]),
                           X86_PTE_RW | X86_PTE_P | X86_PTE_A | X86_PTE_D);
        AssertRC(rc);
    }
    return VINF_SUCCESS;
}


/**
 * Setup the hypervisor GDT selectors in our shadow table
 *
 * @param   pVM     The VM handle.
 */
static void selmR3SetupHyperGDTSelectors(PVM pVM)
{
    PX86DESC paGdt = pVM->selm.s.paGdtR3;

    /*
     * Set up global code and data descriptors for use in the guest context.
     * Both are wide open (base 0, limit 4GB)
     */
    PX86DESC   pDesc = &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS] >> 3];
    pDesc->Gen.u16LimitLow      = 0xffff;
    pDesc->Gen.u4LimitHigh      = 0xf;
    pDesc->Gen.u16BaseLow       = 0;
    pDesc->Gen.u8BaseHigh1      = 0;
    pDesc->Gen.u8BaseHigh2      = 0;
    pDesc->Gen.u4Type           = X86_SEL_TYPE_ER_ACC;
    pDesc->Gen.u1DescType       = 1; /* not system, but code/data */
    pDesc->Gen.u2Dpl            = 0; /* supervisor */
    pDesc->Gen.u1Present        = 1;
    pDesc->Gen.u1Available      = 0;
    pDesc->Gen.u1Long           = 0;
    pDesc->Gen.u1DefBig         = 1; /* def 32 bit */
    pDesc->Gen.u1Granularity    = 1; /* 4KB limit */

    /* data */
    pDesc = &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS] >> 3];
    pDesc->Gen.u16LimitLow      = 0xffff;
    pDesc->Gen.u4LimitHigh      = 0xf;
    pDesc->Gen.u16BaseLow       = 0;
    pDesc->Gen.u8BaseHigh1      = 0;
    pDesc->Gen.u8BaseHigh2      = 0;
    pDesc->Gen.u4Type           = X86_SEL_TYPE_RW_ACC;
    pDesc->Gen.u1DescType       = 1; /* not system, but code/data */
    pDesc->Gen.u2Dpl            = 0; /* supervisor */
    pDesc->Gen.u1Present        = 1;
    pDesc->Gen.u1Available      = 0;
    pDesc->Gen.u1Long           = 0;
    pDesc->Gen.u1DefBig         = 1; /* big */
    pDesc->Gen.u1Granularity    = 1; /* 4KB limit */

    /* 64-bit mode code (& data?) */
    pDesc = &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64] >> 3];
    pDesc->Gen.u16LimitLow      = 0xffff;
    pDesc->Gen.u4LimitHigh      = 0xf;
    pDesc->Gen.u16BaseLow       = 0;
    pDesc->Gen.u8BaseHigh1      = 0;
    pDesc->Gen.u8BaseHigh2      = 0;
    pDesc->Gen.u4Type           = X86_SEL_TYPE_ER_ACC;
    pDesc->Gen.u1DescType       = 1; /* not system, but code/data */
    pDesc->Gen.u2Dpl            = 0; /* supervisor */
    pDesc->Gen.u1Present        = 1;
    pDesc->Gen.u1Available      = 0;
    pDesc->Gen.u1Long           = 1; /* The Long (L) attribute bit. */
    pDesc->Gen.u1DefBig         = 0; /* With L=1 this must be 0. */
    pDesc->Gen.u1Granularity    = 1; /* 4KB limit */

    /*
     * TSS descriptor
     */
    pDesc = &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS] >> 3];
    RTRCPTR RCPtrTSS = VM_RC_ADDR(pVM, &pVM->selm.s.Tss);
    pDesc->Gen.u16BaseLow       = RT_LOWORD(RCPtrTSS);
    pDesc->Gen.u8BaseHigh1      = RT_BYTE3(RCPtrTSS);
    pDesc->Gen.u8BaseHigh2      = RT_BYTE4(RCPtrTSS);
    pDesc->Gen.u16LimitLow      = sizeof(VBOXTSS) - 1;
    pDesc->Gen.u4LimitHigh      = 0;
    pDesc->Gen.u4Type           = X86_SEL_TYPE_SYS_386_TSS_AVAIL;
    pDesc->Gen.u1DescType       = 0; /* system */
    pDesc->Gen.u2Dpl            = 0; /* supervisor */
    pDesc->Gen.u1Present        = 1;
    pDesc->Gen.u1Available      = 0;
    pDesc->Gen.u1Long           = 0;
    pDesc->Gen.u1DefBig         = 0;
    pDesc->Gen.u1Granularity    = 0; /* byte limit */

    /*
     * TSS descriptor for trap 08
     */
    pDesc = &paGdt[pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] >> 3];
    pDesc->Gen.u16LimitLow      = sizeof(VBOXTSS) - 1;
    pDesc->Gen.u4LimitHigh      = 0;
    RCPtrTSS = VM_RC_ADDR(pVM, &pVM->selm.s.TssTrap08);
    pDesc->Gen.u16BaseLow       = RT_LOWORD(RCPtrTSS);
    pDesc->Gen.u8BaseHigh1      = RT_BYTE3(RCPtrTSS);
    pDesc->Gen.u8BaseHigh2      = RT_BYTE4(RCPtrTSS);
    pDesc->Gen.u4Type           = X86_SEL_TYPE_SYS_386_TSS_AVAIL;
    pDesc->Gen.u1DescType       = 0; /* system */
    pDesc->Gen.u2Dpl            = 0; /* supervisor */
    pDesc->Gen.u1Present        = 1;
    pDesc->Gen.u1Available      = 0;
    pDesc->Gen.u1Long           = 0;
    pDesc->Gen.u1DefBig         = 0;
    pDesc->Gen.u1Granularity    = 0; /* byte limit */
}

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 */
VMMR3DECL(void) SELMR3Relocate(PVM pVM)
{
    PX86DESC paGdt = pVM->selm.s.paGdtR3;
    LogFlow(("SELMR3Relocate\n"));

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        /*
         * Update GDTR and selector.
         */
        CPUMSetHyperGDTR(pVCpu, MMHyperR3ToRC(pVM, paGdt), SELM_GDT_ELEMENTS * sizeof(paGdt[0]) - 1);

        /** @todo selector relocations should be a seperate operation? */
        CPUMSetHyperCS(pVCpu, pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS]);
        CPUMSetHyperDS(pVCpu, pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]);
        CPUMSetHyperES(pVCpu, pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]);
        CPUMSetHyperSS(pVCpu, pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]);
        CPUMSetHyperTR(pVCpu, pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]);
    }

    selmR3SetupHyperGDTSelectors(pVM);

/** @todo SELM must be called when any of the CR3s changes during a cpu mode change. */
/** @todo PGM knows the proper CR3 values these days, not CPUM. */
    /*
     * Update the TSSes.
     */
    /* Only applies to raw mode which supports only 1 VCPU */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /* Current TSS */
    pVM->selm.s.Tss.cr3     = PGMGetHyperCR3(pVCpu);
    pVM->selm.s.Tss.ss0     = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.Tss.esp0    = VMMGetStackRC(pVM);
    pVM->selm.s.Tss.cs      = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS];
    pVM->selm.s.Tss.ds      = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.Tss.es      = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.Tss.offIoBitmap = sizeof(VBOXTSS);

    /* trap 08 */
    pVM->selm.s.TssTrap08.cr3    = PGMGetInterRCCR3(pVM, pVCpu);                   /* this should give use better survival chances. */
    pVM->selm.s.TssTrap08.ss0    = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.TssTrap08.ss     = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.TssTrap08.esp0   = VMMGetStackRC(pVM) - PAGE_SIZE / 2;  /* upper half can be analysed this way. */
    pVM->selm.s.TssTrap08.esp    = pVM->selm.s.TssTrap08.esp0;
    pVM->selm.s.TssTrap08.ebp    = pVM->selm.s.TssTrap08.esp0;
    pVM->selm.s.TssTrap08.cs     = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS];
    pVM->selm.s.TssTrap08.ds     = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.TssTrap08.es     = pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS];
    pVM->selm.s.TssTrap08.fs     = 0;
    pVM->selm.s.TssTrap08.gs     = 0;
    pVM->selm.s.TssTrap08.selLdt = 0;
    pVM->selm.s.TssTrap08.eflags = 0x2;    /* all cleared */
    pVM->selm.s.TssTrap08.ecx    = VM_RC_ADDR(pVM, &pVM->selm.s.Tss);       /* setup ecx to normal Hypervisor TSS address. */
    pVM->selm.s.TssTrap08.edi    = pVM->selm.s.TssTrap08.ecx;
    pVM->selm.s.TssTrap08.eax    = pVM->selm.s.TssTrap08.ecx;
    pVM->selm.s.TssTrap08.edx    = VM_RC_ADDR(pVM, pVM);                    /* setup edx VM address. */
    pVM->selm.s.TssTrap08.edi    = pVM->selm.s.TssTrap08.edx;
    pVM->selm.s.TssTrap08.ebx    = pVM->selm.s.TssTrap08.edx;
    pVM->selm.s.TssTrap08.offIoBitmap = sizeof(VBOXTSS);
    /* TRPM will be updating the eip */

    if (!pVM->selm.s.fDisableMonitoring)
    {
        /*
         * Update shadow GDT/LDT/TSS write access handlers.
         */
        int rc;
#ifdef SELM_TRACK_SHADOW_GDT_CHANGES
        if (pVM->selm.s.paGdtRC != NIL_RTRCPTR)
        {
            rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.paGdtRC);
            AssertRC(rc);
        }
        pVM->selm.s.paGdtRC = MMHyperR3ToRC(pVM, paGdt);
        rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_HYPERVISOR, pVM->selm.s.paGdtRC,
                                         pVM->selm.s.paGdtRC + SELM_GDT_ELEMENTS * sizeof(paGdt[0]) - 1,
                                         0, 0, "selmRCShadowGDTWriteHandler", 0, "Shadow GDT write access handler");
        AssertRC(rc);
#endif
#ifdef SELM_TRACK_SHADOW_TSS_CHANGES
        if (pVM->selm.s.pvMonShwTssRC != RTRCPTR_MAX)
        {
            rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.pvMonShwTssRC);
            AssertRC(rc);
        }
        pVM->selm.s.pvMonShwTssRC = VM_RC_ADDR(pVM, &pVM->selm.s.Tss);
        rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_HYPERVISOR, pVM->selm.s.pvMonShwTssRC,
                                         pVM->selm.s.pvMonShwTssRC + sizeof(pVM->selm.s.Tss) - 1,
                                         0, 0, "selmRCShadowTSSWriteHandler", 0, "Shadow TSS write access handler");
        AssertRC(rc);
#endif

        /*
         * Update the GC LDT region handler and address.
         */
#ifdef SELM_TRACK_SHADOW_LDT_CHANGES
        if (pVM->selm.s.pvLdtRC != RTRCPTR_MAX)
        {
            rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.pvLdtRC);
            AssertRC(rc);
        }
#endif
        pVM->selm.s.pvLdtRC = MMHyperR3ToRC(pVM, pVM->selm.s.pvLdtR3);
#ifdef SELM_TRACK_SHADOW_LDT_CHANGES
        rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_HYPERVISOR, pVM->selm.s.pvLdtRC,
                                         pVM->selm.s.pvLdtRC + _64K + PAGE_SIZE - 1,
                                         0, 0, "selmRCShadowLDTWriteHandler", 0, "Shadow LDT write access handler");
        AssertRC(rc);
#endif
    }
}


/**
 * Terminates the SELM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) SELMR3Term(PVM pVM)
{
    return 0;
}


/**
 * The VM is being reset.
 *
 * For the SELM component this means that any GDT/LDT/TSS monitors
 * needs to be removed.
 *
 * @param   pVM     VM handle.
 */
VMMR3DECL(void) SELMR3Reset(PVM pVM)
{
    LogFlow(("SELMR3Reset:\n"));
    VM_ASSERT_EMT(pVM);

    /*
     * Uninstall guest GDT/LDT/TSS write access handlers.
     */
    int rc;
#ifdef SELM_TRACK_GUEST_GDT_CHANGES
    if (pVM->selm.s.GuestGdtr.pGdt != RTRCPTR_MAX && pVM->selm.s.fGDTRangeRegistered)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GuestGdtr.pGdt);
        AssertRC(rc);
        pVM->selm.s.GuestGdtr.pGdt = RTRCPTR_MAX;
        pVM->selm.s.GuestGdtr.cbGdt = 0;
    }
    pVM->selm.s.fGDTRangeRegistered = false;
#endif
#ifdef SELM_TRACK_GUEST_LDT_CHANGES
    if (pVM->selm.s.GCPtrGuestLdt != RTRCPTR_MAX)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestLdt);
        AssertRC(rc);
        pVM->selm.s.GCPtrGuestLdt = RTRCPTR_MAX;
    }
#endif
#ifdef SELM_TRACK_GUEST_TSS_CHANGES
    if (pVM->selm.s.GCPtrGuestTss != RTRCPTR_MAX)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestTss);
        AssertRC(rc);
        pVM->selm.s.GCPtrGuestTss = RTRCPTR_MAX;
        pVM->selm.s.GCSelTss      = RTSEL_MAX;
    }
#endif

    /*
     * Re-initialize other members.
     */
    pVM->selm.s.cbLdtLimit = 0;
    pVM->selm.s.offLdtHyper = 0;
    pVM->selm.s.cbMonitoredGuestTss = 0;

    pVM->selm.s.fSyncTSSRing0Stack = false;

    /*
     * Default action when entering raw mode for the first time
     */
    PVMCPU pVCpu = &pVM->aCpus[0];  /* raw mode implies on VCPU */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_GDT);
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_LDT);
}

/**
 * Disable GDT/LDT/TSS monitoring and syncing
 *
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(void) SELMR3DisableMonitoring(PVM pVM)
{
    /*
     * Uninstall guest GDT/LDT/TSS write access handlers.
     */
    int rc;
#ifdef SELM_TRACK_GUEST_GDT_CHANGES
    if (pVM->selm.s.GuestGdtr.pGdt != RTRCPTR_MAX && pVM->selm.s.fGDTRangeRegistered)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GuestGdtr.pGdt);
        AssertRC(rc);
        pVM->selm.s.GuestGdtr.pGdt = RTRCPTR_MAX;
        pVM->selm.s.GuestGdtr.cbGdt = 0;
    }
    pVM->selm.s.fGDTRangeRegistered = false;
#endif
#ifdef SELM_TRACK_GUEST_LDT_CHANGES
    if (pVM->selm.s.GCPtrGuestLdt != RTRCPTR_MAX)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestLdt);
        AssertRC(rc);
        pVM->selm.s.GCPtrGuestLdt = RTRCPTR_MAX;
    }
#endif
#ifdef SELM_TRACK_GUEST_TSS_CHANGES
    if (pVM->selm.s.GCPtrGuestTss != RTRCPTR_MAX)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestTss);
        AssertRC(rc);
        pVM->selm.s.GCPtrGuestTss = RTRCPTR_MAX;
        pVM->selm.s.GCSelTss      = RTSEL_MAX;
    }
#endif

    /*
     * Unregister shadow GDT/LDT/TSS write access handlers.
     */
#ifdef SELM_TRACK_SHADOW_GDT_CHANGES
    if (pVM->selm.s.paGdtRC != NIL_RTRCPTR)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.paGdtRC);
        AssertRC(rc);
        pVM->selm.s.paGdtRC = NIL_RTRCPTR;
    }
#endif
#ifdef SELM_TRACK_SHADOW_TSS_CHANGES
    if (pVM->selm.s.pvMonShwTssRC != RTRCPTR_MAX)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.pvMonShwTssRC);
        AssertRC(rc);
        pVM->selm.s.pvMonShwTssRC = RTRCPTR_MAX;
    }
#endif
#ifdef SELM_TRACK_SHADOW_LDT_CHANGES
    if (pVM->selm.s.pvLdtRC != RTRCPTR_MAX)
    {
        rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.pvLdtRC);
        AssertRC(rc);
        pVM->selm.s.pvLdtRC = RTRCPTR_MAX;
    }
#endif

    PVMCPU pVCpu = &pVM->aCpus[0];  /* raw mode implies on VCPU */
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_TSS);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_GDT);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_LDT);

    pVM->selm.s.fDisableMonitoring = true;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) selmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("selmR3Save:\n"));

    /*
     * Save the basic bits - fortunately all the other things can be resynced on load.
     */
    PSELM pSelm = &pVM->selm.s;

    SSMR3PutBool(pSSM, pSelm->fDisableMonitoring);
    SSMR3PutBool(pSSM, pSelm->fSyncTSSRing0Stack);
    SSMR3PutSel(pSSM, pSelm->aHyperSel[SELM_HYPER_SEL_CS]);
    SSMR3PutSel(pSSM, pSelm->aHyperSel[SELM_HYPER_SEL_DS]);
    SSMR3PutSel(pSSM, pSelm->aHyperSel[SELM_HYPER_SEL_CS64]);
    SSMR3PutSel(pSSM, pSelm->aHyperSel[SELM_HYPER_SEL_CS64]); /* reserved for DS64. */
    SSMR3PutSel(pSSM, pSelm->aHyperSel[SELM_HYPER_SEL_TSS]);
    return SSMR3PutSel(pSSM, pSelm->aHyperSel[SELM_HYPER_SEL_TSS_TRAP08]);
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) selmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    LogFlow(("selmR3Load:\n"));

    /*
     * Validate version.
     */
    if (u32Version != SELM_SAVED_STATE_VERSION)
    {
        AssertMsgFailed(("selmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Do a reset.
     */
    SELMR3Reset(pVM);

    /* Get the monitoring flag. */
    SSMR3GetBool(pSSM, &pVM->selm.s.fDisableMonitoring);

    /* Get the TSS state flag. */
    SSMR3GetBool(pSSM, &pVM->selm.s.fSyncTSSRing0Stack);

    /*
     * Get the selectors.
     */
    RTSEL SelCS;
    SSMR3GetSel(pSSM, &SelCS);
    RTSEL SelDS;
    SSMR3GetSel(pSSM, &SelDS);
    RTSEL SelCS64;
    SSMR3GetSel(pSSM, &SelCS64);
    RTSEL SelDS64;
    SSMR3GetSel(pSSM, &SelDS64);
    RTSEL SelTSS;
    SSMR3GetSel(pSSM, &SelTSS);
    RTSEL SelTSSTrap08;
    SSMR3GetSel(pSSM, &SelTSSTrap08);

    /* Copy the selectors; they will be checked during relocation. */
    PSELM pSelm = &pVM->selm.s;
    pSelm->aHyperSel[SELM_HYPER_SEL_CS]         = SelCS;
    pSelm->aHyperSel[SELM_HYPER_SEL_DS]         = SelDS;
    pSelm->aHyperSel[SELM_HYPER_SEL_CS64]       = SelCS64;
    pSelm->aHyperSel[SELM_HYPER_SEL_TSS]        = SelTSS;
    pSelm->aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] = SelTSSTrap08;

    return VINF_SUCCESS;
}


/**
 * Sync the GDT, LDT and TSS after loading the state.
 *
 * Just to play save, we set the FFs to force syncing before
 * executing GC code.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) selmR3LoadDone(PVM pVM, PSSMHANDLE pSSM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    LogFlow(("selmR3LoadDone:\n"));

    /*
     * Don't do anything if it's a load failure.
     */
    int rc = SSMR3HandleGetStatus(pSSM);
    if (RT_FAILURE(rc))
        return VINF_SUCCESS;

    /*
     * Do the syncing if we're in protected mode.
     */
    if (PGMGetGuestMode(pVCpu) != PGMMODE_REAL)
    {
        VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_GDT);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_LDT);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);
        SELMR3UpdateFromCPUM(pVM, pVCpu);
    }

    /*
     * Flag everything for resync on next raw mode entry.
     */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_GDT);
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_LDT);
    VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

    return VINF_SUCCESS;
}


/**
 * Updates the Guest GDT & LDT virtualization based on current CPU state.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR3DECL(int) SELMR3UpdateFromCPUM(PVM pVM, PVMCPU pVCpu)
{
    int    rc    = VINF_SUCCESS;

    if (pVM->selm.s.fDisableMonitoring)
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_GDT);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_LDT);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

        return VINF_SUCCESS;
    }

    STAM_PROFILE_START(&pVM->selm.s.StatUpdateFromCPUM, a);

    /*
     * GDT sync
     */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_SELM_SYNC_GDT))
    {
        /*
         * Always assume the best
         */
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_GDT);

        /* If the GDT was changed, then make sure the LDT is checked too */
        /** @todo only do this if the actual ldtr selector was changed; this is a bit excessive */
        VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_LDT);
        /* Same goes for the TSS selector */
        VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

        /*
         * Get the GDTR and check if there is anything to do (there usually is).
         */
        VBOXGDTR    GDTR;
        CPUMGetGuestGDTR(pVCpu, &GDTR);
        if (GDTR.cbGdt < sizeof(X86DESC))
        {
            Log(("No GDT entries...\n"));
            STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
            return VINF_SUCCESS;
        }

        /*
         * Read the Guest GDT.
         * ASSUMES that the entire GDT is in memory.
         */
        RTUINT      cbEffLimit = GDTR.cbGdt;
        PX86DESC   pGDTE = &pVM->selm.s.paGdtR3[1];
        rc = PGMPhysSimpleReadGCPtr(pVCpu, pGDTE, GDTR.pGdt + sizeof(X86DESC), cbEffLimit + 1 - sizeof(X86DESC));
        if (RT_FAILURE(rc))
        {
            /*
             * Read it page by page.
             *
             * Keep track of the last valid page and delay memsets and
             * adjust cbEffLimit to reflect the effective size. The latter
             * is something we do in the belief that the guest will probably
             * never actually commit the last page, thus allowing us to keep
             * our selectors in the high end of the GDT.
             */
            RTUINT  cbLeft = cbEffLimit + 1 - sizeof(X86DESC);
            RTGCPTR GCPtrSrc = (RTGCPTR)GDTR.pGdt + sizeof(X86DESC);
            uint8_t *pu8Dst = (uint8_t *)&pVM->selm.s.paGdtR3[1];
            uint8_t *pu8DstInvalid = pu8Dst;

            while (cbLeft)
            {
                RTUINT cb = PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK);
                cb = RT_MIN(cb, cbLeft);
                rc = PGMPhysSimpleReadGCPtr(pVCpu, pu8Dst, GCPtrSrc, cb);
                if (RT_SUCCESS(rc))
                {
                    if (pu8DstInvalid != pu8Dst)
                        memset(pu8DstInvalid, 0, pu8Dst - pu8DstInvalid);
                    GCPtrSrc += cb;
                    pu8Dst += cb;
                    pu8DstInvalid = pu8Dst;
                }
                else if (   rc == VERR_PAGE_NOT_PRESENT
                         || rc == VERR_PAGE_TABLE_NOT_PRESENT)
                {
                    GCPtrSrc += cb;
                    pu8Dst += cb;
                }
                else
                {
                    AssertReleaseMsgFailed(("Couldn't read GDT at %016RX64, rc=%Rrc!\n", GDTR.pGdt, rc));
                    STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
                    return VERR_NOT_IMPLEMENTED;
                }
                cbLeft -= cb;
            }

            /* any invalid pages at the end? */
            if (pu8DstInvalid != pu8Dst)
            {
                cbEffLimit = pu8DstInvalid - (uint8_t *)pVM->selm.s.paGdtR3 - 1;
                /* If any GDTEs was invalidated, zero them. */
                if (cbEffLimit < pVM->selm.s.cbEffGuestGdtLimit)
                    memset(pu8DstInvalid + cbEffLimit + 1, 0, pVM->selm.s.cbEffGuestGdtLimit - cbEffLimit);
            }

            /* keep track of the effective limit. */
            if (cbEffLimit != pVM->selm.s.cbEffGuestGdtLimit)
            {
                Log(("SELMR3UpdateFromCPUM: cbEffGuestGdtLimit=%#x -> %#x (actual %#x)\n",
                     pVM->selm.s.cbEffGuestGdtLimit, cbEffLimit, GDTR.cbGdt));
                pVM->selm.s.cbEffGuestGdtLimit = cbEffLimit;
            }
        }

        /*
         * Check if the Guest GDT intrudes on our GDT entries.
         */
        /** @todo we should try to minimize relocations by making sure our current selectors can be reused. */
        RTSEL aHyperSel[SELM_HYPER_SEL_MAX];
        if (cbEffLimit >= SELM_HYPER_DEFAULT_BASE)
        {
            PX86DESC pGDTEStart = pVM->selm.s.paGdtR3;
            PX86DESC pGDTE = (PX86DESC)((char *)pGDTEStart + GDTR.cbGdt + 1 - sizeof(X86DESC));
            int       iGDT = 0;

            Log(("Internal SELM GDT conflict: use non-present entries\n"));
            STAM_COUNTER_INC(&pVM->selm.s.StatScanForHyperSels);
            while (pGDTE > pGDTEStart)
            {
                /* We can reuse non-present entries */
                if (!pGDTE->Gen.u1Present)
                {
                    aHyperSel[iGDT] = ((uintptr_t)pGDTE - (uintptr_t)pVM->selm.s.paGdtR3) / sizeof(X86DESC);
                    aHyperSel[iGDT] = aHyperSel[iGDT] << X86_SEL_SHIFT;
                    Log(("SELM: Found unused GDT %04X\n", aHyperSel[iGDT]));
                    iGDT++;
                    if (iGDT >= SELM_HYPER_SEL_MAX)
                        break;
                }

                pGDTE--;
            }
            if (iGDT != SELM_HYPER_SEL_MAX)
            {
                AssertReleaseMsgFailed(("Internal SELM GDT conflict.\n"));
                STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
                return VERR_NOT_IMPLEMENTED;
            }
        }
        else
        {
            aHyperSel[SELM_HYPER_SEL_CS]         = SELM_HYPER_DEFAULT_SEL_CS;
            aHyperSel[SELM_HYPER_SEL_DS]         = SELM_HYPER_DEFAULT_SEL_DS;
            aHyperSel[SELM_HYPER_SEL_CS64]       = SELM_HYPER_DEFAULT_SEL_CS64;
            aHyperSel[SELM_HYPER_SEL_TSS]        = SELM_HYPER_DEFAULT_SEL_TSS;
            aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] = SELM_HYPER_DEFAULT_SEL_TSS_TRAP08;
        }

        /*
         * Work thru the copied GDT entries adjusting them for correct virtualization.
         */
        PX86DESC pGDTEEnd = (PX86DESC)((char *)pGDTE + cbEffLimit + 1 - sizeof(X86DESC));
        while (pGDTE < pGDTEEnd)
        {
            if (pGDTE->Gen.u1Present)
            {
                /*
                 * Code and data selectors are generally 1:1, with the
                 * 'little' adjustment we do for DPL 0 selectors.
                 */
                if (pGDTE->Gen.u1DescType)
                {
                    /*
                     * Hack for A-bit against Trap E on read-only GDT.
                     */
                    /** @todo Fix this by loading ds and cs before turning off WP. */
                    pGDTE->Gen.u4Type |= X86_SEL_TYPE_ACCESSED;

                    /*
                     * All DPL 0 code and data segments are squeezed into DPL 1.
                     *
                     * We're skipping conforming segments here because those
                     * cannot give us any trouble.
                     */
                    if (    pGDTE->Gen.u2Dpl == 0
                        &&      (pGDTE->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
                            !=  (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF) )
                        pGDTE->Gen.u2Dpl = 1;
                }
                else
                {
                    /*
                     * System type selectors are marked not present.
                     * Recompiler or special handling is required for these.
                     */
                    /** @todo what about interrupt gates and rawr0? */
                    pGDTE->Gen.u1Present = 0;
                }
            }

            /* Next GDT entry. */
            pGDTE++;
        }

        /*
         * Check if our hypervisor selectors were changed.
         */
        if (    aHyperSel[SELM_HYPER_SEL_CS]         != pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS]
            ||  aHyperSel[SELM_HYPER_SEL_DS]         != pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]
            ||  aHyperSel[SELM_HYPER_SEL_CS64]       != pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64]
            ||  aHyperSel[SELM_HYPER_SEL_TSS]        != pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]
            ||  aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] != pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08])
        {
            /* Reinitialize our hypervisor GDTs */
            pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS]         = aHyperSel[SELM_HYPER_SEL_CS];
            pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]         = aHyperSel[SELM_HYPER_SEL_DS];
            pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64]       = aHyperSel[SELM_HYPER_SEL_CS64];
            pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]        = aHyperSel[SELM_HYPER_SEL_TSS];
            pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] = aHyperSel[SELM_HYPER_SEL_TSS_TRAP08];

            STAM_COUNTER_INC(&pVM->selm.s.StatHyperSelsChanged);

            /*
             * Do the relocation callbacks to let everyone update their hyper selector dependencies.
             * (SELMR3Relocate will call selmR3SetupHyperGDTSelectors() for us.)
             */
            VMR3Relocate(pVM, 0);
        }
        else if (cbEffLimit >= SELM_HYPER_DEFAULT_BASE)
            /* We overwrote all entries above, so we have to save them again. */
            selmR3SetupHyperGDTSelectors(pVM);

        /*
         * Adjust the cached GDT limit.
         * Any GDT entries which have been removed must be cleared.
         */
        if (pVM->selm.s.GuestGdtr.cbGdt != GDTR.cbGdt)
        {
            if (pVM->selm.s.GuestGdtr.cbGdt > GDTR.cbGdt)
                memset(pGDTE, 0, pVM->selm.s.GuestGdtr.cbGdt - GDTR.cbGdt);
#ifndef SELM_TRACK_GUEST_GDT_CHANGES
            pVM->selm.s.GuestGdtr.cbGdt = GDTR.cbGdt;
#endif
        }

#ifdef SELM_TRACK_GUEST_GDT_CHANGES
        /*
         * Check if Guest's GDTR is changed.
         */
        if (    GDTR.pGdt != pVM->selm.s.GuestGdtr.pGdt
            ||  GDTR.cbGdt != pVM->selm.s.GuestGdtr.cbGdt)
        {
            Log(("SELMR3UpdateFromCPUM: Guest's GDT is changed to pGdt=%016RX64 cbGdt=%08X\n", GDTR.pGdt, GDTR.cbGdt));

            /*
             * [Re]Register write virtual handler for guest's GDT.
             */
            if (pVM->selm.s.GuestGdtr.pGdt != RTRCPTR_MAX && pVM->selm.s.fGDTRangeRegistered)
            {
                rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GuestGdtr.pGdt);
                AssertRC(rc);
            }

            rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_WRITE, GDTR.pGdt, GDTR.pGdt + GDTR.cbGdt /* already inclusive */,
                                             0, selmR3GuestGDTWriteHandler, "selmRCGuestGDTWriteHandler", 0, "Guest GDT write access handler");
            if (RT_FAILURE(rc))
                return rc;

            /* Update saved Guest GDTR. */
            pVM->selm.s.GuestGdtr = GDTR;
            pVM->selm.s.fGDTRangeRegistered = true;
        }
#endif
    }

    /*
     * TSS sync
     */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_SELM_SYNC_TSS))
    {
        SELMR3SyncTSS(pVM, pVCpu);
    }

    /*
     * LDT sync
     */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_SELM_SYNC_LDT))
    {
        /*
         * Always assume the best
         */
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_LDT);

        /*
         * LDT handling is done similarly to the GDT handling with a shadow
         * array. However, since the LDT is expected to be swappable (at least
         * some ancient OSes makes it swappable) it must be floating and
         * synced on a per-page basis.
         *
         * Eventually we will change this to be fully on demand. Meaning that
         * we will only sync pages containing LDT selectors actually used and
         * let the #PF handler lazily sync pages as they are used.
         * (This applies to GDT too, when we start making OS/2 fast.)
         */

        /*
         * First, determin the current LDT selector.
         */
        RTSEL SelLdt = CPUMGetGuestLDTR(pVCpu);
        if ((SelLdt & X86_SEL_MASK) == 0)
        {
            /* ldtr = 0 - update hyper LDTR and deregister any active handler. */
            CPUMSetHyperLDTR(pVCpu, 0);
#ifdef SELM_TRACK_GUEST_LDT_CHANGES
            if (pVM->selm.s.GCPtrGuestLdt != RTRCPTR_MAX)
            {
                rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestLdt);
                AssertRC(rc);
                pVM->selm.s.GCPtrGuestLdt = RTRCPTR_MAX;
            }
#endif
            STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
            return VINF_SUCCESS;
        }

        /*
         * Get the LDT selector.
         */
        PX86DESC    pDesc = &pVM->selm.s.paGdtR3[SelLdt >> X86_SEL_SHIFT];
        RTGCPTR     GCPtrLdt = X86DESC_BASE(*pDesc);
        unsigned    cbLdt = X86DESC_LIMIT(*pDesc);
        if (pDesc->Gen.u1Granularity)
            cbLdt = (cbLdt << PAGE_SHIFT) | PAGE_OFFSET_MASK;

        /*
         * Validate it.
         */
        if (    !cbLdt
            ||  SelLdt >= pVM->selm.s.GuestGdtr.cbGdt
            ||  pDesc->Gen.u1DescType
            ||  pDesc->Gen.u4Type != X86_SEL_TYPE_SYS_LDT)
        {
            AssertMsg(!cbLdt, ("Invalid LDT %04x!\n", SelLdt));

            /* cbLdt > 0:
             * This is quite impossible, so we do as most people do when faced with
             * the impossible, we simply ignore it.
             */
            CPUMSetHyperLDTR(pVCpu, 0);
#ifdef SELM_TRACK_GUEST_LDT_CHANGES
            if (pVM->selm.s.GCPtrGuestLdt != RTRCPTR_MAX)
            {
                rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestLdt);
                AssertRC(rc);
                pVM->selm.s.GCPtrGuestLdt = RTRCPTR_MAX;
            }
#endif
            STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
            return VINF_SUCCESS;
        }
        /** @todo check what intel does about odd limits. */
        AssertMsg(RT_ALIGN(cbLdt + 1, sizeof(X86DESC)) == cbLdt + 1 && cbLdt <= 0xffff, ("cbLdt=%d\n", cbLdt));

        /*
         * Use the cached guest ldt address if the descriptor has already been modified (see below)
         * (this is necessary due to redundant LDT updates; see todo above at GDT sync)
         */
        if (MMHyperIsInsideArea(pVM, GCPtrLdt))
            GCPtrLdt = pVM->selm.s.GCPtrGuestLdt;   /* use the old one */


#ifdef SELM_TRACK_GUEST_LDT_CHANGES
        /** @todo Handle only present LDT segments. */
    //    if (pDesc->Gen.u1Present)
        {
            /*
             * Check if Guest's LDT address/limit is changed.
             */
            if (    GCPtrLdt != pVM->selm.s.GCPtrGuestLdt
                ||  cbLdt != pVM->selm.s.cbLdtLimit)
            {
                Log(("SELMR3UpdateFromCPUM: Guest LDT changed to from %RGv:%04x to %RGv:%04x. (GDTR=%016RX64:%04x)\n",
                     pVM->selm.s.GCPtrGuestLdt, pVM->selm.s.cbLdtLimit, GCPtrLdt, cbLdt, pVM->selm.s.GuestGdtr.pGdt, pVM->selm.s.GuestGdtr.cbGdt));

                /*
                 * [Re]Register write virtual handler for guest's GDT.
                 * In the event of LDT overlapping something, don't install it just assume it's being updated.
                 */
                if (pVM->selm.s.GCPtrGuestLdt != RTRCPTR_MAX)
                {
                    rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestLdt);
                    AssertRC(rc);
                }
#ifdef DEBUG
                if (pDesc->Gen.u1Present)
                    Log(("LDT selector marked not present!!\n"));
#endif
                rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_WRITE, GCPtrLdt, GCPtrLdt + cbLdt /* already inclusive */,
                                                 0, selmR3GuestLDTWriteHandler, "selmRCGuestLDTWriteHandler", 0, "Guest LDT write access handler");
                if (rc == VERR_PGM_HANDLER_VIRTUAL_CONFLICT)
                {
                    /** @todo investigate the various cases where conflicts happen and try avoid them by enh. the instruction emulation. */
                    pVM->selm.s.GCPtrGuestLdt = RTRCPTR_MAX;
                    Log(("WARNING: Guest LDT (%RGv:%04x) conflicted with existing access range!! Assumes LDT is begin updated. (GDTR=%016RX64:%04x)\n",
                         GCPtrLdt, cbLdt, pVM->selm.s.GuestGdtr.pGdt, pVM->selm.s.GuestGdtr.cbGdt));
                }
                else if (RT_SUCCESS(rc))
                    pVM->selm.s.GCPtrGuestLdt = GCPtrLdt;
                else
                {
                    CPUMSetHyperLDTR(pVCpu, 0);
                    STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
                    return rc;
                }

                pVM->selm.s.cbLdtLimit = cbLdt;
            }
        }
#else
        pVM->selm.s.cbLdtLimit = cbLdt;
#endif

        /*
         * Calc Shadow LDT base.
         */
        unsigned    off;
        pVM->selm.s.offLdtHyper = off = (GCPtrLdt & PAGE_OFFSET_MASK);
        RTGCPTR     GCPtrShadowLDT  = (RTGCPTR)((RTGCUINTPTR)pVM->selm.s.pvLdtRC + off);
        PX86DESC    pShadowLDT      = (PX86DESC)((uintptr_t)pVM->selm.s.pvLdtR3 + off);

        /*
         * Enable the LDT selector in the shadow GDT.
         */
        pDesc->Gen.u1Present    = 1;
        pDesc->Gen.u16BaseLow   = RT_LOWORD(GCPtrShadowLDT);
        pDesc->Gen.u8BaseHigh1  = RT_BYTE3(GCPtrShadowLDT);
        pDesc->Gen.u8BaseHigh2  = RT_BYTE4(GCPtrShadowLDT);
        pDesc->Gen.u1Available  = 0;
        pDesc->Gen.u1Long       = 0;
        if (cbLdt > 0xffff)
        {
            cbLdt = 0xffff;
            pDesc->Gen.u4LimitHigh  = 0;
            pDesc->Gen.u16LimitLow  = pDesc->Gen.u1Granularity ? 0xf : 0xffff;
        }

        /*
         * Set Hyper LDTR and notify TRPM.
         */
        CPUMSetHyperLDTR(pVCpu, SelLdt);

        /*
         * Loop synchronising the LDT page by page.
         */
        /** @todo investigate how intel handle various operations on half present cross page entries. */
        off = GCPtrLdt & (sizeof(X86DESC) - 1);
        AssertMsg(!off, ("LDT is not aligned on entry size! GCPtrLdt=%08x\n", GCPtrLdt));

        /* Note: Do not skip the first selector; unlike the GDT, a zero LDT selector is perfectly valid. */
        unsigned    cbLeft = cbLdt + 1;
        PX86DESC   pLDTE = pShadowLDT;
        while (cbLeft)
        {
            /*
             * Read a chunk.
             */
            unsigned  cbChunk = PAGE_SIZE - ((RTGCUINTPTR)GCPtrLdt & PAGE_OFFSET_MASK);
            if (cbChunk > cbLeft)
                cbChunk = cbLeft;
            rc = PGMPhysSimpleReadGCPtr(pVCpu, pShadowLDT, GCPtrLdt, cbChunk);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Mark page
                 */
                rc = PGMMapSetPage(pVM, GCPtrShadowLDT & PAGE_BASE_GC_MASK, PAGE_SIZE, X86_PTE_P | X86_PTE_A | X86_PTE_D);
                AssertRC(rc);

                /*
                 * Loop thru the available LDT entries.
                 * Figure out where to start and end and the potential cross pageness of
                 * things adds a little complexity. pLDTE is updated there and not in the
                 * 'next' part of the loop. The pLDTEEnd is inclusive.
                 */
                PX86DESC pLDTEEnd = (PX86DESC)((uintptr_t)pShadowLDT + cbChunk) - 1;
                if (pLDTE + 1 < pShadowLDT)
                    pLDTE = (PX86DESC)((uintptr_t)pShadowLDT + off);
                while (pLDTE <= pLDTEEnd)
                {
                    if (pLDTE->Gen.u1Present)
                    {
                        /*
                         * Code and data selectors are generally 1:1, with the
                         * 'little' adjustment we do for DPL 0 selectors.
                         */
                        if (pLDTE->Gen.u1DescType)
                        {
                            /*
                             * Hack for A-bit against Trap E on read-only GDT.
                             */
                            /** @todo Fix this by loading ds and cs before turning off WP. */
                            if (!(pLDTE->Gen.u4Type & X86_SEL_TYPE_ACCESSED))
                                pLDTE->Gen.u4Type |= X86_SEL_TYPE_ACCESSED;

                            /*
                             * All DPL 0 code and data segments are squeezed into DPL 1.
                             *
                             * We're skipping conforming segments here because those
                             * cannot give us any trouble.
                             */
                            if (    pLDTE->Gen.u2Dpl == 0
                                &&      (pLDTE->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
                                    !=  (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF) )
                                pLDTE->Gen.u2Dpl = 1;
                        }
                        else
                        {
                            /*
                             * System type selectors are marked not present.
                             * Recompiler or special handling is required for these.
                             */
                            /** @todo what about interrupt gates and rawr0? */
                            pLDTE->Gen.u1Present = 0;
                        }
                    }

                    /* Next LDT entry. */
                    pLDTE++;
                }
            }
            else
            {
                AssertMsg(rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc=%Rrc\n", rc));
                rc = PGMMapSetPage(pVM, GCPtrShadowLDT & PAGE_BASE_GC_MASK, PAGE_SIZE, 0);
                AssertRC(rc);
            }

            /*
             * Advance to the next page.
             */
            cbLeft          -= cbChunk;
            GCPtrShadowLDT  += cbChunk;
            pShadowLDT       = (PX86DESC)((char *)pShadowLDT + cbChunk);
            GCPtrLdt        += cbChunk;
        }
    }

    STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
    return VINF_SUCCESS;
}


/**
 * \#PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   pvPtr           The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) selmR3GuestGDTWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    Log(("selmR3GuestGDTWriteHandler: write to %RGv size %d\n", GCPtr, cbBuf));

    VMCPU_FF_SET(VMMGetCpu(pVM), VMCPU_FF_SELM_SYNC_GDT);
    return VINF_PGM_HANDLER_DO_DEFAULT;
}


/**
 * \#PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   pvPtr           The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) selmR3GuestLDTWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    Log(("selmR3GuestLDTWriteHandler: write to %RGv size %d\n", GCPtr, cbBuf));
    VMCPU_FF_SET(VMMGetCpu(pVM), VMCPU_FF_SELM_SYNC_LDT);
    return VINF_PGM_HANDLER_DO_DEFAULT;
}


/**
 * \#PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   pvPtr           The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) selmR3GuestTSSWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    Log(("selmR3GuestTSSWriteHandler: write %.*Rhxs to %RGv size %d\n", RT_MIN(8, cbBuf), pvBuf, GCPtr, cbBuf));

    /** @todo This can be optimized by checking for the ESP0 offset and tracking TR
     *        reloads in REM (setting VM_FF_SELM_SYNC_TSS if TR is reloaded). We
     *        should probably also deregister the virtual handler if TR.base/size
     *        changes while we're in REM. */

    VMCPU_FF_SET(VMMGetCpu(pVM), VMCPU_FF_SELM_SYNC_TSS);

    return VINF_PGM_HANDLER_DO_DEFAULT;
}


/**
 * Synchronize the shadowed fields in the TSS.
 *
 * At present we're shadowing the ring-0 stack selector & pointer, and the
 * interrupt redirection bitmap (if present). We take the lazy approach wrt to
 * REM and this function is called both if REM made any changes to the TSS or
 * loaded TR.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR3DECL(int) SELMR3SyncTSS(PVM pVM, PVMCPU pVCpu)
{
    int    rc;

    if (pVM->selm.s.fDisableMonitoring)
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_TSS);
        return VINF_SUCCESS;
    }

    STAM_PROFILE_START(&pVM->selm.s.StatTSSSync, a);
    Assert(VMCPU_FF_ISSET(pVCpu, VMCPU_FF_SELM_SYNC_TSS));

    /*
     * Get TR and extract and store the basic info.
     *
     * Note! The TSS limit is not checked by the LTR code, so we
     *       have to be a bit careful with it. We make sure cbTss
     *       won't be zero if TR is valid and if it's NULL we'll
     *       make sure cbTss is 0.
     */
    CPUMSELREGHID   trHid;
    RTSEL           SelTss   = CPUMGetGuestTR(pVCpu, &trHid);
    RTGCPTR         GCPtrTss = trHid.u64Base;
    uint32_t        cbTss    = trHid.u32Limit;
    Assert(     (SelTss & X86_SEL_MASK)
           ||   (cbTss == 0 && GCPtrTss == 0 && trHid.Attr.u == 0 /* TR=0 */)
           ||   (cbTss == 0xffff && GCPtrTss == 0 && trHid.Attr.n.u1Present && trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY /* RESET */));
    if (SelTss & X86_SEL_MASK)
    {
        Assert(!(SelTss & X86_SEL_LDT));
        Assert(trHid.Attr.n.u1DescType == 0);
        Assert(   trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY
               || trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY);
        if (!++cbTss)
            cbTss = UINT32_MAX;
    }
    else
    {
        Assert(   (cbTss == 0 && GCPtrTss == 0 && trHid.Attr.u == 0 /* TR=0 */)
               || (cbTss == 0xffff && GCPtrTss == 0 && trHid.Attr.n.u1Present && trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY /* RESET */));
        cbTss = 0; /* the reset case. */
    }
    pVM->selm.s.cbGuestTss     = cbTss;
    pVM->selm.s.fGuestTss32Bit = trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_AVAIL
                              || trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY;

    /*
     * Figure out the size of what need to monitor.
     */
    /* We're not interested in any 16-bit TSSes. */
    uint32_t cbMonitoredTss = cbTss;
    if (    trHid.Attr.n.u4Type != X86_SEL_TYPE_SYS_386_TSS_AVAIL
        &&  trHid.Attr.n.u4Type != X86_SEL_TYPE_SYS_386_TSS_BUSY)
        cbMonitoredTss = 0;

    pVM->selm.s.offGuestIoBitmap = 0;
    bool fNoRing1Stack = true;
    if (cbMonitoredTss)
    {
        /*
         * 32-bit TSS. What we're really keen on is the SS0 and ESP0 fields.
         * If VME is enabled we also want to keep an eye on the interrupt
         * redirection bitmap.
         */
        VBOXTSS Tss;
        uint32_t cr4 = CPUMGetGuestCR4(pVCpu);
        rc = PGMPhysSimpleReadGCPtr(pVCpu, &Tss, GCPtrTss, RT_OFFSETOF(VBOXTSS, IntRedirBitmap));
        if (    !(cr4 & X86_CR4_VME)
            ||   (  VBOX_SUCCESS(rc)
                 && Tss.offIoBitmap < sizeof(VBOXTSS) /* too small */
                 && Tss.offIoBitmap > cbTss)          /* beyond the end */ /** @todo not sure how the partial case is handled; probably not allowed. */
           )
            /* No interrupt redirection bitmap, just ESP0 and SS0. */
            cbMonitoredTss = RT_UOFFSETOF(VBOXTSS, padding_ss0);
        else if (RT_SUCCESS(rc))
        {
            /*
             * Everything up to and including the interrupt redirection bitmap. Unfortunately
             * this can be quite a large chunk. We use to skip it earlier and just hope it
             * was kind of static...
             *
             * Update the virtual interrupt redirection bitmap while we're here.
             * (It is located in the 32 bytes before TR:offIoBitmap.)
             */
            cbMonitoredTss = Tss.offIoBitmap;
            pVM->selm.s.offGuestIoBitmap = Tss.offIoBitmap;

            uint32_t offRedirBitmap = Tss.offIoBitmap - sizeof(Tss.IntRedirBitmap);
            rc = PGMPhysSimpleReadGCPtr(pVCpu, &pVM->selm.s.Tss.IntRedirBitmap,
                                        GCPtrTss + offRedirBitmap, sizeof(Tss.IntRedirBitmap));
            AssertRC(rc);
            /** @todo memset the bitmap on failure? */
            Log2(("Redirection bitmap:\n"));
            Log2(("%.*Rhxd\n", sizeof(Tss.IntRedirBitmap), &pVM->selm.s.Tss.IntRedirBitmap));
        }
        else
        {
            cbMonitoredTss = RT_OFFSETOF(VBOXTSS, IntRedirBitmap);
            pVM->selm.s.offGuestIoBitmap = 0;
            /** @todo memset the bitmap? */
        }

        /*
         * Update the ring 0 stack selector and base address.
         */
        if (RT_SUCCESS(rc))
        {
#ifdef LOG_ENABLED
            if (LogIsEnabled())
            {
                uint32_t ssr0, espr0;
                SELMGetRing1Stack(pVM, &ssr0, &espr0);
                if ((ssr0 & ~1) != Tss.ss0 || espr0 != Tss.esp0)
                {
                    RTGCPHYS GCPhys = NIL_RTGCPHYS;
                    rc = PGMGstGetPage(pVCpu, GCPtrTss, NULL, &GCPhys); AssertRC(rc);
                    Log(("SELMR3SyncTSS: Updating TSS ring 0 stack to %04X:%08X from %04X:%08X; TSS Phys=%VGp)\n",
                         Tss.ss0, Tss.esp0, (ssr0 & ~1), espr0,  GCPhys));
                    AssertMsg(ssr0 != Tss.ss0,
                              ("ring-1 leak into TSS.SS0! %04X:%08X from %04X:%08X; TSS Phys=%VGp)\n",
                               Tss.ss0, Tss.esp0, (ssr0 & ~1), espr0,  GCPhys));
                }
                Log(("offIoBitmap=%#x\n", Tss.offIoBitmap));
            }
#endif /* LOG_ENABLED */
            AssertMsg(!(Tss.ss0 & 3), ("ring-1 leak into TSS.SS0? %04X:%08X\n", Tss.ss0, Tss.esp0));

            /* Update our TSS structure for the guest's ring 1 stack */
            selmSetRing1Stack(pVM, Tss.ss0 | 1, Tss.esp0);
            pVM->selm.s.fSyncTSSRing0Stack = fNoRing1Stack = false;
        }
    }

    /*
     * Flush the ring-1 stack and the direct syscall dispatching if we
     * cannot obtain SS0:ESP0.
     */
    if (fNoRing1Stack)
    {
        selmSetRing1Stack(pVM, 0 /* invalid SS */, 0);
        pVM->selm.s.fSyncTSSRing0Stack = cbMonitoredTss != 0;

        /** @todo handle these dependencies better! */
        TRPMR3SetGuestTrapHandler(pVM, 0x2E, TRPM_INVALID_HANDLER);
        TRPMR3SetGuestTrapHandler(pVM, 0x80, TRPM_INVALID_HANDLER);
    }

    /*
     * Check for monitor changes and apply them.
     */
    if (    GCPtrTss        != pVM->selm.s.GCPtrGuestTss
        ||  cbMonitoredTss  != pVM->selm.s.cbMonitoredGuestTss)
    {
        Log(("SELMR3SyncTSS: Guest's TSS is changed to pTss=%RGv cbMonitoredTss=%08X cbGuestTss=%#08x\n",
             GCPtrTss, cbMonitoredTss, pVM->selm.s.cbGuestTss));

        /* Release the old range first. */
        if (pVM->selm.s.GCPtrGuestTss != RTRCPTR_MAX)
        {
            rc = PGMHandlerVirtualDeregister(pVM, pVM->selm.s.GCPtrGuestTss);
            AssertRC(rc);
        }

        /* Register the write handler if TS != 0. */
        if (cbMonitoredTss != 0)
        {
            rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_WRITE, GCPtrTss, GCPtrTss + cbMonitoredTss - 1,
                                             0, selmR3GuestTSSWriteHandler,
                                             "selmRCGuestTSSWriteHandler", 0, "Guest TSS write access handler");
            if (RT_FAILURE(rc))
            {
                STAM_PROFILE_STOP(&pVM->selm.s.StatUpdateFromCPUM, a);
                return rc;
            }

            /* Update saved Guest TSS info. */
            pVM->selm.s.GCPtrGuestTss       = GCPtrTss;
            pVM->selm.s.cbMonitoredGuestTss = cbMonitoredTss;
            pVM->selm.s.GCSelTss            = SelTss;
        }
        else
        {
            pVM->selm.s.GCPtrGuestTss       = RTRCPTR_MAX;
            pVM->selm.s.cbMonitoredGuestTss = 0;
            pVM->selm.s.GCSelTss            = 0;
        }
    }

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

    STAM_PROFILE_STOP(&pVM->selm.s.StatTSSSync, a);
    return VINF_SUCCESS;
}


/**
 * Compares the Guest GDT and LDT with the shadow tables.
 * This is a VBOX_STRICT only function.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle.
 */
VMMR3DECL(int) SELMR3DebugCheck(PVM pVM)
{
#ifdef VBOX_STRICT
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /*
     * Get GDTR and check for conflict.
     */
    VBOXGDTR  GDTR;
    CPUMGetGuestGDTR(pVCpu, &GDTR);
    if (GDTR.cbGdt == 0)
        return VINF_SUCCESS;

    if (GDTR.cbGdt >= (unsigned)(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] >> X86_SEL_SHIFT))
        Log(("SELMR3DebugCheck: guest GDT size forced us to look for unused selectors.\n"));

    if (GDTR.cbGdt != pVM->selm.s.GuestGdtr.cbGdt)
        Log(("SELMR3DebugCheck: limits have changed! new=%d old=%d\n", GDTR.cbGdt, pVM->selm.s.GuestGdtr.cbGdt));

    /*
     * Loop thru the GDT checking each entry.
     */
    RTGCPTR     GCPtrGDTEGuest = GDTR.pGdt;
    PX86DESC    pGDTE = pVM->selm.s.paGdtR3;
    PX86DESC    pGDTEEnd = (PX86DESC)((uintptr_t)pGDTE + GDTR.cbGdt);
    while (pGDTE < pGDTEEnd)
    {
        X86DESC    GDTEGuest;
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &GDTEGuest, GCPtrGDTEGuest, sizeof(GDTEGuest));
        if (RT_SUCCESS(rc))
        {
            if (pGDTE->Gen.u1DescType || pGDTE->Gen.u4Type != X86_SEL_TYPE_SYS_LDT)
            {
                if (   pGDTE->Gen.u16LimitLow != GDTEGuest.Gen.u16LimitLow
                    || pGDTE->Gen.u4LimitHigh != GDTEGuest.Gen.u4LimitHigh
                    || pGDTE->Gen.u16BaseLow  != GDTEGuest.Gen.u16BaseLow
                    || pGDTE->Gen.u8BaseHigh1 != GDTEGuest.Gen.u8BaseHigh1
                    || pGDTE->Gen.u8BaseHigh2 != GDTEGuest.Gen.u8BaseHigh2
                    || pGDTE->Gen.u1DefBig    != GDTEGuest.Gen.u1DefBig
                    || pGDTE->Gen.u1DescType  != GDTEGuest.Gen.u1DescType)
                {
                    unsigned iGDT = pGDTE - pVM->selm.s.paGdtR3;
                    SELMR3DumpDescriptor(*pGDTE, iGDT << 3, "SELMR3DebugCheck: GDT mismatch, shadow");
                    SELMR3DumpDescriptor(GDTEGuest, iGDT << 3, "SELMR3DebugCheck: GDT mismatch,  guest");
                }
            }
        }

        /* Advance to the next descriptor. */
        GCPtrGDTEGuest += sizeof(X86DESC);
        pGDTE++;
    }


    /*
     * LDT?
     */
    RTSEL SelLdt = CPUMGetGuestLDTR(pVCpu);
    if ((SelLdt & X86_SEL_MASK) == 0)
        return VINF_SUCCESS;
    if (SelLdt > GDTR.cbGdt)
    {
        Log(("SELMR3DebugCheck: ldt is out of bound SelLdt=%#x\n", SelLdt));
        return VERR_INTERNAL_ERROR;
    }
    X86DESC    LDTDesc;
    int rc = PGMPhysSimpleReadGCPtr(pVCpu, &LDTDesc, GDTR.pGdt + (SelLdt & X86_SEL_MASK), sizeof(LDTDesc));
    if (RT_FAILURE(rc))
    {
        Log(("SELMR3DebugCheck: Failed to read LDT descriptor. rc=%d\n", rc));
        return rc;
    }
    RTGCPTR     GCPtrLDTEGuest = X86DESC_BASE(LDTDesc);
    unsigned    cbLdt = X86DESC_LIMIT(LDTDesc);
    if (LDTDesc.Gen.u1Granularity)
        cbLdt = (cbLdt << PAGE_SHIFT) | PAGE_OFFSET_MASK;

    /*
     * Validate it.
     */
    if (!cbLdt)
        return VINF_SUCCESS;
    /** @todo check what intel does about odd limits. */
    AssertMsg(RT_ALIGN(cbLdt + 1, sizeof(X86DESC)) == cbLdt + 1 && cbLdt <= 0xffff, ("cbLdt=%d\n", cbLdt));
    if (    LDTDesc.Gen.u1DescType
        ||  LDTDesc.Gen.u4Type != X86_SEL_TYPE_SYS_LDT
        ||  SelLdt >= pVM->selm.s.GuestGdtr.cbGdt)
    {
        Log(("SELmR3DebugCheck: Invalid LDT %04x!\n", SelLdt));
        return VERR_INTERNAL_ERROR;
    }

    /*
     * Loop thru the LDT checking each entry.
     */
    unsigned    off = (GCPtrLDTEGuest & PAGE_OFFSET_MASK);
    PX86DESC    pLDTE = (PX86DESC)((uintptr_t)pVM->selm.s.pvLdtR3 + off);
    PX86DESC    pLDTEEnd = (PX86DESC)((uintptr_t)pGDTE + cbLdt);
    while (pLDTE < pLDTEEnd)
    {
        X86DESC    LDTEGuest;
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &LDTEGuest, GCPtrLDTEGuest, sizeof(LDTEGuest));
        if (RT_SUCCESS(rc))
        {
            if (   pLDTE->Gen.u16LimitLow != LDTEGuest.Gen.u16LimitLow
                || pLDTE->Gen.u4LimitHigh != LDTEGuest.Gen.u4LimitHigh
                || pLDTE->Gen.u16BaseLow  != LDTEGuest.Gen.u16BaseLow
                || pLDTE->Gen.u8BaseHigh1 != LDTEGuest.Gen.u8BaseHigh1
                || pLDTE->Gen.u8BaseHigh2 != LDTEGuest.Gen.u8BaseHigh2
                || pLDTE->Gen.u1DefBig    != LDTEGuest.Gen.u1DefBig
                || pLDTE->Gen.u1DescType  != LDTEGuest.Gen.u1DescType)
            {
                unsigned iLDT = pLDTE - (PX86DESC)((uintptr_t)pVM->selm.s.pvLdtR3 + off);
                SELMR3DumpDescriptor(*pLDTE, iLDT << 3, "SELMR3DebugCheck: LDT mismatch, shadow");
                SELMR3DumpDescriptor(LDTEGuest, iLDT << 3, "SELMR3DebugCheck: LDT mismatch,  guest");
            }
        }

        /* Advance to the next descriptor. */
        GCPtrLDTEGuest += sizeof(X86DESC);
        pLDTE++;
    }

#else  /* !VBOX_STRICT */
    NOREF(pVM);
#endif /* !VBOX_STRICT */

    return VINF_SUCCESS;
}


/**
 * Validates the RawR0 TSS values against the one in the Guest TSS.
 *
 * @returns true if it matches.
 * @returns false and assertions on mismatch..
 * @param   pVM     VM Handle.
 */
VMMR3DECL(bool) SELMR3CheckTSS(PVM pVM)
{
#ifdef VBOX_STRICT
    PVMCPU pVCpu = VMMGetCpu(pVM);

    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_SELM_SYNC_TSS))
        return true;

    /*
     * Get TR and extract the basic info.
     */
    CPUMSELREGHID   trHid;
    RTSEL           SelTss   = CPUMGetGuestTR(pVCpu, &trHid);
    RTGCPTR         GCPtrTss = trHid.u64Base;
    uint32_t        cbTss    = trHid.u32Limit;
    Assert(     (SelTss & X86_SEL_MASK)
           ||   (cbTss == 0 && GCPtrTss == 0 && trHid.Attr.u == 0 /* TR=0 */)
           ||   (cbTss == 0xffff && GCPtrTss == 0 && trHid.Attr.n.u1Present && trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY /* RESET */));
    if (SelTss & X86_SEL_MASK)
    {
        AssertReturn(!(SelTss & X86_SEL_LDT), false);
        AssertReturn(trHid.Attr.n.u1DescType == 0, false);
        AssertReturn(   trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY
                     || trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY,
                     false);
        if (!++cbTss)
            cbTss = UINT32_MAX;
    }
    else
    {
        AssertReturn(   (cbTss == 0 && GCPtrTss == 0 && trHid.Attr.u == 0 /* TR=0 */)
                     || (cbTss == 0xffff && GCPtrTss == 0 && trHid.Attr.n.u1Present && trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY /* RESET */),
                     false);
        cbTss = 0; /* the reset case. */
    }
    AssertMsgReturn(pVM->selm.s.cbGuestTss == cbTss, ("%#x %#x\n", pVM->selm.s.cbGuestTss, cbTss), false);
    AssertMsgReturn(pVM->selm.s.fGuestTss32Bit == (   trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_AVAIL
                                                   || trHid.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY),
                    ("%RTbool u4Type=%d\n", pVM->selm.s.fGuestTss32Bit, trHid.Attr.n.u4Type),
                    false);
    AssertMsgReturn(    pVM->selm.s.GCSelTss == SelTss
                    ||  (!pVM->selm.s.GCSelTss && !(SelTss & X86_SEL_LDT)),
                    ("%#x %#x\n", pVM->selm.s.GCSelTss, SelTss),
                    false);
    AssertMsgReturn(    pVM->selm.s.GCPtrGuestTss == GCPtrTss
                    ||  (pVM->selm.s.GCPtrGuestTss == RTRCPTR_MAX && !GCPtrTss),
                    ("%#RGv %#RGv\n", pVM->selm.s.GCPtrGuestTss, GCPtrTss),
                    false);


    /*
     * Figure out the size of what need to monitor.
     */
    bool fNoRing1Stack = true;
    /* We're not interested in any 16-bit TSSes. */
    uint32_t cbMonitoredTss = cbTss;
    if (    trHid.Attr.n.u4Type != X86_SEL_TYPE_SYS_386_TSS_AVAIL
        &&  trHid.Attr.n.u4Type != X86_SEL_TYPE_SYS_386_TSS_BUSY)
        cbMonitoredTss = 0;
    if (cbMonitoredTss)
    {
        VBOXTSS Tss;
        uint32_t cr4 = CPUMGetGuestCR4(pVCpu);
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &Tss, GCPtrTss, RT_OFFSETOF(VBOXTSS, IntRedirBitmap));
        AssertReturn(   rc == VINF_SUCCESS
                        /* Happends early in XP boot during page table switching. */
                     || (   (rc == VERR_PAGE_TABLE_NOT_PRESENT || rc == VERR_PAGE_NOT_PRESENT)
                         && !(CPUMGetGuestEFlags(pVCpu) & X86_EFL_IF)),
                     false);
        if (    !(cr4 & X86_CR4_VME)
            ||   (  VBOX_SUCCESS(rc)
                 && Tss.offIoBitmap < sizeof(VBOXTSS) /* too small */
                 && Tss.offIoBitmap > cbTss)
           )
            cbMonitoredTss = RT_UOFFSETOF(VBOXTSS, padding_ss0);
        else if (RT_SUCCESS(rc))
        {
            cbMonitoredTss = Tss.offIoBitmap;
            AssertMsgReturn(pVM->selm.s.offGuestIoBitmap == Tss.offIoBitmap,
                            ("#x %#x\n", pVM->selm.s.offGuestIoBitmap, Tss.offIoBitmap),
                            false);

            /* check the bitmap */
            uint32_t offRedirBitmap = Tss.offIoBitmap - sizeof(Tss.IntRedirBitmap);
            rc = PGMPhysSimpleReadGCPtr(pVCpu, &Tss.IntRedirBitmap,
                                        GCPtrTss + offRedirBitmap, sizeof(Tss.IntRedirBitmap));
            AssertRCReturn(rc, false);
            AssertMsgReturn(!memcmp(&Tss.IntRedirBitmap[0], &pVM->selm.s.Tss.IntRedirBitmap[0], sizeof(Tss.IntRedirBitmap)),
                            ("offIoBitmap=%#x cbTss=%#x\n"
                             " Guest: %.32Rhxs\n"
                             "Shadow: %.32Rhxs\n",
                             Tss.offIoBitmap, cbTss,
                             &Tss.IntRedirBitmap[0],
                             &pVM->selm.s.Tss.IntRedirBitmap[0]),
                            false);
        }
        else
            cbMonitoredTss = RT_OFFSETOF(VBOXTSS, IntRedirBitmap);

        /*
         * Check SS0 and ESP0.
         */
        if (    !pVM->selm.s.fSyncTSSRing0Stack
            &&  RT_SUCCESS(rc))
        {
            if (    Tss.esp0 != pVM->selm.s.Tss.esp1
                ||  Tss.ss0  != (pVM->selm.s.Tss.ss1 & ~1))
            {
                RTGCPHYS GCPhys;
                rc = PGMGstGetPage(pVCpu, GCPtrTss, NULL, &GCPhys); AssertRC(rc);
                AssertMsgFailed(("TSS out of sync!! (%04X:%08X vs %04X:%08X (guest)) Tss=%RGv Phys=%RGp\n",
                                 (pVM->selm.s.Tss.ss1 & ~1), pVM->selm.s.Tss.esp1,
                                 Tss.ss1, Tss.esp1, GCPtrTss, GCPhys));
                return false;
            }
        }
        AssertMsgReturn(pVM->selm.s.cbMonitoredGuestTss == cbMonitoredTss, ("%#x %#x\n", pVM->selm.s.cbMonitoredGuestTss, cbMonitoredTss), false);
    }
    else
    {
        AssertMsgReturn(pVM->selm.s.Tss.ss1 == 0 && pVM->selm.s.Tss.esp1 == 0, ("%04x:%08x\n", pVM->selm.s.Tss.ss1, pVM->selm.s.Tss.esp1), false);
        AssertReturn(!pVM->selm.s.fSyncTSSRing0Stack, false);
        AssertMsgReturn(pVM->selm.s.cbMonitoredGuestTss == cbMonitoredTss, ("%#x %#x\n", pVM->selm.s.cbMonitoredGuestTss, cbMonitoredTss), false);
    }



    return true;

#else  /* !VBOX_STRICT */
    NOREF(pVM);
    return true;
#endif /* !VBOX_STRICT */
}


/**
 * Returns flat address and limit of LDT by LDT selector from guest GDTR.
 *
 * Fully validate selector.
 *
 * @returns VBox status.
 * @param   pVM       VM Handle.
 * @param   SelLdt    LDT selector.
 * @param   ppvLdt    Where to store the flat address of LDT.
 * @param   pcbLimit  Where to store LDT limit.
 */
VMMDECL(int) SELMGetLDTFromSel(PVM pVM, RTSEL SelLdt, PRTGCPTR ppvLdt, unsigned *pcbLimit)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /* Get guest GDTR. */
    VBOXGDTR GDTR;
    CPUMGetGuestGDTR(pVCpu, &GDTR);

    /* Check selector TI and GDT limit. */
    if (    SelLdt & X86_SEL_LDT
        ||  (SelLdt > GDTR.cbGdt))
        return VERR_INVALID_SELECTOR;

    /* Read descriptor from GC. */
    X86DESC Desc;
    int rc = PGMPhysSimpleReadGCPtr(pVCpu, (void *)&Desc, (RTGCPTR)(GDTR.pGdt + (SelLdt & X86_SEL_MASK)), sizeof(Desc));
    if (RT_FAILURE(rc))
    {
        /* fatal */
        AssertMsgFailed(("Can't read LDT descriptor for selector=%04X\n", SelLdt));
        return VERR_SELECTOR_NOT_PRESENT;
    }

    /* Check if LDT descriptor is not present. */
    if (Desc.Gen.u1Present == 0)
        return VERR_SELECTOR_NOT_PRESENT;

    /* Check LDT descriptor type. */
    if (    Desc.Gen.u1DescType == 1
        ||  Desc.Gen.u4Type != X86_SEL_TYPE_SYS_LDT)
        return VERR_INVALID_SELECTOR;

    /* LDT descriptor is ok. */
    if (ppvLdt)
    {
        *ppvLdt = (RTGCPTR)X86DESC_BASE(Desc);
        *pcbLimit = X86DESC_LIMIT(Desc);
    }
    return VINF_SUCCESS;
}


/**
 * Gets information about a 64-bit selector, SELMR3GetSelectorInfo helper.
 *
 * See SELMR3GetSelectorInfo for details.
 *
 * @returns VBox status code, see SELMR3GetSelectorInfo for details.
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   Sel         The selector to get info about.
 * @param   pSelInfo    Where to store the information.
 */
static int selmR3GetSelectorInfo64(PVM pVM, PVMCPU pVCpu, RTSEL Sel, PDBGFSELINFO pSelInfo)
{
    /*
     * Read it from the guest descriptor table.
     */
    X86DESC64   Desc;
    VBOXGDTR    Gdtr;
    RTGCPTR     GCPtrDesc;
    CPUMGetGuestGDTR(pVCpu, &Gdtr);
    if (!(Sel & X86_SEL_LDT))
    {
        /* GDT */
        if ((unsigned)(Sel & X86_SEL_MASK) + sizeof(X86DESC) - 1 > (unsigned)Gdtr.cbGdt)
            return VERR_INVALID_SELECTOR;
        GCPtrDesc = Gdtr.pGdt + (Sel & X86_SEL_MASK);
    }
    else
    {
        /*
         * LDT - must locate the LDT first.
         */
        RTSEL SelLdt = CPUMGetGuestLDTR(pVCpu);
        if (    (unsigned)(SelLdt & X86_SEL_MASK) < sizeof(X86DESC) /* the first selector is invalid, right? */
            ||  (unsigned)(SelLdt & X86_SEL_MASK) + sizeof(X86DESC) - 1 > (unsigned)Gdtr.cbGdt)
            return VERR_INVALID_SELECTOR;
        GCPtrDesc = Gdtr.pGdt + (SelLdt & X86_SEL_MASK);
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &Desc, GCPtrDesc, sizeof(Desc));
        if (RT_FAILURE(rc))
            return rc;

        /* validate the LDT descriptor. */
        if (Desc.Gen.u1Present == 0)
            return VERR_SELECTOR_NOT_PRESENT;
        if (    Desc.Gen.u1DescType == 1
            ||  Desc.Gen.u4Type != AMD64_SEL_TYPE_SYS_LDT)
            return VERR_INVALID_SELECTOR;

        uint32_t cbLimit = X86DESC_LIMIT(Desc);
        if (Desc.Gen.u1Granularity)
            cbLimit = (cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
        if ((uint32_t)(Sel & X86_SEL_MASK) + sizeof(X86DESC) - 1 > cbLimit)
            return VERR_INVALID_SELECTOR;

        /* calc the descriptor location. */
        GCPtrDesc = X86DESC64_BASE(Desc);
        GCPtrDesc += (Sel & X86_SEL_MASK);
    }

    /* read the descriptor. */
    int rc = PGMPhysSimpleReadGCPtr(pVCpu, &Desc, GCPtrDesc, sizeof(Desc));
    if (RT_FAILURE(rc))
    {
        rc = PGMPhysSimpleReadGCPtr(pVCpu, &Desc, GCPtrDesc, sizeof(X86DESC));
        if (RT_FAILURE(rc))
            return rc;
        Desc.au64[1] = 0;
    }

    /*
     * Extract the base and limit
     * (We ignore the present bit here, which is probably a bit silly...)
     */
    pSelInfo->Sel    = Sel;
    pSelInfo->fFlags = DBGFSELINFO_FLAGS_LONG_MODE;
    pSelInfo->Raw64  = Desc;
    if (Desc.Gen.u1DescType)
    {
        if (    Desc.Gen.u1Long
            &&  Desc.Gen.u1DefBig
            &&  (Desc.Gen.u4Type & X86_SEL_TYPE_CODE))
        {
            /* 64-bit code selectors are wide open. It's not possible to
               detect 64-bit data or stack selectors without also dragging
               in assumptions about current CS. So, the selinfo user needs
               to deal with this in the context the info is used unfortunately.
               Note. We ignore the segment limit hacks that was added by AMD. */
            pSelInfo->GCPtrBase = 0;
            pSelInfo->cbLimit   = ~(RTGCUINTPTR)0;
        }
        else
        {
            pSelInfo->cbLimit = X86DESC_LIMIT(Desc);
            if (Desc.Gen.u1Granularity)
                pSelInfo->cbLimit = (pSelInfo->cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
            pSelInfo->GCPtrBase = X86DESC_BASE(Desc);
        }
        pSelInfo->SelGate = 0;
    }
    else if (   Desc.Gen.u4Type == AMD64_SEL_TYPE_SYS_LDT
             || Desc.Gen.u4Type == AMD64_SEL_TYPE_SYS_TSS_AVAIL
             || Desc.Gen.u4Type == AMD64_SEL_TYPE_SYS_TSS_BUSY)
    {
        /* Note. LDT descriptors are weird in long mode, we ignore the footnote
           in the AMD manual here as a simplification. */
        pSelInfo->GCPtrBase = X86DESC64_BASE(Desc);
        pSelInfo->cbLimit = X86DESC_LIMIT(Desc);
        if (Desc.Gen.u1Granularity)
            pSelInfo->cbLimit = (pSelInfo->cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
        pSelInfo->SelGate = 0;
    }
    else if (   Desc.Gen.u4Type == AMD64_SEL_TYPE_SYS_CALL_GATE
             || Desc.Gen.u4Type == AMD64_SEL_TYPE_SYS_TRAP_GATE
             || Desc.Gen.u4Type == AMD64_SEL_TYPE_SYS_INT_GATE)
    {
        pSelInfo->cbLimit   = X86DESC64_BASE(Desc);
        pSelInfo->GCPtrBase = Desc.Gate.u16OffsetLow
                            | ((uint32_t)Desc.Gate.u16OffsetHigh << 16)
                            | ((uint64_t)Desc.Gate.u32OffsetTop << 32);
        pSelInfo->SelGate   = Desc.Gate.u16Sel;
        pSelInfo->fFlags   |= DBGFSELINFO_FLAGS_GATE;
    }
    else
    {
        pSelInfo->cbLimit   = 0;
        pSelInfo->GCPtrBase = 0;
        pSelInfo->SelGate   = 0;
        pSelInfo->fFlags   |= DBGFSELINFO_FLAGS_INVALID;
    }
    if (!Desc.Gen.u1Present)
        pSelInfo->fFlags |= DBGFSELINFO_FLAGS_NOT_PRESENT;

    return VINF_SUCCESS;
}


/**
 * Worker for selmR3GetSelectorInfo32 and SELMR3GetShadowSelectorInfo that
 * interprets a legacy descriptor table entry and fills in the selector info
 * structure from it.
 *
 * @param  pSelInfo     Where to store the selector info. Only the fFlags and
 *                      Sel members have been initialized.
 * @param  pDesc        The legacy descriptor to parse.
 */
DECLINLINE(void) selmR3SelInfoFromDesc32(PDBGFSELINFO pSelInfo, PCX86DESC pDesc)
{
    pSelInfo->Raw64.au64[1] = 0;
    pSelInfo->Raw = *pDesc;
    if (    pDesc->Gen.u1DescType
        ||  !(pDesc->Gen.u4Type & 4))
    {
        pSelInfo->cbLimit = X86DESC_LIMIT(*pDesc);
        if (pDesc->Gen.u1Granularity)
            pSelInfo->cbLimit = (pSelInfo->cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
        pSelInfo->GCPtrBase = X86DESC_BASE(*pDesc);
        pSelInfo->SelGate = 0;
    }
    else if (pDesc->Gen.u4Type != X86_SEL_TYPE_SYS_UNDEFINED4)
    {
        pSelInfo->cbLimit = 0;
        if (pDesc->Gen.u4Type == X86_SEL_TYPE_SYS_TASK_GATE)
            pSelInfo->GCPtrBase = 0;
        else
            pSelInfo->GCPtrBase = pDesc->Gate.u16OffsetLow
                                | (uint32_t)pDesc->Gate.u16OffsetHigh << 16;
        pSelInfo->SelGate = pDesc->Gate.u16Sel;
        pSelInfo->fFlags |= DBGFSELINFO_FLAGS_GATE;
    }
    else
    {
        pSelInfo->cbLimit = 0;
        pSelInfo->GCPtrBase = 0;
        pSelInfo->SelGate = 0;
        pSelInfo->fFlags |= DBGFSELINFO_FLAGS_INVALID;
    }
    if (!pDesc->Gen.u1Present)
        pSelInfo->fFlags |= DBGFSELINFO_FLAGS_NOT_PRESENT;
}


/**
 * Gets information about a 64-bit selector, SELMR3GetSelectorInfo helper.
 *
 * See SELMR3GetSelectorInfo for details.
 *
 * @returns VBox status code, see SELMR3GetSelectorInfo for details.
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   Sel         The selector to get info about.
 * @param   pSelInfo    Where to store the information.
 */
static int selmR3GetSelectorInfo32(PVM pVM, PVMCPU pVCpu, RTSEL Sel, PDBGFSELINFO pSelInfo)
{
    /*
     * Read the descriptor entry
     */
    pSelInfo->fFlags = 0;
    X86DESC Desc;
    if (    !(Sel & X86_SEL_LDT)
        && (    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS] == (Sel & X86_SEL_MASK)
            ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS] == (Sel & X86_SEL_MASK)
            ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64] == (Sel & X86_SEL_MASK)
            ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS] == (Sel & X86_SEL_MASK)
            ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] == (Sel & X86_SEL_MASK))
       )
    {
        /*
         * Hypervisor descriptor.
         */
        pSelInfo->fFlags = DBGFSELINFO_FLAGS_HYPER;
        if (CPUMIsGuestInProtectedMode(pVCpu))
            pSelInfo->fFlags |= DBGFSELINFO_FLAGS_PROT_MODE;
        else
            pSelInfo->fFlags |= DBGFSELINFO_FLAGS_REAL_MODE;

        Desc = pVM->selm.s.paGdtR3[Sel >> X86_SEL_SHIFT];
    }
    else if (CPUMIsGuestInProtectedMode(pVCpu))
    {
        /*
         * Read it from the guest descriptor table.
         */
        pSelInfo->fFlags = DBGFSELINFO_FLAGS_PROT_MODE;

        VBOXGDTR    Gdtr;
        RTGCPTR     GCPtrDesc;
        CPUMGetGuestGDTR(pVCpu, &Gdtr);
        if (!(Sel & X86_SEL_LDT))
        {
            /* GDT */
            if ((unsigned)(Sel & X86_SEL_MASK) + sizeof(X86DESC) - 1 > (unsigned)Gdtr.cbGdt)
                return VERR_INVALID_SELECTOR;
            GCPtrDesc = Gdtr.pGdt + (Sel & X86_SEL_MASK);
        }
        else
        {
            /*
             * LDT - must locate the LDT first...
             */
            RTSEL SelLdt = CPUMGetGuestLDTR(pVCpu);
            if (    (unsigned)(SelLdt & X86_SEL_MASK) < sizeof(X86DESC) /* the first selector is invalid, right? */
                ||  (unsigned)(SelLdt & X86_SEL_MASK) + sizeof(X86DESC) - 1 > (unsigned)Gdtr.cbGdt)
                return VERR_INVALID_SELECTOR;
            GCPtrDesc = Gdtr.pGdt + (SelLdt & X86_SEL_MASK);
            int rc = PGMPhysSimpleReadGCPtr(pVCpu, &Desc, GCPtrDesc, sizeof(Desc));
            if (RT_FAILURE(rc))
                return rc;

            /* validate the LDT descriptor. */
            if (Desc.Gen.u1Present == 0)
                return VERR_SELECTOR_NOT_PRESENT;
            if (    Desc.Gen.u1DescType == 1
                ||  Desc.Gen.u4Type != X86_SEL_TYPE_SYS_LDT)
                return VERR_INVALID_SELECTOR;

            unsigned cbLimit = X86DESC_LIMIT(Desc);
            if (Desc.Gen.u1Granularity)
                cbLimit = (cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
            if ((unsigned)(Sel & X86_SEL_MASK) + sizeof(X86DESC) - 1 > cbLimit)
                return VERR_INVALID_SELECTOR;

            /* calc the descriptor location. */
            GCPtrDesc = X86DESC_BASE(Desc);
            GCPtrDesc += (Sel & X86_SEL_MASK);
        }

        /* read the descriptor. */
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &Desc, GCPtrDesc, sizeof(Desc));
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        /*
         * We're in real mode.
         */
        pSelInfo->Sel       = Sel;
        pSelInfo->GCPtrBase = Sel << 4;
        pSelInfo->cbLimit   = 0xffff;
        pSelInfo->fFlags    = DBGFSELINFO_FLAGS_REAL_MODE;
        pSelInfo->Raw64.au64[0] = 0;
        pSelInfo->Raw64.au64[1] = 0;
        pSelInfo->SelGate   = 0;
        return VINF_SUCCESS;
    }

    /*
     * Extract the base and limit or sel:offset for gates.
     */
    pSelInfo->Sel = Sel;
    selmR3SelInfoFromDesc32(pSelInfo, &Desc);

    return VINF_SUCCESS;
}


/**
 * Gets information about a selector.
 *
 * Intended for the debugger mostly and will prefer the guest descriptor tables
 * over the shadow ones.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_SELECTOR if the selector isn't fully inside the
 *          descriptor table.
 * @retval  VERR_SELECTOR_NOT_PRESENT if the LDT is invalid or not present. This
 *          is not returned if the selector itself isn't present, you have to
 *          check that for yourself (see DBGFSELINFO::fFlags).
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT or VERR_PAGE_NOT_PRESENT if the
 *          pagetable or page backing the selector table wasn't present.
 * @returns Other VBox status code on other errors.
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       The virtual CPU handle.
 * @param   Sel         The selector to get info about.
 * @param   pSelInfo    Where to store the information.
 */
VMMR3DECL(int) SELMR3GetSelectorInfo(PVM pVM, PVMCPU pVCpu, RTSEL Sel, PDBGFSELINFO pSelInfo)
{
    AssertPtr(pSelInfo);
    if (CPUMIsGuestInLongMode(pVCpu))
        return selmR3GetSelectorInfo64(pVM, pVCpu, Sel, pSelInfo);
    return selmR3GetSelectorInfo32(pVM, pVCpu, Sel, pSelInfo);
}


/**
 * Gets information about a selector from the shadow tables.
 *
 * This is intended to be faster than the SELMR3GetSelectorInfo() method, but
 * requires that the caller ensures that the shadow tables are up to date.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_SELECTOR if the selector isn't fully inside the
 *          descriptor table.
 * @retval  VERR_SELECTOR_NOT_PRESENT if the LDT is invalid or not present. This
 *          is not returned if the selector itself isn't present, you have to
 *          check that for yourself (see DBGFSELINFO::fFlags).
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT or VERR_PAGE_NOT_PRESENT if the
 *          pagetable or page backing the selector table wasn't present.
 * @returns Other VBox status code on other errors.
 *
 * @param   pVM         VM handle.
 * @param   Sel         The selector to get info about.
 * @param   pSelInfo    Where to store the information.
 *
 * @remarks Don't use this when in hardware assisted virtualization mode.
 */
VMMR3DECL(int) SELMR3GetShadowSelectorInfo(PVM pVM, RTSEL Sel, PDBGFSELINFO pSelInfo)
{
    Assert(pSelInfo);

    /*
     * Read the descriptor entry
     */
    X86DESC Desc;
    if (!(Sel & X86_SEL_LDT))
    {
        /*
         * Global descriptor.
         */
        Desc = pVM->selm.s.paGdtR3[Sel >> X86_SEL_SHIFT];
        pSelInfo->fFlags =    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS] == (Sel & X86_SEL_MASK)
                           || pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS] == (Sel & X86_SEL_MASK)
                           || pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64] == (Sel & X86_SEL_MASK)
                           || pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS] == (Sel & X86_SEL_MASK)
                           || pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] == (Sel & X86_SEL_MASK)
                         ? DBGFSELINFO_FLAGS_HYPER
                         : 0;
        /** @todo check that the GDT offset is valid. */
    }
    else
    {
        /*
         * Local Descriptor.
         */
        PX86DESC paLDT = (PX86DESC)((char *)pVM->selm.s.pvLdtR3 + pVM->selm.s.offLdtHyper);
        Desc = paLDT[Sel >> X86_SEL_SHIFT];
        /** @todo check if the LDT page is actually available. */
        /** @todo check that the LDT offset is valid. */
        pSelInfo->fFlags = 0;
    }
    if (CPUMIsGuestInProtectedMode(VMMGetCpu0(pVM)))
        pSelInfo->fFlags |= DBGFSELINFO_FLAGS_PROT_MODE;
    else
        pSelInfo->fFlags |= DBGFSELINFO_FLAGS_REAL_MODE;

    /*
     * Extract the base and limit or sel:offset for gates.
     */
    pSelInfo->Sel = Sel;
    selmR3SelInfoFromDesc32(pSelInfo, &Desc);

    return VINF_SUCCESS;
}


/**
 * Formats a descriptor.
 *
 * @param   Desc        Descriptor to format.
 * @param   Sel         Selector number.
 * @param   pszOutput   Output buffer.
 * @param   cchOutput   Size of output buffer.
 */
static void selmR3FormatDescriptor(X86DESC Desc, RTSEL Sel, char *pszOutput, size_t cchOutput)
{
    /*
     * Make variable description string.
     */
    static struct
    {
        unsigned    cch;
        const char *psz;
    } const aTypes[32] =
    {
#define STRENTRY(str) { sizeof(str) - 1, str }
        /* system */
        STRENTRY("Reserved0 "),                  /* 0x00 */
        STRENTRY("TSS16Avail "),                 /* 0x01 */
        STRENTRY("LDT "),                        /* 0x02 */
        STRENTRY("TSS16Busy "),                  /* 0x03 */
        STRENTRY("Call16 "),                     /* 0x04 */
        STRENTRY("Task "),                       /* 0x05 */
        STRENTRY("Int16 "),                      /* 0x06 */
        STRENTRY("Trap16 "),                     /* 0x07 */
        STRENTRY("Reserved8 "),                  /* 0x08 */
        STRENTRY("TSS32Avail "),                 /* 0x09 */
        STRENTRY("ReservedA "),                  /* 0x0a */
        STRENTRY("TSS32Busy "),                  /* 0x0b */
        STRENTRY("Call32 "),                     /* 0x0c */
        STRENTRY("ReservedD "),                  /* 0x0d */
        STRENTRY("Int32 "),                      /* 0x0e */
        STRENTRY("Trap32 "),                     /* 0x0f */
        /* non system */
        STRENTRY("DataRO "),                     /* 0x10 */
        STRENTRY("DataRO Accessed "),            /* 0x11 */
        STRENTRY("DataRW "),                     /* 0x12 */
        STRENTRY("DataRW Accessed "),            /* 0x13 */
        STRENTRY("DataDownRO "),                 /* 0x14 */
        STRENTRY("DataDownRO Accessed "),        /* 0x15 */
        STRENTRY("DataDownRW "),                 /* 0x16 */
        STRENTRY("DataDownRW Accessed "),        /* 0x17 */
        STRENTRY("CodeEO "),                     /* 0x18 */
        STRENTRY("CodeEO Accessed "),            /* 0x19 */
        STRENTRY("CodeER "),                     /* 0x1a */
        STRENTRY("CodeER Accessed "),            /* 0x1b */
        STRENTRY("CodeConfEO "),                 /* 0x1c */
        STRENTRY("CodeConfEO Accessed "),        /* 0x1d */
        STRENTRY("CodeConfER "),                 /* 0x1e */
        STRENTRY("CodeConfER Accessed ")         /* 0x1f */
#undef SYSENTRY
    };
#define ADD_STR(psz, pszAdd) do { strcpy(psz, pszAdd); psz += strlen(pszAdd); } while (0)
    char        szMsg[128];
    char       *psz = &szMsg[0];
    unsigned    i = Desc.Gen.u1DescType << 4 | Desc.Gen.u4Type;
    memcpy(psz, aTypes[i].psz, aTypes[i].cch);
    psz += aTypes[i].cch;

    if (Desc.Gen.u1Present)
        ADD_STR(psz, "Present ");
    else
        ADD_STR(psz, "Not-Present ");
    if (Desc.Gen.u1Granularity)
        ADD_STR(psz, "Page ");
    if (Desc.Gen.u1DefBig)
        ADD_STR(psz, "32-bit ");
    else
        ADD_STR(psz, "16-bit ");
#undef ADD_STR
    *psz = '\0';

    /*
     * Limit and Base and format the output.
     */
    uint32_t    u32Limit = X86DESC_LIMIT(Desc);
    if (Desc.Gen.u1Granularity)
        u32Limit = u32Limit << PAGE_SHIFT | PAGE_OFFSET_MASK;
    uint32_t    u32Base =  X86DESC_BASE(Desc);

    RTStrPrintf(pszOutput, cchOutput, "%04x - %08x %08x - base=%08x limit=%08x dpl=%d %s",
                Sel, Desc.au32[0], Desc.au32[1], u32Base, u32Limit, Desc.Gen.u2Dpl, szMsg);
}


/**
 * Dumps a descriptor.
 *
 * @param   Desc    Descriptor to dump.
 * @param   Sel     Selector number.
 * @param   pszMsg  Message to prepend the log entry with.
 */
VMMR3DECL(void) SELMR3DumpDescriptor(X86DESC  Desc, RTSEL Sel, const char *pszMsg)
{
    char szOutput[128];
    selmR3FormatDescriptor(Desc, Sel, &szOutput[0], sizeof(szOutput));
    Log(("%s: %s\n", pszMsg, szOutput));
    NOREF(szOutput[0]);
}


/**
 * Display the shadow gdt.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) selmR3InfoGdt(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    pHlp->pfnPrintf(pHlp, "Shadow GDT (GCAddr=%RRv):\n", MMHyperR3ToRC(pVM, pVM->selm.s.paGdtR3));
    for (unsigned iGDT = 0; iGDT < SELM_GDT_ELEMENTS; iGDT++)
    {
        if (pVM->selm.s.paGdtR3[iGDT].Gen.u1Present)
        {
            char szOutput[128];
            selmR3FormatDescriptor(pVM->selm.s.paGdtR3[iGDT], iGDT << X86_SEL_SHIFT, &szOutput[0], sizeof(szOutput));
            const char *psz = "";
            if (iGDT == ((unsigned)pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS] >> X86_SEL_SHIFT))
                psz = " HyperCS";
            else if (iGDT == ((unsigned)pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS] >> X86_SEL_SHIFT))
                psz = " HyperDS";
            else if (iGDT == ((unsigned)pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64] >> X86_SEL_SHIFT))
                psz = " HyperCS64";
            else if (iGDT == ((unsigned)pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS] >> X86_SEL_SHIFT))
                psz = " HyperTSS";
            else if (iGDT == ((unsigned)pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] >> X86_SEL_SHIFT))
                psz = " HyperTSSTrap08";
            pHlp->pfnPrintf(pHlp, "%s%s\n", szOutput, psz);
        }
    }
}


/**
 * Display the guest gdt.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) selmR3InfoGdtGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /** @todo SMP support! */
    PVMCPU      pVCpu = &pVM->aCpus[0];

    VBOXGDTR    GDTR;
    CPUMGetGuestGDTR(pVCpu, &GDTR);
    RTGCPTR     GCPtrGDT = GDTR.pGdt;
    unsigned    cGDTs = ((unsigned)GDTR.cbGdt + 1) / sizeof(X86DESC);

    pHlp->pfnPrintf(pHlp, "Guest GDT (GCAddr=%RGv limit=%x):\n", GCPtrGDT, GDTR.cbGdt);
    for (unsigned iGDT = 0; iGDT < cGDTs; iGDT++, GCPtrGDT += sizeof(X86DESC))
    {
        X86DESC GDTE;
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &GDTE, GCPtrGDT, sizeof(GDTE));
        if (RT_SUCCESS(rc))
        {
            if (GDTE.Gen.u1Present)
            {
                char szOutput[128];
                selmR3FormatDescriptor(GDTE, iGDT << X86_SEL_SHIFT, &szOutput[0], sizeof(szOutput));
                pHlp->pfnPrintf(pHlp, "%s\n", szOutput);
            }
        }
        else if (rc == VERR_PAGE_NOT_PRESENT)
        {
            if ((GCPtrGDT & PAGE_OFFSET_MASK) + sizeof(X86DESC) - 1 < sizeof(X86DESC))
                pHlp->pfnPrintf(pHlp, "%04x - page not present (GCAddr=%RGv)\n", iGDT << X86_SEL_SHIFT, GCPtrGDT);
        }
        else
            pHlp->pfnPrintf(pHlp, "%04x - read error rc=%Rrc GCAddr=%RGv\n", iGDT << X86_SEL_SHIFT, rc, GCPtrGDT);
    }
}


/**
 * Display the shadow ldt.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) selmR3InfoLdt(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    unsigned    cLDTs = ((unsigned)pVM->selm.s.cbLdtLimit + 1) >> X86_SEL_SHIFT;
    PX86DESC    paLDT = (PX86DESC)((char *)pVM->selm.s.pvLdtR3 + pVM->selm.s.offLdtHyper);
    pHlp->pfnPrintf(pHlp, "Shadow LDT (GCAddr=%RRv limit=%#x):\n", pVM->selm.s.pvLdtRC + pVM->selm.s.offLdtHyper, pVM->selm.s.cbLdtLimit);
    for (unsigned iLDT = 0; iLDT < cLDTs; iLDT++)
    {
        if (paLDT[iLDT].Gen.u1Present)
        {
            char szOutput[128];
            selmR3FormatDescriptor(paLDT[iLDT], (iLDT << X86_SEL_SHIFT) | X86_SEL_LDT, &szOutput[0], sizeof(szOutput));
            pHlp->pfnPrintf(pHlp, "%s\n", szOutput);
        }
    }
}


/**
 * Display the guest ldt.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) selmR3InfoLdtGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /** @todo SMP support! */
    PVMCPU      pVCpu = &pVM->aCpus[0];

    RTSEL SelLdt = CPUMGetGuestLDTR(pVCpu);
    if (!(SelLdt & X86_SEL_MASK))
    {
        pHlp->pfnPrintf(pHlp, "Guest LDT (Sel=%x): Null-Selector\n", SelLdt);
        return;
    }

    RTGCPTR     GCPtrLdt;
    unsigned    cbLdt;
    int rc = SELMGetLDTFromSel(pVM, SelLdt, &GCPtrLdt, &cbLdt);
    if (RT_FAILURE(rc))
    {
        pHlp->pfnPrintf(pHlp, "Guest LDT (Sel=%x): rc=%Rrc\n", SelLdt, rc);
        return;
    }

    pHlp->pfnPrintf(pHlp, "Guest LDT (Sel=%x GCAddr=%RGv limit=%x):\n", SelLdt, GCPtrLdt, cbLdt);
    unsigned    cLdts  = (cbLdt + 1) >> X86_SEL_SHIFT;
    for (unsigned iLdt = 0; iLdt < cLdts; iLdt++, GCPtrLdt += sizeof(X86DESC))
    {
        X86DESC LdtE;
        int rc = PGMPhysSimpleReadGCPtr(pVCpu, &LdtE, GCPtrLdt, sizeof(LdtE));
        if (RT_SUCCESS(rc))
        {
            if (LdtE.Gen.u1Present)
            {
                char szOutput[128];
                selmR3FormatDescriptor(LdtE, (iLdt << X86_SEL_SHIFT) | X86_SEL_LDT, &szOutput[0], sizeof(szOutput));
                pHlp->pfnPrintf(pHlp, "%s\n", szOutput);
            }
        }
        else if (rc == VERR_PAGE_NOT_PRESENT)
        {
            if ((GCPtrLdt & PAGE_OFFSET_MASK) + sizeof(X86DESC) - 1 < sizeof(X86DESC))
                pHlp->pfnPrintf(pHlp, "%04x - page not present (GCAddr=%RGv)\n", (iLdt << X86_SEL_SHIFT) | X86_SEL_LDT, GCPtrLdt);
        }
        else
            pHlp->pfnPrintf(pHlp, "%04x - read error rc=%Rrc GCAddr=%RGv\n", (iLdt << X86_SEL_SHIFT) | X86_SEL_LDT, rc, GCPtrLdt);
    }
}


/**
 * Dumps the hypervisor GDT
 *
 * @param   pVM     VM handle.
 */
VMMR3DECL(void) SELMR3DumpHyperGDT(PVM pVM)
{
    DBGFR3Info(pVM, "gdt", NULL, NULL);
}


/**
 * Dumps the hypervisor LDT
 *
 * @param   pVM     VM handle.
 */
VMMR3DECL(void) SELMR3DumpHyperLDT(PVM pVM)
{
    DBGFR3Info(pVM, "ldt", NULL, NULL);
}


/**
 * Dumps the guest GDT
 *
 * @param   pVM     VM handle.
 */
VMMR3DECL(void) SELMR3DumpGuestGDT(PVM pVM)
{
    DBGFR3Info(pVM, "gdtguest", NULL, NULL);
}


/**
 * Dumps the guest LDT
 *
 * @param   pVM     VM handle.
 */
VMMR3DECL(void) SELMR3DumpGuestLDT(PVM pVM)
{
    DBGFR3Info(pVM, "ldtguest", NULL, NULL);
}

