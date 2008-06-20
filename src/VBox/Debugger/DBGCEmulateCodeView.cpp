/** $Id$ */
/** @file
 * DBGC - Debugger Console, CodeView / WinDbg Emulation.
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
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/cpum.h>
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
static DECLCALLBACK(int) dbgcCmdBrkAccess(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkClear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkDisable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkEnable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkList(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdBrkREM(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpIDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageDir(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageDirBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageTable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpPageTableBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdDumpTSS(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdGo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdListSource(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdListNear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdMemoryInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdReg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRegGuest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRegHyper(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRegTerse(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdSearchMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdSearchMemType(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdStack(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdTrace(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdUnassemble(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
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


/** 'br' arguments. */
static const DBGCVARDESC    g_aArgBrkREM[] =
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
    {  0,           1,          DBGCVAR_CAT_OPTION,     0,                              "-b",           "Byte string." },
    {  0,           1,          DBGCVAR_CAT_OPTION,     0,                              "-w",           "Word string." },
    {  0,           1,          DBGCVAR_CAT_OPTION,     0,                              "-d",           "DWord string." },
    {  0,           1,          DBGCVAR_CAT_OPTION,     0,                              "-q",           "QWord string." },
    {  0,           1,          DBGCVAR_CAT_OPTION,     0,                              "-a",           "ASCII string." },
    {  0,           1,          DBGCVAR_CAT_OPTION,     0,                              "-u",           "Unicode string." },
    {  0,           1,          DBGCVAR_CAT_OPTION_NUMBER, 0,                           "-n <Hits>",    "Maximum number of hits." },
    {  0,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "range",        "Register to show or set." },
    {  0,           ~0,         DBGCVAR_CAT_ANY,        0,                              "pattern",      "Pattern to search for." },
};


