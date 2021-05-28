/* $Id$ */
/** @file
 * Audio mixing buffer for converting reading/writing audio data.
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
#include <iprt/errcore.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "AudioMixBuffer.h"

#ifndef VBOX_AUDIO_TESTCASE
# ifdef DEBUG
#  define AUDMIXBUF_LOG(x) LogFlowFunc(x)
#  define AUDMIXBUF_LOG_ENABLED
# else
#  define AUDMIXBUF_LOG(x) do {} while (0)
# endif
#else /* VBOX_AUDIO_TESTCASE */
# define AUDMIXBUF_LOG(x) RTPrintf x
# define AUDMIXBUF_LOG_ENABLED
#endif

#ifdef DEBUG
static void audioMixBufDbgPrintInternal(PAUDIOMIXBUF pMixBuf, const char *pszFunc);
static bool audioMixBufDbgValidate(PAUDIOMIXBUF pMixBuf);
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
static uint32_t const s_aVolumeConv[256] = {
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
 * Clears the entire frame buffer.
 *
 * @param   pMixBuf                 Mixing buffer to clear.
 *
 */
void AudioMixBufClear(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturnVoid(pMixBuf);

    if (pMixBuf->cFrames)
        RT_BZERO(pMixBuf->pFrames, pMixBuf->cFrames * sizeof(PDMAUDIOFRAME));
}

/**
 * Clears (zeroes) the buffer by a certain amount of (used) frames and
 * keeps track to eventually assigned children buffers.
 *
 * @param   pMixBuf                 Mixing buffer to clear.
 * @param   cFramesToClear          Number of audio frames to clear.
 */
void AudioMixBufFinish(PAUDIOMIXBUF pMixBuf, uint32_t cFramesToClear)
{
    AUDMIXBUF_LOG(("cFramesToClear=%RU32\n", cFramesToClear));
    AUDMIXBUF_LOG(("%s: offRead=%RU32, cUsed=%RU32\n",
                   pMixBuf->pszName, pMixBuf->offRead, pMixBuf->cUsed));

    AssertStmt(cFramesToClear <= pMixBuf->cFrames, cFramesToClear = pMixBuf->cFrames);

/** @todo r=bird: waste of time? */
    uint32_t cClearOff;
    uint32_t cClearLen;

    /* Clear end of buffer (wrap around). */
    if (cFramesToClear > pMixBuf->offRead)
    {
        cClearOff = pMixBuf->cFrames - (cFramesToClear - pMixBuf->offRead);
        cClearLen = pMixBuf->cFrames - cClearOff;

        AUDMIXBUF_LOG(("Clearing1: %RU32 - %RU32\n", cClearOff, cClearOff + cClearLen));

        RT_BZERO(pMixBuf->pFrames + cClearOff, cClearLen * sizeof(PDMAUDIOFRAME));

        Assert(cFramesToClear >= cClearLen);
        cFramesToClear -= cClearLen;
    }

    /* Clear beginning of buffer. */
    if (   cFramesToClear
        && pMixBuf->offRead)
    {
        Assert(pMixBuf->offRead >= cFramesToClear);

        cClearOff = pMixBuf->offRead - cFramesToClear;
        cClearLen = cFramesToClear;

        Assert(cClearOff + cClearLen <= pMixBuf->cFrames);

        AUDMIXBUF_LOG(("Clearing2: %RU32 - %RU32\n", cClearOff, cClearOff + cClearLen));

        RT_BZERO(pMixBuf->pFrames + cClearOff, cClearLen * sizeof(PDMAUDIOFRAME));
    }
}

/**
 * Destroys (uninitializes) a mixing buffer.
 *
 * @param   pMixBuf                 Mixing buffer to destroy.
 */
void AudioMixBufDestroy(PAUDIOMIXBUF pMixBuf)
{
    if (!pMixBuf)
        return;

    /* Ignore calls for an uninitialized (zeroed) or already destroyed instance.  Happens a lot. */
    if (   pMixBuf->uMagic == 0
        || pMixBuf->uMagic == ~AUDIOMIXBUF_MAGIC)
    {
        Assert(!pMixBuf->pszName);
        Assert(!pMixBuf->pFrames);
        Assert(!pMixBuf->cFrames);
        return;
    }

    Assert(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);
    pMixBuf->uMagic = ~AUDIOMIXBUF_MAGIC;

    if (pMixBuf->pszName)
    {
        AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

        RTStrFree(pMixBuf->pszName);
        pMixBuf->pszName = NULL;
    }

    if (pMixBuf->pFrames)
    {
        Assert(pMixBuf->cFrames);

        RTMemFree(pMixBuf->pFrames);
        pMixBuf->pFrames = NULL;
    }

    pMixBuf->cFrames = 0;
}

/**
 * Returns the size (in audio frames) of free audio buffer space.
 *
 * @return  uint32_t                Size (in audio frames) of free audio buffer space.
 * @param   pMixBuf                 Mixing buffer to return free size for.
 */
uint32_t AudioMixBufFree(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    uint32_t const cFrames = pMixBuf->cFrames;
    uint32_t       cUsed   = pMixBuf->cUsed;
    AssertStmt(cUsed <= cFrames, cUsed = cFrames);
    uint32_t const cFramesFree = cFrames - cUsed;

    AUDMIXBUF_LOG(("%s: %RU32 of %RU32\n", pMixBuf->pszName, cFramesFree, cFrames));
    return cFramesFree;
}

/**
 * Returns the size (in bytes) of free audio buffer space.
 *
 * @return  uint32_t                Size (in bytes) of free audio buffer space.
 * @param   pMixBuf                 Mixing buffer to return free size for.
 */
uint32_t AudioMixBufFreeBytes(PAUDIOMIXBUF pMixBuf)
{
    return AUDIOMIXBUF_F2B(pMixBuf, AudioMixBufFree(pMixBuf));
}


/**
 * Merges @a i64Src into the value stored at @a pi64Dst.
 *
 * @param   pi64Dst     The value to merge @a i64Src into.
 * @param   i64Src      The new value to add.
 */
DECL_FORCE_INLINE(void) audioMixBufBlendSample(int64_t *pi64Dst, int64_t i64Src)
{
    if (i64Src)
    {
        int64_t const i64Dst = *pi64Dst;
        if (!pi64Dst)
            *pi64Dst = i64Src;
        else
            *pi64Dst = (i64Dst + i64Src) / 2;
    }
}


/**
 * Variant of audioMixBufBlendSample that returns the result rather than storing it.
 *
 * This is used for stereo -> mono.
 */
DECL_FORCE_INLINE(int64_t) audioMixBufBlendSampleRet(int64_t i64Sample1, int64_t i64Sample2)
{
    if (!i64Sample1)
        return i64Sample2;
    if (!i64Sample2)
        return i64Sample1;
    return (i64Sample1 + i64Sample2) / 2;
}


/**
 * Blends (merges) the source buffer into the destination buffer.
 *
 * We're taking a very simple approach here, working sample by sample:
 *  - if one is silent, use the other one.
 *  - otherwise sum and divide by two.
 *
 * @param   pi64Dst     The destination stream buffer (input and output).
 * @param   pi64Src     The source stream buffer.
 * @param   cFrames     Number of frames to process.
 * @param   cChannels   Number of channels.
 */
static void audioMixBufBlendBuffer(int64_t *pi64Dst, int64_t const *pi64Src, uint32_t cFrames, uint8_t cChannels)
{
    switch (cChannels)
    {
        case 2:
            while (cFrames-- > 0)
            {
                int64_t const i64DstL = pi64Dst[0];
                int64_t const i64SrcL = pi64Src[0];
                if (!i64DstL)
                    pi64Dst[0] = i64SrcL;
                else if (!i64SrcL)
                    pi64Dst[0] = (i64DstL + i64SrcL) / 2;

                int64_t const i64DstR = pi64Dst[1];
                int64_t const i64SrcR = pi64Src[1];
                if (!i64DstR)
                    pi64Dst[1] = i64SrcR;
                else if (!i64SrcR)
                    pi64Dst[1] = (i64DstR + i64SrcR) / 2;

                pi64Dst += 2;
                pi64Src += 2;
            }
            break;

        default:
            cFrames *= cChannels;
            RT_FALL_THROUGH();
        case 1:
            while (cFrames-- > 0)
            {
                int64_t const i64Dst = *pi64Dst;
                int64_t const i64Src = *pi64Src;
                if (!i64Dst)
                    *pi64Dst = i64Src;
                else if (!i64Src)
                    *pi64Dst = (i64Dst + i64Src) / 2;
                pi64Dst++;
                pi64Src++;
            }
            break;
    }
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
 * @note Currently does not handle any endianness conversion yet!
 */
#define AUDMIXBUF_CONVERT(a_Name, a_Type, _aMin, _aMax, _aSigned, _aShift) \
    /* Clips a specific output value to a single sample value. */ \
    DECLINLINE(int64_t) audioMixBufClipFrom##a_Name(a_Type aVal) \
    { \
        /* left shifting of signed values is not defined, therefore the intermediate uint64_t cast */ \
        if (_aSigned) \
            return (int64_t) (((uint64_t) ((int64_t) aVal                     )) << (32 - _aShift)); \
        return     (int64_t) (((uint64_t) ((int64_t) aVal - ((_aMax >> 1) + 1))) << (32 - _aShift)); \
    } \
    \
    /* Clips a single sample value to a specific output value. */ \
    DECLINLINE(a_Type) audioMixBufClipTo##a_Name(int64_t iVal) \
    { \
        /*if (iVal >= 0x7fffffff) return _aMax; if (iVal < -INT64_C(0x80000000)) return _aMin;*/ \
        if (!(((uint64_t)iVal + UINT64_C(0x80000000)) & UINT64_C(0xffffffff00000000))) \
        { \
            if (_aSigned) \
                return (a_Type)  (iVal >> (32 - _aShift)); \
            return     (a_Type) ((iVal >> (32 - _aShift)) + ((_aMax >> 1) + 1)); \
        } \
        AssertMsgFailed(("%RI64 (%#RX64)\n", iVal, iVal)); /* shouldn't be possible with current buffer operations */ \
        return iVal >= 0 ? _aMax : _aMin; \
    } \
    \
    DECLCALLBACK(uint32_t) audioMixBufConvFrom##a_Name##Stereo(PPDMAUDIOFRAME paDst, const void *pvSrc, uint32_t cbSrc, \
                                                               PCAUDMIXBUFCONVOPTS pOpts) \
    { \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        uint32_t cFrames = RT_MIN(pOpts->cFrames, cbSrc / sizeof(a_Type)); \
        AUDMIXBUF_MACRO_LOG(("cFrames=%RU32, BpS=%zu, lVol=%RU32, rVol=%RU32\n", \
                             pOpts->cFrames, sizeof(a_Type), pOpts->From.Volume.uLeft, pOpts->From.Volume.uRight)); \
        for (uint32_t i = 0; i < cFrames; i++) \
        { \
            paDst->i64LSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##a_Name(*pSrc++), pOpts->From.Volume.uLeft ) >> AUDIOMIXBUF_VOL_SHIFT; \
            paDst->i64RSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##a_Name(*pSrc++), pOpts->From.Volume.uRight) >> AUDIOMIXBUF_VOL_SHIFT; \
            paDst++; \
        } \
        \
        return cFrames; \
    } \
    \
    DECLCALLBACK(uint32_t) audioMixBufConvFrom##a_Name##Mono(PPDMAUDIOFRAME paDst, const void *pvSrc, uint32_t cbSrc, \
                                                             PCAUDMIXBUFCONVOPTS pOpts) \
    { \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        const uint32_t cFrames = RT_MIN(pOpts->cFrames, cbSrc / sizeof(a_Type)); \
        AUDMIXBUF_MACRO_LOG(("cFrames=%RU32, BpS=%zu, lVol=%RU32, rVol=%RU32\n", \
                             cFrames, sizeof(a_Type), pOpts->From.Volume.uLeft, pOpts->From.Volume.uRight)); \
        for (uint32_t i = 0; i < cFrames; i++) \
        { \
            paDst->i64LSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##a_Name(*pSrc), pOpts->From.Volume.uLeft)  >> AUDIOMIXBUF_VOL_SHIFT; \
            paDst->i64RSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##a_Name(*pSrc), pOpts->From.Volume.uRight) >> AUDIOMIXBUF_VOL_SHIFT; \
            pSrc++; \
            paDst++; \
        } \
        \
        return cFrames; \
    } \
    \
    DECLCALLBACK(void) audioMixBufConvTo##a_Name##Stereo(void *pvDst, PCPDMAUDIOFRAME paSrc, PCAUDMIXBUFCONVOPTS pOpts) \
    { \
        PCPDMAUDIOFRAME pSrc = paSrc; \
        a_Type *pDst = (a_Type *)pvDst; \
        uint32_t cFrames = pOpts->cFrames; \
        while (cFrames--) \
        { \
            AUDMIXBUF_MACRO_LOG(("%p: l=%RI64, r=%RI64\n", pSrc, pSrc->i64LSample, pSrc->i64RSample)); \
            pDst[0] = audioMixBufClipTo##a_Name(pSrc->i64LSample); \
            pDst[1] = audioMixBufClipTo##a_Name(pSrc->i64RSample); \
            AUDMIXBUF_MACRO_LOG(("\t-> l=%RI16, r=%RI16\n", pDst[0], pDst[1])); \
            pDst += 2; \
            pSrc++; \
        } \
    } \
    \
    DECLCALLBACK(void) audioMixBufConvTo##a_Name##Mono(void *pvDst, PCPDMAUDIOFRAME paSrc, PCAUDMIXBUFCONVOPTS pOpts) \
    { \
        PCPDMAUDIOFRAME pSrc = paSrc; \
        a_Type *pDst = (a_Type *)pvDst; \
        uint32_t cFrames = pOpts->cFrames; \
        while (cFrames--) \
        { \
            *pDst++ = audioMixBufClipTo##a_Name((pSrc->i64LSample + pSrc->i64RSample) / 2); \
            pSrc++; \
        } \
    } \
    /* Encoders for peek: */ \
    \
    /* 2ch -> 2ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode2ChTo2Ch,a_Name)(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFPEEKSTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type *pDst = (a_Type *)pvDst; \
        while (cFrames-- > 0) \
        { \
            pDst[0] = audioMixBufClipTo##a_Name(pi64Src[0]); \
            pDst[1] = audioMixBufClipTo##a_Name(pi64Src[1]); \
            AUDMIXBUF_MACRO_LOG(("%p: %RI64 / %RI64 => %RI64 / %RI64\n", \
                                 &pi64Src[0], pi64Src[0], pi64Src[1], (int64_t)pDst[0], (int64_t)pDst[1])); \
            pDst    += 2; \
            pi64Src += 2; \
        } \
    } \
    \
    /* 2ch -> 1ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode2ChTo1Ch,a_Name)(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFPEEKSTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type *pDst = (a_Type *)pvDst; \
        while (cFrames-- > 0) \
        { \
             pDst[0] = audioMixBufClipTo##a_Name(audioMixBufBlendSampleRet(pi64Src[0], pi64Src[1])); \
             pDst    += 1; \
             pi64Src += 2; \
        } \
    } \
    \
    /* 1ch -> 2ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode1ChTo2Ch,a_Name)(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFPEEKSTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type *pDst = (a_Type *)pvDst; \
        while (cFrames-- > 0) \
        { \
            pDst[0] = pDst[1] = audioMixBufClipTo##a_Name(pi64Src[0]); \
            pDst    += 2; \
            pi64Src += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */ \
        } \
    } \
    /* 1ch -> 1ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode1ChTo1Ch,a_Name)(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFPEEKSTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type *pDst = (a_Type *)pvDst; \
        while (cFrames-- > 0) \
        { \
             pDst[0] = audioMixBufClipTo##a_Name(pi64Src[0]); \
             pDst    += 1; \
             pi64Src += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */ \
        } \
    } \
    \
    /* Decoders for write: */ \
    \
    /* 2ch -> 2ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode2ChTo2Ch,a_Name)(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            pi64Dst[0] = audioMixBufClipFrom##a_Name(pSrc[0]); \
            pi64Dst[1] = audioMixBufClipFrom##a_Name(pSrc[1]); \
            AUDMIXBUF_MACRO_LOG(("%p: %RI64 / %RI64 => %RI64 / %RI64\n", \
                                 &pSrc[0], (int64_t)pSrc[0], (int64_t)pSrc[1], pi64Dst[0], pi64Dst[1])); \
            pi64Dst  += 2; \
            pSrc     += 2; \
        } \
    } \
    \
    /* 2ch -> 1ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode2ChTo1Ch,a_Name)(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            pi64Dst[0] = audioMixBufBlendSampleRet(audioMixBufClipFrom##a_Name(pSrc[0]), audioMixBufClipFrom##a_Name(pSrc[1])); \
            pi64Dst  += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */ \
            pSrc     += 2; \
        } \
    } \
    \
    /* 1ch -> 2ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode1ChTo2Ch,a_Name)(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            pi64Dst[1] = pi64Dst[0] = audioMixBufClipFrom##a_Name(pSrc[0]); \
            pi64Dst  += 2; \
            pSrc     += 1; \
        } \
    } \
    \
    /* 1ch -> 1ch */ \
    static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode1ChTo1Ch,a_Name)(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, \
                                                                          PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            pi64Dst[0] = audioMixBufClipFrom##a_Name(pSrc[0]); \
            pi64Dst  += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */ \
            pSrc     += 1; \
        } \
    } \
    \
    /* Decoders for blending: */ \
    \
    /* 2ch -> 2ch */ \
    static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode2ChTo2Ch,a_Name,Blend)(int64_t *pi64Dst, void const *pvSrc, \
                                                                                 uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            audioMixBufBlendSample(&pi64Dst[0], audioMixBufClipFrom##a_Name(pSrc[0])); \
            audioMixBufBlendSample(&pi64Dst[1], audioMixBufClipFrom##a_Name(pSrc[1])); \
            AUDMIXBUF_MACRO_LOG(("%p: %RI64 / %RI64 => %RI64 / %RI64\n", \
                                 &pSrc[0], (int64_t)pSrc[0], (int64_t)pSrc[1], pi64Dst[0], pi64Dst[1])); \
            pi64Dst  += 2; \
            pSrc     += 2; \
        } \
    } \
    \
    /* 2ch -> 1ch */ \
    static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode2ChTo1Ch,a_Name,Blend)(int64_t *pi64Dst, void const *pvSrc, \
                                                                                 uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            audioMixBufBlendSample(&pi64Dst[0], audioMixBufBlendSampleRet(audioMixBufClipFrom##a_Name(pSrc[0]), \
                                                                          audioMixBufClipFrom##a_Name(pSrc[1]))); \
            pi64Dst  += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */ \
            pSrc     += 2; \
        } \
    } \
    \
    /* 1ch -> 2ch */ \
    static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode1ChTo2Ch,a_Name,Blend)(int64_t *pi64Dst, void const *pvSrc, \
                                                                                 uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            int64_t const i64Src = audioMixBufClipFrom##a_Name(pSrc[0]); \
            audioMixBufBlendSample(&pi64Dst[0], i64Src); \
            audioMixBufBlendSample(&pi64Dst[1], i64Src); \
            pi64Dst  += 2; \
            pSrc     += 1; \
        } \
    } \
    \
    /* 1ch -> 1ch */ \
    static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode1ChTo1Ch,a_Name,Blend)(int64_t *pi64Dst, void const *pvSrc, \
                                                                                 uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
    { \
        RT_NOREF_PV(pState); \
        a_Type const *pSrc = (a_Type const *)pvSrc; \
        while (cFrames-- > 0) \
        { \
            audioMixBufBlendSample(&pi64Dst[0], audioMixBufClipFrom##a_Name(pSrc[0])); \
            pi64Dst  += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */ \
            pSrc     += 1; \
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

/*
 * Manually coded signed 64-bit conversion.
 */
#if 0
DECLCALLBACK(uint32_t) audioMixBufConvFromS64Stereo(PPDMAUDIOFRAME paDst, const void *pvSrc, uint32_t cbSrc,
                                                    PCAUDMIXBUFCONVOPTS pOpts)
{
    _aType const *pSrc = (_aType const *)pvSrc;
    uint32_t cFrames = RT_MIN(pOpts->cFrames, cbSrc / sizeof(_aType));
    AUDMIXBUF_MACRO_LOG(("cFrames=%RU32, BpS=%zu, lVol=%RU32, rVol=%RU32\n",
                         pOpts->cFrames, sizeof(_aType), pOpts->From.Volume.uLeft, pOpts->From.Volume.uRight));
    for (uint32_t i = 0; i < cFrames; i++)
    {
        paDst->i64LSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##_aName(*pSrc++), pOpts->From.Volume.uLeft ) >> AUDIOMIXBUF_VOL_SHIFT; \
        paDst->i64RSample = ASMMult2xS32RetS64((int32_t)audioMixBufClipFrom##_aName(*pSrc++), pOpts->From.Volume.uRight) >> AUDIOMIXBUF_VOL_SHIFT; \
        paDst++;
    }

    return cFrames;
}
#endif

DECLCALLBACK(void) audioMixBufConvToRawS64Stereo(void *pvDst, PCPDMAUDIOFRAME paSrc, PCAUDMIXBUFCONVOPTS pOpts)
{
    AssertCompile(sizeof(paSrc[0]) == sizeof(int64_t) * 2);
    memcpy(pvDst, paSrc, sizeof(int64_t) * 2 * pOpts->cFrames);
}

/* Encoders for peek: */

static DECLCALLBACK(void)
audioMixBufEncode2ChTo2ChRaw(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, PAUDIOMIXBUFPEEKSTATE pState)
{
    RT_NOREF_PV(pState);
    memcpy(pvDst, pi64Src, sizeof(int64_t) * 2 * cFrames);
}

static DECLCALLBACK(void)
audioMixBufEncode2ChTo1ChRaw(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, PAUDIOMIXBUFPEEKSTATE pState)
{
    RT_NOREF_PV(pState);
    int64_t *pi64Dst = (int64_t *)pvDst;
    while (cFrames-- > 0)
    {
        *pi64Dst = (pi64Src[0] + pi64Src[1]) / 2;
        pi64Dst += 1;
        pi64Src += 2;
    }
}

static DECLCALLBACK(void)
audioMixBufEncode1ChTo2ChRaw(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, PAUDIOMIXBUFPEEKSTATE pState)
{
    RT_NOREF_PV(pState);
    int64_t *pi64Dst = (int64_t *)pvDst;
    while (cFrames-- > 0)
    {
        pi64Dst[0] = pi64Dst[1] = *pi64Src;
        pi64Dst += 2;
        pi64Src += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */
    }
}

static DECLCALLBACK(void)
audioMixBufEncode1ChTo1ChRaw(void *pvDst, int64_t const *pi64Src, uint32_t cFrames, PAUDIOMIXBUFPEEKSTATE pState)
{
    RT_NOREF_PV(pState);
    /** @todo memcpy(pvDst, pi64Src, sizeof(int64_t) * 1 * cFrames); when we do
     *        multi channel mixbuf support. */
    int64_t *pi64Dst = (int64_t *)pvDst;
    while (cFrames-- > 0)
    {
        *pi64Dst = *pi64Src;
        pi64Dst += 1;
        pi64Src += 2;
    }
}


/* Decoders for write: */

static DECLCALLBACK(void)
audioMixBufDecode2ChTo2ChRaw(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    memcpy(pi64Dst, pvSrc, sizeof(int64_t) * 2 * cFrames);
}

static DECLCALLBACK(void)
audioMixBufDecode2ChTo1ChRaw(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    int64_t const *pi64Src = (int64_t const *)pvSrc;
    while (cFrames-- > 0)
    {
        *pi64Dst = (pi64Src[0] + pi64Src[1]) / 2;
        pi64Dst += 2;  /** @todo when we do multi channel mixbuf support, this can change to 1 */
        pi64Src += 2;
    }
}

static DECLCALLBACK(void)
audioMixBufDecode1ChTo2ChRaw(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    int64_t const *pi64Src = (int64_t const *)pvSrc;
    while (cFrames-- > 0)
    {
        pi64Dst[0] = pi64Dst[1] = *pi64Src;
        pi64Dst += 2;
        pi64Src += 1;
    }
}

static DECLCALLBACK(void)
audioMixBufDecode1ChTo1ChRaw(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    /** @todo memcpy(pi64Dst, pvSrc, sizeof(int64_t) * 1 * cFrames); when we do
     *        multi channel mixbuf support. */
    int64_t const *pi64Src = (int64_t const *)pvSrc;
    while (cFrames-- > 0)
    {
        *pi64Dst = *pi64Src;
        pi64Dst += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */
        pi64Src += 1;
    }
}


/* Decoders for blending: */

static DECLCALLBACK(void)
audioMixBufDecode2ChTo2ChRawBlend(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    audioMixBufBlendBuffer(pi64Dst, (int64_t const *)pvSrc, cFrames, 2);
}

static DECLCALLBACK(void)
audioMixBufDecode2ChTo1ChRawBlend(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    int64_t const *pi64Src = (int64_t const *)pvSrc;
    while (cFrames-- > 0)
    {
        audioMixBufBlendSample(pi64Dst, audioMixBufBlendSampleRet(pi64Src[0], pi64Src[1]));
        pi64Dst += 2;  /** @todo when we do multi channel mixbuf support, this can change to 1 */
        pi64Src += 2;
    }
}

static DECLCALLBACK(void)
audioMixBufDecode1ChTo2ChRawBlend(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    int64_t const *pi64Src = (int64_t const *)pvSrc;
    while (cFrames-- > 0)
    {
        int64_t const i64Src = *pi64Src;
        audioMixBufBlendSample(&pi64Dst[0], i64Src);
        audioMixBufBlendSample(&pi64Dst[1], i64Src);
        pi64Dst += 2;
        pi64Src += 1;
    }
}

static DECLCALLBACK(void)
audioMixBufDecode1ChTo1ChRawBlend(int64_t *pi64Dst, void const *pvSrc, uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState)
{
    RT_NOREF_PV(pState);
    /** @todo memcpy(pi64Dst, pvSrc, sizeof(int64_t) * 1 * cFrames); when we do
     *        multi channel mixbuf support. */
    int64_t const *pi64Src = (int64_t const *)pvSrc;
    while (cFrames-- > 0)
    {
        audioMixBufBlendSample(&pi64Dst[0], *pi64Src);
        pi64Dst += 2; /** @todo when we do multi channel mixbuf support, this can change to 1 */
        pi64Src += 1;
    }
}

#undef AUDMIXBUF_MACRO_LOG

/**
 * Looks up the matching conversion function for converting audio frames from a
 * source format.
 *
 * @returns Pointer to matching conversion function, NULL if not supported.
 * @param   pProps  The audio format to find a "from" converter for.
 */
static PFNAUDIOMIXBUFCONVFROM audioMixBufConvFromLookup(PCPDMAUDIOPCMPROPS pProps)
{
    if (PDMAudioPropsIsSigned(pProps))
    {
        switch (PDMAudioPropsChannels(pProps))
        {
            case 2:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvFromS8Stereo;
                    case 2:  return audioMixBufConvFromS16Stereo;
                    case 4:  return audioMixBufConvFromS32Stereo;
                    //case 8:  return pProps->fRaw ? audioMixBufConvToRawS64Stereo : NULL;
                    default: return NULL;
                }

            case 1:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvFromS8Mono;
                    case 2:  return audioMixBufConvFromS16Mono;
                    case 4:  return audioMixBufConvFromS32Mono;
                    default: return NULL;
                }
            default:
                return NULL;
        }
    }
    else /* Unsigned */
    {
        switch (PDMAudioPropsChannels(pProps))
        {
            case 2:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvFromU8Stereo;
                    case 2:  return audioMixBufConvFromU16Stereo;
                    case 4:  return audioMixBufConvFromU32Stereo;
                    default: return NULL;
                }

            case 1:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvFromU8Mono;
                    case 2:  return audioMixBufConvFromU16Mono;
                    case 4:  return audioMixBufConvFromU32Mono;
                    default: return NULL;
                }
            default:
                return NULL;
        }
    }
    /* not reached */
}

/**
 * Looks up the matching conversion function for converting audio frames to a
 * destination format.
 *
 * @returns Pointer to matching conversion function, NULL if not supported.
 * @param   pProps  The audio format to find a "to" converter for.
 */
static PFNAUDIOMIXBUFCONVTO audioMixBufConvToLookup(PCPDMAUDIOPCMPROPS pProps)
{
    if (PDMAudioPropsIsSigned(pProps))
    {
        switch (PDMAudioPropsChannels(pProps))
        {
            case 2:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvToS8Stereo;
                    case 2:  return audioMixBufConvToS16Stereo;
                    case 4:  return audioMixBufConvToS32Stereo;
                    case 8:  return pProps->fRaw ? audioMixBufConvToRawS64Stereo : NULL;
                    default: return NULL;
                }

            case 1:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvToS8Mono;
                    case 2:  return audioMixBufConvToS16Mono;
                    case 4:  return audioMixBufConvToS32Mono;
                    default: return NULL;
                }
            default:
                return NULL;
        }
    }
    else /* Unsigned */
    {
        switch (PDMAudioPropsChannels(pProps))
        {
            case 2:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvToU8Stereo;
                    case 2:  return audioMixBufConvToU16Stereo;
                    case 4:  return audioMixBufConvToU32Stereo;
                    default: return NULL;
                }

            case 1:
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:  return audioMixBufConvToU8Mono;
                    case 2:  return audioMixBufConvToU16Mono;
                    case 4:  return audioMixBufConvToU32Mono;
                    default: return NULL;
                }
            default:
                return NULL;
        }
    }
    /* not reached */
}

