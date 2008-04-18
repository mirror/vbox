/** @file
 * Shared Folders: Main header - Common data and function prototypes definitions.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___SHFL_H
#define ___SHFL_H

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

#define LOG_GROUP LOG_GROUP_SHARED_FOLDERS
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

/** @} */

typedef struct _SHFLCLIENTDATA
{
    /** Client flags */
    uint32_t fu32Flags;

    RTUTF16  PathDelimiter;
} SHFLCLIENTDATA;


#endif /* !___SHFL_H */

