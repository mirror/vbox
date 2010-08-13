// 45678901234567890123456789012345678901234567890123456789012345678901234567890
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1999.
//
//  File:       COINST.C
//
//  Contents:   co-installer hook.
//
//  Notes:      For a complete description of CoInstallers, please see the
//                 Microsoft Windows 2000 DDK Documentation
//
//  Author:     keithga   4 June 1999
//
// Revision History:
//              Added FriendlyName interface (Eliyas Yakub Aug 2, 1999)
//
//----------------------------------------------------------------------------

#undef UNICODE
#if !defined(_UNICODE) && defined(UNICODE)
#define _UNICODE
#endif

#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <tchar.h>

// #define LOG_ENABLED

#ifdef LOG_ENABLED
#include <stdio.h>

static VOID _dprintf(LPSTR String, ...)
{
    va_list va;

    va_start(va, String);

    CHAR Buffer[1024];
    if (strlen(String) < 1000)
    {
        _vsntprintf (Buffer, sizeof(Buffer), String, va);

        FILE *f = fopen ("\\coinst.log", "ab");
        if (f)
        {
            fprintf (f, "%s", Buffer);
            fclose (f);
        }
    }

    va_end (va);
}
#define dprintf(a) _dprintf a
#else
#define dprintf(a) do {} while (0)
#endif /* LOG_ENABLED */

//+---------------------------------------------------------------------------
//
// WARNING!
//
// A Coinstaller must not generate any popup to the user.
//     it should provide appropriate defaults.
//
//  OutputDebugString should be fine...
//
#if DBG
#define DbgOut(Text) OutputDebugString(TEXT("CoInstaller: " Text "\n"))
#else
#define DbgOut(Text)
#endif

/** strip the filename and leave the slash. */
void StripFilename (TCHAR *psz)
{
    TCHAR *pchSep = NULL;
    TCHAR *pch = psz;

    /* strip of the filename. */
    for (; *pch; pch++)
    {
        if (*pch == '\\' || *pch == '/' || *pch == ':')
            pchSep = pch + 1;
    }
    if (pchSep)
        *pchSep = '\0';
    else
    {
        psz[0] = '.';
        psz[1] = '\0';
    }
}

/**
 Check the installation type.
 This function checks if the currently used
 INF file not OEMxx.inf. If yes, the user installs the guest additions by hand,
 if no this is an automated installation.
 */
BOOL CheckForNormalInstall (TCHAR *psz)
{
    TCHAR *pchSep = NULL;
    TCHAR *pch = psz;

    /* strip of the filename. */
    for (; *pch; pch++)
    {
        if (*pch == '\\' || *pch == '/' || *pch == ':')
            pchSep = pch + 1;
    }
    if (pchSep)
    {
        if (*pchSep == 'o' || *pchSep == 'O')
            return FALSE;
        return TRUE;
    }
    return TRUE; /* We shouldn't end here... */
}

