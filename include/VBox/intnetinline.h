/* $Id$ */
/** @file
 * INTNET - Internal Networking, Inlined Code. (DEV,++)
 *
 * This is all inlined because it's too tedious to create 2-3 libraries to
 * contain it all.  Large parts of this header is only accessible from C++
 * sources because of mixed code and variables.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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

#ifndef ___VBox_intnetinline_h
#define ___VBox_intnetinline_h

#include <VBox/intnet.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <VBox/log.h>



/**
 * Valid internal networking frame type.
 *
 * @returns  true / false.
 * @param   u16Type             The frame type to check.
 */
DECLINLINE(bool) INETNETIsValidFrameType(uint16_t u16Type)
{
    if (RT_LIKELY(   u16Type == INTNETHDR_TYPE_FRAME
                  || u16Type == INTNETHDR_TYPE_GSO
                  || u16Type == INTNETHDR_TYPE_PADDING))
        return true;
    return false;
}


/**
 * Partly initializes a scatter / gather buffer, leaving the segments to the
 * caller.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 * @param   cbTotal     The total size.
 * @param   cSegs       The number of segments.
 * @param   cSegsUsed   The number of used segments.
 */
DECLINLINE(void) INTNETSgInitTempSegs(PINTNETSG pSG, uint32_t cbTotal, unsigned cSegs, unsigned cSegsUsed)
{
    pSG->pvOwnerData    = NULL;
    pSG->pvUserData     = NULL;
    pSG->pvUserData2    = NULL;
    pSG->cbTotal        = cbTotal;
    pSG->cUsers         = 1;
    pSG->fFlags         = INTNETSG_FLAGS_TEMP;
    pSG->GsoCtx.u8Type  = (uint8_t)PDMNETWORKGSOTYPE_INVALID;
    pSG->GsoCtx.cbHdrs  = 0;
    pSG->GsoCtx.cbMaxSeg= 0;
    pSG->GsoCtx.offHdr1 = 0;
    pSG->GsoCtx.cbHdr1  = 0;
    pSG->GsoCtx.offHdr2 = 0;
    pSG->GsoCtx.cbHdr2  = 0;
#if ARCH_BITS == 64
    pSG->uPadding       = 0;
#endif
    pSG->cSegsAlloc     = (uint16_t)cSegs;
    Assert(pSG->cSegsAlloc == cSegs);
    pSG->cSegsUsed      = (uint16_t)cSegsUsed;
    Assert(pSG->cSegsUsed == cSegsUsed);
    Assert(cSegs >= cSegsUsed);
}


/**
 * Partly initializes a scatter / gather buffer w/ GSO, leaving the segments to
 * the caller.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 * @param   cbTotal     The total size.
 * @param   cSegs       The number of segments.
 * @param   cSegsUsed   The number of used segments.
 * @param   pGso        The GSO context.
 */
DECLINLINE(void) INTNETSgInitTempSegsGso(PINTNETSG pSG, uint32_t cbTotal, unsigned cSegs,
                                         unsigned cSegsUsed, PCPDMNETWORKGSO pGso)
{
    pSG->pvOwnerData    = NULL;
    pSG->pvUserData     = NULL;
    pSG->pvUserData2    = NULL;
    pSG->cbTotal        = cbTotal;
    pSG->cUsers         = 1;
    pSG->fFlags         = INTNETSG_FLAGS_TEMP;
    pSG->GsoCtx.u8Type  = pGso->u8Type;
    pSG->GsoCtx.cbHdrs  = pGso->cbHdrs;
    pSG->GsoCtx.cbMaxSeg= pGso->cbMaxSeg;
    pSG->GsoCtx.offHdr1 = pGso->offHdr1;
    pSG->GsoCtx.cbHdr1  = pGso->cbHdr1;
    pSG->GsoCtx.offHdr2 = pGso->offHdr2;
    pSG->GsoCtx.cbHdr2  = pGso->cbHdr2;
#if ARCH_BITS == 64
    pSG->uPadding       = 0;
#endif
    pSG->cSegsAlloc     = (uint16_t)cSegs;
    Assert(pSG->cSegsAlloc == cSegs);
    pSG->cSegsUsed      = (uint16_t)cSegsUsed;
    Assert(pSG->cSegsUsed == cSegsUsed);
    Assert(cSegs >= cSegsUsed);
}



