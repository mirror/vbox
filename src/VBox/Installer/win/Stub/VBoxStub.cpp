/* $Id$ */
/** @file
 * VBoxStub - VirtualBox's Windows installer stub.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "VBoxStub.h"
#include "../StubBld/VBoxStubBld.h"
#include "resource.h"

#ifndef  _UNICODE
#define  _UNICODE
#endif


/**
 * Shows a message box with a printf() style formatted string.
 *
 * @returns Message box result (IDOK, IDCANCEL, ...).
 *
 * @param   uType               Type of the message box (see MSDN).
 * @param   pszFmt              Printf-style format string to show in the message box body.
 *
 */
static int ShowInfo(const char *pszFmt, ...)
{
    char       *pszMsg;
    va_list     va;

    va_start(va, pszFmt);
    RTStrAPrintfV(&pszMsg, pszFmt, va);
    va_end(va);

    int rc;
    if (pszMsg)
        rc = MessageBox(GetDesktopWindow(), pszMsg, VBOX_STUB_TITLE, MB_ICONINFORMATION);
    else
        rc = MessageBox(GetDesktopWindow(), pszFmt, VBOX_STUB_TITLE, MB_ICONINFORMATION);
    RTStrFree(pszMsg);
    return rc;
}


/**
 * Shows an error message box with a printf() style formatted string.
 *
 * @returns Message box result (IDOK, IDCANCEL, ...).
 *
 * @param   pszFmt              Printf-style format string to show in the message box body.
 *
 */
static int ShowError(const char *pszFmt, ...)
{
    char       *pszMsg;
    va_list     va;
    int         rc;

    va_start(va, pszFmt);
    if (RTStrAPrintfV(&pszMsg, pszFmt, va))
    {
        rc = MessageBox(GetDesktopWindow(), pszMsg, VBOX_STUB_TITLE, MB_ICONERROR);
        RTStrFree(pszMsg);
    }
    else /* Should never happen! */
        AssertMsgFailed(("Failed to format error text of format string: %s!\n", pszFmt));
    va_end(va);
    return rc;
}


/**
 * Reads data from a built-in resource.
 *
 * @returns iprt status code.
 *
 * @param   hInst               Instance to read the data from.
 * @param   pszDataName         Name of resource to read.
 * @param   ppvResource         Pointer to buffer which holds the read resource data.
 * @param   pdwSize             Pointer which holds the read data size.
 *
 */
