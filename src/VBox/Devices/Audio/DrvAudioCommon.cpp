/* $Id$ */
/** @file
 * Intermedia audio driver, common routines.
 *
 * These are also used in the drivers which are bound to Main, e.g. the VRDE
 * or the video audio recording drivers.
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/mm.h>

#include <ctype.h>
#include <stdlib.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
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
AssertCompileSize(AUDIOWAVFILEHDR, 11*4);

/**
 * Structure for keeeping the internal .WAV file data
 */
typedef struct AUDIOWAVFILEDATA
{
    /** The file header/footer. */
    AUDIOWAVFILEHDR Hdr;
} AUDIOWAVFILEDATA, *PAUDIOWAVFILEDATA;



#if 0 /* unused, no header prototypes */

/**
 * Retrieves the matching PDMAUDIOFMT for the given bits + signing flag.
 *
 * @return  Matching PDMAUDIOFMT value.
 * @retval  PDMAUDIOFMT_INVALID if unsupported @a cBits value.
 *
 * @param   cBits       The number of bits in the audio format.
 * @param   fSigned     Whether the audio format is signed @c true or not.
 */
PDMAUDIOFMT DrvAudioAudFmtBitsToFormat(uint8_t cBits, bool fSigned)
{
    if (fSigned)
    {
        switch (cBits)
        {
            case 8:  return PDMAUDIOFMT_S8;
            case 16: return PDMAUDIOFMT_S16;
            case 32: return PDMAUDIOFMT_S32;
            default: AssertMsgFailedReturn(("Bogus audio bits %RU8\n", cBits), PDMAUDIOFMT_INVALID);
        }
    }
    else
    {
        switch (cBits)
        {
            case 8:  return PDMAUDIOFMT_U8;
            case 16: return PDMAUDIOFMT_U16;
            case 32: return PDMAUDIOFMT_U32;
            default: AssertMsgFailedReturn(("Bogus audio bits %RU8\n", cBits), PDMAUDIOFMT_INVALID);
        }
    }
}

/**
 * Returns an unique file name for this given audio connector instance.
 *
 * @return  Allocated file name. Must be free'd using RTStrFree().
 * @param   uInstance           Driver / device instance.
 * @param   pszPath             Path name of the file to delete. The path must exist.
 * @param   pszSuffix           File name suffix to use.
 */
char *DrvAudioDbgGetFileNameA(uint8_t uInstance, const char *pszPath, const char *pszSuffix)
{
    char szFileName[64];
    RTStrPrintf(szFileName, sizeof(szFileName), "drvAudio%RU8-%s", uInstance, pszSuffix);

    char szFilePath[RTPATH_MAX];
    int rc2 = RTStrCopy(szFilePath, sizeof(szFilePath), pszPath);
    AssertRC(rc2);
    rc2 = RTPathAppend(szFilePath, sizeof(szFilePath), szFileName);
    AssertRC(rc2);

    return RTStrDup(szFilePath);
}

#endif /* unused */

/**
 * Allocates an audio device.
 *
 * @returns Newly allocated audio device, or NULL on failure.
 * @param   cb      The total device structure size.   This must be at least the
 *                  size of PDMAUDIODEVICE.  The idea is that the caller extends
 *                  the PDMAUDIODEVICE structure and appends additional data
 *                  after it in its private structure.
 */
PPDMAUDIODEVICE PDMAudioDeviceAlloc(size_t cb)
{
    AssertReturn(cb >= sizeof(PDMAUDIODEVICE), NULL);
    AssertReturn(cb < _4M, NULL);

    PPDMAUDIODEVICE pDev = (PPDMAUDIODEVICE)RTMemAllocZ(RT_ALIGN_Z(cb, 64));
    if (pDev)
    {
        pDev->uMagic = PDMAUDIODEVICE_MAGIC;
        pDev->cbSelf = (uint32_t)cb;
        RTListInit(&pDev->Node);

        //pDev->cMaxInputChannels  = 0;
        //pDev->cMaxOutputChannels = 0;
    }
    return pDev;
}

