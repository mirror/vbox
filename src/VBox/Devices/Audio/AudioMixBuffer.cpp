/* $Id$ */
/** @file
 * VBox audio: Audio mixing buffer for converting reading/writing audio
 *             samples.
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

/*
 * DEBUG_DUMP_PCM_DATA enables dumping the raw PCM data
 * to a file on the host. Be sure to adjust the dumping path
 * to your needs before using this!
 */
#ifdef DEBUG
//# define DEBUG_DUMP_PCM_DATA
#endif

#include <iprt/asm-math.h>
#include <iprt/assert.h>
#ifdef DEBUG_DUMP_PCM_DATA
# include <iprt/file.h>
#endif
#include <iprt/mem.h>
#include <iprt/string.h> /* For RT_BZERO. */

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

#ifdef TESTCASE
# define LOG_ENABLED
# include <iprt/stream.h>
#endif
#include <VBox/err.h>

#include "AudioMixBuffer.h"

#if 0
# define AUDMIXBUF_LOG(x) LogFlowFunc(x)
#else
# if defined(TESTCASE)
#  define AUDMIXBUF_LOG(x) LogFunc(x)
# else
#  define AUDMIXBUF_LOG(x) do {} while (0)
# endif
#endif

/**
 * Structure for holding sample conversion parameters for
 * the audioMixBufConvFromXXX / audioMixBufConvToXXX macros.
 */
typedef struct AUDMIXBUF_CONVOPTS
{
    /** Number of audio samples to convert. */
    uint32_t       cSamples;
    /** Volume to apply during conversion. Pass 0
     *  to convert the original values. May not apply to
     *  all conversion functions. */
    PDMAUDIOVOLUME Volume;
} AUDMIXBUF_CONVOPTS, *PAUDMIXBUF_CONVOPTS;

/*
 * When running the audio testcases we want to verfiy
 * the macro-generated routines separately, so unmark them as being
 * inlined + static.
 */
#ifdef TESTCASE
# define AUDMIXBUF_MACRO_FN
#else
# define AUDMIXBUF_MACRO_FN static inline
#endif

#ifdef DEBUG
static uint64_t s_cSamplesMixedTotal = 0;
#endif

static void audioMixBufFreeBuf(PPDMAUDIOMIXBUF pMixBuf);
static inline void audioMixBufPrint(PPDMAUDIOMIXBUF pMixBuf);

typedef uint32_t (AUDMIXBUF_FN_CONVFROM) (PPDMAUDIOSAMPLE paDst, const void *pvSrc, size_t cbSrc, const PAUDMIXBUF_CONVOPTS pOpts);
typedef AUDMIXBUF_FN_CONVFROM *PAUDMIXBUF_FN_CONVFROM;

typedef void (AUDMIXBUF_FN_CONVTO) (void *pvDst, const PPDMAUDIOSAMPLE paSrc, const PAUDMIXBUF_CONVOPTS pOpts);
typedef AUDMIXBUF_FN_CONVTO *PAUDMIXBUF_FN_CONVTO;

/* Can return VINF_TRY_AGAIN for getting next pointer at beginning (circular) */
int audioMixBufAcquire(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamplesToRead,
                       PPDMAUDIOSAMPLE *ppvSamples, uint32_t *pcSamplesRead)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvSamples, VERR_INVALID_POINTER);
    AssertPtrReturn(pcSamplesRead, VERR_INVALID_POINTER);

    int rc;

    if (!cSamplesToRead)
    {
        *pcSamplesRead = 0;
        return VINF_SUCCESS;
    }

    uint32_t cSamplesRead;
    if (pMixBuf->offReadWrite + cSamplesToRead > pMixBuf->cSamples)
    {
        cSamplesRead = pMixBuf->cSamples - pMixBuf->offReadWrite;
        rc = VINF_TRY_AGAIN;
    }
    else
    {
        cSamplesRead = cSamplesToRead;
        rc = VINF_SUCCESS;
    }

    *ppvSamples = &pMixBuf->pSamples[pMixBuf->offReadWrite];
    AssertPtr(ppvSamples);

    pMixBuf->offReadWrite = (pMixBuf->offReadWrite + cSamplesRead) % pMixBuf->cSamples;
    Assert(pMixBuf->offReadWrite <= pMixBuf->cSamples);
    pMixBuf->cProcessed -= RT_MIN(cSamplesRead, pMixBuf->cProcessed);

    *pcSamplesRead = cSamplesRead;

    return rc;
}

/**
 * Clears (zeroes) the buffer by a certain amount of (processed) samples and
 * keeps track to eventually assigned children buffers.
 *
 * @param   pMixBuf
 * @param   cSamplesToClear
 */
void audioMixBufFinish(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamplesToClear)
{
    AUDMIXBUF_LOG(("cSamples=%RU32\n", cSamplesToClear));
    AUDMIXBUF_LOG(("%s: offReadWrite=%RU32, cProcessed=%RU32\n",
                   pMixBuf->pszName, pMixBuf->offReadWrite, pMixBuf->cProcessed));

    PPDMAUDIOMIXBUF pIter;
    RTListForEach(&pMixBuf->lstBuffers, pIter, PDMAUDIOMIXBUF, Node)
    {
        AUDMIXBUF_LOG(("\t%s: cMixed=%RU32 -> %RU32\n",
                       pIter->pszName, pIter->cMixed, pIter->cMixed - cSamplesToClear));

        pIter->cMixed -= RT_MIN(pIter->cMixed, cSamplesToClear);
        pIter->offReadWrite = 0;
    }

    uint32_t cLeft = RT_MIN(cSamplesToClear, pMixBuf->cSamples);
    uint32_t offClear;

    if (cLeft > pMixBuf->offReadWrite) /* Zero end of buffer first (wrap-around). */
    {
        AUDMIXBUF_LOG(("Clearing1: %RU32 - %RU32\n",
                       (pMixBuf->cSamples - (cLeft - pMixBuf->offReadWrite)),
                        pMixBuf->cSamples));

        RT_BZERO(pMixBuf->pSamples + (pMixBuf->cSamples - (cLeft - pMixBuf->offReadWrite)),
                 (cLeft - pMixBuf->offReadWrite) * sizeof(PDMAUDIOSAMPLE));

        cLeft -= cLeft - pMixBuf->offReadWrite;
        offClear = 0;
    }
    else
        offClear = pMixBuf->offReadWrite - cLeft;

    if (cLeft)
    {
        AUDMIXBUF_LOG(("Clearing2: %RU32 - %RU32\n",
                       offClear, offClear + cLeft));
        RT_BZERO(pMixBuf->pSamples + offClear, cLeft * sizeof(PDMAUDIOSAMPLE));
    }
}

