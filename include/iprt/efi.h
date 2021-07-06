/** @file
 * IPRT - EFI related utilities.
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

#ifndef IPRT_INCLUDED_efi_h
#define IPRT_INCLUDED_efi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/time.h>
#include <iprt/vfs.h>

#include <iprt/formats/efi-common.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_efi    RTEfi - EFI utilities
 * @ingroup grp_rt
 * @{
 */


#ifdef IN_RING3

/**
 * Converts an EFI time to a time spec (UTC).
 *
 * @returns pTimeSpec on success.
 * @returns NULL if the pEfiTime data is invalid.
 * @param   pTimeSpec   Where to store the converted time.
 * @param   pEfiTime    Pointer to the EFI time struct.
 */
RTDECL(PRTTIMESPEC) RTEfiTimeToTimeSpec(PRTTIMESPEC pTimeSpec, PCEFI_TIME pEfiTime);


/**
 * Converts a time spec (UTC) to an EFI time.
 *
 * @returns pEfiTime on success.
 * @returns NULL if the pTimeSpec data is invalid.
 * @param   pEfiTime    Pointer to the EFI time struct.
 * @param   pTimeSpec   The time spec to convert.
 */
RTDECL(PEFI_TIME) RTEfiTimeFromTimeSpec(PEFI_TIME pEfiTime, PCRTTIMESPEC pTimeSpec);


/**
 * Converts the given EFI GUID to the IPRT UUID representation.
 *
 * @returns pUuid.
 * @param   pUuid       Where to store the converted GUID.
 * @param   pEfiGuid    The EFI GUID to convert.
 */
RTDECL(PRTUUID) RTEfiGuidToUuid(PRTUUID pUuid, PCEFI_GUID pEfiGuid);


/**
 * Converts the given EFI GUID to the IPRT UUID representation.
 *
 * @returns pEfiGuid.
 * @param   pEfiGuid    Where to store the converted UUID.
 * @param   pUuid       The UUID to convert.
 */
RTDECL(PEFI_GUID) RTEfiGuidFromUuid(PEFI_GUID pEfiGuid, PCRTUUID pUuid);


/**
 * Opens an EFI variable store.
 *
 * @returns IPRT status code.
 * @param   hVfsFileIn      The file or device backing the store.
 * @param   fMntFlags       RTVFSMNT_F_XXX.
 * @param   fVarStoreFlags  Reserved, MBZ.
 * @param   phVfs           Where to return the virtual file system handle.
 * @param   pErrInfo        Where to return additional error information.
 */
RTDECL(int) RTEfiVarStoreOpenAsVfs(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fVarStoreFlags, PRTVFS phVfs, PRTERRINFO pErrInfo);


/** @name RTEFIVARSTORE_CREATE_F_XXX - RTEfiVarStoreCreate flags
 * @{ */
/** Use default options. */
#define RTEFIVARSTORE_CREATE_F_DEFAULT                  UINT32_C(0)
/** Don't create a fault tolerant write working space.
 * The default is to create one reducing the size of the variable store. */
#define RTEFIVARSTORE_CREATE_F_NO_FTW_WORKING_SPACE     RT_BIT_32(0)
/** Mask containing all valid flags.   */
#define RTEFIVARSTORE_CREATE_F_VALID_MASK               UINT32_C(0x00000001)
/** @} */

/**
 * Creates a new EFI variable store.
 *
 * @returns IRPT status code.
 * @param   hVfsFile            The store file.
 * @param   offStore            The offset into @a hVfsFile of the file.
 *                              Typically 0.
 * @param   cbStore             The size of the variable store.  Pass 0 if the rest of
 *                              hVfsFile should be used. The remaining space for variables
 *                              will be less because of some metadata overhead.
 * @param   fFlags              See RTEFIVARSTORE_F_XXX.
 * @param   cbBlock             The logical block size.
 * @param   pErrInfo            Additional error information, maybe.  Optional.
 */
RTDECL(int) RTEfiVarStoreCreate(RTVFSFILE hVfsFile, uint64_t offStore, uint64_t cbStore, uint32_t fFlags, uint32_t cbBlock,
                                PRTERRINFO pErrInfo);

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_efi_h */

