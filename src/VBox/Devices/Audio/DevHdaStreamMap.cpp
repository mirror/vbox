/* $Id$ */
/** @file
 * HDAStreamMap.cpp - Stream mapping functions for HD Audio.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include <iprt/mem.h>
#include <iprt/string.h>

#include "DrvAudioCommon.h"

#include "DevHdaStreamChannel.h"
#include "DevHdaStreamMap.h"



/** Convert host stereo to mono guest, signed 16-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_H2G_GenericS16_Stereo2Mono(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    /** @todo this doesn't merge the two, it just picks the first one. */
    uint16_t       *pu16Dst    = (uint16_t *)pvDst;
    uint16_t const *pu16Src    = (uint16_t const *)pvSrc;
    size_t const    cbDstFrame = pMap->cbGuestFrame;

    if (cbDstFrame != sizeof(*pu16Dst))
        RT_BZERO(pvDst, cbDstFrame * cFrames);

    while (cFrames-- > 0)
    {
        *pu16Dst = *pu16Src;
        pu16Src += 2;
        pu16Dst  = (uint16_t *)((uintptr_t)pu16Dst + cbDstFrame);
    }
}


/** Convert mono guest channel to host stereo, signed 16-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_G2H_GenericS16_Mono2Stereo(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    uint32_t       *pu32Dst    = (uint32_t *)pvDst;
    uint16_t const *pu16Src    = (uint16_t const *)pvSrc;
    size_t const    cbSrcFrame = pMap->cbGuestFrame;

    while (cFrames-- > 0)
    {
        uint16_t uSample = *pu16Src;
        *pu32Dst++ = RT_MAKE_U32(uSample, uSample);
        pu16Src = (uint16_t const *)((uintptr_t)pu16Src + cbSrcFrame);
    }
}


/** Convert host stereo to mono guest, signed 32-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_H2G_GenericS32_Stereo2Mono(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    /** @todo this doesn't merge the two, it just picks the first one. */
    uint32_t       *pu32Dst    = (uint32_t *)pvDst;
    uint32_t const *pu32Src    = (uint32_t const *)pvSrc;
    size_t const    cbDstFrame = pMap->cbGuestFrame;

    if (cbDstFrame != sizeof(*pu32Dst))
        RT_BZERO(pvDst, cbDstFrame * cFrames);

    while (cFrames-- > 0)
    {
        *pu32Dst = *pu32Src;
        pu32Src += 2;
        pu32Dst  = (uint32_t *)((uintptr_t)pu32Dst + cbDstFrame);
    }
}


/** Convert mono guest channel to host stereo, signed 32-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_G2H_GenericS32_Mono2Stereo(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    uint64_t       *pu64Dst    = (uint64_t *)pvDst;
    uint32_t const *pu32Src    = (uint32_t const *)pvSrc;
    size_t const    cbSrcFrame = pMap->cbGuestFrame;

    while (cFrames-- > 0)
    {
        uint32_t uSample = *pu32Src;
        *pu64Dst++ = RT_MAKE_U64(uSample, uSample);
        pu32Src = (uint32_t const *)((uintptr_t)pu32Src + cbSrcFrame);
    }
}


/** Convert stereo host to 2 or more guest channels, signed 16-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_H2G_GenericS16_Stereo2NonMono(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    uint32_t       *pu32Dst    = (uint32_t *)pvDst;
    uint32_t const *pu32Src    = (uint32_t const *)pvSrc; /** @todo could be misaligned */
    size_t const    cbDstFrame = pMap->cbGuestFrame;

    RT_BZERO(pvDst, cbDstFrame * cFrames);

    while (cFrames-- > 0)
    {
        *pu32Dst = *pu32Src++;
        pu32Dst = (uint32_t *)((uintptr_t)pu32Dst + cbDstFrame);
    }
}


/** Convert 2 or more guest channels to host stereo, signed 16-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_G2H_GenericS16_NonMono2Stereo(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    uint32_t       *pu32Dst    = (uint32_t *)pvDst;
    uint32_t const *pu32Src    = (uint32_t const *)pvSrc; /** @todo could be misaligned */
    size_t const    cbSrcFrame = pMap->cbGuestFrame;

    while (cFrames-- > 0)
    {
        *pu32Dst++ = *pu32Src;
        pu32Src = (uint32_t const *)((uintptr_t)pu32Src + cbSrcFrame);
    }
}


/** Convert stereo host to 2 or more guest channels, signed 32-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_H2G_GenericS32_Stereo2NonMono(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    uint64_t       *pu64Dst    = (uint64_t *)pvDst;         /** @todo could be misaligned. */
    uint64_t const *pu64Src    = (uint64_t const *)pvSrc;
    size_t const    cbDstFrame = pMap->cbGuestFrame;

    RT_BZERO(pvDst, cbDstFrame * cFrames);

    while (cFrames-- > 0)
    {
        *pu64Dst = *pu64Src++;
        pu64Dst = (uint64_t *)((uintptr_t)pu64Dst + cbDstFrame);
    }
}


