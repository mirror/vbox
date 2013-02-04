/* $Id$ */
/** @file
 * SELM - Internal header file.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___SELMInternal_h
#define ___SELMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/cpum.h>
#include <VBox/log.h>
#include <iprt/x86.h>



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
    /** Counts the times we detected state selectors in SELMR3UpdateFromCPUM. */
    STAMCOUNTER             aStatDetectedStaleSReg[X86_SREG_COUNT];
    /** Counts the times we were called with already state selectors in
     * SELMR3UpdateFromCPUM. */
    STAMCOUNTER             aStatAlreadyStaleSReg[X86_SREG_COUNT];
    /** Counts the times we found a stale selector becomming valid again. */
    STAMCOUNTER             StatStaleToUnstaleSReg;
#ifdef VBOX_WITH_STATISTICS
    /** Times we updated hidden selector registers in CPUMR3UpdateFromCPUM. */
    STAMCOUNTER             aStatUpdatedSReg[X86_SREG_COUNT];
    STAMCOUNTER             StatLoadHidSelGst;
    STAMCOUNTER             StatLoadHidSelShw;
#endif
    STAMCOUNTER             StatLoadHidSelReadErrors;
    STAMCOUNTER             StatLoadHidSelGstNoGood;
} SELM, *PSELM;

RT_C_DECLS_BEGIN

VMMRCDECL(int) selmRCGuestGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCGuestLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCGuestTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);

VMMRCDECL(int) selmRCShadowGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCShadowLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
VMMRCDECL(int) selmRCShadowTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);

void           selmSetRing1Stack(PVM pVM, uint32_t ss, RTGCPTR32 esp);

RT_C_DECLS_END


#ifdef VBOX_WITH_RAW_MODE_NOT_R0

/**
 * Checks if a shadow descriptor table entry is good for the given segment
 * register.
 *
 * @returns @c true if good, @c false if not.
 * @param   pSReg               The segment register.
 * @param   pShwDesc            The shadow descriptor table entry.
 * @param   iSReg               The segment register index (X86_SREG_XXX).
 * @param   uCpl                The CPL.
 */
