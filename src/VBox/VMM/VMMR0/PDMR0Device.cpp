/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, R0 Device parts.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define PDMPCIDEV_INCLUDE_PRIVATE  /* Hack to get pdmpcidevint.h included at the right point. */
#include "PDMInternal.h"
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/gvm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/gvmm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/msi.h>
#include <VBox/sup.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include "dtrace/VBoxVMM.h"
#include "PDMInline.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
extern DECLEXPORT(const PDMDEVHLPR0)    g_pdmR0DevHlp;
extern DECLEXPORT(const PDMPICHLP)      g_pdmR0PicHlp;
extern DECLEXPORT(const PDMIOAPICHLP)   g_pdmR0IoApicHlp;
extern DECLEXPORT(const PDMPCIHLPR0)    g_pdmR0PciHlp;
extern DECLEXPORT(const PDMIOMMUHLPR0)  g_pdmR0IommuHlp;
extern DECLEXPORT(const PDMHPETHLPR0)   g_pdmR0HpetHlp;
extern DECLEXPORT(const PDMPCIRAWHLPR0) g_pdmR0PciRawHlp;
extern DECLEXPORT(const PDMDRVHLPR0)    g_pdmR0DrvHlp;
RT_C_DECLS_END

/** List of PDMDEVMODREGR0 structures protected by the loader lock. */
static RTLISTANCHOR g_PDMDevModList;


/**
 * Pointer to the ring-0 device registrations for VMMR0.
 */
static const PDMDEVREGR0 *g_apVMM0DevRegs[] =
{
    &g_DeviceAPIC,
};

/**
 * Module device registration record for VMMR0.
 */
static PDMDEVMODREGR0 g_VBoxDDR0ModDevReg =
{
    /* .u32Version = */ PDM_DEVMODREGR0_VERSION,
    /* .cDevRegs = */   RT_ELEMENTS(g_apVMM0DevRegs),
    /* .papDevRegs = */ &g_apVMM0DevRegs[0],
    /* .hMod = */       NULL,
    /* .ListEntry = */  { NULL, NULL },
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static bool pdmR0IsaSetIrq(PGVM pGVM, int iIrq, int iLevel, uint32_t uTagSrc);


/**
 * Initializes the global ring-0 PDM data.
 */
VMMR0_INT_DECL(void) PDMR0Init(void *hMod)
{
    RTListInit(&g_PDMDevModList);
    g_VBoxDDR0ModDevReg.hMod = hMod;
    RTListAppend(&g_PDMDevModList, &g_VBoxDDR0ModDevReg.ListEntry);
}


/**
 * Used by PDMR0CleanupVM to destroy a device instance.
 *
 * This is done during VM cleanup so that we're sure there are no active threads
 * inside the device code.
 *
 * @param   pGVM        The global (ring-0) VM structure.
 * @param   pDevIns     The device instance.
 * @param   idxR0Device The device instance handle.
 */
static int pdmR0DeviceDestroy(PGVM pGVM, PPDMDEVINSR0 pDevIns, uint32_t idxR0Device)
{
    /*
     * Assert sanity.
     */
    Assert(idxR0Device < pGVM->pdmr0.s.cDevInstances);
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    Assert(pDevIns->u32Version == PDM_DEVINSR0_VERSION);
    Assert(pDevIns->Internal.s.idxR0Device == idxR0Device);

    /*
     * Call the final destructor if there is one.
     */
    if (pDevIns->pReg->pfnFinalDestruct)
        pDevIns->pReg->pfnFinalDestruct(pDevIns);
    pDevIns->u32Version = ~PDM_DEVINSR0_VERSION;

    /*
     * Remove the device from the instance table.
     */
    Assert(pGVM->pdmr0.s.apDevInstances[idxR0Device] == pDevIns);
    pGVM->pdmr0.s.apDevInstances[idxR0Device] = NULL;
    if (idxR0Device + 1 == pGVM->pdmr0.s.cDevInstances)
        pGVM->pdmr0.s.cDevInstances = idxR0Device;

    /*
     * Free the ring-3 mapping and instance memory.
     */
    RTR0MEMOBJ hMemObj = pDevIns->Internal.s.hMapObj;
    pDevIns->Internal.s.hMapObj = NIL_RTR0MEMOBJ;
    RTR0MemObjFree(hMemObj, true);

    hMemObj = pDevIns->Internal.s.hMemObj;
    pDevIns->Internal.s.hMemObj = NIL_RTR0MEMOBJ;
    RTR0MemObjFree(hMemObj, true);

    return VINF_SUCCESS;
}


/**
 * Initializes the per-VM data for the PDM.
 *
 * This is called from under the GVMM lock, so it only need to initialize the
 * data so PDMR0CleanupVM and others will work smoothly.
 *
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(void) PDMR0InitPerVMData(PGVM pGVM)
{
    AssertCompile(sizeof(pGVM->pdm.s) <= sizeof(pGVM->pdm.padding));
    AssertCompile(sizeof(pGVM->pdmr0.s) <= sizeof(pGVM->pdmr0.padding));

    pGVM->pdmr0.s.cDevInstances = 0;
}


/**
 * Cleans up any loose ends before the GVM structure is destroyed.
 */
VMMR0_INT_DECL(void) PDMR0CleanupVM(PGVM pGVM)
{
    uint32_t i = pGVM->pdmr0.s.cDevInstances;
    while (i-- > 0)
    {
        PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[i];
        if (pDevIns)
            pdmR0DeviceDestroy(pGVM, pDevIns, i);
    }
}


/** @name Ring-0 Device Helpers
 * @{
 */

/** @interface_method_impl{PDMDEVHLPR0,pfnIoPortSetUpContextEx} */
static DECLCALLBACK(int) pdmR0DevHlp_IoPortSetUpContextEx(PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts,
                                                          PFNIOMIOPORTNEWOUT pfnOut, PFNIOMIOPORTNEWIN pfnIn,
                                                          PFNIOMIOPORTNEWOUTSTRING pfnOutStr, PFNIOMIOPORTNEWINSTRING pfnInStr,
                                                          void *pvUser)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_IoPortSetUpContextEx: caller='%s'/%d: hIoPorts=%#x pfnOut=%p pfnIn=%p pfnOutStr=%p pfnInStr=%p pvUser=%p\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hIoPorts, pfnOut, pfnIn, pfnOutStr, pfnInStr, pvUser));
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE);

    int rc = IOMR0IoPortSetUpContext(pGVM, pDevIns, hIoPorts, pfnOut, pfnIn, pfnOutStr, pfnInStr, pvUser);

    LogFlow(("pdmR0DevHlp_IoPortSetUpContextEx: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnMmioSetUpContextEx} */
static DECLCALLBACK(int) pdmR0DevHlp_MmioSetUpContextEx(PPDMDEVINS pDevIns, IOMMMIOHANDLE hRegion, PFNIOMMMIONEWWRITE pfnWrite,
                                                        PFNIOMMMIONEWREAD pfnRead, PFNIOMMMIONEWFILL pfnFill, void *pvUser)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_MmioSetUpContextEx: caller='%s'/%d: hRegion=%#x pfnWrite=%p pfnRead=%p pfnFill=%p pvUser=%p\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hRegion, pfnWrite, pfnRead, pfnFill, pvUser));
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE);

    int rc = IOMR0MmioSetUpContext(pGVM, pDevIns, hRegion, pfnWrite, pfnRead, pfnFill, pvUser);

    LogFlow(("pdmR0DevHlp_MmioSetUpContextEx: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnMmio2SetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_Mmio2SetUpContext(PPDMDEVINS pDevIns, PGMMMIO2HANDLE hRegion,
                                                       size_t offSub, size_t cbSub, void **ppvMapping)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_Mmio2SetUpContext: caller='%s'/%d: hRegion=%#x offSub=%#zx cbSub=%#zx ppvMapping=%p\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hRegion, offSub, cbSub, ppvMapping));
    *ppvMapping = NULL;

    PGVM pGVM = pDevIns->Internal.s.pGVM;
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE);

    int rc = PGMR0PhysMMIO2MapKernel(pGVM, pDevIns, hRegion, offSub, cbSub, ppvMapping);

    LogFlow(("pdmR0DevHlp_Mmio2SetUpContext: caller='%s'/%d: returns %Rrc (%p)\n", pDevIns->pReg->szName, pDevIns->iInstance, rc, *ppvMapping));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCIPhysRead} */
static DECLCALLBACK(int) pdmR0DevHlp_PCIPhysRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys,
                                                 void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!pPciDev) /* NULL is an alias for the default PCI device. */
        pPciDev = pDevIns->apPciDevs[0];
    AssertReturn(pPciDev, VERR_PDM_NOT_PCI_DEVICE);
    PDMPCIDEV_ASSERT_VALID_AND_REGISTERED(pDevIns, pPciDev);

#ifndef PDM_DO_NOT_RESPECT_PCI_BM_BIT
    /*
     * Just check the busmaster setting here and forward the request to the generic read helper.
     */
    if (PCIDevIsBusmaster(pPciDev))
    { /* likely */ }
    else
    {
        Log(("pdmRCDevHlp_PCIPhysRead: caller=%p/%d: returns %Rrc - Not bus master! GCPhys=%RGp cbRead=%#zx\n",
             pDevIns, pDevIns->iInstance, VERR_PDM_NOT_PCI_BUS_MASTER, GCPhys, cbRead));
        memset(pvBuf, 0xff, cbRead);
        return VERR_PDM_NOT_PCI_BUS_MASTER;
    }
#endif

    return pDevIns->pHlpR0->pfnPhysRead(pDevIns, GCPhys, pvBuf, cbRead);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCIPhysWrite} */
static DECLCALLBACK(int) pdmR0DevHlp_PCIPhysWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys,
                                                  const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!pPciDev) /* NULL is an alias for the default PCI device. */
        pPciDev = pDevIns->apPciDevs[0];
    AssertReturn(pPciDev, VERR_PDM_NOT_PCI_DEVICE);
    PDMPCIDEV_ASSERT_VALID_AND_REGISTERED(pDevIns, pPciDev);

#ifndef PDM_DO_NOT_RESPECT_PCI_BM_BIT
    /*
     * Just check the busmaster setting here and forward the request to the generic read helper.
     */
    if (PCIDevIsBusmaster(pPciDev))
    { /* likely */ }
    else
    {
        Log(("pdmRCDevHlp_PCIPhysWrite: caller=%p/%d: returns %Rrc - Not bus master! GCPhys=%RGp cbWrite=%#zx\n",
             pDevIns, pDevIns->iInstance, VERR_PDM_NOT_PCI_BUS_MASTER, GCPhys, cbWrite));
        return VERR_PDM_NOT_PCI_BUS_MASTER;
    }
#endif

    return pDevIns->pHlpR0->pfnPhysWrite(pDevIns, GCPhys, pvBuf, cbWrite);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCISetIrq} */
static DECLCALLBACK(void) pdmR0DevHlp_PCISetIrq(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!pPciDev) /* NULL is an alias for the default PCI device. */
        pPciDev = pDevIns->apPciDevs[0];
    AssertReturnVoid(pPciDev);
    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: pPciDev=%p:{%#x} iIrq=%d iLevel=%d\n",
             pDevIns, pDevIns->iInstance, pPciDev, pPciDev->uDevFn, iIrq, iLevel));
    PDMPCIDEV_ASSERT_VALID_AND_REGISTERED(pDevIns, pPciDev);

    PGVM         pGVM      = pDevIns->Internal.s.pGVM;
    size_t const idxBus    = pPciDev->Int.s.idxPdmBus;
    AssertReturnVoid(idxBus < RT_ELEMENTS(pGVM->pdmr0.s.aPciBuses));
    PPDMPCIBUSR0 pPciBusR0 = &pGVM->pdmr0.s.aPciBuses[idxBus];

    pdmLock(pGVM);

    uint32_t uTagSrc;
    if (iLevel & PDM_IRQ_LEVEL_HIGH)
    {
        pDevIns->Internal.s.pIntR3R0->uLastIrqTag = uTagSrc = pdmCalcIrqTag(pGVM, pDevIns->Internal.s.pInsR3R0->idTracing);
        if (iLevel == PDM_IRQ_LEVEL_HIGH)
            VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
        else
            VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
        uTagSrc = pDevIns->Internal.s.pIntR3R0->uLastIrqTag;

    if (pPciBusR0->pDevInsR0)
    {
        pPciBusR0->pfnSetIrqR0(pPciBusR0->pDevInsR0, pPciDev, iIrq, iLevel, uTagSrc);

        pdmUnlock(pGVM);

        if (iLevel == PDM_IRQ_LEVEL_LOW)
            VBOXVMM_PDM_IRQ_LOW(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
    {
        pdmUnlock(pGVM);

        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pGVM->pdm.s.pDevHlpQueueR0);
        AssertReturnVoid(pTask);

        pTask->enmOp = PDMDEVHLPTASKOP_PCI_SET_IRQ;
        pTask->pDevInsR3 = PDMDEVINS_2_R3PTR(pDevIns);
        pTask->u.PciSetIRQ.iIrq = iIrq;
        pTask->u.PciSetIRQ.iLevel = iLevel;
        pTask->u.PciSetIRQ.uTagSrc = uTagSrc;
        pTask->u.PciSetIRQ.pPciDevR3 = MMHyperR0ToR3(pGVM, pPciDev);

        PDMQueueInsertEx(pGVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
    }

    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: returns void; uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, uTagSrc));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnISASetIrq} */
