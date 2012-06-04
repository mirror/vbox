/* $Id$ */
/** @file
 * DBGC - Debugger Console.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_dbgc                       DBGC - The Debug Console
 *
 * The debugger console is an early attempt to make some interactive
 * debugging facilities for the VirtualBox VMM. It was initially only
 * accessible thru a telnet session on debug builds. Later it was hastily
 * built into the VBoxDbg module with a very simple Qt wrapper around it.
 *
 * The debugger is optional and presently not built into release builds
 * of VirtualBox. It is therefore necessary to enclose code related to it
 * in \#ifdef VBOX_WITH_DEBUGGER blocks. This is mandatory for components
 * that register extenral commands.
 *
 *
 * @section sec_dbgc_op                 Operation (intentions)
 *
 * The console will process commands in a manner similar to the OS/2 and
 * windows kernel debuggers. This means ';' is a command separator and
 * that when possible we'll use the same command names as these two uses.
 *
 *
 * @subsection sec_dbg_op_numbers       Numbers
 *
 * Numbers are hexadecimal unless specified with a prefix indicating
 * elsewise. Prefixes:
 *      - '0x' - hexadecimal.
 *      - '0i' - decimal
 *      - '0t' - octal.
 *      - '0y' - binary.
 *
 * Some of the prefixes are a bit uncommon, the reason for this that
 * the typical binary prefix '0b' can also be a hexadecimal value since
 * no prefix or suffix is required for such values. Ditto for '0d' and
 * '0' for decimal and octal.
 *
 *
 * @subsection sec_dbg_op_address       Addressing modes
 *
 *      - Default is flat. For compatibility '%' also means flat.
 *      - Segmented addresses are specified selector:offset.
 *      - Physical addresses are specified using '%%'.
 *      - The default target for the addressing is the guest context, the '#'
 *        will override this and set it to the host.
 *        Note that several operations won't work on host addresses.
 *
 * The '%', '%%' and '#' prefixes is implemented as unary operators, while ':'
 * is a binary operator. Operator precedence takes care of evaluation order.
 *
 *
 * @subsection sec_dbg_op_evalution     Evaluation
 *
 * Most unary and binary C operators are supported, check the help text for
 * details. However, some of these are not yet implemented because this is
 * tiresome and annoying work. So, if something is missing and you need it
 * you implement it or complain to bird. (Ditto for missing functions.)
 *
 * Simple variable support is provided thru the 'set' and 'unset' commands and
 * the unary '$' operator.
 *
 * The unary '@' operator will indicate function calls. Commands and functions
 * are the same thing, except that functions has a return type.
 *
 *
 * @subsection sec_dbg_op_registers     Registers
 *
 * Registers are addressed using their name. Some registers which have several fields
 * (like gdtr) will have separate names indicating the different fields. The default
 * register set is the guest one. To access the hypervisor register one have to
 * prefix the register names with '.'.
 *
 * The registers are implemented as built-in symbols. For making gdb guys more at
 * home it is possible to access them with the '$' operator, i.e. as a variable.
 *
 *
 * @subsection sec_dbg_op_commands      Commands and Functions
 *
 * Commands and functions are the same thing, except that functions may return a
 * value. So, functions may be used as commands. The command/function handlers
 * can detect whether they are invoked as a command or function by checking whether
 * there is a return variable or not.
 *
 * The command/function names are all lowercase, case sensitive, and starting
 * with a letter. Operator characters are not permitted in the names of course.
 * Space is allowed, but must be flagged so the parser can check for multiple
 * spaces and tabs. (This feature is for 'dump xyz' and for emulating the
 * gdb 'info abc'.)
 *
 * The '.' prefix indicates the set of external commands. External commands are
 * command registered by VMM components.
 *
 *
 * @section sec_dbgc_logging            Logging
 *
 * The idea is to be able to pass thru debug and release logs to the console
 * if the user so wishes. This feature requires some kind of hook into the
 * logger instance and while this was sketched it hasn't yet been implemented
 * (dbgcProcessLog and DBGC::fLog).
 *
 *
 *
 * @section sec_dbgc_linking            Linking and API
 *
 * The DBGC code is linked into the VBoxVMM module. (At present it is also
 * linked into VBoxDbg, but this is obviously very wrong.)
 *
 * A COM object will be created for the DBGC so it can be operated remotely
 * without using TCP. VBoxDbg is the intended audience for this usage. Some
 * questions about callbacks (for output) and security (you may wish to
 * restrict users from debugging a VM) needs to be answered first though.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGC
#include <VBox/dbg.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include "DBGCInternal.h"
#include "DBGPlugIns.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int dbgcProcessLog(PDBGC pDbgc);


/**
 * Resolves a symbol (or tries to do so at least).
 *
 * @returns 0 on success.
 * @returns VBox status on failure.
 * @param   pDbgc       The debug console instance.
 * @param   pszSymbol   The symbol name.
 * @param   enmType     The result type.  Specifying DBGCVAR_TYPE_GC_FAR may
 *                      cause failure, avoid it.
 * @param   pResult     Where to store the result.
 */