void audioMixBufDestroy(PPDMAUDIOMIXBUF pMixBuf)
{
    if (!pMixBuf)
        return;

    audioMixBufUnlink(pMixBuf);

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

    audioMixBufFreeBuf(pMixBuf);
}

/** @todo Rename this function! Too confusing in combination with audioMixBufFreeBuf(). */
uint32_t audioMixBufFree(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    uint32_t cFree;
    if (pMixBuf->pParent)
    {
        /*
         * As a linked child buffer we want to know how many samples
         * already have been consumed by the parent.
         */
        Assert(pMixBuf->cMixed <= pMixBuf->pParent->cSamples);
        cFree = pMixBuf->pParent->cSamples - pMixBuf->cMixed;
    }
    else
        cFree = pMixBuf->cSamples - pMixBuf->cProcessed;

    AUDMIXBUF_LOG(("%s: cFree=%RU32\n", pMixBuf->pszName, cFree));
    return cFree;
}

size_t audioMixBufFreeBytes(PPDMAUDIOMIXBUF pMixBuf)
{
    return AUDIOMIXBUF_S2B(pMixBuf, audioMixBufFree(pMixBuf));
}

static int audioMixBufAllocBuf(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamples)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertReturn(cSamples, VERR_INVALID_PARAMETER);

    AUDMIXBUF_LOG(("%s: cSamples=%RU32\n", pMixBuf->pszName, cSamples));

    size_t cbSamples = cSamples * sizeof(PDMAUDIOSAMPLE);
    if (!cbSamples)
        return VERR_INVALID_PARAMETER;

    pMixBuf->pSamples = (PPDMAUDIOSAMPLE)RTMemAllocZ(cbSamples);
    if (!pMixBuf->pSamples)
        return VERR_NO_MEMORY;

    pMixBuf->cSamples = cSamples;

    return VINF_SUCCESS;
}

/** Note: Enabling this will generate huge logs! */
//#define DEBUG_MACROS

#ifdef DEBUG_MACROS
# define AUDMIXBUF_MACRO_LOG(x) AUDMIXBUF_LOG(x)
#elif defined(TESTCASE)
# define AUDMIXBUF_MACRO_LOG(x) RTPrintf x
#else
# define AUDMIXBUF_MACRO_LOG(x) do {} while (0)
#endif

/**
 * Macro for generating the conversion routines from/to different formats.
 * Be careful what to pass in/out, as most of the macros are optimized for speed and
 * thus don't do any bounds checking!
 *
 * Note: Currently does not handle any endianess conversion yet!
 */