static DECLCALLBACK(void) pdmR0DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    pdmLock(pGVM);
    uint32_t uTagSrc;
    if (iLevel & PDM_IRQ_LEVEL_HIGH)
    {
        pDevIns->Internal.s.pIntR3R0->uLastIrqTag = uTagSrc = pdmCalcIrqTag(pGVM, pDevIns->Internal.s.pInsR3R0->idTracing);
        if (iLevel == PDM_IRQ_LEVEL_HIGH)
            VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
        else
            VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
        uTagSrc = pDevIns->Internal.s.pIntR3R0->uLastIrqTag;

    bool fRc = pdmR0IsaSetIrq(pGVM, iIrq, iLevel, uTagSrc);

    if (iLevel == PDM_IRQ_LEVEL_LOW && fRc)
        VBOXVMM_PDM_IRQ_LOW(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    pdmUnlock(pGVM);
    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: returns void; uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, uTagSrc));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnIoApicSendMsi} */
static DECLCALLBACK(void) pdmR0DevHlp_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_IoApicSendMsi: caller=%p/%d: GCPhys=%RGp uValue=%#x\n", pDevIns, pDevIns->iInstance, GCPhys, uValue));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    uint32_t uTagSrc;
    pDevIns->Internal.s.pIntR3R0->uLastIrqTag = uTagSrc = pdmCalcIrqTag(pGVM, pDevIns->Internal.s.pInsR3R0->idTracing);
    VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));

    if (pGVM->pdm.s.IoApic.pDevInsR0)
        pGVM->pdm.s.IoApic.pfnSendMsiR0(pGVM->pdm.s.IoApic.pDevInsR0, GCPhys, uValue, uTagSrc);
    else
        AssertFatalMsgFailed(("Lazy bastards!"));

    LogFlow(("pdmR0DevHlp_IoApicSendMsi: caller=%p/%d: returns void; uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, uTagSrc));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPhysRead} */
static DECLCALLBACK(int) pdmR0DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysRead: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbRead=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    VBOXSTRICTRC rcStrict = PGMPhysRead(pDevIns->Internal.s.pGVM, GCPhys, pvBuf, cbRead, PGMACCESSORIGIN_DEVICE);
    AssertMsg(rcStrict == VINF_SUCCESS, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict))); /** @todo track down the users for this bugger. */

    Log(("pdmR0DevHlp_PhysRead: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, VBOXSTRICTRC_VAL(rcStrict) ));
    return VBOXSTRICTRC_VAL(rcStrict);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPhysWrite} */
static DECLCALLBACK(int) pdmR0DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysWrite: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbWrite=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    VBOXSTRICTRC rcStrict = PGMPhysWrite(pDevIns->Internal.s.pGVM, GCPhys, pvBuf, cbWrite, PGMACCESSORIGIN_DEVICE);
    AssertMsg(rcStrict == VINF_SUCCESS, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict))); /** @todo track down the users for this bugger. */

    Log(("pdmR0DevHlp_PhysWrite: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, VBOXSTRICTRC_VAL(rcStrict) ));
    return VBOXSTRICTRC_VAL(rcStrict);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnA20IsEnabled} */
static DECLCALLBACK(bool) pdmR0DevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    bool fEnabled = PGMPhysIsA20Enabled(VMMGetCpu(pDevIns->Internal.s.pGVM));

    Log(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d: returns %RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    return fEnabled;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMState} */
static DECLCALLBACK(VMSTATE) pdmR0DevHlp_VMState(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);

    VMSTATE enmVMState = pDevIns->Internal.s.pGVM->enmVMState;

    LogFlow(("pdmR0DevHlp_VMState: caller=%p/%d: returns %d\n", pDevIns, pDevIns->iInstance, enmVMState));
    return enmVMState;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetError} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pGVM, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetErrorV} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pGVM, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetRuntimeError} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pGVM, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetRuntimeErrorV} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pGVM, fFlags, pszErrorId, pszFormat, va);
    return rc;
}



