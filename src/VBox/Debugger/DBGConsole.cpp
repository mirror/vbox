/** $Id$ */
/** @file
 * DBGC - Debugger Console.
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


/** @page pg_dbgc                       DBGC - The Debug Console
 *
 * The debugger console is a first attempt to make some interactive
 * debugging facilities for the VirtualBox backend (i.e. the VM). At a later
 * stage we'll make a fancy gui around this, but for the present a telnet (or
 * serial terminal) will have to suffice.
 *
 * The debugger is only built into the VM with debug builds or when
 * VBOX_WITH_DEBUGGER is defined. There might be need for \#ifdef'ing on this
 * define to enable special debugger hooks, but the general approach is to
 * make generic interfaces. The individual components also can register
 * external commands, and such code must be within \#ifdef.
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
 *
 * @subsection sec_dbg_op_address       Addressing modes
 *
 *      - Default is flat. For compatability '%' also means flat.
 *      - Segmented addresses are specified selector:offset.
 *      - Physical addresses are specified using '%%'.
 *      - The default target for the addressing is the guest context, the '#'
 *        will override this and set it to the host.
 *
 *
 * @subsection sec_dbg_op_evalution     Evaluation
 *
 * As time permits support will be implemented support for a subset of the C
 * binary operators, starting with '+', '-', '*' and '/'. Support for variables
 * are provided thru commands 'set' and 'unset' and the unary operator '$'. The
 * unary '@' operator will indicate function calls. The debugger needs a set of
 * memory read functions, but we might later extend this to allow registration of
 * external functions too.
 *
 * A special command '?' will then be added which evalutates a given expression
 * and prints it in all the different formats.
 *
 *
 * @subsection sec_dbg_op_registers     Registers
 *
 * Registers are addressed using their name. Some registers which have several fields
 * (like gdtr) will have separate names indicating the different fields. The default
 * register set is the guest one. To access the hypervisor register one have to
 * prefix the register names with '.'.
 *
 *
 * @subsection sec_dbg_op_commands      Commands
 *
 * The commands are all lowercase, case sensitive, and starting with a letter. We will
 * later add some special commands ('?' for evaulation) and perhaps command classes ('.', '!')
 *
 *
 * @section sec_dbg_tasks               Tasks
 *
 * To implement DBGT and instrument VMM for basic state inspection and log
 * viewing, the follwing task must be executed:
 *
 *      -# Basic threading layer in RT.
 *      -# Basic tcpip server abstration in RT.
 *      -# Write DBGC.
 *      -# Write DBCTCP.
 *      -# Integrate with VMM and the rest.
 *      -# Start writing DBGF (VMM).
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
static DECLCALLBACK(int) dbgcCmdBrkREM(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdHelp(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdQuit(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdStop(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdStack(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpIDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageDir(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageDirBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageTable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageTableBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpTSS(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLog(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLogDest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLogFlags(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdMemoryInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdFormat(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLoadSyms(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdUnset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLoadVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdShowVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdHarakiri(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdEcho(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRunScript(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);

static DECLCALLBACK(int) dbgcCmdBrkAccess(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkClear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkDisable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkEnable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkList(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdGo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdListSource(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdListNear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdReg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRegGuest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRegHyper(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRegTerse(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdSearchMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdTrace(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdUnassemble(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);

static DECLCALLBACK(int) dbgcOpMinus(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpPluss(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBooleanNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpAddrFlat(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpAddrPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpAddrHost(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpAddrHostPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpVar(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);

static DECLCALLBACK(int) dbgcOpAddrFar(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpMult(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpDiv(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpMod(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpAdd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpSub(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseShiftLeft(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseShiftRight(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseXor(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBitwiseOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBooleanAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpBooleanOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpRangeLength(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpRangeLengthBytes(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcOpRangeTo(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);

static DECLCALLBACK(int) dbgcSymGetReg(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, DBGCVARTYPE enmType, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcSymSetReg(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pValue);

static void dbgcVarSetGCFlat(PDBGCVAR pVar, RTGCPTR GCFlat);
static void dbgcVarSetGCFlatByteRange(PDBGCVAR pVar, RTGCPTR GCFlat, uint64_t cb);
static void dbgcVarSetVar(PDBGCVAR pVar, PCDBGCVAR pVar2);
static void dbgcVarSetNoRange(PDBGCVAR pVar);
static void dbgcVarSetByteRange(PDBGCVAR pVar, uint64_t cb);
static int dbgcVarToDbgfAddr(PDBGC pDbgc, PCDBGCVAR pVar, PDBGFADDRESS pAddress);

static int dbgcBpAdd(PDBGC pDbgc, RTUINT iBp, const char *pszCmd);
static int dbgcBpUpdate(PDBGC pDbgc, RTUINT iBp, const char *pszCmd);
static int dbgcBpDelete(PDBGC pDbgc, RTUINT iBp);
static PDBGCBP dbgcBpGet(PDBGC pDbgc, RTUINT iBp);
static int dbgcBpExec(PDBGC pDbgc, RTUINT iBp);

static PCDBGCSYM dbgcLookupRegisterSymbol(PDBGC pDbgc, const char *pszSymbol);
static int dbgcSymbolGet(PDBGC pDbgc, const char *pszSymbol, DBGCVARTYPE enmType, PDBGCVAR pResult);
static int dbgcEvalSub(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pResult);
static int dbgcProcessCommand(PDBGC pDbgc, char *pszCmd, size_t cchCmd);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Pointer to head of the list of external commands.
 */
static PDBGCEXTCMDS     g_pExtCmdsHead;     /** @todo rw protect g_pExtCmdsHead! */
/** Locks the g_pExtCmdsHead list for reading. */
#define DBGCEXTCMDS_LOCK_RD()       do { } while (0)
/** Locks the g_pExtCmdsHead list for writing. */
#define DBGCEXTCMDS_LOCK_WR()       do { } while (0)
/** UnLocks the g_pExtCmdsHead list after reading. */
#define DBGCEXTCMDS_UNLOCK_RD()     do { } while (0)
/** UnLocks the g_pExtCmdsHead list after writing. */
#define DBGCEXTCMDS_UNLOCK_WR()     do { } while (0)


/** One argument of any kind. */
static const DBGCVARDESC    g_aArgAny[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_ANY,        0,                              "var",          "Any type of argument." },
};

/** Multiple string arguments (min 1). */
static const DBGCVARDESC    g_aArgMultiStr[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           ~0,         DBGCVAR_CAT_STRING,     0,                              "strings",      "One or more strings." },
};

/** Filename string. */
static const DBGCVARDESC    g_aArgFilename[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "path",         "Filename string." },
};


/** 'br' arguments. */
static const DBGCVARDESC    g_aArgBrkREM[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "address",      "The address." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "passes",       "The number of passes before we trigger the breakpoint. (0 is default)" },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     DBGCVD_FLAGS_DEP_PREV,          "max passes",   "The number of passes after which we stop triggering the breakpoint. (~0 is default)" },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "cmds",         "String of commands to be executed when the breakpoint is hit. Quote it!" },
};


/** 'dg', 'dga', 'dl', 'dla' arguments. */
static const DBGCVARDESC    g_aArgDumpDT[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0,         DBGCVAR_CAT_NUMBER,     0,                              "sel",          "Selector or selector range." },
    {  0,           ~0,         DBGCVAR_CAT_POINTER,    0,                              "address",      "Far address which selector should be dumped." },
};


/** 'di', 'dia' arguments. */
static const DBGCVARDESC    g_aArgDumpIDT[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0,         DBGCVAR_CAT_NUMBER,     0,                              "int",          "The interrupt vector or interrupt vector range." },
};


/** 'dpd*' arguments. */
static const DBGCVARDESC    g_aArgDumpPD[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "index",        "Index into the page directory." },
    {  0,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address which page directory entry to start dumping from. Range is applied to the page directory." },
};


/** 'dpda' arguments. */
static const DBGCVARDESC    g_aArgDumpPDAddr[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address of the page directory entry to start dumping from." },
};


/** 'dpt?' arguments. */
static const DBGCVARDESC    g_aArgDumpPT[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address which page directory entry to start dumping from." },
};


/** 'dpta' arguments. */
static const DBGCVARDESC    g_aArgDumpPTAddr[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address of the page table entry to start dumping from." },
};


/** 'dt' arguments. */
static const DBGCVARDESC    g_aArgDumpTSS[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "tss",          "TSS selector number." },
    {  0,           1,          DBGCVAR_CAT_POINTER,    0,                              "tss:ign|addr", "TSS address. If the selector is a TSS selector, the offset will be ignored." }
};


/** 'help' arguments. */
static const DBGCVARDESC    g_aArgHelp[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0,         DBGCVAR_CAT_STRING,     0,                              "cmd/op",       "Zero or more command or operator names." },
};


/** 'info' arguments. */
static const DBGCVARDESC    g_aArgInfo[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "info",         "The name of the info to display." },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "args",         "String arguments to the handler." },
};


/** loadsyms arguments. */
static const DBGCVARDESC    g_aArgLoadSyms[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "path",         "Filename string." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "delta",        "Delta to add to the loaded symbols. (optional)" },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "module name",  "Module name." },
    {  0,           1,          DBGCVAR_CAT_POINTER,    DBGCVD_FLAGS_DEP_PREV,          "module address", "Module address." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "module size",  "The module size. (optional)" },
};


/** log arguments. */
static const DBGCVARDESC    g_aArgLog[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "groups",       "Group modifier string (quote it!)." }
};


/** logdest arguments. */
static const DBGCVARDESC    g_aArgLogDest[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "dests",        "Destination modifier string (quote it!)." }
};


/** logflags arguments. */
static const DBGCVARDESC    g_aArgLogFlags[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "flags",        "Flag modifier string (quote it!)." }
};


/** 'set' arguments */
static const DBGCVARDESC    g_aArgSet[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "var",          "Variable name." },
    {  1,           1,          DBGCVAR_CAT_ANY,        0,                              "value",        "Value to assign to the variable." },
};





/** Command descriptors for the basic commands. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDescs,         cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "br",         1,        4,        &g_aArgBrkREM[0],   ELEMENTS(g_aArgBrkREM),     NULL,               0,          dbgcCmdBrkREM,      "<address> [passes [max passes]] [cmds]",
                                                                                                                                                                    "Sets a recompiler specific breakpoint." },
    { "bye",        0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdQuit,        "",                     "Exits the debugger." },
    { "dg",         0,       ~0,        &g_aArgDumpDT[0],   ELEMENTS(g_aArgDumpDT),     NULL,               0,          dbgcCmdDumpDT,      "[sel [..]]",           "Dump the global descriptor table (GDT)." },
    { "dga",        0,       ~0,        &g_aArgDumpDT[0],   ELEMENTS(g_aArgDumpDT),     NULL,               0,          dbgcCmdDumpDT,      "[sel [..]]",           "Dump the global descriptor table (GDT) including not-present entries." },
    { "di",         0,       ~0,        &g_aArgDumpIDT[0],  ELEMENTS(g_aArgDumpIDT),    NULL,               0,          dbgcCmdDumpIDT,     "[int [..]]",           "Dump the interrupt descriptor table (IDT)." },
    { "dia",        0,       ~0,        &g_aArgDumpIDT[0],  ELEMENTS(g_aArgDumpIDT),    NULL,               0,          dbgcCmdDumpIDT,     "[int [..]]",           "Dump the interrupt descriptor table (IDT) including not-present entries." },
    { "dl",         0,       ~0,        &g_aArgDumpDT[0],   ELEMENTS(g_aArgDumpDT),     NULL,               0,          dbgcCmdDumpDT,      "[sel [..]]",           "Dump the local descriptor table (LDT)." },
    { "dla",        0,       ~0,        &g_aArgDumpDT[0],   ELEMENTS(g_aArgDumpDT),     NULL,               0,          dbgcCmdDumpDT,      "[sel [..]]",           "Dump the local descriptor table (LDT) including not-present entries." },
    { "dpd",        0,        1,        &g_aArgDumpPD[0],   ELEMENTS(g_aArgDumpPD),     NULL,               0,          dbgcCmdDumpPageDir, "[addr] [index]",       "Dumps page directory entries of the default context." },
    { "dpda",       0,        1,        &g_aArgDumpPDAddr[0],ELEMENTS(g_aArgDumpPDAddr),NULL,               0,          dbgcCmdDumpPageDir, "[addr]",               "Dumps specified page directory." },
    { "dpdb",       1,        1,        &g_aArgDumpPD[0],   ELEMENTS(g_aArgDumpPD),     NULL,               0,          dbgcCmdDumpPageDirBoth, "[addr] [index]",   "Dumps page directory entries of the guest and the hypervisor. " },
    { "dpdg",       0,        1,        &g_aArgDumpPD[0],   ELEMENTS(g_aArgDumpPD),     NULL,               0,          dbgcCmdDumpPageDir, "[addr] [index]",       "Dumps page directory entries of the guest." },
    { "dpdh",       0,        1,        &g_aArgDumpPD[0],   ELEMENTS(g_aArgDumpPD),     NULL,               0,          dbgcCmdDumpPageDir, "[addr] [index]",       "Dumps page directory entries of the hypervisor. " },
    { "dpt",        1,        1,        &g_aArgDumpPT[0],   ELEMENTS(g_aArgDumpPT),     NULL,               0,          dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the default context." },
    { "dpta",       1,        1,        &g_aArgDumpPTAddr[0],ELEMENTS(g_aArgDumpPTAddr), NULL,              0,          dbgcCmdDumpPageTable,"<addr>",              "Dumps specified page table." },
    { "dptb",       1,        1,        &g_aArgDumpPT[0],   ELEMENTS(g_aArgDumpPT),     NULL,               0,          dbgcCmdDumpPageTableBoth,"<addr>",          "Dumps page table entries of the guest and the hypervisor." },
    { "dptg",       1,        1,        &g_aArgDumpPT[0],   ELEMENTS(g_aArgDumpPT),     NULL,               0,          dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the guest." },
    { "dpth",       1,        1,        &g_aArgDumpPT[0],   ELEMENTS(g_aArgDumpPT),     NULL,               0,          dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the hypervisor." },
    { "dt",         0,        1,        &g_aArgDumpTSS[0],  ELEMENTS(g_aArgDumpTSS),    NULL,               0,          dbgcCmdDumpTSS,     "[tss|tss:ign|addr]",   "Dump the task state segment (TSS)." },
    { "echo",       1,        ~0,       &g_aArgMultiStr[0], ELEMENTS(g_aArgMultiStr),   NULL,               0,          dbgcCmdEcho,        "<str1> [str2..[strN]]", "Displays the strings separated by one blank space and the last one followed by a newline." },
    { "exit",       0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdQuit,        "",                     "Exits the debugger." },
    { "format",     1,        1,        &g_aArgAny[0],      ELEMENTS(g_aArgAny),        NULL,               0,          dbgcCmdFormat,      "",                     "Evaluates an expression and formats it." },
    { "harakiri",   0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdHarakiri,    "",                     "Kills debugger process." },
    { "help",       0,        ~0,       &g_aArgHelp[0],     ELEMENTS(g_aArgHelp),       NULL,               0,          dbgcCmdHelp,        "[cmd/op [..]]",        "Display help. For help about info items try 'info help'." },
    { "info",       1,        2,        &g_aArgInfo[0],     ELEMENTS(g_aArgInfo),       NULL,               0,          dbgcCmdInfo,        "<info> [args]",        "Display info register in the DBGF. For a list of info items try 'info help'." },
    { "loadsyms",   1,        5,        &g_aArgLoadSyms[0], ELEMENTS(g_aArgLoadSyms),   NULL,               0,          dbgcCmdLoadSyms,    "<filename> [delta] [module] [module address]", "Loads symbols from a text file. Optionally giving a delta and a module." },
    { "loadvars",   1,        1,        &g_aArgFilename[0], ELEMENTS(g_aArgFilename),   NULL,               0,          dbgcCmdLoadVars,    "<filename>",           "Load variables from file. One per line, same as the args to the set command." },
    { "log",        1,        1,        &g_aArgLog[0],      ELEMENTS(g_aArgLog),        NULL,               0,          dbgcCmdLog,         "<group string>",       "Modifies the logging group settings (VBOX_LOG)" },
    { "logdest",    1,        1,        &g_aArgLogDest[0],  ELEMENTS(g_aArgLogDest),    NULL,               0,          dbgcCmdLogDest,     "<dest string>",        "Modifies the logging destination (VBOX_LOG_DEST)." },
    { "logflags",   1,        1,        &g_aArgLogFlags[0], ELEMENTS(g_aArgLogFlags),   NULL,               0,          dbgcCmdLogFlags,    "<flags string>",       "Modifies the logging flags (VBOX_LOG_FLAGS)." },
    { "quit",       0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdQuit,        "",                     "Exits the debugger." },
    { "runscript",  1,        1,        &g_aArgFilename[0], ELEMENTS(g_aArgFilename),   NULL,               0,          dbgcCmdRunScript,   "<filename>",           "Runs the command listed in the script. Lines starting with '#' (after removing blanks) are comment. blank lines are ignored. Stops on failure." },
    { "set",        2,        2,        &g_aArgSet[0],      ELEMENTS(g_aArgSet),        NULL,               0,          dbgcCmdSet,         "<var> <value>",        "Sets a global variable." },
    { "showvars",   0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdShowVars,    "",                     "List all the defined variables." },
    { "stop",       0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdStop,        "",                     "Stop execution." },
    { "unset",      1,        ~0,       &g_aArgMultiStr[0], ELEMENTS(g_aArgMultiStr),   NULL,               0,          dbgcCmdUnset,       "<var1> [var1..[varN]]", "Unsets (delete) one or more global variables." },
};


/** 'ba' arguments. */
static const DBGCVARDESC    g_aArgBrkAcc[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "access",       "The access type: x=execute, rw=read/write (alias r), w=write, i=not implemented." },
    {  1,           1,          DBGCVAR_CAT_NUMBER,     0,                              "size",         "The access size: 1, 2, 4, or 8. 'x' access requires 1, and 8 requires amd64 long mode." },
    {  1,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "address",      "The address." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "passes",       "The number of passes before we trigger the breakpoint. (0 is default)" },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     DBGCVD_FLAGS_DEP_PREV,          "max passes",   "The number of passes after which we stop triggering the breakpoint. (~0 is default)" },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "cmds",         "String of commands to be executed when the breakpoint is hit. Quote it!" },
};


/** 'bc', 'bd', 'be' arguments. */
static const DBGCVARDESC    g_aArgBrks[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0,         DBGCVAR_CAT_NUMBER,     0,                              "#bp",          "Breakpoint number." },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "all",          "All breakpoints." },
};


/** 'bp' arguments. */
static const DBGCVARDESC    g_aArgBrkSet[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "address",      "The address." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "passes",       "The number of passes before we trigger the breakpoint. (0 is default)" },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     DBGCVD_FLAGS_DEP_PREV,          "max passes",   "The number of passes after which we stop triggering the breakpoint. (~0 is default)" },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "cmds",         "String of commands to be executed when the breakpoint is hit. Quote it!" },
};


/** 'd?' arguments. */
static const DBGCVARDESC    g_aArgDumpMem[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address where to start dumping memory." },
};


/** 'ln' arguments. */
static const DBGCVARDESC    g_aArgListNear[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0,         DBGCVAR_CAT_POINTER,    0,                              "address",      "Address of the symbol to look up." },
    {  0,           ~0,         DBGCVAR_CAT_SYMBOL,     0,                              "symbol",       "Symbol to lookup." },
};

/** 'ln' return. */
static const DBGCVARDESC    g_RetListNear =
{
       1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "The last resolved symbol/address with adjusted range."
};


/** 'ls' arguments. */
static const DBGCVARDESC    g_aArgListSource[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address where to start looking for source lines." },
};


/** 'm' argument. */
static const DBGCVARDESC    g_aArgMemoryInfo[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Pointer to obtain info about." },
};


/** 'r' arguments. */
static const DBGCVARDESC    g_aArgReg[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_SYMBOL,     0,                              "register",     "Register to show or set." },
    {  0,           1,     DBGCVAR_CAT_NUMBER_NO_RANGE, DBGCVD_FLAGS_DEP_PREV,          "value",        "New register value." },
};


/** 's' arguments. */
static const DBGCVARDESC    g_aArgSearchMem[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "range",        "Register to show or set." },
    {  1,           ~0,         DBGCVAR_CAT_ANY,        0,                              "pattern",      "Pattern to search for." },
};


/** 'u' arguments. */
static const DBGCVARDESC    g_aArgUnassemble[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address where to start disassembling." },
};


/** Command descriptors for the CodeView / WinDbg emulation.
 * The emulation isn't attempting to be identical, only somewhat similar.
 */