/** 's?' arguments. */
static const DBGCVARDESC    g_aArgSearchMemType[] =
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
const DBGCCMD    g_aCmdsCodeView[] =
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
    { "br",         1,        4,        &g_aArgBrkREM[0],   RT_ELEMENTS(g_aArgBrkREM),     NULL,               0,       dbgcCmdBrkREM,      "<address> [passes [max passes]] [cmds]",
                                                                                                                                                                    "Sets a recompiler specific breakpoint." },
    { "d",          0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory using last element size." },
    { "da",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory as ascii string." },
    { "db",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in bytes." },
    { "dd",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in double words." },
    { "dg",         0,       ~0,        &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),     NULL,               0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the global descriptor table (GDT)." },
    { "dga",        0,       ~0,        &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),     NULL,               0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the global descriptor table (GDT) including not-present entries." },
    { "di",         0,       ~0,        &g_aArgDumpIDT[0],  RT_ELEMENTS(g_aArgDumpIDT),    NULL,               0,       dbgcCmdDumpIDT,     "[int [..]]",           "Dump the interrupt descriptor table (IDT)." },
    { "dia",        0,       ~0,        &g_aArgDumpIDT[0],  RT_ELEMENTS(g_aArgDumpIDT),    NULL,               0,       dbgcCmdDumpIDT,     "[int [..]]",           "Dump the interrupt descriptor table (IDT) including not-present entries." },
    { "dl",         0,       ~0,        &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),     NULL,               0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the local descriptor table (LDT)." },
    { "dla",        0,       ~0,        &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),     NULL,               0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the local descriptor table (LDT) including not-present entries." },
    { "dpd",        0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),     NULL,               0,       dbgcCmdDumpPageDir, "[addr] [index]",       "Dumps page directory entries of the default context." },
    { "dpda",       0,        1,        &g_aArgDumpPDAddr[0],RT_ELEMENTS(g_aArgDumpPDAddr),NULL,               0,       dbgcCmdDumpPageDir, "[addr]",               "Dumps specified page directory." },
    { "dpdb",       1,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),     NULL,               0,       dbgcCmdDumpPageDirBoth, "[addr] [index]",   "Dumps page directory entries of the guest and the hypervisor. " },
    { "dpdg",       0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),     NULL,               0,       dbgcCmdDumpPageDir, "[addr] [index]",       "Dumps page directory entries of the guest." },
    { "dpdh",       0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),     NULL,               0,       dbgcCmdDumpPageDir, "[addr] [index]",       "Dumps page directory entries of the hypervisor. " },
    { "dpt",        1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),     NULL,               0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the default context." },
    { "dpta",       1,        1,        &g_aArgDumpPTAddr[0],RT_ELEMENTS(g_aArgDumpPTAddr), NULL,              0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps specified page table." },
    { "dptb",       1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),     NULL,               0,       dbgcCmdDumpPageTableBoth,"<addr>",          "Dumps page table entries of the guest and the hypervisor." },
    { "dptg",       1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),     NULL,               0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the guest." },
    { "dpth",       1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),     NULL,               0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the hypervisor." },
    { "dq",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),    NULL,               0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in quad words." },
    { "dt",         0,        1,        &g_aArgDumpTSS[0],  RT_ELEMENTS(g_aArgDumpTSS),    NULL,               0,       dbgcCmdDumpTSS,     "[tss|tss:ign|addr]",   "Dump the task state segment (TSS)." },
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
    { "s",          0,       ~0,        &g_aArgSearchMem[0], RT_ELEMENTS(g_aArgSearchMem),  NULL,              0,       dbgcCmdSearchMem,   "[options] <range> <pattern>",  "Continue last search." },
    { "sa",         2,       ~0,        &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),  NULL,      0,       dbgcCmdSearchMemType,   "<range> <pattern>",    "Search memory for an ascii string." },
    { "sb",         2,       ~0,        &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),  NULL,      0,       dbgcCmdSearchMemType,   "<range> <pattern>",    "Search memory for one or more bytes." },
    { "sd",         2,       ~0,        &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),  NULL,      0,       dbgcCmdSearchMemType,   "<range> <pattern>",    "Search memory for one or more double words." },
    { "sq",         2,       ~0,        &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),  NULL,      0,       dbgcCmdSearchMemType,   "<range> <pattern>",    "Search memory for one or more quad words." },
    { "su",         2,       ~0,        &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),  NULL,      0,       dbgcCmdSearchMemType,   "<range> <pattern>",    "Search memory for an unicode string." },
    { "sw",         2,       ~0,        &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),  NULL,      0,       dbgcCmdSearchMemType,   "<range> <pattern>",    "Search memory for one or more words." },
    { "t",          0,        0,        NULL,               0,                             NULL,               0,       dbgcCmdTrace,       "",                     "Instruction trace (step into)." },
    { "u",          0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble),NULL,               0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble." },
};

