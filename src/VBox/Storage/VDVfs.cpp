/* $Id$ */
/** @file
 * Virtual Disk Container implementation. - VFS glue.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/sg.h>
#include <iprt/vfslowlevel.h>
#include <iprt/poll.h>
#include <VBox/vd.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * The internal data of a DVM volume I/O stream.
 */
typedef struct VDVFSFILE
{
    /** The volume the VFS file belongs to. */
    PVDISK         pDisk;
    /** Current position. */
    uint64_t       offCurPos;
    /** Flags given during creation. */
    uint32_t       fFlags;
} VDVFSFILE;
/** Pointer to a the internal data of a DVM volume file. */
typedef VDVFSFILE *PVDVFSFILE;

/**
 * VD read helper taking care of unaligned accesses.
 *
 * @return  VBox status code.
 * @param   pDisk    VD disk container.
 * @param   off      Offset to start reading from.
 * @param   pvBuf    Pointer to the buffer to read into.
 * @param   cbRead   Amount of bytes to read.
 */
static int vdReadHelper(PVDISK pDisk, uint64_t off, void *pvBuf, size_t cbRead)
{
    int rc = VINF_SUCCESS;

    /* Take shortcut if possible. */
    if (   off % 512 == 0
        && cbRead % 512 == 0)
        rc = VDRead(pDisk, off, pvBuf, cbRead);
    else
    {
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        uint8_t abBuf[512];

        /* Unaligned access, make it aligned. */
        if (off % 512 != 0)
        {
            uint64_t offAligned = off & ~(uint64_t)(512 - 1);
            size_t cbToCopy = 512 - (off - offAligned);
            rc = VDRead(pDisk, offAligned, abBuf, 512);
            if (RT_SUCCESS(rc))
            {
                memcpy(pbBuf, &abBuf[off - offAligned], cbToCopy);
                pbBuf  += cbToCopy;
                off    += cbToCopy;
                cbRead -= cbToCopy;
            }
        }

        if (   RT_SUCCESS(rc)
            && (cbRead & ~(uint64_t)(512 - 1)))
        {
            size_t cbReadAligned = cbRead & ~(uint64_t)(512 - 1);

            Assert(!(off % 512));
            rc = VDRead(pDisk, off, pbBuf, cbReadAligned);
            if (RT_SUCCESS(rc))
            {
                pbBuf  += cbReadAligned;
                off    += cbReadAligned;
                cbRead -= cbReadAligned;
            }
        }

        if (   RT_SUCCESS(rc)
            && cbRead)
        {
            Assert(cbRead < 512);
            Assert(!(off % 512));

            rc = VDRead(pDisk, off, abBuf, 512);
            if (RT_SUCCESS(rc))
                memcpy(pbBuf, abBuf, cbRead);
        }
    }

    return rc;
}


/**
 * VD write helper taking care of unaligned accesses.
 *
 * @return  VBox status code.
 * @param   pDisk    VD disk container.
 * @param   off      Offset to start writing to.
 * @param   pvBuf    Pointer to the buffer to read from.
 * @param   cbWrite  Amount of bytes to write.
 */
static int vdWriteHelper(PVDISK pDisk, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    int rc = VINF_SUCCESS;

    /* Take shortcut if possible. */
    if (   off % 512 == 0
        && cbWrite % 512 == 0)
        rc = VDWrite(pDisk, off, pvBuf, cbWrite);
    else
    {
        uint8_t *pbBuf = (uint8_t *)pvBuf;
        uint8_t abBuf[512];

        /* Unaligned access, make it aligned. */
        if (off % 512 != 0)
        {
            uint64_t offAligned = off & ~(uint64_t)(512 - 1);
            size_t cbToCopy = 512 - (off - offAligned);
            rc = VDRead(pDisk, offAligned, abBuf, 512);
            if (RT_SUCCESS(rc))
            {
                memcpy(&abBuf[off - offAligned], pbBuf, cbToCopy);
                rc = VDWrite(pDisk, offAligned, abBuf, 512);

                pbBuf   += cbToCopy;
                off     += cbToCopy;
                cbWrite -= cbToCopy;
            }
        }

        if (   RT_SUCCESS(rc)
            && (cbWrite & ~(uint64_t)(512 - 1)))
        {
            size_t cbWriteAligned = cbWrite & ~(uint64_t)(512 - 1);

            Assert(!(off % 512));
            rc = VDWrite(pDisk, off, pbBuf, cbWriteAligned);
            if (RT_SUCCESS(rc))
            {
                pbBuf   += cbWriteAligned;
                off     += cbWriteAligned;
                cbWrite -= cbWriteAligned;
            }
        }

        if (   RT_SUCCESS(rc)
            && cbWrite)
        {
            Assert(cbWrite < 512);
            Assert(!(off % 512));

            rc = VDRead(pDisk, off, abBuf, 512);
            if (RT_SUCCESS(rc))
            {
                memcpy(abBuf, pbBuf, cbWrite);
                rc = VDWrite(pDisk, off, abBuf, 512);
            }
        }
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) vdVfsFile_Close(void *pvThis)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;

    if (pThis->fFlags & VD_VFSFILE_DESTROY_ON_RELEASE)
        VDDestroy(pThis->pDisk);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) vdVfsFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo,
                                               RTFSOBJATTRADD enmAddAttr)
{
    NOREF(pvThis);
    NOREF(pObjInfo);
    NOREF(enmAddAttr);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) vdVfsFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;
    int rc = VINF_SUCCESS;

    Assert(pSgBuf->cSegs == 1);
    NOREF(fBlocking);

    /*
     * Find the current position and check if it's within the volume.
     */
    uint64_t offUnsigned = off < 0 ? pThis->offCurPos : (uint64_t)off;
    if (offUnsigned >= VDGetSize(pThis->pDisk, VD_LAST_IMAGE))
    {
        if (pcbRead)
        {
            *pcbRead = 0;
            pThis->offCurPos = offUnsigned;
            return VINF_EOF;
        }
        return VERR_EOF;
    }

    size_t cbLeftToRead;
    if (offUnsigned + pSgBuf->paSegs[0].cbSeg > VDGetSize(pThis->pDisk, VD_LAST_IMAGE))
    {
        if (!pcbRead)
            return VERR_EOF;
        *pcbRead = cbLeftToRead = (size_t)(VDGetSize(pThis->pDisk, VD_LAST_IMAGE) - offUnsigned);
    }
    else
    {
        cbLeftToRead = pSgBuf->paSegs[0].cbSeg;
        if (pcbRead)
            *pcbRead = cbLeftToRead;
    }

    /*
     * Ok, we've got a valid stretch within the file.  Do the reading.
     */
    if (cbLeftToRead > 0)
    {
        rc = vdReadHelper(pThis->pDisk, (uint64_t)off, pSgBuf->paSegs[0].pvSeg, cbLeftToRead);
        if (RT_SUCCESS(rc))
            offUnsigned += cbLeftToRead;
    }

    pThis->offCurPos = offUnsigned;
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) vdVfsFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;
    int rc = VINF_SUCCESS;

    Assert(pSgBuf->cSegs == 1);
    NOREF(fBlocking);

    /*
     * Find the current position and check if it's within the volume.
     * Writing beyond the end of a volume is not supported.
     */
    uint64_t offUnsigned = off < 0 ? pThis->offCurPos : (uint64_t)off;
    if (offUnsigned >= VDGetSize(pThis->pDisk, VD_LAST_IMAGE))
    {
        if (pcbWritten)
        {
            *pcbWritten = 0;
            pThis->offCurPos = offUnsigned;
        }
        return VERR_NOT_SUPPORTED;
    }

    size_t cbLeftToWrite;
    if (offUnsigned + pSgBuf->paSegs[0].cbSeg > VDGetSize(pThis->pDisk, VD_LAST_IMAGE))
    {
        if (!pcbWritten)
            return VERR_EOF;
        *pcbWritten = cbLeftToWrite = (size_t)(VDGetSize(pThis->pDisk, VD_LAST_IMAGE) - offUnsigned);
    }
    else
    {
        cbLeftToWrite = pSgBuf->paSegs[0].cbSeg;
        if (pcbWritten)
            *pcbWritten = cbLeftToWrite;
    }

    /*
     * Ok, we've got a valid stretch within the file.  Do the reading.
     */
    if (cbLeftToWrite > 0)
    {
        rc = vdWriteHelper(pThis->pDisk, (uint64_t)off, pSgBuf->paSegs[0].pvSeg, cbLeftToWrite);
        if (RT_SUCCESS(rc))
            offUnsigned += cbLeftToWrite;
    }

    pThis->offCurPos = offUnsigned;
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) vdVfsFile_Flush(void *pvThis)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;
    return VDFlush(pThis->pDisk);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) vdVfsFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                              uint32_t *pfRetEvents)
{
    NOREF(pvThis);
    int rc;
    if (fEvents != RTPOLL_EVT_ERROR)
    {
        *pfRetEvents = fEvents & ~RTPOLL_EVT_ERROR;
        rc = VINF_SUCCESS;
    }
    else
        rc = RTVfsUtilDummyPollOne(fEvents, cMillies, fIntr, pfRetEvents);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) vdVfsFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;
    *poffActual = pThis->offCurPos;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetMode}
 */
