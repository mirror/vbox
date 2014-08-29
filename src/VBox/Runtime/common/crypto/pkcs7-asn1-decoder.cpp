/* $Id$ */
/** @file
 * IPRT - Crypto - PKCS \#7, Decoder for ASN.1.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/crypto/pkcs7.h>

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/crypto/spc.h>

#include "pkcs7-internal.h"


/*
 * PKCS #7 ContentInfo
 */

static int rtCrPkcs7ContentInfo_DecodeExtra(PRTASN1CURSOR pCursor, uint32_t fFlags, PRTCRPKCS7CONTENTINFO pThis,
                                            const char *pszErrorTag)
{
    pThis->u.pCore = NULL;

    int             rc;
    RTASN1CURSOR    ContentCursor;
    if (RTAsn1ObjId_CompareWithString(&pThis->ContentType, RTCRPKCS7SIGNEDDATA_OID) == 0)
    {
        rc = RTAsn1MemAllocZ(&pThis->Content.EncapsulatedAllocation, (void **)&pThis->Content.pEncapsulated,
                             sizeof(*pThis->u.pSignedData));
        if (RT_SUCCESS(rc))
        {
            pThis->u.pCore = pThis->Content.pEncapsulated;
            rc = RTAsn1CursorInitSubFromCore(pCursor, &pThis->Content.Asn1Core, &ContentCursor, "Content");
            if (RT_SUCCESS(rc))
                rc = RTCrPkcs7SignedData_DecodeAsn1(&ContentCursor, 0, pThis->u.pSignedData, "SignedData");
        }
    }
    else if (RTAsn1ObjId_CompareWithString(&pThis->ContentType, RTCRSPCINDIRECTDATACONTENT_OID) == 0)
    {
        rc = RTAsn1MemAllocZ(&pThis->Content.EncapsulatedAllocation, (void **)&pThis->Content.pEncapsulated,
                             sizeof(*pThis->u.pIndirectDataContent));
        if (RT_SUCCESS(rc))
        {
            pThis->u.pCore = pThis->Content.pEncapsulated;
            rc = RTAsn1CursorInitSubFromCore(pCursor, &pThis->Content.Asn1Core, &ContentCursor, "Content");
            if (RT_SUCCESS(rc))
                rc = RTCrSpcIndirectDataContent_DecodeAsn1(&ContentCursor, 0, pThis->u.pIndirectDataContent, "IndirectDataContent");
        }
    }
    else
        return VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        rc = RTAsn1CursorCheckEnd(&ContentCursor);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    return rc;
}


/*
 * Generate the code.
 */
#include <iprt/asn1-generator-asn1-decoder.h>

