/* $Id$ */
/** @file
 * VBoxCredPoller - Thread for querying / retrieving user credentials.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <windows.h>

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/VMMDev.h>
#include <iprt/string.h>

#include "VBoxCredProvProvider.h"

#include "VBoxCredProvCredential.h"
#include "VBoxCredProvPoller.h"
#include "VBoxCredProvUtils.h"


VBoxCredProvPoller::VBoxCredProvPoller(void) :
    m_hThreadPoller(NIL_RTTHREAD),
    m_pProv(NULL)
{
}


VBoxCredProvPoller::~VBoxCredProvPoller(void)
{
    Shutdown();
}


int VBoxCredProvPoller::Initialize(VBoxCredProvProvider *pProvider)
{
    AssertPtrReturn(pProvider, VERR_INVALID_POINTER);

    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Initializing\n");

    /* Don't create more than one of them. */
    if (m_hThreadPoller != NIL_RTTHREAD)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvPoller: Thread already running, returning!\n");
        return VINF_SUCCESS;
    }

    if (m_pProv != NULL)
        m_pProv->Release();
    m_pProv = pProvider;
    AssertPtr(m_pProv);

    /* Create the poller thread. */
    int rc = RTThreadCreate(&m_hThreadPoller, VBoxCredProvPoller::threadPoller, this, 0, RTTHREADTYPE_INFREQUENT_POLLER,
                            RTTHREADFLAGS_WAITABLE, "credpoll");
    if (RT_FAILURE(rc))
        VBoxCredProvVerbose(0, "VBoxCredProvPoller::Initialize: Failed to create thread, rc=%Rrc\n", rc);

    return rc;
}


int VBoxCredProvPoller::Shutdown(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Shutdown\n");

    if (m_hThreadPoller == NIL_RTTHREAD)
        return VINF_SUCCESS;

    /* Post termination event semaphore. */
    int rc = RTThreadUserSignal(m_hThreadPoller);
    if (RT_SUCCESS(rc))
    {
        VBoxCredProvVerbose(0, "VBoxCredProvPoller: Waiting for thread to terminate\n");
        /* Wait until the thread has terminated. */
        rc = RTThreadWait(m_hThreadPoller, RT_INDEFINITE_WAIT, NULL);
        VBoxCredProvVerbose(0, "VBoxCredProvPoller: Thread has (probably) terminated (rc=%Rrc)\n", rc);
    }
    else
    {
        /* Failed to signal the thread - very unlikely - so no point in waiting long. */
        VBoxCredProvVerbose(0, "VBoxCredProvPoller::Shutdown: Failed to signal semaphore, rc=%Rrc\n", rc);
        rc = RTThreadWait(m_hThreadPoller, 100, NULL);
        VBoxCredProvVerbose(0, "VBoxCredProvPoller::Shutdown: Thread has terminated? Wait rc=%Rrc\n", rc);
    }

    m_hThreadPoller = NIL_RTTHREAD;
    return rc;
}


DECLCALLBACK(int) VBoxCredProvPoller::threadPoller(RTTHREAD ThreadSelf, void *pvUser)
{
    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Starting, pvUser=0x%p\n", pvUser);

    VBoxCredProvPoller *pThis = (VBoxCredProvPoller*)pvUser;
    AssertPtr(pThis);

    for (;;)
    {
        int rc;
        rc = VbglR3CredentialsQueryAvailability();
        if (RT_FAILURE(rc))
        {
            if (rc != VERR_NOT_FOUND)
                VBoxCredProvVerbose(0, "VBoxCredProvPoller: Could not retrieve credentials! rc=%Rc\n", rc);
        }
        else
        {
            VBoxCredProvVerbose(0, "VBoxCredProvPoller: Credentials available, notifying provider\n");

            if (pThis->m_pProv)
                pThis->m_pProv->OnCredentialsProvided();
        }

        /* Wait a bit. */
        if (RTThreadUserWait(ThreadSelf, 500) == VINF_SUCCESS)
        {
            VBoxCredProvVerbose(0, "VBoxCredProvPoller: Terminating\n");
            break;
        }
    }

    return VINF_SUCCESS;
}

