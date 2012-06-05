/* $Id$ */
/** @file
 * DBGC - Debugger Console, Native Commands.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <VBox/vmm/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>

#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <stdlib.h>
#include <stdio.h>

#include "DBGCInternal.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) dbgcCmdHelp(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdQuit(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdStop(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdDetect(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdCpu(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLog(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLogDest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLogFlags(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdFormat(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLoadImage(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLoadMap(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLoadSeg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLoadSyms(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdUnset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLoadVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdShowVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdLoadPlugIn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdUnloadPlugIn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdShowPlugIns(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdHarakiri(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdEcho(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdRunScript(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
static DECLCALLBACK(int) dbgcCmdWriteCore(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
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
    {  1,           ~0U,        DBGCVAR_CAT_STRING,     0,                              "strings",      "One or more strings." },
};

/** Filename string. */
static const DBGCVARDESC    g_aArgFilename[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "path",         "Filename string." },
};


/** 'cpu' arguments. */
static const DBGCVARDESC    g_aArgCpu[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,     DBGCVAR_CAT_NUMBER_NO_RANGE, 0,                              "idCpu",        "CPU ID" },
};


/** 'help' arguments. */
static const DBGCVARDESC    g_aArgHelp[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           ~0U,        DBGCVAR_CAT_STRING,     0,                              "cmd/op",       "Zero or more command or operator names." },
};


/** 'info' arguments. */
static const DBGCVARDESC    g_aArgInfo[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "info",         "The name of the info to display." },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "args",         "String arguments to the handler." },
};


/** loadimage arguments. */
static const DBGCVARDESC    g_aArgLoadImage[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "filename",     "Filename string." },
    {  1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "The module address." },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "name",         "The module name. (optional)" },
};


/** loadmap arguments. */
static const DBGCVARDESC    g_aArgLoadMap[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "filename",     "Filename string." },
    {  1,           1,          DBGCVAR_CAT_POINTER,    DBGCVD_FLAGS_DEP_PREV,          "address",      "The module address." },
    {  0,           1,          DBGCVAR_CAT_STRING,     DBGCVD_FLAGS_DEP_PREV,          "name",         "The module name. Empty string means default. (optional)" },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     DBGCVD_FLAGS_DEP_PREV,          "subtrahend",   "Value to subtract from the addresses in the map file to rebase it correctly to address. (optional)" },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     DBGCVD_FLAGS_DEP_PREV,          "seg",          "The module segment number (0-based). (optional)" },
};


/** loadseg arguments. */
static const DBGCVARDESC    g_aArgLoadSeg[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "filename",     "Filename string." },
    {  1,           1,          DBGCVAR_CAT_POINTER,    0,                              "address",      "The module address." },
    {  1,           1,          DBGCVAR_CAT_NUMBER,     0,                              "seg",          "The module segment number (0-based)." },
    {  0,           1,          DBGCVAR_CAT_STRING,     DBGCVD_FLAGS_DEP_PREV,          "name",         "The module name. Empty string means default. (optional)" },
};


/** loadsyms arguments. */
static const DBGCVARDESC    g_aArgLoadSyms[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "path",         "Filename string." },
    {  0,           1,          DBGCVAR_CAT_NUMBER,     0,                              "delta",        "Delta to add to the loaded symbols. (optional)" },
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "module name",  "Module name. (optional)" },
    {  0,           1,          DBGCVAR_CAT_POINTER,    DBGCVD_FLAGS_DEP_PREV,          "module address", "Module address. (optional)" },
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


/** loadplugin, unloadplugin. */
static const DBGCVARDESC    g_aArgPlugIn[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           ~0U,        DBGCVAR_CAT_STRING,     0,                              "plugin",       "Plug-in name or filename." },
};


/** 'set' arguments */
static const DBGCVARDESC    g_aArgSet[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_SYMBOL,     0,                              "var",          "Variable name." },
    {  1,           1,          DBGCVAR_CAT_ANY,        0,                              "value",        "Value to assign to the variable." },
};

/** 'unset' arguments */
static const DBGCVARDESC    g_aArgUnset[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           ~0U,        DBGCVAR_CAT_SYMBOL,     0,                              "vars",         "One or more variable names." },
};

/** writecore arguments. */
static const DBGCVARDESC    g_aArgWriteCore[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "path",         "Filename string." },
};



