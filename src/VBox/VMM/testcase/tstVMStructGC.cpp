/* $Id$ */
/** @file
 * tstVMMStructGC - Generate structure member and size checks from the GC perspective.
 *
 * This is built using the VBOXGC template but linked into a host
 * ring-3 executable, rather hacky.
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


/*
 * Sanity checks.
 */
#ifndef IN_RC
# error Incorrect template!
#endif
#if defined(IN_RING3) || defined(IN_RING0)
# error Incorrect template!
#endif

#include <VBox/types.h>
#include <iprt/assert.h>
AssertCompileSize(uint8_t,  1);
AssertCompileSize(uint16_t, 2);
AssertCompileSize(uint32_t, 4);
AssertCompileSize(uint64_t, 8);
AssertCompileSize(RTRCPTR,  4);
#ifdef VBOX_WITH_64_BITS_GUESTS
AssertCompileSize(RTGCPTR,  8);
#else
AssertCompileSize(RTGCPTR,  4);
#endif
AssertCompileSize(RTGCPHYS, 8);
AssertCompileSize(RTHCPHYS, 8);


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
#include "../PDMInternal.h"
#include <VBox/pdm.h>
#include "../CFGMInternal.h"
#include "../CPUMInternal.h"
#include "../MMInternal.h"
#include "../PGMInternal.h"
#include "../SELMInternal.h"
#include "../TRPMInternal.h"
#include "../TMInternal.h"
#include "../IOMInternal.h"
#include "../REMInternal.h"
#include "../HWACCMInternal.h"
#include "../PATM/PATMInternal.h"
#include "../VMMInternal.h"
#include "../DBGFInternal.h"
#include "../STAMInternal.h"
#include "../PATM/CSAMInternal.h"
#include "../EMInternal.h"
#include "../REMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/x86.h>
#include <iprt/assert.h>

/* we don't use iprt here because we're pretending to be in GC! */
#include <stdio.h>

#define GEN_CHECK_SIZE(s)   printf("    CHECK_SIZE(%s, %u);\n", #s, (unsigned)sizeof(s))
#define GEN_CHECK_OFF(s, m) printf("    CHECK_OFF(%s, %u, %s);\n", #s, (unsigned)RT_OFFSETOF(s, m), #m)

