/* $Id$ */
/** @file
 * VMM - The Virtual Machine Monitor, World Switcher(s).
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
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include <VBox/sup.h>
#include "VMMInternal.h"
#include "VMMSwitcher/VMMSwitcher.h"
#include <VBox/vm.h>
#include <VBox/dis.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/ctype.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Array of switcher defininitions.
 * The type and index shall match!
 */
static PVMMSWITCHERDEF s_apSwitchers[VMMSWITCHER_MAX] =
{
    NULL, /* invalid entry */
#ifndef RT_ARCH_AMD64
    &vmmR3Switcher32BitTo32Bit_Def,
    &vmmR3Switcher32BitToPAE_Def,
    &vmmR3Switcher32BitToAMD64_Def,
    &vmmR3SwitcherPAETo32Bit_Def,
    &vmmR3SwitcherPAEToPAE_Def,
    &vmmR3SwitcherPAEToAMD64_Def,
    NULL,   //&vmmR3SwitcherPAETo32Bit_Def,
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    &vmmR3SwitcherAMD64ToPAE_Def,
# else
    NULL,   //&vmmR3SwitcherAMD64ToPAE_Def,
# endif
    NULL    //&vmmR3SwitcherAMD64ToAMD64_Def,
#else  /* RT_ARCH_AMD64 */
    NULL,   //&vmmR3Switcher32BitTo32Bit_Def,
    NULL,   //&vmmR3Switcher32BitToPAE_Def,
    NULL,   //&vmmR3Switcher32BitToAMD64_Def,
    NULL,   //&vmmR3SwitcherPAETo32Bit_Def,
    NULL,   //&vmmR3SwitcherPAEToPAE_Def,
    NULL,   //&vmmR3SwitcherPAEToAMD64_Def,
    &vmmR3SwitcherAMD64To32Bit_Def,
    &vmmR3SwitcherAMD64ToPAE_Def,
    NULL    //&vmmR3SwitcherAMD64ToAMD64_Def,
#endif /* RT_ARCH_AMD64 */
};


/**
 * VMMR3Init worker that initiates the switcher code (aka core code).
 *
 * This is core per VM code which might need fixups and/or for ease of use are
 * put on linear contiguous backing.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the shared VM structure.
 */
