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
#define AUDIOTEST_SEC_HDR_STR        "header"
/** Maximum section name length (in UTF-8 characters). */
#define AUDIOTEST_MAX_SEC_LEN        128
/** Maximum object name length (in UTF-8 characters). */
#define AUDIOTEST_MAX_OBJ_LEN        128


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Enumeration for an audio test object type.
 */
typedef enum AUDIOTESTOBJTYPE
{
    /** Unknown / invalid, do not use. */
    AUDIOTESTOBJTYPE_UNKNOWN = 0,
    /** The test object is a file. */
    AUDIOTESTOBJTYPE_FILE,
    /** The usual 32-bit hack. */
    AUDIOTESTOBJTYPE_32BIT_HACK = 0x7fffffff
} AUDIOTESTOBJTYPE;

/**
 * Structure for keeping an audio test object file.
 */
typedef struct AUDIOTESTOBJFILE
{
    /** File handle. */
    RTFILE              hFile;
    /** Total size (in bytes). */
    size_t              cbSize;
} AUDIOTESTOBJFILE;
/** Pointer to an audio test object file. */
typedef AUDIOTESTOBJFILE *PAUDIOTESTOBJFILE;

/**
 * Enumeration for an audio test object meta data type.
 */
typedef enum AUDIOTESTOBJMETADATATYPE
{
    /** Unknown / invalid, do not use. */
    AUDIOTESTOBJMETADATATYPE_INVALID = 0,
    /** Meta data is an UTF-8 string. */
    AUDIOTESTOBJMETADATATYPE_STRING,
    /** The usual 32-bit hack. */
    AUDIOTESTOBJMETADATATYPE_32BIT_HACK = 0x7fffffff
} AUDIOTESTOBJMETADATATYPE;

/**
 * Structure for keeping a meta data block.
 */
typedef struct AUDIOTESTOBJMETA
{
    /** List node. */
    RTLISTNODE                Node;
    /** Meta data type. */
    AUDIOTESTOBJMETADATATYPE  enmType;
    /** Meta data block. */
    void                     *pvMeta;
    /** Size (in bytes) of \a pvMeta. */
    size_t                    cbMeta;
} AUDIOTESTOBJMETA;
/** Pointer to an audio test object file. */
typedef AUDIOTESTOBJMETA *PAUDIOTESTOBJMETA;

/**
 * Structure for keeping a single audio test object.
 *
 * A test object is data which is needed in order to perform and verify one or
 * more audio test case(s).
 */
typedef struct AUDIOTESTOBJINT
{
    /** List node. */
    RTLISTNODE           Node;
    /** Pointer to test set this handle is bound to. */
    PAUDIOTESTSET        pSet;
    /** As we only support .INI-style files for now, this only has the object's section name in it. */
    /** @todo Make this more generic (union, ++). */
    char                 szSec[AUDIOTEST_MAX_SEC_LEN];
    /** The UUID of the object.
     *  Used to identify an object within a test set. */
    RTUUID               Uuid;
    /** Number of references to this test object. */
    uint32_t             cRefs;
    /** Name of the test object.
     *  Must not contain a path and has to be able to serialize to disk. */
    char                 szName[64];
    /** The object type. */
    AUDIOTESTOBJTYPE     enmType;
    /** Meta data list. */
    RTLISTANCHOR         lstMeta;
    /** Union for holding the object type-specific data. */
    union
    {
        AUDIOTESTOBJFILE File;
    };
} AUDIOTESTOBJINT;
/** Pointer to an audio test object. */
typedef AUDIOTESTOBJINT *PAUDIOTESTOBJINT;

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
    /** PCM properties to use for verification. */
    PDMAUDIOPCMPROPS    PCMProps;
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
static int  audioTestObjCloseInternal(PAUDIOTESTOBJINT pObj);
static void audioTestObjFinalize(PAUDIOTESTOBJINT pObj);


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
        dbFreq = AudioTestToneGetRandomFreq();

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
 * Returns a random test tone frequency.
 */