#define AUDMIXBUF_CONVERT(_aName, _aType, _aMin, _aMax, _aSigned, _aShift) \
    /* Clips a specific output value to a single sample value. */ \
    AUDMIXBUF_MACRO_FN int64_t audioMixBufClipFrom##_aName(_aType aVal) \
    { \
        if (_aSigned) \
            return ((int64_t) aVal) << (32 - _aShift); \
        return ((int64_t) aVal - (_aMax >> 1)) << (32 - _aShift); \
    } \
    \
    /* Clips a single sample value to a specific output value. */ \
    AUDMIXBUF_MACRO_FN _aType audioMixBufClipTo##_aName(int64_t iVal) \
    { \
        if (iVal >= 0x7f000000) \
            return _aMax; \
        else if (iVal < -2147483648LL) \
            return _aMin; \
        \
        if (_aSigned) \
            return (_aType) (iVal >> (32 - _aShift)); \
        return ((_aType) ((iVal >> (32 - _aShift)) + (_aMax >> 1))); \
    } \
    \
    AUDMIXBUF_MACRO_FN uint32_t audioMixBufConvFrom##_aName##Stereo(PPDMAUDIOSAMPLE paDst, const void *pvSrc, size_t cbSrc, \
                                                                    const PAUDMIXBUF_CONVOPTS pOpts) \
    { \
        _aType *pSrc = (_aType *)pvSrc; \
        uint32_t cSamples = (uint32_t)RT_MIN(pOpts->cSamples, cbSrc / sizeof(_aType)); \
        AUDMIXBUF_MACRO_LOG(("cSamples=%RU32, sizeof(%zu), lVol=%RU32, rVol=%RU32\n", \
                             pOpts->cSamples, sizeof(_aType), pOpts->Volume.uLeft, pOpts->Volume.uRight)); \
        for (uint32_t i = 0; i < cSamples; i++) \
        { \
            AUDMIXBUF_MACRO_LOG(("%p: l=%RI16, r=%RI16\n", paDst, *pSrc, *(pSrc + 1))); \
            paDst->u64LSample = (ASMMult2xS32RetS64(audioMixBufClipFrom##_aName(*pSrc++), pOpts->Volume.uLeft ) >> 31); \
            paDst->u64RSample = (ASMMult2xS32RetS64(audioMixBufClipFrom##_aName(*pSrc++), pOpts->Volume.uRight) >> 31); \
            AUDMIXBUF_MACRO_LOG(("\t-> l=%RI64, r=%RI64\n", paDst->u64LSample, paDst->u64RSample)); \
            paDst++; \
        } \
        \
        return cSamples; \
    } \
    \
    AUDMIXBUF_MACRO_FN uint32_t audioMixBufConvFrom##_aName##Mono(PPDMAUDIOSAMPLE paDst, const void *pvSrc, size_t cbSrc, \
                                                                  const PAUDMIXBUF_CONVOPTS pOpts) \
    { \
        _aType *pSrc = (_aType *)pvSrc; \
        uint32_t cSamples = (uint32_t)RT_MIN(pOpts->cSamples, cbSrc / sizeof(_aType)); \
        AUDMIXBUF_MACRO_LOG(("cSamples=%RU32, sizeof(%zu)\n", cSamples, sizeof(_aType))); \
        for (uint32_t i = 0; i < cSamples; i++) \
        { \
            paDst->u64LSample = ASMMult2xS32RetS64(audioMixBufClipFrom##_aName(pSrc[0]), pOpts->Volume.uLeft); \
            paDst->u64RSample = paDst->u64LSample; \
            pSrc++; \
            paDst++; \
        } \
        \
        return cSamples; \
    } \
    \
    AUDMIXBUF_MACRO_FN void audioMixBufConvTo##_aName##Stereo(void *pvDst, const PPDMAUDIOSAMPLE paSrc, \
                                                              const PAUDMIXBUF_CONVOPTS pOpts) \
    { \
        PPDMAUDIOSAMPLE pSrc = paSrc; \
        _aType *pDst = (_aType *)pvDst; \
        _aType l, r; \
        uint32_t cSamples = pOpts->cSamples; \
        while (cSamples--) \
        { \
            AUDMIXBUF_MACRO_LOG(("%p: l=%RI64, r=%RI64\n", pSrc, pSrc->u64LSample, pSrc->u64RSample)); \
            l = audioMixBufClipTo##_aName(pSrc->u64LSample); \
            r = audioMixBufClipTo##_aName(pSrc->u64RSample); \
            AUDMIXBUF_MACRO_LOG(("\t-> l=%RI16, r=%RI16\n", l, r)); \
            *pDst++ = l; \
            *pDst++ = r; \
            pSrc++; \
        } \
    } \
    \
    AUDMIXBUF_MACRO_FN void audioMixBufConvTo##_aName##Mono(void *pvDst, const PPDMAUDIOSAMPLE paSrc, \
                                                            const PAUDMIXBUF_CONVOPTS pOpts) \
    { \
        PPDMAUDIOSAMPLE pSrc = paSrc; \
        _aType *pDst = (_aType *)pvDst; \
        uint32_t cSamples = pOpts->cSamples; \
        while (cSamples--) \
        { \
            *pDst++ = audioMixBufClipTo##_aName(pSrc->u64LSample + pSrc->u64RSample); \
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
    AUDMIXBUF_MACRO_FN void audioMixBufOp##_aName(PPDMAUDIOSAMPLE paDst, uint32_t cDstSamples, \
                                                  PPDMAUDIOSAMPLE paSrc, uint32_t cSrcSamples, \
                                                  PPDMAUDIOSTRMRATE pRate, \
                                                  uint32_t *pcDstWritten, uint32_t *pcSrcRead) \
    { \
        AUDMIXBUF_MACRO_LOG(("cSrcSamples=%RU32, cDstSamples=%RU32\n", cSrcSamples, cDstSamples)); \
        AUDMIXBUF_MACRO_LOG(("pRate=%p: srcOffset=0x%x (%RU32), dstOffset=0x%x (%RU32), dstInc=0x%x (%RU64)\n", \
                             pRate, pRate->srcOffset, pRate->srcOffset, pRate->dstOffset >> 32, pRate->dstOffset >> 32, \
                             pRate->dstInc, pRate->dstInc)); \
        \
        if (pRate->dstInc == (UINT64_C(1) + UINT32_MAX)) /* No conversion needed? */ \
        { \
            uint32_t cSamples = RT_MIN(cSrcSamples, cDstSamples); \
            AUDMIXBUF_MACRO_LOG(("cSamples=%RU32\n", cSamples)); \
            for (uint32_t i = 0; i < cSamples; i++) \
            { \
                paDst[i].u64LSample _aOp paSrc[i].u64LSample; \
                paDst[i].u64RSample _aOp paSrc[i].u64RSample; \
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
        PDMAUDIOSAMPLE  samCur = { 0 }; \
        PDMAUDIOSAMPLE  samOut; \
        PDMAUDIOSAMPLE  samLast    = pRate->srcSampleLast; \
        uint64_t        lDelta; \
        \
        AUDMIXBUF_MACRO_LOG(("Start: paDstEnd=%p - paDstStart=%p -> %zu\n", paDstEnd, paDst, paDstEnd - paDstStart)); \
        AUDMIXBUF_MACRO_LOG(("Start: paSrcEnd=%p - paSrcStart=%p -> %zu\n", paSrcEnd, paSrc, paSrcEnd - paSrcStart)); \
        \
        while (paDst < paDstEnd) \
        { \
            Assert(paSrc <= paSrcEnd); \
            Assert(paDst <= paDstEnd); \
            if (paSrc == paSrcEnd) \
                break; \
            \
            lDelta = 0; \
            while (pRate->srcOffset <= (pRate->dstOffset >> 32)) \
            { \
                Assert(paSrc <= paSrcEnd); \
                samLast = *paSrc++; \
                pRate->srcOffset++; \
                lDelta++; \
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
            samOut.u64LSample = (samLast.u64LSample * ((int64_t) UINT32_MAX - iDstOffInt) + samCur.u64LSample * iDstOffInt) >> 32; \
            samOut.u64RSample = (samLast.u64RSample * ((int64_t) UINT32_MAX - iDstOffInt) + samCur.u64RSample * iDstOffInt) >> 32; \
            \
            paDst->u64LSample _aOp samOut.u64LSample; \
            paDst->u64RSample _aOp samOut.u64RSample; \
            paDst++; \
            \
            AUDMIXBUF_MACRO_LOG(("\tlDelta=0x%x (%RU64), iDstOffInt=0x%x (%RI64), l=%RI64, r=%RI64 (cur l=%RI64, r=%RI64)\n", \
                                 lDelta, lDelta, iDstOffInt, iDstOffInt, \
                                 paDst->u64LSample, paDst->u64RSample, \
                                 samCur.u64LSample, samCur.u64RSample)); \
            \
            pRate->dstOffset += pRate->dstInc; \
            \
            AUDMIXBUF_MACRO_LOG(("\t\tpRate->dstOffset=0x%x (%RU32)\n", pRate->dstOffset, pRate->dstOffset >> 32)); \
            \
        } \
        \
        AUDMIXBUF_MACRO_LOG(("End: paDst=%p - paDstStart=%p -> %zu\n", paDst, paDstStart, paDst - paDstStart)); \
        AUDMIXBUF_MACRO_LOG(("End: paSrc=%p - paSrcStart=%p -> %zu\n", paSrc, paSrcStart, paSrc - paSrcStart)); \
        \
        pRate->srcSampleLast = samLast; \
        \
        AUDMIXBUF_MACRO_LOG(("pRate->srcSampleLast l=%RI64, r=%RI64, lDelta=0x%x (%RU64)\n", \
                              pRate->srcSampleLast.u64LSample, pRate->srcSampleLast.u64RSample, lDelta, lDelta)); \
        \
        if (pcDstWritten) \
            *pcDstWritten = paDst - paDstStart; \
        if (pcSrcRead) \
            *pcSrcRead = paSrc - paSrcStart; \
    }

/* audioMixBufOpAssign: Assigns values from source buffer to destination bufffer, overwriting the destination. */
AUDMIXBUF_MIXOP(Assign /* Name */,  = /* Operation */)
/* audioMixBufOpBlend: Blends together the values from both, the source and the destination buffer. */
AUDMIXBUF_MIXOP(Blend  /* Name */, += /* Operation */)

#undef AUDMIXBUF_MIXOP
#undef AUDMIXBUF_MACRO_LOG

/**
 *
 ** @todo Speed up the lookup by binding it to the actual stream state.
 *
 * @return  IPRT status code.
 * @return  PAUDMIXBUF_FN_CONVFROM
 * @param   enmFmt
 */
static inline PAUDMIXBUF_FN_CONVFROM audioMixBufConvFromLookup(PDMAUDIOMIXBUFFMT enmFmt)
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
        else if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 1)
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
        else if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 1)
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

    return NULL;
}

/**
 *
 ** @todo Speed up the lookup by binding it to the actual stream state.
 *
 * @return  PAUDMIXBUF_FN_CONVTO
 * @param   enmFmt
 */
static inline PAUDMIXBUF_FN_CONVTO audioMixBufConvToLookup(PDMAUDIOMIXBUFFMT enmFmt)
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
        else if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 1)
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
        else if (AUDMIXBUF_FMT_CHANNELS(enmFmt) == 1)
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

    return NULL;
}

static void audioMixBufFreeBuf(PPDMAUDIOMIXBUF pMixBuf)
{
    if (pMixBuf)
    {
        if (pMixBuf->pSamples)
        {
            RTMemFree(pMixBuf->pSamples);
            pMixBuf->pSamples = NULL;
        }

        pMixBuf->cSamples = 0;
    }
}

int audioMixBufInit(PPDMAUDIOMIXBUF pMixBuf, const char *pszName, PPDMPCMPROPS pProps, uint32_t cSamples)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pProps,  VERR_INVALID_POINTER);

    pMixBuf->pParent = NULL;
    RTListInit(&pMixBuf->lstBuffers);

    pMixBuf->pSamples = NULL;
    pMixBuf->cSamples = 0;

    pMixBuf->offReadWrite = 0;
    pMixBuf->cMixed       = 0;
    pMixBuf->cProcessed   = 0;

    /* Set initial volume to max. */
    pMixBuf->Volume.fMuted = false;
    pMixBuf->Volume.uLeft  = 0x7F;
    pMixBuf->Volume.uRight = 0x7F;

    /* Prevent division by zero.
     * Do a 1:1 conversion according to AUDIOMIXBUF_S2B_RATIO. */
    pMixBuf->iFreqRatio = 1 << 20;

    pMixBuf->pRate = NULL;

    pMixBuf->AudioFmt = AUDMIXBUF_AUDIO_FMT_MAKE(pProps->uHz,
                                                 pProps->cChannels,
                                                 pProps->cBits,
                                                 pProps->fSigned);
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

    return audioMixBufAllocBuf(pMixBuf, cSamples);
}

bool audioMixBufIsEmpty(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, true);

    if (pMixBuf->pParent)
        return (pMixBuf->cMixed == 0);
    return (pMixBuf->cProcessed == 0);
}

int audioMixBufLinkTo(PPDMAUDIOMIXBUF pMixBuf, PPDMAUDIOMIXBUF pParent)
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
        AUDMIXBUF_LOG(("%s: Already linked to \"%s\"\n",
                       pMixBuf->pszName, pMixBuf->pParent->pszName));
        return VERR_ACCESS_DENIED;
    }

    RTListAppend(&pParent->lstBuffers, &pMixBuf->Node);
    pMixBuf->pParent = pParent;

    /* Calculate the frequency ratio. */
    pMixBuf->iFreqRatio = ((int64_t)AUDMIXBUF_FMT_SAMPLE_FREQ(pParent->AudioFmt) << 32)
                        /           AUDMIXBUF_FMT_SAMPLE_FREQ(pMixBuf->AudioFmt);

    if (pMixBuf->iFreqRatio == 0) /* Catch division by zero. */
        pMixBuf->iFreqRatio = 1 << 20; /* Do a 1:1 conversion instead. */

    uint32_t cSamples = RT_MIN(  ((uint64_t)pParent->cSamples << 32)
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

        AUDMIXBUF_LOG(("uThisHz=%RU32, uParentHz=%RU32, iFreqRatio=0x%x (%RI64), uRateInc=0x%x (%RU64), cSamples=%RU32 (%RU32 parent)\n",
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

uint32_t audioMixBufMixed(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    AssertMsgReturn(VALID_PTR(pMixBuf->pParent),
                              ("Buffer is not linked to a parent buffer\n"),
                              0);

    AUDMIXBUF_LOG(("%s: cMixed=%RU32\n", pMixBuf->pszName, pMixBuf->cMixed));
    return pMixBuf->cMixed;
}

static int audioMixBufMixTo(PPDMAUDIOMIXBUF pDst, PPDMAUDIOMIXBUF pSrc, uint32_t cSamples, uint32_t *pcProcessed)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);
    /* pcProcessed is optional. */

    /* Live samples indicate how many samples there are in the source buffer
     * which have not been processed yet by the destination buffer. */
    uint32_t cLive = pSrc->cMixed;
    if (cLive >= pDst->cSamples)
        AUDMIXBUF_LOG(("Destination buffer \"%s\" full (%RU32 samples max), live samples = %RU32\n",
                       pDst->pszName, pDst->cSamples, cLive));

    /* Dead samples are the number of samples in the destination buffer which
     * will not be needed, that is, are not needed in order to process the live
     * samples of the source buffer. */
    uint32_t cDead = pDst->cSamples - cLive;

    uint32_t cToReadTotal = RT_MIN(cSamples, AUDIOMIXBUF_S2S_RATIO(pSrc, cDead));
    uint32_t offRead = 0;

    uint32_t cReadTotal = 0;
    uint32_t cWrittenTotal = 0;
    uint32_t offWrite = (pDst->offReadWrite + cLive) % pDst->cSamples;

    AUDMIXBUF_LOG(("pSrc=%s (%RU32 samples), pDst=%s (%RU32 samples), cLive=%RU32, cDead=%RU32, cToReadTotal=%RU32, offWrite=%RU32\n",
                   pSrc->pszName, pSrc->cSamples, pDst->pszName, pDst->cSamples, cLive, cDead, cToReadTotal, offWrite));

    uint32_t cToRead, cToWrite;
    uint32_t cWritten, cRead;

    while (cToReadTotal)
    {
        cDead = pDst->cSamples - cLive;

        cToRead  = cToReadTotal;
        cToWrite = RT_MIN(cDead, pDst->cSamples - offWrite);
        if (!cToWrite)
        {
            AUDMIXBUF_LOG(("Warning: Destination buffer \"%s\" full\n", pDst->pszName));
            break;
        }

        Assert(offWrite + cToWrite <= pDst->cSamples);
        Assert(offRead + cToRead <= pSrc->cSamples);

        AUDMIXBUF_LOG(("\t%RU32Hz -> %RU32Hz\n", AUDMIXBUF_FMT_SAMPLE_FREQ(pSrc->AudioFmt), AUDMIXBUF_FMT_SAMPLE_FREQ(pDst->AudioFmt)));
        AUDMIXBUF_LOG(("\tcDead=%RU32, offWrite=%RU32, cToWrite=%RU32, offRead=%RU32, cToRead=%RU32\n",
                       cDead, offWrite, cToWrite, offRead, cToRead));

        audioMixBufOpBlend(pDst->pSamples + offWrite, cToWrite,
                           pSrc->pSamples + offRead, cToRead,
                           pSrc->pRate, &cWritten, &cRead);

        AUDMIXBUF_LOG(("\t\tcWritten=%RU32, cRead=%RU32\n", cWritten, cRead));

        cReadTotal    += cRead;
        cWrittenTotal += cWritten;

        offRead += cRead;
        Assert(cToReadTotal >= cRead);
        cToReadTotal -= cRead;

        offWrite = (offWrite + cWritten) % pDst->cSamples;

        cLive += cWritten;
    }

    pSrc->cMixed     += cWrittenTotal;
    pDst->cProcessed += cWrittenTotal;
#ifdef DEBUG
    s_cSamplesMixedTotal += cWrittenTotal;
    audioMixBufPrint(pDst);
#endif

    if (pcProcessed)
        *pcProcessed = cReadTotal;

    AUDMIXBUF_LOG(("cReadTotal=%RU32 (pcProcessed), cWrittenTotal=%RU32, cSrcMixed=%RU32, cDstProc=%RU32\n",
                   cReadTotal, cWrittenTotal, pSrc->cMixed, pDst->cProcessed));
    return VINF_SUCCESS;
}

int audioMixBufMixToChildren(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamples,
                             uint32_t *pcProcessed)
{
    int rc = VINF_SUCCESS;

    uint32_t cProcessed;
    uint32_t cProcessedMax = 0;

    PPDMAUDIOMIXBUF pIter;
    RTListForEach(&pMixBuf->lstBuffers, pIter, PDMAUDIOMIXBUF, Node)
    {
        rc = audioMixBufMixTo(pIter, pMixBuf, cSamples, &cProcessed);
        if (RT_FAILURE(rc))
            break;

        cProcessedMax = RT_MAX(cProcessedMax, cProcessed);
    }

    if (pcProcessed)
        *pcProcessed = cProcessedMax;

    return rc;
}

int audioMixBufMixToParent(PPDMAUDIOMIXBUF pMixBuf, uint32_t cSamples,
                           uint32_t *pcProcessed)
{
    AssertMsgReturn(VALID_PTR(pMixBuf->pParent),
                    ("Buffer is not linked to a parent buffer\n"),
                    VERR_INVALID_PARAMETER);

    return audioMixBufMixTo(pMixBuf->pParent, pMixBuf, cSamples, pcProcessed);
}

static inline void audioMixBufPrint(PPDMAUDIOMIXBUF pMixBuf)
{
#ifdef DEBUG_DISABLED
    PPDMAUDIOMIXBUF pParent = pMixBuf;
    if (pMixBuf->pParent)
        pParent = pMixBuf->pParent;

    AUDMIXBUF_LOG(("********************************************\n"));
    AUDMIXBUF_LOG(("%s: offReadWrite=%RU32, cProcessed=%RU32, cMixed=%RU32 (BpS=%RU32)\n",
                   pParent->pszName,
                   pParent->offReadWrite, pParent->cProcessed, pParent->cMixed,
                   AUDIOMIXBUF_S2B(pParent, 1)));

    PPDMAUDIOMIXBUF pIter;
    RTListForEach(&pParent->lstBuffers, pIter, PDMAUDIOMIXBUF, Node)
    {
        AUDMIXBUF_LOG(("\t%s: offReadWrite=%RU32, cProcessed=%RU32, cMixed=%RU32 (BpS=%RU32)\n",
                       pIter->pszName,
                       pIter->offReadWrite, pIter->cProcessed, pIter->cMixed,
                       AUDIOMIXBUF_S2B(pIter, 1)));
    }
    AUDMIXBUF_LOG(("Total samples mixed: %RU64\n", s_cSamplesMixedTotal));
    AUDMIXBUF_LOG(("********************************************\n"));
#endif
}

/**
 * Returns the total number of samples processed.
 *
 * @return  uint32_t
 * @param   pMixBuf
 */
uint32_t audioMixBufProcessed(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturn(pMixBuf, 0);

    AUDMIXBUF_LOG(("%s: cProcessed=%RU32\n", pMixBuf->pszName, pMixBuf->cProcessed));
    return pMixBuf->cProcessed;
}

int audioMixBufReadAt(PPDMAUDIOMIXBUF pMixBuf,
                      uint32_t offSamples,
                      void *pvBuf, size_t cbBuf,
                      uint32_t *pcbRead)
{
    return audioMixBufReadAtEx(pMixBuf, pMixBuf->AudioFmt,
                               offSamples, pvBuf, cbBuf, pcbRead);
}

int audioMixBufReadAtEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                        uint32_t offSamples,
                        void *pvBuf, size_t cbBuf,
                        uint32_t *pcbRead)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    uint32_t cDstSamples = pMixBuf->cSamples;
    uint32_t cLive = pMixBuf->cProcessed;

    uint32_t cDead = cDstSamples - cLive;
    uint32_t cToProcess = AUDIOMIXBUF_S2S_RATIO(pMixBuf, cDead);
    cToProcess = RT_MIN(cToProcess, AUDIOMIXBUF_B2S(pMixBuf, cbBuf));

    AUDMIXBUF_LOG(("%s: offSamples=%RU32, cLive=%RU32, cDead=%RU32, cToProcess=%RU32\n",
                   pMixBuf->pszName, offSamples, cLive, cDead, cToProcess));

    int rc;
    if (cToProcess)
    {
        PAUDMIXBUF_FN_CONVTO pConv = audioMixBufConvToLookup(enmFmt);
        if (pConv)
        {
            AUDMIXBUF_CONVOPTS convOpts = { cToProcess, pMixBuf->Volume };
            pConv(pvBuf, pMixBuf->pSamples + offSamples, &convOpts);

            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_INVALID_PARAMETER;

        audioMixBufPrint(pMixBuf);
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

int audioMixBufReadCirc(PPDMAUDIOMIXBUF pMixBuf,
                        void *pvBuf, size_t cbBuf, uint32_t *pcRead)
{
    return audioMixBufReadCircEx(pMixBuf, pMixBuf->AudioFmt,
                                 pvBuf, cbBuf, pcRead);
}

int audioMixBufReadCircEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                          void *pvBuf, size_t cbBuf, uint32_t *pcRead)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    if (!cbBuf)
        return VINF_SUCCESS;

    uint32_t cToRead = RT_MIN(AUDIOMIXBUF_B2S(pMixBuf, cbBuf), pMixBuf->cProcessed);

    AUDMIXBUF_LOG(("%s: pvBuf=%p, cbBuf=%zu (%RU32 samples), cToRead=%RU32\n",
                   pMixBuf->pszName, pvBuf, cbBuf, AUDIOMIXBUF_B2S(pMixBuf, cbBuf), cToRead));

    if (!cToRead)
    {
        audioMixBufPrint(pMixBuf);

        if (pcRead)
            *pcRead = 0;
        return VINF_SUCCESS;
    }

    PAUDMIXBUF_FN_CONVTO pConv = audioMixBufConvToLookup(enmFmt);
    if (!pConv) /* Audio format not supported. */
        return VERR_NOT_SUPPORTED;

    PPDMAUDIOSAMPLE pSamplesSrc1 = pMixBuf->pSamples + pMixBuf->offReadWrite;
    size_t cLenSrc1 = cToRead;

    PPDMAUDIOSAMPLE pSamplesSrc2 = NULL;
    size_t cLenSrc2 = 0;

    uint32_t offRead = pMixBuf->offReadWrite + cToRead;

    /*
     * Do we need to wrap around to read all requested data, that is,
     * starting at the beginning of our circular buffer? This then will
     * be the optional second part to do.
     */
    if (offRead >= pMixBuf->cSamples)
    {
        Assert(pMixBuf->offReadWrite <= pMixBuf->cSamples);
        cLenSrc1 = pMixBuf->cSamples - pMixBuf->offReadWrite;

        pSamplesSrc2 = pMixBuf->pSamples;
        Assert(cToRead >= cLenSrc1);
        cLenSrc2 = RT_MIN(cToRead - cLenSrc1, pMixBuf->cSamples);

        /* Save new read offset. */
        offRead = cLenSrc2;
    }

    AUDMIXBUF_CONVOPTS convOpts;
    convOpts.Volume = pMixBuf->Volume;

    /* Anything to do at all? */
    int rc = VINF_SUCCESS;
    if (cLenSrc1)
    {
        convOpts.cSamples = cLenSrc1;

        AUDMIXBUF_LOG(("P1: offRead=%RU32, cToRead=%RU32\n", pMixBuf->offReadWrite, cLenSrc1));
        pConv(pvBuf, pSamplesSrc1, &convOpts);
    }

    /* Second part present? */
    if (   RT_LIKELY(RT_SUCCESS(rc))
        && cLenSrc2)
    {
        AssertPtr(pSamplesSrc2);

        convOpts.cSamples = cLenSrc2;

        AUDMIXBUF_LOG(("P2: cToRead=%RU32, offWrite=%RU32 (%zu bytes)\n", cLenSrc2, cLenSrc1,
                       AUDIOMIXBUF_S2B(pMixBuf, cLenSrc1)));
        pConv((uint8_t *)pvBuf + AUDIOMIXBUF_S2B(pMixBuf, cLenSrc1), pSamplesSrc2, &convOpts);
    }

    if (RT_SUCCESS(rc))
    {
#ifdef DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        rc = RTFileOpen(&fh, "c:\\temp\\mixbuf_readcirc.pcm",
                        RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            RTFileWrite(fh, pvBuf, AUDIOMIXBUF_S2B(pMixBuf, cLenSrc1 + cLenSrc2), NULL);
            RTFileClose(fh);
        }
#endif
        pMixBuf->offReadWrite  = offRead % pMixBuf->cSamples;
        pMixBuf->cProcessed   -= RT_MIN(cLenSrc1 + cLenSrc2, pMixBuf->cProcessed);

        if (pcRead)
            *pcRead = cLenSrc1 + cLenSrc2;
    }

    audioMixBufPrint(pMixBuf);

    AUDMIXBUF_LOG(("cRead=%RU32 (%zu bytes), rc=%Rrc\n",
                   cLenSrc1 + cLenSrc2,
                   AUDIOMIXBUF_S2B(pMixBuf, cLenSrc1 + cLenSrc2), rc));
    return rc;
}