/**
 * Frees an audio device allocated by PDMAudioDeviceAlloc.
 *
 * @param   pDev    The device to free.  NULL is ignored.
 */
void PDMAudioDeviceFree(PPDMAUDIODEVICE pDev)
{
    if (pDev)
    {
        Assert(pDev->uMagic == PDMAUDIODEVICE_MAGIC);
        Assert(pDev->cRefCount == 0);
        pDev->uMagic = PDMAUDIODEVICE_MAGIC_DEAD;
        pDev->cbSelf = 0;

        RTMemFree(pDev);
    }
}

/**
 * Duplicates an audio device entry.
 *
 * @returns Duplicated audio device entry on success, or NULL on failure.
 * @param   pDev            The audio device enum entry to duplicate.
 * @param   fCopyUserData   Whether to also copy the user data portion or not.
 */
PPDMAUDIODEVICE PDMAudioDeviceDup(PPDMAUDIODEVICE pDev, bool fCopyUserData)
{
    AssertPtrReturn(pDev, NULL);
    Assert(pDev->uMagic == PDMAUDIODEVICE_MAGIC);
    Assert(!fCopyUserData || !(pDev->fFlags & PDMAUDIODEV_FLAGS_NO_DUP));

    uint32_t cbToDup = fCopyUserData ? pDev->cbSelf : sizeof(PDMAUDIODEVICE);
    AssertReturn(cbToDup >= sizeof(*pDev), NULL);

    PPDMAUDIODEVICE pDevDup = PDMAudioDeviceAlloc(cbToDup);
    if (pDevDup)
    {
        memcpy(pDevDup, pDev, cbToDup);
        RTListInit(&pDevDup->Node);
        pDev->cbSelf = cbToDup;
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

    RTListInit(&pDevEnm->LstDevices);
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
    RTListForEachSafe(&pDevEnm->LstDevices, pDev, pDevNext, PDMAUDIODEVICE, Node)
    {
        RTListNodeRemove(&pDev->Node);

        PDMAudioDeviceFree(pDev);

        pDevEnm->cDevices--;
    }

    /* Sanity. */
    Assert(RTListIsEmpty(&pDevEnm->LstDevices));
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

    RTListAppend(&pDevEnm->LstDevices, &pDev->Node);
    pDevEnm->cDevices++;

    return VINF_SUCCESS;
}

#if 0 /* unused, duplicated by copy  */
/**
 * Duplicates a device enumeration.
 *
 * @returns Duplicated device enumeration, or NULL on failure.
 *          Must be free'd with DrvAudioHlpDeviceEnumFree().
 * @param   pDevEnm             Device enumeration to duplicate.
 */
PPDMAUDIODEVICEENUM DrvAudioHlpDeviceEnumDup(const PPDMAUDIODEVICEENUM pDevEnm)
{
    AssertPtrReturn(pDevEnm, NULL);

    PPDMAUDIODEVICEENUM pDevEnmDup = (PPDMAUDIODEVICEENUM)RTMemAlloc(sizeof(PDMAUDIODEVICEENUM));
    if (!pDevEnmDup)
        return NULL;

    int rc2 = DrvAudioHlpDeviceEnumInit(pDevEnmDup);
    AssertRC(rc2);

    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        PPDMAUDIODEVICE pDevDup = PDMAudioDeviceDup(pDev, true /* fCopyUserData */);
        if (!pDevDup)
        {
            rc2 = VERR_NO_MEMORY;
            break;
        }

        rc2 = DrvAudioHlpDeviceEnumAdd(pDevEnmDup, pDevDup);
        if (RT_FAILURE(rc2))
        {
            PDMAudioDeviceFree(pDevDup);
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
#endif /* unused */

/**
 * Copies device enumeration entries from the source to the destination enumeration.
 *
 * @returns IPRT status code.
 * @param   pDstDevEnm          Destination enumeration to store enumeration entries into.
 * @param   pSrcDevEnm          Source enumeration to use.
 * @param   enmUsage            The usage to match for copying.  Use
 *                              PDMAUDIODIR_INVALID to match all entries.
 * @param   fCopyUserData       Whether to also copy the user data portion or not.
 */
int DrvAudioHlpDeviceEnumCopyEx(PPDMAUDIODEVICEENUM pDstDevEnm, PCPDMAUDIODEVICEENUM pSrcDevEnm,
                                PDMAUDIODIR enmUsage, bool fCopyUserData)
{
    AssertPtrReturn(pDstDevEnm, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrcDevEnm, VERR_INVALID_POINTER);

    PPDMAUDIODEVICE pSrcDev;
    RTListForEach(&pSrcDevEnm->LstDevices, pSrcDev, PDMAUDIODEVICE, Node)
    {
        if (   enmUsage == pSrcDev->enmUsage
            || enmUsage == PDMAUDIODIR_INVALID /*all*/)
        {
            PPDMAUDIODEVICE pDstDev = PDMAudioDeviceDup(pSrcDev, fCopyUserData);
            AssertReturn(pDstDev, VERR_NO_MEMORY);

            int rc = DrvAudioHlpDeviceEnumAdd(pDstDevEnm, pDstDev);
            AssertRCReturnStmt(rc, PDMAudioDeviceFree(pDstDev), VERR_NO_MEMORY);
        }
    }

    return VINF_SUCCESS;
}

/**
 * Copies all device enumeration entries from the source to the destination enumeration.
 *
 * @note Does *not* copy the user-specific data assigned to a device enumeration
 *       entry. To do so, use DrvAudioHlpDeviceEnumCopyEx().
 *
 * @returns IPRT status code.
 * @param   pDstDevEnm          Destination enumeration to store enumeration entries into.
 * @param   pSrcDevEnm          Source enumeration to use.
 */
int DrvAudioHlpDeviceEnumCopy(PPDMAUDIODEVICEENUM pDstDevEnm, PCPDMAUDIODEVICEENUM pSrcDevEnm)
{
    return DrvAudioHlpDeviceEnumCopyEx(pDstDevEnm, pSrcDevEnm, PDMAUDIODIR_INVALID /*all*/, false /* fCopyUserData */);
}

/**
 * Get the default device with the given usage.
 *
 * This assumes that only one default device per usage is set, if there should
 * be more than one, the first one is returned.
 *
 * @returns Default device if found, or NULL if not.
 * @param   pDevEnm     Device enumeration to get default device for.
 * @param   enmUsage    Usage to get default device for.
 *                      Pass PDMAUDIODIR_INVALID to get the first device with
 *                      PDMAUDIODEV_FLAGS_DEFAULT set.
 */
PPDMAUDIODEVICE DrvAudioHlpDeviceEnumGetDefaultDevice(PCPDMAUDIODEVICEENUM pDevEnm, PDMAUDIODIR enmUsage)
{
    AssertPtrReturn(pDevEnm, NULL);

    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        if (pDev->fFlags & PDMAUDIODEV_FLAGS_DEFAULT)
        {
            if (   enmUsage == pDev->enmUsage
                || enmUsage  == PDMAUDIODIR_INVALID)
                return pDev;
        }
    }

    return NULL;
}

/**
 * Get the number of device with the given usage.
 *
 * @returns Number of matching devices.
 * @param   pDevEnm     Device enumeration to get default device for.
 * @param   enmUsage    Usage to count devices for.
 *                      Pass PDMAUDIODIR_INVALID to get the total number of devices.
 */
uint32_t DrvAudioHlpDeviceEnumGetDeviceCount(PCPDMAUDIODEVICEENUM pDevEnm, PDMAUDIODIR enmUsage)
{
    AssertPtrReturn(pDevEnm, 0);

    if (enmUsage == PDMAUDIODIR_INVALID)
        return pDevEnm->cDevices;

    uint32_t        cDevs = 0;
    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        if (enmUsage == pDev->enmUsage)
            cDevs++;
    }

    return cDevs;
}

/**
 * Logs an audio device enumeration.
 *
 * @param  pDevEnm  Device enumeration to log.
 * @param  pszDesc  Logging description (prefix).
 */
void DrvAudioHlpDeviceEnumLog(PCPDMAUDIODEVICEENUM pDevEnm, const char *pszDesc)
{
    AssertPtrReturnVoid(pszDesc);
    AssertPtrReturnVoid(pDevEnm);

    LogFunc(("%s: %RU32 devices\n", pszDesc, pDevEnm->cDevices));

    PPDMAUDIODEVICE pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIODEVICE, Node)
    {
        char *pszFlags = DrvAudioHlpAudDevFlagsToStrA(pDev->fFlags);

        LogFunc(("Device '%s':\n", pDev->szName));
        LogFunc(("  Usage           = %s\n",             PDMAudioDirGetName(pDev->enmUsage)));
        LogFunc(("  Flags           = %s\n",             pszFlags ? pszFlags : "<NONE>"));
        LogFunc(("  Input channels  = %RU8\n",           pDev->cMaxInputChannels));
        LogFunc(("  Output channels = %RU8\n",           pDev->cMaxOutputChannels));
        LogFunc(("  cbExtra         = %RU32 bytes\n",    pDev->cbSelf - sizeof(PDMAUDIODEVICE)));

        if (pszFlags)
            RTStrFree(pszFlags);
    }
}

