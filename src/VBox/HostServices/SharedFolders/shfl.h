/** @file
 * Shared Folders: Main header - Common data and function prototypes definitions.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___SHFL_H
#define ___SHFL_H
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>
#include <VBox/shflsvc.h>

#include <VBox/log.h>

/**
 * Shared Folders client flags.
 * @{
 */

/** Client has queried mappings at least once and, therefore,
 *  the service can process its other requests too.
 */
#define SHFL_CF_MAPPINGS_QUERIED (0x00000001)

/** Mappings have been changed since last query. */
#define SHFL_CF_MAPPINGS_CHANGED (0x00000002)

/** Client uses UTF8 encoding, if not set then unicode 16 bit (UCS2) is used. */
#define SHFL_CF_UTF8             (0x00000004)

/** Client both supports and wants to use symlinks. */
#define SHFL_CF_SYMLINKS         (0x00000008)

/** The call to SHFL_FN_WAIT_FOR_MAPPINGS_CHANGES will return immediately
 *  because of a SHFL_FN_CANCEL_MAPPINGS_CHANGES_WAITS call. */
#define SHFL_CF_CANCEL_NEXT_WAIT (0x00000010)

/** @} */

typedef struct _SHFLCLIENTDATA
{
    /** Client flags */
    uint32_t fu32Flags;
    /** Path delimiter. */
    RTUTF16  PathDelimiter;
    /** Currently unused.   */
    uint8_t  bPadding;
    /** Set if the client has mapping usage counts.
     * This is for helping with saved state. */
    uint8_t  fHasMappingCounts;
    /** Mapping counts for each root ID so we can unmap the folders when the
     *  session disconnects or the VM resets. */
    uint16_t acMappings[SHFL_MAX_MAPPINGS];
} SHFLCLIENTDATA;
/** Pointer to a SHFLCLIENTDATA structure. */
typedef SHFLCLIENTDATA *PSHFLCLIENTDATA;

#endif /* !___SHFL_H */

