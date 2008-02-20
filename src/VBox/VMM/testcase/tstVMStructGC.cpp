/* $Id$ */
/** @file
 * tstVMMStructGC - Generate structure member and size checks from the GC perspective.
 *
 * This is built using the VBOXGC template but linked into a host
 * ring-3 executable, rather hacky.
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


/*
 * Sanity checks.
 */
#ifndef IN_GC
# error Incorrect template!
#endif
#if defined(IN_RING3) || defined(IN_RING0)
# error Incorrect template!
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cfgm.h>
#include <VBox/cpum.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/trpm.h>
#include <VBox/vmm.h>
#include <VBox/stam.h>
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include "CFGMInternal.h"
#include "CPUMInternal.h"
#include "MMInternal.h"
#include "PGMInternal.h"
#include "SELMInternal.h"
#include "TRPMInternal.h"
#include "TMInternal.h"
#include "IOMInternal.h"
#include "REMInternal.h"
#include "HWACCMInternal.h"
#include "PATMInternal.h"
#include "VMMInternal.h"
#include "DBGFInternal.h"
#include "STAMInternal.h"
#include "CSAMInternal.h"
#include "EMInternal.h"
#include "REMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/x86.h>

/* we don't use iprt here because we're pretending to be in GC! */
#include <stdio.h>

#define GEN_CHECK_SIZE(s)   printf("    CHECK_SIZE(%s, %u);\n", #s, (unsigned)sizeof(s))
#define GEN_CHECK_OFF(s, m) printf("    CHECK_OFF(%s, %u, %s);\n", #s, (unsigned)RT_OFFSETOF(s, m), #m)