/**
 * Converts an audio device flags to a string.
 *
 * @returns Stringified audio flags. Must be free'd with RTStrFree().
 *          NULL if no flags set.
 * @param   fFlags      Audio flags (PDMAUDIODEV_FLAGS_XXX) to convert.
 */
char *DrvAudioHlpAudDevFlagsToStrA(uint32_t fFlags)
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
        APPEND_FLAG_TO_STR(NO_DUP);

    } while (0);

    if (!pszFlags)
        rc2 = RTStrAAppend(&pszFlags, "NONE");

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
    if (!RTStrICmp(pszFmt, "u16"))
        return PDMAUDIOFMT_U16;
    if (!RTStrICmp(pszFmt, "u32"))
        return PDMAUDIOFMT_U32;
    if (!RTStrICmp(pszFmt, "s8"))
        return PDMAUDIOFMT_S8;
    if (!RTStrICmp(pszFmt, "s16"))
        return PDMAUDIOFMT_S16;
    if (!RTStrICmp(pszFmt, "s32"))
        return PDMAUDIOFMT_S32;

    AssertMsgFailed(("Invalid audio format '%s'\n", pszFmt));
    return PDMAUDIOFMT_INVALID;
}

/**
 * Checks whether a given stream configuration is valid or not.
 *
 * @note    See notes on DrvAudioHlpPcmPropsAreValid().
 *
 * Returns @c true if configuration is valid, @c false if not.
 * @param   pCfg                Stream configuration to check.
 */
