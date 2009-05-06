/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, GC Device parts.
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

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
__BEGIN_DECLS
extern DECLEXPORT(const PDMDEVHLPRC)    g_pdmRCDevHlp;
extern DECLEXPORT(const PDMPICHLPRC)    g_pdmRCPicHlp;
extern DECLEXPORT(const PDMAPICHLPRC)   g_pdmRCApicHlp;
extern DECLEXPORT(const PDMIOAPICHLPRC) g_pdmRCIoApicHlp;
extern DECLEXPORT(const PDMPCIHLPRC)    g_pdmRCPciHlp;
__END_DECLS


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/** @name GC Device Helpers
 * @{
 */
static DECLCALLBACK(void) pdmGCDevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmGCDevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(int)  pdmGCDevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
static DECLCALLBACK(int)  pdmGCDevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(bool) pdmGCDevHlp_A20IsEnabled(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmGCDevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
static DECLCALLBACK(int)  pdmGCDevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va);
static DECLCALLBACK(int)  pdmGCDevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...);
static DECLCALLBACK(int)  pdmGCDevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va);
static DECLCALLBACK(int)  pdmGCDevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData);
static DECLCALLBACK(PVM)  pdmGCDevHlp_GetVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(PVMCPU)  pdmGCDevHlp_GetVMCPU(PPDMDEVINS pDevIns);
/** @} */


/** @name PIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmRCPicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmRCPicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmRCPicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmRCPicHlp_Unlock(PPDMDEVINS pDevIns);
/** @} */


/** @name APIC RC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmRCApicHlp_SetInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu);
static DECLCALLBACK(void) pdmRCApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu);
static DECLCALLBACK(void) pdmRCApicHlp_ChangeFeature(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion);
static DECLCALLBACK(int) pdmRCApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmRCApicHlp_Unlock(PPDMDEVINS pDevIns);
static DECLCALLBACK(VMCPUID) pdmRCApicHlp_GetCpuId(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmRCApicHlp_SendSipi(PPDMDEVINS pDevIns, VMCPUID idCpu, int iVector);
/** @} */


/** @name I/O APIC RC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmRCIoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode);
static DECLCALLBACK(int) pdmRCIoApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmRCIoApicHlp_Unlock(PPDMDEVINS pDevIns);
/** @} */


/** @name PCI Bus RC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmRCPciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmRCPciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(int) pdmRCPciHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmRCPciHlp_Unlock(PPDMDEVINS pDevIns);
/** @} */


static void pdmGCIsaSetIrq(PVM pVM, int iIrq, int iLevel);
static void pdmGCIoApicSetIrq(PVM pVM, int iIrq, int iLevel);



/**
 * The Guest Context Device Helper Callbacks.
 */
extern DECLEXPORT(const PDMDEVHLPRC) g_pdmRCDevHlp =
{
    PDM_DEVHLPRC_VERSION,
    pdmGCDevHlp_PCISetIrq,
    pdmGCDevHlp_ISASetIrq,
    pdmGCDevHlp_PhysRead,
    pdmGCDevHlp_PhysWrite,
    pdmGCDevHlp_A20IsEnabled,
    pdmGCDevHlp_VMSetError,
    pdmGCDevHlp_VMSetErrorV,
    pdmGCDevHlp_VMSetRuntimeError,
    pdmGCDevHlp_VMSetRuntimeErrorV,
    pdmGCDevHlp_PATMSetMMIOPatchInfo,
    pdmGCDevHlp_GetVM,
    pdmGCDevHlp_GetVMCPU,
    PDM_DEVHLPRC_VERSION
};

/**
 * The Raw-Mode Context PIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMPICHLPRC) g_pdmRCPicHlp =
{
    PDM_PICHLPRC_VERSION,
    pdmRCPicHlp_SetInterruptFF,
    pdmRCPicHlp_ClearInterruptFF,
    pdmRCPicHlp_Lock,
    pdmRCPicHlp_Unlock,
    PDM_PICHLPRC_VERSION
};


/**
 * The Raw-Mode Context APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMAPICHLPRC) g_pdmRCApicHlp =
{
    PDM_APICHLPRC_VERSION,
    pdmRCApicHlp_SetInterruptFF,
    pdmRCApicHlp_ClearInterruptFF,
    pdmRCApicHlp_ChangeFeature,
    pdmRCApicHlp_Lock,
    pdmRCApicHlp_Unlock,
    pdmRCApicHlp_GetCpuId,
    pdmRCApicHlp_SendSipi,
    PDM_APICHLPRC_VERSION
};


/**
 * The Raw-Mode Context I/O APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOAPICHLPRC) g_pdmRCIoApicHlp =
{
    PDM_IOAPICHLPRC_VERSION,
    pdmRCIoApicHlp_ApicBusDeliver,
    pdmRCIoApicHlp_Lock,
    pdmRCIoApicHlp_Unlock,
    PDM_IOAPICHLPRC_VERSION
};


/**
 * The Raw-Mode Context PCI Bus Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIHLPRC) g_pdmRCPciHlp =
{
    PDM_PCIHLPRC_VERSION,
    pdmRCPciHlp_IsaSetIrq,
    pdmRCPciHlp_IoApicSetIrq,
    pdmRCPciHlp_Lock,
    pdmRCPciHlp_Unlock,
    PDM_PCIHLPRC_VERSION, /* the end */
};




