/* $Id$ */
/** @file
 * innotek Portable Runtime - Command Line Parsing
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/getopt.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/assert.h>



RTDECL(int) RTGetOpt(int argc, char *argv[], PCRTOPTIONDEF paOptions, size_t cOptions, int *piThis, PRTOPTIONUNION pValueUnion)
{
    pValueUnion->pDef = NULL;

    if (    !piThis
         || *piThis >= argc
       )
        return 0;

    int iThis = (*piThis)++;
    const char *pszArgThis = argv[iThis];

    if (*pszArgThis == '-')
    {
        for (size_t i = 0; i < cOptions; i++)
        {
            bool fShort = false;
            if (    (   paOptions[i].pszLong
                     && !strcmp(pszArgThis, paOptions[i].pszLong))
                ||  (   (fShort = (pszArgThis[1] == paOptions[i].iShort))
                     && pszArgThis[2] == '\0'
                    )
               )
            {
                Assert(!(paOptions[i].fFlags & ~RTGETOPT_REQ_MASK));
                pValueUnion->pDef = &paOptions[i];

                if ((paOptions[i].fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING)
                {
                    if (iThis >= argc - 1)
                        return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;

                    int iNext = (*piThis)++;
                    switch (paOptions[i].fFlags & RTGETOPT_REQ_MASK)
                    {
                        case RTGETOPT_REQ_STRING:
                            pValueUnion->psz = argv[iNext];
                            break;

                        case RTGETOPT_REQ_INT32:
                        {
                            int32_t i32;
                            if (RTStrToInt32Full(argv[iNext], 10, &i32))
                                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

                            pValueUnion->i32 = i32;
                            break;
                        }

                        case RTGETOPT_REQ_UINT32:
                        {
                            uint32_t u32;
                            if (RTStrToUInt32Full(argv[iNext], 10, &u32))
                                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    
                            pValueUnion->u32 = u32;
                            break;
                        }

                        default:
                            AssertMsgFailed(("i=%d f=%#x\n", i, paOptions[i].fFlags));
                            return VERR_INTERNAL_ERROR;
                    }
                }

                return paOptions[i].iShort;
            }
        }
    }

    /** @todo Sort options and arguments (i.e. stuff that doesn't start with '-'), stop when 
     * encountering the first argument. */

    return VERR_GETOPT_UNKNOWN_OPTION;
}