/** @interface_method_impl{PDMDEVHLPR0,pfnGetVM} */
static DECLCALLBACK(PVMCC)  pdmR0DevHlp_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVM: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return pDevIns->Internal.s.pGVM;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnGetVMCPU} */
static DECLCALLBACK(PVMCPUCC) pdmR0DevHlp_GetVMCPU(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVMCPU: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return VMMGetCpu(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMDEVHLPRC,pfnGetCurrentCpuId} */
static DECLCALLBACK(VMCPUID) pdmR0DevHlp_GetCurrentCpuId(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VMCPUID idCpu = VMMGetCpuId(pDevIns->Internal.s.pGVM);
    LogFlow(("pdmR0DevHlp_GetCurrentCpuId: caller='%p'/%d for CPU %u\n", pDevIns, pDevIns->iInstance, idCpu));
    return idCpu;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerToPtr} */
static DECLCALLBACK(PTMTIMERR0) pdmR0DevHlp_TimerToPtr(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return (PTMTIMERR0)MMHyperR3ToCC(pDevIns->Internal.s.pGVM, hTimer);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerFromMicro} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerFromMicro(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMicroSecs)
{
    return TMTimerFromMicro(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMicroSecs);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerFromMilli} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerFromMilli(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMilliSecs)
{
    return TMTimerFromMilli(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMilliSecs);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerFromNano} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerFromNano(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cNanoSecs)
{
    return TMTimerFromNano(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cNanoSecs);
}

/** @interface_method_impl{PDMDEVHLPR0,pfnTimerGet} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerGet(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerGet(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerGetFreq} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerGetFreq(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerGetFreq(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerGetNano} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerGetNano(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerGetNano(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerIsActive} */
static DECLCALLBACK(bool) pdmR0DevHlp_TimerIsActive(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerIsActive(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerIsLockOwner} */
static DECLCALLBACK(bool) pdmR0DevHlp_TimerIsLockOwner(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerIsLockOwner(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerLockClock} */
static DECLCALLBACK(VBOXSTRICTRC) pdmR0DevHlp_TimerLockClock(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, int rcBusy)
{
    return TMTimerLock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), rcBusy);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerLockClock2} */
static DECLCALLBACK(VBOXSTRICTRC) pdmR0DevHlp_TimerLockClock2(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer,
                                                              PPDMCRITSECT pCritSect, int rcBusy)
{
    VBOXSTRICTRC rc = TMTimerLock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), rcBusy);
    if (rc == VINF_SUCCESS)
    {
        rc = PDMCritSectEnter(pCritSect, rcBusy);
        if (rc == VINF_SUCCESS)
            return rc;
        AssertRC(VBOXSTRICTRC_VAL(rc));
        TMTimerUnlock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
    }
    else
        AssertRC(VBOXSTRICTRC_VAL(rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSet} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSet(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t uExpire)
{
    return TMTimerSet(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), uExpire);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetFrequencyHint} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetFrequencyHint(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint32_t uHz)
{
    return TMTimerSetFrequencyHint(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), uHz);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetMicro} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetMicro(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMicrosToNext)
{
    return TMTimerSetMicro(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMicrosToNext);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetMillies} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetMillies(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMilliesToNext)
{
    return TMTimerSetMillies(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMilliesToNext);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetNano} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetNano(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cNanosToNext)
{
    return TMTimerSetNano(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cNanosToNext);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetRelative} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetRelative(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cTicksToNext, uint64_t *pu64Now)
{
    return TMTimerSetRelative(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cTicksToNext, pu64Now);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerStop} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerStop(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerStop(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerUnlockClock} */
static DECLCALLBACK(void) pdmR0DevHlp_TimerUnlockClock(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    TMTimerUnlock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerUnlockClock2} */
static DECLCALLBACK(void) pdmR0DevHlp_TimerUnlockClock2(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, PPDMCRITSECT pCritSect)
{
    TMTimerUnlock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
    int rc = PDMCritSectLeave(pCritSect);
    AssertRC(rc);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTMTimeVirtGet} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TMTimeVirtGet(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TMTimeVirtGet: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return TMVirtualGet(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTMTimeVirtGetFreq} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TMTimeVirtGetFreq(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TMTimeVirtGetFreq: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return TMVirtualGetFreq(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTMTimeVirtGetNano} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TMTimeVirtGetNano(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TMTimeVirtGetNano: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return TMVirtualToNano(pDevIns->Internal.s.pGVM, TMVirtualGet(pDevIns->Internal.s.pGVM));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnQueueToPtr} */
static DECLCALLBACK(PPDMQUEUE)  pdmR0DevHlp_QueueToPtr(PPDMDEVINS pDevIns, PDMQUEUEHANDLE hQueue)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return (PPDMQUEUE)MMHyperR3ToCC(pDevIns->Internal.s.pGVM, hQueue);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnQueueAlloc} */
static DECLCALLBACK(PPDMQUEUEITEMCORE) pdmR0DevHlp_QueueAlloc(PPDMDEVINS pDevIns, PDMQUEUEHANDLE hQueue)
{
    return PDMQueueAlloc(pdmR0DevHlp_QueueToPtr(pDevIns, hQueue));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnQueueInsert} */
static DECLCALLBACK(void) pdmR0DevHlp_QueueInsert(PPDMDEVINS pDevIns, PDMQUEUEHANDLE hQueue, PPDMQUEUEITEMCORE pItem)
{
    return PDMQueueInsert(pdmR0DevHlp_QueueToPtr(pDevIns, hQueue), pItem);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnQueueInsertEx} */
static DECLCALLBACK(void) pdmR0DevHlp_QueueInsertEx(PPDMDEVINS pDevIns, PDMQUEUEHANDLE hQueue, PPDMQUEUEITEMCORE pItem,
                                                    uint64_t cNanoMaxDelay)
{
    return PDMQueueInsertEx(pdmR0DevHlp_QueueToPtr(pDevIns, hQueue), pItem, cNanoMaxDelay);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnQueueFlushIfNecessary} */
static DECLCALLBACK(bool) pdmR0DevHlp_QueueFlushIfNecessary(PPDMDEVINS pDevIns, PDMQUEUEHANDLE hQueue)
{
    return PDMQueueFlushIfNecessary(pdmR0DevHlp_QueueToPtr(pDevIns, hQueue));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTaskTrigger} */
static DECLCALLBACK(int) pdmR0DevHlp_TaskTrigger(PPDMDEVINS pDevIns, PDMTASKHANDLE hTask)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TaskTrigger: caller='%s'/%d: hTask=%RU64\n", pDevIns->pReg->szName, pDevIns->iInstance, hTask));

    int rc = PDMTaskTrigger(pDevIns->Internal.s.pGVM, PDMTASKTYPE_DEV, pDevIns->pDevInsForR3, hTask);

    LogFlow(("pdmR0DevHlp_TaskTrigger: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventSignal} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventSignal(PPDMDEVINS pDevIns, SUPSEMEVENT hEvent)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventSignal: caller='%s'/%d: hEvent=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, hEvent));

    int rc = SUPSemEventSignal(pDevIns->Internal.s.pGVM->pSession, hEvent);

    LogFlow(("pdmR0DevHlp_SUPSemEventSignal: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventWaitNoResume} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventWaitNoResume(PPDMDEVINS pDevIns, SUPSEMEVENT hEvent, uint32_t cMillies)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventWaitNoResume: caller='%s'/%d: hEvent=%p cNsTimeout=%RU32\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hEvent, cMillies));

    int rc = SUPSemEventWaitNoResume(pDevIns->Internal.s.pGVM->pSession, hEvent, cMillies);

    LogFlow(("pdmR0DevHlp_SUPSemEventWaitNoResume: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventWaitNsAbsIntr} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventWaitNsAbsIntr(PPDMDEVINS pDevIns, SUPSEMEVENT hEvent, uint64_t uNsTimeout)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventWaitNsAbsIntr: caller='%s'/%d: hEvent=%p uNsTimeout=%RU64\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hEvent, uNsTimeout));

    int rc = SUPSemEventWaitNsAbsIntr(pDevIns->Internal.s.pGVM->pSession, hEvent, uNsTimeout);

    LogFlow(("pdmR0DevHlp_SUPSemEventWaitNsAbsIntr: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventWaitNsRelIntr} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventWaitNsRelIntr(PPDMDEVINS pDevIns, SUPSEMEVENT hEvent, uint64_t cNsTimeout)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventWaitNsRelIntr: caller='%s'/%d: hEvent=%p cNsTimeout=%RU64\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hEvent, cNsTimeout));

    int rc = SUPSemEventWaitNsRelIntr(pDevIns->Internal.s.pGVM->pSession, hEvent, cNsTimeout);

    LogFlow(("pdmR0DevHlp_SUPSemEventWaitNsRelIntr: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventGetResolution} */
static DECLCALLBACK(uint32_t) pdmR0DevHlp_SUPSemEventGetResolution(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventGetResolution: caller='%s'/%d:\n", pDevIns->pReg->szName, pDevIns->iInstance));

    uint32_t cNsResolution = SUPSemEventGetResolution(pDevIns->Internal.s.pGVM->pSession);

    LogFlow(("pdmR0DevHlp_SUPSemEventGetResolution: caller='%s'/%d: returns %u\n", pDevIns->pReg->szName, pDevIns->iInstance, cNsResolution));
    return cNsResolution;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventMultiSignal} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventMultiSignal(PPDMDEVINS pDevIns, SUPSEMEVENTMULTI hEventMulti)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventMultiSignal: caller='%s'/%d: hEventMulti=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, hEventMulti));

    int rc = SUPSemEventMultiSignal(pDevIns->Internal.s.pGVM->pSession, hEventMulti);

    LogFlow(("pdmR0DevHlp_SUPSemEventMultiSignal: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventMultiReset} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventMultiReset(PPDMDEVINS pDevIns, SUPSEMEVENTMULTI hEventMulti)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventMultiReset: caller='%s'/%d: hEventMulti=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, hEventMulti));

    int rc = SUPSemEventMultiReset(pDevIns->Internal.s.pGVM->pSession, hEventMulti);

    LogFlow(("pdmR0DevHlp_SUPSemEventMultiReset: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventMultiWaitNoResume} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventMultiWaitNoResume(PPDMDEVINS pDevIns, SUPSEMEVENTMULTI hEventMulti,
                                                                  uint32_t cMillies)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventMultiWaitNoResume: caller='%s'/%d: hEventMulti=%p cMillies=%RU32\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hEventMulti, cMillies));

    int rc = SUPSemEventMultiWaitNoResume(pDevIns->Internal.s.pGVM->pSession, hEventMulti, cMillies);

    LogFlow(("pdmR0DevHlp_SUPSemEventMultiWaitNoResume: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventMultiWaitNsAbsIntr} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventMultiWaitNsAbsIntr(PPDMDEVINS pDevIns, SUPSEMEVENTMULTI hEventMulti,
                                                                   uint64_t uNsTimeout)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventMultiWaitNsAbsIntr: caller='%s'/%d: hEventMulti=%p uNsTimeout=%RU64\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hEventMulti, uNsTimeout));

    int rc = SUPSemEventMultiWaitNsAbsIntr(pDevIns->Internal.s.pGVM->pSession, hEventMulti, uNsTimeout);

    LogFlow(("pdmR0DevHlp_SUPSemEventMultiWaitNsAbsIntr: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventMultiWaitNsRelIntr} */
static DECLCALLBACK(int) pdmR0DevHlp_SUPSemEventMultiWaitNsRelIntr(PPDMDEVINS pDevIns, SUPSEMEVENTMULTI hEventMulti,
                                                                   uint64_t cNsTimeout)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventMultiWaitNsRelIntr: caller='%s'/%d: hEventMulti=%p cNsTimeout=%RU64\n",
             pDevIns->pReg->szName, pDevIns->iInstance, hEventMulti, cNsTimeout));

    int rc = SUPSemEventMultiWaitNsRelIntr(pDevIns->Internal.s.pGVM->pSession, hEventMulti, cNsTimeout);

    LogFlow(("pdmR0DevHlp_SUPSemEventMultiWaitNsRelIntr: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, rc));
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSUPSemEventMultiGetResolution} */
static DECLCALLBACK(uint32_t) pdmR0DevHlp_SUPSemEventMultiGetResolution(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_SUPSemEventMultiGetResolution: caller='%s'/%d:\n", pDevIns->pReg->szName, pDevIns->iInstance));

    uint32_t cNsResolution = SUPSemEventMultiGetResolution(pDevIns->Internal.s.pGVM->pSession);

    LogFlow(("pdmR0DevHlp_SUPSemEventMultiGetResolution: caller='%s'/%d: returns %u\n", pDevIns->pReg->szName, pDevIns->iInstance, cNsResolution));
    return cNsResolution;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectGetNop} */
static DECLCALLBACK(PPDMCRITSECT) pdmR0DevHlp_CritSectGetNop(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    PPDMCRITSECT pCritSect = &pGVM->pdm.s.NopCritSect;
    LogFlow(("pdmR0DevHlp_CritSectGetNop: caller='%s'/%d: return %p\n", pDevIns->pReg->szName, pDevIns->iInstance, pCritSect));
    return pCritSect;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnSetDeviceCritSect} */
static DECLCALLBACK(int) pdmR0DevHlp_SetDeviceCritSect(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect)
{
    /*
     * Validate input.
     *
     * Note! We only allow the automatically created default critical section
     *       to be replaced by this API.
     */
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertPtrReturn(pCritSect, VERR_INVALID_POINTER);
    LogFlow(("pdmR0DevHlp_SetDeviceCritSect: caller='%s'/%d: pCritSect=%p (%s)\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pCritSect, pCritSect->s.pszName));
    AssertReturn(PDMCritSectIsInitialized(pCritSect), VERR_INVALID_PARAMETER);
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    AssertReturn(pCritSect->s.pVMR0 == pGVM, VERR_INVALID_PARAMETER);

    VM_ASSERT_EMT(pGVM);
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);

    /*
     * Check that ring-3 has already done this, then effect the change.
     */
    AssertReturn(pDevIns->pDevInsForR3R0->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_CHANGED_CRITSECT, VERR_WRONG_ORDER);
    pDevIns->pCritSectRoR0 = pCritSect;

    LogFlow(("pdmR0DevHlp_SetDeviceCritSect: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectEnter} */
static DECLCALLBACK(int)      pdmR0DevHlp_CritSectEnter(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, int rcBusy)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns); /** @todo pass pDevIns->Internal.s.pGVM to the crit sect code.   */
    return PDMCritSectEnter(pCritSect, rcBusy);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectEnterDebug} */
static DECLCALLBACK(int)      pdmR0DevHlp_CritSectEnterDebug(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, int rcBusy, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns); /** @todo pass pDevIns->Internal.s.pGVM to the crit sect code.   */
    return PDMCritSectEnterDebug(pCritSect, rcBusy, uId, RT_SRC_POS_ARGS);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectTryEnter} */
static DECLCALLBACK(int)      pdmR0DevHlp_CritSectTryEnter(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns); /** @todo pass pDevIns->Internal.s.pGVM to the crit sect code.   */
    return PDMCritSectTryEnter(pCritSect);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectTryEnterDebug} */
static DECLCALLBACK(int)      pdmR0DevHlp_CritSectTryEnterDebug(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns); /** @todo pass pDevIns->Internal.s.pGVM to the crit sect code.   */
    return PDMCritSectTryEnterDebug(pCritSect, uId, RT_SRC_POS_ARGS);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectLeave} */
static DECLCALLBACK(int)      pdmR0DevHlp_CritSectLeave(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns); /** @todo pass pDevIns->Internal.s.pGVM to the crit sect code.   */
    return PDMCritSectLeave(pCritSect);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectIsOwner} */
static DECLCALLBACK(bool)     pdmR0DevHlp_CritSectIsOwner(PPDMDEVINS pDevIns, PCPDMCRITSECT pCritSect)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns); /** @todo pass pDevIns->Internal.s.pGVM to the crit sect code.   */
    return PDMCritSectIsOwner(pCritSect);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectIsInitialized} */
static DECLCALLBACK(bool)     pdmR0DevHlp_CritSectIsInitialized(PPDMDEVINS pDevIns, PCPDMCRITSECT pCritSect)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return PDMCritSectIsInitialized(pCritSect);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectHasWaiters} */
static DECLCALLBACK(bool)     pdmR0DevHlp_CritSectHasWaiters(PPDMDEVINS pDevIns, PCPDMCRITSECT pCritSect)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return PDMCritSectHasWaiters(pCritSect);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectGetRecursion} */
static DECLCALLBACK(uint32_t) pdmR0DevHlp_CritSectGetRecursion(PPDMDEVINS pDevIns, PCPDMCRITSECT pCritSect)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return PDMCritSectGetRecursion(pCritSect);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectScheduleExitEvent} */
static DECLCALLBACK(int) pdmR0DevHlp_CritSectScheduleExitEvent(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect,
                                                               SUPSEMEVENT hEventToSignal)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return PDMHCCritSectScheduleExitEvent(pCritSect, hEventToSignal);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnDBGFTraceBuf} */
static DECLCALLBACK(RTTRACEBUF) pdmR0DevHlp_DBGFTraceBuf(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RTTRACEBUF hTraceBuf = pDevIns->Internal.s.pGVM->hTraceBufR0;
    LogFlow(("pdmR0DevHlp_DBGFTraceBuf: caller='%p'/%d: returns %p\n", pDevIns, pDevIns->iInstance, hTraceBuf));
    return hTraceBuf;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCIBusSetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_PCIBusSetUpContext(PPDMDEVINS pDevIns, PPDMPCIBUSREGR0 pPciBusReg, PCPDMPCIHLPR0 *ppPciHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PCIBusSetUpContext: caller='%p'/%d: pPciBusReg=%p{.u32Version=%#x, .iBus=%#u, .pfnSetIrq=%p, u32EnvVersion=%#x} ppPciHlp=%p\n",
             pDevIns, pDevIns->iInstance, pPciBusReg, pPciBusReg->u32Version, pPciBusReg->iBus, pPciBusReg->pfnSetIrq,
             pPciBusReg->u32EndVersion, ppPciHlp));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    /*
     * Validate input.
     */
    AssertPtrReturn(pPciBusReg, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(pPciBusReg->u32Version == PDM_PCIBUSREGCC_VERSION,
                          ("%#x vs %#x\n", pPciBusReg->u32Version, PDM_PCIBUSREGCC_VERSION), VERR_VERSION_MISMATCH);
    AssertPtrReturn(pPciBusReg->pfnSetIrq, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(pPciBusReg->u32EndVersion == PDM_PCIBUSREGCC_VERSION,
                          ("%#x vs %#x\n", pPciBusReg->u32EndVersion, PDM_PCIBUSREGCC_VERSION), VERR_VERSION_MISMATCH);

    AssertPtrReturn(ppPciHlp, VERR_INVALID_POINTER);

    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);

    /* Check the shared bus data (registered earlier from ring-3): */
    uint32_t iBus = pPciBusReg->iBus;
    ASMCompilerBarrier();
    AssertLogRelMsgReturn(iBus < RT_ELEMENTS(pGVM->pdm.s.aPciBuses), ("iBus=%#x\n", iBus), VERR_OUT_OF_RANGE);
    PPDMPCIBUS pPciBusShared = &pGVM->pdm.s.aPciBuses[iBus];
    AssertLogRelMsgReturn(pPciBusShared->iBus == iBus, ("%u vs %u\n", pPciBusShared->iBus, iBus), VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pPciBusShared->pDevInsR3 == pDevIns->pDevInsForR3,
                          ("%p vs %p (iBus=%u)\n", pPciBusShared->pDevInsR3, pDevIns->pDevInsForR3, iBus), VERR_NOT_OWNER);

    /* Check that the bus isn't already registered in ring-0: */
    AssertCompile(RT_ELEMENTS(pGVM->pdm.s.aPciBuses) == RT_ELEMENTS(pGVM->pdmr0.s.aPciBuses));
    PPDMPCIBUSR0 pPciBusR0 = &pGVM->pdmr0.s.aPciBuses[iBus];
    AssertLogRelMsgReturn(pPciBusR0->pDevInsR0 == NULL,
                          ("%p (caller pDevIns=%p, iBus=%u)\n", pPciBusR0->pDevInsR0, pDevIns, iBus),
                          VERR_ALREADY_EXISTS);

    /*
     * Do the registering.
     */
    pPciBusR0->iBus        = iBus;
    pPciBusR0->uPadding0   = 0xbeefbeef;
    pPciBusR0->pfnSetIrqR0 = pPciBusReg->pfnSetIrq;
    pPciBusR0->pDevInsR0   = pDevIns;

    *ppPciHlp = &g_pdmR0PciHlp;

    LogFlow(("pdmR0DevHlp_PCIBusSetUpContext: caller='%p'/%d: returns VINF_SUCCESS\n", pDevIns, pDevIns->iInstance));
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnIommuSetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_IommuSetUpContext(PPDMDEVINS pDevIns, PPDMIOMMUREGR0 pIommuReg, PCPDMIOMMUHLPR0 *ppIommuHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_IommuSetUpContext: caller='%p'/%d: pIommuReg=%p{.u32Version=%#x, u32TheEnd=%#x} ppIommuHlp=%p\n",
             pDevIns, pDevIns->iInstance, pIommuReg, pIommuReg->u32Version, pIommuReg->u32TheEnd, ppIommuHlp));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    /*
     * Validate input.
     */
    AssertPtrReturn(pIommuReg, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(pIommuReg->u32Version == PDM_IOMMUREGCC_VERSION,
                          ("%#x vs %#x\n", pIommuReg->u32Version, PDM_IOMMUREGCC_VERSION), VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pIommuReg->u32TheEnd == PDM_IOMMUREGCC_VERSION,
                          ("%#x vs %#x\n", pIommuReg->u32TheEnd, PDM_IOMMUREGCC_VERSION), VERR_VERSION_MISMATCH);

    AssertPtrReturn(ppIommuHlp, VERR_INVALID_POINTER);

    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);

    /* Check the IOMMU shared data (registered earlier from ring-3). */
    uint32_t const idxIommu = pIommuReg->idxIommu;
    ASMCompilerBarrier();
    AssertLogRelMsgReturn(idxIommu < RT_ELEMENTS(pGVM->pdm.s.aIommus), ("idxIommu=%#x\n", idxIommu), VERR_OUT_OF_RANGE);
    PPDMIOMMU pIommuShared = &pGVM->pdm.s.aIommus[idxIommu];
    AssertLogRelMsgReturn(pIommuShared->idxIommu == idxIommu, ("%u vs %u\n", pIommuShared->idxIommu, idxIommu), VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pIommuShared->pDevInsR3 == pDevIns->pDevInsForR3,
                          ("%p vs %p (idxIommu=%u)\n", pIommuShared->pDevInsR3, pDevIns->pDevInsForR3, idxIommu), VERR_NOT_OWNER);

    /* Check that the IOMMU isn't already registered in ring-0. */
    AssertCompile(RT_ELEMENTS(pGVM->pdm.s.aIommus) == RT_ELEMENTS(pGVM->pdmr0.s.aIommus));
    PPDMIOMMUR0 pIommuR0 = &pGVM->pdmr0.s.aIommus[idxIommu];
    AssertLogRelMsgReturn(pIommuR0->pDevInsR0 == NULL,
                          ("%p (caller pDevIns=%p, idxIommu=%u)\n", pIommuR0->pDevInsR0, pDevIns, idxIommu),
                          VERR_ALREADY_EXISTS);

    /*
     * Register.
     */
    pIommuR0->idxIommu    = idxIommu;
    pIommuR0->uPadding0   = 0xdeaddead;
    pIommuR0->pDevInsR0   = pDevIns;

    *ppIommuHlp = &g_pdmR0IommuHlp;

    LogFlow(("pdmR0DevHlp_IommuSetUpContext: caller='%p'/%d: returns VINF_SUCCESS\n", pDevIns, pDevIns->iInstance));
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPICSetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_PICSetUpContext(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLP *ppPicHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PICSetUpContext: caller='%s'/%d: pPicReg=%p:{.u32Version=%#x, .pfnSetIrq=%p, .pfnGetInterrupt=%p, .u32TheEnd=%#x } ppPicHlp=%p\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pPicReg, pPicReg->u32Version, pPicReg->pfnSetIrq, pPicReg->pfnGetInterrupt, pPicReg->u32TheEnd, ppPicHlp));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    /*
     * Validate input.
     */
    AssertMsgReturn(pPicReg->u32Version == PDM_PICREG_VERSION,
                    ("%s/%d: u32Version=%#x expected %#x\n", pDevIns->pReg->szName, pDevIns->iInstance, pPicReg->u32Version, PDM_PICREG_VERSION),
                    VERR_VERSION_MISMATCH);
    AssertPtrReturn(pPicReg->pfnSetIrq, VERR_INVALID_POINTER);
    AssertPtrReturn(pPicReg->pfnGetInterrupt, VERR_INVALID_POINTER);
    AssertMsgReturn(pPicReg->u32TheEnd == PDM_PICREG_VERSION,
                    ("%s/%d: u32TheEnd=%#x expected %#x\n", pDevIns->pReg->szName, pDevIns->iInstance, pPicReg->u32TheEnd, PDM_PICREG_VERSION),
                    VERR_VERSION_MISMATCH);
    AssertPtrReturn(ppPicHlp, VERR_INVALID_POINTER);

    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);

    /* Check that it's the same device as made the ring-3 registrations: */
    AssertLogRelMsgReturn(pGVM->pdm.s.Pic.pDevInsR3 == pDevIns->pDevInsForR3,
                          ("%p vs %p\n", pGVM->pdm.s.Pic.pDevInsR3, pDevIns->pDevInsForR3), VERR_NOT_OWNER);

    /* Check that it isn't already registered in ring-0: */
    AssertLogRelMsgReturn(pGVM->pdm.s.Pic.pDevInsR0 == NULL, ("%p (caller pDevIns=%p)\n", pGVM->pdm.s.Pic.pDevInsR0, pDevIns),
                          VERR_ALREADY_EXISTS);

    /*
     * Take down the callbacks and instance.
     */
    pGVM->pdm.s.Pic.pDevInsR0 = pDevIns;
    pGVM->pdm.s.Pic.pfnSetIrqR0 = pPicReg->pfnSetIrq;
    pGVM->pdm.s.Pic.pfnGetInterruptR0 = pPicReg->pfnGetInterrupt;
    Log(("PDM: Registered PIC device '%s'/%d pDevIns=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppPicHlp = &g_pdmR0PicHlp;
    LogFlow(("pdmR0DevHlp_PICSetUpContext: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnApicSetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_ApicSetUpContext(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_ApicSetUpContext: caller='%s'/%d:\n", pDevIns->pReg->szName, pDevIns->iInstance));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    /*
     * Validate input.
     */
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);

    /* Check that it's the same device as made the ring-3 registrations: */
    AssertLogRelMsgReturn(pGVM->pdm.s.Apic.pDevInsR3 == pDevIns->pDevInsForR3,
                          ("%p vs %p\n", pGVM->pdm.s.Apic.pDevInsR3, pDevIns->pDevInsForR3), VERR_NOT_OWNER);

    /* Check that it isn't already registered in ring-0: */
    AssertLogRelMsgReturn(pGVM->pdm.s.Apic.pDevInsR0 == NULL, ("%p (caller pDevIns=%p)\n", pGVM->pdm.s.Apic.pDevInsR0, pDevIns),
                          VERR_ALREADY_EXISTS);

    /*
     * Take down the instance.
     */
    pGVM->pdm.s.Apic.pDevInsR0 = pDevIns;
    Log(("PDM: Registered APIC device '%s'/%d pDevIns=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    LogFlow(("pdmR0DevHlp_ApicSetUpContext: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnIoApicSetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_IoApicSetUpContext(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLP *ppIoApicHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_IoApicSetUpContext: caller='%s'/%d: pIoApicReg=%p:{.u32Version=%#x, .pfnSetIrq=%p, .pfnSendMsi=%p, .pfnSetEoi=%p, .u32TheEnd=%#x } ppIoApicHlp=%p\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pIoApicReg, pIoApicReg->u32Version, pIoApicReg->pfnSetIrq, pIoApicReg->pfnSendMsi, pIoApicReg->pfnSetEoi, pIoApicReg->u32TheEnd, ppIoApicHlp));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    /*
     * Validate input.
     */
    AssertMsgReturn(pIoApicReg->u32Version == PDM_IOAPICREG_VERSION,
                    ("%s/%d: u32Version=%#x expected %#x\n", pDevIns->pReg->szName, pDevIns->iInstance, pIoApicReg->u32Version, PDM_IOAPICREG_VERSION),
                    VERR_VERSION_MISMATCH);
    AssertPtrReturn(pIoApicReg->pfnSetIrq, VERR_INVALID_POINTER);
    AssertPtrReturn(pIoApicReg->pfnSendMsi, VERR_INVALID_POINTER);
    AssertPtrReturn(pIoApicReg->pfnSetEoi, VERR_INVALID_POINTER);
    AssertMsgReturn(pIoApicReg->u32TheEnd == PDM_IOAPICREG_VERSION,
                    ("%s/%d: u32TheEnd=%#x expected %#x\n", pDevIns->pReg->szName, pDevIns->iInstance, pIoApicReg->u32TheEnd, PDM_IOAPICREG_VERSION),
                    VERR_VERSION_MISMATCH);
    AssertPtrReturn(ppIoApicHlp, VERR_INVALID_POINTER);

    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);

    /* Check that it's the same device as made the ring-3 registrations: */
    AssertLogRelMsgReturn(pGVM->pdm.s.IoApic.pDevInsR3 == pDevIns->pDevInsForR3,
                          ("%p vs %p\n", pGVM->pdm.s.IoApic.pDevInsR3, pDevIns->pDevInsForR3), VERR_NOT_OWNER);

    /* Check that it isn't already registered in ring-0: */
    AssertLogRelMsgReturn(pGVM->pdm.s.IoApic.pDevInsR0 == NULL, ("%p (caller pDevIns=%p)\n", pGVM->pdm.s.IoApic.pDevInsR0, pDevIns),
                          VERR_ALREADY_EXISTS);

    /*
     * Take down the callbacks and instance.
     */
    pGVM->pdm.s.IoApic.pDevInsR0    = pDevIns;
    pGVM->pdm.s.IoApic.pfnSetIrqR0  = pIoApicReg->pfnSetIrq;
    pGVM->pdm.s.IoApic.pfnSendMsiR0 = pIoApicReg->pfnSendMsi;
    pGVM->pdm.s.IoApic.pfnSetEoiR0  = pIoApicReg->pfnSetEoi;
    Log(("PDM: Registered IOAPIC device '%s'/%d pDevIns=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppIoApicHlp = &g_pdmR0IoApicHlp;
    LogFlow(("pdmR0DevHlp_IoApicSetUpContext: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnHpetSetUpContext} */
static DECLCALLBACK(int) pdmR0DevHlp_HpetSetUpContext(PPDMDEVINS pDevIns, PPDMHPETREG pHpetReg, PCPDMHPETHLPR0 *ppHpetHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_HpetSetUpContext: caller='%s'/%d: pHpetReg=%p:{.u32Version=%#x, } ppHpetHlp=%p\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pHpetReg, pHpetReg->u32Version, ppHpetHlp));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    /*
     * Validate input.
     */
    AssertMsgReturn(pHpetReg->u32Version == PDM_HPETREG_VERSION,
                    ("%s/%d: u32Version=%#x expected %#x\n", pDevIns->pReg->szName, pDevIns->iInstance, pHpetReg->u32Version, PDM_HPETREG_VERSION),
                    VERR_VERSION_MISMATCH);
    AssertPtrReturn(ppHpetHlp, VERR_INVALID_POINTER);

    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);
    VM_ASSERT_EMT0_RETURN(pGVM, VERR_VM_THREAD_NOT_EMT);

    /* Check that it's the same device as made the ring-3 registrations: */
    AssertLogRelMsgReturn(pGVM->pdm.s.pHpet == pDevIns->pDevInsForR3, ("%p vs %p\n", pGVM->pdm.s.pHpet, pDevIns->pDevInsForR3),
                          VERR_NOT_OWNER);

    ///* Check that it isn't already registered in ring-0: */
    //AssertLogRelMsgReturn(pGVM->pdm.s.Hpet.pDevInsR0 == NULL, ("%p (caller pDevIns=%p)\n", pGVM->pdm.s.Hpet.pDevInsR0, pDevIns),
    //                      VERR_ALREADY_EXISTS);

    /*
     * Nothing to take down here at present.
     */
    Log(("PDM: Registered HPET device '%s'/%d pDevIns=%p\n", pDevIns->pReg->szName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppHpetHlp = &g_pdmR0HpetHlp;
    LogFlow(("pdmR0DevHlp_HpetSetUpContext: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/**
 * The Ring-0 Device Helper Callbacks.
 */
extern DECLEXPORT(const PDMDEVHLPR0) g_pdmR0DevHlp =
{
    PDM_DEVHLPR0_VERSION,
    pdmR0DevHlp_IoPortSetUpContextEx,
    pdmR0DevHlp_MmioSetUpContextEx,
    pdmR0DevHlp_Mmio2SetUpContext,
    pdmR0DevHlp_PCIPhysRead,
    pdmR0DevHlp_PCIPhysWrite,
    pdmR0DevHlp_PCISetIrq,
    pdmR0DevHlp_ISASetIrq,
    pdmR0DevHlp_IoApicSendMsi,
    pdmR0DevHlp_PhysRead,
    pdmR0DevHlp_PhysWrite,
    pdmR0DevHlp_A20IsEnabled,
    pdmR0DevHlp_VMState,
    pdmR0DevHlp_VMSetError,
    pdmR0DevHlp_VMSetErrorV,
    pdmR0DevHlp_VMSetRuntimeError,
    pdmR0DevHlp_VMSetRuntimeErrorV,
    pdmR0DevHlp_GetVM,
    pdmR0DevHlp_GetVMCPU,
    pdmR0DevHlp_GetCurrentCpuId,
    pdmR0DevHlp_TimerToPtr,
    pdmR0DevHlp_TimerFromMicro,
    pdmR0DevHlp_TimerFromMilli,
    pdmR0DevHlp_TimerFromNano,
    pdmR0DevHlp_TimerGet,
    pdmR0DevHlp_TimerGetFreq,
    pdmR0DevHlp_TimerGetNano,
    pdmR0DevHlp_TimerIsActive,
    pdmR0DevHlp_TimerIsLockOwner,
    pdmR0DevHlp_TimerLockClock,
    pdmR0DevHlp_TimerLockClock2,
    pdmR0DevHlp_TimerSet,
    pdmR0DevHlp_TimerSetFrequencyHint,
    pdmR0DevHlp_TimerSetMicro,
    pdmR0DevHlp_TimerSetMillies,
    pdmR0DevHlp_TimerSetNano,
    pdmR0DevHlp_TimerSetRelative,
    pdmR0DevHlp_TimerStop,
    pdmR0DevHlp_TimerUnlockClock,
    pdmR0DevHlp_TimerUnlockClock2,
    pdmR0DevHlp_TMTimeVirtGet,
    pdmR0DevHlp_TMTimeVirtGetFreq,
    pdmR0DevHlp_TMTimeVirtGetNano,
    pdmR0DevHlp_QueueToPtr,
    pdmR0DevHlp_QueueAlloc,
    pdmR0DevHlp_QueueInsert,
    pdmR0DevHlp_QueueInsertEx,
    pdmR0DevHlp_QueueFlushIfNecessary,
    pdmR0DevHlp_TaskTrigger,
    pdmR0DevHlp_SUPSemEventSignal,
    pdmR0DevHlp_SUPSemEventWaitNoResume,
    pdmR0DevHlp_SUPSemEventWaitNsAbsIntr,
    pdmR0DevHlp_SUPSemEventWaitNsRelIntr,
    pdmR0DevHlp_SUPSemEventGetResolution,
    pdmR0DevHlp_SUPSemEventMultiSignal,
    pdmR0DevHlp_SUPSemEventMultiReset,
    pdmR0DevHlp_SUPSemEventMultiWaitNoResume,
    pdmR0DevHlp_SUPSemEventMultiWaitNsAbsIntr,
    pdmR0DevHlp_SUPSemEventMultiWaitNsRelIntr,
    pdmR0DevHlp_SUPSemEventMultiGetResolution,
    pdmR0DevHlp_CritSectGetNop,
    pdmR0DevHlp_SetDeviceCritSect,
    pdmR0DevHlp_CritSectEnter,
    pdmR0DevHlp_CritSectEnterDebug,
    pdmR0DevHlp_CritSectTryEnter,
    pdmR0DevHlp_CritSectTryEnterDebug,
    pdmR0DevHlp_CritSectLeave,
    pdmR0DevHlp_CritSectIsOwner,
    pdmR0DevHlp_CritSectIsInitialized,
    pdmR0DevHlp_CritSectHasWaiters,
    pdmR0DevHlp_CritSectGetRecursion,
    pdmR0DevHlp_CritSectScheduleExitEvent,
    pdmR0DevHlp_DBGFTraceBuf,
    pdmR0DevHlp_PCIBusSetUpContext,
    pdmR0DevHlp_IommuSetUpContext,
    pdmR0DevHlp_PICSetUpContext,
    pdmR0DevHlp_ApicSetUpContext,
    pdmR0DevHlp_IoApicSetUpContext,
    pdmR0DevHlp_HpetSetUpContext,
    NULL /*pfnReserved1*/,
    NULL /*pfnReserved2*/,
    NULL /*pfnReserved3*/,
    NULL /*pfnReserved4*/,
    NULL /*pfnReserved5*/,
    NULL /*pfnReserved6*/,
    NULL /*pfnReserved7*/,
    NULL /*pfnReserved8*/,
    NULL /*pfnReserved9*/,
    NULL /*pfnReserved10*/,
    PDM_DEVHLPR0_VERSION
};

/** @} */




/** @name PIC Ring-0 Helpers
 * @{
 */

/** @interface_method_impl{PDMPICHLP,pfnSetInterruptFF} */
static DECLCALLBACK(void) pdmR0PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM     pGVM  = (PGVM)pDevIns->Internal.s.pGVM;
    PVMCPUCC pVCpu = &pGVM->aCpus[0];     /* for PIC we always deliver to CPU 0, MP use APIC */
    /** @todo r=ramshankar: Propagating rcRZ and make all callers handle it? */
    APICLocalInterrupt(pVCpu, 0 /* u8Pin */, 1 /* u8Level */, VINF_SUCCESS /* rcRZ */);
}


/** @interface_method_impl{PDMPICHLP,pfnClearInterruptFF} */
static DECLCALLBACK(void) pdmR0PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM     pGVM  = (PGVM)pDevIns->Internal.s.pGVM;
    PVMCPUCC pVCpu = &pGVM->aCpus[0];     /* for PIC we always deliver to CPU 0, MP use APIC */
    /** @todo r=ramshankar: Propagating rcRZ and make all callers handle it? */
    APICLocalInterrupt(pVCpu, 0 /* u8Pin */, 0 /* u8Level */, VINF_SUCCESS /* rcRZ */);
}


/** @interface_method_impl{PDMPICHLP,pfnLock} */
static DECLCALLBACK(int) pdmR0PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pGVM, rc);
}


/** @interface_method_impl{PDMPICHLP,pfnUnlock} */
static DECLCALLBACK(void) pdmR0PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pGVM);
}


/**
 * The Ring-0 PIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMPICHLP) g_pdmR0PicHlp =
{
    PDM_PICHLP_VERSION,
    pdmR0PicHlp_SetInterruptFF,
    pdmR0PicHlp_ClearInterruptFF,
    pdmR0PicHlp_Lock,
    pdmR0PicHlp_Unlock,
    PDM_PICHLP_VERSION
};

/** @} */


/** @name I/O APIC Ring-0 Helpers
 * @{
 */

/** @interface_method_impl{PDMIOAPICHLP,pfnApicBusDeliver} */
static DECLCALLBACK(int) pdmR0IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode,
                                                       uint8_t u8DeliveryMode, uint8_t uVector, uint8_t u8Polarity,
                                                       uint8_t u8TriggerMode, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    LogFlow(("pdmR0IoApicHlp_ApicBusDeliver: caller=%p/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 uVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8 uTagSrc=%#x\n",
             pDevIns, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, uVector, u8Polarity, u8TriggerMode, uTagSrc));
    return APICBusDeliver(pGVM, u8Dest, u8DestMode, u8DeliveryMode, uVector, u8Polarity, u8TriggerMode, uTagSrc);
}


/** @interface_method_impl{PDMIOAPICHLP,pfnLock} */
static DECLCALLBACK(int) pdmR0IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pGVM, rc);
}


/** @interface_method_impl{PDMIOAPICHLP,pfnUnlock} */
static DECLCALLBACK(void) pdmR0IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pGVM);
}


/**
 * The Ring-0 I/O APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOAPICHLP) g_pdmR0IoApicHlp =
{
    PDM_IOAPICHLP_VERSION,
    pdmR0IoApicHlp_ApicBusDeliver,
    pdmR0IoApicHlp_Lock,
    pdmR0IoApicHlp_Unlock,
    PDM_IOAPICHLP_VERSION
};

/** @} */




/** @name PCI Bus Ring-0 Helpers
 * @{
 */

/** @interface_method_impl{PDMPCIHLPR0,pfnIsaSetIrq} */
static DECLCALLBACK(void) pdmR0PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IsaSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    pdmLock(pGVM);
    pdmR0IsaSetIrq(pGVM, iIrq, iLevel, uTagSrc);
    pdmUnlock(pGVM);
}


/** @interface_method_impl{PDMPCIHLPR0,pfnIoApicSetIrq} */
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IoApicSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    if (pGVM->pdm.s.IoApic.pDevInsR0)
        pGVM->pdm.s.IoApic.pfnSetIrqR0(pGVM->pdm.s.IoApic.pDevInsR0, iIrq, iLevel, uTagSrc);
    else if (pGVM->pdm.s.IoApic.pDevInsR3)
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pGVM->pdm.s.pDevHlpQueueR0);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_IOAPIC_SET_IRQ;
            pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
            pTask->u.IoApicSetIRQ.iIrq = iIrq;
            pTask->u.IoApicSetIRQ.iLevel = iLevel;
            pTask->u.IoApicSetIRQ.uTagSrc = uTagSrc;

            PDMQueueInsertEx(pGVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}


/** @interface_method_impl{PDMPCIHLPR0,pfnIoApicSendMsi} */
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IoApicSendMsi: GCPhys=%p uValue=%d uTagSrc=%#x\n", GCPhys, uValue, uTagSrc));
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    if (pGVM->pdm.s.IoApic.pDevInsR0)
        pGVM->pdm.s.IoApic.pfnSendMsiR0(pGVM->pdm.s.IoApic.pDevInsR0, GCPhys, uValue, uTagSrc);
    else
        AssertFatalMsgFailed(("Lazy bastards!"));
}


/** @interface_method_impl{PDMPCIHLPR0,pfnLock} */
static DECLCALLBACK(int) pdmR0PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pGVM, rc);
}


/** @interface_method_impl{PDMPCIHLPR0,pfnUnlock} */
static DECLCALLBACK(void) pdmR0PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMPCIHLPR0,pfnGetBusByNo} */
static DECLCALLBACK(PPDMDEVINS) pdmR0PciHlp_GetBusByNo(PPDMDEVINS pDevIns, uint32_t idxPdmBus)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    AssertReturn(idxPdmBus < RT_ELEMENTS(pGVM->pdmr0.s.aPciBuses), NULL);
    PPDMDEVINS pRetDevIns = pGVM->pdmr0.s.aPciBuses[idxPdmBus].pDevInsR0;
    LogFlow(("pdmR3PciHlp_GetBusByNo: caller='%s'/%d: returns %p\n", pDevIns->pReg->szName, pDevIns->iInstance, pRetDevIns));
    return pRetDevIns;
}


/**
 * The Ring-0 PCI Bus Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIHLPR0) g_pdmR0PciHlp =
{
    PDM_PCIHLPR0_VERSION,
    pdmR0PciHlp_IsaSetIrq,
    pdmR0PciHlp_IoApicSetIrq,
    pdmR0PciHlp_IoApicSendMsi,
    pdmR0PciHlp_Lock,
    pdmR0PciHlp_Unlock,
    pdmR0PciHlp_GetBusByNo,
    PDM_PCIHLPR0_VERSION, /* the end */
};

