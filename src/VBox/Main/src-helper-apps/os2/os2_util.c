/* $Id$ */
/** @file
 * Os2Util - Unattended Installation Helper Utility for OS/2.
 *
 * Helps TEE'ing the installation script output to VBox.log and guest side log
 * files.  Also helps with displaying program exit codes, something CMD.exe can't.
 */

/*
 * Copyright (C) 2015-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define INCL_BASE
#include <os2.h>
#include <iprt/asm-amd64-x86.h>
#include <VBox/log.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define IS_BLANK(ch)  ((ch) == ' ' || (ch) == '\t' || (ch) == '\r' || (ch) == '\n')


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to buffered output. */
typedef struct MYBUFFER __far *PMYBUFFER;

/** Buffered output. */
typedef struct MYBUFFER
{
    PMYBUFFER pNext;
    USHORT    cb;
    USHORT    off;
    CHAR      sz[65536 - sizeof(USHORT) * 2 - sizeof(PMYBUFFER) - 2];
} MYBUFFER;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
void __far VBoxBackdoorPrint(PSZ psz, unsigned cch);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static HFILE        g_hStdOut = 1;
static HFILE        g_hStdErr = 2;
static BOOL         g_fOutputToBackdoor = FALSE;
static USHORT       g_cBuffers = 0;
static PMYBUFFER    g_pBufferHead = NULL;
static PMYBUFFER    g_pBufferTail = NULL;



/** strlen-like function. */
static unsigned MyStrLen(PSZ psz)
{
    unsigned cch = 0;
    while (psz[cch] != '\0')
        cch++;
    return cch;
}


/** memcpy-like function.   */
static void *MyMemCopy(void __far *pvDst, void const __far *pvSrc, USHORT cb)
{
    BYTE __far       *pbDst = (BYTE __far *)pvDst;
    BYTE const __far *pbSrc = (BYTE const __far *)pvSrc;
    while (cb-- > 0)
        *pbDst++ = *pbSrc++;
    return pvDst;
}


static void MyOutStr(PSZ psz)
{
    unsigned const cch = MyStrLen(psz);
    USHORT         usIgnored;
    DosWrite(g_hStdErr, psz, cch, &usIgnored);
    if (g_fOutputToBackdoor)
        VBoxBackdoorPrint(psz, cch);
}


static PSZ MyNumToString(PSZ pszBuf, unsigned uNum)
{
    /* Convert to decimal and inverted digit order:  */
    char     szTmp[32];
    unsigned off = 0;
    do
    {
        szTmp[off++] = uNum % 10 + '0';
        uNum /= 10;
    } while (uNum);

    /* Copy it out to the destination buffer in the right order and add a terminator: */
    while (off-- > 0)
        *pszBuf++ = szTmp[off];
    *pszBuf = '\0';
    return pszBuf;
}


static void MyOutNum(unsigned uNum)
{
    char szTmp[32];
    MyNumToString(szTmp, uNum);
    MyOutStr(szTmp);
}


static void MyApiErrorAndQuit(PSZ pszOperation, USHORT rc)
{
    MyOutStr("Os2Util: error: ");
    MyOutStr(pszOperation);
    MyOutStr(" failed: ");
    MyOutNum(rc);
    MyOutStr("\r\n");
    DosExit(EXIT_PROCESS, 0);
}


