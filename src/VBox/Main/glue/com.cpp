/* $Id$ */
/** @file
 * MS COM / XPCOM Abstraction Layer
 */

/*
 * Copyright (C) 2005-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#if !defined (VBOX_WITH_XPCOM)

# include <iprt/win/objbase.h>

#else /* !defined (VBOX_WITH_XPCOM) */
# include <stdlib.h>
# include <nsCOMPtr.h>
# include <nsIServiceManagerUtils.h>
# include <nsIComponentManager.h>
# include <ipcIService.h>
# include <ipcCID.h>
# include <ipcIDConnectService.h>
# include <nsIInterfaceInfo.h>
# include <nsIInterfaceInfoManager.h>
// official XPCOM headers don't define it yet
#define IPC_DCONNECTSERVICE_CONTRACTID \
    "@mozilla.org/ipc/dconnect-service;1"
#endif /* !defined (VBOX_WITH_XPCOM) */

#include "VBox/com/com.h"
#include "VBox/com/assert.h"

#include "VBox/com/Guid.h"
#include "VBox/com/array.h"

#include <package-generated.h>

#include <iprt/buildconfig.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/process.h>

#include <VBox/err.h>
#include <VBox/version.h>

#if !defined(RT_OS_DARWIN) && !defined(RT_OS_WINDOWS)
char g_szXdgConfigHome[RTPATH_MAX] = "";
#endif

/**
 * Possible locations for the VirtualBox user configuration folder,
 * listed from oldest (as in legacy) to newest.  These can be either
 * absolute or relative to the home directory.  We use the first entry
 * of the list which corresponds to a real folder on storage, or
 * create a folder corresponding to the last in the list (the least
 * legacy) if none do.
 */
const char * const g_apcszUserHome[] =
#ifdef RT_OS_DARWIN
{ "Library/VirtualBox" };
#elif defined RT_OS_WINDOWS
{ ".VirtualBox" };
#else
{ ".VirtualBox", g_szXdgConfigHome };
#endif

#include "Logging.h"

namespace com
{

void GetInterfaceNameByIID(const GUID &aIID, BSTR *aName)
{
    AssertPtrReturnVoid(aName);
    *aName = NULL;

#if !defined(VBOX_WITH_XPCOM)

    LONG rc;
    LPOLESTR iidStr = NULL;
    if (StringFromIID(aIID, &iidStr) == S_OK)
    {
        HKEY ifaceKey;
        rc = RegOpenKeyExW(HKEY_CLASSES_ROOT, L"Interface",
                           0, KEY_QUERY_VALUE, &ifaceKey);
        if (rc == ERROR_SUCCESS)
        {
            HKEY iidKey;
            rc = RegOpenKeyExW(ifaceKey, iidStr, 0, KEY_QUERY_VALUE, &iidKey);
            if (rc == ERROR_SUCCESS)
            {
                /* determine the size and type */
                DWORD sz, type;
                rc = RegQueryValueExW(iidKey, NULL, NULL, &type, NULL, &sz);
                if (rc == ERROR_SUCCESS && type == REG_SZ)
                {
                    /* query the value to BSTR */
                    *aName = SysAllocStringLen(NULL, (sz + 1) / sizeof(TCHAR) + 1);
                    rc = RegQueryValueExW(iidKey, NULL, NULL, NULL, (LPBYTE) *aName, &sz);
                    if (rc != ERROR_SUCCESS)
                    {
                        SysFreeString(*aName);
                        *aName = NULL;
                    }
                }
                RegCloseKey(iidKey);
            }
            RegCloseKey(ifaceKey);
        }
        CoTaskMemFree(iidStr);
    }

#else /* !defined (VBOX_WITH_XPCOM) */

    nsresult rv;
    nsCOMPtr<nsIInterfaceInfoManager> iim =
        do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIInterfaceInfo> iinfo;
        rv = iim->GetInfoForIID(&aIID, getter_AddRefs(iinfo));
        if (NS_SUCCEEDED(rv))
        {
            const char *iname = NULL;
            iinfo->GetNameShared(&iname);
            char *utf8IName = NULL;
            if (RT_SUCCESS(RTStrCurrentCPToUtf8(&utf8IName, iname)))
            {
                PRTUTF16 utf16IName = NULL;
                if (RT_SUCCESS(RTStrToUtf16(utf8IName, &utf16IName)))
                {
                    *aName = SysAllocString((OLECHAR *) utf16IName);
                    RTUtf16Free(utf16IName);
                }
                RTStrFree(utf8IName);
            }
        }
    }

#endif /* !defined (VBOX_WITH_XPCOM) */
}

