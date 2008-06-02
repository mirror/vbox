/* $Id$ */
/** @file
 * IOM - Input / Output Monitor.
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


/** @page pg_iom        IOM - The Input/Output Monitor
 *
 * The input/output monitor will handle I/O exceptions routing them to the
 * appropriate device. It implements an API to register and deregister
 * virtual port I/O handler and memory mapped I/O handlers. A handler is
 * PDM devices and a set of callback functions.
 *
 * Port I/O (PIO) is easily trapped by ensuring IOPL is 0, thus causing \#GP(0) on
 * any access to I/O ports. Using the dissassembler (DIS) the faulting
 * instruction will be interpreted determing the port and if there is a handler
 * for it. If a handler exists it will be called, else default action will be
 * performed.
 *
 * Memory Mapped I/O (MMIO) is gonna be worse since there are numerous instructions
 * which can access memory. I'm afraid we might have to emulate each
 * instruction which faults. The Execution Monitor (EM) will provide facilities
 * for doing this using DIS.
 *
 * Emulating I/O port access is less complex and sligtly faster than emulating MMIO,
 * so in most cases we should encourage the OS to use PIO. Devices which are freqently
 * accessed should register GC handlers to speed up execution.
 *
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_IOM
#include <VBox/iom.h>
#include <VBox/cpum.h>
#include <VBox/pgm.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/stam.h>
#include <VBox/dbgf.h>
#include <VBox/pdm.h>
#include "IOMInternal.h"
#include <VBox/vm.h>

#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/err.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void iomR3FlushCache(PVM pVM);
static DECLCALLBACK(int) iomr3RelocateIOPortCallback(PAVLROIOPORTNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) iomr3RelocateMMIOCallback(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(void) iomR3IOPortInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) iomR3MMIOInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(int) iomR3IOPortDummyIn(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
static DECLCALLBACK(int) iomR3IOPortDummyOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
static DECLCALLBACK(int) iomR3IOPortDummyInStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfer, unsigned cb);
static DECLCALLBACK(int) iomR3IOPortDummyOutStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb);

#ifdef VBOX_WITH_STATISTICS
static const char *iomr3IOPortGetStandardName(RTIOPORT Port);
#endif


/**
 * Initializes the IOM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
IOMR3DECL(int) IOMR3Init(PVM pVM)
{
    LogFlow(("IOMR3Init:\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, iom.s) & 31));
    AssertRelease(sizeof(pVM->iom.s) <= sizeof(pVM->iom.padding));

    /*
     * Setup any fixed pointers and offsets.
     */
    pVM->iom.s.offVM = RT_OFFSETOF(VM, iom);

    /*
     * Allocate the trees structure.
     */
    int rc = MMHyperAlloc(pVM, sizeof(*pVM->iom.s.pTreesHC), 0, MM_TAG_IOM, (void **)&pVM->iom.s.pTreesHC);
    if (VBOX_SUCCESS(rc))
    {
        pVM->iom.s.pTreesGC = MMHyperHC2GC(pVM, pVM->iom.s.pTreesHC);
        pVM->iom.s.pfnMMIOHandlerGC = NIL_RTGCPTR;
        pVM->iom.s.pfnMMIOHandlerR0 = NIL_RTR0PTR;

        /*
         * Info.
         */
        DBGFR3InfoRegisterInternal(pVM, "ioport", "Dumps all IOPort ranges. No arguments.", &iomR3IOPortInfo);
        DBGFR3InfoRegisterInternal(pVM, "mmio", "Dumps all MMIO ranges. No arguments.", &iomR3MMIOInfo);

        /*
         * Statistics.
         */
        STAM_REG(pVM, &pVM->iom.s.StatGCMMIOHandler,      STAMTYPE_PROFILE, "/IOM/GC/MMIOHandler",         STAMUNIT_TICKS_PER_CALL, "Profiling of the IOMGCMMIOHandler() body, only success calls.");
        STAM_REG(pVM, &pVM->iom.s.StatGCMMIOFailures,     STAMTYPE_COUNTER, "/IOM/GC/MMIOFailures",        STAMUNIT_OCCURENCES,     "Number of times IOMGCMMIOHandler() didn't service the request.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstMov,          STAMTYPE_PROFILE, "/IOM/GC/Inst/MOV",            STAMUNIT_TICKS_PER_CALL, "Profiling of the MOV instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstCmp,          STAMTYPE_PROFILE, "/IOM/GC/Inst/CMP",            STAMUNIT_TICKS_PER_CALL, "Profiling of the CMP instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstAnd,          STAMTYPE_PROFILE, "/IOM/GC/Inst/AND",            STAMUNIT_TICKS_PER_CALL, "Profiling of the AND instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstTest,         STAMTYPE_PROFILE, "/IOM/GC/Inst/TEST",           STAMUNIT_TICKS_PER_CALL, "Profiling of the TEST instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstXchg,         STAMTYPE_PROFILE, "/IOM/GC/Inst/XCHG",           STAMUNIT_TICKS_PER_CALL, "Profiling of the XCHG instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstStos,         STAMTYPE_PROFILE, "/IOM/GC/Inst/STOS",           STAMUNIT_TICKS_PER_CALL, "Profiling of the STOS instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstLods,         STAMTYPE_PROFILE, "/IOM/GC/Inst/LODS",           STAMUNIT_TICKS_PER_CALL, "Profiling of the LODS instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstMovs,         STAMTYPE_PROFILE, "/IOM/GC/Inst/MOVS",           STAMUNIT_TICKS_PER_CALL, "Profiling of the MOVS instruction emulation.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstMovsToMMIO,   STAMTYPE_PROFILE, "/IOM/GC/Inst/MOVS/ToMMIO",    STAMUNIT_TICKS_PER_CALL, "Profiling of the MOVS instruction emulation - Mem2MMIO.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstMovsFromMMIO, STAMTYPE_PROFILE, "/IOM/GC/Inst/MOVS/FromMMIO",  STAMUNIT_TICKS_PER_CALL, "Profiling of the MOVS instruction emulation - MMIO2Mem.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstMovsMMIO,     STAMTYPE_PROFILE, "/IOM/GC/Inst/MOVS/MMIO2MMIO", STAMUNIT_TICKS_PER_CALL, "Profiling of the MOVS instruction emulation - MMIO2MMIO.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstOther,        STAMTYPE_COUNTER, "/IOM/GC/Inst/Other",          STAMUNIT_OCCURENCES,     "Other instructions counter.");
        STAM_REG(pVM, &pVM->iom.s.StatGCMMIO1Byte,        STAMTYPE_COUNTER, "/IOM/GC/MMIO/Access1",        STAMUNIT_OCCURENCES,     "MMIO access by 1 byte counter.");
        STAM_REG(pVM, &pVM->iom.s.StatGCMMIO2Bytes,       STAMTYPE_COUNTER, "/IOM/GC/MMIO/Access2",        STAMUNIT_OCCURENCES,     "MMIO access by 2 bytes counter.");
        STAM_REG(pVM, &pVM->iom.s.StatGCMMIO4Bytes,       STAMTYPE_COUNTER, "/IOM/GC/MMIO/Access4",        STAMUNIT_OCCURENCES,     "MMIO access by 4 bytes counter.");
        STAM_REG(pVM, &pVM->iom.s.StatGCIOPortHandler,    STAMTYPE_PROFILE, "/IOM/GC/PortIOHandler",       STAMUNIT_TICKS_PER_CALL, "Profiling of the IOMGCPortIOHandler() body, only success calls.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstIn,           STAMTYPE_COUNTER, "/IOM/GC/Inst/In",             STAMUNIT_OCCURENCES,     "Counter of any IN instructions.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstOut,          STAMTYPE_COUNTER, "/IOM/GC/Inst/Out",            STAMUNIT_OCCURENCES,     "Counter of any OUT instructions.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstIns,          STAMTYPE_COUNTER, "/IOM/GC/Inst/Ins",            STAMUNIT_OCCURENCES,     "Counter of any INS instructions.");
        STAM_REG(pVM, &pVM->iom.s.StatGCInstOuts,         STAMTYPE_COUNTER, "/IOM/GC/Inst/Outs",           STAMUNIT_OCCURENCES,     "Counter of any OUTS instructions.");
    }

    /* Redundant, but just in case we change something in the future */
    iomR3FlushCache(pVM);

    LogFlow(("IOMR3Init: returns %Vrc\n", rc));
    return rc;
}


/**
 * Flushes the IOM port & statistics lookup cache
 *
 * @param   pVM     The VM.
 */
static void iomR3FlushCache(PVM pVM)
{
    /*
     * Caching of port and statistics (saves some time in rep outs/ins instruction emulation)
     */
    pVM->iom.s.pRangeLastReadR0  = NIL_RTR0PTR;
    pVM->iom.s.pRangeLastWriteR0 = NIL_RTR0PTR;
    pVM->iom.s.pStatsLastReadR0  = NIL_RTR0PTR;
    pVM->iom.s.pStatsLastWriteR0 = NIL_RTR0PTR;
    pVM->iom.s.pMMIORangeLastR0  = NIL_RTR0PTR;
    pVM->iom.s.pMMIOStatsLastR0  = NIL_RTR0PTR;

    pVM->iom.s.pRangeLastReadR3  = NULL;
    pVM->iom.s.pRangeLastWriteR3 = NULL;
    pVM->iom.s.pStatsLastReadR3  = NULL;
    pVM->iom.s.pStatsLastWriteR3 = NULL;
    pVM->iom.s.pMMIORangeLastR3  = NULL;
    pVM->iom.s.pMMIOStatsLastR3  = NULL;

    pVM->iom.s.pRangeLastReadGC  = NIL_RTGCPTR;
    pVM->iom.s.pRangeLastWriteGC = NIL_RTGCPTR;
    pVM->iom.s.pStatsLastReadGC  = NIL_RTGCPTR;
    pVM->iom.s.pStatsLastWriteGC = NIL_RTGCPTR;
    pVM->iom.s.pMMIORangeLastGC  = NIL_RTGCPTR;
    pVM->iom.s.pMMIOStatsLastGC  = NIL_RTGCPTR;
}


