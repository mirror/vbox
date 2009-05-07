/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, R0 Device parts.
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
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/patm.h>
#include <VBox/hwaccm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
__BEGIN_DECLS
extern DECLEXPORT(const PDMDEVHLPR0)    g_pdmR0DevHlp;
extern DECLEXPORT(const PDMPICHLPR0)    g_pdmR0PicHlp;
extern DECLEXPORT(const PDMAPICHLPR0)   g_pdmR0ApicHlp;
extern DECLEXPORT(const PDMIOAPICHLPR0) g_pdmR0IoApicHlp;
extern DECLEXPORT(const PDMPCIHLPR0)    g_pdmR0PciHlp;
__END_DECLS


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/** @name GC Device Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0DevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR0DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(int)  pdmR0DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
static DECLCALLBACK(int)  pdmR0DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(bool) pdmR0DevHlp_A20IsEnabled(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmR0DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
static DECLCALLBACK(int)  pdmR0DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va);
static DECLCALLBACK(int)  pdmR0DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...);
static DECLCALLBACK(int)  pdmR0DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va);
static DECLCALLBACK(int)  pdmR0DevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData);
static DECLCALLBACK(PVM)  pdmR0DevHlp_GetVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(bool) pdmR0DevHlp_CanEmulateIoBlock(PPDMDEVINS pDevIns);
static DECLCALLBACK(PVMCPU) pdmR0DevHlp_GetVMCPU(PPDMDEVINS pDevIns);
/** @} */


/** @name PIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0PicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR0PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmR0PicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0PicHlp_Unlock(PPDMDEVINS pDevIns);
/** @} */


/** @name APIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu);
static DECLCALLBACK(void) pdmR0ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu);
static DECLCALLBACK(void) pdmR0ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion);
static DECLCALLBACK(int)  pdmR0ApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0ApicHlp_Unlock(PPDMDEVINS pDevIns);
static DECLCALLBACK(VMCPUID) pdmR0ApicHlp_GetCpuId(PPDMDEVINS pDevIns);
/** @} */


/** @name I/O APIC GC Helpers
 * @{
 */
static DECLCALLBACK(int) pdmR0IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode);
static DECLCALLBACK(int) pdmR0IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0IoApicHlp_Unlock(PPDMDEVINS pDevIns);
/** @} */


/** @name PCI Bus GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(int) pdmR0PciHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0PciHlp_Unlock(PPDMDEVINS pDevIns);
/** @} */


static void pdmR0IsaSetIrq(PVM pVM, int iIrq, int iLevel);
static void pdmR0IoApicSetIrq(PVM pVM, int iIrq, int iLevel);



/**
 * The Guest Context Device Helper Callbacks.
 */
extern DECLEXPORT(const PDMDEVHLPR0) g_pdmR0DevHlp =
{
    PDM_DEVHLPR0_VERSION,
    pdmR0DevHlp_PCISetIrq,
    pdmR0DevHlp_ISASetIrq,
    pdmR0DevHlp_PhysRead,
    pdmR0DevHlp_PhysWrite,
    pdmR0DevHlp_A20IsEnabled,
    pdmR0DevHlp_VMSetError,
    pdmR0DevHlp_VMSetErrorV,
    pdmR0DevHlp_VMSetRuntimeError,
    pdmR0DevHlp_VMSetRuntimeErrorV,
    pdmR0DevHlp_PATMSetMMIOPatchInfo,
    pdmR0DevHlp_GetVM,
    pdmR0DevHlp_CanEmulateIoBlock,
    pdmR0DevHlp_GetVMCPU,
    PDM_DEVHLPR0_VERSION
};

/**
 * The Guest Context PIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMPICHLPR0) g_pdmR0PicHlp =
{
    PDM_PICHLPR0_VERSION,
    pdmR0PicHlp_SetInterruptFF,
    pdmR0PicHlp_ClearInterruptFF,
    pdmR0PicHlp_Lock,
    pdmR0PicHlp_Unlock,
    PDM_PICHLPR0_VERSION
};


/**
 * The Guest Context APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMAPICHLPR0) g_pdmR0ApicHlp =
{
    PDM_APICHLPR0_VERSION,
    pdmR0ApicHlp_SetInterruptFF,
    pdmR0ApicHlp_ClearInterruptFF,
    pdmR0ApicHlp_ChangeFeature,
    pdmR0ApicHlp_Lock,
    pdmR0ApicHlp_Unlock,
    pdmR0ApicHlp_GetCpuId,
    PDM_APICHLPR0_VERSION
};


/**
 * The Guest Context I/O APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOAPICHLPR0) g_pdmR0IoApicHlp =
{
    PDM_IOAPICHLPR0_VERSION,
    pdmR0IoApicHlp_ApicBusDeliver,
    pdmR0IoApicHlp_Lock,
    pdmR0IoApicHlp_Unlock,
    PDM_IOAPICHLPR0_VERSION
};


/**
 * The Guest Context PCI Bus Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIHLPR0) g_pdmR0PciHlp =
{
    PDM_PCIHLPR0_VERSION,
    pdmR0PciHlp_IsaSetIrq,
    pdmR0PciHlp_IoApicSetIrq,
    pdmR0PciHlp_Lock,
    pdmR0PciHlp_Unlock,
    PDM_PCIHLPR0_VERSION, /* the end */
};




