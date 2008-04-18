/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Random Numbers and Byte Streams, POSIX.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#ifdef _MSC_VER
# include <io.h>
# include <stdio.h>
#else
# include <unistd.h>
# include <sys/time.h>
#endif

#include <iprt/rand.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/rand.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** File handle of /dev/random. */
static int g_fhDevRandom = -1;


void rtRandLazyInitNative(void)
{
    if (g_fhDevRandom != -1)
        return;

    int fh = open("/dev/urandom", O_RDONLY);
    if (fh <= 0)
        fh = open("/dev/random", O_RDONLY | O_NONBLOCK);
    if (fh >= 0)
    {
        fcntl(fh, F_SETFD, FD_CLOEXEC);
        g_fhDevRandom = fh;
    }
}


int rtRandGenBytesNative(void *pv, size_t cb)
{
    int fh = g_fhDevRandom;
    if (fh == -1)
        return VERR_NOT_SUPPORTED;

    ssize_t cbRead = read(fh, pv, cb);
    if ((size_t)cbRead != cb)
    {
        /* 
         * Use the fallback for the remainder if /dev/urandom / /dev/random 
         * is out to lunch. 
         */
        if (cbRead <= 0)
            rtRandGenBytesFallback(pv, cb);
        else 
        {
            AssertRelease((size_t)cbRead < cb);
            rtRandGenBytesFallback((uint8_t *)pv + cbRead, cb - cbRead);
        }
    }
    return VINF_SUCCESS;
}