int vmmR3SwitcherInit(PVM pVM)
{
    /*
     * Calc the size.
     */
    unsigned cbCoreCode = 0;
    for (unsigned iSwitcher = 0; iSwitcher < RT_ELEMENTS(s_apSwitchers); iSwitcher++)
    {
        pVM->vmm.s.aoffSwitchers[iSwitcher] = cbCoreCode;
        PVMMSWITCHERDEF pSwitcher = s_apSwitchers[iSwitcher];
        if (pSwitcher)
        {
            AssertRelease((unsigned)pSwitcher->enmType == iSwitcher);
            cbCoreCode += RT_ALIGN_32(pSwitcher->cbCode + 1, 32);
        }
    }

    /*
     * Allocate continguous pages for switchers and deal with
     * conflicts in the intermediate mapping of the code.
     */
    pVM->vmm.s.cbCoreCode = RT_ALIGN_32(cbCoreCode, PAGE_SIZE);
    pVM->vmm.s.pvCoreCodeR3 = SUPContAlloc2(pVM->vmm.s.cbCoreCode >> PAGE_SHIFT, &pVM->vmm.s.pvCoreCodeR0, &pVM->vmm.s.HCPhysCoreCode);
    int rc = VERR_NO_MEMORY;
    if (pVM->vmm.s.pvCoreCodeR3)
    {
        rc = PGMR3MapIntermediate(pVM, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.HCPhysCoreCode, cbCoreCode);
        if (rc == VERR_PGM_INTERMEDIATE_PAGING_CONFLICT)
        {
            /* try more allocations - Solaris, Linux.  */
            const unsigned cTries = 8234;
            struct VMMInitBadTry
            {
                RTR0PTR  pvR0;
                void    *pvR3;
                RTHCPHYS HCPhys;
                RTUINT   cb;
            } *paBadTries = (struct VMMInitBadTry *)RTMemTmpAlloc(sizeof(*paBadTries) * cTries);
            AssertReturn(paBadTries, VERR_NO_TMP_MEMORY);
            unsigned i = 0;
            do
            {
                paBadTries[i].pvR3 = pVM->vmm.s.pvCoreCodeR3;
                paBadTries[i].pvR0 = pVM->vmm.s.pvCoreCodeR0;
                paBadTries[i].HCPhys = pVM->vmm.s.HCPhysCoreCode;
                i++;
                pVM->vmm.s.pvCoreCodeR0 = NIL_RTR0PTR;
                pVM->vmm.s.HCPhysCoreCode = NIL_RTHCPHYS;
                pVM->vmm.s.pvCoreCodeR3 = SUPContAlloc2(pVM->vmm.s.cbCoreCode >> PAGE_SHIFT, &pVM->vmm.s.pvCoreCodeR0, &pVM->vmm.s.HCPhysCoreCode);
                if (!pVM->vmm.s.pvCoreCodeR3)
                    break;
                rc = PGMR3MapIntermediate(pVM, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.HCPhysCoreCode, cbCoreCode);
            } while (   rc == VERR_PGM_INTERMEDIATE_PAGING_CONFLICT
                     && i < cTries - 1);

            /* cleanup */
            if (RT_FAILURE(rc))
            {
                paBadTries[i].pvR3   = pVM->vmm.s.pvCoreCodeR3;
                paBadTries[i].pvR0   = pVM->vmm.s.pvCoreCodeR0;
                paBadTries[i].HCPhys = pVM->vmm.s.HCPhysCoreCode;
                paBadTries[i].cb     = pVM->vmm.s.cbCoreCode;
                i++;
                LogRel(("Failed to allocated and map core code: rc=%Rrc\n", rc));
            }
            while (i-- > 0)
            {
                LogRel(("Core code alloc attempt #%d: pvR3=%p pvR0=%p HCPhys=%RHp\n",
                        i, paBadTries[i].pvR3, paBadTries[i].pvR0, paBadTries[i].HCPhys));
                SUPContFree(paBadTries[i].pvR3, paBadTries[i].cb >> PAGE_SHIFT);
            }
            RTMemTmpFree(paBadTries);
        }
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * copy the code.
         */
        for (unsigned iSwitcher = 0; iSwitcher < RT_ELEMENTS(s_apSwitchers); iSwitcher++)
        {
            PVMMSWITCHERDEF pSwitcher = s_apSwitchers[iSwitcher];
            if (pSwitcher)
                memcpy((uint8_t *)pVM->vmm.s.pvCoreCodeR3 + pVM->vmm.s.aoffSwitchers[iSwitcher],
                       pSwitcher->pvCode, pSwitcher->cbCode);
        }

        /*
         * Map the code into the GC address space.
         */
        RTGCPTR GCPtr;
        rc = MMR3HyperMapHCPhys(pVM, pVM->vmm.s.pvCoreCodeR3, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.HCPhysCoreCode,
                                cbCoreCode, "Core Code", &GCPtr);
        if (RT_SUCCESS(rc))
        {
            pVM->vmm.s.pvCoreCodeRC = GCPtr;
            MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);
            LogRel(("CoreCode: R3=%RHv R0=%RHv RC=%RRv Phys=%RHp cb=%#x\n",
                    pVM->vmm.s.pvCoreCodeR3, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.pvCoreCodeRC, pVM->vmm.s.HCPhysCoreCode, pVM->vmm.s.cbCoreCode));

            /*
             * Finally, PGM probably has selected a switcher already but we need
             * to get the routine addresses, so we'll reselect it.
             * This may legally fail so, we're ignoring the rc.
             */
            VMMR3SelectSwitcher(pVM, pVM->vmm.s.enmSwitcher);
            return rc;
        }

        /* shit */
        AssertMsgFailed(("PGMR3Map(,%RRv, %RHp, %#x, 0) failed with rc=%Rrc\n", pVM->vmm.s.pvCoreCodeRC, pVM->vmm.s.HCPhysCoreCode, cbCoreCode, rc));
        SUPContFree(pVM->vmm.s.pvCoreCodeR3, pVM->vmm.s.cbCoreCode >> PAGE_SHIFT);
    }
    else
        VMSetError(pVM, rc, RT_SRC_POS,
                   N_("Failed to allocate %d bytes of contiguous memory for the world switcher code"),
                   cbCoreCode);

    pVM->vmm.s.pvCoreCodeR3 = NULL;
    pVM->vmm.s.pvCoreCodeR0 = NIL_RTR0PTR;
    pVM->vmm.s.pvCoreCodeRC = 0;
    return rc;
}

