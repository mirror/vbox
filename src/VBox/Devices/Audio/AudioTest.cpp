/* $Id$ */
/** @file
 * Audio testing routines.
 *
 * Common code which is being used by the ValidationKit and the
 * debug / ValdikationKit audio driver(s).
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include <package-generated.h>
#include "product-generated.h"

#include <iprt/buildconfig.h>
#include <iprt/cdefs.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/formats/riff.h>
#include <iprt/inifile.h>
#include <iprt/list.h>
#include <iprt/message.h> /** @todo Get rid of this once we have own log hooks. */
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/system.h>
#include <iprt/uuid.h>
#include <iprt/vfs.h>
#include <iprt/zip.h>

#define _USE_MATH_DEFINES
#include <math.h> /* sin, M_PI */

#include <VBox/version.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "AudioTest.h"


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
/** The test manifest file name. */
#define AUDIOTEST_MANIFEST_FILE_STR "vkat_manifest.ini"
/** The current test manifest version. */
#define AUDIOTEST_MANIFEST_VER      1
/** Audio test archive default suffix.
 *  According to IPRT terminology this always contains the dot. */
#define AUDIOTEST_ARCHIVE_SUFF_STR  ".tar.gz"

/** Test manifest header name. */
#define AUDIOTEST_INI_SEC_HDR_STR   "header"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Structure for an internal object handle.
 *
 * As we only support .INI-style files for now, this only has the object's section name in it.
 */
typedef struct AUDIOTESTOBJHANDLE
{
    char szSec[128];
} AUDIOTESTOBJHANDLE;
/** Pointer to an audio test object handle. */
typedef AUDIOTESTOBJHANDLE* PAUDIOTESTOBJHANDLE;

/**
 * Structure for keeping an audio test verification job.
 */
