/* $Id$ */
/** @file
 * IPRT - Resource Interchange File Format (RIFF), WAVE, ++.
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

#ifndef IPRT_INCLUDED_formats_riff_h
#define IPRT_INCLUDED_formats_riff_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/assertcompile.h>


/** @defgroup grp_rt_formats_riff    RIFF & WAVE structures and definitions
 * @ingroup grp_rt_formats
 * @{
 */

/**
 * Resource interchange file format (RIFF) file header.
 */
typedef struct RTRIFFHDR
{
    /** The 'RIFF' magic (RTRIFFHDR_MAGIC). */
    uint32_t        uMagic;
    /** The file size. */
    uint32_t        cbFile;
    /** The file type. */
    uint32_t        uFileType;
} RTRIFFHDR;
AssertCompileSize(RTRIFFHDR, 12);
/** Pointer to a RIFF file header. */
typedef RTRIFFHDR *PRTRIFFHDR;

/** Magic value for RTRIFFHDR::uMagic ('RIFF'). */
#define RTRIFFHDR_MAGIC         RT_BE2H_U32_C(0x52494646)

/** @name RIFF file types (RTRIFFHDR::uFileType)
 * @{ */
/** RIFF file type: WAVE (audio) */
#define RTRIFF_FILE_TYPE_WAVE   RT_BE2H_U32_C(0x57415645)
/** RIFF file type: AVI (video) */
#define RTRIFF_FILE_TYPE_AVI    RT_BE2H_U32_C(0x41564920)
/** @} */

/**
 * A RIFF chunk.
 */
typedef struct RTRIFFCHUNK
{
    /** The chunk magic (four character code). */
    uint32_t        uMagic;
    /** The size of the chunk minus this header. */
    uint32_t        cbChunk;
} RTRIFFCHUNK;
AssertCompileSize(RTRIFFCHUNK, 8);
/** Pointer to a RIFF chunk. */
typedef RTRIFFCHUNK *PRTRIFFCHUNK;

/**
 * A RIFF list.
 */
typedef struct RTRIFFLIST
{
    /** The list indicator (RTRIFFLIST_MAGIC). */
    uint32_t        uMagic;
    /** The size of the chunk minus this header. */
    uint32_t        cbChunk;
    /** The list type (four character code). */
    uint32_t        uListType;
} RTRIFFLIST;
AssertCompileSize(RTRIFFLIST, 12);
/** Pointer to a RIFF list. */
typedef RTRIFFLIST *PRTRIFFLIST;
/** Magic value for RTRIFFLIST::uMagic ('LIST'). */
#define RTRIFFLIST_MAGIC    RT_BE2H_U32_C(0x4c495354)

/** Generic 'INFO' list type. */
#define RTRIFFLIST_TYPE_INFO    RT_BE2H_U32_C(0x494e464f)


/**
 * Wave file format (WAVEFORMATEX w/o cbSize).
 * @see RTRIFFWAVEFMTCHUNK.
 */
typedef struct RTRIFFWAVEFMT
{
    /** Audio format tag. */
    uint16_t        uFormatTag;
    /** Number of channels. */
    uint16_t        cChannels;
    /** Sample rate. */
    uint32_t        uHz;
    /** Byte rate (= uHz * cChannels * cBitsPerSample / 8) */
    uint32_t        cbRate;
    /** Frame size (aka block alignment).    */
    uint16_t        cbFrame;
    /** Number of bits per sample. */
    uint16_t        cBitsPerSample;
} RTRIFFWAVEFMT;
AssertCompileSize(RTRIFFWAVEFMT, 16);
/** Pointer to a wave file format structure. */
typedef RTRIFFWAVEFMT *PRTRIFFWAVEFMT;

/** RTRIFFWAVEFMT::uFormatTag value for PCM. */
#define RTRIFFWAVEFMT_TAG_PCM       1

/**
 * Wave file format chunk.
 */
typedef struct RTRIFFWAVEFMTCHUNK
{
    /** Chunk header with RTRIFFWAVEFMT_MAGIC as magic. */
    RTRIFFCHUNK     Chunk;
    /** The wave file format. */
    RTRIFFWAVEFMT   Data;
} RTRIFFWAVEFMTCHUNK;
AssertCompileSize(RTRIFFWAVEFMTCHUNK, 8+16);
/** Pointer to a wave file format chunk.   */
typedef RTRIFFWAVEFMTCHUNK *PRTRIFFWAVEFMTCHUNK;
/** Magic value for RTRIFFWAVEFMTCHUNK::uMagic ('fmt '). */
#define RTRIFFWAVEFMT_MAGIC RT_BE2H_U32_C(0x666d7420)

/**
 * Wave file data chunk.
 */
typedef struct RTRIFFWAVEDATACHUNK
{
    /** Chunk header with RTRIFFWAVEFMT_MAGIC as magic. */
    RTRIFFCHUNK     Chunk;
    /** Variable sized sample data. */
    uint8_t         abData[RT_FLEXIBLE_ARRAY_IN_NESTED_UNION];
} RTRIFFWAVEDATACHUNK;

/** Magic value for RTRIFFWAVEFMT::uMagic ('data'). */
#define RTRIFFWAVEDATACHUNK_MAGIC RT_BE2H_U32_C(0x64617461)

/** @} */

#endif /* !IPRT_INCLUDED_formats_riff_h */

