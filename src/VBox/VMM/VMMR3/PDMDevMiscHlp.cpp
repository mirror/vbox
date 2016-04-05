/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, Misc. Device Helpers.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_DEVICE
#include "PDMInternal.h"
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/hm.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/vm.h>
#include <VBox/vmm/vmm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>


#include "PDMInline.h"
#include "dtrace/VBoxVMM.h"



/** @name Ring-3 PIC Helpers
 * @{
 */

/** @interface_method_impl{PDMPICHLPR3,pfnSetInterruptFF} */
static DECLCALLBACK(void) pdmR3PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[0];  /* for PIC we always deliver to CPU 0, MP use APIC */

    if (pVM->pdm.s.Apic.pfnLocalInterruptR3)
    {
        LogFlow(("pdmR3PicHlp_SetInterruptFF: caller='%s'/%d: Setting local interrupt on LAPIC\n",
                 pDevIns->pReg->szName, pDevIns->iInstance));

        /* Raise the LAPIC's LINT0 line instead of signaling the CPU directly. */
        /** @todo 'rcRZ' propagation to pfnLocalInterrupt from caller. */
        pVM->pdm.s.Apic.pfnLocalInterruptR3(pVM->pdm.s.Apic.pDevInsR3, pVCpu, 0 /* u8Pin */, 1 /* u8Level */,
                                            VINF_SUCCESS /* rcRZ */);
        return;
    }

    LogFlow(("pdmR3PicHlp_SetInterruptFF: caller='%s'/%d: VMCPU_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns->pReg->szName, pDevIns->iInstance, VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);
#ifdef VBOX_WITH_REM
    REMR3NotifyInterruptSet(pVM, pVCpu);
#endif
    VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM | VMNOTIFYFF_FLAGS_POKE);
}


/** @interface_method_impl{PDMPICHLPR3,pfnClearInterruptFF} */
static DECLCALLBACK(void) pdmR3PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[0];  /* for PIC we always deliver to CPU 0, MP use APIC */

    if (pVM->pdm.s.Apic.pfnLocalInterruptR3)
    {
        /* Raise the LAPIC's LINT0 line instead of signaling the CPU directly. */
        LogFlow(("pdmR3PicHlp_ClearInterruptFF: caller='%s'/%d: Clearing local interrupt on LAPIC\n",
                 pDevIns->pReg->szName, pDevIns->iInstance));

        /* Lower the LAPIC's LINT0 line instead of signaling the CPU directly. */
        /** @todo 'rcRZ' propagation to pfnLocalInterrupt from caller. */
        pVM->pdm.s.Apic.pfnLocalInterruptR3(pVM->pdm.s.Apic.pDevInsR3, pVCpu, 0 /* u8Pin */, 0 /* u8Level */,
                                            VINF_SUCCESS /* rcRZ */);
        return;
    }

    LogFlow(("pdmR3PicHlp_ClearInterruptFF: caller='%s'/%d: VMCPU_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns->pReg->szName, pDevIns->iInstance, VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
#ifdef VBOX_WITH_REM
    REMR3NotifyInterruptClear(pVM, pVCpu);
#endif
}


/** @interface_method_impl{PDMPICHLPR3,pfnLock} */
static DECLCALLBACK(int) pdmR3PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @interface_method_impl{PDMPICHLPR3,pfnUnlock} */
static DECLCALLBACK(void) pdmR3PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @interface_method_impl{PDMPICHLPR3,pfnGetRCHelpers} */
static DECLCALLBACK(PCPDMPICHLPRC) pdmR3PicHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    RTRCPTR pRCHelpers = NIL_RTRCPTR;
    if (!HMIsEnabled(pVM))
    {
        int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCPicHlp", &pRCHelpers);
        AssertReleaseRC(rc);
        AssertRelease(pRCHelpers);
    }

    LogFlow(("pdmR3PicHlp_GetRCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @interface_method_impl{PDMPICHLPR3,pfnGetR0Helpers} */
static DECLCALLBACK(PCPDMPICHLPR0) pdmR3PicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    PCPDMPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pVM, NULL, "g_pdmR0PicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3PicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * PIC Device Helpers.
 */
const PDMPICHLPR3 g_pdmR3DevPicHlp =
{
    PDM_PICHLPR3_VERSION,
    pdmR3PicHlp_SetInterruptFF,
    pdmR3PicHlp_ClearInterruptFF,
    pdmR3PicHlp_Lock,
    pdmR3PicHlp_Unlock,
    pdmR3PicHlp_GetRCHelpers,
    pdmR3PicHlp_GetR0Helpers,
    PDM_PICHLPR3_VERSION /* the end */
};

/** @} */




/** @name R3 APIC Helpers
 * @{
 */

/** @interface_method_impl{PDMAPICHLPR3,pfnSetInterruptFF} */
static DECLCALLBACK(void) pdmR3ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns, PDMAPICIRQ enmType, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCpus);

    LogFlow(("pdmR3ApicHlp_SetInterruptFF: caller='%s'/%d: VMCPU_FF_INTERRUPT_APIC(%d) %d -> 1\n",
             pDevIns->pReg->szName, pDevIns->iInstance, idCpu, VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));

    switch (enmType)
    {
        case PDMAPICIRQ_HARDWARE:
            VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC);
            break;
        case PDMAPICIRQ_NMI:
            VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_NMI);
            break;
        case PDMAPICIRQ_SMI:
            VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_SMI);
            break;
        case PDMAPICIRQ_EXTINT:
            VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);
            break;
        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            break;
    }
