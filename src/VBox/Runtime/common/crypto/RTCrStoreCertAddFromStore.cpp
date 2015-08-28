/* $Id$ */
/** @file
 * IPRT - Cryptographic (Certificate) Store, RTCrStoreCertAddFromStore.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/crypto/store.h>

#include <iprt/assert.h>
#include <iprt/err.h>



RTDECL(int) RTCrStoreCertAddFromStore(RTCRSTORE hStore, uint32_t fFlags, RTCRSTORE hStoreSrc)
{
    /*
     * Validate input.
     */
    AssertReturn(!(fFlags & ~(RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR)), VERR_INVALID_FLAGS);

    /*
     * Enumerate all the certificates in the source store, adding them to the destination.
     */
    RTCRSTORECERTSEARCH Search;
    int rc = RTCrStoreCertFindAll(hStoreSrc, &Search);
    if (RT_SUCCESS(rc))
    {
        PCRTCRCERTCTX pCertCtx;
        while ((pCertCtx = RTCrStoreCertSearchNext(hStoreSrc, &Search)) != NULL)
        {
            int rc2 = RTCrStoreCertAddEncoded(hStore, pCertCtx->fFlags | (fFlags & RTCRCERTCTX_F_ADD_IF_NOT_FOUND),
                                              pCertCtx->pabEncoded, pCertCtx->cbEncoded, NULL);
            if (RT_FAILURE(rc2))
            {
                rc = rc2;
                if (!(fFlags & RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR))
                    break;
            }
            RTCrCertCtxRelease(pCertCtx);
        }

        int rc2 = RTCrStoreCertSearchDestroy(hStoreSrc, &Search); AssertRC(rc2);
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTCrStoreCertAddFromStore);

