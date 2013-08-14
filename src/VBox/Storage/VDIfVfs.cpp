/* $Id$ */
/** @file
 * Virtual Disk Image (VDI), I/O interface to IPRT VFS I/O stream glue.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * The internal data of a DVM volume I/O stream.
 */
typedef struct VDIFVFSIOS
{
    /** The VD I/O interface we wrap. */
    PVDINTERFACEIO  pVDIfsIo;
    /** User pointer to pass to the VD I/O interface methods. */
    void           *pvStorage;
    /** The current stream position relative to the VDIfCreateVfsStream call. */
    uint64_t        offCurPos;
} VDIFVFSIOS;
/** Pointer to a the internal data of a DVM volume file. */
typedef VDIFVFSIOS *PVDIFVFSIOS;



/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) vdIfVfsIos_Close(void *pvThis)
{
    /* We don't close anything. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) vdIfVfsIos_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    NOREF(pvThis);
    NOREF(pObjInfo);
    NOREF(enmAddAttr);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) vdIfVfsIos_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PVDIFVFSIOS pThis = (PVDIFVFSIOS)pvThis;
    Assert(pSgBuf->cSegs == 1); NOREF(fBlocking);

    /*
     * This may end up being a little more complicated, esp. wrt VERR_EOF.
     */
    if (off == -1)
        off = pThis->offCurPos;
    int rc = vdIfIoFileReadSync(pThis->pVDIfsIo, pThis->pvStorage, off, pSgBuf[0].pvSegCur, pSgBuf->paSegs[0].cbSeg, pcbRead);
    if (RT_SUCCESS(rc))
        pThis->offCurPos = off + (pcbRead ? *pcbRead : pSgBuf->paSegs[0].cbSeg);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) vdIfVfsIos_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PVDIFVFSIOS pThis = (PVDIFVFSIOS)pvThis;
    Assert(pSgBuf->cSegs == 1); NOREF(fBlocking);

    /*
     * This may end up being a little more complicated, esp. wrt VERR_EOF.
     */
    if (off == -1)
        off = pThis->offCurPos;
    int rc = vdIfIoFileWriteSync(pThis->pVDIfsIo, pThis->pvStorage, off, pSgBuf[0].pvSegCur, pSgBuf->paSegs[0].cbSeg, pcbWritten);
    if (RT_SUCCESS(rc))
        pThis->offCurPos = off + (pcbWritten ? *pcbWritten : pSgBuf->paSegs[0].cbSeg);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) vdIfVfsIos_Flush(void *pvThis)
{
    PVDIFVFSIOS pThis = (PVDIFVFSIOS)pvThis;
    return vdIfIoFileFlushSync(pThis->pVDIfsIo, pThis->pvStorage);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) vdIfVfsIos_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
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
static DECLCALLBACK(int) vdIfVfsIos_Tell(void *pvThis, PRTFOFF poffActual)
{
    PVDIFVFSIOS pThis = (PVDIFVFSIOS)pvThis;
    *poffActual = pThis->offCurPos;
    return VINF_SUCCESS;
}

/**
 * Standard file operations.
 */
DECL_HIDDEN_CONST(const RTVFSIOSTREAMOPS) g_vdIfVfsStdIosOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "VDIfIos",
        vdIfVfsIos_Close,
        vdIfVfsIos_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    RTVFSIOSTREAMOPS_FEAT_NO_SG,
    vdIfVfsIos_Read,
    vdIfVfsIos_Write,
    vdIfVfsIos_Flush,
    vdIfVfsIos_PollOne,
    vdIfVfsIos_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION,

};

VBOXDDU_DECL(int) VDIfCreateVfsStream(PVDINTERFACEIO pVDIfsIo, void *pvStorage, uint32_t fFlags, PRTVFSIOSTREAM phVfsIos)
{
    AssertPtrReturn(pVDIfsIo, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVfsIos, VERR_INVALID_POINTER);

    /*
     * Create the volume file.
     */
    RTVFSIOSTREAM hVfsIos;
    PVDIFVFSIOS pThis;
    int rc = RTVfsNewIoStream(&g_vdIfVfsStdIosOps, sizeof(*pThis), fFlags,
                              NIL_RTVFS, NIL_RTVFSLOCK, &hVfsIos, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->pVDIfsIo  = pVDIfsIo;
        pThis->pvStorage = pvStorage;
        pThis->offCurPos = 0;

        *phVfsIos = hVfsIos;
        return VINF_SUCCESS;
    }

    return rc;
}