static const DBGCCMD    g_aCmdsCodeView[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDescs,         cArgDescs,                     pResultDesc,        fFlags,  pfnHandler          pszSyntax,          ....pszDescription */
    { "ba",         3,        6,        &g_aArgBrkAcc[0],   RT_ELEMENTS(g_aArgBrkAcc),     NULL,               0,       dbgcCmdBrkAccess,   "<access> <size> <address> [passes [max passes]] [cmds]",
                                                                                                                                                                    "Sets a data access breakpoint." },
    { "bc",         1,       ~0,        &g_aArgBrks[0],     RT_ELEMENTS(g_aArgBrks),       NULL,               0,       dbgcCmdBrkClear,    "all | <bp#> [bp# []]", "Enabled a set of breakpoints." },
    { "bd",         1,       ~0,        &g_aArgBrks[0],     RT_ELEMENTS(g_aArgBrks),       NULL,               0,       dbgcCmdBrkDisable,  "all | <bp#> [bp# []]", "Disables a set of breakpoints." },
    { "be",         1,       ~0,        &g_aArgBrks[0],     RT_ELEMENTS(g_aArgBrks),       NULL,               0,       dbgcCmdBrkEnable,   "all | <bp#> [bp# []]", "Enabled a set of breakpoints." },
    { "bl",         0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdBrkList,     "",                     "Lists all the breakpoints." },
    { "bp",         1,        4,        &g_aArgBrkSet[0],   RT_ELEMENTS(g_aArgBrkSet),     NULL,               0,       dbgcCmdBrkSet,      "<address> [passes [max passes]] [cmds]",
                                                                                                                                                                 "Sets a breakpoint (int 3)." },
    { "d",          0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory using last element size." },
    { "da",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory as ascii string." },
    { "db",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in bytes." },
    { "dd",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in double words." },
    { "dq",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in quad words." },
    { "dw",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in words." },
    { "g",          0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdGo,          "",                     "Continue execution." },
    { "k",          0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdStack,       "",                     "Callstack." },
    { "kg",         0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdStack,       "",                     "Callstack - guest." },
    { "kh",         0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdStack,       "",                     "Callstack - hypervisor." },
    { "ln",         0,        ~0,       &g_aArgListNear[0], RT_ELEMENTS(g_aArgListNear),   &g_RetListNear,     0,       dbgcCmdListNear,    "[addr/sym [..]]",      "List symbols near to the address. Default address is CS:EIP." },
    { "ls",         0,        1,        &g_aArgListSource[0],RT_ELEMENTS(g_aArgListSource),NULL,               0,       dbgcCmdListSource,  "[addr]",               "Source." },
    { "m",          1,        1,        &g_aArgMemoryInfo[0],RT_ELEMENTS(g_aArgMemoryInfo),NULL,               0,       dbgcCmdMemoryInfo,  "<addr>",               "Display information about that piece of memory." },
    { "r",          0,        2,        &g_aArgReg[0],      RT_ELEMENTS(g_aArgReg),        NULL,               0,       dbgcCmdReg,         "[reg [newval]]",       "Show or set register(s) - active reg set." },
    { "rg",         0,        2,        &g_aArgReg[0],      RT_ELEMENTS(g_aArgReg),        NULL,               0,       dbgcCmdRegGuest,    "[reg [newval]]",       "Show or set register(s) - guest reg set." },
    { "rh",         0,        2,        &g_aArgReg[0],      RT_ELEMENTS(g_aArgReg),        NULL,               0,       dbgcCmdRegHyper,    "[reg [newval]]",       "Show or set register(s) - hypervisor reg set." },
    { "rt",         0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdRegTerse,    "",                     "Toggles terse / verbose register info." },
    //{ "s",          2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Continue last search." },
    { "sa",         2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Search memory for an ascii string." },
    { "sb",         2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Search memory for one or more bytes." },
    { "sd",         2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Search memory for one or more double words." },
    { "sq",         2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Search memory for one or more quad words." },
    { "su",         2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Search memory for an unicode string." },
    { "sw",         2,       ~0,        &g_aArgSearchMem[0],RT_ELEMENTS(g_aArgSearchMem),  NULL,               0,       dbgcCmdSearchMem,   "<range> <pattern>",    "Search memory for one or more words." },
    { "t",          0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdTrace,       "",                     "Instruction trace (step into)." },
    { "u",          0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble),NULL,               0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble." },
};


/** Operators. */
static const DBGCOP g_aOps[] =
{
    /* szName is initialized as a 4 char array because of M$C elsewise optimizing it away in /Ox mode (the 'const char' vs 'char' problem). */
    /* szName,          cchName, fBinary,    iPrecedence,    pfnHandlerUnary,    pfnHandlerBitwise  */
    { {'-'},            1,       false,      1,              dbgcOpMinus,        NULL,                       "Unary minus." },
    { {'+'},            1,       false,      1,              dbgcOpPluss,        NULL,                       "Unary pluss." },
    { {'!'},            1,       false,      1,              dbgcOpBooleanNot,   NULL,                       "Boolean not." },
    { {'~'},            1,       false,      1,              dbgcOpBitwiseNot,   NULL,                       "Bitwise complement." },
    { {':'},            1,       true,       2,              NULL,               dbgcOpAddrFar,              "Far pointer." },
    { {'%'},            1,       false,      3,              dbgcOpAddrFlat,     NULL,                       "Flat address." },
    { {'%','%'},        2,       false,      3,              dbgcOpAddrPhys,     NULL,                       "Physical address." },
    { {'#'},            1,       false,      3,              dbgcOpAddrHost,     NULL,                       "Flat host address." },
    { {'#','%','%'},    3,       false,      3,              dbgcOpAddrHostPhys, NULL,                       "Physical host address." },
    { {'$'},            1,       false,      3,              dbgcOpVar,          NULL,                       "Reference a variable." },
    { {'*'},            1,       true,       10,             NULL,               dbgcOpMult,                 "Multiplication." },
    { {'/'},            1,       true,       11,             NULL,               dbgcOpDiv,                  "Division." },
    { {'%'},            1,       true,       12,             NULL,               dbgcOpMod,                  "Modulus." },
    { {'+'},            1,       true,       13,             NULL,               dbgcOpAdd,                  "Addition." },
    { {'-'},            1,       true,       14,             NULL,               dbgcOpSub,                  "Subtraction." },
    { {'<','<'},        2,       true,       15,             NULL,               dbgcOpBitwiseShiftLeft,     "Bitwise left shift." },
    { {'>','>'},        2,       true,       16,             NULL,               dbgcOpBitwiseShiftRight,    "Bitwise right shift." },
    { {'&'},            1,       true,       17,             NULL,               dbgcOpBitwiseAnd,           "Bitwise and." },
    { {'^'},            1,       true,       18,             NULL,               dbgcOpBitwiseXor,           "Bitwise exclusiv or." },
    { {'|'},            1,       true,       19,             NULL,               dbgcOpBitwiseOr,            "Bitwise inclusive or." },
    { {'&','&'},        2,       true,       20,             NULL,               dbgcOpBooleanAnd,           "Boolean and." },
    { {'|','|'},        2,       true,       21,             NULL,               dbgcOpBooleanOr,            "Boolean or." },
    { {'L'},            1,       true,       22,             NULL,               dbgcOpRangeLength,          "Range elements." },
    { {'L','B'},        2,       true,       23,             NULL,               dbgcOpRangeLengthBytes,     "Range bytes." },
    { {'T'},            1,       true,       24,             NULL,               dbgcOpRangeTo,              "Range to." }
};

/** Bitmap where set bits indicates the characters the may start an operator name. */
static uint32_t g_bmOperatorChars[256 / (4*8)];

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
    { "ah",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax) + 1  | SYMREG_SIZE_1 },

    { "ebx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_4 },
    { "bx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_2 },
    { "bl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_1 },
    { "bh",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx) + 1  | SYMREG_SIZE_1 },

    { "ecx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_4 },
    { "cx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_2 },
    { "cl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_1 },
    { "ch",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx) + 1  | SYMREG_SIZE_1 },

    { "edx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_4 },
    { "dx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_2 },
    { "dl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_1 },
    { "dh",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx) + 1  | SYMREG_SIZE_1 },

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
    {".ah",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, eax) + 1  | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".ebx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".bx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".bl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".bh",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ebx) + 1  | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".ecx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".cx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".cl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".ch",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, ecx) + 1  | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

    {".edx",    dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_4 | SYMREG_FLAGS_HYPER },
    {".dx",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_2 | SYMREG_FLAGS_HYPER },
    {".dl",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx)      | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },
    {".dh",     dbgcSymGetReg,          dbgcSymSetReg,      offsetof(CPUMCTX, edx) + 1  | SYMREG_SIZE_1 | SYMREG_FLAGS_HYPER },

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
 * Prints full command help.
 */
static int dbgcPrintHelp(PDBGCCMDHLP pCmdHlp, PCDBGCCMD pCmd, bool fExternal)
{
    int rc;

    /* the command */
    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                            "%s%-*s %-30s %s",
                            fExternal ? "." : "",
                            fExternal ? 10 : 11,
                            pCmd->pszCmd,
                            pCmd->pszSyntax,
                            pCmd->pszDescription);
    if (!pCmd->cArgsMin && pCmd->cArgsMin == pCmd->cArgsMax)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <no args>\n");
    else if (pCmd->cArgsMin == pCmd->cArgsMax)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u args>\n", pCmd->cArgsMin);
    else if (pCmd->cArgsMax == ~0U)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u+ args>\n", pCmd->cArgsMin);
    else
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u to %u args>\n", pCmd->cArgsMin, pCmd->cArgsMax);

    /* argument descriptions. */
    for (unsigned i = 0; i < pCmd->cArgDescs; i++)
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                "    %-12s %s",
                                pCmd->paArgDescs[i].pszName,
                                pCmd->paArgDescs[i].pszDescription);
        if (!pCmd->paArgDescs[i].cTimesMin)
        {
            if (pCmd->paArgDescs[i].cTimesMax == ~0U)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <optional+>\n");
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <optional-%u>\n", pCmd->paArgDescs[i].cTimesMax);
        }
        else
        {
            if (pCmd->paArgDescs[i].cTimesMax == ~0U)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u+>\n", pCmd->paArgDescs[i].cTimesMin);
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u-%u>\n", pCmd->paArgDescs[i].cTimesMin, pCmd->paArgDescs[i].cTimesMax);
        }
    }
    return rc;
}


/**
 * The 'help' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdHelp(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC       pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    int         rc = VINF_SUCCESS;
    unsigned    i;

    if (!cArgs)
    {
        /*
         * All the stuff.
         */
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                "VirtualBox Debugger\n"
                                "-------------------\n"
                                "\n"
                                "Commands and Functions:\n");
        for (i = 0; i < ELEMENTS(g_aCmds); i++)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                    "%-11s %-30s %s\n",
                                    g_aCmds[i].pszCmd,
                                    g_aCmds[i].pszSyntax,
                                    g_aCmds[i].pszDescription);
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                "\n"
                                "Emulation: %s\n", pDbgc->pszEmulation);
        PCDBGCCMD pCmd = pDbgc->paEmulationCmds;
        for (i = 0; i < pDbgc->cEmulationCmds; i++, pCmd++)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                    "%-11s %-30s %s\n",
                                    pCmd->pszCmd,
                                    pCmd->pszSyntax,
                                    pCmd->pszDescription);

        if (g_pExtCmdsHead)
        {
            DBGCEXTCMDS_LOCK_RD();
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                    "\n"
                                    "External Commands and Functions:\n");
            for (PDBGCEXTCMDS pExtCmd = g_pExtCmdsHead; pExtCmd; pExtCmd = pExtCmd->pNext)
                for (i = 0; i < pExtCmd->cCmds; i++)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                            ".%-10s %-30s %s\n",
                                            pExtCmd->paCmds[i].pszCmd,
                                            pExtCmd->paCmds[i].pszSyntax,
                                            pExtCmd->paCmds[i].pszDescription);
            DBGCEXTCMDS_UNLOCK_RD();
        }

        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                "\n"
                                "Operators:\n");
        unsigned iPrecedence = 0;
        unsigned cLeft = ELEMENTS(g_aOps);
        while (cLeft > 0)
        {
            for (i = 0; i < ELEMENTS(g_aOps); i++)
                if (g_aOps[i].iPrecedence == iPrecedence)
                {
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                            "%-10s  %s  %s\n",
                                            g_aOps[i].szName,
                                            g_aOps[i].fBinary ? "Binary" : "Unary ",
                                            g_aOps[i].pszDescription);
                    cLeft--;
                }
            iPrecedence++;
        }
    }
    else
    {
        /*
         * Search for the arguments (strings).
         */
        for (unsigned iArg = 0; iArg < cArgs; iArg++)
        {
            Assert(paArgs[iArg].enmType == DBGCVAR_TYPE_STRING);
            bool fFound = false;

            /* lookup in the emulation command list first */
            for (i = 0; i < pDbgc->cEmulationCmds; i++)
                if (!strcmp(pDbgc->paEmulationCmds[i].pszCmd, paArgs[iArg].u.pszString))
                {
                    rc = dbgcPrintHelp(pCmdHlp, &pDbgc->paEmulationCmds[i], false);
                    fFound = true;
                    break;
                }

            /* lookup in the command list (even when found in the emulation) */
            for (i = 0; i < ELEMENTS(g_aCmds); i++)
                if (!strcmp(g_aCmds[i].pszCmd, paArgs[iArg].u.pszString))
                {
                    rc = dbgcPrintHelp(pCmdHlp, &g_aCmds[i], false);
                    fFound = true;
                    break;
                }

           /* external commands */
           if (     !fFound
               &&   g_pExtCmdsHead
               &&   paArgs[iArg].u.pszString[0] == '.')
           {
               DBGCEXTCMDS_LOCK_RD();
               for (PDBGCEXTCMDS pExtCmd = g_pExtCmdsHead; pExtCmd; pExtCmd = pExtCmd->pNext)
                   for (i = 0; i < pExtCmd->cCmds; i++)
                       if (!strcmp(pExtCmd->paCmds[i].pszCmd, paArgs[iArg].u.pszString + 1))
                       {
                           rc = dbgcPrintHelp(pCmdHlp, &g_aCmds[i], true);
                           fFound = true;
                           break;
                       }
               DBGCEXTCMDS_UNLOCK_RD();
           }

           /* operators */
           if (!fFound && strlen(paArgs[iArg].u.pszString) < sizeof(g_aOps[i].szName))
           {
               for (i = 0; i < ELEMENTS(g_aOps); i++)
                   if (!strcmp(g_aOps[i].szName, paArgs[iArg].u.pszString))
                   {
                       rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                               "%-10s  %s  %s\n",
                                               g_aOps[i].szName,
                                               g_aOps[i].fBinary ? "Binary" : "Unary ",
                                               g_aOps[i].pszDescription);
                       fFound = true;
                       break;
                   }
           }

           /* found? */
           if (!fFound)
               rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                       "error: '%s' was not found!\n",
                                       paArgs[iArg].u.pszString);
        } /* foreach argument */
    }

    NOREF(pCmd);
    NOREF(pVM);
    NOREF(pResult);
    return rc;
}


/**
 * The 'quit', 'exit' and 'bye' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdQuit(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Quitting console...\n");
    NOREF(pCmd);
    NOREF(pVM);
    NOREF(paArgs);
    NOREF(cArgs);
    NOREF(pResult);
    return VERR_DBGC_QUIT;
}


/**
 * The 'go' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdGo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Check if the VM is halted or not before trying to resume it.
     */
    if (!DBGFR3IsHalted(pVM))
        pCmdHlp->pfnPrintf(pCmdHlp, NULL, "warning: The VM is already running...\n");
    else
    {
        int rc = DBGFR3Resume(pVM);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Executing DBGFR3Resume().");
    }

    NOREF(pCmd);
    NOREF(paArgs);
    NOREF(cArgs);
    NOREF(pResult);
    return 0;
}

/**
 * The 'stop' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdStop(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Check if the VM is halted or not before trying to halt it.
     */
    int rc;
    if (DBGFR3IsHalted(pVM))
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "warning: The VM is already halted...\n");
    else
    {
        rc = DBGFR3Halt(pVM);
        if (VBOX_SUCCESS(rc))
            rc = VWRN_DBGC_CMD_PENDING;
        else
            rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Executing DBGFR3Halt().");
    }

    NOREF(pCmd); NOREF(paArgs); NOREF(cArgs); NOREF(pResult);
    return rc;
}


/**
 * The 'ba' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkAccess(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR /*pResult*/)
{
    /*
     * Interpret access type.
     */
    if (    !strchr("xrwi", paArgs[0].u.pszString[0])
        ||  paArgs[0].u.pszString[1])
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid access type '%s' for '%s'. Valid types are 'e', 'r', 'w' and 'i'.\n",
                                  paArgs[0].u.pszString, pCmd->pszCmd);
    uint8_t fType = 0;
    switch (paArgs[0].u.pszString[0])
    {
        case 'x':  fType = X86_DR7_RW_EO; break;
        case 'r':  fType = X86_DR7_RW_RW; break;
        case 'w':  fType = X86_DR7_RW_WO; break;
        case 'i':  fType = X86_DR7_RW_IO; break;
    }

    /*
     * Validate size.
     */
    if (fType == X86_DR7_RW_EO && paArgs[1].u.u64Number != 1)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid access size %RX64 for '%s'. 'x' access type requires size 1!\n",
                                  paArgs[1].u.u64Number, pCmd->pszCmd);
    switch (paArgs[1].u.u64Number)
    {
        case 1:
        case 2:
        case 4:
            break;
        /*case 8: - later*/
        default:
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid access size %RX64 for '%s'. 1, 2 or 4!\n",
                                      paArgs[1].u.u64Number, pCmd->pszCmd);
    }
    uint8_t cb = (uint8_t)paArgs[1].u.u64Number;

    /*
     * Convert the pointer to a DBGF address.
     */
    DBGFADDRESS Address;
    int rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, &paArgs[2], &Address);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Couldn't convert '%DV' to a DBGF address, rc=%Vrc.\n", &paArgs[2], rc);

    /*
     * Pick out the optional arguments.
     */
    uint64_t iHitTrigger = 0;
    uint64_t iHitDisable = ~0;
    const char *pszCmds = NULL;
    unsigned iArg = 3;
    if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
    {
        iHitTrigger = paArgs[iArg].u.u64Number;
        iArg++;
        if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
        {
            iHitDisable = paArgs[iArg].u.u64Number;
            iArg++;
        }
    }
    if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_STRING)
    {
        pszCmds = paArgs[iArg].u.pszString;
        iArg++;
    }

    /*
     * Try set the breakpoint.
     */
    RTUINT iBp;
    rc = DBGFR3BpSetReg(pVM, &Address, iHitTrigger, iHitDisable, fType, cb, &iBp);
    if (VBOX_SUCCESS(rc))
    {
        PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        rc = dbgcBpAdd(pDbgc, iBp, pszCmds);
        if (VBOX_SUCCESS(rc))
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Set access breakpoint %u at %VGv\n", iBp, Address.FlatPtr);
        if (rc == VERR_DBGC_BP_EXISTS)
        {
            rc = dbgcBpUpdate(pDbgc, iBp, pszCmds);
            if (VBOX_SUCCESS(rc))
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Updated access breakpoint %u at %VGv\n", iBp, Address.FlatPtr);
        }
        int rc2 = DBGFR3BpClear(pDbgc->pVM, iBp);
        AssertRC(rc2);
    }
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Failed to set access breakpoint at %VGv, rc=%Vrc.\n", Address.FlatPtr, rc);
}


/**
 * The 'bc' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkClear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR /*pResult*/)
{
    /*
     * Enumerate the arguments.
     */
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    int     rc = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs && VBOX_SUCCESS(rc); iArg++)
    {
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
        {
            /* one */
            RTUINT iBp = (RTUINT)paArgs[iArg].u.u64Number;
            if (iBp != paArgs[iArg].u.u64Number)
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Breakpoint id %RX64 is too large!\n", paArgs[iArg].u.u64Number);
                break;
            }
            int rc2 = DBGFR3BpClear(pVM, iBp);
            if (VBOX_FAILURE(rc2))
                rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc2, "DBGFR3BpClear failed for breakpoint %u!\n", iBp);
            if (VBOX_SUCCESS(rc2) || rc2 == VERR_DBGF_BP_NOT_FOUND)
                dbgcBpDelete(pDbgc, iBp);
        }
        else if (!strcmp(paArgs[iArg].u.pszString, "all"))
        {
            /* all */
            PDBGCBP pBp = pDbgc->pFirstBp;
            while (pBp)
            {
                RTUINT iBp = pBp->iBp;
                pBp = pBp->pNext;

                int rc2 = DBGFR3BpClear(pVM, iBp);
                if (VBOX_FAILURE(rc2))
                    rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc2, "DBGFR3BpClear failed for breakpoint %u!\n", iBp);
                if (VBOX_SUCCESS(rc2) || rc2 == VERR_DBGF_BP_NOT_FOUND)
                    dbgcBpDelete(pDbgc, iBp);
            }
        }
        else
        {
            /* invalid parameter */
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid argument '%s' to '%s'!\n", paArgs[iArg].u.pszString, pCmd->pszCmd);
            break;
        }
    }
    return rc;
}


/**
 * The 'bd' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkDisable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR /*pResult*/)
{
    /*
     * Enumerate the arguments.
     */
    int rc = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs && VBOX_SUCCESS(rc); iArg++)
    {
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
        {
            /* one */
            RTUINT iBp = (RTUINT)paArgs[iArg].u.u64Number;
            if (iBp != paArgs[iArg].u.u64Number)
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Breakpoint id %RX64 is too large!\n", paArgs[iArg].u.u64Number);
                break;
            }
            rc = DBGFR3BpDisable(pVM, iBp);
            if (VBOX_FAILURE(rc))
                rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3BpDisable failed for breakpoint %u!\n", iBp);
        }
        else if (!strcmp(paArgs[iArg].u.pszString, "all"))
        {
            /* all */
            PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
            for (PDBGCBP pBp = pDbgc->pFirstBp; pBp; pBp = pBp->pNext)
            {
                rc = DBGFR3BpDisable(pVM, pBp->iBp);
                if (VBOX_FAILURE(rc))
                    rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3BpDisable failed for breakpoint %u!\n", pBp->iBp);
            }
        }
        else
        {
            /* invalid parameter */
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid argument '%s' to '%s'!\n", paArgs[iArg].u.pszString, pCmd->pszCmd);
            break;
        }
    }
    return rc;
}


/**
 * The 'be' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkEnable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR /*pResult*/)
{
    /*
     * Enumerate the arguments.
     */
    int rc = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs && VBOX_SUCCESS(rc); iArg++)
    {
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
        {
            /* one */
            RTUINT iBp = (RTUINT)paArgs[iArg].u.u64Number;
            if (iBp != paArgs[iArg].u.u64Number)
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Breakpoint id %RX64 is too large!\n", paArgs[iArg].u.u64Number);
                break;
            }
            rc = DBGFR3BpEnable(pVM, iBp);
            if (VBOX_FAILURE(rc))
                rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3BpEnable failed for breakpoint %u!\n", iBp);
        }
        else if (!strcmp(paArgs[iArg].u.pszString, "all"))
        {
            /* all */
            PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
            for (PDBGCBP pBp = pDbgc->pFirstBp; pBp; pBp = pBp->pNext)
            {
                rc = DBGFR3BpEnable(pVM, pBp->iBp);
                if (VBOX_FAILURE(rc))
                    rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3BpEnable failed for breakpoint %u!\n", pBp->iBp);
            }
        }
        else
        {
            /* invalid parameter */
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid argument '%s' to '%s'!\n", paArgs[iArg].u.pszString, pCmd->pszCmd);
            break;
        }
    }
    return rc;
}


/**
 * Breakpoint enumeration callback function.
 *
 * @returns VBox status code. Any failure will stop the enumeration.
 * @param   pVM         The VM handle.
 * @param   pvUser      The user argument.
 * @param   pBp         Pointer to the breakpoint information. (readonly)
 */
static DECLCALLBACK(int) dbgcEnumBreakpointsCallback(PVM pVM, void *pvUser, PCDBGFBP pBp)
{
    PDBGC pDbgc = (PDBGC)pvUser;
    PDBGCBP pDbgcBp = dbgcBpGet(pDbgc, pBp->iBp);

    /*
     * BP type and size.
     */
    char chType;
    char cb = 1;
    switch (pBp->enmType)
    {
        case DBGFBPTYPE_INT3:
            chType = 'p';
            break;
        case DBGFBPTYPE_REG:
            switch (pBp->u.Reg.fType)
            {
                case X86_DR7_RW_EO: chType = 'x'; break;
                case X86_DR7_RW_WO: chType = 'w'; break;
                case X86_DR7_RW_IO: chType = 'i'; break;
                case X86_DR7_RW_RW: chType = 'r'; break;
                default:            chType = '?'; break;

            }
            cb = pBp->u.Reg.cb;
            break;
        case DBGFBPTYPE_REM:
            chType = 'r';
            break;
        default:
            chType = '?';
            break;
    }

    pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "%2u %c %d %c %VGv %04RX64 (%04RX64 to ",
                            pBp->iBp, pBp->fEnabled ? 'e' : 'd', cb, chType,
                            pBp->GCPtr, pBp->cHits, pBp->iHitTrigger);
    if (pBp->iHitDisable == ~(uint64_t)0)
        pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "~0)  ");
    else
        pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "%04RX64)");

    /*
     * Try resolve the address.
     */
    DBGFSYMBOL Sym;
    RTGCINTPTR off;
    int rc = DBGFR3SymbolByAddr(pVM, pBp->GCPtr, &off, &Sym);
    if (VBOX_SUCCESS(rc))
    {
        if (!off)
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "%s", Sym.szName);
        else if (off > 0)
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "%s+%VGv", Sym.szName, off);
        else
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "%s+%VGv", Sym.szName, -off);
    }

    /*
     * The commands.
     */
    if (pDbgcBp)
    {
        if (pDbgcBp->cchCmd)
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\n  cmds: '%s'\n",
                                    pDbgcBp->szCmd);
        else
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\n");
    }
    else
        pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, " [unknown bp]\n");

    return VINF_SUCCESS;
}


/**
 * The 'bl' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkList(PCDBGCCMD /*pCmd*/, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR /*paArgs*/, unsigned /*cArgs*/, PDBGCVAR /*pResult*/)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Enumerate the breakpoints.
     */
    int rc = DBGFR3BpEnum(pVM, dbgcEnumBreakpointsCallback, pDbgc);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3BpEnum failed.\n");
    return rc;
}


/**
 * The 'bp' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkSet(PCDBGCCMD /*pCmd*/, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR /*pResult*/)
{
    /*
     * Convert the pointer to a DBGF address.
     */
    DBGFADDRESS Address;
    int rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, &paArgs[0], &Address);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Couldn't convert '%DV' to a DBGF address, rc=%Vrc.\n", &paArgs[0], rc);

    /*
     * Pick out the optional arguments.
     */
    uint64_t iHitTrigger = 0;
    uint64_t iHitDisable = ~0;
    const char *pszCmds = NULL;
    unsigned iArg = 1;
    if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
    {
        iHitTrigger = paArgs[iArg].u.u64Number;
        iArg++;
        if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
        {
            iHitDisable = paArgs[iArg].u.u64Number;
            iArg++;
        }
    }
    if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_STRING)
    {
        pszCmds = paArgs[iArg].u.pszString;
        iArg++;
    }

    /*
     * Try set the breakpoint.
     */
    RTUINT iBp;
    rc = DBGFR3BpSet(pVM, &Address, iHitTrigger, iHitDisable, &iBp);
    if (VBOX_SUCCESS(rc))
    {
        PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        rc = dbgcBpAdd(pDbgc, iBp, pszCmds);
        if (VBOX_SUCCESS(rc))
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Set breakpoint %u at %VGv\n", iBp, Address.FlatPtr);
        if (rc == VERR_DBGC_BP_EXISTS)
        {
            rc = dbgcBpUpdate(pDbgc, iBp, pszCmds);
            if (VBOX_SUCCESS(rc))
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Updated breakpoint %u at %VGv\n", iBp, Address.FlatPtr);
        }
        int rc2 = DBGFR3BpClear(pDbgc->pVM, iBp);
        AssertRC(rc2);
    }
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Failed to set breakpoint at %VGv, rc=%Vrc.\n", Address.FlatPtr, rc);
}


/**
 * The 'br' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdBrkREM(PCDBGCCMD /*pCmd*/, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR /*pResult*/)
{
    /*
     * Convert the pointer to a DBGF address.
     */
    DBGFADDRESS Address;
    int rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, &paArgs[0], &Address);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Couldn't convert '%DV' to a DBGF address, rc=%Vrc.\n", &paArgs[0], rc);

    /*
     * Pick out the optional arguments.
     */
    uint64_t iHitTrigger = 0;
    uint64_t iHitDisable = ~0;
    const char *pszCmds = NULL;
    unsigned iArg = 1;
    if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
    {
        iHitTrigger = paArgs[iArg].u.u64Number;
        iArg++;
        if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
        {
            iHitDisable = paArgs[iArg].u.u64Number;
            iArg++;
        }
    }
    if (iArg < cArgs && paArgs[iArg].enmType == DBGCVAR_TYPE_STRING)
    {
        pszCmds = paArgs[iArg].u.pszString;
        iArg++;
    }

    /*
     * Try set the breakpoint.
     */
    RTUINT iBp;
    rc = DBGFR3BpSetREM(pVM, &Address, iHitTrigger, iHitDisable, &iBp);
    if (VBOX_SUCCESS(rc))
    {
        PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        rc = dbgcBpAdd(pDbgc, iBp, pszCmds);
        if (VBOX_SUCCESS(rc))
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Set REM breakpoint %u at %VGv\n", iBp, Address.FlatPtr);
        if (rc == VERR_DBGC_BP_EXISTS)
        {
            rc = dbgcBpUpdate(pDbgc, iBp, pszCmds);
            if (VBOX_SUCCESS(rc))
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Updated REM breakpoint %u at %VGv\n", iBp, Address.FlatPtr);
        }
        int rc2 = DBGFR3BpClear(pDbgc->pVM, iBp);
        AssertRC(rc2);
    }
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Failed to set REM breakpoint at %VGv, rc=%Vrc.\n", Address.FlatPtr, rc);
}


/**
 * The 'u' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdUnassemble(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Validate input.
     */
    if (    cArgs > 1
        ||  (cArgs == 1 && !DBGCVAR_ISPOINTER(paArgs[0].enmType)))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: The parser doesn't do its job properly yet.. It might help to use the '%%' operator.\n");
    if (!pVM && !cArgs && !DBGCVAR_ISPOINTER(pDbgc->DisasmPos.enmType))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Don't know where to start disassembling...\n");
    if (!pVM && cArgs && DBGCVAR_ISGCPOINTER(paArgs[0].enmType))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: GC address but no VM.\n");

    /*
     * Find address.
     */
    unsigned fFlags = DBGF_DISAS_FLAGS_NO_ADDRESS;
    if (!cArgs)
    {
        if (!DBGCVAR_ISPOINTER(pDbgc->DisasmPos.enmType))
        {
            pDbgc->DisasmPos.enmType     = DBGCVAR_TYPE_GC_FAR;
            pDbgc->SourcePos.u.GCFar.off = pDbgc->fRegCtxGuest ? CPUMGetGuestEIP(pVM) : CPUMGetHyperEIP(pVM);
            pDbgc->SourcePos.u.GCFar.sel = pDbgc->fRegCtxGuest ? CPUMGetGuestCS(pVM)  : CPUMGetHyperCS(pVM);
            if (pDbgc->fRegCtxGuest)
                fFlags |= DBGF_DISAS_FLAGS_CURRENT_GUEST;
            else
                fFlags |= DBGF_DISAS_FLAGS_CURRENT_HYPER;
        }
        pDbgc->DisasmPos.enmRangeType = DBGCVAR_RANGE_NONE;
    }
    else
        pDbgc->DisasmPos = paArgs[0];

    /*
     * Range.
     */
    switch (pDbgc->DisasmPos.enmRangeType)
    {
        case DBGCVAR_RANGE_NONE:
            pDbgc->DisasmPos.enmRangeType = DBGCVAR_RANGE_ELEMENTS;
            pDbgc->DisasmPos.u64Range     = 10;
            break;

        case DBGCVAR_RANGE_ELEMENTS:
            if (pDbgc->DisasmPos.u64Range > 2048)
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Too many lines requested. Max is 2048 lines.\n");
            break;

        case DBGCVAR_RANGE_BYTES:
            if (pDbgc->DisasmPos.u64Range > 65536)
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The requested range is too big. Max is 64KB.\n");
            break;

        default:
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: Unknown range type %d.\n", pDbgc->DisasmPos.enmRangeType);
    }

    /*
     * Convert physical and host addresses to guest addresses.
     */
    int rc;
    switch (pDbgc->DisasmPos.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_FAR:
            break;
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_HC_FAR:
        {
            DBGCVAR VarTmp;
            rc = pCmdHlp->pfnEval(pCmdHlp, &VarTmp, "%%(%Dv)", &pDbgc->DisasmPos);
            if (VBOX_FAILURE(rc))
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: failed to evaluate '%%(%Dv)' -> %Vrc .\n", pDbgc->DisasmPos, rc);
            pDbgc->DisasmPos = VarTmp;
            break;
        }
        default: AssertFailed(); break;
    }

    /*
     * Print address.
     * todo: Change to list near.
     */
#if 0
    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV:\n", &pDbgc->DisasmPos);
    if (VBOX_FAILURE(rc))
        return rc;
#endif

    /*
     * Do the disassembling.
     */
    unsigned    cTries = 32;
    int         iRangeLeft = (int)pDbgc->DisasmPos.u64Range;
    if (iRangeLeft == 0)                /* klugde for 'r'. */
        iRangeLeft = -1;
    for (;;)
    {
        /*
         * Disassemble the instruction.
         */
        char        szDis[256];
        uint32_t    cbInstr = 1;
        if (pDbgc->DisasmPos.enmType == DBGCVAR_TYPE_GC_FLAT)
            rc = DBGFR3DisasInstrEx(pVM, DBGF_SEL_FLAT, pDbgc->DisasmPos.u.GCFlat, fFlags, &szDis[0], sizeof(szDis), &cbInstr);
        else
            rc = DBGFR3DisasInstrEx(pVM, pDbgc->DisasmPos.u.GCFar.sel, pDbgc->DisasmPos.u.GCFar.off, fFlags, &szDis[0], sizeof(szDis), &cbInstr);
        if (VBOX_SUCCESS(rc))
        {
            /* print it */
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%-16DV %s\n", &pDbgc->DisasmPos, &szDis[0]);
            if (VBOX_FAILURE(rc))
                return rc;
        }
        else
        {
            /* bitch. */
            int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Failed to disassemble instruction, skipping one byte.\n");
            if (VBOX_FAILURE(rc))
                return rc;
            if (cTries-- > 0)
                return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Too many disassembly failures. Giving up.\n");
            cbInstr = 1;
        }

        /* advance */
        if (iRangeLeft < 0)             /* 'r' */
            break;
        if (pDbgc->DisasmPos.enmRangeType == DBGCVAR_RANGE_ELEMENTS)
            iRangeLeft--;
        else
            iRangeLeft -= cbInstr;
        rc = pCmdHlp->pfnEval(pCmdHlp, &pDbgc->DisasmPos, "(%Dv) + %x", &pDbgc->DisasmPos, cbInstr);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Expression: (%Dv) + %x\n", &pDbgc->DisasmPos, cbInstr);
        if (iRangeLeft <= 0)
            break;
        fFlags &= ~(DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_CURRENT_HYPER);
    }

    NOREF(pCmd); NOREF(pResult);
    return 0;
}