int main()
{
    GEN_CHECK_SIZE(CFGM);

    GEN_CHECK_SIZE(CPUM); // has .mac
    GEN_CHECK_SIZE(CPUMCPU); // has .mac
    GEN_CHECK_SIZE(CPUMHOSTCTX);
    GEN_CHECK_SIZE(CPUMCTX);
    GEN_CHECK_SIZE(CPUMCTXMSR);
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
    GEN_CHECK_OFF(EMCPU, pCtx);
    GEN_CHECK_OFF(EMCPU, enmState);
    GEN_CHECK_OFF(EMCPU, fForceRAW);
    GEN_CHECK_OFF(EMCPU, u.achPaddingFatalLongJump);
    GEN_CHECK_OFF(EMCPU, StatForcedActions);
    GEN_CHECK_OFF(EMCPU, StatTotalClis);
    GEN_CHECK_OFF(EMCPU, pStatsR3);
    GEN_CHECK_OFF(EMCPU, pStatsR0);
    GEN_CHECK_OFF(EMCPU, pStatsRC);
    GEN_CHECK_OFF(EMCPU, pCliStatTree);

    GEN_CHECK_SIZE(IOM);
    GEN_CHECK_OFF(IOM, pTreesRC);
    GEN_CHECK_OFF(IOM, pTreesR3);
    GEN_CHECK_OFF(IOM, pTreesR0);
    GEN_CHECK_OFF(IOM, pMMIORangeLastR3);
    GEN_CHECK_OFF(IOM, pMMIOStatsLastR3);
    GEN_CHECK_OFF(IOM, pMMIORangeLastR0);
    GEN_CHECK_OFF(IOM, pMMIOStatsLastR0);
    GEN_CHECK_OFF(IOM, pMMIORangeLastRC);
    GEN_CHECK_OFF(IOM, pMMIOStatsLastRC);
    GEN_CHECK_OFF(IOM, pRangeLastReadR0);
    GEN_CHECK_OFF(IOM, pRangeLastReadRC);

    GEN_CHECK_SIZE(IOMMMIORANGE);
    GEN_CHECK_OFF(IOMMMIORANGE, GCPhys);
    GEN_CHECK_OFF(IOMMMIORANGE, cb);
    GEN_CHECK_OFF(IOMMMIORANGE, pszDesc);
    GEN_CHECK_OFF(IOMMMIORANGE, pvUserR3);
    GEN_CHECK_OFF(IOMMMIORANGE, pDevInsR3);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnWriteCallbackR3);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnReadCallbackR3);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnFillCallbackR3);
    GEN_CHECK_OFF(IOMMMIORANGE, pvUserR0);
    GEN_CHECK_OFF(IOMMMIORANGE, pDevInsR0);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnWriteCallbackR0);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnReadCallbackR0);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnFillCallbackR0);
    GEN_CHECK_OFF(IOMMMIORANGE, pvUserRC);
    GEN_CHECK_OFF(IOMMMIORANGE, pDevInsRC);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnWriteCallbackRC);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnReadCallbackRC);
    GEN_CHECK_OFF(IOMMMIORANGE, pfnFillCallbackRC);

    GEN_CHECK_SIZE(IOMMMIOSTATS);
    GEN_CHECK_OFF(IOMMMIOSTATS, ReadR3);

    GEN_CHECK_SIZE(IOMIOPORTRANGER0);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, Port);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, cPorts);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, pvUser);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, pDevIns);
    GEN_CHECK_OFF(IOMIOPORTRANGER0, pszDesc);

    GEN_CHECK_SIZE(IOMIOPORTRANGERC);
    GEN_CHECK_OFF(IOMIOPORTRANGERC, Port);
    GEN_CHECK_OFF(IOMIOPORTRANGERC, cPorts);
    GEN_CHECK_OFF(IOMIOPORTRANGERC, pvUser);
    GEN_CHECK_OFF(IOMIOPORTRANGERC, pDevIns);
    GEN_CHECK_OFF(IOMIOPORTRANGERC, pszDesc);

    GEN_CHECK_SIZE(IOMIOPORTSTATS);
    GEN_CHECK_OFF(IOMIOPORTSTATS, InR3);

    GEN_CHECK_SIZE(IOMTREES);
    GEN_CHECK_OFF(IOMTREES, IOPortTreeR3);
    GEN_CHECK_OFF(IOMTREES, IOPortTreeR0);
    GEN_CHECK_OFF(IOMTREES, IOPortTreeRC);
    GEN_CHECK_OFF(IOMTREES, MMIOTree);
    GEN_CHECK_OFF(IOMTREES, IOPortStatTree);
    GEN_CHECK_OFF(IOMTREES, MMIOStatTree);

    GEN_CHECK_SIZE(MM);
    GEN_CHECK_OFF(MM, offVM);
    GEN_CHECK_OFF(MM, offHyperNextStatic);
    GEN_CHECK_OFF(MM, cbHyperArea);
    GEN_CHECK_OFF(MM, fDoneMMR3InitPaging);
    GEN_CHECK_OFF(MM, fPGMInitialized);
    GEN_CHECK_OFF(MM, offLookupHyper);
    GEN_CHECK_OFF(MM, pHyperHeapRC);
    GEN_CHECK_OFF(MM, pHyperHeapR3);
    GEN_CHECK_OFF(MM, pHyperHeapR0);
    GEN_CHECK_OFF(MM, pPagePoolR3);
    GEN_CHECK_OFF(MM, pPagePoolLowR3);
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    GEN_CHECK_OFF(MM, pPagePoolR0);
    GEN_CHECK_OFF(MM, pPagePoolLowR0);
