/** $Id$ */
/** @file
 * DBGC - Debugger Console, Command Helpers.
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



/**
 * Command helper for writing text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pvBuf       What to write.
 * @param   cbBuf       Number of bytes to write.
 * @param   pcbWritten  Where to store the number of bytes actually written.
 *                      If NULL the entire buffer must be successfully written.
 */
static DECLCALLBACK(int) dbgcHlpWrite(PDBGCCMDHLP pCmdHlp, const void *pvBuf, size_t cbBuf, size_t *pcbWritten)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    return pDbgc->pBack->pfnWrite(pDbgc->pBack, pvBuf, cbBuf, pcbWritten);
}


/**
 * Command helper for writing formatted text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pcb         Where to store the number of bytes written.
 * @param   pszFormat   The format string.
 *                      This is using the log formatter, so it's format extensions can be used.
 * @param   ...         Arguments specified in the format string.
 */
static DECLCALLBACK(int) dbgcHlpPrintf(PDBGCCMDHLP pCmdHlp, size_t *pcbWritten, const char *pszFormat, ...)
{
    /*
     * Do the formatting and output.
     */
    va_list args;
    va_start(args, pszFormat);
    int rc = pCmdHlp->pfnPrintfV(pCmdHlp, pcbWritten, pszFormat, args);
    va_end(args);

    return rc;
}

/**
 * Callback to format non-standard format specifiers.
 *
 * @returns The number of bytes formatted.
 * @param   pvArg           Formatter argument.
 * @param   pfnOutput       Pointer to output function.
 * @param   pvArgOutput     Argument for the output function.
 * @param   ppszFormat      Pointer to the format string pointer. Advance this till the char
 *                          after the format specifier.
 * @param   pArgs           Pointer to the argument list. Use this to fetch the arguments.
 * @param   cchWidth        Format Width. -1 if not specified.
 * @param   cchPrecision    Format Precision. -1 if not specified.
 * @param   fFlags          Flags (RTSTR_NTFS_*).
 * @param   chArgSize       The argument size specifier, 'l' or 'L'.
 */
static DECLCALLBACK(size_t) dbgcStringFormatter(void *pvArg, PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                const char **ppszFormat, va_list *pArgs, int cchWidth,
                                                int cchPrecision, unsigned fFlags, char chArgSize)
{
    NOREF(cchWidth); NOREF(cchPrecision); NOREF(fFlags); NOREF(chArgSize); NOREF(pvArg);
    if (**ppszFormat != 'D')
    {
        (*ppszFormat)++;
        return 0;
    }

    (*ppszFormat)++;
    switch (**ppszFormat)
    {
        /*
         * Print variable without range.
         * The argument is a const pointer to the variable.
         */
        case 'V':
        {
            (*ppszFormat)++;
            PCDBGCVAR   pVar = va_arg(*pArgs, PCDBGCVAR);
            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%%%VGv", pVar->u.GCFlat);
                case DBGCVAR_TYPE_GC_FAR:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%04x:%08x", pVar->u.GCFar.sel, pVar->u.GCFar.off);
                case DBGCVAR_TYPE_GC_PHYS:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%%%%%VGp", pVar->u.GCPhys);
                case DBGCVAR_TYPE_HC_FLAT:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%%#%VHv", (uintptr_t)pVar->u.pvHCFlat);
                case DBGCVAR_TYPE_HC_FAR:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "#%04x:%08x", pVar->u.HCFar.sel, pVar->u.HCFar.off);
                case DBGCVAR_TYPE_HC_PHYS:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "#%%%%%VHp", pVar->u.HCPhys);
                case DBGCVAR_TYPE_STRING:
                    return pfnOutput(pvArgOutput, pVar->u.pszString, (size_t)pVar->u64Range);
                case DBGCVAR_TYPE_NUMBER:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%llx", pVar->u.u64Number);

                case DBGCVAR_TYPE_UNKNOWN:
                default:
                    return pfnOutput(pvArgOutput, "??", 2);
            }
        }

        /*
         * Print variable with range.
         * The argument is a const pointer to the variable.
         */
        case 'v':
        {
            (*ppszFormat)++;
            PCDBGCVAR   pVar = va_arg(*pArgs, PCDBGCVAR);

            char szRange[32];
            switch (pVar->enmRangeType)
            {
                case DBGCVAR_RANGE_NONE:
                    szRange[0] = '\0';
                    break;
                case DBGCVAR_RANGE_ELEMENTS:
                    RTStrPrintf(szRange, sizeof(szRange), " L %llx", pVar->u64Range);
                    break;
                case DBGCVAR_RANGE_BYTES:
                    RTStrPrintf(szRange, sizeof(szRange), " LB %llx", pVar->u64Range);
                    break;
            }

            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%%%VGv%s", pVar->u.GCFlat, szRange);
                case DBGCVAR_TYPE_GC_FAR:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%04x:%08x%s", pVar->u.GCFar.sel, pVar->u.GCFar.off, szRange);
                case DBGCVAR_TYPE_GC_PHYS:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%%%%%VGp%s", pVar->u.GCPhys, szRange);
                case DBGCVAR_TYPE_HC_FLAT:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%%#%VHv%s", (uintptr_t)pVar->u.pvHCFlat, szRange);
                case DBGCVAR_TYPE_HC_FAR:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "#%04x:%08x%s", pVar->u.HCFar.sel, pVar->u.HCFar.off, szRange);
                case DBGCVAR_TYPE_HC_PHYS:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "#%%%%%VHp%s", pVar->u.HCPhys, szRange);
                case DBGCVAR_TYPE_STRING:
                    return pfnOutput(pvArgOutput, pVar->u.pszString, (size_t)pVar->u64Range);
                case DBGCVAR_TYPE_NUMBER:
                    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%llx%s", pVar->u.u64Number, szRange);

                case DBGCVAR_TYPE_UNKNOWN:
                default:
                    return pfnOutput(pvArgOutput, "??", 2);
            }
        }

        default:
            AssertMsgFailed(("Invalid format type '%s'!\n", **ppszFormat));
            return 0;
    }
}


