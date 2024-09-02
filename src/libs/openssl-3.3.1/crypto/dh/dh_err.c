/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/err.h>
#include <openssl/dherr.h>
#include "crypto/dherr.h"

#ifndef OPENSSL_NO_DH

# ifndef OPENSSL_NO_ERR

static const ERR_STRING_DATA DH_str_reasons[] = {
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_BAD_FFC_PARAMETERS), "bad ffc parameters"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_BAD_GENERATOR), "bad generator"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_BN_DECODE_ERROR), "bn decode error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_BN_ERROR), "bn error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_INVALID_J_VALUE),
    "check invalid j value"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_INVALID_Q_VALUE),
    "check invalid q value"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_PUBKEY_INVALID),
    "check pubkey invalid"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_PUBKEY_TOO_LARGE),
    "check pubkey too large"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_PUBKEY_TOO_SMALL),
    "check pubkey too small"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_P_NOT_PRIME), "check p not prime"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_P_NOT_SAFE_PRIME),
    "check p not safe prime"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_CHECK_Q_NOT_PRIME), "check q not prime"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_DECODE_ERROR), "decode error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_INVALID_PARAMETER_NAME),
    "invalid parameter name"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_INVALID_PARAMETER_NID),
    "invalid parameter nid"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_INVALID_PUBKEY), "invalid public key"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_INVALID_SECRET), "invalid secret"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_INVALID_SIZE), "invalid size"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_KDF_PARAMETER_ERROR), "kdf parameter error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_KEYS_NOT_SET), "keys not set"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_MISSING_PUBKEY), "missing pubkey"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_MODULUS_TOO_LARGE), "modulus too large"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_MODULUS_TOO_SMALL), "modulus too small"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_NOT_SUITABLE_GENERATOR),
    "not suitable generator"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_NO_PARAMETERS_SET), "no parameters set"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_NO_PRIVATE_VALUE), "no private value"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_PARAMETER_ENCODING_ERROR),
    "parameter encoding error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_PEER_KEY_ERROR), "peer key error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_Q_TOO_LARGE), "q too large"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_SHARED_INFO_ERROR), "shared info error"},
    {ERR_PACK(ERR_LIB_DH, 0, DH_R_UNABLE_TO_CHECK_GENERATOR),
    "unable to check generator"},
    {0, NULL}
};

# endif

int ossl_err_load_DH_strings(void)
{
# ifndef OPENSSL_NO_ERR
    if (ERR_reason_error_string(DH_str_reasons[0].error) == NULL)
        ERR_load_strings_const(DH_str_reasons);
# endif
    return 1;
}
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
