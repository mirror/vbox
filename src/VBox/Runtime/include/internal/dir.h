/* $Id$ */
/** @file
 * innotek Portable Runtime - Internal Header for RTDir.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___internal_dir_h
#define ___internal_dir_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include "internal/magics.h"


/**
 * Filter a the filename in the against a filter.
 *
 * @returns true if the name matches the filter.
 * @returns false if the name doesn't match filter.
 * @param   pDir        The directory handle.
 * @param   pszName     The path to match to the filter.
 */
typedef DECLCALLBACK(bool) FNRTDIRFILTER(PRTDIR pDir, const char *pszName);
/** Pointer to a filter function. */
typedef FNRTDIRFILTER *PFNRTDIRFILTER;


/**
 * Open directory.
 */
typedef struct RTDIR
{
    /** Magic value, RTDIR_MAGIC. */
    uint32_t            u32Magic;
    /** The type of filter that's to be applied to the directory listing. */
    RTDIRFILTER         enmFilter;
    /** The filter function. */
    PFNRTDIRFILTER      pfnFilter;
    /** The filter Code Point string.
     * This is allocated in the same block as this structure. */
    PRTUNICP            puszFilter;
    /** The number of Code Points in the filter string. */
    size_t              cucFilter;
    /** The filter string.
     * This is allocated in the same block as this structure, thus the const. */
    const char         *pszFilter;
    /** The length of the filter string. */
    unsigned            cchFilter;
    /** Normalized path to the directory including a trailing slash.
     * We keep this around so we can query more information if required (posix).
     * This is allocated in the same block as this structure, thus the const. */
    const char         *pszPath;
    /** The length of the path. */
    unsigned            cchPath;
    /** Set to indicate that the Data member contains unread data. */
    bool                fDataUnread;
#ifndef RT_DONT_CONVERT_FILENAMES
    /** Pointer to the converted filename.
     * This can be NULL. */
    char               *pszName;
    /** The length of the converted filename. */
    unsigned            cchName;
#endif

#ifdef RT_OS_WINDOWS
    /** Handle to the opened directory search. */
    HANDLE              hDir;
    /** Find data buffer.
     * fDataUnread indicates valid data. */
# ifdef RT_DONT_CONVERT_FILENAMES
    WIN32_FIND_DATAA    Data;
# else
    WIN32_FIND_DATAW    Data;
# endif

#else /* 'POSIX': */
    /** What opendir() returned. */
    DIR                *pDir;
    /** Find data buffer.
     * fDataUnread indicates valid data. */
    struct dirent       Data;
#endif
} RTDIR;


/**
 * Validates a directory handle.
 * @returns true if valid.
 * @returns false if valid after having bitched about it first.
 */
DECLINLINE(bool) rtDirValidHandle(PRTDIR pDir)
{
    AssertMsgReturn(VALID_PTR(pDir), ("%p\n", pDir), false);
    AssertMsgReturn(pDir->u32Magic == RTDIR_MAGIC, ("%#RX32\n", pDir->u32Magic), false);
    return true;
}


/**
 * Initialize the OS specific part of the handle and open the directory.
 * Called by rtDirOpenCommon().
 *
 * @returns IPRT status code.
 * @param   pDir        The directory to open. The pszPath member contains the
 *                      path to the directory.
 * @param   pszPathBuf  Pointer to a RTPATH_MAX sized buffer containing pszPath.
 *                      Find-first style systems can use this to setup the
 *                      wildcard expression.
 */
int rtOpenDirNative(PRTDIR pDir, char *pszPathBuf);

#endif
