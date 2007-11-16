/** @file
 * innotek Portable Runtime - Command Line Parsing
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/getopt.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <string.h>

/*******************************************************************************
*   Code                                                                       *
*******************************************************************************/
int RTGetOpt(int argc,
             char *argv[],
             const RTOPTIONDEF *paOptions,
             int cOptions,
             int *piThis,
             RTOPTIONUNION *pValueUnion)
{
    if (    (!piThis)
         || (*piThis >= argc)
       )
        return 0;

    int iThis = (*piThis)++;
    const char *pcszArgThis = argv[iThis];

    if (*pcszArgThis == '-')
    {
        int i;
        for (i = 0;
             i < cOptions;
             ++i)
        {
            bool fShort = false;
            if (    (!strcmp(pcszArgThis, paOptions[i].pcszLong))
                 || (    ((fShort = (pcszArgThis[1] == paOptions[i].cShort)))
                      && (pcszArgThis[2] == '\0')
                    )
               )
            {
                if (paOptions[i].fl & RTGETOPT_REQUIRES_ARGUMENT)
                {
                    if (iThis >= argc - 1)
                    {
                        pValueUnion->pcsz = paOptions[i].pcszLong;
                        return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;
                    }
                    else
                    {
                        int iNext = (*piThis)++;
                        if (paOptions[i].fl & RTGETOPT_ARG_FORMAT_INT32)
                        {
                            int32_t i;
                            if (RTStrToInt32Full(argv[iNext], 10, &i))
                            {
                                pValueUnion->pcsz = paOptions[i].pcszLong;
                                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
                            }

                            pValueUnion->i = i;
                        }
                        else if (paOptions[i].fl & RTGETOPT_ARG_FORMAT_UINT32)
                        {
                            uint32_t u;
                            if (RTStrToUInt32Full(argv[iNext], 10, &u))
                            {
                                pValueUnion->pcsz = paOptions[i].pcszLong;
                                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
                            }

                            pValueUnion->u = u;
                        }
                        else
                            pValueUnion->pcsz = argv[iNext];
                    }
                }

                return paOptions[i].cShort;
            }
        }
    }

    return VERR_GETOPT_UNKNOWN_OPTION;
}

