/* $Id$ */
/** @file
 * CSAM - Guest OS Code Scanning and Analysis Manager
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
#define LOG_GROUP LOG_GROUP_CSAM
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/cpumdis.h>
#include <VBox/pgm.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/rem.h>
#include <VBox/selm.h>
#include <VBox/trpm.h>
#include <VBox/cfgm.h>
#include <VBox/param.h>
#include <iprt/avl.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include "CSAMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/ssm.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <stdlib.h>
#include <stdio.h>


/* Enabled by default */
#define CSAM_ENABLE

/* Enable to monitor code pages for self-modifying code. */
#define CSAM_MONITOR_CODE_PAGES
/* Enable to monitor all scanned pages
#define CSAM_MONITOR_CSAM_CODE_PAGES */
/* Enable to scan beyond ret instructions.  
#define CSAM_ANALYSE_BEYOND_RET */

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) csamr3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) csamr3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(int) CSAMCodePageWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
static DECLCALLBACK(int) CSAMCodePageInvalidate(PVM pVM, RTGCPTR GCPtr);

bool                csamIsCodeScanned(PVM pVM, RTGCPTR pInstr, PCSAMPAGE *pPage);
int                 csamR3CheckPageRecord(PVM pVM, RTGCPTR pInstr);
static PCSAMPAGE    csamCreatePageRecord(PVM pVM, RTGCPTR GCPtr, CSAMTAG enmTag, bool fCode32, bool fMonitorInvalidation = false);
static int          csamRemovePageRecord(PVM pVM, RTGCPTR GCPtr);
static int          csamReinit(PVM pVM);
static void         csamMarkCode(PVM pVM, PCSAMPAGE pPage, RTGCPTR pInstr, uint32_t opsize, bool fScanned);
static int          csamAnalyseCodeStream(PVM pVM, GCPTRTYPE(uint8_t *) pInstrGC, GCPTRTYPE(uint8_t *) pCurInstrGC, bool fCode32,
                                          PFN_CSAMR3ANALYSE pfnCSAMR3Analyse, void *pUserData, PCSAMP2GLOOKUPREC pCacheRec);

/** @todo Temporary for debugging. */
static bool fInCSAMCodePageInvalidate = false;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_DEBUGGER
static DECLCALLBACK(int) csamr3CmdOn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) csamr3CmdOff(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);

/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDesc,          cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "csamon",     0,        0,        NULL,               0,                          NULL,               0,          csamr3CmdOn,        "",                     "Enable CSAM code scanning." },
    { "csamoff",    0,        0,        NULL,               0,                          NULL,               0,          csamr3CmdOff,       "",                     "Disable CSAM code scanning." },
};
#endif


/**
 * Initializes the CSAM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CSAMR3DECL(int) CSAMR3Init(PVM pVM)
{
    int rc;

    LogFlow(("CSAMR3Init\n"));

    /* Allocate bitmap for the page directory. */
    rc = MMR3HyperAllocOnceNoRel(pVM, CSAM_PGDIRBMP_CHUNKS*sizeof(RTHCPTR), 0, MM_TAG_CSAM, (void **)&pVM->csam.s.pPDBitmapHC);
    AssertRCReturn(rc, rc);
    rc = MMR3HyperAllocOnceNoRel(pVM, CSAM_PGDIRBMP_CHUNKS*sizeof(RTGCPTR), 0, MM_TAG_CSAM, (void **)&pVM->csam.s.pPDGCBitmapHC);
    AssertRCReturn(rc, rc);
    pVM->csam.s.pPDBitmapGC   = MMHyperHC2GC(pVM, pVM->csam.s.pPDGCBitmapHC);
    pVM->csam.s.pPDHCBitmapGC = MMHyperHC2GC(pVM, pVM->csam.s.pPDBitmapHC);

    rc = csamReinit(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Register save and load state notificators.
     */
    rc = SSMR3RegisterInternal(pVM, "CSAM", 0, CSAM_SSM_VERSION, sizeof(pVM->csam.s) + PAGE_SIZE*16,
                               NULL, csamr3Save, NULL,
                               NULL, csamr3Load, NULL);
    AssertRCReturn(rc, rc);

    STAM_REG(pVM, &pVM->csam.s.StatNrTraps,          STAMTYPE_COUNTER,   "/CSAM/PageTraps",           STAMUNIT_OCCURENCES,     "The number of CSAM page traps.");
    STAM_REG(pVM, &pVM->csam.s.StatDangerousWrite,   STAMTYPE_COUNTER,   "/CSAM/DangerousWrites",     STAMUNIT_OCCURENCES,     "The number of dangerous writes that cause a context switch.");

    STAM_REG(pVM, &pVM->csam.s.StatNrPageNPHC,       STAMTYPE_COUNTER,   "/CSAM/HC/PageNotPresent",   STAMUNIT_OCCURENCES,     "The number of CSAM pages marked not present.");
    STAM_REG(pVM, &pVM->csam.s.StatNrPageNPGC,       STAMTYPE_COUNTER,   "/CSAM/GC/PageNotPresent",   STAMUNIT_OCCURENCES,     "The number of CSAM pages marked not present.");
    STAM_REG(pVM, &pVM->csam.s.StatNrPages,          STAMTYPE_COUNTER,   "/CSAM/PageRec/AddedRW",     STAMUNIT_OCCURENCES,     "The number of CSAM page records (RW monitoring).");
    STAM_REG(pVM, &pVM->csam.s.StatNrPagesInv,       STAMTYPE_COUNTER,   "/CSAM/PageRec/AddedRWI",    STAMUNIT_OCCURENCES,     "The number of CSAM page records (RW & invalidation monitoring).");
    STAM_REG(pVM, &pVM->csam.s.StatNrRemovedPages,   STAMTYPE_COUNTER,   "/CSAM/PageRec/Removed",     STAMUNIT_OCCURENCES,     "The number of removed CSAM page records.");
    STAM_REG(pVM, &pVM->csam.s.StatPageRemoveREMFlush,STAMTYPE_COUNTER,  "/CSAM/PageRec/Removed/REMFlush",     STAMUNIT_OCCURENCES,     "The number of removed CSAM page records that caused a REM flush.");

    STAM_REG(pVM, &pVM->csam.s.StatNrPatchPages,     STAMTYPE_COUNTER,   "/CSAM/PageRec/Patch",       STAMUNIT_OCCURENCES,     "The number of CSAM patch page records.");
    STAM_REG(pVM, &pVM->csam.s.StatNrUserPages,      STAMTYPE_COUNTER,   "/CSAM/PageRec/Ignore/User", STAMUNIT_OCCURENCES,     "The number of CSAM user page records (ignored).");
    STAM_REG(pVM, &pVM->csam.s.StatPagePATM,         STAMTYPE_COUNTER,   "/CSAM/PageRec/Type/PATM",   STAMUNIT_OCCURENCES,     "The number of PATM page records.");
    STAM_REG(pVM, &pVM->csam.s.StatPageCSAM,         STAMTYPE_COUNTER,   "/CSAM/PageRec/Type/CSAM",   STAMUNIT_OCCURENCES,     "The number of CSAM page records.");
    STAM_REG(pVM, &pVM->csam.s.StatPageREM,          STAMTYPE_COUNTER,   "/CSAM/PageRec/Type/REM",    STAMUNIT_OCCURENCES,     "The number of REM page records.");
    STAM_REG(pVM, &pVM->csam.s.StatPageMonitor,      STAMTYPE_COUNTER,   "/CSAM/PageRec/Monitored",   STAMUNIT_OCCURENCES,     "The number of monitored pages.");

    STAM_REG(pVM, &pVM->csam.s.StatCodePageModified, STAMTYPE_COUNTER, "/CSAM/Monitor/DirtyPage",     STAMUNIT_OCCURENCES,     "The number of code page modifications.");

    STAM_REG(pVM, &pVM->csam.s.StatNrFlushes,        STAMTYPE_COUNTER, "/CSAM/PageFlushes",           STAMUNIT_OCCURENCES,     "The number of CSAM page flushes.");
    STAM_REG(pVM, &pVM->csam.s.StatNrFlushesSkipped, STAMTYPE_COUNTER, "/CSAM/PageFlushesSkipped",    STAMUNIT_OCCURENCES,     "The number of CSAM page flushes that were skipped.");
    STAM_REG(pVM, &pVM->csam.s.StatNrKnownPagesHC,   STAMTYPE_COUNTER, "/CSAM/HC/KnownPageRecords",   STAMUNIT_OCCURENCES,     "The number of known CSAM page records.");
    STAM_REG(pVM, &pVM->csam.s.StatNrKnownPagesGC,   STAMTYPE_COUNTER, "/CSAM/GC/KnownPageRecords",   STAMUNIT_OCCURENCES,     "The number of known CSAM page records.");
    STAM_REG(pVM, &pVM->csam.s.StatNrInstr,          STAMTYPE_COUNTER, "/CSAM/ScannedInstr",          STAMUNIT_OCCURENCES,     "The number of scanned instructions.");
    STAM_REG(pVM, &pVM->csam.s.StatNrBytesRead,      STAMTYPE_COUNTER, "/CSAM/BytesRead",             STAMUNIT_OCCURENCES,     "The number of bytes read for scanning.");
    STAM_REG(pVM, &pVM->csam.s.StatNrOpcodeRead,     STAMTYPE_COUNTER, "/CSAM/OpcodeBytesRead",       STAMUNIT_OCCURENCES,     "The number of opcode bytes read by the recompiler.");

    STAM_REG(pVM, &pVM->csam.s.StatBitmapAlloc,      STAMTYPE_COUNTER, "/CSAM/Alloc/PageBitmap",      STAMUNIT_OCCURENCES,     "The number of page bitmap allocations.");

    STAM_REG(pVM, &pVM->csam.s.StatInstrCacheHit,    STAMTYPE_COUNTER, "/CSAM/Cache/Hit",             STAMUNIT_OCCURENCES,     "The number of dangerous instruction cache hits.");
    STAM_REG(pVM, &pVM->csam.s.StatInstrCacheMiss,   STAMTYPE_COUNTER, "/CSAM/Cache/Miss",            STAMUNIT_OCCURENCES,     "The number of dangerous instruction cache misses.");

    STAM_REG(pVM, &pVM->csam.s.StatScanNextFunction,         STAMTYPE_COUNTER, "/CSAM/Function/Scan/Success",  STAMUNIT_OCCURENCES,     "The number of found functions beyond the ret border.");
    STAM_REG(pVM, &pVM->csam.s.StatScanNextFunctionFailed,   STAMTYPE_COUNTER, "/CSAM/Function/Scan/Failed",   STAMUNIT_OCCURENCES,     "The number of refused functions beyond the ret border.");

    STAM_REG(pVM, &pVM->csam.s.StatTime,             STAMTYPE_PROFILE, "/PROF/CSAM/Scan",             STAMUNIT_TICKS_PER_CALL, "Scanning overhead.");
    STAM_REG(pVM, &pVM->csam.s.StatTimeCheckAddr,    STAMTYPE_PROFILE, "/PROF/CSAM/CheckAddr",        STAMUNIT_TICKS_PER_CALL, "Address check overhead.");
    STAM_REG(pVM, &pVM->csam.s.StatTimeAddrConv,     STAMTYPE_PROFILE, "/PROF/CSAM/AddrConv",         STAMUNIT_TICKS_PER_CALL, "Address conversion overhead.");
    STAM_REG(pVM, &pVM->csam.s.StatTimeFlushPage,    STAMTYPE_PROFILE, "/PROF/CSAM/FlushPage",        STAMUNIT_TICKS_PER_CALL, "Page flushing overhead.");
    STAM_REG(pVM, &pVM->csam.s.StatTimeDisasm,       STAMTYPE_PROFILE, "/PROF/CSAM/Disasm",           STAMUNIT_TICKS_PER_CALL, "Disassembly overhead.");
    STAM_REG(pVM, &pVM->csam.s.StatFlushDirtyPages,  STAMTYPE_PROFILE, "/PROF/CSAM/FlushDirtyPage",   STAMUNIT_TICKS_PER_CALL, "Dirty page flushing overhead.");
    STAM_REG(pVM, &pVM->csam.s.StatCheckGates,       STAMTYPE_PROFILE, "/PROF/CSAM/CheckGates",       STAMUNIT_TICKS_PER_CALL, "CSAMR3CheckGates overhead.");

    /* 
     * Check CFGM option and enable/disable CSAM.
     */
    bool fEnabled;
    rc = CFGMR3QueryBool(CFGMR3GetRoot(pVM), "CSAMEnabled", &fEnabled);
    if (VBOX_FAILURE(rc))
#ifdef CSAM_ENABLE
        fEnabled = true;
#else
        fEnabled = false;
#endif
    if (fEnabled)
        CSAMEnableScanning(pVM);

#ifdef VBOX_WITH_DEBUGGER
    /*
     * Debugger commands.
     */
    static bool fRegisteredCmds = false;
    if (!fRegisteredCmds)
    {
        int rc = DBGCRegisterCommands(&g_aCmds[0], ELEMENTS(g_aCmds));
        if (VBOX_SUCCESS(rc))
            fRegisteredCmds = true;
    }
#endif

    return VINF_SUCCESS;
}