/** @} */


/** @name IOMMU Ring-0 Helpers
 * @{
 */

/**
 * The Ring-0 IOMMU Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOMMUHLPR0) g_pdmR0IommuHlp =
{
    PDM_IOMMUHLPR0_VERSION,
    PDM_IOMMUHLPR0_VERSION, /* the end */
};

/** @} */


/** @name HPET Ring-0 Helpers
 * @{
 */
/* none */

/**
 * The Ring-0 HPET Helper Callbacks.
 */
extern DECLEXPORT(const PDMHPETHLPR0) g_pdmR0HpetHlp =
{
    PDM_HPETHLPR0_VERSION,
    PDM_HPETHLPR0_VERSION, /* the end */
};

/** @} */


/** @name Raw PCI Ring-0 Helpers
 * @{
 */
/* none */

/**
 * The Ring-0 PCI raw Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIRAWHLPR0) g_pdmR0PciRawHlp =
{
    PDM_PCIRAWHLPR0_VERSION,
    PDM_PCIRAWHLPR0_VERSION, /* the end */
};

/** @} */


/** @name Ring-0 Context Driver Helpers
 * @{
 */

/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetError} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetError(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetErrorV} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetErrorV(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetRuntimeError} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetRuntimeError(PPDMDRVINS pDrvIns, uint32_t fFlags, const char *pszErrorId,
                                                       const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetRuntimeErrorV} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetRuntimeErrorV(PPDMDRVINS pDrvIns, uint32_t fFlags, const char *pszErrorId,
                                                        const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnAssertEMT} */