void audioMixBufReset(PPDMAUDIOMIXBUF pMixBuf)
{
    AssertPtrReturnVoid(pMixBuf);

    AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

    pMixBuf->offReadWrite = 0;
    pMixBuf->cMixed       = 0;
    pMixBuf->cProcessed   = 0;

    if (pMixBuf->cSamples)
        RT_BZERO(pMixBuf->pSamples, pMixBuf->cSamples * sizeof(PDMAUDIOSAMPLE));
}

void audioMixBufSetVolume(PPDMAUDIOMIXBUF pMixBuf, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturnVoid(pMixBuf);
    AssertPtrReturnVoid(pVol);

    LogFlowFunc(("%s: lVol=%RU32, rVol=%RU32\n", pMixBuf->pszName, pVol->uLeft, pVol->uRight));

    pMixBuf->Volume.fMuted = pVol->fMuted;
    pMixBuf->Volume.uLeft  = (UINT64_C(0x100000000) * pVol->uLeft) / 255;
    pMixBuf->Volume.uRight = (UINT64_C(0x100000000) * pVol->uRight) / 255;

    LogFlowFunc(("\t-> lVol=%RU32, rVol=%RU32\n", pMixBuf->Volume.uLeft, pMixBuf->Volume.uRight));
}

uint32_t audioMixBufSize(PPDMAUDIOMIXBUF pMixBuf)
{
    return pMixBuf->cSamples;
}

