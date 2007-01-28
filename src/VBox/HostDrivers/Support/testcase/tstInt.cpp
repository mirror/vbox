/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the interrupt gate feature of the support library
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#include <VBox/vm.h>
#include <VBox/param.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/**
 * Makes a path to a file in the executable directory.
 */
static char *ExeDirFile(char *pszFile, const char *pszArgv0, const char *pszFilename)
{
    char   *psz;
    char   *psz2;

    strcpy(pszFile, pszArgv0);
    psz = strrchr(pszFile, '/');
    psz2 = strrchr(pszFile, '\\');
    if (psz < psz2)
        psz = psz2;
    if (!psz)
        psz = strrchr(pszFile, ':');
    if (!psz)
    {
        strcpy(pszFile, "./");
        psz = &pszFile[1];
    }
    strcpy(psz + 1, "VMMR0.r0");
    return pszFile;
}

int main(int argc, char **argv)
{
    int rcRet = 0;
    int rc;
    int cIterations = argc > 1 ? RTStrToUInt32(argv[1]) : 32;
    if (cIterations == 0)
        cIterations = 64;

    /*
     * Init.
     */
    RTR3Init();
    VM vm;
    memset(&vm, 0, sizeof(vm));
    rc = SUPInit(&vm.pSession);
    rcRet += rc != 0;
    RTPrintf("tstInt: SUPInit -> rc=%Vrc\n", rc);
    if (!rc)
    {
        /*
         * Load VMM code.
         */
        char    szFile[RTPATH_MAX];
        rc = SUPLoadVMM(ExeDirFile(szFile, argv[0], "VMMR0.r0"));
        rcRet += rc != 0;
        if (!rc)
        {
            /*
             * Call VMM code with invalid function.
             */
            for (int i = cIterations; i > 0; i--)
            {
                rc = SUPCallVMMR0(&vm, VMMR0_DO_NOP, NULL);
                if (rc != VINF_SUCCESS)
                {
                    RTPrintf("tstInt: SUPCallVMMR0 -> rc=%Vrc i=%d Expected VINF_SUCCESS!\n", rc, i);
                    break;s
                }
            }
            RTPrintf("tstInt: Performed SUPCallVMMR0 %d times (rc=%Vrc)\n", cIterations, rc);

            /*
             * Unload VMM.
             */
            rc = SUPUnloadVMM();
            rcRet += rc != 0;
            if (rc)
                RTPrintf("tstInt: SUPUnloadVMM failed with rc=%Vrc\n", rc);
        }
        else
            RTPrintf("tstInt: SUPLoadVMM failed with rc=%Vrc\n", rc);

        /*
         * Terminate.
         */
        rc = SUPTerm();
        rcRet += rc != 0;
        RTPrintf("tstInt: SUPTerm -> rc=%Vrc\n", rc);
    }

    return !!rc;
}

