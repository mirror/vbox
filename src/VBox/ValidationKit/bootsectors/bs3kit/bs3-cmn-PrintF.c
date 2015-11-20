/* $Id$ */
/** @file
 * BS3Kit - Bs3PrintF
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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
 */

#include "bs3kit-template-header.h"
#include <iprt/ctype.h>


#define STR_F_CAPITAL         0x0001
#define STR_F_LEFT            0x0002
#define STR_F_ZEROPAD         0x0004
#define STR_F_SPECIAL         0x0008
#define STR_F_VALSIGNED       0x0010
#define STR_F_PLUS            0x0020
#define STR_F_BLANK           0x0040
#define STR_F_WIDTH           0x0080
#define STR_F_PRECISION       0x0100
#define STR_F_THOUSAND_SEP    0x0200


static size_t bs3PrintFormatString(const char *psz, size_t cchMax, uint16_t fFlags, int cchWidth, int cchPrecision)
{
    size_t cchRet;
    if (cchMax == ~(size_t)0)
        cchMax = Bs3StrNLen(psz, cchMax);
    cchRet = cchMax;

    /** @todo this is kind of crude, full fleged formatting can wait.  */
    while (cchMax-- > 0)
        Bs3PrintChr(*psz++);

    return cchRet;
}


static size_t bs3PrintFormatU64(char szTmp[64], uint64_t uValue, unsigned uBase, uint16_t fFlags, int cchWidth, int cchPrecision)
{
#if 0
    const char *pachDigits = !(fFlags & STR_F_CAPITAL) ? BS3_DATA_NM(g_achBs3HexDigits) : BS3_DATA_NM(g_achBs3HexDigitsUpper);
    char       *psz = &szTmp[64];

    *--psz = '\0';
    if (uBase == 10)
    {
        do
        {
            *--psz = pachDigits[uValue % 10];
            uValue /= 10;
        } while (uValue > 0);
    }
    else
    {
        unsigned const cShift = uBase == 8 ? 2 : 3;
        unsigned const fMask  = uBase == 8 ? 7 : 15;
        do
        {
            *--psz = pachDigits[uValue & fMask];
            uValue >>= cShift;
        } while (uValue > 0);
    }
    return bs3PrintFormatString(psz, &szTmp[63] - psz, 0, 0, 0);
#else
    return 0;
#endif
}

#if ARCH_BITS == 64
# define bs3PrintFormatU32 bs3PrintFormatU64
# define bs3PrintFormatU16 bs3PrintFormatU64

#else
static size_t bs3PrintFormatU32(char szTmp[64], uint32_t uValue, unsigned uBase, uint16_t fFlags, int cchWidth, int cchPrecision)
{
    const char *pachDigits = !(fFlags & STR_F_CAPITAL) ? BS3_DATA_NM(g_achBs3HexDigits) : BS3_DATA_NM(g_achBs3HexDigitsUpper);
    char       *psz = &szTmp[64];

    *--psz = '\0';
    if (uBase == 10)
    {
        do
        {
            *--psz = pachDigits[uValue % 10];
            uValue /= 10;
        } while (uValue > 0);
    }
    else
    {
        unsigned const cShift = uBase == 8 ? 2 : 3;
        unsigned const fMask  = uBase == 8 ? 7 : 15;
        do
        {
            *--psz = pachDigits[uValue & fMask];
            uValue >>= cShift;
        } while (uValue > 0);
    }
    return bs3PrintFormatString(psz, &szTmp[63] - psz, 0, 0, 0);
}