/** The number of commands in the CodeView/WinDbg emulation. */
const unsigned g_cCmdsCodeView = RT_ELEMENTS(g_aCmdsCodeView);



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
            PCPUMCTX pCtx;
            int rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
            AssertRC(rc);

            if (    pDbgc->fRegCtxGuest
                &&  CPUMIsGuestIn64BitCodeEx(pCtx))
            {
                pDbgc->DisasmPos.enmType    = DBGCVAR_TYPE_GC_FLAT;
                pDbgc->SourcePos.u.GCFlat   = CPUMGetGuestRIP(pVM);
            }
            else
            {
                pDbgc->DisasmPos.enmType     = DBGCVAR_TYPE_GC_FAR;
                pDbgc->SourcePos.u.GCFar.off = pDbgc->fRegCtxGuest ? CPUMGetGuestEIP(pVM) : CPUMGetHyperEIP(pVM);
                pDbgc->SourcePos.u.GCFar.sel = pDbgc->fRegCtxGuest ? CPUMGetGuestCS(pVM)  : CPUMGetHyperCS(pVM);
            }

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
                return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: failed to evaluate '%%(%Dv)' -> %Vrc .\n", &pDbgc->DisasmPos, rc);
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
            { "ng", "pl", X86_EFL_SF },
            { "zr", "nz", X86_EFL_ZF },
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
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %sss=%04x                %seflags=%08x\n",
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 34 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                    pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, efl);
            }
            else
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
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
            {
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sds={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%ses={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sfs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sgs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sss={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%scr0=%016RX64 %scr2=%016RX64 %scr3=%016RX64 %scr4=%016RX64\n"
                    "%sdr0=%016RX64 %sdr1=%016RX64 %sdr2=%016RX64 %sdr3=%016RX64\n"
                    "%sdr4=%016RX64 %sdr5=%016RX64 %sdr6=%016RX64 %sdr7=%016RX64\n"
                    "%sgdtr=%016RX64:%04x  %sidtr=%016RX64:%04x  %seflags=%08x\n"
                    "%sldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%str  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%sSysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
                    ,
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u,
                    pszPrefix, (RTSEL)pCtxCore->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, 
                    pszPrefix, (RTSEL)pCtxCore->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, 
                    pszPrefix, (RTSEL)pCtxCore->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, 
                    pszPrefix, (RTSEL)pCtxCore->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, 
                    pszPrefix, (RTSEL)pCtxCore->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, 
                    pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->dr0,  pszPrefix, pCtx->dr1, pszPrefix, pCtx->dr2,  pszPrefix, pCtx->dr3,
                    pszPrefix, pCtx->dr4,  pszPrefix, pCtx->dr5, pszPrefix, pCtx->dr6,  pszPrefix, pCtx->dr7,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, efl,
                    pszPrefix, (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u64Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
                    pszPrefix, (RTSEL)pCtx->tr, pCtx->trHid.u64Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
                    pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);

                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "MSR:\n"
                        "%sEFER         =%016RX64\n"
                        "%sPAT          =%016RX64\n"
                        "%sSTAR         =%016RX64\n"
                        "%sCSTAR        =%016RX64\n"
                        "%sLSTAR        =%016RX64\n"
                        "%sSFMASK       =%016RX64\n"
                        "%sKERNELGSBASE =%016RX64\n",
                        pszPrefix, pCtx->msrEFER,
                        pszPrefix, pCtx->msrPAT,
                        pszPrefix, pCtx->msrSTAR,
                        pszPrefix, pCtx->msrCSTAR,
                        pszPrefix, pCtx->msrLSTAR,
                        pszPrefix, pCtx->msrSFMASK,
                        pszPrefix, pCtx->msrKERNELGSBASE);
            }
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs={%04x base=%016RX64 limit=%08x flags=%08x} %sdr0=%016RX64 %sdr1=%016RX64\n"
                    "%sds={%04x base=%016RX64 limit=%08x flags=%08x} %sdr2=%016RX64 %sdr3=%016RX64\n"
                    "%ses={%04x base=%016RX64 limit=%08x flags=%08x} %sdr4=%016RX64 %sdr5=%016RX64\n"
                    "%sfs={%04x base=%016RX64 limit=%08x flags=%08x} %sdr6=%016RX64 %sdr7=%016RX64\n"
                    "%sgs={%04x base=%016RX64 limit=%08x flags=%08x} %scr0=%016RX64 %scr2=%016RX64\n"
                    "%sss={%04x base=%016RX64 limit=%08x flags=%08x} %scr3=%016RX64 %scr4=%016RX64\n"
                    "%sgdtr=%016RX64:%04x  %sidtr=%016RX64:%04x  %seflags=%08x\n"
                    "%sldtr={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%str  ={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sSysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
                    "%sFCW=%04x %sFSW=%04x %sFTW=%04x\n"
                    ,
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pszPrefix, pCtx->dr0,  pszPrefix, pCtx->dr1,
                    pszPrefix, (RTSEL)pCtxCore->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pszPrefix, pCtx->dr2,  pszPrefix, pCtx->dr3,
                    pszPrefix, (RTSEL)pCtxCore->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pszPrefix, pCtx->dr4,  pszPrefix, pCtx->dr5,
                    pszPrefix, (RTSEL)pCtxCore->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pszPrefix, pCtx->dr6,  pszPrefix, pCtx->dr7,
                    pszPrefix, (RTSEL)pCtxCore->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2,
                    pszPrefix, (RTSEL)pCtxCore->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt,pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, pCtxCore->eflags,
                    pszPrefix, (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u64Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
                    pszPrefix, (RTSEL)pCtx->tr, pCtx->trHid.u64Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
                    pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp,
                    pszPrefix, pCtx->fpu.FCW, pszPrefix, pCtx->fpu.FSW, pszPrefix, pCtx->fpu.FTW);
        }
        if (CPUMIsGuestIn64BitCodeEx(pCtx))
        {
            /*
             * Disassemble one instruction at cs:rip.
             */
            return pCmdHlp->pfnExec(pCmdHlp, "u %016x L 0", pCtx->rip);
        }
        else
        {
            /*
             * Disassemble one instruction at cs:eip.
             */
            return pCmdHlp->pfnExec(pCmdHlp, "u %04x:%08x L 0", pCtx->cs, pCtx->eip);
        }
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

        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Bas=%08x Lim=%08x DPL=%d %s %s %s %s AVL=%d L=%d%s\n",
                                iEntry, s_apszTypes[pDesc->Gen.u4Type], u32Base, cbLimit,
                                pDesc->Gen.u2Dpl, pszPresent, pszAccessed, pszGranularity, pszBig,
                                pDesc->Gen.u1Available, pDesc->Gen.u1Long, pszHyper);
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
                                        pDesc->Gen.u1Available, pDesc->Gen.u1Long | (pDesc->Gen.u1DefBig << 1),
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
                    if (isprint(u8) && u8 < 127 && u8 >= 32)
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
                    && (    (isprint(u8) && u8 >= 32)
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
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE) * sizeof(Pdpe);
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
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE) * sizeof(Pdpe);
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
 * Converts one or more variables into a byte buffer for a
 * given unit size.
 *
 * @returns VBox status codes:
 * @retval  VERR_TOO_MUCH_DATA if the buffer is too small, bitched.
 * @retval  VERR_INTERNAL_ERROR on bad variable type, bitched.
 * @retval  VINF_SUCCESS on success.
 *
 * @param   pvBuf   The buffer to convert into.
 * @param   pcbBuf  The buffer size on input. The size of the result on output.
 * @param   cbUnit  The unit size to apply when converting.
 *                  The high bit is used to indicate unicode string.
 * @param   paVars  The array of variables to convert.
 * @param   cVars   The number of variables.
 */