int main()
{
    GEN_CHECK_SIZE(CFGM);

    GEN_CHECK_SIZE(CPUM); // has .mac
    GEN_CHECK_SIZE(CPUMHOSTCTX);
    GEN_CHECK_SIZE(CPUMCTX);
    GEN_CHECK_SIZE(CPUMCTXCORE);
    GEN_CHECK_SIZE(STAMRATIOU32);
    GEN_CHECK_SIZE(AVLOHCPHYSNODECORE);
    GEN_CHECK_SIZE(AVLOGCPHYSNODECORE);
    GEN_CHECK_SIZE(AVLROGCPHYSNODECORE);
    GEN_CHECK_SIZE(AVLOGCPTRNODECORE);
    GEN_CHECK_SIZE(AVLROGCPTRNODECORE);
    GEN_CHECK_SIZE(AVLOIOPORTNODECORE);
    GEN_CHECK_SIZE(AVLROIOPORTNODECORE);

    GEN_CHECK_SIZE(DBGF);
    GEN_CHECK_OFF(DBGF, offVM);
    GEN_CHECK_OFF(DBGF, fAttached);
    GEN_CHECK_OFF(DBGF, fStoppedInHyper);
    GEN_CHECK_OFF(DBGF, PingPong);
    GEN_CHECK_OFF(DBGF, DbgEvent);
    GEN_CHECK_OFF(DBGF, enmVMMCmd);
    GEN_CHECK_OFF(DBGF, VMMCmdData);
    GEN_CHECK_OFF(DBGF, pInfoFirst);
    GEN_CHECK_OFF(DBGF, InfoCritSect);
    GEN_CHECK_OFF(DBGF, SymbolTree);
    GEN_CHECK_OFF(DBGF, pSymbolSpace);
    GEN_CHECK_OFF(DBGF, fSymInited);
    GEN_CHECK_OFF(DBGF, cHwBreakpoints);
    GEN_CHECK_OFF(DBGF, cBreakpoints);
    GEN_CHECK_OFF(DBGF, aHwBreakpoints);
    GEN_CHECK_OFF(DBGF, aBreakpoints);
    GEN_CHECK_OFF(DBGF, iActiveBp);
    GEN_CHECK_OFF(DBGF, fSingleSteppingRaw);
    GEN_CHECK_SIZE(DBGFEVENT);

    GEN_CHECK_SIZE(EM);
    GEN_CHECK_OFF(EM, offVM);
    GEN_CHECK_OFF(EM, pCtx);
    GEN_CHECK_OFF(EM, enmState);
    GEN_CHECK_OFF(EM, fForceRAW);
    GEN_CHECK_OFF(EM, u.achPaddingFatalLongJump);
    GEN_CHECK_OFF(EM, StatForcedActions);
    GEN_CHECK_OFF(EM, StatTotalClis);
    GEN_CHECK_OFF(EM, pStatsHC);
    GEN_CHECK_OFF(EM, pStatsGC);
    GEN_CHECK_OFF(EM, pCliStatTree);

    GEN_CHECK_SIZE(IOM);

    GEN_CHECK_SIZE(IOMMMIORANGER0);
    GEN_CHECK_OFF(IOMMMIORANGER0, GCPhys);
    GEN_CHECK_OFF(IOMMMIORANGER0, cbSize);
    GEN_CHECK_OFF(IOMMMIORANGER0, pvUser);
    GEN_CHECK_OFF(IOMMMIORANGER0, pDevIns);
    GEN_CHECK_OFF(IOMMMIORANGER0, pfnWriteCallback);
    GEN_CHECK_OFF(IOMMMIORANGER0, pszDesc);

    GEN_CHECK_SIZE(IOMMMIORANGEGC);
    GEN_CHECK_OFF(IOMMMIORANGEGC, GCPhys);
    GEN_CHECK_OFF(IOMMMIORANGEGC, cbSize);
    GEN_CHECK_OFF(IOMMMIORANGEGC, pvUser);
    GEN_CHECK_OFF(IOMMMIORANGEGC, pDevIns);
    GEN_CHECK_OFF(IOMMMIORANGEGC, pfnWriteCallback);
    GEN_CHECK_OFF(IOMMMIORANGEGC, pszDesc);

    GEN_CHECK_SIZE(IOMMMIOSTATS);
    GEN_CHECK_OFF(IOMMMIOSTATS, ReadR3);

    GEN_CHECK_SIZE(IOMIOPORTRANGER0);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, Port);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, cPorts);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, pvUser);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, pDevIns);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, pszDesc);

    GEN_CHECK_SIZE(IOMIOPORTRANGEGC);
    GEN_CHECK_OFF(IOMIOPORTRANGEGC, Port);
    GEN_CHECK_OFF(IOMIOPORTRANGEGC, cPorts);
    GEN_CHECK_OFF(IOMIOPORTRANGEGC, pvUser);
    GEN_CHECK_OFF(IOMIOPORTRANGEGC, pDevIns);
    GEN_CHECK_OFF(IOMIOPORTRANGEGC, pszDesc);

    GEN_CHECK_SIZE(IOMIOPORTSTATS);
    GEN_CHECK_OFF(IOMIOPORTSTATS, InR3);

    GEN_CHECK_SIZE(IOMTREES);
    GEN_CHECK_OFF(IOMTREES, IOPortTreeR3);
    GEN_CHECK_OFF(IOMTREES, IOPortTreeR0);
    GEN_CHECK_OFF(IOMTREES, IOPortTreeGC);
    GEN_CHECK_OFF(IOMTREES, MMIOTreeR3);
    GEN_CHECK_OFF(IOMTREES, MMIOTreeR0);
    GEN_CHECK_OFF(IOMTREES, MMIOTreeGC);
    GEN_CHECK_OFF(IOMTREES, IOPortStatTree);
    GEN_CHECK_OFF(IOMTREES, MMIOStatTree);

    GEN_CHECK_SIZE(MM);
    GEN_CHECK_OFF(MM, offVM);
    GEN_CHECK_OFF(MM, offHyperNextStatic);
    GEN_CHECK_OFF(MM, cbHyperArea);
    GEN_CHECK_OFF(MM, fDoneMMR3InitPaging);
    GEN_CHECK_OFF(MM, fPGMInitialized);
    GEN_CHECK_OFF(MM, offLookupHyper);
    GEN_CHECK_OFF(MM, pHyperHeapHC);
    GEN_CHECK_OFF(MM, pHyperHeapGC);
    GEN_CHECK_OFF(MM, pLockedMem);
    GEN_CHECK_OFF(MM, pPagePool);
    GEN_CHECK_OFF(MM, pPagePoolLow);
    GEN_CHECK_OFF(MM, pvDummyPage);
    GEN_CHECK_OFF(MM, HCPhysDummyPage);
    GEN_CHECK_OFF(MM, cbRamBase);
    GEN_CHECK_OFF(MM, cBasePages);
    GEN_CHECK_OFF(MM, cShadowPages);
    GEN_CHECK_OFF(MM, cFixedPages);
    GEN_CHECK_SIZE(MMHYPERSTAT);
    GEN_CHECK_SIZE(MMHYPERCHUNK);
    GEN_CHECK_SIZE(MMHYPERCHUNKFREE);
    GEN_CHECK_SIZE(MMHYPERHEAP);
    GEN_CHECK_OFF(MMHYPERHEAP, u32Magic);
    GEN_CHECK_OFF(MMHYPERHEAP, cbHeap);
    GEN_CHECK_OFF(MMHYPERHEAP, pbHeapHC);
    GEN_CHECK_OFF(MMHYPERHEAP, pbHeapGC);
    GEN_CHECK_OFF(MMHYPERHEAP, pVMHC);
    GEN_CHECK_OFF(MMHYPERHEAP, pVMGC);
    GEN_CHECK_OFF(MMHYPERHEAP, cbFree);
    GEN_CHECK_OFF(MMHYPERHEAP, offFreeHead);
    GEN_CHECK_OFF(MMHYPERHEAP, offFreeTail);
    GEN_CHECK_OFF(MMHYPERHEAP, offPageAligned);
    GEN_CHECK_OFF(MMHYPERHEAP, HyperHeapStatTree);
    GEN_CHECK_SIZE(MMLOOKUPHYPER);
    GEN_CHECK_OFF(MMLOOKUPHYPER, offNext);
    GEN_CHECK_OFF(MMLOOKUPHYPER, off);
    GEN_CHECK_OFF(MMLOOKUPHYPER, cb);
    GEN_CHECK_OFF(MMLOOKUPHYPER, enmType);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.Locked.pvHC);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.Locked.pvR0);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.Locked.pLockedMem);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.HCPhys.pvHC);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.HCPhys.HCPhys);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.GCPhys.GCPhys);
    GEN_CHECK_OFF(MMLOOKUPHYPER, pszDesc);

    GEN_CHECK_SIZE(PDM);
    GEN_CHECK_OFF(PDM, offVM);
    GEN_CHECK_OFF(PDM, pDevs);
    GEN_CHECK_OFF(PDM, pDevInstances);
    GEN_CHECK_OFF(PDM, pUsbDevs);
    GEN_CHECK_OFF(PDM, pUsbInstances);
    GEN_CHECK_OFF(PDM, pDrvs);
    GEN_CHECK_OFF(PDM, pCritSects);
    GEN_CHECK_OFF(PDM, aPciBuses);
    GEN_CHECK_OFF(PDM, aPciBuses[0].iBus);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pDevInsR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnSetIrqR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnRegisterR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnIORegionRegisterR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnSaveExecR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnLoadExecR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnFakePCIBIOSR3);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pDevInsR0);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnSetIrqR0);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pDevInsGC);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnSetIrqGC);
    GEN_CHECK_OFF(PDM, Pic);
    GEN_CHECK_OFF(PDM, Pic.pDevInsR3);
    GEN_CHECK_OFF(PDM, Pic.pfnSetIrqR3);
    GEN_CHECK_OFF(PDM, Pic.pfnGetInterruptR3);
    GEN_CHECK_OFF(PDM, Pic.pDevInsR0);
    GEN_CHECK_OFF(PDM, Pic.pfnSetIrqR0);
    GEN_CHECK_OFF(PDM, Pic.pfnGetInterruptR0);
    GEN_CHECK_OFF(PDM, Pic.pDevInsGC);
    GEN_CHECK_OFF(PDM, Pic.pfnSetIrqGC);
    GEN_CHECK_OFF(PDM, Pic.pfnGetInterruptGC);
    GEN_CHECK_OFF(PDM, Apic);
    GEN_CHECK_OFF(PDM, Apic.pDevInsR3);
    GEN_CHECK_OFF(PDM, Apic.pfnGetInterruptR3);
    GEN_CHECK_OFF(PDM, Apic.pfnSetBaseR3);
    GEN_CHECK_OFF(PDM, Apic.pfnGetBaseR3);
    GEN_CHECK_OFF(PDM, Apic.pfnSetTPRR3);
    GEN_CHECK_OFF(PDM, Apic.pfnGetTPRR3);
    GEN_CHECK_OFF(PDM, Apic.pfnBusDeliverR3);
    GEN_CHECK_OFF(PDM, Apic.pDevInsR0);
    GEN_CHECK_OFF(PDM, Apic.pfnGetInterruptR0);
    GEN_CHECK_OFF(PDM, Apic.pfnSetBaseR0);
    GEN_CHECK_OFF(PDM, Apic.pfnGetBaseR0);
    GEN_CHECK_OFF(PDM, Apic.pfnSetTPRR0);
    GEN_CHECK_OFF(PDM, Apic.pfnGetTPRR0);
    GEN_CHECK_OFF(PDM, Apic.pfnBusDeliverR0);
    GEN_CHECK_OFF(PDM, Apic.pDevInsGC);
    GEN_CHECK_OFF(PDM, Apic.pfnGetInterruptGC);
    GEN_CHECK_OFF(PDM, Apic.pfnSetBaseGC);
    GEN_CHECK_OFF(PDM, Apic.pfnGetBaseGC);
    GEN_CHECK_OFF(PDM, Apic.pfnSetTPRGC);
    GEN_CHECK_OFF(PDM, Apic.pfnGetTPRGC);
    GEN_CHECK_OFF(PDM, Apic.pfnBusDeliverGC);
    GEN_CHECK_OFF(PDM, IoApic);
    GEN_CHECK_OFF(PDM, IoApic.pDevInsR3);
    GEN_CHECK_OFF(PDM, IoApic.pfnSetIrqR3);
    GEN_CHECK_OFF(PDM, IoApic.pDevInsR0);
    GEN_CHECK_OFF(PDM, IoApic.pfnSetIrqR0);
    GEN_CHECK_OFF(PDM, IoApic.pDevInsGC);
    GEN_CHECK_OFF(PDM, IoApic.pfnSetIrqGC);
    GEN_CHECK_OFF(PDM, pDmac);
    GEN_CHECK_OFF(PDM, pRtc);
    GEN_CHECK_OFF(PDM, pUsbHubs);
    GEN_CHECK_OFF(PDM, pDevHlpQueueGC);
    GEN_CHECK_OFF(PDM, pDevHlpQueueHC);
    GEN_CHECK_OFF(PDM, cQueuedCritSectLeaves);
    GEN_CHECK_OFF(PDM, apQueuedCritSectsLeaves);
    GEN_CHECK_OFF(PDM, pQueuesTimer);
    GEN_CHECK_OFF(PDM, pQueuesForced);
    GEN_CHECK_OFF(PDM, pQueueFlushGC);
    GEN_CHECK_OFF(PDM, pQueueFlushHC);
    GEN_CHECK_OFF(PDM, pThreads);
    GEN_CHECK_OFF(PDM, pThreadsTail);
    GEN_CHECK_OFF(PDM, cPollers);
    GEN_CHECK_OFF(PDM, apfnPollers);
    GEN_CHECK_OFF(PDM, aDrvInsPollers);
    GEN_CHECK_OFF(PDM, pTimerPollers);
