/* $Id$ */
/** @file
 * Audio testing routines.
 * Common code which is being used by the ValidationKit audio test (VKAT)
 * and the debug / ValdikationKit audio driver(s).
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

#ifndef VBOX_INCLUDED_SRC_Audio_AudioTest_h
#define VBOX_INCLUDED_SRC_Audio_AudioTest_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @todo Some stuff here can be private-only to the implementation. */

/** Maximum length in characters an audio test tag can have. */
#define AUDIOTEST_TAG_MAX               64
/** Maximum length in characters a single audio test error description can have. */
#define AUDIOTEST_ERROR_DESC_MAX        128
/** Prefix for audio test (set) directories. */
#define AUDIOTEST_PATH_PREFIX_STR       "vkat"

/**
 * Enumeration for an audio test tone (wave) type.
 */
typedef enum AUDIOTESTTONETYPE
{
    /** Invalid type. */
    AUDIOTESTTONETYPE_INVALID = 0,
    /** Sine wave. */
    AUDIOTESTTONETYPE_SINE,
    /** Square wave. Not implemented yet. */
    AUDIOTESTTONETYPE_SQUARE,
    /** Triangluar wave. Not implemented yet. */
    AUDIOTESTTONETYPE_TRIANGLE,
    /** Sawtooth wave. Not implemented yet. */
    AUDIOTESTTONETYPE_SAWTOOTH,
    /** The usual 32-bit hack. */
    AUDIOTESTTONETYPE_32BIT_HACK = 0x7fffffff
} AUDIOTESTTONETYPE;

/**
 * Structure for handling an audio (sine wave) test tone.
 */
typedef struct AUDIOTESTTONE
{
    /** The tone's wave type. */
    AUDIOTESTTONETYPE   enmType;
    /** The PCM properties. */
    PDMAUDIOPCMPROPS    Props;
    /** Current sample index for generate the sine wave. */
    uint64_t            uSample;
    /** The fixed portion of the sin() input. */
    double              rdFixed;
    /** Frequency (in Hz) of the sine wave to generate. */
    double              rdFreqHz;
} AUDIOTESTTONE;
/** Pointer to an audio test tone. */
typedef AUDIOTESTTONE *PAUDIOTESTTONE;

/**
 * Structure for handling audio test tone parameters.
 */
typedef struct AUDIOTESTTONEPARMS
{
    /** The PCM properties. */
    PDMAUDIOPCMPROPS Props;
    /** Prequel (in ms) to play silence. Optional and can be set to 0. */
    RTMSINTERVAL     msPrequel;
    /** Duration (in ms) to play the test tone. */
    RTMSINTERVAL     msDuration;
    /** Sequel (in ms) to play silence. Optional and can be set to 0. */
    RTMSINTERVAL     msSequel;
    /** Volume (in percent, 0-100) to use.
     *  If set to 0, the tone is muted (i.e. silent). */
    uint8_t          uVolumePercent;
} AUDIOTESTTONEPARMS;
/** Pointer to audio test tone parameters. */
typedef AUDIOTESTTONEPARMS *PAUDIOTESTTONEPARMS;

/**
 * Enumeration for the test set mode.
 */
typedef enum AUDIOTESTSETMODE
{
    /** Invalid test set mode. */
    AUDIOTESTSETMODE_INVALID = 0,
    /** Test set is being created (testing in progress). */
    AUDIOTESTSETMODE_TEST,
    /** Existing test set is being verified. */
    AUDIOTESTSETMODE_VERIFY,
    /** The usual 32-bit hack. */
    AUDIOTESTSETMODE_32BIT_HACK = 0x7fffffff
} AUDIOTESTSETMODE;

/**
 * Enumeration to specify an audio test type.
 */
typedef enum AUDIOTESTTYPE
{
    /** Invalid test type, do not use. */
    AUDIOTESTTYPE_INVALID = 0,
    /** Play a test tone. */
    AUDIOTESTTYPE_TESTTONE,
        /** The usual 32-bit hack. */
    AUDIOTESTTYPE_32BIT_HACK = 0x7fffffff
} AUDIOTESTTYPE;

/**
 * Audio test request data.
 */
