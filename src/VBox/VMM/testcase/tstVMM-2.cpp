/* $Id$ */
/** @file
 * VMM Testcase - no. 2.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/cpum.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/runtime.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/stream.h>

#include <stdio.h> /** @todo get rid of this. */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define TESTCASE    "tstVMM-2"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct TSTVMMARGS
{
    /** The filename. */
    const char *pszFilename;
    /** The opened file. */
    FILE       *pFile;
} TSTVMMARGS, *PTSTVMMARGS;



/**
 * Configuration constructor - takes flat file as input.
 */
static DECLCALLBACK(int) ConfigConstructor(PVM pVM, void *pvArgs)
{
    PTSTVMMARGS pArgs = (PTSTVMMARGS)pvArgs;
    LogFlow(("%s: Reading configuration %s.\n", TESTCASE, pArgs->pszFilename));

    /*
     * Read the file.
     */
    unsigned    iLine = 0;
    char        szLine[16384];
    PCFGMNODE   pNode = NULL;
    while (fgets(szLine, sizeof(szLine), pArgs->pFile))
    {
        iLine++;
        char *psz = szLine;
        /* leading blanks */
        while (isspace(*psz))
            psz++;
        if (*psz == '[')
        {
            /*
             * Key path.
             */
            psz++;
            if (*psz != '/')
            {
                RTPrintf("%s(%d): error: key path must start with slash.\n", pArgs->pszFilename, iLine);
                return VERR_GENERAL_FAILURE;
            }
            char *pszEnd = strchr(psz, ']');
            if (!pszEnd)
            {
                RTPrintf("%s(%d): error: missing ']'.\n", pArgs->pszFilename, iLine);
                return VERR_GENERAL_FAILURE;
            }
            while (pszEnd - 1 > psz && pszEnd[-1] == '/') /* strip trailing slashes. */
                pszEnd--;
            *pszEnd++ = '/';
            *pszEnd = '\0';

            /*
             * Create the key path.
             */
            pNode = CFGMR3GetRoot(pVM);
            char *pszCur = psz + 1;
            while (pNode && psz < pszEnd)
            {
                char *pszCurEnd = strchr(pszCur, '/');
                *pszCurEnd = '\0';
                PCFGMNODE pChild = CFGMR3GetChild(pNode, pszCur);
                if (!pChild)
                {
                    int rc = CFGMR3InsertNode(pNode, pszCur, &pChild);
                    if (VBOX_FAILURE(rc))
                    {
                        *pszCurEnd = '/';
                        RTPrintf("%s(%d): error: failed to create node '%s', rc=%d.\n", pArgs->pszFilename, iLine, psz, rc);
                        return rc;
                    }
                }

                /* next path component */
                *pszCurEnd = '/';
                pNode = pChild;
                pszCur = pszCurEnd + 1;
            }
        }
        else if (*psz != '#' && *psz != ';' && *psz != '\0' && *psz != '\n' && *psz != '\r')
        {
            /*
             * Value Name.
             */
            char *pszName = psz;
            char *pszNameEnd;
            if (*pszName == '"' || *pszName == '\'')
            {
                /* quoted name */
                pszNameEnd = pszName;
                for (;;)
                {
                    pszNameEnd = strchr(pszNameEnd + 1, *pszName);
                    if (!pszNameEnd)
                    {
                        RTPrintf("%s(%d): error: unbalanced quote.\n", pArgs->pszFilename, iLine);
                        return VERR_GENERAL_FAILURE;
                    }
                    if (pszNameEnd[1] != pszNameEnd[0])
                        break;
                    pszNameEnd++;
                }
                psz = strchr(pszNameEnd, '=');
                *pszNameEnd-- = '\0';
                *pszName++ = '\0';
            }
            else
            {
                /* unquoted name. Name ends before '<type>' or '='. */
                pszNameEnd = strpbrk(psz, "=<");
                if (!pszNameEnd)
                {
                    RTPrintf("%s(%d): error: missing '='.\n", pArgs->pszFilename, iLine);
                    return VERR_GENERAL_FAILURE;
                }
                psz = strchr(pszNameEnd, '=');
                *pszNameEnd-- = '\0';
                while (pszNameEnd > psz && isspace(pszNameEnd[-1]))
                    pszNameEnd--;
                if (!pszNameEnd)
                {
                    RTPrintf("%s(%d): error: missing '<'.\n", pArgs->pszFilename, iLine);
                    return VERR_GENERAL_FAILURE;
                }
            }
            if (pszName == pszNameEnd)
            {
                RTPrintf("%s(%d): error: empty value name.\n", pArgs->pszFilename, iLine);
                return VERR_GENERAL_FAILURE;
            }

            /* check if equal sign present and skip past it and all following blanks. */
            if (!psz)
            {
                RTPrintf("%s(%d): error: missing '='.\n", pArgs->pszFilename, iLine);
                return VERR_GENERAL_FAILURE;
            }
            psz++;
            while (isspace(*psz))
                psz++;

            /*
             * Value.
             */
            if (*psz == '"' || *psz == '\'')
            {
                /* string */
                char *pszValueEnd = psz;
                for (;;)
                {
                    pszValueEnd = strchr(pszValueEnd + 1, *psz);
                    if (!pszValueEnd)
                    {
                        RTPrintf("%s(%d): error: unbalanced quote in string value.\n", pArgs->pszFilename, iLine);
                        return VERR_GENERAL_FAILURE;
                    }
                    if (pszValueEnd[1] != pszValueEnd[0])
                        break;
                    pszValueEnd++;
                }
                *pszValueEnd-- = '\0';
                *psz++ = '\0';

                int rc = CFGMR3InsertString(pNode, pszName, psz);
                if (VBOX_FAILURE(rc))
                {
                    RTPrintf("%s(%d): error: failed to insert string value named '%s' and with value '%s', rc=%d.\n",
                             pArgs->pszFilename, iLine, pszName, psz, rc);
                    return rc;
                }
            }
            else if (   isxdigit(psz[0]) && isxdigit(psz[1])
                     && isspace(psz[2])
                     && isxdigit(psz[3]) && isxdigit(psz[4]))
            {
                /* byte string */
                RTPrintf("%s(%d): error: byte string is not implemented\n", pArgs->pszFilename, iLine);
                return VERR_GENERAL_FAILURE;
            }
            else if (isdigit(psz[0]))
            {
                /* integer */
                uint64_t u64 = 0;
                if (psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X'))
                {   /* hex */
                    psz += 2;
                    while (isxdigit(*psz))
                    {
                        u64 *= 16;
                        u64 += *psz <= '9' ? *psz - '0' : *psz >= 'a' ? *psz - 'a' + 10 : *psz - 'A' + 10;
                        psz++;
                    }
                }
                else
                {   /* decimal */
                    while (isdigit(*psz))
                    {
                        u64 *= 10;
                        u64 += *psz - '0';
                        psz++;
                    }
                }
                if (isalpha(*psz))
                {
                    RTPrintf("%s(%d): error: unexpected char(s) after number '%.10s'\n", pArgs->pszFilename, iLine, psz);
                    return VERR_GENERAL_FAILURE;
                }

                int rc = CFGMR3InsertInteger(pNode, pszName, u64);
                if (VBOX_FAILURE(rc))
                {
                    RTPrintf("%s(%d): error: failed to insert integer value named '%s' and with value %#llx, rc=%d.\n",
                             pArgs->pszFilename, iLine, pszName, u64, rc);
                    return rc;
                }
            }
            else
            {
                RTPrintf("%s(%d): error: unknown value format.\n", pArgs->pszFilename, iLine);
                return VERR_GENERAL_FAILURE;
            }
        }
        /* else: skip */
    } /* for each line */

    LogFlow(("%s: successfully read config\n"));
    return VINF_SUCCESS;
}



int main(int argc, char **argv)
{
    TSTVMMARGS  Args = {0};
    int         rcRet = 0;              /* error count. */
    int         fPowerOn = 0;

    RTR3Init();


    /*
     * Parse arguments.
     */
    for (int i = 1; i < argc; i++)
    {
        const char *psz = argv[i];
        if (*psz == '-')
        {
            psz++;
            if (*psz == '-')
                psz++;
            if (!strcmp(psz, "config"))
            {
                if (Args.pszFilename)
                {
                    RTPrintf("syntax error: only one config argument!\n");
                    return 1;
                }
                if (i + 1 >= argc)
                {
                    RTPrintf("syntax error: no configuration filename!\n");
                    return 1;
                }
                Args.pszFilename = argv[++i];
                Args.pFile = fopen(Args.pszFilename, "r");
                if (!Args.pFile)
                {
                    RTPrintf("%s: Failed to open '%s' for reading!\n", TESTCASE, Args.pszFilename);
                    return 1;
                }
            }
            else if (!strcmp(psz, "poweron"))
                fPowerOn = 1;
            else if (   !strncmp(psz, "help", 4)
                     || (*psz == 'h' && !psz[1]))
            {
                RTPrintf("syntax: %s [options]\n"
                         "\n"
                         "options (prefixed with a dash or two)\n"
                         "  config <filename>     Load the flat config file.\n"
                         "  help                  This help text.\n", argv[0]);
                return 1;
            }
            else
            {
                RTPrintf("syntax error: unknown option '%s' in arg %d.\n", psz, i);
                return 1;
            }

        }
        else
        {
            RTPrintf("syntax error: Sorry dude, no idea what you're passing to me.\n"
                     "              arg %d '%s'\n", i, argv[i]);
            return 1;
        }
    }


    /*
     * Create empty VM.
     */
    PVM pVM;
    int rc = VMR3Create(NULL, NULL, Args.pszFilename ? ConfigConstructor : NULL, &Args, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Power it on if that was requested.
         */
        if (fPowerOn)
        {
            PVMREQ pReq;
            rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)VMR3PowerOn, 1, pVM);
            if (VBOX_SUCCESS(rc))
            {
                rc = pReq->iStatus;
                VMR3ReqFree(pReq);
                if (VBOX_SUCCESS(rc))
                {
                    RTPrintf(TESTCASE ": info: VMR3PowerOn succeeded. rc=%d\n", rc);
                    RTPrintf(TESTCASE ": info: main thread is taking a nap...\n");
                    rc = RTThreadSleep(8*3600*1000); /* 8 hours */
                    RTPrintf(TESTCASE ": info: main thread is woke up... rc=%d\n", rc);

                    /*
                     * Power Off the VM.
                     */
                    rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)VMR3PowerOff, 1, pVM);
                    if (VBOX_SUCCESS(rc))
                    {
                        rc = pReq->iStatus;
                        VMR3ReqFree(pReq);
                        if (VBOX_SUCCESS(rc))
                            RTPrintf(TESTCASE ": info: Successfully powered off the VM. rc=%d\n", rc);
                        else
                        {
                            RTPrintf(TESTCASE ": error: VMR3PowerOff -> %d\n", rc);
                            rcRet++;
                        }
                    }
                    else
                    {
                        RTPrintf(TESTCASE ": error: VMR3ReqCall (power off) -> %d\n", rc);
                        rcRet++;
                    }
                }
            }
            else
            {
                RTPrintf(TESTCASE ": error: VMR3ReqCall (power on) -> %d\n", rc);
                rcRet++;
            }
        }

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%d\n", rc);
            rcRet++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%d\n", rc);
        rcRet++;
    }

    return rcRet;
}
