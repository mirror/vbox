/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Sample report creation.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_dbgf_flow DBGFR3SampleReport - Sample Report Interface
 *
 * @todo
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vmm/mm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/list.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** Pointer to a single sample frame. */
typedef struct DBGFSAMPLEFRAME *PDBGFSAMPLEFRAME;

/**
 * Frame information.
 */
typedef struct DBGFSAMPLEFRAME
{
    /** Frame address. */
    DBGFADDRESS                     AddrFrame;
    /** Number of times this frame was encountered. */
    uint64_t                        cSamples;
    /** Pointer to the array of frames below in the call stack. */
    PDBGFSAMPLEFRAME                paFrames;
    /** Number of valid entries in the frams array. */
    uint64_t                        cFramesValid;
    /** Maximum number of entries in the frames array. */
    uint64_t                        cFramesMax;
} DBGFSAMPLEFRAME;
typedef const DBGFSAMPLEFRAME *PCDBGFSAMPLEFRAME;


/**
 * Per VCPU sample report data.
 */
typedef struct DBGFSAMPLEREPORTVCPU
{
    /** The root frame. */
    DBGFSAMPLEFRAME                 FrameRoot;
} DBGFSAMPLEREPORTVCPU;
/** Pointer to the per VCPU sample report data. */
typedef DBGFSAMPLEREPORTVCPU *PDBGFSAMPLEREPORTVCPU;
/** Pointer to const per VCPU sample report data. */
typedef const DBGFSAMPLEREPORTVCPU *PCDBGFSAMPLEREPORTVCPU;


/**
 * Internal sample report instance data.
 */
typedef struct DBGFSAMPLEREPORTINT
{
    /** References hold for this trace module. */
    volatile uint32_t                cRefs;
    /** The user mode VM handle. */
    PUVM                             pUVM;
    /** Flags passed during report creation. */
    uint32_t                         fFlags;
    /** The timer handle for the sample report collector. */
    PRTTIMER                         hTimer;
    /** Array of per VCPU samples collected. */
    DBGFSAMPLEREPORTVCPU             aCpus[1];
} DBGFSAMPLEREPORTINT;
/** Pointer to a const internal trace module instance data. */
typedef DBGFSAMPLEREPORTINT *PDBGFSAMPLEREPORTINT;
/** Pointer to a const internal trace module instance data. */
typedef const DBGFSAMPLEREPORTINT *PCDBGFSAMPLEREPORTINT;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Destroys the given sample report freeing all allocated resources.
 *
 * @returns nothing.
 * @param   pThis                   The sample report instance data.
 */
static void dbgfR3SampleReportDestroy(PDBGFSAMPLEREPORTINT pThis)
{
    int rc = RTTimerDestroy(pThis->hTimer); AssertRC(rc); RT_NOREF(rc);
    MMR3HeapFree(pThis);
}


static PDBGFSAMPLEFRAME dbgfR3SampleReportFrameFindByAddr(PCDBGFSAMPLEFRAME pFrame, PCDBGFADDRESS pAddr)
{
    for (uint32_t i = 0; i < pFrame->cFramesValid; i++)
        if (!memcmp(pAddr, &pFrame->paFrames[i].AddrFrame, sizeof(*pAddr)))
            return &pFrame->paFrames[i];

    return NULL;
}


static PDBGFSAMPLEFRAME dbgfR3SampleReportAddFrameByAddr(PUVM pUVM, PDBGFSAMPLEFRAME pFrame, PCDBGFADDRESS pAddr)
{
    if (pFrame->cFramesValid == pFrame->cFramesMax)
    {
        uint32_t cFramesMaxNew = pFrame->cFramesMax + 10;
        PDBGFSAMPLEFRAME paFramesNew = NULL;
        if (pFrame->paFrames)
            paFramesNew = (PDBGFSAMPLEFRAME)MMR3HeapRealloc(pFrame->paFrames, sizeof(*pFrame->paFrames) * cFramesMaxNew);
        else
            paFramesNew = (PDBGFSAMPLEFRAME)MMR3HeapAllocZU(pUVM, MM_TAG_DBGF, sizeof(*pFrame->paFrames) * cFramesMaxNew);

        if (!paFramesNew)
            return NULL;

        pFrame->cFramesMax = cFramesMaxNew;
        pFrame->paFrames   = paFramesNew;
    }

    PDBGFSAMPLEFRAME pFrameNew = &pFrame->paFrames[pFrame->cFramesValid++];
    pFrameNew->AddrFrame    = *pAddr;
    pFrameNew->cSamples     = 1;
    pFrameNew->paFrames     = NULL;
    pFrameNew->cFramesMax   = 0;
    pFrameNew->cFramesValid = 0;
    return pFrameNew;
}


/**
 * Worker for dbgfR3SampleReportTakeSample(), doing the work in an EMT rendezvous point on
 * each CCPU.
 *
 * @returns Strict VBox status code.
 * @param   pVM                     The VM instance data.
 * @param   pVCpu                   The virtual CPU we execute on.
 * @param   pvUser                  Opaque user data.
 */
