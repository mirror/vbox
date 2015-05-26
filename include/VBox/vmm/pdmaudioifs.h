/** @file
 * PDM - Pluggable Device Manager, audio interfaces.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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

#ifndef ___VBox_vmm_pdmaudioifs_h
#define ___VBox_vmm_pdmaudioifs_h

#include <VBox/types.h>
#include <iprt/list.h>

typedef uint32_t PDMAUDIODRVFLAGS;

/** No flags set. */
#define PDMAUDIODRVFLAG_NONE        0
/** Marks a primary audio driver which is critical
 *  when running the VM. */
#define PDMAUDIODRVFLAG_PRIMARY     RT_BIT(0)

/**
 * Audio format in signed or unsigned variants.
 */
typedef enum PDMAUDIOFMT
{
    AUD_FMT_INVALID,
    AUD_FMT_U8,
    AUD_FMT_S8,
    AUD_FMT_U16,
    AUD_FMT_S16,
    AUD_FMT_U32,
    AUD_FMT_S32,
    /** Hack to blow the type up to 32-bit. */
    AUD_FMT_32BIT_HACK = 0x7fffffff
} PDMAUDIOFMT;

/**
 * Audio configuration of a certain backend.
 */
typedef struct PDMAUDIOBACKENDCFG
{
    uint32_t    cbStreamOut;
    uint32_t    cbStreamIn;
    uint32_t    cMaxHstStrmsOut;
    uint32_t    cMaxHstStrmsIn;
} PDMAUDIOBACKENDCFG, *PPDMAUDIOBACKENDCFG;

/**
 * An audio sample. At the moment stereo (left + right channels) only.
 * @todo Replace this with a more generic union
 *       which then also could handle 2.1 or 5.1 sound.
 */
typedef struct PDMAUDIOSAMPLE
{
    int64_t i64LSample;
    int64_t i64RSample;
} PDMAUDIOSAMPLE, *PPDMAUDIOSAMPLE;

typedef enum PDMAUDIOENDIANNESS
{
    /** The usual invalid endian. */
    PDMAUDIOENDIANNESS_INVALID,
    /** Little endian. */
    PDMAUDIOENDIANNESS_LITTLE,
    /** Bit endian. */
    PDMAUDIOENDIANNESS_BIG,
    /** Endianness doesn't have a meaning in the context. */
    PDMAUDIOENDIANNESS_NA,
    /** The end of the valid endian values (exclusive). */
    PDMAUDIOENDIANNESS_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOENDIANNESS_32BIT_HACK = 0x7fffffff
} PDMAUDIOENDIANNESS;

typedef struct PDMAUDIOSTREAMCFG
{
    /** Frequency in Hertz (Hz). */
    uint32_t uHz;
    /** Number of channels (2 for stereo). */
    uint8_t cChannels;
    /** Audio format. */
    PDMAUDIOFMT enmFormat;
    /** @todo Use RT_LE2H_*? */
    PDMAUDIOENDIANNESS enmEndianness;
} PDMAUDIOSTREAMCFG, *PPDMAUDIOSTREAMCFG;

#if defined(RT_LITTLE_ENDIAN)
# define PDMAUDIOHOSTENDIANNESS PDMAUDIOENDIANNESS_LITTLE
#elif defined(RT_BIG_ENDIAN)
# define PDMAUDIOHOSTENDIANNESS PDMAUDIOENDIANNESS_BIG
#else
# error "Port me!"
#endif

typedef enum PDMAUDIODIR
{
    PDMAUDIODIR_UNKNOWN = 0,
    PDMAUDIODIR_IN      = 1,
    PDMAUDIODIR_OUT     = 2,
    PDMAUDIODIR_BOTH    = 3
} PDMAUDIODIR;

typedef enum PDMAUDIOMIXERCTL
{
    PDMAUDIOMIXERCTL_UNKNOWN = 0,
    PDMAUDIOMIXERCTL_VOLUME,
    PDMAUDIOMIXERCTL_PCM,
    PDMAUDIOMIXERCTL_LINE_IN,
    PDMAUDIOMIXERCTL_MIC_IN,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOMIXERCTL_32BIT_HACK = 0x7fffffff
} PDMAUDIOMIXERCTL;