int dbgcVarsToBytes(PDBGCCMDHLP pCmdHlp, void *pvBuf, uint32_t *pcbBuf, size_t cbUnit, PCDBGCVAR paVars, unsigned cVars)
{
    union
    {
        uint8_t *pu8;
        uint16_t *pu16;
        uint32_t *pu32;
        uint64_t *pu64;
    } u, uEnd;
    u.pu8 = (uint8_t *)pvBuf;
    uEnd.pu8 = u.pu8 + *pcbBuf;

    unsigned i;
    for (i = 0; i < cVars && u.pu8 < uEnd.pu8; i++)
    {
        switch (paVars[i].enmType)
        {
            case DBGCVAR_TYPE_GC_FAR:
            case DBGCVAR_TYPE_HC_FAR:
            case DBGCVAR_TYPE_GC_FLAT:
            case DBGCVAR_TYPE_GC_PHYS:
            case DBGCVAR_TYPE_HC_FLAT:
            case DBGCVAR_TYPE_HC_PHYS:
            case DBGCVAR_TYPE_NUMBER:
            {
                uint64_t u64 = paVars[i].u.u64Number;
                switch (cbUnit & 0x1f)
                {
                    case 1:
                        do
                        {
                            *u.pu8++ = u64;
                            u64 >>= 8;
                        } while (u64);
                        break;
                    case 2:
                        do
                        {
                            *u.pu16++ = u64;
                            u64 >>= 16;
                        } while (u64);
                        break;
                    case 4:
                        *u.pu32++ = u64;
                        u64 >>= 32;
                        if (u64)
                            *u.pu32++ = u64;
                        break;
                    case 8:
                        *u.pu64++ = u64;
                        break;
                }
                break;
            }

            case DBGCVAR_TYPE_STRING:
            case DBGCVAR_TYPE_SYMBOL:
            {
                const char *psz = paVars[i].u.pszString;
                size_t cbString = strlen(psz);
                if (cbUnit & RT_BIT_32(31))
                {
                    /* Explode char to unit. */
                    if (cbString > (uintptr_t)(uEnd.pu8 - u.pu8) * (cbUnit & 0x1f))
                    {
                        pCmdHlp->pfnVBoxError(pCmdHlp, VERR_TOO_MUCH_DATA, "Max %d bytes.\n", uEnd.pu8 - (uint8_t *)pvBuf);
                        return VERR_TOO_MUCH_DATA;
                    }
                    while (*psz)
                    {
                        switch (cbUnit & 0x1f)
                        {
                            case 1: *u.pu8++ = *psz; break;
                            case 2: *u.pu16++ = *psz; break;
                            case 4: *u.pu32++ = *psz; break;
                            case 8: *u.pu64++ = *psz; break;
                        }
                        psz++;
                    }
                }
                else
                {
                    /* Raw copy with zero padding if the size isn't aligned. */
                    if (cbString > (uintptr_t)(uEnd.pu8 - u.pu8))
                    {
                        pCmdHlp->pfnVBoxError(pCmdHlp, VERR_TOO_MUCH_DATA, "Max %d bytes.\n", uEnd.pu8 - (uint8_t *)pvBuf);
                        return VERR_TOO_MUCH_DATA;
                    }

                    size_t cbCopy = cbString & ~(cbUnit - 1);
                    memcpy(u.pu8, psz, cbCopy);
                    u.pu8 += cbCopy;
                    psz += cbCopy;

                    size_t cbReminder = cbString & (cbUnit - 1);
                    if (cbReminder)
                    {
                        memcpy(u.pu8, psz, cbString & (cbUnit - 1));
                        memset(u.pu8 + cbReminder, 0, cbUnit - cbReminder);
                        u.pu8 += cbUnit;
                    }
                }
                break;
            }

            default:
                *pcbBuf = u.pu8 - (uint8_t *)pvBuf;
                pCmdHlp->pfnVBoxError(pCmdHlp, VERR_INTERNAL_ERROR,
                                      "i=%d enmType=%d\n", i, paVars[i].enmType);
                return VERR_INTERNAL_ERROR;
        }
    }
    *pcbBuf = u.pu8 - (uint8_t *)pvBuf;
    if (i != cVars)
    {
        pCmdHlp->pfnVBoxError(pCmdHlp, VERR_TOO_MUCH_DATA, "Max %d bytes.\n", uEnd.pu8 - (uint8_t *)pvBuf);
        return VERR_TOO_MUCH_DATA;
    }
    return VINF_SUCCESS;
}


