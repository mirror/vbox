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
#include <iprt/alloc.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#define LOG_GROUP LOG_GROUP_DRV_AUDIO
#include <VBox/log.h>

#include <VBox/err.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/mm.h>

#include <ctype.h>
#include <stdlib.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#pragma pack(1)
/**
 * Structure for building up a .WAV file header.
 */
typedef struct AUDIOWAVFILEHDR
{
    uint32_t u32RIFF;
    uint32_t u32Size;
    uint32_t u32WAVE;

    uint32_t u32Fmt;
    uint32_t u32Size1;
    uint16_t u16AudioFormat;
    uint16_t u16NumChannels;
    uint32_t u32SampleRate;
    uint32_t u32ByteRate;
    uint16_t u16BlockAlign;
    uint16_t u16BitsPerSample;

    uint32_t u32ID2;
    uint32_t u32Size2;
} AUDIOWAVFILEHDR, *PAUDIOWAVFILEHDR;
#pragma pack()

/**
 * Structure for keeeping the internal .WAV file data
 */
typedef struct AUDIOWAVFILEDATA
{
    /** The file header/footer. */
    AUDIOWAVFILEHDR Hdr;
} AUDIOWAVFILEDATA, *PAUDIOWAVFILEDATA;

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
void DrvAudioHlpClearBuf(PPDMAUDIOPCMPROPS pPCMProps, void *pvBuf, size_t cbBuf, uint32_t cSamples)
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

/**
 * Allocates an audio device.
 *
 * @returns Newly allocated audio device, or NULL if failed.
 * @param   cbData              How much additional data (in bytes) should be allocated to provide
 *                              a (backend) specific area to store additional data.
 *                              Optional, can be 0.
 */
PPDMAUDIODEVICE DrvAudioHlpDeviceAlloc(size_t cbData)
{
    PPDMAUDIODEVICE pDev = (PPDMAUDIODEVICE)RTMemAllocZ(sizeof(PDMAUDIODEVICE));
    if (!pDev)
        return NULL;

    if (cbData)
    {
        pDev->pvData = RTMemAllocZ(cbData);
        if (!pDev->pvData)
        {
            RTMemFree(pDev);
            return NULL;
        }
    }

    pDev->cbData = cbData;

    pDev->cMaxInputChannels  = 0;
    pDev->cMaxOutputChannels = 0;

    return pDev;
}

/**
 * Frees an audio device.
 *
 * @param pDev                  Device to free.
 */
void DrvAudioHlpDeviceFree(PPDMAUDIODEVICE pDev)
{
    if (!pDev)
        return;

    Assert(pDev->cRefCount == 0);

    if (pDev->pvData)
    {
        Assert(pDev->cbData);

        RTMemFree(pDev->pvData);
        pDev->pvData = NULL;
    }

    RTMemFree(pDev);
    pDev = NULL;
}

/**
 * Duplicates an audio device entry.
 *
 * @returns Duplicated audio device entry on success, or NULL on failure.
 * @param   pDev                Audio device entry to duplicate.
 * @param   fCopyUserData       Whether to also copy the user data portion or not.
 */
PPDMAUDIODEVICE DrvAudioHlpDeviceDup(PPDMAUDIODEVICE pDev, bool fCopyUserData)
{
    AssertPtrReturn(pDev, NULL);

    PPDMAUDIODEVICE pDevDup = DrvAudioHlpDeviceAlloc(fCopyUserData ? pDev->cbData : 0);
    if (pDevDup)
    {
        memcpy(pDevDup, pDev, sizeof(PDMAUDIODEVICE));

        if (   fCopyUserData
            && pDevDup->cbData)
        {
            memcpy(pDevDup->pvData, pDev->pvData, pDevDup->cbData);
        }
        else
        {
            pDevDup->cbData = 0;
            pDevDup->pvData = NULL;
        }
    }

    return pDevDup;
}

/**
 * Initializes an audio device enumeration structure.
 *
 * @returns IPRT status code.
 * @param   pDevEnm             Device enumeration to initialize.
 */
