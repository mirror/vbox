/* $Id$ */
/** @file
 * VBoxService - Memory Ballooning.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The balloon size. */
static uint32_t g_cMemBalloonChunks = 0;

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_MemBalloonEvent = NIL_RTSEMEVENTMULTI;

/** The array holding the R3 pointers of the balloon */
static void **g_pavBalloon = NULL;


/**
 * Adapt the R0 memory balloon by granting/reclaiming 1MB chunks to/from R0.
 *
 * returns IPRT status code.
 * @param   cNewChunks     The new number of 1MB chunks in the balloon.
 */
static int VBoxServiceBalloonSetUser(uint32_t cNewChunks)
{
    if (cNewChunks == g_cMemBalloonChunks)
        return VINF_SUCCESS;

    VBoxServiceVerbose(3, "VBoxServiceBalloonSetUser: cNewChunks=%u g_cMemBalloonChunks=%u\n", cNewChunks, g_cMemBalloonChunks);
    int rc = VINF_SUCCESS;
    if (cNewChunks > g_cMemBalloonChunks)
    {
        /* inflate */
        g_pavBalloon = (void**)RTMemRealloc(g_pavBalloon, cNewChunks * sizeof(void*));
        uint32_t i;
        for (i = g_cMemBalloonChunks; i < cNewChunks; i++)
        {
/** @todo r=bird: this isn't safe on linux. See suplibOsPageAlloc in
 *        SUPLib-linux.cpp. We should probably just fail outright here if
 *        linux, just in case...
 *        frank: To be more specific, the problem is fork(). */
            void *pv = RTMemPageAlloc(VMMDEV_MEMORY_BALLOON_CHUNK_SIZE);
            rc = VbglR3MemBalloonChange(pv, /* inflate=*/ true);
            if (RT_SUCCESS(rc))
            {
                g_pavBalloon[i] = pv;
#ifndef RT_OS_SOLARIS
                /*
                 * Protect against access by dangling pointers (ignore errors as it may fail).
                 * On Solaris it corrupts the address space leaving the process unkillable. This could
                 * perhaps be related to what the underlying segment driver does; currently just disable it.
                 */
                RTMemProtect(pv, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE, RTMEM_PROT_NONE);
#endif
                g_cMemBalloonChunks++;
            }
            else
            {
                RTMemPageFree(pv);
                break;
            }
        }
        VBoxServiceVerbose(3, "VBoxServiceBalloonSetUser: inflation complete. chunks=%u rc=%d\n", i, rc);
    }
    else
    {
        /* deflate */
        uint32_t i;
        for (i = g_cMemBalloonChunks; i-- > cNewChunks;)
        {
            void *pv = g_pavBalloon[i];
            rc = VbglR3MemBalloonChange(pv, /* inflate=*/ false);
            if (RT_SUCCESS(rc))
            {
#ifndef RT_OS_SOLARIS
                /* unprotect */
                RTMemProtect(pv, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
#endif
                RTMemPageFree(g_pavBalloon[i]);
                g_pavBalloon[i] = NULL;
                g_cMemBalloonChunks--;
            }
            else
                break;
            VBoxServiceVerbose(3, "VBoxServiceBalloonSetUser: deflation complete. chunks=%u rc=%d\n", i, rc);
        }
    }

    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceBalloonPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceBalloonOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    NOREF(ppszShort);
    NOREF(argc);
    NOREF(argv);
    NOREF(pi);
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceBalloonInit(void)
{
    VBoxServiceVerbose(3, "VBoxServiceBalloonInit\n");

    int rc = RTSemEventMultiCreate(&g_MemBalloonEvent);
    AssertRCReturn(rc, rc);

    g_cMemBalloonChunks = 0;
    uint32_t cNewChunks = 0;
    bool fHandleInR3;

    /* Check balloon size */
    rc = VbglR3MemBalloonRefresh(&cNewChunks, &fHandleInR3);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "VBoxMemBalloonInit: new balloon size %d MB (%s memory)\n",
                           cNewChunks, fHandleInR3 ? "R3" : "R0");
        if (fHandleInR3)
            rc = VBoxServiceBalloonSetUser(cNewChunks);
        else
            g_cMemBalloonChunks = cNewChunks;
    }
    else
        VBoxServiceVerbose(3, "VBoxMemBalloonInit: VbglR3MemBalloonRefresh failed with %Rrc\n", rc);

    /* We shouldn't fail here if ballooning isn't available. This can have several reasons,
     * for instance, host too old  (which is not that fatal). */
    return VINF_SUCCESS;
}


/**
 * Query the size of the memory balloon, given as a count of chunks.
 *
 * @returns The number of chunks (VMMDEV_MEMORY_BALLOON_CHUNK_SIZE).
 */
uint32_t VBoxServiceBalloonQueryChunks(void)
{
    return g_cMemBalloonChunks;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceBalloonWorker(bool volatile *pfShutdown)
{
    /* Start monitoring of the stat event change event. */
    int rc = VbglR3CtlFilterMask(VMMDEV_EVENT_BALLOON_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: VbglR3CtlFilterMask failed with %Rrc\n", rc);
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
            uint32_t cNewChunks;
            bool fHandleInR3;
            rc = VbglR3MemBalloonRefresh(&cNewChunks, &fHandleInR3);
            if (RT_SUCCESS(rc))
            {
                VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: new balloon size %d MB (%s memory)\n",
                                   cNewChunks, fHandleInR3 ? "R3" : "R0");
                if (fHandleInR3)
                {
                    rc = VBoxServiceBalloonSetUser(cNewChunks);
                    if (RT_FAILURE(rc))
                    {
                        VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: failed to set balloon size %d MB (%s memory)\n",
                                    cNewChunks, fHandleInR3 ? "R3" : "R0");
                    }
                    else
                        VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: successfully set requested balloon size %d.\n", cNewChunks);
                }
                else
                    g_cMemBalloonChunks = cNewChunks;
            }
            else
                VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: VbglR3MemBalloonRefresh failed with %Rrc\n", rc);
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
        VBoxServiceVerbose(3, "VBoxServiceBalloonWorker: VbglR3CtlFilterMask failed with %Rrc\n", rc);

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
