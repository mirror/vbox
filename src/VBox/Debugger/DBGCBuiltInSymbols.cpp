/** $Id$ */
/** @file
 * DBGC - Debugger Console, Built-In Symbols.
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

#include <stdlib.h>
#include <stdio.h>

#include "DBGCInternal.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgcSymGetReg(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, DBGCVARTYPE enmType, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcSymSetReg(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pValue);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Register symbol uUser value.
 * @{
 */
/** If set the register set is the hypervisor and not the guest one. */
#define SYMREG_FLAGS_HYPER      RT_BIT(20)
/** If set a far conversion of the value will use the high 16 bit for the selector.
 * If clear the low 16 bit will be used. */
#define SYMREG_FLAGS_HIGH_SEL   RT_BIT(21)
/** The shift value to calc the size of a register symbol from the uUser value. */
#define SYMREG_SIZE_SHIFT       (24)
/** Get the offset */
#define SYMREG_OFFSET(uUser)    (uUser & ((1 << 20) - 1))
/** Get the size. */
#define SYMREG_SIZE(uUser)      ((uUser >> SYMREG_SIZE_SHIFT) & 0xff)
/** 1 byte. */
#define SYMREG_SIZE_1           ( 1 << SYMREG_SIZE_SHIFT)
/** 2 byte. */
#define SYMREG_SIZE_2           ( 2 << SYMREG_SIZE_SHIFT)
/** 4 byte. */
#define SYMREG_SIZE_4           ( 4 << SYMREG_SIZE_SHIFT)
/** 6 byte. */
#define SYMREG_SIZE_6           ( 6 << SYMREG_SIZE_SHIFT)
/** 8 byte. */
#define SYMREG_SIZE_8           ( 8 << SYMREG_SIZE_SHIFT)
/** 12 byte. */
#define SYMREG_SIZE_12          (12 << SYMREG_SIZE_SHIFT)
/** 16 byte. */
#define SYMREG_SIZE_16          (16 << SYMREG_SIZE_SHIFT)
/** @} */

/** Builtin Symbols.
 * ASSUMES little endian register representation!
 */
static const DBGCSYM    g_aSyms[] =
{
    { "eax",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax)      | SYMREG_SIZE_4 },
    { "ax",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax)      | SYMREG_SIZE_2 },
    { "al",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax)      | SYMREG_SIZE_1 },
    { "ah",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, eax) + 1)| SYMREG_SIZE_1 },

    { "ebx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_4 },
    { "bx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_2 },
    { "bl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_1 },
    { "bh",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, ebx) + 1)| SYMREG_SIZE_1 },

    { "ecx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_4 },
    { "cx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_2 },
    { "cl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_1 },
    { "ch",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, ecx) + 1)| SYMREG_SIZE_1 },

    { "edx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_4 },
    { "dx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_2 },
    { "dl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_1 },
    { "dh",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, edx) + 1)| SYMREG_SIZE_1 },

    { "edi",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edi)      | SYMREG_SIZE_4 },
    { "di",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edi)      | SYMREG_SIZE_2 },

    { "esi",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esi)      | SYMREG_SIZE_4 },
    { "si",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esi)      | SYMREG_SIZE_2 },

    { "ebp",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebp)      | SYMREG_SIZE_4 },
    { "bp",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebp)      | SYMREG_SIZE_2 },

    { "esp",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esp)      | SYMREG_SIZE_4 },
    { "sp",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esp)      | SYMREG_SIZE_2 },

    { "eip",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eip)      | SYMREG_SIZE_4 },
    { "ip",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eip)      | SYMREG_SIZE_2 },

    { "efl",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_4 },
    { "eflags", dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_4 },
    { "fl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_2 },
    { "flags",  dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_2 },

    { "cs",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cs)       | SYMREG_SIZE_2 },
    { "ds",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ds)       | SYMREG_SIZE_2 },
    { "es",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, es)       | SYMREG_SIZE_2 },
    { "fs",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, fs)       | SYMREG_SIZE_2 },
    { "gs",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, gs)       | SYMREG_SIZE_2 },
    { "ss",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ss)       | SYMREG_SIZE_2 },

    { "cr0",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr0)      | SYMREG_SIZE_4 },
    { "cr2",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr2)      | SYMREG_SIZE_4 },
    { "cr3",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr3)      | SYMREG_SIZE_4 },
    { "cr4",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr4)      | SYMREG_SIZE_4 },

    { "tr",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, tr)       | SYMREG_SIZE_2 },
    { "ldtr",   dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ldtr)     | SYMREG_SIZE_2 },

    { "gdtr",   dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, gdtr)     | SYMREG_SIZE_6 },
    { "gdtr.limit", dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, gdtr.cbGdt)| SYMREG_SIZE_2 },
    { "gdtr.base",  dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, gdtr.pGdt)| SYMREG_SIZE_4 },

    { "idtr",   dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, idtr)     | SYMREG_SIZE_6 },
    { "idtr.limit", dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, idtr.cbIdt)| SYMREG_SIZE_2 },
    { "idtr.base",  dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, idtr.pIdt)| SYMREG_SIZE_4 },

    /* hypervisor */

    {".eax",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".ax",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".al",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".ah",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, eax) + 1)| SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".ebx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".bx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".bl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".bh",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, ebx) + 1)| SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".ecx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".cx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".cl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".ch",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, ecx) + 1)| SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".edx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".dx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".dl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".dh",     dbgcSymGetReg,          dbgcSymSetReg,      (offsetof(CPUMCTX, edx) + 1)| SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".edi",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edi)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".di",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edi)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".esi",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esi)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".si",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esi)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".ebp",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebp)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".bp",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebp)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".esp",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esp)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".sp",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, esp)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".eip",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eip)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".ip",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eip)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".efl",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".eflags", dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".fl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".flags",  dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eflags)   | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".cs",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cs)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".ds",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ds)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".es",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, es)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".fs",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, fs)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".gs",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, gs)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".ss",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ss)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".cr0",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr0)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".cr2",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr2)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".cr3",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr3)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".cr4",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, cr4)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },

    {".tr",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, tr)       | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".ldtr",   dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ldtr)     | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },

    {".gdtr",   dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, gdtr)     | SYMREG_SIZE_6 | SYMREG_FLAGS_HYPER },
    {".gdtr.limit", dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, gdtr.cbGdt)| SYMREG_SIZE_2| SYMREG_FLAGS_HYPER },
    {".gdtr.base",  dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, gdtr.pGdt)| SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },

    {".idtr",   dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, idtr)     | SYMREG_SIZE_6 | SYMREG_FLAGS_HYPER },
    {".idtr.limit", dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, idtr.cbIdt)| SYMREG_SIZE_2| SYMREG_FLAGS_HYPER },
    {".idtr.base",  dbgcSymGetReg,      dbgcSymSetReg,      offsetof(CPUMCTX, idtr.pIdt)| SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },

};