typedef struct AUDIOTESTVERIFYJOB
{
    /** Pointer to set A. */
    PAUDIOTESTSET       pSetA;
    /** Pointer to set B. */
    PAUDIOTESTSET       pSetB;
    /** Pointer to the error description to use. */
    PAUDIOTESTERRORDESC pErr;
    /** Zero-based index of current test being verified. */
    uint32_t            idxTest;
    /** Flag indicating whether to keep going after an error has occurred. */
    bool                fKeepGoing;
} AUDIOTESTVERIFYJOB;
/** Pointer to an audio test verification job. */
typedef AUDIOTESTVERIFYJOB *PAUDIOTESTVERIFYJOB;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Well-known frequency selection test tones. */
static const double s_aAudioTestToneFreqsHz[] =
{
     349.2282 /*F4*/,
     440.0000 /*A4*/,
     523.2511 /*C5*/,
     698.4565 /*F5*/,
     880.0000 /*A5*/,
    1046.502  /*C6*/,
    1174.659  /*D6*/,
    1396.913  /*F6*/,
    1760.0000 /*A6*/
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int  audioTestSetObjCloseInternal(PAUDIOTESTOBJ pObj);
static void audioTestSetObjFinalize(PAUDIOTESTOBJ pObj);


/**
 * Returns a random test tone frequency.
 */
DECLINLINE(double) audioTestToneGetRandomFreq(void)
{
    return s_aAudioTestToneFreqsHz[RTRandU32Ex(0, RT_ELEMENTS(s_aAudioTestToneFreqsHz) - 1)];
}

/**
 * Initializes a test tone with a specific frequency (in Hz).
 *
 * @returns Used tone frequency (in Hz).
 * @param   pTone               Pointer to test tone to initialize.
 * @param   pProps              PCM properties to use for the test tone.
 * @param   dbFreq              Frequency (in Hz) to initialize tone with.
 *                              When set to 0.0, a random frequency will be chosen.
 */
double AudioTestToneInit(PAUDIOTESTTONE pTone, PPDMAUDIOPCMPROPS pProps, double dbFreq)
{
    if (dbFreq == 0.0)
        dbFreq = audioTestToneGetRandomFreq();

    pTone->rdFreqHz = dbFreq;
    pTone->rdFixed  = 2.0 * M_PI * pTone->rdFreqHz / PDMAudioPropsHz(pProps);
    pTone->uSample  = 0;

    memcpy(&pTone->Props, pProps, sizeof(PDMAUDIOPCMPROPS));

    pTone->enmType = AUDIOTESTTONETYPE_SINE; /* Only type implemented so far. */

    return dbFreq;
}

/**
 * Initializes a test tone by picking a random but well-known frequency (in Hz).
 *
 * @returns Randomly picked tone frequency (in Hz).
 * @param   pTone               Pointer to test tone to initialize.
 * @param   pProps              PCM properties to use for the test tone.
 */
double AudioTestToneInitRandom(PAUDIOTESTTONE pTone, PPDMAUDIOPCMPROPS pProps)
{
    return AudioTestToneInit(pTone, pProps,
                             /* Pick a frequency from our selection, so that every time a recording starts
                              * we'll hopfully generate a different note. */
                             0.0);
}

/**
 * Writes (and iterates) a given test tone to an output buffer.
 *
 * @returns VBox status code.
 * @param   pTone               Pointer to test tone to write.
 * @param   pvBuf               Pointer to output buffer to write test tone to.
 * @param   cbBuf               Size (in bytes) of output buffer.
 * @param   pcbWritten          How many bytes were written on success.
 */
int AudioTestToneGenerate(PAUDIOTESTTONE pTone, void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    /*
     * Clear the buffer first so we don't need to think about additional channels.
     */
    uint32_t cFrames   = PDMAudioPropsBytesToFrames(&pTone->Props, cbBuf);

    /* Input cbBuf not necessarily is aligned to the frames, so re-calculate it. */
    const uint32_t cbToWrite = PDMAudioPropsFramesToBytes(&pTone->Props, cFrames);

    PDMAudioPropsClearBuffer(&pTone->Props, pvBuf, cbBuf, cFrames);

    /*
     * Generate the select sin wave in the first channel:
     */
    uint32_t const cbFrame   = PDMAudioPropsFrameSize(&pTone->Props);
    double const   rdFixed   = pTone->rdFixed;
    uint64_t       iSrcFrame = pTone->uSample;
    switch (PDMAudioPropsSampleSize(&pTone->Props))
    {
        case 1:
            /* untested */
            if (PDMAudioPropsIsSigned(&pTone->Props))
            {
                int8_t *piSample = (int8_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *piSample = (int8_t)(126 /*Amplitude*/ * sin(rdFixed * iSrcFrame));
                    iSrcFrame++;
                    piSample += cbFrame;
                }
            }
            else
            {
                /* untested */
                uint8_t *pbSample = (uint8_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *pbSample = (uint8_t)(126 /*Amplitude*/ * sin(rdFixed * iSrcFrame) + 0x80);
                    iSrcFrame++;
                    pbSample += cbFrame;
                }
            }
            break;

        case 2:
            if (PDMAudioPropsIsSigned(&pTone->Props))
            {
                int16_t *piSample = (int16_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *piSample = (int16_t)(32760 /*Amplitude*/ * sin(rdFixed * iSrcFrame));
                    iSrcFrame++;
                    piSample = (int16_t *)((uint8_t *)piSample + cbFrame);
                }
            }
            else
            {
                /* untested */
                uint16_t *puSample = (uint16_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *puSample = (uint16_t)(32760 /*Amplitude*/ * sin(rdFixed * iSrcFrame) + 0x8000);
                    iSrcFrame++;
                    puSample = (uint16_t *)((uint8_t *)puSample + cbFrame);
                }
            }
            break;

        case 4:
            /* untested */
            if (PDMAudioPropsIsSigned(&pTone->Props))
            {
                int32_t *piSample = (int32_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *piSample = (int32_t)((32760 << 16) /*Amplitude*/ * sin(rdFixed * iSrcFrame));
                    iSrcFrame++;
                    piSample = (int32_t *)((uint8_t *)piSample + cbFrame);
                }
            }
            else
            {
                uint32_t *puSample = (uint32_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *puSample = (uint32_t)((32760 << 16) /*Amplitude*/ * sin(rdFixed * iSrcFrame) + UINT32_C(0x80000000));
                    iSrcFrame++;
                    puSample = (uint32_t *)((uint8_t *)puSample + cbFrame);
                }
            }
            break;

        default:
            AssertFailedReturn(VERR_NOT_SUPPORTED);
    }

    pTone->uSample = iSrcFrame;

    if (pcbWritten)
        *pcbWritten = cbToWrite;

    return VINF_SUCCESS;
}

/**
 * Initializes an audio test tone parameters struct with random values.
 * @param   pToneParams         Test tone parameters to initialize.
 * @param   pProps              PCM properties to use for the test tone.
 */
int AudioTestToneParamsInitRandom(PAUDIOTESTTONEPARMS pToneParams, PPDMAUDIOPCMPROPS pProps)
{
    AssertReturn(PDMAudioPropsAreValid(pProps), VERR_INVALID_PARAMETER);

    memcpy(&pToneParams->Props, pProps, sizeof(PDMAUDIOPCMPROPS));

    /** @todo Make this a bit more sophisticated later, e.g. muting and prequel/sequel are not very balanced. */

    pToneParams->dbFreqHz       = audioTestToneGetRandomFreq();
    pToneParams->msPrequel      = RTRandU32Ex(0, RT_MS_5SEC);
#ifdef DEBUG_andy
    pToneParams->msDuration     = RTRandU32Ex(0, RT_MS_1SEC);
#else
    pToneParams->msDuration     = RTRandU32Ex(0, RT_MS_10SEC); /** @todo Probably a bit too long, but let's see. */
#endif
    pToneParams->msSequel       = RTRandU32Ex(0, RT_MS_5SEC);
    pToneParams->uVolumePercent = RTRandU32Ex(0, 100);

    return VINF_SUCCESS;
}

/**
 * Generates a tag.
 *
 * @returns VBox status code.
 * @param   pszTag              The output buffer.
 * @param   cbTag               The size of the output buffer.
 *                              AUDIOTEST_TAG_MAX is a good size.
 */
int AudioTestGenTag(char *pszTag, size_t cbTag)
{
    RTUUID UUID;
    int rc = RTUuidCreate(&UUID);
    AssertRCReturn(rc, rc);
    rc = RTUuidToStr(&UUID, pszTag, cbTag);
    AssertRCReturn(rc, rc);
    return rc;
}

/**
 * Return the tag to use in the given buffer, generating one if needed.
 *
 * @returns VBox status code.
 * @param   pszTag              The output buffer.
 * @param   cbTag               The size of the output buffer.
 *                              AUDIOTEST_TAG_MAX is a good size.
 * @param   pszTagUser          User specified tag, optional.
 */
static int audioTestCopyOrGenTag(char *pszTag, size_t cbTag, const char *pszTagUser)
{
    if (pszTagUser && *pszTagUser)
        return RTStrCopy(pszTag, cbTag, pszTagUser);
    return AudioTestGenTag(pszTag, cbTag);
}


/**
 * Creates a new path (directory) for a specific audio test set tag.
 *
 * @returns VBox status code.
 * @param   pszPath             On input, specifies the absolute base path where to create the test set path.
 *                              On output this specifies the absolute path created.
 * @param   cbPath              Size (in bytes) of \a pszPath.
 * @param   pszTag              Tag to use for path creation.
 *
 * @note    Can be used multiple times with the same tag; a sub directory with an ISO time string will be used
 *          on each call.
 */
int AudioTestPathCreate(char *pszPath, size_t cbPath, const char *pszTag)
{
    char szTag[AUDIOTEST_TAG_MAX];
    int rc = audioTestCopyOrGenTag(szTag, sizeof(szTag), pszTag);
    AssertRCReturn(rc, rc);

    char szName[RT_ELEMENTS(AUDIOTEST_PATH_PREFIX_STR) + AUDIOTEST_TAG_MAX + 4];
    if (RTStrPrintf2(szName, sizeof(szName), "%s-%s", AUDIOTEST_PATH_PREFIX_STR, szTag) < 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    rc = RTPathAppend(pszPath, cbPath, szName);
    AssertRCReturn(rc, rc);

#ifndef DEBUG /* Makes debugging easier to have a deterministic directory. */
    char szTime[64];
    RTTIMESPEC time;
    if (!RTTimeSpecToString(RTTimeNow(&time), szTime, sizeof(szTime)))
        return VERR_BUFFER_UNDERFLOW;

    /* Colons aren't allowed in windows filenames, so change to dashes. */
    char *pszColon;
    while ((pszColon = strchr(szTime, ':')) != NULL)
        *pszColon = '-';

    rc = RTPathAppend(pszPath, cbPath, szTime);
    AssertRCReturn(rc, rc);
#endif

    return RTDirCreateFullPath(pszPath, RTFS_UNIX_IRWXU);
}

DECLINLINE(int) audioTestManifestWriteData(PAUDIOTESTSET pSet, const void *pvData, size_t cbData)
{
    /** @todo Use RTIniFileWrite once its implemented. */
    return RTFileWrite(pSet->f.hFile, pvData, cbData, NULL);
}

/**
 * Writes string data to a test set manifest.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to write manifest for.
 * @param   pszFormat           Format string to write.
 * @param   args                Variable arguments for \a pszFormat.
 */
static int audioTestManifestWriteV(PAUDIOTESTSET pSet, const char *pszFormat, va_list args)
{
    /** @todo r=bird: Use RTStrmOpen + RTStrmPrintf instead of this slow
     *        do-it-all-yourself stuff. */
    char *psz = NULL;
    if (RTStrAPrintfV(&psz, pszFormat, args) == -1)
        return VERR_NO_MEMORY;
    AssertPtrReturn(psz, VERR_NO_MEMORY);

    int rc = audioTestManifestWriteData(pSet, psz, strlen(psz));
    AssertRC(rc);

    RTStrFree(psz);

    return rc;
}

/**
 * Writes a string to a test set manifest.
 * Convenience function.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to write manifest for.
 * @param   pszFormat           Format string to write.
 * @param   ...                 Variable arguments for \a pszFormat. Optional.
 */
static int audioTestManifestWrite(PAUDIOTESTSET pSet, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);

    int rc = audioTestManifestWriteV(pSet, pszFormat, va);
    AssertRC(rc);

    va_end(va);

    return rc;
}

/**
 * Returns the current read/write offset (in bytes) of the opened manifest file.
 *
 * @returns Current read/write offset (in bytes).
 * @param   pSet                Set to return offset for.
 *                              Must have an opened manifest file.
 */
DECLINLINE(uint64_t) audioTestManifestGetOffsetAbs(PAUDIOTESTSET pSet)
{
    AssertReturn(RTFileIsValid(pSet->f.hFile), 0);
    return RTFileTell(pSet->f.hFile);
}

/**
 * Writes a section header to a test set manifest.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to write manifest for.
 * @param   pszSection          Format string of section to write.
 * @param   ...                 Variable arguments for \a pszSection. Optional.
 */
static int audioTestManifestWriteSectionHdr(PAUDIOTESTSET pSet, const char *pszSection, ...)
{
    va_list va;
    va_start(va, pszSection);

    /** @todo Keep it as simple as possible for now. Improve this later. */
    int rc = audioTestManifestWrite(pSet, "[%N]\n", pszSection, &va);

    va_end(va);

    return rc;
}

/**
 * Initializes an audio test set, internal function.
 *
 * @param   pSet                Test set to initialize.
 */
static void audioTestSetInitInternal(PAUDIOTESTSET pSet)
{
    pSet->f.hFile = NIL_RTFILE;

    RTListInit(&pSet->lstObj);
    pSet->cObj = 0;

    RTListInit(&pSet->lstTest);
    pSet->cTests         = 0;
    pSet->cTestsRunning  = 0;
    pSet->offTestCount   = 0;
    pSet->pTestCur       = NULL;
    pSet->cObj           = 0;
    pSet->offObjCount    = 0;
    pSet->cTotalFailures = 0;
}

/**
 * Returns whether a test set's manifest file is open (and thus ready) or not.
 *
 * @returns \c true if open (and ready), or \c false if not.
 * @retval  VERR_
 * @param   pSet                Test set to return open status for.
 */
static bool audioTestManifestIsOpen(PAUDIOTESTSET pSet)
{
    if (   pSet->enmMode == AUDIOTESTSETMODE_TEST
        && pSet->f.hFile != NIL_RTFILE)
        return true;
    else if (   pSet->enmMode    == AUDIOTESTSETMODE_VERIFY
             && pSet->f.hIniFile != NIL_RTINIFILE)
        return true;

    return false;
}

/**
 * Initializes an audio test error description.
 *
 * @param   pErr                Test error description to initialize.
 */
static void audioTestErrorDescInit(PAUDIOTESTERRORDESC pErr)
{
    RTListInit(&pErr->List);
    pErr->cErrors = 0;
}

/**
 * Destroys an audio test error description.
 *
 * @param   pErr                Test error description to destroy.
 */
void AudioTestErrorDescDestroy(PAUDIOTESTERRORDESC pErr)
{
    if (!pErr)
        return;

    PAUDIOTESTERRORENTRY pErrEntry, pErrEntryNext;
    RTListForEachSafe(&pErr->List, pErrEntry, pErrEntryNext, AUDIOTESTERRORENTRY, Node)
    {
        RTListNodeRemove(&pErrEntry->Node);

        RTMemFree(pErrEntry);

        Assert(pErr->cErrors);
        pErr->cErrors--;
    }

    Assert(pErr->cErrors == 0);
}

/**
 * Returns the the number of errors of an audio test error description.
 *
 * @returns Error count.
 * @param   pErr                Test error description to return error count for.
 */
uint32_t AudioTestErrorDescCount(PCAUDIOTESTERRORDESC pErr)
{
    return pErr->cErrors;
}

/**
 * Returns if an audio test error description contains any errors or not.
 *
 * @returns \c true if it contains errors, or \c false if not.
 * @param   pErr                Test error description to return error status for.
 */
bool AudioTestErrorDescFailed(PCAUDIOTESTERRORDESC pErr)
{
    if (pErr->cErrors)
    {
        Assert(!RTListIsEmpty(&pErr->List));
        return true;
    }

    return false;
}

/**
 * Adds a single error entry to an audio test error description, va_list version.
 *
 * @returns VBox status code.
 * @param   pErr                Test error description to add entry for.
 * @param   idxTest             Index of failing test (zero-based).
 * @param   rc                  Result code of entry to add.
 * @param   pszFormat           Error description format string to add.
 * @param   va                  Optional format arguments of \a pszDesc to add.
 */
static int audioTestErrorDescAddV(PAUDIOTESTERRORDESC pErr, uint32_t idxTest, int rc, const char *pszFormat, va_list va)
{
    PAUDIOTESTERRORENTRY pEntry = (PAUDIOTESTERRORENTRY)RTMemAlloc(sizeof(AUDIOTESTERRORENTRY));
    AssertPtrReturn(pEntry, VERR_NO_MEMORY);

    char *pszDescTmp;
    if (RTStrAPrintfV(&pszDescTmp, pszFormat, va) < 0)
        AssertFailedReturn(VERR_NO_MEMORY);

    const ssize_t cch = RTStrPrintf2(pEntry->szDesc, sizeof(pEntry->szDesc), "Test #%RU32 failed: %s", idxTest, pszDescTmp);
    RTStrFree(pszDescTmp);
    AssertReturn(cch > 0, VERR_BUFFER_OVERFLOW);

    pEntry->rc = rc;

    RTListAppend(&pErr->List, &pEntry->Node);

    pErr->cErrors++;

    return VINF_SUCCESS;
}

/**
 * Adds a single error entry to an audio test error description, va_list version.
 *
 * @returns VBox status code.
 * @param   pErr                Test error description to add entry for.
 * @param   idxTest             Index of failing test (zero-based).
 * @param   pszFormat           Error description format string to add.
 * @param   ...                 Optional format arguments of \a pszDesc to add.
 */
static int audioTestErrorDescAdd(PAUDIOTESTERRORDESC pErr, uint32_t idxTest, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);

    int rc = audioTestErrorDescAddV(pErr, idxTest, VERR_GENERAL_FAILURE /** @todo Fudge! */, pszFormat, va);

    va_end(va);
    return rc;
}

#if 0
static int audioTestErrorDescAddRc(PAUDIOTESTERRORDESC pErr, int rc, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);

    int rc2 = audioTestErrorDescAddV(pErr, rc, pszFormat, va);

    va_end(va);
    return rc2;
}
#endif

/**
 * Retrieves the temporary directory.
 *
 * @returns VBox status code.
 * @param   pszPath             Where to return the absolute path of the created directory on success.
 * @param   cbPath              Size (in bytes) of \a pszPath.
 */
int AudioTestPathGetTemp(char *pszPath, size_t cbPath)
{
    int rc = RTEnvGetEx(RTENV_DEFAULT, "TESTBOX_PATH_SCRATCH", pszPath, cbPath, NULL);
    if (RT_FAILURE(rc))
    {
        rc = RTPathTemp(pszPath, cbPath);
        AssertRCReturn(rc, rc);
    }

    return rc;
}

/**
 * Creates a new temporary directory with a specific (test) tag.
 *
 * @returns VBox status code.
 * @param   pszPath             Where to return the absolute path of the created directory on success.
 * @param   cbPath              Size (in bytes) of \a pszPath.
 * @param   pszTag              Tag name to use for directory creation.
 *
 * @note    Can be used multiple times with the same tag; a sub directory with an ISO time string will be used
 *          on each call.
 */
int AudioTestPathCreateTemp(char *pszPath, size_t cbPath, const char *pszTag)
{
    AssertReturn(pszTag && strlen(pszTag) <= AUDIOTEST_TAG_MAX, VERR_INVALID_PARAMETER);

    char szTemp[RTPATH_MAX];
    int rc = AudioTestPathGetTemp(szTemp, sizeof(szTemp));
    AssertRCReturn(rc, rc);

    rc = AudioTestPathCreate(szTemp, sizeof(szTemp), pszTag);
    AssertRCReturn(rc, rc);

    return RTStrCopy(pszPath, cbPath, szTemp);
}

