/* $Id$ */
/** @file
 * VBoxNetCfgWin - Briefly describe this file, optionally with a longer description in a separate paragraph.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
/*
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 *+---------------------------------------------------------------------------
 *
 *  Microsoft Windows
 *  Copyright (C) Microsoft Corporation, 2001.
 *
 *  Author:     Alok Sinha    15-May-01
 *
 *----------------------------------------------------------------------------
 */
#include "VBox/WinNetConfig.h"

#define _WIN32_DCOM


#include <iphlpapi.h>

#include <devguid.h>
#include <stdio.h>
#include <regstr.h>
//#define INITGUID
//#include <guiddef.h>
//#include <devguid.h>
//#include <objbase.h>
//#include <setupapi.h>
#include <shlobj.h>
#include <cfgmgr32.h>
#include <tchar.h>
//#include <VBox/com/Guid.h>
//#include <VBox/com/String.h>
//#include <wtypes.h>
#include <objbase.h>

#include <crtdbg.h>
#include <stdlib.h>

#include <Wbemidl.h>
#include <comdef.h>

//#include <Winsock2.h>

//using namespace com;

#ifndef Assert
//# ifdef DEBUG
//#  define Assert(_expr) assert(_expr)
//# else
//#  define Assert(_expr) do{ }while(0)
//# endif
# define Assert _ASSERT
# define AssertMsg(expr, msg) do{}while(0)
#endif
static LOG_ROUTINE g_Logger = NULL;

static VOID DoLogging(LPCWSTR szString, ...);
#define Log DoLogging

#define DbgLog

#define VBOX_NETCFG_LOCK_TIME_OUT     5000

typedef bool (*ENUMERATION_CALLBACK) (LPCWSTR pFileName, PVOID pContext);

typedef struct _INF_INFO
{
    LPCWSTR pPnPId;
}INF_INFO, *PINF_INFO;

typedef struct _INFENUM_CONTEXT
{
    INF_INFO InfInfo;
    DWORD Flags;
    HRESULT hr;
} INFENUM_CONTEXT, *PINFENUM_CONTEXT;

static bool vboxNetCfgWinInfEnumerationCallback(LPCWSTR pFileName, PVOID pCtxt);

class VBoxNetCfgStringList
{
public:
    VBoxNetCfgStringList(int aSize);

    ~VBoxNetCfgStringList();

    HRESULT add(LPWSTR pStr);

    int size() {return mSize;}

    LPWSTR get(int i) {return maList[i];}
private:
    HRESULT resize(int newSize);

    LPWSTR *maList;
    int mBufSize;
    int mSize;
};

VBoxNetCfgStringList::VBoxNetCfgStringList(int aSize)
{
    maList = (LPWSTR*)CoTaskMemAlloc( sizeof(maList[0]) * aSize);
    mBufSize = aSize;
    mSize = 0;
}

VBoxNetCfgStringList::~VBoxNetCfgStringList()
{
    if(!mBufSize)
        return;

    for(int i = 0; i < mSize; ++i)
    {
        CoTaskMemFree(maList[i]);
    }

    CoTaskMemFree(maList);
}

HRESULT VBoxNetCfgStringList::add(LPWSTR pStr)
{
    if(mSize == mBufSize)
    {
        int hr = resize(mBufSize+10);
        if(SUCCEEDED(hr))
            return hr;
    }
    size_t cStr = wcslen(pStr) + 1;
    LPWSTR str = (LPWSTR)CoTaskMemAlloc( sizeof(maList[0][0]) * cStr);
    memcpy(str, pStr, sizeof(maList[0][0]) * cStr);
    maList[mSize] = str;
    ++mSize;
    return S_OK;
}

HRESULT VBoxNetCfgStringList::resize(int newSize)
{
    Assert(newSize >= mSize);
    if(newSize < mSize)
        return E_FAIL;
    LPWSTR* pOld = maList;
    maList = (LPWSTR*)CoTaskMemAlloc( sizeof(maList[0]) * newSize);
    mBufSize = newSize;
    memcpy(maList, pOld, mSize*sizeof(maList[0]));
    CoTaskMemFree(pOld);
    return S_OK;
}

