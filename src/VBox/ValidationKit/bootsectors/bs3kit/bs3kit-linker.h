/* $Id$ */
/** @file
 * BS3Kit - Linker related structures and defines.
 */

/*
 * Copyright (C) 2007-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef BS3KIT_INCLUDED_bs3kit_linker_h
#define BS3KIT_INCLUDED_bs3kit_linker_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#pragma pack(1)
typedef struct BS3BOOTSECTOR
{
    uint8_t     abJmp[3];
    char        abOemId[8];
    /** @name EBPB, DOS 4.0 style.
     * @{  */
    uint16_t    cBytesPerSector;        /**< 00bh */
    uint8_t     cSectorsPerCluster;     /**< 00dh */
    uint16_t    cReservedSectors;       /**< 00eh */
    uint8_t     cFATs;                  /**< 010h */
    uint16_t    cRootDirEntries;        /**< 011h */
    uint16_t    cTotalSectors;          /**< 013h */
    uint8_t     bMediaDescriptor;       /**< 015h */
    uint16_t    cSectorsPerFAT;         /**< 016h */
    uint16_t    cPhysSectorsPerTrack;   /**< 018h */
    uint16_t    cHeads;                 /**< 01ah */
    uint32_t    cHiddentSectors;        /**< 01ch */
    uint32_t    cLargeTotalSectors;     /**< 020h - We (ab)use this to indicate the number of sectors to load. */
    uint8_t     bBootDrv;               /**< 024h */
    uint8_t     bFlagsEtc;              /**< 025h */
    uint8_t     bExtendedSignature;     /**< 026h */
    uint32_t    dwSerialNumber;         /**< 027h */
    char        abLabel[11];            /**< 02bh */
    char        abFSType[8];            /**< 036h */
    /** @} */
} BS3BOOTSECTOR;
#pragma pack()
typedef BS3BOOTSECTOR *PBS3BOOTSECTOR;

AssertCompileMemberOffset(BS3BOOTSECTOR, cLargeTotalSectors, 0x20);
AssertCompileMemberOffset(BS3BOOTSECTOR, abLabel, 0x2b);
AssertCompileMemberOffset(BS3BOOTSECTOR, abFSType, 0x36);

#define BS3_OEMID                       "BS3Kit\n\n"
#define BS3_FSTYPE                      "RawCode\n"
#define BS3_LABEL                       "VirtualBox\n"
#define BS3_MAX_SIZE                    UINT32_C(491520) /* 480KB */


/** The default address to start load high DLLs at.
 * We start at 8MB to make sure the start is 16-bit tiled addressable, that we
 * can load high DLLs on 286es, and to avoid using up the upper/tiled memory. */
#define BS3HIGHDLL_LOAD_ADDRESS         ( _8M )

/**
 * High DLL table entry.
 * These are found at the end of the base image.
 */
typedef struct BS3HIGHDLLENTRY
{
    char        achMagic[8];        /**< BS3HIGHDLLENTRY_MAGIC */
    uint32_t    uLoadAddr;          /**< The load address. Set by the linker. */
    uint32_t    cbLoaded;           /**< The number of bytes when loaded. */
    uint32_t    offInImage;         /**< Byte offset in the linked image blob. */
    uint32_t    cbInImage;          /**< The number of bytes to load from the linked image blob. */
    uint32_t    cImports;           /**< Number of imports. */
    int32_t     offImports;         /**< Relative address (to entry start) of the import table. */
    uint32_t    cExports;           /**< Number of exports. */
    int32_t     offExports;         /**< Relative address (to entry start) of the export table. */
    uint32_t    cbStrings;          /**< Size of the string table in bytes. */
    int32_t     offStrings;         /**< Relative address (to entry start) of the string table. */
    uint32_t    offFilename;        /**< String table offset of the DLL name (sans path, with extension). */
} BS3HIGHDLLENTRY;
/** Magic value for BS3HIGHDLLENTRY. */
#define BS3HIGHDLLENTRY_MAGIC           "HighDLL"

typedef struct BS3HIGHDLLIMPORTENTRY
{
    uint16_t    offName;
    uint16_t    uSeg;
    uint32_t    offFlat;
} BS3HIGHDLLIMPORTENTRY;

typedef struct BS3HIGHDLLEXPORTENTRY
{
    uint32_t    offFlat;
    uint16_t    offName;
} BS3HIGHDLLEXPORTENTRY;


#endif /* !BS3KIT_INCLUDED_bs3kit_linker_h */