/**
 * Initializes a scatter / gather buffer describing a simple linear buffer.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 * @param   pvFrame     Pointer to the frame
 * @param   cbFrame     The size of the frame.
 */
DECLINLINE(void) INTNETSgInitTemp(PINTNETSG pSG, void *pvFrame, uint32_t cbFrame)
{
    INTNETSgInitTempSegs(pSG, cbFrame, 1, 1);
    pSG->aSegs[0].Phys  = NIL_RTHCPHYS;
    pSG->aSegs[0].pv    = pvFrame;
    pSG->aSegs[0].cb    = cbFrame;
}

/**
 * Initializes a scatter / gather buffer describing a simple linear buffer.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 * @param   pvFrame     Pointer to the frame
 * @param   cbFrame     The size of the frame.
 * @param   pGso        The GSO context.
 */
DECLINLINE(void) INTNETSgInitTempGso(PINTNETSG pSG, void *pvFrame, uint32_t cbFrame, PCPDMNETWORKGSO pGso)
{
    INTNETSgInitTempSegsGso(pSG, cbFrame, 1, 1, pGso);
    pSG->aSegs[0].Phys  = NIL_RTHCPHYS;
    pSG->aSegs[0].pv    = pvFrame;
    pSG->aSegs[0].cb    = cbFrame;
}

#ifdef __cplusplus

/**
 * Get the amount of space available for writing.
 *
 * @returns Number of available bytes.
 * @param   pRingBuf        The ring buffer.
 */
DECLINLINE(uint32_t) INTNETRingGetWritable(PINTNETRINGBUF pRingBuf)
{
    uint32_t const offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    uint32_t const offWriteInt = ASMAtomicUoReadU32(&pRingBuf->offWriteInt);
    return offRead <= offWriteInt
        ?  pRingBuf->offEnd  - offWriteInt + offRead - pRingBuf->offStart - 1
        :  offRead - offWriteInt - 1;
}


/**
 * Checks if the ring has more for us to read.
 *
 * @returns Number of ready bytes.
 * @param   pRingBuf        The ring buffer.
 */
DECLINLINE(bool) INTNETRingHasMoreToRead(PINTNETRINGBUF pRingBuf)
{
    uint32_t const offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    uint32_t const offWriteCom = ASMAtomicUoReadU32(&pRingBuf->offWriteCom);
    return offRead != offWriteCom;
}


/**
 * Gets the next frame to read.
 *
 * @returns Pointer to the next frame.  NULL if done.
 * @param   pRingBuf        The ring buffer.
 */
DECLINLINE(PINTNETHDR) INTNETRingGetNextFrameToRead(PINTNETRINGBUF pRingBuf)
{
    uint32_t const offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    uint32_t const offWriteCom = ASMAtomicUoReadU32(&pRingBuf->offWriteCom);
    if (offRead == offWriteCom)
        return NULL;
    return (PINTNETHDR)((uint8_t *)pRingBuf + offRead);
}


/**
 * Get the amount of data ready for reading.
 *
 * @returns Number of ready bytes.
 * @param   pRingBuf        The ring buffer.
 */
DECLINLINE(uint32_t) INTNETRingGetReadable(PINTNETRINGBUF pRingBuf)
{
    uint32_t const offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    uint32_t const offWriteCom = ASMAtomicUoReadU32(&pRingBuf->offWriteCom);
    return offRead <= offWriteCom
        ?  offWriteCom - offRead
        :  pRingBuf->offEnd - offRead + offWriteCom - pRingBuf->offStart;
}


