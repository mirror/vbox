/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer - Initialization and Termination.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#if !defined (VBOX_WITH_XPCOM)

#include <objbase.h>

#else /* !defined (VBOX_WITH_XPCOM) */

#include <stdlib.h>

/* XPCOM_GLUE is defined when the client uses the standalone glue
 * (i.e. dynamically picks up the existing XPCOM shared library installation).
 * This is not the case for VirtualBox XPCOM clients (they are always
 * distrubuted with the self-built XPCOM library, and therefore have a binary
 * dependency on it) but left here for clarity.
 */
#if defined (XPCOM_GLUE)
#include <nsXPCOMGlue.h>
#endif

#include <nsIComponentRegistrar.h>
#include <nsIServiceManager.h>
#include <nsCOMPtr.h>
#include <nsEventQueueUtils.h>
#include <nsEmbedString.h>

#include <nsILocalFile.h>
#include <nsIDirectoryService.h>
#include <nsDirectoryServiceDefs.h>

#endif /* !defined (VBOX_WITH_XPCOM) */

#include "VBox/com/com.h"
#include "VBox/com/assert.h"

#include "../include/Logging.h"

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/env.h>
#include <iprt/asm.h>

#include <VBox/err.h>

namespace com
{

#if defined (VBOX_WITH_XPCOM)

class DirectoryServiceProvider : public nsIDirectoryServiceProvider
{
public:

    NS_DECL_ISUPPORTS

    DirectoryServiceProvider()
        : mCompRegLocation (NULL), mXPTIDatLocation (NULL)
        , mComponentDirLocation (NULL), mCurrProcDirLocation (NULL)
        {}

    virtual ~DirectoryServiceProvider();

    HRESULT init (const char *aCompRegLocation,
                  const char *aXPTIDatLocation,
                  const char *aComponentDirLocation,
                  const char *aCurrProcDirLocation);

    NS_DECL_NSIDIRECTORYSERVICEPROVIDER

private:

    char *mCompRegLocation;
    char *mXPTIDatLocation;
    char *mComponentDirLocation;
    char *mCurrProcDirLocation;
};

NS_IMPL_ISUPPORTS1 (DirectoryServiceProvider, nsIDirectoryServiceProvider)

DirectoryServiceProvider::~DirectoryServiceProvider()
{
    if (mCompRegLocation)
    {
        RTStrFree (mCompRegLocation);
        mCompRegLocation = NULL;
    }
    if (mXPTIDatLocation)
    {
        RTStrFree (mXPTIDatLocation);
        mXPTIDatLocation = NULL;
    }
    if (mComponentDirLocation)
    {
        RTStrFree (mComponentDirLocation);
        mComponentDirLocation = NULL;
    }
    if (mCurrProcDirLocation)
    {
        RTStrFree (mCurrProcDirLocation);
        mCurrProcDirLocation = NULL;
    }
}

/**
 *  @param aCompRegLocation Path to compreg.dat, in Utf8.
 *  @param aXPTIDatLocation Path to xpti.data, in Utf8.
 */
HRESULT
DirectoryServiceProvider::init (const char *aCompRegLocation,
                                const char *aXPTIDatLocation,
                                const char *aComponentDirLocation,
                                const char *aCurrProcDirLocation)
{
    AssertReturn (aCompRegLocation, NS_ERROR_INVALID_ARG);
    AssertReturn (aXPTIDatLocation, NS_ERROR_INVALID_ARG);

    int vrc = RTStrUtf8ToCurrentCP (&mCompRegLocation, aCompRegLocation);
    if (RT_SUCCESS (vrc))
        vrc = RTStrUtf8ToCurrentCP (&mXPTIDatLocation, aXPTIDatLocation);
    if (RT_SUCCESS (vrc) && aComponentDirLocation)
        vrc = RTStrUtf8ToCurrentCP (&mComponentDirLocation, aComponentDirLocation);
    if (RT_SUCCESS (vrc) && aCurrProcDirLocation)
        vrc = RTStrUtf8ToCurrentCP (&mCurrProcDirLocation, aCurrProcDirLocation);

    return RT_SUCCESS (vrc) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
DirectoryServiceProvider::GetFile (const char *aProp,
                                   PRBool *aPersistent,
                                   nsIFile **aRetval)
{
    nsCOMPtr <nsILocalFile> localFile;
    nsresult rv = NS_ERROR_FAILURE;

    *aRetval = nsnull;
    *aPersistent = PR_TRUE;

    const char *fileLocation = NULL;

    if (strcmp (aProp, NS_XPCOM_COMPONENT_REGISTRY_FILE) == 0)
        fileLocation = mCompRegLocation;
    else if (strcmp (aProp, NS_XPCOM_XPTI_REGISTRY_FILE) == 0)
        fileLocation = mXPTIDatLocation;
    else if (mComponentDirLocation && strcmp (aProp, NS_XPCOM_COMPONENT_DIR) == 0)
        fileLocation = mComponentDirLocation;
    else if (mCurrProcDirLocation && strcmp (aProp, NS_XPCOM_CURRENT_PROCESS_DIR) == 0)
        fileLocation = mCurrProcDirLocation;
    else
        return NS_ERROR_FAILURE;

    rv = NS_NewNativeLocalFile (nsEmbedCString (fileLocation),
                                PR_TRUE, getter_AddRefs (localFile));
    if (NS_FAILED(rv))
        return rv;

    return localFile->QueryInterface (NS_GET_IID (nsIFile),
                                      (void **) aRetval);
}

/**
 *  Global XPCOM initialization flag (we maintain it ourselves since XPCOM
 *  doesn't provide such functionality)
 */
static bool gIsXPCOMInitialized = false;

/**
 *  Number of Initialize() calls on the main thread.
 */
static unsigned int gXPCOMInitCount = 0;

#endif /* defined (VBOX_WITH_XPCOM) */

/**
 * Initializes the COM runtime.
 *
 * This method must be called on each thread of the client application that
 * wants to access COM facilities. The initialization must be performed before
 * calling any other COM method or attempting to instantiate COM objects.
 *
 * On platforms using XPCOM, this method uses the following scheme to search for
 * XPCOM runtime:
 *
 * 1. If the VBOX_APP_HOME environment variable is set, the path it specifies
 *    is used to search XPCOM libraries and components. If this method fails to
 *    initialize XPCOM runtime using this path, it will immediately return a
 *    failure and will NOT check for other paths as described below.
 *
 * 2. If VBOX_APP_HOME is not set, this methods tries the following paths in the
 *    given order:
 *
 *    a) Compiled-in application data directory (as returned by
 *       RTPathAppPrivateArch())
 *    b) "/usr/lib/virtualbox" (Linux only)
 *    c) "/opt/VirtualBox" (Linux only)
 *
 *    The first path for which the initialization succeeds will be used.
 *
 * On MS COM platforms, the COM runtime is provided by the system and does not
 * need to be searched for.
 *
 * Once the COM subsystem is no longer necessary on a given thread, Shutdown()
 * must be called to free resources allocated for it. Note that a thread may
 * call Initialize() several times but for each of tese calls there must be a
 * corresponding Shutdown() call.
 *
 * @return S_OK on success and a COM result code in case of failure.
 */
HRESULT Initialize()
{
    HRESULT rc = E_FAIL;

#if !defined (VBOX_WITH_XPCOM)

    DWORD flags = COINIT_MULTITHREADED |
                  COINIT_DISABLE_OLE1DDE |
                  COINIT_SPEED_OVER_MEMORY;

    rc = CoInitializeEx (NULL, flags);

    /// @todo the below rough method of changing the aparment type doesn't
    /// work on some systems for unknown reason (CoUninitialize() simply does
    /// nothing there, or at least all 10 000 of subsequent CoInitializeEx()
    /// continue to return RPC_E_CHANGED_MODE there). The problem on those
    /// systems is related to the "Extend support for advanced text services
    /// to all programs" checkbox in the advanced language settings dialog,
    /// i.e. the problem appears when this checkbox is checked and disappears
    /// if you clear it. For this reason, we disable the code below and
    /// instead initialize COM in MTA as early as possible, before 3rd party
    /// libraries we use have done so (i.e. Qt).
#if 0
    /* If we fail to set the necessary apartment model, it may mean that some
     * DLL that was indirectly loaded by the process calling this function has
     * already initialized COM on the given thread in an incompatible way
     * which we can't leave with. Therefore, we try to fix this by using the
     * brute force method: */

    if (rc == RPC_E_CHANGED_MODE)
    {
        /* Before we use brute force, we need to check if we are in the
         * neutral threaded apartment -- in this case there is no need to
         * worry at all. */

        rc = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
        if (rc == RPC_E_CHANGED_MODE)
        {
            /* This is a neutral apartment, reset the error */
            rc = S_OK;

            LogFlowFunc (("COM is already initialized in neutral threaded "
                          "apartment mode,\nwill accept it.\n"));
        }
        else if (rc == S_FALSE)
        {
            /* balance the test CoInitializeEx above */
            CoUninitialize();
            rc = RPC_E_CHANGED_MODE;

            LogFlowFunc (("COM is already initialized in single threaded "
                          "apartment mode,\nwill reinitialize as "
                          "multi threaded.\n"));

            enum { MaxTries = 10000 };
            int tries = MaxTries;
            while (rc == RPC_E_CHANGED_MODE && tries --)
            {
                CoUninitialize();
                rc = CoInitializeEx (NULL, flags);
                if (rc == S_OK)
                {
                    /* We've successfully reinitialized COM; restore the
                     * initialization reference counter */

                    LogFlowFunc (("Will call CoInitializeEx() %d times.\n",
                                  MaxTries - tries));

                    while (tries ++ < MaxTries)
                    {
                        rc = CoInitializeEx (NULL, flags);
                        Assert (rc == S_FALSE);
                    }
                }
            }
        }
        else
            AssertMsgFailed (("rc=%08X\n", rc));
    }
#endif

    /* the overall result must be either S_OK or S_FALSE (S_FALSE means
     * "already initialized using the same apartment model") */
    AssertMsg (rc == S_OK || rc == S_FALSE, ("rc=%08X\n", rc));

#else /* !defined (VBOX_WITH_XPCOM) */

    if (ASMAtomicXchgBool (&gIsXPCOMInitialized, true) == true)
    {
        /* XPCOM is already initialized on the main thread, no special
         * initialization is necessary on additional threads. Just increase
         * the init counter if it's a main thread again (to correctly support
         * nested calls to Initialize()/Shutdown() for compatibility with
         * Win32). */

        nsCOMPtr <nsIEventQueue> eventQ;
        rc = NS_GetMainEventQ (getter_AddRefs (eventQ));

        if (NS_SUCCEEDED (rc))
        {
            PRBool isOnMainThread = PR_FALSE;
            rc = eventQ->IsOnCurrentThread (&isOnMainThread);
            if (NS_SUCCEEDED (rc) && isOnMainThread)
                ++ gXPCOMInitCount;
        }

        AssertComRC (rc);
        return rc;
    }

    /* this is the first initialization */
    gXPCOMInitCount = 1;

    /* prepare paths for registry files */
    char userHomeDir [RTPATH_MAX];
    int vrc = GetVBoxUserHomeDirectory (userHomeDir, sizeof (userHomeDir));
    AssertRCReturn (vrc, NS_ERROR_FAILURE);

    char compReg [RTPATH_MAX];
    char xptiDat [RTPATH_MAX];

    RTStrPrintf (compReg, sizeof (compReg), "%s%c%s",
                 userHomeDir, RTPATH_DELIMITER, "compreg.dat");
    RTStrPrintf (xptiDat, sizeof (xptiDat), "%s%c%s",
                 userHomeDir, RTPATH_DELIMITER, "xpti.dat");

    LogFlowFunc (("component registry  : \"%s\"\n", compReg));
    LogFlowFunc (("XPTI data file      : \"%s\"\n", xptiDat));

#if defined (XPCOM_GLUE)
    XPCOMGlueStartup (nsnull);
#endif

    const char *kAppPathsToProbe[] =
    {
        NULL, /* 0: will use VBOX_APP_HOME */
        NULL, /* 1: will try RTPathAppPrivateArch() */
#ifdef RT_OS_LINUX
        "/usr/lib/virtualbox",
        "/opt/VirtualBox",
#elif RT_OS_SOLARIS
        "/opt/VirtualBox/amd64",
        "/opt/VirtualBox/i386",
#elif RT_OS_DARWIN
        "/Application/VirtualBox.app/Contents/MacOS",
#endif
    };

    /* Find out the directory where VirtualBox binaries are located */
    for (size_t i = 0; i < RT_ELEMENTS (kAppPathsToProbe); ++ i)
    {
        char appHomeDir [RTPATH_MAX];
        appHomeDir [RTPATH_MAX - 1] = '\0';

        if (i == 0)
        {
            /* Use VBOX_APP_HOME if present */
            if (!RTEnvExist ("VBOX_APP_HOME"))
                continue;

            strncpy (appHomeDir, RTEnvGet ("VBOX_APP_HOME"), RTPATH_MAX - 1);
        }
        else if (i == 1)
        {
            /* Use RTPathAppPrivateArch() first */
            vrc = RTPathAppPrivateArch (appHomeDir, sizeof (appHomeDir));
            AssertRC (vrc);
            if (RT_FAILURE (vrc))
            {
                rc = NS_ERROR_FAILURE;
                continue;
            }
        }
        else
        {
            /* Iterate over all other paths */
            strncpy (appHomeDir, kAppPathsToProbe [i], RTPATH_MAX - 1);
        }

        nsCOMPtr <DirectoryServiceProvider> dsProv;

        char compDir [RTPATH_MAX];
        RTStrPrintf (compDir, sizeof (compDir), "%s%c%s",
                     appHomeDir, RTPATH_DELIMITER, "components");
        LogFlowFunc (("component directory : \"%s\"\n", compDir));

        dsProv = new DirectoryServiceProvider();
        if (dsProv)
            rc = dsProv->init (compReg, xptiDat, compDir, appHomeDir);
        else
            rc = NS_ERROR_OUT_OF_MEMORY;
        if (NS_FAILED (rc))
            break;

        /* Setup the application path for NS_InitXPCOM2. Note that we properly
         * answer the NS_XPCOM_CURRENT_PROCESS_DIR query in our directory
         * service provider but it seems to be activated after the directory
         * service is used for the first time (see the source NS_InitXPCOM2). So
         * use the same value here to be on the safe side. */
        nsCOMPtr <nsIFile> appDir;
        {
            char *appDirCP = NULL;
            vrc = RTStrUtf8ToCurrentCP (&appDirCP, appHomeDir);
            if (RT_SUCCESS (vrc))
            {
                nsCOMPtr <nsILocalFile> file;
                rc = NS_NewNativeLocalFile (nsEmbedCString (appDirCP),
                                            PR_FALSE, getter_AddRefs (file));
                if (NS_SUCCEEDED (rc))
                    appDir = do_QueryInterface (file, &rc);

                RTStrFree (appDirCP);
            }
            else
                rc = NS_ERROR_FAILURE;
        }
        if (NS_FAILED (rc))
            break;

        /* Set VBOX_XPCOM_HOME to the same app path to make XPCOM sources that
         * still use it instead of the directory service happy */
        {
            char *pathCP = NULL;
            vrc = RTStrUtf8ToCurrentCP (&pathCP, appHomeDir);
            if (RT_SUCCESS (vrc))
            {
                vrc = RTEnvSet ("VBOX_XPCOM_HOME", pathCP);
                RTStrFree (pathCP);
            }
            AssertRC (vrc);
        }

        /* Finally, initialize XPCOM */
        {
            nsCOMPtr <nsIServiceManager> serviceManager;
            rc = NS_InitXPCOM2 (getter_AddRefs (serviceManager),
                                appDir, dsProv);

            if (NS_SUCCEEDED (rc))
            {
                nsCOMPtr <nsIComponentRegistrar> registrar =
                    do_QueryInterface (serviceManager, &rc);
                if (NS_SUCCEEDED (rc))
                {
                    rc = registrar->AutoRegister (nsnull);
                    if (NS_SUCCEEDED (rc))
                    {
                        /* We succeeded, stop probing paths */
                        LogFlowFunc (("Succeeded.\n"));
                        break;
                    }
                }
            }
        }

        /* clean up before the new try */
        rc = NS_ShutdownXPCOM (nsnull);

        if (i == 0)
        {
            /* We failed with VBOX_APP_HOME, don't probe other paths */
            break;
        }
    }

#endif /* !defined (VBOX_WITH_XPCOM) */

    AssertComRC (rc);

    return rc;
}

HRESULT Shutdown()
{
    HRESULT rc = S_OK;

#if !defined (VBOX_WITH_XPCOM)

    CoUninitialize();

#else /* !defined (VBOX_WITH_XPCOM) */

    nsCOMPtr <nsIEventQueue> eventQ;
    rc = NS_GetMainEventQ (getter_AddRefs (eventQ));

    if (NS_SUCCEEDED (rc) || rc == NS_ERROR_NOT_AVAILABLE)
    {
        /* NS_ERROR_NOT_AVAILABLE seems to mean that
         * nsIEventQueue::StopAcceptingEvents() has been called (see
         * nsEventQueueService.cpp). We hope that this error code always means
         * just that in this case and assume that we're on the main thread
         * (it's a kind of unexpected behavior if a non-main thread ever calls
         * StopAcceptingEvents() on the main event queue). */

        PRBool isOnMainThread = PR_FALSE;
        if (NS_SUCCEEDED (rc))
        {
            rc = eventQ->IsOnCurrentThread (&isOnMainThread);
            eventQ = nsnull; /* early release before shutdown */
        }
        else
        {
            isOnMainThread = PR_TRUE;
            rc = NS_OK;
        }

        if (NS_SUCCEEDED (rc) && isOnMainThread)
        {
            /* only the main thread needs to uninitialize XPCOM and only if
             * init counter drops to zero */
            if (-- gXPCOMInitCount == 0)
            {
                rc = NS_ShutdownXPCOM (nsnull);

                /* This is a thread initialized XPCOM and set gIsXPCOMInitialized to
                 * true. Reset it back to false. */
                bool wasInited = ASMAtomicXchgBool (&gIsXPCOMInitialized, false);
                Assert (wasInited == true);
                NOREF (wasInited);

#if defined (XPCOM_GLUE)
                XPCOMGlueShutdown();
#endif
            }
        }
    }

#endif /* !defined (VBOX_WITH_XPCOM) */

    AssertComRC (rc);

    return rc;
}

} /* namespace com */