#ifdef VBOX_WITH_XPCOM

HRESULT GlueCreateObjectOnServer(const CLSID &clsid,
                                 const char *serverName,
                                 const nsIID &id,
                                 void** ppobj)
{
    HRESULT rc;
    nsCOMPtr<ipcIService> ipcServ = do_GetService(IPC_SERVICE_CONTRACTID, &rc);
    if (SUCCEEDED(rc))
    {
        PRUint32 serverID = 0;
        rc = ipcServ->ResolveClientName(serverName, &serverID);
        if (SUCCEEDED (rc))
        {
            nsCOMPtr<ipcIDConnectService> dconServ = do_GetService(IPC_DCONNECTSERVICE_CONTRACTID, &rc);
            if (SUCCEEDED(rc))
                rc = dconServ->CreateInstance(serverID,
                                              clsid,
                                              id,
                                              ppobj);
        }
    }
    return rc;
}

HRESULT GlueCreateInstance(const CLSID &clsid,
                           const nsIID &id,
                           void** ppobj)
{
    nsCOMPtr<nsIComponentManager> manager;
    HRESULT rc = NS_GetComponentManager(getter_AddRefs(manager));
    if (SUCCEEDED(rc))
        rc = manager->CreateInstance(clsid,
                                     nsnull,
                                     id,
                                     ppobj);
    return rc;
}

#endif // VBOX_WITH_XPCOM

static int composeHomePath(char *aDir, size_t aDirLen,
                           const char *pcszBase)
{
    int vrc;
    if (RTPathStartsWithRoot(pcszBase))
        vrc = RTStrCopy(aDir, aDirLen, pcszBase);
    else
    {
        /* compose the config directory (full path) */
        /** @todo r=bird: RTPathUserHome doesn't necessarily return a
         * full (abs) path like the comment above seems to indicate. */
        vrc = RTPathUserHome(aDir, aDirLen);
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppend(aDir, aDirLen, pcszBase);
    }
    return vrc;
}

int GetVBoxUserHomeDirectory(char *aDir, size_t aDirLen, bool fCreateDir)
{
    AssertReturn(aDir, VERR_INVALID_POINTER);
    AssertReturn(aDirLen > 0, VERR_BUFFER_OVERFLOW);

    /* start with null */
    *aDir = 0;

    char szTmp[RTPATH_MAX];
    int vrc = RTEnvGetEx(RTENV_DEFAULT, "VBOX_USER_HOME", szTmp, sizeof(szTmp), NULL);
    if (RT_SUCCESS(vrc) || vrc == VERR_ENV_VAR_NOT_FOUND)
    {
        bool fFound = false;
        if (RT_SUCCESS(vrc))
        {
            /* get the full path name */
            vrc = RTPathAbs(szTmp, aDir, aDirLen);
        }
        else
        {
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_DARWIN)
            vrc = RTEnvGetEx(RTENV_DEFAULT, "XDG_CONFIG_HOME", g_szXdgConfigHome, sizeof(g_szXdgConfigHome), NULL);
            if (RT_SUCCESS(vrc))
                vrc = RTPathAppend(g_szXdgConfigHome, sizeof(g_szXdgConfigHome), "VirtualBox");
            AssertMsg(vrc == VINF_SUCCESS || vrc == VERR_ENV_VAR_NOT_FOUND, ("%Rrc\n", vrc));
            if (RT_FAILURE_NP(vrc))
                vrc = RTStrCopy(g_szXdgConfigHome, sizeof(g_szXdgConfigHome), ".config/VirtualBox");
#endif
            for (unsigned i = 0; i < RT_ELEMENTS(g_apcszUserHome); ++i)
            {
                vrc = composeHomePath(aDir, aDirLen, g_apcszUserHome[i]);
                if (   RT_SUCCESS(vrc)
                    && RTDirExists(aDir))
                {
                    fFound = true;
                    break;
                }
            }
        }

        /* ensure the home directory exists */
        if (RT_SUCCESS(vrc))
            if (!fFound && fCreateDir)
                vrc = RTDirCreateFullPath(aDir, 0700);
    }

    return vrc;
}

