/* $Id$ */
/** @file
 * VBoxServiceToolBox - Internal (BusyBox-like) toolbox.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <stdio.h>

#include <iprt/assert.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/**
 * Displays a help text to stdout.
 */
void VBoxServiceToolboxShowUsage(void)
{
    RTPrintf("Toolbox Usage:\n"
             "cat [FILE] - Concatenate FILE(s), or standard input, to standard output\n"
             "\n");
}

/**
 *
 *
 * @return  int
 *
 * @param   pszFormat
 */
int VBoxServiceToolboxErrorSyntax(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    RTPrintf("\n"
             "Syntax error: %N\n", pszFormat, &args);
    va_end(args);
    VBoxServiceToolboxShowUsage();
    return VERR_INVALID_PARAMETER;
}

int VBoxServiceToolboxCatMain(int argc, char **argv)
{
    int rc = VINF_SUCCESS;
    bool usageOK = true;
    for (int i = 1; usageOK && i < argc; i++)
    {
    }
    if (!usageOK)
        rc = VBoxServiceToolboxErrorSyntax(0 /* TODO */, "Incorrect parameters");
    return rc;
}

/**
 * Main routine for toolbox command line handling.
 *
 * @return  int
 *
 * @param   argc
 * @param   argv
 */
int VBoxServiceToolboxMain(int argc, char **argv)
{
    int rc = VERR_NOT_FOUND;

    bool fUsageOK = true;
    if (argc >= 1) /* Do we have at least a main command? */
    {
        if (!strcmp(argv[1], "cat"))
        {
            /** @todo Move this block into an own "cat main" routine! */
            PRTSTREAM pStream;
            bool fHaveFile = false;

            /* Do we have a file as second argument? */
            if (argc == 2)
            {
                /* Use stdin as standard input stream. */
                pStream = g_pStdIn;
                rc = VINF_SUCCESS;
            }
            else if (argc == 3)
            {
                rc = RTStrmOpen(argv[2], /* Filename */
                                "rb",    /* Read binary */
                                &pStream);
                if (RT_SUCCESS(rc))
                    fHaveFile = true;
            }
            else
                fUsageOK = false;


            if (RT_SUCCESS(rc))
            {
                uint8_t cBuf[_64K];
                uint32_t cbRead;
                do
                {
                    rc = RTStrmReadEx(pStream, cBuf, sizeof(cBuf), &cbRead);
                    if (RT_SUCCESS(rc))
                        rc = RTStrmWrite(g_pStdOut, cBuf, cbRead);
                } while (RT_SUCCESS(rc) && cbRead);
                RTStrmFlush(g_pStdOut);

                /* Close opened file handle. */
                if (fHaveFile)
                    rc = RTStrmClose(pStream);
            }
        }
    }
    /* Ignore usage errors when using an unknown command - this might
     * be a regular VBoxService startup command (--whatever)! */
    if (!fUsageOK)
        rc = VBoxServiceToolboxErrorSyntax(0 /* TODO */, "Incorrect parameters");

    if (rc != VERR_NOT_FOUND)
        rc = RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    return rc;
}