/** Command descriptors for the basic commands. */
const DBGCCMD    g_aDbgcCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDescs,          cArgDescs,               fFlags, pfnHandler        pszSyntax,          ....pszDescription */
    { "bye",        0,        0,        NULL,                0,                            0, dbgcCmdQuit,      "",                     "Exits the debugger." },
    { "cpu",        0,        1,        &g_aArgCpu[0],       RT_ELEMENTS(g_aArgCpu),       0, dbgcCmdCpu,       "[idCpu]",              "If no argument, display the current CPU, else change to the specified CPU." },
    { "echo",       1,        ~0U,      &g_aArgMultiStr[0],  RT_ELEMENTS(g_aArgMultiStr),  0, dbgcCmdEcho,      "<str1> [str2..[strN]]", "Displays the strings separated by one blank space and the last one followed by a newline." },
    { "exit",       0,        0,        NULL,                0,                            0, dbgcCmdQuit,      "",                     "Exits the debugger." },
    { "format",     1,        1,        &g_aArgAny[0],       RT_ELEMENTS(g_aArgAny),       0, dbgcCmdFormat,    "",                     "Evaluates an expression and formats it." },
    { "detect",     0,        0,        NULL,                0,                            0, dbgcCmdDetect,    "",                     "Detects or re-detects the guest os and starts the OS specific digger." },
    { "harakiri",   0,        0,        NULL,                0,                            0, dbgcCmdHarakiri,  "",                     "Kills debugger process." },
    { "help",       0,        ~0U,      &g_aArgHelp[0],      RT_ELEMENTS(g_aArgHelp),      0, dbgcCmdHelp,      "[cmd/op [..]]",        "Display help. For help about info items try 'info help'." },
    { "info",       1,        2,        &g_aArgInfo[0],      RT_ELEMENTS(g_aArgInfo),      0, dbgcCmdInfo,      "<info> [args]",        "Display info register in the DBGF. For a list of info items try 'info help'." },
    { "loadimage",  2,        3,        &g_aArgLoadImage[0], RT_ELEMENTS(g_aArgLoadImage), 0, dbgcCmdLoadImage, "<filename> <address> [name]",
                                                                                                                                                 "Loads the symbols of an executable image at the specified address. "
                                                                                                                                                 /*"Optionally giving the module a name other than the file name stem."*/ }, /** @todo implement line breaks */
    { "loadmap",    2,        5,        &g_aArgLoadMap[0],   RT_ELEMENTS(g_aArgLoadMap),   0, dbgcCmdLoadMap,   "<filename> <address> [name] [subtrahend] [seg]",
                                                                                                                                       "Loads the symbols from a map file, usually at a specified address. "
                                                                                                                                       /*"Optionally giving the module a name other than the file name stem "
                                                                                                                                       "and a subtrahend to subtract from the addresses."*/ },
    { "loadplugin", 1,        1,        &g_aArgPlugIn[0],    RT_ELEMENTS(g_aArgPlugIn),    0, dbgcCmdLoadPlugIn,"<plugin1> [plugin2..N]", "Loads one or more plugins" },
    { "loadseg",    3,        4,        &g_aArgLoadSeg[0],   RT_ELEMENTS(g_aArgLoadSeg),   0, dbgcCmdLoadSeg,   "<filename> <address> <seg> [name]",
                                                                                                                                       "Loads the symbols of a segment in the executable image at the specified address. "
                                                                                                                                       /*"Optionally giving the module a name other than the file name stem."*/ },
    { "loadsyms",   1,        5,        &g_aArgLoadSyms[0],  RT_ELEMENTS(g_aArgLoadSyms),  0, dbgcCmdLoadSyms,  "<filename> [delta] [module] [module address]", "Loads symbols from a text file. Optionally giving a delta and a module." },
    { "loadvars",   1,        1,        &g_aArgFilename[0],  RT_ELEMENTS(g_aArgFilename),  0, dbgcCmdLoadVars,  "<filename>",           "Load variables from file. One per line, same as the args to the set command." },
    { "log",        1,        1,        &g_aArgLog[0],       RT_ELEMENTS(g_aArgLog),       0, dbgcCmdLog,       "<group string>",       "Modifies the logging group settings (VBOX_LOG)" },
    { "logdest",    1,        1,        &g_aArgLogDest[0],   RT_ELEMENTS(g_aArgLogDest),   0, dbgcCmdLogDest,   "<dest string>",        "Modifies the logging destination (VBOX_LOG_DEST)." },
    { "logflags",   1,        1,        &g_aArgLogFlags[0],  RT_ELEMENTS(g_aArgLogFlags),  0, dbgcCmdLogFlags,  "<flags string>",       "Modifies the logging flags (VBOX_LOG_FLAGS)." },
    { "quit",       0,        0,        NULL,                0,                            0, dbgcCmdQuit,      "",                     "Exits the debugger." },
    { "runscript",  1,        1,        &g_aArgFilename[0],  RT_ELEMENTS(g_aArgFilename),  0, dbgcCmdRunScript, "<filename>",           "Runs the command listed in the script. Lines starting with '#' "
                                                                                                                                        "(after removing blanks) are comment. blank lines are ignored. Stops on failure." },
    { "set",        2,        2,        &g_aArgSet[0],       RT_ELEMENTS(g_aArgSet),       0, dbgcCmdSet,       "<var> <value>",        "Sets a global variable." },
    { "showplugins",0,        0,        NULL,                0,                            0, dbgcCmdShowPlugIns,"",                     "List loaded plugins." },
    { "showvars",   0,        0,        NULL,                0,                            0, dbgcCmdShowVars,  "",                     "List all the defined variables." },
    { "stop",       0,        0,        NULL,                0,                            0, dbgcCmdStop,      "",                     "Stop execution." },
    { "unloadplugin", 1,     ~0U,       &g_aArgPlugIn[0],    RT_ELEMENTS(g_aArgPlugIn),    0, dbgcCmdUnloadPlugIn, "<plugin1> [plugin2..N]", "Unloads one or more plugins." },
    { "unset",      1,       ~0U,       &g_aArgUnset[0],     RT_ELEMENTS(g_aArgUnset),     0, dbgcCmdUnset,     "<var1> [var1..[varN]]",  "Unsets (delete) one or more global variables." },
    { "writecore",  1,        1,        &g_aArgWriteCore[0], RT_ELEMENTS(g_aArgWriteCore), 0, dbgcCmdWriteCore,   "<filename>",           "Write core to file." },
};
/** The number of native commands. */
const uint32_t      g_cDbgcCmds = RT_ELEMENTS(g_aDbgcCmds);
/** Pointer to head of the list of external commands. */
static PDBGCEXTCMDS g_pExtCmdsHead;




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
PCDBGCCMD dbgcCommandLookup(PDBGC pDbgc, const char *pachName, size_t cchName, bool fExternal)
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

        for (unsigned iCmd = 0; iCmd < RT_ELEMENTS(g_aDbgcCmds); iCmd++)
        {
            if (    !strncmp(pachName, g_aDbgcCmds[iCmd].pszCmd, cchName)
                &&  !g_aDbgcCmds[iCmd].pszCmd[cchName])
                return &g_aDbgcCmds[iCmd];
        }
    }
    else
    {
        DBGCEXTLISTS_LOCK_RD();
        for (PDBGCEXTCMDS pExtCmds = g_pExtCmdsHead; pExtCmds; pExtCmds = pExtCmds->pNext)
        {
            for (unsigned iCmd = 0; iCmd < pExtCmds->cCmds; iCmd++)
            {
                if (    !strncmp(pachName, pExtCmds->paCmds[iCmd].pszCmd, cchName)
                    &&  !pExtCmds->paCmds[iCmd].pszCmd[cchName])
                    return &pExtCmds->paCmds[iCmd];
            }
        }
        DBGCEXTLISTS_UNLOCK_RD();
    }

    return NULL;
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
    DBGCEXTLISTS_LOCK_WR();
    PDBGCEXTCMDS pCur = g_pExtCmdsHead;
    while (pCur)
    {
        if (paCommands == pCur->paCmds)
        {
            DBGCEXTLISTS_UNLOCK_WR();
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
    DBGCEXTLISTS_UNLOCK_WR();

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
    DBGCEXTLISTS_LOCK_WR();
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
            DBGCEXTLISTS_UNLOCK_WR();

            RTMemFree(pCur);
            return VINF_SUCCESS;
        }
        pPrev = pCur;
        pCur = pCur->pNext;
    }
    DBGCEXTLISTS_UNLOCK_WR();

    NOREF(cCommands);
    return VERR_DBGC_COMMANDS_NOT_REGISTERED;
}


/**
 * Prints full command help.
 */
static int dbgcPrintHelpCmd(PDBGCCMDHLP pCmdHlp, PCDBGCCMD pCmd, bool fExternal)
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


static int dbgcCmdHelpCommandsWorker(PDBGC pDbgc, PDBGCCMDHLP pCmdHlp, PCDBGCCMD paCmds, size_t cCmds, bool fExternal,
                                     const char *pszDescFmt, ...)
{
    int rc = VINF_SUCCESS;
    if (pszDescFmt)
    {
        va_list va;
        va_start(va, pszDescFmt);
        rc = pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszDescFmt, va);
        va_end(va);
    }

    for (unsigned i = 0; i < cCmds && RT_SUCCESS(rc); i++)
        rc = DBGCCmdHlpPrintf(pCmdHlp,
                              !fExternal ? "%-11s %-30s %s\n" :  ".%-10s %-30s %s\n",
                              paCmds[i].pszCmd,
                              paCmds[i].pszSyntax,
                              paCmds[i].pszDescription);
    return VINF_SUCCESS;
}


static int dbgcCmdHelpCommands(PDBGC pDbgc, PDBGCCMDHLP pCmdHlp)
{
    int rc = dbgcCmdHelpCommandsWorker(pDbgc, pCmdHlp, pDbgc->paEmulationCmds, pDbgc->cEmulationCmds, false,
                                       "Commands for %s emulation:\n", pDbgc->pszEmulation);
    if (RT_SUCCESS(rc))
        rc = dbgcCmdHelpCommandsWorker(pDbgc, pCmdHlp, g_aDbgcCmds, RT_ELEMENTS(g_aDbgcCmds), false,
                                       "\nCommon Commands:\n");

    if (RT_SUCCESS(rc))
    {
        DBGCEXTLISTS_LOCK_RD();
        const char *pszDesc = "\nExternal Commands:\n";
        for (PDBGCEXTCMDS pExtCmd = g_pExtCmdsHead; pExtCmd && RT_SUCCESS(rc); pExtCmd = pExtCmd->pNext)
        {
            rc = dbgcCmdHelpCommandsWorker(pDbgc, pCmdHlp, pExtCmd->paCmds, pExtCmd->cCmds, false,
                                           pszDesc);
            pszDesc = NULL;
        }
        DBGCEXTLISTS_UNLOCK_RD();
    }

    return rc;
}