/**
 * Returns the absolute path of a given audio test set object.
 *
 * @returns VBox status code.
 * @param   pSet                Test set the object contains.
 * @param   pszPathAbs          Where to return the absolute path on success.
 * @param   cbPathAbs           Size (in bytes) of \a pszPathAbs.
 * @param   pszObjName          Name of the object to create absolute path for.
 */
DECLINLINE(int) audioTestSetGetObjPath(PAUDIOTESTSET pSet, char *pszPathAbs, size_t cbPathAbs, const char *pszObjName)
{
    return RTPathJoin(pszPathAbs, cbPathAbs, pSet->szPathAbs, pszObjName);
}

/**
 * Returns the tag of a test set.
 *
 * @returns Test set tag.
 * @param   pSet                Test set to return tag for.
 */
const char *AudioTestSetGetTag(PAUDIOTESTSET pSet)
{
    return pSet->szTag;
}

/**
 * Creates a new audio test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to create.
 * @param   pszPath             Where to store the set set data.  If NULL, the
 *                              temporary directory will be used.
 * @param   pszTag              Tag name to use for this test set.
 */
int AudioTestSetCreate(PAUDIOTESTSET pSet, const char *pszPath, const char *pszTag)
{
    audioTestSetInitInternal(pSet);

    int rc = audioTestCopyOrGenTag(pSet->szTag, sizeof(pSet->szTag), pszTag);
    AssertRCReturn(rc, rc);

    /*
     * Test set directory.
     */
    if (pszPath)
    {
        rc = RTPathAbs(pszPath, pSet->szPathAbs, sizeof(pSet->szPathAbs));
        AssertRCReturn(rc, rc);

        rc = AudioTestPathCreate(pSet->szPathAbs, sizeof(pSet->szPathAbs), pSet->szTag);
    }
    else
        rc = AudioTestPathCreateTemp(pSet->szPathAbs, sizeof(pSet->szPathAbs), pSet->szTag);
    AssertRCReturn(rc, rc);

    /*
     * Create the manifest file.
     */
    char szTmp[RTPATH_MAX];
    rc = RTPathJoin(szTmp, sizeof(szTmp), pSet->szPathAbs, AUDIOTEST_MANIFEST_FILE_STR);
    AssertRCReturn(rc, rc);

    rc = RTFileOpen(&pSet->f.hFile, szTmp, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWriteSectionHdr(pSet, "header");
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWrite(pSet, "magic=vkat_ini\n"); /* VKAT Manifest, .INI-style. */
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "ver=%d\n", AUDIOTEST_MANIFEST_VER);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "tag=%s\n", pSet->szTag);
    AssertRCReturn(rc, rc);

    AssertCompile(sizeof(szTmp) > RTTIME_STR_LEN);
    RTTIMESPEC Now;
    rc = audioTestManifestWrite(pSet, "date_created=%s\n", RTTimeSpecToString(RTTimeNow(&Now), szTmp, sizeof(szTmp)));
    AssertRCReturn(rc, rc);

    RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp)); /* do NOT return on failure. */
    rc = audioTestManifestWrite(pSet, "os_product=%s\n", szTmp);
    AssertRCReturn(rc, rc);

    RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp)); /* do NOT return on failure. */
    rc = audioTestManifestWrite(pSet, "os_rel=%s\n", szTmp);
    AssertRCReturn(rc, rc);

    RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp)); /* do NOT return on failure. */
    rc = audioTestManifestWrite(pSet, "os_ver=%s\n", szTmp);
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWrite(pSet, "vbox_ver=%s r%u %s (%s %s)\n",
                                VBOX_VERSION_STRING, RTBldCfgRevision(), RTBldCfgTargetDotArch(), __DATE__, __TIME__);
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWrite(pSet, "test_count=");
    AssertRCReturn(rc, rc);
    pSet->offTestCount = audioTestManifestGetOffsetAbs(pSet);
    rc = audioTestManifestWrite(pSet, "0000\n"); /* A bit messy, but does the trick for now. */
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWrite(pSet, "obj_count=");
    AssertRCReturn(rc, rc);
    pSet->offObjCount = audioTestManifestGetOffsetAbs(pSet);
    rc = audioTestManifestWrite(pSet, "0000\n"); /* A bit messy, but does the trick for now. */
    AssertRCReturn(rc, rc);

    pSet->enmMode = AUDIOTESTSETMODE_TEST;

    return rc;
}

/**
 * Destroys a test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to destroy.
 */
int AudioTestSetDestroy(PAUDIOTESTSET pSet)
{
    if (!pSet)
        return VINF_SUCCESS;

    AssertReturn(pSet->cTestsRunning == 0, VERR_WRONG_ORDER); /* Make sure no tests sill are running. */

    int rc = AudioTestSetClose(pSet);
    if (RT_FAILURE(rc))
        return rc;

    PAUDIOTESTOBJ pObj, pObjNext;
    RTListForEachSafe(&pSet->lstObj, pObj, pObjNext, AUDIOTESTOBJ, Node)
    {
        rc = audioTestSetObjCloseInternal(pObj);
        if (RT_SUCCESS(rc))
        {
            PAUDIOTESTOBJMETA pMeta, pMetaNext;
            RTListForEachSafe(&pObj->lstMeta, pMeta, pMetaNext, AUDIOTESTOBJMETA, Node)
            {
                switch (pMeta->enmType)
                {
                    case AUDIOTESTOBJMETADATATYPE_STRING:
                    {
                        RTStrFree((char *)pMeta->pvMeta);
                        break;
                    }

                    default:
                        AssertFailed();
                        break;
                }

                RTListNodeRemove(&pMeta->Node);
                RTMemFree(pMeta);
            }

            RTListNodeRemove(&pObj->Node);
            RTMemFree(pObj);

            Assert(pSet->cObj);
            pSet->cObj--;
        }
        else
            break;
    }

    if (RT_FAILURE(rc))
        return rc;

    Assert(pSet->cObj == 0);

    PAUDIOTESTENTRY pEntry, pEntryNext;
    RTListForEachSafe(&pSet->lstTest, pEntry, pEntryNext, AUDIOTESTENTRY, Node)
    {
        RTListNodeRemove(&pEntry->Node);
        RTMemFree(pEntry);

        Assert(pSet->cTests);
        pSet->cTests--;
    }

    if (RT_FAILURE(rc))
        return rc;

    Assert(pSet->cTests == 0);

    return rc;
}

/**
 * Opens an existing audio test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to open.
 * @param   pszPath             Absolute path of the test set to open.
 */
int AudioTestSetOpen(PAUDIOTESTSET pSet, const char *pszPath)
{
    audioTestSetInitInternal(pSet);

    char szManifest[RTPATH_MAX];
    int rc = RTPathJoin(szManifest, sizeof(szManifest), pszPath, AUDIOTEST_MANIFEST_FILE_STR);
    AssertRCReturn(rc, rc);

    RTVFSFILE hVfsFile;
    rc = RTVfsFileOpenNormal(szManifest, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, &hVfsFile);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTIniFileCreateFromVfsFile(&pSet->f.hIniFile, hVfsFile, RTINIFILE_F_READONLY);
    RTVfsFileRelease(hVfsFile);
    AssertRCReturn(rc, rc);

    rc = RTStrCopy(pSet->szPathAbs, sizeof(pSet->szPathAbs), pszPath);
    AssertRCReturn(rc, rc);

    pSet->enmMode = AUDIOTESTSETMODE_VERIFY;

    return rc;
}

/**
 * Closes an opened audio test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to close.
 */
int AudioTestSetClose(PAUDIOTESTSET pSet)
{
    if (!pSet)
        return VINF_SUCCESS;

    if (!RTFileIsValid(pSet->f.hFile))
        return VINF_SUCCESS;

    int rc;

    /* Update number of bound test objects. */
    PAUDIOTESTENTRY pTest;
    RTListForEach(&pSet->lstTest, pTest, AUDIOTESTENTRY, Node)
    {
        rc = RTFileSeek(pSet->f.hFile, pTest->offObjCount, RTFILE_SEEK_BEGIN, NULL);
        AssertRCReturn(rc, rc);
        rc = audioTestManifestWrite(pSet, "%04RU32", pTest->cObj);
        AssertRCReturn(rc, rc);
    }

    /*
     * Update number of ran tests.
     */
    rc = RTFileSeek(pSet->f.hFile, pSet->offObjCount, RTFILE_SEEK_BEGIN, NULL);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "%04RU32", pSet->cObj);
    AssertRCReturn(rc, rc);

    /*
     * Update number of ran tests.
     */
    rc = RTFileSeek(pSet->f.hFile, pSet->offTestCount, RTFILE_SEEK_BEGIN, NULL);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "%04RU32", pSet->cTests);
    AssertRCReturn(rc, rc);

    /*
     * Serialize all registered test objects.
     */
    rc = RTFileSeek(pSet->f.hFile, 0, RTFILE_SEEK_END, NULL);
    AssertRCReturn(rc, rc);

    PAUDIOTESTOBJ pObj;
    RTListForEach(&pSet->lstObj, pObj, AUDIOTESTOBJ, Node)
    {
        rc = audioTestManifestWrite(pSet, "\n");
        AssertRCReturn(rc, rc);
        char szUuid[64];
        rc = RTUuidToStr(&pObj->Uuid, szUuid, sizeof(szUuid));
        AssertRCReturn(rc, rc);
        rc = audioTestManifestWriteSectionHdr(pSet, "obj_%s", szUuid);
        AssertRCReturn(rc, rc);
        rc = audioTestManifestWrite(pSet, "obj_type=%RU32\n", pObj->enmType);
        AssertRCReturn(rc, rc);
        rc = audioTestManifestWrite(pSet, "obj_name=%s\n", pObj->szName);
        AssertRCReturn(rc, rc);

        switch (pObj->enmType)
        {
            case AUDIOTESTOBJTYPE_FILE:
            {
                rc = audioTestManifestWrite(pSet, "obj_size=%RU64\n", pObj->File.cbSize);
                AssertRCReturn(rc, rc);
                break;
            }

            default:
                AssertFailed();
                break;
        }

        /*
         * Write all meta data.
         */
        PAUDIOTESTOBJMETA pMeta;
        RTListForEach(&pObj->lstMeta, pMeta, AUDIOTESTOBJMETA, Node)
        {
            switch (pMeta->enmType)
            {
                case AUDIOTESTOBJMETADATATYPE_STRING:
                {
                    rc = audioTestManifestWrite(pSet, (const char *)pMeta->pvMeta);
                    AssertRCReturn(rc, rc);
                    break;
                }

                default:
                    AssertFailed();
                    break;
            }
        }
    }

    RTFileClose(pSet->f.hFile);
    pSet->f.hFile = NIL_RTFILE;

    return rc;
}

