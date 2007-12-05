/* $Id$ */
/** @file
 * STAM - The Statistics Manager.
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
#define LOG_GROUP LOG_GROUP_STAM
#include <VBox/stam.h>
#include "STAMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/dbg.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/alloc.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Argument structure for stamR3PrintOne().
 */
typedef struct STAMR3PRINTONEARGS
{
    PVM pVM;
    void *pvArg;
    DECLCALLBACKMEMBER(void, pfnPrintf)(struct STAMR3PRINTONEARGS *pvArg, const char *pszFormat, ...);
} STAMR3PRINTONEARGS, *PSTAMR3PRINTONEARGS;


/**
 * Argument structure to stamR3EnumOne().
 */
typedef struct STAMR3ENUMONEARGS
{
    PVM             pVM;
    PFNSTAMR3ENUM   pfnEnum;
    void           *pvUser;
} STAMR3ENUMONEARGS, *PSTAMR3ENUMONEARGS;


/**
 * The snapshot status structure.
 * Argument package passed to stamR3SnapshotOne, stamR3SnapshotPrintf and stamR3SnapshotOutput.
 */
typedef struct STAMR3SNAPSHOTONE
{
    /** Pointer to the buffer start. */
    char           *pszStart;
    /** Pointer to the buffer end. */
    char           *pszEnd;
    /** Pointer to the current buffer position. */
    char           *psz;
    /** The VM handle (just in case). */
    PVM             pVM;
    /** The number of bytes allocated. */
    size_t          cbAllocated;
    /** The status code. */
    int             rc;
    /** Whether to include the description strings. */
    bool            fWithDesc;
} STAMR3SNAPSHOTONE, *PSTAMR3SNAPSHOTONE;


/**
 * Init record for a ring-0 statistic sample.
 */
typedef struct STAMR0SAMPLE
{
    /** The GVMMSTATS structure offset of the variable. */
    unsigned        offVar;
    /** The type. */
    STAMTYPE        enmType;
    /** The unit. */
    STAMUNIT        enmUnit;
    /** The name. */
    const char     *pszName;
    /** The description. */
    const char     *pszDesc;
} STAMR0SAMPLE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int stamR3Register(PVM pVM, void *pvSample, PFNSTAMR3CALLBACKRESET pfnReset, PFNSTAMR3CALLBACKPRINT pfnPrint,
                          STAMTYPE enmType, STAMVISIBILITY enmVisibility, const char *pszName, STAMUNIT enmUnit, const char *pszDesc);
static int stamR3ResetOne(PSTAMDESC pDesc, void *pvArg);
static DECLCALLBACK(void) stamR3EnumLogPrintf(PSTAMR3PRINTONEARGS pvArg, const char *pszFormat, ...);
static DECLCALLBACK(void) stamR3EnumRelLogPrintf(PSTAMR3PRINTONEARGS pvArg, const char *pszFormat, ...);
static DECLCALLBACK(void) stamR3EnumPrintf(PSTAMR3PRINTONEARGS pvArg, const char *pszFormat, ...);
static int stamR3SnapshotOne(PSTAMDESC pDesc, void *pvArg);
static int stamR3SnapshotPrintf(PSTAMR3SNAPSHOTONE pThis, const char *pszFormat, ...);
static int stamR3PrintOne(PSTAMDESC pDesc, void *pvArg);
static int stamR3EnumOne(PSTAMDESC pDesc, void *pvArg);
static bool stamR3MultiMatch(const char * const *papszExpressions, unsigned cExpressions, unsigned *piExpression, const char *pszName);
static char **stamR3SplitPattern(const char *pszPat, unsigned *pcExpressions, char **ppszCopy);
static int stamR3Enum(PVM pVM, const char *pszPat, bool fUpdateRing0, int (pfnCallback)(PSTAMDESC pDesc, void *pvArg), void *pvArg);
static void stamR3Ring0StatsRegister(PVM pVM);
static void stamR3Ring0StatsUpdate(PVM pVM, const char *pszPat);
static void stamR3Ring0StatsUpdateMulti(PVM pVM, const char * const *papszExpressions, unsigned cExpressions);

#ifdef VBOX_WITH_DEBUGGER
static DECLCALLBACK(int)  stamR3CmdStats(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(void) stamR3EnumDbgfPrintf(PSTAMR3PRINTONEARGS pArgs, const char *pszFormat, ...);
static DECLCALLBACK(int)  stamR3CmdStatsReset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_DEBUGGER
/** Pattern argument. */
static const DBGCVARDESC    g_aArgPat[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "pattern",      "Which samples the command shall be applied to. Use '*' as wildcard. Use ';' to separate expression." }
};

/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDesc,          cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "stats",      0,        1,        &g_aArgPat[0],      ELEMENTS(g_aArgPat),        NULL,               0,          stamR3CmdStats,     "[pattern]",        "Display statistics." },
    { "statsreset", 0,        1,        &g_aArgPat[0],      ELEMENTS(g_aArgPat),        NULL,               0,          stamR3CmdStatsReset,"[pattern]",        "Resets statistics." }
};
#endif


/**
 * The GVMM mapping records.
 */
static const STAMR0SAMPLE g_aGVMMStats[] =
{
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cHaltCalls),        STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/HaltCalls", "The number of calls to GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cHaltBlocking),     STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/HaltBlocking", "The number of times we did go to sleep in GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cHaltTimeouts),     STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/HaltTimeouts", "The number of times we timed out in GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cHaltNotBlocking),  STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/HaltNotBlocking", "The number of times we didn't go to sleep in GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cHaltWakeUps),      STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/HaltWakeUps", "The number of wake ups done during GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cWakeUpCalls),      STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/WakeUpCalls", "The number of calls to GVMMR0WakeUp." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cWakeUpNotHalted),  STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/WakeUpNotHalted", "The number of times the EMT thread wasn't actually halted when GVMMR0WakeUp was called." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cWakeUpWakeUps),    STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/WakeUpWakeUps", "The number of wake ups done during GVMMR0WakeUp (not counting the explicit one)." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cPollCalls),        STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/PollCalls", "The number of calls to GVMMR0SchedPoll." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cPollHalts),        STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/PollHalts", "The number of times the EMT has halted in a GVMMR0SchedPoll call." },
    { RT_UOFFSETOF(GVMMSTATS, SchedVM.cPollWakeUps),      STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/VM/PollWakeUps", "The number of wake ups done during GVMMR0SchedPoll." },

    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cHaltCalls),       STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/HaltCalls", "The number of calls to GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cHaltBlocking),    STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/HaltBlocking", "The number of times we did go to sleep in GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cHaltTimeouts),    STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/HaltTimeouts", "The number of times we timed out in GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cHaltNotBlocking), STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/HaltNotBlocking", "The number of times we didn't go to sleep in GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cHaltWakeUps),     STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/HaltWakeUps", "The number of wake ups done during GVMMR0SchedHalt." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cWakeUpCalls),     STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/WakeUpCalls", "The number of calls to GVMMR0WakeUp." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cWakeUpNotHalted), STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/WakeUpNotHalted", "The number of times the EMT thread wasn't actually halted when GVMMR0WakeUp was called." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cWakeUpWakeUps),   STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/WakeUpWakeUps", "The number of wake ups done during GVMMR0WakeUp (not counting the explicit one)." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cPollCalls),       STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/PollCalls", "The number of calls to GVMMR0SchedPoll." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cPollHalts),       STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/PollHalts", "The number of times the EMT has halted in a GVMMR0SchedPoll call." },
    { RT_UOFFSETOF(GVMMSTATS, SchedSum.cPollWakeUps),     STAMTYPE_U64_RESET, STAMUNIT_CALLS, "/GVMM/Sum/PollWakeUps", "The number of wake ups done during GVMMR0SchedPoll." },

    { RT_UOFFSETOF(GVMMSTATS, cVMs),                      STAMTYPE_U32,       STAMUNIT_CALLS, "/GVMM/VMs", "The number of VMs accessible to the caller." },
};