typedef enum PDMAUDIORECSOURCE
{
    PDMAUDIORECSOURCE_UNKNOWN = 0,
    PDMAUDIORECSOURCE_MIC,
    PDMAUDIORECSOURCE_CD,
    PDMAUDIORECSOURCE_VIDEO,
    PDMAUDIORECSOURCE_AUX,
    PDMAUDIORECSOURCE_LINE_IN,
    PDMAUDIORECSOURCE_PHONE,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIORECSOURCE_32BIT_HACK = 0x7fffffff
} PDMAUDIORECSOURCE;

/**
 * Audio stream commands. Used in the audio connector
 * as well as in the actual host backends.
 */
typedef enum PDMAUDIOSTREAMCMD
{
    /** Unknown command, do not use. */
    PDMAUDIOSTREAMCMD_UNKNOWN = 0,
    /** Enables the stream. */
    PDMAUDIOSTREAMCMD_ENABLE,
    /** Disables the stream. */
    PDMAUDIOSTREAMCMD_DISABLE,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMCMD_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMCMD;

/**
 * Properties of audio streams for host/guest
 * for in or out directions.
 */
typedef struct PDMPCMPROPS
{
    /** Sample width. Bits per sample. */
    uint8_t     cBits;
    /** Signed or unsigned sample. */
    bool        fSigned;
    /** Shift count used for faster calculation of various
     *  values, such as the alignment, bytes to samples and so on.
     *  Depends on number of stream channels and the stream format
     *  being used.
     *
     ** @todo Use some RTAsmXXX functions instead?
     */
    uint8_t     cShift;
    /** Number of audio channels. */
    uint8_t     cChannels;
    /** Alignment mask. */
    uint32_t    uAlign;
    /** Sample frequency in Hertz (Hz). */
    uint32_t    uHz;
    /** Bandwidth (bytes/s). */
    uint32_t    cbPerSec;
    /** Whether the endianness is swapped or not. */
    bool        fSwapEndian;
} PDMPCMPROPS, *PPDMPCMPROPS;

/**
 * Structure keeping an audio volume level.
 */
typedef struct PDMAUDIOVOLUME
{
    /** Set to @c true if this stream is muted, @c false if not. */
    bool                   fMuted;
    /** Left channel volume. */
    uint32_t               uLeft;
    /** Right channel volume. */
    uint32_t               uRight;
} PDMAUDIOVOLUME, *PPDMAUDIOVOLUME;

/**
 * Structure for holding rate processing information
 * of a source + destination audio stream. This is needed
 * because both streams can differ regarding their rates
 * and therefore need to be treated accordingly.
 */
typedef struct PDMAUDIOSTRMRATE
{
    /** Current (absolute) offset in the output
     *  (destination) stream. */
    uint64_t       dstOffset;
    /** Increment for moving dstOffset for the
     *  destination stream. This is needed because the
     *  source <-> destination rate might be different. */
    uint64_t       dstInc;
    /** Current (absolute) offset in the input
     *  stream. */
    uint32_t       srcOffset;
    /** Last processed sample of the input stream.
     *  Needed for interpolation. */
    PDMAUDIOSAMPLE srcSampleLast;
} PDMAUDIOSTRMRATE, *PPDMAUDIOSTRMRATE;

/**
 * Note: All internal handling is done in samples,
 *       not in bytes!
 */
typedef uint32_t PDMAUDIOMIXBUFFMT;
typedef PDMAUDIOMIXBUFFMT *PPDMAUDIOMIXBUFFMT;

typedef struct PDMAUDIOMIXBUF *PPDMAUDIOMIXBUF;
typedef struct PDMAUDIOMIXBUF
{
    RTLISTNODE             Node;
    /** Name of the buffer. */
    char                  *pszName;
    /** Sample buffer. */
    PPDMAUDIOSAMPLE        pSamples;
    /** Size of the sample buffer (in samples). */
    uint32_t               cSamples;
    /** The current read/write position (in samples)
     *  in the samples buffer. */
    uint32_t               offReadWrite;
    /**
     * Total samples already mixed down to the parent buffer (if any). Always starting at
     * the parent's offReadWrite position.
     *
     * Note: Count always is specified in parent samples, as the sample count can differ between parent
     *       and child.
     */
    uint32_t               cMixed;
    uint32_t               cProcessed;
    /** Pointer to parent buffer (if any). */
    PPDMAUDIOMIXBUF        pParent;
    /** List of children mix buffers to keep in sync with (if being a parent buffer). */
    RTLISTANCHOR           lstBuffers;
    /** Intermediate structure for buffer conversion tasks. */
    PPDMAUDIOSTRMRATE      pRate;
    /** Current volume used for mixing. */
    PDMAUDIOVOLUME         Volume;
    /** This buffer's audio format. */
    PDMAUDIOMIXBUFFMT      AudioFmt;
    /**
     * Ratio of the associated parent stream's frequency by this stream's
     * frequency (1<<32), represented as a signed 64 bit integer.
     *
     * For example, if the parent stream has a frequency of 44 khZ, and this
     * stream has a frequency of 11 kHz, the ration then would be
     * (44/11 * (1 << 32)).
     *
     * Currently this does not get changed once assigned.
     */
    int64_t                iFreqRatio;
    /* For quickly converting samples <-> bytes and
     * vice versa. */
    uint8_t                cShift;
} PDMAUDIOMIXBUF;

/**
 * Represents an audio input on the host of a certain
 * backend (e.g. DirectSound, PulseAudio etc).
 *
 * One host audio input is assigned to exactly one parent
 * guest input stream.
 */
struct PDMAUDIOGSTSTRMIN;
typedef PDMAUDIOGSTSTRMIN *PPDMAUDIOGSTSTRMIN;

typedef struct PDMAUDIOHSTSTRMIN
{
    /** List node. */
    RTLISTNODE             Node;
    /** PCM properties. */
    PDMPCMPROPS            Props;
    /** Whether this input is enabled or not. */
    bool                   fEnabled;
    /** This stream's mixing buffer. */
    PDMAUDIOMIXBUF         MixBuf;
    /** Pointer to (parent) guest stream. */
    PPDMAUDIOGSTSTRMIN     pGstStrmIn;
} PDMAUDIOHSTSTRMIN, *PPDMAUDIOHSTSTRMIN;

/*
 * Represents an audio output on the host through a certain
 * backend (e.g. DirectSound, PulseAudio etc).
 *
 * One host audio output can have multiple (1:N) guest outputs
 * assigned.
 */
typedef struct PDMAUDIOHSTSTRMOUT
{
    /** List node. */
    RTLISTNODE             Node;
    /** Stream properites. */
    PDMPCMPROPS            Props;
    /** Enabled or disabled flag. */
    bool                   fEnabled;
    /** Whether this stream was marked as being disabled
     *  but there are still associated guest output streams
     *  which rely on its data. */
    bool                   fPendingDisable;
    /** This stream's mixing buffer. */
    PDMAUDIOMIXBUF         MixBuf;
    /** Associated guest output streams. */
    RTLISTANCHOR           lstGstStrmOut;
} PDMAUDIOHSTSTRMOUT, *PPDMAUDIOHSTSTRMOUT;

/**
 * Guest audio stream state.
 */
typedef struct PDMAUDIOGSTSTRMSTATE
{
    /** Guest audio out stream active or not. */
    bool                   fActive;
    /** Guest audio output stream has some samples or not. */
    bool                   fEmpty;
    /** Name of this stream. */
    char                  *pszName;
} PDMAUDIOGSTSTRMSTATE, *PPDMAUDIOGSTSTRMSTATE;

/**
 * Represents an audio input from the guest (that is, from the
 * emulated device, e.g. Intel HDA).
 *
 * Each guest input can have multiple host input streams.
 */
typedef struct PDMAUDIOGSTSTRMIN
{
    /** Guest stream properites. */
    PDMPCMPROPS            Props;
    /** Current stream state. */
    PDMAUDIOGSTSTRMSTATE   State;
    /** This stream's mixing buffer. */
    PDMAUDIOMIXBUF         MixBuf;
    /** Pointer to associated host input stream. */
    PPDMAUDIOHSTSTRMIN     pHstStrmIn;
} PDMAUDIOGSTSTRMIN, *PPDMAUDIOGSTSTRMIN;

/**
 * Represents an audio output from the guest (that is, from the
 * emulated device, e.g. Intel HDA).
 *
 * Each guest output is assigned to a single host output.
 */
typedef struct PDMAUDIOGSTSTRMOUT
{
    /** List node. */
    RTLISTNODE             Node;
    /** Guest output stream properites. */
    PDMPCMPROPS            Props;
    /** Current stream state. */
    PDMAUDIOGSTSTRMSTATE   State;
    /** This stream's mixing buffer. */
    PDMAUDIOMIXBUF         MixBuf;
    /** Pointer to the associated host output stream. */
    PPDMAUDIOHSTSTRMOUT    pHstStrmOut;
} PDMAUDIOGSTSTRMOUT, *PPDMAUDIOGSTSTRMOUT;

/** Pointer to a audio connector interface. */
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;
/**
 * Audio connector interface (up).
 */
typedef struct PDMIAUDIOCONNECTOR
{
    DECLR3CALLBACKMEMBER(int, pfnQueryStatus, (PPDMIAUDIOCONNECTOR pInterface, uint32_t *pcbAvailIn, uint32_t *pcbFreeOut, uint32_t *pcSamplesLive));

    /**
     * Reads PCM audio data from the host (input).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmIn      Pointer to guest input stream to write to.
     * @param   pvBuf           Where to store the read data.
     * @param   cbSize          Number of bytes to read.
     * @param   pcbRead         Bytes of audio data read. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn, void *pvBuf, uint32_t cbSize, uint32_t *pcbRead));

    /**
     * Writes PCM audio data to the host (output).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmOut     Pointer to guest output stream to read from.
     * @param   pvBuf           Audio data to be written.
     * @param   cbSize          Number of bytes to be written.
     * @param   pcbWritten      Bytes of audio data written. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut, const void *pvBuf, uint32_t cbSize, uint32_t *pcbWritten));

    /**
     * Checks whether the specified guest input stream is in a working state.
     *
     * @returns True if a host voice in is available, false if not.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmIn      Pointer to guest input stream to check.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsInputOK, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn));

    /**
     * Checks whether the specified guest output stream is in a working state.
     *
     * @returns True if a host voice out is available, false if not.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmOut     Pointer to guest output stream to check.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsOutputOK, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut));

    /**
     * Initializes the NULL audio driver as a fallback in case no host backend is available.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnInitNull, (PPDMIAUDIOCONNECTOR pInterface));

    /**
     * Enables a specific guest output stream and starts the audio device.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmOut     Pointer to guest output stream.
     * @param   fEnable         Whether to enable or disable the specified output stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnEnableOut, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut, bool fEnable));

    /**
     * Enables a specific guest input stream and starts the audio device.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmIn      Pointer to guest input stream.
     * @param   fEnable         Whether to enable or disable the specified input stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnEnableIn, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn, bool fEnable));

    /**
     * Closes a specific guest input stream.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmIn      Pointer to guest input stream.
     */
    DECLR3CALLBACKMEMBER(void, pfnCloseIn, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn));

    /**
     * Closes a specific guest output stream.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmOut     Pointer to guest output stream.
     */
    DECLR3CALLBACKMEMBER(void, pfnCloseOut, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut));

    /**
     * Opens an input audio channel.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pszName              Name of the audio channel.
     * @param   enmRecSource         Specifies the type of recording source to be opened.
     * @param   pCfg                 Pointer to PDMAUDIOSTREAMCFG to use.
     * @param   ppGstStrmIn          Pointer where to return the guest guest input stream on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpenIn, (PPDMIAUDIOCONNECTOR pInterface, const char *pszName,
                                          PDMAUDIORECSOURCE enmRecSource, PPDMAUDIOSTREAMCFG pCfg,
                                          PPDMAUDIOGSTSTRMIN *ppGstStrmIn));

    /**
     * Opens an output audio channel.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pszName              Name of the audio channel.
     * @param   pCfg                 Pointer to PDMAUDIOSTREAMCFG to use.
     * @param   ppGstStrmOut         Pointer where to return the guest guest input stream on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpenOut, (PPDMIAUDIOCONNECTOR pInterface, const char *pszName,
                                           PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOGSTSTRMOUT *ppGstStrmOut));

    DECLR3CALLBACKMEMBER(int, pfnPlayOut, (PPDMIAUDIOCONNECTOR pInterface, uint32_t *pcSamplesPlayed));

    /**
     * Checks whether a specific guest input stream is active or not.
     *
     * @returns Whether the specified stream is active or not.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmIn      Pointer to guest input stream.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsActiveIn, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn));

    /**
     * Checks whether a specific guest output stream is active or not.
     *
     * @returns Whether the specified stream is active or not.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pGstStrmOut     Pointer to guest output stream.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsActiveOut, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut));

} PDMIAUDIOCONNECTOR;

/** PDMIAUDIOCONNECTOR interface ID. */
#define PDMIAUDIOCONNECTOR_IID                  "a41ca770-ed07-4f57-a0a6-41377d9d484f"

