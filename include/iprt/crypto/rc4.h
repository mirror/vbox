/** @file
 * IPRT - Crypto - Alleged RC4 Cipher.
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

#ifndef ___iprt_crypto_rc4_h
#define ___iprt_crypto_rc4_h

#include <iprt/types.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_cr_rc4  RTCrRc4 - Alleged RC4 Cipher.
 * @ingroup grp_rt_crypto
 * @{
 */

/** RC4 key structure. */
typedef union RTCRRC4KEY
{
    uint64_t    au64Padding[(2 + 256) * sizeof(RC4_INT) / 8];
#ifdef HEADER_RC4_H
    RC4_KEY     Ossl;
#endif
} RTCRRC4KEY;
/** Pointer to a RC4 key structure. */
typedef RTCRRC4KEY *PRTCRRC4KEY;
/** Pointer to a const RC4 key structure. */
typedef RTCRRC4KEY const *PCRTCRRC4KEY;

RTDECL(void) RTCrRc4SetKey(PRTCRRC4KEY pKey, size_t cbData, void const *pvData);
RTDECL(void) RTCrRc4(PRTCRRC4KEY pKey, size_t cbData, void const *pvDataIn, void *pvDataOut);

/** @} */

RT_C_DECLS_END

#endif

