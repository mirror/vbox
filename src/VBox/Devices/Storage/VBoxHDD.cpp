/* $Id$ */
/** @file
 * VBoxHDD - VBox HDD Container implementation.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD
#include <VBox/VBoxHDD.h>
#include <VBox/err.h>
#include <VBox/sup.h>
#include <VBox/log.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/memcache.h>
#include <iprt/sg.h>
#include <iprt/critsect.h>
#include <iprt/list.h>
#include <iprt/avl.h>

#include <VBox/VBoxHDD-Plugin.h>
#include <VBox/VBoxHDD-CachePlugin.h>

#define VBOXHDDDISK_SIGNATURE 0x6f0e2a7d

/** Buffer size used for merging images. */
#define VD_MERGE_BUFFER_SIZE    (16 * _1M)

/** Maximum number of segments in one I/O task. */
#define VD_IO_TASK_SEGMENTS_MAX 64

/**
 * VD async I/O interface storage descriptor.
 */
typedef struct VDIIOFALLBACKSTORAGE
{
    /** File handle. */
    RTFILE         File;
    /** Completion callback. */
    PFNVDCOMPLETED pfnCompleted;
    /** Thread for async access. */
    RTTHREAD       ThreadAsync;
} VDIIOFALLBACKSTORAGE, *PVDIIOFALLBACKSTORAGE;

/**
 * VBox HDD Container image descriptor.
 */
typedef struct VDIMAGE
{
    /** Link to parent image descriptor, if any. */
    struct VDIMAGE  *pPrev;
    /** Link to child image descriptor, if any. */
    struct VDIMAGE  *pNext;
    /** Container base filename. (UTF-8) */
    char            *pszFilename;
    /** Data managed by the backend which keeps the actual info. */
    void            *pBackendData;
    /** Cached sanitized image flags. */
    unsigned        uImageFlags;
    /** Image open flags (only those handled generically in this code and which
     * the backends will never ever see). */
    unsigned        uOpenFlags;

    /** Function pointers for the various backend methods. */
    PCVBOXHDDBACKEND    Backend;
    /** Per image internal I/O interface. */
    VDINTERFACE         VDIIO;
    /** Pointer to list of VD interfaces, per-image. */
    PVDINTERFACE        pVDIfsImage;
    /** Disk this image is part of */
    PVBOXHDD            pDisk;
} VDIMAGE, *PVDIMAGE;

/**
 * uModified bit flags.
 */
#define VD_IMAGE_MODIFIED_FLAG                  RT_BIT(0)
#define VD_IMAGE_MODIFIED_FIRST                 RT_BIT(1)
#define VD_IMAGE_MODIFIED_DISABLE_UUID_UPDATE   RT_BIT(2)


/**
 * VBox HDD Cache image descriptor.
 */
typedef struct VDCACHE
{
    /** Cache base filename. (UTF-8) */
    char            *pszFilename;
    /** Data managed by the backend which keeps the actual info. */
    void            *pBackendData;
    /** Cached sanitized image flags. */
    unsigned        uImageFlags;
    /** Image open flags (only those handled generically in this code and which
     * the backends will never ever see). */
    unsigned        uOpenFlags;

    /** Function pointers for the various backend methods. */
    PCVDCACHEBACKEND    Backend;
    /** Per image internal I/O interface. */
    VDINTERFACE         VDIIO;
    /** Pointer to list of VD interfaces, per-cache. */
    PVDINTERFACE        pVDIfsCache;
    /** Disk this image is part of */
    PVBOXHDD            pDisk;
} VDCACHE, *PVDCACHE;

/**
 * VBox HDD Container main structure, private part.
 */
struct VBOXHDD
{
    /** Structure signature (VBOXHDDDISK_SIGNATURE). */
    uint32_t            u32Signature;

    /** Number of opened images. */
    unsigned            cImages;

    /** Base image. */
    PVDIMAGE            pBase;

    /** Last opened image in the chain.
     * The same as pBase if only one image is used. */
    PVDIMAGE            pLast;

    /** Flags representing the modification state. */
    unsigned            uModified;

    /** Cached size of this disk. */
    uint64_t            cbSize;
    /** Cached PCHS geometry for this disk. */
    VDGEOMETRY          PCHSGeometry;
    /** Cached LCHS geometry for this disk. */
    VDGEOMETRY          LCHSGeometry;

    /** Pointer to list of VD interfaces, per-disk. */
    PVDINTERFACE        pVDIfsDisk;
    /** Pointer to the common interface structure for error reporting. */
    PVDINTERFACE        pInterfaceError;
    /** Pointer to the error interface callbacks we use if available. */
    PVDINTERFACEERROR   pInterfaceErrorCallbacks;

    /** Pointer to the optional thread synchronization interface. */
    PVDINTERFACE        pInterfaceThreadSync;
    /** Pointer to the optional thread synchronization callbacks. */
    PVDINTERFACETHREADSYNC pInterfaceThreadSyncCallbacks;

    /** Internal I/O interface callback table for the images. */
    VDINTERFACEIOINT    VDIIOIntCallbacks;

    /** I/O interface to the upper layer. */
    PVDINTERFACE        pInterfaceIO;
    /** I/O interface callback table. */
    PVDINTERFACEIO      pInterfaceIOCallbacks;

    /** Fallback I/O interface. */
    VDINTERFACE         VDIIO;
    /** Callback table for the fallback I/O interface. */
    VDINTERFACEIO       VDIIOCallbacks;

    /** Memory cache for I/O contexts */
    RTMEMCACHE          hMemCacheIoCtx;
    /** Memory cache for I/O tasks. */
    RTMEMCACHE          hMemCacheIoTask;
    /** Critical section protecting the disk against concurrent access. */
    RTCRITSECT          CritSect;
    /** Flag whether the last image is currently written to and needs to grow.
     * Other write requests which will grow the image too need to be deferred to
     * prevent data corruption. - Protected by the critical section.
     */
    volatile bool       fGrowing;
    /** List of waiting requests. - Protected by the critical section. */
    RTLISTNODE          ListWriteGrowing;

    /** Pointer to the L2 disk cache if any. */
    PVDCACHE            pCache;
};

# define VD_THREAD_IS_CRITSECT_OWNER(Disk) \
    do \
    { \
        AssertMsg(RTCritSectIsOwner(&Disk->CritSect), \
                  ("Thread does not own critical section\n"));\
    } while(0)

/**
 * VBox parent read descriptor, used internally for compaction.
 */
typedef struct VDPARENTSTATEDESC
{
    /** Pointer to disk descriptor. */
    PVBOXHDD pDisk;
    /** Pointer to image descriptor. */
    PVDIMAGE pImage;
} VDPARENTSTATEDESC, *PVDPARENTSTATEDESC;

/**
 * Transfer direction.
 */
typedef enum VDIOCTXTXDIR
{
    /** Read */
    VDIOCTXTXDIR_READ = 0,
    /** Write */
    VDIOCTXTXDIR_WRITE,
    /** Flush */
    VDIOCTXTXDIR_FLUSH,
    /** 32bit hack */
    VDIOCTXTXDIR_32BIT_HACK = 0x7fffffff
} VDIOCTXTXDIR, *PVDIOCTXTXDIR;

/** Transfer function */
typedef DECLCALLBACK(int) FNVDIOCTXTRANSFER (PVDIOCTX pIoCtx);
/** Pointer to a transfer function. */
typedef FNVDIOCTXTRANSFER *PFNVDIOCTXTRANSFER;

/**
 * I/O context
 */
typedef struct VDIOCTX
{
    /** Disk this is request is for. */
    PVBOXHDD                     pDisk;
    /** Return code. */
    int                          rcReq;
    /** Transfer direction */
    VDIOCTXTXDIR                 enmTxDir;
    /** Number of bytes left until this context completes. */
    volatile uint32_t            cbTransferLeft;
    /** Current offset */
    volatile uint64_t            uOffset;
    /** Number of bytes to transfer */
    volatile size_t              cbTransfer;
    /** Current image in the chain. */
    PVDIMAGE                     pImage;
    /** S/G buffer */
    RTSGBUF                      SgBuf;
    /** Flag whether the I/O context is blocked because it is in the growing list. */
    bool                         fBlocked;
    /** Number of data transfers currently pending. */
    volatile uint32_t            cDataTransfersPending;
    /** How many meta data transfers are pending. */
    volatile uint32_t            cMetaTransfersPending;
    /** Flag whether the request finished */
    volatile bool                fComplete;
    /** Temporary allocated memory which is freed
     * when the context completes. */
    void                        *pvAllocation;
    /** Transfer function. */
    PFNVDIOCTXTRANSFER           pfnIoCtxTransfer;
    /** Next transfer part after the current one completed. */
    PFNVDIOCTXTRANSFER           pfnIoCtxTransferNext;
    /** Parent I/O context if any. Sets the type of the context (root/child) */
    PVDIOCTX                     pIoCtxParent;
    /** Type dependent data (root/child) */
    union
    {
        /** Root data */
        struct
        {
            /** Completion callback */
            PFNVDASYNCTRANSFERCOMPLETE   pfnComplete;
            /** User argument 1 passed on completion. */
            void                        *pvUser1;
            /** User argument 1 passed on completion. */
            void                        *pvUser2;
        } Root;
        /** Child data */
        struct
        {
            /** Saved start offset */
            uint64_t                     uOffsetSaved;
            /** Saved transfer size */
            size_t                       cbTransferLeftSaved;
            /** Number of bytes transfered from the parent if this context completes. */
            size_t                       cbTransferParent;
            /** Number of bytes to pre read */
            size_t                       cbPreRead;
            /** Number of bytes to post read. */
            size_t                       cbPostRead;
            /** Number of bytes to write left in the parent. */
            size_t                       cbWriteParent;
            /** Write type dependent data. */
            union
            {
                /** Optimized */
                struct
                {
                    /** Bytes to fill to satisfy the block size. Not part of the virtual disk. */
                    size_t               cbFill;
                    /** Bytes to copy instead of reading from the parent */
                    size_t               cbWriteCopy;
                    /** Bytes to read from the image. */
                    size_t               cbReadImage;
                } Optimized;
            } Write;
        } Child;
    } Type;
} VDIOCTX;

typedef struct VDIOCTXDEFERRED
{
    /** Node in the list of deferred requests.
     * A request can be deferred if the image is growing
     * and the request accesses the same range or if
     * the backend needs to read or write metadata from the disk
     * before it can continue. */
    RTLISTNODE NodeDeferred;
    /** I/O context this entry points to. */
    PVDIOCTX   pIoCtx;
} VDIOCTXDEFERRED, *PVDIOCTXDEFERRED;

/**
 * I/O task.
 */
typedef struct VDIOTASK
{
    /** Storage this task belongs to. */
    PVDIOSTORAGE                 pIoStorage;
    /** Optional completion callback. */
    PFNVDXFERCOMPLETED           pfnComplete;
    /** Opaque user data. */
    void                        *pvUser;
    /** Flag whether this is a meta data transfer. */
    bool                         fMeta;
    /** Type dependent data. */
    union
    {
        /** User data transfer. */
        struct
        {
            /** Number of bytes this task transfered. */
            uint32_t             cbTransfer;
            /** Pointer to the I/O context the task belongs. */
            PVDIOCTX             pIoCtx;
        } User;
        /** Meta data transfer. */
        struct
        {
            /** Meta transfer this task is for. */
            PVDMETAXFER          pMetaXfer;
        } Meta;
    } Type;
} VDIOTASK, *PVDIOTASK;

/**
 * Storage handle.
 */
typedef struct VDIOSTORAGE
{
    /** Image this storage handle belongs to. */
    PVDIMAGE                     pImage;
    /** AVL tree for pending async metadata transfers. */
    PAVLRFOFFTREE                pTreeMetaXfers;
    /** Storage handle */
    void                        *pStorage;
} VDIOSTORAGE;

/**
 *  Metadata transfer.
 *
 *  @note This entry can't be freed if either the list is not empty or
 *  the reference counter is not 0.
 *  The assumption is that the backends don't need to read huge amounts of
 *  metadata to complete a transfer so the additional memory overhead should
 *  be relatively small.
 */
typedef struct VDMETAXFER
{
    /** AVL core for fast search (the file offset is the key) */
    AVLRFOFFNODECORE Core;
    /** I/O storage for this transfer. */
    PVDIOSTORAGE     pIoStorage;
    /** Flags. */
    uint32_t         fFlags;
    /** List of I/O contexts waiting for this metadata transfer to complete. */
    RTLISTNODE       ListIoCtxWaiting;
    /** Number of references to this entry. */
    unsigned         cRefs;
    /** Size of the data stored with this entry. */
    size_t           cbMeta;
    /** Data stored - variable size. */
    uint8_t          abData[1];
} VDMETAXFER;

/**
 * The transfer direction for the metadata.
 */
#define VDMETAXFER_TXDIR_MASK  0x3
#define VDMETAXFER_TXDIR_NONE  0x0
#define VDMETAXFER_TXDIR_WRITE 0x1
#define VDMETAXFER_TXDIR_READ  0x2
#define VDMETAXFER_TXDIR_FLUSH 0x3
#define VDMETAXFER_TXDIR_GET(flags)      ((flags) & VDMETAXFER_TXDIR_MASK)
#define VDMETAXFER_TXDIR_SET(flags, dir) ((flags) = (flags & ~VDMETAXFER_TXDIR_MASK) | (dir))

extern VBOXHDDBACKEND g_RawBackend;
extern VBOXHDDBACKEND g_VmdkBackend;
extern VBOXHDDBACKEND g_VDIBackend;
extern VBOXHDDBACKEND g_VhdBackend;
extern VBOXHDDBACKEND g_ParallelsBackend;
#ifdef VBOX_WITH_DMG
extern VBOXHDDBACKEND g_DmgBackend;
#endif
#ifdef VBOX_WITH_ISCSI
extern VBOXHDDBACKEND g_ISCSIBackend;
#endif

static unsigned g_cBackends = 0;
static PVBOXHDDBACKEND *g_apBackends = NULL;
static PVBOXHDDBACKEND aStaticBackends[] =
{
    &g_RawBackend,
    &g_VmdkBackend,
    &g_VDIBackend,
    &g_VhdBackend,
    &g_ParallelsBackend
#ifdef VBOX_WITH_DMG
    ,&g_DmgBackend
#endif
#ifdef VBOX_WITH_ISCSI
    ,&g_ISCSIBackend
#endif
};

/**
 * Supported backends for the disk cache.
 */
extern VDCACHEBACKEND g_VciCacheBackend;

static unsigned g_cCacheBackends = 0;
static PVDCACHEBACKEND *g_apCacheBackends = NULL;
static PVDCACHEBACKEND aStaticCacheBackends[] =
{
    &g_VciCacheBackend
};

/**
 * internal: add several backends.
 */
static int vdAddBackends(PVBOXHDDBACKEND *ppBackends, unsigned cBackends)
{
    PVBOXHDDBACKEND *pTmp = (PVBOXHDDBACKEND*)RTMemRealloc(g_apBackends,
           (g_cBackends + cBackends) * sizeof(PVBOXHDDBACKEND));
    if (RT_UNLIKELY(!pTmp))
        return VERR_NO_MEMORY;
    g_apBackends = pTmp;
    memcpy(&g_apBackends[g_cBackends], ppBackends, cBackends * sizeof(PVBOXHDDBACKEND));
    g_cBackends += cBackends;
    return VINF_SUCCESS;
}

/**
 * internal: add single backend.
 */
DECLINLINE(int) vdAddBackend(PVBOXHDDBACKEND pBackend)
{
    return vdAddBackends(&pBackend, 1);
}

/**
 * internal: add several cache backends.
 */
static int vdAddCacheBackends(PVDCACHEBACKEND *ppBackends, unsigned cBackends)
{
    PVDCACHEBACKEND *pTmp = (PVDCACHEBACKEND*)RTMemRealloc(g_apCacheBackends,
           (g_cCacheBackends + cBackends) * sizeof(PVDCACHEBACKEND));
    if (RT_UNLIKELY(!pTmp))
        return VERR_NO_MEMORY;
    g_apCacheBackends = pTmp;
    memcpy(&g_apCacheBackends[g_cCacheBackends], ppBackends, cBackends * sizeof(PVDCACHEBACKEND));
    g_cCacheBackends += cBackends;
    return VINF_SUCCESS;
}

/**
 * internal: add single cache backend.
 */
DECLINLINE(int) vdAddCacheBackend(PVDCACHEBACKEND pBackend)
{
    return vdAddCacheBackends(&pBackend, 1);
}

/**
 * internal: issue error message.
 */
static int vdError(PVBOXHDD pDisk, int rc, RT_SRC_POS_DECL,
                   const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pDisk->pInterfaceErrorCallbacks)
        pDisk->pInterfaceErrorCallbacks->pfnError(pDisk->pInterfaceError->pvUser, rc, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * internal: thread synchronization, start read.
 */
DECLINLINE(int) vdThreadStartRead(PVBOXHDD pDisk)
{
    int rc = VINF_SUCCESS;
    if (RT_UNLIKELY(pDisk->pInterfaceThreadSyncCallbacks))
        rc = pDisk->pInterfaceThreadSyncCallbacks->pfnStartRead(pDisk->pInterfaceThreadSync->pvUser);
    return rc;
}

/**
 * internal: thread synchronization, finish read.
 */
DECLINLINE(int) vdThreadFinishRead(PVBOXHDD pDisk)
{
    int rc = VINF_SUCCESS;
    if (RT_UNLIKELY(pDisk->pInterfaceThreadSyncCallbacks))
        rc = pDisk->pInterfaceThreadSyncCallbacks->pfnFinishRead(pDisk->pInterfaceThreadSync->pvUser);
    return rc;
}

/**
 * internal: thread synchronization, start write.
 */
DECLINLINE(int) vdThreadStartWrite(PVBOXHDD pDisk)
{
    int rc = VINF_SUCCESS;
    if (RT_UNLIKELY(pDisk->pInterfaceThreadSyncCallbacks))
        rc = pDisk->pInterfaceThreadSyncCallbacks->pfnStartWrite(pDisk->pInterfaceThreadSync->pvUser);
    return rc;
}

/**
 * internal: thread synchronization, finish write.
 */
DECLINLINE(int) vdThreadFinishWrite(PVBOXHDD pDisk)
{
    int rc = VINF_SUCCESS;
    if (RT_UNLIKELY(pDisk->pInterfaceThreadSyncCallbacks))
        rc = pDisk->pInterfaceThreadSyncCallbacks->pfnFinishWrite(pDisk->pInterfaceThreadSync->pvUser);
    return rc;
}

/**
 * internal: find image format backend.
 */
static int vdFindBackend(const char *pszBackend, PCVBOXHDDBACKEND *ppBackend)
{
    int rc = VINF_SUCCESS;
    PCVBOXHDDBACKEND pBackend = NULL;

    if (!g_apBackends)
        VDInit();

    for (unsigned i = 0; i < g_cBackends; i++)
    {
        if (!RTStrICmp(pszBackend, g_apBackends[i]->pszBackendName))
        {
            pBackend = g_apBackends[i];
            break;
        }
    }
    *ppBackend = pBackend;
    return rc;
}

/**
 * internal: find cache format backend.
 */
static int vdFindCacheBackend(const char *pszBackend, PCVDCACHEBACKEND *ppBackend)
{
    int rc = VINF_SUCCESS;
    PCVDCACHEBACKEND pBackend = NULL;

    if (!g_apCacheBackends)
        VDInit();

    for (unsigned i = 0; i < g_cCacheBackends; i++)
    {
        if (!RTStrICmp(pszBackend, g_apCacheBackends[i]->pszBackendName))
        {
            pBackend = g_apCacheBackends[i];
            break;
        }
    }
    *ppBackend = pBackend;
    return rc;
}

/**
 * internal: add image structure to the end of images list.
 */
static void vdAddImageToList(PVBOXHDD pDisk, PVDIMAGE pImage)
{
    pImage->pPrev = NULL;
    pImage->pNext = NULL;

    if (pDisk->pBase)
    {
        Assert(pDisk->cImages > 0);
        pImage->pPrev = pDisk->pLast;
        pDisk->pLast->pNext = pImage;
        pDisk->pLast = pImage;
    }
    else
    {
        Assert(pDisk->cImages == 0);
        pDisk->pBase = pImage;
        pDisk->pLast = pImage;
    }

    pDisk->cImages++;
}

/**
 * internal: remove image structure from the images list.
 */
static void vdRemoveImageFromList(PVBOXHDD pDisk, PVDIMAGE pImage)
{
    Assert(pDisk->cImages > 0);

    if (pImage->pPrev)
        pImage->pPrev->pNext = pImage->pNext;
    else
        pDisk->pBase = pImage->pNext;

    if (pImage->pNext)
        pImage->pNext->pPrev = pImage->pPrev;
    else
        pDisk->pLast = pImage->pPrev;

    pImage->pPrev = NULL;
    pImage->pNext = NULL;

    pDisk->cImages--;
}

/**
 * internal: find image by index into the images list.
 */
static PVDIMAGE vdGetImageByNumber(PVBOXHDD pDisk, unsigned nImage)
{
    PVDIMAGE pImage = pDisk->pBase;
    if (nImage == VD_LAST_IMAGE)
        return pDisk->pLast;
    while (pImage && nImage)
    {
        pImage = pImage->pNext;
        nImage--;
    }
    return pImage;
}

/**
 * Internal: Tries to read the desired range from the given cache.
 *
 * @returns VBox status code.
 * @retval  VERR_VD_BLOCK_FREE if the block is not in the cache.
 *          pcbRead will be set to the number of bytes not in the cache.
 *          Everything thereafter might be in the cache.
 * @param   pCache   The cache to read from.
 * @param   uOffset  Offset of the virtual disk to read.
 * @param   pvBuf    Where to store the read data.
 * @param   cbRead   How much to read.
 * @param   pcbRead  Where to store the number of bytes actually read.
 *                   On success this indicates the number of bytes read from the cache.
 *                   If VERR_VD_BLOCK_FREE is returned this gives the number of bytes
 *                   whih are not in the cache.
 *                   In both cases everything beyond this value
 *                   might or might not be in the cache.
 */
static int vdCacheReadHelper(PVDCACHE pCache, uint64_t uOffset,
                             void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pCache=%#p uOffset=%llu pvBuf=%#p cbRead=%zu pcbRead=%#p\n",
                 pCache, uOffset, pvBuf, cbRead, pcbRead));

    AssertPtr(pCache);
    AssertPtr(pcbRead);

    rc = pCache->Backend->pfnRead(pCache->pBackendData, uOffset, pvBuf,
                                  cbRead, pcbRead);

    LogFlowFunc(("returns rc=%Rrc pcbRead=%zu\n", rc, *pcbRead));
    return rc;
}

/**
 * Internal: Writes data for the given block into the cache.
 *
 * @returns VBox status code.
 * @param   pCache     The cache to write to.
 * @param   uOffset    Offset of the virtual disk to write to teh cache.
 * @param   pcvBuf     The data to write.
 * @param   cbWrite    How much to write.
 * @param   pcbWritten How much data could be written, optional.
 */
static int vdCacheWriteHelper(PVDCACHE pCache, uint64_t uOffset, const void *pcvBuf,
                              size_t cbWrite, size_t *pcbWritten)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pCache=%#p uOffset=%llu pvBuf=%#p cbWrite=%zu pcbWritten=%#p\n",
                 pCache, uOffset, pcvBuf, cbWrite, pcbWritten));

    AssertPtr(pCache);
    AssertPtr(pcvBuf);
    Assert(cbWrite > 0);

    if (pcbWritten)
        rc = pCache->Backend->pfnWrite(pCache->pBackendData, uOffset, pcvBuf,
                                       cbWrite, pcbWritten);
    else
    {
        size_t cbWritten = 0;

        do
        {
            rc = pCache->Backend->pfnWrite(pCache->pBackendData, uOffset, pcvBuf,
                                           cbWrite, &cbWritten);
            uOffset += cbWritten;
            pcvBuf   = (char *)pcvBuf + cbWritten;
            cbWrite -= cbWritten;
        } while (   cbWrite
                 && RT_SUCCESS(rc));
    }

    LogFlowFunc(("returns rc=%Rrc pcbWritten=%zu\n",
                 rc, pcbWritten ? *pcbWritten : cbWrite));
    return rc;
}

/**
 * Internal: Reads a given amount of data from the image chain of the disk.
 **/
static int vdDiskReadHelper(PVBOXHDD pDisk, PVDIMAGE pImage, PVDIMAGE pImageParentOverride,
                            uint64_t uOffset, void *pvBuf, size_t cbRead, size_t *pcbThisRead)
{
    int rc = VINF_SUCCESS;
    size_t cbThisRead = cbRead;

    AssertPtr(pcbThisRead);

    *pcbThisRead = 0;

    /*
     * Try to read from the given image.
     * If the block is not allocated read from override chain if present.
     */
    rc = pImage->Backend->pfnRead(pImage->pBackendData,
                                  uOffset, pvBuf, cbThisRead,
                                  &cbThisRead);

    if (rc == VERR_VD_BLOCK_FREE)
    {
        for (PVDIMAGE pCurrImage = pImageParentOverride ? pImageParentOverride : pImage->pPrev;
             pCurrImage != NULL && rc == VERR_VD_BLOCK_FREE;
             pCurrImage = pCurrImage->pPrev)
        {
            rc = pCurrImage->Backend->pfnRead(pCurrImage->pBackendData,
                                              uOffset, pvBuf, cbThisRead,
                                              &cbThisRead);
        }
    }

    if (RT_SUCCESS(rc))
        *pcbThisRead = cbThisRead;

    return rc;
}

/**
 * internal: read the specified amount of data in whatever blocks the backend
 * will give us.
 */
static int vdReadHelper(PVBOXHDD pDisk, PVDIMAGE pImage, PVDIMAGE pImageParentOverride,
                        uint64_t uOffset, void *pvBuf, size_t cbRead,
                        bool fZeroFreeBlocks, bool fUpdateCache)
{
    int rc = VINF_SUCCESS;
    size_t cbThisRead;
    bool fAllFree = true;
    size_t cbBufClear = 0;

    /* Loop until all read. */
    do
    {
        /* Search for image with allocated block. Do not attempt to read more
         * than the previous reads marked as valid. Otherwise this would return
         * stale data when different block sizes are used for the images. */
        cbThisRead = cbRead;

        if (   pDisk->pCache
            && !pImageParentOverride)
        {
            rc = vdCacheReadHelper(pDisk->pCache, uOffset, pvBuf,
                                   cbThisRead, &cbThisRead);

            if (rc == VERR_VD_BLOCK_FREE)
            {
                rc = vdDiskReadHelper(pDisk, pImage, pImageParentOverride,
                                      uOffset, pvBuf, cbThisRead, &cbThisRead);

                /* If the read was successful, write the data back into the cache. */
                if (   RT_SUCCESS(rc)
                    && fUpdateCache)
                {
                    rc = vdCacheWriteHelper(pDisk->pCache, uOffset, pvBuf,
                                            cbThisRead, NULL);
                }
            }
        }
        else
        {
            /** @todo can be be replaced by vdDiskReadHelper if it proves to be reliable,
             * don't want to be responsible for data corruption...
             */
            /*
             * Try to read from the given image.
             * If the block is not allocated read from override chain if present.
             */
            rc = pImage->Backend->pfnRead(pImage->pBackendData,
                                          uOffset, pvBuf, cbThisRead,
                                          &cbThisRead);

            if (rc == VERR_VD_BLOCK_FREE)
            {
                for (PVDIMAGE pCurrImage = pImageParentOverride ? pImageParentOverride : pImage->pPrev;
                     pCurrImage != NULL && rc == VERR_VD_BLOCK_FREE;
                     pCurrImage = pCurrImage->pPrev)
                {
                    rc = pCurrImage->Backend->pfnRead(pCurrImage->pBackendData,
                                                      uOffset, pvBuf, cbThisRead,
                                                      &cbThisRead);
                }
            }
        }

        /* No image in the chain contains the data for the block. */
        if (rc == VERR_VD_BLOCK_FREE)
        {
            /* Fill the free space with 0 if we are told to do so
             * or a previous read returned valid data. */
            if (fZeroFreeBlocks || !fAllFree)
                memset(pvBuf, '\0', cbThisRead);
            else
                cbBufClear += cbThisRead;

            rc = VINF_SUCCESS;
        }
        else if (RT_SUCCESS(rc))
        {
            /* First not free block, fill the space before with 0. */
            if (!fZeroFreeBlocks)
            {
                memset((char *)pvBuf - cbBufClear, '\0', cbBufClear);
                cbBufClear = 0;
                fAllFree = false;
            }
        }

        cbRead -= cbThisRead;
        uOffset += cbThisRead;
        pvBuf = (char *)pvBuf + cbThisRead;
    } while (cbRead != 0 && RT_SUCCESS(rc));

    return (!fZeroFreeBlocks && fAllFree) ? VERR_VD_BLOCK_FREE : rc;
}

DECLINLINE(PVDIOCTX) vdIoCtxAlloc(PVBOXHDD pDisk, VDIOCTXTXDIR enmTxDir,
                                  uint64_t uOffset, size_t cbTransfer,
                                  PCRTSGSEG pcaSeg, unsigned cSeg,
                                  void *pvAllocation,
                                  PFNVDIOCTXTRANSFER pfnIoCtxTransfer)
{
    PVDIOCTX pIoCtx = NULL;

    pIoCtx = (PVDIOCTX)RTMemCacheAlloc(pDisk->hMemCacheIoCtx);
    if (RT_LIKELY(pIoCtx))
    {
        pIoCtx->pDisk                 = pDisk;
        pIoCtx->enmTxDir              = enmTxDir;
        pIoCtx->cbTransferLeft        = cbTransfer;
        pIoCtx->uOffset               = uOffset;
        pIoCtx->cbTransfer            = cbTransfer;
        pIoCtx->cDataTransfersPending = 0;
        pIoCtx->cMetaTransfersPending = 0;
        pIoCtx->fComplete             = false;
        pIoCtx->fBlocked              = false;
        pIoCtx->pvAllocation          = pvAllocation;
        pIoCtx->pfnIoCtxTransfer      = pfnIoCtxTransfer;
        pIoCtx->pfnIoCtxTransferNext  = NULL;
        pIoCtx->rcReq                 = VINF_SUCCESS;

        /* There is no S/G list for a flush request. */
        if (enmTxDir != VDIOCTXTXDIR_FLUSH)
            RTSgBufInit(&pIoCtx->SgBuf, pcaSeg, cSeg);
        else
            memset(&pIoCtx->SgBuf, 0, sizeof(RTSGBUF));
    }

    return pIoCtx;
}