/**
 * Calculates the pointer to the frame.
 *
 * @returns Pointer to the start of the frame.
 * @param   pHdr        Pointer to the packet header
 * @param   pBuf        The buffer the header is within. Only used in strict builds.
 */
DECLINLINE(void *) INTNETHdrGetFramePtr(PCINTNETHDR pHdr, PCINTNETBUF pBuf)
{
    uint8_t *pu8 = (uint8_t *)pHdr + pHdr->offFrame;
#ifdef VBOX_STRICT
    const uintptr_t off = (uintptr_t)pu8 - (uintptr_t)pBuf;
    Assert(INETNETIsValidFrameType(pHdr->u16Type));
    Assert(off < pBuf->cbBuf);
    Assert(off + pHdr->cbFrame <= pBuf->cbBuf);
#endif
    NOREF(pBuf);
    return pu8;
}


/**
 * Calculates the pointer to the GSO context.
 *
 * ASSUMES the frame is a GSO frame.
 *
 * The GSO context is immediately followed by the headers and payload.  The size
 * is INTNETBUF::cbFrame - sizeof(PDMNETWORKGSO).
 *
 * @returns Pointer to the GSO context.
 * @param   pHdr        Pointer to the packet header
 * @param   pBuf        The buffer the header is within. Only used in strict builds.
 */
DECLINLINE(PPDMNETWORKGSO) INTNETHdrGetGsoContext(PCINTNETHDR pHdr, PCINTNETBUF pBuf)
{
    PPDMNETWORKGSO pGso = (PPDMNETWORKGSO)((uint8_t *)pHdr + pHdr->offFrame);
#ifdef VBOX_STRICT
    const uintptr_t off = (uintptr_t)pGso - (uintptr_t)pBuf;
    Assert(pHdr->u16Type == INTNETHDR_TYPE_GSO);
    Assert(off < pBuf->cbBuf);
    Assert(off + pHdr->cbFrame <= pBuf->cbBuf);
#endif
    NOREF(pBuf);
    return pGso;
}


/**
 * Skips to the next (read) frame in the buffer.
 *
 * @param   pRingBuf    The ring buffer in question.
 */
DECLINLINE(void) INTNETRingSkipFrame(PINTNETRINGBUF pRingBuf)
{
    uint32_t const  offReadOld  = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    PINTNETHDR      pHdr        = (PINTNETHDR)((uint8_t *)pRingBuf + offReadOld);
    Assert(offReadOld >= pRingBuf->offStart);
    Assert(offReadOld <  pRingBuf->offEnd);
    Assert(RT_ALIGN_PT(pHdr, INTNETHDR_ALIGNMENT, INTNETHDR *) == pHdr);
    Assert(INETNETIsValidFrameType(pHdr->u16Type));

    /* skip the frame */
    uint32_t        offReadNew  = offReadOld + pHdr->offFrame + pHdr->cbFrame;
    offReadNew = RT_ALIGN_32(offReadNew, INTNETHDR_ALIGNMENT);
    Assert(offReadNew <= pRingBuf->offEnd && offReadNew >= pRingBuf->offStart);
    if (offReadNew >= pRingBuf->offEnd)
        offReadNew = pRingBuf->offStart;
    Log2(("INTNETRingSkipFrame: offReadX: %#x -> %#x (1)\n", offReadOld, offReadNew));
#ifdef INTNET_POISON_READ_FRAMES
    memset((uint8_t *)pHdr + pHdr->offFrame, 0xfe, RT_ALIGN_32(pHdr->cbFrame, INTNETHDR_ALIGNMENT));
    memset(pHdr, 0xef, sizeof(*pHdr));
#endif
    ASMAtomicWriteU32(&pRingBuf->offReadX, offReadNew);
}


