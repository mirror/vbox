/** @file
 *
 * MS COM / XPCOM Abstraction Layer
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#if defined (__WIN__)

#include <objbase.h>

#else // !defined (__WIN__)

#include <stdlib.h>
#include <VBox/err.h>
#include <iprt/path.h>

#include <nsXPCOMGlue.h>
#include <nsIComponentRegistrar.h>
#include <nsIServiceManager.h>
#include <nsCOMPtr.h>
#include <nsEventQueueUtils.h>

#endif // !defined (__WIN__)

#include "VBox/com/com.h"
#include "VBox/com/assert.h"

namespace com
{

HRESULT Initialize()
{
    HRESULT rc = E_FAIL;

#if defined (__WIN__)
    rc = CoInitializeEx (NULL, COINIT_MULTITHREADED |
                               COINIT_DISABLE_OLE1DDE |
                               COINIT_SPEED_OVER_MEMORY);
#else
    /*
     * Set VBOX_XPCOM_HOME if not present
     */
    if (!getenv("VBOX_XPCOM_HOME"))
    {
        /* get the executable path */
        char szPathProgram[1024];
        int rcVBox = RTPathProgram(szPathProgram, sizeof(szPathProgram));
        if (VBOX_SUCCESS(rcVBox))
        {
            setenv("VBOX_XPCOM_HOME", szPathProgram, 1);
        }
    }

    nsCOMPtr <nsIEventQueue> eventQ;
    rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
    if (rc == NS_ERROR_NOT_INITIALIZED)
    {
        XPCOMGlueStartup (nsnull);
        nsCOMPtr <nsIServiceManager> serviceManager;
        rc = NS_InitXPCOM2 (getter_AddRefs (serviceManager), nsnull, nsnull);
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIComponentRegistrar> registrar =
                do_QueryInterface (serviceManager, &rc);
            if (NS_SUCCEEDED (rc))
                registrar->AutoRegister (nsnull);
        }
    }
#endif

    AssertComRC (rc);

    return rc;
}

void Shutdown()
{
#if defined (__WIN__)
    CoUninitialize();
#else
    nsCOMPtr <nsIEventQueue> eventQ;
    nsresult rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
    if (NS_SUCCEEDED (rc))
    {
        BOOL isOnMainThread = FALSE;
        eventQ->IsOnCurrentThread (&isOnMainThread);
        eventQ = nsnull; // early release
        if (isOnMainThread)
        {
            // only the main thread needs to uninitialize XPCOM
            NS_ShutdownXPCOM (nsnull);
            XPCOMGlueShutdown();
        }
    }
#endif
}

}; // namespace com
