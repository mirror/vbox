/** $Id$ */
/** @file
 * VBoxOs2AdditionsInstall - Barebone OS/2 Guest Additions Installer.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#include <VBox/version.h>

#include <string.h>
#include <iprt/ctype.h>
#include <iprt/path.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Where the files to install (default: same dir as this program).  */
static CHAR         g_szSrcPath[CCHMAXPATH];
/** The length of g_szSrcPath, including a trailing slash. */
static size_t       g_cchSrcPath                    = 0;
/** The boot drive path, i.e. where Config.kmk & Startup.cmd lives. */
static CHAR         g_szBootDrivePath[CCHMAXPATH]   = "C:\\";
/** The size of the bootdrive path, including a trailing slash. */
static size_t       g_cchBootDrivePath              = sizeof("C:\\") - 1;
/** Where to install the guest additions files.  */
static CHAR         g_szDstPath[CCHMAXPATH]         = "C:\\VBoxGA\\";
/** The length of g_szDstPath, including a trailing slash. */
static size_t       g_cchDstPath                    = sizeof("C:\\VBoxGA\\") - 1;
/** Whether to install shared folders. */
static bool         g_fSharedFolders                = true;
/** Whether to install the mouse driver. */
static bool         g_fMouse                        = true;
/** Whether to install the graphics driver. */
static bool         g_fGraphics                     = true;
/** Whether to install the service. */
static bool         g_fService                      = true;
/** Whether to modify startup.cmd to start VBoxService. */
static bool         g_fModifyStartupCmd             = true;
/** Whether to install the kLibC DLLs. */
static bool         g_fLibcDlls                     = true;

/** The standard output handle. */
static HFILE const g_hStdOut = (HFILE)1;
/** The standard error handle. */
static HFILE const g_hStdErr = (HFILE)2;


static void DoWriteNStr(HFILE hFile, const char *psz, size_t cch)
{
    ULONG cbIgnore;
    while (DosWrite(hFile, (void *)psz, cch, &cbIgnore) == ERROR_INTERRUPT)
        ;
}


static void DoWriteStr(HFILE hFile, const char *psz)
{
    DoWriteNStr(hFile, psz, strlen(psz));
}


static char *MyNumToString(char *pszBuf, unsigned uNum)
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


static void DoWriteNumber(HFILE hFile, unsigned uNum)
{
    char szTmp[32];
    MyNumToString(szTmp, uNum);
    DoWriteStr(hFile, szTmp);
}


static RTEXITCODE ApiError(const char *pszMsg, APIRET rc)
{
    DoWriteNStr(g_hStdErr, RT_STR_TUPLE("VBoxOs2AdditionsInstall: error: "));
    DoWriteStr(g_hStdErr, pszMsg);
    DoWriteNStr(g_hStdErr, RT_STR_TUPLE(": "));
    DoWriteNumber(g_hStdErr, rc);
    DoWriteNStr(g_hStdErr, RT_STR_TUPLE("\r\n"));
    return RTEXITCODE_SYNTAX;
}



static RTEXITCODE SyntaxError(const char *pszMsg, const char *pszArg)
{
    DoWriteNStr(g_hStdErr, RT_STR_TUPLE("VBoxOs2AdditionsInstall: syntax error: "));
    DoWriteNStr(g_hStdErr, pszMsg, strlen(pszMsg));
    DoWriteNStr(g_hStdErr, RT_STR_TUPLE("\r\n"));
    return RTEXITCODE_SYNTAX;
}



/**
 * Checks that the necessary GRADD components are present.
 */
static RTEXITCODE CheckForGradd(void)
{
    return RTEXITCODE_SUCCESS;
}


/**
 * Prepares the config.sys modifications.
 */
static RTEXITCODE PrepareConfigSys(void)
{
    return RTEXITCODE_SUCCESS;
}


/**
 * Prepares the startup.cmd modifications.
 */
static RTEXITCODE PrepareStartupCmd(void)
{
    return RTEXITCODE_SUCCESS;
}


/**
 * Copies the GA files.
 */
static RTEXITCODE CopyFiles(void)
{
    return RTEXITCODE_SUCCESS;
}


/**
 * Writes out the modified config.sys.
 */
static RTEXITCODE WriteConfigSys(void)
{
    return RTEXITCODE_SUCCESS;
}


/**
 * Writes out the modified startup.cmd.
 */
