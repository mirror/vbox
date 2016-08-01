/* $Id$ */
/** @file
 * Guest Control Service: Internal function used by service, Main and testcase.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/HostServices/GuestControlSvc.h>

/** @todo Remove unused header files below! */
#include <iprt/alloca.h>
#include <iprt/initterm.h>
#include <iprt/crc.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/handle.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/thread.h>

#include "gctrl.h"

namespace guestControl {

#if 0 /* unused */
/**
 * Creates the argument list as an array used for executing a program.
 *
 * @returns VBox status code.
 */
int gctrlPrepareExecArgv(char *pszArgs, void **ppvList, uint32_t *pcbList, uint32_t *pcArgs)
{
    char **ppaArg;
    int cArgs;
    int rc = RTGetOptArgvFromString(&ppaArg, &cArgs, pszArgs, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH, NULL);
    if (RT_SUCCESS(rc))
    {
        /** @todo calc size first, allocate buffer, copy stuff. */
        char *pszTemp = NULL;
        *pcbList = 0;
        for (int i = 0; i < cArgs; i++)
        {
            if (i > 0) /* Insert space as delimiter. */
            {
                rc = RTStrAAppendN(&pszTemp, " ", 1);
                if (RT_FAILURE(rc))
                    break;
            }
            rc = RTStrAAppendN(&pszTemp, ppaArg[i], strlen(ppaArg[i]));
            if (RT_FAILURE(rc))
                break;
        }

        RTGetOptArgvFree(ppaArg);

        if (RT_SUCCESS(rc))
        {
            *ppvList = pszTemp;
            *pcArgs  = cArgs;
            *pcbList = (uint32_t)(strlen(pszTemp) + 1); /* Include zero termination. */
        }
        else
            RTStrFree(pszTemp);
    }
    return rc;
}
#endif


#if 0 /* unused */
/**
 * Appends environment variables to the environment block.
 *
 * Each var=value pair is separated by NULL (\0) sequence. The whole block will
 * be stored in one blob and disassembled on the guest side later to fit into
 * the HGCM param structure.
 *
 * @returns VBox status code.
 *
 * @todo Write proper documentation
 * @remarks currently not used.
 */
int gctrlAddToExecEnvv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnv)
{
    int rc = VINF_SUCCESS;
    size_t cchLen = strlen(pszEnv);
    if (*ppvList)
    {
        size_t cbNewLen = *pcbList + cchLen + 1; /* Include zero termination. */
        char *pvTmp = (char*)RTMemRealloc(*ppvList, cbNewLen);
        if (NULL == pvTmp)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            memcpy(pvTmp + *pcbList, pszEnv, cchLen);
            pvTmp[cbNewLen - 1] = '\0'; /* Add zero termination. */
            *ppvList = (void**)pvTmp;
        }
    }
    else
    {
        char *pcTmp;
        if (RTStrAPrintf(&pcTmp, "%s", pszEnv) > 0)
        {
            *ppvList = (void**)pcTmp;
            /* Reset counters. */
            *pcEnv = 0;
            *pcbList = 0;
        }
    }
    if (RT_SUCCESS(rc))
    {
        *pcbList += (uint32_t)(cchLen + 1); /* Include zero termination. */
        *pcEnv   += 1;                      /* Increase env pairs count. */
    }
    return rc;
}
#endif /* unused */

/*
int gctrlAllocateExecBlock(PVBOXGUESTCTRLEXECBLOCK *ppBlock,
                           const char *pszCmd, uint32_t fFlags,
                           uint32_t cArgs,    const char * const *papszArgs,
                           uint32_t cEnvVars, const char * const *papszEnv,
                           const char *pszStdIn, const char *pszStdOut, const char *pszStdErr,
                           const char *pszUsername, const char *pszPassword, RTMSINTERVAL cMillies)
{
    PVBOXGUESTCTRLEXECBLOCK pNewBlock = (VBOXGUESTCTRLEXECBLOCK*)RTMemAlloc(sizeof(VBOXGUESTCTRLEXECBLOCK));
    int rc;
    if (pNewBlock)
    {


        *ppBlock = pNewBlock;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


int gctrlFreeExecBlock(PVBOXGUESTCTRLEXECBLOCK pBlock)
{
    AssertPtr(pBlock);

    RTStrFree(pBlock->pszCmd);
    RTMemFree(pBlock->pvArgs);
    RTMemFree(pBlock->pvEnv);
    RTStrFree(pBlock->pszStdIn);
    RTStrFree(pBlock->pszStdOut);
    RTStrFree(pBlock->pszStdErr);
    RTStrFree(pBlock->pszUsername);
    RTStrFree(pBlock->pszPassword);

    RT_ZERO(*pBlock);
    return VINF_SUCCESS;
}


int gctrlPrepareHostCmdExec(PVBOXHGCMSVCPARM *ppaParms, uint32_t *pcParms,
                            PVBOXGUESTCTRLEXECBLOCK pBlock)
{
    AssertPtr(ppaParms);
    AssertPtr(pBlock);

    PVBOXHGCMSVCPARM pNewParms =
        (VBOXHGCMSVCPARM*)RTMemAlloc(sizeof(VBOXHGCMSVCPARM) * 13);

    int rc;
    if (pNewParms)
    {
        pNewParms[0].setUInt32(HOST_EXEC_CMD);
        pNewParms[1].setUInt32(pBlock->u32Flags);
        pNewParms[2].setPointer((void*)pBlock->pszCmd, (uint32_t)strlen(pBlock->pszCmd) + 1);
        pNewParms[3].setUInt32(pBlock->u32Args);
        pNewParms[4].setPointer((void*)pBlock->pvArgs, pBlock->cbArgs);
        pNewParms[5].setUInt32(pBlock->u32EnvVars);
        pNewParms[6].setPointer((void*)pBlock->pvEnv, pBlock->cbEnv);
        pNewParms[7].setPointer((void*)pBlock->pszStdIn, (uint32_t)strlen(pBlock->pszStdIn) + 1);
        pNewParms[8].setPointer((void*)pBlock->pszStdOut, (uint32_t)strlen(pBlock->pszStdOut) + 1);
        pNewParms[9].setPointer((void*)pBlock->pszStdErr, (uint32_t)strlen(pBlock->pszStdErr) + 1);
        pNewParms[10].setPointer((void*)pBlock->pszUsername, (uint32_t)strlen(pBlock->pszUsername) + 1);
        pNewParms[11].setPointer((void*)pBlock->pszPassword, (uint32_t)strlen(pBlock->pszPassword) + 1);
        pNewParms[12].setUInt32(pBlock->cMillies);

        *ppaParms = pNewParms;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    if (pcParms)
        *pcParms = 13;

    return rc;
}


void gctrlFreeHostCmd(PVBOXHGCMSVCPARM paParms)
{
    RTMemFree(paParms);
}
*/

}

