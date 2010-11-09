/* $Id$ */
/** @file
 * IPRT - Virtual File System, Base.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>

#include "internal/file.h"
#include "internal/magics.h"
//#include "internal/vfs.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define RTVFS_MAGIC                 UINT32_C(0x11112222)
#define RTVFS_MAGIC_DEAD            (~RTVFS_MAGIC)
#define RTVFSDIR_MAGIC              UINT32_C(0x77778888)
#define RTVFSDIR_MAGIC_DEAD         (~RTVFSDIR_MAGIC)
#define RTVFSFILE_MAGIC             UINT32_C(0x55556666)
#define RTVFSFILE_MAGIC_DEAD        (~RTVFSFILE_MAGIC)
#define RTVFSIOSTREAM_MAGIC         UINT32_C(0x33334444)
#define RTVFSIOSTREAM_MAGIC_DEAD    (~RTVFSIOSTREAM_MAGIC)
#define RTVFSSYMLINK_MAGIC          UINT32_C(0x9999aaaa)
#define RTVFSSYMLINK_MAGIC_DEAD     (~RTVFSSYMLINK_MAGIC)

/** The instance data alignment. */
#define RTVFS_INST_ALIGNMENT        16U

/** The max number of symbolic links to resolve in a path. */
#define RTVFS_MAX_LINKS             20U


/** Takes a write lock. */
#define RTVFS_WRITE_LOCK(hSemRW)  \
    do { \
        if ((hSemRW) != NIL_RTSEMRW) \
        { \
            int rcSemEnter = RTSemRWRequestWrite(hSemRW, RT_INDEFINITE_WAIT); \
            AssertRC(rcSemEnter); \
        } \
    } while (0)

/** Releases a write lock. */
#define RTVFS_WRITE_UNLOCK(hSemRW)  \
    do { \
        if ((hSemRW) != NIL_RTSEMRW) \
        { \
            int rcSemLeave = RTSemRWReleaseWrite(hSemRW); \
            AssertRC(rcSemLeave); \
        } \
    } while (0)

/** Takes a read lock. */
#define RTVFS_READ_LOCK(hSemRW)  \
    do { \
        if ((hSemRW) != NIL_RTSEMRW) \
        { \
            int rcSemEnter = RTSemRWRequestRead(hSemRW, RT_INDEFINITE_WAIT); \
            AssertRC(rcSemEnter); \
        } \
    } while (0)

/** Releases a read lock. */
#define RTVFS_READ_UNLOCK(hSemRW)  \
    do { \
        if ((hSemRW) != NIL_RTSEMRW) \
        { \
            int rcSemLeave = RTSemRWReleaseRead(hSemRW); \
            AssertRC(rcSemLeave); \
        } \
    } while (0)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** @todo Move all this stuff to internal/vfs.h */

/**
 * The VFS handle data.
 */
typedef struct RTVFSINTERNAL
{
    /** The VFS magic (RTVFS_MAGIC). */
    uint32_t                uMagic;
    /** Creation flags (RTVFS_C_XXX). */
    uint32_t                fFlags;
    /** Pointer to the instance data. */
    void                   *pvThis;
    /** The vtable. */
    PCRTVFSOPS              pOps;
    /** Read-write semaphore protecting all access to the VFS
     * Only valid RTVFS_C_THREAD_SAFE is set, otherwise it is NIL_RTSEMRW. */
    RTSEMRW                 hSemRW;
    /** The number of references to this VFS.
     * This count includes objects within the file system, so that the VFS
     * won't be destroyed before all objects are closed. */
    uint32_t volatile       cRefs;
} RTVFSINTERNAL;


/**
 * The VFS directory handle data.
 */
typedef struct RTVFSDIRINTERNAL
{
    /** The VFS magic (RTVFSDIR_MAGIC). */
    uint32_t                uMagic;
    /** Reserved for flags or something. */
    uint32_t                fReserved;
    /** Pointer to the instance data. */
    void                   *pvThis;
    /** The vtable. */
    PCRTVFSDIROPS           pOps;
    /** The VFS RW sem if serialized. */
    RTSEMRW                 hSemRW;
    /** Reference back to the VFS containing this directory. */
    RTVFS                   hVfs;
    /** The number of references to this directory handle.  This does not
     * include files or anything. */
    uint32_t volatile       cRefs;
} RTVFSDIRINTERNAL;


/**
 * The VFS symbolic link handle data.
 */
typedef struct RTVFSSYMLINKINTERNAL
{
    /** The VFS magic (RTVFSSYMLINK_MAGIC). */
    uint32_t                uMagic;
    /** Reserved for flags or something. */
    uint32_t                fReserved;
    /** Pointer to the instance data. */
    void                   *pvThis;
    /** The vtable. */
    PCRTVFSSYMLINKOPS       pOps;
    /** The VFS RW sem if serialized. */
    RTSEMRW                 hSemRW;
    /** Reference back to the VFS containing this symbolic link. */
    RTVFS                   hVfs;
    /** The number of references to this symbolic link handle. */
    uint32_t volatile       cRefs;
} RTVFSSYMLINKINTERNAL;


/**
 * The VFS I/O stream handle data.
 *
 * This is normally part of a type specific handle, like a file or pipe.
 */
