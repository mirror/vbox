

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <Windows.h>
#include <Wincrypt.h>

#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/string.h>

static const char *errorToString(DWORD dwErr)
{
    switch (dwErr)
    {
#define MY_CASE(a_uConst)       case a_uConst: return #a_uConst;
        MY_CASE(CRYPT_E_MSG_ERROR);
        MY_CASE(CRYPT_E_UNKNOWN_ALGO);
        MY_CASE(CRYPT_E_OID_FORMAT);
        MY_CASE(CRYPT_E_INVALID_MSG_TYPE);
        MY_CASE(CRYPT_E_UNEXPECTED_ENCODING);
        MY_CASE(CRYPT_E_AUTH_ATTR_MISSING);
        MY_CASE(CRYPT_E_HASH_VALUE);
        MY_CASE(CRYPT_E_INVALID_INDEX);
        MY_CASE(CRYPT_E_ALREADY_DECRYPTED);
        MY_CASE(CRYPT_E_NOT_DECRYPTED);
        MY_CASE(CRYPT_E_RECIPIENT_NOT_FOUND);
        MY_CASE(CRYPT_E_CONTROL_TYPE);
        MY_CASE(CRYPT_E_ISSUER_SERIALNUMBER);
        MY_CASE(CRYPT_E_SIGNER_NOT_FOUND);
        MY_CASE(CRYPT_E_ATTRIBUTES_MISSING);
        MY_CASE(CRYPT_E_STREAM_MSG_NOT_READY);
        MY_CASE(CRYPT_E_STREAM_INSUFFICIENT_DATA);
        MY_CASE(CRYPT_I_NEW_PROTECTION_REQUIRED);
        MY_CASE(CRYPT_E_BAD_LEN);
        MY_CASE(CRYPT_E_BAD_ENCODE);
        MY_CASE(CRYPT_E_FILE_ERROR);
        MY_CASE(CRYPT_E_NOT_FOUND);
        MY_CASE(CRYPT_E_EXISTS);
        MY_CASE(CRYPT_E_NO_PROVIDER);
        MY_CASE(CRYPT_E_SELF_SIGNED);
        MY_CASE(CRYPT_E_DELETED_PREV);
        MY_CASE(CRYPT_E_NO_MATCH);
        MY_CASE(CRYPT_E_UNEXPECTED_MSG_TYPE);
        MY_CASE(CRYPT_E_NO_KEY_PROPERTY);
        MY_CASE(CRYPT_E_NO_DECRYPT_CERT);
        MY_CASE(CRYPT_E_BAD_MSG);
        MY_CASE(CRYPT_E_NO_SIGNER);
        MY_CASE(CRYPT_E_PENDING_CLOSE);
        MY_CASE(CRYPT_E_REVOKED);
        MY_CASE(CRYPT_E_NO_REVOCATION_DLL);
        MY_CASE(CRYPT_E_NO_REVOCATION_CHECK);
        MY_CASE(CRYPT_E_REVOCATION_OFFLINE);
        MY_CASE(CRYPT_E_NOT_IN_REVOCATION_DATABASE);
        MY_CASE(CRYPT_E_INVALID_NUMERIC_STRING);
        MY_CASE(CRYPT_E_INVALID_PRINTABLE_STRING);
        MY_CASE(CRYPT_E_INVALID_IA5_STRING);
        MY_CASE(CRYPT_E_INVALID_X500_STRING);
        MY_CASE(CRYPT_E_NOT_CHAR_STRING);
        MY_CASE(CRYPT_E_FILERESIZED);
        MY_CASE(CRYPT_E_SECURITY_SETTINGS);
        MY_CASE(CRYPT_E_NO_VERIFY_USAGE_DLL);
        MY_CASE(CRYPT_E_NO_VERIFY_USAGE_CHECK);
        MY_CASE(CRYPT_E_VERIFY_USAGE_OFFLINE);
        MY_CASE(CRYPT_E_NOT_IN_CTL);
        MY_CASE(CRYPT_E_NO_TRUSTED_SIGNER);
        MY_CASE(CRYPT_E_MISSING_PUBKEY_PARA);
        MY_CASE(CRYPT_E_OSS_ERROR);
        default:
        {
            static char s_szErr[32];
            RTStrPrintf(s_szErr, sizeof(s_szErr), "#x (%d)", dwErr, dwErr);
            return s_szErr;
        }
    }
}

static RTEXITCODE addToStore(const char *pszFilename, PCRTUTF16 pwszStore)
{
    /*
     * Open the source.
     */
    void   *pvFile;
    size_t  cbFile;
    int rc = RTFileReadAll(pszFilename, &pvFile, &cbFile);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTFileReadAll failed on '%s': %Rrc", pszFilename, rc);

    RTEXITCODE rcExit = RTEXITCODE_FAILURE;

    PCCERT_CONTEXT pCertCtx = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                           (PBYTE)pvFile,
                                                           (DWORD)cbFile);
    if (pCertCtx)
    {
        /*
         * Open the destination.
         */
        HCERTSTORE hDstStore = CertOpenStore(CERT_STORE_PROV_SYSTEM_W,
                                             PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                             NULL /* hCryptProv = default */,
                                             /*CERT_SYSTEM_STORE_LOCAL_MACHINE*/ CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG,
                                             pwszStore);
        if (hDstStore != NULL)
        {
#if 0
            DWORD dwContextType;
            if (CertAddSerializedElementToStore(hDstStore,
                                                pCertCtx->pbCertEncoded,
                                                pCertCtx->cbCertEncoded,
                                                CERT_STORE_ADD_NEW,
                                                0 /* dwFlags (reserved) */,
                                                CERT_STORE_ALL_CONTEXT_FLAG,
                                                &dwContextType,
                                                NULL))
            {
                RTMsgInfo("Successfully added '%s' to the '%ls' store (ctx type %u)", pszFilename, pwszStore, dwContextType);
                rcExit = RTEXITCODE_SUCCESS;
            }
            else
                RTMsgError("CertAddSerializedElementToStore returned %s", errorToString(GetLastError()));
#else
            if (CertAddCertificateContextToStore(hDstStore, pCertCtx, CERT_STORE_ADD_NEW, NULL))
            {
                RTMsgInfo("Successfully added '%s' to the '%ls' store", pszFilename, pwszStore);
                rcExit = RTEXITCODE_SUCCESS;
            }
            else
                RTMsgError("CertAddCertificateContextToStore returned %s", errorToString(GetLastError()));
#endif

            CertCloseStore(hDstStore, CERT_CLOSE_STORE_CHECK_FLAG);
        }
        else
            RTMsgError("CertOpenStoreW returned %s", errorToString(GetLastError()));
        CertFreeCertificateContext(pCertCtx);
    }
    else
        RTMsgError("CertCreateCertificateContext returned %s", errorToString(GetLastError()));
    RTFileReadAllFree(pvFile, cbFile);
    return rcExit;

#if 0

    CRYPT_DATA_BLOB Blob;
    Blob.cbData = (DWORD)cbData;
    Blob.pbData = (PBYTE)pvData;
    HCERTSTORE hSrcStore = PFXImportCertStore(&Blob, L"", )

#endif
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);


    RTEXITCODE rcExit;

    rcExit = addToStore("my", L"my");

    return rcExit;
}