/**
 * Returns the maximum amount of bytes this buffer can hold.
 *
 * @return  uint32_t
 * @param   pMixBuf
 */
size_t audioMixBufSizeBytes(PPDMAUDIOMIXBUF pMixBuf)
{
    return AUDIOMIXBUF_S2B(pMixBuf, pMixBuf->cSamples);
}

void audioMixBufUnlink(PPDMAUDIOMIXBUF pMixBuf)
{
    if (!pMixBuf || !pMixBuf->pszName)
        return;

    AUDMIXBUF_LOG(("%s\n", pMixBuf->pszName));

    if (pMixBuf->pParent)
    {
        AUDMIXBUF_LOG(("%s: Unlinking from parent \"%s\"\n",
                       pMixBuf->pszName, pMixBuf->pParent->pszName));

        RTListNodeRemove(&pMixBuf->Node);

        /* Make sure to reset the parent mixing buffer each time it gets linked
         * to a new child. */
        audioMixBufReset(pMixBuf->pParent);
        pMixBuf->pParent = NULL;
    }

    PPDMAUDIOMIXBUF pIter;
    while (!RTListIsEmpty(&pMixBuf->lstBuffers))
    {
        pIter = RTListGetFirst(&pMixBuf->lstBuffers, PDMAUDIOMIXBUF, Node);

        AUDMIXBUF_LOG(("\tUnlinking \"%s\"\n", pIter->pszName));

        audioMixBufReset(pIter->pParent);
        pIter->pParent = NULL;

        RTListNodeRemove(&pIter->Node);
    }

    if (pMixBuf->pRate)
    {
        pMixBuf->pRate->dstOffset = pMixBuf->pRate->srcOffset = 0;
        pMixBuf->pRate->dstInc = 0;
    }

    pMixBuf->iFreqRatio = 1; /* Prevent division by zero. */
}

