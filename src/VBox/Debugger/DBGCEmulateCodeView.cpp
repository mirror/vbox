/* $Id$ */
/** @file
 * DBGC - Debugger Console, CodeView / WinDbg Emulation.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGC
#include <VBox/dbg.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/dis.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>

#include <stdlib.h>
#include <stdio.h>

#include "DBGCInternal.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgcCmdBrkAccess(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdBrkClear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdBrkDisable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdBrkEnable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdBrkList(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdBrkSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdBrkREM(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpIDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpPageDir(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpPageDirBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpPageHierarchy(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpPageTable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpPageTableBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDumpTSS(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdEditMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdGo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdListModules(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdListNear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdListSource(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdMemoryInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdReg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdRegGuest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdRegHyper(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdRegTerse(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdSearchMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdSearchMemType(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdStack(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdTrace(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdUnassemble(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);


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
    {  0,           ~0U,        DBGCVAR_CAT_NUMBER,     0,                              "#bp",          "Breakpoint number." },
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
    {  0,           ~0U,        DBGCVAR_CAT_NUMBER,     0,                              "sel",          "Selector or selector range." },
    {  0,           ~0U,        DBGCVAR_CAT_POINTER,    0,                              "address",      "Far address which selector should be dumped." },
};


/** 'di', 'dia' arguments. */
static const DBGCVARDESC    g_aArgDumpIDT[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0U,        DBGCVAR_CAT_NUMBER,     0,                              "int",          "The interrupt vector or interrupt vector range." },
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


/** 'dph*' arguments. */
static const DBGCVARDESC    g_aArgDumpPH[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "address",      "Where in the address space to start dumping and for how long (range).  The default address/range will be used if omitted." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     DBGCVD_FLAGS_DEP_PREV,          "cr3",          "The CR3 value to use.  The current CR3 of the context will be used if omitted." },
    {  0,           1,          DBGCVAR_CAT_STRING,     DBGCVD_FLAGS_DEP_PREV,          "mode",         "The paging mode: legacy, pse, pae, long, ept. Append '-np' for nested paging and '-nx' for no-execute.  The current mode will be used if omitted." },
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


/** 'e?' arguments. */
static const DBGCVARDESC    g_aArgEditMem[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "Address where to write." },
    {  1,           ~0U,        DBGCVAR_CAT_NUMBER,     0,                              "value",        "Value to write." },
};


/** 'lm' arguments. */
static const DBGCVARDESC    g_aArgListMods[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0U,        DBGCVAR_CAT_STRING,     0,                              "module",       "Module name." },
};


/** 'ln' arguments. */
static const DBGCVARDESC    g_aArgListNear[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0U,        DBGCVAR_CAT_POINTER,    0,                              "address",      "Address of the symbol to look up." },
    {  0,           ~0U,        DBGCVAR_CAT_SYMBOL,     0,                              "symbol",       "Symbol to lookup." },
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
    {  0,           ~0U,        DBGCVAR_CAT_ANY,        0,                              "pattern",      "Pattern to search for." },
};