#ifdef VBOX_WITH_PDM_LOCK
    GEN_CHECK_OFF(PDM, CritSect);
#endif
    GEN_CHECK_OFF(PDM, StatQueuedCritSectLeaves);
    GEN_CHECK_SIZE(PDMDEVINSINT);
    GEN_CHECK_OFF(PDMDEVINSINT, pNextHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pPerDeviceNextHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pDevHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pVMHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pVMGC);
    GEN_CHECK_OFF(PDMDEVINSINT, pLunsHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pCfgHandle);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciDeviceHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciDeviceGC);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciBusHC);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciBusGC);
    GEN_CHECK_SIZE(PDMCRITSECTINT);
    GEN_CHECK_OFF(PDMCRITSECTINT, Core);
    GEN_CHECK_OFF(PDMCRITSECTINT, pNext);
    GEN_CHECK_OFF(PDMCRITSECTINT, pvKey);
    GEN_CHECK_OFF(PDMCRITSECTINT, pVMR3);
    GEN_CHECK_OFF(PDMCRITSECTINT, pVMR0);
    GEN_CHECK_OFF(PDMCRITSECTINT, pVMGC);
    GEN_CHECK_OFF(PDMCRITSECTINT, StatContentionR0GCLock);
    GEN_CHECK_OFF(PDMCRITSECTINT, StatContentionR0GCUnlock);
    GEN_CHECK_OFF(PDMCRITSECTINT, StatContentionR3);
    GEN_CHECK_OFF(PDMCRITSECTINT, StatLocked);
    GEN_CHECK_SIZE(PDMQUEUE);
    GEN_CHECK_OFF(PDMQUEUE, pNext);
    GEN_CHECK_OFF(PDMQUEUE, enmType);
    GEN_CHECK_OFF(PDMQUEUE, u);
    GEN_CHECK_OFF(PDMQUEUE, u.Dev.pfnCallback);
    GEN_CHECK_OFF(PDMQUEUE, u.Dev.pDevIns);
    GEN_CHECK_OFF(PDMQUEUE, u.Drv.pfnCallback);
    GEN_CHECK_OFF(PDMQUEUE, u.Drv.pDrvIns);
    GEN_CHECK_OFF(PDMQUEUE, u.Int.pfnCallback);
    GEN_CHECK_OFF(PDMQUEUE, u.Ext.pfnCallback);
    GEN_CHECK_OFF(PDMQUEUE, u.Ext.pvUser);
    GEN_CHECK_OFF(PDMQUEUE, pVMHC);
    GEN_CHECK_OFF(PDMQUEUE, pVMGC);
    GEN_CHECK_OFF(PDMQUEUE, cMilliesInterval);
    GEN_CHECK_OFF(PDMQUEUE, pTimer);
    GEN_CHECK_OFF(PDMQUEUE, cbItem);
    GEN_CHECK_OFF(PDMQUEUE, cItems);
    GEN_CHECK_OFF(PDMQUEUE, pPendingHC);
    GEN_CHECK_OFF(PDMQUEUE, pPendingGC);
    GEN_CHECK_OFF(PDMQUEUE, iFreeHead);
    GEN_CHECK_OFF(PDMQUEUE, iFreeTail);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[1]);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[0].pItemGC);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[1].pItemHC);
    GEN_CHECK_SIZE(PDMDEVHLPTASK);
    GEN_CHECK_OFF(PDMDEVHLPTASK, Core);
    GEN_CHECK_OFF(PDMDEVHLPTASK, pDevInsHC);
    GEN_CHECK_OFF(PDMDEVHLPTASK, enmOp);
    GEN_CHECK_OFF(PDMDEVHLPTASK, u);
    GEN_CHECK_OFF(PDMDEVHLPTASK, u.SetIRQ.iIrq);
    GEN_CHECK_OFF(PDMDEVHLPTASK, u.SetIRQ.iLevel);

    GEN_CHECK_SIZE(PGM);
    GEN_CHECK_OFF(PGM, offVM);
    GEN_CHECK_OFF(PGM, paDynPageMap32BitPTEsGC);
    GEN_CHECK_OFF(PGM, paDynPageMapPaePTEsGC);
    GEN_CHECK_OFF(PGM, enmHostMode);
    GEN_CHECK_OFF(PGM, enmShadowMode);
    GEN_CHECK_OFF(PGM, enmGuestMode);
    GEN_CHECK_OFF(PGM, GCPhysCR3);
    GEN_CHECK_OFF(PGM, GCPtrCR3Mapping);
    GEN_CHECK_OFF(PGM, GCPhysGstCR3Monitored);
    GEN_CHECK_OFF(PGM, pGuestPDHC);
    GEN_CHECK_OFF(PGM, pGuestPDGC);
    GEN_CHECK_OFF(PGM, pGstPaePDPTRHC);
    GEN_CHECK_OFF(PGM, pGstPaePDPTRGC);
    GEN_CHECK_OFF(PGM, apGstPaePDsHC);
    GEN_CHECK_OFF(PGM, apGstPaePDsGC);
    GEN_CHECK_OFF(PGM, aGCPhysGstPaePDs);
    GEN_CHECK_OFF(PGM, aGCPhysGstPaePDsMonitored);
    GEN_CHECK_OFF(PGM, pHC32BitPD);
    GEN_CHECK_OFF(PGM, pGC32BitPD);
    GEN_CHECK_OFF(PGM, HCPhys32BitPD);
    GEN_CHECK_OFF(PGM, apHCPaePDs);
    GEN_CHECK_OFF(PGM, apGCPaePDs);
    GEN_CHECK_OFF(PGM, aHCPhysPaePDs);
    GEN_CHECK_OFF(PGM, pHCPaePDPTR);
    GEN_CHECK_OFF(PGM, pGCPaePDPTR);
    GEN_CHECK_OFF(PGM, HCPhysPaePDPTR);
    GEN_CHECK_OFF(PGM, pHCPaePML4);
    GEN_CHECK_OFF(PGM, pGCPaePML4);
    GEN_CHECK_OFF(PGM, HCPhysPaePML4);
    GEN_CHECK_OFF(PGM, pfnR3ShwRelocate);
    GEN_CHECK_OFF(PGM, pfnR3ShwExit);
    GEN_CHECK_OFF(PGM, pfnR3ShwGetPage);
    GEN_CHECK_OFF(PGM, pfnR3ShwModifyPage);
    GEN_CHECK_OFF(PGM, pfnR3ShwGetPDEByIndex);
    GEN_CHECK_OFF(PGM, pfnR3ShwSetPDEByIndex);
    GEN_CHECK_OFF(PGM, pfnR3ShwModifyPDEByIndex);
    GEN_CHECK_OFF(PGM, pfnGCShwGetPage);
    GEN_CHECK_OFF(PGM, pfnGCShwModifyPage);
    GEN_CHECK_OFF(PGM, pfnGCShwGetPDEByIndex);
    GEN_CHECK_OFF(PGM, pfnGCShwSetPDEByIndex);
    GEN_CHECK_OFF(PGM, pfnGCShwModifyPDEByIndex);
    GEN_CHECK_OFF(PGM, pfnR3GstRelocate);
    GEN_CHECK_OFF(PGM, pfnR3GstExit);
    GEN_CHECK_OFF(PGM, pfnR3GstMonitorCR3);
    GEN_CHECK_OFF(PGM, pfnR3GstUnmonitorCR3);
    GEN_CHECK_OFF(PGM, pfnR3GstMapCR3);
    GEN_CHECK_OFF(PGM, pfnR3GstUnmapCR3);
    GEN_CHECK_OFF(PGM, pfnR3GstGetPage);
    GEN_CHECK_OFF(PGM, pfnR3GstModifyPage);
    GEN_CHECK_OFF(PGM, pfnR3GstGetPDE);
    GEN_CHECK_OFF(PGM, pfnGCGstGetPage);
    GEN_CHECK_OFF(PGM, pfnGCGstModifyPage);
    GEN_CHECK_OFF(PGM, pfnGCGstGetPDE);
    GEN_CHECK_OFF(PGM, pfnR3BthRelocate);
    GEN_CHECK_OFF(PGM, pfnR3BthSyncCR3);
    GEN_CHECK_OFF(PGM, pfnR3BthTrap0eHandler);
    GEN_CHECK_OFF(PGM, pfnR3BthInvalidatePage);
    GEN_CHECK_OFF(PGM, pfnR3BthSyncPage);
    GEN_CHECK_OFF(PGM, pfnR3BthPrefetchPage);
    GEN_CHECK_OFF(PGM, pfnR3BthVerifyAccessSyncPage);
    GEN_CHECK_OFF(PGM, pfnR3BthAssertCR3);
    GEN_CHECK_OFF(PGM, pfnGCBthTrap0eHandler);
    GEN_CHECK_OFF(PGM, pfnGCBthInvalidatePage);
    GEN_CHECK_OFF(PGM, pfnGCBthSyncPage);
    GEN_CHECK_OFF(PGM, pfnGCBthPrefetchPage);
    GEN_CHECK_OFF(PGM, pfnGCBthVerifyAccessSyncPage);
    GEN_CHECK_OFF(PGM, pfnGCBthAssertCR3);
    GEN_CHECK_OFF(PGM, pRamRangesR3);
    GEN_CHECK_OFF(PGM, pRamRangesR0);
    GEN_CHECK_OFF(PGM, pRamRangesGC);
    GEN_CHECK_OFF(PGM, pRomRangesR3);
    GEN_CHECK_OFF(PGM, pRomRangesR0);
    GEN_CHECK_OFF(PGM, pRomRangesGC);
    GEN_CHECK_OFF(PGM, cbRamSize);
    GEN_CHECK_OFF(PGM, pTreesHC);
    GEN_CHECK_OFF(PGM, pTreesGC);
    GEN_CHECK_OFF(PGM, pMappingsR3);
    GEN_CHECK_OFF(PGM, pMappingsGC);
    GEN_CHECK_OFF(PGM, pMappingsR0);
    GEN_CHECK_OFF(PGM, fMappingsFixed);
    GEN_CHECK_OFF(PGM, GCPtrMappingFixed);
    GEN_CHECK_OFF(PGM, cbMappingFixed);
    GEN_CHECK_OFF(PGM, pInterPD);
    GEN_CHECK_OFF(PGM, apInterPTs);
    GEN_CHECK_OFF(PGM, apInterPaePTs);
    GEN_CHECK_OFF(PGM, apInterPaePDs);
    GEN_CHECK_OFF(PGM, pInterPaePDPTR);
    GEN_CHECK_OFF(PGM, pInterPaePDPTR64);
    GEN_CHECK_OFF(PGM, pInterPaePML4);
    GEN_CHECK_OFF(PGM, HCPhysInterPD);
    GEN_CHECK_OFF(PGM, HCPhysInterPaePDPTR);
    GEN_CHECK_OFF(PGM, HCPhysInterPaePML4);
    GEN_CHECK_OFF(PGM, pbDynPageMapBaseGC);
    GEN_CHECK_OFF(PGM, iDynPageMapLast);
    GEN_CHECK_OFF(PGM, aHCPhysDynPageMapCache);
    GEN_CHECK_OFF(PGM, GCPhysA20Mask);
    GEN_CHECK_OFF(PGM, fA20Enabled);
    GEN_CHECK_OFF(PGM, fSyncFlags);
    GEN_CHECK_OFF(PGM, CritSect);