/**
 * Prints full function help.
 */
static int dbgcPrintHelpFunction(PDBGCCMDHLP pCmdHlp, PCDBGCFUNC pFunc, bool fExternal)
{
    int rc;

    /* the command */
    rc = DBGCCmdHlpPrintf(pCmdHlp, "%s%-*s %-30s %s",
                          fExternal ? "." : "",
                          fExternal ? 10 : 11,
                          pFunc->pszFuncNm,
                          pFunc->pszSyntax,
                          pFunc->pszDescription);
    if (!pFunc->cArgsMin && pFunc->cArgsMin == pFunc->cArgsMax)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <no args>\n");
    else if (pFunc->cArgsMin == pFunc->cArgsMax)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u args>\n", pFunc->cArgsMin);
    else if (pFunc->cArgsMax == ~0U)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u+ args>\n", pFunc->cArgsMin);
    else
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u to %u args>\n", pFunc->cArgsMin, pFunc->cArgsMax);

    /* argument descriptions. */
    for (unsigned i = 0; i < pFunc->cArgDescs; i++)
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                                "    %-12s %s",
                                pFunc->paArgDescs[i].pszName,
                                pFunc->paArgDescs[i].pszDescription);
        if (!pFunc->paArgDescs[i].cTimesMin)
        {
            if (pFunc->paArgDescs[i].cTimesMax == ~0U)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <optional+>\n");
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <optional-%u>\n", pFunc->paArgDescs[i].cTimesMax);
        }
        else
        {
            if (pFunc->paArgDescs[i].cTimesMax == ~0U)
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u+>\n", pFunc->paArgDescs[i].cTimesMin);
            else
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, " <%u-%u>\n", pFunc->paArgDescs[i].cTimesMin, pFunc->paArgDescs[i].cTimesMax);
        }
    }
    return rc;
}


static int dbgcCmdHelpFunctionsWorker(PDBGC pDbgc, PDBGCCMDHLP pCmdHlp, PCDBGCFUNC paFuncs, size_t cFuncs, bool fExternal,
                                      const char *pszDescFmt, ...)
{
    int rc = VINF_SUCCESS;
    if (pszDescFmt)
    {
        va_list va;
        va_start(va, pszDescFmt);
        rc = DBGCCmdHlpPrintf(pCmdHlp, pszDescFmt, va);
        va_end(va);
    }

    for (unsigned i = 0; i < cFuncs && RT_SUCCESS(rc); i++)
        rc = DBGCCmdHlpPrintf(pCmdHlp,
                              !fExternal ? "%-11s %-30s %s\n" :  ".%-10s %-30s %s\n",
                              paFuncs[i].pszFuncNm,
                              paFuncs[i].pszSyntax,
                              paFuncs[i].pszDescription);
    return VINF_SUCCESS;
}


static int dbgcCmdHelpFunctions(PDBGC pDbgc, PDBGCCMDHLP pCmdHlp)
{
    int rc = dbgcCmdHelpFunctionsWorker(pDbgc, pCmdHlp, pDbgc->paEmulationFuncs, pDbgc->cEmulationFuncs, false,
                                        "Functions for %s emulation:\n", pDbgc->pszEmulation);
    if (RT_SUCCESS(rc))
        rc = dbgcCmdHelpFunctionsWorker(pDbgc, pCmdHlp, g_aDbgcFuncs, g_cDbgcFuncs, false,
                                        "\nCommon Functions:\n");
#if 0
    if (RT_SUCCESS(rc))
    {
        DBGCEXTLISTS_LOCK_RD();
        const char *pszDesc = "\nExternal Functions:\n";
        for (PDBGCEXTFUNCS pExtFunc = g_pExtFuncsHead; pExtFunc && RT_SUCCESS(rc); pExtFunc = pExtFunc->pNext)
        {
            rc = dbgcCmdHelpFunctionsWorker(pDbgc, pCmdHlp, pExtFunc->paFuncs, pExtFunc->cFuncs, false,
                                            pszDesc);
            pszDesc = NULL;
        }
        DBGCEXTLISTS_UNLOCK_RD();
    }
#endif
    return rc;
}