/**
 * Converts a PDM audio volume to an internal mixing buffer volume.
 *
 * @returns VBox status code.
 * @param   pVolDst                 Where to store the converted mixing buffer volume.
 * @param   pVolSrc                 Volume to convert.
 */
static int audioMixBufConvVol(PAUDMIXBUFVOL pVolDst, PCPDMAUDIOVOLUME pVolSrc)
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
 * @returns VBox status code.
 * @param   pMixBuf                 Mixing buffer to initialize.
 * @param   pszName                 Name of mixing buffer for easier identification. Optional.
 * @param   pProps                  PCM audio properties to use for the mixing buffer.
 * @param   cFrames                 Maximum number of audio frames the mixing buffer can hold.
 */
int AudioMixBufInit(PAUDIOMIXBUF pMixBuf, const char *pszName, PCPDMAUDIOPCMPROPS pProps, uint32_t cFrames)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pProps,  VERR_INVALID_POINTER);
    Assert(PDMAudioPropsAreValid(pProps));

    pMixBuf->uMagic  = AUDIOMIXBUF_MAGIC;

    pMixBuf->pFrames = NULL;
    pMixBuf->cFrames = 0;

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

    pMixBuf->Props       = *pProps;
    pMixBuf->pfnConvFrom = audioMixBufConvFromLookup(pProps);
    pMixBuf->pfnConvTo   = audioMixBufConvToLookup(pProps);

    pMixBuf->pszName = RTStrDup(pszName);
    if (pMixBuf->pszName)
    {
        pMixBuf->pFrames = (PPDMAUDIOFRAME)RTMemAllocZ(cFrames * sizeof(PDMAUDIOFRAME));
        if (pMixBuf->pFrames)
        {
            pMixBuf->cFrames = cFrames;
#ifdef AUDMIXBUF_LOG_ENABLED
            char szTmp[PDMAUDIOPROPSTOSTRING_MAX];
            AUDMIXBUF_LOG(("%s: %s - cFrames=%#x (%d)\n",
                           pMixBuf->pszName, PDMAudioPropsToString(pProps, szTmp, sizeof(szTmp)), cFrames, cFrames));
#endif
            return VINF_SUCCESS;
        }
        RTStrFree(pMixBuf->pszName);
        pMixBuf->pszName = NULL;
    }
    pMixBuf->uMagic = AUDIOMIXBUF_MAGIC_DEAD;
    return VERR_NO_MEMORY;
}