#ifdef PGM_PD_CACHING_ENABLED
    GEN_CHECK_OFF(PGM, pdcache);
#endif
    GEN_CHECK_OFF(PGM, pgmphysreadcache);
    GEN_CHECK_OFF(PGM, pgmphyswritecache);
    GEN_CHECK_OFF(PGM, ChunkR3Map);
    GEN_CHECK_OFF(PGM, ChunkR3Map.pTree);
    GEN_CHECK_OFF(PGM, ChunkR3Map.Tlb);
    GEN_CHECK_OFF(PGM, ChunkR3Map.c);
    GEN_CHECK_OFF(PGM, ChunkR3Map.cMax);
    GEN_CHECK_OFF(PGM, ChunkR3Map.iNow);
    GEN_CHECK_OFF(PGM, ChunkR3Map.AgeingCountdown);
    GEN_CHECK_OFF(PGM, PhysTlbHC);
    GEN_CHECK_OFF(PGM, PhysTlbHC.aEntries[0]);
    GEN_CHECK_OFF(PGM, PhysTlbHC.aEntries[1]);
    GEN_CHECK_OFF(PGM, PhysTlbHC.aEntries[1].GCPhys);
    GEN_CHECK_OFF(PGM, PhysTlbHC.aEntries[1].pMap);
    GEN_CHECK_OFF(PGM, PhysTlbHC.aEntries[1].pPage);
    GEN_CHECK_OFF(PGM, PhysTlbHC.aEntries[1].pv);
    GEN_CHECK_OFF(PGM, HCPhysZeroPg);
    GEN_CHECK_OFF(PGM, pvZeroPgR3);
    GEN_CHECK_OFF(PGM, pvZeroPgR0);
    GEN_CHECK_OFF(PGM, pvZeroPgGC);
    GEN_CHECK_OFF(PGM, cHandyPages);
    GEN_CHECK_OFF(PGM, aHandyPages);
    GEN_CHECK_OFF(PGM, aHandyPages[1]);
    GEN_CHECK_OFF(PGM, aHandyPages[1].HCPhysGCPhys);
    GEN_CHECK_OFF(PGM, aHandyPages[1].idPage);
    GEN_CHECK_OFF(PGM, aHandyPages[1].idSharedPage);
    GEN_CHECK_OFF(PGM, cAllPages);
    GEN_CHECK_OFF(PGM, cPrivatePages);
    GEN_CHECK_OFF(PGM, cSharedPages);
    GEN_CHECK_OFF(PGM, cZeroPages);
    GEN_CHECK_OFF(PGM, cGuestModeChanges);

    GEN_CHECK_SIZE(PGMMAPPING);
    GEN_CHECK_OFF(PGMMAPPING, pNextR3);
    GEN_CHECK_OFF(PGMMAPPING, pNextGC);
    GEN_CHECK_OFF(PGMMAPPING, pNextR0);
    GEN_CHECK_OFF(PGMMAPPING, GCPtr);
    GEN_CHECK_OFF(PGMMAPPING, GCPtrLast);
    GEN_CHECK_OFF(PGMMAPPING, cb);
    GEN_CHECK_OFF(PGMMAPPING, pfnRelocate);
    GEN_CHECK_OFF(PGMMAPPING, pvUser);
    GEN_CHECK_OFF(PGMMAPPING, pszDesc);
    GEN_CHECK_OFF(PGMMAPPING, cPTs);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].HCPhysPT);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].pPTR3);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].pPTR0);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].pPTGC);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].HCPhysPaePT0);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].HCPhysPaePT1);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].paPaePTsR3);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].paPaePTsGC);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].paPaePTsR0);
    GEN_CHECK_SIZE(PGMPHYSHANDLER);
    GEN_CHECK_OFF(PGMPHYSHANDLER, Core);
    GEN_CHECK_SIZE(((PPGMPHYSHANDLER)0)->Core);
    GEN_CHECK_OFF(PGMPHYSHANDLER, enmType);
    GEN_CHECK_OFF(PGMPHYSHANDLER, cPages);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pfnHandlerR3);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pvUserR3);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pfnHandlerR0);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pvUserR0);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pfnHandlerGC);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pvUserGC);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pszDesc);
    GEN_CHECK_SIZE(PGMPHYS2VIRTHANDLER);
    GEN_CHECK_OFF(PGMPHYS2VIRTHANDLER, Core);
    GEN_CHECK_OFF(PGMPHYS2VIRTHANDLER, offVirtHandler);
    GEN_CHECK_SIZE(PGMVIRTHANDLER);
    GEN_CHECK_OFF(PGMVIRTHANDLER, Core);
    GEN_CHECK_OFF(PGMVIRTHANDLER, enmType);
    GEN_CHECK_OFF(PGMVIRTHANDLER, GCPtr);
    GEN_CHECK_OFF(PGMVIRTHANDLER, GCPtrLast);
    GEN_CHECK_OFF(PGMVIRTHANDLER, cb);
    GEN_CHECK_OFF(PGMVIRTHANDLER, pfnHandlerHC);
    GEN_CHECK_OFF(PGMVIRTHANDLER, pfnHandlerGC);
    GEN_CHECK_OFF(PGMVIRTHANDLER, pszDesc);
    GEN_CHECK_OFF(PGMVIRTHANDLER, cPages);
    GEN_CHECK_OFF(PGMVIRTHANDLER, aPhysToVirt);
    GEN_CHECK_SIZE(PGMPAGE);
    GEN_CHECK_OFF(PGMPAGE, HCPhys);
    GEN_CHECK_SIZE(PGMRAMRANGE);
    GEN_CHECK_OFF(PGMRAMRANGE, pNextR3);
    GEN_CHECK_OFF(PGMRAMRANGE, pNextR0);
    GEN_CHECK_OFF(PGMRAMRANGE, pNextGC);
    GEN_CHECK_OFF(PGMRAMRANGE, GCPhys);
    GEN_CHECK_OFF(PGMRAMRANGE, GCPhysLast);
    GEN_CHECK_OFF(PGMRAMRANGE, cb);
    GEN_CHECK_OFF(PGMRAMRANGE, fFlags);
    GEN_CHECK_OFF(PGMRAMRANGE, pvHC);
    GEN_CHECK_OFF(PGMRAMRANGE, pszDesc);
    GEN_CHECK_OFF(PGMRAMRANGE, aPages);
    GEN_CHECK_OFF(PGMRAMRANGE, aPages[1]);
    GEN_CHECK_SIZE(PGMROMPAGE);
    GEN_CHECK_OFF(PGMROMPAGE, Virgin);
    GEN_CHECK_OFF(PGMROMPAGE, Shadow);
    GEN_CHECK_OFF(PGMROMPAGE, enmProt);
    GEN_CHECK_SIZE(PGMROMRANGE);
    GEN_CHECK_OFF(PGMROMRANGE, pNextR3);
    GEN_CHECK_OFF(PGMROMRANGE, pNextR0);
    GEN_CHECK_OFF(PGMROMRANGE, pNextGC);
    GEN_CHECK_OFF(PGMROMRANGE, GCPhys);
    GEN_CHECK_OFF(PGMROMRANGE, GCPhysLast);
    GEN_CHECK_OFF(PGMROMRANGE, cb);
    GEN_CHECK_OFF(PGMROMRANGE, fFlags);
    GEN_CHECK_OFF(PGMROMRANGE, pvOriginal);
    GEN_CHECK_OFF(PGMROMRANGE, pszDesc);
    GEN_CHECK_OFF(PGMROMRANGE, aPages);
    GEN_CHECK_OFF(PGMROMRANGE, aPages[1]);
    GEN_CHECK_SIZE(PGMTREES);
    GEN_CHECK_OFF(PGMTREES, PhysHandlers);
    GEN_CHECK_OFF(PGMTREES, VirtHandlers);
    GEN_CHECK_OFF(PGMTREES, PhysToVirtHandlers);
    GEN_CHECK_OFF(PGMTREES, HyperVirtHandlers);
    GEN_CHECK_SIZE(PGMPOOLPAGE);
    GEN_CHECK_OFF(PGMPOOLPAGE, Core);
    GEN_CHECK_OFF(PGMPOOLPAGE, GCPhys);
    GEN_CHECK_OFF(PGMPOOLPAGE, pvPageHC);
    GEN_CHECK_OFF(PGMPOOLPAGE, enmKind);
    GEN_CHECK_OFF(PGMPOOLPAGE, bPadding);
    GEN_CHECK_OFF(PGMPOOLPAGE, idx);
    GEN_CHECK_OFF(PGMPOOLPAGE, iNext);