bool DrvAudioHlpStreamCfgIsValid(PCPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg, false);

    AssertReturn(PDMAudioStrmCfgIsValid(pCfg), false);

    bool fValid = (   pCfg->enmDir == PDMAUDIODIR_IN
                   || pCfg->enmDir == PDMAUDIODIR_OUT);

    fValid &= (   pCfg->enmLayout == PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED
               || pCfg->enmLayout == PDMAUDIOSTREAMLAYOUT_RAW);

    if (fValid)
        fValid = DrvAudioHlpPcmPropsAreValid(&pCfg->Props);

    return fValid;
}

/**
 * Calculates the audio bit rate of the given bits per sample, the Hz and the number
 * of audio channels.
 *
 * Divide the result by 8 to get the byte rate.
 *
 * @returns Bitrate.
 * @param   cBits               Number of bits per sample.
 * @param   uHz                 Hz (Hertz) rate.
 * @param   cChannels           Number of audio channels.
 */
uint32_t DrvAudioHlpCalcBitrate(uint8_t cBits, uint32_t uHz, uint8_t cChannels)
{
    return cBits * uHz * cChannels;
}


/**
 * Checks whether given PCM properties are valid or not.
 *
 * @note  This is more of a supported than valid check.  There is code for
 *        unsigned samples elsewhere (like DrvAudioHlpClearBuf()), but this
 *        function will flag such properties as not valid.
 *
 * @todo  r=bird: See note and explain properly.
 *
 * @returns @c true if the properties are valid, @c false if not.
 * @param   pProps      The PCM properties to check.
 */