DECLINLINE(PVDIOCTX) vdIoCtxRootAlloc(PVBOXHDD pDisk, VDIOCTXTXDIR enmTxDir,
                                      uint64_t uOffset, size_t cbTransfer,
                                      PCRTSGSEG paSeg, unsigned cSeg,
                                      PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                                      void *pvUser1, void *pvUser2,
                                      void *pvAllocation,
                                      PFNVDIOCTXTRANSFER pfnIoCtxTransfer)
{
    PVDIOCTX pIoCtx = vdIoCtxAlloc(pDisk, enmTxDir, uOffset, cbTransfer,
                                   paSeg, cSeg, pvAllocation, pfnIoCtxTransfer);

    if (RT_LIKELY(pIoCtx))
    {
        pIoCtx->pIoCtxParent          = NULL;
        pIoCtx->Type.Root.pfnComplete = pfnComplete;
        pIoCtx->Type.Root.pvUser1     = pvUser1;
        pIoCtx->Type.Root.pvUser2     = pvUser2;
    }

    LogFlow(("Allocated root I/O context %#p\n", pIoCtx));
    return pIoCtx;
}

DECLINLINE(PVDIOCTX) vdIoCtxChildAlloc(PVBOXHDD pDisk, VDIOCTXTXDIR enmTxDir,
                                       uint64_t uOffset, size_t cbTransfer,
                                       PCRTSGSEG paSeg, unsigned cSeg,
                                       PVDIOCTX pIoCtxParent, size_t cbTransferParent,
                                       size_t cbWriteParent, void *pvAllocation,
                                       PFNVDIOCTXTRANSFER pfnIoCtxTransfer)
{
    PVDIOCTX pIoCtx = vdIoCtxAlloc(pDisk, enmTxDir, uOffset, cbTransfer,
                                   paSeg, cSeg, pvAllocation, pfnIoCtxTransfer);

    AssertPtr(pIoCtxParent);
    Assert(!pIoCtxParent->pIoCtxParent);

    if (RT_LIKELY(pIoCtx))
    {
        pIoCtx->pIoCtxParent                   = pIoCtxParent;
        pIoCtx->Type.Child.uOffsetSaved        = uOffset;
        pIoCtx->Type.Child.cbTransferLeftSaved = cbTransfer;
        pIoCtx->Type.Child.cbTransferParent    = cbTransferParent;
        pIoCtx->Type.Child.cbWriteParent       = cbWriteParent;
    }

    LogFlow(("Allocated child I/O context %#p\n", pIoCtx));
    return pIoCtx;
}

DECLINLINE(PVDIOTASK) vdIoTaskUserAlloc(PVDIOSTORAGE pIoStorage, PFNVDXFERCOMPLETED pfnComplete, void *pvUser, PVDIOCTX pIoCtx, uint32_t cbTransfer)
{
    PVDIOTASK pIoTask = NULL;

    pIoTask = (PVDIOTASK)RTMemCacheAlloc(pIoStorage->pImage->pDisk->hMemCacheIoTask);
    if (pIoTask)
    {
        pIoTask->pIoStorage           = pIoStorage;
        pIoTask->pfnComplete          = pfnComplete;
        pIoTask->pvUser               = pvUser;
        pIoTask->fMeta                = false;
        pIoTask->Type.User.cbTransfer = cbTransfer;
        pIoTask->Type.User.pIoCtx     = pIoCtx;
    }

    return pIoTask;
}

DECLINLINE(PVDIOTASK) vdIoTaskMetaAlloc(PVDIOSTORAGE pIoStorage, PFNVDXFERCOMPLETED pfnComplete, void *pvUser, PVDMETAXFER pMetaXfer)
{
    PVDIOTASK pIoTask = NULL;

    pIoTask = (PVDIOTASK)RTMemCacheAlloc(pIoStorage->pImage->pDisk->hMemCacheIoTask);
    if (pIoTask)
    {
        pIoTask->pIoStorage          = pIoStorage;
        pIoTask->pfnComplete         = pfnComplete;
        pIoTask->pvUser              = pvUser;
        pIoTask->fMeta               = true;
        pIoTask->Type.Meta.pMetaXfer = pMetaXfer;
    }

    return pIoTask;
}

DECLINLINE(void) vdIoCtxFree(PVBOXHDD pDisk, PVDIOCTX pIoCtx)
{
    LogFlow(("Freeing I/O context %#p\n", pIoCtx));
    if (pIoCtx->pvAllocation)
        RTMemFree(pIoCtx->pvAllocation);
#ifdef DEBUG
    memset(pIoCtx, 0xff, sizeof(VDIOCTX));
#endif
    RTMemCacheFree(pDisk->hMemCacheIoCtx, pIoCtx);
}

DECLINLINE(void) vdIoTaskFree(PVBOXHDD pDisk, PVDIOTASK pIoTask)
{
    RTMemCacheFree(pDisk->hMemCacheIoTask, pIoTask);
}

DECLINLINE(void) vdIoCtxChildReset(PVDIOCTX pIoCtx)
{
    AssertPtr(pIoCtx->pIoCtxParent);

    RTSgBufReset(&pIoCtx->SgBuf);
    pIoCtx->uOffset        = pIoCtx->Type.Child.uOffsetSaved;
    pIoCtx->cbTransferLeft = pIoCtx->Type.Child.cbTransferLeftSaved;
}

DECLINLINE(PVDMETAXFER) vdMetaXferAlloc(PVDIMAGE pImage, PVDIOSTORAGE pIoStorage, uint64_t uOffset, size_t cb)
{
    PVDMETAXFER pMetaXfer = (PVDMETAXFER)RTMemAlloc(RT_OFFSETOF(VDMETAXFER, abData[cb]));

    if (RT_LIKELY(pMetaXfer))
    {
        pMetaXfer->Core.Key     = uOffset;
        pMetaXfer->Core.KeyLast = uOffset + cb - 1;
        pMetaXfer->fFlags       = VDMETAXFER_TXDIR_NONE;
        pMetaXfer->cbMeta       = cb;
        pMetaXfer->pIoStorage   = pIoStorage;
        pMetaXfer->cRefs        = 0;
        RTListInit(&pMetaXfer->ListIoCtxWaiting);
    }
    return pMetaXfer;
}

static size_t vdIoCtxCopy(PVDIOCTX pIoCtxDst, PVDIOCTX pIoCtxSrc, size_t cbData)
{
    return RTSgBufCopy(&pIoCtxDst->SgBuf, &pIoCtxSrc->SgBuf, cbData);
}

static int vdIoCtxCmp(PVDIOCTX pIoCtx1, PVDIOCTX pIoCtx2, size_t cbData)
{
    return RTSgBufCmp(&pIoCtx1->SgBuf, &pIoCtx2->SgBuf, cbData);
}

static size_t vdIoCtxCopyTo(PVDIOCTX pIoCtx, uint8_t *pbData, size_t cbData)
{
    return RTSgBufCopyToBuf(&pIoCtx->SgBuf, pbData, cbData);
}


static size_t vdIoCtxCopyFrom(PVDIOCTX pIoCtx, uint8_t *pbData, size_t cbData)
{
    return RTSgBufCopyFromBuf(&pIoCtx->SgBuf, pbData, cbData);
}

static size_t vdIoCtxSet(PVDIOCTX pIoCtx, uint8_t ch, size_t cbData)
{
    return RTSgBufSet(&pIoCtx->SgBuf, ch, cbData);
}

static int vdIoCtxProcess(PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = pIoCtx->pDisk;

    LogFlowFunc(("pIoCtx=%#p\n", pIoCtx));

    RTCritSectEnter(&pDisk->CritSect);

    if (   !pIoCtx->cbTransferLeft
        && !pIoCtx->cMetaTransfersPending
        && !pIoCtx->cDataTransfersPending
        && !pIoCtx->pfnIoCtxTransfer)
    {
        rc = VINF_VD_ASYNC_IO_FINISHED;
        goto out;
    }

    /*
     * We complete the I/O context in case of an error
     * if there is no I/O task pending.
     */
    if (   RT_FAILURE(pIoCtx->rcReq)
        && !pIoCtx->cMetaTransfersPending
        && !pIoCtx->cDataTransfersPending)
    {
        rc = VINF_VD_ASYNC_IO_FINISHED;
        goto out;
    }

    /* Don't change anything if there is a metadata transfer pending or we are blocked. */
    if (   pIoCtx->cMetaTransfersPending
        || pIoCtx->fBlocked)
    {
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
        goto out;
    }

    if (pIoCtx->pfnIoCtxTransfer)
    {
        /* Call the transfer function advancing to the next while there is no error. */
        while (   pIoCtx->pfnIoCtxTransfer
               && RT_SUCCESS(rc))
        {
            LogFlowFunc(("calling transfer function %#p\n", pIoCtx->pfnIoCtxTransfer));
            rc = pIoCtx->pfnIoCtxTransfer(pIoCtx);

            /* Advance to the next part of the transfer if the current one succeeded. */
            if (RT_SUCCESS(rc))
            {
                pIoCtx->pfnIoCtxTransfer = pIoCtx->pfnIoCtxTransferNext;
                pIoCtx->pfnIoCtxTransferNext = NULL;
            }
        }
    }

    if (   RT_SUCCESS(rc)
        && !pIoCtx->cbTransferLeft
        && !pIoCtx->cMetaTransfersPending
        && !pIoCtx->cDataTransfersPending)
        rc = VINF_VD_ASYNC_IO_FINISHED;
    else if (   RT_SUCCESS(rc)
             || rc == VERR_VD_NOT_ENOUGH_METADATA
             || rc == VERR_VD_IOCTX_HALT)
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
    else if (RT_FAILURE(rc) && (rc != VERR_VD_ASYNC_IO_IN_PROGRESS))
    {
        ASMAtomicCmpXchgS32(&pIoCtx->rcReq, rc, VINF_SUCCESS);
        /*
         * The I/O context completed if we have an error and there is no data
         * or meta data transfer pending.
         */
        if (   !pIoCtx->cMetaTransfersPending
            && !pIoCtx->cDataTransfersPending)
            rc = VINF_VD_ASYNC_IO_FINISHED;
        else
            rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
    }

out:
    RTCritSectLeave(&pDisk->CritSect);

    LogFlowFunc(("pIoCtx=%#p rc=%Rrc cbTransferLeft=%u cMetaTransfersPending=%u fComplete=%RTbool\n",
                 pIoCtx, rc, pIoCtx->cbTransferLeft, pIoCtx->cMetaTransfersPending,
                 pIoCtx->fComplete));

    return rc;
}

/**
 * internal: read the specified amount of data in whatever blocks the backend
 * will give us - async version.
 */
static int vdReadHelperAsync(PVDIOCTX pIoCtx)
{
    int rc;
    size_t cbToRead     = pIoCtx->cbTransfer;
    uint64_t uOffset    = pIoCtx->uOffset;
    PVDIMAGE pCurrImage = NULL;
    size_t cbThisRead;

    /* Loop until all reads started or we have a backend which needs to read metadata. */
    do
    {
        pCurrImage = pIoCtx->pImage;

        /* Search for image with allocated block. Do not attempt to read more
         * than the previous reads marked as valid. Otherwise this would return
         * stale data when different block sizes are used for the images. */
        cbThisRead = cbToRead;

        /*
         * Try to read from the given image.
         * If the block is not allocated read from override chain if present.
         */
        rc = pCurrImage->Backend->pfnAsyncRead(pCurrImage->pBackendData,
                                               uOffset, cbThisRead,
                                               pIoCtx, &cbThisRead);

        if (rc == VERR_VD_BLOCK_FREE)
        {
            for (pCurrImage =  pCurrImage->pPrev;
                 pCurrImage != NULL && rc == VERR_VD_BLOCK_FREE;
                 pCurrImage = pCurrImage->pPrev)
            {
                rc = pCurrImage->Backend->pfnAsyncRead(pCurrImage->pBackendData,
                                                       uOffset, cbThisRead,
                                                       pIoCtx, &cbThisRead);
            }
        }

        /* The task state will be updated on success already, don't do it here!. */
        if (rc == VERR_VD_BLOCK_FREE)
        {
            /* No image in the chain contains the data for the block. */
            vdIoCtxSet(pIoCtx, '\0', cbThisRead);
            ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbThisRead);
            rc = VINF_SUCCESS;
        }
        else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
            rc = VINF_SUCCESS;
        else if (rc == VERR_VD_IOCTX_HALT)
        {
            uOffset  += cbThisRead;
            cbToRead -= cbThisRead;
            pIoCtx->fBlocked = true;
        }

        if (RT_FAILURE(rc))
            break;

        cbToRead -= cbThisRead;
        uOffset  += cbThisRead;
    } while (cbToRead != 0 && RT_SUCCESS(rc));

    if (   rc == VERR_VD_NOT_ENOUGH_METADATA
        || rc == VERR_VD_IOCTX_HALT)
    {
        /* Save the current state. */
        pIoCtx->uOffset    = uOffset;
        pIoCtx->cbTransfer = cbToRead;
        pIoCtx->pImage     = pCurrImage;
    }

    return rc;
}

/**
 * internal: parent image read wrapper for compacting.
 */
static int vdParentRead(void *pvUser, uint64_t uOffset, void *pvBuf,
                        size_t cbRead)
{
    PVDPARENTSTATEDESC pParentState = (PVDPARENTSTATEDESC)pvUser;
    return vdReadHelper(pParentState->pDisk, pParentState->pImage, NULL, uOffset,
                        pvBuf, cbRead, true /* fZeroFreeBlocks */,
                        false /* fUpdateCache */);
}

/**
 * internal: mark the disk as not modified.
 */
static void vdResetModifiedFlag(PVBOXHDD pDisk)
{
    if (pDisk->uModified & VD_IMAGE_MODIFIED_FLAG)
    {
        /* generate new last-modified uuid */
        if (!(pDisk->uModified & VD_IMAGE_MODIFIED_DISABLE_UUID_UPDATE))
        {
            RTUUID Uuid;

            RTUuidCreate(&Uuid);
            pDisk->pLast->Backend->pfnSetModificationUuid(pDisk->pLast->pBackendData,
                                                          &Uuid);

            if (pDisk->pCache)
                pDisk->pCache->Backend->pfnSetModificationUuid(pDisk->pCache->pBackendData,
                                                               &Uuid);
        }

        pDisk->uModified &= ~VD_IMAGE_MODIFIED_FLAG;
    }
}

/**
 * internal: mark the disk as modified.
 */
static void vdSetModifiedFlag(PVBOXHDD pDisk)
{
    pDisk->uModified |= VD_IMAGE_MODIFIED_FLAG;
    if (pDisk->uModified & VD_IMAGE_MODIFIED_FIRST)
    {
        pDisk->uModified &= ~VD_IMAGE_MODIFIED_FIRST;

        /* First modify, so create a UUID and ensure it's written to disk. */
        vdResetModifiedFlag(pDisk);

        if (!(pDisk->uModified & VD_IMAGE_MODIFIED_DISABLE_UUID_UPDATE))
            pDisk->pLast->Backend->pfnFlush(pDisk->pLast->pBackendData);
    }
}

/**
 * internal: write a complete block (only used for diff images), taking the
 * remaining data from parent images. This implementation does not optimize
 * anything (except that it tries to read only that portions from parent
 * images that are really needed).
 */
static int vdWriteHelperStandard(PVBOXHDD pDisk, PVDIMAGE pImage,
                                 PVDIMAGE pImageParentOverride,
                                 uint64_t uOffset, size_t cbWrite,
                                 size_t cbThisWrite, size_t cbPreRead,
                                 size_t cbPostRead, const void *pvBuf,
                                 void *pvTmp)
{
    int rc = VINF_SUCCESS;

    /* Read the data that goes before the write to fill the block. */
    if (cbPreRead)
    {
        /*
         * Updating the cache doesn't make sense here because
         * this will be done after the complete block was written.
         */
        rc = vdReadHelper(pDisk, pImage, pImageParentOverride,
                          uOffset - cbPreRead, pvTmp, cbPreRead,
                          true /* fZeroFreeBlocks*/,
                          false /* fUpdateCache */);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Copy the data to the right place in the buffer. */
    memcpy((char *)pvTmp + cbPreRead, pvBuf, cbThisWrite);

    /* Read the data that goes after the write to fill the block. */
    if (cbPostRead)
    {
        /* If we have data to be written, use that instead of reading
         * data from the image. */
        size_t cbWriteCopy;
        if (cbWrite > cbThisWrite)
            cbWriteCopy = RT_MIN(cbWrite - cbThisWrite, cbPostRead);
        else
            cbWriteCopy = 0;
        /* Figure out how much we cannnot read from the image, because
         * the last block to write might exceed the nominal size of the
         * image for technical reasons. */
        size_t cbFill;
        if (uOffset + cbThisWrite + cbPostRead > pDisk->cbSize)
            cbFill = uOffset + cbThisWrite + cbPostRead - pDisk->cbSize;
        else
            cbFill = 0;
        /* The rest must be read from the image. */
        size_t cbReadImage = cbPostRead - cbWriteCopy - cbFill;

        /* Now assemble the remaining data. */
        if (cbWriteCopy)
            memcpy((char *)pvTmp + cbPreRead + cbThisWrite,
                   (char *)pvBuf + cbThisWrite, cbWriteCopy);
        if (cbReadImage)
            rc = vdReadHelper(pDisk, pImage, pImageParentOverride,
                              uOffset + cbThisWrite + cbWriteCopy,
                              (char *)pvTmp + cbPreRead + cbThisWrite + cbWriteCopy,
                              cbReadImage, true /* fZeroFreeBlocks */,
                              false /* fUpdateCache */);
        if (RT_FAILURE(rc))
            return rc;
        /* Zero out the remainder of this block. Will never be visible, as this
         * is beyond the limit of the image. */
        if (cbFill)
            memset((char *)pvTmp + cbPreRead + cbThisWrite + cbWriteCopy + cbReadImage,
                   '\0', cbFill);
    }

    /* Write the full block to the virtual disk. */
    rc = pImage->Backend->pfnWrite(pImage->pBackendData,
                                   uOffset - cbPreRead, pvTmp,
                                   cbPreRead + cbThisWrite + cbPostRead,
                                   NULL, &cbPreRead, &cbPostRead, 0);
    Assert(rc != VERR_VD_BLOCK_FREE);
    Assert(cbPreRead == 0);
    Assert(cbPostRead == 0);

    return rc;
}

/**
 * internal: write a complete block (only used for diff images), taking the
 * remaining data from parent images. This implementation optimizes out writes
 * that do not change the data relative to the state as of the parent images.
 * All backends which support differential/growing images support this.
 */
static int vdWriteHelperOptimized(PVBOXHDD pDisk, PVDIMAGE pImage,
                                  PVDIMAGE pImageParentOverride,
                                  uint64_t uOffset, size_t cbWrite,
                                  size_t cbThisWrite, size_t cbPreRead,
                                  size_t cbPostRead, const void *pvBuf,
                                  void *pvTmp)
{
    size_t cbFill = 0;
    size_t cbWriteCopy = 0;
    size_t cbReadImage = 0;
    int rc;

    if (cbPostRead)
    {
        /* Figure out how much we cannnot read from the image, because
         * the last block to write might exceed the nominal size of the
         * image for technical reasons. */
        if (uOffset + cbThisWrite + cbPostRead > pDisk->cbSize)
            cbFill = uOffset + cbThisWrite + cbPostRead - pDisk->cbSize;

        /* If we have data to be written, use that instead of reading
         * data from the image. */
        if (cbWrite > cbThisWrite)
            cbWriteCopy = RT_MIN(cbWrite - cbThisWrite, cbPostRead);

        /* The rest must be read from the image. */
        cbReadImage = cbPostRead - cbWriteCopy - cbFill;
    }

    /* Read the entire data of the block so that we can compare whether it will
     * be modified by the write or not. */
    rc = vdReadHelper(pDisk, pImage, pImageParentOverride, uOffset - cbPreRead, pvTmp,
                      cbPreRead + cbThisWrite + cbPostRead - cbFill,
                      true /* fZeroFreeBlocks */,
                      false /* fUpdateCache */);
    if (RT_FAILURE(rc))
        return rc;

    /* Check if the write would modify anything in this block. */
    if (   !memcmp((char *)pvTmp + cbPreRead, pvBuf, cbThisWrite)
        && (!cbWriteCopy || !memcmp((char *)pvTmp + cbPreRead + cbThisWrite,
                                    (char *)pvBuf + cbThisWrite, cbWriteCopy)))
    {
        /* Block is completely unchanged, so no need to write anything. */
        return VINF_SUCCESS;
    }

    /* Copy the data to the right place in the buffer. */
    memcpy((char *)pvTmp + cbPreRead, pvBuf, cbThisWrite);

    /* Handle the data that goes after the write to fill the block. */
    if (cbPostRead)
    {
        /* Now assemble the remaining data. */
        if (cbWriteCopy)
            memcpy((char *)pvTmp + cbPreRead + cbThisWrite,
                   (char *)pvBuf + cbThisWrite, cbWriteCopy);
        /* Zero out the remainder of this block. Will never be visible, as this
         * is beyond the limit of the image. */
        if (cbFill)
            memset((char *)pvTmp + cbPreRead + cbThisWrite + cbWriteCopy + cbReadImage,
                   '\0', cbFill);
    }

    /* Write the full block to the virtual disk. */
    rc = pImage->Backend->pfnWrite(pImage->pBackendData,
                                   uOffset - cbPreRead, pvTmp,
                                   cbPreRead + cbThisWrite + cbPostRead,
                                   NULL, &cbPreRead, &cbPostRead, 0);
    Assert(rc != VERR_VD_BLOCK_FREE);
    Assert(cbPreRead == 0);
    Assert(cbPostRead == 0);

    return rc;
}

/**
 * internal: write buffer to the image, taking care of block boundaries and
 * write optimizations.
 */
static int vdWriteHelper(PVBOXHDD pDisk, PVDIMAGE pImage, PVDIMAGE pImageParentOverride,
                         uint64_t uOffset, const void *pvBuf, size_t cbWrite,
                         bool fUpdateCache)
{
    int rc;
    unsigned fWrite;
    size_t cbThisWrite;
    size_t cbPreRead, cbPostRead;
    uint64_t uOffsetCur = uOffset;
    size_t cbWriteCur = cbWrite;
    const void *pcvBufCur = pvBuf;

    /* Loop until all written. */
    do
    {
        /* Try to write the possibly partial block to the last opened image.
         * This works when the block is already allocated in this image or
         * if it is a full-block write (and allocation isn't suppressed below).
         * For image formats which don't support zero blocks, it's beneficial
         * to avoid unnecessarily allocating unchanged blocks. This prevents
         * unwanted expanding of images. VMDK is an example. */
        cbThisWrite = cbWriteCur;
        fWrite =   (pImage->uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME)
                 ? 0 : VD_WRITE_NO_ALLOC;
        rc = pImage->Backend->pfnWrite(pImage->pBackendData, uOffsetCur, pcvBufCur,
                                       cbThisWrite, &cbThisWrite, &cbPreRead,
                                       &cbPostRead, fWrite);
        if (rc == VERR_VD_BLOCK_FREE)
        {
            void *pvTmp = RTMemTmpAlloc(cbPreRead + cbThisWrite + cbPostRead);
            AssertBreakStmt(VALID_PTR(pvTmp), rc = VERR_NO_MEMORY);

            if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME))
            {
                /* Optimized write, suppress writing to a so far unallocated
                 * block if the data is in fact not changed. */
                rc = vdWriteHelperOptimized(pDisk, pImage, pImageParentOverride,
                                            uOffsetCur, cbWriteCur,
                                            cbThisWrite, cbPreRead, cbPostRead,
                                            pcvBufCur, pvTmp);
            }
            else
            {
                /* Normal write, not optimized in any way. The block will
                 * be written no matter what. This will usually (unless the
                 * backend has some further optimization enabled) cause the
                 * block to be allocated. */
                rc = vdWriteHelperStandard(pDisk, pImage, pImageParentOverride,
                                           uOffsetCur, cbWriteCur,
                                           cbThisWrite, cbPreRead, cbPostRead,
                                           pcvBufCur, pvTmp);
            }
            RTMemTmpFree(pvTmp);
            if (RT_FAILURE(rc))
                break;
        }

        cbWriteCur -= cbThisWrite;
        uOffsetCur += cbThisWrite;
        pcvBufCur = (char *)pcvBufCur + cbThisWrite;
    } while (cbWriteCur != 0 && RT_SUCCESS(rc));

    /* Update the cache on success */
    if (   RT_SUCCESS(rc)
        && pDisk->pCache
        && fUpdateCache)
        rc = vdCacheWriteHelper(pDisk->pCache, uOffset, pvBuf, cbWrite, NULL);

    return rc;
}

/**
 * internal: write a complete block (only used for diff images), taking the
 * remaining data from parent images. This implementation does not optimize
 * anything (except that it tries to read only that portions from parent
 * images that are really needed) - async version.
 */
