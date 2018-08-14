/* $Id$ */
/** @file
 * IPRT testcase - Crypto - Public-Key Infrastructure \#1.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/crypto/pkix.h>

#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/crypto/key.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;

/**
 * Key pairs to use when testing.
 */
static const struct { unsigned cBits; const char *pszPrivateKey, *pszPublicKey; } g_aKeyPairs[] =
{
    {
        4096,
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "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\n"
        "-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\n"
        "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA1SOurMTVz033GGi+5VrM\n"
        "b0SnU7Dj49ZQCKSuxaSFK4tvbZXQBRSgwC1PcQVyt3GdoC71i3O4f9TxaA870icC\n"
        "IY7cqf4LKL9uB5Vga2SNMfx3+KqcJVt9LFsghXfLocdfV1k+xeDVGcSP7uUvnXoI\n"
        "ZyeS8puqoRYNiua1UT+ddXwihTId+6O9Q8IxcCPWkqW89LYBQVFqqMYoWzNcbEct\n"
        "Y6WpPzZk3er+AvMekBD409LbtT7jTrzIGd6eQ0aF2MyVA6lOwe3u99Ubo/FTpule\n"
        "/FQ5LXaEmlHPfDbIw+LRArdYgjoQU9l4SFajm0VbIKd2LFn5SRXHTbtAoKX2zpao\n"
        "i8GF3u8VR/EmmTPYFHr2gUoLuyeTaY56OG/5ns7N/NRzOX1d1lNRFcQYNCXPEtqa\n"
        "UfUfMJU4Jqp1LOEcd1xMkOUh8lc7DyvUfhry+SAcxB5SxcyjdWEXpj4G12/N3f6v\n"
        "sRoZNTFt5j0hsbiOAOFykgN0a2OF77bsd975e1mxkqXJ9A0sbB8EXsD2PSrUZ7Pt\n"
        "+T9CiQGOjqVUg2Vr1jevcQRHe5ed/R+B2jp6MjYjbr7cKqcXaRxEprGl+U5kIygq\n"
        "l93DTgQaXwX/ZjXmwjXvQ0W4OxxexqyW6YvDBYeNKxstuM5qfgzYf7FD/8lZYkyM\n"
        "AXELgpCqC92xlTbWpRVNpXcCAwEAAQ==\n"
        "-----END PUBLIC KEY-----\n"
    },
    {
        2048,
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEogIBAAKCAQEA06LAmfLBnRldEQF6E9CcMisCiaaDco0fYJvu60jkSBiA29k2\n"
        "Ru7LzTF6ctNXkC25P4RC25RjOYJbC0iS5YIR7VYFP6R505zDWs8vONeFchdQpfau\n"
        "TVjpgipIFovNGEUOGgXKD60n8txceuSygA3fg80movXmI7O+QLyrUkvFx2onDdVM\n"
        "Vlt8uhBwv8h62mJArienFDbNyQcmj47Y5pxkBRrcA8qnti+I3I3yA3kslq2O0QtN\n"
        "LHA7ttFYjieCcVv7pm/5g4kI2XyPv56RSem/RNsEv/qoK+g/h+b2C0sVO7eUyM6n\n"
        "x9VT8w+ODunnYWs1HiAGAhzj7NhsnJp0gm88KwIDAQABAoIBAEvePnlx4yK0Yv6j\n"
        "ruXHlRcPABvki57XJHZ3sBC80sldr2Qg3CpVlM38fM8JIIzZN12jxmv9KA0HxCep\n"
        "Xq/UDyUr/zmvdtT7j7TQLTeNW5No9EpqwlWMGDnHeoxKlb2rk8CUbrlr87RGdwi/\n"
        "T5ZEYupW8xDcYiJOX1fJywj3jPFNX70Iofirz+twKJuq/pT/It1b3VKVBZb5qSW/\n"
        "kfMMnJ1kELEAk7ue1sXm5QzF0/CizHNalEGJjuKauH21iCy1BGuJ00F31iploB4f\n"
        "lqzXpNbDGyFWfQo6bZwduyrdgBe2dFt4mg5htknJPo4oSl+oLi4HewhwO3jpt06z\n"
        "KRoT8XECgYEA7vVX6QwGbfnK/+CePiTBrD3FOgzfDagn5jSrvH0Km/YDVIa/6T7k\n"
        "9M2qw5MP7D9gWPDkS7L8hL/YxCSP0mYf4ABp89/n++V6ON7tEjyA3SixXpCqLYUd\n"
        "nSYl/ygJblEujFvhVtZaKyGpTMQXyJpCbV3ZdAar8Mg2p36MusitsscCgYEA4rqU\n"
        "oTurBhXwGYzFT92OA44aFpJgh/fo532NOpayPA/eeY0cea+N2TLZYtUmUWDAaslu\n"
        "3GG+VCHzYZCwRW5QTDJjZUB7VM0tONQDXPa4TLdI0GSDxnX7QXwyE6tk7JMTJ6fH\n"
        "ZuC/Kt84ngFerZCgr5/JSy2jVfBs2sv0fdjoh30CgYBKvwvkphJMzFoneAeHwM+k\n"
        "JR5Qbj5Hc1YnuEoQB70N1AJuqkfVmgrcWIkV7CaK67gjmhaPZ0l97NTNZfJnCfLm\n"
        "irqZwmw6aym0KGdX0P0uMNBqmC3jV0RQJ+Ky0b9BdrtsxEDUfPBvlXPzw1L9OOOW\n"
        "ekjO9ldKVhZihj9XHfbXeQKBgCh/XzD1cXTi0kIeDNhZIJat+Sby+l8O/wDqQiGm\n"
        "7SshQoG/nMh3fQTAumeW3wNGHth0JmMi6lYowko5B+M+8wTJM0vQmrbo9xzhccBX\n"
        "KVA6pLzkV01JoZluz5sH0D0ZgCBjLZDIsBy+RmSipgCmhq0YA2J0QmqFSUxDheY8\n"
        "qjwZAoGANbzLzEI9wjg7ZgRPqaIfoYjTimJMAeyesXKZMJG5BxoZRyPLa3ytbzRD\n"
        "B3Gf0oOYYI0QEEa1kLv7h1OUCjVRJnKcwsSIU9D1PDZI5WSP4dyoTUqZ/x7KbOZ5\n"
        "9Ze5jxhl4B1Kr+WvZ3VBWbBBCuX8bJzOvh+C8216TWhESaz85+0=\n"
        "-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA06LAmfLBnRldEQF6E9Cc\n"
        "MisCiaaDco0fYJvu60jkSBiA29k2Ru7LzTF6ctNXkC25P4RC25RjOYJbC0iS5YIR\n"
        "7VYFP6R505zDWs8vONeFchdQpfauTVjpgipIFovNGEUOGgXKD60n8txceuSygA3f\n"
        "g80movXmI7O+QLyrUkvFx2onDdVMVlt8uhBwv8h62mJArienFDbNyQcmj47Y5pxk\n"
        "BRrcA8qnti+I3I3yA3kslq2O0QtNLHA7ttFYjieCcVv7pm/5g4kI2XyPv56RSem/\n"
        "RNsEv/qoK+g/h+b2C0sVO7eUyM6nx9VT8w+ODunnYWs1HiAGAhzj7NhsnJp0gm88\n"
        "KwIDAQAB\n"
        "-----END PUBLIC KEY-----\n"
    },
    {
        1024,
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIICXAIBAAKBgQC2wFEkDX17SxuhH4jrSl/+lSEEXI2YGzXbDnsroXMjAa6pGj9f\n"
        "7+VOGvnBTJnT2FubDSvpaXMIEO0PTjMpS2fKKdn1jljAj3vfF9HpyyKOBgLwY1Pl\n"
        "fwj3bNPUomGZ+sgigNYWJ4+lXlSxJ7UlTQuQd7PiRsgCEIRny+5thH/rSwIDAQAB\n"
        "AoGAEzUTUh642YSDWuPdmB0xCajS14qCt0Hk3ykeeO93Em7S1KMVlhe4mgTryw0p\n"
        "/cH3nsw7mUSj+m0M/VbSubxbJA7VMVoaM3gnnHAttQVrGHxKMfA2Yupp0gLB9SFa\n"
        "W0oLO2NNz9IElQfPYWsir2VSqMbgil9srHxNMRMjcTv0O4ECQQDe8vstmZ3b2q5u\n"
        "L+Fd5pGF+rK919Bh59Nuvv3xPsJVoVjcfRJKGLKVMe+AK9YicM2jqqgV9UQ7gSZK\n"
        "z5jxS1YDAkEA0dfOsmFFGrAu4vAJf/YxJm/G7DyinM4Ffq1fVxCIZGOJxU5+EtH3\n"
        "YTRA0U6kM77O9i4Ms2LM9agSz76hdPjXGQJARVxowo4JK44EOGmS/qit23XcR+2t\n"
        "edgq0kh/Lp+szAEvaSFMIFtAq+PmNATvULWxdFqygmpUuQJ8DEg7t84NSwJAfMS7\n"
        "UpbBVvAAwNCGZX5FlRwLA/W9nkxlOf/t2z+qST5h8V4NWjVbyIEgNRN0UIwYVInm\n"
        "5VZOlZX8sWcgawN2KQJBAMvkCsY6sVjlK2FXA9f3FVHs6DT4g2TRLvCkwZAjbibY\n"
        "qy2W1RrPdtPOKXfr251hAlimxwcGXwTsRm07qirlQjE=\n"
        "-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\n"
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC2wFEkDX17SxuhH4jrSl/+lSEE\n"
        "XI2YGzXbDnsroXMjAa6pGj9f7+VOGvnBTJnT2FubDSvpaXMIEO0PTjMpS2fKKdn1\n"
        "jljAj3vfF9HpyyKOBgLwY1Plfwj3bNPUomGZ+sgigNYWJ4+lXlSxJ7UlTQuQd7Pi\n"
        "RsgCEIRny+5thH/rSwIDAQAB\n"
        "-----END PUBLIC KEY-----\n"
    },
    {
        512,
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIBOwIBAAJBAMgbhgcN8LxMNpEZgOC3hgI61pAwSxn4X8rSBHyTt7pfqbU0g2Tk\n"
        "PsNT7J6YS2xN+MwKiYNDeCTjRRbt67o1ZscCAwEAAQJBAKyXOKEq/+CYZ1P8yDCJ\n"
        "eZbAwsD4Nj4+//gB7ga4rXWbeDbkEFtLsN7wHIl1RQobfddStC5edTTbVJMk/NmX\n"
        "ESkCIQDpouOkB/cJvxfqeHqXuk4IS2s/hESEjX8dxFPsa3iNVQIhANtDCGPHhSvf\n"
        "za9hH/Wqxzbf2IrAPn/aJVNmphSi6wOrAiBj77IR2vpXp+7R86D0v9NbBu+kJq6s\n"
        "SF4kXHNNgJb7VQIhAKfuFTTmkRZjWNNj3eh4Hg/nLaBHURb26vOPgM/5X2n1AiAo\n"
        "b9m3zOpoO/0MAGCQ6qDHeebjvd65LSKgsmuDOSiOLw==\n"
        "-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\n"
        "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAMgbhgcN8LxMNpEZgOC3hgI61pAwSxn4\n"
        "X8rSBHyTt7pfqbU0g2TkPsNT7J6YS2xN+MwKiYNDeCTjRRbt67o1ZscCAwEAAQ==\n"
        "-----END PUBLIC KEY-----\n"
    }
};




