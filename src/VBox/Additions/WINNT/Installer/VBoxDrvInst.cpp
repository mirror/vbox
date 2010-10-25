/*++

 Copyright (c) Microsoft Corporation.  All rights reserved.

 Module Name:

 VBoxDrvInst.cpp

 Abstract:

 Command-line interface for installing / uninstalling device drivers
 with a given Hardware-ID (and .INF-file).

 --*/

#ifndef UNICODE
#define UNICODE
#endif

#include <VBox/version.h>

#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <regstr.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <stdio.h>
#include <tchar.h>
#include <string.h>

/*#define _DEBUG*/

typedef int (*fnCallback) (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Index, LPVOID Context);

struct IdEntry
{
    LPCTSTR szString; /* string looking for */
    LPCTSTR szWild; /* first wild character if any */
    BOOL bInstanceId;
};

#define INSTANCEID_PREFIX_CHAR TEXT('@')    /* character used to prefix instance ID's */
#define CLASS_PREFIX_CHAR      TEXT('=')    /* character used to prefix class name */
#define WILD_CHAR              TEXT('*')    /* wild character */
#define QUOTE_PREFIX_CHAR      TEXT('\'')   /* prefix character to ignore wild characters */
#define SPLIT_COMMAND_SEP      TEXT(":=")   /* whole word, indicates end of id's */

/* @todo Split this program into several modules:

 - Main
 - Utility functions
 - Dynamic API loading
 - Installation / uninstallation routines
 - ...
 */

/* Exit codes */
#define EXIT_OK      (0)
#define EXIT_REBOOT  (1)
#define EXIT_FAIL    (2)
#define EXIT_USAGE   (3)
#ifdef VBOX_WITH_WDDM
#define EXIT_FALSE   (4)
#endif

/* Dynamic loaded libs */
HMODULE g_hSetupAPI = NULL;
HMODULE g_hNewDev = NULL;
HMODULE g_hCfgMgr = NULL;

/* Function pointers for dynamic loading of some API calls NT4 hasn't ... */
typedef BOOL (WINAPI *fnSetupDiCreateDeviceInfo) (HDEVINFO DeviceInfoSet, PCTSTR DeviceName, LPGUID ClassGuid, PCTSTR DeviceDescription, HWND hwndParent, DWORD CreationFlags,
                                                  PSP_DEVINFO_DATA DeviceInfoData);
fnSetupDiCreateDeviceInfo g_pfnSetupDiCreateDeviceInfo = NULL;

typedef BOOL (WINAPI *fnSetupDiOpenDeviceInfo) (HDEVINFO DeviceInfoSet, PCTSTR DeviceInstanceId, HWND hwndParent, DWORD OpenFlags, PSP_DEVINFO_DATA DeviceInfoData);
fnSetupDiOpenDeviceInfo g_pfnSetupDiOpenDeviceInfo = NULL;

typedef BOOL (WINAPI *fnSetupDiEnumDeviceInfo) (HDEVINFO DeviceInfoSet, DWORD MemberIndex, PSP_DEVINFO_DATA DeviceInfoData);
fnSetupDiEnumDeviceInfo g_pfnSetupDiEnumDeviceInfo = NULL;

/***/

typedef HDEVINFO (WINAPI *fnSetupDiCreateDeviceInfoList) (LPGUID ClassGuid, HWND hwndParent);
fnSetupDiCreateDeviceInfoList g_pfnSetupDiCreateDeviceInfoList = NULL;

typedef HDEVINFO (WINAPI *fnSetupDiCreateDeviceInfoListEx) (LPGUID ClassGuid, HWND hwndParent, PCTSTR MachineName, PVOID Reserved);
fnSetupDiCreateDeviceInfoListEx g_pfnSetupDiCreateDeviceInfoListEx = NULL;

typedef BOOL (WINAPI *fnSetupDiDestroyDriverInfoList) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD DriverType);
fnSetupDiDestroyDriverInfoList g_pfnSetupDiDestroyDriverInfoList = NULL;

typedef BOOL (WINAPI *fnSetupDiDestroyDeviceInfoList) (HDEVINFO DeviceInfoSet);
fnSetupDiDestroyDeviceInfoList g_pfnSetupDiDestroyDeviceInfoList = NULL;

typedef BOOL (WINAPI *fnSetupDiGetDeviceInfoListDetail) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_LIST_DETAIL_DATA DeviceInfoSetDetailData);
fnSetupDiGetDeviceInfoListDetail g_pfnSetupDiGetDeviceInfoListDetail = NULL;

/***/

typedef BOOL (WINAPI *fnSetupDiSetDeviceRegistryProperty) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Property, CONST BYTE *PropertyBuffer, DWORD PropertyBufferSize);
fnSetupDiSetDeviceRegistryProperty g_pfnSetupDiSetDeviceRegistryProperty = NULL;

typedef BOOL (WINAPI *fnSetupDiGetDeviceRegistryProperty) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Property, PDWORD PropertyRegDataType, PBYTE PropertyBuffer,
                                                           DWORD PropertyBufferSize, PDWORD RequiredSize);
fnSetupDiGetDeviceRegistryProperty g_pfnSetupDiGetDeviceRegistryProperty = NULL;

/***/

typedef BOOL (WINAPI *fnSetupDiCallClassInstaller) (DI_FUNCTION InstallFunction, HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData);
fnSetupDiCallClassInstaller g_pfnSetupDiCallClassInstaller = NULL;

typedef BOOL (WINAPI *fnSetupDiClassGuidsFromNameEx) (PCTSTR ClassName, LPGUID ClassGuidList, DWORD ClassGuidListSize, PDWORD RequiredSize, PCTSTR MachineName, PVOID Reserved);
fnSetupDiClassGuidsFromNameEx g_pfnSetupDiClassGuidsFromNameEx = NULL;

typedef HDEVINFO (WINAPI *fnSetupDiGetClassDevsEx) (LPGUID ClassGuid, PCTSTR Enumerator, HWND hwndParent, DWORD Flags, HDEVINFO DeviceInfoSet, PCTSTR MachineName, PVOID Reserved);
fnSetupDiGetClassDevsEx g_pfnSetupDiGetClassDevsEx = NULL;

typedef BOOL (WINAPI *fnSetupDiSetClassInstallParams) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, PSP_CLASSINSTALL_HEADER ClassInstallParams, DWORD ClassInstallParamsSize);
fnSetupDiSetClassInstallParams g_pfnSetupDiSetClassInstallParams = NULL;

typedef BOOL (WINAPI *fnSetupDiGetDeviceInstallParams) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, PSP_DEVINSTALL_PARAMS DeviceInstallParams);
fnSetupDiGetDeviceInstallParams g_pfnSetupDiGetDeviceInstallParams = NULL;

typedef HKEY (WINAPI *fnSetupDiOpenDevRegKey) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Scope, DWORD HwProfile, DWORD KeyType, REGSAM samDesired);
fnSetupDiOpenDevRegKey g_pfnSetupDiOpenDevRegKey = NULL;

typedef BOOL (WINAPI *fnSetupDiBuildDriverInfoList) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD DriverType);
fnSetupDiBuildDriverInfoList g_pfnSetupDiBuildDriverInfoList = NULL;

typedef BOOL (WINAPI *fnSetupDiEnumDriverInfo) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD DriverType, DWORD MemberIndex, PSP_DRVINFO_DATA DriverInfoData);
fnSetupDiEnumDriverInfo g_pfnSetupDiEnumDriverInfo = NULL;

typedef BOOL (WINAPI *fnSetupDiGetDriverInfoDetail) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, PSP_DRVINFO_DATA DriverInfoData, PSP_DRVINFO_DETAIL_DATA DriverInfoDetailData,
                                                     DWORD DriverInfoDetailDataSize, PDWORD RequiredSize);
fnSetupDiGetDriverInfoDetail g_pfnSetupDiGetDriverInfoDetail = NULL;

typedef BOOL (WINAPI *fnSetupDiSetSelectedDriver) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, PSP_DRVINFO_DATA DriverInfoData);
fnSetupDiSetSelectedDriver g_pfnSetupDiSetSelectedDriver = NULL;

typedef BOOL (WINAPI *fnSetupDiSetDeviceInstallParams) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, PSP_DEVINSTALL_PARAMS DeviceInstallParams);
fnSetupDiSetDeviceInstallParams g_pfnSetupDiSetDeviceInstallParams = NULL;

typedef CONFIGRET (WINAPI *fnCM_Get_Device_ID_Ex) (DEVINST dnDevInst, PTCHAR Buffer, ULONG BufferLen, ULONG ulFlags, HMACHINE hMachine);
fnCM_Get_Device_ID_Ex g_pfnCM_Get_Device_ID_Ex = NULL;

typedef BOOL (WINAPI* fnSetupCopyOEMInf) (PCTSTR SourceInfFileName, PCTSTR OEMSourceMediaLocation, DWORD OEMSourceMediaType, DWORD CopyStyle, PTSTR DestinationInfFileName,
                                          DWORD DestinationInfFileNameSize, PDWORD RequiredSize, PTSTR DestinationInfFileNameComponent);
fnSetupCopyOEMInf g_pfnSetupCopyOEMInf = NULL;

typedef BOOL (WINAPI* fnUpdateDriverForPlugAndPlayDevices) (HWND hwndParent, LPCTSTR HardwareId, LPCTSTR FullInfPath, DWORD InstallFlags, PBOOL bRebootRequired);
fnUpdateDriverForPlugAndPlayDevices g_pfnUpdateDriverForPlugAndPlayDevices = NULL;

