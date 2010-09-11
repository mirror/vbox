/** @file
 * IPRT - Memory Tracker.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#ifndef ___iprt_memtracker_h
#define ___iprt_memtracker_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/list.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_memtracker RTMemTracker - Memory Allocation Tracker.
 * @ingroup grp_rt
 * @{
 */

/**
 * The allocation/free method.
 */
typedef enum RTMEMTRACKERMETHOD
{
    RTMEMTRACKERMETHOD_INVALID = 0,
    RTMEMTRACKERMETHOD_ALLOC,
    RTMEMTRACKERMETHOD_ALLOCZ,
    RTMEMTRACKERMETHOD_REALLOC,
    RTMEMTRACKERMETHOD_FREE,

    RTMEMTRACKERMETHOD_NEW,
    RTMEMTRACKERMETHOD_NEW_ARRAY,
    RTMEMTRACKERMETHOD_DELETE,
    RTMEMTRACKERMETHOD_DELETE_ARRAY,
    RTMEMTRACKERMETHOD_END,
    RTMEMTRACKERMETHOD_32BIT_HACK = 0x7fffffff
} RTMEMTRACKERMETHOD;

/** Pointer to a tag structure. */
typedef struct RTMEMTRACKERTAG *PRTMEMTRACKERTAG;

/**
 * Memory Tracking Header for use with RTMemTrackerHdrAlloc,
 * RTMemTrackerHdrReallocPrep, RTMemTrackerHdrReallocDone and
 * RTMemTrackerHdrFree.
 */
typedef struct RTMEMTRACKERHDR
{
    /** Magic value / eye catcher (RTMEMTRACKERHDR_MAGIC). */
    size_t              uMagic;
    /** The allocation size.  */
    size_t              cbSize;
    /** The list entry. */
    RTLISTNODE          ListEntry;
    /** The tag string. */
    const char         *pszTag;
    /** Pointer to the per-tag structure. */
    PRTMEMTRACKERTAG    pTag;
    /** Alignmnet padding. */
    void               *pvAlignment[2];
} RTMEMTRACKERHDR;

/** Magic value for RTMEMTRACKERHDR::uMagic (Kelly Link). */
#if ARCH_BITS == 64
# define RTMEMTRACKERHDR_MAGIC   UINT64_C(0x1907691919690719)
#else
# define RTMEMTRACKERHDR_MAGIC   UINT32_C(0x19690719)
#endif

/**
 * Initializes the allocation header and links it to the relevant tag.
 *
 * @returns Pointer to the user data part.
 * @param   pv                  The header + user data block.  This must be at
 *                              least @a cb + sizeof(RTMEMTRACKERHDR).
 * @param   cb                  The user data size (bytes).
 * @param   pszTag              The tag string.
 * @param   enmMethod           The method that the user called.
 */
RTDECL(void *) RTMemTrackerHdrAlloc(void *pv, size_t cb, const char *pszTag, RTMEMTRACKERMETHOD enmMethod);

/**
 * Prepares for a realloc, i.e. invalidates the header.
 *
 * @returns Pointer to the user data part.
 * @param   pvOld               Pointer to the old user data.
 * @param   cbOld               The size of the old user data, 0 if not known.
 * @param   pszTag              The tag string.
 * @param   enmMethod           The method that the user called.
 */
RTDECL(void *) RTMemTrackerHdrReallocPrep(void *pvOld, size_t cbOld, const char *pszTag, RTMEMTRACKERMETHOD enmMethod);

/**
 * Initializes the allocation header and links it to the relevant tag.
 *
 * @returns Pointer to the user data part.
 * @param   pvNew               The new header + user data block.  This must be
 *                              at least @a cb + sizeof(RTMEMTRACKERHDR).  If
 *                              this is NULL, we assume the realloc() call
 *                              failed.
 * @param   cbNew               The user data size (bytes).
 * @param   pvOld               Pointer to the old user data.  This is only
 *                              valid on failure of course and used to bail out
 *                              in that case.  Should not be NULL.
 * @param   pszTag              The tag string.
 * @param   enmMethod           The method that the user called.
 */
RTDECL(void *) RTMemTrackerHdrReallocDone(void *pvNew, size_t cbNew, void *pvOld, const char *pszTag, RTMEMTRACKERMETHOD enmMethod);


/**
 * Do the accounting on free.
 *
 * @returns @a pv.
 * @param   pv                  Pointer to the user data.
 * @param   cb                  The size of the user data, 0 if not known.
 * @param   pszTag              The tag string.
 * @param   enmMethod           The method that the user called.
 */
RTDECL(void *) RTMemTrackerHdrFree(void *pv, size_t cb, const char *pszTag, RTMEMTRACKERMETHOD enmMethod);


/**
 * Dumps all the allocations and tag statistics to the log.
 */
RTDECL(void) RTMemTrackerDumpAllToLog(void);

/**
 * Dumps all the allocations and tag statistics to the release log.
 */
RTDECL(void) RTMemTrackerDumpAllToRelLog(void);

/**
 * Dumps all the allocations and tag statistics to standard out.
 */
RTDECL(void) RTMemTrackerDumpAllToStdOut(void);

/**
 * Dumps all the allocations and tag statistics to standard err.
 */
RTDECL(void) RTMemTrackerDumpAllToStdErr(void);

/**
 * Dumps all the allocations and tag statistics to the specified filename.
 */
RTDECL(void) RTMemTrackerDumpAllToFile(const char *pszFilename);


/**
 * Dumps all the tag statistics to the log.
 */
RTDECL(void) RTMemTrackerDumpStatsToLog(void);

/**
 * Dumps all the tag statistics to the release log.
 */
RTDECL(void) RTMemTrackerDumpStatsToRelLog(void);

/**
 * Dumps all the tag statistics to standard out.
 */
RTDECL(void) RTMemTrackerDumpStatsToStdOut(void);

/**
 * Dumps all the tag statistics to standard err.
 */
RTDECL(void) RTMemTrackerDumpStatsToStdErr(void);

/**
 * Dumps all the tag statistics to the specified filename.
 */
RTDECL(void) RTMemTrackerDumpStatsToFile(const char *pszFilename);



/** @} */

RT_C_DECLS_END

#endif

