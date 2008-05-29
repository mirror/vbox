/* $Id$ */
/** @file
 * CPUM - CPU Monitor(/Manager)
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/cpum.h>
#include <VBox/cpumdis.h>
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include <VBox/selm.h>
#include <VBox/dbgf.h>
#include <VBox/patm.h>
#include <VBox/ssm.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>

#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/system.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The saved state version. */
#define CPUM_SAVED_STATE_VERSION    6


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * What kind of cpu info dump to performe.
 */
typedef enum CPUMDUMPTYPE
{
    CPUMDUMPTYPE_TERSE,
    CPUMDUMPTYPE_DEFAULT,
    CPUMDUMPTYPE_VERBOSE

} CPUMDUMPTYPE, *PCPUMDUMPTYPE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int cpumR3CpuIdInit(PVM pVM);
static DECLCALLBACK(int) cpumR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) cpumR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoHyper(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoHost(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/**
 * Initializes the CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR3DECL(int) CPUMR3Init(PVM pVM)
{
    LogFlow(("CPUMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, cpum.s) & 31));
    AssertRelease(sizeof(pVM->cpum.s) <= sizeof(pVM->cpum.padding));

    /*
     * Setup any fixed pointers and offsets.
     */
    pVM->cpum.s.offVM = RT_OFFSETOF(VM, cpum);
    pVM->cpum.s.pCPUMHC = &pVM->cpum.s;
    pVM->cpum.s.pHyperCoreR3 = CPUMCTX2CORE(&pVM->cpum.s.Hyper);
    pVM->cpum.s.pHyperCoreR0 = VM_R0_ADDR(pVM, CPUMCTX2CORE(&pVM->cpum.s.Hyper));

    /* Hidden selector registers are invalid by default. */
    pVM->cpum.s.fValidHiddenSelRegs  = false;

    /*
     * Check that the CPU supports the minimum features we require.
     */
    /** @todo check the contract! */
    if (!ASMHasCpuId())
    {
        Log(("The CPU doesn't support CPUID!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    ASMCpuId_ECX_EDX(1, &pVM->cpum.s.CPUFeatures.ecx, &pVM->cpum.s.CPUFeatures.edx);

    /* Setup the CR4 AND and OR masks used in the switcher */
    /* Depends on the presence of FXSAVE(SSE) support on the host CPU */
    if (!pVM->cpum.s.CPUFeatures.edx.u1FXSR)
    {
        Log(("The CPU doesn't support FXSAVE/FXRSTOR!\n"));
        /* No FXSAVE implies no SSE */
        pVM->cpum.s.CR4.AndMask = X86_CR4_PVI | X86_CR4_VME;
        pVM->cpum.s.CR4.OrMask  = 0;
    }
    else
    {
        pVM->cpum.s.CR4.AndMask = X86_CR4_OSXMMEEXCPT | X86_CR4_PVI | X86_CR4_VME;
        pVM->cpum.s.CR4.OrMask  = X86_CR4_OSFSXR;
    }

    if (!pVM->cpum.s.CPUFeatures.edx.u1MMX)
    {
        Log(("The CPU doesn't support MMX!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    if (!pVM->cpum.s.CPUFeatures.edx.u1TSC)
    {
        Log(("The CPU doesn't support TSC!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    /* Bogus on AMD? */
    if (!pVM->cpum.s.CPUFeatures.edx.u1SEP)
    {
        Log(("The CPU doesn't support SYSENTER/SYSEXIT!\n"));
    }

    /*
     * Setup hypervisor startup values.
     */

    /*
     * Register saved state data item.
     */
    int rc = SSMR3RegisterInternal(pVM, "cpum", 1, CPUM_SAVED_STATE_VERSION, sizeof(CPUM),
                                   NULL, cpumR3Save, NULL,
                                   NULL, cpumR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Register info handlers.
     */
    DBGFR3InfoRegisterInternal(pVM, "cpum",         "Displays the all the cpu states.",    &cpumR3InfoAll);
    DBGFR3InfoRegisterInternal(pVM, "cpumguest",    "Displays the guest cpu state.",       &cpumR3InfoGuest);
    DBGFR3InfoRegisterInternal(pVM, "cpumhyper",    "Displays the hypervisor cpu state.",  &cpumR3InfoHyper);
    DBGFR3InfoRegisterInternal(pVM, "cpumhost",     "Displays the host cpu state.",        &cpumR3InfoHost);
    DBGFR3InfoRegisterInternal(pVM, "cpuid",        "Displays the guest cpuid leaves.",     &cpumR3CpuIdInfo);

    /*
     * Initialize the Guest CPU state.
     */
    rc = cpumR3CpuIdInit(pVM);
    if (VBOX_FAILURE(rc))
        return rc;
    CPUMR3Reset(pVM);
    return VINF_SUCCESS;
}


/**
 * Initializes the emulated CPU's cpuid information.
 *
 * @returns VBox status code.
 * @param   pVM          The VM to operate on.
 */
static int cpumR3CpuIdInit(PVM pVM)
{
    PCPUM    pCPUM = &pVM->cpum.s;
    uint32_t i;

    /*
     * Get the host CPUIDs.
     */
    for (i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd); i++)
        ASMCpuId_Idx_ECX(i, 0,
                         &pCPUM->aGuestCpuIdStd[i].eax, &pCPUM->aGuestCpuIdStd[i].ebx,
                         &pCPUM->aGuestCpuIdStd[i].ecx, &pCPUM->aGuestCpuIdStd[i].edx);
    for (i = 0; i < RT_ELEMENTS(pCPUM->aGuestCpuIdExt); i++)
        ASMCpuId(0x80000000 + i,
                 &pCPUM->aGuestCpuIdExt[i].eax, &pCPUM->aGuestCpuIdExt[i].ebx,
                 &pCPUM->aGuestCpuIdExt[i].ecx, &pCPUM->aGuestCpuIdExt[i].edx);
    for (i = 0; i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur); i++)
        ASMCpuId(0xc0000000 + i,
                 &pCPUM->aGuestCpuIdCentaur[i].eax, &pCPUM->aGuestCpuIdCentaur[i].ebx,
                 &pCPUM->aGuestCpuIdCentaur[i].ecx, &pCPUM->aGuestCpuIdCentaur[i].edx);


    /*
     * Only report features we can support.
     */
    pCPUM->aGuestCpuIdStd[1].edx      &= X86_CPUID_FEATURE_EDX_FPU
                                       | X86_CPUID_FEATURE_EDX_VME
                                       | X86_CPUID_FEATURE_EDX_DE
                                       | X86_CPUID_FEATURE_EDX_PSE
                                       | X86_CPUID_FEATURE_EDX_TSC
                                       | X86_CPUID_FEATURE_EDX_MSR
                                       //| X86_CPUID_FEATURE_EDX_PAE   - not implemented yet.
                                       | X86_CPUID_FEATURE_EDX_MCE
                                       | X86_CPUID_FEATURE_EDX_CX8
                                       //| X86_CPUID_FEATURE_EDX_APIC  - set by the APIC device if present.
                                       /** @note we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see #1757) */
                                       //| X86_CPUID_FEATURE_EDX_SEP
                                       //| X86_CPUID_FEATURE_EDX_MTRR  - no MTRRs.
                                       | X86_CPUID_FEATURE_EDX_PGE
                                       //| X86_CPUID_FEATURE_EDX_MCA   - not virtualized.
                                       | X86_CPUID_FEATURE_EDX_CMOV
                                       //| X86_CPUID_FEATURE_EDX_PAT   - not virtualized.
                                       //| X86_CPUID_FEATURE_EDX_PSE36 - not virtualized.
                                       //| X86_CPUID_FEATURE_EDX_PSN   - no serial number.
                                       | X86_CPUID_FEATURE_EDX_CLFSH
                                       //| X86_CPUID_FEATURE_EDX_DS    - no debug store.
                                       //| X86_CPUID_FEATURE_EDX_ACPI  - not virtualized yet.
                                       | X86_CPUID_FEATURE_EDX_MMX
                                       | X86_CPUID_FEATURE_EDX_FXSR
                                       | X86_CPUID_FEATURE_EDX_SSE
                                       | X86_CPUID_FEATURE_EDX_SSE2
                                       //| X86_CPUID_FEATURE_EDX_SS    - no self snoop.
                                       //| X86_CPUID_FEATURE_EDX_HTT   - no hyperthreading.
                                       //| X86_CPUID_FEATURE_EDX_TM    - no thermal monitor.
                                       //| X86_CPUID_FEATURE_EDX_PBE   - no pneding break enabled.
                                       | 0;
    pCPUM->aGuestCpuIdStd[1].ecx      &= 0//X86_CPUID_FEATURE_ECX_SSE3 - not supported by the recompiler yet.
                                       | X86_CPUID_FEATURE_ECX_MONITOR
                                       //| X86_CPUID_FEATURE_ECX_CPLDS - no CPL qualified debug store.
                                       //| X86_CPUID_FEATURE_ECX_VMX   - not virtualized.
                                       //| X86_CPUID_FEATURE_ECX_EST   - no extended speed step.
                                       //| X86_CPUID_FEATURE_ECX_TM2   - no thermal monitor 2.
                                       //| X86_CPUID_FEATURE_ECX_CNTXID - no L1 context id (MSR++).
                                       | 0;

    /* ASSUMES that this is ALWAYS the AMD define feature set if present. */
    pCPUM->aGuestCpuIdExt[1].edx      &= X86_CPUID_AMD_FEATURE_EDX_FPU
                                       | X86_CPUID_AMD_FEATURE_EDX_VME
                                       | X86_CPUID_AMD_FEATURE_EDX_DE
                                       | X86_CPUID_AMD_FEATURE_EDX_PSE
                                       | X86_CPUID_AMD_FEATURE_EDX_TSC
                                       | X86_CPUID_AMD_FEATURE_EDX_MSR //?? this means AMD MSRs..
                                       //| X86_CPUID_AMD_FEATURE_EDX_PAE    - not implemented yet.
                                       //| X86_CPUID_AMD_FEATURE_EDX_MCE    - not virtualized yet.
                                       | X86_CPUID_AMD_FEATURE_EDX_CX8
                                       //| X86_CPUID_AMD_FEATURE_EDX_APIC   - set by the APIC device if present.
                                       /** @note we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see #1757) */
                                       //| X86_CPUID_AMD_FEATURE_EDX_SEP
                                       //| X86_CPUID_AMD_FEATURE_EDX_MTRR   - not virtualized.
                                       | X86_CPUID_AMD_FEATURE_EDX_PGE
                                       //| X86_CPUID_AMD_FEATURE_EDX_MCA    - not virtualized.
                                       | X86_CPUID_AMD_FEATURE_EDX_CMOV
                                       | X86_CPUID_AMD_FEATURE_EDX_PAT
                                       //| X86_CPUID_AMD_FEATURE_EDX_PSE36  - not virtualized.
                                       //| X86_CPUID_AMD_FEATURE_EDX_NX     - not virtualized, requires PAE.
                                       | X86_CPUID_AMD_FEATURE_EDX_MMX
                                       | X86_CPUID_AMD_FEATURE_EDX_FXSR
                                       | X86_CPUID_AMD_FEATURE_EDX_FFXSR
                                       //| X86_CPUID_AMD_FEATURE_EDX_LONG_MODE - not yet.
                                       | X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX
                                       | X86_CPUID_AMD_FEATURE_EDX_3DNOW
                                       | 0;
    pCPUM->aGuestCpuIdExt[1].ecx      &= 0//X86_CPUID_AMD_FEATURE_ECX_SVM    - not virtualized.
                                       | 0;

    /*
     * Hide HTT, multicode, SMP, whatever.
     * (APIC-ID := 0 and #LogCpus := 0)
     */
    pCPUM->aGuestCpuIdStd[1].ebx &= 0x0000ffff;

    /*
     * Determin the default.
     *
     * Intel returns values of the highest standard function, while AMD
     * returns zeros. VIA on the other hand seems to returning nothing or
     * perhaps some random garbage, we don't try duplicate this behavior.
     */
    ASMCpuId(pCPUM->aGuestCpuIdStd[0].eax + 10,
             &pCPUM->GuestCpuIdDef.eax, &pCPUM->GuestCpuIdDef.ebx,
             &pCPUM->GuestCpuIdDef.ecx, &pCPUM->GuestCpuIdDef.edx);

    /*
     * Limit it the number of entries and fill the remaining with the defaults.
     *
     * The limits are masking off stuff about power saving and similar, this
     * is perhaps a bit crudely done as there is probably some relatively harmless
     * info too in these leaves (like words about having a constant TSC).
     */
    if (pCPUM->aGuestCpuIdStd[0].eax > 2)
        pCPUM->aGuestCpuIdStd[0].eax = 2;
    for (i = pCPUM->aGuestCpuIdStd[0].eax + 1; i < RT_ELEMENTS(pCPUM->aGuestCpuIdStd); i++)
        pCPUM->aGuestCpuIdStd[i] = pCPUM->GuestCpuIdDef;

    if (pCPUM->aGuestCpuIdExt[0].eax > UINT32_C(0x80000004))
        pCPUM->aGuestCpuIdExt[0].eax = UINT32_C(0x80000004);
    for (i = pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000000)
           ? pCPUM->aGuestCpuIdExt[0].eax - UINT32_C(0x80000000) + 1
           : 0;
         i < RT_ELEMENTS(pCPUM->aGuestCpuIdExt); i++)
        pCPUM->aGuestCpuIdExt[i] = pCPUM->GuestCpuIdDef;

    /*
     * Workaround for missing cpuid(0) patches: If we miss to patch a cpuid(0).eax then
     * Linux tries to determine the number of processors from (cpuid(4).eax >> 26) + 1.
     * We don't support more than 1 processor.
     */
    pCPUM->aGuestCpuIdStd[4].eax = 0;

    /*
     * Centaur stuff (VIA).
     *
     * The important part here (we think) is to make sure the 0xc0000000
     * function returns 0xc0000001. As for the features, we don't currently
     * let on about any of those... 0xc0000002 seems to be some
     * temperature/hz/++ stuff, include it as well (static).
     */
    if (    pCPUM->aGuestCpuIdCentaur[0].eax >= UINT32_C(0xc0000000)
        &&  pCPUM->aGuestCpuIdCentaur[0].eax <= UINT32_C(0xc0000004))
    {
        pCPUM->aGuestCpuIdCentaur[0].eax = RT_MIN(pCPUM->aGuestCpuIdCentaur[0].eax, UINT32_C(0xc0000002));
        pCPUM->aGuestCpuIdCentaur[1].edx = 0; /* all features hidden */
        for (i = pCPUM->aGuestCpuIdCentaur[0].eax - UINT32_C(0xc0000000);
             i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur);
             i++)
            pCPUM->aGuestCpuIdCentaur[i] = pCPUM->GuestCpuIdDef;
    }
    else
        for (i = 0; i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur); i++)
            pCPUM->aGuestCpuIdCentaur[i] = pCPUM->GuestCpuIdDef;


    /*
     * Load CPUID overrides from configuration.
     */
    PCPUMCPUID  pCpuId = &pCPUM->aGuestCpuIdStd[0];
    uint32_t    cElements = ELEMENTS(pCPUM->aGuestCpuIdStd);
    for (i=0;; )
    {
        while (cElements-- > 0)
        {
            PCFGMNODE pNode = CFGMR3GetChildF(CFGMR3GetRoot(pVM), "CPUM/CPUID/%RX32", i);
            if (pNode)
            {
                uint32_t u32;
                int rc = CFGMR3QueryU32(pNode, "eax", &u32);
                if (VBOX_SUCCESS(rc))
                    pCpuId->eax = u32;
                else
                    AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

                rc = CFGMR3QueryU32(pNode, "ebx", &u32);
                if (VBOX_SUCCESS(rc))
                    pCpuId->ebx = u32;
                else
                    AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

                rc = CFGMR3QueryU32(pNode, "ecx", &u32);
                if (VBOX_SUCCESS(rc))
                    pCpuId->ecx = u32;
                else
                    AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

                rc = CFGMR3QueryU32(pNode, "edx", &u32);
                if (VBOX_SUCCESS(rc))
                    pCpuId->edx = u32;
                else
                    AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);
            }
            pCpuId++;
            i++;
        }

        /* next */
        if ((i & UINT32_C(0xc0000000)) == 0)
        {
            pCpuId = &pCPUM->aGuestCpuIdExt[0];
            cElements = RT_ELEMENTS(pCPUM->aGuestCpuIdExt);
            i = UINT32_C(0x80000000);
        }
        else if ((i & UINT32_C(0xc0000000)) == UINT32_C(0x80000000))
        {
            pCpuId = &pCPUM->aGuestCpuIdCentaur[0];
            cElements = RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur);
            i = UINT32_C(0xc0000000);
        }
        else
            break;
    }

    /* Check if PAE was explicitely enabled by the user. */
    bool fEnable = false;
    int rc = CFGMR3QueryBool(CFGMR3GetRoot(pVM), "EnablePAE", &fEnable);
    if (VBOX_SUCCESS(rc) && fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);

    /*
     * Log the cpuid and we're good.
     */
    LogRel(("Logical host processors: %d, processor active mask: %08x\n",
            RTSystemProcessorGetCount(), RTSystemProcessorGetActiveMask()));
    LogRel(("************************* CPUID dump ************************\n"));
    DBGFR3Info(pVM, "cpuid", "verbose", DBGFR3InfoLogRelHlp());
    LogRel(("\n"));
    DBGFR3InfoLog(pVM, "cpuid", "verbose"); /* macro */
    LogRel(("******************** End of CPUID dump **********************\n"));
    return VINF_SUCCESS;
}




/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The CPUM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
CPUMR3DECL(void) CPUMR3Relocate(PVM pVM)
{
    LogFlow(("CPUMR3Relocate\n"));
    /*
     * Switcher pointers.
     */
    pVM->cpum.s.pCPUMGC = VM_GUEST_ADDR(pVM, &pVM->cpum.s);
    pVM->cpum.s.pHyperCoreGC = MMHyperCCToGC(pVM, pVM->cpum.s.pHyperCoreR3);
    Assert(pVM->cpum.s.pHyperCoreGC != NIL_RTGCPTR);
}


/**
 * Queries the pointer to the internal CPUMCTX structure
 *
 * @returns VBox status code.
 * @param   pVM         Handle to the virtual machine.
 * @param   ppCtx       Receives the CPUMCTX GC pointer when successful.
 */
CPUMR3DECL(int) CPUMR3QueryGuestCtxGCPtr(PVM pVM, RCPTRTYPE(PCPUMCTX) *ppCtx)
{
    LogFlow(("CPUMR3QueryGuestCtxGCPtr\n"));
    /*
     * Store the address. (Later we might check how's calling, thus the RC.)
     */
    *ppCtx = VM_GUEST_ADDR(pVM, &pVM->cpum.s.Guest);
    return VINF_SUCCESS;
}


/**
 * Terminates the CPUM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR3DECL(int) CPUMR3Term(PVM pVM)
{
    /** @todo */
    return 0;
}


/**
 * Resets the CPU.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM         The VM handle.
 */
CPUMR3DECL(void) CPUMR3Reset(PVM pVM)
{
    PCPUMCTX pCtx = &pVM->cpum.s.Guest;

    /*
     * Initialize everything to ZERO first.
     */
    uint32_t    fUseFlags =  pVM->cpum.s.fUseFlags & ~CPUM_USED_FPU_SINCE_REM;
    memset(pCtx, 0, sizeof(*pCtx));
    pVM->cpum.s.fUseFlags   = fUseFlags;

    pCtx->cr0                       = X86_CR0_CD | X86_CR0_NW | X86_CR0_ET;  //0x60000010
    pCtx->eip                       = 0x0000fff0;
    pCtx->edx                       = 0x00000600;   /* P6 processor */
    pCtx->eflags.Bits.u1Reserved0   = 1;

    pCtx->cs                        = 0xf000;
    pCtx->csHid.u32Base             = 0xffff0000;
    pCtx->csHid.u32Limit            = 0x0000ffff;
    pCtx->csHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->csHid.Attr.n.u1Present    = 1;
    pCtx->csHid.Attr.n.u4Type       = X86_SEL_TYPE_READ | X86_SEL_TYPE_CODE;

    pCtx->dsHid.u32Limit            = 0x0000ffff;
    pCtx->dsHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->dsHid.Attr.n.u1Present    = 1;
    pCtx->dsHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->esHid.u32Limit            = 0x0000ffff;
    pCtx->esHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->esHid.Attr.n.u1Present    = 1;
    pCtx->esHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->fsHid.u32Limit            = 0x0000ffff;
    pCtx->fsHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->fsHid.Attr.n.u1Present    = 1;
    pCtx->fsHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->gsHid.u32Limit            = 0x0000ffff;
    pCtx->gsHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->gsHid.Attr.n.u1Present    = 1;
    pCtx->gsHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->ssHid.u32Limit            = 0x0000ffff;
    pCtx->ssHid.Attr.n.u1Present    = 1;
    pCtx->ssHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->ssHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->idtr.cbIdt                = 0xffff;
    pCtx->gdtr.cbGdt                = 0xffff;

    pCtx->ldtrHid.u32Limit          = 0xffff;
    pCtx->ldtrHid.Attr.n.u1Present  = 1;
    pCtx->ldtrHid.Attr.n.u4Type     = X86_SEL_TYPE_SYS_LDT;

    pCtx->trHid.u32Limit            = 0xffff;
    pCtx->trHid.Attr.n.u1Present    = 1;
    pCtx->trHid.Attr.n.u4Type       = X86_SEL_TYPE_SYS_286_TSS_BUSY;

    pCtx->dr6                       = 0xFFFF0FF0;
    pCtx->dr7                       = 0x400;

    pCtx->fpu.FTW                   = 0xff;         /* All tags are set, i.e. the regs are empty. */
    pCtx->fpu.FCW                   = 0x37f;

    /* Init PAT MSR */
    pCtx->msrPAT                    = 0x0007040600070406ULL; /* @todo correct? */
}



/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) cpumR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save.
     */
    SSMR3PutMem(pSSM, &pVM->cpum.s.Hyper, sizeof(pVM->cpum.s.Hyper));
    SSMR3PutMem(pSSM, &pVM->cpum.s.Guest, sizeof(pVM->cpum.s.Guest));
    SSMR3PutU32(pSSM, pVM->cpum.s.fUseFlags);
    SSMR3PutU32(pSSM, pVM->cpum.s.fChanged);

    SSMR3PutU32(pSSM, ELEMENTS(pVM->cpum.s.aGuestCpuIdStd));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], sizeof(pVM->cpum.s.aGuestCpuIdStd));

    SSMR3PutU32(pSSM, ELEMENTS(pVM->cpum.s.aGuestCpuIdExt));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    SSMR3PutU32(pSSM, ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3PutMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /* Add the cpuid for checking that the cpu is unchanged. */
    uint32_t au32CpuId[8] = {0};
    ASMCpuId(0, &au32CpuId[0], &au32CpuId[1], &au32CpuId[2], &au32CpuId[3]);
    ASMCpuId(1, &au32CpuId[4], &au32CpuId[5], &au32CpuId[6], &au32CpuId[7]);
    return SSMR3PutMem(pSSM, &au32CpuId[0], sizeof(au32CpuId));
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) cpumR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    /*
     * Validate version.
     */
    if (u32Version != CPUM_SAVED_STATE_VERSION)
    {
        Log(("cpuR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Restore.
     */
    uint32_t uCR3 = pVM->cpum.s.Hyper.cr3;
    uint32_t uESP = pVM->cpum.s.Hyper.esp; /* see VMMR3Relocate(). */
    SSMR3GetMem(pSSM, &pVM->cpum.s.Hyper, sizeof(pVM->cpum.s.Hyper));
    pVM->cpum.s.Hyper.cr3 = uCR3;
    pVM->cpum.s.Hyper.esp = uESP;
    SSMR3GetMem(pSSM, &pVM->cpum.s.Guest, sizeof(pVM->cpum.s.Guest));
    SSMR3GetU32(pSSM, &pVM->cpum.s.fUseFlags);
    SSMR3GetU32(pSSM, &pVM->cpum.s.fChanged);

    uint32_t cElements;
    int rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != ELEMENTS(pVM->cpum.s.aGuestCpuIdStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], sizeof(pVM->cpum.s.aGuestCpuIdStd));

    rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != ELEMENTS(pVM->cpum.s.aGuestCpuIdExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3GetMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /*
     * Check that the basic cpuid id information is unchanged.
     */
    uint32_t au32CpuId[8] = {0};
    ASMCpuId(0, &au32CpuId[0], &au32CpuId[1], &au32CpuId[2], &au32CpuId[3]);
    ASMCpuId(1, &au32CpuId[4], &au32CpuId[5], &au32CpuId[6], &au32CpuId[7]);
    uint32_t au32CpuIdSaved[8];
    rc = SSMR3GetMem(pSSM, &au32CpuIdSaved[0], sizeof(au32CpuIdSaved));
    if (VBOX_SUCCESS(rc))
    {
        /* Ignore APIC ID (AMD specs). */
        au32CpuId[5]      &= ~0xff000000;
        au32CpuIdSaved[5] &= ~0xff000000;
        /* Ignore the number of Logical CPUs (AMD specs). */
        au32CpuId[5]      &= ~0x00ff0000;
        au32CpuIdSaved[5] &= ~0x00ff0000;

        /* do the compare */
        if (memcmp(au32CpuIdSaved, au32CpuId, sizeof(au32CpuIdSaved)))
        {
            if (SSMR3HandleGetAfter(pSSM) == SSMAFTER_DEBUG_IT)
                LogRel(("cpumR3Load: CpuId mismatch! (ignored due to SSMAFTER_DEBUG_IT)\n"
                        "Saved=%.*Vhxs\n"
                        "Real =%.*Vhxs\n",
                        sizeof(au32CpuIdSaved), au32CpuIdSaved,
                        sizeof(au32CpuId), au32CpuId));
            else
            {
                LogRel(("cpumR3Load: CpuId mismatch!\n"
                        "Saved=%.*Vhxs\n"
                        "Real =%.*Vhxs\n",
                        sizeof(au32CpuIdSaved), au32CpuIdSaved,
                        sizeof(au32CpuId), au32CpuId));
                rc = VERR_SSM_LOAD_CPUID_MISMATCH;
            }
        }
    }

    return rc;
}


/**
 * Formats the EFLAGS value into mnemonics.
 *
 * @param   pszEFlags   Where to write the mnemonics. (Assumes sufficient buffer space.)
 * @param   efl         The EFLAGS value.
 */
static void cpumR3InfoFormatFlags(char *pszEFlags, uint32_t efl)
{
    /*
     * Format the flags.
     */
    static struct
    {
        const char *pszSet; const char *pszClear; uint32_t fFlag;
    }   s_aFlags[] =
    {
        { "vip",NULL, X86_EFL_VIP },
        { "vif",NULL, X86_EFL_VIF },
        { "ac", NULL, X86_EFL_AC },
        { "vm", NULL, X86_EFL_VM },
        { "rf", NULL, X86_EFL_RF },
        { "nt", NULL, X86_EFL_NT },
        { "ov", "nv", X86_EFL_OF },
        { "dn", "up", X86_EFL_DF },
        { "ei", "di", X86_EFL_IF },
        { "tf", NULL, X86_EFL_TF },
        { "nt", "pl", X86_EFL_SF },
        { "nz", "zr", X86_EFL_ZF },
        { "ac", "na", X86_EFL_AF },
        { "po", "pe", X86_EFL_PF },
        { "cy", "nc", X86_EFL_CF },
    };
    char *psz = pszEFlags;
    for (unsigned i = 0; i < ELEMENTS(s_aFlags); i++)
    {
        const char *pszAdd = s_aFlags[i].fFlag & efl ? s_aFlags[i].pszSet : s_aFlags[i].pszClear;
        if (pszAdd)
        {
            strcpy(psz, pszAdd);
            psz += strlen(pszAdd);
            *psz++ = ' ';
        }
    }
    psz[-1] = '\0';
}


/**
 * Formats a full register dump.
 *
 * @param   pCtx        The context to format.
 * @param   pCtxCore    The context core to format.
 * @param   pHlp        Output functions.
 * @param   enmType     The dump type.
 * @param   pszPrefix   Register name prefix.
 */
static void cpumR3InfoOne(PCPUMCTX pCtx, PCCPUMCTXCORE pCtxCore, PCDBGFINFOHLP pHlp, CPUMDUMPTYPE enmType, const char *pszPrefix)
{
    /*
     * Format the EFLAGS.
     */
    uint32_t efl = pCtxCore->eflags.u32;
    char szEFlags[80];
    cpumR3InfoFormatFlags(&szEFlags[0], efl);

    /*
     * Format the registers.
     */
    switch (enmType)
    {
        case CPUMDUMPTYPE_TERSE:
            pHlp->pfnPrintf(pHlp,
                "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x                %seflags=%08x\n",
                pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, efl);
            break;

        case CPUMDUMPTYPE_DEFAULT:
            pHlp->pfnPrintf(pHlp,
                "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %str=%04x      %seflags=%08x\n"
                "%scr0=%08RX64 %scr2=%08RX64 %scr3=%08RX64 %scr4=%08RX64 %sgdtr=%08x:%04x %sldtr=%04x\n"
                ,
                pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, (RTSEL)pCtx->tr, pszPrefix, efl,
                pszPrefix, pCtx->cr0, pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3, pszPrefix, pCtx->cr4,
                pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, (RTSEL)pCtx->ldtr);
            break;

        case CPUMDUMPTYPE_VERBOSE:
            pHlp->pfnPrintf(pHlp,
                "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                "%scs={%04x base=%08x limit=%08x flags=%08x} %sdr0=%08RX64 %sdr1=%08RX64\n"
                "%sds={%04x base=%08x limit=%08x flags=%08x} %sdr2=%08RX64 %sdr3=%08RX64\n"
                "%ses={%04x base=%08x limit=%08x flags=%08x} %sdr4=%08RX64 %sdr5=%08RX64\n"
                "%sfs={%04x base=%08x limit=%08x flags=%08x} %sdr6=%08RX64 %sdr7=%08RX64\n"
                "%sgs={%04x base=%08x limit=%08x flags=%08x} %scr0=%08RX64 %scr2=%08RX64\n"
                "%sss={%04x base=%08x limit=%08x flags=%08x} %scr3=%08RX64 %scr4=%08RX64\n"
                "%sgdtr=%08x:%04x  %sidtr=%08x:%04x  %seflags=%08x\n"
                "%sldtr={%04x base=%08x limit=%08x flags=%08x}\n"
                "%str  ={%04x base=%08x limit=%08x flags=%08x}\n"
                "%sSysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
                ,
                pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                pszPrefix, (RTSEL)pCtxCore->cs, pCtx->csHid.u32Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pszPrefix, pCtx->dr0,  pszPrefix, pCtx->dr1,
                pszPrefix, (RTSEL)pCtxCore->ds, pCtx->dsHid.u32Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pszPrefix, pCtx->dr2,  pszPrefix, pCtx->dr3,
                pszPrefix, (RTSEL)pCtxCore->es, pCtx->esHid.u32Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pszPrefix, pCtx->dr4,  pszPrefix, pCtx->dr5,
                pszPrefix, (RTSEL)pCtxCore->fs, pCtx->fsHid.u32Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pszPrefix, pCtx->dr6,  pszPrefix, pCtx->dr7,
                pszPrefix, (RTSEL)pCtxCore->gs, pCtx->gsHid.u32Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2,
                pszPrefix, (RTSEL)pCtxCore->ss, pCtx->ssHid.u32Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, efl,
                pszPrefix, (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u32Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
                pszPrefix, (RTSEL)pCtx->tr, pCtx->trHid.u32Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
                pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);

            pHlp->pfnPrintf(pHlp,
                "FPU:\n"
                "%sFCW=%04x %sFSW=%04x %sFTW=%02x\n"
                "%sres1=%02x %sFOP=%04x %sFPUIP=%08x %sCS=%04x %sRsvrd1=%04x\n"
                "%sFPUDP=%04x %sDS=%04x %sRsvrd2=%04x %sMXCSR=%08x %sMXCSR_MASK=%08x\n"
                ,
                pszPrefix, pCtx->fpu.FCW, pszPrefix, pCtx->fpu.FSW, pszPrefix, pCtx->fpu.FTW,
                pszPrefix, pCtx->fpu.huh1, pszPrefix, pCtx->fpu.FOP, pszPrefix, pCtx->fpu.FPUIP, pszPrefix, pCtx->fpu.CS, pszPrefix, pCtx->fpu.Rsvrd1,
                pszPrefix, pCtx->fpu.FPUDP, pszPrefix, pCtx->fpu.DS, pszPrefix, pCtx->fpu.Rsrvd2,
                pszPrefix, pCtx->fpu.MXCSR, pszPrefix, pCtx->fpu.MXCSR_MASK);


            break;
    }
}


/**
 * Display all cpu states and any other cpum info.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    cpumR3InfoGuest(pVM, pHlp, pszArgs);
    cpumR3InfoHyper(pVM, pHlp, pszArgs);
    cpumR3InfoHost(pVM, pHlp, pszArgs);
}


/**
 * Parses the info argument.
 *
 * The argument starts with 'verbose', 'terse' or 'default' and then
 * continues with the comment string.
 *
 * @param   pszArgs         The pointer to the argument string.
 * @param   penmType        Where to store the dump type request.
 * @param   ppszComment     Where to store the pointer to the comment string.
 */
static void cpumR3InfoParseArg(const char *pszArgs, CPUMDUMPTYPE *penmType, const char **ppszComment)
{
    if (!pszArgs)
    {
        *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = "";
    }
    else
    {
        if (!strncmp(pszArgs, "verbose", sizeof("verbose") - 1))
        {
            pszArgs += 5;
            *penmType = CPUMDUMPTYPE_VERBOSE;
        }
        else if (!strncmp(pszArgs, "terse", sizeof("terse") - 1))
        {
            pszArgs += 5;
            *penmType = CPUMDUMPTYPE_TERSE;
        }
        else if (!strncmp(pszArgs, "default", sizeof("default") - 1))
        {
            pszArgs += 7;
            *penmType = CPUMDUMPTYPE_DEFAULT;
        }
        else
            *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = RTStrStripL(pszArgs);
    }
}


/**
 * Display the guest cpu state.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Guest CPUM state: %s\n", pszComment);
    cpumR3InfoOne(&pVM->cpum.s.Guest, CPUMCTX2CORE(&pVM->cpum.s.Guest), pHlp, enmType, "");
}


/**
 * Display the hypervisor cpu state.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoHyper(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Hypervisor CPUM state: %s\n", pszComment);
    cpumR3InfoOne(&pVM->cpum.s.Hyper, pVM->cpum.s.pHyperCoreR3, pHlp, enmType, ".");
    pHlp->pfnPrintf(pHlp, "CR4OrMask=%#x CR4AndMask=%#x\n", pVM->cpum.s.CR4.OrMask, pVM->cpum.s.CR4.AndMask);
}


/**
 * Display the host cpu state.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoHost(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Host CPUM state: %s\n", pszComment);

    /*
     * Format the EFLAGS.
     */
    PCPUMHOSTCTX pCtx = &pVM->cpum.s.Host;
#if HC_ARCH_BITS == 32
    uint32_t efl = pCtx->eflags.u32;
#else
    uint64_t efl = pCtx->rflags;
#endif
    char szEFlags[80];
    cpumR3InfoFormatFlags(&szEFlags[0], efl);

    /*
     * Format the registers.
     */
#if HC_ARCH_BITS == 32
# ifdef VBOX_WITH_HYBIRD_32BIT_KERNEL
    if (!(pCtx->efer & MSR_K6_EFER_LMA))
# endif
    {
        pHlp->pfnPrintf(pHlp,
            "eax=xxxxxxxx ebx=%08x ecx=xxxxxxxx edx=xxxxxxxx esi=%08x edi=%08x\n"
            "eip=xxxxxxxx esp=%08x ebp=%08x iopl=%d %31s\n"
            "cs=%04x ds=%04x es=%04x fs=%04x gs=%04x                       eflags=%08x\n"
            "cr0=%08RX64 cr2=xxxxxxxx cr3=%08RX64 cr4=%08RX64 gdtr=%08x:%04x ldtr=%04x\n"
            "dr0=%08RX64 dr1=%08RX64x dr2=%08RX64 dr3=%08RX64x dr6=%08RX64 dr7=%08RX64\n"
            "SysEnter={cs=%04x eip=%08x esp=%08x}\n"
            ,
            /*pCtx->eax,*/ pCtx->ebx, /*pCtx->ecx, pCtx->edx,*/ pCtx->esi, pCtx->edi,
            /*pCtx->eip,*/ pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(efl), szEFlags,
            (RTSEL)pCtx->cs, (RTSEL)pCtx->ds, (RTSEL)pCtx->es, (RTSEL)pCtx->fs, (RTSEL)pCtx->gs, efl,
            pCtx->cr0, /*pCtx->cr2,*/ pCtx->cr3, pCtx->cr4,
            pCtx->dr0, pCtx->dr1, pCtx->dr2, pCtx->dr3, pCtx->dr6, pCtx->dr7,
            (uint32_t)pCtx->gdtr.uAddr, pCtx->gdtr.cb, (RTSEL)pCtx->ldtr,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);
    }
# ifdef VBOX_WITH_HYBIRD_32BIT_KERNEL
    else
# endif
#endif
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBIRD_32BIT_KERNEL)
    {
        pHlp->pfnPrintf(pHlp,
            "rax=xxxxxxxxxxxxxxxx rbx=%016RX64 rcx=xxxxxxxxxxxxxxxx\n"
            "rdx=xxxxxxxxxxxxxxxx rsi=%016RX64 rdi=%016RX64\n"
            "rip=xxxxxxxxxxxxxxxx rsp=%016RX64 rbp=%016RX64\n"
            " r8=xxxxxxxxxxxxxxxx  r9=xxxxxxxxxxxxxxxx r10=%016RX64\n"
            "r11=%016RX64 r12=%016RX64 r13=%016RX64\n"
            "r14=%016RX64 r15=%016RX64\n"
            "iopl=%d  %31s\n"
            "cs=%04x  ds=%04x  es=%04x  fs=%04x  gs=%04x                   eflags=%08RX64\n"
            "cr0=%016RX64 cr2=xxxxxxxxxxxxxxxx cr3=%016RX64\n"
            "cr4=%016RX64 cr8=%016RX64 ldtr=%04x tr=%04x\n"
            "dr0=%016RX64 dr1=%016RX64 dr2=%016RX64\n"
            "dr3=%016RX64 dr6=%016RX64 dr7=%016RX64\n"
            "gdtr=%016RX64:%04x  idtr=%016RX64:%04x\n"
            "SysEnter={cs=%04x eip=%08x esp=%08x}\n"
            "FSbase=%016RX64 GSbase=%016RX64 efer=%08RX64\n"
            ,
            /*pCtx->rax,*/ pCtx->rbx, /*pCtx->rcx,
            pCtx->rdx,*/ pCtx->rsi, pCtx->rdi,
            /*pCtx->rip,*/ pCtx->rsp, pCtx->rbp,
            /*pCtx->r8,  pCtx->r9,*/  pCtx->r10,
            pCtx->r11, pCtx->r12, pCtx->r13,
            pCtx->r14, pCtx->r15,
            X86_EFL_GET_IOPL(efl), szEFlags,
            (RTSEL)pCtx->cs, (RTSEL)pCtx->ds, (RTSEL)pCtx->es, (RTSEL)pCtx->fs, (RTSEL)pCtx->gs, efl,
            pCtx->cr0, /*pCtx->cr2,*/ pCtx->cr3,
            pCtx->cr4, pCtx->cr8, pCtx->ldtr, pCtx->tr,
            pCtx->dr0, pCtx->dr1, pCtx->dr2,
            pCtx->dr3, pCtx->dr6, pCtx->dr7,
            pCtx->gdtr.uAddr, pCtx->gdtr.cb, pCtx->idtr.uAddr, pCtx->idtr.cb,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp,
            pCtx->FSbase, pCtx->GSbase, pCtx->efer);
    }
#endif
}

/**
 * Get L1 cache / TLS associativity.
 */
static const char *getCacheAss(unsigned u, char *pszBuf)
{
    if (u == 0)
        return "res0  ";
    if (u == 1)
        return "direct";
    if (u >= 256)
        return "???";

    RTStrPrintf(pszBuf, 16, "%d way", u);
    return pszBuf;
}


/**
 * Get L2 cache soociativity.
 */
const char *getL2CacheAss(unsigned u)
{
    switch (u)
    {
        case 0:  return "off   ";
        case 1:  return "direct";
        case 2:  return "2 way ";
        case 3:  return "res3  ";
        case 4:  return "4 way ";
        case 5:  return "res5  ";
        case 6:  return "8 way ";                                    case 7:  return "res7  ";
        case 8:  return "16 way";
        case 9:  return "res9  ";
        case 10: return "res10 ";
        case 11: return "res11 ";
        case 12: return "res12 ";
        case 13: return "res13 ";
        case 14: return "res14 ";
        case 15: return "fully ";
        default:
            return "????";
    }
}


/**
 * Display the guest CpuId leaves.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     "terse", "default" or "verbose".
 */
static DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /*
     * Parse the argument.
     */
    unsigned iVerbosity = 1;
    if (pszArgs)
    {
        pszArgs = RTStrStripL(pszArgs);
        if (!strcmp(pszArgs, "terse"))
            iVerbosity--;
        else if (!strcmp(pszArgs, "verbose"))
            iVerbosity++;
    }

    /*
     * Start cracking.
     */
    CPUMCPUID   Host;
    CPUMCPUID   Guest;
    unsigned    cStdMax = pVM->cpum.s.aGuestCpuIdStd[0].eax;

    pHlp->pfnPrintf(pHlp,
                    "         RAW Standard CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[i];
        ASMCpuId_Idx_ECX(i, 0, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cStdMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * If verbose, decode it.
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[0];
        pHlp->pfnPrintf(pHlp,
                        "Name:                            %.04s%.04s%.04s\n"
                        "Supports:                        0-%x\n",
                        &Guest.ebx, &Guest.edx, &Guest.ecx, Guest.eax);
    }

    /*
     * Get Features.
     */
    bool const fIntel = ASMIsIntelCpuEx(pVM->cpum.s.aGuestCpuIdStd[0].ebx,
                                        pVM->cpum.s.aGuestCpuIdStd[0].ecx,
                                        pVM->cpum.s.aGuestCpuIdStd[0].edx);
    if (cStdMax >= 1 && iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[1];
        uint32_t uEAX = Guest.eax;

        pHlp->pfnPrintf(pHlp,
                        "Family:                          %d  \tExtended: %d \tEffectiv: %d\n"
                        "Model:                           %d  \tExtended: %d \tEffectiv: %d\n"
                        "Stepping:                        %d\n"
                        "APIC ID:                         %#04x\n"
                        "Logical CPUs:                    %d\n"
                        "CLFLUSH Size:                    %d\n"
                        "Brand ID:                        %#04x\n",
                        (uEAX >> 8) & 0xf, (uEAX >> 20) & 0x7f, ASMGetCpuFamily(uEAX),
                        (uEAX >> 4) & 0xf, (uEAX >> 16) & 0x0f, ASMGetCpuModel(uEAX, fIntel),
                        ASMGetCpuStepping(uEAX),
                        (Guest.ebx >> 24) & 0xff,
                        (Guest.ebx >> 16) & 0xff,
                        (Guest.ebx >>  8) & 0xff,
                        (Guest.ebx >>  0) & 0xff);
        if (iVerbosity == 1)
        {
            uint32_t uEDX = Guest.edx;
            pHlp->pfnPrintf(pHlp, "Features EDX:                   ");
            if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " FPU");
            if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " VME");
            if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DE");
            if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " PSE");
            if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TSC");
            if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " MSR");
            if (uEDX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " PAE");
            if (uEDX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MCE");
            if (uEDX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " CX8");
            if (uEDX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " APIC");
            if (uEDX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " 10");
            if (uEDX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SEP");
            if (uEDX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " MTRR");
            if (uEDX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PGE");
            if (uEDX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " MCA");
            if (uEDX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " CMOV");
            if (uEDX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " PAT");
            if (uEDX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PSE36");
            if (uEDX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " PSN");
            if (uEDX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " CLFSH");
            if (uEDX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " 20");
            if (uEDX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " DS");
            if (uEDX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " ACPI");
            if (uEDX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " MMX");
            if (uEDX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " FXSR");
            if (uEDX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " SSE");
            if (uEDX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " SSE2");
            if (uEDX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " SS");
            if (uEDX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " HTT");
            if (uEDX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " TM");
            if (uEDX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " 30");
            if (uEDX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " PBE");
            pHlp->pfnPrintf(pHlp, "\n");

            uint32_t uECX = Guest.ecx;
            pHlp->pfnPrintf(pHlp, "Features ECX:                   ");
            if (uECX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " SSE3");
            if (uECX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " 1");
            if (uECX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " 2");
            if (uECX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " MONITOR");
            if (uECX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " DS-CPL");
            if (uECX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " VMX");
            if (uECX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " 6");
            if (uECX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " EST");
            if (uECX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " TM2");
            if (uECX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " 9");
            if (uECX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " CNXT-ID");
            if (uECX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " 11");
            if (uECX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " 12");
            if (uECX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " CX16");
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if (uECX & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            ASMCpuId(1, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

            X86CPUIDFEATEDX EdxHost  = *(PX86CPUIDFEATEDX)&Host.edx;
            X86CPUIDFEATECX EcxHost  = *(PX86CPUIDFEATECX)&Host.ecx;
            X86CPUIDFEATEDX EdxGuest = *(PX86CPUIDFEATEDX)&Guest.edx;
            X86CPUIDFEATECX EcxGuest = *(PX86CPUIDFEATECX)&Guest.ecx;

            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "FPU - x87 FPU on Chip                  = %d (%d)\n",  EdxGuest.u1FPU,        EdxHost.u1FPU);
            pHlp->pfnPrintf(pHlp, "VME - Virtual 8086 Mode Enhancements   = %d (%d)\n",  EdxGuest.u1VME,        EdxHost.u1VME);
            pHlp->pfnPrintf(pHlp, "DE - Debugging extensions              = %d (%d)\n",  EdxGuest.u1DE,         EdxHost.u1DE);
            pHlp->pfnPrintf(pHlp, "PSE - Page Size Extension              = %d (%d)\n",  EdxGuest.u1PSE,        EdxHost.u1PSE);
            pHlp->pfnPrintf(pHlp, "TSC - Time Stamp Counter               = %d (%d)\n",  EdxGuest.u1TSC,        EdxHost.u1TSC);
            pHlp->pfnPrintf(pHlp, "MSR - Model Specific Registers         = %d (%d)\n",  EdxGuest.u1MSR,        EdxHost.u1MSR);
            pHlp->pfnPrintf(pHlp, "PAE - Physical Address Extension       = %d (%d)\n",  EdxGuest.u1PAE,        EdxHost.u1PAE);
            pHlp->pfnPrintf(pHlp, "MCE - Machine Check Exception          = %d (%d)\n",  EdxGuest.u1MCE,        EdxHost.u1MCE);
            pHlp->pfnPrintf(pHlp, "CX8 - CMPXCHG8B instruction            = %d (%d)\n",  EdxGuest.u1CX8,        EdxHost.u1CX8);
            pHlp->pfnPrintf(pHlp, "APIC - APIC On-Chip                    = %d (%d)\n",  EdxGuest.u1APIC,       EdxHost.u1APIC);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EdxGuest.u1Reserved1,  EdxHost.u1Reserved1);
            pHlp->pfnPrintf(pHlp, "SEP - SYSENTER and SYSEXIT             = %d (%d)\n",  EdxGuest.u1SEP,        EdxHost.u1SEP);
            pHlp->pfnPrintf(pHlp, "MTRR - Memory Type Range Registers     = %d (%d)\n",  EdxGuest.u1MTRR,       EdxHost.u1MTRR);
            pHlp->pfnPrintf(pHlp, "PGE - PTE Global Bit                   = %d (%d)\n",  EdxGuest.u1PGE,        EdxHost.u1PGE);
            pHlp->pfnPrintf(pHlp, "MCA - Machine Check Architecture       = %d (%d)\n",  EdxGuest.u1MCA,        EdxHost.u1MCA);
            pHlp->pfnPrintf(pHlp, "CMOV - Conditional Move Instructions   = %d (%d)\n",  EdxGuest.u1CMOV,       EdxHost.u1CMOV);
            pHlp->pfnPrintf(pHlp, "PAT - Page Attribute Table             = %d (%d)\n",  EdxGuest.u1PAT,        EdxHost.u1PAT);
            pHlp->pfnPrintf(pHlp, "PSE-36 - 36-bit Page Size Extention    = %d (%d)\n",  EdxGuest.u1PSE36,      EdxHost.u1PSE36);
            pHlp->pfnPrintf(pHlp, "PSN - Processor Serial Number          = %d (%d)\n",  EdxGuest.u1PSN,        EdxHost.u1PSN);
            pHlp->pfnPrintf(pHlp, "CLFSH - CLFLUSH Instruction.           = %d (%d)\n",  EdxGuest.u1CLFSH,      EdxHost.u1CLFSH);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EdxGuest.u1Reserved2,  EdxHost.u1Reserved2);
            pHlp->pfnPrintf(pHlp, "DS - Debug Store                       = %d (%d)\n",  EdxGuest.u1DS,         EdxHost.u1DS);
            pHlp->pfnPrintf(pHlp, "ACPI - Thermal Mon. & Soft. Clock Ctrl.= %d (%d)\n",  EdxGuest.u1ACPI,       EdxHost.u1ACPI);
            pHlp->pfnPrintf(pHlp, "MMX - Intel MMX Technology             = %d (%d)\n",  EdxGuest.u1MMX,        EdxHost.u1MMX);
            pHlp->pfnPrintf(pHlp, "FXSR - FXSAVE and FXRSTOR Instructions = %d (%d)\n",  EdxGuest.u1FXSR,       EdxHost.u1FXSR);
            pHlp->pfnPrintf(pHlp, "SSE - SSE Support                      = %d (%d)\n",  EdxGuest.u1SSE,        EdxHost.u1SSE);
            pHlp->pfnPrintf(pHlp, "SSE2 - SSE2 Support                    = %d (%d)\n",  EdxGuest.u1SSE2,       EdxHost.u1SSE2);
            pHlp->pfnPrintf(pHlp, "SS - Self Snoop                        = %d (%d)\n",  EdxGuest.u1SS,         EdxHost.u1SS);
            pHlp->pfnPrintf(pHlp, "HTT - Hyper-Threading Technolog        = %d (%d)\n",  EdxGuest.u1HTT,        EdxHost.u1HTT);
            pHlp->pfnPrintf(pHlp, "TM - Thermal Monitor                   = %d (%d)\n",  EdxGuest.u1TM,         EdxHost.u1TM);
            pHlp->pfnPrintf(pHlp, "30 - Reserved                          = %d (%d)\n",  EdxGuest.u1Reserved3,  EdxHost.u1Reserved3);
            pHlp->pfnPrintf(pHlp, "PBE - Pending Break Enable             = %d (%d)\n",  EdxGuest.u1PBE,        EdxHost.u1PBE);

            pHlp->pfnPrintf(pHlp, "Supports SSE3 or not                   = %d (%d)\n",  EcxGuest.u1SSE3,       EcxHost.u1SSE3);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EcxGuest.u2Reserved1,  EcxHost.u2Reserved1);
            pHlp->pfnPrintf(pHlp, "Supports MONITOR/MWAIT                 = %d (%d)\n",  EcxGuest.u1Monitor,    EcxHost.u1Monitor);
            pHlp->pfnPrintf(pHlp, "CPL-DS - CPL Qualified Debug Store     = %d (%d)\n",  EcxGuest.u1CPLDS,      EcxHost.u1CPLDS);
            pHlp->pfnPrintf(pHlp, "VMX - Virtual Machine Technology       = %d (%d)\n",  EcxGuest.u1VMX,        EcxHost.u1VMX);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EcxGuest.u1Reserved2,  EcxHost.u1Reserved2);
            pHlp->pfnPrintf(pHlp, "Enhanced SpeedStep Technology          = %d (%d)\n",  EcxGuest.u1EST,        EcxHost.u1EST);
            pHlp->pfnPrintf(pHlp, "Terminal Monitor 2                     = %d (%d)\n",  EcxGuest.u1TM2,        EcxHost.u1TM2);
            pHlp->pfnPrintf(pHlp, "Supports Supplemental SSE3 or not      = %d (%d)\n",  EcxGuest.u1SSSE3,      EcxHost.u1SSSE3);
            pHlp->pfnPrintf(pHlp, "L1 Context ID                          = %d (%d)\n",  EcxGuest.u1CNTXID,     EcxHost.u1CNTXID);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %#x (%#x)\n",EcxGuest.u2Reserved4,  EcxHost.u2Reserved4);
            pHlp->pfnPrintf(pHlp, "CMPXCHG16B                             = %d (%d)\n",  EcxGuest.u1CX16,       EcxHost.u1CX16);
            pHlp->pfnPrintf(pHlp, "xTPR Update Control                    = %d (%d)\n",  EcxGuest.u1TPRUpdate,  EcxHost.u1TPRUpdate);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %#x (%#x)\n",EcxGuest.u17Reserved5, EcxHost.u17Reserved5);
        }
    }
    if (cStdMax >= 2 && iVerbosity)
    {
        /** @todo */
    }

    /*
     * Extended.
     * Implemented after AMD specs.
     */
    unsigned    cExtMax = pVM->cpum.s.aGuestCpuIdExt[0].eax & 0xffff;

    pHlp->pfnPrintf(pHlp,
                    "\n"
                    "         RAW Extended CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < ELEMENTS(pVM->cpum.s.aGuestCpuIdExt); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[i];
        ASMCpuId(0x80000000 | i, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        0x80000000 | i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cExtMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * Understandable output
     */
    if (iVerbosity && cExtMax >= 0)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[0];
        pHlp->pfnPrintf(pHlp,
                        "Ext Name:                        %.4s%.4s%.4s\n"
                        "Ext Supports:                    0x80000000-%#010x\n",
                        &Guest.ebx, &Guest.edx, &Guest.ecx, Guest.eax);
    }

    if (iVerbosity && cExtMax >= 1)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[1];
        uint32_t uEAX = Guest.eax;
        pHlp->pfnPrintf(pHlp,
                        "Family:                          %d  \tExtended: %d \tEffectiv: %d\n"
                        "Model:                           %d  \tExtended: %d \tEffectiv: %d\n"
                        "Stepping:                        %d\n"
                        "Brand ID:                        %#05x\n",
                        (uEAX >> 8) & 0xf, (uEAX >> 20) & 0x7f, ASMGetCpuFamily(uEAX),
                        (uEAX >> 4) & 0xf, (uEAX >> 16) & 0x0f, ASMGetCpuModel(uEAX, fIntel),
                        ASMGetCpuStepping(uEAX),
                        Guest.ebx & 0xfff);

        if (iVerbosity == 1)
        {
            uint32_t uEDX = Guest.edx;
            pHlp->pfnPrintf(pHlp, "Features EDX:                   ");
            if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " FPU");
            if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " VME");
            if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DE");
            if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " PSE");
            if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TSC");
            if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " MSR");
            if (uEDX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " PAE");
            if (uEDX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MCE");
            if (uEDX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " CX8");
            if (uEDX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " APIC");
            if (uEDX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " 10");
            if (uEDX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SCR");
            if (uEDX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " MTRR");
            if (uEDX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PGE");
            if (uEDX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " MCA");
            if (uEDX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " CMOV");
            if (uEDX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " PAT");
            if (uEDX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PSE36");
            if (uEDX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " 18");
            if (uEDX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " 19");
            if (uEDX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " NX");
            if (uEDX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " 21");
            if (uEDX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " ExtMMX");
            if (uEDX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " MMX");
            if (uEDX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " FXSR");
            if (uEDX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " FastFXSR");
            if (uEDX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " Page1GB");
            if (uEDX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " RDTSCP");
            if (uEDX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " 28");
            if (uEDX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " LongMode");
            if (uEDX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " Ext3DNow");
            if (uEDX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " 3DNow");
            pHlp->pfnPrintf(pHlp, "\n");

            uint32_t uECX = Guest.ecx;
            pHlp->pfnPrintf(pHlp, "Features ECX:                   ");
            if (uECX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " LAHF/SAHF");
            if (uECX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " CMPL");
            if (uECX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " SVM");
            if (uECX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " ExtAPIC");
            if (uECX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " CR8L");
            if (uECX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " ABM");
            if (uECX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " SSE4A");
            if (uECX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MISALNSSE");
            if (uECX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " 3DNOWPRF");
            if (uECX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " OSVW");
            if (uECX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " IBS");
            if (uECX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SSE5");
            if (uECX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " SKINIT");
            if (uECX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " WDT");
            for (unsigned iBit = 5; iBit < 32; iBit++)
                if (uECX & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            ASMCpuId(0x80000001, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

            uint32_t uEdxGst = Guest.edx;
            uint32_t uEdxHst = Host.edx;
            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "FPU - x87 FPU on Chip                  = %d (%d)\n",  !!(uEdxGst & RT_BIT( 0)),  !!(uEdxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "VME - Virtual 8086 Mode Enhancements   = %d (%d)\n",  !!(uEdxGst & RT_BIT( 1)),  !!(uEdxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "DE - Debugging extensions              = %d (%d)\n",  !!(uEdxGst & RT_BIT( 2)),  !!(uEdxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "PSE - Page Size Extension              = %d (%d)\n",  !!(uEdxGst & RT_BIT( 3)),  !!(uEdxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "TSC - Time Stamp Counter               = %d (%d)\n",  !!(uEdxGst & RT_BIT( 4)),  !!(uEdxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "MSR - K86 Model Specific Registers     = %d (%d)\n",  !!(uEdxGst & RT_BIT( 5)),  !!(uEdxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "PAE - Physical Address Extension       = %d (%d)\n",  !!(uEdxGst & RT_BIT( 6)),  !!(uEdxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "MCE - Machine Check Exception          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 7)),  !!(uEdxHst & RT_BIT( 7)));
            pHlp->pfnPrintf(pHlp, "CX8 - CMPXCHG8B instruction            = %d (%d)\n",  !!(uEdxGst & RT_BIT( 8)),  !!(uEdxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "APIC - APIC On-Chip                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 9)),  !!(uEdxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "10 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(10)),  !!(uEdxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "SEP - SYSCALL and SYSRET               = %d (%d)\n",  !!(uEdxGst & RT_BIT(11)),  !!(uEdxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "MTRR - Memory Type Range Registers     = %d (%d)\n",  !!(uEdxGst & RT_BIT(12)),  !!(uEdxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "PGE - PTE Global Bit                   = %d (%d)\n",  !!(uEdxGst & RT_BIT(13)),  !!(uEdxHst & RT_BIT(13)));
            pHlp->pfnPrintf(pHlp, "MCA - Machine Check Architecture       = %d (%d)\n",  !!(uEdxGst & RT_BIT(14)),  !!(uEdxHst & RT_BIT(14)));
            pHlp->pfnPrintf(pHlp, "CMOV - Conditional Move Instructions   = %d (%d)\n",  !!(uEdxGst & RT_BIT(15)),  !!(uEdxHst & RT_BIT(15)));
            pHlp->pfnPrintf(pHlp, "PAT - Page Attribute Table             = %d (%d)\n",  !!(uEdxGst & RT_BIT(16)),  !!(uEdxHst & RT_BIT(16)));
            pHlp->pfnPrintf(pHlp, "PSE-36 - 36-bit Page Size Extention    = %d (%d)\n",  !!(uEdxGst & RT_BIT(17)),  !!(uEdxHst & RT_BIT(17)));
            pHlp->pfnPrintf(pHlp, "18 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(18)),  !!(uEdxHst & RT_BIT(18)));
            pHlp->pfnPrintf(pHlp, "19 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(19)),  !!(uEdxHst & RT_BIT(19)));
            pHlp->pfnPrintf(pHlp, "NX - No-Execute Page Protection        = %d (%d)\n",  !!(uEdxGst & RT_BIT(20)),  !!(uEdxHst & RT_BIT(20)));
            pHlp->pfnPrintf(pHlp, "DS - Debug Store                       = %d (%d)\n",  !!(uEdxGst & RT_BIT(21)),  !!(uEdxHst & RT_BIT(21)));
            pHlp->pfnPrintf(pHlp, "AXMMX - AMD Extensions to MMX Instr.   = %d (%d)\n",  !!(uEdxGst & RT_BIT(22)),  !!(uEdxHst & RT_BIT(22)));
            pHlp->pfnPrintf(pHlp, "MMX - Intel MMX Technology             = %d (%d)\n",  !!(uEdxGst & RT_BIT(23)),  !!(uEdxHst & RT_BIT(23)));
            pHlp->pfnPrintf(pHlp, "FXSR - FXSAVE and FXRSTOR Instructions = %d (%d)\n",  !!(uEdxGst & RT_BIT(24)),  !!(uEdxHst & RT_BIT(24)));
            pHlp->pfnPrintf(pHlp, "25 - AMD fast FXSAVE and FXRSTOR Instr.= %d (%d)\n",  !!(uEdxGst & RT_BIT(25)),  !!(uEdxHst & RT_BIT(25)));
            pHlp->pfnPrintf(pHlp, "26 - 1 GB large page support           = %d (%d)\n",  !!(uEdxGst & RT_BIT(26)),  !!(uEdxHst & RT_BIT(26)));
            pHlp->pfnPrintf(pHlp, "27 - RDTSCP instruction                = %d (%d)\n",  !!(uEdxGst & RT_BIT(27)),  !!(uEdxHst & RT_BIT(27)));
            pHlp->pfnPrintf(pHlp, "28 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(28)),  !!(uEdxHst & RT_BIT(28)));
            pHlp->pfnPrintf(pHlp, "29 - AMD Long Mode                     = %d (%d)\n",  !!(uEdxGst & RT_BIT(29)),  !!(uEdxHst & RT_BIT(29)));
            pHlp->pfnPrintf(pHlp, "30 - AMD Extensions to 3DNow           = %d (%d)\n",  !!(uEdxGst & RT_BIT(30)),  !!(uEdxHst & RT_BIT(30)));
            pHlp->pfnPrintf(pHlp, "31 - AMD 3DNow                         = %d (%d)\n",  !!(uEdxGst & RT_BIT(31)),  !!(uEdxHst & RT_BIT(31)));

            uint32_t uEcxGst = Guest.ecx;
            uint32_t uEcxHst = Host.ecx;
            pHlp->pfnPrintf(pHlp, "LahfSahf - LAHF/SAHF in 64-bit mode    = %d (%d)\n",  !!(uEcxGst & RT_BIT( 0)),  !!(uEcxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "CmpLegacy - Core MP legacy mode (depr) = %d (%d)\n",  !!(uEcxGst & RT_BIT( 1)),  !!(uEcxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "SVM - AMD VM Extensions                = %d (%d)\n",  !!(uEcxGst & RT_BIT( 2)),  !!(uEcxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "APIC registers starting at 0x400       = %d (%d)\n",  !!(uEcxGst & RT_BIT( 3)),  !!(uEcxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "AltMovCR8 - LOCK MOV CR0 means MOV CR8 = %d (%d)\n",  !!(uEcxGst & RT_BIT( 4)),  !!(uEcxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "Advanced bit manipulation              = %d (%d)\n",  !!(uEcxGst & RT_BIT( 5)),  !!(uEcxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "SSE4A instruction support              = %d (%d)\n",  !!(uEcxGst & RT_BIT( 6)),  !!(uEcxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "Misaligned SSE mode                    = %d (%d)\n",  !!(uEcxGst & RT_BIT( 7)),  !!(uEcxHst & RT_BIT( 7)));
            pHlp->pfnPrintf(pHlp, "PREFETCH and PREFETCHW instruction     = %d (%d)\n",  !!(uEcxGst & RT_BIT( 8)),  !!(uEcxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "OS visible workaround                  = %d (%d)\n",  !!(uEcxGst & RT_BIT( 9)),  !!(uEcxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "Instruction based sampling             = %d (%d)\n",  !!(uEcxGst & RT_BIT(10)),  !!(uEcxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "SSE5 support                           = %d (%d)\n",  !!(uEcxGst & RT_BIT(11)),  !!(uEcxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "SKINIT, STGI, and DEV support          = %d (%d)\n",  !!(uEcxGst & RT_BIT(12)),  !!(uEcxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "Watchdog timer support.                = %d (%d)\n",  !!(uEcxGst & RT_BIT(13)),  !!(uEcxHst & RT_BIT(13)));
            pHlp->pfnPrintf(pHlp, "31:14 - Reserved                       = %#x (%#x)\n",   uEcxGst >> 14,          uEcxHst >> 14);
        }
    }

    if (iVerbosity && cExtMax >= 2)
    {
        char szString[4*4*3+1] = {0};
        uint32_t *pu32 = (uint32_t *)szString;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].eax;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].ebx;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].ecx;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].edx;
        if (cExtMax >= 3)
        {
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].eax;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].ebx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].ecx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].edx;
        }
        if (cExtMax >= 4)
        {
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].eax;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].ebx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].ecx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].edx;
        }
        pHlp->pfnPrintf(pHlp, "Full Name:                       %s\n", szString);
    }

    if (iVerbosity && cExtMax >= 5)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[5].eax;
        uint32_t uEBX = pVM->cpum.s.aGuestCpuIdExt[5].ebx;
        uint32_t uECX = pVM->cpum.s.aGuestCpuIdExt[5].ecx;
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[5].edx;
        char sz1[32];
        char sz2[32];

        pHlp->pfnPrintf(pHlp,
                        "TLB 2/4M Instr/Uni:              %s %3d entries\n"
                        "TLB 2/4M Data:                   %s %3d entries\n",
                        getCacheAss((uEAX >>  8) & 0xff, sz1), (uEAX >>  0) & 0xff,
                        getCacheAss((uEAX >> 24) & 0xff, sz2), (uEAX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "TLB 4K Instr/Uni:                %s %3d entries\n"
                        "TLB 4K Data:                     %s %3d entries\n",
                        getCacheAss((uEBX >>  8) & 0xff, sz1), (uEBX >>  0) & 0xff,
                        getCacheAss((uEBX >> 24) & 0xff, sz2), (uEBX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp, "L1 Instr Cache Line Size:        %d bytes\n"
                        "L1 Instr Cache Lines Per Tag:    %d\n"
                        "L1 Instr Cache Associativity:    %s\n"
                        "L1 Instr Cache Size:             %d KB\n",
                        (uEDX >> 0) & 0xff,
                        (uEDX >> 8) & 0xff,
                        getCacheAss((uEDX >> 16) & 0xff, sz1),
                        (uEDX >> 24) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "L1 Data Cache Line Size:         %d bytes\n"
                        "L1 Data Cache Lines Per Tag:     %d\n"
                        "L1 Data Cache Associativity:     %s\n"
                        "L1 Data Cache Size:              %d KB\n",
                        (uECX >> 0) & 0xff,
                        (uECX >> 8) & 0xff,
                        getCacheAss((uECX >> 16) & 0xff, sz1),
                        (uECX >> 24) & 0xff);
    }

    if (iVerbosity && cExtMax >= 6)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[6].eax;
        uint32_t uEBX = pVM->cpum.s.aGuestCpuIdExt[6].ebx;
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[6].edx;

        pHlp->pfnPrintf(pHlp,
                        "L2 TLB 2/4M Instr/Uni:           %s %4d entries\n"
                        "L2 TLB 2/4M Data:                %s %4d entries\n",
                        getL2CacheAss((uEAX >> 12) & 0xf),  (uEAX >>  0) & 0xfff,
                        getL2CacheAss((uEAX >> 28) & 0xf),  (uEAX >> 16) & 0xfff);
        pHlp->pfnPrintf(pHlp,
                        "L2 TLB 4K Instr/Uni:             %s %4d entries\n"
                        "L2 TLB 4K Data:                  %s %4d entries\n",
                        getL2CacheAss((uEBX >> 12) & 0xf),  (uEBX >>  0) & 0xfff,
                        getL2CacheAss((uEBX >> 28) & 0xf),  (uEBX >> 16) & 0xfff);
        pHlp->pfnPrintf(pHlp,
                        "L2 Cache Line Size:              %d bytes\n"
                        "L2 Cache Lines Per Tag:          %d\n"
                        "L2 Cache Associativity:          %s\n"
                        "L2 Cache Size:                   %d KB\n",
                        (uEDX >> 0) & 0xff,
                        (uEDX >> 8) & 0xf,
                        getL2CacheAss((uEDX >> 12) & 0xf),
                        (uEDX >> 16) & 0xffff);
    }

    if (iVerbosity && cExtMax >= 7)
    {
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[7].edx;

        pHlp->pfnPrintf(pHlp, "APM Features:                   ");
        if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " TS");
        if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " FID");
        if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " VID");
        if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " TTP");
        if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TM");
        if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " STC");
        for (unsigned iBit = 6; iBit < 32; iBit++)
            if (uEDX & RT_BIT(iBit))
                pHlp->pfnPrintf(pHlp, " %d", iBit);
        pHlp->pfnPrintf(pHlp, "\n");
    }

    if (iVerbosity && cExtMax >= 8)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[8].eax;
        uint32_t uECX = pVM->cpum.s.aGuestCpuIdExt[8].ecx;

        pHlp->pfnPrintf(pHlp,
                        "Physical Address Width:          %d bits\n"
                        "Virtual Address Width:           %d bits\n",
                        (uEAX >> 0) & 0xff,
                        (uEAX >> 8) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "Physical Core Count:             %d\n",
                        (uECX >> 0) & 0xff);
    }


    /*
     * Centaur.
     */
    unsigned cCentaurMax = pVM->cpum.s.aGuestCpuIdCentaur[0].eax & 0xffff;

    pHlp->pfnPrintf(pHlp,
                    "\n"
                    "         RAW Centaur CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdCentaur[i];
        ASMCpuId(0xc0000000 | i, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        0xc0000000 | i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cCentaurMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * Understandable output
     */
    if (iVerbosity && cCentaurMax >= 0)
    {
        Guest = pVM->cpum.s.aGuestCpuIdCentaur[0];
        pHlp->pfnPrintf(pHlp,
                        "Centaur Supports:                0xc0000000-%#010x\n",
                        Guest.eax);
    }

    if (iVerbosity && cCentaurMax >= 1)
    {
        ASMCpuId(0xc0000001, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);
        uint32_t uEdxGst = pVM->cpum.s.aGuestCpuIdExt[1].edx;
        uint32_t uEdxHst = Host.edx;

        if (iVerbosity == 1)
        {
            pHlp->pfnPrintf(pHlp, "Centaur Features EDX:           ");
            if (uEdxGst & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " AIS");
            if (uEdxGst & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " AIS-E");
            if (uEdxGst & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " RNG");
            if (uEdxGst & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " RNG-E");
            if (uEdxGst & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " LH");
            if (uEdxGst & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " FEMMS");
            if (uEdxGst & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " ACE");
            if (uEdxGst & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " ACE-E");
            /* possibly indicating MM/HE and MM/HE-E on older chips... */
            if (uEdxGst & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " ACE2");
            if (uEdxGst & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " ACE2-E");
            if (uEdxGst & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " PHE");
            if (uEdxGst & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " PHE-E");
            if (uEdxGst & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " PMM");
            if (uEdxGst & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PMM-E");
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if (uEdxGst & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "AIS - Alternate Instruction Set        = %d (%d)\n",  !!(uEdxGst & RT_BIT( 0)),  !!(uEdxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "AIS-E - AIS enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 1)),  !!(uEdxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "RNG - Random Number Generator          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 2)),  !!(uEdxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "RNG-E - RNG enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 3)),  !!(uEdxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "LH - LongHaul MSR 0000_110Ah           = %d (%d)\n",  !!(uEdxGst & RT_BIT( 4)),  !!(uEdxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "FEMMS - FEMMS                          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 5)),  !!(uEdxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "ACE - Advanced Cryptography Engine     = %d (%d)\n",  !!(uEdxGst & RT_BIT( 6)),  !!(uEdxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "ACE-E - ACE enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 7)),  !!(uEdxHst & RT_BIT( 7)));
            /* possibly indicating MM/HE and MM/HE-E on older chips... */
            pHlp->pfnPrintf(pHlp, "ACE2 - Advanced Cryptography Engine 2  = %d (%d)\n",  !!(uEdxGst & RT_BIT( 8)),  !!(uEdxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "ACE2-E - ACE enabled                   = %d (%d)\n",  !!(uEdxGst & RT_BIT( 9)),  !!(uEdxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "PHE - Hash Engine                      = %d (%d)\n",  !!(uEdxGst & RT_BIT(10)),  !!(uEdxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "PHE-E - PHE enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT(11)),  !!(uEdxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "PMM - Montgomery Multiplier            = %d (%d)\n",  !!(uEdxGst & RT_BIT(12)),  !!(uEdxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "PMM-E - PMM enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT(13)),  !!(uEdxHst & RT_BIT(13)));
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if ((uEdxGst | uEdxHst) & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, "Bit %d                                 = %d (%d)\n",  !!(uEdxGst & RT_BIT(iBit)),  !!(uEdxHst & RT_BIT(iBit)));
            pHlp->pfnPrintf(pHlp, "\n");
        }
    }
}


/**
 * Structure used when disassembling and instructions in DBGF.
 * This is used so the reader function can get the stuff it needs.
 */
typedef struct CPUMDISASSTATE
{
    /** Pointer to the CPU structure. */
    PDISCPUSTATE    pCpu;
    /** The VM handle. */
    PVM             pVM;
    /** Pointer to the first byte in the segemnt. */
    RTGCUINTPTR     GCPtrSegBase;
    /** Pointer to the byte after the end of the segment. (might have wrapped!) */
    RTGCUINTPTR     GCPtrSegEnd;
    /** The size of the segment minus 1. */
    RTGCUINTPTR     cbSegLimit;
    /** Pointer to the current page - HC Ptr. */
    void const     *pvPageHC;
    /** Pointer to the current page - GC Ptr. */
    RTGCPTR         pvPageGC;
    /** The lock information that PGMPhysReleasePageMappingLock needs. */
    PGMPAGEMAPLOCK  PageMapLock;
    /** Whether the PageMapLock is valid or not. */
    bool            fLocked;
} CPUMDISASSTATE, *PCPUMDISASSTATE;


/**
 * Instruction reader.
 *
 * @returns VBox status code.
 * @param   PtrSrc      Address to read from.
 *                      In our case this is relative to the selector pointed to by the 2nd user argument of uDisCpu.
 * @param   pu8Dst      Where to store the bytes.
 * @param   cbRead      Number of bytes to read.
 * @param   uDisCpu     Pointer to the disassembler cpu state.
 *                      In this context it's always pointer to the Core of a DBGFDISASSTATE.
 */
static DECLCALLBACK(int) cpumR3DisasInstrRead(RTUINTPTR PtrSrc, uint8_t *pu8Dst, uint32_t cbRead, void *uDisCpu)
{
    PDISCPUSTATE pCpu = (PDISCPUSTATE)uDisCpu;
    PCPUMDISASSTATE pState = (PCPUMDISASSTATE)pCpu->apvUserData[0];
    Assert(cbRead > 0);
    for (;;)
    {
        RTGCUINTPTR GCPtr = PtrSrc + pState->GCPtrSegBase;

        /* Need to update the page translation? */
        if (    !pState->pvPageHC
            ||  (GCPtr >> PAGE_SHIFT) != (pState->pvPageGC >> PAGE_SHIFT))
        {
            int rc = VINF_SUCCESS;

            /* translate the address */
            pState->pvPageGC = GCPtr & PAGE_BASE_GC_MASK;
            if (MMHyperIsInsideArea(pState->pVM, pState->pvPageGC))
            {
                pState->pvPageHC = MMHyperGC2HC(pState->pVM, pState->pvPageGC);
                if (!pState->pvPageHC)
                    rc = VERR_INVALID_POINTER;
            }
            else
            {
                /* Release mapping lock previously acquired. */
                if (pState->fLocked)
                    PGMPhysReleasePageMappingLock(pState->pVM, &pState->PageMapLock);
                rc = PGMPhysGCPtr2CCPtrReadOnly(pState->pVM, pState->pvPageGC, &pState->pvPageHC, &pState->PageMapLock);
                pState->fLocked = RT_SUCCESS_NP(rc);
            }
            if (VBOX_FAILURE(rc))
            {
                pState->pvPageHC = NULL;
                return rc;
            }
        }

        /* check the segemnt limit */
        if (PtrSrc > pState->cbSegLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        /* calc how much we can read */
        uint32_t cb = PAGE_SIZE - (GCPtr & PAGE_OFFSET_MASK);
        RTGCUINTPTR cbSeg = pState->GCPtrSegEnd - GCPtr;
        if (cb > cbSeg && !cbSeg)
            cb = cbSeg;
        if (cb > cbRead)
            cb = cbRead;

        /* read and advance */
        memcpy(pu8Dst, (char *)pState->pvPageHC + (GCPtr & PAGE_OFFSET_MASK), cb);
        cbRead -= cb;
        if (!cbRead)
            return VINF_SUCCESS;
        pu8Dst += cb;
        PtrSrc += cb;
    }
}


/**
 * Disassemble an instruction and return the information in the provided structure.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pCtx        CPU context
 * @param   GCPtrPC     Program counter (relative to CS) to disassemble from.
 * @param   pCpu        Disassembly state
 * @param   pszPrefix   String prefix for logging (debug only)
 *
 */
CPUMR3DECL(int) CPUMR3DisasmInstrCPU(PVM pVM, PCPUMCTX pCtx, RTGCPTR GCPtrPC, PDISCPUSTATE pCpu, const char *pszPrefix)
{
    CPUMDISASSTATE  State;
    int             rc;

    State.pCpu            = pCpu;
    State.pvPageGC        = 0;
    State.pvPageHC        = NULL;
    State.pVM             = pVM;
    State.fLocked         = false;

    /*
     * Get selector information.
     */
    if (    (pCtx->cr0 & X86_CR0_PE)
        &&   pCtx->eflags.Bits.u1VM == 0)
    {
        if (CPUMAreHiddenSelRegsValid(pVM))
        {
            State.GCPtrSegBase    = pCtx->csHid.u32Base;
            State.GCPtrSegEnd     = pCtx->csHid.u32Limit + 1 + (RTGCUINTPTR)pCtx->csHid.u32Base;
            State.cbSegLimit      = pCtx->csHid.u32Limit;
            pCpu->mode            = pCtx->csHid.Attr.n.u1DefBig ? CPUMODE_32BIT : CPUMODE_16BIT;
        }
        else
        {
            SELMSELINFO SelInfo;

            rc = SELMR3GetShadowSelectorInfo(pVM, pCtx->cs, &SelInfo);
            if (!VBOX_SUCCESS(rc))
            {
                AssertMsgFailed(("SELMR3GetShadowSelectorInfo failed for %04X:%VGv rc=%d\n", pCtx->cs, GCPtrPC, rc));
                return rc;
            }

            /*
             * Validate the selector.
             */
            rc = SELMSelInfoValidateCS(&SelInfo, pCtx->ss);
            if (!VBOX_SUCCESS(rc))
            {
                AssertMsgFailed(("SELMSelInfoValidateCS failed for %04X:%VGv rc=%d\n", pCtx->cs, GCPtrPC, rc));
                return rc;
            }
            State.GCPtrSegBase    = SelInfo.GCPtrBase;
            State.GCPtrSegEnd     = SelInfo.cbLimit + 1 + (RTGCUINTPTR)SelInfo.GCPtrBase;
            State.cbSegLimit      = SelInfo.cbLimit;
            pCpu->mode            = SelInfo.Raw.Gen.u1DefBig ? CPUMODE_32BIT : CPUMODE_16BIT;
        }
    }
    else
    {
        /* real or V86 mode */
        pCpu->mode            = CPUMODE_16BIT;
        State.GCPtrSegBase    = pCtx->cs * 16;
        State.GCPtrSegEnd     = 0xFFFFFFFF;
        State.cbSegLimit      = 0xFFFFFFFF;
    }

    /*
     * Disassemble the instruction.
     */
    pCpu->pfnReadBytes    = cpumR3DisasInstrRead;
    pCpu->apvUserData[0]  = &State;

    uint32_t cbInstr;
#ifndef LOG_ENABLED
    rc = DISInstr(pCpu, GCPtrPC, 0, &cbInstr, NULL);
    if (VBOX_SUCCESS(rc))
    {
#else
    char szOutput[160];
    rc = DISInstr(pCpu, GCPtrPC, 0, &cbInstr, &szOutput[0]);
    if (VBOX_SUCCESS(rc))
    {
        /* log it */
        if (pszPrefix)
            Log(("%s: %s", pszPrefix, szOutput));
        else
            Log(("%s", szOutput));
#endif
        rc = VINF_SUCCESS;
    }
    else
        Log(("CPUMR3DisasmInstrCPU: DISInstr failed for %04X:%VGv rc=%Vrc\n", pCtx->cs, GCPtrPC, rc));

    /* Release mapping lock acquired in cpumR3DisasInstrRead. */
    if (State.fLocked)
        PGMPhysReleasePageMappingLock(pVM, &State.PageMapLock);

    return rc;
}


#ifdef DEBUG
/**
 * Disassemble an instruction and dump it to the log
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pCtx        CPU context
 * @param   pc          GC instruction pointer
 * @param   prefix      String prefix for logging
 * @deprecated  Use DBGFR3DisasInstrCurrentLog().
 *
 */
CPUMR3DECL(void) CPUMR3DisasmInstr(PVM pVM, PCPUMCTX pCtx, RTGCPTR pc, char *prefix)
{
    DISCPUSTATE cpu;

    CPUMR3DisasmInstrCPU(pVM, pCtx, pc, &cpu, prefix);
}

/**
 * Disassemble an instruction and dump it to the log
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pCtx        CPU context
 * @param   pc          GC instruction pointer
 * @param   prefix      String prefix for logging
 * @param   nrInstructions
 *
 */
CPUMR3DECL(void) CPUMR3DisasmBlock(PVM pVM, PCPUMCTX pCtx, RTGCPTR pc, char *prefix, int nrInstructions)
{
    for(int i=0;i<nrInstructions;i++)
    {
        DISCPUSTATE cpu;

        CPUMR3DisasmInstrCPU(pVM, pCtx, pc, &cpu, prefix);
        pc += cpu.opsize;
    }
}

#endif

#ifdef DEBUG
/**
 * Debug helper - Saves guest context on raw mode entry (for fatal dump)
 *
 * @internal
 */
CPUMR3DECL(void) CPUMR3SaveEntryCtx(PVM pVM)
{
    pVM->cpum.s.GuestEntry = pVM->cpum.s.Guest;
}
#endif


/**
 * API for controlling a few of the CPU features found in CR4.
 *
 * Currently only X86_CR4_TSD is accepted as input.
 *
 * @returns VBox status code.
 *
 * @param   pVM     The VM handle.
 * @param   fOr     The CR4 OR mask.
 * @param   fAnd    The CR4 AND mask.
 */
CPUMR3DECL(int) CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd)
{
    AssertMsgReturn(!(fOr & ~(X86_CR4_TSD)), ("%#x\n", fOr), VERR_INVALID_PARAMETER);
    AssertMsgReturn((fAnd & ~(X86_CR4_TSD)) == ~(X86_CR4_TSD), ("%#x\n", fAnd), VERR_INVALID_PARAMETER);

    pVM->cpum.s.CR4.OrMask &= fAnd;
    pVM->cpum.s.CR4.OrMask |= fOr;

    return VINF_SUCCESS;
}