int audioMixBufWriteAt(PPDMAUDIOMIXBUF pMixBuf,
                       uint32_t offSamples,
                       const void *pvBuf, size_t cbBuf,
                       uint32_t *pcWritten)
{
    return audioMixBufWriteAtEx(pMixBuf, pMixBuf->AudioFmt,
                                offSamples, pvBuf, cbBuf, pcWritten);
}

/**
 * TODO
 *
 * @return  IPRT status code.
 * @return  int
 * @param   pMixBuf
 * @param   enmFmt
 * @param   offSamples
 * @param   pvBuf
 * @param   cbBuf
 * @param   pcWritten           Returns number of samples written. Optional.
 */
int audioMixBufWriteAtEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                         uint32_t offSamples,
                         const void *pvBuf, size_t cbBuf,
                         uint32_t *pcWritten)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    /* pcWritten is optional. */

    uint32_t cDstSamples = pMixBuf->pParent
                         ? pMixBuf->pParent->cSamples : pMixBuf->cSamples;
    uint32_t cLive = pMixBuf->cProcessed;

    uint32_t cDead = cDstSamples - cLive;
    uint32_t cToProcess = AUDIOMIXBUF_S2S_RATIO(pMixBuf, cDead);
    cToProcess = RT_MIN(cToProcess, AUDIOMIXBUF_B2S(pMixBuf, cbBuf));

    AUDMIXBUF_LOG(("%s: offSamples=%RU32, cLive=%RU32, cDead=%RU32, cToProcess=%RU32\n",
                   pMixBuf->pszName, offSamples, cLive, cDead, cToProcess));

    if (offSamples + cToProcess > pMixBuf->cSamples)
        return VERR_BUFFER_OVERFLOW;

    PAUDMIXBUF_FN_CONVFROM pConv = audioMixBufConvFromLookup(enmFmt);
    if (!pConv)
        return VERR_NOT_SUPPORTED;

    int rc;
    uint32_t cWritten;

