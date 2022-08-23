/* $Id$ */
/** @file
 * instdrvmain - Install guest drivers on NT4
 */

/*
 * Copyright (C) 2006-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/win/windows.h>
#include <iprt/win/setupapi.h>
#include <regstr.h>
#include <DEVGUID.h>

#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/utf16.h>



/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The video service name. */
#define VBOXGUEST_VIDEO_NAME        "VBoxVideo"

/** The video inf file name */
#define VBOXGUEST_VIDEO_INF_NAME    "VBoxVideo.inf"


/*
 * A few error messaging functions that avoids dragging in printf-like stuff.
 */

static RTEXITCODE ErrorMsg(const char *pszMsg)
{
    HANDLE const hStdOut = GetStdHandle(STD_ERROR_HANDLE);
    DWORD        cbIgn;
    WriteFile(hStdOut, RT_STR_TUPLE("error: "), &cbIgn, NULL);
    WriteFile(hStdOut, pszMsg, (DWORD)strlen(pszMsg), &cbIgn, NULL);
    WriteFile(hStdOut, RT_STR_TUPLE("\r\n"), &cbIgn, NULL);
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE ErrorMsgErr(const char *pszMsg, DWORD dwErr, const char *pszErrIntro, size_t cchErrIntro, bool fSigned)
{
    HANDLE const hStdOut = GetStdHandle(STD_ERROR_HANDLE);
    DWORD        cbIgn;
    WriteFile(hStdOut, RT_STR_TUPLE("error: "), &cbIgn, NULL);
    WriteFile(hStdOut, pszMsg, (DWORD)strlen(pszMsg), &cbIgn, NULL);
    WriteFile(hStdOut, pszErrIntro, (DWORD)cchErrIntro, &cbIgn, NULL);
    char    szVal[128];
    ssize_t cchVal = RTStrFormatU32(szVal, sizeof(szVal), dwErr, 10, 0, 0, fSigned ? RTSTR_F_VALSIGNED : 0);
    WriteFile(hStdOut, szVal, (DWORD)cchVal, &cbIgn, NULL);
    WriteFile(hStdOut, RT_STR_TUPLE("/"), &cbIgn, NULL);
    cchVal = RTStrFormatU32(szVal, sizeof(szVal), dwErr, 16, 0, 0, RTSTR_F_SPECIAL);
    WriteFile(hStdOut, szVal, (DWORD)cchVal, &cbIgn, NULL);
    WriteFile(hStdOut, RT_STR_TUPLE(")\r\n"), &cbIgn, NULL);
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE ErrorMsgLastErr(const char *pszMsg)
{
    return ErrorMsgErr(pszMsg, GetLastError(), RT_STR_TUPLE(" (last error "), false);
}


static RTEXITCODE ErrorMsgLStatus(const char *pszMsg, LSTATUS lrc)
{
    return ErrorMsgErr(pszMsg, (DWORD)lrc, RT_STR_TUPLE(" ("), true);
}



/**
 * Inner video driver installation function.
 *
 * This can normally return immediately on errors as the parent will do the
 * cleaning up.
 */
static RTEXITCODE InstallVideoDriverInner(WCHAR const * const pwszDriverDir, HDEVINFO hDevInfo, HINF *phInf)
{
    /*
     * Get the first found driver.
     * Our Inf file only contains one so this is fine.
     */
    SP_DRVINFO_DATA_W drvInfoData = { sizeof(SP_DRVINFO_DATA) };
    if (!SetupDiEnumDriverInfoW(hDevInfo, NULL, SPDIT_CLASSDRIVER, 0, &drvInfoData))
        return ErrorMsgLastErr("SetupDiEnumDriverInfoW");

    /*
     * Get necessary driver details
     */
    union
    {
        SP_DRVINFO_DETAIL_DATA_W s;
        uint64_t                 au64Padding[(sizeof(SP_DRVINFO_DETAIL_DATA_W) + 256) / sizeof(uint64_t)];
    } DriverInfoDetailData = { { sizeof(SP_DRVINFO_DETAIL_DATA) } };
    DWORD                    cbReqSize            = NULL;
    if (   !SetupDiGetDriverInfoDetailW(hDevInfo, NULL, &drvInfoData,
                                        &DriverInfoDetailData.s, sizeof(DriverInfoDetailData), &cbReqSize)
        && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return ErrorMsgLastErr("SetupDiGetDriverInfoDetailW");

    HINF hInf = *phInf = SetupOpenInfFileW(DriverInfoDetailData.s.InfFileName, NULL, INF_STYLE_WIN4, NULL);
    if (hInf == INVALID_HANDLE_VALUE)
        return ErrorMsgLastErr("SetupOpenInfFileW");

    /*
     * First install the service.
     */
    WCHAR wszServiceSection[LINE_LEN];
    int rc = RTUtf16Copy(wszServiceSection, RT_ELEMENTS(wszServiceSection), DriverInfoDetailData.s.SectionName);
    if (RT_SUCCESS(rc))
        rc = RTUtf16CatAscii(wszServiceSection, RT_ELEMENTS(wszServiceSection), ".Services");
    if (RT_FAILURE(rc))
        return ErrorMsg("wszServiceSection too small");

    INFCONTEXT SvcCtx;
    if (!SetupFindFirstLineW(hInf, wszServiceSection, NULL, &SvcCtx))
        return ErrorMsgLastErr("SetupFindFirstLine"); /* impossible... */

    /*
     * Get the name
     */
    WCHAR wszServiceData[LINE_LEN] = {0};
    if (!SetupGetStringFieldW(&SvcCtx, 1, wszServiceData, RT_ELEMENTS(wszServiceData), NULL))
        return ErrorMsgLastErr("SetupGetStringFieldW");

    WCHAR wszDevInstanceId[LINE_LEN];
    rc = RTUtf16CopyAscii(wszDevInstanceId, RT_ELEMENTS(wszDevInstanceId), "Root\\LEGACY_");
    if (RT_SUCCESS(rc))
        rc = RTUtf16Cat(wszDevInstanceId, RT_ELEMENTS(wszDevInstanceId), wszServiceData);
    if (RT_SUCCESS(rc))
        rc = RTUtf16CatAscii(wszDevInstanceId, RT_ELEMENTS(wszDevInstanceId), "\\0000");
    if (RT_FAILURE(rc))
        return ErrorMsg("wszDevInstanceId too small");

    /*
     * ...
     */
    SP_DEVINFO_DATA deviceInfoData = { sizeof(SP_DEVINFO_DATA) };
    /* Check for existing first. */
    BOOL fDevInfoOkay = SetupDiOpenDeviceInfoW(hDevInfo, wszDevInstanceId, NULL, 0, &deviceInfoData);
    if (!fDevInfoOkay)
    {
        /* Okay, try create a new device info element. */
        if (SetupDiCreateDeviceInfoW(hDevInfo, wszDevInstanceId, (LPGUID)&GUID_DEVCLASS_DISPLAY,
                                     NULL, // Do we need a description here?
                                     NULL, // No user interface
                                     0, &deviceInfoData))
        {
            if (SetupDiRegisterDeviceInfo(hDevInfo, &deviceInfoData, 0, NULL, NULL, NULL))
                fDevInfoOkay = TRUE;
            else
                return ErrorMsgLastErr("SetupDiRegisterDeviceInfo"); /** @todo Original code didn't return here. */
        }
        else
            return ErrorMsgLastErr("SetupDiCreateDeviceInfoW"); /** @todo Original code didn't return here. */
    }
    if (fDevInfoOkay) /** @todo if not needed if it's okay to fail on failure above */
    {
        /* We created a new key in the registry */ /* bogus... */

        /*
         * Redo the install parameter thing with deviceInfoData.
         */
        SP_DEVINSTALL_PARAMS_W DeviceInstallParams = { sizeof(SP_DEVINSTALL_PARAMS) };
        if (!SetupDiGetDeviceInstallParamsW(hDevInfo, &deviceInfoData, &DeviceInstallParams))
            return ErrorMsgLastErr("SetupDiGetDeviceInstallParamsW(#2)"); /** @todo Original code didn't return here. */

        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        DeviceInstallParams.Flags |= DI_NOFILECOPY      /* We did our own file copying */
                                   | DI_DONOTCALLCONFIGMG
                                   | DI_ENUMSINGLEINF;  /* .DriverPath specifies an inf file */
        rc = RTUtf16Copy(DeviceInstallParams.DriverPath, RT_ELEMENTS(DeviceInstallParams.DriverPath), pwszDriverDir);
        if (RT_SUCCESS(rc))
            rc = RTUtf16CatAscii(DeviceInstallParams.DriverPath, RT_ELEMENTS(DeviceInstallParams.DriverPath),
                                 VBOXGUEST_VIDEO_INF_NAME);
        if (RT_FAILURE(rc))
            return ErrorMsg("Install dir too deep (long)");

        if (!SetupDiSetDeviceInstallParamsW(hDevInfo, &deviceInfoData, &DeviceInstallParams))
            return ErrorMsgLastErr("SetupDiSetDeviceInstallParamsW(#2)"); /** @todo Original code didn't return here. */

        if (!SetupDiBuildDriverInfoList(hDevInfo, &deviceInfoData, SPDIT_CLASSDRIVER))
            return ErrorMsgLastErr("SetupDiBuildDriverInfoList(#2)");

        /*
         * Repeate the query at the start of the function.
         */
        drvInfoData.cbSize = sizeof(SP_DRVINFO_DATA);
        if (!SetupDiEnumDriverInfoW(hDevInfo, &deviceInfoData, SPDIT_CLASSDRIVER, 0, &drvInfoData))
            return ErrorMsgLastErr("SetupDiEnumDriverInfoW(#2)");

        /*
         * ...
         */
        if (!SetupDiSetSelectedDriverW(hDevInfo, &deviceInfoData, &drvInfoData))
            return ErrorMsgLastErr("SetupDiSetSelectedDriverW(#2)");

        if (!SetupDiInstallDevice(hDevInfo, &deviceInfoData))
            return ErrorMsgLastErr("SetupDiInstallDevice(#2)");
    }

    /*
     * Make sure the device is enabled.
     */
    DWORD fConfig = 0;
    if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &deviceInfoData, SPDRP_CONFIGFLAGS,
                                          NULL, (LPBYTE)&fConfig, sizeof(DWORD), NULL))
    {
        if (fConfig & CONFIGFLAG_DISABLED)
        {
            fConfig &= ~CONFIGFLAG_DISABLED;
            if (!SetupDiSetDeviceRegistryPropertyW(hDevInfo, &deviceInfoData, SPDRP_CONFIGFLAGS,
                                                   (LPBYTE)&fConfig, sizeof(fConfig)))
                ErrorMsg("SetupDiSetDeviceRegistryPropertyW");
        }
    }
    else
        ErrorMsg("SetupDiGetDeviceRegistryPropertyW");

    /*
     * Open the service key.
     */
    WCHAR wszSvcRegKey[LINE_LEN + 64];
    rc = RTUtf16CopyAscii(wszSvcRegKey, RT_ELEMENTS(wszSvcRegKey), "System\\CurrentControlSet\\Services\\");
    if (RT_SUCCESS(rc))
        rc = RTUtf16Cat(wszSvcRegKey, RT_ELEMENTS(wszSvcRegKey), wszServiceData);
    if (RT_SUCCESS(rc))
        rc = RTUtf16CatAscii(wszSvcRegKey, RT_ELEMENTS(wszSvcRegKey), "\\Device0"); /* We only have one device. */
    if (RT_FAILURE(rc))
        return ErrorMsg("Service key name too long");

    DWORD   dwIgn;
    HKEY    hKey = NULL;
    LSTATUS lrc  = RegCreateKeyExW(HKEY_LOCAL_MACHINE, wszSvcRegKey, 0, NULL, REG_OPTION_NON_VOLATILE,
                                   KEY_READ | KEY_WRITE, NULL, &hKey, &dwIgn);
    if (lrc == ERROR_SUCCESS)
    {
        /*
         * Insert service description.
         */
        lrc = RegSetValueExW(hKey, L"Device Description", 0, REG_SZ, (LPBYTE)DriverInfoDetailData.s.DrvDescription,
                            (DWORD)((RTUtf16Len(DriverInfoDetailData.s.DrvDescription) + 1) * sizeof(WCHAR)));
        if (lrc != ERROR_SUCCESS)
            ErrorMsgLStatus("RegSetValueExW", lrc);

        /*
         * Execute the SoftwareSettings section of the INF-file or something like that.
         */
        BOOL fOkay = FALSE;
        WCHAR wszSoftwareSection[LINE_LEN + 32];
        rc = RTUtf16Copy(wszSoftwareSection, RT_ELEMENTS(wszSoftwareSection), wszServiceData);
        if (RT_SUCCESS(rc))
            rc = RTUtf16CatAscii(wszSoftwareSection, RT_ELEMENTS(wszSoftwareSection), ".SoftwareSettings");
        if (RT_SUCCESS(rc))
        {
            if (SetupInstallFromInfSectionW(NULL, hInf, wszSoftwareSection, SPINST_REGISTRY, hKey,
                                            NULL, 0, NULL, NULL, NULL, NULL))
                fOkay = TRUE;
            else
                ErrorMsgLastErr("SetupInstallFromInfSectionW");
        }
        else
            ErrorMsg("Software settings section name too long");
        RegCloseKey(hKey);
        if (!fOkay)
            return RTEXITCODE_FAILURE;
    }
    else
        ErrorMsgLStatus("RegCreateKeyExW/Service", lrc);

    /*
     * Install OpenGL stuff.
     */
    lrc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\OpenGLDrivers", 0, NULL,
                          REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, &dwIgn);
    if (lrc == ERROR_SUCCESS)
    {
        /* Do installation here if ever necessary. Currently there is no OpenGL stuff */
        RegCloseKey(hKey);
    }
    else
        ErrorMsgLStatus("RegCreateKeyExW/OpenGLDrivers", lrc);

#if 0
    /* If this key is inserted into the registry, windows will show the desktop
       applet on next boot. We decide in the installer if we want that so the code
       is disabled here. */
    lrc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers\\NewDisplay",
                          0, NULL, REG_OPTION_NON_VOLATILE,
                          KEY_READ | KEY_WRITE, NULL, &hHey, &dwIgn)
    if (lrc == ERROR_SUCCESS)
        RegCloseKey(hHey);
    else
        ErrorMsgLStatus("RegCreateKeyExW/NewDisplay", lrc);