int DrvAudioHlpDeviceEnumInit(PPDMAUDIODEVICEENUM pDevEnm)
{
    AssertPtrReturn(pDevEnm, VERR_INVALID_POINTER);

    RTListInit(&pDevEnm->lstDevices);
    pDevEnm->cDevices = 0;

    return VINF_SUCCESS;
}

/**
 * Frees audio device enumeration data.
 *
 * @param pDevEnm               Device enumeration to destroy.
 */
void DrvAudioHlpDeviceEnumFree(PPDMAUDIODEVICEENUM pDevEnm)
{
    if (!pDevEnm)
        return;

    PPDMAUDIODEVICE pDev, pDevNext;
    RTListForEachSafe(&pDevEnm->lstDevices, pDev, pDevNext, PDMAUDIODEVICE, Node)
    {
        RTListNodeRemove(&pDev->Node);

        DrvAudioHlpDeviceFree(pDev);

        pDevEnm->cDevices--;
    }

    /* Sanity. */
    Assert(RTListIsEmpty(&pDevEnm->lstDevices));
    Assert(pDevEnm->cDevices == 0);
}

/**
 * Adds an audio device to a device enumeration.
 *
 * @return IPRT status code.
 * @param  pDevEnm              Device enumeration to add device to.
 * @param  pDev                 Device to add. The pointer will be owned by the device enumeration  then.
 */
int DrvAudioHlpDeviceEnumAdd(PPDMAUDIODEVICEENUM pDevEnm, PPDMAUDIODEVICE pDev)
{
    AssertPtrReturn(pDevEnm, VERR_INVALID_POINTER);
    AssertPtrReturn(pDev,    VERR_INVALID_POINTER);

    RTListAppend(&pDevEnm->lstDevices, &pDev->Node);
    pDevEnm->cDevices++;

    return VINF_SUCCESS;
}

/**
 * Duplicates a device enumeration.
 *
 * @returns Duplicated device enumeration, or NULL on failure.
 *          Must be free'd with DrvAudioHlpDeviceEnumFree().
 * @param   pDevEnm             Device enumeration to duplicate.
 */
PPDMAUDIODEVICEENUM DrvAudioHlpDeviceEnumDup(PPDMAUDIODEVICEENUM pDevEnm)
{
    AssertPtrReturn(pDevEnm, NULL);

    PPDMAUDIODEVICEENUM pDevEnmDup = (PPDMAUDIODEVICEENUM)RTMemAlloc(sizeof(PDMAUDIODEVICEENUM));
    if (!pDevEnmDup)
        return NULL;

    int rc2 = DrvAudioHlpDeviceEnumInit(pDevEnmDup);
    AssertRC(rc2);

    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->lstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        PPDMAUDIODEVICE pDevDup = DrvAudioHlpDeviceDup(pDev, true /* fCopyUserData */);
        if (!pDevDup)
        {
            rc2 = VERR_NO_MEMORY;
            break;
        }

        rc2 = DrvAudioHlpDeviceEnumAdd(pDevEnmDup, pDevDup);
        if (RT_FAILURE(rc2))
        {
            DrvAudioHlpDeviceFree(pDevDup);
            break;
        }
    }

    if (RT_FAILURE(rc2))
    {
        DrvAudioHlpDeviceEnumFree(pDevEnmDup);
        pDevEnmDup = NULL;
    }

    return pDevEnmDup;
}

/**
 * Copies device enumeration entries from the source to the destination enumeration.
 *
 * @returns IPRT status code.
 * @param   pDstDevEnm          Destination enumeration to store enumeration entries into.
 * @param   pSrcDevEnm          Source enumeration to use.
 * @param   enmUsage            Which entries to copy. Specify PDMAUDIODIR_ANY to copy all entries.
 * @param   fCopyUserData       Whether to also copy the user data portion or not.
 */