/** Convert 2 or more guest channels to host stereo, signed 32-bit edition. */
static DECLCALLBACK(void)
hdaR3StreamMap_G2H_GenericS32_NonMono2Stereo(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    uint64_t       *pu64Dst    = (uint64_t *)pvDst;
    uint64_t const *pu64Src    = (uint64_t const *)pvSrc; /** @todo could be misaligned */
    size_t const    cbSrcFrame = pMap->cbGuestFrame;

    while (cFrames-- > 0)
    {
        *pu64Dst++ = *pu64Src;
        pu64Src = (uint64_t const *)((uintptr_t)pu64Src + cbSrcFrame);
    }
}


/** No conversion needed, just copy the data. */
static DECLCALLBACK(void)
hdaR3StreamMap_GenericCopy(void *pvDst, void const *pvSrc, uint32_t cFrames, HDASTREAMMAP const *pMap)
{
    memcpy(pvDst, pvSrc, cFrames * pMap->cbGuestFrame);
}


/**
 * Sets up a stream mapping according to the given properties / configuration.
 *
 * @return  VBox status code.
 * @retval  VERR_NOT_SUPPORTED if the channel setup is not supported.
 *
 * @param   pMap            Pointer to mapping to set up.
 * @param   pProps          Input: The stream PCM properties from the guest.
 *                          Output: Properties for the host stream.
 * @param   cHostChannels   The number of host channels to map to.
 */