typedef struct RTVFSIOSTREAMINTERNAL
{
    /** The VFS magic (RTVFSIOSTREAM_MAGIC). */
    uint32_t                uMagic;
    /** File open flags, at a minimum the access mask. */
    uint32_t                fFlags;
    /** Pointer to the instance data. */
    void                   *pvThis;
    /** The vtable. */
    PCRTVFSIOSTREAMOPS      pOps;
    /** The VFS RW sem if serialized. */
    RTSEMRW                 hSemRW;
    /** Reference back to the VFS containing this directory. */
    RTVFS                   hVfs;
    /** The number of references to this file VFS. */
    uint32_t volatile       cRefs;
} RTVFSIOSTREAMINTERNAL;


/**
 * The VFS file handle data.
 *
 * @extends RTVFSIOSTREAMINTERNAL
 */
typedef struct RTVFSFILEINTERNAL
{
    /** The VFS magic (RTVFSFILE_MAGIC). */
    uint32_t                uMagic;
    /** Reserved for flags or something. */
    uint32_t                fReserved;
    /** The vtable. */
    PCRTVFSFILEOPS          pOps;
    /** The stream handle data. */
    RTVFSIOSTREAMINTERNAL   Stream;
} RTVFSFILEINTERNAL;



/**
 * Internal object retainer that asserts sanity in strict builds.
 *
 * @returns The new reference count.
 * @param   pcRefs              The reference counter.
 */
