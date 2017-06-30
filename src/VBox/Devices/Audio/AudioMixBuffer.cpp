/* $Id$ */
/** @file
 * VBox audio: Audio mixing buffer for converting reading/writing audio
 *             samples.
 */

/*
 * Copyright (C) 2014-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOG_GROUP LOG_GROUP_AUDIO_MIXER_BUFFER
#include <VBox/log.h>

#if 0
/*
 * AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA enables dumping the raw PCM data
 * to a file on the host. Be sure to adjust AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH
 * to your needs before using this!
 */
# define AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
# ifdef RT_OS_WINDOWS
#  define AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "c:\\temp\\"
# else
#  define AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "/tmp/"
# endif
/* Warning: Enabling this will generate *huge* logs! */
//# define AUDIOMIXBUF_DEBUG_MACROS
#endif

#include <iprt/asm-math.h>
#include <iprt/assert.h>
#ifdef AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
# include <iprt/file.h>
#endif
#include <iprt/mem.h>
#include <iprt/string.h> /* For RT_BZERO. */

#ifdef VBOX_AUDIO_TESTCASE
# define LOG_ENABLED
# include <iprt/stream.h>
#endif
#include <VBox/err.h>

#include "AudioMixBuffer.h"

#ifndef VBOX_AUDIO_TESTCASE
# ifdef DEBUG
#  define AUDMIXBUF_LOG(x) LogFlowFunc(x)
# else
# define AUDMIXBUF_LOG(x) do {} while (0)
# endif
#else /* VBOX_AUDIO_TESTCASE */
# define AUDMIXBUF_LOG(x) RTPrintf x
#endif

#ifdef DEBUG
DECLINLINE(void)        audioMixBufDbgPrintInternal(PPDMAUDIOMIXBUF pMixBuf, const char *pszFunc);
DECL_FORCE_INLINE(bool) audioMixBufDbgValidate(PPDMAUDIOMIXBUF pMixBuf);
#endif

/*
 *   Soft Volume Control
 *
 * The external code supplies an 8-bit volume (attenuation) value in the
 * 0 .. 255 range. This represents 0 to -96dB attenuation where an input
 * value of 0 corresponds to -96dB and 255 corresponds to 0dB (unchanged).
 *
 * Each step thus corresponds to 96 / 256 or 0.375dB. Every 6dB (16 steps)
 * represents doubling the sample value.
 *
 * For internal use, the volume control needs to be converted to a 16-bit
 * (sort of) exponential value between 1 and 65536. This is used with fixed
 * point arithmetic such that 65536 means 1.0 and 1 means 1/65536.
 *
 * For actual volume calculation, 33.31 fixed point is used. Maximum (or
 * unattenuated) volume is represented as 0x40000000; conveniently, this
 * value fits into a uint32_t.
 *
 * To enable fast processing, the maximum volume must be a power of two
 * and must not have a sign when converted to int32_t. While 0x80000000
 * violates these constraints, 0x40000000 does not.
 */


/** Logarithmic/exponential volume conversion table. */
static uint32_t s_aVolumeConv[256] = {
        1,     1,     1,     1,     1,     1,     1,     1, /*   7 */
        1,     2,     2,     2,     2,     2,     2,     2, /*  15 */
        2,     2,     2,     2,     2,     3,     3,     3, /*  23 */
        3,     3,     3,     3,     4,     4,     4,     4, /*  31 */
        4,     4,     5,     5,     5,     5,     5,     6, /*  39 */
        6,     6,     6,     7,     7,     7,     8,     8, /*  47 */
        8,     9,     9,    10,    10,    10,    11,    11, /*  55 */
       12,    12,    13,    13,    14,    15,    15,    16, /*  63 */
       17,    17,    18,    19,    20,    21,    22,    23, /*  71 */
       24,    25,    26,    27,    28,    29,    31,    32, /*  79 */
       33,    35,    36,    38,    40,    41,    43,    45, /*  87 */
       47,    49,    52,    54,    56,    59,    61,    64, /*  95 */
       67,    70,    73,    76,    79,    83,    87,    91, /* 103 */
       95,    99,   103,   108,   112,   117,   123,   128, /* 111 */
      134,   140,   146,   152,   159,   166,   173,   181, /* 119 */
      189,   197,   206,   215,   225,   235,   245,   256, /* 127 */
      267,   279,   292,   304,   318,   332,   347,   362, /* 135 */
      378,   395,   412,   431,   450,   470,   490,   512, /* 143 */
      535,   558,   583,   609,   636,   664,   693,   724, /* 151 */
      756,   790,   825,   861,   899,   939,   981,  1024, /* 159 */
     1069,  1117,  1166,  1218,  1272,  1328,  1387,  1448, /* 167 */
     1512,  1579,  1649,  1722,  1798,  1878,  1961,  2048, /* 175 */
     2139,  2233,  2332,  2435,  2543,  2656,  2774,  2896, /* 183 */
     3025,  3158,  3298,  3444,  3597,  3756,  3922,  4096, /* 191 */
     4277,  4467,  4664,  4871,  5087,  5312,  5547,  5793, /* 199 */
     6049,  6317,  6597,  6889,  7194,  7512,  7845,  8192, /* 207 */
     8555,  8933,  9329,  9742, 10173, 10624, 11094, 11585, /* 215 */
    12098, 12634, 13193, 13777, 14387, 15024, 15689, 16384, /* 223 */
    17109, 17867, 18658, 19484, 20347, 21247, 22188, 23170, /* 231 */
    24196, 25268, 26386, 27554, 28774, 30048, 31379, 32768, /* 239 */
    34219, 35734, 37316, 38968, 40693, 42495, 44376, 46341, /* 247 */
    48393, 50535, 52773, 55109, 57549, 60097, 62757, 65536, /* 255 */
};

/* Bit shift for fixed point conversion. */
#define AUDIOMIXBUF_VOL_SHIFT       30

/* Internal representation of 0dB volume (1.0 in fixed point). */
#define AUDIOMIXBUF_VOL_0DB         (1 << AUDIOMIXBUF_VOL_SHIFT)

AssertCompile(AUDIOMIXBUF_VOL_0DB <= 0x40000000);   /* Must always hold. */
AssertCompile(AUDIOMIXBUF_VOL_0DB == 0x40000000);   /* For now -- when only attenuation is used. */


/**
 * Peeks for audio samples without any conversion done.
 * This will get the raw sample data out of a mixing buffer.
 *
 * @return  IPRT status code or VINF_AUDIO_MORE_DATA_AVAILABLE if more data is available to read.
 *
 * @param   pMixBuf                 Mixing buffer to acquire audio samples from.
 * @param   cSamplesToRead          Number of audio samples to read.
 * @param   paSampleBuf             Buffer where to store the returned audio samples.
 * @param   cSampleBuf              Size (in samples) of the buffer to store audio samples into.
 * @param   pcSamplesRead           Returns number of read audio samples. Optional.
 *
 * @remark  This function is not thread safe!
 */
int AudioMixBufPeek(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamplesToRead,
                    PPDMAUDIOSAMPLE paSampleBuf, uint32_t cSampleBuf, uint32_t *pcSamplesRead)
{
    AssertPtrReturn(pMixBuf,     VERR_INVALID_POINTER);
    AssertPtrReturn(paSampleBuf, VERR_INVALID_POINTER);
    AssertReturn(cSampleBuf,     VERR_INVALID_PARAMETER);
    /* pcRead is optional. */

    int rc;

    if (!cSamplesToRead)
    {
        if (pcSamplesRead)
            *pcSamplesRead = 0;
        return VINF_SUCCESS;
    }

    uint32_t cRead;
    if (pMixBuf->offRead + cSamplesToRead > pMixBuf->cSamples)
    {
        cRead = pMixBuf->cSamples - pMixBuf->offRead;
        rc = VINF_AUDIO_MORE_DATA_AVAILABLE;
    }
    else
    {
        cRead = cSamplesToRead;
        rc = VINF_SUCCESS;
    }

    if (cRead > cSampleBuf)
    {
        cRead = cSampleBuf;
        rc = VINF_AUDIO_MORE_DATA_AVAILABLE;
    }

    if (cRead)
    {
        memcpy(paSampleBuf, &pMixBuf->pSamples[pMixBuf->offRead], sizeof(PDMAUDIOSAMPLE) * cRead);

        pMixBuf->offRead = (pMixBuf->offRead + cRead) % pMixBuf->cSamples;
        Assert(pMixBuf->offRead <= pMixBuf->cSamples);
        pMixBuf->cUsed  -= RT_MIN(cRead, pMixBuf->cUsed);
    }

    if (pcSamplesRead)
        *pcSamplesRead = cRead;

    return rc;
}

/**
 * Returns a mutable pointer to the mixing buffer's audio sample buffer for writing raw
 * audio samples.
 *
 * @return  IPRT status code. VINF_TRY_AGAIN for getting next pointer at beginning (circular).
 * @param   pMixBuf                 Mixing buffer to acquire audio samples from.
 * @param   cSamples                Number of requested audio samples to write.
 * @param   ppvSamples              Returns a mutable pointer to the buffer's audio sample data.
 * @param   pcSamplesToWrite        Number of available audio samples to write.
 *
 * @remark  This function is not thread safe!
 */
int AudioMixBufPeekMutable(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamples,
                           PPDMAUDIOSAMPLE *ppvSamples, uint32_t *pcSamplesToWrite)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvSamples, VERR_INVALID_POINTER);
    AssertPtrReturn(pcSamplesToWrite, VERR_INVALID_POINTER);

    int rc;

    if (!cSamples)
    {
        *pcSamplesToWrite = 0;
        return VINF_SUCCESS;
    }

    uint32_t cSamplesToWrite;
    if (pMixBuf->offWrite + cSamples > pMixBuf->cSamples)
    {
        cSamplesToWrite = pMixBuf->cSamples - pMixBuf->offWrite;
        rc = VINF_TRY_AGAIN;
    }
    else
    {
        cSamplesToWrite = cSamples;
        rc = VINF_SUCCESS;
    }

    *ppvSamples = &pMixBuf->pSamples[pMixBuf->offWrite];
    AssertPtr(ppvSamples);

    pMixBuf->offWrite = (pMixBuf->offWrite + cSamplesToWrite) % pMixBuf->cSamples;
    Assert(pMixBuf->offWrite <= pMixBuf->cSamples);
    pMixBuf->cUsed += RT_MIN(cSamplesToWrite, pMixBuf->cUsed);

    *pcSamplesToWrite = cSamplesToWrite;

    return rc;
}

