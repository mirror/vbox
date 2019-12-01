/* $Id$ */
/** @file
 * IOM - Input / Output Monitor.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_iom        IOM - The Input / Output Monitor
 *
 * The input/output monitor will handle I/O exceptions routing them to the
 * appropriate device. It implements an API to register and deregister virtual
 * I/0 port handlers and memory mapped I/O handlers. A handler is PDM devices
 * and a set of callback functions.
 *
 * @see grp_iom
 *
 *
 * @section sec_iom_rawmode     Raw-Mode
 *
 * In raw-mode I/O port access is trapped (\#GP(0)) by ensuring that the actual
 * IOPL is 0 regardless of what the guest IOPL is. The \#GP handler use the
 * disassembler (DIS) to figure which instruction caused it (there are a number
 * of instructions in addition to the I/O ones) and if it's an I/O port access
 * it will hand it to IOMRCIOPortHandler (via EMInterpretPortIO).
 * IOMRCIOPortHandler will lookup the port in the AVL tree of registered
 * handlers. If found, the handler will be called otherwise default action is
 * taken. (Default action is to write into the void and read all set bits.)
 *
 * Memory Mapped I/O (MMIO) is implemented as a slightly special case of PGM
 * access handlers. An MMIO range is registered with IOM which then registers it
 * with the PGM access handler sub-system. The access handler catches all
 * access and will be called in the context of a \#PF handler. In RC and R0 this
 * handler is iomMmioPfHandler while in ring-3 it's iomR3MmioHandler (although
 * in ring-3 there can be alternative ways). iomMmioPfHandler will attempt to
 * emulate the instruction that is doing the access and pass the corresponding
 * reads / writes to the device.
 *
 * Emulating I/O port access is less complex and should be slightly faster than
 * emulating MMIO, so in most cases we should encourage the OS to use port I/O.
 * Devices which are frequently accessed should register GC handlers to speed up
 * execution.
 *
 *
 * @section sec_iom_hm     Hardware Assisted Virtualization Mode
 *
 * When running in hardware assisted virtualization mode we'll be doing much the
 * same things as in raw-mode. The main difference is that we're running in the
 * host ring-0 context and that we don't get faults (\#GP(0) and \#PG) but
 * exits.
 *
 *
 * @section sec_iom_rem         Recompiled Execution Mode
 *
 * When running in the recompiler things are different. I/O port access is
 * handled by calling IOMIOPortRead and IOMIOPortWrite directly. While MMIO can
 * be handled in one of two ways. The normal way is that we have a registered a
 * special RAM range with the recompiler and in the three callbacks (for byte,
 * word and dword access) we call IOMMMIORead and IOMMMIOWrite directly. The
 * alternative ways that the physical memory access which goes via PGM will take
 * care of it by calling iomR3MmioHandler via the PGM access handler machinery
 * - this shouldn't happen but it is an alternative...
 *
 *
 * @section sec_iom_other       Other Accesses
 *
 * I/O ports aren't really exposed in any other way, unless you count the
 * instruction interpreter in EM, but that's just what we're doing in the
 * raw-mode \#GP(0) case really. Now, it's possible to call IOMIOPortRead and
 * IOMIOPortWrite directly to talk to a device, but this is really bad behavior
 * and should only be done as temporary hacks (the PC BIOS device used to setup
 * the CMOS this way back in the dark ages).
 *
 * MMIO has similar direct routes as the I/O ports and these shouldn't be used
 * for the same reasons and with the same restrictions. OTOH since MMIO is
 * mapped into the physical memory address space, it can be accessed in a number
 * of ways thru PGM.
 *
 *
 * @section sec_iom_logging     Logging Levels
 *
 * Following assignments:
 *      - Level 5 is used for defering I/O port and MMIO writes to ring-3.
 *
 */

/** @todo MMIO - simplifying the device end.
 * - Add a return status for doing DBGFSTOP on access where there are no known
 *   registers.
 * -
 *
 *   */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_IOM
#include <VBox/vmm/iom.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pgm.h>
#include <VBox/sup.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmdev.h>
#include "IOMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/err.h>

#include "IOMInline.h"


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void iomR3FlushCache(PVM pVM);


/**
 * Initializes the IOM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) IOMR3Init(PVM pVM)
{
    LogFlow(("IOMR3Init:\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, iom.s, 32);
    AssertCompile(sizeof(pVM->iom.s) <= sizeof(pVM->iom.padding));
    AssertCompileMemberAlignment(IOM, CritSect, sizeof(uintptr_t));

    /*
     * Initialize the REM critical section.
     */
#ifdef IOM_WITH_CRIT_SECT_RW
    int rc = PDMR3CritSectRwInit(pVM, &pVM->iom.s.CritSect, RT_SRC_POS, "IOM Lock");
#else
    int rc = PDMR3CritSectInit(pVM, &pVM->iom.s.CritSect, RT_SRC_POS, "IOM Lock");
