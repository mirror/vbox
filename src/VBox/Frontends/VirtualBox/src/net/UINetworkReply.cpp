/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkReply stuff implementation.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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

#include <iprt/crypto/store.h>
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/path.h>
#include <iprt/sha.h>
#include <iprt/zip.h>

/* Our network-reply thread: */
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
        /** @todo add zip path and direct URLs. */
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
    static int abort(RTHTTP pHttp);
    static int applyProxyRules(RTHTTP pHttp, const QString &strHostName, int iPort);
    static int applyRawHeaders(RTHTTP pHttp, const QList<QByteArray> &headers, const QNetworkRequest &request);
    static int performGetRequestForText(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply);
    static int performGetRequestForBinary(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply);
    static bool checkCertificatesInFile(const char *pszCaCertFile);
    static bool checkCertificatesInStore(RTCRSTORE hStore, unsigned *pcCertificates = NULL);
    static int decompressCertificate(const QByteArray &package, QByteArray &certificate, const QString &strName);
    static int downloadCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName);
    static int downloadCertificatePca3G5(RTHTTP pHttp, QByteArray &certificate);
    static int downloadCertificatePca3(RTHTTP pHttp, QByteArray &certificate);
    static int retrieveCertificatesFromSystem(const char *pszCaCertFile);
    static int verifyCertificatePca3G5(RTHTTP pHttp, QByteArray const &certificate);
    static int verifyCertificatePca3(RTHTTP pHttp, QByteArray const &certificate);
    static int verifyCertificate(RTHTTP pHttp, QByteArray const &certificate, CERTINFO const *pCertInfo);
    static int saveDownloadedCertificates(const QString &strFullCertificateFileName, const QByteArray &certificatePca3G5, const QByteArray &certificatePca3);
    static int saveCertificate(QFile &file, const QByteArray &certificate);
    /** @} */

    /** Holds short descriptive context of thread's current operation. */
    QString m_strContext;

    /* Variables: */
    QNetworkRequest m_request;
    int m_iError;
    RTHTTP m_pHttp;
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
        }
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
        }
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
        }
    },
};


/** The certificate file name (no path). */
/* static */ const QString UINetworkReplyPrivateThread::s_strCertificateFileName = QString("vbox-ssl-cacertificate.crt");


UINetworkReplyPrivateThread::UINetworkReplyPrivateThread(const QNetworkRequest &request)
    : m_request(request)
    , m_iError(VINF_SUCCESS)
    , m_pHttp(0)
{
}

void UINetworkReplyPrivateThread::abort()
{
    /* Call for abort: */
    abort(m_pHttp);
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
    return applyProxyRules(m_pHttp,
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
        rc = RTHttpSetCAFile(m_pHttp, pszCaCertFile);
    else
    {
        /*
         * Need to create/update the CA certificate file.  Try see if the necessary
         * certificates are to be found somewhere on the local system, then fall back
         * to downloading them.
         */
        rc = retrieveCertificatesFromSystem(pszCaCertFile);
        if (RT_FAILURE(rc))
            rc = downloadCertificates(m_pHttp, strFullCertificateFileName);

        if (RT_SUCCESS(rc))
            rc = RTHttpSetCAFile(m_pHttp, pszCaCertFile);
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
    return applyRawHeaders(m_pHttp, headers, m_request);
}

int UINetworkReplyPrivateThread::performMainRequest()
{
    /* Set thread context: */
    m_strContext = tr("During network request");

    /* Perform GET request: */
    return performGetRequestForText(m_pHttp, m_request, m_reply);
}

void UINetworkReplyPrivateThread::run()
{
    /* Init: */
    RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB);

    /* Create HTTP object: */
    if (RT_SUCCESS(m_iError))
        m_iError = RTHttpCreate(&m_pHttp);

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

    /* Destroy HTTP object: */
    if (m_pHttp)
    {
        RTHttpDestroy(m_pHttp);
        m_pHttp = 0;
    }
}

/* static */
QString UINetworkReplyPrivateThread::fullCertificateFileName()
{
    const QDir homeDir(QDir::toNativeSeparators(vboxGlobal().homeFolder()));
    return QDir::toNativeSeparators(homeDir.absoluteFilePath(s_strCertificateFileName));
}

/* static */
int UINetworkReplyPrivateThread::abort(RTHTTP pHttp)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Call for HTTP abort: */
    return RTHttpAbort(pHttp);
}

