/** $Id$ */
/** @file
 * DBGC - Debugger Console, Native Commands.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
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
static DECLCALLBACK(int) dbgcCmdHelp(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdQuit(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdStop(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdInfo(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLog(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLogDest(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLogFlags(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdFormat(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLoadSyms(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdSet(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdUnset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdLoadVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdShowVars(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdHarakiri(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdEcho(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) dbgcCmdRunScript(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);


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
    {  1,           ~0,         DBGCVAR_CAT_STRING,     0,                              "strings",      "One or more strings." },
};

/** Filename string. */
static const DBGCVARDESC    g_aArgFilename[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  1,           1,          DBGCVAR_CAT_STRING,     0,                              "path",         "Filename string." },
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
const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDescs,         cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "bye",        0,        0,        NULL,               0,                          NULL,               0,          dbgcCmdQuit,        "",                     "Exits the debugger." },
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

/** The number of native commands. */
const unsigned g_cCmds = RT_ELEMENTS(g_aCmds);


/**
 * Pointer to head of the list of external commands.
 */
static PDBGCEXTCMDS g_pExtCmdsHead;     /** @todo rw protect g_pExtCmdsHead! */
/** Locks the g_pExtCmdsHead list for reading. */
#define DBGCEXTCMDS_LOCK_RD()       do { } while (0)
/** Locks the g_pExtCmdsHead list for writing. */
#define DBGCEXTCMDS_LOCK_WR()       do { } while (0)
/** UnLocks the g_pExtCmdsHead list after reading. */
#define DBGCEXTCMDS_UNLOCK_RD()     do { } while (0)
/** UnLocks the g_pExtCmdsHead list after writing. */
#define DBGCEXTCMDS_UNLOCK_WR()     do { } while (0)




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
PCDBGCCMD dbgcRoutineLookup(PDBGC pDbgc, const char *pachName, size_t cchName, bool fExternal)
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
        unsigned cLeft = g_cOps;
        while (cLeft > 0)
        {
            for (i = 0; i < g_cOps; i++)
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
               for (i = 0; i < g_cOps; i++)
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