#endif
    AssertRCReturn(rc, rc);

    /*
     * Allocate the trees structure.
     */
    rc = MMHyperAlloc(pVM, sizeof(*pVM->iom.s.pTreesR3), 0, MM_TAG_IOM, (void **)&pVM->iom.s.pTreesR3);
    AssertRCReturn(rc, rc);
    pVM->iom.s.pTreesR0 = MMHyperR3ToR0(pVM, pVM->iom.s.pTreesR3);

    /*
     * Register the MMIO access handler type.
     */
    rc = PGMR3HandlerPhysicalTypeRegister(pVM, PGMPHYSHANDLERKIND_MMIO,
                                          iomMmioHandler,
                                          NULL, "iomMmioHandler", "iomMmioPfHandler",
                                          NULL, "iomMmioHandler", "iomMmioPfHandler",
                                          "MMIO", &pVM->iom.s.hMmioHandlerType);
    AssertRCReturn(rc, rc);

    rc = PGMR3HandlerPhysicalTypeRegister(pVM, PGMPHYSHANDLERKIND_MMIO,
                                          iomMmioHandlerNew,
                                          NULL, "iomMmioHandlerNew", "iomMmioPfHandlerNew",
                                          NULL, "iomMmioHandlerNew", "iomMmioPfHandlerNew",
                                          "MMIO New", &pVM->iom.s.hNewMmioHandlerType);
    AssertRCReturn(rc, rc);

    /*
     * Info.
     */
    DBGFR3InfoRegisterInternal(pVM, "ioport", "Dumps all IOPort ranges. No arguments.", &iomR3IoPortInfo);
    DBGFR3InfoRegisterInternal(pVM, "mmio", "Dumps all MMIO ranges. No arguments.", &iomR3MmioInfo);

    /*
     * Statistics.
     */
    STAM_REG(pVM, &pVM->iom.s.StatIoPortCommits,      STAMTYPE_COUNTER, "/IOM/IoPortCommits",                       STAMUNIT_OCCURENCES,     "Number of ring-3 I/O port commits.");

    STAM_REL_REG(pVM, &pVM->iom.s.StatMMIOStaleMappings, STAMTYPE_PROFILE, "/IOM/MMIOStaleMappings",                STAMUNIT_TICKS_PER_CALL, "Number of times iomMmioHandlerNew got a call for a remapped range at the old mapping.");
    STAM_REG(pVM, &pVM->iom.s.StatRZMMIOHandler,      STAMTYPE_PROFILE, "/IOM/RZ-MMIOHandler",                      STAMUNIT_TICKS_PER_CALL, "Profiling of the iomMmioPfHandler() body, only success calls.");
    STAM_REG(pVM, &pVM->iom.s.StatRZMMIOReadsToR3,    STAMTYPE_COUNTER, "/IOM/RZ-MMIOHandler/ReadsToR3",            STAMUNIT_OCCURENCES,     "Number of read deferred to ring-3.");
    STAM_REG(pVM, &pVM->iom.s.StatRZMMIOWritesToR3,   STAMTYPE_COUNTER, "/IOM/RZ-MMIOHandler/WritesToR3",           STAMUNIT_OCCURENCES,     "Number of writes deferred to ring-3.");
    STAM_REG(pVM, &pVM->iom.s.StatRZMMIOCommitsToR3,  STAMTYPE_COUNTER, "/IOM/RZ-MMIOHandler/CommitsToR3",          STAMUNIT_OCCURENCES,     "Number of commits deferred to ring-3.");
    STAM_REG(pVM, &pVM->iom.s.StatRZMMIODevLockContention, STAMTYPE_COUNTER, "/IOM/RZ-MMIOHandler/DevLockContention", STAMUNIT_OCCURENCES,   "Number of device lock contention force return to ring-3.");
    STAM_REG(pVM, &pVM->iom.s.StatR3MMIOHandler,      STAMTYPE_COUNTER, "/IOM/R3-MMIOHandler",                      STAMUNIT_OCCURENCES,     "Number of calls to iomMmioHandler.");

    STAM_REG(pVM, &pVM->iom.s.StatMmioHandlerR3,      STAMTYPE_COUNTER, "/IOM/OldMmioHandlerR3",                    STAMUNIT_OCCURENCES,     "Number of calls to old iomMmioHandler from ring-3.");
    STAM_REG(pVM, &pVM->iom.s.StatMmioHandlerR0,      STAMTYPE_COUNTER, "/IOM/OldMmioHandlerR0",                    STAMUNIT_OCCURENCES,     "Number of calls to old iomMmioHandler from ring-0.");

    STAM_REG(pVM, &pVM->iom.s.StatMmioHandlerNewR3,   STAMTYPE_COUNTER, "/IOM/MmioHandlerNewR3",                    STAMUNIT_OCCURENCES,     "Number of calls to iomMmioHandlerNew from ring-3.");
    STAM_REG(pVM, &pVM->iom.s.StatMmioHandlerNewR0,   STAMTYPE_COUNTER, "/IOM/MmioHandlerNewR0",                    STAMUNIT_OCCURENCES,     "Number of calls to iomMmioHandlerNew from ring-0.");
    STAM_REG(pVM, &pVM->iom.s.StatMmioPfHandlerNew,   STAMTYPE_COUNTER, "/IOM/MmioPfHandlerNew",                    STAMUNIT_OCCURENCES,     "Number of calls to iomMmioPfHandlerNew.");
    STAM_REG(pVM, &pVM->iom.s.StatMmioPhysHandlerNew, STAMTYPE_COUNTER, "/IOM/MmioPhysHandlerNew",                  STAMUNIT_OCCURENCES,     "Number of calls to IOMR0MmioPhysHandler.");
    STAM_REG(pVM, &pVM->iom.s.StatMmioCommitsDirect,  STAMTYPE_COUNTER, "/IOM/MmioCommitsDirect",                   STAMUNIT_OCCURENCES,     "Number of ring-3 MMIO commits direct to handler via handle hint.");
    STAM_REG(pVM, &pVM->iom.s.StatMmioCommitsPgm,     STAMTYPE_COUNTER, "/IOM/MmioCommitsPgm",                      STAMUNIT_OCCURENCES,     "Number of ring-3 MMIO commits via PGM.");

    /* Redundant, but just in case we change something in the future */
    iomR3FlushCache(pVM);

    LogFlow(("IOMR3Init: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Called when a VM initialization stage is completed.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   enmWhat         The initialization state that was completed.
 */
VMMR3_INT_DECL(int) IOMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
#ifdef VBOX_WITH_STATISTICS
    if (enmWhat == VMINITCOMPLETED_RING0)
    {
        /*
         * Synchronize the ring-3 I/O port and MMIO statistics indices into the
         * ring-0 tables to simplify ring-0 code.  This also make sure that any
         * later calls to grow the statistics tables will fail.
         */
        int rc = VMMR3CallR0Emt(pVM, pVM->apCpusR3[0], VMMR0_DO_IOM_SYNC_STATS_INDICES, 0, NULL);
        AssertLogRelRCReturn(rc, rc);

        /*
         * Register I/O port and MMIO stats now that we're done registering MMIO
         * regions and won't grow the table again.
         */
        for (uint32_t i = 0; i < pVM->iom.s.cIoPortRegs; i++)
        {
            PIOMIOPORTENTRYR3 pRegEntry = &pVM->iom.s.paIoPortRegs[i];
            if (   pRegEntry->fMapped
                && pRegEntry->idxStats != UINT16_MAX)
                iomR3IoPortRegStats(pVM, pRegEntry);
        }

        for (uint32_t i = 0; i < pVM->iom.s.cMmioRegs; i++)
        {
            PIOMMMIOENTRYR3 pRegEntry = &pVM->iom.s.paMmioRegs[i];
            if (   pRegEntry->fMapped
                && pRegEntry->idxStats != UINT16_MAX)
                iomR3MmioRegStats(pVM, pRegEntry);
        }
    }
#else
    RT_NOREF(pVM, enmWhat);
#endif
    return VINF_SUCCESS;
}


/**
 * Flushes the IOM port & statistics lookup cache
 *
 * @param   pVM     The cross context VM structure.
 */
static void iomR3FlushCache(PVM pVM)
{
    /*
     * Since all relevant (1) cache use requires at least read access to the
     * critical section, we can exclude all other EMTs by grabbing exclusive
     * access to the critical section and then safely update the caches of
     * other EMTs.
     * (1) The irrelvant access not holding the lock is in assertion code.
     */
    IOM_LOCK_EXCL(pVM);
    VMCPUID idCpu = pVM->cCpus;
    while (idCpu-- > 0)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];
        pVCpu->iom.s.pMMIORangeLastR0  = NIL_RTR0PTR;
        pVCpu->iom.s.pMMIOStatsLastR0  = NIL_RTR0PTR;

        pVCpu->iom.s.pMMIORangeLastR3  = NULL;
        pVCpu->iom.s.pMMIOStatsLastR3  = NULL;
    }

    IOM_UNLOCK_EXCL(pVM);
}