/**
 * Clears the entire sample buffer.
 *
 * @param   pMixBuf                 Mixing buffer to clear.
 *
 */
void AudioMixBufClear(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturnVoid(pMixBuf);

    if (pMixBuf->cSamples)
        RT_BZERO(pMixBuf->pSamples, pMixBuf->cSamples * sizeof(PDMAUDIOSAMPLE));
}

/**
 * Clears (zeroes) the buffer by a certain amount of (used) samples and
 * keeps track to eventually assigned children buffers.
 *
 * @param   pMixBuf                 Mixing buffer to clear.
 * @param   cSamplesToClear         Number of audio samples to clear.
 */
void AudioMixBufFinish(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamplesToClear)
{
    AUDMIXBUF_LOG(("cSamplesToClear=%RU32\n", cSamplesToClear));
    AUDMIXBUF_LOG(("%s: offRead=%RU32, cUsed=%RU32\n",
                   pMixBuf->pszName, pMixBuf->offRead, pMixBuf->cUsed));

    PPDMAUDIOMIXBUF pIter;
    RTListForEach(&pMixBuf->lstChildren, pIter, PDMAUDIOMIXBUF, Node)
    {
        AUDMIXBUF_LOG(("\t%s: cMixed=%RU32 -> %RU32\n",
                       pIter->pszName, pIter->cMixed, pIter->cMixed - cSamplesToClear));

        pIter->cMixed -= RT_MIN(pIter->cMixed, cSamplesToClear);
        /* Note: Do not increment pIter->cUsed here, as this gets done when reading from that buffer using AudioMixBufReadXXX. */
    }

    Assert(cSamplesToClear <= pMixBuf->cSamples);

    uint32_t cClearOff;
    uint32_t cClearLen;

    /* Clear end of buffer (wrap around). */
    if (cSamplesToClear > pMixBuf->offRead)
    {
        cClearOff = pMixBuf->cSamples - (cSamplesToClear - pMixBuf->offRead);
        cClearLen = pMixBuf->cSamples - cClearOff;

        AUDMIXBUF_LOG(("Clearing1: %RU32 - %RU32\n", cClearOff, cClearOff + cClearLen));

        RT_BZERO(pMixBuf->pSamples + cClearOff, cClearLen * sizeof(PDMAUDIOSAMPLE));

        Assert(cSamplesToClear >= cClearLen);
        cSamplesToClear -= cClearLen;
    }

    /* Clear beginning of buffer. */
    if (   cSamplesToClear
        && pMixBuf->offRead)
    {
        Assert(pMixBuf->offRead >= cSamplesToClear);

        cClearOff = pMixBuf->offRead - cSamplesToClear;
        cClearLen = cSamplesToClear;

        Assert(cClearOff + cClearLen <= pMixBuf->cSamples);

        AUDMIXBUF_LOG(("Clearing2: %RU32 - %RU32\n", cClearOff, cClearOff + cClearLen));

        RT_BZERO(pMixBuf->pSamples + cClearOff, cClearLen * sizeof(PDMAUDIOSAMPLE));
    }
}

/**
 * Destroys (uninitializes) a mixing buffer.
 *
 * @param   pMixBuf                 Mixing buffer to destroy.
 */
void AudioMixBufDestroy(PPDMAUDIOMIXBUF pMixBuf)
{
    if (!pMixBuf)
        return;

    AudioMixBufUnlink(pMixBuf);

    if (pMixBuf->pszName)
    {
        AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

        RTStrFree(pMixBuf->pszName);
        pMixBuf->pszName = NULL;
    }

    if (pMixBuf->pRate)
    {
        RTMemFree(pMixBuf->pRate);
        pMixBuf->pRate = NULL;
    }

    if (pMixBuf->pSamples)
    {
        Assert(pMixBuf->cSamples);

        RTMemFree(pMixBuf->pSamples);
        pMixBuf->pSamples = NULL;
    }

    pMixBuf->cSamples = 0;
}

/**
 * Returns the size (in audio samples) of free audio buffer space.
 *
 * @return  uint32_t                Size (in audio samples) of free audio buffer space.
 * @param   pMixBuf                 Mixing buffer to return free size for.
 */
uint32_t AudioMixBufFree(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    uint32_t cSamples, cSamplesFree;
    if (pMixBuf->pParent)
    {
        /*
         * As a linked child buffer we want to know how many samples
         * already have been consumed by the parent.
         */
        cSamples = pMixBuf->pParent->cSamples;

        Assert(pMixBuf->cMixed <= cSamples);
        cSamplesFree = cSamples - pMixBuf->cMixed;
    }
    else /* As a parent. */
    {
        cSamples     = pMixBuf->cSamples;
        Assert(cSamples >= pMixBuf->cUsed);
        cSamplesFree = pMixBuf->cSamples - pMixBuf->cUsed;
    }

    AUDMIXBUF_LOG(("%s: %RU32 of %RU32\n", pMixBuf->pszName, cSamplesFree, cSamples));
    return cSamplesFree;
}

/**
 * Returns the size (in bytes) of free audio buffer space.
 *
 * @return  uint32_t                Size (in bytes) of free audio buffer space.
 * @param   pMixBuf                 Mixing buffer to return free size for.
 */
uint32_t AudioMixBufFreeBytes(PPDMAUDIOMIXBUF pMixBuf)
{
    return AUDIOMIXBUF_S2B(pMixBuf, AudioMixBufFree(pMixBuf));
}

/**
 * Allocates the internal audio sample buffer.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to allocate sample buffer for.
 * @param   cSamples                Number of audio samples to allocate.
 */
static int audioMixBufAlloc(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamples)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertReturn(cSamples, VERR_INVALID_PARAMETER);

    AUDMIXBUF_LOG(("%s: cSamples=%RU32\n", pMixBuf->pszName, cSamples));

    size_t cbSamples = cSamples * sizeof(PDMAUDIOSAMPLE);
    pMixBuf->pSamples = (PPDMAUDIOSAMPLE)RTMemAllocZ(cbSamples);
    if (pMixBuf->pSamples)
    {
        pMixBuf->cSamples = cSamples;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}

#ifdef AUDIOMIXBUF_DEBUG_MACROS
# define AUDMIXBUF_MACRO_LOG(x) AUDMIXBUF_LOG(x)
#elif defined(VBOX_AUDIO_TESTCASE_VERBOSE) /* Warning: VBOX_AUDIO_TESTCASE_VERBOSE will generate huge logs! */
# define AUDMIXBUF_MACRO_LOG(x) RTPrintf x
#else
# define AUDMIXBUF_MACRO_LOG(x) do {} while (0)
#endif

/**
 * Macro for generating the conversion routines from/to different formats.
 * Be careful what to pass in/out, as most of the macros are optimized for speed and
 * thus don't do any bounds checking!
 *
 * Note: Currently does not handle any endianness conversion yet!
 */
#define AUDMIXBUF_CONVERT(_aName, _aType, _aMin, _aMax, _aSigned, _aShift) \
    /* Clips a specific output value to a single sample value. */ \
    DECLCALLBACK(int64_t) audioMixBufClipFrom##_aName(_aType aVal) \
    { \
        if (_aSigned) \
            return (int64_t) (((uint64_t) ((int64_t) aVal                     )) << (32 - _aShift)); \
        return     (int64_t) (((uint64_t) ((int64_t) aVal - ((_aMax >> 1) + 1))) << (32 - _aShift)); \
    } \
    \
    /* Clips a single sample value to a specific output value. */ \
    DECLCALLBACK(_aType) audioMixBufClipTo##_aName(int64_t iVal) \
    { \
        if (iVal >= 0x7fffffff) \
            return _aMax; \
        if (iVal < -INT64_C(0x80000000)) \
            return _aMin; \
        \
        if (_aSigned) \
            return (_aType) (iVal >> (32 - _aShift)); \
        return ((_aType) ((iVal >> (32 - _aShift)) + ((_aMax >> 1) + 1))); \
    } \
    \
    DECLCALLBACK(uint32_t) audioMixBufConvFrom##_aName##Stereo(PPDMAUDIOSAMPLE paDst, const void *pvSrc, uint32_t cbSrc, \
                                                               PCPDMAUDMIXBUFCONVOPTS pOpts) \
    { \
        _aType const *pSrc = (_aType const *)pvSrc; \
        uint32_t cSamples = RT_MIN(pOpts->cSamples, cbSrc / sizeof(_aType)); \
        AUDMIXBUF_MACRO_LOG(("cSamples=%RU32, BpS=%zu, lVol=%RU32, rVol=%RU32\n", \
                             pOpts->cSamples, sizeof(_aType), pOpts->From.Volume.uLeft, pOpts->From.Volume.uRight)); \
        for (uint32_t i = 0; i < cSamples; i++) \
        { \
            paDst->i64LSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##_aName(*pSrc++), pOpts->From.Volume.uLeft ) >> AUDIOMIXBUF_VOL_SHIFT; \
            paDst->i64RSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##_aName(*pSrc++), pOpts->From.Volume.uRight) >> AUDIOMIXBUF_VOL_SHIFT; \
            paDst++; \
        } \
        \
        return cSamples; \
    } \
    \
    DECLCALLBACK(uint32_t) audioMixBufConvFrom##_aName##Mono(PPDMAUDIOSAMPLE paDst, const void *pvSrc, uint32_t cbSrc, \
                                                             PCPDMAUDMIXBUFCONVOPTS pOpts) \
    { \
        _aType const *pSrc = (_aType const *)pvSrc; \
        const uint32_t cSamples = RT_MIN(pOpts->cSamples, cbSrc / sizeof(_aType)); \
        AUDMIXBUF_MACRO_LOG(("cSamples=%RU32, BpS=%zu, lVol=%RU32, rVol=%RU32\n", \
                             cSamples, sizeof(_aType), pOpts->From.Volume.uLeft, pOpts->From.Volume.uRight)); \
        for (uint32_t i = 0; i < cSamples; i++) \
        { \
            paDst->i64LSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##_aName(*pSrc), pOpts->From.Volume.uLeft)  >> AUDIOMIXBUF_VOL_SHIFT; \
            paDst->i64RSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##_aName(*pSrc), pOpts->From.Volume.uRight) >> AUDIOMIXBUF_VOL_SHIFT; \
            pSrc++; \
            paDst++; \
        } \
        \
        return cSamples; \
    } \
    \
    DECLCALLBACK(void) audioMixBufConvTo##_aName##Stereo(void *pvDst, PCPDMAUDIOSAMPLE paSrc, PCPDMAUDMIXBUFCONVOPTS pOpts) \
    { \
        PCPDMAUDIOSAMPLE pSrc = paSrc; \
        _aType *pDst = (_aType *)pvDst; \
        _aType l, r; \
        uint32_t cSamples = pOpts->cSamples; \
        while (cSamples--) \
        { \
            AUDMIXBUF_MACRO_LOG(("%p: l=%RI64, r=%RI64\n", pSrc, pSrc->i64LSample, pSrc->i64RSample)); \
            l = audioMixBufClipTo##_aName(pSrc->i64LSample); \
            r = audioMixBufClipTo##_aName(pSrc->i64RSample); \
            AUDMIXBUF_MACRO_LOG(("\t-> l=%RI16, r=%RI16\n", l, r)); \
            *pDst++ = l; \
            *pDst++ = r; \
            pSrc++; \
        } \
    } \
    \
    DECLCALLBACK(void) audioMixBufConvTo##_aName##Mono(void *pvDst, PCPDMAUDIOSAMPLE paSrc, PCPDMAUDMIXBUFCONVOPTS pOpts) \
    { \
        PCPDMAUDIOSAMPLE pSrc = paSrc; \
        _aType *pDst = (_aType *)pvDst; \
        uint32_t cSamples = pOpts->cSamples; \
        while (cSamples--) \
        { \
            *pDst++ = audioMixBufClipTo##_aName((pSrc->i64LSample + pSrc->i64RSample) / 2); \
            pSrc++; \
        } \
    }