/** @copydoc PDMDEVHLPRC::pfnPCISetIrq */
static DECLCALLBACK(void) pdmGCDevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PCISetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    PVM         pVM     = pDevIns->Internal.s.pVMRC;
    PPCIDEVICE  pPciDev = pDevIns->Internal.s.pPciDeviceRC;
    PPDMPCIBUS  pPciBus = pDevIns->Internal.s.pPciBusRC;
    if (    pPciDev
        &&  pPciBus
        &&  pPciBus->pDevInsRC)
    {
        pdmLock(pVM);
        pPciBus->pfnSetIrqRC(pPciBus->pDevInsRC, pPciDev, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueRC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_PCI_SET_IRQ;
            pTask->pDevInsR3 = PDMDEVINS_2_R3PTR(pDevIns);
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueRC, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }

    LogFlow(("pdmGCDevHlp_PCISetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPRC::pfnPCISetIrq */
static DECLCALLBACK(void) pdmGCDevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_ISASetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    pdmGCIsaSetIrq(pDevIns->Internal.s.pVMRC, iIrq, iLevel);

    LogFlow(("pdmGCDevHlp_ISASetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPRC::pfnPhysRead */
static DECLCALLBACK(int) pdmGCDevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PhysRead: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbRead=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    int rc = PGMPhysRead(pDevIns->Internal.s.pVMRC, GCPhys, pvBuf, cbRead);
    AssertRC(rc); /** @todo track down the users for this bugger. */

    Log(("pdmGCDevHlp_PhysRead: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPRC::pfnPhysWrite */
static DECLCALLBACK(int) pdmGCDevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PhysWrite: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbWrite=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    int rc = PGMPhysWrite(pDevIns->Internal.s.pVMRC, GCPhys, pvBuf, cbWrite);
    AssertRC(rc); /** @todo track down the users for this bugger. */

    Log(("pdmGCDevHlp_PhysWrite: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPRC::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmGCDevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_A20IsEnabled: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    bool fEnabled = PGMPhysIsA20Enabled(VMMGetCpu0(pDevIns->Internal.s.pVMRC));

    Log(("pdmGCDevHlp_A20IsEnabled: caller=%p/%d: returns %RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    return fEnabled;
}


/** @copydoc PDMDEVHLPRC::pfnVMSetError */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMRC, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPRC::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMRC, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDEVHLPRC::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMRC, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @copydoc PDMDEVHLPRC::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMRC, fFlags, pszErrorId, pszFormat, va);
    return rc;
}


/** @copydoc PDMDEVHLPRC::pfnPATMSetMMIOPatchInfo */
static DECLCALLBACK(int) pdmGCDevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PATMSetMMIOPatchInfo: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    return PATMSetMMIOPatchInfo(pDevIns->Internal.s.pVMRC, GCPhys, (RTRCPTR)pCachedData);
}


/** @copydoc PDMDEVHLPRC::pfnGetVM */
static DECLCALLBACK(PVM)  pdmGCDevHlp_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_GetVM: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return pDevIns->Internal.s.pVMRC;
}


/** @copydoc PDMDEVHLPRC::pfnGetVMCPU */
static DECLCALLBACK(PVMCPU) pdmGCDevHlp_GetVMCPU(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_GetVMCPU: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return VMMGetCpu(pDevIns->Internal.s.pVMRC);
}




/** @copydoc PDMPICHLPGC::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmRCPicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMRC;
    PVMCPU pVCpu = &pVM->aCpus[0];  /* for PIC we always deliver to CPU 0, MP use APIC */

    LogFlow(("pdmRCPicHlp_SetInterruptFF: caller=%p/%d: VMMCPU_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);
}


/** @copydoc PDMPICHLPGC::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmRCPicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.CTX_SUFF(pVM);
    PVMCPU pVCpu = &pVM->aCpus[0];  /* for PIC we always deliver to CPU 0, MP use APIC */

    LogFlow(("pdmRCPicHlp_ClearInterruptFF: caller=%p/%d: VMCPU_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC)));

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
}


/** @copydoc PDMPICHLPGC::pfnLock */
static DECLCALLBACK(int) pdmRCPicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMRC, rc);
}


/** @copydoc PDMPICHLPGC::pfnUnlock */
static DECLCALLBACK(void) pdmRCPicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMRC);
}




/** @copydoc PDMAPICHLPRC::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmRCApicHlp_SetInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMRC;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCPUs);

    LogFlow(("pdmRCApicHlp_SetInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 1\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));
    VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPRC::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmRCApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns, VMCPUID idCpu)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMRC;
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    AssertReturnVoid(idCpu < pVM->cCPUs);

    LogFlow(("pdmRCApicHlp_ClearInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 0\n",
             pDevIns, pDevIns->iInstance, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC)));
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPRC::pfnChangeFeature */
static DECLCALLBACK(void) pdmRCApicHlp_ChangeFeature(PPDMDEVINS pDevIns, PDMAPICVERSION enmVersion)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmRCApicHlp_ChangeFeature: caller=%p/%d: version=%d\n", pDevIns, pDevIns->iInstance, (int)enmVersion));
    switch (enmVersion)
    {
        case PDMAPICVERSION_NONE:
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMRC, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMRC, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICVERSION_APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMRC, CPUMCPUIDFEATURE_APIC);
            CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMRC, CPUMCPUIDFEATURE_X2APIC);
            break;
        case PDMAPICVERSION_X2APIC:
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMRC, CPUMCPUIDFEATURE_X2APIC);
            CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMRC, CPUMCPUIDFEATURE_APIC);
            break;
        default:
            AssertMsgFailed(("Unknown APIC version: %d\n", (int)enmVersion));
    }
}


