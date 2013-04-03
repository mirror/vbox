/* $Id$ */
/** @file
 * VBoxStub - VirtualBox's Windows installer stub.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <windows.h>
#include <commctrl.h>
#include <lmerr.h>
#include <msiquery.h>
#include <objbase.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>

#include <VBox/version.h>

#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "VBoxStub.h"
#include "../StubBld/VBoxStubBld.h"
#include "resource.h"

#ifdef VBOX_WITH_CODE_SIGNING
# include "VBoxStubCertUtil.h"
# include "VBoxStubPublicCert.h"
#endif

#ifndef  _UNICODE /* Isn't this a little late? */
#define  _UNICODE
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static bool g_fSilent = false;


/**
 * Shows an error message box with a printf() style formatted string.
 *
 * @returns RTEXITCODE_FAILURE
 * @param   pszFmt              Printf-style format string to show in the message box body.
 *
 */
static RTEXITCODE ShowError(const char *pszFmt, ...)
{
    char       *pszMsg;
    va_list     va;

    va_start(va, pszFmt);
    if (RTStrAPrintfV(&pszMsg, pszFmt, va))
    {
        if (g_fSilent)
            RTMsgError("%s", pszMsg);
        else
            MessageBox(GetDesktopWindow(), pszMsg, VBOX_STUB_TITLE, MB_ICONERROR);
        RTStrFree(pszMsg);
    }
    else /* Should never happen! */
        AssertMsgFailed(("Failed to format error text of format string: %s!\n", pszFmt));
    va_end(va);
    return RTEXITCODE_FAILURE;
}


/**
 * Shows a message box with a printf() style formatted string.
 *
 * @param   uType               Type of the message box (see MSDN).
 * @param   pszFmt              Printf-style format string to show in the message box body.
 *
 */
static void ShowInfo(const char *pszFmt, ...)
{
    char       *pszMsg;
    va_list     va;
    va_start(va, pszFmt);
    int rc = RTStrAPrintfV(&pszMsg, pszFmt, va);
    va_end(va);
    if (rc >= 0)
    {
        if (g_fSilent)
            RTPrintf("%s\n", pszMsg);
        else
            MessageBox(GetDesktopWindow(), pszMsg, VBOX_STUB_TITLE, MB_ICONINFORMATION);
    }
    else /* Should never happen! */
        AssertMsgFailed(("Failed to format error text of format string: %s!\n", pszFmt));
    RTStrFree(pszMsg);
}


/**
 * Finds the specified in the resource section of the executable.
 *
 * @returns IPRT status code.
 *
 * @param   pszDataName         Name of resource to read.
 * @param   ppvResource         Where to return the pointer to the data.
 * @param   pdwSize             Where to return the size of the data (if found).
 *                              Optional.
 *
 */