static int vdWriteHelperStandardAsync(PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;

#if 0

    /* Read the data that goes before the write to fill the block. */
    if (cbPreRead)
    {
        rc = vdReadHelperAsync(pIoCtxDst);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Copy the data to the right place in the buffer. */
    vdIoCtxCopy(pIoCtxDst, pIoCtxSrc, cbThisWrite);

    /* Read the data that goes after the write to fill the block. */
    if (cbPostRead)
    {
        /* If we have data to be written, use that instead of reading
         * data from the image. */
        size_t cbWriteCopy;
        if (cbWrite > cbThisWrite)
            cbWriteCopy = RT_MIN(cbWrite - cbThisWrite, cbPostRead);
        else
            cbWriteCopy = 0;
        /* Figure out how much we cannnot read from the image, because
         * the last block to write might exceed the nominal size of the
         * image for technical reasons. */
        size_t cbFill;
        if (uOffset + cbThisWrite + cbPostRead > pDisk->cbSize)
            cbFill = uOffset + cbThisWrite + cbPostRead - pDisk->cbSize;
        else
            cbFill = 0;
        /* The rest must be read from the image. */
        size_t cbReadImage = cbPostRead - cbWriteCopy - cbFill;

        /* Now assemble the remaining data. */
        if (cbWriteCopy)
        {
            vdIoCtxCopy(pIoCtxDst, pIoCtxSrc, cbWriteCopy);
            ASMAtomicSubU32(&pIoCtxDst->cbTransferLeft, cbWriteCopy);
        }

        if (cbReadImage)
            rc = vdReadHelperAsync(pDisk, pImage, pImageParentOverride, pIoCtxDst,
                                   uOffset + cbThisWrite + cbWriteCopy,
                                   cbReadImage);
        if (RT_FAILURE(rc))
            return rc;
        /* Zero out the remainder of this block. Will never be visible, as this
         * is beyond the limit of the image. */
        if (cbFill)
        {
            vdIoCtxSet(pIoCtxDst, '\0', cbFill);
            ASMAtomicSubU32(&pIoCtxDst->cbTransferLeft, cbFill);
        }
    }

    if (   !pIoCtxDst->cbTransferLeft
        && !pIoCtxDst->cMetaTransfersPending
        && ASMAtomicCmpXchgBool(&pIoCtxDst->fComplete, true, false))
    {
        /* Write the full block to the virtual disk. */
        vdIoCtxChildReset(pIoCtxDst);
        rc = pImage->Backend->pfnAsyncWrite(pImage->pBackendData,
                                            uOffset - cbPreRead,
                                            cbPreRead + cbThisWrite + cbPostRead,
                                            pIoCtxDst,
                                            NULL, &cbPreRead, &cbPostRead, 0);
        Assert(rc != VERR_VD_BLOCK_FREE);
        Assert(cbPreRead == 0);
        Assert(cbPostRead == 0);
    }
    else
    {
        LogFlow(("cbTransferLeft=%u cMetaTransfersPending=%u fComplete=%RTbool\n",
                 pIoCtxDst->cbTransferLeft, pIoCtxDst->cMetaTransfersPending,
                 pIoCtxDst->fComplete));
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
    }

    return rc;
#endif
    return VERR_NOT_IMPLEMENTED;
}

static int vdWriteHelperOptimizedCmpAndWriteAsync(PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pIoCtx->pImage;
    size_t cbThisWrite    = 0;
    size_t cbPreRead      = pIoCtx->Type.Child.cbPreRead;
    size_t cbPostRead     = pIoCtx->Type.Child.cbPostRead;
    size_t cbWriteCopy    = pIoCtx->Type.Child.Write.Optimized.cbWriteCopy;
    size_t cbFill         = pIoCtx->Type.Child.Write.Optimized.cbFill;
    size_t cbReadImage    = pIoCtx->Type.Child.Write.Optimized.cbReadImage;
    PVDIOCTX pIoCtxParent = pIoCtx->pIoCtxParent;

    LogFlowFunc(("pIoCtx=%#p\n", pIoCtx));

    AssertPtr(pIoCtxParent);
    Assert(!pIoCtxParent->pIoCtxParent);
    Assert(!pIoCtx->cbTransferLeft && !pIoCtx->cMetaTransfersPending);

    vdIoCtxChildReset(pIoCtx);
    cbThisWrite = pIoCtx->Type.Child.cbTransferParent;
    RTSgBufAdvance(&pIoCtx->SgBuf, cbPreRead);

    /* Check if the write would modify anything in this block. */
    if (!RTSgBufCmp(&pIoCtx->SgBuf, &pIoCtxParent->SgBuf, cbThisWrite))
    {
        RTSGBUF SgBufSrcTmp;

        RTSgBufClone(&SgBufSrcTmp, &pIoCtxParent->SgBuf);
        RTSgBufAdvance(&SgBufSrcTmp, cbThisWrite);
        RTSgBufAdvance(&pIoCtx->SgBuf, cbThisWrite);

        if (!cbWriteCopy || !RTSgBufCmp(&pIoCtx->SgBuf, &SgBufSrcTmp, cbWriteCopy))
        {
            /* Block is completely unchanged, so no need to write anything. */
            LogFlowFunc(("Block didn't changed\n"));
            ASMAtomicWriteU32(&pIoCtx->cbTransferLeft, 0);
            RTSgBufAdvance(&pIoCtxParent->SgBuf, cbThisWrite);
            return VINF_VD_ASYNC_IO_FINISHED;
        }
    }

    /* Copy the data to the right place in the buffer. */
    RTSgBufReset(&pIoCtx->SgBuf);
    RTSgBufAdvance(&pIoCtx->SgBuf, cbPreRead);
    vdIoCtxCopy(pIoCtx, pIoCtxParent, cbThisWrite);

    /* Handle the data that goes after the write to fill the block. */
    if (cbPostRead)
    {
        /* Now assemble the remaining data. */
        if (cbWriteCopy)
        {
            /*
             * The S/G buffer of the parent needs to be cloned because
             * it is not allowed to modify the state.
             */
            RTSGBUF SgBufParentTmp;

            RTSgBufClone(&SgBufParentTmp, &pIoCtxParent->SgBuf);
            RTSgBufCopy(&pIoCtx->SgBuf, &SgBufParentTmp, cbWriteCopy);
        }

        /* Zero out the remainder of this block. Will never be visible, as this
         * is beyond the limit of the image. */
        if (cbFill)
        {
            RTSgBufAdvance(&pIoCtx->SgBuf, cbReadImage);
            vdIoCtxSet(pIoCtx, '\0', cbFill);
        }
    }

    /* Write the full block to the virtual disk. */
    RTSgBufReset(&pIoCtx->SgBuf);
    rc = pImage->Backend->pfnAsyncWrite(pImage->pBackendData,
                                        pIoCtx->uOffset - cbPreRead,
                                        cbPreRead + cbThisWrite + cbPostRead,
                                        pIoCtx, NULL, &cbPreRead, &cbPostRead, 0);
    Assert(rc != VERR_VD_BLOCK_FREE);
    Assert(cbPreRead == 0);
    Assert(cbPostRead == 0);
    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        rc = VINF_SUCCESS;
    else if (rc == VERR_VD_IOCTX_HALT)
    {
        pIoCtx->fBlocked = true;
        rc = VINF_SUCCESS;
    }

    return rc;
}

static int vdWriteHelperOptimizedPreReadAsync(PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pIoCtx=%#p\n", pIoCtx));

    if (pIoCtx->cbTransferLeft)
        rc = vdReadHelperAsync(pIoCtx);

    if (   RT_SUCCESS(rc)
        && (   pIoCtx->cbTransferLeft
            || pIoCtx->cMetaTransfersPending))
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
     else
        pIoCtx->pfnIoCtxTransferNext = vdWriteHelperOptimizedCmpAndWriteAsync;

    return rc;
}

/**
 * internal: write a complete block (only used for diff images), taking the
 * remaining data from parent images. This implementation optimizes out writes
 * that do not change the data relative to the state as of the parent images.
 * All backends which support differential/growing images support this - async version.
 */
static int vdWriteHelperOptimizedAsync(PVDIOCTX pIoCtx)
{
    PVBOXHDD pDisk = pIoCtx->pDisk;
    uint64_t uOffset   = pIoCtx->Type.Child.uOffsetSaved;
    size_t cbThisWrite = pIoCtx->Type.Child.cbTransferParent;
    size_t cbPreRead   = pIoCtx->Type.Child.cbPreRead;
    size_t cbPostRead  = pIoCtx->Type.Child.cbPostRead;
    size_t cbWrite     = pIoCtx->Type.Child.cbWriteParent;
    size_t cbFill = 0;
    size_t cbWriteCopy = 0;
    size_t cbReadImage = 0;

    LogFlowFunc(("pIoCtx=%#p\n", pIoCtx));

    AssertPtr(pIoCtx->pIoCtxParent);
    Assert(!pIoCtx->pIoCtxParent->pIoCtxParent);

    if (cbPostRead)
    {
        /* Figure out how much we cannnot read from the image, because
         * the last block to write might exceed the nominal size of the
         * image for technical reasons. */
        if (uOffset + cbThisWrite + cbPostRead > pDisk->cbSize)
            cbFill = uOffset + cbThisWrite + cbPostRead - pDisk->cbSize;

        /* If we have data to be written, use that instead of reading
         * data from the image. */
        if (cbWrite > cbThisWrite)
            cbWriteCopy = RT_MIN(cbWrite - cbThisWrite, cbPostRead);

        /* The rest must be read from the image. */
        cbReadImage = cbPostRead - cbWriteCopy - cbFill;
    }

    pIoCtx->Type.Child.Write.Optimized.cbFill      = cbFill;
    pIoCtx->Type.Child.Write.Optimized.cbWriteCopy = cbWriteCopy;
    pIoCtx->Type.Child.Write.Optimized.cbReadImage = cbReadImage;

    /* Read the entire data of the block so that we can compare whether it will
     * be modified by the write or not. */
    pIoCtx->cbTransferLeft = cbPreRead + cbThisWrite + cbPostRead - cbFill;
    pIoCtx->cbTransfer     = pIoCtx->cbTransferLeft;
    pIoCtx->uOffset       -= cbPreRead;

    /* Next step */
    pIoCtx->pfnIoCtxTransferNext = vdWriteHelperOptimizedPreReadAsync;
    return VINF_SUCCESS;
}

/**
 * internal: write buffer to the image, taking care of block boundaries and
 * write optimizations - async version.
 */
static int vdWriteHelperAsync(PVDIOCTX pIoCtx)
{
    int rc;
    size_t cbWrite   = pIoCtx->cbTransfer;
    uint64_t uOffset = pIoCtx->uOffset;
    PVDIMAGE pImage  = pIoCtx->pImage;
    PVBOXHDD pDisk   = pIoCtx->pDisk;
    unsigned fWrite;
    size_t cbThisWrite;
    size_t cbPreRead, cbPostRead;

    /* Loop until all written. */
    do
    {
        /* Try to write the possibly partial block to the last opened image.
         * This works when the block is already allocated in this image or
         * if it is a full-block write (and allocation isn't suppressed below).
         * For image formats which don't support zero blocks, it's beneficial
         * to avoid unnecessarily allocating unchanged blocks. This prevents
         * unwanted expanding of images. VMDK is an example. */
        cbThisWrite = cbWrite;
        fWrite =   (pImage->uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME)
                 ? 0 : VD_WRITE_NO_ALLOC;
        rc = pImage->Backend->pfnAsyncWrite(pImage->pBackendData, uOffset,
                                            cbThisWrite, pIoCtx,
                                            &cbThisWrite, &cbPreRead,
                                            &cbPostRead, fWrite);
        if (rc == VERR_VD_BLOCK_FREE)
        {
            /*
             * If there is a growing request already put this one onto the waiting list.
             * It will be restarted if the current request completes.
             */
            if (ASMAtomicReadBool(&pDisk->fGrowing))
            {
                PVDIOCTXDEFERRED pDeferred = (PVDIOCTXDEFERRED)RTMemAllocZ(sizeof(VDIOCTXDEFERRED));
                AssertPtr(pDeferred);

                LogFlowFunc(("Deferring write pIoCtx=%#p\n", pIoCtx));

                Assert(!pIoCtx->pIoCtxParent && !pIoCtx->fBlocked);

                RTListInit(&pDeferred->NodeDeferred);
                pDeferred->pIoCtx = pIoCtx;
                RTListAppend(&pDisk->ListWriteGrowing, &pDeferred->NodeDeferred);
                pIoCtx->fBlocked = true;
                rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
                break;
            }
            else
            {
                /*
                 * Allocate segment and buffer in one go.
                 * A bit hackish but avoids the need to allocate memory twice.
                 */
                PRTSGSEG pTmp = (PRTSGSEG)RTMemAlloc(cbPreRead + cbThisWrite + cbPostRead + sizeof(RTSGSEG));
                AssertBreakStmt(VALID_PTR(pTmp), rc = VERR_NO_MEMORY);

                pTmp->pvSeg = pTmp + 1;
                pTmp->cbSeg = cbPreRead + cbThisWrite + cbPostRead;

                PVDIOCTX pIoCtxWrite = vdIoCtxChildAlloc(pDisk, VDIOCTXTXDIR_WRITE,
                                                         uOffset, pTmp->cbSeg,
                                                         pTmp, 1,
                                                         pIoCtx, cbThisWrite,
                                                         cbWrite,
                                                         pTmp,
                                                           (pImage->uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME)
                                                         ? vdWriteHelperStandardAsync
                                                         : vdWriteHelperOptimizedAsync);
                if (!VALID_PTR(pIoCtxWrite))
                {
                    RTMemTmpFree(pTmp);
                    rc = VERR_NO_MEMORY;
                    break;
                }

                /* Set the state to growing. */
                LogFlowFunc(("Disk is growing because of pIoCtx=%#p pIoCtxWrite=%#p\n",

                             pIoCtx, pIoCtxWrite));
                ASMAtomicWriteBool(&pDisk->fGrowing, true);

                pIoCtxWrite->pImage                = pImage;
                pIoCtxWrite->Type.Child.cbPreRead  = cbPreRead;
                pIoCtxWrite->Type.Child.cbPostRead = cbPostRead;

                /* Process the write request */
                rc = vdIoCtxProcess(pIoCtxWrite);

                if (RT_FAILURE(rc) && (rc != VERR_VD_ASYNC_IO_IN_PROGRESS))
                {
                    vdIoCtxFree(pDisk, pIoCtxWrite);
                    break;
                }
                else if (   rc == VINF_VD_ASYNC_IO_FINISHED
                         && ASMAtomicCmpXchgBool(&pIoCtxWrite->fComplete, true, false))
                {
                    LogFlow(("Child write request completed\n"));
                    Assert(pIoCtx->cbTransferLeft >= cbThisWrite);
                    ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbThisWrite);
                    ASMAtomicWriteBool(&pDisk->fGrowing, false);
                    vdIoCtxFree(pDisk, pIoCtxWrite);

                    rc = VINF_SUCCESS;
                }
                else
                {
                    LogFlow(("Child write pending\n"));
                    pIoCtx->fBlocked = true;
                    rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
                    cbWrite -= cbThisWrite;
                    uOffset += cbThisWrite;
                    break;
                }
            }
        }

        if (rc == VERR_VD_IOCTX_HALT)
        {
            cbWrite -= cbThisWrite;
            uOffset += cbThisWrite;
            pIoCtx->fBlocked = true;
            break;
        }
        else if (rc == VERR_VD_NOT_ENOUGH_METADATA)
            break;

        cbWrite -= cbThisWrite;
        uOffset += cbThisWrite;
    } while (cbWrite != 0 && (RT_SUCCESS(rc) || rc == VERR_VD_ASYNC_IO_IN_PROGRESS));

    if (   rc == VERR_VD_ASYNC_IO_IN_PROGRESS
        || rc == VERR_VD_NOT_ENOUGH_METADATA
        || rc == VERR_VD_IOCTX_HALT)
    {
        /*
         * Tell the caller that we don't need to go back here because all
         * writes are initiated.
         */
        if (!cbWrite)
            rc = VINF_SUCCESS;

        pIoCtx->uOffset    = uOffset;
        pIoCtx->cbTransfer = cbWrite;
    }

    return rc;
}

/**
 * Flush helper async version.
 */
static int vdFlushHelperAsync(PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = pIoCtx->pDisk;
    PVDIMAGE pImage = pIoCtx->pImage;

    vdResetModifiedFlag(pDisk);
    rc = pImage->Backend->pfnAsyncFlush(pImage->pBackendData, pIoCtx);
    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * internal: scans plugin directory and loads the backends have been found.
 */
static int vdLoadDynamicBackends()
{
#ifndef VBOX_HDD_NO_DYNAMIC_BACKENDS
    int rc = VINF_SUCCESS;
    PRTDIR pPluginDir = NULL;

    /* Enumerate plugin backends. */
    char szPath[RTPATH_MAX];
    rc = RTPathAppPrivateArch(szPath, sizeof(szPath));
    if (RT_FAILURE(rc))
        return rc;

    /* To get all entries with VBoxHDD as prefix. */
    char *pszPluginFilter;
    rc = RTStrAPrintf(&pszPluginFilter, "%s/%s*", szPath, VBOX_HDDFORMAT_PLUGIN_PREFIX);
    if (RT_FAILURE(rc))
    {
        rc = VERR_NO_MEMORY;
        return rc;
    }

    PRTDIRENTRYEX pPluginDirEntry = NULL;
    size_t cbPluginDirEntry = sizeof(RTDIRENTRYEX);
    /* The plugins are in the same directory as the other shared libs. */
    rc = RTDirOpenFiltered(&pPluginDir, pszPluginFilter, RTDIRFILTER_WINNT);
    if (RT_FAILURE(rc))
    {
        /* On Windows the above immediately signals that there are no
         * files matching, while on other platforms enumerating the
         * files below fails. Either way: no plugins. */
        goto out;
    }

    pPluginDirEntry = (PRTDIRENTRYEX)RTMemAllocZ(sizeof(RTDIRENTRYEX));
    if (!pPluginDirEntry)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    while ((rc = RTDirReadEx(pPluginDir, pPluginDirEntry, &cbPluginDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK)) != VERR_NO_MORE_FILES)
    {
        RTLDRMOD hPlugin = NIL_RTLDRMOD;
        PFNVBOXHDDFORMATLOAD pfnHDDFormatLoad = NULL;
        PVBOXHDDBACKEND pBackend = NULL;
        char *pszPluginPath = NULL;

        if (rc == VERR_BUFFER_OVERFLOW)
        {
            /* allocate new buffer. */
            RTMemFree(pPluginDirEntry);
            pPluginDirEntry = (PRTDIRENTRYEX)RTMemAllocZ(cbPluginDirEntry);
            /* Retry. */
            rc = RTDirReadEx(pPluginDir, pPluginDirEntry, &cbPluginDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
            if (RT_FAILURE(rc))
                break;
        }
        else if (RT_FAILURE(rc))
            break;

        /* We got the new entry. */
        if (!RTFS_IS_FILE(pPluginDirEntry->Info.Attr.fMode))
            continue;

        /* Prepend the path to the libraries. */
        rc = RTStrAPrintf(&pszPluginPath, "%s/%s", szPath, pPluginDirEntry->szName);
        if (RT_FAILURE(rc))
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = SUPR3HardenedLdrLoad(pszPluginPath, &hPlugin);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(hPlugin, VBOX_HDDFORMAT_LOAD_NAME, (void**)&pfnHDDFormatLoad);
            if (RT_FAILURE(rc) || !pfnHDDFormatLoad)
            {
                LogFunc(("error resolving the entry point %s in plugin %s, rc=%Rrc, pfnHDDFormat=%#p\n", VBOX_HDDFORMAT_LOAD_NAME, pPluginDirEntry->szName, rc, pfnHDDFormatLoad));
                if (RT_SUCCESS(rc))
                    rc = VERR_SYMBOL_NOT_FOUND;
            }

            if (RT_SUCCESS(rc))
            {
                /* Get the function table. */
                rc = pfnHDDFormatLoad(&pBackend);
                if (RT_SUCCESS(rc) && pBackend->cbSize == sizeof(VBOXHDDBACKEND))
                {
                    pBackend->hPlugin = hPlugin;
                    vdAddBackend(pBackend);
                }
                else
                    LogFunc(("ignored plugin '%s': pBackend->cbSize=%d rc=%Rrc\n", pszPluginPath, pBackend->cbSize, rc));
            }
            else
                LogFunc(("ignored plugin '%s': rc=%Rrc\n", pszPluginPath, rc));

            if (RT_FAILURE(rc))
                RTLdrClose(hPlugin);
        }
        RTStrFree(pszPluginPath);
    }
out:
    if (rc == VERR_NO_MORE_FILES)
        rc = VINF_SUCCESS;
    RTStrFree(pszPluginFilter);
    if (pPluginDirEntry)
        RTMemFree(pPluginDirEntry);
    if (pPluginDir)
        RTDirClose(pPluginDir);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}

/**
 * internal: scans plugin directory and loads the cache backends have been found.
 */
static int vdLoadDynamicCacheBackends()
{
#ifndef VBOX_HDD_NO_DYNAMIC_BACKENDS
    int rc = VINF_SUCCESS;
    PRTDIR pPluginDir = NULL;

    /* Enumerate plugin backends. */
    char szPath[RTPATH_MAX];
    rc = RTPathAppPrivateArch(szPath, sizeof(szPath));
    if (RT_FAILURE(rc))
        return rc;

    /* To get all entries with VBoxHDD as prefix. */
    char *pszPluginFilter;
    rc = RTStrAPrintf(&pszPluginFilter, "%s/%s*", szPath, VD_CACHEFORMAT_PLUGIN_PREFIX);
    if (RT_FAILURE(rc))
    {
        rc = VERR_NO_MEMORY;
        return rc;
    }

    PRTDIRENTRYEX pPluginDirEntry = NULL;
    size_t cbPluginDirEntry = sizeof(RTDIRENTRYEX);
    /* The plugins are in the same directory as the other shared libs. */
    rc = RTDirOpenFiltered(&pPluginDir, pszPluginFilter, RTDIRFILTER_WINNT);
    if (RT_FAILURE(rc))
    {
        /* On Windows the above immediately signals that there are no
         * files matching, while on other platforms enumerating the
         * files below fails. Either way: no plugins. */
        goto out;
    }

    pPluginDirEntry = (PRTDIRENTRYEX)RTMemAllocZ(sizeof(RTDIRENTRYEX));
    if (!pPluginDirEntry)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    while ((rc = RTDirReadEx(pPluginDir, pPluginDirEntry, &cbPluginDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK)) != VERR_NO_MORE_FILES)
    {
        RTLDRMOD hPlugin = NIL_RTLDRMOD;
        PFNVDCACHEFORMATLOAD pfnVDCacheLoad = NULL;
        PVDCACHEBACKEND pBackend = NULL;
        char *pszPluginPath = NULL;

        if (rc == VERR_BUFFER_OVERFLOW)
        {
            /* allocate new buffer. */
            RTMemFree(pPluginDirEntry);
            pPluginDirEntry = (PRTDIRENTRYEX)RTMemAllocZ(cbPluginDirEntry);
            /* Retry. */
            rc = RTDirReadEx(pPluginDir, pPluginDirEntry, &cbPluginDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
            if (RT_FAILURE(rc))
                break;
        }
        else if (RT_FAILURE(rc))
            break;

        /* We got the new entry. */
        if (!RTFS_IS_FILE(pPluginDirEntry->Info.Attr.fMode))
            continue;

        /* Prepend the path to the libraries. */
        rc = RTStrAPrintf(&pszPluginPath, "%s/%s", szPath, pPluginDirEntry->szName);
        if (RT_FAILURE(rc))
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = SUPR3HardenedLdrLoad(pszPluginPath, &hPlugin);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(hPlugin, VD_CACHEFORMAT_LOAD_NAME, (void**)&pfnVDCacheLoad);
            if (RT_FAILURE(rc) || !pfnVDCacheLoad)
            {
                LogFunc(("error resolving the entry point %s in plugin %s, rc=%Rrc, pfnVDCacheLoad=%#p\n",
                         VD_CACHEFORMAT_LOAD_NAME, pPluginDirEntry->szName, rc, pfnVDCacheLoad));
                if (RT_SUCCESS(rc))
                    rc = VERR_SYMBOL_NOT_FOUND;
            }

            if (RT_SUCCESS(rc))
            {
                /* Get the function table. */
                rc = pfnVDCacheLoad(&pBackend);
                if (RT_SUCCESS(rc) && pBackend->cbSize == sizeof(VDCACHEBACKEND))
                {
                    pBackend->hPlugin = hPlugin;
                    vdAddCacheBackend(pBackend);
                }
                else
                    LogFunc(("ignored plugin '%s': pBackend->cbSize=%d rc=%Rrc\n", pszPluginPath, pBackend->cbSize, rc));
            }
            else
                LogFunc(("ignored plugin '%s': rc=%Rrc\n", pszPluginPath, rc));

            if (RT_FAILURE(rc))
                RTLdrClose(hPlugin);
        }
        RTStrFree(pszPluginPath);
    }
out:
    if (rc == VERR_NO_MORE_FILES)
        rc = VINF_SUCCESS;
    RTStrFree(pszPluginFilter);
    if (pPluginDirEntry)
        RTMemFree(pPluginDirEntry);
    if (pPluginDir)
        RTDirClose(pPluginDir);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}

/**
 * VD async I/O interface open callback.
 */
static int vdIOOpenFallback(void *pvUser, const char *pszLocation, uint32_t fOpen,
                            PFNVDCOMPLETED pfnCompleted, PVDINTERFACE pVDIfsDisk,
                            void **ppStorage)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)RTMemAllocZ(sizeof(VDIIOFALLBACKSTORAGE));

    if (!pStorage)
        return VERR_NO_MEMORY;

    pStorage->pfnCompleted = pfnCompleted;

    /* Open the file. */
    int rc = RTFileOpen(&pStorage->File, pszLocation, fOpen);
    if (RT_SUCCESS(rc))
    {
        *ppStorage = pStorage;
        return VINF_SUCCESS;
    }

    RTMemFree(pStorage);
    return rc;
}

/**
 * VD async I/O interface close callback.
 */
static int vdIOCloseFallback(void *pvUser, void *pvStorage)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)pvStorage;

    RTFileClose(pStorage->File);
    RTMemFree(pStorage);
    return VINF_SUCCESS;
}

static int vdIODeleteFallback(void *pvUser, const char *pcszFilename)
{
    return RTFileDelete(pcszFilename);
}

static int vdIOMoveFallback(void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove)
{
    return RTFileMove(pcszSrc, pcszDst, fMove);
}

static int vdIOGetFreeSpaceFallback(void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    return RTFsQuerySizes(pcszFilename, NULL, pcbFreeSpace, NULL, NULL);
}

static int vdIOGetModificationTimeFallback(void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    RTFSOBJINFO info;
    int rc = RTPathQueryInfo(pcszFilename, &info, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
        *pModificationTime = info.ModificationTime;
    return rc;
}

/**
 * VD async I/O interface callback for retrieving the file size.
 */
static int vdIOGetSizeFallback(void *pvUser, void *pvStorage, uint64_t *pcbSize)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)pvStorage;

    return RTFileGetSize(pStorage->File, pcbSize);
}

/**
 * VD async I/O interface callback for setting the file size.
 */
static int vdIOSetSizeFallback(void *pvUser, void *pvStorage, uint64_t cbSize)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)pvStorage;

    return RTFileSetSize(pStorage->File, cbSize);
}

/**
 * VD async I/O interface callback for a synchronous write to the file.
 */
static int vdIOWriteSyncFallback(void *pvUser, void *pvStorage, uint64_t uOffset,
                              const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)pvStorage;

    return RTFileWriteAt(pStorage->File, uOffset, pvBuf, cbWrite, pcbWritten);
}

/**
 * VD async I/O interface callback for a synchronous read from the file.
 */
static int vdIOReadSyncFallback(void *pvUser, void *pvStorage, uint64_t uOffset,
                             void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)pvStorage;

    return RTFileReadAt(pStorage->File, uOffset, pvBuf, cbRead, pcbRead);
}

/**
 * VD async I/O interface callback for a synchronous flush of the file data.
 */
static int vdIOFlushSyncFallback(void *pvUser, void *pvStorage)
{
    PVDIIOFALLBACKSTORAGE pStorage = (PVDIIOFALLBACKSTORAGE)pvStorage;

    return RTFileFlush(pStorage->File);
}

/**
 * VD async I/O interface callback for a asynchronous read from the file.
 */
static int vdIOReadAsyncFallback(void *pvUser, void *pStorage, uint64_t uOffset,
                                 PCRTSGSEG paSegments, size_t cSegments,
                                 size_t cbRead, void *pvCompletion,
                                 void **ppTask)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * VD async I/O interface callback for a asynchronous write to the file.
 */
static int vdIOWriteAsyncFallback(void *pvUser, void *pStorage, uint64_t uOffset,
                                  PCRTSGSEG paSegments, size_t cSegments,
                                  size_t cbWrite, void *pvCompletion,
                                  void **ppTask)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * VD async I/O interface callback for a asynchronous flush of the file data.
 */
static int vdIOFlushAsyncFallback(void *pvUser, void *pStorage,
                                  void *pvCompletion, void **ppTask)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Internal - Continues an I/O context after
 * it was halted because of an active transfer.
 */
static int vdIoCtxContinue(PVDIOCTX pIoCtx, int rcReq)
{
    PVBOXHDD pDisk = pIoCtx->pDisk;
    int rc = VINF_SUCCESS;

    if (RT_FAILURE(rcReq))
        ASMAtomicCmpXchgS32(&pIoCtx->rcReq, rcReq, VINF_SUCCESS);

    if (!pIoCtx->fBlocked)
    {
        /* Continue the transfer */
        rc = vdIoCtxProcess(pIoCtx);

        if (   rc == VINF_VD_ASYNC_IO_FINISHED
            && ASMAtomicCmpXchgBool(&pIoCtx->fComplete, true, false))
        {
            LogFlowFunc(("I/O context completed pIoCtx=%#p\n", pIoCtx));
            if (pIoCtx->pIoCtxParent)
            {
                PVDIOCTX pIoCtxParent = pIoCtx->pIoCtxParent;

                LogFlowFunc(("I/O context transfered %u bytes for the parent pIoCtxParent=%p\n",
                             pIoCtx->Type.Child.cbTransferParent, pIoCtxParent));

                /* Update the parent state. */
                Assert(!pIoCtxParent->pIoCtxParent);
                Assert(pIoCtx->enmTxDir == VDIOCTXTXDIR_WRITE);
                Assert(pIoCtxParent->cbTransferLeft >= pIoCtx->Type.Child.cbTransferParent);
                ASMAtomicSubU32(&pIoCtxParent->cbTransferLeft, pIoCtx->Type.Child.cbTransferParent);

                if (RT_FAILURE(pIoCtx->rcReq))
                    ASMAtomicCmpXchgS32(&pIoCtxParent->rcReq, pIoCtx->rcReq, VINF_SUCCESS);

                /*
                 * A completed child write means that we finished growing the image.
                 * We have to process any pending writes now.
                 */
                Assert(pDisk->fGrowing);
                ASMAtomicWriteBool(&pDisk->fGrowing, false);

                /* Unblock the parent */
                pIoCtxParent->fBlocked = false;

                rc = vdIoCtxProcess(pIoCtxParent);

                if (   rc == VINF_VD_ASYNC_IO_FINISHED
                    && ASMAtomicCmpXchgBool(&pIoCtxParent->fComplete, true, false))
                {
                    LogFlowFunc(("Parent I/O context completed pIoCtxParent=%#p rcReq=%Rrc\n", pIoCtxParent, pIoCtxParent->rcReq));
                    pIoCtxParent->Type.Root.pfnComplete(pIoCtxParent->Type.Root.pvUser1,
                                                        pIoCtxParent->Type.Root.pvUser2,
                                                        pIoCtxParent->rcReq);
                    vdThreadFinishWrite(pDisk);
                    vdIoCtxFree(pDisk, pIoCtxParent);
                }

                /* Process any pending writes if the current request didn't caused another growing. */
                RTCritSectEnter(&pDisk->CritSect);

                if (!RTListIsEmpty(&pDisk->ListWriteGrowing) && !pDisk->fGrowing)
                {
                    RTLISTNODE ListTmp;

                    LogFlowFunc(("Before: pNext=%#p pPrev=%#p\n", pDisk->ListWriteGrowing.pNext,
                                 pDisk->ListWriteGrowing.pPrev));

                    RTListMove(&ListTmp, &pDisk->ListWriteGrowing);

                    LogFlowFunc(("After: pNext=%#p pPrev=%#p\n", pDisk->ListWriteGrowing.pNext,
                                 pDisk->ListWriteGrowing.pPrev));

                    RTCritSectLeave(&pDisk->CritSect);

                    /* Process the list. */
                    do
                    {
                        PVDIOCTXDEFERRED pDeferred = RTListNodeGetFirst(&ListTmp, VDIOCTXDEFERRED, NodeDeferred);
                        PVDIOCTX pIoCtxWait = pDeferred->pIoCtx;

                        AssertPtr(pIoCtxWait);

                        RTListNodeRemove(&pDeferred->NodeDeferred);
                        RTMemFree(pDeferred);

                        Assert(!pIoCtxWait->pIoCtxParent);

                        pIoCtxWait->fBlocked = false;
                        LogFlowFunc(("Processing waiting I/O context pIoCtxWait=%#p\n", pIoCtxWait));

                        rc = vdIoCtxProcess(pIoCtxWait);
                        if (   rc == VINF_VD_ASYNC_IO_FINISHED
                            && ASMAtomicCmpXchgBool(&pIoCtxWait->fComplete, true, false))
                        {
                            LogFlowFunc(("Waiting I/O context completed pIoCtxWait=%#p\n", pIoCtxWait));
                            vdThreadFinishWrite(pDisk);
                            pIoCtxWait->Type.Root.pfnComplete(pIoCtxWait->Type.Root.pvUser1,
                                                              pIoCtxWait->Type.Root.pvUser2,
                                                              pIoCtxWait->rcReq);
                            vdIoCtxFree(pDisk, pIoCtxWait);
                        }
                    } while (!RTListIsEmpty(&ListTmp));
                }
                else
                    RTCritSectLeave(&pDisk->CritSect);
            }
            else
            {
                if (pIoCtx->enmTxDir != VDIOCTXTXDIR_READ)
                {
                    AssertMsg(   pIoCtx->enmTxDir == VDIOCTXTXDIR_WRITE
                              || pIoCtx->enmTxDir == VDIOCTXTXDIR_FLUSH,
                              ("Invalid transfer direction\n"));
                    vdThreadFinishWrite(pDisk);
                }
                else
                    vdThreadFinishRead(pDisk);

                LogFlowFunc(("I/O context completed pIoCtx=%#p rcReq=%Rrc\n", pIoCtx, pIoCtx->rcReq));
                pIoCtx->Type.Root.pfnComplete(pIoCtx->Type.Root.pvUser1,
                                              pIoCtx->Type.Root.pvUser2,
                                              pIoCtx->rcReq);
            }

            vdIoCtxFree(pDisk, pIoCtx);
        }
    }

    return VINF_SUCCESS;
}

/**
 * Internal - Called when user transfer completed.
 */
static int vdUserXferCompleted(PVDIOSTORAGE pIoStorage, PVDIOCTX pIoCtx,
                               PFNVDXFERCOMPLETED pfnComplete, void *pvUser,
                               size_t cbTransfer, int rcReq)
{
    int rc = VINF_SUCCESS;
    bool fIoCtxContinue = true;
    PVBOXHDD pDisk = pIoCtx->pDisk;

    LogFlowFunc(("pIoStorage=%#p pIoCtx=%#p pfnComplete=%#p pvUser=%#p cbTransfer=%zu rcReq=%Rrc\n",
                 pIoStorage, pIoCtx, pfnComplete, pvUser, cbTransfer, rcReq));

    Assert(pIoCtx->cbTransferLeft >= cbTransfer);
    ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbTransfer);
    ASMAtomicDecU32(&pIoCtx->cDataTransfersPending);

    if (pfnComplete)
    {
        RTCritSectEnter(&pDisk->CritSect);
        rc = pfnComplete(pIoStorage->pImage->pBackendData, pIoCtx, pvUser, rcReq);
        RTCritSectLeave(&pDisk->CritSect);
    }

    if (RT_SUCCESS(rc))
        rc = vdIoCtxContinue(pIoCtx, rcReq);
    else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Internal - Called when a meta transfer completed.
 */