/**
 * Initializes the STAM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
STAMR3DECL(int) STAMR3Init(PVM pVM)
{
    LogFlow(("STAMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, stam.s) & 31));
    AssertRelease(sizeof(pVM->stam.s) <= sizeof(pVM->stam.padding));

    /*
     * Setup any fixed pointers and offsets.
     */
    pVM->stam.s.offVM = RT_OFFSETOF(VM, stam);
    int rc = RTSemRWCreate(&pVM->stam.s.RWSem);
    AssertRC(rc);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Register the ring-0 statistics (GVMM/GMM).
     */
    stamR3Ring0StatsRegister(pVM);

#ifdef VBOX_WITH_DEBUGGER
    /*
     * Register debugger commands.
     */
    static bool fRegisteredCmds = false;
    if (!fRegisteredCmds)
    {
        int rc = DBGCRegisterCommands(&g_aCmds[0], ELEMENTS(g_aCmds));
        if (VBOX_SUCCESS(rc))
            fRegisteredCmds = true;
    }
#endif

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 */
STAMR3DECL(void) STAMR3Relocate(PVM pVM)
{
    LogFlow(("STAMR3Relocate\n"));
    NOREF(pVM);
}


/**
 * Terminates the STAM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
STAMR3DECL(int) STAMR3Term(PVM pVM)
{
    /*
     * Free used memory and the RWLock.
     */
    PSTAMDESC   pCur = pVM->stam.s.pHead;
    while (pCur)
    {
        void *pvFree = pCur;
        pCur = pCur->pNext;
        RTMemFree(pvFree);
    }
    pVM->stam.s.pHead = NULL;

    /* careful here as we might be called twice in on some failure paths (?) */
    if (pVM->stam.s.RWSem != NIL_RTSEMRW)
        RTSemRWDestroy(pVM->stam.s.RWSem);
    pVM->stam.s.RWSem = NIL_RTSEMRW;
    return VINF_SUCCESS;
}




/**
 * Registers a sample with the statistics mamanger.
 *
 * Statistics are maintained on a per VM basis and should therefore
 * be registered during the VM init stage. However, there is not problem
 * registering temporary samples or samples for hotpluggable devices. Samples
 * can be deregisterd using the STAMR3Deregister() function, but note that
 * this is only necessary for temporary samples or hotpluggable devices.
 *
 * It is not possible to register the same sample twice.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
 * @param   pszName     Sample name. The name is on this form "/<component>/<sample>".
 *                      Further nesting is possible.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 */
STAMR3DECL(int)  STAMR3Register(PVM pVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    AssertReturn(enmType != STAMTYPE_CALLBACK, VERR_INVALID_PARAMETER);
    return stamR3Register(pVM, pvSample, NULL, NULL, enmType, enmVisibility, pszName, enmUnit, pszDesc);
}


/**
 * Same as STAMR3Register except that the name is specified in a
 * RTStrPrintf like fashion.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 * @param   pszName     The sample name format string.
 * @param   ...         Arguments to the format string.
 */
