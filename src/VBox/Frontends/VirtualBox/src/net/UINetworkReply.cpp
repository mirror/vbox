/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkReply, i.e. HTTP/HTTPS for update pings++.
 */

/*
 * Copyright (C) 2012-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>
# include <QFile>
# include <QThread>
# include <QRegExp>

/* GUI includes: */
# include "UINetworkReply.h"
# include "UINetworkManager.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"

/* Other VBox includes; */
# include <iprt/initterm.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <iprt/crypto/pem.h>
#include <iprt/crypto/store.h>
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/path.h>
#include <iprt/sha.h>
#include <iprt/zip.h>


/**
 * Our network-reply thread
 */
class UINetworkReplyPrivateThread : public QThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UINetworkReplyPrivateThread(const QNetworkRequest &request);

    /** Returns short descriptive context of thread's current operation. */
    const QString context() const { return m_strContext; }

    /** @name APIs
     * @{ */
    /** Read everything. */
    const QByteArray& readAll() const { return m_reply; }
    /** IRPT error status. */
    int error() const { return m_iError; }
    /** Abort HTTP request. */
    void abort();
    /** @} */

private:
    /** Info about wanted certificate. */
    typedef struct CERTINFO
    {
        /** The certificate subject name. */
        const char *pszSubject;
        /** The size of the DER (ASN.1) encoded certificate. */
        uint16_t    cbEncoded;
        /** Gives the s_aCerts index this certificate is an alternative edition of,
         * UINT16_MAX if no alternative.  This is a complication caused by VeriSign
         * reissuing certificates signed with md2WithRSAEncryption using
         * sha1WithRSAEncryption, since MD2 is comprimised.  (Public key unmodified.)
         * It has no practical meaning for the trusted root anchor use we put it to.  */
        uint16_t    iAlternativeTo;
        /** The SHA-1 fingerprint (of the encoded data).   */
        uint8_t     abSha1[RTSHA1_HASH_SIZE];
        /** The SHA-512 fingerprint (of the encoded data).   */
        uint8_t     abSha512[RTSHA512_HASH_SIZE];
        /** Filename in the zip file we download (PEM). */
        const char *pszZipFile;
        /** List of direct URLs to PEM formatted files.. */
        const char *apszUrls[4];
    } CERTINFO;


    /** @name Helpers - HTTP stuff
     * @{ */
    int applyProxyRules();
    int applyHttpsCertificates();
    int applyRawHeaders();
    int performMainRequest();
    /** @} */

    /* Helper: Main thread runner: */
    void run();

    /** @name Static helpers for HTTP and Certificates handling.
     * @{ */
    static QString fullCertificateFileName();
    static int applyProxyRules(RTHTTP hHttp, const QString &strHostName, int iPort);
    static int applyRawHeaders(RTHTTP hHttp, const QList<QByteArray> &headers, const QNetworkRequest &request);
    static uint64_t certAllFoundMask(void);
    static uint64_t certEntryFoundMask(uint32_t iCert);
    static bool checkCertificatesInFile(const char *pszCaCertFile);
    static bool checkCertificatesInStore(RTCRSTORE hStore, unsigned *pcCertificates = NULL);
    static int downloadCertificates(RTHTTP hHttp, const char *pszCaCertFile);
    static int convertVerifyAndAddPemCertificateToStore(RTCRSTORE hStore, void const *pvResponse,
                                                        size_t cbResponse, const CERTINFO *pCertInfo);
    static int retrieveCertificatesFromSystem(const char *pszCaCertFile);
    /** @} */

    /** Holds short descriptive context of thread's current operation. */
    QString m_strContext;

    /* Variables: */
    QNetworkRequest m_request;
    int m_iError;
    /** IPRT HTTP client instance handle. */
    RTHTTP m_hHttp;
    QByteArray m_reply;

    static const QString s_strCertificateFileName;
    static const CERTINFO s_aCerts[3];
};


/**
 * Details on the certificates we are after.
 */
