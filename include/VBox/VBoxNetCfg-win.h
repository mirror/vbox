/* $Id$ */
/** @file
 * VBoxNetCfg-win.h - Network Configuration API for Windows platforms.
 */
/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxNetCfg_win_h___
#define ___VBoxNetCfg_win_h___

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

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinQueryINetCfg(OUT INetCfg **ppNetCfg,
                          IN BOOL fGetWriteLock,
                          IN LPCWSTR pszwClientDescription,
                          IN DWORD cmsTimeout,
                          OUT LPWSTR *ppszwClientDescription);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinReleaseINetCfg(IN INetCfg *pNetCfg, IN BOOL fHasWriteLock);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetComponentByGuid(IN INetCfg *pNc, IN const GUID *pguidClass, IN const GUID * pComponentGuid, OUT INetCfgComponent **ppncc);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinNetFltInstall(IN INetCfg *pNc, IN LPCWSTR * apInfFullPaths, IN UINT cInfFullPaths);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinNetFltUninstall(IN INetCfg *pNc);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinCreateHostOnlyNetworkInterface (IN LPCWSTR pInfPath, IN bool bIsInfPathFile,
        OUT GUID *pGuid, OUT BSTR *lppszName, OUT BSTR *pErrMsg);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveHostOnlyNetworkInterface (IN const GUID *pGUID, OUT BSTR *pErrMsg);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinRemoveAllNetDevicesOfId(IN LPCWSTR lpszPnPId);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinInfInstall(IN LPCWSTR lpszInfPath);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinInfUninstallAll(IN const GUID * pGuidClass, IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD Flags);

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(OUT PULONG pNetIp, OUT PULONG pNetMask);

typedef struct _ADAPTER_SETTINGS
{
    ULONG ip;
    ULONG mask;
    BOOL bDhcp;
}ADAPTER_SETTINGS, *PADAPTER_SETTINGS;

VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableStaticIpConfig(IN const GUID *pGuid, IN ULONG ip, IN ULONG mask);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinGetAdapterSettings(IN const GUID * pGuid, OUT PADAPTER_SETTINGS pSettings);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinEnableDynamicIpConfig(IN const GUID *pGuid);
VBOXNETCFGWIN_DECL(HRESULT) VBoxNetCfgWinDhcpRediscover(IN const GUID *pGuid);


typedef VOID (*LOG_ROUTINE) (LPCSTR szString);
VBOXNETCFGWIN_DECL(VOID) VBoxNetCfgWinSetLogging(IN LOG_ROUTINE pfnLog);

RT_C_DECLS_END

/** @} */

#endif /* #ifndef ___VBoxNetCfg_win_h___ */
