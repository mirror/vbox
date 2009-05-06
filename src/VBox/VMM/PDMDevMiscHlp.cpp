/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, Misc. Device Helpers.
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
#define LOG_GROUP LOG_GROUP_PDM_DEVICE
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/rem.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>



/** @name HC PIC Helpers
 * @{
 */

/** @copydoc PDMPICHLPR3::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR3PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[0];  /* for PIC we always deliver to CPU 0, MP use APIC */

    LogFlow(("pdmR3PicHlp_SetInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);
    REMR3NotifyInterruptSet(pVM, pVCpu);
    VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM | VMNOTIFYFF_FLAGS_POKE);
}


/** @copydoc PDMPICHLPR3::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR3PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[0];  /* for PIC we always deliver to CPU 0, MP use APIC */

    LogFlow(("pdmR3PicHlp_ClearInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
    REMR3NotifyInterruptClear(pVM, pVCpu);
}


/** @copydoc PDMPICHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @copydoc PDMPICHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMPICHLPR3::pfnGetRCHelpers */
static DECLCALLBACK(PCPDMPICHLPRC) pdmR3PicHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    RTRCPTR pRCHelpers = 0;
    int rc = PDMR3LdrGetSymbolRC(pDevIns->Internal.s.pVMR3, NULL, "g_pdmRCPicHlp", &pRCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pRCHelpers);
    LogFlow(("pdmR3PicHlp_GetRCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @copydoc PDMPICHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMPICHLPR0) pdmR3PicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    PCPDMPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pDevIns->Internal.s.pVMR3, NULL, "g_pdmR0PicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3PicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
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




/** @name HC APIC Helpers
 * @{
 */

/** @copydoc PDMAPICHLPR3::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR3ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCPUs);

    LogFlow(("pdmR3ApicHlp_SetInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT(%d) %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, idCpu, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));

    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC);
    REMR3NotifyInterruptSet(pVM, pVCpu);
    VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM | VMNOTIFYFF_FLAGS_POKE);
}


/** @copydoc PDMAPICHLPR3::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR3ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCPUs);

    LogFlow(("pdmR3ApicHlp_ClearInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT(%d) %d -> 0\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, idCpu, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
    REMR3NotifyInterruptClear(pVM, pVCpu);
}


/** @copydoc PDMAPICHLPR3::pfnChangeFeature */
static DECLCALLBACK(void) pdmR3ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_ChangeFeature: caller='%s'/%d: version=%d\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, (int)enmVersion));
    switch (enmVersion)
    {
        case PDMAPICVERSION_NONE:
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICVERSION_APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICVERSION_X2APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_X2APIC);
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR3, CPUMCPUIDFEATURE_APIC);
            break;
        default:
            AssertMsgFailed(("Unknown APIC version: %d\n", (int)enmVersion));
    }
}

/** @copydoc PDMAPICHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3ApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_Lock: caller='%s'/%d: rc=%Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @copydoc PDMAPICHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3ApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_Unlock: caller='%s'/%d:\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMAPICHLPR3::pfnGetCpuId */
static DECLCALLBACK(VMCPUID) pdmR3ApicHlp_GetCpuId(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    return VMMGetCpuId(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMAPICHLPR3::pfnSendSipi */
static DECLCALLBACK(void) pdmR3ApicHlp_SendSipi(PPDMDEVINS pDevIns, VMCPUID idCpu, int iVector) /** @todo why signed? */
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);

    PVM pVM = pDevIns->Internal.s.pVMR3;
    PVMCPU pCpu = VMMGetCpuById(pVM, idCpu);
    CPUMSetGuestCS(pCpu, iVector * 0x100);
    CPUMSetGuestEIP(pCpu, 0);
    /** @todo: how do I unhalt VCPU?
     *  bird: See VMMSendSipi. */

}