DECLINLINE(uint32_t) rtVfsRetain(uint32_t volatile *pcRefs)
{
    uint32_t cRefs = ASMAtomicIncU32(pcRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x\n", cRefs));
    return cRefs;
}


/**
 * Internal object retainer that asserts sanity in strict builds.
 *
 * @param   pcRefs              The reference counter.
 */
DECLINLINE(void) rtVfsRetainVoid(uint32_t volatile *pcRefs)
{
    (void)rtVfsRetain(pcRefs);
}


/**
 * Internal object releaser that asserts sanity in strict builds.
 *
 * @returns The new reference count.
 * @param   pcRefs              The reference counter.
 */
DECLINLINE(uint32_t) rtVfsRelease(uint32_t volatile *pcRefs)
{
    uint32_t cRefs = ASMAtomicDecU32(pcRefs);
    AssertMsg(cRefs < _1M, ("%#x\n", cRefs));
    return cRefs;
}


/*
 *
 *  U T I L   U T I L   U T I L
 *  U T I L   U T I L   U T I L
 *  U T I L   U T I L   U T I L
 *
 */



/**
 * Removes dots from the path.
 *
 * @returns The new @a pszDst value.
 * @param   pPath               The path parsing buffer.
 * @param   pszDst              The current szPath position.  This will be
 *                              updated and returned.
 * @param   fTheEnd             Indicates whether we're at the end of the path
 *                              or not.
 * @param   piRestartComp       The component to restart parsing at.
 */
static char *rtVfsParsePathHandleDots(PRTVFSPARSEDPATH pPath, char *pszDst, bool fTheEnd, uint16_t *piRestartComp)
{
    if (pszDst[-1] != '.')
        return pszDst;

    if (pszDst[-2] == '/')
    {
        pPath->cComponents--;
        pszDst = &pPath->szPath[pPath->aoffComponents[pPath->cComponents]];
    }
    else if (pszDst[-2] == '.' && pszDst[-3] == '/')
    {
        pPath->cComponents -= pPath->cComponents > 1 ? 2 : 1;
        pszDst = &pPath->szPath[pPath->aoffComponents[pPath->cComponents]];
        if (piRestartComp && *piRestartComp + 1 >= pPath->cComponents)
            *piRestartComp = pPath->cComponents > 0 ? pPath->cComponents - 1 : 0;
    }
    else
        return pszDst;

    /*
     * Drop the trailing slash if we're at the end of the source path.
     */
    if (fTheEnd && pPath->cComponents == 0)
        pszDst--;
    return pszDst;
}


RTDECL(int) RTVfsParsePathAppend(PRTVFSPARSEDPATH pPath, const char *pszPath, uint16_t *piRestartComp)
{
    AssertReturn(*pszPath != '/', VERR_INTERNAL_ERROR_4);

    /* In case *piRestartComp was set higher than the number of components
       before making the call to this function. */
    if (piRestartComp && *piRestartComp + 1 >= pPath->cComponents)
        *piRestartComp = pPath->cComponents > 0 ? pPath->cComponents - 1 : 0;

    /*
     * Append a slash to the destination path if necessary.
     */
    char *pszDst = &pPath->szPath[pPath->cch];
    if (pPath->cComponents > 0)
    {
        *pszDst++ = '/';
        if (pszDst - &pPath->szPath[0] >= RTVFSPARSEDPATH_MAX)
            return VERR_FILENAME_TOO_LONG;
    }
    Assert(pszDst[-1] == '/');

    /*
     * Parse and append the relative path.
     */
    const char *pszSrc = pszPath;
    pPath->fDirSlash   = false;
    while (pszSrc[0])
    {
        /* Skip unncessary slashes. */
        while (pszSrc[0] == '/')
            pszSrc++;

        /* Copy until we encounter the next slash. */
        pPath->aoffComponents[pPath->cComponents++] = pszDst - &pPath->szPath[0];
        while (pszSrc[0])
        {
            if (pszSrc[0] == '/')
            {
                pszSrc++;
                if (pszSrc[0])
                    *pszDst++ = '/';
                else
                    pPath->fDirSlash = true;
                pszDst = rtVfsParsePathHandleDots(pPath, pszDst, pszSrc[0] == '\0', piRestartComp);
                break;
            }

            *pszDst++ = *pszSrc++;
            if (pszDst - &pPath->szPath[0] >= RTVFSPARSEDPATH_MAX)
                return VERR_FILENAME_TOO_LONG;
        }
    }
    pszDst = rtVfsParsePathHandleDots(pPath, pszDst, true /*fTheEnd*/, piRestartComp);

    /* Terminate the string and enter its length. */
    pszDst[0] = '\0';
    pszDst[1] = '\0';                   /* for aoffComponents */
    pPath->cch = (uint16_t)(pszDst - &pPath->szPath[0]);
    pPath->aoffComponents[pPath->cComponents] = pPath->cch + 1;

    return VINF_SUCCESS;
}


RTDECL(int) RTVfsParsePath(PRTVFSPARSEDPATH pPath, const char *pszPath, const char *pszCwd)
{
    if (*pszPath != '/')
    {
        /*
         * Relative, recurse and parse pszCwd first.
         */
        int rc = RTVfsParsePath(pPath, pszCwd, NULL /*crash if pszCwd is not absolute*/);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        /*
         * Make pszPath relative, i.e. set up pPath for the root and skip
         * leading slashes in pszPath before appending it.
         */
        pPath->cch               = 1;
        pPath->cComponents       = 0;
        pPath->fDirSlash         = false;
        pPath->aoffComponents[0] = 1;
        pPath->aoffComponents[1] = 2;
        pPath->szPath[0]         = '/';
        pPath->szPath[1]         = '\0';
        pPath->szPath[2]         = '\0';
        while (pszPath[0] == '/')
            pszPath++;
        if (!pszPath[0])
            return VINF_SUCCESS;
    }
    return RTVfsParsePathAppend(pPath, pszPath, NULL);
}



RTDECL(int) RTVfsParsePathA(const char *pszPath, const char *pszCwd, PRTVFSPARSEDPATH *ppPath)
{
    /*
     * Allocate the output buffer and hand the problem to rtVfsParsePath.
     */
    int rc;
    PRTVFSPARSEDPATH pPath = (PRTVFSPARSEDPATH)RTMemTmpAlloc(sizeof(RTVFSPARSEDPATH));
    if (pPath)
    {
        rc = RTVfsParsePath(pPath, pszPath, pszCwd);
        if (RT_FAILURE(rc))
        {
            RTMemTmpFree(pPath);
            pPath = NULL;
        }
    }
    else
        rc = VERR_NO_TMP_MEMORY;
    *ppPath = pPath;                    /* always set it */
    return rc;
}


RTDECL(void) RTVfsParsePathFree(PRTVFSPARSEDPATH pPath)
{
    if (pPath)
    {
        pPath->cch               = UINT16_MAX;
        pPath->cComponents       = UINT16_MAX;
        pPath->aoffComponents[0] = UINT16_MAX;
        pPath->aoffComponents[1] = UINT16_MAX;
        RTMemTmpFree(pPath);
    }
}


/**
 * Handles a symbolic link, adding it to
 *
 * @returns IPRT status code.
 * @param   pPath               The parsed path to update.
 * @param   piComponent         The component iterator to update.
 * @param   hSymlink            The symbolic link to process.
 */
static int rtVfsTraverseHandleSymlink(PRTVFSPARSEDPATH pPath, uint16_t *piComponent, RTVFSSYMLINK hSymlink)
{
    /*
     * Read the link.
     */
    char szPath[RTPATH_MAX];
    int rc = RTVfsSymlinkRead(hSymlink, szPath, sizeof(szPath) - 1);
    if (RT_SUCCESS(rc))
    {
        szPath[sizeof(szPath) - 1] = '\0';
        if (szPath[0] == '/')
        {
            /*
             * Absolute symlink.
             */
            rc = RTVfsParsePath(pPath, szPath, NULL);
            if (RT_SUCCESS(rc))
            {
                *piComponent = 0;
                return VINF_SUCCESS;
            }
        }
        else
        {
            /*
             * Relative symlink, must replace the current component with the
             * link value.  We do that by using the remainder of the symlink
             * buffer as temporary storage.
             */
            uint16_t iComponent = *piComponent;
            if (iComponent + 1 < pPath->cComponents)
                rc = RTPathAppend(szPath, sizeof(szPath), &pPath->szPath[pPath->aoffComponents[iComponent + 1]]);
            if (RT_SUCCESS(rc))
            {
                pPath->cch = pPath->aoffComponents[iComponent] - (iComponent > 0);
                pPath->aoffComponents[iComponent + 1] = pPath->cch + 1;
                pPath->szPath[pPath->cch]     = '\0';
                pPath->szPath[pPath->cch + 1] = '\0';

                rc = RTVfsParsePathAppend(pPath, szPath, &iComponent);
                if (RT_SUCCESS(rc))
                {
                    *piComponent = iComponent;
                    return VINF_SUCCESS;
                }
            }
        }
    }
    return rc == VERR_BUFFER_OVERFLOW ? VERR_FILENAME_TOO_LONG : rc;
}


/**
 * Internal worker for various open functions as well as RTVfsTraverseToParent.
 *
 * @returns IPRT status code.
 * @param   pThis           The VFS.
 * @param   pPath           The parsed path.  This may be changed as symbolic
 *                          links are processed during the path traversal.
 * @param   fFollowSymlink  Whether to follow the final component if it is a
 *                          symbolic link.
 * @param   ppVfsParentDir  Where to return the parent directory handle
 *                          (referenced).
 */
static int rtVfsTraverseToParent(RTVFSINTERNAL *pThis, PRTVFSPARSEDPATH pPath, bool fFollowSymlink,
                                 RTVFSDIRINTERNAL **ppVfsParentDir)
{
    /*
     * Assert sanity.
     */
    AssertPtr(pThis);
    Assert(pThis->uMagic == RTVFS_MAGIC);
    Assert(pThis->cRefs > 0);
    AssertPtr(pPath);
    AssertPtr(ppVfsParentDir);
    *ppVfsParentDir = NULL;
    AssertReturn(pPath->cComponents > 0, VERR_INTERNAL_ERROR_3);

    /*
     * Open the root directory.
     */
    /** @todo Union mounts, traversal optimization methods, races, ++ */
    RTVFSDIRINTERNAL *pCurDir;
    RTVFS_READ_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnOpenRoot(pThis->pvThis, &pCurDir);
    RTVFS_READ_UNLOCK(pThis->hSemRW);
    if (RT_FAILURE(rc))
        return rc;
    Assert(pCurDir->uMagic == RTVFSDIR_MAGIC);

    /*
     * The traversal loop.
     */
    unsigned cLinks     = 0;
    uint16_t iComponent = 0;
    for (;;)
    {
        /*
         * Are we done yet?
         */
        bool fFinal = iComponent + 1 >= pPath->cComponents;
        if (fFinal && !fFollowSymlink)
        {
            *ppVfsParentDir = pCurDir;
            return VINF_SUCCESS;
        }

        /*
         * Try open the next entry.
         */
        const char     *pszEntry    = &pPath->szPath[pPath->aoffComponents[iComponent]];
        char           *pszEntryEnd = &pPath->szPath[pPath->aoffComponents[iComponent + 1] - 1];
        *pszEntryEnd = '\0';
        RTVFSDIR        hDir     = NIL_RTVFSDIR;
        RTVFSSYMLINK    hSymlink = NIL_RTVFSSYMLINK;
        RTVFS           hVfsMnt  = NIL_RTVFS;
        if (fFinal)
        {
            RTVFS_READ_LOCK(pCurDir->hSemRW);
            rc = pCurDir->pOps->pfnTraversalOpen(pCurDir->pvThis, pszEntry, NULL, &hSymlink, NULL);
            RTVFS_READ_UNLOCK(pCurDir->hSemRW);
            *pszEntryEnd = '\0';
            if (rc == VERR_PATH_NOT_FOUND)
                rc = VINF_SUCCESS;
            if (RT_FAILURE(rc))
                break;

            if (hSymlink == NIL_RTVFSSYMLINK)
            {
                *ppVfsParentDir = pCurDir;
                return VINF_SUCCESS;
            }
        }
        else
        {
            RTVFS_READ_LOCK(pCurDir->hSemRW);
            rc = pCurDir->pOps->pfnTraversalOpen(pCurDir->pvThis, pszEntry, &hDir, &hSymlink, &hVfsMnt);
            RTVFS_READ_UNLOCK(pCurDir->hSemRW);
            *pszEntryEnd = '/';
            if (RT_FAILURE(rc))
                break;

            if (   hDir     == NIL_RTVFSDIR
                && hSymlink == NIL_RTVFSSYMLINK
                && hVfsMnt  == NIL_RTVFS)
            {
                rc = VERR_NOT_A_DIRECTORY;
                break;
            }
        }
        Assert(   (hDir != NIL_RTVFSDIR && hSymlink == NIL_RTVFSSYMLINK && hVfsMnt == NIL_RTVFS)
               || (hDir == NIL_RTVFSDIR && hSymlink != NIL_RTVFSSYMLINK && hVfsMnt == NIL_RTVFS)
               || (hDir == NIL_RTVFSDIR && hSymlink == NIL_RTVFSSYMLINK && hVfsMnt != NIL_RTVFS));

        if (hDir != NIL_RTVFSDIR)
        {
            /*
             * Directory - advance down the path.
             */
            AssertPtr(hDir);
            Assert(hDir->uMagic == RTVFSDIR_MAGIC);
            RTVfsDirRelease(pCurDir);
            pCurDir = hDir;
            iComponent++;
        }
        else if (hSymlink != NIL_RTVFSSYMLINK)
        {
            /*
             * Symbolic link - deal with it and retry the current component.
             */
            AssertPtr(hSymlink);
            Assert(hSymlink->uMagic == RTVFSSYMLINK_MAGIC);
            cLinks++;
            if (cLinks >= RTVFS_MAX_LINKS)
            {
                rc = VERR_TOO_MANY_SYMLINKS;
                break;
            }
            uint16_t iRestartComp = iComponent;
            rc = rtVfsTraverseHandleSymlink(pPath, &iRestartComp, hSymlink);
            if (RT_FAILURE(rc))
                break;
            if (iRestartComp != iComponent)
            {
                /* Must restart from the root (optimize this). */
                RTVfsDirRelease(pCurDir);
                RTVFS_READ_LOCK(pThis->hSemRW);
                rc = pThis->pOps->pfnOpenRoot(pThis->pvThis, &pCurDir);
                RTVFS_READ_UNLOCK(pThis->hSemRW);
                if (RT_FAILURE(rc))
                {
                    pCurDir = NULL;
                    break;
                }
                iComponent = 0;
            }
        }
        else
        {
            /*
             * Mount point - deal with it and retry the current component.
             */
            RTVfsDirRelease(pCurDir);
            RTVFS_READ_LOCK(hVfsMnt->hSemRW);
            rc = pThis->pOps->pfnOpenRoot(hVfsMnt->pvThis, &pCurDir);
            RTVFS_READ_UNLOCK(hVfsMnt->hSemRW);
            if (RT_FAILURE(rc))
            {
                pCurDir = NULL;
                break;
            }
            iComponent = 0;
            /** @todo union mounts. */
        }
    }

    if (pCurDir)
        RTVfsDirRelease(pCurDir);

    return rc;
}



/*
 *
 *  D I R   D I R   D I R
 *  D I R   D I R   D I R
 *  D I R   D I R   D I R
 *
 */

RTDECL(uint32_t)    RTVfsDirRetain(RTVFSDIR hVfsDir)
{
    RTVFSDIRINTERNAL *pThis = hVfsDir;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSDIR_MAGIC, UINT32_MAX);
    return rtVfsRetain(&pThis->cRefs);
}


