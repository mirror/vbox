/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened main().
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#if defined(RT_OS_OS2)
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>
# include <stdio.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <unistd.h>

#elif RT_OS_WINDOWS
# include <iprt/nt/nt-and-windows.h>

#else /* UNIXes */
# include <iprt/types.h> /* stdint fun on darwin. */

# include <stdio.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <limits.h>
# include <errno.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/types.h>
# if defined(RT_OS_LINUX)
#  undef USE_LIB_PCAP /* don't depend on libcap as we had to depend on either
                         libcap1 or libcap2 */

#  undef _POSIX_SOURCE
#  include <linux/types.h> /* sys/capabilities from uek-headers require this */
#  include <sys/capability.h>
#  include <sys/prctl.h>
#  ifndef CAP_TO_MASK
#   define CAP_TO_MASK(cap) RT_BIT(cap)
#  endif
# elif defined(RT_OS_FREEBSD)
#  include <sys/param.h>
#  include <sys/sysctl.h>
# elif defined(RT_OS_SOLARIS)
#  include <priv.h>
# endif
# include <pwd.h>
# ifdef RT_OS_DARWIN
#  include <mach-o/dyld.h>
# endif

#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#ifdef RT_OS_WINDOWS
# include <VBox/version.h>
#endif
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/param.h>

#include "SUPLibInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def SUP_HARDENED_SUID
 * Whether we're employing set-user-ID-on-execute in the hardening.
 */
#if !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_L4)
# define SUP_HARDENED_SUID
#else
# undef  SUP_HARDENED_SUID
#endif

/** @def SUP_HARDENED_SYM
 * Decorate a symbol that's resolved dynamically.
 */
#ifdef RT_OS_OS2
# define SUP_HARDENED_SYM(sym)  "_" sym
#else
# define SUP_HARDENED_SYM(sym)  sym
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** @see RTR3InitEx */
typedef DECLCALLBACK(int) FNRTR3INITEX(uint32_t iVersion, uint32_t fFlags, int cArgs,
                                       char **papszArgs, const char *pszProgramPath);
typedef FNRTR3INITEX *PFNRTR3INITEX;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The pre-init data we pass on to SUPR3 (residing in VBoxRT). */
static SUPPREINITDATA   g_SupPreInitData;
/** The program executable path. */
#ifndef RT_OS_WINDOWS
static
#endif
char                    g_szSupLibHardenedExePath[RTPATH_MAX];
/** The program directory path. */
static char             g_szSupLibHardenedDirPath[RTPATH_MAX];

/** The program name. */
static const char      *g_pszSupLibHardenedProgName;
/** The flags passed to SUPR3HardenedMain. */
static uint32_t         g_fSupHardenedMain;

#ifdef SUP_HARDENED_SUID
/** The real UID at startup. */
static uid_t            g_uid;
/** The real GID at startup. */
static gid_t            g_gid;
# ifdef RT_OS_LINUX
static uint32_t         g_uCaps;
# endif
#endif

/** The startup log file. */
#ifdef RT_OS_WINDOWS
static HANDLE           g_hStartupLog = NULL;
#else
static int              g_hStartupLog = -1;
#endif
/** The number of bytes we've written to the startup log. */
static uint32_t volatile g_cbStartupLog = 0;

/** The current SUPR3HardenedMain state / location. */
SUPR3HARDENEDMAINSTATE  g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED;
AssertCompileSize(g_enmSupR3HardenedMainState, sizeof(uint32_t));


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef SUP_HARDENED_SUID
static void supR3HardenedMainDropPrivileges(void);
#endif
static PFNSUPTRUSTEDERROR supR3HardenedMainGetTrustedError(const char *pszProgName);


/**
 * Safely copy one or more strings into the given buffer.
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pszDst              The destionation buffer.
 * @param   cbDst               The size of the destination buffer.
 * @param   ...                 One or more zero terminated strings, ending with
 *                              a NULL.
 */
static int suplibHardenedStrCopyEx(char *pszDst, size_t cbDst, ...)
{
    int rc = VINF_SUCCESS;

    if (cbDst == 0)
        return VERR_BUFFER_OVERFLOW;

    va_list va;
    va_start(va, cbDst);
    for (;;)
    {
        const char *pszSrc = va_arg(va, const char *);
        if (!pszSrc)
            break;

        size_t cchSrc = suplibHardenedStrLen(pszSrc);
        if (cchSrc < cbDst)
        {
            suplibHardenedMemCopy(pszDst, pszSrc, cchSrc);
            pszDst += cchSrc;
            cbDst  -= cchSrc;
        }
        else
        {
            rc = VERR_BUFFER_OVERFLOW;
            if (cbDst > 1)
            {
                suplibHardenedMemCopy(pszDst, pszSrc, cbDst - 1);
                pszDst += cbDst - 1;
                cbDst   = 1;
            }
        }
        *pszDst = '\0';
    }
    va_end(va);

    return rc;
}


/**
 * Exit current process in the quickest possible fashion.
 *
 * @param   rcExit      The exit code.
 */
DECLNORETURN(void) suplibHardenedExit(RTEXITCODE rcExit)
{
    for (;;)
    {
#ifdef RT_OS_WINDOWS
        if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
            ExitProcess(rcExit);
        if (RtlExitUserProcess != NULL)
            RtlExitUserProcess(rcExit);
        NtTerminateProcess(NtCurrentProcess(), rcExit);
#else
        _Exit(rcExit);
#endif
    }
}


/**
 * Writes a substring to standard error.
 *
 * @param   pch                 The start of the substring.
 * @param   cch                 The length of the substring.
 */
static void suplibHardenedPrintStrN(const char *pch, size_t cch)
{
#ifdef RT_OS_WINDOWS
    HANDLE hStdOut = NtCurrentPeb()->ProcessParameters->StandardOutput;
    if (hStdOut != NULL)
    {
        if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
        {
            DWORD cbWritten;
            WriteFile(hStdOut, pch, (DWORD)cch, &cbWritten, NULL);
        }
        /* Windows 7 and earlier uses fake handles, with the last two bits set ((hStdOut & 3) == 3). */
        else if (NtWriteFile != NULL && ((uintptr_t)hStdOut & 3) == 0)
        {
            IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            NtWriteFile(hStdOut, NULL /*Event*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                        &Ios, (PVOID)pch, (ULONG)cch, NULL /*ByteOffset*/, NULL /*Key*/);
        }
    }
#else
    (void)write(2, pch, cch);
#endif
}


/**
 * Writes a string to standard error.
 *
 * @param   psz                 The string.
 */
static void suplibHardenedPrintStr(const char *psz)
{
    suplibHardenedPrintStrN(psz, suplibHardenedStrLen(psz));
}


/**
 * Writes a char to standard error.
 *
 * @param   ch                  The character value to write.
 */
static void suplibHardenedPrintChr(char ch)
{
    suplibHardenedPrintStrN(&ch, 1);
}


/**
 * Writes a decimal number to stdard error.
 *
 * @param   uValue              The value.
 */
static void suplibHardenedPrintDecimal(uint64_t uValue)
{
    char    szBuf[64];
    char   *pszEnd = &szBuf[sizeof(szBuf) - 1];
    char   *psz    = pszEnd;

    *psz-- = '\0';

    do
    {
        *psz-- = '0' + (uValue % 10);
        uValue /= 10;
    } while (uValue > 0);

    psz++;
    suplibHardenedPrintStrN(psz, pszEnd - psz);
}


