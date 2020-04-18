/* $Id$ */
/** @file
 * VBoxStubBld - VirtualBox's Windows installer stub builder.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <iprt/win/windows.h>
#include <shellapi.h>
#include <strsafe.h>

#include <VBox/version.h>
#include <iprt/types.h>

#include "VBoxStubBld.h"

HRESULT GetFile (const char* pszFilePath,
                 HANDLE* phFile,
                 DWORD* pdwFileSize)
{
    HRESULT hr = S_OK;
    *phFile = CreateFile(pszFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == *phFile)
        hr = HRESULT_FROM_WIN32(GetLastError());
    else
    {
        *pdwFileSize = ::GetFileSize(*phFile, NULL);
        if (!*pdwFileSize)
            hr = HRESULT_FROM_WIN32(GetLastError());
    }
    return hr;
}

HRESULT UpdateResource(HANDLE hFile,
                       DWORD dwFileSize,
                       HANDLE hResourceUpdate,
                       const char *pszResourceType,
                       const char *pszResourceId)
{
    HRESULT hr = S_OK;

    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    PVOID pvFile = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, dwFileSize);
    if (!UpdateResourceA(hResourceUpdate, pszResourceType, pszResourceId,
                         MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), pvFile, dwFileSize))
        hr = HRESULT_FROM_WIN32(GetLastError());

    if (pvFile)
        UnmapViewOfFile(pvFile);

    if (hMap)
        CloseHandle(hMap);

    return hr;
}

static HRESULT IntegrateFile(HANDLE hResourceUpdate, const char *pszResourceType,
                             const char *pszResourceId, const char *pszFilePath)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwFileSize = 0;
    HRESULT hr = GetFile(pszFilePath, &hFile, &dwFileSize);
    if (SUCCEEDED(hr))
    {
        hr = UpdateResource(hFile, dwFileSize, hResourceUpdate, pszResourceType, pszResourceId);
        if (FAILED(hr))
            printf("ERROR: Error updating resource for file %s!", pszFilePath);
    }
    else
        hr = HRESULT_FROM_WIN32(GetLastError());

    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
    return hr;
}

static char *MyPathFilename(const char *pszPath)
{
    const char *pszName = pszPath;
    for (const char *psz = pszPath;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
            case ':':
                pszName = psz + 1;
                break;

            case '\\':
            case '/':
                pszName = psz + 1;
                break;

            /* the end */
            case '\0':
                if (*pszName)
                    return (char *)(void *)pszName;
                return NULL;
        }
    }

    /* will never get here */
}