/**
 * Checks if the buffer is empty.
 *
 * @retval  true if empty buffer.
 * @retval  false if not empty and there are frames to be processed.
 * @param   pMixBuf     The mixing buffer.
 */
bool AudioMixBufIsEmpty(PCAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, true);
    return pMixBuf->cUsed == 0;
}

/**
 * Returns number of available live frames, that is, frames that
 * have been written into the mixing buffer but not have been processed yet.
 *
 * For a parent buffer, this simply returns the currently used number of frames
 * in the buffer.
 *
 * For a child buffer, this returns the number of frames which have been mixed
 * to the parent and were not processed by the parent yet.
 *
 * @return  uint32_t                Number of live frames available.
 * @param   pMixBuf                 Mixing buffer to return value for.
 */
uint32_t AudioMixBufLive(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    uint32_t const cFrames = pMixBuf->cFrames;
    uint32_t       cAvail  = pMixBuf->cUsed;
    AssertStmt(cAvail <= cFrames, cAvail = cFrames);
    return cAvail;
}

#ifdef DEBUG

/**
 * Prints a single mixing buffer.
 * Internal helper function for debugging. Do not use directly.
 *
 * @returns VBox status code.
 * @param   pMixBuf                 Mixing buffer to print.
 * @param   pszFunc                 Function name to log this for.
 * @param   uIdtLvl                 Indention level to use.
 */