int dbgcSymbolGet(PDBGC pDbgc, const char *pszSymbol, DBGCVARTYPE enmType, PDBGCVAR pResult)
{
    int rc;

    /*
     * Builtin?
     */
    PCDBGCSYM pSymDesc = dbgcLookupRegisterSymbol(pDbgc, pszSymbol);
    if (pSymDesc)
    {
        if (!pSymDesc->pfnGet)
            return VERR_DBGC_PARSE_WRITEONLY_SYMBOL;
        return pSymDesc->pfnGet(pSymDesc, &pDbgc->CmdHlp, enmType, pResult);
    }

    /*
     * A typical register? (Guest only)
     */
    static const char s_szSixLetterRegisters[] =
        "rflags;eflags;"
    ;
    static const char s_szThreeLetterRegisters[] =
        "eax;rax;"     "r10;" "r8d;r8w;r8b;"  "cr0;"  "dr0;"
        "ebx;rbx;"     "r11;" "r9d;r9w;r8b;"          "dr1;"
        "ecx;rcx;"     "r12;"                 "cr2;"  "dr2;"
        "edx;rdx;"     "r13;"                 "cr3;"  "dr3;"
        "edi;rdi;dil;" "r14;"                 "cr4;"  "dr4;"
        "esi;rsi;sil;" "r15;"                 "cr8;"
        "ebp;rbp;"
        "esp;rsp;"                                    "dr6;"
        "rip;eip;"                                    "dr7;"
        "efl;"
    ;
    static const char s_szTwoLetterRegisters[] =
        "ax;al;ah;"           "r8;"
        "bx;bl;bh;"           "r9;"
        "cx;cl;ch;"    "cs;"
        "dx;dl;dh;"    "ds;"
        "di;"          "es;"
        "si;"          "fs;"
        "bp;"          "gs;"
        "sp;"          "ss;"
        "ip;"
    ;
    size_t const cchSymbol = strlen(pszSymbol);
    if (    (cchSymbol == 2 && strstr(s_szTwoLetterRegisters,   pszSymbol))
        ||  (cchSymbol == 3 && strstr(s_szThreeLetterRegisters, pszSymbol))
        ||  (cchSymbol == 6 && strstr(s_szSixLetterRegisters,   pszSymbol)))
    {
        if (!strchr(pszSymbol, ';'))
        {
            DBGCVAR Var;
            DBGCVAR_INIT_STRING(&Var, pszSymbol);
            rc = dbgcOpRegister(pDbgc, &Var, DBGCVAR_CAT_ANY, pResult);
            if (RT_SUCCESS(rc))
                return DBGCCmdHlpConvert(&pDbgc->CmdHlp, &Var, enmType, false /*fConvSyms*/, pResult);
        }
    }

    /*
     * Ask PDM.
     */
    /** @todo resolve symbols using PDM. */

    /*
     * Ask the debug info manager.
     */
    RTDBGSYMBOL Symbol;
    rc = DBGFR3AsSymbolByName(pDbgc->pVM, pDbgc->hDbgAs, pszSymbol, &Symbol, NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * Default return is a flat gc address.
         */
        DBGCVAR_INIT_GC_FLAT(pResult, Symbol.Value);
        if (Symbol.cb)
            DBGCVAR_SET_RANGE(pResult, DBGCVAR_RANGE_BYTES, Symbol.cb);

        switch (enmType)
        {
            /* nothing to do. */
            case DBGCVAR_TYPE_GC_FLAT:
            case DBGCVAR_TYPE_ANY:
                return VINF_SUCCESS;

            /* impossible at the moment. */
            case DBGCVAR_TYPE_GC_FAR:
                return VERR_DBGC_PARSE_CONVERSION_FAILED;

            /* simply make it numeric. */
            case DBGCVAR_TYPE_NUMBER:
                pResult->enmType = DBGCVAR_TYPE_NUMBER;
                pResult->u.u64Number = Symbol.Value;
                return VINF_SUCCESS;

            /* cast it. */
            case DBGCVAR_TYPE_GC_PHYS:
            case DBGCVAR_TYPE_HC_FLAT:
            case DBGCVAR_TYPE_HC_PHYS:
                return DBGCCmdHlpConvert(&pDbgc->CmdHlp, pResult, enmType, false /*fConvSyms*/, pResult);

            default:
                AssertMsgFailed(("Internal error enmType=%d\n", enmType));
                return VERR_INVALID_PARAMETER;
        }
    }

    return VERR_DBGC_PARSE_NOT_IMPLEMENTED;
}


/**
 * Process all commands currently in the buffer.
 *
 * @returns VBox status code. Any error indicates the termination of the console session.
 * @param   pDbgc       Debugger console instance data.
 * @param   fNoExecute  Indicates that no commands should actually be executed.
 */
static int dbgcProcessCommands(PDBGC pDbgc, bool fNoExecute)
{
    /** @todo Replace this with a sh/ksh/csh/rexx like toplevel language that
     *        allows doing function, loops, if, cases, and such. */
    int rc = VINF_SUCCESS;
    while (pDbgc->cInputLines)
    {
        /*
         * Empty the log buffer if we're hooking the log.
         */
        if (pDbgc->fLog)
        {
            rc = dbgcProcessLog(pDbgc);
            if (RT_FAILURE(rc))
                break;
        }

        if (pDbgc->iRead == pDbgc->iWrite)
        {
            AssertMsgFailed(("The input buffer is empty while cInputLines=%d!\n", pDbgc->cInputLines));
            pDbgc->cInputLines = 0;
            return 0;
        }

        /*
         * Copy the command to the parse buffer.
         */
        char    ch;
        char   *psz = &pDbgc->achInput[pDbgc->iRead];
        char   *pszTrg = &pDbgc->achScratch[0];
        while ((*pszTrg = ch = *psz++) != ';' && ch != '\n' )
        {
            if (psz == &pDbgc->achInput[sizeof(pDbgc->achInput)])
                psz = &pDbgc->achInput[0];

            if (psz == &pDbgc->achInput[pDbgc->iWrite])
            {
                AssertMsgFailed(("The buffer contains no commands while cInputLines=%d!\n", pDbgc->cInputLines));
                pDbgc->cInputLines = 0;
                return 0;
            }

            pszTrg++;
        }
        *pszTrg = '\0';

        /*
         * Advance the buffer.
         */
        pDbgc->iRead = psz - &pDbgc->achInput[0];
        if (ch == '\n')
            pDbgc->cInputLines--;

        /*
         * Parse and execute this command.
         */
        pDbgc->pszScratch = psz;
        pDbgc->iArg       = 0;
        rc = dbgcEvalCommand(pDbgc, &pDbgc->achScratch[0], psz - &pDbgc->achScratch[0] - 1, fNoExecute);
        if (   rc == VERR_DBGC_QUIT
            || rc == VWRN_DBGC_CMD_PENDING)
            break;
        rc = VINF_SUCCESS; /* ignore other statuses */
    }

    return rc;
}


/**
 * Handle input buffer overflow.
 *
 * Will read any available input looking for a '\n' to reset the buffer on.
 *
 * @returns VBox status.
 * @param   pDbgc   Debugger console instance data.
 */
static int dbgcInputOverflow(PDBGC pDbgc)
{
    /*
     * Assert overflow status and reset the input buffer.
     */
    if (!pDbgc->fInputOverflow)
    {
        pDbgc->fInputOverflow = true;
        pDbgc->iRead = pDbgc->iWrite = 0;
        pDbgc->cInputLines = 0;
        pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "Input overflow!!\n");
    }

    /*
     * Eat input till no more or there is a '\n'.
     * When finding a '\n' we'll continue normal processing.
     */
    while (pDbgc->pBack->pfnInput(pDbgc->pBack, 0))
    {
        size_t cbRead;
        int rc = pDbgc->pBack->pfnRead(pDbgc->pBack, &pDbgc->achInput[0], sizeof(pDbgc->achInput) - 1, &cbRead);
        if (RT_FAILURE(rc))
            return rc;
        char *psz = (char *)memchr(&pDbgc->achInput[0], '\n', cbRead);
        if (psz)
        {
            pDbgc->fInputOverflow = false;
            pDbgc->iRead = psz - &pDbgc->achInput[0] + 1;
            pDbgc->iWrite = (unsigned)cbRead;
            pDbgc->cInputLines = 0;
            break;
        }
    }

    return 0;
}


/**
 * Read input and do some preprocessing.
 *
 * @returns VBox status.
 *          In addition to the iWrite and achInput, cInputLines is maintained.
 *          In case of an input overflow the fInputOverflow flag will be set.
 * @param   pDbgc   Debugger console instance data.
 */
static int dbgcInputRead(PDBGC pDbgc)
{
    /*
     * We have ready input.
     * Read it till we don't have any or we have a full input buffer.
     */
    int     rc = 0;
    do
    {
        /*
         * More available buffer space?
         */
        size_t cbLeft;
        if (pDbgc->iWrite > pDbgc->iRead)
            cbLeft = sizeof(pDbgc->achInput) - pDbgc->iWrite - (pDbgc->iRead == 0);
        else
            cbLeft = pDbgc->iRead - pDbgc->iWrite - 1;
        if (!cbLeft)
        {
            /* overflow? */
            if (!pDbgc->cInputLines)
                rc = dbgcInputOverflow(pDbgc);
            break;
        }

        /*
         * Read one char and interpret it.
         */
        char    achRead[128];
        size_t  cbRead;
        rc = pDbgc->pBack->pfnRead(pDbgc->pBack, &achRead[0], RT_MIN(cbLeft, sizeof(achRead)), &cbRead);
        if (RT_FAILURE(rc))
            return rc;
        char *psz = &achRead[0];
        while (cbRead-- > 0)
        {
            char ch = *psz++;
            switch (ch)
            {
                /*
                 * Ignore.
                 */
                case '\0':
                case '\r':
                case '\a':
                    break;

                /*
                 * Backspace.
                 */
                case '\b':
                    Log2(("DBGC: backspace\n"));
                    if (pDbgc->iRead != pDbgc->iWrite)
                    {
                        unsigned iWriteUndo = pDbgc->iWrite;
                        if (pDbgc->iWrite)
                            pDbgc->iWrite--;
                        else
                            pDbgc->iWrite = sizeof(pDbgc->achInput) - 1;

                        if (pDbgc->achInput[pDbgc->iWrite] == '\n')
                            pDbgc->iWrite = iWriteUndo;
                    }
                    break;

                /*
                 * Add char to buffer.
                 */
                case '\t':
                case '\n':
                case ';':
                    switch (ch)
                    {
                        case '\t': ch = ' '; break;
                        case '\n': pDbgc->cInputLines++; break;
                    }
                default:
                    Log2(("DBGC: ch=%02x\n", (unsigned char)ch));
                    pDbgc->achInput[pDbgc->iWrite] = ch;
                    if (++pDbgc->iWrite >= sizeof(pDbgc->achInput))
                        pDbgc->iWrite = 0;
                    break;
            }
        }

        /* Terminate it to make it easier to read in the debugger. */
        pDbgc->achInput[pDbgc->iWrite] = '\0';
    } while (pDbgc->pBack->pfnInput(pDbgc->pBack, 0));

    return rc;
}


