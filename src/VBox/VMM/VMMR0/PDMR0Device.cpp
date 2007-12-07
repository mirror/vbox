/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, R0 Device parts.
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
                                              Assert(pDevIns->pvInstanceDataR0 == (void *)&pDevIns->achInstanceData[0]); \
                                         } while (0)
#else
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { } while (0)
#endif


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
static DECLCALLBACK(void) pdmR0DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
static DECLCALLBACK(void) pdmR0DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(bool) pdmR0DevHlp_A20IsEnabled(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR0DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
static DECLCALLBACK(int) pdmR0DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va);
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...);
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va);
static DECLCALLBACK(int) pdmR0DevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData);
/** @} */


/** @name PIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0PicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR0PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR0PicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0PicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


/** @name APIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR0ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR0ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, bool fEnabled);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR0ApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0ApicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


/** @name I/O APIC GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR0IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0IoApicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
/** @} */


/** @name PCI Bus GC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR0PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR0PciHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR0PciHlp_Unlock(PPDMDEVINS pDevIns);
#endif
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
#ifdef VBOX_WITH_PDM_LOCK
    pdmR0PicHlp_Lock,
    pdmR0PicHlp_Unlock,
#endif
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
#ifdef VBOX_WITH_PDM_LOCK
    pdmR0ApicHlp_Lock,
    pdmR0ApicHlp_Unlock,
#endif
    PDM_APICHLPR0_VERSION
};


/**
 * The Guest Context I/O APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOAPICHLPR0) g_pdmR0IoApicHlp =
{
    PDM_IOAPICHLPR0_VERSION,
    pdmR0IoApicHlp_ApicBusDeliver,
#ifdef VBOX_WITH_PDM_LOCK
    pdmR0IoApicHlp_Lock,
    pdmR0IoApicHlp_Unlock,
#endif
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
#ifdef VBOX_WITH_PDM_LOCK
    pdmR0PciHlp_Lock,
    pdmR0PciHlp_Unlock,
#endif
    PDM_PCIHLPR0_VERSION, /* the end */
};




/** @copydoc PDMDEVHLPR0::pfnPCISetIrq */
static DECLCALLBACK(void) pdmR0DevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));

    PVM          pVM     = pDevIns->Internal.s.pVMHC;
    PPCIDEVICE   pPciDev = pDevIns->Internal.s.pPciDeviceHC;
    PPDMPCIBUS   pPciBus = pDevIns->Internal.s.pPciBusHC;
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
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueHC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_PCI_SET_IRQ;
            pTask->pDevInsHC = pDevIns;
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueHC, &pTask->Core, 0);
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

    pdmR0IsaSetIrq(pDevIns->Internal.s.pVMHC, iIrq, iLevel);

    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR0::pfnPhysRead */
static DECLCALLBACK(void) pdmR0DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysRead: caller=%p/%d: GCPhys=%VGp pvBuf=%p cbRead=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    PGMPhysRead(pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbRead);

    Log(("pdmR0DevHlp_PhysRead: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR0::pfnPhysWrite */
static DECLCALLBACK(void) pdmR0DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysWrite: caller=%p/%d: GCPhys=%VGp pvBuf=%p cbWrite=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    PGMPhysWrite(pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbWrite);

    Log(("pdmR0DevHlp_PhysWrite: caller=%p/%d: returns void\n", pDevIns, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR0::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmR0DevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    bool fEnabled = PGMPhysIsA20Enabled(pDevIns->Internal.s.pVMHC);

    Log(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d: returns %RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    return fEnabled;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetError */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMHC, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMHC, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMHC, fFatal, pszErrorID, pszFormat, args);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pfnVMSetRuntimeErrorV */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMHC, fFatal, pszErrorID, pszFormat, va);
    return rc;
}


/** @copydoc PDMDEVHLPR0::pdmR0DevHlp_PATMSetMMIOPatchInfo*/
static DECLCALLBACK(int) pdmR0DevHlp_PATMSetMMIOPatchInfo(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPTR pCachedData)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PATMSetMMIOPatchInfo: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    AssertFailed();

/*    return PATMSetMMIOPatchInfo(pDevIns->Internal.s.pVMHC, GCPhys, pCachedData); */
    return VINF_SUCCESS;
}





/** @copydoc PDMPICHLPGC::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR0PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0PicHlp_SetInterruptFF: caller=%p/%d: VM_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_PIC)));
    VM_FF_SET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_PIC);
}


/** @copydoc PDMPICHLPGC::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR0PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0PicHlp_ClearInterruptFF: caller=%p/%d: VM_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_PIC)));
    VM_FF_CLEAR(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_PIC);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMPICHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMPICHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */




/** @copydoc PDMAPICHLPGC::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR0ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0ApicHlp_SetInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 1\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_APIC)));
    VM_FF_SET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPGC::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR0ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0ApicHlp_ClearInterruptFF: caller=%p/%d: VM_FF_INTERRUPT %d -> 0\n",
             pDevIns, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_APIC)));
    VM_FF_CLEAR(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_APIC);
}


/** @copydoc PDMAPICHLPGC::pfnChangeFeature */
static DECLCALLBACK(void) pdmR0ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, bool fEnabled)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0ApicHlp_ChangeFeature: caller=%p/%d: fEnabled=%RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    if (fEnabled)
        CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMHC, CPUMCPUIDFEATURE_APIC);
    else
        CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMHC, CPUMCPUIDFEATURE_APIC);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMAPICHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0ApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMAPICHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0ApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */




/** @copydoc PDMIOAPICHLPR0::pfnApicBusDeliver */
static DECLCALLBACK(void) pdmR0IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    LogFlow(("pdmR0IoApicHlp_ApicBusDeliver: caller=%p/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8\n",
             pDevIns, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    Assert(pVM->pdm.s.Apic.pDevInsR0);
    if (pVM->pdm.s.Apic.pfnBusDeliverR0)
        pVM->pdm.s.Apic.pfnBusDeliverR0(pVM->pdm.s.Apic.pDevInsR0, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMIOAPICHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMIOAPICHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */





/** @copydoc PDMPCIHLPR0::pfnIsaSetIrq */
static DECLCALLBACK(void) pdmR0PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmR0IsaSetIrq(pDevIns->Internal.s.pVMHC, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPR0::pfnIoApicSetIrq */
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    pdmR0IoApicSetIrq(pDevIns->Internal.s.pVMHC, iIrq, iLevel);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMPCIHLPR0::pfnLock */
static DECLCALLBACK(int) pdmR0PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMPCIHLPR0::pfnUnlock */
static DECLCALLBACK(void) pdmR0PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */




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
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueHC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_ISA_SET_IRQ;
            pTask->pDevInsHC = 0; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueHC, &pTask->Core, 0);
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
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pVM->pdm.s.pDevHlpQueueHC);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_IOAPIC_SET_IRQ;
            pTask->pDevInsHC = 0; /* not required */
            pTask->u.SetIRQ.iIrq = iIrq;
            pTask->u.SetIRQ.iLevel = iLevel;

            PDMQueueInsertEx(pVM->pdm.s.pDevHlpQueueHC, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}