ULONG startMouseInstallation (TCHAR *pszVBoxGuestInfName)
{
    TCHAR szAppCmd[MAX_PATH];
    STARTUPINFO sInfo = { 0 };
    PROCESS_INFORMATION pInfo = { 0 };
    BOOL fNotAutomated;

    dprintf(("startMouseInstallation: filename = %s\n", pszVBoxGuestInfName));

    /* Check if we do an automated install */
    fNotAutomated = CheckForNormalInstall(pszVBoxGuestInfName);

    StripFilename(pszVBoxGuestInfName);

    dprintf(("startMouseInstallation: fNotAutomated = %d, filename = %s\n", fNotAutomated, pszVBoxGuestInfName));

    if (fNotAutomated)
    {
        /* This is a normal guest installation done by inserting the ISO */
        _sntprintf(szAppCmd, sizeof(szAppCmd), TEXT("rundll32.exe SETUPAPI.DLL,InstallHinfSection VBoxMouse 132 %sVBoxMouse.inf"), pszVBoxGuestInfName);

        sInfo.cb = sizeof(STARTUPINFO);

        if (CreateProcess(NULL, szAppCmd, NULL, //lpProcessAttributes
                          NULL, //lpThreadAttributes
                          FALSE, //bInheritHandles
                          0, //dwCreationFlags
                          NULL, //lpEnvironment
                          NULL, //lpCurrentDirectory,
                          &sInfo, //lpStartupInfo,
                          &pInfo)) //lpProcessInformation
        {
            DWORD dwExitCode = 0;

            /* Wait for rundll32 to finish and then check the exit code; only then do we know if it succeeded or not! */
            WaitForSingleObject(pInfo.hProcess, INFINITE);
            if (GetExitCodeProcess(pInfo.hProcess, &dwExitCode) != 0 && dwExitCode == 0)
            {
                //
                // hook the filter into the mouse class
                //

                dprintf(("startMouseInstallation: hooking\n"));

                // first determine the GUID of the Mouse class
                GUID guid;
                DWORD numGuids;
                if (SetupDiClassGuidsFromNameEx("Mouse", &guid, 1, &numGuids, NULL, NULL) || (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
                {
                    // get the corresponding class registry key
                    HKEY hkey = SetupDiOpenClassRegKeyEx(&guid, KEY_READ | KEY_WRITE, DIOCR_INSTALLER, NULL, NULL);
                    if (hkey)
                    {
                        // hardcoded value, ours + the standard filter
                        RegSetValueEx(hkey, "UpperFilters", 0, REG_MULTI_SZ, (const BYTE*)"VBoxMouse\0mouclass\0\0", 20);
                        RegCloseKey(hkey);
                    }
                }
                CloseHandle(pInfo.hProcess);
                CloseHandle(pInfo.hThread);
                dprintf(("startMouseInstallation: return ok\n"));
                return 0;
            }
            CloseHandle(pInfo.hProcess);
            CloseHandle(pInfo.hThread);
        } /* CreateProcess() */
    } /* fNotAutomated */
    else
    {
        /* This is an automated installation */

        //
        // hook the filter into the mouse class
        //

        dprintf(("startMouseInstallation: automated\n"));

        // first determine the GUID of the Mouse class
        GUID guid;
        DWORD numGuids;
        if (SetupDiClassGuidsFromNameEx("Mouse", &guid, 1, &numGuids, NULL, NULL) || (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            // get the corresponding class registry key
            HKEY hkey = SetupDiOpenClassRegKeyEx(&guid, KEY_READ | KEY_WRITE, DIOCR_INSTALLER, NULL, NULL);
            if (hkey)
            {
                // hardcoded value, ours + the standard filter
                RegSetValueEx(hkey, "UpperFilters", 0, REG_MULTI_SZ, (const BYTE*)"VBoxMouse\0mouclass\0\0", 20);
                RegCloseKey(hkey);
            }
        }
        return 0;
    }

    /* bitch to debug output */
    return -1;
}

//+---------------------------------------------------------------------------
//
//  Function:   VBoxCoInstaller
//
//  Purpose:    Responds to co-installer messages
//
//  Arguments:
//      InstallFunction   [in]
//      DeviceInfoSet     [in]
//      DeviceInfoData    [in]
//      Context           [inout]
//
//  Returns:    NO_ERROR, ERROR_DI_POSTPROCESSING_REQUIRED, or an error code.
//
//  This co-installer is used to install the mouse filter after installation of
//  the VBoxGuest driver
extern "C"
HRESULT WINAPI
VBoxCoInstaller (
        IN DI_FUNCTION InstallFunction,
        IN HDEVINFO DeviceInfoSet,
        IN PSP_DEVINFO_DATA DeviceInfoData, OPTIONAL
        IN OUT PCOINSTALLER_CONTEXT_DATA Context
)
{
    dprintf((__DATE__ __TIME__ "VBoxCoInstaller InstallFunction 0x%02X\n", InstallFunction));

    switch (InstallFunction)
    {
        case DIF_INSTALLDEVICE:
        {
            SP_DEVINSTALL_PARAMS DeviceInstallParams;
            DeviceInstallParams.cbSize = sizeof (DeviceInstallParams);

            if (!SetupDiGetDeviceInstallParams (DeviceInfoSet,
                            DeviceInfoData,
                            &DeviceInstallParams))
            {
                dprintf(("Failed to get DeviceInstallParams\n"));
                return NO_ERROR;
            }

            /* Reboot the system and do not dynamically load the driver. */
            DeviceInstallParams.Flags |= DI_NEEDREBOOT | DI_DONOTCALLCONFIGMG;

            if (!SetupDiSetDeviceInstallParams (DeviceInfoSet,
                            DeviceInfoData,
                            &DeviceInstallParams))
            {
                dprintf(("Failed to set DeviceInstallParams\n"));
                return NO_ERROR;
            }

            return NO_ERROR;
        }

        //        case DIF_REMOVE:
        //        {
        //            return NO_ERROR;
        //        } break;
#if 0
        /* This request is only sent when the installation of the VBoxGuest
         succeeded. Because VBoxMouse needs VBoxGuest to work this is
         a nice installation prerequisite check, too. */
        case DIF_NEWDEVICEWIZARD_FINISHINSTALL:
        {
            SP_DRVINFO_DATA DriverInfoData;
            SP_DRVINFO_DETAIL_DATA DriverInfoDetailData;
            HRESULT Status;

            /* Get the path to the VBoxGuest INF. The VBoxMouse INF is in the same
             directory. */
            DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);
            if (!SetupDiGetSelectedDriver(DeviceInfoSet,
                            DeviceInfoData,
                            &DriverInfoData))
            {
                return NO_ERROR; /* Should return an error here? */
            }

            DriverInfoDetailData.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
            if (!SetupDiGetDriverInfoDetail(DeviceInfoSet,
                            DeviceInfoData,
                            &DriverInfoData,
                            &DriverInfoDetailData,
                            sizeof(SP_DRVINFO_DETAIL_DATA),
                            NULL))
            {
                if ((Status = GetLastError()) == ERROR_INSUFFICIENT_BUFFER)
                {
                    // We don't need the extended information.  Ignore.
                }
                else
                {
                    return NO_ERROR; /* Should return an error here? */
                }
            }

            startMouseInstallation(DriverInfoDetailData.InfFileName);
            break;
        }
#endif

        default:
        break;
    }

    return NO_ERROR;
}