/**
 * Allocates a frame in the specified ring.
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pRingBuf            The ring buffer.
 * @param   cbFrame             The frame size.
 * @param   ppHdr               Where to return the frame header.
 *                              Don't touch this!
 * @param   ppvFrame            Where to return the frame pointer.
 */
DECLINLINE(int) intnetRingAllocateFrameInternal(PINTNETRINGBUF pRingBuf, uint32_t cbFrame, uint16_t u16Type,
                                                PINTNETHDR *ppHdr, void **ppvFrame)
{
    /*
     * Validate input and adjust the input.
     */
    INTNETRINGBUF_ASSERT_SANITY(pRingBuf);
    Assert(cbFrame >= sizeof(RTMAC) * 2);

    const uint32_t  cb          = RT_ALIGN_32(cbFrame, INTNETHDR_ALIGNMENT);
    uint32_t        offWriteInt = ASMAtomicUoReadU32(&pRingBuf->offWriteInt);
    uint32_t        offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    if (offRead <= offWriteInt)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWriteInt >= cb + sizeof(INTNETHDR))
        {
            uint32_t offNew = offWriteInt + cb + sizeof(INTNETHDR);
            if (offNew >= pRingBuf->offEnd)
                offNew = pRingBuf->offStart;
            if (RT_UNLIKELY(!ASMAtomicCmpXchgU32(&pRingBuf->offWriteInt, offNew, offWriteInt)))
                return VERR_WRONG_ORDER; /* race */
            Log2(("INTNETRingAllocateFrame: offWriteInt: %#x -> %#x (1) (offRead=%#x)\n", offWriteInt, offNew, offRead));

            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offWriteInt);
            pHdr->u16Type  = u16Type;
            pHdr->cbFrame  = (uint16_t)cbFrame; Assert(pHdr->cbFrame == cbFrame);
            pHdr->offFrame = sizeof(INTNETHDR);

            *ppHdr = pHdr;
            *ppvFrame = pHdr + 1;
            return VINF_SUCCESS;
        }
        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWriteInt >= sizeof(INTNETHDR), ("offEnd=%x offWriteInt=%x\n", pRingBuf->offEnd, offWriteInt));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            uint32_t offNew = pRingBuf->offStart + cb;
            if (RT_UNLIKELY(!ASMAtomicCmpXchgU32(&pRingBuf->offWriteInt, offNew, offWriteInt)))
                return VERR_WRONG_ORDER; /* race */
            Log2(("INTNETRingAllocateFrame: offWriteInt: %#x -> %#x (2) (offRead=%#x)\n", offWriteInt, offNew, offRead));

            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offWriteInt);
            pHdr->u16Type  = u16Type;
            pHdr->cbFrame  = (uint16_t)cbFrame; Assert(pHdr->cbFrame == cbFrame);
            pHdr->offFrame = pRingBuf->offStart - offWriteInt;

            *ppHdr = pHdr;
            *ppvFrame = (uint8_t *)pRingBuf + pRingBuf->offStart;
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWriteInt > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        uint32_t offNew = offWriteInt + cb + sizeof(INTNETHDR);
        if (RT_UNLIKELY(!ASMAtomicCmpXchgU32(&pRingBuf->offWriteInt, offNew, offWriteInt)))
            return VERR_WRONG_ORDER; /* race */
        Log2(("INTNETRingAllocateFrame: offWriteInt: %#x -> %#x (3) (offRead=%#x)\n", offWriteInt, offNew, offRead));

        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offWriteInt);
        pHdr->u16Type  = u16Type;
        pHdr->cbFrame  = (uint16_t)cbFrame; Assert(pHdr->cbFrame == cbFrame);
        pHdr->offFrame = sizeof(INTNETHDR);

        *ppHdr = pHdr;
        *ppvFrame = pHdr + 1;
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    STAM_REL_COUNTER_INC(&pRingBuf->cOverflows);
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Allocates a normal frame in the specified ring.
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pRingBuf            The ring buffer.
 * @param   cbFrame             The frame size.
 * @param   ppHdr               Where to return the frame header.
 *                              Don't touch this!
 * @param   ppvFrame            Where to return the frame pointer.
 */