#define VBOX_LOAD_API( a_hModule, a_Func )                                 \
{                                                                          \
    g_pfn##a_Func = (fn##a_Func)GetProcAddress(a_hModule, "##a_Func");     \
    _tprintf (_T("API call ##a_Func loaded: %p\n"), g_pfn##a_Func);  \
}

int LoadAPICalls ()
{
    int rc = ERROR_SUCCESS;
    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof(OSinfo);
    GetVersionEx(&OSinfo);

#ifdef _DEBUG
    _tprintf (_T("Loading API calls ...\n"));
#endif

    /* Use unicode calls where available. */
    if (OSinfo.dwMajorVersion >= 5) /* APIs available only on W2K and up! */
    {
        g_hSetupAPI = LoadLibrary(_T("SetupAPI"));
        if (NULL == g_hSetupAPI)
        {
            _tprintf(_T("ERROR: SetupAPI.dll not found! Return code: %d\n"), GetLastError());
            rc = ERROR_NOT_INSTALLED;
        }
        else
        {
            g_pfnSetupDiCreateDeviceInfoList = (fnSetupDiCreateDeviceInfoList)GetProcAddress(g_hSetupAPI, "SetupDiCreateDeviceInfoList");
            g_pfnSetupDiCreateDeviceInfo = (fnSetupDiCreateDeviceInfo)GetProcAddress(g_hSetupAPI, "SetupDiCreateDeviceInfoW");
            g_pfnSetupDiSetDeviceRegistryProperty = (fnSetupDiSetDeviceRegistryProperty)GetProcAddress(g_hSetupAPI, "SetupDiSetDeviceRegistryPropertyW");
            g_pfnSetupDiCallClassInstaller = (fnSetupDiCallClassInstaller)GetProcAddress(g_hSetupAPI, "SetupDiCallClassInstaller");
            g_pfnSetupDiDestroyDeviceInfoList = (fnSetupDiDestroyDeviceInfoList)GetProcAddress(g_hSetupAPI, "SetupDiDestroyDeviceInfoList");
            g_pfnSetupDiClassGuidsFromNameEx = (fnSetupDiClassGuidsFromNameEx)GetProcAddress(g_hSetupAPI, "SetupDiClassGuidsFromNameExW");
            g_pfnSetupDiGetDeviceRegistryProperty = (fnSetupDiGetDeviceRegistryProperty)GetProcAddress(g_hSetupAPI, "SetupDiGetDeviceRegistryPropertyW");
            g_pfnSetupDiGetClassDevsEx = (fnSetupDiGetClassDevsEx)GetProcAddress(g_hSetupAPI, "SetupDiGetClassDevsExW");
            g_pfnSetupDiCreateDeviceInfoListEx = (fnSetupDiCreateDeviceInfoListEx)GetProcAddress(g_hSetupAPI, "SetupDiCreateDeviceInfoListExW");
            g_pfnSetupDiOpenDeviceInfo = (fnSetupDiOpenDeviceInfo)GetProcAddress(g_hSetupAPI, "SetupDiOpenDeviceInfoW");
            g_pfnSetupDiGetDeviceInfoListDetail = (fnSetupDiGetDeviceInfoListDetail)GetProcAddress(g_hSetupAPI, "SetupDiGetDeviceInfoListDetailW");
            g_pfnSetupDiEnumDeviceInfo = (fnSetupDiEnumDeviceInfo)GetProcAddress(g_hSetupAPI, "SetupDiEnumDeviceInfo");
            g_pfnSetupDiSetClassInstallParams = (fnSetupDiSetClassInstallParams)GetProcAddress(g_hSetupAPI, "SetupDiSetClassInstallParamsW");
            g_pfnSetupDiGetDeviceInstallParams = (fnSetupDiGetDeviceInstallParams)GetProcAddress(g_hSetupAPI, "SetupDiGetDeviceInstallParamsW");
            g_pfnSetupDiOpenDevRegKey = (fnSetupDiOpenDevRegKey)GetProcAddress(g_hSetupAPI, "SetupDiOpenDevRegKey");
            g_pfnSetupDiBuildDriverInfoList = (fnSetupDiBuildDriverInfoList)GetProcAddress(g_hSetupAPI, "SetupDiBuildDriverInfoList");
            g_pfnSetupDiEnumDriverInfo = (fnSetupDiEnumDriverInfo)GetProcAddress(g_hSetupAPI, "SetupDiEnumDriverInfoW");
            g_pfnSetupDiGetDriverInfoDetail = (fnSetupDiGetDriverInfoDetail)GetProcAddress(g_hSetupAPI, "SetupDiGetDriverInfoDetailW");
            g_pfnSetupDiDestroyDriverInfoList = (fnSetupDiDestroyDriverInfoList)GetProcAddress(g_hSetupAPI, "SetupDiDestroyDriverInfoList");
            g_pfnSetupDiSetSelectedDriver = (fnSetupDiSetSelectedDriver)GetProcAddress(g_hSetupAPI, "SetupDiSetSelectedDriverW");
            g_pfnSetupDiSetDeviceInstallParams = (fnSetupDiSetDeviceInstallParams)GetProcAddress(g_hSetupAPI, "SetupDiSetDeviceInstallParamsW");
            g_pfnSetupCopyOEMInf = (fnSetupCopyOEMInf)GetProcAddress(g_hSetupAPI, "SetupCopyOEMInfW");
        }

        if (rc == ERROR_SUCCESS)
        {
            g_hNewDev = LoadLibrary(_T("NewDev"));
            if (NULL != g_hNewDev)
            {
                g_pfnUpdateDriverForPlugAndPlayDevices = (fnUpdateDriverForPlugAndPlayDevices)GetProcAddress(g_hNewDev, "UpdateDriverForPlugAndPlayDevicesW");
            }
            else
            {
                _tprintf(_T("ERROR: NewDev.dll not found! Return code: %d\n"), GetLastError());
                rc = ERROR_FILE_NOT_FOUND;
            }
        }

        if (rc == ERROR_SUCCESS)
        {
            g_hCfgMgr = LoadLibrary(_T("CfgMgr32"));
            if (NULL != g_hCfgMgr)
            {
                g_pfnCM_Get_Device_ID_Ex = (fnCM_Get_Device_ID_Ex)GetProcAddress(g_hCfgMgr, "CM_Get_Device_ID_ExW");
            }
            else
            {
                _tprintf(_T("ERROR: NewDev.dll not found! Return code: %d\n"), GetLastError());
                rc = ERROR_FILE_NOT_FOUND;
            }
        }
    }
    else if (OSinfo.dwMajorVersion <= 4) /* Windows NT 4.0. */
    {
        /* Nothing to do here yet. */
    }
    else /* Other platforms */
    {
        _tprintf(_T("ERROR: Platform not supported yet!\n"));
        rc = ERROR_NOT_SUPPORTED;
    }

    return rc;
}

void FreeAPICalls ()
{
#ifdef _DEBUG
    _tprintf (_T("Freeing API calls ...\n"));
#endif

    if (NULL != g_hSetupAPI)
        FreeLibrary(g_hSetupAPI);

    if (NULL != g_hNewDev)
        FreeLibrary(g_hNewDev);

    if (NULL != g_hCfgMgr)
        FreeLibrary(g_hCfgMgr);
}

bool GetErrorMsg (DWORD a_dwLastError, _TCHAR* a_pszMsg, DWORD a_dwBufSize)
{
    if (::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, a_dwLastError, 0, a_pszMsg, a_dwBufSize, NULL) == 0)
    {
        _stprintf(a_pszMsg, _T("Unknown error!\n"), a_dwLastError);
        return false;
    }
    else
    {
        _TCHAR* p = _tcschr(a_pszMsg, _T('\r'));

        if (p != NULL)
            *p = _T('\0');
    }

    return true;
}

/* @todo Add exception handling instead of crappy goto's! */

int CreateDevice (_TCHAR* a_pszHwID, GUID a_devClass)
{
    int iRet = EXIT_OK;
    HDEVINFO devInfoSet;
    SP_DEVINFO_DATA devInfoData;
    DWORD dwErr = 0;
    _TCHAR szErrMsg[_MAX_PATH + 1] = { 0 };

    _tprintf(_T("Creating device ...\n"));

    devInfoSet = g_pfnSetupDiCreateDeviceInfoList(&a_devClass, NULL);
    if (devInfoSet == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Could not build device info list!\n"));
        return EXIT_FAIL;
    }

    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (FALSE == g_pfnSetupDiCreateDeviceInfo(devInfoSet, a_pszHwID, &a_devClass, NULL, NULL, DICD_GENERATE_ID, &devInfoData))
    {
        dwErr = GetLastError();

        switch (dwErr)
        {

        case ERROR_DEVINST_ALREADY_EXISTS:

            _tprintf(_T("Device already exists.\n"));
            break;

        case ERROR_CLASS_MISMATCH:

            _tprintf(_T("ERROR: Device does not match to class ID!\n"));
            break;

        case ERROR_INVALID_USER_BUFFER:

            _tprintf(_T("ERROR: Invalid user buffer!\n"));
            break;

        case ERROR_INVALID_DEVINST_NAME:

            _tprintf(_T("ERROR: Invalid device instance name!\n"));
            break;

        default:

            GetErrorMsg(dwErr, szErrMsg, sizeof(szErrMsg));
            _tprintf(_T("ERROR (%08x): %ws\n"), dwErr, szErrMsg);
            break;
        }

        iRet = EXIT_FAIL;
        goto InstallCleanup;
    }

    if (FALSE == g_pfnSetupDiSetDeviceRegistryProperty(devInfoSet, &devInfoData, SPDRP_HARDWAREID, (LPBYTE)a_pszHwID, (DWORD)((_tcsclen(a_pszHwID) + 2) * sizeof(_TCHAR))))
    {
        dwErr = GetLastError();
        _tprintf(_T("Could not set device registry info!\n"));
        iRet = EXIT_FAIL;
        goto InstallCleanup;
    }

    if (FALSE == g_pfnSetupDiCallClassInstaller(DIF_REGISTERDEVICE, devInfoSet, &devInfoData))
    {
        dwErr = GetLastError();
        _tprintf(_T("Could not register device!\n"));
        iRet = EXIT_FAIL;
        goto InstallCleanup;
    }

    InstallCleanup: g_pfnSetupDiDestroyDeviceInfoList(devInfoSet);

    return iRet;
}