static int FindData(const char *pszDataName,
                    PVOID      *ppvResource,
                    DWORD      *pdwSize)
{
    AssertReturn(pszDataName, VERR_INVALID_PARAMETER);
    HINSTANCE hInst = NULL;

    /* Find our resource. */
    HRSRC hRsrc = FindResourceEx(hInst, RT_RCDATA, pszDataName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    AssertReturn(hRsrc, VERR_IO_GEN_FAILURE);

    /* Get resource size. */
    DWORD cb = SizeofResource(hInst, hRsrc);
    AssertReturn(cb > 0, VERR_NO_DATA);
    if (pdwSize)
        *pdwSize = cb;

    /* Get pointer to resource. */
    HGLOBAL hData = LoadResource(hInst, hRsrc);
    AssertReturn(hData, VERR_IO_GEN_FAILURE);

    /* Lock resource. */
    *ppvResource = LockResource(hData);
    AssertReturn(*ppvResource, VERR_IO_GEN_FAILURE);
    return VINF_SUCCESS;
}


/**
 * Finds the header for the given package.
 *
 * @returns Pointer to the package header on success.  On failure NULL is
 *          returned after ShowError has been invoked.
 * @param   iPackage            The package number.
 */
static PVBOXSTUBPKG FindPackageHeader(unsigned iPackage)
{
    char szHeaderName[32];
    RTStrPrintf(szHeaderName, sizeof(szHeaderName), "HDR_%02d", iPackage);

    PVBOXSTUBPKG pPackage;
    int rc = FindData(szHeaderName, (PVOID *)&pPackage, NULL);
    if (RT_FAILURE(rc))
    {
        ShowError("Internal error: Could not find package header #%u: %Rrc", iPackage, rc);
        return NULL;
    }

    /** @todo validate it. */
    return pPackage;
}



/**
 * Constructs a full temporary file path from the given parameters.
 *
 * @returns iprt status code.
 *
 * @param   pszTempPath         The pure path to use for construction.
 * @param   pszTargetFileName   The pure file name to use for construction.
 * @param   ppszTempFile        Pointer to the constructed string.  Must be freed
 *                              using RTStrFree().
 */
static int GetTempFileAlloc(const char  *pszTempPath,
                            const char  *pszTargetFileName,
                            char       **ppszTempFile)
{
    if (RTStrAPrintf(ppszTempFile, "%s\\%s", pszTempPath, pszTargetFileName) >= 0)
        return VINF_SUCCESS;
    return VERR_NO_STR_MEMORY;
}


/**
 * Extracts a built-in resource to disk.
 *
 * @returns iprt status code.
 *
 * @param   pszResourceName     The resource name to extract.
 * @param   pszTempFile         The full file path + name to extract the resource to.
 *
 */
static int ExtractFile(const char *pszResourceName,
                       const char *pszTempFile)
{
    int rc;
    RTFILE fh;
    BOOL bCreatedFile = FALSE;

    do
    {
        AssertMsgBreak(pszResourceName, ("Resource pointer invalid!\n"));
        AssertMsgBreak(pszTempFile, ("Temp file pointer invalid!"));

        /* Read the data of the built-in resource. */
        PVOID pvData = NULL;
        DWORD dwDataSize = 0;
        rc = FindData(pszResourceName, &pvData, &dwDataSize);
        AssertMsgRCBreak(rc, ("Could not read resource data!\n"));

        /* Create new (and replace an old) file. */
        rc = RTFileOpen(&fh, pszTempFile,
                          RTFILE_O_CREATE_REPLACE
                        | RTFILE_O_WRITE
                        | RTFILE_O_DENY_NOT_DELETE
                        | RTFILE_O_DENY_WRITE);
        AssertMsgRCBreak(rc, ("Could not open file for writing!\n"));
        bCreatedFile = TRUE;

        /* Write contents to new file. */
        size_t cbWritten = 0;
        rc = RTFileWrite(fh, pvData, dwDataSize, &cbWritten);
        AssertMsgRCBreak(rc, ("Could not open file for writing!\n"));
        AssertMsgBreak(dwDataSize == cbWritten, ("File was not extracted completely! Disk full?\n"));

    } while (0);

    if (RTFileIsValid(fh))
        RTFileClose(fh);

    if (RT_FAILURE(rc))
    {
        if (bCreatedFile)
            RTFileDelete(pszTempFile);
    }
    return rc;
}


/**
 * Extracts a built-in resource to disk.
 *
 * @returns iprt status code.
 *
 * @param   pPackage            Pointer to a VBOXSTUBPKG struct that contains the resource.
 * @param   pszTempFile         The full file path + name to extract the resource to.
 *
 */
static int Extract(const PVBOXSTUBPKG  pPackage,
                   const char         *pszTempFile)
{
    return ExtractFile(pPackage->szResourceName,
                       pszTempFile);
}


/**
 * Detects whether we're running on a 32- or 64-bit platform and returns the result.
 *
 * @returns TRUE if we're running on a 64-bit OS, FALSE if not.
 *
 */
static BOOL IsWow64(void)
{
    BOOL bIsWow64 = TRUE;
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
        {
            /* Error in retrieving process type - assume that we're running on 32bit. */
            return FALSE;
        }
    }
    return bIsWow64;
}


/**
 * Decides whether we need a specified package to handle or not.
 *
 * @returns TRUE if we need to handle the specified package, FALSE if not.
 *
 * @param   pPackage            Pointer to a VBOXSTUBPKG struct that contains the resource.
 *
 */
