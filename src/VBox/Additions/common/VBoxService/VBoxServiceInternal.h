/* $Id$ */
/** @file
 * VBoxService - Guest Additions Services.
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
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

#ifndef ___VBoxServiceInternal_h
#define ___VBoxServiceInternal_h

#include <stdio.h>
#ifdef RT_OS_WINDOWS
# include <Windows.h>
# include <tchar.h>   /**@todo just drop this, this will be compiled as UTF-8/ANSI. */
# include <process.h> /**@todo what's this here for?  */
#endif

/**
 * A service descriptor.
 */
typedef struct
{
    /** The short service name. */
    const char *pszName;
    /** The longer service name. */
    const char *pszDescription;
    /** The usage options stuff for the --help screen. */
    const char *pszUsage;
    /** The option descriptions for the --help screen. */
    const char *pszOptions;

    /**
     * Called before parsing arguments.
     * @returns VBox status code.
     */
    DECLCALLBACKMEMBER(int, pfnPreInit)(void);

    /**
     * Tries to parse the given command line option.
     *
     * @returns 0 if we parsed, -1 if it didn't and anything else means exit.
     * @param   ppszShort   If not NULL it points to the short option iterator. a short argument.
     *                      If NULL examine argv[*pi].
     * @param   argc        The argument count.
     * @param   argv        The argument vector.
     * @param   pi          The argument vector index. Update if any value(s) are eaten.
     */
    DECLCALLBACKMEMBER(int, pfnOption)(const char **ppszShort, int argc, char **argv, int *pi);

    /**
     * Called before parsing arguments.
     * @returns VBox status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit)(void);

    /** Called from the worker thread.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS if exitting because *pfTerminate was set.
     * @param   pfTerminate     Pointer to a per service termination flag to check
     *                          before and after blocking.
     */
    DECLCALLBACKMEMBER(int, pfnWorker)(bool volatile *pfTerminate);

    /**
     * Stop an service.
     */
    DECLCALLBACKMEMBER(void, pfnStop)(void);

    /**
     * Does termination cleanups.
     */
    DECLCALLBACKMEMBER(void, pfnTerm)(void);
} VBOXSERVICE;
/** Pointer to a VBOXSERVICE. */
typedef VBOXSERVICE *PVBOXSERVICE;
/** Pointer to a const VBOXSERVICE. */
typedef VBOXSERVICE const *PCVBOXSERVICE;

#ifdef RT_OS_WINDOWS
/** The service name (needed for mutex creation on Windows). */
#define VBOXSERVICE_NAME           L"VBoxService"
/** The friendly service name. */
#define VBOXSERVICE_FRIENDLY_NAME  L"VBoxService"
/** The following constant may be defined by including NtStatus.h. */
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
/** Structure for storing the looked up user information. */
typedef struct
{
    TCHAR szUser [_MAX_PATH];
    TCHAR szAuthenticationPackage [_MAX_PATH];
    TCHAR szLogonDomain [_MAX_PATH];
} VBOXSERVICEVMINFOUSER, *PVBOXSERVICEVMINFOUSER;
/** Structure for the file information lookup. */
typedef struct
{
    TCHAR* pszFilePath;
    TCHAR* pszFileName;
} VBOXSERVICEVMINFOFILE, *PVBOXSERVICEVMINFOFILE;
/** Function prototypes for dynamic loading. */
typedef DWORD (WINAPI* fnWTSGetActiveConsoleSessionId)();
#endif

__BEGIN_DECLS

extern char *g_pszProgName;
extern int g_cVerbosity;
extern uint32_t g_DefaultInterval;

extern int VBoxServiceSyntax(const char *pszFormat, ...);
extern int VBoxServiceError(const char *pszFormat, ...);
extern void VBoxServiceVerbose(int iLevel, const char *pszFormat, ...);
extern int VBoxServiceArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32, uint32_t u32Min, uint32_t u32Max);
extern unsigned VBoxServiceGetStartedServices(void);
extern int VBoxServiceStartServices(unsigned iMain);
extern int VBoxServiceStopServices(void);

extern VBOXSERVICE g_TimeSync;
extern VBOXSERVICE g_Clipboard;
extern VBOXSERVICE g_Control;
extern VBOXSERVICE g_VMInfo;

#ifdef RT_OS_WINDOWS
extern DWORD g_rcWinService;
extern SERVICE_STATUS_HANDLE g_hWinServiceStatus;
extern SERVICE_TABLE_ENTRY const g_aServiceTable[];     /** @todo generate on the fly, see comment in main() from the enabled sub services. */

/** Installs the service into the registry. */
extern int VBoxServiceWinInstall(void);
/** Uninstalls the service from the registry. */
extern int VBoxServiceWinUninstall(void);
#ifdef VBOX_WITH_GUEST_PROPS
/** Detects wheter a user is logged on based on the enumerated processes. */
extern BOOL VboxServiceVMInfoWinIsLoggedIn(VBOXSERVICEVMINFOUSER* a_pUserInfo,
                                           PLUID a_pSession,
                                           PLUID a_pLuid,
                                           DWORD a_dwNumOfProcLUIDs);
/** Gets logon user IDs from enumerated processes. */
extern DWORD VboxServiceVMInfoWinGetLUIDsFromProcesses(PLUID *ppLuid);
#endif /* VBOX_WITH_GUEST_PROPS */
#endif /* RT_OS_WINDOWS */

__END_DECLS

#endif

