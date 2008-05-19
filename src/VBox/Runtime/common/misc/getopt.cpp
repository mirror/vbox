/* $Id$ */
/** @file
 * IPRT - Command Line Parsing
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/getopt.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>



RTDECL(int) RTGetOpt(int argc, char **argv, PCRTOPTIONDEF paOptions, size_t cOptions, int *piThis, PRTOPTIONUNION pValueUnion)
{
    pValueUnion->u64 = 0;
    pValueUnion->pDef = NULL;

    if (    !piThis
         || *piThis >= argc
       )
        return 0;

    int iThis = (*piThis)++;
    const char *pszArgThis = argv[iThis];

    if (*pszArgThis == '-')
    {
/** @todo implement '--'. */
        for (size_t i = 0; i < cOptions; i++)
        {
            Assert(!(paOptions[i].fFlags & ~RTGETOPT_VALID_MASK));
            Assert(paOptions[i].iShort > 0);

            if ((paOptions[i].fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING)
            {
                /*
                 * A value is required with the argument. We're trying to very
                 * understanding here and will permit any of the following:
                 *      -svalue, -s:value, -s=value,
                 *      -s value, -s: value, -s= value
                 * (Ditto for long options.)
                 */
                bool fShort = false;
                size_t cchLong = 2;
                if (    (   paOptions[i].pszLong
                         && !strncmp(pszArgThis, paOptions[i].pszLong, (cchLong = strlen(paOptions[i].pszLong)))
                         && (   pszArgThis[cchLong] == '\0'
                             || pszArgThis[cchLong] == ':'
                             || pszArgThis[cchLong] == '=')
                        )
                    ||  (fShort = (pszArgThis[1] == paOptions[i].iShort))
                   )
                {
                    pValueUnion->pDef = &paOptions[i]; /* in case of error. */

                    /*
                     * Find the argument value
                     */
                    const char *pszValue;
                    if (    fShort
                        ?       pszArgThis[2] == '\0'
                            || ((pszArgThis[2] == ':' || pszArgThis[2] == '=') && pszArgThis[3] == '\0')
                        :   pszArgThis[cchLong] == '\0' || pszArgThis[cchLong + 1] == '\0')
                    {
                        if (iThis + 1 >= argc)
                            return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;
                        pszValue = argv[iThis + 1];
                        (*piThis)++;
                    }
                    else /* same argument. */
                        pszValue = fShort
                                 ? &pszArgThis[2  + (pszArgThis[2] == ':' || pszArgThis[2] == '=')]
                                 : &pszArgThis[cchLong + 1];

                    /*
                     * Transform into a option value as requested.
                     * If decimal conversion fails, we'll check for "0x<xdigit>" and
                     * try a 16 based conversion. We will not interpret any of the
                     * generic ints as octals.
                     */
                    switch (paOptions[i].fFlags & (RTGETOPT_REQ_MASK | RTGETOPT_FLAG_HEX | RTGETOPT_FLAG_OCT | RTGETOPT_FLAG_DEC))
                    {
                        case RTGETOPT_REQ_STRING:
                            pValueUnion->psz = pszValue;
                            break;

#define MY_INT_CASE(req,type,memb,convfn) \
                        case req: \
                        { \
                            type Value; \
                            if (    convfn(pszValue, 10, &Value) != VINF_SUCCESS \
                                &&  (   pszValue[0] != '0' \
                                     || (pszValue[1] != 'x' && pszValue[1] != 'X') \
                                     || !RT_C_IS_XDIGIT(pszValue[2]) \
                                     || convfn(pszValue, 16, &Value) != VINF_SUCCESS ) ) \
                                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT; \
                            pValueUnion->memb = Value; \
                            break; \
                        }
#define MY_BASE_INT_CASE(req,type,memb,convfn,base) \
                        case req: \
                        { \
                            type Value; \
                            if (convfn(pszValue, base, &Value) != VINF_SUCCESS) \
                                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT; \
                            pValueUnion->memb = Value; \
                            break; \
                        }

                        MY_INT_CASE(RTGETOPT_REQ_INT8,   int8_t,   i,   RTStrToInt8Full)
                        MY_INT_CASE(RTGETOPT_REQ_INT16,  int16_t,  i,   RTStrToInt16Full)
                        MY_INT_CASE(RTGETOPT_REQ_INT32,  int32_t,  i,   RTStrToInt32Full)
                        MY_INT_CASE(RTGETOPT_REQ_INT64,  int64_t,  i,   RTStrToInt64Full)
                        MY_INT_CASE(RTGETOPT_REQ_UINT8,  uint8_t,  u,   RTStrToUInt8Full)
                        MY_INT_CASE(RTGETOPT_REQ_UINT16, uint16_t, u,   RTStrToUInt16Full)
                        MY_INT_CASE(RTGETOPT_REQ_UINT32, uint32_t, u,   RTStrToUInt32Full)
                        MY_INT_CASE(RTGETOPT_REQ_UINT64, uint64_t, u,   RTStrToUInt64Full)

                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT8   | RTGETOPT_FLAG_HEX, int8_t,   i,   RTStrToInt8Full,   16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT16  | RTGETOPT_FLAG_HEX, int16_t,  i,   RTStrToInt16Full,  16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT32  | RTGETOPT_FLAG_HEX, int32_t,  i,   RTStrToInt32Full,  16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT64  | RTGETOPT_FLAG_HEX, int64_t,  i,   RTStrToInt64Full,  16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT8  | RTGETOPT_FLAG_HEX, uint8_t,  u,   RTStrToUInt8Full,  16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT16 | RTGETOPT_FLAG_HEX, uint16_t, u,   RTStrToUInt16Full, 16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_HEX, uint32_t, u,   RTStrToUInt32Full, 16)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX, uint64_t, u,   RTStrToUInt64Full, 16)

                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT8   | RTGETOPT_FLAG_DEC, int8_t,   i,   RTStrToInt8Full,   10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT16  | RTGETOPT_FLAG_DEC, int16_t,  i,   RTStrToInt16Full,  10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT32  | RTGETOPT_FLAG_DEC, int32_t,  i,   RTStrToInt32Full,  10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT64  | RTGETOPT_FLAG_DEC, int64_t,  i,   RTStrToInt64Full,  10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT8  | RTGETOPT_FLAG_DEC, uint8_t,  u,   RTStrToUInt8Full,  10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT16 | RTGETOPT_FLAG_DEC, uint16_t, u,   RTStrToUInt16Full, 10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_DEC, uint32_t, u,   RTStrToUInt32Full, 10)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_DEC, uint64_t, u,   RTStrToUInt64Full, 10)

                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT8   | RTGETOPT_FLAG_OCT, int8_t,   i,   RTStrToInt8Full,   8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT16  | RTGETOPT_FLAG_OCT, int16_t,  i,   RTStrToInt16Full,  8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT32  | RTGETOPT_FLAG_OCT, int32_t,  i,   RTStrToInt32Full,  8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_INT64  | RTGETOPT_FLAG_OCT, int64_t,  i,   RTStrToInt64Full,  8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT8  | RTGETOPT_FLAG_OCT, uint8_t,  u,   RTStrToUInt8Full,  8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT16 | RTGETOPT_FLAG_OCT, uint16_t, u,   RTStrToUInt16Full, 8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_OCT, uint32_t, u,   RTStrToUInt32Full, 8)
                        MY_BASE_INT_CASE(RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_OCT, uint64_t, u,   RTStrToUInt64Full, 8)
#undef MY_INT_CASE
#undef MY_BASE_INT_CASE

                        default:
                            AssertMsgFailed(("i=%d f=%#x\n", i, paOptions[i].fFlags));
                            return VERR_INTERNAL_ERROR;
                    }
                    return paOptions[i].iShort;
                }
            }
            else if (   (   paOptions[i].pszLong
                         && !strcmp(pszArgThis, paOptions[i].pszLong))
                     || (   pszArgThis[1] == paOptions[i].iShort
                         && pszArgThis[2] == '\0') /** @todo implement support for ls -lsR like stuff?  */
                    )
            {
                pValueUnion->pDef = &paOptions[i];
                return paOptions[i].iShort;
            }
        }


        return VERR_GETOPT_UNKNOWN_OPTION;
    }

    /*
     * Not an option.
     */
    (*piThis)--;
    /** @todo Sort options and arguments (i.e. stuff that doesn't start with '-'), stop when
     * encountering the first argument. */

    return 0;
}

