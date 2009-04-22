/* $Id$ */
/** @file VBoxXPCOMC.cpp
 * Utility functions to use with the C binding for XPCOM.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#define LOG_GROUP LOG_GROUP_MAIN
#include <nsMemory.h>
#include <nsIServiceManager.h>
#include <nsEventQueueUtils.h>

#include <iprt/string.h>
#include <iprt/env.h>
#include <VBox/log.h>

#include "VBoxCAPI_v2_5.h"
#include "VBox/com/com.h"
#include "VBox/version.h"

using namespace std;

static ISession            *Session        = NULL;
static IVirtualBox         *Ivirtualbox    = NULL;
static nsIComponentManager *manager        = NULL;
static nsIEventQueue       *eventQ         = NULL;

static void VBoxComUninitialize(void);

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
    {
        nsMemory::Free(ptr);
    }
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

    rc = NS_GetComponentManager (&manager);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get component manager! rc=%Rhrc\n", rc));
        VBoxComUninitialize();
        return;
    }

    rc = NS_GetMainEventQ (&eventQ);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get xpcom event queue! rc=%Rhrc\n", rc));
        VBoxComUninitialize();
        return;
    }

    rc = manager->CreateInstanceByContractID(NS_VIRTUALBOX_CONTRACTID,
                                             nsnull,
                                             virtualBoxIID,
                                             (void **)ppVirtualBox);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate VirtualBox object! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    Log(("Cbinding: IVirtualBox object created.\n"));

    rc = manager->CreateInstanceByContractID (NS_SESSION_CONTRACTID,
                                              nsnull,
                                              sessionIID,
                                              (void **)ppSession);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate Session object! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    Log(("Cbinding: ISession object created.\n"));

    /* Store ppSession & ppVirtualBox so that VBoxComUninitialize
     * can later take care of them while cleanup
     */
    Session     = *ppSession;
    Ivirtualbox = *ppVirtualBox;

}

static void
VBoxComInitializeV1(IVirtualBox **ppVirtualBox, ISession **ppSession)
{
    /* stub that always fails. */
    *ppVirtualBox = NULL;
    *ppSession = NULL;
}

static void
VBoxComUninitialize(void)
{
    if (Session)
        NS_RELEASE(Session);        // decrement refcount
    if (Ivirtualbox)
        NS_RELEASE(Ivirtualbox);    // decrement refcount
    if (eventQ)
        NS_RELEASE(eventQ);         // decrement refcount
    if (manager)
        NS_RELEASE(manager);        // decrement refcount
    com::Shutdown();
    Log(("Cbinding: Cleaned up the created IVirtualBox and ISession Objects.\n"));
}

static void
VBoxGetEventQueue(nsIEventQueue **eventQueue)
{
    *eventQueue = eventQ;
}

static uint32_t
VBoxVersion(void)
{
    uint32_t version = 0;

    version = (VBOX_VERSION_MAJOR * 1000 * 1000) + (VBOX_VERSION_MINOR * 1000) + (VBOX_VERSION_BUILD);

    return version;
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

        VBoxComInitialize,
        VBoxComUninitialize,

        VBoxComUnallocMem,
        VBoxUtf16Free,
        VBoxUtf8Free,

        VBoxUtf16ToUtf8,
        VBoxUtf8ToUtf16,

        VBoxGetEventQueue,

        VBOX_XPCOMC_VERSION
    };

    if ((uVersion & 0xffff0000U) == (VBOX_XPCOMC_VERSION & 0xffff0000U))
        return &s_Functions;

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