#ifdef VBOX_WITH_REM
    REMR3NotifyInterruptSet(pVM, pVCpu);
#endif
    VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM | VMNOTIFYFF_FLAGS_POKE);
}


/** @interface_method_impl{PDMAPICHLPR3,pfnClearInterruptFF} */
static DECLCALLBACK(void) pdmR3ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns, PDMAPICIRQ enmType, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCpus);

    LogFlow(("pdmR3ApicHlp_ClearInterruptFF: caller='%s'/%d: VMCPU_FF_INTERRUPT_APIC(%d) %d -> 0\n",
             pDevIns->pReg->szName, pDevIns->iInstance, idCpu, VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));

    /* Note: NMI/SMI can't be cleared. */
    switch (enmType)
    {
        case PDMAPICIRQ_HARDWARE:
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
            break;
        case PDMAPICIRQ_EXTINT:
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
            break;
        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            break;
    }
#ifdef VBOX_WITH_REM
    REMR3NotifyInterruptClear(pVM, pVCpu);
#endif
}


/** @interface_method_impl{PDMAPICHLPR3,pfnCalcIrqTag} */
static DECLCALLBACK(uint32_t) pdmR3ApicHlp_CalcIrqTag(PPDMDEVINS pDevIns, uint8_t u8Level)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    Assert(u8Level == PDM_IRQ_LEVEL_HIGH || u8Level == PDM_IRQ_LEVEL_FLIP_FLOP);

    pdmLock(pVM);

    uint32_t uTagSrc = pdmCalcIrqTag(pVM, pDevIns->idTracing);
    if (u8Level == PDM_IRQ_LEVEL_HIGH)
        VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    else
        VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));


    pdmUnlock(pVM);
    LogFlow(("pdmR3ApicHlp_CalcIrqTag: caller='%s'/%d: returns %#x (u8Level=%d)\n",
             pDevIns->pReg->szName, pDevIns->iInstance, uTagSrc, u8Level));
    return uTagSrc;
}


/** @interface_method_impl{PDMAPICHLPR3,pfnChangeFeature} */
static DECLCALLBACK(void) pdmR3ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, PDMAPICMODE enmMode)
{
#ifdef VBOX_WITH_NEW_APIC
    /*
     * The old code is also most likely incorrect with regards to changing the CPUID bits,
     * see @bugref{8245#c32}.
     *
     * The new code should directly invoke APICUpdateCpuIdForMode() instead of using this
     * indirect helper.
     */
    AssertMsgFailed(("pdmR3ApicHlp_ChangeFeature unsupported in VBOX_WITH_NEW_APIC!"));
#else
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_ChangeFeature: caller='%s'/%d: mode=%d\n",
             pDevIns->pReg->szName, pDevIns->iInstance, (int)enmMode));
    switch (enmMode)
    {
        case PDMAPICMODE_NONE:
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICMODE_APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICMODE_X2APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_X2APIC);
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_APIC);
            break;
        default:
            AssertMsgFailed(("Unknown APIC mode: %d\n", (int)enmMode));
    }