/**
 * The 'ls' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdListSource(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Validate input.
     */
    if (    cArgs > 1
        ||  (cArgs == 1 && !DBGCVAR_ISPOINTER(paArgs[0].enmType)))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: The parser doesn't do its job properly yet.. It might help to use the '%%' operator.\n");
    if (!pVM && !cArgs && !DBGCVAR_ISPOINTER(pDbgc->SourcePos.enmType))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Don't know where to start disassembling...\n");
    if (!pVM && cArgs && DBGCVAR_ISGCPOINTER(paArgs[0].enmType))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: GC address but no VM.\n");

    /*
     * Find address.
     */
    if (!cArgs)
    {
        if (!DBGCVAR_ISPOINTER(pDbgc->SourcePos.enmType))
        {
            pDbgc->SourcePos.enmType     = DBGCVAR_TYPE_GC_FAR;
            pDbgc->SourcePos.u.GCFar.off = pDbgc->fRegCtxGuest ? CPUMGetGuestEIP(pVM) : CPUMGetHyperEIP(pVM);
            pDbgc->SourcePos.u.GCFar.sel = pDbgc->fRegCtxGuest ? CPUMGetGuestCS(pVM)  : CPUMGetHyperCS(pVM);
        }
        pDbgc->SourcePos.enmRangeType = DBGCVAR_RANGE_NONE;
    }
    else
        pDbgc->SourcePos = paArgs[0];

    /*
     * Ensure the the source address is flat GC.
     */
    switch (pDbgc->SourcePos.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            break;
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_HC_FAR:
        {
            int rc = pCmdHlp->pfnEval(pCmdHlp, &pDbgc->SourcePos, "%%(%Dv)", &pDbgc->SourcePos);
            if (VBOX_FAILURE(rc))
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid address or address type. (rc=%d)\n", rc);
            break;
        }
        default: AssertFailed(); break;
    }

    /*
     * Range.
     */
    switch (pDbgc->SourcePos.enmRangeType)
    {
        case DBGCVAR_RANGE_NONE:
            pDbgc->SourcePos.enmRangeType = DBGCVAR_RANGE_ELEMENTS;
            pDbgc->SourcePos.u64Range     = 10;
            break;

        case DBGCVAR_RANGE_ELEMENTS:
            if (pDbgc->SourcePos.u64Range > 2048)
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Too many lines requested. Max is 2048 lines.\n");
            break;

        case DBGCVAR_RANGE_BYTES:
            if (pDbgc->SourcePos.u64Range > 65536)
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The requested range is too big. Max is 64KB.\n");
            break;

        default:
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: Unknown range type %d.\n", pDbgc->SourcePos.enmRangeType);
    }

    /*
     * Do the disassembling.
     */
    bool        fFirst = 1;
    DBGFLINE    LinePrev = { 0, 0, "" };
    int         iRangeLeft = (int)pDbgc->SourcePos.u64Range;
    if (iRangeLeft == 0)                /* klugde for 'r'. */
        iRangeLeft = -1;
    for (;;)
    {
        /*
         * Get line info.
         */
        DBGFLINE    Line;
        RTGCINTPTR  off;
        int rc = DBGFR3LineByAddr(pVM, pDbgc->SourcePos.u.GCFlat, &off, &Line);
        if (VBOX_FAILURE(rc))
            return VINF_SUCCESS;

        unsigned cLines = 0;
        if (memcmp(&Line, &LinePrev, sizeof(Line)))
        {
            /*
             * Print filenamename
             */
            if (!fFirst && strcmp(Line.szFilename, LinePrev.szFilename))
                fFirst = true;
            if (fFirst)
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "[%s @ %d]\n", Line.szFilename, Line.uLineNo);
                if (VBOX_FAILURE(rc))
                    return rc;
            }

            /*
             * Try open the file and read the line.
             */
            FILE *phFile = fopen(Line.szFilename, "r");
            if (phFile)
            {
                /* Skip ahead to the desired line. */
                char szLine[4096];
                unsigned cBefore = fFirst ? RT_MIN(2, Line.uLineNo - 1) : Line.uLineNo - LinePrev.uLineNo - 1;
                if (cBefore > 7)
                    cBefore = 0;
                unsigned cLeft = Line.uLineNo - cBefore;
                while (cLeft > 0)
                {
                    szLine[0] = '\0';
                    if (!fgets(szLine, sizeof(szLine), phFile))
                        break;
                    cLeft--;
                }
                if (!cLeft)
                {
                    /* print the before lines */
                    for (;;)
                    {
                        size_t cch = strlen(szLine);
                        while (cch > 0 && (szLine[cch - 1] == '\r' ||  szLine[cch - 1] == '\n' || isspace(szLine[cch - 1])) )
                            szLine[--cch] = '\0';
                        if (cBefore-- <= 0)
                            break;

                        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "         %4d: %s\n", Line.uLineNo - cBefore - 1, szLine);
                        szLine[0] = '\0';
                        fgets(szLine, sizeof(szLine), phFile);
                        cLines++;
                    }
                    /* print the actual line */
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%08llx %4d: %s\n", Line.Address, Line.uLineNo, szLine);
                }
                fclose(phFile);
                if (VBOX_FAILURE(rc))
                    return rc;
                fFirst = false;
            }
            else
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Warning: couldn't open source file '%s'\n", Line.szFilename);

            LinePrev = Line;
        }


        /*
         * Advance
         */
        if (iRangeLeft < 0)             /* 'r' */
            break;
        if (pDbgc->SourcePos.enmRangeType == DBGCVAR_RANGE_ELEMENTS)
            iRangeLeft -= cLines;
        else
            iRangeLeft -= 1;
        rc = pCmdHlp->pfnEval(pCmdHlp, &pDbgc->SourcePos, "(%Dv) + %x", &pDbgc->SourcePos, 1);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Expression: (%Dv) + %x\n", &pDbgc->SourcePos, 1);
        if (iRangeLeft <= 0)
            break;
    }

    NOREF(pCmd); NOREF(pResult);
    return 0;
}


/**
 * The 'r' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdReg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    if (pDbgc->fRegCtxGuest)
        return dbgcCmdRegGuest(pCmd, pCmdHlp, pVM, paArgs, cArgs, pResult);
    else
        return dbgcCmdRegHyper(pCmd, pCmdHlp, pVM, paArgs, cArgs, pResult);
}


/**
 * Common worker for the dbgcCmdReg*() commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 * @param   pszPrefix   The symbol prefix.
 */
static DECLCALLBACK(int) dbgcCmdRegCommon(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult, const char *pszPrefix)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * cArgs == 0: Show all
     */
    if (cArgs == 0)
    {
        /*
         * Get register context.
         */
        int             rc;
        PCPUMCTX        pCtx;
        PCCPUMCTXCORE   pCtxCore;
        if (!*pszPrefix)
        {
            rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
            pCtxCore = CPUMGetGuestCtxCore(pVM);
        }
        else
        {
            rc = CPUMQueryHyperCtxPtr(pVM, &pCtx);
            pCtxCore = CPUMGetHyperCtxCore(pVM);
        }
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Getting register context\n");

        /*
         * Format the flags.
         */
        static struct
        {
            const char *pszSet; const char *pszClear; uint32_t fFlag;
        }   aFlags[] =
        {
            { "vip",NULL, X86_EFL_VIP },
            { "vif",NULL, X86_EFL_VIF },
            { "ac", NULL, X86_EFL_AC },
            { "vm", NULL, X86_EFL_VM },
            { "rf", NULL, X86_EFL_RF },
            { "nt", NULL, X86_EFL_NT },
            { "ov", "nv", X86_EFL_OF },
            { "dn", "up", X86_EFL_DF },
            { "ei", "di", X86_EFL_IF },
            { "tf", NULL, X86_EFL_TF },
            { "nt", "pl", X86_EFL_SF },
            { "nz", "zr", X86_EFL_ZF },
            { "ac", "na", X86_EFL_AF },
            { "po", "pe", X86_EFL_PF },
            { "cy", "nc", X86_EFL_CF },
        };
        char szEFlags[80];
        char *psz = szEFlags;
        uint32_t efl = pCtxCore->eflags.u32;
        for (unsigned i = 0; i < ELEMENTS(aFlags); i++)
        {
            const char *pszAdd = aFlags[i].fFlag & efl ? aFlags[i].pszSet : aFlags[i].pszClear;
            if (pszAdd)
            {
                strcpy(psz, pszAdd);
                psz += strlen(pszAdd);
                *psz++ = ' ';
            }
        }
        psz[-1] = '\0';


        /*
         * Format the registers.
         */
        if (pDbgc->fRegTerse)
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                "%scs=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %sss=%04x               %seflags=%08x\n",
                pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 34 : 31, szEFlags,
                pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, efl);
        }
        else
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                "%scs={%04x base=%08x limit=%08x flags=%08x} %sdr0=%08x %sdr1=%08x\n"
                "%sds={%04x base=%08x limit=%08x flags=%08x} %sdr2=%08x %sdr3=%08x\n"
                "%ses={%04x base=%08x limit=%08x flags=%08x} %sdr4=%08x %sdr5=%08x\n"
                "%sfs={%04x base=%08x limit=%08x flags=%08x} %sdr6=%08x %sdr7=%08x\n"
                "%sgs={%04x base=%08x limit=%08x flags=%08x} %scr0=%08x %scr2=%08x\n"
                "%sss={%04x base=%08x limit=%08x flags=%08x} %scr3=%08x %scr4=%08x\n"
                "%sgdtr=%08x:%04x  %sidtr=%08x:%04x  %seflags=%08x\n"
                "%sldtr={%04x base=%08x limit=%08x flags=%08x}\n"
                "%str  ={%04x base=%08x limit=%08x flags=%08x}\n"
                "%sSysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
                "%sFCW=%04x %sFSW=%04x %sFTW=%04x\n"
                ,
                pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                pszPrefix, (RTSEL)pCtxCore->cs, pCtx->csHid.u32Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pszPrefix, pCtx->dr0,  pszPrefix, pCtx->dr1,
                pszPrefix, (RTSEL)pCtxCore->ds, pCtx->dsHid.u32Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pszPrefix, pCtx->dr2,  pszPrefix, pCtx->dr3,
                pszPrefix, (RTSEL)pCtxCore->es, pCtx->esHid.u32Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pszPrefix, pCtx->dr4,  pszPrefix, pCtx->dr5,
                pszPrefix, (RTSEL)pCtxCore->fs, pCtx->fsHid.u32Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pszPrefix, pCtx->dr6,  pszPrefix, pCtx->dr7,
                pszPrefix, (RTSEL)pCtxCore->gs, pCtx->gsHid.u32Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2,
                pszPrefix, (RTSEL)pCtxCore->ss, pCtx->ssHid.u32Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                pszPrefix, pCtx->gdtr.pGdt,pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, pCtxCore->eflags,
                pszPrefix, (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u32Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
                pszPrefix, (RTSEL)pCtx->tr, pCtx->trHid.u32Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
                pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp,
                pszPrefix, pCtx->fpu.FCW, pszPrefix, pCtx->fpu.FSW, pszPrefix, pCtx->fpu.FTW);
        }

        /*
         * Disassemble one instruction at cs:eip.
         */
        return pCmdHlp->pfnExec(pCmdHlp, "u %04x:%08x L 0", pCtx->cs, pCtx->eip);
    }

    /*
     * cArgs == 1: Show the register.
     * cArgs == 2: Modify the register.
     */
    if (    cArgs == 1
        ||  cArgs == 2)
    {
        /* locate the register symbol. */
        const char *pszReg = paArgs[0].u.pszString;
        if (    *pszPrefix
            &&  pszReg[0] != *pszPrefix)
        {
            /* prepend the prefix. */
            char *psz = (char *)alloca(strlen(pszReg) + 2);
            psz[0] = *pszPrefix;
            strcpy(psz + 1, paArgs[0].u.pszString);
            pszReg = psz;
        }
        PCDBGCSYM pSym = dbgcLookupRegisterSymbol(pDbgc, pszReg);
        if (!pSym)
            return pCmdHlp->pfnVBoxError(pCmdHlp, VERR_INVALID_PARAMETER /* VERR_DBGC_INVALID_REGISTER */, "Invalid register name '%s'.\n", pszReg);

        /* show the register */
        if (cArgs == 1)
        {
            DBGCVAR Var;
            memset(&Var, 0, sizeof(Var));
            int rc = pSym->pfnGet(pSym, pCmdHlp, DBGCVAR_TYPE_NUMBER, &Var);
            if (VBOX_FAILURE(rc))
                return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Failed getting value for register '%s'.\n", pszReg);
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%s=%Dv\n", pszReg, &Var);
        }

        /* change the register */
        int rc = pSym->pfnSet(pSym, pCmdHlp, &paArgs[1]);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Failed setting value for register '%s'.\n", pszReg);
        return VINF_SUCCESS;
    }


    NOREF(pCmd); NOREF(paArgs); NOREF(pResult);
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Huh? cArgs=%d Expected 0, 1 or 2!\n", cArgs);
}


/**
 * The 'rg' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdRegGuest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    return dbgcCmdRegCommon(pCmd, pCmdHlp, pVM, paArgs, cArgs, pResult, "");
}


/**
 * The 'rh' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdRegHyper(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    return dbgcCmdRegCommon(pCmd, pCmdHlp, pVM, paArgs, cArgs, pResult, ".");
}


/**
 * The 'rt' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdRegTerse(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    NOREF(pCmd); NOREF(pVM); NOREF(paArgs); NOREF(cArgs); NOREF(pResult);

    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    pDbgc->fRegTerse = !pDbgc->fRegTerse;
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, pDbgc->fRegTerse ? "info: Terse register info.\n" : "info: Verbose register info.\n");
}


/**
 * The 't' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdTrace(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    int rc = DBGFR3Step(pVM);
    if (VBOX_SUCCESS(rc))
        pDbgc->fReady = false;
    else
        rc = pDbgc->CmdHlp.pfnVBoxError(&pDbgc->CmdHlp, rc, "When trying to single step VM %p\n", pDbgc->pVM);

    NOREF(pCmd); NOREF(paArgs); NOREF(cArgs); NOREF(pResult);
    return rc;
}


/**
 * The 'k', 'kg' and 'kh' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdStack(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Figure which context we're called for.
     */
    bool fGuest = pCmd->pszCmd[1] == 'g'
               || (!pCmd->pszCmd[1] && pDbgc->fRegCtxGuest);


    DBGFSTACKFRAME Frame;
    memset(&Frame, 0, sizeof(Frame));
    int rc;
    if (fGuest)
        rc = DBGFR3StackWalkBeginGuest(pVM, &Frame);
    else
        rc = DBGFR3StackWalkBeginHyper(pVM, &Frame);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Failed to begin stack walk, rc=%Vrc\n", rc);

    /*
     * Print header.
     *                                      12345678 12345678 0023:87654321 12345678 87654321 12345678 87654321 symbol
     */
    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "EBP      Ret EBP  Ret CS:EIP    Arg0     Arg1     Arg2     Arg3     CS:EIP / Symbol [line]\n");
    if (VBOX_FAILURE(rc))
        return rc;
    do
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%08RX32 %08RX32 %04RX32:%08RX32 %08RX32 %08RX32 %08RX32 %08RX32",
                                (uint32_t)Frame.AddrFrame.off,
                                (uint32_t)Frame.AddrReturnFrame.off,
                                (uint32_t)Frame.AddrReturnPC.Sel,
                                (uint32_t)Frame.AddrReturnPC.off,
                                Frame.Args.au32[0],
                                Frame.Args.au32[1],
                                Frame.Args.au32[2],
                                Frame.Args.au32[3]);
        if (VBOX_FAILURE(rc))
            return rc;
        if (!Frame.pSymPC)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %RTsel:%08RGv", Frame.AddrPC.Sel, Frame.AddrPC.off);
        else
        {
            RTGCINTPTR offDisp = Frame.AddrPC.FlatPtr - Frame.pSymPC->Value; /** @todo this isn't 100% correct for segemnted stuff. */
            if (offDisp > 0)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %s+%llx", Frame.pSymPC->szName, (int64_t)offDisp);
            else if (offDisp < 0)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %s-%llx", Frame.pSymPC->szName, -(int64_t)offDisp);
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %s", Frame.pSymPC->szName);
        }
        if (VBOX_SUCCESS(rc) && Frame.pLinePC)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " [%s @ 0i%d]", Frame.pLinePC->szFilename, Frame.pLinePC->uLineNo);
        if (VBOX_SUCCESS(rc))
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
        if (VBOX_FAILURE(rc))
            return rc;

        /* next */
        rc = DBGFR3StackWalkNext(pVM, &Frame);
    } while (VBOX_SUCCESS(rc));

    NOREF(paArgs); NOREF(cArgs); NOREF(pResult);
    return VINF_SUCCESS;
}


static int dbgcCmdDumpDTWorker64(PDBGCCMDHLP /*pCmdHlp*/, PCX86DESC64 /*pDesc*/, unsigned /*iEntry*/, bool /* fHyper */, bool * /*fDblEntry*/)
{
    /* GUEST64 */
    return VINF_SUCCESS;
}


/**
 * Wroker function that displays one descriptor entry (GDT, LDT, IDT).
 *
 * @returns pfnPrintf status code.
 * @param   pCmdHlp     The DBGC command helpers.
 * @param   pDesc       The descriptor to display.
 * @param   iEntry      The descriptor entry number.
 * @param   fHyper      Whether the selector belongs to the hypervisor or not.
 */
static int dbgcCmdDumpDTWorker32(PDBGCCMDHLP pCmdHlp, PCX86DESC pDesc, unsigned iEntry, bool fHyper)
{
    int rc;

    const char *pszHyper = fHyper ? " HYPER" : "";
    const char *pszPresent = pDesc->Gen.u1Present ? "P " : "NP";
    if (pDesc->Gen.u1DescType)
    {
        static const char * const s_apszTypes[] =
        {
            "DataRO", /* 0 Read-Only */
            "DataRO", /* 1 Read-Only - Accessed */
            "DataRW", /* 2 Read/Write  */
            "DataRW", /* 3 Read/Write - Accessed  */
            "DownRO", /* 4 Expand-down, Read-Only  */
            "DownRO", /* 5 Expand-down, Read-Only - Accessed */
            "DownRW", /* 6 Expand-down, Read/Write  */
            "DownRO", /* 7 Expand-down, Read/Write - Accessed */
            "CodeEO", /* 8 Execute-Only */
            "CodeEO", /* 9 Execute-Only - Accessed */
            "CodeER", /* A Execute/Readable */
            "CodeER", /* B Execute/Readable - Accessed */
            "ConfE0", /* C Conforming, Execute-Only */
            "ConfE0", /* D Conforming, Execute-Only - Accessed */
            "ConfER", /* E Conforming, Execute/Readable */
            "ConfER"  /* F Conforming, Execute/Readable - Accessed */
        };
        const char *pszAccessed = pDesc->Gen.u4Type & RT_BIT(0) ? "A " : "NA";
        const char *pszGranularity = pDesc->Gen.u1Granularity ? "G" : " ";
        const char *pszBig = pDesc->Gen.u1DefBig ? "BIG" : "   ";
        uint32_t u32Base = pDesc->Gen.u16BaseLow
                         | ((uint32_t)pDesc->Gen.u8BaseHigh1 << 16)
                         | ((uint32_t)pDesc->Gen.u8BaseHigh2 << 24);
        uint32_t cbLimit = pDesc->Gen.u16LimitLow | (pDesc->Gen.u4LimitHigh << 16);
        if (pDesc->Gen.u1Granularity)
            cbLimit <<= PAGE_SHIFT;

        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Bas=%08x Lim=%08x DPL=%d %s %s %s %s AVL=%d R=%d%s\n",
                                iEntry, s_apszTypes[pDesc->Gen.u4Type], u32Base, cbLimit,
                                pDesc->Gen.u2Dpl, pszPresent, pszAccessed, pszGranularity, pszBig,
                                pDesc->Gen.u1Available, pDesc->Gen.u1Reserved, pszHyper);
    }
    else
    {
        static const char * const s_apszTypes[] =
        {
            "Ill-0 ", /* 0 0000 Reserved (Illegal) */
            "Tss16A", /* 1 0001 Available 16-bit TSS */
            "LDT   ", /* 2 0010 LDT */
            "Tss16B", /* 3 0011 Busy 16-bit TSS */
            "Call16", /* 4 0100 16-bit Call Gate */
            "TaskG ", /* 5 0101 Task Gate */
            "Int16 ", /* 6 0110 16-bit Interrupt Gate */
            "Trap16", /* 7 0111 16-bit Trap Gate */
            "Ill-8 ", /* 8 1000 Reserved (Illegal) */
            "Tss32A", /* 9 1001 Available 32-bit TSS */
            "Ill-A ", /* A 1010 Reserved (Illegal) */
            "Tss32B", /* B 1011 Busy 32-bit TSS */
            "Call32", /* C 1100 32-bit Call Gate */
            "Ill-D ", /* D 1101 Reserved (Illegal) */
            "Int32 ", /* E 1110 32-bit Interrupt Gate */
            "Trap32"  /* F 1111 32-bit Trap Gate */
        };
        switch (pDesc->Gen.u4Type)
        {
            /* raw */
            case X86_SEL_TYPE_SYS_UNDEFINED:
            case X86_SEL_TYPE_SYS_UNDEFINED2:
            case X86_SEL_TYPE_SYS_UNDEFINED4:
            case X86_SEL_TYPE_SYS_UNDEFINED3:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s %.8Rhxs   DPL=%d %s%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], pDesc,
                                        pDesc->Gen.u2Dpl, pszPresent, pszHyper);
                break;

            case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
            case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
            case X86_SEL_TYPE_SYS_286_TSS_BUSY:
            case X86_SEL_TYPE_SYS_386_TSS_BUSY:
            case X86_SEL_TYPE_SYS_LDT:
            {
                const char *pszGranularity = pDesc->Gen.u1Granularity ? "G" : " ";
                const char *pszBusy = pDesc->Gen.u4Type & RT_BIT(1) ? "B " : "NB";
                const char *pszBig = pDesc->Gen.u1DefBig ? "BIG" : "   ";
                uint32_t u32Base = pDesc->Gen.u16BaseLow
                                 | ((uint32_t)pDesc->Gen.u8BaseHigh1 << 16)
                                 | ((uint32_t)pDesc->Gen.u8BaseHigh2 << 24);
                uint32_t cbLimit = pDesc->Gen.u16LimitLow | (pDesc->Gen.u4LimitHigh << 16);
                if (pDesc->Gen.u1Granularity)
                    cbLimit <<= PAGE_SHIFT;

                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Bas=%08x Lim=%08x DPL=%d %s %s %s %s AVL=%d R=%d%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], u32Base, cbLimit,
                                        pDesc->Gen.u2Dpl, pszPresent, pszBusy, pszGranularity, pszBig,
                                        pDesc->Gen.u1Available, pDesc->Gen.u1Reserved | (pDesc->Gen.u1DefBig << 1),
                                        pszHyper);
                break;
            }

            case X86_SEL_TYPE_SYS_TASK_GATE:
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s TSS=%04x                  DPL=%d %s%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], pDesc->au16[1],
                                        pDesc->Gen.u2Dpl, pszPresent, pszHyper);
                break;
            }

            case X86_SEL_TYPE_SYS_286_CALL_GATE:
            case X86_SEL_TYPE_SYS_386_CALL_GATE:
            {
                unsigned cParams = pDesc->au8[0] & 0x1f;
                const char *pszCountOf = pDesc->Gen.u4Type & RT_BIT(3) ? "DC" : "WC";
                RTSEL sel = pDesc->au16[1];
                uint32_t off = pDesc->au16[0] | ((uint32_t)pDesc->au16[3] << 16);
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Sel:Off=%04x:%08x     DPL=%d %s %s=%d%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], sel, off,
                                        pDesc->Gen.u2Dpl, pszPresent, pszCountOf, cParams, pszHyper);
                break;
            }

            case X86_SEL_TYPE_SYS_286_INT_GATE:
            case X86_SEL_TYPE_SYS_386_INT_GATE:
            case X86_SEL_TYPE_SYS_286_TRAP_GATE:
            case X86_SEL_TYPE_SYS_386_TRAP_GATE:
            {
                RTSEL sel = pDesc->au16[1];
                uint32_t off = pDesc->au16[0] | ((uint32_t)pDesc->au16[3] << 16);
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Sel:Off=%04x:%08x     DPL=%d %s%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], sel, off,
                                        pDesc->Gen.u2Dpl, pszPresent, pszHyper);
                break;
            }

            /* impossible, just it's necessary to keep gcc happy. */
            default:
                return VINF_SUCCESS;
        }
    }
    return rc;
}


