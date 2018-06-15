/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, master command.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/fuzz.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/base64.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/json.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/tcp.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>
#include <iprt/zip.h>


/**
 * A running fuzzer state.
 */
typedef struct RTFUZZRUN
{
    /** List node. */
    RTLISTNODE                  NdFuzzed;
    /** Identifier. */
    char                        *pszId;
    /** Number of processes. */
    uint32_t                    cProcs;
    /** Maximum input size to generate. */
    size_t                      cbInputMax;
    /** The fuzzing observer state handle. */
    RTFUZZOBS                   hFuzzObs;
} RTFUZZRUN;
/** Pointer to a running fuzzer state. */
typedef RTFUZZRUN *PRTFUZZRUN;


/**
 * Fuzzing master command state.
 */
typedef struct RTFUZZCMDMASTER
{
    /** List of running fuzzers. */
    RTLISTANCHOR                LstFuzzed;
    /** The port to listen on. */
    uint16_t                    uPort;
    /** The TCP server for requests. */
    PRTTCPSERVER                hTcpSrv;
    /** The root temp directory. */
    const char                  *pszTmpDir;
    /** The root results directory. */
    const char                  *pszResultsDir;
    /** Flag whether to shutdown. */
    bool                        fShutdown;
} RTFUZZCMDMASTER;
/** Pointer to a fuzzing master command state. */
typedef RTFUZZCMDMASTER *PRTFUZZCMDMASTER;


/**
 * Wrapper around RTErrInfoSetV / RTMsgErrorV.
 *
 * @returns @a rc
 * @param   pErrInfo            Extended error info.
 * @param   rc                  The return code.
 * @param   pszFormat           The message format.
 * @param   ...                 The message format arguments.
 */