/* static */ const UINetworkReplyPrivateThread::CERTINFO UINetworkReplyPrivateThread::s_aCerts[3] =
{
    /*[0] =*/   /* The reissued version with the SHA-1 signature. */
    {
        /*.pszSubject =*/
        "C=US, O=VeriSign, Inc., OU=Class 3 Public Primary Certification Authority",
        /*.cbEncoded      =*/   0x240,
        /*.iAlternativeTo =*/   1,
        /*.abSha1         =*/
        {
            0xa1, 0xdb, 0x63, 0x93, 0x91, 0x6f, 0x17, 0xe4, 0x18, 0x55,
            0x09, 0x40, 0x04, 0x15, 0xc7, 0x02, 0x40, 0xb0, 0xae, 0x6b
        },
        /*.abSha512       =*/
        {
            0xbb, 0xf7, 0x8a, 0x19, 0x9f, 0x37, 0xee, 0xa2,
            0xce, 0xc8, 0xaf, 0xe3, 0xd6, 0x22, 0x54, 0x20,
            0x74, 0x67, 0x6e, 0xa5, 0x19, 0xb7, 0x62, 0x1e,
            0xc1, 0x2f, 0xd5, 0x08, 0xf4, 0x64, 0xc4, 0xc6,
            0xbb, 0xc2, 0xf2, 0x35, 0xe7, 0xbe, 0x32, 0x0b,
            0xde, 0xb2, 0xfc, 0x44, 0x92, 0x5b, 0x8b, 0x9b,
            0x77, 0xa5, 0x40, 0x22, 0x18, 0x12, 0xcb, 0x3d,
            0x0a, 0x67, 0x83, 0x87, 0xc5, 0x45, 0xc4, 0x99
        },
        /*.pszZipFile     =*/
        "VeriSign Root Certificates/Generation 1 (G1) PCAs/Class 3 Public Primary Certification Authority.pem",
        /*.apszUrls[3]    =*/
        {
            "http://www.symantec.com/content/en/us/enterprise/verisign/roots/Class-3-Public-Primary-Certification-Authority.pem",
            "http://www.verisign.com/repository/roots/root-certificates/PCA-3.pem", /* dead */
            NULL,
            "http://update.virtualbox.org/cacerts-symantec-PCA-3-pem-has-gone-missing-again" /* attention getter */
        },
    },
    /*[1] =*/   /* The original version with the MD2 signature. */
    {
        /*.pszSubject     =*/
        "C=US, O=VeriSign, Inc., OU=Class 3 Public Primary Certification Authority",
        /*.cbEncoded      =*/   0x240,
        /*.iAlternativeTo =*/   0,
        /*.abSha1         =*/
        {
            0x74, 0x2c, 0x31, 0x92, 0xe6, 0x07, 0xe4, 0x24, 0xeb, 0x45,
            0x49, 0x54, 0x2b, 0xe1, 0xbb, 0xc5, 0x3e, 0x61, 0x74, 0xe2
        },
        /*.abSha512       =*/
        {
            0x7c, 0x2f, 0x94, 0x22, 0x5f, 0x67, 0x98, 0x89,
            0xb9, 0xde, 0xd7, 0x41, 0xa0, 0x0d, 0xb1, 0x5c,
            0xc6, 0xca, 0x28, 0x12, 0xbf, 0xbc, 0xa8, 0x2b,
            0x22, 0x53, 0x7a, 0xf8, 0x32, 0x41, 0x2a, 0xbb,
            0xc1, 0x05, 0xe0, 0x0c, 0xd0, 0xa3, 0x97, 0x9d,
            0x5f, 0xcd, 0xe9, 0x9b, 0x68, 0x06, 0xe8, 0xe6,
            0xce, 0xef, 0xb2, 0x71, 0x8e, 0x91, 0x60, 0xa2,
            0xc8, 0x0c, 0x5a, 0xe7, 0x8b, 0x33, 0xf2, 0xaa
        },
        /*.pszZipFile     =*/ NULL,
        /*.apszUrls[3]    =*/ { NULL, NULL, NULL },
    },
    /*[2] =*/
    {
        /*.pszSubject =*/
        "C=US, O=VeriSign, Inc., OU=VeriSign Trust Network, OU=(c) 2006 VeriSign, Inc. - For authorized use only, "
        "CN=VeriSign Class 3 Public Primary Certification Authority - G5",
        /*.cbEncoded      =*/   0x4d7,
        /*.iAlternativeTo =*/   UINT16_MAX,
        /*.abSha1         =*/
        {
            0x4e, 0xb6, 0xd5, 0x78, 0x49, 0x9b, 0x1c, 0xcf, 0x5f, 0x58,
            0x1e, 0xad, 0x56, 0xbe, 0x3d, 0x9b, 0x67, 0x44, 0xa5, 0xe5
        },
        /*.abSha512   =*/
        {
            0xd4, 0xf8, 0x10, 0x54, 0x72, 0x77, 0x0a, 0x2d,
            0xe3, 0x17, 0xb3, 0xcf, 0xed, 0x61, 0xae, 0x5c,
            0x5d, 0x3e, 0xde, 0xa1, 0x41, 0x35, 0xb2, 0xdf,
            0x60, 0xe2, 0x61, 0xfe, 0x3a, 0xc1, 0x66, 0xa3,
            0x3c, 0x88, 0x54, 0x04, 0x4f, 0x1d, 0x13, 0x46,
            0xe3, 0x8c, 0x06, 0x92, 0x9d, 0x70, 0x54, 0xc3,
            0x44, 0xeb, 0x2c, 0x74, 0x25, 0x9e, 0x5d, 0xfb,
            0xd2, 0x6b, 0xa8, 0x9a, 0xf0, 0xb3, 0x6a, 0x01
        },
        /*.pszZipFile     =*/
        "VeriSign Root Certificates/Generation 5 (G5) PCA/VeriSign Class 3 Public Primary Certification Authority - G5.pem",
        /*.apszUrls[3]    =*/
        {
            "http://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class%203-Public-Primary-Certification-Authority-G5.pem",
            "http://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class-3-Public-Primary-Certification-Authority-G5.pem", /* (in case they correct above typo) */
            "http://www.verisign.com/repository/roots/root-certificates/PCA-3G5.pem", /* dead */
            "http://update.virtualbox.org/cacerts-symantec-PCA-3G5-pem-has-gone-missing-again" /* attention getter */
        },
    },
};


