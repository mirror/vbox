/* $Id$ */
/** @file
 * AudioTestService - Audio test execution server, Public Header.
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

#ifndef VBOX_INCLUDED_SRC_Audio_AudioTestService_h
#define VBOX_INCLUDED_SRC_Audio_AudioTestService_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "AudioTestServiceInternal.h"


/** Default TCP/IP port the host ATS (Audio Test Service) is running on. */
#define ATS_TCP_HOST_DEFAULT_PORT        6052
/** Default TCP/IP address the host ATS (Audio Test Service) is running on. */
#define ATS_TCP_HOST_DEFAULT_ADDR_STR    "127.0.0.1"
/** Default TCP/IP port the guest ATS (Audio Test Service) is running on. */
#define ATS_TCP_GUEST_DEFAULT_PORT       6042

/**
 * Structure for keeping an Audio Test Service (ATS) callback table.
 */
typedef struct ATSCALLBACKS
{
    /**
     * Begins a test set. Optional.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pszTag          Tag of test set to begin.
     */
    DECLR3CALLBACKMEMBER(int, pfnTestSetBegin, (void const *pvUser, const char *pszTag));

    /**
     * Ends the current test set. Optional.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pszTag          Tag of test set to end.
     */
    DECLR3CALLBACKMEMBER(int, pfnTestSetEnd,   (void const *pvUser, const char *pszTag));

    /**
     * Marks the begin of sending a test set. Optional.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pszTag          Tag of test set to begin sending.
     */
    DECLR3CALLBACKMEMBER(int, pfnTestSetSendBegin, (void const *pvUser, const char *pszTag));

    /**
     * Reads data from a test set for sending it.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pszTag          Tag of test set to begin sending.
     * @param   pvBuf           Where to store the read test set data.
     * @param   cbBuf           Size of \a pvBuf (in bytes).
     * @param   pcbRead         Where to return the amount of read data in bytes. Optional and can be NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnTestSetSendRead,  (void const *pvUser, const char *pszTag, void *pvBuf, size_t cbBuf, size_t *pcbRead));

    /**
     * Marks the end of sending a test set. Optional.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pszTag          Tag of test set to end sending.
     */
    DECLR3CALLBACKMEMBER(int, pfnTestSetSendEnd,   (void const *pvUser, const char *pszTag));

    /**
     * Plays a test tone.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pToneParms      Tone parameters to use for playback.
     */
    DECLR3CALLBACKMEMBER(int, pfnTonePlay, (void const *pvUser, PAUDIOTESTTONEPARMS pToneParms));

    /**
     * Records a test tone.
     *
     * @returns VBox status code.
     * @param   pvUser          User-supplied pointer to context data. Optional.
     * @param   pToneParms      Tone parameters to use for recording.
     */
    DECLR3CALLBACKMEMBER(int, pfnToneRecord, (void const *pvUser, PAUDIOTESTTONEPARMS pToneParms));

    /** Pointer to opaque user-provided context data. */
    void const *pvUser;
} ATSCALLBACKS;
/** Pointer to a const ATS callbacks table. */
typedef const struct ATSCALLBACKS *PCATSCALLBACKS;

/**
 * Structure for keeping Audio Test Service (ATS) transport instance-specific data.
 *
 * Currently only TCP/IP is supported.
 */
typedef struct ATSTRANSPORTINST
{
    /** The addresses to bind to.  Empty string means any. */
    char                 szTcpBindAddr[256];
    /** The TCP port to listen to. */
    uint32_t             uTcpBindPort;
    /** Pointer to the TCP server instance. */
    PRTTCPSERVER         pTcpServer;
} ATSTRANSPORTINST;
/** Pointer to an Audio Test Service (ATS) TCP/IP transport instance. */
typedef ATSTRANSPORTINST *PATSTRANSPORTINSTTCP;

/**
 * Structure for keeping an Audio Test Service (ATS) instance.
 */
typedef struct ATSSERVER
{
    /** The selected transport layer. */
    PCATSTRANSPORT       pTransport;
    /** The transport instance. */
    ATSTRANSPORTINST     TransportInst;
    /** The callbacks table. */
    ATSCALLBACKS         Callbacks;
    /** Whether server is in started state or not. */
    bool volatile        fStarted;
    /** Whether to terminate or not. */
    bool volatile        fTerminate;
    /** The main thread's poll set to handle new clients. */
    RTPOLLSET            hPollSet;
    /** Pipe for communicating with the serving thread about new clients. - read end */
    RTPIPE               hPipeR;
    /** Pipe for communicating with the serving thread about new clients. - write end */
    RTPIPE               hPipeW;
    /** Main thread waiting for connections. */
    RTTHREAD             hThreadMain;
    /** Thread serving connected clients. */
    RTTHREAD             hThreadServing;
    /** Critical section protecting the list of new clients. */
    RTCRITSECT           CritSectClients;
    /** List of new clients waiting to be picked up by the client worker thread. */
    RTLISTANCHOR         LstClientsNew;
} ATSSERVER;
/** Pointer to an Audio Test Service (ATS) instance. */
typedef ATSSERVER *PATSSERVER;

int AudioTestSvcInit(PATSSERVER pThis, const char *pszBindAddr, uint32_t uBindPort, PCATSCALLBACKS pCallbacks);
int AudioTestSvcDestroy(PATSSERVER pThis);
int AudioTestSvcStart(PATSSERVER pThis);
int AudioTestSvcShutdown(PATSSERVER pThis);

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestService_h */