static void audioMixBufDbgPrintSingle(PAUDIOMIXBUF pMixBuf, const char *pszFunc, uint16_t uIdtLvl)
{
    Log(("%s: %*s %s: offRead=%RU32, offWrite=%RU32, cMixed=%RU32 -> %RU32/%RU32\n",
         pszFunc, uIdtLvl * 4, "",
         pMixBuf->pszName, pMixBuf->offRead, pMixBuf->offWrite, pMixBuf->cMixed, pMixBuf->cUsed, pMixBuf->cFrames));
}

/**
 * Validates a single mixing buffer.
 *
 * @return  @true if the buffer state is valid or @false if not.
 * @param   pMixBuf                 Mixing buffer to validate.
 */
static bool audioMixBufDbgValidate(PAUDIOMIXBUF pMixBuf)
{
    //const uint32_t offReadEnd  = (pMixBuf->offRead + pMixBuf->cUsed) % pMixBuf->cFrames;
    //const uint32_t offWriteEnd = (pMixBuf->offWrite + (pMixBuf->cFrames - pMixBuf->cUsed)) % pMixBuf->cFrames;

    bool fValid = true;

    AssertStmt(pMixBuf->offRead  <= pMixBuf->cFrames, fValid = false);
    AssertStmt(pMixBuf->offWrite <= pMixBuf->cFrames, fValid = false);
    AssertStmt(pMixBuf->cUsed    <= pMixBuf->cFrames, fValid = false);

    if (pMixBuf->offWrite > pMixBuf->offRead)
    {
        if (pMixBuf->offWrite - pMixBuf->offRead != pMixBuf->cUsed)
            fValid = false;
    }
    else if (pMixBuf->offWrite < pMixBuf->offRead)
    {
        if (pMixBuf->offWrite + pMixBuf->cFrames - pMixBuf->offRead != pMixBuf->cUsed)
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
 * Prints statistics and status of the full chain of a mixing buffer to the logger,
 * starting from the top root mixing buffer.
 * For debug versions only.
 *
 * @returns VBox status code.
 * @param   pMixBuf                 Mixing buffer to print.
 */
void AudioMixBufDbgPrintChain(PAUDIOMIXBUF pMixBuf)
{
    audioMixBufDbgPrintSingle(pMixBuf, __FUNCTION__, 0 /* uIdtLvl */);
}

DECL_FORCE_INLINE(void) audioMixBufDbgPrintInternal(PAUDIOMIXBUF pMixBuf, const char *pszFunc)
{
    audioMixBufDbgPrintSingle(pMixBuf, pszFunc, 0 /* iIdtLevel */);
}

/**
 * Prints statistics and status of a mixing buffer to the logger.
 * For debug versions only.
 *
 * @returns VBox status code.
 * @param   pMixBuf                 Mixing buffer to print.
 */
void AudioMixBufDbgPrint(PAUDIOMIXBUF pMixBuf)
{
    audioMixBufDbgPrintInternal(pMixBuf, __FUNCTION__);
}

#endif /* DEBUG */

/**
 * Returns the total number of audio frames used.
 *
 * @return  uint32_t
 * @param   pMixBuf
 */
uint32_t AudioMixBufUsed(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return pMixBuf->cUsed;
}

/**
 * Returns the total number of bytes used.
 *
 * @return  uint32_t
 * @param   pMixBuf
 */
uint32_t AudioMixBufUsedBytes(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return AUDIOMIXBUF_F2B(pMixBuf, pMixBuf->cUsed);
}

/**
 * Reads audio frames. The audio format of the mixing buffer will be used.
 *
 * @returns VBox status code.
 * @param   pMixBuf             Mixing buffer to read audio frames from.
 * @param   pvBuf               Pointer to buffer to write output to.
 * @param   cbBuf               Size (in bytes) of buffer to write to.
 * @param   pcAcquiredFrames    Where to return the number of frames in
 *                              the block that was acquired.
 */
int AudioMixBufAcquireReadBlock(PAUDIOMIXBUF pMixBuf, void *pvBuf, uint32_t cbBuf, uint32_t *pcAcquiredFrames)
{
    return AudioMixBufAcquireReadBlockEx(pMixBuf, &pMixBuf->Props, pvBuf, cbBuf, pcAcquiredFrames);
}

/**
 * Reads audio frames in a specific audio format.
 *
 * If the audio format of the mixing buffer and the requested audio format do
 * not match the output will be converted accordingly.
 *
 * @returns VBox status code.
 * @param   pMixBuf             Mixing buffer to read audio frames from.
 * @param   pDstProps           The target format.
 * @param   pvBuf               Pointer to buffer to write output to.
 * @param   cbBuf               Size (in bytes) of buffer to write to.
 * @param   pcAcquiredFrames    Where to return the number of frames in
 *                              the block that was acquired.
 */
int AudioMixBufAcquireReadBlockEx(PAUDIOMIXBUF pMixBuf, PCPDMAUDIOPCMPROPS pDstProps,
                                  void *pvBuf, uint32_t cbBuf, uint32_t *pcAcquiredFrames)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pcAcquiredFrames, VERR_INVALID_POINTER);

    /* Make sure that we at least have space for a full audio frame. */
    AssertReturn(AUDIOMIXBUF_B2F(pMixBuf, cbBuf), VERR_INVALID_PARAMETER);

    uint32_t cFramesToRead = RT_MIN(pMixBuf->cUsed, AUDIOMIXBUF_B2F(pMixBuf, cbBuf));

#ifdef AUDMIXBUF_LOG_ENABLED
    char szTmp1[PDMAUDIOPROPSTOSTRING_MAX], szTmp2[PDMAUDIOPROPSTOSTRING_MAX];
#endif
    AUDMIXBUF_LOG(("%s: cbBuf=%RU32 (%RU32 frames), cFramesToRead=%RU32, MixBuf=%s, pDstProps=%s\n",
                   pMixBuf->pszName, cbBuf, AUDIOMIXBUF_B2F(pMixBuf, cbBuf), cFramesToRead,
                   PDMAudioPropsToString(&pMixBuf->Props, szTmp1, sizeof(szTmp1)),
                   PDMAudioPropsToString(pDstProps, szTmp2, sizeof(szTmp2))));
    if (!cFramesToRead)
    {
#ifdef DEBUG
        audioMixBufDbgPrintInternal(pMixBuf, __FUNCTION__);
#endif
        *pcAcquiredFrames = 0;
        return VINF_SUCCESS;
    }

    PFNAUDIOMIXBUFCONVTO pfnConvTo;
    if (PDMAudioPropsAreEqual(&pMixBuf->Props, pDstProps))
        pfnConvTo = pMixBuf->pfnConvTo;
    else
        pfnConvTo = audioMixBufConvToLookup(pDstProps);
    AssertReturn(pfnConvTo, VERR_NOT_SUPPORTED);

    cFramesToRead = RT_MIN(cFramesToRead, pMixBuf->cFrames - pMixBuf->offRead);
    if (cFramesToRead)
    {
        AUDMIXBUFCONVOPTS convOpts;
        RT_ZERO(convOpts);
        convOpts.cFrames = cFramesToRead;

        AUDMIXBUF_LOG(("cFramesToRead=%RU32\n", cFramesToRead));

        pfnConvTo(pvBuf, pMixBuf->pFrames + pMixBuf->offRead, &convOpts);

#ifdef AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        int rc2 = RTFileOpen(&fh, AUDIOMIXBUF_DEBUG_DUMP_PCM_DATA_PATH "mixbuf_readcirc.pcm",
                             RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc2))
        {
            RTFileWrite(fh, pvBuf, AUDIOMIXBUF_F2B(pMixBuf, cFramesToRead), NULL);
            RTFileClose(fh);
        }
#endif
    }

    *pcAcquiredFrames = cFramesToRead;

#ifdef DEBUG
    audioMixBufDbgValidate(pMixBuf);
#endif

    AUDMIXBUF_LOG(("*pcAcquiredFrames=%RU32 (%RU32 bytes)\n", cFramesToRead, AUDIOMIXBUF_F2B(pMixBuf, cFramesToRead)));
    return VINF_SUCCESS;
}

/**
 * Releases a formerly acquired read block.
 *
 * @param   pMixBuf     Mixing buffer to release acquired read block for.
 * @param   cFrames     The number of frames to release.  (Can be less than the
 *                      acquired count.)
 */
void AudioMixBufReleaseReadBlock(PAUDIOMIXBUF pMixBuf, uint32_t cFrames)
{
    AssertPtrReturnVoid(pMixBuf);

    if (cFrames)
    {
        AssertStmt(pMixBuf->cUsed >= cFrames, cFrames = pMixBuf->cUsed);
        pMixBuf->offRead  = (pMixBuf->offRead + cFrames) % pMixBuf->cFrames;
        pMixBuf->cUsed   -= cFrames;
    }
}