/**
 * The VM is being reset.
 *
 * @param   pVM     VM handle.
 */
IOMR3DECL(void) IOMR3Reset(PVM pVM)
{
    iomR3FlushCache(pVM);
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The IOM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
IOMR3DECL(void) IOMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("IOMR3Relocate: offDelta=%d\n", offDelta));

    /*
     * Apply relocations to the GC callbacks.
     */
    pVM->iom.s.pTreesGC = MMHyperHC2GC(pVM, pVM->iom.s.pTreesHC);
    RTAvlroIOPortDoWithAll(&pVM->iom.s.pTreesHC->IOPortTreeGC, true, iomr3RelocateIOPortCallback, &offDelta);
    RTAvlroGCPhysDoWithAll(&pVM->iom.s.pTreesHC->MMIOTree, true, iomr3RelocateMMIOCallback, &offDelta);

    if (pVM->iom.s.pfnMMIOHandlerGC)
        pVM->iom.s.pfnMMIOHandlerGC += offDelta;

    /*
     * Apply relocations to the cached GC handlers
     */
    if (pVM->iom.s.pRangeLastReadGC)
        pVM->iom.s.pRangeLastReadGC  += offDelta;
    if (pVM->iom.s.pRangeLastWriteGC)
        pVM->iom.s.pRangeLastWriteGC += offDelta;
    if (pVM->iom.s.pStatsLastReadGC)
        pVM->iom.s.pStatsLastReadGC  += offDelta;
    if (pVM->iom.s.pStatsLastWriteGC)
        pVM->iom.s.pStatsLastWriteGC += offDelta;
    if (pVM->iom.s.pMMIORangeLastGC)
        pVM->iom.s.pMMIORangeLastGC  += offDelta;
    if (pVM->iom.s.pMMIOStatsLastGC)
        pVM->iom.s.pMMIOStatsLastGC  += offDelta;
}


/**
 * Callback function for relocating a I/O port range.
 *
 * @returns 0 (continue enum)
 * @param   pNode       Pointer to a IOMIOPORTRANGEGC node.
 * @param   pvUser      Pointer to the offDelta. This is a pointer to the delta since we're
 *                      not certain the delta will fit in a void pointer for all possible configs.
 */
static DECLCALLBACK(int) iomr3RelocateIOPortCallback(PAVLROIOPORTNODECORE pNode, void *pvUser)
{
    PIOMIOPORTRANGEGC pRange = (PIOMIOPORTRANGEGC)pNode;
    RTGCINTPTR      offDelta = *(PRTGCINTPTR)pvUser;

    Assert(pRange->pDevIns);
    pRange->pDevIns                 += offDelta;
    if (pRange->pfnOutCallback)
        pRange->pfnOutCallback      += offDelta;
    if (pRange->pfnInCallback)
        pRange->pfnInCallback       += offDelta;
    if (pRange->pfnOutStrCallback)
        pRange->pfnOutStrCallback   += offDelta;
    if (pRange->pfnInStrCallback)
        pRange->pfnInStrCallback    += offDelta;
    if (pRange->pvUser > _64K)
        pRange->pvUser              += offDelta;
    return 0;
}


/**
 * Callback function for relocating a MMIO range.
 *
 * @returns 0 (continue enum)
 * @param   pNode       Pointer to a IOMMMIORANGE node.
 * @param   pvUser      Pointer to the offDelta. This is a pointer to the delta since we're
 *                      not certain the delta will fit in a void pointer for all possible configs.
 */
static DECLCALLBACK(int) iomr3RelocateMMIOCallback(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PIOMMMIORANGE pRange = (PIOMMMIORANGE)pNode;
    RTGCINTPTR    offDelta = *(PRTGCINTPTR)pvUser;

    if (pRange->pDevInsGC)
        pRange->pDevInsGC           += offDelta;
    if (pRange->pfnWriteCallbackGC)
        pRange->pfnWriteCallbackGC  += offDelta;
    if (pRange->pfnReadCallbackGC)
        pRange->pfnReadCallbackGC   += offDelta;
    if (pRange->pfnFillCallbackGC)
        pRange->pfnFillCallbackGC   += offDelta;
    if (pRange->pvUserGC > _64K)
        pRange->pvUserGC            += offDelta;

    return 0;
}


/**
 * Terminates the IOM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
IOMR3DECL(int) IOMR3Term(PVM pVM)
{
    /*
     * IOM is not owning anything but automatically freed resources,
     * so there's nothing to do here.
     */
    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_STATISTICS
/**
 * Create the statistics node for an I/O port.
 *
 * @returns Pointer to new stats node.
 *
 * @param   pVM         VM handle.
 * @param   Port        Port.
 * @param   pszDesc     Description.
 */
