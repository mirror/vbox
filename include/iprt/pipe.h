/** @file
 * IPRT - Anonymous Pipes.
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

#ifndef ___iprt_pipe_h
#define ___iprt_pipe_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_pipe      RTPipe - Anonymous Pipes
 * @ingroup grp_rt
 * @{
 */

/**
 * Create an anonymous pipe.
 *
 * @returns IPRT status code.
 * @param   phPipeRead      Where to return the read end of the pipe.
 * @param   phPipeWrite     Where to return the write end of the pipe.
 * @param   fFlags          A combination of RTPIPE_C_XXX defines.
 */
RTDECL(int)  RTPipeCreate(PRTPIPE phPipeRead, PRTPIPE phPipeWrite, uint32_t fFlags);

/** @name RTPipeCreate flags.
 * @{ */
/** Mark the read end as inheritable. */
#define RTPIPE_C_INHERIT_READ       RT_BIT(0)
/** Mark the write end as inheritable. */
#define RTPIPE_C_INHERIT_WRITE      RT_BIT(1)
/** Mask of valid flags. */
#define RTPIPE_C_VALID_MASK         UINT32_C(0x00000003)
/** @} */

/**
 * Closes one end of a pipe created by RTPipeCreate.
 *
 * @returns IPRT status code.
 * @param   hPipe           The pipe end to close.
 */
RTDECL(int)  RTPipeClose(RTPIPE hPipe);

/**
 * Gets the native handle for an IPRT pipe handle.
 *
 * @returns The native handle.
 * @param   hPipe           The IPRT pipe handle.
 */
RTDECL(RTHCINTPTR) RTPipeToNative(RTPIPE hPipe);

/**
 * Read bytes from a pipe, non-blocking.
 *
 * @returns IPRT status code.
 * @retval  VERR_WRONG_ORDER if racing a call to RTPipeReadBlocking.
 * @retval  VERR_BROKEN_PIPE if the remote party has disconnected and we've read
 *          all the buffered data.
 * @retval  VINF_TRY_AGAIN if no data was available.  @a *pcbRead will be set to
 *          0.
 * @retval  VERR_ACCESS_DENIED if it's a write pipe.
 *
 * @param   hPipe           The IPRT pipe handle to read from.
 * @param   pvBuf           Where to put the bytes we read.
 * @param   cbToRead        How much to read.  Must be greater than 0.
 * @param   pcbRead         Where to return the number of bytes that has been
 *                          read (mandatory).  This is 0 if there is no more
 *                          bytes to read.
 * @sa      RTPipeReadBlocking.
 */
RTDECL(int) RTPipeRead(RTPIPE hPipe, void *pvBuf, size_t cbToRead, size_t *pcbRead);

/**
 * Read bytes from a pipe, blocking.
 *
 * @returns IPRT status code.
 * @retval  VERR_WRONG_ORDER if racing a call to RTPipeRead.
 * @retval  VERR_BROKEN_PIPE if the remote party has disconnected and we've read
 *          all the buffered data.
 * @retval  VERR_ACCESS_DENIED if it's a write pipe.
 *
 * @param   hPipe           The IPRT pipe handle to read from.
 * @param   pvBuf           Where to put the bytes we read.
 * @param   cbToRead        How much to read.
 * @param   pcbRead         Where to return the number of bytes that has been
 *                          read. Optional.
 */
RTDECL(int) RTPipeReadBlocking(RTPIPE hPipe, void *pvBuf, size_t cbToRead, size_t *pcbRead);

/**
 * Write bytes to a pipe, non-blocking.
 *
 * @returns IPRT status code.
 * @retval  VERR_WRONG_ORDER if racing a call to RTPipeWriteBlocking.
 * @retval  VERR_BROKEN_PIPE if the remote party has disconnected.  Does not
 *          trigger when @a cbToWrite is 0.
 * @retval  VINF_TRY_AGAIN if no data was written.  @a *pcbWritten will be set
 *          to 0.
 * @retval  VERR_ACCESS_DENIED if it's a read pipe.
 *
 * @param   hPipe           The IPRT pipe handle to write to.
 * @param   pvBuf           What to write.
 * @param   cbToWrite       How much to write.
 * @param   pcbWritten      How many bytes we wrote, mandatory.  The return can
 *                          be 0.
 */
RTDECL(int) RTPipeWrite(RTPIPE hPipe, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);

/**
 * Write bytes to a pipe, blocking.
 *
 * @returns IPRT status code.
 * @retval  VERR_WRONG_ORDER if racing a call to RTPipeWrite.
 * @retval  VERR_BROKEN_PIPE if the remote party has disconnected.  Does not
 *          trigger when @a cbToWrite is 0.
 * @retval  VERR_ACCESS_DENIED if it's a read pipe.
 *
 * @param   hPipe           The IPRT pipe handle to write to.
 * @param   pvBuf           What to write.
 * @param   cbToWrite       How much to write.
 * @param   pcbWritten      How many bytes we wrote, optional.  If NULL then all
 *                          bytes will be written.
 */
RTDECL(int) RTPipeWriteBlocking(RTPIPE hPipe, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);

/**
 * Flushes the buffers for the specified pipe and making sure the other party
 * reads them.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if not supported by the OS.
 * @retval  VERR_BROKEN_PIPE if the remote party has disconnected.
 * @retval  VERR_ACCESS_DENIED if it's a read pipe.
 *
 * @param   hPipe           The IPRT pipe handle to flush.
 */
RTDECL(int) RTPipeFlush(RTPIPE hPipe);

/**
 * Checks if the pipe is ready for reading or writing (depending on the pipe
 * end).
 *
 * @returns IPRT status code.
 * @retval  VERR_TIMEOUT if the timeout was reached before the pipe was ready
 *          for reading/writing.
 * @retval  VERR_NOT_SUPPORTED if not supported by the OS?
 *
 * @param   hPipe           The IPRT pipe handle to select on.
 * @param   cMillies        Number of milliseconds to wait.  Use
 *                          RT_INDEFINITE_WAIT to wait for ever.
 */
RTDECL(int) RTPipeSelectOne(RTPIPE hPipe, RTMSINTERVAL cMillies);

/** @} */

RT_C_DECLS_END

#endif

