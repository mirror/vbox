/** $Id$ */
/** @file
 * DBGC - Debugger Console, Command Worker Routines.
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
#define LOG_GROUP LOG_GROUP_DBGC
#include <VBox/dbg.h>
#include <VBox/dbgf.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/dis.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>

#include "DBGCInternal.h"



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      V a r i a b l e   M a n i p u l a t i o n
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



/** @todo move me!*/
void dbgcVarSetGCFlat(PDBGCVAR pVar, RTGCPTR GCFlat)
{
    if (pVar)
    {
        pVar->enmType  = DBGCVAR_TYPE_GC_FLAT;
        pVar->u.GCFlat = GCFlat;
        pVar->enmRangeType = DBGCVAR_RANGE_NONE;
        pVar->u64Range  = 0;
    }
}


/** @todo move me!*/
void dbgcVarSetGCFlatByteRange(PDBGCVAR pVar, RTGCPTR GCFlat, uint64_t cb)
{
    if (pVar)
    {
        pVar->enmType  = DBGCVAR_TYPE_GC_FLAT;
        pVar->u.GCFlat = GCFlat;
        pVar->enmRangeType = DBGCVAR_RANGE_BYTES;
        pVar->u64Range  = cb;
    }
}


/** @todo move me!*/
void dbgcVarSetVar(PDBGCVAR pVar, PCDBGCVAR pVar2)
{
    if (pVar)
    {
        if (pVar2)
            *pVar = *pVar2;
        else
        {
            pVar->enmType = DBGCVAR_TYPE_UNKNOWN;
            memset(&pVar->u, 0, sizeof(pVar->u));
            pVar->enmRangeType = DBGCVAR_RANGE_NONE;
            pVar->u64Range  = 0;
        }
    }
}


/** @todo move me!*/
void dbgcVarSetByteRange(PDBGCVAR pVar, uint64_t cb)
{
    if (pVar)
    {
        pVar->enmRangeType = DBGCVAR_RANGE_BYTES;
        pVar->u64Range  = cb;
    }
}


/** @todo move me!*/
void dbgcVarSetNoRange(PDBGCVAR pVar)
{
    if (pVar)
    {
        pVar->enmRangeType = DBGCVAR_RANGE_NONE;
        pVar->u64Range  = 0;
    }
}


/**
 * Converts a DBGC variable to a DBGF address.
 *
 * @returns VBox status code.
 * @param   pDbgc       The DBGC instance.
 * @param   pVar        The variable.
 * @param   pAddress    Where to store the address.
 */
int dbgcVarToDbgfAddr(PDBGC pDbgc, PCDBGCVAR pVar, PDBGFADDRESS pAddress)
{
    AssertReturn(pVar, VERR_INVALID_PARAMETER);
    switch (pVar->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            DBGFR3AddrFromFlat(pDbgc->pVM, pAddress, pVar->u.GCFlat);
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_NUMBER:
            DBGFR3AddrFromFlat(pDbgc->pVM, pAddress, (RTGCUINTPTR)pVar->u.u64Number);
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_GC_FAR:
            return DBGFR3AddrFromSelOff(pDbgc->pVM, pAddress, pVar->u.GCFar.sel, pVar->u.GCFar.sel);

        case DBGCVAR_TYPE_STRING:
        case DBGCVAR_TYPE_SYMBOL:
        {
            DBGCVAR Var;
            int rc = pDbgc->CmdHlp.pfnEval(&pDbgc->CmdHlp, &Var, "%%(%DV)", pVar);
            if (VBOX_FAILURE(rc))
                return rc;
            return dbgcVarToDbgfAddr(pDbgc, &Var, pAddress);
        }

        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        default:
            return VERR_PARSE_CONVERSION_FAILED;
    }
}



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      B r e a k p o i n t   M a n a g e m e n t
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Adds a breakpoint to the DBGC breakpoint list.
 */
int dbgcBpAdd(PDBGC pDbgc, RTUINT iBp, const char *pszCmd)
{
    /*
     * Check if it already exists.
     */
    PDBGCBP pBp = dbgcBpGet(pDbgc, iBp);
    if (pBp)
        return VERR_DBGC_BP_EXISTS;

    /*
     * Add the breakpoint.
     */
    if (pszCmd)
        pszCmd = RTStrStripL(pszCmd);
    size_t cchCmd = pszCmd ? strlen(pszCmd) : 0;
    pBp = (PDBGCBP)RTMemAlloc(RT_OFFSETOF(DBGCBP, szCmd[cchCmd + 1]));
    if (!pBp)
        return VERR_NO_MEMORY;
    if (cchCmd)
        memcpy(pBp->szCmd, pszCmd, cchCmd + 1);
    else
        pBp->szCmd[0] = '\0';
    pBp->cchCmd = cchCmd;
    pBp->iBp    = iBp;
    pBp->pNext  = pDbgc->pFirstBp;
    pDbgc->pFirstBp = pBp;

    return VINF_SUCCESS;
}