static HRESULT vboxNetCfgWinCollectInfs(const GUID * pGuid, LPCWSTR pPnPId, VBoxNetCfgStringList & list)
{
    DWORD winEr = ERROR_SUCCESS;
    int counter = 0;
    HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(
                            pGuid, /* IN LPGUID  ClassGuid,  OPTIONAL */
                            NULL /*IN HWND  hwndParent  OPTIONAL */
                            );
    if(hDevInfo != INVALID_HANDLE_VALUE)
    {
        if(SetupDiBuildDriverInfoList(hDevInfo,
                    NULL, /*IN OUT PSP_DEVINFO_DATA  DeviceInfoData,  OPTIONAL*/
                    SPDIT_CLASSDRIVER  /*IN DWORD  DriverType*/
                    ))
        {
            SP_DRVINFO_DATA DrvInfo;
            DrvInfo.cbSize = sizeof(SP_DRVINFO_DATA);
            char DetailBuf[16384];
            PSP_DRVINFO_DETAIL_DATA pDrvDetail = (PSP_DRVINFO_DETAIL_DATA)DetailBuf;

            for(DWORD i = 0;;i++)
            {
                if(SetupDiEnumDriverInfo(hDevInfo,
                        NULL, /* IN PSP_DEVINFO_DATA  DeviceInfoData,  OPTIONAL*/
                        SPDIT_CLASSDRIVER , /*IN DWORD  DriverType,*/
                        i, /*IN DWORD  MemberIndex,*/
                        &DrvInfo /*OUT PSP_DRVINFO_DATA  DriverInfoData*/
                        ))
                {
                    DWORD dwReq;
                    pDrvDetail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
                    if(SetupDiGetDriverInfoDetail(
                            hDevInfo, /*IN HDEVINFO  DeviceInfoSet,*/
                            NULL, /*IN PSP_DEVINFO_DATA  DeviceInfoData,  OPTIONAL*/
                            &DrvInfo, /*IN PSP_DRVINFO_DATA  DriverInfoData,*/
                            pDrvDetail, /*OUT PSP_DRVINFO_DETAIL_DATA  DriverInfoDetailData,  OPTIONAL*/
                            sizeof(DetailBuf), /*IN DWORD  DriverInfoDetailDataSize,*/
                            &dwReq /*OUT PDWORD  RequiredSize  OPTIONAL*/
                            ))
                    {
                        for(WCHAR * pHwId = pDrvDetail->HardwareID; pHwId && *pHwId && pHwId < (TCHAR*)(DetailBuf + sizeof(DetailBuf)/sizeof(DetailBuf[0])) ;pHwId += _tcslen(pHwId) + 1)
                        {
                            if(!wcsicmp(pHwId, pPnPId))
                            {
                                Assert(pDrvDetail->InfFileName[0]);
                                if(pDrvDetail->InfFileName)
                                {
                                    list.add(pDrvDetail->InfFileName);
                                }
                            }
                        }
                    }
                    else
                    {
                        DWORD winEr = GetLastError();
                        Log(L"Err SetupDiGetDriverInfoDetail (%d), req = %d", winEr, dwReq);
//                        Assert(0);
                    }

                }
                else
                {
                    DWORD winEr = GetLastError();
                    if(winEr == ERROR_NO_MORE_ITEMS)
                    {
                        break;
                    }

                    Assert(0);
                }
            }

            SetupDiDestroyDriverInfoList(hDevInfo,
                      NULL, /*IN PSP_DEVINFO_DATA  DeviceInfoData,  OPTIONAL*/
                      SPDIT_CLASSDRIVER/*IN DWORD  DriverType*/
                      );
        }
        else
        {
            winEr = GetLastError();
            Assert(0);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    else
    {
        winEr = GetLastError();
        Assert(0);
    }

    return HRESULT_FROM_WIN32(winEr);
}

//
// Function:  GetKeyValue
//
// Purpose:   Retrieve the value of a key from the inf file.
//
// Arguments:
//    hInf        [in]  Inf file handle.
//    lpszSection [in]  Section name.
//    lpszKey     [in]  Key name.
//    dwIndex     [in]  Key index.
//    lppszValue  [out] Key value.
//
// Returns:   S_OK on success, otherwise and error code.
//
// Notes:
//

static HRESULT vboxNetCfgWinGetKeyValue (HINF hInf,
                     LPCWSTR lpszSection,
                     LPCWSTR lpszKey,
                     DWORD  dwIndex,
                     LPWSTR *lppszValue)
{
    INFCONTEXT  infCtx;
    DWORD       dwSizeNeeded;
    HRESULT     hr;

    *lppszValue = NULL;

    if ( SetupFindFirstLineW(hInf,
                             lpszSection,
                             lpszKey,
                             &infCtx) == FALSE )
    {
        DWORD winEr = GetLastError();
        DbgLog(L"vboxNetCfgWinGetKeyValue: SetupFindFirstLineW failed, winEr = (%d)\n", winEr);
        //Assert(0);
        return HRESULT_FROM_WIN32(winEr);
    }

    SetupGetStringFieldW( &infCtx,
                          dwIndex,
                          NULL,
                          0,
                          &dwSizeNeeded );

    *lppszValue = (LPWSTR)CoTaskMemAlloc( sizeof(WCHAR) * dwSizeNeeded );

    if ( !*lppszValue  )
    {
        Log(L"vboxNetCfgWinGetKeyValue: CoTaskMemAlloc failed\n");
        Assert(0);
        return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
    }

    if ( SetupGetStringFieldW(&infCtx,
                              dwIndex,
                              *lppszValue,
                              dwSizeNeeded,
                              NULL) == FALSE )
    {
        DWORD winEr = GetLastError();
        DbgLog(L"vboxNetCfgWinGetKeyValue: SetupGetStringFieldW failed, winEr = (%d)\n", winEr);
        hr = HRESULT_FROM_WIN32(winEr);
        //Assert(0);
        CoTaskMemFree( *lppszValue );
        *lppszValue = NULL;
    }
    else
    {
        hr = S_OK;
    }

    return hr;
}

//
// Function:  GetPnpID
//
// Purpose:   Retrieve PnpID from an inf file.
//
// Arguments:
//    lpszInfFile [in]  Inf file to search.
//    lppszPnpID  [out] PnpID found.
//
// Returns:   TRUE on success.
//
// Notes:
//

static HRESULT vboxNetCfgWinGetPnpID (LPCWSTR lpszInfFile,
                  LPWSTR *lppszPnpID)
{
    HINF    hInf;
    LPWSTR  lpszModelSection;
    HRESULT hr;

    *lppszPnpID = NULL;

    hInf = SetupOpenInfFileW( lpszInfFile,
                              NULL,
                              INF_STYLE_WIN4,
                              NULL );

    if ( hInf == INVALID_HANDLE_VALUE )
    {
        DWORD winEr = GetLastError();
        DbgLog(L"vboxNetCfgWinGetPnpID: SetupOpenInfFileW failed, winEr = (%d), for file (%s)\n", winEr, lpszInfFile);
        //Assert(0);
        return HRESULT_FROM_WIN32(winEr);
    }

    //
    // Read the Model section name from Manufacturer section.
    //

    hr = vboxNetCfgWinGetKeyValue( hInf,
                      L"Manufacturer",
                      NULL,
                      1,
                      &lpszModelSection );
    //Assert(hr == S_OK);
    if ( hr == S_OK )
    {

        //
        // Read PnpID from the Model section.
        //

        hr = vboxNetCfgWinGetKeyValue( hInf,
                          lpszModelSection,
                          NULL,
                          2,
                          lppszPnpID );
        //Assert(hr == S_OK);
        if ( hr != S_OK )
        {
            DbgLog(L"vboxNetCfgWinGetPnpID: vboxNetCfgWinGetKeyValue lpszModelSection failed, hr = (0x%x), for file (%s)\n", hr, lpszInfFile);
        }

        CoTaskMemFree( lpszModelSection );
    }
    else
    {
        DbgLog(L"vboxNetCfgWinGetPnpID: vboxNetCfgWinGetKeyValue Manufacturer failed, hr = (0x%x), for file (%s)\n", hr, lpszInfFile);
    }

    SetupCloseInfFile( hInf );

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinUninstallInfs (const GUID * pGuid, LPCWSTR pPnPId, DWORD Flags)
{
    VBoxNetCfgStringList list(128);
    HRESULT hr = vboxNetCfgWinCollectInfs(pGuid, pPnPId, list);
    if(hr == S_OK)
    {
        INFENUM_CONTEXT Context;
        Context.InfInfo.pPnPId = pPnPId;
        Context.Flags = Flags;
        Context.hr = S_OK;
        int size = list.size();
        for (int i = 0; i < size; ++i)
        {
            LPCWSTR pInf = list.get(i);
            const WCHAR* pRel = wcsrchr(pInf, '\\');
            if(pRel)
                ++pRel;
            else
                pRel = pInf;

            vboxNetCfgWinInfEnumerationCallback(pRel, &Context);
//            Log(L"inf : %s\n", list.get(i));
        }
    }
    return hr;
}

static HRESULT vboxNetCfgWinEnumFiles(LPCWSTR pPattern, ENUMERATION_CALLBACK pCallback, PVOID pContext)
{
    WIN32_FIND_DATA Data;
    memset(&Data, 0, sizeof(Data));
    HRESULT hr = S_OK;

    HANDLE hEnum = FindFirstFile(pPattern,&Data);
    if(hEnum != INVALID_HANDLE_VALUE)
    {

        do
        {
            if(!pCallback(Data.cFileName, pContext))
            {
                break;
            }

            /* next iteration */
            memset(&Data, 0, sizeof(Data));
            BOOL bNext = FindNextFile(hEnum,&Data);
            if(!bNext)
            {
                int winEr = GetLastError();
                if(winEr != ERROR_NO_MORE_FILES)
                {
                    Log(L"vboxNetCfgWinEnumFiles: FindNextFile err winEr (%d)\n", winEr);
                    Assert(0);
                    hr = HRESULT_FROM_WIN32(winEr);
                }
                break;
            }
        }while(true);
        FindClose(hEnum);
    }
    else
    {
        int winEr = GetLastError();
        if(winEr != ERROR_NO_MORE_FILES)
        {
            Log(L"vboxNetCfgWinEnumFiles: FindFirstFile err winEr (%d)\n", winEr);
            Assert(0);
            hr = HRESULT_FROM_WIN32(winEr);
        }
    }

    return hr;
}

static bool vboxNetCfgWinInfEnumerationCallback(LPCWSTR pFileName, PVOID pCtxt)
{
    PINFENUM_CONTEXT pContext = (PINFENUM_CONTEXT)pCtxt;
//    Log(L"vboxNetCfgWinInfEnumerationCallback: pFileName (%s)\n", pFileName);

    LPWSTR lpszPnpID;
    HRESULT hr = vboxNetCfgWinGetPnpID (pFileName,
                      &lpszPnpID);
//    Assert(hr == S_OK);
    if(hr == S_OK)
    {
        if(!wcsicmp(pContext->InfInfo.pPnPId, lpszPnpID))
        {
            if(!SetupUninstallOEMInfW(pFileName,
                        pContext->Flags, /*DWORD Flags could be SUOI_FORCEDELETE */
                        NULL /*__in  PVOID Reserved == NULL */
                        ))
            {
                DWORD dwError = GetLastError();

                Log(L"vboxNetCfgWinInfEnumerationCallback: SetupUninstallOEMInf failed for file (%s), r (%d)\n", pFileName, dwError);
                Assert(0);
                hr = HRESULT_FROM_WIN32( dwError );
            }
        }
        CoTaskMemFree(lpszPnpID);
    }
    else
    {
        DbgLog(L"vboxNetCfgWinInfEnumerationCallback: vboxNetCfgWinGetPnpID failed, hr = (0x%x)\n", hr);
    }

    return true;
}


static HRESULT VBoxNetCfgWinUninstallInfs(LPCWSTR pPnPId, DWORD Flags)
{
    WCHAR InfDirPath[MAX_PATH];
    HRESULT hr = SHGetFolderPathW(NULL, /*          HWND hwndOwner*/
            CSIDL_WINDOWS, /* int nFolder*/
            NULL, /*HANDLE hToken*/
            SHGFP_TYPE_CURRENT, /*DWORD dwFlags*/
            InfDirPath);
    Assert(hr == S_OK);
    if(hr ==  S_OK)
    {
        wcscat(InfDirPath, L"\\inf\\oem*.inf");

        INFENUM_CONTEXT Context;
        Context.InfInfo.pPnPId = pPnPId;
        Context.Flags = Flags;
        Context.hr = S_OK;
        hr = vboxNetCfgWinEnumFiles(InfDirPath, vboxNetCfgWinInfEnumerationCallback, &Context);
        Assert(hr == S_OK);
        if(hr == S_OK)
        {
            hr = Context.hr;
        }
        else
        {
            Log(L"VBoxNetCfgWinUninstallInfs: vboxNetCfgWinEnumFiles failed, hr = (0x%x)\n", hr);
        }
//
//        HANDLE hInfDir = CreateFileW(InfDirPath,
//                FILE_READ_DATA, /*      DWORD dwDesiredAccess*/
//                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, /*      DWORD dwShareMode */
//                NULL, /* LPSECURITY_ATTRIBUTES lpSecurityAttributes */
//                OPEN_EXISTING, /* DWORD dwCreationDisposition */
//                0, /* DWORD dwFlagsAndAttributes */
//                NULL);
//        if(hInfDir != INVALID_HANDLE_VALUE)
//        {
//
//            CloseHandle(hInfDir);
//        }
//        else
//        {
//            int winEr = GetLastError();
//            hr = HRESULT_FROM_WIN32(winEr);
//        }
    }
    else
    {
        Log(L"VBoxNetCfgWinUninstallInfs: SHGetFolderPathW failed, hr = (0x%x)\n", hr);
    }

    return hr;

}



/**
 * Release reference
 *
 * @param   punk    Pointer to the interface to release reference to.
 */
VBOXNETCFGWIN_DECL(VOID)
VBoxNetCfgWinReleaseRef(IN IUnknown* punk)
{
    if(punk)
    {
        punk->Release();
    }

    return;
}

/**
 * Get a reference to INetCfg.
 *
 * @return HRESULT       S_OK on success, otherwise an error code
 * @param fGetWriteLock  If TRUE, Write lock requested
 * @param lpszAppName    Application name requesting the reference.
 * @param ppnc           pointer the Reference to INetCfg to be stored to
 * @param lpszLockedBy   pointer the Application name who holds the write lock to be stored to, optional
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinQueryINetCfgEx(IN BOOL fGetWriteLock,
                          IN LPCWSTR lpszAppName,
                          IN DWORD cmsTimeout,
                          OUT INetCfg **ppnc,
                          OUT LPWSTR *lpszLockedBy)
{
    INetCfg      *pnc = NULL;
    INetCfgLock  *pncLock = NULL;
    HRESULT      hr = S_OK;

    /*
    * Initialize the output parameters.
    */
    *ppnc = NULL;

    if ( lpszLockedBy )
    {
        *lpszLockedBy = NULL;
    }
//    COM should be initialized before using the NetConfig API
//    /*
//    * Initialize COM
//    */
//    hr = CoInitialize( NULL );

    if ( hr == S_OK )
    {

        /*
        * Create the object implementing INetCfg.
        */
        hr = CoCreateInstance( CLSID_CNetCfg,
                               NULL, CLSCTX_INPROC_SERVER,
                               IID_INetCfg,
                               (void**)&pnc );
        Assert(hr == S_OK);
        if ( hr == S_OK )
        {

            if ( fGetWriteLock )
            {

                /*
                * Get the locking reference
                */
                hr = pnc->QueryInterface( IID_INetCfgLock,
                                          (LPVOID *)&pncLock );
                Assert(hr == S_OK);
                if ( hr == S_OK )
                {

                    /*
                    * Attempt to lock the INetCfg for read/write
                    */
                    hr = pncLock->AcquireWriteLock( cmsTimeout,
                                                    lpszAppName,
                                                    lpszLockedBy);
                    if (hr == S_FALSE )
                    {
                        Log(L"VBoxNetCfgWinQueryINetCfg: WriteLock busy\n");
                        hr = NETCFG_E_NO_WRITE_LOCK;
                    }
                    else if (hr != S_OK)
                    {
                        Log(L"VBoxNetCfgWinQueryINetCfg: AcquireWriteLock failed, hr (0x%x)\n", hr);
                    }
                }
                else
                {
                    Log(L"VBoxNetCfgWinQueryINetCfg: QueryInterface for IID_INetCfgLock failed, hr (0x%x)\n", hr);
                }
            }

            if ( hr == S_OK )
            {

                /*
                * Initialize the INetCfg object.
                */
                hr = pnc->Initialize( NULL );
                Assert(hr == S_OK);
                if ( hr == S_OK )
                {
                    *ppnc = pnc;
                    pnc->AddRef();
                }
                else
                {
                    Log(L"VBoxNetCfgWinQueryINetCfg: Initialize failed, hr (0x%x)\n", hr);

                    /*
                    * Initialize failed, if obtained lock, release it
                    */
                    if ( pncLock )
                    {
                        pncLock->ReleaseWriteLock();
                    }
                }
            }

            VBoxNetCfgWinReleaseRef( pncLock );
            VBoxNetCfgWinReleaseRef( pnc );
        }
        else
        {
            Log(L"VBoxNetCfgWinQueryINetCfg: CoCreateInstance for CLSID_CNetCfg failed, hr (0x%x)\n", hr);
        }

//    COM should be initialized before using the NetConfig API
//        /*
//        * In case of error, uninitialize COM.
//        */
//        if ( hr != S_OK )
//        {
//            CoUninitialize();
//        }
    }

    return hr;
}

/**
 * Get a reference to INetCfg.
 *
 * @return HRESULT       S_OK on success, otherwise an error code
 * @param fGetWriteLock  If TRUE, Write lock requested
 * @param lpszAppName    Application name requesting the reference.
 * @param ppnc           pointer the Reference to INetCfg to be stored to
 * @param lpszLockedBy   pointer the Application name who holds the write lock to be stored to, optional
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinQueryINetCfg(IN BOOL fGetWriteLock,
                          IN LPCWSTR lpszAppName,
                          OUT INetCfg **ppnc,
                          OUT LPWSTR *lpszLockedBy)
{
    return VBoxNetCfgWinQueryINetCfgEx(fGetWriteLock,
            lpszAppName,
            VBOX_NETCFG_LOCK_TIME_OUT,
            ppnc,
            lpszLockedBy);
}
/**
 * Release a reference to INetCfg.
 *
 * @param pnc           Reference to INetCfg to release.
 * @param fHasWriteLock If TRUE, reference was held with write lock.
 * @return              S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinReleaseINetCfg(IN INetCfg *pnc,
                            IN BOOL fHasWriteLock)
{
    INetCfgLock    *pncLock = NULL;
    HRESULT        hr = S_OK;

    /*
    * Uninitialize INetCfg
    */
    hr = pnc->Uninitialize();
    Assert(hr == S_OK);
    /*
    * If write lock is present, unlock it
    */
    if ( hr == S_OK && fHasWriteLock )
    {

        /*
        * Get the locking reference
        */
        hr = pnc->QueryInterface( IID_INetCfgLock,
                                 (LPVOID *)&pncLock);
        Assert(hr == S_OK);
        if ( hr == S_OK )
        {
            hr = pncLock->ReleaseWriteLock();
            VBoxNetCfgWinReleaseRef( pncLock );
        }
    }
    else if(hr != S_OK)
    {
        Log(L"VBoxNetCfgWinReleaseINetCfg: Uninitialize failed, hr (0x%x)\n", hr);
    }

    VBoxNetCfgWinReleaseRef( pnc );

//    COM should be initialized before using the NetConfig API
//    /*
//    * Uninitialize COM.
//    */
//    CoUninitialize();

    return hr;
}

/**
 * Get network component enumerator reference.
 *
 * @param pnc        Reference to INetCfg.
 * @param pguidClass Class GUID of the network component.
 * @param ppencc     address the Enumerator reference to be stored to.
 * @return           S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetComponentEnum(INetCfg *pnc,
                              IN const GUID *pguidClass,
                              OUT IEnumNetCfgComponent **ppencc)
{
    INetCfgClass  *pncclass;
    HRESULT       hr;

    *ppencc = NULL;

    /*
    * Get the class reference.
    */
    hr = pnc->QueryNetCfgClass( pguidClass,
                                IID_INetCfgClass,
                                (PVOID *)&pncclass );
    Assert(hr == S_OK);
    if ( hr == S_OK )
    {

        /*
        * Get the enumerator reference.
        */
        hr = pncclass->EnumComponents( ppencc );

        /*
        * We don't need the class reference any more.
        */
        VBoxNetCfgWinReleaseRef( pncclass );
    }
    else
    {
        Log(L"VBoxNetCfgWinGetComponentEnum: QueryNetCfgClass for IID_INetCfgClass failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/**
 * Enumerates the first network component.
 *
 * @param pencc Component enumerator reference.
 * @param ppncc address the Network component reference to be stored to
 * @return S_OK on success, otherwise an error code.
 */
/**
 * Enumerate the next network component.
 * The function behaves just like VBoxNetCfgWinGetFirstComponent if
 * it is called right after VBoxNetCfgWinGetComponentEnum.
 *
 * @param pencc Component enumerator reference.
 * @param ppncc address the Network component reference to be stored to.
 * @return S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetNextComponent(IN IEnumNetCfgComponent *pencc,
                              OUT INetCfgComponent **ppncc)
{
    HRESULT  hr;
    ULONG    ulCount;

    *ppncc = NULL;

    hr = pencc->Next( 1,
                     ppncc,
                     &ulCount );
    Assert(hr == S_OK || hr == S_FALSE);
    if(hr != S_OK && hr != S_FALSE)
    {
        Log(L"VBoxNetCfgWinGetNextComponent: Next failed, hr (0x%x)\n", hr);
    }
    return hr;
}

/**
 * Install a network component(protocols, clients and services)
 *            given its INF file.
 * @param pnc Reference to INetCfg.
 * @param lpszComponentId PnpID of the network component.
 * @param pguidClass Class GUID of the network component.
 * @return S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinInstallComponent(IN INetCfg *pnc,
                              IN LPCWSTR szComponentId,
                              IN const GUID *pguidClass)
{
    INetCfgClassSetup   *pncClassSetup = NULL;
    INetCfgComponent    *pncc = NULL;
    OBO_TOKEN           OboToken;
    HRESULT             hr = S_OK;

    /*
    * OBO_TOKEN specifies on whose behalf this
    * component is being installed.
    * Set it to OBO_USER so that szComponentId will be installed
    * on behalf of the user.
    */

    ZeroMemory( &OboToken,
                sizeof(OboToken) );
    OboToken.Type = OBO_USER;

    /*
    * Get component's setup class reference.
    */
    hr = pnc->QueryNetCfgClass ( pguidClass,
                                 IID_INetCfgClassSetup,
                                 (void**)&pncClassSetup );
    Assert(hr == S_OK);
    if ( hr == S_OK )
    {

        hr = pncClassSetup->Install( szComponentId,
                                     &OboToken,
                                     0,
                                     0,       /* Upgrade from build number. */
                                     NULL,    /* Answerfile name */
                                     NULL,    /* Answerfile section name */
                                     &pncc ); /* Reference after the component */
                                              /* is installed. */
        Assert(hr == S_OK);
        if ( S_OK == hr )
        {

            /*
            * we don't need to use pncc (INetCfgComponent), release it
            */
            VBoxNetCfgWinReleaseRef( pncc );
        }
        else
        {
            Log(L"VBoxNetCfgWinInstallComponent: Install failed, hr (0x%x)\n", hr);
        }

        VBoxNetCfgWinReleaseRef( pncClassSetup );
    }
    else
    {
        Log(L"VBoxNetCfgWinInstallComponent: QueryNetCfgClass for IID_INetCfgClassSetup failed, hr (0x%x)\n", hr);
    }

    return hr;
}

static HRESULT vboxNetCfgWinRemoveInf(IN LPCWSTR pInfFullPath, DWORD Flags)
{
    DWORD     dwError;
    HRESULT   hr = S_OK;
    WCHAR     Drive[_MAX_DRIVE];
    WCHAR     Dir[_MAX_DIR];
    WCHAR     DirWithDrive[_MAX_DRIVE+_MAX_DIR];
    WCHAR     OemInfFullPath[MAX_PATH];
    PWCHAR    pOemInfName;

    /*
    * Get the path where the INF file is.
    */
    _wsplitpath( pInfFullPath, Drive, Dir, NULL, NULL );

    wcscpy( DirWithDrive, Drive );
    wcscat( DirWithDrive, Dir );

    /*
     * get the oem file name.
     */
    if (SetupCopyOEMInfW( pInfFullPath,
                                DirWithDrive,  /* Other files are in the */
                                                /* same dir. as primary INF */
                                SPOST_PATH,    /* First param is path to INF */
                                SP_COPY_REPLACEONLY, /* we want to get the inf file name */
                                OemInfFullPath,          /* Name of the INF after */
                                                /* it's copied to %windir%\inf */
                                sizeof(OemInfFullPath) / sizeof(OemInfFullPath[0]),             /* Max buf. size for the above */
                                NULL,          /* Required size if non-null */
                                &pOemInfName ) )       /* Optionally get the filename */
    {
        Log(L"vboxNetCfgWinRemoveInf: found inf file (%s) for (%s)\n", OemInfFullPath, pInfFullPath);

        if(!SetupUninstallOEMInfW(pOemInfName,
                    Flags, /*DWORD Flags could be SUOI_FORCEDELETE */
                    NULL /*__in  PVOID Reserved == NULL */
                    ))
        {
                dwError = GetLastError();

                Log(L"vboxNetCfgWinRemoveInf: SetupUninstallOEMInf failed for file (%s), r (%d)\n", pInfFullPath, dwError);
                Log(L"vboxNetCfgWinRemoveInf: DirWithDrive (%s)\n", DirWithDrive);

                Assert(0);
                hr = HRESULT_FROM_WIN32( dwError );
        }
    }
    else
    {
        dwError = GetLastError();

        Log(L"vboxNetCfgWinRemoveInf: SetupCopyOEMInfW failed for file (%s), r (%d)\n", pInfFullPath, dwError);
        Log(L"vboxNetCfgWinRemoveInf: DirWithDrive (%s)\n", DirWithDrive);


        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }
    return hr;
}

static HRESULT vboxNetCfgWinCopyInf(IN LPCWSTR pInfFullPath)
{
    DWORD     dwError;
    HRESULT   hr = S_OK;
    WCHAR     Drive[_MAX_DRIVE];
    WCHAR     Dir[_MAX_DIR];
    WCHAR     DirWithDrive[_MAX_DRIVE+_MAX_DIR];
    WCHAR     DirDestInf[_MAX_PATH] = { 0 };

    /*
    * Get the path where the INF file is.
    */
    _wsplitpath( pInfFullPath, Drive, Dir, NULL, NULL );

    wcscpy( DirWithDrive, Drive );
    wcscat( DirWithDrive, Dir );

    /*
    * Copy the INF file and other files referenced in the INF file.
    */
    if ( !SetupCopyOEMInfW( pInfFullPath,
                            DirWithDrive,  /* Other files are in the */
                                            /* same dir. as primary INF */
                            SPOST_PATH,    /* First param is path to INF */
                            0,             /* Default copy style */
                            DirDestInf,    /* Name of the INF after */
                                           /* it's copied to %windir%\inf */
                            sizeof(DirDestInf), /* Max buf. size for the above */
                            NULL,          /* Required size if non-null */
                            NULL ) )       /* Optionally get the filename */
    {
        dwError = GetLastError();

        Log(L"vboxNetCfgWinCopyInf: SetupCopyOEMInfW failed for file (%s), r (%d)\n", pInfFullPath, dwError);
        Log(L"vboxNetCfgWinCopyInf: DirWithDrive (%s), DirDestInf(%s)\n", DirWithDrive, DirDestInf);

        Assert(0);

        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}

/**
 * Install a network component(protocols, clients and services)
 *            given its INF file.
 *
 * @param pnc Reference to INetCfg.
 * @param lpszComponentId PnpID of the network component.
 * @param pguidClass Class GUID of the network component.
 * @param lpszInfFullPath INF file to install from.
 * @return S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinInstallNetComponent(IN INetCfg *pnc,
                                 IN LPCWSTR lpszComponentId,
                                 IN const GUID *pguidClass,
                                 IN LPCWSTR * apInfFullPaths,
                                 IN UINT cInfFullPaths)
{
    HRESULT   hr = S_OK;

    /*
    * If full path to INF has been specified, the INF
    * needs to be copied using Setup API to ensure that any other files
    * that the primary INF copies will be correctly found by Setup API
    */
    for(UINT i = 0; i < cInfFullPaths; i++)
    {
        hr = vboxNetCfgWinCopyInf(apInfFullPaths[i]);
        Assert(hr == S_OK);
        if(hr != S_OK)
        {
            Log(L"VBoxNetCfgWinInstallNetComponent: vboxNetCfgWinCopyInf failed, hr (0x%x)\n", hr);
            if(i != 0)
            {
                /* remove infs already installed */
                for(UINT j = i-1; j != 0; j--)
                {
                    vboxNetCfgWinRemoveInf(apInfFullPaths[j], 0);
                }
            }

            break;
        }
    }

    if (S_OK == hr)
    {
        /*
        * Install the network component.
        */
        hr = VBoxNetCfgWinInstallComponent( pnc,
                                            lpszComponentId,
                                            pguidClass );
        Assert(hr == S_OK);
        if ( hr == S_OK )
        {
            /*
            * On success, apply the changes
            */
            HRESULT tmpHr = pnc->Apply();
            Assert(tmpHr == S_OK);
            if(tmpHr != S_OK)
            {
                Log(L"VBoxNetCfgWinInstallNetComponent: Apply failed, hr (0x%x)\n", tmpHr);
            }
        }
        else
        {
            Log(L"VBoxNetCfgWinInstallNetComponent: VBoxNetCfgWinInstallComponent failed, hr (0x%x)\n", hr);
        }
    }

    return hr;
}

/**
 * Uninstall a network component.
 *
 * @param pnc Reference to INetCfg.
 * @param lpszInfId PnpID of the network component to uninstall
 * @param apInfFiles array of null-terminated strings containing the inf file names describing drivers to be removed from the system
 * @param cInfFiles the size of apInfFiles array
 * @return S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinUninstallComponent(IN INetCfg *pnc,
                                IN INetCfgComponent *pncc)
{
    INetCfgClass         *pncClass;
    INetCfgClassSetup    *pncClassSetup;
    GUID                 guidClass;
    OBO_TOKEN            obo;
    HRESULT              hr;

        /*
         * Get the class GUID.
         */
        hr = pncc->GetClassGuid( &guidClass );
        Assert(hr == S_OK);
        if ( hr == S_OK )
        {

            /*
            * Get a reference to component's class.
            */
            hr = pnc->QueryNetCfgClass( &guidClass,
                                        IID_INetCfgClass,
                                        (PVOID *)&pncClass );
            Assert(hr == S_OK);
            if ( hr == S_OK )
            {

                /*
                * Get the setup interface.
                */
                hr = pncClass->QueryInterface( IID_INetCfgClassSetup,
                                               (LPVOID *)&pncClassSetup );
                Assert(hr == S_OK);
                if ( hr == S_OK )
                {

                    /*
                    * Uninstall the component.
                    */
                    ZeroMemory( &obo,
                                sizeof(OBO_TOKEN) );

                    obo.Type = OBO_USER;

                    hr = pncClassSetup->DeInstall( pncc,
                                                   &obo,
                                                   NULL );
                    Assert(hr == S_OK);
                    if ( (hr == S_OK) || (hr == NETCFG_S_REBOOT) )
                    {
                        HRESULT tmpHr = pnc->Apply();
                        /* we ignore apply failures since they might occur on misconfigured systems*/
                        Assert(tmpHr == S_OK);
                        if ( (tmpHr != S_OK) )
                        {
                            Log(L"VBoxNetCfgWinUninstallComponent: Apply failed, hr (0x%x), ignoring\n", tmpHr);
                        }
                    }
                    else
                    {
                        Log(L"VBoxNetCfgWinUninstallComponent: DeInstall failed, hr (0x%x)\n", hr);
                    }

                    VBoxNetCfgWinReleaseRef( pncClassSetup );
                }
                else
                {
                    Log(L"VBoxNetCfgWinUninstallComponent: QueryInterface for IID_INetCfgClassSetup failed, hr (0x%x)\n", hr);
                }

                VBoxNetCfgWinReleaseRef( pncClass );
            }
            else
            {
                Log(L"VBoxNetCfgWinUninstallComponent: QueryNetCfgClass failed, hr (0x%x)\n", hr);
            }
        }
        else
        {
            Log(L"VBoxNetCfgWinUninstallComponent: GetClassGuid failed, hr (0x%x)\n", hr);
        }

    return hr;
}

#define VBOXNETCFGWIN_NETFLT_ID    L"sun_VBoxNetFlt"
#define VBOXNETCFGWIN_NETFLT_MP_ID L"sun_VBoxNetFltmp"

static HRESULT vboxNetCfgWinNetFltUninstall(IN INetCfg *pNc, DWORD InfRmFlags)
{
    INetCfgComponent * pNcc = NULL;
    HRESULT hr = pNc->FindComponent(VBOXNETCFGWIN_NETFLT_ID, &pNcc);
    if(hr == S_OK)
    {
        Log(L"NetFlt Is Installed currently\n");

        hr = VBoxNetCfgWinUninstallComponent(pNc, pNcc);

        VBoxNetCfgWinReleaseRef(pNcc);
    }
    else if(hr == S_FALSE)
    {
        Log(L"NetFlt Is Not Installed currently\n");
        hr = S_OK;
    }
    else
    {
        Log(L"vboxNetCfgWinNetFltUninstall: FindComponent for NetFlt failed, hr (0x%x)\n", hr);
        hr = S_OK;
    }

    VBoxNetCfgWinUninstallInfs(VBOXNETCFGWIN_NETFLT_ID, InfRmFlags);
    VBoxNetCfgWinUninstallInfs(VBOXNETCFGWIN_NETFLT_MP_ID, InfRmFlags);

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinNetFltUninstall(IN INetCfg *pNc)
{
    return vboxNetCfgWinNetFltUninstall(pNc, 0);
}

VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinNetFltInstall(IN INetCfg *pNc, IN LPCWSTR * apInfFullPaths, IN UINT cInfFullPaths)
{
    HRESULT hr = vboxNetCfgWinNetFltUninstall(pNc, SUOI_FORCEDELETE);

    hr = VBoxNetCfgWinInstallNetComponent(pNc, VBOXNETCFGWIN_NETFLT_ID,
                                     &GUID_DEVCLASS_NETSERVICE,
                                     apInfFullPaths,
                                     cInfFullPaths);

    return hr;
}


/**
 * Get network component's binding path enumerator reference.
 *
 * @param pncc Network component reference.
 * @param dwBindingType EBP_ABOVE or EBP_BELOW.
 * @param ppencbp address the Enumerator reference to be stored to
 * @return S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetBindingPathEnum(IN INetCfgComponent *pncc,
                                IN DWORD dwBindingType,
                                OUT IEnumNetCfgBindingPath **ppencbp)
{
    INetCfgComponentBindings *pnccb = NULL;
    HRESULT                  hr;

    *ppencbp = NULL;

    /* Get component's binding. */
    hr = pncc->QueryInterface( IID_INetCfgComponentBindings,
                               (PVOID *)&pnccb );
    Assert(hr == S_OK);
    if ( hr == S_OK )
    {

        /* Get binding path enumerator reference. */
        hr = pnccb->EnumBindingPaths( dwBindingType,
                                      ppencbp );
        if(hr != S_OK)
        {
            Log(L"VBoxNetCfgWinGetBindingPathEnum: EnumBindingPaths failed, hr (0x%x)\n", hr);
        }

        VBoxNetCfgWinReleaseRef( pnccb );
    }
    else
    {
        Log(L"VBoxNetCfgWinGetBindingPathEnum: QueryInterface for IID_INetCfgComponentBindings failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/**
 * Enumerates the first binding path.
 *
 * @param pencc Binding path enumerator reference.
 * @param ppncc address the Binding path reference to be stored to
 * @return S_OK on success, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetFirstComponent(IN IEnumNetCfgComponent *pencc,
                               OUT INetCfgComponent **ppncc)
{
    HRESULT  hr;
    ULONG    ulCount;

    *ppncc = NULL;

    pencc->Reset();

    hr = pencc->Next( 1,
                      ppncc,
                      &ulCount );
    Assert(hr == S_OK || hr == S_FALSE);
    if(hr != S_OK && hr != S_FALSE)
    {
        Log(L"VBoxNetCfgWinGetFirstComponent: Next failed, hr (0x%x)\n", hr);
    }
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetFirstBindingPath(IN IEnumNetCfgBindingPath *pencbp,
                                 OUT INetCfgBindingPath **ppncbp)
{
    ULONG   ulCount;
    HRESULT hr;

    *ppncbp = NULL;

    pencbp->Reset();

    hr = pencbp->Next( 1,
                       ppncbp,
                       &ulCount );
    Assert(hr == S_OK || hr == S_FALSE);
    if(hr != S_OK && hr != S_FALSE)
    {
        Log(L"VBoxNetCfgWinGetFirstBindingPath: Next failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/**
 * Get binding interface enumerator reference.
 *
 * @param pncbp Binding path reference.
 * @param ppencbp address the Enumerator reference to be stored to
 * @return S_OK on success, otherwise an error code
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetBindingInterfaceEnum(IN INetCfgBindingPath *pncbp,
                                     OUT IEnumNetCfgBindingInterface **ppencbi)
{
    HRESULT hr;

    *ppencbi = NULL;

    hr = pncbp->EnumBindingInterfaces( ppencbi );
    Assert(hr == S_OK);
    if(hr != S_OK)
    {
        Log(L"VBoxNetCfgWinGetBindingInterfaceEnum: EnumBindingInterfaces failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/**
 * Enumerates the first binding interface.
 *
 * @param pencbi Binding interface enumerator reference.
 * @param ppncbi address the Binding interface reference to be stored to
 * @return S_OK on success, otherwise an error code
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetFirstBindingInterface(IN IEnumNetCfgBindingInterface *pencbi,
                                      OUT INetCfgBindingInterface **ppncbi)
{
    ULONG   ulCount;
    HRESULT hr;

    *ppncbi = NULL;

    pencbi->Reset();

    hr = pencbi->Next( 1,
                       ppncbi,
                       &ulCount );
    Assert(hr == S_OK || hr == S_FALSE);
    if(hr != S_OK && hr != S_FALSE)
    {
        Log(L"VBoxNetCfgWinGetFirstBindingInterface: Next failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/*@
 * Enumerate the next binding interface.
 * The function behaves just like VBoxNetCfgWinGetFirstBindingInterface if
 * it is called right after VBoxNetCfgWinGetBindingInterfaceEnum.
 *
 * @param pencbi Binding interface enumerator reference.
 * @param ppncbi address the Binding interface reference t be stored to
 * @return S_OK on success, otherwise an error code
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetNextBindingInterface(IN IEnumNetCfgBindingInterface *pencbi,
                                     OUT INetCfgBindingInterface **ppncbi)
{
    ULONG   ulCount;
    HRESULT hr;

    *ppncbi = NULL;

    hr = pencbi->Next( 1,
                       ppncbi,
                       &ulCount );
    Assert(hr == S_OK || hr == S_FALSE);
    if(hr != S_OK && hr != S_FALSE)
    {
        Log(L"VBoxNetCfgWinGetNextBindingInterface: Next failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/**
 * Enumerate the next binding path.
 * The function behaves just like VBoxNetCfgWinGetFirstBindingPath if
 * it is called right after VBoxNetCfgWinGetBindingPathEnum.
 *
 * @param pencbp Binding path enumerator reference.
 * @param ppncbp Address the Binding path reference to be stored to
 * @return S_OK on sucess, otherwise an error code.
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetNextBindingPath(IN IEnumNetCfgBindingPath *pencbp,
                                OUT INetCfgBindingPath **ppncbp)
{
    ULONG   ulCount;
    HRESULT hr;

    *ppncbp = NULL;

    hr = pencbp->Next( 1,
                       ppncbp,
                       &ulCount );
    Assert(hr == S_OK || hr == S_FALSE);
    if(hr != S_OK && hr != S_FALSE)
    {
        Log(L"VBoxNetCfgWinGetNextBindingPath: Next failed, hr (0x%x)\n", hr);
    }

    return hr;
}

/*
 * helper function to get the component by its guid given enum interface
 */
static HRESULT vboxNetCfgWinGetComponentByGuidEnum(IEnumNetCfgComponent *pEnumNcc, IN const GUID * pGuid, OUT INetCfgComponent ** ppNcc)
{
    INetCfgComponent * pNcc;
    GUID NccGuid;
    HRESULT hr;

    hr = VBoxNetCfgWinGetFirstComponent( pEnumNcc, &pNcc );
    Assert(hr == S_OK || hr == S_FALSE);
    while(hr == S_OK)
    {
        ULONG uComponentStatus;
        hr = pNcc->GetDeviceStatus(&uComponentStatus);
        if(hr == S_OK)
        {
            if(uComponentStatus == 0)
            {
                hr = pNcc->GetInstanceGuid(&NccGuid);
                Assert(hr == S_OK);
                if(hr == S_OK)
                {
                    if(NccGuid == *pGuid)
                    {
                        /* found the needed device */
                        *ppNcc = pNcc;
                        break;
                    }
                }
                else
                {
                    Log(L"vboxNetCfgWinGetComponentByGuidEnum: GetInstanceGuid failed, hr (0x%x)\n", hr);
                }
            }
        }

        VBoxNetCfgWinReleaseRef(pNcc);

        hr = VBoxNetCfgWinGetNextComponent( pEnumNcc, &pNcc );
    }

    return hr;
}

/**
 * get the component by its guid
 */
VBOXNETCFGWIN_DECL(HRESULT)
VBoxNetCfgWinGetComponentByGuid(IN INetCfg *pNc,
                                IN const GUID *pguidClass,
                                IN const GUID * pComponentGuid,
                                OUT INetCfgComponent **ppncc)
{
    IEnumNetCfgComponent *pEnumNcc;
    HRESULT hr = VBoxNetCfgWinGetComponentEnum( pNc, pguidClass, &pEnumNcc );
    Assert(hr == S_OK);
    if(hr == S_OK)
    {
        hr = vboxNetCfgWinGetComponentByGuidEnum(pEnumNcc, pComponentGuid, ppncc);
        Assert(hr == S_OK || hr == S_FALSE);
        if(hr == S_FALSE)
        {
            Log(L"VBoxNetCfgWinGetComponentByGuid: component not found \n");
        }
        else if(hr != S_OK)
        {
            Log(L"VBoxNetCfgWinGetComponentByGuid: vboxNetCfgWinGetComponentByGuidEnum failed, hr (0x%x)\n", hr);
        }
        VBoxNetCfgWinReleaseRef(pEnumNcc);
    }
    else
    {
        Log(L"VBoxNetCfgWinGetComponentByGuid: VBoxNetCfgWinGetComponentEnum failed, hr (0x%x)\n", hr);
    }
    return hr;
}

typedef BOOL (*VBOXNETCFGWIN_NETCFGENUM_CALLBACK) (IN INetCfg *pNc, IN INetCfgComponent *pNcc, PVOID pContext);

static HRESULT vboxNetCfgWinEnumNetCfgComponents(IN INetCfg *pNc,
        IN const GUID *pguidClass,
        VBOXNETCFGWIN_NETCFGENUM_CALLBACK callback,
        PVOID pContext)
{
    IEnumNetCfgComponent *pEnumComponent;
    HRESULT hr = pNc->EnumComponents(pguidClass, &pEnumComponent);
    bool bBreak = false;
    Assert(hr == S_OK);
    if(hr == S_OK)
    {
        INetCfgComponent *pNcc;
        hr = pEnumComponent->Reset();
        Assert(hr == S_OK);
        do
        {
            hr = VBoxNetCfgWinGetNextComponent(pEnumComponent, &pNcc);
            Assert(hr == S_OK || hr == S_FALSE);
            if(hr == S_OK)
            {
                /* this is E_NOTIMPL for all components other than NET */
//                ULONG uComponentStatus;
//                hr = pNcc->GetDeviceStatus(&uComponentStatus);
//                if(hr == S_OK)
                {
                    if(!callback(pNc, pNcc, pContext))
                    {
                        bBreak = true;
                    }
                }
                VBoxNetCfgWinReleaseRef(pNcc);
            }
            else
            {
                if(hr ==S_FALSE)
                {
                    hr = S_OK;
                }
                else
                {
                    Log(L"vboxNetCfgWinEnumNetCfgComponents: VBoxNetCfgWinGetNextComponent failed, hr (0x%x)\n", hr);
                }
                break;
            }
        } while(!bBreak);

        VBoxNetCfgWinReleaseRef(pEnumComponent);
    }
    return hr;
}

static VOID DoLogging(LPCWSTR szString, ...)
{
    LOG_ROUTINE pRoutine = (LOG_ROUTINE)(*((void * volatile *)&g_Logger));
    if(pRoutine)
    {
        WCHAR szBuffer[1024] = {0};
        va_list pArgList;
        va_start(pArgList, szString);
        _vsnwprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szString, pArgList);
        va_end(pArgList);

        pRoutine(szBuffer);
    }
}

VBOXNETCFGWIN_DECL(VOID) VBoxNetCfgWinSetLogging(LOG_ROUTINE Log)
{
    *((void * volatile *)&g_Logger) = Log;
}

static BOOL vboxNetCfgWinRemoveAllNetDevicesOfIdCallback(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext)
{
    DWORD winEr;
    HRESULT hr = S_OK;
    SP_REMOVEDEVICE_PARAMS rmdParams;
    rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
    rmdParams.HwProfile = 0;
    if(SetupDiSetClassInstallParams(hDevInfo,pDev,&rmdParams.ClassInstallHeader,sizeof(rmdParams)))
    {
        if(SetupDiSetSelectedDevice (hDevInfo, pDev))
        {
            if(SetupDiCallClassInstaller(DIF_REMOVE,hDevInfo,pDev))
            {
                SP_DEVINSTALL_PARAMS devParams;
                /*
                 * see if device needs reboot
                 */
                devParams.cbSize = sizeof(devParams);
                if(SetupDiGetDeviceInstallParams(hDevInfo,pDev,&devParams))
                {
                    if(devParams.Flags & (DI_NEEDRESTART|DI_NEEDREBOOT))
                    {
                        //
                        // reboot required
                        //
                        hr = S_FALSE;
                        Log(L"vboxNetCfgWinRemoveAllNetDevicesOfIdCallback: !!!REBOOT REQUIRED!!!\n");
                    }
                }
                else
                {
                    //
                    // appears to have succeeded
                    //
                }
            }
            else
            {
                winEr = GetLastError();
                Log(L"vboxNetCfgWinRemoveAllNetDevicesOfIdCallback: SetupDiCallClassInstaller failed winErr(%d)\n", winEr);
                hr = HRESULT_FROM_WIN32(winEr);
            }
        }
        else
        {
            winEr = GetLastError();
            Log(L"vboxNetCfgWinRemoveAllNetDevicesOfIdCallback: SetupDiSetSelectedDevice failed winErr(%d)\n", winEr);
            hr = HRESULT_FROM_WIN32(winEr);
        }
    }
    else
    {
        winEr = GetLastError();
        Log(L"vboxNetCfgWinRemoveAllNetDevicesOfIdCallback: SetupDiSetClassInstallParams failed winErr(%d)\n", winEr);
        hr = HRESULT_FROM_WIN32(winEr);
    }

    return TRUE;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveAllNetDevicesOfId(LPWSTR pPnPId)
{
    return VBoxNetCfgWinEnumNetDevices(pPnPId, vboxNetCfgWinRemoveAllNetDevicesOfIdCallback, NULL);
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnumNetDevices(LPWSTR pPnPId, VBOXNETCFGWIN_NETENUM_CALLBACK callback, PVOID pContext)
{
    DWORD winEr;
    HRESULT hr = S_OK;

    HDEVINFO hDevInfo = SetupDiGetClassDevsExW(
            &GUID_DEVCLASS_NET,
            NULL, /* IN PCTSTR  Enumerator,  OPTIONAL*/
            NULL, /*IN HWND  hwndParent,  OPTIONAL*/
            DIGCF_PRESENT, /*IN DWORD  Flags,*/
            NULL, /*IN HDEVINFO  DeviceInfoSet,  OPTIONAL*/
            NULL, /*IN PCTSTR  MachineName,  OPTIONAL*/
            NULL /*IN PVOID  Reserved*/
        );
    if(hDevInfo != INVALID_HANDLE_VALUE)
    {
        DWORD iDev = 0;
        SP_DEVINFO_DATA Dev;
        PBYTE pBuffer = NULL;
        DWORD cbBuffer = 0;
        DWORD cbRequired = 0;
        BOOL bEnumCompleted;
        size_t cPnPId = wcslen(pPnPId);

        Dev.cbSize = sizeof(Dev);

        for(; bEnumCompleted = SetupDiEnumDeviceInfo(hDevInfo, iDev, &Dev); iDev++)
        {
            if(!SetupDiGetDeviceRegistryPropertyW(hDevInfo,&Dev,
                      SPDRP_HARDWAREID, /* IN DWORD  Property,*/
                      NULL, /*OUT PDWORD  PropertyRegDataType,  OPTIONAL*/
                      pBuffer, /*OUT PBYTE  PropertyBuffer,*/
                      cbBuffer, /* IN DWORD  PropertyBufferSize,*/
                      &cbRequired /*OUT PDWORD  RequiredSize  OPTIONAL*/
                    ))
            {
                winEr = GetLastError();
                if(winEr != ERROR_INSUFFICIENT_BUFFER)
                {
                	Log(L"VBoxNetCfgWinEnumNetDevices: SetupDiGetDeviceRegistryPropertyW (1) failed winErr(%d)\n", winEr);
                    hr = HRESULT_FROM_WIN32(winEr);
                    break;
                }

                if(pBuffer)
                {
                    free(pBuffer);
                }

                pBuffer = (PBYTE)malloc(cbRequired);
                cbBuffer = cbRequired;

                if(!SetupDiGetDeviceRegistryPropertyW(hDevInfo,&Dev,
                          SPDRP_HARDWAREID, /* IN DWORD  Property,*/
                          NULL, /*OUT PDWORD  PropertyRegDataType,  OPTIONAL*/
                          pBuffer, /*OUT PBYTE  PropertyBuffer,*/
                          cbBuffer, /* IN DWORD  PropertyBufferSize,*/
                          &cbRequired /*OUT PDWORD  RequiredSize  OPTIONAL*/
                        ))
                {
                    winEr = GetLastError();
                	Log(L"VBoxNetCfgWinEnumNetDevices: SetupDiGetDeviceRegistryPropertyW (2) failed winErr(%d)\n", winEr);
                    hr = HRESULT_FROM_WIN32(winEr);
                    break;
                }
            }

            PWCHAR pCurId = (PWCHAR)pBuffer;
            size_t cCurId = wcslen(pCurId);
            if(cCurId >= cPnPId)
            {
                pCurId += cCurId - cPnPId;
                if(!wcsnicmp(pCurId, pPnPId, cPnPId))
                {

                    if(!callback(hDevInfo,&Dev,pContext))
                        break;
                }
            }

        }

        if(pBuffer)
            free(pBuffer);

        if(bEnumCompleted)
        {
            winEr = GetLastError();
            hr = winEr == ERROR_NO_MORE_ITEMS ? S_OK : HRESULT_FROM_WIN32(winEr);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    else
    {
    	DWORD winEr = GetLastError();
    	Log(L"VBoxNetCfgWinEnumNetDevices: SetupDiGetClassDevsExW failed winErr(%d)\n", winEr);
    	hr = HRESULT_FROM_WIN32(winEr);
    }

    return hr;
}



/* The original source of the VBoxNetAdp adapter creation/destruction code has the following copyright */
/*
   Copyright 2004 by the Massachusetts Institute of Technology

   All rights reserved.

   Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee is hereby granted,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation, and that the name of the Massachusetts
   Institute of Technology (M.I.T.) not be used in advertising or publicity
   pertaining to distribution of the software without specific, written
   prior permission.

   M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
   ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
   M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
   ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
   SOFTWARE.
*/


#define NETSHELL_LIBRARY _T("netshell.dll")

/**
 *  Use the IShellFolder API to rename the connection.
 */
static HRESULT rename_shellfolder (PCWSTR wGuid, PCWSTR wNewName)
{
    /* This is the GUID for the network connections folder. It is constant.
     * {7007ACC7-3202-11D1-AAD2-00805FC1270E} */
    const GUID CLSID_NetworkConnections = {
        0x7007ACC7, 0x3202, 0x11D1, {
            0xAA, 0xD2, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E
        }
    };

    LPITEMIDLIST pidl = NULL;
    IShellFolder *pShellFolder = NULL;
    HRESULT hr;

    /* Build the display name in the form "::{GUID}". */
    if (wcslen (wGuid) >= MAX_PATH)
        return E_INVALIDARG;
    WCHAR szAdapterGuid[MAX_PATH + 2] = {0};
    swprintf (szAdapterGuid, L"::%ls", wGuid);

    /* Create an instance of the network connections folder. */
    hr = CoCreateInstance (CLSID_NetworkConnections, NULL,
                           CLSCTX_INPROC_SERVER, IID_IShellFolder,
                           reinterpret_cast <LPVOID *> (&pShellFolder));
    /* Parse the display name. */
    if (SUCCEEDED (hr))
    {
        hr = pShellFolder->ParseDisplayName (NULL, NULL, szAdapterGuid, NULL,
                                             &pidl, NULL);
    }
    if (SUCCEEDED (hr))
    {
        hr = pShellFolder->SetNameOf (NULL, pidl, wNewName, SHGDN_NORMAL,
                                      &pidl);
    }

    CoTaskMemFree (pidl);

    if (pShellFolder)
        pShellFolder->Release();

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRenameConnection (LPWSTR pGuid, PCWSTR NewName)
{
    typedef HRESULT (WINAPI *lpHrRenameConnection) (const GUID *, PCWSTR);
    lpHrRenameConnection RenameConnectionFunc = NULL;
    HRESULT status;
//    Guid guid(*pDevInstanceGuid);
//    Bstr bstr(guid.toString());
//    BSTR GuidString = bstr.mutableRaw();
//    WCHAR GuidString[50];
//
//    int length = StringFromGUID2(*pDevInstanceGuid, GuidString, sizeof(GuidString)/sizeof(GuidString[0]));
//    if(!length)
//        return E_FAIL;

//    strString[wcslen(strString) - 1] = L'\0';
//
//    WCHAR * GuidString = strString + 1;

    /* First try the IShellFolder interface, which was unimplemented
     * for the network connections folder before XP. */
    status = rename_shellfolder (pGuid, NewName);
    if (status == E_NOTIMPL)
    {
/** @todo that code doesn't seem to work! */
        /* The IShellFolder interface is not implemented on this platform.
         * Try the (undocumented) HrRenameConnection API in the netshell
         * library. */
        CLSID clsid;
        HINSTANCE hNetShell;
        status = CLSIDFromString ((LPOLESTR) pGuid, &clsid);
        if (FAILED(status))
            return E_FAIL;
        hNetShell = LoadLibrary (NETSHELL_LIBRARY);
        if (hNetShell == NULL)
            return E_FAIL;
        RenameConnectionFunc =
          (lpHrRenameConnection) GetProcAddress (hNetShell,
                                                 "HrRenameConnection");
        if (RenameConnectionFunc == NULL)
        {
            FreeLibrary (hNetShell);
            return E_FAIL;
        }
        status = RenameConnectionFunc (&clsid, NewName);
        FreeLibrary (hNetShell);
    }
    if (FAILED (status))
        return status;

    return S_OK;
}

#define DRIVERHWID _T("sun_VBoxNetAdp")

#define SetErrBreak(strAndArgs) \
    if (1) { \
        hrc = E_FAIL; \
        Log strAndArgs; \
        Assert(0); \
        break; \
    } else do {} while (0)


VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveHostOnlyNetworkInterface (const GUID *pGUID, BSTR *pErrMsg)
{
//    LogFlowFuncEnter();
//    LogFlowFunc (("Network connection GUID = {%RTuuid}\n", aGUID.raw()));

//    AssertReturn (aClient, VERR_INVALID_POINTER);
//    AssertReturn (!aGUID.isEmpty(), VERR_INVALID_PARAMETER);

    HRESULT hrc = S_OK;

    do
    {
        TCHAR lszPnPInstanceId [512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do
        {
            WCHAR strRegLocation [256];
            WCHAR GuidString[50];

            int length = StringFromGUID2(*pGUID, GuidString, sizeof(GuidString)/sizeof(GuidString[0]));
            if(!length)
                SetErrBreak((L"Failed to create a Guid string"));

            swprintf (strRegLocation,
                     L"SYSTEM\\CurrentControlSet\\Control\\Network\\"
                     L"{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s",
                     GuidString);

            LONG status;
            status = RegOpenKeyExW (HKEY_LOCAL_MACHINE, strRegLocation, 0,
                                    KEY_READ, &hkeyNetwork);
            if ((status != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak ((
                        L"Host interface network is not found in registry (%s) [1]",
                    strRegLocation));

            status = RegOpenKeyExW (hkeyNetwork, L"Connection", 0,
                                    KEY_READ, &hkeyConnection);
            if ((status != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak ((
                        L"Host interface network is not found in registry (%s) [2]",
                    strRegLocation));

            DWORD len = sizeof (lszPnPInstanceId);
            DWORD dwKeyType;
            status = RegQueryValueExW (hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE) lszPnPInstanceId, &len);
            if ((status != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak ((
                        L"Host interface network is not found in registry (%s) [3]",
                    strRegLocation));
        }
        while (0);

        if (hkeyConnection)
            RegCloseKey (hkeyConnection);
        if (hkeyNetwork)
            RegCloseKey (hkeyNetwork);

        if (FAILED (hrc))
            break;

        /*
         * Now we are going to enumerate all network devices and
         * wait until we encounter the right device instance ID
         */

        HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;

        do
        {
            BOOL ok;
            DWORD ret = 0;
            GUID netGuid;
            SP_DEVINFO_DATA DeviceInfoData;
            DWORD index = 0;
            BOOL found = FALSE;
            DWORD size = 0;

            /* initialize the structure size */
            DeviceInfoData.cbSize = sizeof (SP_DEVINFO_DATA);

            /* copy the net class GUID */
            memcpy (&netGuid, &GUID_DEVCLASS_NET, sizeof (GUID_DEVCLASS_NET));

            /* return a device info set contains all installed devices of the Net class */
            hDeviceInfo = SetupDiGetClassDevs (&netGuid, NULL, NULL, DIGCF_PRESENT);

            if (hDeviceInfo == INVALID_HANDLE_VALUE)
                SetErrBreak ((L"SetupDiGetClassDevs failed (0x%08X)", GetLastError()));

            /* enumerate the driver info list */
            while (TRUE)
            {
                TCHAR *deviceHwid;

                ok = SetupDiEnumDeviceInfo (hDeviceInfo, index, &DeviceInfoData);

                if (!ok)
                {
                    if (GetLastError() == ERROR_NO_MORE_ITEMS)
                        break;
                    else
                    {
                        index++;
                        continue;
                    }
                }

                /* try to get the hardware ID registry property */
                ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                       &DeviceInfoData,
                                                       SPDRP_HARDWAREID,
                                                       NULL,
                                                       NULL,
                                                       0,
                                                       &size);
                if (!ok)
                {
                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                    {
                        index++;
                        continue;
                    }

                    deviceHwid = (TCHAR *) malloc (size);
                    ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                           &DeviceInfoData,
                                                           SPDRP_HARDWAREID,
                                                           NULL,
                                                           (PBYTE)deviceHwid,
                                                           size,
                                                           NULL);
                    if (!ok)
                    {
                        free (deviceHwid);
                        deviceHwid = NULL;
                        index++;
                        continue;
                    }
                }
                else
                {
                    /* something is wrong.  This shouldn't have worked with a NULL buffer */
                    index++;
                    continue;
                }

                for (TCHAR *t = deviceHwid;
                     t && *t && t < &deviceHwid[size / sizeof(TCHAR)];
                     t += _tcslen (t) + 1)
                {
                    if (!_tcsicmp (DRIVERHWID, t))
                    {
                          /* get the device instance ID */
                          TCHAR devID [MAX_DEVICE_ID_LEN];
                          if (CM_Get_Device_ID(DeviceInfoData.DevInst,
                                               devID, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
                          {
                              /* compare to what we determined before */
                              if (wcscmp(devID, lszPnPInstanceId) == 0)
                              {
                                  found = TRUE;
                                  break;
                              }
                          }
                    }
                }

                if (deviceHwid)
                {
                    free (deviceHwid);
                    deviceHwid = NULL;
                }

                if (found)
                    break;

                index++;
            }

            if (found == FALSE)
                SetErrBreak ((L"Host Interface Network driver not found (0x%08X)",
                              GetLastError()));

            ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
            if (!ok)
                SetErrBreak ((L"SetupDiSetSelectedDevice failed (0x%08X)",
                              GetLastError()));

            ok = SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
            if (!ok)
                SetErrBreak ((L"SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)",
                              GetLastError()));
        }
        while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList (hDeviceInfo);

        if (FAILED (hrc))
            break;
    }
    while (0);

//    LogFlowFunc (("vrc=%Rrc\n", vrc));
//    LogFlowFuncLeave();
    return hrc;
}

static UINT WINAPI vboxNetCfgWinPspFileCallback(
        PVOID Context,
        UINT Notification,
        UINT_PTR Param1,
        UINT_PTR Param2
    )
{
    switch(Notification)
    {
        case SPFILENOTIFY_TARGETNEWER:
        case SPFILENOTIFY_TARGETEXISTS:
            return TRUE;
    }
    return SetupDefaultQueueCallback(Context, Notification, Param1, Param2);
}

static BOOL vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority (IN INetCfg *pNc, IN INetCfgComponent *pNcc, PVOID pContext)
{
    INetCfgComponentBindings *pNccb = NULL;
    IEnumNetCfgBindingPath  *pEnumNccbp;
    GUID *pGuid = (GUID*)pContext;
    HRESULT                  hr;
    bool bFound = false;

    /* Get component's binding. */
    hr = pNcc->QueryInterface( IID_INetCfgComponentBindings,
                               (PVOID *)&pNccb );
    Assert(hr == S_OK);
    if ( hr == S_OK )
    {
        /* Get binding path enumerator reference. */
        hr = pNccb->EnumBindingPaths(EBP_BELOW, &pEnumNccbp);
        Assert(hr == S_OK);
        if(hr == S_OK)
        {
            INetCfgBindingPath *pNccbp;
            hr = pEnumNccbp->Reset();
            Assert(hr == S_OK);
            do
            {
                hr = VBoxNetCfgWinGetNextBindingPath(pEnumNccbp, &pNccbp);
                Assert(hr == S_OK || hr == S_FALSE);
                if(hr == S_OK)
                {
//                    if(pNccbp->IsEnabled() == S_OK)
                     {
                         IEnumNetCfgBindingInterface *pEnumNcbi;
                         hr = VBoxNetCfgWinGetBindingInterfaceEnum(pNccbp, &pEnumNcbi);
                         Assert(hr == S_OK);
                         if ( hr == S_OK )
                         {
                             INetCfgBindingInterface *pNcbi;
                             hr = pEnumNcbi->Reset();
                             Assert(hr == S_OK);
                             do
                             {
                                 hr = VBoxNetCfgWinGetNextBindingInterface(pEnumNcbi, &pNcbi);
                                 Assert(hr == S_OK || hr == S_FALSE);
                                 if(hr == S_OK)
                                 {
                                     INetCfgComponent * pNccBoud;
                                     hr = pNcbi->GetLowerComponent(&pNccBoud);
                                     Assert(hr == S_OK);
                                     if(hr == S_OK)
                                     {
                                         ULONG uComponentStatus;
                                         hr = pNccBoud->GetDeviceStatus(&uComponentStatus);
                                         if(hr == S_OK)
                                         {
//                                             if(uComponentStatus == 0)
                                             {
                                                 GUID guid;
                                                 hr = pNccBoud->GetInstanceGuid(&guid);
                                                 if(guid == *pGuid)
                                                 {
                                                     hr = pNccb->MoveAfter(pNccbp, NULL);
                                                     Assert(hr == S_OK);
                                                     bFound = true;
                                                 }
                                             }
                                         }
                                         VBoxNetCfgWinReleaseRef(pNccBoud);
                                     }
                                     VBoxNetCfgWinReleaseRef(pNcbi);
                                 }
                                 else
                                 {
                                     if(hr == S_FALSE)
                                     {
                                         hr = S_OK;
                                     }
                                     else
                                     {
                                         Log(L"vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority: VBoxNetCfgWinGetNextBindingInterface failed, hr (0x%x)\n", hr);
                                     }
                                     break;
                                 }
                             } while(!bFound);
                             VBoxNetCfgWinReleaseRef(pEnumNcbi);
                         }
                         else
                         {
                             Log(L"vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority: VBoxNetCfgWinGetBindingInterfaceEnum failed, hr (0x%x)\n", hr);
                         }
                     }

                    VBoxNetCfgWinReleaseRef(pNccbp);
                }
                else
                {
                    if(hr = S_FALSE)
                    {
                        hr = S_OK;
                    }
                    else
                    {
                        Log(L"vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority: VBoxNetCfgWinGetNextBindingPath failed, hr (0x%x)\n", hr);
                    }
                    break;
                }
            } while(!bFound);

            VBoxNetCfgWinReleaseRef(pEnumNccbp);
        }
        else
        {
            Log(L"vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority: EnumBindingPaths failed, hr (0x%x)\n", hr);
        }

        VBoxNetCfgWinReleaseRef( pNccb );
    }
    else
    {
        Log(L"vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority: QueryInterface for IID_INetCfgComponentBindings failed, hr (0x%x)\n", hr);
    }

    return true;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinCreateHostOnlyNetworkInterface (LPCWSTR pInfPath, bool bIsInfPathFile, /* <- input params */
        GUID *pGuid, BSTR *lppszName, BSTR *pErrMsg) /* <- output params */
{
    HRESULT hrc = S_OK;

    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    PVOID pQueueCallbackContext = NULL;
    DWORD ret = 0;
    BOOL found = FALSE;
    BOOL registered = FALSE;
    BOOL destroyList = FALSE;
    WCHAR pWCfgGuidString [50];
    WCHAR DevName[256];

    do
    {
        BOOL ok;
        GUID netGuid;
        SP_DRVINFO_DATA DriverInfoData;
        SP_DEVINSTALL_PARAMS  DeviceInstallParams;
        TCHAR className [MAX_PATH];
        DWORD index = 0;
        PSP_DRVINFO_DETAIL_DATA pDriverInfoDetail;
        /* for our purposes, 2k buffer is more
         * than enough to obtain the hardware ID
         * of the VBoxNetAdp driver. */
        DWORD detailBuf [2048];

        HKEY hkey = NULL;
        DWORD cbSize;
        DWORD dwValueType;

        /* initialize the structure size */
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

        /* copy the net class GUID */
        memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof(GUID_DEVCLASS_NET));

        /* create an empty device info set associated with the net class GUID */
        hDeviceInfo = SetupDiCreateDeviceInfoList (&netGuid, NULL);
        if (hDeviceInfo == INVALID_HANDLE_VALUE)
            SetErrBreak ((L"SetupDiCreateDeviceInfoList failed (0x%08X)",
                          GetLastError()));

        /* get the class name from GUID */
        ok = SetupDiClassNameFromGuid (&netGuid, className, MAX_PATH, NULL);
        if (!ok)
            SetErrBreak ((L"SetupDiClassNameFromGuid failed (0x%08X)",
                          GetLastError()));

        /* create a device info element and add the new device instance
         * key to registry */
        ok = SetupDiCreateDeviceInfo (hDeviceInfo, className, &netGuid, NULL, NULL,
                                     DICD_GENERATE_ID, &DeviceInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiCreateDeviceInfo failed (0x%08X)",
                          GetLastError()));

        /* select the newly created device info to be the currently
           selected member */
        ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiSetSelectedDevice failed (0x%08X)",
                          GetLastError()));

        if(pInfPath)
        {
            /* get the device install parameters and disable filecopy */
            DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
            ok = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                &DeviceInstallParams);
            if (ok)
            {
                memset(DeviceInstallParams.DriverPath, 0, sizeof(DeviceInstallParams.DriverPath));
                size_t pathLenght = wcslen(pInfPath) + 1/* null terminator */;
                if(pathLenght < sizeof(DeviceInstallParams.DriverPath)/sizeof(DeviceInstallParams.DriverPath[0]))
                {
                    memcpy(DeviceInstallParams.DriverPath, pInfPath, pathLenght*sizeof(DeviceInstallParams.DriverPath[0]));

                    if(bIsInfPathFile)
                    {
                        DeviceInstallParams.Flags |= DI_ENUMSINGLEINF;
                    }

                    ok = SetupDiSetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                        &DeviceInstallParams);
                    if(!ok)
                    {
                        DWORD winEr = GetLastError();
                        Log(L"SetupDiSetDeviceInstallParams: SetupDiSetDeviceInstallParams failed, winEr (%d)\n", winEr);
                        Assert(0);
                        break;
                    }
                }
                else
                {
                    Log(L"SetupDiSetDeviceInstallParams: inf path is too long\n");
                    Assert(0);
                    break;
                }
            }
            else
            {
                DWORD winEr = GetLastError();
                Assert(0);
                Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: SetupDiGetDeviceInstallParams failed, winEr (%d)\n", winEr);
            }

        }

        /* build a list of class drivers */
        ok = SetupDiBuildDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                        SPDIT_CLASSDRIVER);
        if (!ok)
            SetErrBreak ((L"SetupDiBuildDriverInfoList failed (0x%08X)",
                          GetLastError()));

        destroyList = TRUE;

        /* enumerate the driver info list */
        while (TRUE)
        {
            BOOL ret;

            ret = SetupDiEnumDriverInfo (hDeviceInfo, &DeviceInfoData,
                                         SPDIT_CLASSDRIVER, index, &DriverInfoData);

            /* if the function failed and GetLastError() returned
             * ERROR_NO_MORE_ITEMS, then we have reached the end of the
             * list.  Othewise there was something wrong with this
             * particular driver. */
            if (!ret)
            {
                if(GetLastError() == ERROR_NO_MORE_ITEMS)
                    break;
                else
                {
                    index++;
                    continue;
                }
            }

            pDriverInfoDetail = (PSP_DRVINFO_DETAIL_DATA) detailBuf;
            pDriverInfoDetail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);

            /* if we successfully find the hardware ID and it turns out to
             * be the one for the loopback driver, then we are done. */
            if (SetupDiGetDriverInfoDetail (hDeviceInfo,
                                            &DeviceInfoData,
                                            &DriverInfoData,
                                            pDriverInfoDetail,
                                            sizeof (detailBuf),
                                            NULL))
            {
                TCHAR * t;

                /* pDriverInfoDetail->HardwareID is a MULTISZ string.  Go through the
                 * whole list and see if there is a match somewhere. */
                t = pDriverInfoDetail->HardwareID;
                while (t && *t && t < (TCHAR *) &detailBuf [RT_ELEMENTS(detailBuf)])
                {
                    if (!_tcsicmp(t, DRIVERHWID))
                        break;

                    t += _tcslen(t) + 1;
                }

                if (t && *t && t < (TCHAR *) &detailBuf [RT_ELEMENTS(detailBuf)])
                {
                    found = TRUE;
                    break;
                }
            }

            index ++;
        }

        if (!found)
            SetErrBreak ((L"Could not find Host Interface Networking driver! "
                              L"Please reinstall"));

        /* set the loopback driver to be the currently selected */
        ok = SetupDiSetSelectedDriver (hDeviceInfo, &DeviceInfoData,
                                       &DriverInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiSetSelectedDriver failed (0x%08X)",
                          GetLastError()));

        /* register the phantom device to prepare for install */
        ok = SetupDiCallClassInstaller (DIF_REGISTERDEVICE, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
        {
            DWORD err = GetLastError();
            SetErrBreak ((L"SetupDiCallClassInstaller failed (0x%08X)",
                          err));
        }

        /* registered, but remove if errors occur in the following code */
        registered = TRUE;

        /* ask the installer if we can install the device */
        ok = SetupDiCallClassInstaller (DIF_ALLOW_INSTALL, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
        {
            if (GetLastError() != ERROR_DI_DO_DEFAULT)
                SetErrBreak ((L"SetupDiCallClassInstaller (DIF_ALLOW_INSTALL) failed (0x%08X)",
                              GetLastError()));
            /* that's fine */
        }

        /* get the device install parameters and disable filecopy */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        ok = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                            &DeviceInstallParams);
        if (ok)
        {
            pQueueCallbackContext = SetupInitDefaultQueueCallback(NULL);
            if(pQueueCallbackContext)
            {
                DeviceInstallParams.InstallMsgHandlerContext = pQueueCallbackContext;
                DeviceInstallParams.InstallMsgHandler = (PSP_FILE_CALLBACK)vboxNetCfgWinPspFileCallback;
                ok = SetupDiSetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                    &DeviceInstallParams);
                if(!ok)
                {
                    DWORD winEr = GetLastError();
                    Assert(0);
                    Log(L"SetupDiSetDeviceInstallParams: SetupDiSetDeviceInstallParams failed, winEr (%d)\n", winEr);
                }
                Assert(ok);
            }
            else
            {
                DWORD winEr = GetLastError();
                Assert(0);
                Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: SetupInitDefaultQueueCallback failed, winEr (%d)\n", winEr);
            }
        }
        else
        {
            DWORD winEr = GetLastError();
            Assert(0);
            Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: SetupDiGetDeviceInstallParams failed, winEr (%d)\n", winEr);
        }

        /* install the files first */
        ok = SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES) failed (0x%08X)",
                          GetLastError()));

        /* get the device install parameters and disable filecopy */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        ok = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                            &DeviceInstallParams);
        if (ok)
        {
            DeviceInstallParams.Flags |= DI_NOFILECOPY;
            ok = SetupDiSetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                &DeviceInstallParams);
            if (!ok)
                SetErrBreak ((L"SetupDiSetDeviceInstallParams failed (0x%08X)",
                              GetLastError()));
        }

        /*
         * Register any device-specific co-installers for this device,
         */

        ok = SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS) failed (0x%08X)",
                          GetLastError()));

        /*
         * install any  installer-specified interfaces.
         * and then do the real install
         */
        ok = SetupDiCallClassInstaller (DIF_INSTALLINTERFACES,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiCallClassInstaller (DIF_INSTALLINTERFACES) failed (0x%08X)",
                          GetLastError()));

        ok = SetupDiCallClassInstaller (DIF_INSTALLDEVICE,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak ((L"SetupDiCallClassInstaller (DIF_INSTALLDEVICE) failed (0x%08X)",
                          GetLastError()));

        /* Figure out NetCfgInstanceId */
        hkey = SetupDiOpenDevRegKey (hDeviceInfo,
                                     &DeviceInfoData,
                                     DICS_FLAG_GLOBAL,
                                     0,
                                     DIREG_DRV,
                                     KEY_READ);
        if (hkey == INVALID_HANDLE_VALUE)
            SetErrBreak ((L"SetupDiOpenDevRegKey failed (0x%08X)",
                          GetLastError()));

        cbSize = sizeof (pWCfgGuidString);
        DWORD ret;
        ret = RegQueryValueExW (hkey, L"NetCfgInstanceId", NULL,
                               &dwValueType, (LPBYTE) pWCfgGuidString, &cbSize);

        RegCloseKey (hkey);

        if(!SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &DeviceInfoData,
                SPDRP_FRIENDLYNAME , /* IN DWORD  Property,*/
                  NULL, /*OUT PDWORD  PropertyRegDataType,  OPTIONAL*/
                  (PBYTE)DevName, /*OUT PBYTE  PropertyBuffer,*/
                  sizeof(DevName), /* IN DWORD  PropertyBufferSize,*/
                  NULL /*OUT PDWORD  RequiredSize  OPTIONAL*/
                ))
        {
            int err = GetLastError();
            if(err != ERROR_INVALID_DATA)
            {
                SetErrBreak ((L"SetupDiGetDeviceRegistryProperty failed (0x%08X)",
                              err));
            }

            if(!SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &DeviceInfoData,
                              SPDRP_DEVICEDESC  , /* IN DWORD  Property,*/
                              NULL, /*OUT PDWORD  PropertyRegDataType,  OPTIONAL*/
                              (PBYTE)DevName, /*OUT PBYTE  PropertyBuffer,*/
                              sizeof(DevName), /* IN DWORD  PropertyBufferSize,*/
                              NULL /*OUT PDWORD  RequiredSize  OPTIONAL*/
                            ))
            {
                err = GetLastError();
                SetErrBreak ((L"SetupDiGetDeviceRegistryProperty failed (0x%08X)",
                                              err));
            }
        }
    }
    while (0);

    /*
     * cleanup
     */
    if(pQueueCallbackContext)
    {
        SetupTermDefaultQueueCallback(pQueueCallbackContext);
    }

    if (hDeviceInfo != INVALID_HANDLE_VALUE)
    {
        /* an error has occurred, but the device is registered, we must remove it */
        if (ret != 0 && registered)
            SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);

        found = SetupDiDeleteDeviceInfo (hDeviceInfo, &DeviceInfoData);

        /* destroy the driver info list */
        if (destroyList)
            SetupDiDestroyDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                          SPDIT_CLASSDRIVER);
        /* clean up the device info set */
        SetupDiDestroyDeviceInfoList (hDeviceInfo);
    }

    /* return the network connection GUID on success */
    if (SUCCEEDED(hrc))
    {
//        Bstr str(DevName);
//        str.detachTo(pName);
        WCHAR ConnectoinName[128];
        ULONG cbName = sizeof(ConnectoinName);

        HRESULT hr = VBoxNetCfgWinGenHostonlyConnectionName (DevName, ConnectoinName, &cbName);
        if(hr == S_OK)
        {
            hr = VBoxNetCfgWinRenameConnection (pWCfgGuidString, ConnectoinName);
        }

        if(lppszName)
        {
            *lppszName = ::SysAllocString ((const OLECHAR *) DevName);
            if ( !*lppszName  )
            {
                Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: SysAllocString failed\n");
                Assert(0);
                hrc = HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
            }
        }

        if(pGuid)
        {
            hrc = CLSIDFromString(pWCfgGuidString, (LPCLSID) pGuid);
            if(hrc != S_OK)
            {
                Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: CLSIDFromString failed, hrc (0x%x)\n", hrc);
                Assert(0);
            }
        }

        INetCfg *pNc;
        LPWSTR               lpszApp = NULL;



        hr = VBoxNetCfgWinQueryINetCfgEx( TRUE,
                           L"VirtualBox Host-Only Creation",
                           30000, /* on Vista we often get 6to4svc.dll holding the lock, wait for 30 sec,  */
                           &pNc,  /* TODO: special handling for 6to4svc.dll ???, i.e. several retrieves */
                           &lpszApp );
        Assert(hr == S_OK);
        if(hr == S_OK)
        {
            hr = vboxNetCfgWinEnumNetCfgComponents(pNc,
                    &GUID_DEVCLASS_NETSERVICE,
                    vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority,
                    pGuid);
            Assert(hr == S_OK);

            hr = vboxNetCfgWinEnumNetCfgComponents(pNc,
                    &GUID_DEVCLASS_NETTRANS,
                    vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority,
                    pGuid);
            Assert(hr == S_OK);

            hr = vboxNetCfgWinEnumNetCfgComponents(pNc,
                    &GUID_DEVCLASS_NETCLIENT,
                    vboxNetCfgWinAdjustHostOnlyNetworkInterfacePriority,
                    pGuid);
            Assert(hr == S_OK);

            if(hr == S_OK)
            {
                hr = pNc->Apply();
                Assert(hr == S_OK);
            }

            VBoxNetCfgWinReleaseINetCfg(pNc, TRUE);
        }
        else if(hr == NETCFG_E_NO_WRITE_LOCK && lpszApp)
        {
            Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: app %s is holding the lock, failed\n", lpszApp);
            CoTaskMemFree(lpszApp);
        }
        else
        {
            Log(L"VBoxNetCfgWinCreateHostOnlyNetworkInterface: VBoxNetCfgWinQueryINetCfgEx failed, hr 0x%x\n", hr);
        }
    }

    return hrc;
}

#undef SetErrBreak

#define VBOX_CONNECTION_NAME L"VirtualBox Host-Only Network"
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostonlyConnectionName (PCWSTR DevName, WCHAR *pBuf, PULONG pcbBuf)
{
    const WCHAR * pSuffix = wcsrchr( DevName, L'#' );
    ULONG cbSize = sizeof(VBOX_CONNECTION_NAME);
    ULONG cbSufSize = 0;

    if(pSuffix)
    {
        cbSize += (ULONG)wcslen(pSuffix) * 2;
        cbSize += 2; /* for space */
    }

    if(*pcbBuf < cbSize)
    {
        *pcbBuf = cbSize;
        return E_FAIL;
    }

    wcscpy(pBuf, VBOX_CONNECTION_NAME);
    if(pSuffix)
    {
        wcscat(pBuf, L" ");
        wcscat(pBuf, pSuffix);
    }

    return S_OK;
}

/* network settings config */
/**
 *  Strong referencing operators. Used as a second argument to ComPtr<>/ComObjPtr<>.
 */
template <class C>
class ComStrongRef
{
protected:

    static void addref (C *p) { p->AddRef(); }
    static void release (C *p) { p->Release(); }
};

/**
 *  Weak referencing operators. Used as a second argument to ComPtr<>/ComObjPtr<>.
 */
template <class C>
class ComWeakRef
{
protected:

    static void addref  (C * /* p */) {}
    static void release (C * /* p */) {}
};

/**
 *  Base template for smart COM pointers. Not intended to be used directly.
 */
template <class C, template <class> class RefOps = ComStrongRef>
class ComPtrBase : protected RefOps <C>
{
public:

    /* special template to disable AddRef()/Release() */
    template <class I>
    class NoAddRefRelease : public I
    {
        private:
#if !defined (VBOX_WITH_XPCOM)
            STDMETHOD_(ULONG, AddRef)() = 0;
            STDMETHOD_(ULONG, Release)() = 0;
#else /* !defined (VBOX_WITH_XPCOM) */
            NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
            NS_IMETHOD_(nsrefcnt) Release(void) = 0;
#endif /* !defined (VBOX_WITH_XPCOM) */
    };

protected:

    ComPtrBase () : p (NULL) {}
    ComPtrBase (const ComPtrBase &that) : p (that.p) { addref(); }
    ComPtrBase (C *that_p) : p (that_p) { addref(); }

    ~ComPtrBase() { release(); }

    ComPtrBase &operator= (const ComPtrBase &that)
    {
        safe_assign (that.p);
        return *this;
    }

    ComPtrBase &operator= (C *that_p)
    {
        safe_assign (that_p);
        return *this;
    }

public:

    void setNull()
    {
        release();
        p = NULL;
    }

    bool isNull() const
    {
        return (p == NULL);
    }

    bool operator! () const { return isNull(); }

    bool operator< (C* that_p) const { return p < that_p; }
    bool operator== (C* that_p) const { return p == that_p; }

    template <class I>
    bool equalsTo (I *aThat) const
    {
        return ComPtrEquals (p, aThat);
    }

    template <class OC>
    bool equalsTo (const ComPtrBase <OC> &oc) const
    {
        return equalsTo ((OC *) oc);
    }

    /** Intended to pass instances as in parameters to interface methods */
    operator C* () const { return p; }

    /**
     *  Dereferences the instance (redirects the -> operator to the managed
     *  pointer).
     */
    NoAddRefRelease <C> *operator-> () const
    {
        AssertMsg (p, ("Managed pointer must not be null\n"));
        return (NoAddRefRelease <C> *) p;
    }

    template <class I>
    HRESULT queryInterfaceTo (I **pp) const
    {
        if (pp)
        {
            if (p)
            {
                return p->QueryInterface (COM_IIDOF (I), (void **) pp);
            }
            else
            {
                *pp = NULL;
                return S_OK;
            }
        }

        return E_INVALIDARG;
    }

    /** Intended to pass instances as out parameters to interface methods */
    C **asOutParam()
    {
        setNull();
        return &p;
    }

private:

    void addref()
    {
        if (p)
            RefOps <C>::addref (p);
    }

    void release()
    {
        if (p)
            RefOps <C>::release (p);
    }

    void safe_assign (C *that_p)
    {
        /* be aware of self-assignment */
        if (that_p)
            RefOps <C>::addref (that_p);
        release();
        p = that_p;
    }

    C *p;
};

/**
 *  Smart COM pointer wrapper that automatically manages refcounting of
 *  interface pointers.
 *
 *  @param I    COM interface class
 */
template <class I, template <class> class RefOps = ComStrongRef>
class ComPtr : public ComPtrBase <I, RefOps>
{
    typedef ComPtrBase <I, RefOps> Base;

public:

    ComPtr () : Base() {}
    ComPtr (const ComPtr &that) : Base (that) {}
    ComPtr &operator= (const ComPtr &that)
    {
        Base::operator= (that);
        return *this;
    }

    template <class OI>
    ComPtr (OI *that_p) : Base () { operator= (that_p); }

    /* specialization for I */
    ComPtr (I *that_p) : Base (that_p) {}

    template <class OC>
    ComPtr (const ComPtr <OC, RefOps> &oc) : Base () { operator= ((OC *) oc); }

    template <class OI>
    ComPtr &operator= (OI *that_p)
    {
        if (that_p)
            that_p->QueryInterface (COM_IIDOF (I), (void **) Base::asOutParam());
        else
            Base::setNull();
        return *this;
    }

    /* specialization for I */
    ComPtr &operator=(I *that_p)
    {
        Base::operator= (that_p);
        return *this;
    }

    template <class OC>
    ComPtr &operator= (const ComPtr <OC, RefOps> &oc)
    {
        return operator= ((OC *) oc);
    }
};

static HRESULT netIfWinFindAdapterClassById(IWbemServices * pSvc, const GUID * pGuid, IWbemClassObject **pAdapterConfig)
{
    HRESULT hres;
    WCHAR aQueryString[256];
    WCHAR GuidString[50];

    int length = StringFromGUID2(*pGuid, GuidString, sizeof(GuidString)/sizeof(GuidString[0]));
    if(length)
    {
        swprintf(aQueryString, L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE SettingID = \"%s\"", GuidString);
        // Step 6: --------------------------------------------------
        // Use the IWbemServices pointer to make requests of WMI ----

        IEnumWbemClassObject* pEnumerator = NULL;
        hres = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(aQueryString),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);
        if(SUCCEEDED(hres))
        {
            // Step 7: -------------------------------------------------
            // Get the data from the query in step 6 -------------------

            IWbemClassObject *pclsObj;
            ULONG uReturn = 0;

            if (pEnumerator)
            {
                HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
                    &pclsObj, &uReturn);

                if(SUCCEEDED(hres))
                {
                    if(uReturn)
                    {
                        pEnumerator->Release();
                        *pAdapterConfig = pclsObj;
                        hres = S_OK;
                        return hres;
                    }
                    else
                    {
                        hres = S_FALSE;
                    }
                }

                pEnumerator->Release();
            }
        }
        else
        {
            Log(L"Query for operating system name failed. Error code = 0x%x\n", hres);
        }
    }
    else
    {
        DWORD winEr = GetLastError();
        Log(L"Failed to create guid string from guid, winEr (%d)\n", winEr);
        hres = HRESULT_FROM_WIN32( winEr );
    }

    return hres;
}

static HRESULT netIfWinIsHostOnly(IWbemClassObject * pAdapterConfig, BOOL * pbIsHostOnly)
{
    VARIANT vtServiceName;
    BOOL bIsHostOnly = FALSE;
    VariantInit(&vtServiceName);

    HRESULT hr = pAdapterConfig->Get(L"ServiceName", 0, &vtServiceName, 0, 0);
    if(SUCCEEDED(hr))
    {
        *pbIsHostOnly = (bstr_t(vtServiceName.bstrVal) == bstr_t("VBoxNetAdp"));

        VariantClear(&vtServiceName);
    }

    return hr;
}

static HRESULT netIfWinGetIpSettings(IWbemClassObject * pAdapterConfig, ULONG *pIpv4, ULONG *pMaskv4)
{
    VARIANT vtIp;
    HRESULT hr;
    VariantInit(&vtIp);

    *pIpv4 = 0;
    *pMaskv4 = 0;

    hr = pAdapterConfig->Get(L"IPAddress", 0, &vtIp, 0, 0);
    if(SUCCEEDED(hr))
    {
        if(vtIp.vt == (VT_ARRAY | VT_BSTR))
        {
            VARIANT vtMask;
            VariantInit(&vtMask);
            hr = pAdapterConfig->Get(L"IPSubnet", 0, &vtMask, 0, 0);
            if(SUCCEEDED(hr))
            {
                if(vtMask.vt == (VT_ARRAY | VT_BSTR))
                {
                    SAFEARRAY * pIpArray = vtIp.parray;
                    SAFEARRAY * pMaskArray = vtMask.parray;
                    if(pIpArray && pMaskArray)
                    {
                        BSTR pCurIp;
                        BSTR pCurMask;
                        for(long index = 0;
                            SafeArrayGetElement(pIpArray, &index, (PVOID)&pCurIp) == S_OK
                            && SafeArrayGetElement(pMaskArray, &index, (PVOID)&pCurMask) == S_OK;
                            index++)
                        {
                            _bstr_t ip(pCurIp);

                            ULONG Ipv4 = inet_addr((char*)(ip));
                            if(Ipv4 != INADDR_NONE)
                            {
                                *pIpv4 = Ipv4;
                                _bstr_t mask(pCurMask);
                                *pMaskv4 = inet_addr((char*)(mask));
                                break;
                            }
                        }
                    }
                }
                else
                {
                    *pIpv4 = 0;
                    *pMaskv4 = 0;
                }

                VariantClear(&vtMask);
            }
        }
        else
        {
            *pIpv4 = 0;
            *pMaskv4 = 0;
        }

        VariantClear(&vtIp);
    }

    return hr;
}


static HRESULT netIfWinHasIpSettings(IWbemClassObject * pAdapterConfig, SAFEARRAY * pCheckIp, SAFEARRAY * pCheckMask, bool *pFound)
{
    VARIANT vtIp;
    HRESULT hr;
    VariantInit(&vtIp);

    *pFound = false;

    hr = pAdapterConfig->Get(L"IPAddress", 0, &vtIp, 0, 0);
    if(SUCCEEDED(hr))
    {
        VARIANT vtMask;
        VariantInit(&vtMask);
        hr = pAdapterConfig->Get(L"IPSubnet", 0, &vtMask, 0, 0);
        if(SUCCEEDED(hr))
        {
            SAFEARRAY * pIpArray = vtIp.parray;
            SAFEARRAY * pMaskArray = vtMask.parray;
            if(pIpArray && pMaskArray)
            {
                BSTR pIp, pMask;
                for(long k = 0;
                    SafeArrayGetElement(pCheckIp, &k, (PVOID)&pIp) == S_OK
                    && SafeArrayGetElement(pCheckMask, &k, (PVOID)&pMask) == S_OK;
                    k++)
                {
                    BSTR pCurIp;
                    BSTR pCurMask;
                    for(long index = 0;
                        SafeArrayGetElement(pIpArray, &index, (PVOID)&pCurIp) == S_OK
                        && SafeArrayGetElement(pMaskArray, &index, (PVOID)&pCurMask) == S_OK;
                        index++)
                    {
                        if(!wcsicmp(pCurIp, pIp))
                        {
                            if(!wcsicmp(pCurMask, pMask))
                            {
                                *pFound = true;
                            }
                            break;
                        }
                    }
                }
            }


            VariantClear(&vtMask);
        }

        VariantClear(&vtIp);
    }

    return hr;
}

static HRESULT netIfWinWaitIpSettings(IWbemServices *pSvc, const GUID * pGuid, SAFEARRAY * pCheckIp, SAFEARRAY * pCheckMask, ULONG sec2Wait, bool *pFound)
{
    /* on Vista we need to wait for the address to get applied */
    /* wait for the address to appear in the list */
    HRESULT hr = S_OK;
    ULONG i;
    *pFound = false;
    ComPtr <IWbemClassObject> pAdapterConfig;
    for(i = 0;
            (hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam())) == S_OK
            && (hr = netIfWinHasIpSettings(pAdapterConfig, pCheckIp, pCheckMask, pFound)) == S_OK
            && !(*pFound)
            && i < sec2Wait/6;
            i++)
    {
        Sleep(6000);
    }

    return hr;
}

static HRESULT netIfWinCreateIWbemServices(IWbemServices ** ppSvc)
{
    HRESULT hres;

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------

    IWbemLocator *pLoc = NULL;

    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *) &pLoc);
    if(SUCCEEDED(hres))
    {
        // Step 4: -----------------------------------------------------
        // Connect to WMI through the IWbemLocator::ConnectServer method

        IWbemServices *pSvc = NULL;

        // Connect to the root\cimv2 namespace with
        // the current user and obtain pointer pSvc
        // to make IWbemServices calls.
        hres = pLoc->ConnectServer(
             _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
             NULL,                    // User name. NULL = current user
             NULL,                    // User password. NULL = current
             0,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             0,                       // Authority (e.g. Kerberos)
             0,                       // Context object
             &pSvc                    // pointer to IWbemServices proxy
             );
        if(SUCCEEDED(hres))
        {
            Log(L"Connected to ROOT\\CIMV2 WMI namespace\n");

            // Step 5: --------------------------------------------------
            // Set security levels on the proxy -------------------------

            hres = CoSetProxyBlanket(
               pSvc,                        // Indicates the proxy to set
               RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
               RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
               NULL,                        // Server principal name
               RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
               RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
               NULL,                        // client identity
               EOAC_NONE                    // proxy capabilities
            );
            if(SUCCEEDED(hres))
            {
                *ppSvc = pSvc;
                /* do not need it any more */
                pLoc->Release();
                return hres;
            }
            else
            {
                Log(L"Could not set proxy blanket. Error code = 0x%x\n", hres);
            }

            pSvc->Release();
        }
        else
        {
            Log(L"Could not connect. Error code = 0x%x\n", hres);
        }

        pLoc->Release();
    }
    else
    {
        Log(L"Failed to create IWbemLocator object. Err code = 0x%x\n", hres);
//        CoUninitialize();
    }

    return hres;
}

static HRESULT netIfWinAdapterConfigPath(IWbemClassObject *pObj, BSTR * pStr)
{
    VARIANT index;

    // Get the value of the key property
    HRESULT hr = pObj->Get(L"Index", 0, &index, 0, 0);
    if(SUCCEEDED(hr))
    {
        WCHAR strIndex[8];
        swprintf(strIndex, L"%u", index.uintVal);
        *pStr = (bstr_t(L"Win32_NetworkAdapterConfiguration.Index='") + strIndex + "'").copy();
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }
    return hr;
}

static HRESULT netIfExecMethod(IWbemServices * pSvc, IWbemClassObject *pClass, BSTR ObjPath,
        BSTR MethodName, LPWSTR *pArgNames, LPVARIANT *pArgs, UINT cArgs,
        IWbemClassObject** ppOutParams
        )
{
    HRESULT hres = S_OK;
    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    ComPtr<IWbemClassObject> pInParamsDefinition;
    ComPtr<IWbemClassObject> pClassInstance;

    if(cArgs)
    {
        hres = pClass->GetMethod(MethodName, 0,
            pInParamsDefinition.asOutParam(), NULL);
        if(SUCCEEDED(hres))
        {
            hres = pInParamsDefinition->SpawnInstance(0, pClassInstance.asOutParam());

            if(SUCCEEDED(hres))
            {
                for(UINT i = 0; i < cArgs; i++)
                {
                    // Store the value for the in parameters
                    hres = pClassInstance->Put(pArgNames[i], 0,
                        pArgs[i], 0);
                    if(FAILED(hres))
                    {
                        break;
                    }
                }
            }
        }
    }

    if(SUCCEEDED(hres))
    {
        IWbemClassObject* pOutParams = NULL;
        hres = pSvc->ExecMethod(ObjPath, MethodName, 0,
                        NULL, pClassInstance, &pOutParams, NULL);
        if(SUCCEEDED(hres))
        {
            *ppOutParams = pOutParams;
        }
    }

    return hres;
}

static HRESULT netIfWinCreateIpArray(SAFEARRAY **ppArray, in_addr* aIp, UINT cIp)
{
    HRESULT hr;
    SAFEARRAY * pIpArray = SafeArrayCreateVector(VT_BSTR, 0, cIp);
    if(pIpArray)
    {
        for(UINT i = 0; i < cIp; i++)
        {
            char* addr = inet_ntoa(aIp[i]);
            BSTR val = bstr_t(addr).copy();
            long aIndex[1];
            aIndex[0] = i;
            hr = SafeArrayPutElement(pIpArray, aIndex, val);
            if(FAILED(hr))
            {
                SysFreeString(val);
                SafeArrayDestroy(pIpArray);
                break;
            }
        }

        if(SUCCEEDED(hr))
        {
            *ppArray = pIpArray;
        }
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}

static HRESULT netIfWinCreateIpArrayV4V6(SAFEARRAY **ppArray, BSTR Ip)
{
    HRESULT hr;
    SAFEARRAY * pIpArray = SafeArrayCreateVector(VT_BSTR, 0, 1);
    if(pIpArray)
    {
        BSTR val = bstr_t(Ip, false).copy();
        long aIndex[1];
        aIndex[0] = 0;
        hr = SafeArrayPutElement(pIpArray, aIndex, val);
        if(FAILED(hr))
        {
            SysFreeString(val);
            SafeArrayDestroy(pIpArray);
        }

        if(SUCCEEDED(hr))
        {
            *ppArray = pIpArray;
        }
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}


static HRESULT netIfWinCreateIpArrayVariantV4(VARIANT * pIpAddresses, in_addr* aIp, UINT cIp)
{
    HRESULT hr;
    VariantInit(pIpAddresses);
    pIpAddresses->vt = VT_ARRAY | VT_BSTR;
    SAFEARRAY *pIpArray;
    hr = netIfWinCreateIpArray(&pIpArray, aIp, cIp);
    if(SUCCEEDED(hr))
    {
        pIpAddresses->parray = pIpArray;
    }
    return hr;
}

static HRESULT netIfWinCreateIpArrayVariantV4V6(VARIANT * pIpAddresses, BSTR Ip)
{
    HRESULT hr;
    VariantInit(pIpAddresses);
    pIpAddresses->vt = VT_ARRAY | VT_BSTR;
    SAFEARRAY *pIpArray;
    hr = netIfWinCreateIpArrayV4V6(&pIpArray, Ip);
    if(SUCCEEDED(hr))
    {
        pIpAddresses->parray = pIpArray;
    }
    return hr;
}

static HRESULT netIfWinEnableStatic(IWbemServices * pSvc, const GUID * pGuid, BSTR ObjPath, VARIANT * pIp, VARIANT * pMask)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            LPWSTR argNames[] = {L"IPAddress", L"SubnetMask"};
            LPVARIANT args[] = {pIp, pMask};
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"EnableStatic"), argNames, args, 2, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if(SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch(winEr)
                    {
                    case 0:
                        {
                            hr = S_OK;
//                            bool bFound;
//                            HRESULT tmpHr = netIfWinWaitIpSettings(pSvc, pGuid, pIp->parray, pMask->parray, 180, &bFound);
                        }
                        break;
                    default:
                        hr = HRESULT_FROM_WIN32( winEr );
                        break;
                    }
                }
            }
        }
        SysFreeString(ClassName);
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}