/** The certificate file name (no path). */
/* static */ const QString UINetworkReplyPrivateThread::s_strCertificateFileName = QString("vbox-ssl-cacertificate.crt");


UINetworkReplyPrivateThread::UINetworkReplyPrivateThread(const QNetworkRequest &request)
    : m_request(request)
    , m_iError(VINF_SUCCESS)
    , m_hHttp(NIL_RTHTTP)
{
}

void UINetworkReplyPrivateThread::abort()
{
    /* Call for abort: */
    if (m_hHttp != NIL_RTHTTP)
        RTHttpAbort(m_hHttp);
}

int UINetworkReplyPrivateThread::applyProxyRules()
{
    /* Set thread context: */
    m_strContext = tr("During proxy configuration");

    /* Make sure proxy is enabled in Proxy Manager: */
    UIProxyManager proxyManager(vboxGlobal().settings().proxySettings());
    if (!proxyManager.proxyEnabled())
        return VINF_SUCCESS;

    /* Apply proxy rules: */
    return applyProxyRules(m_hHttp,
                           proxyManager.proxyHost(),
                           proxyManager.proxyPort().toUInt());
}

int UINetworkReplyPrivateThread::applyHttpsCertificates()
{
    /* Set thread context: */
    m_strContext = tr("During certificate downloading");

    /*
     * Calc the filename of the CA certificate file.
     */
    const QString strFullCertificateFileName(fullCertificateFileName());
    QByteArray utf8FullCertificateFileName = strFullCertificateFileName.toUtf8();
    const char *pszCaCertFile = utf8FullCertificateFileName.constData();

    /*
     * Check that the certificate file is recent and contains the necessary certificates.
     */
    int rc;
    if (checkCertificatesInFile(pszCaCertFile))
        rc = RTHttpSetCAFile(m_hHttp, pszCaCertFile);
    else
    {
        /*
         * Need to create/update the CA certificate file.  Try see if the necessary
         * certificates are to be found somewhere on the local system, then fall back
         * to downloading them.
         */
        rc = retrieveCertificatesFromSystem(pszCaCertFile);
        if (RT_FAILURE(rc))
            rc = downloadCertificates(m_hHttp, pszCaCertFile);

        if (RT_SUCCESS(rc))
            rc = RTHttpSetCAFile(m_hHttp, pszCaCertFile);
    }

    return rc;
}

int UINetworkReplyPrivateThread::applyRawHeaders()
{
    /* Set thread context: */
    m_strContext = tr("During network request");

    /* Make sure we have a raw headers at all: */
    QList<QByteArray> headers = m_request.rawHeaderList();
    if (headers.isEmpty())
        return VINF_SUCCESS;

    /* Apply raw headers: */
    return applyRawHeaders(m_hHttp, headers, m_request);
}

int UINetworkReplyPrivateThread::performMainRequest()
{
    /* Set thread context: */
    m_strContext = tr("During network request");

    /* Paranoia: */
    m_reply.clear();

    /* Perform blocking HTTP GET request: */
    char *pszResponse;
    /** @todo r=bird: Use RTHttpGetBinary? */
    int rc = RTHttpGetText(m_hHttp, m_request.url().toString().toUtf8().constData(), &pszResponse);
    if (RT_SUCCESS(rc))
    {
        m_reply = QByteArray(pszResponse);
        RTHttpFreeResponseText(pszResponse);
    }

    return rc;
}

void UINetworkReplyPrivateThread::run()
{
    /* Init: */
    RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB); /** @todo r=bird: WTF? */

    /* Create HTTP object: */
    if (RT_SUCCESS(m_iError))
        m_iError = RTHttpCreate(&m_hHttp);

    /* Apply proxy-rules: */
    if (RT_SUCCESS(m_iError))
        m_iError = applyProxyRules();

    /* Apply https-certificates: */
    if (RT_SUCCESS(m_iError))
        m_iError = applyHttpsCertificates();

    /* Assign raw-headers: */
    if (RT_SUCCESS(m_iError))
        m_iError = applyRawHeaders();

    /* Perform main request: */
    if (RT_SUCCESS(m_iError))
        m_iError = performMainRequest();

    /* Destroy HTTP client instance: */
    RTHTTP hHttp = m_hHttp;
    if (hHttp != NIL_RTHTTP)
    {
        /** @todo r=bird: There is a race here between this and abort()! */
        m_hHttp = NIL_RTHTTP;
        RTHttpDestroy(hHttp);
    }
}

/* static */
QString UINetworkReplyPrivateThread::fullCertificateFileName()
{
    const QDir homeDir(QDir::toNativeSeparators(vboxGlobal().homeFolder()));
    return QDir::toNativeSeparators(homeDir.absoluteFilePath(s_strCertificateFileName));
}

