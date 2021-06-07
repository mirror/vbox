/* $Id$ */
/** @file
 * AudioTestService - Audio test execution server, Internal Header.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_AudioTestServiceInternal_h
#define VBOX_INCLUDED_SRC_Audio_AudioTestServiceInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/getopt.h>
#include <iprt/stream.h>

#include "AudioTestServiceProtocol.h"

RT_C_DECLS_BEGIN

/** Opaque ATS transport layer specific client data. */
typedef struct ATSTRANSPORTCLIENT *PATSTRANSPORTCLIENT;
typedef PATSTRANSPORTCLIENT *PPATSTRANSPORTCLIENT;

/** Opaque ATS transport specific instance data. */
typedef struct ATSTRANSPORTINST *PATSTRANSPORTINST;
typedef PATSTRANSPORTINST *PPATSTRANSPORTINST;

/**
 * Transport layer descriptor.
 */
typedef struct ATSTRANSPORT
{
    /** The name. */
    char            szName[16];
    /** The description. */
    const char     *pszDesc;

    /**
     * Initializes the transport layer.
     *
     * @returns IPRT status code.  On errors, the transport layer shall call
     *          RTMsgError to display the error details to the user.
     * @param   pThis               The transport instance.
     * @param   pszBindAddr         Bind address. Empty means any address.
     * @param   uBindPort           Bind port. If set to 0, ATS_DEFAULT_PORT is being used.
     */
    DECLR3CALLBACKMEMBER(int, pfnInit, (PATSTRANSPORTINST pThis, const char *pszBindAddr, uint32_t uBindPort));

    /**
     * Terminate the transport layer, closing and freeing resources.
     *
     * On errors, the transport layer shall call RTMsgError to display the error
     * details to the user.
     *
     * @param   pThis               The transport instance.
     */
    DECLR3CALLBACKMEMBER(void, pfnTerm, (PATSTRANSPORTINST pThis));

    /**
     * Waits for a new client to connect and returns the client specific data on
     * success.
     */
    DECLR3CALLBACKMEMBER(int, pfnWaitForConnect, (PATSTRANSPORTINST pThis, PPATSTRANSPORTCLIENT ppClientNew));

    /**
     * Polls for incoming packets.
     *
     * @returns true if there are pending packets, false if there isn't.
     * @param   pThis               The transport instance.
     * @param   pClient             The client to poll for data.
     */
    DECLR3CALLBACKMEMBER(bool, pfnPollIn, (PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient));

    /**
     * Adds any pollable handles to the poll set.
     *
     * @returns IPRT status code.
     * @param   pThis               The transport instance.
     * @param   hPollSet            The poll set to add them to.
     * @param   pClient             The transport client structure.
     * @param   idStart             The handle ID to start at.
     */
    DECLR3CALLBACKMEMBER(int, pfnPollSetAdd, (PATSTRANSPORTINST pThis, RTPOLLSET hPollSet, PATSTRANSPORTCLIENT pClient, uint32_t idStart));

    /**
     * Removes the given client frmo the given pollset.
     *
     * @returns IPRT status code.
     * @param   pThis               The transport instance.
     * @param   hPollSet            The poll set to remove from.
     * @param   pClient             The transport client structure.
     * @param   idStart             The handle ID to remove.
     */
    DECLR3CALLBACKMEMBER(int, pfnPollSetRemove, (PATSTRANSPORTINST pThis, RTPOLLSET hPollSet, PATSTRANSPORTCLIENT pClient, uint32_t idStart));

    /**
     * Receives an incoming packet.
     *
     * This will block until the data becomes available or we're interrupted by a
     * signal or something.
     *
     * @returns IPRT status code.  On error conditions other than VERR_INTERRUPTED,
     *          the current operation will be aborted when applicable.  When
     *          interrupted, the transport layer will store the data until the next
     *          receive call.
     *
     * @param   pThis               The transport instance.
     * @param   pClient             The transport client structure.
     * @param   ppPktHdr            Where to return the pointer to the packet we've
     *                              read.  This is allocated from the heap using
     *                              RTMemAlloc (w/ ATSPKT_ALIGNMENT) and must be
     *                              free by calling RTMemFree.
     */
    DECLR3CALLBACKMEMBER(int, pfnRecvPkt, (PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PPATSPKTHDR ppPktHdr));

    /**
     * Sends an outgoing packet.
     *
     * This will block until the data has been written.
     *
     * @returns IPRT status code.
     * @retval  VERR_INTERRUPTED if interrupted before anything was sent.
     *
     * @param   pThis               The transport instance.
     * @param   pClient             The transport client structure.
     * @param   pPktHdr             The packet to send.  The size is given by
     *                              aligning the size in the header by
     *                              ATSPKT_ALIGNMENT.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendPkt, (PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr));

    /**
     * Sends a babble packet and disconnects the client (if applicable).
     *
     * @param   pThis               The transport instance.
     * @param   pClient             The transport client structure.
     * @param   pPktHdr             The packet to send.  The size is given by
     *                              aligning the size in the header by
     *                              ATSPKT_ALIGNMENT.
     * @param   cMsSendTimeout      The send timeout measured in milliseconds.
     */
    DECLR3CALLBACKMEMBER(void, pfnBabble, (PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr, RTMSINTERVAL cMsSendTimeout));

    /**
     * Notification about a client HOWDY.
     *
     * @param   pThis               The transport instance.
     * @param   pClient             The transport client structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyHowdy, (PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient));

    /**
     * Notification about a client BYE.
     *
     * For connection oriented transport layers, it would be good to disconnect the
     * client at this point.
     *
     * @param   pThis               The transport instance.
     * @param   pClient             The transport client structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyBye, (PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient));

    /**
     * Notification about a REBOOT or SHUTDOWN.
     *
     * For connection oriented transport layers, stop listening for and
     * accepting at this point.
     *
     * @param   pThis               The transport instance.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyReboot, (PATSTRANSPORTINST pThis));

    /** Non-zero end marker. */
    uint32_t u32EndMarker;
} ATSTRANSPORT;
/** Pointer to a const transport layer descriptor. */
typedef const struct ATSTRANSPORT *PCATSTRANSPORT;


extern ATSTRANSPORT const g_TcpTransport;

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestServiceInternal_h */

