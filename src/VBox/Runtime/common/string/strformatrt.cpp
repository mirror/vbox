/* $Id$ */
/** @file
 * innotek Portable Runtime - IPRT String Formatter Extensions.
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

/** @page pg_rt_str_format_rt   The IPRT String Format Extensions
 *
 * The string formatter supports most of the non-float format types and flags.
 * See RTStrFormatV() for the full tail there. In addition we've added a number
 * of iprt specific format types for the iprt typedefs and other useful stuff.
 * Note that several of these are similar to \%p and doesn't care much if you try
 * add formating flags/width/precision.
 *
 *
 * Group 1, the basic runtime typedefs (excluding those which obviously are pointer).
 *      - \%RTbool          - Takes a bool value and prints 'true', 'false', or '!%d!'.
 *      - \%RTfile          - Takes a #RTFILE value.
 *      - \%RTfmode         - Takes a #RTFMODE value.
 *      - \%RTfoff          - Takes a #RTFOFF value.
 *      - \%RTfp16          - Takes a #RTFAR16 value.
 *      - \%RTfp32          - Takes a #RTFAR32 value.
 *      - \%RTfp64          - Takes a #RTFAR64 value.
 *      - \%RTgid           - Takes a #RTGID value.
 *      - \%RTino           - Takes a #RTINODE value.
 *      - \%RTint           - Takes a #RTINT value.
 *      - \%RTiop           - Takes a #RTIOPORT value.
 *      - \%RTldrm          - Takes a #RTLDRMOD value.
 *      - \%RTnthrd         - Takes a #RTNATIVETHREAD value.
 *      - \%RTproc          - Takes a #RTPROCESS value.
 *      - \%RTptr           - Takes a #RTINTPTR or #RTUINTPTR value (but not void *).
 *      - \%RTreg           - Takes a #RTUINTREG value.
 *      - \%RTsel           - Takes a #RTSEL value.
 *      - \%RTsem           - Takes a #RTSEMEVENT, #RTSEMEVENTMULTI, #RTSEMMUTEX, #RTSEMFASTMUTEX, or #RTSEMRW value.
 *      - \%RTsock          - Takes a #RTSOCKET value.
 *      - \%RTthrd          - Takes a #RTTHREAD value.
 *      - \%RTuid           - Takes a #RTUID value.
 *      - \%RTuint          - Takes a #RTUINT value.
 *      - \%RTunicp         - Takes a #RTUNICP value.
 *      - \%RTutf16         - Takes a #RTUTF16 value.
 *      - \%RTuuid          - Takes a #PCRTUUID and will print the UUID as a string.
 *      - \%RTxuint         - Takes a #RTUINT or #RTINT value, formatting it as hex.
 *      - \%RGi             - Takes a #RTGCINT value.
 *      - \%RGp             - Takes a #RTGCPHYS value.
 *      - \%RGr             - Takes a #RTGCUINTREG value.
 *      - \%RGu             - Takes a #RTGCUINT value.
 *      - \%RGv             - Takes a #RTGCPTR, #RTGCINTPTR or #RTGCUINTPTR value.
 *      - \%RGx             - Takes a #RTGCUINT or #RTGCINT value, formatting it as hex.
 *      - \%RHi             - Takes a #RTHCINT value.
 *      - \%RHp             - Takes a #RTHCPHYS value.
 *      - \%RHr             - Takes a #RTHCUINTREG value.
 *      - \%RHu             - Takes a #RTHCUINT value.
 *      - \%RHv             - Takes a #RTHCPTR, #RTHCINTPTR or #RTHCUINTPTR value.
 *      - \%RHx             - Takes a #RTHCUINT or #RTHCINT value, formatting it as hex.
 *      - \%RCi             - Takes a #RTCCINT value.
 *      - \%RCp             - Takes a #RTCCPHYS value.
 *      - \%RCr             - Takes a #RTCCUINTREG value.
 *      - \%RCu             - Takes a #RTUINT value.
 *      - \%RCv             - Takes a #uintptr_t, #intptr_t, void * value.
 *      - \%RCx             - Takes a #RTUINT or #RTINT value, formatting it as hex.
 *
 *
 * Group 2, the generic integer types which are prefered over relying on what
 * bit-count a 'long', 'short',  or 'long long' has on a platform. This are
 * highly prefered for the [u]intXX_t kind of types.
 *      - \%RI[8|16|32|64]  - Signed integer value of the specifed bit count.
 *      - \%RU[8|16|32|64]  - Unsigned integer value of the specifed bit count.
 *      - \%RX[8|16|32|64]  - Hexadecimal integer value of the specifed bit count.
 *
 *
 * Group 3, hex dumpers and other complex stuff which requires more than simple formatting.
 *      - \%Rhxd            - Takes a pointer to the memory which is to be dumped in typical
 *                            hex format. Use the width to specify the length, and the precision to
 *                            set the number of bytes per line. Default width and precision is 16.
 *      - \%Rhxs            - Takes a pointer to the memory to be displayed as a hex string,
 *                            i.e. a series of space separated bytes formatted as two digit hex value.
 *                            Use the width to specify the length. Default length is 16 bytes.
 *      - \%Rrc             - Takes an integer iprt status code as argument. Will insert the
 *                            status code define corresponding to the iprt status code.
 *      - \%Rrs             - Takes an integer iprt status code as argument. Will insert the
 *                            short description of the specified status code.
 *      - \%Rrf             - Takes an integer iprt status code as argument. Will insert the
 *                            full description of the specified status code.
 *      - \%Rra             - Takes an integer iprt status code as argument. Will insert the
 *                            status code define + full description.
 *      - \%Rt              - Current thread (RTThreadSelf()), no arguments.
 *
 *      - \%Rwc             - Takes a long Windows error code as argument. Will insert the status
 *                            code define corresponding to the Windows error code.
 *      - \%Rwf             - Takes a long Windows error code as argument. Will insert the
 *                            full description of the specified status code.
 *      - \%Rwa             - Takes a long Windows error code as argument. Will insert the
 *                            error code define + full description.
 *
 * On other platforms, \%Rw? simply prints the argument in a form of 0xXXXXXXXX.
 *
 * @todo (r=dmik) Add a cross-platform \%Rcomr? that will fall back to \%Rw? on
 * Win32 platforms and will interpret XPCOM result codes on all other.
 *
 *
 * Group 4, structure dumpers.
 *
 *      - \%RDtimespec      - Takes a PCRTTIMESPEC.
 *
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_STRING
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#ifdef IN_RING3
# include <iprt/thread.h>
# include <iprt/err.h>
#endif
#include <iprt/time.h>
#include "internal/string.h"



/**
 * Callback to format iprt formatting extentions.
 * See @ref pg_rt_str_format_rt for a reference on the format types.
 *
 * @returns The number of bytes formatted.
 * @param   pfnOutput       Pointer to output function.
 * @param   pvArgOutput     Argument for the output function.
 * @param   ppszFormat      Pointer to the format string pointer. Advance this till the char
 *                          after the format specifier.
 * @param   pArgs           Pointer to the argument list. Use this to fetch the arguments.
 * @param   cchWidth        Format Width. -1 if not specified.
 * @param   cchPrecision    Format Precision. -1 if not specified.
 * @param   fFlags          Flags (RTSTR_NTFS_*).
 * @param   chArgSize       The argument size specifier, 'l' or 'L'.
 */
