/* $Id$ */
/** @file
 * Intermedia audio driver, common routines. These are also used
 * in the drivers which are bound to Main, e.g. the VRDE or the
 * video audio recording drivers.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on: audio_template.h from QEMU AUDIO subsystem.
 *
 * QEMU Audio subsystem header
 *
 * Copyright (c) 2005 Vassili Karpov (malc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define LOG_GROUP LOG_GROUP_DRV_AUDIO
#include <VBox/log.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/vmm/mm.h>

#include <ctype.h>
#include <stdlib.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"


/**
 * Retrieves the matching PDMAUDIOFMT for given bits + signing flag.
 *
 * @return  IPRT status code.
 * @return  PDMAUDIOFMT         Resulting audio format or PDMAUDIOFMT_INVALID if invalid.
 * @param   cBits               Bits to retrieve audio format for.
 * @param   fSigned             Signed flag for bits to retrieve audio format for.
 */
PDMAUDIOFMT DrvAudioAudFmtBitsToAudFmt(uint8_t cBits, bool fSigned)
{
    if (fSigned)
    {
        switch (cBits)
        {
            case 8:  return PDMAUDIOFMT_S8;
            case 16: return PDMAUDIOFMT_S16;
            case 32: return PDMAUDIOFMT_S32;
            default: break;
        }
    }
    else
    {
        switch (cBits)
        {
            case 8:  return PDMAUDIOFMT_U8;
            case 16: return PDMAUDIOFMT_U16;
            case 32: return PDMAUDIOFMT_U32;
            default: break;
        }
    }

    AssertMsgFailed(("Bogus audio bits %RU8\n", cBits));
    return PDMAUDIOFMT_INVALID;
}

/**
 * Clears a sample buffer by the given amount of audio samples.
 *
 * @return  IPRT status code.
 * @param   pPCMProps               PCM properties to use for the buffer to clear.
 * @param   pvBuf                   Buffer to clear.
 * @param   cbBuf                   Size (in bytes) of the buffer.
 * @param   cSamples                Number of audio samples to clear in the buffer.
 */
void DrvAudioHlpClearBuf(PPDMPCMPROPS pPCMProps, void *pvBuf, size_t cbBuf, uint32_t cSamples)
{
    AssertPtrReturnVoid(pPCMProps);
    AssertPtrReturnVoid(pvBuf);

    if (!cbBuf || !cSamples)
        return;

    Log2Func(("pPCMInfo=%p, pvBuf=%p, cSamples=%RU32, fSigned=%RTbool, cBits=%RU8, cShift=%RU8\n",
              pPCMProps, pvBuf, cSamples, pPCMProps->fSigned, pPCMProps->cBits, pPCMProps->cShift));

    if (pPCMProps->fSigned)
    {
        memset(pvBuf, 0, cSamples << pPCMProps->cShift);
    }
    else
    {
        switch (pPCMProps->cBits)
        {
            case 8:
            {
                memset(pvBuf, 0x80, cSamples << pPCMProps->cShift);
                break;
            }

            case 16:
            {
                uint16_t *p = (uint16_t *)pvBuf;
                int shift = pPCMProps->cChannels - 1;
                short s = INT16_MAX;

                if (pPCMProps->fSwapEndian)
                    s = RT_BSWAP_U16(s);

                for (unsigned i = 0; i < cSamples << shift; i++)
                    p[i] = s;

                break;
            }

            case 32:
            {
                uint32_t *p = (uint32_t *)pvBuf;
                int shift = pPCMProps->cChannels - 1;
                int32_t s = INT32_MAX;

                if (pPCMProps->fSwapEndian)
                    s = RT_BSWAP_U32(s);

                for (unsigned i = 0; i < cSamples << shift; i++)
                    p[i] = s;

                break;
            }

            default:
            {
                AssertMsgFailed(("Invalid bits: %RU8\n", pPCMProps->cBits));
                break;
            }
        }
    }
}

const char *DrvAudHlpRecSrcToStr(PDMAUDIORECSOURCE enmRecSrc)
{
    switch (enmRecSrc)
    {
        case PDMAUDIORECSOURCE_MIC:   return "Microphone In";
        case PDMAUDIORECSOURCE_CD:    return "CD";
        case PDMAUDIORECSOURCE_VIDEO: return "Video";
        case PDMAUDIORECSOURCE_AUX:   return "AUX";
        case PDMAUDIORECSOURCE_LINE:  return "Line In";
        case PDMAUDIORECSOURCE_PHONE: return "Phone";
        default:
            break;
    }

    AssertMsgFailed(("Invalid recording source %ld\n", enmRecSrc));
    return "Unknown";
}