/**
 * Reads input, parses it and executes commands on '\n'.
 *
 * @returns VBox status.
 * @param   pDbgc       Debugger console instance data.
 * @param   fNoExecute  Indicates that no commands should actually be executed.
 */
int dbgcProcessInput(PDBGC pDbgc, bool fNoExecute)
{
    /*
     * We know there's input ready, so let's read it first.
     */
    int rc = dbgcInputRead(pDbgc);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Now execute any ready commands.
     */
    if (pDbgc->cInputLines)
    {
        pDbgc->pBack->pfnSetReady(pDbgc->pBack, false);
        pDbgc->fReady = false;
        rc = dbgcProcessCommands(pDbgc, fNoExecute);
        if (RT_SUCCESS(rc) && rc != VWRN_DBGC_CMD_PENDING)
            pDbgc->fReady = true;

        if (    RT_SUCCESS(rc)
            &&  pDbgc->iRead == pDbgc->iWrite
            &&  pDbgc->fReady)
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "VBoxDbg> ");

        if (    RT_SUCCESS(rc)
            &&  pDbgc->fReady)
            pDbgc->pBack->pfnSetReady(pDbgc->pBack, true);
    }
    else
        /* Received nonsense; just skip it. */
        pDbgc->iRead = pDbgc->iWrite;

    return rc;
}


/**
 * Gets the event context identifier string.
 * @returns Read only string.
 * @param   enmCtx          The context.
 */
static const char *dbgcGetEventCtx(DBGFEVENTCTX enmCtx)
{
    switch (enmCtx)
    {
        case DBGFEVENTCTX_RAW:      return "raw";
        case DBGFEVENTCTX_REM:      return "rem";
        case DBGFEVENTCTX_HWACCL:   return "hwaccl";
        case DBGFEVENTCTX_HYPER:    return "hyper";
        case DBGFEVENTCTX_OTHER:    return "other";

        case DBGFEVENTCTX_INVALID:  return "!Invalid Event Ctx!";
        default:
            AssertMsgFailed(("enmCtx=%d\n", enmCtx));
            return "!Unknown Event Ctx!";
    }
}


/**
 * Processes debugger events.
 *
 * @returns VBox status.
 * @param   pDbgc   DBGC Instance data.
 * @param   pEvent  Pointer to event data.
 */