#endif
    GEN_CHECK_OFF(MM, pvDummyPage);
    GEN_CHECK_OFF(MM, HCPhysDummyPage);
    GEN_CHECK_OFF(MM, cbRamBase);
    GEN_CHECK_OFF(MM, cBasePages);
    GEN_CHECK_OFF(MM, cHandyPages);
    GEN_CHECK_OFF(MM, cShadowPages);
    GEN_CHECK_OFF(MM, cFixedPages);
    GEN_CHECK_SIZE(MMHYPERSTAT);
    GEN_CHECK_SIZE(MMHYPERCHUNK);
    GEN_CHECK_SIZE(MMHYPERCHUNKFREE);
    GEN_CHECK_SIZE(MMHYPERHEAP);
    GEN_CHECK_OFF(MMHYPERHEAP, u32Magic);
    GEN_CHECK_OFF(MMHYPERHEAP, cbHeap);
    GEN_CHECK_OFF(MMHYPERHEAP, pbHeapR3);
    GEN_CHECK_OFF(MMHYPERHEAP, pVMR3);
    GEN_CHECK_OFF(MMHYPERHEAP, pbHeapR0);
    GEN_CHECK_OFF(MMHYPERHEAP, pVMR0);
    GEN_CHECK_OFF(MMHYPERHEAP, pbHeapRC);
    GEN_CHECK_OFF(MMHYPERHEAP, pVMRC);
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
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.Locked.pvR3);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.Locked.pvR0);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.Locked.paHCPhysPages);
    GEN_CHECK_OFF(MMLOOKUPHYPER, u.HCPhys.pvR3);
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
    GEN_CHECK_OFF(PDM, aPciBuses[0].pDevInsRC);
    GEN_CHECK_OFF(PDM, aPciBuses[0].pfnSetIrqRC);
    GEN_CHECK_OFF(PDM, Pic);
    GEN_CHECK_OFF(PDM, Pic.pDevInsR3);
    GEN_CHECK_OFF(PDM, Pic.pfnSetIrqR3);
    GEN_CHECK_OFF(PDM, Pic.pfnGetInterruptR3);
    GEN_CHECK_OFF(PDM, Pic.pDevInsR0);
    GEN_CHECK_OFF(PDM, Pic.pfnSetIrqR0);
    GEN_CHECK_OFF(PDM, Pic.pfnGetInterruptR0);
    GEN_CHECK_OFF(PDM, Pic.pDevInsRC);
    GEN_CHECK_OFF(PDM, Pic.pfnSetIrqRC);
    GEN_CHECK_OFF(PDM, Pic.pfnGetInterruptRC);
    GEN_CHECK_OFF(PDM, Apic);
    GEN_CHECK_OFF(PDM, Apic.pDevInsR3);
    GEN_CHECK_OFF(PDM, Apic.pfnGetInterruptR3);
    GEN_CHECK_OFF(PDM, Apic.pfnSetBaseR3);
    GEN_CHECK_OFF(PDM, Apic.pfnGetBaseR3);
    GEN_CHECK_OFF(PDM, Apic.pfnSetTPRR3);
    GEN_CHECK_OFF(PDM, Apic.pfnWriteMSRR3);
    GEN_CHECK_OFF(PDM, Apic.pfnReadMSRR3);
    GEN_CHECK_OFF(PDM, Apic.pfnGetTPRR3);
    GEN_CHECK_OFF(PDM, Apic.pfnBusDeliverR3);
    GEN_CHECK_OFF(PDM, Apic.pDevInsR0);
    GEN_CHECK_OFF(PDM, Apic.pfnGetInterruptR0);
    GEN_CHECK_OFF(PDM, Apic.pfnSetBaseR0);
    GEN_CHECK_OFF(PDM, Apic.pfnGetBaseR0);
    GEN_CHECK_OFF(PDM, Apic.pfnSetTPRR0);
    GEN_CHECK_OFF(PDM, Apic.pfnGetTPRR0);
    GEN_CHECK_OFF(PDM, Apic.pfnWriteMSRR0);
    GEN_CHECK_OFF(PDM, Apic.pfnReadMSRR0);
    GEN_CHECK_OFF(PDM, Apic.pfnBusDeliverR0);
    GEN_CHECK_OFF(PDM, Apic.pDevInsRC);
    GEN_CHECK_OFF(PDM, Apic.pfnGetInterruptRC);
    GEN_CHECK_OFF(PDM, Apic.pfnSetBaseRC);
    GEN_CHECK_OFF(PDM, Apic.pfnGetBaseRC);
    GEN_CHECK_OFF(PDM, Apic.pfnSetTPRRC);
    GEN_CHECK_OFF(PDM, Apic.pfnGetTPRRC);
    GEN_CHECK_OFF(PDM, Apic.pfnWriteMSRRC);
    GEN_CHECK_OFF(PDM, Apic.pfnReadMSRRC);
    GEN_CHECK_OFF(PDM, Apic.pfnBusDeliverRC);
    GEN_CHECK_OFF(PDM, IoApic);
    GEN_CHECK_OFF(PDM, IoApic.pDevInsR3);
    GEN_CHECK_OFF(PDM, IoApic.pfnSetIrqR3);
    GEN_CHECK_OFF(PDM, IoApic.pDevInsR0);
    GEN_CHECK_OFF(PDM, IoApic.pfnSetIrqR0);
    GEN_CHECK_OFF(PDM, IoApic.pDevInsRC);
    GEN_CHECK_OFF(PDM, IoApic.pfnSetIrqRC);
    GEN_CHECK_OFF(PDM, pDmac);
    GEN_CHECK_OFF(PDM, pRtc);
    GEN_CHECK_OFF(PDM, pUsbHubs);
    GEN_CHECK_OFF(PDM, pDevHlpQueueR3);
    GEN_CHECK_OFF(PDM, pDevHlpQueueR0);
    GEN_CHECK_OFF(PDM, pDevHlpQueueRC);
    GEN_CHECK_OFF(PDM, cQueuedCritSectLeaves);
    GEN_CHECK_OFF(PDM, apQueuedCritSectsLeaves);
    GEN_CHECK_OFF(PDM, pQueuesTimer);
    GEN_CHECK_OFF(PDM, pQueuesForced);
    GEN_CHECK_OFF(PDM, pQueueFlushR0);
    GEN_CHECK_OFF(PDM, pQueueFlushRC);
    GEN_CHECK_OFF(PDM, pThreads);
    GEN_CHECK_OFF(PDM, pThreadsTail);
    GEN_CHECK_OFF(PDM, CritSect);
    GEN_CHECK_OFF(PDM, StatQueuedCritSectLeaves);
    GEN_CHECK_SIZE(PDMDEVINSINT);
    GEN_CHECK_OFF(PDMDEVINSINT, pNextR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pPerDeviceNextR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pDevR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pVMR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pVMR0);
    GEN_CHECK_OFF(PDMDEVINSINT, pVMRC);
    GEN_CHECK_OFF(PDMDEVINSINT, pLunsR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pCfgHandle);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciDeviceR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciDeviceR0);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciDeviceRC);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciBusR3);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciBusR0);
    GEN_CHECK_OFF(PDMDEVINSINT, pPciBusRC);
    GEN_CHECK_SIZE(PDMCRITSECTINT);
    GEN_CHECK_OFF(PDMCRITSECTINT, Core);
    GEN_CHECK_OFF(PDMCRITSECTINT, pNext);
    GEN_CHECK_OFF(PDMCRITSECTINT, pvKey);
    GEN_CHECK_OFF(PDMCRITSECTINT, pVMR3);
    GEN_CHECK_OFF(PDMCRITSECTINT, pVMR0);
    GEN_CHECK_OFF(PDMCRITSECTINT, pVMRC);
    GEN_CHECK_OFF(PDMCRITSECTINT, StatContentionRZLock);
    GEN_CHECK_OFF(PDMCRITSECTINT, StatContentionRZUnlock);
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
    GEN_CHECK_OFF(PDMQUEUE, pVMR3);
    GEN_CHECK_OFF(PDMQUEUE, pVMR0);
    GEN_CHECK_OFF(PDMQUEUE, pVMRC);
    GEN_CHECK_OFF(PDMQUEUE, cMilliesInterval);
    GEN_CHECK_OFF(PDMQUEUE, pTimer);
    GEN_CHECK_OFF(PDMQUEUE, cbItem);
    GEN_CHECK_OFF(PDMQUEUE, cItems);
    GEN_CHECK_OFF(PDMQUEUE, pPendingR3);
    GEN_CHECK_OFF(PDMQUEUE, pPendingR0);
    GEN_CHECK_OFF(PDMQUEUE, pPendingRC);
    GEN_CHECK_OFF(PDMQUEUE, iFreeHead);
    GEN_CHECK_OFF(PDMQUEUE, iFreeTail);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[1]);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[0].pItemR3);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[0].pItemR0);
    GEN_CHECK_OFF(PDMQUEUE, aFreeItems[1].pItemRC);
    GEN_CHECK_SIZE(PDMDEVHLPTASK);
    GEN_CHECK_OFF(PDMDEVHLPTASK, Core);
    GEN_CHECK_OFF(PDMDEVHLPTASK, pDevInsR3);
    GEN_CHECK_OFF(PDMDEVHLPTASK, enmOp);
    GEN_CHECK_OFF(PDMDEVHLPTASK, u);
    GEN_CHECK_OFF(PDMDEVHLPTASK, u.SetIRQ.iIrq);
    GEN_CHECK_OFF(PDMDEVHLPTASK, u.SetIRQ.iLevel);

    GEN_CHECK_SIZE(PGM);
    GEN_CHECK_OFF(PGM, offVM);
    GEN_CHECK_OFF(PGM, fRamPreAlloc);
    GEN_CHECK_OFF(PGM, paDynPageMap32BitPTEsGC);
    GEN_CHECK_OFF(PGM, paDynPageMapPaePTEsGC);
    GEN_CHECK_OFF(PGM, enmHostMode);
    GEN_CHECK_OFF(PGMCPU, offVM);
    GEN_CHECK_OFF(PGMCPU, offVCpu);
    GEN_CHECK_OFF(PGMCPU, offPGM);
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    GEN_CHECK_OFF(PGMCPU, AutoSet);
#endif
    GEN_CHECK_OFF(PGMCPU, GCPhysA20Mask);
    GEN_CHECK_OFF(PGMCPU, fA20Enabled);
    GEN_CHECK_OFF(PGMCPU, fSyncFlags);
    GEN_CHECK_OFF(PGMCPU, enmShadowMode);
    GEN_CHECK_OFF(PGMCPU, enmGuestMode);
    GEN_CHECK_OFF(PGMCPU, GCPhysCR3);
    GEN_CHECK_OFF(PGM, GCPtrCR3Mapping);
    GEN_CHECK_OFF(PGMCPU, pGst32BitPdR3);
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    GEN_CHECK_OFF(PGMCPU, pGst32BitPdR0);
#endif
    GEN_CHECK_OFF(PGMCPU, pGst32BitPdRC);
    GEN_CHECK_OFF(PGMCPU, pGstPaePdptR3);
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    GEN_CHECK_OFF(PGMCPU, pGstPaePdptR0);
#endif
    GEN_CHECK_OFF(PGMCPU, pGstPaePdptRC);
    GEN_CHECK_OFF(PGMCPU, apGstPaePDsR3);
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    GEN_CHECK_OFF(PGMCPU, apGstPaePDsR0);
#endif
    GEN_CHECK_OFF(PGMCPU, apGstPaePDsRC);
    GEN_CHECK_OFF(PGMCPU, aGCPhysGstPaePDs);
    GEN_CHECK_OFF(PGMCPU, aGCPhysGstPaePDsMonitored);
    GEN_CHECK_OFF(PGMCPU, pShwPageCR3R3);
    GEN_CHECK_OFF(PGMCPU, pShwPageCR3R0);
    GEN_CHECK_OFF(PGMCPU, pShwPageCR3RC);
    GEN_CHECK_OFF(PGMCPU, pfnR3ShwRelocate);
    GEN_CHECK_OFF(PGMCPU, pfnR3ShwExit);
    GEN_CHECK_OFF(PGMCPU, pfnR3ShwGetPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3ShwModifyPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCShwGetPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCShwModifyPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3GstRelocate);
    GEN_CHECK_OFF(PGMCPU, pfnR3GstExit);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthMapCR3);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthUnmapCR3);
    GEN_CHECK_OFF(PGMCPU, pfnR3GstGetPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3GstModifyPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3GstGetPDE);
    GEN_CHECK_OFF(PGMCPU, pfnRCGstGetPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCGstModifyPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCGstGetPDE);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthRelocate);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthSyncCR3);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthInvalidatePage);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthSyncPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthPrefetchPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthVerifyAccessSyncPage);
    GEN_CHECK_OFF(PGMCPU, pfnR3BthAssertCR3);
    GEN_CHECK_OFF(PGMCPU, pfnRCBthTrap0eHandler);
    GEN_CHECK_OFF(PGMCPU, pfnRCBthInvalidatePage);
    GEN_CHECK_OFF(PGMCPU, pfnRCBthSyncPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCBthPrefetchPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCBthVerifyAccessSyncPage);
    GEN_CHECK_OFF(PGMCPU, pfnRCBthAssertCR3);
    GEN_CHECK_OFF(PGM, offVM);
    GEN_CHECK_OFF(PGM, offVCpuPGM);
    GEN_CHECK_OFF(PGM, fRamPreAlloc);
    GEN_CHECK_OFF(PGM, fGlobalSyncFlags);
    GEN_CHECK_OFF(PGM, paDynPageMap32BitPTEsGC);
    GEN_CHECK_OFF(PGM, paDynPageMapPaePTEsGC);
    GEN_CHECK_OFF(PGM, enmHostMode);
    GEN_CHECK_OFF(PGM, GCPhys4MBPSEMask);
    GEN_CHECK_OFF(PGM, pRamRangesR3);
    GEN_CHECK_OFF(PGM, pRamRangesR0);
    GEN_CHECK_OFF(PGM, pRamRangesRC);
    GEN_CHECK_OFF(PGM, pRomRangesR3);
    GEN_CHECK_OFF(PGM, pRomRangesR0);
    GEN_CHECK_OFF(PGM, pRomRangesRC);
    GEN_CHECK_OFF(PGM, pTreesR3);
    GEN_CHECK_OFF(PGM, pTreesR0);
    GEN_CHECK_OFF(PGM, pTreesRC);
    GEN_CHECK_OFF(PGM, pMappingsR3);
    GEN_CHECK_OFF(PGM, pMappingsRC);
    GEN_CHECK_OFF(PGM, pMappingsR0);
    GEN_CHECK_OFF(PGM, fFinalizedMappings);
    GEN_CHECK_OFF(PGM, fMappingsFixed);
    GEN_CHECK_OFF(PGM, GCPtrMappingFixed);
    GEN_CHECK_OFF(PGM, cbMappingFixed);
    GEN_CHECK_OFF(PGM, pInterPD);
    GEN_CHECK_OFF(PGM, apInterPTs);
    GEN_CHECK_OFF(PGM, apInterPaePTs);
    GEN_CHECK_OFF(PGM, apInterPaePDs);
    GEN_CHECK_OFF(PGM, pInterPaePDPT);
    GEN_CHECK_OFF(PGM, pInterPaePDPT64);
    GEN_CHECK_OFF(PGM, pInterPaePML4);
    GEN_CHECK_OFF(PGM, HCPhysInterPD);
    GEN_CHECK_OFF(PGM, HCPhysInterPaePDPT);
    GEN_CHECK_OFF(PGM, HCPhysInterPaePML4);
    GEN_CHECK_OFF(PGM, pbDynPageMapBaseGC);
    GEN_CHECK_OFF(PGM, iDynPageMapLast);
    GEN_CHECK_OFF(PGM, aHCPhysDynPageMapCache);
    GEN_CHECK_OFF(PGM, pvR0DynMapUsed);
    GEN_CHECK_OFF(PGM, GCPhys4MBPSEMask);
    GEN_CHECK_OFF(PGMCPU, GCPhysA20Mask);
    GEN_CHECK_OFF(PGMCPU, fA20Enabled);
    GEN_CHECK_OFF(PGMCPU, fSyncFlags);
    GEN_CHECK_OFF(PGM, aHCPhysDynPageMapCache);
    GEN_CHECK_OFF(PGM, aLockedDynPageMapCache);
    GEN_CHECK_OFF(PGM, CritSect);
    GEN_CHECK_OFF(PGM, pPoolR3);
    GEN_CHECK_OFF(PGM, pPoolR0);
    GEN_CHECK_OFF(PGM, pPoolRC);
    GEN_CHECK_OFF(PGM, fNoMorePhysWrites);
    GEN_CHECK_OFF(PGM, fPhysCacheFlushPending);
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
    GEN_CHECK_OFF(PGM, pvZeroPgRC);
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
    GEN_CHECK_OFF(PGMCPU, cGuestModeChanges);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(PGMCPU, pStatTrap0eAttributionR0);
    GEN_CHECK_OFF(PGMCPU, pStatTrap0eAttributionRC);