static int rtFuzzCmdMasterErrorRc(PRTERRINFO pErrInfo, int rc, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pErrInfo)
        RTErrInfoSetV(pErrInfo, rc, pszFormat, va);
    else
        RTMsgErrorV(pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Returns a running fuzzer state by the given ID.
 *
 * @returns Pointer to the running fuzzer state or NULL if not found.
 * @param   pThis               The fuzzing master command state.
 * @param   pszId               The ID to look for.
 */
static PRTFUZZRUN rtFuzzCmdMasterGetFuzzerById(PRTFUZZCMDMASTER pThis, const char *pszId)
{
    PRTFUZZRUN pIt = NULL;
    RTListForEach(&pThis->LstFuzzed, pIt, RTFUZZRUN, NdFuzzed)
    {
        if (!RTStrCmp(pIt->pszId, pszId))
            return pIt;
    }

    return NULL;
}


#if 0 /* unused */
/**
 * Processes and returns the value of the given config item in the JSON request.
 *
 * @returns IPRT status code.
 * @param   ppszStr             Where to store the pointer to the string on success.
 * @param   pszCfgItem          The config item to resolve.
 * @param   hJsonCfg            The JSON object containing the item.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessCfgString(char **ppszStr, const char *pszCfgItem, RTJSONVAL hJsonCfg, PRTERRINFO pErrInfo)
{
    int rc = RTJsonValueQueryStringByName(hJsonCfg, pszCfgItem, ppszStr);
    if (RT_FAILURE(rc))
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to query string value of \"%s\"", pszCfgItem);

    return rc;
}


/**
 * Processes and returns the value of the given config item in the JSON request.
 *
 * @returns IPRT status code.
 * @param   pfVal               Where to store the config value on success.
 * @param   pszCfgItem          The config item to resolve.
 * @param   hJsonCfg            The JSON object containing the item.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessCfgBool(bool *pfVal, const char *pszCfgItem, RTJSONVAL hJsonCfg, PRTERRINFO pErrInfo)
{
    int rc = RTJsonValueQueryBooleanByName(hJsonCfg, pszCfgItem, pfVal);
    if (RT_FAILURE(rc))
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to query boolean value of \"%s\"", pszCfgItem);

    return rc;
}
#endif


/**
 * Processes and returns the value of the given config item in the JSON request.
 *
 * @returns IPRT status code.
 * @param   pfVal               Where to store the config value on success.
 * @param   pszCfgItem          The config item to resolve.
 * @param   hJsonCfg            The JSON object containing the item.
 * @param   fDef                Default value if the item wasn't found.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessCfgBoolDef(bool *pfVal, const char *pszCfgItem, RTJSONVAL hJsonCfg, bool fDef, PRTERRINFO pErrInfo)
{
    int rc = RTJsonValueQueryBooleanByName(hJsonCfg, pszCfgItem, pfVal);
    if (rc == VERR_NOT_FOUND)
    {
        *pfVal = fDef;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to query boolean value of \"%s\"", pszCfgItem);

    return rc;
}


/**
 * Processes and returns the value of the given config item in the JSON request.
 *
 * @returns IPRT status code.
 * @param   pcbVal              Where to store the config value on success.
 * @param   pszCfgItem          The config item to resolve.
 * @param   hJsonCfg            The JSON object containing the item.
 * @param   cbDef               Default value if the item wasn't found.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessCfgSizeDef(size_t *pcbVal, const char *pszCfgItem, RTJSONVAL hJsonCfg, size_t cbDef, PRTERRINFO pErrInfo)
{
    int64_t i64Val = 0;
    int rc = RTJsonValueQueryIntegerByName(hJsonCfg, pszCfgItem, &i64Val);
    if (rc == VERR_NOT_FOUND)
    {
        *pcbVal = cbDef;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to query size_t value of \"%s\"", pszCfgItem);
    else if (i64Val < 0 || (size_t)i64Val != (uint64_t)i64Val)
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_OUT_OF_RANGE, "JSON request malformed: Integer \"%s\" is out of range", pszCfgItem);
    else
        *pcbVal = (size_t)i64Val;

    return rc;
}


/**
 * Processes and returns the value of the given config item in the JSON request.
 *
 * @returns IPRT status code.
 * @param   pcbVal              Where to store the config value on success.
 * @param   pszCfgItem          The config item to resolve.
 * @param   hJsonCfg            The JSON object containing the item.
 * @param   cbDef               Default value if the item wasn't found.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessCfgU32Def(uint32_t *pu32Val, const char *pszCfgItem, RTJSONVAL hJsonCfg, size_t u32Def, PRTERRINFO pErrInfo)
{
    int64_t i64Val = 0;
    int rc = RTJsonValueQueryIntegerByName(hJsonCfg, pszCfgItem, &i64Val);
    if (rc == VERR_NOT_FOUND)
    {
        *pu32Val = u32Def;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to query uint32_t value of \"%s\"", pszCfgItem);
    else if (i64Val < 0 || (uint32_t)i64Val != (uint64_t)i64Val)
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_OUT_OF_RANGE, "JSON request malformed: Integer \"%s\" is out of range", pszCfgItem);
    else
        *pu32Val = (uint32_t)i64Val;

    return rc;
}


/**
 * Processes binary related configs for the given fuzzing run.
 *
 * @returns IPRT status code.
 * @param   pFuzzRun            The fuzzing run.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessBinaryCfg(PRTFUZZRUN pFuzzRun, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    RTJSONVAL hJsonVal;
    int rc = RTJsonValueQueryByName(hJsonRoot, "BinaryPath", &hJsonVal);
    if (RT_SUCCESS(rc))
    {
        const char *pszBinary = RTJsonValueGetString(hJsonVal);
        if (RT_LIKELY(pszBinary))
        {
            bool fFileInput = false;
            rc = rtFuzzCmdMasterFuzzRunProcessCfgBoolDef(&fFileInput, "FileInput", hJsonRoot, false, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                uint32_t fFlags = 0;
                if (fFileInput)
                    fFlags |= RTFUZZ_OBS_BINARY_F_INPUT_FILE;
                rc = RTFuzzObsSetTestBinary(pFuzzRun->hFuzzObs, pszBinary, fFlags);
                if (RT_FAILURE(rc))
                    rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Failed to add the binary path for the fuzzing run");
            }
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "JSON request malformed: \"BinaryPath\" is not a string");
        RTJsonValueRelease(hJsonVal);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to query value of \"BinaryPath\"");

    return rc;
}


/**
 * Processes argument related configs for the given fuzzing run.
 *
 * @returns IPRT status code.
 * @param   pFuzzRun            The fuzzing run.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessArgCfg(PRTFUZZRUN pFuzzRun, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    RTJSONVAL hJsonValArgArray;
    int rc = RTJsonValueQueryByName(hJsonRoot, "Arguments", &hJsonValArgArray);
    if (RT_SUCCESS(rc))
    {
        unsigned cArgs = 0;
        rc = RTJsonValueQueryArraySize(hJsonValArgArray, &cArgs);
        if (RT_SUCCESS(rc))
        {
            if (cArgs > 0)
            {
                const char **papszArgs = (const char **)RTMemAllocZ(cArgs * sizeof(const char *));
                RTJSONVAL *pahJsonVal = (RTJSONVAL *)RTMemAllocZ(cArgs * sizeof(RTJSONVAL));
                if (RT_LIKELY(papszArgs && pahJsonVal))
                {
                    unsigned idx = 0;

                    for (idx = 0; idx < cArgs && RT_SUCCESS(rc); idx++)
                    {
                        rc = RTJsonValueQueryByIndex(hJsonValArgArray, idx, &pahJsonVal[idx]);
                        if (RT_SUCCESS(rc))
                        {
                            papszArgs[idx] = RTJsonValueGetString(pahJsonVal[idx]);
                            if (RT_UNLIKELY(!papszArgs[idx]))
                                rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "Argument %u is not a string", idx);
                        }
                    }

                    if (RT_SUCCESS(rc))
                    {
                        rc = RTFuzzObsSetTestBinaryArgs(pFuzzRun->hFuzzObs, papszArgs, cArgs);
                        if (RT_FAILURE(rc))
                            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Failed to set arguments for the fuzzing run");
                    }

                    /* Release queried values. */
                    while (idx > 0)
                    {
                        RTJsonValueRelease(pahJsonVal[idx - 1]);
                        idx--;
                    }
                }
                else
                    rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_NO_MEMORY, "Out of memory allocating memory for the argument vector");

                if (papszArgs)
                    RTMemFree(papszArgs);
                if (pahJsonVal)
                    RTMemFree(pahJsonVal);
            }
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: \"Arguments\" is not an array");
        RTJsonValueRelease(hJsonValArgArray);
    }

    return rc;
}


