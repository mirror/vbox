/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, Device parts.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_DEVICE
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/iom.h>
#include <VBox/cfgm.h>
#include <VBox/rem.h>
#include <VBox/dbgf.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/hwaccm.h>

#include <VBox/version.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Internal callback structure pointer.
 * The main purpose is to define the extra data we associate
 * with PDMDEVREGCB so we can find the VM instance and so on.
 */
typedef struct PDMDEVREGCBINT
{
    /** The callback structure. */
    PDMDEVREGCB     Core;
    /** A bit of padding. */
    uint32_t        u32[4];
    /** VM Handle. */
    PVM             pVM;
} PDMDEVREGCBINT, *PPDMDEVREGCBINT;
typedef const PDMDEVREGCBINT *PCPDMDEVREGCBINT;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pdmR3DevReg_Register(PPDMDEVREGCB pCallbacks, PCPDMDEVREG pDevReg);
static DECLCALLBACK(void *) pdmR3DevReg_MMHeapAlloc(PPDMDEVREGCB pCallbacks, size_t cb);
static DECLCALLBACK(bool) pdmR3DevHlpQueueConsumer(PVM pVM, PPDMQUEUEITEMCORE pItem);

/* VSlick regex:
search : \om/\*\*.+?\*\/\nDECLCALLBACKMEMBER\(([^,]*), *pfn([^)]*)\)\(
replace: \/\*\* @copydoc PDMDEVHLP::pfn\2 \*\/\nstatic DECLCALLBACK\(\1\) pdmR3DevHlp_\2\(
 */

/** @name R3 DevHlp
 * @{
 */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser, PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn, PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegisterGC(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTGCPTR pvUser, const char *pszOut, const char *pszIn, const char *pszOutStr, const char *pszInStr, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegisterR0(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTR0PTR pvUser, const char *pszOut, const char *pszIn, const char *pszOutStr, const char *pszInStr, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_IOPortDeregister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts);
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                         PFNIOMMMIOWRITE pfnWrite, PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill,
                                         const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegisterGC(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                           const char *pszWrite, const char *pszRead, const char *pszFill,
                                           const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegisterR0(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                           const char *pszWrite, const char *pszRead, const char *pszFill,
                                           const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_MMIODeregister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange);
static DECLCALLBACK(int) pdmR3DevHlp_ROMRegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, bool fShadow, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_SSMRegister(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                        PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                        PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone);
static DECLCALLBACK(int) pdmR3DevHlp_TMTimerCreate(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer);
static DECLCALLBACK(PTMTIMERR3) pdmR3DevHlp_TMTimerCreateExternal(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_PCIRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev);
static DECLCALLBACK(int) pdmR3DevHlp_PCIIORegionRegister(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback);
static DECLCALLBACK(void) pdmR3DevHlp_PCISetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                            PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld);
static DECLCALLBACK(void) pdmR3DevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR3DevHlp_PCISetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR3DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR3DevHlp_ISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(int) pdmR3DevHlp_DriverAttach(PPDMDEVINS pDevIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc);
static DECLCALLBACK(void *) pdmR3DevHlp_MMHeapAlloc(PPDMDEVINS pDevIns, size_t cb);
static DECLCALLBACK(void *) pdmR3DevHlp_MMHeapAllocZ(PPDMDEVINS pDevIns, size_t cb);
static DECLCALLBACK(void) pdmR3DevHlp_MMHeapFree(PPDMDEVINS pDevIns, void *pv);
static DECLCALLBACK(int) pdmR3DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...);
static DECLCALLBACK(int) pdmR3DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va);
static DECLCALLBACK(int) pdmR3DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...);
static DECLCALLBACK(int) pdmR3DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va);
static DECLCALLBACK(bool) pdmR3DevHlp_AssertEMT(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction);
static DECLCALLBACK(bool) pdmR3DevHlp_AssertOther(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction);
static DECLCALLBACK(int) pdmR3DevHlp_DBGFStopV(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction, const char *pszFormat, va_list args);
static DECLCALLBACK(int) pdmR3DevHlp_DBGFInfoRegister(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler);
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegister(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc);
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegisterF(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...);
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegisterV(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args);
static DECLCALLBACK(int) pdmR3DevHlp_CritSectInit(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName);
static DECLCALLBACK(PRTTIMESPEC) pdmR3DevHlp_UTCNow(PPDMDEVINS pDevIns, PRTTIMESPEC pTime);
static DECLCALLBACK(int) pdmR3DevHlp_PDMThreadCreate(PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                                     PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName);