/** @copydoc PDMDEVHLPR0::pfnPCISetIrq */
static DECLCALLBACK(void) pdmR0DevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    PVM          pVM     = pDevIns->Internal.s.pVMR0;
    PPCIDEVICE   pPciDev = pDevIns->Internal.s.pPciDeviceR0;
    PPDMPCIBUS   pPciBus = pDevIns->Internal.s.pPciBusR0;
    if (    pPciDev
        &&  pPciBus
        &&  pPciBus->pDevInsR0)
    {
        pdmLock(pVM);
        pPciBus->pfnSetIrqR0(pPciBus->pDevInsR0, pPciDev, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueR0);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_PCI_SET_IRQ;
            pTask->pDevInsR3 = PDMDEVINS_2_R3PTR(pDevIns);
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }

    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR0::pfnPCISetIrq */
static DECLCALLBACK(void) pdmR0DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    pdmR0IsaSetIrq(pDevIns->Internal.s.pVMR0, iIrq, iLevel);

    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR0::pfnPhysRead */
static DECLCALLBACK(int) pdmR0DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysRead: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbRead=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    int rc = PGMPhysRead(pDevIns->Internal.s.pVMR0, GCPhys, pvBuf, cbRead);
    AssertRC(rc); /** @todo track down the users for this bugger. */

    Log(("pdmR0DevHlp_PhysRead: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnPhysWrite */
static DECLCALLBACK(int) pdmR0DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysWrite: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbWrite=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    int rc = PGMPhysWrite(pDevIns->Internal.s.pVMR0, GCPhys, pvBuf, cbWrite);
    AssertRC(rc); /** @todo track down the users for this bugger. */

    Log(("pdmR0DevHlp_PhysWrite: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmR0DevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    bool fEnabled = PGMPhysIsA20Enabled(VMMGetCpu(pDevIns->Internal.s.pVMR0));

    Log(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d: returns %RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    return fEnabled;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetError */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetRuntimeErrorV */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pdmR0DevHlp_PATMSetMMIOPatchInfo*/
static DECLCALLBACK(int) pdmR0DevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PATMSetMMIOPatchInfo: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    AssertFailed();

/*    return PATMSetMMIOPatchInfo(pDevIns->Internal.s.pVMR0, GCPhys, pCachedData); */
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLPR0::pfnGetVM */
static DECLCALLBACK(PVM)  pdmR0DevHlp_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVM: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return pDevIns->Internal.s.pVMR0;
}


/** @copydoc PDMDEVHLPR0::pfnCanEmulateIoBlock */
static DECLCALLBACK(bool) pdmR0DevHlp_CanEmulateIoBlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVM: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return HWACCMCanEmulateIoBlock(VMMGetCpu(pDevIns->Internal.s.pVMR0));
}


/** @copydoc PDMDEVHLPR0::pfnGetVMCPU */
static DECLCALLBACK(PVMCPU) pdmR0DevHlp_GetVMCPU(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVMCPU: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return VMMGetCpu(pDevIns->Internal.s.pVMR0);
}




/** @copydoc PDMPICHLPR0::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR0PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM    pVM   = pDevIns->Internal.s.pVMR0;
    PVMCPU pVCpu = &pVM->aCpus[0];      /* for PIC we always deliver to CPU 0, MP use APIC */

    LogFlow(("pdmR0PicHlp_SetInterruptFF: caller=%p/%d: VMCPU_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);
}


/** @copydoc PDMPICHLPR0::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR0PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM    pVM   = pDevIns->Internal.s.pVMR0;
    PVMCPU pVCpu = &pVM->aCpus[0];      /* for PIC we always deliver to CPU 0, MP use APIC */

    LogFlow(("pdmR0PicHlp_ClearInterruptFF: caller=%p/%d: VMCPU_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
}


/** @copydoc PDMPICHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMR0, rc);
}


/** @copydoc PDMPICHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMR0);
}



/** @copydoc PDMAPICHLPR0::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR0ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM    pVM   = pDevIns->Internal.s.pVMR0;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCPUs);

    LogFlow(("pdmR0ApicHlp_SetInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 1\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));
    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPR0::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR0ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM    pVM   = pDevIns->Internal.s.pVMR0;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCPUs);

    LogFlow(("pdmR0ApicHlp_ClearInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 0\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPR0::pfnChangeFeature */
static DECLCALLBACK(void) pdmR0ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0ApicHlp_ChangeFeature: caller=%p/%d: version=%d\n", pDevIns, pDevIns->iInstance, (int)enmVersion));
    switch (enmVersion)
    {
        case PDMAPICVERSION_NONE:
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR0, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR0, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICVERSION_APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR0, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMR0, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICVERSION_X2APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR0, CPUMCPUIDFEATURE_X2APIC);
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMR0, CPUMCPUIDFEATURE_APIC);
            break;
        default:
            AssertMsgFailed(("Unknown APIC version: %d\n", (int)enmVersion));
    }
}


/** @copydoc PDMAPICHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0ApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMR0, rc);
}


/** @copydoc PDMAPICHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0ApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMR0);
}


/** @copydoc PDMAPICHLPR0::pfnGetCpuId */
static DECLCALLBACK(VMCPUID) pdmR0ApicHlp_GetCpuId(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return VMMGetCpuId(pDevIns->Internal.s.pVMR0);
}

/** @copydoc PDMIOAPICHLPR0::pfnApicBusDeliver */
static DECLCALLBACK(int) pdmR0IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR0;
    LogFlow(("pdmR0IoApicHlp_ApicBusDeliver: caller=%p/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8\n",
             pDevIns, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    Assert(pVM->pdm.s.Apic.pDevInsR0);
    if (pVM->pdm.s.Apic.pfnBusDeliverR0)
        return pVM->pdm.s.Apic.pfnBusDeliverR0(pVM->pdm.s.Apic.pDevInsR0, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
    return VINF_SUCCESS;
}


/** @copydoc PDMIOAPICHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMR0, rc);
}


/** @copydoc PDMIOAPICHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMR0);
}





/** @copydoc PDMPCIHLPR0::pfnIsaSetIrq */
static DECLCALLBACK(void) pdmR0PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmR0IsaSetIrq(pDevIns->Internal.s.pVMR0, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPR0::pfnIoApicSetIrq */
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmR0IoApicSetIrq(pDevIns->Internal.s.pVMR0, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMR0, rc);
}


/** @copydoc PDMPCIHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMR0);
}




/**
 * Sets an irq on the I/O APIC.
 *
 * @param   pVM     The VM handle.
 * @param   iIrq    The irq.
 * @param   iLevel  The new level.
 */
static void pdmR0IsaSetIrq(PVM pVM, int iIrq, int iLevel)
{
    if (    (   pVM->pdm.s.IoApic.pDevInsR0
             || !pVM->pdm.s.IoApic.pDevInsR3)
        &&  (   pVM->pdm.s.Pic.pDevInsR0
             || !pVM->pdm.s.Pic.pDevInsR3))
    {
        pdmLock(pVM);
        if (pVM->pdm.s.Pic.pDevInsR0)
            pVM->pdm.s.Pic.pfnSetIrqR0(pVM->pdm.s.Pic.pDevInsR0, iIrq, iLevel);
        if (pVM->pdm.s.IoApic.pDevInsR0)
            pVM->pdm.s.IoApic.pfnSetIrqR0(pVM->pdm.s.IoApic.pDevInsR0, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueR0);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_ISA_SET_IRQ;
            pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}


/**
 * Sets an irq on the I/O APIC.
 *
 * @param   pVM     The VM handle.
 * @param   iIrq    The irq.
 * @param   iLevel  The new level.
 */
static void pdmR0IoApicSetIrq(PVM pVM, int iIrq, int iLevel)
{
    if (pVM->pdm.s.IoApic.pDevInsR0)
    {
        pdmLock(pVM);
        pVM->pdm.s.IoApic.pfnSetIrqR0(pVM->pdm.s.IoApic.pDevInsR0, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else if (pVM->pdm.s.IoApic.pDevInsR3)
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueR0);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_IOAPIC_SET_IRQ;
            pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}