/**
 * Physically wipes all related test set files off the disk.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to wipe.
 */
int AudioTestSetWipe(PAUDIOTESTSET pSet)
{
    AssertPtrReturn(pSet, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    char szFilePath[RTPATH_MAX];

    PAUDIOTESTOBJ pObj;
    RTListForEach(&pSet->lstObj, pObj, AUDIOTESTOBJ, Node)
    {
        int rc2 = audioTestSetObjCloseInternal(pObj);
        if (RT_SUCCESS(rc2))
        {
            rc2 = audioTestSetGetObjPath(pSet, szFilePath, sizeof(szFilePath), pObj->szName);
            if (RT_SUCCESS(rc2))
                rc2 = RTFileDelete(szFilePath);
        }

        if (RT_SUCCESS(rc))
            rc = rc2;
        /* Keep going. */
    }

    if (RT_SUCCESS(rc))
    {
        rc = RTPathJoin(szFilePath, sizeof(szFilePath), pSet->szPathAbs, AUDIOTEST_MANIFEST_FILE_STR);
        if (RT_SUCCESS(rc))
            rc = RTFileDelete(szFilePath);
    }

    /* Remove the (hopefully now empty) directory. Otherwise let this fail. */
    if (RT_SUCCESS(rc))
        rc = RTDirRemove(pSet->szPathAbs);

    return rc;
}

/**
 * Creates and registers a new audio test object to the current running test.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to create and register new object for.
 * @param   pszName             Name of new object to create.
 * @param   ppObj               Where to return the pointer to the newly created object on success.
 */
int AudioTestSetObjCreateAndRegister(PAUDIOTESTSET pSet, const char *pszName, PAUDIOTESTOBJ *ppObj)
{
    AssertReturn(pSet->cTestsRunning == 1, VERR_WRONG_ORDER); /* No test nesting allowed. */

    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    PAUDIOTESTOBJ pObj = (PAUDIOTESTOBJ)RTMemAlloc(sizeof(AUDIOTESTOBJ));
    AssertPtrReturn(pObj, VERR_NO_MEMORY);

    RTListInit(&pObj->lstMeta);

    if (RTStrPrintf2(pObj->szName, sizeof(pObj->szName), "%04RU32-%s", pSet->cObj, pszName) <= 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    /** @todo Generalize this function more once we have more object types. */

    char szObjPathAbs[RTPATH_MAX];
    int rc = audioTestSetGetObjPath(pSet, szObjPathAbs, sizeof(szObjPathAbs), pObj->szName);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileOpen(&pObj->File.hFile, szObjPathAbs, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            pObj->enmType = AUDIOTESTOBJTYPE_FILE;
            pObj->cRefs   = 1; /* Currently only 1:1 mapping. */

            RTListAppend(&pSet->lstObj, &pObj->Node);
            pSet->cObj++;

            /* Generate + set an UUID for the object and assign it to the current test. */
            rc = RTUuidCreate(&pObj->Uuid);
            AssertRCReturn(rc, rc);
            char szUuid[64];
            rc = RTUuidToStr(&pObj->Uuid, szUuid, sizeof(szUuid));
            AssertRCReturn(rc, rc);

            rc = audioTestManifestWrite(pSet, "obj%RU32_uuid=%s\n", pSet->pTestCur->cObj, szUuid);
            AssertRCReturn(rc, rc);

            AssertPtr(pSet->pTestCur);
            pSet->pTestCur->cObj++;

            *ppObj = pObj;
        }
    }

    if (RT_FAILURE(rc))
        RTMemFree(pObj);

    return rc;
}

/**
 * Writes to a created audio test object.
 *
 * @returns VBox status code.
 * @param   pObj                Audio test object to write to.
 * @param   pvBuf               Pointer to data to write.
 * @param   cbBuf               Size (in bytes) of \a pvBuf to write.
 */
int AudioTestSetObjWrite(PAUDIOTESTOBJ pObj, const void *pvBuf, size_t cbBuf)
{
    /** @todo Generalize this function more once we have more object types. */
    AssertReturn(pObj->enmType == AUDIOTESTOBJTYPE_FILE, VERR_INVALID_PARAMETER);

    return RTFileWrite(pObj->File.hFile, pvBuf, cbBuf, NULL);
}

/**
 * Adds meta data to a test object as a string, va_list version.
 *
 * @returns VBox status code.
 * @param   pObj                Test object to add meta data for.
 * @param   pszFormat           Format string to add.
 * @param   va                  Variable arguments list to use for the format string.
 */
static int audioTestSetObjAddMetadataStrV(PAUDIOTESTOBJ pObj, const char *pszFormat, va_list va)
{
    PAUDIOTESTOBJMETA pMeta = (PAUDIOTESTOBJMETA)RTMemAlloc(sizeof(AUDIOTESTOBJMETA));
    AssertPtrReturn(pMeta, VERR_NO_MEMORY);

    pMeta->pvMeta = RTStrAPrintf2V(pszFormat, va);
    AssertPtrReturn(pMeta->pvMeta, VERR_BUFFER_OVERFLOW);
    pMeta->cbMeta = RTStrNLen((const char *)pMeta->pvMeta, RTSTR_MAX);

    pMeta->enmType = AUDIOTESTOBJMETADATATYPE_STRING;

    RTListAppend(&pObj->lstMeta, &pMeta->Node);

    return VINF_SUCCESS;
}

/**
 * Adds meta data to a test object as a string.
 *
 * @returns VBox status code.
 * @param   pObj                Test object to add meta data for.
 * @param   pszFormat           Format string to add.
 * @param   ...                 Variable arguments for the format string.
 */
int AudioTestSetObjAddMetadataStr(PAUDIOTESTOBJ pObj, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int rc = audioTestSetObjAddMetadataStrV(pObj, pszFormat, va);
    va_end(va);

    return rc;
}

/**
 * Closes an opened audio test object.
 *
 * @returns VBox status code.
 * @param   pObj                Audio test object to close.
 */
int AudioTestSetObjClose(PAUDIOTESTOBJ pObj)
{
    if (!pObj)
        return VINF_SUCCESS;

    audioTestSetObjFinalize(pObj);

    return audioTestSetObjCloseInternal(pObj);
}

/**
 * Begins a new test of a test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to begin new test for.
 * @param   pszDesc             Test description.
 * @param   pParms              Test parameters to use.
 * @param   ppEntry             Where to return the new test handle.
 */
int AudioTestSetTestBegin(PAUDIOTESTSET pSet, const char *pszDesc, PAUDIOTESTPARMS pParms, PAUDIOTESTENTRY *ppEntry)
{
    AssertReturn(pSet->cTestsRunning == 0, VERR_WRONG_ORDER); /* No test nesting allowed. */

    PAUDIOTESTENTRY pEntry = (PAUDIOTESTENTRY)RTMemAllocZ(sizeof(AUDIOTESTENTRY));
    AssertPtrReturn(pEntry, VERR_NO_MEMORY);

    int rc = RTStrCopy(pEntry->szDesc, sizeof(pEntry->szDesc), pszDesc);
    AssertRCReturn(rc, rc);

    memcpy(&pEntry->Parms, pParms, sizeof(AUDIOTESTPARMS));
    pEntry->pParent = pSet;

    rc = audioTestManifestWrite(pSet, "\n");
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWriteSectionHdr(pSet, "test_%04RU32", pSet->cTests);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "test_desc=%s\n", pszDesc);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "test_type=%RU32\n", pParms->enmType);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "test_delay_ms=%RU32\n", pParms->msDelay);
    AssertRCReturn(rc, rc);
    rc = audioTestManifestWrite(pSet, "audio_direction=%s\n", PDMAudioDirGetName(pParms->enmDir));
    AssertRCReturn(rc, rc);

    rc = audioTestManifestWrite(pSet, "obj_count=");
    AssertRCReturn(rc, rc);
    pEntry->offObjCount = audioTestManifestGetOffsetAbs(pSet);
    rc = audioTestManifestWrite(pSet, "0000\n"); /* A bit messy, but does the trick for now. */
    AssertRCReturn(rc, rc);

    switch (pParms->enmType)
    {
        case AUDIOTESTTYPE_TESTTONE_PLAY:
            RT_FALL_THROUGH();
        case AUDIOTESTTYPE_TESTTONE_RECORD:
        {
            rc = audioTestManifestWrite(pSet, "tone_freq_hz=%RU16\n", (uint16_t)pParms->TestTone.dbFreqHz);
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_prequel_ms=%RU32\n", pParms->TestTone.msPrequel);
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_duration_ms=%RU32\n", pParms->TestTone.msDuration);
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_sequel_ms=%RU32\n", pParms->TestTone.msSequel);
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_volume_percent=%RU32\n", pParms->TestTone.uVolumePercent);
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_pcm_hz=%RU32\n", PDMAudioPropsHz(&pParms->TestTone.Props));
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_pcm_channels=%RU8\n", PDMAudioPropsChannels(&pParms->TestTone.Props));
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_pcm_bits=%RU8\n", PDMAudioPropsSampleBits(&pParms->TestTone.Props));
            AssertRCReturn(rc, rc);
            rc = audioTestManifestWrite(pSet, "tone_pcm_is_signed=%RTbool\n", PDMAudioPropsIsSigned(&pParms->TestTone.Props));
            AssertRCReturn(rc, rc);
            break;
        }

        default:
            AssertFailed();
            break;
    }

    RTListAppend(&pSet->lstTest, &pEntry->Node);
    pSet->cTests++;
    pSet->cTestsRunning++;
    pSet->pTestCur = pEntry;

    *ppEntry = pEntry;

    return rc;
}

/**
 * Marks a running test as failed.
 *
 * @returns VBox status code.
 * @param   pEntry              Test to mark.
 * @param   rc                  Error code.
 * @param   pszErr              Error description.
 */
int AudioTestSetTestFailed(PAUDIOTESTENTRY pEntry, int rc, const char *pszErr)
{
    AssertReturn(pEntry->pParent->cTestsRunning == 1,            VERR_WRONG_ORDER); /* No test nesting allowed. */
    AssertReturn(pEntry->rc                     == VINF_SUCCESS, VERR_WRONG_ORDER);

    pEntry->rc = rc;

    int rc2 = audioTestManifestWrite(pEntry->pParent, "error_rc=%RI32\n", rc);
    AssertRCReturn(rc2, rc2);
    rc2 = audioTestManifestWrite(pEntry->pParent, "error_desc=%s\n", pszErr);
    AssertRCReturn(rc2, rc2);

    pEntry->pParent->cTestsRunning--;
    pEntry->pParent->pTestCur = NULL;

    return rc2;
}

/**
 * Marks a running test as successfully done.
 *
 * @returns VBox status code.
 * @param   pEntry              Test to mark.
 */
