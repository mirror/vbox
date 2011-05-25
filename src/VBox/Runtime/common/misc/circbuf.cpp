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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/circbuf.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>


RTDECL(int) RTCircBufCreate(PRTCIRCBUF *ppBuf, size_t cbSize)
{
    /* Validate input. */
    AssertPtrReturn(ppBuf, VERR_INVALID_POINTER);
    AssertReturn(cbSize > 0, VERR_INVALID_PARAMETER);

    PRTCIRCBUF pTmpBuf;
    pTmpBuf = (PRTCIRCBUF)RTMemAllocZ(sizeof(RTCIRCBUF));
    if (!pTmpBuf)
        return VERR_NO_MEMORY;

    pTmpBuf->pvBuf = RTMemAlloc(cbSize);
    if (pTmpBuf->pvBuf)
    {
        pTmpBuf->cbBufSize = cbSize;
        *ppBuf = pTmpBuf;
        return VINF_SUCCESS;
    }

    RTMemFree(pTmpBuf);
    return VERR_NO_MEMORY;
}


RTDECL(void) RTCircBufDestroy(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    if (!pBuf)
        return;
    AssertPtr(pBuf);
    RTMemFree(pBuf->pvBuf);
    RTMemFree(pBuf);
}


RTDECL(void) RTCircBufReset(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtr(pBuf);

    pBuf->uReadPos  = 0;
    pBuf->uWritePos = 0;
    pBuf->cbBufUsed = 0;
}


RTDECL(size_t) RTCircBufFree(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtrReturn(pBuf, 0);

    return pBuf->cbBufSize - ASMAtomicReadZ(&pBuf->cbBufUsed);
}


RTDECL(size_t) RTCircBufUsed(PRTCIRCBUF pBuf)
{
    /* Validate input. */
    AssertPtrReturn(pBuf, 0);

    return ASMAtomicReadZ(&pBuf->cbBufUsed);
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

    *ppvStart = 0;
    *pcbSize = 0;

    /* How much is in use? */
    size_t cbUsed = ASMAtomicReadZ(&pBuf->cbBufUsed);
    if (cbUsed > 0)
    {
        /* Get the size out of the requested size, the read block till the end
         * of the buffer & the currently used size. */
        size_t cbSize = RT_MIN(cbReqSize, RT_MIN(pBuf->cbBufSize - pBuf->uReadPos, cbUsed));
        if (cbSize > 0)
        {
            /* Return the pointer address which point to the current read
             * position. */
            *ppvStart = (char *)pBuf->pvBuf + pBuf->uReadPos;
            *pcbSize = cbSize;
        }
    }
}


RTDECL(void) RTCircBufReleaseReadBlock(PRTCIRCBUF pBuf, size_t cbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);

    /* Split at the end of the buffer. */
    pBuf->uReadPos = (pBuf->uReadPos + cbSize) % pBuf->cbBufSize;

    ASMAtomicSubZ(&pBuf->cbBufUsed, cbSize);
}


RTDECL(void) RTCircBufAcquireWriteBlock(PRTCIRCBUF pBuf, size_t cbReqSize, void **ppvStart, size_t *pcbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);
    Assert(cbReqSize > 0);
    AssertPtr(ppvStart);
    AssertPtr(pcbSize);

    *ppvStart = 0;
    *pcbSize = 0;

    /* How much is free? */
    size_t cbFree = pBuf->cbBufSize - ASMAtomicReadZ(&pBuf->cbBufUsed);
    if (cbFree > 0)
    {
        /* Get the size out of the requested size, the write block till the end
         * of the buffer & the currently free size. */
        size_t cbSize = RT_MIN(cbReqSize, RT_MIN(pBuf->cbBufSize - pBuf->uWritePos, cbFree));
        if (cbSize > 0)
        {
            /* Return the pointer address which point to the current write
             * position. */
            *ppvStart = (char*)pBuf->pvBuf + pBuf->uWritePos;
            *pcbSize = cbSize;
        }
    }
}


RTDECL(void) RTCircBufReleaseWriteBlock(PRTCIRCBUF pBuf, size_t cbSize)
{
    /* Validate input. */
    AssertPtr(pBuf);

    /* Split at the end of the buffer. */
    pBuf->uWritePos = (pBuf->uWritePos + cbSize) % pBuf->cbBufSize;

    size_t cbOldIgnored = 0;
    ASMAtomicAddZ(&pBuf->cbBufUsed, cbSize);
}