/* static */
int UINetworkReplyPrivateThread::applyProxyRules(RTHTTP pHttp, const QString &strHostName, int iPort)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Apply HTTP proxy: */
    return RTHttpSetProxy(pHttp,
                          strHostName.toAscii().constData(),
                          iPort,
                          0 /* login */, 0 /* password */);
}

/* static */
int UINetworkReplyPrivateThread::applyRawHeaders(RTHTTP pHttp, const QList<QByteArray> &headers, const QNetworkRequest &request)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

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
    return RTHttpSetHeaders(pHttp, formattedHeaderPointers.size(), ppFormattedHeaders);
}

/* static */
int UINetworkReplyPrivateThread::performGetRequestForText(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Perform blocking HTTP GET request: */
    char *pszBuffer = 0;
    int rc = RTHttpGetText(pHttp,
                           request.url().toString().toAscii().constData(),
                           &pszBuffer);
    reply = QByteArray(pszBuffer);
    RTMemFree(pszBuffer);
    return rc;
}

/* static */
int UINetworkReplyPrivateThread::performGetRequestForBinary(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Perform blocking HTTP GET request: */
    void *pBuffer = 0;
    size_t size = 0;
    int rc = RTHttpGetBinary(pHttp,
                             request.url().toString().toAscii().constData(),
                             &pBuffer, &size);
    reply = QByteArray((const char*)pBuffer, (int)size);
    RTMemFree(pBuffer);
    return rc;
}

/**
 * Checks if the certificates we desire are all present in the given file, and
 * that the file is recent enough (not for downloaded certs).
 *
 * @returns true if fine, false if not.
 * @param   pszCaCertFile   The path to the certificate file.
 */
/* static */ bool
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
                                fFoundCerts |= RT_BIT_64(i);

                                /*
                                 * Tedium: Also mark certificates that this is an alternative to, we only need
                                 *         the public key once.
                                 */
                                uint16_t iAlt = s_aCerts[i].iAlternativeTo;
                                if (iAlt != UINT16_MAX)
                                {
                                    unsigned cMax = 10;
                                    do
                                    {
                                        Assert(iAlt < RT_ELEMENTS(s_aCerts));
                                        Assert(cMax > 1);
                                        Assert(strcmp(s_aCerts[iAlt].pszSubject, s_aCerts[i].pszSubject) == 0);
                                        fFoundCerts |= RT_BIT_64(iAlt);
                                        iAlt = s_aCerts[iAlt].iAlternativeTo;
                                    } while (iAlt != i && cMax-- > 0);
                                }
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
        AssertCompile(RT_ELEMENTS(s_aCerts) < 64);
        if (fFoundCerts == RT_BIT_64(RT_ELEMENTS(s_aCerts)) - UINT64_C(1))
            return true;
    }
    AssertRC(rc);
    return false;
}

/* static */
int UINetworkReplyPrivateThread::decompressCertificate(const QByteArray &package, QByteArray &certificate, const QString &strName)
{
    /* Decompress certificate: */
    void *pDecompressedBuffer;
    size_t cDecompressedSize;
    int rc = RTZipPkzipMemDecompress(&pDecompressedBuffer, &cDecompressedSize, package, package.size(), strName.toLatin1().constData());
    if (RT_SUCCESS(rc))
    {
        /* Copy certificate: */
        certificate = QByteArray((const char*)pDecompressedBuffer, (int)cDecompressedSize);
        /* Free decompressed buffer: */
        RTMemFree(pDecompressedBuffer);
    }
    /* Return result: */
    return rc;
}

