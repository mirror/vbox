/** @file
 * IPRT - Fuzzing framework
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

#ifndef ___iprt_fuzz_h
#define ___iprt_fuzz_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_fuzz RTFuzz - Data fuzzing framework
 * @ingroup grp_rt
 * @sa grp_rt_test
 * @{
 */

/** A fuzzer context handle. */
typedef struct RTFUZZCTXINT    *RTFUZZCTX;
/** Pointer to a fuzzer context handle. */
typedef RTFUZZCTX              *PRTFUZZCTX;
/** NIL fuzzer context handle. */
#define NIL_RTFUZZCTX           ((RTFUZZCTX)~(uintptr_t)0)
/** A fuzzer input handle. */
typedef struct RTFUZZINPUTINT  *RTFUZZINPUT;
/** Pointer to a fuzzer input handle. */
typedef RTFUZZINPUT            *PRTFUZZINPUT;
/** NIL fuzzer input handle. */
#define NIL_RTFUZZINPUT        ((RTFUZZINPUT)~(uintptr_t)0)


/** Fuzzing observer handle. */
typedef struct RTFUZZOBSINT    *RTFUZZOBS;
/** Pointer to a fuzzing observer handle. */
typedef RTFUZZOBS              *PRTFUZZOBS;
/** NIL fuzzing observer handle. */
#define NIL_RTFUZZOBS           ((RTFUZZOBS)~(uintptr_t)0)


/** @name RTFUZZCTX_F_XXX - Flags for RTFuzzCtxCfgSetBehavioralFlags
 * @{ */
/** Adds all generated inputs automatically to the input corpus for the owning context. */
#define RTFUZZCTX_F_BEHAVIORAL_ADD_INPUT_AUTOMATICALLY_TO_CORPUS    RT_BIT_32(0)
/** All valid behavioral modification flags. */
#define RTFUZZCTX_F_BEHAVIORAL_VALID                                (RTFUZZCTX_F_BEHAVIORAL_ADD_INPUT_AUTOMATICALLY_TO_CORPUS)
/** @} */

/**
 * Creates a new fuzzing context.
 *
 * @returns IPRT status code.
 * @param   phFuzzCtx           Where to store the handle to the fuzzing context on success.
 */
RTDECL(int) RTFuzzCtxCreate(PRTFUZZCTX phFuzzCtx);

/**
 * Creates a new fuzzing context from the given state.
 *
 * @returns IPRT status code.
 * @param   phFuzzCtx           Where to store the handle to the fuzzing context on success.
 * @param   pvState             The pointer to the fuzzing state.
 * @param   cbState             Size of the state buffer in bytes.
 */
RTDECL(int) RTFuzzCtxCreateFromState(PRTFUZZCTX phFuzzCtx, const void *pvState, size_t cbState);

/**
 * Creates a new fuzzing context loading the state from the given file.
 *
 * @returns IPRT status code.
 * @param   phFuzzCtx           Where to store the handle to the fuzzing context on success.
 * @param   pszFilename         File to load the fuzzing context from.
 */
RTDECL(int) RTFuzzCtxCreateFromStateFile(PRTFUZZCTX phFuzzCtx, const char *pszFilename);

/**
 * Retains a reference to the given fuzzing context.
 *
 * @returns New reference count on success.
 * @param   hFuzzCtx            Handle of the fuzzing context.
 */
RTDECL(uint32_t) RTFuzzCtxRetain(RTFUZZCTX hFuzzCtx);

/**
 * Releases a reference from the given fuzzing context, destroying it when reaching 0.
 *
 * @returns New reference count on success, 0 if the fuzzing context got destroyed.
 * @param   hFuzzCtx            Handle of the fuzzing context.
 */
RTDECL(uint32_t) RTFuzzCtxRelease(RTFUZZCTX hFuzzCtx);

/**
 * Exports the given fuzzing context state.
 *
 * @returns IPRT statuse code
 * @param   hFuzzCtx            The fuzzing context to export.
 * @param   ppvState            Where to store the buffer of the state on success, free with RTMemFree().
 * @param   pcbState            Where to store the size of the context on success.
 */
RTDECL(int) RTFuzzCtxStateExport(RTFUZZCTX hFuzzCtx, void **ppvState, size_t *pcbState);

/**
 * Exports the given fuzzing context state to the given file.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context to export.
 * @param   pszFilename         The file to save the state to.
 */
RTDECL(int) RTFuzzCtxStateExportToFile(RTFUZZCTX hFuzzCtx, const char *pszFilename);

/**
 * Adds a new seed to the input corpus of the given fuzzing context.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   pvInput             The pointer to the input buffer.
 * @param   cbInput             Size of the input buffer.
 */