#endif

    GEN_CHECK_SIZE(PGMMAPPING);
    GEN_CHECK_OFF(PGMMAPPING, pNextR3);
    GEN_CHECK_OFF(PGMMAPPING, pNextRC);
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
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].pPTRC);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].HCPhysPaePT0);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].HCPhysPaePT1);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].paPaePTsR3);
    GEN_CHECK_OFF(PGMMAPPING, aPTs[1].paPaePTsRC);
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
    GEN_CHECK_OFF(PGMPHYSHANDLER, pfnHandlerRC);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pvUserRC);
    GEN_CHECK_OFF(PGMPHYSHANDLER, pszDesc);
    GEN_CHECK_SIZE(PGMPHYS2VIRTHANDLER);
    GEN_CHECK_OFF(PGMPHYS2VIRTHANDLER, Core);
    GEN_CHECK_OFF(PGMPHYS2VIRTHANDLER, offVirtHandler);
    GEN_CHECK_SIZE(PGMVIRTHANDLER);
    GEN_CHECK_OFF(PGMVIRTHANDLER, Core);
    GEN_CHECK_OFF(PGMVIRTHANDLER, enmType);
    GEN_CHECK_OFF(PGMVIRTHANDLER, cb);
    GEN_CHECK_OFF(PGMVIRTHANDLER, pfnHandlerR3);
    GEN_CHECK_OFF(PGMVIRTHANDLER, pfnHandlerRC);
    GEN_CHECK_OFF(PGMVIRTHANDLER, pszDesc);
    GEN_CHECK_OFF(PGMVIRTHANDLER, cPages);
    GEN_CHECK_OFF(PGMVIRTHANDLER, aPhysToVirt);
    GEN_CHECK_SIZE(PGMPAGE);
    GEN_CHECK_OFF(PGMPAGE, HCPhysX);
    GEN_CHECK_SIZE(PGMRAMRANGE);
    GEN_CHECK_OFF(PGMRAMRANGE, pNextR3);
    GEN_CHECK_OFF(PGMRAMRANGE, pNextR0);
    GEN_CHECK_OFF(PGMRAMRANGE, pNextRC);
    GEN_CHECK_OFF(PGMRAMRANGE, GCPhys);
    GEN_CHECK_OFF(PGMRAMRANGE, GCPhysLast);
    GEN_CHECK_OFF(PGMRAMRANGE, cb);
    GEN_CHECK_OFF(PGMRAMRANGE, fFlags);
    GEN_CHECK_OFF(PGMRAMRANGE, pvR3);
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
    GEN_CHECK_OFF(PGMROMRANGE, pNextRC);
    GEN_CHECK_OFF(PGMROMRANGE, GCPhys);
    GEN_CHECK_OFF(PGMROMRANGE, GCPhysLast);
    GEN_CHECK_OFF(PGMROMRANGE, cb);
    GEN_CHECK_OFF(PGMROMRANGE, fFlags);
    GEN_CHECK_OFF(PGMROMRANGE, pvOriginal);
    GEN_CHECK_OFF(PGMROMRANGE, pszDesc);
    GEN_CHECK_OFF(PGMROMRANGE, aPages);
    GEN_CHECK_OFF(PGMROMRANGE, aPages[1]);
    GEN_CHECK_SIZE(PGMMMIO2RANGE);
    GEN_CHECK_OFF(PGMMMIO2RANGE, pDevInsR3);
    GEN_CHECK_OFF(PGMMMIO2RANGE, pNextR3);
    GEN_CHECK_OFF(PGMMMIO2RANGE, fMapped);
    GEN_CHECK_OFF(PGMMMIO2RANGE, fOverlapping);
    GEN_CHECK_OFF(PGMMMIO2RANGE, iRegion);
    GEN_CHECK_OFF(PGMMMIO2RANGE, RamRange);
    GEN_CHECK_SIZE(PGMTREES);
    GEN_CHECK_OFF(PGMTREES, PhysHandlers);
    GEN_CHECK_OFF(PGMTREES, VirtHandlers);
    GEN_CHECK_OFF(PGMTREES, PhysToVirtHandlers);
    GEN_CHECK_OFF(PGMTREES, HyperVirtHandlers);
    GEN_CHECK_SIZE(PGMPOOLPAGE);
    GEN_CHECK_OFF(PGMPOOLPAGE, Core);
    GEN_CHECK_OFF(PGMPOOLPAGE, GCPhys);
    GEN_CHECK_OFF(PGMPOOLPAGE, pvPageR3);
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
    GEN_CHECK_SIZE(PGMPOOL);
    GEN_CHECK_OFF(PGMPOOL, pVMR3);
    GEN_CHECK_OFF(PGMPOOL, pVMR0);
    GEN_CHECK_OFF(PGMPOOL, pVMRC);
    GEN_CHECK_OFF(PGMPOOL, cMaxPages);
    GEN_CHECK_OFF(PGMPOOL, cCurPages);
    GEN_CHECK_OFF(PGMPOOL, iFreeHead);
    GEN_CHECK_OFF(PGMPOOL, u16Padding);