/**
 * Updates the a breakpoint.
 *
 * @returns VBox status code.
 * @param   pDbgc       The DBGC instance.
 * @param   iBp         The breakpoint to update.
 * @param   pszCmd      The new command.
 */
int dbgcBpUpdate(PDBGC pDbgc, RTUINT iBp, const char *pszCmd)
{
    /*
     * Find the breakpoint.
     */
    PDBGCBP pBp = dbgcBpGet(pDbgc, iBp);
    if (!pBp)
        return VERR_DBGC_BP_NOT_FOUND;

    /*
     * Do we need to reallocate?
     */
    if (pszCmd)
        pszCmd = RTStrStripL(pszCmd);
    if (!pszCmd || !*pszCmd)
        pBp->szCmd[0] = '\0';
    else
    {
        size_t cchCmd = strlen(pszCmd);
        if (strlen(pBp->szCmd) >= cchCmd)
        {
            memcpy(pBp->szCmd, pszCmd, cchCmd + 1);
            pBp->cchCmd = cchCmd;
        }
        else
        {
            /*
             * Yes, let's do it the simple way...
             */
            int rc = dbgcBpDelete(pDbgc, iBp);
            AssertRC(rc);
            return dbgcBpAdd(pDbgc, iBp, pszCmd);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Deletes a breakpoint.
 *
 * @returns VBox status code.
 * @param   pDbgc       The DBGC instance.
 * @param   iBp         The breakpoint to delete.
 */
int dbgcBpDelete(PDBGC pDbgc, RTUINT iBp)
{
    /*
     * Search thru the list, when found unlink and free it.
     */
    PDBGCBP pBpPrev = NULL;
    PDBGCBP pBp = pDbgc->pFirstBp;
    for (; pBp; pBp = pBp->pNext)
    {
        if (pBp->iBp == iBp)
        {
            if (pBpPrev)
                pBpPrev->pNext = pBp->pNext;
            else
                pDbgc->pFirstBp = pBp->pNext;
            RTMemFree(pBp);
            return VINF_SUCCESS;
        }
        pBpPrev = pBp;
    }

    return VERR_DBGC_BP_NOT_FOUND;
}


/**
 * Get a breakpoint.
 *
 * @returns Pointer to the breakpoint.
 * @returns NULL if the breakpoint wasn't found.
 * @param   pDbgc       The DBGC instance.
 * @param   iBp         The breakpoint to get.
 */
PDBGCBP dbgcBpGet(PDBGC pDbgc, RTUINT iBp)
{
    /*
     * Enumerate the list.
     */
    PDBGCBP pBp = pDbgc->pFirstBp;
    for (; pBp; pBp = pBp->pNext)
        if (pBp->iBp == iBp)
            return pBp;
    return NULL;
}


/**
 * Executes the command of a breakpoint.
 *
 * @returns VINF_DBGC_BP_NO_COMMAND if there is no command associated with the breakpoint.
 * @returns VERR_DBGC_BP_NOT_FOUND if the breakpoint wasn't found.
 * @returns VERR_BUFFER_OVERFLOW if the is not enough space in the scratch buffer for the command.
 * @returns VBox status code from dbgcProcessCommand() other wise.
 * @param   pDbgc       The DBGC instance.
 * @param   iBp         The breakpoint to execute.
 */
int dbgcBpExec(PDBGC pDbgc, RTUINT iBp)
{
    /*
     * Find the breakpoint.
     */
    PDBGCBP pBp = dbgcBpGet(pDbgc, iBp);
    if (!pBp)
        return VERR_DBGC_BP_NOT_FOUND;

    /*
     * Anything to do?
     */
    if (!pBp->cchCmd)
        return VINF_DBGC_BP_NO_COMMAND;

    /*
     * Execute the command.
     * This means copying it to the scratch buffer and process it as if it
     * were user input. We must save and restore the state of the scratch buffer.
     */
    /* Save the scratch state. */
    char       *pszScratch  = pDbgc->pszScratch;
    unsigned    iArg        = pDbgc->iArg;

    /* Copy the command to the scratch buffer. */
    size_t cbScratch = sizeof(pDbgc->achScratch) - (pDbgc->pszScratch - &pDbgc->achScratch[0]);
    if (pBp->cchCmd >= cbScratch)
        return VERR_BUFFER_OVERFLOW;
    memcpy(pDbgc->pszScratch, pBp->szCmd, pBp->cchCmd + 1);

    /* Execute the command. */
    pDbgc->pszScratch = pDbgc->pszScratch + pBp->cchCmd + 1;
    int rc = dbgcProcessCommand(pDbgc, pszScratch, pBp->cchCmd);

    /* Restore the scratch state. */
    pDbgc->iArg         = iArg;
    pDbgc->pszScratch   = pszScratch;

    return rc;
}

