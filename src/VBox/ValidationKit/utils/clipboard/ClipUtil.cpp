/* $Id$ */
/** @file
 * ClipUtil - Clipboard Utility
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#ifdef RT_OS_OS2
# define INCL_BASE
# define INCL_PM
# define INCL_ERRORS
# include <os2.h>
# undef RT_MAX
#endif

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/utf16.h>
#include <iprt/zero.h>

#ifdef RT_OS_WINDOWS
# include <iprt/nt/nt-and-windows.h>
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2) || defined(RT_OS_DARWIN)
# undef MULTI_TARGET_CLIPBOARD
#else
# define MULTI_TARGET_CLIPBOARD
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Clipboard format descriptor.
 */
typedef struct CLIPUTILFORMAT
{
    /** Format name. */
    const char *pszName;

#if defined(RT_OS_WINDOWS)
    /** Windows integer format (CF_XXXX). */
    UINT        fFormat;
    /** Windows string format name. */
    const WCHAR *pwszFormat;

#elif defined(RT_OS_OS2)
    /** OS/2 integer format. */
    ULONG       fFormat;
    /** OS/2 string format name. */
    const char *pszFormat;

#elif defined(RT_OS_DARWIN)
    /** Native format (flavor). */
    CFStringRef *hStrFormat;
#else
    /** @todo X11 */
#endif

    /** Description. */
    const char *pszDesc;
    /** CLIPUTILFORMAT_F_XXX. */
    uint32_t    fFlags;
} CLIPUTILFORMAT;
/** Pointer to a clipobard format descriptor. */
typedef CLIPUTILFORMAT const *PCCLIPUTILFORMAT;

/** Convert to/from UTF-8.  */
#define CLIPUTILFORMAT_F_CONVERT_UTF8       RT_BIT_32(0)


/**
 * Clipboard target descriptor.
 */
typedef struct CLIPUTILTARGET
{
    /** Target name.   */
    const char *pszName;
    /** Description. */
    const char *pszDesc;
} CLIPUTILTARGET;
/** Pointer to clipboard target descriptor. */
typedef CLIPUTILTARGET const *PCCLIPUTILTARGET;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    { "--list",                     'l',                            RTGETOPT_REQ_NOTHING },
    { "--get",                      'g',                            RTGETOPT_REQ_STRING  },
    { "--get-file",                 'G',                            RTGETOPT_REQ_STRING  },
    { "--put",                      'p',                            RTGETOPT_REQ_STRING  },
    { "--put-file",                 'P',                            RTGETOPT_REQ_STRING  },
    { "--check",                    'c',                            RTGETOPT_REQ_STRING  },
    { "--check-file",               'C',                            RTGETOPT_REQ_STRING  },
    { "--check-not",                'n',                            RTGETOPT_REQ_STRING  },
    { "--zap",                      'z',                            RTGETOPT_REQ_NOTHING },
#ifdef MULTI_TARGET_CLIPBOARD
    { "--target",                   't',                            RTGETOPT_REQ_STRING  },
#endif
    { "--quiet",                    'q',                            RTGETOPT_REQ_NOTHING },
    { "--verbose",                  'v',                            RTGETOPT_REQ_NOTHING },
    { "--version",                  'V',                            RTGETOPT_REQ_NOTHING },
    { "--help",                     'h',                            RTGETOPT_REQ_NOTHING }, /* for Usage() */
};

