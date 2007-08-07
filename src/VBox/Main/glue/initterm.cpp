/** @file
 * MS COM / XPCOM Abstraction Layer - Initialization and Termination.
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

#if !defined (VBOX_WITH_XPCOM)

#include <objbase.h>

#else /* !defined (VBOX_WITH_XPCOM) */

#include <stdlib.h>

#include <nsXPCOMGlue.h>
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

#include <VBox/err.h>

#include "VBox/com/com.h"
#include "VBox/com/assert.h"


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

#endif /* defined (VBOX_WITH_XPCOM) */

HRESULT Initialize()
{
    HRESULT rc = E_FAIL;

#if !defined (VBOX_WITH_XPCOM)

    rc = CoInitializeEx (NULL, COINIT_MULTITHREADED |
                               COINIT_DISABLE_OLE1DDE |
                               COINIT_SPEED_OVER_MEMORY);

#else /* !defined (VBOX_WITH_XPCOM) */

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

    nsCOMPtr <nsIEventQueue> eventQ;
    rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
    if (rc == NS_ERROR_NOT_INITIALIZED)
    {
        XPCOMGlueStartup (nsnull);

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

            dsProv = new DirectoryServiceProvider();
            if (dsProv)
                rc = dsProv->init (compReg, xptiDat, compDir, privateArchDir);
            else
                rc = NS_ERROR_OUT_OF_MEMORY;
        }
        else
            rc = NS_ERROR_FAILURE;

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

        BOOL isOnMainThread = FALSE;
        if (NS_SUCCEEDED (rc))
        {
            rc = eventQ->IsOnCurrentThread (&isOnMainThread);
            eventQ = nsnull; /* early release */
        }
        else
        {
            isOnMainThread = TRUE;
            rc = NS_OK;
        }

        if (NS_SUCCEEDED (rc) && isOnMainThread)
        {
            /* only the main thread needs to uninitialize XPCOM */
            rc = NS_ShutdownXPCOM (nsnull);
            XPCOMGlueShutdown();
        }
    }

#endif /* !defined (VBOX_WITH_XPCOM) */

    AssertComRC (rc);

    return rc;
}

}; // namespace com
