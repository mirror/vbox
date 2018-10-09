/* $Id$ */
/** @file
 * IPRT - Crypto - Alleged RC4 via OpenSSL.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
# include "internal/iprt-openssl.h"
# include "openssl/rc4.h"
# include <iprt/crypto/rc4.h>

# include <iprt/assert.h>
# include <iprt/assertcompile.h>



RTDECL(void) RTCrRc4SetKey(PRTCRRC4KEY pKey, size_t cbData, void const *pvData)
{

    AssertCompile(sizeof(RC4_INT) == 4 ? sizeof(pKey->au64Padding) == sizeof(pKey->Ossl) : sizeof(pKey->au64Padding) >= sizeof(pKey->Ossl));
    Assert((int)cbData == (ssize_t)cbData);
    AssertPtr(pKey);

    RC4_set_key(&pKey->Ossl, (int)cbData, (const unsigned char *)pvData);
}


RTDECL(void) RTCrRc4(PRTCRRC4KEY pKey, size_t cbData, void const *pvDataIn, void *pvDataOut)
{
    AssertCompile(sizeof(RC4_INT) == 4 ? sizeof(pKey->au64Padding) == sizeof(pKey->Ossl) : sizeof(pKey->au64Padding) >= sizeof(pKey->Ossl));
    Assert((int)cbData == (ssize_t)cbData);
    AssertPtr(pKey);

    RC4(&pKey->Ossl, (int)cbData, (const unsigned char *)pvDataIn, (unsigned char *)pvDataOut);
}

#endif /* IPRT_WITH_OPENSSL */