/**
 * Output callback.
 *
 * @returns number of bytes written.
 * @param   pvArg       User argument.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) dbgcFormatOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    PDBGC   pDbgc = (PDBGC)pvArg;
    if (cbChars)
    {
        int rc = pDbgc->pBack->pfnWrite(pDbgc->pBack, pachChars, cbChars, NULL);
        if (VBOX_FAILURE(rc))
        {
            pDbgc->rcOutput = rc;
            cbChars = 0;
        }
    }

    return cbChars;
}



/**
 * Command helper for writing formatted text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pcb         Where to store the number of bytes written.
 * @param   pszFormat   The format string.
 *                      This is using the log formatter, so it's format extensions can be used.
 * @param   args        Arguments specified in the format string.
 */
static DECLCALLBACK(int) dbgcHlpPrintfV(PDBGCCMDHLP pCmdHlp, size_t *pcbWritten, const char *pszFormat, va_list args)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Do the formatting and output.
     */
    pDbgc->rcOutput = 0;
    size_t cb = RTStrFormatV(dbgcFormatOutput, pDbgc, dbgcStringFormatter, pDbgc, pszFormat, args);

    if (pcbWritten)
        *pcbWritten = cb;

    return pDbgc->rcOutput;
}


/**
 * Reports an error from a DBGF call.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to command helpers.
 * @param   rc          The VBox status code returned by a DBGF call.
 * @param   pszFormat   Format string for additional messages. Can be NULL.
 * @param   ...         Format arguments, optional.
 */
static DECLCALLBACK(int) dbgcHlpVBoxErrorV(PDBGCCMDHLP pCmdHlp, int rc, const char *pszFormat, va_list args)
{
    switch (rc)
    {
        case VINF_SUCCESS:
            break;

        default:
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: %Vrc: %s", rc, pszFormat ? " " : "\n");
            if (RT_SUCCESS(rc) && pszFormat)
                rc = pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszFormat, args);
            if (RT_SUCCESS(rc))
                rc = VERR_DBGC_COMMAND_FAILED;
            break;
    }
    return rc;
}


/**
 * Reports an error from a DBGF call.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to command helpers.
 * @param   rc          The VBox status code returned by a DBGF call.
 * @param   pszFormat   Format string for additional messages. Can be NULL.
 * @param   ...         Format arguments, optional.
 */
static DECLCALLBACK(int) dbgcHlpVBoxError(PDBGCCMDHLP pCmdHlp, int rc, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rcRet = pCmdHlp->pfnVBoxErrorV(pCmdHlp, rc, pszFormat, args);
    va_end(args);
    return rcRet;
}