/** Defines all needed interface callbacks for an audio backend. */
#define PDMAUDIO_IHOSTAUDIO_CALLBACKS(_aDrvName) \
    pThis->IHostAudio.pfnCaptureIn  = _aDrvName##CaptureIn;  \
    pThis->IHostAudio.pfnControlIn  = _aDrvName##ControlIn;  \
    pThis->IHostAudio.pfnControlOut = _aDrvName##ControlOut; \
    pThis->IHostAudio.pfnFiniIn     = _aDrvName##FiniIn;     \
    pThis->IHostAudio.pfnFiniOut    = _aDrvName##FiniOut;    \
    pThis->IHostAudio.pfnGetConf    = _aDrvName##GetConf;    \
    pThis->IHostAudio.pfnInit       = _aDrvName##Init;       \
    pThis->IHostAudio.pfnInitIn     = _aDrvName##InitIn;     \
    pThis->IHostAudio.pfnInitOut    = _aDrvName##InitOut;    \
    pThis->IHostAudio.pfnIsEnabled  = _aDrvName##IsEnabled;  \
    pThis->IHostAudio.pfnPlayOut    = _aDrvName##PlayOut;    \
    pThis->IHostAudio.pfnShutdown   = _aDrvName##Shutdown;

/** Pointer to a host audio interface. */
typedef struct PDMIHOSTAUDIO *PPDMIHOSTAUDIO;
/**
 * PDM host audio interface.
 */
