/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - String To Number Conversion.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/err.h>

struct TstI64
{
    const char *psz;
    unsigned    uBase;
    int         rc;
    int64_t     Result;
};

struct TstU64
{
    const char *psz;
    unsigned    uBase;
    int         rc;
    uint64_t    Result;
};

struct TstI32
{
    const char *psz;
    unsigned    uBase;
    int         rc;
    int32_t     Result;
};

struct TstU32
{
    const char *psz;
    unsigned    uBase;
    int         rc;
    uint32_t    Result;
};


#define TEST(Test, Type, Fmt, Fun) \
    do \
    { \
        Type Result; \
        int rc = Fun(Test.psz, NULL, Test.uBase, &Result); \
        if (Result != Test.Result) \
        { \
            RTPrintf("failure: '%s' -> " Fmt " expected " Fmt ". (%s)\n", Test.psz, Result, Test.Result, #Fun); \
            cErrors++; \
        } \
        else if (rc != Test.rc) \
        { \
            RTPrintf("failure: '%s' -> rc=%Vrc expected %Vrc. (%s)\n", Test.psz, rc, Test.rc, #Fun); \
            cErrors++; \
        } \
    } while (0)


#define RUN_TESTS(aTests, Type, Fmt, Fun) \
    do \
    { \
        for (unsigned iTest = 0; iTest < ELEMENTS(aTests); iTest++) \
        { \
            TEST(aTests[iTest], Type, Fmt, Fun); \
        } \
    } while (0)

int main()
{
    int cErrors = 0;
    static const struct TstU64 aTstU64[] =
    {
        { "0",                      0,  VINF_SUCCESS,           0 },
        { "1",                      0,  VINF_SUCCESS,           1 },
        { "-1",                     0,  VWRN_NEGATIVE_UNSIGNED,  ~0ULL },
        { "0x",                     0,  VINF_SUCCESS,           0 },
        { "0x1",                    0,  VINF_SUCCESS,           1 },
        { "0x0fffffffffffffff",     0,  VINF_SUCCESS,           0x0fffffffffffffffULL },
        { "0x0ffffffffffffffffffffff",0,  VWRN_NUMBER_TOO_BIG,  0xffffffffffffffffULL },
        { "asdfasdfasdf",           0,  VERR_NO_DIGITS,         0 },
        { "0x111111111",            0,  VINF_SUCCESS,           0x111111111ULL },
    };
    RUN_TESTS(aTstU64, uint64_t, "%#llx", RTStrToUInt64Ex);

    static const struct TstI64 aTstI64[] =
    {
        { "0",                      0,  VINF_SUCCESS,           0 },
        { "1",                      0,  VINF_SUCCESS,           1 },
        { "-1",                     0,  VINF_SUCCESS,          -1 },
        { "-1",                    10,  VINF_SUCCESS,          -1 },
        { "-31",                    0,  VINF_SUCCESS,          -31 },
        { "-31",                   10,  VINF_SUCCESS,          -31 },
        { "-32",                    0,  VINF_SUCCESS,          -32 },
        { "-33",                    0,  VINF_SUCCESS,          -33 },
        { "-64",                    0,  VINF_SUCCESS,          -64 },
        { "-127",                   0,  VINF_SUCCESS,          -127 },
        { "-128",                   0,  VINF_SUCCESS,          -128 },
        { "-129",                   0,  VINF_SUCCESS,          -129 },
        { "-254",                   0,  VINF_SUCCESS,          -254 },
        { "-255",                   0,  VINF_SUCCESS,          -255 },
        { "-256",                   0,  VINF_SUCCESS,          -256 },
        { "-257",                   0,  VINF_SUCCESS,          -257 },
        { "-511",                   0,  VINF_SUCCESS,          -511 },
        { "-512",                   0,  VINF_SUCCESS,          -512 },
        { "-513",                   0,  VINF_SUCCESS,          -513 },
        { "-1023",                  0,  VINF_SUCCESS,          -1023 },
        { "-1023",                  0,  VINF_SUCCESS,          -1023 },
        { "-1023",                  0,  VINF_SUCCESS,          -1023},
        { "-1023",                 10,  VINF_SUCCESS,          -1023 },
        { "-4564678",               0,  VINF_SUCCESS,          -4564678 },
        { "-4564678",              10,  VINF_SUCCESS,          -4564678 },
        { "-1234567890123456789",   0,  VINF_SUCCESS,          -1234567890123456789LL },
        { "-1234567890123456789",  10,  VINF_SUCCESS,          -1234567890123456789LL },
        { "0x",                     0,  VINF_SUCCESS,           0 },
        { "0x1",                    0,  VINF_SUCCESS,           1 },
        { "0x1",                   10,  VINF_SUCCESS,           0 },
        { "0x1",                   16,  VINF_SUCCESS,           1 },
        { "0x0fffffffffffffff",     0,  VINF_SUCCESS,           0x0fffffffffffffffULL },
        //{ "0x01111111111111111111111",0,  VWRN_NUMBER_TOO_BIG,  0x1111111111111111ULL },
        { "0x0ffffffffffffffffffffff",0,  VWRN_NUMBER_TOO_BIG,  0xffffffffffffffffULL },
        { "asdfasdfasdf",           0,  VERR_NO_DIGITS,         0 },
        { "0x111111111",            0,  VINF_SUCCESS,           0x111111111ULL },
    };
    RUN_TESTS(aTstI64, int64_t, "%#lld", RTStrToInt64Ex);



    static const struct TstI32 aTstI32[] =
    {
        { "0",                      0,  VINF_SUCCESS,           0 },
        { "1",                      0,  VINF_SUCCESS,           1 },
        { "-1",                     0,  VINF_SUCCESS,          -1 },
        { "-1",                    10,  VINF_SUCCESS,          -1 },
        { "-31",                    0,  VINF_SUCCESS,          -31 },
        { "-31",                   10,  VINF_SUCCESS,          -31 },
        { "-32",                    0,  VINF_SUCCESS,          -32 },
        { "-33",                    0,  VINF_SUCCESS,          -33 },
        { "-64",                    0,  VINF_SUCCESS,          -64 },
        { "-127",                   0,  VINF_SUCCESS,          -127 },
        { "-128",                   0,  VINF_SUCCESS,          -128 },
        { "-129",                   0,  VINF_SUCCESS,          -129 },
        { "-254",                   0,  VINF_SUCCESS,          -254 },
        { "-255",                   0,  VINF_SUCCESS,          -255 },
        { "-256",                   0,  VINF_SUCCESS,          -256 },
        { "-257",                   0,  VINF_SUCCESS,          -257 },
        { "-511",                   0,  VINF_SUCCESS,          -511 },
        { "-512",                   0,  VINF_SUCCESS,          -512 },
        { "-513",                   0,  VINF_SUCCESS,          -513 },
        { "-1023",                  0,  VINF_SUCCESS,          -1023 },
        { "-1023",                  0,  VINF_SUCCESS,          -1023 },
        { "-1023",                  0,  VINF_SUCCESS,          -1023},
        { "-1023",                 10,  VINF_SUCCESS,          -1023 },
        { "-4564678",               0,  VINF_SUCCESS,          -4564678 },
        { "-4564678",              10,  VINF_SUCCESS,          -4564678 },
        { "4564678",                0,  VINF_SUCCESS,          4564678 },
        { "4564678",               10,  VINF_SUCCESS,          4564678 },
        { "-1234567890123456789",   0,  VWRN_NUMBER_TOO_BIG,   (int32_t)-1234567890123456789LL },
        { "-1234567890123456789",  10,  VWRN_NUMBER_TOO_BIG,   (int32_t)-1234567890123456789LL },
        { "1234567890123456789",    0,  VWRN_NUMBER_TOO_BIG,   (int32_t)1234567890123456789LL },
        { "1234567890123456789",   10,  VWRN_NUMBER_TOO_BIG,   (int32_t)1234567890123456789LL },
        { "0x",                     0,  VINF_SUCCESS,           0 },
        { "0x1",                    0,  VINF_SUCCESS,           1 },
        { "0x1",                   10,  VINF_SUCCESS,           0 },
        { "0x1",                   16,  VINF_SUCCESS,           1 },
        { "0x0fffffffffffffff",     0,  VWRN_NUMBER_TOO_BIG,           0xffffffff },
        { "0x01111111111111111111111",0,  VWRN_NUMBER_TOO_BIG,  0x11111111 },
        { "0x0ffffffffffffffffffffff",0,  VWRN_NUMBER_TOO_BIG,  0xffffffff },
        { "asdfasdfasdf",           0,  VERR_NO_DIGITS,         0 },
        { "0x1111111",              0,  VINF_SUCCESS,           0x01111111 },
    };
    RUN_TESTS(aTstI32, int32_t, "%#d", RTStrToInt32Ex);

    static const struct TstU32 aTstU32[] =
    {
        { "0",                      0,  VINF_SUCCESS,           0 },
        { "1",                      0,  VINF_SUCCESS,           1 },
        { "-1",                     0,  VWRN_NEGATIVE_UNSIGNED, ~0 },
        { "0x",                     0,  VINF_SUCCESS,           0 },
        { "0x1",                    0,  VINF_SUCCESS,           1 },
        { "0x0fffffffffffffff",     0,  VWRN_NUMBER_TOO_BIG,    0xffffffffU },
        { "0x0ffffffffffffffffffffff",0,  VWRN_NUMBER_TOO_BIG,  0xffffffffU },
        { "asdfasdfasdf",           0,  VERR_NO_DIGITS,         0 },
        { "0x1111111",              0,  VINF_SUCCESS,           0x1111111 },
    };
    RUN_TESTS(aTstU32, uint32_t, "%#x", RTStrToUInt32Ex);

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstStrToNum: SUCCESS\n");
    else
        RTPrintf("tstStrToNum: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