static BOOL PackageIsNeeded(PVBOXSTUBPKG pPackage)
{
    BOOL bIsWow64 = IsWow64();
    if ((bIsWow64 && pPackage->byArch == VBOXSTUBPKGARCH_AMD64)) /* 64bit Windows. */
    {
        return TRUE;
    }
    else if ((!bIsWow64 && pPackage->byArch == VBOXSTUBPKGARCH_X86)) /* 32bit. */
    {
        return TRUE;
    }
    else if (pPackage->byArch == VBOXSTUBPKGARCH_ALL)
    {
        return TRUE;
    }
    return FALSE;
}


/**
 * Processes an MSI package.
 *
 * @returns Fully complained exit code.
 * @param   iPackage            The package number.
 * @param   pszMsi              The path to the MSI to process.
 * @param   pszMsiArgs          Any additional installer (MSI) argument
 * @param   fLogging            Whether to enable installer logging.
 */
static RTEXITCODE ProcessMsiPackage(unsigned iPackage, const char *pszMsi, const char *pszMsiArgs, bool fLogging)
{
    int rc;

    /*
     * Set UI level.
     */
    INSTALLUILEVEL enmDesiredUiLevel = g_fSilent ? INSTALLUILEVEL_NONE : INSTALLUILEVEL_FULL;
    INSTALLUILEVEL enmRet = MsiSetInternalUI(enmDesiredUiLevel, NULL);
    if (enmRet == INSTALLUILEVEL_NOCHANGE /* means error */)
        return ShowError("Internal error: MsiSetInternalUI failed.");

    /*
     * Enable logging?
     */
    if (fLogging)
    {
        char szLogFile[RTPATH_MAX];
        rc = RTStrCopy(szLogFile, sizeof(szLogFile), pszMsi);
        if (RT_SUCCESS(rc))
        {
            RTPathStripFilename(szLogFile);
            rc = RTPathAppend(szLogFile, sizeof(szLogFile), "VBoxInstallLog.txt");
        }
        if (RT_FAILURE(rc))
            return ShowError("Internal error: Filename path too long.");

        PRTUTF16 pwszLogFile;
        rc = RTStrToUtf16(szLogFile, &pwszLogFile);
        if (RT_FAILURE(rc))
            return ShowError("RTStrToUtf16 failed on '%s': %Rrc", szLogFile, rc);

        UINT uLogLevel = MsiEnableLogW(INSTALLLOGMODE_VERBOSE,
                                       pwszLogFile,
                                       INSTALLLOGATTRIBUTES_FLUSHEACHLINE | (iPackage > 0 ? INSTALLLOGATTRIBUTES_APPEND : 0));
        RTUtf16Free(pwszLogFile);
        if (uLogLevel != ERROR_SUCCESS)
            return ShowError("MsiEnableLogW failed");
    }

    /*
     * Initialize the common controls (extended version). This is necessary to
     * run the actual .MSI installers with the new fancy visual control
     * styles (XP+). Also, an integrated manifest is required.
     */
    INITCOMMONCONTROLSEX ccEx;
    ccEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    ccEx.dwICC = ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES | ICC_PAGESCROLLER_CLASS |
                 ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                 ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&ccEx); /* Ignore failure. */

    /*
     * Convert both strings to UTF-16 and start the installation.
     */
    PRTUTF16 pwszMsi;
    rc = RTStrToUtf16(pszMsi, &pwszMsi);
    if (RT_FAILURE(rc))
        return ShowError("RTStrToUtf16 failed on '%s': %Rrc", pszMsi, rc);
    PRTUTF16 pwszMsiArgs;
    rc = RTStrToUtf16(pszMsiArgs, &pwszMsiArgs);
    if (RT_FAILURE(rc))
    {
        RTUtf16Free(pwszMsi);
        return ShowError("RTStrToUtf16 failed on '%s': %Rrc", pszMsi, rc);
    }

    UINT uStatus = MsiInstallProductW(pwszMsi, pwszMsiArgs);
    RTUtf16Free(pwszMsi);
    RTUtf16Free(pwszMsiArgs);

    if (uStatus == ERROR_SUCCESS)
        return RTEXITCODE_SUCCESS;
    if (uStatus == ERROR_SUCCESS_REBOOT_REQUIRED)
        return RTEXITCODE_SUCCESS; /* we currently don't indicate this */

    /*
     * Installation failed. Figure out what to say.
     */
    switch (uStatus)
    {
        case ERROR_INSTALL_USEREXIT:
            /* Don't say anything? */
            break;

        case ERROR_INSTALL_PACKAGE_VERSION:
            ShowError("This installation package cannot be installed by the Windows Installer service.\n"
                      "You must install a Windows service pack that contains a newer version of the Windows Installer service.");
            break;

        case ERROR_INSTALL_PLATFORM_UNSUPPORTED:
            ShowError("This installation package is not supported on this platform.");
            break;

        default:
        {
            /*
             * Try get windows to format the message.
             */
            DWORD dwFormatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER
                                | FORMAT_MESSAGE_IGNORE_INSERTS
                                | FORMAT_MESSAGE_FROM_SYSTEM;
            HMODULE hModule = NULL;
            if (uStatus >= NERR_BASE && uStatus <= MAX_NERR)
            {
                hModule = LoadLibraryEx(TEXT("netmsg.dll"),
                                        NULL,
                                        LOAD_LIBRARY_AS_DATAFILE);
                if (hModule != NULL)
                    dwFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
            }

            /** @todo this is totally WRONG wrt to string code pages. We expect UTF-8
             *        while the ANSI code page might be one of the special Chinese ones,
             *        IPRT is going to be so angry with us (and so will the users). */
            DWORD dwBufferLength;
            LPSTR szMessageBuffer;
            if (dwBufferLength = FormatMessageA(dwFormatFlags,
                                                hModule, /* If NULL, load system stuff. */
                                                uStatus,
                                                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                                (LPSTR)&szMessageBuffer,
                                                0,
                                                NULL))
            {
                ShowError("Installation failed! Error: %s", szMessageBuffer);
                LocalFree(szMessageBuffer);
            }
            else /* If text lookup failed, show at least the error number. */
                ShowError("Installation failed! Error: %u", uStatus);

            if (hModule)
                FreeLibrary(hModule);
            break;
        }
    }

    return RTEXITCODE_FAILURE;
}