int main(int argc, char* argv[])
{
    HRESULT hr = S_OK;
    int rcExit = RTEXITCODE_SUCCESS;

    char szSetupStub[_MAX_PATH] = {"VBoxStub.exe"};
    char szOutput[_MAX_PATH] = {"VirtualBox-MultiArch.exe"};
    HANDLE hUpdate = NULL;

    do /* goto avoidance "loop" */
    {
        printf(VBOX_PRODUCT " Stub Builder v%d.%d.%d.%d\n",
               VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);

        if (argc < 2)
            printf("WARNING: No parameters given! Using default values!\n");

        VBOXSTUBBUILDPKG stbBuildPkg[VBOXSTUB_MAX_PACKAGES] = {0};
        VBOXSTUBPKG stbPkg[VBOXSTUB_MAX_PACKAGES] = {0};
        VBOXSTUBPKGHEADER stbHeader =
        {
            "vbox$tub",    /* File magic. */
            1,             /* Version. */
            0              /* No files yet. */
        };

        for (int i=1; i<argc; i++)
        {
            if (!stricmp(argv[i], "-out") && argc > i+1)
            {
                hr = StringCchCopy(szOutput, _MAX_PATH, argv[i+1]);
                i++;
            }

            else if (!stricmp(argv[i], "-stub") && argc > i+1)
            {
                hr = StringCchCopy(szSetupStub, _MAX_PATH, argv[i+1]);
                i++;
            }

            else if (!stricmp(argv[i], "-target-all") && argc > i+1)
            {
                hr = StringCchCopy(stbBuildPkg[stbHeader.byCntPkgs].szSourcePath, _MAX_PATH, argv[i+1]);
                stbBuildPkg[stbHeader.byCntPkgs].byArch = VBOXSTUBPKGARCH_ALL;
                stbHeader.byCntPkgs++;
                i++;
            }

            else if (!stricmp(argv[i], "-target-x86") && argc > i+1)
            {
                hr = StringCchCopy(stbBuildPkg[stbHeader.byCntPkgs].szSourcePath, _MAX_PATH, argv[i+1]);
                stbBuildPkg[stbHeader.byCntPkgs].byArch = VBOXSTUBPKGARCH_X86;
                stbHeader.byCntPkgs++;
                i++;
            }

            else if (!stricmp(argv[i], "-target-amd64") && argc > i+1)
            {
                hr = StringCchCopy(stbBuildPkg[stbHeader.byCntPkgs].szSourcePath, _MAX_PATH, argv[i+1]);
                stbBuildPkg[stbHeader.byCntPkgs].byArch = VBOXSTUBPKGARCH_AMD64;
                stbHeader.byCntPkgs++;
                i++;
            }
            else
            {
                printf("ERROR: Invalid parameter: %s\n", argv[i]);
                hr = E_INVALIDARG;
                break;
            }
            if (FAILED(hr))
            {
                printf("ERROR: StringCchCopy failed: %#lx\n", hr);
                break;
            }
        }
        if (FAILED(hr))
            break;

        if (stbHeader.byCntPkgs <= 0)
        {
            printf("ERROR: No packages defined! Exiting.\n");
            break;
        }

        printf("Stub: %s\n", szSetupStub);
        printf("Output: %s\n", szOutput);
        printf("# Packages: %u\n", stbHeader.byCntPkgs);

        if (!CopyFile(szSetupStub, szOutput, FALSE))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            printf("ERROR: Could not create stub loader: %#lx\n", hr);
            break;
        }

        hUpdate = BeginUpdateResource(szOutput, FALSE);

        PVBOXSTUBPKG pPackage = stbPkg;
        char szHeaderName[_MAX_PATH] = {0};

        for (BYTE i = 0; i < stbHeader.byCntPkgs; i++)
        {
            printf("Integrating (Platform %d): %s\n", stbBuildPkg[i].byArch, stbBuildPkg[i].szSourcePath);

            /* Construct resource name. */
            hr = StringCchPrintf(pPackage->szResourceName, _MAX_PATH, "BIN_%02d", i);
            pPackage->byArch = stbBuildPkg[i].byArch;

            /* Construct final name used when extracting. */
            hr = StringCchCopy(pPackage->szFileName, _MAX_PATH, MyPathFilename(stbBuildPkg[i].szSourcePath));

            /* Integrate header into binary. */
            hr = StringCchPrintf(szHeaderName, _MAX_PATH, "HDR_%02d", i);
            hr = UpdateResource(hUpdate, RT_RCDATA, szHeaderName, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), pPackage, sizeof(VBOXSTUBPKG));

            /* Integrate file into binary. */
            hr = IntegrateFile(hUpdate, RT_RCDATA, pPackage->szResourceName, stbBuildPkg[i].szSourcePath);
            if (FAILED(hr))
            {
                printf("ERROR: Could not integrate binary %s (%s): %#lx\n",
                         pPackage->szResourceName, pPackage->szFileName, hr);
                rcExit = RTEXITCODE_FAILURE;
            }

            pPackage++;
        }

        if (FAILED(hr))
            break;

        if (!UpdateResource(hUpdate, RT_RCDATA, "MANIFEST", MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), &stbHeader, sizeof(VBOXSTUBPKGHEADER)))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        if (!EndUpdateResource(hUpdate, FALSE))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        printf("Integration done!\n");

    } while (0);

    hUpdate = NULL;

    if (FAILED(hr))
    {
        printf("ERROR: Building failed! Last error: %lu\n", GetLastError());
        rcExit = RTEXITCODE_FAILURE;
    }
    return rcExit;
}