/** Format descriptors. */
static const CLIPUTILFORMAT g_aFormats[] =
{
#if defined(RT_OS_WINDOWS)
    { "text/ansi",                  CF_TEXT, NULL,              "ANSI text", 0 },
    { "text/utf-16",                CF_UNICODETEXT, NULL,       "UTF-16 text", 0 },
    { "text/utf-8",                 CF_UNICODETEXT, NULL,       "UTF-8 text", CLIPUTILFORMAT_F_CONVERT_UTF8 },
    /* https://docs.microsoft.com/en-us/windows/desktop/dataxchg/html-clipboard-format */
    { "text/html",                  0, L"HTML Format",          "HTML text", 0 },
    { "bitmap",                     CF_DIB, NULL,               "Bitmap (DIB)", 0 },
    { "bitmap/v5",                  CF_DIBV5, NULL,             "Bitmap version 5 (DIBv5)", 0 },
#elif defined(RT_OS_OS2)
    { "text/ascii",                 CF_TEXT, NULL,              "ASCII text", 0 },
    { "text/utf-8",                 CF_TEXT, NULL,              "UTF-8 text", CLIPUTILFORMAT_F_CONVERT_UTF8 },
    { "text/utf-16",                0, "Odin32 UnicodeText",    "UTF-16 text", 0},
#elif defined(RT_OS_DARWIN)
    { "text/utf-8",                 kUTTypeUTF8PlainText,       "UTF-8 text", 0 },
    { "text/utf-16",                kUTTypeUTF16PlainText,      "UTF-16 text", 0 },
#else
    /** @todo X11   */
    { "text/utf-8",                        "UTF-8 text", 0 },
#endif
};

/** Target descriptors. */
static const CLIPUTILTARGET g_aTargets[] =
{
#ifndef MULTI_TARGET_CLIPBOARD
    { "default", "placeholder" }
#else
    { "default", "default" }, /** @todo X11 */
#endif
};

/** The -v/-q state. */
static unsigned g_uVerbosity = 1;

#ifdef RT_OS_WINDOWS
static bool     g_fOpenedClipboard = false;
#endif


/**
 * Gets a format descriptor, complaining if unknown format.
 *
 * @returns Pointer to the descriptor if found, NULL + msg if not.
 * @param   pszFormat           The format to get.
 */
static PCCLIPUTILFORMAT GetFormatDesc(const char *pszFormat)
{
    for (size_t i = 0; i < RT_ELEMENTS(g_aFormats); i++)
        if (strcmp(pszFormat, g_aFormats[i].pszName))
            return &g_aFormats[i];
    RTMsgError("Unknown format '%s'", pszFormat);
    return NULL;
}

#ifdef RT_OS_WINDOWS

/**
 * Opens the window clipboard.
 */
static RTEXITCODE WinOpenClipboardIfNecessary(void)
{
    if (g_fOpenedClipboard)
        return RTEXITCODE_SUCCESS;
    if (OpenClipboard(NULL))
    {
        g_fOpenedClipboard = true;
        return RTEXITCODE_SUCCESS;
    }
    return RTMsgErrorExitFailure("OpenClipboard failed: %u (%#x)", GetLastError(), GetLastError());
}


/**
 * Wrapper that automatically registers non-standard formats.
 */
static UINT FormatDescToWindows(PCCLIPUTILFORMAT pFmtDesc)
{
    if (pFmtDesc->fFormat)
        return pFmtDesc->fFormat;
    Assert(pFmtDesc->pwszFormat);
    UINT fFormat = RegisterClipboardFormatW(pFmtDesc->pwszFormat);
    if (fFormat == 0)
        RTMsgError("RegisterClipboardFormatW(%ls) failed: %u (%#x)", pFmtDesc->pwszFormat, GetLastError(), GetLastError());
    return fFormat;
}

#endif /* RT_OS_WINDOWS */



/**
 * Lists the clipboard format.
 */