/**
 * Processes the given seed and adds it to the input corpus.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   pszCompression      Compression used for the seed.
 * @param   pszSeed             The seed as a base64 encoded string.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessSeed(RTFUZZCTX hFuzzCtx, const char *pszCompression, const char *pszSeed, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    ssize_t cbSeedDecoded = RTBase64DecodedSize(pszSeed, NULL);
    if (cbSeedDecoded > 0)
    {
        uint8_t *pbSeedDecoded = (uint8_t *)RTMemAllocZ(cbSeedDecoded);
        if (RT_LIKELY(pbSeedDecoded))
        {
            rc = RTBase64Decode(pszSeed, pbSeedDecoded, cbSeedDecoded, NULL, NULL);
            if (RT_SUCCESS(rc))
            {
                /* Decompress if applicable. */
                if (!RTStrICmp(pszCompression, "None"))
                    rc = RTFuzzCtxCorpusInputAdd(hFuzzCtx, pbSeedDecoded, cbSeedDecoded);
                else
                {
                    RTVFSIOSTREAM hVfsIosSeed;
                    rc = RTVfsIoStrmFromBuffer(RTFILE_O_READ, pbSeedDecoded, cbSeedDecoded, &hVfsIosSeed);
                    if (RT_SUCCESS(rc))
                    {
                        RTVFSIOSTREAM hVfsDecomp;

                        if (!RTStrICmp(pszCompression, "Gzip"))
                            rc = RTZipGzipDecompressIoStream(hVfsIosSeed, RTZIPGZIPDECOMP_F_ALLOW_ZLIB_HDR, &hVfsDecomp);
                        else
                            rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "Request error: Compression \"%s\" is not known", pszCompression);

                        if (RT_SUCCESS(rc))
                        {
                            RTVFSFILE hVfsFile;
                            rc = RTVfsMemFileCreate(hVfsDecomp, 2 * _1M, &hVfsFile);
                            if (RT_SUCCESS(rc))
                            {
                                rc = RTVfsFileSeek(hVfsFile, 0, RTFILE_SEEK_BEGIN, NULL);
                                if (RT_SUCCESS(rc))
                                {
                                    /* The VFS file contains the buffer for the seed now. */
                                    rc = RTFuzzCtxCorpusInputAddFromVfsFile(hFuzzCtx, hVfsFile);
                                    if (RT_FAILURE(rc))
                                        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to add input seed");
                                    RTVfsFileRelease(hVfsFile);
                                }
                                else
                                    rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "Request error: Failed to seek to the beginning of the seed");
                            }
                            else
                                rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "Request error: Failed to decompress input seed");

                            RTVfsIoStrmRelease(hVfsDecomp);
                        }

                        RTVfsIoStrmRelease(hVfsIosSeed);
                    }
                    else
                        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to create I/O stream from seed buffer");
                }
            }
            else
                rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to decode the seed string");

            RTMemFree(pbSeedDecoded);
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_NO_MEMORY, "Request error: Failed to allocate %zd bytes of memory for the seed", cbSeedDecoded);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "JSON request malformed: Couldn't find \"Seed\" doesn't contain a base64 encoded value");

    return rc;
}