static int ReadData(HINSTANCE   hInst,
                    const char *pszDataName,
                    PVOID      *ppvResource,
                    DWORD      *pdwSize)
{
    do
    {
        AssertMsgBreak(pszDataName, ("Resource name is empty!\n"));

        /* Find our resource. */
        HRSRC hRsrc = FindResourceEx(hInst, RT_RCDATA, pszDataName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
        AssertMsgBreak(hRsrc, ("Could not find resource!\n"));

        /* Get resource size. */
        *pdwSize = SizeofResource(hInst, hRsrc);
        AssertMsgBreak(*pdwSize > 0, ("Size of resource is invalid!\n"));

        /* Get pointer to resource. */
        HGLOBAL hData = LoadResource(hInst, hRsrc);
        AssertMsgBreak(hData, ("Could not load resource!\n"));

        /* Lock resource. */
        *ppvResource = LockResource(hData);
        AssertMsgBreak(*ppvResource, ("Could not lock resource!\n"));
        return VINF_SUCCESS;

    } while (0);

    return VERR_IO_GEN_FAILURE;
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
        rc = ReadData(NULL, pszResourceName, &pvData, &dwDataSize);
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
 * Recursively copies a directory to another location.
 *
 * @returns iprt status code.
 *
 * @param   pszDestDir          Location to copy the source directory to.
 * @param   pszSourceDir        The source directory to copy.
 *
 */
int CopyDir(const char *pszDestDir, const char *pszSourceDir)
{
    char szDest[RTPATH_MAX + 1];
    char szSource[RTPATH_MAX + 1];

    AssertStmt(pszDestDir, "Destination directory invalid!");
    AssertStmt(pszSourceDir, "Source directory invalid!");

    SHFILEOPSTRUCT s = {0};
    if (   RTStrPrintf(szDest, _MAX_PATH, "%s%c", pszDestDir, '\0') > 0
        && RTStrPrintf(szSource, _MAX_PATH, "%s%c", pszSourceDir, '\0') > 0)
    {
        s.hwnd = NULL;
        s.wFunc = FO_COPY;
        s.pTo = szDest;
        s.pFrom = szSource;
        s.fFlags = FOF_SILENT |
                   FOF_NOCONFIRMATION |
                   FOF_NOCONFIRMMKDIR |
                   FOF_NOERRORUI;
    }
    return RTErrConvertFromWin32(SHFileOperation(&s));
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
    HANDLE hMutexAppRunning = CreateMutex (NULL, FALSE, "VBoxStubInstaller");
    if (   (hMutexAppRunning != NULL)
        && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        /* Close the mutex for this application instance. */
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
        return 1;
    }

    /* Init IPRT. */
    int vrc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(vrc))
        return vrc;

    BOOL fExtractOnly = FALSE;
    BOOL fSilent = FALSE;
    BOOL fEnableLogging = FALSE;
    BOOL fExit = FALSE;

    /* Temp variables for arguments. */
    char szExtractPath[RTPATH_MAX] = {0};
    char szMSIArgs[RTPATH_MAX] = {0};

    /* Process arguments. */
    for (int i = 0; i < argc; i++)
    {
        if (   (0 == RTStrICmp(argv[i], "-x"))
            || (0 == RTStrICmp(argv[i], "-extract"))
            || (0 == RTStrICmp(argv[i], "/extract")))
        {
            fExtractOnly = TRUE;
        }

        else if (   (0 == RTStrICmp(argv[i], "-s"))
                 || (0 == RTStrICmp(argv[i], "-silent"))
                 || (0 == RTStrICmp(argv[i], "/silent")))
        {
            fSilent = TRUE;
        }

        else if (   (0 == RTStrICmp(argv[i], "-l"))
                 || (0 == RTStrICmp(argv[i], "-logging"))
                 || (0 == RTStrICmp(argv[i], "/logging")))
        {
            fEnableLogging = TRUE;
        }

        else if ((  (0 == RTStrICmp(argv[i], "-p"))
                 || (0 == RTStrICmp(argv[i], "-path"))
                 || (0 == RTStrICmp(argv[i], "/path")))
                 )
        {
            if (argc > i)
            {
                vrc = ::StringCbCat(szExtractPath, sizeof(szExtractPath), argv[i+1]);
                i++; /* Avoid the specified path from being parsed. */
            }
            else
            {
                ShowError("No path for extraction specified!");
                fExit = TRUE;
            }
        }

        else if ((  (0 == RTStrICmp(argv[i], "-msiparams"))
                 || (0 == RTStrICmp(argv[i], "/msiparams")))
                 && (argc > i))
        {
            for (int a = i + 1; a < argc; a++)
            {
                if (a > i+1) /* Insert a space. */
                    vrc = ::StringCbCat(szMSIArgs, sizeof(szMSIArgs), " ");

                vrc = ::StringCbCat(szMSIArgs, sizeof(szMSIArgs), argv[a]);
            }
        }

        else if (   (0 == RTStrICmp(argv[i], "-v"))
                 || (0 == RTStrICmp(argv[i], "-version"))
                 || (0 == RTStrICmp(argv[i], "/version")))
        {
            ShowInfo("Version: %d.%d.%d.%d",
                     VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);
            fExit = TRUE;
        }

        else if (   (0 == RTStrICmp(argv[i], "-help"))
                 || (0 == RTStrICmp(argv[i], "/help"))
                 || (0 == RTStrICmp(argv[i], "/?")))
        {
            ShowInfo("-- %s v%d.%d.%d.%d --\n"
                         "Command Line Parameters:\n\n"
                         "-extract | -x           - Extract file contents to temporary directory\n"
                         "-silent | -s            - Enables silent mode installation\n"
                         "-path | -p              - Sets the path of the extraction directory\n"
                         "-help | /?              - Print this help and exit\n"
                         "-msiparams <parameters> - Specifies extra parameters for the MSI installers\n"
                         "-logging | -l           - Enables installer logging\n"
                         "-version | -v           - Print version number and exit\n\n"
                         "Examples:\n"
                         "%s -msiparams INSTALLDIR=C:\\VBox\n"
                         "%s -extract -path C:\\VBox\n",
                         VBOX_STUB_TITLE, VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV,
                         argv[0], argv[0]);
            fExit = TRUE;
        }
        else
        {
            if (i > 0)
            {
                ShowError("Unknown option \"%s\"!\n"
                          "Please refer to the command line help by specifying \"/?\"\n"
                          "to get more information.", argv[i]);
                fExit = TRUE;
            }
        }
    }

    if (fExit)
        return 0;

    HRESULT hr = S_OK;

    do /* break loop */
    {
        /* Get/create our temp path (only if not already set). */
        if (szExtractPath[0] == '\0')
        {
            vrc = RTPathTemp(szExtractPath, sizeof(szExtractPath));
            AssertMsgRCBreak(vrc, ("Could not retrieve temp directory!\n"));

            vrc = RTPathAppend(szExtractPath, sizeof(szExtractPath), "VirtualBox");
            AssertMsgRCBreak(vrc, ("Could not construct temp directory!\n"));

            /* Convert slahes; this is necessary for MSI routines later! */
            RTPathChangeToDosSlashes(szExtractPath, true /* Force conversion. */);
        }
        if (!RTDirExists(szExtractPath))
        {
            vrc = RTDirCreate(szExtractPath, 0700, 0);
            AssertMsgRCBreak(vrc, ("Could not create temp directory!\n"));
        }

        /* Get our executable path */
        char szPathExe[_MAX_PATH];
        vrc = RTPathExecDir(szPathExe, sizeof(szPathExe));
        /** @todo error checking */

        /* Read our manifest. */
        PVBOXSTUBPKGHEADER pHeader = NULL;
        DWORD cbHeader = 0;
        vrc = ReadData(NULL, "MANIFEST", (LPVOID*)&pHeader, &cbHeader);
        AssertMsgRCBreak(vrc, ("Manifest not found!\n"));

        /* Extract files. */
        for (BYTE k = 0; k < pHeader->byCntPkgs; k++)
        {
            PVBOXSTUBPKG pPackage = NULL;
            DWORD cbPackage = 0;
            char szHeaderName[RTPATH_MAX + 1] = {0};

            hr = ::StringCchPrintf(szHeaderName, RTPATH_MAX, "HDR_%02d", k);
            vrc = ReadData(NULL, szHeaderName, (LPVOID*)&pPackage, &cbPackage);
            AssertMsgRCBreak(vrc, ("Header not found!\n")); /** @todo include header name, how? */

            if (PackageIsNeeded(pPackage) || fExtractOnly)
            {
                char *pszTempFile = NULL;
                vrc = GetTempFileAlloc(szExtractPath, pPackage->szFileName, &pszTempFile);
                AssertMsgRCBreak(vrc, ("Could not create name for temporary extracted file!\n"));
                vrc = Extract(pPackage, pszTempFile);
                AssertMsgRCBreak(vrc, ("Could not extract file!\n"));
                RTStrFree(pszTempFile);
            }
        }

        if (FALSE == fExtractOnly && !RT_FAILURE(vrc))
        {
            /*
             * Copy ".custom" directory into temp directory so that the extracted .MSI
             * file(s) can use it.
             */
            char *pszPathCustomDir = RTPathJoinA(szPathExe, ".custom");
            pszPathCustomDir = RTPathChangeToDosSlashes(pszPathCustomDir, true /* Force conversion. */);
            if (pszPathCustomDir && RTDirExists(pszPathCustomDir))
            {
                vrc = CopyDir(szExtractPath, pszPathCustomDir);
                if (RT_FAILURE(vrc)) /* Don't fail if it's missing! */
                    vrc = VINF_SUCCESS;

                RTStrFree(pszPathCustomDir);
            }

            /* Do actions on files. */
            for (BYTE k = 0; k < pHeader->byCntPkgs; k++)
            {
                PVBOXSTUBPKG pPackage = NULL;
                DWORD cbPackage = 0;
                char szHeaderName[RTPATH_MAX] = {0};

                hr = StringCchPrintf(szHeaderName, RTPATH_MAX, "HDR_%02d", k);
                vrc = ReadData(NULL, szHeaderName, (LPVOID*)&pPackage, &cbPackage);
                AssertMsgRCBreak(vrc, ("Package not found!\n"));

                if (PackageIsNeeded(pPackage))
                {
                    char *pszTempFile = NULL;

                    vrc = GetTempFileAlloc(szExtractPath, pPackage->szFileName, &pszTempFile);
                    AssertMsgRCBreak(vrc, ("Could not create name for temporary action file!\n"));

                    /* Handle MSI files. */
                    if (RTStrICmp(RTPathExt(pszTempFile), ".msi") == 0)
                    {
                        /* Set UI level. */
                        INSTALLUILEVEL UILevel = MsiSetInternalUI(  fSilent
                                                                  ? INSTALLUILEVEL_NONE
                                                                  : INSTALLUILEVEL_FULL,
                                                                    NULL);
                        AssertMsgBreak(UILevel != INSTALLUILEVEL_NOCHANGE, ("Could not set installer UI level!\n"));

                        /* Enable logging? */
                        if (fEnableLogging)
                        {
                            char *pszLog = RTPathJoinA(szExtractPath, "VBoxInstallLog.txt");
                            /* Convert slahes; this is necessary for MSI routines! */
                            pszLog = RTPathChangeToDosSlashes(pszLog, true /* Force conversion. */);
                            AssertMsgBreak(pszLog, ("Could not construct path for log file!\n"));
                            UINT uLogLevel = MsiEnableLog(INSTALLLOGMODE_VERBOSE,
                                                          pszLog, INSTALLLOGATTRIBUTES_FLUSHEACHLINE);
                            RTStrFree(pszLog);
                            AssertMsgBreak(uLogLevel == ERROR_SUCCESS, ("Could not set installer logging level!\n"));
                        }

                        /* Initialize the common controls (extended version). This is necessary to
                         * run the actual .MSI installers with the new fancy visual control
                         * styles (XP+). Also, an integrated manifest is required. */
                        INITCOMMONCONTROLSEX ccEx;
                        ccEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
                        ccEx.dwICC = ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES | ICC_PAGESCROLLER_CLASS |
                                     ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                                     ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES | ICC_WIN95_CLASSES;
                        InitCommonControlsEx(&ccEx); /* Ignore failure. */

                        UINT uStatus = ::MsiInstallProductA(pszTempFile, szMSIArgs);
                        if (   (uStatus != ERROR_SUCCESS)
                            && (uStatus != ERROR_SUCCESS_REBOOT_REQUIRED)
                            && (uStatus != ERROR_INSTALL_USEREXIT))
                        {
                            if (!fSilent)
                            {
                                switch (uStatus)
                                {
                                    case ERROR_INSTALL_PACKAGE_VERSION:

                                        ShowError("This installation package cannot be installed by the Windows Installer service.\n"
                                                  "You must install a Windows service pack that contains a newer version of the Windows Installer service.");
                                        break;

                                    case ERROR_INSTALL_PLATFORM_UNSUPPORTED:

                                        ShowError("This installation package is not supported on this platform.");
                                        break;

                                    default:
                                    {
                                        DWORD dwFormatFlags =   FORMAT_MESSAGE_ALLOCATE_BUFFER
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
                            }

                            vrc = VERR_NO_CHANGE; /* No change done to the system. */
                        }
                    }
                    RTStrFree(pszTempFile);
                } /* Package needed? */
            } /* For all packages */
        }

        /* Clean up (only on success - prevent deleting the log). */
        if (   !fExtractOnly
            && RT_SUCCESS(vrc))
        {
            for (int i=0; i<5; i++)
            {
                vrc = RTDirRemoveRecursive(szExtractPath, 0 /*fFlags*/);
                if (RT_SUCCESS(vrc))
                    break;
                RTThreadSleep(3000 /* Wait 3 seconds.*/);
            }
        }

    } while (0);

    if (RT_SUCCESS(vrc))
    {
        if (   fExtractOnly
            && !fSilent)
        {
            ShowInfo("Files were extracted to: %s", szExtractPath);
        }

        /** @todo Add more post installation stuff here if required. */
    }

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL)
    {
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    /* Set final exit (return) code (error level). */
    if (RT_FAILURE(vrc))
    {
        switch(vrc)
        {
            case VERR_NO_CHANGE:
            default:
                vrc = 1;
        }
    }
    else /* Always set to (VINF_SUCCESS), even if we got something else (like a VWRN etc). */
        vrc = VINF_SUCCESS;
    return vrc;
}