static HFILE OpenTeeFile(PSZ pszTeeToFile, BOOL fAppend, PSZ pszToWrite, USHORT cchToWrite)
{
    PMYBUFFER pBuf, pNext;
    USHORT    usIgnored;
    USHORT    usAction = 0;
    HFILE     hFile    = -1;
    USHORT    rc;
    rc = DosOpen(pszTeeToFile, &hFile, &usAction, 0 /*cbInitial*/, 0 /*fFileAttribs*/,
                  OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYNONE | OPEN_FLAGS_NOINHERIT | OPEN_FLAGS_SEQUENTIAL, 0 /*Reserved*/);
    if (rc == NO_ERROR)
    {

        if (fAppend)
        {
            ULONG offNew = 0;
            DosChgFilePtr(hFile, 0, FILE_END, &offNew);
        }

        /*
         * Write out buffered data
         */
        pBuf = g_pBufferHead;
        while (pBuf)
        {
            do
                rc = DosWrite(hFile, pBuf->sz, pBuf->off, &usIgnored);
            while (rc == ERROR_INTERRUPT);
            pNext = pBuf->pNext;
            DosFreeSeg((__segment)pBuf);
            pBuf = pNext;
        }
        g_pBufferTail = g_pBufferHead = NULL;

        /*
         * Write the current output.
         */
        do
            rc = DosWrite(hFile, pszToWrite, cchToWrite, &usIgnored);
        while (rc == ERROR_INTERRUPT);
    }
    else
    {
        /*
         * Failed to open the file.  Buffer a bit in case the file can be
         * opened later (like when we've formatted the disk).
         */
        pBuf = g_pBufferTail;
        if (pBuf && pBuf->off < pBuf->cb)
        {
            USHORT cbToCopy = pBuf->cb - pBuf->off;
            if (cbToCopy > cchToWrite)
                cbToCopy = cchToWrite;
            MyMemCopy(&pBuf->sz[pBuf->off], pszToWrite, cbToCopy);
            pszToWrite += cbToCopy;
            cchToWrite -= cbToCopy;
        }
        if (cchToWrite > 0)
        {
            USHORT uSel = 0xffff;
            if (   g_cBuffers < 10
                && (rc = DosAllocSeg(0 /*64KiB*/, &uSel, 0 /*fFlags*/)) == NO_ERROR)
            {
                pBuf = ((__segment)uSel) :> ((MYBUFFER __near *)0);
                pBuf->pNext = NULL;
                pBuf->cb    = sizeof(pBuf->sz);
                pBuf->off   = cchToWrite;
                MyMemCopy(&pBuf->sz[0], pszToWrite, cchToWrite);

                if (g_pBufferTail)
                    g_pBufferTail->pNext = pBuf;
                else
                    g_pBufferHead = pBuf;
                g_pBufferTail = pBuf;
            }
            else if (g_cBuffers > 0)
            {
                pBuf = g_pBufferHead;
                pBuf->off = cchToWrite;
                MyMemCopy(&pBuf->sz[0], pszToWrite, cchToWrite);

                if (g_pBufferTail != pBuf)
                {
                    g_pBufferHead = pBuf->pNext;
                    pBuf->pNext = NULL;
                    g_pBufferTail->pNext = pBuf;
                    g_pBufferTail = pBuf;
                }
            }
        }
        hFile = -1;
    }
    return hFile;
}


static void ShowUsageAndQuit(void)
{
    static char s_szHelp[] =
        "Os2Util.exe is tiny helper utility that implements TEE'ing to the VBox release log,\r\n"
        "files and shows the actual exit code of a program.  Standard error and output will\r\n"
        "be merged into one for simplicity reasons.\r\n"
        "\r\n"
        "usage: Os2Util.exe [-a|--append] [-f<filename>|--tee-to-file <filename>] \\\r\n"
        "                   [-b|--tee-to-backdoor] -- <prog> [args]\r\n"
        "   or  Os2Util.exe <-w<msg>|--write-backdoor <msg>> \r\n";
    USHORT usIgnored;
    DosWrite(g_hStdErr, s_szHelp, sizeof(s_szHelp) - 1, &usIgnored);
    DosExit(EXIT_PROCESS, 0);
}


/**
 * Gets the an option value.
 *
 * The option value string will be terminated.
 */
static PSZ MyGetOptValue(PSZ psz, PSZ pszOption, PSZ *ppszValue)
{
    CHAR ch;
    while ((ch = *psz) != '\0' && IS_BLANK(ch))
        psz++;
    if (*psz == '\0')
    {
        USHORT usIgnored;
        DosWrite(g_hStdErr, RT_STR_TUPLE("Os2Util: syntax error: Option '"), &usIgnored);
        DosWrite(g_hStdErr, pszOption, MyStrLen(pszOption), &usIgnored);
        DosWrite(g_hStdErr, RT_STR_TUPLE("' takes a value\r\n"), &usIgnored);
        DosExit(EXIT_PROCESS, 2);
    }

    *ppszValue = psz;

    while ((ch = *psz) != '\0' && !IS_BLANK(ch))
        psz++;
    if (ch != '\0')
        *psz++ = '\0';
    return psz;
}