STAMR3DECL(int)  STAMR3RegisterF(PVM pVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                 const char *pszDesc, const char *pszName, ...)
{
    va_list args;
    va_start(args, pszName);
    int rc = STAMR3RegisterV(pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    va_end(args);
    return rc;
}


/**
 * Same as STAMR3Register except that the name is specified in a
 * RTStrPrintfV like fashion.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 * @param   pszName     The sample name format string.
 * @param   args        Arguments to the format string.
 */
STAMR3DECL(int)  STAMR3RegisterV(PVM pVM, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                 const char *pszDesc, const char *pszName, va_list args)
{
    AssertReturn(enmType != STAMTYPE_CALLBACK, VERR_INVALID_PARAMETER);

    char *pszFormattedName;
    RTStrAPrintfV(&pszFormattedName, pszName, args);
    if (!pszFormattedName)
        return VERR_NO_MEMORY;

    int rc = STAMR3Register(pVM, pvSample, enmType, enmVisibility, pszFormattedName, enmUnit, pszDesc);
    RTStrFree(pszFormattedName);
    return rc;
}


/**
 * Similar to STAMR3Register except for the two callbacks, the implied type (STAMTYPE_CALLBACK),
 * and name given in an RTStrPrintf like fashion.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
 * @param   enmUnit     Sample unit.
 * @param   pfnReset    Callback for resetting the sample. NULL should be used if the sample can't be reset.
 * @param   pfnPrint    Print the sample.
 * @param   pszDesc     Sample description.
 * @param   pszName     The sample name format string.
 * @param   ...         Arguments to the format string.
 * @remark  There is currently no device or driver variant of this API. Add one if it should become necessary!
 */
STAMR3DECL(int)  STAMR3RegisterCallback(PVM pVM, void *pvSample, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                        PFNSTAMR3CALLBACKRESET pfnReset, PFNSTAMR3CALLBACKPRINT pfnPrint,
                                        const char *pszDesc, const char *pszName, ...)
{
    va_list args;
    va_start(args, pszName);
    int rc = STAMR3RegisterCallbackV(pVM, pvSample, enmVisibility, enmUnit, pfnReset, pfnPrint, pszDesc, pszName, args);
    va_end(args);
    return rc;
}


/**
 * Same as STAMR3RegisterCallback() except for the ellipsis which is a va_list here.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample.
 * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
 * @param   enmUnit     Sample unit.
 * @param   pfnReset    Callback for resetting the sample. NULL should be used if the sample can't be reset.
 * @param   pfnPrint    Print the sample.
 * @param   pszDesc     Sample description.
 * @param   pszName     The sample name format string.
 * @param   args        Arguments to the format string.
 * @remark  There is currently no device or driver variant of this API. Add one if it should become necessary!
 */
STAMR3DECL(int)  STAMR3RegisterCallbackV(PVM pVM, void *pvSample, STAMVISIBILITY enmVisibility, STAMUNIT enmUnit,
                                         PFNSTAMR3CALLBACKRESET pfnReset, PFNSTAMR3CALLBACKPRINT pfnPrint,
                                         const char *pszDesc, const char *pszName, va_list args)
{
    char *pszFormattedName;
    RTStrAPrintfV(&pszFormattedName, pszName, args);
    if (!pszFormattedName)
        return VERR_NO_MEMORY;

    int rc = stamR3Register(pVM, pvSample, pfnReset, pfnPrint, STAMTYPE_CALLBACK, enmVisibility, pszFormattedName, enmUnit, pszDesc);
    RTStrFree(pszFormattedName);
    return rc;
}


/**
 * Internal worker for the different register calls.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample.
 * @param   pfnReset    Callback for resetting the sample. NULL should be used if the sample can't be reset.
 * @param   pfnPrint    Print the sample.
 * @param   enmType     Sample type. This indicates what pvSample is pointing at.
 * @param   enmVisibility  Visibility type specifying whether unused statistics should be visible or not.
 * @param   enmUnit     Sample unit.
 * @param   pszDesc     Sample description.
 * @param   pszName     The sample name format string.
 * @param   args        Arguments to the format string.
 * @remark  There is currently no device or driver variant of this API. Add one if it should become necessary!
 */
static int stamR3Register(PVM pVM, void *pvSample, PFNSTAMR3CALLBACKRESET pfnReset, PFNSTAMR3CALLBACKPRINT pfnPrint,
                          STAMTYPE enmType, STAMVISIBILITY enmVisibility, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    STAM_LOCK_WR(pVM);

    /*
     * Check if exists.
     */
    PSTAMDESC   pPrev = NULL;
    PSTAMDESC   pCur = pVM->stam.s.pHead;
    while (pCur)
    {
        int iDiff = strcmp(pCur->pszName, pszName);
        /* passed it */
        if (iDiff > 0)
            break;
        /* found it. */
        if (!iDiff)
        {
            STAM_UNLOCK_WR(pVM);
            AssertMsgFailed(("Duplicate sample name: %s\n", pszName));
            return VERR_ALREADY_EXISTS;
        }

        /* next */
        pPrev = pCur;
        pCur = pCur->pNext;
    }

    /*
     * Create a new node and insert it at the current location.
     */
    int rc;
    size_t cchName = strlen(pszName) + 1;
    size_t cchDesc = pszDesc ? strlen(pszDesc) + 1 : 0;
    PSTAMDESC pNew = (PSTAMDESC)RTMemAlloc(sizeof(*pNew) + cchName + cchDesc);
    if (pNew)
    {
        pNew->pszName       = (char *)memcpy((char *)(pNew + 1), pszName, cchName);
        pNew->enmType       = enmType;
        pNew->enmVisibility = enmVisibility;
        if (enmType != STAMTYPE_CALLBACK)
            pNew->u.pv      = pvSample;
        else
        {
            pNew->u.Callback.pvSample = pvSample;
            pNew->u.Callback.pfnReset = pfnReset;
            pNew->u.Callback.pfnPrint = pfnPrint;
        }
        pNew->enmUnit       = enmUnit;
        pNew->pszDesc       = NULL;
        if (pszDesc)
            pNew->pszDesc   = (char *)memcpy((char *)(pNew + 1) + cchName,  pszDesc,  cchDesc);

        pNew->pNext         = pCur;
        if (pPrev)
            pPrev->pNext    = pNew;
        else
            pVM->stam.s.pHead = pNew;

        stamR3ResetOne(pNew, pVM);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    STAM_UNLOCK_WR(pVM);
    return rc;
}


/**
 * Deregisters a sample previously registered by STAR3Register().
 *
 * This is intended used for devices which can be unplugged and for
 * temporary samples.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pvSample    Pointer to the sample registered with STAMR3Register().
 */
STAMR3DECL(int)  STAMR3Deregister(PVM pVM, void *pvSample)
{
    STAM_LOCK_WR(pVM);

    /*
     * Search for it.
     */
    int         rc = VERR_INVALID_HANDLE;
    PSTAMDESC   pPrev = NULL;
    PSTAMDESC   pCur = pVM->stam.s.pHead;
    while (pCur)
    {
        if (pCur->u.pv == pvSample)
        {
            void *pvFree = pCur;
            pCur = pCur->pNext;
            if (pPrev)
                pPrev->pNext = pCur;
            else
                pVM->stam.s.pHead = pCur;

            RTMemFree(pvFree);
            rc = VINF_SUCCESS;
            continue;
        }

        /* next */
        pPrev = pCur;
        pCur = pCur->pNext;
    }

    STAM_UNLOCK_WR(pVM);
    return rc;
}


/**
 * Resets statistics for the specified VM.
 * It's possible to select a subset of the samples.
 *
 * @returns VBox status. (Basically, it cannot fail.)
 * @param   pVM         The VM handle.
 * @param   pszPat      The name matching pattern. See somewhere_where_this_is_described_in_detail.
 *                      If NULL all samples are reset.
 * @remarks Don't confuse this with the other 'XYZR3Reset' methods, it's not called at VM reset.
 */
STAMR3DECL(int)  STAMR3Reset(PVM pVM, const char *pszPat)
{
    int rc = VINF_SUCCESS;

    /* ring-0 */
    GVMMRESETSTATISTICSSREQ GVMMReq;
    //GMMRESETSTATISTICSSREQ GMMReq;
    bool fGVMMMatched = !pszPat || !*pszPat;
    //bool fGMMMatched = fGVMMMatched;
    if (fGVMMMatched)
        memset(&GVMMReq.Stats, 0xff, sizeof(GVMMReq.Stats));
    else
    {
        char *pszCopy;
        unsigned cExpressions;
        char **papszExpressions = stamR3SplitPattern(pszPat, &cExpressions, &pszCopy);
        if (!papszExpressions)
            return VERR_NO_MEMORY;

        /* GVMM */
        memset(&GVMMReq.Stats, 0, sizeof(GVMMReq.Stats));
        for (unsigned i = 0; i < RT_ELEMENTS(g_aGVMMStats); i++)
            if (stamR3MultiMatch(papszExpressions, cExpressions, NULL, g_aGVMMStats[i].pszName))
            {
                *((uint8_t *)&GVMMReq.Stats + g_aGVMMStats[i].offVar) = 0xff;
                fGVMMMatched = true;
            }

        /* GMM */
//        memset(&GMMReq.Stats, 0, sizeof(GMMReq.Stats));
//        for (unsigned i = 0; i < RT_ELEMENTS(g_aGMMStats); i++)
//            if (stamR3MultiMatch(papszExpressions, cExpressions, NULL, g_aGMMStats[i].pszName))
//            {
//                 *((uint8_t *)&GMMReq.Stats + g_aGMMStats[i].offVar) = 0xff;
//                 fGMMMatched = true;
//            }

        RTMemTmpFree(papszExpressions);
        RTStrFree(pszCopy);
    }

    STAM_LOCK_WR(pVM);
    if (fGVMMMatched)
    {
        GVMMReq.Hdr.cbReq = sizeof(GVMMReq);
        GVMMReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        GVMMReq.pSession = pVM->pSession;
        rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GVMM_RESET_STATISTICS, 0, &GVMMReq.Hdr);
    }

//    if (fGMMMatched)
//    {
//        GMMReq.Hdr.cbReq = sizeof(Req);
//        GMMReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
//        GMMReq.pSession = pVM->pSession;
//        rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_RESET_STATISTICS, 0, &Req.Hdr);
//    }

    /* and the reset */
    stamR3Enum(pVM, pszPat, false /* fUpdateRing0 */, stamR3ResetOne, pVM);

    STAM_UNLOCK_WR(pVM);
    return rc;
}


/**
 * Resets one statistics sample.
 * Callback for stamR3Enum().
 *
 * @returns VINF_SUCCESS
 * @param   pDesc   Pointer to the current descriptor.
 * @param   pvArg   User argument - The VM handle.
 */
static int stamR3ResetOne(PSTAMDESC pDesc, void *pvArg)
{
    switch (pDesc->enmType)
    {
        case STAMTYPE_COUNTER:
            ASMAtomicXchgU64(&pDesc->u.pCounter->c, 0);
            break;

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
            ASMAtomicXchgU64(&pDesc->u.pProfile->cPeriods, 0);
            ASMAtomicXchgU64(&pDesc->u.pProfile->cTicks, 0);
            ASMAtomicXchgU64(&pDesc->u.pProfile->cTicksMax, 0);
            ASMAtomicXchgU64(&pDesc->u.pProfile->cTicksMin, ~0);
            break;

        case STAMTYPE_RATIO_U32_RESET:
            ASMAtomicXchgU32(&pDesc->u.pRatioU32->u32A, 0);
            ASMAtomicXchgU32(&pDesc->u.pRatioU32->u32B, 0);
            break;

        case STAMTYPE_CALLBACK:
            if (pDesc->u.Callback.pfnReset)
                pDesc->u.Callback.pfnReset((PVM)pvArg, pDesc->u.Callback.pvSample);
            break;

        case STAMTYPE_U8_RESET:
        case STAMTYPE_X8_RESET:
            ASMAtomicXchgU8(pDesc->u.pu8, 0);
            break;

        case STAMTYPE_U16_RESET:
        case STAMTYPE_X16_RESET:
            ASMAtomicXchgU16(pDesc->u.pu16, 0);
            break;

        case STAMTYPE_U32_RESET:
        case STAMTYPE_X32_RESET:
            ASMAtomicXchgU32(pDesc->u.pu32, 0);
            break;

        case STAMTYPE_U64_RESET:
        case STAMTYPE_X64_RESET:
            ASMAtomicXchgU64(pDesc->u.pu64, 0);
            break;

        /* These are custom and will not be touched. */
        case STAMTYPE_U8:
        case STAMTYPE_X8:
        case STAMTYPE_U16:
        case STAMTYPE_X16:
        case STAMTYPE_U32:
        case STAMTYPE_X32:
        case STAMTYPE_U64:
        case STAMTYPE_X64:
        case STAMTYPE_RATIO_U32:
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", pDesc->enmType));
            break;
    }
    NOREF(pvArg);
    return VINF_SUCCESS;
}


/**
 * Get a snapshot of the statistics.
 * It's possible to select a subset of the samples.
 *
 * @returns VBox status. (Basically, it cannot fail.)
 * @param   pVM             The VM handle.
 * @param   pszPat          The name matching pattern. See somewhere_where_this_is_described_in_detail.
 *                          If NULL all samples are reset.
 * @param   fWithDesc       Whether to include the descriptions.
 * @param   ppszSnapshot    Where to store the pointer to the snapshot data.
 *                          The format of the snapshot should be XML, but that will have to be discussed
 *                          when this function is implemented.
 *                          The returned pointer must be freed by calling STAMR3SnapshotFree().
 * @param   pcchSnapshot    Where to store the size of the snapshot data. (Excluding the trailing '\0')
 */
STAMR3DECL(int) STAMR3Snapshot(PVM pVM, const char *pszPat, char **ppszSnapshot, size_t *pcchSnapshot, bool fWithDesc)
{
    STAMR3SNAPSHOTONE State = { NULL, NULL, NULL, pVM, 0, VINF_SUCCESS, fWithDesc };

    /*
     * Write the XML header.
     */
    /** @todo Make this proper & valid XML. */
    stamR3SnapshotPrintf(&State, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");

    /*
     * Write the content.
     */
    stamR3SnapshotPrintf(&State, "<Statistics>\n");
    STAM_LOCK_RD(pVM);
    int rc = stamR3Enum(pVM, pszPat, true /* fUpdateRing0 */, stamR3SnapshotOne, &State);
    STAM_UNLOCK_RD(pVM);
    stamR3SnapshotPrintf(&State, "</Statistics>\n");

    if (VBOX_SUCCESS(rc))
        rc = State.rc;
    else
    {
        RTMemFree(State.pszStart);
        State.pszStart = State.pszEnd = State.psz = NULL;
        State.cbAllocated = 0;
    }

    /*
     * Done.
     */
    *ppszSnapshot = State.pszStart;
    if (pcchSnapshot)
        *pcchSnapshot = State.psz - State.pszStart;
    return rc;
}


/**
 * stamR3Enum callback employed by STAMR3Snapshot.
 *
 * @returns VBox status code, but it's interpreted as 0 == success / !0 == failure by enmR3Enum.
 * @param   pDesc       The sample.
 * @param   pvArg       The snapshot status structure.
 */
static int stamR3SnapshotOne(PSTAMDESC pDesc, void *pvArg)
{
    PSTAMR3SNAPSHOTONE pThis = (PSTAMR3SNAPSHOTONE)pvArg;

    switch (pDesc->enmType)
    {
        case STAMTYPE_COUNTER:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && pDesc->u.pCounter->c == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<Counter c=\"%lld\"", pDesc->u.pCounter->c);
            break;

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && pDesc->u.pProfile->cPeriods == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<Profile cPeriods=\"%lld\" cTicks=\"%lld\" cTicksMin=\"%lld\" cTicksMax=\"%lld\"",
                                 pDesc->u.pProfile->cPeriods, pDesc->u.pProfile->cTicks, pDesc->u.pProfile->cTicksMin,
                                 pDesc->u.pProfile->cTicksMax);
            break;

        case STAMTYPE_RATIO_U32:
        case STAMTYPE_RATIO_U32_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && !pDesc->u.pRatioU32->u32A && !pDesc->u.pRatioU32->u32B)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<Ratio32 u32A=\"%lld\" u32B=\"%lld\"",
                                 pDesc->u.pRatioU32->u32A, pDesc->u.pRatioU32->u32B);
            break;

        case STAMTYPE_CALLBACK:
        {
            char szBuf[512];
            pDesc->u.Callback.pfnPrint(pThis->pVM, pDesc->u.Callback.pvSample, szBuf, sizeof(szBuf));
            stamR3SnapshotPrintf(pThis, "<Callback val=\"%s\"", szBuf);
            break;
        }

        case STAMTYPE_U8:
        case STAMTYPE_U8_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu8 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<U8 val=\"%u\"", *pDesc->u.pu8);
            break;

        case STAMTYPE_X8:
        case STAMTYPE_X8_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu8 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<X8 val=\"%#x\"", *pDesc->u.pu8);
            break;

        case STAMTYPE_U16:
        case STAMTYPE_U16_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu16 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<U16 val=\"%u\"", *pDesc->u.pu16);
            break;

        case STAMTYPE_X16:
        case STAMTYPE_X16_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu16 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<X16 val=\"%#x\"", *pDesc->u.pu16);
            break;

        case STAMTYPE_U32:
        case STAMTYPE_U32_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu32 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<U32 val=\"%u\"", *pDesc->u.pu32);
            break;

        case STAMTYPE_X32:
        case STAMTYPE_X32_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu32 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<X32 val=\"%#x\"", *pDesc->u.pu32);
            break;

        case STAMTYPE_U64:
        case STAMTYPE_U64_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu64 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<U64 val=\"%llu\"", *pDesc->u.pu64);
            break;

        case STAMTYPE_X64:
        case STAMTYPE_X64_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu64 == 0)
                return VINF_SUCCESS;
            stamR3SnapshotPrintf(pThis, "<X64 val=\"%#llx\"", *pDesc->u.pu64);
            break;

        default:
            AssertMsgFailed(("%d\n", pDesc->enmType));
            return 0;
    }

    stamR3SnapshotPrintf(pThis, " unit=\"%s\"", STAMR3GetUnit(pDesc->enmUnit));

    switch (pDesc->enmVisibility)
    {
        default:
        case STAMVISIBILITY_ALWAYS:
            break;
        case STAMVISIBILITY_USED:
            stamR3SnapshotPrintf(pThis, " vis=\"used\"");
            break;
        case STAMVISIBILITY_NOT_GUI:
            stamR3SnapshotPrintf(pThis, " vis=\"not-gui\"");
            break;
    }

    stamR3SnapshotPrintf(pThis, " name=\"%s\"", pDesc->pszName);

    if (pThis->fWithDesc && pDesc->pszDesc)
    {
        /*
         * The description is a bit tricky as it may include chars that
         * xml requires to be escaped.
         */
        const char *pszBadChar = strpbrk(pDesc->pszDesc, "&<>\"'");
        if (!pszBadChar)
            return stamR3SnapshotPrintf(pThis, " desc=\"%s\"/>\n", pDesc->pszDesc);

        stamR3SnapshotPrintf(pThis, " desc=\"");
        const char *pszCur = pDesc->pszDesc;
        do
        {
            stamR3SnapshotPrintf(pThis, "%.*s", pszBadChar - pszCur, pszCur);
            switch (*pszBadChar)
            {
                case '&':   stamR3SnapshotPrintf(pThis, "&amp;");   break;
                case '<':   stamR3SnapshotPrintf(pThis, "&lt;");    break;
                case '>':   stamR3SnapshotPrintf(pThis, "&gt;");    break;
                case '"':   stamR3SnapshotPrintf(pThis, "&quot;");  break;
                case '\'':  stamR3SnapshotPrintf(pThis, "&apos;");  break;
                default:    AssertMsgFailed(("%c", *pszBadChar));    break;
            }
            pszCur = pszBadChar + 1;
            pszBadChar = strpbrk(pszCur, "&<>\"'");
        } while (pszBadChar);
        return stamR3SnapshotPrintf(pThis, "%s\"/>\n", pszCur);
    }
    return stamR3SnapshotPrintf(pThis, "/>\n");
}


