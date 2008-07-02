/** $Id$ */
/** @file
 * VBoxControl - Guest Additions Command Line Management Interface
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/initterm.h>
#include <VBox/VBoxGuest.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_INFO_SVC
# include <VBox/HostServices/VBoxInfoSvc.h>
#endif
#include "VBoxControl.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The program name (derived from argv[0]). */
char const *g_pszProgName = "";
/** The current verbosity level. */
int g_cVerbosity = 0;


/**
 * Displays the program usage message.
 *
 * @param u64Which 
 * 
 * @{
 */ 

/** Helper function */
static void doUsage(char const *line, char const *name = "", char const *command = "")
{
    RTPrintf("%s %-*s%s", name, 30 - strlen(name), command, line);
}

/** Enumerate the different parts of the usage we might want to print out */
enum g_eUsage
{
#ifdef VBOX_WITH_INFO_SVC
    GET_GUEST_PROP,
    SET_GUEST_PROP,
#endif
    USAGE_ALL = UINT32_MAX
};

static void usage(g_eUsage eWhich = USAGE_ALL)
{
    RTPrintf("Usage:\n\n");
    RTPrintf("%s [-v|--version]    print version number and exit\n", g_pszProgName);
    RTPrintf("%s --nologo ...      suppress the logo\n\n", g_pszProgName);

    if ((GET_GUEST_PROP == eWhich) || (USAGE_ALL == eWhich))
        doUsage("<key>\n", g_pszProgName, "getguestproperty");
    if ((SET_GUEST_PROP == eWhich) || (USAGE_ALL == eWhich))
        doUsage("<key> [<value>] (no value deletes key)\n", g_pszProgName, "setguestproperty");
}
/** @} */

/**
 * Displays an error message.
 *
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
static void VBoxControlError(const char *pszFormat, ...)
{
    // RTStrmPrintf(g_pStdErr, "%s: error: ", g_pszProgName);

    va_list va;
    va_start(va, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, va);
    va_end(va);
}

#ifdef VBOX_WITH_INFO_SVC
/**
 * Retrieves a value from the host/guest configuration registry.
 * This is accessed through the "VBoxSharedInfoSvc" HGCM service.
 *
 * @returns 0 on success, 1 on failure
 * @param   key  (string) the key which the value is stored under.
 */
int getGuestProperty(int argc, char **argv)
{
    using namespace svcInfo;

    uint32_t u32ClientID = 0;
    int rc = VINF_SUCCESS;

    char szValue[KEY_MAX_VALUE_LEN];
    if (argc != 1)
    {
        usage(GET_GUEST_PROP);
        return 1;
    }
    rc = VbglR3InfoSvcConnect(&u32ClientID);
    if (!RT_SUCCESS(rc))
        VBoxControlError("Failed to connect to the guest property service, error %Rrc\n", rc);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3InfoSvcReadKey(u32ClientID, argv[0], szValue, sizeof(szValue), NULL);
        if (!RT_SUCCESS(rc) && (rc != VERR_NOT_FOUND))
            VBoxControlError("Failed to retrieve the property value, error %Rrc\n", rc);
    }
    if (RT_SUCCESS(rc) || (VERR_NOT_FOUND == rc))
    {
        if (RT_SUCCESS(rc))
            RTPrintf("Value: %s\n", szValue);
        else
            RTPrintf("No value set!\n");
    }
    if (u32ClientID != 0)
        VbglR3InfoSvcDisconnect(u32ClientID);
    return RT_SUCCESS(rc) ? 0 : 1;
}


/**
 * Writes a value to the host/guest configuration registry.
 * This is accessed through the "VBoxSharedInfoSvc" HGCM service.
 *
 * @returns 0 on success, 1 on failure
 * @param   key   (string) the key which the value is stored under.
 * @param   value (string) the value to write.  If empty, the key will be
 *                removed.
 */
static int setGuestProperty(int argc, char *argv[])
{
    if (argc != 1 && argc != 2)
    {
        usage();
        return 1;
    }
    char *pszValue = NULL;
    int rc = VINF_SUCCESS;
    uint32_t u32ClientID = 0;

    if (2 == argc)
    pszValue = argv[1];
    rc = VbglR3InfoSvcConnect(&u32ClientID);
    if (!RT_SUCCESS(rc))
        VBoxControlError("Failed to connect to the host/guest registry service, error %Rrc\n", rc);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3InfoSvcWriteKey(u32ClientID, argv[0], pszValue);
        if (!RT_SUCCESS(rc))
            VBoxControlError("Failed to store the property value, error %Rrc\n", rc);
    }
    if (u32ClientID != 0)
        VbglR3InfoSvcDisconnect(u32ClientID);
    return RT_SUCCESS(rc) ? 0 : 1;
}
#endif