/**
 * Writes a hexadecimal or octal number to standard error.
 *
 * @param   uValue              The value.
 * @param   uBase               The base (16 or 8).
 * @param   fFlags              Format flags.
 */
static void suplibHardenedPrintHexOctal(uint64_t uValue, unsigned uBase, uint32_t fFlags)
{
    static char const   s_achDigitsLower[17] = "0123456789abcdef";
    static char const   s_achDigitsUpper[17] = "0123456789ABCDEF";
    const char         *pchDigits   = !(fFlags & RTSTR_F_CAPITAL) ? s_achDigitsLower : s_achDigitsUpper;
    unsigned            cShift      = uBase == 16 ?   4 : 3;
    unsigned            fDigitMask  = uBase == 16 ? 0xf : 7;
    char                szBuf[64];
    char               *pszEnd = &szBuf[sizeof(szBuf) - 1];
    char               *psz    = pszEnd;

    *psz-- = '\0';

    do
    {
        *psz-- = pchDigits[uValue & fDigitMask];
        uValue >>= cShift;
    } while (uValue > 0);

    if ((fFlags & RTSTR_F_SPECIAL) && uBase == 16)
    {
        *psz-- = !(fFlags & RTSTR_F_CAPITAL) ? 'x' : 'X';
        *psz-- = '0';
    }

    psz++;
    suplibHardenedPrintStrN(psz, pszEnd - psz);
}


/**
 * Writes a wide character string to standard error.
 *
 * @param   pwsz                The string.
 */
static void suplibHardenedPrintWideStr(PCRTUTF16 pwsz)
{
    for (;;)
    {
        RTUTF16 wc = *pwsz++;
        if (!wc)
            return;
        if (   (wc < 0x7f && wc >= 0x20)
            || wc == '\n'
            || wc == '\r')
            suplibHardenedPrintChr((char)wc);
        else
        {
            suplibHardenedPrintStrN(RT_STR_TUPLE("\\x"));
            suplibHardenedPrintHexOctal(wc, 16, 0);
        }
    }
}

#ifdef IPRT_NO_CRT

/** Buffer structure used by suplibHardenedOutput. */
struct SUPLIBHARDENEDOUTPUTBUF
{
    size_t off;
    char   szBuf[2048];
};

/** Callback for RTStrFormatV, see FNRTSTROUTPUT. */
static DECLCALLBACK(size_t) suplibHardenedOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    SUPLIBHARDENEDOUTPUTBUF *pBuf = (SUPLIBHARDENEDOUTPUTBUF *)pvArg;
    size_t cbTodo = cbChars;
    for (;;)
    {
        size_t cbSpace = sizeof(pBuf->szBuf) - pBuf->off - 1;

        /* Flush the buffer? */
        if (   cbSpace == 0
            || (cbTodo == 0 && pBuf->off))
        {
            suplibHardenedPrintStrN(pBuf->szBuf, pBuf->off);
# ifdef RT_OS_WINDOWS
            if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
                OutputDebugString(pBuf->szBuf);
# endif
            pBuf->off = 0;
            cbSpace = sizeof(pBuf->szBuf) - 1;
        }

        /* Copy the string into the buffer. */
        if (cbTodo == 1)
        {
            pBuf->szBuf[pBuf->off++] = *pachChars;
            break;
        }
        if (cbSpace >= cbTodo)
        {
            memcpy(&pBuf->szBuf[pBuf->off], pachChars, cbTodo);
            pBuf->off += cbTodo;
            break;
        }
        memcpy(&pBuf->szBuf[pBuf->off], pachChars, cbSpace);
        pBuf->off += cbSpace;
        cbTodo -= cbSpace;
    }
    pBuf->szBuf[pBuf->off] = '\0';

    return cbChars;
}

#endif /* IPRT_NO_CRT */

/**
 * Simple printf to standard error.
 *
 * @param   pszFormat   The format string.
 * @param   va          Arguments to format.
 */
DECLHIDDEN(void) suplibHardenedPrintFV(const char *pszFormat, va_list va)
{
#ifdef IPRT_NO_CRT
    /*
     * Use buffered output here to avoid character mixing on the windows
     * console and to enable us to use OutputDebugString.
     */
    SUPLIBHARDENEDOUTPUTBUF Buf;
    Buf.off = 0;
    Buf.szBuf[0] = '\0';
    RTStrFormatV(suplibHardenedOutput, &Buf, NULL, NULL, pszFormat, va);

#else /* !IPRT_NO_CRT */
    /*
     * Format loop.
     */
    char ch;
    const char *pszLast = pszFormat;
    for (;;)
    {
        ch = *pszFormat;
        if (!ch)
            break;
        pszFormat++;

        if (ch == '%')
        {
            /*
             * Format argument.
             */

            /* Flush unwritten bits. */
            if (pszLast != pszFormat - 1)
                suplibHardenedPrintStrN(pszLast, pszFormat - pszLast - 1);
            pszLast = pszFormat;
            ch = *pszFormat++;

            /* flags. */
            uint32_t fFlags = 0;
            for (;;)
            {
                if (ch == '#')          fFlags |= RTSTR_F_SPECIAL;
                else if (ch == '-')     fFlags |= RTSTR_F_LEFT;
                else if (ch == '+')     fFlags |= RTSTR_F_PLUS;
                else if (ch == ' ')     fFlags |= RTSTR_F_BLANK;
                else if (ch == '0')     fFlags |= RTSTR_F_ZEROPAD;
                else if (ch == '\'')    fFlags |= RTSTR_F_THOUSAND_SEP;
                else                    break;
                ch = *pszFormat++;
            }

            /* Width and precision - ignored. */
            while (RT_C_IS_DIGIT(ch))
                ch = *pszFormat++;
            if (ch == '*')
                va_arg(va, int);
            if (ch == '.')
            {
                do ch = *pszFormat++;
                while (RT_C_IS_DIGIT(ch));
                if (ch == '*')
                    va_arg(va, int);
            }

            /* Size. */
            char chArgSize = 0;
            switch (ch)
            {
                case 'z':
                case 'L':
                case 'j':
                case 't':
                    chArgSize = ch;
                    ch = *pszFormat++;
                    break;

                case 'l':
                    chArgSize = ch;
                    ch = *pszFormat++;
                    if (ch == 'l')
                    {
                        chArgSize = 'L';
                        ch = *pszFormat++;
                    }
                    break;

                case 'h':
                    chArgSize = ch;
                    ch = *pszFormat++;
                    if (ch == 'h')
                    {
                        chArgSize = 'H';
                        ch = *pszFormat++;
                    }
                    break;
            }

            /*
             * Do type specific formatting.
             */
            switch (ch)
            {
                case 'c':
                    ch = (char)va_arg(va, int);
                    suplibHardenedPrintChr(ch);
                    break;

                case 's':
                    if (chArgSize == 'l')
                    {
                        PCRTUTF16 pwszStr = va_arg(va, PCRTUTF16 );
                        if (RT_VALID_PTR(pwszStr))
                            suplibHardenedPrintWideStr(pwszStr);
                        else
                            suplibHardenedPrintStr("<NULL>");
                    }
                    else
                    {
                        const char *pszStr = va_arg(va, const char *);
                        if (!RT_VALID_PTR(pszStr))
                            pszStr = "<NULL>";
                        suplibHardenedPrintStr(pszStr);
                    }
                    break;

                case 'd':
                case 'i':
                {
                    int64_t iValue;
                    if (chArgSize == 'L' || chArgSize == 'j')
                        iValue = va_arg(va, int64_t);
                    else if (chArgSize == 'l')
                        iValue = va_arg(va, signed long);
                    else if (chArgSize == 'z' || chArgSize == 't')
                        iValue = va_arg(va, intptr_t);
                    else
                        iValue = va_arg(va, signed int);
                    if (iValue < 0)
                    {
                        suplibHardenedPrintChr('-');
                        iValue = -iValue;
                    }
                    suplibHardenedPrintDecimal(iValue);
                    break;
                }

                case 'p':
                case 'x':
                case 'X':
                case 'u':
                case 'o':
                {
                    unsigned uBase = 10;
                    uint64_t uValue;

                    switch (ch)
                    {
                        case 'p':
                            fFlags |= RTSTR_F_ZEROPAD; /* Note not standard behaviour (but I like it this way!) */
                            uBase = 16;
                            break;
                        case 'X':
                            fFlags |= RTSTR_F_CAPITAL;
                        case 'x':
                            uBase = 16;
                            break;
                        case 'u':
                            uBase = 10;
                            break;
                        case 'o':
                            uBase = 8;
                            break;
                    }

                    if (ch == 'p' || chArgSize == 'z' || chArgSize == 't')
                        uValue = va_arg(va, uintptr_t);
                    else if (chArgSize == 'L' || chArgSize == 'j')
                        uValue = va_arg(va, uint64_t);
                    else if (chArgSize == 'l')
                        uValue = va_arg(va, unsigned long);
                    else
                        uValue = va_arg(va, unsigned int);

                    if (uBase == 10)
                        suplibHardenedPrintDecimal(uValue);
                    else
                        suplibHardenedPrintHexOctal(uValue, uBase, fFlags);
                    break;
                }

                case 'R':
                    if (pszFormat[0] == 'r' && pszFormat[1] == 'c')
                    {
                        int iValue = va_arg(va, int);
                        if (iValue < 0)
                        {
                            suplibHardenedPrintChr('-');
                            iValue = -iValue;
                        }
                        suplibHardenedPrintDecimal(iValue);
                        pszFormat += 2;
                        break;
                    }
                    /* fall thru */

                /*
                 * Custom format.
                 */
                default:
                    suplibHardenedPrintStr("[bad format: ");
                    suplibHardenedPrintStrN(pszLast, pszFormat - pszLast);
                    suplibHardenedPrintChr(']');
                    break;
            }

            /* continue */
            pszLast = pszFormat;
        }
    }

    /* Flush the last bits of the string. */
    if (pszLast != pszFormat)
        suplibHardenedPrintStrN(pszLast, pszFormat - pszLast);
#endif /* !IPRT_NO_CRT */
}