static int dbgcCmdHelpOperators(PDBGC pDbgc, PDBGCCMDHLP pCmdHlp)
{
    int rc = DBGCCmdHlpPrintf(pCmdHlp, "Operators:\n");

    unsigned iPrecedence = 0;
    unsigned cLeft = g_cDbgcOps;
    while (cLeft > 0)
    {
        for (unsigned i = 0; i < g_cDbgcOps; i++)
            if (g_aDbgcOps[i].iPrecedence == iPrecedence)
            {
                rc = DBGCCmdHlpPrintf(pCmdHlp,
                                      "%-10s  %s  %s\n",
                                      g_aDbgcOps[i].szName,
                                      g_aDbgcOps[i].fBinary ? "Binary" : "Unary ",
                                      g_aDbgcOps[i].pszDescription);
                cLeft--;
            }
        iPrecedence++;
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
static DECLCALLBACK(int) dbgcCmdHelp(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC       pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    int         rc = VINF_SUCCESS;

    if (!cArgs)
    {
        /*
         * No arguments, show summary.
         */
        rc = DBGCCmdHlpPrintf(pCmdHlp,
                              "VirtualBox Debugger Help Summary\n"
                              "--------------------------------\n"
                              "\n"
                              "help commands      Show help on all commands.\n"
                              "help functions     Show help on all functions.\n"
                              "help operators     Show help on all operators.\n"
                              "help <cmd-pattern> [...]\n"
                              "                   Show details help on individual commands, simple\n"
                              "                   patterns can be used to match several commands.\n"
                              );

    }
    else
    {
        /*
         * Search for the arguments (strings).
         */
        DBGCEXTCMDS  aFixedCmds[] =
        {
            { pDbgc->cEmulationCmds,    pDbgc->paEmulationCmds,     NULL },
            { g_cDbgcCmds,              g_aDbgcCmds,                NULL },
        };
        DBGCEXTFUNCS aFixedFuncs[] =
        {
            { pDbgc->cEmulationFuncs,   pDbgc->paEmulationFuncs,    NULL },
            { g_cDbgcFuncs,             g_aDbgcFuncs,               NULL },
        };

        for (unsigned iArg = 0; iArg < cArgs && RT_SUCCESS(rc); iArg++)
        {
            AssertReturn(paArgs[iArg].enmType == DBGCVAR_TYPE_STRING, VERR_DBGC_PARSE_BUG);
            const char *pszPattern = paArgs[iArg].u.pszString;

            /* aliases */
            if (!strcmp(pszPattern, "commands"))
                rc = dbgcCmdHelpCommands(pDbgc, pCmdHlp);
            else if (!strcmp(pszPattern, "functions"))
                rc = dbgcCmdHelpFunctions(pDbgc, pCmdHlp);
            else if (!strcmp(pszPattern, "operators"))
                rc = dbgcCmdHelpOperators(pDbgc, pCmdHlp);
            else if (!strcmp(pszPattern, "all"))
            {
                rc = dbgcCmdHelpFunctions(pDbgc, pCmdHlp);
                rc = DBGCCmdHlpPrintf(pCmdHlp,
                                      "VirtualBox Debugger Help\n"
                                      "------------------------\n"
                                      "\n");
                rc = dbgcCmdHelpCommands(pDbgc, pCmdHlp);
                rc = DBGCCmdHlpPrintf(pCmdHlp, "\n");
                rc = dbgcCmdHelpFunctions(pDbgc, pCmdHlp);
                rc = DBGCCmdHlpPrintf(pCmdHlp, "\n");
                rc = dbgcCmdHelpOperators(pDbgc, pCmdHlp);
            }
            else
            {
                /* Individual commands. */
                bool    fFound     = false;

                /* lookup in the emulation command list first */
                for (unsigned j = 0; j < RT_ELEMENTS(aFixedCmds); j++)
                    for (unsigned i = 0; i < aFixedCmds[j].cCmds; i++)
                        if (RTStrSimplePatternMatch(pszPattern, aFixedCmds[j].paCmds[i].pszCmd))
                        {
                            rc = dbgcPrintHelpCmd(pCmdHlp, &aFixedCmds[j].paCmds[i], false);
                            fFound = true;
                        }
                for (unsigned j = 0; j < RT_ELEMENTS(aFixedFuncs); j++)
                    for (unsigned i = 0; i < aFixedFuncs[j].cFuncs; i++)
                        if (RTStrSimplePatternMatch(pszPattern, aFixedFuncs[j].paFuncs[i].pszFuncNm))
                        {
                            rc = dbgcPrintHelpFunction(pCmdHlp, &aFixedFuncs[j].paFuncs[i], false);
                            fFound = true;
                        }

               /* external commands */
               if (     g_pExtCmdsHead
                   &&   (   *pszPattern == '.'
                         || *pszPattern == '?'
                         || *pszPattern == '*'))
               {
                   DBGCEXTLISTS_LOCK_RD();
                   const char *pszPattern2 = pszPattern + (*pszPattern == '.' || *pszPattern == '?');
                   for (PDBGCEXTCMDS pExtCmd = g_pExtCmdsHead; pExtCmd; pExtCmd = pExtCmd->pNext)
                       for (unsigned i = 0; i < pExtCmd->cCmds; i++)
                           if (RTStrSimplePatternMatch(pszPattern2, pExtCmd->paCmds[i].pszCmd))
                           {
                               rc = dbgcPrintHelpCmd(pCmdHlp, &pExtCmd->paCmds[i], true);
                               fFound = true;
                           }
#if 0
                   for (PDBGCEXTFUNCS pExtFunc = g_pExtFuncsHead; pExtFunc; pExtFunc = pExtFunc->pNext)
                       for (unsigned i = 0; i < pExtFunc->cFuncs; i++)
                           if (RTStrSimplePatternMatch(pszPattern2, pExtFunc->paFuncs[i].pszFuncNm))
                           {
                               rc = dbgcPrintHelpFunction(pCmdHlp, &pExtFunc->paFuncs[i], true);
                               fFound = true;
                           }
#endif
                   DBGCEXTLISTS_UNLOCK_RD();
               }

               /* operators */
               if (!fFound && strlen(paArgs[iArg].u.pszString) < sizeof(g_aDbgcOps[0].szName))
                   for (unsigned i = 0; i < g_cDbgcOps && RT_SUCCESS(rc); i++)
                       if (RTStrSimplePatternMatch(pszPattern, g_aDbgcOps[i].szName))
                       {
                           rc = DBGCCmdHlpPrintf(pCmdHlp, "%-10s  %s  %s\n",
                                                 g_aDbgcOps[i].szName,
                                                 g_aDbgcOps[i].fBinary ? "Binary" : "Unary ",
                                                 g_aDbgcOps[i].pszDescription);
                           fFound = true;
                       }

               /* found? */
               if (!fFound)
                   rc = DBGCCmdHlpPrintf(pCmdHlp, "error: '%s' was not found!\n",
                                         paArgs[iArg].u.pszString);
            }
        } /* foreach argument */
    }

    NOREF(pCmd);
    NOREF(pVM);
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
static DECLCALLBACK(int) dbgcCmdQuit(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Quitting console...\n");
    NOREF(pCmd);
    NOREF(pVM);
    NOREF(paArgs);
    NOREF(cArgs);
    return VERR_DBGC_QUIT;
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
static DECLCALLBACK(int) dbgcCmdStop(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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
        if (RT_SUCCESS(rc))
            rc = VWRN_DBGC_CMD_PENDING;
        else
            rc = pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Executing DBGFR3Halt().");
    }

    NOREF(pCmd); NOREF(paArgs); NOREF(cArgs);
    return rc;
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
static DECLCALLBACK(int) dbgcCmdEcho(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Loop thru the arguments and print them with one space between.
     */
    int rc = 0;
    for (unsigned i = 0; i < cArgs; i++)
    {
        AssertReturn(paArgs[i].enmType == DBGCVAR_TYPE_STRING, VERR_DBGC_PARSE_BUG);
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, i ? " %s" : "%s", paArgs[i].u.pszString);
        if (RT_FAILURE(rc))
            return rc;
    }
    NOREF(pCmd); NOREF(pVM);
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
static DECLCALLBACK(int) dbgcCmdRunScript(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /* check that the parser did what it's supposed to do. */
    if (    cArgs != 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");

    /** @todo Load the script here, but someone else should do the actual
     *        evaluation and execution of it.  */

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
               &&   RT_C_IS_SPACE(pszEnd[-1])) /* RT_C_IS_SPACE includes \n and \r normally. */
            *--pszEnd = '\0';

        /** @todo check for Control-C / Cancel at this point... */

        /*
         * Execute the command.
         *
         * This is a bit wasteful with scratch space btw., can fix it later.
         * The whole return code crap should be fixed too, so that it's possible
         * to know whether a command succeeded (RT_SUCCESS()) or failed, and
         * more importantly why it failed.
         */
        rc = pCmdHlp->pfnExec(pCmdHlp, "%s", psz);
        if (RT_FAILURE(rc))
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

    NOREF(pCmd); NOREF(pVM);
    return rc;
}


/**
 * The 'detect' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdDetect(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /* check that the parser did what it's supposed to do. */
    if (cArgs != 0)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");

    /*
     * Perform the detection.
     */
    char szName[64];
    int rc = DBGFR3OSDetect(pVM, szName, sizeof(szName));
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Executing DBGFR3OSDetect().");
    if (rc == VINF_SUCCESS)
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Guest OS: %s\n", szName);
        char szVersion[512];
        int rc2 = DBGFR3OSQueryNameAndVersion(pVM, NULL, 0, szVersion, sizeof(szVersion));
        if (RT_SUCCESS(rc2))
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Version : %s\n", szVersion);
    }
    else
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Unable to figure out which guest OS it is, sorry.\n");
    NOREF(pCmd); NOREF(paArgs);
    return rc;
}


/**
 * The 'cpu' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdCpu(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC       pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /* check that the parser did what it's supposed to do. */
    if (    cArgs != 0
        &&  (   cArgs != 1
             || paArgs[0].enmType != DBGCVAR_TYPE_NUMBER))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "parser error\n");
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: No VM.\n");

    int rc;
    if (!cArgs)
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Current CPU ID: %u\n", pDbgc->idCpu);
    else
    {
/** @todo add a DBGF getter for this. */
        if (paArgs[0].u.u64Number >= pVM->cCpus)
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: idCpu %u is out of range! Highest ID is %u.\n",
                                    paArgs[0].u.u64Number, pVM->cCpus);
        else
        {
            rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Changed CPU from %u to %u.\n",
                                    pDbgc->idCpu, (VMCPUID)paArgs[0].u.u64Number);
            pDbgc->idCpu = (VMCPUID)paArgs[0].u.u64Number;
        }
    }
    return rc;
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
static DECLCALLBACK(int) dbgcCmdInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC       pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

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
    int rc = DBGFR3InfoEx(pVM, pDbgc->idCpu,
                          paArgs[0].u.pszString,
                          cArgs == 2 ? paArgs[1].u.pszString : NULL,
                          DBGCCmdHlpGetDbgfOutputHlp(pCmdHlp));
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3InfoEx()\n");

    NOREF(pCmd);
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
static DECLCALLBACK(int) dbgcCmdLog(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    int rc = DBGFR3LogModifyGroups(pVM, paArgs[0].u.pszString);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    NOREF(pCmd); NOREF(cArgs);
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
static DECLCALLBACK(int) dbgcCmdLogDest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    int rc = DBGFR3LogModifyDestinations(pVM, paArgs[0].u.pszString);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    NOREF(pCmd); NOREF(cArgs);
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
static DECLCALLBACK(int) dbgcCmdLogFlags(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    int rc = DBGFR3LogModifyFlags(pVM, paArgs[0].u.pszString);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    NOREF(pCmd); NOREF(cArgs);
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
static DECLCALLBACK(int) dbgcCmdFormat(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
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
            case DBGCVAR_TYPE_HC_PHYS:
                if (paArgs[iArg].enmRangeType != DBGCVAR_RANGE_NONE)
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host physical address: %RHp range %lld %s\n",
                        paArgs[iArg].u.HCPhys,
                        paArgs[iArg].u64Range,
                        apszRangeDesc[paArgs[iArg].enmRangeType]);
                else
                    rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                        "Host physical address: %RHp\n",
                        paArgs[iArg].u.HCPhys);
                break;

            case DBGCVAR_TYPE_STRING:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "String, %lld bytes long: %s\n",
                    paArgs[iArg].u64Range,
                    paArgs[iArg].u.pszString);
                break;

            case DBGCVAR_TYPE_SYMBOL:
                rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
                    "Symbol, %lld bytes long: %s\n",
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

    NOREF(pCmd); NOREF(pVM);
    return 0;
}