bool DrvAudioHlpPcmPropsAreValid(PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pProps, false);

    AssertReturn(PDMAudioPropsAreValid(pProps), false);

    /** @todo r=bird: This code is cannot make up its mind whether to return on
     *        false, or whether to return at the end. (hint: just return
     *        immediately, duh.) */

    /* Minimum 1 channel (mono), maximum 7.1 (= 8) channels. */
    bool fValid = (   pProps->cChannels >= 1
                   && pProps->cChannels <= 8);

    if (fValid)
    {
        switch (pProps->cbSample)
        {
            case 1: /* 8 bit */
               if (pProps->fSigned)
                   fValid = false;
               break;
            case 2: /* 16 bit */
                if (!pProps->fSigned)
                    fValid = false;
                break;
            /** @todo Do we need support for 24 bit samples? */
            case 4: /* 32 bit */
                if (!pProps->fSigned)
                    fValid = false;
                break;
            default:
                fValid = false;
                break;
        }
    }

    if (!fValid)
        return false;

    fValid &= pProps->uHz > 0;
    fValid &= pProps->cShift == PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pProps->cbSample, pProps->cChannels);
    fValid &= pProps->fSwapEndian == false; /** @todo Handling Big Endian audio data is not supported yet. */

    return fValid;
}


/*********************************************************************************************************************************
*   Audio File Helpers                                                                                                           *
*********************************************************************************************************************************/

/**
 * Sanitizes the file name component so that unsupported characters
 * will be replaced by an underscore ("_").
 *
 * @return  IPRT status code.
 * @param   pszPath             Path to sanitize.
 * @param   cbPath              Size (in bytes) of path to sanitize.
 */
int DrvAudioHlpFileNameSanitize(char *pszPath, size_t cbPath)
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
 *                              If NULL or empty, the system's temporary directory will be used.
 * @param   pszName             A name for better identifying the file.
 * @param   uInstance           Device / driver instance which is using this file.
 * @param   enmType             Audio file type to construct file name for.
 * @param   fFlags              File naming flags, PDMAUDIOFILENAME_FLAGS_XXX.
 */