/**
 * Output callback for stamR3SnapshotPrintf.
 *
 * @returns number of bytes written.
 * @param   pvArg       The snapshot status structure.
 * @param   pach        Pointer to an array of characters (bytes).
 * @param   cch         The number or chars (bytes) to write from the array.
 */
static DECLCALLBACK(size_t) stamR3SnapshotOutput(void *pvArg, const char *pach, size_t cch)
{
    PSTAMR3SNAPSHOTONE pThis = (PSTAMR3SNAPSHOTONE)pvArg;

    /*
     * Make sure we've got space for it.
     */
    if (RT_UNLIKELY((uintptr_t)pThis->pszEnd - (uintptr_t)pThis->psz < cch + 1))
    {
        if (RT_FAILURE(pThis->rc))
            return 0;

        size_t cbNewSize = pThis->cbAllocated;
        if (cbNewSize > cch)
            cbNewSize *= 2;
        else
            cbNewSize += RT_ALIGN(cch + 1, 0x1000);
        char *pszNew = (char *)RTMemRealloc(pThis->pszStart, cbNewSize);
        if (!pszNew)
        {
            /*
             * Free up immediately, out-of-memory is bad news and this
             * isn't an important allocations / API.
             */
            pThis->rc = VERR_NO_MEMORY;
            RTMemFree(pThis->pszStart);
            pThis->pszStart = pThis->pszEnd = pThis->psz = NULL;
            pThis->cbAllocated = 0;
            return 0;
        }

        pThis->psz = pszNew + (pThis->psz - pThis->pszStart);
        pThis->pszStart = pszNew;
        pThis->pszEnd = pszNew + cbNewSize;
        pThis->cbAllocated = cbNewSize;
    }

    /*
     * Copy the chars to the buffer and terminate it.
     */
    memcpy(pThis->psz, pach, cch);
    pThis->psz += cch;
    *pThis->psz = '\0';
    return cch;
}