typedef struct AUDIOTESTPARMS
{
    /** Specifies the current test iteration. */
    uint32_t                idxCurrent;
    /** How many iterations the test should be executed. */
    uint32_t                cIterations;
    /** Audio device to use. */
    PDMAUDIOHOSTDEV         Dev;
    /** How much to delay (wait, in ms) the test being executed. */
    RTMSINTERVAL            msDelay;
    /** The test direction. */
    PDMAUDIODIR             enmDir;
    /** The test type. */
    AUDIOTESTTYPE           enmType;
    /** Union for test type-specific data. */
    union
    {
        AUDIOTESTTONEPARMS  TestTone;
    };
} AUDIOTESTPARMS;
/** Pointer to a test parameter structure. */
typedef AUDIOTESTPARMS *PAUDIOTESTPARMS;

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
    RTFILE hFile;
} AUDIOTESTOBJFILE;
/** Pointer to an audio test object file. */
typedef AUDIOTESTOBJFILE *PAUDIOTESTOBJFILE;

/**
 * Structure for keeping a single audio test object.
 *
 * A test object is data which is needed in order to perform and verify one or
 * more audio test case(s).
 */
typedef struct AUDIOTESTOBJ
{
    /** List node. */
    RTLISTNODE           Node;
    /** Name of the test object.
     *  Must not contain a path and has to be able to serialize to disk. */
    char                 szName[64];
    /** The object type. */
    AUDIOTESTOBJTYPE     enmType;
    /** Union for holding the object type-specific data. */
    union
    {
        AUDIOTESTOBJFILE File;
    };
} AUDIOTESTOBJ;
/** Pointer to an audio test object. */
typedef AUDIOTESTOBJ *PAUDIOTESTOBJ;

struct AUDIOTESTSET;

typedef struct AUDIOTESTENTRY
{
    /** List node. */
    RTLISTNODE           Node;
    AUDIOTESTSET        *pParent;
    char                 szDesc[64];
    AUDIOTESTPARMS       Parms;
    int                  rc;
} AUDIOTESTENTRY;
/** Pointer to an audio test entry. */
typedef AUDIOTESTENTRY *PAUDIOTESTENTRY;

/**
 * Structure specifying an audio test set.
 */
typedef struct AUDIOTESTSET
{
    /** The set's tag. */
    char             szTag[AUDIOTEST_TAG_MAX];
    /** Absolute path where to store the test audio data. */
    char             szPathAbs[RTPATH_MAX];
    /** Current mode the test set is in. */
    AUDIOTESTSETMODE enmMode;
    union
    {
        /** @todo r=bird: RTSTREAM not RTFILE.  That means you don't have to check
         *        every write status code and it's buffered and thus faster.  Also,
         *        you don't have to re-invent fprintf-style RTFileWrite wrappers. */
        RTFILE       hFile;
        RTINIFILE    hIniFile;
    } f;
    /** Number of test objects in lstObj. */
    uint32_t         cObj;
    /** List containing PAUDIOTESTOBJ test object entries. */
    RTLISTANCHOR     lstObj;
    /** Number of performed tests.
     *  Not necessarily bound to the test object entries above. */
    uint32_t         cTests;
    /** Absolute offset (in bytes) where to write the "test_count" value later. */
    uint64_t         offTestCount;
    /** List containing PAUDIOTESTENTRY test entries. */
    RTLISTANCHOR     lstTest;
    /** Number of tests currently running. */
    uint32_t         cTestsRunning;
    /** Number of total (test) failures. */
    uint32_t         cTotalFailures;
} AUDIOTESTSET;
/** Pointer to an audio test set. */
typedef AUDIOTESTSET *PAUDIOTESTSET;

/**
 * Structure for holding a single audio test error entry.
 */
typedef struct AUDIOTESTERRORENTRY
{
    /** The entrie's list node. */
    RTLISTNODE       Node;
    /** Additional rc. */
    int              rc;
    /** Actual error description. */
    char             szDesc[AUDIOTEST_ERROR_DESC_MAX];
} AUDIOTESTERRORENTRY;
/** Pointer to an audio test error description. */
typedef AUDIOTESTERRORENTRY *PAUDIOTESTERRORENTRY;