/**
 * The 'loadimage' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLoadImage(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Validate the parsing and make sense of the input.
     * This is a mess as usual because we don't trust the parser yet.
     */
    AssertReturn(    cArgs >= 2
                 &&  cArgs <= 3
                 &&  paArgs[0].enmType == DBGCVAR_TYPE_STRING
                 &&  DBGCVAR_ISPOINTER(paArgs[1].enmType),
                 VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);

    const char     *pszFilename = paArgs[0].u.pszString;

    DBGFADDRESS     ModAddress;
    int rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, &paArgs[1], &ModAddress);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "pfnVarToDbgfAddr: %Dv\n", &paArgs[1]);

    const char     *pszModName  = NULL;
    if (cArgs >= 3)
    {
        AssertReturn(paArgs[2].enmType == DBGCVAR_TYPE_STRING, VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
        pszModName = paArgs[2].u.pszString;
    }

    /*
     * Try create a module for it.
     */
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    rc = DBGFR3AsLoadImage(pVM, pDbgc->hDbgAs, pszFilename, pszModName, &ModAddress, NIL_RTDBGSEGIDX, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3ModuleLoadImage(,,'%s','%s',%Dv,)\n",
                                     pszFilename, pszModName, &paArgs[1]);

    NOREF(pCmd);
    return VINF_SUCCESS;
}