/* static */
int UINetworkReplyPrivateThread::downloadCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName)
{
    /* Prepare certificates: */
    QByteArray certificatePca3G5;
    QByteArray certificatePca3;

    /* Receive certificate package: */
    QByteArray package;
    const QNetworkRequest address(QUrl("http://www.symantec.com/content/en/us/enterprise/verisign/roots/roots.zip"));
    int rc = performGetRequestForBinary(pHttp, address, package);
    /* UnZIP PCA-3G5 certificate: */
    if (RT_SUCCESS(rc))
    {
        rc = decompressCertificate(package, certificatePca3G5,
                                   "VeriSign Root Certificates/Generation 5 (G5) PCA/VeriSign Class 3 Public Primary Certification Authority - G5.pem");
        /* Verify PCA-3G5 certificate: */
        if (RT_SUCCESS(rc))
            rc = verifyCertificatePca3G5(pHttp, certificatePca3G5);
    }
    /* UnZIP PCA-3 certificate: */
    if (RT_SUCCESS(rc))
    {
        rc = decompressCertificate(package, certificatePca3,
                                   "VeriSign Root Certificates/Generation 1 (G1) PCAs/Class 3 Public Primary Certification Authority.pem");
        /* Verify PCA-3 certificate: */
        if (RT_SUCCESS(rc))
            rc = verifyCertificatePca3(pHttp, certificatePca3);
    }

    /* Fallback.. download certificates separately: */
    if (RT_FAILURE(rc))
    {
        /* Reset result: */
        rc = VINF_SUCCESS;
        /* Download PCA-3G5 certificate: */
        if (RT_SUCCESS(rc))
            rc = downloadCertificatePca3G5(pHttp, certificatePca3G5);
        /* Download PCA-3 certificate: */
        if (RT_SUCCESS(rc))
            rc = downloadCertificatePca3(pHttp, certificatePca3);
    }

    /* Save certificates: */
    if (RT_SUCCESS(rc))
        rc = saveDownloadedCertificates(strFullCertificateFileName, certificatePca3G5, certificatePca3);

    /* Return result-code: */
    return rc;
}

/* static */
int UINetworkReplyPrivateThread::downloadCertificatePca3G5(RTHTTP pHttp, QByteArray &certificate)
{
    /* Receive certificate: */
    const QNetworkRequest address(QUrl("http://www.verisign.com/repository/roots/root-certificates/PCA-3G5.pem"));
    int rc = performGetRequestForText(pHttp, address, certificate);

    /* Verify certificate: */
    if (RT_SUCCESS(rc))
        rc = verifyCertificatePca3G5(pHttp, certificate);

    /* Return result-code: */
    return rc;
}

/* static */
int UINetworkReplyPrivateThread::downloadCertificatePca3(RTHTTP pHttp, QByteArray &certificate)
{
    /* Receive certificate: */
    const QNetworkRequest address(QUrl("http://www.verisign.com/repository/roots/root-certificates/PCA-3.pem"));
    int rc = performGetRequestForText(pHttp, address, certificate);

    /* Verify certificate: */
    if (RT_SUCCESS(rc))
        rc = verifyCertificatePca3(pHttp, certificate);

    /* Return result-code: */
    return rc;
}

/**
 * Tries to retrieve an up to date list of certificates from the system that
 * includes the necessary certs.
 *
 * @returns IPRT status code, success indicating that we've found what we need.
 * @param   pszCaCertFile           Where to store the certificates.
 */
int UINetworkReplyPrivateThread::retrieveCertificatesFromSystem(const char *pszCaCertFile)
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
     * See if we've got the certificates we want.
     */
    if (checkCertificatesInStore(hUserStore))
        rc = RTCrStoreCertExportAsPem(hUserStore, 0 /*fFlags*/, pszCaCertFile);
    else
        rc = VERR_NOT_FOUND;
    RTCrStoreRelease(hUserStore);
    return rc;
}