#endif

    /*
     * We must reboot at some point
     */
    lrc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers\\RebootNecessary", 0, NULL,
                          REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, &dwIgn);
    if (lrc == ERROR_SUCCESS)
        RegCloseKey(hKey);
    else
        ErrorMsgLStatus("RegCreateKeyExW/RebootNecessary", lrc);

    return RTEXITCODE_SUCCESS;
}



/**
 * Install the VBox video driver.
 *
 * @param   pwszDriverDir     The base directory where we find the INF.
 */
static RTEXITCODE InstallVideoDriver(WCHAR const * const pwszDriverDir)
{
    /*
     * Create an empty list
     */
    HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList((LPGUID)&GUID_DEVCLASS_DISPLAY, NULL);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return ErrorMsgLastErr("SetupDiCreateDeviceInfoList");

    /*
     * Get the default install parameters.
     */
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    SP_DEVINSTALL_PARAMS_W DeviceInstallParams = { sizeof(SP_DEVINSTALL_PARAMS) };
    if (SetupDiGetDeviceInstallParamsW(hDevInfo, NULL, &DeviceInstallParams))
    {
        /*
         * Insert our install parameters and update hDevInfo with them.
         */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        DeviceInstallParams.Flags |= DI_NOFILECOPY /* We did our own file copying */
                                   | DI_DONOTCALLCONFIGMG
                                   | DI_ENUMSINGLEINF; /* .DriverPath specifies an inf file */
        int rc = RTUtf16Copy(DeviceInstallParams.DriverPath, RT_ELEMENTS(DeviceInstallParams.DriverPath), pwszDriverDir);
        if (RT_SUCCESS(rc))
            rc = RTUtf16CatAscii(DeviceInstallParams.DriverPath, RT_ELEMENTS(DeviceInstallParams.DriverPath),
                                 VBOXGUEST_VIDEO_INF_NAME);
        if (RT_SUCCESS(rc))
        {
            if (SetupDiSetDeviceInstallParamsW(hDevInfo, NULL, &DeviceInstallParams))
            {
                /*
                 * Read the drivers from the INF-file.
                 */
                if (SetupDiBuildDriverInfoList(hDevInfo, NULL, SPDIT_CLASSDRIVER))
                {
                    HINF hInf = NULL;
                    rcExit = InstallVideoDriverInner(pwszDriverDir, hDevInfo, &hInf);

                    if (hInf)
                        SetupCloseInfFile(hInf);
                    SetupDiDestroyDriverInfoList(hDevInfo, NULL, SPDIT_CLASSDRIVER);
                }
                else
                    ErrorMsgLastErr("SetupDiBuildDriverInfoList");
            }
            else
                ErrorMsgLastErr("SetupDiSetDeviceInstallParamsW");

        }
        else
            ErrorMsg("Install dir too deep (long)");
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    else
        ErrorMsgLastErr("SetupDiGetDeviceInstallParams"); /** @todo Original code didn't return here. */
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return rcExit;
}


/**
 * Video driver uninstallation will be added later if necessary.
 */
static RTEXITCODE UninstallDrivers(void)
{
    return ErrorMsg("Uninstall not implemented");
}


static RTEXITCODE displayHelpAndExit(char *pszProgName)
{
    HANDLE const hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD        cbIgn;
    WriteFile(hStdOut, RT_STR_TUPLE("Installs VirtualBox Guest Additions Graphics Drivers for Windows NT 4.0\r\n"
                                    "\r\n"
                                    "Syntax: "), &cbIgn, NULL);
    WriteFile(hStdOut, pszProgName, (DWORD)strlen(pszProgName), &cbIgn, NULL);
    WriteFile(hStdOut, RT_STR_TUPLE("</i|/u>\r\n"
                                    "\r\n"
                                    "Options:\r\n"
                                    "    /i - Install draphics drivers\r\n"
                                    "    /u - Uninstall draphics drivers (not implemented)\r\n"
                                    ), &cbIgn, NULL);
    return RTEXITCODE_SYNTAX;
}


static bool IsNt4(void)
{
    OSVERSIONINFOW VerInfo = { sizeof(VerInfo), 0 };
    GetVersionExW(&VerInfo);
    return VerInfo.dwPlatformId  == VER_PLATFORM_WIN32_NT
        && VerInfo.dwMajorVersion == 4;
}


int main(int argc, char **argv)
{
    /*
     * "Parse" arguments
     */
    if (argc != 2)
    {
        if (argc > 2)
            ErrorMsg("Too many parameter. Expected only one.");
        return displayHelpAndExit(argv[0]);
    }

    bool        fInstall = true;
    const char *pszArg   = argv[1];
    if (RTStrICmpAscii(pszArg, "/i") == 0)
        fInstall = true;
    else if (RTStrICmpAscii(pszArg, "/u") == 0)
        fInstall = false;
    else
    {
        ErrorMsg("Unknown parameter (only known parameters are '/i' and '/u')");
        ErrorMsg(pszArg);
        return displayHelpAndExit(argv[0]);
    }

    /*
     * This program is only for installing drivers on NT4.
     */
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    if (IsNt4())
    {
        /*
         * Derive the installation directory from the executable location.
         * Just strip the filename and use the path.
         */
        WCHAR wszInstallDir[MAX_PATH];
        DWORD cwcInstallDir = GetModuleFileNameW(GetModuleHandle(NULL), &wszInstallDir[0], RT_ELEMENTS(wszInstallDir));
        if (cwcInstallDir > 0)
        {
            while (cwcInstallDir > 0 && RTPATH_IS_SEP(wszInstallDir[cwcInstallDir - 1]))
                cwcInstallDir--;
            if (!cwcInstallDir) /* paranoia^3 */
            {
                wszInstallDir[cwcInstallDir++] = '.';
                wszInstallDir[cwcInstallDir++] = '\\';
            }
            wszInstallDir[cwcInstallDir] = '\0';

            /*
             * Do the install/uninstall.
             */
            if (fInstall)
                rcExit = InstallVideoDriver(wszInstallDir);
            else
                rcExit = UninstallDrivers();

            /*
             * Summary message.
             */
            if (rcExit != RTEXITCODE_SUCCESS)
                ErrorMsg("Some failure occurred during driver installation");
        }
        else
            ErrorMsgLastErr("GetModuleFileNameW failed!");
    }
    else
        ErrorMsg("This program only runs on NT4");
    return rcExit;
}

