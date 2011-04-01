/* $Id$ */
/** @file
 * VBoxDrvCfg-win.h - Windows Driver Manipulation API
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
#ifndef ___VBoxDrvCfg_win_h___
#define ___VBoxDrvCfg_win_h___

#include <Windows.h>

#include <iprt/cdefs.h>
#include <VBox/cdefs.h>

RT_C_DECLS_BEGIN

#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VBOXDRVCFG
#  define VBOXDRVCFG_DECL(_type) DECLEXPORT(_type)
# else
#  define VBOXDRVCFG_DECL(_type) DECLIMPORT(_type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VBOXDRVCFG_DECL(_type) _type VBOXCALL
#endif

typedef enum
{
    VBOXDRVCFG_LOG_SEVERITY_FLOW = 1,
    VBOXDRVCFG_LOG_SEVERITY_REGULAR,
    VBOXDRVCFG_LOG_SEVERITY_REL
} VBOXDRVCFG_LOG_SEVERITY;

typedef DECLCALLBACK(void) FNVBOXDRVCFG_LOG(VBOXDRVCFG_LOG_SEVERITY enmSeverity, char * msg, void * pvContext);
typedef FNVBOXDRVCFG_LOG *PFNVBOXDRVCFG_LOG;

VBOXDRVCFG_DECL(void) VBoxDrvCfgLoggerSet(PFNVBOXDRVCFG_LOG pfnLog, void *pvLog);

typedef DECLCALLBACK(void) FNVBOXDRVCFG_PANIC(void * pvPanic);
typedef FNVBOXDRVCFG_PANIC *PFNVBOXDRVCFG_PANIC;
VBOXDRVCFG_DECL(void) VBoxDrvCfgPanicSet(PFNVBOXDRVCFG_PANIC pfnPanic, void *pvPanic);

VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfInstall(IN LPCWSTR lpszInfPath);
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfUninstall(IN LPCWSTR lpszInfPath, IN DWORD Flags);
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfUninstallAllSetupDi(IN const GUID * pGuidClass, IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD Flags);
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfUninstallAllF(IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD Flags);

RT_C_DECLS_END

#endif /* #ifndef ___VBoxDrvCfg_win_h___ */