int DrvAudioHlpDeviceEnumCopyEx(PPDMAUDIODEVICEENUM pDstDevEnm, PPDMAUDIODEVICEENUM pSrcDevEnm,
                                PDMAUDIODIR enmUsage, bool fCopyUserData)
{
    AssertPtrReturn(pDstDevEnm, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrcDevEnm, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PPDMAUDIODEVICE pSrcDev;
    RTListForEach(&pSrcDevEnm->lstDevices, pSrcDev, PDMAUDIODEVICE, Node)
    {
        if (   enmUsage != PDMAUDIODIR_ANY
            && enmUsage != pSrcDev->enmUsage)
        {
            continue;
        }

        PPDMAUDIODEVICE pDstDev = DrvAudioHlpDeviceDup(pSrcDev, fCopyUserData);
        if (!pDstDev)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = DrvAudioHlpDeviceEnumAdd(pDstDevEnm, pDstDev);
        if (RT_FAILURE(rc))
            break;
    }

    return rc;
}

/**
 * Copies all device enumeration entries from the source to the destination enumeration.
 *
 * Note: Does *not* copy the user-specific data assigned to a device enumeration entry.
 *       To do so, use DrvAudioHlpDeviceEnumCopyEx().
 *
 * @returns IPRT status code.
 * @param   pDstDevEnm          Destination enumeration to store enumeration entries into.
 * @param   pSrcDevEnm          Source enumeration to use.
 */
int DrvAudioHlpDeviceEnumCopy(PPDMAUDIODEVICEENUM pDstDevEnm, PPDMAUDIODEVICEENUM pSrcDevEnm)
{
    return DrvAudioHlpDeviceEnumCopyEx(pDstDevEnm, pSrcDevEnm, PDMAUDIODIR_ANY, false /* fCopyUserData */);
}

/**
 * Returns the default device of a given device enumeration.
 * This assumes that only one default device per usage is set.
 *
 * @returns Default device if found, or NULL if none found.
 * @param   pDevEnm             Device enumeration to get default device for.
 * @param   enmUsage            Usage to get default device for.
 */
PPDMAUDIODEVICE DrvAudioHlpDeviceEnumGetDefaultDevice(PPDMAUDIODEVICEENUM pDevEnm, PDMAUDIODIR enmUsage)
{
    AssertPtrReturn(pDevEnm, NULL);

    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->lstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        if (enmUsage != PDMAUDIODIR_ANY)
        {
            if (enmUsage != pDev->enmUsage) /* Wrong usage? Skip. */
                continue;
        }

        if (pDev->fFlags & PDMAUDIODEV_FLAGS_DEFAULT)
            return pDev;
    }

    return NULL;
}

/**
 * Logs an audio device enumeration.
 *
 * @param  pszDesc              Logging description.
 * @param  pDevEnm              Device enumeration to log.
 */
void DrvAudioHlpDeviceEnumPrint(const char *pszDesc, PPDMAUDIODEVICEENUM pDevEnm)
{
    AssertPtrReturnVoid(pszDesc);
    AssertPtrReturnVoid(pDevEnm);

    LogFunc(("%s: %RU16 devices\n", pszDesc, pDevEnm->cDevices));

    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->lstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        char *pszFlags = DrvAudioHlpAudDevFlagsToStrA(pDev->fFlags);

        LogFunc(("Device '%s':\n", pDev->szName));
        LogFunc(("\tUsage           = %s\n",             DrvAudioHlpAudDirToStr(pDev->enmUsage)));
        LogFunc(("\tFlags           = %s\n",             pszFlags ? pszFlags : "<NONE>"));
        LogFunc(("\tInput channels  = %RU8\n",           pDev->cMaxInputChannels));
        LogFunc(("\tOutput channels = %RU8\n",           pDev->cMaxOutputChannels));
        LogFunc(("\tData            = %p (%zu bytes)\n", pDev->pvData, pDev->cbData));

        if (pszFlags)
            RTStrFree(pszFlags);
    }
}

/**
 * Converts an audio direction to a string.
 *
 * @returns Stringified audio direction, or "Unknown", if not found.
 * @param   enmDir              Audio direction to convert.
 */
