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


/**
 * Structure for keeping an Audio Test Server (ATS) instance.
 */
typedef struct ATSSERVER
{
    /** The selected transport layer. */
    PCATSTRANSPORT       pTransport;
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
/** Pointer to an Audio Test Server (ATS) instance. */
typedef ATSSERVER *PATSSERVER;


int AudioTestSvcInit(PATSSERVER pThis);
int AudioTestSvcDestroy(PATSSERVER pThis);
int AudioTestSvcStart(PATSSERVER pThis);
int AudioTestSvcShutdown(PATSSERVER pThis);

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestService_h */