/**
 * Get builtin register symbol.
 *
 * The uUser is special for these symbol descriptors. See the SYMREG_* \#defines.
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pSymDesc    Pointer to the symbol descriptor.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   enmType     The result type.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcSymGetReg(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, DBGCVARTYPE enmType, PDBGCVAR pResult)
{
    LogFlow(("dbgcSymSetReg: pSymDesc->pszName=%d\n", pSymDesc->pszName));

    /*
     * pVM is required.
     */
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    Assert(pDbgc->pVM);

    /*
     * Get the right CPU context.
     */
    PCPUMCTX    pCtx;
    int         rc;
    if (!(pSymDesc->uUser & SYMREG_FLAGS_HYPER))
        rc = CPUMQueryGuestCtxPtr(pDbgc->pVM, &pCtx);
    else
        rc = CPUMQueryHyperCtxPtr(pDbgc->pVM, &pCtx);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Get the value.
     */
    void       *pvValue = (char *)pCtx + SYMREG_OFFSET(pSymDesc->uUser);
    uint64_t    u64;
    switch (SYMREG_SIZE(pSymDesc->uUser))
    {
        case 1: u64 = *(uint8_t *)pvValue; break;
        case 2: u64 = *(uint16_t *)pvValue; break;
        case 4: u64 = *(uint32_t *)pvValue; break;
        case 6: u64 = *(uint32_t *)pvValue | ((uint64_t)*(uint16_t *)((char *)pvValue + sizeof(uint32_t)) << 32); break;
        case 8: u64 = *(uint64_t *)pvValue; break;
        default:
            return VERR_PARSE_NOT_IMPLEMENTED;
    }

    /*
     * Construct the desired result.
     */
    if (enmType == DBGCVAR_TYPE_ANY)
        enmType = DBGCVAR_TYPE_NUMBER;
    pResult->pDesc          = NULL;
    pResult->pNext          = NULL;
    pResult->enmType        = enmType;
    pResult->enmRangeType   = DBGCVAR_RANGE_NONE;
    pResult->u64Range       = 0;

    switch (enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFlat = (RTGCPTR)u64;
            break;

        case DBGCVAR_TYPE_GC_FAR:
            switch (SYMREG_SIZE(pSymDesc->uUser))
            {
                case 4:
                    if (!(pSymDesc->uUser & SYMREG_FLAGS_HIGH_SEL))
                    {
                        pResult->u.GCFar.off = (uint16_t)u64;
                        pResult->u.GCFar.sel = (uint16_t)(u64 >> 16);
                    }
                    else
                    {
                        pResult->u.GCFar.sel = (uint16_t)u64;
                        pResult->u.GCFar.off = (uint16_t)(u64 >> 16);
                    }
                    break;
                case 6:
                    if (!(pSymDesc->uUser & SYMREG_FLAGS_HIGH_SEL))
                    {
                        pResult->u.GCFar.off = (uint32_t)u64;
                        pResult->u.GCFar.sel = (uint16_t)(u64 >> 32);
                    }
                    else
                    {
                        pResult->u.GCFar.sel = (uint32_t)u64;
                        pResult->u.GCFar.off = (uint16_t)(u64 >> 32);
                    }
                    break;

                default:
                    return VERR_PARSE_BAD_RESULT_TYPE;
            }
            break;

        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.GCPhys = (RTGCPHYS)u64;
            break;

        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.pvHCFlat = (void *)(uintptr_t)u64;
            break;

        case DBGCVAR_TYPE_HC_FAR:
            switch (SYMREG_SIZE(pSymDesc->uUser))
            {
                case 4:
                    if (!(pSymDesc->uUser & SYMREG_FLAGS_HIGH_SEL))
                    {
                        pResult->u.HCFar.off = (uint16_t)u64;
                        pResult->u.HCFar.sel = (uint16_t)(u64 >> 16);
                    }
                    else
                    {
                        pResult->u.HCFar.sel = (uint16_t)u64;
                        pResult->u.HCFar.off = (uint16_t)(u64 >> 16);
                    }
                    break;
                case 6:
                    if (!(pSymDesc->uUser & SYMREG_FLAGS_HIGH_SEL))
                    {
                        pResult->u.HCFar.off = (uint32_t)u64;
                        pResult->u.HCFar.sel = (uint16_t)(u64 >> 32);
                    }
                    else
                    {
                        pResult->u.HCFar.sel = (uint32_t)u64;
                        pResult->u.HCFar.off = (uint16_t)(u64 >> 32);
                    }
                    break;

                default:
                    return VERR_PARSE_BAD_RESULT_TYPE;
            }
            break;

        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.GCPhys = (RTGCPHYS)u64;
            break;

        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number = u64;
            break;

        case DBGCVAR_TYPE_STRING:
        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_BAD_RESULT_TYPE;

    }

    return 0;
}


