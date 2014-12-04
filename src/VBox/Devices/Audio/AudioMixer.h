/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
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

#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <iprt/cdefs.h>
#include <VBox/vmm/pdmaudioifs.h>

typedef struct AUDIOMIXER
{
    /** Mixer name. */
    char                   *pszName;
    /** Format the mixer should convert/output
     *  data to so that the underlying device emulation
     *  can work with it. */
    PDMAUDIOSTREAMCFG       devFmt;
    /* List of audio mixer sinks. */
    RTLISTANCHOR            lstSinks;
    /** Number of used audio sinks. */
    uint8_t                 cSinks;
} AUDIOMIXER, *PAUDIOMIXER;

typedef struct AUDMIXSTREAM
{
    RTLISTNODE              Node;
    PPDMIAUDIOCONNECTOR     pConn;
    /** @todo Add support for output streams. */
    PPDMAUDIOGSTSTRMIN      pStrm;
} AUDMIXSTREAM, *PAUDMIXSTREAM;

typedef enum AUDMIXSINKDIR
{
    AUDMIXSINKDIR_UNKNOWN = 0,
    AUDMIXSINKDIR_INPUT,
    AUDMIXSINKDIR_OUTPUT
} AUDMIXSINKDIR;

typedef struct AUDMIXSINK
{
    RTLISTNODE              Node;
    /** Name of this sink. */
    char                   *pszName;
    /** The sink direction, that is,
     *  if this sink handles input or output. */
    AUDMIXSINKDIR           enmDir;
    /** Pointer to mixer object this sink is bound
     *  to. */
    PAUDIOMIXER             pParent;
    /** Number of streams assigned. */
    uint8_t                 cStreams;
    /** List of assigned streams. */
    RTLISTANCHOR            lstStreams;
    /** This sink's mixing buffer. */
    PDMAUDIOMIXBUF          MixBuf;
} AUDMIXSINK, *PAUDMIXSINK;

typedef enum AUDMIXOP
{
    AUDMIXOP_NONE = 0,
    AUDMIXOP_COPY,
    AUDMIXOP_BLEND
} AUDMIXOP;


int audioMixerAddSink(PAUDIOMIXER pMixer, const char *pszName, PAUDMIXSINK *ppSink);
int audioMixerAddStreamIn(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMIN pStream, uint32_t uFlags, PAUDMIXSTREAM *ppStream);
int audioMixerControlStream(AUDMIXSTREAM pHandle); /** @todo */
int audioMixerCreate(const char *pszName, uint32_t uFlags, PAUDIOMIXER *ppMixer);
void audioMixerDestroy(PAUDIOMIXER pMixer);
int audioMixerProcessSamples(AUDMIXOP enmOp, PPDMAUDIOSAMPLE pDst, uint32_t cToWrite, PPDMAUDIOSAMPLE pSrc, uint32_t cToRead, uint32_t *pcRead, uint32_t *pcWritten);
int audioMixerProcessSamplesEx(AUDMIXOP enmOp, void *pvParms, size_t cbParms, PPDMAUDIOSAMPLE pDst, uint32_t cToWrite, PPDMAUDIOSAMPLE pSrc, uint32_t cToRead, uint32_t *pcRead, uint32_t *pcWritten);
uint32_t audioMixerGetStreamCount(PAUDIOMIXER pMixer);
int audioMixerProcessSinkIn(PAUDMIXSINK pSink, void *pvBuf, size_t cbBuf, uint32_t *pcbProcessed);
void audioMixerRemoveSink(PAUDIOMIXER pMixer, PAUDMIXSINK pSink);
void audioMixerRemoveStream(PAUDMIXSINK pMixer, PAUDMIXSTREAM pStream);
int audioMixerSetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg);

#endif /* AUDIO_MIXER_H */