static int vdMetaXferCompleted(PVDIOSTORAGE pIoStorage, PFNVDXFERCOMPLETED pfnComplete, void *pvUser,
                               PVDMETAXFER pMetaXfer, int rcReq)
{
    PVBOXHDD pDisk = pIoStorage->pImage->pDisk;
    RTLISTNODE ListIoCtxWaiting;
    bool fFlush;

    LogFlowFunc(("pIoStorage=%#p pfnComplete=%#p pvUser=%#p pMetaXfer=%#p rcReq=%Rrc\n",
                 pIoStorage, pfnComplete, pvUser, pMetaXfer, rcReq));

    RTCritSectEnter(&pDisk->CritSect);
    fFlush = VDMETAXFER_TXDIR_GET(pMetaXfer->fFlags) == VDMETAXFER_TXDIR_FLUSH;
    VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_NONE);

    if (!fFlush)
    {
        RTListMove(&ListIoCtxWaiting, &pMetaXfer->ListIoCtxWaiting);

        if (RT_FAILURE(rcReq))
        {
            /* Remove from the AVL tree. */
            LogFlow(("Removing meta xfer=%#p\n", pMetaXfer));
            bool fRemoved = RTAvlrFileOffsetRemove(pIoStorage->pTreeMetaXfers, pMetaXfer->Core.Key) != NULL;
            Assert(fRemoved);
            RTMemFree(pMetaXfer);
        }
        else
        {
            /* Increase the reference counter to make sure it doesn't go away before the last context is processed. */
            pMetaXfer->cRefs++;
        }
    }
    else
        RTListMove(&ListIoCtxWaiting, &pMetaXfer->ListIoCtxWaiting);
    RTCritSectLeave(&pDisk->CritSect);

    /* Go through the waiting list and continue the I/O contexts. */
    while (!RTListIsEmpty(&ListIoCtxWaiting))
    {
        int rc = VINF_SUCCESS;
        bool fContinue = true;
        PVDIOCTXDEFERRED pDeferred = RTListNodeGetFirst(&ListIoCtxWaiting, VDIOCTXDEFERRED, NodeDeferred);
        PVDIOCTX pIoCtx = pDeferred->pIoCtx;
        RTListNodeRemove(&pDeferred->NodeDeferred);

        RTMemFree(pDeferred);
        ASMAtomicDecU32(&pIoCtx->cMetaTransfersPending);

        if (pfnComplete)
        {
            RTCritSectEnter(&pDisk->CritSect);
            rc = pfnComplete(pIoStorage->pImage->pBackendData, pIoCtx, pvUser, rcReq);
            RTCritSectLeave(&pDisk->CritSect);
        }

        LogFlow(("Completion callback for I/O context %#p returned %Rrc\n", pIoCtx, rc));

        if (RT_SUCCESS(rc))
        {
            rc = vdIoCtxContinue(pIoCtx, rcReq);
            AssertRC(rc);
        }
        else
            Assert(rc == VERR_VD_ASYNC_IO_IN_PROGRESS);
    }

    /* Remove if not used anymore. */
    if (RT_SUCCESS(rcReq) && !fFlush)
    {
        RTCritSectEnter(&pDisk->CritSect);
        pMetaXfer->cRefs--;
        if (!pMetaXfer->cRefs && RTListIsEmpty(&pMetaXfer->ListIoCtxWaiting))
        {
            /* Remove from the AVL tree. */
            LogFlow(("Removing meta xfer=%#p\n", pMetaXfer));
            bool fRemoved = RTAvlrFileOffsetRemove(pIoStorage->pTreeMetaXfers, pMetaXfer->Core.Key) != NULL;
            Assert(fRemoved);
            RTMemFree(pMetaXfer);
        }
        RTCritSectLeave(&pDisk->CritSect);
    }
    else if (fFlush)
        RTMemFree(pMetaXfer);

    return VINF_SUCCESS;
}

static int vdIOIntReqCompleted(void *pvUser, int rcReq)
{
    int rc = VINF_SUCCESS;
    PVDIOTASK pIoTask = (PVDIOTASK)pvUser;
    PVDIOSTORAGE pIoStorage = pIoTask->pIoStorage;

    LogFlowFunc(("Task completed pIoTask=%#p\n", pIoTask));

    if (!pIoTask->fMeta)
        rc = vdUserXferCompleted(pIoStorage, pIoTask->Type.User.pIoCtx,
                                 pIoTask->pfnComplete, pIoTask->pvUser,
                                 pIoTask->Type.User.cbTransfer, rcReq);
    else
        rc = vdMetaXferCompleted(pIoStorage, pIoTask->pfnComplete, pIoTask->pvUser,
                                 pIoTask->Type.Meta.pMetaXfer, rcReq);

    vdIoTaskFree(pIoStorage->pImage->pDisk, pIoTask);

    return rc;
}

/**
 * VD I/O interface callback for opening a file.
 */
static int vdIOIntOpen(void *pvUser, const char *pszLocation,
                    unsigned uOpenFlags, PPVDIOSTORAGE ppIoStorage)
{
    int rc = VINF_SUCCESS;
    PVDIMAGE pImage         = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk          = pImage->pDisk;
    PVDIOSTORAGE pIoStorage = (PVDIOSTORAGE)RTMemAllocZ(sizeof(VDIOSTORAGE));

    if (!pIoStorage)
        return VERR_NO_MEMORY;

    pIoStorage->pImage = pImage;

    /* Create the AVl tree. */
    pIoStorage->pTreeMetaXfers = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
    if (pIoStorage->pTreeMetaXfers)
    {
        rc = pDisk->pInterfaceIOCallbacks->pfnOpen(pDisk->pInterfaceIO->pvUser,
                                                   pszLocation, uOpenFlags,
                                                   vdIOIntReqCompleted,
                                                   pDisk->pVDIfsDisk,
                                                   &pIoStorage->pStorage);
        if (RT_SUCCESS(rc))
        {
            *ppIoStorage = pIoStorage;
            return VINF_SUCCESS;
        }

        RTMemFree(pIoStorage->pTreeMetaXfers);
    }
    else
        rc = VERR_NO_MEMORY;

    RTMemFree(pIoStorage);
    return rc;
}

static int vdIOIntTreeMetaXferDestroy(PAVLRFOFFNODECORE pNode, void *pvUser)
{
    AssertMsgFailed(("Tree should be empty at this point!\n"));
    return VINF_SUCCESS;
}

static int vdIOIntClose(void *pvUser, PVDIOSTORAGE pIoStorage)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    int rc = pDisk->pInterfaceIOCallbacks->pfnClose(pDisk->pInterfaceIO->pvUser,
                                                    pIoStorage->pStorage);
    AssertRC(rc);

    RTAvlrFileOffsetDestroy(pIoStorage->pTreeMetaXfers, vdIOIntTreeMetaXferDestroy, NULL);
    RTMemFree(pIoStorage->pTreeMetaXfers);
    RTMemFree(pIoStorage);
    return VINF_SUCCESS;
}

static int vdIOIntDelete(void *pvUser, const char *pcszFilename)
{
    PVDIMAGE pImage         = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk          = pImage->pDisk;
    return pDisk->pInterfaceIOCallbacks->pfnDelete(pDisk->pInterfaceIO->pvUser,
                                                   pcszFilename);
}

static int vdIOIntMove(void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove)
{
    PVDIMAGE pImage         = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk          = pImage->pDisk;
    return pDisk->pInterfaceIOCallbacks->pfnMove(pDisk->pInterfaceIO->pvUser,
                                                 pcszSrc, pcszDst, fMove);
}

static int vdIOIntGetFreeSpace(void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    PVDIMAGE pImage         = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk          = pImage->pDisk;
    return pDisk->pInterfaceIOCallbacks->pfnGetFreeSpace(pDisk->pInterfaceIO->pvUser,
                                                         pcszFilename, pcbFreeSpace);
}

static int vdIOIntGetModificationTime(void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    PVDIMAGE pImage         = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk          = pImage->pDisk;
    return pDisk->pInterfaceIOCallbacks->pfnGetModificationTime(pDisk->pInterfaceIO->pvUser,
                                                                pcszFilename, pModificationTime);
}

static int vdIOIntGetSize(void *pvUser, PVDIOSTORAGE pIoStorage,
                       uint64_t *pcbSize)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    return pDisk->pInterfaceIOCallbacks->pfnGetSize(pDisk->pInterfaceIO->pvUser,
                                                    pIoStorage->pStorage,
                                                    pcbSize);
}

static int vdIOIntSetSize(void *pvUser, PVDIOSTORAGE pIoStorage,
                       uint64_t cbSize)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    return pDisk->pInterfaceIOCallbacks->pfnSetSize(pDisk->pInterfaceIO->pvUser,
                                                    pIoStorage->pStorage,
                                                    cbSize);
}

static int vdIOIntWriteSync(void *pvUser, PVDIOSTORAGE pIoStorage, uint64_t uOffset,
                         const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    return pDisk->pInterfaceIOCallbacks->pfnWriteSync(pDisk->pInterfaceIO->pvUser,
                                                      pIoStorage->pStorage,
                                                      uOffset, pvBuf, cbWrite,
                                                      pcbWritten);
}

static int vdIOIntReadSync(void *pvUser, PVDIOSTORAGE pIoStorage, uint64_t uOffset,
                        void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    return pDisk->pInterfaceIOCallbacks->pfnReadSync(pDisk->pInterfaceIO->pvUser,
                                                     pIoStorage->pStorage,
                                                     uOffset, pvBuf, cbRead,
                                                     pcbRead);
}

static int vdIOIntFlushSync(void *pvUser, PVDIOSTORAGE pIoStorage)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    return pDisk->pInterfaceIOCallbacks->pfnFlushSync(pDisk->pInterfaceIO->pvUser,
                                                      pIoStorage->pStorage);
}

static int vdIOIntReadUserAsync(void *pvUser, PVDIOSTORAGE pIoStorage,
                             uint64_t uOffset, PVDIOCTX pIoCtx,
                             size_t cbRead)
{
    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    LogFlowFunc(("pvUser=%#p pIoStorage=%#p uOffset=%llu pIoCtx=%#p cbRead=%u\n",
                 pvUser, pIoStorage, uOffset, pIoCtx, cbRead));

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    /* Build the S/G array and spawn a new I/O task */
    while (cbRead)
    {
        RTSGSEG  aSeg[VD_IO_TASK_SEGMENTS_MAX];
        unsigned cSegments  = VD_IO_TASK_SEGMENTS_MAX;
        size_t   cbTaskRead = 0;

        cbTaskRead = RTSgBufSegArrayCreate(&pIoCtx->SgBuf, aSeg, &cSegments, cbRead);

        AssertMsg(cbTaskRead <= cbRead, ("Invalid number of bytes to read\n"));

        LogFlow(("Reading %u bytes into %u segments\n", cbTaskRead, cSegments));

#ifdef RT_STRICT
        for (unsigned i = 0; i < cSegments; i++)
                AssertMsg(aSeg[i].pvSeg && !(aSeg[i].cbSeg % 512),
                          ("Segment %u is invalid\n", i));
#endif

        PVDIOTASK pIoTask = vdIoTaskUserAlloc(pIoStorage, NULL, NULL, pIoCtx, cbTaskRead);

        if (!pIoTask)
            return VERR_NO_MEMORY;

        ASMAtomicIncU32(&pIoCtx->cDataTransfersPending);

        void *pvTask;
        rc = pDisk->pInterfaceIOCallbacks->pfnReadAsync(pDisk->pInterfaceIO->pvUser,
                                                             pIoStorage->pStorage,
                                                             uOffset, aSeg, cSegments,
                                                             cbTaskRead, pIoTask,
                                                             &pvTask);
        if (RT_SUCCESS(rc))
        {
            AssertMsg(cbTaskRead <= pIoCtx->cbTransferLeft, ("Impossible!\n"));
            ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbTaskRead);
            ASMAtomicDecU32(&pIoCtx->cDataTransfersPending);
            vdIoTaskFree(pDisk, pIoTask);
        }
        else if (rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        {
            ASMAtomicDecU32(&pIoCtx->cDataTransfersPending);
            break;
        }

        uOffset += cbTaskRead;
        cbRead  -= cbTaskRead;
    }

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

static int vdIOIntWriteUserAsync(void *pvUser, PVDIOSTORAGE pIoStorage,
                              uint64_t uOffset, PVDIOCTX pIoCtx,
                              size_t cbWrite,
                              PFNVDXFERCOMPLETED pfnComplete,
                              void *pvCompleteUser)
{
    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;

    LogFlowFunc(("pvUser=%#p pIoStorage=%#p uOffset=%llu pIoCtx=%#p cbWrite=%u\n",
                 pvUser, pIoStorage, uOffset, pIoCtx, cbWrite));

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    /* Build the S/G array and spawn a new I/O task */
    while (cbWrite)
    {
        RTSGSEG  aSeg[VD_IO_TASK_SEGMENTS_MAX];
        unsigned cSegments   = VD_IO_TASK_SEGMENTS_MAX;
        size_t   cbTaskWrite = 0;

        cbTaskWrite = RTSgBufSegArrayCreate(&pIoCtx->SgBuf, aSeg, &cSegments, cbWrite);

        AssertMsg(cbTaskWrite <= cbWrite, ("Invalid number of bytes to write\n"));

        LogFlow(("Writing %u bytes from %u segments\n", cbTaskWrite, cSegments));

#ifdef DEBUG
        for (unsigned i = 0; i < cSegments; i++)
                AssertMsg(aSeg[i].pvSeg && !(aSeg[i].cbSeg % 512),
                          ("Segment %u is invalid\n", i));
#endif

        PVDIOTASK pIoTask = vdIoTaskUserAlloc(pIoStorage, pfnComplete, pvCompleteUser, pIoCtx, cbTaskWrite);

        if (!pIoTask)
            return VERR_NO_MEMORY;

        ASMAtomicIncU32(&pIoCtx->cDataTransfersPending);

        void *pvTask;
        rc = pDisk->pInterfaceIOCallbacks->pfnWriteAsync(pDisk->pInterfaceIO->pvUser,
                                                              pIoStorage->pStorage,
                                                              uOffset, aSeg, cSegments,
                                                              cbTaskWrite, pIoTask,
                                                              &pvTask);
        if (RT_SUCCESS(rc))
        {
            AssertMsg(cbTaskWrite <= pIoCtx->cbTransferLeft, ("Impossible!\n"));
            ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbTaskWrite);
            ASMAtomicDecU32(&pIoCtx->cDataTransfersPending);
            vdIoTaskFree(pDisk, pIoTask);
        }
        else if (rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        {
            ASMAtomicDecU32(&pIoCtx->cDataTransfersPending);
            break;
        }

        uOffset += cbTaskWrite;
        cbWrite -= cbTaskWrite;
    }

    return rc;
}

static int vdIOIntReadMetaAsync(void *pvUser, PVDIOSTORAGE pIoStorage,
                             uint64_t uOffset, void *pvBuf,
                             size_t cbRead, PVDIOCTX pIoCtx,
                             PPVDMETAXFER ppMetaXfer,
                             PFNVDXFERCOMPLETED pfnComplete,
                             void *pvCompleteUser)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    int rc = VINF_SUCCESS;
    RTSGSEG Seg;
    PVDIOTASK pIoTask;
    PVDMETAXFER pMetaXfer = NULL;
    void *pvTask = NULL;

    LogFlowFunc(("pvUser=%#p pIoStorage=%#p uOffset=%llu pvBuf=%#p cbRead=%u\n",
                 pvUser, pIoStorage, uOffset, pvBuf, cbRead));

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    pMetaXfer = (PVDMETAXFER)RTAvlrFileOffsetGet(pIoStorage->pTreeMetaXfers, uOffset);
    if (!pMetaXfer)
    {
#ifdef RT_STRICT
        pMetaXfer = (PVDMETAXFER)RTAvlrFileOffsetGetBestFit(pIoStorage->pTreeMetaXfers, uOffset, false /* fAbove */);
        AssertMsg(!pMetaXfer || (pMetaXfer->Core.Key + (RTFOFF)pMetaXfer->cbMeta <= (RTFOFF)uOffset),
                  ("Overlapping meta transfers!\n"));
#endif

        /* Allocate a new meta transfer. */
        pMetaXfer = vdMetaXferAlloc(pImage, pIoStorage, uOffset, cbRead);
        if (!pMetaXfer)
            return VERR_NO_MEMORY;

        pIoTask = vdIoTaskMetaAlloc(pIoStorage, pfnComplete, pvCompleteUser, pMetaXfer);
        if (!pIoTask)
        {
            RTMemFree(pMetaXfer);
            return VERR_NO_MEMORY;
        }

        Seg.cbSeg = cbRead;
        Seg.pvSeg = pMetaXfer->abData;

        VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_READ);
        rc = pDisk->pInterfaceIOCallbacks->pfnReadAsync(pDisk->pInterfaceIO->pvUser,
                                                             pIoStorage->pStorage,
                                                             uOffset, &Seg, 1,
                                                             cbRead, pIoTask,
                                                             &pvTask);

        if (RT_SUCCESS(rc) || rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        {
            bool fInserted = RTAvlrFileOffsetInsert(pIoStorage->pTreeMetaXfers, &pMetaXfer->Core);
            Assert(fInserted);
        }
        else
            RTMemFree(pMetaXfer);

        if (RT_SUCCESS(rc))
        {
            VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_NONE);
            vdIoTaskFree(pDisk, pIoTask);
        }
        else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS && !pfnComplete)
            rc = VERR_VD_NOT_ENOUGH_METADATA;
    }

    Assert(VALID_PTR(pMetaXfer) || RT_FAILURE(rc));

    if (RT_SUCCESS(rc) || rc == VERR_VD_NOT_ENOUGH_METADATA || rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
    {
        /* If it is pending add the request to the list. */
        if (VDMETAXFER_TXDIR_GET(pMetaXfer->fFlags) == VDMETAXFER_TXDIR_READ)
        {
            PVDIOCTXDEFERRED pDeferred = (PVDIOCTXDEFERRED)RTMemAllocZ(sizeof(VDIOCTXDEFERRED));
            AssertPtr(pDeferred);

            RTListInit(&pDeferred->NodeDeferred);
            pDeferred->pIoCtx = pIoCtx;

            ASMAtomicIncU32(&pIoCtx->cMetaTransfersPending);
            RTListAppend(&pMetaXfer->ListIoCtxWaiting, &pDeferred->NodeDeferred);
            rc = VERR_VD_NOT_ENOUGH_METADATA;
        }
        else
        {
            /* Transfer the data. */
            pMetaXfer->cRefs++;
            Assert(pMetaXfer->cbMeta >= cbRead);
            Assert(pMetaXfer->Core.Key == (RTFOFF)uOffset);
            memcpy(pvBuf, pMetaXfer->abData, cbRead);
            *ppMetaXfer = pMetaXfer;
        }
    }

    return rc;
}

static int vdIOIntWriteMetaAsync(void *pvUser, PVDIOSTORAGE pIoStorage,
                              uint64_t uOffset, void *pvBuf,
                              size_t cbWrite, PVDIOCTX pIoCtx,
                              PFNVDXFERCOMPLETED pfnComplete,
                              void *pvCompleteUser)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    int rc = VINF_SUCCESS;
    RTSGSEG Seg;
    PVDIOTASK pIoTask;
    PVDMETAXFER pMetaXfer = NULL;
    bool fInTree = false;
    void *pvTask = NULL;

    LogFlowFunc(("pvUser=%#p pIoStorage=%#p uOffset=%llu pvBuf=%#p cbWrite=%u\n",
                 pvUser, pIoStorage, uOffset, pvBuf, cbWrite));

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    pMetaXfer = (PVDMETAXFER)RTAvlrFileOffsetGet(pIoStorage->pTreeMetaXfers, uOffset);
    if (!pMetaXfer)
    {
        /* Allocate a new meta transfer. */
        pMetaXfer = vdMetaXferAlloc(pImage, pIoStorage, uOffset, cbWrite);
        if (!pMetaXfer)
            return VERR_NO_MEMORY;
    }
    else
    {
        Assert(pMetaXfer->cbMeta >= cbWrite);
        Assert(pMetaXfer->Core.Key == (RTFOFF)uOffset);
        fInTree = true;
    }

    Assert(VDMETAXFER_TXDIR_GET(pMetaXfer->fFlags) == VDMETAXFER_TXDIR_NONE);

    pIoTask = vdIoTaskMetaAlloc(pIoStorage, pfnComplete, pvCompleteUser, pMetaXfer);
    if (!pIoTask)
    {
        RTMemFree(pMetaXfer);
        return VERR_NO_MEMORY;
    }

    memcpy(pMetaXfer->abData, pvBuf, cbWrite);
    Seg.cbSeg = cbWrite;
    Seg.pvSeg = pMetaXfer->abData;

    ASMAtomicIncU32(&pIoCtx->cMetaTransfersPending);

    VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_WRITE);
    rc = pDisk->pInterfaceIOCallbacks->pfnWriteAsync(pDisk->pInterfaceIO->pvUser,
                                                          pIoStorage->pStorage,
                                                          uOffset, &Seg, 1,
                                                          cbWrite, pIoTask,
                                                          &pvTask);
    if (RT_SUCCESS(rc))
    {
        VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_NONE);
        ASMAtomicDecU32(&pIoCtx->cMetaTransfersPending);
        vdIoTaskFree(pDisk, pIoTask);
        if (fInTree && !pMetaXfer->cRefs)
        {
            LogFlow(("Removing meta xfer=%#p\n", pMetaXfer));
            bool fRemoved = RTAvlrFileOffsetRemove(pIoStorage->pTreeMetaXfers, pMetaXfer->Core.Key) != NULL;
            AssertMsg(fRemoved, ("Metadata transfer wasn't removed\n"));
            RTMemFree(pMetaXfer);
            pMetaXfer = NULL;
        }
    }
    else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
    {
        PVDIOCTXDEFERRED pDeferred = (PVDIOCTXDEFERRED)RTMemAllocZ(sizeof(VDIOCTXDEFERRED));
        AssertPtr(pDeferred);

        RTListInit(&pDeferred->NodeDeferred);
        pDeferred->pIoCtx = pIoCtx;

        if (!fInTree)
        {
            bool fInserted = RTAvlrFileOffsetInsert(pIoStorage->pTreeMetaXfers, &pMetaXfer->Core);
            Assert(fInserted);
        }

        RTListAppend(&pMetaXfer->ListIoCtxWaiting, &pDeferred->NodeDeferred);
    }
    else
    {
        RTMemFree(pMetaXfer);
        pMetaXfer = NULL;
    }

    return rc;
}

static void vdIOIntMetaXferRelease(void *pvUser, PVDMETAXFER pMetaXfer)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVDIOSTORAGE pIoStorage = pMetaXfer->pIoStorage;

    VD_THREAD_IS_CRITSECT_OWNER(pImage->pDisk);

    Assert(   VDMETAXFER_TXDIR_GET(pMetaXfer->fFlags) == VDMETAXFER_TXDIR_NONE
           || VDMETAXFER_TXDIR_GET(pMetaXfer->fFlags) == VDMETAXFER_TXDIR_WRITE);
    Assert(pMetaXfer->cRefs > 0);

    pMetaXfer->cRefs--;
    if (   !pMetaXfer->cRefs
        && RTListIsEmpty(&pMetaXfer->ListIoCtxWaiting)
        && VDMETAXFER_TXDIR_GET(pMetaXfer->fFlags) == VDMETAXFER_TXDIR_NONE)
    {
        /* Free the meta data entry. */
        LogFlow(("Removing meta xfer=%#p\n", pMetaXfer));
        bool fRemoved = RTAvlrFileOffsetRemove(pIoStorage->pTreeMetaXfers, pMetaXfer->Core.Key) != NULL;
        AssertMsg(fRemoved, ("Metadata transfer wasn't removed\n"));

        RTMemFree(pMetaXfer);
    }
}

static int vdIOIntFlushAsync(void *pvUser, PVDIOSTORAGE pIoStorage,
                          PVDIOCTX pIoCtx,
                          PFNVDXFERCOMPLETED pfnComplete,
                          void *pvCompleteUser)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    int rc = VINF_SUCCESS;
    PVDIOTASK pIoTask;
    PVDMETAXFER pMetaXfer = NULL;
    void *pvTask = NULL;

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    LogFlowFunc(("pvUser=%#p pIoStorage=%#p pIoCtx=%#p\n",
                 pvUser, pIoStorage, pIoCtx));

    /* Allocate a new meta transfer. */
    pMetaXfer = vdMetaXferAlloc(pImage, pIoStorage, 0, 0);
    if (!pMetaXfer)
        return VERR_NO_MEMORY;

    pIoTask = vdIoTaskMetaAlloc(pIoStorage, pfnComplete, pvUser, pMetaXfer);
    if (!pIoTask)
    {
        RTMemFree(pMetaXfer);
        return VERR_NO_MEMORY;
    }

    ASMAtomicIncU32(&pIoCtx->cMetaTransfersPending);

    PVDIOCTXDEFERRED pDeferred = (PVDIOCTXDEFERRED)RTMemAllocZ(sizeof(VDIOCTXDEFERRED));
    AssertPtr(pDeferred);

    RTListInit(&pDeferred->NodeDeferred);
    pDeferred->pIoCtx = pIoCtx;

    RTListAppend(&pMetaXfer->ListIoCtxWaiting, &pDeferred->NodeDeferred);
    VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_FLUSH);
    rc = pDisk->pInterfaceIOCallbacks->pfnFlushAsync(pDisk->pInterfaceIO->pvUser,
                                                          pIoStorage->pStorage,
                                                          pIoTask,
                                                          &pvTask);
    if (RT_SUCCESS(rc))
    {
        VDMETAXFER_TXDIR_SET(pMetaXfer->fFlags, VDMETAXFER_TXDIR_NONE);
        ASMAtomicDecU32(&pIoCtx->cMetaTransfersPending);
        vdIoTaskFree(pDisk, pIoTask);
        RTMemFree(pDeferred);
        RTMemFree(pMetaXfer);
    }
    else if (rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        RTMemFree(pMetaXfer);

    return rc;
}

static size_t vdIOIntIoCtxCopyTo(void *pvUser, PVDIOCTX pIoCtx,
                              void *pvBuf, size_t cbBuf)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    size_t cbCopied = 0;

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    cbCopied = vdIoCtxCopyTo(pIoCtx, (uint8_t *)pvBuf, cbBuf);
    Assert(cbCopied == cbBuf);

    ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbCopied);

    return cbCopied;
}

static size_t vdIOIntIoCtxCopyFrom(void *pvUser, PVDIOCTX pIoCtx,
                                void *pvBuf, size_t cbBuf)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    size_t cbCopied = 0;

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    cbCopied = vdIoCtxCopyFrom(pIoCtx, (uint8_t *)pvBuf, cbBuf);
    Assert(cbCopied == cbBuf);

    ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbCopied);

    return cbCopied;
}

static size_t vdIOIntIoCtxSet(void *pvUser, PVDIOCTX pIoCtx,
                           int ch, size_t cb)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    size_t cbSet = 0;

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    cbSet = vdIoCtxSet(pIoCtx, ch, cb);
    Assert(cbSet == cb);

    ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbSet);

    return cbSet;
}

static size_t vdIOIntIoCtxSegArrayCreate(void *pvUser, PVDIOCTX pIoCtx,
                                     PRTSGSEG paSeg, unsigned *pcSeg,
                                     size_t cbData)
{
    PVDIMAGE pImage = (PVDIMAGE)pvUser;
    PVBOXHDD pDisk  = pImage->pDisk;
    size_t cbCreated = 0;

    VD_THREAD_IS_CRITSECT_OWNER(pDisk);

    cbCreated = RTSgBufSegArrayCreate(&pIoCtx->SgBuf, paSeg, pcSeg, cbData);
    Assert(!paSeg || cbData == cbCreated);

    return cbCreated;
}

static void vdIOIntIoCtxCompleted(void *pvUser, PVDIOCTX pIoCtx, int rcReq,
                               size_t cbCompleted)
{
    /* Continue */
    pIoCtx->fBlocked = false;
    ASMAtomicSubU32(&pIoCtx->cbTransferLeft, cbCompleted);

    /* Clear the pointer to next transfer function in case we have nothing to transfer anymore.
     * @todo: Find a better way to prevent vdIoCtxContinue from calling the read/write helper again. */
    if (!pIoCtx->cbTransferLeft)
        pIoCtx->pfnIoCtxTransfer = NULL;

    vdIoCtxContinue(pIoCtx, rcReq);
}

/**
 * VD I/O interface callback for opening a file (limited version for VDGetFormat).
 */
static int vdIOIntOpenLimited(void *pvUser, const char *pszLocation,
                              uint32_t fOpen, PPVDIOSTORAGE ppIoStorage)
{
    int rc = VINF_SUCCESS;
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    PVDIOSTORAGE pIoStorage = (PVDIOSTORAGE)RTMemAllocZ(sizeof(VDIOSTORAGE));

    if (!pIoStorage)
        return VERR_NO_MEMORY;

    rc = pInterfaceIOCallbacks->pfnOpen(NULL, pszLocation, fOpen,
                                        NULL, NULL, &pIoStorage->pStorage);
    if (RT_SUCCESS(rc))
        *ppIoStorage = pIoStorage;
    else
        RTMemFree(pIoStorage);

    return rc;
}

static int vdIOIntCloseLimited(void *pvUser, PVDIOSTORAGE pIoStorage)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    int rc = pInterfaceIOCallbacks->pfnClose(NULL, pIoStorage->pStorage);
    AssertRC(rc);

    RTMemFree(pIoStorage);
    return VINF_SUCCESS;
}

static int vdIOIntDeleteLimited(void *pvUser, const char *pcszFilename)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnDelete(NULL, pcszFilename);
}

static int vdIOIntMoveLimited(void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnMove(NULL, pcszSrc, pcszDst, fMove);
}

static int vdIOIntGetFreeSpaceLimited(void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnGetFreeSpace(NULL, pcszFilename, pcbFreeSpace);
}

static int vdIOIntGetModificationTimeLimited(void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnGetModificationTime(NULL, pcszFilename, pModificationTime);
}

static int vdIOIntGetSizeLimited(void *pvUser, PVDIOSTORAGE pIoStorage,
                       uint64_t *pcbSize)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnGetSize(NULL, pIoStorage->pStorage, pcbSize);
}

static int vdIOIntSetSizeLimited(void *pvUser, PVDIOSTORAGE pIoStorage,
                       uint64_t cbSize)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnSetSize(NULL, pIoStorage->pStorage, cbSize);
}

static int vdIOIntWriteSyncLimited(void *pvUser, PVDIOSTORAGE pIoStorage, uint64_t uOffset,
                         const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnWriteSync(NULL, pIoStorage->pStorage, uOffset, pvBuf, cbWrite, pcbWritten);
}

static int vdIOIntReadSyncLimited(void *pvUser, PVDIOSTORAGE pIoStorage, uint64_t uOffset,
                        void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnReadSync(NULL, pIoStorage->pStorage, uOffset, pvBuf, cbRead, pcbRead);
}

static int vdIOIntFlushSyncLimited(void *pvUser, PVDIOSTORAGE pIoStorage)
{
    PVDINTERFACEIO pInterfaceIOCallbacks = (PVDINTERFACEIO)pvUser;
    return pInterfaceIOCallbacks->pfnFlushSync(NULL, pIoStorage->pStorage);
}

/**
 * internal: send output to the log (unconditionally).
 */
int vdLogMessage(void *pvUser, const char *pszFormat, va_list args)
{
    NOREF(pvUser);
    RTLogPrintfV(pszFormat, args);
    return VINF_SUCCESS;
}