RTDECL(int) RTFuzzCtxCorpusInputAdd(RTFUZZCTX hFuzzCtx, const void *pvInput, size_t cbInput);

/**
 * Adds a new seed to the input corpus of the given fuzzing context from the given file.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   pszFilename         The filename to load the seed from.
 */
RTDECL(int) RTFuzzCtxCorpusInputAddFromFile(RTFUZZCTX hFuzzCtx, const char *pszFilename);

/**
 * Adds a new seed to the input corpus of the given fuzzing context from the given VFS file.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   hVfsFile            The VFS file handle to load the seed from.
 */
RTDECL(int) RTFuzzCtxCorpusInputAddFromVfsFile(RTFUZZCTX hFuzzCtx, RTVFSFILE hVfsFile);

/**
 * Adds new seeds to the input corpus of the given fuzzing context from the given directory.
 *
 * Will only process regular files, i.e. ignores directories, symbolic links, devices, fifos
 * and such.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   pszDirPath          The directory to load seeds from.
 */
RTDECL(int) RTFuzzCtxCorpusInputAddFromDirPath(RTFUZZCTX hFuzzCtx, const char *pszDirPath);

/**
 * Restricts the maximum input size to generate by the fuzzing context.
 *
 * @returns IPRT status code
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   cbMax               Maximum input size in bytes.
 */
RTDECL(int) RTFuzzCtxCfgSetInputSeedMaximum(RTFUZZCTX hFuzzCtx, size_t cbMax);

/**
 * Returns the maximum input size of the given fuzzing context.
 *
 * @returns Maximum input size generated in bytes.
 * @param   hFuzzCtx            The fuzzing context handle.
 */
RTDECL(size_t) RTFuzzCtxCfgGetInputSeedMaximum(RTFUZZCTX hFuzzCtx);

/**
 * Sets flags controlling the behavior of the fuzzing context.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   fFlags              Flags controlling the fuzzing context, RTFUZZCTX_F_XXX.
 */
RTDECL(int) RTFuzzCtxCfgSetBehavioralFlags(RTFUZZCTX hFuzzCtx, uint32_t fFlags);

/**
 * Returns the current set behavioral flags for the given fuzzing context.
 *
 * @returns Behavioral flags of the given fuzzing context.
 * @param   hFuzzCtx            The fuzzing context handle.
 */
RTDECL(uint32_t) RTFuzzCfgGetBehavioralFlags(RTFUZZCTX hFuzzCtx);

/**
 * Sets the temporary directory used by the fuzzing context.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   pszPathTmp          The directory for the temporary state.
 */
RTDECL(int) RTFuzzCtxCfgSetTmpDirectory(RTFUZZCTX hFuzzCtx, const char *pszPathTmp);

/**
 * Returns the current temporary directory.
 *
 * @returns Current temporary directory.
 * @param   hFuzzCtx            The fuzzing context handle.
 */
RTDECL(const char *) RTFuzzCtxCfgGetTmpDirectory(RTFUZZCTX hFuzzCtx);

/**
 * Generates a new input from the given fuzzing context and returns it.
 *
 * @returns IPRT status code.
 * @param   hFuzzCtx            The fuzzing context handle.
 * @param   phFuzzInput         Where to store the handle to the fuzzed input on success.
 */
RTDECL(int) RTFuzzCtxInputGenerate(RTFUZZCTX hFuzzCtx, PRTFUZZINPUT phFuzzInput);



/**
 * Retains a reference to the given fuzzing input handle.
 *
 * @returns New reference count on success.
 * @param   hFuzzInput          The fuzzing input handle.
 */
RTDECL(uint32_t) RTFuzzInputRetain(RTFUZZINPUT hFuzzInput);

/**
 * Releases a reference from the given fuzzing input handle, destroying it when reaaching 0.
 *
 * @returns New reference count on success, 0 if the fuzzing input got destroyed.
 * @param   hFuzzInput          The fuzzing input handle.
 */
RTDECL(uint32_t) RTFuzzInputRelease(RTFUZZINPUT hFuzzInput);

/**
 * Queries the data pointer and size of the given fuzzing input.
 *
 * @returns IPRT status code
 * @param   hFuzzInput          The fuzzing input handle.
 * @param   ppv                 Where to store the pointer to the input data on success.
 * @param   pcb                 Where to store the size of the input data on success.
 */
RTDECL(int) RTFuzzInputQueryData(RTFUZZINPUT hFuzzInput, void **ppv, size_t *pcb);

