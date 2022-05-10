/* $Id$ */
/** @file
 * tstVBoxCrypto - Testcase for the cryptographic support module.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/VBoxCryptoIf.h>
#include <VBox/err.h>

#include <iprt/test.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/memsafer.h>
#include <iprt/string.h>
#include <iprt/vfs.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;
static const uint8_t g_abDek[64]          = { 0x42 };
static const char    g_szPassword[]       = "testtesttest";
static const char    g_szPasswordWrong[]  = "testtest";

static const char *g_aCiphers[] =
{
    "AES-XTS128-PLAIN64",
    "AES-GCM128",
    "AES-CTR128",

    "AES-XTS256-PLAIN64",
    "AES-GCM256",
    "AES-CTR256"
};

#define CHECK_STR(str1, str2)  do { if (strcmp(str1, str2)) { RTTestIFailed("line %u: '%s' != '%s' (*)", __LINE__, str1, str2); } } while (0)
#define CHECK_BYTES(bytes1, bytes2, size)  do { if (memcmp(bytes1, bytes2, size)) { RTTestIFailed("line %u: '%s' != '%s' (*)", __LINE__, #bytes1, bytes2); } } while (0)


/**
 * Testing some basics of the crypto keystore code.
 *
 * @returns nothing.
 * @param   pCryptoIf           Pointer to the callback table.
 */
static void tstCryptoKeyStoreBasics(PCVBOXCRYPTOIF pCryptoIf)
{
    RTTestISub("Crypto Keystore - Basics");

    RTTestDisableAssertions(g_hTest);

    for (uint32_t i = 0; i < RT_ELEMENTS(g_aCiphers); i++)
    {
        RTTestISubF("Creating a new keystore for cipher '%s'", g_aCiphers[i]);

        char *pszKeystoreEnc = NULL; /**< The encoded keystore. */
        int rc = pCryptoIf->pfnCryptoKeyStoreCreate(g_szPassword, &g_abDek[0], sizeof(g_abDek),
                                                    g_aCiphers[i], &pszKeystoreEnc);
        if (RT_SUCCESS(rc))
        {
            uint8_t *pbKey = NULL;
            size_t cbKey = 0;
            char *pszCipher = NULL;

            RTTestSub(g_hTest, "Trying to unlock DEK with wrong password");
            rc = pCryptoIf->pfnCryptoKeyStoreGetDekFromEncoded(pszKeystoreEnc, g_szPasswordWrong,
                                                               &pbKey, &cbKey, &pszCipher);
            RTTESTI_CHECK_RC(rc, VERR_VD_PASSWORD_INCORRECT);

            RTTestSub(g_hTest, "Trying to unlock DEK with correct password");
            rc = pCryptoIf->pfnCryptoKeyStoreGetDekFromEncoded(pszKeystoreEnc, g_szPassword,
                                                               &pbKey, &cbKey, &pszCipher);
            RTTESTI_CHECK_RC_OK(rc);
            if (RT_SUCCESS(rc))
            {
                RTTESTI_CHECK(cbKey == sizeof(g_abDek));
                CHECK_STR(pszCipher, g_aCiphers[i]);
                CHECK_BYTES(pbKey, &g_abDek[0], sizeof(g_abDek));

                RTMemSaferFree(pbKey, cbKey);
            }

            RTMemFree(pszKeystoreEnc);
        }
        else
            RTTestIFailed("Creating a new keystore failed with %Rrc", rc);
    }

    RTTestRestoreAssertions(g_hTest);
}


int main(int argc, char *argv[])
{
    /*
     * Initialization.
     */
    RTEXITCODE rcExit = RTTestInitAndCreate("tstVBoxCrypto", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    RTTestSub(g_hTest, "Loading the cryptographic support module");
    const char *pszModCrypto = NULL;
    if (argc == 2)
    {
        /* The module to load is given on the command line. */
        pszModCrypto = argv[1];
    }
    else
    {
        /* Try find it in the extension pack. */
        /** @todo */
    }

    RTLDRMOD hLdrModCrypto = NIL_RTLDRMOD;
    int rc = RTLdrLoad(pszModCrypto, &hLdrModCrypto);
    if (RT_SUCCESS(rc))
    {
        PFNVBOXCRYPTOENTRY pfnCryptoEntry = NULL;
        rc = RTLdrGetSymbol(hLdrModCrypto, VBOX_CRYPTO_MOD_ENTRY_POINT, (void **)&pfnCryptoEntry);
        if (RT_SUCCESS(rc))
        {
            PCVBOXCRYPTOIF pCryptoIf = NULL;
            rc = pfnCryptoEntry(&pCryptoIf);
            if (RT_SUCCESS(rc))
            {
                /* Loading succeeded, now we can start real testing. */
                tstCryptoKeyStoreBasics(pCryptoIf);
            }
            else
                RTTestIFailed("Calling '%s' failed with %Rrc", VBOX_CRYPTO_MOD_ENTRY_POINT, rc);
        }
        else
            RTTestIFailed("Failed to resolve entry point '%s' with %Rrc", VBOX_CRYPTO_MOD_ENTRY_POINT, rc);
    }
    else
        RTTestIFailed("Failed to load the crypto module '%s' with %Rrc", pszModCrypto, rc);
    return RTTestSummaryAndDestroy(g_hTest);
}