static DECLCALLBACK(bool) pdmR0DrvHlp_AssertEMT(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (VM_IS_EMT(pDrvIns->Internal.s.pVMR0))
        return true;

    RTAssertMsg1Weak("AssertEMT", iLine, pszFile, pszFunction);
    RTAssertPanic();
    return false;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnAssertOther} */
static DECLCALLBACK(bool) pdmR0DrvHlp_AssertOther(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (!VM_IS_EMT(pDrvIns->Internal.s.pVMR0))
        return true;

    RTAssertMsg1Weak("AssertOther", iLine, pszFile, pszFunction);
    RTAssertPanic();
    return false;
}


/**
 * The Ring-0 Context Driver Helper Callbacks.
 */
extern DECLEXPORT(const PDMDRVHLPR0) g_pdmR0DrvHlp =
{
    PDM_DRVHLPRC_VERSION,
    pdmR0DrvHlp_VMSetError,
    pdmR0DrvHlp_VMSetErrorV,
    pdmR0DrvHlp_VMSetRuntimeError,
    pdmR0DrvHlp_VMSetRuntimeErrorV,
    pdmR0DrvHlp_AssertEMT,
    pdmR0DrvHlp_AssertOther,
    PDM_DRVHLPRC_VERSION
};

/** @} */




/**
 * Sets an irq on the PIC and I/O APIC.
 *
 * @returns true if delivered, false if postponed.
 * @param   pGVM        The global (ring-0) VM structure.
 * @param   iIrq        The irq.
 * @param   iLevel      The new level.
 * @param   uTagSrc     The IRQ tag and source.
 *
 * @remarks The caller holds the PDM lock.
 */
