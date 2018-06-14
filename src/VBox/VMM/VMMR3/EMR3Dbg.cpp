/* $Id$ */
/** @file
 * EM - Execution Monitor / Manager, Debugger Related Bits.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/vmm/em.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/nem.h>
#include <VBox/dbg.h>
#include "EMInternal.h"
#include <VBox/vmm/vm.h>
#include <iprt/string.h>
#include <iprt/ctype.h>


/** @callback_method_impl{FNDBGCCMD,
 * Implements the '.alliem' command. }
 */
static DECLCALLBACK(int) enmR3DbgCmdAllIem(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PUVM pUVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    int  rc;
    bool f;

    if (cArgs == 0)
    {
        rc = EMR3QueryExecutionPolicy(pUVM, EMEXECPOLICY_IEM_ALL, &f);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "EMR3QueryExecutionPolicy(,EMEXECPOLICY_IEM_ALL,");
        DBGCCmdHlpPrintf(pCmdHlp, f ? "alliem: enabled\n" : "alliem: disabled\n");
    }
    else
    {
        rc = DBGCCmdHlpVarToBool(pCmdHlp, &paArgs[0], &f);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "DBGCCmdHlpVarToBool");
        rc = EMR3SetExecutionPolicy(pUVM, EMEXECPOLICY_IEM_ALL, f);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "EMR3SetExecutionPolicy(,EMEXECPOLICY_IEM_ALL,%RTbool)", f);
    }
    return VINF_SUCCESS;
}


/** Describes a optional boolean argument. */
static DBGCVARDESC const g_BoolArg = { 0, 1, DBGCVAR_CAT_ANY, 0, "boolean", "Boolean value." };

/** Commands.  */
static DBGCCMD const g_aCmds[] =
{
    {
        "alliem", 0, 1, &g_BoolArg, 1, 0, enmR3DbgCmdAllIem, "[boolean]",
        "Enables or disabled executing ALL code in IEM, if no arguments are given it displays the current status."
    },
};

VMM_INT_DECL(const char *) EMR3GetExitTypeName(uint32_t uExitType)
{
    switch ((EMEXITTYPE)uExitType)
    {
        case EMEXITTYPE_INVALID:            return "invalid";
        case EMEXITTYPE_IO_PORT_READ:       return "I/O port read";
        case EMEXITTYPE_IO_PORT_WRITE:      return "I/O port write";
        case EMEXITTYPE_IO_PORT_STR_READ:   return "I/O port string read";
        case EMEXITTYPE_IO_PORT_STR_WRITE:  return "I/O port string write";
        case EMEXITTYPE_MMIO_READ:          return "MMIO read";
        case EMEXITTYPE_MMIO_WRITE:         return "MMIO write";
        case EMEXITTYPE_MSR_READ:           return "MSR read";
        case EMEXITTYPE_MSR_WRITE:          return "MSR write";
        case EMEXITTYPE_CPUID:              return "CPUID";
    }
    return NULL;
}

/**
 * Translates flags+type into an exit name.
 *
 * @returns Exit name.
 * @param   uFlagsAndType   The exit to name.
 * @param   pszFallback     Buffer for formatting a numeric fallback.
 * @param   cbFallback      Size of fallback buffer.
 */
static const char *emR3HistoryGetExitName(uint32_t uFlagsAndType, char *pszFallback, size_t cbFallback)
{
    const char *pszExitName;
    switch (uFlagsAndType & EMEXIT_F_KIND_MASK)
    {
        case EMEXIT_F_KIND_EM:
            pszExitName = EMR3GetExitTypeName(uFlagsAndType & EMEXIT_F_TYPE_MASK);
            break;

        case EMEXIT_F_KIND_VMX:
            pszExitName = HMR3GetVmxExitName( uFlagsAndType & EMEXIT_F_TYPE_MASK);
            break;

        case EMEXIT_F_KIND_SVM:
            pszExitName = HMR3GetSvmExitName( uFlagsAndType & EMEXIT_F_TYPE_MASK);
            break;

        case EMEXIT_F_KIND_NEM:
            pszExitName = NEMR3GetExitName(   uFlagsAndType & EMEXIT_F_TYPE_MASK);
            break;

        case EMEXIT_F_KIND_XCPT:
            switch (uFlagsAndType & EMEXIT_F_TYPE_MASK)
            {
                case X86_XCPT_DE:               return "Xcpt #DE";
                case X86_XCPT_DB:               return "Xcpt #DB";
                case X86_XCPT_NMI:              return "Xcpt #NMI";
                case X86_XCPT_BP:               return "Xcpt #BP";
                case X86_XCPT_OF:               return "Xcpt #OF";
                case X86_XCPT_BR:               return "Xcpt #BR";
                case X86_XCPT_UD:               return "Xcpt #UD";
                case X86_XCPT_NM:               return "Xcpt #NM";
                case X86_XCPT_DF:               return "Xcpt #DF";
                case X86_XCPT_CO_SEG_OVERRUN:   return "Xcpt #CO_SEG_OVERRUN";
                case X86_XCPT_TS:               return "Xcpt #TS";
                case X86_XCPT_NP:               return "Xcpt #NP";
                case X86_XCPT_SS:               return "Xcpt #SS";
                case X86_XCPT_GP:               return "Xcpt #GP";
                case X86_XCPT_PF:               return "Xcpt #PF";
                case X86_XCPT_MF:               return "Xcpt #MF";
                case X86_XCPT_AC:               return "Xcpt #AC";
                case X86_XCPT_MC:               return "Xcpt #MC";
                case X86_XCPT_XF:               return "Xcpt #XF";
                case X86_XCPT_VE:               return "Xcpt #VE";
                case X86_XCPT_SX:               return "Xcpt #SX";
                default:
                    pszExitName = NULL;
                    break;
            }
            break;

        default:
            AssertFailed();
            pszExitName = NULL;
            break;
    }
    if (pszExitName)
        return pszExitName;
    RTStrPrintf(pszFallback, cbFallback, "%#06x", uFlagsAndType & (EMEXIT_F_KIND_MASK | EMEXIT_F_TYPE_MASK));
    return pszFallback;
}