/* audioMixBufConvXXXS8: 8 bit, signed. */
AUDMIXBUF_CONVERT(S8 /* Name */,  int8_t,   INT8_MIN  /* Min */, INT8_MAX   /* Max */, true  /* fSigned */, 8  /* cShift */)
/* audioMixBufConvXXXU8: 8 bit, unsigned. */
AUDMIXBUF_CONVERT(U8 /* Name */,  uint8_t,  0         /* Min */, UINT8_MAX  /* Max */, false /* fSigned */, 8  /* cShift */)
/* audioMixBufConvXXXS16: 16 bit, signed. */
AUDMIXBUF_CONVERT(S16 /* Name */, int16_t,  INT16_MIN /* Min */, INT16_MAX  /* Max */, true  /* fSigned */, 16 /* cShift */)
/* audioMixBufConvXXXU16: 16 bit, unsigned. */
AUDMIXBUF_CONVERT(U16 /* Name */, uint16_t, 0         /* Min */, UINT16_MAX /* Max */, false /* fSigned */, 16 /* cShift */)
/* audioMixBufConvXXXS32: 32 bit, signed. */
AUDMIXBUF_CONVERT(S32 /* Name */, int32_t,  INT32_MIN /* Min */, INT32_MAX  /* Max */, true  /* fSigned */, 32 /* cShift */)
/* audioMixBufConvXXXU32: 32 bit, unsigned. */
AUDMIXBUF_CONVERT(U32 /* Name */, uint32_t, 0         /* Min */, UINT32_MAX /* Max */, false /* fSigned */, 32 /* cShift */)

#undef AUDMIXBUF_CONVERT

#define AUDMIXBUF_MIXOP(_aName, _aOp) \
    static void audioMixBufOp##_aName(PPDMAUDIOSAMPLE paDst, uint32_t cDstSamples, \
                                      PPDMAUDIOSAMPLE paSrc, uint32_t cSrcSamples, \
                                      PPDMAUDIOSTRMRATE pRate, \
                                      uint32_t *pcDstWritten, uint32_t *pcSrcRead) \
    { \
        AUDMIXBUF_MACRO_LOG(("cSrcSamples=%RU32, cDstSamples=%RU32\n", cSrcSamples, cDstSamples)); \
        AUDMIXBUF_MACRO_LOG(("Rate: srcOffset=%RU32, dstOffset=%RU32, dstInc=%RU32\n", \
                             pRate->srcOffset, \
                             (uint32_t)(pRate->dstOffset >> 32), (uint32_t)(pRate->dstInc >> 32))); \
        \
        if (pRate->dstInc == (UINT64_C(1) + UINT32_MAX)) /* No conversion needed? */ \
        { \
            uint32_t cSamples = RT_MIN(cSrcSamples, cDstSamples); \
            AUDMIXBUF_MACRO_LOG(("cSamples=%RU32\n", cSamples)); \
            for (uint32_t i = 0; i < cSamples; i++) \
            { \
                paDst[i].i64LSample _aOp paSrc[i].i64LSample; \
                paDst[i].i64RSample _aOp paSrc[i].i64RSample; \
            } \
            \
            if (pcDstWritten) \
                *pcDstWritten = cSamples; \
            if (pcSrcRead) \
                *pcSrcRead = cSamples; \
            return; \
        } \
        \
        PPDMAUDIOSAMPLE paSrcStart = paSrc; \
        PPDMAUDIOSAMPLE paSrcEnd   = paSrc + cSrcSamples; \
        PPDMAUDIOSAMPLE paDstStart = paDst; \
        PPDMAUDIOSAMPLE paDstEnd   = paDst + cDstSamples; \
        PDMAUDIOSAMPLE  samCur     = { 0 }; \
        PDMAUDIOSAMPLE  samOut; \
        PDMAUDIOSAMPLE  samLast    = pRate->srcSampleLast; \
        \
        while (paDst < paDstEnd) \
        { \
            Assert(paSrc <= paSrcEnd); \
            Assert(paDst <= paDstEnd); \
            if (paSrc >= paSrcEnd) \
                break; \
            \
            while (pRate->srcOffset <= (pRate->dstOffset >> 32)) \
            { \
                Assert(paSrc <= paSrcEnd); \
                samLast = *paSrc++; \
                pRate->srcOffset++; \
                if (paSrc == paSrcEnd) \
                    break; \
            } \
            \
            Assert(paSrc <= paSrcEnd); \
            if (paSrc == paSrcEnd) \
                break; \
            \
            samCur = *paSrc; \
            \
            /* Interpolate. */ \
            int64_t iDstOffInt = pRate->dstOffset & UINT32_MAX; \
            \
            samOut.i64LSample = (samLast.i64LSample * ((int64_t) (INT64_C(1) << 32) - iDstOffInt) + samCur.i64LSample * iDstOffInt) >> 32; \
            samOut.i64RSample = (samLast.i64RSample * ((int64_t) (INT64_C(1) << 32) - iDstOffInt) + samCur.i64RSample * iDstOffInt) >> 32; \
            \
            paDst->i64LSample _aOp samOut.i64LSample; \
            paDst->i64RSample _aOp samOut.i64RSample; \
            \
            AUDMIXBUF_MACRO_LOG(("\tiDstOffInt=%RI64, l=%RI64, r=%RI64 (cur l=%RI64, r=%RI64)\n", \
                                 iDstOffInt, \
                                 paDst->i64LSample >> 32, paDst->i64RSample >> 32, \
                                 samCur.i64LSample >> 32, samCur.i64RSample >> 32)); \
            \
            paDst++; \
            pRate->dstOffset += pRate->dstInc; \
            \
            AUDMIXBUF_MACRO_LOG(("\t\tpRate->dstOffset=%RU32\n", pRate->dstOffset >> 32)); \
            \
        } \
        \
        AUDMIXBUF_MACRO_LOG(("%zu source samples -> %zu dest samples\n", paSrc - paSrcStart, paDst - paDstStart)); \
        \
        pRate->srcSampleLast = samLast; \
        \
        AUDMIXBUF_MACRO_LOG(("pRate->srcSampleLast l=%RI64, r=%RI64\n", \
                              pRate->srcSampleLast.i64LSample, pRate->srcSampleLast.i64RSample)); \
        \
        if (pcDstWritten) \
            *pcDstWritten = paDst - paDstStart; \
        if (pcSrcRead) \
            *pcSrcRead = paSrc - paSrcStart; \
    }

/* audioMixBufOpAssign: Assigns values from source buffer to destination bufffer, overwriting the destination. */
AUDMIXBUF_MIXOP(Assign /* Name */,  = /* Operation */)
#if 0 /* unused */
/* audioMixBufOpBlend: Blends together the values from both, the source and the destination buffer. */
AUDMIXBUF_MIXOP(Blend  /* Name */, += /* Operation */)
#endif

#undef AUDMIXBUF_MIXOP
#undef AUDMIXBUF_MACRO_LOG

/** Dummy conversion used when the source is muted. */
static DECLCALLBACK(uint32_t)
audioMixBufConvFromSilence(PPDMAUDIOSAMPLE paDst, const void *pvSrc, uint32_t cbSrc, PCPDMAUDMIXBUFCONVOPTS pOpts)
{
    RT_NOREF(cbSrc, pvSrc);

    /* Internally zero always corresponds to silence. */
    RT_BZERO(paDst, pOpts->cSamples * sizeof(paDst[0]));
    return pOpts->cSamples;
}

/**
 * Looks up the matching conversion (macro) routine for converting
 * audio samples from a source format.
 *
 ** @todo Speed up the lookup by binding it to the actual stream state.
 *
 * @return  PAUDMIXBUF_FN_CONVFROM  Function pointer to conversion macro if found, NULL if not supported.
 * @param   enmFmt                  Audio format to lookup conversion macro for.
 */