/**
 * Processes a signle input seed for the given fuzzing run.
 *
 * @returns IPRT status code.
 * @param   pFuzzRun            The fuzzing run.
 * @param   hJsonSeed           The seed node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessInputSeedSingle(PRTFUZZRUN pFuzzRun, RTJSONVAL hJsonSeed, PRTERRINFO pErrInfo)
{
    RTFUZZCTX hFuzzCtx;
    int rc = RTFuzzObsQueryCtx(pFuzzRun->hFuzzObs, &hFuzzCtx);
    if (RT_SUCCESS(rc))
    {
        RTJSONVAL hJsonValComp;
        rc = RTJsonValueQueryByName(hJsonSeed, "Compression", &hJsonValComp);
        if (RT_SUCCESS(rc))
        {
            const char *pszCompression = RTJsonValueGetString(hJsonValComp);
            if (RT_LIKELY(pszCompression))
            {
                RTJSONVAL hJsonValSeed;
                rc = RTJsonValueQueryByName(hJsonSeed, "Seed", &hJsonValSeed);
                if (RT_SUCCESS(rc))
                {
                    const char *pszSeed = RTJsonValueGetString(hJsonValSeed);
                    if (RT_LIKELY(pszSeed))
                        rc = rtFuzzCmdMasterFuzzRunProcessSeed(hFuzzCtx, pszCompression, pszSeed, pErrInfo);
                    else
                        rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "JSON request malformed: \"Seed\" value is not a string");

                    RTJsonValueRelease(hJsonValSeed);
                }
                else
                    rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Couldn't find \"Seed\" value");
            }
            else
                rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_INVALID_STATE, "JSON request malformed: \"Compression\" value is not a string");

            RTJsonValueRelease(hJsonValComp);
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Couldn't find \"Compression\" value");

        RTFuzzCtxRelease(hFuzzCtx);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Failed to query fuzzing context from observer");

    return rc;
}


/**
 * Processes input seed related configs for the given fuzzing run.
 *
 * @returns IPRT status code.
 * @param   pFuzzRun            The fuzzing run.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessInputSeeds(PRTFUZZRUN pFuzzRun, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    RTJSONVAL hJsonValSeedArray;
    int rc = RTJsonValueQueryByName(hJsonRoot, "InputSeeds", &hJsonValSeedArray);
    if (RT_SUCCESS(rc))
    {
        RTJSONIT hIt;
        rc = RTJsonIteratorBegin(hJsonValSeedArray, &hIt);
        if (RT_SUCCESS(rc))
        {
            RTJSONVAL hJsonInpSeed;
            while (   RT_SUCCESS(rc)
                   && RTJsonIteratorQueryValue(hIt, &hJsonInpSeed, NULL) != VERR_JSON_ITERATOR_END)
            {
                rc = rtFuzzCmdMasterFuzzRunProcessInputSeedSingle(pFuzzRun, hJsonInpSeed, pErrInfo);
                RTJsonValueRelease(hJsonInpSeed);
                if (RT_FAILURE(rc))
                    break;
                rc = RTJsonIteratorNext(hIt);
            }

            if (rc == VERR_JSON_ITERATOR_END)
                rc = VINF_SUCCESS;
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Failed to create array iterator");

        RTJsonValueRelease(hJsonValSeedArray);
    }

    return rc;
}


/**
 * Processes miscellaneous config items.
 *
 * @returns IPRT status code.
 * @param   pFuzzRun            The fuzzing run.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterFuzzRunProcessMiscCfg(PRTFUZZRUN pFuzzRun, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    size_t cbTmp;
    int rc = rtFuzzCmdMasterFuzzRunProcessCfgSizeDef(&cbTmp, "InputSeedMax", hJsonRoot, 0, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        RTFUZZCTX hFuzzCtx;
        rc = RTFuzzObsQueryCtx(pFuzzRun->hFuzzObs, &hFuzzCtx);
        AssertRC(rc);

        rc = RTFuzzCtxCfgSetInputSeedMaximum(hFuzzCtx, cbTmp);
        if (RT_FAILURE(rc))
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to set maximum input seed size to %zu", cbTmp);
    }

    if (RT_SUCCESS(rc))
        rc = rtFuzzCmdMasterFuzzRunProcessCfgU32Def(&pFuzzRun->cProcs, "FuzzingProcs", hJsonRoot, 0, pErrInfo);

    return rc;
}


/**
 * Creates a new fuzzing run with the given ID.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   pszId               The ID to use.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterCreateFuzzRunWithId(PRTFUZZCMDMASTER pThis, const char *pszId, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    PRTFUZZRUN pFuzzRun = (PRTFUZZRUN)RTMemAllocZ(sizeof(*pFuzzRun));
    if (RT_LIKELY(pFuzzRun))
    {
        pFuzzRun->pszId = RTStrDup(pszId);
        if (RT_LIKELY(pFuzzRun->pszId))
        {
            rc = RTFuzzObsCreate(&pFuzzRun->hFuzzObs);
            if (RT_SUCCESS(rc))
            {
                rc = rtFuzzCmdMasterFuzzRunProcessBinaryCfg(pFuzzRun, hJsonRoot, pErrInfo);
                if (RT_SUCCESS(rc))
                    rc = rtFuzzCmdMasterFuzzRunProcessArgCfg(pFuzzRun, hJsonRoot, pErrInfo);
                if (RT_SUCCESS(rc))
                    rc = rtFuzzCmdMasterFuzzRunProcessInputSeeds(pFuzzRun, hJsonRoot, pErrInfo);
                if (RT_SUCCESS(rc))
                    rc = rtFuzzCmdMasterFuzzRunProcessMiscCfg(pFuzzRun, hJsonRoot, pErrInfo);
                if (RT_SUCCESS(rc))
                {
                    /* Create temp directories. */
                    char szTmpDir[RTPATH_MAX];
                    rc = RTPathJoin(&szTmpDir[0], sizeof(szTmpDir), pThis->pszTmpDir, pFuzzRun->pszId);
                    AssertRC(rc);
                    rc = RTDirCreate(szTmpDir, 0700,   RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_SET
                                                     | RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_NOT_CRITICAL);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTFuzzObsSetTmpDirectory(pFuzzRun->hFuzzObs, szTmpDir);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTPathJoin(&szTmpDir[0], sizeof(szTmpDir), pThis->pszResultsDir, pFuzzRun->pszId);
                            AssertRC(rc);
                            rc = RTDirCreate(szTmpDir, 0700,   RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_SET
                                                             | RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_NOT_CRITICAL);
                            if (RT_SUCCESS(rc))
                            {
                                rc = RTFuzzObsSetResultDirectory(pFuzzRun->hFuzzObs, szTmpDir);
                                if (RT_SUCCESS(rc))
                                {
                                    /* Start fuzzing. */
                                    RTListAppend(&pThis->LstFuzzed, &pFuzzRun->NdFuzzed);
                                    rc = RTFuzzObsExecStart(pFuzzRun->hFuzzObs, pFuzzRun->cProcs);
                                    if (RT_FAILURE(rc))
                                        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to start fuzzing with %Rrc", rc);
                                }
                                else
                                    rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to set results directory to %s", szTmpDir);
                            }
                            else
                                rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to create results directory %s", szTmpDir);
                        }
                        else
                            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to set temporary directory to %s", szTmpDir);
                    }
                    else
                        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Failed to create temporary directory %s", szTmpDir);
                }
            }
        }
        else
            rc = VERR_NO_STR_MEMORY;
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_NO_MEMORY, "Request error: Out of memory allocating the fuzzer state");

    return rc;
}