/**
 * Displays the VM-exit history.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) emR3InfoExitHistory(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);

    /*
     * Figure out target cpu and parse arguments.
     */
    PVMCPU   pVCpu    = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];
    bool     fReverse = true;
    uint32_t cLeft    = RT_ELEMENTS(pVCpu->em.s.aExitHistory);

    while (pszArgs && *pszArgs)
    {
        pszArgs = RTStrStripL(pszArgs);
        if (!*pszArgs)
            break;
        if (RT_C_IS_DIGIT(*pszArgs))
        {
            /* The number to dump. */
            uint32_t uValue = cLeft;
            RTStrToUInt32Ex(pszArgs, (char **)&pszArgs, 0, &uValue);
            if (uValue > 0)
                cLeft = RT_MIN(uValue, RT_ELEMENTS(pVCpu->em.s.aExitHistory));
        }
        else if (RTStrCmp(pszArgs, "reverse") == 0)
        {
            pszArgs += 7;
            fReverse = true;
        }
        else if (RTStrCmp(pszArgs, "ascending") == 0)
        {
            pszArgs += 9;
            fReverse = false;
        }
        else if (RTStrCmp(pszArgs, "asc") == 0)
        {
            pszArgs += 3;
            fReverse = false;
        }
        else
        {
            const char *pszStart = pszArgs;
            while (*pszArgs && !RT_C_IS_SPACE(*pszArgs))
                pszArgs++;
            pHlp->pfnPrintf(pHlp, "Unknown option: %.*s\n", pszArgs - pszStart, pszArgs);
        }
    }

    /*
     * Do the job.
     */
    uint64_t idx = pVCpu->em.s.iNextExit;
    if (idx == 0)
        pHlp->pfnPrintf(pHlp, "CPU[%u]: VM-exit history: empty\n", pVCpu->idCpu);
    else
    {
        /*
         * Print header.
         */
        pHlp->pfnPrintf(pHlp, "CPU[%u]: VM-exit history:\n", pVCpu->idCpu);
        pHlp->pfnPrintf(pHlp, "   Exit No.:      TSC timestamp / delta    Flat RIP         Exit   Name\n");

        /*
         * Adjust bounds if ascending order.
         */
        if (!fReverse)
        {
            if (idx > cLeft)
                idx -= cLeft;
            else
            {
                cLeft = idx;
                idx = 0;
            }
        }

        /*
         * Print the entries.
         */
        uint64_t uPrevTimestamp = 0;
        do
        {
            if (fReverse)
                idx -= 1;
            PCEMEXITENTRY const pEntry = &pVCpu->em.s.aExitHistory[(uintptr_t)idx & 0xff];
            if (!fReverse)
                idx += 1;

            /* Get the exit name. */
            char        szExitName[16];
            const char *pszExitName = emR3HistoryGetExitName(pEntry->uFlagsAndType, szExitName, sizeof(szExitName));

            int64_t offDelta = uPrevTimestamp != 0 && pEntry->uTimestamp != 0 ? pEntry->uTimestamp - uPrevTimestamp : 0;
            uPrevTimestamp = pEntry->uTimestamp;

            if (pEntry->idxSlot == UINT32_MAX)
                pHlp->pfnPrintf(pHlp, " %10RU64: %#018RX64/%+-9RI64 %016RX64 %#06x %s\n",
                                idx, pEntry->uTimestamp, offDelta, pEntry->uFlatPC, pEntry->uFlagsAndType, pszExitName);
            else
            {
                /** @todo more on this later */
                pHlp->pfnPrintf(pHlp, " %10RU64: %#018RX64/%+-9RI64 %016RX64 %#06x %s slot=%#x\n",
                                idx, pEntry->uTimestamp, offDelta, pEntry->uFlatPC, pEntry->uFlagsAndType, pszExitName,
                                pEntry->idxSlot);
            }
        } while (--cLeft > 0 && idx > 0);
    }
}


int emR3InitDbg(PVM pVM)
{
    /*
     * Register info dumpers.
     */
    int rc = DBGFR3InfoRegisterInternalEx(pVM, "exits", "Dumps the VM-exit history.",
                                          emR3InfoExitHistory, DBGFINFO_FLAGS_ALL_EMTS);
    AssertLogRelRCReturn(rc, rc);

#ifdef VBOX_WITH_DEBUGGER
    /*
     * Register debugger commands.
     */
    rc = DBGCRegisterCommands(&g_aCmds[0], RT_ELEMENTS(g_aCmds));
    AssertLogRelRCReturn(rc, rc);
#endif

    return VINF_SUCCESS;
}