/**
 * The 'dg', 'dga', 'dl' and 'dla' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");

    /*
     * Get the CPU mode, check which command variation this is
     * and fix a default parameter if needed.
     */
    CPUMMODE enmMode = CPUMGetGuestMode(pVM);
    bool fGdt = pCmd->pszCmd[1] == 'g';
    bool fAll = pCmd->pszCmd[2] == 'a';

    DBGCVAR Var;
    if (!cArgs)
    {
        cArgs = 1;
        paArgs = &Var;
        Var.enmType = DBGCVAR_TYPE_NUMBER;
        Var.u.u64Number = fGdt ? 0 : 4;
        Var.enmRangeType = DBGCVAR_RANGE_ELEMENTS;
        Var.u64Range = 1024;
    }

    /*
     * Process the arguments.
     */
    for (unsigned i = 0; i < cArgs; i++)
    {
         /*
          * Retrive the selector value from the argument.
          * The parser may confuse pointers and numbers if more than one
          * argument is given, that that into account.
          */
        /* check that what've got makes sense as we don't trust the parser yet. */
        if (    paArgs[i].enmType != DBGCVAR_TYPE_NUMBER
            &&  !DBGCVAR_ISPOINTER(paArgs[i].enmType))
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: arg #%u isn't of number or pointer type but %d.\n", i, paArgs[i].enmType);
        uint64_t u64;
        unsigned cSels = 1;
        switch (paArgs[i].enmType)
        {
            case DBGCVAR_TYPE_NUMBER:
                u64 = paArgs[i].u.u64Number;
                if (paArgs[i].enmRangeType != DBGCVAR_RANGE_NONE)
                    cSels = RT_MIN(paArgs[i].u64Range, 1024);
                break;
            case DBGCVAR_TYPE_GC_FAR:   u64 = paArgs[i].u.GCFar.sel; break;
            case DBGCVAR_TYPE_GC_FLAT:  u64 = paArgs[i].u.GCFlat; break;
            case DBGCVAR_TYPE_GC_PHYS:  u64 = paArgs[i].u.GCPhys; break;
            case DBGCVAR_TYPE_HC_FAR:   u64 = paArgs[i].u.HCFar.sel; break;
            case DBGCVAR_TYPE_HC_FLAT:  u64 = (uintptr_t)paArgs[i].u.pvHCFlat; break;
            case DBGCVAR_TYPE_HC_PHYS:  u64 = paArgs[i].u.HCPhys; break;
            default:                    u64 = _64K; break;
        }
        if (u64 < _64K)
        {
            unsigned Sel = (RTSEL)u64;

            /*
             * Dump the specified range.
             */
            bool fSingle = cSels == 1;
            while (     cSels-- > 0
                   &&   Sel < _64K)
            {
                SELMSELINFO SelInfo;
                int rc = SELMR3GetSelectorInfo(pVM, Sel, &SelInfo);
                if (RT_SUCCESS(rc))
                {
                    if (SelInfo.fRealMode)
                        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x RealM   Bas=%04x     Lim=%04x\n",
                                                Sel, (unsigned)SelInfo.GCPtrBase, (unsigned)SelInfo.cbLimit);
                    else if (fAll || fSingle || SelInfo.Raw.Gen.u1Present)
                    {
                        if (enmMode == CPUMMODE_PROTECTED)
                            rc = dbgcCmdDumpDTWorker32(pCmdHlp, (PX86DESC)&SelInfo.Raw, Sel, SelInfo.fHyper);
                        else
                        {
                            bool fDblSkip = false;
                            rc = dbgcCmdDumpDTWorker64(pCmdHlp, (PX86DESC64)&SelInfo.Raw, Sel, SelInfo.fHyper, &fDblSkip);
                            if (fDblSkip)
                                Sel += 4;
                        }
                    }
                }
                else
                {
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %Vrc\n", Sel, rc);
                    if (!fAll)
                        return rc;
                }
                if (RT_FAILURE(rc))
                    return rc;

                /* next */
                Sel += 4;
            }
        }
        else
            pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: %llx is out of bounds\n", u64);
    }

    NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * The 'di' and 'dia' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpIDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");

    /*
     * Establish some stuff like the current IDTR and CPU mode,
     * and fix a default parameter.
     */
    uint16_t cbLimit;
    RTGCUINTPTR GCPtrBase = CPUMGetGuestIDTR(pVM, &cbLimit);
    CPUMMODE enmMode = CPUMGetGuestMode(pVM);
    unsigned cbEntry;
    switch (enmMode)
    {
        case CPUMMODE_REAL:         cbEntry = sizeof(RTFAR16); break;
        case CPUMMODE_PROTECTED:    cbEntry = sizeof(X86DESC); break;
        case CPUMMODE_LONG:         cbEntry = sizeof(X86DESC64); break;
        default:
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid CPU mode %d.\n", enmMode);
    }

    bool fAll = pCmd->pszCmd[2] == 'a';
    DBGCVAR Var;
    if (!cArgs)
    {
        cArgs = 1;
        paArgs = &Var;
        Var.enmType = DBGCVAR_TYPE_NUMBER;
        Var.u.u64Number = 0;
        Var.enmRangeType = DBGCVAR_RANGE_ELEMENTS;
        Var.u64Range = 256;
    }

    /*
     * Process the arguments.
     */
    for (unsigned i = 0; i < cArgs; i++)
    {
        /* check that what've got makes sense as we don't trust the parser yet. */
        if (paArgs[i].enmType != DBGCVAR_TYPE_NUMBER)
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: arg #%u isn't of number type but %d.\n", i, paArgs[i].enmType);
        if (paArgs[i].u.u64Number < 256)
        {
            RTGCUINTPTR iInt = (RTGCUINTPTR)paArgs[i].u.u64Number;
            unsigned cInts = paArgs[i].enmRangeType != DBGCVAR_RANGE_NONE
                           ? paArgs[i].u64Range
                           : 1;
            bool fSingle = cInts == 1;
            while (     cInts-- > 0
                   &&   iInt < 256)
            {
                /*
                 * Try read it.
                 */
                union
                {
                    RTFAR16 Real;
                    X86DESC Prot;
                    X86DESC64 Long;
                } u;
                if (iInt * cbEntry  + (cbEntry - 1) > cbLimit)
                {
                    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x not within the IDT\n", (unsigned)iInt);
                    if (!fAll && !fSingle)
                        return VINF_SUCCESS;
                }
                DBGCVAR AddrVar;
                AddrVar.enmType = DBGCVAR_TYPE_GC_FLAT;
                AddrVar.u.GCFlat = GCPtrBase + iInt * cbEntry;
                AddrVar.enmRangeType = DBGCVAR_RANGE_NONE;
                int rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &u, cbEntry, &AddrVar, NULL);
                if (VBOX_FAILURE(rc))
                    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Reading IDT entry %#04x.\n", (unsigned)iInt);

                /*
                 * Display it.
                 */
                switch (enmMode)
                {
                    case CPUMMODE_REAL:
                        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %RTfp16\n", (unsigned)iInt, u.Real);
                        /** @todo resolve 16:16 IDTE to a symbol */
                        break;
                    case CPUMMODE_PROTECTED:
                        if (fAll || fSingle || u.Prot.Gen.u1Present)
                            rc = dbgcCmdDumpDTWorker32(pCmdHlp, &u.Prot, iInt, false);
                        break;
                    case CPUMMODE_LONG:
                        if (fAll || fSingle || u.Long.Gen.u1Present)
                            rc = dbgcCmdDumpDTWorker64(pCmdHlp, &u.Long, iInt, false, NULL);
                        break;
                    default: break; /* to shut up gcc */
                }
                if (RT_FAILURE(rc))
                    return rc;

                /* next */
                iInt++;
            }
        }
        else
            pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: %llx is out of bounds (max 256)\n", paArgs[i].u.u64Number);
    }

    NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * The 'da', 'dq', 'dd', 'dw' and 'db' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Validate input.
     */
    if (    cArgs > 1
        ||  (cArgs == 1 && !DBGCVAR_ISPOINTER(paArgs[0].enmType)))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: The parser doesn't do its job properly yet.. It might help to use the '%%' operator.\n");
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");

    /*
     * Figure out the element size.
     */
    unsigned    cbElement;
    bool        fAscii = false;
    switch (pCmd->pszCmd[1])
    {
        default:
        case 'b':   cbElement = 1; break;
        case 'w':   cbElement = 2; break;
        case 'd':   cbElement = 4; break;
        case 'q':   cbElement = 8; break;
        case 'a':
            cbElement = 1;
            fAscii = true;
            break;
        case '\0':
            fAscii = !!(pDbgc->cbDumpElement & 0x80000000);
            cbElement = pDbgc->cbDumpElement & 0x7fffffff;
            if (!cbElement)
                cbElement = 1;
            break;
    }

    /*
     * Find address.
     */
    if (!cArgs)
        pDbgc->DumpPos.enmRangeType = DBGCVAR_RANGE_NONE;
    else
        pDbgc->DumpPos = paArgs[0];

    /*
     * Range.
     */
    switch (pDbgc->DumpPos.enmRangeType)
    {
        case DBGCVAR_RANGE_NONE:
            pDbgc->DumpPos.enmRangeType = DBGCVAR_RANGE_BYTES;
            pDbgc->DumpPos.u64Range     = 0x60;
            break;

        case DBGCVAR_RANGE_ELEMENTS:
            if (pDbgc->DumpPos.u64Range > 2048)
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Too many elements requested. Max is 2048 elements.\n");
            pDbgc->DumpPos.enmRangeType = DBGCVAR_RANGE_BYTES;
            pDbgc->DumpPos.u64Range     = (cbElement ? cbElement : 1) * pDbgc->DumpPos.u64Range;
            break;

        case DBGCVAR_RANGE_BYTES:
            if (pDbgc->DumpPos.u64Range > 65536)
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The requested range is too big. Max is 64KB.\n");
            break;

        default:
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: Unknown range type %d.\n", pDbgc->DumpPos.enmRangeType);
    }

    /*
     * Do the dumping.
     */
    pDbgc->cbDumpElement = cbElement | (fAscii << 31);
    int     cbLeft = (int)pDbgc->DumpPos.u64Range;
    uint8_t u8Prev = '\0';
    for (;;)
    {
        /*
         * Read memory.
         */
        char    achBuffer[16];
        size_t  cbReq = RT_MIN((int)sizeof(achBuffer), cbLeft);
        size_t  cb = RT_MIN((int)sizeof(achBuffer), cbLeft);
        int rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &achBuffer, cbReq, &pDbgc->DumpPos, &cb);
        if (VBOX_FAILURE(rc))
        {
            if (u8Prev && u8Prev != '\n')
                pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Reading memory at %DV.\n", &pDbgc->DumpPos);
        }

        /*
         * Display it.
         */
        memset(&achBuffer[cb], 0, sizeof(achBuffer) - cb);
        if (!fAscii)
        {
            pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV:", &pDbgc->DumpPos);
            unsigned i;
            for (i = 0; i < cb; i += cbElement)
            {
                const char *pszSpace = " ";
                if (cbElement <= 2 && i == 8 && !fAscii)
                    pszSpace = "-";
                switch (cbElement)
                {
                    case 1: pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%s%02x",    pszSpace, *(uint8_t *)&achBuffer[i]); break;
                    case 2: pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%s%04x",    pszSpace, *(uint16_t *)&achBuffer[i]); break;
                    case 4: pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%s%08x",    pszSpace, *(uint32_t *)&achBuffer[i]); break;
                    case 8: pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%s%016llx", pszSpace, *(uint64_t *)&achBuffer[i]); break;
                }
            }

            /* chars column */
            if (pDbgc->cbDumpElement == 1)
            {
                while (i++ < sizeof(achBuffer))
                    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "   ");
                pCmdHlp->pfnPrintf(pCmdHlp, NULL, "  ");
                for (i = 0; i < cb; i += cbElement)
                {
                    uint8_t u8 = *(uint8_t *)&achBuffer[i];
                    if (isprint(u8) && u8 < 127)
                        pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%c", u8);
                    else
                        pCmdHlp->pfnPrintf(pCmdHlp, NULL, ".");
                }
            }
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
        }
        else
        {
            /*
             * We print up to the first zero and stop there.
             * Only printables + '\t' and '\n' are printed.
             */
            if (!u8Prev)
                pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV:\n", &pDbgc->DumpPos);
            uint8_t u8 = '\0';
            unsigned i;
            for (i = 0; i < cb; i++)
            {
                u8Prev = u8;
                u8 = *(uint8_t *)&achBuffer[i];
                if (    u8 < 127
                    && (    isprint(u8)
                        ||  u8 == '\t'
                        ||  u8 == '\n'))
                    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%c", u8);
                else if (!u8)
                    break;
                else
                    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\\x%x", u8);
            }
            if (u8 == '\0')
                cb = cbLeft = i + 1;
            if (cbLeft - cb <= 0 && u8Prev != '\n')
                pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
        }

        /*
         * Advance
         */
        cbLeft -= (int)cb;
        rc = pCmdHlp->pfnEval(pCmdHlp, &pDbgc->DumpPos, "(%Dv) + %x", &pDbgc->DumpPos, cb);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Expression: (%Dv) + %x\n", &pDbgc->DumpPos, cb);
        if (cbLeft <= 0)
            break;
    }

    NOREF(pCmd); NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * Best guess at which paging mode currently applies to the guest
 * paging structures.
 *
 * This have to come up with a decent answer even when the guest
 * is in non-paged protected mode or real mode.
 *
 * @returns cr3.
 * @param   pDbgc   The DBGC instance.
 * @param   pfPAE   Where to store the page address extension indicator.
 * @param   pfLME   Where to store the long mode enabled indicator.
 * @param   pfPSE   Where to store the page size extension indicator.
 * @param   pfPGE   Where to store the page global enabled indicator.
 * @param   pfNXE   Where to store the no-execution enabled inidicator.
 */
static RTGCPHYS dbgcGetGuestPageMode(PDBGC pDbgc, bool *pfPAE, bool *pfLME, bool *pfPSE, bool *pfPGE, bool *pfNXE)
{
    RTGCUINTREG cr4 = CPUMGetGuestCR4(pDbgc->pVM);
    *pfPSE = !!(cr4 & X86_CR4_PSE);
    *pfPGE = !!(cr4 & X86_CR4_PGE);
    *pfPAE = !!(cr4 & X86_CR4_PAE);
    *pfLME = CPUMGetGuestMode(pDbgc->pVM) == CPUMMODE_LONG;
    *pfNXE = false; /* GUEST64 GUESTNX */
    return CPUMGetGuestCR3(pDbgc->pVM);
}


/**
 * Determin the shadow paging mode.
 *
 * @returns cr3.
 * @param   pDbgc   The DBGC instance.
 * @param   pfPAE   Where to store the page address extension indicator.
 * @param   pfLME   Where to store the long mode enabled indicator.
 * @param   pfPSE   Where to store the page size extension indicator.
 * @param   pfPGE   Where to store the page global enabled indicator.
 * @param   pfNXE   Where to store the no-execution enabled inidicator.
 */
static RTHCPHYS dbgcGetShadowPageMode(PDBGC pDbgc, bool *pfPAE, bool *pfLME, bool *pfPSE, bool *pfPGE, bool *pfNXE)
{
    *pfPSE = true;
    *pfPGE = false;
    switch (PGMGetShadowMode(pDbgc->pVM))
    {
        default:
        case PGMMODE_32_BIT:
            *pfPAE = *pfLME = *pfNXE = false;
            break;
        case PGMMODE_PAE:
            *pfLME = *pfNXE = false;
            *pfPAE = true;
            break;
        case PGMMODE_PAE_NX:
            *pfLME = false;
            *pfPAE = *pfNXE = true;
            break;
        case PGMMODE_AMD64:
            *pfNXE = false;
            *pfPAE = *pfLME = true;
            break;
        case PGMMODE_AMD64_NX:
            *pfPAE = *pfLME = *pfNXE = true;
            break;
    }
    return PGMGetHyperCR3(pDbgc->pVM);
}


/**
 * The 'dpd', 'dpda', 'dpdb', 'dpdg' and 'dpdh' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpPageDir(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Validate input.
     */
    if (    cArgs > 1
        ||  (cArgs == 1 && pCmd->pszCmd[3] == 'a' && !DBGCVAR_ISPOINTER(paArgs[0].enmType))
        ||  (cArgs == 1 && pCmd->pszCmd[3] != 'a' && !(paArgs[0].enmType == DBGCVAR_TYPE_NUMBER || DBGCVAR_ISPOINTER(paArgs[0].enmType)))
        )
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: The parser doesn't do its job properly yet.. It might help to use the '%%' operator.\n");
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");

    /*
     * Guest or shadow page directories? Get the paging parameters.
     */
    bool fGuest = pCmd->pszCmd[3] != 'h';
    if (!pCmd->pszCmd[3] || pCmd->pszCmd[3] == 'a')
        fGuest = paArgs[0].enmType == DBGCVAR_TYPE_NUMBER
               ? pDbgc->fRegCtxGuest
               : DBGCVAR_ISGCPOINTER(paArgs[0].enmType);

    bool fPAE, fLME, fPSE, fPGE, fNXE;
    uint64_t cr3 = fGuest
                 ? dbgcGetGuestPageMode(pDbgc, &fPAE, &fLME, &fPSE, &fPGE, &fNXE)
                 : dbgcGetShadowPageMode(pDbgc, &fPAE, &fLME, &fPSE, &fPGE, &fNXE);
    const unsigned cbEntry = fPAE ? sizeof(X86PTEPAE) : sizeof(X86PTE);

    /*
     * Setup default arugment if none was specified.
     * Fix address / index confusion.
     */
    DBGCVAR VarDefault;
    if (!cArgs)
    {
        if (pCmd->pszCmd[3] == 'a')
        {
            if (fLME || fPAE)
                return DBGCCmdHlpPrintf(pCmdHlp, "Default argument for 'dpda' hasn't been fully implemented yet. Try with an address or use one of the other commands.\n");
            if (fGuest)
                DBGCVAR_INIT_GC_PHYS(&VarDefault, cr3);
            else
                DBGCVAR_INIT_HC_PHYS(&VarDefault, cr3);
        }
        else
            DBGCVAR_INIT_GC_FLAT(&VarDefault, 0);
        paArgs = &VarDefault;
        cArgs = 1;
    }
    else if (paArgs[0].enmType == DBGCVAR_TYPE_NUMBER)
    {
        Assert(pCmd->pszCmd[3] != 'a');
        VarDefault = paArgs[0];
        if (VarDefault.u.u64Number <= 1024)
        {
            if (fPAE)
                return DBGCCmdHlpPrintf(pCmdHlp, "PDE indexing is only implemented for 32-bit paging.\n");
            if (VarDefault.u.u64Number >= PAGE_SIZE / cbEntry)
                return DBGCCmdHlpPrintf(pCmdHlp, "PDE index is out of range [0..%d].\n", PAGE_SIZE / cbEntry - 1);
            VarDefault.u.u64Number <<= X86_PD_SHIFT;
        }
        VarDefault.enmType = DBGCVAR_TYPE_GC_FLAT;
        paArgs = &VarDefault;
    }

    /*
     * Locate the PDE to start displaying at.
     *
     * The 'dpda' command takes the address of a PDE, while the others are guest
     * virtual address which PDEs should be displayed. So, 'dpda' is rather simple
     * while the others require us to do all the tedious walking thru the paging
     * hierarchy to find the intended PDE.
     */
    unsigned    iEntry = ~0U;           /* The page directory index. ~0U for 'dpta'. */
    DBGCVAR     VarGCPtr;               /* The GC address corresponding to the current PDE (iEntry != ~0U). */
    DBGCVAR     VarPDEAddr;             /* The address of the current PDE. */
    unsigned    cEntries;               /* The number of entries to display. */
    unsigned    cEntriesMax;            /* The max number of entries to display. */
    int         rc;
    if (pCmd->pszCmd[3] == 'a')
    {
        VarPDEAddr = paArgs[0];
        switch (VarPDEAddr.enmRangeType)
        {
            case DBGCVAR_RANGE_BYTES:       cEntries = VarPDEAddr.u64Range / cbEntry; break;
            case DBGCVAR_RANGE_ELEMENTS:    cEntries = VarPDEAddr.u64Range; break;
            default:                        cEntries = 10; break;
        }
        cEntriesMax = PAGE_SIZE / cbEntry;
    }
    else
    {
        /*
         * Determin the range.
         */
        switch (paArgs[0].enmRangeType)
        {
            case DBGCVAR_RANGE_BYTES:       cEntries = paArgs[0].u64Range / PAGE_SIZE; break;
            case DBGCVAR_RANGE_ELEMENTS:    cEntries = paArgs[0].u64Range; break;
            default:                        cEntries = 10; break;
        }

        /*
         * Normalize the input address, it must be a flat GC address.
         */
        rc = pCmdHlp->pfnEval(pCmdHlp, &VarGCPtr, "%%(%Dv)", &paArgs[0]);
        if (VBOX_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "%%(%Dv)", &paArgs[0]);
        if (VarGCPtr.enmType == DBGCVAR_TYPE_HC_FLAT)
        {
            VarGCPtr.u.GCFlat = (uintptr_t)VarGCPtr.u.pvHCFlat;
            VarGCPtr.enmType = DBGCVAR_TYPE_GC_FLAT;
        }
        if (fPAE)
            VarGCPtr.u.GCFlat &= ~(((RTGCPTR)1 << X86_PD_PAE_SHIFT) - 1);
        else
            VarGCPtr.u.GCFlat &= ~(((RTGCPTR)1 << X86_PD_SHIFT) - 1);

        /*
         * Do the paging walk until we get to the page directory.
         */
        DBGCVAR VarCur;
        if (fGuest)
            DBGCVAR_INIT_GC_PHYS(&VarCur, cr3);
        else
            DBGCVAR_INIT_HC_PHYS(&VarCur, cr3);
        if (fLME)
        {
            /* Page Map Level 4 Lookup. */
            /* Check if it's a valid address first? */
            VarCur.u.u64Number &= X86_PTE_PAE_PG_MASK;
            VarCur.u.u64Number += (((uint64_t)VarGCPtr.u.GCFlat >> X86_PML4_SHIFT) & X86_PML4_MASK) * sizeof(X86PML4E);
            X86PML4E Pml4e;
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pml4e, sizeof(Pml4e), &VarCur, NULL);
            if (VBOX_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PML4E memory at %DV.\n", &VarCur);
            if (!Pml4e.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page directory pointer table is not present for %Dv.\n", &VarGCPtr);

            VarCur.u.u64Number = Pml4e.u & X86_PML4E_PG_MASK;
            Assert(fPAE);
        }
        if (fPAE)
        {
            /* Page directory pointer table. */
            X86PDPE Pdpe;
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PDPTR_SHIFT) & X86_PDPTR_MASK) * sizeof(Pdpe);
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pdpe, sizeof(Pdpe), &VarCur, NULL);
            if (VBOX_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PDPE memory at %DV.\n", &VarCur);
            if (!Pdpe.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page directory is not present for %Dv.\n", &VarGCPtr);

            iEntry = (VarGCPtr.u.GCFlat >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            VarPDEAddr = VarCur;
            VarPDEAddr.u.u64Number = Pdpe.u & X86_PDPE_PG_MASK;
            VarPDEAddr.u.u64Number += iEntry * sizeof(X86PDEPAE);
        }
        else
        {
            /* 32-bit legacy - CR3 == page directory. */
            iEntry = (VarGCPtr.u.GCFlat >> X86_PD_SHIFT) & X86_PD_MASK;
            VarPDEAddr = VarCur;
            VarPDEAddr.u.u64Number += iEntry * sizeof(X86PDE);
        }
        cEntriesMax = (PAGE_SIZE - iEntry) / cbEntry;
        iEntry /= cbEntry;
    }

    /* adjust cEntries */
    cEntries = RT_MAX(1, cEntries);
    cEntries = RT_MIN(cEntries, cEntriesMax);

    /*
     * The display loop.
     */
    DBGCCmdHlpPrintf(pCmdHlp, iEntry != ~0U ? "%DV (index %#x):\n" : "%DV:\n",
                     &VarPDEAddr, iEntry);
    do
    {
        /*
         * Read.
         */
        X86PDEPAE Pde;
        Pde.u = 0;
        rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pde, cbEntry, &VarPDEAddr, NULL);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Reading PDE memory at %DV.\n", &VarPDEAddr);

        /*
         * Display.
         */
        if (iEntry != ~0U)
        {
            DBGCCmdHlpPrintf(pCmdHlp, "%03x %DV: ", iEntry, &VarGCPtr);
            iEntry++;
        }
        if (fPSE && Pde.b.u1Size)
            DBGCCmdHlpPrintf(pCmdHlp,
                             fPAE
                             ? "%016llx big phys=%016llx %s %s %s %s %s avl=%02x %s %s %s %s %s"
                             :   "%08llx big phys=%08llx %s %s %s %s %s avl=%02x %s %s %s %s %s",
                             Pde.u,
                             Pde.u & X86_PDE_PAE_PG_MASK,
                             Pde.b.u1Present        ? "p "  : "np",
                             Pde.b.u1Write          ? "w"   : "r",
                             Pde.b.u1User           ? "u"   : "s",
                             Pde.b.u1Accessed       ? "a "  : "na",
                             Pde.b.u1Dirty          ? "d "  : "nd",
                             Pde.b.u3Available,
                             Pde.b.u1Global         ? (fPGE ? "g" : "G") : " ",
                             Pde.b.u1WriteThru      ? "pwt" : "   ",
                             Pde.b.u1CacheDisable   ? "pcd" : "   ",
                             Pde.b.u1PAT            ? "pat" : "",
                             Pde.b.u1NoExecute      ? (fNXE ? "nx" : "NX") : "  ");
        else
            DBGCCmdHlpPrintf(pCmdHlp,
                             fPAE
                             ? "%016llx 4kb phys=%016llx %s %s %s %s %s avl=%02x %s %s %s %s"
                             :   "%08llx 4kb phys=%08llx %s %s %s %s %s avl=%02x %s %s %s %s",
                             Pde.u,
                             Pde.u & X86_PDE_PAE_PG_MASK,
                             Pde.n.u1Present        ? "p "  : "np",
                             Pde.n.u1Write          ? "w"   : "r",
                             Pde.n.u1User           ? "u"   : "s",
                             Pde.n.u1Accessed       ? "a "  : "na",
                             Pde.u & RT_BIT(6)      ? "6 "  : "  ",
                             Pde.n.u3Available,
                             Pde.u & RT_BIT(8)      ? "8"   : " ",
                             Pde.n.u1WriteThru      ? "pwt" : "   ",
                             Pde.n.u1CacheDisable   ? "pcd" : "   ",
                             Pde.u & RT_BIT(7)      ? "7"   : "",
                             Pde.n.u1NoExecute      ? (fNXE ? "nx" : "NX") : "  ");
        if (Pde.u & UINT64_C(0x7fff000000000000))
            DBGCCmdHlpPrintf(pCmdHlp, " weird=%RX64", (Pde.u & UINT64_C(0x7fff000000000000)));
        rc = DBGCCmdHlpPrintf(pCmdHlp, "\n");
        if (VBOX_FAILURE(rc))
            return rc;

        /*
         * Advance.
         */
        VarPDEAddr.u.u64Number += cbEntry;
        if (iEntry != ~0U)
            VarGCPtr.u.GCFlat += 1 << (fPAE ? X86_PD_PAE_SHIFT : X86_PD_SHIFT);
    } while (cEntries-- > 0);

    NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * The 'dpdb' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpPageDirBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");
    int rc1 = pCmdHlp->pfnExec(pCmdHlp, "dpdg %DV", &paArgs[0]);
    int rc2 = pCmdHlp->pfnExec(pCmdHlp, "dpdh %DV", &paArgs[0]);
    if (VBOX_FAILURE(rc1))
        return rc1;
    NOREF(pCmd); NOREF(paArgs); NOREF(cArgs); NOREF(pResult);
    return rc2;
}