static DECLCALLBACK(int) vdVfsFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    NOREF(pvThis);
    NOREF(fMode);
    NOREF(fMask);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) vdVfsFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                              PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    NOREF(pvThis);
    NOREF(pAccessTime);
    NOREF(pModificationTime);
    NOREF(pChangeTime);
    NOREF(pBirthTime);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) vdVfsFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    NOREF(pvThis);
    NOREF(uid);
    NOREF(gid);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) vdVfsFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;

    /*
     * Seek relative to which position.
     */
    uint64_t offWrt;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offWrt = 0;
            break;

        case RTFILE_SEEK_CURRENT:
            offWrt = pThis->offCurPos;
            break;

        case RTFILE_SEEK_END:
            offWrt = VDGetSize(pThis->pDisk, VD_LAST_IMAGE);
            break;

        default:
            return VERR_INTERNAL_ERROR_5;
    }

    /*
     * Calc new position, take care to stay within bounds.
     *
     * @todo: Setting position beyond the end of the disk does not make sense.
     */
    uint64_t offNew;
    if (offSeek == 0)
        offNew = offWrt;
    else if (offSeek > 0)
    {
        offNew = offWrt + offSeek;
        if (   offNew < offWrt
            || offNew > RTFOFF_MAX)
            offNew = RTFOFF_MAX;
    }
    else if ((uint64_t)-offSeek < offWrt)
        offNew = offWrt + offSeek;
    else
        offNew = 0;

    /*
     * Update the state and set return value.
     */
    pThis->offCurPos = offNew;

    *poffActual = offNew;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) vdVfsFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PVDVFSFILE pThis = (PVDVFSFILE)pvThis;
    *pcbFile = VDGetSize(pThis->pDisk, VD_LAST_IMAGE);
    return VINF_SUCCESS;
}


/**
 * Standard file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_vdVfsStdFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "VDFile",
            vdVfsFile_Close,
            vdVfsFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        vdVfsFile_Read,
        vdVfsFile_Write,
        vdVfsFile_Flush,
        vdVfsFile_PollOne,
        vdVfsFile_Tell,
        NULL /*Skip*/,
        NULL /*ZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    /*RTVFSIOFILEOPS_FEAT_NO_AT_OFFSET*/ 0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        vdVfsFile_SetMode,
        vdVfsFile_SetTimes,
        vdVfsFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    vdVfsFile_Seek,
    vdVfsFile_QuerySize,
    RTVFSFILEOPS_VERSION
};


VBOXDDU_DECL(int) VDCreateVfsFileFromDisk(PVDISK pDisk, uint32_t fFlags,
                                          PRTVFSFILE phVfsFile)
{
    AssertPtrReturn(pDisk, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVfsFile, VERR_INVALID_POINTER);
    AssertReturn((fFlags & ~VD_VFSFILE_FLAGS_MASK) == 0, VERR_INVALID_PARAMETER);

    /*
     * Create the volume file.
     */
    RTVFSFILE  hVfsFile;
    PVDVFSFILE pThis;
    int rc = RTVfsNewFile(&g_vdVfsStdFileOps, sizeof(*pThis), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_WRITE,
                          NIL_RTVFS, NIL_RTVFSLOCK, &hVfsFile, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->offCurPos = 0;
        pThis->pDisk     = pDisk;
        pThis->fFlags    = fFlags;

        *phVfsFile = hVfsFile;
        return VINF_SUCCESS;
    }

    return rc;
}