static RTEXITCODE ListClipboardContent(void)
{
#ifdef RT_OS_WINDOWS
    RTEXITCODE rcExit = WinOpenClipboardIfNecessary();
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        SetLastError(0);
        uint32_t idx     = 0;
        UINT     fFormat = 0;
        while ((fFormat = EnumClipboardFormats(fFormat)) != 0)
        {
            WCHAR wszName[256];
            int   cchName = GetClipboardFormatNameW(fFormat, wszName, RT_ELEMENTS(wszName));
            if (cchName > 0)
                RTPrintf("#%u: %#06x - %ls\n", idx, fFormat, wszName);
            else
            {
                const char *pszName = NULL;
                switch (fFormat)
                {
                    case CF_TEXT: pszName = "CF_TEXT"; break;
                    case CF_BITMAP: pszName = "CF_BITMAP"; break;
                    case CF_METAFILEPICT: pszName = "CF_METAFILEPICT"; break;
                    case CF_SYLK: pszName = "CF_SYLK"; break;
                    case CF_DIF: pszName = "CF_DIF"; break;
                    case CF_TIFF: pszName = "CF_TIFF"; break;
                    case CF_OEMTEXT: pszName = "CF_OEMTEXT"; break;
                    case CF_DIB: pszName = "CF_DIB"; break;
                    case CF_PALETTE: pszName = "CF_PALETTE"; break;
                    case CF_PENDATA: pszName = "CF_PENDATA"; break;
                    case CF_RIFF: pszName = "CF_RIFF"; break;
                    case CF_WAVE: pszName = "CF_WAVE"; break;
                    case CF_UNICODETEXT: pszName = "CF_UNICODETEXT"; break;
                    case CF_ENHMETAFILE: pszName = "CF_ENHMETAFILE"; break;
                    case CF_HDROP: pszName = "CF_HDROP"; break;
                    case CF_LOCALE: pszName = "CF_LOCALE"; break;
                    case CF_DIBV5: pszName = "CF_DIBV5"; break;
                    default:
                        break;
                }
                if (pszName)
                    RTPrintf("#%u: %#06x - %s\n", idx, fFormat, pszName);
                else
                    RTPrintf("#%u: %#06x\n", idx, fFormat);
            }

            idx++;
        }
        if (idx == 0)
            RTPrintf("Empty\n");
    }
    return rcExit;

#else
    return RTMsgErrorExitFailure("ListClipboardContent is not implemented");
#endif
}


/**
 * Reads the given clipboard format and stores it in on the heap.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to get.
 * @param   ppvData     Where to return the pointer to the data. Free using
 *                      RTMemFree when done.
 * @param   pcbData     Where to return the amount of data returned.
 */
static RTEXITCODE ReadClipboardData(PCCLIPUTILFORMAT pFmtDesc, void **ppvData, size_t *pcbData)
{
    *ppvData = NULL;
    *pcbData = 0;
#ifdef RT_OS_WINDOWS
    RTEXITCODE rcExit = WinOpenClipboardIfNecessary();
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        HANDLE hData = GetClipboardData(FormatDescToWindows(pFmtDesc));
        if (hData != NULL)
        {
            SIZE_T const cbData = GlobalSize(hData);
            PVOID  const pvData = GlobalLock(hData);
            if (pvData != NULL)
            {
                *pcbData = cbData;
                if (cbData != 0)
                {
                    if (pFmtDesc->fFlags & CLIPUTILFORMAT_F_CONVERT_UTF8)
                    {
                        char  *pszUtf8 = NULL;
                        size_t cchUtf8 = 0;
                        int rc = RTUtf16ToUtf8Ex((PCRTUTF16)pvData, cbData / sizeof(RTUTF16), &pszUtf8, 0, &cchUtf8);
                        if (RT_SUCCESS(rc))
                        {
                            *pcbData = cchUtf8 + 1;
                            *ppvData = RTMemDup(pszUtf8, cchUtf8 + 1);
                            RTStrFree(pszUtf8);
                            if (!*ppvData)
                                rcExit = RTMsgErrorExitFailure("Out of memory allocating %#zx bytes.", cbData);
                        }
                        else
                            rcExit = RTMsgErrorExitFailure("RTUtf16ToUtf8Ex failed: %Rrc", rc);
                    }
                    else
                    {
                        *ppvData = RTMemDup(pvData, cbData);
                        if (!*ppvData)
                            rcExit = RTMsgErrorExitFailure("Out of memory allocating %#zx bytes.", cbData);
                    }
                }
                GlobalUnlock(hData);
            }
            else
                rcExit = RTMsgErrorExitFailure("GetClipboardData(%s) failed: %u (%#x)\n",
                                               pFmtDesc->pszName, GetLastError(), GetLastError());
        }
        else
            rcExit = RTMsgErrorExitFailure("GetClipboardData(%s) failed: %u (%#x)\n",
                                           pFmtDesc->pszName, GetLastError(), GetLastError());
    }
    return rcExit;

