/* $Id$ */
/** @file
 * Audio helper routines.
 *
 * These are used with both drivers and devices.
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

#include "AudioHlp.h"
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
 * Checks whether a given stream configuration is valid or not.
 *
 * @note    See notes on AudioHlpPcmPropsAreValid().
 *
 * Returns @c true if configuration is valid, @c false if not.
 * @param   pCfg                Stream configuration to check.
 */
bool AudioHlpStreamCfgIsValid(PCPDMAUDIOSTREAMCFG pCfg)
{
    /* Ugly! HDA attach code calls us with uninitialized (all zero) config. */
    if (PDMAudioPropsHz(&pCfg->Props) != 0)
    {
        if (PDMAudioStrmCfgIsValid(pCfg))
        {
            if (   pCfg->enmDir == PDMAUDIODIR_IN
                || pCfg->enmDir == PDMAUDIODIR_OUT)
                return AudioHlpPcmPropsAreValid(&pCfg->Props);
        }
    }
    return false;
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
uint32_t AudioHlpCalcBitrate(uint8_t cBits, uint32_t uHz, uint8_t cChannels)
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
 * @todo  r=bird: See note and explain properly. Perhaps rename to
 *        AudioHlpPcmPropsAreValidAndSupported?
 *
 * @returns @c true if the properties are valid, @c false if not.
 * @param   pProps      The PCM properties to check.
 */
bool AudioHlpPcmPropsAreValid(PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pProps, false);
    AssertReturn(PDMAudioPropsAreValid(pProps), false);

    switch (PDMAudioPropsSampleSize(pProps))
    {
        case 1: /* 8 bit */
           if (PDMAudioPropsIsSigned(pProps))
               return false;
           break;
        case 2: /* 16 bit */
            if (!PDMAudioPropsIsSigned(pProps))
                return false;
            break;
        /** @todo Do we need support for 24 bit samples? */
        case 4: /* 32 bit */
            if (!PDMAudioPropsIsSigned(pProps))
                return false;
            break;
        case 8: /* 64-bit raw */
            if (   !PDMAudioPropsIsSigned(pProps)
                || !pProps->fRaw)
                return false;
            break;
        default:
            return false;
    }

    if (!pProps->fSwapEndian) /** @todo Handling Big Endian audio data is not supported yet. */
        return true;
    return false;
}


/*********************************************************************************************************************************
*   Audio File Helpers                                                                                                           *
*********************************************************************************************************************************/

/**
 * Constructs an unique file name, based on the given path and the audio file type.
 *
 * @returns VBox status code.
 * @param   pszDst      Where to store the constructed file name.
 * @param   cbDst       Size of the destination buffer (bytes; incl terminator).
 * @param   pszPath     Base path to use.  If NULL or empty, the user's
 *                      temporary directory will be used.
 * @param   pszNameFmt  A name for better identifying the file.
 * @param   va          Arguments for @a pszNameFmt.
 * @param   uInstance   Device / driver instance which is using this file.
 * @param   enmType     Audio file type to construct file name for.
 * @param   fFlags      File naming flags, AUDIOHLPFILENAME_FLAGS_XXX.
 * @param   chTweak     Retry tweak character.
 */