int InstallDriver (_TCHAR* a_pszInfFile, _TCHAR* a_pszHwID, _TCHAR* a_pszDevClass)
{
    int rc = EXIT_OK; /** @todo Use IPRT values! */

    DWORD dwErr = 0;
    BOOL bReboot = FALSE;

    _TCHAR szInf[_MAX_PATH] = { 0 }; /* Full path + .INF file */
    _TCHAR szInfPath[_MAX_PATH] = { 0 }; /* Full path to .INF file */
    GUID devClassArr[32]; /* The device class GUID  array */
    DWORD dwReqSize = 0; /* Number of GUIDs in array after lookup */

    _tprintf(_T("Installing driver ...\n"));
    _tprintf(_T("HardwareID: %ws\n"), a_pszHwID);
    _tprintf(_T("Device class name: %ws\n"), a_pszDevClass);

    /* Retrieve GUID of device class */
    /* Not used here: g_pfnSetupDiClassNameFromGuidEx( ... ) */
    if (FALSE == g_pfnSetupDiClassGuidsFromNameEx(a_pszDevClass, devClassArr, 32, &dwReqSize, NULL, NULL))
    {
        _tprintf(_T("Could not retrieve device class GUID! Error: %ld\n"), GetLastError());
        rc = EXIT_FAIL;
    }
    else
    {
        /* Do not fail if dwReqSize is 0. For whatever reason Windows Server 2008 Core does not have the "Media"
           device class installed. Maybe they stripped down too much? :-/ */
        if (dwReqSize <= 0)
        {
            _tprintf(_T("WARNING: No device class with this name found! ReqSize: %ld, Error: %ld\n"), dwReqSize, GetLastError());
        }
        else
        {
            _tprintf(_T("Number of GUIDs found: %ld\n"), dwReqSize);
        }

        /* Not needed for now!
        if (EXIT_FAIL == CreateDevice (a_pszHwID, devClassArr[0]))
            return EXIT_FAIL;*/
    }

    if (rc == EXIT_OK)
    {
        _TCHAR* pcFile = NULL;
        if (0 == GetFullPathName(a_pszInfFile, _MAX_PATH, szInf, &pcFile))
        {
            dwErr = GetLastError();

            _tprintf(_T("ERROR: INF-Path too long / could not be retrieved!\n"));
            rc = EXIT_FAIL;
        }

        /* Extract path from path+INF */
        if (pcFile != NULL)
            _tcsnccpy(szInfPath, szInf, pcFile - szInf);

        _tprintf(_T("INF-File: %ws\n"), szInf);
        _tprintf(_T("INF-Path: %ws\n"), szInfPath);

        _tprintf(_T("Updating driver for plug'n play devices ...\n"));
        if (!g_pfnUpdateDriverForPlugAndPlayDevices(NULL, a_pszHwID, szInf, INSTALLFLAG_FORCE, &bReboot))
        {
            DWORD dwErr = GetLastError();
            _TCHAR szErrMsg[_MAX_PATH + 1] = { 0 };

            if (dwErr == ERROR_NO_SUCH_DEVINST)
            {
                _TCHAR szDestInf[_MAX_PATH] = { 0 };
                _tprintf(_T("The device is not plugged in (yet), pre-installing drivers ...\n"));

                if (FALSE == g_pfnSetupCopyOEMInf(szInf, szInfPath, SPOST_PATH, 0, szDestInf, sizeof(szDestInf), NULL, NULL))
                {
                    dwErr = GetLastError();
                    GetErrorMsg(dwErr, szErrMsg, sizeof(szErrMsg));
                    _tprintf(_T("ERROR (%08x): %ws\n"), dwErr, szErrMsg);

                    rc = EXIT_FAIL;
                }
                else
                    _tprintf(_T("OK. Installed to: %ws\n"), szDestInf);
            }
            else
            {
                switch (dwErr)
                {

                case ERROR_INVALID_FLAGS:

                    _tprintf(_T("ERROR: The value specified for InstallFlags is invalid!\n"));
                    break;

                case ERROR_NO_MORE_ITEMS:

                    _tprintf(
                             _T(
                                "ERROR: The function found a match for the HardwareId value, but the specified driver was not a better match than the current driver and the caller did not specify the INSTALLFLAG_FORCE flag!\n"));
                    break;

                case ERROR_FILE_NOT_FOUND:

                    _tprintf(_T("ERROR: File not found! File = %ws\n"), szInf);
                    break;

                case ERROR_IN_WOW64:

                    _tprintf(_T("ERROR: The calling application is a 32-bit application attempting to execute in a 64-bit environment, which is not allowed!"));
                    break;

                case ERROR_NO_DRIVER_SELECTED:

                    _tprintf(_T("ERROR: No driver in .INF-file selected!\n"));
                    break;

                case ERROR_SECTION_NOT_FOUND:

                    _tprintf(_T("ERROR: Section in .INF-file was not found!\n"));
                    break;

                default:

                    /* Try error lookup with GetErrorMsg() */
                    GetErrorMsg(dwErr, szErrMsg, sizeof(szErrMsg));
                    _tprintf(_T("ERROR (%08x): %ws\n"), dwErr, szErrMsg);
                    break;
                }

                rc = EXIT_FAIL;
            }
        }
    }

    if (rc == EXIT_OK)
        _tprintf(_T("Installation successful.\n"));
    return rc;
}

/*++

 Routine Description:

 Determine if this is instance id or hardware id and if there's any wildcards
 instance ID is prefixed by '@'
 wildcards are '*'


 Arguments:

 Id - ptr to string to check

 Return Value:

 IdEntry

 --*/
IdEntry GetIdType (LPCTSTR Id)
{
    IdEntry Entry;

    Entry.bInstanceId = FALSE;
    Entry.szWild = NULL;
    Entry.szString = Id;

    if (Entry.szString[0] == INSTANCEID_PREFIX_CHAR)
    {
        Entry.bInstanceId = TRUE;
        Entry.szString = CharNext(Entry.szString);
    }
    if (Entry.szString[0] == QUOTE_PREFIX_CHAR)
    {
        /* prefix to treat rest of string literally */
        Entry.szString = CharNext(Entry.szString);
    }
    else
    {
        /* see if any wild characters exist */
        Entry.szWild = _tcschr(Entry.szString, WILD_CHAR);
    }
    return Entry;
}

/*++

 Routine Description:

 Get an index array pointing to the MultiSz passed in

 Arguments:

 MultiSz - well formed multi-sz string

 Return Value:

 array of strings. last entry+1 of array contains NULL
 returns NULL on failure

 --*/
LPTSTR * GetMultiSzIndexArray (LPTSTR MultiSz)
{
    LPTSTR scan;
    LPTSTR * array;
    int elements;

    for (scan = MultiSz, elements = 0; scan[0]; elements++)
    {
        scan += lstrlen(scan) + 1;
    }
    array = new LPTSTR[elements + 2];
    if (!array)
    {
        return NULL;
    }
    array[0] = MultiSz;
    array++;
    if (elements)
    {
        for (scan = MultiSz, elements = 0; scan[0]; elements++)
        {
            array[elements] = scan;
            scan += lstrlen(scan) + 1;
        }
    }
    array[elements] = NULL;
    return array;
}

/*++

 Routine Description:

 Deletes the string array allocated by GetDevMultiSz/GetRegMultiSz/GetMultiSzIndexArray

 Arguments:

 Array - pointer returned by GetMultiSzIndexArray

 Return Value:

 None

 --*/
void DelMultiSz (LPTSTR * Array)
{
    if (Array)
    {
        Array--;
        if (Array[0])
        {
            delete[] Array[0];
        }
        delete[] Array;
    }
}

/*++

 Routine Description:

 Get a multi-sz device property
 and return as an array of strings

 Arguments:

 Devs    - HDEVINFO containing DevInfo
 DevInfo - Specific device
 Prop    - SPDRP_HARDWAREID or SPDRP_COMPATIBLEIDS

 Return Value:

 array of strings. last entry+1 of array contains NULL
 returns NULL on failure

 --*/
LPTSTR * GetDevMultiSz (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Prop)
{
    LPTSTR buffer;
    DWORD size;
    DWORD reqSize;
    DWORD dataType;
    LPTSTR * array;
    DWORD szChars;

    size = 8192; /* initial guess, nothing magic about this */
    buffer = new TCHAR[(size / sizeof(TCHAR)) + 2];
    if (!buffer)
    {
        return NULL;
    }
    while (!g_pfnSetupDiGetDeviceRegistryProperty(Devs, DevInfo, Prop, &dataType, (LPBYTE)buffer, size, &reqSize))
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            goto failed;
        }
        if (dataType != REG_MULTI_SZ)
        {
            goto failed;
        }
        size = reqSize;
        delete[] buffer;
        buffer = new TCHAR[(size / sizeof(TCHAR)) + 2];
        if (!buffer)
        {
            goto failed;
        }
    }
    szChars = reqSize / sizeof(TCHAR);
    buffer[szChars] = TEXT('\0');
    buffer[szChars + 1] = TEXT('\0');
    array = GetMultiSzIndexArray(buffer);
    if (array)
    {
        return array;
    }

    failed: if (buffer)
    {
        delete[] buffer;
    }
    return NULL;
}

/*++

 Routine Description:

 Compare a single item against wildcard
 I'm sure there's better ways of implementing this
 Other than a command-line management tools
 it's a bad idea to use wildcards as it implies
 assumptions about the hardware/instance ID
 eg, it might be tempting to enumerate root\* to
 find all root devices, however there is a CfgMgr
 API to query status and determine if a device is
 root enumerated, which doesn't rely on implementation
 details.

 Arguments:

 Item - item to find match for eg a\abcd\c
 MatchEntry - eg *\*bc*\*

 Return Value:

 TRUE if any match, otherwise FALSE

 --*/
BOOL WildCardMatch (LPCTSTR Item, const IdEntry & MatchEntry)
{
    LPCTSTR scanItem;
    LPCTSTR wildMark;
    LPCTSTR nextWild;
    size_t matchlen;

    /* Before attempting anything else,
     try and compare everything up to first wild */
    //
    if (!MatchEntry.szWild)
    {
        return _tcsicmp(Item, MatchEntry.szString) ? FALSE : TRUE;
    }
    if (_tcsnicmp(Item, MatchEntry.szString, MatchEntry.szWild - MatchEntry.szString) != 0)
    {
        return FALSE;
    }
    wildMark = MatchEntry.szWild;
    scanItem = Item + (MatchEntry.szWild - MatchEntry.szString);

    for (; wildMark[0];)
    {
        /* If we get here, we're either at or past a wildcard */
        if (wildMark[0] == WILD_CHAR)
        {
            /* So skip wild chars */
            wildMark = CharNext(wildMark);
            continue;
        }

        /* Find next wild-card */
        nextWild = _tcschr(wildMark, WILD_CHAR);

        if (nextWild)
        {
            /* Substring */
            matchlen = nextWild - wildMark;
        }
        else
        {
            /* Last portion of match */
            size_t scanlen = lstrlen(scanItem);
            matchlen = lstrlen(wildMark);

            if (scanlen < matchlen)
            {
                return FALSE;
            }

            return _tcsicmp(scanItem + scanlen - matchlen, wildMark) ? FALSE : TRUE;
        }

        if (_istalpha(wildMark[0]))
        {
            /* Scan for either lower or uppercase version of first character */
            TCHAR u = _totupper(wildMark[0]);
            TCHAR l = _totlower(wildMark[0]);
            while (scanItem[0] && scanItem[0] != u && scanItem[0] != l)
            {
                scanItem = CharNext(scanItem);
            }

            if (!scanItem[0])
            {
                /* Ran out of string */
                return FALSE;
            }
        }
        else
        {
            /* Scan for first character (no case) */
            scanItem = _tcschr(scanItem, wildMark[0]);
            if (!scanItem)
            {
                /* Ran out of string */
                return FALSE;
            }
        }

        /* Try and match the sub-string at wildMark against scanItem */
        if (_tcsnicmp(scanItem, wildMark, matchlen) != 0)
        {
            /* Nope, try again */
            scanItem = CharNext(scanItem);
            continue;
        }

        /* Substring matched */
        scanItem += matchlen;
        wildMark += matchlen;
    }
    return (wildMark[0] ? FALSE : TRUE);
}