size_t rtstrFormatRt(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, const char **ppszFormat, va_list *pArgs, int cchWidth, int cchPrecision, unsigned fFlags, char chArgSize)
{
    const char *pszFormatOrg = *ppszFormat;
    char ch = *(*ppszFormat)++;
    if (ch == 'R')
    {
        ch = *(*ppszFormat)++;
        switch (ch)
        {
            /*
             * Groups 1 and 2.
             */
            case 'T':
            case 'G':
            case 'H':
            case 'C':
            case 'I':
            case 'X':
            case 'U':
            {
                /*
                 * Interpret the type.
                 */
                typedef enum { RTSF_INT, RTSF_INTW, RTSF_FP16, RTSF_FP32, RTSF_FP64, RTSF_UUID, RTSF_BOOL } RTSF;
                static const struct
                {
                    uint8_t     cch;        /**< the length of the string. */
                    char        sz[10];     /**< the part following 'R'. */
                    uint8_t     cb;         /**< the size of the type. */
                    uint8_t     u8Base;     /**< the size of the type. */
                    RTSF        enmFormat;  /**< The way to format it. */
                    uint16_t    fFlags;     /**< additional RTSTR_F_* flags. */
                }
                /** Sorted array of types, looked up using binary search! */
                s_aTypes[] =
                {
#define STRMEM(str) sizeof(str) - 1, str
                    { STRMEM("Ci"),      sizeof(RTINT),          10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Cp"),      sizeof(RTGCPHYS),       16, RTSF_INTW,  0 },
                    { STRMEM("Cr"),      sizeof(RTCCUINTREG),    16, RTSF_INTW,  0 },
                    { STRMEM("Cu"),      sizeof(RTUINT),         10, RTSF_INT,   0 },
                    { STRMEM("Cv"),      sizeof(void *),         16, RTSF_INTW,  0 },
                    { STRMEM("Cx"),      sizeof(RTUINT),         16, RTSF_INT,   0 },
                    { STRMEM("Gi"),      sizeof(RTGCINT),        10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Gp"),      sizeof(RTGCPHYS),       16, RTSF_INTW,  0 },
                    { STRMEM("Gr"),      sizeof(RTGCUINTREG),    16, RTSF_INTW,  0 },
                    { STRMEM("Gu"),      sizeof(RTGCUINT),       10, RTSF_INT,   0 },
                    { STRMEM("Gv"),      sizeof(RTGCPTR),        16, RTSF_INTW,  0 },
                    { STRMEM("Gx"),      sizeof(RTGCUINT),       16, RTSF_INT,   0 },
                    { STRMEM("Hi"),      sizeof(RTHCINT),        10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Hp"),      sizeof(RTHCPHYS),       16, RTSF_INTW,  0 },
                    { STRMEM("Hr"),      sizeof(RTGCUINTREG),    16, RTSF_INTW,  0 },
                    { STRMEM("Hu"),      sizeof(RTHCUINT),       10, RTSF_INT,   0 },
                    { STRMEM("Hv"),      sizeof(RTHCPTR),        16, RTSF_INTW,  0 },
                    { STRMEM("Hx"),      sizeof(RTHCUINT),       16, RTSF_INT,   0 },
                    { STRMEM("I16"),     sizeof(int16_t),        10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("I32"),     sizeof(int32_t),        10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("I64"),     sizeof(int64_t),        10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("I8"),      sizeof(int8_t),         10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Tbool"),   sizeof(bool),           10, RTSF_BOOL,  0 },
                    { STRMEM("Tfile"),   sizeof(RTFILE),         10, RTSF_INT,   0 },
                    { STRMEM("Tfmode"),  sizeof(RTFMODE),        16, RTSF_INTW,  0 },
                    { STRMEM("Tfoff"),   sizeof(RTFOFF),         10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Tfp16"),   sizeof(RTFAR16),        16, RTSF_FP16,  RTSTR_F_ZEROPAD },
                    { STRMEM("Tfp32"),   sizeof(RTFAR32),        16, RTSF_FP32,  RTSTR_F_ZEROPAD },
                    { STRMEM("Tfp64"),   sizeof(RTFAR64),        16, RTSF_FP64,  RTSTR_F_ZEROPAD },
                    { STRMEM("Tgid"),    sizeof(RTGID),          10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Tino"),    sizeof(RTINODE),        16, RTSF_INTW,  0 },
                    { STRMEM("Tint"),    sizeof(RTINT),          10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Tiop"),    sizeof(RTIOPORT),       16, RTSF_INTW,  0 },
                    { STRMEM("Tldrm"),   sizeof(RTLDRMOD),       16, RTSF_INTW,  0 },
                    { STRMEM("Tnthrd"),  sizeof(RTNATIVETHREAD), 16, RTSF_INTW,  0 },
                    { STRMEM("Tproc"),   sizeof(RTPROCESS),      16, RTSF_INTW,  0 },
                    { STRMEM("Tptr"),    sizeof(RTUINTPTR),      16, RTSF_INTW,  0 },
                    { STRMEM("Treg"),    sizeof(RTUINTREG),      16, RTSF_INTW,  0 },
                    { STRMEM("Tsel"),    sizeof(RTSEL),          16, RTSF_INTW,  0 },
                    { STRMEM("Tsem"),    sizeof(RTSEMEVENT),     16, RTSF_INTW,  0 },
                    { STRMEM("Tsock"),   sizeof(RTSOCKET),       10, RTSF_INT,   0 },
                    { STRMEM("Tthrd"),   sizeof(RTTHREAD),       16, RTSF_INTW,  0 },
                    { STRMEM("Tuid"),    sizeof(RTUID),          10, RTSF_INT,   RTSTR_F_VALSIGNED },
                    { STRMEM("Tuint"),   sizeof(RTUINT),         10, RTSF_INT,   0 },
                    { STRMEM("Tunicp"),  sizeof(RTUNICP),        16, RTSF_INTW,  RTSTR_F_ZEROPAD },
                    { STRMEM("Tutf16"),  sizeof(RTUTF16),        16, RTSF_INTW,  RTSTR_F_ZEROPAD },
                    { STRMEM("Tuuid"),   sizeof(PCRTUUID),       16, RTSF_UUID,  0 },
                    { STRMEM("Txint"),   sizeof(RTUINT),         16, RTSF_INT,   0 },
                    { STRMEM("U16"),     sizeof(uint16_t),       10, RTSF_INT,   0 },
                    { STRMEM("U32"),     sizeof(uint32_t),       10, RTSF_INT,   0 },
                    { STRMEM("U64"),     sizeof(uint64_t),       10, RTSF_INT,   0 },
                    { STRMEM("U8"),      sizeof(uint8_t),        10, RTSF_INT,   0 },
                    { STRMEM("X16"),     sizeof(uint16_t),       16, RTSF_INT,   0 },
                    { STRMEM("X32"),     sizeof(uint32_t),       16, RTSF_INT,   0 },
                    { STRMEM("X64"),     sizeof(uint64_t),       16, RTSF_INT,   0 },
                    { STRMEM("X8"),      sizeof(uint8_t),        16, RTSF_INT,   0 },
#undef STRMEM
                };
                const char *pszType = *ppszFormat - 1;
                int         iStart  = 0;
                int         iEnd    = ELEMENTS(s_aTypes) - 1;
                int         i       = ELEMENTS(s_aTypes) / 2;

                union
                {
                    uint8_t     u8;
                    uint16_t    u16;
                    uint32_t    u32;
                    uint64_t    u64;
                    int8_t      i8;
                    int16_t     i16;
                    int32_t     i32;
                    int64_t     i64;
                    RTFAR16     fp16;
                    RTFAR32     fp32;
                    RTFAR64     fp64;
                    bool        fBool;
                    PCRTUUID    pUuid;
                } u;
                char        szBuf[80];
                unsigned    cch;

                AssertMsg(!chArgSize, ("Not argument size '%c' for RT types! '%.10s'\n", chArgSize, pszFormatOrg));

                /*
                 * Lookup the type - binary search.
                 */
                for (;;)
                {
                    int iDiff = strncmp(pszType, s_aTypes[i].sz, s_aTypes[i].cch);
                    if (!iDiff)
                        break;
                    if (iEnd == iStart)
                    {
                        AssertMsgFailed(("Invalid format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                    }
                    if (iDiff < 0)
                        iEnd = i - 1;
                    else
                        iStart = i + 1;
                    if (iEnd < iStart)
                    {
                        AssertMsgFailed(("Invalid format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                    }
                    i = iStart + (iEnd - iStart) / 2;
                }

                /*
                 * Advance the format string and merge flags.
                 */
                *ppszFormat += s_aTypes[i].cch - 1;
                fFlags |= s_aTypes[i].fFlags;

                /*
                 * Fetch the argument.
                 * It's important that a signed value gets sign-extended up to 64-bit.
                 */
                u.u64 = 0;
                if (fFlags & RTSTR_F_VALSIGNED)
                {
                    switch (s_aTypes[i].cb)
                    {
                        case sizeof(int8_t):
                            u.i64 = va_arg(*pArgs, /*int8_t*/int);
                            fFlags |= RTSTR_F_8BIT;
                            break;
                        case sizeof(int16_t):
                            u.i64 = va_arg(*pArgs, /*int16_t*/int);
                            fFlags |= RTSTR_F_16BIT;
                            break;
                        case sizeof(int32_t):
                            u.i64 = va_arg(*pArgs, int32_t);
                            fFlags |= RTSTR_F_32BIT;
                            break;
                        case sizeof(int64_t):
                            u.i64 = va_arg(*pArgs, int64_t);
                            fFlags |= RTSTR_F_64BIT;
                            break;
                        default:
                            AssertMsgFailed(("Invalid format error, size %d'!\n", s_aTypes[i].cb));
                            break;
                    }
                }
                else
                {
                    switch (s_aTypes[i].cb)
                    {
                        case sizeof(uint8_t):
                            u.u8 = va_arg(*pArgs, /*uint8_t*/unsigned);
                            fFlags |= RTSTR_F_8BIT;
                            break;
                        case sizeof(uint16_t):
                            u.u16 = va_arg(*pArgs, /*uint16_t*/unsigned);
                            fFlags |= RTSTR_F_16BIT;
                            break;
                        case sizeof(uint32_t):
                            u.u32 = va_arg(*pArgs, uint32_t);
                            fFlags |= RTSTR_F_32BIT;
                            break;
                        case sizeof(uint64_t):
                            u.u64 = va_arg(*pArgs, uint64_t);
                            fFlags |= RTSTR_F_64BIT;
                            break;
                        case sizeof(RTFAR32):
                            u.fp32 = va_arg(*pArgs, RTFAR32);
                            break;
                        case sizeof(RTFAR64):
                            u.fp64 = va_arg(*pArgs, RTFAR64);
                            break;
                        default:
                            AssertMsgFailed(("Invalid format error, size %d'!\n", s_aTypes[i].cb));
                            break;
                    }
                }

                /*
                 * Format the output.
                 */
                switch (s_aTypes[i].enmFormat)
                {
                    case RTSF_INT:
                    {
                        cch = RTStrFormatNumber(szBuf, u.u64, s_aTypes[i].u8Base, cchWidth, cchPrecision, fFlags);
                        break;
                    }

                    /* hex which defaults to max width. */
                    case RTSF_INTW:
                    {
                        Assert(s_aTypes[i].u8Base == 16);
                        if (cchWidth < 0)
                        {
                            cchWidth = s_aTypes[i].cb * 2 + (fFlags & RTSTR_F_SPECIAL ? 2 : 0);
                            fFlags |= RTSTR_F_ZEROPAD;
                        }
                        cch = RTStrFormatNumber(szBuf, u.u64, s_aTypes[i].u8Base, cchWidth, cchPrecision, fFlags);
                        break;
                    }

                    case RTSF_FP16:
                    {
                        fFlags &= ~(RTSTR_F_VALSIGNED | RTSTR_F_BIT_MASK | RTSTR_F_WIDTH | RTSTR_F_PRECISION);
                        cch = RTStrFormatNumber(&szBuf[0], u.fp16.sel, 16, 4, -1, fFlags | RTSTR_F_16BIT);
                        Assert(cch == 4);
                        szBuf[4] = ':';
                        cch = RTStrFormatNumber(&szBuf[5], u.fp16.off, 16, 4, -1, fFlags | RTSTR_F_16BIT);
                        Assert(cch == 4);
                        cch = 4 + 1 + 4;
                        break;
                    }
                    case RTSF_FP32:
                    {
                        fFlags &= ~(RTSTR_F_VALSIGNED | RTSTR_F_BIT_MASK | RTSTR_F_WIDTH | RTSTR_F_PRECISION);
                        cch = RTStrFormatNumber(&szBuf[0], u.fp32.sel, 16, 4, -1, fFlags | RTSTR_F_16BIT);
                        Assert(cch == 4);
                        szBuf[4] = ':';
                        cch = RTStrFormatNumber(&szBuf[5], u.fp32.off, 16, 8, -1, fFlags | RTSTR_F_32BIT);
                        Assert(cch == 8);
                        cch = 4 + 1 + 8;
                        break;
                    }
                    case RTSF_FP64:
                    {
                        fFlags &= ~(RTSTR_F_VALSIGNED | RTSTR_F_BIT_MASK | RTSTR_F_WIDTH | RTSTR_F_PRECISION);
                        cch = RTStrFormatNumber(&szBuf[0], u.fp64.sel, 16, 4, -1, fFlags | RTSTR_F_16BIT);
                        Assert(cch == 4);
                        szBuf[4] = ':';
                        cch = RTStrFormatNumber(&szBuf[5], u.fp64.off, 16, 16, -1, fFlags | RTSTR_F_64BIT);
                        Assert(cch == 16);
                        cch = 4 + 1 + 16;
                        break;
                    }

                    case RTSF_UUID:
                    {
                        static const char szNull[] = "<NULL>";

                        if (VALID_PTR(u.pUuid))
                        {
                            /* cannot call RTUuidToStr because of GC/R0. */
                            return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                                               "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                                               u.pUuid->Gen.u32TimeLow,
                                               u.pUuid->Gen.u16TimeMid,
                                               u.pUuid->Gen.u16TimeHiAndVersion,
                                               u.pUuid->Gen.u16ClockSeq & 0xff,
                                               u.pUuid->Gen.u16ClockSeq >> 8,
                                               u.pUuid->Gen.au8Node[0],
                                               u.pUuid->Gen.au8Node[1],
                                               u.pUuid->Gen.au8Node[2],
                                               u.pUuid->Gen.au8Node[3],
                                               u.pUuid->Gen.au8Node[4],
                                               u.pUuid->Gen.au8Node[5]);
                        }
                        return pfnOutput(pvArgOutput, szNull, sizeof(szNull) - 1);
                    }

                    case RTSF_BOOL:
                    {
                        static const char szTrue[]  = "true ";
                        static const char szFalse[] = "false";
                        if (u.u64 == 1)
                            return pfnOutput(pvArgOutput, szTrue, sizeof(szTrue) - 1);
                        if (u.u64 == 0)
                            return pfnOutput(pvArgOutput, szFalse, sizeof(szFalse) - 1);
                        /* invalid boolean value */
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "!%lld!", u.u64);
                    }

                    default:
                        AssertMsgFailed(("Internal error %d\n", s_aTypes[i].enmFormat));
                        return 0;
                }

                /*
                 * Finally, output the formatted string and return.
                 */
                return pfnOutput(pvArgOutput, szBuf, cch);
            }


            /* Group 3 */

            /*
             * hex dumping.
             */
            case 'h':
            {
                char ch = *(*ppszFormat)++;
                switch (ch)
                {
                    /*
                     * Hex stuff.
                     */
                    case 'x':
                    {
                        uint8_t *pu8 = va_arg(*pArgs, uint8_t *);
                        if (cchWidth <= 0)
                            cchWidth = 16;
                        if (pu8)
                        {
                            ch = *(*ppszFormat)++;
                            switch (ch)
                            {
                                /*
                                 * Regular hex dump.
                                 */
                                case 'd':
                                {
                                    size_t cch = 0;
                                    int off = 0;

                                    if (cchPrecision <= 0)
                                        cchPrecision = 16;

                                    while (off < cchWidth)
                                    {
                                        int i;
                                        cch += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%s%0*x %04x:", off ? "\n" : "", sizeof(pu8) * 2, (uintptr_t)pu8, off);
                                        for (i = 0; i < cchPrecision && off + i < cchWidth ; i++)
                                            cch += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                                                               off + i < cchWidth ? !(i & 7) && i ? "-%02x" : " %02x" : "   ", pu8[i]);
                                        while (i++ < cchPrecision)
                                            cch += pfnOutput(pvArgOutput, "   ", 3);

                                        cch += pfnOutput(pvArgOutput, " ", 1);

                                        for (i = 0; i < cchPrecision && off + i < cchWidth; i++)
                                        {
                                            uint8_t u8 = pu8[i];
                                            cch += pfnOutput(pvArgOutput, u8 < 127 && u8 >= 32 ? (const char *)&u8 : ".", 1);
                                        }

                                        /* next */
                                        pu8 += cchPrecision;
                                        off += cchPrecision;
                                    }
                                    return cch;
                                }

                                /*
                                 * Hex string.
                                 */
                                case 's':
                                {
                                    if (cchWidth-- > 0)
                                    {
                                        size_t cch = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%02x", *pu8++);
                                        for (; cchWidth > 0; cchWidth--, pu8++)
                                            cch += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, " %02x", *pu8);
                                        return cch;
                                    }
                                    break;
                                }

                                default:
                                    AssertMsgFailed(("Invalid status code format type '%.10s'!\n", ch, pszFormatOrg));
                                    break;
                            }
                        }
                        else
                            return pfnOutput(pvArgOutput, "<null>", sizeof("<null>") - 1);
                        break;
                    }

                    default:
                        AssertMsgFailed(("Invalid status code format type '%.10s'!\n", ch, pszFormatOrg));
                        return 0;

                }
                break;
            }

            /*
             * iprt status code: %Vrc, %Vrs, %Vrf, %Vra.
             */
            case 'r':
            {
                int rc = va_arg(*pArgs, int);
                char ch = *(*ppszFormat)++;
#ifdef IN_RING3                         /* we don't want this anywhere else yet. */
                PCRTSTATUSMSG pMsg = RTErrGet(rc);
                switch (ch)
                {
                    case 'c':
                        return pfnOutput(pvArgOutput, pMsg->pszDefine,    strlen(pMsg->pszDefine));
                    case 's':
                        return pfnOutput(pvArgOutput, pMsg->pszMsgShort,  strlen(pMsg->pszMsgShort));
                    case 'f':
                        return pfnOutput(pvArgOutput, pMsg->pszMsgFull,   strlen(pMsg->pszMsgFull));
                    case 'a':
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%s (%d) - %s", pMsg->pszDefine, rc, pMsg->pszMsgFull);
                    default:
                        AssertMsgFailed(("Invalid status code format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                }
#else /* !IN_RING3 */
                switch (ch)
                {
                    case 'c':
                    case 's':
                    case 'f':
                    case 'a':
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%d", rc);
                    default:
                        AssertMsgFailed(("Invalid status code format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                }
#endif /* !IN_RING3 */
                break;
            }

#if defined(IN_RING3)
            /*
             * Windows status code: %Rwc, %Rwf, %Rwa
             */
            case 'w':
            {
                long rc = va_arg(*pArgs, long);
                char ch = *(*ppszFormat)++;
#if defined(RT_OS_WINDOWS)
                PCRTWINERRMSG pMsg = RTErrWinGet(rc);
#endif
                switch (ch)
                {
#if defined(RT_OS_WINDOWS)
                    case 'c':
                        return pfnOutput(pvArgOutput, pMsg->pszDefine, strlen(pMsg->pszDefine));
                    case 'f':
                        return pfnOutput(pvArgOutput, pMsg->pszMsgFull,strlen(pMsg->pszMsgFull));
                    case 'a':
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%s (0x%08X) - %s", pMsg->pszDefine, rc, pMsg->pszMsgFull);
#else
                    case 'c':
                    case 'f':
                    case 'a':
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "0x%08X", rc);
#endif
                    default:
                        AssertMsgFailed(("Invalid status code format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                }
                break;
            }
#endif /* IN_RING3 */

            /*
             * Group 4, structure dumpers.
             */
            case 'D':
            {
                /*
                 * Interpret the type.
                 */
                typedef enum
                {
                    RTST_TIMESPEC
                } RTST;
/** Set if it's a pointer */
#define RTST_FLAGS_POINTER  RT_BIT(0)
                static const struct
                {
                    uint8_t     cch;        /**< the length of the string. */
                    char        sz[16-2];   /**< the part following 'R'. */
                    uint8_t     cb;         /**< the size of the argument. */
                    uint8_t     fFlags;     /**< RTST_FLAGS_* */
                    RTST        enmType;    /**< The structure type. */
                }
                /** Sorted array of types, looked up using binary search! */
                s_aTypes[] =
                {
#define STRMEM(str) sizeof(str) - 1, str
                    { STRMEM("Dtimespec"),   sizeof(PCRTTIMESPEC),  RTST_FLAGS_POINTER, RTST_TIMESPEC},
#undef STRMEM
                };
                const char *pszType = *ppszFormat - 1;
                int         iStart  = 0;
                int         iEnd    = ELEMENTS(s_aTypes) - 1;
                int         i       = ELEMENTS(s_aTypes) / 2;

                union
                {
                    const void     *pv;
                    uint64_t        u64;
                    PCRTTIMESPEC    pTimeSpec;
                } u;

                AssertMsg(!chArgSize, ("Not argument size '%c' for RT types! '%.10s'\n", chArgSize, pszFormatOrg));

                /*
                 * Lookup the type - binary search.
                 */
                for (;;)
                {
                    int iDiff = strncmp(pszType, s_aTypes[i].sz, s_aTypes[i].cch);
                    if (!iDiff)
                        break;
                    if (iEnd == iStart)
                    {
                        AssertMsgFailed(("Invalid format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                    }
                    if (iDiff < 0)
                        iEnd = i - 1;
                    else
                        iStart = i + 1;
                    if (iEnd < iStart)
                    {
                        AssertMsgFailed(("Invalid format type '%.10s'!\n", pszFormatOrg));
                        return 0;
                    }
                    i = iStart + (iEnd - iStart) / 2;
                }
                *ppszFormat += s_aTypes[i].cch - 1;

                /*
                 * Fetch the argument.
                 */
                u.u64 = 0;
                switch (s_aTypes[i].cb)
                {
                    case sizeof(const void *):
                        u.pv = va_arg(*pArgs, const void *);
                        break;
                    default:
                        AssertMsgFailed(("Invalid format error, size %d'!\n", s_aTypes[i].cb));
                        break;
                }

                /*
                 * If it's a pointer, we'll check if it's valid before going on.
                 */
                if ((s_aTypes[i].fFlags & RTST_FLAGS_POINTER) && !VALID_PTR(u.pv))
                    return pfnOutput(pvArgOutput, "<null>", sizeof("<null>") - 1);

                /*
                 * Format the output.
                 */
                switch (s_aTypes[i].enmType)
                {
                    case RTST_TIMESPEC:
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, NULL, "%lld ns", RTTimeSpecGetNano(u.pTimeSpec));

                    default:
                        AssertMsgFailed(("Invalid/unhandled enmType=%d\n", s_aTypes[i].enmType));
                        break;
                }
                break;
            }

            /*
             * Invalid/Unknown. Bitch about it.
             */
            default:
                AssertMsgFailed(("Invalid VBox format type '%.10s'!\n", pszFormatOrg));
                break;
        }
    }
    else
        AssertMsgFailed(("Invalid VBox format type '%.10s'!\n", pszFormatOrg));

    NOREF(pszFormatOrg);
    return 0;
}