int AudioTestSetTestDone(PAUDIOTESTENTRY pEntry)
{
    AssertReturn(pEntry->pParent->cTestsRunning == 1,            VERR_WRONG_ORDER); /* No test nesting allowed. */
    AssertReturn(pEntry->rc                     == VINF_SUCCESS, VERR_WRONG_ORDER);

    int rc2 = audioTestManifestWrite(pEntry->pParent, "error_rc=%RI32\n", VINF_SUCCESS);
    AssertRCReturn(rc2, rc2);

    pEntry->pParent->cTestsRunning--;
    pEntry->pParent->pTestCur = NULL;

    return rc2;
}

/**
 * Packs a closed audio test so that it's ready for transmission.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to pack.
 * @param   pszOutDir           Directory where to store the packed test set.
 * @param   pszFileName         Where to return the final name of the packed test set. Optional and can be NULL.
 * @param   cbFileName          Size (in bytes) of \a pszFileName.
 */
int AudioTestSetPack(PAUDIOTESTSET pSet, const char *pszOutDir, char *pszFileName, size_t cbFileName)
{
    AssertReturn(!pszFileName || cbFileName, VERR_INVALID_PARAMETER);
    AssertReturn(!audioTestManifestIsOpen(pSet), VERR_WRONG_ORDER);

    /** @todo Check and deny if \a pszOutDir is part of the set's path. */

    int rc = RTDirCreateFullPath(pszOutDir, 0755);
    if (RT_FAILURE(rc))
        return rc;

    char szOutName[RT_ELEMENTS(AUDIOTEST_PATH_PREFIX_STR) + AUDIOTEST_TAG_MAX + 16];
    if (RTStrPrintf2(szOutName, sizeof(szOutName), "%s-%s%s",
                     AUDIOTEST_PATH_PREFIX_STR, pSet->szTag, AUDIOTEST_ARCHIVE_SUFF_STR) <= 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    char szOutPath[RTPATH_MAX];
    rc = RTPathJoin(szOutPath, sizeof(szOutPath), pszOutDir, szOutName);
    AssertRCReturn(rc, rc);

    const char *apszArgs[10];
    unsigned    cArgs = 0;

    apszArgs[cArgs++] = "vkat";
    apszArgs[cArgs++] = "--create";
    apszArgs[cArgs++] = "--gzip";
    apszArgs[cArgs++] = "--directory";
    apszArgs[cArgs++] = pSet->szPathAbs;
    apszArgs[cArgs++] = "--file";
    apszArgs[cArgs++] = szOutPath;
    apszArgs[cArgs++] = ".";

    RTEXITCODE rcExit = RTZipTarCmd(cArgs, (char **)apszArgs);
    if (rcExit != RTEXITCODE_SUCCESS)
        rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */

    if (RT_SUCCESS(rc))
    {
        if (pszFileName)
            rc = RTStrCopy(pszFileName, cbFileName, szOutPath);
    }

    return rc;
}

/**
 * Returns whether a test set archive is packed (as .tar.gz by default) or
 * a plain directory.
 *
 * @returns \c true if packed (as .tar.gz), or \c false if not (directory).
 * @param   pszPath             Path to return packed staus for.
 */
bool AudioTestSetIsPacked(const char *pszPath)
{
    /** @todo Improve this, good enough for now. */
    return (RTStrIStr(pszPath, AUDIOTEST_ARCHIVE_SUFF_STR) != NULL);
}

/**
 * Unpacks a formerly packed audio test set.
 *
 * @returns VBox status code.
 * @param   pszFile             Test set file to unpack. Must contain the absolute path.
 * @param   pszOutDir           Directory where to unpack the test set into.
 *                              If the directory does not exist it will be created.
 */
int AudioTestSetUnpack(const char *pszFile, const char *pszOutDir)
{
    AssertReturn(pszFile && pszOutDir, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    if (!RTDirExists(pszOutDir))
    {
        rc = RTDirCreateFullPath(pszOutDir, 0755);
        if (RT_FAILURE(rc))
            return rc;
    }

    const char *apszArgs[8];
    unsigned    cArgs = 0;

    apszArgs[cArgs++] = "vkat";
    apszArgs[cArgs++] = "--extract";
    apszArgs[cArgs++] = "--gunzip";
    apszArgs[cArgs++] = "--directory";
    apszArgs[cArgs++] = pszOutDir;
    apszArgs[cArgs++] = "--file";
    apszArgs[cArgs++] = pszFile;

    RTEXITCODE rcExit = RTZipTarCmd(cArgs, (char **)apszArgs);
    if (rcExit != RTEXITCODE_SUCCESS)
        rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */

    return rc;
}

/**
 * Gets a value as string.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to get value from.
 * @param   phObj               Object handle to get value for.
 * @param   pszKey              Key to get value from.
 * @param   pszVal              Where to return the value on success.
 * @param   cbVal               Size (in bytes) of \a pszVal.
 */
static int audioTestGetValueStr(PAUDIOTESTSET pSet,
                                PAUDIOTESTOBJHANDLE phObj, const char *pszKey, char *pszVal, size_t cbVal)
{
    return RTIniFileQueryValue(pSet->f.hIniFile, phObj->szSec, pszKey, pszVal, cbVal, NULL);
}

/**
 * Gets a value as uint32_t.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to get value from.
 * @param   phObj               Object handle to get value for.
 * @param   pszKey              Key to get value from.
 * @param   puVal               Where to return the value on success.
 */
static int audioTestGetValueUInt32(PAUDIOTESTSET pSet,
                                   PAUDIOTESTOBJHANDLE phObj, const char *pszKey, uint32_t *puVal)
{
    char szVal[_1K];
    int rc = audioTestGetValueStr(pSet, phObj, pszKey, szVal, sizeof(szVal));
    if (RT_SUCCESS(rc))
        *puVal = RTStrToUInt32(szVal);

    return rc;
}

/**
 * Verifies a value of a test verification job.
 *
 * @returns VBox status code.
 * @returns Error if the verification failed and test verification job has fKeepGoing not set.
 * @param   pVerify             Verification job to verify value for.
 * @param   phObj               Object handle to verify value for.
 * @param   pszKey              Key to verify.
 * @param   pszVal              Value to verify.
 * @param   pszErrFmt           Error format string in case the verification failed.
 * @param   ...                 Variable aruments for error format string.
 */
static int audioTestVerifyValue(PAUDIOTESTVERIFYJOB pVerify,
                                PAUDIOTESTOBJHANDLE phObj, const char *pszKey, const char *pszVal, const char *pszErrFmt, ...)
{
    va_list va;
    va_start(va, pszErrFmt);

    char szValA[_1K];
    int rc = audioTestGetValueStr(pVerify->pSetA, phObj, pszKey, szValA, sizeof(szValA));
    if (RT_SUCCESS(rc))
    {
        char szValB[_1K];
        rc = audioTestGetValueStr(pVerify->pSetB, phObj, pszKey, szValB, sizeof(szValB));
        if (RT_SUCCESS(rc))
        {
            if (RTStrCmp(szValA, szValB))
                rc = VERR_WRONG_TYPE; /** @todo Fudge! */

            if (pszVal)
            {
                if (RTStrCmp(szValA, pszVal))
                    rc = VERR_WRONG_TYPE; /** @todo Fudge! */
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        int rc2 = audioTestErrorDescAddV(pVerify->pErr, pVerify->idxTest, rc, pszErrFmt, va);
        AssertRC(rc2);
    }

    va_end(va);

    return pVerify->fKeepGoing ? VINF_SUCCESS : rc;
}

/**
 * Opens an existing audio test object.
 *
 * @returns VBox status code.
 * @param   pSet                Audio test set the object contains.
 * @param   pszUUID             UUID of object to open.
 * @param   ppObj               Where to return the pointer of the allocated and registered audio test object.
 */
static int audioTestSetObjOpen(PAUDIOTESTSET pSet, const char *pszUUID, PAUDIOTESTOBJ *ppObj)
{
    AUDIOTESTOBJHANDLE hSec;
    if (RTStrPrintf2(hSec.szSec, sizeof(hSec.szSec), "obj_%s", pszUUID) <= 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    PAUDIOTESTOBJ pObj = (PAUDIOTESTOBJ)RTMemAlloc(sizeof(AUDIOTESTOBJ));
    AssertPtrReturn(pObj, VERR_NO_MEMORY);

    char szFileName[128];
    int rc = audioTestGetValueStr(pSet, &hSec, "obj_name", szFileName, sizeof(szFileName));
    if (RT_SUCCESS(rc))
    {
        char szFilePath[RTPATH_MAX];
        rc = RTPathJoin(szFilePath, sizeof(szFilePath), pSet->szPathAbs, szFileName);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileOpen(&pObj->File.hFile, szFilePath, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                int rc2 = RTStrCopy(pObj->szName, sizeof(pObj->szName), szFileName);
                AssertRC(rc2);

                pObj->enmType = AUDIOTESTOBJTYPE_FILE;
                pObj->cRefs   = 1; /* Currently only 1:1 mapping. */

                RTListAppend(&pSet->lstObj, &pObj->Node);
                pSet->cObj++;

                *ppObj = pObj;
                return VINF_SUCCESS;
            }
        }
    }

    RTMemFree(pObj);
    return rc;
}

/**
 * Closes an audio test set object.
 *
 * @returns VBox status code.
 * @param   pObj                Object to close.
 */
static int audioTestSetObjCloseInternal(PAUDIOTESTOBJ pObj)
{
    int rc;

    /** @todo Generalize this function more once we have more object types. */
    AssertReturn(pObj->enmType == AUDIOTESTOBJTYPE_FILE, VERR_INVALID_PARAMETER);

    if (RTFileIsValid(pObj->File.hFile))
    {
        rc = RTFileClose(pObj->File.hFile);
        if (RT_SUCCESS(rc))
            pObj->File.hFile = NIL_RTFILE;
    }
    else
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Finalizes an audio test set object.
 *
 * @param   pObj                Object to finalize.
 */
static void audioTestSetObjFinalize(PAUDIOTESTOBJ pObj)
{
    /** @todo Generalize this function more once we have more object types. */
    AssertReturnVoid(pObj->enmType == AUDIOTESTOBJTYPE_FILE);

    if (RTFileIsValid(pObj->File.hFile))
        pObj->File.cbSize = RTFileTell(pObj->File.hFile);
}

/**
 * Compares two (binary) files.
 *
 * @returns \c true if equal, or \c false if not.
 * @param   hFileA              File handle to file A to compare.
 * @param   hFileB              File handle to file B to compare file A with.
 * @param   cbToCompare         Number of bytes to compare starting the the both file's
 *                              current position.
 */
static bool audioTestFilesCompareBinary(RTFILE hFileA, RTFILE hFileB, uint64_t cbToCompare)
{
    uint8_t auBufA[_32K];
    uint8_t auBufB[_32K];

    int rc = VINF_SUCCESS;

    while (cbToCompare)
    {
        size_t cbReadA;
        rc = RTFileRead(hFileA, auBufA, RT_MIN(cbToCompare, sizeof(auBufA)), &cbReadA);
        AssertRCBreak(rc);
        size_t cbReadB;
        rc = RTFileRead(hFileB, auBufB, RT_MIN(cbToCompare, sizeof(auBufB)), &cbReadB);
        AssertRCBreak(rc);
        AssertBreakStmt(cbReadA == cbReadB, rc = VERR_INVALID_PARAMETER); /** @todo Find a better rc. */
        if (memcmp(auBufA, auBufB, RT_MIN(cbReadA, cbReadB)) != 0)
            return false;
        Assert(cbToCompare >= cbReadA);
        cbToCompare -= cbReadA;
    }

    return RT_SUCCESS(rc) && (cbToCompare == 0);
}

#define CHECK_RC_MAYBE_RET(a_rc, a_pVerJob) \
    if (RT_FAILURE(a_rc)) \
    { \
        if (!a_pVerJob->fKeepGoing) \
            return VINF_SUCCESS; \
    }

#define CHECK_RC_MSG_MAYBE_RET(a_rc, a_pVerJob, a_Msg) \
    if (RT_FAILURE(a_rc)) \
    { \
        int rc3 = audioTestErrorDescAdd(a_pVerJob->pErr, a_pVerJob->idxTest, a_Msg); \
        AssertRC(rc3); \
        if (!a_pVerJob->fKeepGoing) \
            return VINF_SUCCESS; \
    }

#define CHECK_RC_MSG_VA_MAYBE_RET(a_rc, a_pVerJob, a_Msg, ...) \
    if (RT_FAILURE(a_rc)) \
    { \
        int rc3 = audioTestErrorDescAdd(a_pVerJob->pErr, a_pVerJob->idxTest, a_Msg, __VA_ARGS__); \
        AssertRC(rc3); \
        if (!a_pVerJob->fKeepGoing) \
            return VINF_SUCCESS; \
    }

/**
 * Does the actual PCM data verification of a test tone.
 *
 * @returns VBox status code.
 * @param   pVerJob             Verification job to verify PCM data for.
 * @param   phTest              Test handle of test to verify PCM data for.
 */
static int audioTestVerifyTestToneData(PAUDIOTESTVERIFYJOB pVerJob, PAUDIOTESTOBJHANDLE phTest)
{
    int rc;

    /** @todo For now ASSUME that we only have one object per test. */

    char szObjA[128];
    rc = audioTestGetValueStr(pVerJob->pSetA, phTest, "obj0_uuid", szObjA, sizeof(szObjA));
    PAUDIOTESTOBJ pObjA;
    rc = audioTestSetObjOpen(pVerJob->pSetA, szObjA, &pObjA);
    CHECK_RC_MSG_VA_MAYBE_RET(rc, pVerJob, "Unable to open object A '%s'", szObjA);

    char szObjB[128];
    rc = audioTestGetValueStr(pVerJob->pSetB, phTest, "obj0_uuid", szObjB, sizeof(szObjB));
    PAUDIOTESTOBJ pObjB;
    rc = audioTestSetObjOpen(pVerJob->pSetB, szObjB, &pObjB);
    CHECK_RC_MSG_VA_MAYBE_RET(rc, pVerJob, "Unable to open object B '%s'", szObjB);
    AssertReturn(pObjA->enmType == AUDIOTESTOBJTYPE_FILE, VERR_NOT_SUPPORTED);
    AssertReturn(pObjB->enmType == AUDIOTESTOBJTYPE_FILE, VERR_NOT_SUPPORTED);

    /*
     * Start with most obvious methods first.
     */
    uint64_t cbSizeA, cbSizeB;
    rc = RTFileQuerySize(pObjA->File.hFile, &cbSizeA);
    AssertRCReturn(rc, rc);
    rc = RTFileQuerySize(pObjB->File.hFile, &cbSizeB);
    AssertRCReturn(rc, rc);
    if (   cbSizeA != cbSizeB
        || !audioTestFilesCompareBinary(pObjA->File.hFile, pObjB->File.hFile, cbSizeA))
    {
        /** @todo Add more sophisticated stuff here. */

        int rc2 = audioTestErrorDescAdd(pVerJob->pErr, pVerJob->idxTest, "Files '%s' and '%s' don't match\n", szObjA, szObjB);
        AssertRC(rc2);
    }

    rc = audioTestSetObjCloseInternal(pObjA);
    AssertRCReturn(rc, rc);
    rc = audioTestSetObjCloseInternal(pObjB);
    AssertRCReturn(rc, rc);

    return rc;
}

/**
 * Verifies a test tone test.
 *
 * @returns VBox status code.
 * @returns Error if the verification failed and test verification job has fKeepGoing not set.
 * @retval  VERR_
 * @param   pVerify             Verification job to verify test tone for.
 * @param   phTest              Test handle of test tone to verify.
 * @param   pSetPlay            Test set which did the playing part.
 * @param   pSetRecord          Test set which did the recording part.
 */
static int audioTestVerifyTestTone(PAUDIOTESTVERIFYJOB pVerify, PAUDIOTESTOBJHANDLE phTest, PAUDIOTESTSET pSetPlay, PAUDIOTESTSET pSetRecord)
{
    RT_NOREF(pSetPlay, pSetRecord);

    int rc;

    /*
     * Verify test parameters.
     * More important items have precedence.
     */
    rc = audioTestVerifyValue(pVerify, phTest, "error_rc", "0", "Test was reported as failed");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "obj_count", NULL, "Object counts don't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_freq_hz", NULL, "Tone frequency doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_prequel_ms", NULL, "Tone prequel (ms) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_duration_ms", NULL, "Tone duration (ms) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_sequel_ms", NULL, "Tone sequel (ms) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_volume_percent", NULL, "Tone volume (percent) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_pcm_hz", NULL, "Tone PCM Hz doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_pcm_channels", NULL, "Tone PCM channels don't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_pcm_bits", NULL, "Tone PCM bits don't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTest, "tone_pcm_is_signed", NULL, "Tone PCM signed bit doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);

    /*
     * Now the fun stuff, PCM data analysis.
     */
    rc = audioTestVerifyTestToneData(pVerify, phTest);
    if (RT_FAILURE(rc))
    {
       int rc2 = audioTestErrorDescAdd(pVerify->pErr, pVerify->idxTest, "Verififcation of test tone data failed\n");
       AssertRC(rc2);
    }

    return VINF_SUCCESS;
}

/**
 * Verifies an opened audio test set.
 *
 * @returns VBox status code.
 * @param   pSetA               Test set A to verify.
 * @param   pSetB               Test set to verify test set A with.
 * @param   pErrDesc            Where to return the test verification errors.
 *
 * @note    Test verification errors have to be checked for errors, regardless of the
 *          actual return code.
 */
int AudioTestSetVerify(PAUDIOTESTSET pSetA, PAUDIOTESTSET pSetB, PAUDIOTESTERRORDESC pErrDesc)
{
    AssertReturn(audioTestManifestIsOpen(pSetA), VERR_WRONG_ORDER);
    AssertReturn(audioTestManifestIsOpen(pSetB), VERR_WRONG_ORDER);

    /* We ASSUME the caller has not init'd pErrDesc. */
    audioTestErrorDescInit(pErrDesc);

    AUDIOTESTVERIFYJOB VerJob;
    RT_ZERO(VerJob);
    VerJob.pErr       = pErrDesc;
    VerJob.pSetA      = pSetA;
    VerJob.pSetB      = pSetB;
    VerJob.fKeepGoing = true;

    PAUDIOTESTVERIFYJOB pVerJob = &VerJob;

    int rc;

    /*
     * Compare obvious values first.
     */
    AUDIOTESTOBJHANDLE hHdr;
    RTStrPrintf(hHdr.szSec, sizeof(hHdr.szSec), "header");

    rc = audioTestVerifyValue(&VerJob, &hHdr,   "magic",        "vkat_ini",    "Manifest magic wrong");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdr,   "ver",          "1"       ,    "Manifest version wrong");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdr,   "tag",          NULL,          "Manifest tags don't match");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdr,   "test_count",   NULL,          "Test counts don't match");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdr,   "obj_count",    NULL,          "Object counts don't match");
    CHECK_RC_MAYBE_RET(rc, pVerJob);

    /*
     * Compare ran tests.
     */
    uint32_t cTests;
    rc = audioTestGetValueUInt32(VerJob.pSetA, &hHdr, "test_count", &cTests);
    AssertRCReturn(rc, rc);

    for (uint32_t i = 0; i < cTests; i++)
    {
        VerJob.idxTest = i;

        AUDIOTESTOBJHANDLE hTest;
        RTStrPrintf(hTest.szSec, sizeof(hTest.szSec), "test_%04RU32", i);

        AUDIOTESTTYPE enmTestTypeA;
        rc = audioTestGetValueUInt32(VerJob.pSetA, &hTest, "test_type", (uint32_t *)&enmTestTypeA);
        CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Test type A not found");

        AUDIOTESTTYPE enmTestTypeB;
        rc = audioTestGetValueUInt32(VerJob.pSetB, &hTest, "test_type", (uint32_t *)&enmTestTypeB);
        CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Test type B not found %RU32");

        switch (enmTestTypeA)
        {
            case AUDIOTESTTYPE_TESTTONE_PLAY:
            {
                if (enmTestTypeB == AUDIOTESTTYPE_TESTTONE_RECORD)
                {
                    rc = audioTestVerifyTestTone(&VerJob, &hTest, VerJob.pSetA, VerJob.pSetB);
                }
                else
                    rc = audioTestErrorDescAdd(pErrDesc, i, "Playback test types don't match (set A=%#x, set B=%#x)",
                                               enmTestTypeA, enmTestTypeB);
                break;
            }

            case AUDIOTESTTYPE_TESTTONE_RECORD:
            {
                if (enmTestTypeB == AUDIOTESTTYPE_TESTTONE_PLAY)
                {
                    rc = audioTestVerifyTestTone(&VerJob, &hTest, VerJob.pSetB, VerJob.pSetA);
                }
                else
                    rc = audioTestErrorDescAdd(pErrDesc, i, "Recording test types don't match (set A=%#x, set B=%#x)",
                                               enmTestTypeA, enmTestTypeB);
                break;
            }

            case AUDIOTESTTYPE_INVALID:
                rc = VERR_INVALID_PARAMETER;
                break;

            default:
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }

        AssertRC(rc);
    }

    /* Only return critical stuff not related to actual testing here. */
    return VINF_SUCCESS;
}

