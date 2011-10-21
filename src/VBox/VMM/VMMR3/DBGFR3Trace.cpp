/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Tracing.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgftrace.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/mm.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <iprt/trace.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void) dbgfR3TraceInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/**
 * Initializes the tracing.
 *
 * @returns VBox status code
 * @param   pVM                 The VM handle.
 */
static int dbgfR3TraceEnable(PVM pVM, uint32_t cbEntry, uint32_t cEntries)
{
    /*
     * Don't enable it twice.
     */
    if (pVM->hTraceBufR3 != NIL_RTTRACEBUF)
        return VERR_ALREADY_EXISTS;

    /*
     * Resolve default parameter values.
     */
    int rc;
    if (!cbEntry)
    {
        rc = CFGMR3QueryU32Def(CFGMR3GetChild(CFGMR3GetRoot(pVM), "DBGF"), "TraceBufEntrySize", &cbEntry, 128);
        AssertRCReturn(rc, rc);
    }
    if (!cEntries)
    {
        rc = CFGMR3QueryU32Def(CFGMR3GetChild(CFGMR3GetRoot(pVM), "DBGF"), "TraceBufEntries", &cEntries, 4096);
        AssertRCReturn(rc, rc);
    }

    /*
     * Figure the required size.
     */
    RTTRACEBUF  hTraceBuf;
    size_t      cbBlock = 0;
    rc = RTTraceBufCarve(&hTraceBuf, cEntries, cbEntry, 0 /*fFlags*/, NULL, &cbBlock);
    if (rc != VERR_BUFFER_OVERFLOW)
    {
        AssertReturn(!RT_SUCCESS_NP(rc), VERR_INTERNAL_ERROR_4);
        return rc;
    }

    /*
     * Allocate a hyper heap block and carve a trace buffer out of it.
     *
     * Note! We ASSUME that the returned trace buffer handle has the same value
     *       as the heap block.
     */
    cbBlock = RT_ALIGN_Z(cbBlock, PAGE_SIZE);
    void *pvBlock;
    rc = MMR3HyperAllocOnceNoRel(pVM, cbBlock, PAGE_SIZE, MM_TAG_DBGF, &pvBlock);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTTraceBufCarve(&hTraceBuf, cEntries, cbEntry, 0 /*fFlags*/, pvBlock, &cbBlock);
    AssertRCReturn(rc, rc);
    AssertRelease(hTraceBuf == (RTTRACEBUF)pvBlock && (void *)hTraceBuf == pvBlock);

    pVM->hTraceBufR3 = hTraceBuf;
    pVM->hTraceBufR0 = MMHyperCCToR0(pVM, hTraceBuf);
    pVM->hTraceBufRC = MMHyperCCToRC(pVM, hTraceBuf);
    return VINF_SUCCESS;
}


/**
 * Initializes the tracing.
 *
 * @returns VBox status code
 * @param   pVM                 The VM handle.
 */
int dbgfR3TraceInit(PVM pVM)
{
    /*
     * Initialize the trace buffer handles.
     */
    Assert(NIL_RTTRACEBUF == (RTTRACEBUF)NULL);
    pVM->hTraceBufR3 = NIL_RTTRACEBUF;
    pVM->hTraceBufRC = NIL_RTRCPTR;
    pVM->hTraceBufR0 = NIL_RTR0PTR;

    /*
     * Check the config and enable tracing if requested.
     */
#if defined(DEBUG) || defined(RTTRACE_ENABLED)
    bool const  fDefault = true;
#else
    bool const  fDefault = false;
#endif
    bool        fTracingEnabled;
    int rc = CFGMR3QueryBoolDef(CFGMR3GetChild(CFGMR3GetRoot(pVM), "DBGF"), "TracingEnabled",
                                &fTracingEnabled, fDefault);
    AssertRCReturn(rc, rc);
    if (fTracingEnabled)
        rc = dbgfR3TraceEnable(pVM, 0, 0);

    /*
     * Register a debug info item that will dump the trace buffer content.
     */
    if (RT_SUCCESS(rc))
        rc = DBGFR3InfoRegisterInternal(pVM, "tracebuf", "Display the trace buffer content. No arguments.", dbgfR3TraceInfo);

    return rc;
}


/**
 * Terminates the tracing.
 *
 * @param   pVM                 The VM handle.
 */
void dbgfR3TraceTerm(PVM pVM)
{
    /* nothing to do */
    NOREF(pVM);
}


/**
 * Relocates the trace buffer handle in RC.
 *
 * @param   pVM                 The VM handle.
 */
void dbgfR3TraceRelocate(PVM pVM)
{
    if (pVM->hTraceBufR3 != NIL_RTTRACEBUF)
        pVM->hTraceBufRC = MMHyperCCToRC(pVM, pVM->hTraceBufR3);
}


/**
 * @callback_method_impl{FNRTTRACEBUFCALLBACK}
 */
static DECLCALLBACK(int)
dbgfR3TraceInfoDumpEntry(RTTRACEBUF hTraceBuf, uint32_t iEntry, uint64_t NanoTS, RTCPUID idCpu, const char *pszMsg, void *pvUser)
{
    PCDBGFINFOHLP pHlp = (PCDBGFINFOHLP)pvUser;
    pHlp->pfnPrintf(pHlp, "#%04u/%'llu/%02x: %s\n", iEntry, NanoTS, idCpu, pszMsg);
    NOREF(hTraceBuf);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNDBGFHANDLERINT, Info handler for displaying the trace buffer content.}
 */
static DECLCALLBACK(void) dbgfR3TraceInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RTTRACEBUF hTraceBuf = pVM->hTraceBufR3;
    if (hTraceBuf == NIL_RTTRACEBUF)
        pHlp->pfnPrintf(pHlp, "Tracing is disable\n");
    else
    {
        pHlp->pfnPrintf(pHlp, "Trace buffer %p - %u entries of %u bytes\n",
                        hTraceBuf, RTTraceBufGetEntryCount(hTraceBuf), RTTraceBufGetEntrySize(hTraceBuf));
        RTTraceBufEnumEntries(hTraceBuf, dbgfR3TraceInfoDumpEntry, (void *)pHlp);
    }
    NOREF(pszArgs);
}

