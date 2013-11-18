/* $Id$ */
/** @file VBoxXPCOMC.cpp
 * Utility functions to use with the C binding for XPCOM.
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN
#include <nsMemory.h>
#include <nsIServiceManager.h>
#include <nsEventQueueUtils.h>
#include <nsIExceptionService.h>

#include <iprt/string.h>
#include <iprt/env.h>
#include <VBox/log.h>

#include "VBoxCAPI.h"
#include "VBox/com/com.h"
#include "VBox/version.h"

using namespace std;

/* The following 3 object references should be eliminated once the legacy
 * way to initialize the XPCOM C bindings is removed. */
static ISession            *g_Session           = NULL;
static IVirtualBox         *g_VirtualBox        = NULL;
static nsIComponentManager *g_Manager           = NULL;

static nsIEventQueue       *g_EventQueue        = NULL;

static void VBoxComUninitialize(void);
static void VBoxClientUninitialize(void);

static int
VBoxUtf16ToUtf8(const PRUnichar *pwszString, char **ppszString)
{
    return RTUtf16ToUtf8(pwszString, ppszString);
}

static int
VBoxUtf8ToUtf16(const char *pszString, PRUnichar **ppwszString)
{
    return RTStrToUtf16(pszString, ppwszString);
}

static void
VBoxUtf16Free(PRUnichar *pwszString)
{
    RTUtf16Free(pwszString);
}

static void
VBoxUtf8Free(char *pszString)
{
    RTStrFree(pszString);
}

static void
VBoxComUnallocMem(void *ptr)
{
    if (ptr)
        nsMemory::Free(ptr);
}

static void
VBoxComInitialize(const char *pszVirtualBoxIID, IVirtualBox **ppVirtualBox,
                  const char *pszSessionIID, ISession **ppSession)
{
    nsresult rc;
    nsID virtualBoxIID;
    nsID sessionIID;

    *ppSession    = NULL;
    *ppVirtualBox = NULL;

    /* convert the string representation of UUID to nsIID type */
    if (!(virtualBoxIID.Parse(pszVirtualBoxIID) && sessionIID.Parse(pszSessionIID)))
        return;

    rc = com::Initialize();
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: XPCOM could not be initialized! rc=%Rhrc\n", rc));
        VBoxComUninitialize();
        return;
    }

    rc = NS_GetComponentManager(&g_Manager);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get component manager! rc=%Rhrc\n", rc));
        VBoxComUninitialize();
        return;
    }

    rc = NS_GetMainEventQ(&g_EventQueue);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get xpcom event queue! rc=%Rhrc\n", rc));
        VBoxComUninitialize();
        return;
    }

    rc = g_Manager->CreateInstanceByContractID(NS_VIRTUALBOX_CONTRACTID,
                                               nsnull,
                                               virtualBoxIID,
                                               (void **)&g_VirtualBox);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate VirtualBox object! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    Log(("Cbinding: IVirtualBox object created.\n"));

    rc = g_Manager->CreateInstanceByContractID(NS_SESSION_CONTRACTID,
                                               nsnull,
                                               sessionIID,
                                               (void **)&g_Session);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate Session object! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    Log(("Cbinding: ISession object created.\n"));

    *ppSession = g_Session;
    *ppVirtualBox = g_VirtualBox;
}

static void
VBoxComInitializeV1(IVirtualBox **ppVirtualBox, ISession **ppSession)
{
    VBoxComInitialize(IVIRTUALBOX_IID_STR, ppVirtualBox,
                      ISESSION_IID_STR, ppSession);
}

static void
VBoxComUninitialize(void)
{
    if (g_Session)
    {
        NS_RELEASE(g_Session);
        g_Session = NULL;
    }
    if (g_VirtualBox)
    {
        NS_RELEASE(g_VirtualBox);
        g_VirtualBox = NULL;
    }
    if (g_EventQueue)
    {
        NS_RELEASE(g_EventQueue);
        g_EventQueue = NULL;
    }
    if (g_Manager)
    {
        NS_RELEASE(g_Manager);
        g_Manager = NULL;
    }
    com::Shutdown();
    Log(("Cbinding: Cleaned up the created objects.\n"));
}

static void
VBoxGetEventQueue(nsIEventQueue **ppEventQueue)
{
    *ppEventQueue = g_EventQueue;
}