#undef CHECK_RC_MAYBE_RET
#undef CHECK_RC_MSG_MAYBE_RET


/*********************************************************************************************************************************
*   WAVE File Reader.                                                                                                            *
*********************************************************************************************************************************/

/**
 * Counts the number of set bits in @a fMask.
 */
static unsigned audioTestWaveCountBits(uint32_t fMask)
{
    unsigned cBits = 0;
    while (fMask)
    {
        if (fMask & 1)
            cBits++;
        fMask >>= 1;
    }
    return cBits;
}

/**
 * Opens a wave (.WAV) file for reading.
 *
 * @returns VBox status code.
 * @param   pszFile     The file to open.
 * @param   pWaveFile   The open wave file structure to fill in on success.
 * @param   pErrInfo    Where to return addition error details on failure.
 */
int AudioTestWaveFileOpen(const char *pszFile, PAUDIOTESTWAVEFILE pWaveFile, PRTERRINFO pErrInfo)
{
    pWaveFile->u32Magic = AUDIOTESTWAVEFILE_MAGIC_DEAD;
    RT_ZERO(pWaveFile->Props);
    pWaveFile->hFile = NIL_RTFILE;
    int rc = RTFileOpen(&pWaveFile->hFile, pszFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return RTErrInfoSet(pErrInfo, rc, "RTFileOpen failed");
    uint64_t cbFile = 0;
    rc = RTFileQuerySize(pWaveFile->hFile, &cbFile);
    if (RT_SUCCESS(rc))
    {
        union
        {
            uint8_t                 ab[512];
            struct
            {
                RTRIFFHDR           Hdr;
                union
                {
                    RTRIFFWAVEFMTCHUNK      Fmt;
                    RTRIFFWAVEFMTEXTCHUNK   FmtExt;
                } u;
            } Wave;
            RTRIFFLIST              List;
            RTRIFFCHUNK             Chunk;
            RTRIFFWAVEDATACHUNK     Data;
        } uBuf;

        rc = RTFileRead(pWaveFile->hFile, &uBuf.Wave, sizeof(uBuf.Wave), NULL);
        if (RT_SUCCESS(rc))
        {
            rc = VERR_VFS_UNKNOWN_FORMAT;
            if (   uBuf.Wave.Hdr.uMagic    == RTRIFFHDR_MAGIC
                && uBuf.Wave.Hdr.uFileType == RTRIFF_FILE_TYPE_WAVE
                && uBuf.Wave.u.Fmt.Chunk.uMagic == RTRIFFWAVEFMT_MAGIC
                && uBuf.Wave.u.Fmt.Chunk.cbChunk >= sizeof(uBuf.Wave.u.Fmt.Data))
            {
                if (uBuf.Wave.Hdr.cbFile != cbFile - sizeof(RTRIFFCHUNK))
                    RTErrInfoSetF(pErrInfo, rc, "File size mismatch: %#x, actual %#RX64 (ignored)",
                                  uBuf.Wave.Hdr.cbFile, cbFile - sizeof(RTRIFFCHUNK));
                rc = VERR_VFS_BOGUS_FORMAT;
                if (   uBuf.Wave.u.Fmt.Data.uFormatTag != RTRIFFWAVEFMT_TAG_PCM
                    && uBuf.Wave.u.Fmt.Data.uFormatTag != RTRIFFWAVEFMT_TAG_EXTENSIBLE)
                    RTErrInfoSetF(pErrInfo, rc, "Unsupported uFormatTag value: %#x (expected %#x or %#x)",
                                  uBuf.Wave.u.Fmt.Data.uFormatTag, RTRIFFWAVEFMT_TAG_PCM, RTRIFFWAVEFMT_TAG_EXTENSIBLE);
                else if (   uBuf.Wave.u.Fmt.Data.cBitsPerSample != 8
                         && uBuf.Wave.u.Fmt.Data.cBitsPerSample != 16
                         /* && uBuf.Wave.u.Fmt.Data.cBitsPerSample != 24 - not supported by our stack */
                         && uBuf.Wave.u.Fmt.Data.cBitsPerSample != 32)
                    RTErrInfoSetF(pErrInfo, rc, "Unsupported cBitsPerSample value: %u", uBuf.Wave.u.Fmt.Data.cBitsPerSample);
                else if (   uBuf.Wave.u.Fmt.Data.cChannels < 1
                         || uBuf.Wave.u.Fmt.Data.cChannels >= 16)
                    RTErrInfoSetF(pErrInfo, rc, "Unsupported cChannels value: %u (expected 1..15)", uBuf.Wave.u.Fmt.Data.cChannels);
                else if (   uBuf.Wave.u.Fmt.Data.uHz < 4096
                         || uBuf.Wave.u.Fmt.Data.uHz > 768000)
                    RTErrInfoSetF(pErrInfo, rc, "Unsupported uHz value: %u (expected 4096..768000)", uBuf.Wave.u.Fmt.Data.uHz);
                else if (uBuf.Wave.u.Fmt.Data.cbFrame != uBuf.Wave.u.Fmt.Data.cChannels * uBuf.Wave.u.Fmt.Data.cBitsPerSample / 8)
                    RTErrInfoSetF(pErrInfo, rc, "Invalid cbFrame value: %u (expected %u)", uBuf.Wave.u.Fmt.Data.cbFrame,
                                  uBuf.Wave.u.Fmt.Data.cChannels * uBuf.Wave.u.Fmt.Data.cBitsPerSample / 8);
                else if (uBuf.Wave.u.Fmt.Data.cbRate != uBuf.Wave.u.Fmt.Data.cbFrame * uBuf.Wave.u.Fmt.Data.uHz)
                    RTErrInfoSetF(pErrInfo, rc, "Invalid cbRate value: %u (expected %u)", uBuf.Wave.u.Fmt.Data.cbRate,
                                  uBuf.Wave.u.Fmt.Data.cbFrame * uBuf.Wave.u.Fmt.Data.uHz);
                else if (   uBuf.Wave.u.Fmt.Data.uFormatTag == RTRIFFWAVEFMT_TAG_EXTENSIBLE
                         && uBuf.Wave.u.FmtExt.Data.cbExtra < RTRIFFWAVEFMTEXT_EXTRA_SIZE)
                    RTErrInfoSetF(pErrInfo, rc, "Invalid cbExtra value: %#x (expected at least %#x)",
                                  uBuf.Wave.u.FmtExt.Data.cbExtra, RTRIFFWAVEFMTEXT_EXTRA_SIZE);
                else if (   uBuf.Wave.u.Fmt.Data.uFormatTag == RTRIFFWAVEFMT_TAG_EXTENSIBLE
                         && audioTestWaveCountBits(uBuf.Wave.u.FmtExt.Data.fChannelMask) != uBuf.Wave.u.Fmt.Data.cChannels)
                    RTErrInfoSetF(pErrInfo, rc, "fChannelMask does not match cChannels: %#x (%u bits set) vs %u channels",
                                  uBuf.Wave.u.FmtExt.Data.fChannelMask,
                                  audioTestWaveCountBits(uBuf.Wave.u.FmtExt.Data.fChannelMask), uBuf.Wave.u.Fmt.Data.cChannels);
                else if (   uBuf.Wave.u.Fmt.Data.uFormatTag == RTRIFFWAVEFMT_TAG_EXTENSIBLE
                         && RTUuidCompareStr(&uBuf.Wave.u.FmtExt.Data.SubFormat, RTRIFFWAVEFMTEXT_SUBTYPE_PCM) != 0)
                    RTErrInfoSetF(pErrInfo, rc, "SubFormat is not PCM: %RTuuid (expected %s)",
                                  &uBuf.Wave.u.FmtExt.Data.SubFormat, RTRIFFWAVEFMTEXT_SUBTYPE_PCM);
                else
                {
                    /*
                     * Copy out the data we need from the file format structure.
                     */
                    PDMAudioPropsInit(&pWaveFile->Props, uBuf.Wave.u.Fmt.Data.cBitsPerSample / 8, true /*fSigned*/,
                                      uBuf.Wave.u.Fmt.Data.cChannels, uBuf.Wave.u.Fmt.Data.uHz);
                    pWaveFile->offSamples = sizeof(RTRIFFHDR) + sizeof(RTRIFFCHUNK) + uBuf.Wave.u.Fmt.Chunk.cbChunk;

                    /*
                     * Pick up channel assignments if present.
                     */
                    if (uBuf.Wave.u.Fmt.Data.uFormatTag == RTRIFFWAVEFMT_TAG_EXTENSIBLE)
                    {
                        static unsigned const   s_cStdIds = (unsigned)PDMAUDIOCHANNELID_END_STANDARD
                                                          - (unsigned)PDMAUDIOCHANNELID_FIRST_STANDARD;
                        unsigned                iCh       = 0;
                        for (unsigned idCh = 0; idCh < 32 && iCh < uBuf.Wave.u.Fmt.Data.cChannels; idCh++)
                            if (uBuf.Wave.u.FmtExt.Data.fChannelMask & RT_BIT_32(idCh))
                            {
                                pWaveFile->Props.aidChannels[iCh] = idCh < s_cStdIds
                                                                  ? idCh + (unsigned)PDMAUDIOCHANNELID_FIRST_STANDARD
                                                                  : (unsigned)PDMAUDIOCHANNELID_UNKNOWN;
                                iCh++;
                            }
                    }

                    /*
                     * Find the 'data' chunk with the audio samples.
                     *
                     * There can be INFO lists both preceeding this and succeeding
                     * it, containing IART and other things we can ignored.  Thus
                     * we read a list header here rather than just a chunk header,
                     * since it doesn't matter if we read 4 bytes extra as
                     * AudioTestWaveFileRead uses RTFileReadAt anyway.
                     */
                    rc = RTFileReadAt(pWaveFile->hFile, pWaveFile->offSamples, &uBuf, sizeof(uBuf.List), NULL);
                    for (uint32_t i = 0;
                            i < 128
                         && RT_SUCCESS(rc)
                         && uBuf.Chunk.uMagic != RTRIFFWAVEDATACHUNK_MAGIC
                         && (uint64_t)uBuf.Chunk.cbChunk + sizeof(RTRIFFCHUNK) * 2 <= cbFile - pWaveFile->offSamples;
                         i++)
                    {
                        if (   uBuf.List.uMagic    == RTRIFFLIST_MAGIC
                            && uBuf.List.uListType == RTRIFFLIST_TYPE_INFO)
                        { /*skip*/ }
                        else if (uBuf.Chunk.uMagic == RTRIFFPADCHUNK_MAGIC)
                        { /*skip*/ }
                        else
                            break;
                        pWaveFile->offSamples += sizeof(RTRIFFCHUNK) + uBuf.Chunk.cbChunk;
                        rc = RTFileReadAt(pWaveFile->hFile, pWaveFile->offSamples, &uBuf, sizeof(uBuf.List), NULL);
                    }
                    if (RT_SUCCESS(rc))
                    {
                        pWaveFile->offSamples += sizeof(uBuf.Data.Chunk);
                        pWaveFile->cbSamples   = (uint32_t)cbFile - pWaveFile->offSamples;

                        rc = VERR_VFS_BOGUS_FORMAT;
                        if (   uBuf.Data.Chunk.uMagic == RTRIFFWAVEDATACHUNK_MAGIC
                            && uBuf.Data.Chunk.cbChunk <= pWaveFile->cbSamples
                            && PDMAudioPropsIsSizeAligned(&pWaveFile->Props, uBuf.Data.Chunk.cbChunk))
                        {
                            pWaveFile->cbSamples = uBuf.Data.Chunk.cbChunk;

                            /*
                             * We're good!
                             */
                            pWaveFile->offCur    = 0;
                            pWaveFile->fReadMode = true;
                            pWaveFile->u32Magic  = AUDIOTESTWAVEFILE_MAGIC;
                            return VINF_SUCCESS;
                        }

                        RTErrInfoSetF(pErrInfo, rc, "Bad data header: uMagic=%#x (expected %#x), cbChunk=%#x (max %#RX64, align %u)",
                                      uBuf.Data.Chunk.uMagic, RTRIFFWAVEDATACHUNK_MAGIC,
                                      uBuf.Data.Chunk.cbChunk, pWaveFile->cbSamples, PDMAudioPropsFrameSize(&pWaveFile->Props));
                    }
                    else
                        RTErrInfoSet(pErrInfo, rc, "Failed to read data header");
                }
            }
            else
                RTErrInfoSetF(pErrInfo, rc, "Bad file header: uMagic=%#x (vs. %#x), uFileType=%#x (vs %#x), uFmtMagic=%#x (vs %#x) cbFmtChunk=%#x (min %#x)",
                              uBuf.Wave.Hdr.uMagic, RTRIFFHDR_MAGIC, uBuf.Wave.Hdr.uFileType, RTRIFF_FILE_TYPE_WAVE,
                              uBuf.Wave.u.Fmt.Chunk.uMagic, RTRIFFWAVEFMT_MAGIC,
                              uBuf.Wave.u.Fmt.Chunk.cbChunk, sizeof(uBuf.Wave.u.Fmt.Data));
        }
        else
            rc = RTErrInfoSet(pErrInfo, rc, "Failed to read file header");
    }
    else
        rc = RTErrInfoSet(pErrInfo, rc, "Failed to query file size");

    RTFileClose(pWaveFile->hFile);
    pWaveFile->hFile = NIL_RTFILE;
    return rc;
}


/**
 * Creates a new wave file.
 *
 * @returns VBox status code.
 * @param   pszFile     The filename.
 * @param   pProps      The audio format properties.
 * @param   pWaveFile   The wave file structure to fill in on success.
 * @param   pErrInfo    Where to return addition error details on failure.
 */
int AudioTestWaveFileCreate(const char *pszFile, PCPDMAUDIOPCMPROPS pProps, PAUDIOTESTWAVEFILE pWaveFile, PRTERRINFO pErrInfo)
{
    /*
     * Construct the file header first (we'll do some input validation
     * here, so better do it before creating the file).
     */
    struct
    {
        RTRIFFHDR               Hdr;
        RTRIFFWAVEFMTEXTCHUNK   FmtExt;
        RTRIFFCHUNK             Data;
    } FileHdr;

    FileHdr.Hdr.uMagic                      = RTRIFFHDR_MAGIC;
    FileHdr.Hdr.cbFile                      = 0; /* need to update this later */
    FileHdr.Hdr.uFileType                   = RTRIFF_FILE_TYPE_WAVE;
    FileHdr.FmtExt.Chunk.uMagic             = RTRIFFWAVEFMT_MAGIC;
    FileHdr.FmtExt.Chunk.cbChunk            = sizeof(RTRIFFWAVEFMTEXTCHUNK) - sizeof(RTRIFFCHUNK);
    FileHdr.FmtExt.Data.Core.uFormatTag     = RTRIFFWAVEFMT_TAG_EXTENSIBLE;
    FileHdr.FmtExt.Data.Core.cChannels      = PDMAudioPropsChannels(pProps);
    FileHdr.FmtExt.Data.Core.uHz            = PDMAudioPropsHz(pProps);
    FileHdr.FmtExt.Data.Core.cbRate         = PDMAudioPropsFramesToBytes(pProps, PDMAudioPropsHz(pProps));
    FileHdr.FmtExt.Data.Core.cbFrame        = PDMAudioPropsFrameSize(pProps);
    FileHdr.FmtExt.Data.Core.cBitsPerSample = PDMAudioPropsSampleBits(pProps);
    FileHdr.FmtExt.Data.cbExtra             = sizeof(FileHdr.FmtExt.Data) - sizeof(FileHdr.FmtExt.Data.Core);
    FileHdr.FmtExt.Data.cValidBitsPerSample = PDMAudioPropsSampleBits(pProps);
    FileHdr.FmtExt.Data.fChannelMask        = 0;
    for (uintptr_t idxCh = 0; idxCh < FileHdr.FmtExt.Data.Core.cChannels; idxCh++)
    {
        PDMAUDIOCHANNELID const idCh = (PDMAUDIOCHANNELID)pProps->aidChannels[idxCh];
        if (   idCh >= PDMAUDIOCHANNELID_FIRST_STANDARD
            && idCh <  PDMAUDIOCHANNELID_END_STANDARD)
        {
            if (!(FileHdr.FmtExt.Data.fChannelMask & RT_BIT_32(idCh - PDMAUDIOCHANNELID_FIRST_STANDARD)))
                FileHdr.FmtExt.Data.fChannelMask |= RT_BIT_32(idCh - PDMAUDIOCHANNELID_FIRST_STANDARD);
            else
                return RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "Channel #%u repeats channel ID %d", idxCh, idCh);
        }
        else
            return RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "Invalid channel ID %d for channel #%u", idCh, idxCh);
    }

    RTUUID UuidTmp;
    int rc = RTUuidFromStr(&UuidTmp, RTRIFFWAVEFMTEXT_SUBTYPE_PCM);
    AssertRCReturn(rc, rc);
    FileHdr.FmtExt.Data.SubFormat = UuidTmp; /* (64-bit field maybe unaligned) */

    FileHdr.Data.uMagic  = RTRIFFWAVEDATACHUNK_MAGIC;
    FileHdr.Data.cbChunk = 0; /* need to update this later */

    /*
     * Create the file and write the header.
     */
    pWaveFile->hFile = NIL_RTFILE;
    rc = RTFileOpen(&pWaveFile->hFile, pszFile, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return RTErrInfoSet(pErrInfo, rc, "RTFileOpen failed");

    rc = RTFileWrite(pWaveFile->hFile, &FileHdr, sizeof(FileHdr), NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize the wave file structure.
         */
        pWaveFile->fReadMode       = false;
        pWaveFile->offCur          = 0;
        pWaveFile->offSamples      = 0;
        pWaveFile->cbSamples       = 0;
        pWaveFile->Props           = *pProps;
        pWaveFile->offSamples      = RTFileTell(pWaveFile->hFile);
        if (pWaveFile->offSamples != UINT32_MAX)
        {
            pWaveFile->u32Magic = AUDIOTESTWAVEFILE_MAGIC;
            return VINF_SUCCESS;
        }
        rc = RTErrInfoSet(pErrInfo, VERR_SEEK, "RTFileTell failed");
    }
    else
        RTErrInfoSet(pErrInfo, rc, "RTFileWrite failed writing header");

    RTFileClose(pWaveFile->hFile);
    pWaveFile->hFile    = NIL_RTFILE;
    pWaveFile->u32Magic = AUDIOTESTWAVEFILE_MAGIC_DEAD;

    RTFileDelete(pszFile);
    return rc;
}