DECLINLINE(int) vdMessageWrapper(PVBOXHDD pDisk, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = pDisk->pInterfaceErrorCallbacks->pfnMessage(pDisk->pInterfaceError->pvUser,
                                                         pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Initializes HDD backends.
 *
 * @returns VBox status code.
 */
VBOXDDU_DECL(int) VDInit(void)
{
    int rc = vdAddBackends(aStaticBackends, RT_ELEMENTS(aStaticBackends));
    if (RT_SUCCESS(rc))
    {
        rc = vdAddCacheBackends(aStaticCacheBackends, RT_ELEMENTS(aStaticCacheBackends));
        if (RT_SUCCESS(rc))
        {
            rc = vdLoadDynamicBackends();
            if (RT_SUCCESS(rc))
                rc = vdLoadDynamicCacheBackends();
        }
    }
    LogRel(("VDInit finished\n"));
    return rc;
}

/**
 * Destroys loaded HDD backends.
 *
 * @returns VBox status code.
 */
VBOXDDU_DECL(int) VDShutdown(void)
{
    PVBOXHDDBACKEND *pBackends = g_apBackends;
    PVDCACHEBACKEND *pCacheBackends = g_apCacheBackends;
    unsigned cBackends = g_cBackends;

    if (!pBackends)
        return VERR_INTERNAL_ERROR;

    g_cBackends = 0;
    g_apBackends = NULL;

#ifndef VBOX_HDD_NO_DYNAMIC_BACKENDS
    for (unsigned i = 0; i < cBackends; i++)
        if (pBackends[i]->hPlugin != NIL_RTLDRMOD)
            RTLdrClose(pBackends[i]->hPlugin);
#endif

    /* Clear the supported cache backends. */
    cBackends = g_cCacheBackends;
    g_cCacheBackends = 0;
    g_apCacheBackends = NULL;

#ifndef VBOX_HDD_NO_DYNAMIC_BACKENDS
    for (unsigned i = 0; i < cBackends; i++)
        if (pCacheBackends[i]->hPlugin != NIL_RTLDRMOD)
            RTLdrClose(pCacheBackends[i]->hPlugin);
#endif

    if (pCacheBackends)
        RTMemFree(pCacheBackends);
    RTMemFree(pBackends);
    return VINF_SUCCESS;
}


/**
 * Lists all HDD backends and their capabilities in a caller-provided buffer.
 *
 * @returns VBox status code.
 *          VERR_BUFFER_OVERFLOW if not enough space is passed.
 * @param   cEntriesAlloc   Number of list entries available.
 * @param   pEntries        Pointer to array for the entries.
 * @param   pcEntriesUsed   Number of entries returned.
 */
VBOXDDU_DECL(int) VDBackendInfo(unsigned cEntriesAlloc, PVDBACKENDINFO pEntries,
                                unsigned *pcEntriesUsed)
{
    int rc = VINF_SUCCESS;
    PRTDIR pPluginDir = NULL;
    unsigned cEntries = 0;

    LogFlowFunc(("cEntriesAlloc=%u pEntries=%#p pcEntriesUsed=%#p\n", cEntriesAlloc, pEntries, pcEntriesUsed));
    /* Check arguments. */
    AssertMsgReturn(cEntriesAlloc,
                    ("cEntriesAlloc=%u\n", cEntriesAlloc),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pEntries),
                    ("pEntries=%#p\n", pEntries),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pcEntriesUsed),
                    ("pcEntriesUsed=%#p\n", pcEntriesUsed),
                    VERR_INVALID_PARAMETER);
    if (!g_apBackends)
        VDInit();

    if (cEntriesAlloc < g_cBackends)
    {
        *pcEntriesUsed = g_cBackends;
        return VERR_BUFFER_OVERFLOW;
    }

    for (unsigned i = 0; i < g_cBackends; i++)
    {
        pEntries[i].pszBackend = g_apBackends[i]->pszBackendName;
        pEntries[i].uBackendCaps = g_apBackends[i]->uBackendCaps;
        pEntries[i].papszFileExtensions = g_apBackends[i]->papszFileExtensions;
        pEntries[i].paConfigInfo = g_apBackends[i]->paConfigInfo;
        pEntries[i].pfnComposeLocation = g_apBackends[i]->pfnComposeLocation;
        pEntries[i].pfnComposeName = g_apBackends[i]->pfnComposeName;
    }

    LogFlowFunc(("returns %Rrc *pcEntriesUsed=%u\n", rc, cEntries));
    *pcEntriesUsed = g_cBackends;
    return rc;
}

/**
 * Lists the capablities of a backend indentified by its name.
 *
 * @returns VBox status code.
 * @param   pszBackend      The backend name.
 * @param   pEntries        Pointer to an entry.
 */
VBOXDDU_DECL(int) VDBackendInfoOne(const char *pszBackend, PVDBACKENDINFO pEntry)
{
    LogFlowFunc(("pszBackend=%#p pEntry=%#p\n", pszBackend, pEntry));
    /* Check arguments. */
    AssertMsgReturn(VALID_PTR(pszBackend),
                    ("pszBackend=%#p\n", pszBackend),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pEntry),
                    ("pEntry=%#p\n", pEntry),
                    VERR_INVALID_PARAMETER);
    if (!g_apBackends)
        VDInit();

    /* Go through loaded backends. */
    for (unsigned i = 0; i < g_cBackends; i++)
    {
        if (!RTStrICmp(pszBackend, g_apBackends[i]->pszBackendName))
        {
            pEntry->pszBackend = g_apBackends[i]->pszBackendName;
            pEntry->uBackendCaps = g_apBackends[i]->uBackendCaps;
            pEntry->papszFileExtensions = g_apBackends[i]->papszFileExtensions;
            pEntry->paConfigInfo = g_apBackends[i]->paConfigInfo;
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

/**
 * Allocates and initializes an empty HDD container.
 * No image files are opened.
 *
 * @returns VBox status code.
 * @param   pVDIfsDisk      Pointer to the per-disk VD interface list.
 * @param   ppDisk          Where to store the reference to HDD container.
 */
VBOXDDU_DECL(int) VDCreate(PVDINTERFACE pVDIfsDisk, PVBOXHDD *ppDisk)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = NULL;

    LogFlowFunc(("pVDIfsDisk=%#p\n", pVDIfsDisk));
    do
    {
        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(ppDisk),
                           ("ppDisk=%#p\n", ppDisk),
                           rc = VERR_INVALID_PARAMETER);

        pDisk = (PVBOXHDD)RTMemAllocZ(sizeof(VBOXHDD));
        if (pDisk)
        {
            pDisk->u32Signature = VBOXHDDDISK_SIGNATURE;
            pDisk->cImages      = 0;
            pDisk->pBase        = NULL;
            pDisk->pLast        = NULL;
            pDisk->cbSize       = 0;
            pDisk->PCHSGeometry.cCylinders = 0;
            pDisk->PCHSGeometry.cHeads     = 0;
            pDisk->PCHSGeometry.cSectors   = 0;
            pDisk->LCHSGeometry.cCylinders = 0;
            pDisk->LCHSGeometry.cHeads     = 0;
            pDisk->LCHSGeometry.cSectors   = 0;
            pDisk->pVDIfsDisk  = pVDIfsDisk;
            pDisk->pInterfaceError = NULL;
            pDisk->pInterfaceErrorCallbacks = NULL;
            pDisk->pInterfaceThreadSync = NULL;
            pDisk->pInterfaceThreadSyncCallbacks = NULL;
            pDisk->fGrowing = false;
            RTListInit(&pDisk->ListWriteGrowing);

            /* Create the I/O ctx cache */
            rc = RTMemCacheCreate(&pDisk->hMemCacheIoCtx, sizeof(VDIOCTX), 0, UINT32_MAX,
                                  NULL, NULL, NULL, 0);
            if (RT_FAILURE(rc))
            {
                RTMemFree(pDisk);
                break;
            }

            /* Create the I/O task cache */
            rc = RTMemCacheCreate(&pDisk->hMemCacheIoTask, sizeof(VDIOTASK), 0, UINT32_MAX,
                                  NULL, NULL, NULL, 0);
            if (RT_FAILURE(rc))
            {
                RTMemCacheDestroy(pDisk->hMemCacheIoCtx);
                RTMemFree(pDisk);
                break;
            }

            /* Create critical section. */
            rc = RTCritSectInit(&pDisk->CritSect);
            if (RT_FAILURE(rc))
            {
                RTMemCacheDestroy(pDisk->hMemCacheIoCtx);
                RTMemCacheDestroy(pDisk->hMemCacheIoTask);
                RTMemFree(pDisk);
                break;
            }

            pDisk->pInterfaceError = VDInterfaceGet(pVDIfsDisk, VDINTERFACETYPE_ERROR);
            if (pDisk->pInterfaceError)
                pDisk->pInterfaceErrorCallbacks = VDGetInterfaceError(pDisk->pInterfaceError);

            pDisk->pInterfaceThreadSync = VDInterfaceGet(pVDIfsDisk, VDINTERFACETYPE_THREADSYNC);
            if (pDisk->pInterfaceThreadSync)
                pDisk->pInterfaceThreadSyncCallbacks = VDGetInterfaceThreadSync(pDisk->pInterfaceThreadSync);
            pDisk->pInterfaceIO    = VDInterfaceGet(pVDIfsDisk, VDINTERFACETYPE_IO);
            if (pDisk->pInterfaceIO)
                pDisk->pInterfaceIOCallbacks = VDGetInterfaceIO(pDisk->pInterfaceIO);
            else
            {
                /* Create fallback I/O interface */
                pDisk->VDIIOCallbacks.cbSize                 = sizeof(VDINTERFACEIO);
                pDisk->VDIIOCallbacks.enmInterface           = VDINTERFACETYPE_IO;
                pDisk->VDIIOCallbacks.pfnOpen                = vdIOOpenFallback;
                pDisk->VDIIOCallbacks.pfnClose               = vdIOCloseFallback;
                pDisk->VDIIOCallbacks.pfnDelete              = vdIODeleteFallback;
                pDisk->VDIIOCallbacks.pfnMove                = vdIOMoveFallback;
                pDisk->VDIIOCallbacks.pfnGetFreeSpace        = vdIOGetFreeSpaceFallback;
                pDisk->VDIIOCallbacks.pfnGetModificationTime = vdIOGetModificationTimeFallback;
                pDisk->VDIIOCallbacks.pfnGetSize             = vdIOGetSizeFallback;
                pDisk->VDIIOCallbacks.pfnSetSize             = vdIOSetSizeFallback;
                pDisk->VDIIOCallbacks.pfnReadSync            = vdIOReadSyncFallback;
                pDisk->VDIIOCallbacks.pfnWriteSync           = vdIOWriteSyncFallback;
                pDisk->VDIIOCallbacks.pfnFlushSync           = vdIOFlushSyncFallback;
                pDisk->VDIIOCallbacks.pfnReadAsync           = vdIOReadAsyncFallback;
                pDisk->VDIIOCallbacks.pfnWriteAsync          = vdIOWriteAsyncFallback;
                pDisk->VDIIOCallbacks.pfnFlushAsync          = vdIOFlushAsyncFallback;
                pDisk->pInterfaceIOCallbacks                 = &pDisk->VDIIOCallbacks;

                pDisk->VDIIO.pszInterfaceName = "VD_IO";
                pDisk->VDIIO.cbSize           = sizeof(VDINTERFACE);
                pDisk->VDIIO.pNext            = NULL;
                pDisk->VDIIO.enmInterface     = VDINTERFACETYPE_IO;
                pDisk->VDIIO.pvUser           = pDisk;
                pDisk->VDIIO.pCallbacks       = pDisk->pInterfaceIOCallbacks;
                pDisk->pInterfaceIO           = &pDisk->VDIIO;
            }

            /*
             * Create the internal I/O callback table.
             * The interface is per-image but no need to duplicate the
             * callback table every time.
             */
            pDisk->VDIIOIntCallbacks.cbSize                 = sizeof(VDINTERFACEIOINT);
            pDisk->VDIIOIntCallbacks.enmInterface           = VDINTERFACETYPE_IOINT;
            pDisk->VDIIOIntCallbacks.pfnOpen                = vdIOIntOpen;
            pDisk->VDIIOIntCallbacks.pfnClose               = vdIOIntClose;
            pDisk->VDIIOIntCallbacks.pfnDelete              = vdIOIntDelete;
            pDisk->VDIIOIntCallbacks.pfnMove                = vdIOIntMove;
            pDisk->VDIIOIntCallbacks.pfnGetFreeSpace        = vdIOIntGetFreeSpace;
            pDisk->VDIIOIntCallbacks.pfnGetModificationTime = vdIOIntGetModificationTime;
            pDisk->VDIIOIntCallbacks.pfnGetSize             = vdIOIntGetSize;
            pDisk->VDIIOIntCallbacks.pfnSetSize             = vdIOIntSetSize;
            pDisk->VDIIOIntCallbacks.pfnReadSync            = vdIOIntReadSync;
            pDisk->VDIIOIntCallbacks.pfnWriteSync           = vdIOIntWriteSync;
            pDisk->VDIIOIntCallbacks.pfnFlushSync           = vdIOIntFlushSync;
            pDisk->VDIIOIntCallbacks.pfnReadUserAsync       = vdIOIntReadUserAsync;
            pDisk->VDIIOIntCallbacks.pfnWriteUserAsync      = vdIOIntWriteUserAsync;
            pDisk->VDIIOIntCallbacks.pfnReadMetaAsync       = vdIOIntReadMetaAsync;
            pDisk->VDIIOIntCallbacks.pfnWriteMetaAsync      = vdIOIntWriteMetaAsync;
            pDisk->VDIIOIntCallbacks.pfnMetaXferRelease     = vdIOIntMetaXferRelease;
            pDisk->VDIIOIntCallbacks.pfnFlushAsync          = vdIOIntFlushAsync;
            pDisk->VDIIOIntCallbacks.pfnIoCtxCopyFrom       = vdIOIntIoCtxCopyFrom;
            pDisk->VDIIOIntCallbacks.pfnIoCtxCopyTo         = vdIOIntIoCtxCopyTo;
            pDisk->VDIIOIntCallbacks.pfnIoCtxSet            = vdIOIntIoCtxSet;
            pDisk->VDIIOIntCallbacks.pfnIoCtxSegArrayCreate = vdIOIntIoCtxSegArrayCreate;
            pDisk->VDIIOIntCallbacks.pfnIoCtxCompleted      = vdIOIntIoCtxCompleted;

            *ppDisk = pDisk;
        }
        else
        {
            rc = VERR_NO_MEMORY;
            break;
        }
    } while (0);

    LogFlowFunc(("returns %Rrc (pDisk=%#p)\n", rc, pDisk));
    return rc;
}

/**
 * Destroys HDD container.
 * If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(void) VDDestroy(PVBOXHDD pDisk)
{
    LogFlowFunc(("pDisk=%#p\n", pDisk));
    do
    {
        /* sanity check */
        AssertPtrBreak(pDisk);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
        VDCloseAll(pDisk);
        RTMemCacheDestroy(pDisk->hMemCacheIoCtx);
        RTMemCacheDestroy(pDisk->hMemCacheIoTask);
        RTMemFree(pDisk);
    } while (0);
    LogFlowFunc(("returns\n"));
}

/**
 * Try to get the backend name which can use this image.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS if a plugin was found.
 *                       ppszFormat contains the string which can be used as backend name.
 *          VERR_NOT_SUPPORTED if no backend was found.
 * @param   pVDIfsDisk      Pointer to the per-disk VD interface list.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pszFilename     Name of the image file for which the backend is queried.
 * @param   ppszFormat      Receives pointer of the UTF-8 string which contains the format name.
 *                          The returned pointer must be freed using RTStrFree().
 */
VBOXDDU_DECL(int) VDGetFormat(PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                              const char *pszFilename, char **ppszFormat)
{
    int rc = VERR_NOT_SUPPORTED;
    VDINTERFACEIOINT VDIIOIntCallbacks;
    VDINTERFACE      VDIIOInt;
    VDINTERFACEIO    VDIIOCallbacksFallback;
    PVDINTERFACE     pInterfaceIO;
    PVDINTERFACEIO   pInterfaceIOCallbacks;

    LogFlowFunc(("pszFilename=\"%s\"\n", pszFilename));
    /* Check arguments. */
    AssertMsgReturn(VALID_PTR(pszFilename) && *pszFilename,
                    ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(ppszFormat),
                    ("ppszFormat=%#p\n", ppszFormat),
                    VERR_INVALID_PARAMETER);

    if (!g_apBackends)
        VDInit();

    pInterfaceIO = VDInterfaceGet(pVDIfsImage, VDINTERFACETYPE_IO);
    if (!pInterfaceIO)
    {
        /*
         * Caller doesn't provide an I/O interface, create our own using the
         * native file API.
         */
        VDIIOCallbacksFallback.cbSize                    = sizeof(VDINTERFACEIO);
        VDIIOCallbacksFallback.enmInterface              = VDINTERFACETYPE_IO;
        VDIIOCallbacksFallback.pfnOpen                   = vdIOOpenFallback;
        VDIIOCallbacksFallback.pfnClose                  = vdIOCloseFallback;
        VDIIOCallbacksFallback.pfnDelete                 = vdIODeleteFallback;
        VDIIOCallbacksFallback.pfnMove                   = vdIOMoveFallback;
        VDIIOCallbacksFallback.pfnGetFreeSpace           = vdIOGetFreeSpaceFallback;
        VDIIOCallbacksFallback.pfnGetModificationTime    = vdIOGetModificationTimeFallback;
        VDIIOCallbacksFallback.pfnGetSize                = vdIOGetSizeFallback;
        VDIIOCallbacksFallback.pfnSetSize                = vdIOSetSizeFallback;
        VDIIOCallbacksFallback.pfnReadSync               = vdIOReadSyncFallback;
        VDIIOCallbacksFallback.pfnWriteSync              = vdIOWriteSyncFallback;
        VDIIOCallbacksFallback.pfnFlushSync              = vdIOFlushSyncFallback;
        pInterfaceIOCallbacks = &VDIIOCallbacksFallback;
    }
    else
        pInterfaceIOCallbacks = VDGetInterfaceIO(pInterfaceIO);

    VDIIOIntCallbacks.cbSize                    = sizeof(VDINTERFACEIOINT);
    VDIIOIntCallbacks.enmInterface              = VDINTERFACETYPE_IOINT;
    VDIIOIntCallbacks.pfnOpen                   = vdIOIntOpenLimited;
    VDIIOIntCallbacks.pfnClose                  = vdIOIntCloseLimited;
    VDIIOIntCallbacks.pfnDelete                 = vdIOIntDeleteLimited;
    VDIIOIntCallbacks.pfnMove                   = vdIOIntMoveLimited;
    VDIIOIntCallbacks.pfnGetFreeSpace           = vdIOIntGetFreeSpaceLimited;
    VDIIOIntCallbacks.pfnGetModificationTime    = vdIOIntGetModificationTimeLimited;
    VDIIOIntCallbacks.pfnGetSize                = vdIOIntGetSizeLimited;
    VDIIOIntCallbacks.pfnSetSize                = vdIOIntSetSizeLimited;
    VDIIOIntCallbacks.pfnReadSync               = vdIOIntReadSyncLimited;
    VDIIOIntCallbacks.pfnWriteSync              = vdIOIntWriteSyncLimited;
    VDIIOIntCallbacks.pfnFlushSync              = vdIOIntFlushSyncLimited;
    VDIIOIntCallbacks.pfnReadUserAsync          = NULL;
    VDIIOIntCallbacks.pfnWriteUserAsync         = NULL;
    VDIIOIntCallbacks.pfnReadMetaAsync          = NULL;
    VDIIOIntCallbacks.pfnWriteMetaAsync         = NULL;
    VDIIOIntCallbacks.pfnFlushAsync             = NULL;
    rc = VDInterfaceAdd(&VDIIOInt, "VD_IOINT", VDINTERFACETYPE_IOINT,
                        &VDIIOIntCallbacks, pInterfaceIOCallbacks, &pVDIfsImage);
    AssertRC(rc);

    /* Find the backend supporting this file format. */
    for (unsigned i = 0; i < g_cBackends; i++)
    {
        if (g_apBackends[i]->pfnCheckIfValid)
        {
            rc = g_apBackends[i]->pfnCheckIfValid(pszFilename, pVDIfsDisk,
                                                  pVDIfsImage);
            if (    RT_SUCCESS(rc)
                /* The correct backend has been found, but there is a small
                 * incompatibility so that the file cannot be used. Stop here
                 * and signal success - the actual open will of course fail,
                 * but that will create a really sensible error message. */
                ||  (   rc != VERR_VD_GEN_INVALID_HEADER
                     && rc != VERR_VD_VDI_INVALID_HEADER
                     && rc != VERR_VD_VMDK_INVALID_HEADER
                     && rc != VERR_VD_ISCSI_INVALID_HEADER
                     && rc != VERR_VD_VHD_INVALID_HEADER
                     && rc != VERR_VD_RAW_INVALID_HEADER
                     && rc != VERR_VD_DMG_INVALID_HEADER))
            {
                /* Copy the name into the new string. */
                char *pszFormat = RTStrDup(g_apBackends[i]->pszBackendName);
                if (!pszFormat)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                *ppszFormat = pszFormat;
                rc = VINF_SUCCESS;
                break;
            }
            rc = VERR_NOT_SUPPORTED;
        }
    }

    /* Try the cache backends. */
    if (rc == VERR_NOT_SUPPORTED)
    {
        for (unsigned i = 0; i < g_cCacheBackends; i++)
        {
            if (g_apCacheBackends[i]->pfnProbe)
            {
                rc = g_apCacheBackends[i]->pfnProbe(pszFilename, pVDIfsDisk,
                                                    pVDIfsImage);
                if (    RT_SUCCESS(rc)
                    ||  (rc != VERR_VD_GEN_INVALID_HEADER))
                {
                    /* Copy the name into the new string. */
                    char *pszFormat = RTStrDup(g_apBackends[i]->pszBackendName);
                    if (!pszFormat)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                    *ppszFormat = pszFormat;
                    rc = VINF_SUCCESS;
                    break;
                }
                rc = VERR_NOT_SUPPORTED;
            }
        }
    }

    LogFlowFunc(("returns %Rrc *ppszFormat=\"%s\"\n", rc, *ppszFormat));
    return rc;
}

/**
 * Opens an image file.
 *
 * The first opened image file in HDD container must have a base image type,
 * others (next opened images) must be a differencing or undo images.
 * Linkage is checked for differencing image to be in consistence with the previously opened image.
 * When another differencing image is opened and the last image was opened in read/write access
 * mode, then the last image is reopened in read-only with deny write sharing mode. This allows
 * other processes to use images in read-only mode too.
 *
 * Note that the image is opened in read-only mode if a read/write open is not possible.
 * Use VDIsReadOnly to check open mode.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     Name of the image file to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 */