#ifdef DEBUG_DUMP_PCM_DATA
    RTFILE fh;
    rc = RTFileOpen(&fh, "c:\\temp\\mixbuf_writeat.pcm",
                    RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        RTFileWrite(fh, pvBuf, cbBuf, NULL);
        RTFileClose(fh);
    }
#endif

    if (cToProcess)
    {
        AUDMIXBUF_CONVOPTS convOpts = { cToProcess, pMixBuf->Volume };

        cWritten = pConv(pMixBuf->pSamples + offSamples, pvBuf, cbBuf, &convOpts);
        audioMixBufPrint(pMixBuf);

        rc = cWritten ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Fudge! */
    }
    else
    {
        cWritten = 0;
        rc = VINF_SUCCESS;
    }

    if (RT_SUCCESS(rc))
    {
        if (pcWritten)
            *pcWritten = cWritten;
    }

    AUDMIXBUF_LOG(("cWritten=%RU32, rc=%Rrc\n", cWritten, rc));
    return rc;
}

int audioMixBufWriteCirc(PPDMAUDIOMIXBUF pMixBuf,
                         const void *pvBuf, size_t cbBuf,
                         uint32_t *pcWritten)
{
    return audioMixBufWriteCircEx(pMixBuf, pMixBuf->AudioFmt, pvBuf, cbBuf, pcWritten);
}

int audioMixBufWriteCircEx(PPDMAUDIOMIXBUF pMixBuf, PDMAUDIOMIXBUFFMT enmFmt,
                           const void *pvBuf, size_t cbBuf,
                           uint32_t *pcWritten)
{
    AssertPtrReturn(pMixBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    if (!cbBuf)
    {
        if (pcWritten)
            *pcWritten = 0;
        return VINF_SUCCESS;
    }

    PPDMAUDIOMIXBUF pParent = pMixBuf->pParent;

    AUDMIXBUF_LOG(("%s: enmFmt=%ld, pBuf=%p, cbBuf=%zu, pParent=%p (%RU32)\n",
                   pMixBuf->pszName, enmFmt, pvBuf, cbBuf, pParent, pParent ? pParent->cSamples : 0));

    if (   pParent
        && pParent->cSamples <= pMixBuf->cMixed)
    {
        if (pcWritten)
            *pcWritten = 0;

        AUDMIXBUF_LOG(("%s: Parent buffer %s is full\n",
                       pMixBuf->pszName, pMixBuf->pParent->pszName));

        return VINF_SUCCESS;
    }

    PAUDMIXBUF_FN_CONVFROM pConv = audioMixBufConvFromLookup(enmFmt);
    if (!pConv)
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;

    uint32_t cToWrite = AUDIOMIXBUF_B2S(pMixBuf, cbBuf);
    AssertMsg(cToWrite, ("cToWrite is 0 (cbBuf=%zu)\n", cbBuf));

    PPDMAUDIOSAMPLE pSamplesDst1 = pMixBuf->pSamples + pMixBuf->offReadWrite;
    size_t cLenDst1 = cToWrite;

    PPDMAUDIOSAMPLE pSamplesDst2 = NULL;
    size_t cLenDst2 = 0;

    uint32_t offWrite = pMixBuf->offReadWrite + cToWrite;

    /*
     * Do we need to wrap around to write all requested data, that is,
     * starting at the beginning of our circular buffer? This then will
     * be the optional second part to do.
     */
    if (offWrite >= pMixBuf->cSamples)
    {
        Assert(pMixBuf->offReadWrite <= pMixBuf->cSamples);
        cLenDst1 = pMixBuf->cSamples - pMixBuf->offReadWrite;

        pSamplesDst2 = pMixBuf->pSamples;
        Assert(cToWrite >= cLenDst1);
        cLenDst2 = RT_MIN(cToWrite - cLenDst1, pMixBuf->cSamples);

        /* Save new read offset. */
        offWrite = cLenDst2;
    }

    uint32_t cWrittenTotal = 0;

    AUDMIXBUF_CONVOPTS convOpts;
    convOpts.Volume = pMixBuf->Volume;

    /* Anything to do at all? */
    if (cLenDst1)
    {
        convOpts.cSamples = cLenDst1;
        cWrittenTotal = pConv(pSamplesDst1, pvBuf, cbBuf, &convOpts);
    }

    /* Second part present? */
    if (   RT_LIKELY(RT_SUCCESS(rc))
        && cLenDst2)
    {
        AssertPtr(pSamplesDst2);

        convOpts.cSamples = cLenDst2;
        cWrittenTotal += pConv(pSamplesDst2, (uint8_t *)pvBuf + AUDIOMIXBUF_S2B(pMixBuf, cLenDst1), cbBuf, &convOpts);
    }

#ifdef DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        RTFileOpen(&fh, "c:\\temp\\mixbuf_writeex.pcm",
                   RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        RTFileWrite(fh, pSamplesDst1, AUDIOMIXBUF_S2B(pMixBuf, cLenDst1), NULL);
        RTFileClose(fh);
#endif

    AUDMIXBUF_LOG(("cLenDst1=%RU32, cLenDst2=%RU32, offWrite=%RU32\n",
                   cLenDst1, cLenDst2, offWrite));

    if (RT_SUCCESS(rc))
    {
        pMixBuf->offReadWrite = offWrite % pMixBuf->cSamples;
        pMixBuf->cProcessed = RT_MIN(pMixBuf->cProcessed + cLenDst1 + cLenDst2,
                                     pMixBuf->cSamples /* Max */);
        if (pcWritten)
            *pcWritten = cLenDst1 + cLenDst2;
    }

    audioMixBufPrint(pMixBuf);

    AUDMIXBUF_LOG(("cWritten=%RU32 (%zu bytes), rc=%Rrc\n",
                   cLenDst1 + cLenDst2,
                   AUDIOMIXBUF_S2B(pMixBuf, cLenDst1 + cLenDst2), rc));
    return rc;
}
