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
 * Sanitizes the file name component so that unsupported characters
 * will be replaced by an underscore ("_").
 *
 * @returns VBox status code.
 * @param   pszPath             Path to sanitize.
 * @param   cbPath              Size (in bytes) of path to sanitize.
 */
int AudioHlpFileNameSanitize(char *pszPath, size_t cbPath)
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
 * @returns VBox status code.
 * @param   pszFile             Where to store the constructed file name.
 * @param   cchFile             Size (in characters) of the file name buffer.
 * @param   pszPath             Base path to use.
 *                              If NULL or empty, the system's temporary directory will be used.
 * @param   pszName             A name for better identifying the file.
 * @param   uInstance           Device / driver instance which is using this file.
 * @param   enmType             Audio file type to construct file name for.
 * @param   fFlags              File naming flags, AUDIOHLPFILENAME_FLAGS_XXX.
 */
int AudioHlpFileNameGet(char *pszFile, size_t cchFile, const char *pszPath, const char *pszName,
                        uint32_t uInstance, AUDIOHLPFILETYPE enmType, uint32_t fFlags)
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

        if (fFlags & AUDIOHLPFILENAME_FLAGS_TS)
        {
            RTTIMESPEC time;
            if (!RTTimeSpecToString(RTTimeNow(&time), szFileName, sizeof(szFileName)))
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            rc = AudioHlpFileNameSanitize(szFileName, sizeof(szFileName));
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
            case AUDIOHLPFILETYPE_RAW:
                rc = RTStrCat(szFileName, sizeof(szFileName), ".pcm");
                break;

            case AUDIOHLPFILETYPE_WAV:
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
 * @returns VBox status code.
 * @param   enmType             Audio file type to open / create.
 * @param   pszFile             File path of file to open or create.
 * @param   fFlags              Audio file flags, AUDIOHLPFILE_FLAGS_XXX.
 * @param   ppFile              Where to store the created audio file handle.
 *                              Needs to be destroyed with AudioHlpFileDestroy().
 */
int AudioHlpFileCreate(AUDIOHLPFILETYPE enmType, const char *pszFile, uint32_t fFlags, PAUDIOHLPFILE *ppFile)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    /** @todo Validate fFlags. */

    PAUDIOHLPFILE pFile = (PAUDIOHLPFILE)RTMemAlloc(sizeof(AUDIOHLPFILE));
    if (!pFile)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    switch (enmType)
    {
        case AUDIOHLPFILETYPE_RAW:
        case AUDIOHLPFILETYPE_WAV:
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
    {
        rc = RTFileOpen(&pFile->hFile, pFile->szName, fOpen);
    }
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
 * @param   pszName     The base filename.
 * @param   iInstance   The device/driver instance.
 * @param   fFilename   AUDIOHLPFILENAME_FLAGS_XXX.
 * @param   fCreate     AUDIOHLPFILE_FLAGS_XXX.
 * @param   pProps      PCM audio properties for the file.
 * @param   fOpen       RTFILE_O_XXX or AUDIOHLPFILE_DEFAULT_OPEN_FLAGS.
 */
int AudioHlpFileCreateAndOpenEx(PAUDIOHLPFILE *ppFile, AUDIOHLPFILETYPE enmType, const char *pszDir, const char *pszName,
                                uint32_t iInstance, uint32_t fFilename, uint32_t fCreate,
                                PCPDMAUDIOPCMPROPS pProps, uint64_t fOpen)
{
    char szFile[RTPATH_MAX];
    int rc = AudioHlpFileNameGet(szFile, sizeof(szFile), pszDir, pszName, iInstance, enmType, fFilename);
    if (RT_SUCCESS(rc))
    {
        PAUDIOHLPFILE pFile = NULL;
        rc = AudioHlpFileCreate(enmType, szFile, fCreate, &pFile);
        if (RT_SUCCESS(rc))
        {
            rc = AudioHlpFileOpen(pFile, fOpen, pProps);
            if (RT_SUCCESS(rc))
            {
                *ppFile = pFile;
                return rc;
            }
            AudioHlpFileDestroy(pFile);
        }
    }
    *ppFile = NULL;
    return rc;
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
    return AudioHlpFileCreateAndOpenEx(ppFile, AUDIOHLPFILETYPE_WAV, pszDir, pszName, iInstance,
                                       AUDIOHLPFILENAME_FLAGS_NONE, AUDIOHLPFILE_FLAGS_NONE,
                                       pProps, AUDIOHLPFILE_DEFAULT_OPEN_FLAGS);
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
    {
        rc = AudioHlpFileDelete(pFile);
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
 * @returns VBox status code.
 * @param   pFile               Audio file handle to delete.
 */
int AudioHlpFileDelete(PAUDIOHLPFILE pFile)
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
size_t AudioHlpFileGetDataSize(PAUDIOHLPFILE pFile)
{
    AssertPtrReturn(pFile, 0);

    size_t cbSize = 0;

    if (pFile->enmType == AUDIOHLPFILETYPE_RAW)
    {
        cbSize = RTFileTell(pFile->hFile);
    }
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
 * @return  bool                True if open, false if not.
 * @param   pFile               Audio file handle to check open status for.
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
 * @param   pFile               Audio file handle to write PCM data to.
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
    {
        rc = RTFileWrite(pFile->hFile, pvBuf, cbBuf, NULL);
    }
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