/* static */
int UINetworkReplyPrivateThread::applyProxyRules(RTHTTP hHttp, const QString &strHostName, int iPort)
{
    /* Make sure HTTP is created: */
    if (hHttp == NIL_RTHTTP)
        return VERR_INVALID_HANDLE;

    /* Apply HTTP proxy: */
    return RTHttpSetProxy(hHttp,
                          strHostName.toAscii().constData(),
                          iPort,
                          0 /* login */, 0 /* password */);
}

/* static */
int UINetworkReplyPrivateThread::applyRawHeaders(RTHTTP hHttp, const QList<QByteArray> &headers, const QNetworkRequest &request)
{
    /* Make sure HTTP is created: */
    if (hHttp == NIL_RTHTTP)
        return VERR_INVALID_HANDLE;

    /* We should format them first: */
    QVector<QByteArray> formattedHeaders;
    QVector<const char*> formattedHeaderPointers;
    foreach (const QByteArray &header, headers)
    {
        /* Prepare formatted representation: */
        QString strFormattedString = QString("%1: %2").arg(QString(header), QString(request.rawHeader(header)));
        formattedHeaders << strFormattedString.toAscii();
        formattedHeaderPointers << formattedHeaders.last().constData();
    }
    const char **ppFormattedHeaders = formattedHeaderPointers.data();

    /* Apply HTTP headers: */
    return RTHttpSetHeaders(hHttp, formattedHeaderPointers.size(), ppFormattedHeaders);
}

/**
 * Checks if the certificates we desire are all present in the given file, and
 * that the file is recent enough (not for downloaded certs).
 *
 * @returns true if fine, false if not.
 * @param   pszCaCertFile   The path to the certificate file.
 */
/*static*/ bool
UINetworkReplyPrivateThread::checkCertificatesInFile(const char *pszCaCertFile)
{
    bool fFoundCerts = false;

    /*
     * Check whether the file exists.
     */
    RTFSOBJINFO Info;
    int rc = RTPathQueryInfoEx(pszCaCertFile, &Info, RTFSOBJATTRADD_NOTHING, RTPATH_F_FOLLOW_LINK);
    if (   RT_SUCCESS(rc)
        && RTFS_IS_FILE(Info.Attr.fMode))
    {
        /*
         * Load the CA certificate file into a store and use
         * checkCertificatesInStore to do the real work.
         */
        RTCRSTORE hStore;
        int rc = RTCrStoreCreateInMem(&hStore, 256);
        if (RT_SUCCESS(rc))
        {
            RTERRINFOSTATIC StaticErrInfo;
            rc = RTCrStoreCertAddFromFile(hStore, RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR, pszCaCertFile,
                                          RTErrInfoInitStatic(&StaticErrInfo));
            if (RTErrInfoIsSet(&StaticErrInfo.Core))
                LogRel(("checkCertificates: %s\n", StaticErrInfo.Core.pszMsg));
            else
                AssertRC(rc);

            unsigned cCertificates = 0;
            fFoundCerts = checkCertificatesInStore(hStore, &cCertificates);

            RTCrStoreRelease(hStore);

            /*
             * If there are more than two certificates in the database, we're looking
             * at a mirror of the system CA stores.  Refresh our snapshot once every 28 days.
             */
            RTTIMESPEC MaxAge;
            if (   fFoundCerts
                && cCertificates > 2
                && RTTimeSpecCompare(&Info.ModificationTime, RTTimeSpecSubSeconds(RTTimeNow(&MaxAge), 28 *24*3600)) < 0)
                fFoundCerts = false;
        }
    }

    return fFoundCerts;
}

/**
 * Calculates the 64-bit all-certs found mask.
 *
 * @returns 64-bit mask.
 */
/*static*/ uint64_t
UINetworkReplyPrivateThread::certAllFoundMask()
{
    AssertCompile(RT_ELEMENTS(s_aCerts) < 64);
    return RT_BIT_64(RT_ELEMENTS(s_aCerts)) - UINT64_C(1);
}

/**
 * Calculates the 64-bit 'found' mask for a certificate entry.
 *
 * @returns 64-bit mask.
 * @param   iCert               The certificate entry.
 */
/*static*/ uint64_t
UINetworkReplyPrivateThread::certEntryFoundMask(uint32_t iCert)
{
    Assert(iCert < RT_ELEMENTS(s_aCerts));
    uint64_t fMask = RT_BIT_64(iCert);

    /*
     * Tedium: Also mark certificates that this is an alternative to, we only need
     *         the public key once.
     */
    uint16_t iAlt = s_aCerts[iCert].iAlternativeTo;
    if (iAlt != UINT16_MAX)
    {
        unsigned cMax = 10;
        do
        {
            Assert(iAlt < RT_ELEMENTS(s_aCerts));
            Assert(cMax > 1);
            Assert(strcmp(s_aCerts[iAlt].pszSubject, s_aCerts[iCert].pszSubject) == 0);
            fMask |= RT_BIT_64(iAlt);
            iAlt = s_aCerts[iAlt].iAlternativeTo;
        } while (iAlt != iCert && cMax-- > 0);
    }

    return fMask;
}