static HRESULT netIfWinEnableStaticV4(IWbemServices * pSvc, const GUID * pGuid, BSTR ObjPath, in_addr* aIp, in_addr * aMask, UINT cIp)
{
    VARIANT ipAddresses;
    HRESULT hr = netIfWinCreateIpArrayVariantV4(&ipAddresses, aIp, cIp);
    if(SUCCEEDED(hr))
    {
        VARIANT ipMasks;
        hr = netIfWinCreateIpArrayVariantV4(&ipMasks, aMask, cIp);
        if(SUCCEEDED(hr))
        {
            hr = netIfWinEnableStatic(pSvc, pGuid, ObjPath, &ipAddresses, &ipMasks);
            VariantClear(&ipMasks);
        }
        VariantClear(&ipAddresses);
    }
    return hr;
}

static HRESULT netIfWinEnableStaticV4V6(IWbemServices * pSvc, const GUID * pGuid, BSTR ObjPath, BSTR Ip, BSTR Mask)
{
    VARIANT ipAddresses;
    HRESULT hr = netIfWinCreateIpArrayVariantV4V6(&ipAddresses, Ip);
    if(SUCCEEDED(hr))
    {
        VARIANT ipMasks;
        hr = netIfWinCreateIpArrayVariantV4V6(&ipMasks, Mask);
        if(SUCCEEDED(hr))
        {
            hr = netIfWinEnableStatic(pSvc, pGuid, ObjPath, &ipAddresses, &ipMasks);
            VariantClear(&ipMasks);
        }
        VariantClear(&ipAddresses);
    }
    return hr;
}

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGateways(IWbemServices * pSvc, BSTR ObjPath, VARIANT * pGw)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            LPWSTR argNames[] = {L"DefaultIPGateway"};
            LPVARIANT args[] = {pGw};
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"SetGateways"), argNames, args, 1, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if(SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch(winEr)
                    {
                    case 0:
                        hr = S_OK;
                        break;
                    default:
                        hr = HRESULT_FROM_WIN32( winEr );
                        break;
                    }
                }
            }        }
        SysFreeString(ClassName);
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGatewaysV4(IWbemServices * pSvc, BSTR ObjPath, in_addr* aGw, UINT cGw)
{
    VARIANT gwais;
    HRESULT hr = netIfWinCreateIpArrayVariantV4(&gwais, aGw, cGw);
    if(SUCCEEDED(hr))
    {
        netIfWinSetGateways(pSvc, ObjPath, &gwais);
        VariantClear(&gwais);
    }
    return hr;
}