static DECLCALLBACK(VBOXSTRICTRC) dbgfR3SampleReportSample(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    RT_NOREF(pVM);
    PDBGFSAMPLEREPORTINT pThis = (PDBGFSAMPLEREPORTINT)pvUser;

    PCDBGFSTACKFRAME pFrameFirst;
    int rc = DBGFR3StackWalkBegin(pThis->pUVM, pVCpu->idCpu, DBGFCODETYPE_GUEST, &pFrameFirst);
    if (RT_SUCCESS(rc))
    {
        /* As we want the final call stacks to be from top to bottom we have to reverse the addresses. */
        DBGFADDRESS aFrameAddresses[512];
        uint32_t idxFrameMax = 0;

        PDBGFSAMPLEFRAME pFrame = &pThis->aCpus[pVCpu->idCpu].FrameRoot;
        pFrame->cSamples++;

        for (PCDBGFSTACKFRAME pStackFrame = pFrameFirst;
             pStackFrame && idxFrameMax < RT_ELEMENTS(aFrameAddresses);
             pStackFrame = DBGFR3StackWalkNext(pStackFrame))
        {
            if (pThis->fFlags & DBGF_SAMPLE_REPORT_F_UPSIDE_DOWN)
            {
                PDBGFSAMPLEFRAME pFrameNext = dbgfR3SampleReportFrameFindByAddr(pFrame, &pStackFrame->AddrPC);
                if (!pFrameNext)
                    pFrameNext = dbgfR3SampleReportAddFrameByAddr(pThis->pUVM, pFrame, &pStackFrame->AddrPC);
                else
                    pFrameNext->cSamples++;

                pFrame = pFrameNext;
            }
            else
                aFrameAddresses[idxFrameMax++] = pStackFrame->AddrPC;
        }

        DBGFR3StackWalkEnd(pFrameFirst);

        if (!(pThis->fFlags & DBGF_SAMPLE_REPORT_F_UPSIDE_DOWN))
        {
            /* Walk the frame stack backwards and construct the call stack. */
            while (idxFrameMax--)
            {
                PDBGFSAMPLEFRAME pFrameNext = dbgfR3SampleReportFrameFindByAddr(pFrame, &aFrameAddresses[idxFrameMax]);
                if (!pFrameNext)
                    pFrameNext = dbgfR3SampleReportAddFrameByAddr(pThis->pUVM, pFrame, &aFrameAddresses[idxFrameMax]);
                else
                    pFrameNext->cSamples++;

                pFrame = pFrameNext;
            }
        }
    }
    else
        LogRelMax(10, ("Sampling guest stack on VCPU %u failed with rc=%Rrc\n", pVCpu->idCpu, rc));

    return VINF_SUCCESS;
}


/**
 * @copydoc FNRTTIMER
 */
static DECLCALLBACK(void) dbgfR3SampleReportTakeSample(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    RT_NOREF(pTimer, iTick);
    PDBGFSAMPLEREPORTINT pThis = (PDBGFSAMPLEREPORTINT)pvUser;

    int rc = VMMR3EmtRendezvous(pThis->pUVM->pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ALL_AT_ONCE,
                                 dbgfR3SampleReportSample, pThis);
    RT_NOREF(rc);
}


/**
 * Creates a new sample report instance for the specified VM.
 *
 * @returns VBox status code.
 * @param   pUVM                    The usermode VM handle.
 * @param   cSampleIntervalUs       The sample interval in micro seconds.
 * @param   fFlags                  Combination of DBGF_SAMPLE_REPORT_F_XXX.
 * @param   phSample                Where to return the handle to the sample report on success.
 */
