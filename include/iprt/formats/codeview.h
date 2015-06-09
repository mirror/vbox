/** @file
 * IPRT - Microsoft CodeView Debug Information.
 */

/*
 * Copyright (C) 2009-2015 Oracle Corporation
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

#ifndef ___iprt_formats_codeview_h
#define ___iprt_formats_codeview_h


#include <iprt/types.h>
#include <iprt/assert.h>


/** @defgroup grp_rt_fmt_codeview  Microsoft CodeView Debug Information
 * @{
 */
/**
 * PDB v2.0 in image debug info.
 * The URL is constructed from the timestamp and age?
 */
typedef struct CVPDB20INFO
{
    uint32_t    u32Magic;               /**< CVPDB20INFO_SIGNATURE. */
    int32_t     offDbgInfo;             /**< Always 0. Used to be the offset to the real debug info. */
    uint32_t    uTimestamp;
    uint32_t    uAge;
    uint8_t     szPdbFilename[4];
} CVPDB20INFO;
/** Pointer to in executable image PDB v2.0 info. */
typedef CVPDB20INFO       *PCVPDB20INFO;
/** Pointer to read only in executable image PDB v2.0 info. */
typedef CVPDB20INFO const *PCCVPDB20INFO;
/** The CVPDB20INFO magic value. */
#define CVPDB20INFO_MAGIC           RT_MAKE_U32_FROM_U8('N','B','1','0')

/**
 * PDB v7.0 in image debug info.
 * The URL is constructed from the signature and the age.
 */
#pragma pack(4)
typedef struct CVPDB70INFO
{
    uint32_t    u32Magic;            /**< CVPDB70INFO_SIGNATURE. */
    RTUUID      PdbUuid;
    uint32_t    uAge;
    uint8_t     szPdbFilename[4];
} CVPDB70INFO;
#pragma pack()
AssertCompileMemberOffset(CVPDB70INFO, PdbUuid, 4);
AssertCompileMemberOffset(CVPDB70INFO, uAge, 4 + 16);
/** Pointer to in executable image PDB v7.0 info. */
typedef CVPDB70INFO       *PCVPDB70INFO;
/** Pointer to read only in executable image PDB v7.0 info. */
typedef CVPDB70INFO const *PCCVPDB70INFO;
/** The CVPDB70INFO magic value. */
#define CVPDB70INFO_MAGIC           RT_MAKE_U32_FROM_U8('R','S','D','S')


/** @}  */

#endif