#ifdef PGMPOOL_WITH_USER_TRACKING
    GEN_CHECK_OFF(PGMPOOL, iUserFreeHead);
    GEN_CHECK_OFF(PGMPOOL, cMaxUsers);
    GEN_CHECK_OFF(PGMPOOL, cPresent);
    GEN_CHECK_OFF(PGMPOOL, paUsersR3);
    GEN_CHECK_OFF(PGMPOOL, paUsersR0);
    GEN_CHECK_OFF(PGMPOOL, paUsersRC);
#endif /* PGMPOOL_WITH_USER_TRACKING */
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    GEN_CHECK_OFF(PGMPOOL, iPhysExtFreeHead);
    GEN_CHECK_OFF(PGMPOOL, cMaxPhysExts);
    GEN_CHECK_OFF(PGMPOOL, paPhysExtsR3);
    GEN_CHECK_OFF(PGMPOOL, paPhysExtsR0);
    GEN_CHECK_OFF(PGMPOOL, paPhysExtsRC);
#endif
#ifdef PGMPOOL_WITH_CACHE
    GEN_CHECK_OFF(PGMPOOL, aiHash);
    GEN_CHECK_OFF(PGMPOOL, iAgeHead);
    GEN_CHECK_OFF(PGMPOOL, iAgeTail);
    GEN_CHECK_OFF(PGMPOOL, fCacheEnabled);