/**
 * The 'dpg*' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpPageTable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Validate input.
     */
    if (    cArgs != 1
        ||  (pCmd->pszCmd[3] == 'a' && !DBGCVAR_ISPOINTER(paArgs[0].enmType))
        ||  (pCmd->pszCmd[3] != 'a' && !(paArgs[0].enmType == DBGCVAR_TYPE_NUMBER || DBGCVAR_ISPOINTER(paArgs[0].enmType)))
        )
        return DBGCCmdHlpPrintf(pCmdHlp, "internal error: The parser doesn't do its job properly yet.. It might help to use the '%%' operator.\n");
    if (!pVM)
        return DBGCCmdHlpPrintf(pCmdHlp, "error: No VM.\n");

    /*
     * Guest or shadow page tables? Get the paging parameters.
     */
    bool fGuest = pCmd->pszCmd[3] != 'h';
    if (!pCmd->pszCmd[3] || pCmd->pszCmd[3] == 'a')
        fGuest = paArgs[0].enmType == DBGCVAR_TYPE_NUMBER
               ? pDbgc->fRegCtxGuest
               : DBGCVAR_ISGCPOINTER(paArgs[0].enmType);

    bool fPAE, fLME, fPSE, fPGE, fNXE;
    uint64_t cr3 = fGuest
                 ? dbgcGetGuestPageMode(pDbgc, &fPAE, &fLME, &fPSE, &fPGE, &fNXE)
                 : dbgcGetShadowPageMode(pDbgc, &fPAE, &fLME, &fPSE, &fPGE, &fNXE);
    const unsigned cbEntry = fPAE ? sizeof(X86PTEPAE) : sizeof(X86PTE);

    /*
     * Locate the PTE to start displaying at.
     *
     * The 'dpta' command takes the address of a PTE, while the others are guest
     * virtual address which PTEs should be displayed. So, 'pdta' is rather simple
     * while the others require us to do all the tedious walking thru the paging
     * hierarchy to find the intended PTE.
     */
    unsigned    iEntry = ~0U;           /* The page table index. ~0U for 'dpta'. */
    DBGCVAR     VarGCPtr;               /* The GC address corresponding to the current PTE (iEntry != ~0U). */
    DBGCVAR     VarPTEAddr;             /* The address of the current PTE. */
    unsigned    cEntries;               /* The number of entries to display. */
    unsigned    cEntriesMax;            /* The max number of entries to display. */
    int         rc;
    if (pCmd->pszCmd[3] == 'a')
    {
        VarPTEAddr = paArgs[0];
        switch (VarPTEAddr.enmRangeType)
        {
            case DBGCVAR_RANGE_BYTES:       cEntries = VarPTEAddr.u64Range / cbEntry; break;
            case DBGCVAR_RANGE_ELEMENTS:    cEntries = VarPTEAddr.u64Range; break;
            default:                        cEntries = 10; break;
        }
        cEntriesMax = PAGE_SIZE / cbEntry;
    }
    else
    {
        /*
         * Determin the range.
         */
        switch (paArgs[0].enmRangeType)
        {
            case DBGCVAR_RANGE_BYTES:       cEntries = paArgs[0].u64Range / PAGE_SIZE; break;
            case DBGCVAR_RANGE_ELEMENTS:    cEntries = paArgs[0].u64Range; break;
            default:                        cEntries = 10; break;
        }

        /*
         * Normalize the input address, it must be a flat GC address.
         */
        rc = pCmdHlp->pfnEval(pCmdHlp, &VarGCPtr, "%%(%Dv)", &paArgs[0]);
        if (VBOX_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "%%(%Dv)", &paArgs[0]);
        if (VarGCPtr.enmType == DBGCVAR_TYPE_HC_FLAT)
        {
            VarGCPtr.u.GCFlat = (uintptr_t)VarGCPtr.u.pvHCFlat;
            VarGCPtr.enmType = DBGCVAR_TYPE_GC_FLAT;
        }
        VarGCPtr.u.GCFlat &= ~(RTGCPTR)PAGE_OFFSET_MASK;

        /*
         * Do the paging walk until we get to the page table.
         */
        DBGCVAR VarCur;
        if (fGuest)
            DBGCVAR_INIT_GC_PHYS(&VarCur, cr3);
        else
            DBGCVAR_INIT_HC_PHYS(&VarCur, cr3);
        if (fLME)
        {
            /* Page Map Level 4 Lookup. */
            /* Check if it's a valid address first? */
            VarCur.u.u64Number &= X86_PTE_PAE_PG_MASK;
            VarCur.u.u64Number += (((uint64_t)VarGCPtr.u.GCFlat >> X86_PML4_SHIFT) & X86_PML4_MASK) * sizeof(X86PML4E);
            X86PML4E Pml4e;
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pml4e, sizeof(Pml4e), &VarCur, NULL);
            if (VBOX_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PML4E memory at %DV.\n", &VarCur);
            if (!Pml4e.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page directory pointer table is not present for %Dv.\n", &VarGCPtr);

            VarCur.u.u64Number = Pml4e.u & X86_PML4E_PG_MASK;
            Assert(fPAE);
        }
        if (fPAE)
        {
            /* Page directory pointer table. */
            X86PDPE Pdpe;
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PDPTR_SHIFT) & X86_PDPTR_MASK) * sizeof(Pdpe);
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pdpe, sizeof(Pdpe), &VarCur, NULL);
            if (VBOX_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PDPE memory at %DV.\n", &VarCur);
            if (!Pdpe.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page directory is not present for %Dv.\n", &VarGCPtr);

            VarCur.u.u64Number = Pdpe.u & X86_PDPE_PG_MASK;

            /* Page directory (PAE). */
            X86PDEPAE Pde;
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK) * sizeof(Pde);
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pde, sizeof(Pde), &VarCur, NULL);
            if (VBOX_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PDE memory at %DV.\n", &VarCur);
            if (!Pde.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page table is not present for %Dv.\n", &VarGCPtr);
            if (fPSE && Pde.n.u1Size)
                return pCmdHlp->pfnExec(pCmdHlp, "dpd%s %Dv L3", &pCmd->pszCmd[3], &VarGCPtr);

            iEntry = (VarGCPtr.u.GCFlat >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK;
            VarPTEAddr = VarCur;
            VarPTEAddr.u.u64Number = Pde.u & X86_PDE_PAE_PG_MASK;
            VarPTEAddr.u.u64Number += iEntry * sizeof(X86PTEPAE);
        }
        else
        {
            /* Page directory (legacy). */
            X86PDE Pde;
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PD_SHIFT) & X86_PD_MASK) * sizeof(Pde);
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pde, sizeof(Pde), &VarCur, NULL);
            if (VBOX_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PDE memory at %DV.\n", &VarCur);
            if (!Pde.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page table is not present for %Dv.\n", &VarGCPtr);
            if (fPSE && Pde.n.u1Size)
                return pCmdHlp->pfnExec(pCmdHlp, "dpd%s %Dv L3", &pCmd->pszCmd[3], &VarGCPtr);

            iEntry = (VarGCPtr.u.GCFlat >> X86_PT_SHIFT) & X86_PT_MASK;
            VarPTEAddr = VarCur;
            VarPTEAddr.u.u64Number = Pde.u & X86_PDE_PG_MASK;
            VarPTEAddr.u.u64Number += iEntry * sizeof(X86PTE);
        }
        cEntriesMax = (PAGE_SIZE - iEntry) / cbEntry;
        iEntry /= cbEntry;
    }

    /* adjust cEntries */
    cEntries = RT_MAX(1, cEntries);
    cEntries = RT_MIN(cEntries, cEntriesMax);

    /*
     * The display loop.
     */
    DBGCCmdHlpPrintf(pCmdHlp, iEntry != ~0U ? "%DV (base %DV / index %#x):\n" : "%DV:\n",
                     &VarPTEAddr, &VarGCPtr, iEntry);
    do
    {
        /*
         * Read.
         */
        X86PTEPAE Pte;
        Pte.u = 0;
        rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pte, cbEntry, &VarPTEAddr, NULL);
        if (VBOX_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PTE memory at %DV.\n", &VarPTEAddr);

        /*
         * Display.
         */
        if (iEntry != ~0U)
        {
            DBGCCmdHlpPrintf(pCmdHlp, "%03x %DV: ", iEntry, &VarGCPtr);
            iEntry++;
        }
        DBGCCmdHlpPrintf(pCmdHlp,
                         fPAE
                         ? "%016llx 4kb phys=%016llx %s %s %s %s %s avl=%02x %s %s %s %s %s"
                         :   "%08llx 4kb phys=%08llx %s %s %s %s %s avl=%02x %s %s %s %s %s",
                         Pte.u,
                         Pte.u & X86_PTE_PAE_PG_MASK,
                         Pte.n.u1Present         ? "p " : "np",
                         Pte.n.u1Write           ? "w" : "r",
                         Pte.n.u1User            ? "u" : "s",
                         Pte.n.u1Accessed        ? "a " : "na",
                         Pte.n.u1Dirty           ? "d " : "nd",
                         Pte.n.u3Available,
                         Pte.n.u1Global          ? (fPGE ? "g" : "G") : " ",
                         Pte.n.u1WriteThru       ? "pwt" : "   ",
                         Pte.n.u1CacheDisable    ? "pcd" : "   ",
                         Pte.n.u1PAT             ? "pat" : "   ",
                         Pte.n.u1NoExecute       ? (fNXE ? "nx" : "NX") : "  "
                         );
        if (Pte.u & UINT64_C(0x7fff000000000000))
            DBGCCmdHlpPrintf(pCmdHlp, " weird=%RX64", (Pte.u & UINT64_C(0x7fff000000000000)));
        rc = DBGCCmdHlpPrintf(pCmdHlp, "\n");
        if (VBOX_FAILURE(rc))
            return rc;

        /*
         * Advance.
         */
        VarPTEAddr.u.u64Number += cbEntry;
        if (iEntry != ~0U)
            VarGCPtr.u.GCFlat += PAGE_SIZE;
    } while (cEntries-- > 0);

    NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * The 'dptb' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpPageTableBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");
    int rc1 = pCmdHlp->pfnExec(pCmdHlp, "dptg %DV", &paArgs[0]);
    int rc2 = pCmdHlp->pfnExec(pCmdHlp, "dpth %DV", &paArgs[0]);
    if (VBOX_FAILURE(rc1))
        return rc1;
    NOREF(pCmd); NOREF(cArgs); NOREF(pResult);
    return rc2;
}


/**
 * The 'dt' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpTSS(PCDBGCCMD /*pCmd*/, PDBGCCMDHLP pCmdHlp, PVM /*pVM*/, PCDBGCVAR /*paArgs*/, unsigned /*cArgs*/, PDBGCVAR /*pResult*/)
{
    /*
     * We can get a TSS selector (number), a far pointer using a TSS selector, or some kind of TSS pointer.
     */

    /** @todo */
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "dt is not implemented yet, feel free to do it. \n");
}


/**
 * The 'm' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdMemoryInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Address: %DV\n", &paArgs[0]);
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");
    int rc1 = pCmdHlp->pfnExec(pCmdHlp, "dpdg %DV", &paArgs[0]);
    int rc2 = pCmdHlp->pfnExec(pCmdHlp, "dpdh %DV", &paArgs[0]);
    int rc3 = pCmdHlp->pfnExec(pCmdHlp, "dptg %DV", &paArgs[0]);
    int rc4 = pCmdHlp->pfnExec(pCmdHlp, "dpth %DV", &paArgs[0]);
    if (VBOX_FAILURE(rc1))
        return rc1;
    if (VBOX_FAILURE(rc2))
        return rc2;
    if (VBOX_FAILURE(rc3))
        return rc3;
    NOREF(pCmd); NOREF(cArgs); NOREF(pResult);
    return rc4;
}


/**
 * The 'echo' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdEcho(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Loop thru the arguments and print them with one space between.
     */
    int rc = 0;
    for (unsigned i = 0; i < cArgs; i++)
    {
        if (paArgs[i].enmType == DBGCVAR_TYPE_STRING)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, i ? " %s" : "%s", paArgs[i].u.pszString);
        else
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, i ? " <parser error>" : "<parser error>");
        if (VBOX_FAILURE(rc))
            return rc;
    }
    NOREF(pCmd); NOREF(pResult); NOREF(pVM);
    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
}


/**
 * The 'runscript' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdRunScript(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /* check that the parser did what it's supposed to do. */
    if (    cArgs != 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");

    /*
     * Try open the script.
     */
    const char *pszFilename = paArgs[0].u.pszString;
    FILE *pFile = fopen(pszFilename, "r");
    if (!pFile)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Failed to open '%s'.\n", pszFilename);

    /*
     * Execute it line by line.
     */
    int rc = 0;
    unsigned iLine = 0;
    char szLine[8192];
    while (fgets(szLine, sizeof(szLine), pFile))
    {
        /* check that the line isn't too long. */
        char *pszEnd = strchr(szLine, '\0');
        if (pszEnd == &szLine[sizeof(szLine) - 1])
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "runscript error: Line #%u is too long\n", iLine);
            break;
        }
        iLine++;

        /* strip leading blanks and check for comment / blank line. */
        char *psz = RTStrStripL(szLine);
        if (    *psz == '\0'
            ||  *psz == '\n'
            ||  *psz == '#')
            continue;

        /* strip trailing blanks and check for empty line (\r case). */
        while (     pszEnd > psz
               &&   isspace(pszEnd[-1])) /* isspace includes \n and \r normally. */
            *--pszEnd = '\0';

        /** @todo check for Control-C / Cancel at this point... */

        /*
         * Execute the command.
         *
         * This is a bit wasteful with scratch space btw., can fix it later.
         * The whole return code crap should be fixed too, so that it's possible
         * to know whether a command succeeded (VBOX_SUCCESS()) or failed, and
         * more importantly why it failed.
         */
        rc = pCmdHlp->pfnExec(pCmdHlp, "%s", psz);
        if (VBOX_FAILURE(rc))
        {
            if (rc == VERR_BUFFER_OVERFLOW)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "runscript error: Line #%u is too long (exec overflowed)\n", iLine);
            break;
        }
        if (rc == VWRN_DBGC_CMD_PENDING)
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "runscript error: VWRN_DBGC_CMD_PENDING on line #%u, script terminated\n", iLine);
            break;
        }
    }

    fclose(pFile);

    NOREF(pCmd); NOREF(pResult); NOREF(pVM);
    return rc;
}


/**
 * The 's' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdSearchMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /* check that the parser did what it's supposed to do. */
    if (    cArgs != 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");
    return -1;
}



/**
 * Print formatted string.
 *
 * @param   pHlp        Pointer to this structure.
 * @param   pszFormat   The format string.
 * @param   ...         Arguments.
 */
static DECLCALLBACK(void) dbgcCmdInfo_Printf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...)
{
    PDBGCCMDHLP pCmdHlp = *(PDBGCCMDHLP *)(pHlp + 1);
    va_list args;
    va_start(args,  pszFormat);
    pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszFormat, args);
    va_end(args);
}


/**
 * Print formatted string.
 *
 * @param   pHlp        Pointer to this structure.
 * @param   pszFormat   The format string.
 * @param   args        Argument list.
 */
static DECLCALLBACK(void) dbgcCmdInfo_PrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args)
{
    PDBGCCMDHLP pCmdHlp = *(PDBGCCMDHLP *)(pHlp + 1);
    pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszFormat, args);
}


/**
 * The 'info' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (    cArgs < 1
        ||  cArgs > 2
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING
        ||  paArgs[cArgs - 1].enmType != DBGCVAR_TYPE_STRING)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "internal error: The parser doesn't do its job properly yet.. quote the string.\n");
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");

    /*
     * Dump it.
     */
    struct
    {
        DBGFINFOHLP Hlp;
        PDBGCCMDHLP pCmdHlp;
    } Hlp = { { dbgcCmdInfo_Printf, dbgcCmdInfo_PrintfV }, pCmdHlp };
    int rc = DBGFR3Info(pVM, paArgs[0].u.pszString, cArgs == 2 ? paArgs[1].u.pszString : NULL, &Hlp.Hlp);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3Info()\n");

    NOREF(pCmd); NOREF(pResult);
    return 0;
}


/**
 * The 'log' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLog(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    int rc = DBGFR3LogModifyGroups(pVM, paArgs[0].u.pszString);
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    NOREF(pCmd); NOREF(cArgs); NOREF(pResult);
    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3LogModifyGroups(%p,'%s')\n", pVM, paArgs[0].u.pszString);
}


/**
 * The 'logdest' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLogDest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    int rc = DBGFR3LogModifyDestinations(pVM, paArgs[0].u.pszString);
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    NOREF(pCmd); NOREF(cArgs); NOREF(pResult);
    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3LogModifyDestinations(%p,'%s')\n", pVM, paArgs[0].u.pszString);
}


/**
 * The 'logflags' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLogFlags(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    int rc = DBGFR3LogModifyFlags(pVM, paArgs[0].u.pszString);
    if (VBOX_SUCCESS(rc))
        return VINF_SUCCESS;
    NOREF(pCmd); NOREF(cArgs); NOREF(pResult);
    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3LogModifyFlags(%p,'%s')\n", pVM, paArgs[0].u.pszString);
}


/**
 * The 'format' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdFormat(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    LogFlow(("dbgcCmdFormat\n"));
    static const char *apszRangeDesc[] =
    {
        "none", "bytes", "elements"
    };
    int rc;

    for (unsigned iArg = 0; iArg < cArgs; iArg++)
    {
        switch (paArgs[iArg].enmType)
        {
            case DBGCVAR_TYPE_UNKNOWN:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "Unknown variable type!\n");
                break;
            case DBGCVAR_TYPE_GC_FLAT:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Guest flat address: %%%08x range %lld %s\n",
                        paArgs[iArg].u.GCFlat,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Guest flat address: %%%08x\n",
                        paArgs[iArg].u.GCFlat);
                break;
            case DBGCVAR_TYPE_GC_FAR:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Guest far address: %04x:%08x range %lld %s\n",
                        paArgs[iArg].u.GCFar.sel,
                        paArgs[iArg].u.GCFar.off,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Guest far address: %04x:%08x\n",
                        paArgs[iArg].u.GCFar.sel,
                        paArgs[iArg].u.GCFar.off);
                break;
            case DBGCVAR_TYPE_GC_PHYS:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Guest physical address: %%%%%08x range %lld %s\n",
                        paArgs[iArg].u.GCPhys,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Guest physical address: %%%%%08x\n",
                        paArgs[iArg].u.GCPhys);
                break;
            case DBGCVAR_TYPE_HC_FLAT:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host flat address: %%%08x range %lld %s\n",
                        paArgs[iArg].u.pvHCFlat,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host flat address: %%%08x\n",
                        paArgs[iArg].u.pvHCFlat);
                break;
            case DBGCVAR_TYPE_HC_FAR:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host far address: %04x:%08x range %lld %s\n",
                        paArgs[iArg].u.HCFar.sel,
                        paArgs[iArg].u.HCFar.off,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host far address: %04x:%08x\n",
                        paArgs[iArg].u.HCFar.sel,
                        paArgs[iArg].u.HCFar.off);
                break;
            case DBGCVAR_TYPE_HC_PHYS:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host physical address: %VHp range %lld %s\n",
                        paArgs[iArg].u.HCPhys,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host physical address: %VHp\n",
                        paArgs[iArg].u.HCPhys);
                break;

            case DBGCVAR_TYPE_STRING:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "String, %lld bytes long: %s\n",
                    paArgs[iArg].u64Range,
                    paArgs[iArg].u.pszString);
                break;

            case DBGCVAR_TYPE_NUMBER:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Number: hex %llx  dec 0i%lld  oct 0t%llo  range %lld %s\n",
                        paArgs[iArg].u.u64Number,
                        paArgs[iArg].u.u64Number,
                        paArgs[iArg].u.u64Number,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Number: hex %llx  dec 0i%lld  oct 0t%llo\n",
                        paArgs[iArg].u.u64Number,
                        paArgs[iArg].u.u64Number,
                        paArgs[iArg].u.u64Number);
                break;

            default:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "Invalid argument type %d\n",
                    paArgs[iArg].enmType);
                break;
        }
    } /* arg loop */

    NOREF(pCmd); NOREF(pVM); NOREF(pResult);
    return 0;
}


/**
 * List near symbol.
 *
 * @returns VBox status code.
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   pArg        Pointer to the address or symbol to lookup.
 */
static int dbgcDoListNear(PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR pArg, PDBGCVAR pResult)
{
    dbgcVarSetGCFlat(pResult, 0);

    DBGFSYMBOL  Symbol;
    int         rc;
    if (pArg->enmType == DBGCVAR_TYPE_SYMBOL)
    {
        /*
         * Lookup the symbol address.
         */
        rc = DBGFR3SymbolByName(pVM, pArg->u.pszString, &Symbol);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3SymbolByName(, %s,)\n", pArg->u.pszString);

        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%VGv %s\n", (RTGCUINTPTR)Symbol.Value, Symbol.szName); /** @todo remove the RTUINGCPTR cast once DBGF got correct interfaces! */
        dbgcVarSetGCFlatByteRange(pResult, Symbol.Value, Symbol.cb);
    }
    else
    {
        /*
         * Convert it to a flat GC address and lookup that address.
         */
        DBGCVAR AddrVar;
        rc = pCmdHlp->pfnEval(pCmdHlp, &AddrVar, "%%(%DV)", pArg);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "%%(%DV)\n", pArg);

        dbgcVarSetVar(pResult, &AddrVar);

        RTGCINTPTR offDisp = 0;
        rc = DBGFR3SymbolByAddr(pVM, AddrVar.u.GCFlat, &offDisp, &Symbol);
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3SymbolByAddr(, %VGv,,)\n", AddrVar.u.GCFlat);

        if (!offDisp)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV %s", &AddrVar, Symbol.szName);
        else if (offDisp > 0)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV %s + %RGv", &AddrVar, Symbol.szName, offDisp);
        else
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV %s - %RGv", &AddrVar, Symbol.szName, -offDisp);
        if ((RTGCINTPTR)Symbol.cb > -offDisp)
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " LB %RGv\n", Symbol.cb + offDisp);
            dbgcVarSetByteRange(pResult, Symbol.cb + offDisp);
        }
        else
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
            dbgcVarSetNoRange(pResult);
        }
    }

    return rc;
}


/**
 * The 'ln' (listnear) command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdListNear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    dbgcVarSetGCFlat(pResult, 0);
    if (!cArgs)
    {
        /*
         * Current cs:eip symbol.
         */
        DBGCVAR AddrVar;
        int rc = pCmdHlp->pfnEval(pCmdHlp, &AddrVar, "%%(cs:eip)");
        if (VBOX_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "%%(cs:eip)\n");
        return dbgcDoListNear(pCmdHlp, pVM, &AddrVar, pResult);
    }

    /*
     * Iterate arguments.
     */
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
    {
        int rc = dbgcDoListNear(pCmdHlp, pVM, &paArgs[iArg], pResult);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    NOREF(pCmd); NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * The 'loadsyms' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLoadSyms(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate the parsing and make sense of the input.
     * This is a mess as usual because we don't trust the parser yet.
     */
    if (    cArgs < 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    {
        AssertMsgFailed(("Parse error, first argument required to be string!\n"));
        return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    DBGCVAR     AddrVar;
    RTGCUINTPTR Delta = 0;
    const char *pszModule = NULL;
    RTGCUINTPTR ModuleAddress = 0;
    unsigned    cbModule = 0;
    if (cArgs > 1)
    {
        unsigned iArg = 1;
        if (paArgs[iArg].enmType == DBGCVAR_TYPE_NUMBER)
        {
            Delta = (RTGCUINTPTR)paArgs[iArg].u.u64Number;
            iArg++;
        }
        if (iArg < cArgs)
        {
            if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
            {
                AssertMsgFailed(("Parse error, module argument required to be string!\n"));
                return VERR_PARSE_INCORRECT_ARG_TYPE;
            }
            pszModule = paArgs[iArg].u.pszString;
            iArg++;
            if (iArg < cArgs)
            {
                if (DBGCVAR_ISPOINTER(paArgs[iArg].enmType))
                {
                    AssertMsgFailed(("Parse error, module argument required to be GC pointer!\n"));
                    return VERR_PARSE_INCORRECT_ARG_TYPE;
                }
                int rc = pCmdHlp->pfnEval(pCmdHlp, &AddrVar, "%%(%Dv)", &paArgs[iArg]);
                if (VBOX_FAILURE(rc))
                    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Module address cast %%(%Dv) failed.", &paArgs[iArg]);
                ModuleAddress = paArgs[iArg].u.GCFlat;
                iArg++;
                if (iArg < cArgs)
                {
                    if (paArgs[iArg].enmType != DBGCVAR_TYPE_NUMBER)
                    {
                        AssertMsgFailed(("Parse error, module argument required to be an interger!\n"));
                        return VERR_PARSE_INCORRECT_ARG_TYPE;
                    }
                    cbModule = (unsigned)paArgs[iArg].u.u64Number;
                    iArg++;
                    if (iArg < cArgs)
                    {
                        AssertMsgFailed(("Parse error, too many arguments!\n"));
                        return VERR_PARSE_TOO_MANY_ARGUMENTS;
                    }
                }
            }
        }
    }

    /*
     * Call the debug info manager about this loading...
     */
    int rc = DBGFR3ModuleLoad(pVM, paArgs[0].u.pszString, Delta, pszModule, ModuleAddress, cbModule);
    if (VBOX_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGInfoSymbolLoad(, '%s', %VGv, '%s', %VGv, 0)\n",
                                     paArgs[0].u.pszString, Delta, pszModule, ModuleAddress);

    NOREF(pCmd); NOREF(pResult);
    return VINF_SUCCESS;
}


/**
 * The 'set' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /* parse sanity check. */
    AssertMsg(paArgs[0].enmType == DBGCVAR_TYPE_STRING, ("expected string not %d as first arg!\n", paArgs[0].enmType));
    if (paArgs[0].enmType != DBGCVAR_TYPE_STRING)
        return VERR_PARSE_INCORRECT_ARG_TYPE;


    /*
     * A variable must start with an alpha chars and only contain alpha numerical chars.
     */
    const char *pszVar = paArgs[0].u.pszString;
    if (!isalpha(*pszVar) || *pszVar == '_')
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL,
            "syntax error: Invalid variable name '%s'. Variable names must match regex '[_a-zA-Z][_a-zA-Z0-9*'!", paArgs[0].u.pszString);

    while (isalnum(*pszVar) || *pszVar == '_')
        *pszVar++;
    if (*pszVar)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL,
            "syntax error: Invalid variable name '%s'. Variable names must match regex '[_a-zA-Z][_a-zA-Z0-9*]'!", paArgs[0].u.pszString);


    /*
     * Calc variable size.
     */
    size_t  cbVar = (size_t)paArgs[0].u64Range + sizeof(DBGCNAMEDVAR);
    if (paArgs[1].enmType == DBGCVAR_TYPE_STRING)
        cbVar += 1 + (size_t)paArgs[1].u64Range;

    /*
     * Look for existing one.
     */
    pszVar = paArgs[0].u.pszString;
    for (unsigned iVar = 0; iVar < pDbgc->cVars; iVar++)
    {
        if (!strcmp(pszVar, pDbgc->papVars[iVar]->szName))
        {
            /*
             * Update existing variable.
             */
            void *pv = RTMemRealloc(pDbgc->papVars[iVar], cbVar);
            if (!pv)
                return VERR_PARSE_NO_MEMORY;
            PDBGCNAMEDVAR pVar = pDbgc->papVars[iVar] = (PDBGCNAMEDVAR)pv;

            pVar->Var = paArgs[1];
            memcpy(pVar->szName, paArgs[0].u.pszString, (size_t)paArgs[0].u64Range + 1);
            if (paArgs[1].enmType == DBGCVAR_TYPE_STRING)
                pVar->Var.u.pszString = (char *)memcpy(&pVar->szName[paArgs[0].u64Range + 1], paArgs[1].u.pszString, (size_t)paArgs[1].u64Range + 1);
            return 0;
        }
    }

    /*
     * Allocate another.
     */
    PDBGCNAMEDVAR pVar = (PDBGCNAMEDVAR)RTMemAlloc(cbVar);

    pVar->Var = paArgs[1];
    memcpy(pVar->szName, pszVar, (size_t)paArgs[0].u64Range + 1);
    if (paArgs[1].enmType == DBGCVAR_TYPE_STRING)
        pVar->Var.u.pszString = (char *)memcpy(&pVar->szName[paArgs[0].u64Range + 1], paArgs[1].u.pszString, (size_t)paArgs[1].u64Range + 1);

    /* need to reallocate the pointer array too? */
    if (!(pDbgc->cVars % 0x20))
    {
        void *pv = RTMemRealloc(pDbgc->papVars, (pDbgc->cVars + 0x20) * sizeof(pDbgc->papVars[0]));
        if (!pv)
        {
            RTMemFree(pVar);
            return VERR_PARSE_NO_MEMORY;
        }
        pDbgc->papVars = (PDBGCNAMEDVAR *)pv;
    }
    pDbgc->papVars[pDbgc->cVars++] = pVar;

    NOREF(pCmd); NOREF(pVM); NOREF(cArgs); NOREF(pResult);
    return 0;
}