/**
 * Prints to standard error.
 *
 * @param   pszFormat   The format string.
 * @param   ...         Arguments to format.
 */
DECLHIDDEN(void) suplibHardenedPrintF(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    suplibHardenedPrintFV(pszFormat, va);
    va_end(va);
}


/**
 * @copydoc RTPathStripFilename.
 */
static void suplibHardenedPathStripFilename(char *pszPath)
{
    char *psz = pszPath;
    char *pszLastSep = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastSep = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastSep = psz;
                break;

            /* the end */
            case '\0':
                if (pszLastSep == pszPath)
                    *pszLastSep++ = '.';
                *pszLastSep = '\0';
                return;
        }
    }
    /* will never get here */
}


/**
 * @copydoc RTPathFilename
 */
DECLHIDDEN(char *) supR3HardenedPathFilename(const char *pszPath)
{
    const char *psz = pszPath;
    const char *pszLastComp = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastComp = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastComp = psz + 1;
                break;

            /* the end */
            case '\0':
                if (*pszLastComp)
                    return (char *)(void *)pszLastComp;
                return NULL;
        }
    }

    /* will never get here */
    return NULL;
}


/**
 * @copydoc RTPathAppPrivateNoArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateNoArch(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE)
    const char *pszSrcPath = RTPATH_APP_PRIVATE;
    size_t cchPathPrivateNoArch = suplibHardenedStrLen(pszSrcPath);
    if (cchPathPrivateNoArch >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppPrivateNoArch: Buffer overflow, %zu >= %zu\n", cchPathPrivateNoArch, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathPrivateNoArch + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathExecDir(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathAppPrivateArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateArch(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE_ARCH)
    const char *pszSrcPath = RTPATH_APP_PRIVATE_ARCH;
    size_t cchPathPrivateArch = suplibHardenedStrLen(pszSrcPath);
    if (cchPathPrivateArch >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppPrivateArch: Buffer overflow, %zu >= %zu\n", cchPathPrivateArch, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathPrivateArch + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathExecDir(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathSharedLibs
 */