RTDECL(uint32_t)    RTVfsDirRelease(RTVFSDIR hVfsDir)
{
    RTVFSDIRINTERNAL *pThis = hVfsDir;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSDIR_MAGIC, UINT32_MAX);

    uint32_t cRefs = rtVfsRelease(&pThis->cRefs);
    if (!cRefs)
    {
        RTVFS_WRITE_LOCK(pThis->hSemRW);
        ASMAtomicWriteU32(&pThis->uMagic, RTVFSDIR_MAGIC_DEAD);
        RTVFS_WRITE_UNLOCK(pThis->hSemRW);
        pThis->pOps->Obj.pfnClose(pThis->pvThis);
        RTMemFree(pThis);
    }

    return cRefs;
}



/*
 *
 *  S Y M B O L I C   L I N K
 *  S Y M B O L I C   L I N K
 *  S Y M B O L I C   L I N K
 *
 */

RTDECL(uint32_t)    RTVfsSymlinkRetain(RTVFSSYMLINK hVfsSym)
{
    RTVFSSYMLINKINTERNAL *pThis = hVfsSym;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSSYMLINK_MAGIC, UINT32_MAX);
    return rtVfsRetain(&pThis->cRefs);
}


RTDECL(uint32_t)    RTVfsSymlinkRelease(RTVFSSYMLINK hVfsSym)
{
    RTVFSSYMLINKINTERNAL *pThis = hVfsSym;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSSYMLINK_MAGIC, UINT32_MAX);

    uint32_t cRefs = rtVfsRelease(&pThis->cRefs);
    if (!cRefs)
    {
        RTVFS_WRITE_LOCK(pThis->hSemRW);
        ASMAtomicWriteU32(&pThis->uMagic, RTVFSSYMLINK_MAGIC_DEAD);
        RTVFS_WRITE_UNLOCK(pThis->hSemRW);
        pThis->pOps->Obj.pfnClose(pThis->pvThis);
        RTMemFree(pThis);
    }

    return cRefs;
}


