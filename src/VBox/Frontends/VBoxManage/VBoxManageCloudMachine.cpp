/* $Id$ */
/** @file
 * VBoxManageCloudMachine - The cloud machine related commands.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxManage.h"

#include <VBox/log.h>

#include <VBox/com/ErrorInfo.h>
#include <VBox/com/Guid.h>
#include <VBox/com/errorprint.h>


RTEXITCODE listCloudMachines(HandlerArg *a, int iFirst, const ComPtr<ICloudProfile> &pCloudProfile);


/*
 * RTGETOPTINIT_FLAGS_NO_STD_OPTS recognizes both --help and --version
 * and we don't want the latter.  It's easier to add one line of this
 * macro to the s_aOptions initializers than to filter out --version.
 */
#define CLOUD_MACHINE_RTGETOPTDEF_HELP                                      \
        { "--help",         'h',                    RTGETOPT_REQ_NOTHING }, \
        { "-help",          'h',                    RTGETOPT_REQ_NOTHING }, \
        { "help",           'h',                    RTGETOPT_REQ_NOTHING }, \
        { "-?",             'h',                    RTGETOPT_REQ_NOTHING }


/*
 * VBoxManage cloud machine ...
 *
 * The "cloud" prefix handling is in VBoxManageCloud.cpp, so this
 * function is not static.
 */
RTEXITCODE handleCloudMachine(HandlerArg *a, int iFirst,
                              const ComPtr<ICloudProfile> &pCloudProfile)
{
    enum
    {
        kMachineIota = 1000,
        kMachine_Info,
        kMachine_List,
    };

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "info",           kMachine_Info,          RTGETOPT_REQ_NOTHING },
        { "list",           kMachine_List,          RTGETOPT_REQ_NOTHING },
          CLOUD_MACHINE_RTGETOPTDEF_HELP
    };

    int rc;

    RTGETOPTSTATE OptState;
    rc = RTGetOptInit(&OptState, a->argc, a->argv,
                      s_aOptions, RT_ELEMENTS(s_aOptions),
                      iFirst, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCStmt(rc,
        return RTMsgErrorExit(RTEXITCODE_INIT, /* internal error */
                   "cloud machine: RTGetOptInit: %Rra", rc));

    int ch;
    RTGETOPTUNION Val;
    while ((ch = RTGetOpt(&OptState, &Val)) != 0)
    {
        switch (ch)
        {
            case kMachine_Info:
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "cloud machine info: not yet implemented");

            case kMachine_List:
                return listCloudMachines(a, OptState.iNext, pCloudProfile);


            case 'h':           /* --help */
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;


            case VINF_GETOPT_NOT_OPTION:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                           "Invalid sub-command: %s", Val.psz);

            default:
                return RTGetOptPrintError(ch, &Val);
        }
    }

    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
               "cloud machine: command required\n"
               "Try '--help' for more information.");
}


/*
 * VBoxManage cloud list machines
 * VBoxManage cloud machine list # convenience alias
 *
 * The "cloud list" prefix handling is in VBoxManageCloud.cpp, so this
 * function is not static.
 */
RTEXITCODE listCloudMachines(HandlerArg *a, int iFirst,
                             const ComPtr<ICloudProfile> &pCloudProfile)
{
    RT_NOREF(a, iFirst, pCloudProfile);

    return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "cloud list machines - not yet implemented");
}