DECLINLINE(int) INTNETRingAllocateFrame(PINTNETRINGBUF pRingBuf, uint32_t cbFrame, PINTNETHDR *ppHdr, void **ppvFrame)
{
    return intnetRingAllocateFrameInternal(pRingBuf, cbFrame, INTNETHDR_TYPE_FRAME, ppHdr, ppvFrame);
}


/**
 * Allocates a GSO frame in the specified ring.
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pRingBuf            The ring buffer.
 * @param   cbFrame             The frame size.
 * @param   pGso                Pointer to the GSO context.
 * @param   ppHdr               Where to return the frame header.
 *                              Don't touch this!
 * @param   ppvFrame            Where to return the frame pointer.
 */
DECLINLINE(int) INTNETRingAllocateGsoFrame(PINTNETRINGBUF pRingBuf, uint32_t cbFrame, PCPDMNETWORKGSO pGso,
                                           PINTNETHDR *ppHdr, void **ppvFrame)
{
    void *pvFrame;
    int rc = intnetRingAllocateFrameInternal(pRingBuf, cbFrame + sizeof(*pGso), INTNETHDR_TYPE_GSO, ppHdr, &pvFrame);
    if (RT_SUCCESS(rc))
    {
        PPDMNETWORKGSO pGsoCopy = (PPDMNETWORKGSO)pvFrame;
        *pGsoCopy = *pGso;
        *ppvFrame = pGsoCopy + 1;
    }
    return rc;
}


/**
 * Commits a frame.
 *
 * Make sure to commit the frames in the order they've been allocated!
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pRingBuf            The ring buffer.
 * @param   pHdr                The frame header returned by
 *                              INTNETRingAllocateFrame.
 */
DECLINLINE(void) INTNETRingCommitFrame(PINTNETRINGBUF pRingBuf, PINTNETHDR pHdr)
{
    /*
     * Validate input and commit order.
     */
    INTNETRINGBUF_ASSERT_SANITY(pRingBuf);
    INTNETHDR_ASSERT_SANITY(pHdr, pRingBuf);
    Assert(pRingBuf->offWriteCom == ((uintptr_t)pHdr - (uintptr_t)pRingBuf));

    /*
     * Figure out the offWriteCom for this packet and update the ring.
     */
    const uint32_t cbFrame     = pHdr->cbFrame;
    const uint32_t cb          = RT_ALIGN_32(cbFrame, INTNETHDR_ALIGNMENT);
    uint32_t       offWriteCom = (uint32_t)((uintptr_t)pHdr - (uintptr_t)pRingBuf)
                               + pHdr->offFrame
                               + cb;
    if (offWriteCom >= pRingBuf->offEnd)
    {
        Assert(offWriteCom == pRingBuf->offEnd);
        offWriteCom = pRingBuf->offStart;
    }
    Log2(("INTNETRingCommitFrame:   offWriteCom: %#x -> %#x (offRead=%#x)\n", pRingBuf->offWriteCom, offWriteCom, pRingBuf->offReadX));
    ASMAtomicWriteU32(&pRingBuf->offWriteCom, offWriteCom);
    STAM_REL_COUNTER_ADD(&pRingBuf->cbStatWritten, cbFrame);
    STAM_REL_COUNTER_INC(&pRingBuf->cStatFrames);
}


/**
 * Commits a frame and injects a filler frame if not all of the buffer was used.
 *
 * Make sure to commit the frames in the order they've been allocated!
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pRingBuf            The ring buffer.
 * @param   pHdr                The frame header returned by
 *                              INTNETRingAllocateFrame.
 * @param   cbUsed              The amount of space actually used.
 */