int DrvAudioHlpFileNameGet(char *pszFile, size_t cchFile, const char *pszPath, const char *pszName,
                           uint32_t uInstance, PDMAUDIOFILETYPE enmType, uint32_t fFlags)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(cchFile,    VERR_INVALID_PARAMETER);
    /* pszPath can be NULL. */
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    /** @todo Validate fFlags. */

    int rc;

    char *pszPathTmp = NULL;

    do
    {
        if (   pszPath == NULL
            || !strlen(pszPath))
        {
            char szTemp[RTPATH_MAX];
            rc = RTPathTemp(szTemp, sizeof(szTemp));
            if (RT_SUCCESS(rc))
            {
                pszPathTmp = RTStrDup(szTemp);
            }
            else
                break;
        }
        else
            pszPathTmp = RTStrDup(pszPath);

        AssertPtrBreakStmt(pszPathTmp, rc = VERR_NO_MEMORY);

        char szFilePath[RTPATH_MAX];
        rc = RTStrCopy(szFilePath, sizeof(szFilePath), pszPathTmp);
        AssertRCBreak(rc);

        /* Create it when necessary. */
        if (!RTDirExists(szFilePath))
        {
            rc = RTDirCreateFullPath(szFilePath, RTFS_UNIX_IRWXU);
            if (RT_FAILURE(rc))
                break;
        }

        char szFileName[RTPATH_MAX];
        szFileName[0] = '\0';

        if (fFlags & PDMAUDIOFILENAME_FLAGS_TS)
        {
            RTTIMESPEC time;
            if (!RTTimeSpecToString(RTTimeNow(&time), szFileName, sizeof(szFileName)))
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            rc = DrvAudioHlpFileNameSanitize(szFileName, sizeof(szFileName));
            if (RT_FAILURE(rc))
                break;

            rc = RTStrCat(szFileName, sizeof(szFileName), "-");
            if (RT_FAILURE(rc))
                break;
        }

        rc = RTStrCat(szFileName, sizeof(szFileName), pszName);
        if (RT_FAILURE(rc))
            break;

        rc = RTStrCat(szFileName, sizeof(szFileName), "-");
        if (RT_FAILURE(rc))
            break;

        char szInst[16];
        RTStrPrintf2(szInst, sizeof(szInst), "%RU32", uInstance);
        rc = RTStrCat(szFileName, sizeof(szFileName), szInst);
        if (RT_FAILURE(rc))
            break;

        switch (enmType)
        {
            case PDMAUDIOFILETYPE_RAW:
                rc = RTStrCat(szFileName, sizeof(szFileName), ".pcm");
                break;

            case PDMAUDIOFILETYPE_WAV:
                rc = RTStrCat(szFileName, sizeof(szFileName), ".wav");
                break;

            default:
                AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
                break;
        }

        if (RT_FAILURE(rc))
            break;

        rc = RTPathAppend(szFilePath, sizeof(szFilePath), szFileName);
        if (RT_FAILURE(rc))
            break;

        rc = RTStrCopy(pszFile, cchFile, szFilePath);

    } while (0);

    RTStrFree(pszPathTmp);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates an audio file.
 *
 * @returns IPRT status code.
 * @param   enmType             Audio file type to open / create.
 * @param   pszFile             File path of file to open or create.
 * @param   fFlags              Audio file flags, PDMAUDIOFILE_FLAGS_XXX.
 * @param   ppFile              Where to store the created audio file handle.
 *                              Needs to be destroyed with DrvAudioHlpFileDestroy().
 */
int DrvAudioHlpFileCreate(PDMAUDIOFILETYPE enmType, const char *pszFile, uint32_t fFlags, PPDMAUDIOFILE *ppFile)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    /** @todo Validate fFlags. */

    PPDMAUDIOFILE pFile = (PPDMAUDIOFILE)RTMemAlloc(sizeof(PDMAUDIOFILE));
    if (!pFile)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    switch (enmType)
    {
        case PDMAUDIOFILETYPE_RAW:
        case PDMAUDIOFILETYPE_WAV:
            pFile->enmType = enmType;
            break;

        default:
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        RTStrPrintf(pFile->szName, RT_ELEMENTS(pFile->szName), "%s", pszFile);
        pFile->hFile  = NIL_RTFILE;
        pFile->fFlags = fFlags;
        pFile->pvData = NULL;
        pFile->cbData = 0;
    }

    if (RT_FAILURE(rc))
    {
        RTMemFree(pFile);
        pFile = NULL;
    }
    else
        *ppFile = pFile;

    return rc;
}

/**
 * Destroys a formerly created audio file.
 *
 * @param   pFile               Audio file (object) to destroy.
 */
