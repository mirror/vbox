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

#ifdef RT_OS_WINDOWS
# include <Windows.h>
# include <tchar.h>   /**@todo just drop this, this will be compiled as UTF-8/ANSI. */
# include <process.h> /**@todo what's this here for?  */
#endif

/** @todo just move this into the windows specific code, it's not needed
 *        here. */
/** The service name. */
#define VBOXSERVICE_NAME           "VBoxService"
/** The friendly service name. */
#define VBOXSERVICE_FRIENDLY_NAME  "VBoxService"

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


__BEGIN_DECLS

extern char *g_pszProgName;
extern int g_cVerbosity;
extern uint32_t g_DefaultInterval;
/** Windows SCM stuff.
 *  @todo document each of them individually, this comment only documents
 *  g_vboxServiceStatusCode on windows. On the other platforms it will be
 *  dangling.
 *  @todo all this should be moved to -win.cpp and exposed via functions. */
#ifdef RT_OS_WINDOWS
extern DWORD                 g_rcWinService;
extern SERVICE_STATUS_HANDLE g_hWinServiceStatus;
extern SERVICE_TABLE_ENTRY const g_aServiceTable[];     /** @todo generate on the fly, see comment in main() from the enabled sub services. */
#endif

extern int VBoxServiceSyntax(const char *pszFormat, ...);
extern int VBoxServiceError(const char *pszFormat, ...);
extern void VBoxServiceVerbose(int iLevel, const char *pszFormat, ...);
extern int VBoxServiceArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32, uint32_t u32Min, uint32_t u32Max);
extern unsigned VBoxServiceGetStartedServices(void);
extern int VBoxServiceStartServices(unsigned iMain);
extern int VBoxServiceStopServices(void);
#ifdef RT_OS_WINDOWS
extern int VBoxServiceWinInstall(void);
extern int VBoxServiceWinUninstall(void);
#endif

extern VBOXSERVICE g_TimeSync;
extern VBOXSERVICE g_Clipboard;
extern VBOXSERVICE g_Control;
extern VBOXSERVICE g_VMInfo;

__END_DECLS

#endif