/**
 * The 'unset' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdUnset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Don't trust the parser.
     */
    for (unsigned  i = 0; i < cArgs; i++)
        if (paArgs[i].enmType != DBGCVAR_TYPE_STRING)
        {
            AssertMsgFailed(("expected strings only. (arg=%d)!\n", i));
            return VERR_PARSE_INCORRECT_ARG_TYPE;
        }

    /*
     * Iterate the variables and unset them.
     */
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
    {
        const char *pszVar = paArgs[iArg].u.pszString;

        /*
         * Look up the variable.
         */
        for (unsigned iVar = 0; iVar < pDbgc->cVars; iVar++)
        {
            if (!strcmp(pszVar, pDbgc->papVars[iVar]->szName))
            {
                /*
                 * Shuffle the array removing this entry.
                 */
                void *pvFree = pDbgc->papVars[iVar];
                if (iVar + 1 < pDbgc->cVars)
                    memmove(&pDbgc->papVars[iVar],
                            &pDbgc->papVars[iVar + 1],
                            (pDbgc->cVars - iVar - 1) * sizeof(pDbgc->papVars[0]));
                pDbgc->papVars[--pDbgc->cVars] = NULL;

                RTMemFree(pvFree);
            }
        } /* lookup */
    } /* arg loop */

    NOREF(pCmd); NOREF(pVM); NOREF(pResult);
    return 0;
}


/**
 * The 'loadvars' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLoadVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Don't trust the parser.
     */
    if (    cArgs != 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    {
        AssertMsgFailed(("Expected one string exactly!\n"));
        return VERR_PARSE_INCORRECT_ARG_TYPE;
    }

    /*
     * Iterate the variables and unset them.
     */
    FILE *pFile = fopen(paArgs[0].u.pszString, "r");
    if (pFile)
    {
        char szLine[4096];
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            /* Strip it. */
            char *psz = szLine;
            while (isblank(*psz))
                psz++;
            int i = (int)strlen(psz) - 1;
            while (i >= 0 && isspace(psz[i]))
                psz[i--] ='\0';
            /* Execute it if not comment or empty line. */
            if (    *psz != '\0'
                &&  *psz != '#'
                &&  *psz != ';')
            {
                pCmdHlp->pfnPrintf(pCmdHlp, NULL, "dbg: set %s", psz);
                pCmdHlp->pfnExec(pCmdHlp, "set %s", psz);
            }
        }
        fclose(pFile);
    }
    else
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Failed to open file '%s'.\n", paArgs[0].u.pszString);

    NOREF(pCmd); NOREF(pVM); NOREF(pResult);
    return 0;
}


/**
 * The 'showvars' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdShowVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    for (unsigned iVar = 0; iVar < pDbgc->cVars; iVar++)
    {
        int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%-20s ", &pDbgc->papVars[iVar]->szName);
        if (!rc)
            rc = dbgcCmdFormat(pCmd, pCmdHlp, pVM, &pDbgc->papVars[iVar]->Var, 1, NULL);
        if (rc)
            return rc;
    }

    NOREF(paArgs); NOREF(cArgs); NOREF(pResult);
    return 0;
}


/**
 * The 'harakiri' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdHarakiri(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    Log(("dbgcCmdHarakiri\n"));
    for (;;)
        exit(126);
    NOREF(pCmd); NOREF(pCmdHlp); NOREF(pVM); NOREF(paArgs); NOREF(cArgs); NOREF(pResult);
}





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      B u l t i n   S y m b o l s
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



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







//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      O p e r a t o r s
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Minus (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpMinus(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpMinus\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFlat       = -(RTGCINTPTR)pResult->u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            pResult->u.GCFar.off    = -(int32_t)pResult->u.GCFar.off;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.GCPhys       = (RTGCPHYS) -(int64_t)pResult->u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.pvHCFlat     = (void *) -(intptr_t)pResult->u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            pResult->u.HCFar.off    = -(int32_t)pResult->u.HCFar.off;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.HCPhys       = (RTHCPHYS) -(int64_t)pResult->u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number    = -(int64_t)pResult->u.u64Number;
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        case DBGCVAR_TYPE_STRING:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    NOREF(pDbgc);
    return 0;
}


/**
 * Pluss (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpPluss(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpPluss\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_NUMBER:
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        case DBGCVAR_TYPE_STRING:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    NOREF(pDbgc);
    return 0;
}


/**
 * Boolean not (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBooleanNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpBooleanNot\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.u64Number    = !pResult->u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            pResult->u.u64Number    = !pResult->u.GCFar.off && pResult->u.GCFar.sel <= 3;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.u64Number    = !pResult->u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.u64Number    = !pResult->u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            pResult->u.u64Number    = !pResult->u.HCFar.off && pResult->u.HCFar.sel <= 3;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.u64Number    = !pResult->u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number    = !pResult->u.u64Number;
            break;
        case DBGCVAR_TYPE_STRING:
            pResult->u.u64Number    = !pResult->u64Range;
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    pResult->enmType = DBGCVAR_TYPE_NUMBER;
    NOREF(pDbgc);
    return 0;
}


/**
 * Bitwise not (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseNot(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpBitwiseNot\n"));
    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFlat       = ~pResult->u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_FAR:
            pResult->u.GCFar.off    = ~pResult->u.GCFar.off;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u.GCPhys       = ~pResult->u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.pvHCFlat     = (void *)~(uintptr_t)pResult->u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_FAR:
            pResult->u.HCFar.off= ~pResult->u.HCFar.off;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u.HCPhys       = ~pResult->u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u.u64Number    = ~pResult->u.u64Number;
            break;

        case DBGCVAR_TYPE_UNKNOWN:
        case DBGCVAR_TYPE_STRING:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    NOREF(pDbgc);
    return 0;
}


/**
 * Reference variable (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpVar(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpVar: %s\n", pArg->u.pszString));
    /*
     * Parse sanity.
     */
    if (pArg->enmType != DBGCVAR_TYPE_STRING)
        return VERR_PARSE_INCORRECT_ARG_TYPE;

    /*
     * Lookup the variable.
     */
    const char *pszVar = pArg->u.pszString;
    for (unsigned iVar = 0; iVar < pDbgc->cVars; iVar++)
    {
        if (!strcmp(pszVar, pDbgc->papVars[iVar]->szName))
        {
            *pResult = pDbgc->papVars[iVar]->Var;
            return 0;
        }
    }

    return VERR_PARSE_VARIABLE_NOT_FOUND;
}


/**
 * Flat address (unary).
 *
 * @returns VINF_SUCCESS on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAddrFlat(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpAddrFlat\n"));
    int     rc;
    *pResult = *pArg;

    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_GC_FAR:
        {
            Assert(pDbgc->pVM);
            DBGFADDRESS Address;
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (VBOX_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_GC_FLAT;
                pResult->u.GCFlat = Address.FlatPtr;
                return VINF_SUCCESS;
            }
            return VERR_PARSE_CONVERSION_FAILED;
        }

        case DBGCVAR_TYPE_GC_PHYS:
            //rc = MMR3PhysGCPhys2GCVirtEx(pDbgc->pVM, pResult->u.GCPhys, ..., &pResult->u.GCFlat); - yea, sure.
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_FLAT:
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_HC_FAR:
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_PHYS:
            Assert(pDbgc->pVM);
            pResult->enmType        = DBGCVAR_TYPE_HC_FLAT;
            rc = MMR3HCPhys2HCVirt(pDbgc->pVM, pResult->u.HCPhys, &pResult->u.pvHCFlat);
            if (VBOX_SUCCESS(rc))
                return VINF_SUCCESS;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_GC_FLAT;
            pResult->u.GCFlat   = (RTGCPTR)pResult->u.u64Number;
            return VINF_SUCCESS;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_GC_FLAT, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
}


/**
 * Physical address (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAddrPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpAddrPhys\n"));
    int     rc;

    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_GC_PHYS;
            rc = PGMPhysGCPtr2GCPhys(pDbgc->pVM, pArg->u.GCFlat, &pResult->u.GCPhys);
            if (VBOX_SUCCESS(rc))
                return 0;
            /** @todo more memory types! */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_FAR:
        {
            Assert(pDbgc->pVM);
            DBGFADDRESS Address;
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (VBOX_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_GC_PHYS;
                rc = PGMPhysGCPtr2GCPhys(pDbgc->pVM, Address.FlatPtr, &pResult->u.GCPhys);
                if (VBOX_SUCCESS(rc))
                    return 0;
                /** @todo more memory types! */
            }
            return VERR_PARSE_CONVERSION_FAILED;
        }

        case DBGCVAR_TYPE_GC_PHYS:
            return 0;

        case DBGCVAR_TYPE_HC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_GC_PHYS;
            rc = PGMR3DbgHCPtr2GCPhys(pDbgc->pVM, pArg->u.pvHCFlat, &pResult->u.GCPhys);
            if (VBOX_SUCCESS(rc))
                return 0;
            /** @todo more memory types! */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FAR:
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_PHYS:
            return 0;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_GC_PHYS;
            pResult->u.GCPhys   = (RTGCPHYS)pResult->u.u64Number;
            return 0;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_GC_PHYS, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return 0;
}


/**
 * Physical host address (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAddrHostPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpAddrPhys\n"));
    int     rc;

    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
            rc = PGMPhysGCPtr2HCPhys(pDbgc->pVM, pArg->u.GCFlat, &pResult->u.HCPhys);
            if (VBOX_SUCCESS(rc))
                return 0;
            /** @todo more memory types. */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_FAR:
        {
            Assert(pDbgc->pVM);
            DBGFADDRESS Address;
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (VBOX_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
                rc = PGMPhysGCPtr2HCPhys(pDbgc->pVM, Address.FlatPtr, &pResult->u.HCPhys);
                if (VBOX_SUCCESS(rc))
                    return 0;
                /** @todo more memory types. */
            }
            return VERR_PARSE_CONVERSION_FAILED;
        }

        case DBGCVAR_TYPE_GC_PHYS:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
            rc = PGMPhysGCPhys2HCPhys(pDbgc->pVM, pArg->u.GCFlat, &pResult->u.HCPhys);
            if (VBOX_SUCCESS(rc))
                return 0;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_PHYS;
            rc = PGMR3DbgHCPtr2HCPhys(pDbgc->pVM, pArg->u.pvHCFlat, &pResult->u.HCPhys);
            if (VBOX_SUCCESS(rc))
                return 0;
            /** @todo more memory types! */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FAR:
            return VERR_PARSE_INCORRECT_ARG_TYPE;

        case DBGCVAR_TYPE_HC_PHYS:
            return 0;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_HC_PHYS;
            pResult->u.HCPhys   = (RTGCPHYS)pResult->u.u64Number;
            return 0;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_HC_PHYS, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return 0;
}


/**
 * Host address (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAddrHost(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpAddrHost\n"));
    int     rc;

    *pResult = *pArg;
    switch (pArg->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_FLAT;
            rc = PGMPhysGCPtr2HCPtr(pDbgc->pVM, pArg->u.GCFlat, &pResult->u.pvHCFlat);
            if (VBOX_SUCCESS(rc))
                return 0;
            /** @todo more memory types. */
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_GC_FAR:
        {
            Assert(pDbgc->pVM);
            DBGFADDRESS Address;
            rc = DBGFR3AddrFromSelOff(pDbgc->pVM, &Address, pArg->u.GCFar.sel, pArg->u.GCFar.off);
            if (VBOX_SUCCESS(rc))
            {
                pResult->enmType = DBGCVAR_TYPE_HC_FLAT;
                rc = PGMPhysGCPtr2HCPtr(pDbgc->pVM, Address.FlatPtr, &pResult->u.pvHCFlat);
                if (VBOX_SUCCESS(rc))
                    return 0;
                /** @todo more memory types. */
            }
            return VERR_PARSE_CONVERSION_FAILED;
        }

        case DBGCVAR_TYPE_GC_PHYS:
            Assert(pDbgc->pVM);
            pResult->enmType = DBGCVAR_TYPE_HC_FLAT;
            rc = PGMPhysGCPhys2HCPtr(pDbgc->pVM, pArg->u.GCPhys, 1, &pResult->u.pvHCFlat);
            if (VBOX_SUCCESS(rc))
                return 0;
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_HC_FLAT:
            return 0;

        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
            /** @todo !*/
            return VERR_PARSE_CONVERSION_FAILED;

        case DBGCVAR_TYPE_NUMBER:
            pResult->enmType    = DBGCVAR_TYPE_HC_FLAT;
            pResult->u.pvHCFlat = (void *)(uintptr_t)pResult->u.u64Number;
            return 0;

        case DBGCVAR_TYPE_STRING:
            return dbgcSymbolGet(pDbgc, pArg->u.pszString, DBGCVAR_TYPE_HC_FLAT, pResult);

        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
}

/**
 * Bitwise not (unary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAddrFar(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpAddrFar\n"));
    int     rc;

    switch (pArg1->enmType)
    {
        case DBGCVAR_TYPE_STRING:
            rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_NUMBER, pResult);
            if (VBOX_FAILURE(rc))
                return rc;
            break;
        case DBGCVAR_TYPE_NUMBER:
            *pResult = *pArg1;
            break;

        case DBGCVAR_TYPE_GC_FLAT:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    pResult->u.GCFar.sel = (RTSEL)pResult->u.u64Number;

    /* common code for the two types we support. */
    switch (pArg2->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u.GCFar.off = pArg2->u.GCFlat;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;

        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.HCFar.off = pArg2->u.GCFlat;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;

        case DBGCVAR_TYPE_NUMBER:
            pResult->u.GCFar.off = (RTGCPTR)pArg2->u.u64Number;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;

        case DBGCVAR_TYPE_STRING:
        {
            DBGCVAR Var;
            rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
            if (VBOX_FAILURE(rc))
                return rc;
            pResult->u.GCFar.off = (RTGCPTR)Var.u.u64Number;
            pResult->enmType    = DBGCVAR_TYPE_GC_FAR;
            break;
        }

        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_HC_FAR:
        case DBGCVAR_TYPE_HC_PHYS:
        case DBGCVAR_TYPE_UNKNOWN:
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return 0;

}


/**
 * Multiplication operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpMult(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpMult\n"));
    int     rc;

    /*
     * Switch the factors so we preserve pointers, far pointers are considered more
     * important that physical and flat pointers.
     */
    if (    DBGCVAR_ISPOINTER(pArg2->enmType)
        &&  (   !DBGCVAR_ISPOINTER(pArg1->enmType)
             || (   DBGCVAR_IS_FAR_PTR(pArg2->enmType)
                 && !DBGCVAR_IS_FAR_PTR(pArg1->enmType))))
    {
        PCDBGCVAR pTmp = pArg1;
        pArg2 = pArg1;
        pArg1 = pTmp;
    }

    /*
     * Convert the 2nd number into a number we use multiply the first with.
     */
    DBGCVAR Factor2 = *pArg2;
    if (    Factor2.enmType == DBGCVAR_TYPE_STRING
        ||  Factor2.enmType == DBGCVAR_TYPE_SYMBOL)
    {
        rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Factor2);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    uint64_t u64Factor;
    switch (Factor2.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:  u64Factor = Factor2.u.GCFlat; break;
        case DBGCVAR_TYPE_GC_FAR:   u64Factor = Factor2.u.GCFar.off; break;
        case DBGCVAR_TYPE_GC_PHYS:  u64Factor = Factor2.u.GCPhys; break;
        case DBGCVAR_TYPE_HC_FLAT:  u64Factor = (uintptr_t)Factor2.u.pvHCFlat; break;
        case DBGCVAR_TYPE_HC_FAR:   u64Factor = Factor2.u.HCFar.off; break;
        case DBGCVAR_TYPE_HC_PHYS:  u64Factor = Factor2.u.HCPhys; break;
        case DBGCVAR_TYPE_NUMBER:   u64Factor = Factor2.u.u64Number; break;
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }

    /*
     * Fix symbols in the 1st factor.
     */
    *pResult = *pArg1;
    if (    pResult->enmType == DBGCVAR_TYPE_STRING
        ||  pResult->enmType == DBGCVAR_TYPE_SYMBOL)
    {
        rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, pResult);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    /*
     * Do the multiplication.
     */
    switch (pArg1->enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:  pResult->u.GCFlat *= u64Factor; break;
        case DBGCVAR_TYPE_GC_FAR:   pResult->u.GCFar.off *= u64Factor; break;
        case DBGCVAR_TYPE_GC_PHYS:  pResult->u.GCPhys *= u64Factor; break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u.pvHCFlat = (void *)(uintptr_t)((uintptr_t)pResult->u.pvHCFlat * u64Factor);
            break;
        case DBGCVAR_TYPE_HC_FAR:   pResult->u.HCFar.off *= u64Factor; break;
        case DBGCVAR_TYPE_HC_PHYS:  pResult->u.HCPhys *= u64Factor; break;
        case DBGCVAR_TYPE_NUMBER:   pResult->u.u64Number *= u64Factor; break;
        default:
            return VERR_PARSE_INCORRECT_ARG_TYPE;
    }
    return 0;
}


/**
 * Division operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpDiv(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpDiv\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Modulus operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpMod(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpMod\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Addition operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpAdd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpAdd\n"));

    /*
     * An addition operation will return (when possible) the left side type in the
     * expression. We make an omission for numbers, where we'll take the right side
     * type instead. An expression where only the left hand side is a string we'll
     * use the right hand type assuming that the string is a symbol.
     */
    if (    (pArg1->enmType == DBGCVAR_TYPE_NUMBER && pArg2->enmType != DBGCVAR_TYPE_STRING)
        ||  (pArg1->enmType == DBGCVAR_TYPE_STRING && pArg2->enmType != DBGCVAR_TYPE_STRING))
    {
        PCDBGCVAR pTmp = pArg2;
        pArg2 = pArg1;
        pArg1 = pTmp;
    }
    DBGCVAR     Sym1, Sym2;
    if (pArg1->enmType == DBGCVAR_TYPE_STRING)
    {
        int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, &Sym1);
        if (VBOX_FAILURE(rc))
            return rc;
        pArg1 = &Sym1;

        rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_ANY, &Sym2);
        if (VBOX_FAILURE(rc))
            return rc;
        pArg2 = &Sym2;
    }

    int         rc;
    DBGCVAR     Var;
    DBGCVAR     Var2;
    switch (pArg1->enmType)
    {
        /*
         * GC Flat
         */
        case DBGCVAR_TYPE_GC_FLAT:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat += pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Far
         */
        case DBGCVAR_TYPE_GC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.GCFar.off += (RTGCPTR)pArg2->u.u64Number;
                    break;
                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat += pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Phys
         */
        case DBGCVAR_TYPE_GC_PHYS:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrPhys(pDbgc, pArg2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    if (Var.enmType != DBGCVAR_TYPE_GC_PHYS)
                        return VERR_PARSE_INVALID_OPERATION;
                    pResult->u.GCPhys += Var.u.GCPhys;
                    break;
            }
            break;

        /*
         * HC Flat
         */
        case DBGCVAR_TYPE_HC_FLAT:
            *pResult = *pArg1;
            rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
            if (VBOX_FAILURE(rc))
                return rc;
            rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
            if (VBOX_FAILURE(rc))
                return rc;
            pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat + (uintptr_t)Var.u.pvHCFlat;
            break;

        /*
         * HC Far
         */
        case DBGCVAR_TYPE_HC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.HCFar.off += (uintptr_t)pArg2->u.u64Number;
                    break;

                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat + (uintptr_t)Var.u.pvHCFlat;
                    break;
            }
            break;

        /*
         * HC Phys
         */
        case DBGCVAR_TYPE_HC_PHYS:
            *pResult = *pArg1;
            rc = dbgcOpAddrHostPhys(pDbgc, pArg2, &Var);
            if (VBOX_FAILURE(rc))
                return rc;
            pResult->u.HCPhys += Var.u.HCPhys;
            break;

        /*
         * Numbers (see start of function)
         */
        case DBGCVAR_TYPE_NUMBER:
            *pResult = *pArg1;
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                    rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                case DBGCVAR_TYPE_NUMBER:
                    pResult->u.u64Number += pArg2->u.u64Number;
                    break;
                default:
                    return VERR_PARSE_INVALID_OPERATION;
            }
            break;

        default:
            return VERR_PARSE_INVALID_OPERATION;

    }
    return 0;
}


/**
 * Subtration operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpSub(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpSub\n"));

    /*
     * An subtraction operation will return the left side type in the expression.
     * However, if the left hand side is a number and the right hand a pointer of
     * some kind we'll convert the left hand side to the same type as the right hand.
     * Any strings will be attempted resolved as symbols.
     */
    DBGCVAR     Sym1, Sym2;
    if (    pArg2->enmType == DBGCVAR_TYPE_STRING
        &&  (   pArg1->enmType == DBGCVAR_TYPE_NUMBER
             || pArg1->enmType == DBGCVAR_TYPE_STRING))
    {
        int rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_ANY, &Sym2);
        if (VBOX_FAILURE(rc))
            return rc;
        pArg2 = &Sym2;
    }

    if (pArg1->enmType == DBGCVAR_TYPE_STRING)
    {
        DBGCVARTYPE enmType;
        switch (pArg2->enmType)
        {
            case DBGCVAR_TYPE_NUMBER:
                enmType = DBGCVAR_TYPE_ANY;
                break;
            case DBGCVAR_TYPE_GC_FLAT:
            case DBGCVAR_TYPE_GC_PHYS:
            case DBGCVAR_TYPE_HC_FLAT:
            case DBGCVAR_TYPE_HC_PHYS:
                enmType = pArg2->enmType;
                break;
            case DBGCVAR_TYPE_GC_FAR:
                enmType = DBGCVAR_TYPE_GC_FLAT;
                break;
            case DBGCVAR_TYPE_HC_FAR:
                enmType = DBGCVAR_TYPE_HC_FLAT;
                break;

            default:
            case DBGCVAR_TYPE_STRING:
                AssertMsgFailed(("Can't happen\n"));
                enmType = DBGCVAR_TYPE_STRING;
                break;
        }
        if (enmType != DBGCVAR_TYPE_STRING)
        {
            int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, &Sym1);
            if (VBOX_FAILURE(rc))
                return rc;
            pArg1 = &Sym1;
        }
    }
    else if (pArg1->enmType == DBGCVAR_TYPE_NUMBER)
    {
        PFNDBGCOPUNARY pOp = NULL;
        switch (pArg2->enmType)
        {
            case DBGCVAR_TYPE_GC_FAR:
            case DBGCVAR_TYPE_GC_FLAT:
                pOp = dbgcOpAddrFlat;
                break;
            case DBGCVAR_TYPE_GC_PHYS:
                pOp = dbgcOpAddrPhys;
                break;
            case DBGCVAR_TYPE_HC_FAR:
            case DBGCVAR_TYPE_HC_FLAT:
                pOp = dbgcOpAddrHost;
                break;
            case DBGCVAR_TYPE_HC_PHYS:
                pOp = dbgcOpAddrHostPhys;
                break;
            case DBGCVAR_TYPE_NUMBER:
                break;
            default:
            case DBGCVAR_TYPE_STRING:
                AssertMsgFailed(("Can't happen\n"));
                break;
        }
        if (pOp)
        {
            int rc = pOp(pDbgc, pArg1, &Sym1);
            if (VBOX_FAILURE(rc))
                return rc;
            pArg1 = &Sym1;
        }
    }


    /*
     * Normal processing.
     */
    int         rc;
    DBGCVAR     Var;
    DBGCVAR     Var2;
    switch (pArg1->enmType)
    {
        /*
         * GC Flat
         */
        case DBGCVAR_TYPE_GC_FLAT:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat -= pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Far
         */
        case DBGCVAR_TYPE_GC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.GCFar.off -= (RTGCPTR)pArg2->u.u64Number;
                    break;
                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, pArg2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    pResult->u.GCFlat -= pArg2->u.GCFlat;
                    break;
            }
            break;

        /*
         * GC Phys
         */
        case DBGCVAR_TYPE_GC_PHYS:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_INVALID_OPERATION;
                default:
                    *pResult = *pArg1;
                    rc = dbgcOpAddrPhys(pDbgc, pArg2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    if (Var.enmType != DBGCVAR_TYPE_GC_PHYS)
                        return VERR_PARSE_INVALID_OPERATION;
                    pResult->u.GCPhys -= Var.u.GCPhys;
                    break;
            }
            break;

        /*
         * HC Flat
         */
        case DBGCVAR_TYPE_HC_FLAT:
            *pResult = *pArg1;
            rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
            if (VBOX_FAILURE(rc))
                return rc;
            rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
            if (VBOX_FAILURE(rc))
                return rc;
            pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat - (uintptr_t)Var.u.pvHCFlat;
            break;

        /*
         * HC Far
         */
        case DBGCVAR_TYPE_HC_FAR:
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_NUMBER:
                    *pResult = *pArg1;
                    pResult->u.HCFar.off -= (uintptr_t)pArg2->u.u64Number;
                    break;

                default:
                    rc = dbgcOpAddrFlat(pDbgc, pArg1, pResult);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrHost(pDbgc, pArg2, &Var2);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    rc = dbgcOpAddrFlat(pDbgc, &Var2, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                    pResult->u.pvHCFlat = (char *)pResult->u.pvHCFlat - (uintptr_t)Var.u.pvHCFlat;
                    break;
            }
            break;

        /*
         * HC Phys
         */
        case DBGCVAR_TYPE_HC_PHYS:
            *pResult = *pArg1;
            rc = dbgcOpAddrHostPhys(pDbgc, pArg2, &Var);
            if (VBOX_FAILURE(rc))
                return rc;
            pResult->u.HCPhys -= Var.u.HCPhys;
            break;

        /*
         * Numbers (see start of function)
         */
        case DBGCVAR_TYPE_NUMBER:
            *pResult = *pArg1;
            switch (pArg2->enmType)
            {
                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                    rc = dbgcSymbolGet(pDbgc, pArg2->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
                    if (VBOX_FAILURE(rc))
                        return rc;
                case DBGCVAR_TYPE_NUMBER:
                    pResult->u.u64Number -= pArg2->u.u64Number;
                    break;
                default:
                    return VERR_PARSE_INVALID_OPERATION;
            }
            break;

        default:
            return VERR_PARSE_INVALID_OPERATION;

    }
    return 0;
}


/**
 * Bitwise shift left operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseShiftLeft(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseShiftLeft\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Bitwise shift right operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseShiftRight(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseShiftRight\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Bitwise and operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseAnd\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Bitwise exclusive or operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseXor(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseXor\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Bitwise inclusive or operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBitwiseOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBitwiseOr\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Boolean and operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBooleanAnd(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBooleanAnd\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Boolean or operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpBooleanOr(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
    LogFlow(("dbgcOpBooleanOr\n"));
    NOREF(pDbgc); NOREF(pArg1); NOREF(pArg2); NOREF(pResult);
    return -1;
}


/**
 * Range to operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpRangeLength(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpRangeLength\n"));
    /*
     * Make result. Strings needs to be resolved into symbols.
     */
    if (pArg1->enmType == DBGCVAR_TYPE_STRING)
    {
        int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_ANY, pResult);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    else
        *pResult = *pArg1;

    /*
     * Convert 2nd argument to element count.
     */
    pResult->enmRangeType = DBGCVAR_RANGE_ELEMENTS;
    switch (pArg2->enmType)
    {
        case DBGCVAR_TYPE_NUMBER:
            pResult->u64Range = pArg2->u.u64Number;
            break;

        case DBGCVAR_TYPE_STRING:
        {
            int rc = dbgcSymbolGet(pDbgc, pArg1->u.pszString, DBGCVAR_TYPE_NUMBER, pResult);
            if (VBOX_FAILURE(rc))
                return rc;
            pResult->u64Range = pArg2->u.u64Number;
            break;
        }

        default:
            return VERR_PARSE_INVALID_OPERATION;
    }

    return VINF_SUCCESS;
}


