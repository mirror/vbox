/* $Id$ */
/** @file
 * CPUM - CPU Monitor / Manager (ARMv8 variant).
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/** @page pg_cpum CPUM - CPU Monitor / Manager
 *
 * The CPU Monitor / Manager keeps track of all the CPU registers.
 * This is the ARMv8 variant which is doing much less than its x86/amd64
 * counterpart due to the fact that we currently only support the NEM backends
 * for running ARM guests. It might become complex iff we decide to implement our
 * own hypervisor.
 *
 * @section sec_cpum_logging        Logging Level Assignments.
 *
 * Following log level assignments:
 *      - @todo
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#define CPUM_WITH_NONCONST_HOST_FEATURES
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/cpumdis.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/ssm.h>
#include "CPUMInternal-armv8.h"
#include <VBox/vmm/vm.h>

#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/cpuset.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * What kind of cpu info dump to perform.
 */
typedef enum CPUMDUMPTYPE
{
    CPUMDUMPTYPE_TERSE,
    CPUMDUMPTYPE_DEFAULT,
    CPUMDUMPTYPE_VERBOSE
} CPUMDUMPTYPE;
/** Pointer to a cpu info dump type. */
typedef CPUMDUMPTYPE *PCPUMDUMPTYPE;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int)  cpumR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass);
static DECLCALLBACK(int)  cpumR3SaveExec(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  cpumR3LoadPrep(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  cpumR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static DECLCALLBACK(int)  cpumR3LoadDone(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuestInstr(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#if 0 /** @todo Will come later. */
/** Saved state field descriptors for CPUMCTX. */
static const SSMFIELD g_aCpumCtxFields[] =
{
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[0].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[1].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[2].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[3].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[4].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[5].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[6].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[7].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[8].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[9].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[10].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[11].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[12].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[13].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[14].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[15].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[16].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[17].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[18].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[19].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[20].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[21].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[22].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[23].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[24].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[25].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[26].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[27].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[28].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[29].x),
    SSMFIELD_ENTRY(         CPUMCTX, aGRegs[30].x),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[0].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[1].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[2].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[3].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[4].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[5].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[6].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[7].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[8].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[9].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[10].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[11].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[12].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[13].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[14].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[15].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[16].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[17].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[18].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[19].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[20].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[21].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[22].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[23].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[24].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[25].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[26].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[27].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[28].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[29].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[30].v),
    SSMFIELD_ENTRY(         CPUMCTX, aVRegs[31].v),
    SSMFIELD_ENTRY(         CPUMCTX, aSpReg[0].u64),
    SSMFIELD_ENTRY(         CPUMCTX, aSpReg[1].u64),
    SSMFIELD_ENTRY(         CPUMCTX, Pc.u64),
    SSMFIELD_ENTRY(         CPUMCTX, Spsr.u64),
    SSMFIELD_ENTRY(         CPUMCTX, Elr.u64),
    SSMFIELD_ENTRY(         CPUMCTX, fpcr),
    SSMFIELD_ENTRY(         CPUMCTX, fpsr),
    SSMFIELD_ENTRY(         CPUMCTX, fPState),
    /** @todo */
    SSMFIELD_ENTRY_TERM()
};
#endif


/**
 * Initializes the debug aids of CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
static int cpumR3DbgInit(PVM pVM)
{
    RT_NOREF(pVM);
    /** @todo */
    return VINF_SUCCESS;
}


/**
 * Initializes the CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(int) CPUMR3Init(PVM pVM)
{
    LogFlow(("CPUMR3Init\n"));

    /*
     * Assert alignment, sizes and tables.
     */
    AssertCompileMemberAlignment(VM, cpum.s, 32);
    AssertCompile(sizeof(pVM->cpum.s) <= sizeof(pVM->cpum.padding));
    AssertCompileSizeAlignment(CPUMCTX, 64);
    AssertCompileMemberAlignment(VM, cpum, 64);
    AssertCompileMemberAlignment(VMCPU, cpum.s, 64);

    /*
     * Register saved state data item.
     */
    int rc = SSMR3RegisterInternal(pVM, "cpum", 1, CPUM_SAVED_STATE_VERSION, sizeof(CPUM),
                                   NULL, cpumR3LiveExec, NULL,
                                   NULL, cpumR3SaveExec, NULL,
                                   cpumR3LoadPrep, cpumR3LoadExec, cpumR3LoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register info handlers and registers with the debugger facility.
     */
    DBGFR3InfoRegisterInternalEx(pVM, "cpum",             "Displays the all the cpu states.",
                                 &cpumR3InfoAll, DBGFINFO_FLAGS_ALL_EMTS);
    DBGFR3InfoRegisterInternalEx(pVM, "cpumguest",        "Displays the guest cpu state.",
                                 &cpumR3InfoGuest, DBGFINFO_FLAGS_ALL_EMTS);

    rc = cpumR3DbgInit(pVM);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the general guest CPU state.
     */
    CPUMR3Reset(pVM);

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The CPUM will update the addresses used by the switcher.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3DECL(void) CPUMR3Relocate(PVM pVM)
{
    RT_NOREF(pVM);
}


/**
 * Terminates the CPUM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(int) CPUMR3Term(PVM pVM)
{
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * Resets a virtual CPU.
 *
 * Used by CPUMR3Reset and CPU hot plugging.
 *
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the CPU that is
 *                  being reset.  This may differ from the current EMT.
 */
VMMR3DECL(void) CPUMR3ResetCpu(PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF(pVM);

    /** @todo anything different for VCPU > 0? */
    PCPUMCTX pCtx = &pVCpu->cpum.s.Guest;

    /*
     * Initialize everything to ZERO first.
     */
    RT_BZERO(pCtx, sizeof(*pCtx));
    /** @todo */
}


/**
 * Resets the CPU.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(void) CPUMR3Reset(PVM pVM)
{
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];
        CPUMR3ResetCpu(pVM, pVCpu);
    }
}




/**
 * Pass 0 live exec callback.
 *
 * @returns VINF_SSM_DONT_CALL_AGAIN.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The saved state handle.
 * @param   uPass               The pass (0).
 */
static DECLCALLBACK(int) cpumR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass)
{
    AssertReturn(uPass == 0, VERR_SSM_UNEXPECTED_PASS);
    /** @todo */ RT_NOREF(pVM, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) cpumR3SaveExec(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save.
     */
    SSMR3PutU32(pSSM, pVM->cCpus);
    /** @todo */
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMINTLOADPREP}
 */
static DECLCALLBACK(int) cpumR3LoadPrep(PVM pVM, PSSMHANDLE pSSM)
{
    RT_NOREF(pSSM);
    pVM->cpum.s.fPendingRestore = true;
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMINTLOADEXEC}
 */
static DECLCALLBACK(int) cpumR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /*
     * Validate version.
     */
    /** @todo */ RT_NOREF(pSSM, uVersion);

    if (uPass == SSM_PASS_FINAL)
    {
        /** @todo */
    }

    pVM->cpum.s.fPendingRestore = false;
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMINTLOADDONE}
 */
static DECLCALLBACK(int) cpumR3LoadDone(PVM pVM, PSSMHANDLE pSSM)
{
    if (RT_FAILURE(SSMR3HandleGetStatus(pSSM)))
        return VINF_SUCCESS;

    /* just check this since we can. */ /** @todo Add a SSM unit flag for indicating that it's mandatory during a restore.  */
    if (pVM->cpum.s.fPendingRestore)
    {
        LogRel(("CPUM: Missing state!\n"));
        return VERR_INTERNAL_ERROR_2;
    }

    /** @todo */
    return VINF_SUCCESS;
}


/**
 * Checks if the CPUM state restore is still pending.
 *
 * @returns true / false.
 * @param   pVM                 The cross context VM structure.
 */
VMMDECL(bool) CPUMR3IsStateRestorePending(PVM pVM)
{
    return pVM->cpum.s.fPendingRestore;
}


/**
 * Formats the PSTATE value into mnemonics.
 *
 * @param   pszPState   Where to write the mnemonics. (Assumes sufficient buffer space.)
 * @param   fPState     The PSTATE value with both guest hardware and VBox
 *                      internal bits included.
 */
static void cpumR3InfoFormatPState(char *pszPState, uint32_t fPState)
{
    /*
     * Format the flags.
     */
    static const struct
    {
        const char *pszSet; const char *pszClear; uint32_t fFlag;
    }   s_aFlags[] =
    {
        { NULL, NULL, 0 }, /** @todo */
    };
    char *psz = pszPState;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFlags); i++)
    {
        const char *pszAdd = s_aFlags[i].fFlag & fPState ? s_aFlags[i].pszSet : s_aFlags[i].pszClear;
        if (pszAdd)
        {
            strcpy(psz, pszAdd);
            psz += strlen(pszAdd);
            *psz++ = ' ';
        }
    }
    psz[-1] = '\0';
}


