/* $Id$ */
/** @file
 * VirtualBox Main - Extension Pack Helper Application, usually set-uid-to-root.
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
#include "include/ExtPackUtil.h"

#include <iprt/buildconfig.h>
//#include <iprt/ctype.h>
//#include <iprt/dir.h>
//#include <iprt/env.h>
//#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
//#include <iprt/param.h>
#include <iprt/path.h>
//#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/version.h>


/* Override RTAssertShouldPanic to prevent gdb process creation. */
RTDECL(bool) RTAssertShouldPanic(void)
{
    return true;
}


/**
 * Implements the 'install' command.
 *
 * @returns The program exit code.
 * @param   argc            The number of program arguments.
 * @param   argv            The program arguments.
 */
static RTEXITCODE DoInstall(int argc, char **argv)
{
    return RTEXITCODE_FAILURE;
}

/**
 * Implements the 'uninstall' command.
 *
 * @returns The program exit code.
 * @param   argc            The number of program arguments.
 * @param   argv            The program arguments.
 */
static RTEXITCODE DoUninstall(int argc, char **argv)
{
    return RTEXITCODE_FAILURE;
}


int main(int argc, char **argv)
{
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    if (argc > 1)
    {
        /*
         * Command string switch.
         */
        if (!strcmp(argv[1], "install"))
            rcExit = DoInstall(argc, argv);
        else if (!strcmp(argv[1], "uninstall"))
            rcExit = DoUninstall(argc, argv);
        else
        {
            /*
             * Didn't match a command, check for standard options.
             */
            RTGETOPTSTATE State;
            rc = RTGetOptInit(&State, argc, argv, NULL, 0, 1, 0 /*fFlags*/);
            if (RT_SUCCESS(rc))
            {
                for (;;)
                {
                    RTGETOPTUNION ValueUnion;
                    int ch = RTGetOpt(&State, &ValueUnion);
                    switch (ch)
                    {
                        case 'h':
                            RTMsgInfo(VBOX_PRODUCT " Extension Pack Helper App\n"
                                      "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                                      "All rights reserved.\n"
                                      "\n"
                                      "This NOT intended for general use, please use VBoxManage instead\n"
                                      "or call the IExtPackManager API directly.\n"
                                      "\n"
                                      "Usage: %s <command> [options]\n"
                                      "Commands:\n"
                                      "    install --basepath <dir> --name <name> --tarball <tarball> --tarball-fd <fd>\n"
                                      "    uninstall --basepath <dir> --name <name>\n"
                                      , RTPathFilename(argv[0]));
                            rcExit = RTEXITCODE_SUCCESS;
                            break;

                        case 'V':
                            RTPrintf("%sr%d\n", VBOX_VERSION_STRING, RTBldCfgRevision());
                            rcExit = RTEXITCODE_SUCCESS;
                            break;

                        default:
                            rcExit = RTGetOptPrintError(ch, &ValueUnion);
                            break;
                    }
                }
            }
            else
                RTMsgError("RTGetOptInit failed: %Rrc\n", rc);
        }
    }
    else
        RTMsgError("No command was specified\n");
    return rcExit;
}