/**
 * Executes the search.
 *
 * @returns VBox status code.
 * @param   pCmdHlp     The command helpers.
 * @param   pVM         The VM handle.
 * @param   pAddress    The address to start searching from. (undefined on output)
 * @param   cbRange     The address range to search. Must not wrap.
 * @param   pabBytes    The byte pattern to search for.
 * @param   cbBytes     The size of the pattern.
 * @param   cbUnit      The search unit.
 * @param   cMaxHits    The max number of hits.
 * @param   pResult     Where to store the result if it's a function invocation.
 */
static int dbgcCmdWorkerSearchMemDoIt(PDBGCCMDHLP pCmdHlp, PVM pVM, PDBGFADDRESS pAddress, RTGCUINTPTR cbRange,
                                      const uint8_t *pabBytes, uint32_t cbBytes,
                                      uint32_t cbUnit, uint64_t cMaxHits, PDBGCVAR pResult)
{
    /*
     * Do the search.
     */
    uint64_t cHits = 0;
    for (;;)
    {
        /* search */
        DBGFADDRESS HitAddress;
        int rc = DBGFR3MemScan(pVM, pAddress, cbRange, pabBytes, cbBytes, &HitAddress);
        if (RT_FAILURE(rc))
        {
            if (rc != VERR_DBGF_MEM_NOT_FOUND)
                return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3MemScan\n");

            /* update the current address so we can save it (later). */
            pAddress->off += cbRange;
            pAddress->FlatPtr += cbRange;
            cbRange = 0;
            break;
        }

        /* report result */
        DBGCVAR VarCur;
        dbgcVarInit(&VarCur);
        dbgcVarSetDbgfAddr(&VarCur, &HitAddress);
        if (!pResult)
            pCmdHlp->pfnExec(pCmdHlp, "db %DV LB 10", &VarCur);
        else
            dbgcVarSetDbgfAddr(pResult, &HitAddress);

        /* advance */
        cbRange -= HitAddress.FlatPtr - pAddress->FlatPtr;
        *pAddress = HitAddress;
        pAddress->FlatPtr += cbBytes;
        pAddress->off += cbBytes;
        if (cbRange <= cbBytes)
        {
            cbRange = 0;
            break;
        }
        cbRange -= cbBytes;

        if (++cHits >= cMaxHits)
        {
            /// @todo save the search.
            break;
        }
    }

    /*
     * Save the search so we can resume it...
     */
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    if (pDbgc->abSearch != pabBytes)
    {
        memcpy(pDbgc->abSearch, pabBytes, cbBytes);
        pDbgc->cbSearch = cbBytes;
        pDbgc->cbSearchUnit = cbUnit;
    }
    pDbgc->cMaxSearchHits = cMaxHits;
    pDbgc->SearchAddr = *pAddress;
    pDbgc->cbSearchRange = cbRange;

    return cHits ? VINF_SUCCESS : VERR_DBGC_COMMAND_FAILED;
}