#endif
}

/** @interface_method_impl{PDMAPICHLPR3,pfnGetCpuId} */
static DECLCALLBACK(VMCPUID) pdmR3ApicHlp_GetCpuId(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    return VMMGetCpuId(pDevIns->Internal.s.pVMR3);
}


/** @interface_method_impl{PDMAPICHLPR3,pfnSendStartupIpi} */
static DECLCALLBACK(void) pdmR3ApicHlp_SendStartupIpi(PPDMDEVINS pDevIns, VMCPUID idCpu, uint32_t uVector)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    VMMR3SendStartupIpi(pDevIns->Internal.s.pVMR3, idCpu, uVector);
}


/** @interface_method_impl{PDMAPICHLPR3,pfnSendInitIpi} */
static DECLCALLBACK(void) pdmR3ApicHlp_SendInitIpi(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    VMMR3SendInitIpi(pDevIns->Internal.s.pVMR3, idCpu);
}


/** @interface_method_impl{PDMAPICHLPR3,pfnGetRCHelpers} */
static DECLCALLBACK(PCPDMAPICHLPRC) pdmR3ApicHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    RTRCPTR pRCHelpers = NIL_RTRCPTR;
    if (!HMIsEnabled(pVM))
    {
        int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCApicHlp", &pRCHelpers);
        AssertReleaseRC(rc);
        AssertRelease(pRCHelpers);
    }

    LogFlow(("pdmR3ApicHlp_GetRCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @interface_method_impl{PDMAPICHLPR3,pfnGetR0Helpers} */
static DECLCALLBACK(PCPDMAPICHLPR0) pdmR3ApicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    PCPDMAPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pVM, NULL, "g_pdmR0ApicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3ApicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/** @interface_method_impl{PDMAPICHLPR3,pfnGetR3CritSect} */
static DECLCALLBACK(R3PTRTYPE(PPDMCRITSECT)) pdmR3ApicHlp_GetR3CritSect(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_Lock: caller='%s'/%d\n", pDevIns->pReg->szName, pDevIns->iInstance));
    return &pDevIns->Internal.s.pVMR3->pdm.s.CritSect;
}


/** @interface_method_impl{PDMAPICHLPR3,pfnGetRCCritSect} */
static DECLCALLBACK(RCPTRTYPE(PPDMCRITSECT)) pdmR3ApicHlp_GetRCCritSect(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    RTRCPTR RCPtr = MMHyperCCToRC(pVM, &pVM->pdm.s.CritSect);
    LogFlow(("pdmR3ApicHlp_GetR0CritSect: caller='%s'/%d: return %RRv\n", pDevIns->pReg->szName, pDevIns->iInstance, RCPtr));
    return RCPtr;
}


/** @interface_method_impl{PDMAPICHLPR3,pfnGetR3CritSect} */
static DECLCALLBACK(R0PTRTYPE(PPDMCRITSECT)) pdmR3ApicHlp_GetR0CritSect(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    RTR0PTR R0Ptr = MMHyperCCToR0(pVM, &pVM->pdm.s.CritSect);
    LogFlow(("pdmR3ApicHlp_GetR0CritSect: caller='%s'/%d: return %RHv\n", pDevIns->pReg->szName, pDevIns->iInstance, R0Ptr));
    return R0Ptr;
}



/**
 * APIC Device Helpers.
 */
const PDMAPICHLPR3 g_pdmR3DevApicHlp =
{
    PDM_APICHLPR3_VERSION,
    pdmR3ApicHlp_SetInterruptFF,
    pdmR3ApicHlp_ClearInterruptFF,
    pdmR3ApicHlp_CalcIrqTag,
    pdmR3ApicHlp_ChangeFeature,
    pdmR3ApicHlp_GetCpuId,
    pdmR3ApicHlp_SendStartupIpi,
    pdmR3ApicHlp_SendInitIpi,
    pdmR3ApicHlp_GetRCHelpers,
    pdmR3ApicHlp_GetR0Helpers,
    pdmR3ApicHlp_GetR3CritSect,
    pdmR3ApicHlp_GetRCCritSect,
    pdmR3ApicHlp_GetR0CritSect,
    PDM_APICHLPR3_VERSION /* the end */
};

/** @} */




/** @name Ring-3 I/O APIC Helpers
 * @{
 */

/** @interface_method_impl{PDMIOAPICHLPR3,pfnApicBusDeliver} */
static DECLCALLBACK(int) pdmR3IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3IoApicHlp_ApicBusDeliver: caller='%s'/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8 uTagSrc=%#x\n",
             pDevIns->pReg->szName, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode, uTagSrc));
    if (pVM->pdm.s.Apic.pfnBusDeliverR3)
        return pVM->pdm.s.Apic.pfnBusDeliverR3(pVM->pdm.s.Apic.pDevInsR3, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode, uTagSrc);
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMIOAPICHLPR3,pfnLock} */
static DECLCALLBACK(int) pdmR3IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3IoApicHlp_Lock: caller='%s'/%d: rc=%Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @interface_method_impl{PDMIOAPICHLPR3,pfnUnlock} */
static DECLCALLBACK(void) pdmR3IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3IoApicHlp_Unlock: caller='%s'/%d:\n", pDevIns->pReg->szName, pDevIns->iInstance));
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @interface_method_impl{PDMIOAPICHLPR3,pfnGetRCHelpers} */
static DECLCALLBACK(PCPDMIOAPICHLPRC) pdmR3IoApicHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    RTRCPTR pRCHelpers = NIL_RTRCPTR;
    if (!HMIsEnabled(pVM))
    {
        int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCIoApicHlp", &pRCHelpers);
        AssertReleaseRC(rc);
        AssertRelease(pRCHelpers);
    }

    LogFlow(("pdmR3IoApicHlp_GetRCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @interface_method_impl{PDMIOAPICHLPR3,pfnGetR0Helpers} */
static DECLCALLBACK(PCPDMIOAPICHLPR0) pdmR3IoApicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    PCPDMIOAPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pVM, NULL, "g_pdmR0IoApicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3IoApicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * I/O APIC Device Helpers.
 */
const PDMIOAPICHLPR3 g_pdmR3DevIoApicHlp =
{
    PDM_IOAPICHLPR3_VERSION,
    pdmR3IoApicHlp_ApicBusDeliver,
    pdmR3IoApicHlp_Lock,
    pdmR3IoApicHlp_Unlock,
    pdmR3IoApicHlp_GetRCHelpers,
    pdmR3IoApicHlp_GetR0Helpers,
    PDM_IOAPICHLPR3_VERSION /* the end */
};

/** @} */




/** @name Ring-3 PCI Bus Helpers
 * @{
 */

/** @interface_method_impl{PDMPCIHLPR3,pfnIsaSetIrq} */
static DECLCALLBACK(void) pdmR3PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR3PciHlp_IsaSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PDMIsaSetIrq(pDevIns->Internal.s.pVMR3, iIrq, iLevel, uTagSrc);
}

/** @interface_method_impl{PDMPCIHLPR3,pfnIoApicSetIrq} */
static DECLCALLBACK(void) pdmR3PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR3PciHlp_IoApicSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    PDMIoApicSetIrq(pDevIns->Internal.s.pVMR3, iIrq, iLevel, uTagSrc);
}