/* win API allows to set gw metrics as well, we are not setting them */
static HRESULT netIfWinSetGatewaysV4V6(IWbemServices * pSvc, BSTR ObjPath, BSTR Gw)
{
    VARIANT vGw;
    HRESULT hr = netIfWinCreateIpArrayVariantV4V6(&vGw, Gw);
    if(SUCCEEDED(hr))
    {
        netIfWinSetGateways(pSvc, ObjPath, &vGw);
        VariantClear(&vGw);
    }
    return hr;
}

static HRESULT netIfWinEnableDHCP(IWbemServices * pSvc, BSTR ObjPath)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"EnableDHCP"), NULL, NULL, 0, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if(SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    switch(winEr)
                    {
                    case 0:
                        hr = S_OK;
                        break;
                    default:
                        hr = HRESULT_FROM_WIN32( winEr );
                        break;
                    }
                }
            }
        }
        SysFreeString(ClassName);
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}

static HRESULT netIfWinDhcpRediscover(IWbemServices * pSvc, BSTR ObjPath)
{
    ComPtr<IWbemClassObject> pClass;
    BSTR ClassName = SysAllocString(L"Win32_NetworkAdapterConfiguration");
    HRESULT hr;
    if(ClassName)
    {
        hr = pSvc->GetObject(ClassName, 0, NULL, pClass.asOutParam(), NULL);
        if(SUCCEEDED(hr))
        {
            ComPtr<IWbemClassObject> pOutParams;

            hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                bstr_t(L"ReleaseDHCPLease"), NULL, NULL, 0, pOutParams.asOutParam());
            if(SUCCEEDED(hr))
            {
                VARIANT varReturnValue;
                hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                    &varReturnValue, NULL, 0);
                Assert(SUCCEEDED(hr));
                if(SUCCEEDED(hr))
                {
//                    Assert(varReturnValue.vt == VT_UINT);
                    int winEr = varReturnValue.uintVal;
                    if(winEr == 0)
                    {
                        hr = netIfExecMethod(pSvc, pClass, ObjPath,
                                            bstr_t(L"RenewDHCPLease"), NULL, NULL, 0, pOutParams.asOutParam());
                        if(SUCCEEDED(hr))
                        {
                            VARIANT varReturnValue;
                            hr = pOutParams->Get(bstr_t(L"ReturnValue"), 0,
                                &varReturnValue, NULL, 0);
                            Assert(SUCCEEDED(hr));
                            if(SUCCEEDED(hr))
                            {
            //                    Assert(varReturnValue.vt == VT_UINT);
                                int winEr = varReturnValue.uintVal;
                                if(winEr == 0)
                                {
                                    hr = S_OK;
                                }
                                else
                                {
                                    hr = HRESULT_FROM_WIN32( winEr );
                                }
                            }
                        }
                    }
                    else
                    {
                        hr = HRESULT_FROM_WIN32( winEr );
                    }
                }
            }
        }
        SysFreeString(ClassName);
    }
    else
    {
        DWORD dwError = GetLastError();
        Assert(0);
        hr = HRESULT_FROM_WIN32( dwError );
    }

    return hr;
}