/**
 * Queries the string of the MD5 digest for the given fuzzed input.
 *
 * @returns IPRT status code.
 * @retval  VERR_BUFFER_OVERFLOW if the size of the string buffer is not sufficient.
 * @param   hFuzzInput          The fuzzing input handle.
 * @param   pszDigest           Where to store the digest string and a closing terminator.
 * @param   cchDigest           Size of the string buffer in characters (including the zero terminator).
 */
RTDECL(int) RTFuzzInputQueryDigestString(RTFUZZINPUT hFuzzInput, char *pszDigest, size_t cchDigest);

/**
 * Writes the given fuzzing input to the given file.
 *
 * @returns IPRT status code.
 * @param   hFuzzInput          The fuzzing input handle.
 * @param   pszFilename         The filename to store the input to.
 */
RTDECL(int) RTFuzzInputWriteToFile(RTFUZZINPUT hFuzzInput, const char *pszFilename);

/**
 * Adds the given fuzzed input to the input corpus of the owning context.
 *
 * @returns IPRT status code.
 * @retval  VERR_ALREADY_EXISTS if the input exists already.
 * @param   hFuzzInput          The fuzzing input handle.
 */
RTDECL(int) RTFuzzInputAddToCtxCorpus(RTFUZZINPUT hFuzzInput);

/**
 * Removes the given fuzzed input from the input corpus of the owning context.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_FOUND if the input is not part of the corpus.
 * @param   hFuzzInput          The fuzzing input handle.
 */
RTDECL(int) RTFuzzInputRemoveFromCtxCorpus(RTFUZZINPUT hFuzzInput);



/**
 * Creates a new fuzzing observer.
 *
 * @returns IPRT status code.
 * @param   phFuzzObs           Where to store the fuzzing observer handle on success.
 */
RTDECL(int) RTFuzzObsCreate(PRTFUZZOBS phFuzzObs);

/**
 * Destroys a previously created fuzzing observer.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 */
RTDECL(int) RTFuzzObsDestroy(RTFUZZOBS hFuzzObs);

/**
 * Queries the internal fuzzing context of the given observer.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 * @param   phFuzzCtx           Where to store the handle to the fuzzing context on success.
 *
 * @note The fuzzing context handle should be released with RTFuzzCtxRelease() when not used anymore.
 */
RTDECL(int) RTFuzzObsQueryCtx(RTFUZZOBS hFuzzObs, PRTFUZZCTX phFuzzCtx);

/**
 * Sets the temp directory for the given fuzzing observer.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 * @param   pszTmp              The temp directory path.
 */
RTDECL(int) RTFuzzObsSetTmpDirectory(RTFUZZOBS hFuzzObs, const char *pszTmp);


/**
 * Sets the directory to store results to.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 * @param   pszResults          The path to store the results.
 */
RTDECL(int) RTFuzzObsSetResultDirectory(RTFUZZOBS hFuzzObs, const char *pszResults);


/**
 * Sets the binary to run for each fuzzed input.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 * @param   pszBinary           The binary path.
 * @param   fFlags              Flags controlling execution of the binary, RTFUZZ_OBS_BINARY_F_XXX.
 */
RTDECL(int) RTFuzzObsSetTestBinary(RTFUZZOBS hFuzzObs, const char *pszBinary, uint32_t fFlags);

/** @name RTFUZZ_OBS_BINARY_F_XXX
 * @{ */
/** The tested binary requires a real file to read from and doesn't support stdin. */
#define RTFUZZ_OBS_BINARY_F_INPUT_FILE  RT_BIT_32(0)
/** @} */

/**
 * Sets additional arguments to run the binary with.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 * @param   papszArgs           Pointer to the array of arguments.
 * @param   cArgs               Number of arguments.
 */
RTDECL(int) RTFuzzObsSetTestBinaryArgs(RTFUZZOBS hFuzzObs, const char * const *papszArgs, unsigned cArgs);

/**
 * Starts fuzzing the set binary.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 * @param   cProcs              Number of processes to run simulteanously,
 *                              0 will create as many processes as there are CPUs available.
 */
RTDECL(int) RTFuzzObsExecStart(RTFUZZOBS hFuzzObs, uint32_t cProcs);

/**
 * Stops the fuzzing process.
 *
 * @returns IPRT status code.
 * @param   hFuzzObs            The fuzzing observer handle.
 */
RTDECL(int) RTFuzzObsExecStop(RTFUZZOBS hFuzzObs);


/**
 * A fuzzing master program.
 *
 * @returns Program exit code.
 *
 * @param   cArgs               The number of arguments.
 * @param   papszArgs           The argument vector.  (Note that this may be
 *                              reordered, so the memory must be writable.)
 */
RTR3DECL(RTEXITCODE) RTFuzzCmdMaster(unsigned cArgs, char **papszArgs);

/** @} */

RT_C_DECLS_END

#endif

