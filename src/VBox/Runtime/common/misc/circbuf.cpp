/* $Id$ */
/** @file
 * IPRT - Lock Free Circular Buffer
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


/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/
#include <iprt/circbuf.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>

/******************************************************************************
 *   Public Functions                                                         *
 ******************************************************************************/

RTDECL(int) RTCircBufCreate(PRTCIRCBUF *ppBuf, size_t cbSize)
{
    /* Validate input. */
    AssertPtrReturn(ppBuf, VERR_INVALID_POINTER);
    AssertReturn(cbSize > 0, VERR_INVALID_PARAMETER);

    PRTCIRCBUF pTmpBuf;
    pTmpBuf = (PRTCIRCBUF)RTMemAllocZ(sizeof(RTCIRCBUF));
    if (!pTmpBuf)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    do
    {
        pTmpBuf->pvBuf = RTMemAlloc(cbSize);
        if (!pTmpBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pTmpBuf->cbBufSize = cbSize;
        *ppBuf = pTmpBuf;
    }while (0);

    if (RT_FAILURE(rc))
        RTMemFree(pTmpBuf);

    return rc;
}

RTDECL(void) RTCircBufDestroy(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtrNull(pBuf);

    if (pBuf)
    {
        if (pBuf->pvBuf)
            RTMemFree(pBuf->pvBuf);
        RTMemFree(pBuf);
    }
}

RTDECL(void) RTCircBufReset(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtr(pBuf);

    pBuf->uReadPos = 0;
    pBuf->uWritePos = 0;
    pBuf->cbBufUsed = 0;
}

RTDECL(size_t) RTCircBufFree(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtrReturn(pBuf, 0);

    size_t cbSize = 0;
    ASMAtomicReadSize(&pBuf->cbBufUsed, &cbSize);
    return pBuf->cbBufSize - cbSize;
}

RTDECL(size_t) RTCircBufUsed(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtrReturn(pBuf, 0);

    size_t cbSize = 0;
    ASMAtomicReadSize(&pBuf->cbBufUsed, &cbSize);
    return cbSize;
}

RTDECL(size_t) RTCircBufSize(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtrReturn(pBuf, 0);

    return pBuf->cbBufSize;
}

RTDECL(void) RTCircBufAcquireReadBlock(PRTCIRCBUF pBuf, size_t cbReqSize, void **ppvStart, size_t *pcbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);
    Assert(cbReqSize > 0);
    AssertPtr(ppvStart);
    AssertPtr(pcbSize);

    size_t uUsed = 0;
    size_t uSize = 0;

    *ppvStart = 0;
    *pcbSize = 0;

    /* How much is in use? */
    ASMAtomicReadSize(&pBuf->cbBufUsed, &uUsed);
    if (uUsed > 0)
    {
        /* Get the size out of the requested size, the read block till the end
         * of the buffer & the currently used size. */
        uSize = RT_MIN(cbReqSize, RT_MIN(pBuf->cbBufSize - pBuf->uReadPos, uUsed));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current read
             * position. */
            *ppvStart = (char*)pBuf->pvBuf + pBuf->uReadPos;
            *pcbSize = uSize;
        }
    }
}

RTDECL(void) RTCircBufReleaseReadBlock(PRTCIRCBUF pBuf, size_t cbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);

    /* Split at the end of the buffer. */
    pBuf->uReadPos = (pBuf->uReadPos + cbSize) % pBuf->cbBufSize;

    size_t cbOld = 0;
    ASMAtomicSubSize(&pBuf->cbBufUsed, cbSize, &cbOld);
}

RTDECL(void) RTCircBufAcquireWriteBlock(PRTCIRCBUF pBuf, size_t cbReqSize, void **ppvStart, size_t *pcbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);
    Assert(cbReqSize > 0);
    AssertPtr(ppvStart);
    AssertPtr(pcbSize);

    size_t uFree;
    size_t uSize;

    *ppvStart = 0;
    *pcbSize = 0;

    /* How much is free? */
    size_t cbSize = 0;
    ASMAtomicReadSize(&pBuf->cbBufUsed, &cbSize);
    uFree = pBuf->cbBufSize - cbSize;
    if (uFree > 0)
    {
        /* Get the size out of the requested size, the write block till the end
         * of the buffer & the currently free size. */
        uSize = RT_MIN(cbReqSize, RT_MIN(pBuf->cbBufSize - pBuf->uWritePos, uFree));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current write
             * position. */
            *ppvStart = (char*)pBuf->pvBuf + pBuf->uWritePos;
            *pcbSize = uSize;
        }
    }
}

RTDECL(void) RTCircBufReleaseWriteBlock(PRTCIRCBUF pBuf, size_t cbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);

    /* Split at the end of the buffer. */
    pBuf->uWritePos = (pBuf->uWritePos + cbSize) % pBuf->cbBufSize;

    size_t cbOld = 0;
    ASMAtomicAddSize(&pBuf->cbBufUsed, cbSize, &cbOld);
}

