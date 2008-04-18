/* $Id$ */
/** @file
 * Incredibly Portable Runtime - I/O Stream.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "internal/magics.h"

#include <stdio.h>
#include <errno.h>

#if defined(RT_OS_LINUX) /* PORTME: check for the _unlocked functions in stdio.h */
#define HAVE_FWRITE_UNLOCKED
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * File stream.
 */
typedef struct RTSTREAM
{
    /** Magic value used to validate the stream. (RTSTREAM_MAGIC) */
    uint32_t            u32Magic;
    /** File stream error. */
    int32_t volatile    i32Error;
    /** Pointer to the LIBC file stream. */
    FILE                *pFile;
} RTSTREAM;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The standard input stream. */
static RTSTREAM    g_StdIn =
{
    RTSTREAM_MAGIC,
    0,
    stdin
};

/** The standard error stream. */
static RTSTREAM    g_StdErr =
{
    RTSTREAM_MAGIC,
    0,
    stderr
};

/** The standard output stream. */
static RTSTREAM    g_StdOut =
{
    RTSTREAM_MAGIC,
    0,
    stdout
};

/** Pointer to the standard input stream. */
RTDATADECL(PRTSTREAM)   g_pStdIn  = &g_StdIn;

/** Pointer to the standard output stream. */
RTDATADECL(PRTSTREAM)   g_pStdErr = &g_StdErr;

/** Pointer to the standard output stream. */
RTDATADECL(PRTSTREAM)   g_pStdOut = &g_StdOut;


/**
 * Opens a file stream.
 *
 * @returns iprt status code.
 * @param   pszFilename     Path to the file to open.
 * @param   pszMode         The open mode. See fopen() standard.
 *                          Format: <a|r|w>[+][b|t]
 * @param   ppStream        Where to store the opened stream.
 */