/** @copydoc PDMAPICHLPRC::pfnLock */
static DECLCALLBACK(int) pdmRCApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMRC, rc);
}


/** @copydoc PDMAPICHLPRC::pfnUnlock */
static DECLCALLBACK(void) pdmRCApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMRC);
}


/** @copydoc PDMAPICHLPRC::pfnGetCpuId */
static DECLCALLBACK(VMCPUID) pdmRCApicHlp_GetCpuId(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return VMMGetCpuId(pDevIns->Internal.s.pVMRC);
}


/** @copydoc PDMAPICHLPRC::pfnSendSipi */
static DECLCALLBACK(void) pdmRCApicHlp_SendSipi(PPDMDEVINS pDevIns, VMCPUID idCpu, int iVector)
{
    /* we shall never send a SIPI in raw mode */
    AssertFailed();
}




/** @copydoc PDMIOAPICHLPRC::pfnApicBusDeliver */
static DECLCALLBACK(void) pdmRCIoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMRC;
    LogFlow(("pdmRCIoApicHlp_ApicBusDeliver: caller=%p/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8\n",
             pDevIns, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    if (pVM->pdm.s.Apic.pfnBusDeliverRC)
        pVM->pdm.s.Apic.pfnBusDeliverRC(pVM->pdm.s.Apic.pDevInsRC, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}


/** @copydoc PDMIOAPICHLPRC::pfnLock */
static DECLCALLBACK(int) pdmRCIoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMRC, rc);
}


/** @copydoc PDMIOAPICHLPRC::pfnUnlock */
static DECLCALLBACK(void) pdmRCIoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMRC);
}




/** @copydoc PDMPCIHLPRC::pfnIsaSetIrq */
static DECLCALLBACK(void) pdmRCPciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmRCPciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmGCIsaSetIrq(pDevIns->Internal.s.pVMRC, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPRC::pfnIoApicSetIrq */
static DECLCALLBACK(void) pdmRCPciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmRCPciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmGCIoApicSetIrq(pDevIns->Internal.s.pVMRC, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPRC::pfnLock */
static DECLCALLBACK(int) pdmRCPciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMRC, rc);
}


/** @copydoc PDMPCIHLPRC::pfnUnlock */
static DECLCALLBACK(void) pdmRCPciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMRC);
}




/**
 * Sets an irq on the I/O APIC.
 *
 * @param   pVM     The VM handle.
 * @param   iIrq    The irq.
 * @param   iLevel  The new level.
 */
static void pdmGCIsaSetIrq(PVM pVM, int iIrq, int iLevel)
{
    if (    (   pVM->pdm.s.IoApic.pDevInsRC
             || !pVM->pdm.s.IoApic.pDevInsR3)
        &&  (   pVM->pdm.s.Pic.pDevInsRC
             || !pVM->pdm.s.Pic.pDevInsR3))
    {
        pdmLock(pVM);
        if (pVM->pdm.s.Pic.pDevInsRC)
            pVM->pdm.s.Pic.pfnSetIrqRC(pVM->pdm.s.Pic.pDevInsRC, iIrq, iLevel);
        if (pVM->pdm.s.IoApic.pDevInsRC)
            pVM->pdm.s.IoApic.pfnSetIrqRC(pVM->pdm.s.IoApic.pDevInsRC, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueRC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_ISA_SET_IRQ;
            pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueRC, &pTask->Core, 0);
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
static void pdmGCIoApicSetIrq(PVM pVM, int iIrq, int iLevel)
{
    if (pVM->pdm.s.IoApic.pDevInsRC)
    {
        pdmLock(pVM);
        pVM->pdm.s.IoApic.pfnSetIrqRC(pVM->pdm.s.IoApic.pDevInsRC, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else if (pVM->pdm.s.IoApic.pDevInsR3)
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueRC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_IOAPIC_SET_IRQ;
            pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueRC, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}
