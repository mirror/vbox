/* $Id$ */
/** @file
 * VBoxService - Guest Additions Video Mode Hint Monitoring Service.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/**
 * Convenience macro for returning an error.
 * @todo It would be nice to have something which displayed the error more
 *       visibly to the user, say as a host notification.
 */
#define RETURN_ERROR(msg, val) \
do { \
    VBoxServiceError msg; \
    return (val); \
while(0)

#define ERROR_PREFIX "VBoxService/Display: "

#define MAX_CLIENTS 16

/** Structure for keeping track of previously received mode hints. */
struct videoMode
{
    int cx;              /** Width */
    int cy;              /** Height */
    int x;               /** X offset, -1 if none. */
    int y;               /** Y offset, -1 if none. */
    bool fEnabled;       /** Should this virtual screen be enabled? */
};

struct serviceState
{
    /** FDs of currently connected clients, empty slots set to -1. */
    int fdClients[MAX_CLIENTS];  /* Limit number of connections for now. */
    /** FD of the socket we listen to for connections */
    int fdSocket;
    /** Array of video mode hints received up to now or saved in previous runs.
     */
    struct videoMode *paLastModeHintsReceived;
    /** Size of paLastModeHintsReceived structure. */
    size_t cModeHints;
};

void initialiseServiceState(struct serviceState *pState)
{
    unsigned i;

    for (i = 0; i < MAX_CLIENTS; ++i)
        pState->fdClients[i] = -1;
    pState->fdSocket = -1;
    pState->paLastModeHintsReceived = NULL;
    pState->cModeHints = 0;
}

/** Set up a socket to pass on size hints to interested clients. */
static int setUpSocket(int *pfdSocket)
{
    struct sockaddr_un address;
    int fdSocket;
    socklen_t cbAddress;
b
    fdSocket = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fdSocket < 0)
        RETURN_ERROR((ERROR_PREFIX "failed to create a socket: %s\n",
                      strerror(errno), RTErrConvertFromErrno(errno));
    unlink(VBGLR3HOSTDISPSOCKET);  /* Catch relevant failures later. */
    RT_ZERO(address);
    mkdir(VBGLR3HOSTDISPSOCKETPATH, 0755);  /* Root-only later? */
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof(address.sun_path),
             VBGLR3HOSTDISPSOCKET);
    if (bind(fdSocket, (struct sockaddr *) &address,
             sizeof(struct sockaddr_un)) != 0)
        RETURN_ERROR((ERROR_PREFIX "Failed to bind socket: %s\n",
                      strerror(errno), RTErrConvertFromErrno(errno));
    if (listen(fdSocket, MAX_CLIENTS) != 0)
        RETURN_ERROR((ERROR_PREFIX "Failed to start listening to socket: %s\n",
                      strerror(errno), RTErrConvertFromErrno(errno));
    *pfdSocket = fdSocket;
    return VINF_SUCCESS;
}

/** Tell the VBoxGuest driver which events we want to listen out for. */
static int registerForHostEvents()
{
    int rc = VbglR3CtlFilterMask(  VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED
                                 | VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
        RETURN_ERROR((ERROR_PREFIX "failed to set filter mask, rc=%Rrc.\n", rc),
                     rc);
    return VINF_SUCCESS;
}

int setUpServiceState(struct serviceState *pState)
{
    int rc;

    rc = setUpSocket(&pState->fdSocket);
    if (RT_FAILURE(rc))
        return rc;
    rc = registerForHostEvents();
    if (RT_FAILURE(rc))
        return rc;
    rc = readSavedVideoModes(&pState->paLastModeHintsReceived);
    if (RT_FAILURE(rc))
        return rc;
    for (rc = VINF_SUCCESS; rc == VINF_SUCCESS;)
        rc = getVideoModeHintFromHost(pState, NULL);
    if (RT_FAILURE(rc))
        return rc;
    return VINF_SUCCESS;
}

int sendInitialSizeHintsToKernelDriver(struct serviceState *pState)
{
    unsigned iScreen;
    int rc;
    
    for (iScreen = 0; iScreen < pState->cModeHints; ++iScreen)
    {
        rc = sendModeHintToKernelDriver(pState, i);
        if (RT_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
}

int runMainServiceLoop(struct serviceState *pState, bool volatile *pfShutdown)
{
    int rc;

    for (;;)
    {
        uint32_t fEventsFromHost;

        if (RT_FAILURE(rc))
            return rc;
        /** @todo We really want to be able to poll for events and new clients
         *      in one system call. */
        rc = VbglR3WaitEvent(  VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST
                             | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED
                             | VMMDEV_EVENT_MOUSE_POSITION_CHANGED,
                             RT_INDEFINITE_WAIT, &fEventsFromHost);
        if (RT_FAILURE(rc) && rc != VERR_INTERRUPTED)
            RETURN_ERROR((ERROR_PREFIX "VbglR3WaitEvent returned %Rrc\n",
                          rc), rc);
        if (pfShutdown)
            return VINF_SUCCESS;
        rc = updateClientConnectionsAndSendInitialSizeHints(pState);
        if (RT_FAILURE(rc))
            return rc;
        rc = updateKernelDriverMasterState(pState);
        if (RT_FAILURE(rc))
            return rc;
        rc = updateGraphicsCapabilityAndTellHost(pState);
        /** @note We only pass on display change information to other user space
         *     processes, as it can't safely be polled by several processes. */
        if (fEventsFromHost & VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED)
        {
            rc = sendMouseCapabilitiesToKernelDriver(pState);
            if (RT_FAILURE(rc))
                return rc;
        }
        if (fEventsFromHost & VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
        {
            rc = sendMousePositionToKernelDriver(pState);
            if (RT_FAILURE(rc))
                return rc;
        }
        if (fEventsFromHost & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
        {
            while (rc == VINF_SUCCESS)
            {
                unsigned iScreen;

                rc = fetchNewModeHintFromHost(&paLastModeHintsReceived,
                                              &iScreen);
                if (RT_FAILURE(rc))
                    return rc;
                rc = sendModeHintToKernelDriver(pState, iScreen);
                if (RT_FAILURE(rc))
                    return rc;
                rc = sendModeHintToClients(pState, iScreen);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
    }
}

/** @copydoc VBOXSERVICE::pfnWorker */
static DECLCALLBACK(int) serviceWorker(bool volatile *pfShutdown)
{
    struct serviceState State;
    int rc;

    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    initialiseServiceState(&State);
    /** @todo Set guest capabilities and mouse status (doesn't need host
     *     cursor) when we support this in the kernel driver.  We should do
     *     this when any process acquires DRM_MASTER and release them when
     *     it is released.  We need a way of detecting this. */
    /** @note Update: sysfs attributes can be polled, and the kernel API
     *     sysfs_notify() releases the poll.
     *     http://lwn.net/Articles/174660/
     */
    rc = setUpServiceState(pState);
    if (RT_SUCCESS(rc))
        rc = sendInitialSizeHintsToKernelDriver(pState);
    if (RT_SUCCESS(rc))
        rc = runMainServiceLoop(pState);

    cleanUpServiceState(pState);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) serviceStop(void)
{
    VbglR3InterruptEventWaits();
    return;
}


/**
 * The 'DisplayService' service description.
 */
VBOXSERVICE g_Display =
{
    /* pszName. */
    "display",
    /* pszDescription. */
    "Connect display drivers to VMM device",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServiceDefaultPreInit,
    VBoxServiceDefaultOption,
    VBoxServiceDefaultInit,
    serviceWorker,
    serviceStop,
    VBoxServiceDefaultTerm
};