/*++

 Routine Description:

 Compares all strings in Array against Id
 Use WildCardMatch to do real compare

 Arguments:

 Array - pointer returned by GetDevMultiSz
 MatchEntry - string to compare against

 Return Value:

 TRUE if any match, otherwise FALSE

 --*/
BOOL WildCompareHwIds (LPTSTR * Array, const IdEntry & MatchEntry)
{
    if (Array)
    {
        while (Array[0])
        {
            if (WildCardMatch(Array[0], MatchEntry))
            {
                return TRUE;
            }
            Array++;
        }
    }
    return FALSE;
}

/*++

 Routine Description:

 Generic enumerator for devices that will be passed the following arguments:
 <id> [<id>...]
 =<class> [<id>...]
 where <id> can either be @instance-id, or hardware-id and may contain wildcards
 <class> is a class name

 Arguments:

 BaseName - name of executable
 Machine  - name of machine to enumerate
 Flags    - extra enumeration flags (eg DIGCF_PRESENT)
 argc/argv - remaining arguments on command line
 Callback - function to call for each hit
 Context  - data to pass function for each hit

 Return Value:

 EXIT_xxxx

 --*/
int EnumerateDevices (LPCTSTR BaseName, LPCTSTR Machine, DWORD Flags, int argc, LPTSTR argv[], fnCallback Callback, LPVOID Context)
{
    HDEVINFO devs = INVALID_HANDLE_VALUE;
    IdEntry * templ = NULL;
    int failcode = EXIT_FAIL;
    int retcode;
    int argIndex;
    DWORD devIndex;
    SP_DEVINFO_DATA devInfo;
    SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;
    BOOL doSearch = FALSE;
    BOOL match;
    BOOL all = FALSE;
    GUID cls;
    DWORD numClass = 0;
    int skip = 0;

    if (!argc)
    {
        return EXIT_USAGE;
    }

    templ = new IdEntry[argc];
    if (!templ)
    {
        goto final;
    }

    /* Determine if a class is specified */
    if (argc > skip && argv[skip][0] == CLASS_PREFIX_CHAR && argv[skip][1])
    {
        if (!g_pfnSetupDiClassGuidsFromNameEx(argv[skip] + 1, &cls, 1, &numClass, Machine, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            goto final;
        }
        if (!numClass)
        {
            failcode = EXIT_OK;
            goto final;
        }
        skip++;
    }

    if (argc > skip && argv[skip][0] == WILD_CHAR && !argv[skip][1])
    {
        /* Catch convinient case of specifying a single argument '*' */
        all = TRUE;
        skip++;
    }
    else if (argc <= skip)
    {
        /* At least one parameter, but no <id>'s */
        all = TRUE;
    }

    /* Determine if any instance id's were specified */

    /* Note, if =<class> was specified with no id's
     we'll mark it as not doSearch
     but will go ahead and add them all */
    for (argIndex = skip; argIndex < argc; argIndex++)
    {
        templ[argIndex] = GetIdType(argv[argIndex]);
        if (templ[argIndex].szWild || !templ[argIndex].bInstanceId)
        {
            /* Anything other than simple bInstanceId's require a search */
            doSearch = TRUE;
        }
    }
    if (doSearch || all)
    {
        /* Add all id's to list
         If there's a class, filter on specified class */
        devs = g_pfnSetupDiGetClassDevsEx(numClass ? &cls : NULL, NULL, NULL, (numClass ? 0 : DIGCF_ALLCLASSES) | Flags, NULL, Machine, NULL);

    }
    else
    {
        /* Blank list, we'll add instance id's by hand */
        devs = g_pfnSetupDiCreateDeviceInfoListEx(numClass ? &cls : NULL, NULL, Machine, NULL);
    }
    if (devs == INVALID_HANDLE_VALUE)
    {
        goto final;
    }
    for (argIndex = skip; argIndex < argc; argIndex++)
    {
        /* Add explicit instances to list (even if enumerated all,
         this gets around DIGCF_PRESENT)
         do this even if wildcards appear to be detected since they
         might actually be part of the instance ID of a non-present device */
        if (templ[argIndex].bInstanceId)
        {
            g_pfnSetupDiOpenDeviceInfo(devs, templ[argIndex].szString, NULL, 0, NULL);
        }
    }

    devInfoListDetail.cbSize = sizeof(devInfoListDetail);
    if (!g_pfnSetupDiGetDeviceInfoListDetail(devs, &devInfoListDetail))
    {
        goto final;
    }

    /* Now enumerate them */
    if (all)
    {
        doSearch = FALSE;
    }

    devInfo.cbSize = sizeof(devInfo);
    for (devIndex = 0; g_pfnSetupDiEnumDeviceInfo(devs, devIndex, &devInfo); devIndex++)
    {

        if (doSearch)
        {
            for (argIndex = skip, match = FALSE; (argIndex < argc) && !match; argIndex++)
            {
                TCHAR devID[MAX_DEVICE_ID_LEN];
                LPTSTR *hwIds = NULL;
                LPTSTR *compatIds = NULL;

                /* Determine instance ID */
                if (g_pfnCM_Get_Device_ID_Ex(devInfo.DevInst, devID, MAX_DEVICE_ID_LEN, 0, devInfoListDetail.RemoteMachineHandle) != CR_SUCCESS)
                {
                    devID[0] = TEXT('\0');
                }

                if (templ[argIndex].bInstanceId)
                {
                    /* Match on the instance ID */
                    if (WildCardMatch(devID, templ[argIndex]))
                        match = TRUE;

                }
                else
                {
                    /* Determine hardware ID's and search for matches */
                    hwIds = GetDevMultiSz(devs, &devInfo, SPDRP_HARDWAREID);
                    compatIds = GetDevMultiSz(devs, &devInfo, SPDRP_COMPATIBLEIDS);

                    if (WildCompareHwIds(hwIds, templ[argIndex]) || WildCompareHwIds(compatIds, templ[argIndex]))
                    {
                        match = TRUE;
                    }
                }
                DelMultiSz(hwIds);
                DelMultiSz(compatIds);
            }
        }
        else
        {
            match = TRUE;
        }
        if (match)
        {
            retcode = Callback(devs, &devInfo, devIndex, Context);
            if (retcode)
            {
                failcode = retcode;
                goto final;
            }
        }
    }

    failcode = EXIT_OK;

    final: if (templ)
    {
        delete[] templ;
    }
    if (devs != INVALID_HANDLE_VALUE)
    {
        g_pfnSetupDiDestroyDeviceInfoList(devs);
    }
    return failcode;

}

/*++

 Routine Description:

 Callback for use by Remove
 Invokes DIF_REMOVE
 uses g_pfnSetupDiCallClassInstaller so cannot be done for remote devices
 Don't use CM_xxx API's, they bypass class/co-installers and this is bad.

 Arguments:

 Devs    )_ uniquely identify the device
 DevInfo )
 Index    - index of device
 Context  - GenericContext

 Return Value:

 EXIT_xxxx

 --*/
int UninstallCallback (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Index, LPVOID Context)
{
    SP_REMOVEDEVICE_PARAMS rmdParams;
    SP_DEVINSTALL_PARAMS devParams;
    LPCTSTR action = NULL;

    /* Need hardware ID before trying to remove, as we wont have it after */
    TCHAR devID[MAX_DEVICE_ID_LEN];
    SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail;

    devInfoListDetail.cbSize = sizeof(devInfoListDetail);

    if ((!g_pfnSetupDiGetDeviceInfoListDetail(Devs, &devInfoListDetail)) || (g_pfnCM_Get_Device_ID_Ex(DevInfo->DevInst, devID, MAX_DEVICE_ID_LEN, 0, devInfoListDetail.RemoteMachineHandle)
            != CR_SUCCESS))
    {
        /* Skip this */
        return EXIT_OK;
    }

    rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
    rmdParams.HwProfile = 0;

    if (!g_pfnSetupDiSetClassInstallParams(Devs, DevInfo, &rmdParams.ClassInstallHeader, sizeof(rmdParams)) || !g_pfnSetupDiCallClassInstaller(DIF_REMOVE, Devs, DevInfo))
    {
        /* Failed to invoke DIF_REMOVE, TODO! */
        _tprintf(_T("Failed to invoke interface!\n"));
        return EXIT_FAIL;
    }

    /* See if device needs reboot */
    devParams.cbSize = sizeof(devParams);
    if (g_pfnSetupDiGetDeviceInstallParams(Devs, DevInfo, &devParams) && (devParams.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT)))
    {
        /* Reboot required */
        _tprintf(_T("To fully uninstall, a reboot is required!\n"));
    }
    else
    {
        /* Appears to have succeeded */
        _tprintf(_T("Uninstall succeeded!\n"));
    }

    return EXIT_OK;
}

/*++

 Routine Description:

 Find the driver that is associated with the current device
 We can do this either the quick way (available in WinXP)
 or the long way that works in Win2k.

 Arguments:

 Devs    )_ uniquely identify device
 DevInfo )

 Return Value:

 TRUE if we managed to determine and select current driver

 --*/