DECLINLINE(void) INTNETRingCommitFrameEx(PINTNETRINGBUF pRingBuf, PINTNETHDR pHdr, size_t cbUsed)
{
    /*
     * Validate input and commit order.
     */
    INTNETRINGBUF_ASSERT_SANITY(pRingBuf);
    INTNETHDR_ASSERT_SANITY(pHdr, pRingBuf);
    Assert(pRingBuf->offWriteCom == ((uintptr_t)pHdr - (uintptr_t)pRingBuf));

    /*
     * Calc the new write commit offset.
     */
    const uint32_t  cbAlignedFrame = RT_ALIGN_32(pHdr->cbFrame, INTNETHDR_ALIGNMENT);
    const uint32_t  cbAlignedUsed  = RT_ALIGN_32(cbUsed, INTNETHDR_ALIGNMENT);
    uint32_t        offWriteCom    = (uint32_t)((uintptr_t)pHdr - (uintptr_t)pRingBuf)
                                   + pHdr->offFrame
                                   + cbAlignedFrame;
    if (offWriteCom >= pRingBuf->offEnd)
    {
        Assert(offWriteCom == pRingBuf->offEnd);
        offWriteCom = pRingBuf->offStart;
    }

    /*
     * Insert a dummy frame to pad any unused space.
     */
    if (cbAlignedFrame != cbAlignedUsed)
    {
        /** @todo Later: Try unallocate the extra memory.  */
        PINTNETHDR pHdrPadding = (PINTNETHDR)((uint8_t *)pHdr + pHdr->offFrame + cbAlignedUsed);
        pHdrPadding->u16Type  = INTNETHDR_TYPE_PADDING;
        pHdrPadding->cbFrame  = (uint16_t)(cbAlignedFrame - cbAlignedUsed - sizeof(INTNETHDR));
        pHdrPadding->offFrame = sizeof(INTNETHDR);
        pHdr->cbFrame = (uint16_t)cbUsed;
    }

    Log2(("INTNETRingCommitFrame:   offWriteCom: %#x -> %#x (offRead=%#x)\n", pRingBuf->offWriteCom, offWriteCom, pRingBuf->offReadX));
    ASMAtomicWriteU32(&pRingBuf->offWriteCom, offWriteCom);
    STAM_REL_COUNTER_ADD(&pRingBuf->cbStatWritten, cbUsed);
    STAM_REL_COUNTER_INC(&pRingBuf->cStatFrames);
}


/**
 * Writes a frame to the specified ring.
 *
 * Make sure you don't have any uncommitted frames when calling this function!
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pRingBuf            The ring buffer.
 * @param   pvFrame             The bits to write.
 * @param   cbFrame             How much to write.
 */