/**
 * (Re)initializes CSAM
 *
 * @param   pVM     The VM.
 */
static int csamReinit(PVM pVM)
{
    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, csam.s) & 31));
    AssertRelease(sizeof(pVM->csam.s) <= sizeof(pVM->csam.padding));

    /*
     * Setup any fixed pointers and offsets.
     */
    pVM->csam.s.offVM = RT_OFFSETOF(VM, patm);

    pVM->csam.s.fGatesChecked    = false;
    pVM->csam.s.fScanningStarted = false;

    VM_FF_CLEAR(pVM, VM_FF_CSAM_PENDING_ACTION);
    pVM->csam.s.cDirtyPages = 0;
    /* not necessary */
    memset(pVM->csam.s.pvDirtyBasePage, 0, sizeof(pVM->csam.s.pvDirtyBasePage));
    memset(pVM->csam.s.pvDirtyFaultPage, 0, sizeof(pVM->csam.s.pvDirtyFaultPage));

    memset(&pVM->csam.s.aDangerousInstr, 0, sizeof(pVM->csam.s.aDangerousInstr));
    pVM->csam.s.cDangerousInstr = 0;
    pVM->csam.s.iDangerousInstr  = 0;

    memset(pVM->csam.s.pvCallInstruction, 0, sizeof(pVM->csam.s.pvCallInstruction));
    pVM->csam.s.iCallInstruction = 0;

    /** @note never mess with the pgdir bitmap here! */
    return VINF_SUCCESS;
}

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate itself inside the GC.
 *
 * The csam will update the addresses used by the switcher.
 *
 * @param   pVM      The VM.
 * @param   offDelta Relocation delta.
 */
CSAMR3DECL(void) CSAMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    if (offDelta)
    {
        /* Adjust pgdir and page bitmap pointers. */
        pVM->csam.s.pPDBitmapGC   = MMHyperHC2GC(pVM, pVM->csam.s.pPDGCBitmapHC);
        pVM->csam.s.pPDHCBitmapGC = MMHyperHC2GC(pVM, pVM->csam.s.pPDBitmapHC);

        for(int i=0;i<CSAM_PGDIRBMP_CHUNKS;i++)
        {
            if (pVM->csam.s.pPDGCBitmapHC[i])
            {
                pVM->csam.s.pPDGCBitmapHC[i] += offDelta;
            }
        }
    }
    return;
}

/**
 * Terminates the csam.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CSAMR3DECL(int) CSAMR3Term(PVM pVM)
{
    int rc;

    rc = CSAMR3Reset(pVM);
    AssertRC(rc);

    /* @todo triggers assertion in MMHyperFree */
#if 0
    for(int i=0;i<CSAM_PAGEBMP_CHUNKS;i++)
    {
        if (pVM->csam.s.pPDBitmapHC[i])
            MMHyperFree(pVM, pVM->csam.s.pPDBitmapHC[i]);
    }
#endif

    return VINF_SUCCESS;
}

/**
 * CSAM reset callback.
 *
 * @returns VBox status code.
 * @param   pVM     The VM which is reset.
 */
CSAMR3DECL(int) CSAMR3Reset(PVM pVM)
{
    /* Clear page bitmaps. */
    for(int i=0;i<CSAM_PGDIRBMP_CHUNKS;i++)
    {
        if (pVM->csam.s.pPDBitmapHC[i])
        {
            Assert((CSAM_PAGE_BITMAP_SIZE& 3) == 0);
            ASMMemZero32(pVM->csam.s.pPDBitmapHC[i], CSAM_PAGE_BITMAP_SIZE);
        }
    }

    /* Remove all CSAM page records. */
    while(true)
    {
        PCSAMPAGEREC pPageRec = (PCSAMPAGEREC)RTAvlPVGetBestFit(&pVM->csam.s.pPageTree, 0, true);
        if (pPageRec)
        {
            csamRemovePageRecord(pVM, pPageRec->page.pPageGC);
        }
        else
            break;
    }
    Assert(!pVM->csam.s.pPageTree);

    csamReinit(pVM);

    return VINF_SUCCESS;
}

#define CSAM_SUBTRACT_PTR(a, b) *(uintptr_t *)&(a) = (uintptr_t)(a) - (uintptr_t)(b)
#define CSAM_ADD_PTR(a, b)      *(uintptr_t *)&(a) = (uintptr_t)(a) + (uintptr_t)(b)


/**
 * Callback function for RTAvlPVDoWithAll
 *
 * Counts the number of records in the tree
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pcPatches       Pointer to patch counter
 */
static DECLCALLBACK(int) CountRecord(PAVLPVNODECORE pNode, void *pcPatches)
{
    *(uint32_t *)pcPatches = *(uint32_t *)pcPatches + 1;
    return VINF_SUCCESS;
}

/**
 * Callback function for RTAvlPVDoWithAll
 *
 * Saves the state of the page record
 *
 * @returns VBox status code.
 * @param   pNode           Current node
 * @param   pVM1            VM Handle
 */
static DECLCALLBACK(int) SavePageState(PAVLPVNODECORE pNode, void *pVM1)
{
    PVM           pVM    = (PVM)pVM1;
    PCSAMPAGEREC  pPage  = (PCSAMPAGEREC)pNode;
    CSAMPAGEREC   page   = *pPage;
    PSSMHANDLE    pSSM   = pVM->csam.s.savedstate.pSSM;
    int           rc;

    /* Save the page record itself */
    rc = SSMR3PutMem(pSSM, &page, sizeof(page));
    AssertRCReturn(rc, rc);

    if (page.page.pBitmap)
    {
        rc = SSMR3PutMem(pSSM, page.page.pBitmap, CSAM_PAGE_BITMAP_SIZE);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) csamr3Save(PVM pVM, PSSMHANDLE pSSM)
{
    CSAM csamInfo = pVM->csam.s;
    int  rc;

    /*
     * Count the number of page records in the tree (feeling lazy)
     */
    csamInfo.savedstate.cPageRecords = 0;
    RTAvlPVDoWithAll(&pVM->csam.s.pPageTree, true, CountRecord, &csamInfo.savedstate.cPageRecords);

    /*
     * Save CSAM structure
     */
    pVM->csam.s.savedstate.pSSM = pSSM;
    rc = SSMR3PutMem(pSSM, &csamInfo, sizeof(csamInfo));
    AssertRCReturn(rc, rc);

    /* Save pgdir bitmap */
    rc = SSMR3PutMem(pSSM, csamInfo.pPDBitmapHC, CSAM_PGDIRBMP_CHUNKS*sizeof(RTHCPTR));
    AssertRCReturn(rc, rc);

    for (unsigned i=0;i<CSAM_PGDIRBMP_CHUNKS;i++)
    {
        if(csamInfo.pPDBitmapHC[i])
        {
            /* Save the page bitmap. */
            rc = SSMR3PutMem(pSSM, csamInfo.pPDBitmapHC[i], CSAM_PAGE_BITMAP_SIZE);
            AssertRCReturn(rc, rc);
        }
    }

    /*
     * Save page records
     */
    rc = RTAvlPVDoWithAll(&pVM->csam.s.pPageTree, true, SavePageState, pVM);
    AssertRCReturn(rc, rc);

    /** @note we don't restore aDangerousInstr; it will be recreated automatically. */
    return VINF_SUCCESS;
}

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) csamr3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    int  rc;
    CSAM csamInfo;

    if (u32Version != CSAM_SSM_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    pVM->csam.s.savedstate.pSSM = pSSM;

    /*
     * Restore CSAM structure
     */
    rc = SSMR3GetMem(pSSM, &csamInfo, sizeof(csamInfo));
    AssertRCReturn(rc, rc);

    pVM->csam.s.fGatesChecked    = csamInfo.fGatesChecked;
    pVM->csam.s.fScanningStarted = csamInfo.fScanningStarted;

     /* Restore dirty code page info. */
    pVM->csam.s.cDirtyPages = csamInfo.cDirtyPages;
    memcpy(pVM->csam.s.pvDirtyBasePage,  csamInfo.pvDirtyBasePage, sizeof(pVM->csam.s.pvDirtyBasePage));
    memcpy(pVM->csam.s.pvDirtyFaultPage, csamInfo.pvDirtyFaultPage, sizeof(pVM->csam.s.pvDirtyFaultPage));

     /* Restore possible code page  */
    pVM->csam.s.cPossibleCodePages = csamInfo.cPossibleCodePages;
    memcpy(pVM->csam.s.pvPossibleCodePage,  csamInfo.pvPossibleCodePage, sizeof(pVM->csam.s.pvPossibleCodePage));

    /* Restore pgdir bitmap (we'll change the pointers next). */
    rc = SSMR3GetMem(pSSM, pVM->csam.s.pPDBitmapHC, CSAM_PGDIRBMP_CHUNKS*sizeof(RTHCPTR));
    AssertRCReturn(rc, rc);

    /*
     * Restore page bitmaps
     */
    for (unsigned i=0;i<CSAM_PGDIRBMP_CHUNKS;i++)
    {
        if(pVM->csam.s.pPDBitmapHC[i])
        {
            rc = MMHyperAlloc(pVM, CSAM_PAGE_BITMAP_SIZE, 0, MM_TAG_CSAM, (void **)&pVM->csam.s.pPDBitmapHC[i]);
            if (VBOX_FAILURE(rc))
            {
                Log(("MMR3HyperAlloc failed with %d\n", rc));
                return rc;
            }
            /* Convert to GC pointer. */
            pVM->csam.s.pPDGCBitmapHC[i] = MMHyperHC2GC(pVM, pVM->csam.s.pPDBitmapHC[i]);
            Assert(pVM->csam.s.pPDGCBitmapHC[i]);

            /* Restore the bitmap. */
            rc = SSMR3GetMem(pSSM, pVM->csam.s.pPDBitmapHC[i], CSAM_PAGE_BITMAP_SIZE);
            AssertRCReturn(rc, rc);
        }
        else
        {
            Assert(!pVM->csam.s.pPDGCBitmapHC[i]);
            pVM->csam.s.pPDGCBitmapHC[i] = 0;
        }
    }

    /*
     * Restore page records
     */
    for (uint32_t i=0;i<csamInfo.savedstate.cPageRecords + csamInfo.savedstate.cPatchPageRecords;i++)
    {
        CSAMPAGEREC  page;
        PCSAMPAGE    pPage;

        rc = SSMR3GetMem(pSSM, &page, sizeof(page));
        AssertRCReturn(rc, rc);

        /*
         * Recreate the page record
         */
        pPage = csamCreatePageRecord(pVM, page.page.pPageGC, page.page.enmTag, page.page.fCode32, page.page.fMonitorInvalidation);
        AssertReturn(pPage, VERR_NO_MEMORY);

        pPage->GCPhys  = page.page.GCPhys;
        pPage->fFlags  = page.page.fFlags;
        pPage->u64Hash = page.page.u64Hash;

        if (page.page.pBitmap)
        {
            rc = SSMR3GetMem(pSSM, pPage->pBitmap, CSAM_PAGE_BITMAP_SIZE);
            AssertRCReturn(rc, rc);
        }
        else
        {
            MMR3HeapFree(pPage->pBitmap);
            pPage->pBitmap = 0;
        }
    }

    /** @note we don't restore aDangerousInstr; it will be recreated automatically. */
    memset(&pVM->csam.s.aDangerousInstr, 0, sizeof(pVM->csam.s.aDangerousInstr));
    pVM->csam.s.cDangerousInstr = 0;
    pVM->csam.s.iDangerousInstr  = 0;
    return VINF_SUCCESS;
}

