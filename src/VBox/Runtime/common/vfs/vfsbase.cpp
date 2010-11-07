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


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define RTVFS_MAGIC                 UINT32_C(0x11112222)
#define RTVFS_MAGIC_DEAD            (~RTVFS_MAGIC)
#define RTVFSIOSTREAM_MAGIC         UINT32_C(0x33334444)
#define RTVFSIOSTREAM_MAGIC_DEAD    (~RTVFSIOSTREAM_MAGIC)
#define RTVFSFILE_MAGIC             UINT32_C(0x55556666)
#define RTVFSFILE_MAGIC_DEAD        (~RTVFSFILE_MAGIC)

/** The instance data alignment. */
#define RTVFS_INST_ALIGNMENT        16U


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
        ASMAtomicWriteU32(&pThis->uMagic, RTVFSFILE_MAGIC_DEAD);
        ASMAtomicWriteU32(&pThis->Stream.uMagic, RTVFSIOSTREAM_MAGIC_DEAD);
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