/** @copydoc PDMAPICHLPR3::pfnGetRCHelpers */
static DECLCALLBACK(PCPDMAPICHLPRC) pdmR3ApicHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    RTRCPTR pRCHelpers = 0;
    int rc = PDMR3LdrGetSymbolRC(pDevIns->Internal.s.pVMR3, NULL, "g_pdmRCApicHlp", &pRCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pRCHelpers);
    LogFlow(("pdmR3ApicHlp_GetRCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @copydoc PDMAPICHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMAPICHLPR0) pdmR3ApicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    PCPDMAPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pDevIns->Internal.s.pVMR3, NULL, "g_pdmR0ApicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3ApicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * APIC Device Helpers.
 */
const PDMAPICHLPR3 g_pdmR3DevApicHlp =
{
    PDM_APICHLPR3_VERSION,
    pdmR3ApicHlp_SetInterruptFF,
    pdmR3ApicHlp_ClearInterruptFF,
    pdmR3ApicHlp_ChangeFeature,
    pdmR3ApicHlp_Lock,
    pdmR3ApicHlp_Unlock,
    pdmR3ApicHlp_GetCpuId,
    pdmR3ApicHlp_SendSipi,
    pdmR3ApicHlp_GetRCHelpers,
    pdmR3ApicHlp_GetR0Helpers,
    PDM_APICHLPR3_VERSION /* the end */
};

/** @} */




/** @name HC I/O APIC Helpers
 * @{
 */

/** @copydoc PDMIOAPICHLPR3::pfnApicBusDeliver */
static DECLCALLBACK(void) pdmR3IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3IoApicHlp_ApicBusDeliver: caller='%s'/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    if (pVM->pdm.s.Apic.pfnBusDeliverR3)
        pVM->pdm.s.Apic.pfnBusDeliverR3(pVM->pdm.s.Apic.pDevInsR3, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}


/** @copydoc PDMIOAPICHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3IoApicHlp_Lock: caller='%s'/%d: rc=%Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @copydoc PDMIOAPICHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3IoApicHlp_Unlock: caller='%s'/%d:\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMIOAPICHLPR3::pfnGetRCHelpers */
static DECLCALLBACK(PCPDMIOAPICHLPRC) pdmR3IoApicHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    RTRCPTR pRCHelpers = 0;
    int rc = PDMR3LdrGetSymbolRC(pDevIns->Internal.s.pVMR3, NULL, "g_pdmRCIoApicHlp", &pRCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pRCHelpers);
    LogFlow(("pdmR3IoApicHlp_GetRCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @copydoc PDMIOAPICHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMIOAPICHLPR0) pdmR3IoApicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    PCPDMIOAPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pDevIns->Internal.s.pVMR3, NULL, "g_pdmR0IoApicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3IoApicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
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




/** @name HC PCI Bus Helpers
 * @{
 */

/** @copydoc PDMPCIHLPR3::pfnIsaSetIrq */
static DECLCALLBACK(void) pdmR3PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR3PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    PDMIsaSetIrq(pDevIns->Internal.s.pVMR3, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPR3::pfnIoApicSetIrq */
static DECLCALLBACK(void) pdmR3PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR3PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    PDMIoApicSetIrq(pDevIns->Internal.s.pVMR3, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPR3::pfnIsMMIO2Base */
static DECLCALLBACK(bool) pdmR3PciHlp_IsMMIO2Base(PPDMDEVINS pDevIns, PPDMDEVINS pOwner, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    bool fRc = PGMR3PhysMMIO2IsBase(pDevIns->Internal.s.pVMR3, pOwner, GCPhys);
    Log4(("pdmR3PciHlp_IsMMIO2Base: pOwner=%p GCPhys=%RGp -> %RTbool\n", pOwner, GCPhys, fRc));
    return fRc;
}


/** @copydoc PDMPCIHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3PciHlp_Lock: caller='%s'/%d: rc=%Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return pdmLockEx(pDevIns->Internal.s.pVMR3, rc);
}


/** @copydoc PDMPCIHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3PciHlp_Unlock: caller='%s'/%d:\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    pdmUnlock(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMPCIHLPR3::pfnGetRCHelpers */
static DECLCALLBACK(PCPDMPCIHLPRC) pdmR3PciHlp_GetRCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    RTRCPTR pRCHelpers = 0;
    int rc = PDMR3LdrGetSymbolRC(pDevIns->Internal.s.pVMR3, NULL, "g_pdmRCPciHlp", &pRCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pRCHelpers);
    LogFlow(("pdmR3IoApicHlp_GetGCHelpers: caller='%s'/%d: returns %RRv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pRCHelpers));
    return pRCHelpers;
}


/** @copydoc PDMPCIHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMPCIHLPR0) pdmR3PciHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    PCPDMPCIHLPR0 pR0Helpers = 0;
    int rc = PDMR3LdrGetSymbolR0(pDevIns->Internal.s.pVMR3, NULL, "g_pdmR0PciHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3IoApicHlp_GetR0Helpers: caller='%s'/%d: returns %RHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
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
    pdmR3PciHlp_IsMMIO2Base,
    pdmR3PciHlp_GetRCHelpers,
    pdmR3PciHlp_GetR0Helpers,
    pdmR3PciHlp_Lock,
    pdmR3PciHlp_Unlock,
    PDM_PCIHLPR3_VERSION, /* the end */
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