/** @interface_method_impl{PDMPCIHLPR3,pfnIoApicSendMsi} */
static DECLCALLBACK(void) pdmR3PciHlp_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR3PciHlp_IoApicSendMsi: address=%p value=%x uTagSrc=%#x\n", GCPhys, uValue, uTagSrc));
    PDMIoApicSendMsi(pDevIns->Internal.s.pVMR3, GCPhys, uValue, uTagSrc);
}

/** @interface_method_impl{PDMPCIHLPR3,pfnIsMMIO2Base} */
static DECLCALLBACK(bool) pdmR3PciHlp_IsMMIO2Base(PPDMDEVINS pDevIns, PPDMDEVINS pOwner, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    bool fRc = PGMR3PhysMMIO2IsBase(pDevIns->Internal.s.pVMR3, pOwner, GCPhys);
    Log4(("pdmR3PciHlp_IsMMIO2Base: pOwner=%p GCPhys=%RGp -> %RTbool\n", pOwner, GCPhys, fRc));
    return fRc;
}


/** @interface_method_impl{PDMPCIHLPR3,pfnLock} */
static DECLCALLBACK(int) pdmR3PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3PciHlp_Lock: caller='%s'/%d: rc=%Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @interface_method_impl{PDMPCIHLPR3,pfnUnlock} */
static DECLCALLBACK(void) pdmR3PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3PciHlp_Unlock: caller='%s'/%d:\n", pDevIns->pReg->szName, pDevIns->iInstance));
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @interface_method_impl{PDMPCIHLPR3,pfnGetRCHelpers} */
static DECLCALLBACK(PCPDMPCIHLPRC) pdmR3PciHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    RTRCPTR pRCHelpers = NIL_RTRCPTR;
    if (!HMIsEnabled(pVM))
    {
        int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCPciHlp", &pRCHelpers);
        AssertReleaseRC(rc);
        AssertRelease(pRCHelpers);
    }

    LogFlow(("pdmR3IoApicHlp_GetGCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @interface_method_impl{PDMPCIHLPR3,pfnGetR0Helpers} */
static DECLCALLBACK(PCPDMPCIHLPR0) pdmR3PciHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    PCPDMPCIHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pVM, NULL, "g_pdmR0PciHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3IoApicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * PCI Bus Device Helpers.
 */
const PDMPCIHLPR3 g_pdmR3DevPciHlp =
{
    PDM_PCIHLPR3_VERSION,
    pdmR3PciHlp_IsaSetIrq,
    pdmR3PciHlp_IoApicSetIrq,
    pdmR3PciHlp_IoApicSendMsi,
    pdmR3PciHlp_IsMMIO2Base,
    pdmR3PciHlp_GetRCHelpers,
    pdmR3PciHlp_GetR0Helpers,
    pdmR3PciHlp_Lock,
    pdmR3PciHlp_Unlock,
    PDM_PCIHLPR3_VERSION, /* the end */
};

/** @} */




/** @name Ring-3 HPET Helpers
 * {@
 */

/** @interface_method_impl{PDMHPETHLPR3,pfnSetLegacyMode} */
static DECLCALLBACK(int) pdmR3HpetHlp_SetLegacyMode(PPDMDEVINS pDevIns, bool fActivated)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3HpetHlp_SetLegacyMode: caller='%s'/%d: fActivated=%RTbool\n", pDevIns->pReg->szName, pDevIns->iInstance, fActivated));

    size_t                      i;
    int                         rc = VINF_SUCCESS;
    static const char * const   s_apszDevsToNotify[] =
    {
        "i8254",
        "mc146818"
    };
    for (i = 0; i < RT_ELEMENTS(s_apszDevsToNotify); i++)
    {
        PPDMIBASE pBase;
        rc = PDMR3QueryDevice(pDevIns->Internal.s.pVMR3->pUVM, "i8254", 0, &pBase);
        if (RT_SUCCESS(rc))
        {
            PPDMIHPETLEGACYNOTIFY pPort = PDMIBASE_QUERY_INTERFACE(pBase, PDMIHPETLEGACYNOTIFY);
            AssertLogRelMsgBreakStmt(pPort, ("%s\n", s_apszDevsToNotify[i]), rc = VERR_PDM_HPET_LEGACY_NOTIFY_MISSING);
            pPort->pfnModeChanged(pPort, fActivated);
        }
        else if (   rc == VERR_PDM_DEVICE_NOT_FOUND
                 || rc == VERR_PDM_DEVICE_INSTANCE_NOT_FOUND)
            rc = VINF_SUCCESS; /* the device isn't configured, ignore. */
        else
            AssertLogRelMsgFailedBreak(("%s -> %Rrc\n", s_apszDevsToNotify[i], rc));
    }

    /* Don't bother cleaning up, any failure here will cause a guru meditation. */

    LogFlow(("pdmR3HpetHlp_SetLegacyMode: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMHPETHLPR3,pfnSetIrq} */
static DECLCALLBACK(int) pdmR3HpetHlp_SetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3HpetHlp_SetIrq: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pReg->szName, pDevIns->iInstance, iIrq, iLevel));
    PVM pVM = pDevIns->Internal.s.pVMR3;

    pdmLock(pVM);
    uint32_t uTagSrc;
    if (iLevel & PDM_IRQ_LEVEL_HIGH)
    {
        pDevIns->Internal.s.uLastIrqTag = uTagSrc = pdmCalcIrqTag(pVM, pDevIns->idTracing);
        if (iLevel == PDM_IRQ_LEVEL_HIGH)
            VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
        else
            VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
        uTagSrc = pDevIns->Internal.s.uLastIrqTag;

    PDMIsaSetIrq(pVM, iIrq, iLevel, uTagSrc); /* (The API takes the lock recursively.) */

    if (iLevel == PDM_IRQ_LEVEL_LOW)
        VBOXVMM_PDM_IRQ_LOW(VMMGetCpu(pVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    pdmUnlock(pVM);
    return 0;
}


/** @interface_method_impl{PDMHPETHLPR3,pfnGetRCHelpers} */
static DECLCALLBACK(PCPDMHPETHLPRC) pdmR3HpetHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    RTRCPTR pRCHelpers = NIL_RTRCPTR;
    if (!HMIsEnabled(pVM))
    {
        int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCHpetHlp", &pRCHelpers);
        AssertReleaseRC(rc);
        AssertRelease(pRCHelpers);
    }

    LogFlow(("pdmR3HpetHlp_GetGCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @interface_method_impl{PDMHPETHLPR3,pfnGetR0Helpers} */
static DECLCALLBACK(PCPDMHPETHLPR0) pdmR3HpetHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    PCPDMHPETHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pVM, NULL, "g_pdmR0HpetHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3HpetHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * HPET Device Helpers.
 */
const PDMHPETHLPR3 g_pdmR3DevHpetHlp =
{
    PDM_HPETHLPR3_VERSION,
    pdmR3HpetHlp_GetRCHelpers,
    pdmR3HpetHlp_GetR0Helpers,
    pdmR3HpetHlp_SetLegacyMode,
    pdmR3HpetHlp_SetIrq,
    PDM_HPETHLPR3_VERSION, /* the end */
};

/** @} */


/** @name Ring-3 Raw PCI Device Helpers
 * {@
 */

/** @interface_method_impl{PDMPCIRAWHLPR3,pfnGetRCHelpers} */
static DECLCALLBACK(PCPDMPCIRAWHLPRC) pdmR3PciRawHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    RTRCPTR pRCHelpers = NIL_RTRCPTR;
    if (!HMIsEnabled(pVM))
    {
        int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_pdmRCPciRawHlp", &pRCHelpers);
        AssertReleaseRC(rc);
        AssertRelease(pRCHelpers);
    }

    LogFlow(("pdmR3PciRawHlp_GetGCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @interface_method_impl{PDMPCIRAWHLPR3,pfnGetR0Helpers} */
static DECLCALLBACK(PCPDMPCIRAWHLPR0) pdmR3PciRawHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    PCPDMHPETHLPR0 pR0Helpers = NIL_RTR0PTR;
    int rc = PDMR3LdrGetSymbolR0(pVM, NULL, "g_pdmR0PciRawHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3PciRawHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * Raw PCI Device Helpers.
 */
const PDMPCIRAWHLPR3 g_pdmR3DevPciRawHlp =
{
    PDM_PCIRAWHLPR3_VERSION,
    pdmR3PciRawHlp_GetRCHelpers,
    pdmR3PciRawHlp_GetR0Helpers,
    PDM_PCIRAWHLPR3_VERSION, /* the end */
};

/** @} */


/* none yet */

/**
 * DMAC Device Helpers.
 */
const PDMDMACHLP g_pdmR3DevDmacHlp =
{
    PDM_DMACHLP_VERSION
};




/* none yet */

/**
 * RTC Device Helpers.
 */
const PDMRTCHLP g_pdmR3DevRtcHlp =
{
    PDM_RTCHLP_VERSION
};