/**
 * Range to operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpRangeLengthBytes(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpRangeLengthBytes\n"));
    int rc = dbgcOpRangeLength(pDbgc, pArg1, pArg2, pResult);
    if (VBOX_SUCCESS(rc))
        pResult->enmRangeType = DBGCVAR_RANGE_BYTES;
    return rc;
}


/**
 * Range to operator (binary).
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
static DECLCALLBACK(int) dbgcOpRangeTo(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult)
{
//    LogFlow(("dbgcOpRangeTo\n"));
    /*
     * Calc number of bytes between the two args.
     */
    DBGCVAR Diff;
    int rc = dbgcOpSub(pDbgc, pArg2, pArg1, &Diff);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Use the diff as the range of Arg1.
     */
    *pResult = *pArg1;
    pResult->enmRangeType = DBGCVAR_RANGE_BYTES;
    switch (Diff.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            pResult->u64Range = (RTGCUINTPTR)Diff.u.GCFlat;
            break;
        case DBGCVAR_TYPE_GC_PHYS:
            pResult->u64Range = Diff.u.GCPhys;
            break;
        case DBGCVAR_TYPE_HC_FLAT:
            pResult->u64Range = (uintptr_t)Diff.u.pvHCFlat;
            break;
        case DBGCVAR_TYPE_HC_PHYS:
            pResult->u64Range = Diff.u.HCPhys;
            break;
        case DBGCVAR_TYPE_NUMBER:
            pResult->u64Range = Diff.u.u64Number;
            break;

        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_STRING:
        case DBGCVAR_TYPE_HC_FAR:
        default:
            AssertMsgFailed(("Impossible!\n"));
            return VERR_PARSE_INVALID_OPERATION;
    }

    return 0;
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









//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      C a l l b a c k   H e l p e r s
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



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
            if (VBOX_SUCCESS(rc) && pszFormat)
                rc = pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszFormat, args);
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
    int rc = dbgcProcessCommand(pDbgc, pszScratch, cb);

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





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      V a r i a b l e   M a n i p u l a t i o n
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



/** @todo move me!*/
static void dbgcVarSetGCFlat(PDBGCVAR pVar, RTGCPTR GCFlat)
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
static void dbgcVarSetGCFlatByteRange(PDBGCVAR pVar, RTGCPTR GCFlat, uint64_t cb)
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
static void dbgcVarSetVar(PDBGCVAR pVar, PCDBGCVAR pVar2)
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
static void dbgcVarSetByteRange(PDBGCVAR pVar, uint64_t cb)
{
    if (pVar)
    {
        pVar->enmRangeType = DBGCVAR_RANGE_BYTES;
        pVar->u64Range  = cb;
    }
}


/** @todo move me!*/
static void dbgcVarSetNoRange(PDBGCVAR pVar)
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
static int dbgcVarToDbgfAddr(PDBGC pDbgc, PCDBGCVAR pVar, PDBGFADDRESS pAddress)
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
static int dbgcBpAdd(PDBGC pDbgc, RTUINT iBp, const char *pszCmd)
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
static int dbgcBpUpdate(PDBGC pDbgc, RTUINT iBp, const char *pszCmd)
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
static int dbgcBpDelete(PDBGC pDbgc, RTUINT iBp)
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
static PDBGCBP dbgcBpGet(PDBGC pDbgc, RTUINT iBp)
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
static int dbgcBpExec(PDBGC pDbgc, RTUINT iBp)
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





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//
//      I n p u t ,   p a r s i n g   a n d   l o g g i n g
//
//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



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
        if (VBOX_FAILURE(rc))
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
        if (VBOX_FAILURE(rc))
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
 * Finds a builtin symbol.
 * @returns Pointer to symbol descriptor on success.
 * @returns NULL on failure.
 * @param   pDbgc       The debug console instance.
 * @param   pszSymbol   The symbol name.
 */
static PCDBGCSYM dbgcLookupRegisterSymbol(PDBGC pDbgc, const char *pszSymbol)
{
    for (unsigned iSym = 0; iSym < ELEMENTS(g_aSyms); iSym++)
        if (!strcmp(pszSymbol, g_aSyms[iSym].pszName))
            return &g_aSyms[iSym];

    /** @todo externally registered symbols. */
    NOREF(pDbgc);
    return NULL;
}


/**
 * Resolves a symbol (or tries to do so at least).
 *
 * @returns 0 on success.
 * @returns VBox status on failure.
 * @param   pDbgc       The debug console instance.
 * @param   pszSymbol   The symbol name.
 * @param   enmType     The result type.
 * @param   pResult     Where to store the result.
 */
static int dbgcSymbolGet(PDBGC pDbgc, const char *pszSymbol, DBGCVARTYPE enmType, PDBGCVAR pResult)
{
    /*
     * Builtin?
     */
    PCDBGCSYM   pSymDesc = dbgcLookupRegisterSymbol(pDbgc, pszSymbol);
    if (pSymDesc)
    {
        if (!pSymDesc->pfnGet)
            return VERR_PARSE_WRITEONLY_SYMBOL;
        return pSymDesc->pfnGet(pSymDesc, &pDbgc->CmdHlp, enmType, pResult);
    }


    /*
     * Ask PDM.
     */
    /** @todo resolve symbols using PDM. */


    /*
     * Ask the debug info manager.
     */
    DBGFSYMBOL Symbol;
    int rc = DBGFR3SymbolByName(pDbgc->pVM, pszSymbol, &Symbol);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Default return is a flat gc address.
         */
        memset(pResult, 0,  sizeof(*pResult));
        pResult->enmRangeType = Symbol.cb ? DBGCVAR_RANGE_BYTES : DBGCVAR_RANGE_NONE;
        pResult->u64Range     = Symbol.cb;
        pResult->enmType      = DBGCVAR_TYPE_GC_FLAT;
        pResult->u.GCFlat     = Symbol.Value;
        DBGCVAR VarTmp;
        switch (enmType)
        {
            /* nothing to do. */
            case DBGCVAR_TYPE_GC_FLAT:
            case DBGCVAR_TYPE_GC_FAR:
            case DBGCVAR_TYPE_ANY:
                return VINF_SUCCESS;

            /* simply make it numeric. */
            case DBGCVAR_TYPE_NUMBER:
                pResult->enmType = DBGCVAR_TYPE_NUMBER;
                pResult->u.u64Number = Symbol.Value;
                return VINF_SUCCESS;

            /* cast it. */

            case DBGCVAR_TYPE_GC_PHYS:
                VarTmp = *pResult;
                return dbgcOpAddrPhys(pDbgc, &VarTmp, pResult);

            case DBGCVAR_TYPE_HC_FAR:
            case DBGCVAR_TYPE_HC_FLAT:
                VarTmp = *pResult;
                return dbgcOpAddrHost(pDbgc, &VarTmp, pResult);

            case DBGCVAR_TYPE_HC_PHYS:
                VarTmp = *pResult;
                return dbgcOpAddrHostPhys(pDbgc, &VarTmp, pResult);

            default:
                AssertMsgFailed(("Internal error enmType=%d\n", enmType));
                return VERR_INVALID_PARAMETER;
        }
    }

    return VERR_PARSE_NOT_IMPLEMENTED;
}



/**
 * Finds a routine.
 *
 * @returns Pointer to the command descriptor.
 *          If the request was for an external command, the caller is responsible for
 *          unlocking the external command list.
 * @returns NULL if not found.
 * @param   pDbgc       The debug console instance.
 * @param   pachName    Pointer to the routine string (not terminated).
 * @param   cchName     Length of the routine name.
 * @param   fExternal   Whether or not the routine is external.
 */
static PCDBGCCMD dbgcRoutineLookup(PDBGC pDbgc, const char *pachName, size_t cchName, bool fExternal)
{
    if (!fExternal)
    {
        /* emulation first, so commands can be overloaded (info ++). */
        PCDBGCCMD pCmd = pDbgc->paEmulationCmds;
        unsigned cLeft = pDbgc->cEmulationCmds;
        while (cLeft-- > 0)
        {
            if (    !strncmp(pachName, pCmd->pszCmd, cchName)
                &&  !pCmd->pszCmd[cchName])
                return pCmd;
            pCmd++;
        }

        for (unsigned iCmd = 0; iCmd < ELEMENTS(g_aCmds); iCmd++)
        {
            if (    !strncmp(pachName, g_aCmds[iCmd].pszCmd, cchName)
                &&  !g_aCmds[iCmd].pszCmd[cchName])
                return &g_aCmds[iCmd];
        }
    }
    else
    {
        DBGCEXTCMDS_LOCK_RD();
        for (PDBGCEXTCMDS pExtCmds = g_pExtCmdsHead; pExtCmds; pExtCmds = pExtCmds->pNext)
        {
            for (unsigned iCmd = 0; iCmd < pExtCmds->cCmds; iCmd++)
            {
                if (    !strncmp(pachName, pExtCmds->paCmds[iCmd].pszCmd, cchName)
                    &&  !pExtCmds->paCmds[iCmd].pszCmd[cchName])
                    return &pExtCmds->paCmds[iCmd];
            }
        }
        DBGCEXTCMDS_UNLOCK_RD();
    }

    NOREF(pDbgc);
    return NULL;
}


/**
 * Searches for an operator descriptor which matches the start of
 * the expression given us.
 *
 * @returns Pointer to the operator on success.
 * @param   pDbgc           The debug console instance.
 * @param   pszExpr         Pointer to the expression string which might start with an operator.
 * @param   fPreferBinary   Whether to favour binary or unary operators.
 *                          Caller must assert that it's the disired type! Both types will still
 *                          be returned, this is only for resolving duplicates.
 * @param   chPrev          The previous char. Some operators requires a blank in front of it.
 */
static PCDBGCOP dbgcOperatorLookup(PDBGC pDbgc, const char *pszExpr, bool fPreferBinary, char chPrev)
{
    PCDBGCOP    pOp = NULL;
    for (unsigned iOp = 0; iOp < ELEMENTS(g_aOps); iOp++)
    {
        if (     g_aOps[iOp].szName[0] == pszExpr[0]
            &&  (!g_aOps[iOp].szName[1] || g_aOps[iOp].szName[1] == pszExpr[1])
            &&  (!g_aOps[iOp].szName[2] || g_aOps[iOp].szName[2] == pszExpr[2]))
        {
            /*
             * Check that we don't mistake it for some other operator which have more chars.
             */
            unsigned j;
            for (j = iOp + 1; j < ELEMENTS(g_aOps); j++)
                if (    g_aOps[j].cchName > g_aOps[iOp].cchName
                    &&  g_aOps[j].szName[0] == pszExpr[0]
                    &&  (!g_aOps[j].szName[1] || g_aOps[j].szName[1] == pszExpr[1])
                    &&  (!g_aOps[j].szName[2] || g_aOps[j].szName[2] == pszExpr[2]) )
                    break;
            if (j < ELEMENTS(g_aOps))
                continue;       /* we'll catch it later. (for theoretical +,++,+++ cases.) */
            pOp = &g_aOps[iOp];

            /*
             * Prefered type?
             */
            if (g_aOps[iOp].fBinary == fPreferBinary)
                break;
        }
    }

    if (pOp)
        Log2(("dbgcOperatorLookup: pOp=%p %s\n", pOp, pOp->szName));
    NOREF(pDbgc); NOREF(chPrev);
    return pOp;
}


/**
 * Initalizes g_bmOperatorChars.
 */
static void dbgcInitOpCharBitMap(void)
{
    memset(g_bmOperatorChars, 0, sizeof(g_bmOperatorChars));
    for (unsigned iOp = 0; iOp < RT_ELEMENTS(g_aOps); iOp++)
        ASMBitSet(&g_bmOperatorChars[0], (uint8_t)g_aOps[iOp].szName[0]);
}


/**
 * Checks whether the character may be the start of an operator.
 *
 * @returns true/false.
 * @param   ch      The character.
 */
DECLINLINE(bool) dbgcIsOpChar(char ch)
{
    return ASMBitTest(&g_bmOperatorChars[0], (uint8_t)ch);
}


static int dbgcEvalSubString(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pArg)
{
    Log2(("dbgcEvalSubString: cchExpr=%d pszExpr=%s\n", cchExpr, pszExpr));

    /*
     * Removing any quoting and escapings.
     */
    char ch = *pszExpr;
    if (ch == '"' || ch == '\'' || ch == '`')
    {
        if (pszExpr[--cchExpr] != ch)
            return VERR_PARSE_UNBALANCED_QUOTE;
        cchExpr--;
        pszExpr++;

        /** @todo string unescaping. */
    }
    pszExpr[cchExpr] = '\0';

    /*
     * Make the argument.
     */
    pArg->pDesc         = NULL;
    pArg->pNext         = NULL;
    pArg->enmType       = DBGCVAR_TYPE_STRING;
    pArg->u.pszString   = pszExpr;
    pArg->enmRangeType  = DBGCVAR_RANGE_BYTES;
    pArg->u64Range      = cchExpr;

    NOREF(pDbgc);
    return 0;
}


static int dbgcEvalSubNum(char *pszExpr, unsigned uBase, PDBGCVAR pArg)
{
    Log2(("dbgcEvalSubNum: uBase=%d pszExpr=%s\n", uBase, pszExpr));
    /*
     * Convert to number.
     */
    uint64_t    u64 = 0;
    char        ch;
    while ((ch = *pszExpr) != '\0')
    {
        uint64_t    u64Prev = u64;
        unsigned    u = ch - '0';
        if (u < 10 && u < uBase)
            u64 = u64 * uBase + u;
        else if (ch >= 'a' && (u = ch - ('a' - 10)) < uBase)
            u64 = u64 * uBase + u;
        else if (ch >= 'A' && (u = ch - ('A' - 10)) < uBase)
            u64 = u64 * uBase + u;
        else
            return VERR_PARSE_INVALID_NUMBER;

        /* check for overflow - ARG!!! How to detect overflow correctly!?!?!? */
        if (u64Prev != u64 / uBase)
            return VERR_PARSE_NUMBER_TOO_BIG;

        /* next */
        pszExpr++;
    }

    /*
     * Initialize the argument.
     */
    pArg->pDesc         = NULL;
    pArg->pNext         = NULL;
    pArg->enmType       = DBGCVAR_TYPE_NUMBER;
    pArg->u.u64Number   = u64;
    pArg->enmRangeType  = DBGCVAR_RANGE_NONE;
    pArg->u64Range      = 0;

    return 0;
}


/**
 * Match variable and variable descriptor, promoting the variable if necessary.
 *
 * @returns VBox status code.
 * @param   pDbgc       Debug console instanace.
 * @param   pVar        Variable.
 * @param   pVarDesc    Variable descriptor.
 */
static int dbgcEvalSubMatchVar(PDBGC pDbgc, PDBGCVAR pVar, PCDBGCVARDESC pVarDesc)
{
    /*
     * (If match or promoted to match, return, else break.)
     */
    switch (pVarDesc->enmCategory)
    {
        /*
         * Anything goes
         */
        case DBGCVAR_CAT_ANY:
            return VINF_SUCCESS;

        /*
         * Pointer with and without range.
         * We can try resolve strings and symbols as symbols and
         * promote numbers to flat GC pointers.
         */
        case DBGCVAR_CAT_POINTER_NO_RANGE:
            if (pVar->enmRangeType != DBGCVAR_RANGE_NONE)
                return VERR_PARSE_NO_RANGE_ALLOWED;
            /* fallthru */
        case DBGCVAR_CAT_POINTER:
            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                case DBGCVAR_TYPE_GC_FAR:
                case DBGCVAR_TYPE_GC_PHYS:
                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                {
                    DBGCVAR Var;
                    int rc = dbgcSymbolGet(pDbgc, pVar->u.pszString, DBGCVAR_TYPE_GC_FLAT, &Var);
                    if (VBOX_SUCCESS(rc))
                    {
                        /* deal with range */
                        if (pVar->enmRangeType != DBGCVAR_RANGE_NONE)
                        {
                            Var.enmRangeType = pVar->enmRangeType;
                            Var.u64Range = pVar->u64Range;
                        }
                        else if (pVarDesc->enmCategory == DBGCVAR_CAT_POINTER_NO_RANGE)
                            Var.enmRangeType = DBGCVAR_RANGE_NONE;
                        *pVar = Var;
                        return rc;
                    }
                    break;
                }

                case DBGCVAR_TYPE_NUMBER:
                {
                    RTGCPTR GCPtr = (RTGCPTR)pVar->u.u64Number;
                    pVar->enmType = DBGCVAR_TYPE_GC_FLAT;
                    pVar->u.GCFlat = GCPtr;
                    return VINF_SUCCESS;
                }

                default:
                    break;
            }
            break;

        /*
         * GC pointer with and without range.
         * We can try resolve strings and symbols as symbols and
         * promote numbers to flat GC pointers.
         */
        case DBGCVAR_CAT_GC_POINTER_NO_RANGE:
            if (pVar->enmRangeType != DBGCVAR_RANGE_NONE)
                return VERR_PARSE_NO_RANGE_ALLOWED;
            /* fallthru */
        case DBGCVAR_CAT_GC_POINTER:
            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_GC_FLAT:
                case DBGCVAR_TYPE_GC_FAR:
                case DBGCVAR_TYPE_GC_PHYS:
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_HC_FLAT:
                case DBGCVAR_TYPE_HC_FAR:
                case DBGCVAR_TYPE_HC_PHYS:
                    return VERR_PARSE_CONVERSION_FAILED;

                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                {
                    DBGCVAR Var;
                    int rc = dbgcSymbolGet(pDbgc, pVar->u.pszString, DBGCVAR_TYPE_GC_FLAT, &Var);
                    if (VBOX_SUCCESS(rc))
                    {
                        /* deal with range */
                        if (pVar->enmRangeType != DBGCVAR_RANGE_NONE)
                        {
                            Var.enmRangeType = pVar->enmRangeType;
                            Var.u64Range = pVar->u64Range;
                        }
                        else if (pVarDesc->enmCategory == DBGCVAR_CAT_POINTER_NO_RANGE)
                            Var.enmRangeType = DBGCVAR_RANGE_NONE;
                        *pVar = Var;
                        return rc;
                    }
                    break;
                }

                case DBGCVAR_TYPE_NUMBER:
                {
                    RTGCPTR GCPtr = (RTGCPTR)pVar->u.u64Number;
                    pVar->enmType = DBGCVAR_TYPE_GC_FLAT;
                    pVar->u.GCFlat = GCPtr;
                    return VINF_SUCCESS;
                }

                default:
                    break;
            }
            break;

        /*
         * Number with or without a range.
         * Numbers can be resolved from symbols, but we cannot demote a pointer
         * to a number.
         */
        case DBGCVAR_CAT_NUMBER_NO_RANGE:
            if (pVar->enmRangeType != DBGCVAR_RANGE_NONE)
                return VERR_PARSE_NO_RANGE_ALLOWED;
            /* fallthru */
        case DBGCVAR_CAT_NUMBER:
            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_NUMBER:
                    return VINF_SUCCESS;

                case DBGCVAR_TYPE_SYMBOL:
                case DBGCVAR_TYPE_STRING:
                {
                    DBGCVAR Var;
                    int rc = dbgcSymbolGet(pDbgc, pVar->u.pszString, DBGCVAR_TYPE_NUMBER, &Var);
                    if (VBOX_SUCCESS(rc))
                    {
                        *pVar = Var;
                        return rc;
                    }
                    break;
                }
                default:
                    break;
            }
            break;

        /*
         * Strings can easily be made from symbols (and of course strings).
         * We could consider reformatting the addresses and numbers into strings later...
         */
        case DBGCVAR_CAT_STRING:
            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_SYMBOL:
                    pVar->enmType = DBGCVAR_TYPE_STRING;
                    /* fallthru */
                case DBGCVAR_TYPE_STRING:
                    return VINF_SUCCESS;
                default:
                    break;
            }
            break;

        /*
         * Symol is pretty much the same thing as a string (at least until we actually implement it).
         */
        case DBGCVAR_CAT_SYMBOL:
            switch (pVar->enmType)
            {
                case DBGCVAR_TYPE_STRING:
                    pVar->enmType = DBGCVAR_TYPE_SYMBOL;
                    /* fallthru */
                case DBGCVAR_TYPE_SYMBOL:
                    return VINF_SUCCESS;
                default:
                    break;
            }
            break;

        /*
         * Anything else is illegal.
         */
        default:
            AssertMsgFailed(("enmCategory=%d\n", pVar->enmType));
            break;
    }

    return VERR_PARSE_NO_ARGUMENT_MATCH;
}


/**
 * Matches a set of variables with a description set.
 *
 * This is typically used for routine arguments before a call. The effects in
 * addition to the validation, is that some variables might be propagated to
 * other types in order to match the description. The following transformations
 * are supported:
 *      - String reinterpreted as a symbol and resolved to a number or pointer.
 *      - Number to a pointer.
 *      - Pointer to a number.
 * @returns 0 on success with paVars.
 * @returns VBox error code for match errors.
 */
static int dbgcEvalSubMatchVars(PDBGC pDbgc, unsigned cVarsMin, unsigned cVarsMax,
                                PCDBGCVARDESC paVarDescs, unsigned cVarDescs,
                                PDBGCVAR paVars, unsigned cVars)
{
    /*
     * Just do basic min / max checks first.
     */
    if (cVars < cVarsMin)
        return VERR_PARSE_TOO_FEW_ARGUMENTS;
    if (cVars > cVarsMax)
        return VERR_PARSE_TOO_MANY_ARGUMENTS;

    /*
     * Match the descriptors and actual variables.
     */
    PCDBGCVARDESC   pPrevDesc = NULL;
    unsigned        cCurDesc = 0;
    unsigned        iVar = 0;
    unsigned        iVarDesc = 0;
    while (iVar < cVars)
    {
        /* walk the descriptors */
        if (iVarDesc >= cVarDescs)
            return VERR_PARSE_TOO_MANY_ARGUMENTS;
        if (    (    paVarDescs[iVarDesc].fFlags & DBGCVD_FLAGS_DEP_PREV
                &&  &paVarDescs[iVarDesc - 1] != pPrevDesc)
            ||  cCurDesc >= paVarDescs[iVarDesc].cTimesMax)
        {
            iVarDesc++;
            if (iVarDesc >= cVarDescs)
                return VERR_PARSE_TOO_MANY_ARGUMENTS;
            cCurDesc = 0;
        }

        /*
         * Skip thru optional arguments until we find something which matches
         * or can easily be promoted to what the descriptor want.
         */
        for (;;)
        {
            int rc = dbgcEvalSubMatchVar(pDbgc, &paVars[iVar], &paVarDescs[iVarDesc]);
            if (VBOX_SUCCESS(rc))
            {
                paVars[iVar].pDesc = &paVarDescs[iVarDesc];
                cCurDesc++;
                break;
            }

            /* can we advance? */
            if (paVarDescs[iVarDesc].cTimesMin > cCurDesc)
                return VERR_PARSE_ARGUMENT_TYPE_MISMATCH;
            if (++iVarDesc >= cVarDescs)
                return VERR_PARSE_ARGUMENT_TYPE_MISMATCH;
            cCurDesc = 0;
        }

        /* next var */
        iVar++;
    }

    /*
     * Check that the rest of the descriptors are optional.
     */
    while (iVarDesc < cVarDescs)
    {
        if (paVarDescs[iVarDesc].cTimesMin > cCurDesc)
            return VERR_PARSE_TOO_FEW_ARGUMENTS;
        cCurDesc = 0;

        /* next */
        iVarDesc++;
    }

    return 0;
}


/**
 * Evaluates one argument with respect to unary operators.
 *
 * @returns 0 on success. pResult contains the result.
 * @returns VBox error code on parse or other evaluation error.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   pszExpr     The expression string.
 * @param   pResult     Where to store the result of the expression evaluation.
 */
static int dbgcEvalSubUnary(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pResult)
{
    Log2(("dbgcEvalSubUnary: cchExpr=%d pszExpr=%s\n", cchExpr, pszExpr));

    /*
     * The state of the expression is now such that it will start by zero or more
     * unary operators and being followed by an expression of some kind.
     * The expression is either plain or in parenthesis.
     *
     * Being in a lazy, recursive mode today, the parsing is done as simple as possible. :-)
     * ASSUME: unary operators are all of equal precedence.
     */
    int         rc = 0;
    PCDBGCOP    pOp = dbgcOperatorLookup(pDbgc, pszExpr, false, ' ');
    if (pOp)
    {
        /* binary operators means syntax error. */
        if (pOp->fBinary)
            return VERR_PARSE_UNEXPECTED_OPERATOR;

        /*
         * If the next expression (the one following the unary operator) is in a
         * parenthesis a full eval is needed. If not the unary eval will suffice.
         */
        /* calc and strip next expr. */
        char *pszExpr2 = pszExpr + pOp->cchName;
        while (isblank(*pszExpr2))
            pszExpr2++;

        if (!*pszExpr2)
            rc = VERR_PARSE_EMPTY_ARGUMENT;
        else
        {
            DBGCVAR Arg;
            if (*pszExpr2 == '(')
                rc = dbgcEvalSub(pDbgc, pszExpr2, cchExpr - (pszExpr2 - pszExpr), &Arg);
            else
                rc = dbgcEvalSubUnary(pDbgc, pszExpr2, cchExpr - (pszExpr2 - pszExpr), &Arg);
            if (VBOX_SUCCESS(rc))
                rc = pOp->pfnHandlerUnary(pDbgc, &Arg, pResult);
        }
    }
    else
    {
        /*
         * Didn't find any operators, so it we have to check if this can be an
         * function call before assuming numeric or string expression.
         *
         * (ASSUMPTIONS:)
         * A function name only contains alphanumerical chars and it can not start
         * with a numerical character.
         * Immediately following the name is a parenthesis which must over
         * the remaining part of the expression.
         */
        bool    fExternal = *pszExpr == '.';
        char   *pszFun    = fExternal ? pszExpr + 1 : pszExpr;
        char   *pszFunEnd = NULL;
        if (pszExpr[cchExpr - 1] == ')' && isalpha(*pszFun))
        {
            pszFunEnd = pszExpr + 1;
            while (*pszFunEnd != '(' && isalnum(*pszFunEnd))
                pszFunEnd++;
            if (*pszFunEnd != '(')
                pszFunEnd = NULL;
        }

        if (pszFunEnd)
        {
            /*
             * Ok, it's a function call.
             */
            if (fExternal)
                pszExpr++, cchExpr--;
            PCDBGCCMD pFun = dbgcRoutineLookup(pDbgc, pszExpr, pszFunEnd - pszExpr, fExternal);
            if (!pFun)
                return VERR_PARSE_FUNCTION_NOT_FOUND;
            if (!pFun->pResultDesc)
                return VERR_PARSE_NOT_A_FUNCTION;

            /*
             * Parse the expression in parenthesis.
             */
            cchExpr -= pszFunEnd - pszExpr;
            pszExpr = pszFunEnd;
            /** @todo implement multiple arguments. */
            DBGCVAR     Arg;
            rc = dbgcEvalSub(pDbgc, pszExpr, cchExpr, &Arg);
            if (!rc)
            {
                rc = dbgcEvalSubMatchVars(pDbgc, pFun->cArgsMin, pFun->cArgsMax, pFun->paArgDescs, pFun->cArgDescs, &Arg, 1);
                if (!rc)
                    rc = pFun->pfnHandler(pFun, &pDbgc->CmdHlp, pDbgc->pVM, &Arg, 1, pResult);
            }
            else if (rc == VERR_PARSE_EMPTY_ARGUMENT && pFun->cArgsMin == 0)
                rc = pFun->pfnHandler(pFun, &pDbgc->CmdHlp, pDbgc->pVM, NULL, 0, pResult);
        }
        else
        {
            /*
             * Didn't find any operators, so it must be a plain expression.
             * This might be numeric or a string expression.
             */
            char ch  = pszExpr[0];
            char ch2 = pszExpr[1];
            if (ch == '0' && (ch2 == 'x' || ch2 == 'X'))
                rc = dbgcEvalSubNum(pszExpr + 2, 16, pResult);
            else if (ch == '0' && (ch2 == 'i' || ch2 == 'i'))
                rc = dbgcEvalSubNum(pszExpr + 2, 10, pResult);
            else if (ch == '0' && (ch2 == 't' || ch2 == 'T'))
                rc = dbgcEvalSubNum(pszExpr + 2, 8, pResult);
            /// @todo 0b doesn't work as a binary prefix, we confuse it with 0bf8:0123 and stuff.
            //else if (ch == '0' && (ch2 == 'b' || ch2 == 'b'))
            //    rc = dbgcEvalSubNum(pszExpr + 2, 2, pResult);
            else
            {
                /*
                 * Hexadecimal number or a string?
                 */
                char *psz = pszExpr;
                while (isxdigit(*psz))
                    psz++;
                if (!*psz)
                    rc = dbgcEvalSubNum(pszExpr, 16, pResult);
                else if ((*psz == 'h' || *psz == 'H') && !psz[1])
                {
                    *psz = '\0';
                    rc = dbgcEvalSubNum(pszExpr, 16, pResult);
                }
                else
                    rc = dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);
            }
        }
    }

    return rc;
}