const char *DrvAudioHlpAudDirToStr(PDMAUDIODIR enmDir)
{
    switch (enmDir)
    {
        case PDMAUDIODIR_UNKNOWN: return "Unknown";
        case PDMAUDIODIR_IN:      return "Input";
        case PDMAUDIODIR_OUT:     return "Output";
        case PDMAUDIODIR_ANY:     return "Duplex";
        default:                  break;
    }

    AssertMsgFailed(("Invalid audio direction %ld\n", enmDir));
    return "Unknown";
}

/**
 * Converts an audio device flags to a string.
 *
 * @returns Stringified audio flags. Must be free'd with RTStrFree().
 *          NULL if no flags set.
 * @param   fFlags              Audio flags to convert.
 */
char *DrvAudioHlpAudDevFlagsToStrA(PDMAUDIODEVFLAG fFlags)
{

#define APPEND_FLAG_TO_STR(_aFlag)              \
    if (fFlags & PDMAUDIODEV_FLAGS_##_aFlag)    \
    {                                           \
        if (pszFlags)                           \
        {                                       \
            rc2 = RTStrAAppend(&pszFlags, " "); \
            if (RT_FAILURE(rc2))                \
                break;                          \
        }                                       \
                                                \
        rc2 = RTStrAAppend(&pszFlags, #_aFlag); \
        if (RT_FAILURE(rc2))                    \
            break;                              \
    }                                           \

    char *pszFlags = NULL;
    int rc2 = VINF_SUCCESS;

    do
    {
        APPEND_FLAG_TO_STR(DEFAULT);
        APPEND_FLAG_TO_STR(HOTPLUG);
        APPEND_FLAG_TO_STR(BUGGY);
        APPEND_FLAG_TO_STR(IGNORE);
        APPEND_FLAG_TO_STR(LOCKED);
        APPEND_FLAG_TO_STR(DEAD);

    } while (0);

    if (   RT_FAILURE(rc2)
        && pszFlags)
    {
        RTStrFree(pszFlags);
        pszFlags = NULL;
    }

#undef APPEND_FLAG_TO_STR

    return pszFlags;
}

/**
 * Converts a recording source enumeration to a string.
 *
 * @returns Stringified recording source, or "Unknown", if not found.
 * @param   enmRecSrc           Recording source to convert.
 */
const char *DrvAudioHlpRecSrcToStr(PDMAUDIORECSOURCE enmRecSrc)
{
    switch (enmRecSrc)
    {
        case PDMAUDIORECSOURCE_UNKNOWN: return "Unknown";
        case PDMAUDIORECSOURCE_MIC:     return "Microphone In";
        case PDMAUDIORECSOURCE_CD:      return "CD";
        case PDMAUDIORECSOURCE_VIDEO:   return "Video";
        case PDMAUDIORECSOURCE_AUX:     return "AUX";
        case PDMAUDIORECSOURCE_LINE:    return "Line In";
        case PDMAUDIORECSOURCE_PHONE:   return "Phone";
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

/**
 * Converts an audio format to a string.
 *
 * @returns Stringified audio format, or "Unknown", if not found.
 * @param   enmFmt              Audio format to convert.
 */
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
    return "Unknown";
}

/**
 * Converts a given string to an audio format.
 *
 * @returns Audio format for the given string, or PDMAUDIOFMT_INVALID if not found.
 * @param   pszFmt              String to convert to an audio format.
 */
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

    AssertMsgFailed(("Invalid audio format '%s'\n", pszFmt));
    return PDMAUDIOFMT_INVALID;
}

/**
 * Checks whether the given PCM properties are equal with the given
 * stream configuration.
 *
 * @returns @true if equal, @false if not.
 * @param   pProps              PCM properties to compare.
 * @param   pCfg                Stream configuration to compare.
 */
bool DrvAudioHlpPCMPropsAreEqual(PPDMAUDIOPCMPROPS pProps, PPDMAUDIOSTREAMCFG pCfg)
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

/**
 * Checks whether two given PCM properties are equal.
 *
 * @returns @true if equal, @false if not.
 * @param   pProps1             First properties to compare.
 * @param   pProps2             Second properties to compare.
 */
bool DrvAudioHlpPCMPropsAreEqual(PPDMAUDIOPCMPROPS pProps1, PPDMAUDIOPCMPROPS pProps2)
{
    AssertPtrReturn(pProps1, false);
    AssertPtrReturn(pProps2, false);

    if (pProps1 == pProps2) /* If the pointers match, take a shortcut. */
        return true;

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
int DrvAudioHlpPCMPropsToStreamCfg(PPDMAUDIOPCMPROPS pPCMProps, PPDMAUDIOSTREAMCFG pCfg)
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

/**
 * Checks whether a given stream configuration is valid or not.
 *
 * Returns @true if configuration is valid, @false if not.
 * @param   pCfg                Stream configuration to check.
 */
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

    fValid |= pCfg->uHz > 0;
    /** @todo Check for defined frequencies supported. */

    return fValid;
}

/**
 * Converts an audio stream configuration to matching PCM properties.
 *
 * @return  IPRT status code.
 * @param   pCfg                    Audio stream configuration to convert.
 * @param   pProps                  PCM properties to save result to.
 */
int DrvAudioHlpStreamCfgToProps(PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOPCMPROPS pProps)
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
        pProps->cShift      = (pCfg->cChannels == 2) + cShift;
        pProps->cChannels   = pCfg->cChannels;
        pProps->uAlign      = (1 << pProps->cShift) - 1;
        pProps->fSwapEndian = pCfg->enmEndianness != PDMAUDIOHOSTENDIANNESS;
    }

    return rc;
}