/** 's?' arguments. */
static const DBGCVARDESC    g_aArgSearchMemType[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_GC_POINTER, 0,                              "range",        "Register to show or set." },
    {  1,           ~0U,        DBGCVAR_CAT_ANY,        0,                              "pattern",      "Pattern to search for." },
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
    /* pszCmd,      cArgsMin, cArgsMax, paArgDescs,         cArgDescs,                      fFlags,  pfnHandler          pszSyntax,          ....pszDescription */
    { "ba",         3,        6,        &g_aArgBrkAcc[0],   RT_ELEMENTS(g_aArgBrkAcc),      0,       dbgcCmdBrkAccess,   "<access> <size> <address> [passes [max passes]] [cmds]",
                                                                                                                                                 "Sets a data access breakpoint." },
    { "bc",         1,       ~0U,       &g_aArgBrks[0],     RT_ELEMENTS(g_aArgBrks),        0,       dbgcCmdBrkClear,    "all | <bp#> [bp# []]", "Deletes a set of breakpoints." },
    { "bd",         1,       ~0U,       &g_aArgBrks[0],     RT_ELEMENTS(g_aArgBrks),        0,       dbgcCmdBrkDisable,  "all | <bp#> [bp# []]", "Disables a set of breakpoints." },
    { "be",         1,       ~0U,       &g_aArgBrks[0],     RT_ELEMENTS(g_aArgBrks),        0,       dbgcCmdBrkEnable,   "all | <bp#> [bp# []]", "Enables a set of breakpoints." },
    { "bl",         0,        0,        NULL,               0,                              0,       dbgcCmdBrkList,     "",                     "Lists all the breakpoints." },
    { "bp",         1,        4,        &g_aArgBrkSet[0],   RT_ELEMENTS(g_aArgBrkSet),      0,       dbgcCmdBrkSet,      "<address> [passes [max passes]] [cmds]",
                                                                                                                                                 "Sets a breakpoint (int 3)." },
    { "br",         1,        4,        &g_aArgBrkREM[0],   RT_ELEMENTS(g_aArgBrkREM),      0,       dbgcCmdBrkREM,      "<address> [passes [max passes]] [cmds]",
                                                                                                                                                 "Sets a recompiler specific breakpoint." },
    { "d",          0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory using last element size." },
    { "da",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory as ascii string." },
    { "db",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in bytes." },
    { "dd",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in double words." },
    { "da",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory as ascii string." },
    { "dg",         0,       ~0U,       &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),      0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the global descriptor table (GDT)." },
    { "dga",        0,       ~0U,       &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),      0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the global descriptor table (GDT) including not-present entries." },
    { "di",         0,       ~0U,       &g_aArgDumpIDT[0],  RT_ELEMENTS(g_aArgDumpIDT),     0,       dbgcCmdDumpIDT,     "[int [..]]",           "Dump the interrupt descriptor table (IDT)." },
    { "dia",        0,       ~0U,       &g_aArgDumpIDT[0],  RT_ELEMENTS(g_aArgDumpIDT),     0,       dbgcCmdDumpIDT,     "[int [..]]",           "Dump the interrupt descriptor table (IDT) including not-present entries." },
    { "dl",         0,       ~0U,       &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),      0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the local descriptor table (LDT)." },
    { "dla",        0,       ~0U,       &g_aArgDumpDT[0],   RT_ELEMENTS(g_aArgDumpDT),      0,       dbgcCmdDumpDT,      "[sel [..]]",           "Dump the local descriptor table (LDT) including not-present entries." },
    { "dpd",        0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),      0,       dbgcCmdDumpPageDir, "[addr|index]",         "Dumps page directory entries of the default context." },
    { "dpda",       0,        1,        &g_aArgDumpPDAddr[0],RT_ELEMENTS(g_aArgDumpPDAddr), 0,       dbgcCmdDumpPageDir, "[addr]",               "Dumps memory at given address as a page directory." },
    { "dpdb",       0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),      0,       dbgcCmdDumpPageDirBoth, "[addr|index]",     "Dumps page directory entries of the guest and the hypervisor. " },
    { "dpdg",       0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),      0,       dbgcCmdDumpPageDir, "[addr|index]",         "Dumps page directory entries of the guest." },
    { "dpdh",       0,        1,        &g_aArgDumpPD[0],   RT_ELEMENTS(g_aArgDumpPD),      0,       dbgcCmdDumpPageDir, "[addr|index]",         "Dumps page directory entries of the hypervisor. " },
    { "dph",        0,        3,        &g_aArgDumpPH[0],   RT_ELEMENTS(g_aArgDumpPH),      0, dbgcCmdDumpPageHierarchy, "[addr [cr3 [mode]]",   "Dumps the paging hierarchy at for specfied address range. Default context." },
    { "dphg",       0,        3,        &g_aArgDumpPH[0],   RT_ELEMENTS(g_aArgDumpPH),      0, dbgcCmdDumpPageHierarchy, "[addr [cr3 [mode]]",   "Dumps the paging hierarchy at for specfied address range. Guest context." },
    { "dphh",       0,        3,        &g_aArgDumpPH[0],   RT_ELEMENTS(g_aArgDumpPH),      0, dbgcCmdDumpPageHierarchy, "[addr [cr3 [mode]]",   "Dumps the paging hierarchy at for specfied address range. Hypervisor context." },
    { "dpt",        1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),      0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the default context." },
    { "dpta",       1,        1,        &g_aArgDumpPTAddr[0],RT_ELEMENTS(g_aArgDumpPTAddr), 0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps memory at given address as a page table." },
    { "dptb",       1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),      0,       dbgcCmdDumpPageTableBoth,"<addr>",          "Dumps page table entries of the guest and the hypervisor." },
    { "dptg",       1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),      0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the guest." },
    { "dpth",       1,        1,        &g_aArgDumpPT[0],   RT_ELEMENTS(g_aArgDumpPT),      0,       dbgcCmdDumpPageTable,"<addr>",              "Dumps page table entries of the hypervisor." },
    { "dq",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in quad words." },
    { "dt",         0,        1,        &g_aArgDumpTSS[0],  RT_ELEMENTS(g_aArgDumpTSS),     0,       dbgcCmdDumpTSS,     "[tss|tss:ign|addr]",   "Dump the task state segment (TSS)." },
    { "dt16",       0,        1,        &g_aArgDumpTSS[0],  RT_ELEMENTS(g_aArgDumpTSS),     0,       dbgcCmdDumpTSS,     "[tss|tss:ign|addr]",   "Dump the 16-bit task state segment (TSS)." },
    { "dt32",       0,        1,        &g_aArgDumpTSS[0],  RT_ELEMENTS(g_aArgDumpTSS),     0,       dbgcCmdDumpTSS,     "[tss|tss:ign|addr]",   "Dump the 32-bit task state segment (TSS)." },
    { "dt64",       0,        1,        &g_aArgDumpTSS[0],  RT_ELEMENTS(g_aArgDumpTSS),     0,       dbgcCmdDumpTSS,     "[tss|tss:ign|addr]",   "Dump the 64-bit task state segment (TSS)." },
    { "dw",         0,        1,        &g_aArgDumpMem[0],  RT_ELEMENTS(g_aArgDumpMem),     0,       dbgcCmdDumpMem,     "[addr]",               "Dump memory in words." },
    /** @todo add 'e', 'ea str', 'eza str', 'eu str' and 'ezu str'. See also
     *        dbgcCmdSearchMem and its dbgcVarsToBytes usage. */
    { "eb",         2,        2,        &g_aArgEditMem[0],  RT_ELEMENTS(g_aArgEditMem),     0,       dbgcCmdEditMem,     "<addr> <value>",       "Write a 1-byte value to memory." },
    { "ew",         2,        2,        &g_aArgEditMem[0],  RT_ELEMENTS(g_aArgEditMem),     0,       dbgcCmdEditMem,     "<addr> <value>",       "Write a 2-byte value to memory." },
    { "ed",         2,        2,        &g_aArgEditMem[0],  RT_ELEMENTS(g_aArgEditMem),     0,       dbgcCmdEditMem,     "<addr> <value>",       "Write a 4-byte value to memory." },
    { "eq",         2,        2,        &g_aArgEditMem[0],  RT_ELEMENTS(g_aArgEditMem),     0,       dbgcCmdEditMem,     "<addr> <value>",       "Write a 8-byte value to memory." },
    { "g",          0,        0,        NULL,               0,                              0,       dbgcCmdGo,          "",                     "Continue execution." },
    { "k",          0,        0,        NULL,               0,                              0,       dbgcCmdStack,       "",                     "Callstack." },
    { "kg",         0,        0,        NULL,               0,                              0,       dbgcCmdStack,       "",                     "Callstack - guest." },
    { "kh",         0,        0,        NULL,               0,                              0,       dbgcCmdStack,       "",                     "Callstack - hypervisor." },
    { "lm",         0,        ~0U,      &g_aArgListMods[0], RT_ELEMENTS(g_aArgListMods),    0,       dbgcCmdListModules, "[module [..]]",        "List modules." },
    { "lmo",        0,        ~0U,      &g_aArgListMods[0], RT_ELEMENTS(g_aArgListMods),    0,       dbgcCmdListModules, "[module [..]]",        "List modules and their segments." },
    { "ln",         0,        ~0U,      &g_aArgListNear[0], RT_ELEMENTS(g_aArgListNear),    0,       dbgcCmdListNear,    "[addr/sym [..]]",      "List symbols near to the address. Default address is CS:EIP." },
    { "ls",         0,        1,        &g_aArgListSource[0],RT_ELEMENTS(g_aArgListSource), 0,       dbgcCmdListSource,  "[addr]",               "Source." },
    { "m",          1,        1,        &g_aArgMemoryInfo[0],RT_ELEMENTS(g_aArgMemoryInfo), 0,       dbgcCmdMemoryInfo,  "<addr>",               "Display information about that piece of memory." },
    { "r",          0,        2,        &g_aArgReg[0],      RT_ELEMENTS(g_aArgReg),         0,       dbgcCmdReg,         "[reg [newval]]",       "Show or set register(s) - active reg set." },
    { "rg",         0,        2,        &g_aArgReg[0],      RT_ELEMENTS(g_aArgReg),         0,       dbgcCmdRegGuest,    "[reg [newval]]",       "Show or set register(s) - guest reg set." },
    { "rg32",       0,        0,        NULL,               0,                              0,       dbgcCmdRegGuest,    "",                     "Show 32-bit guest registers." },
    { "rg64",       0,        0,        NULL,               0,                              0,       dbgcCmdRegGuest,    "",                     "Show 64-bit guest registers." },
    { "rh",         0,        2,        &g_aArgReg[0],      RT_ELEMENTS(g_aArgReg),         0,       dbgcCmdRegHyper,    "[reg [newval]]",       "Show or set register(s) - hypervisor reg set." },
    { "rt",         0,        0,        NULL,               0,                              0,       dbgcCmdRegTerse,    "",                     "Toggles terse / verbose register info." },
    { "s",          0,       ~0U,       &g_aArgSearchMem[0], RT_ELEMENTS(g_aArgSearchMem),  0,       dbgcCmdSearchMem,   "[options] <range> <pattern>",  "Continue last search." },
    { "sa",         2,       ~0U,       &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),0, dbgcCmdSearchMemType, "<range> <pattern>",  "Search memory for an ascii string." },
    { "sb",         2,       ~0U,       &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),0, dbgcCmdSearchMemType, "<range> <pattern>",  "Search memory for one or more bytes." },
    { "sd",         2,       ~0U,       &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),0, dbgcCmdSearchMemType, "<range> <pattern>",  "Search memory for one or more double words." },
    { "sq",         2,       ~0U,       &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),0, dbgcCmdSearchMemType, "<range> <pattern>",  "Search memory for one or more quad words." },
    { "su",         2,       ~0U,       &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),0, dbgcCmdSearchMemType, "<range> <pattern>",  "Search memory for an unicode string." },
    { "sw",         2,       ~0U,       &g_aArgSearchMemType[0], RT_ELEMENTS(g_aArgSearchMemType),0, dbgcCmdSearchMemType, "<range> <pattern>",  "Search memory for one or more words." },
    { "t",          0,        0,        NULL,               0,                              0,       dbgcCmdTrace,       "",                     "Instruction trace (step into)." },
    { "u",          0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble), 0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble." },
    { "u64",        0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble), 0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble 64-bit code." },
    { "u32",        0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble), 0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble 32-bit code." },
    { "u16",        0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble), 0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble 16-bit code." },
    { "uv86",       0,        1,        &g_aArgUnassemble[0],RT_ELEMENTS(g_aArgUnassemble), 0,       dbgcCmdUnassemble,  "[addr]",               "Unassemble 16-bit code with v8086/real mode addressing." },
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
static DECLCALLBACK(int) dbgcCmdGo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);

    /*
     * Check if the VM is halted or not before trying to resume it.
     */
    if (!DBGFR3IsHalted(pVM))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "The VM is already running");

    int rc = DBGFR3Resume(pVM);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGFR3Resume");

    NOREF(paArgs); NOREF(cArgs);
    return VINF_SUCCESS;
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
static DECLCALLBACK(int) dbgcCmdBrkAccess(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);

    /*
     * Interpret access type.
     */
    if (    !strchr("xrwi", paArgs[0].u.pszString[0])
        ||  paArgs[0].u.pszString[1])
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Invalid access type '%s' for '%s'. Valid types are 'e', 'r', 'w' and 'i'",
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
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Invalid access size %RX64 for '%s'. 'x' access type requires size 1!",
                              paArgs[1].u.u64Number, pCmd->pszCmd);
    switch (paArgs[1].u.u64Number)
    {
        case 1:
        case 2:
        case 4:
            break;
        /*case 8: - later*/
        default:
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Invalid access size %RX64 for '%s'. 1, 2 or 4!",
                                  paArgs[1].u.u64Number, pCmd->pszCmd);
    }
    uint8_t cb = (uint8_t)paArgs[1].u.u64Number;

    /*
     * Convert the pointer to a DBGF address.
     */
    DBGFADDRESS Address;
    int rc = DBGCCmdHlpVarToDbgfAddr(pCmdHlp, &paArgs[2], &Address);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGCCmdHlpVarToDbgfAddr(,%DV,)", &paArgs[2]);

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
    uint32_t iBp;
    rc = DBGFR3BpSetReg(pVM, &Address, iHitTrigger, iHitDisable, fType, cb, &iBp);
    if (RT_SUCCESS(rc))
    {
        PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        rc = dbgcBpAdd(pDbgc, iBp, pszCmds);
        if (RT_SUCCESS(rc))
            return DBGCCmdHlpPrintf(pCmdHlp, "Set access breakpoint %u at %RGv\n", iBp, Address.FlatPtr);
        if (rc == VERR_DBGC_BP_EXISTS)
        {
            rc = dbgcBpUpdate(pDbgc, iBp, pszCmds);
            if (RT_SUCCESS(rc))
                return DBGCCmdHlpPrintf(pCmdHlp, "Updated access breakpoint %u at %RGv\n", iBp, Address.FlatPtr);
        }
        int rc2 = DBGFR3BpClear(pDbgc->pVM, iBp);
        AssertRC(rc2);
    }
    return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "Failed to set access breakpoint at %RGv", Address.FlatPtr);
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
static DECLCALLBACK(int) dbgcCmdBrkClear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);

    /*
     * Enumerate the arguments.
     */
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    int     rc    = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs && RT_SUCCESS(rc); iArg++)
    {
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
        {
            /* one */
            uint32_t iBp = (uint32_t)paArgs[iArg].u.u64Number;
            if (iBp == paArgs[iArg].u.u64Number)
            {
                int rc2 = DBGFR3BpClear(pVM, iBp);
                if (RT_FAILURE(rc2))
                    rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc2, "DBGFR3BpClear(,%#x)", iBp);
                if (RT_SUCCESS(rc2) || rc2 == VERR_DBGF_BP_NOT_FOUND)
                    dbgcBpDelete(pDbgc, iBp);
            }
            else
                rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "Breakpoint id %RX64 is too large", paArgs[iArg].u.u64Number);
        }
        else if (!strcmp(paArgs[iArg].u.pszString, "all"))
        {
            /* all */
            PDBGCBP pBp = pDbgc->pFirstBp;
            while (pBp)
            {
                uint32_t iBp = pBp->iBp;
                pBp = pBp->pNext;

                int rc2 = DBGFR3BpClear(pVM, iBp);
                if (RT_FAILURE(rc2))
                    rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc2, "DBGFR3BpClear(,%#x)", iBp);
                if (RT_SUCCESS(rc2) || rc2 == VERR_DBGF_BP_NOT_FOUND)
                    dbgcBpDelete(pDbgc, iBp);
            }
        }
        else
            rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "Invalid argument '%s'", paArgs[iArg].u.pszString);
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
static DECLCALLBACK(int) dbgcCmdBrkDisable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Enumerate the arguments.
     */
    int rc = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs && RT_SUCCESS(rc); iArg++)
    {
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
        {
            /* one */
            uint32_t iBp = (uint32_t)paArgs[iArg].u.u64Number;
            if (iBp == paArgs[iArg].u.u64Number)
            {
                rc = DBGFR3BpDisable(pVM, iBp);
                if (RT_FAILURE(rc))
                    rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGFR3BpDisable failed for breakpoint %#x", iBp);
            }
            else
                rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "Breakpoint id %RX64 is too large", paArgs[iArg].u.u64Number);
        }
        else if (!strcmp(paArgs[iArg].u.pszString, "all"))
        {
            /* all */
            PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
            for (PDBGCBP pBp = pDbgc->pFirstBp; pBp; pBp = pBp->pNext)
            {
                int rc2 = DBGFR3BpDisable(pVM, pBp->iBp);
                if (RT_FAILURE(rc2))
                    rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc2, "DBGFR3BpDisable failed for breakpoint %#x", pBp->iBp);
            }
        }
        else
            rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "Invalid argument '%s'", paArgs[iArg].u.pszString);
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
static DECLCALLBACK(int) dbgcCmdBrkEnable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);

    /*
     * Enumerate the arguments.
     */
    int rc = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs && RT_SUCCESS(rc); iArg++)
    {
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_STRING)
        {
            /* one */
            uint32_t iBp = (uint32_t)paArgs[iArg].u.u64Number;
            if (iBp == paArgs[iArg].u.u64Number)
            {
                rc = DBGFR3BpEnable(pVM, iBp);
                if (RT_FAILURE(rc))
                    rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGFR3BpEnable failed for breakpoint %#x", iBp);
            }
            else
                rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "Breakpoint id %RX64 is too large", paArgs[iArg].u.u64Number);
        }
        else if (!strcmp(paArgs[iArg].u.pszString, "all"))
        {
            /* all */
            PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
            for (PDBGCBP pBp = pDbgc->pFirstBp; pBp; pBp = pBp->pNext)
            {
                int rc2 = DBGFR3BpEnable(pVM, pBp->iBp);
                if (RT_FAILURE(rc2))
                    rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc2, "DBGFR3BpEnable failed for breakpoint %#x", pBp->iBp);
            }
        }
        else
            rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "Invalid argument '%s'", paArgs[iArg].u.pszString);
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
    PDBGC   pDbgc   = (PDBGC)pvUser;
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

    DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "%4#x %c %d %c %RGv %04RX64 (%04RX64 to ",
                     pBp->iBp, pBp->fEnabled ? 'e' : 'd', cb, chType,
                     pBp->GCPtr, pBp->cHits, pBp->iHitTrigger);
    if (pBp->iHitDisable == ~(uint64_t)0)
        DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "~0)  ");
    else
        DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "%04RX64)");

    /*
     * Try resolve the address.
     */
    RTDBGSYMBOL Sym;
    RTINTPTR    off;
    DBGFADDRESS Addr;
    int rc = DBGFR3AsSymbolByAddr(pVM, pDbgc->hDbgAs, DBGFR3AddrFromFlat(pVM, &Addr, pBp->GCPtr), &off, &Sym, NULL);
    if (RT_SUCCESS(rc))
    {
        if (!off)
            DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "%s", Sym.szName);
        else if (off > 0)
            DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "%s+%RGv", Sym.szName, off);
        else
            DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "%s+%RGv", Sym.szName, -off);
    }

    /*
     * The commands.
     */
    if (pDbgcBp)
    {
        if (pDbgcBp->cchCmd)
            DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "\n  cmds: '%s'\n", pDbgcBp->szCmd);
        else
            DBGCCmdHlpPrintf(&pDbgc->CmdHlp, "\n");
    }
    else
        DBGCCmdHlpPrintf(&pDbgc->CmdHlp, " [unknown bp]\n");

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
static DECLCALLBACK(int) dbgcCmdBrkList(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR /*paArgs*/, unsigned cArgs)
{
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);
    DBGC_CMDHLP_ASSERT_PARSER_RET(pCmdHlp, pCmd, -1, cArgs == 0);

    /*
     * Enumerate the breakpoints.
     */
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    int rc = DBGFR3BpEnum(pVM, dbgcEnumBreakpointsCallback, pDbgc);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGFR3BpEnum");
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
static DECLCALLBACK(int) dbgcCmdBrkSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Convert the pointer to a DBGF address.
     */
    DBGFADDRESS Address;
    int rc = DBGCCmdHlpVarToDbgfAddr(pCmdHlp, &paArgs[0], &Address);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGCCmdHlpVarToDbgfAddr(,'%DV',)", &paArgs[0]);

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
    uint32_t iBp;
    rc = DBGFR3BpSet(pVM, &Address, iHitTrigger, iHitDisable, &iBp);
    if (RT_SUCCESS(rc))
    {
        PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        rc = dbgcBpAdd(pDbgc, iBp, pszCmds);
        if (RT_SUCCESS(rc))
            return DBGCCmdHlpPrintf(pCmdHlp, "Set breakpoint %u at %RGv\n", iBp, Address.FlatPtr);
        if (rc == VERR_DBGC_BP_EXISTS)
        {
            rc = dbgcBpUpdate(pDbgc, iBp, pszCmds);
            if (RT_SUCCESS(rc))
                return DBGCCmdHlpPrintf(pCmdHlp, "Updated breakpoint %u at %RGv\n", iBp, Address.FlatPtr);
        }
        int rc2 = DBGFR3BpClear(pDbgc->pVM, iBp);
        AssertRC(rc2);
    }
    return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "Failed to set breakpoint at %RGv", Address.FlatPtr);
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
static DECLCALLBACK(int) dbgcCmdBrkREM(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Convert the pointer to a DBGF address.
     */
    DBGFADDRESS Address;
    int rc = DBGCCmdHlpVarToDbgfAddr(pCmdHlp, &paArgs[0], &Address);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGCCmdHlpVarToDbgfAddr(,'%DV',)", &paArgs[0]);

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
    uint32_t iBp;
    rc = DBGFR3BpSetREM(pVM, &Address, iHitTrigger, iHitDisable, &iBp);
    if (RT_SUCCESS(rc))
    {
        PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        rc = dbgcBpAdd(pDbgc, iBp, pszCmds);
        if (RT_SUCCESS(rc))
            return DBGCCmdHlpPrintf(pCmdHlp, "Set REM breakpoint %u at %RGv\n", iBp, Address.FlatPtr);
        if (rc == VERR_DBGC_BP_EXISTS)
        {
            rc = dbgcBpUpdate(pDbgc, iBp, pszCmds);
            if (RT_SUCCESS(rc))
                return DBGCCmdHlpPrintf(pCmdHlp, "Updated REM breakpoint %u at %RGv\n", iBp, Address.FlatPtr);
        }
        int rc2 = DBGFR3BpClear(pDbgc->pVM, iBp);
        AssertRC(rc2);
    }
    return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "Failed to set REM breakpoint at %RGv", Address.FlatPtr);
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
static DECLCALLBACK(int) dbgcCmdUnassemble(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Validate input.
     */
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);
    DBGC_CMDHLP_ASSERT_PARSER_RET(pCmdHlp, pCmd, -1, cArgs <= 1);
    DBGC_CMDHLP_ASSERT_PARSER_RET(pCmdHlp, pCmd, 0, cArgs == 0 || DBGCVAR_ISPOINTER(paArgs[0].enmType));

    if (!cArgs && !DBGCVAR_ISPOINTER(pDbgc->DisasmPos.enmType))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Don't know where to start disassembling");

    /*
     * Check the desired mode.
     */
    unsigned fFlags = DBGF_DISAS_FLAGS_NO_ADDRESS;
    switch (pCmd->pszCmd[1])
    {
        default: AssertFailed();
        case '\0':  fFlags |= DBGF_DISAS_FLAGS_DEFAULT_MODE;    break;
        case '6':   fFlags |= DBGF_DISAS_FLAGS_64BIT_MODE;      break;
        case '3':   fFlags |= DBGF_DISAS_FLAGS_32BIT_MODE;      break;
        case '1':   fFlags |= DBGF_DISAS_FLAGS_16BIT_MODE;      break;
        case 'v':   fFlags |= DBGF_DISAS_FLAGS_16BIT_REAL_MODE; break;
    }

    /*
     * Find address.
     */
    if (!cArgs)
    {
        if (!DBGCVAR_ISPOINTER(pDbgc->DisasmPos.enmType))
        {
            /** @todo Batch query CS, RIP & CPU mode. */
            PVMCPU pVCpu = VMMGetCpuById(pVM, pDbgc->idCpu);
            if (    pDbgc->fRegCtxGuest
                &&  CPUMIsGuestIn64BitCodeEx(CPUMQueryGuestCtxPtr(pVCpu)))
            {
                pDbgc->DisasmPos.enmType    = DBGCVAR_TYPE_GC_FLAT;
                pDbgc->SourcePos.u.GCFlat   = CPUMGetGuestRIP(pVCpu);
            }
            else
            {
                pDbgc->DisasmPos.enmType     = DBGCVAR_TYPE_GC_FAR;
                pDbgc->SourcePos.u.GCFar.off = pDbgc->fRegCtxGuest ? CPUMGetGuestEIP(pVCpu) : CPUMGetHyperEIP(pVCpu);
                pDbgc->SourcePos.u.GCFar.sel = pDbgc->fRegCtxGuest ? CPUMGetGuestCS(pVCpu)  : CPUMGetHyperCS(pVCpu);
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
    pDbgc->pLastPos = &pDbgc->DisasmPos;

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
                return DBGCCmdHlpFail(pCmdHlp, pCmd, "Too many lines requested. Max is 2048 lines");
            break;

        case DBGCVAR_RANGE_BYTES:
            if (pDbgc->DisasmPos.u64Range > 65536)
                return DBGCCmdHlpFail(pCmdHlp, pCmd, "The requested range is too big. Max is 64KB");
            break;

        default:
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Unknown range type %d", pDbgc->DisasmPos.enmRangeType);
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
        {
            DBGCVAR VarTmp;
            rc = DBGCCmdHlpEval(pCmdHlp, &VarTmp, "%%(%Dv)", &pDbgc->DisasmPos);
            if (RT_FAILURE(rc))
                return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "failed to evaluate '%%(%Dv)'", &pDbgc->DisasmPos);
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
    if (RT_FAILURE(rc))
        return rc;
#endif

    /*
     * Do the disassembling.
     */
    unsigned    cTries = 32;
    int         iRangeLeft = (int)pDbgc->DisasmPos.u64Range;
    if (iRangeLeft == 0)                /* kludge for 'r'. */
        iRangeLeft = -1;
    for (;;)
    {
        /*
         * Disassemble the instruction.
         */
        char        szDis[256];
        uint32_t    cbInstr = 1;
        if (pDbgc->DisasmPos.enmType == DBGCVAR_TYPE_GC_FLAT)
            rc = DBGFR3DisasInstrEx(pVM, pDbgc->idCpu, DBGF_SEL_FLAT, pDbgc->DisasmPos.u.GCFlat, fFlags,
                                    &szDis[0], sizeof(szDis), &cbInstr);
        else
            rc = DBGFR3DisasInstrEx(pVM, pDbgc->idCpu, pDbgc->DisasmPos.u.GCFar.sel, pDbgc->DisasmPos.u.GCFar.off, fFlags,
                                    &szDis[0], sizeof(szDis), &cbInstr);
        if (RT_SUCCESS(rc))
        {
            /* print it */
            rc = DBGCCmdHlpPrintf(pCmdHlp, "%-16DV %s\n", &pDbgc->DisasmPos, &szDis[0]);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
        {
            /* bitch. */
            rc = DBGCCmdHlpPrintf(pCmdHlp, "Failed to disassemble instruction, skipping one byte.\n");
            if (RT_FAILURE(rc))
                return rc;
            if (cTries-- > 0)
                return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "Too many disassembly failures. Giving up");
            cbInstr = 1;
        }

        /* advance */
        if (iRangeLeft < 0)             /* 'r' */
            break;
        if (pDbgc->DisasmPos.enmRangeType == DBGCVAR_RANGE_ELEMENTS)
            iRangeLeft--;
        else
            iRangeLeft -= cbInstr;
        rc = DBGCCmdHlpEval(pCmdHlp, &pDbgc->DisasmPos, "(%Dv) + %x", &pDbgc->DisasmPos, cbInstr);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGCCmdHlpEval(,,'(%Dv) + %x')", &pDbgc->DisasmPos, cbInstr);
        if (iRangeLeft <= 0)
            break;
        fFlags &= ~(DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_CURRENT_HYPER);
    }

    NOREF(pCmd);
    return VINF_SUCCESS;
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
static DECLCALLBACK(int) dbgcCmdListSource(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC  pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

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
            PVMCPU pVCpu = VMMGetCpuById(pVM, pDbgc->idCpu);
            pDbgc->SourcePos.enmType     = DBGCVAR_TYPE_GC_FAR;
            pDbgc->SourcePos.u.GCFar.off = pDbgc->fRegCtxGuest ? CPUMGetGuestEIP(pVCpu) : CPUMGetHyperEIP(pVCpu);
            pDbgc->SourcePos.u.GCFar.sel = pDbgc->fRegCtxGuest ? CPUMGetGuestCS(pVCpu)  : CPUMGetHyperCS(pVCpu);
        }
        pDbgc->SourcePos.enmRangeType = DBGCVAR_RANGE_NONE;
    }
    else
        pDbgc->SourcePos = paArgs[0];
    pDbgc->pLastPos = &pDbgc->SourcePos;

    /*
     * Ensure the source address is flat GC.
     */
    switch (pDbgc->SourcePos.enmType)
    {
        case DBGCVAR_TYPE_GC_FLAT:
            break;
        case DBGCVAR_TYPE_GC_PHYS:
        case DBGCVAR_TYPE_GC_FAR:
        case DBGCVAR_TYPE_HC_FLAT:
        case DBGCVAR_TYPE_HC_PHYS:
        {
            int rc = DBGCCmdHlpEval(pCmdHlp, &pDbgc->SourcePos, "%%(%Dv)", &pDbgc->SourcePos);
            if (RT_FAILURE(rc))
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
    if (iRangeLeft == 0)                /* kludge for 'r'. */
        iRangeLeft = -1;
    for (;;)
    {
        /*
         * Get line info.
         */
        DBGFLINE    Line;
        RTGCINTPTR  off;
        int rc = DBGFR3LineByAddr(pVM, pDbgc->SourcePos.u.GCFlat, &off, &Line);
        if (RT_FAILURE(rc))
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
                if (RT_FAILURE(rc))
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
                        while (cch > 0 && (szLine[cch - 1] == '\r' ||  szLine[cch - 1] == '\n' || RT_C_IS_SPACE(szLine[cch - 1])) )
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
                if (RT_FAILURE(rc))
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
        rc = DBGCCmdHlpEval(pCmdHlp, &pDbgc->SourcePos, "(%Dv) + %x", &pDbgc->SourcePos, 1);
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Expression: (%Dv) + %x\n", &pDbgc->SourcePos, 1);
        if (iRangeLeft <= 0)
            break;
    }

    NOREF(pCmd);
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
static DECLCALLBACK(int) dbgcCmdReg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    if (!pDbgc->fRegCtxGuest)
        return dbgcCmdRegHyper(pCmd, pCmdHlp, pVM, paArgs, cArgs);
    return dbgcCmdRegGuest(pCmd, pCmdHlp, pVM, paArgs, cArgs);
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
static DECLCALLBACK(int) dbgcCmdRegCommon(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs,
                                          const char *pszPrefix)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    Assert(cArgs == 1 || cArgs == 2); /* cArgs == 0 is handled by the caller */
    if (   paArgs[0].enmType != DBGCVAR_TYPE_STRING
        && paArgs[0].enmType != DBGCVAR_TYPE_SYMBOL)
        return DBGCCmdHlpPrintf(pCmdHlp, "internal error: The parser doesn't do its job properly yet.. Try drop the '@' or/and quote the register name\n");

    /*
     * Parse the register name and kind.
     */
    const char *pszReg = paArgs[0].u.pszString;
    if (*pszReg == '@')
        pszReg++;
    VMCPUID idCpu = pDbgc->idCpu;
    if (*pszPrefix)
        idCpu |= DBGFREG_HYPER_VMCPUID;
    if (*pszReg == '.')
    {
        pszReg++;
        idCpu |= DBGFREG_HYPER_VMCPUID;
    }
    const char * const pszActualPrefix = idCpu & DBGFREG_HYPER_VMCPUID ? "." : "";

    /*
     * Query the register type & value (the setter needs the type).
     */
    DBGFREGVALTYPE  enmType;
    DBGFREGVAL      Value;
    int rc = DBGFR3RegNmQuery(pVM, idCpu, pszReg, &Value, &enmType);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_DBGF_REGISTER_NOT_FOUND)
            return DBGCCmdHlpVBoxError(pCmdHlp, VERR_INVALID_PARAMETER, "Unknown register: '%s%s'.\n",
                                       pszActualPrefix,  pszReg);
        return DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGFR3RegNmQuery failed querying '%s%s': %Rrc.\n",
                                   pszActualPrefix,  pszReg, rc);
    }
    if (cArgs == 1)
    {
        /*
         * Show the register.
         */
        char szValue[160];
        rc = DBGFR3RegFormatValue(szValue, sizeof(szValue), &Value, enmType, true /*fSpecial*/);
        if (RT_SUCCESS(rc))
            rc = DBGCCmdHlpPrintf(pCmdHlp, "%s%s=%s\n", pszActualPrefix, pszReg, szValue);
        else
            rc = DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGFR3RegFormatValue failed: %Rrc.\n", rc);
    }
    else if (cArgs == 2)
    {
        /*
         * Modify the register.
         */
        if (   paArgs[1].enmType == DBGCVAR_TYPE_STRING
            || paArgs[1].enmType == DBGCVAR_TYPE_SYMBOL)
            return DBGCCmdHlpPrintf(pCmdHlp, "internal error: The parser doesn't do its job properly on the 2nd argument yet...\n");
        if (enmType != DBGFREGVALTYPE_DTR)
        {
            enmType = DBGFREGVALTYPE_U64;
            rc = DBGCCmdHlpVarToNumber(pCmdHlp, &paArgs[1], &Value.u64);
        }
        else
        {
            enmType = DBGFREGVALTYPE_DTR;
            rc = DBGCCmdHlpVarToNumber(pCmdHlp, &paArgs[1], &Value.dtr.u64Base);
            if (RT_SUCCESS(rc) && paArgs[1].enmRangeType != DBGCVAR_RANGE_NONE)
                Value.dtr.u32Limit = (uint32_t)paArgs[1].u64Range;
        }
        if (RT_SUCCESS(rc))
        {
            rc = DBGFR3RegNmSet(pVM, idCpu, pszReg, &Value, enmType);
            if (RT_FAILURE(rc))
                rc = DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGFR3RegNmSet failed settings '%s%s': %Rrc\n",
                                         pszActualPrefix, pszReg, rc);
        }
        else
            rc = DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGFR3RegFormatValue failed: %Rrc.\n", rc);
    }
    else
    {
        NOREF(pCmd); NOREF(paArgs);
        rc = DBGCCmdHlpPrintf(pCmdHlp, "Huh? cArgs=%d Expected 0, 1 or 2!\n", cArgs);
    }
    return rc;
}