/**
 * Returns wether the given audio format has signed bits or not.
 *
 * @return  IPRT status code.
 * @return  bool                @true for signed bits, @false for unsigned.
 * @param   enmFmt              Audio format to retrieve value for.
 */
bool DrvAudioHlpAudFmtIsSigned(PDMAUDIOFMT enmFmt)
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

        default:
            break;
    }

    AssertMsgFailed(("Bogus audio format %ld\n", enmFmt));
    return false;
}

/**
 * Returns the bits of a given audio format.
 *
 * @return  IPRT status code.
 * @return  uint8_t             Bits of audio format.
 * @param   enmFmt              Audio format to retrieve value for.
 */
uint8_t DrvAudioHlpAudFmtToBits(PDMAUDIOFMT enmFmt)
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

        default:
            break;
    }

    AssertMsgFailed(("Bogus audio format %ld\n", enmFmt));
    return 0;
}

const char *DrvAudioHlpAudFmtToStr(PDMAUDIOFMT enmFmt)
{
    switch (enmFmt)
    {
        case PDMAUDIOFMT_U8:
            return "U8";

        case PDMAUDIOFMT_U16:
            return "U16";

        case PDMAUDIOFMT_U32:
            return "U32";

        case PDMAUDIOFMT_S8:
            return "S8";

        case PDMAUDIOFMT_S16:
            return "S16";

        case PDMAUDIOFMT_S32:
            return "S32";

        default:
            break;
    }

    AssertMsgFailed(("Bogus audio format %ld\n", enmFmt));
    return "Invalid";
}

PDMAUDIOFMT DrvAudioHlpStrToAudFmt(const char *pszFmt)
{
    AssertPtrReturn(pszFmt, PDMAUDIOFMT_INVALID);

    if (!RTStrICmp(pszFmt, "u8"))
        return PDMAUDIOFMT_U8;
    else if (!RTStrICmp(pszFmt, "u16"))
        return PDMAUDIOFMT_U16;
    else if (!RTStrICmp(pszFmt, "u32"))
        return PDMAUDIOFMT_U32;
    else if (!RTStrICmp(pszFmt, "s8"))
        return PDMAUDIOFMT_S8;
    else if (!RTStrICmp(pszFmt, "s16"))
        return PDMAUDIOFMT_S16;
    else if (!RTStrICmp(pszFmt, "s32"))
        return PDMAUDIOFMT_S32;

    AssertMsgFailed(("Invalid audio format \"%s\"\n", pszFmt));
    return PDMAUDIOFMT_INVALID;
}

bool DrvAudioHlpPCMPropsAreEqual(PPDMPCMPROPS pProps, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pProps, false);
    AssertPtrReturn(pCfg,   false);

    int cBits = 8;
    bool fSigned = false;

    switch (pCfg->enmFormat)
    {
        case PDMAUDIOFMT_S8:
            fSigned = true;
        case PDMAUDIOFMT_U8:
            break;

        case PDMAUDIOFMT_S16:
            fSigned = true;
        case PDMAUDIOFMT_U16:
            cBits = 16;
            break;

        case PDMAUDIOFMT_S32:
            fSigned = true;
        case PDMAUDIOFMT_U32:
            cBits = 32;
            break;

        default:
            AssertMsgFailed(("Unknown format %ld\n", pCfg->enmFormat));
            break;
    }

    bool fEqual =    pProps->uHz         == pCfg->uHz
                  && pProps->cChannels   == pCfg->cChannels
                  && pProps->fSigned     == fSigned
                  && pProps->cBits       == cBits
                  && pProps->fSwapEndian == !(pCfg->enmEndianness == PDMAUDIOHOSTENDIANNESS);
    return fEqual;
}

bool DrvAudioHlpPCMPropsAreEqual(PPDMPCMPROPS pProps1, PPDMPCMPROPS pProps2)
{
    AssertPtrReturn(pProps1, false);
    AssertPtrReturn(pProps2, false);

    return    pProps1->uHz         == pProps2->uHz
           && pProps1->cChannels   == pProps2->cChannels
           && pProps1->fSigned     == pProps2->fSigned
           && pProps1->cBits       == pProps2->cBits
           && pProps1->fSwapEndian == pProps2->fSwapEndian;
}

/**
 * Converts PCM properties to a audio stream configuration.
 *
 * @return  IPRT status code.
 * @param   pPCMProps           Pointer to PCM properties to convert.
 * @param   pCfg                Pointer to audio stream configuration to store result into.
 */