DECLINLINE(int) INTNETRingWriteFrame(PINTNETRINGBUF pRingBuf, const void *pvFrame, size_t cbFrame)
{
    /*
     * Validate input.
     */
    INTNETRINGBUF_ASSERT_SANITY(pRingBuf);
    Assert(cbFrame >= sizeof(RTMAC) * 2);

    /*
     * Align the size and read the volatile ring buffer variables.
     */
    const uint32_t  cb          = RT_ALIGN_32(cbFrame, INTNETHDR_ALIGNMENT);
    uint32_t        offWriteInt = ASMAtomicUoReadU32(&pRingBuf->offWriteInt);
    uint32_t        offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    if (offRead <= offWriteInt)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWriteInt >= cb + sizeof(INTNETHDR))
        {
            uint32_t offNew = offWriteInt + cb + sizeof(INTNETHDR);
            if (offNew >= pRingBuf->offEnd)
                offNew = pRingBuf->offStart;
            if (RT_UNLIKELY(!ASMAtomicCmpXchgU32(&pRingBuf->offWriteInt, offNew, offWriteInt)))
                return VERR_WRONG_ORDER; /* race */
            Log2(("INTNETRingWriteFrame: offWriteInt: %#x -> %#x (1)\n", offWriteInt, offNew));

            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offWriteInt);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = (uint16_t)cbFrame; Assert(pHdr->cbFrame == cbFrame);
            pHdr->offFrame = sizeof(INTNETHDR);

            memcpy(pHdr + 1, pvFrame, cbFrame);

            Log2(("INTNETRingWriteFrame: offWriteCom: %#x -> %#x (1)\n", pRingBuf->offWriteCom, offNew));
            ASMAtomicWriteU32(&pRingBuf->offWriteCom, offNew);
            STAM_REL_COUNTER_ADD(&pRingBuf->cbStatWritten, cbFrame);
            STAM_REL_COUNTER_INC(&pRingBuf->cStatFrames);
            return VINF_SUCCESS;
        }
        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWriteInt >= sizeof(INTNETHDR), ("offEnd=%x offWriteInt=%x\n", pRingBuf->offEnd, offWriteInt));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            uint32_t offNew = pRingBuf->offStart + cb;
            if (RT_UNLIKELY(!ASMAtomicCmpXchgU32(&pRingBuf->offWriteInt, offNew, offWriteInt)))
                return VERR_WRONG_ORDER; /* race */
            Log2(("INTNETRingWriteFrame: offWriteInt: %#x -> %#x (2)\n", offWriteInt, offNew));

            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offWriteInt);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = (uint16_t)cbFrame; Assert(pHdr->cbFrame == cbFrame);
            pHdr->offFrame = pRingBuf->offStart - offWriteInt;

            memcpy((uint8_t *)pRingBuf + pRingBuf->offStart, pvFrame, cbFrame);

            Log2(("INTNETRingWriteFrame: offWriteCom: %#x -> %#x (2)\n", pRingBuf->offWriteCom, offNew));
            ASMAtomicWriteU32(&pRingBuf->offWriteCom, offNew);
            STAM_REL_COUNTER_ADD(&pRingBuf->cbStatWritten, cbFrame);
            STAM_REL_COUNTER_INC(&pRingBuf->cStatFrames);
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWriteInt > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        uint32_t offNew = offWriteInt + cb + sizeof(INTNETHDR);
        if (RT_UNLIKELY(!ASMAtomicCmpXchgU32(&pRingBuf->offWriteInt, offNew, offWriteInt)))
            return VERR_WRONG_ORDER; /* race */
        Log2(("INTNETRingWriteFrame: offWriteInt: %#x -> %#x (3)\n", offWriteInt, offNew));

        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offWriteInt);
        pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
        pHdr->cbFrame  = (uint16_t)cbFrame; Assert(pHdr->cbFrame == cbFrame);
        pHdr->offFrame = sizeof(INTNETHDR);

        memcpy(pHdr + 1, pvFrame, cbFrame);

        Log2(("INTNETRingWriteFrame: offWriteCom: %#x -> %#x (3)\n", pRingBuf->offWriteCom, offNew));
        ASMAtomicWriteU32(&pRingBuf->offWriteCom, offNew);
        STAM_REL_COUNTER_ADD(&pRingBuf->cbStatWritten, cbFrame);
        STAM_REL_COUNTER_INC(&pRingBuf->cStatFrames);
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    STAM_REL_COUNTER_INC(&pRingBuf->cOverflows);
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Reads the next frame in the buffer and moves the read cursor past it.
 *
 * @returns Size of the frame in bytes.  0 is returned if nothing in the buffer.
 * @param   pRingBuff   The ring buffer to read from.
 * @param   pvFrameDst  Where to put the frame.  The caller is responsible for
 *                      ensuring that there is sufficient space for the frame.
 *
 * @deprecated  Bad interface, do NOT use it!  Only for tstIntNetR0.
 */