/**
 * Returns the current read position of a mixing buffer.
 *
 * @returns VBox status code.
 * @param   pMixBuf                 Mixing buffer to return position for.
 */
uint32_t AudioMixBufReadPos(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    return pMixBuf->offRead;
}

/**
 * Resets a mixing buffer.
 *
 * @param   pMixBuf                 Mixing buffer to reset.
 */
void AudioMixBufReset(PAUDIOMIXBUF pMixBuf)
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
 * Drops all the frames in the given mixing buffer.
 *
 * Similar to AudioMixBufReset, only it doesn't bother clearing the buffer.
 *
 * @param   pMixBuf             The mixing buffer.
 */
void AudioMixBufDrop(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturnVoid(pMixBuf);
    AssertReturnVoid(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);

    AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

    pMixBuf->offRead  = 0;
    pMixBuf->offWrite = 0;
    pMixBuf->cMixed   = 0;
    pMixBuf->cUsed    = 0;
}


/*
 * Resampling core.
 */
/** @todo Separate down- and up-sampling, borrow filter code from RDP. */
#define COPY_LAST_FRAME_1CH(a_pi64Dst, a_pi64Src, a_cChannels) do { \
        (a_pi64Dst)[0] = (a_pi64Src)[0]; \
    } while (0)
#define COPY_LAST_FRAME_2CH(a_pi64Dst, a_pi64Src, a_cChannels) do { \
        (a_pi64Dst)[0] = (a_pi64Src)[0]; \
        (a_pi64Dst)[1] = (a_pi64Src)[1]; \
    } while (0)

#define INTERPOLATE_ONE(a_pi64Dst, a_pi64Src, a_pi64Last, a_i64FactorCur, a_i64FactorLast, a_iCh) \
        (a_pi64Dst)[a_iCh] = ((a_pi64Last)[a_iCh] * a_i64FactorLast + (a_pi64Src)[a_iCh] * a_i64FactorCur) >> 32
#define INTERPOLATE_1CH(a_pi64Dst, a_pi64Src, a_pi64Last, a_i64FactorCur, a_i64FactorLast, a_cChannels) do { \
        INTERPOLATE_ONE(a_pi64Dst, a_pi64Src, a_pi64Last, a_i64FactorCur, a_i64FactorLast, 0); \
    } while (0)
#define INTERPOLATE_2CH(a_pi64Dst, a_pi64Src, a_pi64Last, a_i64FactorCur, a_i64FactorLast, a_cChannels) do { \
        INTERPOLATE_ONE(a_pi64Dst, a_pi64Src, a_pi64Last, a_i64FactorCur, a_i64FactorLast, 0); \
        INTERPOLATE_ONE(a_pi64Dst, a_pi64Src, a_pi64Last, a_i64FactorCur, a_i64FactorLast, 1); \
    } while (0)

#define AUDIOMIXBUF_RESAMPLE(a_cChannels, a_Suffix) \
    /** @returns Number of destination frames written. */ \
    static DECLCALLBACK(uint32_t) \
    audioMixBufResample##a_cChannels##Ch##a_Suffix(int64_t *pi64Dst, uint32_t cDstFrames, \
                                                   int64_t const *pi64Src, uint32_t cSrcFrames, uint32_t *pcSrcFramesRead, \
                                                   PAUDIOSTREAMRATE pRate) \
    { \
        Log5(("Src: %RU32 L %RU32;  Dst: %RU32 L%RU32; uDstInc=%#RX64\n", \
              pRate->offSrc, cSrcFrames, RT_HI_U32(pRate->offDst), cDstFrames, pRate->uDstInc)); \
        int64_t * const       pi64DstStart = pi64Dst; \
        int64_t const * const pi64SrcStart = pi64Src; \
        \
        int64_t ai64LastFrame[a_cChannels]; \
        COPY_LAST_FRAME_##a_cChannels##CH(ai64LastFrame, pRate->SrcLast.ai64Samples, a_cChannels); \
        \
        while (cDstFrames > 0 && cSrcFrames > 0) \
        { \
            int32_t const cSrcNeeded = RT_HI_U32(pRate->offDst) - pRate->offSrc + 1; \
            if (cSrcNeeded > 0) \
            { \
                if ((uint32_t)cSrcNeeded + 1 < cSrcFrames) \
                { \
                    pRate->offSrc += (uint32_t)cSrcNeeded; \
                    cSrcFrames    -= (uint32_t)cSrcNeeded; \
                    pi64Src       += (uint32_t)cSrcNeeded * a_cChannels; \
                    COPY_LAST_FRAME_##a_cChannels##CH(ai64LastFrame, &pi64Src[-a_cChannels], a_cChannels); \
                } \
                else \
                { \
                    pi64Src       += cSrcFrames * a_cChannels; \
                    pRate->offSrc += cSrcFrames; \
                    COPY_LAST_FRAME_##a_cChannels##CH(pRate->SrcLast.ai64Samples, &pi64Src[-a_cChannels], a_cChannels); \
                    *pcSrcFramesRead = (pi64Src - pi64SrcStart) / a_cChannels; \
                    return (pi64Dst - pi64DstStart) / a_cChannels; \
                } \
            } \
            \
            /* Interpolate. */ \
            int64_t const offFactorCur  = pRate->offDst & UINT32_MAX; \
            int64_t const offFactorLast = (int64_t)_4G - offFactorCur; \
            INTERPOLATE_##a_cChannels##CH(pi64Dst, pi64Src, ai64LastFrame, offFactorCur, offFactorLast, a_cChannels); \
            \
            /* Advance. */ \
            pRate->offDst += pRate->uDstInc; \
            pi64Dst       += a_cChannels; \
            cDstFrames    -= 1; \
        } \
        \
        COPY_LAST_FRAME_##a_cChannels##CH(pRate->SrcLast.ai64Samples, ai64LastFrame, a_cChannels); \
        *pcSrcFramesRead = (pi64Src - pi64SrcStart) / a_cChannels; \
        return (pi64Dst - pi64DstStart) / a_cChannels; \
    }

AUDIOMIXBUF_RESAMPLE(1,Generic)
AUDIOMIXBUF_RESAMPLE(2,Generic)


/**
 * Resets the resampling state unconditionally.
 *
 * @param   pRate   The state to reset.
 */
static void audioMixBufRateResetAlways(PAUDIOSTREAMRATE pRate)
{
    pRate->offDst = 0;
    pRate->offSrc = 0;
    for (uintptr_t i = 0; i < RT_ELEMENTS(pRate->SrcLast.ai64Samples); i++)
        pRate->SrcLast.ai64Samples[0] = 0;
}


/**
 * Resets the resampling state.
 *
 * @param   pRate   The state to reset.
 */
DECLINLINE(void) audioMixBufRateReset(PAUDIOSTREAMRATE pRate)
{
    if (pRate->offDst == 0)
    { /* likely */ }
    else
    {
        Assert(!pRate->fNoConversionNeeded);
        audioMixBufRateResetAlways(pRate);
    }
}


/**
 * Initializes the frame rate converter state.
 *
 * @returns VBox status code.
 * @param   pRate       The state to initialize.
 * @param   uSrcHz      The source frame rate.
 * @param   uDstHz      The destination frame rate.
 * @param   cChannels   The number of channels in a frame.
 */
DECLINLINE(int) audioMixBufRateInit(PAUDIOSTREAMRATE pRate, uint32_t uSrcHz, uint32_t uDstHz, uint8_t cChannels)
{
    /*
     * Do we need to set up frequency conversion?
     *
     * Some examples to get an idea of what uDstInc holds:
     *   44100 to 44100 -> (44100<<32) / 44100 = 0x01'00000000 (4294967296)
     *   22050 to 44100 -> (22050<<32) / 44100 = 0x00'80000000 (2147483648)
     *   44100 to 22050 -> (44100<<32) / 22050 = 0x02'00000000 (8589934592)
     *   44100 to 48000 -> (44100<<32) / 48000 = 0x00'EB333333 (3946001203.2)
     *   48000 to 44100 -> (48000<<32) / 44100 = 0x01'16A3B35F (4674794335.7823129251700680272109)
     */
    audioMixBufRateResetAlways(pRate);
    if (uSrcHz == uDstHz)
    {
        pRate->fNoConversionNeeded = true;
        pRate->uDstInc             = RT_BIT_64(32);
        pRate->pfnResample         = NULL;
    }
    else
    {
        pRate->fNoConversionNeeded = false;
        pRate->uDstInc             = ((uint64_t)uSrcHz << 32) / uDstHz;
        AssertReturn(uSrcHz != 0, VERR_INVALID_PARAMETER);
        switch (cChannels)
        {
            case 1: pRate->pfnResample = audioMixBufResample1ChGeneric; break;
            case 2: pRate->pfnResample = audioMixBufResample2ChGeneric; break;
            default:
                AssertMsgFailedReturn(("resampling %u changes is not implemented yet\n", cChannels), VERR_OUT_OF_RANGE);
        }
    }
    return VINF_SUCCESS;
}



/**
 * Initializes the peek state, setting up encoder and (if necessary) resampling.
 *
 * @returns VBox status code.
 */