static HRESULT vboxNetCfgWinIsDhcpEnabled(IWbemClassObject * pAdapterConfig, BOOL *pEnabled)
{
    VARIANT vtEnabled;
    HRESULT hr = pAdapterConfig->Get(L"DHCPEnabled", 0, &vtEnabled, 0, 0);
    if(SUCCEEDED(hr))
    {
        *pEnabled = vtEnabled.boolVal;
    }
    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetAdapterSettings(const GUID * pGuid, PADAPTER_SETTINGS pSettings)
{
    HRESULT hr;
    ComPtr <IWbemServices> pSvc;
    hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
    if(SUCCEEDED(hr))
    {
        ComPtr <IWbemClassObject> pAdapterConfig;
        hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
        if(hr == S_OK)
        {
            hr = vboxNetCfgWinIsDhcpEnabled(pAdapterConfig, &pSettings->bDhcp);
            if(SUCCEEDED(hr))
            {
                hr = netIfWinGetIpSettings(pAdapterConfig, &pSettings->ip, &pSettings->mask);
            }
        }
    }

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinIsDhcpEnabled(const GUID * pGuid, BOOL *pEnabled)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if(hr == S_OK)
            {
                VARIANT vtEnabled;
                hr = pAdapterConfig->Get(L"DHCPEnabled", 0, &vtEnabled, 0, 0);
                if(SUCCEEDED(hr))
                {
                    *pEnabled = vtEnabled.boolVal;
                }
            }
        }

    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableStaticIpConfig(const GUID *pGuid, ULONG ip, ULONG mask)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if(hr == S_OK)
            {
                BOOL bIsHostOnly;
                hr = netIfWinIsHostOnly(pAdapterConfig, &bIsHostOnly);
                if(SUCCEEDED(hr))
                {
                    if(bIsHostOnly)
                    {
                        in_addr aIp[1];
                        in_addr aMask[1];
                        aIp[0].S_un.S_addr = ip;
                        aMask[0].S_un.S_addr = mask;

                        BSTR ObjPath;
                        hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                        if(SUCCEEDED(hr))
                        {
                            hr = netIfWinEnableStaticV4(pSvc, pGuid, ObjPath, aIp, aMask, ip != 0 ? 1 : 0);
                            if(SUCCEEDED(hr))
                            {
#if 0
                                in_addr aGw[1];
                                aGw[0].S_un.S_addr = gw;
                                hr = netIfWinSetGatewaysV4(pSvc, ObjPath, aGw, 1);
                                if(SUCCEEDED(hr))
#endif
                                {
                                }
                            }
                            SysFreeString(ObjPath);
                        }
                    }
                    else
                    {
                        hr = E_FAIL;
                    }
                }
            }
        }

    return hr;
}