BOOL FindCurrentDriver (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, PSP_DRVINFO_DATA DriverInfoData)
{
    SP_DEVINSTALL_PARAMS deviceInstallParams;
    WCHAR SectionName[LINE_LEN];
    WCHAR DrvDescription[LINE_LEN];
    WCHAR MfgName[LINE_LEN];
    WCHAR ProviderName[LINE_LEN];
    HKEY hKey = NULL;
    DWORD RegDataLength;
    DWORD RegDataType;
    DWORD c;
    BOOL match = FALSE;
    long regerr;

    ZeroMemory(&deviceInstallParams, sizeof(deviceInstallParams));
    deviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

    if (!g_pfnSetupDiGetDeviceInstallParams(Devs, DevInfo, &deviceInstallParams))
    {
        printf("Could not retrieve install params!");
        return FALSE;
    }

#ifdef DI_FLAGSEX_INSTALLEDDRIVER

    /* Set the flags that tell g_pfnSetupDiBuildDriverInfoList to just put the
     currently installed driver node in the list, and that it should allow
     excluded drivers. This flag introduced in WinXP. */
    deviceInstallParams.FlagsEx |= (DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS);

    if (g_pfnSetupDiSetDeviceInstallParams(Devs, DevInfo, &deviceInstallParams))
    {
        /* We were able to specify this flag, so proceed the easy way
         We should get a list of no more than 1 driver */
        if (!g_pfnSetupDiBuildDriverInfoList(Devs, DevInfo, SPDIT_CLASSDRIVER))
        {
            return FALSE;
        }
        if (!g_pfnSetupDiEnumDriverInfo(Devs, DevInfo, SPDIT_CLASSDRIVER, 0, DriverInfoData))
        {
            return FALSE;
        }

        /* We've selected the current driver */
        return TRUE;
    }

    deviceInstallParams.FlagsEx &= ~(DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS);

#endif

    /* The following method works in Win2k, but it's slow and painful.
     First, get driver key - if it doesn't exist, no driver */
    hKey = g_pfnSetupDiOpenDevRegKey(Devs, DevInfo, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ
    );

    if (hKey == INVALID_HANDLE_VALUE)
    {

        _tprintf(_T("No associated driver found in registry!"));

        /* No such value exists, so there can't be an associated driver */
        RegCloseKey(hKey);
        return FALSE;
    }

    /* Obtain path of INF - we'll do a search on this specific INF */
    RegDataLength = sizeof(deviceInstallParams.DriverPath); /* Bytes!!! */
    regerr = RegQueryValueEx(hKey, REGSTR_VAL_INFPATH, NULL, &RegDataType, (PBYTE)deviceInstallParams.DriverPath, &RegDataLength);

    if ((regerr != ERROR_SUCCESS) || (RegDataType != REG_SZ))
    {

        _tprintf(_T("No associated .inf path found in registry!"));

        /* No such value exists, so no associated driver */
        RegCloseKey(hKey);
        return FALSE;
    }

    /* Obtain name of Provider to fill into DriverInfoData */
    RegDataLength = sizeof(ProviderName); /* Bytes!!! */
    regerr = RegQueryValueEx(hKey, REGSTR_VAL_PROVIDER_NAME, NULL, &RegDataType, (PBYTE)ProviderName, &RegDataLength);

    if ((regerr != ERROR_SUCCESS) || (RegDataType != REG_SZ))
    {
        /* No such value exists, so we don't have a valid associated driver */
        RegCloseKey(hKey);
        return FALSE;
    }

    /* Obtain name of section - for final verification */
    RegDataLength = sizeof(SectionName); /* Bytes!!! */
    regerr = RegQueryValueEx(hKey, REGSTR_VAL_INFSECTION, NULL, &RegDataType, (PBYTE)SectionName, &RegDataLength);

    if ((regerr != ERROR_SUCCESS) || (RegDataType != REG_SZ))
    {
        /* No such value exists, so we don't have a valid associated driver */
        RegCloseKey(hKey);
        return FALSE;
    }

    /* Driver description (need not be same as device description) - for final verification */
    RegDataLength = sizeof(DrvDescription); /* Bytes!!! */
    regerr = RegQueryValueEx(hKey, REGSTR_VAL_DRVDESC, NULL, &RegDataType, (PBYTE)DrvDescription, &RegDataLength);

    RegCloseKey(hKey);

    if ((regerr != ERROR_SUCCESS) || (RegDataType != REG_SZ))
    {
        /* No such value exists, so we don't have a valid associated driver */
        return FALSE;
    }

    /* Manufacturer (via SPDRP_MFG, don't access registry directly!) */
    if (!g_pfnSetupDiGetDeviceRegistryProperty(Devs, DevInfo, SPDRP_MFG, NULL, /* Datatype is guaranteed to always be REG_SZ */
    (PBYTE)MfgName, sizeof(MfgName), /* Bytes!!! */
    NULL))
    {
        /* No such value exists, so we don't have a valid associated driver */
        return FALSE;
    }

    /* Now search for drivers listed in the INF */
    deviceInstallParams.Flags |= DI_ENUMSINGLEINF;
    deviceInstallParams.FlagsEx |= DI_FLAGSEX_ALLOWEXCLUDEDDRVS;

    if (!g_pfnSetupDiSetDeviceInstallParams(Devs, DevInfo, &deviceInstallParams))
    {
        return FALSE;
    }
    if (!g_pfnSetupDiBuildDriverInfoList(Devs, DevInfo, SPDIT_CLASSDRIVER))
    {
        return FALSE;
    }

    /* Find the entry in the INF that was used to install the driver for this device */
    for (c = 0; g_pfnSetupDiEnumDriverInfo(Devs, DevInfo, SPDIT_CLASSDRIVER, c, DriverInfoData); c++)
    {
        if ((_tcscmp(DriverInfoData->MfgName, MfgName) == 0) && (_tcscmp(DriverInfoData->ProviderName, ProviderName) == 0))
        {
            /* These two fields match, try more detailed info to ensure we have the exact driver entry used */
            SP_DRVINFO_DETAIL_DATA detail;
            detail.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
            if (!g_pfnSetupDiGetDriverInfoDetail(Devs, DevInfo, DriverInfoData, &detail, sizeof(detail), NULL) && (GetLastError() != ERROR_INSUFFICIENT_BUFFER))
            {
                continue;
            }
            if ((_tcscmp(detail.SectionName, SectionName) == 0) && (_tcscmp(detail.DrvDescription, DrvDescription) == 0))
            {
                match = TRUE;
                break;
            }
        }
    }
    if (!match)
    {
        g_pfnSetupDiDestroyDriverInfoList(Devs, DevInfo, SPDIT_CLASSDRIVER);
    }
    return match;
}

/*++

 Routine Description:

 if Context provided, Simply count
 otherwise dump files indented 2

 Arguments:

 Context      - DWORD Count
 Notification - SPFILENOTIFY_QUEUESCAN
 Param1       - scan

 Return Value:

 none

 --*/
UINT DumpDeviceDriversCallback (IN PVOID Context, IN UINT Notification, IN UINT_PTR Param1, IN UINT_PTR Param2)
{
    LPDWORD count = (LPDWORD)Context;
    LPTSTR file = (LPTSTR)Param1;
    if (count)
    {
        count[0]++;
    }
    else
    {
        _tprintf(TEXT("%s\n"), file);
    }

    return NO_ERROR;
}

/*++

 Routine Description:

 Dump information about what files were installed for driver package
 <tab>Installed using OEM123.INF section [abc.NT]
 <tab><tab>file...

 Arguments:

 Devs    )_ uniquely identify device
 DevInfo )

 Return Value:

 none

 --*/
