/* $Id$ */
/** @file
 * Audio Mixing bufer convert audio samples to/from different rates / formats.
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_AudioMixBuffer_h
#define VBOX_INCLUDED_SRC_Audio_AudioMixBuffer_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <VBox/vmm/pdmaudioifs.h>


/**
 * Rate processing information of a source & destination audio stream.
 *
 * This is needed because both streams can differ regarding their rates and
 * therefore need to be treated accordingly.
 */
typedef struct AUDIOSTREAMRATE
{
    /** Current (absolute) offset in the output (destination) stream.
     * @todo r=bird: Please reveal which unit these members are given in. */
    uint64_t        offDst;
    /** Increment for moving offDst for the destination stream.
     * This is needed because the source <-> destination rate might be different. */
    uint64_t        uDstInc;
    /** Current (absolute) offset in the input stream. */
    uint32_t        offSrc;
    /** Set if no conversion is necessary. */
    bool            fNoConversionNeeded;
    bool            afPadding[3];

    /** Last processed frame of the input stream.
     *  Needed for interpolation. */
    union
    {
        int64_t         ai64Samples[2];
        PDMAUDIOFRAME   Frame;
    } SrcLast;

    /**
     * Resampling function.
     * @returns Number of destination frames written.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnResample, (int64_t *pi64Dst, uint32_t cDstFrames,
                                                 int64_t const *pi64Src, uint32_t cSrcFrames, uint32_t *pcSrcFramesRead,
                                                 struct AUDIOSTREAMRATE *pRate));

} AUDIOSTREAMRATE;
/** Pointer to rate processing information of a stream. */
typedef AUDIOSTREAMRATE *PAUDIOSTREAMRATE;

/**
 * Mixing buffer volume parameters.
 *
 * The volume values are in fixed point style and must be converted to/from
 * before using with e.g. PDMAUDIOVOLUME.
 */
typedef struct AUDMIXBUFVOL
{
    /** Set to @c true if this stream is muted, @c false if not. */
    bool            fMuted;
    /** Left volume to apply during conversion.
     * Pass 0 to convert the original values. May not apply to all conversion functions. */
    uint32_t        uLeft;
    /** Right volume to apply during conversion.
     * Pass 0 to convert the original values. May not apply to all conversion functions. */
    uint32_t        uRight;
} AUDMIXBUFVOL;
/** Pointer to mixing buffer volument parameters. */
typedef AUDMIXBUFVOL *PAUDMIXBUFVOL;

/*
 * Frame conversion parameters for the audioMixBufConvFromXXX / audioMixBufConvToXXX functions.
 */
typedef struct AUDMIXBUFCONVOPTS
{
    /** Number of audio frames to convert. */
    uint32_t        cFrames;
    union
    {
        struct
        {
            /** Volume to use for conversion. */
            AUDMIXBUFVOL Volume;
        } From;
    } RT_UNION_NM(u);
} AUDMIXBUFCONVOPTS;
/** Pointer to conversion parameters for the audio mixer.   */
typedef AUDMIXBUFCONVOPTS *PAUDMIXBUFCONVOPTS;
/** Pointer to const conversion parameters for the audio mixer.   */
typedef AUDMIXBUFCONVOPTS const *PCAUDMIXBUFCONVOPTS;

/**
 * @note All internal handling is done in audio frames, not in bytes!
 * @todo r=bird: What does this note actually apply to?
 */
typedef uint32_t AUDIOMIXBUFFMT;
typedef AUDIOMIXBUFFMT *PAUDIOMIXBUFFMT;

/**
 * Convertion-from function used by the audio buffer mixer.
 *
 * @returns Number of audio frames returned.
 * @param   paDst           Where to return the converted frames.
 * @param   pvSrc           The source frame bytes.
 * @param   cbSrc           Number of bytes to convert.
 * @param   pOpts           Conversion options.
 * @todo r=bird: The @a paDst size is presumable given in @a pOpts->cFrames?
 */
typedef DECLCALLBACKTYPE(uint32_t, FNAUDIOMIXBUFCONVFROM,(PPDMAUDIOFRAME paDst, const void *pvSrc, uint32_t cbSrc,
                                                          PCAUDMIXBUFCONVOPTS pOpts));
/** Pointer to a convertion-from function used by the audio buffer mixer. */
typedef FNAUDIOMIXBUFCONVFROM *PFNAUDIOMIXBUFCONVFROM;

/**
 * Convertion-to function used by the audio buffer mixer.
 *
 * @param   pvDst           Output buffer.
 * @param   paSrc           The input frames.
 * @param   pOpts           Conversion options.
 * @todo r=bird: The @a paSrc size is presumable given in @a pOpts->cFrames and
 *       this implicitly gives the pvDst size too, right?
 */
typedef DECLCALLBACKTYPE(void, FNAUDIOMIXBUFCONVTO,(void *pvDst, PCPDMAUDIOFRAME paSrc, PCAUDMIXBUFCONVOPTS pOpts));
/** Pointer to a convertion-to function used by the audio buffer mixer. */
typedef FNAUDIOMIXBUFCONVTO *PFNAUDIOMIXBUFCONVTO;

/** Pointer to audio mixing buffer.  */
typedef struct AUDIOMIXBUF *PAUDIOMIXBUF;
/** Pointer to a const audio mixing buffer.  */
typedef struct AUDIOMIXBUF const *PCAUDIOMIXBUF;


/**
 * State & config for AudioMixBufPeek created by AudioMixBufInitPeekState.
 */
typedef struct AUDIOMIXBUFPEEKSTATE
{
    /** Encodes @a cFrames from @a paSrc to @a pvDst. */
    DECLR3CALLBACKMEMBER(void,  pfnEncode,(void *pvDst, int64_t const *paSrc, uint32_t cFrames, struct AUDIOMIXBUFPEEKSTATE *pState));
    /** Sample rate conversion state (only used when needed). */
    AUDIOSTREAMRATE             Rate;
    /** Source (mixer) channels. */
    uint8_t                     cSrcChannels;
    /** Destination channels. */
    uint8_t                     cDstChannels;
    /** Destination frame size. */
    uint8_t                     cbDstFrame;
} AUDIOMIXBUFPEEKSTATE;
/** Pointer to peek state & config. */
typedef AUDIOMIXBUFPEEKSTATE *PAUDIOMIXBUFPEEKSTATE;


/**
 * State & config for AudioMixBufWrite, AudioMixBufSilence, AudioMixBufBlend and
 * AudioMixBufBlendGap, created by AudioMixBufInitWriteState.
 */
typedef struct AUDIOMIXBUFWRITESTATE
{
    /** Encodes @a cFrames from @a pvSrc to @a paDst. */
    DECLR3CALLBACKMEMBER(void,  pfnDecode,(int64_t *paDst, const void *pvSrc, uint32_t cFrames, struct AUDIOMIXBUFWRITESTATE *pState));
    /** Encodes @a cFrames from @a pvSrc blending into @a paDst. */
    DECLR3CALLBACKMEMBER(void,  pfnDecodeBlend,(int64_t *paDst, const void *pvSrc, uint32_t cFrames, struct AUDIOMIXBUFWRITESTATE *pState));
    /** Sample rate conversion state (only used when needed). */
    AUDIOSTREAMRATE             Rate;
    /** Destination (mixer) channels. */
    uint8_t                     cDstChannels;
    /** Source hannels. */
    uint8_t                     cSrcChannels;
    /** Source frame size. */
    uint8_t                     cbSrcFrame;
} AUDIOMIXBUFWRITESTATE;
/** Pointer to write state & config. */
typedef AUDIOMIXBUFWRITESTATE *PAUDIOMIXBUFWRITESTATE;


/**
 * Audio mixing buffer.
 */
typedef struct AUDIOMIXBUF
{
    /** Magic value (AUDIOMIXBUF_MAGIC). */
    uint32_t                    uMagic;
    uint8_t                     abPadding[4];
    /* ???Undocumented??? */
    RTLISTNODE                  Node;
    /** Name of the buffer. */
    char                       *pszName;
    /** Frame buffer. */
    PPDMAUDIOFRAME              pFrames;
    /** Size of the frame buffer (in audio frames). */
    uint32_t                    cFrames;
    /** The current read position (in frames). */
    uint32_t                    offRead;
    /** The current write position (in frames). */
    uint32_t                    offWrite;
    /** Total frames already mixed down to the parent buffer (if any).
     *
     * Always starting at the parent's offRead position.
     * @note Count always is specified in parent frames, as the sample count can
     *       differ between parent and child.  */
    uint32_t                    cMixed;
    /** How much audio frames are currently being used in this buffer.
     * @note This also is known as the distance in ring buffer terms. */
    uint32_t                    cUsed;
    /** Number of children mix buffers kept in lstChildren. */
    uint32_t                    cChildren;
    /** List of children mix buffers to keep in sync with (if being a parent buffer). */
    RTLISTANCHOR                lstChildren;
    /** Pointer to parent buffer (if any). */
    PAUDIOMIXBUF                pParent;
    /** Intermediate structure for buffer conversion tasks. */
    PAUDIOSTREAMRATE            pRate;
    /** Internal representation of current volume used for mixing. */
    AUDMIXBUFVOL                Volume;
    /** This buffer's audio format.
     * @todo r=bird: This seems to be a value created by AUDMIXBUF_AUDIO_FMT_MAKE(),
     *       which is not define here.  Does this structure really belong here at
     *       all?  */
    AUDIOMIXBUFFMT              uAudioFmt;
    /** Audio input properties.
     * @note There is only one set of audio properties here because we have one
     *       mixer buffer for the guest side and a separate one for the host side.
     * @todo r=bird: Why exactly do we need to use separate mixer buffers?
     *       Couldn't we just have different conversion fuctions and save the
     *       extra copying? */
    PDMAUDIOPCMPROPS            Props;
    /** Standard conversion-to function for set uAudioFmt. */
    PFNAUDIOMIXBUFCONVTO        pfnConvTo;
    /** Standard conversion-from function for set uAudioFmt. */
    PFNAUDIOMIXBUFCONVFROM      pfnConvFrom;

    /** Ratio of the associated parent stream's frequency by this stream's
     * frequency (1<<32), represented as a signed 64 bit integer.
     *
     * For example, if the parent stream has a frequency of 44 khZ, and this
     * stream has a frequency of 11 kHz, the ration then would be
     * (44/11 * (1 << 32)).
     *
     * Currently this does not get changed once assigned. */
    int64_t                     iFreqRatio;
} AUDIOMIXBUF;

/** Magic value for AUDIOMIXBUF (Antonio Lucio Vivaldi). */
#define AUDIOMIXBUF_MAGIC           UINT32_C(0x16780304)
/** Dead mixer buffer magic. */
#define AUDIOMIXBUF_MAGIC_DEAD      UINT32_C(0x17410728)


/** Constructs 32 bit value for given frequency, number of channels, bits per sample and signed bit.
 * @note This currently matches 1:1 the VRDE encoding -- this might change in the future, so better don't rely on this fact! */
#define AUDMIXBUF_AUDIO_FMT_MAKE(freq, c, bps, s) ((((s) & 0x1) << 28) + (((bps) & 0xFF) << 20) + (((c) & 0xF) << 16) + ((freq) & 0xFFFF))

/** Decodes frequency (Hz). */
#define AUDMIXBUF_FMT_SAMPLE_FREQ(a) ((a) & 0xFFFF)
/** Decodes number of channels. */
#define AUDMIXBUF_FMT_CHANNELS(a) (((a) >> 16) & 0xF)
/** Decodes signed bit. */
#define AUDMIXBUF_FMT_SIGNED(a) (((a) >> 28) & 0x1)
/** Decodes number of bits per sample. */
#define AUDMIXBUF_FMT_BITS_PER_SAMPLE(a) (((a) >> 20) & 0xFF)
/** Decodes number of bytes per sample. */
#define AUDMIXBUF_FMT_BYTES_PER_SAMPLE(a) ((AUDMIXBUF_AUDIO_FMT_BITS_PER_SAMPLE(a) + 7) / 8)

/** Converts (audio) frames to bytes. */
#define AUDIOMIXBUF_F2B(a_pMixBuf, a_cFrames)   PDMAUDIOPCMPROPS_F2B(&(a_pMixBuf)->Props, a_cFrames)
/** Converts bytes to (audio) frames.
 * @note Does *not* take the conversion ratio into account. */
#define AUDIOMIXBUF_B2F(a_pMixBuf, a_cb)        PDMAUDIOPCMPROPS_B2F(&(a_pMixBuf)->Props, a_cb)

/** Converts frames to bytes, respecting the conversion ratio to
 *  a linked buffer. */
#define AUDIOMIXBUF_F2B_RATIO(a_pMixBuf, a_cFrames) AUDIOMIXBUF_F2B(a_pMixBuf, AUDIOMIXBUF_F2F_RATIO(a_pMixBuf, a_cFrames))
/** Converts number of frames according to the buffer's ratio.
 * @todo r=bird: Why the *signed* cast?  */
#define AUDIOMIXBUF_F2F_RATIO(a_pMixBuf, a_cFrames) (((int64_t)(a_cFrames) << 32) / (a_pMixBuf)->iFreqRatio)


int     AudioMixBufInit(PAUDIOMIXBUF pMixBuf, const char *pszName, PCPDMAUDIOPCMPROPS pProps, uint32_t cFrames);
void    AudioMixBufDestroy(PAUDIOMIXBUF pMixBuf);

int     AudioMixBufInitPeekState(PCAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFPEEKSTATE pState, PCPDMAUDIOPCMPROPS pDstProps);
void    AudioMixBufPeek(PCAUDIOMIXBUF pMixBuf, uint32_t offSrcFrame, uint32_t cMaxSrcFrames, uint32_t *pcSrcFramesPeeked,
                        PAUDIOMIXBUFPEEKSTATE pState, void *pvDst, uint32_t cbDst, uint32_t *pcbDstPeeked);
void    AudioMixBufAdvance(PAUDIOMIXBUF pMixBuf, uint32_t cFrames);
void    AudioMixBufDrop(PAUDIOMIXBUF pMixBuf);

int     AudioMixBufInitWriteState(PCAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, PCPDMAUDIOPCMPROPS pSrcProps);
void    AudioMixBufWrite(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                         uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesWritten);
void    AudioMixBufSilence(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, uint32_t offFrame, uint32_t cFrames);
void    AudioMixBufBlend(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                         uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesBlended);
void    AudioMixBufBlendGap(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, uint32_t cFrames);
void    AudioMixBufCommit(PAUDIOMIXBUF pMixBuf, uint32_t cFrames);

void AudioMixBufClear(PAUDIOMIXBUF pMixBuf);
void AudioMixBufFinish(PAUDIOMIXBUF pMixBuf, uint32_t cFramesToClear);
uint32_t AudioMixBufFree(PAUDIOMIXBUF pMixBuf);
uint32_t AudioMixBufFreeBytes(PAUDIOMIXBUF pMixBuf);
bool AudioMixBufIsEmpty(PAUDIOMIXBUF pMixBuf);
int AudioMixBufLinkTo(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUF pParent);
uint32_t AudioMixBufLive(PAUDIOMIXBUF pMixBuf);
int AudioMixBufMixToParent(PAUDIOMIXBUF pMixBuf, uint32_t cSrcFrames, uint32_t *pcSrcMixed);
int AudioMixBufMixToParentEx(PAUDIOMIXBUF pMixBuf, uint32_t cSrcOffset, uint32_t cSrcFrames, uint32_t *pcSrcMixed);
int AudioMixBufPeekMutable(PAUDIOMIXBUF pMixBuf, uint32_t cFramesToRead, PPDMAUDIOFRAME *ppvSamples, uint32_t *pcFramesRead);
uint32_t AudioMixBufUsed(PAUDIOMIXBUF pMixBuf);
uint32_t AudioMixBufUsedBytes(PAUDIOMIXBUF pMixBuf);
int         AudioMixBufAcquireReadBlock(PAUDIOMIXBUF pMixBuf, void *pvBuf, uint32_t cbBuf, uint32_t *pcAcquiredFrames);
int         AudioMixBufAcquireReadBlockEx(PAUDIOMIXBUF pMixBuf, PCPDMAUDIOPCMPROPS pDstProps,
                                          void *pvBuf, uint32_t cbBuf, uint32_t *pcAcquiredFrames);
void AudioMixBufReleaseReadBlock(PAUDIOMIXBUF pMixBuf, uint32_t cFrames);
uint32_t AudioMixBufReadPos(PAUDIOMIXBUF pMixBuf);
void AudioMixBufReset(PAUDIOMIXBUF pMixBuf);
void AudioMixBufSetVolume(PAUDIOMIXBUF pMixBuf, PPDMAUDIOVOLUME pVol);
uint32_t AudioMixBufSize(PAUDIOMIXBUF pMixBuf);
uint32_t AudioMixBufSizeBytes(PAUDIOMIXBUF pMixBuf);
void AudioMixBufUnlink(PAUDIOMIXBUF pMixBuf);
int         AudioMixBufWriteAt(PAUDIOMIXBUF pMixBuf, uint32_t offSamples, const void *pvBuf, uint32_t cbBuf, uint32_t *pcWritten);
int         AudioMixBufWriteAtEx(PAUDIOMIXBUF pMixBuf, PCPDMAUDIOPCMPROPS pSrcProps, uint32_t offFrames,
                                 const void *pvBuf, uint32_t cbBuf, uint32_t *pcWritten);
int         AudioMixBufWriteCirc(PAUDIOMIXBUF pMixBuf, const void *pvBuf, uint32_t cbBuf, uint32_t *pcWritten);
int         AudioMixBufWriteCircEx(PAUDIOMIXBUF pMixBuf, PCPDMAUDIOPCMPROPS pSrcProps,
                                   const void *pvBuf,uint32_t cbBuf, uint32_t *pcWritten);
uint32_t AudioMixBufWritePos(PAUDIOMIXBUF pMixBuf);

#ifdef DEBUG
void AudioMixBufDbgPrint(PAUDIOMIXBUF pMixBuf);
void AudioMixBufDbgPrintChain(PAUDIOMIXBUF pMixBuf);
#endif

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioMixBuffer_h */

