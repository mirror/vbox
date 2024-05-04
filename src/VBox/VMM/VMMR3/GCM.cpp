/** @file
 * GCM - Guest Compatibility Manager.
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
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

/** @page pg_gcm        GCM - The Guest Compatibility Manager
 *
 * The Guest Compatibility Manager provides run-time compatibility fixes for
 * certain known guest bugs.
 *
 * @see grp_gcm
 *
 *
 * @section sec_gcm_fixer   Fixers
 *
 * A GCM fixer implements a collection of run-time helpers/patches suitable for
 * a specific guest type. Several fixers can be active at the same time; for
 * example OS/2 or Windows 9x need their own fixers, but can also runs DOS
 * applications which need DOS-specific fixers.
 *
 * The concept of fixers exists to reduce the number of false positives to a
 * minimum. Heuristics are used to decide whether a particular fix should be
 * applied or not; restricting the number of applicable fixes minimizes the
 * chance that a fix could be misapplied.
 *
 * The fixers are invisible to a guest. It is not expected that the set of
 * active fixers would be changed during the lifetime of the VM.
 *
 *
 * @subsection sec_gcm_fixer_div_by_zero  Division By Zero
 *
 * A common problem is division by zero caused by a software timing loop which
 * cannot deal with fast CPUs (where "fast" very much depends on the era when
 * the software was written). A fixer intercepts division by zero, recognizes
 * known register contents and code sequence, modifies one or more registers to
 * avoid a divide error, and restarts the instruction.
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GCM
#include <VBox/vmm/gcm.h>
#include <VBox/vmm/ssm.h>
#include "GCMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/log.h>
#include <VBox/err.h>

#include <iprt/string.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static FNSSMINTSAVEEXEC  gcmR3Save;
static FNSSMINTLOADEXEC  gcmR3Load;


/**
 * Initializes the GCM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) GCMR3Init(PVM pVM)
{
    LogFlow(("GCMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompile(sizeof(pVM->gcm.s) <= sizeof(pVM->gcm.padding));

    /*
     * Register the saved state data unit.
     */
    int rc = SSMR3RegisterInternal(pVM, "GCM", 0 /* uInstance */, GCM_SAVED_STATE_VERSION, sizeof(GCM),
                                   NULL /* pfnLivePrep */, NULL /* pfnLiveExec */, NULL /* pfnLiveVote*/,
                                   NULL /* pfnSavePrep */, gcmR3Save,              NULL /* pfnSaveDone */,
                                   NULL /* pfnLoadPrep */, gcmR3Load,              NULL /* pfnLoadDone */);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Read & validate configuration.
     */
    static struct { const char *pszName; uint32_t cchName; uint32_t fFlag; } const s_aFixerIds[] =
    {
        { RT_STR_TUPLE("DivByZeroDOS"),       GCMFIXER_DBZ_DOS },
        { RT_STR_TUPLE("DivByZeroOS2"),       GCMFIXER_DBZ_OS2 },
        { RT_STR_TUPLE("DivByZeroWin9x"),     GCMFIXER_DBZ_WIN9X },
        { RT_STR_TUPLE("MesaVmsvgaDrv"),      GCMFIXER_MESA_VMSVGA_DRV },
    };

    /* Assemble valid value names for CFMGR3ValidateConfig. */
    char   szValidValues[1024];
    size_t offValidValues = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFixerIds); i++)
    {
        AssertReturn(offValidValues + s_aFixerIds[i].cchName + 2 <= sizeof(szValidValues), VERR_INTERNAL_ERROR_2);
        if (offValidValues)
            szValidValues[offValidValues++] = '|';
        memcpy(&szValidValues[offValidValues], s_aFixerIds[i].pszName, s_aFixerIds[i].cchName);
        offValidValues += s_aFixerIds[i].cchName;
    }
    szValidValues[offValidValues] = '\0';

    /* Validate the configuration. */
    PCFGMNODE pCfgNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "GCM/");
    rc = CFGMR3ValidateConfig(pCfgNode,
                              "/GCM/",              /* pszNode (for error msgs) */
                              szValidValues,
                              "",                   /* pszValidNodes */
                              "GCM",                /* pszWho */
                              0);                   /* uInstance */
    if (RT_FAILURE(rc))
        return rc;

    /* Read the configuration. */
    pVM->gcm.s.fFixerSet = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFixerIds); i++)
    {
        bool fEnabled = false;
        rc = CFGMR3QueryBoolDef(pCfgNode, s_aFixerIds[i].pszName, &fEnabled, false);
        if (RT_FAILURE(rc))
            return VMR3SetError(pVM->pUVM, rc, RT_SRC_POS, "Error reading /GCM/%s as boolean: %Rrc", s_aFixerIds[i].pszName, rc);
        if (fEnabled)
            pVM->gcm.s.fFixerSet = s_aFixerIds[i].fFlag;
    }

#if 0 /* development override */
    pVM->gcm.s.fFixerSet = GCMFIXER_DBZ_OS2 | GCMFIXER_DBZ_DOS | GCMFIXER_DBZ_WIN9X;
#endif

    /*
     * Log what's enabled.
     */
    offValidValues = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFixerIds); i++)
        if (pVM->gcm.s.fFixerSet & s_aFixerIds[i].fFlag)
        {
            AssertReturn(offValidValues + s_aFixerIds[i].cchName + 4 <= sizeof(szValidValues), VERR_INTERNAL_ERROR_2);
            if (!offValidValues)
            {
                szValidValues[offValidValues++] = ' ';
                szValidValues[offValidValues++] = '(';
            }
            else
            {
                szValidValues[offValidValues++] = ',';
                szValidValues[offValidValues++] = ' ';
            }
            memcpy(&szValidValues[offValidValues], s_aFixerIds[i].pszName, s_aFixerIds[i].cchName);
            offValidValues += s_aFixerIds[i].cchName;
        }
    if (offValidValues)
        szValidValues[offValidValues++] = ')';
    szValidValues[offValidValues] = '\0';
    LogRel(("GCM: Initialized - Fixer bits: %#x%s\n", pVM->gcm.s.fFixerSet, szValidValues));

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMINTSAVEEXEC}
 */
static DECLCALLBACK(int) gcmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    AssertReturn(pVM,  VERR_INVALID_PARAMETER);
    AssertReturn(pSSM, VERR_SSM_INVALID_STATE);

    /*
     * At present there is only configuration to save.
     */
    return SSMR3PutU32(pSSM, pVM->gcm.s.fFixerSet);
}


/**
 * @callback_method_impl{FNSSMINTLOADEXEC}
 */
static DECLCALLBACK(int) gcmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;
    if (uVersion != GCM_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /*
     * Load configuration and check it aginst the current (live migration,
     * general paranoia).
     */
    uint32_t fFixerSet = 0;
    int rc = SSMR3GetU32(pSSM, &fFixerSet);
    AssertRCReturn(rc, rc);

    if (fFixerSet != pVM->gcm.s.fFixerSet)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Saved GCM fixer set %#X differs from the configured one (%#X)."),
                                fFixerSet, pVM->gcm.s.fFixerSet);

    return VINF_SUCCESS;
}


/**
 * Terminates the GCM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM itself is, at this point, powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) GCMR3Term(PVM pVM)
{
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * The VM is being reset.
 *
 * Do whatever fixer-specific resetting that needs to be done.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) GCMR3Reset(PVM pVM)
{
    RT_NOREF(pVM);
}