static int dbgcProcessEvent(PDBGC pDbgc, PCDBGFEVENT pEvent)
{
    /*
     * Flush log first.
     */
    if (pDbgc->fLog)
    {
        int rc = dbgcProcessLog(pDbgc);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Process the event.
     */
    pDbgc->pszScratch = &pDbgc->achInput[0];
    pDbgc->iArg       = 0;
    bool fPrintPrompt = true;
    int rc = VINF_SUCCESS;
    switch (pEvent->enmType)
    {
        /*
         * The first part is events we have initiated with commands.
         */
        case DBGFEVENT_HALT_DONE:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: VM %p is halted! (%s)\n",
                                         pDbgc->pVM, dbgcGetEventCtx(pEvent->enmCtx));
            pDbgc->fRegCtxGuest = true; /* we're always in guest context when halted. */
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }


        /*
         * The second part is events which can occur at any time.
         */
        case DBGFEVENT_FATAL_ERROR:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbf event: Fatal error! (%s)\n",
                                         dbgcGetEventCtx(pEvent->enmCtx));
            pDbgc->fRegCtxGuest = false; /* fatal errors are always in hypervisor. */
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }

        case DBGFEVENT_BREAKPOINT:
        case DBGFEVENT_BREAKPOINT_HYPER:
        {
            bool fRegCtxGuest = pDbgc->fRegCtxGuest;
            pDbgc->fRegCtxGuest = pEvent->enmType == DBGFEVENT_BREAKPOINT;

            rc = dbgcBpExec(pDbgc, pEvent->u.Bp.iBp);
            switch (rc)
            {
                case VERR_DBGC_BP_NOT_FOUND:
                    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: Unknown breakpoint %u! (%s)\n",
                                                 pEvent->u.Bp.iBp, dbgcGetEventCtx(pEvent->enmCtx));
                    break;

                case VINF_DBGC_BP_NO_COMMAND:
                    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: Breakpoint %u! (%s)\n",
                                                 pEvent->u.Bp.iBp, dbgcGetEventCtx(pEvent->enmCtx));
                    break;

                case VINF_BUFFER_OVERFLOW:
                    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: Breakpoint %u! Command too long to execute! (%s)\n",
                                                 pEvent->u.Bp.iBp, dbgcGetEventCtx(pEvent->enmCtx));
                    break;

                default:
                    break;
            }
            if (RT_SUCCESS(rc) && DBGFR3IsHalted(pDbgc->pVM))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            else
                pDbgc->fRegCtxGuest = fRegCtxGuest;
            break;
        }

        case DBGFEVENT_STEPPED:
        case DBGFEVENT_STEPPED_HYPER:
        {
            pDbgc->fRegCtxGuest = pEvent->enmType == DBGFEVENT_STEPPED;

            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: Single step! (%s)\n", dbgcGetEventCtx(pEvent->enmCtx));
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }

        case DBGFEVENT_ASSERTION_HYPER:
        {
            pDbgc->fRegCtxGuest = false;

            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                         "\ndbgf event: Hypervisor Assertion! (%s)\n"
                                         "%s"
                                         "%s"
                                         "\n",
                                         dbgcGetEventCtx(pEvent->enmCtx),
                                         pEvent->u.Assert.pszMsg1,
                                         pEvent->u.Assert.pszMsg2);
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }

        case DBGFEVENT_DEV_STOP:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                         "\n"
                                         "dbgf event: DBGFSTOP (%s)\n"
                                         "File:     %s\n"
                                         "Line:     %d\n"
                                         "Function: %s\n",
                                         dbgcGetEventCtx(pEvent->enmCtx),
                                         pEvent->u.Src.pszFile,
                                         pEvent->u.Src.uLine,
                                         pEvent->u.Src.pszFunction);
            if (RT_SUCCESS(rc) && pEvent->u.Src.pszMessage && *pEvent->u.Src.pszMessage)
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                             "Message:  %s\n",
                                             pEvent->u.Src.pszMessage);
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }


        case DBGFEVENT_INVALID_COMMAND:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf/dbgc error: Invalid command event!\n");
            break;
        }

        case DBGFEVENT_TERMINATING:
        {
            pDbgc->fReady = false;
            pDbgc->pBack->pfnSetReady(pDbgc->pBack, false);
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\nVM is terminating!\n");
            fPrintPrompt = false;
            rc = VERR_GENERAL_FAILURE;
            break;
        }


        default:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf/dbgc error: Unknown event %d!\n", pEvent->enmType);
            break;
        }
    }

    /*
     * Prompt, anyone?
     */
    if (fPrintPrompt && RT_SUCCESS(rc))
    {
        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "VBoxDbg> ");
        pDbgc->fReady = true;
        if (RT_SUCCESS(rc))
            pDbgc->pBack->pfnSetReady(pDbgc->pBack, true);
    }

    return rc;
}


/**
 * Prints any log lines from the log buffer.
 *
 * The caller must not call function this unless pDbgc->fLog is set.
 *
 * @returns VBox status. (output related)
 * @param   pDbgc   Debugger console instance data.
 */
