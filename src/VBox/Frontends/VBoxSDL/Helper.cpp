/** @file
 *
 * VBox frontends: VBoxSDL (simple frontend based on SDL):
 * Miscellaneous helpers
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

#define LOG_GROUP LOG_GROUP_GUI
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include "VBoxSDL.h"
#include "Helper.h"


/**
 * Globals
 */


#ifdef USE_XPCOM_QUEUE_THREAD

/** global flag indicating that the event queue thread should terminate */
bool volatile   g_fTerminateXPCOMQueueThread = false;

/** Semaphore the XPCOM event thread will sleep on while it waits for the main thread to process pending requests. */
RTSEMEVENT    g_EventSemXPCOMQueueThread = NULL;

/**
 * Thread method to wait for XPCOM events and notify the SDL thread.
 *
 * @returns Error code
 * @param   thread  Thread ID
 * @param   pvUser  User specific parameter, the file descriptor
 *                  of the event queue socket
 */
DECLCALLBACK(int) xpcomEventThread(RTTHREAD thread, void *pvUser)
{
    int eqFD = (intptr_t)pvUser;
    unsigned cErrors = 0;
    int rc;

    /* Wait with the processing till the main thread needs it. */
    RTSemEventWait(g_EventSemXPCOMQueueThread, 2500);

    do
    {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(eqFD, &fdset);
        int n = select(eqFD + 1, &fdset, NULL, NULL, NULL);


        /* are there any events to process? */
        if ((n > 0) && !g_fTerminateXPCOMQueueThread)
        {
            /*
             * Post the event and wait for it to be processed. If we don't wait,
             * we'll flood the queue on SMP systems and when the main thread is busy.
             * In the event of a push error, we'll yield the timeslice and retry.
             */
            SDL_Event event = {0};
            event.type = SDL_USEREVENT;
            event.user.type = SDL_USER_EVENT_XPCOM_EVENTQUEUE;
            rc = SDL_PushEvent(&event);
            if (!rc)
            {
                RTSemEventWait(g_EventSemXPCOMQueueThread, 100);
                cErrors = 0;
            }
            else
            {
                cErrors++;
                if (!RTThreadYield())
                    RTThreadSleep(2);
                if (cErrors >= 10)
                    RTSemEventWait(g_EventSemXPCOMQueueThread, RT_MIN(cErrors - 8, 50));
            }
        }
    } while (!g_fTerminateXPCOMQueueThread);
    return VINF_SUCCESS;
}

/**
 * Creates the XPCOM event thread
 *
 * @returns VBOX status code
 * @param   eqFD XPCOM event queue file descriptor
 */
int startXPCOMEventQueueThread(int eqFD)
{
    int rc = RTSemEventCreate(&g_EventSemXPCOMQueueThread);
    if (VBOX_SUCCESS(rc))
    {
        RTTHREAD Thread;
        rc = RTThreadCreate(&Thread, xpcomEventThread, (void *)eqFD, 0, RTTHREADTYPE_MSG_PUMP, 0, "XPCOMEvent");
    }
    AssertRC(rc);
    return rc;
}

/**
 * Signal to the XPCOM even queue thread that it should select for more events.
 */
void signalXPCOMEventQueueThread(void)
{
    int rc = RTSemEventSignal(g_EventSemXPCOMQueueThread);
    AssertRC(rc);
}

/**
 * Indicates to the XPCOM thread that it should terminate now.
 */
void terminateXPCOMQueueThread(void)
{
    g_fTerminateXPCOMQueueThread = true;
    if (g_EventSemXPCOMQueueThread)
    {
        RTSemEventSignal(g_EventSemXPCOMQueueThread);
        RTThreadYield();
    }
}



#endif /* USE_XPCOM_QUEUE_THREAD */