/**
 * Checks if the certificates we desire are all present in the given store.
 *
 * @returns true if present, false if not.
 * @param   hStore              The store to examine.
 * @param   pcCertificates      Where to return the number of certificates in
 *                              the store. Optional.
 */
/* static */ bool
UINetworkReplyPrivateThread::checkCertificatesInStore(RTCRSTORE hStore, unsigned *pcCertificates /* = NULL*/)
{
    if (pcCertificates)
        *pcCertificates = 0;

    /*
     * Enumerate the store, checking for the certificates we need.
     */
    RTCRSTORECERTSEARCH Search;
    int rc = RTCrStoreCertFindAll(hStore, &Search);
    if (RT_SUCCESS(rc))
    {
        uint64_t      fFoundCerts   = 0;
        unsigned      cCertificates = 0;
        PCRTCRCERTCTX pCertCtx;
        while ((pCertCtx = RTCrStoreCertSearchNext(hStore, &Search)) != NULL)
        {
            if (   (pCertCtx->fFlags & RTCRCERTCTX_F_ENC_MASK) == RTCRCERTCTX_F_ENC_X509_DER
                && pCertCtx->cbEncoded > 0
                && pCertCtx->pCert)
            {
                cCertificates++;

                /*
                 * It is a X.509 certificate.  Check if it matches any of those we're looking for.
                 */
                for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
                    if (   pCertCtx->cbEncoded == s_aCerts[i].cbEncoded
                        && RTCrX509Name_MatchWithString(&pCertCtx->pCert->TbsCertificate.Subject, s_aCerts[i].pszSubject))
                    {
                        if (RTSha1Check(pCertCtx->pabEncoded, pCertCtx->cbEncoded, s_aCerts[i].abSha1))
                        {
                            if (RTSha512Check(pCertCtx->pabEncoded, pCertCtx->cbEncoded, s_aCerts[i].abSha512))
                            {
                                fFoundCerts |= certEntryFoundMask(i);
                                break;
                            }
                        }
                    }
            }
            RTCrCertCtxRelease(pCertCtx);
        }
        int rc2 = RTCrStoreCertSearchDestroy(hStore, &Search); AssertRC(rc2);

        /*
         * Set the certificate count.
         */
        if (pcCertificates)
            *pcCertificates = cCertificates;

        /*
         * Did we locate all of them?
         */
        if (fFoundCerts == certAllFoundMask())
            return true;
    }
    AssertRC(rc);
    return false;
}

/*static*/ int
UINetworkReplyPrivateThread::downloadCertificates(RTHTTP hHttp, const char *pszCaCertFile)
{
    /*
     * Prepare temporary certificate store.
     */
    RTCRSTORE hStore;
    int rc = RTCrStoreCreateInMem(&hStore, RT_ELEMENTS(s_aCerts));
    AssertRCReturn(rc, rc);

    /*
     * Accounts for certificates we've downloaded, verified and added to the store.
     */
    uint64_t fFoundCerts = 0;
    AssertCompile(RT_ELEMENTS(s_aCerts) < 64);

    /*
     * Try get the roots.zip from symantec (or virtualbox.org) first.
     */
    static const char * const a_apszRootsZipUrls[] =
    {
        "http://www.symantec.com/content/en/us/enterprise/verisign/roots/roots.zip",
        "http://update.virtualbox.org/cacerts-symantec-roots-zip-has-gone-missing-again" /* Just to try grab our attention. */
    };
    for (uint32_t iUrl = 0; iUrl < RT_ELEMENTS(a_apszRootsZipUrls); iUrl++)
    {
        void   *pvRootsZip;
        size_t  cbRootsZip;
        rc = RTHttpGetBinary(hHttp, a_apszRootsZipUrls[iUrl], &pvRootsZip, &cbRootsZip);
        if (RT_SUCCESS(rc))
        {
            for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
                if (s_aCerts[i].pszZipFile)
                {
                    void  *pvFile;
                    size_t cbFile;
                    rc = RTZipPkzipMemDecompress(&pvFile, &cbFile, pvRootsZip, cbRootsZip, s_aCerts[i].pszZipFile);
                    if (RT_SUCCESS(rc))
                    {
                        rc = convertVerifyAndAddPemCertificateToStore(hStore, pvFile, cbFile, &s_aCerts[i]);
                        RTMemFree(pvFile);
                        if (RT_SUCCESS(rc))
                            fFoundCerts |= certEntryFoundMask(i);
                    }
                }
            RTHttpFreeResponse(pvRootsZip);
            if (fFoundCerts == certAllFoundMask())
                break;
        }
    }

    /*
     * Fallback: Try download certificates separately.
     */
    if (fFoundCerts != certAllFoundMask())
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
            if (!(fFoundCerts & RT_BIT_64(i)))
                for (uint32_t iUrl = 0; iUrl < RT_ELEMENTS(s_aCerts[i].apszUrls); i++)
                    if (s_aCerts[i].apszUrls[iUrl])
                    {
                        void  *pvResponse;
                        size_t cbResponse;
                        rc = RTHttpGetBinary(hHttp, s_aCerts[i].apszUrls[iUrl], &pvResponse, &cbResponse);
                        if (RT_SUCCESS(rc))
                        {
                            rc = convertVerifyAndAddPemCertificateToStore(hStore, pvResponse, cbResponse, &s_aCerts[i]);
                            RTHttpFreeResponse(pvResponse);
                            if (RT_SUCCESS(rc))
                            {
                                fFoundCerts |= certEntryFoundMask(i);
                                break;
                            }
                        }
                    }

    /*
     * See if we've got the certificates we want, save it we do.
     */
    if (fFoundCerts == certAllFoundMask())
        rc = RTCrStoreCertExportAsPem(hStore, 0 /*fFlags*/, pszCaCertFile);
    else if (RT_SUCCESS(rc))
        rc = VERR_NOT_FOUND;

    RTCrStoreRelease(hStore);
    return rc;
}