/**
 * Wrapper around RTStrFormatV for use by the snapshot API.
 *
 * @returns VBox status code.
 * @param   pThis       The snapshot status structure.
 * @param   pszFormat   The format string.
 * @param   ...         Optional arguments.
 */
static int stamR3SnapshotPrintf(PSTAMR3SNAPSHOTONE pThis, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTStrFormatV(stamR3SnapshotOutput, pThis, NULL, NULL, pszFormat, va);
    va_end(va);
    return pThis->rc;
}


/**
 * Releases a statistics snapshot returned by STAMR3Snapshot().
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszSnapshot     The snapshot data pointer returned by STAMR3Snapshot().
 *                          NULL is allowed.
 */
STAMR3DECL(int)  STAMR3SnapshotFree(PVM pVM, char *pszSnapshot)
{
    if (!pszSnapshot)
        RTMemFree(pszSnapshot);
    return VINF_SUCCESS;
}


/**
 * Dumps the selected statistics to the log.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszPat          The name matching pattern. See somewhere_where_this_is_described_in_detail.
 *                          If NULL all samples are written to the log.
 */
STAMR3DECL(int)  STAMR3Dump(PVM pVM, const char *pszPat)
{
    STAMR3PRINTONEARGS Args;
    Args.pVM = pVM;
    Args.pvArg = NULL;
    Args.pfnPrintf = stamR3EnumLogPrintf;

    STAM_LOCK_RD(pVM);
    stamR3Enum(pVM, pszPat, true /* fUpdateRing0 */, stamR3PrintOne, &Args);
    STAM_UNLOCK_RD(pVM);
    return VINF_SUCCESS;
}