typedef struct PDMIHOSTAUDIO
{
    /**
     * Initialize the host-specific audio device.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnInit, (PPDMIHOSTAUDIO pInterface));

    /**
     * Shuts down the host-specific audio device.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(void, pfnShutdown, (PPDMIHOSTAUDIO pInterface));

    /**
     * Initialize the host-specific audio device for input stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmIn          Pointer to host input stream.
     * @param   pStreamCfg          Pointer to stream configuration.
     * @param   enmRecSource        Specifies the type of recording source to be initialized.
     * @param   pcSamples           Returns how many samples the backend can handle. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnInitIn, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pStreamCfg, PDMAUDIORECSOURCE enmRecSource, uint32_t *pcSamples));

    /**
     * Initialize the host-specific output device for output stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmOut         Pointer to host output stream.
     * @param   pStreamCfg          Pointer to stream configuration.
     * @param   pcSamples           Returns how many samples the backend can handle. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnInitOut, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pStreamCfg, uint32_t *pcSamples));

    /**
     * Control the host audio device for an input stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmOut         Pointer to host output stream.
     * @param   enmStreamCmd        The stream command to issue.
     */
    DECLR3CALLBACKMEMBER(int, pfnControlOut, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut, PDMAUDIOSTREAMCMD enmStreamCmd));

    /**
     * Control the host audio device for an output stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmOut         Pointer to host output stream.
     * @param   enmStreamCmd        The stream command to issue.
     */
    DECLR3CALLBACKMEMBER(int, pfnControlIn, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn, PDMAUDIOSTREAMCMD enmStreamCmd));

    /**
     * Ends the host audio input streamm.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmIn          Pointer to host input stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnFiniIn, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn));

    /**
     * Ends the host output stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmOut         Pointer to host output stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnFiniOut, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut));

    DECLR3CALLBACKMEMBER(bool, pfnIsEnabled, (PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir));

    /**
     * Plays a host audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmOut         Pointer to host output stream.
     * @param   pcSamplesPlayed     Pointer to number of samples captured.
     */
    DECLR3CALLBACKMEMBER(int, pfnPlayOut, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut, uint32_t *pcSamplesPlayed));

    /**
     * Records audio to input stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pHstStrmIn          Pointer to host input stream.
     * @param   pcSamplesCaptured   Pointer to number of samples captured.
     */
    DECLR3CALLBACKMEMBER(int, pfnCaptureIn, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn, uint32_t *pcSamplesCaptured));

    /**
     * Gets the configuration from the host audio (backend) driver.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pBackendCfg         Pointer where to store the backend audio configuration to.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetConf, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg));

} PDMIHOSTAUDIO;
#define PDMIHOSTAUDIO_IID                           "39feea4f-c824-4197-bcff-7d4a6ede7420"

#endif /* ___VBox_vmm_pdmaudioifs_h */

