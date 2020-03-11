/* $Id$ */
/** @file
 * tstDeviceSsmFuzz - SSM fuzzing testcase.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEFAULT /** @todo */
#include <VBox/types.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/fuzz.h>
#include <iprt/time.h>
#include <iprt/string.h>

#include "tstDeviceBuiltin.h"
#include "tstDeviceCfg.h"
#include "tstDeviceInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


static PCTSTDEVCFGITEM tstDevSsmFuzzGetCfgItem(PCTSTDEVCFGITEM paCfg, uint32_t cCfgItems, const char *pszName)
{
    for (uint32_t i = 0; i < cCfgItems; i++)
    {
        if (!RTStrCmp(paCfg[i].pszKey, pszName))
            return &paCfg[i];
    }

    return NULL;
}


static const char *tstDevSsmFuzzGetCfgString(PCTSTDEVCFGITEM paCfg, uint32_t cCfgItems, const char *pszName)
{
    PCTSTDEVCFGITEM pCfgItem = tstDevSsmFuzzGetCfgItem(paCfg, cCfgItems, pszName);
    if (   pCfgItem
        && pCfgItem->enmType == TSTDEVCFGITEMTYPE_STRING)
        return pCfgItem->u.psz;

    return NULL;
}


static uint64_t tstDevSsmFuzzGetCfgU64(PCTSTDEVCFGITEM paCfg, uint32_t cCfgItems, const char *pszName)
{
    PCTSTDEVCFGITEM pCfgItem = tstDevSsmFuzzGetCfgItem(paCfg, cCfgItems, pszName);
    if (   pCfgItem
        && pCfgItem->enmType == TSTDEVCFGITEMTYPE_INTEGER)
        return (uint64_t)pCfgItem->u.i64;

    return 0;
}


static uint32_t tstDevSsmFuzzGetCfgU32(PCTSTDEVCFGITEM paCfg, uint32_t cCfgItems, const char *pszName)
{
    PCTSTDEVCFGITEM pCfgItem = tstDevSsmFuzzGetCfgItem(paCfg, cCfgItems, pszName);
    if (   pCfgItem
        && pCfgItem->enmType == TSTDEVCFGITEMTYPE_INTEGER)
        return (uint32_t)pCfgItem->u.i64;

    return 0;
}


/**
 * Entry point for the SSM fuzzer.
 *
 * @returns VBox status code.
 * @param   hDut                The device under test.
 * @param   paCfg               The testcase config.
 * @param   cCfgItems           Number of config items.
 */
static DECLCALLBACK(int) tstDevSsmFuzzEntry(TSTDEVDUT hDut, PCTSTDEVCFGITEM paCfg, uint32_t cCfgItems)
{
    RT_NOREF(hDut, paCfg);

    RTFUZZCTX hFuzzCtx;
    int rc = RTFuzzCtxCreate(&hFuzzCtx, RTFUZZCTXTYPE_BLOB);
    if (RT_SUCCESS(rc))
    {
        rc = RTFuzzCtxCorpusInputAddFromDirPath(hFuzzCtx, tstDevSsmFuzzGetCfgString(paCfg, cCfgItems, "CorpusPath"));
        if (RT_SUCCESS(rc))
        {
            rc = RTFuzzCtxCfgSetInputSeedMaximum(hFuzzCtx, (size_t)tstDevSsmFuzzGetCfgU64(paCfg, cCfgItems, "InputSizeMax"));
            if (RT_SUCCESS(rc))
            {
                rc = RTFuzzCtxReseed(hFuzzCtx, tstDevSsmFuzzGetCfgU64(paCfg, cCfgItems, "Seed"));
                if (RT_SUCCESS(rc))
                {
                    /* Create a new SSM handle to use. */
                    PSSMHANDLE pSsm = (PSSMHANDLE)RTMemAllocZ(sizeof(*pSsm));
                    if (RT_LIKELY(pSsm))
                    {
                        pSsm->pDut          = hDut;
                        pSsm->pbSavedState  = NULL;
                        pSsm->cbSavedState  = 0;
                        pSsm->offDataBuffer = 0;
                        pSsm->uCurUnitVer   = tstDevSsmFuzzGetCfgU32(paCfg, cCfgItems, "UnitVersion");
                        pSsm->rc            = VINF_SUCCESS;

                        uint64_t cRuntimeMs = tstDevSsmFuzzGetCfgU64(paCfg, cCfgItems, "RuntimeSec") * RT_MS_1SEC_64;
                        uint64_t tsStart = RTTimeMilliTS();
                        uint64_t cFuzzedInputs = 0;
                        do
                        {
                            RTFUZZINPUT hFuzzInp;
                            rc = RTFuzzCtxInputGenerate(hFuzzCtx, &hFuzzInp);
                            if (RT_SUCCESS(rc))
                            {
                                void *pvBlob = NULL;
                                size_t cbBlob = 0;

                                rc = RTFuzzInputQueryBlobData(hFuzzInp, &pvBlob, &cbBlob);
                                if (RT_SUCCESS(rc))
                                {
                                    pSsm->pbSavedState  = (uint8_t *)pvBlob;
                                    pSsm->cbSavedState  = cbBlob;
                                    pSsm->offDataBuffer = 0;
                                    pSsm->rc            = VINF_SUCCESS;

                                    /* Get the SSM handler from the device. */
                                    int rcDut = VINF_SUCCESS;
                                    PTSTDEVDUTSSM pSsmClbks = RTListGetFirst(&hDut->LstSsmHandlers, TSTDEVDUTSSM, NdSsm);
                                    if (pSsmClbks)
                                    {
                                        /* Load preparations. */
                                        if (pSsmClbks->pfnLoadPrep)
                                            rcDut = pSsmClbks->pfnLoadPrep(hDut->pDevIns, pSsm);
                                        if (RT_SUCCESS(rcDut))
                                            rcDut = pSsmClbks->pfnLoadExec(hDut->pDevIns, pSsm, pSsm->uCurUnitVer, SSM_PASS_FINAL);

                                        cFuzzedInputs++;
                                    }
                                    if (RT_SUCCESS(rcDut))
                                        RTFuzzInputAddToCtxCorpus(hFuzzInp);
                                }
                                RTFuzzInputRelease(hFuzzInp);
                            }
                        } while (   RT_SUCCESS(rc)
                                 && RTTimeMilliTS() - tsStart < cRuntimeMs);

                        RTMemFree(pSsm);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
            }
        }

        RTFuzzCtxRelease(hFuzzCtx);
    }

    return rc;
}


const TSTDEVTESTCASEREG g_TestcaseSsmFuzz =
{
    /** szName */
    "SsmFuzz",
    /** pszDesc */
    "Fuzzes devices SSM state loaders",
    /** fFlags */
    0,
    /** pfnTestEntry */
    tstDevSsmFuzzEntry
};