#else
    RT_NOREF(pFmtDesc);
    return RTMsgErrorExitFailure("ReadClipboardData is not implemented\n");
#endif
}


/**
 * Puts the given data and format on the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc     The format.
 * @param   pvData       The data.
 * @param   cbData       The amount of data in bytes.
 */
static RTEXITCODE WriteClipboardData(PCCLIPUTILFORMAT pFmtDesc, void const *pvData, size_t cbData)
{
#ifdef RT_OS_WINDOWS
    RTEXITCODE rcExit = WinOpenClipboardIfNecessary();
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Do input data conversion.
         */
        PRTUTF16 pwszFree = NULL;
        if (pFmtDesc->fFlags & CLIPUTILFORMAT_F_CONVERT_UTF8)
        {
            size_t cwcConv = 0;
            int rc = RTStrToUtf16Ex((char const *)pvData, cbData, &pwszFree, 0, &cwcConv);
            if (RT_SUCCESS(rc))
            {
                pvData = pwszFree;
                cbData = cwcConv * sizeof(RTUTF16);
            }
            else
                return RTMsgErrorExitFailure("RTStrToTUtf16Ex failed: %Rrc\n", rc);
        }

        /*
         * Text formats generally include the zero terminator.
         */
        uint32_t cbZeroPadding = 0;
        if (pFmtDesc->fFormat == CF_UNICODETEXT)
            cbZeroPadding = sizeof(WCHAR);
        else if (pFmtDesc->fFormat == CF_TEXT)
            cbZeroPadding = sizeof(char);

        HANDLE hDstData = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, cbData + cbZeroPadding);
        if (hDstData)
        {
            if (cbData)
            {
                PVOID pvDstData = GlobalLock(hDstData);
                if (pvDstData)
                    memcpy(pvDstData, pvData, cbData);
                else
                    rcExit = RTMsgErrorExitFailure("GlobalLock failed: %u (%#x)\n", GetLastError(), GetLastError());
            }
            if (rcExit == RTEXITCODE_SUCCESS)
            {
                if (!SetClipboardData(FormatDescToWindows(pFmtDesc), hDstData))
                {
                    rcExit = RTMsgErrorExitFailure("SetClipboardData(%s) failed: %u (%#x)\n",
                                                   pFmtDesc->pszName, GetLastError(), GetLastError());
                    GlobalFree(hDstData);
                }
            }
            else
                GlobalFree(hDstData);
        }
        else
            rcExit = RTMsgErrorExitFailure("GlobalAlloc(,%#zx) failed: %u (%#x)\n",
                                           cbData + cbZeroPadding, GetLastError(), GetLastError());
    }
    return rcExit;

#else
    RT_NOREF(pFmtDesc, pvData, cbData);
    return RTMsgErrorExitFailure("WriteClipboardData is not implemented\n");
#endif
}


/**
 * Check if the given data + format matches what's actually on the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to compare.
 * @param   pvExpect    The expected clipboard data.
 * @param   cbExpect    The size of the expected clipboard data.
 */
static RTEXITCODE CompareDataWithClipboard(PCCLIPUTILFORMAT pFmtDesc, void const *pvExpect, size_t cbExpect)
{
    void      *pvData = NULL;
    size_t     cbData = 0;
    RTEXITCODE rcExit = ReadClipboardData(pFmtDesc, &pvData, &cbData);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        if (   cbData == cbExpect
            && memcmp(pvData, pvExpect, cbData) == 0)
            rcExit = RTEXITCODE_SUCCESS;
        else
            rcExit = RTMsgErrorExitFailure("Mismatch for '%s' (cbData=%#zx cbExpect=%#zx)\n",
                                           pFmtDesc->pszName, cbData, cbExpect);
        RTMemFree(pvData);
    }
    return rcExit;
}


/**
 * Gets the given clipboard format.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to get.
 * @param   pStrmOut    Where to output the data.
 * @param   fIsStdOut   Set if @a pStrmOut is standard output, clear if not.
 */