/**
 * Resolves the fuzzing run from the given ID config item and the given JSON request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pszIdItem           The JSON item which contains the ID of the fuzzing run.
 * @param   ppFuzzRun           Where to store the pointer to the fuzzing run on success.
 */
static int rtFuzzCmdMasterQueryFuzzRunFromJson(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, const char *pszIdItem, PRTERRINFO pErrInfo,
                                               PRTFUZZRUN *ppFuzzRun)
{
    RTJSONVAL hJsonValId;
    int rc = RTJsonValueQueryByName(hJsonRoot, pszIdItem, &hJsonValId);
    if (RT_SUCCESS(rc))
    {
        const char *pszId = RTJsonValueGetString(hJsonValId);
        if (pszId)
        {
            PRTFUZZRUN pFuzzRun = rtFuzzCmdMasterGetFuzzerById(pThis, pszId);
            if (pFuzzRun)
                *ppFuzzRun = pFuzzRun;
            else
                rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_NOT_FOUND, "Request error: The ID \"%s\" wasn't found", pszId);
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_JSON_VALUE_INVALID_TYPE, "JSON request malformed: \"Id\" is not a string value");

        RTJsonValueRelease(hJsonValId);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Couldn't find \"Id\" value");
    return rc;
}


/**
 * Processes the "StartFuzzing" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonRoot           The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterProcessJsonReqStart(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    RTJSONVAL hJsonValId;
    int rc = RTJsonValueQueryByName(hJsonRoot, "Id", &hJsonValId);
    if (RT_SUCCESS(rc))
    {
        const char *pszId = RTJsonValueGetString(hJsonValId);
        if (pszId)
        {
            PRTFUZZRUN pFuzzRun = rtFuzzCmdMasterGetFuzzerById(pThis, pszId);
            if (!pFuzzRun)
                rc = rtFuzzCmdMasterCreateFuzzRunWithId(pThis, pszId, hJsonRoot, pErrInfo);
            else
                rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_ALREADY_EXISTS, "Request error: The ID \"%s\" is already registered", pszId);
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_JSON_VALUE_INVALID_TYPE, "JSON request malformed: \"Id\" is not a string value");

        RTJsonValueRelease(hJsonValId);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Couldn't find \"Id\" value");
    return rc;
}


/**
 * Processes the "StopFuzzing" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonValRoot        The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterProcessJsonReqStop(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    PRTFUZZRUN pFuzzRun;
    int rc = rtFuzzCmdMasterQueryFuzzRunFromJson(pThis, hJsonRoot, "Id", pErrInfo, &pFuzzRun);
    if (RT_SUCCESS(rc))
    {
        RTListNodeRemove(&pFuzzRun->NdFuzzed);
        RTFuzzObsExecStop(pFuzzRun->hFuzzObs);
        RTFuzzObsDestroy(pFuzzRun->hFuzzObs);
        RTStrFree(pFuzzRun->pszId);
        RTMemFree(pFuzzRun);
    }

    return rc;
}


/**
 * Processes the "SuspendFuzzing" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonValRoot        The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterProcessJsonReqSuspend(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    PRTFUZZRUN pFuzzRun;
    int rc = rtFuzzCmdMasterQueryFuzzRunFromJson(pThis, hJsonRoot, "Id", pErrInfo, &pFuzzRun);
    if (RT_SUCCESS(rc))
    {
        rc = RTFuzzObsExecStop(pFuzzRun->hFuzzObs);
        if (RT_FAILURE(rc))
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Suspending the fuzzing process failed");
    }

    return rc;
}


/**
 * Processes the "ResumeFuzzing" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonValRoot        The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterProcessJsonReqResume(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    PRTFUZZRUN pFuzzRun;
    int rc = rtFuzzCmdMasterQueryFuzzRunFromJson(pThis, hJsonRoot, "Id", pErrInfo, &pFuzzRun);
    if (RT_SUCCESS(rc))
    {
        rc = rtFuzzCmdMasterFuzzRunProcessCfgU32Def(&pFuzzRun->cProcs, "FuzzingProcs", hJsonRoot, pFuzzRun->cProcs, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            rc = RTFuzzObsExecStart(pFuzzRun->hFuzzObs, pFuzzRun->cProcs);
            if (RT_FAILURE(rc))
                rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "Request error: Resuming the fuzzing process failed");
        }
    }

    return rc;
}


/**
 * Processes the "QueryStats" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonValRoot        The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterProcessJsonReqQueryStats(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    RT_NOREF(pThis, hJsonRoot, pErrInfo);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Processes a JSON request.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   hJsonValRoot        The root node of the JSON request.
 * @param   pErrInfo            Where to store the error information on failure, optional.
 */