double AudioTestToneGetRandomFreq(void)
{
    return s_aAudioTestToneFreqsHz[RTRandU32Ex(0, RT_ELEMENTS(s_aAudioTestToneFreqsHz) - 1)];
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
    }

    pErr->cErrors = 0;
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

    const ssize_t cch = RTStrPrintf2(pEntry->szDesc, sizeof(pEntry->szDesc), "Test #%RU32 %s: %s",
                                     idxTest, RT_FAILURE(rc) ? "failed" : "info", pszDescTmp);
    RTStrFree(pszDescTmp);
    AssertReturn(cch > 0, VERR_BUFFER_OVERFLOW);

    pEntry->rc = rc;

    RTListAppend(&pErr->List, &pEntry->Node);

    if (RT_FAILURE(rc))
        pErr->cErrors++;

    return VINF_SUCCESS;
}

/**
 * Adds a single error entry to an audio test error description.
 *
 * @returns VBox status code.
 * @param   pErr                Test error description to add entry for.
 * @param   idxTest             Index of failing test (zero-based).
 * @param   pszFormat           Error description format string to add.
 * @param   ...                 Optional format arguments of \a pszDesc to add.
 */
static int audioTestErrorDescAddError(PAUDIOTESTERRORDESC pErr, uint32_t idxTest, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);

    int rc = audioTestErrorDescAddV(pErr, idxTest, VERR_GENERAL_FAILURE /** @todo Fudge! */, pszFormat, va);

    va_end(va);
    return rc;
}

/**
 * Adds a single info entry to an audio test error description, va_list version.
 *
 * @returns VBox status code.
 * @param   pErr                Test error description to add entry for.
 * @param   idxTest             Index of failing test (zero-based).
 * @param   pszFormat           Error description format string to add.
 * @param   ...                 Optional format arguments of \a pszDesc to add.
 */
static int audioTestErrorDescAddInfo(PAUDIOTESTERRORDESC pErr, uint32_t idxTest, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);

    int rc = audioTestErrorDescAddV(pErr, idxTest, VINF_SUCCESS, pszFormat, va);

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
 * Gets a value as string.
 *
 * @returns VBox status code.
 * @param   phObj               Object handle to get value for.
 * @param   pszKey              Key to get value from.
 * @param   pszVal              Where to return the value on success.
 * @param   cbVal               Size (in bytes) of \a pszVal.
 */
static int audioTestObjGetStr(PAUDIOTESTOBJINT phObj, const char *pszKey, char *pszVal, size_t cbVal)
{
    /** @todo For now we only support .INI-style files. */
    AssertPtrReturn(phObj->pSet, VERR_WRONG_ORDER);
    return RTIniFileQueryValue(phObj->pSet->f.hIniFile, phObj->szSec, pszKey, pszVal, cbVal, NULL);
}

/**
 * Gets a value as boolean.
 *
 * @returns VBox status code.
 * @param   phObj               Object handle to get value for.
 * @param   pszKey              Key to get value from.
 * @param   pbVal               Where to return the value on success.
 */
static int audioTestObjGetBool(PAUDIOTESTOBJINT phObj, const char *pszKey, bool *pbVal)
{
    char szVal[_1K];
    int rc = audioTestObjGetStr(phObj, pszKey, szVal, sizeof(szVal));
    if (RT_SUCCESS(rc))
        *pbVal = RT_BOOL(RTStrToUInt8(szVal));

    return rc;
}

/**
 * Gets a value as uint8_t.
 *
 * @returns VBox status code.
 * @param   phObj               Object handle to get value for.
 * @param   pszKey              Key to get value from.
 * @param   puVal               Where to return the value on success.
 */
static int audioTestObjGetUInt8(PAUDIOTESTOBJINT phObj, const char *pszKey, uint8_t *puVal)
{
    char szVal[_1K];
    int rc = audioTestObjGetStr(phObj, pszKey, szVal, sizeof(szVal));
    if (RT_SUCCESS(rc))
        *puVal = RTStrToUInt8(szVal);

    return rc;
}

/**
 * Gets a value as uint32_t.
 *
 * @returns VBox status code.
 * @param   phObj               Object handle to get value for.
 * @param   pszKey              Key to get value from.
 * @param   puVal               Where to return the value on success.
 */
