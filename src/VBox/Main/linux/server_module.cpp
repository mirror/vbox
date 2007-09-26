/** @file
 *
 * XPCOM server process hepler module implementation functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef RT_OS_OS2
# include <prproces.h>
#endif

#include <nsMemory.h>
#include <nsString.h>
#include <nsCOMPtr.h>
#include <nsIFile.h>
#include <nsIGenericFactory.h>
#include <nsIServiceManagerUtils.h>
#include <nsICategoryManager.h>
#include <nsDirectoryServiceDefs.h>

#include <ipcIService.h>
#include <ipcIDConnectService.h>
#include <ipcCID.h>
#include <ipcdclient.h>

// official XPCOM headers don't define it yet
#define IPC_DCONNECTSERVICE_CONTRACTID \
    "@mozilla.org/ipc/dconnect-service;1"

// generated file
#include <VirtualBox_XPCOM.h>

#include "linux/server.h"
#include "Logging.h"

#include <VBox/err.h>

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/env.h>
#include <iprt/thread.h>

#include <string.h>


/// @todo move this to RT headers (and use them in MachineImpl.cpp as well)
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
#define HOSTSUFF_EXE ".exe"
#else /* !RT_OS_WINDOWS */
#define HOSTSUFF_EXE ""
#endif /* !RT_OS_WINDOWS */


/** Name of the server executable. */
const char VBoxSVC_exe[] = RTPATH_SLASH_STR "VBoxSVC" HOSTSUFF_EXE;

enum
{
    /** Amount of time to wait for the server to establish a connection, ms */
    VBoxSVC_Timeout = 30000,
    /** How often to perform a connection check, ms */
    VBoxSVC_WaitSlice = 100,
};

/** 
 *  Full path to the VBoxSVC executable.
 */
static char VBoxSVCPath [RTPATH_MAX];
static bool IsVBoxSVCPathSet = false;

/*
 *  The following macros define the method necessary to provide a list of
 *  interfaces implemented by the VirtualBox component. Note that this must be
 *  in sync with macros used for VirtualBox in server.cpp for the same purpose.
 */

NS_DECL_CLASSINFO (VirtualBox)
NS_IMPL_CI_INTERFACE_GETTER1 (VirtualBox, IVirtualBox)

/** 
 *  VirtualBox component constructor.
 *
 *  This constructor is responsible for starting the VirtualBox server
 *  process, connecting to it, and redirecting the constructor request to the
 *  VirtualBox component defined on the server.
 */