/**
 * Convert guest context address to host context pointer
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCacheRec   Address conversion cache record
 * @param   pGCPtr      Guest context pointer
 *
 * @returns             Host context pointer or NULL in case of an error
 *
 */
static R3PTRTYPE(void *) CSAMGCVirtToHCVirt(PVM pVM, PCSAMP2GLOOKUPREC pCacheRec, GCPTRTYPE(uint8_t *) pGCPtr)
{
    int rc;
    R3PTRTYPE(void *) pHCPtr;

    STAM_PROFILE_START(&pVM->csam.s.StatTimeAddrConv, a);

    pHCPtr = PATMR3GCPtrToHCPtr(pVM, pGCPtr);
    if (pHCPtr) return pHCPtr;

    if (pCacheRec->pPageLocStartHC)
    {
        uint32_t offset = pGCPtr & PAGE_OFFSET_MASK;
        if (pCacheRec->pGuestLoc == (pGCPtr & PAGE_BASE_GC_MASK))
        {
            STAM_PROFILE_STOP(&pVM->csam.s.StatTimeAddrConv, a);
            return pCacheRec->pPageLocStartHC + offset;
        }
    }

    rc = PGMPhysGCPtr2HCPtr(pVM, pGCPtr, &pHCPtr);
    if (rc != VINF_SUCCESS)
    {
////        AssertMsgRC(rc, ("MMR3PhysGCVirt2HCVirtEx failed for %VGv\n", pGCPtr));
        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeAddrConv, a);
        return NULL;
    }
////invalid?    Assert(sizeof(R3PTRTYPE(uint8_t*)) == sizeof(uint32_t));

    pCacheRec->pPageLocStartHC = (R3PTRTYPE(uint8_t*))((RTHCUINTPTR)pHCPtr & PAGE_BASE_HC_MASK);
    pCacheRec->pGuestLoc      = pGCPtr & PAGE_BASE_GC_MASK;
    STAM_PROFILE_STOP(&pVM->csam.s.StatTimeAddrConv, a);
    return pHCPtr;
}

/**
 * Read callback for disassembly function; supports reading bytes that cross a page boundary
 *
 * @returns VBox status code.
 * @param   pSrc        GC source pointer
 * @param   pDest       HC destination pointer
 * @param   size        Number of bytes to read
 * @param   dwUserdata  Callback specific user data (pCpu)
 *
 */
static DECLCALLBACK(int) CSAMR3ReadBytes(RTHCUINTPTR pSrc, uint8_t *pDest, unsigned size, void *pvUserdata)
{
    DISCPUSTATE  *pCpu     = (DISCPUSTATE *)pvUserdata;
    PVM           pVM      = (PVM)pCpu->apvUserData[0];
    RTHCUINTPTR   pInstrHC = (RTHCUINTPTR)pCpu->apvUserData[1];
    RTGCUINTPTR   pInstrGC = (uintptr_t)pCpu->apvUserData[2];
    int           orgsize  = size;

    /* We are not interested in patched instructions, so read the original opcode bytes. */
    /** @note single instruction patches (int3) are checked in CSAMR3AnalyseCallback */
    for (int i=0;i<orgsize;i++)
    {
        int rc = PATMR3QueryOpcode(pVM, (RTGCPTR)pSrc, pDest);
        if (VBOX_SUCCESS(rc))
        {
            pSrc++;
            pDest++;
            size--;
        }
        else
            break;
    }
    if (size == 0)
        return VINF_SUCCESS;

    if (PAGE_ADDRESS(pInstrGC) != PAGE_ADDRESS(pSrc + size - 1) && !PATMIsPatchGCAddr(pVM, pSrc))
    {
        return PGMPhysReadGCPtr(pVM, pDest, pSrc, size);
    }
    else
    {
        Assert(pInstrHC);

        /* pInstrHC is the base address; adjust according to the GC pointer. */
        pInstrHC = pInstrHC + (pSrc - pInstrGC);

        memcpy(pDest, (void *)pInstrHC, size);
    }

    return VINF_SUCCESS;
}

inline int CSAMR3DISInstr(PVM pVM, DISCPUSTATE *pCpu, RTGCPTR InstrGC, uint8_t *InstrHC, uint32_t *pOpsize, char *pszOutput)
{
    (pCpu)->pfnReadBytes  = CSAMR3ReadBytes;
    (pCpu)->apvUserData[0] = pVM;
    (pCpu)->apvUserData[1] = InstrHC;
    (pCpu)->apvUserData[2] = (void *)InstrGC; Assert(sizeof(InstrGC) <= sizeof(pCpu->apvUserData[0]));
#ifdef DEBUG
    return DISInstrEx(pCpu, InstrGC, 0, pOpsize, pszOutput, OPTYPE_ALL);
#else
    /* We are interested in everything except harmless stuff */
    return DISInstrEx(pCpu, InstrGC, 0, pOpsize, pszOutput, ~(OPTYPE_INVALID | OPTYPE_HARMLESS | OPTYPE_RRM_MASK));
#endif
}

/**
 * Analyses the instructions following the cli for compliance with our heuristics for cli
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU disassembly state
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   pCacheRec   GC to HC cache record
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int CSAMR3AnalyseCallback(PVM pVM, DISCPUSTATE *pCpu, GCPTRTYPE(uint8_t *) pInstrGC, GCPTRTYPE(uint8_t *) pCurInstrGC,
                                 PCSAMP2GLOOKUPREC pCacheRec, void *pUserData)
{
    PCSAMPAGE pPage = (PCSAMPAGE)pUserData;
    int rc;

    switch(pCpu->pCurInstr->opcode)
    {
    case OP_INT:
        Assert(pCpu->param1.flags & USE_IMMEDIATE8);
        if (pCpu->param1.parval == 3)
        {
            //two byte int 3
            return VINF_SUCCESS;
        }
        break;

    case OP_ILLUD2:
        /* This appears to be some kind of kernel panic in Linux 2.4; no point to continue. */
    case OP_RETN:
    case OP_INT3:
    case OP_INVALID:
#if 1
    /* removing breaks win2k guests? */
    case OP_IRET:
#endif
        return VINF_SUCCESS;
    }

    // Check for exit points
    switch (pCpu->pCurInstr->opcode)
    {
    /* It's not a good idea to patch pushf instructions:
     * - increases the chance of conflicts (code jumping to the next instruction)
     * - better to patch the cli
     * - code that branches before the cli will likely hit an int 3
     * - in general doesn't offer any benefits as we don't allow nested patch blocks (IF is always 1)
     */
    case OP_PUSHF:
    case OP_POPF:
        break;

    case OP_CLI:
    {
        uint32_t     cbInstr = 0;
        uint32_t     opsize  = pCpu->opsize;

        PATMR3AddHint(pVM, pCurInstrGC, (pPage->fCode32) ? PATMFL_CODE32 : 0);

        /* Make sure the instructions that follow the cli have not been encountered before. */
        while (true)
        {
            DISCPUSTATE  cpu;
            uint8_t     *pCurInstrHC = 0;

            if (cbInstr + opsize >= SIZEOF_NEARJUMP32)
                break;

            if (csamIsCodeScanned(pVM, pCurInstrGC + opsize, &pPage) == true)
            {
                /* We've scanned the next instruction(s) already. This means we've followed a branch that ended up there before -> dangerous!! */
                PATMR3DetectConflict(pVM, pCurInstrGC, pCurInstrGC + opsize);
                break;
            }
            pCurInstrGC += opsize;
            cbInstr     += opsize;

            pCurInstrHC = (uint8_t *)CSAMGCVirtToHCVirt(pVM, pCacheRec, pCurInstrGC);
            if (pCurInstrHC == NULL)
            {
                Log(("CSAMGCVirtToHCVirt failed for %VGv\n", pCurInstrGC));
                break;
            }
            Assert(VALID_PTR(pCurInstrHC));

            cpu.mode = (pPage->fCode32) ? CPUMODE_32BIT : CPUMODE_16BIT;
            rc = CSAMR3DISInstr(pVM, &cpu, pCurInstrGC, pCurInstrHC, &opsize, NULL);
            Assert(VBOX_SUCCESS(rc));
            if (VBOX_FAILURE(rc))
                break;
        }
        break;
    }

    case OP_PUSH:
        if (pCpu->pCurInstr->param1 != OP_PARM_REG_CS)
            break;

        /* no break */
    case OP_STR:
    case OP_LSL:
    case OP_LAR:
    case OP_SGDT:
    case OP_SLDT:
    case OP_SIDT:
    case OP_SMSW:
    case OP_VERW:
    case OP_VERR:
    case OP_CPUID:
    case OP_IRET:
#ifdef DEBUG
        switch(pCpu->pCurInstr->opcode)
        {
        case OP_STR:
            Log(("Privileged instruction at %VGv: str!!\n", pCurInstrGC));
            break;
        case OP_LSL:
            Log(("Privileged instruction at %VGv: lsl!!\n", pCurInstrGC));
            break;
        case OP_LAR:
            Log(("Privileged instruction at %VGv: lar!!\n", pCurInstrGC));
            break;
        case OP_SGDT:
            Log(("Privileged instruction at %VGv: sgdt!!\n", pCurInstrGC));
            break;
        case OP_SLDT:
            Log(("Privileged instruction at %VGv: sldt!!\n", pCurInstrGC));
            break;
        case OP_SIDT:
            Log(("Privileged instruction at %VGv: sidt!!\n", pCurInstrGC));
            break;
        case OP_SMSW:
            Log(("Privileged instruction at %VGv: smsw!!\n", pCurInstrGC));
            break;
        case OP_VERW:
            Log(("Privileged instruction at %VGv: verw!!\n", pCurInstrGC));
            break;
        case OP_VERR:
            Log(("Privileged instruction at %VGv: verr!!\n", pCurInstrGC));
            break;
        case OP_CPUID:
            Log(("Privileged instruction at %VGv: cpuid!!\n", pCurInstrGC));
            break;
        case OP_PUSH:
            Log(("Privileged instruction at %VGv: push cs!!\n", pCurInstrGC));
            break;
        case OP_IRET:
            Log(("Privileged instruction at %VGv: iret!!\n", pCurInstrGC));
            break;
        }
#endif

        if (PATMR3HasBeenPatched(pVM, pCurInstrGC) == false)
        {
            rc = PATMR3InstallPatch(pVM, pCurInstrGC, (pPage->fCode32) ? PATMFL_CODE32 : 0);
            if (VBOX_FAILURE(rc))
            {
                Log(("PATMR3InstallPatch failed with %d\n", rc));
                return VWRN_CONTINUE_ANALYSIS;
            }
        }
        if (pCpu->pCurInstr->opcode == OP_IRET)
            return VINF_SUCCESS;    /* Look no further in this branch. */

        return VWRN_CONTINUE_ANALYSIS;

    case OP_JMP:
    case OP_CALL:
    {
        // return or jump/call through a jump table
        if (OP_PARM_VTYPE(pCpu->pCurInstr->param1) != OP_PARM_J)
        {
#ifdef DEBUG
            switch(pCpu->pCurInstr->opcode)
            {
            case OP_JMP:
                Log(("Control Flow instruction at %VGv: jmp!!\n", pCurInstrGC));
                break;
            case OP_CALL:
                Log(("Control Flow instruction at %VGv: call!!\n", pCurInstrGC));
                break;
            }
#endif
            return VWRN_CONTINUE_ANALYSIS;
        }
        return VWRN_CONTINUE_ANALYSIS;
    }

    }

    return VWRN_CONTINUE_ANALYSIS;
}

