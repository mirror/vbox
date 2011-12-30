/* $Id$ */
/** @file
 * IPRT - Anonymous Pipes, OS/2 Implementation.
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
#include <iprt/pipe.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>


RTDECL(int)  RTPipeCreate(PRTPIPE phPipeRead, PRTPIPE phPipeWrite, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int)  RTPipeClose(RTPIPE hPipe)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int)  RTPipeFromNative(PRTPIPE phPipe, RTHCINTPTR hNativePipe, uint32_t fFlags)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(RTHCINTPTR) RTPipeToNative(RTPIPE hPipe)
{
    return -1;
}


RTDECL(int) RTPipeRead(RTPIPE hPipe, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPipeReadBlocking(RTPIPE hPipe, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPipeWrite(RTPIPE hPipe, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPipeWriteBlocking(RTPIPE hPipe, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPipeFlush(RTPIPE hPipe)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPipeSelectOne(RTPIPE hPipe, RTMSINTERVAL cMillies)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPipeQueryReadable(RTPIPE hPipe, size_t *pcbReadable)
{
    return VERR_NOT_IMPLEMENTED;
}

