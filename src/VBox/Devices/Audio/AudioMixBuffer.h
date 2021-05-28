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
    /** Size of the frame buffer (in audio frames). */
    uint32_t                    cFrames;
    /** Frame buffer. */
    PPDMAUDIOFRAME              pFrames;
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
    /** Audio properties for the buffer content - for frequency and channel count.
     * (This is the guest side PCM properties.) */
    PDMAUDIOPCMPROPS            Props;
    /** Internal representation of current volume used for mixing. */
    AUDMIXBUFVOL                Volume;
    /** Name of the buffer. */
    char                       *pszName;
} AUDIOMIXBUF;

/** Magic value for AUDIOMIXBUF (Antonio Lucio Vivaldi). */
#define AUDIOMIXBUF_MAGIC           UINT32_C(0x16780304)
/** Dead mixer buffer magic. */
#define AUDIOMIXBUF_MAGIC_DEAD      UINT32_C(0x17410728)

/** Converts (audio) frames to bytes. */
#define AUDIOMIXBUF_F2B(a_pMixBuf, a_cFrames)   PDMAUDIOPCMPROPS_F2B(&(a_pMixBuf)->Props, a_cFrames)
/** Converts bytes to (audio) frames.
 * @note Does *not* take the conversion ratio into account. */
#define AUDIOMIXBUF_B2F(a_pMixBuf, a_cb)        PDMAUDIOPCMPROPS_B2F(&(a_pMixBuf)->Props, a_cb)


int         AudioMixBufInit(PAUDIOMIXBUF pMixBuf, const char *pszName, PCPDMAUDIOPCMPROPS pProps, uint32_t cFrames);
void        AudioMixBufDestroy(PAUDIOMIXBUF pMixBuf);
void        AudioMixBufDrop(PAUDIOMIXBUF pMixBuf);
void        AudioMixBufSetVolume(PAUDIOMIXBUF pMixBuf, PCPDMAUDIOVOLUME pVol);

/** @name Mixer buffer getters
 * @{ */
uint32_t    AudioMixBufSize(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufSizeBytes(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufUsed(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufUsedBytes(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufFree(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufFreeBytes(PCAUDIOMIXBUF pMixBuf);
bool        AudioMixBufIsEmpty(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufReadPos(PCAUDIOMIXBUF pMixBuf);
uint32_t    AudioMixBufWritePos(PCAUDIOMIXBUF pMixBuf);
/** @} */

/** @name Mixer buffer reading
 *  @{ */
int         AudioMixBufInitPeekState(PCAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFPEEKSTATE pState, PCPDMAUDIOPCMPROPS pDstProps);
void        AudioMixBufPeek(PCAUDIOMIXBUF pMixBuf, uint32_t offSrcFrame, uint32_t cMaxSrcFrames, uint32_t *pcSrcFramesPeeked,
                            PAUDIOMIXBUFPEEKSTATE pState, void *pvDst, uint32_t cbDst, uint32_t *pcbDstPeeked);
void        AudioMixBufAdvance(PAUDIOMIXBUF pMixBuf, uint32_t cFrames);
/** @} */

/** @name Mixer buffer writing
 * @{ */
int         AudioMixBufInitWriteState(PCAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, PCPDMAUDIOPCMPROPS pSrcProps);
void        AudioMixBufWrite(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                             uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesWritten);
void        AudioMixBufSilence(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, uint32_t offFrame, uint32_t cFrames);
void        AudioMixBufBlend(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                             uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesBlended);
void        AudioMixBufBlendGap(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, uint32_t cFrames);
void        AudioMixBufCommit(PAUDIOMIXBUF pMixBuf, uint32_t cFrames);
/** @} */

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioMixBuffer_h */