#ifdef CSAM_ANALYSE_BEYOND_RET
/**
 * Wrapper for csamAnalyseCodeStream for call instructions.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   fCode32     16 or 32 bits code
 * @param   pfnCSAMR3Analyse Callback for testing the disassembled instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int csamAnalyseCallCodeStream(PVM pVM, GCPTRTYPE(uint8_t *) pInstrGC, GCPTRTYPE(uint8_t *) pCurInstrGC, bool fCode32,
                                     PFN_CSAMR3ANALYSE pfnCSAMR3Analyse, void *pUserData, PCSAMP2GLOOKUPREC pCacheRec)
{
    int                 rc;
    CSAMCALLEXITREC     CallExitRec;
    PCSAMCALLEXITREC    pOldCallRec;
    PCSAMPAGE           pPage = 0;
    uint32_t            i;

    CallExitRec.cInstrAfterRet = 0;

    pOldCallRec = pCacheRec->pCallExitRec;
    pCacheRec->pCallExitRec = &CallExitRec;

    rc = csamAnalyseCodeStream(pVM, pInstrGC, pCurInstrGC, fCode32, pfnCSAMR3Analyse, pUserData, pCacheRec);

    for (i=0;i<CallExitRec.cInstrAfterRet;i++)
    {
        PCSAMPAGE pPage = 0;

        pCurInstrGC = CallExitRec.pInstrAfterRetGC[i];

        /* Check if we've previously encountered the instruction after the ret. */
        if (csamIsCodeScanned(pVM, pCurInstrGC, &pPage) == false)
        {
            DISCPUSTATE  cpu;
            uint32_t     opsize;
            uint8_t     *pCurInstrHC = 0;
            int          rc2;
#ifdef DEBUG
            char szOutput[256];
#endif
            if (pPage == NULL)
            {
                /* New address; let's take a look at it. */
                pPage = csamCreatePageRecord(pVM, pCurInstrGC, CSAM_TAG_CSAM, fCode32);
                if (pPage == NULL)
                {
                    rc = VERR_NO_MEMORY;
                    goto done;
                }
            }

            /**
             * Some generic requirements for recognizing an adjacent function:
             * - alignment fillers that consist of:
             *   - nop
             *   - lea genregX, [genregX (+ 0)]
             * - push ebp after the filler (can extend this later); aligned at at least a 4 byte boundary
             */
            for (int j=0;j<16;j++)
            {
                pCurInstrHC = (uint8_t *)CSAMGCVirtToHCVirt(pVM, pCacheRec, pCurInstrGC);
                if (pCurInstrHC == NULL)
                {
                    Log(("CSAMGCVirtToHCVirt failed for %VGv\n", pCurInstrGC));
                    goto done;
                }
                Assert(VALID_PTR(pCurInstrHC));

                cpu.mode = (fCode32) ? CPUMODE_32BIT : CPUMODE_16BIT;
                STAM_PROFILE_START(&pVM->csam.s.StatTimeDisasm, a);
#ifdef DEBUG
                rc2 = CSAMR3DISInstr(pVM, &cpu, pCurInstrGC, pCurInstrHC, &opsize, szOutput);
                if (VBOX_SUCCESS(rc2)) Log(("CSAM Call Analysis: %s", szOutput));
#else
                rc2 = CSAMR3DISInstr(pVM, &cpu, pCurInstrGC, pCurInstrHC, &opsize, NULL);
#endif
                STAM_PROFILE_STOP(&pVM->csam.s.StatTimeDisasm, a);
                if (VBOX_FAILURE(rc2))
                {
                    Log(("Disassembly failed at %VGv with %Vrc (probably page not present) -> return to caller\n", pCurInstrGC, rc2));
                    goto done;
                }

                STAM_COUNTER_ADD(&pVM->csam.s.StatNrBytesRead, opsize);

                GCPTRTYPE(uint8_t *) addr = 0;
                PCSAMPAGE pJmpPage = NULL;

                if (PAGE_ADDRESS(pCurInstrGC) != PAGE_ADDRESS(pCurInstrGC + opsize - 1))
                {
                    if (!PGMGstIsPagePresent(pVM, pCurInstrGC + opsize - 1))
                    {
                        /// @todo fault in the page
                        Log(("Page for current instruction %VGv is not present!!\n", pCurInstrGC));
                        goto done;
                    }
                    //all is fine, let's continue
                    csamR3CheckPageRecord(pVM, pCurInstrGC + opsize - 1);
                }

                switch (cpu.pCurInstr->opcode)
                {
                case OP_NOP:
                case OP_INT3:
                    break;  /* acceptable */

                case OP_LEA:
                    /* Must be similar to:
                     *
                     * lea esi, [esi]
                     * lea esi, [esi+0]
                     * Any register is allowed as long as source and destination are identical.
                     */
                    if (    cpu.param1.flags != USE_REG_GEN32
                        ||  (   cpu.param2.flags != USE_REG_GEN32
                             && (   !(cpu.param2.flags & USE_REG_GEN32)
                                 || !(cpu.param2.flags & (USE_DISPLACEMENT8|USE_DISPLACEMENT16|USE_DISPLACEMENT32))
                                 ||  cpu.param2.parval != 0
                                )
                            )
                        ||  cpu.param1.base.reg_gen32 != cpu.param2.base.reg_gen32
                       )
                    {
                       STAM_COUNTER_INC(&pVM->csam.s.StatScanNextFunctionFailed);
                       goto next_function;
                    }
                    break;

                case OP_PUSH:
                {
                    if (    (pCurInstrGC & 0x3) != 0
                        ||  cpu.param1.flags != USE_REG_GEN32
                        ||  cpu.param1.base.reg_gen32 != USE_REG_EBP
                       )
                    {
                       STAM_COUNTER_INC(&pVM->csam.s.StatScanNextFunctionFailed);
                       goto next_function;
                    }

                    if (csamIsCodeScanned(pVM, pCurInstrGC, &pPage) == false)
                    {
                        CSAMCALLEXITREC CallExitRec2;
                        CallExitRec2.cInstrAfterRet = 0;

                        pCacheRec->pCallExitRec = &CallExitRec2;

                        /* Analyse the function. */
                        Log(("Found new function at %VGv\n", pCurInstrGC));
                        STAM_COUNTER_INC(&pVM->csam.s.StatScanNextFunction);
                        csamAnalyseCallCodeStream(pVM, pInstrGC, pCurInstrGC, fCode32, pfnCSAMR3Analyse, pUserData, pCacheRec);
                    }
                    goto next_function;
                }

                case OP_SUB:
                {
                    if (    (pCurInstrGC & 0x3) != 0
                        ||  cpu.param1.flags != USE_REG_GEN32
                        ||  cpu.param1.base.reg_gen32 != USE_REG_ESP
                       )
                    {
                       STAM_COUNTER_INC(&pVM->csam.s.StatScanNextFunctionFailed);
                       goto next_function;
                    }

                    if (csamIsCodeScanned(pVM, pCurInstrGC, &pPage) == false)
                    {
                        CSAMCALLEXITREC CallExitRec2;
                        CallExitRec2.cInstrAfterRet = 0;

                        pCacheRec->pCallExitRec = &CallExitRec2;

                        /* Analyse the function. */
                        Log(("Found new function at %VGv\n", pCurInstrGC));
                        STAM_COUNTER_INC(&pVM->csam.s.StatScanNextFunction);
                        csamAnalyseCallCodeStream(pVM, pInstrGC, pCurInstrGC, fCode32, pfnCSAMR3Analyse, pUserData, pCacheRec);
                    }
                    goto next_function;
                }

                default:
                   STAM_COUNTER_INC(&pVM->csam.s.StatScanNextFunctionFailed);
                   goto next_function;
                }
                /* Mark it as scanned. */
                csamMarkCode(pVM, pPage, pCurInstrGC, opsize, true);
                pCurInstrGC += opsize;
            } /* for at most 16 instructions */
next_function:
            ; /* MSVC complains otherwise */
        }
    }
done:
    pCacheRec->pCallExitRec = pOldCallRec;
    return rc;
}
#else
#define csamAnalyseCallCodeStream       csamAnalyseCodeStream
#endif

/**
 * Disassembles the code stream until the callback function detects a failure or decides everything is acceptable
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Guest context pointer to privileged instruction
 * @param   pCurInstrGC Guest context pointer to the current instruction
 * @param   fCode32     16 or 32 bits code
 * @param   pfnCSAMR3Analyse Callback for testing the disassembled instruction
 * @param   pUserData   User pointer (callback specific)
 *
 */