/**
 * Command helper for reading memory specified by a DBGC variable.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pVM         VM handle if GC or physical HC address.
 * @param   pvBuffer    Where to store the read data.
 * @param   cbRead      Number of bytes to read.
 * @param   pVarPointer DBGC variable specifying where to start reading.
 * @param   pcbRead     Where to store the number of bytes actually read.
 *                      This optional, but it's useful when read GC virtual memory where a
 *                      page in the requested range might not be present.
 *                      If not specified not-present failure or end of a HC physical page
 *                      will cause failure.
 */
static DECLCALLBACK(int) dbgcHlpMemRead(PDBGCCMDHLP pCmdHlp, PVM pVM, void *pvBuffer, size_t cbRead, PCDBGCVAR pVarPointer, size_t *pcbRead)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Dummy check.
     */
    if (cbRead == 0)
    {
        if (*pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    /*
     * Convert Far addresses getting size and the correct base address.
     * Getting and checking the size is what makes this messy and slow.
     */
    DBGCVAR Var = *pVarPointer;
    switch (pVarPointer->enmType)
    {
        case DBGCVAR_TYPE_GC_FAR:
        {
            /* Use DBGFR3AddrFromSelOff for the conversion. */
            Assert(pDbgc->pVM);
            DBGFADDRESS Address;
            int rc = DBGFR3AddrFromSelOff(pDbgc->pVM, &Address, Var.u.GCFar.sel, Var.u.GCFar.off);
            if (VBOX_FAILURE(rc))
                return rc;

            /* don't bother with flat selectors (for now). */
            if (!DBGFADDRESS_IS_FLAT(&Address))
            {
                SELMSELINFO SelInfo;
                rc = SELMR3GetSelectorInfo(pDbgc->pVM, Address.Sel, &SelInfo);
                if (VBOX_SUCCESS(rc))
                {
                    RTGCUINTPTR cb; /* -1 byte */
                    if (SELMSelInfoIsExpandDown(&SelInfo))
                    {
                        if (    !SelInfo.Raw.Gen.u1Granularity
                            &&  Address.off > UINT16_C(0xffff))
                            return VERR_OUT_OF_SELECTOR_BOUNDS;
                        if (Address.off <= SelInfo.cbLimit)
                            return VERR_OUT_OF_SELECTOR_BOUNDS;
                        cb = (SelInfo.Raw.Gen.u1Granularity ? UINT32_C(0xffffffff) : UINT32_C(0xffff)) - Address.off;
                    }
                    else
                    {
                        if (Address.off > SelInfo.cbLimit)
                            return VERR_OUT_OF_SELECTOR_BOUNDS;
                        cb = SelInfo.cbLimit - Address.off;
                    }
                    if (cbRead - 1 > cb)
                    {
                        if (!pcbRead)
                            return VERR_OUT_OF_SELECTOR_BOUNDS;
                        cbRead = cb + 1;
                    }
                }

                Var.enmType = DBGCVAR_TYPE_GC_FLAT;
                Var.u.GCFlat = Address.FlatPtr;
            }
            break;
        }

        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_PHYS:
            break;

        case DBGCVAR_TYPE_HC_FAR: /* not supported yet! */
        default:
            return VERR_NOT_IMPLEMENTED;
    }



    /*
     * Copy page by page.
     */
    size_t cbLeft = cbRead;
    for (;;)
    {
        /*
         * Calc read size.
         */
        size_t cb = RT_MIN(PAGE_SIZE, cbLeft);
        switch (pVarPointer->enmType)
        {
            case DBGCVAR_TYPE_GC_FLAT: cb = RT_MIN(cb, PAGE_SIZE - (Var.u.GCFlat & PAGE_OFFSET_MASK)); break;
            case DBGCVAR_TYPE_GC_PHYS: cb = RT_MIN(cb, PAGE_SIZE - (Var.u.GCPhys & PAGE_OFFSET_MASK)); break;
            case DBGCVAR_TYPE_HC_FLAT: cb = RT_MIN(cb, PAGE_SIZE - ((uintptr_t)Var.u.pvHCFlat & PAGE_OFFSET_MASK)); break;
            case DBGCVAR_TYPE_HC_PHYS: cb = RT_MIN(cb, PAGE_SIZE - ((size_t)Var.u.HCPhys & PAGE_OFFSET_MASK)); break; /* size_t: MSC has braindead loss of data warnings! */
            default: break;
        }

        /*
         * Perform read.
         */
        int rc;
        switch (Var.enmType)
        {
            case DBGCVAR_TYPE_GC_FLAT:
                rc = MMR3ReadGCVirt(pVM, pvBuffer, Var.u.GCFlat, cb);
                break;
            case DBGCVAR_TYPE_GC_PHYS:
                rc = PGMPhysReadGCPhys(pVM, pvBuffer, Var.u.GCPhys, cb);
                break;

            case DBGCVAR_TYPE_HC_PHYS:
            case DBGCVAR_TYPE_HC_FLAT:
            case DBGCVAR_TYPE_HC_FAR:
            {
                DBGCVAR Var2;
                rc = dbgcOpAddrFlat(pDbgc, &Var, &Var2);
                if (VBOX_SUCCESS(rc))
                {
                    /** @todo protect this!!! */
                    memcpy(pvBuffer, Var2.u.pvHCFlat, cb);
                    rc = 0;
                }
                else
                    rc = VERR_INVALID_POINTER;
                break;
            }

            default:
                rc = VERR_PARSE_INCORRECT_ARG_TYPE;
        }

        /*
         * Check for failure.
         */
        if (VBOX_FAILURE(rc))
        {
            if (pcbRead && (*pcbRead = cbRead - cbLeft) > 0)
                return VINF_SUCCESS;
            return rc;
        }

        /*
         * Next.
         */
        cbLeft -= cb;
        if (!cbLeft)
            break;
        pvBuffer = (char *)pvBuffer + cb;
        rc = pCmdHlp->pfnEval(pCmdHlp, &Var, "%DV + %d", &Var, cb);
        if (VBOX_FAILURE(rc))
        {
            if (pcbRead && (*pcbRead = cbRead - cbLeft) > 0)
                return VINF_SUCCESS;
            return rc;
        }
    }

    /*
     * Done
     */
    if (pcbRead)
        *pcbRead = cbRead;
    return 0;
}

/**
 * Command helper for writing memory specified by a DBGC variable.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pVM         VM handle if GC or physical HC address.
 * @param   pvBuffer    What to write.
 * @param   cbWrite     Number of bytes to write.
 * @param   pVarPointer DBGC variable specifying where to start reading.
 * @param   pcbWritten  Where to store the number of bytes written.
 *                      This is optional. If NULL be aware that some of the buffer
 *                      might have been written to the specified address.
 */
static DECLCALLBACK(int) dbgcHlpMemWrite(PDBGCCMDHLP pCmdHlp, PVM pVM, const void *pvBuffer, size_t cbWrite, PCDBGCVAR pVarPointer, size_t *pcbWritten)
{
    NOREF(pCmdHlp); NOREF(pVM); NOREF(pvBuffer); NOREF(cbWrite); NOREF(pVarPointer); NOREF(pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Evaluates an expression.
 * (Hopefully the parser and functions are fully reentrant.)
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pResult     Where to store the result.
 * @param   pszExpr     The expression. Format string with the format DBGC extensions.
 * @param   ...         Format arguments.
 */
static DECLCALLBACK(int) dbgcHlpEval(PDBGCCMDHLP pCmdHlp, PDBGCVAR pResult, const char *pszExpr, ...)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Format the expression.
     */
    char szExprFormatted[2048];
    va_list args;
    va_start(args, pszExpr);
    size_t cb = RTStrPrintfExV(dbgcStringFormatter, pDbgc, szExprFormatted, sizeof(szExprFormatted), pszExpr, args);
    va_end(args);
    /* ignore overflows. */

    return dbgcEvalSub(pDbgc, &szExprFormatted[0], cb, pResult);
}


/**
 * Executes one command expression.
 * (Hopefully the parser and functions are fully reentrant.)
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pszExpr     The expression. Format string with the format DBGC extensions.
 * @param   ...         Format arguments.
 */
static DECLCALLBACK(int) dbgcHlpExec(PDBGCCMDHLP pCmdHlp, const char *pszExpr, ...)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    /* Save the scratch state. */
    char       *pszScratch  = pDbgc->pszScratch;
    unsigned    iArg        = pDbgc->iArg;

    /*
     * Format the expression.
     */
    va_list args;
    va_start(args, pszExpr);
    size_t cbScratch = sizeof(pDbgc->achScratch) - (pDbgc->pszScratch - &pDbgc->achScratch[0]);
    size_t cb = RTStrPrintfExV(dbgcStringFormatter, pDbgc, pDbgc->pszScratch, cbScratch, pszExpr, args);
    va_end(args);
    if (cb >= cbScratch)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Execute the command.
     * We save and restore the arg index and scratch buffer pointer.
     */
    pDbgc->pszScratch = pDbgc->pszScratch + cb + 1;
    int rc = dbgcProcessCommand(pDbgc, pszScratch, cb, false /* fNoExecute */);

    /* Restore the scratch state. */
    pDbgc->iArg         = iArg;
    pDbgc->pszScratch   = pszScratch;

    return rc;
}


/**
 * Converts a DBGC variable to a DBGF address structure.
 *
 * @returns VBox status code.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pVar        The variable to convert.
 * @param   pAddress    The target address.
 */
static DECLCALLBACK(int) dbgcHlpVarToDbgfAddr(PDBGCCMDHLP pCmdHlp, PCDBGCVAR pVar, PDBGFADDRESS pAddress)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    return dbgcVarToDbgfAddr(pDbgc, pVar, pAddress);
}