#if 0
static HRESULT netIfEnableStaticIpConfigV6(const GUID *pGuid, IN_BSTR aIPV6Address, IN_BSTR aIPV6Mask, IN_BSTR aIPV6DefaultGateway)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if(hr == S_OK)
            {
                BSTR ObjPath;
                hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                if(SUCCEEDED(hr))
                {
                    hr = netIfWinEnableStaticV4V6(pSvc, pAdapterConfig, ObjPath, aIPV6Address, aIPV6Mask);
                    if(SUCCEEDED(hr))
                    {
                        if(aIPV6DefaultGateway)
                        {
                            hr = netIfWinSetGatewaysV4V6(pSvc, ObjPath, aIPV6DefaultGateway);
                        }
                        if(SUCCEEDED(hr))
                        {
//                            hr = netIfWinUpdateConfig(pIf);
                        }
                    }
                    SysFreeString(ObjPath);
                }
            }
        }

    return SUCCEEDED(hr) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

static HRESULT netIfEnableStaticIpConfigV6(const GUID *pGuid, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    RTNETADDRIPV6 Mask;
    int rc = prefixLength2IPv6Address(aIPV6MaskPrefixLength, &Mask);
    if(RT_SUCCESS(rc))
    {
        Bstr maskStr = composeIPv6Address(&Mask);
        rc = netIfEnableStaticIpConfigV6(pGuid, aIPV6Address, maskStr, NULL);
    }
    return rc;
}
#endif

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableDynamicIpConfig(const GUID *pGuid)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if(hr == S_OK)
            {
                BOOL bIsHostOnly;
                hr = netIfWinIsHostOnly(pAdapterConfig, &bIsHostOnly);
                if(SUCCEEDED(hr))
                {
                    if(bIsHostOnly)
                    {
                        BSTR ObjPath;
                        hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                        if(SUCCEEDED(hr))
                        {
                            hr = netIfWinEnableDHCP(pSvc, ObjPath);
                            if(SUCCEEDED(hr))
                            {
//                              hr = netIfWinUpdateConfig(pIf);
                            }
                            SysFreeString(ObjPath);
                        }
                    }
                    else
                    {
                        hr = E_FAIL;
                    }
                }
            }
        }


    return hr;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinDhcpRediscover(const GUID *pGuid)
{
    HRESULT hr;
        ComPtr <IWbemServices> pSvc;
        hr = netIfWinCreateIWbemServices(pSvc.asOutParam());
        if(SUCCEEDED(hr))
        {
            ComPtr <IWbemClassObject> pAdapterConfig;
            hr = netIfWinFindAdapterClassById(pSvc, pGuid, pAdapterConfig.asOutParam());
            if(hr == S_OK)
            {
                BOOL bIsHostOnly;
                hr = netIfWinIsHostOnly(pAdapterConfig, &bIsHostOnly);
                if(SUCCEEDED(hr))
                {
                    if(bIsHostOnly)
                    {
                        BSTR ObjPath;
                        hr = netIfWinAdapterConfigPath(pAdapterConfig, &ObjPath);
                        if(SUCCEEDED(hr))
                        {
                            hr = netIfWinDhcpRediscover(pSvc, ObjPath);
                            if(SUCCEEDED(hr))
                            {
//                        hr = netIfWinUpdateConfig(pIf);
                            }
                            SysFreeString(ObjPath);
                        }
                    }
                    else
                    {
                        hr = E_FAIL;
                    }
                }
            }
        }


    return hr;
}

