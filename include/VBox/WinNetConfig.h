/** @file
 * VBoxNetFlt - Briefly describe this file, optionally with a longer description
 * in a separate paragraph. (HostDrivers)
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
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

#ifndef ___VBox_WinNetConfig_h
#define ___VBox_WinNetConfig_h

#include <winsock2.h>
#include <Windows.h>
#include <Netcfgn.h>
#include <Setupapi.h>
#include <iprt/cdefs.h>

/** @defgroup grp_vboxnetcfgwin     The Windows Network Configration Library
 * @{ */

/** @def VBOXNETCFGWIN_DECL
 * The usual declaration wrapper.
 */
#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VBOXDDU
#  define VBOXNETCFGWIN_DECL(_type) DECLEXPORT(_type)
# else
#  define VBOXNETCFGWIN_DECL(_type) DECLIMPORT(_type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VBOXNETCFGWIN_DECL(_type) _type
#endif

RT_C_DECLS_BEGIN

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinQueryINetCfg(IN BOOL fGetWriteLock, IN LPCWSTR lpszAppName, OUT INetCfg** ppnc, OUT LPWSTR *lpszLockedBy);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinReleaseINetCfg(IN INetCfg *pnc, IN BOOL fHasWriteLock);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetComponentEnum(INetCfg *pnc, IN const GUID *pguidClass, OUT IEnumNetCfgComponent **ppencc);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetFirstComponent(IN IEnumNetCfgComponent *pencc, OUT INetCfgComponent **ppncc);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetNextComponent(IN IEnumNetCfgComponent *pencc, OUT INetCfgComponent **ppncc);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinInstallComponent(IN INetCfg *pnc, IN LPCWSTR szComponentId, IN const GUID *pguidClass);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinInstallNetComponent(IN INetCfg *pnc, IN LPCWSTR lpszComponentId, IN const GUID *pguidClass,
                                                             IN LPCWSTR * apInfFullPaths, IN UINT cInfFullPaths);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinUninstallComponent(IN INetCfg *pnc, IN INetCfgComponent *pncc);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetNextBindingPath(IN IEnumNetCfgBindingPath *pencbp, OUT INetCfgBindingPath **ppncbp);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetNextBindingInterface(IN IEnumNetCfgBindingInterface *pencbi, OUT INetCfgBindingInterface **ppncbi);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetFirstBindingInterface(IN IEnumNetCfgBindingInterface *pencbi, OUT INetCfgBindingInterface **ppncbi);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetBindingInterfaceEnum(IN INetCfgBindingPath *pncbp, OUT IEnumNetCfgBindingInterface **ppencbi);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetFirstBindingPath(IN IEnumNetCfgBindingPath *pencbp, OUT INetCfgBindingPath **ppncbp);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetBindingPathEnum(IN INetCfgComponent *pncc, IN DWORD dwBindingType, OUT IEnumNetCfgBindingPath **ppencbp);
VBOXNETCFGWIN_DECL(VOID)    VBoxNetCfgWinReleaseRef(IN IUnknown *punk);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetComponentByGuid(IN INetCfg *pNc, IN const GUID *pguidClass, IN const GUID * pComponentGuid, OUT INetCfgComponent **ppncc);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinNetFltUninstall(IN INetCfg *pNc);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinNetFltInstall(IN INetCfg *pNc, IN LPCWSTR * apInfFullPaths, IN UINT cInfFullPaths);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostonlyConnectionName (PCWSTR DevName, WCHAR *pBuf, PULONG pcbBuf);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRenameConnection (LPWSTR pGuid, PCWSTR NewName);

typedef BOOL (*VBOXNETCFGWIN_NETENUM_CALLBACK) (HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnumNetDevices(LPWSTR pPnPId, VBOXNETCFGWIN_NETENUM_CALLBACK callback, PVOID pContext);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveHostOnlyNetworkInterface (const GUID *pGUID, BSTR *pErrMsg);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveAllNetDevicesOfId(LPWSTR pPnPId);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinCreateHostOnlyNetworkInterface (LPCWSTR pInfPath, bool bIsInfPathFile, GUID *pGuid, BSTR *lppszName, BSTR *pErrMsg);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinUninstallInfs (const GUID * pGuid, LPCWSTR pPnPId, DWORD Flags);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinDhcpRediscover(const GUID *pGuid);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableDynamicIpConfig(const GUID *pGuid);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableStaticIpConfig(const GUID *pGuid, ULONG ip, ULONG mask);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinIsDhcpEnabled(const GUID * pGuid, BOOL *pEnabled);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(PULONG pNetIp, PULONG pNetMask);

typedef struct _ADAPTER_SETTINGS
{
    ULONG ip;
    ULONG mask;
    BOOL bDhcp;
}ADAPTER_SETTINGS, *PADAPTER_SETTINGS;

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetAdapterSettings(const GUID * pGuid, PADAPTER_SETTINGS pSettings);

typedef VOID (*LOG_ROUTINE) (LPCWSTR szString);
VBOXNETCFGWIN_DECL(VOID) VBoxNetCfgWinSetLogging(LOG_ROUTINE Log);

RT_C_DECLS_END

/** @} */

#endif