int DeleteOEMInfCallback (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Index, LPVOID Context)
{
    /* Do this by 'searching' for the current driver
     mimmicing a copy-only install to our own file queue
     and then parsing that file queue */
    SP_DEVINSTALL_PARAMS deviceInstallParams;
    SP_DRVINFO_DATA driverInfoData;
    SP_DRVINFO_DETAIL_DATA driverInfoDetail;
    HSPFILEQ queueHandle = INVALID_HANDLE_VALUE;
    DWORD count;
    DWORD scanResult;
    int success = EXIT_FAIL;

    ZeroMemory(&driverInfoData,sizeof(driverInfoData));
    driverInfoData.cbSize = sizeof(driverInfoData);

    if (!FindCurrentDriver(Devs, DevInfo, &driverInfoData))
        return EXIT_FAIL;

    _tprintf(_T("Driver files found!\n"));

    /* Get useful driver information */
    driverInfoDetail.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
    if (!g_pfnSetupDiGetDriverInfoDetail(Devs, DevInfo, &driverInfoData, &driverInfoDetail, sizeof(SP_DRVINFO_DETAIL_DATA), NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        /* No information about driver or section */
        _tprintf(_T("No information about driver or section!\n"));
        goto final;
    }

    if (!driverInfoDetail.InfFileName[0] || !driverInfoDetail.SectionName[0])
    {
        _tprintf(_T("Driver or section name is empty!\n"));
        goto final;
    }

    _tprintf(_T("Desc: %s\n"), driverInfoDetail.DrvDescription);
    _tprintf(_T("SecName: %s\n"), driverInfoDetail.SectionName);
    _tprintf(_T("INF-File: %s\n"), driverInfoDetail.InfFileName);

    /* Pretend to do the file-copy part of a driver install
     to determine what files are used
     the specified driver must be selected as the active driver */
    if (!g_pfnSetupDiSetSelectedDriver(Devs, DevInfo, &driverInfoData))
        goto final;

    /* Create a file queue so we can look at this queue later */
    queueHandle = SetupOpenFileQueue();

    if (queueHandle == (HSPFILEQ)INVALID_HANDLE_VALUE)
    {
        goto final;
    }

    /* Modify flags to indicate we're providing our own queue */
    ZeroMemory(&deviceInstallParams, sizeof(deviceInstallParams));
    deviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
    if (!g_pfnSetupDiGetDeviceInstallParams(Devs, DevInfo, &deviceInstallParams))
        goto final;

    /* We want to add the files to the file queue, not install them! */
    deviceInstallParams.FileQueue = queueHandle;
    deviceInstallParams.Flags |= DI_NOVCP;

    if (!g_pfnSetupDiSetDeviceInstallParams(Devs, DevInfo, &deviceInstallParams))
        goto final;

    /* Now fill queue with files that are to be installed this involves all class/co-installers */
    if (!g_pfnSetupDiCallClassInstaller(DIF_INSTALLDEVICEFILES, Devs, DevInfo))
        goto final;

    /* We now have a list of delete/rename/copy files
     iterate the copy queue twice - 1st time to get # of files
     2nd time to get files (WinXP has API to get # of files, but we want this to work
     on Win2k too */
    count = 0;
    scanResult = 0;

    /* Call once to count (NOT YET IMPLEMENTED!) */
    //SetupScanFileQueue(queueHandle,SPQ_SCAN_USE_CALLBACK,NULL,DumpDeviceDriversCallback,&count,&scanResult);
    //FormatToStream(stdout, count ? MSG_DUMP_DRIVER_FILES : MSG_DUMP_NO_DRIVER_FILES, count, driverInfoDetail.InfFileName, driverInfoDetail.SectionName);

    /* Call again to dump the files (NOT YET IMPLEMENTED!) */
    //SetupScanFileQueue(queueHandle,SPQ_SCAN_USE_CALLBACK,NULL,DumpDeviceDriversCallback,NULL,&scanResult);

    if (!DeleteFile(driverInfoDetail.InfFileName))
        scanResult = GetLastError();
    else
    {
        DWORD index = 0;

        index = lstrlen(driverInfoDetail.InfFileName);
        if (index > 3)
        {
            lstrcpy(driverInfoDetail.InfFileName + index - 3, TEXT( "pnf" ) );

            if (!DeleteFile(driverInfoDetail.InfFileName))
                scanResult = GetLastError();
        }
    }

    success = EXIT_OK;

    final:

    g_pfnSetupDiDestroyDriverInfoList(Devs, DevInfo, SPDIT_CLASSDRIVER);

    if (queueHandle != (HSPFILEQ)INVALID_HANDLE_VALUE)
    {
        SetupCloseFileQueue(queueHandle);
    }

    if (EXIT_OK != success)
    {
        _tprintf(_T("Something went wrong while delete the OEM INF-files!\n\n"));
    }

    return success;

}

int UninstallDriver (_TCHAR* a_pszHwID)
{
    _tprintf(_T("Uninstalling device: %ws\n"), a_pszHwID);

    _tprintf(_T("Removing driver files ...\n\n"));
    int iRet = EnumerateDevices(NULL, NULL, DIGCF_PRESENT, 1, &a_pszHwID, DeleteOEMInfCallback, NULL);

    if (EXIT_OK != iRet)
        return iRet;

    _tprintf(_T("Uninstalling driver ...\n\n"));
    iRet = EnumerateDevices(NULL, NULL, DIGCF_PRESENT, 1, &a_pszHwID, UninstallCallback, NULL);

    return iRet;
}

#ifdef VBOX_WITH_WDDM
/*++

 Routine Description:

 Dump information about what files were installed for driver package
 <tab>Installed using OEM123.INF section [abc.NT]
 <tab><tab>file...

 Arguments:

 Devs    )_ uniquely identify device
 DevInfo )

 Return Value:

 none

 --*/
int MatchDriverCallback (HDEVINFO Devs, PSP_DEVINFO_DATA DevInfo, DWORD Index, LPVOID Context)
{
    /* Do this by 'searching' for the current driver
     mimmicing a copy-only install to our own file queue
     and then parsing that file queue */
    SP_DRVINFO_DATA driverInfoData;
    SP_DRVINFO_DETAIL_DATA driverInfoDetail;
    LPCTSTR pStr = (LPCTSTR)Context;
    int success = EXIT_FAIL;

    ZeroMemory(&driverInfoData,sizeof(driverInfoData));
    driverInfoData.cbSize = sizeof(driverInfoData);

    if (!FindCurrentDriver(Devs, DevInfo, &driverInfoData))
        return EXIT_FAIL;

    _tprintf(_T("Driver files found!\n"));

    /* Get useful driver information */
    driverInfoDetail.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
    if (!g_pfnSetupDiGetDriverInfoDetail(Devs, DevInfo, &driverInfoData, &driverInfoDetail, sizeof(SP_DRVINFO_DETAIL_DATA), NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        /* No information about driver or section */
        _tprintf(_T("No information about driver or section!\n"));
        goto final;
    }

    if (!driverInfoDetail.InfFileName[0] || !driverInfoDetail.SectionName[0])
    {
        _tprintf(_T("Driver or section name is empty!\n"));
        goto final;
    }

    _tprintf(_T("Desc: %s\n"), driverInfoDetail.DrvDescription);
    _tprintf(_T("SecName: %s\n"), driverInfoDetail.SectionName);
    _tprintf(_T("INF-File: %s\n"), driverInfoDetail.InfFileName);

    if (_tcsstr(driverInfoDetail.DrvDescription, pStr))
    {
        _tprintf(_T("Driver name matched\n"));
        success = EXIT_OK;
    }
    else
    {
        _tprintf(_T("Driver name NOT matched\n"));
        success = EXIT_FALSE;
    }

    final:

    g_pfnSetupDiDestroyDriverInfoList(Devs, DevInfo, SPDIT_CLASSDRIVER);

    if (EXIT_OK != success && EXIT_FALSE != success)
    {
        _tprintf(_T("Something went wrong while delete the OEM INF-files!\n\n"));
    }

    return success;
}

int MatchDriver (_TCHAR* a_pszHwID, _TCHAR* a_pszDrvName)
{
    _tprintf(_T("Checking Device: %ws ; for driver desc string %ws\n"), a_pszHwID, a_pszDrvName);

    int iRet = EnumerateDevices(NULL, NULL, DIGCF_PRESENT, 1, &a_pszHwID, MatchDriverCallback, a_pszDrvName);

    return iRet;
}
#endif

int ExecuteInfFile (_TCHAR* a_pszSection, int a_iMode, _TCHAR* a_pszInf)
{
    _tprintf(_T("Executing INF-File: %ws (%ws) ...\n"), a_pszInf, a_pszSection);

    /* Executed by the installer that already has proper privileges. */
    _TCHAR szCommandLine[_MAX_PATH + 1] = { 0 };
    swprintf(szCommandLine, sizeof(szCommandLine), TEXT( "%ws %d %ws" ), a_pszSection, a_iMode, a_pszInf);

#ifdef _DEBUG
    _tprintf (_T( "Commandline: %ws\n"), szCommandLine);
#endif

    InstallHinfSection(NULL, NULL, szCommandLine, SW_SHOW);

    return EXIT_OK;
}

int AddNetworkProvider (TCHAR* a_pszProvider, int a_iOrder)
{
    TCHAR szKeyValue[512] = { 0 };
    TCHAR szNewKeyValue[512] = { 0 };
    HKEY hKey = NULL;
    DWORD disp, dwType;
    int rc;

    _tprintf(_T("Adding network provider: %ws (Order = %d)\n"), a_pszProvider, a_iOrder);

    /* Note: HWOrder is not accessible in Windows 2000; it is updated automatically anyway. */
    TCHAR *pszKey = _T("System\\CurrentControlSet\\Control\\NetworkProvider\\Order");

    rc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, pszKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, &disp);
    if (rc != ERROR_SUCCESS)
    {
        _tprintf(_T("RegCreateKeyEx %ts failed with %d!\n"), pszKey, rc);
        return EXIT_FAIL;
    }
    DWORD cbKeyValue = sizeof(szKeyValue);

    rc = RegQueryValueEx(hKey, _T("ProviderOrder"), NULL, &dwType, (LPBYTE)szKeyValue, &cbKeyValue);
    if (rc != ERROR_SUCCESS || dwType != REG_SZ)
    {
        _tprintf(_T("RegQueryValueEx failed with %d dwType = 0x%x!\n"), rc, dwType);
        return EXIT_FAIL;
    }

#ifdef _DEBUG
    _tprintf(_T("Key value: %ws\n"), szKeyValue);
#endif

    /* Create entire new list. */
    int iPos = 0;

    TCHAR* pszToken = wcstok(szKeyValue, _T(","));
    TCHAR* pszNewToken = NULL;
    while (pszToken != NULL)
    {
        pszNewToken = wcstok(NULL, _T(","));

        /* Append new provider name (at beginning if a_iOrder=0). */
        if (iPos == a_iOrder)
        {
            wcscat(szNewKeyValue, a_pszProvider);
            wcscat(szNewKeyValue, _T(","));
            iPos++;
        }

        if (0 != wcsicmp(pszToken, a_pszProvider))
        {
            wcscat(szNewKeyValue, pszToken);
            wcscat(szNewKeyValue, _T(","));
            iPos++;
        }

#ifdef _DEBUG
        _tprintf (_T("Temp new key value: %ws\n"), szNewKeyValue);
#endif

        pszToken = pszNewToken;
    }

    /* Append as last item if needed. */
    if (a_iOrder >= iPos)
        wcscat(szNewKeyValue, a_pszProvider);

    /* Last char a delimiter? Cut off ... */
    if (szNewKeyValue[wcslen(szNewKeyValue) - 1] == ',')
        szNewKeyValue[wcslen(szNewKeyValue) - 1] = '\0';

    size_t iNewLen = (wcslen(szNewKeyValue) * sizeof(WCHAR)) + sizeof(WCHAR);

    _tprintf(_T("New provider list (%u bytes): %ws\n"), iNewLen, szNewKeyValue);

    rc = RegSetValueExW(hKey, _T("ProviderOrder"), 0, REG_SZ, (LPBYTE)szNewKeyValue, (DWORD)iNewLen);

    if (rc != ERROR_SUCCESS)
    {
        _tprintf(_T("RegSetValueEx failed with %d!\n"), rc);
        return EXIT_FAIL;
    }

    rc = RegCloseKey(hKey);

    if (rc == ERROR_SUCCESS)
    {
        _tprintf(_T("Network provider successfully installed!\n"), rc);
        rc = EXIT_OK;
    }

    return rc;
}

int AddStringToMultiSZ (TCHAR* a_pszSubKey, TCHAR* a_pszKeyValue, TCHAR* a_pszValueToAdd, int a_iOrder)
{
    TCHAR szKeyValue[512] = { 0 };
    TCHAR szNewKeyValue[512] = { 0 };
    HKEY hKey = NULL;
    DWORD disp, dwType;
    int rc = 0;

    _tprintf(_T("Adding MULTI_SZ string: %ws to %ws\\%ws (Order = %d)\n"), a_pszValueToAdd, a_pszSubKey, a_pszKeyValue, a_iOrder);

    rc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, a_pszSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, &disp);
    if (rc != ERROR_SUCCESS)
    {
       _tprintf(_T("RegCreateKeyEx %ts failed with %d!\n"), a_pszSubKey, rc);
       return EXIT_FAIL;
    }
    DWORD cbKeyValue = sizeof(szKeyValue);

    rc = RegQueryValueEx(hKey, a_pszKeyValue, NULL, &dwType, (LPBYTE)szKeyValue, &cbKeyValue);
    if (rc != ERROR_SUCCESS || dwType != REG_MULTI_SZ)
    {
       _tprintf(_T("RegQueryValueEx failed with %d, dwType = 0x%x!\n"), rc, dwType);
       return EXIT_FAIL;
    }

    /* Look if the network provider is already in the list. */
    int iPos = 0;
    size_t cb = 0;

    /* Replace delimiting "\0"'s with "," to make tokenizing work. */
    for (int i=0; i<cbKeyValue/sizeof(TCHAR);i++)
        if (szKeyValue[i] == '\0') szKeyValue[i] = ',';

    TCHAR* pszToken = wcstok(szKeyValue, _T(","));
    TCHAR* pszNewToken = NULL;
    TCHAR* pNewKeyValuePos = szNewKeyValue;
    while (pszToken != NULL)
    {
       pszNewToken = wcstok(NULL, _T(","));

       /* Append new value (at beginning if a_iOrder=0). */
       if (iPos == a_iOrder)
       {
           memcpy(pNewKeyValuePos, a_pszValueToAdd, wcslen(a_pszValueToAdd)*sizeof(TCHAR));

           cb += (wcslen(a_pszValueToAdd) + 1) * sizeof(TCHAR);  /* Add trailing zero as well. */
           pNewKeyValuePos += wcslen(a_pszValueToAdd) + 1;
           iPos++;
       }

       if (0 != wcsicmp(pszToken, a_pszValueToAdd))
       {
           memcpy(pNewKeyValuePos, pszToken, wcslen(pszToken)*sizeof(TCHAR));
           cb += (wcslen(pszToken) + 1) * sizeof(TCHAR);  /* Add trailing zero as well. */
           pNewKeyValuePos += wcslen(pszToken) + 1;
           iPos++;
       }

       pszToken = pszNewToken;
    }

    /* Append as last item if needed. */
    if (a_iOrder >= iPos)
    {
        memcpy(pNewKeyValuePos, a_pszValueToAdd, wcslen(a_pszValueToAdd)*sizeof(TCHAR));
        cb += wcslen(a_pszValueToAdd) * sizeof(TCHAR);  /* Add trailing zero as well. */
    }

    rc = RegSetValueExW(hKey, a_pszKeyValue, 0, REG_MULTI_SZ, (LPBYTE)szNewKeyValue, (DWORD)cb);

    if (rc != ERROR_SUCCESS)
    {
       _tprintf(_T("RegSetValueEx failed with %d!\n"), rc);
       return EXIT_FAIL;
    }

    rc = RegCloseKey(hKey);

    if (rc == ERROR_SUCCESS)
    {
       _tprintf(_T("Value successfully written (%u bytes)!\n"), cb);
       rc = EXIT_OK;
    }

    return rc;
}