RTR3DECL(int) RTStrmOpen(const char *pszFilename, const char *pszMode, PRTSTREAM *ppStream)
{
    /*
     * Validate input.
     */
    if (!pszMode || !*pszMode)
    {
        AssertMsgFailed(("No pszMode!\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (!pszFilename)
    {
        AssertMsgFailed(("No pszFilename!\n"));
        return VERR_INVALID_PARAMETER;
    }
    bool fOk = true;
    switch (*pszMode)
    {
        case 'a':
        case 'w':
        case 'r':
            switch (pszMode[1])
            {
                case '\0':
                    break;
                case '+':
                    switch (pszMode[2])
                    {
                        case '\0':
                        //case 't':
                        case 'b':
                            break;
                        default:
                            fOk = false;
                            break;
                    }
                    break;
                //case 't':
                case 'b':
                    break;
                default:
                    fOk = false;
                    break;
            }
            break;
        default:
            fOk = false;
            break;
    }
    if (!fOk)
    {
        AssertMsgFailed(("Invalid pszMode='%s', '<a|r|w>[+][b|t]'\n", pszMode));
        return VINF_SUCCESS;
    }

    /*
     * Allocate the stream handle and try open it.
     */
    PRTSTREAM pStream = (PRTSTREAM)RTMemAlloc(sizeof(*pStream));
    if (pStream)
    {
        pStream->u32Magic = RTSTREAM_MAGIC;
        pStream->i32Error = VINF_SUCCESS;
        pStream->pFile = fopen(pszFilename, pszMode);
        if (pStream->pFile)
        {
            *ppStream = pStream;
            return VINF_SUCCESS;
        }
        return RTErrConvertFromErrno(errno);
    }
    return VERR_NO_MEMORY;
}


/**
 * Opens a file stream.
 *
 * @returns iprt status code.
 * @param   pszMode         The open mode. See fopen() standard.
 *                          Format: <a|r|w>[+][b|t]
 * @param   ppStream        Where to store the opened stream.
 * @param   pszFilenameFmt  Filename path format string.
 * @param   args            Arguments to the format string.
 */
RTR3DECL(int) RTStrmOpenfV(const char *pszMode, PRTSTREAM *ppStream, const char *pszFilenameFmt, va_list args)
{
    int     rc;
    char    szFilename[RTPATH_MAX];
    int     cch = RTStrPrintf(szFilename, sizeof(szFilename), pszFilenameFmt, args);
    if (cch < (int)sizeof(szFilename))
        rc = RTStrmOpen(szFilename, pszMode, ppStream);
    else
    {
        AssertMsgFailed(("The filename is too long cch=%d\n", cch));
        rc = VERR_FILENAME_TOO_LONG;
    }
    return rc;
}


/**
 * Opens a file stream.
 *
 * @returns iprt status code.
 * @param   pszMode         The open mode. See fopen() standard.
 *                          Format: <a|r|w>[+][b|t]
 * @param   ppStream        Where to store the opened stream.
 * @param   pszFilenameFmt  Filename path format string.
 * @param   ...             Arguments to the format string.
 */
RTR3DECL(int) RTStrmOpenf(const char *pszMode, PRTSTREAM *ppStream, const char *pszFilenameFmt, ...)
{
    va_list args;
    va_start(args, pszFilenameFmt);
    int rc = RTStrmOpenfV(pszMode, ppStream, pszFilenameFmt, args);
    va_end(args);
    return rc;
}


/**
 * Closes the specified stream.
 *
 * @returns iprt status code.
 * @param   pStream         The stream to close.
 */
RTR3DECL(int) RTStrmClose(PRTSTREAM pStream)
{
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
    {
        if (!fclose(pStream->pFile))
        {
            pStream->u32Magic = 0xdeaddead;
            pStream->pFile = NULL;
            RTMemFree(pStream);
            return VINF_SUCCESS;
        }
        return RTErrConvertFromErrno(errno);
    }
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        return VERR_INVALID_PARAMETER;
    }
}


/**
 * Get the pending error of the stream.
 *
 * @returns iprt status code. of the stream.
 * @param   pStream         The stream.
 */
RTR3DECL(int) RTStrmError(PRTSTREAM pStream)
{
    int rc;
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
        rc = pStream->i32Error;
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}


/**
 * Clears stream error condition.
 *
 * All stream operations save RTStrmClose and this will fail
 * while an error is asserted on the stream
 *
 * @returns iprt status code.
 * @param   pStream         The stream.
 */
RTR3DECL(int) RTStrmClearError(PRTSTREAM pStream)
{
    int rc;
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
    {
        ASMAtomicXchgS32(&pStream->i32Error, VINF_SUCCESS);
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}


/**
 * Reads from a file stream.
 *
 * @returns iprt status code.
 * @param   pStream         The stream.
 * @param   pvBuf           Where to put the read bits.
 *                          Must be cbRead bytes or more.
 * @param   cbRead          Number of bytes to read.
 * @param   pcbRead         Where to store the number of bytes actually read.
 *                          If NULL cbRead bytes are read or an error is returned.
 */
RTR3DECL(int) RTStrmReadEx(PRTSTREAM pStream, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    int rc;
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
    {
        rc = pStream->i32Error;
        if (RT_SUCCESS(rc))
        {
            if (pcbRead)
            {
                /*
                 * Can do with a partial read.
                 */
                *pcbRead = fread(pvBuf, 1, cbRead, pStream->pFile);
                if (    *pcbRead == cbRead
                    || !ferror(pStream->pFile))
                    return VINF_SUCCESS;
                if (feof(pStream->pFile))
                {
                    if (*pcbRead)
                        return VINF_EOF;
                    rc = VERR_EOF;
                }
                else if (ferror(pStream->pFile))
                    rc = VERR_READ_ERROR;
                else
                {
                    AssertMsgFailed(("This shouldn't happen\n"));
                    rc = VERR_INTERNAL_ERROR;
                }
            }
            else
            {
                /*
                 * Must read it all!
                 */
                if (fread(pvBuf, cbRead, 1, pStream->pFile) == 1)
                    return VINF_SUCCESS;

                /* possible error/eof. */
                if (feof(pStream->pFile))
                    rc = VERR_EOF;
                else if (ferror(pStream->pFile))
                    rc = VERR_READ_ERROR;
                else
                {
                    AssertMsgFailed(("This shouldn't happen\n"));
                    rc = VERR_INTERNAL_ERROR;
                }
            }
            ASMAtomicXchgS32(&pStream->i32Error, rc);
        }
    }
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}


/**
 * Writes to a file stream.
 *
 * @returns iprt status code.
 * @param   pStream         The stream.
 * @param   pvBuf           Where to get the bits to write from.
 * @param   cbWrite         Number of bytes to write.
 * @param   pcbWritten      Where to store the number of bytes actually written.
 *                          If NULL cbWrite bytes are written or an error is returned.
 */
RTR3DECL(int) RTStrmWriteEx(PRTSTREAM pStream, const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    int rc;
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
    {
        rc = pStream->i32Error;
        if (RT_SUCCESS(rc))
        {
            if (pcbWritten)
            {
                *pcbWritten = fwrite(pvBuf, 1, cbWrite, pStream->pFile);
                if (    *pcbWritten == cbWrite
                    ||  !ferror(pStream->pFile))
                    return VINF_SUCCESS;
                rc = VERR_WRITE_ERROR;
            }
            else
            {
                /*
                 * Must read it all!
                 */
                if (fwrite(pvBuf, cbWrite, 1, pStream->pFile) == 1)
                    return VINF_SUCCESS;
                if (!ferror(pStream->pFile))
                    return VINF_SUCCESS; /* WEIRD! But anyway... */

                rc = VERR_WRITE_ERROR;
            }
            ASMAtomicXchgS32(&pStream->i32Error, rc);
        }
    }
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}


/**
 * Reads a character from a file stream.
 *
 * @returns The char as an unsigned char cast to int.
 * @returns -1 on failure.
 * @param   pStream         The stream.
 */
RTR3DECL(int) RTStrmGetCh(PRTSTREAM pStream)
{
    unsigned char ch;
    int rc = RTStrmReadEx(pStream, &ch, 1, NULL);
    if (RT_SUCCESS(rc))
        return ch;
    return -1;
}


/**
 * Writes a character to a file stream.
 *
 * @returns iprt status code.
 * @param   pStream         The stream.
 * @param   ch              The char to write.
 */
RTR3DECL(int) RTStrmPutCh(PRTSTREAM pStream, int ch)
{
    return RTStrmWriteEx(pStream, &ch, 1, NULL);
}


/**
 * Writes a string to a file stream.
 *
 * @returns iprt status code.
 * @param   pStream         The stream.
 * @param   pszString       The string to write.
 *                          No newlines or anything is appended or prepended.
 *                          The terminating '\\0' is not written, of course.
 */
RTR3DECL(int) RTStrmPutStr(PRTSTREAM pStream, const char *pszString)
{
    size_t cch = strlen(pszString);
    return RTStrmWriteEx(pStream, pszString, cch, NULL);
}


/**
 * Reads a line from a file stream.
 * A line ends with a '\\n', '\\0' or the end of the file.
 *
 * @returns iprt status code.
 * @returns VINF_BUFFER_OVERFLOW if the buffer wasn't big enough to read an entire line.
 * @param   pStream         The stream.
 * @param   pszString       Where to store the line.
 *                          The line will *NOT* contain any '\\n'.
 * @param   cchString       The size of the string buffer.
 */
RTR3DECL(int) RTStrmGetLine(PRTSTREAM pStream, char *pszString, size_t cchString)
{
    int rc;
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
    {
        if (pszString && cchString > 1)
        {
            rc = pStream->i32Error;
            if (RT_SUCCESS(rc))
            {
                cchString--;            /* save space for the terminator. */
                #ifdef HAVE_FWRITE_UNLOCKED
                flockfile(pStream->pFile);
                #endif
                for (;;)
                {
                    #ifdef HAVE_FWRITE_UNLOCKED
                    int ch = fgetc_unlocked(pStream->pFile);
                    #else
                    int ch = fgetc(pStream->pFile);
                    #endif
                    if (ch == EOF)
                    {
                        #ifdef HAVE_FWRITE_UNLOCKED
                        if (feof_unlocked(pStream->pFile))
                        #else
                        if (feof(pStream->pFile))
                        #endif
                            break;
                        #ifdef HAVE_FWRITE_UNLOCKED
                        if (ferror_unlocked(pStream->pFile))
                        #else
                        if (ferror(pStream->pFile))
                        #endif
                            rc = VERR_READ_ERROR;
                        else
                        {
                            AssertMsgFailed(("This shouldn't happen\n"));
                            rc = VERR_INTERNAL_ERROR;
                        }
                        break;
                    }
                    if (ch == '\0' || ch == '\n' || ch == '\r')
                        break;
                    *pszString++ = ch;
                    if (--cchString <= 0)
                    {
                        rc = VINF_BUFFER_OVERFLOW;
                        break;
                    }
                }
                #ifdef HAVE_FWRITE_UNLOCKED
                funlockfile(pStream->pFile);
                #endif

                *pszString = '\0';
                if (RT_FAILURE(rc))
                    ASMAtomicXchgS32(&pStream->i32Error, rc);
            }
        }
        else
        {
            AssertMsgFailed(("no buffer or too small buffer!\n"));
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}


/**
 * Flushes a stream.
 *
 * @returns iprt status code.
 * @param   pStream         The stream to flush.
 */
RTR3DECL(int) RTStrmFlush(PRTSTREAM pStream)
{
    if (!fflush(pStream->pFile))
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


/**
 * Output callback.
 *
 * @returns number of bytes written.
 * @param   pvArg       User argument.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cchChars    Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) rtstrmOutput(void *pvArg, const char *pachChars, size_t cchChars)
{
    if (cchChars)
    {
        PRTSTREAM pStream = (PRTSTREAM)pvArg;
        int rc = pStream->i32Error;
        if (RT_SUCCESS(rc))
        {
            #ifdef HAVE_FWRITE_UNLOCKED
            if (fwrite_unlocked(pachChars, cchChars, 1, pStream->pFile) != 1)
            #else
            if (fwrite(pachChars, cchChars, 1, pStream->pFile) != 1)
            #endif
                ASMAtomicXchgS32(&pStream->i32Error, VERR_WRITE_ERROR);
        }
    }
    /* else: ignore termination call. */
    return cchChars;
}


/**
 * Prints a formatted string to the specified stream.
 *
 * @returns Number of bytes printed.
 * @param   pStream         The stream to print to.
 * @param   pszFormat       Incredibly Portable Runtime format string.
 * @param   args            Arguments specified by pszFormat.
 */
RTR3DECL(int) RTStrmPrintfV(PRTSTREAM pStream, const char *pszFormat, va_list args)
{
    int rc;
    if (pStream && pStream->u32Magic == RTSTREAM_MAGIC)
    {
        rc = pStream->i32Error;
        if (RT_SUCCESS(rc))
        {
            /** @todo consider making this thread safe... */
            #ifdef HAVE_FWRITE_UNLOCKED
            flockfile(pStream->pFile);
            rc = RTStrFormatV(rtstrmOutput, pStream, NULL, NULL, pszFormat, args);
            funlockfile(pStream->pFile);
            #else
            rc = RTStrFormatV(rtstrmOutput, pStream, NULL, NULL, pszFormat, args);
            #endif
        }
        else
            rc = -1;
    }
    else
    {
        AssertMsgFailed(("Invalid stream!\n"));
        rc = -1;
    }
    return rc;
}


/**
 * Prints a formatted string to the specified stream.
 *
 * @returns Number of bytes printed.
 * @param   pStream         The stream to print to.
 * @param   pszFormat       Incredibly Portable Runtime format string.
 * @param   ...             Arguments specified by pszFormat.
 */
RTR3DECL(int) RTStrmPrintf(PRTSTREAM pStream, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rc = RTStrmPrintfV(pStream, pszFormat, args);
    va_end(args);
    return rc;
}


/**
 * Prints a formatted string to the standard output stream (g_pStdOut).
 *
 * @returns Number of bytes printed.
 * @param   pszFormat       Incredibly Portable Runtime format string.
 * @param   args            Arguments specified by pszFormat.
 */
RTR3DECL(int) RTPrintfV(const char *pszFormat, va_list args)
{
    return RTStrmPrintfV(g_pStdOut, pszFormat, args);
}


/**
 * Prints a formatted string to the standard output stream (g_pStdOut).
 *
 * @returns Number of bytes printed.
 * @param   pszFormat       Incredibly Portable Runtime format string.
 * @param   ...             Arguments specified by pszFormat.
 */
RTR3DECL(int) RTPrintf(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rc = RTStrmPrintfV(g_pStdOut, pszFormat, args);
    va_end(args);
    return rc;
}