/**
 * Converts a DBGC variable to a boolean.
 *
 * @returns VBox status code.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pVar        The variable to convert.
 * @param   pf          Where to store the boolean.
 */
static DECLCALLBACK(int) dbgcHlpVarToBool(PDBGCCMDHLP pCmdHlp, PCDBGCVAR pVar, bool *pf)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    NOREF(pDbgc);

    switch (pVar->enmType)
    {
        case DBGCVAR_TYPE_STRING:
            /** @todo add strcasecmp / stricmp wrappers to iprt/string.h. */
            if (    !strcmp(pVar->u.pszString, "true")
                ||  !strcmp(pVar->u.pszString, "True")
                ||  !strcmp(pVar->u.pszString, "TRUE")
                ||  !strcmp(pVar->u.pszString, "on")
                ||  !strcmp(pVar->u.pszString, "On")
                ||  !strcmp(pVar->u.pszString, "oN")
                ||  !strcmp(pVar->u.pszString, "ON")
                ||  !strcmp(pVar->u.pszString, "enabled")
                ||  !strcmp(pVar->u.pszString, "Enabled")
                ||  !strcmp(pVar->u.pszString, "DISABLED"))
            {
                *pf = true;
                return VINF_SUCCESS;
            }
            if (    !strcmp(pVar->u.pszString, "false")
                ||  !strcmp(pVar->u.pszString, "False")
                ||  !strcmp(pVar->u.pszString, "FALSE")
                ||  !strcmp(pVar->u.pszString, "off")
                ||  !strcmp(pVar->u.pszString, "Off")
                ||  !strcmp(pVar->u.pszString, "OFF")
                ||  !strcmp(pVar->u.pszString, "disabled")
                ||  !strcmp(pVar->u.pszString, "Disabled")
                ||  !strcmp(pVar->u.pszString, "DISABLED"))
            {
                *pf = false;
                return VINF_SUCCESS;
            }
            return VERR_PARSE_INCORRECT_ARG_TYPE; /** @todo better error code! */

        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_NUMBER:
            *pf = pVar->u.u64Number != 0;
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_SYMBOL:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
}


/**
 * Initializes the Command Helpers for a DBGC instance.
 *
 * @param   pDbgc   Pointer to the DBGC instance.
 */
void dbgcInitCmdHlp(PDBGC pDbgc)
{
    pDbgc->CmdHlp.pfnWrite      = dbgcHlpWrite;
    pDbgc->CmdHlp.pfnPrintfV    = dbgcHlpPrintfV;
    pDbgc->CmdHlp.pfnPrintf     = dbgcHlpPrintf;
    pDbgc->CmdHlp.pfnVBoxErrorV = dbgcHlpVBoxErrorV;
    pDbgc->CmdHlp.pfnVBoxError  = dbgcHlpVBoxError;
    pDbgc->CmdHlp.pfnMemRead    = dbgcHlpMemRead;
    pDbgc->CmdHlp.pfnMemWrite   = dbgcHlpMemWrite;
    pDbgc->CmdHlp.pfnEval       = dbgcHlpEval;
    pDbgc->CmdHlp.pfnExec       = dbgcHlpExec;
    pDbgc->CmdHlp.pfnVarToDbgfAddr = dbgcHlpVarToDbgfAddr;
    pDbgc->CmdHlp.pfnVarToBool = dbgcHlpVarToBool;
}