DECLINLINE(uint32_t) INTNETRingReadAndSkipFrame(PINTNETRINGBUF pRingBuf, void *pvFrameDst)
{
    INTNETRINGBUF_ASSERT_SANITY(pRingBuf);

    uint32_t       offRead     = ASMAtomicUoReadU32(&pRingBuf->offReadX);
    uint32_t const offWriteCom = ASMAtomicUoReadU32(&pRingBuf->offWriteCom);
    if (offRead == offWriteCom)
        return 0;

    PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pRingBuf + offRead);
    INTNETHDR_ASSERT_SANITY(pHdr, pRingBuf);

    uint32_t const cbFrame     = pHdr->cbFrame;
    int32_t const  offFrame    = pHdr->offFrame;
    const void    *pvFrameSrc  = (uint8_t *)pHdr + offFrame;
    memcpy(pvFrameDst, pvFrameSrc, cbFrame);
#ifdef INTNET_POISON_READ_FRAMES
    memset((void *)pvFrameSrc, 0xfe, RT_ALIGN_32(cbFrame, INTNETHDR_ALIGNMENT));
    memset(pHdr, 0xef, sizeof(*pHdr));
#endif

    /* skip the frame */
    offRead += offFrame + cbFrame;
    offRead = RT_ALIGN_32(offRead, INTNETHDR_ALIGNMENT);
    Assert(offRead <= pRingBuf->offEnd && offRead >= pRingBuf->offStart);
    if (offRead >= pRingBuf->offEnd)
        offRead = pRingBuf->offStart;
    ASMAtomicWriteU32(&pRingBuf->offReadX, offRead);
    return cbFrame;
}


/**
 * Initializes a buffer structure.
 *
 * @param   pIntBuf             The internal networking interface buffer.  This
 *                              expected to be cleared prior to calling this
 *                              function.
 * @param   cbBuf               The size of the whole buffer.
 * @param   cbRecv              The receive size.
 * @param   cbSend              The send size.
 */
DECLINLINE(void) INTNETBufInit(PINTNETBUF pIntBuf, uint32_t cbBuf, uint32_t cbRecv, uint32_t cbSend)
{
    AssertCompileSizeAlignment(INTNETBUF, INTNETHDR_ALIGNMENT);
    AssertCompileSizeAlignment(INTNETBUF, INTNETRINGBUF_ALIGNMENT);
    Assert(cbBuf >= sizeof(INTNETBUF) + cbRecv + cbSend);
    Assert(RT_ALIGN_32(cbRecv, INTNETRINGBUF_ALIGNMENT) == cbRecv);
    Assert(RT_ALIGN_32(cbSend, INTNETRINGBUF_ALIGNMENT) == cbSend);
    Assert(ASMMemIsAll8(pIntBuf, cbBuf, '\0') == NULL);

    pIntBuf->u32Magic  = INTNETBUF_MAGIC;
    pIntBuf->cbBuf     = cbBuf;
    pIntBuf->cbRecv    = cbRecv;
    pIntBuf->cbSend    = cbSend;

    /* receive ring buffer. */
    uint32_t offBuf = RT_ALIGN_32(sizeof(INTNETBUF), INTNETRINGBUF_ALIGNMENT) - RT_OFFSETOF(INTNETBUF, Recv);
    pIntBuf->Recv.offStart      = offBuf;
    pIntBuf->Recv.offReadX      = offBuf;
    pIntBuf->Recv.offWriteInt   = offBuf;
    pIntBuf->Recv.offWriteCom   = offBuf;
    pIntBuf->Recv.offEnd        = offBuf + cbRecv;

    /* send ring buffer. */
    offBuf += cbRecv + RT_OFFSETOF(INTNETBUF, Recv) - RT_OFFSETOF(INTNETBUF, Send);
    pIntBuf->Send.offStart      = offBuf;
    pIntBuf->Send.offReadX      = offBuf;
    pIntBuf->Send.offWriteCom   = offBuf;
    pIntBuf->Send.offWriteInt   = offBuf;
    pIntBuf->Send.offEnd        = offBuf + cbSend;
    Assert(cbBuf >= offBuf + cbSend);
}

#endif /* __cplusplus */

#endif