static PFNPDMAUDIOMIXBUFCONVFROM audioMixBufConvFromLookup(PDMAUDIOMIXBUFFMT enmFmt)
{
    if (AUDMIXBUF_FMT_SIGNED(enmFmt))
    {
        if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 2)
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvFromS8Stereo;
                case 16: return audioMixBufConvFromS16Stereo;
                case 32: return audioMixBufConvFromS32Stereo;
                default: return NULL;
            }
        }
        else
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvFromS8Mono;
                case 16: return audioMixBufConvFromS16Mono;
                case 32: return audioMixBufConvFromS32Mono;
                default: return NULL;
            }
        }
    }
    else /* Unsigned */
    {
        if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 2)
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvFromU8Stereo;
                case 16: return audioMixBufConvFromU16Stereo;
                case 32: return audioMixBufConvFromU32Stereo;
                default: return NULL;
            }
        }
        else
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvFromU8Mono;
                case 16: return audioMixBufConvFromU16Mono;
                case 32: return audioMixBufConvFromU32Mono;
                default: return NULL;
            }
        }
    }
    /* not reached */
}

/**
 * Looks up the matching conversion (macro) routine for converting
 * audio samples to a destination format.
 *
 ** @todo Speed up the lookup by binding it to the actual stream state.
 *
 * @return  PAUDMIXBUF_FN_CONVTO    Function pointer to conversion macro if found, NULL if not supported.
 * @param   enmFmt                  Audio format to lookup conversion macro for.
 */
static PFNPDMAUDIOMIXBUFCONVTO audioMixBufConvToLookup(PDMAUDIOMIXBUFFMT enmFmt)
{
    if (AUDMIXBUF_FMT_SIGNED(enmFmt))
    {
        if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 2)
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvToS8Stereo;
                case 16: return audioMixBufConvToS16Stereo;
                case 32: return audioMixBufConvToS32Stereo;
                default: return NULL;
            }
        }
        else
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvToS8Mono;
                case 16: return audioMixBufConvToS16Mono;
                case 32: return audioMixBufConvToS32Mono;
                default: return NULL;
            }
        }
    }
    else /* Unsigned */
    {
        if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 2)
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvToU8Stereo;
                case 16: return audioMixBufConvToU16Stereo;
                case 32: return audioMixBufConvToU32Stereo;
                default: return NULL;
            }
        }
        else
        {
            switch (AUDMIXBUF_FMT_BITS_PER_SAMPLE(enmFmt))
            {
                case 8:  return audioMixBufConvToU8Mono;
                case 16: return audioMixBufConvToU16Mono;
                case 32: return audioMixBufConvToU32Mono;
                default: return NULL;
            }
        }
    }
    /* not reached */
}

/**
 * Converts a PDM audio volume to an internal mixing buffer volume.
 *
 * @returns IPRT status code.
 * @param   pVolDst                 Where to store the converted mixing buffer volume.
 * @param   pVolSrc                 Volume to convert.
 */
static int audioMixBufConvVol(PPDMAUDMIXBUFVOL pVolDst, PPDMAUDIOVOLUME pVolSrc)
{
    if (!pVolSrc->fMuted) /* Only change/convert the volume value if we're not muted. */
    {
        uint8_t uVolL = pVolSrc->uLeft  & 0xFF;
        uint8_t uVolR = pVolSrc->uRight & 0xFF;

        /** @todo Ensure that the input is in the correct range/initialized! */
        pVolDst->uLeft  = s_aVolumeConv[uVolL] * (AUDIOMIXBUF_VOL_0DB >> 16);
        pVolDst->uRight = s_aVolumeConv[uVolR] * (AUDIOMIXBUF_VOL_0DB >> 16);
    }

    pVolDst->fMuted = pVolSrc->fMuted;

    return VINF_SUCCESS;
}

/**
 * Initializes a mixing buffer.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to initialize.
 * @param   pszName                 Name of mixing buffer for easier identification. Optional.
 * @param   pProps                  PCM audio properties to use for the mixing buffer.
 * @param   cSamples                Maximum number of audio samples the mixing buffer can hold.
 */
int AudioMixBufInit(PPDMAUDIOMIXBUF pMixBuf, const char *pszName, PPDMAUDIOPCMPROPS pProps, uint32_t cSamples)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pProps,  VERR_INVALID_POINTER);

    pMixBuf->pParent = NULL;

    RTListInit(&pMixBuf->lstChildren);
    pMixBuf->cChildren = 0;

    pMixBuf->pSamples = NULL;
    pMixBuf->cSamples = 0;

    pMixBuf->offRead  = 0;
    pMixBuf->offWrite = 0;
    pMixBuf->cMixed   = 0;
    pMixBuf->cUsed    = 0;

    /* Set initial volume to max. */
    pMixBuf->Volume.fMuted = false;
    pMixBuf->Volume.uLeft  = AUDIOMIXBUF_VOL_0DB;
    pMixBuf->Volume.uRight = AUDIOMIXBUF_VOL_0DB;

    /* Prevent division by zero.
     * Do a 1:1 conversion according to AUDIOMIXBUF_S2B_RATIO. */
    pMixBuf->iFreqRatio = 1 << 20;

    pMixBuf->pRate = NULL;

    pMixBuf->AudioFmt = AUDMIXBUF_AUDIO_FMT_MAKE(pProps->uHz,
                                                 pProps->cChannels,
                                                 pProps->cBits,
                                                 pProps->fSigned);

    pMixBuf->pfnConvFrom = audioMixBufConvFromLookup(pMixBuf->AudioFmt);
    pMixBuf->pfnConvTo   = audioMixBufConvToLookup(pMixBuf->AudioFmt);

    pMixBuf->cShift = pProps->cShift;
    pMixBuf->pszName = RTStrDup(pszName);
    if (!pMixBuf->pszName)
        return VERR_NO_MEMORY;

    AUDMIXBUF_LOG(("%s: uHz=%RU32, cChan=%RU8, cBits=%RU8, fSigned=%RTbool\n",
                   pMixBuf->pszName,
                   AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBuf->AudioFmt),
                   AUDMIXBUF_FMT_CHANNELS(pMixBuf->AudioFmt),
                   AUDMIXBUF_FMT_BITS_PER_SAMPLE(pMixBuf->AudioFmt),
                   RT_BOOL(AUDMIXBUF_FMT_SIGNED(pMixBuf->AudioFmt))));

    return audioMixBufAlloc(pMixBuf, cSamples);
}

/**
 * Returns @c true if there are any audio samples available for processing,
 * @c false if not.
 *
 * @return  bool                    @c true if there are any audio samples available for processing, @c false if not.
 * @param   pMixBuf                 Mixing buffer to return value for.
 */
bool AudioMixBufIsEmpty(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, true);

    if (pMixBuf->pParent)
        return (pMixBuf->cMixed == 0);
    return (pMixBuf->cUsed == 0);
}

/**
 * Calculates the frequency (sample rate) ratio of mixing buffer A in relation to mixing buffer B.
 *
 * @returns Calculated frequency ratio.
 * @param   pMixBufA            First mixing buffer.
 * @param   pMixBufB            Second mixing buffer.
 */
static int64_t audioMixBufCalcFreqRatio(PPDMAUDIOMIXBUF pMixBufA, PPDMAUDIOMIXBUF pMixBufB)
{
    int64_t iRatio = ((int64_t)AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBufA->AudioFmt) << 32)
                   /           AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBufB->AudioFmt);

    if (iRatio == 0)      /* Catch division by zero. */
        iRatio = 1 << 20; /* Do a 1:1 conversion instead. */

    return iRatio;
}

/**
 * Links an audio mixing buffer to a parent mixing buffer. A parent mixing
 * buffer can have multiple children mixing buffers [1:N], whereas a child only can
 * have one parent mixing buffer [N:1].
 *
 * The mixing direction always goes from the child/children buffer(s) to the
 * parent buffer.
 *
 * For guest audio output the host backend owns the parent mixing buffer, the
 * device emulation owns the child/children.
 *
 * The audio format of each mixing buffer can vary; the internal mixing code
 * then will automatically do the (needed) conversion.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to link parent to.
 * @param   pParent                 Parent mixing buffer to use for linking.
 *
 * @remark  Circular linking is not allowed.
 */
int AudioMixBufLinkTo(PPDMAUDIOMIXBUF pMixBuf, PPDMAUDIOMIXBUF pParent)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pParent, VERR_INVALID_POINTER);

    AssertMsgReturn(AUDMIXBUF_FMT_SAMPLE_FREQ(pParent->AudioFmt),
                    ("Parent sample frequency (Hz) not set\n"), VERR_INVALID_PARAMETER);
    AssertMsgReturn(AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBuf->AudioFmt),
                    ("Buffer sample frequency (Hz) not set\n"), VERR_INVALID_PARAMETER);
    AssertMsgReturn(pMixBuf != pParent,
                    ("Circular linking not allowed\n"), VERR_INVALID_PARAMETER);

    if (pMixBuf->pParent) /* Already linked? */
    {
        AUDMIXBUF_LOG(("%s: Already linked to parent '%s'\n",
                       pMixBuf->pszName, pMixBuf->pParent->pszName));
        return VERR_ACCESS_DENIED;
    }

    RTListAppend(&pParent->lstChildren, &pMixBuf->Node);
    pParent->cChildren++;

    /* Set the parent. */
    pMixBuf->pParent = pParent;

    /* Calculate the frequency ratios. */
    pMixBuf->iFreqRatio = audioMixBufCalcFreqRatio(pParent, pMixBuf);

    int rc = VINF_SUCCESS;