DECLHIDDEN(int) supR3HardenedPathSharedLibs(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_SHARED_LIBS)
    const char *pszSrcPath = RTPATH_SHARED_LIBS;
    size_t cchPathSharedLibs = suplibHardenedStrLen(pszSrcPath);
    if (cchPathSharedLibs >= cchPath)
        supR3HardenedFatal("supR3HardenedPathSharedLibs: Buffer overflow, %zu >= %zu\n", cchPathSharedLibs, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathSharedLibs + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathExecDir(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathAppDocs
 */
DECLHIDDEN(int) supR3HardenedPathAppDocs(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_DOCS)
    const char *pszSrcPath = RTPATH_APP_DOCS;
    size_t cchPathAppDocs = suplibHardenedStrLen(pszSrcPath);
    if (cchPathAppDocs >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppDocs: Buffer overflow, %zu >= %zu\n", cchPathAppDocs, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathAppDocs + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathExecDir(pszPath, cchPath);
#endif
}


/**
 * Returns the full path to the executable.
 *
 * @returns IPRT status code.
 * @param   pszPath     Where to store it.
 * @param   cchPath     How big that buffer is.
 */
static void supR3HardenedGetFullExePath(void)
{
    /*
     * Get the program filename.
     *
     * Most UNIXes have no API for obtaining the executable path, but provides a symbolic
     * link in the proc file system that tells who was exec'ed. The bad thing about this
     * is that we have to use readlink, one of the weirder UNIX APIs.
     *
     * Darwin, OS/2 and Windows all have proper APIs for getting the program file name.
     */
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD) || defined(RT_OS_SOLARIS)
# ifdef RT_OS_LINUX
    int cchLink = readlink("/proc/self/exe", &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);

# elif defined(RT_OS_SOLARIS)
    char szFileBuf[PATH_MAX + 1];
    sprintf(szFileBuf, "/proc/%ld/path/a.out", (long)getpid());
    int cchLink = readlink(szFileBuf, &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);

# else /* RT_OS_FREEBSD */
    int aiName[4];
    aiName[0] = CTL_KERN;
    aiName[1] = KERN_PROC;
    aiName[2] = KERN_PROC_PATHNAME;
    aiName[3] = getpid();

    size_t cbPath = sizeof(g_szSupLibHardenedExePath);
    if (sysctl(aiName, RT_ELEMENTS(aiName), g_szSupLibHardenedExePath, &cbPath, NULL, 0) < 0)
        supR3HardenedFatal("supR3HardenedExecDir: sysctl failed\n");
    g_szSupLibHardenedExePath[sizeof(g_szSupLibHardenedExePath) - 1] = '\0';
    int cchLink = suplibHardenedStrLen(g_szSupLibHardenedExePath); /* paranoid? can't we use cbPath? */

# endif
    if (cchLink < 0 || cchLink == sizeof(g_szSupLibHardenedExePath) - 1)
        supR3HardenedFatal("supR3HardenedExecDir: couldn't read \"%s\", errno=%d cchLink=%d\n",
                            g_szSupLibHardenedExePath, errno, cchLink);
    g_szSupLibHardenedExePath[cchLink] = '\0';

#elif defined(RT_OS_OS2) || defined(RT_OS_L4)
    _execname(g_szSupLibHardenedExePath, sizeof(g_szSupLibHardenedExePath));

#elif defined(RT_OS_DARWIN)
    const char *pszImageName = _dyld_get_image_name(0);
    if (!pszImageName)
        supR3HardenedFatal("supR3HardenedExecDir: _dyld_get_image_name(0) failed\n");
    size_t cchImageName = suplibHardenedStrLen(pszImageName);
    if (!cchImageName || cchImageName >= sizeof(g_szSupLibHardenedExePath))
        supR3HardenedFatal("supR3HardenedExecDir: _dyld_get_image_name(0) failed, cchImageName=%d\n", cchImageName);
    suplibHardenedMemCopy(g_szSupLibHardenedExePath, pszImageName, cchImageName + 1);

#elif defined(RT_OS_WINDOWS)
    char *pszDst = g_szSupLibHardenedExePath;
    int rc = RTUtf16ToUtf8Ex(g_wszSupLibHardenedExePath, RTSTR_MAX, &pszDst, sizeof(g_szSupLibHardenedExePath), NULL);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3HardenedExecDir: RTUtf16ToUtf8Ex failed, rc=%Rrc\n", rc);
#else
# error needs porting.
#endif

    /*
     * Strip off the filename part (RTPathStripFilename()).
     */
    suplibHardenedStrCopy(g_szSupLibHardenedDirPath, g_szSupLibHardenedExePath);
    suplibHardenedPathStripFilename(g_szSupLibHardenedDirPath);
}


#ifdef RT_OS_LINUX
/**
 * Checks if we can read /proc/self/exe.
 *
 * This is used on linux to see if we have to call init
 * with program path or not.
 *
 * @returns true / false.
 */
static bool supR3HardenedMainIsProcSelfExeAccssible(void)
{
    char szPath[RTPATH_MAX];
    int cchLink = readlink("/proc/self/exe", szPath, sizeof(szPath));
    return cchLink != -1;
}
#endif /* RT_OS_LINUX */



/**
 * @copydoc RTPathExecDir
 */
DECLHIDDEN(int) supR3HardenedPathExecDir(char *pszPath, size_t cchPath)
{
    /*
     * Lazy init (probably not required).
     */
    if (!g_szSupLibHardenedDirPath[0])
        supR3HardenedGetFullExePath();

    /*
     * Calc the length and check if there is space before copying.
     */
    size_t cch = suplibHardenedStrLen(g_szSupLibHardenedDirPath) + 1;
    if (cch <= cchPath)
    {
        suplibHardenedMemCopy(pszPath, g_szSupLibHardenedDirPath, cch + 1);
        return VINF_SUCCESS;
    }

    supR3HardenedFatal("supR3HardenedPathExecDir: Buffer too small (%u < %u)\n", cchPath, cch);
    return VERR_BUFFER_OVERFLOW;
}


#ifdef RT_OS_WINDOWS
extern "C" uint32_t g_uNtVerCombined;
#endif

DECLHIDDEN(void) supR3HardenedOpenLog(int *pcArgs, char **papszArgs)
{
    static const char s_szLogOption[] = "--sup-startup-log=";

    /*
     * Scan the argument vector.
     */
    int cArgs = *pcArgs;
    for (int iArg = 1; iArg < cArgs; iArg++)
        if (strncmp(papszArgs[iArg], s_szLogOption, sizeof(s_szLogOption) - 1) == 0)
        {
            const char *pszLogFile = &papszArgs[iArg][sizeof(s_szLogOption) - 1];

            /*
             * Drop the argument from the vector (has trailing NULL entry).
             */
            memmove(&papszArgs[iArg], &papszArgs[iArg + 1], (cArgs - iArg) * sizeof(papszArgs[0]));
            *pcArgs -= 1;
            cArgs   -= 1;

            /*
             * Open the log file, unless we've already opened one.
             * First argument takes precedence
             */
#ifdef RT_OS_WINDOWS
            if (g_hStartupLog == NULL)
            {
                int rc = RTNtPathOpen(pszLogFile,
                                      GENERIC_WRITE | SYNCHRONIZE,
                                      FILE_ATTRIBUTE_NORMAL,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      FILE_OPEN_IF,
                                      FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                      OBJ_CASE_INSENSITIVE,
                                      &g_hStartupLog,
                                      NULL);
                if (RT_SUCCESS(rc))
                    SUP_DPRINTF(("Log file opened: " VBOX_VERSION_STRING "r%u g_hStartupLog=%p g_uNtVerCombined=%#x\n",
                                 VBOX_SVN_REV, g_hStartupLog, g_uNtVerCombined));
                else
                    g_hStartupLog = NULL;
            }
#else
            //g_hStartupLog = open()
#endif
        }
}


DECLHIDDEN(void) supR3HardenedLogV(const char *pszFormat, va_list va)
{
#ifdef RT_OS_WINDOWS
    if (   g_hStartupLog != NULL
        && g_cbStartupLog < 128*_1M)
    {
        char szBuf[5120];
        PCLIENT_ID pSelfId = &((PTEB)NtCurrentTeb())->ClientId;
        size_t cchPrefix = RTStrPrintf(szBuf, sizeof(szBuf), "%x.%x: ", pSelfId->UniqueProcess, pSelfId->UniqueThread);
        size_t cch = RTStrPrintfV(&szBuf[cchPrefix], sizeof(szBuf) - cchPrefix, pszFormat, va) + cchPrefix;

        if ((size_t)cch >= sizeof(szBuf))
            cch = sizeof(szBuf) - 1;

        if (!cch || szBuf[cch - 1] != '\n')
            szBuf[cch++] = '\n';

        ASMAtomicAddU32(&g_cbStartupLog, (uint32_t)cch);

        IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        LARGE_INTEGER   Offset;
        Offset.QuadPart = -1; /* Write to end of file. */
        NtWriteFile(g_hStartupLog, NULL /*Event*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                    &Ios, szBuf, (ULONG)cch, &Offset, NULL /*Key*/);
    }
#else
    /* later */
#endif
}


DECLHIDDEN(void) supR3HardenedLog(const char *pszFormat,  ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedLogV(pszFormat, va);
    va_end(va);
}


/**
 * Prints the message prefix.
 */
static void suplibHardenedPrintPrefix(void)
{
    if (g_pszSupLibHardenedProgName)
        suplibHardenedPrintStr(g_pszSupLibHardenedProgName);
    suplibHardenedPrintStr(": ");
}


DECLHIDDEN(void)   supR3HardenedFatalMsgV(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, va_list va)
{
    /*
     * First to the log.
     */
    supR3HardenedLog("Error %d in %s! (enmWhat=%d)\n", rc, pszWhere, enmWhat);
    va_list vaCopy;
    va_copy(vaCopy, va);
    supR3HardenedLogV(pszMsgFmt, vaCopy);
    va_end(vaCopy);

    /*
     * Then to the console.
     */
    suplibHardenedPrintPrefix();
    suplibHardenedPrintF("Error %d in %s!\n", rc, pszWhere);

    suplibHardenedPrintPrefix();
    va_copy(vaCopy, va);
    suplibHardenedPrintFV(pszMsgFmt, vaCopy);
    va_end(vaCopy);
    suplibHardenedPrintChr('\n');

    switch (enmWhat)
    {
        case kSupInitOp_Driver:
            suplibHardenedPrintChr('\n');
            suplibHardenedPrintPrefix();
            suplibHardenedPrintStr("Tip! Make sure the kernel module is loaded. It may also help to reinstall VirtualBox.\n");
            break;

        case kSupInitOp_Misc:
        case kSupInitOp_IPRT:
        case kSupInitOp_Integrity:
        case kSupInitOp_RootCheck:
            suplibHardenedPrintChr('\n');
            suplibHardenedPrintPrefix();
            suplibHardenedPrintStr("Tip! It may help to reinstall VirtualBox.\n");
            break;

        default:
            /* no hints here */
            break;
    }

    /*
     * Don't call TrustedError if it's too early.
     */
    if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
    {
#ifdef SUP_HARDENED_SUID
        /*
         * Drop any root privileges we might be holding, this won't return
         * if it fails but end up calling supR3HardenedFatal[V].
         */
        supR3HardenedMainDropPrivileges();
#endif

        /*
         * Now try resolve and call the TrustedError entry point if we can
         * find it.  We'll fork before we attempt this because that way the
         * session management in main will see us exiting immediately (if
         * it's involved with us).
         */
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
        int pid = fork();
        if (pid <= 0)
#endif
        {
            static volatile bool s_fRecursive = false; /* Loader hooks may cause recursion. */
            if (!s_fRecursive)
            {
                s_fRecursive = true;

                PFNSUPTRUSTEDERROR pfnTrustedError = supR3HardenedMainGetTrustedError(g_pszSupLibHardenedProgName);
                if (pfnTrustedError)
                    pfnTrustedError(pszWhere, enmWhat, rc, pszMsgFmt, va);

                s_fRecursive = false;
            }
        }
    }
#if defined(RT_OS_WINDOWS)
    /*
     * Report the error to the parent if this happens during early VM init.
     */
    else if (   g_enmSupR3HardenedMainState < SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED
             && g_enmSupR3HardenedMainState != SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED)
        supR3HardenedWinReportErrorToParent(pszWhere, enmWhat, rc, pszMsgFmt, va);
#endif

    /*
     * Quit
     */
    suplibHardenedExit(RTEXITCODE_FAILURE);
}


DECLHIDDEN(void)   supR3HardenedFatalMsg(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, ...)
{
    va_list va;
    va_start(va, pszMsgFmt);
    supR3HardenedFatalMsgV(pszWhere, enmWhat, rc, pszMsgFmt, va);
    va_end(va);
}


DECLHIDDEN(void) supR3HardenedFatalV(const char *pszFormat, va_list va)
{
    supR3HardenedLog("Fatal error:\n");
    va_list vaCopy;
    va_copy(vaCopy, va);
    supR3HardenedLogV(pszFormat, vaCopy);
    va_end(vaCopy);

#if defined(RT_OS_WINDOWS)
    /*
     * Report the error to the parent if this happens during early VM init.
     */
    if (   g_enmSupR3HardenedMainState < SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED
        && g_enmSupR3HardenedMainState != SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED)
        supR3HardenedWinReportErrorToParent(NULL, kSupInitOp_Invalid, VERR_INTERNAL_ERROR, pszFormat, va);
    else
#endif
    {
        suplibHardenedPrintPrefix();
        suplibHardenedPrintFV(pszFormat, va);
    }

    suplibHardenedExit(RTEXITCODE_FAILURE);
}


DECLHIDDEN(void) supR3HardenedFatal(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedFatalV(pszFormat, va);
    va_end(va);
}


DECLHIDDEN(int) supR3HardenedErrorV(int rc, bool fFatal, const char *pszFormat, va_list va)
{
    if (fFatal)
        supR3HardenedFatalV(pszFormat, va);

    supR3HardenedLog("Error (rc=%d):\n", rc);
    va_list vaCopy;
    va_copy(vaCopy, va);
    supR3HardenedLogV(pszFormat, vaCopy);
    va_end(vaCopy);

    suplibHardenedPrintPrefix();
    suplibHardenedPrintFV(pszFormat, va);
    return rc;
}


DECLHIDDEN(int) supR3HardenedError(int rc, bool fFatal, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedErrorV(rc, fFatal, pszFormat, va);
    va_end(va);
    return rc;
}



/**
 * Attempts to open /dev/vboxdrv (or equvivalent).
 *
 * @remarks This function will not return on failure.
 */
DECLHIDDEN(void) supR3HardenedMainOpenDevice(void)
{
    int rc = suplibOsInit(&g_SupPreInitData.Data, false /*fPreInit*/, true /*fUnrestricted*/);
    if (RT_SUCCESS(rc))
        return;

    switch (rc)
    {
        /** @todo better messages! */
        case VERR_VM_DRIVER_NOT_INSTALLED:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "Kernel driver not installed");
        case VERR_VM_DRIVER_NOT_ACCESSIBLE:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "Kernel driver not accessible");
        case VERR_VM_DRIVER_LOAD_ERROR:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "VERR_VM_DRIVER_LOAD_ERROR");
        case VERR_VM_DRIVER_OPEN_ERROR:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "VERR_VM_DRIVER_OPEN_ERROR");
        case VERR_VM_DRIVER_VERSION_MISMATCH:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "Kernel driver version mismatch");
        case VERR_ACCESS_DENIED:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "VERR_ACCESS_DENIED");
        case VERR_NO_MEMORY:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "Kernel memory allocation/mapping failed");
        case VERR_SUPDRV_HARDENING_EVIL_HANDLE:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_SUPDRV_HARDENING_EVIL_HANDLE");
        case VERR_SUPLIB_NT_PROCESS_UNTRUSTED_0:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_SUPLIB_NT_PROCESS_UNTRUSTED_0");
        case VERR_SUPLIB_NT_PROCESS_UNTRUSTED_1:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_SUPLIB_NT_PROCESS_UNTRUSTED_1");
        case VERR_SUPLIB_NT_PROCESS_UNTRUSTED_2:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_SUPLIB_NT_PROCESS_UNTRUSTED_2");
        default:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc,
                                  "Unknown rc=%d", rc);
    }
}