DECLINLINE(bool) selmIsShwDescGoodForSReg(PCCPUMSELREG pSReg, PCX86DESC pShwDesc, uint32_t iSReg, uint32_t uCpl)
{
    /*
     * See iemMiscValidateNewSS, iemCImpl_LoadSReg and intel+amd manuals.
     */

    if (!pShwDesc->Gen.u1Present)
    {
        Log(("selmIsShwDescGoodForSReg: Not present\n"));
        return false;
    }

    if (!pShwDesc->Gen.u1DescType)
    {
        Log(("selmIsShwDescGoodForSReg: System descriptor\n"));
        return false;
    }

    if (iSReg == X86_SREG_SS)
    {
        if ((pShwDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_WRITE)) != X86_SEL_TYPE_WRITE)
        {
            Log(("selmIsShwDescGoodForSReg: Stack must be writable\n"));
            return false;
        }
        if (uCpl > (unsigned)pShwDesc->Gen.u2Dpl - pShwDesc->Gen.u1Available)
        {
            Log(("selmIsShwDescGoodForSReg: CPL(%d) > DPL(%d)\n", uCpl, pShwDesc->Gen.u2Dpl - pShwDesc->Gen.u1Available));
            return false;
        }
    }
    else
    {
        if (iSReg == X86_SREG_CS)
        {
            if (!(pShwDesc->Gen.u4Type & X86_SEL_TYPE_CODE))
            {
                Log(("selmIsShwDescGoodForSReg: CS needs code segment\n"));
                return false;
            }
        }
        else if ((pShwDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
        {
            Log(("selmIsShwDescGoodForSReg: iSReg=%u execute only\n", iSReg));
            return false;
        }

        if (       (pShwDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
                != (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF)
            &&  (   (   (pSReg->Sel & X86_SEL_RPL) > (unsigned)pShwDesc->Gen.u2Dpl - pShwDesc->Gen.u1Available
                     && (pSReg->Sel & X86_SEL_RPL) != pShwDesc->Gen.u1Available )
                 || uCpl > (unsigned)pShwDesc->Gen.u2Dpl - pShwDesc->Gen.u1Available ) )
        {
            Log(("selmIsShwDescGoodForSReg: iSReg=%u DPL=%u CPL=%u RPL=%u\n", iSReg,
                 pShwDesc->Gen.u2Dpl - pShwDesc->Gen.u1Available, uCpl, pSReg->Sel & X86_SEL_RPL));
            return false;
        }
    }

    return true;
}


/**
 * Checks if a guest descriptor table entry is good for the given segment
 * register.
 *
 * @returns @c true if good, @c false if not.
 * @param   pVCpu               The current virtual CPU.
 * @param   pSReg               The segment register.
 * @param   pGstDesc            The guest descriptor table entry.
 * @param   iSReg               The segment register index (X86_SREG_XXX).
 * @param   uCpl                The CPL.
 */
DECLINLINE(bool) selmIsGstDescGoodForSReg(PVMCPU pVCpu, PCCPUMSELREG pSReg, PCX86DESC pGstDesc, uint32_t iSReg, uint32_t uCpl)
{
    /*
     * See iemMiscValidateNewSS, iemCImpl_LoadSReg and intel+amd manuals.
     */

    if (!pGstDesc->Gen.u1Present)
    {
        Log(("selmIsGstDescGoodForSReg: Not present\n"));
        return false;
    }

    if (!pGstDesc->Gen.u1DescType)
    {
        Log(("selmIsGstDescGoodForSReg: System descriptor\n"));
        return false;
    }

    if (iSReg == X86_SREG_SS)
    {
        if ((pGstDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_WRITE)) != X86_SEL_TYPE_WRITE)
        {
            Log(("selmIsGstDescGoodForSReg: Stack must be writable\n"));
            return false;
        }
        if (uCpl > pGstDesc->Gen.u2Dpl)
        {
            Log(("selmIsGstDescGoodForSReg: CPL(%d) > DPL(%d)\n", uCpl, pGstDesc->Gen.u2Dpl));
            return false;
        }
    }
    else
    {
        if (iSReg == X86_SREG_CS)
        {
            if (!(pGstDesc->Gen.u4Type & X86_SEL_TYPE_CODE))
            {
                Log(("selmIsGstDescGoodForSReg: CS needs code segment\n"));
                return false;
            }
        }
        else if ((pGstDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
        {
            Log(("selmIsGstDescGoodForSReg: iSReg=%u execute only\n", iSReg));
            return false;
        }

        if (       (pGstDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
                != (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF)
            &&  (   (   (pSReg->Sel & X86_SEL_RPL) > pGstDesc->Gen.u2Dpl
                     && (   (pSReg->Sel & X86_SEL_RPL) != 1
                         || !CPUMIsGuestInRawMode(pVCpu) ) )
                 || uCpl > (unsigned)pGstDesc->Gen.u2Dpl
                )
           )
        {
            Log(("selmIsGstDescGoodForSReg: iSReg=%u DPL=%u CPL=%u RPL=%u InRawMode=%u\n", iSReg,
                 pGstDesc->Gen.u2Dpl, uCpl, pSReg->Sel & X86_SEL_RPL, CPUMIsGuestInRawMode(pVCpu)));
            return false;
        }
    }

    return true;
}


/**
 * Converts a guest GDT or LDT entry to a shadow table entry.
 *
 * @param   pDesc       Guest entry on input, shadow entry on return.
 */
DECL_FORCE_INLINE(void) selmGuestToShadowDesc(PX86DESC pDesc)
{
    /*
     * Code and data selectors are generally 1:1, with the
     * 'little' adjustment we do for DPL 0 selectors.
     */
    if (pDesc->Gen.u1DescType)
    {
        /*
         * Hack for A-bit against Trap E on read-only GDT.
         */
        /** @todo Fix this by loading ds and cs before turning off WP. */
        pDesc->Gen.u4Type |= X86_SEL_TYPE_ACCESSED;

        /*
         * All DPL 0 code and data segments are squeezed into DPL 1.
         *
         * We're skipping conforming segments here because those
         * cannot give us any trouble.
         */
        if (    pDesc->Gen.u2Dpl == 0
            &&      (pDesc->Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
                !=  (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF) )
        {
            pDesc->Gen.u2Dpl       = 1;
            pDesc->Gen.u1Available = 1;
        }
        else
            pDesc->Gen.u1Available = 0;
    }
    else
    {
        /*
         * System type selectors are marked not present.
         * Recompiler or special handling is required for these.
         */
        /** @todo what about interrupt gates and rawr0? */
        pDesc->Gen.u1Present = 0;
    }
}


/**
 * Checks if a segment register is stale given the shadow descriptor table
 * entry.
 *
 * @returns @c true if stale, @c false if not.
 * @param   pSReg               The segment register.
 * @param   pShwDesc            The shadow descriptor entry.
 * @param   iSReg               The segment register number (X86_SREG_XXX).
 */
DECLINLINE(bool) selmIsSRegStale32(PCCPUMSELREG pSReg, PCX86DESC pShwDesc, uint32_t iSReg)
{
    if (   pSReg->Attr.n.u1Present     != pShwDesc->Gen.u1Present
        || pSReg->Attr.n.u4Type        != pShwDesc->Gen.u4Type
        || pSReg->Attr.n.u1DescType    != pShwDesc->Gen.u1DescType
        || pSReg->Attr.n.u1DefBig      != pShwDesc->Gen.u1DefBig
        || pSReg->Attr.n.u1Granularity != pShwDesc->Gen.u1Granularity
        || pSReg->Attr.n.u2Dpl         != pShwDesc->Gen.u2Dpl - pShwDesc->Gen.u1Available)
    {
        Log(("selmIsSRegStale32: Attributes changed (%#x -> %#x)\n", pSReg->Attr.u, X86DESC_GET_HID_ATTR(pShwDesc)));
        return true;
    }

    if (pSReg->u64Base != X86DESC_BASE(pShwDesc))
    {
        Log(("selmIsSRegStale32: base changed (%#llx -> %#llx)\n", pSReg->u64Base, X86DESC_BASE(pShwDesc)));
        return true;
    }

    if (pSReg->u32Limit != X86DESC_LIMIT_G(pShwDesc))
    {
        Log(("selmIsSRegStale32: limit changed (%#x -> %#x)\n", pSReg->u32Limit, X86DESC_LIMIT_G(pShwDesc)));
        return true;
    }

    return false;
}


/**
 * Loads the hidden bits of a selector register from a shadow descriptor table
 * entry.
 *
 * @param   pSReg               The segment register in question.
 * @param   pShwDesc            The shadow descriptor table entry.
 */
DECLINLINE(void) selmLoadHiddenSRegFromShadowDesc(PCPUMSELREG pSReg, PCX86DESC pShwDesc)
{
    pSReg->Attr.u         = X86DESC_GET_HID_ATTR(pShwDesc);
    pSReg->Attr.n.u2Dpl  -= pSReg->Attr.n.u1Available;
    Assert(pSReg->Attr.n.u4Type & X86_SEL_TYPE_ACCESSED);
    pSReg->u32Limit       = X86DESC_LIMIT_G(pShwDesc);
    pSReg->u64Base        = X86DESC_BASE(pShwDesc);
    pSReg->ValidSel       = pSReg->Sel;
    if (pSReg->Attr.n.u1Available)
        pSReg->ValidSel  &= ~(RTSEL)1;
    pSReg->fFlags         = CPUMSELREG_FLAGS_VALID;
}


/**
 * Loads the hidden bits of a selector register from a guest descriptor table
 * entry.
 *
 * @param   pVCpu               The current virtual CPU.
 * @param   pSReg               The segment register in question.
 * @param   pGstDesc            The guest descriptor table entry.
 */
DECLINLINE(void) selmLoadHiddenSRegFromGuestDesc(PVMCPU pVCpu, PCPUMSELREG pSReg, PCX86DESC pGstDesc)
{
    pSReg->Attr.u         = X86DESC_GET_HID_ATTR(pGstDesc);
    pSReg->Attr.n.u4Type |= X86_SEL_TYPE_ACCESSED;
    pSReg->u32Limit       = X86DESC_LIMIT_G(pGstDesc);
    pSReg->u64Base        = X86DESC_BASE(pGstDesc);
    pSReg->ValidSel       = pSReg->Sel;
    if ((pSReg->ValidSel & 1) && CPUMIsGuestInRawMode(pVCpu))
        pSReg->ValidSel  &= ~(RTSEL)1;
    pSReg->fFlags         = CPUMSELREG_FLAGS_VALID;
}

#endif /* VBOX_WITH_RAW_MODE_NOT_R0 */

/** @} */

#endif
