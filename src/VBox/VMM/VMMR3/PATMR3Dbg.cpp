/* $Id$ */
/** @file
 * PATM - Dynamic Guest OS Patching Manager, Debugger Related Parts.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/vmm/patm.h>
#include <VBox/vmm/hm.h>
#include "PATMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/dbg.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Adds a structure member to a debug (pseudo) module as a symbol. */
#define ADD_MEMBER(a_hDbgMod, a_Struct, a_Member, a_pszName) \
        do { \
            rc = RTDbgModSymbolAdd(hDbgMod, a_pszName, 0 /*iSeg*/, RT_OFFSETOF(a_Struct, a_Member),  \
                                   RT_SIZEOFMEMB(a_Struct, a_Member), 0 /*fFlags*/, NULL /*piOrdinal*/); \
            AssertRC(rc); \
        } while (0)



void patmR3DbgInit(PVM pVM)
{
    pVM->patm.s.hDbgModPatchMem = NIL_RTDBGMOD;
}


void patmR3DbgTerm(PVM pVM)
{
    if (pVM->patm.s.hDbgModPatchMem != NIL_RTDBGMOD)
    {
        RTDbgModRelease(pVM->patm.s.hDbgModPatchMem);
        pVM->patm.s.hDbgModPatchMem = NIL_RTDBGMOD;
    }
}


void patmR3DbgReset(PVM pVM)
{
    if (pVM->patm.s.hDbgModPatchMem != NIL_RTDBGMOD)
    {
        RTDbgModRemoveAll(pVM->patm.s.hDbgModPatchMem, true);
    }
}



/**
 * Populate DBGF_AS_RC with PATM symbols.
 *
 * Called by dbgfR3AsLazyPopulate when DBGF_AS_RC or DBGF_AS_RC_AND_GC_GLOBAL is
 * accessed for the first time.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   hDbgAs              The DBGF_AS_RC address space handle.
 */
VMMR3_INT_DECL(void) PATMR3DbgPopulateAddrSpace(PVM pVM, RTDBGAS hDbgAs)
{
    AssertReturnVoid(!HMIsEnabled(pVM));

    /*
     * Add a fake debug module for the PATMGCSTATE structure.
     */
    RTDBGMOD hDbgMod;
    int rc = RTDbgModCreate(&hDbgMod, "patmgcstate", sizeof(PATMGCSTATE), 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uVMFlags,                  "uVMFlags");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uPendingAction,            "uPendingAction");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uPatchCalls,               "uPatchCalls");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uScratch,                  "uScratch");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uIretEFlags,               "uIretEFlags");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uIretCS,                   "uIretCS");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, uIretEIP,                  "uIretEIP");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Psp,                       "Psp");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, fPIF,                      "fPIF");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, GCPtrInhibitInterrupts,    "GCPtrInhibitInterrupts");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, GCCallPatchTargetAddr,     "GCCallPatchTargetAddr");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, GCCallReturnAddr,          "GCCallReturnAddr");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uEAX,              "Restore.uEAX");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uECX,              "Restore.uECX");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uEDI,              "Restore.uEDI");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.eFlags,            "Restore.eFlags");
        ADD_MEMBER(hDbgMod, PATMGCSTATE, Restore.uFlags,            "Restore.uFlags");

        rc = RTDbgAsModuleLink(hDbgAs, hDbgMod, pVM->patm.s.pGCStateGC, 0 /*fFlags/*/);
        AssertLogRelRC(rc);
        RTDbgModRelease(hDbgMod);
    }

    /*
     * Add a fake debug module for the patches.
     */
    rc = RTDbgModCreate(&hDbgMod, "patches", pVM->patm.s.cbPatchMem, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        /** @todo add global functions  and  all existing patches. */

        rc = RTDbgAsModuleLink(hDbgAs, hDbgMod, pVM->patm.s.pGCStateGC, 0 /*fFlags/*/);
        AssertLogRelRC(rc);
        pVM->patm.s.hDbgModPatchMem = hDbgMod;
    }
}