/**
 * Checks if @a pszOption matches @a *ppsz, advance *ppsz if TRUE.
 */
static BOOL MyMatchLongOption(PSZ _far *ppsz, PSZ pszOption, unsigned cchOption)
{
    /* Match option and command line strings: */
    PSZ psz = *ppsz;
    while (cchOption-- > 0)
    {
        if (*psz != *pszOption)
            return FALSE;
        psz++;
        pszOption++;
    }

    /* Is this the end of a word on the command line? */
    if (*psz == '\0')
        *ppsz = psz;
    else if (IS_BLANK(*psz))
        *ppsz = psz + 1;
    else
        return FALSE;
    return TRUE;
}


/**
 * The entrypoint (no crt).
 */
#pragma aux Os2UtilMain "_*" parm caller [ ax ] [ bx ];
void Os2UtilMain(USHORT uSelEnv, USHORT offCmdLine)
{
    PSZ         pszzEnv        = ((__segment)uSelEnv) :> ((char _near *)0);
    PSZ         pszzCmdLine    = ((__segment)uSelEnv) :> ((char _near *)offCmdLine);
    USHORT      uExitCode      = 1;
    BOOL        fTeeToBackdoor = FALSE;
    BOOL        fAppend        = FALSE;
    PSZ         pszTeeToFile   = NULL;
    HFILE       hTeeToFile     = -1;
    HFILE       hPipeRead      = -1;
    PSZ         pszzNewCmdLine;
    PSZ         psz;
    CHAR        ch;
    USHORT      usIgnored;
    USHORT      rc;
    RESULTCODES ResultCodes    = { 0xffff, 0xffff };
    CHAR        szBuf[512];

    /*
     * Parse the command line.
     * Note! We do not accept any kind of quoting.
     */
    /* Skip the executable filename: */
    psz = pszzCmdLine;
    while (*psz != '\0')
        psz++;
    psz++;

    /* Now parse arguments. */
    while ((ch = *psz) != '\0')
    {
        if (IS_BLANK(ch))
            psz++;
        else if (ch != '-')
            break;
        else
        {
            PSZ const pszOptStart = psz;
            ch = *++psz;
            if (ch == '-')
            {
                ch = *++psz;
                if (IS_BLANK(ch))
                {
                    /* Found end-of-arguments marker "--" */
                    psz++;
                    break;
                }
                if (ch == 'a' && MyMatchLongOption(&psz, RT_STR_TUPLE("append")))
                    fAppend = TRUE;
                else if (ch == 'h' && MyMatchLongOption(&psz, RT_STR_TUPLE("help")))
                    ShowUsageAndQuit();
                else if (ch == 't' && MyMatchLongOption(&psz, RT_STR_TUPLE("tee-to-backdoor")))
                    g_fOutputToBackdoor = fTeeToBackdoor = TRUE;
                else if (ch == 't' && MyMatchLongOption(&psz, RT_STR_TUPLE("tee-to-file")))
                    psz = MyGetOptValue(psz, "--tee-to-file", &pszTeeToFile);
                else if (ch == 'w' && MyMatchLongOption(&psz, RT_STR_TUPLE("write-backdoor")))
                {
                    VBoxBackdoorPrint(psz, MyStrLen(psz));
                    VBoxBackdoorPrint("\n", 1);
                    DosExit(EXIT_PROCESS, 0);
                }
                else
                {
                    DosWrite(g_hStdErr, RT_STR_TUPLE("Os2util: syntax error: "), &usIgnored);
                    DosWrite(g_hStdErr, pszOptStart, MyStrLen(pszOptStart), &usIgnored);
                    DosWrite(g_hStdErr, RT_STR_TUPLE("\r\n"), &usIgnored);
                    DosExit(EXIT_PROCESS, 2);
                }
            }
            else
            {
                do
                {
                    if (ch == 'a')
                        fAppend = TRUE;
                    else if (ch == 'b')
                        g_fOutputToBackdoor = fTeeToBackdoor = TRUE;
                    else if (ch == 'f')
                    {
                        psz = MyGetOptValue(psz + 1, "-f", &pszTeeToFile);
                        break;
                    }
                    else if (ch == 'w')
                    {
                        psz++;
                        VBoxBackdoorPrint(psz, MyStrLen(psz));
                        VBoxBackdoorPrint("\n", 1);
                        DosExit(EXIT_PROCESS, 0);
                    }
                    else if (ch == '?' || ch == 'h' || ch == 'H')
                        ShowUsageAndQuit();
                    else
                    {
                        DosWrite(g_hStdErr, RT_STR_TUPLE("Os2util: syntax error: "), &usIgnored);
                        if (ch)
                            DosWrite(g_hStdErr, &ch, 1, &usIgnored);
                        else
                            DosWrite(g_hStdErr, RT_STR_TUPLE("lone dash"), &usIgnored);
                        DosWrite(g_hStdErr, RT_STR_TUPLE(" ("), &usIgnored);
                        DosWrite(g_hStdErr, pszOptStart, MyStrLen(pszOptStart), &usIgnored);
                        DosWrite(g_hStdErr, RT_STR_TUPLE(")\r\n"), &usIgnored);
                        DosExit(EXIT_PROCESS, 2);
                    }
                    ch = *++psz;
                } while (!IS_BLANK(ch) && ch != '\0');
            }
        }
    }

    /*
     * Zero terminate the executable name in the command line.
     */
    pszzNewCmdLine = psz;
    if (ch == '\0')
    {
        DosWrite(g_hStdErr, RT_STR_TUPLE("Os2Util: syntax error: No program specified\r\n"), &usIgnored);
        DosExit(EXIT_PROCESS, 2);
    }
    psz++;
    while ((ch = *psz) != '\0' && !IS_BLANK(ch))
        psz++;
    *psz++ = '\0';

    /*
     * Prepare redirection.
     */
    if (fTeeToBackdoor || pszTeeToFile != NULL)
    {
        HFILE   hPipeWrite = -1;
        HFILE   hDup;

        /* Make new copies of the standard handles. */
        hDup = 0xffff;
        rc = DosDupHandle(g_hStdErr, &hDup);
        if (rc != NO_ERROR)
            MyApiErrorAndQuit("DosDupHandle(g_hStdErr, &hDup);", rc);
        g_hStdErr = hDup;
        DosSetFHandState(hDup, OPEN_FLAGS_NOINHERIT); /* not strictly necessary, so ignore errors */

        hDup = 0xffff;
        rc = DosDupHandle(g_hStdOut, &hDup);
        if (rc != NO_ERROR)
            MyApiErrorAndQuit("DosDupHandle(g_hStdOut, &hDup);", rc);
        g_hStdOut = hDup;
        DosSetFHandState(hDup, OPEN_FLAGS_NOINHERIT); /* not strictly necessary, so ignore errors */

        /* Create the pipe and make the read-end non-inheritable (we'll hang otherwise). */
        rc = DosMakePipe(&hPipeRead, &hPipeWrite, 0 /*default size*/);
        if (rc != NO_ERROR)
            MyApiErrorAndQuit("DosMakePipe", rc);

        rc = DosSetFHandState(hPipeRead, OPEN_FLAGS_NOINHERIT);
        if (rc != NO_ERROR)
            MyApiErrorAndQuit("DosSetFHandState(hPipeRead, OPEN_FLAGS_NOINHERIT)", rc);

        /* Replace standard output and standard error with the write end of the pipe. */
        hDup = 1;
        rc = DosDupHandle(hPipeWrite, &hDup);
        if (rc != NO_ERROR)
            MyApiErrorAndQuit("DosDupHandle(hPipeWrite, &hDup[=1]);", rc);

        hDup = 2;
        rc = DosDupHandle(hPipeWrite, &hDup);
        if (rc != NO_ERROR)
            MyApiErrorAndQuit("DosDupHandle(hPipeWrite, &hDup[=2]);", rc);

        /* We can close the write end of the pipe as we don't need the original handle any more. */
        DosClose(hPipeWrite);
    }

    /*
     * Execute the program.
     */
    szBuf[0] = '\0';
    rc = DosExecPgm(szBuf, sizeof(szBuf), hPipeRead == -1 ? EXEC_SYNC : EXEC_ASYNCRESULT,
                    pszzNewCmdLine, pszzEnv, &ResultCodes, pszzNewCmdLine);
    if (rc != NO_ERROR)
    {
        MyOutStr("Os2Util: error: DosExecPgm failed for \"");
        MyOutStr(pszzNewCmdLine);
        MyOutStr("\": ");
        MyOutNum(rc);
        if (szBuf[0])
        {
            MyOutStr(" ErrObj=");
            szBuf[sizeof(szBuf) - 1] = '\0';
            MyOutStr(szBuf);
        }
        MyOutStr("\r\n");
        DosExit(EXIT_PROCESS, 1);
    }

    /*
     * Wait for the child process to complete.
     */
    if (hPipeRead != -1)
    {
        /* Save the child pid. */
        PID const pidChild = ResultCodes.codeTerminate;
        PID       pidIgnored;

        /* Close the write handles or we'll hang in the read loop. */
        DosClose(1);
        DosClose(2);

        /* Disable hard error popups (file output to unformatted disks). */
        DosError(2 /* only exceptions */);


        /*
         * Read the pipe and tee it to the desired outputs
         */
        for (;;)
        {
            USHORT cbRead = 0;
            rc = DosRead(hPipeRead, szBuf, sizeof(szBuf), &cbRead);
            if (rc == NO_ERROR)
            {
                if (cbRead == 0)
                    break; /* No more writers. */

                /* Standard output: */
                do
                    rc = DosWrite(g_hStdOut, szBuf, cbRead, &usIgnored);
                while (rc == ERROR_INTERRUPT);

                /* Backdoor: */
                if (fTeeToBackdoor)
                    VBoxBackdoorPrint(psz, cbRead);

                /* File: */
                if (hTeeToFile != -1)
                    do
                        rc = DosWrite(hTeeToFile, szBuf, cbRead, &usIgnored);
                    while (rc == ERROR_INTERRUPT);
                else if (pszTeeToFile != NULL)
                    hTeeToFile = OpenTeeFile(pszTeeToFile, fAppend, szBuf, cbRead);
            }
            else if (rc == ERROR_BROKEN_PIPE)
                break;
            else
            {
                MyOutStr("Os2Util: error: Error reading pipe: ");
                MyOutNum(rc);
                MyOutStr("\r\n");
                break;
            }
        }

        DosClose(hPipeRead);
        rc = DosCwait(DCWA_PROCESS, DCWW_WAIT, &ResultCodes, &pidIgnored, pidChild);
        if (rc != NO_ERROR)
        {
            MyOutStr("Os2Util: error: DosCwait(DCWA_PROCESS,DCWW_WAIT,,,");
            MyOutNum(pidChild);
            MyOutStr(") failed: ");
            MyOutNum(rc);
            MyOutStr("\r\n");
        }
    }

    /*
     * Report the status code and quit.
     */
    MyOutStr("Os2Util: Child: ");
    MyOutStr(pszzNewCmdLine);
    MyOutStr(" ");
    MyOutStr(psz);
    MyOutStr("\r\n"
             "Os2Util: codeTerminate=");
    MyOutNum(ResultCodes.codeTerminate);
    MyOutStr(" codeResult=");
    MyOutNum(ResultCodes.codeResult);
    MyOutStr("\r\n");

    for (;;)
        DosExit(EXIT_PROCESS, ResultCodes.codeTerminate == 0 ? ResultCodes.codeResult : 127);
}


/**
 * Backdoor print function living in an IOPL=2 segment.
 */
#pragma code_seg("IOPL", "CODE")
void __far VBoxBackdoorPrint(PSZ psz, unsigned cch)
{
    ASMOutStrU8(RTLOG_DEBUG_PORT, psz, cch);
}