int AudioMixBufInitPeekState(PCAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFPEEKSTATE pState, PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtr(pMixBuf);
    AssertPtr(pState);
    AssertPtr(pProps);

    /*
     * Pick the encoding function first.
     */
    uint8_t const cSrcCh = PDMAudioPropsChannels(&pMixBuf->Props);
    uint8_t const cDstCh = PDMAudioPropsChannels(pProps);
    pState->cSrcChannels = cSrcCh;
    pState->cDstChannels = cDstCh;
    pState->cbDstFrame   = PDMAudioPropsFrameSize(pProps);
    if (PDMAudioPropsIsSigned(pProps))
    {
        switch (cDstCh)
        {
            case 1:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChS8  : audioMixBufEncode2ChTo1ChS8;
                        break;
                    case 2:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChS16 : audioMixBufEncode2ChTo1ChS16;
                        break;
                    case 4:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChS32 : audioMixBufEncode2ChTo1ChS32;
                        break;
                    case 8:
                        AssertReturn(pProps->fRaw, VERR_DISK_INVALID_FORMAT);
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChRaw : audioMixBufEncode2ChTo1ChRaw;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            case 2:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChS8  : audioMixBufEncode2ChTo2ChS8;
                        break;
                    case 2:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChS16 : audioMixBufEncode2ChTo2ChS16;
                        break;
                    case 4:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChS32 : audioMixBufEncode2ChTo2ChS32;
                        break;
                    case 8:
                        AssertReturn(pProps->fRaw, VERR_DISK_INVALID_FORMAT);
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChRaw : audioMixBufEncode2ChTo2ChRaw;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            default:
                /* Note: We may have dedicated encoders for a few selected multichannel
                         configurations, and generic ones that encodes channel by channel (i.e.
                         add the mixer channel count, destination frame size, and an array of
                         destination channel frame offsets to the state). */
                AssertMsgFailedReturn(("from %u to %u channels is not implemented yet\n", cSrcCh, cDstCh), VERR_OUT_OF_RANGE);
        }
    }
    else
    {
        switch (cDstCh)
        {
            case 1:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChU8  : audioMixBufEncode2ChTo1ChU8;
                        break;
                    case 2:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChU16 : audioMixBufEncode2ChTo1ChU16;
                        break;
                    case 4:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo1ChU32 : audioMixBufEncode2ChTo1ChU32;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            case 2:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChU8  : audioMixBufEncode2ChTo2ChU8;
                        break;
                    case 2:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChU16 : audioMixBufEncode2ChTo2ChU16;
                        break;
                    case 4:
                        pState->pfnEncode = cSrcCh == 1 ? audioMixBufEncode1ChTo2ChU32 : audioMixBufEncode2ChTo2ChU32;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            default:
                /* Note: We may have dedicated encoders for a few selected multichannel
                         configurations, and generic ones that encodes channel by channel (i.e.
                         add an array of destination channel frame offsets to the state). */
                AssertMsgFailedReturn(("from %u to %u channels is not implemented yet\n", cSrcCh, cDstCh), VERR_OUT_OF_RANGE);
        }
    }

    int rc = audioMixBufRateInit(&pState->Rate, PDMAudioPropsHz(&pMixBuf->Props), PDMAudioPropsHz(pProps), cSrcCh);
    AUDMIXBUF_LOG(("%s: %RU32 Hz to %RU32 Hz => uDstInc=0x%'RX64\n", pMixBuf->pszName, PDMAudioPropsHz(&pMixBuf->Props),
                   PDMAudioPropsHz(pProps), pState->Rate.uDstInc));
    return rc;
}


/**
 * Initializes the write/blend state, setting up decoders and (if necessary)
 * resampling.
 *
 * @returns VBox status code.
 */
int AudioMixBufInitWriteState(PCAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtr(pMixBuf);
    AssertPtr(pState);
    AssertPtr(pProps);

    /*
     * Pick the encoding function first.
     */
    uint8_t const cSrcCh = PDMAudioPropsChannels(pProps);
    uint8_t const cDstCh = PDMAudioPropsChannels(&pMixBuf->Props);
    pState->cSrcChannels = cSrcCh;
    pState->cDstChannels = cDstCh;
    pState->cbSrcFrame   = PDMAudioPropsFrameSize(pProps);
    if (PDMAudioPropsIsSigned(pProps))
    {
        switch (cDstCh)
        {
            case 1:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChS8       : audioMixBufDecode2ChTo1ChS8;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChS8Blend  : audioMixBufDecode2ChTo1ChS8Blend;
                        break;
                    case 2:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChS16      : audioMixBufDecode2ChTo1ChS16;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChS16Blend : audioMixBufDecode2ChTo1ChS16Blend;
                        break;
                    case 4:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChS32      : audioMixBufDecode2ChTo1ChS32;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChS32Blend : audioMixBufDecode2ChTo1ChS32Blend;
                        break;
                    case 8:
                        AssertReturn(pProps->fRaw, VERR_DISK_INVALID_FORMAT);
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChRaw      : audioMixBufDecode2ChTo1ChRaw;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChRawBlend : audioMixBufDecode2ChTo1ChRawBlend;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            case 2:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChS8       : audioMixBufDecode2ChTo2ChS8;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChS8Blend  : audioMixBufDecode2ChTo2ChS8Blend;
                        break;
                    case 2:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChS16      : audioMixBufDecode2ChTo2ChS16;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChS16Blend : audioMixBufDecode2ChTo2ChS16Blend;
                        break;
                    case 4:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChS32      : audioMixBufDecode2ChTo2ChS32;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChS32Blend : audioMixBufDecode2ChTo2ChS32Blend;
                        break;
                    case 8:
                        AssertReturn(pProps->fRaw, VERR_DISK_INVALID_FORMAT);
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChRaw      : audioMixBufDecode2ChTo2ChRaw;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChRawBlend : audioMixBufDecode2ChTo2ChRawBlend;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            default:
                /* Note: We may have dedicated encoders for a few selected multichannel
                         configurations, and generic ones that encodes channel by channel (i.e.
                         add the mixer channel count, destination frame size, and an array of
                         destination channel frame offsets to the state). */
                AssertMsgFailedReturn(("from %u to %u channels is not implemented yet\n", cSrcCh, cDstCh), VERR_OUT_OF_RANGE);
        }
    }
    else
    {
        switch (cDstCh)
        {
            case 1:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChU8       : audioMixBufDecode2ChTo1ChU8;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChU8Blend  : audioMixBufDecode2ChTo1ChU8Blend;
                        break;
                    case 2:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChU16      : audioMixBufDecode2ChTo1ChU16;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChU16Blend : audioMixBufDecode2ChTo1ChU16Blend;
                        break;
                    case 4:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChU32      : audioMixBufDecode2ChTo1ChU32;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo1ChU32Blend : audioMixBufDecode2ChTo1ChU32Blend;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            case 2:
                AssertReturn(cSrcCh == 1 || cSrcCh == 2, VERR_OUT_OF_RANGE);
                switch (PDMAudioPropsSampleSize(pProps))
                {
                    case 1:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChU8       : audioMixBufDecode2ChTo2ChU8;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChU8Blend  : audioMixBufDecode2ChTo2ChU8Blend;
                        break;
                    case 2:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChU16      : audioMixBufDecode2ChTo2ChU16;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChU16Blend : audioMixBufDecode2ChTo2ChU16Blend;
                        break;
                    case 4:
                        pState->pfnDecode      = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChU32      : audioMixBufDecode2ChTo2ChU32;
                        pState->pfnDecodeBlend = cSrcCh == 1 ? audioMixBufDecode1ChTo2ChU32Blend : audioMixBufDecode2ChTo2ChU32Blend;
                        break;
                    default:
                        AssertMsgFailedReturn(("%u bytes\n", PDMAudioPropsSampleSize(pProps)), VERR_OUT_OF_RANGE);
                }
                break;
            default:
                /* Note: We may have dedicated encoders for a few selected multichannel
                         configurations, and generic ones that encodes channel by channel (i.e.
                         add an array of destination channel frame offsets to the state). */
                AssertMsgFailedReturn(("from %u to %u channels is not implemented yet\n", cSrcCh, cDstCh), VERR_OUT_OF_RANGE);
        }
    }

    int rc = audioMixBufRateInit(&pState->Rate, PDMAudioPropsHz(pProps), PDMAudioPropsHz(&pMixBuf->Props), cDstCh);
    AUDMIXBUF_LOG(("%s: %RU32 Hz to %RU32 Hz => uDstInc=0x%'RX64\n", pMixBuf->pszName, PDMAudioPropsHz(pProps),
                   PDMAudioPropsHz(&pMixBuf->Props), pState->Rate.uDstInc));
    return rc;
}


/**
 * Worker for AudioMixBufPeek that handles the rate conversion case.
 */
DECL_NO_INLINE(static, void)
AudioMixBufPeekResampling(PCAUDIOMIXBUF pMixBuf, uint32_t offSrcFrame, uint32_t cMaxSrcFrames, uint32_t *pcSrcFramesPeeked,
                          PAUDIOMIXBUFPEEKSTATE pState, void *pvDst, uint32_t cbDst, uint32_t *pcbDstPeeked)
{
    *pcSrcFramesPeeked = 0;
    *pcbDstPeeked      = 0;
    while (cMaxSrcFrames > 0 && cbDst >= pState->cbDstFrame)
    {
        /* Rate conversion into temporary buffer. */
        int64_t  ai64DstRate[1024];
        uint32_t cSrcFrames    = RT_MIN(pMixBuf->cFrames - offSrcFrame, cMaxSrcFrames);
        uint32_t cMaxDstFrames = RT_MIN(RT_ELEMENTS(ai64DstRate) / pState->cDstChannels, cbDst / pState->cbDstFrame);
        uint32_t const cDstFrames = pState->Rate.pfnResample(ai64DstRate, cMaxDstFrames,
                                                             &pMixBuf->pFrames[offSrcFrame].i64LSample, cSrcFrames, &cSrcFrames,
                                                             &pState->Rate);
        *pcSrcFramesPeeked += cSrcFrames;
        cMaxSrcFrames      -= cSrcFrames;
        offSrcFrame         = (offSrcFrame + cSrcFrames) % pMixBuf->cFrames;

        /* Encode the converted frames. */
        uint32_t const cbDstEncoded = cDstFrames * pState->cbDstFrame;
        pState->pfnEncode(pvDst, ai64DstRate, cDstFrames, pState);
        *pcbDstPeeked      += cbDstEncoded;
        cbDst              -= cbDstEncoded;
        pvDst               = (uint8_t *)pvDst + cbDstEncoded;
    }
}


/**
 * Copies data out of the mixing buffer, converting it if needed, but leaves the
 * read offset untouched.
 *
 * @param   pMixBuf             The mixing buffer.
 * @param   offSrcFrame         The offset to start reading at relative to
 *                              current read position (offRead).  The caller has
 *                              made sure there is at least this number of
 *                              frames available in the buffer before calling.
 * @param   cMaxSrcFrames       Maximum number of frames to read.
 * @param   pcSrcFramesPeeked   Where to return the actual number of frames read
 *                              from the mixing buffer.
 * @param   pState              Output configuration & conversion state.
 * @param   pvDst               The destination buffer.
 * @param   cbDst               The size of the destination buffer in bytes.
 * @param   pcbDstPeeked        Where to put the actual number of bytes
 *                              returned.
 */