#endif
#ifdef PGMPOOL_WITH_MONITORING
    GEN_CHECK_OFF(PGMPOOL, pfnAccessHandlerRC);
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
    GEN_CHECK_OFF(SELM, paGdtR3);
    GEN_CHECK_OFF(SELM, paGdtRC);
    GEN_CHECK_OFF(SELM, GuestGdtr);
    GEN_CHECK_OFF(SELM, cbEffGuestGdtLimit);
    GEN_CHECK_OFF(SELM, pvLdtR3);
    GEN_CHECK_OFF(SELM, pvLdtRC);
    GEN_CHECK_OFF(SELM, GCPtrGuestLdt);
    GEN_CHECK_OFF(SELM, cbLdtLimit);
    GEN_CHECK_OFF(SELM, offLdtHyper);
    GEN_CHECK_OFF(SELM, Tss);
    GEN_CHECK_OFF(SELM, TssTrap08);
    GEN_CHECK_OFF(SELM, pvMonShwTssRC);
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
    GEN_CHECK_OFF(TM, pvGIPRC);
    GEN_CHECK_OFF(TM, fTSCTicking);
    GEN_CHECK_OFF(TM, fTSCUseRealTSC);
    GEN_CHECK_OFF(TM, fTSCTiedToExecution);
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
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.pu64Prev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.pfnBad);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.pfnRediscover);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.c1nsSteps);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.cBadPrev);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.cExpired);
    GEN_CHECK_OFF(TM, VirtualGetRawDataRC.cUpdateRaces);
    GEN_CHECK_OFF(TM, pfnVirtualGetRawR3);
    GEN_CHECK_OFF(TM, pfnVirtualGetRawR0);
    GEN_CHECK_OFF(TM, pfnVirtualGetRawRC);
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
    GEN_CHECK_OFF(TM, paTimerQueuesRC);
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
    GEN_CHECK_OFF(TMTIMER, pVMRC);
    GEN_CHECK_SIZE(TMTIMERQUEUE);
    GEN_CHECK_OFF(TMTIMERQUEUE, offActive);
    GEN_CHECK_OFF(TMTIMERQUEUE, offSchedule);
    GEN_CHECK_OFF(TMTIMERQUEUE, enmClock);

    GEN_CHECK_SIZE(TRPM); // has .mac
    GEN_CHECK_SIZE(TRPMCPU); // has .mac
    GEN_CHECK_SIZE(VM);  // has .mac
    GEN_CHECK_SIZE(VMM);
    GEN_CHECK_OFF(VMM, offVM);
    GEN_CHECK_OFF(VMM, cbCoreCode);
    GEN_CHECK_OFF(VMM, HCPhysCoreCode);
    GEN_CHECK_OFF(VMM, pvCoreCodeR3);
    GEN_CHECK_OFF(VMM, pvCoreCodeR0);
    GEN_CHECK_OFF(VMM, pvCoreCodeRC);
    GEN_CHECK_OFF(VMM, enmSwitcher);
    GEN_CHECK_OFF(VMM, aoffSwitchers);
    GEN_CHECK_OFF(VMM, aoffSwitchers[1]);
    GEN_CHECK_OFF(VMM, pfnHostToGuestR0);
    GEN_CHECK_OFF(VMM, pfnGuestToHostRC);
    GEN_CHECK_OFF(VMM, pfnCallTrampolineRC);
    GEN_CHECK_OFF(VMM, pfnCPUMRCResumeGuest);
    GEN_CHECK_OFF(VMM, pfnCPUMRCResumeGuestV86);
    GEN_CHECK_OFF(VMM, iLastGZRc);
    GEN_CHECK_OFF(VMM, pbEMTStackR3);
    GEN_CHECK_OFF(VMM, pbEMTStackRC);
    GEN_CHECK_OFF(VMM, pbEMTStackBottomRC);
    GEN_CHECK_OFF(VMM, pRCLoggerRC);
    GEN_CHECK_OFF(VMM, pRCLoggerR3);
    GEN_CHECK_OFF(VMM, pR0LoggerR0);
    GEN_CHECK_OFF(VMM, pR0LoggerR3);
    GEN_CHECK_OFF(VMM, cbRCLogger);
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
    GEN_CHECK_OFF(VMM, StatRunRC);
    GEN_CHECK_OFF(VMM, StatRZCallPGMLock);

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