/**
 * The VM is being reset.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) IOMR3Reset(PVM pVM)
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
 * @param   pVM     The cross context VM structure.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMMR3_INT_DECL(void) IOMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    RT_NOREF(pVM, offDelta);
}

/**
 * Terminates the IOM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) IOMR3Term(PVM pVM)
{
    /*
     * IOM is not owning anything but automatically freed resources,
     * so there's nothing to do here.
     */
    NOREF(pVM);
    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_STATISTICS

/**
 * Create the statistics node for an MMIO address.
 *
 * @returns Pointer to new stats node.
 *
 * @param   pVM         The cross context VM structure.
 * @param   GCPhys      The address.
 * @param   pszDesc     Description.
 */
PIOMMMIOSTATS iomR3MMIOStatsCreate(PVM pVM, RTGCPHYS GCPhys, const char *pszDesc)
{
    IOM_LOCK_EXCL(pVM);

    /* check if it already exists. */
    PIOMMMIOSTATS pStats = (PIOMMMIOSTATS)RTAvloGCPhysGet(&pVM->iom.s.pTreesR3->MmioStatTree, GCPhys);
    if (pStats)
    {
        IOM_UNLOCK_EXCL(pVM);
        return pStats;
    }

    /* allocate stats node. */
    int rc = MMHyperAlloc(pVM, sizeof(*pStats), 0, MM_TAG_IOM_STATS, (void **)&pStats);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /* insert into the tree. */
        pStats->Core.Key = GCPhys;
        if (RTAvloGCPhysInsert(&pVM->iom.s.pTreesR3->MmioStatTree, &pStats->Core))
        {
            IOM_UNLOCK_EXCL(pVM);

            rc = STAMR3RegisterF(pVM, &pStats->Accesses,    STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,     pszDesc, "/IOM/MMIO/%RGp",              GCPhys); AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfReadR3,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp/Read-R3",      GCPhys); AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfWriteR3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp/Write-R3",     GCPhys); AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfReadRZ,  STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp/Read-RZ",      GCPhys); AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ProfWriteRZ, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc, "/IOM/MMIO/%RGp/Write-RZ",     GCPhys); AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->ReadRZToR3,  STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,     pszDesc, "/IOM/MMIO/%RGp/Read-RZtoR3",  GCPhys); AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pStats->WriteRZToR3, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,     pszDesc, "/IOM/MMIO/%RGp/Write-RZtoR3", GCPhys); AssertRC(rc);

            return pStats;
        }
        AssertMsgFailed(("what! GCPhys=%RGp\n", GCPhys));
        MMHyperFree(pVM, pStats);
    }
    IOM_UNLOCK_EXCL(pVM);
    return NULL;
}