/**
 * Resumes the previous search.
 *
 * @returns VBox status code.
 * @param   pCmdHlp     Pointer to the command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   pResult     Where to store the result of a function invocation.
 */
static int dbgcCmdWorkerSearchMemResume(PDBGCCMDHLP pCmdHlp, PVM pVM, PDBGCVAR pResult)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Make sure there is a previous command.
     */
    if (!pDbgc->cbSearch)
    {
        pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Error: No previous search\n");
        return VERR_DBGC_COMMAND_FAILED;
    }

    /*
     * Make range and address adjustments.
     */
    DBGFADDRESS Address = pDbgc->SearchAddr;
    if (Address.FlatPtr == ~(RTGCUINTPTR)0)
    {
        Address.FlatPtr -= Address.off;
        Address.off = 0;
    }

    RTGCUINTPTR cbRange = pDbgc->cbSearchRange;
    if (!cbRange)
        cbRange = ~(RTGCUINTPTR)0;
    if (Address.FlatPtr + cbRange < pDbgc->SearchAddr.FlatPtr)
        cbRange = ~(RTGCUINTPTR)0 - pDbgc->SearchAddr.FlatPtr + !!pDbgc->SearchAddr.FlatPtr;

    return dbgcCmdWorkerSearchMemDoIt(pCmdHlp, pVM, &Address, cbRange, pDbgc->abSearch, pDbgc->cbSearch,
                                      pDbgc->cbSearchUnit, pDbgc->cMaxSearchHits, pResult);
}


