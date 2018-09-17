/* $Id$ */
/** @file
 * IPRT - Crypto - RTCrRandBytes implementation using OpenSSL.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt.h"
# include <iprt/crypto/misc.h>

# include <iprt/rand.h>
# include <iprt/assert.h>

# include "internal/iprt-openssl.h"
# include "openssl/rand.h"


RTDECL(int) RTCrRandBytes(void *pvDst, size_t cbDst)
{
    /* Make sure the return buffer is always fully initialized in case the caller
       doesn't properly check the return value. */
    RTRandBytes(pvDst, cbDst); /* */

    /* Get cryptographically strong random. */
    rtCrOpenSslInit();
    Assert((size_t)(int)cbDst == cbDst);
    int rcOpenSsl = RAND_bytes((uint8_t *)pvDst, (int)cbDst);
    return rcOpenSsl > 0 ? VINF_SUCCESS : rcOpenSsl == 0 ? VERR_CR_RANDOM_FAILED : VERR_CR_RANDOM_SETUP_FAILED;
}

#endif /* IPRT_WITH_OPENSSL */