/**
 * Relocate the switchers, called by VMMR#Relocate.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   offDelta    The relocation delta.
 */
void vmmR3SwitcherRelocate(PVM pVM, RTGCINTPTR offDelta)
{
    /*
     * Relocate all the switchers.
     */
    for (unsigned iSwitcher = 0; iSwitcher < RT_ELEMENTS(s_apSwitchers); iSwitcher++)
    {
        PVMMSWITCHERDEF pSwitcher = s_apSwitchers[iSwitcher];
        if (pSwitcher && pSwitcher->pfnRelocate)
        {
            unsigned off = pVM->vmm.s.aoffSwitchers[iSwitcher];
            pSwitcher->pfnRelocate(pVM,
                                   pSwitcher,
                                   pVM->vmm.s.pvCoreCodeR0 + off,
                                   (uint8_t *)pVM->vmm.s.pvCoreCodeR3 + off,
                                   pVM->vmm.s.pvCoreCodeRC + off,
                                   pVM->vmm.s.HCPhysCoreCode + off);
        }
    }

    /*
     * Recalc the RC address for the current switcher.
     */
    PVMMSWITCHERDEF pSwitcher   = s_apSwitchers[pVM->vmm.s.enmSwitcher];
    RTRCPTR         RCPtr       = pVM->vmm.s.pvCoreCodeRC + pVM->vmm.s.aoffSwitchers[pVM->vmm.s.enmSwitcher];
    pVM->vmm.s.pfnGuestToHostRC         = RCPtr + pSwitcher->offGCGuestToHost;
    pVM->vmm.s.pfnCallTrampolineRC      = RCPtr + pSwitcher->offGCCallTrampoline;
    pVM->pfnVMMGCGuestToHostAsm         = RCPtr + pSwitcher->offGCGuestToHostAsm;
    pVM->pfnVMMGCGuestToHostAsmHyperCtx = RCPtr + pSwitcher->offGCGuestToHostAsmHyperCtx;
    pVM->pfnVMMGCGuestToHostAsmGuestCtx = RCPtr + pSwitcher->offGCGuestToHostAsmGuestCtx;

//    AssertFailed();
}


/**
 * Generic switcher code relocator.
 *
 * @param   pVM         The VM handle.
 * @param   pSwitcher   The switcher definition.
 * @param   pu8CodeR3   Pointer to the core code block for the switcher, ring-3 mapping.
 * @param   R0PtrCode   Pointer to the core code block for the switcher, ring-0 mapping.
 * @param   GCPtrCode   The guest context address corresponding to pu8Code.
 * @param   u32IDCode   The identity mapped (ID) address corresponding to pu8Code.
 * @param   SelCS       The hypervisor CS selector.
 * @param   SelDS       The hypervisor DS selector.
 * @param   SelTSS      The hypervisor TSS selector.
 * @param   GCPtrGDT    The GC address of the hypervisor GDT.
 * @param   SelCS64     The 64-bit mode hypervisor CS selector.
 */