#ifdef SUP_HARDENED_SUID

/**
 * Grabs extra non-root capabilities / privileges that we might require.
 *
 * This is currently only used for being able to do ICMP from the NAT engine.
 *
 * @note We still have root privileges at the time of this call.
 */
static void supR3HardenedMainGrabCapabilites(void)
{
# if defined(RT_OS_LINUX)
    /*
     * We are about to drop all our privileges. Remove all capabilities but
     * keep the cap_net_raw capability for ICMP sockets for the NAT stack.
     */
    if (g_uCaps != 0)
    {
#  ifdef USE_LIB_PCAP
        /* XXX cap_net_bind_service */
        if (!cap_set_proc(cap_from_text("all-eip cap_net_raw+ep")))
            prctl(PR_SET_KEEPCAPS, 1 /*keep=*/, 0, 0, 0);
        prctl(PR_SET_DUMPABLE, 1 /*dump*/, 0, 0, 0);
#  else
        cap_user_header_t hdr = (cap_user_header_t)alloca(sizeof(*hdr));
        cap_user_data_t   cap = (cap_user_data_t)alloca(sizeof(*cap));
        memset(hdr, 0, sizeof(*hdr));
        hdr->version = _LINUX_CAPABILITY_VERSION;
        memset(cap, 0, sizeof(*cap));
        cap->effective = g_uCaps;
        cap->permitted = g_uCaps;
        if (!capset(hdr, cap))
            prctl(PR_SET_KEEPCAPS, 1 /*keep*/, 0, 0, 0);
        prctl(PR_SET_DUMPABLE, 1 /*dump*/, 0, 0, 0);
#  endif /* !USE_LIB_PCAP */
    }

# elif defined(RT_OS_SOLARIS)
    /*
     * Add net_icmpaccess privilege to effective privileges and limit
     * permitted privileges before completely dropping root privileges.
     * This requires dropping root privileges temporarily to get the normal
     * user's privileges.
     */
    seteuid(g_uid);
    priv_set_t *pPrivEffective = priv_allocset();
    priv_set_t *pPrivNew = priv_allocset();
    if (pPrivEffective && pPrivNew)
    {
        int rc = getppriv(PRIV_EFFECTIVE, pPrivEffective);
        seteuid(0);
        if (!rc)
        {
            priv_copyset(pPrivEffective, pPrivNew);
            rc = priv_addset(pPrivNew, PRIV_NET_ICMPACCESS);
            if (!rc)
            {
                /* Order is important, as one can't set a privilege which is
                 * not in the permitted privilege set. */
                rc = setppriv(PRIV_SET, PRIV_EFFECTIVE, pPrivNew);
                if (rc)
                    supR3HardenedError(rc, false, "SUPR3HardenedMain: failed to set effective privilege set.\n");
                rc = setppriv(PRIV_SET, PRIV_PERMITTED, pPrivNew);
                if (rc)
                    supR3HardenedError(rc, false, "SUPR3HardenedMain: failed to set permitted privilege set.\n");
            }
            else
                supR3HardenedError(rc, false, "SUPR3HardenedMain: failed to add NET_ICMPACCESS privilege.\n");
        }
    }
    else
    {
        /* for memory allocation failures just continue */
        seteuid(0);
    }

    if (pPrivEffective)
        priv_freeset(pPrivEffective);
    if (pPrivNew)
        priv_freeset(pPrivNew);
# endif
}