/**
 * Prints to the log.
 *
 * @param   pArgs       Pointer to the print one argument structure.
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 */
static DECLCALLBACK(void) stamR3EnumLogPrintf(PSTAMR3PRINTONEARGS pArgs, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTLogPrintfV(pszFormat, va);
    va_end(va);
    NOREF(pArgs);
}


/**
 * Dumps the selected statistics to the release log.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszPat          The name matching pattern. See somewhere_where_this_is_described_in_detail.
 *                          If NULL all samples are written to the log.
 */
STAMR3DECL(int)  STAMR3DumpToReleaseLog(PVM pVM, const char *pszPat)
{
    STAMR3PRINTONEARGS Args;
    Args.pVM = pVM;
    Args.pvArg = NULL;
    Args.pfnPrintf = stamR3EnumRelLogPrintf;

    STAM_LOCK_RD(pVM);
    stamR3Enum(pVM, pszPat, true /* fUpdateRing0 */, stamR3PrintOne, &Args);
    STAM_UNLOCK_RD(pVM);
    return VINF_SUCCESS;
}


/**
 * Prints to the release log.
 *
 * @param   pArgs       Pointer to the print one argument structure.
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 */
static DECLCALLBACK(void) stamR3EnumRelLogPrintf(PSTAMR3PRINTONEARGS pArgs, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTLogRelPrintfV(pszFormat, va);
    va_end(va);
    NOREF(pArgs);
}


/**
 * Prints the selected statistics to standard out.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszPat          The name matching pattern. See somewhere_where_this_is_described_in_detail.
 *                          If NULL all samples are reset.
 */
STAMR3DECL(int)  STAMR3Print(PVM pVM, const char *pszPat)
{
    STAMR3PRINTONEARGS Args;
    Args.pVM = pVM;
    Args.pvArg = NULL;
    Args.pfnPrintf = stamR3EnumPrintf;

    STAM_LOCK_RD(pVM);
    stamR3Enum(pVM, pszPat, true /* fUpdateRing0 */, stamR3PrintOne, &Args);
    STAM_UNLOCK_RD(pVM);
    return VINF_SUCCESS;
}


/**
 * Prints to stdout.
 *
 * @param   pArgs       Pointer to the print one argument structure.
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 */
static DECLCALLBACK(void) stamR3EnumPrintf(PSTAMR3PRINTONEARGS pArgs, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTPrintfV(pszFormat, va);
    va_end(va);
    NOREF(pArgs);
}


/**
 * Prints one sample.
 * Callback for stamR3Enum().
 *
 * @returns VINF_SUCCESS
 * @param   pDesc   Pointer to the current descriptor.
 * @param   pvArg   User argument - STAMR3PRINTONEARGS.
 */
static int stamR3PrintOne(PSTAMDESC pDesc, void *pvArg)
{
    PSTAMR3PRINTONEARGS pArgs = (PSTAMR3PRINTONEARGS)pvArg;

    switch (pDesc->enmType)
    {
        case STAMTYPE_COUNTER:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && pDesc->u.pCounter->c == 0)
                return VINF_SUCCESS;

            pArgs->pfnPrintf(pArgs, "%-32s %8llu %s\n", pDesc->pszName, pDesc->u.pCounter->c, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_PROFILE:
        case STAMTYPE_PROFILE_ADV:
        {
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && pDesc->u.pProfile->cPeriods == 0)
                return VINF_SUCCESS;

            uint64_t u64 = pDesc->u.pProfile->cPeriods ? pDesc->u.pProfile->cPeriods : 1;
            pArgs->pfnPrintf(pArgs, "%-32s %8llu %s (%12llu ticks, %7llu times, max %9llu, min %7lld)\n", pDesc->pszName,
                             pDesc->u.pProfile->cTicks / u64, STAMR3GetUnit(pDesc->enmUnit),
                             pDesc->u.pProfile->cTicks, pDesc->u.pProfile->cPeriods, pDesc->u.pProfile->cTicksMax, pDesc->u.pProfile->cTicksMin);
            break;
        }

        case STAMTYPE_RATIO_U32:
        case STAMTYPE_RATIO_U32_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && !pDesc->u.pRatioU32->u32A && !pDesc->u.pRatioU32->u32B)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8u:%-8u %s\n", pDesc->pszName,
                             pDesc->u.pRatioU32->u32A, pDesc->u.pRatioU32->u32B, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_CALLBACK:
        {
            char szBuf[512];
            pDesc->u.Callback.pfnPrint(pArgs->pVM, pDesc->u.Callback.pvSample, szBuf, sizeof(szBuf));
            pArgs->pfnPrintf(pArgs, "%-32s %s %s\n", pDesc->pszName, szBuf, STAMR3GetUnit(pDesc->enmUnit));
            break;
        }

        case STAMTYPE_U8:
        case STAMTYPE_U8_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu8 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8u %s\n", pDesc->pszName, *pDesc->u.pu8, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_X8:
        case STAMTYPE_X8_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu8 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8x %s\n", pDesc->pszName, *pDesc->u.pu8, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_U16:
        case STAMTYPE_U16_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu16 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8u %s\n", pDesc->pszName, *pDesc->u.pu16, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_X16:
        case STAMTYPE_X16_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu16 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8x %s\n", pDesc->pszName, *pDesc->u.pu16, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_U32:
        case STAMTYPE_U32_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu32 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8u %s\n", pDesc->pszName, *pDesc->u.pu32, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_X32:
        case STAMTYPE_X32_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu32 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8x %s\n", pDesc->pszName, *pDesc->u.pu32, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_U64:
        case STAMTYPE_U64_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu64 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8llu %s\n", pDesc->pszName, *pDesc->u.pu64, STAMR3GetUnit(pDesc->enmUnit));
            break;

        case STAMTYPE_X64:
        case STAMTYPE_X64_RESET:
            if (pDesc->enmVisibility == STAMVISIBILITY_USED && *pDesc->u.pu64 == 0)
                return VINF_SUCCESS;
            pArgs->pfnPrintf(pArgs, "%-32s %8llx %s\n", pDesc->pszName, *pDesc->u.pu64, STAMR3GetUnit(pDesc->enmUnit));
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", pDesc->enmType));
            break;
    }
    NOREF(pvArg);
    return VINF_SUCCESS;
}