# if ARCH_BITS == 16
static size_t bs3PrintFormatU16(char szTmp[64], uint16_t uValue, unsigned uBase, uint16_t fFlags, int cchWidth, int cchPrecision)
{
    if (uBase == 10)
    {
        const char *pachDigits = !(fFlags & STR_F_CAPITAL) ? BS3_DATA_NM(g_achBs3HexDigits) : BS3_DATA_NM(g_achBs3HexDigitsUpper);
        char       *psz = &szTmp[64];

        *--psz = '\0';
        do
        {
            *--psz = pachDigits[uValue % 10];
            uValue /= 10;
        } while (uValue > 0);
        return bs3PrintFormatString(psz, &szTmp[63] - psz, 0, 0, 0);
    }
    /* 32-bit shifting is reasonably cheap, so combine with 32-bit. */
    return bs3PrintFormatU32(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
}
# endif /* 16-bit */
#endif /* !64-bit */


static size_t bs3PrintFormatS64(char szTmp[64], int16_t iValue, uint16_t fFlags, int cchWidth, int cchPrecision)
{
    /** @todo this is kind of crude, full fleged formatting can wait.  */
    size_t cchRet = 0;
    if (iValue < 0)
    {
        Bs3PrintChr('-');
        cchRet++;
    }
    cchRet += bs3PrintFormatU64(szTmp, iValue, 10, fFlags & ~STR_F_VALSIGNED, cchWidth, cchPrecision);
    return cchRet;
}


static size_t bs3PrintFormatS32(char szTmp[64], int16_t iValue, uint16_t fFlags, int cchWidth, int cchPrecision)
{
    /** @todo this is kind of crude, full fleged formatting can wait.  */
    size_t cchRet = 0;
    if (iValue < 0)
    {
        Bs3PrintChr('-');
        cchRet++;
    }
    cchRet += bs3PrintFormatU32(szTmp, iValue, 10, fFlags & ~STR_F_VALSIGNED, cchWidth, cchPrecision);
    return cchRet;
}


#if ARCH_BITS == 16
static size_t bs3PrintFormatS16(char szTmp[64], int16_t iValue, uint16_t fFlags, int cchWidth, int cchPrecision)
{
    /** @todo this is kind of crude, full fleged formatting can wait.  */
    size_t cchRet = 0;
    if (iValue < 0)
    {
        Bs3PrintChr('-');
        cchRet++;
    }
    cchRet += bs3PrintFormatU16(szTmp, iValue, 10, fFlags & ~STR_F_VALSIGNED, cchWidth, cchPrecision);
    return cchRet;
}
#endif





BS3_DECL(size_t) Bs3PrintFV(const char BS3_FAR *pszFormat, va_list va)
{
    size_t cchRet = 0;
    char ch;
    while ((ch = *pszFormat++) != '\0')
    {
        unsigned int    fFlags;
        int             cchWidth;
        int             cchPrecision;
        char            chArgSize;
        char            szTmp[64];

        /*
         * Deal with plain chars.
         */
        if (ch != '%')
        {
            if (ch == '\n')
                Bs3PrintChr('\r');
            Bs3PrintChr(ch);
            cchRet++;
            continue;
        }

        ch = *pszFormat++;
        if (ch == '%')
        {
            Bs3PrintChr(ch);
            cchRet++;
            continue;
        }

        /*
         * Flags.
         */
        fFlags = 0;
        for (;;)
        {
            unsigned int fThis;
            switch (ch)
            {
                default:    fThis = 0;                  break;
                case '#':   fThis = STR_F_SPECIAL;      break;
                case '-':   fThis = STR_F_LEFT;         break;
                case '+':   fThis = STR_F_PLUS;         break;
                case ' ':   fThis = STR_F_BLANK;        break;
                case '0':   fThis = STR_F_ZEROPAD;      break;
                case '\'':  fThis = STR_F_THOUSAND_SEP; break;
            }
            if (!fThis)
                break;
            fFlags |= fThis;
            ch = *pszFormat++;
        }

        /*
         * Width.
         */
        cchWidth = 0;
        if (RT_C_IS_DIGIT(ch))
        {
            do
            {
                cchWidth *= 10;
                cchWidth  = ch - '0';
                ch = *pszFormat++;
            } while (RT_C_IS_DIGIT(ch));
            fFlags |= STR_F_WIDTH;
        }
        else if (ch == '*')
        {
            cchWidth = va_arg(va, int);
            if (cchWidth < 0)
            {
                cchWidth = -cchWidth;
                fFlags |= STR_F_LEFT;
            }
            fFlags |= STR_F_WIDTH;
        }

        /*
         * Precision
         */
        cchPrecision = 0;
        if (RT_C_IS_DIGIT(ch))
        {
            do
            {
                cchPrecision *= 10;
                cchPrecision  = ch - '0';
                ch = *pszFormat++;
            } while (RT_C_IS_DIGIT(ch));
            fFlags |= STR_F_PRECISION;
        }
        else if (ch == '*')
        {
            cchPrecision = va_arg(va, int);
            if (cchPrecision < 0)
                cchPrecision = 0;
            fFlags |= STR_F_PRECISION;
        }

        /*
         * Argument size.
         */
        chArgSize = ch;
        switch (ch)
        {
            default:
                chArgSize = 0;
                break;

            case 'z':
            case 'L':
            case 'j':
            case 't':
                ch = *pszFormat++;
                break;

            case 'l':
                ch = *pszFormat++;
                if (ch == 'l')
                {
                    chArgSize = 'L';
                    ch = *pszFormat++;
                }
                break;

            case 'h':
                ch = *pszFormat++;
                if (ch == 'h')
                {
                    chArgSize = 'H';
                    ch = *pszFormat++;
                }
                break;
        }

        /*
         * The type.
         */
        switch (ch)
        {
            /*
             * Char
             */
            case 'c':
                szTmp[0] = va_arg(va, int /*char*/);
                szTmp[1] = '\0';
                cchRet += bs3PrintFormatString(szTmp, 1, fFlags, cchWidth, cchPrecision);
                break;

            /*
             * String.
             */
            case 's':
            {
                const char BS3_FAR *psz = va_arg(va, const char BS3_FAR *);
                if (psz == NULL)
                    psz = "<NULL>";
                cchRet += bs3PrintFormatString(psz, ~(size_t)0, fFlags, cchWidth, cchPrecision);
                break;
            }

            /*
             * Signed integers.
             */
            case 'i':
            case 'd':
                fFlags |= STR_F_VALSIGNED;
                switch (chArgSize)
                {
                    case 0:
                    case 'h': /* signed short should be promoted to int or be the same as int */
                    case 'H': /* signed char should be promoted to int. */
                    {
                        signed int iValue = va_arg(va, signed int);
#if ARCH_BITS == 16
                        cchRet += bs3PrintFormatS16(szTmp, iValue, fFlags, cchWidth, cchPrecision);
#else
                        cchRet += bs3PrintFormatS32(szTmp, iValue, fFlags, cchWidth, cchPrecision);
#endif
                        break;
                    }
                    case 'l':
                    {
                        signed long lValue = va_arg(va, signed long);
                        if (sizeof(lValue) == 4)
                            cchRet += bs3PrintFormatS32(szTmp, lValue, fFlags, cchWidth, cchPrecision);
                        else
                            cchRet += bs3PrintFormatS64(szTmp, lValue, fFlags, cchWidth, cchPrecision);
                        break;
                    }
                    case 'L':
                    {
                        unsigned long long ullValue = va_arg(va, unsigned long long);
                        cchRet += bs3PrintFormatS64(szTmp, ullValue, fFlags, cchWidth, cchPrecision);
                        break;
                    }
                }
                break;

            /*
             * Unsigned integers.
             */
            case 'X':
                fFlags |= STR_F_CAPITAL;
            case 'x':
            case 'u':
            {
                unsigned uBase = ch = 'u' ? 10 : 16;
                switch (chArgSize)
                {
                    case 0:
                    case 'h': /* unsigned short should be promoted to int or be the same as int */
                    case 'H': /* unsigned char should be promoted to int. */
                    {
                        unsigned int uValue = va_arg(va, unsigned int);
#if ARCH_BITS == 16
                        cchRet += bs3PrintFormatU16(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
#else
                        cchRet += bs3PrintFormatU32(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
#endif
                        break;
                    }
                    case 'l':
                    {
                        unsigned long ulValue = va_arg(va, unsigned long);
                        if (sizeof(ulValue) == 4)
                            cchRet += bs3PrintFormatU32(szTmp, ulValue, uBase, fFlags, cchWidth, cchPrecision);
                        else
                            cchRet += bs3PrintFormatU64(szTmp, ulValue, uBase, fFlags, cchWidth, cchPrecision);
                        break;
                    }
                    case 'L':
                    {
                        unsigned long long ullValue = va_arg(va, unsigned long long);
                        cchRet += bs3PrintFormatU64(szTmp, ullValue, uBase, fFlags, cchWidth, cchPrecision);
                        break;
                    }
                }
                break;
            }

            /*
             * Our stuff.
             */
            case 'R':
            {
                unsigned uBase;
                ch = *pszFormat++;
                switch (ch)
                {
                    case 'I':
                        fFlags |= STR_F_VALSIGNED;
                        /* fall thru */
                    case 'U':
                        uBase = 10;
                        break;
                    case 'X':
                        uBase = 16;
                        break;
                    default:
                        uBase = 0;
                        break;
                }
                if (uBase)
                {
                    ch = *pszFormat++;
                    switch (ch)
                    {
#if ARCH_BITS != 16
                        case '3':
                        case '1': /* Will an unsigned 16-bit value always be promoted
                                     to a 16-bit unsigned int. It certainly will be promoted to a 32-bit int. */
                            pszFormat++; /* Assumes (1)'6' or (3)'2' */
#else
                        case '1':
                            pszFormat++; /* Assumes (1)'6' */
#endif
                        case '8': /* An unsigned 8-bit value should be promoted to int, which is at least 16-bit. */
                        {
                            unsigned int uValue = va_arg(va, unsigned int);
#if ARCH_BITS == 16
                            cchRet += bs3PrintFormatU16(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
#else
                            cchRet += bs3PrintFormatU32(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
#endif
                            break;
                        }
#if ARCH_BITS == 16
                        case '3':
                        {
                            uint32_t uValue = va_arg(va, uint32_t);
                            pszFormat++;
                            cchRet += bs3PrintFormatU32(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
                            break;
                        }
#endif
                        case '6':
                        {
                            uint64_t uValue = va_arg(va, uint64_t);
                            pszFormat++;
                            cchRet += bs3PrintFormatU64(szTmp, uValue, uBase, fFlags, cchWidth, cchPrecision);
                            break;
                        }
                    }
                }
                break;
            }

            /*
             * Pointers.
             */
            case 'P':
                fFlags |= STR_F_CAPITAL;
                /* fall thru */
            case 'p':
            {
                void BS3_FAR *pv = va_arg(va, void BS3_FAR *);
#if ARCH_BITS == 16
                cchRet += bs3PrintFormatU16(szTmp, BS3_FP_SEG(pv), 16, fFlags, fFlags & STR_F_SPECIAL ? 6 : 4, 0);
                Bs3PrintChr(':');
                cchRet += 1;
                cchRet += bs3PrintFormatU16(szTmp, BS3_FP_OFF(pv), 16, fFlags & ~STR_F_SPECIAL, 4, 0);
#elif ARCH_BITS == 32
                cchRet += bs3PrintFormatU32(szTmp, (uintptr_t)pv, 16, fFlags | STR_F_SPECIAL, 10, 0);
#elif ARCH_BITS == 64
                cchRet += bs3PrintFormatU64(szTmp, (uintptr_t)pv, 16, fFlags | STR_F_SPECIAL | STR_F_THOUSAND_SEP, 19, 0);
#else
# error "Undefined or invalid ARCH_BITS."
#endif
                break;
            }

        }
    }
    return cchRet;
}


BS3_DECL(size_t) Bs3PrintF(const char BS3_FAR *pszFormat, ...)
{
    size_t cchRet;
    va_list va;
    va_start(va, pszFormat);
    cchRet = Bs3PrintFV(pszFormat, va);
    va_end(va);
    return cchRet;
}