/*
 * Look at the environment for some special options.
 */
static void supR3GrabOptions(void)
{
    const char *pszOpt;

# ifdef RT_OS_LINUX
    g_uCaps = 0;

    /*
     * Do _not_ perform any capability-related system calls for root processes
     * (leaving g_uCaps at 0).
     * (Hint: getuid gets the real user id, not the effective.)
     */
    if (getuid() != 0)
    {
        /*
         * CAP_NET_RAW.
         * Default: enabled.
         * Can be disabled with 'export VBOX_HARD_CAP_NET_RAW=0'.
         */
        pszOpt = getenv("VBOX_HARD_CAP_NET_RAW");
        if (   !pszOpt
            || memcmp(pszOpt, "0", sizeof("0")) != 0)
            g_uCaps = CAP_TO_MASK(CAP_NET_RAW);

        /*
         * CAP_NET_BIND_SERVICE.
         * Default: disabled.
         * Can be enabled with 'export VBOX_HARD_CAP_NET_BIND_SERVICE=1'.
         */
        pszOpt = getenv("VBOX_HARD_CAP_NET_BIND_SERVICE");
        if (   pszOpt
            && memcmp(pszOpt, "0", sizeof("0")) != 0)
            g_uCaps |= CAP_TO_MASK(CAP_NET_BIND_SERVICE);
    }
# endif
}

/**
 * Drop any root privileges we might be holding.
 */
static void supR3HardenedMainDropPrivileges(void)
{
    /*
     * Try use setre[ug]id since this will clear the save uid/gid and thus
     * leave fewer traces behind that libs like GTK+ may pick up.
     */
    uid_t euid, ruid, suid;
    gid_t egid, rgid, sgid;
# if defined(RT_OS_DARWIN)
    /* The really great thing here is that setreuid isn't available on
       OS X 10.4, libc emulates it. While 10.4 have a slightly different and
       non-standard setuid implementation compared to 10.5, the following
       works the same way with both version since we're super user (10.5 req).
       The following will set all three variants of the group and user IDs. */
    setgid(g_gid);
    setuid(g_uid);
    euid = geteuid();
    ruid = suid = getuid();
    egid = getegid();
    rgid = sgid = getgid();

# elif defined(RT_OS_SOLARIS)
    /* Solaris doesn't have setresuid, but the setreuid interface is BSD
       compatible and will set the saved uid to euid when we pass it a ruid
       that isn't -1 (which we do). */
    setregid(g_gid, g_gid);
    setreuid(g_uid, g_uid);
    euid = geteuid();
    ruid = suid = getuid();
    egid = getegid();
    rgid = sgid = getgid();

# else
    /* This is the preferred one, full control no questions about semantics.
       PORTME: If this isn't work, try join one of two other gangs above. */
    setresgid(g_gid, g_gid, g_gid);
    setresuid(g_uid, g_uid, g_uid);
    if (getresuid(&ruid, &euid, &suid) != 0)
    {
        euid = geteuid();
        ruid = suid = getuid();
    }
    if (getresgid(&rgid, &egid, &sgid) != 0)
    {
        egid = getegid();
        rgid = sgid = getgid();
    }
# endif


    /* Check that it worked out all right. */
    if (    euid != g_uid
        ||  ruid != g_uid
        ||  suid != g_uid
        ||  egid != g_gid
        ||  rgid != g_gid
        ||  sgid != g_gid)
        supR3HardenedFatal("SUPR3HardenedMain: failed to drop root privileges!"
                           " (euid=%d ruid=%d suid=%d  egid=%d rgid=%d sgid=%d; wanted uid=%d and gid=%d)\n",
                           euid, ruid, suid, egid, rgid, sgid, g_uid, g_gid);

# if RT_OS_LINUX
    /*
     * Re-enable the cap_net_raw capability which was disabled during setresuid.
     */
    if (g_uCaps != 0)
    {
#  ifdef USE_LIB_PCAP
        /** @todo Warn if that does not work? */
        /* XXX cap_net_bind_service */
        cap_set_proc(cap_from_text("cap_net_raw+ep"));
#  else
        cap_user_header_t hdr = (cap_user_header_t)alloca(sizeof(*hdr));
        cap_user_data_t   cap = (cap_user_data_t)alloca(sizeof(*cap));
        memset(hdr, 0, sizeof(*hdr));
        hdr->version = _LINUX_CAPABILITY_VERSION;
        memset(cap, 0, sizeof(*cap));
        cap->effective = g_uCaps;
        cap->permitted = g_uCaps;
        /** @todo Warn if that does not work? */
        capset(hdr, cap);
#  endif /* !USE_LIB_PCAP */
    }
# endif
}

