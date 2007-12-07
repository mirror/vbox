/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, GC Device parts.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
#include <VBox/patm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def PDMDEV_ASSERT_DEVINS
 * Asserts the validity of the driver instance.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { Assert(VALID_PTR(pDevIns)); \
                                              Assert(pDevIns->u32Version == PDM_DEVINS_VERSION); \
                                              Assert(pDevIns->pvInstanceDataGC == (void *)&pDevIns->achInstanceData[0]); \
                                         } while (0)
#else
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { } while (0)
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
__BEGIN_DECLS
extern DECLEXPORT(const PDMDEVHLPGC)    g_pdmGCDevHlp;
extern DECLEXPORT(const PDMPICHLPGC)    g_pdmGCPicHlp;
extern DECLEXPORT(const PDMAPICHLPGC)   g_pdmGCApicHlp;
extern DECLEXPORT(const PDMIOAPICHLPGC) g_pdmGCIoApicHlp;
extern DECLEXPORT(const PDMPCIHLPGC)    g_pdmGCPciHlp;
__END_DECLS


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/** @name GC Device Helpers
 * @{
 */
static DECLCALLBACK(void) pdmGCDevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmGCDevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmGCDevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
static DECLCALLBACK(void) pdmGCDevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(bool) pdmGCDevHlp_A20IsEnabled(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmGCDevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
static DECLCALLBACK(int) pdmGCDevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va);
static DECLCALLBACK(int) pdmGCDevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...);
static DECLCALLBACK(int) pdmGCDevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va);
static DECLCALLBACK(int)  pdmGCDevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData);
/** @} */


/** @name PIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmGCPicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmGCPicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmGCPicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmGCPicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


/** @name APIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmGCApicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmGCApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmGCApicHlp_ChangeFeature(PPDMDEVINS pDevIns, bool fEnabled);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmGCApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmGCApicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


/** @name I/O APIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmGCIoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmGCIoApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmGCIoApicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


/** @name PCI Bus GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmGCPciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmGCPciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmGCPciHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmGCPciHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


static void pdmGCIsaSetIrq(PVM pVM, int iIrq, int iLevel);
static void pdmGCIoApicSetIrq(PVM pVM, int iIrq, int iLevel);



/**
 * The Guest Context Device Helper Callbacks.
 */
extern DECLEXPORT(const PDMDEVHLPGC) g_pdmGCDevHlp =
{
    PDM_DEVHLPGC_VERSION,
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
    PDM_DEVHLPGC_VERSION
};

/**
 * The Guest Context PIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMPICHLPGC) g_pdmGCPicHlp =
{
    PDM_PICHLPGC_VERSION,
    pdmGCPicHlp_SetInterruptFF,
    pdmGCPicHlp_ClearInterruptFF,
#ifdef VBOX_WITH_PDM_LOCK
    pdmGCPicHlp_Lock,
    pdmGCPicHlp_Unlock,
#endif
    PDM_PICHLPGC_VERSION
};


/**
 * The Guest Context APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMAPICHLPGC) g_pdmGCApicHlp =
{
    PDM_APICHLPGC_VERSION,
    pdmGCApicHlp_SetInterruptFF,
    pdmGCApicHlp_ClearInterruptFF,
    pdmGCApicHlp_ChangeFeature,
#ifdef VBOX_WITH_PDM_LOCK
    pdmGCApicHlp_Lock,
    pdmGCApicHlp_Unlock,
#endif
    PDM_APICHLPGC_VERSION
};


/**
 * The Guest Context I/O APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOAPICHLPGC) g_pdmGCIoApicHlp =
{
    PDM_IOAPICHLPGC_VERSION,
    pdmGCIoApicHlp_ApicBusDeliver,
#ifdef VBOX_WITH_PDM_LOCK
    pdmGCIoApicHlp_Lock,
    pdmGCIoApicHlp_Unlock,
#endif
    PDM_IOAPICHLPGC_VERSION
};


/**
 * The Guest Context PCI Bus Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIHLPGC) g_pdmGCPciHlp =
{
    PDM_PCIHLPGC_VERSION,
    pdmGCPciHlp_IsaSetIrq,
    pdmGCPciHlp_IoApicSetIrq,
#ifdef VBOX_WITH_PDM_LOCK
    pdmGCPciHlp_Lock,
    pdmGCPciHlp_Unlock,
#endif
    PDM_PCIHLPGC_VERSION, /* the end */
};