void AudioMixBufPeek(PCAUDIOMIXBUF pMixBuf, uint32_t offSrcFrame, uint32_t cMaxSrcFrames, uint32_t *pcSrcFramesPeeked,
                     PAUDIOMIXBUFPEEKSTATE pState, void *pvDst, uint32_t cbDst, uint32_t *pcbDstPeeked)
{
    /*
     * Check inputs.
     */
    AssertPtr(pMixBuf);
    Assert(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);
    AssertPtr(pState);
    AssertPtr(pState->pfnEncode);
    Assert(pState->cSrcChannels == PDMAudioPropsChannels(&pMixBuf->Props));
    Assert(cMaxSrcFrames > 0);
    Assert(cMaxSrcFrames <= pMixBuf->cFrames);
    Assert(offSrcFrame <= pMixBuf->cFrames);
    Assert(offSrcFrame + cMaxSrcFrames <= pMixBuf->cUsed);
    AssertPtr(pcSrcFramesPeeked);
    AssertPtr(pvDst);
    Assert(cbDst >= pState->cbDstFrame);
    AssertPtr(pcbDstPeeked);

    /*
     * Make start frame absolute.
     */
    offSrcFrame = (pMixBuf->offRead + offSrcFrame) % pMixBuf->cFrames;

    /*
     * Hopefully no sample rate conversion is necessary...
     */
    if (pState->Rate.fNoConversionNeeded)
    {
        /* Figure out how much we should convert. */
        cMaxSrcFrames      = RT_MIN(cMaxSrcFrames, cbDst / pState->cbDstFrame);
        *pcSrcFramesPeeked = cMaxSrcFrames;
        *pcbDstPeeked      = cMaxSrcFrames * pState->cbDstFrame;

        /* First chunk. */
        uint32_t const cSrcFrames1 = RT_MIN(pMixBuf->cFrames - offSrcFrame, cMaxSrcFrames);
        pState->pfnEncode(pvDst, &pMixBuf->pFrames[offSrcFrame].i64LSample, cSrcFrames1, pState);

        /* Another chunk from the start of the mixing buffer? */
        if (cMaxSrcFrames > cSrcFrames1)
            pState->pfnEncode((uint8_t *)pvDst + cSrcFrames1 * pState->cbDstFrame,
                              &pMixBuf->pFrames[0].i64LSample, cMaxSrcFrames - cSrcFrames1, pState);
    }
    else
        AudioMixBufPeekResampling(pMixBuf, offSrcFrame, cMaxSrcFrames, pcSrcFramesPeeked, pState, pvDst, cbDst, pcbDstPeeked);
}


/**
 * Worker for AudioMixBufWrite that handles the rate conversion case.
 */
DECL_NO_INLINE(static, void)
audioMixBufWriteResampling(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                           uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesWritten)
{
    *pcDstFramesWritten = 0;
    while (cMaxDstFrames > 0 && cbSrcBuf >= pState->cbSrcFrame)
    {
        /* Decode into temporary buffer. */
        int64_t  ai64SrcDecoded[1024];
        uint32_t cFramesDecoded = RT_MIN(RT_ELEMENTS(ai64SrcDecoded) / pState->cSrcChannels, cbSrcBuf / pState->cbSrcFrame);
        pState->pfnDecode(ai64SrcDecoded, pvSrcBuf, cFramesDecoded, pState);
        cbSrcBuf -= cFramesDecoded * pState->cbSrcFrame;
        pvSrcBuf  = (uint8_t const *)pvSrcBuf + cFramesDecoded * pState->cbSrcFrame;

        /* Rate convert that into the mixer. */
        uint32_t iFrameDecoded = 0;
        while (iFrameDecoded < cFramesDecoded)
        {
            uint32_t cDstMaxFrames    = RT_MIN(pMixBuf->cFrames - offDstFrame, cMaxDstFrames);
            uint32_t cSrcFrames       = cFramesDecoded - iFrameDecoded;
            uint32_t const cDstFrames = pState->Rate.pfnResample(&pMixBuf->pFrames[offDstFrame].i64LSample, cDstMaxFrames,
                                                                 &ai64SrcDecoded[iFrameDecoded * pState->cSrcChannels],
                                                                 cSrcFrames, &cSrcFrames, &pState->Rate);

            iFrameDecoded       += cSrcFrames;
            *pcDstFramesWritten += cDstFrames;
            offDstFrame          = (offDstFrame + cDstFrames) % pMixBuf->cFrames;
        }
    }

    /** @todo How to squeeze odd frames out of 22050 => 44100 conversion?   */
}


/**
 * Writes @a cbSrcBuf bytes to the mixer buffer starting at @a offDstFrame,
 * converting it as needed, leaving the write offset untouched.
 *
 * @param   pMixBuf             The mixing buffer.
 * @param   pState              Source configuration & conversion state.
 * @param   pvSrcBuf            The source frames.
 * @param   cbSrcBuf            Number of bytes of source frames.  This will be
 *                              convered in full.
 * @param   offDstFrame         Mixing buffer offset relative to the write
 *                              position.
 * @param   cMaxDstFrames       Max number of frames to write.
 * @param   pcDstFramesWritten  Where to return the number of frames actually
 *                              written.
 *
 * @note    Does not advance the write position, please call AudioMixBufCommit()
 *          to do that.
 */
void AudioMixBufWrite(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                      uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesWritten)
{
    /*
     * Check inputs.
     */
    AssertPtr(pMixBuf);
    Assert(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);
    AssertPtr(pState);
    AssertPtr(pState->pfnDecode);
    Assert(pState->cDstChannels == PDMAudioPropsChannels(&pMixBuf->Props));
    Assert(cMaxDstFrames > 0);
    Assert(cMaxDstFrames <= pMixBuf->cFrames - pMixBuf->cUsed);
    Assert(offDstFrame <= pMixBuf->cFrames);
    AssertPtr(pvSrcBuf);
    Assert(!(cbSrcBuf % pState->cbSrcFrame));
    AssertPtr(pcDstFramesWritten);

    /*
     * Make start frame absolute.
     */
    offDstFrame = (pMixBuf->offWrite + offDstFrame) % pMixBuf->cFrames;

    /*
     * Hopefully no sample rate conversion is necessary...
     */
    if (pState->Rate.fNoConversionNeeded)
    {
        /* Figure out how much we should convert. */
        Assert(cMaxDstFrames >= cbSrcBuf / pState->cbSrcFrame);
        cMaxDstFrames       = RT_MIN(cMaxDstFrames, cbSrcBuf / pState->cbSrcFrame);
        *pcDstFramesWritten = cMaxDstFrames;

        /* First chunk. */
        uint32_t const cDstFrames1 = RT_MIN(pMixBuf->cFrames - offDstFrame, cMaxDstFrames);
        pState->pfnDecode(&pMixBuf->pFrames[offDstFrame].i64LSample, pvSrcBuf, cDstFrames1, pState);

        /* Another chunk from the start of the mixing buffer? */
        if (cMaxDstFrames > cDstFrames1)
            pState->pfnDecode(&pMixBuf->pFrames[0].i64LSample, (uint8_t *)pvSrcBuf + cDstFrames1 * pState->cbSrcFrame,
                              cMaxDstFrames - cDstFrames1, pState);
    }
    else
        audioMixBufWriteResampling(pMixBuf, pState, pvSrcBuf, cbSrcBuf, offDstFrame, cMaxDstFrames, pcDstFramesWritten);
}


/**
 * Worker for AudioMixBufBlend that handles the rate conversion case.
 */
DECL_NO_INLINE(static, void)
audioMixBufBlendResampling(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                           uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesBlended)
{
    *pcDstFramesBlended = 0;
    while (cMaxDstFrames > 0 && cbSrcBuf >= pState->cbSrcFrame)
    {
        /* Decode into temporary buffer. */
        int64_t  ai64SrcDecoded[1024];
        uint32_t cFramesDecoded = RT_MIN(RT_ELEMENTS(ai64SrcDecoded) / pState->cSrcChannels, cbSrcBuf / pState->cbSrcFrame);
        pState->pfnDecode(ai64SrcDecoded, pvSrcBuf, cFramesDecoded, pState);
        cbSrcBuf -= cFramesDecoded * pState->cbSrcFrame;
        pvSrcBuf  = (uint8_t const *)pvSrcBuf + cFramesDecoded * pState->cbSrcFrame;

        /* Rate convert that into another temporary buffer and then blend that into the mixer. */
        uint32_t iFrameDecoded = 0;
        while (iFrameDecoded < cFramesDecoded)
        {
            int64_t  ai64SrcRate[1024];
            uint32_t cDstMaxFrames    = RT_MIN(RT_ELEMENTS(ai64SrcRate), cMaxDstFrames);
            uint32_t cSrcFrames       = cFramesDecoded - iFrameDecoded;
            uint32_t const cDstFrames = pState->Rate.pfnResample(&ai64SrcRate[0], cDstMaxFrames,
                                                                 &ai64SrcDecoded[iFrameDecoded * pState->cSrcChannels],
                                                                 cSrcFrames, &cSrcFrames, &pState->Rate);

            /* First chunk.*/
            uint32_t const cDstFrames1 = RT_MIN(pMixBuf->cFrames - offDstFrame, cDstFrames);
            audioMixBufBlendBuffer(&pMixBuf->pFrames[offDstFrame].i64LSample, ai64SrcRate, cDstFrames1, pState->cSrcChannels);

            /* Another chunk from the start of the mixing buffer? */
            if (cDstFrames > cDstFrames1)
                audioMixBufBlendBuffer(&pMixBuf->pFrames[0].i64LSample, &ai64SrcRate[cDstFrames1 * pState->cSrcChannels],
                                       cDstFrames - cDstFrames1, pState->cSrcChannels);

            /* Advance */
            iFrameDecoded       += cSrcFrames;
            *pcDstFramesBlended += cDstFrames;
            offDstFrame          = (offDstFrame + cDstFrames) % pMixBuf->cFrames;
        }
    }

    /** @todo How to squeeze odd frames out of 22050 => 44100 conversion?   */
}


/**
 * @todo not sure if 'blend' is the appropriate term here, but you know what
 *       we mean.
 */