#ifdef PGMPOOL_WITH_USER_TRACKING
    GEN_CHECK_OFF(PGMPOOLPAGE, iUserHead);
    GEN_CHECK_OFF(PGMPOOLPAGE, cPresent);
    GEN_CHECK_OFF(PGMPOOLPAGE, iFirstPresent);
#endif
#ifdef PGMPOOL_WITH_MONITORING
    GEN_CHECK_OFF(PGMPOOLPAGE, cModifications);
    GEN_CHECK_OFF(PGMPOOLPAGE, iModifiedNext);
    GEN_CHECK_OFF(PGMPOOLPAGE, iModifiedPrev);
    GEN_CHECK_OFF(PGMPOOLPAGE, iMonitoredNext);
    GEN_CHECK_OFF(PGMPOOLPAGE, iMonitoredPrev);
#endif
#ifdef PGMPOOL_WITH_CACHE
    GEN_CHECK_OFF(PGMPOOLPAGE, iAgeNext);
    GEN_CHECK_OFF(PGMPOOLPAGE, iAgePrev);
#endif
    GEN_CHECK_OFF(PGMPOOLPAGE, fZeroed);
    GEN_CHECK_OFF(PGMPOOLPAGE, fSeenNonGlobal);
    GEN_CHECK_OFF(PGMPOOLPAGE, fMonitored);
    GEN_CHECK_OFF(PGMPOOLPAGE, fCached);
    GEN_CHECK_OFF(PGMPOOLPAGE, fReusedFlushPending);
    GEN_CHECK_OFF(PGMPOOLPAGE, fCR3Mix);
    GEN_CHECK_SIZE(PGMPOOL);
    GEN_CHECK_OFF(PGMPOOL, pVMHC);
    GEN_CHECK_OFF(PGMPOOL, pVMGC);
    GEN_CHECK_OFF(PGMPOOL, cMaxPages);
    GEN_CHECK_OFF(PGMPOOL, cCurPages);
    GEN_CHECK_OFF(PGMPOOL, iFreeHead);
    GEN_CHECK_OFF(PGMPOOL, u16Padding);
#ifdef PGMPOOL_WITH_USER_TRACKING
    GEN_CHECK_OFF(PGMPOOL, iUserFreeHead);
    GEN_CHECK_OFF(PGMPOOL, cMaxUsers);
    GEN_CHECK_OFF(PGMPOOL, cPresent);
    GEN_CHECK_OFF(PGMPOOL, paUsersHC);
    GEN_CHECK_OFF(PGMPOOL, paUsersGC);
#endif /* PGMPOOL_WITH_USER_TRACKING */
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    GEN_CHECK_OFF(PGMPOOL, iPhysExtFreeHead);
    GEN_CHECK_OFF(PGMPOOL, cMaxPhysExts);
    GEN_CHECK_OFF(PGMPOOL, paPhysExtsHC);
    GEN_CHECK_OFF(PGMPOOL, paPhysExtsGC);
#endif
#ifdef PGMPOOL_WITH_CACHE
    GEN_CHECK_OFF(PGMPOOL, aiHash);
    GEN_CHECK_OFF(PGMPOOL, iAgeHead);
    GEN_CHECK_OFF(PGMPOOL, iAgeTail);
    GEN_CHECK_OFF(PGMPOOL, fCacheEnabled);
#endif
#ifdef PGMPOOL_WITH_MONITORING
    GEN_CHECK_OFF(PGMPOOL, pfnAccessHandlerGC);
    GEN_CHECK_OFF(PGMPOOL, pfnAccessHandlerR0);
    GEN_CHECK_OFF(PGMPOOL, pfnAccessHandlerR3);
    GEN_CHECK_OFF(PGMPOOL, pszAccessHandler);
    GEN_CHECK_OFF(PGMPOOL, iModifiedHead);
    GEN_CHECK_OFF(PGMPOOL, cModifiedPages);