static void vmmR3SwitcherGenericRelocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode,
                                         RTSEL SelCS, RTSEL SelDS, RTSEL SelTSS, RTGCPTR GCPtrGDT, RTSEL SelCS64)
{
    union
    {
        const uint8_t *pu8;
        const uint16_t *pu16;
        const uint32_t *pu32;
        const uint64_t *pu64;
        const void     *pv;
        uintptr_t       u;
    } u;
    u.pv = pSwitcher->pvFixups;

    /*
     * Process fixups.
     */
    uint8_t u8;
    while ((u8 = *u.pu8++) != FIX_THE_END)
    {
        /*
         * Get the source (where to write the fixup).
         */
        uint32_t offSrc = *u.pu32++;
        Assert(offSrc < pSwitcher->cbCode);
        union
        {
            uint8_t    *pu8;
            uint16_t   *pu16;
            uint32_t   *pu32;
            uint64_t   *pu64;
            uintptr_t   u;
        } uSrc;
        uSrc.pu8 = pu8CodeR3 + offSrc;

        /* The fixup target and method depends on the type. */
        switch (u8)
        {
            /*
             * 32-bit relative, source in HC and target in GC.
             */
            case FIX_HC_2_GC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offSrc - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                *uSrc.pu32 = (uint32_t)((GCPtrCode + offTrg) - (uSrc.u + 4));
                break;
            }

            /*
             * 32-bit relative, source in HC and target in ID.
             */
            case FIX_HC_2_ID_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offSrc - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32 = (uint32_t)((u32IDCode + offTrg) - (R0PtrCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in GC and target in HC.
             */
            case FIX_GC_2_HC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu32 = (uint32_t)((R0PtrCode + offTrg) - (GCPtrCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in GC and target in ID.
             */
            case FIX_GC_2_ID_NEAR_REL:
            {
                AssertMsg(offSrc - pSwitcher->offGCCode < pSwitcher->cbGCCode, ("%x - %x < %x\n", offSrc, pSwitcher->offGCCode, pSwitcher->cbGCCode));
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32 = (uint32_t)((u32IDCode + offTrg) - (GCPtrCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in ID and target in HC.
             */
            case FIX_ID_2_HC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offSrc - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu32 = (uint32_t)((R0PtrCode + offTrg) - (u32IDCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in ID and target in HC.
             */
            case FIX_ID_2_GC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offSrc - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                *uSrc.pu32 = (uint32_t)((GCPtrCode + offTrg) - (u32IDCode + offSrc + 4));
                break;
            }

            /*
             * 16:32 far jump, target in GC.
             */
            case FIX_GC_FAR32:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                *uSrc.pu32++ = (uint32_t)(GCPtrCode + offTrg);
                *uSrc.pu16++ = SelCS;
                break;
            }

            /*
             * Make 32-bit GC pointer given CPUM offset.
             */
            case FIX_GC_CPUM_OFF:
            {
                uint32_t offCPUM = *u.pu32++;
                Assert(offCPUM < sizeof(pVM->cpum));
                *uSrc.pu32 = (uint32_t)(VM_RC_ADDR(pVM, &pVM->cpum) + offCPUM);
                break;
            }

            /*
             * Make 32-bit GC pointer given CPUMCPU offset.
             */
            case FIX_GC_CPUMCPU_OFF:
            {
                uint32_t offCPUM = *u.pu32++;
                Assert(offCPUM < sizeof(pVM->aCpus[0].cpum));
                *uSrc.pu32 = (uint32_t)(VM_RC_ADDR(pVM, &pVM->aCpus[0].cpum) + offCPUM);
                break;
            }

            /*
             * Make 32-bit GC pointer given VM offset.
             */
            case FIX_GC_VM_OFF:
            {
                uint32_t offVM = *u.pu32++;
                Assert(offVM < sizeof(VM));
                *uSrc.pu32 = (uint32_t)(VM_RC_ADDR(pVM, pVM) + offVM);
                break;
            }

            /*
             * Make 32-bit HC pointer given CPUM offset.
             */
            case FIX_HC_CPUM_OFF:
            {
                uint32_t offCPUM = *u.pu32++;
                Assert(offCPUM < sizeof(pVM->cpum));
                *uSrc.pu32 = (uint32_t)pVM->pVMR0 + RT_OFFSETOF(VM, cpum) + offCPUM;
                break;
            }

            /*
             * Make 32-bit R0 pointer given VM offset.
             */
            case FIX_HC_VM_OFF:
            {
                uint32_t offVM = *u.pu32++;
                Assert(offVM < sizeof(VM));
                *uSrc.pu32 = (uint32_t)pVM->pVMR0 + offVM;
                break;
            }

            /*
             * Store the 32-Bit CR3 (32-bit) for the intermediate memory context.
             */
            case FIX_INTER_32BIT_CR3:
            {

                *uSrc.pu32 = PGMGetInter32BitCR3(pVM);
                break;
            }

            /*
             * Store the PAE CR3 (32-bit) for the intermediate memory context.
             */
            case FIX_INTER_PAE_CR3:
            {

                *uSrc.pu32 = PGMGetInterPaeCR3(pVM);
                break;
            }

            /*
             * Store the AMD64 CR3 (32-bit) for the intermediate memory context.
             */
            case FIX_INTER_AMD64_CR3:
            {

                *uSrc.pu32 = PGMGetInterAmd64CR3(pVM);
                break;
            }

            /*
             * Store Hypervisor CS (16-bit).
             */
            case FIX_HYPER_CS:
            {
                *uSrc.pu16 = SelCS;
                break;
            }

            /*
             * Store Hypervisor DS (16-bit).
             */
            case FIX_HYPER_DS:
            {
                *uSrc.pu16 = SelDS;
                break;
            }

            /*
             * Store Hypervisor TSS (16-bit).
             */
            case FIX_HYPER_TSS:
            {
                *uSrc.pu16 = SelTSS;
                break;
            }

            /*
             * Store the 32-bit GC address of the 2nd dword of the TSS descriptor (in the GDT).
             */
            case FIX_GC_TSS_GDTE_DW2:
            {
                RTGCPTR GCPtr = GCPtrGDT + (SelTSS & ~7) + 4;
                *uSrc.pu32 = (uint32_t)GCPtr;
                break;
            }


            ///@todo case FIX_CR4_MASK:
            ///@todo case FIX_CR4_OSFSXR:

            /*
             * Insert relative jump to specified target it FXSAVE/FXRSTOR isn't supported by the cpu.
             */
            case FIX_NO_FXSAVE_JMP:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg < pSwitcher->cbCode);
                if (!CPUMSupportsFXSR(pVM))
                {
                    *uSrc.pu8++ = 0xe9; /* jmp rel32 */
                    *uSrc.pu32++ = offTrg - (offSrc + 5);
                }
                else
                {
                    *uSrc.pu8++ = *((uint8_t *)pSwitcher->pvCode + offSrc);
                    *uSrc.pu32++ = *(uint32_t *)((uint8_t *)pSwitcher->pvCode + offSrc + 1);
                }
                break;
            }

            /*
             * Insert relative jump to specified target it SYSENTER isn't used by the host.
             */
            case FIX_NO_SYSENTER_JMP:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg < pSwitcher->cbCode);
                if (!CPUMIsHostUsingSysEnter(pVM))
                {
                    *uSrc.pu8++ = 0xe9; /* jmp rel32 */
                    *uSrc.pu32++ = offTrg - (offSrc + 5);
                }
                else
                {
                    *uSrc.pu8++ = *((uint8_t *)pSwitcher->pvCode + offSrc);
                    *uSrc.pu32++ = *(uint32_t *)((uint8_t *)pSwitcher->pvCode + offSrc + 1);
                }
                break;
            }

            /*
             * Insert relative jump to specified target it SYSENTER isn't used by the host.
             */
            case FIX_NO_SYSCALL_JMP:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg < pSwitcher->cbCode);
                if (!CPUMIsHostUsingSysEnter(pVM))
                {
                    *uSrc.pu8++ = 0xe9; /* jmp rel32 */
                    *uSrc.pu32++ = offTrg - (offSrc + 5);
                }
                else
                {
                    *uSrc.pu8++ = *((uint8_t *)pSwitcher->pvCode + offSrc);
                    *uSrc.pu32++ = *(uint32_t *)((uint8_t *)pSwitcher->pvCode + offSrc + 1);
                }
                break;
            }

            /*
             * 32-bit HC pointer fixup to (HC) target within the code (32-bit offset).
             */
            case FIX_HC_32BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu32 = R0PtrCode + offTrg;
                break;
            }

#if defined(RT_ARCH_AMD64) || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
            /*
             * 64-bit HC Code Selector (no argument).
             */
            case FIX_HC_64BIT_CS:
            {
                Assert(offSrc < pSwitcher->cbCode);
# if defined(RT_OS_DARWIN) && defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
                *uSrc.pu16 = 0x80; /* KERNEL64_CS from i386/seg.h */
# else
                AssertFatalMsgFailed(("FIX_HC_64BIT_CS not implemented for this host\n"));
# endif
                break;
            }

            /*
             * 64-bit HC pointer to the CPUM instance data (no argument).
             */
            case FIX_HC_64BIT_CPUM:
            {
                Assert(offSrc < pSwitcher->cbCode);
                *uSrc.pu64 = pVM->pVMR0 + RT_OFFSETOF(VM, cpum);
                break;
            }
#endif
            /*
             * 64-bit HC pointer fixup to (HC) target within the code (32-bit offset).
             */
            case FIX_HC_64BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu64 = R0PtrCode + offTrg;
                break;
            }

#ifdef RT_ARCH_X86
            case FIX_GC_64_BIT_CPUM_OFF:
            {
                uint32_t offCPUM = *u.pu32++;
                Assert(offCPUM < sizeof(pVM->cpum));
                *uSrc.pu64 = (uint32_t)(VM_RC_ADDR(pVM, &pVM->cpum) + offCPUM);
                break;
            }
#endif

            /*
             * 32-bit ID pointer to (ID) target within the code (32-bit offset).
             */
            case FIX_ID_32BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32 = u32IDCode + offTrg;
                break;
            }

            /*
             * 64-bit ID pointer to (ID) target within the code (32-bit offset).
             */
            case FIX_ID_64BIT:
            case FIX_HC_64BIT_NOCHECK:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(u8 == FIX_HC_64BIT_NOCHECK || offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu64 = u32IDCode + offTrg;
                break;
            }

            /*
             * Far 16:32 ID pointer to 64-bit mode (ID) target within the code (32-bit offset).
             */
            case FIX_ID_FAR32_TO_64BIT_MODE:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32++ = u32IDCode + offTrg;
                *uSrc.pu16 = SelCS64;
                AssertRelease(SelCS64);
                break;
            }

