/* $Id$ */
/** @file
 * IPRT - Crypto - X.509, Certificate Creation and Signing.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/

# if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
#  include <io.h>
# endif

#include <iprt/file.h>
#include "internal/iprt.h"
#include <iprt/crypto/x509.h>

# ifdef _MSC_VER
#  define IPRT_COMPILER_VCC_WITH_C_INIT_TERM_SECTIONS
#  include "internal/compiler-vcc.h"
# endif

#include <fcntl.h>
#include <iprt/err.h>
#include <iprt/string.h>

#ifdef IPRT_WITH_OPENSSL
# include <openssl/evp.h>
# include <openssl/pem.h>
# include <openssl/x509.h>
# include <openssl/bio.h>

RTDECL(int) RTCrX509Certificate_Generate(const char *pszServerCertificate, const char *pszServerPrivateKey)
{
    int rc = VINF_SUCCESS;
    /*
     * Set up private key using rsa
     */
    EVP_PKEY * pkey;
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L) /* OpenSSL 3 needed */
    pkey = EVP_RSA_gen(2048);
#else
    pkey = EVP_PKEY_new();
    RSA * rsa;
    rsa = RSA_generate_key(
        2048,   /* Number of bits for the key */
        RSA_F4, /* Exponent - RSA_F4 is defined as 0x10001L */
        NULL,   /* Callback */
        NULL    /* Callback argument */
    );
    EVP_PKEY_assign_RSA(pkey, rsa);
#endif

    if ( pkey == NULL )
        return VERR_CR_KEY_GEN_FAILED_RSA;

    /*
     * Set up certificate
     */
    X509* tempX509 = X509_new();
    if ( tempX509 == NULL )
        return VERR_CR_X509_GENERIC_ERROR;
    X509_set_version(tempX509,0); /** Set to X509 version 1 */
    ASN1_INTEGER_set(X509_get_serialNumber(tempX509), 1);
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
    X509_gmtime_adj(X509_getm_notBefore(tempX509), 0);
    X509_gmtime_adj(X509_getm_notAfter(tempX509), 60*60*24*3650); /** 10 years time */
#else
    X509_gmtime_adj(X509_get_notBefore(tempX509), 0);
    X509_gmtime_adj(X509_get_notAfter(tempX509), 60*60*24*3650); /** 10 years time */
#endif
    X509_set_pubkey(tempX509,pkey);

    X509_NAME *x509_name = NULL;
    x509_name = X509_get_subject_name(tempX509);

    rc = X509_set_issuer_name(tempX509, x509_name);
    if ( RT_FAILURE(rc) )
        return rc;

    rc = X509_sign( tempX509, pkey, EVP_sha1());
    if ( RT_FAILURE(rc) )
        return rc;

    RTFILE hKeyFile;
    rc = RTFileOpen(&hKeyFile, pszServerPrivateKey, RTFILE_O_WRITE | RTFILE_O_DENY_ALL | RTFILE_O_CREATE | (0600 << RTFILE_O_CREATE_MODE_SHIFT) );
    if ( RT_FAILURE(rc) )
        return rc;
# ifndef _MSC_VER
    int fd1 = (int)RTFileToNative(hKeyFile);
# else
    int fd1 = _open_osfhandle(RTFileToNative(hKeyFile), _O_WRONLY);
# endif
    if ( fd1 < 0 )
        return VERR_FILE_IO_ERROR;

    BIO *fp1 = BIO_new_fd(fd1, BIO_NOCLOSE);
    rc = PEM_write_bio_PrivateKey( fp1, pkey, NULL, NULL, 0, NULL, NULL);
    if ( RT_FAILURE(rc) )
        return rc;
    BIO_free(fp1);
# ifdef _MSC_VER
    close(fd1);
#endif
    RTFileClose(hKeyFile);

    RTFILE hCertFile;
    rc = RTFileOpen(&hCertFile, pszServerCertificate, RTFILE_O_WRITE | RTFILE_O_DENY_ALL | RTFILE_O_CREATE | (0600 << RTFILE_O_CREATE_MODE_SHIFT) );
    if ( RT_FAILURE(rc) )
        return rc;
# ifndef _MSC_VER
    int fd2 = (int)RTFileToNative(hCertFile);
# else
    int fd2 = _open_osfhandle(RTFileToNative(hCertFile), _O_WRONLY);
# endif
    if ( fd2 < 0 )
        return VERR_FILE_IO_ERROR;

    BIO *fp2 = BIO_new_fd(fd2, BIO_NOCLOSE);
    rc = PEM_write_bio_X509( fp2, tempX509 );
    if ( RT_FAILURE(rc) )
        return rc;
    BIO_free(fp2);
# ifdef _MSC_VER
    close(fd2);
#endif
    RTFileClose(hCertFile);

    X509_free(tempX509);
    EVP_PKEY_free(pkey);

    return rc;
}

#endif /* IPRT_WITH_OPENSSL */
