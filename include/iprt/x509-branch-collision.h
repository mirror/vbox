/** @file
 * IPRT - X509 functions
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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

#ifndef ___iprt_x509_h
#define ___iprt_x509_h

#include <iprt/types.h>
#include <iprt/manifest.h>
#include <openssl/x509v3.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_x509   RTX509 - X509 Functions
 * @ingroup grp_rt
 * @{
 */

/**
 * Preparation before start to work with openssl
 *
 * @todo This should return a status and check that X509 code seems sane.  This
 *       would allow dynamic linking if necessary at some point.
 */
RTDECL(int) RTX509PrepareOpenSSL(void);

/**
 * Verify RSA signature for the given memory buffer.
 *
 * @returns iprt status code.
 *
 * @param   pvBuf                 Memory buffer containing a RSA
 *                                signature
 * @param   cbSize                The amount of data (in bytes)
 * @param   pManifestDigestIn     string contains manifest
 *                                digest
 * @param   digestType            Type of digest
 */
RTDECL(int) RTRSAVerify(void *pvBuf, unsigned int cbSize,  const char* pManifestDigestIn, RTDIGESTTYPE digestType);

/**
 * Verify X509 certificate for the given memory buffer.
 *
 * @returns iprt status code.
 *
 * @param   pvBuf                 Memory buffer containing X509
 *                                certificate
 * @param   cbSize                The amount of data (in bytes)
 */
RTDECL(int) RTX509CertificateVerify(void *pvBuf, unsigned int cbSize);

/** @todo document me. */
RTDECL(unsigned long) RTX509GetErrorDescription(char** pErrorDesc);

/** @} */

RT_C_DECLS_END

#endif /* ___iprt_x509_h */