int DrvAudioHlpPCMPropsToStreamCfg(PPDMPCMPROPS pPCMProps, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pPCMProps, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,      VERR_INVALID_POINTER);

    pCfg->uHz           = pPCMProps->uHz;
    pCfg->cChannels     = pPCMProps->cChannels;
    pCfg->enmFormat     = DrvAudioAudFmtBitsToAudFmt(pPCMProps->cBits, pPCMProps->fSigned);

    /** @todo We assume little endian is the default for now. */
    pCfg->enmEndianness = pPCMProps->fSwapEndian == false ? PDMAUDIOENDIANNESS_LITTLE : PDMAUDIOENDIANNESS_BIG;
    return VINF_SUCCESS;
}

bool DrvAudioHlpStreamCfgIsValid(PPDMAUDIOSTREAMCFG pCfg)
{
    bool fValid = (   pCfg->cChannels == 1
                   || pCfg->cChannels == 2); /* Either stereo (2) or mono (1), per stream. */

    fValid |= (   pCfg->enmEndianness == PDMAUDIOENDIANNESS_LITTLE
               || pCfg->enmEndianness == PDMAUDIOENDIANNESS_BIG);

    fValid |= (   pCfg->enmDir == PDMAUDIODIR_IN
               || pCfg->enmDir == PDMAUDIODIR_OUT);

    if (fValid)
    {
        switch (pCfg->enmFormat)
        {
            case PDMAUDIOFMT_S8:
            case PDMAUDIOFMT_U8:
            case PDMAUDIOFMT_S16:
            case PDMAUDIOFMT_U16:
            case PDMAUDIOFMT_S32:
            case PDMAUDIOFMT_U32:
                break;
            default:
                fValid = false;
                break;
        }
    }

    /** @todo Check for defined frequencies supported. */
    fValid |= pCfg->uHz > 0;

    return fValid;
}

/**
 * Converts an audio stream configuration to matching PCM properties.
 *
 * @return  IPRT status code.
 * @param   pCfg                    Audio stream configuration to convert.
 * @param   pProps                  PCM properties to save result to.
 */
int DrvAudioHlpStreamCfgToProps(PPDMAUDIOSTREAMCFG pCfg, PPDMPCMPROPS pProps)
{
    AssertPtrReturn(pCfg,   VERR_INVALID_POINTER);
    AssertPtrReturn(pProps, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    int cBits = 8, cShift = 0;
    bool fSigned = false;

    switch (pCfg->enmFormat)
    {
        case PDMAUDIOFMT_S8:
            fSigned = true;
        case PDMAUDIOFMT_U8:
            break;

        case PDMAUDIOFMT_S16:
            fSigned = true;
        case PDMAUDIOFMT_U16:
            cBits = 16;
            cShift = 1;
            break;

        case PDMAUDIOFMT_S32:
            fSigned = true;
        case PDMAUDIOFMT_U32:
            cBits = 32;
            cShift = 2;
            break;

        default:
            AssertMsgFailed(("Unknown format %ld\n", pCfg->enmFormat));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        pProps->uHz         = pCfg->uHz;
        pProps->cBits       = cBits;
        pProps->fSigned     = fSigned;
        pProps->cChannels   = pCfg->cChannels;
        pProps->cShift      = (pCfg->cChannels == 2) + cShift;
        pProps->uAlign      = (1 << pProps->cShift) - 1;
        pProps->cbPerSec    = pProps->uHz << pProps->cShift;
        pProps->fSwapEndian = pCfg->enmEndianness != PDMAUDIOHOSTENDIANNESS;
    }

    return rc;
}

void DrvAudioHlpStreamCfgPrint(PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturnVoid(pCfg);

    LogFlowFunc(("uHz=%RU32, cChannels=%RU8, enmFormat=", pCfg->uHz, pCfg->cChannels));

    switch (pCfg->enmFormat)
    {
        case PDMAUDIOFMT_S8:
            LogFlow(("S8"));
            break;
        case PDMAUDIOFMT_U8:
            LogFlow(("U8"));
            break;
        case PDMAUDIOFMT_S16:
            LogFlow(("S16"));
            break;
        case PDMAUDIOFMT_U16:
            LogFlow(("U16"));
            break;
        case PDMAUDIOFMT_S32:
            LogFlow(("S32"));
            break;
        case PDMAUDIOFMT_U32:
            LogFlow(("U32"));
            break;
        default:
            LogFlow(("invalid(%d)", pCfg->enmFormat));
            break;
    }

    LogFlow((", endianness="));
    switch (pCfg->enmEndianness)
    {
        case PDMAUDIOENDIANNESS_LITTLE:
            LogFlow(("little\n"));
            break;
        case PDMAUDIOENDIANNESS_BIG:
            LogFlow(("big\n"));
            break;
        default:
            LogFlow(("invalid\n"));
            break;
    }
}