#endif
    GEN_CHECK_OFF(PGMPOOL, cUsedPages);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(PGMPOOL, cUsedPagesHigh);
    GEN_CHECK_OFF(PGMPOOL, StatAlloc);
    GEN_CHECK_OFF(PGMPOOL, StatClearAll);
#endif
    GEN_CHECK_OFF(PGMPOOL, HCPhysTree);
    GEN_CHECK_OFF(PGMPOOL, aPages);
    GEN_CHECK_OFF(PGMPOOL, aPages[1]);
    GEN_CHECK_OFF(PGMPOOL, aPages[PGMPOOL_IDX_FIRST - 1]);

    GEN_CHECK_SIZE(REM);
    GEN_CHECK_OFF(REM, pCtx);
    GEN_CHECK_OFF(REM, cCanExecuteRaw);
    GEN_CHECK_OFF(REM, aGCPtrInvalidatedPages);
    GEN_CHECK_OFF(REM, cHandlerNotifications);
    GEN_CHECK_OFF(REM, aHandlerNotifications);
    GEN_CHECK_OFF(REM, paHCVirtToGCPhys);
    GEN_CHECK_OFF(REM, cPhysRegistrations);
    GEN_CHECK_OFF(REM, aPhysReg);
    GEN_CHECK_OFF(REM, rc);
    GEN_CHECK_OFF(REM, StatsInQEMU);
    GEN_CHECK_OFF(REM, Env);

    GEN_CHECK_SIZE(SELM);
    GEN_CHECK_OFF(SELM, offVM);
    GEN_CHECK_OFF(SELM, aHyperSel[SELM_HYPER_SEL_CS]);
    GEN_CHECK_OFF(SELM, aHyperSel[SELM_HYPER_SEL_DS]);
    GEN_CHECK_OFF(SELM, aHyperSel[SELM_HYPER_SEL_CS64]);
    GEN_CHECK_OFF(SELM, aHyperSel[SELM_HYPER_SEL_TSS]);
    GEN_CHECK_OFF(SELM, aHyperSel[SELM_HYPER_SEL_TSS_TRAP08]);
    GEN_CHECK_OFF(SELM, paGdtHC);
    GEN_CHECK_OFF(SELM, paGdtGC);
    GEN_CHECK_OFF(SELM, GuestGdtr);
    GEN_CHECK_OFF(SELM, cbEffGuestGdtLimit);
    GEN_CHECK_OFF(SELM, HCPtrLdt);
    GEN_CHECK_OFF(SELM, GCPtrLdt);
    GEN_CHECK_OFF(SELM, GCPtrGuestLdt);
    GEN_CHECK_OFF(SELM, cbLdtLimit);
    GEN_CHECK_OFF(SELM, offLdtHyper);
    GEN_CHECK_OFF(SELM, Tss);
    GEN_CHECK_OFF(SELM, TssTrap08);
    GEN_CHECK_OFF(SELM, GCPtrTss);
    GEN_CHECK_OFF(SELM, GCPtrGuestTss);
    GEN_CHECK_OFF(SELM, cbGuestTss);
    GEN_CHECK_OFF(SELM, fGuestTss32Bit);
    GEN_CHECK_OFF(SELM, cbMonitoredGuestTss);
    GEN_CHECK_OFF(SELM, GCSelTss);
    GEN_CHECK_OFF(SELM, fGDTRangeRegistered);
    GEN_CHECK_OFF(SELM, StatUpdateFromCPUM);

    GEN_CHECK_SIZE(TM);
    GEN_CHECK_OFF(TM, offVM);
    GEN_CHECK_OFF(TM, pvGIPR3);
    //GEN_CHECK_OFF(TM, pvGIPR0);
    GEN_CHECK_OFF(TM, pvGIPGC);
    GEN_CHECK_OFF(TM, fTSCTicking);
    GEN_CHECK_OFF(TM, u64TSCOffset);
    GEN_CHECK_OFF(TM, u64TSC);
    GEN_CHECK_OFF(TM, cTSCTicksPerSecond);
    GEN_CHECK_OFF(TM, fVirtualTicking);
    GEN_CHECK_OFF(TM, fVirtualWarpDrive);
    GEN_CHECK_OFF(TM, fVirtualSyncTicking);
    GEN_CHECK_OFF(TM, fVirtualSyncCatchUp);
    GEN_CHECK_OFF(TM, u32VirtualWarpDrivePercentage);
    GEN_CHECK_OFF(TM, u64VirtualOffset);
    GEN_CHECK_OFF(TM, u64Virtual);
    GEN_CHECK_OFF(TM, u64VirtualRawPrev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.pu64Prev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.pfnBad);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.pfnRediscover);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.c1nsSteps);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.cBadPrev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.cExpired);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR3.cUpdateRaces);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.pu64Prev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.pfnBad);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.pfnRediscover);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.c1nsSteps);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.cBadPrev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.cExpired);
    GEN_CHECK_OFF(TM, VirtualGetRawDataR0.cUpdateRaces);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.pu64Prev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.pfnBad);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.pfnRediscover);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.c1nsSteps);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.cBadPrev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.cExpired);
    GEN_CHECK_OFF(TM, VirtualGetRawDataGC.cUpdateRaces);
    GEN_CHECK_OFF(TM, pfnVirtualGetRawR3);
    GEN_CHECK_OFF(TM, pfnVirtualGetRawR0);
    GEN_CHECK_OFF(TM, pfnVirtualGetRawGC);
    GEN_CHECK_OFF(TM, u64VirtualWarpDriveStart);
    GEN_CHECK_OFF(TM, u64VirtualSync);
    GEN_CHECK_OFF(TM, offVirtualSync);
    GEN_CHECK_OFF(TM, offVirtualSyncGivenUp);
    GEN_CHECK_OFF(TM, u64VirtualSyncCatchUpPrev);
    GEN_CHECK_OFF(TM, u32VirtualSyncCatchUpPercentage);
    GEN_CHECK_OFF(TM, u32VirtualSyncScheduleSlack);
    GEN_CHECK_OFF(TM, u64VirtualSyncCatchUpStopThreshold);
    GEN_CHECK_OFF(TM, u64VirtualSyncCatchUpGiveUpThreshold);
    GEN_CHECK_OFF(TM, aVirtualSyncCatchUpPeriods);
    GEN_CHECK_OFF(TM, aVirtualSyncCatchUpPeriods[0].u64Start);
    GEN_CHECK_OFF(TM, aVirtualSyncCatchUpPeriods[0].u32Percentage);
    GEN_CHECK_OFF(TM, aVirtualSyncCatchUpPeriods[1].u64Start);
    GEN_CHECK_OFF(TM, aVirtualSyncCatchUpPeriods[1].u32Percentage);
    GEN_CHECK_OFF(TM, pTimer);
    GEN_CHECK_OFF(TM, u32TimerMillies);
    GEN_CHECK_OFF(TM, pFree);
    GEN_CHECK_OFF(TM, pCreated);
    GEN_CHECK_OFF(TM, paTimerQueuesR3);
    GEN_CHECK_OFF(TM, paTimerQueuesR0);
    GEN_CHECK_OFF(TM, paTimerQueuesGC);
    GEN_CHECK_OFF(TM, StatDoQueues);
    GEN_CHECK_OFF(TM, StatTimerCallbackSetFF);
    GEN_CHECK_SIZE(TMTIMER);
    GEN_CHECK_OFF(TMTIMER, u64Expire);
    GEN_CHECK_OFF(TMTIMER, enmClock);
    GEN_CHECK_OFF(TMTIMER, enmType);
    GEN_CHECK_OFF(TMTIMER, u.Dev.pfnTimer);
    GEN_CHECK_OFF(TMTIMER, u.Dev.pDevIns);
    GEN_CHECK_OFF(TMTIMER, u.Drv.pfnTimer);
    GEN_CHECK_OFF(TMTIMER, u.Drv.pDrvIns);
    GEN_CHECK_OFF(TMTIMER, u.Internal.pfnTimer);
    GEN_CHECK_OFF(TMTIMER, u.Internal.pvUser);
    GEN_CHECK_OFF(TMTIMER, u.External.pfnTimer);
    GEN_CHECK_OFF(TMTIMER, u.External.pvUser);
    GEN_CHECK_OFF(TMTIMER, enmState);
    GEN_CHECK_OFF(TMTIMER, offScheduleNext);
    GEN_CHECK_OFF(TMTIMER, offNext);
    GEN_CHECK_OFF(TMTIMER, offPrev);
    GEN_CHECK_OFF(TMTIMER, pBigNext);
    GEN_CHECK_OFF(TMTIMER, pBigPrev);
    GEN_CHECK_OFF(TMTIMER, pszDesc);
    GEN_CHECK_OFF(TMTIMER, pVMR0);
    GEN_CHECK_OFF(TMTIMER, pVMR3);
    GEN_CHECK_OFF(TMTIMER, pVMGC);
    GEN_CHECK_SIZE(TMTIMERQUEUE);
    GEN_CHECK_OFF(TMTIMERQUEUE, offActive);
    GEN_CHECK_OFF(TMTIMERQUEUE, offSchedule);
    GEN_CHECK_OFF(TMTIMERQUEUE, enmClock);

    GEN_CHECK_SIZE(TRPM); // has .mac
    GEN_CHECK_SIZE(VM);  // has .mac
    GEN_CHECK_SIZE(VMM);
    GEN_CHECK_OFF(VMM, offVM);
    GEN_CHECK_OFF(VMM, cbCoreCode);
    GEN_CHECK_OFF(VMM, HCPhysCoreCode);
    GEN_CHECK_OFF(VMM, pvHCCoreCodeR3);
    GEN_CHECK_OFF(VMM, pvHCCoreCodeR0);
    GEN_CHECK_OFF(VMM, pvGCCoreCode);
    GEN_CHECK_OFF(VMM, enmSwitcher);
    GEN_CHECK_OFF(VMM, aoffSwitchers);
    GEN_CHECK_OFF(VMM, aoffSwitchers[1]);
    GEN_CHECK_OFF(VMM, pfnR0HostToGuest);
    GEN_CHECK_OFF(VMM, pfnGCGuestToHost);
    GEN_CHECK_OFF(VMM, pfnGCCallTrampoline);
    GEN_CHECK_OFF(VMM, pfnCPUMGCResumeGuest);
    GEN_CHECK_OFF(VMM, pfnCPUMGCResumeGuestV86);
    GEN_CHECK_OFF(VMM, iLastGCRc);
    GEN_CHECK_OFF(VMM, pbHCStack);
    GEN_CHECK_OFF(VMM, pbGCStack);
    GEN_CHECK_OFF(VMM, pbGCStackBottom);
    GEN_CHECK_OFF(VMM, pLoggerGC);
    GEN_CHECK_OFF(VMM, pLoggerHC);
    GEN_CHECK_OFF(VMM, cbLoggerGC);
    GEN_CHECK_OFF(VMM, CritSectVMLock);
    GEN_CHECK_OFF(VMM, pYieldTimer);
    GEN_CHECK_OFF(VMM, cYieldResumeMillies);
    GEN_CHECK_OFF(VMM, cYieldEveryMillies);
    GEN_CHECK_OFF(VMM, enmCallHostOperation);
    GEN_CHECK_OFF(VMM, rcCallHost);
    GEN_CHECK_OFF(VMM, u64CallHostArg);
    GEN_CHECK_OFF(VMM, CallHostR0JmpBuf);
    GEN_CHECK_OFF(VMM, CallHostR0JmpBuf.SpCheck);
    GEN_CHECK_OFF(VMM, CallHostR0JmpBuf.SpResume);
    GEN_CHECK_OFF(VMM, StatRunGC);
    GEN_CHECK_OFF(VMM, StatGCRetPGMLock);

    GEN_CHECK_SIZE(RTPINGPONG);
    GEN_CHECK_SIZE(RTCRITSECT);
    GEN_CHECK_OFF(RTCRITSECT, u32Magic);
    GEN_CHECK_OFF(RTCRITSECT, cLockers);
    GEN_CHECK_OFF(RTCRITSECT, NativeThreadOwner);
    GEN_CHECK_OFF(RTCRITSECT, cNestings);
    GEN_CHECK_OFF(RTCRITSECT, fFlags);
    GEN_CHECK_OFF(RTCRITSECT, EventSem);
    GEN_CHECK_OFF(RTCRITSECT, Strict.ThreadOwner);
    GEN_CHECK_OFF(RTCRITSECT, Strict.pszEnterFile);
    GEN_CHECK_OFF(RTCRITSECT, Strict.u32EnterLine);
    GEN_CHECK_OFF(RTCRITSECT, Strict.uEnterId);


    GEN_CHECK_SIZE(CSAM);
    GEN_CHECK_OFF(CSAM, offVM);
    GEN_CHECK_OFF(CSAM, pPageTree);
    GEN_CHECK_OFF(CSAM, aDangerousInstr);
    GEN_CHECK_OFF(CSAM, aDangerousInstr[1]);
    GEN_CHECK_OFF(CSAM, aDangerousInstr[CSAM_MAX_DANGR_INSTR - 1]);
    GEN_CHECK_OFF(CSAM, cDangerousInstr);
    GEN_CHECK_OFF(CSAM, iDangerousInstr);
    GEN_CHECK_OFF(CSAM, pPDBitmapGC);
    GEN_CHECK_OFF(CSAM, pPDHCBitmapGC);
    GEN_CHECK_OFF(CSAM, pPDBitmapHC);
    GEN_CHECK_OFF(CSAM, pPDGCBitmapHC);
    GEN_CHECK_OFF(CSAM, savedstate);
    GEN_CHECK_OFF(CSAM, savedstate.pSSM);
    GEN_CHECK_OFF(CSAM, savedstate.cPageRecords);
    GEN_CHECK_OFF(CSAM, savedstate.cPatchPageRecords);
    GEN_CHECK_OFF(CSAM, cDirtyPages);
    GEN_CHECK_OFF(CSAM, pvDirtyBasePage);
    GEN_CHECK_OFF(CSAM, pvDirtyBasePage[1]);
    GEN_CHECK_OFF(CSAM, pvDirtyBasePage[CSAM_MAX_DIRTY_PAGES - 1]);
    GEN_CHECK_OFF(CSAM, pvDirtyFaultPage);
    GEN_CHECK_OFF(CSAM, pvDirtyFaultPage[1]);
    GEN_CHECK_OFF(CSAM, pvDirtyFaultPage[CSAM_MAX_DIRTY_PAGES - 1]);
    GEN_CHECK_OFF(CSAM, pvCallInstruction);
    GEN_CHECK_OFF(CSAM, iCallInstruction);
    GEN_CHECK_OFF(CSAM, fScanningStarted);
    GEN_CHECK_OFF(CSAM, fGatesChecked);
    GEN_CHECK_OFF(CSAM, StatNrTraps);
    GEN_CHECK_OFF(CSAM, StatNrPages);

    GEN_CHECK_SIZE(PATM);
    GEN_CHECK_OFF(PATM, offVM);
    GEN_CHECK_OFF(PATM, pPatchMemGC);
    GEN_CHECK_OFF(PATM, pPatchMemHC);
    GEN_CHECK_OFF(PATM, cbPatchMem);
    GEN_CHECK_OFF(PATM, offPatchMem);
    GEN_CHECK_OFF(PATM, fOutOfMemory);
    GEN_CHECK_OFF(PATM, deltaReloc);
    GEN_CHECK_OFF(PATM, pGCStateGC);
    GEN_CHECK_OFF(PATM, pGCStateHC);
    GEN_CHECK_OFF(PATM, pGCStackGC);
    GEN_CHECK_OFF(PATM, pGCStackHC);
    GEN_CHECK_OFF(PATM, pCPUMCtxGC);
    GEN_CHECK_OFF(PATM, pStatsGC);
    GEN_CHECK_OFF(PATM, pStatsHC);
    GEN_CHECK_OFF(PATM, uCurrentPatchIdx);
    GEN_CHECK_OFF(PATM, ulCallDepth);
    GEN_CHECK_OFF(PATM, cPageRecords);
    GEN_CHECK_OFF(PATM, pPatchedInstrGCLowest);
    GEN_CHECK_OFF(PATM, pPatchedInstrGCHighest);
    GEN_CHECK_OFF(PATM, PatchLookupTreeHC);
    GEN_CHECK_OFF(PATM, PatchLookupTreeGC);
    GEN_CHECK_OFF(PATM, pfnHelperCallGC);
    GEN_CHECK_OFF(PATM, pfnHelperRetGC);
    GEN_CHECK_OFF(PATM, pfnHelperJumpGC);
    GEN_CHECK_OFF(PATM, pfnHelperIretGC);
    GEN_CHECK_OFF(PATM, pGlobalPatchRec);
    GEN_CHECK_OFF(PATM, pfnSysEnterGC);
    GEN_CHECK_OFF(PATM, pfnSysEnterPatchGC);
    GEN_CHECK_OFF(PATM, uSysEnterPatchIdx);
    GEN_CHECK_OFF(PATM, pvFaultMonitor);
    GEN_CHECK_OFF(PATM, mmio);
    GEN_CHECK_OFF(PATM, mmio.GCPhys);
    GEN_CHECK_OFF(PATM, mmio.pCachedData);
    GEN_CHECK_OFF(PATM, savedstate);
    GEN_CHECK_OFF(PATM, savedstate.pSSM);
    GEN_CHECK_OFF(PATM, savedstate.cPatches);
    GEN_CHECK_OFF(PATM, StatNrOpcodeRead);
    GEN_CHECK_OFF(PATM, StatU32FunctionMaxSlotsUsed);

    GEN_CHECK_SIZE(PATMGCSTATE);
    GEN_CHECK_OFF(PATMGCSTATE, uVMFlags);
    GEN_CHECK_OFF(PATMGCSTATE, uPendingAction);
    GEN_CHECK_OFF(PATMGCSTATE, uPatchCalls);
    GEN_CHECK_OFF(PATMGCSTATE, uScratch);
    GEN_CHECK_OFF(PATMGCSTATE, uIretEFlags);
    GEN_CHECK_OFF(PATMGCSTATE, uIretCS);
    GEN_CHECK_OFF(PATMGCSTATE, uIretEIP);
    GEN_CHECK_OFF(PATMGCSTATE, Psp);
    GEN_CHECK_OFF(PATMGCSTATE, fPIF);
    GEN_CHECK_OFF(PATMGCSTATE, GCPtrInhibitInterrupts);
    GEN_CHECK_OFF(PATMGCSTATE, Restore);
    GEN_CHECK_OFF(PATMGCSTATE, Restore.uEAX);
    GEN_CHECK_OFF(PATMGCSTATE, Restore.uECX);
    GEN_CHECK_OFF(PATMGCSTATE, Restore.uEDI);
    GEN_CHECK_OFF(PATMGCSTATE, Restore.eFlags);
    GEN_CHECK_OFF(PATMGCSTATE, Restore.uFlags);
    GEN_CHECK_SIZE(PATMTREES);
    GEN_CHECK_OFF(PATMTREES, PatchTree);
    GEN_CHECK_OFF(PATMTREES, PatchTreeByPatchAddr);
    GEN_CHECK_OFF(PATMTREES, PatchTreeByPage);
    GEN_CHECK_SIZE(PATMPATCHREC);
    GEN_CHECK_OFF(PATMPATCHREC, Core);
    GEN_CHECK_OFF(PATMPATCHREC, CoreOffset);
    GEN_CHECK_OFF(PATMPATCHREC, patch);
    GEN_CHECK_SIZE(PATCHINFO);
    GEN_CHECK_OFF(PATCHINFO, uState);
    GEN_CHECK_OFF(PATCHINFO, uOldState);
    GEN_CHECK_OFF(PATCHINFO, uOpMode);
    GEN_CHECK_OFF(PATCHINFO, pPrivInstrHC);
    GEN_CHECK_OFF(PATCHINFO, pPrivInstrGC);
    GEN_CHECK_OFF(PATCHINFO, aPrivInstr);
    GEN_CHECK_OFF(PATCHINFO, aPrivInstr[1]);
    GEN_CHECK_OFF(PATCHINFO, aPrivInstr[MAX_INSTR_SIZE - 1]);
    GEN_CHECK_OFF(PATCHINFO, cbPrivInstr);
    GEN_CHECK_OFF(PATCHINFO, opcode);
    GEN_CHECK_OFF(PATCHINFO, cbPatchJump);
    GEN_CHECK_OFF(PATCHINFO, pPatchJumpDestGC);
    GEN_CHECK_OFF(PATCHINFO, pPatchBlockOffset);
    GEN_CHECK_OFF(PATCHINFO, cbPatchBlockSize);
    GEN_CHECK_OFF(PATCHINFO, uCurPatchOffset);
    GEN_CHECK_OFF(PATCHINFO, flags);
    GEN_CHECK_OFF(PATCHINFO, pInstrGCLowest);
    GEN_CHECK_OFF(PATCHINFO, pInstrGCHighest);
    GEN_CHECK_OFF(PATCHINFO, FixupTree);
    GEN_CHECK_OFF(PATCHINFO, nrFixups);
    GEN_CHECK_OFF(PATCHINFO, JumpTree);
    GEN_CHECK_OFF(PATCHINFO, nrJumpRecs);
    GEN_CHECK_OFF(PATCHINFO, Patch2GuestAddrTree);
    GEN_CHECK_OFF(PATCHINFO, Guest2PatchAddrTree);
    GEN_CHECK_OFF(PATCHINFO, nrPatch2GuestRecs);
    GEN_CHECK_OFF(PATCHINFO, cacheRec);
    GEN_CHECK_OFF(PATCHINFO, cacheRec.pPatchLocStartHC);
    GEN_CHECK_OFF(PATCHINFO, cacheRec.pPatchLocEndHC);
    GEN_CHECK_OFF(PATCHINFO, cacheRec.pGuestLoc);
    GEN_CHECK_OFF(PATCHINFO, cacheRec.opsize);
    GEN_CHECK_OFF(PATCHINFO, pTempInfo);
    GEN_CHECK_OFF(PATCHINFO, cCodeWrites);
    GEN_CHECK_OFF(PATCHINFO, cTraps);
    GEN_CHECK_OFF(PATCHINFO, cInvalidWrites);
    GEN_CHECK_OFF(PATCHINFO, uPatchIdx);
    GEN_CHECK_OFF(PATCHINFO, bDirtyOpcode);
    GEN_CHECK_SIZE(PATMPATCHPAGE);
    GEN_CHECK_OFF(PATMPATCHPAGE, Core);
    GEN_CHECK_OFF(PATMPATCHPAGE, pLowestAddrGC);
    GEN_CHECK_OFF(PATMPATCHPAGE, pHighestAddrGC);
    GEN_CHECK_OFF(PATMPATCHPAGE, cCount);
    GEN_CHECK_OFF(PATMPATCHPAGE, cMaxPatches);
    GEN_CHECK_OFF(PATMPATCHPAGE, aPatch);

    return (0);
}

