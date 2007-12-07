/** @file
 * innotek Portable Runtime - Compression.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___iprt_zip_h
#define ___iprt_zip_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


__BEGIN_DECLS

/** @defgroup grp_rt_zip        RTZip - Compression
 * @ingroup grp_rt
 * @{
 */



/**
 * Callback function for consuming compressed data during compression.
 *
 * @returns iprt status code.
 * @param   pvUser      User argument.
 * @param   pvBuf       Compressed data.
 * @param   cbBuf       Size of the compressed data.
 */
typedef DECLCALLBACK(int) FNRTZIPOUT(void *pvUser, const void *pvBuf, size_t cbBuf);
/** Pointer to FNRTZIPOUT() function. */
typedef FNRTZIPOUT *PFNRTZIPOUT;

/**
 * Callback function for supplying compressed data during decompression.
 *
 * @returns iprt status code.
 * @param   pvUser      User argument.
 * @param   pvBuf       Where to store the compressed data.
 * @param   cbBuf       Size of the buffer.
 * @param   pcbBuf      Number of bytes actually stored in the buffer.
 */
typedef DECLCALLBACK(int) FNRTZIPIN(void *pvUser, void *pvBuf, size_t cbBuf, size_t *pcbBuf);
/** Pointer to FNRTZIPIN() function. */
typedef FNRTZIPIN *PFNRTZIPIN;

/**
 * Compression type.
 * (Be careful with these they are stored in files!)
 */
typedef enum RTZIPTYPE
{
    /** Invalid. */
    RTZIPTYPE_INVALID = 0,
    /** Choose best fitting one. */
    RTZIPTYPE_AUTO,
    /** Store the data. */
    RTZIPTYPE_STORE,
    /** Zlib compression the data. */
    RTZIPTYPE_ZLIB,
    /** BZlib compress. */
    RTZIPTYPE_BZLIB,
    /** libLZF compress. */
    RTZIPTYPE_LZF
} RTZIPTYPE;

/**
 * Compression level.
 */
typedef enum RTZIPLEVEL
{
    /** Store, don't compress. */
    RTZIPLEVEL_STORE = 0,
    /** Fast compression. */
    RTZIPLEVEL_FAST,
    /** Default compression. */
    RTZIPLEVEL_DEFAULT,
    /** Maximal compression. */
    RTZIPLEVEL_MAX
} RTZIPLEVEL;


/**
 * Create a compressor instance.
 *
 * @returns iprt status code.
 * @param   ppZip       Where to store the instance handle.
 * @param   pvUser      User argument which will be passed on to pfnOut and pfnIn.
 * @param   pfnOut      Callback for consuming output of compression.
 * @param   enmType     Type of compressor to create.
 * @param   enmLevel    Compression level.
 */
RTDECL(int)     RTZipCompCreate(PRTZIPCOMP *ppZip, void *pvUser, PFNRTZIPOUT pfnOut, RTZIPTYPE enmType, RTZIPLEVEL enmLevel);

/**
 * Compresses a chunk of memory.
 *
 * @returns iprt status code.
 * @param   pZip        The compressor instance.
 * @param   pvBuf       Pointer to buffer containing the bits to compress.
 * @param   cbBuf       Number of bytes to compress.
 */
RTDECL(int)     RTZipCompress(PRTZIPCOMP pZip, const void *pvBuf, size_t cbBuf);

/**
 * Finishes the compression.
 * This will flush all data and terminate the compression data stream.
 *
 * @returns iprt status code.
 * @param   pZip        The compressor instance.
 */
RTDECL(int)     RTZipCompFinish(PRTZIPCOMP pZip);

/**
 * Destroys the compressor instance.
 *
 * @returns iprt status code.
 * @param   pZip        The compressor instance.
 */
RTDECL(int)     RTZipCompDestroy(PRTZIPCOMP pZip);


/**
 * Create a decompressor instance.
 *
 * @returns iprt status code.
 * @param   ppZip       Where to store the instance handle.
 * @param   pvUser      User argument which will be passed on to pfnOut and pfnIn.
 * @param   pfnIn       Callback for producing input for decompression.
 */
RTDECL(int)     RTZipDecompCreate(PRTZIPDECOMP *ppZip, void *pvUser, PFNRTZIPIN pfnIn);

/**
 * Decompresses a chunk of memory.
 *
 * @returns iprt status code.
 * @param   pZip        The decompressor instance.
 * @param   pvBuf       Where to store the decompressed data.
 * @param   cbBuf       Number of bytes to produce. If pcbWritten is set
 *                      any number of bytes up to cbBuf might be returned.
 * @param   pcbWritten  Number of bytes actually written to the buffer. If NULL
 *                      cbBuf number of bytes must be written.
 */
RTDECL(int)     RTZipDecompress(PRTZIPDECOMP pZip, void *pvBuf, size_t cbBuf, size_t *pcbWritten);

/**
 * Destroys the decompressor instance.
 *
 * @returns iprt status code.
 * @param   pZip        The decompressor instance.
 */
RTDECL(int)     RTZipDecompDestroy(PRTZIPDECOMP pZip);


/** @} */

__END_DECLS

#endif