static bool pdmR0IsaSetIrq(PGVM pGVM, int iIrq, int iLevel, uint32_t uTagSrc)
{
    if (RT_LIKELY(    (   pGVM->pdm.s.IoApic.pDevInsR0
                       || !pGVM->pdm.s.IoApic.pDevInsR3)
                  &&  (   pGVM->pdm.s.Pic.pDevInsR0
                       || !pGVM->pdm.s.Pic.pDevInsR3)))
    {
        if (pGVM->pdm.s.Pic.pDevInsR0)
            pGVM->pdm.s.Pic.pfnSetIrqR0(pGVM->pdm.s.Pic.pDevInsR0, iIrq, iLevel, uTagSrc);
        if (pGVM->pdm.s.IoApic.pDevInsR0)
            pGVM->pdm.s.IoApic.pfnSetIrqR0(pGVM->pdm.s.IoApic.pDevInsR0, iIrq, iLevel, uTagSrc);
        return true;
    }

    /* queue for ring-3 execution. */
    PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pGVM->pdm.s.pDevHlpQueueR0);
    AssertReturn(pTask, false);

    pTask->enmOp = PDMDEVHLPTASKOP_ISA_SET_IRQ;
    pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
    pTask->u.IsaSetIRQ.iIrq = iIrq;
    pTask->u.IsaSetIRQ.iLevel = iLevel;
    pTask->u.IsaSetIRQ.uTagSrc = uTagSrc;

    PDMQueueInsertEx(pGVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
    return false;
}


/**
 * Worker for PDMR0DeviceCreate that does the actual instantiation.
 *
 * Allocates a memory object and divides it up as follows:
 * @verbatim
   --------------------------------------
   ring-0 devins
   --------------------------------------
   ring-0 instance data
   --------------------------------------
   ring-0 PCI device data (optional) ??
   --------------------------------------
   page alignment padding
   --------------------------------------
   ring-3 devins
   --------------------------------------
   ring-3 instance data
   --------------------------------------
   ring-3 PCI device data (optional) ??
   --------------------------------------
  [page alignment padding                ] -
  [--------------------------------------]  \
  [raw-mode devins                       ]   \
  [--------------------------------------]   - Optional, only when raw-mode is enabled.
  [raw-mode instance data                ]   /
  [--------------------------------------]  /
  [raw-mode PCI device data (optional)?? ] -
   --------------------------------------
   shared instance data
   --------------------------------------
   default crit section
   --------------------------------------
   shared PCI device data (optional)
   --------------------------------------
   @endverbatim
 *
 * @returns VBox status code.
 * @param   pGVM            The global (ring-0) VM structure.
 * @param   pDevReg         The device registration structure.
 * @param   iInstance       The device instance number.
 * @param   cbInstanceR3    The size of the ring-3 instance data.
 * @param   cbInstanceRC    The size of the raw-mode instance data.
 * @param   hMod            The module implementing the device.  On success, the
 * @param   RCPtrMapping    The raw-mode context mapping address, NIL_RTGCPTR if
 *                          not to include raw-mode.
 * @param   ppDevInsR3      Where to return the ring-3 device instance address.
 * @thread  EMT(0)
 */