#ifdef VBOX_WITH_NMI
            /*
             * 32-bit address to the APIC base.
             */
            case FIX_GC_APIC_BASE_32BIT:
            {
                *uSrc.pu32 = pVM->vmm.s.GCPtrApicBase;
                break;
            }
#endif

            default:
                AssertReleaseMsgFailed(("Unknown fixup %d in switcher %s\n", u8, pSwitcher->pszDesc));
                break;
        }
    }

#ifdef LOG_ENABLED
    /*
     * If Log2 is enabled disassemble the switcher code.
     *
     * The switcher code have 1-2 HC parts, 1 GC part and 0-2 ID parts.
     */
    if (LogIs2Enabled())
    {
        RTLogPrintf("*** Disassembly of switcher %d '%s' %#x bytes ***\n"
                    "   R0PtrCode   = %p\n"
                    "   pu8CodeR3   = %p\n"
                    "   GCPtrCode   = %RGv\n"
                    "   u32IDCode   = %08x\n"
                    "   pVMRC       = %RRv\n"
                    "   pCPUMRC     = %RRv\n"
                    "   pVMR3       = %p\n"
                    "   pCPUMR3     = %p\n"
                    "   GCPtrGDT    = %RGv\n"
                    "   InterCR3s   = %08RHp, %08RHp, %08RHp (32-Bit, PAE, AMD64)\n"
                    "   HyperCR3s   = %08RHp (32-Bit, PAE & AMD64)\n"
                    "   SelCS       = %04x\n"
                    "   SelDS       = %04x\n"
                    "   SelCS64     = %04x\n"
                    "   SelTSS      = %04x\n",
                    pSwitcher->enmType, pSwitcher->pszDesc, pSwitcher->cbCode,
                    R0PtrCode,
                    pu8CodeR3,
                    GCPtrCode,
                    u32IDCode,
                    VM_RC_ADDR(pVM, pVM),
                    VM_RC_ADDR(pVM, &pVM->cpum),
                    pVM,
                    &pVM->cpum,
                    GCPtrGDT,
                    PGMGetInter32BitCR3(pVM), PGMGetInterPaeCR3(pVM), PGMGetInterAmd64CR3(pVM),
                    PGMGetHyperCR3(VMMGetCpu(pVM)),
                    SelCS, SelDS, SelCS64, SelTSS);

        uint32_t offCode = 0;
        while (offCode < pSwitcher->cbCode)
        {
            /*
             * Figure out where this is.
             */
            const char *pszDesc = NULL;
            RTUINTPTR   uBase;
            uint32_t    cbCode;
            if (offCode - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0)
            {
                pszDesc = "HCCode0";
                uBase   = R0PtrCode;
                offCode = pSwitcher->offHCCode0;
                cbCode  = pSwitcher->cbHCCode0;
            }
            else if (offCode - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1)
            {
                pszDesc = "HCCode1";
                uBase   = R0PtrCode;
                offCode = pSwitcher->offHCCode1;
                cbCode  = pSwitcher->cbHCCode1;
            }
            else if (offCode - pSwitcher->offGCCode < pSwitcher->cbGCCode)
            {
                pszDesc = "GCCode";
                uBase   = GCPtrCode;
                offCode = pSwitcher->offGCCode;
                cbCode  = pSwitcher->cbGCCode;
            }
            else if (offCode - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0)
            {
                pszDesc = "IDCode0";
                uBase   = u32IDCode;
                offCode = pSwitcher->offIDCode0;
                cbCode  = pSwitcher->cbIDCode0;
            }
            else if (offCode - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1)
            {
                pszDesc = "IDCode1";
                uBase   = u32IDCode;
                offCode = pSwitcher->offIDCode1;
                cbCode  = pSwitcher->cbIDCode1;
            }
            else
            {
                RTLogPrintf("  %04x: %02x '%c' (nowhere)\n",
                            offCode, pu8CodeR3[offCode], isprint(pu8CodeR3[offCode]) ? pu8CodeR3[offCode] : ' ');
                offCode++;
                continue;
            }

            /*
             * Disassemble it.
             */
            RTLogPrintf("  %s: offCode=%#x cbCode=%#x\n", pszDesc, offCode, cbCode);
            DISCPUSTATE Cpu;

            memset(&Cpu, 0, sizeof(Cpu));
            Cpu.mode = CPUMODE_32BIT;
            while (cbCode > 0)
            {
                /* try label it */
                if (pSwitcher->offR0HostToGuest == offCode)
                    RTLogPrintf(" *R0HostToGuest:\n");
                if (pSwitcher->offGCGuestToHost == offCode)
                    RTLogPrintf(" *GCGuestToHost:\n");
                if (pSwitcher->offGCCallTrampoline == offCode)
                    RTLogPrintf(" *GCCallTrampoline:\n");
                if (pSwitcher->offGCGuestToHostAsm == offCode)
                    RTLogPrintf(" *GCGuestToHostAsm:\n");
                if (pSwitcher->offGCGuestToHostAsmHyperCtx == offCode)
                    RTLogPrintf(" *GCGuestToHostAsmHyperCtx:\n");
                if (pSwitcher->offGCGuestToHostAsmGuestCtx == offCode)
                    RTLogPrintf(" *GCGuestToHostAsmGuestCtx:\n");

                /* disas */
                uint32_t cbInstr = 0;
                char szDisas[256];
                if (RT_SUCCESS(DISInstr(&Cpu, (RTUINTPTR)pu8CodeR3 + offCode, uBase - (RTUINTPTR)pu8CodeR3, &cbInstr, szDisas)))
                    RTLogPrintf("  %04x: %s", offCode, szDisas); //for whatever reason szDisas includes '\n'.
                else
                {
                    RTLogPrintf("  %04x: %02x '%c'\n",
                                offCode, pu8CodeR3[offCode], isprint(pu8CodeR3[offCode]) ? pu8CodeR3[offCode] : ' ');
                    cbInstr = 1;
                }
                offCode += cbInstr;
                cbCode -= RT_MIN(cbInstr, cbCode);
            }
        }
    }
