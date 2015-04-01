/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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
    /** The master volume of this mixer. */
    PDMAUDIOVOLUME          VolMaster;
    /* List of audio mixer sinks. */
    RTLISTANCHOR            lstSinks;
    /** Number of used audio sinks. */
    uint8_t                 cSinks;
} AUDIOMIXER, *PAUDIOMIXER;

typedef struct AUDMIXSTREAM
{
    RTLISTNODE              Node;
    PPDMIAUDIOCONNECTOR     pConn;
    union
    {
        PPDMAUDIOGSTSTRMIN  pIn;
        PPDMAUDIOGSTSTRMOUT pOut;
    };
} AUDMIXSTREAM, *PAUDMIXSTREAM;

typedef enum AUDMIXSINKDIR
{
    AUDMIXSINKDIR_UNKNOWN = 0,
    AUDMIXSINKDIR_INPUT,
    AUDMIXSINKDIR_OUTPUT,
    /** The usual 32-bit hack. */
    AUDMIXSINKDIR_32BIT_HACK = 0x7fffffff
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
    /** The volume of this sink. The volume always will
     *  be combined with the mixer's master volume. */
    PDMAUDIOVOLUME          Volume;
} AUDMIXSINK, *PAUDMIXSINK;

typedef enum AUDMIXOP
{
    AUDMIXOP_NONE = 0,
    AUDMIXOP_COPY,
    AUDMIXOP_BLEND,
    /** The usual 32-bit hack. */
    AUDMIXOP_32BIT_HACK = 0x7fffffff
} AUDMIXOP;


int audioMixerAddSink(PAUDIOMIXER pMixer, const char *pszName, AUDMIXSINKDIR enmDir, PAUDMIXSINK *ppSink);
int audioMixerAddStreamIn(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMIN pStream, uint32_t uFlags, PAUDMIXSTREAM *ppStream);
int audioMixerAddStreamOut(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMOUT pStream, uint32_t uFlags, PAUDMIXSTREAM *ppStream);
int audioMixerControlStream(AUDMIXSTREAM pHandle); /** @todo Implement me. */
int audioMixerCreate(const char *pszName, uint32_t uFlags, PAUDIOMIXER *ppMixer);
void audioMixerDestroy(PAUDIOMIXER pMixer);
uint32_t audioMixerGetStreamCount(PAUDIOMIXER pMixer);
void audioMixerInvalidate(PAUDIOMIXER pMixer);
int audioMixerProcessSinkIn(PAUDMIXSINK pSink, AUDMIXOP enmOp, void *pvBuf, size_t cbBuf, uint32_t *pcbProcessed);
int audioMixerProcessSinkOut(PAUDMIXSINK pSink, AUDMIXOP enmOp, const void *pvBuf, size_t cbBuf, uint32_t *pcbProcessed);
void audioMixerRemoveSink(PAUDIOMIXER pMixer, PAUDMIXSINK pSink);
void audioMixerRemoveStream(PAUDMIXSINK pMixer, PAUDMIXSTREAM pStream);
int audioMixerSetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg);
int audioMixerSetMasterVolume(PAUDIOMIXER pMixer, PPDMAUDIOVOLUME pVol);
int audioMixerSetSinkVolume(PAUDMIXSINK pSink, PPDMAUDIOVOLUME pVol);

#endif /* AUDIO_MIXER_H */