static RTEXITCODE ClipboardContentToStdOut(PCCLIPUTILFORMAT pFmtDesc)
{
    void      *pvData = NULL;
    size_t     cbData = 0;
    RTEXITCODE rcExit = ReadClipboardData(pFmtDesc, &pvData, &cbData);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        int rc = RTStrmWrite(g_pStdOut, pvData, cbData);
        RTMemFree(pvData);
        if (RT_FAILURE(rc))
            rcExit = RTMsgErrorExitFailure("Error writing %#zx bytes to standard output: %Rrc", cbData, rc);
    }
    return rcExit;
}


/**
 * Gets the given clipboard format.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to get.
 * @param   pszFilename The output filename.
 */
static RTEXITCODE ClipboardContentToFile(PCCLIPUTILFORMAT pFmtDesc, const char *pszFilename)
{
    void      *pvData = NULL;
    size_t     cbData = 0;
    RTEXITCODE rcExit = ReadClipboardData(pFmtDesc, &pvData, &cbData);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        RTFILE hFile = NIL_RTFILE;
        int rc = RTFileOpen(&hFile, pszFilename,
                            RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE_REPLACE
                            | (0770 << RTFILE_O_CREATE_MODE_SHIFT));
        if (RT_SUCCESS(rc))
        {
            rc = RTFileWrite(hFile, pvData, cbData, NULL);
            int const rc2 = RTFileClose(hFile);
            if (RT_FAILURE(rc) || RT_FAILURE(rc2))
            {
                if (RT_FAILURE_NP(rc))
                    RTMsgError("Writing %#z bytes to '%s' failed: %Rrc", cbData, pszFilename, rc);
                else
                    RTMsgError("Closing '%s' failed: %Rrc", pszFilename, rc2);
                RTMsgInfo("Deleting '%s'.", pszFilename);
                RTFileDelete(pszFilename);
                rcExit = RTEXITCODE_FAILURE;
            }
        }
        else
            rcExit = RTMsgErrorExitFailure("Failed to open '%s' for writing: %Rrc", pszFilename, rc);
        RTMemFree(pvData);
    }
    return rcExit;
}


/**
 * Puts the given format + data onto the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to put.
 * @param   pszData     The string data.
 */
static RTEXITCODE PutStringOnClipboard(PCCLIPUTILFORMAT pFmtDesc, const char *pszData)
{
    return WriteClipboardData(pFmtDesc, pszData, strlen(pszData));
}


/**
 * Puts a format + file content onto the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to put.
 * @param   pszFilename The filename.
 */
static RTEXITCODE PutFileOnClipboard(PCCLIPUTILFORMAT pFmtDesc, const char *pszFilename)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    void      *pvData = NULL;
    size_t     cbData = 0;
    int rc = RTFileReadAll(pszFilename, &pvData, &cbData);
    if (RT_SUCCESS(rc))
    {
        rcExit = WriteClipboardData(pFmtDesc, pvData, cbData);
        RTFileReadAllFree(pvData, cbData);
    }
    else
        rcExit = RTMsgErrorExitFailure("Failed to open and read '%s' into memory: %Rrc", pszFilename, rc);
    return rcExit;
}


/**
 * Checks if the given format + data matches what's on the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to check.
 * @param   pszData     The string data.
 */
static RTEXITCODE CheckStringAgainstClipboard(PCCLIPUTILFORMAT pFmtDesc, const char *pszData)
{
    return CompareDataWithClipboard(pFmtDesc, pszData, strlen(pszData));
}


/**
 * Check if the given format + file content matches what's on the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to check.
 * @param   pszFilename The filename.
 */
static RTEXITCODE CheckFileAgainstClipboard(PCCLIPUTILFORMAT pFmtDesc, const char *pszFilename)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    void      *pvData = NULL;
    size_t     cbData = 0;
    int rc = RTFileReadAll(pszFilename, &pvData, &cbData);
    if (RT_SUCCESS(rc))
    {
        rcExit = CompareDataWithClipboard(pFmtDesc, pvData, cbData);
        RTFileReadAllFree(pvData, cbData);
    }
    else
        rcExit = RTMsgErrorExitFailure("Failed to open and read '%s' into memory: %Rrc", pszFilename, rc);
    return rcExit;
}


