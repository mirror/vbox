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
#include <iprt/net.h>                   /* must come before getopt.h */
#include <iprt/getopt.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/uuid.h>



RTDECL(int) RTGetOptInit(PRTGETOPTSTATE pState, int argc, char **argv,
                         PCRTGETOPTDEF paOptions, size_t cOptions,
                         int iFirst, uint32_t fFlags)
{
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

    pState->argv         = argv;
    pState->argc         = argc;
    pState->paOptions    = paOptions;
    pState->cOptions     = cOptions;
    pState->iNext        = iFirst;
    pState->pszNextShort = NULL;
    pState->pDef         = NULL;

    /* validate the options. */
    for (size_t i = 0; i < cOptions; i++)
    {
        Assert(!(paOptions[i].fFlags & ~RTGETOPT_VALID_MASK));
        Assert(paOptions[i].iShort > 0);
        Assert(paOptions[i].iShort != VINF_GETOPT_NOT_OPTION);
        Assert(paOptions[i].iShort != '-');
    }

    /** @todo Add an flag for sorting the arguments so that all the options comes
     *        first. */
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTGetOptInit);


/**
 * Converts an stringified IPv4 address into the RTNETADDRIPV4 representation.
 *
 * @todo This should be move to some generic part of the runtime.
 *
 * @returns VINF_SUCCESS on success, VERR_GETOPT_INVALID_ARGUMENT_FORMAT on
 *          failure.
 *
 * @param   pszValue        The value to convert.
 * @param   pAddr           Where to store the result.
 */
static int rtgetoptConvertIPv4Addr(const char *pszValue, PRTNETADDRIPV4 pAddr)
{
    char *pszNext;
    int rc = RTStrToUInt8Ex(RTStrStripL(pszValue), &pszNext, 10, &pAddr->au8[0]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    if (*pszNext++ != '.')
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[1]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    if (*pszNext++ != '.')
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[2]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    if (*pszNext++ != '.')
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[3]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_SPACES)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    pszNext = RTStrStripL(pszNext);
    if (*pszNext)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

    return VINF_SUCCESS;
}


/**
 * Converts an stringified Ethernet MAC address into the RTMAC representation.
 *
 * @todo This should be move to some generic part of the runtime.
 *
 * @returns VINF_SUCCESS on success, VERR_GETOPT_INVALID_ARGUMENT_FORMAT on
 *          failure.
 *
 * @param   pszValue        The value to convert.
 * @param   pAddr           Where to store the result.
 */