/** @copydoc PDMDEVHLPGC::pfnPCISetIrq */
static DECLCALLBACK(void) pdmGCDevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PCISetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    PVM         pVM     = pDevIns->Internal.s.pVMGC;
    PPCIDEVICE  pPciDev = pDevIns->Internal.s.pPciDeviceGC;
    PPDMPCIBUS  pPciBus = pDevIns->Internal.s.pPciBusGC;
    if (    pPciDev
        &&  pPciBus
        &&  pPciBus->pDevInsGC)
    {
        pdmLock(pVM);
        pPciBus->pfnSetIrqGC(pPciBus->pDevInsGC, pPciDev, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueGC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_PCI_SET_IRQ;
            pTask->pDevInsHC = MMHyperGC2HC(pVM, pDevIns);
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueGC, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }

    LogFlow(("pdmGCDevHlp_PCISetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPGC::pfnPCISetIrq */
static DECLCALLBACK(void) pdmGCDevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_ISASetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    pdmGCIsaSetIrq(pDevIns->Internal.s.pVMGC, iIrq, iLevel);

    LogFlow(("pdmGCDevHlp_ISASetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPGC::pfnPhysRead */
static DECLCALLBACK(void) pdmGCDevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PhysRead: caller=%p/%d: GCPhys=%VGp pvBuf=%p cbRead=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    PGMPhysRead(pDevIns->Internal.s.pVMGC, GCPhys, pvBuf, cbRead);

    Log(("pdmGCDevHlp_PhysRead: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPGC::pfnPhysWrite */
static DECLCALLBACK(void) pdmGCDevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PhysWrite: caller=%p/%d: GCPhys=%VGp pvBuf=%p cbWrite=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    PGMPhysWrite(pDevIns->Internal.s.pVMGC, GCPhys, pvBuf, cbWrite);

    Log(("pdmGCDevHlp_PhysWrite: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPGC::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmGCDevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_A20IsEnabled: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    bool fEnabled = PGMPhysIsA20Enabled(pDevIns->Internal.s.pVMGC);

    Log(("pdmGCDevHlp_A20IsEnabled: caller=%p/%d: returns %RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    return fEnabled;
}


/** @copydoc PDMDEVHLPGC::pfnVMSetError */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMGC, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPGC::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMGC, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDEVHLPGC::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMGC, fFatal, pszErrorID, pszFormat, args);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPGC::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmGCDevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMGC, fFatal, pszErrorID, pszFormat, va);
    return rc;
}


/** @copydoc PDMDEVHLPGC::pdmGCDevHlp_PATMSetMMIOPatchInfo */
static DECLCALLBACK(int) pdmGCDevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCDevHlp_PATMSetMMIOPatchInfo: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    return PATMSetMMIOPatchInfo(pDevIns->Internal.s.pVMGC, GCPhys, pCachedData);
}




/** @copydoc PDMPICHLPGC::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmGCPicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCPicHlp_SetInterruptFF: caller=%p/%d: VM_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_PIC)));
    VM_FF_SET(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_PIC);
}


/** @copydoc PDMPICHLPGC::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmGCPicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCPicHlp_ClearInterruptFF: caller=%p/%d: VM_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_PIC)));
    VM_FF_CLEAR(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_PIC);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMPICHLPGC::pfnLock */
static DECLCALLBACK(int) pdmGCPicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMGC, rc);
}


/** @copydoc PDMPICHLPGC::pfnUnlock */
static DECLCALLBACK(void) pdmGCPicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMGC);
}
#endif /* VBOX_WITH_PDM_LOCK */




/** @copydoc PDMAPICHLPGC::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmGCApicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCApicHlp_SetInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 1\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_APIC)));
    VM_FF_SET(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPGC::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmGCApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCApicHlp_ClearInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 0\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_APIC)));
    VM_FF_CLEAR(pDevIns->Internal.s.pVMGC, VM_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPGC::pfnChangeFeature */
static DECLCALLBACK(void) pdmGCApicHlp_ChangeFeature(PPDMDEVINS pDevIns, bool fEnabled)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmGCApicHlp_ChangeFeature: caller=%p/%d: fEnabled=%RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    if (fEnabled)
        CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMGC, CPUMCPUIDFEATURE_APIC);
    else
        CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMGC, CPUMCPUIDFEATURE_APIC);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMAPICHLPGC::pfnLock */