/**
 * Converts a PEM certificate, verifies it against @a pCertInfo and adds it to
 * the given store.
 *
 * @returns IPRT status code.
 * @param   hStore              The store to add it to.
 * @param   pvResponse          The raw PEM certificate file bytes.
 * @param   cbResponse          The number of bytes.
 * @param   pCertInfo           The certificate info (we use hashes and encoded
 *                              size).
 */
/*static*/ int
UINetworkReplyPrivateThread::convertVerifyAndAddPemCertificateToStore(RTCRSTORE hStore,
                                                                      void const *pvResponse, size_t cbResponse,
                                                                      const CERTINFO *pCertInfo)
{
    /*
     * Convert the PEM certificate to its binary form so we can hash it.
     */
    static RTCRPEMMARKERWORD const s_aWords_Certificate[]  = { { RT_STR_TUPLE("CERTIFICATE") } };
    static RTCRPEMMARKER     const s_aCertificateMarkers[] = { { s_aWords_Certificate, RT_ELEMENTS(s_aWords_Certificate) }, };
    RTERRINFOSTATIC StaticErrInfo;
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemParseContent(pvResponse, cbResponse, 0 /*fFlags*/,
                                 &s_aCertificateMarkers[0], RT_ELEMENTS(s_aCertificateMarkers),
                                 &pSectionHead, RTErrInfoInitStatic(&StaticErrInfo));
    if (RTErrInfoIsSet(&StaticErrInfo.Core))
        LogRel(("RTCrPemParseContent: %s\n", StaticErrInfo.Core.pszMsg));
    if (RT_SUCCESS(rc))
    {
        /*
         * Look at what we got back and hash it.
         */
        rc = VERR_NOT_FOUND;
        for (PCRTCRPEMSECTION pCur = pSectionHead; pCur; pCur = pCur->pNext)
            if (pCur->cbData == pCertInfo->cbEncoded)
            {
                if (   RTSha1Check(pCur->pbData, pCur->cbData, pCertInfo->abSha1)
                    && RTSha512Check(pCur->pbData, pCur->cbData, pCertInfo->abSha512))
                {
                    /*
                     * Matching, add it to the store.
                     */
                    rc = RTCrStoreCertAddEncoded(hStore,
                                                 RTCRCERTCTX_F_ENC_X509_DER | RTCRCERTCTX_F_ADD_IF_NOT_FOUND,
                                                 pCur->pbData, pCur->cbData,
                                                 RTErrInfoInitStatic(&StaticErrInfo));
                    if (RTErrInfoIsSet(&StaticErrInfo.Core))
                        LogRel(("RTCrStoreCertAddEncoded: %s\n", StaticErrInfo.Core.pszMsg));
                    else if (RT_FAILURE(rc))
                        LogRel(("RTCrStoreCertAddEncoded: %Rrc\n", rc));
                    if (RT_SUCCESS(rc))
                        break;
                }
                else
                    LogRel(("convertVerifyAndAddPemCertificateToStore: hash mismatch (cbData=%#zx)\n", pCur->cbData));
            }
            else
                LogRel(("convertVerifyAndAddPemCertificateToStore: cbData=%#zx expected %#zx\n",
                        pCur->cbData, pCertInfo->cbEncoded));

        RTCrPemFreeSections(pSectionHead);
    }
    return rc;
}

/**
 * Tries to retrieve an up to date list of certificates from the system that
 * includes the necessary certs.
 *
 * @returns IPRT status code, success indicating that we've found what we need.
 * @param   pszCaCertFile           Where to store the certificates.
 */