static int csamAnalyseCodeStream(PVM pVM, GCPTRTYPE(uint8_t *) pInstrGC, GCPTRTYPE(uint8_t *) pCurInstrGC, bool fCode32,
                                   PFN_CSAMR3ANALYSE pfnCSAMR3Analyse, void *pUserData, PCSAMP2GLOOKUPREC pCacheRec)
{
    DISCPUSTATE cpu;
    PCSAMPAGE pPage = (PCSAMPAGE)pUserData;
    int rc = VWRN_CONTINUE_ANALYSIS;
    uint32_t opsize;
    R3PTRTYPE(uint8_t *) pCurInstrHC = 0;
    int rc2;

#ifdef DEBUG
    char szOutput[256];
#endif

    LogFlow(("csamAnalyseCodeStream: code at %VGv depth=%d\n", pCurInstrGC, pCacheRec->depth));

    pVM->csam.s.fScanningStarted = true;

    pCacheRec->depth++;
    /*
     * Limit the call depth. (rather arbitrary upper limit; too low and we won't detect certain
     * cpuid instructions in Linux kernels; too high and we waste too much time scanning code)
     * (512 is necessary to detect cpuid instructions in Red Hat EL4; see defect 1355)
     * @note we are using a lot of stack here. couple of 100k when we go to the full depth (!)
     */
    if (pCacheRec->depth > 512)
    {
        LogFlow(("CSAM: maximum calldepth reached for %VGv\n", pCurInstrGC));
        pCacheRec->depth--;
        return VINF_SUCCESS;    //let's not go on forever
    }

    Assert(!PATMIsPatchGCAddr(pVM, pCurInstrGC));
    csamR3CheckPageRecord(pVM, pCurInstrGC);

    while(rc == VWRN_CONTINUE_ANALYSIS)
    {
        if (csamIsCodeScanned(pVM, pCurInstrGC, &pPage) == false)
        {
            if (pPage == NULL)
            {
                /* New address; let's take a look at it. */
                pPage = csamCreatePageRecord(pVM, pCurInstrGC, CSAM_TAG_CSAM, fCode32);
                if (pPage == NULL)
                {
                    rc = VERR_NO_MEMORY;
                    goto done;
                }
            }
        }
        else
        {
            LogFlow(("Code at %VGv has been scanned before\n", pCurInstrGC));
            rc = VINF_SUCCESS;
            goto done;
        }

        pCurInstrHC = (uint8_t *)CSAMGCVirtToHCVirt(pVM, pCacheRec, pCurInstrGC);
        if (pCurInstrHC == NULL)
        {
            Log(("CSAMGCVirtToHCVirt failed for %VGv\n", pCurInstrGC));
            rc = VERR_PATCHING_REFUSED;
            goto done;
        }
        Assert(VALID_PTR(pCurInstrHC));

        cpu.mode = (fCode32) ? CPUMODE_32BIT : CPUMODE_16BIT;
        STAM_PROFILE_START(&pVM->csam.s.StatTimeDisasm, a);
#ifdef DEBUG
        rc2 = CSAMR3DISInstr(pVM, &cpu, pCurInstrGC, pCurInstrHC, &opsize, szOutput);
        if (VBOX_SUCCESS(rc2)) Log(("CSAM Analysis: %s", szOutput));
#else
        rc2 = CSAMR3DISInstr(pVM, &cpu, pCurInstrGC, pCurInstrHC, &opsize, NULL);
#endif
        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeDisasm, a);
        if (VBOX_FAILURE(rc2))
        {
            Log(("Disassembly failed at %VGv with %Vrc (probably page not present) -> return to caller\n", pCurInstrGC, rc2));
            rc = VINF_SUCCESS;
            goto done;
        }

        STAM_COUNTER_ADD(&pVM->csam.s.StatNrBytesRead, opsize);

        csamMarkCode(pVM, pPage, pCurInstrGC, opsize, true);

        GCPTRTYPE(uint8_t *) addr = 0;
        PCSAMPAGE pJmpPage = NULL;

        if (PAGE_ADDRESS(pCurInstrGC) != PAGE_ADDRESS(pCurInstrGC + opsize - 1))
        {
            if (!PGMGstIsPagePresent(pVM, pCurInstrGC + opsize - 1))
            {
                /// @todo fault in the page
                Log(("Page for current instruction %VGv is not present!!\n", pCurInstrGC));
                rc = VWRN_CONTINUE_ANALYSIS;
                goto next_please;
            }
            //all is fine, let's continue
            csamR3CheckPageRecord(pVM, pCurInstrGC + opsize - 1);
        }
        /*
         * If it's harmless, then don't bother checking it (the disasm tables had better be accurate!)
         */
        if ((cpu.pCurInstr->optype & ~OPTYPE_RRM_MASK) == OPTYPE_HARMLESS)
        {
            AssertMsg(pfnCSAMR3Analyse(pVM, &cpu, pInstrGC, pCurInstrGC, pCacheRec, (void *)pPage) == VWRN_CONTINUE_ANALYSIS, ("Instruction incorrectly marked harmless?!?!?\n"));
            rc = VWRN_CONTINUE_ANALYSIS;
            goto next_please;
        }

#ifdef CSAM_ANALYSE_BEYOND_RET
        /* Remember the address of the instruction following the ret in case the parent instruction was a call. */
        if (    pCacheRec->pCallExitRec
            &&  cpu.pCurInstr->opcode == OP_RETN
            &&  pCacheRec->pCallExitRec->cInstrAfterRet < CSAM_MAX_CALLEXIT_RET)
        {
            pCacheRec->pCallExitRec->pInstrAfterRetGC[pCacheRec->pCallExitRec->cInstrAfterRet] = pCurInstrGC + opsize;
            pCacheRec->pCallExitRec->cInstrAfterRet++;
        }
#endif

        rc = pfnCSAMR3Analyse(pVM, &cpu, pInstrGC, pCurInstrGC, pCacheRec, (void *)pPage);
        if (rc == VINF_SUCCESS)
            goto done;

        // For our first attempt, we'll handle only simple relative jumps and calls (immediate offset coded in instruction)
        if ((cpu.pCurInstr->optype & OPTYPE_CONTROLFLOW) && (OP_PARM_VTYPE(cpu.pCurInstr->param1) == OP_PARM_J))
        {
            addr = CSAMResolveBranch(&cpu, pCurInstrGC);
            if (addr == 0)
            {
                Log(("We don't support far jumps here!! (%08X)\n", cpu.param1.flags));
                rc = VINF_SUCCESS;
                break;
            }
            Assert(!PATMIsPatchGCAddr(pVM, addr));

            /* If the target address lies in a patch generated jump, then special action needs to be taken. */
            PATMR3DetectConflict(pVM, pCurInstrGC, addr);

            /* Same page? */
            if (PAGE_ADDRESS(addr) != PAGE_ADDRESS(pCurInstrGC ))
            {
                if (!PGMGstIsPagePresent(pVM, addr))
                {
                    Log(("Page for current instruction %VGv is not present!!\n", addr));
                    rc = VWRN_CONTINUE_ANALYSIS;
                    goto next_please;
                }

                /* All is fine, let's continue. */
                csamR3CheckPageRecord(pVM, addr);
            }

            pJmpPage = NULL;
            if (csamIsCodeScanned(pVM, addr, &pJmpPage) == false)
            {
                if (pJmpPage == NULL)
                {
                    /* New branch target; let's take a look at it. */
                    pJmpPage = csamCreatePageRecord(pVM, addr, CSAM_TAG_CSAM, fCode32);
                    if (pJmpPage == NULL)
                    {
                        rc = VERR_NO_MEMORY;
                        goto done;
                    }
                    Assert(pPage);
                }
                if (cpu.pCurInstr->opcode == OP_CALL)
                    rc = csamAnalyseCallCodeStream(pVM, pInstrGC, addr, fCode32, pfnCSAMR3Analyse, (void *)pJmpPage, pCacheRec);
                else
                    rc = csamAnalyseCodeStream(pVM, pInstrGC, addr, fCode32, pfnCSAMR3Analyse, (void *)pJmpPage, pCacheRec);

                if (rc != VINF_SUCCESS) {
                    goto done;
                }
            }
            if (cpu.pCurInstr->opcode == OP_JMP)
            {//unconditional jump; return to caller
                rc = VINF_SUCCESS;
                goto done;
            }

            rc = VWRN_CONTINUE_ANALYSIS;
        } //if ((cpu.pCurInstr->optype & OPTYPE_CONTROLFLOW) && (OP_PARM_VTYPE(cpu.pCurInstr->param1) == OP_PARM_J))
#ifdef CSAM_SCAN_JUMP_TABLE
        else
        if (    cpu.pCurInstr->opcode == OP_JMP
            &&  (cpu.param1.flags & (USE_DISPLACEMENT32|USE_INDEX|USE_SCALE)) == (USE_DISPLACEMENT32|USE_INDEX|USE_SCALE)
           )
        {
            RTGCPTR  pJumpTableGC = (RTGCPTR)cpu.param1.disp32;
            uint8_t *pJumpTableHC;
            int      rc2;

            Log(("Jump through jump table\n"));

            rc2 = PGMPhysGCPtr2HCPtr(pVM, pJumpTableGC, (PRTHCPTR)&pJumpTableHC);
            if (rc2 == VINF_SUCCESS)
            {
                for (uint32_t i=0;i<2;i++)
                {
                    uint64_t fFlags;

                    addr = pJumpTableGC + cpu.param1.scale * i;
                    /* Same page? */
                    if (PAGE_ADDRESS(addr) != PAGE_ADDRESS(pJumpTableGC))
                        break;

                    addr = *(RTGCPTR *)(pJumpTableHC + cpu.param1.scale * i);

                    rc2 = PGMGstGetPage(pVM, addr, &fFlags, NULL);
                    if (    rc2 != VINF_SUCCESS
                        ||  (fFlags & X86_PTE_US)
                        || !(fFlags & X86_PTE_P)
                       )
                       break;

                    Log(("Jump to %VGv\n", addr));

                    pJmpPage = NULL;
                    if (csamIsCodeScanned(pVM, addr, &pJmpPage) == false)
                    {
                        if (pJmpPage == NULL)
                        {
                            /* New branch target; let's take a look at it. */
                            pJmpPage = csamCreatePageRecord(pVM, addr, CSAM_TAG_CSAM, fCode32);
                            if (pJmpPage == NULL)
                            {
                                rc = VERR_NO_MEMORY;
                                goto done;
                            }
                            Assert(pPage);
                        }
                        rc = csamAnalyseCodeStream(pVM, pInstrGC, addr, fCode32, pfnCSAMR3Analyse, (void *)pJmpPage, pCacheRec);
                        if (rc != VINF_SUCCESS) {
                            goto done;
                        }
                    }
                }
            }
        }
#endif
        if (rc != VWRN_CONTINUE_ANALYSIS) {
            break; //done!
        }
next_please:
        if (cpu.pCurInstr->opcode == OP_JMP)
        {
            rc = VINF_SUCCESS;
            goto done;
        }
        pCurInstrGC += opsize;
    }
done:
    pCacheRec->depth--;
    return rc;
}


/**
 * Calculates the 64 bits hash value for the current page
 *
 * @returns hash value
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Page address
 */
uint64_t csamR3CalcPageHash(PVM pVM, RTGCPTR pInstr)
{
    uint64_t hash   = 0;
    uint32_t val[5];
    int      rc;

    Assert((pInstr & PAGE_OFFSET_MASK) == 0);

    rc = PGMPhysReadGCPtr(pVM, &val[0], pInstr, sizeof(val[0]));
    AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
    if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
    {
        Log(("csamR3CalcPageHash: page %VGv not present!!\n", pInstr));
        return ~0ULL;
    }

    rc = PGMPhysReadGCPtr(pVM, &val[1], pInstr+1024, sizeof(val[0]));
    AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
    if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
    {
        Log(("csamR3CalcPageHash: page %VGv not present!!\n", pInstr));
        return ~0ULL;
    }

    rc = PGMPhysReadGCPtr(pVM, &val[2], pInstr+2048, sizeof(val[0]));
    AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
    if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
    {
        Log(("csamR3CalcPageHash: page %VGv not present!!\n", pInstr));
        return ~0ULL;
    }

    rc = PGMPhysReadGCPtr(pVM, &val[3], pInstr+3072, sizeof(val[0]));
    AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
    if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
    {
        Log(("csamR3CalcPageHash: page %VGv not present!!\n", pInstr));
        return ~0ULL;
    }

    rc = PGMPhysReadGCPtr(pVM, &val[4], pInstr+4092, sizeof(val[0]));
    AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
    if (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
    {
        Log(("csamR3CalcPageHash: page %VGv not present!!\n", pInstr));
        return ~0ULL;
    }

    // don't want to get division by zero traps
    val[2] |= 1;
    val[4] |= 1;

    hash = (uint64_t)val[0] * (uint64_t)val[1] / (uint64_t)val[2] + (val[3]%val[4]);
    return (hash == ~0ULL) ? hash - 1 : hash;
}


/**
 * Notify CSAM of a page flush
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 * @param   fRemovePage Page removal flag
 */
static int csamFlushPage(PVM pVM, RTGCPTR addr, bool fRemovePage)
{
    PCSAMPAGEREC pPageRec;
    int rc;
    RTGCPHYS GCPhys = 0;
    uint64_t fFlags = 0;

    if (!CSAMIsEnabled(pVM))
        return VINF_SUCCESS;

    STAM_PROFILE_START(&pVM->csam.s.StatTimeFlushPage, a);

    addr = addr & PAGE_BASE_GC_MASK;

    /*
     * Note: searching for the page in our tree first is more expensive (skipped flushes are two orders of magnitude more common)
     */
    if (pVM->csam.s.pPageTree == NULL)
    {
        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeFlushPage, a);
        return VWRN_CSAM_PAGE_NOT_FOUND;
    }

    rc = PGMGstGetPage(pVM, addr, &fFlags, &GCPhys);
    /* Returned at a very early stage (no paging yet presumably). */
    if (rc == VERR_NOT_SUPPORTED)
    {
        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeFlushPage, a);
        return rc;
    }

    if (VBOX_SUCCESS(rc))
    {
        if (    (fFlags & X86_PTE_US)
            ||  rc == VERR_PGM_PHYS_PAGE_RESERVED
           )
        {
            /* User page -> not relevant for us. */
            STAM_COUNTER_ADD(&pVM->csam.s.StatNrFlushesSkipped, 1);
            STAM_PROFILE_STOP(&pVM->csam.s.StatTimeFlushPage, a);
            return VINF_SUCCESS;
        }
    }
    else
    if (rc != VERR_PAGE_NOT_PRESENT && rc != VERR_PAGE_TABLE_NOT_PRESENT)
        AssertMsgFailed(("PGMR3GetPage %VGv failed with %Vrc\n", addr, rc));

    pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)addr);
    if (pPageRec)
    {
        if (    GCPhys == pPageRec->page.GCPhys
            &&  (fFlags & X86_PTE_P))
        {
            STAM_COUNTER_ADD(&pVM->csam.s.StatNrFlushesSkipped, 1);
            STAM_PROFILE_STOP(&pVM->csam.s.StatTimeFlushPage, a);
            return VINF_SUCCESS;
        }

        Log(("CSAMR3FlushPage: page %VGv has changed -> FLUSH (rc=%Vrc) (Phys: %VGp vs %VGp)\n", addr, rc, GCPhys, pPageRec->page.GCPhys));

        STAM_COUNTER_ADD(&pVM->csam.s.StatNrFlushes, 1);

        if (fRemovePage)
            csamRemovePageRecord(pVM, addr);
        else
        {
            CSAMMarkPage(pVM, addr, false);
            pPageRec->page.GCPhys = 0;
            pPageRec->page.fFlags = 0;
            rc = PGMGstGetPage(pVM, addr, &pPageRec->page.fFlags, &pPageRec->page.GCPhys);
            if (rc == VINF_SUCCESS)
                pPageRec->page.u64Hash = csamR3CalcPageHash(pVM, addr);

            if (pPageRec->page.pBitmap == NULL)
            {
                pPageRec->page.pBitmap = (uint8_t *)MMR3HeapAllocZ(pVM, MM_TAG_CSAM_PATCH, CSAM_PAGE_BITMAP_SIZE);
                Assert(pPageRec->page.pBitmap);
                if (pPageRec->page.pBitmap == NULL)
                    return VERR_NO_MEMORY;
            }
            else
                memset(pPageRec->page.pBitmap, 0, CSAM_PAGE_BITMAP_SIZE);
        }


        /*
         * Inform patch manager about the flush; no need to repeat the above check twice.
         */
        PATMR3FlushPage(pVM, addr);

        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeFlushPage, a);
        return VINF_SUCCESS;
    }
    else
    {
        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeFlushPage, a);
        return VWRN_CSAM_PAGE_NOT_FOUND;
    }
}