static int rtFuzzCmdMasterProcessJsonReq(PRTFUZZCMDMASTER pThis, RTJSONVAL hJsonRoot, PRTERRINFO pErrInfo)
{
    RTJSONVAL hJsonValReq;
    int rc = RTJsonValueQueryByName(hJsonRoot, "Request", &hJsonValReq);
    if (RT_SUCCESS(rc))
    {
        const char *pszReq = RTJsonValueGetString(hJsonValReq);
        if (pszReq)
        {
            if (!RTStrCmp(pszReq, "StartFuzzing"))
                rc = rtFuzzCmdMasterProcessJsonReqStart(pThis, hJsonRoot, pErrInfo);
            else if (!RTStrCmp(pszReq, "StopFuzzing"))
                rc = rtFuzzCmdMasterProcessJsonReqStop(pThis, hJsonRoot, pErrInfo);
            else if (!RTStrCmp(pszReq, "SuspendFuzzing"))
                rc = rtFuzzCmdMasterProcessJsonReqSuspend(pThis, hJsonRoot, pErrInfo);
            else if (!RTStrCmp(pszReq, "ResumeFuzzing"))
                rc = rtFuzzCmdMasterProcessJsonReqResume(pThis, hJsonRoot, pErrInfo);
            else if (!RTStrCmp(pszReq, "QueryStats"))
                rc = rtFuzzCmdMasterProcessJsonReqQueryStats(pThis, hJsonRoot, pErrInfo);
            else if (!RTStrCmp(pszReq, "Shutdown"))
                pThis->fShutdown = true;
            else
                rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_JSON_VALUE_INVALID_TYPE, "JSON request malformed: \"Request\" contains unknown value \"%s\"", pszReq);
        }
        else
            rc = rtFuzzCmdMasterErrorRc(pErrInfo, VERR_JSON_VALUE_INVALID_TYPE, "JSON request malformed: \"Request\" is not a string value");

        RTJsonValueRelease(hJsonValReq);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(pErrInfo, rc, "JSON request malformed: Couldn't find \"Request\" value");

    return rc;
}


/**
 * Loads a fuzzing configuration for immediate startup from the given file.
 *
 * @returns IPRT status code.
 * @param   pThis               The fuzzing master command state.
 * @param   pszFuzzCfg          The fuzzing config to load.
 */