/**
 * The 'loadmap' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLoadMap(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Validate the parsing and make sense of the input.
     * This is a mess as usual because we don't trust the parser yet.
     */
    AssertReturn(    cArgs >= 2
                 &&  cArgs <= 5
                 &&  paArgs[0].enmType == DBGCVAR_TYPE_STRING
                 &&  DBGCVAR_ISPOINTER(paArgs[1].enmType),
                 VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);

    const char     *pszFilename = paArgs[0].u.pszString;

    DBGFADDRESS     ModAddress;
    int rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, &paArgs[1], &ModAddress);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "pfnVarToDbgfAddr: %Dv\n", &paArgs[1]);

    const char     *pszModName  = NULL;
    if (cArgs >= 3)
    {
        AssertReturn(paArgs[2].enmType == DBGCVAR_TYPE_STRING, VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
        pszModName = paArgs[2].u.pszString;
    }

    RTGCUINTPTR uSubtrahend = 0;
    if (cArgs >= 4)
    {
        AssertReturn(paArgs[3].enmType == DBGCVAR_TYPE_NUMBER, VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
        uSubtrahend = paArgs[3].u.u64Number;
    }

    RTDBGSEGIDX     iModSeg = NIL_RTDBGSEGIDX;
    if (cArgs >= 5)
    {
        AssertReturn(paArgs[4].enmType == DBGCVAR_TYPE_NUMBER, VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
        iModSeg = (RTDBGSEGIDX)paArgs[4].u.u64Number;
        if (    iModSeg != paArgs[4].u.u64Number
            ||  iModSeg > RTDBGSEGIDX_LAST)
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Segment index out of range: %Dv; range={0..%#x}\n", &paArgs[1], RTDBGSEGIDX_LAST);
    }

    /*
     * Try create a module for it.
     */
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    rc = DBGFR3AsLoadMap(pVM, pDbgc->hDbgAs, pszFilename, pszModName, &ModAddress, NIL_RTDBGSEGIDX, uSubtrahend, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3AsLoadMap(,,'%s','%s',%Dv,)\n",
                                     pszFilename, pszModName, &paArgs[1]);

    NOREF(pCmd);
    return VINF_SUCCESS;
}


/**
 * The 'loadseg' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLoadSeg(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Validate the parsing and make sense of the input.
     * This is a mess as usual because we don't trust the parser yet.
     */
    AssertReturn(    cArgs >= 3
                 &&  cArgs <= 4
                 &&  paArgs[0].enmType == DBGCVAR_TYPE_STRING
                 &&  DBGCVAR_ISPOINTER(paArgs[1].enmType)
                 &&  paArgs[2].enmType == DBGCVAR_TYPE_NUMBER,
                 VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);

    const char     *pszFilename = paArgs[0].u.pszString;

    DBGFADDRESS     ModAddress;
    int rc = pCmdHlp->pfnVarToDbgfAddr(pCmdHlp, &paArgs[1], &ModAddress);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "pfnVarToDbgfAddr: %Dv\n", &paArgs[1]);

    RTDBGSEGIDX     iModSeg     = (RTDBGSEGIDX)paArgs[1].u.u64Number;
    if (    iModSeg != paArgs[2].u.u64Number
        ||  iModSeg > RTDBGSEGIDX_LAST)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Segment index out of range: %Dv; range={0..%#x}\n", &paArgs[1], RTDBGSEGIDX_LAST);

    const char     *pszModName  = NULL;
    if (cArgs >= 4)
    {
        AssertReturn(paArgs[3].enmType == DBGCVAR_TYPE_STRING, VERR_DBGC_PARSE_INCORRECT_ARG_TYPE);
        pszModName = paArgs[3].u.pszString;
    }

    /*
     * Call the debug info manager about this loading.
     */
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    rc = DBGFR3AsLoadImage(pVM, pDbgc->hDbgAs, pszFilename, pszModName, &ModAddress, iModSeg, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGFR3ModuleLoadImage(,,'%s','%s',%Dv,)\n",
                                     pszFilename, pszModName, &paArgs[1]);

    NOREF(pCmd);
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
static DECLCALLBACK(int) dbgcCmdLoadSyms(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Validate the parsing and make sense of the input.
     * This is a mess as usual because we don't trust the parser yet.
     */
    if (    cArgs < 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    {
        AssertMsgFailed(("Parse error, first argument required to be string!\n"));
        return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;
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
                return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;
            }
            pszModule = paArgs[iArg].u.pszString;
            iArg++;
            if (iArg < cArgs)
            {
                if (!DBGCVAR_ISPOINTER(paArgs[iArg].enmType))
                {
                    AssertMsgFailed(("Parse error, module argument required to be GC pointer!\n"));
                    return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;
                }
                int rc = DBGCCmdHlpEval(pCmdHlp, &AddrVar, "%%(%Dv)", &paArgs[iArg]);
                if (RT_FAILURE(rc))
                    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Module address cast %%(%Dv) failed.", &paArgs[iArg]);
                ModuleAddress = paArgs[iArg].u.GCFlat;
                iArg++;
                if (iArg < cArgs)
                {
                    if (paArgs[iArg].enmType != DBGCVAR_TYPE_NUMBER)
                    {
                        AssertMsgFailed(("Parse error, module argument required to be an integer!\n"));
                        return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;
                    }
                    cbModule = (unsigned)paArgs[iArg].u.u64Number;
                    iArg++;
                    if (iArg < cArgs)
                    {
                        AssertMsgFailed(("Parse error, too many arguments!\n"));
                        return VERR_DBGC_PARSE_TOO_MANY_ARGUMENTS;
                    }
                }
            }
        }
    }

    /*
     * Call the debug info manager about this loading...
     */
    int rc = DBGFR3ModuleLoad(pVM, paArgs[0].u.pszString, Delta, pszModule, ModuleAddress, cbModule);
    if (RT_FAILURE(rc))
        return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "DBGInfoSymbolLoad(, '%s', %RGv, '%s', %RGv, 0)\n",
                                     paArgs[0].u.pszString, Delta, pszModule, ModuleAddress);

    NOREF(pCmd);
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
static DECLCALLBACK(int) dbgcCmdSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /* parse sanity check. */
    AssertMsg(paArgs[0].enmType == DBGCVAR_TYPE_STRING, ("expected string not %d as first arg!\n", paArgs[0].enmType));
    if (paArgs[0].enmType != DBGCVAR_TYPE_STRING)
        return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;


    /*
     * A variable must start with an alpha chars and only contain alpha numerical chars.
     */
    const char *pszVar = paArgs[0].u.pszString;
    if (!RT_C_IS_ALPHA(*pszVar) || *pszVar == '_')
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL,
            "syntax error: Invalid variable name '%s'. Variable names must match regex '[_a-zA-Z][_a-zA-Z0-9*'!", paArgs[0].u.pszString);

    while (RT_C_IS_ALNUM(*pszVar) || *pszVar == '_')
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
                return VERR_DBGC_PARSE_NO_MEMORY;
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
            return VERR_DBGC_PARSE_NO_MEMORY;
        }
        pDbgc->papVars = (PDBGCNAMEDVAR *)pv;
    }
    pDbgc->papVars[pDbgc->cVars++] = pVar;

    NOREF(pCmd); NOREF(pVM); NOREF(cArgs);
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
static DECLCALLBACK(int) dbgcCmdUnset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC   pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    for (unsigned  i = 0; i < cArgs; i++)
        AssertReturn(paArgs[i].enmType == DBGCVAR_TYPE_SYMBOL, VERR_DBGC_PARSE_BUG);

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

    NOREF(pCmd); NOREF(pVM);
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
static DECLCALLBACK(int) dbgcCmdLoadVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    /*
     * Don't trust the parser.
     */
    if (    cArgs != 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    {
        AssertMsgFailed(("Expected one string exactly!\n"));
        return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;
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
            while (RT_C_IS_BLANK(*psz))
                psz++;
            int i = (int)strlen(psz) - 1;
            while (i >= 0 && RT_C_IS_SPACE(psz[i]))
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

    NOREF(pCmd); NOREF(pVM);
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
static DECLCALLBACK(int) dbgcCmdShowVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    for (unsigned iVar = 0; iVar < pDbgc->cVars; iVar++)
    {
        int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "%-20s ", &pDbgc->papVars[iVar]->szName);
        if (!rc)
            rc = dbgcCmdFormat(pCmd, pCmdHlp, pVM, &pDbgc->papVars[iVar]->Var, 1);
        if (rc)
            return rc;
    }

    NOREF(paArgs); NOREF(cArgs);
    return 0;
}


/**
 * Extracts the plugin name from a plugin specifier that may or may not include
 * path and/or suffix.
 *
 * @returns VBox status code.
 *
 * @param   pszDst      Where to return the name. At least DBGCPLUGIN_MAX_NAME
 *                      worth of buffer space.
 * @param   pszPlugIn   The plugin specifier to parse.
 */
static int dbgcPlugInExtractName(char *pszDst, const char *pszPlugIn)
{
    /*
     * Parse out the name stopping at the extension.
     */
    const char *pszName = RTPathFilename(pszPlugIn);
    if (!pszName || !*pszName)
        return VERR_INVALID_NAME;
    if (!RTStrNICmp(pszName, DBGC_PLUG_IN_PREFIX, sizeof(DBGC_PLUG_IN_PREFIX) - 1))
    {
        pszName += sizeof(DBGC_PLUG_IN_PREFIX) - 1;
        if (!*pszName)
            return VERR_INVALID_NAME;
    }

    int         ch;
    size_t      cchName = 0;
    while (   (ch = pszName[cchName]) != '\0'
           && ch != '.')
    {
        if (    !RT_C_IS_ALPHA(ch)
            &&  (   !RT_C_IS_DIGIT(ch)
                 || cchName == 0))
            return VERR_INVALID_NAME;
        cchName++;
    }

    if (cchName >= DBGCPLUGIN_MAX_NAME)
        return VERR_OUT_OF_RANGE;

    /*
     * We're very picky about the extension if there is no path.
     */
    if (    ch == '.'
        &&  !RTPathHavePath(pszPlugIn)
        &&  RTStrICmp(&pszName[cchName], RTLdrGetSuff()))
        return VERR_INVALID_NAME;

    /*
     * Copy it.
     */
    memcpy(pszDst, pszName, cchName);
    pszDst[cchName] = '\0';
    return VINF_SUCCESS;
}


