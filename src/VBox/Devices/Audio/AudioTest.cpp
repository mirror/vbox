/* $Id$ */
/** @file
 * Audio testing routines.
 * Common code which is being used by the ValidationKit and the debug / ValdikationKit audio driver(s).
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
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/formats/riff.h>
#include <iprt/inifile.h>
#include <iprt/list.h>
#include <iprt/message.h> /** @todo Get rid of this once we have own log hooks. */
#include <iprt/rand.h>
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
int AudioTestCopyOrGenTag(char *pszTag, size_t cbTag, const char *pszTagUser)
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
    int rc = AudioTestCopyOrGenTag(szTag, sizeof(szTag), pszTag);
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
 * Returns if an audio test error description contains any errors or not.
 *
 * @returns \c true if it contains errors, or \c false if not.
 *
 * @param   pErr                Test error description to return error status for.
 */
bool AudioTestErrorDescFailed(PAUDIOTESTERRORDESC pErr)
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
 * @param   rc                  Result code of entry to add.
 * @param   pszDesc             Error description format string to add.
 * @param   args                Optional format arguments of \a pszDesc to add.
 */
static int audioTestErrorDescAddV(PAUDIOTESTERRORDESC pErr, int rc, const char *pszDesc, va_list args)
{
    PAUDIOTESTERRORENTRY pEntry = (PAUDIOTESTERRORENTRY)RTMemAlloc(sizeof(AUDIOTESTERRORENTRY));
    AssertReturn(pEntry, VERR_NO_MEMORY);

    if (RTStrPrintf2V(pEntry->szDesc, sizeof(pEntry->szDesc), pszDesc, args) < 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

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
 * @param   pszDesc             Error description format string to add.
 * @param   ...                 Optional format arguments of \a pszDesc to add.
 */
static int audioTestErrorDescAdd(PAUDIOTESTERRORDESC pErr, const char *pszDesc, ...)
{
    va_list va;
    va_start(va, pszDesc);

    int rc = audioTestErrorDescAddV(pErr, VERR_GENERAL_FAILURE /** @todo Fudge! */, pszDesc, va);

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

    char szPath[RTPATH_MAX];

    int rc = RTPathTemp(szPath, sizeof(szPath));
    AssertRCReturn(rc, rc);
    rc = AudioTestPathCreate(szPath, sizeof(szPath), pszTag);
    AssertRCReturn(rc, rc);

    return RTStrCopy(pszPath, cbPath, szPath);
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

    int rc = AudioTestCopyOrGenTag(pSet->szTag, sizeof(pSet->szTag), pszTag);
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
        rc = AudioTestSetObjClose(pObj);
        if (RT_SUCCESS(rc))
        {
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
        int rc2 = AudioTestSetObjClose(pObj);
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
int AudioTestSetObjWrite(PAUDIOTESTOBJ pObj, void *pvBuf, size_t cbBuf)
{
    /** @todo Generalize this function more once we have more object types. */
    AssertReturn(pObj->enmType == AUDIOTESTOBJTYPE_FILE, VERR_INVALID_PARAMETER);

    return RTFileWrite(pObj->File.hFile, pvBuf, cbBuf, NULL);
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

    /** @todo Generalize this function more once we have more object types. */
    AssertReturn(pObj->enmType == AUDIOTESTOBJTYPE_FILE, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    if (RTFileIsValid(pObj->File.hFile))
    {
        rc = RTFileClose(pObj->File.hFile);
        pObj->File.hFile = NIL_RTFILE;
    }

    return rc;
}

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
 * Packs an audio test so that it's ready for transmission.
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
    AssertReturn(audioTestManifestIsOpen(pSet), VERR_WRONG_ORDER);

    /** @todo Check and deny if \a pszOutDir is part of the set's path. */

    char szOutName[RT_ELEMENTS(AUDIOTEST_PATH_PREFIX_STR) + AUDIOTEST_TAG_MAX + 16];
    if (RTStrPrintf2(szOutName, sizeof(szOutName), "%s-%s%s",
                     AUDIOTEST_PATH_PREFIX_STR, pSet->szTag, AUDIOTEST_ARCHIVE_SUFF_STR) <= 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    char szOutPath[RTPATH_MAX];
    int rc = RTPathJoin(szOutPath, sizeof(szOutPath), pszOutDir, szOutName);
    AssertRCReturn(rc, rc);

    const char *apszArgs[10];
    unsigned    cArgs = 0;

    apszArgs[cArgs++] = "AudioTest";
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

    apszArgs[cArgs++] = "AudioTest";
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
 * Verifies an opened audio test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to verify.
 * @param   pszTag              Tag to use for verification purpose.
 * @param   pErrDesc            Where to return the test verification errors.
 *
 * @note    Test verification errors have to be checked for errors, regardless of the
 *          actual return code.
 */
int AudioTestSetVerify(PAUDIOTESTSET pSet, const char *pszTag, PAUDIOTESTERRORDESC pErrDesc)
{
    AssertReturn(audioTestManifestIsOpen(pSet), VERR_WRONG_ORDER);

    /* We ASSUME the caller has not init'd pErrDesc. */
    audioTestErrorDescInit(pErrDesc);

    char szVal[_1K]; /** @todo Enough, too much? */

    int rc2 = RTIniFileQueryValue(pSet->f.hIniFile, AUDIOTEST_INI_SEC_HDR_STR, "tag", szVal, sizeof(szVal), NULL);
    if (   RT_FAILURE(rc2)
        || RTStrICmp(pszTag, szVal))
        audioTestErrorDescAdd(pErrDesc, "Tag '%s' does not match with manifest's tag '%s'", pszTag, szVal);

    /* Only return critical stuff not related to actual testing here. */
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   WAVE File Reader.                                                                                                            *
*********************************************************************************************************************************/
/**
 * Opens a wave (.WAV) file for reading.
 *
 * @returns VBox status code.
 * @param   pszFile     The file to open.
 * @param   pWaveFile   The open wave file structure to fill in on success.
 */
int AudioTestWaveFileOpen(const char *pszFile, PAUDIOTESTWAVEFILE pWaveFile)
{
    RT_ZERO(pWaveFile->Props);
    pWaveFile->hFile = NIL_RTFILE;
    int rc = RTFileOpen(&pWaveFile->hFile, pszFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;
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
                RTRIFFWAVEFMTCHUNK  Fmt;
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
                && uBuf.Wave.Fmt.Chunk.uMagic == RTRIFFWAVEFMT_MAGIC
                && uBuf.Wave.Fmt.Chunk.cbChunk >= sizeof(uBuf.Wave.Fmt.Data))
            {
                if (uBuf.Wave.Hdr.cbFile != cbFile - sizeof(RTRIFFCHUNK))
                    RTMsgWarning("%s: File size mismatch: %#x, actual %#RX64 (ignored)",
                                 pszFile, uBuf.Wave.Hdr.cbFile, cbFile - sizeof(RTRIFFCHUNK));
                rc = VERR_VFS_BOGUS_FORMAT;
                if (uBuf.Wave.Fmt.Data.uFormatTag != RTRIFFWAVEFMT_TAG_PCM)
                    RTMsgError("%s: Unsupported uFormatTag value: %u (expected 1)", pszFile, uBuf.Wave.Fmt.Data.uFormatTag);
                else if (   uBuf.Wave.Fmt.Data.cBitsPerSample != 8
                         && uBuf.Wave.Fmt.Data.cBitsPerSample != 16
                         && uBuf.Wave.Fmt.Data.cBitsPerSample != 32)
                    RTMsgError("%s: Unsupported cBitsPerSample value: %u", pszFile, uBuf.Wave.Fmt.Data.cBitsPerSample);
                else if (   uBuf.Wave.Fmt.Data.cChannels < 1
                         || uBuf.Wave.Fmt.Data.cChannels >= 16)
                    RTMsgError("%s: Unsupported cChannels value: %u (expected 1..15)", pszFile, uBuf.Wave.Fmt.Data.cChannels);
                else if (   uBuf.Wave.Fmt.Data.uHz < 4096
                         || uBuf.Wave.Fmt.Data.uHz > 768000)
                    RTMsgError("%s: Unsupported uHz value: %u (expected 4096..768000)", pszFile, uBuf.Wave.Fmt.Data.uHz);
                else if (uBuf.Wave.Fmt.Data.cbFrame != uBuf.Wave.Fmt.Data.cChannels * uBuf.Wave.Fmt.Data.cBitsPerSample / 8)
                    RTMsgError("%s: Invalid cbFrame value: %u (expected %u)", pszFile, uBuf.Wave.Fmt.Data.cbFrame,
                               uBuf.Wave.Fmt.Data.cChannels * uBuf.Wave.Fmt.Data.cBitsPerSample / 8);
                else if (uBuf.Wave.Fmt.Data.cbRate != uBuf.Wave.Fmt.Data.cbFrame * uBuf.Wave.Fmt.Data.uHz)
                    RTMsgError("%s: Invalid cbRate value: %u (expected %u)", pszFile, uBuf.Wave.Fmt.Data.cbRate,
                               uBuf.Wave.Fmt.Data.cbFrame * uBuf.Wave.Fmt.Data.uHz);
                else
                {
                    /*
                     * Copy out the data we need from the file format structure.
                     */
                    PDMAudioPropsInit(&pWaveFile->Props, uBuf.Wave.Fmt.Data.cBitsPerSample / 8, true /*fSigned*/,
                                      uBuf.Wave.Fmt.Data.cChannels, uBuf.Wave.Fmt.Data.uHz);
                    pWaveFile->offSamples = sizeof(RTRIFFHDR) + sizeof(RTRIFFCHUNK) + uBuf.Wave.Fmt.Chunk.cbChunk;

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
                            pWaveFile->offCur = 0;
                            return VINF_SUCCESS;
                        }

                        RTMsgError("%s: Bad data header: uMagic=%#x (expected %#x), cbChunk=%#x (max %#RX64, align %u)",
                                   pszFile, uBuf.Data.Chunk.uMagic, RTRIFFWAVEDATACHUNK_MAGIC,
                                   uBuf.Data.Chunk.cbChunk, pWaveFile->cbSamples, PDMAudioPropsFrameSize(&pWaveFile->Props));
                    }
                    else
                        RTMsgError("%s: Failed to read data header: %Rrc", pszFile, rc);
                }
            }
            else
                RTMsgError("%s: Bad file header: uMagic=%#x (vs. %#x), uFileType=%#x (vs %#x), uFmtMagic=%#x (vs %#x) cbFmtChunk=%#x (min %#x)",
                           pszFile, uBuf.Wave.Hdr.uMagic, RTRIFFHDR_MAGIC, uBuf.Wave.Hdr.uFileType, RTRIFF_FILE_TYPE_WAVE,
                           uBuf.Wave.Fmt.Chunk.uMagic, RTRIFFWAVEFMT_MAGIC,
                           uBuf.Wave.Fmt.Chunk.cbChunk, sizeof(uBuf.Wave.Fmt.Data));
        }
        else
            RTMsgError("%s: Failed to read file header: %Rrc", pszFile, rc);
    }
    else
        RTMsgError("%s: Failed to query file size: %Rrc", pszFile, rc);

    RTFileClose(pWaveFile->hFile);
    pWaveFile->hFile = NIL_RTFILE;
    return rc;
}

/**
 * Closes a wave file.
 */
void AudioTestWaveFileClose(PAUDIOTESTWAVEFILE pWaveFile)
{
    RTFileClose(pWaveFile->hFile);
    pWaveFile->hFile = NIL_RTFILE;
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