VMMR3DECL(int) DBGFR3SampleReportCreate(PUVM pUVM, uint32_t cSampleIntervalUs, uint32_t fFlags, PDBGFSAMPLEREPORT phSample)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(!(fFlags & ~DBGF_AMPLE_REPORT_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phSample, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    PDBGFSAMPLEREPORTINT pThis = (PDBGFSAMPLEREPORTINT)MMR3HeapAllocZU(pUVM, MM_TAG_DBGF,
                                                                       RT_UOFFSETOF_DYN(DBGFSAMPLEREPORTINT, aCpus[pUVM->cCpus]));
    if (RT_LIKELY(pThis))
    {
        pThis->cRefs  = 1;
        pThis->pUVM   = pUVM;
        pThis->fFlags = fFlags;

        for (uint32_t i = 0; i < pUVM->cCpus; i++)
        {
            pThis->aCpus[i].FrameRoot.paFrames     = NULL;
            pThis->aCpus[i].FrameRoot.cSamples     = 0;
            pThis->aCpus[i].FrameRoot.cFramesValid = 0;
            pThis->aCpus[i].FrameRoot.cFramesMax   = 0;
        }

        rc = RTTimerCreateEx(&pThis->hTimer, (uint64_t)cSampleIntervalUs * 1000,
                             RTTIMER_FLAGS_CPU_ANY | RTTIMER_FLAGS_HIGH_RES,
                             dbgfR3SampleReportTakeSample, pThis);
        if (RT_SUCCESS(rc))
        {
            *phSample = pThis;
            return VINF_SUCCESS;
        }

        MMR3HeapFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Retains a reference to the given sample report handle.
 *
 * @returns New reference count.
 * @param   hSample                 Sample report handle.
 */
VMMR3DECL(uint32_t) DBGFR3SampleReportRetain(DBGFSAMPLEREPORT hSample)
{
    PDBGFSAMPLEREPORTINT pThis = hSample;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


/**
 * Release a given sample report handle reference.
 *
 * @returns New reference count, on 0 the sample report instance is destroyed.
 * @param   hSample                 Sample report handle.
 */
VMMR3DECL(uint32_t) DBGFR3SampleReportRelease(DBGFSAMPLEREPORT hSample)
{
    PDBGFSAMPLEREPORTINT pThis = hSample;
    if (!pThis)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        dbgfR3SampleReportDestroy(pThis);
    return cRefs;
}


/**
 * Starts collecting samples for the given sample report.
 *
 * @returns VBox status code.
 * @param   hSample                 Sample report handle.
 */
VMMR3DECL(int) DBGFR3SampleReportStart(DBGFSAMPLEREPORT hSample)
{
    PDBGFSAMPLEREPORTINT pThis = hSample;

    /* Try to detect the guest OS first so we can get more accurate symbols and addressing. */
    char szName[64];
    int rc = DBGFR3OSDetect(pThis->pUVM, &szName[0], sizeof(szName));
    if (RT_SUCCESS(rc))
    {
        LogRel(("DBGF/SampleReport: Detected guest OS \"%s\"\n", szName));
        char szVersion[512];
        int rc2 = DBGFR3OSQueryNameAndVersion(pThis->pUVM, NULL, 0, szVersion, sizeof(szVersion));
        if (RT_SUCCESS(rc2))
            LogRel(("DBGF/SampleReport: Version : \"%s\"\n", szVersion));
    }
    else
        LogRel(("DBGF/SampleReport: Couldn't detect guest operating system rc=%Rcr\n", rc));

    rc = RTTimerStart(pThis->hTimer, 0 /*u64First*/);
    return rc;
}


static void dbgfR3SampleReportDumpFrame(PUVM pUVM, PCDBGFSAMPLEFRAME pFrame, uint32_t idxFrame)
{
    RTGCINTPTR offDisp;
    RTDBGMOD hMod;
    RTDBGSYMBOL SymPC;

    if (DBGFR3AddrIsValid(pUVM, &pFrame->AddrFrame))
    {
        int rc = DBGFR3AsSymbolByAddr(pUVM, DBGF_AS_GLOBAL, &pFrame->AddrFrame,
                                      RTDBGSYMADDR_FLAGS_LESS_OR_EQUAL | RTDBGSYMADDR_FLAGS_SKIP_ABS_IN_DEFERRED,
                                      &offDisp, &SymPC, &hMod);
        if (RT_SUCCESS(rc))
        {
            const char *pszModName = hMod != NIL_RTDBGMOD ? RTDbgModName(hMod) : NULL;

            LogRel(("%*s%RU64 %s+%llx (%s) [%RGv]\n", idxFrame * 4, " ",
                                                      pFrame->cSamples,
                                                      SymPC.szName, offDisp,
                                                      hMod ? pszModName : "",
                                                      pFrame->AddrFrame.FlatPtr));
            RTDbgModRelease(hMod);
        }
        else
            LogRel(("%*s%RU64 %RGv\n", idxFrame * 4, " ", pFrame->cSamples, pFrame->AddrFrame.FlatPtr));
    }
    else
        LogRel(("%*s%RU64 %RGv\n", idxFrame * 4, " ", pFrame->cSamples, pFrame->AddrFrame.FlatPtr));

    for (uint32_t i = 0; i < pFrame->cFramesValid; i++)
        dbgfR3SampleReportDumpFrame(pUVM, &pFrame->paFrames[i], idxFrame + 1);
}


/**
 * Stops collecting samples for the given sample report.
 *
 * @returns VBox status code.
 * @param   hSample                 Sample report handle.
 */
VMMR3DECL(int) DBGFR3SampleReportStop(DBGFSAMPLEREPORT hSample)
{
    PDBGFSAMPLEREPORTINT pThis = hSample;

    int rc = RTTimerStop(pThis->hTimer);

#if 1
    /* Some early dump code. */
    for (uint32_t i = 0; i < pThis->pUVM->cCpus; i++)
    {
        PCDBGFSAMPLEREPORTVCPU pSampleVCpu = &pThis->aCpus[i];

        LogRel(("Sample report for vCPU %u:\n", i));
        dbgfR3SampleReportDumpFrame(pThis->pUVM, &pSampleVCpu->FrameRoot, 0);
    }
#endif

    return rc;
}


/**
 * Dumps the current sample report to the given file.
 *
 * @returns VBox status code.
 * @param   hSample                 Sample report handle.
 * @param   pszFilename             The filename to dump the report to.
 */
VMMR3DECL(int) DBGFR3SampleReportDumpToFile(DBGFSAMPLEREPORT hSample, const char *pszFilename)
{
    RT_NOREF(hSample, pszFilename);
    return VINF_SUCCESS;
}