/**
 * Closes a wave file.
 */
int AudioTestWaveFileClose(PAUDIOTESTWAVEFILE pWaveFile)
{
    AssertReturn(pWaveFile->u32Magic == AUDIOTESTWAVEFILE_MAGIC, VERR_INVALID_MAGIC);
    int rcRet = VINF_SUCCESS;
    int rc;

    /*
     * Update the size fields if writing.
     */
    if (!pWaveFile->fReadMode)
    {
        uint64_t cbFile = RTFileTell(pWaveFile->hFile);
        if (cbFile != UINT64_MAX)
        {
            uint32_t cbFile32 = cbFile - sizeof(RTRIFFCHUNK);
            rc = RTFileWriteAt(pWaveFile->hFile, RT_OFFSETOF(RTRIFFHDR, cbFile), &cbFile32, sizeof(cbFile32), NULL);
            AssertRCStmt(rc, rcRet = rc);

            uint32_t cbSamples = cbFile - pWaveFile->offSamples;
            rc = RTFileWriteAt(pWaveFile->hFile, pWaveFile->offSamples - sizeof(uint32_t), &cbSamples, sizeof(cbSamples), NULL);
            AssertRCStmt(rc, rcRet = rc);
        }
        else
            rcRet = VERR_SEEK;
    }

    /*
     * Close it.
     */
    rc = RTFileClose(pWaveFile->hFile);
    AssertRCStmt(rc, rcRet = rc);

    pWaveFile->hFile    = NIL_RTFILE;
    pWaveFile->u32Magic = AUDIOTESTWAVEFILE_MAGIC_DEAD;
    return rcRet;
}