/* static */
int UINetworkReplyPrivateThread::verifyCertificatePca3G5(RTHTTP pHttp, QByteArray const &certificate)
{
    const CERTINFO *pCertInfo = &s_aCerts[2];
    Assert(pCertInfo->cbEncoded == 0x4d7);
    return verifyCertificate(pHttp, certificate, pCertInfo);
}

/* static */
int UINetworkReplyPrivateThread::verifyCertificatePca3(RTHTTP pHttp, QByteArray const &certificate)
{
    const CERTINFO *pCertInfo = &s_aCerts[0];
    Assert(pCertInfo->cbEncoded == 0x240);
    Assert(pCertInfo->abSha1[0] == 0xa1);
    return verifyCertificate(pHttp, certificate, pCertInfo);
}

/* static */
int UINetworkReplyPrivateThread::verifyCertificate(RTHTTP pHttp, QByteArray const &certificate, CERTINFO const *pCertInfo)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /*
     * Convert the PEM formatted certificate into bytes and create
     * SHA-1 and SHA-512 digest for them.
     */
    uint8_t *pabSha1;
    size_t   cbSha1;
    uint8_t *pabSha512;
    size_t   cbSha512;
    int rc = RTHttpCertDigest(pHttp, (char *)certificate.constData(), certificate.size(),
                              &pabSha1, &cbSha1, &pabSha512, &cbSha512); /* Insane interface! (cbSha1, cbSha512, non-const data) Will be replaced eventually... */
    if (RT_SUCCESS(rc))
    {
        Assert(cbSha1   == RTSHA1_DIGEST_LEN);
        Assert(cbSha512 == RTSHA512_DIGEST_LEN);

        /* Verify digest: */
        if (memcmp(pCertInfo->abSha1, pabSha1, RTSHA1_DIGEST_LEN))
            rc = VERR_HTTP_CACERT_WRONG_FORMAT;
        if (memcmp(pCertInfo->abSha512, pabSha512, RTSHA512_DIGEST_LEN))
            rc = VERR_HTTP_CACERT_WRONG_FORMAT;

        /* Cleanup digest: */
        RTMemFree(pabSha1);
        RTMemFree(pabSha512);
    }

    /* Return result-code: */
    return rc;
}

/* static */ int
UINetworkReplyPrivateThread::saveDownloadedCertificates(const QString &strFullCertificateFileName,
                                                        const QByteArray &certificatePca3G5,
                                                        const QByteArray &certificatePca3)
{
    /* Open certificates file: */
    QFile file(strFullCertificateFileName);
    bool fFileOpened = file.open(QIODevice::WriteOnly);
    int rc = fFileOpened ? VINF_SUCCESS : VERR_OPEN_FAILED;

    /* Save certificates: */
    if (RT_SUCCESS(rc))
        rc = saveCertificate(file, certificatePca3G5);
    if (RT_SUCCESS(rc))
        rc = saveCertificate(file, certificatePca3);

    /* Close certificates file: */
    if (fFileOpened)
        file.close();

    /* Return result-code: */
    return rc;
}

/* static */
int UINetworkReplyPrivateThread::saveCertificate(QFile &file, const QByteArray &certificate)
{
    /* Save certificate: */
    int rc = VINF_SUCCESS;
    if (RT_SUCCESS(rc))
        rc = file.write(certificate) != -1 ? VINF_SUCCESS : VERR_WRITE_ERROR;

    /* Add 'new-line' character: */
    if (RT_SUCCESS(rc))
#ifdef Q_WS_WIN
        rc = file.write("\r\n") != -1 ? VINF_SUCCESS : VERR_WRITE_ERROR;
#else /* Q_WS_WIN */
        rc = file.write("\n") != -1 ? VINF_SUCCESS : VERR_WRITE_ERROR;
#endif /* !Q_WS_WIN */

    /* Return result-code: */
    return rc;
}


/* Our network-reply object: */
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