static void test1()
{
    RTTestSub(g_hTest, "Basics");
    int rc;
    RTCRKEY hPublicKey  = NIL_RTCRKEY;
    RTCRKEY hPrivateKey = NIL_RTCRKEY;

    for (unsigned i = 0; i < RT_ELEMENTS(g_aKeyPairs); i++)
    {
        RTCrKeyRelease(hPublicKey);
        hPublicKey = NIL_RTCRKEY;
        RTCrKeyRelease(hPrivateKey);
        hPrivateKey = NIL_RTCRKEY;

        /*
         * Load the key pair.
         */
        rc = RTCrKeyCreateFromBuffer(&hPublicKey, 0, g_aKeyPairs[i].pszPublicKey, strlen(g_aKeyPairs[i].pszPublicKey),
                                     NULL /*pErrInfo*/, NULL /*pszErrorTag*/);
        if (RT_FAILURE(rc))
            RTTestIFailed("Error %Rrc decoding public key #%u (%u bits)", rc, i, g_aKeyPairs[i].cBits);

        rc = RTCrKeyCreateFromBuffer(&hPrivateKey, 0, g_aKeyPairs[i].pszPrivateKey, strlen(g_aKeyPairs[i].pszPrivateKey),
                                     NULL /*pErrInfo*/, NULL /*pszErrorTag*/);
        if (RT_FAILURE(rc))
            RTTestIFailed("Error %Rrc decoding private key #%u (%u bits)", rc, i, g_aKeyPairs[i].cBits);

        if (hPrivateKey == NIL_RTCRKEY || hPublicKey == NIL_RTCRKEY)
            continue;

#if 0
        /*
         * Decode.
         */
        /* Raw decoding of DER bytes, structure will have pointers to the raw data. */
        RTASN1CURSORPRIMARY PrimaryCursor;
        RTAsn1CursorInitPrimary(&PrimaryCursor, g_aFiles[i].pbDer, (uint32_t)g_aFiles[i].cbDer,
                                NULL /*pErrInfo*/, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, NULL /*pszErrorTag*/);
        rc = RTCrRsaPublicKey_DecodeAsn1(&PrimaryCursor.Cursor, 0, &Cert0, "Cert0");
        if (RT_SUCCESS(rc))
        {
            rc = RTCrX509Certificate_CheckSanity(&Cert0, 0, NULL /*pErrInfo*/, "Cert0");
            if (RT_SUCCESS(rc))
            {
                /* Check the API, this clones the certificate so no data pointers. */
                RTCRX509CERTIFICATE Cert1;
                memset(&Cert1, i, sizeof(Cert1));
                rc = RTCrX509Certificate_ReadFromBuffer(&Cert1, g_aFiles[i].pbDer, g_aFiles[i].cbDer, 0 /*fFlags*/,
                                                        &g_RTAsn1EFenceAllocator, NULL /*pErrInfo*/, NULL /*pszErrorTag*/);
                if (RT_SUCCESS(rc))
                {
                    /* Read the PEM variant. */
                    RTCRX509CERTIFICATE Cert2;
                    memset(&Cert2, ~i, sizeof(Cert2));
                    rc = RTCrX509Certificate_ReadFromBuffer(&Cert2, g_aFiles[i].pchPem, g_aFiles[i].cbPem, 0 /*fFlags*/,
                                                            &g_RTAsn1DefaultAllocator, NULL /*pErrInfo*/, NULL /*pszErrorTag*/);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Compare them, they should be all the same.
                         */
                        if (RTCrX509Certificate_Compare(&Cert0, &Cert1) != 0)
                            RTTestIFailed("Cert0 and Cert1 (DER) decoding of file %s (#%u) differs", g_aFiles[i].pszFile, i);
                        else if (RTCrX509Certificate_Compare(&Cert0, &Cert2) != 0)
                            RTTestIFailed("Cert0 and Cert2 (PEM) decoding of file %s (#%u) differs", g_aFiles[i].pszFile, i);
                        else if (RTCrX509Certificate_Compare(&Cert1, &Cert2) != 0)
                            RTTestIFailed("Cert1 (DER) and Cert2 (PEM) decoding of file %s (#%u) differs", g_aFiles[i].pszFile, i);
                        else
                        {
                            /*
                             * Encode the certificates.
                             */
                            unsigned j;
                            PRTCRX509CERTIFICATE paCerts[] = { &Cert0, &Cert1, &Cert2 };
                            for (j = 0; j < RT_ELEMENTS(paCerts); j++)
                            {
                                uint32_t cbEncoded = ~(j ^ i);
                                RTTESTI_CHECK_RC(rc = RTAsn1EncodePrepare(&paCerts[j]->SeqCore.Asn1Core,
                                                                          RTASN1ENCODE_F_DER, &cbEncoded, NULL), VINF_SUCCESS);
                                if (RT_SUCCESS(rc) && cbEncoded != g_aFiles[i].cbDer)
                                    RTTestIFailed("RTAsn1EncodePrepare of file %s (#%u) returned %#x bytes instead of %#x",
                                                  g_aFiles[i].pszFile, i, cbEncoded, g_aFiles[i].cbDer);

                                cbEncoded = (uint32_t)g_aFiles[i].cbDer;
                                void *pvTmp = RTTestGuardedAllocTail(g_hTest, cbEncoded);
                                RTTESTI_CHECK_RC(rc = RTAsn1EncodeToBuffer(&paCerts[j]->SeqCore.Asn1Core, RTASN1ENCODE_F_DER,
                                                                           pvTmp, cbEncoded, NULL /*pErrInfo*/), VINF_SUCCESS);
                                if (RT_SUCCESS(rc) && memcmp(pvTmp, g_aFiles[i].pbDer, cbEncoded) != 0)
                                    RTTestIFailed("RTAsn1EncodeToBuffer produces the wrong output for file %s (#%u), variation %u",
                                                  g_aFiles[i].pszFile, i, j);
                                RTTestGuardedFree(g_hTest, pvTmp);
                            }

                            /*
                             * Check that our self signed check works.
                             */
                            RTTESTI_CHECK(RTCrX509Certificate_IsSelfSigned(&Cert0) == g_aFiles[i].fSelfSigned);
                            RTTESTI_CHECK(RTCrX509Certificate_IsSelfSigned(&Cert1) == g_aFiles[i].fSelfSigned);
                            RTTESTI_CHECK(RTCrX509Certificate_IsSelfSigned(&Cert2) == g_aFiles[i].fSelfSigned);

                            if (g_aFiles[i].fSelfSigned)
                            {
                                /*
                                 * Verify the certificate signature (self signed).
                                 */
                                for (j = 0; j < RT_ELEMENTS(paCerts); j++)
                                {
                                    rc = RTCrX509Certificate_VerifySignatureSelfSigned(paCerts[j], NULL /*pErrInfo*/);
                                    if (   RT_FAILURE(rc)
                                        && (   rc != VERR_CR_PKIX_OSSL_CIPHER_ALGO_NOT_KNOWN_EVP
                                            || !g_aFiles[i].fMaybeNotInOpenSSL) )
                                        RTTestIFailed("RTCrX509Certificate_VerifySignatureSelfSigned failed for %s (#%u), variation %u: %Rrc",
                                                      g_aFiles[i].pszFile, i, j, rc);
                                }
                            }
                        }

                        RTCrX509Certificate_Delete(&Cert2);
                    }
                    else
                        RTTestIFailed("Error %Rrc decoding PEM file %s (#%u)", rc, g_aFiles[i].pszFile, i);
                    RTCrX509Certificate_Delete(&Cert1);
                }
                else
                    RTTestIFailed("Error %Rrc decoding DER file %s (#%u)", rc, g_aFiles[i].pszFile, i);
            }
            RTCrX509Certificate_Delete(&Cert0);
        }
#endif
    }

    RTCrKeyRelease(hPublicKey);
    hPublicKey = NIL_RTCRKEY;
    RTCrKeyRelease(hPrivateKey);
    hPrivateKey = NIL_RTCRKEY;
}




int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTCrPkix-1", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    test1();

    return RTTestSummaryAndDestroy(g_hTest);
}