#endif /* VBOX_WITH_STATISTICS */

/**
 * Registers a Memory Mapped I/O R3 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must register ring-3 ranges
 * before any GC and R0 ranges can be registered using IOMR3MMIORegisterRC() and IOMR3MMIORegisterR0().
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   fFlags              Flags, see IOMMMIO_FLAGS_XXX.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
VMMR3_INT_DECL(int)
IOMR3MmioRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTGCPHYS cbRange, RTHCPTR pvUser,
                    R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R3PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                    R3PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, uint32_t fFlags, const char *pszDesc)
{
    LogFlow(("IOMR3MmioRegisterR3: pDevIns=%p GCPhysStart=%RGp cbRange=%RGp pvUser=%RHv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x fFlags=%#x pszDesc=%s\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback, fFlags, pszDesc));
    int rc;

    /*
     * Validate input.
     */
    AssertMsgReturn(GCPhysStart + (cbRange - 1) >= GCPhysStart,("Wrapped! %RGp LB %RGp\n", GCPhysStart, cbRange),
                    VERR_IOM_INVALID_MMIO_RANGE);
    AssertMsgReturn(   !(fFlags & ~(IOMMMIO_FLAGS_VALID_MASK & ~IOMMMIO_FLAGS_ABS))
                    && (fFlags & IOMMMIO_FLAGS_READ_MODE)  <= IOMMMIO_FLAGS_READ_DWORD_QWORD
                    && (fFlags & IOMMMIO_FLAGS_WRITE_MODE) <= IOMMMIO_FLAGS_WRITE_ONLY_DWORD_QWORD,
                    ("%#x\n", fFlags),
                    VERR_INVALID_PARAMETER);

    /*
     * Allocate new range record and initialize it.
     */
    PIOMMMIORANGE pRange;
    rc = MMHyperAlloc(pVM, sizeof(*pRange), 0, MM_TAG_IOM, (void **)&pRange);
    if (RT_SUCCESS(rc))
    {
        pRange->Core.Key            = GCPhysStart;
        pRange->Core.KeyLast        = GCPhysStart + (cbRange - 1);
        pRange->GCPhys              = GCPhysStart;
        pRange->cb                  = cbRange;
        pRange->cRefs               = 1; /* The tree reference. */
        pRange->pszDesc             = pszDesc;

        //pRange->pvUserR0            = NIL_RTR0PTR;
        //pRange->pDevInsR0           = NIL_RTR0PTR;
        //pRange->pfnReadCallbackR0   = NIL_RTR0PTR;
        //pRange->pfnWriteCallbackR0  = NIL_RTR0PTR;
        //pRange->pfnFillCallbackR0   = NIL_RTR0PTR;

        //pRange->pvUserRC            = NIL_RTRCPTR;
        //pRange->pDevInsRC           = NIL_RTRCPTR;
        //pRange->pfnReadCallbackRC   = NIL_RTRCPTR;
        //pRange->pfnWriteCallbackRC  = NIL_RTRCPTR;
        //pRange->pfnFillCallbackRC   = NIL_RTRCPTR;

        pRange->fFlags              = fFlags;

        pRange->pvUserR3            = pvUser;
        pRange->pDevInsR3           = pDevIns;
        pRange->pfnReadCallbackR3   = pfnReadCallback;
        pRange->pfnWriteCallbackR3  = pfnWriteCallback;
        pRange->pfnFillCallbackR3   = pfnFillCallback;

        /*
         * Try register it with PGM and then insert it into the tree.
         */
        rc = PGMR3PhysMMIORegister(pVM, GCPhysStart, cbRange, pVM->iom.s.hMmioHandlerType,
                                   pRange, MMHyperR3ToR0(pVM, pRange), MMHyperR3ToRC(pVM, pRange), pszDesc);
        if (RT_SUCCESS(rc))
        {
            IOM_LOCK_EXCL(pVM);
            if (RTAvlroGCPhysInsert(&pVM->iom.s.pTreesR3->MMIOTree, &pRange->Core))
            {
                iomR3FlushCache(pVM);
                IOM_UNLOCK_EXCL(pVM);
                return VINF_SUCCESS;
            }

            /* bail out */
            IOM_UNLOCK_EXCL(pVM);
            DBGFR3Info(pVM->pUVM, "mmio", NULL, NULL);
            AssertMsgFailed(("This cannot happen!\n"));
            rc = VERR_IOM_IOPORT_IPE_3;
        }

        MMHyperFree(pVM, pRange);
    }
    if (pDevIns->iInstance > 0)
        MMR3HeapFree((void *)pszDesc);
    return rc;
}