static nsresult
VBoxGetException(nsIException **ppException)
{
    nsresult rc;

    *ppException = NULL;
    nsIServiceManager *mgr = NULL;
    rc = NS_GetServiceManager(&mgr);
    if (NS_FAILED(rc) || !mgr)
        return rc;

    nsIID esid = NS_IEXCEPTIONSERVICE_IID;
    nsIExceptionService *es = NULL;
    rc = mgr->GetServiceByContractID(NS_EXCEPTIONSERVICE_CONTRACTID, esid, (void **)&es);
    if (NS_FAILED(rc) || !es)
    {
        NS_RELEASE(mgr);
        return rc;
    }

    nsIExceptionManager *em;
    rc = es->GetCurrentExceptionManager(&em);
    if (NS_FAILED(rc) || !em)
    {
        NS_RELEASE(es);
        NS_RELEASE(mgr);
        return rc;
    }

    nsIException *ex;
    rc = em->GetCurrentException(&ex);
    if (NS_FAILED(rc))
    {
        NS_RELEASE(em);
        NS_RELEASE(es);
        NS_RELEASE(mgr);
        return rc;
    }

    *ppException = ex;
    NS_RELEASE(em);
    NS_RELEASE(es);
    NS_RELEASE(mgr);
    return rc;
}

static nsresult
VBoxClearException(void)
{
    nsresult rc;

    nsIServiceManager *mgr = NULL;
    rc = NS_GetServiceManager(&mgr);
    if (NS_FAILED(rc) || !mgr)
        return rc;

    nsIID esid = NS_IEXCEPTIONSERVICE_IID;
    nsIExceptionService *es = NULL;
    rc = mgr->GetServiceByContractID(NS_EXCEPTIONSERVICE_CONTRACTID, esid, (void **)&es);
    if (NS_FAILED(rc) || !es)
    {
        NS_RELEASE(mgr);
        return rc;
    }

    nsIExceptionManager *em;
    rc = es->GetCurrentExceptionManager(&em);
    if (NS_FAILED(rc) || !em)
    {
        NS_RELEASE(es);
        NS_RELEASE(mgr);
        return rc;
    }

    rc = em->SetCurrentException(NULL);
    NS_RELEASE(em);
    NS_RELEASE(es);
    NS_RELEASE(mgr);
    return rc;
}

static nsresult
VBoxClientInitialize(const char *pszVirtualBoxClientIID, IVirtualBoxClient **ppVirtualBoxClient)
{
    nsresult rc;
    nsID virtualBoxClientIID;
    nsID sessionIID;

    *ppVirtualBoxClient = NULL;

    /* convert the string representation of UUID to nsIID type */
    if (!virtualBoxClientIID.Parse(pszVirtualBoxClientIID))
        return NS_ERROR_INVALID_ARG;

    rc = com::Initialize();
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: XPCOM could not be initialized! rc=%Rhrc\n", rc));
        VBoxClientUninitialize();
        return rc;
    }

    nsIComponentManager *pManager;
    rc = NS_GetComponentManager(&pManager);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get component manager! rc=%Rhrc\n", rc));
        VBoxClientUninitialize();
        return rc;
    }

    rc = NS_GetMainEventQ(&g_EventQueue);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get xpcom event queue! rc=%Rhrc\n", rc));
        VBoxClientUninitialize();
        return rc;
    }

    rc = pManager->CreateInstanceByContractID(NS_VIRTUALBOXCLIENT_CONTRACTID,
                                              nsnull,
                                              virtualBoxClientIID,
                                              (void **)ppVirtualBoxClient);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate VirtualBoxClient object! rc=%Rhrc\n",rc));
        VBoxClientUninitialize();
        return rc;
    }

    NS_RELEASE(pManager);
    pManager = NULL;

    Log(("Cbinding: IVirtualBoxClient object created.\n"));

    return NS_OK;
}

static void
VBoxClientUninitialize(void)
{
    if (g_EventQueue)
    {
        NS_RELEASE(g_EventQueue);
        g_EventQueue = NULL;
    }
    com::Shutdown();
    Log(("Cbinding: Cleaned up the created objects.\n"));
}

static unsigned int
VBoxVersion(void)
{
    return VBOX_VERSION_MAJOR * 1000 * 1000 + VBOX_VERSION_MINOR * 1000 + VBOX_VERSION_BUILD;
}

