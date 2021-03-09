/* $Id$ */
/** @file
 * PDM - Audio Helpers, Inlined Code. (DEV,++)
 *
 * This is all inlined because it's too tedious to create a couple libraries to
 * contain it all (same bad excuse as for intnetinline.h & pdmnetinline.h).
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef VBOX_INCLUDED_vmm_pdmaudioinline_h
#define VBOX_INCLUDED_vmm_pdmaudioinline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/** @defgroup grp_pdm_audio_inline      The PDM Audio Helper APIs
 * @ingroup grp_pdm
 * @{
 */

/* Fix later: */
DECLINLINE(bool) PDMAudioPropsAreValid(PCPDMAUDIOPCMPROPS pProps);
DECLINLINE(bool) PDMAudioPropsAreEqual(PCPDMAUDIOPCMPROPS pProps1, PCPDMAUDIOPCMPROPS pProps2);



/**
 * Gets the name of an audio direction enum value.
 *
 * @returns Pointer to read-only name string on success, "bad" if
 *          passed an invalid enum value.
 * @param   enmDir  The audio direction value to name.
 */
DECLINLINE(const char *) PDMAudioDirGetName(PDMAUDIODIR enmDir)
{
    switch (enmDir)
    {
        case PDMAUDIODIR_UNKNOWN: return "Unknown";
        case PDMAUDIODIR_IN:      return "Input";
        case PDMAUDIODIR_OUT:     return "Output";
        case PDMAUDIODIR_DUPLEX:  return "Duplex";

        /* no default */
        case PDMAUDIODIR_INVALID:
        case PDMAUDIODIR_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Invalid audio direction %d\n", enmDir), "bad");
}

/**
 * Gets the name of an audio mixer control enum value.
 *
 * @returns Pointer to read-only name, "bad" if invalid input.
 * @param   enmMixerCtl     The audio mixer control value.
 */
DECLINLINE(const char *) PDMAudioMixerCtlGetName(PDMAUDIOMIXERCTL enmMixerCtl)
{
    switch (enmMixerCtl)
    {
        case PDMAUDIOMIXERCTL_UNKNOWN:       return "Unknown";
        case PDMAUDIOMIXERCTL_VOLUME_MASTER: return "Master Volume";
        case PDMAUDIOMIXERCTL_FRONT:         return "Front";
        case PDMAUDIOMIXERCTL_CENTER_LFE:    return "Center / LFE";
        case PDMAUDIOMIXERCTL_REAR:          return "Rear";
        case PDMAUDIOMIXERCTL_LINE_IN:       return "Line-In";
        case PDMAUDIOMIXERCTL_MIC_IN:        return "Microphone-In";
        /* no default */
        case PDMAUDIOMIXERCTL_INVALID:
        case PDMAUDIOMIXERCTL_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Invalid mixer control %ld\n", enmMixerCtl), "bad");
}

/**
 * Gets the name of a playback destination enum value.
 *
 * @returns Pointer to read-only name, "bad" if invalid input.
 * @param   enmPlaybackDst      The playback destination value.
 */
DECLINLINE(const char *) PDMAudioPlaybackDstGetName(PDMAUDIOPLAYBACKDST enmPlaybackDst)
{
    switch (enmPlaybackDst)
    {
        case PDMAUDIOPLAYBACKDST_UNKNOWN:    return "Unknown";
        case PDMAUDIOPLAYBACKDST_FRONT:      return "Front";
        case PDMAUDIOPLAYBACKDST_CENTER_LFE: return "Center / LFE";
        case PDMAUDIOPLAYBACKDST_REAR:       return "Rear";
        /* no default */
        case PDMAUDIOPLAYBACKDST_INVALID:
        case PDMAUDIOPLAYBACKDST_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Invalid playback destination %ld\n", enmPlaybackDst), "bad");
}

/**
 * Gets the name of a recording source enum value.
 *
 * @returns Pointer to read-only name, "bad" if invalid input.
 * @param   enmRecSrc       The recording source value.
 */
DECLINLINE(const char *) PDMAudioRecSrcGetName(PDMAUDIORECSRC enmRecSrc)
{
    switch (enmRecSrc)
    {
        case PDMAUDIORECSRC_UNKNOWN: return "Unknown";
        case PDMAUDIORECSRC_MIC:     return "Microphone In";
        case PDMAUDIORECSRC_CD:      return "CD";
        case PDMAUDIORECSRC_VIDEO:   return "Video";
        case PDMAUDIORECSRC_AUX:     return "AUX";
        case PDMAUDIORECSRC_LINE:    return "Line In";
        case PDMAUDIORECSRC_PHONE:   return "Phone";
        /* no default */
        case PDMAUDIORECSRC_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Invalid recording source %ld\n", enmRecSrc), "bad");
}

/**
 * Checks whether the audio format is signed.
 *
 * @returns @c true for signed format, @c false for unsigned.
 * @param   enmFmt  The audio format.
 */
DECLINLINE(bool) PDMAudioFormatIsSigned(PDMAUDIOFMT enmFmt)
{
    switch (enmFmt)
    {
        case PDMAUDIOFMT_S8:
        case PDMAUDIOFMT_S16:
        case PDMAUDIOFMT_S32:
            return true;

        case PDMAUDIOFMT_U8:
        case PDMAUDIOFMT_U16:
        case PDMAUDIOFMT_U32:
            return false;

        /* no default */
        case PDMAUDIOFMT_INVALID:
        case PDMAUDIOFMT_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Bogus audio format %ld\n", enmFmt), false);
}

/**
 * Gets the encoding width in bits of the give audio format.
 *
 * @returns Bit count. 0 if invalid input.
 * @param   enmFmt      The audio format.
 */
DECLINLINE(uint8_t) PDMAudioFormatGetBits(PDMAUDIOFMT enmFmt)
{
    switch (enmFmt)
    {
        case PDMAUDIOFMT_S8:
        case PDMAUDIOFMT_U8:
            return 8;

        case PDMAUDIOFMT_U16:
        case PDMAUDIOFMT_S16:
            return 16;

        case PDMAUDIOFMT_U32:
        case PDMAUDIOFMT_S32:
            return 32;

        /* no default */
        case PDMAUDIOFMT_INVALID:
        case PDMAUDIOFMT_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Bogus audio format %ld\n", enmFmt), 0);
}

/**
 * Gets the name of an audio format enum value.
 *
 * @returns Pointer to read-only name on success, returns "bad" on if
 *          invalid enum value.
 * @param   enmFmt      The audio format to name.
 */
DECLINLINE(const char *) PDMAudioFormatGetName(PDMAUDIOFMT enmFmt)
{
    switch (enmFmt)
    {
        case PDMAUDIOFMT_U8:    return "U8";
        case PDMAUDIOFMT_U16:   return "U16";
        case PDMAUDIOFMT_U32:   return "U32";
        case PDMAUDIOFMT_S8:    return "S8";
        case PDMAUDIOFMT_S16:   return "S16";
        case PDMAUDIOFMT_S32:   return "S32";
        /* no default */
        case PDMAUDIOFMT_INVALID:
        case PDMAUDIOFMT_32BIT_HACK:
            break;
    }
    AssertMsgFailedReturn(("Bogus audio format %d\n", enmFmt), "bad");
}

/**
 * Initializes a stream configuration from PCM properties.
 *
 * @return  IPRT status code.
 * @param   pCfg        The stream configuration to initialize.
 * @param   pProps      The PCM properties to use.
 */
DECLINLINE(int) PDMAudioStrmCfgInitWithProps(PPDMAUDIOSTREAMCFG pCfg, PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pProps, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,   VERR_INVALID_POINTER);

    RT_ZERO(*pCfg);
    pCfg->Backend.cFramesPreBuffering = UINT32_MAX; /* Explicitly set to "undefined". */

    memcpy(&pCfg->Props, pProps, sizeof(PDMAUDIOPCMPROPS));

    return VINF_SUCCESS;
}

/**
 * Checks whether stream configuration matches the given PCM properties.
 *
 * @returns @c true if equal, @c false if not.
 * @param   pCfg    The stream configuration.
 * @param   pProps  The PCM properties to match with.
 */
DECLINLINE(bool) PDMAudioStrmCfgMatchesProps(PCPDMAUDIOSTREAMCFG pCfg, PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pCfg, false);
    return PDMAudioPropsAreEqual(pProps, &pCfg->Props);
}

/**
 * Frees an audio stream allocated by PDMAudioStrmCfgDup().
 *
 * @param   pCfg    The stream configuration to free.
 */
DECLINLINE(void) PDMAudioStrmCfgFree(PPDMAUDIOSTREAMCFG pCfg)
{
    if (pCfg)
        RTMemFree(pCfg);
}

/**
 * Checks whether the given stream configuration is valid or not.
 *
 * @returns true/false accordingly.
 * @param   pCfg    Stream configuration to check.
 *
 * @remarks This just performs a generic check of value ranges.  Further, it
 *          will assert if the input is invalid.
 *
 * @sa      PDMAudioPropsAreValid
 */
DECLINLINE(bool) PDMAudioStrmCfgIsValid(PCPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg, false);
    AssertMsgReturn(pCfg->enmDir    >= PDMAUDIODIR_UNKNOWN          && pCfg->enmDir    <= PDMAUDIODIR_DUPLEX,
                    ("%d\n", pCfg->enmDir), false);
    AssertMsgReturn(pCfg->enmLayout >= PDMAUDIOSTREAMLAYOUT_UNKNOWN && pCfg->enmLayout <= PDMAUDIOSTREAMLAYOUT_RAW,
                    ("%d\n", pCfg->enmLayout), false);
    return PDMAudioPropsAreValid(&pCfg->Props);
}

/**
 * Copies one stream configuration to another.
 *
 * @returns IPRT status code.
 * @param   pDstCfg     The destination stream configuration.
 * @param   pSrcCfg     The source stream configuration.
 */
DECLINLINE(int) PDMAudioStrmCfgCopy(PPDMAUDIOSTREAMCFG pDstCfg, PCPDMAUDIOSTREAMCFG pSrcCfg)
{
    AssertPtrReturn(pDstCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrcCfg, VERR_INVALID_POINTER);

    /* This used to be VBOX_STRICT only and return VERR_INVALID_PARAMETER, but
       that's making release builds work differently from debug & strict builds,
       which is a terrible idea: */
    Assert(PDMAudioStrmCfgIsValid(pSrcCfg));

    memcpy(pDstCfg, pSrcCfg, sizeof(PDMAUDIOSTREAMCFG));

    return VINF_SUCCESS;
}

/**
 * Duplicates an audio stream configuration.
 *
 * @returns Pointer to duplicate on success, NULL on failure.  Must be freed
 *          using PDMAudioStrmCfgFree().
 *
 * @param   pCfg        The audio stream configuration to duplicate.
 */
DECLINLINE(PPDMAUDIOSTREAMCFG) PDMAudioStrmCfgDup(PCPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg, NULL);

    PPDMAUDIOSTREAMCFG pDst = (PPDMAUDIOSTREAMCFG)RTMemAllocZ(sizeof(PDMAUDIOSTREAMCFG));
    if (pDst)
    {
        int rc = PDMAudioStrmCfgCopy(pDst, pCfg);
        if (RT_SUCCESS(rc))
            return pDst;

        PDMAudioStrmCfgFree(pDst);
    }
    return NULL;
}

/**
 * Logs an audio stream configuration.
 *
 * @param   pCfg        The stream configuration to log.
 */
DECLINLINE(void) PDMAudioStrmCfgLog(PCPDMAUDIOSTREAMCFG pCfg)
{
    if (pCfg)
        LogFunc(("szName=%s enmDir=%RU32 uHz=%RU32 cBits=%RU8%s cChannels=%RU8\n", pCfg->szName, pCfg->enmDir,
                 pCfg->Props.uHz, pCfg->Props.cbSample * 8, pCfg->Props.fSigned ? "S" : "U", pCfg->Props.cChannels));
}

/**
 * Converts a stream command enum value to a string.
 *
 * @returns Pointer to read-only stream command name on success,
 *          "bad" if invalid command value.
 * @param   enmCmd      The stream command to name.
 */
DECLINLINE(const char *) PDMAudioStrmCmdGetName(PDMAUDIOSTREAMCMD enmCmd)
{
    switch (enmCmd)
    {
        case PDMAUDIOSTREAMCMD_INVALID: return "Invalid";
        case PDMAUDIOSTREAMCMD_UNKNOWN: return "Unknown";
        case PDMAUDIOSTREAMCMD_ENABLE:  return "Enable";
        case PDMAUDIOSTREAMCMD_DISABLE: return "Disable";
        case PDMAUDIOSTREAMCMD_PAUSE:   return "Pause";
        case PDMAUDIOSTREAMCMD_RESUME:  return "Resume";
        case PDMAUDIOSTREAMCMD_DRAIN:   return "Drain";
        case PDMAUDIOSTREAMCMD_DROP:    return "Drop";
        case PDMAUDIOSTREAMCMD_32BIT_HACK:
            break;
        /* no default! */
    }
    AssertMsgFailedReturn(("Invalid stream command %d\n", enmCmd), "bad");
}

/**
 * Checks if the stream status is one that can be read from.
 *
 * @returns @c true if ready to be read from, @c false if not.
 * @param   fStatus     Stream status to evaluate, PDMAUDIOSTREAMSTS_FLAGS_XXX.
 */
DECLINLINE(bool) PDMAudioStrmStatusCanRead(PDMAUDIOSTREAMSTS fStatus)
{
    AssertReturn(fStatus & PDMAUDIOSTREAMSTS_VALID_MASK, false);
    /*
    return      fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
           &&   fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED
           && !(fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED)
           && !(fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT);*/
    return (fStatus & (  PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
                       | PDMAUDIOSTREAMSTS_FLAGS_ENABLED
                       | PDMAUDIOSTREAMSTS_FLAGS_PAUSED
                       | PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT ))
        == (  PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
            | PDMAUDIOSTREAMSTS_FLAGS_ENABLED);
}

/**
 * Checks if the stream status is one that can be written to.
 *
 * @returns @c true if ready to be written to, @c false if not.
 * @param   fStatus     Stream status to evaluate, PDMAUDIOSTREAMSTS_FLAGS_XXX.
 */
DECLINLINE(bool) PDMAudioStrmStatusCanWrite(PDMAUDIOSTREAMSTS fStatus)
{
    AssertReturn(fStatus & PDMAUDIOSTREAMSTS_VALID_MASK, false);
    /*
    return      fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
           &&   fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED
           && !(fStatus & PDMAUDIOSTREAMSTS_FLAGS_PAUSED)
           && !(fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE)
           && !(fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT);*/
    return (fStatus & (  PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
                       | PDMAUDIOSTREAMSTS_FLAGS_ENABLED
                       | PDMAUDIOSTREAMSTS_FLAGS_PAUSED
                       | PDMAUDIOSTREAMSTS_FLAGS_PENDING_DISABLE
                       | PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT))
        == (  PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
            | PDMAUDIOSTREAMSTS_FLAGS_ENABLED);
}

/**
 * Checks if the stream status is a read-to-operate one.
 *
 * @returns @c true if ready to operate, @c false if not.
 * @param   fStatus     Stream status to evaluate, PDMAUDIOSTREAMSTS_FLAGS_XXX.
 */
DECLINLINE(bool) PDMAudioStrmStatusIsReady(PDMAUDIOSTREAMSTS fStatus)
{
    AssertReturn(fStatus & PDMAUDIOSTREAMSTS_VALID_MASK, false);
    /*
    return      fStatus & PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
           &&   fStatus & PDMAUDIOSTREAMSTS_FLAGS_ENABLED
           && !(fStatus & PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT);*/
    return (fStatus & (  PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
                       | PDMAUDIOSTREAMSTS_FLAGS_ENABLED
                       | PDMAUDIOSTREAMSTS_FLAGS_PENDING_REINIT))
        == (  PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED
            | PDMAUDIOSTREAMSTS_FLAGS_ENABLED);
}


/*********************************************************************************************************************************
*   PCM Property Helpers                                                                                                         *
*********************************************************************************************************************************/

/**
 * Gets the bitrate.
 *
 * Divide the result by 8 to get the byte rate.
 *
 * @returns Bit rate.
 * @param   pProps              PCM properties to calculate bitrate for.
 */
DECLINLINE(uint32_t) PDMAudioPropsGetBitrate(PCPDMAUDIOPCMPROPS pProps)
{
    return pProps->cbSample * pProps->cChannels * pProps->uHz * 8;
}

/**
 * Rounds down the given byte amount to the nearest frame boundrary.
 *
 * @returns Rounded byte amount.
 * @param   pProps      PCM properties to use.
 * @param   cb          The size (in bytes) to round.
 */
DECLINLINE(uint32_t) PDMAudioPropsFloorBytesToFrame(PCPDMAUDIOPCMPROPS pProps, uint32_t cb)
{
    AssertPtrReturn(pProps, 0);
    return PDMAUDIOPCMPROPS_F2B(pProps, PDMAUDIOPCMPROPS_B2F(pProps, cb));
}

/**
 * Checks if the given size is aligned on a frame boundrary.
 *
 * @returns @c true if properly aligned, @c false if not.
 * @param   pProps      PCM properties to use.
 * @param   cb          The size (in bytes) to check.
 */
DECLINLINE(bool) PDMAudioPropsIsSizeAligned(PCPDMAUDIOPCMPROPS pProps, uint32_t cb)
{
    AssertPtrReturn(pProps, false);
    uint32_t const cbFrame = PDMAUDIOPCMPROPS_F2B(pProps, 1 /* Frame */);
    AssertReturn(cbFrame, false);
    return cb % cbFrame == 0;
}

/**
 * Converts bytes to frames (rounding down of course).
 *
 * @returns Number of frames.
 * @param   pProps      PCM properties to use.
 * @param   cb          The number of bytes to convert.
 */
DECLINLINE(uint32_t) PDMAudioPropsBytesToFrames(PCPDMAUDIOPCMPROPS pProps, uint32_t cb)
{
    AssertPtrReturn(pProps, 0);
    return PDMAUDIOPCMPROPS_B2F(pProps, cb);
}

/**
 * Converts bytes to milliseconds.
 *
 * @return  Number milliseconds @a cb takes to play or record.
 * @param   pProps      PCM properties to use.
 * @param   cb          The number of bytes to convert.
 *
 * @note    Rounds up the result.
 */
DECLINLINE(uint64_t) PDMAudioPropsBytesToMilli(PCPDMAUDIOPCMPROPS pProps, uint32_t cb)
{
    AssertPtrReturn(pProps, 0);

    /* Check parameters to prevent division by chainsaw: */
    uint32_t const uHz = pProps->uHz;
    if (uHz)
    {
        const unsigned cbFrame = PDMAUDIOPCMPROPS_F2B(pProps, 1 /* Frame */);
        if (cbFrame)
        {
            /* Round cb up to closest frame size: */
            cb = (cb + cbFrame - 1) / cbFrame;

            /* Convert to milliseconds. */
            return (cb * (uint64_t)RT_MS_1SEC + uHz - 1) / uHz;
        }
    }
    return 0;
}

/**
 * Converts bytes to microseconds.
 *
 * @return  Number microseconds @a cb takes to play or record.
 * @param   pProps      PCM properties to use.
 * @param   cb          The number of bytes to convert.
 *
 * @note    Rounds up the result.
 */
DECLINLINE(uint64_t) PDMAudioPropsBytesToMicro(PCPDMAUDIOPCMPROPS pProps, uint32_t cb)
{
    AssertPtrReturn(pProps, 0);

    /* Check parameters to prevent division by chainsaw: */
    uint32_t const uHz = pProps->uHz;
    if (uHz)
    {
        const unsigned cbFrame = PDMAUDIOPCMPROPS_F2B(pProps, 1 /* Frame */);
        if (cbFrame)
        {
            /* Round cb up to closest frame size: */
            cb = (cb + cbFrame - 1) / cbFrame;

            /* Convert to microseconds. */
            return (cb * (uint64_t)RT_US_1SEC + uHz - 1) / uHz;
        }
    }
    return 0;
}

/**
 * Converts bytes to nanoseconds.
 *
 * @return  Number nanoseconds @a cb takes to play or record.
 * @param   pProps      PCM properties to use.
 * @param   cb          The number of bytes to convert.
 *
 * @note    Rounds up the result.
 */
DECLINLINE(uint64_t) PDMAudioPropsBytesToNano(PCPDMAUDIOPCMPROPS pProps, uint32_t cb)
{
    AssertPtrReturn(pProps, 0);

    /* Check parameters to prevent division by chainsaw: */
    uint32_t const uHz = pProps->uHz;
    if (uHz)
    {
        const unsigned cbFrame = PDMAUDIOPCMPROPS_F2B(pProps, 1 /* Frame */);
        if (cbFrame)
        {
            /* Round cb up to closest frame size: */
            cb = (cb + cbFrame - 1) / cbFrame;

            /* Convert to nanoseconds. */
            return (cb * (uint64_t)RT_NS_1SEC + uHz - 1) / uHz;
        }
    }
    return 0;
}

/**
 * Converts frames to bytes.
 *
 * @returns Number of bytes.
 * @param   pProps      The PCM properties to use.
 * @param   cFrames     Number of audio frames to convert.
 * @sa      PDMAUDIOPCMPROPS_F2B
 */
DECLINLINE(uint32_t) PDMAudioPropsFramesToBytes(PCPDMAUDIOPCMPROPS pProps, uint32_t cFrames)
{
    AssertPtrReturn(pProps, 0);
    return PDMAUDIOPCMPROPS_F2B(pProps, cFrames);
}

/**
 * Converts frames to milliseconds.
 *
 * @returns milliseconds.
 * @param   pProps      The PCM properties to use.
 * @param   cFrames     Number of audio frames to convert.
 * @note    No rounding here, result is floored.
 */
DECLINLINE(uint64_t) PDMAudioPropsFramesToMilli(PCPDMAUDIOPCMPROPS pProps, uint32_t cFrames)
{
    AssertPtrReturn(pProps, 0);

    /* Check input to prevent division by chainsaw: */
    uint32_t const uHz = pProps->uHz;
    if (uHz)
        return ASMMultU32ByU32DivByU32(cFrames, RT_MS_1SEC, uHz);
    return 0;
}

/**
 * Converts frames to nanoseconds.
 *
 * @returns Nanoseconds.
 * @param   pProps      The PCM properties to use.
 * @param   cFrames     Number of audio frames to convert.
 * @note    No rounding here, result is floored.
 */
DECLINLINE(uint64_t) PDMAudioPropsFramesToNano(PCPDMAUDIOPCMPROPS pProps, uint32_t cFrames)
{
    AssertPtrReturn(pProps, 0);

    /* Check input to prevent division by chainsaw: */
    uint32_t const uHz = pProps->uHz;
    if (uHz)
        return ASMMultU32ByU32DivByU32(cFrames, RT_NS_1SEC, uHz);
    return 0;
}

/**
 * Converts milliseconds to frames.
 *
 * @returns Number of frames
 * @param   pProps      The PCM properties to use.
 * @param   cMs         The number of milliseconds to convert.
 *
 * @note    The result is rounded rather than floored (hysterical raisins).
 */
DECLINLINE(uint32_t) PDMAudioPropsMilliToFrames(PCPDMAUDIOPCMPROPS pProps, uint64_t cMs)
{
    AssertPtrReturn(pProps, 0);

    uint32_t const uHz = pProps->uHz;
    uint32_t cFrames;
    if (cMs < RT_MS_1SEC)
        cFrames = 0;
    else
    {
        cFrames = cMs / RT_MS_1SEC * uHz;
        cMs %= RT_MS_1SEC;
    }
    cFrames += (ASMMult2xU32RetU64(uHz, (uint32_t)cMs) + RT_MS_1SEC - 1) / RT_MS_1SEC;
    return cFrames;
}

/**
 * Converts milliseconds to bytes.
 *
 * @returns Number of bytes (frame aligned).
 * @param   pProps      The PCM properties to use.
 * @param   cMs         The number of milliseconds to convert.
 *
 * @note    The result is rounded rather than floored (hysterical raisins).
 */
DECLINLINE(uint32_t) PDMAudioPropsMilliToBytes(PCPDMAUDIOPCMPROPS pProps, uint64_t cMs)
{
    return PDMAUDIOPCMPROPS_F2B(pProps, PDMAudioPropsMilliToFrames(pProps, cMs));
}

/**
 * Converts nanoseconds to frames.
 *
 * @returns Number of frames
 * @param   pProps      The PCM properties to use.
 * @param   cNs         The number of nanoseconds to convert.
 *
 * @note    The result is rounded rather than floored (hysterical raisins).
 */
DECLINLINE(uint32_t) PDMAudioPropsNanoToFrames(PCPDMAUDIOPCMPROPS pProps, uint64_t cNs)
{
    AssertPtrReturn(pProps, 0);

    uint32_t const uHz = pProps->uHz;
    uint32_t cFrames;
    if (cNs < RT_NS_1SEC)
        cFrames = 0;
    else
    {
        cFrames = cNs / RT_NS_1SEC * uHz;
        cNs %= RT_NS_1SEC;
    }
    cFrames += (ASMMult2xU32RetU64(uHz, (uint32_t)cNs) + RT_NS_1SEC - 1) / RT_NS_1SEC;
    return cFrames;
}

/**
 * Converts nanoseconds to bytes.
 *
 * @returns Number of bytes (frame aligned).
 * @param   pProps      The PCM properties to use.
 * @param   cNs         The number of nanoseconds to convert.
 *
 * @note    The result is rounded rather than floored (hysterical raisins).
 */
DECLINLINE(uint32_t) PDMAudioPropsNanoToBytes(PCPDMAUDIOPCMPROPS pProps, uint64_t cNs)
{
    return PDMAUDIOPCMPROPS_F2B(pProps, PDMAudioPropsNanoToFrames(pProps, cNs));
}

/**
 * Clears a sample buffer by the given amount of audio frames with silence (according to the format
 * given by the PCM properties).
 *
 * @param   pProps      The PCM properties to apply.
 * @param   pvBuf       The buffer to clear.
 * @param   cbBuf       The buffer size in bytes.
 * @param   cFrames     The number of audio frames to clear.  Capped at @a cbBuf
 *                      if exceeding the buffer.  If the size is an unaligned
 *                      number of frames, the extra bytes may be left
 *                      uninitialized in some configurations.
 */
DECLINLINE(void) PDMAudioPropsClearBuffer(PCPDMAUDIOPCMPROPS pProps, void *pvBuf, size_t cbBuf, uint32_t cFrames)
{
    /*
     * Validate input
     */
    AssertPtrReturnVoid(pProps);
    Assert(pProps->cbSample);
    if (!cbBuf || !cFrames)
        return;
    AssertPtrReturnVoid(pvBuf);

    Assert(pProps->fSwapEndian == false); /** @todo Swapping Endianness is not supported yet. */

    /*
     * Decide how much needs clearing.
     */
    size_t cbToClear = PDMAudioPropsFramesToBytes(pProps, cFrames);
    AssertStmt(cbToClear <= cbBuf, cbToClear = cbBuf);

    Log2Func(("pProps=%p, pvBuf=%p, cFrames=%RU32, fSigned=%RTbool, cBytes=%RU8\n",
              pProps, pvBuf, cFrames, pProps->fSigned, pProps->cbSample));

    /*
     * Do the job.
     */
    if (pProps->fSigned)
        RT_BZERO(pvBuf, cbToClear);
    else /* Unsigned formats. */
    {
        switch (pProps->cbSample)
        {
            case 1: /* 8 bit */
                memset(pvBuf, 0x80, cbToClear);
                break;

            case 2: /* 16 bit */
            {
                uint16_t *pu16Dst = (uint16_t *)pvBuf;
                size_t    cLeft   = cbToClear / sizeof(uint16_t);
                while (cLeft-- > 0)
                    *pu16Dst++ = 0x80;
                break;
            }

            /** @todo Add 24 bit? */

            case 4: /* 32 bit */
                ASMMemFill32(pvBuf, cbToClear & ~(size_t)3, 0x80);
                break;

            default:
                AssertMsgFailed(("Invalid bytes per sample: %RU8\n", pProps->cbSample));
        }
    }
}

/**
 * Compares two sets of PCM properties.
 *
 * @returns @c true if the same, @c false if not.
 * @param   pProps1     The first set of properties to compare.
 * @param   pProps2     The second set of properties to compare.
 */
DECLINLINE(bool) PDMAudioPropsAreEqual(PCPDMAUDIOPCMPROPS pProps1, PCPDMAUDIOPCMPROPS pProps2)
{
    AssertPtrReturn(pProps1, false);
    AssertPtrReturn(pProps2, false);

    if (pProps1 == pProps2) /* If the pointers match, take a shortcut. */
        return true;

    return pProps1->uHz         == pProps2->uHz
        && pProps1->cChannels   == pProps2->cChannels
        && pProps1->cbSample    == pProps2->cbSample
        && pProps1->fSigned     == pProps2->fSigned
        && pProps1->fSwapEndian == pProps2->fSwapEndian;
}

/**
 * Checks whether the given PCM properties are valid or not.
 *
 * @returns true/false accordingly.
 * @param   pProps  The PCM properties to check.
 *
 * @remarks This just performs a generic check of value ranges.  Further, it
 *          will assert if the input is invalid.
 *
 * @sa      PDMAudioStrmCfgIsValid
 */
DECLINLINE(bool) PDMAudioPropsAreValid(PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pProps, false);

    AssertReturn(pProps->cChannels != 0, false);
    AssertMsgReturn(pProps->cbSample == 1 || pProps->cbSample == 2 || pProps->cbSample == 4,
                    ("%u\n", pProps->cbSample), false);
    AssertMsgReturn(pProps->uHz >= 1000 && pProps->uHz < 1000000, ("%u\n", pProps->uHz), false);
    AssertMsgReturn(pProps->cShift == PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pProps->cbSample, pProps->cChannels),
                    ("cShift=%u cbSample=%u cChannels=%u\n", pProps->cShift, pProps->cbSample, pProps->cChannels),
                    false);
    return true;
}

/**
 * Get number of bytes per frame.
 *
 * @returns Number of bytes per audio frame.
 * @param   pProps  PCM properties to use.
 * @sa      PDMAUDIOPCMPROPS_F2B
 */
DECLINLINE(uint32_t) PDMAudioPropsBytesPerFrame(PCPDMAUDIOPCMPROPS pProps)
{
    return PDMAUDIOPCMPROPS_F2B(pProps, 1 /*cFrames*/);
}

/**
 * Prints PCM properties to the debug log.
 *
 * @param   pProps              Stream configuration to log.
 */
DECLINLINE(void) PDMAudioPropsLog(PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturnVoid(pProps);

    Log(("uHz=%RU32, cChannels=%RU8, cBits=%RU8%s",
         pProps->uHz, pProps->cChannels, pProps->cbSample * 8, pProps->fSigned ? "S" : "U"));
}

/** @} */

#endif /* !VBOX_INCLUDED_vmm_pdmaudioinline_h */