static int rtFuzzCmdMasterFuzzCfgLoadFromFile(PRTFUZZCMDMASTER pThis, const char *pszFuzzCfg)
{
    RTJSONVAL hJsonRoot;
    int rc = RTJsonParseFromFile(&hJsonRoot, pszFuzzCfg, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = rtFuzzCmdMasterProcessJsonReqStart(pThis, hJsonRoot, NULL);
        RTJsonValueRelease(hJsonRoot);
    }
    else
        rc = rtFuzzCmdMasterErrorRc(NULL, rc, "JSON request malformed: Couldn't load file \"%s\"", pszFuzzCfg);

    return rc;
}


/**
 * Destroys all running fuzzers for the given master state.
 *
 * @returns nothing.
 * @param   pThis               The fuzzing master command state.
 */
static void rtFuzzCmdMasterDestroy(PRTFUZZCMDMASTER pThis)
{
    RT_NOREF(pThis);
}


/**
 * Sends an ACK response to the client.
 *
 * @returns nothing.
 * @param   hSocket             The socket handle to send the ACK to.
 */
static void rtFuzzCmdMasterTcpSendAck(RTSOCKET hSocket)
{
    const char s_szSucc[] = "{ \"Status\": \"ACK\" }\n";
    RTTcpWrite(hSocket, s_szSucc, sizeof(s_szSucc));
}


/**
 * Sends an NACK response to the client.
 *
 * @returns nothing.
 * @param   hSocket             The socket handle to send the ACK to.
 * @param   pErrInfo            Optional error information to send along.
 */
static void rtFuzzCmdMasterTcpSendNAck(RTSOCKET hSocket, PRTERRINFO pErrInfo)
{
    const char s_szFail[] = "{ \"Status\": \"NACK\" }\n";
    const char s_szFailInfo[] = "{ \"Status\": \"NACK\"\n \"Information\": \"%s\" }\n";

    if (pErrInfo)
    {
        char szTmp[1024];
        ssize_t cchResp = RTStrPrintf2(szTmp, sizeof(szTmp), s_szFailInfo, pErrInfo->pszMsg);
        if (cchResp > 0)
            RTTcpWrite(hSocket, szTmp, cchResp);
        else
            RTTcpWrite(hSocket, s_szFail, strlen(s_szFail));
    }
    else
        RTTcpWrite(hSocket, s_szFail, strlen(s_szFail));
}


/**
 * TCP server serving callback for a single connection.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle of the connection.
 * @param   pvUser              Opaque user data.
 */
static DECLCALLBACK(int) rtFuzzCmdMasterTcpServe(RTSOCKET hSocket, void *pvUser)
{
    PRTFUZZCMDMASTER pThis = (PRTFUZZCMDMASTER)pvUser;
    size_t cbReqMax = _32K;
    size_t cbReq = 0;
    uint8_t *pbReq = (uint8_t *)RTMemAllocZ(cbReqMax);

    if (RT_LIKELY(pbReq))
    {
        uint8_t *pbCur = pbReq;

        for (;;)
        {
            size_t cbThisRead = cbReqMax - cbReq;
            int rc = RTTcpRead(hSocket, pbCur, cbThisRead, &cbThisRead);
            if (RT_SUCCESS(rc))
            {
                cbReq += cbThisRead;

                /* Check for a zero terminator marking the end of the request. */
                uint8_t *pbEnd = (uint8_t *)memchr(pbCur, 0, cbThisRead);
                if (pbEnd)
                {
                    /* Adjust request size, data coming after the zero terminiator is ignored right now. */
                    cbReq -= cbThisRead - (pbEnd - pbCur) + 1;

                    RTJSONVAL hJsonReq;
                    RTERRINFOSTATIC ErrInfo;
                    RTErrInfoInitStatic(&ErrInfo);

                    rc = RTJsonParseFromBuf(&hJsonReq, pbReq, cbReq, &ErrInfo.Core);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rtFuzzCmdMasterProcessJsonReq(pThis, hJsonReq, &ErrInfo.Core);
                        if (RT_SUCCESS(rc))
                            rtFuzzCmdMasterTcpSendAck(hSocket);
                        else
                            rtFuzzCmdMasterTcpSendNAck(hSocket, &ErrInfo.Core);
                        RTJsonValueRelease(hJsonReq);
                    }
                    else
                        rtFuzzCmdMasterTcpSendNAck(hSocket, &ErrInfo.Core);
                    break;
                }
                else if (cbReq == cbReqMax)
                {
                    /* Try to increase the buffer. */
                    uint8_t *pbReqNew = (uint8_t *)RTMemRealloc(pbReq, cbReqMax + _32K);
                    if (RT_LIKELY(pbReqNew))
                    {
                        cbReqMax += _32K;
                        pbReq = pbReqNew;
                        pbCur = pbReq + cbReq;
                    }
                    else
                        rtFuzzCmdMasterTcpSendNAck(hSocket, NULL);
                }
                else
                    pbCur += cbThisRead;
            }
            else
                break;
        }
    }
    else
        rtFuzzCmdMasterTcpSendNAck(hSocket, NULL);

    if (pbReq)
        RTMemFree(pbReq);

    return pThis->fShutdown ? VERR_TCP_SERVER_STOP : VINF_SUCCESS;
}


