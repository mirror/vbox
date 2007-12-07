/** @file
 * MS COM / XPCOM Abstraction Layer - Initialization and Termination.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/env.h>
#include <iprt/asm.h>

#include <VBox/err.h>

#include "VBox/com/com.h"
#include "VBox/com/assert.h"

#include "../include/Logging.h"

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
    /// libraries we use have done so (i.e. Qt3).
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

    /* Set VBOX_XPCOM_HOME if not present */
    if (!RTEnvExist ("VBOX_XPCOM_HOME"))
    {
        /* get the executable path */
        char pathProgram [RTPATH_MAX];
        int vrc = RTPathProgram (pathProgram, sizeof (pathProgram));
        if (RT_SUCCESS (vrc))
        {
            char *pathProgramCP = NULL;
            vrc = RTStrUtf8ToCurrentCP (&pathProgramCP, pathProgram);
            if (RT_SUCCESS (vrc))
            {
                vrc = RTEnvSet ("VBOX_XPCOM_HOME", pathProgramCP);
                RTStrFree (pathProgramCP);
            }
        }
        AssertRC (vrc);
    }

#if defined (XPCOM_GLUE)
    XPCOMGlueStartup (nsnull);
#endif

    nsCOMPtr <DirectoryServiceProvider> dsProv;

    /* prepare paths for registry files */
    char homeDir [RTPATH_MAX];
    char privateArchDir [RTPATH_MAX];
    int vrc = GetVBoxUserHomeDirectory (homeDir, sizeof (homeDir));
    if (RT_SUCCESS (vrc))
        vrc = RTPathAppPrivateArch (privateArchDir, sizeof (privateArchDir));
    if (RT_SUCCESS (vrc))
    {
        char compReg [RTPATH_MAX];
        char xptiDat [RTPATH_MAX];
        char compDir [RTPATH_MAX];

        RTStrPrintf (compReg, sizeof (compReg), "%s%c%s",
                     homeDir, RTPATH_DELIMITER, "compreg.dat");
        RTStrPrintf (xptiDat, sizeof (xptiDat), "%s%c%s",
                     homeDir, RTPATH_DELIMITER, "xpti.dat");
        RTStrPrintf (compDir, sizeof (compDir), "%s%c/components",
                     privateArchDir, RTPATH_DELIMITER);

        LogFlowFunc (("component registry  : \"%s\"\n", compReg));
        LogFlowFunc (("XPTI data file      : \"%s\"\n", xptiDat));
        LogFlowFunc (("component directory : \"%s\"\n", compDir));

        dsProv = new DirectoryServiceProvider();
        if (dsProv)
            rc = dsProv->init (compReg, xptiDat, compDir, privateArchDir);
        else
            rc = NS_ERROR_OUT_OF_MEMORY;
    }
    else
        rc = NS_ERROR_FAILURE;

    if (NS_SUCCEEDED (rc))
    {
        /* get the path to the executable */
        nsCOMPtr <nsIFile> appDir;
        {
            char path [RTPATH_MAX];
            char *appDirCP = NULL;
#if defined (DEBUG)
            const char *env = RTEnvGet ("VIRTUALBOX_APP_HOME");
            if (env)
            {
                char *appDirUtf8 = NULL;
                vrc = RTStrCurrentCPToUtf8 (&appDirUtf8, env);
                if (RT_SUCCESS (vrc))
                {
                    vrc = RTPathReal (appDirUtf8, path, RTPATH_MAX);
                    if (RT_SUCCESS (vrc))
                        vrc = RTStrUtf8ToCurrentCP (&appDirCP, appDirUtf8);
                    RTStrFree (appDirUtf8);
                }
            }
            else
#endif
            {
                vrc = RTPathProgram (path, RTPATH_MAX);
                if (RT_SUCCESS (vrc))
                    vrc = RTStrUtf8ToCurrentCP (&appDirCP, path);
            }

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

        /* Finally, initialize XPCOM */
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIServiceManager> serviceManager;
            rc = NS_InitXPCOM2 (getter_AddRefs (serviceManager),
                                appDir, dsProv);

            if (NS_SUCCEEDED (rc))
            {
                nsCOMPtr <nsIComponentRegistrar> registrar =
                    do_QueryInterface (serviceManager, &rc);
                if (NS_SUCCEEDED (rc))
                    registrar->AutoRegister (nsnull);
            }
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
