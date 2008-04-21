/* $Id$ */
/** @file
 * IPRT - VBox String Formatter extensions.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/** @page pg_rt_str_format_vbox    The VBox String Format Extensions
 *
 * The string formatter supports most of the non-float format types and flags.
 * See RTStrFormatV() for the full tail there. In addition we've added a number
 * of VBox specific format types. Most of these doesn't work with any flags (like
 * width, precision and such), but they should still be very useful.
 *
 * The types:
 *
 *      - \%Vrc  - Takes an integer iprt status code as argument. Will insert the
 *        status code define corresponding to the iprt status code.
 *      - \%Vrs  - Takes an integer iprt status code as argument. Will insert the
 *        short description of the specified status code.
 *      - \%Vrf  - Takes an integer iprt status code as argument. Will insert the
 *        full description of the specified status code.
 *      - \%Vra  - Takes an integer iprt status code as argument. Will insert the
 *        status code define + full description.
 *      - \%Vt   - Current thread (RTThreadSelf()).
 *      - \%Vhxd - Takes a pointer to the memory which is to be dumped in typical
 *        hex format. Use the width to specify the length, and the precision to
 *        set the number of bytes per line. Default width and precision is 16.
 *      - \%Vhxs - Takes a pointer to the memory to be displayed as a hex string,
 *        i.e. a series of space separated bytes formatted as two digit hex value.
 *        Use the width to specify the length. Default length is 16 bytes.
 *      - \%VGp  - Guest context physical address.
 *      - \%VGv  - Guest context virtual address.
 *      - \%VHp  - Host context physical address.
 *      - \%VHv  - Host context virtual address.
 *      - \%VI[8|16|32|64] - Signed integer value of the specifed bit count.
 *      - \%VU[8|16|32|64] - Unsigned integer value of the specifed bit count.
 *      - \%VX[8|16|32|64] - Hexadecimal integer value of the specifed bit count.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/log.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/stdarg.h>
#ifdef IN_RING3
# include <iprt/thread.h>
# include <iprt/err.h>
#endif
#include "internal/string.h"


/**
 * Callback to format VBox formatting extentions.
 * See @ref pg_rt_str_format_vbox for a reference on the format types.
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
size_t rtstrFormatVBox(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, const char **ppszFormat, va_list *pArgs, int cchWidth, int cchPrecision, unsigned fFlags, char chArgSize)
{
    char ch = *(*ppszFormat)++;
    if (ch == 'V')
    {
        ch = *(*ppszFormat)++;
        switch (ch)
        {
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
                        AssertMsgFailed(("Invalid status code format type '%%Vr%c%.10s'!\n", ch, *ppszFormat));
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
                        AssertMsgFailed(("Invalid status code format type '%%Vr%c%.10s'!\n", ch, *ppszFormat));
                        return 0;
                }
#endif /* !IN_RING3 */
                break;
            }

            /*
             * Current thread.
             */
            case 't':
#if defined(IN_RING3) && !defined(RT_MINI)  /* we don't want this anywhere else yet. */
                return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%#x\n", RTThreadSelf());
#else /* !IN_RING3 || RT_MINI */
                return pfnOutput(pvArgOutput, "0xffffffff", sizeof("0xffffffff") - 1);