static int audioTestObjGetUInt32(PAUDIOTESTOBJINT phObj, const char *pszKey, uint32_t *puVal)
{
    char szVal[_1K];
    int rc = audioTestObjGetStr(phObj, pszKey, szVal, sizeof(szVal));
    if (RT_SUCCESS(rc))
        *puVal = RTStrToUInt32(szVal);

    return rc;
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

    PAUDIOTESTOBJINT pObj, pObjNext;
    RTListForEachSafe(&pSet->lstObj, pObj, pObjNext, AUDIOTESTOBJINT, Node)
    {
        rc = audioTestObjCloseInternal(pObj);
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

    PAUDIOTESTOBJINT pObj;
    RTListForEach(&pSet->lstObj, pObj, AUDIOTESTOBJINT, Node)
    {
        rc = audioTestManifestWrite(pSet, "\n");
        AssertRCReturn(rc, rc);
        char szUuid[AUDIOTEST_MAX_SEC_LEN];
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

    PAUDIOTESTOBJINT pObj;
    RTListForEach(&pSet->lstObj, pObj, AUDIOTESTOBJINT, Node)
    {
        int rc2 = audioTestObjCloseInternal(pObj);
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
 * @param   pObj                Where to return the pointer to the newly created object on success.
 */
int AudioTestSetObjCreateAndRegister(PAUDIOTESTSET pSet, const char *pszName, PAUDIOTESTOBJ pObj)
{
    AssertReturn(pSet->cTestsRunning == 1, VERR_WRONG_ORDER); /* No test nesting allowed. */

    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    PAUDIOTESTOBJINT pThis = (PAUDIOTESTOBJINT)RTMemAlloc(sizeof(AUDIOTESTOBJINT));
    AssertPtrReturn(pThis, VERR_NO_MEMORY);

    RTListInit(&pThis->lstMeta);

    if (RTStrPrintf2(pThis->szName, sizeof(pThis->szName), "%04RU32-%s", pSet->cObj, pszName) <= 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    /** @todo Generalize this function more once we have more object types. */

    char szObjPathAbs[RTPATH_MAX];
    int rc = audioTestSetGetObjPath(pSet, szObjPathAbs, sizeof(szObjPathAbs), pThis->szName);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileOpen(&pThis->File.hFile, szObjPathAbs, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            pThis->enmType = AUDIOTESTOBJTYPE_FILE;
            pThis->cRefs   = 1; /* Currently only 1:1 mapping. */

            RTListAppend(&pSet->lstObj, &pThis->Node);
            pSet->cObj++;

            /* Generate + set an UUID for the object and assign it to the current test. */
            rc = RTUuidCreate(&pThis->Uuid);
            AssertRCReturn(rc, rc);
            char szUuid[AUDIOTEST_MAX_OBJ_LEN];
            rc = RTUuidToStr(&pThis->Uuid, szUuid, sizeof(szUuid));
            AssertRCReturn(rc, rc);

            rc = audioTestManifestWrite(pSet, "obj%RU32_uuid=%s\n", pSet->pTestCur->cObj, szUuid);
            AssertRCReturn(rc, rc);

            AssertPtr(pSet->pTestCur);
            pSet->pTestCur->cObj++;

            *pObj = pThis;
        }
    }

    if (RT_FAILURE(rc))
        RTMemFree(pThis);

    return rc;
}

/**
 * Writes to a created audio test object.
 *
 * @returns VBox status code.
 * @param   Obj                 Audio test object to write to.
 * @param   pvBuf               Pointer to data to write.
 * @param   cbBuf               Size (in bytes) of \a pvBuf to write.
 */
int AudioTestObjWrite(AUDIOTESTOBJ Obj, const void *pvBuf, size_t cbBuf)
{
    AUDIOTESTOBJINT *pThis = Obj;

    /** @todo Generalize this function more once we have more object types. */
    AssertReturn(pThis->enmType == AUDIOTESTOBJTYPE_FILE, VERR_INVALID_PARAMETER);

    return RTFileWrite(pThis->File.hFile, pvBuf, cbBuf, NULL);
}

/**
 * Adds meta data to a test object as a string, va_list version.
 *
 * @returns VBox status code.
 * @param   Obj                 Test object to add meta data for.
 * @param   pszFormat           Format string to add.
 * @param   va                  Variable arguments list to use for the format string.
 */
static int audioTestObjAddMetadataStrV(PAUDIOTESTOBJINT pObj, const char *pszFormat, va_list va)
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
 * @param   Obj                 Test object to add meta data for.
 * @param   pszFormat           Format string to add.
 * @param   ...                 Variable arguments for the format string.
 */
int AudioTestObjAddMetadataStr(AUDIOTESTOBJ Obj, const char *pszFormat, ...)
{
    AUDIOTESTOBJINT *pThis = Obj;

    va_list va;

    va_start(va, pszFormat);
    int rc = audioTestObjAddMetadataStrV(pThis, pszFormat, va);
    va_end(va);

    return rc;
}

/**
 * Closes an opened audio test object.
 *
 * @returns VBox status code.
 * @param   Obj                 Audio test object to close.
 */
int AudioTestObjClose(AUDIOTESTOBJ Obj)
{
    AUDIOTESTOBJINT *pThis = Obj;

    if (!pThis)
        return VINF_SUCCESS;

    audioTestObjFinalize(pThis);

    return audioTestObjCloseInternal(pThis);
}

/**
 * Begins a new test of a test set.
 *
 * @returns VBox status code.
 * @param   pSet                Test set to begin new test for.
 * @param   pszDesc             Test description.
 * @param   pParms              Test parameters to use.
 * @param   ppEntry             Where to return the new test
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
    pEntry->rc      = VERR_IPE_UNINITIALIZED_STATUS;

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
    AssertReturn(pEntry->pParent->cTestsRunning == 1,                             VERR_WRONG_ORDER); /* No test nesting allowed. */
    AssertReturn(pEntry->rc                     == VERR_IPE_UNINITIALIZED_STATUS, VERR_WRONG_ORDER);

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
    AssertReturn(pEntry->pParent->cTestsRunning == 1,                             VERR_WRONG_ORDER); /* No test nesting allowed. */
    AssertReturn(pEntry->rc                     == VERR_IPE_UNINITIALIZED_STATUS, VERR_WRONG_ORDER);

    pEntry->rc = VINF_SUCCESS;

    int rc2 = audioTestManifestWrite(pEntry->pParent, "error_rc=%RI32\n", VINF_SUCCESS);
    AssertRCReturn(rc2, rc2);

    pEntry->pParent->cTestsRunning--;
    pEntry->pParent->pTestCur = NULL;

    return rc2;
}

/**
 * Returns whether a test is still running or not.
 *
 * @returns \c true if test is still running, or \c false if not.
 * @param   pEntry              Test to get running status for.
 */
bool AudioTestSetTestIsRunning(PAUDIOTESTENTRY pEntry)
{
    return (pEntry->rc == VERR_IPE_UNINITIALIZED_STATUS);
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
 * Returns whether a test set has running (active) tests or not.
 *
 * @returns \c true if it has running tests, or \c false if not.
 * @param   pSet                Test set to return status for.
 */
bool AudioTestSetIsRunning(PAUDIOTESTSET pSet)
{
    return (pSet->cTestsRunning > 0);
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
 * Retrieves an object handle of a specific test set section.
 *
 * @returns VBox status code.
 * @param   pSet                Test set the section contains.
 * @param   pszSec              Name of section to retrieve object handle for.
 * @param   phSec               Where to store the object handle on success.
 */
static int audioTestSetGetSection(PAUDIOTESTSET pSet, const char *pszSec, PAUDIOTESTOBJINT phSec)
{
    int rc = RTStrCopy(phSec->szSec, sizeof(phSec->szSec), pszSec);
    if (RT_FAILURE(rc))
        return rc;

    phSec->pSet = pSet;

    /** @todo Check for section existence. */
    RT_NOREF(pSet);

    return VINF_SUCCESS;
}

/**
 * Retrieves an object handle of a specific test.
 *
 * @returns VBox status code.
 * @param   pSet                Test set the test contains.
 * @param   idxTst              Index of test to retrieve the object handle for.
 * @param   phTst               Where to store the object handle on success.
 */
static int audioTestSetGetTest(PAUDIOTESTSET pSet, uint32_t idxTst, PAUDIOTESTOBJINT phTst)
{
    char szSec[AUDIOTEST_MAX_SEC_LEN];
    if (RTStrPrintf2(szSec, sizeof(szSec), "test_%04RU32", idxTst) <= 0)
        return VERR_BUFFER_OVERFLOW;

    return audioTestSetGetSection(pSet, szSec, phTst);
}

/**
 * Retrieves a child object of a specific parent object.
 *
 * @returns VBox status code.
 * @param   phParent            Parent object the child object contains.
 * @param   idxObj              Index of object to retrieve the object handle for.
 * @param   phObj               Where to store the object handle on success.
 */
static int audioTestObjGetChild(PAUDIOTESTOBJINT phParent, uint32_t idxObj, PAUDIOTESTOBJINT phObj)
{
    char szObj[AUDIOTEST_MAX_SEC_LEN];
    if (RTStrPrintf2(szObj, sizeof(szObj), "obj%RU32_uuid", idxObj) <= 0)
        AssertFailedReturn(VERR_BUFFER_OVERFLOW);

    char szUuid[AUDIOTEST_MAX_SEC_LEN];
    int rc = audioTestObjGetStr(phParent, szObj, szUuid, sizeof(szUuid));
    if (RT_SUCCESS(rc))
    {
        if (RTStrPrintf2(phObj->szSec, sizeof(phObj->szSec), "obj_%s", szUuid) <= 0)
            AssertFailedReturn(VERR_BUFFER_OVERFLOW);

        /** @todo Check test section existence. */

        phObj->pSet = phParent->pSet;
    }

    return rc;
}

/**
 * Verifies a value of a test verification job.
 *
 * @returns VBox status code.
 * @returns Error if the verification failed and test verification job has fKeepGoing not set.
 * @param   pVerify             Verification job to verify value for.
 * @param   phObjA              Object handle A to verify value for.
 * @param   phObjB              Object handle B to verify value for.
 * @param   pszKey              Key to verify.
 * @param   pszVal              Value to verify.
 * @param   pszErrFmt           Error format string in case the verification failed.
 * @param   ...                 Variable aruments for error format string.
 */
static int audioTestVerifyValue(PAUDIOTESTVERIFYJOB pVerify,
                                PAUDIOTESTOBJINT phObjA, PAUDIOTESTOBJINT phObjB, const char *pszKey, const char *pszVal, const char *pszErrFmt, ...)
{
    va_list va;
    va_start(va, pszErrFmt);

    char szValA[_1K];
    int rc = audioTestObjGetStr(phObjA, pszKey, szValA, sizeof(szValA));
    if (RT_SUCCESS(rc))
    {
        char szValB[_1K];
        rc = audioTestObjGetStr(phObjB, pszKey, szValB, sizeof(szValB));
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
 * @param   phObj               Object handle to open.
 * @param   ppObj               Where to return the pointer of the allocated and registered audio test object.
 */
static int audioTestObjOpen(PAUDIOTESTOBJINT phObj, PAUDIOTESTOBJINT *ppObj)
{
    PAUDIOTESTOBJINT pObj = (PAUDIOTESTOBJINT)RTMemAlloc(sizeof(AUDIOTESTOBJINT));
    AssertPtrReturn(pObj, VERR_NO_MEMORY);

    char szFileName[AUDIOTEST_MAX_SEC_LEN];
    int rc = audioTestObjGetStr(phObj, "obj_name", szFileName, sizeof(szFileName));
    if (RT_SUCCESS(rc))
    {
        char szFilePath[RTPATH_MAX];
        rc = RTPathJoin(szFilePath, sizeof(szFilePath), phObj->pSet->szPathAbs, szFileName);
        if (RT_SUCCESS(rc))
        {
            /** @todo Check "obj_type". */

            rc = RTFileOpen(&pObj->File.hFile, szFilePath, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc))
            {
                int rc2 = RTStrCopy(pObj->szName, sizeof(pObj->szName), szFileName);
                AssertRC(rc2);

                pObj->enmType = AUDIOTESTOBJTYPE_FILE;
                pObj->cRefs   = 1; /* Currently only 1:1 mapping. */

                RTListAppend(&phObj->pSet->lstObj, &pObj->Node);
                phObj->pSet->cObj++;

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
 * @param   Obj                 Object to close.
 */
static int audioTestObjCloseInternal(PAUDIOTESTOBJINT pObj)
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
 * @param   Obj                 Test object to finalize.
 */
static void audioTestObjFinalize(PAUDIOTESTOBJINT pObj)
{
    /** @todo Generalize this function more once we have more object types. */
    AssertReturnVoid(pObj->enmType == AUDIOTESTOBJTYPE_FILE);

    if (RTFileIsValid(pObj->File.hFile))
        pObj->File.cbSize = RTFileTell(pObj->File.hFile);
}

/**
 * Retrieves tone PCM properties of an object.
 *
 * @returns VBox status code.
 * @param   phObj               Object to retrieve PCM properties for.
 * @param   pProps              Where to store the PCM properties on success.
 */
static int audioTestObjGetTonePcmProps(PAUDIOTESTOBJINT phObj, PPDMAUDIOPCMPROPS pProps)
{
    int rc;

    uint32_t uHz;
    rc = audioTestObjGetUInt32(phObj, "tone_pcm_hz", &uHz);
    AssertRCReturn(rc, rc);
    uint8_t cBits;
    rc = audioTestObjGetUInt8(phObj, "tone_pcm_bits", &cBits);
    AssertRCReturn(rc, rc);
    uint8_t cChan;
    rc = audioTestObjGetUInt8(phObj, "tone_pcm_channels", &cChan);
    AssertRCReturn(rc, rc);
    bool    fSigned;
    rc = audioTestObjGetBool(phObj, "tone_pcm_is_signed", &fSigned);
    AssertRCReturn(rc, rc);

    PDMAudioPropsInit(pProps, (cBits / 8), fSigned, cChan, uHz);

    return VINF_SUCCESS;
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
        int rc3 = audioTestErrorDescAddError(a_pVerJob->pErr, a_pVerJob->idxTest, a_Msg); \
        AssertRC(rc3); \
        if (!a_pVerJob->fKeepGoing) \
            return VINF_SUCCESS; \
    }

#define CHECK_RC_MSG_VA_MAYBE_RET(a_rc, a_pVerJob, a_Msg, ...) \
    if (RT_FAILURE(a_rc)) \
    { \
        int rc3 = audioTestErrorDescAddError(a_pVerJob->pErr, a_pVerJob->idxTest, a_Msg, __VA_ARGS__); \
        AssertRC(rc3); \
        if (!a_pVerJob->fKeepGoing) \
            return VINF_SUCCESS; \

/**
 * Does the actual PCM data verification of a test tone.
 *
 * @returns VBox status code.
 * @param   pVerJob             Verification job to verify PCM data for.
 * @param   phTestA             Test handle A of test to verify PCM data for.
 * @param   phTestB             Test handle B of test to verify PCM data for.
 */
static int audioTestVerifyTestToneData(PAUDIOTESTVERIFYJOB pVerJob, PAUDIOTESTOBJINT phTestA, PAUDIOTESTOBJINT phTestB)
{
    int rc;

    /** @todo For now ASSUME that we only have one object per test. */

    AUDIOTESTOBJINT hObjA;

    rc = audioTestObjGetChild(phTestA, 0 /* idxObj */, &hObjA);
    CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Unable to get object A");

    PAUDIOTESTOBJINT pObjA;
    rc = audioTestObjOpen(&hObjA, &pObjA);
    CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Unable to open object A");

    AUDIOTESTOBJINT hObjB;
    rc = audioTestObjGetChild(phTestB, 0 /* idxObj */, &hObjB);
    CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Unable to get object B");

    PAUDIOTESTOBJINT pObjB;
    rc = audioTestObjOpen(&hObjB, &pObjB);
    CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Unable to open object B");
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

    if (!cbSizeA)
    {
        int rc2 = audioTestErrorDescAddError(pVerJob->pErr, pVerJob->idxTest, "File '%s' is empty", pObjA->szName);
        AssertRC(rc2);
    }

    if (!cbSizeB)
    {
        int rc2 = audioTestErrorDescAddError(pVerJob->pErr, pVerJob->idxTest, "File '%s' is empty", pObjB->szName);
        AssertRC(rc2);
    }

    if (cbSizeA != cbSizeB)
    {
        size_t const cbDiffAbs = cbSizeA > cbSizeB ? cbSizeA - cbSizeB : cbSizeB - cbSizeA;

        int rc2 = audioTestErrorDescAddInfo(pVerJob->pErr, pVerJob->idxTest, "File '%s' is %zu bytes (%zums)",
                                            pObjA->szName, cbSizeA, PDMAudioPropsBytesToMilli(&pVerJob->PCMProps, cbSizeA));
        AssertRC(rc2);
        rc2 = audioTestErrorDescAddInfo(pVerJob->pErr, pVerJob->idxTest, "File '%s' is %zu bytes (%zums)",
                                        pObjB->szName, cbSizeB, PDMAudioPropsBytesToMilli(&pVerJob->PCMProps, cbSizeB));
        AssertRC(rc2);

        rc2 = audioTestErrorDescAddInfo(pVerJob->pErr, pVerJob->idxTest, "File '%s' is %u%% (%zu bytes, %zums) %s than '%s'",
                                        pObjA->szName,
                                        cbSizeA > cbSizeB ? 100 - ((cbSizeB * 100) / cbSizeA) : 100 - ((cbSizeA * 100) / cbSizeB),
                                        cbDiffAbs, PDMAudioPropsBytesToMilli(&pVerJob->PCMProps, (uint32_t)cbDiffAbs),
                                        cbSizeA > cbSizeB ? "bigger" : "smaller",
                                        pObjB->szName);
        AssertRC(rc2);
    }

    if (!audioTestFilesCompareBinary(pObjA->File.hFile, pObjB->File.hFile, cbSizeA))
    {
        /** @todo Add more sophisticated stuff here. */

        int rc2 = audioTestErrorDescAddInfo(pVerJob->pErr, pVerJob->idxTest, "Files '%s' and '%s' have different content",
                                            pObjA->szName, pObjB->szName);
        AssertRC(rc2);
    }

    rc = audioTestObjCloseInternal(pObjA);
    AssertRCReturn(rc, rc);
    rc = audioTestObjCloseInternal(pObjB);
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
 * @param   phTestA             Test handle of test tone A to verify tone B with.
 * @param   phTestB             Test handle of test tone B to verify tone A with.*
 */
static int audioTestVerifyTestTone(PAUDIOTESTVERIFYJOB pVerify, PAUDIOTESTOBJINT phTestA, PAUDIOTESTOBJINT phTestB)
{
    int rc;

    /*
     * Verify test parameters.
     * More important items have precedence.
     */
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "error_rc", "0", "Test was reported as failed");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "obj_count", NULL, "Object counts don't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_freq_hz", NULL, "Tone frequency doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_prequel_ms", NULL, "Tone prequel (ms) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_duration_ms", NULL, "Tone duration (ms) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_sequel_ms", NULL, "Tone sequel (ms) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_volume_percent", NULL, "Tone volume (percent) doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_pcm_hz", NULL, "Tone PCM Hz doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_pcm_channels", NULL, "Tone PCM channels don't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_pcm_bits", NULL, "Tone PCM bits don't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);
    rc = audioTestVerifyValue(pVerify, phTestA, phTestB, "tone_pcm_is_signed", NULL, "Tone PCM signed bit doesn't match");
    CHECK_RC_MAYBE_RET(rc, pVerify);

    rc = audioTestObjGetTonePcmProps(phTestA, &pVerify->PCMProps);
    CHECK_RC_MAYBE_RET(rc, pVerify);

    /*
     * Now the fun stuff, PCM data analysis.
     */
    rc = audioTestVerifyTestToneData(pVerify, phTestA, phTestB);
    if (RT_FAILURE(rc))
    {
       int rc2 = audioTestErrorDescAddError(pVerify->pErr, pVerify->idxTest, "Verififcation of test tone data failed\n");
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
    VerJob.fKeepGoing = true; /** @todo Make this configurable. */

    PAUDIOTESTVERIFYJOB pVerJob = &VerJob;

    int rc;

    /*
     * Compare obvious values first.
     */
    AUDIOTESTOBJINT hHdrA;
    rc = audioTestSetGetSection(pVerJob->pSetA, AUDIOTEST_SEC_HDR_STR, &hHdrA);
    CHECK_RC_MAYBE_RET(rc, pVerJob);

    AUDIOTESTOBJINT hHdrB;
    rc = audioTestSetGetSection(pVerJob->pSetB, AUDIOTEST_SEC_HDR_STR, &hHdrB);
    CHECK_RC_MAYBE_RET(rc, pVerJob);

    rc = audioTestVerifyValue(&VerJob, &hHdrA, &hHdrB,   "magic",        "vkat_ini",    "Manifest magic wrong");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdrA, &hHdrB,   "ver",          "1"       ,    "Manifest version wrong");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdrA, &hHdrB,   "tag",          NULL,          "Manifest tags don't match");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdrA, &hHdrB,   "test_count",   NULL,          "Test counts don't match");
    CHECK_RC_MAYBE_RET(rc, pVerJob);
    rc = audioTestVerifyValue(&VerJob, &hHdrA, &hHdrB,   "obj_count",    NULL,          "Object counts don't match");
    CHECK_RC_MAYBE_RET(rc, pVerJob);

    /*
     * Compare ran tests.
     */
    uint32_t cTests;
    rc = audioTestObjGetUInt32(&hHdrA, "test_count", &cTests);
    AssertRCReturn(rc, rc);

    for (uint32_t i = 0; i < cTests; i++)
    {
        VerJob.idxTest = i;

        AUDIOTESTOBJINT hTestA;
        rc = audioTestSetGetTest(VerJob.pSetA, i, &hTestA);
        CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Test A not found");

        AUDIOTESTOBJINT hTestB;
        rc = audioTestSetGetTest(VerJob.pSetB, i, &hTestB);
        CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Test B not found");

        AUDIOTESTTYPE enmTestTypeA = AUDIOTESTTYPE_INVALID;
        rc = audioTestObjGetUInt32(&hTestA, "test_type", (uint32_t *)&enmTestTypeA);
        CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Test type A not found");

        AUDIOTESTTYPE enmTestTypeB = AUDIOTESTTYPE_INVALID;
        rc = audioTestObjGetUInt32(&hTestB, "test_type", (uint32_t *)&enmTestTypeB);
        CHECK_RC_MSG_MAYBE_RET(rc, pVerJob, "Test type B not found");

        switch (enmTestTypeA)
        {
            case AUDIOTESTTYPE_TESTTONE_PLAY:
            {
                if (enmTestTypeB == AUDIOTESTTYPE_TESTTONE_RECORD)
                    rc = audioTestVerifyTestTone(&VerJob, &hTestA, &hTestB);
                else
                    rc = audioTestErrorDescAddError(pErrDesc, i, "Playback test types don't match (set A=%#x, set B=%#x)",
                                                    enmTestTypeA, enmTestTypeB);
                break;
            }

            case AUDIOTESTTYPE_TESTTONE_RECORD:
            {
                if (enmTestTypeB == AUDIOTESTTYPE_TESTTONE_PLAY)
                    rc = audioTestVerifyTestTone(&VerJob, &hTestB, &hTestA);
                else
                    rc = audioTestErrorDescAddError(pErrDesc, i, "Recording test types don't match (set A=%#x, set B=%#x)",
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