VBOXDDU_DECL(int) VDOpen(PVBOXHDD pDisk, const char *pszBackend,
                         const char *pszFilename, unsigned uOpenFlags,
                         PVDINTERFACE pVDIfsImage)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;
    PVDIMAGE pImage = NULL;

    LogFlowFunc(("pDisk=%#p pszBackend=\"%s\" pszFilename=\"%s\" uOpenFlags=%#x, pVDIfsImage=%#p\n",
                 pDisk, pszBackend, pszFilename, uOpenFlags, pVDIfsImage));

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszBackend) && *pszBackend,
                           ("pszBackend=%#p \"%s\"\n", pszBackend, pszBackend),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(pszFilename) && *pszFilename,
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uOpenFlags & ~VD_OPEN_FLAGS_MASK) == 0,
                           ("uOpenFlags=%#x\n", uOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        /* Set up image descriptor. */
        pImage = (PVDIMAGE)RTMemAllocZ(sizeof(VDIMAGE));
        if (!pImage)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pImage->pszFilename = RTStrDup(pszFilename);
        if (!pImage->pszFilename)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pImage->pDisk       = pDisk;
        pImage->pVDIfsImage = pVDIfsImage;

        rc = vdFindBackend(pszBackend, &pImage->Backend);
        if (RT_FAILURE(rc))
            break;
        if (!pImage->Backend)
        {
            rc = vdError(pDisk, VERR_INVALID_PARAMETER, RT_SRC_POS,
                         N_("VD: unknown backend name '%s'"), pszBackend);
            break;
        }

        /* Set up the I/O interface if necessary. */
        if (!VDInterfaceGet(pVDIfsImage, VDINTERFACETYPE_IOINT))
        {
            rc = VDInterfaceAdd(&pImage->VDIIO, "VD_IOINT", VDINTERFACETYPE_IOINT,
                                &pDisk->VDIIOIntCallbacks, pImage,
                                &pImage->pVDIfsImage);
            AssertRC(rc);
        }

        pImage->uOpenFlags = uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME;
        rc = pImage->Backend->pfnOpen(pImage->pszFilename,
                                      uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME,
                                      pDisk->pVDIfsDisk,
                                      pImage->pVDIfsImage,
                                      &pImage->pBackendData);
        /* If the open in read-write mode failed, retry in read-only mode. */
        if (RT_FAILURE(rc))
        {
            if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY)
                &&  (   rc == VERR_ACCESS_DENIED
                     || rc == VERR_PERMISSION_DENIED
                     || rc == VERR_WRITE_PROTECT
                     || rc == VERR_SHARING_VIOLATION
                     || rc == VERR_FILE_LOCK_FAILED))
                rc = pImage->Backend->pfnOpen(pImage->pszFilename,
                                                (uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME)
                                               | VD_OPEN_FLAGS_READONLY,
                                               pDisk->pVDIfsDisk,
                                               pImage->pVDIfsImage,
                                               &pImage->pBackendData);
            if (RT_FAILURE(rc))
            {
                rc = vdError(pDisk, rc, RT_SRC_POS,
                             N_("VD: error %Rrc opening image file '%s'"), rc, pszFilename);
                break;
            }
        }

        /* Lock disk for writing, as we modify pDisk information below. */
        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        /* Check image type. As the image itself has only partial knowledge
         * whether it's a base image or not, this info is derived here. The
         * base image can be fixed or normal, all others must be normal or
         * diff images. Some image formats don't distinguish between normal
         * and diff images, so this must be corrected here. */
        unsigned uImageFlags;
        uImageFlags = pImage->Backend->pfnGetImageFlags(pImage->pBackendData);
        if (RT_FAILURE(rc))
            uImageFlags = VD_IMAGE_FLAGS_NONE;
        if (    RT_SUCCESS(rc)
            &&  !(uOpenFlags & VD_OPEN_FLAGS_INFO))
        {
            if (    pDisk->cImages == 0
                &&  (uImageFlags & VD_IMAGE_FLAGS_DIFF))
            {
                rc = VERR_VD_INVALID_TYPE;
                break;
            }
            else if (pDisk->cImages != 0)
            {
                if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
                {
                    rc = VERR_VD_INVALID_TYPE;
                    break;
                }
                else
                    uImageFlags |= VD_IMAGE_FLAGS_DIFF;
            }
        }

        /* Ensure we always get correct diff information, even if the backend
         * doesn't actually have a stored flag for this. It must not return
         * bogus information for the parent UUID if it is not a diff image. */
        RTUUID parentUuid;
        RTUuidClear(&parentUuid);
        rc2 = pImage->Backend->pfnGetParentUuid(pImage->pBackendData, &parentUuid);
        if (RT_SUCCESS(rc2) && !RTUuidIsNull(&parentUuid))
            uImageFlags |= VD_IMAGE_FLAGS_DIFF;

        pImage->uImageFlags = uImageFlags;

        /* Force sane optimization settings. It's not worth avoiding writes
         * to fixed size images. The overhead would have almost no payback. */
        if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
            pImage->uOpenFlags |= VD_OPEN_FLAGS_HONOR_SAME;

        /** @todo optionally check UUIDs */

        /* Cache disk information. */
        pDisk->cbSize = pImage->Backend->pfnGetSize(pImage->pBackendData);

        /* Cache PCHS geometry. */
        rc2 = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData,
                                                  &pDisk->PCHSGeometry);
        if (RT_FAILURE(rc2))
        {
            pDisk->PCHSGeometry.cCylinders = 0;
            pDisk->PCHSGeometry.cHeads = 0;
            pDisk->PCHSGeometry.cSectors = 0;
        }
        else
        {
            /* Make sure the PCHS geometry is properly clipped. */
            pDisk->PCHSGeometry.cCylinders = RT_MIN(pDisk->PCHSGeometry.cCylinders, 16383);
            pDisk->PCHSGeometry.cHeads = RT_MIN(pDisk->PCHSGeometry.cHeads, 16);
            pDisk->PCHSGeometry.cSectors = RT_MIN(pDisk->PCHSGeometry.cSectors, 63);
        }

        /* Cache LCHS geometry. */
        rc2 = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData,
                                                  &pDisk->LCHSGeometry);
        if (RT_FAILURE(rc2))
        {
            pDisk->LCHSGeometry.cCylinders = 0;
            pDisk->LCHSGeometry.cHeads = 0;
            pDisk->LCHSGeometry.cSectors = 0;
        }
        else
        {
            /* Make sure the LCHS geometry is properly clipped. */
            pDisk->LCHSGeometry.cHeads = RT_MIN(pDisk->LCHSGeometry.cHeads, 255);
            pDisk->LCHSGeometry.cSectors = RT_MIN(pDisk->LCHSGeometry.cSectors, 63);
        }

        if (pDisk->cImages != 0)
        {
            /* Switch previous image to read-only mode. */
            unsigned uOpenFlagsPrevImg;
            uOpenFlagsPrevImg = pDisk->pLast->Backend->pfnGetOpenFlags(pDisk->pLast->pBackendData);
            if (!(uOpenFlagsPrevImg & VD_OPEN_FLAGS_READONLY))
            {
                uOpenFlagsPrevImg |= VD_OPEN_FLAGS_READONLY;
                rc = pDisk->pLast->Backend->pfnSetOpenFlags(pDisk->pLast->pBackendData, uOpenFlagsPrevImg);
            }
        }

        if (RT_SUCCESS(rc))
        {
            /* Image successfully opened, make it the last image. */
            vdAddImageToList(pDisk, pImage);
            if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
                pDisk->uModified = VD_IMAGE_MODIFIED_FIRST;
        }
        else
        {
            /* Error detected, but image opened. Close image. */
            rc2 = pImage->Backend->pfnClose(pImage->pBackendData, false);
            AssertRC(rc2);
            pImage->pBackendData = NULL;
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        if (pImage)
        {
            if (pImage->pszFilename)
                RTStrFree(pImage->pszFilename);
            RTMemFree(pImage);
        }
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Opens a cache image.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container which should use the cache image.
 * @param   pszBackend      Name of the cache file backend to use (case insensitive).
 * @param   pszFilename     Name of the cache image to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsCache     Pointer to the per-cache VD interface list.
 */
VBOXDDU_DECL(int) VDCacheOpen(PVBOXHDD pDisk, const char *pszBackend,
                              const char *pszFilename, unsigned uOpenFlags,
                              PVDINTERFACE pVDIfsCache)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;
    PVDCACHE pCache = NULL;

    LogFlowFunc(("pDisk=%#p pszBackend=\"%s\" pszFilename=\"%s\" uOpenFlags=%#x, pVDIfsCache=%#p\n",
                 pDisk, pszBackend, pszFilename, uOpenFlags, pVDIfsCache));

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszBackend) && *pszBackend,
                           ("pszBackend=%#p \"%s\"\n", pszBackend, pszBackend),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(pszFilename) && *pszFilename,
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uOpenFlags & ~VD_OPEN_FLAGS_MASK) == 0,
                           ("uOpenFlags=%#x\n", uOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        /* Set up image descriptor. */
        pCache = (PVDCACHE)RTMemAllocZ(sizeof(VDCACHE));
        if (!pCache)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pCache->pszFilename = RTStrDup(pszFilename);
        if (!pCache->pszFilename)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pCache->pDisk       = pDisk;
        pCache->pVDIfsCache = pVDIfsCache;

        rc = vdFindCacheBackend(pszBackend, &pCache->Backend);
        if (RT_FAILURE(rc))
            break;
        if (!pCache->Backend)
        {
            rc = vdError(pDisk, VERR_INVALID_PARAMETER, RT_SRC_POS,
                         N_("VD: unknown backend name '%s'"), pszBackend);
            break;
        }

        /* Set up the I/O interface. */
        rc = VDInterfaceAdd(&pCache->VDIIO, "VD_IOINT", VDINTERFACETYPE_IOINT,
                            &pDisk->VDIIOIntCallbacks, pCache, &pCache->pVDIfsCache);
        AssertRC(rc);

        pCache->uOpenFlags = uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME;
        rc = pCache->Backend->pfnOpen(pCache->pszFilename,
                                      uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME,
                                      pDisk->pVDIfsDisk,
                                      pCache->pVDIfsCache,
                                      &pCache->pBackendData);
        /* If the open in read-write mode failed, retry in read-only mode. */
        if (RT_FAILURE(rc))
        {
            if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY)
                &&  (   rc == VERR_ACCESS_DENIED
                     || rc == VERR_PERMISSION_DENIED
                     || rc == VERR_WRITE_PROTECT
                     || rc == VERR_SHARING_VIOLATION
                     || rc == VERR_FILE_LOCK_FAILED))
                rc = pCache->Backend->pfnOpen(pCache->pszFilename,
                                                (uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME)
                                               | VD_OPEN_FLAGS_READONLY,
                                               pDisk->pVDIfsDisk,
                                               pCache->pVDIfsCache,
                                               &pCache->pBackendData);
            if (RT_FAILURE(rc))
            {
                rc = vdError(pDisk, rc, RT_SRC_POS,
                             N_("VD: error %Rrc opening image file '%s'"), rc, pszFilename);
                break;
            }
        }

        /* Lock disk for writing, as we modify pDisk information below. */
        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        /*
         * Check that the modification UUID of the cache and last image
         * match. If not the image was modified inbetween without the cache.
         * The cache might contain stale data.
         */
        RTUUID UuidImage, UuidCache;

        rc = pCache->Backend->pfnGetModificationUuid(pCache->pBackendData,
                                                     &UuidCache);
        if (RT_SUCCESS(rc))
        {
            rc = pDisk->pLast->Backend->pfnGetModificationUuid(pDisk->pLast->pBackendData,
                                                               &UuidImage);
            if (RT_SUCCESS(rc))
            {
                if (RTUuidCompare(&UuidImage, &UuidCache))
                    rc = VERR_VD_CACHE_NOT_UP_TO_DATE;
            }
        }

        /*
         * We assume that the user knows what he is doing if one of the images
         * doesn't support the modification uuid.
         */
        if (rc == VERR_NOT_SUPPORTED)
            rc = VINF_SUCCESS;

        if (RT_SUCCESS(rc))
        {
            /* Cache successfully opened, make it the current one. */
            if (!pDisk->pCache)
                pDisk->pCache = pCache;
            else
                rc = VERR_VD_CACHE_ALREADY_EXISTS;
        }

        if (RT_FAILURE(rc))
        {
            /* Error detected, but image opened. Close image. */
            rc2 = pCache->Backend->pfnClose(pCache->pBackendData, false);
            AssertRC(rc2);
            pCache->pBackendData = NULL;
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        if (pCache)
        {
            if (pCache->pszFilename)
                RTStrFree(pCache->pszFilename);
            RTMemFree(pCache);
        }
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Creates and opens a new base image file.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     Name of the image file to create.
 * @param   cbSize          Image size in bytes.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pPCHSGeometry   Pointer to physical disk geometry <= (16383,16,63). Not NULL.
 * @param   pLCHSGeometry   Pointer to logical disk geometry <= (x,255,63). Not NULL.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCreateBase(PVBOXHDD pDisk, const char *pszBackend,
                               const char *pszFilename, uint64_t cbSize,
                               unsigned uImageFlags, const char *pszComment,
                               PCVDGEOMETRY pPCHSGeometry,
                               PCVDGEOMETRY pLCHSGeometry,
                               PCRTUUID pUuid, unsigned uOpenFlags,
                               PVDINTERFACE pVDIfsImage,
                               PVDINTERFACE pVDIfsOperation)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false, fLockRead = false;
    PVDIMAGE pImage = NULL;
    RTUUID uuid;

    LogFlowFunc(("pDisk=%#p pszBackend=\"%s\" pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" PCHS=%u/%u/%u LCHS=%u/%u/%u Uuid=%RTuuid uOpenFlags=%#x pVDIfsImage=%#p pVDIfsOperation=%#p\n",
                 pDisk, pszBackend, pszFilename, cbSize, uImageFlags, pszComment,
                 pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads,
                 pPCHSGeometry->cSectors, pLCHSGeometry->cCylinders,
                 pLCHSGeometry->cHeads, pLCHSGeometry->cSectors, pUuid,
                 uOpenFlags, pVDIfsImage, pVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszBackend) && *pszBackend,
                           ("pszBackend=%#p \"%s\"\n", pszBackend, pszBackend),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(pszFilename) && *pszFilename,
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cbSize,
                           ("cbSize=%llu\n", cbSize),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(   ((uImageFlags & ~VD_IMAGE_FLAGS_MASK) == 0)
                           || ((uImageFlags & (VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF)) != VD_IMAGE_FLAGS_FIXED),
                           ("uImageFlags=%#x\n", uImageFlags),
                           rc = VERR_INVALID_PARAMETER);
        /* The PCHS geometry fields may be 0 to leave it for later. */
        AssertMsgBreakStmt(   VALID_PTR(pPCHSGeometry)
                           && pPCHSGeometry->cHeads <= 16
                           && pPCHSGeometry->cSectors <= 63,
                           ("pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pPCHSGeometry,
                            pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads,
                            pPCHSGeometry->cSectors),
                           rc = VERR_INVALID_PARAMETER);
        /* The LCHS geometry fields may be 0 to leave it to later autodetection. */
        AssertMsgBreakStmt(   VALID_PTR(pLCHSGeometry)
                           && pLCHSGeometry->cHeads <= 255
                           && pLCHSGeometry->cSectors <= 63,
                           ("pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pLCHSGeometry,
                            pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads,
                            pLCHSGeometry->cSectors),
                           rc = VERR_INVALID_PARAMETER);
        /* The UUID may be NULL. */
        AssertMsgBreakStmt(pUuid == NULL || VALID_PTR(pUuid),
                           ("pUuid=%#p UUID=%RTuuid\n", pUuid, pUuid),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uOpenFlags & ~VD_OPEN_FLAGS_MASK) == 0,
                           ("uOpenFlags=%#x\n", uOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        /* Check state. Needs a temporary read lock. Holding the write lock
         * all the time would be blocking other activities for too long. */
        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;
        AssertMsgBreakStmt(pDisk->cImages == 0,
                           ("Create base image cannot be done with other images open\n"),
                           rc = VERR_VD_INVALID_STATE);
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
        fLockRead = false;

        /* Set up image descriptor. */
        pImage = (PVDIMAGE)RTMemAllocZ(sizeof(VDIMAGE));
        if (!pImage)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pImage->pszFilename = RTStrDup(pszFilename);
        if (!pImage->pszFilename)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pImage->pDisk       = pDisk;
        pImage->pVDIfsImage = pVDIfsImage;

        rc = VDInterfaceAdd(&pImage->VDIIO, "VD_IOINT", VDINTERFACETYPE_IOINT,
                            &pDisk->VDIIOIntCallbacks, pImage,
                            &pImage->pVDIfsImage);
        AssertRC(rc);

        rc = vdFindBackend(pszBackend, &pImage->Backend);
        if (RT_FAILURE(rc))
            break;
        if (!pImage->Backend)
        {
            rc = vdError(pDisk, VERR_INVALID_PARAMETER, RT_SRC_POS,
                         N_("VD: unknown backend name '%s'"), pszBackend);
            break;
        }

        /* Create UUID if the caller didn't specify one. */
        if (!pUuid)
        {
            rc = RTUuidCreate(&uuid);
            if (RT_FAILURE(rc))
            {
                rc = vdError(pDisk, rc, RT_SRC_POS,
                             N_("VD: cannot generate UUID for image '%s'"),
                             pszFilename);
                break;
            }
            pUuid = &uuid;
        }

        pImage->uOpenFlags = uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME;
        uImageFlags &= ~VD_IMAGE_FLAGS_DIFF;
        rc = pImage->Backend->pfnCreate(pImage->pszFilename, cbSize,
                                        uImageFlags, pszComment, pPCHSGeometry,
                                        pLCHSGeometry, pUuid,
                                        uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME,
                                        0, 99,
                                        pDisk->pVDIfsDisk,
                                        pImage->pVDIfsImage,
                                        pVDIfsOperation,
                                        &pImage->pBackendData);

        if (RT_SUCCESS(rc))
        {
            pImage->uImageFlags = uImageFlags;

            /* Force sane optimization settings. It's not worth avoiding writes
             * to fixed size images. The overhead would have almost no payback. */
            if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
                pImage->uOpenFlags |= VD_OPEN_FLAGS_HONOR_SAME;

            /* Lock disk for writing, as we modify pDisk information below. */
            rc2 = vdThreadStartWrite(pDisk);
            AssertRC(rc2);
            fLockWrite = true;

            /** @todo optionally check UUIDs */

            /* Re-check state, as the lock wasn't held and another image
             * creation call could have been done by another thread. */
            AssertMsgStmt(pDisk->cImages == 0,
                          ("Create base image cannot be done with other images open\n"),
                          rc = VERR_VD_INVALID_STATE);
        }

        if (RT_SUCCESS(rc))
        {
            /* Cache disk information. */
            pDisk->cbSize = pImage->Backend->pfnGetSize(pImage->pBackendData);

            /* Cache PCHS geometry. */
            rc2 = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData,
                                                      &pDisk->PCHSGeometry);
            if (RT_FAILURE(rc2))
            {
                pDisk->PCHSGeometry.cCylinders = 0;
                pDisk->PCHSGeometry.cHeads = 0;
                pDisk->PCHSGeometry.cSectors = 0;
            }
            else
            {
                /* Make sure the CHS geometry is properly clipped. */
                pDisk->PCHSGeometry.cCylinders = RT_MIN(pDisk->PCHSGeometry.cCylinders, 16383);
                pDisk->PCHSGeometry.cHeads = RT_MIN(pDisk->PCHSGeometry.cHeads, 16);
                pDisk->PCHSGeometry.cSectors = RT_MIN(pDisk->PCHSGeometry.cSectors, 63);
            }

            /* Cache LCHS geometry. */
            rc2 = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData,
                                                      &pDisk->LCHSGeometry);
            if (RT_FAILURE(rc2))
            {
                pDisk->LCHSGeometry.cCylinders = 0;
                pDisk->LCHSGeometry.cHeads = 0;
                pDisk->LCHSGeometry.cSectors = 0;
            }
            else
            {
                /* Make sure the CHS geometry is properly clipped. */
                pDisk->LCHSGeometry.cHeads = RT_MIN(pDisk->LCHSGeometry.cHeads, 255);
                pDisk->LCHSGeometry.cSectors = RT_MIN(pDisk->LCHSGeometry.cSectors, 63);
            }

            /* Image successfully opened, make it the last image. */
            vdAddImageToList(pDisk, pImage);
            if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
                pDisk->uModified = VD_IMAGE_MODIFIED_FIRST;
        }
        else
        {
            /* Error detected, but image opened. Close and delete image. */
            rc2 = pImage->Backend->pfnClose(pImage->pBackendData, true);
            AssertRC(rc2);
            pImage->pBackendData = NULL;
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        if (pImage)
        {
            if (pImage->pszFilename)
                RTStrFree(pImage->pszFilename);
            RTMemFree(pImage);
        }
    }

    if (RT_SUCCESS(rc) && pCbProgress && pCbProgress->pfnProgress)
        pCbProgress->pfnProgress(pIfProgress->pvUser, 100);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VDOpen function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 * @param   pParentUuid     New parent UUID of the image. If NULL, the UUID is queried automatically.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCreateDiff(PVBOXHDD pDisk, const char *pszBackend,
                               const char *pszFilename, unsigned uImageFlags,
                               const char *pszComment, PCRTUUID pUuid,
                               PCRTUUID pParentUuid, unsigned uOpenFlags,
                               PVDINTERFACE pVDIfsImage,
                               PVDINTERFACE pVDIfsOperation)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false, fLockRead = false;
    PVDIMAGE pImage = NULL;
    RTUUID uuid;

    LogFlowFunc(("pDisk=%#p pszBackend=\"%s\" pszFilename=\"%s\" uImageFlags=%#x pszComment=\"%s\" Uuid=%RTuuid uOpenFlags=%#x pVDIfsImage=%#p pVDIfsOperation=%#p\n",
                 pDisk, pszBackend, pszFilename, uImageFlags, pszComment, pUuid, uOpenFlags, pVDIfsImage, pVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszBackend) && *pszBackend,
                           ("pszBackend=%#p \"%s\"\n", pszBackend, pszBackend),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(pszFilename) && *pszFilename,
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uImageFlags & ~VD_IMAGE_FLAGS_MASK) == 0,
                           ("uImageFlags=%#x\n", uImageFlags),
                           rc = VERR_INVALID_PARAMETER);
        /* The UUID may be NULL. */
        AssertMsgBreakStmt(pUuid == NULL || VALID_PTR(pUuid),
                           ("pUuid=%#p UUID=%RTuuid\n", pUuid, pUuid),
                           rc = VERR_INVALID_PARAMETER);
        /* The parent UUID may be NULL. */
        AssertMsgBreakStmt(pParentUuid == NULL || VALID_PTR(pParentUuid),
                           ("pParentUuid=%#p ParentUUID=%RTuuid\n", pParentUuid, pParentUuid),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uOpenFlags & ~VD_OPEN_FLAGS_MASK) == 0,
                           ("uOpenFlags=%#x\n", uOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        /* Check state. Needs a temporary read lock. Holding the write lock
         * all the time would be blocking other activities for too long. */
        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;
        AssertMsgBreakStmt(pDisk->cImages != 0,
                           ("Create diff image cannot be done without other images open\n"),
                           rc = VERR_VD_INVALID_STATE);
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
        fLockRead = false;

        /* Set up image descriptor. */
        pImage = (PVDIMAGE)RTMemAllocZ(sizeof(VDIMAGE));
        if (!pImage)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pImage->pszFilename = RTStrDup(pszFilename);
        if (!pImage->pszFilename)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = vdFindBackend(pszBackend, &pImage->Backend);
        if (RT_FAILURE(rc))
            break;
        if (!pImage->Backend)
        {
            rc = vdError(pDisk, VERR_INVALID_PARAMETER, RT_SRC_POS,
                         N_("VD: unknown backend name '%s'"), pszBackend);
            break;
        }

        pImage->pDisk       = pDisk;
        pImage->pVDIfsImage = pVDIfsImage;

        rc = VDInterfaceAdd(&pImage->VDIIO, "VD_IOINT", VDINTERFACETYPE_IOINT,
                            &pDisk->VDIIOIntCallbacks, pImage,
                            &pImage->pVDIfsImage);
        AssertRC(rc);

        /* Create UUID if the caller didn't specify one. */
        if (!pUuid)
        {
            rc = RTUuidCreate(&uuid);
            if (RT_FAILURE(rc))
            {
                rc = vdError(pDisk, rc, RT_SRC_POS,
                             N_("VD: cannot generate UUID for image '%s'"),
                             pszFilename);
                break;
            }
            pUuid = &uuid;
        }

        pImage->uOpenFlags = uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME;
        uImageFlags |= VD_IMAGE_FLAGS_DIFF;
        rc = pImage->Backend->pfnCreate(pImage->pszFilename, pDisk->cbSize,
                                        uImageFlags | VD_IMAGE_FLAGS_DIFF,
                                        pszComment, &pDisk->PCHSGeometry,
                                        &pDisk->LCHSGeometry, pUuid,
                                        uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME,
                                        0, 99,
                                        pDisk->pVDIfsDisk,
                                        pImage->pVDIfsImage,
                                        pVDIfsOperation,
                                        &pImage->pBackendData);

        if (RT_SUCCESS(rc))
        {
            pImage->uImageFlags = uImageFlags;

            /* Lock disk for writing, as we modify pDisk information below. */
            rc2 = vdThreadStartWrite(pDisk);
            AssertRC(rc2);
            fLockWrite = true;

            /* Switch previous image to read-only mode. */
            unsigned uOpenFlagsPrevImg;
            uOpenFlagsPrevImg = pDisk->pLast->Backend->pfnGetOpenFlags(pDisk->pLast->pBackendData);
            if (!(uOpenFlagsPrevImg & VD_OPEN_FLAGS_READONLY))
            {
                uOpenFlagsPrevImg |= VD_OPEN_FLAGS_READONLY;
                rc = pDisk->pLast->Backend->pfnSetOpenFlags(pDisk->pLast->pBackendData, uOpenFlagsPrevImg);
            }

            /** @todo optionally check UUIDs */

            /* Re-check state, as the lock wasn't held and another image
             * creation call could have been done by another thread. */
            AssertMsgStmt(pDisk->cImages != 0,
                          ("Create diff image cannot be done without other images open\n"),
                          rc = VERR_VD_INVALID_STATE);
        }

        if (RT_SUCCESS(rc))
        {
            RTUUID Uuid;
            RTTIMESPEC ts;

            if (pParentUuid && !RTUuidIsNull(pParentUuid))
            {
                Uuid = *pParentUuid;
                pImage->Backend->pfnSetParentUuid(pImage->pBackendData, &Uuid);
            }
            else
            {
                rc2 = pDisk->pLast->Backend->pfnGetUuid(pDisk->pLast->pBackendData,
                                                        &Uuid);
                if (RT_SUCCESS(rc2))
                    pImage->Backend->pfnSetParentUuid(pImage->pBackendData, &Uuid);
            }
            rc2 = pDisk->pLast->Backend->pfnGetModificationUuid(pDisk->pLast->pBackendData,
                                                                &Uuid);
            if (RT_SUCCESS(rc2))
                pImage->Backend->pfnSetParentModificationUuid(pImage->pBackendData,
                                                              &Uuid);
            if (pDisk->pLast->Backend->pfnGetTimeStamp)
                rc2 = pDisk->pLast->Backend->pfnGetTimeStamp(pDisk->pLast->pBackendData,
                                                             &ts);
            else
                rc = VERR_NOT_IMPLEMENTED;
            if (RT_SUCCESS(rc2) && pImage->Backend->pfnSetParentTimeStamp)
                pImage->Backend->pfnSetParentTimeStamp(pImage->pBackendData, &ts);

            if (pImage->Backend->pfnSetParentFilename)
                rc2 = pImage->Backend->pfnSetParentFilename(pImage->pBackendData, pDisk->pLast->pszFilename);
        }

        if (RT_SUCCESS(rc))
        {
            /* Image successfully opened, make it the last image. */
            vdAddImageToList(pDisk, pImage);
            if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
                pDisk->uModified = VD_IMAGE_MODIFIED_FIRST;
        }
        else
        {
            /* Error detected, but image opened. Close and delete image. */
            rc2 = pImage->Backend->pfnClose(pImage->pBackendData, true);
            AssertRC(rc2);
            pImage->pBackendData = NULL;
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        if (pImage)
        {
            if (pImage->pszFilename)
                RTStrFree(pImage->pszFilename);
            RTMemFree(pImage);
        }
    }

    if (RT_SUCCESS(rc) && pCbProgress && pCbProgress->pfnProgress)
        pCbProgress->pfnProgress(pIfProgress->pvUser, 100);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * Creates and opens new cache image file in HDD container.
 *
 * @return  VBox status code.
 * @param   pDisk           Name of the cache file backend to use (case insensitive).
 * @param   pszFilename     Name of the differencing cache file to create.
 * @param   cbSize          Maximum size of the cache.
 * @param   uImageFlags     Flags specifying special cache features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsCache     Pointer to the per-cache VD interface list.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCreateCache(PVBOXHDD pDisk, const char *pszBackend,
                                const char *pszFilename, uint64_t cbSize,
                                unsigned uImageFlags, const char *pszComment,
                                PCRTUUID pUuid, unsigned uOpenFlags,
                                PVDINTERFACE pVDIfsCache, PVDINTERFACE pVDIfsOperation)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false, fLockRead = false;
    PVDCACHE pCache = NULL;
    RTUUID uuid;

    LogFlowFunc(("pDisk=%#p pszBackend=\"%s\" pszFilename=\"%s\" cbSIze=%llu uImageFlags=%#x pszComment=\"%s\" Uuid=%RTuuid uOpenFlags=%#x pVDIfsImage=%#p pVDIfsOperation=%#p\n",
                 pDisk, pszBackend, pszFilename, cbSize, uImageFlags, pszComment, pUuid, uOpenFlags, pVDIfsCache, pVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszBackend) && *pszBackend,
                           ("pszBackend=%#p \"%s\"\n", pszBackend, pszBackend),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(pszFilename) && *pszFilename,
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cbSize,
                           ("cbSize=%llu\n", cbSize),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uImageFlags & ~VD_IMAGE_FLAGS_MASK) == 0,
                           ("uImageFlags=%#x\n", uImageFlags),
                           rc = VERR_INVALID_PARAMETER);
        /* The UUID may be NULL. */
        AssertMsgBreakStmt(pUuid == NULL || VALID_PTR(pUuid),
                           ("pUuid=%#p UUID=%RTuuid\n", pUuid, pUuid),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt((uOpenFlags & ~VD_OPEN_FLAGS_MASK) == 0,
                           ("uOpenFlags=%#x\n", uOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        /* Check state. Needs a temporary read lock. Holding the write lock
         * all the time would be blocking other activities for too long. */
        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;
        AssertMsgBreakStmt(pDisk->cImages != 0,
                           ("Create diff image cannot be done without other images open\n"),
                           rc = VERR_VD_INVALID_STATE);
        AssertMsgBreakStmt(!pDisk->pCache,
                           ("Create cache image cannot be done with a cache already attached\n"),
                           rc = VERR_VD_CACHE_ALREADY_EXISTS);
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
        fLockRead = false;

        /* Set up image descriptor. */
        pCache = (PVDCACHE)RTMemAllocZ(sizeof(VDCACHE));
        if (!pCache)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pCache->pszFilename = RTStrDup(pszFilename);
        if (!pCache->pszFilename)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = vdFindCacheBackend(pszBackend, &pCache->Backend);
        if (RT_FAILURE(rc))
            break;
        if (!pCache->Backend)
        {
            rc = vdError(pDisk, VERR_INVALID_PARAMETER, RT_SRC_POS,
                         N_("VD: unknown backend name '%s'"), pszBackend);
            break;
        }

        pCache->pDisk       = pDisk;
        pCache->pVDIfsCache = pVDIfsCache;

        /* Set up the I/O interface. */
        rc = VDInterfaceAdd(&pCache->VDIIO, "VD_IOINT", VDINTERFACETYPE_IOINT,
                            &pDisk->VDIIOIntCallbacks, pCache, &pCache->pVDIfsCache);
        AssertRC(rc);

        /* Create UUID if the caller didn't specify one. */
        if (!pUuid)
        {
            rc = RTUuidCreate(&uuid);
            if (RT_FAILURE(rc))
            {
                rc = vdError(pDisk, rc, RT_SRC_POS,
                             N_("VD: cannot generate UUID for image '%s'"),
                             pszFilename);
                break;
            }
            pUuid = &uuid;
        }

        pCache->uOpenFlags = uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME;
        rc = pCache->Backend->pfnCreate(pCache->pszFilename, cbSize,
                                        uImageFlags,
                                        pszComment, pUuid,
                                        uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME,
                                        0, 99,
                                        pDisk->pVDIfsDisk,
                                        pCache->pVDIfsCache,
                                        pVDIfsOperation,
                                        &pCache->pBackendData);

        if (RT_SUCCESS(rc))
        {
            /* Lock disk for writing, as we modify pDisk information below. */
            rc2 = vdThreadStartWrite(pDisk);
            AssertRC(rc2);
            fLockWrite = true;

            /* Re-check state, as the lock wasn't held and another image
             * creation call could have been done by another thread. */
            AssertMsgStmt(!pDisk->pCache,
                          ("Create cache image cannot be done with another cache open\n"),
                          rc = VERR_VD_CACHE_ALREADY_EXISTS);
        }

        if (RT_SUCCESS(rc))
        {
            RTUUID UuidModification;

            /* Set same modification Uuid as the last image. */
            rc = pDisk->pLast->Backend->pfnGetModificationUuid(pDisk->pLast->pBackendData,
                                                               &UuidModification);
            if (RT_SUCCESS(rc))
            {
                rc = pCache->Backend->pfnSetModificationUuid(pCache->pBackendData,
                                                             &UuidModification);
            }

            if (rc == VERR_NOT_SUPPORTED)
                rc = VINF_SUCCESS;
        }

        if (RT_SUCCESS(rc))
        {
            /* Cache successfully created. */
            pDisk->pCache = pCache;
        }
        else
        {
            /* Error detected, but image opened. Close and delete image. */
            rc2 = pCache->Backend->pfnClose(pCache->pBackendData, true);
            AssertRC(rc2);
            pCache->pBackendData = NULL;
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        if (pCache)
        {
            if (pCache->pszFilename)
                RTStrFree(pCache->pszFilename);
            RTMemFree(pCache);
        }
    }

    if (RT_SUCCESS(rc) && pCbProgress && pCbProgress->pfnProgress)
        pCbProgress->pfnProgress(pIfProgress->pvUser, 100);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Merges two images (not necessarily with direct parent/child relationship).
 * As a side effect the source image and potentially the other images which
 * are also merged to the destination are deleted from both the disk and the
 * images in the HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImageFrom      Name of the image file to merge from.
 * @param   nImageTo        Name of the image file to merge to.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDMerge(PVBOXHDD pDisk, unsigned nImageFrom,
                          unsigned nImageTo, PVDINTERFACE pVDIfsOperation)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false, fLockRead = false;
    void *pvBuf = NULL;

    LogFlowFunc(("pDisk=%#p nImageFrom=%u nImageTo=%u pVDIfsOperation=%#p\n",
                 pDisk, nImageFrom, nImageTo, pVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* For simplicity reasons lock for writing as the image reopen below
         * might need it. After all the reopen is usually needed. */
        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;
        PVDIMAGE pImageFrom = vdGetImageByNumber(pDisk, nImageFrom);
        PVDIMAGE pImageTo = vdGetImageByNumber(pDisk, nImageTo);
        if (!pImageFrom || !pImageTo)
        {
            rc = VERR_VD_IMAGE_NOT_FOUND;
            break;
        }
        AssertBreakStmt(pImageFrom != pImageTo, rc = VERR_INVALID_PARAMETER);

        /* Make sure destination image is writable. */
        unsigned uOpenFlags = pImageTo->Backend->pfnGetOpenFlags(pImageTo->pBackendData);
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            uOpenFlags &= ~VD_OPEN_FLAGS_READONLY;
            rc = pImageTo->Backend->pfnSetOpenFlags(pImageTo->pBackendData,
                                                    uOpenFlags);
            if (RT_FAILURE(rc))
                break;
        }

        /* Get size of destination image. */
        uint64_t cbSize = pImageTo->Backend->pfnGetSize(pImageTo->pBackendData);
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = false;

        /* Allocate tmp buffer. */
        pvBuf = RTMemTmpAlloc(VD_MERGE_BUFFER_SIZE);
        if (!pvBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        /* Merging is done directly on the images itself. This potentially
         * causes trouble if the disk is full in the middle of operation. */
        if (nImageFrom < nImageTo)
        {
            /* Merge parent state into child. This means writing all not
             * allocated blocks in the destination image which are allocated in
             * the images to be merged. */
            uint64_t uOffset = 0;
            uint64_t cbRemaining = cbSize;
            do
            {
                size_t cbThisRead = RT_MIN(VD_MERGE_BUFFER_SIZE, cbRemaining);

                /* Need to hold the write lock during a read-write operation. */
                rc2 = vdThreadStartWrite(pDisk);
                AssertRC(rc2);
                fLockWrite = true;

                rc = pImageTo->Backend->pfnRead(pImageTo->pBackendData,
                                                uOffset, pvBuf, cbThisRead,
                                                &cbThisRead);
                if (rc == VERR_VD_BLOCK_FREE)
                {
                    /* Search for image with allocated block. Do not attempt to
                     * read more than the previous reads marked as valid.
                     * Otherwise this would return stale data when different
                     * block sizes are used for the images. */
                    for (PVDIMAGE pCurrImage = pImageTo->pPrev;
                         pCurrImage != NULL && pCurrImage != pImageFrom->pPrev && rc == VERR_VD_BLOCK_FREE;
                         pCurrImage = pCurrImage->pPrev)
                    {
                        rc = pCurrImage->Backend->pfnRead(pCurrImage->pBackendData,
                                                          uOffset, pvBuf,
                                                          cbThisRead,
                                                          &cbThisRead);
                    }

                    if (rc != VERR_VD_BLOCK_FREE)
                    {
                        if (RT_FAILURE(rc))
                            break;
                        /* Updating the cache is required because this might be a live merge. */
                        rc = vdWriteHelper(pDisk, pImageTo, pImageFrom->pPrev,
                                           uOffset, pvBuf, cbThisRead,
                                           true /* fUpdateCache */);
                        if (RT_FAILURE(rc))
                            break;
                    }
                    else
                        rc = VINF_SUCCESS;
                }
                else if (RT_FAILURE(rc))
                    break;

                rc2 = vdThreadFinishWrite(pDisk);
                AssertRC(rc2);
                fLockWrite = false;

                uOffset += cbThisRead;
                cbRemaining -= cbThisRead;

                if (pCbProgress && pCbProgress->pfnProgress)
                {
                    /** @todo r=klaus: this can update the progress to the same
                     * percentage over and over again if the image format makes
                     * relatively small increments. */
                    rc = pCbProgress->pfnProgress(pIfProgress->pvUser,
                                                  uOffset * 99 / cbSize);
                    if (RT_FAILURE(rc))
                        break;
                }
            } while (uOffset < cbSize);
        }
        else
        {
            /*
             * We may need to update the parent uuid of the child coming after the
             * last image to be merged. We have to reopen it read/write.
             *
             * This is done before we do the actual merge to prevent an incosistent
             * chain if the mode change fails for some reason.
             */
            if (pImageFrom->pNext)
            {
                PVDIMAGE pImageChild = pImageFrom->pNext;

                /* Take the write lock. */
                rc2 = vdThreadStartWrite(pDisk);
                AssertRC(rc2);
                fLockWrite = true;

                /* We need to open the image in read/write mode. */
                uOpenFlags = pImageChild->Backend->pfnGetOpenFlags(pImageChild->pBackendData);

                if (uOpenFlags  & VD_OPEN_FLAGS_READONLY)
                {
                    uOpenFlags  &= ~VD_OPEN_FLAGS_READONLY;
                    rc = pImageChild->Backend->pfnSetOpenFlags(pImageChild->pBackendData,
                                                               uOpenFlags);
                    if (RT_FAILURE(rc))
                        break;
                }

                rc2 = vdThreadFinishWrite(pDisk);
                AssertRC(rc2);
                fLockWrite = false;
            }

            /* Merge child state into parent. This means writing all blocks
             * which are allocated in the image up to the source image to the
             * destination image. */
            uint64_t uOffset = 0;
            uint64_t cbRemaining = cbSize;
            do
            {
                size_t cbThisRead = RT_MIN(VD_MERGE_BUFFER_SIZE, cbRemaining);
                rc = VERR_VD_BLOCK_FREE;

                /* Need to hold the write lock during a read-write operation. */
                rc2 = vdThreadStartWrite(pDisk);
                AssertRC(rc2);
                fLockWrite = true;

                /* Search for image with allocated block. Do not attempt to
                 * read more than the previous reads marked as valid. Otherwise
                 * this would return stale data when different block sizes are
                 * used for the images. */
                for (PVDIMAGE pCurrImage = pImageFrom;
                     pCurrImage != NULL && pCurrImage != pImageTo && rc == VERR_VD_BLOCK_FREE;
                     pCurrImage = pCurrImage->pPrev)
                {
                    rc = pCurrImage->Backend->pfnRead(pCurrImage->pBackendData,
                                                      uOffset, pvBuf,
                                                      cbThisRead, &cbThisRead);
                }

                if (rc != VERR_VD_BLOCK_FREE)
                {
                    if (RT_FAILURE(rc))
                        break;
                    rc = vdWriteHelper(pDisk, pImageTo, NULL, uOffset, pvBuf,
                                       cbThisRead, true /* fUpdateCache */);
                    if (RT_FAILURE(rc))
                        break;
                }
                else
                    rc = VINF_SUCCESS;

                rc2 = vdThreadFinishWrite(pDisk);
                AssertRC(rc2);
                fLockWrite = false;

                uOffset += cbThisRead;
                cbRemaining -= cbThisRead;

                if (pCbProgress && pCbProgress->pfnProgress)
                {
                    /** @todo r=klaus: this can update the progress to the same
                     * percentage over and over again if the image format makes
                     * relatively small increments. */
                    rc = pCbProgress->pfnProgress(pIfProgress->pvUser,
                                                  uOffset * 99 / cbSize);
                    if (RT_FAILURE(rc))
                        break;
                }
            } while (uOffset < cbSize);
        }

        /*
         * Leave in case of an error to avoid corrupted data in the image chain
         * (includes cancelling the operation by the user).
         */
        if (RT_FAILURE(rc))
            break;

        /* Need to hold the write lock while finishing the merge. */
        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        /* Update parent UUID so that image chain is consistent. */
        RTUUID Uuid;
        PVDIMAGE pImageChild = NULL;
        if (nImageFrom < nImageTo)
        {
            if (pImageFrom->pPrev)
            {
                rc = pImageFrom->pPrev->Backend->pfnGetUuid(pImageFrom->pPrev->pBackendData,
                                                            &Uuid);
                AssertRC(rc);
            }
            else
                RTUuidClear(&Uuid);
            rc = pImageTo->Backend->pfnSetParentUuid(pImageTo->pBackendData,
                                                     &Uuid);
            AssertRC(rc);
        }
        else
        {
            /* Update the parent uuid of the child of the last merged image. */
            if (pImageFrom->pNext)
            {
                rc = pImageTo->Backend->pfnGetUuid(pImageTo->pBackendData,
                                                   &Uuid);
                AssertRC(rc);

                rc = pImageFrom->Backend->pfnSetParentUuid(pImageFrom->pNext->pBackendData,
                                                           &Uuid);
                AssertRC(rc);

                pImageChild = pImageFrom->pNext;
            }
        }

        /* Delete the no longer needed images. */
        PVDIMAGE pImg = pImageFrom, pTmp;
        while (pImg != pImageTo)
        {
            if (nImageFrom < nImageTo)
                pTmp = pImg->pNext;
            else
                pTmp = pImg->pPrev;
            vdRemoveImageFromList(pDisk, pImg);
            pImg->Backend->pfnClose(pImg->pBackendData, true);
            RTMemFree(pImg->pszFilename);
            RTMemFree(pImg);
            pImg = pTmp;
        }

        /* Make sure destination image is back to read only if necessary. */
        if (pImageTo != pDisk->pLast)
        {
            uOpenFlags = pImageTo->Backend->pfnGetOpenFlags(pImageTo->pBackendData);
            uOpenFlags |= VD_OPEN_FLAGS_READONLY;
            rc = pImageTo->Backend->pfnSetOpenFlags(pImageTo->pBackendData,
                                                    uOpenFlags);
            if (RT_FAILURE(rc))
                break;
        }

        /*
         * Make sure the child is readonly
         * for the child -> parent merge direction
         * if neccessary.
        */
        if (   nImageFrom > nImageTo
            && pImageChild
            && pImageChild != pDisk->pLast)
        {
            uOpenFlags = pImageChild->Backend->pfnGetOpenFlags(pImageChild->pBackendData);
            uOpenFlags |= VD_OPEN_FLAGS_READONLY;
            rc = pImageChild->Backend->pfnSetOpenFlags(pImageChild->pBackendData,
                                                       uOpenFlags);
            if (RT_FAILURE(rc))
                break;
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    if (pvBuf)
        RTMemTmpFree(pvBuf);

    if (RT_SUCCESS(rc) && pCbProgress && pCbProgress->pfnProgress)
        pCbProgress->pfnProgress(pIfProgress->pvUser, 100);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Copies an image from one HDD container to another.
 * The copy is opened in the target HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination may be different from the source.
 * If both the source and destination reference the same HDD container,
 * then the image is moved (by copying/deleting or renaming) to the new location.
 * The source container is unchanged if the move operation fails, otherwise
 * the image at the new location is opened in the same way as the old one was.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDiskFrom       Pointer to source HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     New name of the image (may be NULL if pDiskFrom == pDiskTo).
 * @param   fMoveByRename   If true, attempt to perform a move by renaming (if successful the new size is ignored).
 * @param   cbSize          New image size (0 means leave unchanged).
 * @param   uImageFlags     Flags specifying special destination image features.
 * @param   pDstUuid        New UUID of the destination image. If NULL, a new UUID is created.
 *                          This parameter is used if and only if a true copy is created.
 *                          In all rename/move cases the UUIDs are copied over.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 * @param   pDstVDIfsImage  Pointer to the per-image VD interface list, for the
 *                          destination image.
 * @param   pDstVDIfsOperation Pointer to the per-image VD interface list,
 *                          for the destination image.
 */
VBOXDDU_DECL(int) VDCopy(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                         const char *pszBackend, const char *pszFilename,
                         bool fMoveByRename, uint64_t cbSize,
                         unsigned uImageFlags, PCRTUUID pDstUuid,
                         PVDINTERFACE pVDIfsOperation,
                         PVDINTERFACE pDstVDIfsImage,
                         PVDINTERFACE pDstVDIfsOperation)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockReadFrom = false, fLockWriteFrom = false, fLockWriteTo = false;
    void *pvBuf = NULL;
    PVDIMAGE pImageTo = NULL;

    LogFlowFunc(("pDiskFrom=%#p nImage=%u pDiskTo=%#p pszBackend=\"%s\" pszFilename=\"%s\" fMoveByRename=%d cbSize=%llu pVDIfsOperation=%#p pDstVDIfsImage=%#p pDstVDIfsOperation=%#p\n",
                 pDiskFrom, nImage, pDiskTo, pszBackend, pszFilename, fMoveByRename, cbSize, pVDIfsOperation, pDstVDIfsImage, pDstVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    PVDINTERFACE pDstIfProgress = VDInterfaceGet(pDstVDIfsOperation,
                                                 VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pDstCbProgress = NULL;
    if (pDstIfProgress)
        pDstCbProgress = VDGetInterfaceProgress(pDstIfProgress);

    do {
        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pDiskFrom), ("pDiskFrom=%#p\n", pDiskFrom),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDiskFrom->u32Signature == VBOXHDDDISK_SIGNATURE,
                  ("u32Signature=%08x\n", pDiskFrom->u32Signature));

        rc2 = vdThreadStartRead(pDiskFrom);
        AssertRC(rc2);
        fLockReadFrom = true;
        PVDIMAGE pImageFrom = vdGetImageByNumber(pDiskFrom, nImage);
        AssertPtrBreakStmt(pImageFrom, rc = VERR_VD_IMAGE_NOT_FOUND);
        AssertMsgBreakStmt(VALID_PTR(pDiskTo), ("pDiskTo=%#p\n", pDiskTo),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDiskTo->u32Signature == VBOXHDDDISK_SIGNATURE,
                  ("u32Signature=%08x\n", pDiskTo->u32Signature));

        /* Move the image. */
        if (pDiskFrom == pDiskTo)
        {
            /* Rename only works when backends are the same, are file based
             * and the rename method is implemented. */
            if (    fMoveByRename
                &&  !RTStrICmp(pszBackend, pImageFrom->Backend->pszBackendName)
                &&  pImageFrom->Backend->uBackendCaps & VD_CAP_FILE
                &&  pImageFrom->Backend->pfnRename)
            {
                rc2 = vdThreadFinishRead(pDiskFrom);
                AssertRC(rc2);
                fLockReadFrom = false;

                rc2 = vdThreadStartWrite(pDiskFrom);
                AssertRC(rc2);
                fLockWriteFrom = true;
                rc = pImageFrom->Backend->pfnRename(pImageFrom->pBackendData, pszFilename ? pszFilename : pImageFrom->pszFilename);
                break;
            }

            /** @todo Moving (including shrinking/growing) of the image is
             * requested, but the rename attempt failed or it wasn't possible.
             * Must now copy image to temp location. */
            AssertReleaseMsgFailed(("VDCopy: moving by copy/delete not implemented\n"));
        }

        /* pszFilename is allowed to be NULL, as this indicates copy to the existing image. */
        AssertMsgBreakStmt(pszFilename == NULL || (VALID_PTR(pszFilename) && *pszFilename),
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);

        uint64_t cbSizeFrom;
        cbSizeFrom = pImageFrom->Backend->pfnGetSize(pImageFrom->pBackendData);
        if (cbSizeFrom == 0)
        {
            rc = VERR_VD_VALUE_NOT_FOUND;
            break;
        }

        VDGEOMETRY PCHSGeometryFrom = {0, 0, 0};
        VDGEOMETRY LCHSGeometryFrom = {0, 0, 0};
        pImageFrom->Backend->pfnGetPCHSGeometry(pImageFrom->pBackendData, &PCHSGeometryFrom);
        pImageFrom->Backend->pfnGetLCHSGeometry(pImageFrom->pBackendData, &LCHSGeometryFrom);

        RTUUID ImageUuid, ImageModificationUuid;
        if (pDiskFrom != pDiskTo)
        {
            if (pDstUuid)
                ImageUuid = *pDstUuid;
            else
                RTUuidCreate(&ImageUuid);
        }
        else
        {
            rc = pImageFrom->Backend->pfnGetUuid(pImageFrom->pBackendData, &ImageUuid);
            if (RT_FAILURE(rc))
                RTUuidCreate(&ImageUuid);
        }
        rc = pImageFrom->Backend->pfnGetModificationUuid(pImageFrom->pBackendData, &ImageModificationUuid);
        if (RT_FAILURE(rc))
            RTUuidClear(&ImageModificationUuid);

        char szComment[1024];
        rc = pImageFrom->Backend->pfnGetComment(pImageFrom->pBackendData, szComment, sizeof(szComment));
        if (RT_FAILURE(rc))
            szComment[0] = '\0';
        else
            szComment[sizeof(szComment) - 1] = '\0';

        unsigned uOpenFlagsFrom;
        uOpenFlagsFrom = pImageFrom->Backend->pfnGetOpenFlags(pImageFrom->pBackendData);

        rc2 = vdThreadFinishRead(pDiskFrom);
        AssertRC(rc2);
        fLockReadFrom = false;

        rc2 = vdThreadStartRead(pDiskTo);
        AssertRC(rc2);
        unsigned cImagesTo = pDiskTo->cImages;
        rc2 = vdThreadFinishRead(pDiskTo);
        AssertRC(rc2);

        if (pszFilename)
        {
            if (cbSize == 0)
                cbSize = cbSizeFrom;

            /* Create destination image with the properties of source image. */
            /** @todo replace the VDCreateDiff/VDCreateBase calls by direct
             * calls to the backend. Unifies the code and reduces the API
             * dependencies. Would also make the synchronization explicit. */
            if (cImagesTo > 0)
            {
                rc = VDCreateDiff(pDiskTo, pszBackend, pszFilename,
                                  uImageFlags, szComment, &ImageUuid,
                                  NULL /* pParentUuid */,
                                  uOpenFlagsFrom & ~VD_OPEN_FLAGS_READONLY,
                                  NULL, NULL);

                rc2 = vdThreadStartWrite(pDiskTo);
                AssertRC(rc2);
                fLockWriteTo = true;
            } else {
                /** @todo hack to force creation of a fixed image for
                 * the RAW backend, which can't handle anything else. */
                if (!RTStrICmp(pszBackend, "RAW"))
                    uImageFlags |= VD_IMAGE_FLAGS_FIXED;

                /* Fix broken PCHS geometry. Can happen for two reasons: either
                 * the backend mixes up PCHS and LCHS, or the application used
                 * to create the source image has put garbage in it. */
                /** @todo double-check if the VHD backend correctly handles
                 * PCHS and LCHS geometry. also reconsider our current paranoia
                 * level when it comes to geometry settings here and in the
                 * backends. */
                if (PCHSGeometryFrom.cHeads > 16 || PCHSGeometryFrom.cSectors > 63)
                {
                    Assert(RT_MIN(cbSize / 512 / 16 / 63, 16383) - (uint32_t)RT_MIN(cbSize / 512 / 16 / 63, 16383));
                    PCHSGeometryFrom.cCylinders = (uint32_t)RT_MIN(cbSize / 512 / 16 / 63, 16383);
                    PCHSGeometryFrom.cHeads = 16;
                    PCHSGeometryFrom.cSectors = 63;
                }

                rc = VDCreateBase(pDiskTo, pszBackend, pszFilename, cbSize,
                                  uImageFlags, szComment,
                                  &PCHSGeometryFrom, &LCHSGeometryFrom,
                                  NULL, uOpenFlagsFrom & ~VD_OPEN_FLAGS_READONLY, NULL, NULL);

                rc2 = vdThreadStartWrite(pDiskTo);
                AssertRC(rc2);
                fLockWriteTo = true;

                if (RT_SUCCESS(rc) && !RTUuidIsNull(&ImageUuid))
                     pDiskTo->pLast->Backend->pfnSetUuid(pDiskTo->pLast->pBackendData, &ImageUuid);
            }
            if (RT_FAILURE(rc))
                break;

            pImageTo = pDiskTo->pLast;
            AssertPtrBreakStmt(pImageTo, rc = VERR_VD_IMAGE_NOT_FOUND);

            cbSize = RT_MIN(cbSize, cbSizeFrom);
        }
        else
        {
            pImageTo = pDiskTo->pLast;
            AssertPtrBreakStmt(pImageTo, rc = VERR_VD_IMAGE_NOT_FOUND);

            uint64_t cbSizeTo;
            cbSizeTo = pImageTo->Backend->pfnGetSize(pImageTo->pBackendData);
            if (cbSizeTo == 0)
            {
                rc = VERR_VD_VALUE_NOT_FOUND;
                break;
            }

            if (cbSize == 0)
                cbSize = RT_MIN(cbSizeFrom, cbSizeTo);
        }

        rc2 = vdThreadFinishWrite(pDiskTo);
        AssertRC(rc2);
        fLockWriteTo = false;

        /* Allocate tmp buffer. */
        pvBuf = RTMemTmpAlloc(VD_MERGE_BUFFER_SIZE);
        if (!pvBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        /* Whether we can take the optimized copy path (false) or not.
         * Don't optimize if the image existed or if it is a child image. */
        bool fRegularRead = (pszFilename == NULL) || (cImagesTo > 0);

        /* Copy the data. */
        uint64_t uOffset = 0;
        uint64_t cbRemaining = cbSize;

        do
        {
            size_t cbThisRead = RT_MIN(VD_MERGE_BUFFER_SIZE, cbRemaining);

            /* Note that we don't attempt to synchronize cross-disk accesses.
             * It wouldn't be very difficult to do, just the lock order would
             * need to be defined somehow to prevent deadlocks. Postpone such
             * magic as there is no use case for this. */

            rc2 = vdThreadStartRead(pDiskFrom);
            AssertRC(rc2);
            fLockReadFrom = true;

            /*
             * Updating the cache doesn't make any sense
             * as we are looping once through the image.
             */
            rc = vdReadHelper(pDiskFrom, pImageFrom, NULL, uOffset, pvBuf,
                              cbThisRead, fRegularRead,
                              false /* fUpdateCache */);
            if (RT_FAILURE(rc) && rc != VERR_VD_BLOCK_FREE)
                break;

            rc2 = vdThreadFinishRead(pDiskFrom);
            AssertRC(rc2);
            fLockReadFrom = false;

            if (rc != VERR_VD_BLOCK_FREE)
            {
                rc2 = vdThreadStartWrite(pDiskTo);
                AssertRC(rc2);
                fLockWriteTo = true;

                rc = vdWriteHelper(pDiskTo, pImageTo, NULL, uOffset, pvBuf,
                                   cbThisRead, false /* fUpdateCache */);
                if (RT_FAILURE(rc))
                    break;

                rc2 = vdThreadFinishWrite(pDiskTo);
                AssertRC(rc2);
                fLockWriteTo = false;
            }
            else /* Don't propagate the error to the outside */
                rc = VINF_SUCCESS;

            uOffset += cbThisRead;
            cbRemaining -= cbThisRead;

            if (pCbProgress && pCbProgress->pfnProgress)
            {
                /** @todo r=klaus: this can update the progress to the same
                 * percentage over and over again if the image format makes
                 * relatively small increments. */
                rc = pCbProgress->pfnProgress(pIfProgress->pvUser,
                                              uOffset * 99 / cbSize);
                if (RT_FAILURE(rc))
                    break;
            }
            if (pDstCbProgress && pDstCbProgress->pfnProgress)
            {
                /** @todo r=klaus: this can update the progress to the same
                 * percentage over and over again if the image format makes
                 * relatively small increments. */
                rc = pDstCbProgress->pfnProgress(pDstIfProgress->pvUser,
                                                 uOffset * 99 / cbSize);
                if (RT_FAILURE(rc))
                    break;
            }
        } while (uOffset < cbSize);

        if (RT_SUCCESS(rc))
        {
            rc2 = vdThreadStartWrite(pDiskTo);
            AssertRC(rc2);
            fLockWriteTo = true;

            /* Only set modification UUID if it is non-null, since the source
             * backend might not provide a valid modification UUID. */
            if (!RTUuidIsNull(&ImageModificationUuid))
                pImageTo->Backend->pfnSetModificationUuid(pImageTo->pBackendData, &ImageModificationUuid);
        }
    } while (0);

    if (RT_FAILURE(rc) && pImageTo && pszFilename)
    {
        /* Take the write lock only if it is not taken. Not worth making the
         * above code even more complicated. */
        if (RT_UNLIKELY(!fLockWriteTo))
        {
            rc2 = vdThreadStartWrite(pDiskTo);
            AssertRC(rc2);
            fLockWriteTo = true;
        }
        /* Error detected, but new image created. Remove image from list. */
        vdRemoveImageFromList(pDiskTo, pImageTo);

        /* Close and delete image. */
        rc2 = pImageTo->Backend->pfnClose(pImageTo->pBackendData, true);
        AssertRC(rc2);
        pImageTo->pBackendData = NULL;

        /* Free remaining resources. */
        if (pImageTo->pszFilename)
            RTStrFree(pImageTo->pszFilename);

        RTMemFree(pImageTo);
    }

    if (RT_UNLIKELY(fLockWriteTo))
    {
        rc2 = vdThreadFinishWrite(pDiskTo);
        AssertRC(rc2);
    }
    if (RT_UNLIKELY(fLockWriteFrom))
    {
        rc2 = vdThreadFinishWrite(pDiskFrom);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockReadFrom))
    {
        rc2 = vdThreadFinishRead(pDiskFrom);
        AssertRC(rc2);
    }

    if (pvBuf)
        RTMemTmpFree(pvBuf);

    if (RT_SUCCESS(rc))
    {
        if (pCbProgress && pCbProgress->pfnProgress)
            pCbProgress->pfnProgress(pIfProgress->pvUser, 100);
        if (pDstCbProgress && pDstCbProgress->pfnProgress)
            pDstCbProgress->pfnProgress(pDstIfProgress->pvUser, 100);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Optimizes the storage consumption of an image. Typically the unused blocks
 * have to be wiped with zeroes to achieve a substantial reduced storage use.
 * Another optimization done is reordering the image blocks, which can provide
 * a significant performance boost, as reads and writes tend to use less random
 * file offsets.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @return  VERR_VD_IMAGE_READ_ONLY if image is not writable.
 * @return  VERR_NOT_SUPPORTED if this kind of image can be compacted, but
 *                             the code for this isn't implemented yet.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCompact(PVBOXHDD pDisk, unsigned nImage,
                            PVDINTERFACE pVDIfsOperation)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false, fLockWrite = false;
    void *pvBuf = NULL;
    void *pvTmp = NULL;

    LogFlowFunc(("pDisk=%#p nImage=%u pVDIfsOperation=%#p\n",
                 pDisk, nImage, pVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    do {
        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pDisk), ("pDisk=%#p\n", pDisk),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE,
                  ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        /* If there is no compact callback for not file based backends then
         * the backend doesn't need compaction. No need to make much fuss about
         * this. For file based ones signal this as not yet supported. */
        if (!pImage->Backend->pfnCompact)
        {
            if (pImage->Backend->uBackendCaps & VD_CAP_FILE)
                rc = VERR_NOT_SUPPORTED;
            else
                rc = VINF_SUCCESS;
            break;
        }

        /* Insert interface for reading parent state into per-operation list,
         * if there is a parent image. */
        VDINTERFACE IfOpParent;
        VDINTERFACEPARENTSTATE ParentCb;
        VDPARENTSTATEDESC ParentUser;
        if (pImage->pPrev)
        {
            ParentCb.cbSize = sizeof(ParentCb);
            ParentCb.enmInterface = VDINTERFACETYPE_PARENTSTATE;
            ParentCb.pfnParentRead = vdParentRead;
            ParentUser.pDisk = pDisk;
            ParentUser.pImage = pImage->pPrev;
            rc = VDInterfaceAdd(&IfOpParent, "VDCompact_ParentState", VDINTERFACETYPE_PARENTSTATE,
                                &ParentCb, &ParentUser, &pVDIfsOperation);
            AssertRC(rc);
        }

        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
        fLockRead = false;

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        rc = pImage->Backend->pfnCompact(pImage->pBackendData,
                                         0, 99,
                                         pDisk->pVDIfsDisk,
                                         pImage->pVDIfsImage,
                                         pVDIfsOperation);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    if (pvBuf)
        RTMemTmpFree(pvBuf);
    if (pvTmp)
        RTMemTmpFree(pvTmp);

    if (RT_SUCCESS(rc))
    {
        if (pCbProgress && pCbProgress->pfnProgress)
            pCbProgress->pfnProgress(pIfProgress->pvUser, 100);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Resizes the the given disk image to the given size.
 *
 * @return  VBox status
 * @return  VERR_VD_IMAGE_READ_ONLY if image is not writable.
 * @return  VERR_NOT_SUPPORTED if this kind of image can be compacted, but
 *
 * @param   pDisk           Pointer to the HDD container.
 * @param   cbSize          New size of the image.
 * @param   pPCHSGeometry   Pointer to the new physical disk geometry <= (16383,16,63). Not NULL.
 * @param   pLCHSGeometry   Pointer to the new logical disk geometry <= (x,255,63). Not NULL.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDResize(PVBOXHDD pDisk, uint64_t cbSize,
                           PCVDGEOMETRY pPCHSGeometry,
                           PCVDGEOMETRY pLCHSGeometry,
                           PVDINTERFACE pVDIfsOperation)
{
    /** @todo r=klaus resizing was designed to be part of VDCopy, so having a separate function is not desirable. */
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false, fLockWrite = false;

    LogFlowFunc(("pDisk=%#p cbSize=%llu pVDIfsOperation=%#p\n",
                 pDisk, cbSize, pVDIfsOperation));

    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
        pCbProgress = VDGetInterfaceProgress(pIfProgress);

    do {
        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pDisk), ("pDisk=%#p\n", pDisk),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE,
                  ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        /* Not supported if the disk has child images attached. */
        AssertMsgBreakStmt(pDisk->cImages == 1, ("cImages=%u\n", pDisk->cImages),
                           rc = VERR_NOT_SUPPORTED);

        PVDIMAGE pImage = pDisk->pBase;

        /* If there is no compact callback for not file based backends then
         * the backend doesn't need compaction. No need to make much fuss about
         * this. For file based ones signal this as not yet supported. */
        if (!pImage->Backend->pfnResize)
        {
            if (pImage->Backend->uBackendCaps & VD_CAP_FILE)
                rc = VERR_NOT_SUPPORTED;
            else
                rc = VINF_SUCCESS;
            break;
        }

        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
        fLockRead = false;

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        VDGEOMETRY PCHSGeometryOld;
        VDGEOMETRY LCHSGeometryOld;
        PCVDGEOMETRY pPCHSGeometryNew;
        PCVDGEOMETRY pLCHSGeometryNew;

        if (pPCHSGeometry->cCylinders == 0)
        {
            /* Auto-detect marker, calculate new value ourself. */
            rc = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData, &PCHSGeometryOld);
            if (RT_SUCCESS(rc) && (PCHSGeometryOld.cCylinders != 0))
                PCHSGeometryOld.cCylinders = RT_MIN(cbSize / 512 / PCHSGeometryOld.cHeads / PCHSGeometryOld.cSectors, 16383);
            else if (rc == VERR_VD_GEOMETRY_NOT_SET)
                rc = VINF_SUCCESS;

            pPCHSGeometryNew = &PCHSGeometryOld;
        }
        else
            pPCHSGeometryNew = pPCHSGeometry;

        if (pLCHSGeometry->cCylinders == 0)
        {
            /* Auto-detect marker, calculate new value ourself. */
            rc = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData, &LCHSGeometryOld);
            if (RT_SUCCESS(rc) && (LCHSGeometryOld.cCylinders != 0))
                LCHSGeometryOld.cCylinders = cbSize / 512 / LCHSGeometryOld.cHeads / LCHSGeometryOld.cSectors;
            else if (rc == VERR_VD_GEOMETRY_NOT_SET)
                rc = VINF_SUCCESS;

            pLCHSGeometryNew = &LCHSGeometryOld;
        }
        else
            pLCHSGeometryNew = pLCHSGeometry;

        if (RT_SUCCESS(rc))
            rc = pImage->Backend->pfnResize(pImage->pBackendData,
                                            cbSize,
                                            pPCHSGeometryNew,
                                            pLCHSGeometryNew,
                                            0, 99,
                                            pDisk->pVDIfsDisk,
                                            pImage->pVDIfsImage,
                                            pVDIfsOperation);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }
    else if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    if (RT_SUCCESS(rc))
    {
        if (pCbProgress && pCbProgress->pfnProgress)
            pCbProgress->pfnProgress(pIfProgress->pvUser, 100);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Closes the last opened image file in HDD container.
 * If previous image file was opened in read-only mode (the normal case) and
 * the last opened image is in read-write mode then the previous image will be
 * reopened in read/write mode.
 *
 * @returns VBox status code.
 * @returns VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDClose(PVBOXHDD pDisk, bool fDelete)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p fDelete=%d\n", pDisk, fDelete));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Not worth splitting this up into a read lock phase and write
         * lock phase, as closing an image is a relatively fast operation
         * dominated by the part which needs the write lock. */
        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = pDisk->pLast;
        if (!pImage)
        {
            rc = VERR_VD_NOT_OPENED;
            break;
        }
        unsigned uOpenFlags = pImage->Backend->pfnGetOpenFlags(pImage->pBackendData);
        /* Remove image from list of opened images. */
        vdRemoveImageFromList(pDisk, pImage);
        /* Close (and optionally delete) image. */
        rc = pImage->Backend->pfnClose(pImage->pBackendData, fDelete);
        /* Free remaining resources related to the image. */
        RTStrFree(pImage->pszFilename);
        RTMemFree(pImage);

        pImage = pDisk->pLast;
        if (!pImage)
            break;

        /* If disk was previously in read/write mode, make sure it will stay
         * like this (if possible) after closing this image. Set the open flags
         * accordingly. */
        if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            uOpenFlags = pImage->Backend->pfnGetOpenFlags(pImage->pBackendData);
            uOpenFlags &= ~ VD_OPEN_FLAGS_READONLY;
            rc = pImage->Backend->pfnSetOpenFlags(pImage->pBackendData, uOpenFlags);
        }

        /* Cache disk information. */
        pDisk->cbSize = pImage->Backend->pfnGetSize(pImage->pBackendData);

        /* Cache PCHS geometry. */
        rc2 = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData,
                                                 &pDisk->PCHSGeometry);
        if (RT_FAILURE(rc2))
        {
            pDisk->PCHSGeometry.cCylinders = 0;
            pDisk->PCHSGeometry.cHeads = 0;
            pDisk->PCHSGeometry.cSectors = 0;
        }
        else
        {
            /* Make sure the PCHS geometry is properly clipped. */
            pDisk->PCHSGeometry.cCylinders = RT_MIN(pDisk->PCHSGeometry.cCylinders, 16383);
            pDisk->PCHSGeometry.cHeads = RT_MIN(pDisk->PCHSGeometry.cHeads, 16);
            pDisk->PCHSGeometry.cSectors = RT_MIN(pDisk->PCHSGeometry.cSectors, 63);
        }

        /* Cache LCHS geometry. */
        rc2 = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData,
                                                  &pDisk->LCHSGeometry);
        if (RT_FAILURE(rc2))
        {
            pDisk->LCHSGeometry.cCylinders = 0;
            pDisk->LCHSGeometry.cHeads = 0;
            pDisk->LCHSGeometry.cSectors = 0;
        }
        else
        {
            /* Make sure the LCHS geometry is properly clipped. */
            pDisk->LCHSGeometry.cHeads = RT_MIN(pDisk->LCHSGeometry.cHeads, 255);
            pDisk->LCHSGeometry.cSectors = RT_MIN(pDisk->LCHSGeometry.cSectors, 63);
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Closes the currently opened cache image file in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no cache is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDCacheClose(PVBOXHDD pDisk, bool fDelete)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;
    PVDCACHE pCache = NULL;

    LogFlowFunc(("pDisk=%#p fDelete=%d\n", pDisk, fDelete));

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        AssertPtrBreakStmt(pDisk->pCache, rc = VERR_VD_CACHE_NOT_FOUND);

        pCache = pDisk->pCache;
        pDisk->pCache = NULL;

        pCache->Backend->pfnClose(pCache->pBackendData, fDelete);
        if (pCache->pszFilename)
            RTStrFree(pCache->pszFilename);
        RTMemFree(pCache);
    } while (0);

    if (RT_LIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Closes all opened image files in HDD container.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDCloseAll(PVBOXHDD pDisk)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p\n", pDisk));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Lock the entire operation. */
        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDCACHE pCache = pDisk->pCache;
        if (pCache)
        {
            rc2 = pCache->Backend->pfnClose(pCache->pBackendData, false);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;

            if (pCache->pszFilename)
                RTStrFree(pCache->pszFilename);
            RTMemFree(pCache);
        }

        PVDIMAGE pImage = pDisk->pLast;
        while (VALID_PTR(pImage))
        {
            PVDIMAGE pPrev = pImage->pPrev;
            /* Remove image from list of opened images. */
            vdRemoveImageFromList(pDisk, pImage);
            /* Close image. */
            rc2 = pImage->Backend->pfnClose(pImage->pBackendData, false);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
            /* Free remaining resources related to the image. */
            RTStrFree(pImage->pszFilename);
            RTMemFree(pImage);
            pImage = pPrev;
        }
        Assert(!VALID_PTR(pDisk->pLast));
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @returns VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbRead          Number of bytes to read.
 */
VBOXDDU_DECL(int) VDRead(PVBOXHDD pDisk, uint64_t uOffset, void *pvBuf,
                         size_t cbRead)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p uOffset=%llu pvBuf=%p cbRead=%zu\n",
                 pDisk, uOffset, pvBuf, cbRead));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pvBuf),
                           ("pvBuf=%#p\n", pvBuf),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cbRead,
                           ("cbRead=%zu\n", cbRead),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        AssertMsgBreakStmt(uOffset + cbRead <= pDisk->cbSize,
                           ("uOffset=%llu cbRead=%zu pDisk->cbSize=%llu\n",
                            uOffset, cbRead, pDisk->cbSize),
                           rc = VERR_INVALID_PARAMETER);

        PVDIMAGE pImage = pDisk->pLast;
        AssertPtrBreakStmt(pImage, rc = VERR_VD_NOT_OPENED);

        rc = vdReadHelper(pDisk, pImage, NULL, uOffset, pvBuf, cbRead,
                          true /* fZeroFreeBlocks */,
                          true /* fUpdateCache */);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Write data to virtual HDD.
 *
 * @returns VBox status code.
 * @returns VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of the first byte being
 *                          written from start of disk.
 * @param   pvBuf           Pointer to buffer for writing data.
 * @param   cbWrite         Number of bytes to write.
 */
VBOXDDU_DECL(int) VDWrite(PVBOXHDD pDisk, uint64_t uOffset, const void *pvBuf,
                          size_t cbWrite)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p uOffset=%llu pvBuf=%p cbWrite=%zu\n",
                 pDisk, uOffset, pvBuf, cbWrite));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pvBuf),
                           ("pvBuf=%#p\n", pvBuf),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cbWrite,
                           ("cbWrite=%zu\n", cbWrite),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        AssertMsgBreakStmt(uOffset + cbWrite <= pDisk->cbSize,
                           ("uOffset=%llu cbWrite=%zu pDisk->cbSize=%llu\n",
                            uOffset, cbWrite, pDisk->cbSize),
                           rc = VERR_INVALID_PARAMETER);

        PVDIMAGE pImage = pDisk->pLast;
        AssertPtrBreakStmt(pImage, rc = VERR_VD_NOT_OPENED);

        vdSetModifiedFlag(pDisk);
        rc = vdWriteHelper(pDisk, pImage, NULL, uOffset, pvBuf, cbWrite,
                           true /* fUpdateCache */);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Make sure the on disk representation of a virtual HDD is up to date.
 *
 * @returns VBox status code.
 * @returns VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDFlush(PVBOXHDD pDisk)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p\n", pDisk));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = pDisk->pLast;
        AssertPtrBreakStmt(pImage, rc = VERR_VD_NOT_OPENED);

        vdResetModifiedFlag(pDisk);
        rc = pImage->Backend->pfnFlush(pImage->pBackendData);

        if (   RT_SUCCESS(rc)
            && pDisk->pCache)
            rc = pDisk->pCache->Backend->pfnFlush(pDisk->pCache->pBackendData);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images have been opened.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(unsigned) VDGetCount(PVBOXHDD pDisk)
{
    unsigned cImages;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p\n", pDisk));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, cImages = 0);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        cImages = pDisk->cImages;
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %u\n", cImages));
    return cImages;
}

/**
 * Get read/write mode of HDD container.
 *
 * @returns Virtual disk ReadOnly status.
 * @returns true if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(bool) VDIsReadOnly(PVBOXHDD pDisk)
{
    bool fReadOnly;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p\n", pDisk));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, fReadOnly = false);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = pDisk->pLast;
        AssertPtrBreakStmt(pImage, fReadOnly = true);

        unsigned uOpenFlags;
        uOpenFlags = pDisk->pLast->Backend->pfnGetOpenFlags(pDisk->pLast->pBackendData);
        fReadOnly = !!(uOpenFlags & VD_OPEN_FLAGS_READONLY);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %d\n", fReadOnly));
    return fReadOnly;
}

/**
 * Get total capacity of an image in HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counds from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint64_t) VDGetSize(PVBOXHDD pDisk, unsigned nImage)
{
    uint64_t cbSize;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u\n", pDisk, nImage));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, cbSize = 0);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, cbSize = 0);
        cbSize = pImage->Backend->pfnGetSize(pImage->pBackendData);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %llu\n", cbSize));
    return cbSize;
}

/**
 * Get total file size of an image in HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint64_t) VDGetFileSize(PVBOXHDD pDisk, unsigned nImage)
{
    uint64_t cbSize;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u\n", pDisk, nImage));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, cbSize = 0);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, cbSize = 0);
        cbSize = pImage->Backend->pfnGetFileSize(pImage->pBackendData);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %llu\n", cbSize));
    return cbSize;
}

/**
 * Get virtual disk PCHS geometry stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_VD_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pPCHSGeometry   Where to store PCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDGetPCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PVDGEOMETRY pPCHSGeometry)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pPCHSGeometry=%#p\n",
                 pDisk, nImage, pPCHSGeometry));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pPCHSGeometry),
                           ("pPCHSGeometry=%#p\n", pPCHSGeometry),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        if (pImage == pDisk->pLast)
        {
            /* Use cached information if possible. */
            if (pDisk->PCHSGeometry.cCylinders != 0)
                *pPCHSGeometry = pDisk->PCHSGeometry;
            else
                rc = VERR_VD_GEOMETRY_NOT_SET;
        }
        else
            rc = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData,
                                                     pPCHSGeometry);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("%Rrc (PCHS=%u/%u/%u)\n", rc,
                 pDisk->PCHSGeometry.cCylinders, pDisk->PCHSGeometry.cHeads,
                 pDisk->PCHSGeometry.cSectors));
    return rc;
}