typedef bool (*IPSETTINGS_CALLBACK) (ULONG ip, ULONG mask, PVOID pContext);

static void vboxNetCfgWinEnumIpConfig(PIP_ADAPTER_ADDRESSES pAddresses, IPSETTINGS_CALLBACK pCallback, PVOID pContext)
{
    PIP_ADAPTER_ADDRESSES pAdapter;
    for (pAdapter = pAddresses; pAdapter; pAdapter = pAdapter->Next)
    {
        PIP_ADAPTER_UNICAST_ADDRESS pAddr = pAdapter->FirstUnicastAddress;
        PIP_ADAPTER_PREFIX pPrefix = pAdapter->FirstPrefix;

        if(pAddr && pPrefix)
        {
            do
            {
                bool fIPFound, fMaskFound;
                fIPFound = fMaskFound = false;
                ULONG ip, mask;
                for (; pAddr && !fIPFound; pAddr = pAddr->Next)
                {
                    switch (pAddr->Address.lpSockaddr->sa_family)
                    {
                        case AF_INET:
                            fIPFound = true;
                            memcpy(&ip,
                                    &((struct sockaddr_in *)pAddr->Address.lpSockaddr)->sin_addr.s_addr,
                                    sizeof(ip));
                            break;
//                            case AF_INET6:
//                                break;
                    }
                }

                for (; pPrefix && !fMaskFound; pPrefix = pPrefix->Next)
                {
                    switch (pPrefix->Address.lpSockaddr->sa_family)
                    {
                        case AF_INET:
                            if(!pPrefix->PrefixLength || pPrefix->PrefixLength > 31) /* in case the ip helper API is queried while NetCfg write lock is held */
                                break;                                               /* the address values can contain illegal values */
                            fMaskFound = true;
                            mask = (~(((ULONG)~0) >> pPrefix->PrefixLength));
                            mask = htonl(mask);
                            break;
//                            case AF_INET6:
//                                break;
                    }
                }

                if(!fIPFound || !fMaskFound)
                    break;

                if(!pCallback(ip, mask, pContext))
                    return;
            }while(true);
        }
    }
}