static int dbgcProcessLog(PDBGC pDbgc)
{
    /** @todo */
    NOREF(pDbgc);
    return 0;
}


/**
 * Run the debugger console.
 *
 * @returns VBox status.
 * @param   pDbgc   Pointer to the debugger console instance data.
 */
int dbgcRun(PDBGC pDbgc)
{
    /*
     * We're ready for commands now.
     */
    pDbgc->fReady = true;
    pDbgc->pBack->pfnSetReady(pDbgc->pBack, true);

    /*
     * Main Debugger Loop.
     *
     * This loop will either block on waiting for input or on waiting on
     * debug events. If we're forwarding the log we cannot wait for long
     * before we must flush the log.
     */
    int rc = VINF_SUCCESS;
    for (;;)
    {
        if (    pDbgc->pVM
            &&  DBGFR3CanWait(pDbgc->pVM))
        {
            /*
             * Wait for a debug event.
             */
            PCDBGFEVENT pEvent;
            rc = DBGFR3EventWait(pDbgc->pVM, pDbgc->fLog ? 1 : 32, &pEvent);
            if (RT_SUCCESS(rc))
            {
                rc = dbgcProcessEvent(pDbgc, pEvent);
                if (RT_FAILURE(rc))
                    break;
            }
            else if (rc != VERR_TIMEOUT)
                break;

            /*
             * Check for input.
             */
            if (pDbgc->pBack->pfnInput(pDbgc->pBack, 0))
            {
                rc = dbgcProcessInput(pDbgc, false /* fNoExecute */);
                if (RT_FAILURE(rc))
                    break;
            }
        }
        else
        {
            /*
             * Wait for input. If Logging is enabled we'll only wait very briefly.
             */
            if (pDbgc->pBack->pfnInput(pDbgc->pBack, pDbgc->fLog ? 1 : 1000))
            {
                rc = dbgcProcessInput(pDbgc, false /* fNoExecute */);
                if (RT_FAILURE(rc))
                    break;
            }
        }

        /*
         * Forward log output.
         */
        if (pDbgc->fLog)
        {
            rc = dbgcProcessLog(pDbgc);
            if (RT_FAILURE(rc))
                break;
        }
    }

    return rc;
}


/**
 * Creates a a new instance.
 *
 * @returns VBox status code.
 * @param   ppDbgc      Where to store the pointer to the instance data.
 * @param   pBack       Pointer to the backend.
 * @param   fFlags      The flags.
 */