static int pdmR0DeviceCreateWorker(PGVM pGVM, PCPDMDEVREGR0 pDevReg, uint32_t iInstance, uint32_t cbInstanceR3,
                                   uint32_t cbInstanceRC, RTRGPTR RCPtrMapping, void *hMod, PPDMDEVINSR3 *ppDevInsR3)
{
    /*
     * Check that the instance number isn't a duplicate.
     */
    for (size_t i = 0; i < pGVM->pdmr0.s.cDevInstances; i++)
    {
        PPDMDEVINS pCur = pGVM->pdmr0.s.apDevInstances[i];
        AssertLogRelReturn(!pCur || pCur->pReg != pDevReg || pCur->iInstance != iInstance, VERR_DUPLICATE);
    }

    /*
     * Figure out how much memory we need and allocate it.
     */
    uint32_t const cbRing0     = RT_ALIGN_32(RT_UOFFSETOF(PDMDEVINSR0, achInstanceData) + pDevReg->cbInstanceCC, PAGE_SIZE);
    uint32_t const cbRing3     = RT_ALIGN_32(RT_UOFFSETOF(PDMDEVINSR3, achInstanceData) + cbInstanceR3,
                                             RCPtrMapping != NIL_RTRGPTR ? PAGE_SIZE : 64);
    uint32_t const cbRC        = RCPtrMapping != NIL_RTRGPTR ? 0
                               : RT_ALIGN_32(RT_UOFFSETOF(PDMDEVINSRC, achInstanceData) + cbInstanceRC, 64);
    uint32_t const cbShared    = RT_ALIGN_32(pDevReg->cbInstanceShared, 64);
    uint32_t const cbCritSect  = RT_ALIGN_32(sizeof(PDMCRITSECT), 64);
    uint32_t const cbMsixState = RT_ALIGN_32(pDevReg->cMaxMsixVectors * 16 + (pDevReg->cMaxMsixVectors + 7) / 8, _4K);
    uint32_t const cbPciDev    = RT_ALIGN_32(RT_UOFFSETOF_DYN(PDMPCIDEV, abMsixState[cbMsixState]), 64);
    uint32_t const cPciDevs    = RT_MIN(pDevReg->cMaxPciDevices, 8);
    uint32_t const cbPciDevs   = cbPciDev * cPciDevs;
    uint32_t const cbTotal     = RT_ALIGN_32(cbRing0 + cbRing3 + cbRC + cbShared + cbCritSect + cbPciDevs, PAGE_SIZE);
    AssertLogRelMsgReturn(cbTotal <= PDM_MAX_DEVICE_INSTANCE_SIZE,
                          ("Instance of '%s' is too big: cbTotal=%u, max %u\n",
                           pDevReg->szName, cbTotal, PDM_MAX_DEVICE_INSTANCE_SIZE),
                          VERR_OUT_OF_RANGE);

    RTR0MEMOBJ hMemObj;
    int rc = RTR0MemObjAllocPage(&hMemObj, cbTotal, false /*fExecutable*/);
    if (RT_FAILURE(rc))
        return rc;
    RT_BZERO(RTR0MemObjAddress(hMemObj), cbTotal);

    /* Map it. */
    RTR0MEMOBJ hMapObj;
    rc = RTR0MemObjMapUserEx(&hMapObj, hMemObj, (RTR3PTR)-1, 0, RTMEM_PROT_READ | RTMEM_PROT_WRITE, RTR0ProcHandleSelf(),
                             cbRing0, cbTotal - cbRing0);
    if (RT_SUCCESS(rc))
    {
        PPDMDEVINSR0        pDevIns   = (PPDMDEVINSR0)RTR0MemObjAddress(hMemObj);
        struct PDMDEVINSR3 *pDevInsR3 = (struct PDMDEVINSR3 *)((uint8_t *)pDevIns + cbRing0);

        /*
         * Initialize the ring-0 instance.
         */
        pDevIns->u32Version             = PDM_DEVINSR0_VERSION;
        pDevIns->iInstance              = iInstance;
        pDevIns->pHlpR0                 = &g_pdmR0DevHlp;
        pDevIns->pvInstanceDataR0       = (uint8_t *)pDevIns + cbRing0 + cbRing3 + cbRC;
        pDevIns->pvInstanceDataForR0    = &pDevIns->achInstanceData[0];
        pDevIns->pCritSectRoR0          = (PPDMCRITSECT)((uint8_t *)pDevIns->pvInstanceDataR0 + cbShared);
        pDevIns->pReg                   = pDevReg;
        pDevIns->pDevInsForR3           = RTR0MemObjAddressR3(hMapObj);
        pDevIns->pDevInsForR3R0         = pDevInsR3;
        pDevIns->pvInstanceDataForR3R0  = &pDevInsR3->achInstanceData[0];
        pDevIns->cbPciDev               = cbPciDev;
        pDevIns->cPciDevs               = cPciDevs;
        for (uint32_t iPciDev = 0; iPciDev < cPciDevs; iPciDev++)
        {
            /* Note! PDMDevice.cpp has a copy of this code.  Keep in sync. */
            PPDMPCIDEV pPciDev = (PPDMPCIDEV)((uint8_t *)pDevIns->pCritSectRoR0 + cbCritSect + cbPciDev * iPciDev);
            if (iPciDev < RT_ELEMENTS(pDevIns->apPciDevs))
                pDevIns->apPciDevs[iPciDev] = pPciDev;
            pPciDev->cbConfig           = _4K;
            pPciDev->cbMsixState        = cbMsixState;
            pPciDev->idxSubDev          = (uint16_t)iPciDev;
            pPciDev->Int.s.idxSubDev    = (uint16_t)iPciDev;
            pPciDev->u32Magic           = PDMPCIDEV_MAGIC;
        }
        pDevIns->Internal.s.pGVM        = pGVM;
        pDevIns->Internal.s.pRegR0      = pDevReg;
        pDevIns->Internal.s.hMod        = hMod;
        pDevIns->Internal.s.hMemObj     = hMemObj;
        pDevIns->Internal.s.hMapObj     = hMapObj;
        pDevIns->Internal.s.pInsR3R0    = pDevInsR3;
        pDevIns->Internal.s.pIntR3R0    = &pDevInsR3->Internal.s;

        /*
         * Initialize the ring-3 instance data as much as we can.
         * Note! PDMDevice.cpp does this job for ring-3 only devices.  Keep in sync.
         */
        pDevInsR3->u32Version           = PDM_DEVINSR3_VERSION;
        pDevInsR3->iInstance            = iInstance;
        pDevInsR3->cbRing3              = cbTotal - cbRing0;
        pDevInsR3->fR0Enabled           = true;
        pDevInsR3->fRCEnabled           = RCPtrMapping != NIL_RTRGPTR;
        pDevInsR3->pvInstanceDataR3     = pDevIns->pDevInsForR3 + cbRing3 + cbRC;
        pDevInsR3->pvInstanceDataForR3  = pDevIns->pDevInsForR3 + RT_UOFFSETOF(PDMDEVINSR3, achInstanceData);
        pDevInsR3->pCritSectRoR3        = pDevIns->pDevInsForR3 + cbRing3 + cbRC + cbShared;
        pDevInsR3->pDevInsR0RemoveMe    = pDevIns;
        pDevInsR3->pvInstanceDataR0     = pDevIns->pvInstanceDataR0;
        pDevInsR3->pvInstanceDataRC     = RCPtrMapping == NIL_RTRGPTR
                                        ? NIL_RTRGPTR : pDevIns->pDevInsForRC + RT_UOFFSETOF(PDMDEVINSRC, achInstanceData);
        pDevInsR3->pDevInsForRC         = pDevIns->pDevInsForRC;
        pDevInsR3->pDevInsForRCR3       = pDevIns->pDevInsForR3 + cbRing3;
        pDevInsR3->pDevInsForRCR3       = pDevInsR3->pDevInsForRCR3 + RT_UOFFSETOF(PDMDEVINSRC, achInstanceData);
        pDevInsR3->cbPciDev             = cbPciDev;
        pDevInsR3->cPciDevs             = cPciDevs;
        for (uint32_t i = 0; i < RT_MIN(cPciDevs, RT_ELEMENTS(pDevIns->apPciDevs)); i++)
            pDevInsR3->apPciDevs[i] = pDevInsR3->pCritSectRoR3 + cbCritSect + cbPciDev * i;

        pDevInsR3->Internal.s.pVMR3     = pGVM->pVMR3;
        pDevInsR3->Internal.s.fIntFlags = RCPtrMapping == NIL_RTRGPTR ? PDMDEVINSINT_FLAGS_R0_ENABLED
                                        : PDMDEVINSINT_FLAGS_R0_ENABLED | PDMDEVINSINT_FLAGS_RC_ENABLED;

        /*
         * Initialize the raw-mode instance data as much as possible.
         */
        if (RCPtrMapping != NIL_RTRGPTR)
        {
            struct PDMDEVINSRC *pDevInsRC = RCPtrMapping == NIL_RTRGPTR ? NULL
                                          : (struct PDMDEVINSRC *)((uint8_t *)pDevIns + cbRing0 + cbRing3);

            pDevIns->pDevInsForRC           = RCPtrMapping;
            pDevIns->pDevInsForRCR0         = pDevInsRC;
            pDevIns->pvInstanceDataForRCR0  = &pDevInsRC->achInstanceData[0];

            pDevInsRC->u32Version           = PDM_DEVINSRC_VERSION;
            pDevInsRC->iInstance            = iInstance;
            pDevInsRC->pvInstanceDataRC     = pDevIns->pDevInsForRC + cbRC;
            pDevInsRC->pvInstanceDataForRC  = pDevIns->pDevInsForRC + RT_UOFFSETOF(PDMDEVINSRC, achInstanceData);
            pDevInsRC->pCritSectRoRC        = pDevIns->pDevInsForRC + cbRC + cbShared;
            pDevInsRC->cbPciDev             = cbPciDev;
            pDevInsRC->cPciDevs             = cPciDevs;
            for (uint32_t i = 0; i < RT_MIN(cPciDevs, RT_ELEMENTS(pDevIns->apPciDevs)); i++)
                pDevInsRC->apPciDevs[i] = pDevInsRC->pCritSectRoRC + cbCritSect + cbPciDev * i;

            pDevInsRC->Internal.s.pVMRC     = pGVM->pVMRC;
        }

        /*
         * Add to the device instance array and set its handle value.
         */
        AssertCompile(sizeof(pGVM->pdmr0.padding) == sizeof(pGVM->pdmr0));
        uint32_t idxR0Device = pGVM->pdmr0.s.cDevInstances;
        if (idxR0Device < RT_ELEMENTS(pGVM->pdmr0.s.apDevInstances))
        {
            pGVM->pdmr0.s.apDevInstances[idxR0Device] = pDevIns;
            pGVM->pdmr0.s.cDevInstances = idxR0Device + 1;
            pDevIns->Internal.s.idxR0Device   = idxR0Device;
            pDevInsR3->Internal.s.idxR0Device = idxR0Device;

            /*
             * Call the early constructor if present.
             */
            if (pDevReg->pfnEarlyConstruct)
                rc = pDevReg->pfnEarlyConstruct(pDevIns);
            if (RT_SUCCESS(rc))
            {
                /*
                 * We're done.
                 */
                *ppDevInsR3 = RTR0MemObjAddressR3(hMapObj);
                return rc;
            }

            /*
             * Bail out.
             */
            if (pDevIns->pReg->pfnFinalDestruct)
                pDevIns->pReg->pfnFinalDestruct(pDevIns);

            pGVM->pdmr0.s.apDevInstances[idxR0Device] = NULL;
            Assert(pGVM->pdmr0.s.cDevInstances == idxR0Device + 1);
            pGVM->pdmr0.s.cDevInstances = idxR0Device;
        }

        RTR0MemObjFree(hMapObj, true);
    }
    RTR0MemObjFree(hMemObj, true);
    return rc;
}


/**
 * Used by ring-3 PDM to create a device instance that operates both in ring-3
 * and ring-0.
 *
 * Creates an instance of a device (for both ring-3 and ring-0, and optionally
 * raw-mode context).
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) PDMR0DeviceCreateReqHandler(PGVM pGVM, PPDMDEVICECREATEREQ pReq)
{
    LogFlow(("PDMR0DeviceCreateReqHandler: %s in %s\n", pReq->szDevName, pReq->szModName));

    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);
    pReq->pDevInsR3 = NIL_RTR3PTR;

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->fFlags           != 0, VERR_INVALID_FLAGS);
    AssertReturn(pReq->fClass           != 0, VERR_WRONG_TYPE);
    AssertReturn(pReq->uSharedVersion   != 0, VERR_INVALID_PARAMETER);
    AssertReturn(pReq->cbInstanceShared != 0, VERR_INVALID_PARAMETER);
    size_t const cchDevName = RTStrNLen(pReq->szDevName, sizeof(pReq->szDevName));
    AssertReturn(cchDevName < sizeof(pReq->szDevName), VERR_NO_STRING_TERMINATOR);
    AssertReturn(cchDevName > 0, VERR_EMPTY_STRING);
    AssertReturn(cchDevName < RT_SIZEOFMEMB(PDMDEVREG, szName), VERR_NOT_FOUND);

    size_t const cchModName = RTStrNLen(pReq->szModName, sizeof(pReq->szModName));
    AssertReturn(cchModName < sizeof(pReq->szModName), VERR_NO_STRING_TERMINATOR);
    AssertReturn(cchModName > 0, VERR_EMPTY_STRING);
    AssertReturn(pReq->cbInstanceShared <= PDM_MAX_DEVICE_INSTANCE_SIZE, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->cbInstanceR3 <= PDM_MAX_DEVICE_INSTANCE_SIZE, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->cbInstanceRC <= PDM_MAX_DEVICE_INSTANCE_SIZE, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->iInstance < 1024, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->iInstance < pReq->cMaxInstances, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->cMaxPciDevices <= 8, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->cMaxMsixVectors <= VBOX_MSIX_MAX_ENTRIES, VERR_OUT_OF_RANGE);

    /*
     * Reference the module.
     */
    void *hMod = NULL;
    rc = SUPR0LdrModByName(pGVM->pSession, pReq->szModName, &hMod);
    if (RT_FAILURE(rc))
    {
        LogRel(("PDMR0DeviceCreateReqHandler: SUPR0LdrModByName(,%s,) failed: %Rrc\n", pReq->szModName, rc));
        return rc;
    }

    /*
     * Look for the the module and the device registration structure.
     */
    int rcLock = SUPR0LdrLock(pGVM->pSession);
    AssertRC(rc);

    rc = VERR_NOT_FOUND;
    PPDMDEVMODREGR0 pMod;
    RTListForEach(&g_PDMDevModList, pMod, PDMDEVMODREGR0, ListEntry)
    {
        if (pMod->hMod == hMod)
        {
            /*
             * Found the module. We can drop the loader lock now before we
             * search the devices it registers.
             */
            if (RT_SUCCESS(rcLock))
            {
                rcLock = SUPR0LdrUnlock(pGVM->pSession);
                AssertRC(rcLock);
            }
            rcLock = VERR_ALREADY_RESET;

            PCPDMDEVREGR0 *papDevRegs = pMod->papDevRegs;
            size_t         i          = pMod->cDevRegs;
            while (i-- > 0)
            {
                PCPDMDEVREGR0 pDevReg = papDevRegs[i];
                LogFlow(("PDMR0DeviceCreateReqHandler: candidate #%u: %s %#x\n", i, pReq->szDevName, pDevReg->u32Version));
                if (   PDM_VERSION_ARE_COMPATIBLE(pDevReg->u32Version, PDM_DEVREGR0_VERSION)
                    && pDevReg->szName[cchDevName] == '\0'
                    && memcmp(pDevReg->szName, pReq->szDevName, cchDevName) == 0)
                {

                    /*
                     * Found the device, now check whether it matches the ring-3 registration.
                     */
                    if (   pReq->uSharedVersion   == pDevReg->uSharedVersion
                        && pReq->cbInstanceShared == pDevReg->cbInstanceShared
                        && pReq->cbInstanceRC     == pDevReg->cbInstanceRC
                        && pReq->fFlags           == pDevReg->fFlags
                        && pReq->fClass           == pDevReg->fClass
                        && pReq->cMaxInstances    == pDevReg->cMaxInstances
                        && pReq->cMaxPciDevices   == pDevReg->cMaxPciDevices
                        && pReq->cMaxMsixVectors  == pDevReg->cMaxMsixVectors)
                    {
                        rc = pdmR0DeviceCreateWorker(pGVM, pDevReg, pReq->iInstance, pReq->cbInstanceR3, pReq->cbInstanceRC,
                                                     NIL_RTRCPTR /** @todo new raw-mode */, hMod, &pReq->pDevInsR3);
                        if (RT_SUCCESS(rc))
                            hMod = NULL; /* keep the module reference */
                    }
                    else
                    {
                        LogRel(("PDMR0DeviceCreate: Ring-3 does not match ring-0 device registration (%s):\n"
                                "    uSharedVersion: %#x vs %#x\n"
                                "  cbInstanceShared: %#x vs %#x\n"
                                "      cbInstanceRC: %#x vs %#x\n"
                                "            fFlags: %#x vs %#x\n"
                                "            fClass: %#x vs %#x\n"
                                "     cMaxInstances: %#x vs %#x\n"
                                "    cMaxPciDevices: %#x vs %#x\n"
                                "   cMaxMsixVectors: %#x vs %#x\n"
                                ,
                                pReq->szDevName,
                                pReq->uSharedVersion,   pDevReg->uSharedVersion,
                                pReq->cbInstanceShared, pDevReg->cbInstanceShared,
                                pReq->cbInstanceRC,     pDevReg->cbInstanceRC,
                                pReq->fFlags,           pDevReg->fFlags,
                                pReq->fClass,           pDevReg->fClass,
                                pReq->cMaxInstances,    pDevReg->cMaxInstances,
                                pReq->cMaxPciDevices,   pDevReg->cMaxPciDevices,
                                pReq->cMaxMsixVectors,  pDevReg->cMaxMsixVectors));
                        rc = VERR_INCOMPATIBLE_CONFIG;
                    }
                }
            }
            break;
        }
    }

    if (RT_SUCCESS_NP(rcLock))
    {
        rcLock = SUPR0LdrUnlock(pGVM->pSession);
        AssertRC(rcLock);
    }
    SUPR0LdrModRelease(pGVM->pSession, hMod);
    return rc;
}