void DrvAudioHlpFileDestroy(PPDMAUDIOFILE pFile)
{
    if (!pFile)
        return;

    DrvAudioHlpFileClose(pFile);

    RTMemFree(pFile);
    pFile = NULL;
}

/**
 * Opens or creates an audio file.
 *
 * @returns IPRT status code.
 * @param   pFile               Pointer to audio file handle to use.
 * @param   fOpen               Open flags.
 *                              Use PDMAUDIOFILE_DEFAULT_OPEN_FLAGS for the default open flags.
 * @param   pProps              PCM properties to use.
 */
int DrvAudioHlpFileOpen(PPDMAUDIOFILE pFile, uint32_t fOpen, PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pFile,   VERR_INVALID_POINTER);
    /** @todo Validate fOpen flags. */
    AssertPtrReturn(pProps,  VERR_INVALID_POINTER);

    int rc;

    if (pFile->enmType == PDMAUDIOFILETYPE_RAW)
    {
        rc = RTFileOpen(&pFile->hFile, pFile->szName, fOpen);
    }
    else if (pFile->enmType == PDMAUDIOFILETYPE_WAV)
    {
        Assert(pProps->cChannels);
        Assert(pProps->uHz);
        Assert(pProps->cbSample);

        pFile->pvData = (PAUDIOWAVFILEDATA)RTMemAllocZ(sizeof(AUDIOWAVFILEDATA));
        if (pFile->pvData)
        {
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
            pData->Hdr.u32ByteRate      = PDMAudioPropsGetBitrate(pProps) / 8;
            pData->Hdr.u16BlockAlign    = pProps->cChannels * pProps->cbSample;
            pData->Hdr.u16BitsPerSample = pProps->cbSample * 8;

            /* Data chunk. */
            pData->Hdr.u32ID2           = AUDIO_MAKE_FOURCC('d','a','t','a');
            pData->Hdr.u32Size2         = 0;

            rc = RTFileOpen(&pFile->hFile, pFile->szName, fOpen);
            if (RT_SUCCESS(rc))
            {
                rc = RTFileWrite(pFile->hFile, &pData->Hdr, sizeof(pData->Hdr), NULL);
                if (RT_FAILURE(rc))
                {
                    RTFileClose(pFile->hFile);
                    pFile->hFile = NIL_RTFILE;
                }
            }

            if (RT_FAILURE(rc))
            {
                RTMemFree(pFile->pvData);
                pFile->pvData = NULL;
                pFile->cbData = 0;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    if (RT_SUCCESS(rc))
    {
        LogRel2(("Audio: Opened file '%s'\n", pFile->szName));
    }
    else
        LogRel(("Audio: Failed opening file '%s', rc=%Rrc\n", pFile->szName, rc));

    return rc;
}

/**
 * Closes an audio file.
 *
 * @returns IPRT status code.
 * @param   pFile               Audio file handle to close.
 */
int DrvAudioHlpFileClose(PPDMAUDIOFILE pFile)
{
    if (!pFile)
        return VINF_SUCCESS;

    size_t cbSize = DrvAudioHlpFileGetDataSize(pFile);

    int rc = VINF_SUCCESS;

    if (pFile->enmType == PDMAUDIOFILETYPE_RAW)
    {
        if (RTFileIsValid(pFile->hFile))
            rc = RTFileClose(pFile->hFile);
    }
    else if (pFile->enmType == PDMAUDIOFILETYPE_WAV)
    {
        if (RTFileIsValid(pFile->hFile))
        {
            PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
            if (pData) /* The .WAV file data only is valid when a file actually has been created. */
            {
                /* Update the header with the current data size. */
                RTFileWriteAt(pFile->hFile, 0, &pData->Hdr, sizeof(pData->Hdr), NULL);
            }

            rc = RTFileClose(pFile->hFile);
        }

        if (pFile->pvData)
        {
            RTMemFree(pFile->pvData);
            pFile->pvData = NULL;
        }
    }
    else
        AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

    if (   RT_SUCCESS(rc)
        && !cbSize
        && !(pFile->fFlags & PDMAUDIOFILE_FLAGS_KEEP_IF_EMPTY))
    {
        rc = DrvAudioHlpFileDelete(pFile);
    }

    pFile->cbData = 0;

    if (RT_SUCCESS(rc))
    {
        pFile->hFile = NIL_RTFILE;
        LogRel2(("Audio: Closed file '%s' (%zu bytes)\n", pFile->szName, cbSize));
    }
    else
        LogRel(("Audio: Failed closing file '%s', rc=%Rrc\n", pFile->szName, rc));

    return rc;
}

/**
 * Deletes an audio file.
 *
 * @returns IPRT status code.
 * @param   pFile               Audio file handle to delete.
 */
int DrvAudioHlpFileDelete(PPDMAUDIOFILE pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);

    int rc = RTFileDelete(pFile->szName);
    if (RT_SUCCESS(rc))
    {
        LogRel2(("Audio: Deleted file '%s'\n", pFile->szName));
    }
    else if (rc == VERR_FILE_NOT_FOUND) /* Don't bitch if the file is not around (anymore). */
        rc = VINF_SUCCESS;

    if (RT_FAILURE(rc))
        LogRel(("Audio: Failed deleting file '%s', rc=%Rrc\n", pFile->szName, rc));

    return rc;
}

/**
 * Returns the raw audio data size of an audio file.
 *
 * Note: This does *not* include file headers and other data which does
 *       not belong to the actual PCM audio data.
 *
 * @returns Size (in bytes) of the raw PCM audio data.
 * @param   pFile               Audio file handle to retrieve the audio data size for.
 */
size_t DrvAudioHlpFileGetDataSize(PPDMAUDIOFILE pFile)
{
    AssertPtrReturn(pFile, 0);

    size_t cbSize = 0;

    if (pFile->enmType == PDMAUDIOFILETYPE_RAW)
    {
        cbSize = RTFileTell(pFile->hFile);
    }
    else if (pFile->enmType == PDMAUDIOFILETYPE_WAV)
    {
        PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
        if (pData) /* The .WAV file data only is valid when a file actually has been created. */
            cbSize = pData->Hdr.u32Size2;
    }

    return cbSize;
}

/**
 * Returns whether the given audio file is open and in use or not.
 *
 * @return  bool                True if open, false if not.
 * @param   pFile               Audio file handle to check open status for.
 */
bool DrvAudioHlpFileIsOpen(PPDMAUDIOFILE pFile)
{
    if (!pFile)
        return false;

    return RTFileIsValid(pFile->hFile);
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
int DrvAudioHlpFileWrite(PPDMAUDIOFILE pFile, const void *pvBuf, size_t cbBuf, uint32_t fFlags)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);

    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER); /** @todo fFlags are currently not implemented. */

    if (!cbBuf)
        return VINF_SUCCESS;

    AssertReturn(RTFileIsValid(pFile->hFile), VERR_WRONG_ORDER);

    int rc;

    if (pFile->enmType == PDMAUDIOFILETYPE_RAW)
    {
        rc = RTFileWrite(pFile->hFile, pvBuf, cbBuf, NULL);
    }
    else if (pFile->enmType == PDMAUDIOFILETYPE_WAV)
    {
        PAUDIOWAVFILEDATA pData = (PAUDIOWAVFILEDATA)pFile->pvData;
        AssertPtr(pData);

        rc = RTFileWrite(pFile->hFile, pvBuf, cbBuf, NULL);
        if (RT_SUCCESS(rc))
        {
            pData->Hdr.u32Size  += (uint32_t)cbBuf;
            pData->Hdr.u32Size2 += (uint32_t)cbBuf;
        }
    }
    else
        rc = VERR_NOT_SUPPORTED;

    return rc;
}