#endif
}


/**
 * Relocator for the 32-Bit to 32-Bit world switcher.
 */
DECLCALLBACK(void) vmmR3Switcher32BitTo32Bit_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the 32-Bit to PAE world switcher.
 */
DECLCALLBACK(void) vmmR3Switcher32BitToPAE_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the 32-Bit to AMD64 world switcher.
 */
DECLCALLBACK(void) vmmR3Switcher32BitToAMD64_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), SELMGetHyperCS64(pVM));
}


/**
 * Relocator for the PAE to 32-Bit world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherPAETo32Bit_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the PAE to PAE world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherPAEToPAE_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}

/**
 * Relocator for the PAE to AMD64 world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherPAEToAMD64_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), SELMGetHyperCS64(pVM));
}


/**
 * Relocator for the AMD64 to 32-bit world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherAMD64To32Bit_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), SELMGetHyperCS64(pVM));
}


/**
 * Relocator for the AMD64 to PAE world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherAMD64ToPAE_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), SELMGetHyperCS64(pVM));
}


/**
 * Selects the switcher to be used for switching to GC.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmSwitcher     The new switcher.
 * @remark  This function may be called before the VMM is initialized.
 */
VMMR3DECL(int) VMMR3SelectSwitcher(PVM pVM, VMMSWITCHER enmSwitcher)
{
    /*
     * Validate input.
     */
    if (    enmSwitcher < VMMSWITCHER_INVALID
        ||  enmSwitcher >= VMMSWITCHER_MAX)
    {
        AssertMsgFailed(("Invalid input enmSwitcher=%d\n", enmSwitcher));
        return VERR_INVALID_PARAMETER;
    }

    /* Do nothing if the switcher is disabled. */
    if (pVM->vmm.s.fSwitcherDisabled)
        return VINF_SUCCESS;

    /*
     * Select the new switcher.
     */
    PVMMSWITCHERDEF pSwitcher = s_apSwitchers[enmSwitcher];
    if (pSwitcher)
    {
        Log(("VMMR3SelectSwitcher: enmSwitcher %d -> %d %s\n", pVM->vmm.s.enmSwitcher, enmSwitcher, pSwitcher->pszDesc));
        pVM->vmm.s.enmSwitcher = enmSwitcher;

        RTR0PTR     pbCodeR0 = (RTR0PTR)pVM->vmm.s.pvCoreCodeR0 + pVM->vmm.s.aoffSwitchers[enmSwitcher]; /** @todo fix the pvCoreCodeR0 type */
        pVM->vmm.s.pfnHostToGuestR0 = pbCodeR0 + pSwitcher->offR0HostToGuest;

        RTGCPTR     GCPtr = pVM->vmm.s.pvCoreCodeRC + pVM->vmm.s.aoffSwitchers[enmSwitcher];
        pVM->vmm.s.pfnGuestToHostRC         = GCPtr + pSwitcher->offGCGuestToHost;
        pVM->vmm.s.pfnCallTrampolineRC      = GCPtr + pSwitcher->offGCCallTrampoline;
        pVM->pfnVMMGCGuestToHostAsm         = GCPtr + pSwitcher->offGCGuestToHostAsm;
        pVM->pfnVMMGCGuestToHostAsmHyperCtx = GCPtr + pSwitcher->offGCGuestToHostAsmHyperCtx;
        pVM->pfnVMMGCGuestToHostAsmGuestCtx = GCPtr + pSwitcher->offGCGuestToHostAsmGuestCtx;
        return VINF_SUCCESS;
    }

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Disable the switcher logic permanently.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 */
VMMR3DECL(int) VMMR3DisableSwitcher(PVM pVM)
{
/** @todo r=bird: I would suggest that we create a dummy switcher which just does something like:
 * @code
 *       mov eax, VERR_INTERNAL_ERROR
 *       ret
 * @endcode
 * And then check for fSwitcherDisabled in VMMR3SelectSwitcher() in order to prevent it from being removed.
 */
    pVM->vmm.s.fSwitcherDisabled = true;
    return VINF_SUCCESS;
}

/**
 * Gets the switcher to be used for switching to GC.
 *
 * @returns host to guest ring 0 switcher entrypoint
 * @param   pVM             VM handle.
 * @param   enmSwitcher     The new switcher.
 */
VMMR3DECL(RTR0PTR) VMMR3GetHostToGuestSwitcher(PVM pVM, VMMSWITCHER enmSwitcher)
{
    /*
     * Validate input.
     */
    if (    enmSwitcher < VMMSWITCHER_INVALID
        ||  enmSwitcher >= VMMSWITCHER_MAX)
    {
        AssertMsgFailed(("Invalid input enmSwitcher=%d\n", enmSwitcher));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Select the new switcher.
     */
    PVMMSWITCHERDEF pSwitcher = s_apSwitchers[enmSwitcher];
    if (pSwitcher)
    {
        RTR0PTR     pbCodeR0 = (RTR0PTR)pVM->vmm.s.pvCoreCodeR0 + pVM->vmm.s.aoffSwitchers[enmSwitcher]; /** @todo fix the pvCoreCodeR0 type */
        return pbCodeR0 + pSwitcher->offR0HostToGuest;
    }
    return (RTR0PTR)0;
}