static DECLCALLBACK(PVM) pdmR3DevHlp_GetVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_PCIBusRegister(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_PICRegister(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_APICRegister(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_IOAPICRegister(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_DMACRegister(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp);
static DECLCALLBACK(int) pdmR3DevHlp_RTCRegister(PPDMDEVINS pDevIns, PCPDMRTCREG pRtcReg, PCPDMRTCHLP *ppRtcHlp);
static DECLCALLBACK(int) pdmR3DevHlp_PDMQueueCreate(PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval, PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);
static DECLCALLBACK(void) pdmR3DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
static DECLCALLBACK(void) pdmR3DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(int) pdmR3DevHlp_PhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb);
static DECLCALLBACK(int) pdmR3DevHlp_PhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb);
static DECLCALLBACK(int) pdmR3DevHlp_PhysReserve(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_PhysGCPtr2GCPhys(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTGCPHYS pGCPhys);
static DECLCALLBACK(bool) pdmR3DevHlp_A20IsEnabled(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR3DevHlp_A20Set(PPDMDEVINS pDevIns, bool fEnable);
static DECLCALLBACK(int) pdmR3DevHlp_VMReset(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_VMPowerOff(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_VMSuspend(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmR3DevHlp_LockVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmR3DevHlp_UnlockVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(bool) pdmR3DevHlp_AssertVMLock(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction);
static DECLCALLBACK(int) pdmR3DevHlp_DMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser);
static DECLCALLBACK(int) pdmR3DevHlp_DMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead);
static DECLCALLBACK(int) pdmR3DevHlp_DMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten);
static DECLCALLBACK(int) pdmR3DevHlp_DMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel);
static DECLCALLBACK(uint8_t) pdmR3DevHlp_DMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel);
static DECLCALLBACK(void) pdmR3DevHlp_DMASchedule(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_CMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value);
static DECLCALLBACK(int) pdmR3DevHlp_CMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value);
static DECLCALLBACK(void) pdmR3DevHlp_GetCpuId(PPDMDEVINS pDevIns, uint32_t iLeaf,
                                               uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
static DECLCALLBACK(int) pdmR3DevHlp_ROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange);

static DECLCALLBACK(PVM) pdmR3DevHlp_Untrusted_GetVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PCIBusRegister(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PICRegister(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_APICRegister(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_IOAPICRegister(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMACRegister(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp);
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysReserve(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_Obsolete_Phys2HCVirt(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR ppvHC);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_Obsolete_PhysGCPtr2HCPtr(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTHCPTR pHCPtr);

static DECLCALLBACK(bool) pdmR3DevHlp_Untrusted_A20IsEnabled(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_A20Set(PPDMDEVINS pDevIns, bool fEnable);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMReset(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMPowerOff(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMSuspend(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmR3DevHlp_Untrusted_LockVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(int)  pdmR3DevHlp_Untrusted_UnlockVM(PPDMDEVINS pDevIns);
static DECLCALLBACK(bool) pdmR3DevHlp_Untrusted_AssertVMLock(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel);
static DECLCALLBACK(uint8_t) pdmR3DevHlp_Untrusted_DMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel);
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_DMASchedule(PPDMDEVINS pDevIns);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_CMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_CMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value);
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_QueryCPUId(PPDMDEVINS pDevIns, uint32_t iLeaf,
                                                           uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_ROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange);
/** @} */


/** @name HC PIC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR3PicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR3PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR3PicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR3PicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
static DECLCALLBACK(PCPDMPICHLPGC) pdmR3PicHlp_GetGCHelpers(PPDMDEVINS pDevIns);
static DECLCALLBACK(PCPDMPICHLPR0) pdmR3PicHlp_GetR0Helpers(PPDMDEVINS pDevIns);
/** @} */


/** @name HC APIC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR3ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR3ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns);
static DECLCALLBACK(void) pdmR3ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, bool fEnabled);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR3ApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR3ApicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
static DECLCALLBACK(PCPDMAPICHLPGC) pdmR3ApicHlp_GetGCHelpers(PPDMDEVINS pDevIns);
static DECLCALLBACK(PCPDMAPICHLPR0) pdmR3ApicHlp_GetR0Helpers(PPDMDEVINS pDevIns);
/** @} */


/** @name HC I/O APIC Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR3IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR3IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR3IoApicHlp_Unlock(PPDMDEVINS pDevIns);
#endif
static DECLCALLBACK(PCPDMIOAPICHLPGC) pdmR3IoApicHlp_GetGCHelpers(PPDMDEVINS pDevIns);
static DECLCALLBACK(PCPDMIOAPICHLPR0) pdmR3IoApicHlp_GetR0Helpers(PPDMDEVINS pDevIns);
/** @} */


/** @name HC PCI Bus Helpers
 * @{
 */
static DECLCALLBACK(void) pdmR3PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
static DECLCALLBACK(void) pdmR3PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel);
#ifdef VBOX_WITH_PDM_LOCK
static DECLCALLBACK(int) pdmR3PciHlp_Lock(PPDMDEVINS pDevIns, int rc);
static DECLCALLBACK(void) pdmR3PciHlp_Unlock(PPDMDEVINS pDevIns);
#endif
static DECLCALLBACK(PCPDMPCIHLPGC) pdmR3PciHlp_GetGCHelpers(PPDMDEVINS pDevIns);
static DECLCALLBACK(PCPDMPCIHLPR0) pdmR3PciHlp_GetR0Helpers(PPDMDEVINS pDevIns);
/** @} */

/** @def PDMDEV_ASSERT_DEVINS
 * Asserts the validity of the driver instance.
 */
#ifdef VBOX_STRICT
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { Assert(pDevIns); Assert(pDevIns->u32Version == PDM_DEVINS_VERSION); Assert(pDevIns->pvInstanceDataR3 == (void *)&pDevIns->achInstanceData[0]); } while (0)
#else
# define PDMDEV_ASSERT_DEVINS(pDevIns)   do { } while (0)
#endif
static int pdmR3DevLoad(PVM pVM, PPDMDEVREGCBINT pRegCB, const char *pszFilename, const char *pszName);


/*
 * Allow physical read and writes from any thread
 */
#define PDM_PHYS_READWRITE_FROM_ANY_THREAD

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * The device helper structure for trusted devices.
 */
const PDMDEVHLP g_pdmR3DevHlpTrusted =
{
    PDM_DEVHLP_VERSION,
    pdmR3DevHlp_IOPortRegister,
    pdmR3DevHlp_IOPortRegisterGC,
    pdmR3DevHlp_IOPortRegisterR0,
    pdmR3DevHlp_IOPortDeregister,
    pdmR3DevHlp_MMIORegister,
    pdmR3DevHlp_MMIORegisterGC,
    pdmR3DevHlp_MMIORegisterR0,
    pdmR3DevHlp_MMIODeregister,
    pdmR3DevHlp_ROMRegister,
    pdmR3DevHlp_SSMRegister,
    pdmR3DevHlp_TMTimerCreate,
    pdmR3DevHlp_TMTimerCreateExternal,
    pdmR3DevHlp_PCIRegister,
    pdmR3DevHlp_PCIIORegionRegister,
    pdmR3DevHlp_PCISetConfigCallbacks,
    pdmR3DevHlp_PCISetIrq,
    pdmR3DevHlp_PCISetIrqNoWait,
    pdmR3DevHlp_ISASetIrq,
    pdmR3DevHlp_ISASetIrqNoWait,
    pdmR3DevHlp_DriverAttach,
    pdmR3DevHlp_MMHeapAlloc,
    pdmR3DevHlp_MMHeapAllocZ,
    pdmR3DevHlp_MMHeapFree,
    pdmR3DevHlp_VMSetError,
    pdmR3DevHlp_VMSetErrorV,
    pdmR3DevHlp_VMSetRuntimeError,
    pdmR3DevHlp_VMSetRuntimeErrorV,
    pdmR3DevHlp_AssertEMT,
    pdmR3DevHlp_AssertOther,
    pdmR3DevHlp_DBGFStopV,
    pdmR3DevHlp_DBGFInfoRegister,
    pdmR3DevHlp_STAMRegister,
    pdmR3DevHlp_STAMRegisterF,
    pdmR3DevHlp_STAMRegisterV,
    pdmR3DevHlp_RTCRegister,
    pdmR3DevHlp_PDMQueueCreate,
    pdmR3DevHlp_CritSectInit,
    pdmR3DevHlp_UTCNow,
    pdmR3DevHlp_PDMThreadCreate,
    pdmR3DevHlp_PhysGCPtr2GCPhys,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    pdmR3DevHlp_GetVM,
    pdmR3DevHlp_PCIBusRegister,
    pdmR3DevHlp_PICRegister,
    pdmR3DevHlp_APICRegister,
    pdmR3DevHlp_IOAPICRegister,
    pdmR3DevHlp_DMACRegister,
    pdmR3DevHlp_PhysRead,
    pdmR3DevHlp_PhysWrite,
    pdmR3DevHlp_PhysReadGCVirt,
    pdmR3DevHlp_PhysWriteGCVirt,
    pdmR3DevHlp_PhysReserve,
    pdmR3DevHlp_Untrusted_Obsolete_Phys2HCVirt,
    pdmR3DevHlp_Untrusted_Obsolete_PhysGCPtr2HCPtr,
    pdmR3DevHlp_A20IsEnabled,
    pdmR3DevHlp_A20Set,
    pdmR3DevHlp_VMReset,
    pdmR3DevHlp_VMSuspend,
    pdmR3DevHlp_VMPowerOff,
    pdmR3DevHlp_LockVM,
    pdmR3DevHlp_UnlockVM,
    pdmR3DevHlp_AssertVMLock,
    pdmR3DevHlp_DMARegister,
    pdmR3DevHlp_DMAReadMemory,
    pdmR3DevHlp_DMAWriteMemory,
    pdmR3DevHlp_DMASetDREQ,
    pdmR3DevHlp_DMAGetChannelMode,
    pdmR3DevHlp_DMASchedule,
    pdmR3DevHlp_CMOSWrite,
    pdmR3DevHlp_CMOSRead,
    pdmR3DevHlp_GetCpuId,
    pdmR3DevHlp_ROMProtectShadow,
    PDM_DEVHLP_VERSION /* the end */
};


/**
 * The device helper structure for non-trusted devices.
 */
const PDMDEVHLP g_pdmR3DevHlpUnTrusted =
{
    PDM_DEVHLP_VERSION,
    pdmR3DevHlp_IOPortRegister,
    pdmR3DevHlp_IOPortRegisterGC,
    pdmR3DevHlp_IOPortRegisterR0,
    pdmR3DevHlp_IOPortDeregister,
    pdmR3DevHlp_MMIORegister,
    pdmR3DevHlp_MMIORegisterGC,
    pdmR3DevHlp_MMIORegisterR0,
    pdmR3DevHlp_MMIODeregister,
    pdmR3DevHlp_ROMRegister,
    pdmR3DevHlp_SSMRegister,
    pdmR3DevHlp_TMTimerCreate,
    pdmR3DevHlp_TMTimerCreateExternal,
    pdmR3DevHlp_PCIRegister,
    pdmR3DevHlp_PCIIORegionRegister,
    pdmR3DevHlp_PCISetConfigCallbacks,
    pdmR3DevHlp_PCISetIrq,
    pdmR3DevHlp_PCISetIrqNoWait,
    pdmR3DevHlp_ISASetIrq,
    pdmR3DevHlp_ISASetIrqNoWait,
    pdmR3DevHlp_DriverAttach,
    pdmR3DevHlp_MMHeapAlloc,
    pdmR3DevHlp_MMHeapAllocZ,
    pdmR3DevHlp_MMHeapFree,
    pdmR3DevHlp_VMSetError,
    pdmR3DevHlp_VMSetErrorV,
    pdmR3DevHlp_VMSetRuntimeError,
    pdmR3DevHlp_VMSetRuntimeErrorV,
    pdmR3DevHlp_AssertEMT,
    pdmR3DevHlp_AssertOther,
    pdmR3DevHlp_DBGFStopV,
    pdmR3DevHlp_DBGFInfoRegister,
    pdmR3DevHlp_STAMRegister,
    pdmR3DevHlp_STAMRegisterF,
    pdmR3DevHlp_STAMRegisterV,
    pdmR3DevHlp_RTCRegister,
    pdmR3DevHlp_PDMQueueCreate,
    pdmR3DevHlp_CritSectInit,
    pdmR3DevHlp_UTCNow,
    pdmR3DevHlp_PDMThreadCreate,
    pdmR3DevHlp_PhysGCPtr2GCPhys,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    pdmR3DevHlp_Untrusted_GetVM,
    pdmR3DevHlp_Untrusted_PCIBusRegister,
    pdmR3DevHlp_Untrusted_PICRegister,
    pdmR3DevHlp_Untrusted_APICRegister,
    pdmR3DevHlp_Untrusted_IOAPICRegister,
    pdmR3DevHlp_Untrusted_DMACRegister,
    pdmR3DevHlp_Untrusted_PhysRead,
    pdmR3DevHlp_Untrusted_PhysWrite,
    pdmR3DevHlp_Untrusted_PhysReadGCVirt,
    pdmR3DevHlp_Untrusted_PhysWriteGCVirt,
    pdmR3DevHlp_Untrusted_PhysReserve,
    pdmR3DevHlp_Untrusted_Obsolete_Phys2HCVirt,
    pdmR3DevHlp_Untrusted_Obsolete_PhysGCPtr2HCPtr,
    pdmR3DevHlp_Untrusted_A20IsEnabled,
    pdmR3DevHlp_Untrusted_A20Set,
    pdmR3DevHlp_Untrusted_VMReset,
    pdmR3DevHlp_Untrusted_VMSuspend,
    pdmR3DevHlp_Untrusted_VMPowerOff,
    pdmR3DevHlp_Untrusted_LockVM,
    pdmR3DevHlp_Untrusted_UnlockVM,
    pdmR3DevHlp_Untrusted_AssertVMLock,
    pdmR3DevHlp_Untrusted_DMARegister,
    pdmR3DevHlp_Untrusted_DMAReadMemory,
    pdmR3DevHlp_Untrusted_DMAWriteMemory,
    pdmR3DevHlp_Untrusted_DMASetDREQ,
    pdmR3DevHlp_Untrusted_DMAGetChannelMode,
    pdmR3DevHlp_Untrusted_DMASchedule,
    pdmR3DevHlp_Untrusted_CMOSWrite,
    pdmR3DevHlp_Untrusted_CMOSRead,
    pdmR3DevHlp_Untrusted_QueryCPUId,
    pdmR3DevHlp_Untrusted_ROMProtectShadow,
    PDM_DEVHLP_VERSION /* the end */
};


/**
 * PIC Device Helpers.
 */
const PDMPICHLPR3 g_pdmR3DevPicHlp =
{
    PDM_PICHLPR3_VERSION,
    pdmR3PicHlp_SetInterruptFF,
    pdmR3PicHlp_ClearInterruptFF,
#ifdef VBOX_WITH_PDM_LOCK
    pdmR3PicHlp_Lock,
    pdmR3PicHlp_Unlock,
#endif
    pdmR3PicHlp_GetGCHelpers,
    pdmR3PicHlp_GetR0Helpers,
    PDM_PICHLPR3_VERSION /* the end */
};


/**
 * APIC Device Helpers.
 */
const PDMAPICHLPR3 g_pdmR3DevApicHlp =
{
    PDM_APICHLPR3_VERSION,
    pdmR3ApicHlp_SetInterruptFF,
    pdmR3ApicHlp_ClearInterruptFF,
    pdmR3ApicHlp_ChangeFeature,
#ifdef VBOX_WITH_PDM_LOCK
    pdmR3ApicHlp_Lock,
    pdmR3ApicHlp_Unlock,
#endif
    pdmR3ApicHlp_GetGCHelpers,
    pdmR3ApicHlp_GetR0Helpers,
    PDM_APICHLPR3_VERSION /* the end */
};


/**
 * I/O APIC Device Helpers.
 */
const PDMIOAPICHLPR3 g_pdmR3DevIoApicHlp =
{
    PDM_IOAPICHLPR3_VERSION,
    pdmR3IoApicHlp_ApicBusDeliver,
#ifdef VBOX_WITH_PDM_LOCK
    pdmR3IoApicHlp_Lock,
    pdmR3IoApicHlp_Unlock,
#endif
    pdmR3IoApicHlp_GetGCHelpers,
    pdmR3IoApicHlp_GetR0Helpers,
    PDM_IOAPICHLPR3_VERSION /* the end */
};


/**
 * PCI Bus Device Helpers.
 */
const PDMPCIHLPR3 g_pdmR3DevPciHlp =
{
    PDM_PCIHLPR3_VERSION,
    pdmR3PciHlp_IsaSetIrq,
    pdmR3PciHlp_IoApicSetIrq,
#ifdef VBOX_WITH_PDM_LOCK
    pdmR3PciHlp_Lock,
    pdmR3PciHlp_Unlock,
#endif
    pdmR3PciHlp_GetGCHelpers,
    pdmR3PciHlp_GetR0Helpers,
    PDM_PCIHLPR3_VERSION, /* the end */
};


/**
 * DMAC Device Helpers.
 */
const PDMDMACHLP g_pdmR3DevDmacHlp =
{
    PDM_DMACHLP_VERSION
};


/**
 * RTC Device Helpers.
 */
const PDMRTCHLP g_pdmR3DevRtcHlp =
{
    PDM_RTCHLP_VERSION
};


/**
 * This function will initialize the devices for this VM instance.
 *
 *
 * First of all this mean loading the builtin device and letting them
 * register themselves. Beyond that any additional device modules are
 * loaded and called for registration.
 *
 * Then the device configuration is enumerated, the instantiation order
 * is determined, and finally they are instantiated.
 *
 * After all device have been successfully instantiated the the primary
 * PCI Bus device is called to emulate the PCI BIOS, i.e. making the
 * resource assignments. If there is no PCI device, this step is of course
 * skipped.
 *
 * Finally the init completion routines of the instantiated devices
 * are called.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 */
int pdmR3DevInit(PVM pVM)
{
    LogFlow(("pdmR3DevInit:\n"));

    AssertRelease(!(RT_OFFSETOF(PDMDEVINS, achInstanceData) & 15));
    AssertRelease(sizeof(pVM->pdm.s.pDevInstances->Internal.s) <= sizeof(pVM->pdm.s.pDevInstances->Internal.padding));

    /*
     * Get the GC & R0 devhlps and create the devhlp R3 task queue.
     */
    GCPTRTYPE(PCPDMDEVHLPGC) pDevHlpGC;
    int rc = PDMR3GetSymbolGC(pVM, NULL, "g_pdmGCDevHlp", &pDevHlpGC);
    AssertReleaseRCReturn(rc, rc);

    R0PTRTYPE(PCPDMDEVHLPR0) pDevHlpR0;
    rc = PDMR3GetSymbolR0(pVM, NULL, "g_pdmR0DevHlp", &pDevHlpR0);
    AssertReleaseRCReturn(rc, rc);

    rc = PDMR3QueueCreateInternal(pVM, sizeof(PDMDEVHLPTASK), 8, 0, pdmR3DevHlpQueueConsumer, true, &pVM->pdm.s.pDevHlpQueueHC);
    AssertRCReturn(rc, rc);
    pVM->pdm.s.pDevHlpQueueGC = PDMQueueGCPtr(pVM->pdm.s.pDevHlpQueueHC);


    /*
     * Initialize the callback structure.
     */
    PDMDEVREGCBINT RegCB;
    RegCB.Core.u32Version = PDM_DEVREG_CB_VERSION;
    RegCB.Core.pfnRegister = pdmR3DevReg_Register;
    RegCB.Core.pfnMMHeapAlloc = pdmR3DevReg_MMHeapAlloc;
    RegCB.pVM = pVM;

    /*
     * Load the builtin module
     */
    PCFGMNODE pDevicesNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "PDM/Devices");
    bool fLoadBuiltin;
    rc = CFGMR3QueryBool(pDevicesNode, "LoadBuiltin", &fLoadBuiltin);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        fLoadBuiltin = true;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Querying boolean \"LoadBuiltin\" failed with %Vrc\n", rc));
        return rc;
    }
    if (fLoadBuiltin)
    {
        /* make filename */
        char *pszFilename = pdmR3FileR3("VBoxDD", /* fShared = */ true);
        if (!pszFilename)
            return VERR_NO_TMP_MEMORY;
        rc = pdmR3DevLoad(pVM, &RegCB, pszFilename, "VBoxDD");
        RTMemTmpFree(pszFilename);
        if (VBOX_FAILURE(rc))
            return rc;

        /* make filename */
        pszFilename = pdmR3FileR3("VBoxDD2", /* fShared = */ true);
        if (!pszFilename)
            return VERR_NO_TMP_MEMORY;
        rc = pdmR3DevLoad(pVM, &RegCB, pszFilename, "VBoxDD2");
        RTMemTmpFree(pszFilename);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    /*
     * Load additional device modules.
     */
    PCFGMNODE pCur;
    for (pCur = CFGMR3GetFirstChild(pDevicesNode); pCur; pCur = CFGMR3GetNextChild(pCur))
    {
        /*
         * Get the name and path.
         */
        char szName[PDMMOD_NAME_LEN];
        rc = CFGMR3GetName(pCur, &szName[0], sizeof(szName));
        if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
        {
            AssertMsgFailed(("configuration error: The module name is too long, cchName=%d.\n", CFGMR3GetNameLen(pCur)));
            return VERR_PDM_MODULE_NAME_TOO_LONG;
        }
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("CFGMR3GetName -> %Vrc.\n", rc));
            return rc;
        }

        /* the path is optional, if no path the module name + path is used. */
        char szFilename[RTPATH_MAX];
        rc = CFGMR3QueryString(pCur, "Path", &szFilename[0], sizeof(szFilename));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
            strcpy(szFilename, szName);
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("configuration error: Failure to query the module path, rc=%Vrc.\n", rc));
            return rc;
        }

        /* prepend path? */
        if (!RTPathHavePath(szFilename))
        {
            char *psz = pdmR3FileR3(szFilename);
            if (!psz)
                return VERR_NO_TMP_MEMORY;
            size_t cch = strlen(psz) + 1;
            if (cch > sizeof(szFilename))
            {
                RTMemTmpFree(psz);
                AssertMsgFailed(("Filename too long! cch=%d '%s'\n", cch, psz));
                return VERR_FILENAME_TOO_LONG;
            }
            memcpy(szFilename, psz, cch);
            RTMemTmpFree(psz);
        }

        /*
         * Load the module and register it's devices.
         */
        rc = pdmR3DevLoad(pVM, &RegCB, szFilename, szName);
        if (VBOX_FAILURE(rc))
            return rc;
    }

#ifdef VBOX_WITH_USB
    /* ditto for USB Devices. */
    rc = pdmR3UsbLoadModules(pVM);
    if (RT_FAILURE(rc))
        return rc;
#endif


    /*
     *
     * Enumerate the device instance configurations
     * and come up with a instantiation order.
     *
     */
    /* Switch to /Devices, which contains the device instantiations. */
    pDevicesNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "Devices");

    /*
     * Count the device instances.
     */
    PCFGMNODE pInstanceNode;
    unsigned cDevs = 0;
    for (pCur = CFGMR3GetFirstChild(pDevicesNode); pCur; pCur = CFGMR3GetNextChild(pCur))
        for (pInstanceNode = CFGMR3GetFirstChild(pCur); pInstanceNode; pInstanceNode = CFGMR3GetNextChild(pInstanceNode))
            cDevs++;
    if (!cDevs)
    {
        Log(("PDM: No devices were configured!\n"));
        return VINF_SUCCESS;
    }
    Log2(("PDM: cDevs=%d!\n", cDevs));

    /*
     * Collect info on each device instance.
     */
    struct DEVORDER
    {
        /** Configuration node. */
        PCFGMNODE   pNode;
        /** Pointer to device. */
        PPDMDEV     pDev;
        /** Init order. */
        uint32_t    u32Order;
        /** VBox instance number. */
        uint32_t    iInstance;
    } *paDevs = (struct DEVORDER *)alloca(sizeof(paDevs[0]) * (cDevs + 1)); /* (One extra for swapping) */
    Assert(paDevs);
    unsigned i = 0;
    for (pCur = CFGMR3GetFirstChild(pDevicesNode); pCur; pCur = CFGMR3GetNextChild(pCur))
    {
        /* Get the device name. */
        char szName[sizeof(paDevs[0].pDev->pDevReg->szDeviceName)];
        rc = CFGMR3GetName(pCur, szName, sizeof(szName));
        AssertMsgRCReturn(rc, ("Configuration error: device name is too long (or something)! rc=%Vrc\n", rc), rc);

        /* Find the device. */
        PPDMDEV pDev = pdmR3DevLookup(pVM, szName);
        AssertMsgReturn(pDev, ("Configuration error: device '%s' not found!\n", szName), VERR_PDM_DEVICE_NOT_FOUND);

        /* Configured priority or use default based on device class? */
        uint32_t u32Order;
        rc = CFGMR3QueryU32(pCur, "Priority", &u32Order);
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            uint32_t u32 = pDev->pDevReg->fClass;
            for (u32Order = 1; !(u32 & u32Order); u32Order <<= 1)
                /* nop */;
        }
        else
            AssertMsgRCReturn(rc, ("Configuration error: reading \"Priority\" for the '%s' device failed rc=%Vrc!\n", szName, rc), rc);

        /* Enumerate the device instances. */
        for (pInstanceNode = CFGMR3GetFirstChild(pCur); pInstanceNode; pInstanceNode = CFGMR3GetNextChild(pInstanceNode))
        {
            paDevs[i].pNode = pInstanceNode;
            paDevs[i].pDev = pDev;
            paDevs[i].u32Order = u32Order;

            /* Get the instance number. */
            char szInstance[32];
            rc = CFGMR3GetName(pInstanceNode, szInstance, sizeof(szInstance));
            AssertMsgRCReturn(rc, ("Configuration error: instance name is too long (or something)! rc=%Vrc\n", rc), rc);
            char *pszNext = NULL;
            rc = RTStrToUInt32Ex(szInstance, &pszNext, 0, &paDevs[i].iInstance);
            AssertMsgRCReturn(rc, ("Configuration error: RTStrToInt32Ex failed on the instance name '%s'! rc=%Vrc\n", szInstance, rc), rc);
            AssertMsgReturn(!*pszNext, ("Configuration error: the instance name '%s' isn't all digits. (%s)\n", szInstance, pszNext), VERR_INVALID_PARAMETER);

            /* next instance */
            i++;
        }
    } /* devices */
    Assert(i == cDevs);

    /*
     * Sort the device array ascending on u32Order. (bubble)
     */
    unsigned c = cDevs - 1;
    while (c)
    {
        unsigned j = 0;
        for (i = 0; i < c; i++)
            if (paDevs[i].u32Order > paDevs[i + 1].u32Order)
            {
                paDevs[cDevs] = paDevs[i + 1];
                paDevs[i + 1] = paDevs[i];
                paDevs[i] = paDevs[cDevs];
                j = i;
            }
        c = j;
    }


    /*
     *
     * Instantiate the devices.
     *
     */
    for (i = 0; i < cDevs; i++)
    {
        /*
         * Gather a bit of config.
         */
        /* trusted */
        bool fTrusted;
        rc = CFGMR3QueryBool(paDevs[i].pNode, "Trusted", &fTrusted);
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
            fTrusted = false;
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("configuration error: failed to query boolean \"Trusted\", rc=%Vrc\n", rc));
            return rc;
        }
        /* config node */
        PCFGMNODE pConfigNode = CFGMR3GetChild(paDevs[i].pNode, "Config");
        if (!pConfigNode)
        {
            rc = CFGMR3InsertNode(paDevs[i].pNode, "Config", &pConfigNode);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("Failed to create Config node! rc=%Vrc\n", rc));
                return rc;
            }
        }
        CFGMR3SetRestrictedRoot(pConfigNode);

        /*
         * Allocate the device instance.
         */
        size_t cb = RT_OFFSETOF(PDMDEVINS, achInstanceData[paDevs[i].pDev->pDevReg->cbInstance]);
        cb = RT_ALIGN_Z(cb, 16);
        PPDMDEVINS pDevIns;
        if (paDevs[i].pDev->pDevReg->fFlags & (PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0))
            rc = MMR3HyperAllocOnceNoRel(pVM, cb, 0, MM_TAG_PDM_DEVICE, (void **)&pDevIns);
        else
            rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_DEVICE, cb, (void **)&pDevIns);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("Failed to allocate %d bytes of instance data for device '%s'. rc=%Vrc\n",
                             cb, paDevs[i].pDev->pDevReg->szDeviceName, rc));
            return rc;
        }

        /*
         * Initialize it.
         */
        pDevIns->u32Version                     = PDM_DEVINS_VERSION;
        //pDevIns->Internal.s.pNextHC             = NULL;
        //pDevIns->Internal.s.pPerDeviceNextHC    = NULL;
        pDevIns->Internal.s.pDevHC              = paDevs[i].pDev;
        pDevIns->Internal.s.pVMHC               = pVM;
        pDevIns->Internal.s.pVMGC               = pVM->pVMGC;
        //pDevIns->Internal.s.pLunsHC             = NULL;
        pDevIns->Internal.s.pCfgHandle          = paDevs[i].pNode;
        //pDevIns->Internal.s.pPciDevice          = NULL;
        //pDevIns->Internal.s.pPciBus             = NULL; /** @todo pci bus selection. (in 2008 perhaps) */
        pDevIns->pDevHlp                        = fTrusted ? &g_pdmR3DevHlpTrusted : &g_pdmR3DevHlpUnTrusted;
        pDevIns->pDevHlpGC                      = pDevHlpGC;
        pDevIns->pDevHlpR0                      = pDevHlpR0;
        pDevIns->pDevReg                        = paDevs[i].pDev->pDevReg;
        pDevIns->pCfgHandle                     = pConfigNode;
        pDevIns->iInstance                      = paDevs[i].iInstance;
        pDevIns->pvInstanceDataR3               = &pDevIns->achInstanceData[0];
        pDevIns->pvInstanceDataGC               =  pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC
                                                   ? MMHyperHC2GC(pVM, pDevIns->pvInstanceDataR3) : 0;
        pDevIns->pvInstanceDataR0               =  pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0
                                                   ? MMHyperR3ToR0(pVM, pDevIns->pvInstanceDataR3) : 0;

        /*
         * Link it into all the lists.
         */
        /* The global instance FIFO. */
        PPDMDEVINS pPrev1 = pVM->pdm.s.pDevInstances;
        if (!pPrev1)
            pVM->pdm.s.pDevInstances = pDevIns;
        else
        {
            while (pPrev1->Internal.s.pNextHC)
                pPrev1 = pPrev1->Internal.s.pNextHC;
            pPrev1->Internal.s.pNextHC = pDevIns;
        }

        /* The per device instance FIFO. */
        PPDMDEVINS pPrev2 = paDevs[i].pDev->pInstances;
        if (!pPrev2)
            paDevs[i].pDev->pInstances = pDevIns;
        else
        {
            while (pPrev2->Internal.s.pPerDeviceNextHC)
                pPrev2 = pPrev2->Internal.s.pPerDeviceNextHC;
            pPrev2->Internal.s.pPerDeviceNextHC = pDevIns;
        }

        /*
         * Call the constructor.
         */
        Log(("PDM: Constructing device '%s' instance %d...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        rc = pDevIns->pDevReg->pfnConstruct(pDevIns, pDevIns->iInstance, pDevIns->pCfgHandle);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("Failed to construct '%s'/%d! %Vra\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            /* because we're damn lazy right now, we'll say that the destructor will be called even if the constructor fails. */
            return rc;
        }
    } /* for device instances */