/**
 * Locate a plug-in in list.
 *
 * @returns Pointer to the plug-in tracking structure.
 * @param   pDbgc               Pointer to the DBGC instance data.
 * @param   pszName             The name of the plug-in we're looking for.
 * @param   ppPrev              Where to optionally return the pointer to the
 *                              previous list member.
 */
static PDBGCPLUGIN dbgcPlugInLocate(PDBGC pDbgc, const char *pszName, PDBGCPLUGIN *ppPrev)
{
    PDBGCPLUGIN pPrev = NULL;
    PDBGCPLUGIN pCur  = pDbgc->pPlugInHead;
    while (pCur)
    {
        if (!RTStrICmp(pCur->szName, pszName))
        {
            if (ppPrev)
                *ppPrev = pPrev;
            return pCur;
        }

        /* advance */
        pPrev = pCur;
        pCur  = pCur->pNext;
    }
    return NULL;
}


/**
 * Try load the specified plug-in module.
 *
 * @returns VINF_SUCCESS on success, path error or loader error on failure.
 *
 * @param   pPlugIn     The plugin tracing record.
 * @param   pszModule   Module name.
 */
static int dbgcPlugInTryLoad(PDBGCPLUGIN pPlugIn, const char *pszModule)
{
    /*
     * Load it and try resolve the entry point.
     */
    int rc = RTLdrLoad(pszModule, &pPlugIn->hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(pPlugIn->hLdrMod, DBGC_PLUG_IN_ENTRYPOINT, (void **)&pPlugIn->pfnEntry);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        LogRel(("DBGC: RTLdrGetSymbol('%s', '%s',) -> %Rrc\n", pszModule, DBGC_PLUG_IN_ENTRYPOINT, rc));

        RTLdrClose(pPlugIn->hLdrMod);
        pPlugIn->hLdrMod = NIL_RTLDRMOD;
    }
    return rc;
}


/**
 * RTPathTraverseList callback.
 *
 * @returns See FNRTPATHTRAVERSER.
 *
 * @param   pchPath     See FNRTPATHTRAVERSER.
 * @param   cchPath     See FNRTPATHTRAVERSER.
 * @param   pvUser1     The plug-in specifier.
 * @param   pvUser2     The plug-in tracking record.
 */
static DECLCALLBACK(int) dbgcPlugInLoadCallback(const char *pchPath, size_t cchPath, void *pvUser1, void *pvUser2)
{
    PDBGCPLUGIN pPlugIn   = (PDBGCPLUGIN)pvUser2;
    const char *pszPlugIn = (const char *)pvUser1;

    /*
     * Join the path and the specified plug-in module name, first with the
     * prefix and then without it.
     */
    size_t      cchModule = cchPath + 1 + strlen(pszPlugIn) + sizeof(DBGC_PLUG_IN_PREFIX) + 8;
    char       *pszModule = (char *)alloca(cchModule);
    AssertReturn(pszModule, VERR_TRY_AGAIN);
    memcpy(pszModule, pchPath, cchPath);
    pszModule[cchPath] = '\0';

    int rc = RTPathAppend(pszModule, cchModule, DBGC_PLUG_IN_PREFIX);
    AssertRCReturn(rc, VERR_TRY_AGAIN);
    strcat(pszModule, pszPlugIn);
    rc = dbgcPlugInTryLoad(pPlugIn, pszModule);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    pszModule[cchPath] = '\0';
    rc = RTPathAppend(pszModule, cchModule, pszPlugIn);
    AssertRCReturn(rc, VERR_TRY_AGAIN);
    rc = dbgcPlugInTryLoad(pPlugIn, pszModule);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    return VERR_TRY_AGAIN;
}


/**
 * Loads a plug-in.
 *
 * @returns VBox status code. If pCmd is specified, it's the return from
 *          DBGCCmdHlpFail.
 * @param   pDbgc               The DBGC instance data.
 * @param   pszName             The plug-in name.
 * @param   pszPlugIn           The plug-in module name.
 * @param   pCmd                The command pointer if invoked by the user, NULL
 *                              if invoked from elsewhere.
 */
static int dbgcPlugInLoad(PDBGC pDbgc, const char *pszName, const char *pszPlugIn, PCDBGCCMD pCmd)
{
    PDBGCCMDHLP pCmdHlp = &pDbgc->CmdHlp;

    /*
     * Try load it.  If specified with a path, we're assuming the user
     * wants to load a plug-in from some specific location.  Otherwise
     * search for it.
     */
    PDBGCPLUGIN pPlugIn = (PDBGCPLUGIN)RTMemAllocZ(sizeof(*pPlugIn));
    if (!pPlugIn)
        return pCmd
             ? DBGCCmdHlpFail(pCmdHlp, pCmd, "out of memory\n")
             : VERR_NO_MEMORY;
    strcpy(pPlugIn->szName, pszName);

    int rc;
    if (RTPathHavePath(pszPlugIn))
        rc = dbgcPlugInTryLoad(pPlugIn, pszPlugIn);
    else
    {
        /* 1. The private architecture directory. */
        char szPath[4*_1K];
        rc = RTPathAppPrivateArch(szPath, sizeof(szPath));
        if (RT_SUCCESS(rc))
            rc = RTPathTraverseList(szPath, '\0', dbgcPlugInLoadCallback, (void *)pszPlugIn, pPlugIn);
        if (RT_FAILURE(rc))
        {
            /* 2. The DBGC PLUGIN_PATH variable. */
            DBGCVAR PathVar;
            int rc2 = DBGCCmdHlpEval(pCmdHlp, &PathVar, "$PLUGIN_PATH");
            if (    RT_SUCCESS(rc2)
                &&  PathVar.enmType == DBGCVAR_TYPE_STRING)
                rc = RTPathTraverseList(PathVar.u.pszString, ';', dbgcPlugInLoadCallback, (void *)pszPlugIn, pPlugIn);
            if (RT_FAILURE_NP(rc))
            {
                /* 3. The DBGC_PLUGIN_PATH environment variable. */
                rc2 = RTEnvGetEx(RTENV_DEFAULT, "DBGC_PLUGIN_PATH", szPath, sizeof(szPath), NULL);
                if (RT_SUCCESS(rc2))
                    rc = RTPathTraverseList(szPath, ';', dbgcPlugInLoadCallback, (void *)pszPlugIn, pPlugIn);
            }
        }
    }
    if (RT_FAILURE(rc))
    {
        RTMemFree(pPlugIn);
        return pCmd
            ? DBGCCmdHlpFail(pCmdHlp, pCmd, "could not find/load '%s'\n", pszPlugIn)
            : rc;
    }

    /*
     * Try initialize it.
     */
    rc = pPlugIn->pfnEntry(DBGCPLUGINOP_INIT, pDbgc->pVM, VBOX_VERSION);
    if (RT_FAILURE(rc))
    {
        RTLdrClose(pPlugIn->hLdrMod);
        RTMemFree(pPlugIn);
        return pCmd
            ? DBGCCmdHlpFail(pCmdHlp, pCmd, "initialization of plug-in '%s' failed with rc=%Rrc\n", pszPlugIn, rc)
            : rc;
    }

    /*
     * Link it and we're good.
     */
    pPlugIn->pNext = pDbgc->pPlugInHead;
    pDbgc->pPlugInHead = pPlugIn;
    DBGCCmdHlpPrintf(pCmdHlp, "Loaded plug-in '%s'.\n", pPlugIn->szName);
    return VINF_SUCCESS;
}