static NS_IMETHODIMP
VirtualBoxConstructor (nsISupports *aOuter, REFNSIID aIID,
                       void **aResult)
{
    LogFlowFuncEnter();

    nsresult rc = NS_OK;
    int vrc = VINF_SUCCESS;

    do
    {
        *aResult = NULL;
        if (NULL != aOuter)
        {
            rc = NS_ERROR_NO_AGGREGATION;
            break;
        }

        if (!IsVBoxSVCPathSet)
        {
            /* Get the directory containing XPCOM components -- the VBoxSVC
             * executable is expected in the parent directory. */
            nsCOMPtr <nsIProperties> dirServ = do_GetService (NS_DIRECTORY_SERVICE_CONTRACTID, &rc);
            if (NS_SUCCEEDED (rc))
            {
                nsCOMPtr <nsIFile> componentDir;
                rc = dirServ->Get (NS_XPCOM_COMPONENT_DIR,
                                   NS_GET_IID (nsIFile), getter_AddRefs (componentDir));

                if (NS_SUCCEEDED (rc))
                {
                    nsCAutoString path;
                    componentDir->GetNativePath (path);

                    LogFlowFunc (("components path = \"%s\"\n", path.get()));
                    AssertBreak (path.Length() + strlen (VBoxSVC_exe) < RTPATH_MAX,
                                 rc = NS_ERROR_FAILURE);

                    strcpy (VBoxSVCPath, path.get());
                    RTPathStripFilename (VBoxSVCPath);
                    strcat (VBoxSVCPath, VBoxSVC_exe);

                    IsVBoxSVCPathSet = true;
                }
            }
            if (NS_FAILED (rc))
                break;
        }

        nsCOMPtr <ipcIService> ipcServ = do_GetService (IPC_SERVICE_CONTRACTID, &rc);
        if (NS_FAILED (rc))
            break;

        /* connect to the VBoxSVC server process */

        bool startedOnce = false;
        unsigned timeLeft = VBoxSVC_Timeout;

        do
        {
            LogFlowFunc (("Resolving server name \"%s\"...\n", VBOXSVC_IPC_NAME));

            PRUint32 serverID = 0;
            rc = ipcServ->ResolveClientName (VBOXSVC_IPC_NAME, &serverID);
            if (NS_FAILED (rc))
            {
                LogFlowFunc (("Starting server...\n", VBoxSVCPath));

                startedOnce = true;

#ifdef RT_OS_OS2
                char * const args[] = { VBoxSVCPath, "--automate", 0 };
                /* use NSPR because we want the process to be detached right
                 * at startup (it isn't possible to detach it later on),
                 * RTProcCreate() isn't yet capable of doing that. */
                PRStatus rv = PR_CreateProcessDetached (VBoxSVCPath,
                                                        args, NULL, NULL);
                if (rv != PR_SUCCESS)
                {
                    rc = NS_ERROR_FAILURE;
                    break;
                }
#else
                const char *args[] = { VBoxSVCPath, "--automate", 0 };
                RTPROCESS pid = NIL_RTPROCESS;
                vrc = RTProcCreate (VBoxSVCPath, args, RTENV_DEFAULT, 0, &pid);
                if (VBOX_FAILURE (vrc))
                {
                    rc = NS_ERROR_FAILURE;
                    break;
                }
#endif

                /* wait for the server process to establish a connection */
                do
                {
                    RTThreadSleep (VBoxSVC_WaitSlice);
                    rc = ipcServ->ResolveClientName (VBOXSVC_IPC_NAME, &serverID);
                    if (NS_SUCCEEDED (rc))
                        break;
                    if (timeLeft <= VBoxSVC_WaitSlice)
                    {
                        timeLeft = 0;
                        break;
                    }
                    timeLeft -= VBoxSVC_WaitSlice;
                }
                while (1);

                if (!timeLeft)
                {
                    rc = IPC_ERROR_WOULD_BLOCK;
                    break;
                }
            }

            LogFlowFunc (("Connecting to server (ID=%d)...\n", serverID));

            nsCOMPtr <ipcIDConnectService> dconServ =
                do_GetService (IPC_DCONNECTSERVICE_CONTRACTID, &rc);
            if (NS_FAILED (rc))
                break;

            rc = dconServ->CreateInstance (serverID,
                                           (nsCID) NS_VIRTUALBOX_CID,
                                           aIID, aResult);
            if (NS_SUCCEEDED (rc))
                break;

            /* It's possible that the server gets shut down after we
             * successfully resolve the server name but before it
             * receives our CreateInstance() request. So, check for the
             * name again, and restart the cycle if it fails. */
            if (!startedOnce)
            {
                nsresult rc2 =
                    ipcServ->ResolveClientName (VBOXSVC_IPC_NAME, &serverID);
                if (NS_SUCCEEDED (rc2))
                    break;
            }
            else
                break;
        }
        while (1);
    }
    while (0);

    LogFlowFunc (("rc=%08X, vrc=%Vrc\n", rc, vrc));
    LogFlowFuncLeave();

    return rc;
}

#if 0
/// @todo not really necessary for the moment
/** 
 * 
 * @param aCompMgr 
 * @param aPath 
 * @param aLoaderStr 
 * @param aType 
 * @param aInfo 
 * 
 * @return 
 */
static NS_IMETHODIMP
VirtualBoxRegistration (nsIComponentManager *aCompMgr,
                        nsIFile *aPath,
                        const char *aLoaderStr,
                        const char *aType,
                        const nsModuleComponentInfo *aInfo)
{
    nsCAutoString modulePath;
    aPath->GetNativePath (modulePath);
    nsCAutoString moduleTarget;
    aPath->GetNativeTarget (moduleTarget);

    LogFlowFunc (("aPath=%s, aTarget=%s, aLoaderStr=%s, aType=%s\n",
                  modulePath.get(), moduleTarget.get(), aLoaderStr, aType));

    nsresult rc = NS_OK;

    return rc;
}
#endif

/** 
 *  Component definition table.
 *  Lists all components defined in this module.
 */
static const nsModuleComponentInfo components[] =
{
    {
        "VirtualBox component", // description
        NS_VIRTUALBOX_CID, NS_VIRTUALBOX_CONTRACTID, // CID/ContractID
        VirtualBoxConstructor, // constructor function
        NULL, /* VirtualBoxRegistration, */ // registration function
        NULL, // deregistration function
        NULL, // destructor function
        /// @todo 
        NS_CI_INTERFACE_GETTER_NAME(VirtualBox), // interfaces function
        NULL, // language helper
        /// @todo 
        &NS_CLASSINFO_NAME(VirtualBox) // global class info & flags
    }
};

NS_IMPL_NSGETMODULE (VirtualBox_Server_Module, components)