int RemoveStringFromMultiSZ (TCHAR* a_pszSubKey, TCHAR* a_pszKeyValue, TCHAR* a_pszValueToRemove)
{
    // @todo Make string sizes dynamically allocated!

    TCHAR szKeyValue[1024];
    HKEY hkey;
    DWORD disp, dwType;
    int rc;

    TCHAR *pszKey = a_pszSubKey;

    _tprintf(_T("Removing MULTI_SZ string: %ws from %ws\\%ws ...\n"), a_pszValueToRemove, a_pszSubKey, a_pszKeyValue);

    rc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, pszKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hkey, &disp);
    if (rc != ERROR_SUCCESS)
    {
        _tprintf(_T("RegCreateKeyEx %ts failed with %d!\n"), pszKey, rc);
        return EXIT_FAIL;
    }
    DWORD cbKeyValue = sizeof(szKeyValue);

    rc = RegQueryValueEx(hkey, a_pszKeyValue, NULL, &dwType, (LPBYTE)szKeyValue, &cbKeyValue);
    if (rc != ERROR_SUCCESS || dwType != REG_MULTI_SZ)
    {
        _tprintf(_T("RegQueryValueEx failed with %d dwType = 0x%x!\n"), rc, dwType);
        return EXIT_FAIL;
    }

#ifdef _DEBUG
    _tprintf(_T("Current key len: %d\n"), cbKeyValue);
#endif

    TCHAR szCurString[1024] = { 0 };
    TCHAR szFinalString[1024] = { 0 };
    int iIndex = 0;
    int iNewIndex = 0;
    for (int i = 0; i < cbKeyValue / sizeof(TCHAR); i++)
    {
        if (szKeyValue[i] != _T('\0'))
            szCurString[iIndex++] = szKeyValue[i];

        if ((!szKeyValue[i] == _T('\0')) && szKeyValue[i + 1] == _T('\0'))
        {
            if (NULL == wcsstr(szCurString, a_pszValueToRemove))
            {
                wcscat(&szFinalString[iNewIndex], szCurString);

                if (iNewIndex == 0)
                    iNewIndex = iIndex;
                else iNewIndex += iIndex;

                szFinalString[++iNewIndex] = _T('\0');
            }

            iIndex = 0;
            ZeroMemory( szCurString, sizeof(szCurString));
        }
    }

    szFinalString[++iNewIndex] = _T('\0');

#ifdef _DEBUG
    _tprintf(_T("New key len: %d\n"), iNewIndex * sizeof(TCHAR));
    _tprintf(_T("New key value: %ws\n"), szFinalString);
#endif

    rc = RegSetValueExW(hkey, a_pszKeyValue, 0, REG_MULTI_SZ, (LPBYTE)szFinalString, iNewIndex * sizeof(TCHAR));

    if (rc != ERROR_SUCCESS)
    {
        _tprintf(_T("RegSetValueEx failed with %d!\n"), rc);
        return EXIT_FAIL;
    }

    rc = RegCloseKey(hkey);

    if (rc == ERROR_SUCCESS)
    {
        _tprintf(_T("Value successfully removed!\n"), rc);
        rc = EXIT_OK;
    }

    return rc;
}

int CreateService (TCHAR* a_pszStartStopName,
                   TCHAR* a_pszDisplayName,
                   int a_iServiceType,
                   int a_iStartType,
                   TCHAR* a_pszBinPath,
                   TCHAR* a_pszLoadOrderGroup,
                   TCHAR* a_pszDependencies,
                   TCHAR* a_pszLogonUser,
                   TCHAR* a_pszLogonPw)
{
    int rc = ERROR_SUCCESS;

    _tprintf(_T("Installing service %ws (%ws) ...\n"), a_pszDisplayName, a_pszStartStopName);

    SC_HANDLE hSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
    {
        _tprintf(_T("Could not get handle to SCM! Error: %ld\n"), GetLastError());
        return EXIT_FAIL;
    }

    /* Fixup end of multistring */
    TCHAR szDepend[ _MAX_PATH ] = { 0 };        /* @todo Use dynamically allocated string here! */
    if (a_pszDependencies != NULL)
    {
        _tcsnccpy (szDepend, a_pszDependencies, wcslen(a_pszDependencies));
        DWORD len = (DWORD)wcslen (szDepend);
        szDepend [len + 1] = 0;

        /* Replace comma separator on null separator */
        for (DWORD i = 0; i < len; i++)
        {
            if (',' == szDepend [i])
                szDepend [i] = 0;
        }
    }

    DWORD dwTag = 0xDEADBEAF;
    SC_HANDLE hService = CreateService (hSCManager,             // SCManager database
                                        a_pszStartStopName,     // name of service
                                        a_pszDisplayName,       // name to display
                                        SERVICE_ALL_ACCESS,     // desired access
                                        a_iServiceType,         // service type
                                        a_iStartType,           // start type
                                        SERVICE_ERROR_NORMAL,   // error control type
                                        a_pszBinPath,           // service's binary
                                        a_pszLoadOrderGroup,    // ordering group
                                        (a_pszLoadOrderGroup != NULL) ? &dwTag : NULL,    // tag identifier
                                        (a_pszDependencies != NULL) ? szDepend : NULL,    // dependencies
                                        (a_pszLogonUser != NULL) ? a_pszLogonUser: NULL,  // account
                                        (a_pszLogonPw != NULL) ? a_pszLogonPw : NULL);    // password
    if (NULL == hService)
    {
        DWORD dwErr = GetLastError();
        switch (dwErr)
        {

        case ERROR_SERVICE_EXISTS:
        {
            _tprintf(_T("Service already exists. No installation required. Updating the service config.\n"));

            hService = OpenService (hSCManager,             // SCManager database
                                    a_pszStartStopName,     // name of service
                                    SERVICE_ALL_ACCESS);    // desired access
            if (NULL == hService)
            {
                dwErr = GetLastError();
                _tprintf(_T("Could not open service! Error: %ld\n"), dwErr);
            }
            else
            {
                BOOL Result = ChangeServiceConfig (hService,               // service handle
                                                   a_iServiceType,         // service type
                                                   a_iStartType,           // start type
                                                   SERVICE_ERROR_NORMAL,   // error control type
                                                   a_pszBinPath,           // service's binary
                                                   a_pszLoadOrderGroup,    // ordering group
                                                   (a_pszLoadOrderGroup != NULL) ? &dwTag : NULL,    // tag identifier
                                                   (a_pszDependencies != NULL) ? szDepend : NULL,    // dependencies
                                                   (a_pszLogonUser != NULL) ? a_pszLogonUser: NULL,  // account
                                                   (a_pszLogonPw != NULL) ? a_pszLogonPw : NULL,     // password
                                                   a_pszDisplayName);      // name to display
                if (Result)
                {
                    _tprintf(_T("The service config has been successfully updated.\n"));
                }
                else
                {
                    dwErr = GetLastError();
                    _tprintf(_T("Could not change service config! Error: %ld\n"), dwErr);
                }

                CloseServiceHandle (hService);
            }

            /* This entire branch do not return an error to avoid installations failures,
             * if updating service parameters. Better to have a running system with old
             * parameters and the failure information in the installation log.
             */
            break;
        }

        case ERROR_INVALID_PARAMETER:

            _tprintf(_T("Invalid parameter specified!\n"));
            rc = EXIT_FAIL;
            break;

        default:

            _tprintf(_T("Could not create service! Error: %ld\n"), dwErr);
            rc = EXIT_FAIL;
            break;
        }

        if (rc == EXIT_FAIL)
            goto cleanup;
    }
    else
    {
        CloseServiceHandle (hService);
        _tprintf(_T("Installation of service successful!\n"));
    }

cleanup:

    if (hSCManager != NULL)
        CloseServiceHandle (hSCManager);

    return rc;
}