/**
 * Formats a full register dump.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pCtx        The context to format.
 * @param   pHlp        Output functions.
 * @param   enmType     The dump type.
 * @param   pszPrefix   Register name prefix.
 */
static void cpumR3InfoOne(PVM pVM, PCPUMCTX pCtx, PCDBGFINFOHLP pHlp, CPUMDUMPTYPE enmType, const char *pszPrefix)
{
    RT_NOREF(pVM, pHlp, enmType, pszPrefix);

    /*
     * Format the PSTATE.
     */
    char szPState[80];
    cpumR3InfoFormatPState(&szPState[0], pCtx->fPState);

    /** @todo */
}


/**
 * Display all cpu states and any other cpum info.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    cpumR3InfoGuest(pVM, pHlp, pszArgs);
    cpumR3InfoGuestInstr(pVM, pHlp, pszArgs);
}


/**
 * Parses the info argument.
 *
 * The argument starts with 'verbose', 'terse' or 'default' and then
 * continues with the comment string.
 *
 * @param   pszArgs         The pointer to the argument string.
 * @param   penmType        Where to store the dump type request.
 * @param   ppszComment     Where to store the pointer to the comment string.
 */
static void cpumR3InfoParseArg(const char *pszArgs, CPUMDUMPTYPE *penmType, const char **ppszComment)
{
    if (!pszArgs)
    {
        *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = "";
    }
    else
    {
        if (!strncmp(pszArgs, RT_STR_TUPLE("verbose")))
        {
            pszArgs += 7;
            *penmType = CPUMDUMPTYPE_VERBOSE;
        }
        else if (!strncmp(pszArgs, RT_STR_TUPLE("terse")))
        {
            pszArgs += 5;
            *penmType = CPUMDUMPTYPE_TERSE;
        }
        else if (!strncmp(pszArgs, RT_STR_TUPLE("default")))
        {
            pszArgs += 7;
            *penmType = CPUMDUMPTYPE_DEFAULT;
        }
        else
            *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = RTStrStripL(pszArgs);
    }
}