/** command handler type */
typedef DECLCALLBACK(int) FNHANDLER(int argc, char *argv[]);
typedef FNHANDLER *PFNHANDLER;

/** The table of all registered command handlers. */
struct COMMANDHANDLER
{
    const char *command;
    PFNHANDLER handler;
} g_commandHandlers[] =
{
#ifdef VBOX_WITH_INFO_SVC
    { "getguestproperty", getGuestProperty },
    { "setguestproperty", setGuestProperty }
#endif
};

/** Main function */
int main(int argc, char **argv)
{
    /** The application's global return code */
    int rc = 0;
    /** An IPRT return code for local use */
    int rrc = VINF_SUCCESS;
    /** The index of the command line argument we are currently processing */
    int iArg = 1;
    /** Should we show the logo text? */
    bool showlogo = true;
    /** Should we print the usage after the logo?  For the --help switch. */
    bool dohelp = false;
    /** Will we be executing a command or just printing information? */
    bool onlyinfo = false;

/*
 * Start by handling command line switches
 */

    /** Are we finished with handling switches? */
    bool done = false;
    while (!done && (iArg < argc))
    {
        if (   (0 == strcmp(argv[iArg], "-v"))
            || (0 == strcmp(argv[iArg], "--version"))
           )
            {
                /* Print version number, and do nothing else. */
                RTPrintf("%sr%d\n", VBOX_VERSION_STRING, VBoxSVNRev ());
                onlyinfo = true;
                showlogo = false;
                done = true;
            }
        else if (0 == strcmp(argv[iArg], "--nologo"))
            showlogo = false;
        else if (0 == strcmp(argv[iArg], "--help"))
        {
            onlyinfo = true;
            dohelp = true;
            done = true;           
        }
        else
            /* We have found an argument which isn't a switch.  Exit to the
             * command processing bit. */
            done = true;
        if (!done)
            ++iArg;
    }

/*
 * Find the application name, show our logo if the user hasn't suppressed it,
 * and show the usage if the user asked us to
 */

    g_pszProgName = RTPathFilename(argv[0]);
    if (showlogo)
        RTPrintf("VirtualBox Guest Additions Command Line Management Interface Version "
                 VBOX_VERSION_STRING "\n"
                 "(C) 2008 Sun Microsystems, Inc.\n"
                 "All rights reserved\n\n");
    if (dohelp)
        usage();

/*
 * Do global initialisation for the programme if we will be handling a command
 */

    if (!onlyinfo)
    {
        rrc = RTR3Init(false, 0);
        if (!RT_SUCCESS(rrc))
        {
            VBoxControlError("Failed to initialise the VirtualBox runtime - error %Rrc\n", rrc);
            rc = 1;
        }
        if (0 == rc)
        {
            if (!RT_SUCCESS(VbglR3Init()))
            {
                VBoxControlError("Could not contact the host system.  Make sure that you are running this\n"
                                 "application inside a VirtualBox guest system, and that you have sufficient\n"
                                 "user permissions.\n");
                rc = 1;
            }
        }
    }

/*
 * Now look for an actual command in the argument list and handle it.
 */

    if (!onlyinfo && (0 == rc))
    {
        if (argc > iArg)
        {
            /** Is next parameter a known command? */
            bool found = false;
            /** And if so, what is its position in the table? */
            unsigned index = 0;
            while (index < RT_ELEMENTS(g_commandHandlers) && !found)
            {
                if (0 == strcmp(argv[iArg], g_commandHandlers[index].command))
                    found = true;
                else
                    ++index;
            }
            if (found)
                rc = g_commandHandlers[index].handler(argc - iArg - 1, argv + iArg + 1);
            else
            {
                rc = 1;
                usage();
            }
        }
        else
        {
            /* The user didn't specify a command. */
            rc = 1;
            usage();
        }
    }

/*
 * And exit, returning the status
 */

    return rc;
}