#if 0
    uint32_t cSamples = (uint32_t)RT_MIN(  ((uint64_t)pParent->cSamples << 32)
                                         / pMixBuf->iFreqRatio, _64K /* 64K samples max. */);
    if (!cSamples)
        cSamples = pParent->cSamples;

    int rc = VINF_SUCCESS;

    if (cSamples != pMixBuf->cSamples)
    {
        AUDMIXBUF_LOG(("%s: Reallocating samples %RU32 -> %RU32\n",
                       pMixBuf->pszName, pMixBuf->cSamples, cSamples));

        uint32_t cbSamples = cSamples * sizeof(PDMAUDIOSAMPLE);
        Assert(cbSamples);
        pMixBuf->pSamples = (PPDMAUDIOSAMPLE)RTMemRealloc(pMixBuf->pSamples, cbSamples);
        if (!pMixBuf->pSamples)
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            pMixBuf->cSamples = cSamples;

            /* Make sure to zero the reallocated buffer so that it can be
             * used properly when blending with another buffer later. */
            RT_BZERO(pMixBuf->pSamples, cbSamples);
        }
    }
#endif

    if (RT_SUCCESS(rc))
    {
        if (!pMixBuf->pRate)
        {
            /* Create rate conversion. */
            pMixBuf->pRate = (PPDMAUDIOSTRMRATE)RTMemAllocZ(sizeof(PDMAUDIOSTRMRATE));
            if (!pMixBuf->pRate)
                return VERR_NO_MEMORY;
        }
        else
            RT_BZERO(pMixBuf->pRate, sizeof(PDMAUDIOSTRMRATE));

        pMixBuf->pRate->dstInc = ((uint64_t)AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBuf->AudioFmt) << 32)
                               /            AUDMIXBUF_FMT_SAMPLE_FREQ(pParent->AudioFmt);

        AUDMIXBUF_LOG(("uThisHz=%RU32, uParentHz=%RU32, iFreqRatio=0x%RX64 (%RI64), uRateInc=0x%RX64 (%RU64), cSamples=%RU32 (%RU32 parent)\n",
                       AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBuf->AudioFmt),
                       AUDMIXBUF_FMT_SAMPLE_FREQ(pParent->AudioFmt),
                       pMixBuf->iFreqRatio, pMixBuf->iFreqRatio,
                       pMixBuf->pRate->dstInc, pMixBuf->pRate->dstInc,
                       pMixBuf->cSamples,
                       pParent->cSamples));
        AUDMIXBUF_LOG(("%s (%RU32Hz) -> %s (%RU32Hz)\n",
                       pMixBuf->pszName, AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBuf->AudioFmt),
                       pMixBuf->pParent->pszName, AUDMIXBUF_FMT_SAMPLE_FREQ(pParent->AudioFmt)));
    }

    return rc;
}

/**
 * Returns number of available live samples, that is, samples that
 * have been written into the mixing buffer but not have been processed yet.
 *
 * For a parent buffer, this simply returns the currently used number of samples
 * in the buffer.
 *
 * For a child buffer, this returns the number of samples which have been mixed
 * to the parent and were not processed by the parent yet.
 *
 * @return  uint32_t                Number of live samples available.
 * @param   pMixBuf                 Mixing buffer to return value for.
 */
uint32_t AudioMixBufLive(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

#ifdef RT_STRICT
    uint32_t cSamples;
#endif
    uint32_t cAvail;
    if (pMixBuf->pParent) /* Is this a child buffer? */
    {
#ifdef RT_STRICT
        /* Use the sample count from the parent, as
         * pMixBuf->cMixed specifies the sample count
         * in parent samples. */
        cSamples = pMixBuf->pParent->cSamples;
#endif
        cAvail   = pMixBuf->cMixed;
    }
    else
    {
#ifdef RT_STRICT
        cSamples = pMixBuf->cSamples;
#endif
        cAvail   = pMixBuf->cUsed;
    }

    Assert(cAvail <= cSamples);
    return cAvail;
}

/**
 * Mixes audio samples from a source mixing buffer to a destination mixing buffer.
 *
 * @return  IPRT status code.
 *          VERR_BUFFER_UNDERFLOW if the source did not have enough audio data.
 *          VERR_BUFFER_OVERFLOW if the destination did not have enough space to store the converted source audio data.
 *
 * @param   pDst                    Destination mixing buffer.
 * @param   pSrc                    Source mixing buffer.
 * @param   cSrcOff                 Offset of source audio samples to mix.
 * @param   cSrcSamples             Number of source audio samples to mix.
 * @param   pcSrcMixed              Number of source audio samples successfully mixed. Optional.
 */
static int audioMixBufMixTo(PPDMAUDIOMIXBUF pDst, PPDMAUDIOMIXBUF pSrc, uint32_t cSrcOff, uint32_t cSrcSamples,
                            uint32_t *pcSrcMixed)
{
    AssertPtrReturn(pDst,  VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc,  VERR_INVALID_POINTER);
    /* pcSrcMixed is optional. */

    AssertMsgReturn(pDst == pSrc->pParent, ("Source buffer '%s' is not a child of destination '%s'\n",
                                            pSrc->pszName, pDst->pszName), VERR_INVALID_PARAMETER);
    uint32_t cReadTotal    = 0;
    uint32_t cWrittenTotal = 0;

    Assert(pSrc->cMixed <= pDst->cSamples);

    Assert(pSrc->cUsed >= pDst->cMixed);
    Assert(pDst->cUsed <= pDst->cSamples);

    uint32_t offSrcRead  = cSrcOff;

    uint32_t offDstWrite = pDst->offWrite;
    uint32_t cDstMixed   = pSrc->cMixed;

    uint32_t cSrcAvail   = RT_MIN(cSrcSamples, pSrc->cUsed);
    uint32_t cDstAvail   = pDst->cSamples - pDst->cUsed; /** @todo Use pDst->cMixed later? */

    AUDMIXBUF_LOG(("%s (%RU32 available) -> %s (%RU32 available)\n",
                   pSrc->pszName, cSrcAvail, pDst->pszName, cDstAvail));
#ifdef DEBUG
    audioMixBufDbgPrintInternal(pDst, __FUNCTION__);
#endif

    if (!cSrcAvail)
        return VERR_BUFFER_UNDERFLOW;

    if (!cDstAvail)
        return VERR_BUFFER_OVERFLOW;

    uint32_t cSrcToRead = 0;
    uint32_t cSrcRead;

    uint32_t cDstToWrite;
    uint32_t cDstWritten;

    int rc = VINF_SUCCESS;

    while (cSrcAvail && cDstAvail)
    {
        cSrcToRead  = RT_MIN(cSrcAvail, pSrc->cSamples - offSrcRead);
        cDstToWrite = RT_MIN(cDstAvail, pDst->cSamples - offDstWrite);

        AUDMIXBUF_LOG(("\tSource: %RU32 @ %RU32 -> reading %RU32\n", cSrcAvail, offSrcRead, cSrcToRead));
        AUDMIXBUF_LOG(("\tDest  : %RU32 @ %RU32 -> writing %RU32\n", cDstAvail, offDstWrite, cDstToWrite));

        if (   !cDstToWrite
            || !cSrcToRead)
        {
            break;
        }

        cDstWritten = cSrcRead = 0;

        Assert(offSrcRead < pSrc->cSamples);
        Assert(offSrcRead + cSrcToRead <= pSrc->cSamples);

        Assert(offDstWrite < pDst->cSamples);
        Assert(offDstWrite + cDstToWrite <= pDst->cSamples);

        audioMixBufOpAssign(pDst->pSamples + offDstWrite, cDstToWrite,
                            pSrc->pSamples + offSrcRead,  cSrcToRead,
                            pSrc->pRate, &cDstWritten, &cSrcRead);

        cReadTotal    += cSrcRead;
        cWrittenTotal += cDstWritten;

        offSrcRead     = (offSrcRead  + cSrcRead)    % pSrc->cSamples;
        offDstWrite    = (offDstWrite + cDstWritten) % pDst->cSamples;

        cDstMixed     += cDstWritten;

        Assert(cSrcAvail >= cSrcRead);
        cSrcAvail        -= cSrcRead;

        Assert(cDstAvail >= cDstWritten);
        cDstAvail        -= cDstWritten;

        AUDMIXBUF_LOG(("\t%RU32 read (%RU32 left @ %RU32), %RU32 written (%RU32 left @ %RU32)\n",
                       cSrcRead, cSrcAvail, offSrcRead,
                       cDstWritten, cDstAvail, offDstWrite));
    }

    pSrc->offRead     = offSrcRead;
    Assert(pSrc->cUsed >= cReadTotal);
    pSrc->cUsed      -= RT_MIN(pSrc->cUsed, cReadTotal);

    /* Note: Always count in parent samples, as the rate can differ! */
    pSrc->cMixed      = RT_MIN(cDstMixed, pDst->cSamples);

    pDst->offWrite    = offDstWrite;
    Assert(pDst->offWrite <= pDst->cSamples);
    Assert((pDst->cUsed + cWrittenTotal) <= pDst->cSamples);
    pDst->cUsed      += cWrittenTotal;

    /* If there are more used samples than fitting in the destination buffer,
     * adjust the values accordingly.
     *
     * This can happen if this routine has been called too often without
     * actually processing the destination buffer in between. */
    if (pDst->cUsed > pDst->cSamples)
    {
        LogFunc(("%s: Warning: Destination buffer used %RU32 / %RU32 samples\n", pDst->pszName, pDst->cUsed, pDst->cSamples));
        pDst->offWrite     = 0;
        pDst->cUsed        = pDst->cSamples;

        rc = VERR_BUFFER_OVERFLOW;
    }

#ifdef DEBUG
    audioMixBufDbgValidate(pSrc);
    audioMixBufDbgValidate(pDst);

    Assert(pSrc->cMixed <= pDst->cSamples);
#endif

#ifdef AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
    uint32_t offRead = pDst->offRead;

    uint32_t cLeft = cWrittenTotal;
    while (cLeft)
    {
        uint8_t auBuf[256];
        RT_ZERO(auBuf);

        Assert(sizeof(auBuf) >= 4);
        Assert(sizeof(auBuf) % 4 == 0);

        uint32_t cToRead = RT_MIN(AUDIOMIXBUF_B2S(pDst, sizeof(auBuf)), RT_MIN(cLeft, pDst->cSamples - offRead));
        Assert(cToRead <= pDst->cUsed);

        PDMAUDMIXBUFCONVOPTS convOpts;
        RT_ZERO(convOpts);
        convOpts.cSamples = cToRead;

        pDst->pfnConvTo(auBuf, pDst->pSamples + offRead, &convOpts);

        RTFILE fh;
        int rc2 = RTFileOpen(&fh, AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "mixbuf_mixto.pcm",
                             RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc2))
        {
            RTFileWrite(fh, auBuf, AUDIOMIXBUF_S2B(pDst, cToRead), NULL);
            RTFileClose(fh);
        }

        offRead  = (offRead + cToRead) % pDst->cSamples;
        cLeft   -= cToRead;
    }