/**
 * Enumerate the statistics by the means of a callback function.
 *
 * @returns Whatever the callback returns.
 *
 * @param   pVM         The VM handle.
 * @param   pszPat      The pattern to match samples.
 * @param   pfnEnum     The callback function.
 * @param   pvUser      The pvUser argument of the callback function.
 */
STAMR3DECL(int) STAMR3Enum(PVM pVM, const char *pszPat, PFNSTAMR3ENUM pfnEnum, void *pvUser)
{
    STAMR3ENUMONEARGS Args;
    Args.pVM     = pVM;
    Args.pfnEnum = pfnEnum;
    Args.pvUser  = pvUser;

    STAM_LOCK_RD(pVM);
    int rc = stamR3Enum(pVM, pszPat, true /* fUpdateRing0 */, stamR3EnumOne, &Args);
    STAM_UNLOCK_RD(pVM);
    return rc;
}


/**
 * Callback function for STARTR3Enum().
 *
 * @returns whatever the callback returns.
 * @param   pDesc       Pointer to the current descriptor.
 * @param   pvArg       Points to a STAMR3ENUMONEARGS structure.
 */
static int stamR3EnumOne(PSTAMDESC pDesc, void *pvArg)
{
    PSTAMR3ENUMONEARGS pArgs = (PSTAMR3ENUMONEARGS)pvArg;
    int rc;
    if (pDesc->enmType == STAMTYPE_CALLBACK)
    {
        /* Give the enumerator something useful. */
        char szBuf[512];
        pDesc->u.Callback.pfnPrint(pArgs->pVM, pDesc->u.Callback.pvSample, szBuf, sizeof(szBuf));
        rc = pArgs->pfnEnum(pDesc->pszName, pDesc->enmType, szBuf, pDesc->enmUnit,
                            pDesc->enmVisibility, pDesc->pszDesc, pArgs->pvUser);
    }
    else
        rc = pArgs->pfnEnum(pDesc->pszName, pDesc->enmType, pDesc->u.pv, pDesc->enmUnit,
                            pDesc->enmVisibility, pDesc->pszDesc, pArgs->pvUser);
    return rc;
}


/**
 * Matches a sample name against a pattern.
 *
 * @returns True if matches, false if not.
 * @param   pszPat      Pattern.
 * @param   pszName     Name to match against the pattern.
 */
static bool stamR3Match(const char *pszPat, const char *pszName)
{
    /* ASSUMES ASCII */
    for (;;)
    {
        char chPat = *pszPat;
        switch (chPat)
        {
            default:
                if (*pszName != chPat)
                    return false;
                break;

            case '*':
            {
                while ((chPat = *++pszPat) == '*' || chPat == '?')
                    /* nothing */;

                for (;;)
                {
                    char ch = *pszName++;
                    if (    ch == chPat
                        &&  (   !chPat
                             || stamR3Match(pszPat + 1, pszName)))
                        return true;
                    if (!ch)
                        return false;
                }
                /* won't ever get here */
                break;
            }

            case '?':
                if (!*pszName)
                    return false;
                break;

            case '\0':
                return !*pszName;
        }
        pszName++;
        pszPat++;
    }
    return true;
}


/**
 * Match a name against an array of patterns.
 *
 * @returns true if it matches, false if it doesn't match.
 * @param   papszExpressions    The array of pattern expressions.
 * @param   cExpressions        The number of array entries.
 * @param   piExpression        Where to read/store the current skip index. Optional.
 * @param   pszName             The name to match.
 */
static bool stamR3MultiMatch(const char * const *papszExpressions, unsigned cExpressions,
                             unsigned *piExpression, const char *pszName)
{
    for (unsigned i = piExpression ? *piExpression : 0; i < cExpressions; i++)
    {
        const char *pszPat = papszExpressions[i];
        if (stamR3Match(pszPat, pszName))
        {
            /* later:
            if (piExpression && i > *piExpression)
            {
                check if we can skip some expressions
            }*/
            return true;
        }
    }
    return false;
}


/**
 * Splits a multi pattern into single ones.
 *
 * @returns Pointer to an array of single patterns. Free it with RTMemTmpFree.
 * @param   pszPat          The pattern to split.
 * @param   pcExpressions   The number of array elements.
 * @param   pszCopy         The pattern copy to free using RTStrFree.
 */
static char **stamR3SplitPattern(const char *pszPat, unsigned *pcExpressions, char **ppszCopy)
{
    Assert(pszPat && *pszPat);

    char *pszCopy = RTStrDup(pszPat);
    if (!pszCopy)
        return NULL;

    /* count them & allocate array. */
    char *psz = pszCopy;
    unsigned cExpressions = 1;
    while ((psz = strchr(psz, '|')) != NULL)
        cExpressions++, psz++;

    char **papszExpressions = (char **)RTMemTmpAllocZ((cExpressions + 1) * sizeof(char *));
    if (!papszExpressions)
    {
        RTStrFree(pszCopy);
        return NULL;
    }

    /* split */
    psz = pszCopy;
    for (unsigned i = 0;;)
    {
        papszExpressions[i] = psz;
        if (++i >= cExpressions)
            break;
        psz = strchr(psz, '|');
        *psz++ = '\0';
    }

    /* sort the array, putting '*' last. */
    /** @todo sort it... */

    *pcExpressions = cExpressions;
    *ppszCopy = pszCopy;
    return papszExpressions;
}


/**
 * Enumerates the nodes selected by a pattern or all nodes if no pattern
 * is specified.
 *
 * The call must own at least a read lock to the STAM data.
 *
 * @returns The rc from the callback.
 * @param   pVM             VM handle
 * @param   pszPat          Pattern.
 * @param   fUpdateRing0    Update the ring-0 .
 * @param   pfnCallback     Callback function which shall be called for matching nodes.
 *                          If it returns anything but VINF_SUCCESS the enumeration is
 *                          terminated and the status code returned to the caller.
 * @param   pvArg           User parameter for the callback.
 */