#endif /* SUP_HARDENED_SUID */

/**
 * Loads the VBoxRT DLL/SO/DYLIB, hands it the open driver,
 * and calls RTR3InitEx.
 *
 * @param   fFlags      The SUPR3HardenedMain fFlags argument, passed to supR3PreInit.
 *
 * @remarks VBoxRT contains both IPRT and SUPR3.
 * @remarks This function will not return on failure.
 */
static void supR3HardenedMainInitRuntime(uint32_t fFlags)
{
    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathSharedLibs(szPath, sizeof(szPath) - sizeof("/VBoxRT" SUPLIB_DLL_SUFF));
    suplibHardenedStrCat(szPath, "/VBoxRT" SUPLIB_DLL_SUFF);

    /*
     * Open it and resolve the symbols.
     */
#if defined(RT_OS_WINDOWS)
    HMODULE hMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, false /*fSystem32Only*/);
    if (!hMod)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_MODULE_NOT_FOUND,
                              "LoadLibrary \"%s\" failed (rc=%d)",
                              szPath, RtlGetLastWin32Error());
    PFNRTR3INITEX pfnRTInitEx = (PFNRTR3INITEX)GetProcAddress(hMod, SUP_HARDENED_SYM("RTR3InitEx"));
    if (!pfnRTInitEx)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"RTR3InitEx\" not found in \"%s\" (rc=%d)",
                              szPath, RtlGetLastWin32Error());

    PFNSUPR3PREINIT pfnSUPPreInit = (PFNSUPR3PREINIT)GetProcAddress(hMod, SUP_HARDENED_SYM("supR3PreInit"));
    if (!pfnSUPPreInit)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"supR3PreInit\" not found in \"%s\" (rc=%d)",
                              szPath, RtlGetLastWin32Error());

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_MODULE_NOT_FOUND,
                              "dlopen(\"%s\",) failed: %s",
                              szPath, dlerror());
    PFNRTR3INITEX pfnRTInitEx = (PFNRTR3INITEX)(uintptr_t)dlsym(pvMod, SUP_HARDENED_SYM("RTR3InitEx"));
    if (!pfnRTInitEx)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"RTR3InitEx\" not found in \"%s\"!\ndlerror: %s",
                              szPath, dlerror());
    PFNSUPR3PREINIT pfnSUPPreInit = (PFNSUPR3PREINIT)(uintptr_t)dlsym(pvMod, SUP_HARDENED_SYM("supR3PreInit"));
    if (!pfnSUPPreInit)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"supR3PreInit\" not found in \"%s\"!\ndlerror: %s",
                              szPath, dlerror());
#endif

    /*
     * Make the calls.
     */
    supR3HardenedGetPreInitData(&g_SupPreInitData);
    int rc = pfnSUPPreInit(&g_SupPreInitData, fFlags);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, rc,
                              "supR3PreInit failed with rc=%d", rc);
    const char *pszExePath = NULL;
#ifdef RT_OS_LINUX
    if (!supR3HardenedMainIsProcSelfExeAccssible())
        pszExePath = g_szSupLibHardenedExePath;
#endif
    rc = pfnRTInitEx(RTR3INIT_VER_1,
                     fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV ? 0 : RTR3INIT_FLAGS_SUPLIB,
                     0 /*cArgs*/, NULL /*papszArgs*/, pszExePath);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, rc,
                              "RTR3InitEx failed with rc=%d", rc);

#if defined(RT_OS_WINDOWS)
    /*
     * Windows: Create thread that terminates the process when the parent stub
     *          process terminates (VBoxNetDHCP, Ctrl-C, etc).
     */
    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
        supR3HardenedWinCreateParentWatcherThread(hMod);
#endif
}


/**
 * Loads the DLL/SO/DYLIB containing the actual program and
 * resolves the TrustedError symbol.
 *
 * This is very similar to supR3HardenedMainGetTrustedMain().
 *
 * @returns Pointer to the trusted error symbol if it is exported, NULL
 *          and no error messages otherwise.
 * @param   pszProgName     The program name.
 */
static PFNSUPTRUSTEDERROR supR3HardenedMainGetTrustedError(const char *pszProgName)
{
    /*
     * Don't bother if the main() function didn't advertise any TrustedError
     * export.  It's both a waste of time and may trigger additional problems,
     * confusing or obscuring the original issue.
     */
    if (!(g_fSupHardenedMain & SUPSECMAIN_FLAGS_TRUSTED_ERROR))
        return NULL;

    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathAppPrivateArch(szPath, sizeof(szPath) - 10);
    size_t cch = suplibHardenedStrLen(szPath);
    suplibHardenedStrCopyEx(&szPath[cch], sizeof(szPath) - cch, "/", pszProgName, SUPLIB_DLL_SUFF, NULL);

    /*
     * Open it and resolve the symbol.
     */
#if defined(RT_OS_WINDOWS)
    supR3HardenedWinEnableThreadCreation();
    HMODULE hMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, false /*fSystem32Only*/);
    if (!hMod)
        return NULL;
    FARPROC pfn = GetProcAddress(hMod, SUP_HARDENED_SYM("TrustedError"));
    if (!pfn)
        return NULL;
    return (PFNSUPTRUSTEDERROR)pfn;

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        return NULL;
    void *pvSym = dlsym(pvMod, SUP_HARDENED_SYM("TrustedError"));
    if (!pvSym)
        return NULL;
    return (PFNSUPTRUSTEDERROR)(uintptr_t)pvSym;
#endif
}


/**
 * Loads the DLL/SO/DYLIB containing the actual program and
 * resolves the TrustedMain symbol.
 *
 * @returns Pointer to the trusted main of the actual program.
 * @param   pszProgName     The program name.
 * @remarks This function will not return on failure.
 */
static PFNSUPTRUSTEDMAIN supR3HardenedMainGetTrustedMain(const char *pszProgName)
{
    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathAppPrivateArch(szPath, sizeof(szPath) - 10);
    size_t cch = suplibHardenedStrLen(szPath);
    suplibHardenedStrCopyEx(&szPath[cch], sizeof(szPath) - cch, "/", pszProgName, SUPLIB_DLL_SUFF, NULL);

    /*
     * Open it and resolve the symbol.
     */
#if defined(RT_OS_WINDOWS)
    HMODULE hMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, false /*fSystem32Only*/);
    if (!hMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: LoadLibrary \"%s\" failed, rc=%d\n",
                            szPath, RtlGetLastWin32Error());
    FARPROC pfn = GetProcAddress(hMod, SUP_HARDENED_SYM("TrustedMain"));
    if (!pfn)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"TrustedMain\" not found in \"%s\" (rc=%d)\n",
                            szPath, RtlGetLastWin32Error());
    return (PFNSUPTRUSTEDMAIN)pfn;

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: dlopen(\"%s\",) failed: %s\n",
                            szPath, dlerror());
    void *pvSym = dlsym(pvMod, SUP_HARDENED_SYM("TrustedMain"));
    if (!pvSym)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"TrustedMain\" not found in \"%s\"!\ndlerror: %s\n",
                            szPath, dlerror());
    return (PFNSUPTRUSTEDMAIN)(uintptr_t)pvSym;