/**
 * Used by ring-3 PDM to call standard ring-0 device methods.
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @param   idCpu   The ID of the calling EMT.
 * @thread  EMT(0), except for PDMDEVICEGENCALL_REQUEST which can be any EMT.
 */
VMMR0_INT_DECL(int) PDMR0DeviceGenCallReqHandler(PGVM pGVM, PPDMDEVICEGENCALLREQ pReq, VMCPUID idCpu)
{
    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);

    int rc = GVMMR0ValidateGVMandEMT(pGVM, idCpu);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->idxR0Device < pGVM->pdmr0.s.cDevInstances, VERR_INVALID_HANDLE);
    PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[pReq->idxR0Device];
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(pDevIns->pDevInsForR3 == pReq->pDevInsR3, VERR_INVALID_HANDLE);

    /*
     * Make the call.
     */
    rc = VINF_SUCCESS /*VINF_NOT_IMPLEMENTED*/;
    switch (pReq->enmCall)
    {
        case PDMDEVICEGENCALL_CONSTRUCT:
            AssertMsgBreakStmt(pGVM->enmVMState < VMSTATE_CREATED, ("enmVMState=%d\n", pGVM->enmVMState), rc = VERR_INVALID_STATE);
            AssertReturn(idCpu == 0,  VERR_VM_THREAD_NOT_EMT);
            if (pDevIns->pReg->pfnConstruct)
                rc = pDevIns->pReg->pfnConstruct(pDevIns);
            break;

        case PDMDEVICEGENCALL_DESTRUCT:
            AssertMsgBreakStmt(pGVM->enmVMState < VMSTATE_CREATED || pGVM->enmVMState >= VMSTATE_DESTROYING,
                               ("enmVMState=%d\n", pGVM->enmVMState), rc = VERR_INVALID_STATE);
            AssertReturn(idCpu == 0,  VERR_VM_THREAD_NOT_EMT);
            if (pDevIns->pReg->pfnDestruct)
            {
                pDevIns->pReg->pfnDestruct(pDevIns);
                rc = VINF_SUCCESS;
            }
            break;

        case PDMDEVICEGENCALL_REQUEST:
            if (pDevIns->pReg->pfnRequest)
                rc = pDevIns->pReg->pfnRequest(pDevIns, pReq->Params.Req.uReq, pReq->Params.Req.uArg);
            else
                rc = VERR_INVALID_FUNCTION;
            break;

        default:
            AssertMsgFailed(("enmCall=%d\n", pReq->enmCall));
            rc = VERR_INVALID_FUNCTION;
            break;
    }

    return rc;
}


/**
 * Legacy device mode compatiblity.
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) PDMR0DeviceCompatSetCritSectReqHandler(PGVM pGVM, PPDMDEVICECOMPATSETCRITSECTREQ pReq)
{
    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->idxR0Device < pGVM->pdmr0.s.cDevInstances, VERR_INVALID_HANDLE);
    PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[pReq->idxR0Device];
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(pDevIns->pDevInsForR3 == pReq->pDevInsR3, VERR_INVALID_HANDLE);

    AssertReturn(pGVM->enmVMState == VMSTATE_CREATING, VERR_INVALID_STATE);

    /*
     * The critical section address can be in a few different places:
     *      1. shared data.
     *      2. nop section.
     *      3. pdm critsect.
     */
    PPDMCRITSECT pCritSect;
    if (pReq->pCritSectR3 == pGVM->pVMR3 + RT_UOFFSETOF(VM, pdm.s.NopCritSect))
    {
        pCritSect = &pGVM->pdm.s.NopCritSect;
        Log(("PDMR0DeviceCompatSetCritSectReqHandler: Nop - %p %#x\n", pCritSect, pCritSect->s.Core.u32Magic));
    }
    else if (pReq->pCritSectR3 == pGVM->pVMR3 + RT_UOFFSETOF(VM, pdm.s.CritSect))
    {
        pCritSect = &pGVM->pdm.s.CritSect;
        Log(("PDMR0DeviceCompatSetCritSectReqHandler: PDM - %p %#x\n", pCritSect, pCritSect->s.Core.u32Magic));
    }
    else
    {
        size_t offCritSect = pReq->pCritSectR3 - pDevIns->pDevInsForR3R0->pvInstanceDataR3;
        AssertLogRelMsgReturn(   offCritSect                       <  pDevIns->pReg->cbInstanceShared
                              && offCritSect + sizeof(PDMCRITSECT) <= pDevIns->pReg->cbInstanceShared,
                              ("offCritSect=%p pCritSectR3=%p cbInstanceShared=%#x (%s)\n",
                               offCritSect, pReq->pCritSectR3, pDevIns->pReg->cbInstanceShared, pDevIns->pReg->szName),
                              VERR_INVALID_POINTER);
        pCritSect = (PPDMCRITSECT)((uint8_t *)pDevIns->pvInstanceDataR0 + offCritSect);
        Log(("PDMR0DeviceCompatSetCritSectReqHandler: custom - %#x/%p %#x\n", offCritSect, pCritSect, pCritSect->s.Core.u32Magic));
    }
    AssertLogRelMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC,
                          ("cs=%p magic=%#x dev=%s\n", pCritSect, pCritSect->s.Core.u32Magic, pDevIns->pReg->szName),
                          VERR_INVALID_MAGIC);

    /*
     * Make the update.
     */
    pDevIns->pCritSectRoR0 = pCritSect;

    return VINF_SUCCESS;
}


/**
 * Registers the device implementations living in a module.
 *
 * This should normally only be called during ModuleInit().  The should be a
 * call to PDMR0DeviceDeregisterModule from the ModuleTerm() function to undo
 * the effects of this call.
 *
 * @returns VBox status code.
 * @param   hMod            The module handle of the module being registered.
 * @param   pModReg         The module registration structure.  This will be
 *                          used directly so it must live as long as the module
 *                          and be writable.
 *
 * @note    Caller must own the loader lock!
 */
VMMR0DECL(int) PDMR0DeviceRegisterModule(void *hMod, PPDMDEVMODREGR0 pModReg)
{
    /*
     * Validate the input.
     */
    AssertPtrReturn(hMod, VERR_INVALID_HANDLE);
    Assert(SUPR0LdrIsLockOwnerByMod(hMod, true));

    AssertPtrReturn(pModReg, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(PDM_VERSION_ARE_COMPATIBLE(pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          ("pModReg->u32Version=%#x vs %#x\n", pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pModReg->cDevRegs <= 256 && pModReg->cDevRegs > 0, ("cDevRegs=%u\n", pModReg->cDevRegs),
                          VERR_OUT_OF_RANGE);
    AssertLogRelMsgReturn(pModReg->hMod == NULL, ("hMod=%p\n", pModReg->hMod), VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pModReg->ListEntry.pNext == NULL, ("pNext=%p\n", pModReg->ListEntry.pNext), VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pModReg->ListEntry.pPrev == NULL, ("pPrev=%p\n", pModReg->ListEntry.pPrev), VERR_INVALID_PARAMETER);

    for (size_t i = 0; i < pModReg->cDevRegs; i++)
    {
        PCPDMDEVREGR0 pDevReg = pModReg->papDevRegs[i];
        AssertLogRelMsgReturn(RT_VALID_PTR(pDevReg), ("[%u]: %p\n", i, pDevReg), VERR_INVALID_POINTER);
        AssertLogRelMsgReturn(PDM_VERSION_ARE_COMPATIBLE(pDevReg->u32Version, PDM_DEVREGR0_VERSION),
                              ("pDevReg->u32Version=%#x vs %#x\n", pModReg->u32Version, PDM_DEVREGR0_VERSION), VERR_VERSION_MISMATCH);
        AssertLogRelMsgReturn(RT_VALID_PTR(pDevReg->pszDescription), ("[%u]: %p\n", i, pDevReg->pszDescription), VERR_INVALID_POINTER);
        AssertLogRelMsgReturn(pDevReg->uReserved0     == 0, ("[%u]: %#x\n", i, pDevReg->uReserved0),     VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->fClass         != 0, ("[%u]: %#x\n", i, pDevReg->fClass),         VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->fFlags         != 0, ("[%u]: %#x\n", i, pDevReg->fFlags),         VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->cMaxInstances   > 0, ("[%u]: %#x\n", i, pDevReg->cMaxInstances),  VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->cMaxPciDevices <= 8, ("[%u]: %#x\n", i, pDevReg->cMaxPciDevices), VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->cMaxMsixVectors <= VBOX_MSIX_MAX_ENTRIES,
                              ("[%u]: %#x\n", i, pDevReg->cMaxMsixVectors), VERR_INVALID_PARAMETER);

        /* The name must be printable ascii and correctly terminated. */
        for (size_t off = 0; off < RT_ELEMENTS(pDevReg->szName); off++)
        {
            char ch = pDevReg->szName[off];
            AssertLogRelMsgReturn(RT_C_IS_PRINT(ch) || (ch == '\0' && off > 0),
                                  ("[%u]: off=%u  szName: %.*Rhxs\n", i, off, sizeof(pDevReg->szName), &pDevReg->szName[0]),
                                  VERR_INVALID_NAME);
            if (ch == '\0')
                break;
        }
    }

    /*
     * Add it, assuming we're being called at ModuleInit/ModuleTerm time only, or
     * that the caller has already taken the loader lock.
     */
    pModReg->hMod = hMod;
    RTListAppend(&g_PDMDevModList, &pModReg->ListEntry);

    return VINF_SUCCESS;
}


/**
 * Deregisters the device implementations living in a module.
 *
 * This should normally only be called during ModuleTerm().
 *
 * @returns VBox status code.
 * @param   hMod            The module handle of the module being registered.
 * @param   pModReg         The module registration structure.  This will be
 *                          used directly so it must live as long as the module
 *                          and be writable.
 *
 * @note    Caller must own the loader lock!
 */
VMMR0DECL(int) PDMR0DeviceDeregisterModule(void *hMod, PPDMDEVMODREGR0 pModReg)
{
    /*
     * Validate the input.
     */
    AssertPtrReturn(hMod, VERR_INVALID_HANDLE);
    Assert(SUPR0LdrIsLockOwnerByMod(hMod, true));

    AssertPtrReturn(pModReg, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(PDM_VERSION_ARE_COMPATIBLE(pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          ("pModReg->u32Version=%#x vs %#x\n", pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pModReg->hMod == hMod || pModReg->hMod == NULL, ("pModReg->hMod=%p vs %p\n", pModReg->hMod, hMod),
                          VERR_INVALID_PARAMETER);

    /*
     * Unlink the registration record and return it to virgin conditions.  Ignore
     * the call if not registered.
     */
    if (pModReg->hMod)
    {
        pModReg->hMod = NULL;
        RTListNodeRemove(&pModReg->ListEntry);
        pModReg->ListEntry.pNext = NULL;
        pModReg->ListEntry.pPrev = NULL;
        return VINF_SUCCESS;
    }
    return VWRN_NOT_FOUND;
}

