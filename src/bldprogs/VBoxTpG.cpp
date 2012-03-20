/* $Id$ */
/** @file
 * IPRT Testcase / Tool - VBox Tracepoint Compiler.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scmstream.h"

enum
{
    kVBoxTpGOpt_32Bit = 1000,
    kVBoxTpGOpt_64Bit,
    kVBoxTpGOpt_Assembler,
    kVBoxTpGOpt_AssemblerOption,
    kVBoxTpGOpt_AssemblerFmtOpt,
    kVBoxTpGOpt_AssemblerFmtVal,
    kVBoxTpGOpt_AssemblerOutputOpt,
};


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return 1;

    /*
     * Parse arguments and process input in order (because this is the only
     * thing that works at the moment).
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "-32",                                kVBoxTpGOpt_32BIT,                      RTGETOPT_REQ_NOTHING },
        { "-64",                                kVBoxTpGOpt_64BIT,                      RTGETOPT_REQ_NOTHING },
        { "--apply-cpp",                        'C',                                    RTGETOPT_REQ_NOTHING },
        { "--generate-obj",                     'G',                                    RTGETOPT_REQ_NOTHING },
        { "--generate-header",                  'h',                                    RTGETOPT_REQ_NOTHING },
        { "--output",                           'o',                                    RTGETOPT_REQ_STRING  },
        { "--script",                           's',                                    RTGETOPT_REQ_STRING  },
        { "--assembler",                        kVBoxTpGOpt_Assembler,                  RTGETOPT_REQ_STRING  },
        { "--assembler-fmt-opt",                kVBoxTpGOpt_AssemblerFmtOpt,            RTGETOPT_REQ_STRING  },
        { "--assembler-fmt-val",                kVBoxTpGOpt_AssemblerFmtVal,            RTGETOPT_REQ_STRING  },
        { "--assembler-output-opt",             kVBoxTpGOpt_AssemblerOutputOpt,         RTGETOPT_REQ_STRING  },
        { "--assembler-option",                 kVBoxTpGOpt_AssemblerOption,            RTGETOPT_REQ_STRING  },
    };

    enum 
    {
        kVBoxTpGAction_Nothing,
        kVBoxTpGAction_GenerateHeader,
        kVBoxTpGAction_GenerateObject,
    }               enmAction = kVBoxTpGAction_Nothing;

    const char     *pszAssembler                 = "yasm";
    const char     *pszAssemblerFmtOpt           = "--oformat";
#ifdef RT_OS_DARWIN
    const char     *pszAssemblerFmtVal           = "macho" RT_XSTR(ARCH_BITS);
#elif defined(RT_OS_WINDOWS)
    const char     *pszAssemblerFmtVal           = "win" RT_XSTR(ARCH_BITS);
#else
    const char     *pszAssemblerFmtVal           = "elf" RT_XSTR(ARCH_BITS);
#endif
    const char     *pszAssemblerOutputOpt        = "-o"
    const char     *apszAssemblerOptions[32]     = { NULL, NULL,  };


    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, 1);
    size_t          cProcessed = 0;

    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'h':
                RTPrintf("VirtualBox Tracepoint Generator\n"
                         "\n"
                         "Usage: %s [options]\n"
                         "\n"
                         "Options:\n", g_szProgName);
                for (size_t i = 0; i < RT_ELEMENTS(s_aOpts); i++)
                    if ((unsigned)s_aOpts[i].iShort < 128)
                        RTPrint("   -%c,%s\n", s_aOpts[i].iShort, s_aOpts[i].pszLong);
                    else
                        RTPrint("   %s\n", s_aOpts[i].pszLong);
                return 1;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                static const char s_szRev[] = "$Revision$";
                const char *psz = RTStrStripL(strchr(s_szRev, ' '));
                RTPrintf("r%.*s\n", strchr(psz, ' ') - psz, psz);
                return 0;
            }


            case VINF_GETOPT_NOT_OPTION:
                if (enmAction == kVBoxTpGAction_GenerateObject)
                    break; /* object files, ignore them. */
                /* fall thru */

            default:
            {
                int rc2 = scmSettingsBaseHandleOpt(&pSettings->Base, rc, &ValueUnion);
                if (RT_SUCCESS(rc2))
                    break;
                if (rc2 != VERR_GETOPT_UNKNOWN_OPTION)
                    return 2;
                return RTGetOptPrintError(rc, &ValueUnion);
            }
        }
    }


    return 0;
}