RTDECL(int)         RTVfsSymlinkRead(RTVFSSYMLINK hVfsSym, char *pszTarget, size_t cbTarget)
{
    RTVFSSYMLINKINTERNAL *pThis = hVfsSym;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSSYMLINK_MAGIC, VERR_INVALID_HANDLE);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnRead(pThis->pvThis, pszTarget, cbTarget);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);

    return rc;
}



/*
 *
 *  I / O   S T R E A M     I / O   S T R E A M     I / O   S T R E A M
 *  I / O   S T R E A M     I / O   S T R E A M     I / O   S T R E A M
 *  I / O   S T R E A M     I / O   S T R E A M     I / O   S T R E A M
 *
 */

RTDECL(int) RTVfsNewIoStream(PCRTVFSIOSTREAMOPS pIoStreamOps, size_t cbInstance, uint32_t fOpen, RTVFS hVfs, RTSEMRW hSemRW,
                             PRTVFSIOSTREAM phVfsIos, void **ppvInstance)
{
    /*
     * Validate the input, be extra strict in strict builds.
     */
    AssertPtr(pIoStreamOps);
    AssertReturn(pIoStreamOps->uVersion   == RTVFSIOSTREAMOPS_VERSION, VERR_VERSION_MISMATCH);
    AssertReturn(pIoStreamOps->uEndMarker == RTVFSIOSTREAMOPS_VERSION, VERR_VERSION_MISMATCH);
    Assert(!pIoStreamOps->fReserved);
    Assert(cbInstance > 0);
    Assert(fOpen & RTFILE_O_ACCESS_MASK);
    AssertPtr(ppvInstance);
    AssertPtr(phVfsIos);

    RTVFSINTERNAL *pVfs = NULL;
    if (hVfs == NIL_RTVFS)
    {
        pVfs = hVfs;
        AssertPtrReturn(pVfs, VERR_INVALID_HANDLE);
        AssertReturn(pVfs->uMagic == RTVFS_MAGIC, VERR_INVALID_HANDLE);
    }

    /*
     * Allocate the handle + instance data.
     */
    size_t const cbThis = RT_ALIGN_Z(sizeof(RTVFSIOSTREAMINTERNAL), RTVFS_INST_ALIGNMENT)
                        + RT_ALIGN_Z(cbInstance, RTVFS_INST_ALIGNMENT);
    RTVFSIOSTREAMINTERNAL *pThis = (RTVFSIOSTREAMINTERNAL *)RTMemAllocZ(cbThis);
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->uMagic   = RTVFSIOSTREAM_MAGIC;
    pThis->fFlags   = fOpen;
    pThis->pvThis   = (char *)pThis + RT_ALIGN_Z(sizeof(*pThis), RTVFS_INST_ALIGNMENT);
    pThis->pOps     = pIoStreamOps;
    pThis->hSemRW   = hSemRW != NIL_RTSEMRW ? hSemRW : pVfs ? pVfs->hSemRW : NIL_RTSEMRW;
    pThis->hVfs     = hVfs;
    pThis->cRefs    = 1;
    if (hVfs != NIL_RTVFS)
        rtVfsRetainVoid(&pVfs->cRefs);

    *phVfsIos    = pThis;
    *ppvInstance = pThis->pvThis;
    return VINF_SUCCESS;
}