static int rtgetoptConvertMacAddr(const char *pszValue, PRTMAC pAddr)
{
    /*
     * Not quite sure if I should accept stuff like "08::27:::1" here...
     * The code is accepting "::" patterns now, except for for the first
     * and last parts.
     */

    /* first */
    char *pszNext;
    int rc = RTStrToUInt8Ex(RTStrStripL(pszValue), &pszNext, 16, &pAddr->au8[0]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    if (*pszNext++ != ':')
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

    /* middle */
    for (unsigned i = 1; i < 5; i++)
    {
        if (*pszNext == ':')
            pAddr->au8[i] = 0;
        else
        {
            rc = RTStrToUInt8Ex(pszNext, &pszNext, 16, &pAddr->au8[i]);
            if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
            if (*pszNext != ':')
                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
        }
        pszNext++;
    }

    /* last */
    rc = RTStrToUInt8Ex(pszNext, &pszNext, 16, &pAddr->au8[5]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_SPACES)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
    pszNext = RTStrStripL(pszNext);
    if (*pszNext)
        return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;

    return VINF_SUCCESS;
}


/**
 * Searches for a long option.
 *
 * @returns Pointer to a matching option.
 * @param   pszOption       The alleged long option.
 * @param   paOptions       Option array.
 * @param   cOptions        Number of items in the array.
 */
static PCRTGETOPTDEF rtGetOptSearchLong(const char *pszOption, PCRTGETOPTDEF paOptions, size_t cOptions)
{
    PCRTGETOPTDEF pOpt = paOptions;
    while (cOptions-- > 0)
    {
        if (pOpt->pszLong)
        {
            if ((pOpt->fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING)
            {
                /*
                 * A value is required with the argument. We're trying to be very
                 * understanding here and will permit any of the following:
                 *      --long12:value,  --long12=value, --long12 value,
                 *      --long:value,    --long=value,   --long value,
                 *      --long: value,   --long= value
                 *
                 * If the option is index, then all trailing chars must be
                 * digits.  For error reporting reasons we also match where
                 * there is no index.
                 */
                size_t cchLong = strlen(pOpt->pszLong);
                if (!strncmp(pszOption, pOpt->pszLong, cchLong))
                {
                    if (pOpt->fFlags & RTGETOPT_FLAG_INDEX)
                        while (RT_C_IS_DIGIT(pszOption[cchLong]))
                            cchLong++;
                    if (   pszOption[cchLong] == '\0'
                        || pszOption[cchLong] == ':'
                        || pszOption[cchLong] == '=')
                        return pOpt;
                }
            }
            else if (pOpt->fFlags & RTGETOPT_FLAG_INDEX)
            {
                /*
                 * The option takes an index but no value.
                 * As above, we also match where there is no index.
                 */
                size_t cchLong = strlen(pOpt->pszLong);
                if (!strncmp(pszOption, pOpt->pszLong, cchLong))
                {
                    while (RT_C_IS_DIGIT(pszOption[cchLong]))
                        cchLong++;
                    if (pszOption[cchLong] == '\0')
                        return pOpt;
                }
            }
            else if (!strcmp(pszOption, pOpt->pszLong))
                return pOpt;
        }
        pOpt++;
    }
    return NULL;
}


/**
 * Searches for a matching short option.
 *
 * @returns Pointer to a matching option.
 * @param   chOption        The option char.
 * @param   paOptions       Option array.
 * @param   cOptions        Number of items in the array.
 */
static PCRTGETOPTDEF rtGetOptSearchShort(int chOption, PCRTGETOPTDEF paOptions, size_t cOptions)
{
    PCRTGETOPTDEF pOpt = paOptions;
    while (cOptions-- > 0)
    {
        if (pOpt->iShort == chOption)
            return pOpt;
        pOpt++;
    }
    return NULL;
}


/**
 * Value string -> Value union.
 *
 * @returns IPRT status code.
 * @param   fFlags              The value flags.
 * @param   pszValue            The value string.
 * @param   pValueUnion         Where to return the processed value.
 */
static int rtGetOptProcessValue(uint32_t fFlags, const char *pszValue, PRTGETOPTUNION pValueUnion)
{
    /*
     * Transform into a option value as requested.
     * If decimal conversion fails, we'll check for "0x<xdigit>" and
     * try a 16 based conversion. We will not interpret any of the
     * generic ints as octals.
     */
    switch (fFlags & (  RTGETOPT_REQ_MASK
                      | RTGETOPT_FLAG_HEX
                      | RTGETOPT_FLAG_DEC
                      | RTGETOPT_FLAG_OCT))
    {
        case RTGETOPT_REQ_STRING:
            pValueUnion->psz = pszValue;
            break;

        case RTGETOPT_REQ_BOOL_ONOFF:
            if (!RTStrICmp(pszValue, "on"))
                pValueUnion->f = true;
            else if (!RTStrICmp(pszValue, "off"))
                pValueUnion->f = false;
            else
            {
                pValueUnion->psz = pszValue;
                return VERR_GETOPT_UNKNOWN_OPTION;
            }
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

        case RTGETOPT_REQ_IPV4ADDR:
        {
            RTNETADDRIPV4 Addr;
            if (rtgetoptConvertIPv4Addr(pszValue, &Addr) != VINF_SUCCESS)
                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
            pValueUnion->IPv4Addr = Addr;
            break;
        }
#if 0  /** @todo CIDR */
#endif

        case RTGETOPT_REQ_MACADDR:
        {
            RTMAC Addr;
            if (rtgetoptConvertMacAddr(pszValue, &Addr) != VINF_SUCCESS)
                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
            pValueUnion->MacAddr = Addr;
            break;
        }

        case RTGETOPT_REQ_UUID:
        {
            RTUUID Uuid;
            if (RTUuidFromStr(&Uuid, pszValue) != VINF_SUCCESS)
                return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
            pValueUnion->Uuid = Uuid;
            break;
        }

        default:
            AssertMsgFailed(("f=%#x\n", fFlags));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTGetOpt(PRTGETOPTSTATE pState, PRTGETOPTUNION pValueUnion)
{
    /*
     * Reset the variables kept in state.
     */
    pState->pDef = NULL;
    pState->uIndex = UINT64_MAX;

    /*
     * Make sure the union is completely cleared out, whatever happens below.
     */
    pValueUnion->u64 = 0;
    pValueUnion->pDef = NULL;

    /** @todo Handle '--' (end of options).*/
    /** @todo Add a flag to RTGetOptInit for handling the various help options in
     *        a common way. (-?,-h,-help,--help,++) */
    /** @todo Add a flag to RTGetOptInit for handling the standard version options
     *        in a common way. (-V,--version) */

    /*
     * The next option.
     */
    bool            fShort;
    int             iThis;
    const char     *pszArgThis;
    PCRTGETOPTDEF   pOpt;

    if (pState->pszNextShort)
    {
        /*
         * We've got short options left over from the previous call.
         */
        pOpt = rtGetOptSearchShort(*pState->pszNextShort, pState->paOptions, pState->cOptions);
        if (!pOpt)
        {
            pValueUnion->psz = pState->pszNextShort;
            return VERR_GETOPT_UNKNOWN_OPTION;
        }
        pState->pszNextShort++;
        pszArgThis = pState->pszNextShort - 2;
        iThis = pState->iNext;
        fShort = true;
    }
    else
    {
        /*
         * Pop off the next argument.
         */
        if (pState->iNext >= pState->argc)
            return 0;
        iThis = pState->iNext++;
        pszArgThis = pState->argv[iThis];

        /*
         * Do a long option search first and then a short option one.
         * This way we can make sure single dash long options doesn't
         * get mixed up with short ones.
         */
        pOpt = rtGetOptSearchLong(pszArgThis, pState->paOptions, pState->cOptions);
        if (    !pOpt
            &&  pszArgThis[0] == '-'
            &&  pszArgThis[1] != '-'
            &&  pszArgThis[1] != '\0')
        {
            pOpt = rtGetOptSearchShort(pszArgThis[1], pState->paOptions, pState->cOptions);
            fShort = pOpt != NULL;
        }
        else
            fShort = false;
    }

    if (pOpt)
    {
        pValueUnion->pDef = pOpt; /* in case of no value or error. */

        if ((pOpt->fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING)
        {
            /*
             * Find the argument value.
             *
             * A value is required with the argument. We're trying to be very
             * understanding here and will permit any of the following:
             *      -svalue, -s:value, -s=value,
             *      -s value, -s: value, -s= value
             * (Ditto for long options.)
             */
            const char *pszValue;
            if (fShort)
            {
                if (    pszArgThis[2] == '\0'
                    ||  (  pszArgThis[3] == '\0'
                         && (   pszArgThis[2] == ':'
                             || pszArgThis[2] == '=')) )
                {
                    if (iThis + 1 >= pState->argc)
                        return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;
                    pszValue = pState->argv[iThis + 1];
                    pState->iNext++;
                }
                else /* same argument. */
                    pszValue = &pszArgThis[2  + (pszArgThis[2] == ':' || pszArgThis[2] == '=')];
                if (pState->pszNextShort)
                {
                    pState->pszNextShort = NULL;
                    pState->iNext++;
                }
            }
            else
            {
                size_t cchLong = strlen(pOpt->pszLong);
                if (pOpt->fFlags & RTGETOPT_FLAG_INDEX)
                {

                    if (pszArgThis[cchLong] == '\0')
                        return VERR_GETOPT_INDEX_MISSING;

                    uint64_t uIndex;
                    char *pszRet = NULL;
                    int rc = RTStrToUInt64Ex(&pszArgThis[cchLong], &pszRet, 10, &uIndex);
                    if (rc == VWRN_TRAILING_CHARS)
                    {
                        if (   pszRet[0] != ':'
                            && pszRet[0] != '=')
                            return VERR_GETOPT_INVALID_ARGUMENT_FORMAT;
                        pState->uIndex = uIndex;
                        pszValue = pszRet + 1;
                    }
                    else if (rc == VINF_SUCCESS)
                    {
                        if (iThis + 1 >= pState->argc)
                            return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;
                        pState->uIndex = uIndex;
                        pszValue = pState->argv[iThis + 1];
                        pState->iNext++;
                    }
                    else
                        AssertMsgFailedReturn(("%s\n", pszArgThis), VERR_GETOPT_INVALID_ARGUMENT_FORMAT); /* search bug */
                }
                else
                {
                    if (    pszArgThis[cchLong]     == '\0'
                        ||  pszArgThis[cchLong + 1] == '\0')
                    {
                        if (iThis + 1 >= pState->argc)
                            return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;
                        pszValue = pState->argv[iThis + 1];
                        pState->iNext++;
                    }
                    else /* same argument. */
                        pszValue = &pszArgThis[cchLong + 1];
                }
            }

            /*
             * Set up the ValueUnion.
             */
            int rc = rtGetOptProcessValue(pOpt->fFlags, pszValue, pValueUnion);
            if (RT_FAILURE(rc))
                return rc;
        }
        else if (fShort)
        {
            /*
             * Deal with "compressed" short option lists, correcting the next
             * state variables for the start and end cases.
             */
            if (pszArgThis[2])
            {
                if (!pState->pszNextShort)
                {
                    /* start */
                    pState->pszNextShort = &pszArgThis[2];
                    pState->iNext--;
                }
            }
            else if (pState->pszNextShort)
            {
                /* end */
                pState->pszNextShort = NULL;
                pState->iNext++;
            }
        }
        else if (pOpt->fFlags & RTGETOPT_FLAG_INDEX)
        {
            size_t cchLong = strlen(pOpt->pszLong);
            if (pszArgThis[cchLong] == '\0')
                return VERR_GETOPT_INDEX_MISSING;

            uint64_t uIndex;
            char *pszRet = NULL;
            if (RTStrToUInt64Full(&pszArgThis[cchLong], 10, &uIndex) == VINF_SUCCESS)
                pState->uIndex = uIndex;
            else
                AssertMsgFailedReturn(("%s\n", pszArgThis), VERR_GETOPT_INVALID_ARGUMENT_FORMAT); /* search bug */
        }

        pState->pDef = pOpt;
        return pOpt->iShort;
    }

    /*
     * Not a known option argument. If it starts with a switch char (-) we'll
     * fail with unkown option, and if it doesn't we'll return it as a non-option.
     */

    if (*pszArgThis == '-')
    {
        pValueUnion->psz = pszArgThis;
        return VERR_GETOPT_UNKNOWN_OPTION;
    }

    pValueUnion->psz = pszArgThis;
    return VINF_GETOPT_NOT_OPTION;
}
RT_EXPORT_SYMBOL(RTGetOpt);


RTDECL(int) RTGetOptFetchValue(PRTGETOPTSTATE pState, PRTGETOPTUNION pValueUnion, uint32_t fFlags)
{
    /*
     * Validate input.
     */
    PCRTGETOPTDEF pOpt = pState->pDef;
    AssertReturn(pOpt, VERR_GETOPT_UNKNOWN_OPTION);
    AssertReturn(!(fFlags & ~RTGETOPT_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & RTGETOPT_REQ_MASK) != RTGETOPT_REQ_NOTHING, VERR_INVALID_PARAMETER);

    /*
     * Make sure the union is completely cleared out, whatever happens below.
     */
    pValueUnion->u64 = 0;
    pValueUnion->pDef = NULL;

    /*
     * Pop off the next argument and convert it into a value union.
     */
    if (pState->iNext >= pState->argc)
        return VERR_GETOPT_REQUIRED_ARGUMENT_MISSING;
    int         iThis    = pState->iNext++;
    const char *pszValue = pState->argv[iThis];
    pValueUnion->pDef    = pOpt; /* in case of no value or error. */

    return rtGetOptProcessValue(fFlags, pszValue, pValueUnion);
}
RT_EXPORT_SYMBOL(RTGetOptFetchValue);


RTDECL(int) RTGetOptPrintError(int ch, PCRTGETOPTUNION pValueUnion)
{
    if (ch == VINF_GETOPT_NOT_OPTION)
        RTMsgError("Invalid parameter: %s", pValueUnion->psz);
    else if (ch > 0)
    {
        if (RT_C_IS_GRAPH(ch))
            RTMsgError("Unhandled option: -%c", ch);
        else
            RTMsgError("Unhandled option: %i (%#x)", ch, ch);
    }
    else if (ch == VERR_GETOPT_UNKNOWN_OPTION)
        RTMsgError("Unknown option: '%s'", pValueUnion->psz);
    else if (pValueUnion->pDef)
        RTMsgError("%s: %Rrs\n", pValueUnion->pDef->pszLong, ch);
    else
        RTMsgError("%Rrs\n", ch);

    return 2; /** @todo add defines for EXIT_SUCCESS, EXIT_FAILURE, EXIT_INVAL, etc... */
}
RT_EXPORT_SYMBOL(RTGetOptPrintError);