/**
 * Notify CSAM of a page flush
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 */
CSAMR3DECL(int) CSAMR3FlushPage(PVM pVM, RTGCPTR addr)
{
    return csamFlushPage(pVM, addr, true /* remove page record */);
}

/**
 * Remove a CSAM monitored page. Use with care!
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   addr        GC address of the page to flush
 */
CSAMR3DECL(int) CSAMR3RemovePage(PVM pVM, RTGCPTR addr)
{
    PCSAMPAGEREC pPageRec;
    int          rc;

    addr = addr & PAGE_BASE_GC_MASK;

    pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)addr);
    if (pPageRec)
    {
        rc = csamRemovePageRecord(pVM, addr);
        if (VBOX_SUCCESS(rc))
            PATMR3FlushPage(pVM, addr);
        return VINF_SUCCESS;
    }
    return VWRN_CSAM_PAGE_NOT_FOUND;
}

/**
 * Check a page record in case a page has been changed
 *
 * @returns VBox status code. (trap handled or not)
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    GC instruction pointer
 */
int csamR3CheckPageRecord(PVM pVM, RTGCPTR pInstrGC)
{
    PCSAMPAGEREC pPageRec;
    uint64_t     u64hash;

    pInstrGC = pInstrGC & PAGE_BASE_GC_MASK;

    pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)pInstrGC);
    if (pPageRec)
    {
        u64hash = csamR3CalcPageHash(pVM, pInstrGC);
        if (u64hash != pPageRec->page.u64Hash)
            csamFlushPage(pVM, pInstrGC, false /* don't remove page record */);
    }
    else
        return VWRN_CSAM_PAGE_NOT_FOUND;

    return VINF_SUCCESS;
}

/**
 * Returns monitor description based on CSAM tag
 *
 * @return description string
 * @param   enmTag                  Owner tag
 */
const char *csamGetMonitorDescription(CSAMTAG enmTag)
{
    if (enmTag == CSAM_TAG_PATM)
        return "CSAM-PATM self-modifying code monitor handler";
    else
    if (enmTag == CSAM_TAG_REM)
        return "CSAM-REM self-modifying code monitor handler";
    Assert(enmTag == CSAM_TAG_CSAM);
    return "CSAM self-modifying code monitor handler";
}

/**
 * Adds page record to our lookup tree
 *
 * @returns CSAMPAGE ptr or NULL if failure
 * @param   pVM                     The VM to operate on.
 * @param   GCPtr                   Page address
 * @param   enmTag                  Owner tag
 * @param   fCode32                 16 or 32 bits code
 * @param   fMonitorInvalidation    Monitor page invalidation flag
 */
static PCSAMPAGE csamCreatePageRecord(PVM pVM, RTGCPTR GCPtr, CSAMTAG enmTag, bool fCode32, bool fMonitorInvalidation)
{
    PCSAMPAGEREC pPage;
    int          rc;
    bool         ret;

    Log(("New page record for %VGv\n", GCPtr & PAGE_BASE_GC_MASK));

    pPage = (PCSAMPAGEREC)MMR3HeapAllocZ(pVM, MM_TAG_CSAM_PATCH, sizeof(CSAMPAGEREC));
    if (pPage == NULL)
    {
        AssertMsgFailed(("csamCreatePageRecord: Out of memory!!!!\n"));
        return NULL;
    }
    /* Round down to page boundary. */
    GCPtr = (GCPtr & PAGE_BASE_GC_MASK);
    pPage->Core.Key                  = (AVLPVKEY)GCPtr;
    pPage->page.pPageGC              = GCPtr;
    pPage->page.fCode32              = fCode32;
    pPage->page.fMonitorInvalidation = fMonitorInvalidation;
    pPage->page.enmTag               = enmTag;
    pPage->page.fMonitorActive       = false;
    pPage->page.pBitmap              = (uint8_t *)MMR3HeapAllocZ(pVM, MM_TAG_CSAM_PATCH, PAGE_SIZE/sizeof(uint8_t));
    rc = PGMGstGetPage(pVM, GCPtr, &pPage->page.fFlags, &pPage->page.GCPhys);
    AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));

    pPage->page.u64Hash   = csamR3CalcPageHash(pVM, GCPtr);
    ret = RTAvlPVInsert(&pVM->csam.s.pPageTree, &pPage->Core);
    Assert(ret);

#ifdef CSAM_MONITOR_CODE_PAGES
    AssertRelease(!fInCSAMCodePageInvalidate);

    switch (enmTag)
    {
    case CSAM_TAG_PATM:
    case CSAM_TAG_REM:
#ifdef CSAM_MONITOR_CSAM_CODE_PAGES
    case CSAM_TAG_CSAM:
#endif
    {
        int rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_WRITE, GCPtr, GCPtr + (PAGE_SIZE - 1) /* inclusive! */,
                                             (fMonitorInvalidation) ? CSAMCodePageInvalidate : 0, CSAMCodePageWriteHandler, "CSAMGCCodePageWriteHandler", 0,
                                             csamGetMonitorDescription(enmTag));
        AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PGM_HANDLER_VIRTUAL_CONFLICT, ("PGMR3HandlerVirtualRegisterEx %VGv failed with %Vrc\n", GCPtr, rc));
        if (VBOX_FAILURE(rc))
            Log(("PGMR3HandlerVirtualRegisterEx for %VGv failed with %Vrc\n", GCPtr, rc));

        /* Could fail, because it's already monitored. Don't treat that condition as fatal. */

        /* Prefetch it in case it's not there yet. */
        rc = PGMPrefetchPage(pVM, GCPtr);
        AssertRC(rc);

        rc = PGMShwModifyPage(pVM, GCPtr, 1, 0, ~(uint64_t)X86_PTE_RW);
        Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);

        pPage->page.fMonitorActive = true;
        STAM_COUNTER_INC(&pVM->csam.s.StatPageMonitor);
        break;
    }
    default:
        break; /* to shut up GCC */
    }

    Log(("csamCreatePageRecord %VGv HCPhys=%VGp\n", GCPtr, pPage->page.GCPhys));

#ifdef VBOX_WITH_STATISTICS
    switch (enmTag)
    {
    case CSAM_TAG_CSAM:
        STAM_COUNTER_INC(&pVM->csam.s.StatPageCSAM);
        break;
    case CSAM_TAG_PATM:
        STAM_COUNTER_INC(&pVM->csam.s.StatPagePATM);
        break;
    case CSAM_TAG_REM:
        STAM_COUNTER_INC(&pVM->csam.s.StatPageREM);
        break;
    default:
        break; /* to shut up GCC */
    }
#endif

#endif

    STAM_COUNTER_INC(&pVM->csam.s.StatNrPages);
    if (fMonitorInvalidation)
        STAM_COUNTER_INC(&pVM->csam.s.StatNrPagesInv);

    return &pPage->page;
}

/**
 * Monitors a code page (if not already monitored)
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   pPageAddrGC The page to monitor
 * @param   enmTag      Monitor tag
 */
CSAMR3DECL(int) CSAMR3MonitorPage(PVM pVM, RTGCPTR pPageAddrGC, CSAMTAG enmTag)
{
    PCSAMPAGEREC pPageRec = NULL;
    int          rc;
    bool         fMonitorInvalidation;

    /* Dirty pages must be handled before calling this function!. */
    Assert(!pVM->csam.s.cDirtyPages);

    if (pVM->csam.s.fScanningStarted == false)
        return VINF_SUCCESS;    /* too early */

    pPageAddrGC &= PAGE_BASE_GC_MASK;

    Log(("CSAMR3MonitorPage %VGv %d\n", pPageAddrGC, enmTag));

    /** @todo implicit assumption */
    fMonitorInvalidation = (enmTag == CSAM_TAG_PATM);

    pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)pPageAddrGC);
    if (pPageRec == NULL)
    {
        uint64_t fFlags;

        rc = PGMGstGetPage(pVM, pPageAddrGC, &fFlags, NULL);
        AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
        if (    rc == VINF_SUCCESS
            &&  (fFlags & X86_PTE_US))
        {
            /* We don't care about user pages. */
            STAM_COUNTER_INC(&pVM->csam.s.StatNrUserPages);
            return VINF_SUCCESS;
        }

        csamCreatePageRecord(pVM, pPageAddrGC, enmTag, true /* 32 bits code */, fMonitorInvalidation);

        pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)pPageAddrGC);
        Assert(pPageRec);
    }
    /** @todo reference count */

#ifdef CSAM_MONITOR_CSAM_CODE_PAGES
    Assert(pPageRec->page.fMonitorActive);
#endif

#ifdef CSAM_MONITOR_CODE_PAGES
    if (!pPageRec->page.fMonitorActive)
    {
        Log(("CSAMR3MonitorPage: activate monitoring for %VGv\n", pPageAddrGC));

        rc = PGMR3HandlerVirtualRegister(pVM, PGMVIRTHANDLERTYPE_WRITE, pPageAddrGC, pPageAddrGC + (PAGE_SIZE - 1) /* inclusive! */,
                                         (fMonitorInvalidation) ? CSAMCodePageInvalidate : 0, CSAMCodePageWriteHandler, "CSAMGCCodePageWriteHandler", 0,
                                         csamGetMonitorDescription(enmTag));
        AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PGM_HANDLER_VIRTUAL_CONFLICT, ("PGMR3HandlerVirtualRegisterEx %VGv failed with %Vrc\n", pPageAddrGC, rc));
        if (VBOX_FAILURE(rc))
            Log(("PGMR3HandlerVirtualRegisterEx for %VGv failed with %Vrc\n", pPageAddrGC, rc));

        /* Could fail, because it's already monitored. Don't treat that condition as fatal. */

        /* Prefetch it in case it's not there yet. */
        rc = PGMPrefetchPage(pVM, pPageAddrGC);
        AssertRC(rc);

        rc = PGMShwModifyPage(pVM, pPageAddrGC, 1, 0, ~(uint64_t)X86_PTE_RW);
        Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);

        STAM_COUNTER_INC(&pVM->csam.s.StatPageMonitor);

        pPageRec->page.fMonitorActive       = true;
        pPageRec->page.fMonitorInvalidation = fMonitorInvalidation;
    }
    else
    if (   !pPageRec->page.fMonitorInvalidation
        &&  fMonitorInvalidation)
    {
        Assert(pPageRec->page.fMonitorActive);
        PGMHandlerVirtualChangeInvalidateCallback(pVM, pPageRec->page.pPageGC, CSAMCodePageInvalidate);
        pPageRec->page.fMonitorInvalidation = true;
        STAM_COUNTER_INC(&pVM->csam.s.StatNrPagesInv);

        /* Prefetch it in case it's not there yet. */
        rc = PGMPrefetchPage(pVM, pPageAddrGC);
        AssertRC(rc);

        /* Make sure it's readonly. Page invalidation may have modified the attributes. */
        rc = PGMShwModifyPage(pVM, pPageAddrGC, 1, 0, ~(uint64_t)X86_PTE_RW);
        Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);
    }

#if 0 /* def VBOX_STRICT -> very annoying) */
    if (pPageRec->page.fMonitorActive)
    {
        uint64_t fPageShw;
        RTHCPHYS GCPhys;
        rc = PGMShwGetPage(pVM, pPageAddrGC, &fPageShw, &GCPhys);
//        AssertMsg(     (rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT)
//                ||  !(fPageShw & X86_PTE_RW)
//                ||   (pPageRec->page.GCPhys == 0), ("Shadow page flags for %VGv (%VHp) aren't readonly (%VX64)!!\n", pPageAddrGC, GCPhys, fPageShw));
    }
#endif

    if (pPageRec->page.GCPhys == 0)
    {
        /* Prefetch it in case it's not there yet. */
        rc = PGMPrefetchPage(pVM, pPageAddrGC);
        AssertRC(rc);
        /* The page was changed behind our back. It won't be made read-only until the next SyncCR3, so force it here. */
        rc = PGMShwModifyPage(pVM, pPageAddrGC, 1, 0, ~(uint64_t)X86_PTE_RW);
        Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);
    }
#endif /* CSAM_MONITOR_CODE_PAGES */
    return VINF_SUCCESS;
}

/**
 * Removes a page record from our lookup tree
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       Page address
 */