/**
 * Check that the given format is not on the clipboard.
 *
 * @returns Success indicator.
 * @param   pFmtDesc    The format to check.
 */
static RTEXITCODE CheckFormatNotOnClipboard(PCCLIPUTILFORMAT pFmtDesc)
{
#ifdef RT_OS_WINDOWS
    RTEXITCODE rcExit = WinOpenClipboardIfNecessary();
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        if (IsClipboardFormatAvailable(FormatDescToWindows(pFmtDesc)))
            rcExit = RTMsgErrorExitFailure("Format '%s' is present");
    }
    return rcExit;

#else
    RT_NOREF(pFmtDesc);
    return RTMsgErrorExitFailure("CheckFormatNotOnClipboard is not implemented");
#endif
}


/**
 * Empties the clipboard.
 *
 * @returns Success indicator.
 */
static RTEXITCODE ZapAllClipboardData(void)
{
#ifdef RT_OS_WINDOWS
    RTEXITCODE rcExit = WinOpenClipboardIfNecessary();
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        if (!EmptyClipboard())
            rcExit = RTMsgErrorExitFailure("EmptyClipboard() failed: %u (%#x)\n", GetLastError(), GetLastError());
    }
    return rcExit;

#else
    return RTMsgErrorExitFailure("ZapAllClipboardData is not implemented");
#endif
}


/**
 * Display the usage to @a pStrm.
 */
static void Usage(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm,
                 "usage: %s [--get <fmt> [--get ...]] [--get-file <fmt> <file> [--get-file ...]]\n"
                 "       %s [--zap] [--put <fmt> <content> [--put ...]] [--put-file <fmt> <file> [--put-file ...]]\n"
                 "       %s [--check <fmt> <expected> [--check ...]] [--check-file <fmt> <file> [--check-file ...]]\n"
                 "           [--check-no <fmt> [--check-no ...]]\n"
                 , RTProcShortName(), RTProcShortName(), RTProcShortName());
    RTStrmPrintf(pStrm, "\n");
    RTStrmPrintf(pStrm, "Options: \n");

    for (unsigned i = 0; i < RT_ELEMENTS(g_aCmdOptions); i++)
    {
        const char *pszHelp;
        switch (g_aCmdOptions[i].iShort)
        {
            case 'l':   pszHelp = "List the clipboard content."; break;
            case 'g':   pszHelp = "Get given clipboard format and writes it to standard output."; break;
            case 'G':   pszHelp = "Get given clipboard format and writes it to the specified file."; break;
            case 'p':   pszHelp = "Puts given format and content on the clipboard."; break;
            case 'P':   pszHelp = "Puts given format and file content on the clipboard."; break;
            case 'c':   pszHelp = "Checks that the given format and content matches the clipboard."; break;
            case 'C':   pszHelp = "Checks that the given format and file content matches the clipboard."; break;
            case 'n':   pszHelp = "Checks that the given format is not on the clipboard."; break;
            case 'z':   pszHelp = "Zaps the clipboard content."; break;
#ifdef MULTI_TARGET_CLIPBOARD
            case 't':   pszHelp = "Selects the target clipboard." break;
#endif
            case 'v':   pszHelp = "More verbose execution."; break;
            case 'q':   pszHelp = "Quiet execution."; break;
            case 'h':   pszHelp = "Displays this help and exit"; break;
            case 'V':   pszHelp = "Displays the program revision"; break;

            default:
                pszHelp = "Option undocumented";
                break;
        }
        if ((unsigned)g_aCmdOptions[i].iShort < 127U)
        {
            char szOpt[64];
            RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdOptions[i].pszLong, g_aCmdOptions[i].iShort);
            RTStrmPrintf(pStrm, "  %-19s %s\n", szOpt, pszHelp);
        }
        else
            RTStrmPrintf(pStrm, "  %-19s %s\n", g_aCmdOptions[i].pszLong, pszHelp);
    }
    RTStrmPrintf(pStrm, "Note! Options are processed in the order they are given.\n");

    RTStrmPrintf(pStrm, "\nFormats:\n");
    for (size_t i = 0; i < RT_ELEMENTS(g_aFormats); i++)
        RTStrmPrintf(pStrm, "    %-12s: %s\n", g_aFormats[i].pszName, g_aFormats[i].pszDesc);

