/* $Id$ */
/** @file
 * IPRT - No-CRT - minimal stream implementation
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/stream.h>

#include <iprt/nt/nt-and-windows.h>

#include <iprt/ctype.h>
#include <iprt/string.h>



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct PRINTFBUF
{
    HANDLE  hHandle;
    size_t  offBuf;
    char    szBuf[128];
} PRINTFBUF;

struct RTSTREAM
{
    int     iStream;
    HANDLE  hHandle;
};


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
RTSTREAM g_aStdStreams[3] =
{
    { 0, NULL },
    { 1, NULL },
    { 2, NULL },
};

RTSTREAM *g_pStdIn  = &g_aStdStreams[0];
RTSTREAM *g_pStdOut = &g_aStdStreams[1];
RTSTREAM *g_pStdErr = &g_aStdStreams[2];



DECLHIDDEN(void) InitStdHandles(PRTL_USER_PROCESS_PARAMETERS pParams)
{
    if (pParams)
    {
        g_pStdIn->hHandle  = pParams->StandardInput;
        g_pStdOut->hHandle = pParams->StandardOutput;
        g_pStdErr->hHandle = pParams->StandardError;
    }
}


static void FlushPrintfBuffer(PRINTFBUF *pBuf)
{
    if (pBuf->offBuf)
    {
        DWORD cbWritten = 0;
        WriteFile(pBuf->hHandle, pBuf->szBuf, (DWORD)pBuf->offBuf, &cbWritten, NULL);
        pBuf->offBuf   = 0;
        pBuf->szBuf[0] = '\0';
    }
}


/** @callback_method_impl{FNRTSTROUTPUT} */
static DECLCALLBACK(size_t) MyPrintfOutputter(void *pvArg, const char *pachChars, size_t cbChars)
{
    PRINTFBUF *pBuf = (PRINTFBUF *)pvArg;
    if (cbChars != 0)
    {
        size_t offSrc = 0;
        while  (offSrc < cbChars)
        {
            size_t cbLeft = sizeof(pBuf->szBuf) - pBuf->offBuf - 1;
            if (cbLeft > 0)
            {
                size_t cbToCopy = RT_MIN(cbChars - offSrc, cbLeft);
                memcpy(&pBuf->szBuf[pBuf->offBuf], &pachChars[offSrc], cbToCopy);
                pBuf->offBuf += cbToCopy;
                pBuf->szBuf[pBuf->offBuf] = '\0';
                if (cbLeft > cbToCopy)
                    break;
                offSrc += cbToCopy;
            }
            FlushPrintfBuffer(pBuf);
        }
    }
    else /* Special zero byte write at the end of the formatting. */
        FlushPrintfBuffer(pBuf);
    return cbChars;
}


RTR3DECL(int) RTStrmPrintfV(PRTSTREAM pStream, const char *pszFormat, va_list args)
{
    PRINTFBUF Buf;
    Buf.hHandle  = pStream->hHandle;
    Buf.offBuf   = 0;
    Buf.szBuf[0] = '\0';

    return (int)RTStrFormatV(MyPrintfOutputter, &Buf, NULL, NULL, pszFormat, args);
}


RTR3DECL(int) RTStrmPrintf(PRTSTREAM pStream, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rc = RTStrmPrintfV(pStream, pszFormat, args);
    va_end(args);
    return rc;
}


RTR3DECL(int) RTPrintfV(const char *pszFormat, va_list va)
{
    PRINTFBUF Buf;
    Buf.hHandle  = g_pStdOut->hHandle;
    Buf.offBuf   = 0;
    Buf.szBuf[0] = '\0';

    return (int)RTStrFormatV(MyPrintfOutputter, &Buf, NULL, NULL, pszFormat, va);
}


RTR3DECL(int) RTPrintf(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = RTPrintfV(pszFormat, va);
    va_end(va);
    return rc;
}