/**
 * The 'rg', 'rg64' and 'rg32' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdRegGuest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Show all registers our selves.
     */
    if (cArgs == 0)
    {
        PDBGC       pDbgc      = DBGC_CMDHLP2DBGC(pCmdHlp);
        bool const  f64BitMode = !strcmp(pCmd->pszCmd, "rg64")
                              || (   !strcmp(pCmd->pszCmd, "rg32")
                                  && CPUMIsGuestIn64BitCodeEx(CPUMQueryGuestCtxPtr(VMMGetCpuById(pVM, pDbgc->idCpu))));
        char        szDisAndRegs[8192];
        int         rc;

        if (pDbgc->fRegTerse)
        {
            if (f64BitMode)
                rc = DBGFR3RegPrintf(pVM, pDbgc->idCpu, &szDisAndRegs[0], sizeof(szDisAndRegs),
                                     "u %016VR{rip} L 0\n"
                                     "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                                     "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                                     "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                                     "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                                     "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                                     "cs=%04VR{cs} ds=%04VR{ds} es=%04VR{es} fs=%04VR{fs} gs=%04VR{gs} ss=%04VR{ss}                     rflags=%08VR{rflags}\n");
            else
                rc = DBGFR3RegPrintf(pVM, pDbgc->idCpu, szDisAndRegs, sizeof(szDisAndRegs),
                                     "u %04VR{cs}:%08VR{eip} L 0\n"
                                     "eax=%08VR{eax} ebx=%08VR{ebx} ecx=%08VR{ecx} edx=%08VR{edx} esi=%08VR{esi} edi=%08VR{edi}\n"
                                     "eip=%08VR{eip} esp=%08VR{esp} ebp=%08VR{ebp} %VRF{eflags}\n"
                                     "cs=%04VR{cs} ds=%04VR{ds} es=%04VR{es} fs=%04VR{fs} gs=%04VR{gs} ss=%04VR{ss}               eflags=%08VR{eflags}\n");
        }
        else
        {
            if (f64BitMode)
                rc = DBGFR3RegPrintf(pVM, pDbgc->idCpu, &szDisAndRegs[0], sizeof(szDisAndRegs),
                                     "u %016VR{rip} L 0\n"
                                     "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                                     "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                                     "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                                     "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                                     "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                                     "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                                     "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                                     "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                                     "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                                     "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                                     "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                                     "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                                     "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                                     "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                                     "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                                     "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                                     "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                                     "        efer=%016VR{efer}\n"
                                     "         pat=%016VR{pat}\n"
                                     "     sf_mask=%016VR{sf_mask}\n"
                                     "krnl_gs_base=%016VR{krnl_gs_base}\n"
                                     "       lstar=%016VR{lstar}\n"
                                     "        star=%016VR{star} cstar=%016VR{cstar}\n"
                                     "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                                     );
            else
                rc = DBGFR3RegPrintf(pVM, pDbgc->idCpu, szDisAndRegs, sizeof(szDisAndRegs),
                                     "u %04VR{cs}:%08VR{eip} L 0\n"
                                     "eax=%08VR{eax} ebx=%08VR{ebx} ecx=%08VR{ecx} edx=%08VR{edx} esi=%08VR{esi} edi=%08VR{edi}\n"
                                     "eip=%08VR{eip} esp=%08VR{esp} ebp=%08VR{ebp} %VRF{eflags}\n"
                                     "cs={%04VR{cs} base=%08VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} dr0=%08VR{dr0} dr1=%08VR{dr1}\n"
                                     "ds={%04VR{ds} base=%08VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} dr2=%08VR{dr2} dr3=%08VR{dr3}\n"
                                     "es={%04VR{es} base=%08VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} dr6=%08VR{dr6} dr7=%08VR{dr7}\n"
                                     "fs={%04VR{fs} base=%08VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr0=%08VR{cr0} cr2=%08VR{cr2}\n"
                                     "gs={%04VR{gs} base=%08VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr3=%08VR{cr3} cr4=%08VR{cr4}\n"
                                     "ss={%04VR{ss} base=%08VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}} cr8=%08VR{cr8}\n"
                                     "gdtr=%08VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%08VR{idtr_base}:%04VR{idtr_lim}  eflags=%08VR{eflags}\n"
                                     "ldtr={%04VR{ldtr} base=%08VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%04VR{ldtr_attr}}\n"
                                     "tr  ={%04VR{tr} base=%08VR{tr_base} limit=%08VR{tr_lim} flags=%04VR{tr_attr}}\n"
                                     "sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                                     "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                                     );
        }
        if (RT_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGFR3RegPrintf failed");
        char *pszRegs = strchr(szDisAndRegs, '\n');
        *pszRegs++ = '\0';
        rc = DBGCCmdHlpPrintf(pCmdHlp, "%s", pszRegs);

        /*
         * Disassemble one instruction at cs:[r|e]ip.
         */
        return pCmdHlp->pfnExec(pCmdHlp, "%s", szDisAndRegs);
    }
    return dbgcCmdRegCommon(pCmd, pCmdHlp, pVM, paArgs, cArgs, "");
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
static DECLCALLBACK(int) dbgcCmdRegHyper(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Show all registers our selves.
     */
    if (cArgs == 0)
    {
        PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
        char    szDisAndRegs[8192];
        int     rc;

        if (pDbgc->fRegTerse)
            rc = DBGFR3RegPrintf(pVM, pDbgc->idCpu | DBGFREG_HYPER_VMCPUID, szDisAndRegs, sizeof(szDisAndRegs),
                                 "u %VR{cs}:%VR{eip} L 0\n"
                                 ".eax=%08VR{eax} .ebx=%08VR{ebx} .ecx=%08VR{ecx} .edx=%08VR{edx} .esi=%08VR{esi} .edi=%08VR{edi}\n"
                                 ".eip=%08VR{eip} .esp=%08VR{esp} .ebp=%08VR{ebp} .%VRF{eflags}\n"
                                 ".cs=%04VR{cs} .ds=%04VR{ds} .es=%04VR{es} .fs=%04VR{fs} .gs=%04VR{gs} .ss=%04VR{ss}              .eflags=%08VR{eflags}\n");
        else
            rc = DBGFR3RegPrintf(pVM, pDbgc->idCpu | DBGFREG_HYPER_VMCPUID, szDisAndRegs, sizeof(szDisAndRegs),
                                 "u %04VR{cs}:%08VR{eip} L 0\n"
                                 ".eax=%08VR{eax} .ebx=%08VR{ebx} .ecx=%08VR{ecx} .edx=%08VR{edx} .esi=%08VR{esi} .edi=%08VR{edi}\n"
                                 ".eip=%08VR{eip} .esp=%08VR{esp} .ebp=%08VR{ebp} .%VRF{eflags}\n"
                                 ".cs={%04VR{cs} base=%08VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} .dr0=%08VR{dr0} .dr1=%08VR{dr1}\n"
                                 ".ds={%04VR{ds} base=%08VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} .dr2=%08VR{dr2} .dr3=%08VR{dr3}\n"
                                 ".es={%04VR{es} base=%08VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} .dr6=%08VR{dr6} .dr6=%08VR{dr6}\n"
                                 ".fs={%04VR{fs} base=%08VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} .cr3=%016VR{cr3}\n"
                                 ".gs={%04VR{gs} base=%08VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}}\n"
                                 ".ss={%04VR{ss} base=%08VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                                 ".gdtr=%08VR{gdtr_base}:%04VR{gdtr_lim}  .idtr=%08VR{idtr_base}:%04VR{idtr_lim}  .eflags=%08VR{eflags}\n"
                                 ".ldtr={%04VR{ldtr} base=%08VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%04VR{ldtr_attr}}\n"
                                 ".tr  ={%04VR{tr} base=%08VR{tr_base} limit=%08VR{tr_lim} flags=%04VR{tr_attr}}\n"
                                 );
        if (RT_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGFR3RegPrintf failed");
        char *pszRegs = strchr(szDisAndRegs, '\n');
        *pszRegs++ = '\0';
        rc = DBGCCmdHlpPrintf(pCmdHlp, "%s", pszRegs);

        /*
         * Disassemble one instruction at cs:[r|e]ip.
         */
        return pCmdHlp->pfnExec(pCmdHlp, "%s", szDisAndRegs);
    }
    return dbgcCmdRegCommon(pCmd, pCmdHlp, pVM, paArgs, cArgs, ".");
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
static DECLCALLBACK(int) dbgcCmdRegTerse(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    NOREF(pCmd); NOREF(pVM); NOREF(paArgs); NOREF(cArgs);

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
static DECLCALLBACK(int) dbgcCmdTrace(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    int rc = DBGFR3Step(pVM, pDbgc->idCpu);
    if (RT_SUCCESS(rc))
        pDbgc->fReady = false;
    else
        rc = pDbgc->CmdHlp.pfnVBoxError(&pDbgc->CmdHlp, rc, "When trying to single step VM %p\n", pDbgc->pVM);

    NOREF(pCmd); NOREF(paArgs); NOREF(cArgs);
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
static DECLCALLBACK(int) dbgcCmdStack(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Figure which context we're called for and start walking that stack.
     */
    int                 rc;
    PCDBGFSTACKFRAME    pFirstFrame;
    bool const          fGuest = pCmd->pszCmd[1] == 'g'
                              || (!pCmd->pszCmd[1] && pDbgc->fRegCtxGuest);
    rc = DBGFR3StackWalkBegin(pVM, pDbgc->idCpu, fGuest ? DBGFCODETYPE_GUEST : DBGFCODETYPE_HYPER, &pFirstFrame);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Failed to begin stack walk, rc=%Rrc\n", rc);

    /*
     * Print header.
     *                                      12345678 12345678 0023:87654321 12345678 87654321 12345678 87654321 symbol
     */
    uint32_t fBitFlags = 0;
    for (PCDBGFSTACKFRAME pFrame = pFirstFrame;
         pFrame;
         pFrame = DBGFR3StackWalkNext(pFrame))
    {
        uint32_t const fCurBitFlags = pFrame->fFlags & (DBGFSTACKFRAME_FLAGS_16BIT | DBGFSTACKFRAME_FLAGS_32BIT | DBGFSTACKFRAME_FLAGS_64BIT);
        if (fCurBitFlags & DBGFSTACKFRAME_FLAGS_16BIT)
        {
            if (fCurBitFlags != fBitFlags)
                pCmdHlp->pfnPrintf(pCmdHlp,  NULL, "SS:BP     Ret SS:BP Ret CS:EIP    Arg0     Arg1     Arg2     Arg3     CS:EIP / Symbol [line]\n");
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04RX16:%04RX16 %04RX16:%04RX16 %04RX32:%08RX32 %08RX32 %08RX32 %08RX32 %08RX32",
                                    pFrame->AddrFrame.Sel,
                                    (uint16_t)pFrame->AddrFrame.off,
                                    pFrame->AddrReturnFrame.Sel,
                                    (uint16_t)pFrame->AddrReturnFrame.off,
                                    (uint32_t)pFrame->AddrReturnPC.Sel,
                                    (uint32_t)pFrame->AddrReturnPC.off,
                                    pFrame->Args.au32[0],
                                    pFrame->Args.au32[1],
                                    pFrame->Args.au32[2],
                                    pFrame->Args.au32[3]);
        }
        else if (fCurBitFlags & DBGFSTACKFRAME_FLAGS_32BIT)
        {
            if (fCurBitFlags != fBitFlags)
                pCmdHlp->pfnPrintf(pCmdHlp,  NULL, "EBP      Ret EBP  Ret CS:EIP    Arg0     Arg1     Arg2     Arg3     CS:EIP / Symbol [line]\n");
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%08RX32 %08RX32 %04RX32:%08RX32 %08RX32 %08RX32 %08RX32 %08RX32",
                                    (uint32_t)pFrame->AddrFrame.off,
                                    (uint32_t)pFrame->AddrReturnFrame.off,
                                    (uint32_t)pFrame->AddrReturnPC.Sel,
                                    (uint32_t)pFrame->AddrReturnPC.off,
                                    pFrame->Args.au32[0],
                                    pFrame->Args.au32[1],
                                    pFrame->Args.au32[2],
                                    pFrame->Args.au32[3]);
        }
        else if (fCurBitFlags & DBGFSTACKFRAME_FLAGS_64BIT)
        {
            if (fCurBitFlags != fBitFlags)
                pCmdHlp->pfnPrintf(pCmdHlp,  NULL, "RBP              Ret SS:RBP            Ret RIP          CS:RIP / Symbol [line]\n");
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%016RX64 %04RX16:%016RX64 %016RX64",
                                    (uint64_t)pFrame->AddrFrame.off,
                                    pFrame->AddrReturnFrame.Sel,
                                    (uint64_t)pFrame->AddrReturnFrame.off,
                                    (uint64_t)pFrame->AddrReturnPC.off);
        }
        if (RT_FAILURE(rc))
            break;
        if (!pFrame->pSymPC)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                    fCurBitFlags & DBGFSTACKFRAME_FLAGS_64BIT
                                    ? " %RTsel:%016RGv"
                                    : fCurBitFlags & DBGFSTACKFRAME_FLAGS_64BIT
                                    ? " %RTsel:%08RGv"
                                    : " %RTsel:%04RGv"
                                    , pFrame->AddrPC.Sel, pFrame->AddrPC.off);
        else
        {
            RTGCINTPTR offDisp = pFrame->AddrPC.FlatPtr - pFrame->pSymPC->Value; /** @todo this isn't 100% correct for segmented stuff. */
            if (offDisp > 0)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %s+%llx", pFrame->pSymPC->szName, (int64_t)offDisp);
            else if (offDisp < 0)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %s-%llx", pFrame->pSymPC->szName, -(int64_t)offDisp);
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " %s", pFrame->pSymPC->szName);
        }
        if (RT_SUCCESS(rc) && pFrame->pLinePC)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " [%s @ 0i%d]", pFrame->pLinePC->szFilename, pFrame->pLinePC->uLineNo);
        if (RT_SUCCESS(rc))
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
        if (RT_FAILURE(rc))
            break;

        fBitFlags = fCurBitFlags;
    }

    DBGFR3StackWalkEnd(pFirstFrame);

    NOREF(paArgs); NOREF(cArgs);
    return rc;
}


static int dbgcCmdDumpDTWorker64(PDBGCCMDHLP pCmdHlp, PCX86DESC64 pDesc, unsigned iEntry, bool fHyper, bool *pfDblEntry)
{
    /* GUEST64 */
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
            "DownRW", /* 7 Expand-down, Read/Write - Accessed */
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
        uint32_t u32Base = X86DESC_BASE(*pDesc);
        uint32_t cbLimit = X86DESC_LIMIT(*pDesc);
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
            "Ill-1 ", /* 1 0001 Available 16-bit TSS */
            "LDT   ", /* 2 0010 LDT */
            "Ill-3 ", /* 3 0011 Busy 16-bit TSS */
            "Ill-4 ", /* 4 0100 16-bit Call Gate */
            "Ill-5 ", /* 5 0101 Task Gate */
            "Ill-6 ", /* 6 0110 16-bit Interrupt Gate */
            "Ill-7 ", /* 7 0111 16-bit Trap Gate */
            "Ill-8 ", /* 8 1000 Reserved (Illegal) */
            "Tss64A", /* 9 1001 Available 32-bit TSS */
            "Ill-A ", /* A 1010 Reserved (Illegal) */
            "Tss64B", /* B 1011 Busy 32-bit TSS */
            "Call64", /* C 1100 32-bit Call Gate */
            "Ill-D ", /* D 1101 Reserved (Illegal) */
            "Int64 ", /* E 1110 32-bit Interrupt Gate */
            "Trap64"  /* F 1111 32-bit Trap Gate */
        };
        switch (pDesc->Gen.u4Type)
        {
            /* raw */
            case X86_SEL_TYPE_SYS_UNDEFINED:
            case X86_SEL_TYPE_SYS_UNDEFINED2:
            case X86_SEL_TYPE_SYS_UNDEFINED4:
            case X86_SEL_TYPE_SYS_UNDEFINED3:
            case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
            case X86_SEL_TYPE_SYS_286_TSS_BUSY:
            case X86_SEL_TYPE_SYS_286_CALL_GATE:
            case X86_SEL_TYPE_SYS_286_INT_GATE:
            case X86_SEL_TYPE_SYS_286_TRAP_GATE:
            case X86_SEL_TYPE_SYS_TASK_GATE:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s %.8Rhxs   DPL=%d %s%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], pDesc,
                                        pDesc->Gen.u2Dpl, pszPresent, pszHyper);
                break;

            case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
            case X86_SEL_TYPE_SYS_386_TSS_BUSY:
            case X86_SEL_TYPE_SYS_LDT:
            {
                const char *pszBusy        = pDesc->Gen.u4Type & RT_BIT(1) ? "B " : "NB";
                const char *pszBig         = pDesc->Gen.u1DefBig ? "BIG" : "   ";
                const char *pszLong        = pDesc->Gen.u1Long ? "LONG" : "   ";

                uint64_t u32Base = X86DESC64_BASE(*pDesc);
                uint32_t cbLimit = X86DESC_LIMIT(*pDesc);

                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Bas=%016RX64 Lim=%08x DPL=%d %s %s %s %sAVL=%d R=%d%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], u32Base, cbLimit,
                                        pDesc->Gen.u2Dpl, pszPresent, pszBusy, pszLong, pszBig,
                                        pDesc->Gen.u1Available, pDesc->Gen.u1Long | (pDesc->Gen.u1DefBig << 1),
                                        pszHyper);
                if (pfDblEntry)
                    *pfDblEntry = true;
                break;
            }

            case X86_SEL_TYPE_SYS_386_CALL_GATE:
            {
                unsigned cParams = pDesc->au8[4] & 0x1f;
                const char *pszCountOf = pDesc->Gen.u4Type & RT_BIT(3) ? "DC" : "WC";
                RTSEL sel = pDesc->au16[1];
                uint64_t off =    pDesc->au16[0]
                                | ((uint64_t)pDesc->au16[3] << 16)
                                | ((uint64_t)pDesc->Gen.u32BaseHigh3 << 32);
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Sel:Off=%04x:%016RX64     DPL=%d %s %s=%d%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], sel, off,
                                        pDesc->Gen.u2Dpl, pszPresent, pszCountOf, cParams, pszHyper);
                if (pfDblEntry)
                    *pfDblEntry = true;
                break;
            }

            case X86_SEL_TYPE_SYS_386_INT_GATE:
            case X86_SEL_TYPE_SYS_386_TRAP_GATE:
            {
                RTSEL sel = pDesc->au16[1];
                uint64_t off =    pDesc->au16[0]
                                | ((uint64_t)pDesc->au16[3] << 16)
                                | ((uint64_t)pDesc->Gen.u32BaseHigh3 << 32);
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %s Sel:Off=%04x:%016RX64     DPL=%d %s%s\n",
                                        iEntry, s_apszTypes[pDesc->Gen.u4Type], sel, off,
                                        pDesc->Gen.u2Dpl, pszPresent, pszHyper);
                if (pfDblEntry)
                    *pfDblEntry = true;
                break;
            }

            /* impossible, just it's necessary to keep gcc happy. */
            default:
                return VINF_SUCCESS;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Worker function that displays one descriptor entry (GDT, LDT, IDT).
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
            "DownRW", /* 7 Expand-down, Read/Write - Accessed */
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
                unsigned cParams = pDesc->au8[4] & 0x1f;
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
static DECLCALLBACK(int) dbgcCmdDumpDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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
    PDBGC       pDbgc   = DBGC_CMDHLP2DBGC(pCmdHlp);
    PVMCPU      pVCpu   = VMMGetCpuById(pVM, pDbgc->idCpu);
    CPUMMODE    enmMode = CPUMGetGuestMode(pVCpu);
    bool        fGdt    = pCmd->pszCmd[1] == 'g';
    bool        fAll    = pCmd->pszCmd[2] == 'a';
    RTSEL       SelTable = fGdt ? 0 : X86_SEL_LDT;

    DBGCVAR Var;
    if (!cArgs)
    {
        cArgs = 1;
        paArgs = &Var;
        Var.enmType = DBGCVAR_TYPE_NUMBER;
        Var.u.u64Number = 0;
        Var.enmRangeType = DBGCVAR_RANGE_ELEMENTS;
        Var.u64Range = 1024;
    }

    /*
     * Process the arguments.
     */
    for (unsigned i = 0; i < cArgs; i++)
    {
         /*
          * Retrieve the selector value from the argument.
          * The parser may confuse pointers and numbers if more than one
          * argument is given, that that into account.
          */
        /* check that what we got makes sense as we don't trust the parser yet. */
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
                DBGFSELINFO SelInfo;
                int rc = DBGFR3SelQueryInfo(pVM, pDbgc->idCpu, Sel | SelTable, DBGFSELQI_FLAGS_DT_GUEST, &SelInfo);
                if (RT_SUCCESS(rc))
                {
                    if (SelInfo.fFlags & DBGFSELINFO_FLAGS_REAL_MODE)
                        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x RealM   Bas=%04x     Lim=%04x\n",
                                                Sel, (unsigned)SelInfo.GCPtrBase, (unsigned)SelInfo.cbLimit);
                    else if (   fAll
                             || fSingle
                             || SelInfo.u.Raw.Gen.u1Present)
                    {
                        if (enmMode == CPUMMODE_PROTECTED)
                            rc = dbgcCmdDumpDTWorker32(pCmdHlp, &SelInfo.u.Raw, Sel, !!(SelInfo.fFlags & DBGFSELINFO_FLAGS_HYPER));
                        else
                        {
                            bool fDblSkip = false;
                            rc = dbgcCmdDumpDTWorker64(pCmdHlp, &SelInfo.u.Raw64, Sel, !!(SelInfo.fFlags & DBGFSELINFO_FLAGS_HYPER), &fDblSkip);
                            if (fDblSkip)
                                Sel += 4;
                        }
                    }
                }
                else
                {
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%04x %Rrc\n", Sel, rc);
                    if (!fAll)
                        return rc;
                }
                if (RT_FAILURE(rc))
                    return rc;

                /* next */
                Sel += 8;
            }
        }
        else
            pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: %llx is out of bounds\n", u64);
    }

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
static DECLCALLBACK(int) dbgcCmdDumpIDT(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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
    PDBGC       pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    PVMCPU      pVCpu     = VMMGetCpuById(pVM, pDbgc->idCpu);
    uint16_t    cbLimit;
    RTGCUINTPTR GCPtrBase = CPUMGetGuestIDTR(pVCpu, &cbLimit);
    CPUMMODE    enmMode   = CPUMGetGuestMode(pVCpu);
    unsigned    cbEntry;
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
        /* check that what we got makes sense as we don't trust the parser yet. */
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
                if (RT_FAILURE(rc))
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
static DECLCALLBACK(int) dbgcCmdDumpMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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

    pDbgc->pLastPos = &pDbgc->DumpPos;

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
        if (RT_FAILURE(rc))
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
                    if (RT_C_IS_PRINT(u8) && u8 < 127 && u8 >= 32)
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
                    && (    (RT_C_IS_PRINT(u8) && u8 >= 32)
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
        rc = DBGCCmdHlpEval(pCmdHlp, &pDbgc->DumpPos, "(%Dv) + %x", &pDbgc->DumpPos, cb);
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Expression: (%Dv) + %x\n", &pDbgc->DumpPos, cb);
        if (cbLeft <= 0)
            break;
    }

    NOREF(pCmd);
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
 * @param   pfNXE   Where to store the no-execution enabled indicator.
 */
static RTGCPHYS dbgcGetGuestPageMode(PDBGC pDbgc, bool *pfPAE, bool *pfLME, bool *pfPSE, bool *pfPGE, bool *pfNXE)
{
    PVMCPU      pVCpu = VMMGetCpuById(pDbgc->pVM, pDbgc->idCpu);
    RTGCUINTREG cr4   = CPUMGetGuestCR4(pVCpu);
    *pfPSE = !!(cr4 & X86_CR4_PSE);
    *pfPGE = !!(cr4 & X86_CR4_PGE);
    if (cr4 & X86_CR4_PAE)
    {
        *pfPSE = true;
        *pfPAE = true;
    }
    else
        *pfPAE = false;

    *pfLME = CPUMGetGuestMode(pVCpu) == CPUMMODE_LONG;
    *pfNXE = false; /* GUEST64 GUESTNX */
    return CPUMGetGuestCR3(pVCpu);
}


/**
 * Determine the shadow paging mode.
 *
 * @returns cr3.
 * @param   pDbgc   The DBGC instance.
 * @param   pfPAE   Where to store the page address extension indicator.
 * @param   pfLME   Where to store the long mode enabled indicator.
 * @param   pfPSE   Where to store the page size extension indicator.
 * @param   pfPGE   Where to store the page global enabled indicator.
 * @param   pfNXE   Where to store the no-execution enabled indicator.
 */
static RTHCPHYS dbgcGetShadowPageMode(PDBGC pDbgc, bool *pfPAE, bool *pfLME, bool *pfPSE, bool *pfPGE, bool *pfNXE)
{
    PVMCPU pVCpu = VMMGetCpuById(pDbgc->pVM, pDbgc->idCpu);

    *pfPSE = true;
    *pfPGE = false;
    switch (PGMGetShadowMode(pVCpu))
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
    return PGMGetHyperCR3(pVCpu);
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
static DECLCALLBACK(int) dbgcCmdDumpPageDir(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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
     * Setup default argument if none was specified.
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
        /* If it's a number (not an address), it's an index, so convert it to an address. */
        Assert(pCmd->pszCmd[3] != 'a');
        VarDefault = paArgs[0];
        if (fPAE)
            return DBGCCmdHlpPrintf(pCmdHlp, "PDE indexing is only implemented for 32-bit paging.\n");
        if (VarDefault.u.u64Number >= PAGE_SIZE / cbEntry)
            return DBGCCmdHlpPrintf(pCmdHlp, "PDE index is out of range [0..%d].\n", PAGE_SIZE / cbEntry - 1);
        VarDefault.u.u64Number <<= X86_PD_SHIFT;
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
         * Determine the range.
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
        rc = DBGCCmdHlpEval(pCmdHlp, &VarGCPtr, "%%(%Dv)", &paArgs[0]);
        if (RT_FAILURE(rc))
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
            if (RT_FAILURE(rc))
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
            if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Advance.
         */
        VarPDEAddr.u.u64Number += cbEntry;
        if (iEntry != ~0U)
            VarGCPtr.u.GCFlat += fPAE ? RT_BIT_32(X86_PD_PAE_SHIFT) : RT_BIT_32(X86_PD_SHIFT);
    } while (cEntries-- > 0);

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
static DECLCALLBACK(int) dbgcCmdDumpPageDirBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");
    int rc1 = pCmdHlp->pfnExec(pCmdHlp, "dpdg %DV", &paArgs[0]);
    int rc2 = pCmdHlp->pfnExec(pCmdHlp, "dpdh %DV", &paArgs[0]);
    if (RT_FAILURE(rc1))
        return rc1;
    NOREF(pCmd); NOREF(paArgs); NOREF(cArgs);
    return rc2;
}


/**
 * The 'dph*' commands and main part of 'm'.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDumpPageHierarchy(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    if (!pVM)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "No VM.\n");

    /*
     * Figure the context and base flags.
     */
    uint32_t fFlags = DBGFPGDMP_FLAGS_PAGE_INFO | DBGFPGDMP_FLAGS_PRINT_CR3;
    if (pCmd->pszCmd[0] == 'm')
        fFlags |= DBGFPGDMP_FLAGS_GUEST | DBGFPGDMP_FLAGS_SHADOW;
    else if (pCmd->pszCmd[3] == '\0')
        fFlags |= pDbgc->fRegCtxGuest ? DBGFPGDMP_FLAGS_GUEST : DBGFPGDMP_FLAGS_SHADOW;
    else if (pCmd->pszCmd[3] == 'g')
        fFlags |= DBGFPGDMP_FLAGS_GUEST;
    else if (pCmd->pszCmd[3] == 'h')
        fFlags |= DBGFPGDMP_FLAGS_SHADOW;
    else
        AssertFailed();

    if (pDbgc->cPagingHierarchyDumps == 0)
        fFlags |= DBGFPGDMP_FLAGS_HEADER;
    pDbgc->cPagingHierarchyDumps = (pDbgc->cPagingHierarchyDumps + 1) % 42;

    /*
     * Get the range.
     */
    PCDBGCVAR   pRange = cArgs > 0 ? &paArgs[0] : pDbgc->pLastPos;
    RTGCPTR     GCPtrFirst = NIL_RTGCPTR;
    int rc = DBGCCmdHlpVarToFlatAddr(pCmdHlp, pRange, &GCPtrFirst);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Failed to convert %DV to a flat address: %Rrc", pRange, rc);

    uint64_t cbRange;
    rc = DBGCCmdHlpVarGetRange(pCmdHlp, pRange, PAGE_SIZE, PAGE_SIZE * 8, &cbRange);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Failed to obtain the range of %DV: %Rrc", pRange, rc);

    RTGCPTR GCPtrLast = RTGCPTR_MAX - GCPtrFirst;
    if (cbRange >= GCPtrLast)
        GCPtrLast = RTGCPTR_MAX;
    else if (!cbRange)
        GCPtrLast = GCPtrFirst;
    else
        GCPtrLast = GCPtrFirst + cbRange - 1;

    /*
     * Do we have a CR3?
     */
    uint64_t cr3 = 0;
    if (cArgs > 1)
    {
        if ((fFlags & (DBGFPGDMP_FLAGS_GUEST | DBGFPGDMP_FLAGS_SHADOW)) == (DBGFPGDMP_FLAGS_GUEST | DBGFPGDMP_FLAGS_SHADOW))
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "No CR3 or mode arguments when dumping both context, please.");
        if (paArgs[1].enmType != DBGCVAR_TYPE_NUMBER)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "The CR3 argument is not a number: %DV", &paArgs[1]);
        cr3 = paArgs[1].u.u64Number;
    }
    else
        fFlags |= DBGFPGDMP_FLAGS_CURRENT_CR3;

    /*
     * Do we have a mode?
     */
    if (cArgs > 2)
    {
        if (paArgs[2].enmType != DBGCVAR_TYPE_STRING)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "The mode argument is not a string: %DV", &paArgs[2]);
        static const struct MODETOFLAGS
        {
            const char *pszName;
            uint32_t    fFlags;
        } s_aModeToFlags[] =
        {
            { "ept",        DBGFPGDMP_FLAGS_EPT },
            { "legacy",     0 },
            { "legacy-np",  DBGFPGDMP_FLAGS_NP },
            { "pse",        DBGFPGDMP_FLAGS_PSE },
            { "pse-np",     DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_NP },
            { "pae",        DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE },
            { "pae-np",     DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_NP },
            { "pae-nx",     DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_NXE },
            { "pae-nx-np",  DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_NXE | DBGFPGDMP_FLAGS_NP },
            { "long",       DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_LME },
            { "long-np",    DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_LME | DBGFPGDMP_FLAGS_NP },
            { "long-nx",    DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_LME | DBGFPGDMP_FLAGS_NXE },
            { "long-nx-np", DBGFPGDMP_FLAGS_PSE | DBGFPGDMP_FLAGS_PAE | DBGFPGDMP_FLAGS_LME | DBGFPGDMP_FLAGS_NXE | DBGFPGDMP_FLAGS_NP }
        };
        int i = RT_ELEMENTS(s_aModeToFlags);
        while (i-- > 0)
            if (!strcmp(s_aModeToFlags[i].pszName, paArgs[2].u.pszString))
            {
                fFlags |= s_aModeToFlags[i].fFlags;
                break;
            }
        if (i < 0)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Unknown mode: \"%s\"", paArgs[2].u.pszString);
    }
    else
        fFlags |= DBGFPGDMP_FLAGS_CURRENT_MODE;

    /*
     * Call the worker.
     */
    rc = DBGFR3PagingDumpEx(pVM, pDbgc->idCpu, fFlags, cr3, GCPtrFirst, GCPtrLast, 99 /*cMaxDepth*/,
                            DBGCCmdHlpGetDbgfOutputHlp(pCmdHlp));
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "DBGFR3PagingDumpEx: %Rrc\n", rc);
    return VINF_SUCCESS;
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
static DECLCALLBACK(int) dbgcCmdDumpPageTable(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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
         * Determine the range.
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
        rc = DBGCCmdHlpEval(pCmdHlp, &VarGCPtr, "%%(%Dv)", &paArgs[0]);
        if (RT_FAILURE(rc))
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
            if (RT_FAILURE(rc))
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
            if (RT_FAILURE(rc))
                return DBGCCmdHlpVBoxError(pCmdHlp, rc, "Reading PDPE memory at %DV.\n", &VarCur);
            if (!Pdpe.n.u1Present)
                return DBGCCmdHlpPrintf(pCmdHlp, "Page directory is not present for %Dv.\n", &VarGCPtr);

            VarCur.u.u64Number = Pdpe.u & X86_PDPE_PG_MASK;

            /* Page directory (PAE). */
            X86PDEPAE Pde;
            VarCur.u.u64Number += ((VarGCPtr.u.GCFlat >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK) * sizeof(Pde);
            rc = pCmdHlp->pfnMemRead(pCmdHlp, pVM, &Pde, sizeof(Pde), &VarCur, NULL);
            if (RT_FAILURE(rc))
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
            if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Advance.
         */
        VarPTEAddr.u.u64Number += cbEntry;
        if (iEntry != ~0U)
            VarGCPtr.u.GCFlat += PAGE_SIZE;
    } while (cEntries-- > 0);

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
static DECLCALLBACK(int) dbgcCmdDumpPageTableBoth(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");
    int rc1 = pCmdHlp->pfnExec(pCmdHlp, "dptg %DV", &paArgs[0]);
    int rc2 = pCmdHlp->pfnExec(pCmdHlp, "dpth %DV", &paArgs[0]);
    if (RT_FAILURE(rc1))
        return rc1;
    NOREF(pCmd); NOREF(cArgs);
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
static DECLCALLBACK(int) dbgcCmdDumpTSS(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    int   rc;

    if (!pVM)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "No VM.\n");
    if (    cArgs > 1
        ||  (cArgs == 1 && paArgs[0].enmType == DBGCVAR_TYPE_STRING)
        ||  (cArgs == 1 && paArgs[0].enmType == DBGCVAR_TYPE_SYMBOL))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "internal error: The parser doesn't do its job properly yet...\n");

    /*
     * Check if the command indicates the type.
     */
    enum { kTss16, kTss32, kTss64, kTssToBeDetermined } enmTssType = kTssToBeDetermined;
    if (!strcmp(pCmd->pszCmd, "dt16"))
        enmTssType = kTss16;
    else if (!strcmp(pCmd->pszCmd, "dt32"))
        enmTssType = kTss32;
    else if (!strcmp(pCmd->pszCmd, "dt64"))
        enmTssType = kTss64;

    /*
     * We can get a TSS selector (number), a far pointer using a TSS selector, or some kind of TSS pointer.
     */
    uint32_t SelTss = UINT32_MAX;
    DBGCVAR  VarTssAddr;
    if (cArgs == 0)
    {
        /** @todo consider querying the hidden bits instead (missing API). */
        uint16_t SelTR;
        rc = DBGFR3RegCpuQueryU16(pVM, pDbgc->idCpu, DBGFREG_TR, &SelTR);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Failed to query TR, rc=%Rrc\n", rc);
        DBGCVAR_INIT_GC_FAR(&VarTssAddr, SelTR, 0);
        SelTss = SelTR;
    }
    else if (paArgs[0].enmType == DBGCVAR_TYPE_NUMBER)
    {
        if (paArgs[0].u.u64Number < 0xffff)
            DBGCVAR_INIT_GC_FAR(&VarTssAddr, (RTSEL)paArgs[0].u.u64Number, 0);
        else
        {
            if (paArgs[0].enmRangeType == DBGCVAR_RANGE_ELEMENTS)
                return DBGCCmdHlpFail(pCmdHlp, pCmd, "Element count doesn't combine with a TSS address.\n");
            DBGCVAR_INIT_GC_FLAT(&VarTssAddr, paArgs[0].u.u64Number);
            if (paArgs[0].enmRangeType == DBGCVAR_RANGE_BYTES)
            {
                VarTssAddr.enmRangeType = paArgs[0].enmRangeType;
                VarTssAddr.u64Range     = paArgs[0].u64Range;
            }
        }
    }
    else
        VarTssAddr = paArgs[0];

    /*
     * Deal with TSS:ign by means of the GDT.
     */
    if (VarTssAddr.enmType == DBGCVAR_TYPE_GC_FAR)
    {
        SelTss = VarTssAddr.u.GCFar.sel;
        DBGFSELINFO SelInfo;
        rc = DBGFR3SelQueryInfo(pVM, pDbgc->idCpu, VarTssAddr.u.GCFar.sel, DBGFSELQI_FLAGS_DT_GUEST, &SelInfo);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "DBGFR3SelQueryInfo(,%u,%d,,) -> %Rrc.\n",
                                  pDbgc->idCpu, VarTssAddr.u.GCFar.sel, rc);

        if (SelInfo.u.Raw.Gen.u1DescType)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "%04x is not a TSS selector. (!sys)\n", VarTssAddr.u.GCFar.sel);

        switch (SelInfo.u.Raw.Gen.u4Type)
        {
            case X86_SEL_TYPE_SYS_286_TSS_BUSY:
            case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
                if (enmTssType == kTssToBeDetermined)
                    enmTssType = kTss16;
                break;

            case X86_SEL_TYPE_SYS_386_TSS_BUSY:  /* AMD64 too */
            case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
                if (enmTssType == kTssToBeDetermined)
                    enmTssType = SelInfo.fFlags & DBGFSELINFO_FLAGS_LONG_MODE ? kTss64 : kTss32;
                break;

            default:
                return DBGCCmdHlpFail(pCmdHlp, pCmd, "%04x is not a TSS selector. (type=%x)\n",
                                      VarTssAddr.u.GCFar.sel, SelInfo.u.Raw.Gen.u4Type);
        }

        DBGCVAR_INIT_GC_FLAT(&VarTssAddr, SelInfo.GCPtrBase);
        DBGCVAR_SET_RANGE(&VarTssAddr, DBGCVAR_RANGE_BYTES, RT_MAX(SelInfo.cbLimit + 1, SelInfo.cbLimit));
    }

    /*
     * Determine the TSS type if none is currently given.
     */
    if (enmTssType == kTssToBeDetermined)
    {
        if (    VarTssAddr.u64Range > 0
            &&  VarTssAddr.u64Range < sizeof(X86TSS32) - 4)
            enmTssType = kTss16;
        else
        {
            uint64_t uEfer;
            rc = DBGFR3RegCpuQueryU64(pVM, pDbgc->idCpu, DBGFREG_MSR_K6_EFER, &uEfer);
            if (   RT_FAILURE(rc)
                || !(uEfer &  MSR_K6_EFER_LMA) )
                enmTssType = kTss32;
            else
                enmTssType = kTss64;
        }
    }

    /*
     * Figure the min/max sizes.
     * ASSUMES max TSS size is 64 KB.
     */
    uint32_t cbTssMin;
    uint32_t cbTssMax;
    switch (enmTssType)
    {
        case kTss16:
            cbTssMin = cbTssMax = sizeof(X86TSS16);
            break;
        case kTss32:
            cbTssMin = RT_OFFSETOF(X86TSS32, IntRedirBitmap);
            cbTssMax = _64K;
            break;
        case kTss64:
            cbTssMin = RT_OFFSETOF(X86TSS64, IntRedirBitmap);
            cbTssMax = _64K;
            break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }
    uint32_t cbTss = VarTssAddr.enmRangeType == DBGCVAR_RANGE_BYTES ? (uint32_t)VarTssAddr.u64Range : 0;
    if (cbTss == 0)
        cbTss = cbTssMin;
    else if (cbTss < cbTssMin)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Minimum TSS size is %u bytes, you specified %llu (%llx) bytes.\n",
                              cbTssMin, VarTssAddr.u64Range, VarTssAddr.u64Range);
    else if (cbTss > cbTssMax)
        cbTss = cbTssMax;
    DBGCVAR_SET_RANGE(&VarTssAddr, DBGCVAR_RANGE_BYTES, cbTss);

    /*
     * Read the TSS into a temporary buffer.
     */
    uint8_t  abBuf[_64K];
    size_t   cbTssRead;
    rc = DBGCCmdHlpMemRead(pCmdHlp, pVM, abBuf, cbTss, &VarTssAddr, &cbTssRead);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Failed to read TSS at %Dv: %Rrc\n", &VarTssAddr, rc);
    if (cbTssRead < cbTssMin)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Failed to read essential parts of the TSS (read %zu, min %zu).\n",
                              cbTssRead, cbTssMin);
    if (cbTssRead < cbTss)
        memset(&abBuf[cbTssRead], 0xff, cbTss - cbTssRead);


    /*
     * Format the TSS.
     */
    uint16_t offIoBitmap;
    switch (enmTssType)
    {
        case kTss16:
        {
            PCX86TSS16 pTss = (PCX86TSS16)&abBuf[0];
            if (SelTss != UINT32_MAX)
                DBGCCmdHlpPrintf(pCmdHlp, "%04x TSS16 at %Dv\n", SelTss, &VarTssAddr);
            else
                DBGCCmdHlpPrintf(pCmdHlp, "TSS16 at %Dv\n", &VarTssAddr);
            DBGCCmdHlpPrintf(pCmdHlp,
                             "ax=%04x bx=%04x cx=%04x dx=%04x si=%04x di=%04x\n"
                             "ip=%04x sp=%04x bp=%04x\n"
                             "cs=%04x ss=%04x ds=%04x es=%04x      flags=%04x\n"
                             "ss:sp0=%04x:%04x ss:sp1=%04x:%04x ss:sp2=%04x:%04x\n"
                             "prev=%04x ldtr=%04x\n"
                             ,
                             pTss->ax, pTss->bx, pTss->cx, pTss->dx, pTss->si, pTss->di,
                             pTss->ip, pTss->sp, pTss->bp,
                             pTss->cs, pTss->ss, pTss->ds, pTss->es, pTss->flags,
                             pTss->ss0, pTss->sp0,  pTss->ss1, pTss->sp1,  pTss->ss2, pTss->sp2,
                             pTss->selPrev, pTss->selLdt);
            if (pTss->cs != 0)
                pCmdHlp->pfnExec(pCmdHlp, "u %04x:%04x L 0", pTss->cs, pTss->ip);
            offIoBitmap = 0;
            break;
        }

        case kTss32:
        {
            PCX86TSS32 pTss = (PCX86TSS32)&abBuf[0];
            if (SelTss != UINT32_MAX)
                DBGCCmdHlpPrintf(pCmdHlp, "%04x TSS32 at %Dv (min=%04x)\n", SelTss, &VarTssAddr, cbTssMin);
            else
                DBGCCmdHlpPrintf(pCmdHlp, "TSS32 at %Dv  (min=%04x)\n", &VarTssAddr, cbTssMin);
            DBGCCmdHlpPrintf(pCmdHlp,
                             "eax=%08x bx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
                             "eip=%08x esp=%08x ebp=%08x\n"
                             "cs=%04x  ss=%04x  ds=%04x  es=%04x  fs=%04x  gs=%04x         eflags=%08x\n"
                             "ss:esp0=%04x:%08x ss:esp1=%04x:%08x ss:esp2=%04x:%08x\n"
                             "prev=%04x ldtr=%04x cr3=%08x debug=%u iomap=%04x\n"
                             ,
                             pTss->eax, pTss->ebx, pTss->ecx, pTss->edx, pTss->esi, pTss->edi,
                             pTss->eip, pTss->esp, pTss->ebp,
                             pTss->cs, pTss->ss, pTss->ds, pTss->es, pTss->fs, pTss->gs, pTss->eflags,
                             pTss->ss0, pTss->esp0,  pTss->ss1, pTss->esp1,  pTss->ss2, pTss->esp2,
                             pTss->selPrev, pTss->selLdt, pTss->cr3, pTss->fDebugTrap, pTss->offIoBitmap);
            if (pTss->cs != 0)
                pCmdHlp->pfnExec(pCmdHlp, "u %04x:%08x L 0", pTss->cs, pTss->eip);
            offIoBitmap = pTss->offIoBitmap;
            break;
        }

        case kTss64:
        {
            PCX86TSS64 pTss = (PCX86TSS64)&abBuf[0];
            if (SelTss != UINT32_MAX)
                DBGCCmdHlpPrintf(pCmdHlp, "%04x TSS64 at %Dv (min=%04x)\n", SelTss, &VarTssAddr, cbTssMin);
            else
                DBGCCmdHlpPrintf(pCmdHlp, "TSS64 at %Dv (min=%04x)\n", &VarTssAddr, cbTssMin);
            DBGCCmdHlpPrintf(pCmdHlp,
                             "rsp0=%016RX16 rsp1=%016RX16 rsp2=%016RX16\n"
                             "ist1=%016RX16 ist2=%016RX16\n"
                             "ist3=%016RX16 ist4=%016RX16\n"
                             "ist5=%016RX16 ist6=%016RX16\n"
                             "ist7=%016RX16 iomap=%04x\n"
                             ,
                             pTss->rsp0, pTss->rsp1, pTss->rsp2,
                             pTss->ist1, pTss->ist2,
                             pTss->ist3, pTss->ist4,
                             pTss->ist5, pTss->ist6,
                             pTss->ist7, pTss->offIoBitmap);
            offIoBitmap = pTss->offIoBitmap;
            break;
        }

        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }

    /*
     * Dump the interrupt redirection bitmap.
     */
    if (enmTssType != kTss16)
    {
        if (   offIoBitmap > cbTssMin
            && offIoBitmap < cbTss)  /** @todo check exactly what the edge cases are here. */
        {
            if (offIoBitmap - cbTssMin >= 32)
            {
                DBGCCmdHlpPrintf(pCmdHlp, "Interrupt redirection:\n");
                uint8_t const *pbIntRedirBitmap = &abBuf[offIoBitmap - 32];
                uint32_t    iStart = 0;
                bool        fPrev  = ASMBitTest(pbIntRedirBitmap, 0); /* LE/BE issue */
                for (uint32_t i = 0; i < 256; i++)
                {
                    bool fThis = ASMBitTest(pbIntRedirBitmap, i);
                    if (fThis != fPrev)
                    {
                        DBGCCmdHlpPrintf(pCmdHlp, "%02x-%02x %s\n", iStart, i - 1, fPrev ? "Protected mode" : "Redirected");
                        fPrev  = fThis;
                        iStart = i;
                    }
                }
                if (iStart != 255)
                    DBGCCmdHlpPrintf(pCmdHlp, "%02x-%02x %s\n", iStart, 255, fPrev ? "Protected mode" : "Redirected");
            }
            else
                DBGCCmdHlpPrintf(pCmdHlp, "Invalid interrupt redirection bitmap size: %u (%#x), expected 32 bytes.\n",
                                 offIoBitmap - cbTssMin, offIoBitmap - cbTssMin);
        }
        else if (offIoBitmap > 0)
            DBGCCmdHlpPrintf(pCmdHlp, "No interrupt redirection bitmap (-%#x)\n", cbTssMin - offIoBitmap);
        else
            DBGCCmdHlpPrintf(pCmdHlp, "No interrupt redirection bitmap\n");
    }

    /*
     * Dump the I/O permission bitmap if present. The IOPM cannot start below offset 0x64
     * (that applies to both 32-bit and 64-bit TSSs since their size is the same).
     */
    if (enmTssType != kTss16)
    {
        if (offIoBitmap < cbTss && offIoBitmap >= 0x64)
        {
            uint32_t        cPorts      = RT_MIN((cbTss - offIoBitmap) * 8, _64K);
            DBGCVAR         VarAddr;
            DBGCCmdHlpEval(pCmdHlp, &VarAddr, "%DV + %#x", &VarTssAddr, offIoBitmap);
            DBGCCmdHlpPrintf(pCmdHlp, "I/O bitmap at %DV - %#x ports:\n", &VarAddr, cPorts);

            uint8_t const  *pbIoBitmap  = &abBuf[offIoBitmap];
            uint32_t        iStart      = 0;
            bool            fPrev       = ASMBitTest(pbIoBitmap, 0);
            uint32_t        cLine       = 0;
            for (uint32_t i = 1; i < cPorts; i++)
            {
                bool fThis = ASMBitTest(pbIoBitmap, i);
                if (fThis != fPrev)
                {
                    cLine++;
                    DBGCCmdHlpPrintf(pCmdHlp, "%04x-%04x %s%s", iStart, i-1,
                                     fPrev ? "GP" : "OK", (cLine % 6) == 0 ? "\n" : "  ");
                    fPrev  = fThis;
                    iStart = i;
                }
            }
            if (iStart != _64K-1)
                DBGCCmdHlpPrintf(pCmdHlp, "%04x-%04x %s\n", iStart, _64K-1, fPrev ? "GP" : "OK");
        }
        else if (offIoBitmap > 0)
            DBGCCmdHlpPrintf(pCmdHlp, "No I/O bitmap (-%#x)\n", cbTssMin - offIoBitmap);
        else
            DBGCCmdHlpPrintf(pCmdHlp, "No I/O bitmap\n");
    }

    return VINF_SUCCESS;
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
static DECLCALLBACK(int) dbgcCmdMemoryInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    DBGCCmdHlpPrintf(pCmdHlp, "Address: %DV\n", &paArgs[0]);
    if (!pVM)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "No VM.\n");
    return dbgcCmdDumpPageHierarchy(pCmd, pCmdHlp, pVM, paArgs, cArgs);
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
 * The 'eb', 'ew', 'ed' and 'eq' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdEditMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    unsigned iArg;

    /*
     * Validate input.
     */
    if (     cArgs < 2
        ||  !DBGCVAR_ISPOINTER(paArgs[0].enmType))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "internal error: The parser doesn't do its job properly yet... It might help to use the '%%' operator.\n");
    for (iArg = 1; iArg < cArgs; iArg++)
        if (paArgs[iArg].enmType != DBGCVAR_TYPE_NUMBER)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "internal error: The parser doesn't do its job properly yet: Arg #%u is not a number.\n", iArg);
    if (!pVM)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "error: No VM.\n");

    /*
     * Figure out the element size.
     */
    unsigned    cbElement;
    switch (pCmd->pszCmd[1])
    {
        default:
        case 'b':   cbElement = 1; break;
        case 'w':   cbElement = 2; break;
        case 'd':   cbElement = 4; break;
        case 'q':   cbElement = 8; break;
    }

    /*
     * Do setting.
     */
    DBGCVAR Addr = paArgs[0];
    for (iArg = 1;;)
    {
        size_t cbWritten;
        int rc = pCmdHlp->pfnMemWrite(pCmdHlp, pVM, &paArgs[iArg].u, cbElement, &Addr, &cbWritten);
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Writing memory at %DV.\n", &Addr);
        if (cbWritten != cbElement)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Only wrote %u out of %u bytes!\n", cbWritten, cbElement);

        /* advance. */
        iArg++;
        if (iArg >= cArgs)
            break;
        rc = DBGCCmdHlpEval(pCmdHlp, &Addr, "%Dv + %#x", &Addr, cbElement);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "%%(%Dv)", &paArgs[0]);
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
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Do the search.
     */
    uint64_t cHits = 0;
    for (;;)
    {
        /* search */
        DBGFADDRESS HitAddress;
        int rc = DBGFR3MemScan(pVM, pDbgc->idCpu, pAddress, cbRange, 1, pabBytes, cbBytes, &HitAddress);
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
        rc = DBGCCmdHlpVarFromDbgfAddr(pCmdHlp, &HitAddress, &VarCur);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpVBoxError(pCmdHlp, rc, "DBGCCmdHlpVarFromDbgfAddr\n");
        if (!pResult)
            pCmdHlp->pfnExec(pCmdHlp, "db %DV LB 10", &VarCur);
        else
            DBGCVAR_ASSIGN(pResult, &VarCur);

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
    if (pResult)
        DBGCVAR_INIT_GC_FLAT(pResult, 0);

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
static DECLCALLBACK(int) dbgcCmdSearchMem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /* check that the parser did what it's supposed to do. */
    //if (    cArgs <= 2
    //    &&  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    //    return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");

    /*
     * Repeat previous search?
     */
    if (cArgs == 0)
        return dbgcCmdWorkerSearchMemResume(pCmdHlp, pVM, NULL);

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
static DECLCALLBACK(int) dbgcCmdSearchMemType(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /* check that the parser did what it's supposed to do. */
    if (    cArgs < 2
        ||  !DBGCVAR_ISGCPOINTER(paArgs[0].enmType))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");
    return dbgcCmdWorkerSearchMem(pCmdHlp, pVM, &paArgs[0], 25, pCmd->pszCmd[1], paArgs + 1, cArgs - 1, NULL);
}


/**
 * List near symbol.
 *
 * @returns VBox status code.
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   pArg        Pointer to the address or symbol to lookup.
 */
static int dbgcDoListNear(PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR pArg)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    RTDBGSYMBOL Symbol;
    int         rc;
    if (pArg->enmType == DBGCVAR_TYPE_SYMBOL)
    {
        /*
         * Lookup the symbol address.
         */
        rc = DBGFR3AsSymbolByName(pVM, pDbgc->hDbgAs, pArg->u.pszString, &Symbol, NULL);
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3AsSymbolByName(,,%s,)\n", pArg->u.pszString);

        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%Rptr %s\n", Symbol.Value, Symbol.szName);
    }
    else
    {
        /*
         * Convert it to a flat GC address and lookup that address.
         */
        DBGCVAR AddrVar;
        rc = DBGCCmdHlpEval(pCmdHlp, &AddrVar, "%%(%DV)", pArg);
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "%%(%DV)\n", pArg);

        RTINTPTR    offDisp;
        DBGFADDRESS Addr;
        rc = DBGFR3AsSymbolByAddr(pVM, pDbgc->hDbgAs, DBGFR3AddrFromFlat(pVM, &Addr, AddrVar.u.GCFlat), &offDisp, &Symbol, NULL);
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3ASymbolByAddr(,,%RGv,,)\n", AddrVar.u.GCFlat);

        if (!offDisp)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV %s", &AddrVar, Symbol.szName);
        else if (offDisp > 0)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV %s + %RGv", &AddrVar, Symbol.szName, offDisp);
        else
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%DV %s - %RGv", &AddrVar, Symbol.szName, -offDisp);
        if ((RTGCINTPTR)Symbol.cb > -offDisp)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " LB %RGv\n", Symbol.cb + offDisp);
        else
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "\n");
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
static DECLCALLBACK(int) dbgcCmdListNear(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    if (!cArgs)
    {
        /*
         * Current cs:eip symbol.
         */
        DBGCVAR AddrVar;
        int rc = DBGCCmdHlpEval(pCmdHlp, &AddrVar, "%%(cs:eip)");
        if (RT_FAILURE(rc))
            return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "%%(cs:eip)\n");
        return dbgcDoListNear(pCmdHlp, pVM, &AddrVar);
    }