/**
 * Processes a package.
 *
 * @returns Fully complained exit code.
 * @param   iPackage            The package number.
 * @param   pszPkgDir           The package directory (aka extraction dir).
 * @param   pszMsiArgs          Any additional installer (MSI) argument
 * @param   fLogging            Whether to enable installer logging.
 */
static RTEXITCODE ProcessPackage(unsigned iPackage, const char *pszPkgDir, const char *pszMsiArgs, bool fLogging)
{
    /*
     * Get the package header and check if it's needed.
     */
    PVBOXSTUBPKG pPackage = FindPackageHeader(iPackage);
    if (pPackage == NULL)
        return RTEXITCODE_FAILURE;

    if (!PackageIsNeeded(pPackage))
        return RTEXITCODE_SUCCESS;

    /*
     * Deal with the file based on it's extension.
     */
    char *pszPkgFile = RTPathJoinA(pszPkgDir, pPackage->szFileName);
    if (!pszPkgFile)
        return ShowError("Out of memory on line #%u!", __LINE__);
    RTPathChangeToDosSlashes(pszPkgFile, true /* Force conversion. */); /* paranoia */

    RTEXITCODE rcExit;
    const char *pszExt = RTPathExt(pszPkgFile);
    if (RTStrICmp(pszExt, ".msi") == 0)
        rcExit = ProcessMsiPackage(iPackage, pszPkgFile, pszMsiArgs, fLogging);
    else
        rcExit = ShowError("Internal error: Do not know how to handle file '%s'.", pPackage->szFileName);

    RTStrFree(pszPkgFile);
    return rcExit;
}


#ifdef VBOX_WITH_CODE_SIGNING
/**
 * Install the public certificate into TrustedPublishers so the installer won't
 * prompt the user during silent installs.
 *
 * @returns Fully complained exit code.
 */