/**
 * Evaluates one argument.
 *
 * @returns 0 on success. pResult contains the result.
 * @returns VBox error code on parse or other evaluation error.
 *
 * @param   pDbgc       Debugger console instance data.
 * @param   pszExpr     The expression string.
 * @param   pResult     Where to store the result of the expression evaluation.
 */
static int dbgcEvalSub(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pResult)
{
    Log2(("dbgcEvalSub: cchExpr=%d pszExpr=%s\n", cchExpr, pszExpr));
    /*
     * First we need to remove blanks in both ends.
     * ASSUMES: There is no quoting unless the entire expression is a string.
     */

    /* stripping. */
    while (cchExpr > 0 && isblank(pszExpr[cchExpr - 1]))
        pszExpr[--cchExpr] = '\0';
    while (isblank(*pszExpr))
        pszExpr++, cchExpr--;
    if (!*pszExpr)
        return VERR_PARSE_EMPTY_ARGUMENT;

    /* it there is any kind of quoting in the expression, it's string meat. */
    if (strpbrk(pszExpr, "\"'`"))
        return dbgcEvalSubString(pDbgc, pszExpr, cchExpr, pResult);

    /*
     * Check if there are any parenthesis which needs removing.
     */
    if (pszExpr[0] == '(' && pszExpr[cchExpr - 1] == ')')
    {
        do
        {
            unsigned cPar = 1;
            char    *psz = pszExpr + 1;
            char     ch;
            while ((ch = *psz) != '\0')
            {
                if (ch == '(')
                    cPar++;
                else if (ch == ')')
                {
                    if (cPar <= 0)
                        return VERR_PARSE_UNBALANCED_PARENTHESIS;
                    cPar--;
                    if (cPar == 0 && psz[1]) /* If not at end, there's nothing to do. */
                        break;
                }
                /* next */
                psz++;
            }
            if (ch)
                break;

            /* remove the parenthesis. */
            pszExpr++;
            cchExpr -= 2;
            pszExpr[cchExpr] = '\0';

            /* strip blanks. */
            while (cchExpr > 0 && isblank(pszExpr[cchExpr - 1]))
                pszExpr[--cchExpr] = '\0';
            while (isblank(*pszExpr))
                pszExpr++, cchExpr--;
            if (!*pszExpr)
                return VERR_PARSE_EMPTY_ARGUMENT;
        } while (pszExpr[0] == '(' && pszExpr[cchExpr - 1] == ')');
    }

    /* tabs to spaces. */
    char *psz = pszExpr;
    while ((psz = strchr(psz, '\t')) != NULL)
        *psz = ' ';

    /*
     * Now, we need to look for the binary operator with the lowest precedence.
     *
     * If there are no operators we're left with a simple expression which we
     * evaluate with respect to unary operators
     */
    char       *pszOpSplit = NULL;
    PCDBGCOP    pOpSplit = NULL;
    unsigned    cBinaryOps = 0;
    unsigned    cPar = 0;
    char        ch;
    char        chPrev = ' ';
    bool        fBinary = false;
    psz = pszExpr;

    while ((ch = *psz) != '\0')
    {
        //Log2(("ch=%c cPar=%d fBinary=%d\n", ch, cPar, fBinary));
        /*
         * Parenthesis.
         */
        if (ch == '(')
        {
            cPar++;
            fBinary = false;
        }
        else if (ch == ')')
        {
            if (cPar <= 0)
                return VERR_PARSE_UNBALANCED_PARENTHESIS;
            cPar--;
            fBinary = true;
        }
        /*
         * Potential operator.
         */
        else if (cPar == 0 && !isblank(ch))
        {
            PCDBGCOP pOp = dbgcIsOpChar(ch)
                         ? dbgcOperatorLookup(pDbgc, psz, fBinary, chPrev)
                         : NULL;
            if (pOp)
            {
                /* If not the right kind of operator we've got a syntax error. */
                if (pOp->fBinary != fBinary)
                    return VERR_PARSE_UNEXPECTED_OPERATOR;

                /*
                 * Update the parse state and skip the operator.
                 */
                if (!pOpSplit)
                {
                    pOpSplit = pOp;
                    pszOpSplit = psz;
                    cBinaryOps = fBinary;
                }
                else if (fBinary)
                {
                    cBinaryOps++;
                    if (pOp->iPrecedence >= pOpSplit->iPrecedence)
                    {
                        pOpSplit = pOp;
                        pszOpSplit = psz;
                    }
                }

                psz += pOp->cchName - 1;
                fBinary = false;
            }
            else
                fBinary = true;
        }

        /* next */
        psz++;
        chPrev = ch;
    } /* parse loop. */


    /*
     * Either we found an operator to divide the expression by
     * or we didn't find any. In the first case it's divide and
     * conquer. In the latter it's a single expression which
     * needs dealing with its unary operators if any.
     */
    int rc;
    if (    cBinaryOps
        &&  pOpSplit->fBinary)
    {
        /* process 1st sub expression. */
        *pszOpSplit = '\0';
        DBGCVAR     Arg1;
        rc = dbgcEvalSub(pDbgc, pszExpr, pszOpSplit - pszExpr, &Arg1);
        if (VBOX_SUCCESS(rc))
        {
            /* process 2nd sub expression. */
            char       *psz2 = pszOpSplit + pOpSplit->cchName;
            DBGCVAR     Arg2;
            rc = dbgcEvalSub(pDbgc, psz2, cchExpr - (psz2 - pszExpr), &Arg2);
            if (VBOX_SUCCESS(rc))
                /* apply the operator. */
                rc = pOpSplit->pfnHandlerBinary(pDbgc, &Arg1, &Arg2, pResult);
        }
    }
    else if (cBinaryOps)
    {
        /* process sub expression. */
        pszOpSplit += pOpSplit->cchName;
        DBGCVAR     Arg;
        rc = dbgcEvalSub(pDbgc, pszOpSplit, cchExpr - (pszOpSplit - pszExpr), &Arg);
        if (VBOX_SUCCESS(rc))
            /* apply the operator. */
            rc = pOpSplit->pfnHandlerUnary(pDbgc, &Arg, pResult);
    }
    else
        /* plain expression or using unary operators perhaps with paratheses. */
        rc = dbgcEvalSubUnary(pDbgc, pszExpr, cchExpr, pResult);

    return rc;
}


/**
 * Parses the arguments of one command.
 *
 * @returns 0 on success.
 * @returns VBox error code on parse error with *pcArg containing the argument causing trouble.
 * @param   pDbgc       Debugger console instance data.
 * @param   pCmd        Pointer to the command descriptor.
 * @param   pszArg      Pointer to the arguments to parse.
 * @param   paArgs      Where to store the parsed arguments.
 * @param   cArgs       Size of the paArgs array.
 * @param   pcArgs      Where to store the number of arguments.
 *                      In the event of an error this is used to store the index of the offending argument.
 */
static int dbgcProcessArguments(PDBGC pDbgc, PCDBGCCMD pCmd, char *pszArgs, PDBGCVAR paArgs, unsigned cArgs, unsigned *pcArgs)
{
    Log2(("dbgcProcessArguments: pCmd=%s pszArgs='%s'\n", pCmd->pszCmd, pszArgs));
    /*
     * Check if we have any argument and if the command takes any.
     */
    *pcArgs = 0;
    /* strip leading blanks. */
    while (*pszArgs && isblank(*pszArgs))
        pszArgs++;
    if (!*pszArgs)
    {
        if (!pCmd->cArgsMin)
            return 0;
        return VERR_PARSE_TOO_FEW_ARGUMENTS;
    }
    /** @todo fixme - foo() doesn't work. */
    if (!pCmd->cArgsMax)
        return VERR_PARSE_TOO_MANY_ARGUMENTS;

    /*
     * This is a hack, it's "temporary" and should go away "when" the parser is
     * modified to match arguments while parsing.
     */
    if (    pCmd->cArgsMax == 1
        &&  pCmd->cArgsMin == 1
        &&  pCmd->cArgDescs == 1
        &&  pCmd->paArgDescs[0].enmCategory == DBGCVAR_CAT_STRING
        &&  cArgs >= 1)
    {
        *pcArgs = 1;
        RTStrStripR(pszArgs);
        return dbgcEvalSubString(pDbgc, pszArgs, strlen(pszArgs), &paArgs[0]);
    }


    /*
     * The parse loop.
     */
    PDBGCVAR        pArg0 = &paArgs[0];
    PDBGCVAR        pArg = pArg0;
    *pcArgs = 0;
    do
    {
        /*
         * Can we have another argument?
         */
        if (*pcArgs >= pCmd->cArgsMax)
            return VERR_PARSE_TOO_MANY_ARGUMENTS;
        if (pArg >= &paArgs[cArgs])
            return VERR_PARSE_ARGUMENT_OVERFLOW;

        /*
         * Find the end of the argument.
         */
        int     cPar    = 0;
        char    chQuote = '\0';
        char   *pszEnd  = NULL;
        char   *psz     = pszArgs;
        char    ch;
        bool    fBinary = false;
        for (;;)
        {
            /*
             * Check for the end.
             */
            if ((ch = *psz) == '\0')
            {
                if (chQuote)
                    return VERR_PARSE_UNBALANCED_QUOTE;
                if (cPar)
                    return VERR_PARSE_UNBALANCED_PARENTHESIS;
                pszEnd = psz;
                break;
            }
            /*
             * When quoted we ignore everything but the quotation char.
             * We use the REXX way of escaping the quotation char, i.e. double occurence.
             */
            else if (ch == '\'' || ch == '"' || ch == '`')
            {
                if (chQuote)
                {
                    /* end quote? */
                    if (ch == chQuote)
                    {
                        if (psz[1] == ch)
                            psz++;          /* skip the escaped quote char */
                        else
                            chQuote = '\0'; /* end of quoted string. */
                    }
                }
                else
                    chQuote = ch;           /* open new quote */
            }
            /*
             * Parenthesis can of course be nested.
             */
            else if (ch == '(')
            {
                cPar++;
                fBinary = false;
            }
            else if (ch == ')')
            {
                if (!cPar)
                    return VERR_PARSE_UNBALANCED_PARENTHESIS;
                cPar--;
                fBinary = true;
            }
            else if (!chQuote && !cPar)
            {
                /*
                 * Encountering blanks may mean the end of it all. A binary operator
                 * will force continued parsing.
                 */
                if (isblank(*psz))
                {
                    pszEnd = psz++;         /* just in case. */
                    while (isblank(*psz))
                        psz++;
                    PCDBGCOP pOp = dbgcOperatorLookup(pDbgc, psz, fBinary, ' ');
                    if (!pOp || pOp->fBinary != fBinary)
                        break;              /* the end. */
                    psz += pOp->cchName;
                    while (isblank(*psz))   /* skip blanks so we don't get here again */
                        psz++;
                    fBinary = false;
                    continue;
                }

                /*
                 * Look for operators without a space up front.
                 */
                if (dbgcIsOpChar(*psz))
                {
                    PCDBGCOP pOp = dbgcOperatorLookup(pDbgc, psz, fBinary, ' ');
                    if (pOp)
                    {
                        if (pOp->fBinary != fBinary)
                        {
                            pszEnd = psz;
                            /** @todo this is a parsing error really. */
                            break;              /* the end. */
                        }
                        psz += pOp->cchName;
                        while (isblank(*psz))   /* skip blanks so we don't get here again */
                            psz++;
                        fBinary = false;
                        continue;
                    }
                }
                fBinary = true;
            }

            /* next char */
            psz++;
        }
        *pszEnd = '\0';
        /* (psz = next char to process) */

        /*
         * Parse and evaluate the argument.
         */
        int rc = dbgcEvalSub(pDbgc, pszArgs, strlen(pszArgs), pArg);
        if (VBOX_FAILURE(rc))
            return rc;

        /*
         * Next.
         */
        pArg++;
        (*pcArgs)++;
        pszArgs = psz;
        while (*pszArgs && isblank(*pszArgs))
            pszArgs++;
    } while (*pszArgs);

    /*
     * Match the arguments.
     */
    return dbgcEvalSubMatchVars(pDbgc, pCmd->cArgsMin, pCmd->cArgsMax, pCmd->paArgDescs, pCmd->cArgDescs, pArg0, pArg - pArg0);
}


/**
 * Process one command.
 *
 * @returns VBox status code. Any error indicates the termination of the console session.
 * @param   pDbgc   Debugger console instance data.
 * @param   pszCmd  Pointer to the command.
 * @param   cchCmd  Length of the command.
 */
static int dbgcProcessCommand(PDBGC pDbgc, char *pszCmd, size_t cchCmd)
{
    char *pszCmdInput = pszCmd;

    /*
     * Skip blanks.
     */
    while (isblank(*pszCmd))
        pszCmd++, cchCmd--;

    /* external command? */
    bool fExternal = *pszCmd == '.';
    if (fExternal)
        pszCmd++, cchCmd--;

    /*
     * Find arguments.
     */
    char *pszArgs = pszCmd;
    while (isalnum(*pszArgs))
        pszArgs++;
    if (*pszArgs && (!isblank(*pszArgs) || pszArgs == pszCmd))
    {
        pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "Syntax error in command '%s'!\n", pszCmdInput);
        return 0;
    }

    /*
     * Find the command.
     */
    PCDBGCCMD pCmd = dbgcRoutineLookup(pDbgc, pszCmd, pszArgs - pszCmd, fExternal);
    if (!pCmd || (pCmd->fFlags & DBGCCMD_FLAGS_FUNCTION))
        return pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "Unknown command '%s'!\n", pszCmdInput);

    /*
     * Parse arguments (if any).
     */
    unsigned    cArgs;
    int rc = dbgcProcessArguments(pDbgc, pCmd, pszArgs, &pDbgc->aArgs[pDbgc->iArg], ELEMENTS(pDbgc->aArgs) - pDbgc->iArg, &cArgs);

    /*
     * Execute the command.
     */
    if (!rc)
    {
        rc = pCmd->pfnHandler(pCmd, &pDbgc->CmdHlp, pDbgc->pVM, &pDbgc->aArgs[0], cArgs, NULL);
    }
    else
    {
        /* report parse / eval error. */
        switch (rc)
        {
            case VERR_PARSE_TOO_FEW_ARGUMENTS:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Too few arguments. Minimum is %d for command '%s'.\n", pCmd->cArgsMin, pCmd->pszCmd);
                break;
            case VERR_PARSE_TOO_MANY_ARGUMENTS:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Too many arguments. Maximum is %d for command '%s'.\n", pCmd->cArgsMax, pCmd->pszCmd);
                break;
            case VERR_PARSE_ARGUMENT_OVERFLOW:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Too many arguments.\n");
                break;
            case VERR_PARSE_UNBALANCED_QUOTE:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Unbalanced quote (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_UNBALANCED_PARENTHESIS:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Unbalanced parenthesis (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_EMPTY_ARGUMENT:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: An argument or subargument contains nothing useful (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_UNEXPECTED_OPERATOR:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Invalid operator usage (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_INVALID_NUMBER:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Syntax error: Ivalid numeric value (argument %d). If a string was the intention, then quote it.\n", cArgs);
                break;
            case VERR_PARSE_NUMBER_TOO_BIG:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Numeric overflow (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_INVALID_OPERATION:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Invalid operation attempted (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_FUNCTION_NOT_FOUND:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Function not found (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_NOT_A_FUNCTION:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: The function specified is not a function (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_NO_MEMORY:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Out memory in the regular heap! Expect odd stuff to happen...\n", cArgs);
                break;
            case VERR_PARSE_INCORRECT_ARG_TYPE:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Incorrect argument type (argument %d?).\n", cArgs);
                break;
            case VERR_PARSE_VARIABLE_NOT_FOUND:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: An undefined variable was referenced (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_CONVERSION_FAILED:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: A conversion between two types failed (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_NOT_IMPLEMENTED:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: You hit a debugger feature which isn't implemented yet (argument %d).\n", cArgs);
                break;
            case VERR_PARSE_BAD_RESULT_TYPE:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Couldn't satisfy a request for a specific result type (argument %d). (Usually applies to symbols)\n", cArgs);
                break;
            case VERR_PARSE_WRITEONLY_SYMBOL:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Cannot get symbol, it's set only (argument %d).\n", cArgs);
                break;

            default:
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                    "Error: Unknown error %d!\n", rc);
                return rc;
        }

        /*
         * Parse errors are non fatal.
         */
        if (rc >= VERR_PARSE_FIRST && rc < VERR_PARSE_LAST)
            rc = 0;
    }

    return rc;
}


/**
 * Process all commands current in the buffer.
 *
 * @returns VBox status code. Any error indicates the termination of the console session.
 * @param   pDbgc   Debugger console instance data.
 */
static int dbgcProcessCommands(PDBGC pDbgc)
{
    int rc = 0;
    while (pDbgc->cInputLines)
    {
        /*
         * Empty the log buffer if we're hooking the log.
         */
        if (pDbgc->fLog)
        {
            rc = dbgcProcessLog(pDbgc);
            if (VBOX_FAILURE(rc))
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
        rc = dbgcProcessCommand(pDbgc, &pDbgc->achScratch[0], psz - &pDbgc->achScratch[0] - 1);
        if (rc)
            break;
    }

    return rc;
}


/**
 * Reads input, parses it and executes commands on '\n'.
 *
 * @returns VBox status.
 * @param   pDbgc   Debugger console instance data.
 */
static int dbgcProcessInput(PDBGC pDbgc)
{
    /*
     * We know there's input ready, so let's read it first.
     */
    int rc = dbgcInputRead(pDbgc);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Now execute any ready commands.
     */
    if (pDbgc->cInputLines)
    {
        /** @todo this fReady stuff is broken. */
        pDbgc->fReady = false;
        rc = dbgcProcessCommands(pDbgc);
        if (VBOX_SUCCESS(rc) && rc != VWRN_DBGC_CMD_PENDING)
            pDbgc->fReady = true;
        if (    VBOX_SUCCESS(rc)
            &&  pDbgc->iRead == pDbgc->iWrite
            &&  pDbgc->fReady)
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "VBoxDbg> ");
    }

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
        if (VBOX_FAILURE(rc))
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
            if (VBOX_SUCCESS(rc))
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
            if (VBOX_SUCCESS(rc))
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
            if (VBOX_SUCCESS(rc) && DBGFR3IsHalted(pDbgc->pVM))
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
            if (VBOX_SUCCESS(rc))
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
            if (VBOX_SUCCESS(rc))
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
            if (VBOX_SUCCESS(rc) && pEvent->u.Src.pszMessage && *pEvent->u.Src.pszMessage)
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                         "Message:  %s\n",
                                             pEvent->u.Src.pszMessage);
            if (VBOX_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }


        case DBGFEVENT_INVALID_COMMAND:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf/dbgc error: Invalid command event!\n");
            fPrintPrompt = !pDbgc->fReady;
            break;
        }

        case DBGFEVENT_TERMINATING:
        {
            pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\nVM is terminating!\n");
            rc = VERR_GENERAL_FAILURE;
            break;
        }


        default:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf/dbgc error: Unknown event %d!\n", pEvent->enmType);
            fPrintPrompt = !pDbgc->fReady;
            break;
        }
    }

    /*
     * Prompt, anyone?
     */
    if (fPrintPrompt && VBOX_SUCCESS(rc))
    {
        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "VBoxDbg> ");
    }

    return rc;
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
DBGDECL(int)    DBGCCreate(PVM pVM, PDBGCBACK pBack, unsigned fFlags)
{
    /*
     * Validate input.
     */
    AssertReturn(VALID_PTR(pVM), VERR_INVALID_PARAMETER);
    AssertReturn(VALID_PTR(pBack), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!fFlags, ("%#x", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize instance data
     */
    PDBGC   pDbgc = (PDBGC)RTMemAllocZ(sizeof(*pDbgc));
    if (!pDbgc)
        return VERR_NO_MEMORY;

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
    pDbgc->pBack            = pBack;
    pDbgc->pVM              = NULL;
    pDbgc->pszEmulation     = "CodeView/WinDbg";
    pDbgc->paEmulationCmds  = &g_aCmdsCodeView[0];
    pDbgc->cEmulationCmds   = RT_ELEMENTS(g_aCmdsCodeView);
    //pDbgc->fLog             = false;
    pDbgc->fRegCtxGuest     = true;
    pDbgc->fRegTerse        = true;
    //pDbgc->DisasmPos        = {0};
    //pDbgc->SourcePos        = {0};
    //pDbgc->DumpPos          = {0};
    //pDbgc->cbDumpElement    = 0;
    //pDbgc->cVars            = 0;
    //pDbgc->paVars           = NULL;
    //pDbgc->pFirstBp         = NULL;
    //pDbgc->uInputZero       = 0;
    //pDbgc->iRead            = 0;
    //pDbgc->iWrite           = 0;
    //pDbgc->cInputLines      = 0;
    //pDbgc->fInputOverflow   = false;
    pDbgc->fReady           = true;
    pDbgc->pszScratch       = &pDbgc->achScratch[0];
    //pDbgc->iArg             = 0;
    //pDbgc->rcOutput         = 0;

    dbgcInitOpCharBitMap();

    /*
     * Print welcome message.
     */
    int rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
        "Welcome to the VirtualBox Debugger!\n");
    if (VBOX_FAILURE(rc))
        goto l_failure;

    /*
     * Attach to the VM.
     */
    rc = DBGFR3Attach(pVM);
    if (VBOX_FAILURE(rc))
    {
        rc = pDbgc->CmdHlp.pfnVBoxError(&pDbgc->CmdHlp, rc, "When trying to attach to VM %p\n", pDbgc->pVM);
        goto l_failure;
    }
    pDbgc->pVM = pVM;

    /*
     * Print commandline and auto select result.
     */
    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
        "Current VM is %08x\n" /** @todo get and print the VM name! */
        "VBoxDbg> ",
        pDbgc->pVM);
    if (VBOX_FAILURE(rc))
        goto l_failure;

    /*
     * Main Debugger Loop.
     *
     * This loop will either block on waiting for input or on waiting on
     * debug events. If we're forwarding the log we cannot wait for long
     * before we must flush the log.
     */
    for (rc = 0;;)
    {
        if (pDbgc->pVM && DBGFR3CanWait(pDbgc->pVM))
        {
            /*
             * Wait for a debug event.
             */
            PCDBGFEVENT pEvent;
            rc = DBGFR3EventWait(pDbgc->pVM, pDbgc->fLog ? 1 : 32, &pEvent);
            if (VBOX_SUCCESS(rc))
            {
                rc = dbgcProcessEvent(pDbgc, pEvent);
                if (VBOX_FAILURE(rc))
                    break;
            }
            else if (rc != VERR_TIMEOUT)
                break;

            /*
             * Check for input.
             */
            if (pBack->pfnInput(pDbgc->pBack, 0))
            {
                rc = dbgcProcessInput(pDbgc);
                if (VBOX_FAILURE(rc))
                    break;
            }
        }
        else
        {
            /*
             * Wait for input. If Logging is enabled we'll only wait very briefly.
             */
            if (pBack->pfnInput(pDbgc->pBack, pDbgc->fLog ? 1 : 1000))
            {
                rc = dbgcProcessInput(pDbgc);
                if (VBOX_FAILURE(rc))
                    break;
            }
        }

        /*
         * Forward log output.
         */
        if (pDbgc->fLog)
        {
            rc = dbgcProcessLog(pDbgc);
            if (VBOX_FAILURE(rc))
                break;
        }
    }


l_failure:
    /*
     * Cleanup console debugger session.
     */
    /* Disable log hook. */
    if (pDbgc->fLog)
    {

    }

    /* Detach from the VM. */
    if (pDbgc->pVM)
        DBGFR3Detach(pDbgc->pVM);

    /* finally, free the instance memory. */
    RTMemFree(pDbgc);

    return rc;
}



/**
 * Register one or more external commands.
 *
 * @returns VBox status.
 * @param   paCommands      Pointer to an array of command descriptors.
 *                          The commands must be unique. It's not possible
 *                          to register the same commands more than once.
 * @param   cCommands       Number of commands.
 */
DBGDECL(int)    DBGCRegisterCommands(PCDBGCCMD paCommands, unsigned cCommands)
{
    /*
     * Lock the list.
     */
    DBGCEXTCMDS_LOCK_WR();
    PDBGCEXTCMDS pCur = g_pExtCmdsHead;
    while (pCur)
    {
        if (paCommands == pCur->paCmds)
        {
            DBGCEXTCMDS_UNLOCK_WR();
            AssertMsgFailed(("Attempt at re-registering %d command(s)!\n", cCommands));
            return VWRN_DBGC_ALREADY_REGISTERED;
        }
        pCur = pCur->pNext;
    }

    /*
     * Allocate new chunk.
     */
    int rc = 0;
    pCur = (PDBGCEXTCMDS)RTMemAlloc(sizeof(*pCur));
    if (pCur)
    {
        pCur->cCmds  = cCommands;
        pCur->paCmds = paCommands;
        pCur->pNext = g_pExtCmdsHead;
        g_pExtCmdsHead = pCur;
    }
    else
        rc = VERR_NO_MEMORY;
    DBGCEXTCMDS_UNLOCK_WR();

    return rc;
}


/**
 * Deregister one or more external commands previously registered by
 * DBGCRegisterCommands().
 *
 * @returns VBox status.
 * @param   paCommands      Pointer to an array of command descriptors
 *                          as given to DBGCRegisterCommands().
 * @param   cCommands       Number of commands.
 */
DBGDECL(int)    DBGCDeregisterCommands(PCDBGCCMD paCommands, unsigned cCommands)
{
    /*
     * Lock the list.
     */
    DBGCEXTCMDS_LOCK_WR();
    PDBGCEXTCMDS pPrev = NULL;
    PDBGCEXTCMDS pCur = g_pExtCmdsHead;
    while (pCur)
    {
        if (paCommands == pCur->paCmds)
        {
            if (pPrev)
                pPrev->pNext = pCur->pNext;
            else
                g_pExtCmdsHead = pCur->pNext;
            DBGCEXTCMDS_UNLOCK_WR();

            RTMemFree(pCur);
            return VINF_SUCCESS;
        }
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    DBGCEXTCMDS_UNLOCK_WR();

    NOREF(cCommands);
    return VERR_DBGC_COMMANDS_NOT_REGISTERED;
}