#ifdef MULTI_TARGET_CLIPBOARD
    RTStrmPrintf(pStrm, "\nTarget:\n");
    for (size_t i = 0; i < RT_ELEMENTS(g_aTargets); i++)
        RTStrmPrintf(pStrm, "    %-12s: %s\n", g_aTargets[i].pszName, g_aTargets[i].pszDesc);
#endif
}


int main(int argc, char *argv[])
{
    /*
     * Init IPRT.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Process options (in order).
     */
    RTEXITCODE    rcExit = RTEXITCODE_SUCCESS;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdOptions, RT_ELEMENTS(g_aCmdOptions), 1, 0 /* fFlags */);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        RTEXITCODE rcExit2 = RTEXITCODE_SUCCESS;
        switch (rc)
        {
            case 'l':
                rcExit2 = ListClipboardContent();
                break;

            case 'g':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                    rcExit2 = ClipboardContentToStdOut(pFmtDesc);
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }

            case 'G':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                {
                    rc = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_STRING);
                    if (RT_SUCCESS(rc))
                        rcExit2 = ClipboardContentToFile(pFmtDesc, ValueUnion.psz);
                    else
                        return RTMsgError("No filename given with --get-file");
                }
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }

            case 'p':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                {
                    rc = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_STRING);
                    if (RT_SUCCESS(rc))
                        rcExit2 = PutStringOnClipboard(pFmtDesc, ValueUnion.psz);
                    else
                        return RTMsgError("No data string given with --put");
                }
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }

            case 'P':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                {
                    rc = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_STRING);
                    if (RT_SUCCESS(rc))
                        rcExit2 = PutFileOnClipboard(pFmtDesc, ValueUnion.psz);
                    else
                        return RTMsgError("No filename given with --put-file");
                }
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }

            case 'c':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                {
                    rc = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_STRING);
                    if (RT_SUCCESS(rc))
                        rcExit2 = CheckStringAgainstClipboard(pFmtDesc, ValueUnion.psz);
                    else
                        return RTMsgError("No data string given with --check");
                }
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }

            case 'C':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                {
                    rc = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_STRING);
                    if (RT_SUCCESS(rc))
                        rcExit2 = CheckFileAgainstClipboard(pFmtDesc, ValueUnion.psz);
                    else
                        return RTMsgError("No filename given with --check-file");
                }
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }

            case 'n':
            {
                PCCLIPUTILFORMAT pFmtDesc = GetFormatDesc(ValueUnion.psz);
                if (pFmtDesc)
                    rcExit2 = CheckFormatNotOnClipboard(pFmtDesc);
                else
                    rcExit2 = RTEXITCODE_FAILURE;
                break;
            }


            case 'z':
                rcExit2 = ZapAllClipboardData();
                break;

            case 'q':
                g_uVerbosity = 0;
                break;

            case 'v':
                g_uVerbosity++;
                break;

            case 'h':
                Usage(g_pStdOut);
                return RTEXITCODE_SUCCESS;

            case 'V':
            {
                char szRev[] = "$Revision$";
                szRev[RT_ELEMENTS(szRev) - 2] = '\0';
                RTPrintf(RTStrStrip(strchr(szRev, ':') + 1));
                return RTEXITCODE_SUCCESS;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }

        if (rcExit2 != RTEXITCODE_SUCCESS && rcExit == RTEXITCODE_SUCCESS)
            rcExit = rcExit2;
    }

    /*
     * Host specific cleanup.
     */
#ifdef RT_OS_WINDOWS
    if (g_fOpenedClipboard)
    {
        if (!CloseClipboard())
            rcExit = RTMsgErrorExitFailure("CloseClipboard failed: %u (%#x)", GetLastError(), GetLastError());
    }
#endif

    return rcExit;
}