/**
 * Store virtual disk PCHS geometry in HDD container.
 *
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_VD_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pPCHSGeometry   Where to load PCHS geometry from. Not NULL.
 */
VBOXDDU_DECL(int) VDSetPCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PCVDGEOMETRY pPCHSGeometry)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pPCHSGeometry=%#p PCHS=%u/%u/%u\n",
                 pDisk, nImage, pPCHSGeometry, pPCHSGeometry->cCylinders,
                 pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(   VALID_PTR(pPCHSGeometry)
                           && pPCHSGeometry->cHeads <= 16
                           && pPCHSGeometry->cSectors <= 63,
                           ("pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pPCHSGeometry,
                            pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads,
                            pPCHSGeometry->cSectors),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        if (pImage == pDisk->pLast)
        {
            if (    pPCHSGeometry->cCylinders != pDisk->PCHSGeometry.cCylinders
                ||  pPCHSGeometry->cHeads != pDisk->PCHSGeometry.cHeads
                ||  pPCHSGeometry->cSectors != pDisk->PCHSGeometry.cSectors)
            {
                /* Only update geometry if it is changed. Avoids similar checks
                 * in every backend. Most of the time the new geometry is set
                 * to the previous values, so no need to go through the hassle
                 * of updating an image which could be opened in read-only mode
                 * right now. */
                rc = pImage->Backend->pfnSetPCHSGeometry(pImage->pBackendData,
                                                         pPCHSGeometry);

                /* Cache new geometry values in any case. */
                rc2 = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData,
                                                          &pDisk->PCHSGeometry);
                if (RT_FAILURE(rc2))
                {
                    pDisk->PCHSGeometry.cCylinders = 0;
                    pDisk->PCHSGeometry.cHeads = 0;
                    pDisk->PCHSGeometry.cSectors = 0;
                }
                else
                {
                    /* Make sure the CHS geometry is properly clipped. */
                    pDisk->PCHSGeometry.cHeads = RT_MIN(pDisk->PCHSGeometry.cHeads, 255);
                    pDisk->PCHSGeometry.cSectors = RT_MIN(pDisk->PCHSGeometry.cSectors, 63);
                }
            }
        }
        else
        {
            VDGEOMETRY PCHS;
            rc = pImage->Backend->pfnGetPCHSGeometry(pImage->pBackendData,
                                                     &PCHS);
            if (    RT_FAILURE(rc)
                ||  pPCHSGeometry->cCylinders != PCHS.cCylinders
                ||  pPCHSGeometry->cHeads != PCHS.cHeads
                ||  pPCHSGeometry->cSectors != PCHS.cSectors)
            {
                /* Only update geometry if it is changed. Avoids similar checks
                 * in every backend. Most of the time the new geometry is set
                 * to the previous values, so no need to go through the hassle
                 * of updating an image which could be opened in read-only mode
                 * right now. */
                rc = pImage->Backend->pfnSetPCHSGeometry(pImage->pBackendData,
                                                         pPCHSGeometry);
            }
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get virtual disk LCHS geometry stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_VD_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pLCHSGeometry   Where to store LCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDGetLCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PVDGEOMETRY pLCHSGeometry)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pLCHSGeometry=%#p\n",
                 pDisk, nImage, pLCHSGeometry));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pLCHSGeometry),
                           ("pLCHSGeometry=%#p\n", pLCHSGeometry),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        if (pImage == pDisk->pLast)
        {
            /* Use cached information if possible. */
            if (pDisk->LCHSGeometry.cCylinders != 0)
                *pLCHSGeometry = pDisk->LCHSGeometry;
            else
                rc = VERR_VD_GEOMETRY_NOT_SET;
        }
        else
            rc = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData,
                                                     pLCHSGeometry);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc((": %Rrc (LCHS=%u/%u/%u)\n", rc,
                 pDisk->LCHSGeometry.cCylinders, pDisk->LCHSGeometry.cHeads,
                 pDisk->LCHSGeometry.cSectors));
    return rc;
}

