/* $Id$ */
/** @file
 * Audio mixing buffer - Format conversion template.
 */

/*
 * Copyright (C) 2014-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#define AUDMIXBUF_CONVERT(a_Name, a_Type, a_Min, a_Max, a_fSigned, a_cShift) \
/* Clips a specific output value to a single sample value. */ \
DECLINLINE(int32_t) audioMixBufSampleFrom##a_Name(a_Type aVal) \
{ \
    /* left shifting of signed values is not defined, therefore the intermediate uint64_t cast */ \
    if (a_fSigned) \
        return (int32_t) (((uint32_t) ((int32_t) aVal                     )) << (32 - a_cShift)); \
    return     (int32_t) (((uint32_t) ((int32_t) aVal - ((a_Max >> 1) + 1))) << (32 - a_cShift)); \
} \
\
/* Clips a single sample value to a specific output value. */ \
DECLINLINE(a_Type) audioMixBufSampleTo##a_Name(int32_t iVal) \
{ \
    if (a_fSigned) \
        return (a_Type)  (iVal >> (32 - a_cShift)); \
    return     (a_Type) ((iVal >> (32 - a_cShift)) + ((a_Max >> 1) + 1)); \
} \
\
/* Encoders for peek: */ \
\
/* Generic */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncodeGeneric,a_Name)(void *pvDst, int32_t const *pi32Src, uint32_t cFrames, \
                                                                     PAUDIOMIXBUFPEEKSTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    uintptr_t const cSrcChannels = pState->cSrcChannels; \
    uintptr_t const cDstChannels = pState->cDstChannels; \
    a_Type         *pDst = (a_Type *)pvDst; \
    while (cFrames-- > 0) \
    { \
        uintptr_t idxDst = cDstChannels; \
        while (idxDst-- > 0) \
        { \
            int8_t idxSrc = pState->aidxChannelMap[idxDst]; \
            if (idxSrc >= 0) \
                pDst[idxDst] = audioMixBufSampleTo##a_Name(pi32Src[idxSrc]); \
            else if (idxSrc != -2) \
                pDst[idxDst] = (a_fSigned) ? 0 : (a_Max >> 1); \
            else \
                pDst[idxDst] = 0; \
        } \
        pDst    += cDstChannels; \
        pi32Src += cSrcChannels; \
    } \
} \
\
/* 2ch -> 2ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode2ChTo2Ch,a_Name)(void *pvDst, int32_t const *pi32Src, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFPEEKSTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type *pDst = (a_Type *)pvDst; \
    while (cFrames-- > 0) \
    { \
        pDst[0] = audioMixBufSampleTo##a_Name(pi32Src[0]); \
        pDst[1] = audioMixBufSampleTo##a_Name(pi32Src[1]); \
        AUDMIXBUF_MACRO_LOG(("%p: %RI32 / %RI32 => %RI32 / %RI32\n", \
                             &pi32Src[0], pi32Src[0], pi32Src[1], (int32_t)pDst[0], (int32_t)pDst[1])); \
        pDst    += 2; \
        pi32Src += 2; \
    } \
} \
\
/* 2ch -> 1ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode2ChTo1Ch,a_Name)(void *pvDst, int32_t const *pi32Src, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFPEEKSTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type *pDst = (a_Type *)pvDst; \
    while (cFrames-- > 0) \
    { \
         pDst[0] = audioMixBufSampleTo##a_Name(audioMixBufBlendSampleRet(pi32Src[0], pi32Src[1])); \
         pDst    += 1; \
         pi32Src += 2; \
    } \
} \
\
/* 1ch -> 2ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode1ChTo2Ch,a_Name)(void *pvDst, int32_t const *pi32Src, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFPEEKSTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type *pDst = (a_Type *)pvDst; \
    while (cFrames-- > 0) \
    { \
        pDst[0] = pDst[1] = audioMixBufSampleTo##a_Name(pi32Src[0]); \
        pDst    += 2; \
        pi32Src += 1; \
    } \
} \
/* 1ch -> 1ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufEncode1ChTo1Ch,a_Name)(void *pvDst, int32_t const *pi32Src, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFPEEKSTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type *pDst = (a_Type *)pvDst; \
    while (cFrames-- > 0) \
    { \
         pDst[0] = audioMixBufSampleTo##a_Name(pi32Src[0]); \
         pDst    += 1; \
         pi32Src += 1; \
    } \
} \
\
/* Decoders for write: */ \
\
/* Generic */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecodeGeneric,a_Name)(int32_t *pi32Dst, void const *pvSrc, uint32_t cFrames, \
                                                                     PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    uintptr_t const cSrcChannels = pState->cSrcChannels; \
    uintptr_t const cDstChannels = pState->cDstChannels; \
    a_Type const   *pSrc         = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        uintptr_t idxDst = cDstChannels; \
        while (idxDst-- > 0) \
        { \
            int8_t idxSrc = pState->aidxChannelMap[idxDst]; \
            if (idxSrc >= 0) \
                pi32Dst[idxDst] = audioMixBufSampleTo##a_Name(pSrc[idxSrc]); \
            else if (idxSrc != -2) \
                pi32Dst[idxDst] = (a_fSigned) ? 0 : (a_Max >> 1); \
            else \
                pi32Dst[idxDst] = 0; \
        } \
        pi32Dst += cDstChannels; \
        pSrc    += cSrcChannels; \
    } \
} \
\
/* 2ch -> 2ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode2ChTo2Ch,a_Name)(int32_t *pi32Dst, void const *pvSrc, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        pi32Dst[0] = audioMixBufSampleFrom##a_Name(pSrc[0]); \
        pi32Dst[1] = audioMixBufSampleFrom##a_Name(pSrc[1]); \
        AUDMIXBUF_MACRO_LOG(("%p: %RI32 / %RI32 => %RI32 / %RI32\n", \
                             &pSrc[0], (int32_t)pSrc[0], (int32_t)pSrc[1], pi32Dst[0], pi32Dst[1])); \
        pi32Dst  += 2; \
        pSrc     += 2; \
    } \
} \
\
/* 2ch -> 1ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode2ChTo1Ch,a_Name)(int32_t *pi32Dst, void const *pvSrc, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        pi32Dst[0] = audioMixBufBlendSampleRet(audioMixBufSampleFrom##a_Name(pSrc[0]), audioMixBufSampleFrom##a_Name(pSrc[1])); \
        pi32Dst  += 1; \
        pSrc     += 2; \
    } \
} \
\
/* 1ch -> 2ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode1ChTo2Ch,a_Name)(int32_t *pi32Dst, void const *pvSrc, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        pi32Dst[1] = pi32Dst[0] = audioMixBufSampleFrom##a_Name(pSrc[0]); \
        pi32Dst  += 2; \
        pSrc     += 1; \
    } \
} \
\
/* 1ch -> 1ch */ \
static DECLCALLBACK(void) RT_CONCAT(audioMixBufDecode1ChTo1Ch,a_Name)(int32_t *pi32Dst, void const *pvSrc, uint32_t cFrames, \
                                                                      PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        pi32Dst[0] = audioMixBufSampleFrom##a_Name(pSrc[0]); \
        pi32Dst  += 1; \
        pSrc     += 1; \
    } \
} \
\
/* Decoders for blending: */ \
\
/* Generic */ \
static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecodeGeneric,a_Name,Blend)(int32_t *pi32Dst, void const *pvSrc, \
                                                                            uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    uintptr_t const cSrcChannels = pState->cSrcChannels; \
    uintptr_t const cDstChannels = pState->cDstChannels; \
    a_Type const   *pSrc         = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        uintptr_t idxDst = cDstChannels; \
        while (idxDst-- > 0) \
        { \
            int8_t idxSrc = pState->aidxChannelMap[idxDst]; \
            if (idxSrc >= 0) \
                audioMixBufBlendSample(&pi32Dst[idxDst], audioMixBufSampleTo##a_Name(pSrc[idxSrc])); \
        } \
        pi32Dst += cDstChannels; \
        pSrc    += cSrcChannels; \
    } \
} \
\
/* 2ch -> 2ch */ \
static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode2ChTo2Ch,a_Name,Blend)(int32_t *pi32Dst, void const *pvSrc, \
                                                                             uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        audioMixBufBlendSample(&pi32Dst[0], audioMixBufSampleFrom##a_Name(pSrc[0])); \
        audioMixBufBlendSample(&pi32Dst[1], audioMixBufSampleFrom##a_Name(pSrc[1])); \
        AUDMIXBUF_MACRO_LOG(("%p: %RI32 / %RI32 => %RI32 / %RI32\n", \
                             &pSrc[0], (int32_t)pSrc[0], (int32_t)pSrc[1], pi32Dst[0], pi32Dst[1])); \
        pi32Dst  += 2; \
        pSrc     += 2; \
    } \
} \
\
/* 2ch -> 1ch */ \
static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode2ChTo1Ch,a_Name,Blend)(int32_t *pi32Dst, void const *pvSrc, \
                                                                             uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        audioMixBufBlendSample(&pi32Dst[0], audioMixBufBlendSampleRet(audioMixBufSampleFrom##a_Name(pSrc[0]), \
                                                                      audioMixBufSampleFrom##a_Name(pSrc[1]))); \
        pi32Dst  += 1; \
        pSrc     += 2; \
    } \
} \
\
/* 1ch -> 2ch */ \
static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode1ChTo2Ch,a_Name,Blend)(int32_t *pi32Dst, void const *pvSrc, \
                                                                             uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        int32_t const i32Src = audioMixBufSampleFrom##a_Name(pSrc[0]); \
        audioMixBufBlendSample(&pi32Dst[0], i32Src); \
        audioMixBufBlendSample(&pi32Dst[1], i32Src); \
        pi32Dst  += 2; \
        pSrc     += 1; \
    } \
} \
\
/* 1ch -> 1ch */ \
static DECLCALLBACK(void) RT_CONCAT3(audioMixBufDecode1ChTo1Ch,a_Name,Blend)(int32_t *pi32Dst, void const *pvSrc, \
                                                                             uint32_t cFrames, PAUDIOMIXBUFWRITESTATE pState) \
{ \
    RT_NOREF_PV(pState); \
    a_Type const *pSrc = (a_Type const *)pvSrc; \
    while (cFrames-- > 0) \
    { \
        audioMixBufBlendSample(&pi32Dst[0], audioMixBufSampleFrom##a_Name(pSrc[0])); \
        pi32Dst  += 1; \
        pSrc     += 1; \
    } \
}