static RTEXITCODE InstallCertificate(void)
{
    if (addCertToStore(CERT_SYSTEM_STORE_LOCAL_MACHINE,
                       "TrustedPublisher",
                       g_ab_VBoxStubPublicCert,
                       sizeof(g_ab_VBoxStubPublicCert)))
        return RTEXITCODE_SUCCESS;
    return ShowError("Failed to construct install certificate.");
}
#endif /* VBOX_WITH_CODE_SIGNING */


/**
 * Copies the "<exepath>.custom" directory to the extraction path if it exists.
 *
 * This is used by the MSI packages from the resource section.
 *
 * @returns Fully complained exit code.
 * @param   pszDstDir       The destination directory.
 */
static RTEXITCODE CopyCustomDir(const char *pszDstDir)
{
    char szSrcDir[RTPATH_MAX];
    int rc = RTPathExecDir(szSrcDir, sizeof(szSrcDir) - 1);
    if (RT_SUCCESS(rc))
        rc = RTPathAppend(szSrcDir, sizeof(szSrcDir) - 1, ".custom");
    if (RT_FAILURE(rc))
        return ShowError("Failed to construct '.custom' dir path: %Rrc", rc);

    if (RTDirExists(szSrcDir))
    {
        /*
         * Use SHFileOperation w/ FO_COPY to do the job.  This API requires an
         * extra zero at the end of both source and destination paths.  Thus
         * the -1 above and below.
         */
        size_t   cwc;
        RTUTF16  wszSrcDir[RTPATH_MAX + 2];
        PRTUTF16 pwszSrcDir = wszSrcDir;
        rc = RTStrToUtf16Ex(szSrcDir, RTSTR_MAX, &pwszSrcDir, RTPATH_MAX, &cwc);
        if (RT_FAILURE(rc))
            return ShowError("RTStrToUtf16Ex failed on '%s': %Rrc", szSrcDir, rc);
        wszSrcDir[cwc] = '\0';

        RTUTF16  wszDstDir[RTPATH_MAX + 2];
        PRTUTF16 pwszDstDir = wszSrcDir;
        rc = RTStrToUtf16Ex(pszDstDir, RTSTR_MAX, &pwszDstDir, RTPATH_MAX, &cwc);
        if (RT_FAILURE(rc))
            return ShowError("RTStrToUtf16Ex failed on '%s': %Rrc", pszDstDir, rc);
        wszDstDir[cwc] = '\0';

        SHFILEOPSTRUCTW FileOp;
        RT_ZERO(FileOp); /* paranoia */
        FileOp.hwnd     = NULL;
        FileOp.wFunc    = FO_COPY;
        FileOp.pFrom    = wszSrcDir;
        FileOp.pTo      = wszDstDir;
        FileOp.fFlags   = FOF_SILENT
                        | FOF_NOCONFIRMATION
                        | FOF_NOCONFIRMMKDIR
                        | FOF_NOERRORUI;
        FileOp.fAnyOperationsAborted = FALSE;
        FileOp.hNameMappings = NULL;
        FileOp.lpszProgressTitle = NULL;

        rc = SHFileOperationW(&FileOp);
        if (rc != 0)    /* Not a Win32 status code! */
            return ShowError("Copying the '.custom' dir failed: %#x", rc);
    }

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE ExtractFiles(unsigned cPackages, const char *pszDstDir, bool fExtractOnly)
{
    int rc;

    /*
     * Make sure the directory exists.
     */
    if (!RTDirExists(pszDstDir))
    {
        rc = RTDirCreate(pszDstDir, 0700, 0);
        if (RT_FAILURE(rc))
            return ShowError("Failed to create extraction path '%s': %Rrc", pszDstDir, rc);
    }

    /*
     * Extract files.
     */
    for (unsigned k = 0; k < cPackages; k++)
    {
        PVBOXSTUBPKG pPackage = FindPackageHeader(k);
        if (!pPackage)
            return RTEXITCODE_FAILURE; /* Done complaining already. */

        if (fExtractOnly || PackageIsNeeded(pPackage))
        {
            char *pszDstFile = RTPathJoinA(pszDstDir, pPackage->szFileName);
            if (!pszDstFile)
                return ShowError("Out of memory on line %u!", __LINE__);

            rc = Extract(pPackage, pszDstFile);
            RTStrFree(pszDstFile);
            if (RT_FAILURE(rc))
                return ShowError("Error extracting package #%u: %Rrc", k, rc);
        }
    }

    return RTEXITCODE_SUCCESS;
}


int WINAPI WinMain(HINSTANCE  hInstance,
                   HINSTANCE  hPrevInstance,
                   char      *lpCmdLine,
                   int        nCmdShow)
{
    char **argv = __argv;
    int argc    = __argc;

    /* Check if we're already running and jump out if so. */
    /* Do not use a global namespace ("Global\\") for mutex name here, will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex(NULL, FALSE, "VBoxStubInstaller");
    if (   hMutexAppRunning != NULL
        && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        /* Close the mutex for this application instance. */
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
        return RTEXITCODE_FAILURE;
    }

    /* Init IPRT. */
    int vrc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(vrc))
        return RTMsgInitFailure(vrc);

    /*
     * Parse arguments.
     */

    /* Parameter variables. */
    bool fExtractOnly              = false;
    bool fEnableLogging            = false;