PIOMIOPORTSTATS iomr3IOPortStatsCreate(PVM pVM, RTIOPORT Port, const char *pszDesc)
{
    /* check if it already exists. */
    PIOMIOPORTSTATS pPort = (PIOMIOPORTSTATS)RTAvloIOPortGet(&pVM->iom.s.pTreesHC->IOPortStatTree, Port);
    if (pPort)
        return pPort;

    /* allocate stats node. */
    int rc = MMHyperAlloc(pVM, sizeof(*pPort), 0, MM_TAG_IOM_STATS, (void **)&pPort);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        /* insert into the tree. */
        pPort->Core.Key = Port;
        if (RTAvloIOPortInsert(&pVM->iom.s.pTreesHC->IOPortStatTree, &pPort->Core))
        {
            /* put a name on common ports. */
            if (!pszDesc)
                pszDesc = iomr3IOPortGetStandardName(Port);

            /* register the statistics counters. */
            char szName[64];
            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-R3", Port);
            rc = STAMR3Register(pVM, &pPort->InR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-R3", Port);
            rc = STAMR3Register(pVM, &pPort->OutR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-GC", Port);
            rc = STAMR3Register(pVM, &pPort->InGC, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-GC", Port);
            rc = STAMR3Register(pVM, &pPort->OutGC, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-GC-2-R3", Port);
            rc = STAMR3Register(pVM, &pPort->InGCToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-GC-2-R3", Port);
            rc = STAMR3Register(pVM, &pPort->OutGCToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-R0", Port);
            rc = STAMR3Register(pVM, &pPort->InR0, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-R0", Port);
            rc = STAMR3Register(pVM, &pPort->OutR0, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-R0-2-R3", Port);
            rc = STAMR3Register(pVM, &pPort->InR0ToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-R0-2-R3", Port);
            rc = STAMR3Register(pVM, &pPort->OutR0ToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, pszDesc);
            AssertRC(rc);

            /* Profiling */
            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-R3/Prof", Port);
            rc = STAMR3Register(pVM, &pPort->ProfInR3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-R3/Prof", Port);
            rc = STAMR3Register(pVM, &pPort->ProfOutR3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-GC/Prof", Port);
            rc = STAMR3Register(pVM, &pPort->ProfInGC, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-GC/Prof", Port);
            rc = STAMR3Register(pVM, &pPort->ProfOutGC, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-In-R0/Prof", Port);
            rc = STAMR3Register(pVM, &pPort->ProfInR0, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszDesc);
            AssertRC(rc);

            RTStrPrintf(szName, sizeof(szName), "/IOM/Ports/%04x-Out-R0/Prof", Port);
            rc = STAMR3Register(pVM, &pPort->ProfOutR0, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szName, STAMUNIT_TICKS_PER_CALL, pszDesc);
            AssertRC(rc);

            return pPort;
        }
        AssertMsgFailed(("what! Port=%d\n", Port));
        MMHyperFree(pVM, pPort);
    }
    return NULL;
}


/**
 * Create the statistics node for an MMIO address.
 *
 * @returns Pointer to new stats node.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The address.
 * @param   pszDesc     Description.
 */
PIOMMMIOSTATS iomR3MMIOStatsCreate(PVM pVM, RTGCPHYS GCPhys, const char *pszDesc)
{
#ifdef DEBUG_sandervl
    AssertGCPhys32(GCPhys);
#endif
    /* check if it already exists. */
    PIOMMMIOSTATS pStats = (PIOMMMIOSTATS)RTAvloGCPhysGet(&pVM->iom.s.pTreesHC->MMIOStatTree, GCPhys);
    if (pStats)
        return pStats;

    /* allocate stats node. */
    int rc = MMHyperAlloc(pVM, sizeof(*pStats), 0, MM_TAG_IOM_STATS, (void **)&pStats);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        /* insert into the tree. */
        pStats->Core.Key = GCPhys;
        if (RTAvloGCPhysInsert(&pVM->iom.s.pTreesHC->MMIOStatTree, &pStats->Core))
        {
            /* register the statistics counters. */
            rc = STAMR3RegisterF(pVM, &pStats->ReadR3,      STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Read-R3", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->WriteR3,     STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Write-R3", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ReadGC,      STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Read-GC", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->WriteGC,     STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Write-GC", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ReadGCToR3,  STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Read-GC-2-R3", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->WriteGCToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Write-GC-2-R3", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ReadR0,      STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Read-R0", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->WriteR0,     STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Write-R0", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ReadR0ToR3,  STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Read-R0-2-R3", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->WriteR0ToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, pszDesc, "/IOM/MMIO/%RGp-Write-R0-2-R3", GCPhys);
            AssertRC(rc);

            /* Profiling */
            rc = STAMR3RegisterF(pVM, &pStats->ProfReadR3,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp-Read-R3/Prof", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfWriteR3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp-Write-R3/Prof", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfReadGC,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp-Read-GC/Prof", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfWriteGC, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp-Write-GC/Prof", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfReadR0,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp-Read-R0/Prof", GCPhys);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfWriteR0, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp-Write-R0/Prof", GCPhys);
            AssertRC(rc);

            return pStats;
        }
        AssertMsgFailed(("what! GCPhys=%RGp\n", GCPhys));
        MMHyperFree(pVM, pStats);
    }
    return NULL;
}
#endif /* VBOX_WITH_STATISTICS */


/**
 * Registers a I/O port ring-3 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register
 * ring-3 ranges before any GC and R0 ranges can be registerd using IOMR3IOPortRegisterGC()
 * and IOMR3IOPortRegisterR0().
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in R3.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in R3.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle string OUT operations in R3.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle string IN operations in R3.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMR3DECL(int) IOMR3IOPortRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
                                     R3PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R3PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                     R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback, const char *pszDesc)
{
    LogFlow(("IOMR3IOPortRegisterR3: pDevIns=%p PortStart=%#x cPorts=%#x pvUser=%VHv pfnOutCallback=%#x pfnInCallback=%#x pfnOutStrCallback=%#x pfnInStrCallback=%#x pszDesc=%s\n",
             pDevIns, PortStart, cPorts, pvUser, pfnOutCallback, pfnInCallback, pszDesc, pfnOutStrCallback, pfnInStrCallback));

    /*
     * Validate input.
     */
    if (    (RTUINT)PortStart + cPorts <= (RTUINT)PortStart
        ||  (RTUINT)PortStart + cPorts > 0x10000)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x (inclusive)! (%s)\n", PortStart, (RTUINT)PortStart + (cPorts - 1), pszDesc));
        return VERR_IOM_INVALID_IOPORT_RANGE;
    }
    if (!pfnOutCallback && !pfnInCallback)
    {
        AssertMsgFailed(("no handlers specfied for %#x-%#x (inclusive)! (%s)\n", PortStart, (RTUINT)PortStart + (cPorts - 1), pszDesc));
        return VERR_INVALID_PARAMETER;
    }
    if (!pfnOutCallback)
        pfnOutCallback = iomR3IOPortDummyOut;
    if (!pfnInCallback)
        pfnInCallback = iomR3IOPortDummyIn;
    if (!pfnOutStrCallback)
        pfnOutStrCallback = iomR3IOPortDummyOutStr;
    if (!pfnInStrCallback)
        pfnInStrCallback = iomR3IOPortDummyInStr;

    /* Flush the IO port lookup cache */
    iomR3FlushCache(pVM);

    /*
     * Allocate new range record and initialize it.
     */
    PIOMIOPORTRANGER3 pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = PortStart;
        pRange->Core.KeyLast    = PortStart + (cPorts - 1);
        pRange->Port            = PortStart;
        pRange->cPorts          = cPorts;
        pRange->pvUser          = pvUser;
        pRange->pDevIns         = pDevIns;
        pRange->pfnOutCallback  = pfnOutCallback;
        pRange->pfnInCallback   = pfnInCallback;
        pRange->pfnOutStrCallback = pfnOutStrCallback;
        pRange->pfnInStrCallback = pfnInStrCallback;
        pRange->pszDesc         = pszDesc;

        /*
         * Try Insert it.
         */
        if (RTAvlroIOPortInsert(&pVM->iom.s.pTreesHC->IOPortTreeR3, &pRange->Core))
        {
            #ifdef VBOX_WITH_STATISTICS
            for (unsigned iPort = 0; iPort < cPorts; iPort++)
                iomr3IOPortStatsCreate(pVM, PortStart + iPort, pszDesc);
            #endif
            return VINF_SUCCESS;
        }

        /* conflict. */
        DBGFR3Info(pVM, "ioport", NULL, NULL);
        AssertMsgFailed(("Port range %#x-%#x (%s) conflicts with existing range(s)!\n", PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_IOPORT_RANGE_CONFLICT;
    }

    return rc;
}


/**
 * Registers a Port IO GC handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMIOPortRegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in GC.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle string OUT operations in GC.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle string IN operations in GC.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMR3DECL(int)  IOMR3IOPortRegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTRCPTR pvUser,
                                      RCPTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, RCPTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                      RCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, RCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback, const char *pszDesc)
{
    LogFlow(("IOMR3IOPortRegisterGC: pDevIns=%p PortStart=%#x cPorts=%#x pvUser=%VRv pfnOutCallback=%VGv pfnInCallback=%VRv pfnOutStrCallback=%VRv  pfnInStrCallback=%VRv pszDesc=%s\n",
             pDevIns, PortStart, cPorts, pvUser, pfnOutCallback, pfnInCallback, pfnOutStrCallback, pfnInStrCallback, pszDesc));

    /*
     * Validate input.
     */
    if (    (RTUINT)PortStart + cPorts <= (RTUINT)PortStart
        ||  (RTUINT)PortStart + cPorts > 0x10000)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! (%s)\n", PortStart, (RTUINT)PortStart + (cPorts - 1), pszDesc));
        return VERR_IOM_INVALID_IOPORT_RANGE;
    }
    RTIOPORT PortLast = PortStart + (cPorts - 1);
    if (!pfnOutCallback && !pfnInCallback)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! No callbacks! (%s)\n", PortStart, PortLast, pszDesc));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate that there are ring-3 ranges for the ports.
     */
    RTIOPORT Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeR3, Port);
        if (!pRange)
        {
            AssertMsgFailed(("No R3! Port=#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NO_HC_IOPORT_RANGE;
        }
#ifndef IOM_NO_PDMINS_CHECKS
# ifndef IN_GC
        if (pRange->pDevIns != pDevIns)
# else
        if (pRange->pDevIns != MMHyperGC2HC(pVM, pDevIns))
# endif
        {
            AssertMsgFailed(("Not owner! Port=%#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NOT_IOPORT_RANGE_OWNER;
        }
#endif
        Port = pRange->Core.KeyLast + 1;
    }

    /* Flush the IO port lookup cache */
    iomR3FlushCache(pVM);

    /*
     * Allocate new range record and initialize it.
     */
    PIOMIOPORTRANGEGC pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = PortStart;
        pRange->Core.KeyLast    = PortLast;
        pRange->Port            = PortStart;
        pRange->cPorts          = cPorts;
        pRange->pvUser          = pvUser;
        pRange->pfnOutCallback  = pfnOutCallback;
        pRange->pfnInCallback   = pfnInCallback;
        pRange->pfnOutStrCallback = pfnOutStrCallback;
        pRange->pfnInStrCallback = pfnInStrCallback;
#ifdef IN_GC
        pRange->pDevIns         = pDevIns;
        pRange->pszDesc         = MMHyperGC2HC(pVM, (void *)pszDesc);
#else
        pRange->pDevIns         = MMHyperHC2GC(pVM, pDevIns);
        pRange->pszDesc         = pszDesc;
#endif

        /*
         * Insert it.
         */
        if (RTAvlroIOPortInsert(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeGC, &pRange->Core))
            return VINF_SUCCESS;

        /* conflict. */
        AssertMsgFailed(("Port range %#x-%#x (%s) conflicts with existing range(s)!\n", PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_IOPORT_RANGE_CONFLICT;
    }

    return rc;
}


/**
 * Registers a Port IO R0 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMR3IOPortRegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in GC.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle IN operations in GC.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMR3DECL(int)  IOMR3IOPortRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTR0PTR pvUser,
                                      R0PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R0PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                      R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                      const char *pszDesc)
{
    LogFlow(("IOMR3IOPortRegisterR0: pDevIns=%p PortStart=%#x cPorts=%#x pvUser=%VHv pfnOutCallback=%VHv pfnInCallback=%VHv pfnOutStrCallback=%VHv  pfnInStrCallback=%VHv pszDesc=%s\n",
             pDevIns, PortStart, cPorts, pvUser, pfnOutCallback, pfnInCallback, pfnOutStrCallback, pfnInStrCallback, pszDesc));

    /*
     * Validate input.
     */
    if (    (RTUINT)PortStart + cPorts <= (RTUINT)PortStart
        ||  (RTUINT)PortStart + cPorts > 0x10000)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! (%s)\n", PortStart, (RTUINT)PortStart + (cPorts - 1), pszDesc));
        return VERR_IOM_INVALID_IOPORT_RANGE;
    }
    RTIOPORT PortLast = PortStart + (cPorts - 1);
    if (!pfnOutCallback && !pfnInCallback)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x! No callbacks! (%s)\n", PortStart, PortLast, pszDesc));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate that there are ring-3 ranges for the ports.
     */
    RTIOPORT Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeR3, Port);
        if (!pRange)
        {
            AssertMsgFailed(("No R3! Port=#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NO_HC_IOPORT_RANGE;
        }
#ifndef IOM_NO_PDMINS_CHECKS
# ifndef IN_GC
        if (pRange->pDevIns != pDevIns)
# else
        if (pRange->pDevIns != MMHyperGC2HC(pVM, pDevIns))
# endif
        {
            AssertMsgFailed(("Not owner! Port=%#x %#x-%#x! (%s)\n", Port, PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
            return VERR_IOM_NOT_IOPORT_RANGE_OWNER;
        }
#endif
        Port = pRange->Core.KeyLast + 1;
    }

    /* Flush the IO port lookup cache */
    iomR3FlushCache(pVM);

    /*
     * Allocate new range record and initialize it.
     */
    PIOMIOPORTRANGER0 pRange;
    int rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key        = PortStart;
        pRange->Core.KeyLast    = PortLast;
        pRange->Port            = PortStart;
        pRange->cPorts          = cPorts;
        pRange->pvUser          = pvUser;
        pRange->pfnOutCallback  = pfnOutCallback;
        pRange->pfnInCallback   = pfnInCallback;
        pRange->pfnOutStrCallback = pfnOutStrCallback;
        pRange->pfnInStrCallback = pfnInStrCallback;
#ifdef IN_GC
        pRange->pDevIns         = MMHyperGCToR0(pVM, pDevIns);
        pRange->pszDesc         = MMHyperGCToR3(pVM, (void *)pszDesc);
#elif defined(IN_RING3)
        pRange->pDevIns         = MMHyperR3ToR0(pVM, pDevIns);
        pRange->pszDesc         = pszDesc;
#else
        pRange->pDevIns         = pDevIns;
        pRange->pszDesc         = MMHyperR0ToR3(pVM, (RTR0PTR)pszDesc);
#endif

        /*
         * Insert it.
         */
        if (RTAvlroIOPortInsert(&pVM->iom.s.CTXSUFF(pTrees)->IOPortTreeR0, &pRange->Core))
            return VINF_SUCCESS;

        /* conflict. */
        AssertMsgFailed(("Port range %#x-%#x (%s) conflicts with existing range(s)!\n", PortStart, (unsigned)PortStart + cPorts - 1, pszDesc));
        MMHyperFree(pVM, pRange);
        rc = VERR_IOM_IOPORT_RANGE_CONFLICT;
    }

    return rc;
}


/**
 * Deregisters a I/O Port range.
 *
 * The specified range must be registered using IOMR3IOPortRegister previous to
 * this call. The range does can be a smaller part of the range specified to
 * IOMR3IOPortRegister, but it can never be larger.
 *
 * This function will remove GC, R0 and R3 context port handlers for this range.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The virtual machine.
 * @param   pDevIns             The device instance associated with the range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to remove starting at PortStart.
 *
 * @remark  This function mainly for PCI PnP Config and will not do
 *          all the checks you might expect it to do.
 */
IOMR3DECL(int)  IOMR3IOPortDeregister(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts)
{
    LogFlow(("IOMR3IOPortDeregister: pDevIns=%p PortStart=%#x cPorts=%#x\n", pDevIns, PortStart, cPorts));

    /*
     * Validate input.
     */
    if (    (RTUINT)PortStart + cPorts < (RTUINT)PortStart
        ||  (RTUINT)PortStart + cPorts > 0x10000)
    {
        AssertMsgFailed(("Invalid port range %#x-%#x!\n", PortStart, (unsigned)PortStart + cPorts - 1));
        return VERR_IOM_INVALID_IOPORT_RANGE;
    }

    /* Flush the IO port lookup cache */
    iomR3FlushCache(pVM);

    /*
     * Check ownership.
     */
    RTIOPORT PortLast = PortStart + (cPorts - 1);
    RTIOPORT Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.pTreesHC->IOPortTreeR3, Port);
        if (pRange)
        {
            Assert(Port <= pRange->Core.KeyLast);
#ifndef IOM_NO_PDMINS_CHECKS
            if (pRange->pDevIns != pDevIns)
            {
                AssertMsgFailed(("Removal of ports in range %#x-%#x rejected because not owner of %#x-%#x (%s)\n",
                                 PortStart, PortLast, pRange->Core.Key, pRange->Core.KeyLast, pRange->pszDesc));
                return VERR_IOM_NOT_IOPORT_RANGE_OWNER;
            }
#endif /* !IOM_NO_PDMINS_CHECKS */
            Port = pRange->Core.KeyLast;
        }
        Port++;
    }

    /*
     * Remove any GC ranges first.
     */
    int     rc = VINF_SUCCESS;
    Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        /*
         * Try find range.
         */
        PIOMIOPORTRANGEGC pRange = (PIOMIOPORTRANGEGC)RTAvlroIOPortRangeGet(&pVM->iom.s.pTreesHC->IOPortTreeGC, Port);
        if (pRange)
        {
            if (   pRange->Core.Key     == Port
                && pRange->Core.KeyLast <= PortLast)
            {
                /*
                 * Kick out the entire range.
                 */
                void *pv = RTAvlroIOPortRemove(&pVM->iom.s.pTreesHC->IOPortTreeGC, Port);
                Assert(pv == (void *)pRange); NOREF(pv);
                Port += pRange->cPorts;
                MMHyperFree(pVM, pRange);
            }
            else if (pRange->Core.Key == Port)
            {
                /*
                 * Cut of the head of the range, done.
                 */
                pRange->cPorts  -= Port - pRange->Port;
                pRange->Core.Key = Port;
                pRange->Port     = Port;
                break;
            }
            else if (pRange->Core.KeyLast <= PortLast)
            {
                /*
                 * Just cut of the tail.
                 */
                unsigned c = pRange->Core.KeyLast - Port + 1;
                pRange->Core.KeyLast -= c;
                pRange->cPorts -= c;
                Port += c;
            }
            else
            {
                 /*
                  * Split the range, done.
                  */
                Assert(pRange->Core.KeyLast > PortLast && pRange->Core.Key < Port);
                 /* create tail. */
                 PIOMIOPORTRANGEGC pRangeNew;
                 int rc = MMHyperAlloc(pVM, sizeof(*pRangeNew), 0, MM_TAG_IOM, (void **)&pRangeNew);
                 if (VBOX_FAILURE(rc))
                     return rc;

                 *pRangeNew = *pRange;
                 pRangeNew->Core.Key     = PortLast;
                 pRangeNew->Port         = PortLast;
                 pRangeNew->cPorts       = pRangeNew->Core.KeyLast - PortLast + 1;

                 /* adjust head */
                 pRange->Core.KeyLast  = Port - 1;
                 pRange->cPorts        = Port - pRange->Port;

                 /* insert */
                 if (!RTAvlroIOPortInsert(&pVM->iom.s.pTreesHC->IOPortTreeGC, &pRangeNew->Core))
                 {
                     AssertMsgFailed(("This cannot happen!\n"));
                     MMHyperFree(pVM, pRangeNew);
                     rc = VERR_INTERNAL_ERROR;
                 }
                 break;
            }
        }
        else /* next port */
            Port++;
    } /* for all ports - GC. */


    /*
     * Remove any R0 ranges first.
     */
    rc = VINF_SUCCESS;
    Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        /*
         * Try find range.
         */
        PIOMIOPORTRANGER0 pRange = (PIOMIOPORTRANGER0)RTAvlroIOPortRangeGet(&pVM->iom.s.pTreesHC->IOPortTreeR0, Port);
        if (pRange)
        {
            if (   pRange->Core.Key     == Port
                && pRange->Core.KeyLast <= PortLast)
            {
                /*
                 * Kick out the entire range.
                 */
                void *pv = RTAvlroIOPortRemove(&pVM->iom.s.pTreesHC->IOPortTreeR0, Port);
                Assert(pv == (void *)pRange); NOREF(pv);
                Port += pRange->cPorts;
                MMHyperFree(pVM, pRange);
            }
            else if (pRange->Core.Key == Port)
            {
                /*
                 * Cut of the head of the range, done.
                 */
                pRange->cPorts  -= Port - pRange->Port;
                pRange->Core.Key = Port;
                pRange->Port     = Port;
                break;
            }
            else if (pRange->Core.KeyLast <= PortLast)
            {
                /*
                 * Just cut of the tail.
                 */
                unsigned c = pRange->Core.KeyLast - Port + 1;
                pRange->Core.KeyLast -= c;
                pRange->cPorts -= c;
                Port += c;
            }
            else
            {
                 /*
                  * Split the range, done.
                  */
                Assert(pRange->Core.KeyLast > PortLast && pRange->Core.Key < Port);
                 /* create tail. */
                 PIOMIOPORTRANGER0 pRangeNew;
                 int rc = MMHyperAlloc(pVM, sizeof(*pRangeNew), 0, MM_TAG_IOM, (void **)&pRangeNew);
                 if (VBOX_FAILURE(rc))
                     return rc;

                 *pRangeNew = *pRange;
                 pRangeNew->Core.Key     = PortLast;
                 pRangeNew->Port         = PortLast;
                 pRangeNew->cPorts       = pRangeNew->Core.KeyLast - PortLast + 1;

                 /* adjust head */
                 pRange->Core.KeyLast  = Port - 1;
                 pRange->cPorts        = Port - pRange->Port;

                 /* insert */
                 if (!RTAvlroIOPortInsert(&pVM->iom.s.pTreesHC->IOPortTreeR0, &pRangeNew->Core))
                 {
                     AssertMsgFailed(("This cannot happen!\n"));
                     MMHyperFree(pVM, pRangeNew);
                     rc = VERR_INTERNAL_ERROR;
                 }
                 break;
            }
        }
        else /* next port */
            Port++;
    } /* for all ports - R0. */

    /*
     * And the same procedure for ring-3 ranges.
     */
    Port = PortStart;
    while (Port <= PortLast && Port >= PortStart)
    {
        /*
         * Try find range.
         */
        PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)RTAvlroIOPortRangeGet(&pVM->iom.s.pTreesHC->IOPortTreeR3, Port);
        if (pRange)
        {
            if (   pRange->Core.Key     == Port
                && pRange->Core.KeyLast <= PortLast)
            {
                /*
                 * Kick out the entire range.
                 */
                void *pv = RTAvlroIOPortRemove(&pVM->iom.s.pTreesHC->IOPortTreeR3, Port);
                Assert(pv == (void *)pRange); NOREF(pv);
                Port += pRange->cPorts;
                MMHyperFree(pVM, pRange);
            }
            else if (pRange->Core.Key == Port)
            {
                /*
                 * Cut of the head of the range, done.
                 */
                pRange->cPorts  -= Port - pRange->Port;
                pRange->Core.Key = Port;
                pRange->Port     = Port;
                break;
            }
            else if (pRange->Core.KeyLast <= PortLast)
            {
                /*
                 * Just cut of the tail.
                 */
                unsigned c = pRange->Core.KeyLast - Port + 1;
                pRange->Core.KeyLast -= c;
                pRange->cPorts -= c;
                Port += c;
            }
            else
            {
                /*
                 * Split the range, done.
                 */
                Assert(pRange->Core.KeyLast > PortLast && pRange->Core.Key < Port);
                /* create tail. */
                PIOMIOPORTRANGER3 pRangeNew;
                int rc = MMHyperAlloc(pVM, sizeof(*pRangeNew), 0, MM_TAG_IOM, (void **)&pRangeNew);
                if (VBOX_FAILURE(rc))
                    return rc;

                *pRangeNew = *pRange;
                pRangeNew->Core.Key     = PortLast;
                pRangeNew->Port         = PortLast;
                pRangeNew->cPorts       = pRangeNew->Core.KeyLast - PortLast + 1;

                /* adjust head */
                pRange->Core.KeyLast  = Port - 1;
                pRange->cPorts        = Port - pRange->Port;

                /* insert */
                if (!RTAvlroIOPortInsert(&pVM->iom.s.pTreesHC->IOPortTreeR3, &pRangeNew->Core))
                {
                    AssertMsgFailed(("This cannot happen!\n"));
                    MMHyperFree(pVM, pRangeNew);
                    rc = VERR_INTERNAL_ERROR;
                }
                break;
            }
        }
        else /* next port */
            Port++;
    } /* for all ports - ring-3. */

    /* done */
    return rc;
}


/**
 * Dummy Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) iomR3IOPortDummyIn(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    switch (cb)
    {
        case 1: *pu32 = 0xff; break;
        case 2: *pu32 = 0xffff; break;
        case 4: *pu32 = 0xffffffff; break;
        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb));
            return VERR_INTERNAL_ERROR;
    }
    return VINF_SUCCESS;
}


/**
 * Dummy Port I/O Handler for string IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the string IN operation.
 * @param   pGCPtrDst   Pointer to the destination buffer (GC, incremented appropriately).
 * @param   pcTransfer  Pointer to the number of transfer units to read, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 */
static DECLCALLBACK(int) iomR3IOPortDummyInStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfer, unsigned cb)
{
    return VINF_SUCCESS;
}


/**
 * Dummy Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the OUT operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) iomR3IOPortDummyOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    return VINF_SUCCESS;
}


/**
 * Dummy Port I/O Handler for string OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the string OUT operation.
 * @param   pGCPtrSrc   Pointer to the source buffer (GC, incremented appropriately).
 * @param   pcTransfer  Pointer to the number of transfer units to write, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 */
static DECLCALLBACK(int) iomR3IOPortDummyOutStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb)
{
    return VINF_SUCCESS;
}


/**
 * Display a single I/O port ring-3 range.
 *
 * @returns 0
 * @param   pNode   Pointer to I/O port HC range.
 * @param   pvUser  Pointer to info output callback structure.
 */
static DECLCALLBACK(int) iomR3IOPortInfoOneR3(PAVLROIOPORTNODECORE pNode, void *pvUser)
{
    PIOMIOPORTRANGER3 pRange = (PIOMIOPORTRANGER3)pNode;
    PCDBGFINFOHLP pHlp = (PCDBGFINFOHLP)pvUser;
    pHlp->pfnPrintf(pHlp,
                    "%04x-%04x %VHv %VHv %VHv %VHv %s\n",
                    pRange->Core.Key,
                    pRange->Core.KeyLast,
                    pRange->pDevIns,
                    pRange->pfnInCallback,
                    pRange->pfnOutCallback,
                    pRange->pvUser,
                    pRange->pszDesc);
    return 0;
}


/**
 * Display a single I/O port GC range.
 *
 * @returns 0
 * @param   pNode   Pointer to IOPORT GC range.
 * @param   pvUser  Pointer to info output callback structure.
 */
static DECLCALLBACK(int) iomR3IOPortInfoOneGC(PAVLROIOPORTNODECORE pNode, void *pvUser)
{
    PIOMIOPORTRANGEGC pRange = (PIOMIOPORTRANGEGC)pNode;
    PCDBGFINFOHLP pHlp = (PCDBGFINFOHLP)pvUser;
    pHlp->pfnPrintf(pHlp,
                    "%04x-%04x %VRv %VRv %VRv %VRv %s\n",
                    pRange->Core.Key,
                    pRange->Core.KeyLast,
                    pRange->pDevIns,
                    pRange->pfnInCallback,
                    pRange->pfnOutCallback,
                    pRange->pvUser,
                    pRange->pszDesc);
    return 0;
}


/**
 * Display all registered I/O port ranges.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) iomR3IOPortInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    pHlp->pfnPrintf(pHlp,
                    "I/O Port R3 ranges (pVM=%p)\n"
                    "Range     %.*s %.*s %.*s %.*s Description\n",
                    pVM,
                    sizeof(RTHCPTR) * 2,      "pDevIns         ",
                    sizeof(RTHCPTR) * 2,      "In              ",
                    sizeof(RTHCPTR) * 2,      "Out             ",
                    sizeof(RTHCPTR) * 2,      "pvUser          ");
    RTAvlroIOPortDoWithAll(&pVM->iom.s.pTreesHC->IOPortTreeR3, true, iomR3IOPortInfoOneR3, (void *)pHlp);

    pHlp->pfnPrintf(pHlp,
                    "I/O Port R0 ranges (pVM=%p)\n"
                    "Range     %.*s %.*s %.*s %.*s Description\n",
                    pVM,
                    sizeof(RTHCPTR) * 2,      "pDevIns         ",
                    sizeof(RTHCPTR) * 2,      "In              ",
                    sizeof(RTHCPTR) * 2,      "Out             ",
                    sizeof(RTHCPTR) * 2,      "pvUser          ");
    RTAvlroIOPortDoWithAll(&pVM->iom.s.pTreesHC->IOPortTreeR0, true, iomR3IOPortInfoOneR3, (void *)pHlp);

    pHlp->pfnPrintf(pHlp,
                    "I/O Port GC ranges (pVM=%p)\n"
                    "Range     %.*s %.*s %.*s %.*s Description\n",
                    pVM,
                    sizeof(RTRCPTR) * 2,      "pDevIns         ",
                    sizeof(RTRCPTR) * 2,      "In              ",
                    sizeof(RTRCPTR) * 2,      "Out             ",
                    sizeof(RTRCPTR) * 2,      "pvUser          ");
    RTAvlroIOPortDoWithAll(&pVM->iom.s.pTreesHC->IOPortTreeGC, true, iomR3IOPortInfoOneGC, (void *)pHlp);

    if (pVM->iom.s.pRangeLastReadGC)
    {
        PIOMIOPORTRANGEGC pRange = (PIOMIOPORTRANGEGC)MMHyperGC2HC(pVM, pVM->iom.s.pRangeLastReadGC);
        pHlp->pfnPrintf(pHlp, "GC Read  Ports: %#04x-%#04x %VRv %s\n",
                        pRange->Port, pRange->Port + pRange->cPorts, pVM->iom.s.pRangeLastReadGC, pRange->pszDesc);
    }
    if (pVM->iom.s.pStatsLastReadGC)
    {
        PIOMIOPORTSTATS pRange = (PIOMIOPORTSTATS)MMHyperGC2HC(pVM, pVM->iom.s.pStatsLastReadGC);
        pHlp->pfnPrintf(pHlp, "GC Read  Stats: %#04x %VRv\n",
                        pRange->Core.Key, pVM->iom.s.pStatsLastReadGC);
    }

    if (pVM->iom.s.pRangeLastWriteGC)
    {
        PIOMIOPORTRANGEGC pRange = (PIOMIOPORTRANGEGC)MMHyperGC2HC(pVM, pVM->iom.s.pRangeLastWriteGC);
        pHlp->pfnPrintf(pHlp, "GC Write Ports: %#04x-%#04x %VRv %s\n",
                        pRange->Port, pRange->Port + pRange->cPorts, pVM->iom.s.pRangeLastWriteGC, pRange->pszDesc);
    }
    if (pVM->iom.s.pStatsLastWriteGC)
    {
        PIOMIOPORTSTATS pRange = (PIOMIOPORTSTATS)MMHyperGC2HC(pVM, pVM->iom.s.pStatsLastWriteGC);
        pHlp->pfnPrintf(pHlp, "GC Write Stats: %#04x %VRv\n",
                        pRange->Core.Key, pVM->iom.s.pStatsLastWriteGC);
    }

    if (pVM->iom.s.pRangeLastReadR3)
    {
        PIOMIOPORTRANGER3 pRange = pVM->iom.s.pRangeLastReadR3;
        pHlp->pfnPrintf(pHlp, "HC Read  Ports: %#04x-%#04x %VHv %s\n",
                        pRange->Port, pRange->Port + pRange->cPorts, pRange, pRange->pszDesc);
    }
    if (pVM->iom.s.pStatsLastReadR3)
    {
        PIOMIOPORTSTATS pRange = pVM->iom.s.pStatsLastReadR3;
        pHlp->pfnPrintf(pHlp, "HC Read  Stats: %#04x %VHv\n",
                        pRange->Core.Key, pRange);
    }

    if (pVM->iom.s.pRangeLastWriteR3)
    {
        PIOMIOPORTRANGER3 pRange = pVM->iom.s.pRangeLastWriteR3;
        pHlp->pfnPrintf(pHlp, "HC Write Ports: %#04x-%#04x %VHv %s\n",
                        pRange->Port, pRange->Port + pRange->cPorts, pRange, pRange->pszDesc);
    }
    if (pVM->iom.s.pStatsLastWriteR3)
    {
        PIOMIOPORTSTATS pRange = pVM->iom.s.pStatsLastWriteR3;
        pHlp->pfnPrintf(pHlp, "HC Write Stats: %#04x %VHv\n",
                        pRange->Core.Key, pRange);
    }

    if (pVM->iom.s.pRangeLastReadR0)
    {
        PIOMIOPORTRANGER0 pRange = (PIOMIOPORTRANGER0)MMHyperR0ToCC(pVM, pVM->iom.s.pRangeLastReadR0);
        pHlp->pfnPrintf(pHlp, "R0 Read  Ports: %#04x-%#04x %VHv %s\n",
                        pRange->Port, pRange->Port + pRange->cPorts, pRange, pRange->pszDesc);
    }
    if (pVM->iom.s.pStatsLastReadR0)
    {
        PIOMIOPORTSTATS pRange = (PIOMIOPORTSTATS)MMHyperR0ToCC(pVM, pVM->iom.s.pStatsLastReadR0);
        pHlp->pfnPrintf(pHlp, "R0 Read  Stats: %#04x %VHv\n",
                        pRange->Core.Key, pRange);
    }

    if (pVM->iom.s.pRangeLastWriteR0)
    {
        PIOMIOPORTRANGER0 pRange = (PIOMIOPORTRANGER0)MMHyperR0ToCC(pVM, pVM->iom.s.pRangeLastWriteR0);
        pHlp->pfnPrintf(pHlp, "R0 Write Ports: %#04x-%#04x %VGv %s\n",
                        pRange->Port, pRange->Port + pRange->cPorts, pRange, pRange->pszDesc);
    }
    if (pVM->iom.s.pStatsLastWriteR0)
    {
        PIOMIOPORTSTATS pRange = (PIOMIOPORTSTATS)MMHyperR0ToCC(pVM, pVM->iom.s.pStatsLastWriteR0);
        pHlp->pfnPrintf(pHlp, "R0 Write Stats: %#04x %VHv\n",
                        pRange->Core.Key, pRange);
    }
}


/**
 * Registers a Memory Mapped I/O R3 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must register ring-3 ranges
 * before any GC and R0 ranges can be registered using IOMR3MMIORegisterGC() and IOMR3MMIORegisterR0().
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMR3DECL(int)  IOMR3MMIORegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                    R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R3PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    R3PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc)
{
    LogFlow(("IOMR3MMIORegisterR3: pDevIns=%p GCPhysStart=%VGp cbRange=%#x pvUser=%VHv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x pszDesc=%s\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback, pszDesc));
    int rc;

    /*
     * Validate input.
     */
    if (GCPhysStart + (cbRange - 1) < GCPhysStart)
    {
        AssertMsgFailed(("Wrapped! %VGp %#x bytes\n", GCPhysStart, cbRange));
        return VERR_IOM_INVALID_MMIO_RANGE;
    }

    /*
     * Resolve the GC/R0 handler addresses lazily because of init order.
     */
    if (pVM->iom.s.pfnMMIOHandlerR0 == NIL_RTR0PTR)
    {
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "IOMMMIOHandler", &pVM->iom.s.pfnMMIOHandlerGC);
        AssertLogRelRCReturn(rc, rc);
        rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "IOMMMIOHandler", &pVM->iom.s.pfnMMIOHandlerR0);
        AssertLogRelRCReturn(rc, rc);
    }

    /*
     * Allocate new range record and initialize it.
     */
    PIOMMMIORANGE pRange;
    rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (VBOX_SUCCESS(rc))
    {
        pRange->Core.Key            = GCPhysStart;
        pRange->Core.KeyLast        = GCPhysStart + (cbRange - 1);
        pRange->GCPhys              = GCPhysStart;
        pRange->cb                  = cbRange;
        pRange->pszDesc             = pszDesc;

        pRange->pvUserR3            = pvUser;
        pRange->pDevInsR3           = pDevIns;
        pRange->pfnReadCallbackR3   = pfnReadCallback;
        pRange->pfnWriteCallbackR3  = pfnWriteCallback;
        pRange->pfnFillCallbackR3   = pfnFillCallback;

        //pRange->pvUserR0            = NIL_RTR0PTR;
        //pRange->pDevInsR0           = NIL_RTR0PTR;
        //pRange->pfnReadCallbackR0   = NIL_RTR0PTR;
        //pRange->pfnWriteCallbackR0  = NIL_RTR0PTR;
        //pRange->pfnFillCallbackR0   = NIL_RTR0PTR;

        //pRange->pvUserGC            = NIL_RTGCPTR;
        //pRange->pDevInsGC           = NIL_RTGCPTR;
        //pRange->pfnReadCallbackGC   = NIL_RTGCPTR;
        //pRange->pfnWriteCallbackGC  = NIL_RTGCPTR;
        //pRange->pfnFillCallbackGC   = NIL_RTGCPTR;

        /*
         * Try register it with PGM and then insert it into the tree.
         */
        rc = PGMR3PhysMMIORegister(pVM, GCPhysStart, cbRange,
                                   /*IOMR3MMIOHandler*/ NULL, pRange,
                                   pVM->iom.s.pfnMMIOHandlerR0, MMHyperR3ToR0(pVM, pRange),
                                   pVM->iom.s.pfnMMIOHandlerGC, MMHyperR3ToGC(pVM, pRange), pszDesc);
        if (RT_SUCCESS(rc))
        {
            if (RTAvlroGCPhysInsert(&pVM->iom.s.pTreesHC->MMIOTree, &pRange->Core))
                return VINF_SUCCESS;

            DBGFR3Info(pVM, "mmio", NULL, NULL);
            AssertMsgFailed(("This cannot happen!\n"));
            rc = VERR_INTERNAL_ERROR;
        }
        MMHyperFree(pVM, pRange);
    }

    return rc;
}


/**
 * Registers a Memory Mapped I/O GC handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMMIORegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 */
IOMR3DECL(int)  IOMR3MMIORegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                    RCPTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, RCPTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    RCPTRTYPE(PFNIOMMMIOFILL) pfnFillCallback)
{
    LogFlow(("IOMR3MMIORegisterGC: pDevIns=%p GCPhysStart=%VGp cbRange=%#x pvUser=%VGv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback));

    /*
     * Validate input.
     */
    if (!pfnWriteCallback && !pfnReadCallback)
    {
        AssertMsgFailed(("No callbacks! %VGp LB%#x %s\n", GCPhysStart, cbRange));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find the MMIO range and check that the input matches.
     */
    PIOMMMIORANGE pRange = iomMMIOGetRange(&pVM->iom.s, GCPhysStart);
    AssertReturn(pRange, VERR_IOM_MMIO_RANGE_NOT_FOUND);
    AssertReturn(pRange->pDevInsR3 == pDevIns, VERR_IOM_NOT_MMIO_RANGE_OWNER);
    AssertReturn(pRange->GCPhys == GCPhysStart, VERR_IOM_INVALID_MMIO_RANGE);
    AssertReturn(pRange->cb == cbRange, VERR_IOM_INVALID_MMIO_RANGE);

    pRange->pvUserGC          = pvUser;
    pRange->pfnReadCallbackGC = pfnReadCallback;
    pRange->pfnWriteCallbackGC= pfnWriteCallback;
    pRange->pfnFillCallbackGC = pfnFillCallback;
    pRange->pDevInsGC         = MMHyperCCToGC(pVM, pDevIns);

    return VINF_SUCCESS;
}


/**
 * Registers a Memory Mapped I/O R0 handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMR3MIORegisterHC() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 */
IOMR3DECL(int)  IOMR3MMIORegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                    R0PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                                    R0PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    R0PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback)
{
    LogFlow(("IOMR3MMIORegisterR0: pDevIns=%p GCPhysStart=%VGp cbRange=%#x pvUser=%VHv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback));

    /*
     * Validate input.
     */
    if (!pfnWriteCallback && !pfnReadCallback)
    {
        AssertMsgFailed(("No callbacks! %VGp LB%#x %s\n", GCPhysStart, cbRange));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find the MMIO range and check that the input matches.
     */
    PIOMMMIORANGE pRange = iomMMIOGetRange(&pVM->iom.s, GCPhysStart);
    AssertReturn(pRange, VERR_IOM_MMIO_RANGE_NOT_FOUND);
    AssertReturn(pRange->pDevInsR3 == pDevIns, VERR_IOM_NOT_MMIO_RANGE_OWNER);
    AssertReturn(pRange->GCPhys == GCPhysStart, VERR_IOM_INVALID_MMIO_RANGE);
    AssertReturn(pRange->cb == cbRange, VERR_IOM_INVALID_MMIO_RANGE);

    pRange->pvUserR0          = pvUser;
    pRange->pfnReadCallbackR0 = pfnReadCallback;
    pRange->pfnWriteCallbackR0= pfnWriteCallback;
    pRange->pfnFillCallbackR0 = pfnFillCallback;
    pRange->pDevInsR0         = MMHyperCCToR0(pVM, pDevIns);

    return VINF_SUCCESS;
}


/**
 * Deregisters a Memory Mapped I/O handler range.
 *
 * Registered GC, R0, and R3 ranges are affected.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The virtual machine.
 * @param   pDevIns             Device instance which the MMIO region is registered.
 * @param   GCPhysStart         First physical address (GC) in the range.
 * @param   cbRange             Number of bytes to deregister.
 *
 * @remark  This function mainly for PCI PnP Config and will not do
 *          all the checks you might expect it to do.
 */
IOMR3DECL(int)  IOMR3MMIODeregister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange)
{
    LogFlow(("IOMR3MMIODeregister: pDevIns=%p GCPhysStart=%VGp cbRange=%#x\n", pDevIns, GCPhysStart, cbRange));

    /*
     * Validate input.
     */
    RTGCPHYS GCPhysLast = GCPhysStart + (cbRange - 1);
    if (GCPhysLast < GCPhysStart)
    {
        AssertMsgFailed(("Wrapped! %#x LB%#x\n", GCPhysStart, cbRange));
        return VERR_IOM_INVALID_MMIO_RANGE;
    }

    /*
     * Check ownership and such for the entire area.
     */
    RTGCPHYS GCPhys = GCPhysStart;
    while (GCPhys <= GCPhysLast && GCPhys >= GCPhysStart)
    {
        PIOMMMIORANGE pRange = iomMMIOGetRange(&pVM->iom.s, GCPhys);
        if (!pRange)
            return VERR_IOM_MMIO_RANGE_NOT_FOUND;
        AssertMsgReturn(pRange->pDevInsR3 == pDevIns,
                        ("Not owner! GCPhys=%VGp %VGp LB%#x %s\n", GCPhys, GCPhysStart, cbRange, pRange->pszDesc),
                        VERR_IOM_NOT_MMIO_RANGE_OWNER);
        AssertMsgReturn(pRange->Core.KeyLast <= GCPhysLast,
                        ("Incomplete R3 range! GCPhys=%VGp %VGp LB%#x %s\n", GCPhys, GCPhysStart, cbRange, pRange->pszDesc),
                        VERR_IOM_INCOMPLETE_MMIO_RANGE);

        /* next */
        Assert(GCPhys <= pRange->Core.KeyLast);
        GCPhys = pRange->Core.KeyLast + 1;
    }

    /*
     * Do the actual removing of the MMIO ranges.
     */
    GCPhys = GCPhysStart;
    while (GCPhys <= GCPhysLast && GCPhys >= GCPhysStart)
    {
        PIOMMMIORANGE pRange = (PIOMMMIORANGE)RTAvlroGCPhysRemove(&pVM->iom.s.pTreesHC->MMIOTree, GCPhys);
        Assert(pRange);
        Assert(pRange->Core.Key == GCPhys && pRange->Core.KeyLast <= GCPhysLast);

        /* remove it from PGM */
        int rc = PGMR3PhysMMIODeregister(pVM, GCPhys, pRange->cb);
        AssertRC(rc);

        /* advance and free. */
        GCPhys = pRange->Core.KeyLast + 1;
        MMHyperFree(pVM, pRange);
    }

    iomR3FlushCache(pVM);
    return VINF_SUCCESS;
}


/**
 * Display a single MMIO range.
 *
 * @returns 0
 * @param   pNode   Pointer to MMIO R3 range.
 * @param   pvUser  Pointer to info output callback structure.
 */
static DECLCALLBACK(int) iomR3MMIOInfoOne(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PIOMMMIORANGE pRange = (PIOMMMIORANGE)pNode;
    PCDBGFINFOHLP pHlp = (PCDBGFINFOHLP)pvUser;
    pHlp->pfnPrintf(pHlp,
                    "%VGp-%VGp %VHv %VHv %VHv %VHv %VHv %s\n",
                    pRange->Core.Key,
                    pRange->Core.KeyLast,
                    pRange->pDevInsR3,
                    pRange->pfnReadCallbackR3,
                    pRange->pfnWriteCallbackR3,
                    pRange->pfnFillCallbackR3,
                    pRange->pvUserR3,
                    pRange->pszDesc);
    pHlp->pfnPrintf(pHlp,
                    "%*s %VHv %VHv %VHv %VHv %VHv\n",
                    sizeof(RTGCPHYS) * 2 * 2 + 1, "R0",
                    pRange->pDevInsR0,
                    pRange->pfnReadCallbackR0,
                    pRange->pfnWriteCallbackR0,
                    pRange->pfnFillCallbackR0,
                    pRange->pvUserR0);
    pHlp->pfnPrintf(pHlp,
                    "%*s %VRv %VRv %VRv %VRv %VRv\n",
                    sizeof(RTGCPHYS) * 2 * 2 + 1, "GC",
                    pRange->pDevInsGC,
                    pRange->pfnReadCallbackGC,
                    pRange->pfnWriteCallbackGC,
                    pRange->pfnFillCallbackGC,
                    pRange->pvUserGC);
    return 0;
}


/**
 * Display registered MMIO ranges to the log.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) iomR3MMIOInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    pHlp->pfnPrintf(pHlp,
                    "MMIO ranges (pVM=%p)\n"
                    "%.*s %.*s %.*s %.*s %.*s %.*s %s\n",
                    pVM,
                    sizeof(RTGCPHYS) * 4 + 1, "GC Phys Range                    ",
                    sizeof(RTHCPTR) * 2,      "pDevIns         ",
                    sizeof(RTHCPTR) * 2,      "Read            ",
                    sizeof(RTHCPTR) * 2,      "Write           ",
                    sizeof(RTHCPTR) * 2,      "Fill            ",
                    sizeof(RTHCPTR) * 2,      "pvUser          ",
                                              "Description");
    RTAvlroGCPhysDoWithAll(&pVM->iom.s.pTreesHC->MMIOTree, true, iomR3MMIOInfoOne, (void *)pHlp);
}


#ifdef VBOX_WITH_STATISTICS
/**
 * Tries to come up with the standard name for a port.
 *
 * @returns Pointer to readonly string if known.
 * @returns NULL if unknown port number.
 *
 * @param   Port    The port to name.
 */
static const char *iomr3IOPortGetStandardName(RTIOPORT Port)
{
    switch (Port)
    {
        case 0x00: case 0x10: case 0x20: case 0x30: case 0x40: case 0x50:            case 0x70:
        case 0x01: case 0x11: case 0x21: case 0x31: case 0x41: case 0x51: case 0x61: case 0x71:
        case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52: case 0x62: case 0x72:
        case 0x03: case 0x13: case 0x23: case 0x33: case 0x43: case 0x53: case 0x63: case 0x73:
        case 0x04: case 0x14: case 0x24: case 0x34: case 0x44: case 0x54:            case 0x74:
        case 0x05: case 0x15: case 0x25: case 0x35: case 0x45: case 0x55: case 0x65: case 0x75:
        case 0x06: case 0x16: case 0x26: case 0x36: case 0x46: case 0x56: case 0x66: case 0x76:
        case 0x07: case 0x17: case 0x27: case 0x37: case 0x47: case 0x57: case 0x67: case 0x77:
        case 0x08: case 0x18: case 0x28: case 0x38: case 0x48: case 0x58: case 0x68: case 0x78:
        case 0x09: case 0x19: case 0x29: case 0x39: case 0x49: case 0x59: case 0x69: case 0x79:
        case 0x0a: case 0x1a: case 0x2a: case 0x3a: case 0x4a: case 0x5a: case 0x6a: case 0x7a:
        case 0x0b: case 0x1b: case 0x2b: case 0x3b: case 0x4b: case 0x5b: case 0x6b: case 0x7b:
        case 0x0c: case 0x1c: case 0x2c: case 0x3c: case 0x4c: case 0x5c: case 0x6c: case 0x7c:
        case 0x0d: case 0x1d: case 0x2d: case 0x3d: case 0x4d: case 0x5d: case 0x6d: case 0x7d:
        case 0x0e: case 0x1e: case 0x2e: case 0x3e: case 0x4e: case 0x5e: case 0x6e: case 0x7e:
        case 0x0f: case 0x1f: case 0x2f: case 0x3f: case 0x4f: case 0x5f: case 0x6f: case 0x7f:

        case 0x80: case 0x90: case 0xa0: case 0xb0: case 0xc0: case 0xd0: case 0xe0: case 0xf0:
        case 0x81: case 0x91: case 0xa1: case 0xb1: case 0xc1: case 0xd1: case 0xe1: case 0xf1:
        case 0x82: case 0x92: case 0xa2: case 0xb2: case 0xc2: case 0xd2: case 0xe2: case 0xf2:
        case 0x83: case 0x93: case 0xa3: case 0xb3: case 0xc3: case 0xd3: case 0xe3: case 0xf3:
        case 0x84: case 0x94: case 0xa4: case 0xb4: case 0xc4: case 0xd4: case 0xe4: case 0xf4:
        case 0x85: case 0x95: case 0xa5: case 0xb5: case 0xc5: case 0xd5: case 0xe5: case 0xf5:
        case 0x86: case 0x96: case 0xa6: case 0xb6: case 0xc6: case 0xd6: case 0xe6: case 0xf6:
        case 0x87: case 0x97: case 0xa7: case 0xb7: case 0xc7: case 0xd7: case 0xe7: case 0xf7:
        case 0x88: case 0x98: case 0xa8: case 0xb8: case 0xc8: case 0xd8: case 0xe8: case 0xf8:
        case 0x89: case 0x99: case 0xa9: case 0xb9: case 0xc9: case 0xd9: case 0xe9: case 0xf9:
        case 0x8a: case 0x9a: case 0xaa: case 0xba: case 0xca: case 0xda: case 0xea: case 0xfa:
        case 0x8b: case 0x9b: case 0xab: case 0xbb: case 0xcb: case 0xdb: case 0xeb: case 0xfb:
        case 0x8c: case 0x9c: case 0xac: case 0xbc: case 0xcc: case 0xdc: case 0xec: case 0xfc:
        case 0x8d: case 0x9d: case 0xad: case 0xbd: case 0xcd: case 0xdd: case 0xed: case 0xfd:
        case 0x8e: case 0x9e: case 0xae: case 0xbe: case 0xce: case 0xde: case 0xee: case 0xfe:
        case 0x8f: case 0x9f: case 0xaf: case 0xbf: case 0xcf: case 0xdf: case 0xef: case 0xff:
            return "System Reserved";

        case 0x60:
        case 0x64:
            return "Keyboard & Mouse";

        case 0x378:
        case 0x379:
        case 0x37a:
        case 0x37b:
        case 0x37c:
        case 0x37d:
        case 0x37e:
        case 0x37f:
        case 0x3bc:
        case 0x3bd:
        case 0x3be:
        case 0x3bf:
        case 0x278:
        case 0x279:
        case 0x27a:
        case 0x27b:
        case 0x27c:
        case 0x27d:
        case 0x27e:
        case 0x27f:
            return "LPT1/2/3";

        case 0x3f8:
        case 0x3f9:
        case 0x3fa:
        case 0x3fb:
        case 0x3fc:
        case 0x3fd:
        case 0x3fe:
        case 0x3ff:
            return "COM1";

        case 0x2f8:
        case 0x2f9:
        case 0x2fa:
        case 0x2fb:
        case 0x2fc:
        case 0x2fd:
        case 0x2fe:
        case 0x2ff:
            return "COM2";

        case 0x3e8:
        case 0x3e9:
        case 0x3ea:
        case 0x3eb:
        case 0x3ec:
        case 0x3ed:
        case 0x3ee:
        case 0x3ef:
            return "COM3";

        case 0x2e8:
        case 0x2e9:
        case 0x2ea:
        case 0x2eb:
        case 0x2ec:
        case 0x2ed:
        case 0x2ee:
        case 0x2ef:
            return "COM4";

        case 0x200:
        case 0x201:
        case 0x202:
        case 0x203:
        case 0x204:
        case 0x205:
        case 0x206:
        case 0x207:
            return "Joystick";

        case 0x3f0:
        case 0x3f1:
        case 0x3f2:
        case 0x3f3:
        case 0x3f4:
        case 0x3f5:
        case 0x3f6:
        case 0x3f7:
            return "Floppy";

        case 0x1f0:
        case 0x1f1:
        case 0x1f2:
        case 0x1f3:
        case 0x1f4:
        case 0x1f5:
        case 0x1f6:
        case 0x1f7:
        //case 0x3f6:
        //case 0x3f7:
            return "IDE 1st";

        case 0x170:
        case 0x171:
        case 0x172:
        case 0x173:
        case 0x174:
        case 0x175:
        case 0x176:
        case 0x177:
        case 0x376:
        case 0x377:
            return "IDE 2nd";

        case 0x1e0:
        case 0x1e1:
        case 0x1e2:
        case 0x1e3:
        case 0x1e4:
        case 0x1e5:
        case 0x1e6:
        case 0x1e7:
        case 0x3e6:
        case 0x3e7:
            return "IDE 3rd";

        case 0x160:
        case 0x161:
        case 0x162:
        case 0x163:
        case 0x164:
        case 0x165:
        case 0x166:
        case 0x167:
        case 0x366:
        case 0x367:
            return "IDE 4th";

        case 0x130: case 0x140: case 0x150:
        case 0x131: case 0x141: case 0x151:
        case 0x132: case 0x142: case 0x152:
        case 0x133: case 0x143: case 0x153:
        case 0x134: case 0x144: case 0x154:
        case 0x135: case 0x145: case 0x155:
        case 0x136: case 0x146: case 0x156:
        case 0x137: case 0x147: case 0x157:
        case 0x138: case 0x148: case 0x158:
        case 0x139: case 0x149: case 0x159:
        case 0x13a: case 0x14a: case 0x15a:
        case 0x13b: case 0x14b: case 0x15b:
        case 0x13c: case 0x14c: case 0x15c:
        case 0x13d: case 0x14d: case 0x15d:
        case 0x13e: case 0x14e: case 0x15e:
        case 0x13f: case 0x14f: case 0x15f:
        case 0x220: case 0x230:
        case 0x221: case 0x231:
        case 0x222: case 0x232:
        case 0x223: case 0x233:
        case 0x224: case 0x234:
        case 0x225: case 0x235:
        case 0x226: case 0x236:
        case 0x227: case 0x237:
        case 0x228: case 0x238:
        case 0x229: case 0x239:
        case 0x22a: case 0x23a:
        case 0x22b: case 0x23b:
        case 0x22c: case 0x23c:
        case 0x22d: case 0x23d:
        case 0x22e: case 0x23e:
        case 0x22f: case 0x23f:
        case 0x330: case 0x340: case 0x350:
        case 0x331: case 0x341: case 0x351:
        case 0x332: case 0x342: case 0x352:
        case 0x333: case 0x343: case 0x353:
        case 0x334: case 0x344: case 0x354:
        case 0x335: case 0x345: case 0x355:
        case 0x336: case 0x346: case 0x356:
        case 0x337: case 0x347: case 0x357:
        case 0x338: case 0x348: case 0x358:
        case 0x339: case 0x349: case 0x359:
        case 0x33a: case 0x34a: case 0x35a:
        case 0x33b: case 0x34b: case 0x35b:
        case 0x33c: case 0x34c: case 0x35c:
        case 0x33d: case 0x34d: case 0x35d:
        case 0x33e: case 0x34e: case 0x35e:
        case 0x33f: case 0x34f: case 0x35f:
            return "SCSI (typically)";

        case 0x320:
        case 0x321:
        case 0x322:
        case 0x323:
        case 0x324:
        case 0x325:
        case 0x326:
        case 0x327:
            return "XT HD";

        case 0x3b0:
        case 0x3b1:
        case 0x3b2:
        case 0x3b3:
        case 0x3b4:
        case 0x3b5:
        case 0x3b6:
        case 0x3b7:
        case 0x3b8:
        case 0x3b9:
        case 0x3ba:
        case 0x3bb:
            return "VGA";

        case 0x3c0: case 0x3d0:
        case 0x3c1: case 0x3d1:
        case 0x3c2: case 0x3d2:
        case 0x3c3: case 0x3d3:
        case 0x3c4: case 0x3d4:
        case 0x3c5: case 0x3d5:
        case 0x3c6: case 0x3d6:
        case 0x3c7: case 0x3d7:
        case 0x3c8: case 0x3d8:
        case 0x3c9: case 0x3d9:
        case 0x3ca: case 0x3da:
        case 0x3cb: case 0x3db:
        case 0x3cc: case 0x3dc:
        case 0x3cd: case 0x3dd:
        case 0x3ce: case 0x3de:
        case 0x3cf: case 0x3df:
            return "VGA/EGA";

        case 0x240: case 0x260: case 0x280:
        case 0x241: case 0x261: case 0x281:
        case 0x242: case 0x262: case 0x282:
        case 0x243: case 0x263: case 0x283:
        case 0x244: case 0x264: case 0x284:
        case 0x245: case 0x265: case 0x285:
        case 0x246: case 0x266: case 0x286:
        case 0x247: case 0x267: case 0x287:
        case 0x248: case 0x268: case 0x288:
        case 0x249: case 0x269: case 0x289:
        case 0x24a: case 0x26a: case 0x28a:
        case 0x24b: case 0x26b: case 0x28b:
        case 0x24c: case 0x26c: case 0x28c:
        case 0x24d: case 0x26d: case 0x28d:
        case 0x24e: case 0x26e: case 0x28e:
        case 0x24f: case 0x26f: case 0x28f:
        case 0x300:
        case 0x301:
        case 0x388:
        case 0x389:
        case 0x38a:
        case 0x38b:
            return "Sound Card (typically)";

        default:
            return NULL;
    }
}
#endif /* VBOX_WITH_STATISTICS */

