/* $Id$ */
/** @file
 * AudioTestServiceClient - Audio test execution server, Public Header.
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

#ifndef VBOX_INCLUDED_SRC_Audio_AudioTestServiceClient_h
#define VBOX_INCLUDED_SRC_Audio_AudioTestServiceClient_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "AudioTestServiceInternal.h"

/**
 * Structure for maintaining an ATS client.
 */
typedef struct ATSCLIENT
{
    /** Pointer to the selected transport layer. */
    PCATSTRANSPORT      pTransport;
    /** Pointer to the selected transport instance to use. */
    PATSTRANSPORTINST   pTransportInst;
    /** The opaque client instance. */
    PATSTRANSPORTCLIENT pTransportClient;
} ATSCLIENT;
/** Pointer to an ATS client. */
typedef ATSCLIENT *PATSCLIENT;

int AudioTestSvcClientCreate(PATSCLIENT pClient);
void AudioTestSvcClientDestroy(PATSCLIENT pClient);
int AudioTestSvcClientConnect(PATSCLIENT pClient);
int AudioTestSvcClientHandleOption(PATSCLIENT pClient, int ch, PCRTGETOPTUNION pVal);
int AudioTestSvcClientTestSetBegin(PATSCLIENT pClient, const char *pszTag);
int AudioTestSvcClientTestSetEnd(PATSCLIENT pClient, const char *pszTag);
int AudioTestSvcClientTonePlay(PATSCLIENT pClient, PAUDIOTESTTONEPARMS pToneParms);
int AudioTestSvcClientToneRecord(PATSCLIENT pClient, PAUDIOTESTTONEPARMS pToneParms);
int AudioTestSvcClientTestSetDownload(PATSCLIENT pClient, const char *pszTag, const char *pszPathOutAbs);
int AudioTestSvcClientClose(PATSCLIENT pClient);

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestServiceClient_h */