/**
 * Prints an audio stream configuration to the debug log.
 *
 * @param   pCfg                Stream configuration to log.
 */
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

/**
 * Calculates the audio bit rate of the given bits per sample, the Hz and the number
 * of audio channels.
 *
 * Divide the result by 8 to get the byte rate.
 *
 * @returns The calculated bit rate.
 * @param   cBits               Number of bits per sample.
 * @param   uHz                 Hz (Hertz) rate.
 * @param   cChannels           Number of audio channels.
 */
uint32_t DrvAudioHlpCalcBitrate(uint8_t cBits, uint32_t uHz, uint8_t cChannels)
{
    return (cBits * uHz * cChannels);
}

/**
 * Calculates the audio bit rate out of a given audio stream configuration.
 *
 * Divide the result by 8 to get the byte rate.
 *
 * @returns The calculated bit rate.
 * @param   pCfg                Audio stream configuration to calculate bit rate for.
 *
 * @remark
 */
uint32_t DrvAudioHlpCalcBitrate(PPDMAUDIOSTREAMCFG pCfg)
{
    return DrvAudioHlpCalcBitrate(DrvAudioHlpAudFmtToBits(pCfg->enmFormat), pCfg->uHz, pCfg->cChannels);
}

/**
 * Sanitizes the file name component so that unsupported characters
 * will be replaced by an underscore ("_").
 *
 * @return  IPRT status code.
 * @param   pszPath             Path to sanitize.
 * @param   cbPath              Size (in bytes) of path to sanitize.
 */
int DrvAudioHlpSanitizeFileName(char *pszPath, size_t cbPath)
{
    RT_NOREF(cbPath);
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    /* Filter out characters not allowed on Windows platforms, put in by
       RTTimeSpecToString(). */
    /** @todo Use something like RTPathSanitize() if available later some time. */
    static RTUNICP const s_uszValidRangePairs[] =
    {
        ' ', ' ',
        '(', ')',
        '-', '.',
        '0', '9',
        'A', 'Z',
        'a', 'z',
        '_', '_',
        0xa0, 0xd7af,
        '\0'
    };
    ssize_t cReplaced = RTStrPurgeComplementSet(pszPath, s_uszValidRangePairs, '_' /* Replacement */);
    if (cReplaced < 0)
        rc = VERR_INVALID_UTF8_ENCODING;
#else
    RT_NOREF(pszPath);
#endif
    return rc;
}

