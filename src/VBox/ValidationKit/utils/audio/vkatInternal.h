/* $Id$ */
/** @file
 * VKAT - Internal header file for common definitions + structs.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_SRC_audio_vkatInternal_h
#define VBOX_INCLUDED_SRC_audio_vkatInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBoxDD.h"

#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

/**
 * Audio driver stack.
 *
 * This can be just be backend driver alone or DrvAudio with a backend.
 * @todo add automatic resampling via mixer so we can test more of the audio
 *       stack used by the device emulations.
 */
typedef struct AUDIOTESTDRVSTACK
{
    /** The device registration record for the backend. */
    PCPDMDRVREG             pDrvReg;
    /** The backend driver instance. */
    PPDMDRVINS              pDrvBackendIns;
    /** The backend's audio interface. */
    PPDMIHOSTAUDIO          pIHostAudio;

    /** The DrvAudio instance. */
    PPDMDRVINS              pDrvAudioIns;
    /** This is NULL if we don't use DrvAudio. */
    PPDMIAUDIOCONNECTOR     pIAudioConnector;
} AUDIOTESTDRVSTACK;
/** Pointer to an audio driver stack. */
typedef AUDIOTESTDRVSTACK *PAUDIOTESTDRVSTACK;

/**
 * Backend-only stream structure.
 */
typedef struct AUDIOTESTDRVSTACKSTREAM
{
    /** The public stream data. */
    PDMAUDIOSTREAM          Core;
    /** The acquired config. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** The backend data (variable size). */
    PDMAUDIOBACKENDSTREAM   Backend;
} AUDIOTESTDRVSTACKSTREAM;
/** Pointer to a backend-only stream structure. */
typedef AUDIOTESTDRVSTACKSTREAM *PAUDIOTESTDRVSTACKSTREAM;

/** Maximum audio streams a test environment can handle. */
#define AUDIOTESTENV_MAX_STREAMS 8

/** The test handle. */
extern RTTEST         g_hTest;
extern unsigned       g_uVerbosity;
extern bool           g_fDrvAudioDebug;
extern const char    *g_pszDrvAudioDebug;

/** @name Driver stack
 * @{ */
void audioTestDriverStackDelete(PAUDIOTESTDRVSTACK pDrvStack);
int audioTestDriverStackInit(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, bool fWithDrvAudio);
int audioTestDriverStackSetDevice(PAUDIOTESTDRVSTACK pDrvStack, PDMAUDIODIR enmDir, const char *pszDevId);
/** @}  */

/** @name Driver
 * @{ */
int audioTestDrvConstruct(PAUDIOTESTDRVSTACK pDrvStack, PCPDMDRVREG pDrvReg, PPDMDRVINS pParentDrvIns, PPPDMDRVINS ppDrvIns);
/** @}  */

/** @name Driver stack stream
 * @{ */
int audioTestDriverStackStreamCapture(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream,
                                      void *pvBuf, uint32_t cbBuf, uint32_t *pcbCaptured);
int audioTestDriverStackStreamCreateInput(PAUDIOTESTDRVSTACK pDrvStack, PCPDMAUDIOPCMPROPS pProps,
                                          uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                          PPDMAUDIOSTREAM *ppStream, PPDMAUDIOSTREAMCFG pCfgAcq);
int audioTestDriverStackStreamCreateOutput(PAUDIOTESTDRVSTACK pDrvStack, PCPDMAUDIOPCMPROPS pProps,
                                           uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint,
                                           PPDMAUDIOSTREAM *ppStream, PPDMAUDIOSTREAMCFG pCfgAcq);
void audioTestDriverStackStreamDestroy(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
int audioTestDriverStackStreamDrain(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream, bool fSync);
int audioTestDriverStackStreamEnable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
uint32_t audioTestDriverStackStreamGetWritable(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
bool audioTestDriverStackStreamIsOkay(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream);
int audioTestDriverStackStreamPlay(PAUDIOTESTDRVSTACK pDrvStack, PPDMAUDIOSTREAM pStream, void const *pvBuf, uint32_t cbBuf, uint32_t *pcbPlayed);
/** @}  */

#endif /* !VBOX_INCLUDED_SRC_audio_vkatInternal_h */