#ifdef VBOX_WITH_USB
    /* ditto for USB Devices. */
    rc = pdmR3UsbInstantiateDevices(pVM);
    if (RT_FAILURE(rc))
        return rc;
#endif


    /*
     *
     * PCI BIOS Fake and Init Complete.
     *
     */
    if (pVM->pdm.s.aPciBuses[0].pDevInsR3)
    {
        pdmLock(pVM);
        rc = pVM->pdm.s.aPciBuses[0].pfnFakePCIBIOSR3(pVM->pdm.s.aPciBuses[0].pDevInsR3);
        pdmUnlock(pVM);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("PCI BIOS fake failed rc=%Vrc\n", rc));
            return rc;
        }
    }

    for (PPDMDEVINS pDevIns = pVM->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextHC)
    {
        if (pDevIns->pDevReg->pfnInitComplete)
        {
            rc = pDevIns->pDevReg->pfnInitComplete(pDevIns);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("InitComplete on device '%s'/%d failed with rc=%Vrc\n",
                                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
                return rc;
            }
        }
    }

#ifdef VBOX_WITH_USB
    /* ditto for USB Devices. */
    rc = pdmR3UsbInitComplete(pVM);
    if (RT_FAILURE(rc))
        return rc;
#endif

    LogFlow(("pdmR3DevInit: returns %Vrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}


/**
 * Lookups a device structure by name.
 * @internal
 */
PPDMDEV pdmR3DevLookup(PVM pVM, const char *pszName)
{
    RTUINT cchName = strlen(pszName);
    for (PPDMDEV pDev = pVM->pdm.s.pDevs; pDev; pDev = pDev->pNext)
        if (    pDev->cchName == cchName
            &&  !strcmp(pDev->pDevReg->szDeviceName, pszName))
            return pDev;
    return NULL;
}


/**
 * Loads one device module and call the registration entry point.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pRegCB          The registration callback stuff.
 * @param   pszFilename     Module filename.
 * @param   pszName         Module name.
 */
static int pdmR3DevLoad(PVM pVM, PPDMDEVREGCBINT pRegCB, const char *pszFilename, const char *pszName)
{
    /*
     * Load it.
     */
    int rc = pdmR3LoadR3(pVM, pszFilename, pszName);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Get the registration export and call it.
         */
        FNPDMVBOXDEVICESREGISTER *pfnVBoxDevicesRegister;
        rc = PDMR3GetSymbolR3(pVM, pszName, "VBoxDevicesRegister", (void **)&pfnVBoxDevicesRegister);
        if (VBOX_SUCCESS(rc))
        {
            Log(("PDM: Calling VBoxDevicesRegister (%p) of %s (%s)\n", pfnVBoxDevicesRegister, pszName, pszFilename));
            rc = pfnVBoxDevicesRegister(&pRegCB->Core, VBOX_VERSION);
            if (VBOX_SUCCESS(rc))
                Log(("PDM: Successfully loaded device module %s (%s).\n", pszName, pszFilename));
            else
                AssertMsgFailed(("VBoxDevicesRegister failed with rc=%Vrc for module %s (%s)\n", rc, pszName, pszFilename));
        }
        else
        {
            AssertMsgFailed(("Failed to locate 'VBoxDevicesRegister' in %s (%s) rc=%Vrc\n", pszName, pszFilename, rc));
            if (rc == VERR_SYMBOL_NOT_FOUND)
                rc = VERR_PDM_NO_REGISTRATION_EXPORT;
        }
    }
    else
        AssertMsgFailed(("Failed to load %s %s!\n", pszFilename, pszName));
    return rc;
}



/**
 * Registers a device with the current VM instance.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   pDevReg         Pointer to the device registration record.
 *                          This data must be permanent and readonly.
 */
static DECLCALLBACK(int) pdmR3DevReg_Register(PPDMDEVREGCB pCallbacks, PCPDMDEVREG pDevReg)
{
    /*
     * Validate the registration structure.
     */
    Assert(pDevReg);
    if (pDevReg->u32Version != PDM_DEVREG_VERSION)
    {
        AssertMsgFailed(("Unknown struct version %#x!\n", pDevReg->u32Version));
        return VERR_PDM_UNKNOWN_DEVREG_VERSION;
    }
    if (    !pDevReg->szDeviceName[0]
        ||  strlen(pDevReg->szDeviceName) >= sizeof(pDevReg->szDeviceName))
    {
        AssertMsgFailed(("Invalid name '%s'\n", pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if (    (pDevReg->fFlags & PDM_DEVREG_FLAGS_GC)
        &&  (   !pDevReg->szGCMod[0]
             || strlen(pDevReg->szGCMod) >= sizeof(pDevReg->szGCMod)))
    {
        AssertMsgFailed(("Invalid GC module name '%s' - (Device %s)\n", pDevReg->szGCMod, pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if (    (pDevReg->fFlags & PDM_DEVREG_FLAGS_R0)
        &&  (   !pDevReg->szR0Mod[0]
             || strlen(pDevReg->szR0Mod) >= sizeof(pDevReg->szR0Mod)))
    {
        AssertMsgFailed(("Invalid R0 module name '%s' - (Device %s)\n", pDevReg->szR0Mod, pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if ((pDevReg->fFlags & PDM_DEVREG_FLAGS_HOST_BITS_MASK) != PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT)
    {
        AssertMsgFailed(("Invalid host bits flags! fFlags=%#x (Device %s)\n", pDevReg->fFlags, pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_HOST_BITS;
    }
    if (!(pDevReg->fFlags & PDM_DEVREG_FLAGS_GUEST_BITS_MASK))
    {
        AssertMsgFailed(("Invalid guest bits flags! fFlags=%#x (Device %s)\n", pDevReg->fFlags, pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if (!pDevReg->fClass)
    {
        AssertMsgFailed(("No class! (Device %s)\n", pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if (pDevReg->cMaxInstances <= 0)
    {
        AssertMsgFailed(("Max instances %u! (Device %s)\n", pDevReg->cMaxInstances, pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if (pDevReg->cbInstance > (RTUINT)(pDevReg->fFlags & (PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0)  ? 96 * _1K : _1M))
    {
        AssertMsgFailed(("Instance size %d bytes! (Device %s)\n", pDevReg->cbInstance, pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    if (!pDevReg->pfnConstruct)
    {
        AssertMsgFailed(("No constructore! (Device %s)\n", pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_REGISTRATION;
    }
    /* Check matching guest bits last without any asserting. Enables trial and error registration. */
    if (!(pDevReg->fFlags & PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT))
    {
        Log(("PDM: Rejected device '%s' because it didn't match the guest bits.\n", pDevReg->szDeviceName));
        return VERR_PDM_INVALID_DEVICE_GUEST_BITS;
    }

    /*
     * Check for duplicate and find FIFO entry at the same time.
     */
    PCPDMDEVREGCBINT pRegCB = (PCPDMDEVREGCBINT)pCallbacks;
    PPDMDEV pDevPrev = NULL;
    PPDMDEV pDev = pRegCB->pVM->pdm.s.pDevs;
    for (; pDev; pDevPrev = pDev, pDev = pDev->pNext)
    {
        if (!strcmp(pDev->pDevReg->szDeviceName, pDevReg->szDeviceName))
        {
            AssertMsgFailed(("Device '%s' already exists\n", pDevReg->szDeviceName));
            return VERR_PDM_DEVICE_NAME_CLASH;
        }
    }

    /*
     * Allocate new device structure and insert it into the list.
     */
    pDev = (PPDMDEV)MMR3HeapAlloc(pRegCB->pVM, MM_TAG_PDM_DEVICE, sizeof(*pDev));
    if (pDev)
    {
        pDev->pNext = NULL;
        pDev->cInstances = 0;
        pDev->pInstances = NULL;
        pDev->pDevReg = pDevReg;
        pDev->cchName = strlen(pDevReg->szDeviceName);

        if (pDevPrev)
            pDevPrev->pNext = pDev;
        else
            pRegCB->pVM->pdm.s.pDevs = pDev;
        Log(("PDM: Registered device '%s'\n", pDevReg->szDeviceName));
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Allocate memory which is associated with current VM instance
 * and automatically freed on it's destruction.
 *
 * @returns Pointer to allocated memory. The memory is *NOT* zero-ed.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   cb              Number of bytes to allocate.
 */
static DECLCALLBACK(void *) pdmR3DevReg_MMHeapAlloc(PPDMDEVREGCB pCallbacks, size_t cb)
{
    Assert(pCallbacks);
    Assert(pCallbacks->u32Version == PDM_DEVREG_CB_VERSION);
    LogFlow(("pdmR3DevReg_MMHeapAlloc: cb=%#x\n", cb));

    void *pv = MMR3HeapAlloc(((PPDMDEVREGCBINT)pCallbacks)->pVM, MM_TAG_PDM_DEVICE_USER, cb);

    LogFlow(("pdmR3DevReg_MMHeapAlloc: returns %p\n", pv));
    return pv;
}


/**
 * Queue consumer callback for internal component.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pVM         The VM handle.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
static DECLCALLBACK(bool) pdmR3DevHlpQueueConsumer(PVM pVM, PPDMQUEUEITEMCORE pItem)
{
    PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)pItem;
    LogFlow(("pdmR3DevHlpQueueConsumer: enmOp=%d pDevIns=%p\n", pTask->enmOp, pTask->pDevInsHC));
    switch (pTask->enmOp)
    {
        case PDMDEVHLPTASKOP_ISA_SET_IRQ:
            PDMIsaSetIrq(pVM, pTask->u.SetIRQ.iIrq, pTask->u.SetIRQ.iLevel);
            break;

        case PDMDEVHLPTASKOP_PCI_SET_IRQ:
            pdmR3DevHlp_PCISetIrq(pTask->pDevInsHC, pTask->u.SetIRQ.iIrq, pTask->u.SetIRQ.iLevel);
            break;

        case PDMDEVHLPTASKOP_IOAPIC_SET_IRQ:
            PDMIoApicSetIrq(pVM, pTask->u.SetIRQ.iIrq, pTask->u.SetIRQ.iLevel);
            break;

        default:
            AssertReleaseMsgFailed(("Invalid operation %d\n", pTask->enmOp));
            break;
    }
    return true;
}


/** @copydoc PDMDEVHLP::pfnIOPortRegister */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser, PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn,
                                                    PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_IOPortRegister: caller='%s'/%d: Port=%#x cPorts=%#x pvUser=%p pfnOut=%p pfnIn=%p pfnOutStr=%p pfnInStr=%p p32_tszDesc=%p:{%s}\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts, pvUser, pfnOut, pfnIn, pfnOutStr, pfnInStr, pszDesc, pszDesc));
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);

    int rc = IOMR3IOPortRegisterR3(pDevIns->Internal.s.pVMHC, pDevIns, Port, cPorts, pvUser, pfnOut, pfnIn, pfnOutStr, pfnInStr, pszDesc);

    LogFlow(("pdmR3DevHlp_IOPortRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnIOPortRegisterGC */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegisterGC(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTGCPTR pvUser,
                                                      const char *pszOut, const char *pszIn,
                                                      const char *pszOutStr, const char *pszInStr, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_IOPortRegister: caller='%s'/%d: Port=%#x cPorts=%#x pvUser=%p pszOut=%p:{%s} pszIn=%p:{%s} pszOutStr=%p:{%s} pszInStr=%p:{%s} pszDesc=%p:{%s}\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts, pvUser, pszOut, pszOut, pszIn, pszIn, pszOutStr, pszOutStr, pszInStr, pszInStr, pszDesc, pszDesc));

    /*
     * Resolve the functions (one of the can be NULL).
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szGCMod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC))
    {
        RTGCPTR GCPtrIn = 0;
        if (pszIn)
        {
            rc = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszIn, &GCPtrIn);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszIn)\n", pDevIns->pDevReg->szGCMod, pszIn));
        }
        RTGCPTR GCPtrOut = 0;
        if (pszOut && VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszOut, &GCPtrOut);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOut)\n", pDevIns->pDevReg->szGCMod, pszOut));
        }
        RTGCPTR GCPtrInStr = 0;
        if (pszInStr && VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszInStr, &GCPtrInStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszInStr)\n", pDevIns->pDevReg->szGCMod, pszInStr));
        }
        RTGCPTR GCPtrOutStr = 0;
        if (pszOutStr && VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszOutStr, &GCPtrOutStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOutStr)\n", pDevIns->pDevReg->szGCMod, pszOutStr));
        }

        if (VBOX_SUCCESS(rc))
            rc = IOMIOPortRegisterGC(pDevIns->Internal.s.pVMHC, pDevIns, Port, cPorts, pvUser, GCPtrOut, GCPtrIn, GCPtrOutStr, GCPtrInStr, pszDesc);
    }
    else
    {
        AssertMsgFailed(("No GC module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_IOPortRegisterGC: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnIOPortRegisterR0 */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegisterR0(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTR0PTR pvUser,
                                                      const char *pszOut, const char *pszIn,
                                                      const char *pszOutStr, const char *pszInStr, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_IOPortRegisterR0: caller='%s'/%d: Port=%#x cPorts=%#x pvUser=%p pszOut=%p:{%s} pszIn=%p:{%s} pszOutStr=%p:{%s} pszInStr=%p:{%s} pszDesc=%p:{%s}\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts, pvUser, pszOut, pszOut, pszIn, pszIn, pszOutStr, pszOutStr, pszInStr, pszInStr, pszDesc, pszDesc));

    if (!HWACCMR3IsAllowed(pDevIns->Internal.s.pVMHC))
        return VINF_SUCCESS; /* NOP */

    /*
     * Resolve the functions (one of the can be NULL).
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szR0Mod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0))
    {
        R0PTRTYPE(PFNIOMIOPORTIN) pfnR0PtrIn = 0;
        if (pszIn)
        {
            rc = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszIn, &pfnR0PtrIn);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszIn)\n", pDevIns->pDevReg->szR0Mod, pszIn));
        }
        R0PTRTYPE(PFNIOMIOPORTOUT) pfnR0PtrOut = 0;
        if (pszOut && VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszOut, &pfnR0PtrOut);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOut)\n", pDevIns->pDevReg->szR0Mod, pszOut));
        }
        R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnR0PtrInStr = 0;
        if (pszInStr && VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszInStr, &pfnR0PtrInStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszInStr)\n", pDevIns->pDevReg->szR0Mod, pszInStr));
        }
        R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnR0PtrOutStr = 0;
        if (pszOutStr && VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszOutStr, &pfnR0PtrOutStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOutStr)\n", pDevIns->pDevReg->szR0Mod, pszOutStr));
        }

        if (VBOX_SUCCESS(rc))
            rc = IOMIOPortRegisterR0(pDevIns->Internal.s.pVMHC, pDevIns, Port, cPorts, pvUser, pfnR0PtrOut, pfnR0PtrIn, pfnR0PtrOutStr, pfnR0PtrInStr, pszDesc);
    }
    else
    {
        AssertMsgFailed(("No R0 module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_IOPortRegisterR0: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnIOPortDeregister */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortDeregister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_IOPortDeregister: caller='%s'/%d: Port=%#x cPorts=%#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts));

    int rc = IOMR3IOPortDeregister(pDevIns->Internal.s.pVMHC, pDevIns, Port, cPorts);

    LogFlow(("pdmR3DevHlp_IOPortDeregister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnMMIORegister */
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                                  PFNIOMMMIOWRITE pfnWrite, PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill,
                                                  const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_MMIORegister: caller='%s'/%d: GCPhysStart=%VGp cbRange=%#x pvUser=%p pfnWrite=%p pfnRead=%p pfnFill=%p pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvUser, pfnWrite, pfnRead, pfnFill, pszDesc, pszDesc));

    int rc = IOMR3MMIORegisterR3(pDevIns->Internal.s.pVMHC, pDevIns, GCPhysStart, cbRange, pvUser, pfnWrite, pfnRead, pfnFill, pszDesc);

    LogFlow(("pdmR3DevHlp_MMIORegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnMMIORegisterGC */
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegisterGC(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                                    const char *pszWrite, const char *pszRead, const char *pszFill,
                                                    const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_MMIORegisterGC: caller='%s'/%d: GCPhysStart=%VGp cbRange=%#x pvUser=%p pszWrite=%p:{%s} pszRead=%p:{%s} pszFill=%p:{%s} pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvUser, pszWrite, pszWrite, pszRead, pszRead, pszFill, pszFill, pszDesc, pszDesc));

    /*
     * Resolve the functions.
     * Not all function have to present, leave it to IOM to enforce this.
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szGCMod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC))
    {
        RTGCPTR GCPtrWrite = 0;
        if (pszWrite)
            rc = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszWrite, &GCPtrWrite);
        RTGCPTR GCPtrRead = 0;
        int rc2 = VINF_SUCCESS;
        if (pszRead)
            rc2 = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszRead, &GCPtrRead);
        RTGCPTR GCPtrFill = 0;
        int rc3 = VINF_SUCCESS;
        if (pszFill)
            rc3 = PDMR3GetSymbolGCLazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szGCMod, pszFill, &GCPtrFill);
        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(rc2) && VBOX_SUCCESS(rc3))
            rc = IOMMMIORegisterGC(pDevIns->Internal.s.pVMHC, pDevIns, GCPhysStart, cbRange, pvUser, GCPtrWrite, GCPtrRead, GCPtrFill, pszDesc);
        else
        {
            AssertMsgRC(rc,  ("Failed to resolve %s.%s (pszWrite)\n", pDevIns->pDevReg->szGCMod, pszWrite));
            AssertMsgRC(rc2, ("Failed to resolve %s.%s (pszRead)\n",  pDevIns->pDevReg->szGCMod, pszRead));
            AssertMsgRC(rc3, ("Failed to resolve %s.%s (pszFill)\n",  pDevIns->pDevReg->szGCMod, pszFill));
            if (VBOX_FAILURE(rc2) && VBOX_SUCCESS(rc))
                rc = rc2;
            if (VBOX_FAILURE(rc3) && VBOX_SUCCESS(rc))
                rc = rc3;
        }
    }
    else
    {
        AssertMsgFailed(("No GC module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_MMIORegisterGC: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLP::pfnMMIORegisterR0 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegisterR0(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                                    const char *pszWrite, const char *pszRead, const char *pszFill,
                                                    const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_MMIORegisterHC: caller='%s'/%d: GCPhysStart=%VGp cbRange=%#x pvUser=%p pszWrite=%p:{%s} pszRead=%p:{%s} pszFill=%p:{%s} pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvUser, pszWrite, pszWrite, pszRead, pszRead, pszFill, pszFill, pszDesc, pszDesc));

    if (!HWACCMR3IsAllowed(pDevIns->Internal.s.pVMHC))
        return VINF_SUCCESS; /* NOP */

    /*
     * Resolve the functions.
     * Not all function have to present, leave it to IOM to enforce this.
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szR0Mod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0))
    {
        R0PTRTYPE(PFNIOMMMIOWRITE) pfnR0PtrWrite = 0;
        if (pszWrite)
            rc = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszWrite, &pfnR0PtrWrite);
        R0PTRTYPE(PFNIOMMMIOREAD) pfnR0PtrRead = 0;
        int rc2 = VINF_SUCCESS;
        if (pszRead)
            rc2 = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszRead, &pfnR0PtrRead);
        R0PTRTYPE(PFNIOMMMIOFILL) pfnR0PtrFill = 0;
        int rc3 = VINF_SUCCESS;
        if (pszFill)
            rc3 = PDMR3GetSymbolR0Lazy(pDevIns->Internal.s.pVMHC, pDevIns->pDevReg->szR0Mod, pszFill, &pfnR0PtrFill);
        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(rc2) && VBOX_SUCCESS(rc3))
            rc = IOMMMIORegisterR0(pDevIns->Internal.s.pVMHC, pDevIns, GCPhysStart, cbRange, pvUser, pfnR0PtrWrite, pfnR0PtrRead, pfnR0PtrFill, pszDesc);
        else
        {
            AssertMsgRC(rc,  ("Failed to resolve %s.%s (pszWrite)\n", pDevIns->pDevReg->szR0Mod, pszWrite));
            AssertMsgRC(rc2, ("Failed to resolve %s.%s (pszRead)\n",  pDevIns->pDevReg->szR0Mod, pszRead));
            AssertMsgRC(rc3, ("Failed to resolve %s.%s (pszFill)\n",  pDevIns->pDevReg->szR0Mod, pszFill));
            if (VBOX_FAILURE(rc2) && VBOX_SUCCESS(rc))
                rc = rc2;
            if (VBOX_FAILURE(rc3) && VBOX_SUCCESS(rc))
                rc = rc3;
        }
    }
    else
    {
        AssertMsgFailed(("No R0 module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_MMIORegisterR0: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnMMIODeregister */
static DECLCALLBACK(int) pdmR3DevHlp_MMIODeregister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_MMIODeregister: caller='%s'/%d: GCPhysStart=%VGp cbRange=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange));

    int rc = IOMR3MMIODeregister(pDevIns->Internal.s.pVMHC, pDevIns, GCPhysStart, cbRange);

    LogFlow(("pdmR3DevHlp_MMIODeregister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnROMRegister */
static DECLCALLBACK(int) pdmR3DevHlp_ROMRegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, bool fShadow, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_ROMRegister: caller='%s'/%d: GCPhysStart=%VGp cbRange=%#x pvBinary=%p fShadow=%RTbool pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvBinary, fShadow, pszDesc, pszDesc));

    int rc = MMR3PhysRomRegister(pDevIns->Internal.s.pVMHC, pDevIns, GCPhysStart, cbRange, pvBinary, fShadow, pszDesc);

    LogFlow(("pdmR3DevHlp_ROMRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnSSMRegister */
static DECLCALLBACK(int) pdmR3DevHlp_SSMRegister(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                                 PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                                 PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_SSMRegister: caller='%s'/%d: pszName=%p:{%s} u32Instance=%#x u32Version=#x cbGuess=%#x pfnSavePrep=%p pfnSaveExec=%p pfnSaveDone=%p pszLoadPrep=%p pfnLoadExec=%p pfnLoaddone=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pszName, pszName, u32Instance, u32Version, cbGuess, pfnSavePrep, pfnSaveExec, pfnSaveDone, pfnLoadPrep, pfnLoadExec, pfnLoadDone));

    int rc = SSMR3Register(pDevIns->Internal.s.pVMHC, pDevIns, pszName, u32Instance, u32Version, cbGuess,
                           pfnSavePrep, pfnSaveExec, pfnSaveDone,
                           pfnLoadPrep, pfnLoadExec, pfnLoadDone);

    LogFlow(("pdmR3DevHlp_SSMRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnTMTimerCreate */
static DECLCALLBACK(int) pdmR3DevHlp_TMTimerCreate(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_TMTimerCreate: caller='%s'/%d: enmClock=%d pfnCallback=%p pszDesc=%p:{%s} ppTimer=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, enmClock, pfnCallback, pszDesc, pszDesc, ppTimer));

    int rc = TMR3TimerCreateDevice(pDevIns->Internal.s.pVMHC, pDevIns, enmClock, pfnCallback, pszDesc, ppTimer);

    LogFlow(("pdmR3DevHlp_TMTimerCreate: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnTMTimerCreateExternal */
static DECLCALLBACK(PTMTIMERR3) pdmR3DevHlp_TMTimerCreateExternal(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);

    return TMR3TimerCreateExternal(pDevIns->Internal.s.pVMHC, enmClock, pfnCallback, pvUser, pszDesc);
}

/** @copydoc PDMDEVHLP::pfnPCIRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PCIRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: pPciDev=%p:{.config={%#.256Vhxs}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciDev, pPciDev->config));

    /*
     * Validate input.
     */
    if (!pPciDev)
    {
        Assert(pPciDev);
        LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Vrc (pPciDev)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!pPciDev->config[0] && !pPciDev->config[1])
    {
        Assert(pPciDev->config[0] || pPciDev->config[1]);
        LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Vrc (vendor)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (pDevIns->Internal.s.pPciDeviceHC)
    {
        /** @todo the PCI device vs. PDM device designed is a bit flawed if we have to
         * support a PDM device with multiple PCI devices. This might become a problem
         * when upgrading the chipset for instance...
         */
        AssertMsgFailed(("Only one PCI device per device is currently implemented!\n"));
        return VERR_INTERNAL_ERROR;
    }

    /*
     * Choose the PCI bus for the device.
     * This is simple. If the device was configured for a particular bus, it'll
     * already have one. If not, we'll just take the first one.
     */
    PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusHC;
    if (!pBus)
        pBus = pDevIns->Internal.s.pPciBusHC = &pVM->pdm.s.aPciBuses[0];
    int rc;
    if (pBus)
    {
        if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC)
            pDevIns->Internal.s.pPciBusGC = MMHyperHC2GC(pVM, pDevIns->Internal.s.pPciBusHC);

        /*
         * Check the configuration for PCI device and function assignment.
         */
        int iDev = -1;
        uint8_t     u8Device;
        rc = CFGMR3QueryU8(pDevIns->Internal.s.pCfgHandle, "PCIDeviceNo", &u8Device);
        if (VBOX_SUCCESS(rc))
        {
            if (u8Device > 31)
            {
                AssertMsgFailed(("Configuration error: PCIDeviceNo=%d, max is 31. (%s/%d)\n",
                                 u8Device, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                return VERR_INTERNAL_ERROR;
            }

            uint8_t     u8Function;
            rc = CFGMR3QueryU8(pDevIns->Internal.s.pCfgHandle, "PCIFunctionNo", &u8Function);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("Configuration error: PCIDeviceNo, but PCIFunctionNo query failed with rc=%Vrc (%s/%d)\n",
                                 rc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                return rc;
            }
            if (u8Function > 7)
            {
                AssertMsgFailed(("Configuration error: PCIFunctionNo=%d, max is 7. (%s/%d)\n",
                                 u8Function, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                return VERR_INTERNAL_ERROR;
            }
            iDev = (u8Device << 3) | u8Function;
        }
        else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
        {
            AssertMsgFailed(("Configuration error: PCIDeviceNo query failed with rc=%Vrc (%s/%d)\n",
                             rc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            return rc;
        }

        /*
         * Call the pci bus device to do the actual registration.
         */
        pdmLock(pVM);
        rc = pBus->pfnRegisterR3(pBus->pDevInsR3, pPciDev, pDevIns->pDevReg->szDeviceName, iDev);
        pdmUnlock(pVM);
        if (VBOX_SUCCESS(rc))
        {
            pDevIns->Internal.s.pPciDeviceHC = pPciDev;
            if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC)
                pDevIns->Internal.s.pPciDeviceGC = MMHyperHC2GC(pVM, pPciDev);
            else
                pDevIns->Internal.s.pPciDeviceGC = 0;
            pPciDev->pDevIns = pDevIns;
            Log(("PDM: Registered device '%s'/%d as PCI device %d on bus %d\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciDev->devfn, pDevIns->Internal.s.pPciBusHC->iBus));
        }
    }
    else
    {
        AssertMsgFailed(("Configuration error: No PCI bus available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_PCI_BUS;
    }

    LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnPCIIORegionRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PCIIORegionRegister(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: iRegion=%d cbRegion=%#x enmType=%d pfnCallback=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, cbRegion, enmType, pfnCallback));

    /*
     * Validate input.
     */
    if (iRegion < 0 || iRegion >= PCI_NUM_REGIONS)
    {
        Assert(iRegion >= 0 && iRegion < PCI_NUM_REGIONS);
        LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Vrc (iRegion)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    switch (enmType)
    {
        case PCI_ADDRESS_SPACE_MEM:
        case PCI_ADDRESS_SPACE_IO:
        case PCI_ADDRESS_SPACE_MEM_PREFETCH:
            break;
        default:
            AssertMsgFailed(("enmType=%#x is unknown\n", enmType));
            LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Vrc (enmType)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
            return VERR_INVALID_PARAMETER;
    }
    if (!pfnCallback)
    {
        Assert(pfnCallback);
        LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Vrc (callback)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    AssertRelease(VMR3GetState(pVM) != VMSTATE_RUNNING);

    /*
     * Must have a PCI device registered!
     */
    int rc;
    PPCIDEVICE pPciDev = pDevIns->Internal.s.pPciDeviceHC;
    if (pPciDev)
    {
        /*
         * We're currently restricted to page aligned MMIO regions.
         */
        if (    (enmType == PCI_ADDRESS_SPACE_MEM || enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH)
            &&  cbRegion != RT_ALIGN_32(cbRegion, PAGE_SIZE))
        {
            Log(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: aligning cbRegion %#x -> %#x\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cbRegion, RT_ALIGN_32(cbRegion, PAGE_SIZE)));
            cbRegion = RT_ALIGN_32(cbRegion, PAGE_SIZE);
        }

        PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusHC;
        Assert(pBus);
        pdmLock(pVM);
        rc = pBus->pfnIORegionRegisterR3(pBus->pDevInsR3, pPciDev, iRegion, cbRegion, enmType, pfnCallback);
        pdmUnlock(pVM);
    }
    else
    {
        AssertMsgFailed(("No PCI device registered!\n"));
        rc = VERR_PDM_NOT_PCI_DEVICE;
    }

    LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnPCISetConfigCallbacks */
static DECLCALLBACK(void) pdmR3DevHlp_PCISetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                            PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCISetConfigCallbacks: caller='%s'/%d: pPciDev=%p pfnRead=%p ppfnReadOld=%p pfnWrite=%p ppfnWriteOld=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciDev, pfnRead, ppfnReadOld, pfnWrite, ppfnWriteOld));

    /*
     * Validate input and resolve defaults.
     */
    AssertPtr(pfnRead);
    AssertPtr(pfnWrite);
    AssertPtrNull(ppfnReadOld);
    AssertPtrNull(ppfnWriteOld);
    AssertPtrNull(pPciDev);

    if (!pPciDev)
        pPciDev = pDevIns->Internal.s.pPciDeviceHC;
    AssertReleaseMsg(pPciDev, ("You must register your device first!\n"));
    PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusHC;
    AssertRelease(pBus);
    AssertRelease(VMR3GetState(pVM) != VMSTATE_RUNNING);

    /*
     * Do the job.
     */
    pdmLock(pVM);
    pBus->pfnSetConfigCallbacksR3(pBus->pDevInsR3, pPciDev, pfnRead, ppfnReadOld, pfnWrite, ppfnWriteOld);
    pdmUnlock(pVM);

    LogFlow(("pdmR3DevHlp_PCISetConfigCallbacks: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnPCISetIrq */
static DECLCALLBACK(void) pdmR3DevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PCISetIrq: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iIrq, iLevel));

    /*
     * Validate input.
     */
    /** @todo iIrq and iLevel checks. */

    /*
     * Must have a PCI device registered!
     */
    PPCIDEVICE pPciDev = pDevIns->Internal.s.pPciDeviceHC;
    if (pPciDev)
    {
        PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusHC; /** @todo the bus should be associated with the PCI device not the PDM device. */
        Assert(pBus);
#ifdef VBOX_WITH_PDM_LOCK
        PVM pVM = pDevIns->Internal.s.pVMHC;
        pdmLock(pVM);
        pBus->pfnSetIrqR3(pBus->pDevInsR3, pPciDev, iIrq, iLevel);
        pdmUnlock(pVM);

#else /* !VBOX_WITH_PDM_LOCK */
        /*
         * For the convenience of the device we put no thread restriction on this interface.
         * That means we'll have to check which thread we're in and choose our path.
         */
        if (VM_IS_EMT(pDevIns->Internal.s.pVMHC) || VMMR3LockIsOwner(pDevIns->Internal.s.pVMHC))
            pBus->pfnSetIrqR3(pBus->pDevInsR3, pPciDev, iIrq, iLevel);
        else
        {
            Log(("pdmR3DevHlp_PCISetIrq: caller='%s'/%d: Requesting call in EMT...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            PVMREQ pReq;
            int rc = VMR3ReqCallVoid(pDevIns->Internal.s.pVMHC, &pReq, RT_INDEFINITE_WAIT,
                                     (PFNRT)pBus->pfnSetIrqR3, 4, pBus->pDevInsR3, pPciDev, iIrq, iLevel);
            while (rc == VERR_TIMEOUT)
                rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
            AssertReleaseRC(rc);
            VMR3ReqFree(pReq);
        }
#endif /* !VBOX_WITH_PDM_LOCK */
    }
    else
        AssertReleaseMsgFailed(("No PCI device registered!\n"));

    LogFlow(("pdmR3DevHlp_PCISetIrq: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnPCISetIrqNoWait */
static DECLCALLBACK(void) pdmR3DevHlp_PCISetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
#ifdef VBOX_WITH_PDM_LOCK
    pdmR3DevHlp_PCISetIrq(pDevIns, iIrq, iLevel);
#else /* !VBOX_WITH_PDM_LOCK */
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PCISetIrqNoWait: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iIrq, iLevel));

    /*
     * Validate input.
     */
    /** @todo iIrq and iLevel checks. */

    /*
     * Must have a PCI device registered!
     */
    PPCIDEVICE pPciDev = pDevIns->Internal.s.pPciDeviceHC;
    if (pPciDev)
    {
        PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusHC;
        Assert(pBus);

        /*
         * For the convenience of the device we put no thread restriction on this interface.
         * That means we'll have to check which thread we're in and choose our path.
         */
        if (VM_IS_EMT(pDevIns->Internal.s.pVMHC) || VMMR3LockIsOwner(pDevIns->Internal.s.pVMHC))
            pBus->pfnSetIrqR3(pBus->pDevInsR3, pPciDev, iIrq, iLevel);
        else
        {
            Log(("pdmR3DevHlp_PCISetIrqNoWait: caller='%s'/%d: Queueing call in EMT...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            int rc = VMR3ReqCallEx(pDevIns->Internal.s.pVMHC, NULL, RT_INDEFINITE_WAIT, VMREQFLAGS_NO_WAIT | VMREQFLAGS_VOID,
                                     (PFNRT)pBus->pfnSetIrqR3, 4, pBus->pDevInsR3, pPciDev, iIrq, iLevel);
            AssertReleaseRC(rc);
        }
    }
    else
        AssertReleaseMsgFailed(("No PCI device registered!\n"));

    LogFlow(("pdmR3DevHlp_PCISetIrqNoWait: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
#endif /* !VBOX_WITH_PDM_LOCK */
}


/** @copydoc PDMDEVHLP::pfnISASetIrq */
static DECLCALLBACK(void) pdmR3DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_ISASetIrq: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iIrq, iLevel));

    /*
     * Validate input.
     */
    /** @todo iIrq and iLevel checks. */

    PVM pVM = pDevIns->Internal.s.pVMHC;
#ifdef VBOX_WITH_PDM_LOCK
    PDMIsaSetIrq(pVM, iIrq, iLevel);    /* (The API takes the lock.) */
#else /* !VBOX_WITH_PDM_LOCK */
    if (VM_IS_EMT(pDevIns->Internal.s.pVMHC) || VMMR3LockIsOwner(pDevIns->Internal.s.pVMHC))
        PDMIsaSetIrq(pVM, iIrq, iLevel);
    else
    {
        Log(("pdmR3DevHlp_ISASetIrq: caller='%s'/%d: Requesting call in EMT...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        PVMREQ pReq;
        int rc = VMR3ReqCallVoid(pDevIns->Internal.s.pVMHC, &pReq, RT_INDEFINITE_WAIT,
                                 (PFNRT)PDMIsaSetIrq, 3, pVM, iIrq, iLevel);
        while (rc == VERR_TIMEOUT)
            rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertReleaseRC(rc);
        VMR3ReqFree(pReq);
    }
#endif /* !VBOX_WITH_PDM_LOCK */

    LogFlow(("pdmR3DevHlp_ISASetIrq: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}

/** @copydoc PDMDEVHLP::pfnISASetIrqNoWait */
static DECLCALLBACK(void) pdmR3DevHlp_ISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
#ifdef VBOX_WITH_PDM_LOCK
    pdmR3DevHlp_ISASetIrq(pDevIns, iIrq, iLevel);
#else /* !VBOX_WITH_PDM_LOCK */
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_ISASetIrqNoWait: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iIrq, iLevel));

    /*
     * Validate input.
     */
    /** @todo iIrq and iLevel checks. */

    PVM pVM = pDevIns->Internal.s.pVMHC;
    /*
     * For the convenience of the device we put no thread restriction on this interface.
     * That means we'll have to check which thread we're in and choose our path.
     */
    if (VM_IS_EMT(pDevIns->Internal.s.pVMHC) || VMMR3LockIsOwner(pDevIns->Internal.s.pVMHC))
        PDMIsaSetIrq(pVM, iIrq, iLevel);
    else
    {
        Log(("pdmR3DevHlp_ISASetIrqNoWait: caller='%s'/%d: Queueing call in EMT...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        int rc = VMR3ReqCallEx(pDevIns->Internal.s.pVMHC, NULL, 0, VMREQFLAGS_NO_WAIT | VMREQFLAGS_VOID,
                               (PFNRT)PDMIsaSetIrq, 3, pVM, iIrq, iLevel);
        AssertReleaseRC(rc);
    }

    LogFlow(("pdmR3DevHlp_ISASetIrqNoWait: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
#endif /* !VBOX_WITH_PDM_LOCK */
}


/** @copydoc PDMDEVHLP::pfnDriverAttach */
static DECLCALLBACK(int) pdmR3DevHlp_DriverAttach(PPDMDEVINS pDevIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DriverAttach: caller='%s'/%d: iLun=%d pBaseInterface=%p ppBaseInterface=%p pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iLun, pBaseInterface, ppBaseInterface, pszDesc, pszDesc));

    /*
     * Lookup the LUN, it might already be registered.
     */
    PPDMLUN pLunPrev = NULL;
    PPDMLUN pLun = pDevIns->Internal.s.pLunsHC;
    for (; pLun; pLunPrev = pLun, pLun = pLun->pNext)
        if (pLun->iLun == iLun)
            break;

    /*
     * Create the LUN if if wasn't found, else check if driver is already attached to it.
     */
    if (!pLun)
    {
        if (    !pBaseInterface
            ||  !pszDesc
            ||  !*pszDesc)
        {
            Assert(pBaseInterface);
            Assert(pszDesc || *pszDesc);
            return VERR_INVALID_PARAMETER;
        }

        pLun = (PPDMLUN)MMR3HeapAlloc(pVM, MM_TAG_PDM_LUN, sizeof(*pLun));
        if (!pLun)
            return VERR_NO_MEMORY;

        pLun->iLun      = iLun;
        pLun->pNext     = pLunPrev ? pLunPrev->pNext : NULL;
        pLun->pTop      = NULL;
        pLun->pDevIns   = pDevIns;
        pLun->pszDesc   = pszDesc;
        pLun->pBase     = pBaseInterface;
        if (!pLunPrev)
            pDevIns->Internal.s.pLunsHC = pLun;
        else
            pLunPrev->pNext = pLun;
        Log(("pdmR3DevHlp_DriverAttach: Registered LUN#%d '%s' with device '%s'/%d.\n",
             iLun, pszDesc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    }
    else if (pLun->pTop)
    {
        AssertMsgFailed(("Already attached! The device should keep track of such things!\n"));
        LogFlow(("pdmR3DevHlp_DriverAttach: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_PDM_DRIVER_ALREADY_ATTACHED));
        return VERR_PDM_DRIVER_ALREADY_ATTACHED;
    }
    Assert(pLun->pBase == pBaseInterface);


    /*
     * Get the attached driver configuration.
     */
    int rc;
    char szNode[48];
    RTStrPrintf(szNode, sizeof(szNode), "LUN#%d", iLun);
    PCFGMNODE   pNode = CFGMR3GetChild(pDevIns->Internal.s.pCfgHandle, szNode);
    if (pNode)
    {
        char *pszName;
        rc = CFGMR3QueryStringAlloc(pNode, "Driver", &pszName);
        if (VBOX_SUCCESS(rc))
        {
            /*
             * Find the driver.
             */
            PPDMDRV pDrv = pdmR3DrvLookup(pVM, pszName);
            if (pDrv)
            {
                /* config node */
                PCFGMNODE pConfigNode = CFGMR3GetChild(pNode, "Config");
                if (!pConfigNode)
                    rc = CFGMR3InsertNode(pNode, "Config", &pConfigNode);
                if (VBOX_SUCCESS(rc))
                {
                    CFGMR3SetRestrictedRoot(pConfigNode);

                    /*
                     * Allocate the driver instance.
                     */
                    size_t cb = RT_OFFSETOF(PDMDRVINS, achInstanceData[pDrv->pDrvReg->cbInstance]);
                    cb = RT_ALIGN_Z(cb, 16);
                    PPDMDRVINS pNew = (PPDMDRVINS)MMR3HeapAllocZ(pVM, MM_TAG_PDM_DRIVER, cb);
                    if (pNew)
                    {
                        /*
                         * Initialize the instance structure (declaration order).
                         */
                        pNew->u32Version                = PDM_DRVINS_VERSION;
                        //pNew->Internal.s.pUp            = NULL;
                        //pNew->Internal.s.pDown          = NULL;
                        pNew->Internal.s.pLun           = pLun;
                        pNew->Internal.s.pDrv           = pDrv;
                        pNew->Internal.s.pVM            = pVM;
                        //pNew->Internal.s.fDetaching     = false;
                        pNew->Internal.s.pCfgHandle     = pNode;
                        pNew->pDrvHlp                   = &g_pdmR3DrvHlp;
                        pNew->pDrvReg                   = pDrv->pDrvReg;
                        pNew->pCfgHandle                = pConfigNode;
                        pNew->iInstance                 = pDrv->cInstances++;
                        pNew->pUpBase                   = pBaseInterface;
                        //pNew->pDownBase                 = NULL;
                        //pNew->IBase.pfnQueryInterface   = NULL;
                        pNew->pvInstanceData            = &pNew->achInstanceData[0];

                        /*
                         * Link with LUN and call the constructor.
                         */
                        rc = pDrv->pDrvReg->pfnConstruct(pNew, pNew->pCfgHandle);
                        if (VBOX_SUCCESS(rc))
                        {
                            pLun->pTop = pNew;
                            MMR3HeapFree(pszName);
                            *ppBaseInterface = &pNew->IBase;
                            Log(("PDM: Attached driver '%s'/%d to LUN#%d on device '%s'/%d.\n",
                                 pDrv->pDrvReg->szDriverName, pNew->iInstance, iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                            LogFlow(("pdmR3DevHlp_DriverAttach: caller '%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
			    /*
			     * Might return != VINF_SUCCESS (e.g. VINF_NAT_DNS) */
                            return rc;
                        }

                        /*
                         * Free the driver.
                         */
                        pLun->pTop = NULL;
                        ASMMemFill32(pNew, cb, 0xdeadd0d0);
                        MMR3HeapFree(pNew);
                        pDrv->cInstances--;
                    }
                    else
                    {
                        AssertMsgFailed(("Failed to allocate %d bytes for instantiating driver '%s'\n", cb, pszName));
                        rc = VERR_NO_MEMORY;
                    }
                }
                else
                    AssertMsgFailed(("Failed to create Config node! rc=%Vrc\n", rc));
            }
            else
            {
                AssertMsgFailed(("Driver '%s' wasn't found!\n", pszName));
                rc = VERR_PDM_DRIVER_NOT_FOUND;
            }
            MMR3HeapFree(pszName);
        }
        else
        {
            AssertMsgFailed(("Query for string value of \"Driver\" -> %Vrc\n", rc));
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                rc = VERR_PDM_CFG_MISSING_DRIVER_NAME;
        }
    }
    else
        rc = VERR_PDM_NO_ATTACHED_DRIVER;


    LogFlow(("pdmR3DevHlp_DriverAttach: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnMMHeapAlloc */
static DECLCALLBACK(void *) pdmR3DevHlp_MMHeapAlloc(PPDMDEVINS pDevIns, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_MMHeapAlloc: caller='%s'/%d: cb=%#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cb));

    void *pv = MMR3HeapAlloc(pDevIns->Internal.s.pVMHC, MM_TAG_PDM_DEVICE_USER, cb);

    LogFlow(("pdmR3DevHlp_MMHeapAlloc: caller='%s'/%d: returns %p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pv));
    return pv;
}


/** @copydoc PDMDEVHLP::pfnMMHeapAllocZ */
static DECLCALLBACK(void *) pdmR3DevHlp_MMHeapAllocZ(PPDMDEVINS pDevIns, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_MMHeapAllocZ: caller='%s'/%d: cb=%#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cb));

    void *pv = MMR3HeapAllocZ(pDevIns->Internal.s.pVMHC, MM_TAG_PDM_DEVICE_USER, cb);

    LogFlow(("pdmR3DevHlp_MMHeapAllocZ: caller='%s'/%d: returns %p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pv));
    return pv;
}


/** @copydoc PDMDEVHLP::pfnMMHeapFree */
static DECLCALLBACK(void) pdmR3DevHlp_MMHeapFree(PPDMDEVINS pDevIns, void *pv)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_MMHeapFree: caller='%s'/%d: pv=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pv));

    MMR3HeapFree(pv);

    LogFlow(("pdmR3DevHlp_MMHeapAlloc: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnVMSetError */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMHC, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLP::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMHC, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDEVHLP::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMHC, fFatal, pszErrorID, pszFormat, args);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLP::pfnVMSetRuntimeErrorV */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMHC, fFatal, pszErrorID, pszFormat, va);
    return rc;
}


/** @copydoc PDMDEVHLP::pfnAssertEMT */
static DECLCALLBACK(bool) pdmR3DevHlp_AssertEMT(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (VM_IS_EMT(pDevIns->Internal.s.pVMHC))
        return true;

    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertEMT '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance);
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    return false;
}


/** @copydoc PDMDEVHLP::pfnAssertOther */
static DECLCALLBACK(bool) pdmR3DevHlp_AssertOther(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!VM_IS_EMT(pDevIns->Internal.s.pVMHC))
        return true;

    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertOther '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance);
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    return false;
}


/** @copydoc PDMDEVHLP::pfnDBGFStopV */
static DECLCALLBACK(int) pdmR3DevHlp_DBGFStopV(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction, const char *pszFormat, va_list args)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
#ifdef LOG_ENABLED
    va_list va2;
    va_copy(va2, args);
    LogFlow(("pdmR3DevHlp_DBGFStopV: caller='%s'/%d: pszFile=%p:{%s} iLine=%d pszFunction=%p:{%s} pszFormat=%p:{%s} (%N)\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pszFile, pszFile, iLine, pszFunction, pszFunction, pszFormat, pszFormat, pszFormat, &va2));
    va_end(va2);
#endif

    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    int rc = DBGFR3EventSrcV(pVM, DBGFEVENT_DEV_STOP, pszFile, iLine, pszFunction, pszFormat, args);

    LogFlow(("pdmR3DevHlp_DBGFStopV: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnDBGFInfoRegister */
static DECLCALLBACK(int) pdmR3DevHlp_DBGFInfoRegister(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_DBGFInfoRegister: caller='%s'/%d: pszName=%p:{%s} pszDesc=%p:{%s} pfnHandler=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pszName, pszName, pszDesc, pszDesc, pfnHandler));

    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    int rc = DBGFR3InfoRegisterDevice(pVM, pszName, pszDesc, pfnHandler, pDevIns);

    LogFlow(("pdmR3DevHlp_DBGFInfoRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnSTAMRegister */
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegister(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);

    STAM_REG(pVM, pvSample, enmType, pszName, enmUnit, pszDesc);
    NOREF(pVM);
}



/** @copydoc PDMDEVHLP::pfnSTAMRegisterF */
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegisterF(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                    STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);

    va_list args;
    va_start(args, pszName);
    int rc = STAMR3RegisterV(pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    va_end(args);
    AssertRC(rc);

    NOREF(pVM);
}


/** @copydoc PDMDEVHLP::pfnSTAMRegisterV */
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegisterV(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                    STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);

    int rc = STAMR3RegisterV(pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    AssertRC(rc);

    NOREF(pVM);
}


/** @copydoc PDMDEVHLP::pfnRTCRegister */
static DECLCALLBACK(int) pdmR3DevHlp_RTCRegister(PPDMDEVINS pDevIns, PCPDMRTCREG pRtcReg, PCPDMRTCHLP *ppRtcHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: pRtcReg=%p:{.u32Version=%#x, .pfnWrite=%p, .pfnRead=%p} ppRtcHlp=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pRtcReg, pRtcReg->u32Version, pRtcReg->pfnWrite,
             pRtcReg->pfnWrite, ppRtcHlp));

    /*
     * Validate input.
     */
    if (pRtcReg->u32Version != PDM_RTCREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pRtcReg->u32Version,
                         PDM_RTCREG_VERSION));
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Vrc (version)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pRtcReg->pfnWrite
        ||  !pRtcReg->pfnRead)
    {
        Assert(pRtcReg->pfnWrite);
        Assert(pRtcReg->pfnRead);
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Vrc (callbacks)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppRtcHlp)
    {
        Assert(ppRtcHlp);
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Vrc (ppRtcHlp)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one DMA device.
     */
    PVM pVM = pDevIns->Internal.s.pVMHC;
    if (pVM->pdm.s.pRtc)
    {
        AssertMsgFailed(("Only one RTC device is supported!\n"));
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Vrc\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate and initialize pci bus structure.
     */
    int rc = VINF_SUCCESS;
    PPDMRTC pRtc = (PPDMRTC)MMR3HeapAlloc(pDevIns->Internal.s.pVMHC, MM_TAG_PDM_DEVICE, sizeof(*pRtc));
    if (pRtc)
    {
        pRtc->pDevIns   = pDevIns;
        pRtc->Reg       = *pRtcReg;
        pVM->pdm.s.pRtc = pRtc;

        /* set the helper pointer. */
        *ppRtcHlp = &g_pdmR3DevRtcHlp;
        Log(("PDM: Registered RTC device '%s'/%d pDevIns=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnPDMQueueCreate */
static DECLCALLBACK(int) pdmR3DevHlp_PDMQueueCreate(PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                                    PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PDMQueueCreate: caller='%s'/%d: cbItem=%#x cItems=%#x cMilliesInterval=%u pfnCallback=%p fGCEnabled=%RTbool ppQueue=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled, ppQueue));

    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    int rc = PDMR3QueueCreateDevice(pVM, pDevIns, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled, ppQueue);

    LogFlow(("pdmR3DevHlp_PDMQueueCreate: caller='%s'/%d: returns %Vrc *ppQueue=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc, *ppQueue));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnCritSectInit */
static DECLCALLBACK(int) pdmR3DevHlp_CritSectInit(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_CritSectInit: caller='%s'/%d: pCritSect=%p pszName=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pCritSect, pszName, pszName));

    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    int rc = pdmR3CritSectInitDevice(pVM, pDevIns, pCritSect, pszName);

    LogFlow(("pdmR3DevHlp_CritSectInit: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnUTCNow */
static DECLCALLBACK(PRTTIMESPEC) pdmR3DevHlp_UTCNow(PPDMDEVINS pDevIns, PRTTIMESPEC pTime)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_UTCNow: caller='%s'/%d: pTime=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pTime));

    pTime = TMR3UTCNow(pDevIns->Internal.s.pVMHC, pTime);

    LogFlow(("pdmR3DevHlp_UTCNow: caller='%s'/%d: returns %RU64\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, RTTimeSpecGetNano(pTime)));
    return pTime;
}


/** @copydoc PDMDEVHLP::pfnPDMThreadCreate */
static DECLCALLBACK(int) pdmR3DevHlp_PDMThreadCreate(PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                                     PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_PDMThreadCreate: caller='%s'/%d: ppThread=%p pvUser=%p pfnThread=%p pfnWakeup=%p cbStack=%#zx enmType=%d pszName=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName, pszName));

    int rc = pdmR3ThreadCreateDevice(pDevIns->Internal.s.pVMHC, pDevIns, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName);

    LogFlow(("pdmR3DevHlp_PDMThreadCreate: caller='%s'/%d: returns %Vrc *ppThread=%RTthrd\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
            rc, *ppThread));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnGetVM */
static DECLCALLBACK(PVM) pdmR3DevHlp_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_GetVM: caller='%s'/%d: returns %p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns->Internal.s.pVMHC));
    return pDevIns->Internal.s.pVMHC;
}


/** @copydoc PDMDEVHLP::pfnPCIBusRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PCIBusRegister(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: pPciBusReg=%p:{.u32Version=%#x, .pfnRegisterHC=%p, .pfnIORegionRegisterHC=%p, .pfnSetIrqHC=%p, "
             ".pfnSaveExecHC=%p, .pfnLoadExecHC=%p, .pfnFakePCIBIOSHC=%p, .pszSetIrqGC=%p:{%s}} ppPciHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciBusReg, pPciBusReg->u32Version, pPciBusReg->pfnRegisterHC,
             pPciBusReg->pfnIORegionRegisterHC, pPciBusReg->pfnSetIrqHC, pPciBusReg->pfnSaveExecHC, pPciBusReg->pfnLoadExecHC,
             pPciBusReg->pfnFakePCIBIOSHC, pPciBusReg->pszSetIrqGC, pPciBusReg->pszSetIrqGC, ppPciHlpR3));

    /*
     * Validate the structure.
     */
    if (pPciBusReg->u32Version != PDM_PCIBUSREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pPciBusReg->u32Version, PDM_PCIBUSREG_VERSION));
        LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Vrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pPciBusReg->pfnRegisterHC
        ||  !pPciBusReg->pfnIORegionRegisterHC
        ||  !pPciBusReg->pfnSetIrqHC
        ||  !pPciBusReg->pfnSaveExecHC
        ||  !pPciBusReg->pfnLoadExecHC
        ||  !pPciBusReg->pfnFakePCIBIOSHC)
    {
        Assert(pPciBusReg->pfnRegisterHC);
        Assert(pPciBusReg->pfnIORegionRegisterHC);
        Assert(pPciBusReg->pfnSetIrqHC);
        Assert(pPciBusReg->pfnSaveExecHC);
        Assert(pPciBusReg->pfnLoadExecHC);
        Assert(pPciBusReg->pfnFakePCIBIOSHC);
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Vrc (HC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPciBusReg->pszSetIrqGC
        &&  !VALID_PTR(pPciBusReg->pszSetIrqGC))
    {
        Assert(VALID_PTR(pPciBusReg->pszSetIrqGC));
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Vrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPciBusReg->pszSetIrqR0
        &&  !VALID_PTR(pPciBusReg->pszSetIrqR0))
    {
        Assert(VALID_PTR(pPciBusReg->pszSetIrqR0));
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Vrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
     if (!ppPciHlpR3)
    {
        Assert(ppPciHlpR3);
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Vrc (ppPciHlpR3)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find free PCI bus entry.
     */
    unsigned iBus = 0;
    for (iBus = 0; iBus < ELEMENTS(pVM->pdm.s.aPciBuses); iBus++)
        if (!pVM->pdm.s.aPciBuses[iBus].pDevInsR3)
            break;
    if (iBus >= ELEMENTS(pVM->pdm.s.aPciBuses))
    {
        AssertMsgFailed(("Too many PCI buses. Max=%u\n", ELEMENTS(pVM->pdm.s.aPciBuses)));
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Vrc (pci bus)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    PPDMPCIBUS pPciBus = &pVM->pdm.s.aPciBuses[iBus];

    /*
     * Resolve and init the GC bits.
     */
    if (pPciBusReg->pszSetIrqGC)
    {
        int rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pPciBusReg->pszSetIrqGC, &pPciBus->pfnSetIrqGC);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pPciBusReg->pszSetIrqGC, rc));
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pPciBus->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    }
    else
    {
        pPciBus->pfnSetIrqGC = 0;
        pPciBus->pDevInsGC   = 0;
    }

    /*
     * Resolve and init the R0 bits.
     */
    if (    HWACCMR3IsAllowed(pVM)
        &&  pPciBusReg->pszSetIrqR0)
    {
        int rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pPciBusReg->pszSetIrqR0, &pPciBus->pfnSetIrqR0);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pPciBusReg->pszSetIrqR0, rc));
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pPciBus->pDevInsR0 = MMHyperR3ToR0(pVM, pDevIns);
    }
    else
    {
        pPciBus->pfnSetIrqR0 = 0;
        pPciBus->pDevInsR0   = 0;
    }

    /*
     * Init the HC bits.
     */
    pPciBus->iBus                    = iBus;
    pPciBus->pDevInsR3               = pDevIns;
    pPciBus->pfnRegisterR3           = pPciBusReg->pfnRegisterHC;
    pPciBus->pfnIORegionRegisterR3   = pPciBusReg->pfnIORegionRegisterHC;
    pPciBus->pfnSetConfigCallbacksR3 = pPciBusReg->pfnSetConfigCallbacksHC;
    pPciBus->pfnSetIrqR3             = pPciBusReg->pfnSetIrqHC;
    pPciBus->pfnSaveExecR3           = pPciBusReg->pfnSaveExecHC;
    pPciBus->pfnLoadExecR3           = pPciBusReg->pfnLoadExecHC;
    pPciBus->pfnFakePCIBIOSR3        = pPciBusReg->pfnFakePCIBIOSHC;

    Log(("PDM: Registered PCI bus device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppPciHlpR3 = &g_pdmR3DevPciHlp;
    LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLP::pfnPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PICRegister(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: pPicReg=%p:{.u32Version=%#x, .pfnSetIrqHC=%p, .pfnGetInterruptHC=%p, .pszGetIrqGC=%p:{%s}, .pszGetInterruptGC=%p:{%s}} ppPicHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPicReg, pPicReg->u32Version, pPicReg->pfnSetIrqHC, pPicReg->pfnGetInterruptHC,
             pPicReg->pszSetIrqGC, pPicReg->pszSetIrqGC, pPicReg->pszGetInterruptGC, pPicReg->pszGetInterruptGC, ppPicHlpR3));

    /*
     * Validate input.
     */
    if (pPicReg->u32Version != PDM_PICREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pPicReg->u32Version, PDM_PICREG_VERSION));
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pPicReg->pfnSetIrqHC
        ||  !pPicReg->pfnGetInterruptHC)
    {
        Assert(pPicReg->pfnSetIrqHC);
        Assert(pPicReg->pfnGetInterruptHC);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc (HC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    (   pPicReg->pszSetIrqGC
             || pPicReg->pszGetInterruptGC)
        &&  (   !VALID_PTR(pPicReg->pszSetIrqGC)
             || !VALID_PTR(pPicReg->pszGetInterruptGC))
       )
    {
        Assert(VALID_PTR(pPicReg->pszSetIrqGC));
        Assert(VALID_PTR(pPicReg->pszGetInterruptGC));
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPicReg->pszSetIrqGC
        &&  !(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC))
    {
        Assert(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_GC);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc (GC flag)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPicReg->pszSetIrqR0
        &&  !(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0))
    {
        Assert(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc (R0 flag)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!ppPicHlpR3)
    {
        Assert(ppPicHlpR3);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc (ppPicHlpR3)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one PIC device.
     */
    PVM pVM = pDevIns->Internal.s.pVMHC;
    if (pVM->pdm.s.Pic.pDevInsR3)
    {
        AssertMsgFailed(("Only one pic device is supported!\n"));
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * GC stuff.
     */
    if (pPicReg->pszSetIrqGC)
    {
        int rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pPicReg->pszSetIrqGC, &pVM->pdm.s.Pic.pfnSetIrqGC);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pPicReg->pszSetIrqGC, rc));
        if (VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pPicReg->pszGetInterruptGC, &pVM->pdm.s.Pic.pfnGetInterruptGC);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pPicReg->pszGetInterruptGC, rc));
        }
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Pic.pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    }
    else
    {
        pVM->pdm.s.Pic.pDevInsGC = 0;
        pVM->pdm.s.Pic.pfnSetIrqGC = 0;
        pVM->pdm.s.Pic.pfnGetInterruptGC = 0;
    }

    /*
     * R0 stuff.
     */
    if (    HWACCMR3IsAllowed(pVM)
        &&  pPicReg->pszSetIrqR0)
    {
        int rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pPicReg->pszSetIrqR0, &pVM->pdm.s.Pic.pfnSetIrqR0);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pPicReg->pszSetIrqR0, rc));
        if (VBOX_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pPicReg->pszGetInterruptR0, &pVM->pdm.s.Pic.pfnGetInterruptR0);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pPicReg->pszGetInterruptR0, rc));
        }
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Pic.pDevInsR0 = MMHyperR3ToR0(pVM, pDevIns);
        Assert(pVM->pdm.s.Pic.pDevInsR0);
    }
    else
    {
        pVM->pdm.s.Pic.pfnSetIrqR0 = 0;
        pVM->pdm.s.Pic.pfnGetInterruptR0 = 0;
        pVM->pdm.s.Pic.pDevInsR0 = 0;
    }

    /*
     * HC stuff.
     */
    pVM->pdm.s.Pic.pDevInsR3 = pDevIns;
    pVM->pdm.s.Pic.pfnSetIrqR3 = pPicReg->pfnSetIrqHC;
    pVM->pdm.s.Pic.pfnGetInterruptR3 = pPicReg->pfnGetInterruptHC;
    Log(("PDM: Registered PIC device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppPicHlpR3 = &g_pdmR3DevPicHlp;
    LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLP::pfnAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_APICRegister(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: pApicReg=%p:{.u32Version=%#x, .pfnGetInterruptHC=%p, .pfnSetBaseHC=%p, .pfnGetBaseHC=%p, "
             ".pfnSetTPRHC=%p, .pfnGetTPRHC=%p, .pfnBusDeliverHC=%p, pszGetInterruptGC=%p:{%s}, pszSetBaseGC=%p:{%s}, pszGetBaseGC=%p:{%s}, "
             ".pszSetTPRGC=%p:{%s}, .pszGetTPRGC=%p:{%s}, .pszBusDeliverGC=%p:{%s}} ppApicHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pApicReg, pApicReg->u32Version, pApicReg->pfnGetInterruptHC, pApicReg->pfnSetBaseHC,
             pApicReg->pfnGetBaseHC, pApicReg->pfnSetTPRHC, pApicReg->pfnGetTPRHC, pApicReg->pfnBusDeliverHC, pApicReg->pszGetInterruptGC,
             pApicReg->pszGetInterruptGC, pApicReg->pszSetBaseGC, pApicReg->pszSetBaseGC, pApicReg->pszGetBaseGC, pApicReg->pszGetBaseGC,
             pApicReg->pszSetTPRGC, pApicReg->pszSetTPRGC, pApicReg->pszGetTPRGC, pApicReg->pszGetTPRGC, pApicReg->pszBusDeliverGC,
             pApicReg->pszBusDeliverGC, ppApicHlpR3));

    /*
     * Validate input.
     */
    if (pApicReg->u32Version != PDM_APICREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pApicReg->u32Version, PDM_APICREG_VERSION));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pApicReg->pfnGetInterruptHC
        ||  !pApicReg->pfnSetBaseHC
        ||  !pApicReg->pfnGetBaseHC
        ||  !pApicReg->pfnSetTPRHC
        ||  !pApicReg->pfnGetTPRHC
        ||  !pApicReg->pfnBusDeliverHC)
    {
        Assert(pApicReg->pfnGetInterruptHC);
        Assert(pApicReg->pfnSetBaseHC);
        Assert(pApicReg->pfnGetBaseHC);
        Assert(pApicReg->pfnSetTPRHC);
        Assert(pApicReg->pfnGetTPRHC);
        Assert(pApicReg->pfnBusDeliverHC);
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc (HC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (   (    pApicReg->pszGetInterruptGC
            ||  pApicReg->pszSetBaseGC
            ||  pApicReg->pszGetBaseGC
            ||  pApicReg->pszSetTPRGC
            ||  pApicReg->pszGetTPRGC
            ||  pApicReg->pszBusDeliverGC)
        &&  (   !VALID_PTR(pApicReg->pszGetInterruptGC)
            ||  !VALID_PTR(pApicReg->pszSetBaseGC)
            ||  !VALID_PTR(pApicReg->pszGetBaseGC)
            ||  !VALID_PTR(pApicReg->pszSetTPRGC)
            ||  !VALID_PTR(pApicReg->pszGetTPRGC)
            ||  !VALID_PTR(pApicReg->pszBusDeliverGC))
       )
    {
        Assert(VALID_PTR(pApicReg->pszGetInterruptGC));
        Assert(VALID_PTR(pApicReg->pszSetBaseGC));
        Assert(VALID_PTR(pApicReg->pszGetBaseGC));
        Assert(VALID_PTR(pApicReg->pszSetTPRGC));
        Assert(VALID_PTR(pApicReg->pszGetTPRGC));
        Assert(VALID_PTR(pApicReg->pszBusDeliverGC));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (   (    pApicReg->pszGetInterruptR0
            ||  pApicReg->pszSetBaseR0
            ||  pApicReg->pszGetBaseR0
            ||  pApicReg->pszSetTPRR0
            ||  pApicReg->pszGetTPRR0
            ||  pApicReg->pszBusDeliverR0)
        &&  (   !VALID_PTR(pApicReg->pszGetInterruptR0)
            ||  !VALID_PTR(pApicReg->pszSetBaseR0)
            ||  !VALID_PTR(pApicReg->pszGetBaseR0)
            ||  !VALID_PTR(pApicReg->pszSetTPRR0)
            ||  !VALID_PTR(pApicReg->pszGetTPRR0)
            ||  !VALID_PTR(pApicReg->pszBusDeliverR0))
       )
    {
        Assert(VALID_PTR(pApicReg->pszGetInterruptR0));
        Assert(VALID_PTR(pApicReg->pszSetBaseR0));
        Assert(VALID_PTR(pApicReg->pszGetBaseR0));
        Assert(VALID_PTR(pApicReg->pszSetTPRR0));
        Assert(VALID_PTR(pApicReg->pszGetTPRR0));
        Assert(VALID_PTR(pApicReg->pszBusDeliverR0));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc (R0 callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!ppApicHlpR3)
    {
        Assert(ppApicHlpR3);
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc (ppApicHlpR3)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one APIC device. (malc: only in UP case actually)
     */
    PVM pVM = pDevIns->Internal.s.pVMHC;
    if (pVM->pdm.s.Apic.pDevInsR3)
    {
        AssertMsgFailed(("Only one apic device is supported!\n"));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Resolve & initialize the GC bits.
     */
    if (pApicReg->pszGetInterruptGC)
    {
        int rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pApicReg->pszGetInterruptGC, &pVM->pdm.s.Apic.pfnGetInterruptGC);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pApicReg->pszGetInterruptGC, rc));
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pApicReg->pszSetBaseGC, &pVM->pdm.s.Apic.pfnSetBaseGC);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pApicReg->pszSetBaseGC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pApicReg->pszGetBaseGC, &pVM->pdm.s.Apic.pfnGetBaseGC);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pApicReg->pszGetBaseGC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pApicReg->pszSetTPRGC, &pVM->pdm.s.Apic.pfnSetTPRGC);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pApicReg->pszSetTPRGC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pApicReg->pszGetTPRGC, &pVM->pdm.s.Apic.pfnGetTPRGC);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pApicReg->pszGetTPRGC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pApicReg->pszBusDeliverGC, &pVM->pdm.s.Apic.pfnBusDeliverGC);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pApicReg->pszBusDeliverGC, rc));
        }
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Apic.pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    }
    else
    {
        pVM->pdm.s.Apic.pDevInsGC           = 0;
        pVM->pdm.s.Apic.pfnGetInterruptGC   = 0;
        pVM->pdm.s.Apic.pfnSetBaseGC        = 0;
        pVM->pdm.s.Apic.pfnGetBaseGC        = 0;
        pVM->pdm.s.Apic.pfnSetTPRGC         = 0;
        pVM->pdm.s.Apic.pfnGetTPRGC         = 0;
        pVM->pdm.s.Apic.pfnBusDeliverGC     = 0;
    }

    /*
     * Resolve & initialize the R0 bits.
     */
    if (    HWACCMR3IsAllowed(pVM)
        &&  pApicReg->pszGetInterruptR0)
    {
        int rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszGetInterruptR0, &pVM->pdm.s.Apic.pfnGetInterruptR0);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszGetInterruptR0, rc));
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszSetBaseR0, &pVM->pdm.s.Apic.pfnSetBaseR0);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszSetBaseR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszGetBaseR0, &pVM->pdm.s.Apic.pfnGetBaseR0);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszGetBaseR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszSetTPRR0, &pVM->pdm.s.Apic.pfnSetTPRR0);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszSetTPRR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszGetTPRR0, &pVM->pdm.s.Apic.pfnGetTPRR0);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszGetTPRR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszBusDeliverR0, &pVM->pdm.s.Apic.pfnBusDeliverR0);
            AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszBusDeliverR0, rc));
        }
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Apic.pDevInsR0 = MMHyperR3ToR0(pVM, pDevIns);
        Assert(pVM->pdm.s.Apic.pDevInsR0);
    }
    else
    {
        pVM->pdm.s.Apic.pfnGetInterruptR0   = 0;
        pVM->pdm.s.Apic.pfnSetBaseR0        = 0;
        pVM->pdm.s.Apic.pfnGetBaseR0        = 0;
        pVM->pdm.s.Apic.pfnSetTPRR0         = 0;
        pVM->pdm.s.Apic.pfnGetTPRR0         = 0;
        pVM->pdm.s.Apic.pfnBusDeliverR0     = 0;
        pVM->pdm.s.Apic.pDevInsR0           = 0;
    }

    /*
     * Initialize the HC bits.
     */
    pVM->pdm.s.Apic.pDevInsR3           = pDevIns;
    pVM->pdm.s.Apic.pfnGetInterruptR3   = pApicReg->pfnGetInterruptHC;
    pVM->pdm.s.Apic.pfnSetBaseR3        = pApicReg->pfnSetBaseHC;
    pVM->pdm.s.Apic.pfnGetBaseR3        = pApicReg->pfnGetBaseHC;
    pVM->pdm.s.Apic.pfnSetTPRR3         = pApicReg->pfnSetTPRHC;
    pVM->pdm.s.Apic.pfnGetTPRR3         = pApicReg->pfnGetTPRHC;
    pVM->pdm.s.Apic.pfnBusDeliverR3     = pApicReg->pfnBusDeliverHC;
    Log(("PDM: Registered APIC device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppApicHlpR3 = &g_pdmR3DevApicHlp;
    LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLP::pfnIOAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_IOAPICRegister(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: pIoApicReg=%p:{.u32Version=%#x, .pfnSetIrqHC=%p, .pszSetIrqGC=%p:{%s}} ppIoApicHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pIoApicReg, pIoApicReg->u32Version, pIoApicReg->pfnSetIrqHC, pIoApicReg->pszSetIrqGC,
             pIoApicReg->pszSetIrqGC, ppIoApicHlpR3));

    /*
     * Validate input.
     */
    if (pIoApicReg->u32Version != PDM_IOAPICREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pIoApicReg->u32Version, PDM_IOAPICREG_VERSION));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!pIoApicReg->pfnSetIrqHC)
    {
        Assert(pIoApicReg->pfnSetIrqHC);
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (HC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pIoApicReg->pszSetIrqGC
        &&  !VALID_PTR(pIoApicReg->pszSetIrqGC))
    {
        Assert(VALID_PTR(pIoApicReg->pszSetIrqGC));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pIoApicReg->pszSetIrqR0
        &&  !VALID_PTR(pIoApicReg->pszSetIrqR0))
    {
        Assert(VALID_PTR(pIoApicReg->pszSetIrqR0));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!ppIoApicHlpR3)
    {
        Assert(ppIoApicHlpR3);
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (ppApicHlp)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * The I/O APIC requires the APIC to be present (hacks++).
     * If the I/O APIC does GC stuff so must the APIC.
     */
    PVM pVM = pDevIns->Internal.s.pVMHC;
    if (!pVM->pdm.s.Apic.pDevInsR3)
    {
        AssertMsgFailed(("Configuration error / Init order error! No APIC!\n"));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (no APIC)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pIoApicReg->pszSetIrqGC
        &&  !pVM->pdm.s.Apic.pDevInsGC)
    {
        AssertMsgFailed(("Configuration error! APIC doesn't do GC, I/O APIC does!\n"));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (no GC APIC)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one I/O APIC device.
     */
    if (pVM->pdm.s.IoApic.pDevInsR3)
    {
        AssertMsgFailed(("Only one ioapic device is supported!\n"));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc (only one)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Resolve & initialize the GC bits.
     */
    if (pIoApicReg->pszSetIrqGC)
    {
        int rc = PDMR3GetSymbolGCLazy(pVM, pDevIns->pDevReg->szGCMod, pIoApicReg->pszSetIrqGC, &pVM->pdm.s.IoApic.pfnSetIrqGC);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szGCMod, pIoApicReg->pszSetIrqGC, rc));
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.IoApic.pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    }
    else
    {
        pVM->pdm.s.IoApic.pDevInsGC   = 0;
        pVM->pdm.s.IoApic.pfnSetIrqGC = 0;
    }

    /*
     * Resolve & initialize the R0 bits.
     */
    if (    HWACCMR3IsAllowed(pVM)
        &&  pIoApicReg->pszSetIrqR0)
    {
        int rc = PDMR3GetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pIoApicReg->pszSetIrqR0, &pVM->pdm.s.IoApic.pfnSetIrqR0);
        AssertMsgRC(rc, ("%s::%s rc=%Vrc\n", pDevIns->pDevReg->szR0Mod, pIoApicReg->pszSetIrqR0, rc));
        if (VBOX_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.IoApic.pDevInsR0 = MMHyperR3ToR0(pVM, pDevIns);
        Assert(pVM->pdm.s.IoApic.pDevInsR0);
    }
    else
    {
        pVM->pdm.s.IoApic.pfnSetIrqR0 = 0;
        pVM->pdm.s.IoApic.pDevInsR0   = 0;
    }

    /*
     * Initialize the HC bits.
     */
    pVM->pdm.s.IoApic.pDevInsR3   = pDevIns;
    pVM->pdm.s.IoApic.pfnSetIrqR3 = pIoApicReg->pfnSetIrqHC;
    Log(("PDM: Registered I/O APIC device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppIoApicHlpR3 = &g_pdmR3DevIoApicHlp;
    LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLP::pfnDMACRegister */
static DECLCALLBACK(int) pdmR3DevHlp_DMACRegister(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: pDmacReg=%p:{.u32Version=%#x, .pfnRun=%p, .pfnRegister=%p, .pfnReadMemory=%p, .pfnWriteMemory=%p, .pfnSetDREQ=%p, .pfnGetChannelMode=%p} ppDmacHlp=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDmacReg, pDmacReg->u32Version, pDmacReg->pfnRun, pDmacReg->pfnRegister,
             pDmacReg->pfnReadMemory, pDmacReg->pfnWriteMemory, pDmacReg->pfnSetDREQ, pDmacReg->pfnGetChannelMode, ppDmacHlp));

    /*
     * Validate input.
     */
    if (pDmacReg->u32Version != PDM_DMACREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pDmacReg->u32Version,
                         PDM_DMACREG_VERSION));
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Vrc (version)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pDmacReg->pfnRun
        ||  !pDmacReg->pfnRegister
        ||  !pDmacReg->pfnReadMemory
        ||  !pDmacReg->pfnWriteMemory
        ||  !pDmacReg->pfnSetDREQ
        ||  !pDmacReg->pfnGetChannelMode)
    {
        Assert(pDmacReg->pfnRun);
        Assert(pDmacReg->pfnRegister);
        Assert(pDmacReg->pfnReadMemory);
        Assert(pDmacReg->pfnWriteMemory);
        Assert(pDmacReg->pfnSetDREQ);
        Assert(pDmacReg->pfnGetChannelMode);
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Vrc (callbacks)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppDmacHlp)
    {
        Assert(ppDmacHlp);
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Vrc (ppDmacHlp)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one DMA device.
     */
    PVM pVM = pDevIns->Internal.s.pVMHC;
    if (pVM->pdm.s.pDmac)
    {
        AssertMsgFailed(("Only one DMA device is supported!\n"));
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Vrc\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate and initialize pci bus structure.
     */
    int rc = VINF_SUCCESS;
    PPDMDMAC  pDmac = (PPDMDMAC)MMR3HeapAlloc(pDevIns->Internal.s.pVMHC, MM_TAG_PDM_DEVICE, sizeof(*pDmac));
    if (pDmac)
    {
        pDmac->pDevIns   = pDevIns;
        pDmac->Reg       = *pDmacReg;
        pVM->pdm.s.pDmac = pDmac;

        /* set the helper pointer. */
        *ppDmacHlp = &g_pdmR3DevDmacHlp;
        Log(("PDM: Registered DMAC device '%s'/%d pDevIns=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnPhysRead */
static DECLCALLBACK(void) pdmR3DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PhysRead: caller='%s'/%d: GCPhys=%VGp pvBuf=%p cbRead=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    /*
     * For the convenience of the device we put no thread restriction on this interface.
     * That means we'll have to check which thread we're in and choose our path.
     */
#ifdef PDM_PHYS_READWRITE_FROM_ANY_THREAD
    PGMPhysRead(pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbRead);
#else
    if (VM_IS_EMT(pDevIns->Internal.s.pVMHC) || VMMR3LockIsOwner(pDevIns->Internal.s.pVMHC))
        PGMPhysRead(pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbRead);
    else
    {
        Log(("pdmR3DevHlp_PhysRead: caller='%s'/%d: Requesting call in EMT...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        PVMREQ pReq;
        int rc = VMR3ReqCallVoid(pDevIns->Internal.s.pVMHC, &pReq, RT_INDEFINITE_WAIT,
                                 (PFNRT)PGMPhysRead, 4, pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbRead);
        while (rc == VERR_TIMEOUT)
            rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertReleaseRC(rc);
        VMR3ReqFree(pReq);
    }
#endif
    Log(("pdmR3DevHlp_PhysRead: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnPhysWrite */
static DECLCALLBACK(void) pdmR3DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PhysWrite: caller='%s'/%d: GCPhys=%VGp pvBuf=%p cbWrite=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    /*
     * For the convenience of the device we put no thread restriction on this interface.
     * That means we'll have to check which thread we're in and choose our path.
     */
#ifdef PDM_PHYS_READWRITE_FROM_ANY_THREAD
    PGMPhysWrite(pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbWrite);
#else
    if (VM_IS_EMT(pDevIns->Internal.s.pVMHC) || VMMR3LockIsOwner(pDevIns->Internal.s.pVMHC))
        PGMPhysWrite(pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbWrite);
    else
    {
        Log(("pdmR3DevHlp_PhysWrite: caller='%s'/%d: Requesting call in EMT...\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        PVMREQ pReq;
        int rc = VMR3ReqCallVoid(pDevIns->Internal.s.pVMHC, &pReq, RT_INDEFINITE_WAIT,
                                 (PFNRT)PGMPhysWrite, 4, pDevIns->Internal.s.pVMHC, GCPhys, pvBuf, cbWrite);
        while (rc == VERR_TIMEOUT)
            rc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertReleaseRC(rc);
        VMR3ReqFree(pReq);
    }
#endif
    Log(("pdmR3DevHlp_PhysWrite: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnPhysReadGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_PhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PhysReadGCVirt: caller='%s'/%d: pvDst=%p GCVirt=%VGv cb=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pvDst, GCVirtSrc, cb));

    if (!VM_IS_EMT(pVM))
        return VERR_ACCESS_DENIED;

    int rc = PGMPhysReadGCPtr(pVM, pvDst, GCVirtSrc, cb);

    LogFlow(("pdmR3DevHlp_PhysReadGCVirt: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));

    return rc;
}


/** @copydoc PDMDEVHLP::pfnPhysWriteGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_PhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PhysWriteGCVirt: caller='%s'/%d: GCVirtDst=%VGv pvSrc=%p cb=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCVirtDst, pvSrc, cb));

    if (!VM_IS_EMT(pVM))
        return VERR_ACCESS_DENIED;

    int rc = PGMPhysWriteGCPtr(pVM, GCVirtDst, pvSrc, cb);

    LogFlow(("pdmR3DevHlp_PhysWriteGCVirt: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));

    return rc;
}


/** @copydoc PDMDEVHLP::pfnPhysReserve */
static DECLCALLBACK(int) pdmR3DevHlp_PhysReserve(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_PhysReserve: caller='%s'/%d: GCPhys=%VGp cbRange=%#x pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, cbRange, pszDesc, pszDesc));

    int rc = MMR3PhysReserve(pDevIns->Internal.s.pVMHC, GCPhys, cbRange, pszDesc);

    LogFlow(("pdmR3DevHlp_PhysReserve: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));

    return rc;
}

/** @copydoc PDMDEVHLP::pfnPhysGCPtr2GCPhys */
static DECLCALLBACK(int) pdmR3DevHlp_PhysGCPtr2GCPhys(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PhysGCPtr2GCPhys: caller='%s'/%d: GCPtr=%VGv pGCPhys=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPtr, pGCPhys));

    if (!VM_IS_EMT(pVM))
        return VERR_ACCESS_DENIED;

    int rc = PGMPhysGCPtr2GCPhys(pVM, GCPtr, pGCPhys);

    LogFlow(("pdmR3DevHlp_PhysGCPtr2GCPhys: caller='%s'/%d: returns %Vrc *pGCPhys=%VGp\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc, *pGCPhys));

    return rc;
}

/** @copydoc PDMDEVHLP::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmR3DevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);

    bool fRc = PGMPhysIsA20Enabled(pDevIns->Internal.s.pVMHC);

    LogFlow(("pdmR3DevHlp_A20IsEnabled: caller='%s'/%d: returns %d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, fRc));
    return fRc;
}


/** @copydoc PDMDEVHLP::pfnA20Set */
static DECLCALLBACK(void) pdmR3DevHlp_A20Set(PPDMDEVINS pDevIns, bool fEnable)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_A20Set: caller='%s'/%d: fEnable=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, fEnable));
    //Assert(*(unsigned *)&fEnable <= 1);
    PGMR3PhysSetA20(pDevIns->Internal.s.pVMHC, fEnable);
}


/** @copydoc PDMDEVHLP::pfnVMReset */
static DECLCALLBACK(int) pdmR3DevHlp_VMReset(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_VMReset: caller='%s'/%d: VM_FF_RESET %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pVM, VM_FF_RESET)));

    /*
     * We postpone this operation because we're likely to be inside a I/O instruction
     * and the EIP will be updated when we return.
     * We still return VINF_EM_RESET to break out of any execution loops and force FF evaluation.
     */
    bool fHaltOnReset;
    int rc = CFGMR3QueryBool(CFGMR3GetChild(CFGMR3GetRoot(pVM), "PDM"), "HaltOnReset", &fHaltOnReset);
    if (VBOX_SUCCESS(rc) && fHaltOnReset)
    {
        Log(("pdmR3DevHlp_VMReset: Halt On Reset!\n"));
        rc = VINF_EM_HALT;
    }
    else
    {
        VM_FF_SET(pVM, VM_FF_RESET);
        rc = VINF_EM_RESET;
    }

    LogFlow(("pdmR3DevHlp_VMReset: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnVMSuspend */
static DECLCALLBACK(int) pdmR3DevHlp_VMSuspend(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_VMSuspend: caller='%s'/%d:\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));

    int rc = VMR3Suspend(pDevIns->Internal.s.pVMHC);

    LogFlow(("pdmR3DevHlp_VMSuspend: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnVMPowerOff */
static DECLCALLBACK(int) pdmR3DevHlp_VMPowerOff(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    LogFlow(("pdmR3DevHlp_VMPowerOff: caller='%s'/%d:\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));

    int rc = VMR3PowerOff(pDevIns->Internal.s.pVMHC);

    LogFlow(("pdmR3DevHlp_VMPowerOff: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnLockVM */
static DECLCALLBACK(int) pdmR3DevHlp_LockVM(PPDMDEVINS pDevIns)
{
    return VMMR3Lock(pDevIns->Internal.s.pVMHC);
}


/** @copydoc PDMDEVHLP::pfnUnlockVM */
static DECLCALLBACK(int) pdmR3DevHlp_UnlockVM(PPDMDEVINS pDevIns)
{
    return VMMR3Unlock(pDevIns->Internal.s.pVMHC);
}


/** @copydoc PDMDEVHLP::pfnAssertVMLock */
static DECLCALLBACK(bool) pdmR3DevHlp_AssertVMLock(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PVM pVM = pDevIns->Internal.s.pVMHC;
    if (VMMR3LockIsOwner(pVM))
        return true;

    RTNATIVETHREAD NativeThreadOwner = VMMR3LockGetOwner(pVM);
    RTTHREAD ThreadOwner = RTThreadFromNative(NativeThreadOwner);
    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertVMLocked '%s'/%d ThreadOwner=%RTnthrd/%RTthrd/'%s' Self='%s'\n",
                pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
                NativeThreadOwner, ThreadOwner, RTThreadGetName(ThreadOwner), RTThreadSelfName());
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    return false;
}

/** @copydoc PDMDEVHLP::pfnDMARegister */
static DECLCALLBACK(int) pdmR3DevHlp_DMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMARegister: caller='%s'/%d: uChannel=%d pfnTransferHandler=%p pvUser=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, pfnTransferHandler, pvUser));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
        pVM->pdm.s.pDmac->Reg.pfnRegister(pVM->pdm.s.pDmac->pDevIns, uChannel, pfnTransferHandler, pvUser);
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMARegister: caller='%s'/%d: returns %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLP::pfnDMAReadMemory */
static DECLCALLBACK(int) pdmR3DevHlp_DMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMAReadMemory: caller='%s'/%d: uChannel=%d pvBuffer=%p off=%#x cbBlock=%#x pcbRead=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, pvBuffer, off, cbBlock, pcbRead));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
    {
        uint32_t cb = pVM->pdm.s.pDmac->Reg.pfnReadMemory(pVM->pdm.s.pDmac->pDevIns, uChannel, pvBuffer, off, cbBlock);
        if (pcbRead)
            *pcbRead = cb;
    }
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMAReadMemory: caller='%s'/%d: returns %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLP::pfnDMAWriteMemory */
static DECLCALLBACK(int) pdmR3DevHlp_DMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMAWriteMemory: caller='%s'/%d: uChannel=%d pvBuffer=%p off=%#x cbBlock=%#x pcbWritten=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, pvBuffer, off, cbBlock, pcbWritten));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
    {
        uint32_t cb = pVM->pdm.s.pDmac->Reg.pfnWriteMemory(pVM->pdm.s.pDmac->pDevIns, uChannel, pvBuffer, off, cbBlock);
        if (pcbWritten)
            *pcbWritten = cb;
    }
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMAWriteMemory: caller='%s'/%d: returns %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLP::pfnDMASetDREQ */
static DECLCALLBACK(int) pdmR3DevHlp_DMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMASetDREQ: caller='%s'/%d: uChannel=%d uLevel=%d\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, uLevel));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
        pVM->pdm.s.pDmac->Reg.pfnSetDREQ(pVM->pdm.s.pDmac->pDevIns, uChannel, uLevel);
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMASetDREQ: caller='%s'/%d: returns %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLP::pfnDMAGetChannelMode */
static DECLCALLBACK(uint8_t) pdmR3DevHlp_DMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMAGetChannelMode: caller='%s'/%d: uChannel=%d\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel));
    uint8_t u8Mode;
    if (pVM->pdm.s.pDmac)
        u8Mode = pVM->pdm.s.pDmac->Reg.pfnGetChannelMode(pVM->pdm.s.pDmac->pDevIns, uChannel);
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        u8Mode = 3 << 2 /* illegal mode type */;
    }
    LogFlow(("pdmR3DevHlp_DMAGetChannelMode: caller='%s'/%d: returns %#04x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, u8Mode));
    return u8Mode;
}

/** @copydoc PDMDEVHLP::pfnDMASchedule */
static DECLCALLBACK(void) pdmR3DevHlp_DMASchedule(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMASchedule: caller='%s'/%d: VM_FF_PDM_DMA %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pVM, VM_FF_PDM_DMA)));

    AssertMsg(pVM->pdm.s.pDmac, ("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
    VM_FF_SET(pVM, VM_FF_PDM_DMA);
    REMR3NotifyDmaPending(pVM);
    VMR3NotifyFF(pVM, true);
}


/** @copydoc PDMDEVHLP::pfnCMOSWrite */
static DECLCALLBACK(int) pdmR3DevHlp_CMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: iReg=%#04x u8Value=%#04x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iReg, u8Value));
    int rc;
    if (pVM->pdm.s.pRtc)
        rc = pVM->pdm.s.pRtc->Reg.pfnWrite(pVM->pdm.s.pRtc->pDevIns, iReg, u8Value);
    else
        rc = VERR_PDM_NO_RTC_INSTANCE;

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: return %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnCMOSRead */
static DECLCALLBACK(int) pdmR3DevHlp_CMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    VM_ASSERT_EMT(pVM);

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: iReg=%#04x pu8Value=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iReg, pu8Value));
    int rc;
    if (pVM->pdm.s.pRtc)
        rc = pVM->pdm.s.pRtc->Reg.pfnRead(pVM->pdm.s.pRtc->pDevIns, iReg, pu8Value);
    else
        rc = VERR_PDM_NO_RTC_INSTANCE;

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: return %Vrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLP::pfnGetCpuId */
static DECLCALLBACK(void) pdmR3DevHlp_GetCpuId(PPDMDEVINS pDevIns, uint32_t iLeaf,
                                               uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_GetCpuId: caller='%s'/%d: iLeaf=%d pEax=%p pEbx=%p pEcx=%p pEdx=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iLeaf, pEax, pEbx, pEcx, pEdx));
    AssertPtr(pEax); AssertPtr(pEbx); AssertPtr(pEcx); AssertPtr(pEdx);

    CPUMGetGuestCpuId(pDevIns->Internal.s.pVMHC, iLeaf, pEax, pEbx, pEcx, pEdx);

    LogFlow(("pdmR3DevHlp_GetCpuId: caller='%s'/%d: returns void - *pEax=%#x *pEbx=%#x *pEcx=%#x *pEdx=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, *pEax, *pEbx, *pEcx, *pEdx));
}


/** @copydoc PDMDEVHLP::pfnROMProtectShadow */
static DECLCALLBACK(int) pdmR3DevHlp_ROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_ROMProtectShadow: caller='%s'/%d: GCPhysStart=%VGp cbRange=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange));

    int rc = MMR3PhysRomProtect(pDevIns->Internal.s.pVMHC, GCPhysStart, cbRange);

    LogFlow(("pdmR3DevHlp_ROMProtectShadow: caller='%s'/%d: returns %Vrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}



/** @copydoc PDMDEVHLP::pfnGetVM */
static DECLCALLBACK(PVM) pdmR3DevHlp_Untrusted_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return NULL;
}


/** @copydoc PDMDEVHLP::pfnPCIBusRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PCIBusRegister(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pPciBusReg);
    NOREF(ppPciHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PICRegister(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pPicReg);
    NOREF(ppPicHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_APICRegister(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pApicReg);
    NOREF(ppApicHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnIOAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_IOAPICRegister(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pIoApicReg);
    NOREF(ppIoApicHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnDMACRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMACRegister(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pDmacReg);
    NOREF(ppDmacHlp);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnPhysRead */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(pvBuf);
    NOREF(cbRead);
}


/** @copydoc PDMDEVHLP::pfnPhysWrite */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(pvBuf);
    NOREF(cbWrite);
}


/** @copydoc PDMDEVHLP::pfnPhysReadGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pvDst);
    NOREF(GCVirtSrc);
    NOREF(cb);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnPhysWriteGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCVirtDst);
    NOREF(pvSrc);
    NOREF(cb);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnPhysReserve */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysReserve(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(cbRange);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnPhys2HCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_Obsolete_Phys2HCVirt(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR ppvHC)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(cbRange);
    NOREF(ppvHC);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnPhysGCPtr2HCPtr */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_Obsolete_PhysGCPtr2HCPtr(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTHCPTR pHCPtr)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPtr);
    NOREF(pHCPtr);
    return VERR_ACCESS_DENIED;
}

/** @copydoc PDMDEVHLP::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmR3DevHlp_Untrusted_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return false;
}


/** @copydoc PDMDEVHLP::pfnA20Set */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_A20Set(PPDMDEVINS pDevIns, bool fEnable)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(fEnable);
}


/** @copydoc PDMDEVHLP::pfnVMReset */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMReset(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnVMSuspend */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMSuspend(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnVMPowerOff */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMPowerOff(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnLockVM */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_LockVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnUnlockVM */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_UnlockVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnAssertVMLock */
static DECLCALLBACK(bool) pdmR3DevHlp_Untrusted_AssertVMLock(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return false;
}


/** @copydoc PDMDEVHLP::pfnDMARegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnDMAReadMemory */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    if (pcbRead)
        *pcbRead = 0;
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnDMAWriteMemory */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    if (pcbWritten)
        *pcbWritten = 0;
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnDMASetDREQ */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnDMAGetChannelMode */
static DECLCALLBACK(uint8_t) pdmR3DevHlp_Untrusted_DMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return 3 << 2 /* illegal mode type */;
}


/** @copydoc PDMDEVHLP::pfnDMASchedule */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_DMASchedule(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnCMOSWrite */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_CMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnCMOSRead */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_CMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLP::pfnQueryCPUId */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_QueryCPUId(PPDMDEVINS pDevIns, uint32_t iLeaf,
                                                           uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLP::pfnROMProtectShadow */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_ROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMPICHLPR3::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR3PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    LogFlow(("pdmR3PicHlp_SetInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT_PIC %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pVM, VM_FF_INTERRUPT_PIC)));
    VM_FF_SET(pVM, VM_FF_INTERRUPT_PIC);
    REMR3NotifyInterruptSet(pVM);
#ifdef VBOX_WITH_PDM_LOCK
    VMR3NotifyFF(pVM, true);
#endif
}


/** @copydoc PDMPICHLPR3::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR3PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3PicHlp_ClearInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT_PIC %d -> 0\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_PIC)));
    VM_FF_CLEAR(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_PIC);
    REMR3NotifyInterruptClear(pDevIns->Internal.s.pVMHC);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMPICHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMPICHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */


/** @copydoc PDMPICHLPR3::pfnGetGCHelpers */
static DECLCALLBACK(PCPDMPICHLPGC) pdmR3PicHlp_GetGCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    RTGCPTR pGCHelpers = 0;
    int rc = PDMR3GetSymbolGC(pDevIns->Internal.s.pVMHC, NULL, "g_pdmGCPicHlp", &pGCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pGCHelpers);
    LogFlow(("pdmR3PicHlp_GetGCHelpers: caller='%s'/%d: returns %VGv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pGCHelpers));
    return pGCHelpers;
}


/** @copydoc PDMPICHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMPICHLPR0) pdmR3PicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    PCPDMPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3GetSymbolR0(pDevIns->Internal.s.pVMHC, NULL, "g_pdmR0PicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3PicHlp_GetR0Helpers: caller='%s'/%d: returns %VHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/** @copydoc PDMAPICHLPR3::pfnSetInterruptFF */
static DECLCALLBACK(void) pdmR3ApicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
    LogFlow(("pdmR3ApicHlp_SetInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pVM, VM_FF_INTERRUPT_APIC)));
    VM_FF_SET(pVM, VM_FF_INTERRUPT_APIC);
    REMR3NotifyInterruptSet(pVM);
#ifdef VBOX_WITH_PDM_LOCK
    VMR3NotifyFF(pVM, true);
#endif
}


/** @copydoc PDMAPICHLPR3::pfnClearInterruptFF */
static DECLCALLBACK(void) pdmR3ApicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_ClearInterruptFF: caller='%s'/%d: VM_FF_INTERRUPT %d -> 0\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_APIC)));
    VM_FF_CLEAR(pDevIns->Internal.s.pVMHC, VM_FF_INTERRUPT_APIC);
    REMR3NotifyInterruptClear(pDevIns->Internal.s.pVMHC);
}


/** @copydoc PDMAPICHLPR3::pfnChangeFeature */
static DECLCALLBACK(void) pdmR3ApicHlp_ChangeFeature(PPDMDEVINS pDevIns, bool fEnabled)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3ApicHlp_ClearInterruptFF: caller='%s'/%d: fEnabled=%RTbool\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, fEnabled));
    if (fEnabled)
        CPUMSetGuestCpuIdFeature(pDevIns->Internal.s.pVMHC, CPUMCPUIDFEATURE_APIC);
    else
        CPUMClearGuestCpuIdFeature(pDevIns->Internal.s.pVMHC, CPUMCPUIDFEATURE_APIC);
}

#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMAPICHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3ApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMAPICHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3ApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */


/** @copydoc PDMAPICHLPR3::pfnGetGCHelpers */
static DECLCALLBACK(PCPDMAPICHLPGC) pdmR3ApicHlp_GetGCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    RTGCPTR pGCHelpers = 0;
    int rc = PDMR3GetSymbolGC(pDevIns->Internal.s.pVMHC, NULL, "g_pdmGCApicHlp", &pGCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pGCHelpers);
    LogFlow(("pdmR3ApicHlp_GetGCHelpers: caller='%s'/%d: returns %VGv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pGCHelpers));
    return pGCHelpers;
}


/** @copydoc PDMAPICHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMAPICHLPR0) pdmR3ApicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    PCPDMAPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3GetSymbolR0(pDevIns->Internal.s.pVMHC, NULL, "g_pdmR0ApicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3ApicHlp_GetR0Helpers: caller='%s'/%d: returns %VHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/** @copydoc PDMIOAPICHLPR3::pfnApicBusDeliver */
static DECLCALLBACK(void) pdmR3IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode, uint8_t u8DeliveryMode,
                                                        uint8_t iVector, uint8_t u8Polarity, uint8_t u8TriggerMode)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMHC;
#ifndef VBOX_WITH_PDM_LOCK
    VM_ASSERT_EMT(pVM);
#endif
    LogFlow(("pdmR3IoApicHlp_ApicBusDeliver: caller='%s'/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 iVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode));
    if (pVM->pdm.s.Apic.pfnBusDeliverR3)
        pVM->pdm.s.Apic.pfnBusDeliverR3(pVM->pdm.s.Apic.pDevInsR3, u8Dest, u8DestMode, u8DeliveryMode, iVector, u8Polarity, u8TriggerMode);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMIOAPICHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMIOAPICHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */


/** @copydoc PDMIOAPICHLPR3::pfnGetGCHelpers */
static DECLCALLBACK(PCPDMIOAPICHLPGC) pdmR3IoApicHlp_GetGCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    RTGCPTR pGCHelpers = 0;
    int rc = PDMR3GetSymbolGC(pDevIns->Internal.s.pVMHC, NULL, "g_pdmGCIoApicHlp", &pGCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pGCHelpers);
    LogFlow(("pdmR3IoApicHlp_GetGCHelpers: caller='%s'/%d: returns %VGv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pGCHelpers));
    return pGCHelpers;
}


/** @copydoc PDMIOAPICHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMIOAPICHLPR0) pdmR3IoApicHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    PCPDMIOAPICHLPR0 pR0Helpers = 0;
    int rc = PDMR3GetSymbolR0(pDevIns->Internal.s.pVMHC, NULL, "g_pdmR0IoApicHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3IoApicHlp_GetR0Helpers: caller='%s'/%d: returns %VHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/** @copydoc PDMPCIHLPR3::pfnIsaSetIrq */
static DECLCALLBACK(void) pdmR3PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
#ifndef VBOX_WITH_PDM_LOCK
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
#endif
    Log4(("pdmR3PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    PDMIsaSetIrq(pDevIns->Internal.s.pVMHC, iIrq, iLevel);
}


/** @copydoc PDMPCIHLPR3::pfnIoApicSetIrq */
static DECLCALLBACK(void) pdmR3PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
#ifndef VBOX_WITH_PDM_LOCK
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
#endif
    Log4(("pdmR3PciHlp_IsaSetIrq: iIrq=%d iLevel=%d\n", iIrq, iLevel));
    PDMIoApicSetIrq(pDevIns->Internal.s.pVMHC, iIrq, iLevel);
}


#ifdef VBOX_WITH_PDM_LOCK
/** @copydoc PDMPCIHLPR3::pfnLock */
static DECLCALLBACK(int) pdmR3PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pVMHC, rc);
}


/** @copydoc PDMPCIHLPR3::pfnUnlock */
static DECLCALLBACK(void) pdmR3PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pVMHC);
}
#endif /* VBOX_WITH_PDM_LOCK */


/** @copydoc PDMPCIHLPR3::pfnGetGCHelpers */
static DECLCALLBACK(PCPDMPCIHLPGC) pdmR3PciHlp_GetGCHelpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    RTGCPTR pGCHelpers = 0;
    int rc = PDMR3GetSymbolGC(pDevIns->Internal.s.pVMHC, NULL, "g_pdmGCPciHlp", &pGCHelpers);
    AssertReleaseRC(rc);
    AssertRelease(pGCHelpers);
    LogFlow(("pdmR3IoApicHlp_GetGCHelpers: caller='%s'/%d: returns %VGv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pGCHelpers));
    return pGCHelpers;
}


/** @copydoc PDMPCIHLPR3::pfnGetR0Helpers */
static DECLCALLBACK(PCPDMPCIHLPR0) pdmR3PciHlp_GetR0Helpers(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMHC);
    PCPDMPCIHLPR0 pR0Helpers = 0;
    int rc = PDMR3GetSymbolR0(pDevIns->Internal.s.pVMHC, NULL, "g_pdmR0PciHlp", &pR0Helpers);
    AssertReleaseRC(rc);
    AssertRelease(pR0Helpers);
    LogFlow(("pdmR3IoApicHlp_GetR0Helpers: caller='%s'/%d: returns %VHv\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pR0Helpers));
    return pR0Helpers;
}


/**
 * Locates a LUN.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppLun           Where to store the pointer to the LUN if found.
 * @thread  Try only do this in EMT...
 */
int pdmR3DevFindLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMLUN *ppLun)
{
    /*
     * Iterate registered devices looking for the device.
     */
    RTUINT cchDevice = strlen(pszDevice);
    for (PPDMDEV pDev = pVM->pdm.s.pDevs; pDev; pDev = pDev->pNext)
    {
        if (    pDev->cchName == cchDevice
            &&  !memcmp(pDev->pDevReg->szDeviceName, pszDevice, cchDevice))
        {
            /*
             * Iterate device instances.
             */
            for (PPDMDEVINS pDevIns = pDev->pInstances; pDevIns; pDevIns = pDevIns->Internal.s.pPerDeviceNextHC)
            {
                if (pDevIns->iInstance == iInstance)
                {
                    /*
                     * Iterate luns.
                     */
                    for (PPDMLUN pLun = pDevIns->Internal.s.pLunsHC; pLun; pLun = pLun->pNext)
                    {
                        if (pLun->iLun == iLun)
                        {
                            *ppLun = pLun;
                            return VINF_SUCCESS;
                        }
                    }
                    return VERR_PDM_LUN_NOT_FOUND;
                }
            }
            return VERR_PDM_DEVICE_INSTANCE_NOT_FOUND;
        }
    }
    return VERR_PDM_DEVICE_NOT_FOUND;
}


/**
 * Attaches a preconfigured driver to an existing device instance.
 *
 * This is used to change drivers and suchlike at runtime.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer. Optional.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3DeviceAttach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPDMIBASE *ppBase)
{
    VM_ASSERT_EMT(pVM);
    LogFlow(("PDMR3DeviceAttach: pszDevice=%p:{%s} iInstance=%d iLun=%d ppBase=%p\n",
             pszDevice, pszDevice, iInstance, iLun, ppBase));

    /*
     * Find the LUN in question.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Can we attach anything at runtime?
         */
        PPDMDEVINS pDevIns = pLun->pDevIns;
        if (pDevIns->pDevReg->pfnAttach)
        {
            if (!pLun->pTop)
            {
                rc = pDevIns->pDevReg->pfnAttach(pDevIns, iLun);

            }
            else
                rc = VERR_PDM_DRIVER_ALREADY_ATTACHED;
        }
        else
            rc = VERR_PDM_DEVICE_NO_RT_ATTACH;

        if (ppBase)
            *ppBase = pLun->pTop ? &pLun->pTop->IBase : NULL;
    }
    else if (ppBase)
        *ppBase = NULL;

    if (ppBase)
        LogFlow(("PDMR3DeviceAttach: returns %Vrc *ppBase=%p\n", rc, *ppBase));
    else
        LogFlow(("PDMR3DeviceAttach: returns %Vrc\n", rc));
    return rc;
}


/**
 * Detaches a driver from an existing device instance.
 *
 * This is used to change drivers and suchlike at runtime.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3DeviceDetach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun)
{
    VM_ASSERT_EMT(pVM);
    LogFlow(("PDMR3DeviceDetach: pszDevice=%p:{%s} iInstance=%d iLun=%d\n",
             pszDevice, pszDevice, iInstance, iLun));

    /*
     * Find the LUN in question.
     */
    PPDMLUN pLun;
    int rc = pdmR3DevFindLun(pVM, pszDevice, iInstance, iLun, &pLun);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Can we detach anything at runtime?
         */
        PPDMDEVINS pDevIns = pLun->pDevIns;
        if (pDevIns->pDevReg->pfnDetach)
        {
            if (pLun->pTop)
                rc = pdmR3DrvDetach(pLun->pTop);
            else
                rc = VINF_PDM_NO_DRIVER_ATTACHED_TO_LUN;
        }
        else
            rc = VERR_PDM_DEVICE_NO_RT_DETACH;
    }

    LogFlow(("PDMR3DeviceDetach: returns %Vrc\n", rc));
    return rc;
}