static int stamR3Enum(PVM pVM, const char *pszPat, bool fUpdateRing0, int (*pfnCallback)(PSTAMDESC pDesc, void *pvArg), void *pvArg)
{
    int rc = VINF_SUCCESS;

    /*
     * All
     */
    if (!pszPat || !*pszPat || !strcmp(pszPat, "*"))
    {
        if (fUpdateRing0)
            stamR3Ring0StatsUpdate(pVM, "*");

        PSTAMDESC   pCur = pVM->stam.s.pHead;
        while (pCur)
        {
            rc = pfnCallback(pCur, pvArg);
            if (rc)
                break;

            /* next */
            pCur = pCur->pNext;
        }
    }

    /*
     * Single expression pattern.
     */
    else if (!strchr(pszPat, '|'))
    {
        if (fUpdateRing0)
            stamR3Ring0StatsUpdate(pVM, pszPat);

        for (PSTAMDESC pCur = pVM->stam.s.pHead; pCur; pCur = pCur->pNext)
            if (stamR3Match(pszPat, pCur->pszName))
            {
                rc = pfnCallback(pCur, pvArg);
                if (rc)
                    break;
            }
    }

    /*
     * Multi expression pattern.
     */
    else
    {
        /*
         * Split up the pattern first.
         */
        char *pszCopy;
        unsigned cExpressions;
        char **papszExpressions = stamR3SplitPattern(pszPat, &cExpressions, &pszCopy);
        if (!papszExpressions)
            return VERR_NO_MEMORY;

        /*
         * Perform the enumeration.
         */
        if (fUpdateRing0)
            stamR3Ring0StatsUpdateMulti(pVM, papszExpressions, cExpressions);

        unsigned iExpression = 0;
        for (PSTAMDESC pCur = pVM->stam.s.pHead; pCur; pCur = pCur->pNext)
            if (stamR3MultiMatch(papszExpressions, cExpressions, &iExpression, pCur->pszName))
            {
                rc = pfnCallback(pCur, pvArg);
                if (rc)
                    break;
            }

        RTMemTmpFree(papszExpressions);
        RTStrFree(pszCopy);
    }

    return rc;
}


/**
 * Registers the ring-0 statistics.
 *
 * @param   pVM         Pointer to the shared VM structure.
 */
static void stamR3Ring0StatsRegister(PVM pVM)
{
    /* GVMM */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aGVMMStats); i++)
        stamR3Register(pVM, (uint8_t *)&pVM->stam.s.GVMMStats + g_aGVMMStats[i].offVar, NULL, NULL,
                       g_aGVMMStats[i].enmType, STAMVISIBILITY_ALWAYS, g_aGVMMStats[i].pszName,
                       g_aGVMMStats[i].enmUnit, g_aGVMMStats[i].pszDesc);
}


/**
 * Updates the ring-0 statistics (the copy).
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pszPat      The pattern.
 */
static void stamR3Ring0StatsUpdate(PVM pVM, const char *pszPat)
{
    stamR3Ring0StatsUpdateMulti(pVM, &pszPat, 1);
}


/**
 * Updates the ring-0 statistics.
 *
 * The ring-0 statistics aren't directly addressable from ring-3 and
 * must be copied when needed.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pszPat      The pattern (for knowing when to skip).
 */
static void stamR3Ring0StatsUpdateMulti(PVM pVM, const char * const *papszExpressions, unsigned cExpressions)
{
    if (!pVM->pSession)
        return;

    /* GVMM */
    bool fUpdate = false;
    for (unsigned i = 0; i < RT_ELEMENTS(g_aGVMMStats); i++)
        if (stamR3MultiMatch(papszExpressions, cExpressions, NULL, g_aGVMMStats[i].pszName))
        {
            fUpdate = true;
            break;
        }
    if (fUpdate)
    {
        GVMMQUERYSTATISTICSSREQ Req;
        Req.Hdr.cbReq = sizeof(Req);
        Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        Req.pSession = pVM->pSession;
        int rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GVMM_QUERY_STATISTICS, 0, &Req.Hdr);
        if (RT_SUCCESS(rc))
            pVM->stam.s.GVMMStats = Req.Stats;
    }
}


/**
 * Get the unit string.
 *
 * @returns Pointer to read only unit string.
 * @param   enmUnit     The unit.
 */
STAMR3DECL(const char *) STAMR3GetUnit(STAMUNIT enmUnit)
{
    switch (enmUnit)
    {
        case STAMUNIT_NONE:                 return "";
        case STAMUNIT_CALLS:                return "calls";
        case STAMUNIT_COUNT:                return "count";
        case STAMUNIT_BYTES:                return "bytes";
        case STAMUNIT_PAGES:                return "pages";
        case STAMUNIT_ERRORS:               return "errors";
        case STAMUNIT_OCCURENCES:           return "times";
        case STAMUNIT_TICKS_PER_CALL:       return "ticks/call";
        case STAMUNIT_TICKS_PER_OCCURENCE:  return "ticks/time";
        case STAMUNIT_GOOD_BAD:             return "good:bad";
        case STAMUNIT_MEGABYTES:            return "megabytes";
        case STAMUNIT_KILOBYTES:            return "kilobytes";
        case STAMUNIT_NS:                   return "ns";
        case STAMUNIT_NS_PER_CALL:          return "ns/call";
        case STAMUNIT_NS_PER_OCCURENCE:     return "ns/time";
        case STAMUNIT_PCT:                  return "%";

        default:
            AssertMsgFailed(("Unknown unit %d\n", enmUnit));
            return "(?unit?)";
    }
}


#ifdef VBOX_WITH_DEBUGGER
/**
 * The '.stats' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) stamR3CmdStats(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");
    if (!pVM->stam.s.pHead)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Sorry, no statistics present.\n");

    /*
     * Do the printing.
     */
    STAMR3PRINTONEARGS Args;
    Args.pVM = pVM;
    Args.pvArg = pCmdHlp;
    Args.pfnPrintf = stamR3EnumDbgfPrintf;

    STAM_LOCK_RD(pVM);
    int rc = stamR3Enum(pVM, cArgs ? paArgs[0].u.pszString : NULL, true /* fUpdateRing0 */, stamR3PrintOne, &Args);
    STAM_UNLOCK_RD(pVM);

    return rc;
}


/**
 * Display one sample in the debugger.
 *
 * @param   pArgs       Pointer to the print one argument structure.
 * @param   pszFormat   Format string.
 * @param   ...         Format arguments.
 */
static DECLCALLBACK(void) stamR3EnumDbgfPrintf(PSTAMR3PRINTONEARGS pArgs, const char *pszFormat, ...)
{
    PDBGCCMDHLP pCmdHlp = (PDBGCCMDHLP)pArgs->pvArg;

    va_list va;
    va_start(va, pszFormat);
    pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszFormat, va);
    va_end(va);
    NOREF(pArgs);
}


/**
 * The '.statsreset' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) stamR3CmdStatsReset(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");
    if (!pVM->stam.s.pHead)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Sorry, no statistics present.\n");

    /*
     * Execute reset.
     */
    int rc = STAMR3Reset(pVM, cArgs ? paArgs[0].u.pszString : NULL);
    if (VBOX_SUCCESS(rc))
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "info: Statistics reset.\n");

    return pCmdHlp->pfnVBoxError(pCmdHlp, rc, "Restting statistics.\n");
}
#endif

