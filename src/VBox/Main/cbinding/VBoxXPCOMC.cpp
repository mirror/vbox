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

#include "VirtualBox_XPCOM.h"
#include "VirtualBox_CXPCOM.h"
#include "VBox/com/com.h"

using namespace std;

static ISession            *Session;
static IVirtualBox         *Ivirtualbox;
static nsIServiceManager   *serviceManager;
static nsIComponentManager *manager;

void VBoxComUninitialize(void);

int
VBoxUtf16ToUtf8(const PRUnichar *pwszString, char **ppszString)
{
    return RTUtf16ToUtf8(pwszString, ppszString);
}

int
VBoxUtf8ToUtf16(const char *pszString, PRUnichar **ppwszString)
{
    return RTStrToUtf16(pszString, ppwszString);
}

void
VBoxUtf16Free(PRUnichar *pwszString)
{
    RTUtf16Free(pwszString);
}

void
VBoxUtf8Free(char *pszString)
{
    RTStrFree(pszString);
}

void
VBoxComUnallocMem(void *ptr)
{
    if (ptr)
    {
        nsMemory::Free(ptr);
    }
}

int
VBoxSetEnv(const char *pszVar, const char *pszValue)
{
    return RTEnvSet(pszVar, pszValue);
}

const char *
VBoxGetEnv(const char *pszVar)
{
    return RTEnvGet(pszVar);
}

void
VBoxComInitialize(IVirtualBox **virtualBox, ISession **session)
{
    nsresult rc;

    *session    = NULL;
    *virtualBox = NULL;

    Session     = *session;
    Ivirtualbox = *virtualBox;

    rc = com::Initialize();
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: XPCOM could not be initialized! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    rc = NS_GetComponentManager (&manager);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not get component manager! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    rc = manager->CreateInstanceByContractID(NS_VIRTUALBOX_CONTRACTID,
                                             nsnull,
                                             NS_GET_IID(IVirtualBox),
                                             (void **)virtualBox);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate VirtualBox object! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    Log(("Cbinding: IVirtualBox object created.\n"));

    rc = manager->CreateInstanceByContractID (NS_SESSION_CONTRACTID,
                                              nsnull,
                                              NS_GET_IID(ISession),
                                              (void **)session);
    if (NS_FAILED(rc))
    {
        Log(("Cbinding: Could not instantiate Session object! rc=%Rhrc\n",rc));
        VBoxComUninitialize();
        return;
    }

    Log(("Cbinding: ISession object created.\n"));
}

void
VBoxComUninitialize(void)
{
    if (Session)
        NS_RELEASE(Session);        // decrement refcount
    if (Ivirtualbox)
        NS_RELEASE(Ivirtualbox);    // decrement refcount
    if (manager)
        NS_RELEASE(manager);        // decrement refcount
    if (serviceManager)
        NS_RELEASE(serviceManager); // decrement refcount
    com::Shutdown();
    Log(("Cbinding: Cleaned up the created IVirtualBox and ISession Objects.\n"));
}


VBOXXPCOMC_DECL(PCVBOXXPCOM)
VBoxGetXPCOMCFunctions(unsigned uVersion)
{
    /* The current version. */
    static const VBOXXPCOMC s_Functions =
    {
        sizeof(VBOXXPCOMC),
        VBOX_XPCOMC_VERSION,

        VBoxComInitialize,
        VBoxComUninitialize,

        VBoxComUnallocMem,
        VBoxUtf16Free,
        VBoxUtf8Free,

        VBoxUtf16ToUtf8,
        VBoxUtf8ToUtf16,

        VBoxGetEnv,
        VBoxSetEnv,

        VBOX_XPCOMC_VERSION
    };

    if ((uVersion & 0xffff0000U) != VBOX_XPCOMC_VERSION)
        return NULL; /* not supported. */

    return &s_Functions;
}

/* vim: set ts=4 sw=4 et: */