#ifdef VBOX_WITH_CODE_SIGNING
    bool fEnableSilentCert         = true;
#endif
    char szExtractPath[RTPATH_MAX] = {0};
    char szMSIArgs[4096]           = {0};

    /* Parameter definitions. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--extract",          'x', RTGETOPT_REQ_NOTHING },
        { "-extract",           'x', RTGETOPT_REQ_NOTHING },
        { "/extract",           'x', RTGETOPT_REQ_NOTHING },
        { "--silent",           's', RTGETOPT_REQ_NOTHING },
        { "-silent",            's', RTGETOPT_REQ_NOTHING },
        { "/silent",            's', RTGETOPT_REQ_NOTHING },
#ifdef VBOX_WITH_CODE_SIGNING
        { "--no-silent-cert",   'c', RTGETOPT_REQ_NOTHING },
        { "-no-silent-cert",    'c', RTGETOPT_REQ_NOTHING },
        { "/no-silent-cert",    'c', RTGETOPT_REQ_NOTHING },
#endif
        { "--logging",          'l', RTGETOPT_REQ_NOTHING },
        { "-logging",           'l', RTGETOPT_REQ_NOTHING },
        { "/logging",           'l', RTGETOPT_REQ_NOTHING },
        { "--path",             'p', RTGETOPT_REQ_STRING  },
        { "-path",              'p', RTGETOPT_REQ_STRING  },
        { "/path",              'p', RTGETOPT_REQ_STRING  },
        { "--msiparams",        'm', RTGETOPT_REQ_STRING  },
        { "-msiparams",         'm', RTGETOPT_REQ_STRING  },
        { "--version",          'V', RTGETOPT_REQ_NOTHING },
        { "-version",           'V', RTGETOPT_REQ_NOTHING },
        { "/version",           'V', RTGETOPT_REQ_NOTHING },
        { "-v",                 'V', RTGETOPT_REQ_NOTHING },
        { "--help",             'h', RTGETOPT_REQ_NOTHING },
        { "-help",              'h', RTGETOPT_REQ_NOTHING },
        { "/help",              'h', RTGETOPT_REQ_NOTHING },
        { "/?",                 'h', RTGETOPT_REQ_NOTHING },
    };

    /* Parse the parameters. */
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'x':
                fExtractOnly = true;
                break;

            case 's':
                g_fSilent = true;
                break;

#ifdef VBOX_WITH_CODE_SIGNING
            case 'c':
                fEnableSilentCert = false;
                break;