static int csamRemovePageRecord(PVM pVM, RTGCPTR GCPtr)
{
    PCSAMPAGEREC pPageRec;

    Log(("csamRemovePageRecord %VGv\n", GCPtr));
    pPageRec = (PCSAMPAGEREC)RTAvlPVRemove(&pVM->csam.s.pPageTree, (AVLPVKEY)GCPtr);

    if (pPageRec)
    {
        STAM_COUNTER_INC(&pVM->csam.s.StatNrRemovedPages);

#ifdef CSAM_MONITOR_CODE_PAGES
        if (pPageRec->page.fMonitorActive)
        {
            /* @todo -> this is expensive (cr3 reload)!!!
             * if this happens often, then reuse it instead!!!
             */
            Assert(!fInCSAMCodePageInvalidate);
            STAM_COUNTER_DEC(&pVM->csam.s.StatPageMonitor);
            PGMHandlerVirtualDeregister(pVM, GCPtr);
        }
        if (pPageRec->page.enmTag == CSAM_TAG_PATM)
        {
            /* Make sure the recompiler flushes its cache as this page is no longer monitored. */
            STAM_COUNTER_INC(&pVM->csam.s.StatPageRemoveREMFlush);
            CPUMSetChangedFlags(pVM, CPUM_CHANGED_GLOBAL_TLB_FLUSH);
        }
#endif

#ifdef VBOX_WITH_STATISTICS
        switch (pPageRec->page.enmTag)
        {
        case CSAM_TAG_CSAM:
            STAM_COUNTER_DEC(&pVM->csam.s.StatPageCSAM);
            break;
        case CSAM_TAG_PATM:
            STAM_COUNTER_DEC(&pVM->csam.s.StatPagePATM);
            break;
        case CSAM_TAG_REM:
            STAM_COUNTER_DEC(&pVM->csam.s.StatPageREM);
            break;
        default:
            break; /* to shut up GCC */
        }
#endif

        if (pPageRec->page.pBitmap) MMR3HeapFree(pPageRec->page.pBitmap);
        MMR3HeapFree(pPageRec);
    }
    else
        AssertFailed();

    return VINF_SUCCESS;
}

/**
 * Callback for delayed writes from non-EMT threads
 *
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   cbBuf           How much it's reading/writing.
 */
static DECLCALLBACK(void) CSAMDelayedWriteHandler(PVM pVM, RTGCPTR GCPtr, size_t cbBuf)
{
    int rc = PATMR3PatchWrite(pVM, GCPtr, cbBuf);
    AssertRC(rc);
}

/**
 * #PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   pvPtr           The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) CSAMCodePageWriteHandler(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    int rc;

    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    Log(("CSAMCodePageWriteHandler: write to %VGv size=%d\n", GCPtr, cbBuf));

    if (VM_IS_EMT(pVM))
    {
        rc = PATMR3PatchWrite(pVM, GCPtr, cbBuf);
    }
    else
    {
        /* Queue the write instead otherwise we'll get concurrency issues. */
        /** @note in theory not correct to let it write the data first before disabling a patch!
         *        (if it writes the same data as the patch jump and we replace it with obsolete opcodes)
         */
        Log(("CSAMCodePageWriteHandler: delayed write!\n"));
        rc = VMR3ReqCallEx(pVM, NULL, 0, VMREQFLAGS_NO_WAIT | VMREQFLAGS_VOID,
                           (PFNRT)CSAMDelayedWriteHandler, 3, pVM, GCPtr, cbBuf);
    }
    AssertRC(rc);

    return VINF_PGM_HANDLER_DO_DEFAULT;
}

/**
 * #PF Handler callback for invalidation of virtual access handler ranges.
 *
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest has changed.
 */
static DECLCALLBACK(int) CSAMCodePageInvalidate(PVM pVM, RTGCPTR GCPtr)
{
    fInCSAMCodePageInvalidate = true;
    LogFlow(("CSAMCodePageInvalidate %VGv\n", GCPtr));
    /** @todo We can't remove the page (which unregisters the virtual handler) as we are called from a DoWithAll on the virtual handler tree. Argh. */
    csamFlushPage(pVM, GCPtr, false /* don't remove page! */);
    fInCSAMCodePageInvalidate = false;
    return VINF_SUCCESS;
}

/**
 * Check if the current instruction has already been checked before
 *
 * @returns VBox status code. (trap handled or not)
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Instruction pointer
 * @param   pPage       CSAM patch structure pointer
 */
bool csamIsCodeScanned(PVM pVM, RTGCPTR pInstr, PCSAMPAGE *pPage)
{
    PCSAMPAGEREC pPageRec;
    uint32_t offset;

    STAM_PROFILE_START(&pVM->csam.s.StatTimeCheckAddr, a);

    offset = pInstr & PAGE_OFFSET_MASK;
    pInstr = pInstr & PAGE_BASE_GC_MASK;

    Assert(pPage);

    if (*pPage && (*pPage)->pPageGC == pInstr)
    {
        if ((*pPage)->pBitmap == NULL || ASMBitTest((*pPage)->pBitmap, offset))
        {
            STAM_COUNTER_ADD(&pVM->csam.s.StatNrKnownPagesHC, 1);
            STAM_PROFILE_STOP(&pVM->csam.s.StatTimeCheckAddr, a);
            return true;
        }
        STAM_PROFILE_STOP(&pVM->csam.s.StatTimeCheckAddr, a);
        return false;
    }

    pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)pInstr);
    if (pPageRec)
    {
        if (pPage) *pPage= &pPageRec->page;
        if (pPageRec->page.pBitmap == NULL || ASMBitTest(pPageRec->page.pBitmap, offset))
        {
            STAM_COUNTER_ADD(&pVM->csam.s.StatNrKnownPagesHC, 1);
            STAM_PROFILE_STOP(&pVM->csam.s.StatTimeCheckAddr, a);
            return true;
        }
    }
    else
    {
        if (pPage) *pPage = NULL;
    }
    STAM_PROFILE_STOP(&pVM->csam.s.StatTimeCheckAddr, a);
    return false;
}

/**
 * Mark an instruction in a page as scanned/not scanned
 *
 * @param   pVM         The VM to operate on.
 * @param   pPage       Patch structure pointer
 * @param   pInstr      Instruction pointer
 * @param   opsize      Instruction size
 * @param   fScanned    Mark as scanned or not
 */
static void csamMarkCode(PVM pVM, PCSAMPAGE pPage, RTGCPTR pInstr, uint32_t opsize, bool fScanned)
{
    LogFlow(("csamMarkCodeAsScanned %VGv opsize=%d\n", pInstr, opsize));
    CSAMMarkPage(pVM, pInstr, fScanned);

    /** @todo should recreate empty bitmap if !fScanned */
    if (pPage->pBitmap == NULL)
        return;

    if (fScanned)
    {
        // retn instructions can be scanned more than once
        if (ASMBitTest(pPage->pBitmap, pInstr & PAGE_OFFSET_MASK) == 0)
        {
            pPage->uSize += opsize;
            STAM_COUNTER_ADD(&pVM->csam.s.StatNrInstr, 1);
        }
        if (pPage->uSize >= PAGE_SIZE)
        {
            Log(("Scanned full page (%VGv) -> free bitmap\n", pInstr & PAGE_BASE_GC_MASK));
            MMR3HeapFree(pPage->pBitmap);
            pPage->pBitmap = NULL;
        }
        else
            ASMBitSet(pPage->pBitmap, pInstr & PAGE_OFFSET_MASK);
    }
    else
        ASMBitClear(pPage->pBitmap, pInstr & PAGE_OFFSET_MASK);
}

/**
 * Mark an instruction in a page as scanned/not scanned
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstr      Instruction pointer
 * @param   opsize      Instruction size
 * @param   fScanned    Mark as scanned or not
 */
CSAMR3DECL(int) CSAMR3MarkCode(PVM pVM, RTGCPTR pInstr, uint32_t opsize, bool fScanned)
{
    PCSAMPAGE pPage = 0;

    Assert(!fScanned);   /* other case not implemented. */
    Assert(!PATMIsPatchGCAddr(pVM, pInstr));

    if (csamIsCodeScanned(pVM, pInstr, &pPage) == false)
    {
        Assert(fScanned == true);   /* other case should not be possible */
        return VINF_SUCCESS;
    }

    Log(("CSAMR3MarkCode: %VGv size=%d fScanned=%d\n", pInstr, opsize, fScanned));
    csamMarkCode(pVM, pPage, pInstr, opsize, fScanned);
    return VINF_SUCCESS;
}


/**
 * Scan and analyse code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   Sel         selector
 * @param   pHiddenSel  The hidden selector register.
 * @param   pInstrGC    Instruction pointer
 */
CSAMR3DECL(int) CSAMR3CheckCodeEx(PVM pVM, RTSEL Sel, CPUMSELREGHID *pHiddenSel, RTGCPTR pInstrGC)
{
    if (EMIsRawRing0Enabled(pVM) == false || PATMIsPatchGCAddr(pVM, pInstrGC) == true)
    {
        // No use
        return VINF_SUCCESS;
    }

    if (CSAMIsEnabled(pVM))
    {
        X86EFLAGS fakeflags;

        /* we're not in v86 mode here */
        fakeflags.u32 = 0;

        bool fCode32 = SELMIsSelector32Bit(pVM, fakeflags, Sel, pHiddenSel);

        //assuming 32 bits code for now
        Assert(fCode32); NOREF(fCode32);

        pInstrGC = SELMToFlat(pVM, fakeflags, Sel, pHiddenSel, pInstrGC);

        return CSAMR3CheckCode(pVM, pInstrGC);
    }
    return VINF_SUCCESS;
}

/**
 * Scan and analyse code
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pInstrGC    Instruction pointer (0:32 virtual address)
 */
CSAMR3DECL(int) CSAMR3CheckCode(PVM pVM, RTGCPTR pInstrGC)
{
    int rc;
    PCSAMPAGE pPage = NULL;

    if (EMIsRawRing0Enabled(pVM) == false || PATMIsPatchGCAddr(pVM, pInstrGC) == true)
    {
        // No use
        return VINF_SUCCESS;
    }

    if (CSAMIsEnabled(pVM))
    {
        // Cache record for PATMGCVirtToHCVirt
        CSAMP2GLOOKUPREC cacheRec = {0};

        STAM_PROFILE_START(&pVM->csam.s.StatTime, a);
        rc = csamAnalyseCallCodeStream(pVM, pInstrGC, pInstrGC, true /* 32 bits code */, CSAMR3AnalyseCallback, pPage, &cacheRec);
        STAM_PROFILE_STOP(&pVM->csam.s.StatTime, a);
        if (rc != VINF_SUCCESS)
        {
            Log(("csamAnalyseCodeStream failed with %d\n", rc));
            return rc;
        }
    }
    return VINF_SUCCESS;
}

/**
 * Flush dirty code pages
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
static int csamR3FlushDirtyPages(PVM pVM)
{
    STAM_PROFILE_START(&pVM->csam.s.StatFlushDirtyPages, a);

    for (uint32_t i=0;i<pVM->csam.s.cDirtyPages;i++)
    {
        int          rc;
        PCSAMPAGEREC pPageRec;
        RTGCPTR      GCPtr = pVM->csam.s.pvDirtyBasePage[i];

        GCPtr = GCPtr & PAGE_BASE_GC_MASK;

        /* Notify the recompiler that this page has been changed. */
        REMR3NotifyCodePageChanged(pVM, GCPtr);

        /* Enable write protection again. (use the fault address as it might be an alias) */
        rc = PGMShwModifyPage(pVM, pVM->csam.s.pvDirtyFaultPage[i], 1, 0, ~(uint64_t)X86_PTE_RW);
        Assert(rc == VINF_SUCCESS || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT);

        Log(("CSAMR3FlushDirtyPages: flush %VGv (modifypage rc=%Vrc)\n", pVM->csam.s.pvDirtyBasePage[i], rc));

        pPageRec = (PCSAMPAGEREC)RTAvlPVGet(&pVM->csam.s.pPageTree, (AVLPVKEY)GCPtr);
        if (pPageRec && pPageRec->page.enmTag == CSAM_TAG_REM)
        {
            uint64_t fFlags;

            rc = PGMGstGetPage(pVM, GCPtr, &fFlags, NULL);
            AssertMsg(VBOX_SUCCESS(rc) || rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("rc = %Vrc\n", rc));
            if (    rc == VINF_SUCCESS
                &&  (fFlags & X86_PTE_US))
            {
                /* We don't care about user pages. */
                csamRemovePageRecord(pVM, GCPtr);
                STAM_COUNTER_INC(&pVM->csam.s.StatNrUserPages);
            }
        }
    }
    pVM->csam.s.cDirtyPages = 0;
    STAM_PROFILE_STOP(&pVM->csam.s.StatFlushDirtyPages, a);
    return VINF_SUCCESS;
}