static RTEXITCODE WriteStartupCmd(void)
{
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE ShowUsage(void)
{
    static const char g_szUsage[] = "Usage:\r\n";
    DoWriteNStr(g_hStdOut, RT_STR_TUPLE(g_szUsage));
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE ShowVersion(void)
{
    static const char g_szRev[] = "$Rev$\r\n";
    DoWriteNStr(g_hStdOut, RT_STR_TUPLE(g_szRev));
    return RTEXITCODE_SUCCESS;

}


static bool MatchOptWord(PSZ *ppsz, const char *pszWord, size_t cchWord, bool fTakeValue = false)
{
    PSZ psz = *ppsz;
    if (strncmp(psz, pszWord, cchWord) == 0)
    {
        psz += cchWord;
        CHAR ch = *psz;
        if (ch == '\0')
        {
            /* No extra complaining needed when fTakeValue is true, as values must be non-empty strings. */
            *ppsz = psz;
            return true;
        }
        if (RT_C_IS_SPACE(ch))
        {
            if (fTakeValue)
                do
                {
                    ch = *++psz;
                } while (RT_C_IS_SPACE(ch));
            *ppsz = psz;
            return true;
        }
        if (fTakeValue && (ch == ':' || ch == '='))
        {
            *ppsz = psz + 1;
            return true;
        }
    }
    return false;
}


static PSZ GetOptValue(PSZ psz, const char *pszOption, char *pszValue, size_t cbValue)
{
    PSZ const pszStart = psz;
    CHAR      ch       = *psz;
    if (ch != '\0' && RT_C_IS_SPACE(ch))
    {
        do
            ch = *++psz;
        while (ch != '\0' && !RT_C_IS_SPACE(ch));

        size_t const cchSrc = psz - pszStart;
        if (cchSrc < cbValue)
        {
            memcpy(pszValue, pszStart, cchSrc);
            pszValue[cchSrc] = '\0';
            return psz; /* Do not skip space or we won't get out of the inner option loop! */
        }
        SyntaxError("Argument value too long", pszOption);
    }
    else
        SyntaxError("Argument value cannot be empty", pszOption);
    return NULL;
}


static PSZ GetOptPath(PSZ psz, const char *pszOption, char *pszValue, size_t cbValue, size_t cchScratch, size_t *pcchValue)
{
    psz = GetOptValue(psz, pszOption, pszValue, cbValue - cchScratch);
    if (psz)
    {
        /* Only accept drive letters for now.  This could be a UNC path too for CID servers ;-) */
        if (   !RT_C_IS_ALPHA(pszValue[0])
            || pszValue[1] != ':'
            || (pszValue[2] != '\0' && pszValue[2] != '\\' && pszValue[2] != '/'))
            SyntaxError("The path must be absolute", pszOption);

        *pcchValue = RTPathEnsureTrailingSeparator(pszValue, cbValue);
        if (*pcchValue == 0)
            SyntaxError("RTPathEnsureTrailingSeparator overflowed", pszValue);
    }
    return psz;
}



/**
 * This is the main entrypoint of the executable (no CRT).
 *
 * @note Considered doing a main() wrapper by means of RTGetOptArgvFromString,
 *       but the dependencies are bad and we definitely need a half working heap
 *       for that.  Maybe later.
 */
extern "C" int __cdecl VBoxOs2AdditionsInstallMain(HMODULE hmodExe, ULONG ulReserved, PSZ pszzEnv, PSZ pszzCmdLine)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;

    /*
     * Correct defaults.
     */
    ULONG ulBootDrv = 0x80;
    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBootDrv, sizeof(ulBootDrv));
    g_szBootDrivePath[0] = g_szDstPath[0] = 'A' + ulBootDrv;

    /*
     * Parse parameters, skipping the first argv[0] one.
     */
    PSZ  pszArgs = &pszzCmdLine[strlen(pszzCmdLine) + 1];
    CHAR ch;
    while ((ch = *pszArgs++) != '\0')
    {
        if (RT_C_IS_SPACE(ch))
            continue;
        if (ch != '-')
            return SyntaxError("Non-option argument", pszArgs - 1);
        ch = *pszArgs++;
        if (ch == '-')
        {
            if (!pszArgs[0])
                break;
            if (   MatchOptWord(&pszArgs, RT_STR_TUPLE("boot"), true)
                || MatchOptWord(&pszArgs, RT_STR_TUPLE("boot-drive"), true))
                ch = 'b';
            else if (   MatchOptWord(&pszArgs, RT_STR_TUPLE("dst"), true)
                     || MatchOptWord(&pszArgs, RT_STR_TUPLE("destination"), true) )
                ch = 'd';
            else if (   MatchOptWord(&pszArgs, RT_STR_TUPLE("src"), true)
                     || MatchOptWord(&pszArgs, RT_STR_TUPLE("source"), true))
                ch = 's';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("shared-folders")))
                ch = 'f';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("no-shared-folders")))
                ch = 'F';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("graphics")))
                ch = 'g';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("no-graphics")))
                ch = 'G';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("mouse")))
                ch = 'm';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("no-mouse")))
                ch = 'M';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("service")))
                ch = 's';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("no-service")))
                ch = 'S';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("modify-startup-cmd")))
                ch = 't';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("no-modify-startup-cmd")))
                ch = 'T';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("libc-dlls")))
                ch = 'l';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("no-libc-dlls")))
                ch = 'L';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("help")))
                ch = 'h';
            else if (MatchOptWord(&pszArgs, RT_STR_TUPLE("version")))
                ch = 'v';
            else
                return SyntaxError("Unknown option", pszArgs - 2);
        }

        for (;;)
        {
            switch (ch)
            {
                case '-':
                    while ((ch = *pszArgs) != '\0' && RT_C_IS_SPACE(ch))
                        pszArgs++;
                    if (ch == '\0')
                        break;
                    return SyntaxError("Non-option argument", pszArgs);

                case 'b':
                    pszArgs = GetOptPath(pszArgs, "--boot-drive / -b",
                                         g_szBootDrivePath, sizeof(g_szBootDrivePath), 64, &g_cchBootDrivePath);
                    if (!pszArgs)
                        return RTEXITCODE_SYNTAX;
                    break;

                case 'd':
                    pszArgs = GetOptPath(pszArgs, "--destination / -d", g_szDstPath, sizeof(g_szDstPath), 32, &g_cchDstPath);
                    if (!pszArgs)
                        return RTEXITCODE_SYNTAX;
                    break;

                case 's':
                    pszArgs = GetOptPath(pszArgs, "--source / -s", g_szSrcPath, sizeof(g_szSrcPath), 32, &g_cchSrcPath);
                    if (!pszArgs)
                        return RTEXITCODE_SYNTAX;
                    break;

                case 'f':
                    g_fSharedFolders = true;
                    break;
                case 'F':
                    g_fSharedFolders = false;
                    break;

                case 'g':
                    g_fGraphics = true;
                    break;
                case 'G':
                    g_fGraphics = false;
                    break;

                case 'm':
                    g_fMouse = true;
                    break;
                case 'M':
                    g_fMouse = false;
                    break;

                case 'e':
                    g_fService = true;
                    break;
                case 'E':
                    g_fService = false;
                    break;

                case 'u':
                    g_fModifyStartupCmd = true;
                    break;
                case 'U':
                    g_fModifyStartupCmd = false;
                    break;

                case 'l':
                    g_fLibcDlls = true;
                    break;
                case 'L':
                    g_fLibcDlls = false;
                    break;

                case 'h':
                case '?':
                    return ShowUsage();
                case 'v':
                    return ShowVersion();

                default:
                    return SyntaxError("Unknown option", pszArgs - 2);
            }

            ch = *pszArgs;
            if (RT_C_IS_SPACE(ch) || ch == '\0')
                break;
            pszArgs++;
        }
    }

    if (g_szSrcPath[0] == '\0')
    {
        APIRET rc = DosQueryModuleName(hmodExe, sizeof(g_szSrcPath), g_szSrcPath);
        if (rc != NO_ERROR)
            return ApiError("DosQueryModuleName", rc);
        RTPathStripFilename(g_szSrcPath);
        g_cchSrcPath = RTPathEnsureTrailingSeparator(g_szSrcPath, sizeof(g_szSrcPath));
        if (g_cchSrcPath)
            return ApiError("RTPathEnsureTrailingSeparator", ERROR_BUFFER_OVERFLOW);
    }

    /*
     * Do the installation.
     */
    rcExit = CheckForGradd();
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = PrepareConfigSys();
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = PrepareStartupCmd();
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = CopyFiles();
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = WriteConfigSys();
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = WriteStartupCmd();

    return rcExit;
}


#if 0 /* Better off with the assembly file here. */
/*
 * Define the stack.
 *
 * This \#pragma data_seg(STACK,STACK) thing seems to work a little better for
 * 32-bit OS2 binaries than 16-bit.  Wlink still thinks it needs to allocate
 * zero bytes in the file for the abStack variable, but it doesn't looks like
 * both the starting ESP and stack size fields in the LX header are correct.
 *
 * The problem seems to be that wlink will write anything backed by
 * LEDATA32/LIDATA32 even it's all zeros.  The C compiler emits either of those
 * here, and boom the whole BSS is written to the file.
 *
 * For 16-bit (see os2_util.c) it would find the stack, but either put the
 * correct obj:SP or stack size fields in the NE header, never both and the
 * resulting EXE would either not start or crash immediately.
 */
#pragma data_seg("STACK", "STACK")
static uint64_t abStack[4096];
#endif