/**
 * Automatically load plug-ins from the architecture private directory of
 * VirtualBox.
 *
 * This is called during console init.
 *
 * @param   pDbgc       The DBGC instance data.
 */
void dbgcPlugInAutoLoad(PDBGC pDbgc)
{
    /*
     * Open the architecture specific directory with a filter on our prefix
     * and names including a dot.
     */
    const char *pszSuff = RTLdrGetSuff();
    size_t      cchSuff = strlen(pszSuff);

    char szPath[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szPath, sizeof(szPath) - cchSuff);
    AssertRCReturnVoid(rc);
    size_t offDir = strlen(szPath);

    rc = RTPathAppend(szPath, sizeof(szPath) - cchSuff, DBGC_PLUG_IN_PREFIX "*");
    AssertRCReturnVoid(rc);
    strcat(szPath, pszSuff);

    PRTDIR pDir;
    rc = RTDirOpenFiltered(&pDir, szPath, RTDIRFILTER_WINNT, 0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Now read it and try load each of the plug-in modules.
         */
        RTDIRENTRY DirEntry;
        while (RT_SUCCESS(RTDirRead(pDir, &DirEntry, NULL)))
        {
            szPath[offDir] = '\0';
            rc = RTPathAppend(szPath, sizeof(szPath), DirEntry.szName);
            if (RT_SUCCESS(rc))
            {
                char szName[DBGCPLUGIN_MAX_NAME];
                rc = dbgcPlugInExtractName(szName, DirEntry.szName);
                if (RT_SUCCESS(rc))
                    dbgcPlugInLoad(pDbgc, szName, szPath, NULL /*pCmd*/);
            }
        }

        RTDirClose(pDir);
    }
}


/**
 * The 'loadplugin' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdLoadPlugIn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Loop thru the plugin names.
     */
    for (unsigned i = 0; i < cArgs; i++)
    {
        const char *pszPlugIn = paArgs[i].u.pszString;

        /* Extract the plug-in name. */
        char szName[DBGCPLUGIN_MAX_NAME];
        int rc = dbgcPlugInExtractName(szName, pszPlugIn);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Malformed plug-in name: '%s'\n", pszPlugIn);

        /* Loaded? */
        PDBGCPLUGIN pPlugIn = dbgcPlugInLocate(pDbgc, szName, NULL);
        if (pPlugIn)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "'%s' is already loaded\n", szName);

        /* Load it. */
        rc = dbgcPlugInLoad(pDbgc, szName, pszPlugIn, pCmd);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Unload all plug-ins.
 *
 * @param   pDbgc       The DBGC instance data.
 */
void dbgcPlugInUnloadAll(PDBGC pDbgc)
{
    while (pDbgc->pPlugInHead)
    {
        PDBGCPLUGIN pPlugIn = pDbgc->pPlugInHead;
        pDbgc->pPlugInHead = pPlugIn->pNext;

        if (    pDbgc->pVM /* prevents trouble during destruction. */
            &&  pDbgc->pVM->enmVMState < VMSTATE_DESTROYING)
        {
            pPlugIn->pfnEntry(DBGCPLUGINOP_TERM, pDbgc->pVM, 0);
            RTLdrClose(pPlugIn->hLdrMod);
        }
        pPlugIn->hLdrMod = NIL_RTLDRMOD;

        RTMemFree(pPlugIn);
    }
}


/**
 * The 'unload' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdUnloadPlugIn(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);

    /*
     * Loop thru the plugin names.
     */
    for (unsigned i = 0; i < cArgs; i++)
    {
        const char *pszPlugIn = paArgs[i].u.pszString;

        /* Extract the plug-in name. */
        char szName[DBGCPLUGIN_MAX_NAME];
        int rc = dbgcPlugInExtractName(szName, pszPlugIn);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "Malformed plug-in name: '%s'\n", pszPlugIn);

        /* Loaded? */
        PDBGCPLUGIN pPrevPlugIn;
        PDBGCPLUGIN pPlugIn = dbgcPlugInLocate(pDbgc, szName, &pPrevPlugIn);
        if (!pPlugIn)
            return DBGCCmdHlpFail(pCmdHlp, pCmd, "'%s' is not\n", szName);

        /*
         * Terminate and unload it.
         */
        pPlugIn->pfnEntry(DBGCPLUGINOP_TERM, pDbgc->pVM, 0);
        RTLdrClose(pPlugIn->hLdrMod);
        pPlugIn->hLdrMod = NIL_RTLDRMOD;

        if (pPrevPlugIn)
            pPrevPlugIn->pNext = pPlugIn->pNext;
        else
            pDbgc->pPlugInHead = pPlugIn->pNext;
        RTMemFree(pPlugIn->pNext);
        DBGCCmdHlpPrintf(pCmdHlp, "Unloaded plug-in '%s'\n", szName);
    }

    return VINF_SUCCESS;
}


/**
 * The 'showplugins' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdShowPlugIns(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    PDBGC       pDbgc = DBGC_CMDHLP2DBGC(pCmdHlp);
    PDBGCPLUGIN pPlugIn = pDbgc->pPlugInHead;
    if (!pPlugIn)
        return DBGCCmdHlpPrintf(pCmdHlp, "No plug-ins loaded\n");

    DBGCCmdHlpPrintf(pCmdHlp, "Plug-ins: %s", pPlugIn->szName);
    for (;;)
    {
        pPlugIn = pPlugIn->pNext;
        if (!pPlugIn)
            break;
        DBGCCmdHlpPrintf(pCmdHlp, ", %s", pPlugIn->szName);
    }
    return DBGCCmdHlpPrintf(pCmdHlp, "\n");
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
static DECLCALLBACK(int) dbgcCmdHarakiri(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    Log(("dbgcCmdHarakiri\n"));
    for (;;)
        exit(126);
    NOREF(pCmd); NOREF(pCmdHlp); NOREF(pVM); NOREF(paArgs); NOREF(cArgs);
}


/**
 * The 'writecore' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) dbgcCmdWriteCore(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    Log(("dbgcCmdWriteCore\n"));

    /*
     * Validate input, lots of paranoia here.
     */
    if (    cArgs != 1
        ||  paArgs[0].enmType != DBGCVAR_TYPE_STRING)
    {
        AssertMsgFailed(("Expected one string exactly!\n"));
        return VERR_DBGC_PARSE_INCORRECT_ARG_TYPE;
    }

    const char *pszDumpPath = paArgs[0].u.pszString;
    if (!pszDumpPath)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Missing file path.\n");

    int rc = DBGFR3CoreWrite(pVM, pszDumpPath, true /*fReplaceFile*/);
    if (RT_FAILURE(rc))
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "DBGFR3WriteCore failed. rc=%Rrc\n", rc);

    return VINF_SUCCESS;
}



/**
 * @callback_method_impl{The randu32() function implementation.}
 */
static DECLCALLBACK(int) dbgcFuncRandU32(PCDBGCFUNC pFunc, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, uint32_t cArgs,
                                         PDBGCVAR pResult)
{
    AssertReturn(cArgs == 0, VERR_DBGC_PARSE_BUG);
    uint32_t u32 = RTRandU32();
    DBGCVAR_INIT_NUMBER(pResult, u32);
    NOREF(pFunc); NOREF(pCmdHlp); NOREF(pVM); NOREF(paArgs);
    return VINF_SUCCESS;
}