static int audioHlpConstructPathWorker(char *pszDst, size_t cbDst, const char *pszPath, const char *pszNameFmt, va_list va,
                                       uint32_t uInstance, AUDIOHLPFILETYPE enmType, uint32_t fFlags, char chTweak)
{
    /*
     * Validate input.
     */
    AssertPtrNullReturn(pszPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pszNameFmt, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~AUDIOHLPFILENAME_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    /* Validate the type and translate it into a suffix. */
    const char *pszSuffix = NULL;
    switch (enmType)
    {
        case AUDIOHLPFILETYPE_RAW: pszSuffix = ".pcm"; break;
        case AUDIOHLPFILETYPE_WAV: pszSuffix = ".wav"; break;
        case AUDIOHLPFILETYPE_INVALID:
        case AUDIOHLPFILETYPE_32BIT_HACK:
            break; /* no default */
    }
    AssertMsgReturn(pszSuffix, ("enmType=%d\n", enmType), VERR_INVALID_PARAMETER);

    /*
     * The directory.  Make sure it exists and ends with a path separator.
     */
    int rc;
    if (!pszPath || !*pszPath)
        rc = RTPathTemp(pszDst, cbDst);
    else
    {
        AssertPtrReturn(pszDst, VERR_INVALID_POINTER);
        rc = RTStrCopy(pszDst, cbDst, pszPath);
    }
    AssertRCReturn(rc, rc);

    if (!RTDirExists(pszDst))
    {
        rc = RTDirCreateFullPath(pszDst, RTFS_UNIX_IRWXU);
        AssertRCReturn(rc, rc);
    }

    size_t offDst = RTPathEnsureTrailingSeparator(pszDst, cbDst);
    AssertReturn(offDst > 0, VERR_BUFFER_OVERFLOW);
    Assert(offDst < cbDst);

    /*
     * The filename.
     */
    /* Start with a ISO timestamp w/ colons replaced by dashes if requested. */
    if (fFlags & AUDIOHLPFILENAME_FLAGS_TS)
    {
        RTTIMESPEC NowTimeSpec;
        RTTIME     NowUtc;
        AssertReturn(RTTimeToString(RTTimeExplode(&NowUtc, RTTimeNow(&NowTimeSpec)), &pszDst[offDst], cbDst - offDst),
                     VERR_BUFFER_OVERFLOW);

        /* Change the two colons in the time part to dashes.  */
        char *pchColon = &pszDst[offDst];
        while ((pchColon = strchr(pchColon, ':')) != NULL)
            *pchColon++ = '-';

        offDst += strlen(&pszDst[offDst]);
        Assert(pszDst[offDst - 1] == 'Z');

        /* Append a dash to separate the timestamp from the name. */
        AssertReturn(offDst + 2 <= cbDst, VERR_BUFFER_OVERFLOW);
        pszDst[offDst++] = '-';
        pszDst[offDst]   = '\0';
    }

    /* Append the filename, instance, retry-tweak and suffix. */
    va_list vaCopy;
    va_copy(vaCopy, va);
    ssize_t cchTail;
    if (chTweak == '\0')
        cchTail = RTStrPrintf2(&pszDst[offDst], cbDst - offDst, "%N-%u%s", pszNameFmt, &vaCopy, uInstance, pszSuffix);
    else
        cchTail = RTStrPrintf2(&pszDst[offDst], cbDst - offDst, "%N-%u%c%s", pszNameFmt, &vaCopy, uInstance, chTweak, pszSuffix);
    va_end(vaCopy);
    AssertReturn(cchTail > 0, VERR_BUFFER_OVERFLOW);

    return VINF_SUCCESS;
}


/**
 * Worker for AudioHlpFileCreateF and AudioHlpFileCreateAndOpenEx that allocates
 * and initializes a AUDIOHLPFILE instance.
 */
static int audioHlpFileCreateWorker(PAUDIOHLPFILE *ppFile, uint32_t fFlags, AUDIOHLPFILETYPE enmType, const char *pszPath)
{
    AssertReturn(!(fFlags & ~AUDIOHLPFILE_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    size_t const cbPath = strlen(pszPath) + 1;
    PAUDIOHLPFILE pFile = (PAUDIOHLPFILE)RTMemAllocVar(RT_UOFFSETOF_DYN(AUDIOHLPFILE, szName[cbPath]));
    AssertPtrReturn(pFile, VERR_NO_MEMORY);

    pFile->enmType = enmType;
    pFile->fFlags  = fFlags;
    pFile->hFile   = NIL_RTFILE;
    pFile->pvData  = NULL;
    pFile->cbData  = 0;
    memcpy(pFile->szName, pszPath, cbPath);

    *ppFile = pFile;
    return VINF_SUCCESS;
}


/**
 * Creates an instance of AUDIOHLPFILE with the given filename and type.
 *
 * @note This does <b>NOT</b> create the file, see AudioHlpFileOpen for that.
 *
 * @returns VBox status code.
 * @param   ppFile      Where to return the pointer to the audio debug file
 *                      instance on success.
 * @param   fFlags      AUDIOHLPFILE_FLAGS_XXX.
 * @param   enmType     The audio file type to produce.
 * @param   pszPath     The directory path.  The temporary directory will be
 *                      used if NULL or empty.
 * @param   fFilename   AUDIOHLPFILENAME_FLAGS_XXX.
 * @param   uInstance   The instance number (will be appended to the filename
 *                      with a dash inbetween).
 * @param   pszNameFmt  The filename format string.
 * @param   ...         Arguments to the filename format string.
 */
int AudioHlpFileCreateF(PAUDIOHLPFILE *ppFile, uint32_t fFlags, AUDIOHLPFILETYPE enmType,
                        const char *pszPath, uint32_t fFilename, uint32_t uInstance, const char *pszNameFmt, ...)
{
    *ppFile = NULL;

    /*
     * Construct the filename first.
     */
    char szPath[RTPATH_MAX];
    va_list va;
    va_start(va, pszNameFmt);
    int rc = audioHlpConstructPathWorker(szPath, sizeof(szPath), pszPath, pszNameFmt, va, uInstance, enmType, fFilename, '\0');
    va_end(va);
    AssertRCReturn(rc, rc);

    /*
     * Allocate and initializes a debug file instance with that filename path.
     */
    return audioHlpFileCreateWorker(ppFile, fFlags, enmType, szPath);
}


/**
 * Destroys a formerly created audio file.
 *
 * @param   pFile               Audio file (object) to destroy.
 */
void AudioHlpFileDestroy(PAUDIOHLPFILE pFile)
{
    if (!pFile)
        return;

    AudioHlpFileClose(pFile);

    RTMemFree(pFile);
    pFile = NULL;
}


/**
 * Opens or creates an audio file.
 *
 * @returns VBox status code.
 * @param   pFile               Pointer to audio file handle to use.
 * @param   fOpen               Open flags.
 *                              Use AUDIOHLPFILE_DEFAULT_OPEN_FLAGS for the default open flags.
 * @param   pProps              PCM properties to use.
 */
int AudioHlpFileOpen(PAUDIOHLPFILE pFile, uint32_t fOpen, PCPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pFile,   VERR_INVALID_POINTER);
    /** @todo Validate fOpen flags. */
    AssertPtrReturn(pProps,  VERR_INVALID_POINTER);
    Assert(PDMAudioPropsAreValid(pProps));

    int rc;

    if (pFile->enmType == AUDIOHLPFILETYPE_RAW)
        rc = RTFileOpen(&pFile->hFile, pFile->szName, fOpen);
    else if (pFile->enmType == AUDIOHLPFILETYPE_WAV)
    {
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
            pData->Hdr.u16NumChannels   = PDMAudioPropsChannels(pProps);
            pData->Hdr.u32SampleRate    = pProps->uHz;
            pData->Hdr.u32ByteRate      = PDMAudioPropsGetBitrate(pProps) / 8;
            pData->Hdr.u16BlockAlign    = PDMAudioPropsFrameSize(pProps);
            pData->Hdr.u16BitsPerSample = PDMAudioPropsSampleBits(pProps);

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
        LogRel2(("Audio: Opened file '%s'\n", pFile->szName));
    else
        LogRel(("Audio: Failed opening file '%s', rc=%Rrc\n", pFile->szName, rc));

    return rc;
}


/**
 * Creates a debug file structure and opens a file for it, extended version.
 *
 * @returns VBox status code.
 * @param   ppFile      Where to return the debug file instance on success.
 * @param   enmType     The file type.
 * @param   pszDir      The directory to open the file in.
 * @param   iInstance   The device/driver instance.
 * @param   fFilename   AUDIOHLPFILENAME_FLAGS_XXX.
 * @param   fCreate     AUDIOHLPFILE_FLAGS_XXX.
 * @param   pProps      PCM audio properties for the file.
 * @param   fOpen       RTFILE_O_XXX or AUDIOHLPFILE_DEFAULT_OPEN_FLAGS.
 * @param   pszNameFmt  The base filename.
 * @param   ...         Filename format arguments.
 */
int AudioHlpFileCreateAndOpenEx(PAUDIOHLPFILE *ppFile, AUDIOHLPFILETYPE enmType, const char *pszDir,
                                uint32_t iInstance, uint32_t fFilename, uint32_t fCreate,
                                PCPDMAUDIOPCMPROPS pProps, uint64_t fOpen, const char *pszNameFmt, ...)
{
    *ppFile = NULL;

    for (uint32_t iTry = 0; ; iTry++)
    {
        /* Format the path to the filename. */
        char szFile[RTPATH_MAX];
        va_list va;
        va_start(va, pszNameFmt);
        int rc = audioHlpConstructPathWorker(szFile, sizeof(szFile), pszDir, pszNameFmt, va, iInstance, enmType, fFilename,
                                             iTry == 0 ? '\0' : iTry + 'a');
        va_end(va);
        AssertRCReturn(rc, rc);

        /* Create an debug audio file instance with the filename path. */
        PAUDIOHLPFILE pFile = NULL;
        rc = audioHlpFileCreateWorker(&pFile, fCreate, enmType, szFile);
        AssertRCReturn(rc, rc);

        /* Try open it. */
        rc = AudioHlpFileOpen(pFile, fOpen, pProps);
        if (RT_SUCCESS(rc))
        {
            *ppFile = pFile;
            return rc;
        }
        AudioHlpFileDestroy(pFile);

        AssertReturn(iTry < 16, rc);
    }
}


/**
 * Creates a debug wav-file structure and opens a file for it, default flags.
 *
 * @returns VBox status code.
 * @param   ppFile      Where to return the debug file instance on success.
 * @param   pszDir      The directory to open the file in.
 * @param   pszName     The base filename.
 * @param   iInstance   The device/driver instance.
 * @param   pProps      PCM audio properties for the file.
 */
int AudioHlpFileCreateAndOpen(PAUDIOHLPFILE *ppFile, const char *pszDir, const char *pszName,
                              uint32_t iInstance, PCPDMAUDIOPCMPROPS pProps)
{
    return AudioHlpFileCreateAndOpenEx(ppFile, AUDIOHLPFILETYPE_WAV, pszDir, iInstance,
                                       AUDIOHLPFILENAME_FLAGS_NONE, AUDIOHLPFILE_FLAGS_NONE,
                                       pProps, AUDIOHLPFILE_DEFAULT_OPEN_FLAGS, "%s", pszName);
}


/**
 * Closes an audio file.
 *
 * @returns VBox status code.
 * @param   pFile               Audio file handle to close.
 */
int AudioHlpFileClose(PAUDIOHLPFILE pFile)
{
    if (!pFile)
        return VINF_SUCCESS;

    size_t cbSize = AudioHlpFileGetDataSize(pFile);

    int rc = VINF_SUCCESS;

    if (pFile->enmType == AUDIOHLPFILETYPE_RAW)
    {
        if (RTFileIsValid(pFile->hFile))
            rc = RTFileClose(pFile->hFile);
    }
    else if (pFile->enmType == AUDIOHLPFILETYPE_WAV)
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
        && !(pFile->fFlags & AUDIOHLPFILE_FLAGS_KEEP_IF_EMPTY))
        rc = AudioHlpFileDelete(pFile);

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
 * @returns VBox status code.
 * @param   pFile               Audio file to delete.
 */
int AudioHlpFileDelete(PAUDIOHLPFILE pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);

    int rc = RTFileDelete(pFile->szName);
    if (RT_SUCCESS(rc))
        LogRel2(("Audio: Deleted file '%s'\n", pFile->szName));
    else if (rc == VERR_FILE_NOT_FOUND) /* Don't bitch if the file is not around anymore. */
        rc = VINF_SUCCESS;

    if (RT_FAILURE(rc))
        LogRel(("Audio: Failed deleting file '%s', rc=%Rrc\n", pFile->szName, rc));

    return rc;
}


/**
 * Returns the raw audio data size of an audio file.
 *
 * @note This does *not* include file headers and other data which does not
 *       belong to the actual PCM audio data.
 *
 * @returns Size (in bytes) of the raw PCM audio data.
 * @param   pFile               Audio file handle to retrieve the audio data size for.
 */
size_t AudioHlpFileGetDataSize(PAUDIOHLPFILE pFile)
{
    AssertPtrReturn(pFile, 0);

    size_t cbSize = 0;

    if (pFile->enmType == AUDIOHLPFILETYPE_RAW)
        cbSize = RTFileTell(pFile->hFile);
    else if (pFile->enmType == AUDIOHLPFILETYPE_WAV)
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
 * @returnd True if open, false if not.
 * @param   pFile               Audio file to check open status for.
 */
bool AudioHlpFileIsOpen(PAUDIOHLPFILE pFile)
{
    if (!pFile)
        return false;

    return RTFileIsValid(pFile->hFile);
}


/**
 * Write PCM data to a wave (.WAV) file.
 *
 * @returns VBox status code.
 * @param   pFile               Audio file to write PCM data to.
 * @param   pvBuf               Audio data to write.
 * @param   cbBuf               Size (in bytes) of audio data to write.
 * @param   fFlags              Additional write flags. Not being used at the moment and must be 0.
 */
int AudioHlpFileWrite(PAUDIOHLPFILE pFile, const void *pvBuf, size_t cbBuf, uint32_t fFlags)
{
    AssertPtrReturn(pFile, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);

    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER); /** @todo fFlags are currently not implemented. */

    if (!cbBuf)
        return VINF_SUCCESS;

    AssertReturn(RTFileIsValid(pFile->hFile), VERR_WRONG_ORDER);

    int rc;

    if (pFile->enmType == AUDIOHLPFILETYPE_RAW)
        rc = RTFileWrite(pFile->hFile, pvBuf, cbBuf, NULL);
    else if (pFile->enmType == AUDIOHLPFILETYPE_WAV)
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

