/* $Id$ */
/** @file
 * IPRT, EFI firmware volume (FV) definitions.
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

#ifndef IPRT_INCLUDED_formats_efi_fv_h
#define IPRT_INCLUDED_formats_efi_fv_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/assertcompile.h>
#include <iprt/formats/efi-common.h>


/*
 * Definitions come from the UEFI PI Spec 1.5 Volume 3 Firmware, chapter 3 "Firmware Storage Code Definitions"
 */

/**
 * The volume header.
 */
typedef struct EFI_FIRMWARE_VOLUME_HEADER
{
    /** Reserved data for the reset vector. */
    uint8_t         abZeroVec[16];
    /** The filesystem GUID. */
    EFI_GUID        GuidFilesystem;
    /** The firmware volume length in bytes including this header. */
    uint64_t        cbFv;
    /** The signature of the firmware volume header (set to _FVH). */
    uint32_t        u32Signature;
    /** Firmware volume attributes. */
    uint32_t        fAttr;
    /** Size of the header in bytes. */
    uint16_t        cbFvHdr;
    /** Checksum of the header. */
    uint16_t        u16Chksum;
    /** Offset of the extended header (0 for no extended header). */
    uint16_t        offExtHdr;
    /** Reserved MBZ. */
    uint8_t         bRsvd;
    /** Revision of the header. */
    uint8_t         bRevision;
} EFI_FIRMWARE_VOLUME_HEADER;
AssertCompileSize(EFI_FIRMWARE_VOLUME_HEADER, 56);
/** Pointer to a EFI firmware volume header. */
typedef EFI_FIRMWARE_VOLUME_HEADER *PEFI_FIRMWARE_VOLUME_HEADER;
/** Pointer to a const EFI firmware volume header. */
typedef const EFI_FIRMWARE_VOLUME_HEADER *PCEFI_FIRMWARE_VOLUME_HEADER;

/** The signature for a firmware volume header. */
#define EFI_FIRMWARE_VOLUME_HEADER_SIGNATURE RT_MAKE_U32_FROM_U8('_', 'F', 'V', 'H')
/** Revision of the firmware volume header. */
#define EFI_FIRMWARE_VOLUME_HEADER_REVISION  2


/**
 * Firmware block map entry.
 */
typedef struct EFI_FW_BLOCK_MAP
{
    /** Number of blocks for this entry. */
    uint32_t        cBlocks;
    /** Block size in bytes. */
    uint32_t        cbBlock;
} EFI_FW_BLOCK_MAP;
AssertCompileSize(EFI_FW_BLOCK_MAP, 8);
/** Pointer to a firmware volume block map entry. */
typedef EFI_FW_BLOCK_MAP *PEFI_FW_BLOCK_MAP;
/** Pointer to a const firmware volume block map entry. */
typedef const EFI_FW_BLOCK_MAP *PCEFI_FW_BLOCK_MAP;

#endif /* !IPRT_INCLUDED_formats_efi_fv_h */

