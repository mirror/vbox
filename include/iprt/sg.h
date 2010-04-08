/** @file
 * IPRT - S/G buffer handling.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef ___iprt_sg_h
#define ___iprt_sg_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/**
 * A S/G entry.
 */
typedef struct RTSGSEG
{
    /** Pointer to the segment buffer. */
    void   *pvSeg;
    /** Size of the segment buffer. */
    size_t  cbSeg;
} RTSGSEG;
/** Pointer to a S/G entry. */
typedef RTSGSEG *PRTSGSEG;
/** Pointer to a const S/G entry. */
typedef const RTSGSEG *PCRTSGSEG;
/** Pointer to a S/G entry pointer. */
typedef PRTSGSEG *PPRTSGSEG;

/**
 * A S/G buffer.
 *
 * The members should be treated as private.
 */
typedef struct RTSGBUF
{
    /** Pointer to the scatter/gather array. */
    PCRTSGSEG pcaSeg;
    /** Number of segments. */
    unsigned  cSeg;
    /** Current segment we are in. */
    unsigned  idxSeg;
    /** Pointer to the current segment start. */
    void     *pvSegCurr;
    /** Number of bytes left in the current buffer. */
    size_t    cbSegLeft;
} RTSGBUF;
/** Pointer to a S/G entry. */
typedef RTSGBUF *PRTSGBUF;
/** Pointer to a const S/G entry. */
typedef const RTSGBUF *PCRTSGBUF;
/** Pointer to a S/G entry pointer. */
typedef PRTSGBUF *PPRTSGBUF;

/**
 * Initialize a S/G buffer structure.
 *
 * @returns nothing.
 * @param   pSgBuf    Pointer to the S/G buffer to initialize.
 * @param   pcaSeg    Pointer to the start of the segment array.
 * @param   cSeg      Number of segments in the array.
 */
RTDECL(void) RTSgBufInit(PRTSGBUF pSgBuf, PCRTSGSEG pcaSeg, unsigned cSeg);

/**
 * Resets the internal buffer position of the S/G buffer to the beginning.
 *
 * @returns nothing.
 * @param   pSgBuf    The S/G buffer to reset.
 */
RTDECL(void) RTSgBufReset(PRTSGBUF pSgBuf);

/**
 * Clones a given S/G buffer.
 *
 * @returns nothing.
 * @param   pSgBufNew    The new S/G buffer to clone to.
 * @param   pSgBufOld    The source S/G buffer to clone from.
 *
 * @note This is only a shallow copy. Both S/G buffers will point to the
 *       same segment array.
 */
RTDECL(void) RTSgBufClone(PRTSGBUF pSgBufNew, PCRTSGBUF pSgBufOld);

/**
 * Copy data between two S/G buffers.
 *
 * @returns The number of bytes copied.
 * @param   pSgBufDst    The destination S/G buffer.
 * @param   pSgBufSrc    The source S/G buffer.
 * @param   cbCopy       Number of bytes to copy.
 *
 * @note This operation advances the internal buffer pointer of both S/G buffers.
 */
RTDECL(size_t) RTSgBufCopy(PRTSGBUF pSgBufDst, PRTSGBUF pSgBufSrc, size_t cbCopy);

/**
 * Compares the content of two S/G buffers.
 *
 * @returns Whatever memcmp returns.
 * @param   pSgBuf1      First S/G buffer.
 * @param   pSgBuf2      Second S/G buffer.
 * @param   cbCmp        How many bytes to compare.
 *
 * @note This operation doesn't change the internal position of the S/G buffers.
 */
RTDECL(int) RTSgBufCmp(PCRTSGBUF pSgBuf1, PCRTSGBUF pSgBuf2, size_t cbCmp);

/**
 * Fills an S/G buf with a constant byte.
 *
 * @returns The number of actually filled bytes.
 *          Can be less than than cbSet if the end of the S/G buffer was reached.
 * @param   pSgBuf       The S/G buffer.
 * @param   ubFill       The byte to fill the buffer with.
 * @param   cbSet        How many bytes to set.
 *
 * @note This operation advances the internal buffer pointer of the S/G buffer.
 */
RTDECL(size_t) RTSgBufSet(PRTSGBUF pSgBuf, uint8_t ubFill, size_t cbSet);

/**
 * Copies data from an S/G buffer into a given non scattered buffer.
 *
 * @returns Number of bytes copied.
 * @param   pSgBuf       The S/G buffer to copy from.
 * @param   pvBuf        Buffer to copy the data into.
 * @param   cbCopy       How many bytes to copy.
 *
 * @note This operation advances the internal buffer pointer of the S/G buffer.
 */
RTDECL(size_t) RTSgBufCopyToBuf(PRTSGBUF pSgBuf, void *pvBuf, size_t cbCopy);

/**
 * Copies data from a non scattered buffer into an S/G buffer.
 *
 * @returns Number of bytes copied.
 * @param   pSgBuf       The S/G buffer to copy to.
 * @param   pvBuf        Buffer to copy the data into.
 * @param   cbCopy       How many bytes to copy.
 *
 * @note This operation advances the internal buffer pointer of the S/G buffer.
 */
RTDECL(size_t) RTSgBufCopyFromBuf(PRTSGBUF pSgBuf, void *pvBuf, size_t cbCopy);

/**
 * Advances the internal buffer pointer.
 *
 * @returns Number of bytes the pointer was moved forward.
 * @param   pSgBuf      The S/G buffer.
 * @param   cbAdvance   Number of bytes to move forward.
 */
RTDECL(size_t) RTSgBufAdvance(PRTSGBUF pSgBuf, size_t cbAdvance);

/**
 * Constructs a new segment array starting from the current position
 * and describing the given number of bytes.
 *
 * @returns Number of bytes the array describes.
 * @param   pSgBuf      The S/G buffer.
 * @param   paSeg       The unitialized segment array.
 * @param   pcSeg       The number of segments the given array has.
 *                      This will hold the actual number of entries needed upon return.
 * @param   cbData      Number of bytes the new array should describe.
 *
 * @note This operation advances the internal buffer pointer of the S/G buffer.
 */
RTDECL(size_t) RTSgBufSegArrayCreate(PRTSGBUF pSgBuf, PRTSGSEG paSeg, unsigned *pcSeg, size_t cbData);

RT_C_DECLS_END

/** @} */

#endif