/*static*/ int
UINetworkReplyPrivateThread::retrieveCertificatesFromSystem(const char *pszCaCertFile)
{
    /*
     * Duplicate the user and system stores.
     */
    RTERRINFOSTATIC StaticErrInfo;
    RTCRSTORE hUserStore;
    int rc = RTCrStoreCreateSnapshotById(&hUserStore, RTCRSTOREID_USER_TRUSTED_CAS_AND_CERTIFICATES,
                                         RTErrInfoInitStatic(&StaticErrInfo));
    if (RT_FAILURE(rc))
        hUserStore = NIL_RTCRSTORE;
    if (RTErrInfoIsSet(&StaticErrInfo.Core))
        LogRel(("retrieveCertificatesFromSystem/#1: %s\n", StaticErrInfo.Core.pszMsg));

    RTCRSTORE hSystemStore;
    rc = RTCrStoreCreateSnapshotById(&hSystemStore, RTCRSTOREID_SYSTEM_TRUSTED_CAS_AND_CERTIFICATES,
                                     RTErrInfoInitStatic(&StaticErrInfo));
    if (RT_FAILURE(rc))
        hUserStore = NIL_RTCRSTORE;
    if (RTErrInfoIsSet(&StaticErrInfo.Core))
        LogRel(("retrieveCertificatesFromSystem/#2: %s\n", StaticErrInfo.Core.pszMsg));

    /*
     * Merge the two.
     */
    int rc2 = RTCrStoreCertAddFromStore(hUserStore, RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR,
                                        hSystemStore);
    AssertRC(rc2);
    RTCrStoreRelease(hSystemStore);
    hSystemStore = NIL_RTCRSTORE;

    /*
     * See if we've got the certificates we want, save it we do.
     */
    if (checkCertificatesInStore(hUserStore))
        rc = RTCrStoreCertExportAsPem(hUserStore, 0 /*fFlags*/, pszCaCertFile);
    else
        rc = VERR_NOT_FOUND;
    RTCrStoreRelease(hUserStore);
    return rc;
}



/**
 * Our network-reply (HTTP) object.
 */
class UINetworkReplyPrivate : public QObject
{
    Q_OBJECT;

signals:

    /* Notifiers: */
    void downloadProgress(qint64 iBytesReceived, qint64 iBytesTotal);
    void finished();

public:

    /* Constructor: */
    UINetworkReplyPrivate(const QNetworkRequest &request)
        : m_error(QNetworkReply::NoError)
        , m_pThread(0)
    {
        /* Prepare full error template: */
        m_strErrorTemplate = tr("%1: %2", "Context description: Error description");
        /* Create and run network-reply thread: */
        m_pThread = new UINetworkReplyPrivateThread(request);
        connect(m_pThread, SIGNAL(finished()), this, SLOT(sltFinished()));
        m_pThread->start();
    }

    /* Destructor: */
    ~UINetworkReplyPrivate()
    {
        /* Terminate network-reply thread: */
        m_pThread->abort();
        m_pThread->wait();
        delete m_pThread;
        m_pThread = 0;
    }

    /* API: Abort reply: */
    void abort()
    {
        m_pThread->abort();
    }

    /* API: Error-code getter: */
    QNetworkReply::NetworkError error() const { return m_error; }

    /* API: Error-string getter: */
    QString errorString() const
    {
        switch (m_error)
        {
            case QNetworkReply::NoError:                     break;
            case QNetworkReply::RemoteHostClosedError:       return m_strErrorTemplate.arg(m_pThread->context(), tr("Unable to initialize HTTP library"));
            case QNetworkReply::HostNotFoundError:           return m_strErrorTemplate.arg(m_pThread->context(), tr("Host not found"));
            case QNetworkReply::ContentAccessDenied:         return m_strErrorTemplate.arg(m_pThread->context(), tr("Content access denied"));
            case QNetworkReply::ProtocolFailure:             return m_strErrorTemplate.arg(m_pThread->context(), tr("Protocol failure"));
            case QNetworkReply::ConnectionRefusedError:      return m_strErrorTemplate.arg(m_pThread->context(), tr("Connection refused"));
            case QNetworkReply::SslHandshakeFailedError:     return m_strErrorTemplate.arg(m_pThread->context(), tr("SSL authentication failed"));
            case QNetworkReply::AuthenticationRequiredError: return m_strErrorTemplate.arg(m_pThread->context(), tr("Wrong SSL certificate format"));
            case QNetworkReply::ContentReSendError:          return m_strErrorTemplate.arg(m_pThread->context(), tr("Content moved"));
            case QNetworkReply::ProxyNotFoundError:          return m_strErrorTemplate.arg(m_pThread->context(), tr("Proxy not found"));
            default:                                         return m_strErrorTemplate.arg(m_pThread->context(), tr("Unknown reason"));
        }
        return QString();
    }

    /* API: Reply getter: */
    QByteArray readAll() { return m_pThread->readAll(); }

private slots:

    /* Handler: Thread finished: */
    void sltFinished()
    {
        switch (m_pThread->error())
        {
            case VINF_SUCCESS:                         m_error = QNetworkReply::NoError; break;
            case VERR_HTTP_INIT_FAILED:                m_error = QNetworkReply::RemoteHostClosedError; break;
            case VERR_HTTP_NOT_FOUND:                  m_error = QNetworkReply::HostNotFoundError; break;
            case VERR_HTTP_ACCESS_DENIED:              m_error = QNetworkReply::ContentAccessDenied; break;
            case VERR_HTTP_BAD_REQUEST:                m_error = QNetworkReply::ProtocolFailure; break;
            case VERR_HTTP_COULDNT_CONNECT:            m_error = QNetworkReply::ConnectionRefusedError; break;
            case VERR_HTTP_SSL_CONNECT_ERROR:          m_error = QNetworkReply::SslHandshakeFailedError; break;
            case VERR_HTTP_CACERT_WRONG_FORMAT:        m_error = QNetworkReply::AuthenticationRequiredError; break;
            case VERR_HTTP_CACERT_CANNOT_AUTHENTICATE: m_error = QNetworkReply::AuthenticationRequiredError; break;
            case VERR_HTTP_ABORTED:                    m_error = QNetworkReply::OperationCanceledError; break;
            case VERR_HTTP_REDIRECTED:                 m_error = QNetworkReply::ContentReSendError; break;
            case VERR_HTTP_PROXY_NOT_FOUND:            m_error = QNetworkReply::ProxyNotFoundError; break;
            default:                                   m_error = QNetworkReply::UnknownNetworkError; break;
        }
        emit finished();
    }

private:

    /** Holds full error template in "Context description: Error description" form. */
    QString m_strErrorTemplate;

    /* Variables: */
    QNetworkReply::NetworkError m_error;
    UINetworkReplyPrivateThread *m_pThread;
};

UINetworkReply::UINetworkReply(const QNetworkRequest &request, UINetworkRequestType requestType)
    : m_replyType(UINetworkReplyType_Qt)
    , m_pReply(0)
{
    /* Create network-reply object: */
    switch (requestType)
    {
        /* Prepare Qt network-reply (HEAD): */
        case UINetworkRequestType_HEAD:
            m_replyType = UINetworkReplyType_Qt;
            m_pReply = gNetworkManager->head(request);
            break;
        /* Prepare Qt network-reply (GET): */
        case UINetworkRequestType_GET:
            m_replyType = UINetworkReplyType_Qt;
            m_pReply = gNetworkManager->get(request);
            break;
        /* Prepare our network-reply (GET): */
        case UINetworkRequestType_GET_Our:
            m_replyType = UINetworkReplyType_Our;
            m_pReply = new UINetworkReplyPrivate(request);
            break;
    }

    /* Prepare network-reply object connections: */
    connect(m_pReply, SIGNAL(downloadProgress(qint64, qint64)), this, SIGNAL(downloadProgress(qint64, qint64)));
    connect(m_pReply, SIGNAL(finished()), this, SIGNAL(finished()));
}

UINetworkReply::~UINetworkReply()
{
    /* Cleanup network-reply object: */
    if (m_pReply)
    {
        delete m_pReply;
        m_pReply = 0;
    }
}

QVariant UINetworkReply::header(QNetworkRequest::KnownHeaders header) const
{
    QVariant result;
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: result = qobject_cast<QNetworkReply*>(m_pReply)->header(header); break;
        case UINetworkReplyType_Our: /* TODO: header() */ break;
    }
    return result;
}

QVariant UINetworkReply::attribute(QNetworkRequest::Attribute code) const
{
    QVariant result;
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: result = qobject_cast<QNetworkReply*>(m_pReply)->attribute(code); break;
        case UINetworkReplyType_Our: /* TODO: attribute() */ break;
    }
    return result;
}

void UINetworkReply::abort()
{
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: qobject_cast<QNetworkReply*>(m_pReply)->abort(); break;
        case UINetworkReplyType_Our: qobject_cast<UINetworkReplyPrivate*>(m_pReply)->abort(); break;
    }
}

QNetworkReply::NetworkError UINetworkReply::error() const
{
    QNetworkReply::NetworkError result = QNetworkReply::NoError;
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: result = qobject_cast<QNetworkReply*>(m_pReply)->error(); break;
        case UINetworkReplyType_Our: result = qobject_cast<UINetworkReplyPrivate*>(m_pReply)->error(); break;
    }
    return result;
}

QString UINetworkReply::errorString() const
{
    QString strResult;
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: strResult = qobject_cast<QNetworkReply*>(m_pReply)->errorString(); break;
        case UINetworkReplyType_Our: strResult = qobject_cast<UINetworkReplyPrivate*>(m_pReply)->errorString(); break;
    }
    return strResult;
}

QByteArray UINetworkReply::readAll()
{
    QByteArray result;
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: result = qobject_cast<QNetworkReply*>(m_pReply)->readAll(); break;
        case UINetworkReplyType_Our: result = qobject_cast<UINetworkReplyPrivate*>(m_pReply)->readAll(); break;
    }
    return result;
}

QUrl UINetworkReply::url() const
{
    QUrl result;
    switch (m_replyType)
    {
        case UINetworkReplyType_Qt: result = qobject_cast<QNetworkReply*>(m_pReply)->url(); break;
        case UINetworkReplyType_Our: /* TODO: url() */ break;
    }
    return result;
}

#include "UINetworkReply.moc"