#endif

            case 'l':
                fEnableLogging = true;
                break;

            case 'p':
                vrc = RTStrCopy(szExtractPath, sizeof(szExtractPath), ValueUnion.psz);
                if (RT_FAILURE(vrc))
                    return ShowError("Extraction path is too long.");
                break;

            case 'm':
                if (szMSIArgs[0])
                    vrc = RTStrCat(szMSIArgs, sizeof(szMSIArgs), " ");
                if (RT_SUCCESS(vrc))
                    vrc = RTStrCat(szMSIArgs, sizeof(szMSIArgs), ValueUnion.psz);
                if (RT_FAILURE(vrc))
                    return ShowError("MSI parameters are too long.");
                break;

            case 'V':
                ShowInfo("Version: %d.%d.%d.%d",
                         VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);
                return VINF_SUCCESS;

            case 'h':
                ShowInfo("-- %s v%d.%d.%d.%d --\n"
                         "Command Line Parameters:\n\n"
                         "--extract                - Extract file contents to temporary directory\n"
                         "--silent                 - Enables silent mode installation\n"
                         "--no-silent-cert         - Do not install VirtualBox Certificate automatically when --silent option is specified\n"
                         "--path                   - Sets the path of the extraction directory\n"
                         "--msiparams <parameters> - Specifies extra parameters for the MSI installers\n"
                         "--logging                - Enables installer logging\n"
                         "--help                   - Print this help and exit\n"
                         "--version                - Print version number and exit\n\n"
                         "Examples:\n"
                         "%s --msiparams INSTALLDIR=C:\\VBox\n"
                         "%s --extract -path C:\\VBox",
                         VBOX_STUB_TITLE, VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV,
                         argv[0], argv[0]);
                return VINF_SUCCESS;

            default:
                if (g_fSilent)
                    return RTGetOptPrintError(ch, &ValueUnion);
                if (ch == VINF_GETOPT_NOT_OPTION || ch == VERR_GETOPT_UNKNOWN_OPTION)
                    ShowError("Unknown option \"%s\"!\n"
                              "Please refer to the command line help by specifying \"/?\"\n"
                              "to get more information.", ValueUnion.psz);
                else
                    ShowError("Parameter parsing error: %Rrc\n"
                              "Please refer to the command line help by specifying \"/?\"\n"
                              "to get more information.", ch);
                return RTEXITCODE_SYNTAX;

        }
    }

    /*
     * Determine the extration path if not given by the user, and gather some
     * other bits we'll be needing later.
     */
    if (szExtractPath[0] == '\0')
    {
        vrc = RTPathTemp(szExtractPath, sizeof(szExtractPath));
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppend(szExtractPath, sizeof(szExtractPath), "VirtualBox");
        if (RT_FAILURE(vrc))
            return ShowError("Failed to determin extraction path (%Rrc)", vrc);

    }
    RTPathChangeToDosSlashes(szExtractPath, true /* Force conversion. */); /* MSI requirement. */

    /* Read our manifest. */
    PVBOXSTUBPKGHEADER pHeader;
    vrc = FindData("MANIFEST", (PVOID *)&pHeader, NULL);
    if (RT_FAILURE(vrc))
        return ShowError("Internal package error: Manifest not found (%Rrc)", vrc);
    /** @todo If we could, we should validate the header. Only the magic isn't
     *        commonly defined, nor the version number... */

    /*
     * Up to this point, we haven't done anything that requires any cleanup.
     * From here on, we do everything in function so we can counter clean up.
     */
    RTEXITCODE rcExit = ExtractFiles(pHeader->byCntPkgs, szExtractPath, fExtractOnly);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        if (fExtractOnly)
        {
            if (!g_fSilent)
                ShowInfo("Files were extracted to: %s", szExtractPath);
        }
        else
        {
            rcExit = CopyCustomDir(szExtractPath);
#ifdef VBOX_WITH_CODE_SIGNING
            if (rcExit == RTEXITCODE_SUCCESS && fEnableSilentCert && g_fSilent)
                InstallCertificate();
#endif
            unsigned iPackage = 0;
            while (iPackage < pHeader->byCntPkgs && rcExit == RTEXITCODE_SUCCESS)
            {
                rcExit = ProcessPackage(iPackage, szExtractPath, szMSIArgs, fEnableLogging);
                iPackage++;
            }
        }
    }



    do /* break loop */
    {

        /* Clean up (only on success - prevent deleting the log). */
        if (   !fExtractOnly
            && RT_SUCCESS(vrc))
        {
            for (int i = 0; i < 5; i++)
            {
                vrc = RTDirRemoveRecursive(szExtractPath, 0 /*fFlags*/);
                if (RT_SUCCESS(vrc))
                    break;
                RTThreadSleep(3000 /* Wait 3 seconds.*/);
            }
        }

    } while (0);

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL)
    {
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    return rcExit;
}