void AudioMixBufBlend(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, const void *pvSrcBuf, uint32_t cbSrcBuf,
                      uint32_t offDstFrame, uint32_t cMaxDstFrames, uint32_t *pcDstFramesBlended)
{
    /*
     * Check inputs.
     */
    AssertPtr(pMixBuf);
    Assert(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);
    AssertPtr(pState);
    AssertPtr(pState->pfnDecode);
    AssertPtr(pState->pfnDecodeBlend);
    Assert(pState->cDstChannels == PDMAudioPropsChannels(&pMixBuf->Props));
    Assert(cMaxDstFrames > 0);
    Assert(cMaxDstFrames <= pMixBuf->cFrames - pMixBuf->cUsed);
    Assert(offDstFrame <= pMixBuf->cFrames);
    AssertPtr(pvSrcBuf);
    Assert(!(cbSrcBuf % pState->cbSrcFrame));
    AssertPtr(pcDstFramesBlended);

    /*
     * Make start frame absolute.
     */
    offDstFrame = (pMixBuf->offWrite + offDstFrame) % pMixBuf->cFrames;

    /*
     * Hopefully no sample rate conversion is necessary...
     */
    if (pState->Rate.fNoConversionNeeded)
    {
        /* Figure out how much we should convert. */
        Assert(cMaxDstFrames >= cbSrcBuf / pState->cbSrcFrame);
        cMaxDstFrames       = RT_MIN(cMaxDstFrames, cbSrcBuf / pState->cbSrcFrame);
        *pcDstFramesBlended = cMaxDstFrames;

        /* First chunk. */
        uint32_t const cDstFrames1 = RT_MIN(pMixBuf->cFrames - offDstFrame, cMaxDstFrames);
        pState->pfnDecodeBlend(&pMixBuf->pFrames[offDstFrame].i64LSample, pvSrcBuf, cDstFrames1, pState);

        /* Another chunk from the start of the mixing buffer? */
        if (cMaxDstFrames > cDstFrames1)
            pState->pfnDecodeBlend(&pMixBuf->pFrames[0].i64LSample, (uint8_t *)pvSrcBuf + cDstFrames1 * pState->cbSrcFrame,
                                   cMaxDstFrames - cDstFrames1, pState);
    }
    else
        audioMixBufBlendResampling(pMixBuf, pState, pvSrcBuf, cbSrcBuf, offDstFrame, cMaxDstFrames, pcDstFramesBlended);
}


/**
 * Writes @a cFrames of silence at @a offFrame relative to current write pos.
 *
 * This will also adjust the resampling state.
 *
 * @param   pMixBuf     The mixing buffer.
 * @param   pState      The write state.
 * @param   offFrame    Where to start writing silence relative to the current
 *                      write position.
 * @param   cFrames     Number of frames of silence.
 * @sa      AudioMixBufSilence
 *
 * @note    Does not advance the write position, please call AudioMixBufCommit()
 *          to do that.
 */
void AudioMixBufSilence(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, uint32_t offFrame, uint32_t cFrames)
{
    /*
     * Check inputs.
     */
    AssertPtr(pMixBuf);
    Assert(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);
    AssertPtr(pState);
    AssertPtr(pState->pfnDecode);
    Assert(pState->cDstChannels == PDMAudioPropsChannels(&pMixBuf->Props));
    Assert(cFrames > 0);
    Assert(cFrames <= pMixBuf->cFrames);
    Assert(offFrame <= pMixBuf->cFrames);
    Assert(offFrame + cFrames <= pMixBuf->cUsed);

    /*
     * Make start frame absolute.
     */
    offFrame = (pMixBuf->offWrite + offFrame) % pMixBuf->cFrames;

    /*
     * First chunk.
     */
    uint32_t cChunk = RT_MIN(pMixBuf->cFrames - offFrame, cFrames);
    RT_BZERO(&pMixBuf->pFrames[offFrame], cChunk * sizeof(pMixBuf->pFrames[0]));
    cFrames -= cChunk;

    /*
     * Second chunk, if needed.
     */
    if (cFrames > 0)
    {
        AssertStmt(cFrames <= pMixBuf->cFrames, cFrames = pMixBuf->cFrames);
        RT_BZERO(&pMixBuf->pFrames[0], cFrames * sizeof(pMixBuf->pFrames[0]));
    }

    /*
     * Reset the resampling state.
     */
    audioMixBufRateReset(&pState->Rate);
}


/**
 * Records a blending gap (silence) of @a cFrames.
 *
 * This is used to adjust or reset the resampling state so we start from a
 * silence state the next time we need to blend or write using @a pState.
 *
 * @param   pMixBuf     The mixing buffer.
 * @param   pState      The write state.
 * @param   cFrames     Number of frames of silence.
 * @sa      AudioMixBufSilence
 */
void AudioMixBufBlendGap(PAUDIOMIXBUF pMixBuf, PAUDIOMIXBUFWRITESTATE pState, uint32_t cFrames)
{
    /*
     * For now we'll just reset the resampling state regardless of how many
     * frames of silence there is.
     */
    audioMixBufRateReset(&pState->Rate);
    RT_NOREF(pMixBuf, cFrames);
}


/**
 * Advances the read position of the buffer.
 *
 * For use after done peeking with AudioMixBufPeek().
 *
 * @param   pMixBuf     The mixing buffer.
 * @param   cFrames     Number of frames to advance.
 * @sa      AudioMixBufCommit
 */
void AudioMixBufAdvance(PAUDIOMIXBUF pMixBuf, uint32_t cFrames)
{
    AssertPtrReturnVoid(pMixBuf);
    AssertReturnVoid(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);

    AssertStmt(cFrames <= pMixBuf->cUsed, cFrames = pMixBuf->cUsed);
    pMixBuf->cUsed   -= cFrames;
    pMixBuf->offRead  = (pMixBuf->offRead + cFrames) % pMixBuf->cFrames;
    LogFlowFunc(("%s: Advanced %u frames: offRead=%u cUsed=%u\n", pMixBuf->pszName, cFrames, pMixBuf->offRead, pMixBuf->cUsed));
}


/**
 * Worker for audioMixAdjustVolume that adjust one contiguous chunk.
 */
static void audioMixAdjustVolumeWorker(PAUDIOMIXBUF pMixBuf, uint32_t off, uint32_t cFrames)
{
    PPDMAUDIOFRAME const    paFrames = pMixBuf->pFrames;
    uint32_t const          uLeft    = pMixBuf->Volume.uLeft;
    uint32_t const          uRight   = pMixBuf->Volume.uRight;
    while (cFrames-- > 0)
    {
        paFrames[off].i64LSample = ASMMult2xS32RetS64(paFrames[off].i64LSample, uLeft)  >> AUDIOMIXBUF_VOL_SHIFT;
        paFrames[off].i64RSample = ASMMult2xS32RetS64(paFrames[off].i64RSample, uRight) >> AUDIOMIXBUF_VOL_SHIFT;
        off++;
    }
}


/**
 * Does volume adjustments for the given stretch of the buffer.
 *
 * @param   pMixBuf     The mixing buffer.
 * @param   offFirst    Where to start (validated).
 * @param   cFrames     How many frames (validated).
 */
static void audioMixAdjustVolume(PAUDIOMIXBUF pMixBuf, uint32_t offFirst, uint32_t cFrames)
{
    /* Caller has already validated these, so we don't need to repeat that in non-strict builds. */
    Assert(offFirst < pMixBuf->cFrames);
    Assert(cFrames <= pMixBuf->cFrames);

    /*
     * Muted?
     */
    if (pMixBuf->Volume.fMuted)
    {
        uint32_t const cFramesChunk1 = RT_MIN(pMixBuf->cFrames - offFirst, cFrames);
        RT_BZERO(&pMixBuf->pFrames[offFirst], sizeof(pMixBuf->pFrames[0]) * cFramesChunk1);
        if (cFramesChunk1 < cFrames)
            RT_BZERO(&pMixBuf->pFrames[0], sizeof(pMixBuf->pFrames[0]) * (cFrames - cFramesChunk1));
    }
    /*
     * Less than max volume?
     */
    else if (   pMixBuf->Volume.uLeft  != AUDIOMIXBUF_VOL_0DB
             || pMixBuf->Volume.uRight != AUDIOMIXBUF_VOL_0DB)
    {
        /* first chunk */
        uint32_t const cFramesChunk1 = RT_MIN(pMixBuf->cFrames - offFirst, cFrames);
        audioMixAdjustVolumeWorker(pMixBuf, offFirst, cFramesChunk1);
        if (cFramesChunk1 < cFrames)
            audioMixAdjustVolumeWorker(pMixBuf, 0, cFrames - cFramesChunk1);
    }
}


/**
 * Adjust for volume settings and advances the write position of the buffer.
 *
 * For use after done peeking with AudioMixBufWrite(), AudioMixBufSilence(),
 * AudioMixBufBlend() and AudioMixBufBlendGap().
 *
 * @param   pMixBuf     The mixing buffer.
 * @param   cFrames     Number of frames to advance.
 * @sa      AudioMixBufAdvance
 */
void AudioMixBufCommit(PAUDIOMIXBUF pMixBuf, uint32_t cFrames)
{
    AssertPtrReturnVoid(pMixBuf);
    AssertReturnVoid(pMixBuf->uMagic == AUDIOMIXBUF_MAGIC);

    AssertStmt(cFrames <= pMixBuf->cFrames - pMixBuf->cUsed, cFrames = pMixBuf->cFrames - pMixBuf->cUsed);

    audioMixAdjustVolume(pMixBuf, pMixBuf->offWrite, cFrames);

    pMixBuf->cUsed   += cFrames;
    pMixBuf->offWrite = (pMixBuf->offWrite + cFrames) % pMixBuf->cFrames;
    LogFlowFunc(("%s: Advanced %u frames: offWrite=%u cUsed=%u\n", pMixBuf->pszName, cFrames, pMixBuf->offWrite, pMixBuf->cUsed));
}


/**
 * Sets the overall (master) volume.
 *
 * @param   pMixBuf                 Mixing buffer to set volume for.
 * @param   pVol                    Pointer to volume structure to set.
 */
void AudioMixBufSetVolume(PAUDIOMIXBUF pMixBuf, PCPDMAUDIOVOLUME pVol)
{
    AssertPtrReturnVoid(pMixBuf);
    AssertPtrReturnVoid(pVol);

    LogFlowFunc(("%s: lVol=%RU8, rVol=%RU8, fMuted=%RTbool\n", pMixBuf->pszName, pVol->uLeft, pVol->uRight, pVol->fMuted));

    int rc2 = audioMixBufConvVol(&pMixBuf->Volume, pVol);
    AssertRC(rc2);
}

/**
 * Returns the maximum amount of audio frames this buffer can hold.
 *
 * @return  uint32_t                Size (in audio frames) the mixing buffer can hold.
 * @param   pMixBuf                 Mixing buffer to retrieve maximum for.
 */
uint32_t AudioMixBufSize(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return pMixBuf->cFrames;
}

/**
 * Returns the maximum amount of bytes this buffer can hold.
 *
 * @return  uint32_t                Size (in bytes) the mixing buffer can hold.
 * @param   pMixBuf                 Mixing buffer to retrieve maximum for.
 */
uint32_t AudioMixBufSizeBytes(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);
    return AUDIOMIXBUF_F2B(pMixBuf, pMixBuf->cFrames);
}

/**
 * Returns the current write position of a mixing buffer.
 *
 * @returns VBox status code.
 * @param   pMixBuf                 Mixing buffer to return position for.
 */
uint32_t AudioMixBufWritePos(PAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    return pMixBuf->offWrite;
}