static unsigned int
VBoxAPIVersion(void)
{
    return VBOX_VERSION_MAJOR * 1000 + VBOX_VERSION_MINOR + (VBOX_VERSION_BUILD > 50 ? 1 : 0);
}

VBOXXPCOMC_DECL(PCVBOXXPCOM)
VBoxGetXPCOMCFunctions(unsigned uVersion)
{
    /*
     * The current interface version.
     */
    static const VBOXXPCOMC s_Functions =
    {
        sizeof(VBOXXPCOMC),
        VBOX_XPCOMC_VERSION,

        VBoxVersion,
        VBoxAPIVersion,

        VBoxClientInitialize,
        VBoxClientUninitialize,

        VBoxComInitialize,
        VBoxComUninitialize,

        VBoxComUnallocMem,

        VBoxUtf16ToUtf8,
        VBoxUtf8ToUtf16,
        VBoxUtf8Free,
        VBoxUtf16Free,

        VBoxGetEventQueue,
        VBoxGetException,
        VBoxClearException,

        VBOX_XPCOMC_VERSION
    };

    if ((uVersion & 0xffff0000U) == (VBOX_XPCOMC_VERSION & 0xffff0000U))
        return &s_Functions;

    /*
     * Legacy interface version 2.0.
     */
    static const struct VBOXXPCOMCV2
    {
        /** The size of the structure. */
        unsigned cb;
        /** The structure version. */
        unsigned uVersion;

        unsigned int (*pfnGetVersion)(void);

        void  (*pfnComInitialize)(const char *pszVirtualBoxIID,
                                  IVirtualBox **ppVirtualBox,
                                  const char *pszSessionIID,
                                  ISession **ppSession);

        void  (*pfnComUninitialize)(void);

        void  (*pfnComUnallocMem)(void *pv);
        void  (*pfnUtf16Free)(PRUnichar *pwszString);
        void  (*pfnUtf8Free)(char *pszString);

        int   (*pfnUtf16ToUtf8)(const PRUnichar *pwszString, char **ppszString);
        int   (*pfnUtf8ToUtf16)(const char *pszString, PRUnichar **ppwszString);

        void  (*pfnGetEventQueue)(nsIEventQueue **ppEventQueue);

        /** Tail version, same as uVersion. */
        unsigned uEndVersion;
    } s_Functions_v2_0 =
    {
        sizeof(s_Functions_v2_0),
        0x00020000U,

        VBoxVersion,

        VBoxComInitialize,
        VBoxComUninitialize,

        VBoxComUnallocMem,
        VBoxUtf16Free,
        VBoxUtf8Free,

        VBoxUtf16ToUtf8,
        VBoxUtf8ToUtf16,

        VBoxGetEventQueue,

        0x00020000U
    };

    if ((uVersion & 0xffff0000U) == 0x00020000U)
        return (PCVBOXXPCOM)&s_Functions_v2_0;

    /*
     * Legacy interface version 1.0.
     */
    static const struct VBOXXPCOMCV1
    {
        /** The size of the structure. */
        unsigned cb;
        /** The structure version. */
        unsigned uVersion;

        unsigned int (*pfnGetVersion)(void);

        void  (*pfnComInitialize)(IVirtualBox **virtualBox, ISession **session);
        void  (*pfnComUninitialize)(void);

        void  (*pfnComUnallocMem)(void *pv);
        void  (*pfnUtf16Free)(PRUnichar *pwszString);
        void  (*pfnUtf8Free)(char *pszString);

        int   (*pfnUtf16ToUtf8)(const PRUnichar *pwszString, char **ppszString);
        int   (*pfnUtf8ToUtf16)(const char *pszString, PRUnichar **ppwszString);

        /** Tail version, same as uVersion. */
        unsigned uEndVersion;
    } s_Functions_v1_0 =
    {
        sizeof(s_Functions_v1_0),
        0x00010000U,

        VBoxVersion,

        VBoxComInitializeV1,
        VBoxComUninitialize,

        VBoxComUnallocMem,
        VBoxUtf16Free,
        VBoxUtf8Free,

        VBoxUtf16ToUtf8,
        VBoxUtf8ToUtf16,

        0x00010000U
    };

    if ((uVersion & 0xffff0000U) == 0x00010000U)
        return (PCVBOXXPCOM)&s_Functions_v1_0;

    /*
     * Unsupported interface version.
     */
    return NULL;
}

/* vim: set ts=4 sw=4 et: */