/** @todo Fix the darn parser, it's resolving symbols specified as arguments before we get in here. */
    /*
     * Iterate arguments.
     */
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
    {
        int rc = dbgcDoListNear(pCmdHlp, pVM, &paArgs[iArg]);
        if (RT_FAILURE(rc))
            return rc;
    }

    NOREF(pCmd);
    return VINF_SUCCESS;
}


/**
 * Matches the module patters against a module name.
 *
 * @returns true if matching, otherwise false.
 * @param   pszName     The module name.
 * @param   paArgs      The module pattern argument list.
 * @param   cArgs       Number of arguments.
 */
static bool dbgcCmdListModuleMatch(const char *pszName, PCDBGCVAR paArgs, unsigned cArgs)
{
    for (uint32_t i = 0; i < cArgs; i++)
        if (RTStrSimplePatternMatch(paArgs[i].u.pszString, pszName))
            return true;
    return false;
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
static DECLCALLBACK(int) dbgcCmdListModules(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    bool const  fMappings   = pCmd->pszCmd[2] == 'o';
    PDBGC       pDbgc       = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Iterate the modules in the current address space and print info about
     * those matching the input.
     */
    RTDBGAS     hAs         = DBGFR3AsResolveAndRetain(pVM, pDbgc->hDbgAs);
    uint32_t    cMods       = RTDbgAsModuleCount(hAs);
    for (uint32_t iMod = 0; iMod < cMods; iMod++)
    {
        RTDBGMOD hMod = RTDbgAsModuleByIndex(hAs, iMod);
        if (hMod != NIL_RTDBGMOD)
        {
            uint32_t const      cSegs   = RTDbgModSegmentCount(hMod);
            const char * const  pszName = RTDbgModName(hMod);
            if (    cArgs == 0
                ||  dbgcCmdListModuleMatch(pszName, paArgs, cArgs))
            {
                /*
                 * Find the mapping with the lower address, preferring a full
                 * image mapping, for the main line.
                 */
                RTDBGASMAPINFO  aMappings[128];
                uint32_t        cMappings = RT_ELEMENTS(aMappings);
                int rc = RTDbgAsModuleQueryMapByIndex(hAs, iMod, &aMappings[0], &cMappings, 0 /*fFlags*/);
                if (RT_SUCCESS(rc))
                {
                    bool        fFull = false;
                    RTUINTPTR   uMin = RTUINTPTR_MAX;
                    for (uint32_t iMap = 0; iMap < cMappings; iMap++)
                        if (    aMappings[iMap].Address < uMin
                            &&  (   !fFull
                                 ||  aMappings[iMap].iSeg == NIL_RTDBGSEGIDX))
                            uMin = aMappings[iMap].Address;
                    DBGCCmdHlpPrintf(pCmdHlp, "%RGv %04x %s\n", (RTGCUINTPTR)uMin, cSegs, pszName);

                    if (fMappings)
                    {
                        /* sort by address first - not very efficient. */
                        for (uint32_t i = 0; i + 1 < cMappings; i++)
                            for (uint32_t j = i + 1; j < cMappings; j++)
                                if (aMappings[j].Address < aMappings[i].Address)
                                {
                                    RTDBGASMAPINFO Tmp = aMappings[j];
                                    aMappings[j] = aMappings[i];
                                    aMappings[i] = Tmp;
                                }

                        /* print */
                        for (uint32_t iMap = 0; iMap < cMappings; iMap++)
                            if (aMappings[iMap].iSeg != NIL_RTDBGSEGIDX)
                                DBGCCmdHlpPrintf(pCmdHlp, "    %RGv %RGv #%02x %s\n",
                                                 (RTGCUINTPTR)aMappings[iMap].Address,
                                                 (RTGCUINTPTR)RTDbgModSegmentSize(hMod, aMappings[iMap].iSeg),
                                                 aMappings[iMap].iSeg,
                                                 /** @todo RTDbgModSegmentName(hMod, aMappings[iMap].iSeg)*/ "noname");
                            else
                                DBGCCmdHlpPrintf(pCmdHlp, "    %RGv %RGv <everything>\n",
                                                 (RTGCUINTPTR)aMappings[iMap].Address,
                                                 (RTGCUINTPTR)RTDbgModImageSize(hMod));
                    }
                }
                else
                    DBGCCmdHlpPrintf(pCmdHlp, "%.*s %04x %s (rc=%Rrc)\n",
                                     sizeof(RTGCPTR) * 2, "???????????", cSegs, pszName, rc);
                /** @todo missing address space API for enumerating the mappings. */
            }
            RTDbgModRelease(hMod);
        }
    }
    RTDbgAsRelease(hAs);

    NOREF(pCmd);
    return VINF_SUCCESS;
}