/**
 * Constructs an unique file name, based on the given path and the audio file type.
 *
 * @returns IPRT status code.
 * @param   pszFile             Where to store the constructed file name.
 * @param   cchFile             Size (in characters) of the file name buffer.
 * @param   pszPath             Base path to use.
 * @param   pszName             A name for better identifying the file. Optional.
 * @param   enmType             Audio file type to construct file name for.
 */
int DrvAudioHlpGetFileName(char *pszFile, size_t cchFile, const char *pszPath, const char *pszName, PDMAUDIOFILETYPE enmType)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(cchFile,    VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    /* pszName is optional. */

    int rc;

    do
    {
        char szFilePath[RTPATH_MAX];
        RTStrPrintf(szFilePath, sizeof(szFilePath), "%s", pszPath);

        /* Create it when necessary. */
        if (!RTDirExists(szFilePath))
        {
            rc = RTDirCreateFullPath(szFilePath, RTFS_UNIX_IRWXU);
            if (RT_FAILURE(rc))
                break;
        }

        /* The actually drop directory consist of the current time stamp and a
         * unique number when necessary. */
        char pszTime[64];
        RTTIMESPEC time;
        if (!RTTimeSpecToString(RTTimeNow(&time), pszTime, sizeof(pszTime)))
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        rc = DrvAudioHlpSanitizeFileName(pszTime, sizeof(pszTime));
        if (RT_FAILURE(rc))
            break;

        rc = RTPathAppend(szFilePath, sizeof(szFilePath), pszTime);
        if (RT_FAILURE(rc))
            break;

        if (pszName) /* Optional name given? */
        {
            rc = RTStrCat(szFilePath, sizeof(szFilePath), "-");
            if (RT_FAILURE(rc))
                break;

            rc = RTStrCat(szFilePath, sizeof(szFilePath), pszName);
            if (RT_FAILURE(rc))
                break;
        }

        switch (enmType)
        {
            case PDMAUDIOFILETYPE_WAV:
                rc = RTStrCat(szFilePath, sizeof(szFilePath), ".wav");
                break;

            default:
                AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
        }

        if (RT_FAILURE(rc))
            break;

        RTStrPrintf(pszFile, cchFile, "%s", szFilePath);

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Opens or creates a wave (.WAV) file.
 *
 * @returns IPRT status code.
 * @param   pFile               Pointer to audio file handle to use.
 * @param   pszFile             File path of file to open or create.
 * @param   fOpen               Open flags.
 * @param   pProps              PCM properties to use.
 * @param   fFlags              Audio file flags.
 */
int DrvAudioHlpWAVFileOpen(PPDMAUDIOFILE pFile, const char *pszFile, uint32_t fOpen, PPDMAUDIOPCMPROPS pProps,
                           PDMAUDIOFILEFLAGS fFlags)
{
    AssertPtrReturn(pFile,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    /** @todo Validate fOpen flags. */
    AssertPtrReturn(pProps,  VERR_INVALID_POINTER);
    RT_NOREF(fFlags); /** @todo Validate fFlags flags. */

    Assert(pProps->cChannels);
    Assert(pProps->uHz);
    Assert(pProps->cBits);

    pFile->pvData = (PAUDIOWAVFILEDATA)RTMemAllocZ(sizeof(AUDIOWAVFILEDATA));
    if (!pFile->pvData)
        return VERR_NO_MEMORY;
    pFile->cbData = sizeof(PAUDIOWAVFILEDATA);

    PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
    AssertPtr(pData);

    /* Header. */
    pData->Hdr.u32RIFF          = AUDIO_MAKE_FOURCC('R','I','F','F');
    pData->Hdr.u32Size          = 36;
    pData->Hdr.u32WAVE          = AUDIO_MAKE_FOURCC('W','A','V','E');

    pData->Hdr.u32Fmt           = AUDIO_MAKE_FOURCC('f','m','t',' ');
    pData->Hdr.u32Size1         = 16; /* Means PCM. */
    pData->Hdr.u16AudioFormat   = 1;  /* PCM, linear quantization. */
    pData->Hdr.u16NumChannels   = pProps->cChannels;
    pData->Hdr.u32SampleRate    = pProps->uHz;
    pData->Hdr.u32ByteRate      = DrvAudioHlpCalcBitrate(pProps->cBits, pProps->uHz, pProps->cChannels) / 8;
    pData->Hdr.u16BlockAlign    = pProps->cChannels * pProps->cBits / 8;
    pData->Hdr.u16BitsPerSample = pProps->cBits;

    /* Data chunk. */
    pData->Hdr.u32ID2           = AUDIO_MAKE_FOURCC('d','a','t','a');
    pData->Hdr.u32Size2         = 0;

    int rc = RTFileOpen(&pFile->hFile, pszFile, fOpen);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(pFile->hFile, &pData->Hdr, sizeof(pData->Hdr), NULL);
        if (RT_FAILURE(rc))
        {
            RTFileClose(pFile->hFile);
            pFile->hFile = NIL_RTFILE;
        }
    }

    if (RT_SUCCESS(rc))
    {
        pFile->enmType = PDMAUDIOFILETYPE_WAV;

        RTStrPrintf(pFile->szName, RT_ELEMENTS(pFile->szName), "%s", pszFile);
    }
    else
    {
        RTMemFree(pFile->pvData);
        pFile->pvData = NULL;
        pFile->cbData = 0;
    }

    return rc;
}

/**
 * Closes a wave (.WAV) audio file.
 *
 * @returns IPRT status code.
 * @param   pFile               Audio file handle to close.
 */
int DrvAudioHlpWAVFileClose(PPDMAUDIOFILE pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);

    Assert(pFile->enmType == PDMAUDIOFILETYPE_WAV);

    if (pFile->hFile != NIL_RTFILE)
    {
        PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
        AssertPtr(pData);

        /* Update the header with the current data size. */
        RTFileWriteAt(pFile->hFile, 0, &pData->Hdr, sizeof(pData->Hdr), NULL);

        RTFileClose(pFile->hFile);
        pFile->hFile = NIL_RTFILE;
    }

    if (pFile->pvData)
    {
        RTMemFree(pFile->pvData);
        pFile->pvData = NULL;
    }

    pFile->cbData  = 0;
    pFile->enmType = PDMAUDIOFILETYPE_UNKNOWN;

    return VINF_SUCCESS;
}

/**
 * Returns the raw PCM audio data size of a wave file.
 * This does *not* include file headers and other data which does
 * not belong to the actual PCM audio data.
 *
 * @returns Size (in bytes) of the raw PCM audio data.
 * @param   pFile               Audio file handle to retrieve the audio data size for.
 */
size_t DrvAudioHlpWAVFileGetDataSize(PPDMAUDIOFILE pFile)
{
    AssertPtrReturn(pFile, 0);

    Assert(pFile->enmType == PDMAUDIOFILETYPE_WAV);

    PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
    AssertPtr(pData);

    return pData->Hdr.u32Size2;
}

/**
 * Write PCM data to a wave (.WAV) file.
 *
 * @returns IPRT status code.
 * @param   pFile               Audio file handle to write PCM data to.
 * @param   pvBuf               Audio data to write.
 * @param   cbBuf               Size (in bytes) of audio data to write.
 * @param   fFlags              Additional write flags. Not being used at the moment and must be 0.
 */
int DrvAudioHlpWAVFileWrite(PPDMAUDIOFILE pFile, const void *pvBuf, size_t cbBuf, uint32_t fFlags)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);

    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER); /** @todo fFlags are currently not implemented. */

    Assert(pFile->enmType == PDMAUDIOFILETYPE_WAV);

    if (!cbBuf)
        return VINF_SUCCESS;

    PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
    AssertPtr(pData);

    int rc = RTFileWrite(pFile->hFile, pvBuf, cbBuf, NULL);
    if (RT_SUCCESS(rc))
    {
        pData->Hdr.u32Size  += (uint32_t)cbBuf;
        pData->Hdr.u32Size2 += (uint32_t)cbBuf;
    }

    return rc;
}