static DECLCALLBACK(int) pdmGCApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMGC, rc);
}


/** @copydoc PDMAPICHLPGC::pfnUnlock */
static DECLCALLBACK(void) pdmGCApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMGC);
}
#endif /* VBOX_WITH_PDM_LOCK */




/** @copydoc PDMIOAPICHLPGC::pfnApicBusDeliver */
static DECLCALLBACK(void) pdmGCIoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMGC;
    LogFlow(("pdmGCIoApicHlp_ApicBusDeliver: caller=%p/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8\n",
             pDevIns, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    if (pVM->pdm.s.Apic.pfnBusDeliverGC)
        pVM->pdm.s.Apic.pfnBusDeliverGC(pVM->pdm.s.Apic.pDevInsGC, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMIOAPICHLPGC::pfnLock */
static DECLCALLBACK(int) pdmGCIoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMGC, rc);
}


/** @copydoc PDMIOAPICHLPGC::pfnUnlock */
static DECLCALLBACK(void) pdmGCIoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMGC);
}
#endif /* VBOX_WITH_PDM_LOCK */




/** @copydoc PDMPCIHLPGC::pfnIsaSetIrq */
static DECLCALLBACK(void) pdmGCPciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmGCPciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmGCIsaSetIrq(pDevIns->Internal.s.pVMGC, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPGC::pfnIoApicSetIrq */
static DECLCALLBACK(void) pdmGCPciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmGCPciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmGCIoApicSetIrq(pDevIns->Internal.s.pVMGC, iIrq, iLevel);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMPCIHLPGC::pfnLock */
static DECLCALLBACK(int) pdmGCPciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMGC, rc);
}


/** @copydoc PDMPCIHLPGC::pfnUnlock */
static DECLCALLBACK(void) pdmGCPciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMGC);
}
#endif /* VBOX_WITH_PDM_LOCK */




/**
 * Sets an irq on the I/O APIC.
 *
 * @param   pVM     The VM handle.
 * @param   iIrq    The irq.
 * @param   iLevel  The new level.
 */
static void pdmGCIsaSetIrq(PVM pVM, int iIrq, int iLevel)
{
    if (    (   pVM->pdm.s.IoApic.pDevInsGC
             || !pVM->pdm.s.IoApic.pDevInsR3)
        &&  (   pVM->pdm.s.Pic.pDevInsGC
             || !pVM->pdm.s.Pic.pDevInsR3))
    {
        pdmLock(pVM);
        if (pVM->pdm.s.Pic.pDevInsGC)
            pVM->pdm.s.Pic.pfnSetIrqGC(pVM->pdm.s.Pic.pDevInsGC, iIrq, iLevel);
        if (pVM->pdm.s.IoApic.pDevInsGC)
            pVM->pdm.s.IoApic.pfnSetIrqGC(pVM->pdm.s.IoApic.pDevInsGC, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueGC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_ISA_SET_IRQ;
            pTask->pDevInsHC = 0; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueGC, &pTask->Core, 0);
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
    if (pVM->pdm.s.IoApic.pDevInsGC)
    {
        pdmLock(pVM);
        pVM->pdm.s.IoApic.pfnSetIrqGC(pVM->pdm.s.IoApic.pDevInsGC, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else if (pVM->pdm.s.IoApic.pDevInsR3)
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueGC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_IOAPIC_SET_IRQ;
            pTask->pDevInsHC = 0; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueGC, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}
