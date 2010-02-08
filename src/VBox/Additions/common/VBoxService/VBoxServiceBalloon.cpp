/* $Id: $ */
/** @file
 * VBoxMemBalloon - Memory balloon notification.
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
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static uint32_t g_MemBalloonSize = 0;

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_MemBalloonEvent = NIL_RTSEMEVENTMULTI;

/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceBalloonPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceBalloonOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceBalloonInit(void)
{
    VBoxServiceVerbose(3, "VBoxServiceBalloonInit\n");

    int rc = RTSemEventMultiCreate(&g_MemBalloonEvent);
    AssertRCReturn(rc, rc);

    g_MemBalloonSize = 0;

    /* Check balloon size */
    rc = VbglR3MemBalloonRefresh(&g_MemBalloonSize);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "VBoxMemBalloonInit: new balloon size %d MB\n", g_MemBalloonSize);
    else
        VBoxServiceVerbose(3, "VBoxMemBalloonInit: VbglR3MemBalloonRefresh failed with %d\n", rc);

    return rc;
}


uint32_t VBoxServiceBalloonQuerySize()
{
    return g_MemBalloonSize;
}

/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceBalloonWorker(bool volatile *pfShutdown)
{
    int rc = VINF_SUCCESS;

    /* Start monitoring of the stat event change event. */
    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_BALLOON_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: VbglR3CtlFilterMask failed with %d\n", rc);
        return rc;
    }

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        uint32_t fEvents = 0;

        /* Check if an update interval change is pending. */
        rc = VbglR3WaitEvent(VMMDEV_EVENT_BALLOON_CHANGE_REQUEST, 0 /* no wait */, &fEvents);
        if (    RT_SUCCESS(rc)
            &&  (fEvents & VMMDEV_EVENT_BALLOON_CHANGE_REQUEST))
        {
            rc = VbglR3MemBalloonRefresh(&g_MemBalloonSize);
            if (RT_SUCCESS(rc))
                VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: new balloon size %d MB\n", g_MemBalloonSize);
            else
                VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: VbglR3MemBalloonRefresh failed with %d\n", rc);
        }

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_MemBalloonEvent, 5000);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    /* Cancel monitoring of the memory balloon change event. */
    rc = VbglR3CtlFilterMask(0, VMMDEV_EVENT_BALLOON_CHANGE_REQUEST);
    if (RT_FAILURE(rc))
        VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: VbglR3CtlFilterMask failed with %d\n", rc);

    RTSemEventMultiDestroy(g_MemBalloonEvent);
    g_MemBalloonEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: finished mem balloon change request thread\n");
    return 0;
}

/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceBalloonTerm(void)
{
    VBoxServiceVerbose(3, "VBoxServiceBalloonTerm\n");
    return;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceBalloonStop(void)
{
    RTSemEventMultiSignal(g_MemBalloonEvent);
}


/**
 * The 'memballoon' service description.
 */
VBOXSERVICE g_MemBalloon =
{
    /* pszName. */
    "memballoon",
    /* pszDescription. */
    "Memory Ballooning",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServiceBalloonPreInit,
    VBoxServiceBalloonOption,
    VBoxServiceBalloonInit,
    VBoxServiceBalloonWorker,
    VBoxServiceBalloonStop,
    VBoxServiceBalloonTerm
};