#endif /* AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA */

#ifdef DEBUG
    audioMixBufDbgPrintInternal(pDst, __FUNCTION__);
#endif

    if (pcSrcMixed)
        *pcSrcMixed = cReadTotal;

    AUDMIXBUF_LOG(("cReadTotal=%RU32, cWrittenTotal=%RU32, cSrcMixed=%RU32, cDstUsed=%RU32, rc=%Rrc\n",
                   cReadTotal, cWrittenTotal, pSrc->cMixed, pDst->cUsed, rc));
    return rc;
}

/**
 * Mixes audio samples down to the parent mixing buffer, extended version.
 *
 * @return  IPRT status code. See audioMixBufMixTo() for a more detailed explanation.
 * @param   pMixBuf                 Source mixing buffer to mix to its parent.
 * @param   cSrcOffset              Offset (in samples) of source mixing buffer.
 * @param   cSrcSamples             Number of source audio samples to mix to its parent.
 * @param   pcSrcMixed              Number of source audio samples successfully mixed. Optional.
 */
int AudioMixBufMixToParentEx(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSrcOffset, uint32_t cSrcSamples, uint32_t *pcSrcMixed)
{
    AssertMsgReturn(VALID_PTR(pMixBuf->pParent),
                    ("Buffer is not linked to a parent buffer\n"),
                    VERR_INVALID_PARAMETER);

    return audioMixBufMixTo(pMixBuf->pParent, pMixBuf, cSrcOffset, cSrcSamples, pcSrcMixed);
}

/**
 * Mixes audio samples down to the parent mixing buffer.
 *
 * @return  IPRT status code. See audioMixBufMixTo() for a more detailed explanation.
 * @param   pMixBuf                 Source mixing buffer to mix to its parent.
 * @param   cSrcSamples             Number of source audio samples to mix to its parent.
 * @param   pcSrcMixed              Number of source audio samples successfully mixed. Optional.
 */
int AudioMixBufMixToParent(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSrcSamples, uint32_t *pcSrcMixed)
{
    return audioMixBufMixTo(pMixBuf->pParent, pMixBuf, pMixBuf->offRead, cSrcSamples, pcSrcMixed);
}

#ifdef DEBUG
/**
 * Prints a single mixing buffer.
 * Internal helper function for debugging. Do not use directly.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to print.
 * @param   pszFunc                 Function name to log this for.
 * @param   fIsParent               Whether this is a parent buffer or not.
 * @param   uIdtLvl                 Indention level to use.
 */
DECL_FORCE_INLINE(void) audioMixBufDbgPrintSingle(PPDMAUDIOMIXBUF pMixBuf, const char *pszFunc, bool fIsParent, uint16_t uIdtLvl)
{
    Log(("%s: %*s[%s] %s: offRead=%RU32, offWrite=%RU32, cMixed=%RU32 -> %RU32/%RU32\n",
         pszFunc, uIdtLvl * 4, "", fIsParent ? "PARENT" : "CHILD",
         pMixBuf->pszName, pMixBuf->offRead, pMixBuf->offWrite, pMixBuf->cMixed, pMixBuf->cUsed, pMixBuf->cSamples));
}

/**
 * Validates a single mixing buffer.
 *
 * @return  @true if the buffer state is valid or @false if not.
 * @param   pMixBuf                 Mixing buffer to validate.
 */
DECL_FORCE_INLINE(bool) audioMixBufDbgValidate(PPDMAUDIOMIXBUF pMixBuf)
{
    //const uint32_t offReadEnd  = (pMixBuf->offRead + pMixBuf->cUsed) % pMixBuf->cSamples;
    //const uint32_t offWriteEnd = (pMixBuf->offWrite + (pMixBuf->cSamples - pMixBuf->cUsed)) % pMixBuf->cSamples;

    bool fValid = true;

    AssertStmt(pMixBuf->offRead  <= pMixBuf->cSamples, fValid = false);
    AssertStmt(pMixBuf->offWrite <= pMixBuf->cSamples, fValid = false);
    AssertStmt(pMixBuf->cUsed    <= pMixBuf->cSamples, fValid = false);

    if (pMixBuf->offWrite > pMixBuf->offRead)
    {
        if (pMixBuf->offWrite - pMixBuf->offRead != pMixBuf->cUsed)
            fValid = false;
    }
    else if (pMixBuf->offWrite < pMixBuf->offRead)
    {
        if (pMixBuf->offWrite + pMixBuf->cSamples - pMixBuf->offRead != pMixBuf->cUsed)
            fValid = false;
    }

    if (!fValid)
    {
        audioMixBufDbgPrintInternal(pMixBuf, __FUNCTION__);
        AssertFailed();
    }

    return fValid;
}

/**
 * Internal helper function for audioMixBufPrintChain().
 * Do not use directly.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to print.
 * @param   pszFunc                 Function name to print the chain for.
 * @param   uIdtLvl                 Indention level to use.
 * @param   pcChildren              Pointer to children counter.
 */
DECL_FORCE_INLINE(void) audioMixBufDbgPrintChainHelper(PPDMAUDIOMIXBUF pMixBuf, const char *pszFunc, uint16_t uIdtLvl,
                                                       size_t *pcChildren)
{
    PPDMAUDIOMIXBUF pIter;
    RTListForEach(&pMixBuf->lstChildren, pIter, PDMAUDIOMIXBUF, Node)
    {
        audioMixBufDbgPrintSingle(pIter, pszFunc, false /* ifIsParent */, uIdtLvl + 1);
        *pcChildren++;
    }
}

DECL_FORCE_INLINE(void) audioMixBufDbgPrintChainInternal(PPDMAUDIOMIXBUF pMixBuf, const char *pszFunc)
{
    PPDMAUDIOMIXBUF pParent = pMixBuf->pParent;
    while (pParent)
    {
        if (!pParent->pParent)
            break;

        pParent = pParent->pParent;
    }

    if (!pParent)
        pParent = pMixBuf;

    audioMixBufDbgPrintSingle(pParent, pszFunc, true /* fIsParent */, 0 /* uIdtLvl */);

    /* Recursively iterate children. */
    size_t cChildren = 0;
    audioMixBufDbgPrintChainHelper(pParent, pszFunc, 0 /* uIdtLvl */, &cChildren);

    Log(("%s: Children: %zu\n", pszFunc, cChildren));
}

/**
 * Prints statistics and status of the full chain of a mixing buffer to the logger,
 * starting from the top root mixing buffer.
 * For debug versions only.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to print.
 */
void AudioMixBufDbgPrintChain(PPDMAUDIOMIXBUF pMixBuf)
{
    audioMixBufDbgPrintChainInternal(pMixBuf, __FUNCTION__);
}

DECL_FORCE_INLINE(void) audioMixBufDbgPrintInternal(PPDMAUDIOMIXBUF pMixBuf, const char *pszFunc)
{
    PPDMAUDIOMIXBUF pParent = pMixBuf;
    if (pMixBuf->pParent)
        pParent = pMixBuf->pParent;

    audioMixBufDbgPrintSingle(pMixBuf, pszFunc, pParent == pMixBuf /* fIsParent */, 0 /* iIdtLevel */);

    PPDMAUDIOMIXBUF pIter;
    RTListForEach(&pMixBuf->lstChildren, pIter, PDMAUDIOMIXBUF, Node)
    {
        if (pIter == pMixBuf)
            continue;
        audioMixBufDbgPrintSingle(pIter, pszFunc, false /* fIsParent */, 1 /* iIdtLevel */);
    }
}

/**
 * Prints statistics and status of a mixing buffer to the logger.
 * For debug versions only.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to print.
 */
void AudioMixBufDbgPrint(PPDMAUDIOMIXBUF pMixBuf)
{
    audioMixBufDbgPrintInternal(pMixBuf, __FUNCTION__);
}
#endif /* DEBUG */

/**
 * Returns the total number of samples used.
 *
 * @return  uint32_t
 * @param   pMixBuf
 */
uint32_t AudioMixBufUsed(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return pMixBuf->cUsed;
}

/**
 * Reads audio samples at a specific offset.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to read audio samples from.
 * @param   offSamples              Offset (in audio samples) to start reading from.
 * @param   pvBuf                   Pointer to buffer to write output to.
 * @param   cbBuf                   Size (in bytes) of buffer to write to.
 * @param   pcbRead                 Size (in bytes) of data read. Optional.
 */
int AudioMixBufReadAt(PPDMAUDIOMIXBUF pMixBuf,
                      uint32_t offSamples,
                      void *pvBuf, uint32_t cbBuf,
                      uint32_t *pcbRead)
{
    return AudioMixBufReadAtEx(pMixBuf, pMixBuf->AudioFmt,
                               offSamples, pvBuf, cbBuf, pcbRead);
}

/**
 * Reads audio samples at a specific offset.
 * If the audio format of the mixing buffer and the requested audio format do
 * not match the output will be converted accordingly.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to read audio samples from.
 * @param   enmFmt                  Audio format to use for output.
 * @param   offSamples              Offset (in audio samples) to start reading from.
 * @param   pvBuf                   Pointer to buffer to write output to.
 * @param   cbBuf                   Size (in bytes) of buffer to write to.
 * @param   pcbRead                 Size (in bytes) of data read. Optional.
 */
int AudioMixBufReadAtEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                        uint32_t offSamples,
                        void *pvBuf, uint32_t cbBuf,
                        uint32_t *pcbRead)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    uint32_t cDstSamples = pMixBuf->cSamples;
    uint32_t cLive = pMixBuf->cUsed;

    uint32_t cDead = cDstSamples - cLive;
    uint32_t cToProcess = (uint32_t)AUDIOMIXBUF_S2S_RATIO(pMixBuf, cDead);
    cToProcess = RT_MIN(cToProcess, AUDIOMIXBUF_B2S(pMixBuf, cbBuf));

    AUDMIXBUF_LOG(("%s: offSamples=%RU32, cLive=%RU32, cDead=%RU32, cToProcess=%RU32\n",
                   pMixBuf->pszName, offSamples, cLive, cDead, cToProcess));

    int rc;
    if (cToProcess)
    {
        PFNPDMAUDIOMIXBUFCONVTO pfnConvTo = NULL;
        if (pMixBuf->AudioFmt != enmFmt)
            pfnConvTo = audioMixBufConvToLookup(enmFmt);
        else
            pfnConvTo = pMixBuf->pfnConvTo;

        if (pfnConvTo)
        {
            PDMAUDMIXBUFCONVOPTS convOpts;
            RT_ZERO(convOpts);
            /* Note: No volume handling/conversion done in the conversion-to macros (yet). */

            convOpts.cSamples = cToProcess;

            pfnConvTo(pvBuf, pMixBuf->pSamples + offSamples, &convOpts);

#ifdef DEBUG
            AudioMixBufDbgPrint(pMixBuf);
#endif
            rc = VINF_SUCCESS;
        }
        else
        {
            AssertFailed();
            rc = VERR_NOT_SUPPORTED;
        }
    }
    else
        rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = AUDIOMIXBUF_S2B(pMixBuf, cToProcess);
    }

    AUDMIXBUF_LOG(("cbRead=%RU32, rc=%Rrc\n", AUDIOMIXBUF_S2B(pMixBuf, cToProcess), rc));
    return rc;
}

/**
 * Reads audio samples. The audio format of the mixing buffer will be used.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to read audio samples from.
 * @param   pvBuf                   Pointer to buffer to write output to.
 * @param   cbBuf                   Size (in bytes) of buffer to write to.
 * @param   pcRead                  Number of audio samples read. Optional.
 */
int AudioMixBufReadCirc(PPDMAUDIOMIXBUF pMixBuf, void *pvBuf, uint32_t cbBuf, uint32_t *pcRead)
{
    return AudioMixBufReadCircEx(pMixBuf, pMixBuf->AudioFmt, pvBuf, cbBuf, pcRead);
}

/**
 * Reads audio samples in a specific audio format.
 * If the audio format of the mixing buffer and the requested audio format do
 * not match the output will be converted accordingly.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to read audio samples from.
 * @param   enmFmt                  Audio format to use for output.
 * @param   pvBuf                   Pointer to buffer to write output to.
 * @param   cbBuf                   Size (in bytes) of buffer to write to.
 * @param   pcRead                  Number of audio samples read. Optional.
 */
int AudioMixBufReadCircEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt, void *pvBuf, uint32_t cbBuf, uint32_t *pcRead)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvBuf,   VERR_INVALID_POINTER);
    /* pcRead is optional. */

    /* Make sure that we at least have space for a full audio sample. */
    AssertReturn(AUDIOMIXBUF_B2S(pMixBuf, cbBuf), VERR_INVALID_PARAMETER);

    uint32_t cToRead = RT_MIN(pMixBuf->cUsed, AUDIOMIXBUF_B2S(pMixBuf, cbBuf));

    AUDMIXBUF_LOG(("%s: cbBuf=%RU32 (%RU32 samples), cToRead=%RU32, fmtSrc=0x%x, fmtDst=0x%x\n",
                   pMixBuf->pszName, cbBuf, AUDIOMIXBUF_B2S(pMixBuf, cbBuf), cToRead, pMixBuf->AudioFmt, enmFmt));

    if (!cToRead)
    {
#ifdef DEBUG
        audioMixBufDbgPrintInternal(pMixBuf, __FUNCTION__);
#endif
        if (pcRead)
            *pcRead = 0;
        return VINF_SUCCESS;
    }

    PFNPDMAUDIOMIXBUFCONVTO pfnConvTo = NULL;
    if (pMixBuf->AudioFmt != enmFmt)
        pfnConvTo = audioMixBufConvToLookup(enmFmt);
    else
        pfnConvTo = pMixBuf->pfnConvTo;

    if (!pfnConvTo) /* Audio format not supported. */
    {
        AssertFailed();
        return VERR_NOT_SUPPORTED;
    }

    cToRead = RT_MIN(cToRead, pMixBuf->cSamples - pMixBuf->offRead);
    if (cToRead)
    {
        PDMAUDMIXBUFCONVOPTS convOpts;
        RT_ZERO(convOpts);
        convOpts.cSamples = cToRead;

        AUDMIXBUF_LOG(("cToRead=%RU32\n", cToRead));

        pfnConvTo(pvBuf, pMixBuf->pSamples + pMixBuf->offRead, &convOpts);

#ifdef AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        int rc2 = RTFileOpen(&fh, AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "mixbuf_readcirc.pcm",
                             RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc2))
        {
            RTFileWrite(fh, pvBuf, AUDIOMIXBUF_S2B(pMixBuf, cToRead), NULL);
            RTFileClose(fh);
        }
#endif
        pMixBuf->offRead  = (pMixBuf->offRead + cToRead) % pMixBuf->cSamples;
        Assert(pMixBuf->cUsed >= cToRead);
        pMixBuf->cUsed   -= cToRead;
    }

    if (pcRead)
        *pcRead = cToRead;

#ifdef DEBUG
    audioMixBufDbgValidate(pMixBuf);
#endif

    AUDMIXBUF_LOG(("cRead=%RU32 (%RU32 bytes)\n", cToRead, AUDIOMIXBUF_S2B(pMixBuf, cToRead)));
    return VINF_SUCCESS;
}

/**
 * Returns the current read position of a mixing buffer.
 *
 * @returns IPRT status code.
 * @param   pMixBuf                 Mixing buffer to return position for.
 */
uint32_t AudioMixBufReadPos(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    return pMixBuf->offRead;
}

/**
 * Resets a mixing buffer.
 *
 * @param   pMixBuf                 Mixing buffer to reset.
 */
void AudioMixBufReset(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturnVoid(pMixBuf);

    AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

    pMixBuf->offRead  = 0;
    pMixBuf->offWrite = 0;
    pMixBuf->cMixed   = 0;
    pMixBuf->cUsed    = 0;

    AudioMixBufClear(pMixBuf);
}

/**
 * Sets the overall (master) volume.
 *
 * @param   pMixBuf                 Mixing buffer to set volume for.
 * @param   pVol                    Pointer to volume structure to set.
 */
void AudioMixBufSetVolume(PPDMAUDIOMIXBUF pMixBuf, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturnVoid(pMixBuf);
    AssertPtrReturnVoid(pVol);

    LogFlowFunc(("%s: lVol=%RU8, rVol=%RU8, fMuted=%RTbool\n", pMixBuf->pszName, pVol->uLeft, pVol->uRight, pVol->fMuted));

    int rc2 = audioMixBufConvVol(&pMixBuf->Volume /* Dest */, pVol /* Source */);
    AssertRC(rc2);
}

/**
 * Returns the maximum amount of audio samples this buffer can hold.
 *
 * @return  uint32_t                Size (in audio samples) the mixing buffer can hold.
 * @param   pMixBuf                 Mixing buffer to retrieve maximum for.
 */
uint32_t AudioMixBufSize(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return pMixBuf->cSamples;
}

/**
 * Returns the maximum amount of bytes this buffer can hold.
 *
 * @return  uint32_t                Size (in bytes) the mixing buffer can hold.
 * @param   pMixBuf                 Mixing buffer to retrieve maximum for.
 */
uint32_t AudioMixBufSizeBytes(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return AUDIOMIXBUF_S2B(pMixBuf, pMixBuf->cSamples);
}

/**
 * Unlinks a mixing buffer from its parent, if any.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Mixing buffer to unlink from parent.
 */
void AudioMixBufUnlink(PPDMAUDIOMIXBUF pMixBuf)
{
    if (!pMixBuf || !pMixBuf->pszName)
        return;

    AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

    if (pMixBuf->pParent) /* IS this a children buffer? */
    {
        AUDMIXBUF_LOG(("%s: Unlinking from parent \"%s\"\n",
                       pMixBuf->pszName, pMixBuf->pParent->pszName));

        RTListNodeRemove(&pMixBuf->Node);

        /* Decrease the paren't children count. */
        Assert(pMixBuf->pParent->cChildren);
        pMixBuf->pParent->cChildren--;

        /* Make sure to reset the parent mixing buffer each time it gets linked
         * to a new child. */
        AudioMixBufReset(pMixBuf->pParent);
        pMixBuf->pParent = NULL;
    }

    PPDMAUDIOMIXBUF pChild, pChildNext;
    RTListForEachSafe(&pMixBuf->lstChildren, pChild, pChildNext, PDMAUDIOMIXBUF, Node)
    {
        AUDMIXBUF_LOG(("\tUnlinking \"%s\"\n", pChild->pszName));

        AudioMixBufReset(pChild);

        Assert(pChild->pParent == pMixBuf);
        pChild->pParent = NULL;

        RTListNodeRemove(&pChild->Node);

        /* Decrease the children count. */
        Assert(pMixBuf->cChildren);
        pMixBuf->cChildren--;
    }

    Assert(RTListIsEmpty(&pMixBuf->lstChildren));
    Assert(pMixBuf->cChildren == 0);

    AudioMixBufReset(pMixBuf);

    if (pMixBuf->pRate)
    {
        pMixBuf->pRate->dstOffset = pMixBuf->pRate->srcOffset = 0;
        pMixBuf->pRate->dstInc = 0;
    }

    pMixBuf->iFreqRatio = 1; /* Prevent division by zero. */
}