int DelService (TCHAR* a_pszStartStopName)
{
    int rc = ERROR_SUCCESS;

    _tprintf(_T("Deleting service '%ws' ...\n"), a_pszStartStopName);

    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE hService = NULL;
    if (hSCManager == NULL)
    {
        _tprintf(_T("Could not get handle to SCM! Error: %ld\n"), GetLastError());
        rc = EXIT_FAIL;
    }
    else
    {
        hService = OpenService(hSCManager, a_pszStartStopName, SERVICE_ALL_ACCESS);
        if (NULL == hService)
        {
            _tprintf(_T("Could not open service '%ws'! Error: %ld\n"), a_pszStartStopName, GetLastError());
            rc = EXIT_FAIL;
        }
    }

    if (hService != NULL)
    {
        if (LockServiceDatabase(hSCManager))
        {
            if (FALSE == DeleteService(hService))
            {
                DWORD dwErr = GetLastError();
                switch (dwErr)
                {

                case ERROR_SERVICE_MARKED_FOR_DELETE:

                    _tprintf(_T("Service '%ws' already marked for deletion.\n"), a_pszStartStopName);
                    break;

                default:

                    _tprintf(_T("Could not delete service '%ws'! Error: %ld\n"), a_pszStartStopName, GetLastError());
                    rc = EXIT_FAIL;
                    break;
                }
            }
            else
            {
                _tprintf(_T("Service '%ws' successfully removed!\n"), a_pszStartStopName);
            }
            UnlockServiceDatabase(hSCManager);
        }
        else
        {
            _tprintf(_T("Unable to lock service database! Error: %ld\n"), GetLastError());
            rc = EXIT_FAIL;
        }
        CloseServiceHandle(hService);
    }

    if (hSCManager != NULL)
        CloseServiceHandle(hSCManager);

    return rc;
}

DWORD RegistryWrite(HKEY hRootKey,
                    const _TCHAR *pszSubKey,
                    const _TCHAR *pszValueName,
                    DWORD dwType,
                    const BYTE *pbData,
                    DWORD cbData)
{
    DWORD lRet;
    HKEY hKey;
    lRet = RegCreateKeyEx (hRootKey,
                           pszSubKey,
                           0,           /* Reserved */
                           NULL,        /* lpClass [in, optional] */
                           0,           /* dwOptions [in] */
                           KEY_WRITE,
                           NULL,        /* lpSecurityAttributes [in, optional] */
                           &hKey,
                           NULL);       /* lpdwDisposition [out, optional] */
    if (lRet != ERROR_SUCCESS)
    {
        _tprintf(_T("Could not open registry key! Error: %ld\n"), GetLastError());
    }
    else
    {
        lRet = RegSetValueEx(hKey, pszValueName, 0, dwType, (BYTE*)pbData, cbData);
        if (lRet != ERROR_SUCCESS)
            _tprintf(_T("Could not write to registry! Error: %ld\n"), GetLastError());
        RegCloseKey(hKey);

    }
    return lRet;
}

void PrintHelp (void)
{
    _tprintf(_T("Installs / Uninstalls VirtualBox drivers for Windows XP/2K/Vista\n"));
    _tprintf(_T("Version: %d.%d.%d.%d\n\n"), VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);
    _tprintf(_T("Syntax:\n"));
    _tprintf(_T("\tTo install: VBoxDrvInst /i <HardwareID> <INF-File> <Device Class>\n"));
    _tprintf(_T("\tTo uninstall: VBoxDrvInst /u <HardwareID>\n"));
    _tprintf(_T("\tTo execute an INF-File: VBoxDrvInst /inf <INF-File>\n"));
    _tprintf(_T("\tTo add a network provider: VBoxDrvInst /addnetprovider <Name> [Order]\n\n"));
    _tprintf(_T("\tTo write registry values: VBoxDrvInst /registry write <root> <sub key> <key name> <key type> <value> [type] [size]\n\n"));
    _tprintf(_T("Examples:\n"));
    _tprintf(_T("\tVBoxDrvInst /i \"PCI\\VEN_80EE&DEV_BEEF&SUBSYS_00000000&REV_00\" VBoxVideo.inf Display\n"));
    _tprintf(_T("\tVBoxDrvInst /addnetprovider VboxSF 1\n\n"));
}

int __cdecl _tmain (int argc, _TCHAR* argv[])
{
    int rc;
    OSVERSIONINFO OSinfo;

    _TCHAR szHwID[_MAX_PATH] = { 0 }; /* Hardware ID. */
    _TCHAR szINF[_MAX_PATH] = { 0 }; /* Complete path to INF file.*/
    _TCHAR szDevClass[_MAX_PATH] = { 0 }; /* Device class. */
    _TCHAR szProvider[_MAX_PATH] = { 0 }; /* The network provider name for the registry. */

    rc = LoadAPICalls();
    if (   rc == ERROR_SUCCESS
        && argc >= 2)
    {
        OSinfo.dwOSVersionInfoSize = sizeof(OSinfo);
        GetVersionEx(&OSinfo);

        if (0 == _tcsicmp(argv[1], _T("/i")))
        {
            if (argc < 5)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                if (OSinfo.dwMajorVersion < 5)
                {
                    _tprintf(_T("ERROR: Platform not supported yet!\n"));
                    rc = ERROR_NOT_SUPPORTED;
                }

                if (rc == ERROR_SUCCESS)
                {
                    _stprintf(szHwID, _T("%ws"), argv[2]);
                    _stprintf(szINF, _T("%ws"), argv[3]);
                    _stprintf(szDevClass, _T("%ws"), argv[4]);

                    rc = InstallDriver(szINF, szHwID, szDevClass);
                }
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/u")))
        {
            if (argc < 3)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                if (OSinfo.dwMajorVersion < 5)
                {
                    _tprintf(_T("ERROR: Platform not supported yet!\n"));
                    rc = ERROR_NOT_SUPPORTED;
                }

                if (rc == ERROR_SUCCESS)
                {
                    _stprintf(szHwID, _T("%ws"), argv[2]);
                    rc = UninstallDriver(szHwID);
                }
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/inf")))
        {
            if (argc < 3)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                if (OSinfo.dwMajorVersion < 5)
                {
                    _tprintf(_T("ERROR: Platform not supported yet!\n"));
                    rc = ERROR_NOT_SUPPORTED;
                }

                if (rc == ERROR_SUCCESS)
                {
                    _stprintf(szINF, _T("%ws"), argv[2]);
                    rc = ExecuteInfFile(_T("DefaultInstall"), 132, szINF);
                }
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/addnetprovider")))
        {
            if (argc < 3)
            {
                rc = EXIT_USAGE;
            }
            else
            {

                int iOrder = 0;
                if (argc > 3)
                    iOrder = _ttoi(argv[3]);
                _stprintf(szProvider, _T("%ws"), argv[2]);
                rc = AddNetworkProvider(szProvider, iOrder);
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/reg_addmultisz")))
        {
            if (argc < 6)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                rc = AddStringToMultiSZ(argv[2], argv[3], argv[4], _ttoi(argv[5]));
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/reg_delmultisz")))
        {
            if (argc < 5)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                rc = RemoveStringFromMultiSZ(argv[2], argv[3], argv[4]);
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/createsvc")))
        {
            if (argc < 7)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                rc = CreateService(
                         argv[2],
                         argv[3],
                         _ttoi(argv[4]),
                         _ttoi(argv[5]),
                         argv[6],
                         (argc > 7) ? argv[7] : NULL,
                         (argc > 8) ? argv[8] : NULL,
                         (argc > 9) ? argv[9] : NULL,
                         (argc > 10) ? argv[10] : NULL);
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/delsvc")))
        {
            if (argc < 3)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                rc = DelService(argv[2]);
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/registry")))
        {
            if (argc < 8)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                /** @todo add a handleRegistry(argc, argv) method to keep things cleaner */
                if (0 == _tcsicmp(argv[2], _T("write")))
                {
                    HKEY hRootKey = HKEY_LOCAL_MACHINE;  /** @todo needs to be expanded (argv[3]) */
                    DWORD dwValSize;
                    BYTE *pbVal = NULL;
                    DWORD dwVal;

                    if (argc > 8)
                    {
                        if (0 == _tcsicmp(argv[8], _T("dword")))
                        {
                            dwVal = _ttol(argv[7]);
                            pbVal = (BYTE*)&dwVal;
                            dwValSize = sizeof(DWORD);
                        }
                    }
                    if (pbVal == NULL) /* By default interpret value as string */
                    {
                        pbVal = (BYTE*)argv[7];
                        dwValSize = _tcslen(argv[7]);
                    }
                    if (argc > 9)
                        dwValSize = _ttol(argv[9]);      /* Get the size in bytes of the value we want to write */
                    rc = RegistryWrite(hRootKey,
                                       argv[4],          /* Sub key */
                                       argv[5],          /* Value name */
                                       REG_BINARY,       /** @todo needs to be expanded (argv[6]) */
                                       pbVal,            /* The value itself */
                                       dwValSize);       /* Size of the value */
                }
                /*else if (0 == _tcsicmp(argv[2], _T("read")))
                {
                }
                else if (0 == _tcsicmp(argv[2], _T("del")))
                {
                }*/
                else
                    rc = EXIT_USAGE;
            }
        }
#ifdef VBOX_WITH_WDDM
        else if (0 == _tcsicmp(argv[1], _T("/matchdrv")))
        {
            if (argc < 4)
            {
                rc = EXIT_USAGE;
            }
            else
            {
                if (OSinfo.dwMajorVersion < 5)
                {
                    _tprintf(_T("ERROR: Platform not supported yet!\n"));
                    rc = ERROR_NOT_SUPPORTED;
                }

                if (rc == ERROR_SUCCESS)
                {
                    _stprintf(szHwID, _T("%ws"), argv[2]);
                    rc = MatchDriver(szHwID, argv[3]);
                }
            }
        }
#endif
    }

    if (rc == EXIT_USAGE)
        PrintHelp();

    FreeAPICalls();
    return rc;
}
