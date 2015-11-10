/* $Id$ */
/** @file
 * VirtualBox Validation Kit - Boot Sector 3 object file convert.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iprt/types.h>
#include <iprt/assert.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Verbosity level. */
static unsigned g_cVerbose = 0;


/**
 * Opens a file for binary reading or writing.
 *
 * @returns File stream handle.
 * @param   pszFile             The name of the file.
 * @param   fWrite              Whether to open for writing or reading.
 */
static FILE *openfile(const char *pszFile,  bool fWrite)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    FILE *pFile = fopen(pszFile, fWrite ? "wb" : "rb");
#else
    FILE *pFile = fopen(pszFile, fWrite ? "w" : "r");
#endif
    if (!pFile)
        fprintf(stderr, "error: Failed to open '%s' for %s: %s (%d)\n",
                fWrite ? "writing" : "reading", strerror(errno), errno);
    return pFile;
}


static int readfile(const char *pszFile, void **ppvFile, size_t *pcbFile)
{

}


static int writefile(const char *pszFile, void *pvFile, size_t cbFile)
{

}


int main(int argc, char **argv)
{
    int rcExit = 0;

    /*
     * Scan the arguments.
     */
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            const char *pszOpt = &argv[i][1];
            if (*pszOpt == '-')
            {
                /* Convert long options to short ones. */
                pszOpt--;
                if (!strcmp(pszOpt, "--verbose"))
                    pszOpt = 'v';
                else if (!strcmp(pszOpt, "--version"))
                    pszOpt = "V";
                else if (!strcmp(pszOpt, "--help"))
                    pszOpt = "h";
                else
                {
                    fprintf(stderr, "syntax errro: Unknown options '%s'\n", pszOpt);
                    free(paInputs);
                    return 2;
                }
            }

            /* Process the list of short options. */
            while (*pszOpt)
            {
                switch (*pszOpt++)
                {
                    case 'v':
                        g_cVerbose++;
                        break;

                    case 'V':
                        printf("%s\n", "$Revision$");
                        return 0;

                    case '?':
                    case 'h':
                        printf("usage: %s [options] -o <output> <input1> [input2 ... [inputN]]\n",
                               argv[0]);
                        return 0;
                }
            }
        }
        else
        {
            /*
             * File to convert.  Do the job right away.
             */
        }
    }

    return rcExit;
}