/**
 * Writes audio samples at a specific offset.
 * The sample format being written must match the format of the mixing buffer.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Pointer to mixing buffer to write to.
 * @param   offSamples              Offset (in samples) starting to write at.
 * @param   pvBuf                   Pointer to audio buffer to be written.
 * @param   cbBuf                   Size (in bytes) of audio buffer.
 * @param   pcWritten               Returns number of audio samples written. Optional.
 */
int AudioMixBufWriteAt(PPDMAUDIOMIXBUF pMixBuf, uint32_t offSamples, const void *pvBuf, uint32_t cbBuf, uint32_t *pcWritten)
{
    return AudioMixBufWriteAtEx(pMixBuf, pMixBuf->AudioFmt, offSamples, pvBuf, cbBuf, pcWritten);
}

/**
 * Writes audio samples at a specific offset.
 *
 * Note that this operation also modifies the current read and write position
 * to \a offSamples + written samples on success.
 *
 * The audio sample format to be written can be different from the audio format
 * the mixing buffer operates on.
 *
 * @return  IPRT status code.
 * @param   pMixBuf                 Pointer to mixing buffer to write to.
 * @param   enmFmt                  Audio format supplied in the buffer.
 * @param   offSamples              Offset (in samples) starting to write at.
 * @param   pvBuf                   Pointer to audio buffer to be written.
 * @param   cbBuf                   Size (in bytes) of audio buffer.
 * @param   pcWritten               Returns number of audio samples written. Optional.
 */
int AudioMixBufWriteAtEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                         uint32_t offSamples, const void *pvBuf, uint32_t cbBuf,
                         uint32_t *pcWritten)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvBuf,   VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    if (offSamples >= pMixBuf->cSamples)
    {
        if (pcWritten)
            *pcWritten = 0;
        return VERR_BUFFER_OVERFLOW;
    }

    /*
     * Adjust cToWrite so we don't overflow our buffers.
     */
    uint32_t cToWrite = RT_MIN(AUDIOMIXBUF_B2S(pMixBuf, cbBuf), pMixBuf->cSamples - offSamples);

#ifdef AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
    /*
     * Now that we know how much we'll be converting we can log it.
     */
    RTFILE hFile;
    int rc2 = RTFileOpen(&hFile, AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "mixbuf_writeat.pcm",
                         RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc2))
    {
        RTFileWrite(hFile, pvBuf, AUDIOMIXBUF_S2B(pMixBuf, cToWrite), NULL);
        RTFileClose(hFile);
    }
#endif

    /*
     * Pick the conversion function and do the conversion.
     */
    PFNPDMAUDIOMIXBUFCONVFROM pfnConvFrom = NULL;
    if (!pMixBuf->Volume.fMuted)
    {
        if (pMixBuf->AudioFmt != enmFmt)
            pfnConvFrom = audioMixBufConvFromLookup(enmFmt);
        else
            pfnConvFrom = pMixBuf->pfnConvFrom;
    }
    else
        pfnConvFrom = &audioMixBufConvFromSilence;

    int rc = VINF_SUCCESS;

    uint32_t cWritten;
    if (   pfnConvFrom
        && cToWrite)
    {
        PDMAUDMIXBUFCONVOPTS convOpts;

        convOpts.cSamples           = cToWrite;
        convOpts.From.Volume.fMuted = pMixBuf->Volume.fMuted;
        convOpts.From.Volume.uLeft  = pMixBuf->Volume.uLeft;
        convOpts.From.Volume.uRight = pMixBuf->Volume.uRight;

        cWritten = pfnConvFrom(pMixBuf->pSamples + offSamples, pvBuf, AUDIOMIXBUF_S2B(pMixBuf, cToWrite), &convOpts);
    }
    else
    {
        cWritten = 0;
        if (!pfnConvFrom)
        {
            AssertFailed();
            rc = VERR_NOT_SUPPORTED;
        }
    }

    AUDMIXBUF_LOG(("%s: offSamples=%RU32, cbBuf=%RU32, cToWrite=%RU32 (%zu bytes), cWritten=%RU32 (%zu bytes), rc=%Rrc\n",
                   pMixBuf->pszName, offSamples, cbBuf,
                   cToWrite, AUDIOMIXBUF_S2B(pMixBuf, cToWrite),
                   cWritten, AUDIOMIXBUF_S2B(pMixBuf, cWritten), rc));

    if (RT_SUCCESS(rc))
    {
        pMixBuf->offRead  = offSamples % pMixBuf->cSamples;
        pMixBuf->offWrite = (offSamples + cWritten) % pMixBuf->cSamples;
        pMixBuf->cUsed    = cWritten;
        pMixBuf->cMixed   = 0;

#ifdef DEBUG
        audioMixBufDbgValidate(pMixBuf);
#endif
        if (pcWritten)
            *pcWritten = cWritten;
    }
    else
        AUDMIXBUF_LOG(("%s: Failed with %Rrc\n", pMixBuf->pszName, rc));

    return rc;
}

/**
 * Writes audio samples.
 *
 * The sample format being written must match the format of the mixing buffer.
 *
 * @return  IPRT status code, or VERR_BUFFER_OVERFLOW if samples which not have
 *          been processed yet have been overwritten (due to cyclic buffer).
 * @param   pMixBuf                 Pointer to mixing buffer to write to.
 * @param   pvBuf                   Pointer to audio buffer to be written.
 * @param   cbBuf                   Size (in bytes) of audio buffer.
 * @param   pcWritten               Returns number of audio samples written. Optional.
 */
int AudioMixBufWriteCirc(PPDMAUDIOMIXBUF pMixBuf,
                         const void *pvBuf, uint32_t cbBuf,
                         uint32_t *pcWritten)
{
    return AudioMixBufWriteCircEx(pMixBuf, pMixBuf->AudioFmt, pvBuf, cbBuf, pcWritten);
}

/**
 * Writes audio samples of a specific format.
 * This function might write less data at once than requested.
 *
 * @return  IPRT status code, or VERR_BUFFER_OVERFLOW no space is available for writing anymore.
 * @param   pMixBuf                 Pointer to mixing buffer to write to.
 * @param   enmFmt                  Audio format supplied in the buffer.
 * @param   pvBuf                   Pointer to audio buffer to be written.
 * @param   cbBuf                   Size (in bytes) of audio buffer.
 * @param   pcWritten               Returns number of audio samples written. Optional.
 */
int AudioMixBufWriteCircEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                           const void *pvBuf, uint32_t cbBuf, uint32_t *pcWritten)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,   VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    if (!cbBuf)
    {
        if (pcWritten)
            *pcWritten = 0;
        return VINF_SUCCESS;
    }

    /* Make sure that we at least write a full audio sample. */
    AssertReturn(AUDIOMIXBUF_B2S(pMixBuf, cbBuf), VERR_INVALID_PARAMETER);

    Assert(pMixBuf->cSamples);
    AssertPtr(pMixBuf->pSamples);

    PFNPDMAUDIOMIXBUFCONVFROM pfnConvFrom = NULL;
    if (!pMixBuf->Volume.fMuted)
    {
        if (pMixBuf->AudioFmt != enmFmt)
            pfnConvFrom = audioMixBufConvFromLookup(enmFmt);
        else
            pfnConvFrom = pMixBuf->pfnConvFrom;
    }
    else
        pfnConvFrom = &audioMixBufConvFromSilence;

    if (!pfnConvFrom)
    {
        AssertFailed();
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    uint32_t cWritten = 0;

    uint32_t cFree = pMixBuf->cSamples - pMixBuf->cUsed;
    if (cFree)
    {
        if ((pMixBuf->cSamples - pMixBuf->offWrite) == 0)
            pMixBuf->offWrite = 0;

        uint32_t cToWrite = RT_MIN(AUDIOMIXBUF_B2S(pMixBuf, cbBuf), RT_MIN(pMixBuf->cSamples - pMixBuf->offWrite, cFree));
        Assert(cToWrite);

        PDMAUDMIXBUFCONVOPTS convOpts;
        RT_ZERO(convOpts);

        convOpts.From.Volume.fMuted = pMixBuf->Volume.fMuted;
        convOpts.From.Volume.uLeft  = pMixBuf->Volume.uLeft;
        convOpts.From.Volume.uRight = pMixBuf->Volume.uRight;

        convOpts.cSamples = cToWrite;

        cWritten = pfnConvFrom(pMixBuf->pSamples + pMixBuf->offWrite,
                               pvBuf, AUDIOMIXBUF_S2B(pMixBuf, cToWrite), &convOpts);
        Assert(cWritten == cToWrite);

#ifdef AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        RTFileOpen(&fh, AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "mixbuf_writecirc_ex.pcm",
                   RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        RTFileWrite(fh, pvBuf, AUDIOMIXBUF_S2B(pMixBuf, cToWrite), NULL);
        RTFileClose(fh);
#endif
        pMixBuf->cUsed   += cWritten;
        Assert(pMixBuf->cUsed <= pMixBuf->cSamples);

        pMixBuf->offWrite = (pMixBuf->offWrite + cWritten) % pMixBuf->cSamples;
        Assert(pMixBuf->offWrite <= pMixBuf->cSamples);
    }
    else
        rc = VERR_BUFFER_OVERFLOW;

#ifdef DEBUG
    audioMixBufDbgPrintInternal(pMixBuf, __FUNCTION__);
    audioMixBufDbgValidate(pMixBuf);
#endif

    if (pcWritten)
        *pcWritten = cWritten;

    AUDMIXBUF_LOG(("%s: enmFmt=0x%x, cbBuf=%RU32 (%RU32 samples), cWritten=%RU32, rc=%Rrc\n",
                   pMixBuf->pszName, enmFmt, cbBuf, AUDIOMIXBUF_B2S(pMixBuf, cbBuf), cWritten, rc));
    return rc;
}

/**
 * Returns the current write position of a mixing buffer.
 *
 * @returns IPRT status code.
 * @param   pMixBuf                 Mixing buffer to return position for.
 */
uint32_t AudioMixBufWritePos(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    return pMixBuf->offWrite;
}