/**
 * Flush potential new code pages
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
static int csamR3FlushCodePages(PVM pVM)
{
    for (uint32_t i=0;i<pVM->csam.s.cPossibleCodePages;i++)
    {
        RTGCPTR      GCPtr = pVM->csam.s.pvPossibleCodePage[i];

        GCPtr = GCPtr & PAGE_BASE_GC_MASK;

        Log(("csamR3FlushCodePages: %VGv\n", GCPtr));
        PGMShwSetPage(pVM, GCPtr, 1, 0);
        /* Resync the page to make sure instruction fetch will fault */
        CSAMMarkPage(pVM, GCPtr, false);
    }
    pVM->csam.s.cPossibleCodePages = 0;
    return VINF_SUCCESS;
}

/**
 * Perform any pending actions
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CSAMR3DECL(int) CSAMR3DoPendingAction(PVM pVM)
{
    csamR3FlushDirtyPages(pVM);
    csamR3FlushCodePages(pVM);

    VM_FF_CLEAR(pVM, VM_FF_CSAM_PENDING_ACTION);
    return VINF_SUCCESS;
}

/**
 * Analyse interrupt and trap gates
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   iGate       Start gate
 * @param   cGates      Number of gates to check
 */
CSAMR3DECL(int) CSAMR3CheckGates(PVM pVM, uint32_t iGate, uint32_t cGates)
{
    uint16_t    cbIDT;
    RTGCPTR     GCPtrIDT = CPUMGetGuestIDTR(pVM, &cbIDT);
    uint32_t    iGateEnd;
    uint32_t    maxGates;
    VBOXIDTE    aIDT[256];
    PVBOXIDTE   pGuestIdte;
    int         rc;

    if (EMIsRawRing0Enabled(pVM) == false)
    {
        /* Enabling interrupt gates only works when raw ring 0 is enabled. */
        //AssertFailed();
        return VINF_SUCCESS;
    }

    /* We only check all gates once during a session */
    if (    !pVM->csam.s.fGatesChecked
        &&   cGates != 256)
        return VINF_SUCCESS;    /* too early */

    /* We only check all gates once during a session */
    if (    pVM->csam.s.fGatesChecked
        &&  cGates != 1)
        return VINF_SUCCESS;    /* ignored */

    Assert(cGates <= 256);
    if (!GCPtrIDT || cGates > 256)
        return VERR_INVALID_PARAMETER;

    if (cGates != 1)
    {
        pVM->csam.s.fGatesChecked = true;
        for (unsigned i=0;i<RT_ELEMENTS(pVM->csam.s.pvCallInstruction);i++)
        {
            RTGCPTR pHandler = pVM->csam.s.pvCallInstruction[i];
            CSAMP2GLOOKUPREC cacheRec = {0};            /* Cache record for PATMGCVirtToHCVirt. */
            PCSAMPAGE pPage = NULL;

            Log(("CSAMCheckGates: checking previous call instruction %VGv\n", pHandler));
            STAM_PROFILE_START(&pVM->csam.s.StatTime, a);
            rc = csamAnalyseCodeStream(pVM, pHandler, pHandler, true, CSAMR3AnalyseCallback, pPage, &cacheRec);
            STAM_PROFILE_STOP(&pVM->csam.s.StatTime, a);
            if (rc != VINF_SUCCESS)
            {
                Log(("CSAMCheckGates: csamAnalyseCodeStream failed with %d\n", rc));
                continue;
            }
        }
    }

    /* Determine valid upper boundary. */
    maxGates   = (cbIDT+1) / sizeof(VBOXIDTE);
    Assert(iGate < maxGates);
    if (iGate > maxGates)
        return VERR_INVALID_PARAMETER;

    if (iGate + cGates > maxGates)
        cGates = maxGates - iGate;

    GCPtrIDT   = GCPtrIDT + iGate * sizeof(VBOXIDTE);
    iGateEnd   = iGate + cGates;

    STAM_PROFILE_START(&pVM->csam.s.StatCheckGates, a);

    /*
     * Get IDT entries.
     */
    if (PAGE_ADDRESS(GCPtrIDT) == PAGE_ADDRESS(GCPtrIDT+cGates*sizeof(VBOXIDTE)))
    {
        /* Just convert the IDT address to a HC pointer. The whole IDT fits in one page. */
        rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrIDT, (PRTHCPTR)&pGuestIdte);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgRC(rc, ("Failed to read IDTE! rc=%Vrc\n", rc));
            STAM_PROFILE_STOP(&pVM->csam.s.StatCheckGates, a);
            return rc;
        }
    }
    else
    {
        /* Slow method when it crosses a page boundary. */
        rc = PGMPhysReadGCPtr(pVM, aIDT, GCPtrIDT,  cGates*sizeof(VBOXIDTE));
        if (VBOX_FAILURE(rc))
        {
            AssertMsgRC(rc, ("Failed to read IDTE! rc=%Vrc\n", rc));
            STAM_PROFILE_STOP(&pVM->csam.s.StatCheckGates, a);
            return rc;
        }
        pGuestIdte = &aIDT[0];
    }

    for (/*iGate*/; iGate<iGateEnd; iGate++, pGuestIdte++)
    {
        Assert(TRPMR3GetGuestTrapHandler(pVM, iGate) == TRPM_INVALID_HANDLER);

        if (    pGuestIdte->Gen.u1Present
            &&  (pGuestIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_TRAP_32 || pGuestIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_INT_32)
            &&  (pGuestIdte->Gen.u2DPL == 3 || pGuestIdte->Gen.u2DPL == 0)
           )
        {
            RTGCPTR pHandler;
            CSAMP2GLOOKUPREC cacheRec = {0};            /* Cache record for PATMGCVirtToHCVirt. */
            PCSAMPAGE pPage = NULL;
            X86EFLAGS fakeflags;
            SELMSELINFO selInfo;

            /* we're not in v86 mode here */
            fakeflags.u32 = 0;

            pHandler = (pGuestIdte->Gen.u16OffsetHigh << 16) | pGuestIdte->Gen.u16OffsetLow;
            pHandler = SELMToFlat(pVM, fakeflags, pGuestIdte->Gen.u16SegSel, 0, pHandler);

            rc = SELMR3GetSelectorInfo(pVM, pGuestIdte->Gen.u16SegSel, &selInfo);
            if (    VBOX_FAILURE(rc)
                ||  selInfo.GCPtrBase != 0
                ||  selInfo.cbLimit != ~0U
               )
            {
                /* Refuse to patch a handler whose idt cs selector isn't wide open. */
                Log(("CSAMCheckGates: check gate %d failed due to rc %Vrc GCPtrBase=%VGv limit=%x\n", iGate, rc, selInfo.GCPtrBase, selInfo.cbLimit));
                continue;
            }
            

            if (pGuestIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_TRAP_32)
            {
                Log(("CSAMCheckGates: check trap gate %d at %04X:%08X (flat %VGv)\n", iGate, pGuestIdte->Gen.u16SegSel, (pGuestIdte->Gen.u16OffsetHigh << 16) | pGuestIdte->Gen.u16OffsetLow, pHandler));
            }
            else
            {
                Log(("CSAMCheckGates: check interrupt gate %d at %04X:%08X (flat %VGv)\n", iGate, pGuestIdte->Gen.u16SegSel, (pGuestIdte->Gen.u16OffsetHigh << 16) | pGuestIdte->Gen.u16OffsetLow, pHandler));
            }

            STAM_PROFILE_START(&pVM->csam.s.StatTime, a);
            rc = csamAnalyseCodeStream(pVM, pHandler, pHandler, true, CSAMR3AnalyseCallback, pPage, &cacheRec);
            STAM_PROFILE_STOP(&pVM->csam.s.StatTime, a);
            if (rc != VINF_SUCCESS)
            {
                Log(("CSAMCheckGates: csamAnalyseCodeStream failed with %d\n", rc));
                continue;
            }
            /* OpenBSD guest specific patch test. */
            if (iGate >= 0x20)
            {
                PCPUMCTX    pCtx;
                DISCPUSTATE cpu;
                RTGCUINTPTR aOpenBsdPushCSOffset[3] = {0x03,       /* OpenBSD 3.7 & 3.8 */
                                                       0x2B,       /* OpenBSD 4.0 installation ISO */
                                                       0x2F};      /* OpenBSD 4.0 after install */

                rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
                AssertRC(rc);   /* can't fail */

                for (unsigned i=0;i<ELEMENTS(aOpenBsdPushCSOffset);i++)
                {
                    rc = CPUMR3DisasmInstrCPU(pVM, pCtx, pHandler - aOpenBsdPushCSOffset[i], &cpu, NULL);
                    if (    rc == VINF_SUCCESS
                        &&  cpu.pCurInstr->opcode == OP_PUSH
                        &&  cpu.pCurInstr->param1 == OP_PARM_REG_CS)
                    {
                        rc = PATMR3InstallPatch(pVM, pHandler - aOpenBsdPushCSOffset[i], PATMFL_CODE32 | PATMFL_GUEST_SPECIFIC);
                        if (VBOX_SUCCESS(rc))
                            Log(("Installed OpenBSD interrupt handler prefix instruction (push cs) patch\n"));
                    }
                }
            }

            /* Trap gates and certain interrupt gates. */
            uint32_t fPatchFlags = PATMFL_CODE32 | PATMFL_IDTHANDLER;

            if (pGuestIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_TRAP_32)
                fPatchFlags |= PATMFL_TRAPHANDLER;
            else
                fPatchFlags |= PATMFL_INTHANDLER;

            switch (iGate) {
            case 8:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 17:
                fPatchFlags |= PATMFL_TRAPHANDLER_WITH_ERRORCODE;
                break;
            default:
                /* No error code. */
                break;
            }

            Log(("Installing %s gate handler for 0x%X at %VGv\n", (pGuestIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_TRAP_32) ? "trap" : "intr", iGate, pHandler));

            rc = PATMR3InstallPatch(pVM, pHandler, fPatchFlags);
            if (VBOX_SUCCESS(rc) || rc == VERR_PATM_ALREADY_PATCHED)
            {
                Log(("Gate handler 0x%X is SAFE!\n", iGate));

                RTGCPTR pNewHandlerGC = PATMR3QueryPatchGCPtr(pVM, pHandler);
                if (pNewHandlerGC)
                {
                    rc = TRPMR3SetGuestTrapHandler(pVM, iGate, pNewHandlerGC);
                    if (VBOX_FAILURE(rc))
                        Log(("TRPMR3SetGuestTrapHandler %d failed with %Vrc\n", iGate, rc));
                }
            }
        }
    } /* for */
    STAM_PROFILE_STOP(&pVM->csam.s.StatCheckGates, a);
    return VINF_SUCCESS;
}

/**
 * Record previous call instruction addresses
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCPtrCall   Call address
 */
CSAMR3DECL(int) CSAMR3RecordCallAddress(PVM pVM, RTGCPTR GCPtrCall)
{
    for (unsigned i=0;i<RT_ELEMENTS(pVM->csam.s.pvCallInstruction);i++)
    {
        if (pVM->csam.s.pvCallInstruction[i] == GCPtrCall)
            return VINF_SUCCESS;
    }

    Log(("CSAMR3RecordCallAddress %VGv\n", GCPtrCall));

    pVM->csam.s.pvCallInstruction[pVM->csam.s.iCallInstruction++] = GCPtrCall;
    if (pVM->csam.s.iCallInstruction >= RT_ELEMENTS(pVM->csam.s.pvCallInstruction))
        pVM->csam.s.iCallInstruction = 0;

    return VINF_SUCCESS;
}


/**
 * Query CSAM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
CSAMR3DECL(int) CSAMR3IsEnabled(PVM pVM)
{
    return pVM->fCSAMEnabled;
}

#ifdef VBOX_WITH_DEBUGGER
/**
 * The '.csamoff' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) csamr3CmdOff(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");

    CSAMDisableScanning(pVM);
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "CSAM Scanning disabled\n");
}

/**
 * The '.csamon' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) csamr3CmdOn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");

    CSAMEnableScanning(pVM);
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "CSAM Scanning enabled\n");
}
#endif