/**
 * Set builtin register symbol.
 *
 * The uUser is special for these symbol descriptors. See the SYMREG_* #defines.
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pSymDesc    Pointer to the symbol descriptor.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pValue      The value to assign the symbol.
 */
static DECLCALLBACK(int) dbgcSymSetReg(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pValue)
{
    LogFlow(("dbgcSymSetReg: pSymDesc->pszName=%d\n", pSymDesc->pszName));

    /*
     * pVM is required.
     */
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    Assert(pDbgc->pVM);

    /*
     * Get the right CPU context.
     */
    PCPUMCTX    pCtx;
    int         rc;
    if (!(pSymDesc->uUser & SYMREG_FLAGS_HYPER))
        rc = CPUMQueryGuestCtxPtr(pDbgc->pVM, &pCtx);
    else
        rc = CPUMQueryHyperCtxPtr(pDbgc->pVM, &pCtx);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Check the new value.
     */
    if (pValue->enmType != DBGCVAR_TYPE_NUMBER)
        return VERR_PARSE_ARGUMENT_TYPE_MISMATCH;

    /*
     * Set the value.
     */
    void       *pvValue = (char *)pCtx + SYMREG_OFFSET(pSymDesc->uUser);
    switch (SYMREG_SIZE(pSymDesc->uUser))
    {
        case 1:
            *(uint8_t *)pvValue  = (uint8_t)pValue->u.u64Number;
            break;
        case 2:
            *(uint16_t *)pvValue = (uint16_t)pValue->u.u64Number;
            break;
        case 4:
            *(uint32_t *)pvValue = (uint32_t)pValue->u.u64Number;
            break;
        case 6:
            *(uint32_t *)pvValue = (uint32_t)pValue->u.u64Number;
            ((uint16_t *)pvValue)[3] = (uint16_t)(pValue->u.u64Number >> 32);
            break;
        case 8:
            *(uint64_t *)pvValue = pValue->u.u64Number;
            break;
        default:
            return VERR_PARSE_NOT_IMPLEMENTED;
    }

    return VINF_SUCCESS;
}


/**
 * Finds a builtin symbol.
 * @returns Pointer to symbol descriptor on success.
 * @returns NULL on failure.
 * @param   pDbgc       The debug console instance.
 * @param   pszSymbol   The symbol name.
 */
PCDBGCSYM dbgcLookupRegisterSymbol(PDBGC pDbgc, const char *pszSymbol)
{
    for (unsigned iSym = 0; iSym < ELEMENTS(g_aSyms); iSym++)
        if (!strcmp(pszSymbol, g_aSyms[iSym].pszName))
            return &g_aSyms[iSym];

    /** @todo externally registered symbols. */
    NOREF(pDbgc);
    return NULL;
}