RTDECL(uint32_t)    RTVfsIoStrmRetain(RTVFSIOSTREAM hVfsIos)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, UINT32_MAX);
    return rtVfsRetain(&pThis->cRefs);
}


RTDECL(uint32_t)    RTVfsIoStrmRelease(RTVFSIOSTREAM hVfsIos)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, UINT32_MAX);

    uint32_t cRefs = rtVfsRelease(&pThis->cRefs);
    if (!cRefs)
    {
        /*
         * That was the last reference, close the stream.
         *
         * This is a little bit more complicated than when releasing a file or
         * directory handle because the I/O stream can be a sub-object and we
         * need to get to the real one before handing it to RTMemFree.
         */
        RTVFS_WRITE_LOCK(pThis->hSemRW);
        ASMAtomicWriteU32(&pThis->uMagic, RTVFSIOSTREAM_MAGIC_DEAD);
        RTVFS_WRITE_UNLOCK(pThis->hSemRW);
        pThis->pOps->Obj.pfnClose(pThis->pvThis);

        switch (pThis->pOps->Obj.enmType)
        {
            case RTVFSOBJTYPE_IOSTREAM:
                RTMemFree(pThis);
                break;

            case RTVFSOBJTYPE_FILE:
            {
                RTVFSFILEINTERNAL *pThisFile = RT_FROM_MEMBER(pThis, RTVFSFILEINTERNAL, Stream);
                ASMAtomicWriteU32(&pThisFile->uMagic, RTVFSIOSTREAM_MAGIC_DEAD);
                RTMemFree(pThisFile);
                break;
            }

            /* Add new I/O stream compatible handle types here. */

            default:
                AssertMsgFailed(("%d\n", pThis->pOps->Obj.enmType));
                break;
        }
    }

    return cRefs;

}


RTDECL(RTVFSFILE)   RTVfsIoStrmToFile(RTVFSIOSTREAM hVfsIos)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, NIL_RTVFSFILE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, NIL_RTVFSFILE);

    if (pThis->pOps->Obj.enmType == RTVFSOBJTYPE_FILE)
    {
        rtVfsRetainVoid(&pThis->cRefs);
        return RT_FROM_MEMBER(pThis, RTVFSFILEINTERNAL, Stream);
    }

    /* this is no crime, so don't assert. */
    return NIL_RTVFSFILE;
}


RTDECL(int)         RTVfsIoStrmQueryInfo(RTVFSIOSTREAM hVfsIos, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);

    RTVFS_READ_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->Obj.pfnQueryInfo(pThis->pvThis, pObjInfo, enmAddAttr);
    RTVFS_READ_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(int)         RTVfsIoStrmRead(RTVFSIOSTREAM hVfsIos, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    AssertPtrNullReturn(pcbRead, VERR_INVALID_POINTER);
    if (pcbRead)
        *pcbRead = 0;
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);

    RTSGSEG Seg = { pvBuf, cbToRead };
    RTSGBUF SgBuf;
    RTSgBufInit(&SgBuf, &Seg, 1);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnRead(pThis->pvThis, -1 /*off*/, &SgBuf, pcbRead == NULL /*fBlocking*/, pcbRead);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(int)         RTVfsIoStrmWrite(RTVFSIOSTREAM hVfsIos, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    AssertPtrNullReturn(pcbWritten, VERR_INVALID_POINTER);
    if (pcbWritten)
        *pcbWritten = 0;
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);

    RTSGSEG Seg = { (void *)pvBuf, cbToWrite };
    RTSGBUF SgBuf;
    RTSgBufInit(&SgBuf, &Seg, 1);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnWrite(pThis->pvThis, -1 /*off*/, &SgBuf, pcbWritten == NULL /*fBlocking*/, pcbWritten);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(int)         RTVfsIoStrmSgRead(RTVFSIOSTREAM hVfsIos, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    AssertPtrNullReturn(pcbRead, VERR_INVALID_POINTER);
    if (pcbRead)
        *pcbRead = 0;
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pSgBuf);
    AssertReturn(fBlocking || pcbRead, VERR_INVALID_PARAMETER);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnRead(pThis->pvThis, -1 /*off*/, pSgBuf, fBlocking, pcbRead);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(int)         RTVfsIoStrmSgWrite(RTVFSIOSTREAM hVfsIos, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    AssertPtrNullReturn(pcbWritten, VERR_INVALID_POINTER);
    if (pcbWritten)
        *pcbWritten = 0;
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);
    AssertPtr(pSgBuf);
    AssertReturn(fBlocking || pcbWritten, VERR_INVALID_PARAMETER);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnWrite(pThis->pvThis, -1 /*off*/, pSgBuf, fBlocking, pcbWritten);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(int)         RTVfsIoStrmFlush(RTVFSIOSTREAM hVfsIos)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnFlush(pThis->pvThis);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(RTFOFF)      RTVfsIoStrmPoll(RTVFSIOSTREAM hVfsIos, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                    uint32_t *pfRetEvents)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, VERR_INVALID_HANDLE);

    RTVFS_WRITE_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnPollOne(pThis->pvThis, fEvents, cMillies, fIntr, pfRetEvents);
    RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    return rc;
}