static const char *g_pszLogEntity = NULL;

static DECLCALLBACK(void) vboxHeaderFooter(PRTLOGGER pReleaseLogger, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* some introductory information */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            bool fOldBuffered = RTLogSetBuffering(pReleaseLogger, true /*fBuffered*/);
            pfnLog(pReleaseLogger,
                   "VirtualBox %s %s r%u %s (%s %s) release log\n"
#ifdef VBOX_BLEEDING_EDGE
                   "EXPERIMENTAL build " VBOX_BLEEDING_EDGE "\n"
#endif
                   "Log opened %s\n",
                   g_pszLogEntity, VBOX_VERSION_STRING, RTBldCfgRevision(),
                   RTBldCfgTargetDotArch(), __DATE__, __TIME__, szTmp);

            pfnLog(pReleaseLogger, "Build Type: %s\n", KBUILD_TYPE);
            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pReleaseLogger, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pReleaseLogger, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pReleaseLogger, "OS Version: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pReleaseLogger, "OS Service Pack: %s\n", szTmp);

            vrc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_NAME, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pReleaseLogger, "DMI Product Name: %s\n", szTmp);
            vrc = RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pReleaseLogger, "DMI Product Version: %s\n", szTmp);

            uint64_t cbHostRam = 0, cbHostRamAvail = 0;
            vrc = RTSystemQueryTotalRam(&cbHostRam);
            if (RT_SUCCESS(vrc))
                vrc = RTSystemQueryAvailableRam(&cbHostRamAvail);
            if (RT_SUCCESS(vrc))
            {
                pfnLog(pReleaseLogger, "Host RAM: %lluMB", cbHostRam / _1M);
                if (cbHostRam > _2G)
                    pfnLog(pReleaseLogger, " (%lld.%lldGB)",
                           cbHostRam / _1G, (cbHostRam % _1G) / (_1G / 10));
                pfnLog(pReleaseLogger, " total, %lluMB", cbHostRamAvail / _1M);
                if (cbHostRamAvail > _2G)
                    pfnLog(pReleaseLogger, " (%lld.%lldGB)",
                           cbHostRamAvail / _1G, (cbHostRamAvail % _1G) / (_1G / 10));
                pfnLog(pReleaseLogger, " available\n");
            }

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pReleaseLogger,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VBOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VBOX_PACKAGE_STRING);
            RTLogSetBuffering(pReleaseLogger, fOldBuffered);
            break;
        }
        case RTLOGPHASE_PREROTATE:
            pfnLog(pReleaseLogger, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pReleaseLogger, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pReleaseLogger, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */;
    }
}

int VBoxLogRelCreate(const char *pcszEntity, const char *pcszLogFile,
                     uint32_t fFlags, const char *pcszGroupSettings,
                     const char *pcszEnvVarBase, uint32_t fDestFlags,
                     uint32_t cMaxEntriesPerGroup, uint32_t cHistory,
                     uint32_t uHistoryFileTime, uint64_t uHistoryFileSize,
                     char *pszError, size_t cbError)
{
    Assert(cbError >= RTPATH_MAX + 128);

    /* create release logger */
    PRTLOGGER pReleaseLogger;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    g_pszLogEntity = pcszEntity;
    int vrc = RTLogCreateEx(&pReleaseLogger, fFlags, pcszGroupSettings,
                            pcszEnvVarBase, RT_ELEMENTS(s_apszGroups), s_apszGroups, fDestFlags,
                            vboxHeaderFooter, cHistory, uHistoryFileSize, uHistoryFileTime,
                            pszError, cbError,
                            pcszLogFile ? "%s" : NULL, pcszLogFile);
    if (RT_SUCCESS(vrc))
    {
        /* make sure that we don't flood logfiles */
        RTLogSetGroupLimit(pReleaseLogger, cMaxEntriesPerGroup);

        /* explicitly flush the log, to have some info when buffering */
        RTLogFlush(pReleaseLogger);

        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(pReleaseLogger);
    }
    return vrc;
}


/* static */
const Guid Guid::Empty; /* default ctor is OK */

#if defined (VBOX_WITH_XPCOM)

/* static */
const nsID *SafeGUIDArray::nsIDRef::Empty = (const nsID *)Guid::Empty.raw();

#endif /* (VBOX_WITH_XPCOM) */

} /* namespace com */