#endif /* !IN_RING3 || RT_MINI */

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
                                    int cch = 0;
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
                                        int cch = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%02x", *pu8++);
                                        for (; cchWidth > 0; cchWidth--, pu8++)
                                            cch += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, " %02x", *pu8);
                                        return cch;
                                    }
                                    break;
                                }

                                default:
                                    AssertMsgFailed(("Invalid status code format type '%%Vhx%c%.10s'!\n", ch, *ppszFormat));
                                    break;
                            }
                        }
                        else
                            return pfnOutput(pvArgOutput, "<null>", sizeof("<null>") - 1);
                        break;
                    }

                    default:
                        AssertMsgFailed(("Invalid status code format type '%%Vh%c%.10s'!\n", ch, *ppszFormat));
                        return 0;

                }
                break;
            }

            /*
             * Guest types.
             */
            case 'G':
            {
                /*
                 * Guest types.
                 */
                ch = *(*ppszFormat)++;
                switch (ch)
                {
                    case 'p':   /* Physical address. */
                    {
                        RTGCPHYS GCPhys = va_arg(*pArgs, RTGCPHYS);
                        switch (sizeof(GCPhys))
                        {
                            case 4: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%08x", GCPhys);
                            case 8: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%016llx", GCPhys);
                            default: AssertFailed(); return 0;
                        }
                    }

                    case 'v':   /* Virtual address. */
                    {
                        RTGCPTR GCPtr = va_arg(*pArgs, RTGCPTR);
                        switch (sizeof(GCPtr))
                        {
                            case 4: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%08x", GCPtr);
                            case 8: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%016llx", GCPtr);
                            default: AssertFailed(); return 0;
                        }
                    }

                    default:
                        AssertMsgFailed(("Invalid status code format type '%%VG%c%.10s'!\n", ch, *ppszFormat));
                        break;
                }
                break;
            }

            /*
             * Host types.
             */
            case 'H':
            {
                /*
                 * Host types.
                 */
                ch = *(*ppszFormat)++;
                switch (ch)
                {
                    case 'p':   /* Physical address. */
                    {
                        RTHCPHYS HCPhys = va_arg(*pArgs, RTHCPHYS);
                        switch (sizeof(HCPhys))
                        {
                            case 4: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%08x", HCPhys);
                            case 8: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%016llx", HCPhys);
                            default: AssertFailed(); return 0;
                        }
                    }

                    case 'v':   /* Virtual address. */
                    {
                        RTHCPTR HCPtr = va_arg(*pArgs, RTHCPTR);
                        switch (sizeof(HCPtr))
                        {
                            case 4: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%08x", HCPtr);
                            case 8: return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%016llx", HCPtr);
                            default: AssertFailed(); return 0;
                        }
                    }

                    default:
                        AssertMsgFailed(("Invalid status code format type '%%VH%c%.10s'!\n", ch, *ppszFormat));
                        break;
                }
                break;
            }

            /*
             * Signed integer types: VI<bits>
             */
            case 'I':
            {
                /*
                 * Figure out number of bits.
                 */
                int64_t i64 = 0;
                char szNum[64];
                int cch;

                ch = *(*ppszFormat)++;
                if (ch == '8')
                    i64 = (int8_t)va_arg(*pArgs, int);
                else if (ch == '1')
                {
                    ch = *(*ppszFormat)++;
                    if (ch == '6')
                        i64 = (int16_t)va_arg(*pArgs, int);
                    else
                        AssertMsgFailed(("Invalid format %%VI1%c.10s\n", ch, *ppszFormat));
                }
                else if (ch == '3')
                {
                    ch = *(*ppszFormat)++;
                    if (ch == '2')
                        i64 = (int32_t)va_arg(*pArgs, int32_t);
                    else
                        AssertMsgFailed(("Invalid format %%VI1%c.10s\n", ch, *ppszFormat));
                }
                else if (ch == '6')
                {
                    ch = *(*ppszFormat)++;
                    if (ch == '4')
                        i64 = (int64_t)va_arg(*pArgs, int64_t);
                    else
                        AssertMsgFailed(("Invalid format %%VI1%c.10s\n", ch, *ppszFormat));
                }
                else
                    AssertMsgFailed(("Invalid format %%VI%c.10s\n", ch, *ppszFormat));

                cch = RTStrFormatNumber(szNum, i64, 10, cchWidth, cchPrecision, fFlags | RTSTR_F_VALSIGNED);
                Assert(cch < (int)sizeof(szNum));
                return pfnOutput(pvArgOutput, szNum, cch);
            }

            /*
             * Unsigned integer types: VU<bits>
             * Hex integer types     : VX<bits>
             */
            case 'U':
            case 'X':
            {
                int iBase = ch == 'X' ? 16 : 10;

                /*
                 * Figure out number of bits and read the value.
                 */
                uint64_t u64 = 0;
                char szNum[64];
                int cch;

                ch = *(*ppszFormat)++;
                if (ch == '8')
                    u64 = (uint8_t)va_arg(*pArgs, unsigned);
                else if (ch == '1')
                {
                    ch = *(*ppszFormat)++;
                    if (ch == '6')
                        u64 = (uint16_t)va_arg(*pArgs, unsigned);
                    else
                        AssertMsgFailed(("Invalid format %%VI1%c.10s\n", ch, *ppszFormat));
                }
                else if (ch == '3')
                {
                    ch = *(*ppszFormat)++;
                    if (ch == '2')
                        u64 = (uint32_t)va_arg(*pArgs, uint32_t);
                    else
                        AssertMsgFailed(("Invalid format %%VI1%c.10s\n", ch, *ppszFormat));
                }
                else if (ch == '6')
                {
                    ch = *(*ppszFormat)++;
                    if (ch == '4')
                        u64 = (uint64_t)va_arg(*pArgs, uint64_t);
                    else
                        AssertMsgFailed(("Invalid format %%VI1%c.10s\n", ch, *ppszFormat));
                }
                else
                    AssertMsgFailed(("Invalid format %%VI%c.10s\n", ch, *ppszFormat));

                cch = RTStrFormatNumber(szNum, u64, iBase, cchWidth, cchPrecision, fFlags);
                Assert(cch < (int)sizeof(szNum));
                return pfnOutput(pvArgOutput, szNum, cch);
            }

            /*
             * UUID.
             */
            case 'u':
                if (    (*ppszFormat)[0] == 'u'
                    &&  (*ppszFormat)[1] == 'i'
                    &&  (*ppszFormat)[2] == 'd')
                {
                    PRTUUID pUuid = va_arg(*pArgs, PRTUUID);
                    static const char szNull[] = "<NULL>";

                    (*ppszFormat) += 3;
                    if (VALID_PTR(pUuid))
                    {
                        /* cannot call RTUuidToStr because of GC/R0. */
                        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                                           "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                                           pUuid->Gen.u32TimeLow,
                                           pUuid->Gen.u16TimeMid,
                                           pUuid->Gen.u16TimeHiAndVersion,
                                           pUuid->Gen.u16ClockSeq & 0xff,
                                           pUuid->Gen.u16ClockSeq >> 8,
                                           pUuid->Gen.au8Node[0],
                                           pUuid->Gen.au8Node[1],
                                           pUuid->Gen.au8Node[2],
                                           pUuid->Gen.au8Node[3],
                                           pUuid->Gen.au8Node[4],
                                           pUuid->Gen.au8Node[5]);
                    }

                    return pfnOutput(pvArgOutput, szNull, sizeof(szNull) - 1);
                }
                /* fall thru */

            /*
             * Invalid/Unknown. Bitch about it.
             */
            default:
                AssertMsgFailed(("Invalid VBox format type '%%V%c%.10s'!\n", ch, *ppszFormat));
                break;
        }
    }
    else
        AssertMsgFailed(("Invalid VBox format type '%%%c%.10s'!\n", ch, *ppszFormat));

    return 0;
}