RTDECL(RTFOFF)      RTVfsIoStrmTell(RTVFSIOSTREAM hVfsIos)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, -1);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, -1);

    RTFOFF off;
    RTVFS_READ_LOCK(pThis->hSemRW);
    int rc = pThis->pOps->pfnTell(pThis->pvThis, &off);
    RTVFS_READ_UNLOCK(pThis->hSemRW);
    if (RT_FAILURE(rc))
        off = rc;
    return off;
}


RTDECL(int)         RTVfsIoStrmSkip(RTVFSIOSTREAM hVfsIos, RTFOFF cb)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, -1);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, -1);
    AssertReturn(cb >= 0, VERR_INVALID_PARAMETER);

    int rc;
    if (pThis->pOps->pfnSkip)
    {
        RTVFS_WRITE_LOCK(pThis->hSemRW);
        rc = pThis->pOps->pfnSkip(pThis->pvThis, cb);
        RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    }
    else
    {
        void *pvBuf = RTMemTmpAlloc(_64K);
        if (pvBuf)
        {
            rc = VINF_SUCCESS;
            while (cb > 0)
            {
                size_t cbToRead = RT_MIN(cb, _64K);
                RTVFS_WRITE_LOCK(pThis->hSemRW);
                rc = RTVfsIoStrmRead(hVfsIos, pvBuf, cbToRead, NULL);
                RTVFS_WRITE_UNLOCK(pThis->hSemRW);
                if (RT_FAILURE(rc))
                    break;
                cb -= cbToRead;
            }

            RTMemTmpFree(pvBuf);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
    }
    return rc;
}


RTDECL(int)         RTVfsIoStrmZeroFill(RTVFSIOSTREAM hVfsIos, RTFOFF cb)
{
    RTVFSIOSTREAMINTERNAL *pThis = hVfsIos;
    AssertPtrReturn(pThis, -1);
    AssertReturn(pThis->uMagic == RTVFSIOSTREAM_MAGIC, -1);

    int rc;
    if (pThis->pOps->pfnSkip)
    {
        RTVFS_WRITE_LOCK(pThis->hSemRW);
        rc = pThis->pOps->pfnZeroFill(pThis->pvThis, cb);
        RTVFS_WRITE_UNLOCK(pThis->hSemRW);
    }
    else
    {
        void *pvBuf = RTMemTmpAllocZ(_64K);
        if (pvBuf)
        {
            rc = VINF_SUCCESS;
            while (cb > 0)
            {
                size_t cbToWrite = RT_MIN(cb, _64K);
                RTVFS_WRITE_LOCK(pThis->hSemRW);
                rc = RTVfsIoStrmWrite(hVfsIos, pvBuf, cbToWrite, NULL);
                RTVFS_WRITE_UNLOCK(pThis->hSemRW);
                if (RT_FAILURE(rc))
                    break;
                cb -= cbToWrite;
            }

            RTMemTmpFree(pvBuf);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
    }
    return rc;
}






/*
 *
 *  F I L E   F I L E   F I L E
 *  F I L E   F I L E   F I L E
 *  F I L E   F I L E   F I L E
 *
 */

RTDECL(int) RTVfsNewFile(PCRTVFSFILEOPS pFileOps, size_t cbInstance, uint32_t fOpen, RTVFS hVfs,
                         PRTVFSFILE phVfsFile, void **ppvInstance)
{
    /*
     * Validate the input, be extra strict in strict builds.
     */
    AssertPtr(pFileOps);
    AssertReturn(pFileOps->uVersion   == RTVFSFILEOPS_VERSION, VERR_VERSION_MISMATCH);
    AssertReturn(pFileOps->uEndMarker == RTVFSFILEOPS_VERSION, VERR_VERSION_MISMATCH);
    Assert(!pFileOps->fReserved);
    Assert(cbInstance > 0);
    Assert(fOpen & RTFILE_O_ACCESS_MASK);
    AssertPtr(ppvInstance);
    AssertPtr(phVfsFile);

    RTVFSINTERNAL *pVfs = NULL;
    if (hVfs == NIL_RTVFS)
    {
        pVfs = hVfs;
        AssertPtrReturn(pVfs, VERR_INVALID_HANDLE);
        AssertReturn(pVfs->uMagic == RTVFS_MAGIC, VERR_INVALID_HANDLE);
    }

    /*
     * Allocate the handle + instance data.
     */
    size_t const cbThis = RT_ALIGN_Z(sizeof(RTVFSFILEINTERNAL), RTVFS_INST_ALIGNMENT)
                        + RT_ALIGN_Z(cbInstance, RTVFS_INST_ALIGNMENT);
    RTVFSFILEINTERNAL *pThis = (RTVFSFILEINTERNAL *)RTMemAllocZ(cbThis);
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->uMagic           = RTVFSFILE_MAGIC;
    pThis->fReserved        = 0;
    pThis->pOps             = pFileOps;
    pThis->Stream.uMagic    = RTVFSIOSTREAM_MAGIC;
    pThis->Stream.fFlags    = fOpen;
    pThis->Stream.pvThis    = (char *)pThis + RT_ALIGN_Z(sizeof(*pThis), RTVFS_INST_ALIGNMENT);
    pThis->Stream.pOps      = &pFileOps->Stream;
    pThis->Stream.hSemRW    = pVfs ? pVfs->hSemRW : NIL_RTSEMRW;
    pThis->Stream.hVfs      = hVfs;
    pThis->Stream.cRefs     = 1;
    if (hVfs != NIL_RTVFS)
        rtVfsRetainVoid(&pVfs->cRefs);

    *phVfsFile   = pThis;
    *ppvInstance = pThis->Stream.pvThis;
    return VINF_SUCCESS;
}


RTDECL(int)         RTVfsFileOpen(RTVFS hVfs, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile)
{
    /*
     * Validate input.
     */
    RTVFSINTERNAL *pThis = hVfs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->uMagic == RTVFS_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(phVfsFile, VERR_INVALID_POINTER);

    int rc = rtFileRecalcAndValidateFlags(&fOpen);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Parse the path, assume current directory is root since we've got no
     * caller context here.
     */
    PRTVFSPARSEDPATH pPath;
    rc = RTVfsParsePathA(pszFilename, "/", &pPath);
    if (RT_SUCCESS(rc))
    {
        if (!pPath->fDirSlash)
        {
            /*
             * Tranverse the path, resolving the parent node and any symlinks
             * in the final element, and ask the directory to open the file.
             */
            RTVFSDIRINTERNAL *pVfsParentDir;
            rc = rtVfsTraverseToParent(pThis, pPath, true /*fFollowSymlink*/, &pVfsParentDir);
            if (RT_SUCCESS(rc))
            {
                const char *pszEntryName = &pPath->szPath[pPath->aoffComponents[pPath->cComponents - 1]];

                /** @todo there is a symlink creation race here. */
                RTVFS_WRITE_LOCK(pVfsParentDir->hSemRW);
                rc = pVfsParentDir->pOps->pfnOpenFile(pVfsParentDir->pvThis, pszEntryName, fOpen, phVfsFile);
                RTVFS_WRITE_UNLOCK(pVfsParentDir->hSemRW);

                RTVfsDirRelease(pVfsParentDir);

                if (RT_SUCCESS(rc))
                {
                    AssertPtr(*phVfsFile);
                    Assert((*phVfsFile)->uMagic == RTVFSFILE_MAGIC);
                }
            }
        }
        else
            rc = VERR_INVALID_PARAMETER;
        RTVfsParsePathFree(pPath);
    }
    return rc;
}


RTDECL(uint32_t)    RTVfsFileRetain(RTVFSFILE hVfsFile)
{
    RTVFSFILEINTERNAL *pThis = hVfsFile;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSFILE_MAGIC, UINT32_MAX);
    return rtVfsRetain(&pThis->Stream.cRefs);
}


RTDECL(uint32_t)    RTVfsFileRelease(RTVFSFILE hVfsFile)
{
    RTVFSFILEINTERNAL *pThis = hVfsFile;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTVFSFILE_MAGIC, UINT32_MAX);

    uint32_t cRefs = rtVfsRelease(&pThis->Stream.cRefs);
    if (!cRefs)
    {
        RTVFS_WRITE_LOCK(pThis->Stream.hSemRW);
        ASMAtomicWriteU32(&pThis->uMagic, RTVFSFILE_MAGIC_DEAD);
        ASMAtomicWriteU32(&pThis->Stream.uMagic, RTVFSIOSTREAM_MAGIC_DEAD);
        RTVFS_WRITE_UNLOCK(pThis->Stream.hSemRW);

        pThis->pOps->Stream.Obj.pfnClose(pThis->Stream.pvThis);
        RTMemFree(pThis);
    }

    return cRefs;
}


RTDECL(RTVFSIOSTREAM) RTVfsFileToIoStream(RTVFSFILE hVfsFile)
{
    RTVFSFILEINTERNAL *pThis = hVfsFile;
    AssertPtrReturn(pThis, NIL_RTVFSIOSTREAM);
    AssertReturn(pThis->uMagic == RTVFSFILE_MAGIC, NIL_RTVFSIOSTREAM);

    rtVfsRetainVoid(&pThis->Stream.cRefs);
    return &pThis->Stream;
}



#if 0 /* unfinished code => laptop */

/*
 *
 *  V F S   c h a i n   s p e c i f i c a t i o n s
 *  V F S   c h a i n   s p e c i f i c a t i o n s
 *  V F S   c h a i n   s p e c i f i c a t i o n s
 *
 */

/**
 * A parsed VFS setup specficiation.
 *
 * Some specification examples.
 *   :iprtvfs:ios(stdfile="./foo.tgz")|ios(gzip)|vfs(tar)
 */
typedef struct RTVFSPARSEDSPEC
{
    uint32_t    cElements;
} RTVFSPARSEDSPEC;
/** Pointer to a parse VFS setup specification. */
typedef RTVFSPARSEDSPEC *PRTVFSPARSEDSPEC;


/**
 * Parses the VFS setup specficiation.
 *
 * @returns
 * @param   pInfo       The output.
 * @param   pszSpec     The input.  This needs some more work but the basic
 *                      are that anything that does not start with ":iprtvfs:"
 *                      will be treated like a file.  ":iprtvfs:" prefixed
 *                      specifications will be understood as a VFS chain
 *                      specification and parsed and constructured (by the
 *                      caller).
 * @param
 */
static int rtVfsSpecParse(PRTVFSPARSEDSPEC pInfo, const char *pszSpec)
{

}


RTDECL(int) RTVfsOpenIoStreamFromSpec(const char *pszSpec, uint32_t fOpen, RTVFSIOSTREAM hVfs)
{

}

#endif