/**
 * Display the guest cpu state.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments.
 */
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);

    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = pVM->apCpusR3[0];

    pHlp->pfnPrintf(pHlp, "Guest CPUM (VCPU %d) state: %s\n", pVCpu->idCpu, pszComment);

    PCPUMCTX pCtx = &pVCpu->cpum.s.Guest;
    cpumR3InfoOne(pVM, pCtx, pHlp, enmType, "");
}


/**
 * Display the current guest instruction
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoGuestInstr(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);

    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = pVM->apCpusR3[0];

    char szInstruction[256];
    szInstruction[0] = '\0';
    DBGFR3DisasInstrCurrent(pVCpu, szInstruction, sizeof(szInstruction));
    pHlp->pfnPrintf(pHlp, "\nCPUM%u: %s\n\n", pVCpu->idCpu, szInstruction);
}


/**
 * Called when the ring-3 init phase completes.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   enmWhat             Which init phase.
 */
VMMR3DECL(int) CPUMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    RT_NOREF(pVM, enmWhat);
    return VINF_SUCCESS;
}


/**
 * Called when the ring-0 init phases completed.
 *
 * @param   pVM                 The cross context VM structure.
 */
VMMR3DECL(void) CPUMR3LogCpuIdAndMsrFeatures(PVM pVM)
{
    /*
     * Enable log buffering as we're going to log a lot of lines.
     */
    bool const fOldBuffered = RTLogRelSetBuffering(true /*fBuffered*/);

    /*
     * Log the cpuid.
     */
    RTCPUSET OnlineSet;
    LogRel(("CPUM: Logical host processors: %u present, %u max, %u online, online mask: %016RX64\n",
                (unsigned)RTMpGetPresentCount(), (unsigned)RTMpGetCount(), (unsigned)RTMpGetOnlineCount(),
                RTCpuSetToU64(RTMpGetOnlineSet(&OnlineSet)) ));
    RTCPUID cCores = RTMpGetCoreCount();
    if (cCores)
        LogRel(("CPUM: Physical host cores: %u\n", (unsigned)cCores));
    RT_NOREF(pVM);
#if 0 /** @todo Someting similar. */
    LogRel(("************************* CPUID dump ************************\n"));
    DBGFR3Info(pVM->pUVM, "cpuid", "verbose", DBGFR3InfoLogRelHlp());
    LogRel(("\n"));
    DBGFR3_INFO_LOG_SAFE(pVM, "cpuid", "verbose"); /* macro */
    LogRel(("******************** End of CPUID dump **********************\n"));
#endif

    /*
     * Restore the log buffering state to what it was previously.
     */
    RTLogRelSetBuffering(fOldBuffered);
}


/**
 * Marks the guest debug state as active.
 *
 * @returns nothing.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @note This is used solely by NEM (hence the name) to set the correct flags here
 *       without loading the host's DRx registers, which is not possible from ring-3 anyway.
 *       The specific NEM backends have to make sure to load the correct values.
 */
VMMR3_INT_DECL(void) CPUMR3NemActivateGuestDebugState(PVMCPUCC pVCpu)
{
    ASMAtomicAndU32(&pVCpu->cpum.s.fUseFlags, ~CPUM_USED_DEBUG_REGS_HYPER);
    ASMAtomicOrU32(&pVCpu->cpum.s.fUseFlags, CPUM_USED_DEBUG_REGS_GUEST);
}


/**
 * Marks the hyper debug state as active.
 *
 * @returns nothing.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @note This is used solely by NEM (hence the name) to set the correct flags here
 *       without loading the host's debug registers, which is not possible from ring-3 anyway.
 *       The specific NEM backends have to make sure to load the correct values.
 */
VMMR3_INT_DECL(void) CPUMR3NemActivateHyperDebugState(PVMCPUCC pVCpu)
{
    ASMAtomicAndU32(&pVCpu->cpum.s.fUseFlags, ~CPUM_USED_DEBUG_REGS_GUEST);
    ASMAtomicOrU32(&pVCpu->cpum.s.fUseFlags, CPUM_USED_DEBUG_REGS_HYPER);
}