/**
 * Mainloop for the fuzzing master.
 *
 * @returns Process exit code.
 * @param   pThis               The fuzzing master command state.
 * @param   pszLoadCfg          Initial config to load.
 */
static RTEXITCODE rtFuzzCmdMasterRun(PRTFUZZCMDMASTER pThis, const char *pszLoadCfg)
{
    if (pszLoadCfg)
    {
        int rc = rtFuzzCmdMasterFuzzCfgLoadFromFile(pThis, pszLoadCfg);
        if (RT_FAILURE(rc))
            return RTEXITCODE_FAILURE;
    }

    /* Start up the control server. */
    int rc = RTTcpServerCreateEx(NULL, pThis->uPort, &pThis->hTcpSrv);
    if (RT_SUCCESS(rc))
    {
        do
        {
            rc = RTTcpServerListen(pThis->hTcpSrv, rtFuzzCmdMasterTcpServe, pThis);
        } while (rc != VERR_TCP_SERVER_STOP);
    }

    RTTcpServerDestroy(pThis->hTcpSrv);
    rtFuzzCmdMasterDestroy(pThis);
    return RTEXITCODE_SUCCESS;
}


RTR3DECL(RTEXITCODE) RTFuzzCmdMaster(unsigned cArgs, char **papszArgs)
{
    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--fuzz-config",                     'c', RTGETOPT_REQ_STRING  },
        { "--temp-dir",                        't', RTGETOPT_REQ_STRING  },
        { "--results-dir",                     'r', RTGETOPT_REQ_STRING  },
        { "--listen-port",                     'p', RTGETOPT_REQ_UINT16  },
        { "--daemonize",                       'd', RTGETOPT_REQ_NOTHING },
        { "--daemonized",                      'Z', RTGETOPT_REQ_NOTHING },
        { "--help",                            'h', RTGETOPT_REQ_NOTHING },
        { "--version",                         'V', RTGETOPT_REQ_NOTHING },
    };

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1,
                          RTGETOPTINIT_FLAGS_OPTS_FIRST);
    if (RT_SUCCESS(rc))
    {
        /* Option variables:  */
        bool fDaemonize = false;
        bool fDaemonized = false;
        const char *pszLoadCfg = NULL;
        RTFUZZCMDMASTER This;

        RTListInit(&This.LstFuzzed);
        This.hTcpSrv       = NIL_RTTCPSERVER;
        This.uPort         = 4242;
        This.pszTmpDir     = NULL;
        This.pszResultsDir = NULL;
        This.fShutdown     = false;

        /* Argument parsing loop. */
        bool fContinue = true;
        do
        {
            RTGETOPTUNION ValueUnion;
            int chOpt = RTGetOpt(&GetState, &ValueUnion);
            switch (chOpt)
            {
                case 0:
                    fContinue = false;
                    break;

                case 'c':
                    pszLoadCfg = ValueUnion.psz;
                    break;

                case 'p':
                    This.uPort = ValueUnion.u16;
                    break;

                case 't':
                    This.pszTmpDir = ValueUnion.psz;
                    break;

                case 'r':
                    This.pszResultsDir = ValueUnion.psz;
                    break;

                case 'd':
                    fDaemonize = true;
                    break;

                case 'Z':
                    fDaemonized = true;
                    fDaemonize = false;
                    break;

                case 'h':
                    RTPrintf("Usage: to be written\nOption dump:\n");
                    for (unsigned i = 0; i < RT_ELEMENTS(s_aOptions); i++)
                        RTPrintf(" -%c,%s\n", s_aOptions[i].iShort, s_aOptions[i].pszLong);
                    fContinue = false;
                    break;

                case 'V':
                    RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                    fContinue = false;
                    break;

                default:
                    rcExit = RTGetOptPrintError(chOpt, &ValueUnion);
                    fContinue = false;
                    break;
            }
        } while (fContinue);

        if (rcExit == RTEXITCODE_SUCCESS)
        {
            /*
             * Daemonize ourselves if asked to.
             */
            if (fDaemonize)
            {
                rc = RTProcDaemonize(papszArgs, "--daemonized");
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTProcDaemonize: %Rrc\n", rc);
            }
            else
                rcExit = rtFuzzCmdMasterRun(&This, pszLoadCfg);
        }
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_SYNTAX, "RTGetOptInit: %Rrc", rc);
    return rcExit;
}