#endif
}


/**
 * Secure main.
 *
 * This is used for the set-user-ID-on-execute binaries on unixy systems
 * and when using the open-vboxdrv-via-root-service setup on Windows.
 *
 * This function will perform the integrity checks of the VirtualBox
 * installation, open the support driver, open the root service (later),
 * and load the DLL corresponding to \a pszProgName and execute its main
 * function.
 *
 * @returns Return code appropriate for main().
 *
 * @param   pszProgName     The program name. This will be used to figure out which
 *                          DLL/SO/DYLIB to load and execute.
 * @param   fFlags          Flags.
 * @param   argc            The argument count.
 * @param   argv            The argument vector.
 * @param   envp            The environment vector.
 */
DECLHIDDEN(int) SUPR3HardenedMain(const char *pszProgName, uint32_t fFlags, int argc, char **argv, char **envp)
{
    SUP_DPRINTF(("SUPR3HardenedMain: pszProgName=%s fFlags=%#x\n", pszProgName, fFlags));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_HARDENED_MAIN_CALLED;

    /*
     * Note! At this point there is no IPRT, so we will have to stick
     * to basic CRT functions that everyone agree upon.
     */
    g_pszSupLibHardenedProgName   = pszProgName;
    g_fSupHardenedMain            = fFlags;
    g_SupPreInitData.u32Magic     = SUPPREINITDATA_MAGIC;
    g_SupPreInitData.u32EndMagic  = SUPPREINITDATA_MAGIC;
#ifdef RT_OS_WINDOWS
    if (!g_fSupEarlyProcessInit)
#endif
        g_SupPreInitData.Data.hDevice = SUP_HDEVICE_NIL;

#ifdef SUP_HARDENED_SUID
# ifdef RT_OS_LINUX
    /*
     * On linux we have to make sure the path is initialized because we
     * *might* not be able to access /proc/self/exe after the seteuid call.
     */
    supR3HardenedGetFullExePath();
# endif

    /*
     * Grab any options from the environment.
     */
    supR3GrabOptions();

    /*
     * Check that we're root, if we aren't then the installation is butchered.
     */
    g_uid = getuid();
    g_gid = getgid();
    if (geteuid() != 0 /* root */)
        supR3HardenedFatalMsg("SUPR3HardenedMain", kSupInitOp_RootCheck, VERR_PERMISSION_DENIED,
                              "Effective UID is not root (euid=%d egid=%d uid=%d gid=%d)",
                              geteuid(), getegid(), g_uid, g_gid);
#endif /* SUP_HARDENED_SUID */

#ifdef RT_OS_WINDOWS
    /*
     * Windows: First respawn. On Windows we will respawn the process twice to establish
     * something we can put some kind of reliable trust in.  The first respawning aims
     * at dropping compatibility layers and process "security" solutions.
     */
    if (   !g_fSupEarlyProcessInit
        && !(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV)
        && supR3HardenedWinIsReSpawnNeeded(1 /*iWhich*/, argc, argv))
    {
        SUP_DPRINTF(("SUPR3HardenedMain: Respawn #1\n"));
        supR3HardenedWinInit(SUPSECMAIN_FLAGS_DONT_OPEN_DEV, false /*fAvastKludge*/);
        supR3HardenedVerifyAll(true /* fFatal */, pszProgName);
        return supR3HardenedWinReSpawn(1 /*iWhich*/);
    }

    /*
     * Windows: Initialize the image verification global data so we can verify the
     * signature of the process image and hook the core of the DLL loader API so we
     * can check the signature of all DLLs mapped into the process.  (Already done
     * by early VM process init.)
     */
    if (!g_fSupEarlyProcessInit)
        supR3HardenedWinInit(fFlags, true /*fAvastKludge*/);
#endif /* RT_OS_WINDOWS */

    /*
     * Validate the installation.
     */
    supR3HardenedVerifyAll(true /* fFatal */, pszProgName);

    /*
     * The next steps are only taken if we actually need to access the support
     * driver.  (Already done by early process init.)
     */
    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
    {
#ifdef RT_OS_WINDOWS
        /*
         * Windows: Must have done early process init if we get here.
         */
        if (!g_fSupEarlyProcessInit)
            supR3HardenedFatalMsg("SUPR3HardenedMain", kSupInitOp_Integrity, VERR_WRONG_ORDER,
                                  "Early process init was somehow skipped.");

        /*
         * Windows: The second respawn.  This time we make a special arrangement
         * with vboxdrv to monitor access to the new process from its inception.
         */
        if (supR3HardenedWinIsReSpawnNeeded(2 /* iWhich*/, argc, argv))
        {
            SUP_DPRINTF(("SUPR3HardenedMain: Respawn #2\n"));
            return supR3HardenedWinReSpawn(2 /* iWhich*/);
        }
        SUP_DPRINTF(("SUPR3HardenedMain: Final process, opening VBoxDrv...\n"));
        supR3HardenedWinFlushLoaderCache();

#else
        /*
         * Open the vboxdrv device.
         */
        supR3HardenedMainOpenDevice();
#endif /* !RT_OS_WINDOWS */
    }

#ifdef RT_OS_WINDOWS
    /*
     * Windows: Enable the use of windows APIs to verify images at load time.
     */
    supR3HardenedWinEnableThreadCreation();
    supR3HardenedWinFlushLoaderCache();
    supR3HardenedWinResolveVerifyTrustApiAndHookThreadCreation(g_pszSupLibHardenedProgName);
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_VERIFY_TRUST_READY;
#endif

#ifdef SUP_HARDENED_SUID
    /*
     * Grab additional capabilities / privileges.
     */
    supR3HardenedMainGrabCapabilites();

    /*
     * Drop any root privileges we might be holding (won't return on failure)
     */
    supR3HardenedMainDropPrivileges();
#endif

    /*
     * Load the IPRT, hand the SUPLib part the open driver and
     * call RTR3InitEx.
     */
    SUP_DPRINTF(("SUPR3HardenedMain: Load Runtime...\n"));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_INIT_RUNTIME;
    supR3HardenedMainInitRuntime(fFlags);

    /*
     * Load the DLL/SO/DYLIB containing the actual program
     * and pass control to it.
     */
    SUP_DPRINTF(("SUPR3HardenedMain: Load TrustedMain...\n"));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_GET_TRUSTED_MAIN;
    PFNSUPTRUSTEDMAIN pfnTrustedMain = supR3HardenedMainGetTrustedMain(pszProgName);

    SUP_DPRINTF(("SUPR3HardenedMain: Calling TrustedMain (%p)...\n", pfnTrustedMain));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_CALLED_TRUSTED_MAIN;
    return pfnTrustedMain(argc, argv, envp);
}

