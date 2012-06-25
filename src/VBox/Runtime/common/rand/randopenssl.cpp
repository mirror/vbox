/* $Id$ */
/** @file
 * IPRT - Random Numbers and Byte Streams, OpenSSL.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <openssl/err.h>
#include <openssl/rand.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <internal/magics.h>
#include "internal/rand.h"



/** @copydoc RTRANDINT::pfnGetBytes */
static DECLCALLBACK(void) rtRandAdvOpensslGetBytes(PRTRANDINT pThis, uint8_t *pb, size_t cb)
{
    /** @todo proper error handling? */
    int rc = RAND_bytes(pb, (int)cb);
    if (RT_UNLIKELY(rc != 1))
        rc = RAND_pseudo_bytes(pb, (int)cb);
    AssertReleaseMsg(rc == 1, ("RAND_bytes returned %d (error %d)\n", rc, ERR_get_error()));
}


/** @copydoc RTRANDINT::pfnDestroy */
static DECLCALLBACK(int) rtRandAdvOpensslDestroy(PRTRANDINT pThis)
{
    return VINF_SUCCESS;
}


static int rtRandAdvOpensslCreate(PRTRAND phRand) RT_NO_THROW
{
    PRTRANDINT pThis = (PRTRANDINT)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic     = RTRANDINT_MAGIC;
        pThis->pfnGetBytes  = rtRandAdvOpensslGetBytes;
        pThis->pfnGetU32    = rtRandAdvSynthesizeU32FromBytes;
        pThis->pfnGetU64    = rtRandAdvSynthesizeU64FromBytes;
        pThis->pfnSeed      = rtRandAdvStubSeed;
        pThis->pfnSaveState = rtRandAdvStubSaveState;
        pThis->pfnRestoreState = rtRandAdvStubRestoreState;
        pThis->pfnDestroy   = rtRandAdvOpensslDestroy;

        *phRand = pThis;
        return VINF_SUCCESS;
    }

    /* bail out */
    return VERR_NO_MEMORY;
}


RTDECL(int) RTRandAdvCreateOpenssl(PRTRAND phRand) RT_NO_THROW
{
    return rtRandAdvOpensslCreate(phRand);
}
