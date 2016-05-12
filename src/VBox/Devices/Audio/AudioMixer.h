/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
 */

/*
 * Copyright (C) 2014-2016 Oracle Corporation
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

/**
 * Structure for maintaining an audio mixer instance.
 */
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

/** No flags specified. */
#define AUDMIXSTREAM_FLAG_NONE                  0

/**
 * Structure for maintaining an audio mixer stream.
 */
typedef struct AUDMIXSTREAM
{
    /** List node. */
    RTLISTNODE              Node;
    /** Name of this stream. */
    char                   *pszName;
    /** Stream flags of type AUDMIXSTREAM_FLAG_. */
    uint32_t                fFlags;
    /** Pointer to audio connector being used. */
    PPDMIAUDIOCONNECTOR     pConn;
    /** Audio direction of this stream. */
    PDMAUDIODIR             enmDir;
    /** Union of PDM input/output streams for this stream. */
    union
    {
        PPDMAUDIOGSTSTRMIN  pIn;
        PPDMAUDIOGSTSTRMOUT pOut;
    } InOut;
} AUDMIXSTREAM, *PAUDMIXSTREAM;

/**
 * Audio mixer sink direction.
 */
typedef enum AUDMIXSINKDIR
{
    AUDMIXSINKDIR_UNKNOWN = 0,
    AUDMIXSINKDIR_INPUT,
    AUDMIXSINKDIR_OUTPUT,
    /** The usual 32-bit hack. */
    AUDMIXSINKDIR_32BIT_HACK = 0x7fffffff
} AUDMIXSINKDIR;

/**
 * Audio mixer sink command.
 */
typedef enum AUDMIXSINKCMD
{
    /** Unknown command, do not use. */
    AUDMIXSINKCMD_UNKNOWN = 0,
    /** Enables the sink. */
    AUDMIXSINKCMD_ENABLE,
    /** Disables the sink. */
    AUDMIXSINKCMD_DISABLE,
    /** Pauses the sink. */
    AUDMIXSINKCMD_PAUSE,
    /** Resumes the sink. */
    AUDMIXSINKCMD_RESUME,
    /** Hack to blow the type up to 32-bit. */
    AUDMIXSINKCMD_32BIT_HACK = 0x7fffffff
} AUDMIXSINKCMD;

/**
 * Structure for maintaining an audio mixer sink.
 */
typedef struct AUDMIXSINK
{
    RTLISTNODE              Node;
    /** Pointer to mixer object this sink is bound to. */
    PAUDIOMIXER             pParent;
    /** Name of this sink. */
    char                   *pszName;
    /** The sink direction, that is,
     *  if this sink handles input or output. */
    AUDMIXSINKDIR           enmDir;
    /** The sink's PCM format. */
    PDMPCMPROPS             PCMProps;
    /** Number of streams assigned. */
    uint8_t                 cStreams;
    /** List of assigned streams. */
    /** @todo Use something faster -- vector maybe? */
    RTLISTANCHOR            lstStreams;
    /** This sink's mixing buffer. */
    PDMAUDIOMIXBUF          MixBuf;
    /** The volume of this sink. The volume always will
     *  be combined with the mixer's master volume. */
    PDMAUDIOVOLUME          Volume;
} AUDMIXSINK, *PAUDMIXSINK;

/**
 * Audio mixer operation.
 */
typedef enum AUDMIXOP
{
    /** Invalid operation, do not use. */
    AUDMIXOP_INVALID = 0,
    AUDMIXOP_COPY,
    AUDMIXOP_BLEND,
    /** The usual 32-bit hack. */
    AUDMIXOP_32BIT_HACK = 0x7fffffff
} AUDMIXOP;

/** No flags specified. */
#define AUDMIXSTRMCTL_FLAG_NONE         0

int AudioMixerCreate(const char *pszName, uint32_t uFlags, PAUDIOMIXER *ppMixer);
int AudioMixerCreateSink(PAUDIOMIXER pMixer, const char *pszName, AUDMIXSINKDIR enmDir, PAUDMIXSINK *ppSink);
void AudioMixerDestroy(PAUDIOMIXER pMixer);
int AudioMixerGetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg);
void AudioMixerInvalidate(PAUDIOMIXER pMixer);
void AudioMixerRemoveSink(PAUDIOMIXER pMixer, PAUDMIXSINK pSink);
int AudioMixerSetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg);
int AudioMixerSetMasterVolume(PAUDIOMIXER pMixer, PPDMAUDIOVOLUME pVol);
void AudioMixerDebug(PAUDIOMIXER pMixer, PCDBGFINFOHLP pHlp, const char *pszArgs);

int AudioMixerSinkAddStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream);
int AudioMixerSinkCtl(PAUDMIXSINK pSink, AUDMIXSINKCMD enmCmd);
void AudioMixerSinkDestroy(PAUDMIXSINK pSink);
AUDMIXSINKDIR AudioMixerSinkGetDir(PAUDMIXSINK pSink);
PAUDMIXSTREAM AudioMixerSinkGetStream(PAUDMIXSINK pSink, uint8_t uIndex);
uint8_t AudioMixerSinkGetStreamCount(PAUDMIXSINK pSink);
int AudioMixerSinkRead(PAUDMIXSINK pSink, AUDMIXOP enmOp, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead);
void AudioMixerSinkRemoveStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream);
void AudioMixerSinkRemoveAllStreams(PAUDMIXSINK pSink);
int AudioMixerSinkSetFormat(PAUDMIXSINK pSink, PPDMPCMPROPS pPCMProps);
int AudioMixerSinkSetVolume(PAUDMIXSINK pSink, PPDMAUDIOVOLUME pVol);
void AudioMixerSinkTimerUpdate(PAUDMIXSINK pSink, uint64_t cTicksPerSec, uint64_t cTicksElapsed, uint32_t *pcbData);
int AudioMixerSinkWrite(PAUDMIXSINK pSink, AUDMIXOP enmOp, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten);
int AudioMixerSinkUpdate(PAUDMIXSINK pSink);

int AudioMixerStreamCreate(PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOSTREAMCFG pCfg, uint32_t fFlags, PAUDMIXSTREAM *ppStream);
int AudioMixerStreamCtl(PAUDMIXSTREAM pStream, PDMAUDIOSTREAMCMD enmCmd, uint32_t fCtl);
void AudioMixerStreamDestroy(PAUDMIXSTREAM pStream);
bool AudioMixerStreamIsActive(PAUDMIXSTREAM pStream);
bool AudioMixerStreamIsValid(PAUDMIXSTREAM pStream);

#endif /* AUDIO_MIXER_H */