typedef struct _IPPROBE_CONTEXT
{
    ULONG Prefix;
    bool bConflict;
}IPPROBE_CONTEXT, *PIPPROBE_CONTEXT;

#define IPPROBE_INIT(_pContext, _addr) \
    ((_pContext)->bConflict = false, \
     (_pContext)->Prefix = _addr)

#define IPPROBE_INIT_STR(_pContext, _straddr) \
    IPROBE_INIT(_pContext, inet_addr(_straddr))

static bool vboxNetCfgWinIpProbeCallback (ULONG ip, ULONG mask, PVOID pContext)
{
    PIPPROBE_CONTEXT pProbe = (PIPPROBE_CONTEXT)pContext;

    if((ip & mask) == (pProbe->Prefix & mask))
    {
        pProbe->bConflict = true;
        return false;
    }

    return true;
}

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(PULONG pNetIp, PULONG pNetMask)
{
    DWORD dwRc;
    HRESULT hr = S_OK;
    /*
     * Most of the hosts probably have less than 10 adapters,
     * so we'll mostly succeed from the first attempt.
     */
    ULONG uBufLen = sizeof(IP_ADAPTER_ADDRESSES) * 10;
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(uBufLen);
    if (!pAddresses)
        return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
    dwRc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &uBufLen);
    if (dwRc == ERROR_BUFFER_OVERFLOW)
    {
        /* Impressive! More than 10 adapters! Get more memory and try again. */
        free(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(uBufLen);
        if (!pAddresses)
            return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
        dwRc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &uBufLen);
    }
    if (dwRc == NO_ERROR)
    {
        IPPROBE_CONTEXT Context;
        const ULONG ip192168 = inet_addr("192.168.0.0");
        srand(GetTickCount());

        *pNetIp = 0;
        *pNetMask = 0;

        for(int i = 0; i < 255; i++)
        {
            ULONG ipProbe = rand()*255/RAND_MAX;
            ipProbe = ip192168 | (ipProbe << 16);
            IPPROBE_INIT(&Context, ipProbe);
            vboxNetCfgWinEnumIpConfig(pAddresses, vboxNetCfgWinIpProbeCallback, &Context);
            if(!Context.bConflict)
            {
                *pNetIp = ipProbe;
                *pNetMask = inet_addr("255.255.255.0");
                break;
            }
        }
        if(*pNetIp == 0)
            dwRc = ERROR_DHCP_ADDRESS_CONFLICT;
    }
    else
    {
        Log(L"VBoxNetCfgWinGenHostOnlyNetworkNetworkIp: GetAdaptersAddresses err (%d)\n", dwRc);
    }
    free(pAddresses);

    if(dwRc != NO_ERROR)
    {
        hr = HRESULT_FROM_WIN32(dwRc);
    }

    return hr;
}