/**
 * Search memory, worker for the 's' and 's?' functions.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   pAddress    Where to start searching. If no range, search till end of address space.
 * @param   cMaxHits    The maximum number of hits.
 * @param   chType      The search type.
 * @param   paPatArgs   The pattern variable array.
 * @param   cPatArgs    Number of pattern variables.
 * @param   pResult     Where to store the result of a function invocation.
 */
static int dbgcCmdWorkerSearchMem(PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR pAddress, uint64_t cMaxHits, char chType,
                                  PCDBGCVAR paPatArgs, unsigned cPatArgs, PDBGCVAR pResult)
{
    dbgcVarSetGCFlat(pResult, 0);

    /*
     * Convert the search pattern into bytes and DBGFR3MemScan can deal with.
     */
    uint32_t cbUnit;
    switch (chType)
    {
        case 'a':
        case 'b':   cbUnit = 1; break;
        case 'u':   cbUnit = 2 | RT_BIT_32(31); break;
        case 'w':   cbUnit = 2; break;
        case 'd':   cbUnit = 4; break;
        case 'q':   cbUnit = 8; break;
        default:
            return pCmdHlp->pfnVBoxError(pCmdHlp, VERR_INVALID_PARAMETER, "chType=%c\n", chType);
    }
    uint8_t abBytes[RT_SIZEOFMEMB(DBGC, abSearch)];
    uint32_t cbBytes = sizeof(abBytes);
    int rc = dbgcVarsToBytes(pCmdHlp, abBytes, &cbBytes, cbUnit, paPatArgs, cPatArgs);
    if (RT_FAILURE(rc))
        return VERR_DBGC_COMMAND_FAILED;

    /*
     * Make DBGF address and fix the range.
     */
    DBGFADDRESS Address;
    rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, pAddress, &Address);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "VarToDbgfAddr(,%Dv,)\n", pAddress);

    RTGCUINTPTR cbRange;
    switch (pAddress->enmRangeType)
    {
        case DBGCVAR_RANGE_BYTES:
            cbRange = pAddress->u64Range;
            if (cbRange != pAddress->u64Range)
                cbRange = ~(RTGCUINTPTR)0;
            break;

        case DBGCVAR_RANGE_ELEMENTS:
            cbRange = (RTGCUINTPTR)(pAddress->u64Range * cbUnit);
            if (    cbRange != pAddress->u64Range * cbUnit
                ||  cbRange < pAddress->u64Range)
                cbRange = ~(RTGCUINTPTR)0;
            break;

        default:
            cbRange = ~(RTGCUINTPTR)0;
            break;
    }
    if (Address.FlatPtr + cbRange < Address.FlatPtr)
        cbRange = ~(RTGCUINTPTR)0 - Address.FlatPtr + !!Address.FlatPtr;

    /*
     * Ok, do it.
     */
    return dbgcCmdWorkerSearchMemDoIt(pCmdHlp, pVM, &Address, cbRange, abBytes, cbBytes, cbUnit, cMaxHits, pResult);
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
    //if (    cArgs <= 2
    //    &&  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    //    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");

    /*
     * Repeate previous search?
     */
    if (cArgs == 0)
        return dbgcCmdWorkerSearchMemResume(pCmdHlp, pVM, pResult);

    /*
     * Parse arguments.
     */

    return -1;
}


/**
 * The 's?' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdSearchMemType(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /* check that the parser did what it's supposed to do. */
    if (    cArgs < 2
        ||  !DBGCVAR_ISGCPOINTER(paArgs[0].enmType))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");
    return dbgcCmdWorkerSearchMem(pCmdHlp, pVM, &paArgs[0], pResult ? 1 : 25, pCmd->pszCmd[1], paArgs + 1, cArgs - 1, pResult);
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

