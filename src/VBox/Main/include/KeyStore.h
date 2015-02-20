/* $Id$ */
/** @file
 * Main - Simple keystore handling for encrypted media.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___KeyStore_h___
#define ___KeyStore_h___

#include <iprt/cdefs.h>
#include <iprt/types.h>

/**
 * Return the encryption parameters and DEK from the base64 encoded key store data.
 *
 * @returns IPRT status code.
 * @param   pszEnc         The base64 encoded key store data.
 * @param   pszPassword    The password to use for key decryption.
 * @param   ppbKey         Where to store the DEK on success.
 *                         Must be freed with RTMemSaferFree().
 * @param   pcbKey         Where to store the DEK size in bytes on success.
 * @param   ppszCipher     Where to store the used cipher for the decrypted DEK.
 *                         Must be freed with RTStrFree().
 */
DECLHIDDEN(int) VBoxKeyStoreGetDekFromEncoded(const char *pszEnc, const char *pszPassword,
                                              uint8_t **ppbKey, size_t *pcbKey, char **ppszCipher);

/**
 * Stores the given DEK in a key store protected by the given password.
 *
 * @returns IPRT status code.
 * @param   pszPassword    The password to protect the DEK.
 * @param   pbKey          The DEK to protect.
 * @param   cbKey          Size of the DEK to protect.
 * @param   pszCipher      The cipher string associated with the DEK.
 * @param   ppszEnc        Where to store the base64 encoded key store data on success.
 *                         Must be freed with RTMemFree().
 */
DECLHIDDEN(int) VBoxKeyStoreCreate(const char *pszPassword, const uint8_t *pbKey, size_t cbKey,
                                   const char *pszCipher, char **ppszEnc);

#endif