/**
 * Store virtual disk LCHS geometry in HDD container.
 *
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_VD_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pLCHSGeometry   Where to load LCHS geometry from. Not NULL.
 */
VBOXDDU_DECL(int) VDSetLCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PCVDGEOMETRY pLCHSGeometry)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pLCHSGeometry=%#p LCHS=%u/%u/%u\n",
                 pDisk, nImage, pLCHSGeometry, pLCHSGeometry->cCylinders,
                 pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(   VALID_PTR(pLCHSGeometry)
                           && pLCHSGeometry->cHeads <= 255
                           && pLCHSGeometry->cSectors <= 63,
                           ("pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pLCHSGeometry,
                            pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads,
                            pLCHSGeometry->cSectors),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        if (pImage == pDisk->pLast)
        {
            if (    pLCHSGeometry->cCylinders != pDisk->LCHSGeometry.cCylinders
                ||  pLCHSGeometry->cHeads != pDisk->LCHSGeometry.cHeads
                ||  pLCHSGeometry->cSectors != pDisk->LCHSGeometry.cSectors)
            {
                /* Only update geometry if it is changed. Avoids similar checks
                 * in every backend. Most of the time the new geometry is set
                 * to the previous values, so no need to go through the hassle
                 * of updating an image which could be opened in read-only mode
                 * right now. */
                rc = pImage->Backend->pfnSetLCHSGeometry(pImage->pBackendData,
                                                         pLCHSGeometry);

                /* Cache new geometry values in any case. */
                rc2 = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData,
                                                          &pDisk->LCHSGeometry);
                if (RT_FAILURE(rc2))
                {
                    pDisk->LCHSGeometry.cCylinders = 0;
                    pDisk->LCHSGeometry.cHeads = 0;
                    pDisk->LCHSGeometry.cSectors = 0;
                }
                else
                {
                    /* Make sure the CHS geometry is properly clipped. */
                    pDisk->LCHSGeometry.cHeads = RT_MIN(pDisk->LCHSGeometry.cHeads, 255);
                    pDisk->LCHSGeometry.cSectors = RT_MIN(pDisk->LCHSGeometry.cSectors, 63);
                }
            }
        }
        else
        {
            VDGEOMETRY LCHS;
            rc = pImage->Backend->pfnGetLCHSGeometry(pImage->pBackendData,
                                                     &LCHS);
            if (    RT_FAILURE(rc)
                ||  pLCHSGeometry->cCylinders != LCHS.cCylinders
                ||  pLCHSGeometry->cHeads != LCHS.cHeads
                ||  pLCHSGeometry->cSectors != LCHS.cSectors)
            {
                /* Only update geometry if it is changed. Avoids similar checks
                 * in every backend. Most of the time the new geometry is set
                 * to the previous values, so no need to go through the hassle
                 * of updating an image which could be opened in read-only mode
                 * right now. */
                rc = pImage->Backend->pfnSetLCHSGeometry(pImage->pBackendData,
                                                         pLCHSGeometry);
            }
        }
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get version of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VDGetVersion(PVBOXHDD pDisk, unsigned nImage,
                               unsigned *puVersion)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u puVersion=%#p\n",
                 pDisk, nImage, puVersion));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(puVersion),
                           ("puVersion=%#p\n", puVersion),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        *puVersion = pImage->Backend->pfnGetVersion(pImage->pBackendData);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc uVersion=%#x\n", rc, *puVersion));
    return rc;
}

/**
 * List the capabilities of image backend in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to the HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pbackendInfo    Where to store the backend information.
 */
VBOXDDU_DECL(int) VDBackendInfoSingle(PVBOXHDD pDisk, unsigned nImage,
                                      PVDBACKENDINFO pBackendInfo)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pBackendInfo=%#p\n",
                 pDisk, nImage, pBackendInfo));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pBackendInfo),
                           ("pBackendInfo=%#p\n", pBackendInfo),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        pBackendInfo->pszBackend = pImage->Backend->pszBackendName;
        pBackendInfo->uBackendCaps = pImage->Backend->uBackendCaps;
        pBackendInfo->papszFileExtensions = pImage->Backend->papszFileExtensions;
        pBackendInfo->paConfigInfo = pImage->Backend->paConfigInfo;
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get flags of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puImageFlags    Where to store the image flags.
 */
VBOXDDU_DECL(int) VDGetImageFlags(PVBOXHDD pDisk, unsigned nImage,
                                  unsigned *puImageFlags)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u puImageFlags=%#p\n",
                 pDisk, nImage, puImageFlags));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(puImageFlags),
                           ("puImageFlags=%#p\n", puImageFlags),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        *puImageFlags = pImage->uImageFlags;
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc uImageFlags=%#x\n", rc, *puImageFlags));
    return rc;
}

/**
 * Get open flags of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puOpenFlags     Where to store the image open flags.
 */
VBOXDDU_DECL(int) VDGetOpenFlags(PVBOXHDD pDisk, unsigned nImage,
                                 unsigned *puOpenFlags)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u puOpenFlags=%#p\n",
                 pDisk, nImage, puOpenFlags));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(puOpenFlags),
                           ("puOpenFlags=%#p\n", puOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        *puOpenFlags = pImage->Backend->pfnGetOpenFlags(pImage->pBackendData);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc uOpenFlags=%#x\n", rc, *puOpenFlags));
    return rc;
}

/**
 * Set open flags of image in HDD container.
 * This operation may cause file locking changes and/or files being reopened.
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDSetOpenFlags(PVBOXHDD pDisk, unsigned nImage,
                                 unsigned uOpenFlags)
{
    int rc;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p uOpenFlags=%#u\n", pDisk, uOpenFlags));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt((uOpenFlags & ~VD_OPEN_FLAGS_MASK) == 0,
                           ("uOpenFlags=%#x\n", uOpenFlags),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        rc = pImage->Backend->pfnSetOpenFlags(pImage->pBackendData,
                                              uOpenFlags);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get base filename of image in HDD container. Some image formats use
 * other filenames as well, so don't use this for anything but informational
 * purposes.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszFilename     Where to store the image file name.
 * @param   cbFilename      Size of buffer pszFilename points to.
 */
VBOXDDU_DECL(int) VDGetFilename(PVBOXHDD pDisk, unsigned nImage,
                                char *pszFilename, unsigned cbFilename)
{
    int rc;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pszFilename=%#p cbFilename=%u\n",
                 pDisk, nImage, pszFilename, cbFilename));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszFilename) && *pszFilename,
                           ("pszFilename=%#p \"%s\"\n", pszFilename, pszFilename),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cbFilename,
                           ("cbFilename=%u\n", cbFilename),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        size_t cb = strlen(pImage->pszFilename);
        if (cb <= cbFilename)
        {
            strcpy(pszFilename, pImage->pszFilename);
            rc = VINF_SUCCESS;
        }
        else
        {
            strncpy(pszFilename, pImage->pszFilename, cbFilename - 1);
            pszFilename[cbFilename - 1] = '\0';
            rc = VERR_BUFFER_OVERFLOW;
        }
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc, pszFilename=\"%s\"\n", rc, pszFilename));
    return rc;
}

/**
 * Get the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VDGetComment(PVBOXHDD pDisk, unsigned nImage,
                               char *pszComment, unsigned cbComment)
{
    int rc;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pszComment=%#p cbComment=%u\n",
                 pDisk, nImage, pszComment, cbComment));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszComment),
                           ("pszComment=%#p \"%s\"\n", pszComment, pszComment),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cbComment,
                           ("cbComment=%u\n", cbComment),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        rc = pImage->Backend->pfnGetComment(pImage->pBackendData, pszComment,
                                            cbComment);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc, pszComment=\"%s\"\n", rc, pszComment));
    return rc;
}

/**
 * Changes the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VDSetComment(PVBOXHDD pDisk, unsigned nImage,
                               const char *pszComment)
{
    int rc;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pszComment=%#p \"%s\"\n",
                 pDisk, nImage, pszComment, pszComment));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pszComment) || pszComment == NULL,
                           ("pszComment=%#p \"%s\"\n", pszComment, pszComment),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        rc = pImage->Backend->pfnSetComment(pImage->pBackendData, pszComment);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * Get UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image creation UUID.
 */
VBOXDDU_DECL(int) VDGetUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid)
{
    int rc;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pUuid=%#p\n", pDisk, nImage, pUuid));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pUuid),
                           ("pUuid=%#p\n", pUuid),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        rc = pImage->Backend->pfnGetUuid(pImage->pBackendData, pUuid);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc, Uuid={%RTuuid}\n", rc, pUuid));
    return rc;
}

/**
 * Set the image's UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid)
{
    int rc;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pUuid=%#p {%RTuuid}\n",
                 pDisk, nImage, pUuid, pUuid));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        AssertMsgBreakStmt(VALID_PTR(pUuid) || pUuid == NULL,
                           ("pUuid=%#p\n", pUuid),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        RTUUID Uuid;
        if (!pUuid)
        {
            RTUuidCreate(&Uuid);
            pUuid = &Uuid;
        }
        rc = pImage->Backend->pfnSetUuid(pImage->pBackendData, pUuid);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get last modification UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification UUID.
 */
VBOXDDU_DECL(int) VDGetModificationUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pUuid=%#p\n", pDisk, nImage, pUuid));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pUuid),
                           ("pUuid=%#p\n", pUuid),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        rc = pImage->Backend->pfnGetModificationUuid(pImage->pBackendData,
                                                     pUuid);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc, Uuid={%RTuuid}\n", rc, pUuid));
    return rc;
}

/**
 * Set the image's last modification UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New modification UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetModificationUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid)
{
    int rc;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pUuid=%#p {%RTuuid}\n",
                 pDisk, nImage, pUuid, pUuid));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pUuid) || pUuid == NULL,
                           ("pUuid=%#p\n", pUuid),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        RTUUID Uuid;
        if (!pUuid)
        {
            RTUuidCreate(&Uuid);
            pUuid = &Uuid;
        }
        rc = pImage->Backend->pfnSetModificationUuid(pImage->pBackendData,
                                                     pUuid);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Get parent UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the parent image UUID.
 */
VBOXDDU_DECL(int) VDGetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PRTUUID pUuid)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pUuid=%#p\n", pDisk, nImage, pUuid));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pUuid),
                           ("pUuid=%#p\n", pUuid),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        rc = pImage->Backend->pfnGetParentUuid(pImage->pBackendData, pUuid);
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc, Uuid={%RTuuid}\n", rc, pUuid));
    return rc;
}

/**
 * Set the image's parent UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New parent UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PCRTUUID pUuid)
{
    int rc;
    int rc2;
    bool fLockWrite = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pUuid=%#p {%RTuuid}\n",
                 pDisk, nImage, pUuid, pUuid));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pUuid) || pUuid == NULL,
                           ("pUuid=%#p\n", pUuid),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        RTUUID Uuid;
        if (!pUuid)
        {
            RTUuidCreate(&Uuid);
            pUuid = &Uuid;
        }
        rc = pImage->Backend->pfnSetParentUuid(pImage->pBackendData, pUuid);
    } while (0);

    if (RT_UNLIKELY(fLockWrite))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * Debug helper - dumps all opened images in HDD container into the log file.
 *
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(void) VDDumpImages(PVBOXHDD pDisk)
{
    int rc2;
    bool fLockRead = false;

    do
    {
        /* sanity check */
        AssertPtrBreak(pDisk);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        if (!pDisk->pInterfaceErrorCallbacks || !VALID_PTR(pDisk->pInterfaceErrorCallbacks->pfnMessage))
            pDisk->pInterfaceErrorCallbacks->pfnMessage = vdLogMessage;

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        vdMessageWrapper(pDisk, "--- Dumping VD Disk, Images=%u\n", pDisk->cImages);
        for (PVDIMAGE pImage = pDisk->pBase; pImage; pImage = pImage->pNext)
        {
            vdMessageWrapper(pDisk, "Dumping VD image \"%s\" (Backend=%s)\n",
                             pImage->pszFilename, pImage->Backend->pszBackendName);
            pImage->Backend->pfnDump(pImage->pBackendData);
        }
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }
}

/**
 * Query if asynchronous operations are supported for this disk.
 *
 * @returns VBox status code.
 * @returns VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to the HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pfAIOSupported  Where to store if async IO is supported.
 */
VBOXDDU_DECL(int) VDImageIsAsyncIOSupported(PVBOXHDD pDisk, unsigned nImage, bool *pfAIOSupported)
{
    int rc = VINF_SUCCESS;
    int rc2;
    bool fLockRead = false;

    LogFlowFunc(("pDisk=%#p nImage=%u pfAIOSupported=%#p\n", pDisk, nImage, pfAIOSupported));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(VALID_PTR(pfAIOSupported),
                           ("pfAIOSupported=%#p\n", pfAIOSupported),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
        AssertPtrBreakStmt(pImage, rc = VERR_VD_IMAGE_NOT_FOUND);

        if (pImage->Backend->uBackendCaps & VD_CAP_ASYNC)
            *pfAIOSupported = pImage->Backend->pfnIsAsyncIOSupported(pImage->pBackendData);
        else
            *pfAIOSupported = false;
    } while (0);

    if (RT_UNLIKELY(fLockRead))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc, fAIOSupported=%u\n", rc, *pfAIOSupported));
    return rc;
}


VBOXDDU_DECL(int) VDAsyncRead(PVBOXHDD pDisk, uint64_t uOffset, size_t cbRead,
                              PCRTSGSEG paSeg, unsigned cSeg,
                              PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                              void *pvUser1, void *pvUser2)
{
    int rc = VERR_VD_BLOCK_FREE;
    int rc2;
    bool fLockRead = false;
    PVDIOCTX pIoCtx = NULL;

    LogFlowFunc(("pDisk=%#p uOffset=%llu paSeg=%p cSeg=%u cbRead=%zu pvUser1=%#p pvUser2=%#p\n",
                 pDisk, uOffset, paSeg, cSeg, cbRead, pvUser1, pvUser2));

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(cbRead,
                           ("cbRead=%zu\n", cbRead),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(paSeg),
                           ("paSeg=%#p\n", paSeg),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cSeg,
                           ("cSeg=%zu\n", cSeg),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartRead(pDisk);
        AssertRC(rc2);
        fLockRead = true;

        AssertMsgBreakStmt(uOffset + cbRead <= pDisk->cbSize,
                           ("uOffset=%llu cbRead=%zu pDisk->cbSize=%llu\n",
                            uOffset, cbRead, pDisk->cbSize),
                           rc = VERR_INVALID_PARAMETER);

        pIoCtx = vdIoCtxRootAlloc(pDisk, VDIOCTXTXDIR_READ, uOffset,
                                  cbRead, paSeg, cSeg,
                                  pfnComplete, pvUser1, pvUser2,
                                  NULL, vdReadHelperAsync);
        if (!pIoCtx)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pIoCtx->pImage = pDisk->pLast;
        AssertPtrBreakStmt(pIoCtx->pImage, rc = VERR_VD_NOT_OPENED);

        rc = vdIoCtxProcess(pIoCtx);
        if (rc == VINF_VD_ASYNC_IO_FINISHED)
        {
            if (ASMAtomicCmpXchgBool(&pIoCtx->fComplete, true, false))
                vdIoCtxFree(pDisk, pIoCtx);
            else
                rc = VERR_VD_ASYNC_IO_IN_PROGRESS; /* Let the other handler complete the request. */
        }
        else if (rc != VERR_VD_ASYNC_IO_IN_PROGRESS) /* Another error */
            vdIoCtxFree(pDisk, pIoCtx);

    } while (0);

    if (RT_UNLIKELY(fLockRead) && (   rc == VINF_VD_ASYNC_IO_FINISHED
                                   || rc != VERR_VD_ASYNC_IO_IN_PROGRESS))
    {
        rc2 = vdThreadFinishRead(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


VBOXDDU_DECL(int) VDAsyncWrite(PVBOXHDD pDisk, uint64_t uOffset, size_t cbWrite,
                               PCRTSGSEG paSeg, unsigned cSeg,
                               PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                               void *pvUser1, void *pvUser2)
{
    int rc;
    int rc2;
    bool fLockWrite = false;
    PVDIOCTX pIoCtx = NULL;

    LogFlowFunc(("pDisk=%#p uOffset=%llu paSeg=%p cSeg=%u cbWrite=%zu pvUser1=%#p pvUser2=%#p\n",
                 pDisk, uOffset, paSeg, cSeg, cbWrite, pvUser1, pvUser2));
    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        /* Check arguments. */
        AssertMsgBreakStmt(cbWrite,
                           ("cbWrite=%zu\n", cbWrite),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(VALID_PTR(paSeg),
                           ("paSeg=%#p\n", paSeg),
                           rc = VERR_INVALID_PARAMETER);
        AssertMsgBreakStmt(cSeg,
                           ("cSeg=%zu\n", cSeg),
                           rc = VERR_INVALID_PARAMETER);

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        AssertMsgBreakStmt(uOffset + cbWrite <= pDisk->cbSize,
                           ("uOffset=%llu cbWrite=%zu pDisk->cbSize=%llu\n",
                            uOffset, cbWrite, pDisk->cbSize),
                           rc = VERR_INVALID_PARAMETER);

        pIoCtx = vdIoCtxRootAlloc(pDisk, VDIOCTXTXDIR_WRITE, uOffset,
                                  cbWrite, paSeg, cSeg,
                                  pfnComplete, pvUser1, pvUser2,
                                  NULL, vdWriteHelperAsync);
        if (!pIoCtx)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        PVDIMAGE pImage = pDisk->pLast;
        AssertPtrBreakStmt(pImage, rc = VERR_VD_NOT_OPENED);
        pIoCtx->pImage = pImage;

        rc = vdIoCtxProcess(pIoCtx);
        if (rc == VINF_VD_ASYNC_IO_FINISHED)
        {
            if (ASMAtomicCmpXchgBool(&pIoCtx->fComplete, true, false))
                vdIoCtxFree(pDisk, pIoCtx);
            else
                rc = VERR_VD_ASYNC_IO_IN_PROGRESS; /* Let the other handler complete the request. */
        }
        else if (rc != VERR_VD_ASYNC_IO_IN_PROGRESS) /* Another error */
            vdIoCtxFree(pDisk, pIoCtx);
    } while (0);

    if (RT_UNLIKELY(fLockWrite) && (   rc == VINF_VD_ASYNC_IO_FINISHED
                                    || rc != VERR_VD_ASYNC_IO_IN_PROGRESS))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


VBOXDDU_DECL(int) VDAsyncFlush(PVBOXHDD pDisk, PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                               void *pvUser1, void *pvUser2)
{
    int rc;
    int rc2;
    bool fLockWrite = false;
    PVDIOCTX pIoCtx = NULL;

    LogFlowFunc(("pDisk=%#p\n", pDisk));

    do
    {
        /* sanity check */
        AssertPtrBreakStmt(pDisk, rc = VERR_INVALID_PARAMETER);
        AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

        rc2 = vdThreadStartWrite(pDisk);
        AssertRC(rc2);
        fLockWrite = true;

        pIoCtx = vdIoCtxRootAlloc(pDisk, VDIOCTXTXDIR_FLUSH, 0,
                                  0, NULL, 0,
                                  pfnComplete, pvUser1, pvUser2,
                                  NULL, vdFlushHelperAsync);
        if (!pIoCtx)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        PVDIMAGE pImage = pDisk->pLast;
        AssertPtrBreakStmt(pImage, rc = VERR_VD_NOT_OPENED);
        pIoCtx->pImage = pImage;

        rc = vdIoCtxProcess(pIoCtx);
        if (rc == VINF_VD_ASYNC_IO_FINISHED)
        {
            if (ASMAtomicCmpXchgBool(&pIoCtx->fComplete, true, false))
                vdIoCtxFree(pDisk, pIoCtx);
            else
                rc = VERR_VD_ASYNC_IO_IN_PROGRESS; /* Let the other handler complete the request. */
        }
        else if (rc != VERR_VD_ASYNC_IO_IN_PROGRESS) /* Another error */
            vdIoCtxFree(pDisk, pIoCtx);
    } while (0);

    if (RT_UNLIKELY(fLockWrite) && (   rc == VINF_VD_ASYNC_IO_FINISHED
                                    || rc != VERR_VD_ASYNC_IO_IN_PROGRESS))
    {
        rc2 = vdThreadFinishWrite(pDisk);
        AssertRC(rc2);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