/**
 * Reads samples from a wave file.
 *
 * @returns VBox status code.  See RTVfsFileRead for EOF status handling.
 * @param   pWaveFile   The file to read from.
 * @param   pvBuf       Where to put the samples.
 * @param   cbBuf       How much to read at most.
 * @param   pcbRead     Where to return the actual number of bytes read,
 *                      optional.
 */
int AudioTestWaveFileRead(PAUDIOTESTWAVEFILE pWaveFile, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    AssertReturn(pWaveFile->u32Magic == AUDIOTESTWAVEFILE_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(pWaveFile->fReadMode, VERR_ACCESS_DENIED);

    bool fEofAdjusted;
    if (pWaveFile->offCur + cbBuf <= pWaveFile->cbSamples)
        fEofAdjusted = false;
    else if (pcbRead)
    {
        fEofAdjusted = true;
        cbBuf = pWaveFile->cbSamples - pWaveFile->offCur;
    }
    else
        return VERR_EOF;

    int rc = RTFileReadAt(pWaveFile->hFile, pWaveFile->offSamples + pWaveFile->offCur, pvBuf, cbBuf, pcbRead);
    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
        {
            pWaveFile->offCur += (uint32_t)*pcbRead;
            if (fEofAdjusted || cbBuf > *pcbRead)
                rc = VINF_EOF;
            else if (!cbBuf && pWaveFile->offCur == pWaveFile->cbSamples)
                rc = VINF_EOF;
        }
        else
            pWaveFile->offCur += (uint32_t)cbBuf;
    }
    return rc;
}


/**
 * Writes samples to a wave file.
 *
 * @returns VBox status code.
 * @param   pWaveFile   The file to write to.
 * @param   pvBuf       The samples to write.
 * @param   cbBuf       How many bytes of samples to write.
 */
int AudioTestWaveFileWrite(PAUDIOTESTWAVEFILE pWaveFile, const void *pvBuf, size_t cbBuf)
{
    AssertReturn(pWaveFile->u32Magic == AUDIOTESTWAVEFILE_MAGIC, VERR_INVALID_MAGIC);
    AssertReturn(!pWaveFile->fReadMode, VERR_ACCESS_DENIED);

    pWaveFile->cbSamples += (uint32_t)cbBuf;
    return RTFileWrite(pWaveFile->hFile, pvBuf, cbBuf, NULL);
}