int dbgcCreate(PDBGC *ppDbgc, PDBGCBACK pBack, unsigned fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pBack, VERR_INVALID_POINTER);
    AssertMsgReturn(!fFlags, ("%#x", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize.
     */
    PDBGC pDbgc = (PDBGC)RTMemAllocZ(sizeof(*pDbgc));
    if (!pDbgc)
        return VERR_NO_MEMORY;

    dbgcInitCmdHlp(pDbgc);
    pDbgc->pBack            = pBack;
    pDbgc->pVM              = NULL;
    pDbgc->idCpu            = 0;
    pDbgc->hDbgAs           = DBGF_AS_GLOBAL;
    pDbgc->pszEmulation     = "CodeView/WinDbg";
    pDbgc->paEmulationCmds  = &g_aCmdsCodeView[0];
    pDbgc->cEmulationCmds   = g_cCmdsCodeView;
    pDbgc->paEmulationFuncs = &g_aFuncsCodeView[0];
    pDbgc->cEmulationFuncs  = g_cFuncsCodeView;
    //pDbgc->fLog             = false;
    pDbgc->fRegCtxGuest     = true;
    pDbgc->fRegTerse        = true;
    //pDbgc->cPagingHierarchyDumps = 0;
    //pDbgc->DisasmPos        = {0};
    //pDbgc->SourcePos        = {0};
    //pDbgc->DumpPos          = {0};
    pDbgc->pLastPos          = &pDbgc->DisasmPos;
    //pDbgc->cbDumpElement    = 0;
    //pDbgc->cVars            = 0;
    //pDbgc->paVars           = NULL;
    //pDbgc->pPlugInHead      = NULL;
    //pDbgc->pFirstBp         = NULL;
    //pDbgc->abSearch         = {0};
    //pDbgc->cbSearch         = 0;
    pDbgc->cbSearchUnit       = 1;
    pDbgc->cMaxSearchHits     = 1;
    //pDbgc->SearchAddr       = {0};
    //pDbgc->cbSearchRange    = 0;

    //pDbgc->uInputZero       = 0;
    //pDbgc->iRead            = 0;
    //pDbgc->iWrite           = 0;
    //pDbgc->cInputLines      = 0;
    //pDbgc->fInputOverflow   = false;
    pDbgc->fReady           = true;
    pDbgc->pszScratch       = &pDbgc->achScratch[0];
    //pDbgc->iArg             = 0;
    //pDbgc->rcOutput         = 0;
    //pDbgc->rcCmd            = 0;

    dbgcEvalInit();

    *ppDbgc = pDbgc;
    return VINF_SUCCESS;
}

/**
 * Destroys a DBGC instance created by dbgcCreate.
 *
 * @param   pDbgc   Pointer to the debugger console instance data.
 */
void dbgcDestroy(PDBGC pDbgc)
{
    AssertPtr(pDbgc);

    /* Disable log hook. */
    if (pDbgc->fLog)
    {

    }

    /* Unload all plug-ins. */
    dbgcPlugInUnloadAll(pDbgc);

    /* Detach from the VM. */
    if (pDbgc->pVM)
        DBGFR3Detach(pDbgc->pVM);

    /* finally, free the instance memory. */
    RTMemFree(pDbgc);
}


/**
 * Make a console instance.
 *
 * This will not return until either an 'exit' command is issued or a error code
 * indicating connection loss is encountered.
 *
 * @returns VINF_SUCCESS if console termination caused by the 'exit' command.
 * @returns The VBox status code causing the console termination.
 *
 * @param   pVM         VM Handle.
 * @param   pBack       Pointer to the backend structure. This must contain
 *                      a full set of function pointers to service the console.
 * @param   fFlags      Reserved, must be zero.
 * @remark  A forced termination of the console is easiest done by forcing the
 *          callbacks to return fatal failures.
 */
DBGDECL(int) DBGCCreate(PVM pVM, PDBGCBACK pBack, unsigned fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrNullReturn(pVM, VERR_INVALID_POINTER);

    /*
     * Allocate and initialize instance data
     */
    PDBGC pDbgc;
    int rc = dbgcCreate(&pDbgc, pBack, fFlags);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Print welcome message.
     */
    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                 "Welcome to the VirtualBox Debugger!\n");

    /*
     * Attach to the specified VM.
     */
    if (RT_SUCCESS(rc) && pVM)
    {
        rc = DBGFR3Attach(pVM);
        if (RT_SUCCESS(rc))
        {
            pDbgc->pVM   = pVM;
            pDbgc->idCpu = 0;
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                         "Current VM is %08x, CPU #%u\n" /** @todo get and print the VM name! */
                                         , pDbgc->pVM, pDbgc->idCpu);
        }
        else
            rc = pDbgc->CmdHlp.pfnVBoxError(&pDbgc->CmdHlp, rc, "When trying to attach to VM %p\n", pDbgc->pVM);
    }

    /*
     * Load plugins.
     */
    if (RT_SUCCESS(rc))
    {
        if (pVM)
            dbgcPlugInAutoLoad(pDbgc);
        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "VBoxDbg> ");
    }
    else
        pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\nDBGCCreate error: %Rrc\n", rc);

    /*
     * Run the debugger main loop.
     */
    if (RT_SUCCESS(rc))
        rc = dbgcRun(pDbgc);

    /*
     * Cleanup console debugger session.
     */
    dbgcDestroy(pDbgc);
    return rc == VERR_DBGC_QUIT ? VINF_SUCCESS : rc;
}