/**
 * Structure for holding an audio test error description.
 * This can contain multiple errors (FIFO list).
 */
typedef struct AUDIOTESTERRORDESC
{
    /** List entries containing the (FIFO-style) errors of type AUDIOTESTERRORENTRY. */
    RTLISTANCHOR     List;
    /** Number of errors in the list. */
    uint32_t         cErrors;
} AUDIOTESTERRORDESC;
/** Pointer to an audio test error description. */
typedef AUDIOTESTERRORDESC *PAUDIOTESTERRORDESC;

/**
 * An open wave (.WAV) file.
 */
typedef struct AUDIOTESTWAVEFILE
{
    /** The file handle. */
    RTFILE              hFile;
    /** The absolute file offset of the first sample */
    uint32_t            offSamples;
    /** Number of bytes of samples. */
    uint32_t            cbSamples;
    /** The current read position relative to @a offSamples.  */
    uint32_t            offCur;
    /** The PCM properties for the file format.  */
    PDMAUDIOPCMPROPS    Props;
} AUDIOTESTWAVEFILE;
/** Pointer to an open wave file. */
typedef AUDIOTESTWAVEFILE *PAUDIOTESTWAVEFILE;


void   AudioTestToneInit(PAUDIOTESTTONE pTone, PPDMAUDIOPCMPROPS pProps, double dbFreq);
double AudioTestToneInitRandom(PAUDIOTESTTONE pTone, PPDMAUDIOPCMPROPS pProps);
int    AudioTestToneGenerate(PAUDIOTESTTONE pTone, void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten);

int    AudioTestToneParamsInitRandom(PAUDIOTESTTONEPARMS pToneParams, PPDMAUDIOPCMPROPS pProps);

int    AudioTestPathCreateTemp(char *pszPath, size_t cbPath, const char *pszUUID);
int    AudioTestPathCreate(char *pszPath, size_t cbPath, const char *pszUUID);

int    AudioTestSetObjCreateAndRegister(PAUDIOTESTSET pSet, const char *pszName, PAUDIOTESTOBJ *ppObj);
int    AudioTestSetObjWrite(PAUDIOTESTOBJ pObj, void *pvBuf, size_t cbBuf);
int    AudioTestSetObjClose(PAUDIOTESTOBJ pObj);

int    AudioTestSetTestBegin(PAUDIOTESTSET pSet, const char *pszDesc, PAUDIOTESTPARMS pParms, PAUDIOTESTENTRY *ppEntry);
int    AudioTestSetTestFailed(PAUDIOTESTENTRY pEntry, int rc, const char *pszErr);
int    AudioTestSetTestDone(PAUDIOTESTENTRY pEntry);

int    AudioTestSetCreate(PAUDIOTESTSET pSet, const char *pszPath, const char *pszTag);
int    AudioTestSetDestroy(PAUDIOTESTSET pSet);
int    AudioTestSetOpen(PAUDIOTESTSET pSet, const char *pszPath);
int    AudioTestSetClose(PAUDIOTESTSET pSet);
int    AudioTestSetWipe(PAUDIOTESTSET pSet);
bool   AudioTestSetIsPacked(const char *pszPath);
int    AudioTestSetPack(PAUDIOTESTSET pSet, const char *pszOutDir, char *pszFileName, size_t cbFileName);
int    AudioTestSetUnpack(const char *pszFile, const char *pszOutDir);
int    AudioTestSetVerify(PAUDIOTESTSET pSet, const char *pszTag, PAUDIOTESTERRORDESC pErrDesc);

bool   AudioTestErrorDescFailed(PAUDIOTESTERRORDESC pErr);
void   AudioTestErrorDescDestroy(PAUDIOTESTERRORDESC pErr);

int    AudioTestWaveFileOpen(const char *pszFile, PAUDIOTESTWAVEFILE pWaveFile);
int    AudioTestWaveFileRead(PAUDIOTESTWAVEFILE pWaveFile, void *pvBuf, size_t cbBuf, size_t *pcbRead);
void   AudioTestWaveFileClose(PAUDIOTESTWAVEFILE pWaveFile);

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTest_h */