#if 0
/**
 * Registers a Memory Mapped I/O RC handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMMIORegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @thread  EMT
 */
VMMR3_INT_DECL(int)
IOMR3MmioRegisterRC(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTGCPHYS cbRange, RTGCPTR pvUser,
                    RCPTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, RCPTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                    RCPTRTYPE(PFNIOMMMIOFILL) pfnFillCallback)
{
    LogFlow(("IOMR3MmioRegisterRC: pDevIns=%p GCPhysStart=%RGp cbRange=%RGp pvUser=%RGv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback));
    AssertReturn(VM_IS_RAW_MODE_ENABLED(pVM), VERR_IOM_HM_IPE);

    /*
     * Validate input.
     */
    if (!pfnWriteCallback && !pfnReadCallback)
    {
        AssertMsgFailed(("No callbacks! %RGp LB %RGp\n", GCPhysStart, cbRange));
        return VERR_INVALID_PARAMETER;
    }
    PVMCPU pVCpu = VMMGetCpu(pVM); Assert(pVCpu);

    /*
     * Find the MMIO range and check that the input matches.
     */
    IOM_LOCK_EXCL(pVM);
    PIOMMMIORANGE pRange = iomMmioGetRange(pVM, pVCpu, GCPhysStart);
    AssertReturnStmt(pRange, IOM_UNLOCK_EXCL(pVM), VERR_IOM_MMIO_RANGE_NOT_FOUND);
    AssertReturnStmt(pRange->pDevInsR3 == pDevIns, IOM_UNLOCK_EXCL(pVM), VERR_IOM_NOT_MMIO_RANGE_OWNER);
    AssertReturnStmt(pRange->GCPhys == GCPhysStart, IOM_UNLOCK_EXCL(pVM), VERR_IOM_INVALID_MMIO_RANGE);
    AssertReturnStmt(pRange->cb == cbRange, IOM_UNLOCK_EXCL(pVM), VERR_IOM_INVALID_MMIO_RANGE);

    pRange->pvUserRC          = pvUser;
    pRange->pfnReadCallbackRC = pfnReadCallback;
    pRange->pfnWriteCallbackRC= pfnWriteCallback;
    pRange->pfnFillCallbackRC = pfnFillCallback;
    pRange->pDevInsRC         = pDevIns->pDevInsForRC;
    IOM_UNLOCK_EXCL(pVM);

    return VINF_SUCCESS;
}
#endif


/**
 * Registers a Memory Mapped I/O R0 handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMR3MIORegisterHC() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @thread  EMT
 */
VMMR3_INT_DECL(int)
IOMR3MmioRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTGCPHYS cbRange, RTR0PTR pvUser,
                    R0PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                    R0PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                    R0PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback)
{
    LogFlow(("IOMR3MmioRegisterR0: pDevIns=%p GCPhysStart=%RGp cbRange=%RGp pvUser=%RHv pfnWriteCallback=%#x pfnReadCallback=%#x pfnFillCallback=%#x\n",
             pDevIns, GCPhysStart, cbRange, pvUser, pfnWriteCallback, pfnReadCallback, pfnFillCallback));

    /*
     * Validate input.
     */
    if (!pfnWriteCallback && !pfnReadCallback)
    {
        AssertMsgFailed(("No callbacks! %RGp LB %RGp\n", GCPhysStart, cbRange));
        return VERR_INVALID_PARAMETER;
    }
    PVMCPU pVCpu = VMMGetCpu(pVM); Assert(pVCpu);

    /*
     * Find the MMIO range and check that the input matches.
     */
    IOM_LOCK_EXCL(pVM);
    PIOMMMIORANGE pRange = iomMmioGetRange(pVM, pVCpu, GCPhysStart);
    AssertReturnStmt(pRange, IOM_UNLOCK_EXCL(pVM), VERR_IOM_MMIO_RANGE_NOT_FOUND);
    AssertReturnStmt(pRange->pDevInsR3 == pDevIns, IOM_UNLOCK_EXCL(pVM), VERR_IOM_NOT_MMIO_RANGE_OWNER);
    AssertReturnStmt(pRange->GCPhys == GCPhysStart, IOM_UNLOCK_EXCL(pVM), VERR_IOM_INVALID_MMIO_RANGE);
    AssertReturnStmt(pRange->cb == cbRange, IOM_UNLOCK_EXCL(pVM), VERR_IOM_INVALID_MMIO_RANGE);

    pRange->pvUserR0          = pvUser;
    pRange->pfnReadCallbackR0 = pfnReadCallback;
    pRange->pfnWriteCallbackR0= pfnWriteCallback;
    pRange->pfnFillCallbackR0 = pfnFillCallback;
    pRange->pDevInsR0         = pDevIns->pDevInsR0RemoveMe;
    IOM_UNLOCK_EXCL(pVM);

    return VINF_SUCCESS;
}


/**
 * Deregisters a Memory Mapped I/O handler range.
 *
 * Registered GC, R0, and R3 ranges are affected.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pDevIns             Device instance which the MMIO region is registered.
 * @param   GCPhysStart         First physical address (GC) in the range.
 * @param   cbRange             Number of bytes to deregister.
 *
 * @remark  This function mainly for PCI PnP Config and will not do
 *          all the checks you might expect it to do.
 */
VMMR3_INT_DECL(int) IOMR3MmioDeregister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTGCPHYS cbRange)
{
    LogFlow(("IOMR3MmioDeregister: pDevIns=%p GCPhysStart=%RGp cbRange=%RGp\n", pDevIns, GCPhysStart, cbRange));

    /*
     * Validate input.
     */
    RTGCPHYS GCPhysLast = GCPhysStart + (cbRange - 1);
    if (GCPhysLast < GCPhysStart)
    {
        AssertMsgFailed(("Wrapped! %#x LB %RGp\n", GCPhysStart, cbRange));
        return VERR_IOM_INVALID_MMIO_RANGE;
    }
    PVMCPU pVCpu = VMMGetCpu(pVM); Assert(pVCpu);

    IOM_LOCK_EXCL(pVM);

    /*
     * Check ownership and such for the entire area.
     */
    RTGCPHYS GCPhys = GCPhysStart;
    while (GCPhys <= GCPhysLast && GCPhys >= GCPhysStart)
    {
        PIOMMMIORANGE pRange = iomMmioGetRange(pVM, pVCpu, GCPhys);
        if (!pRange)
        {
            IOM_UNLOCK_EXCL(pVM);
            return VERR_IOM_MMIO_RANGE_NOT_FOUND;
        }
        AssertMsgReturnStmt(pRange->pDevInsR3 == pDevIns,
                            ("Not owner! GCPhys=%RGp %RGp LB %RGp %s\n", GCPhys, GCPhysStart, cbRange, pRange->pszDesc),
                            IOM_UNLOCK_EXCL(pVM),
                            VERR_IOM_NOT_MMIO_RANGE_OWNER);
        AssertMsgReturnStmt(pRange->Core.KeyLast <= GCPhysLast,
                            ("Incomplete R3 range! GCPhys=%RGp %RGp LB %RGp %s\n", GCPhys, GCPhysStart, cbRange, pRange->pszDesc),
                            IOM_UNLOCK_EXCL(pVM),
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
        iomR3FlushCache(pVM);

        PIOMMMIORANGE pRange = (PIOMMMIORANGE)RTAvlroGCPhysRemove(&pVM->iom.s.pTreesR3->MMIOTree, GCPhys);
        Assert(pRange);
        Assert(pRange->Core.Key == GCPhys && pRange->Core.KeyLast <= GCPhysLast);
        IOM_UNLOCK_EXCL(pVM); /* Lock order fun. */

        /* remove it from PGM */
        int rc = PGMR3PhysMMIODeregister(pVM, GCPhys, pRange->cb);
        AssertRC(rc);

        IOM_LOCK_EXCL(pVM);

        /* advance and free. */
        GCPhys = pRange->Core.KeyLast + 1;
        if (pDevIns->iInstance > 0)
        {
            void *pvDesc = ASMAtomicXchgPtr((void * volatile *)&pRange->pszDesc, NULL);
            MMR3HeapFree(pvDesc);
        }
        iomMmioReleaseRange(pVM, pRange);
    }

    IOM_UNLOCK_EXCL(pVM);
    return VINF_SUCCESS;
}


/**
 * Notfication from PGM that the pre-registered MMIO region has been mapped into
 * user address space.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   pvUser          The pvUserR3 argument of PGMR3PhysMMIOExPreRegister.
 * @param   GCPhys          The mapping address.
 * @remarks Called while owning the PGM lock.
 */
VMMR3_INT_DECL(int) IOMR3MmioExNotifyMapped(PVM pVM, void *pvUser, RTGCPHYS GCPhys)
{
    PIOMMMIORANGE pRange = (PIOMMMIORANGE)pvUser;
    AssertReturn(pRange->GCPhys == NIL_RTGCPHYS, VERR_IOM_MMIO_IPE_1);

    IOM_LOCK_EXCL(pVM);
    Assert(pRange->GCPhys == NIL_RTGCPHYS);
    pRange->GCPhys       = GCPhys;
    pRange->Core.Key     = GCPhys;
    pRange->Core.KeyLast = GCPhys + pRange->cb - 1;
    if (RTAvlroGCPhysInsert(&pVM->iom.s.pTreesR3->MMIOTree, &pRange->Core))
    {
        iomR3FlushCache(pVM);
        IOM_UNLOCK_EXCL(pVM);
        return VINF_SUCCESS;
    }
    IOM_UNLOCK_EXCL(pVM);

    AssertLogRelMsgFailed(("RTAvlroGCPhysInsert failed on %RGp..%RGp - %s\n", pRange->Core.Key, pRange->Core.KeyLast, pRange->pszDesc));
    pRange->GCPhys       = NIL_RTGCPHYS;
    pRange->Core.Key     = NIL_RTGCPHYS;
    pRange->Core.KeyLast = NIL_RTGCPHYS;
    return VERR_IOM_MMIO_IPE_2;
}


/**
 * Notfication from PGM that the pre-registered MMIO region has been unmapped
 * from user address space.
 *
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   pvUser          The pvUserR3 argument of PGMR3PhysMMIOExPreRegister.
 * @param   GCPhys          The mapping address.
 * @remarks Called while owning the PGM lock.
 */
VMMR3_INT_DECL(void) IOMR3MmioExNotifyUnmapped(PVM pVM, void *pvUser, RTGCPHYS GCPhys)
{
    PIOMMMIORANGE pRange = (PIOMMMIORANGE)pvUser;
    AssertLogRelReturnVoid(pRange->GCPhys == GCPhys);

    IOM_LOCK_EXCL(pVM);
    Assert(pRange->GCPhys == GCPhys);
    PIOMMMIORANGE pRemoved = (PIOMMMIORANGE)RTAvlroGCPhysRemove(&pVM->iom.s.pTreesR3->MMIOTree, GCPhys);
    if (pRemoved == pRange)
    {
        pRange->GCPhys       = NIL_RTGCPHYS;
        pRange->Core.Key     = NIL_RTGCPHYS;
        pRange->Core.KeyLast = NIL_RTGCPHYS;
        iomR3FlushCache(pVM);
        IOM_UNLOCK_EXCL(pVM);
    }
    else
    {
        if (pRemoved)
            RTAvlroGCPhysInsert(&pVM->iom.s.pTreesR3->MMIOTree, &pRemoved->Core);
        IOM_UNLOCK_EXCL(pVM);
        AssertLogRelMsgFailed(("RTAvlroGCPhysRemove returned %p instead of %p for %RGp (%s)\n",
                               pRemoved, pRange, GCPhys, pRange->pszDesc));
    }
}


/**
 * Notfication from PGM that the pre-registered MMIO region has been mapped into
 * user address space.
 *
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   pvUser          The pvUserR3 argument of PGMR3PhysMMIOExPreRegister.
 * @remarks Called while owning the PGM lock.
 */
VMMR3_INT_DECL(void) IOMR3MmioExNotifyDeregistered(PVM pVM, void *pvUser)
{
    PIOMMMIORANGE pRange = (PIOMMMIORANGE)pvUser;
    AssertLogRelReturnVoid(pRange->GCPhys == NIL_RTGCPHYS);
    iomMmioReleaseRange(pVM, pRange);
}


/**
 * Handles the unlikely and probably fatal merge cases.
 *
 * @returns Merged status code.
 * @param   rcStrict        Current EM status code.
 * @param   rcStrictCommit  The IOM I/O or MMIO write commit status to merge
 *                          with @a rcStrict.
 * @param   rcIom           For logging purposes only.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.  For logging purposes.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iomR3MergeStatusSlow(VBOXSTRICTRC rcStrict, VBOXSTRICTRC rcStrictCommit,
                                                          int rcIom, PVMCPU pVCpu)
{
    if (RT_FAILURE_NP(rcStrict))
        return rcStrict;

    if (RT_FAILURE_NP(rcStrictCommit))
        return rcStrictCommit;

    if (rcStrict == rcStrictCommit)
        return rcStrictCommit;

    AssertLogRelMsgFailed(("rcStrictCommit=%Rrc rcStrict=%Rrc IOPort={%#06x<-%#xx/%u} MMIO={%RGp<-%.*Rhxs} (rcIom=%Rrc)\n",
                           VBOXSTRICTRC_VAL(rcStrictCommit), VBOXSTRICTRC_VAL(rcStrict),
                           pVCpu->iom.s.PendingIOPortWrite.IOPort,
                           pVCpu->iom.s.PendingIOPortWrite.u32Value, pVCpu->iom.s.PendingIOPortWrite.cbValue,
                           pVCpu->iom.s.PendingMmioWrite.GCPhys,
                           pVCpu->iom.s.PendingMmioWrite.cbValue, &pVCpu->iom.s.PendingMmioWrite.abValue[0], rcIom));
    return VERR_IOM_FF_STATUS_IPE;
}


/**
 * Helper for IOMR3ProcessForceFlag.
 *
 * @returns Merged status code.
 * @param   rcStrict        Current EM status code.
 * @param   rcStrictCommit  The IOM I/O or MMIO write commit status to merge
 *                          with @a rcStrict.
 * @param   rcIom           Either VINF_IOM_R3_IOPORT_COMMIT_WRITE or
 *                          VINF_IOM_R3_MMIO_COMMIT_WRITE.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 */
DECLINLINE(VBOXSTRICTRC) iomR3MergeStatus(VBOXSTRICTRC rcStrict, VBOXSTRICTRC rcStrictCommit, int rcIom, PVMCPU pVCpu)
{
    /* Simple. */
    if (RT_LIKELY(rcStrict == rcIom || rcStrict == VINF_EM_RAW_TO_R3 || rcStrict == VINF_SUCCESS))
        return rcStrictCommit;

    if (RT_LIKELY(rcStrictCommit == VINF_SUCCESS))
        return rcStrict;

    /* EM scheduling status codes. */
    if (RT_LIKELY(   rcStrict >= VINF_EM_FIRST
                  && rcStrict <= VINF_EM_LAST))
    {
        if (RT_LIKELY(   rcStrictCommit >= VINF_EM_FIRST
                      && rcStrictCommit <= VINF_EM_LAST))
            return rcStrict < rcStrictCommit ? rcStrict : rcStrictCommit;
    }

    /* Unlikely */
    return iomR3MergeStatusSlow(rcStrict, rcStrictCommit, rcIom, pVCpu);
}


/**
 * Called by force-flag handling code when VMCPU_FF_IOM is set.
 *
 * @returns Merge between @a rcStrict and what the commit operation returned.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   rcStrict    The status code returned by ring-0 or raw-mode.
 * @thread  EMT(pVCpu)
 *
 * @remarks The VMCPU_FF_IOM flag is handled before the status codes by EM, so
 *          we're very likely to see @a rcStrict set to
 *          VINF_IOM_R3_IOPORT_COMMIT_WRITE and VINF_IOM_R3_MMIO_COMMIT_WRITE
 *          here.
 */
VMMR3_INT_DECL(VBOXSTRICTRC) IOMR3ProcessForceFlag(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rcStrict)
{
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_IOM);
    Assert(pVCpu->iom.s.PendingIOPortWrite.cbValue || pVCpu->iom.s.PendingMmioWrite.cbValue);

    if (pVCpu->iom.s.PendingIOPortWrite.cbValue)
    {
        Log5(("IOM: Dispatching pending I/O port write: %#x LB %u -> %RTiop\n", pVCpu->iom.s.PendingIOPortWrite.u32Value,
              pVCpu->iom.s.PendingIOPortWrite.cbValue, pVCpu->iom.s.PendingIOPortWrite.IOPort));
        STAM_COUNTER_INC(&pVM->iom.s.StatIoPortCommits);
        VBOXSTRICTRC rcStrictCommit = IOMIOPortWrite(pVM, pVCpu, pVCpu->iom.s.PendingIOPortWrite.IOPort,
                                                     pVCpu->iom.s.PendingIOPortWrite.u32Value,
                                                     pVCpu->iom.s.PendingIOPortWrite.cbValue);
        pVCpu->iom.s.PendingIOPortWrite.cbValue = 0;
        rcStrict = iomR3MergeStatus(rcStrict, rcStrictCommit, VINF_IOM_R3_IOPORT_COMMIT_WRITE, pVCpu);
    }


    if (pVCpu->iom.s.PendingMmioWrite.cbValue)
    {
        Log5(("IOM: Dispatching pending MMIO write: %RGp LB %#x\n",
              pVCpu->iom.s.PendingMmioWrite.GCPhys, pVCpu->iom.s.PendingMmioWrite.cbValue));

        /* Use new MMIO handle hint and bypass PGM if it still looks right. */
        size_t idxMmioRegionHint = pVCpu->iom.s.PendingMmioWrite.idxMmioRegionHint;
        if (idxMmioRegionHint < pVM->iom.s.cMmioRegs)
        {
            PIOMMMIOENTRYR3 pRegEntry    = &pVM->iom.s.paMmioRegs[idxMmioRegionHint];
            RTGCPHYS const GCPhysMapping = pRegEntry->GCPhysMapping;
            RTGCPHYS const offRegion     = pVCpu->iom.s.PendingMmioWrite.GCPhys - GCPhysMapping;
            if (offRegion < pRegEntry->cbRegion && GCPhysMapping != NIL_RTGCPHYS)
            {
                STAM_COUNTER_INC(&pVM->iom.s.StatMmioCommitsDirect);
                VBOXSTRICTRC rcStrictCommit = iomR3MmioCommitWorker(pVM, pVCpu, pRegEntry, offRegion);
                pVCpu->iom.s.PendingMmioWrite.cbValue = 0;
                return iomR3MergeStatus(rcStrict, rcStrictCommit, VINF_IOM_R3_MMIO_COMMIT_WRITE, pVCpu);
            }
        }

        /* Fall back on PGM. */
        STAM_COUNTER_INC(&pVM->iom.s.StatMmioCommitsPgm);
        VBOXSTRICTRC rcStrictCommit = PGMPhysWrite(pVM, pVCpu->iom.s.PendingMmioWrite.GCPhys,
                                                   pVCpu->iom.s.PendingMmioWrite.abValue, pVCpu->iom.s.PendingMmioWrite.cbValue,
                                                   PGMACCESSORIGIN_IOM);
        pVCpu->iom.s.PendingMmioWrite.cbValue = 0;
        rcStrict = iomR3MergeStatus(rcStrict, rcStrictCommit, VINF_IOM_R3_MMIO_COMMIT_WRITE, pVCpu);
    }

    return rcStrict;
}


/**
 * Notification from DBGF that the number of active I/O port or MMIO
 * breakpoints has change.
 *
 * For performance reasons, IOM will only call DBGF before doing I/O and MMIO
 * accesses where there are armed breakpoints.
 *
 * @param   pVM         The cross context VM structure.
 * @param   fPortIo     True if there are armed I/O port breakpoints.
 * @param   fMmio       True if there are armed MMIO breakpoints.
 */
VMMR3_INT_DECL(void) IOMR3NotifyBreakpointCountChange(PVM pVM, bool fPortIo, bool fMmio)
{
    /** @todo I/O breakpoints. */
    RT_NOREF3(pVM, fPortIo, fMmio);
}


/**
 * Notification from DBGF that an event has been enabled or disabled.
 *
 * For performance reasons, IOM may cache the state of events it implements.
 *
 * @param   pVM         The cross context VM structure.
 * @param   enmEvent    The event.
 * @param   fEnabled    The new state.
 */
VMMR3_INT_DECL(void) IOMR3NotifyDebugEventChange(PVM pVM, DBGFEVENT enmEvent, bool fEnabled)
{
    /** @todo IOM debug events. */
    RT_NOREF3(pVM, enmEvent, fEnabled);
}