static int hdaR3StreamMapSetup(PHDASTREAMMAP pMap, PPDMAUDIOPCMPROPS pProps, uint8_t cHostChannels)
{
    int rc;

    /** @todo We ASSUME that all channels in a stream ...
     *        - have the same format
     *        - are in a consecutive order with no gaps in between
     *        - have a simple (raw) data layout
     *        - work in a non-striped fashion, e.g. interleaved (only on one SDn, not spread over multiple SDns) */
    if  (   pProps->cChannels == 1  /* Mono */
         || pProps->cChannels == 2  /* Stereo */
         || pProps->cChannels == 4  /* Quadrophonic */
         || pProps->cChannels == 6) /* Surround (5.1) */
    {
        /*
         * Copy the guest stream properties and see if we need to change the host properties.
         */
        memcpy(&pMap->GuestProps, pProps, sizeof(PDMAUDIOPCMPROPS));
        if (pProps->cChannels != cHostChannels)
        {
            if (pProps->cChannels == 1)
                LogRelMax(32, ("HDA: Warning: Guest mono, host stereo.\n"));
            else if (cHostChannels == 1 && pProps->cChannels == 2)
                LogRelMax(32, ("HDA: Warning: Host mono, guest stereo.\n"));
            else
#ifndef VBOX_WITH_AUDIO_HDA_51_SURROUND
                LogRelMax(32, ("HDA: Warning: Guest configured %u channels, host only supports %u. Ignoring additional channels.\n",
                               pProps->cChannels, cHostChannels));
#else
# error reconsider the above logic
#endif
            pProps->cChannels = cHostChannels;
            pProps->cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT(pProps);
        }

        /*
         * Pick conversion functions.
         */
        Assert(pMap->GuestProps.cbSample == pProps->cbSample);

        /* If the channel count matches, we can use the memcpy converters: */
        if (pProps->cChannels == pMap->GuestProps.cChannels)
        {
            pMap->pfnGuestToHost = hdaR3StreamMap_GenericCopy;
            pMap->pfnHostToGuest = hdaR3StreamMap_GenericCopy;
            pMap->fMappingNeeded = false;
        }
        else
        {
            /* For multi-speaker guest configs, we currently just pick the
               first two channels and map this onto the host stereo ones. */
            AssertReturn(cHostChannels == 2, VERR_NOT_SUPPORTED);
            switch (pMap->GuestProps.cbSample)
            {
                case 2:
                    if (pMap->GuestProps.cChannels > 1)
                    {
                        pMap->pfnGuestToHost = hdaR3StreamMap_G2H_GenericS16_NonMono2Stereo;
                        pMap->pfnHostToGuest = hdaR3StreamMap_H2G_GenericS16_Stereo2NonMono;
                    }
                    else
                    {
                        pMap->pfnGuestToHost = hdaR3StreamMap_G2H_GenericS16_Mono2Stereo;
                        pMap->pfnHostToGuest = hdaR3StreamMap_H2G_GenericS16_Stereo2Mono;
                    }
                    break;

                case 4:
                    if (pMap->GuestProps.cChannels > 1)
                    {
                        pMap->pfnGuestToHost = hdaR3StreamMap_G2H_GenericS32_NonMono2Stereo;
                        pMap->pfnHostToGuest = hdaR3StreamMap_H2G_GenericS32_Stereo2NonMono;
                    }
                    else
                    {
                        pMap->pfnGuestToHost = hdaR3StreamMap_G2H_GenericS32_Mono2Stereo;
                        pMap->pfnHostToGuest = hdaR3StreamMap_H2G_GenericS32_Stereo2Mono;
                    }
                    break;

                default:
                    AssertMsgFailedReturn(("cbSample=%u\n", pMap->GuestProps.cbSample), VERR_NOT_SUPPORTED);
            }
            pMap->fMappingNeeded = true;
        }

        /** @todo bird: Not sure if this is really needed any more. */

        /* For now we don't have anything other as mono / stereo channels being covered by the backends.
         * So just set up one channel covering those and skipping the rest (like configured rear or center/LFE outputs). */
        pMap->cMappings  = 1;
        pMap->paMappings = (PPDMAUDIOSTREAMMAP)RTMemAlloc(sizeof(PDMAUDIOSTREAMMAP) * pMap->cMappings);
        if (pMap->paMappings)
        {
            PPDMAUDIOSTREAMMAP pMapLR = &pMap->paMappings[0];
            pMapLR->aenmIDs[0]  = PDMAUDIOSTREAMCHANNELID_FRONT_LEFT;
            pMapLR->aenmIDs[1]  = PDMAUDIOSTREAMCHANNELID_FRONT_RIGHT;
            pMapLR->cbFrame     = pProps->cbSample * pProps->cChannels;
            pMapLR->cbStep      = pProps->cbSample * 2 /* Front left + Front right channels */;
            pMapLR->offFirst    = 0;
            pMapLR->offNext     = pMapLR->offFirst;

            rc = hdaR3StreamChannelDataInit(&pMapLR->Data, PDMAUDIOSTREAMCHANNELDATA_FLAGS_NONE);
            AssertRC(rc);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_NOT_SUPPORTED; /** @todo r=andy Support more setups. */

    return rc;
}


/**
 * Initializes a stream mapping structure according to the given PCM properties.
 *
 * @return  IPRT status code.
 * @param   pMap            Pointer to mapping to initialize.
 * @param   cHostChannels   The number of host channels to map to.
 * @param   pProps          Input: The stream PCM properties from the guest.
 *                          Output: Properties for the host side.
 */
int hdaR3StreamMapInit(PHDASTREAMMAP pMap, uint8_t cHostChannels, PPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pMap, VERR_INVALID_POINTER);
    AssertPtrReturn(pProps, VERR_INVALID_POINTER);

    if (!DrvAudioHlpPcmPropsAreValid(pProps))
        return VERR_INVALID_PARAMETER;

    hdaR3StreamMapReset(pMap);

    pMap->cbGuestFrame = pProps->cChannels * pProps->cbSample;
    int rc = hdaR3StreamMapSetup(pMap, pProps, cHostChannels);
    if (RT_SUCCESS(rc))
    {
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        /* Create the circular buffer if not created yet. */
        if (!pMap->pCircBuf)
            rc = RTCircBufCreate(&pMap->pCircBuf, _4K); /** @todo Make size configurable? */
        if (RT_SUCCESS(rc))
#endif
        {
            LogFunc(("cChannels=%RU8, cBytes=%RU8 -> cbGuestFrame=%RU32\n",
                     pProps->cChannels, pProps->cbSample, pMap->cbGuestFrame));

            Assert(pMap->cbGuestFrame); /* Frame size must not be 0. */
            pMap->enmLayout = PDMAUDIOSTREAMLAYOUT_INTERLEAVED;
            return VINF_SUCCESS;
        }
    }

    return rc;
}


/**
 * Destroys a given stream mapping.
 *
 * @param   pMap            Pointer to mapping to destroy.
 */
void hdaR3StreamMapDestroy(PHDASTREAMMAP pMap)
{
    hdaR3StreamMapReset(pMap);

#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    if (pMap->pCircBuf)
    {
        RTCircBufDestroy(pMap->pCircBuf);
        pMap->pCircBuf = NULL;
    }
#endif
}


/**
 * Resets a given stream mapping.
 *
 * @param   pMap            Pointer to mapping to reset.
 */
void hdaR3StreamMapReset(PHDASTREAMMAP pMap)
{
    AssertPtrReturnVoid(pMap);

    pMap->enmLayout = PDMAUDIOSTREAMLAYOUT_UNKNOWN;

    if (pMap->paMappings)
    {
        for (uint8_t i = 0; i < pMap->cMappings; i++)
            hdaR3StreamChannelDataDestroy(&pMap->paMappings[i].Data);

        RTMemFree(pMap->paMappings);
        pMap->paMappings = NULL;

        pMap->cMappings = 0;
    }

    pMap->fMappingNeeded = false;
    pMap->pfnGuestToHost = hdaR3StreamMap_GenericCopy;
    pMap->pfnHostToGuest = hdaR3StreamMap_GenericCopy;
    RT_ZERO(pMap->GuestProps);
}

