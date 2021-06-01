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

typedef struct ATSCLIENT
{
    uint8_t  abHdr[16];
    uint16_t cbHdr;
    RTSOCKET hSock;
} ATSCLIENT;
typedef ATSCLIENT *PATSCLIENT;

int AudioTestSvcClientConnect(PATSCLIENT pClient, const char *pszAddr, uint32_t uPort);
int AudioTestSvcClientTestSetBegin(PATSCLIENT pClient, const char *pszTag);
int AudioTestSvcClientTestSetEnd(PATSCLIENT pClient, const char *pszTag);
int AudioTestSvcClientTonePlay(PATSCLIENT pClient, PPDMAUDIOSTREAMCFG pStreamCfg, PAUDIOTESTTONEPARMS pToneParms);
int AudioTestSvcClientClose(PATSCLIENT pClient);

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestServiceClient_h */

