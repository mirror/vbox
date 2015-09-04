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
    /** @name Helpers - HTTP stuff
     * @{ */
    int applyProxyRules();
    int applyHttpsCertificates();
    int applyRawHeaders();
    int performMainRequest();
    /** @} */

    /* Helper: Main thread runner: */
    void run();

    /** Info about wanted certificate. */
    typedef struct CERTINFO
    {
        /** Gives the s_aCerts index this certificate is an alternative edition of,
         * UINT8_MAX if no alternative.  This is a complication caused by VeriSign
         * reissuing certificates signed with md2WithRSAEncryption using
         * sha1WithRSAEncryption, since MD2 is comprimised.  (Public key unmodified.)
         * It has no practical meaning for the trusted root anchor use we put it to.  */
        uint8_t     iAlternativeTo;
        /** Set if mandatory. */
        bool        fMandatory;
        /** Filename in the zip file we download (PEM). */
        const char *pszZipFile;
        /** List of direct URLs to PEM formatted files.. */
        const char *apszUrls[4];
    } CERTINFO;

    /** @name Static helpers for HTTP and Certificates handling.
     * @{ */
    static QString fullCertificateFileName();
    static int applyProxyRules(RTHTTP hHttp, const QString &strHostName, int iPort);
    static int applyRawHeaders(RTHTTP hHttp, const QList<QByteArray> &headers, const QNetworkRequest &request);
    static unsigned countCertsFound(bool const *pafFoundCerts);
    static bool areAllCertsFound(bool const *pafFoundCerts, bool fOnlyMandatory);
    static int  adjustCertsFound(int rc, bool *pafFoundCerts);
    static void refreshCertificates(RTHTTP hHttp, RTCRSTORE hOldStore, bool *pafFoundCerts, const char *pszCaCertFile);
    static void downloadMissingCertificates(RTCRSTORE hNewStore, bool *pafNewFoundCerts, RTHTTP hHttp,
                                            PRTERRINFOSTATIC pStaticErrInfo);
    static int convertVerifyAndAddPemCertificateToStore(RTCRSTORE hStore, void const *pvResponse,
                                                        size_t cbResponse, PCRTCRCERTWANTED pWantedCert);
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
    static const RTCRCERTWANTED s_aCerts[3];
    static const CERTINFO s_CertInfoPcaCls3Gen1Md2;
    static const CERTINFO s_CertInfoPcaCls3Gen1Sha1;
    static const CERTINFO s_CertInfoPcaCls3Gen5;
};

/*static*/ const UINetworkReplyPrivateThread::CERTINFO UINetworkReplyPrivateThread::s_CertInfoPcaCls3Gen1Sha1 =
{
    /*.iAlternativeTo =*/   1,
    /*.fMandatory     =*/   false,
    /*.pszZipFile     =*/
    "VeriSign Root Certificates/Generation 1 (G1) PCAs/Class 3 Public Primary Certification Authority.pem",
    /*.apszUrls[3]    =*/
    {
        "http://www.symantec.com/content/en/us/enterprise/verisign/roots/Class-3-Public-Primary-Certification-Authority.pem",
        "http://www.verisign.com/repository/roots/root-certificates/PCA-3.pem", /* dead */
        NULL,
        "http://update.virtualbox.org/cacerts-symantec-PCA-3-pem-has-gone-missing-again" /* attention getter */
    }
};

/*static*/ const UINetworkReplyPrivateThread::CERTINFO UINetworkReplyPrivateThread::s_CertInfoPcaCls3Gen1Md2 =
{
    /*.iAlternativeTo =*/   0,
    /*.fMandatory     =*/   false,
    /*.pszZipFile     =*/   NULL,
    /*.apszUrls[3]    =*/   { NULL, NULL, NULL },
};

/*static*/ const UINetworkReplyPrivateThread::CERTINFO UINetworkReplyPrivateThread::s_CertInfoPcaCls3Gen5 =
{
    /*.iAlternativeTo =*/   UINT8_MAX,
    /*.fMandatory     =*/   true,
    /*.pszZipFile     =*/
    "VeriSign Root Certificates/Generation 5 (G5) PCA/VeriSign Class 3 Public Primary Certification Authority - G5.pem",
    /*.apszUrls[3]    =*/
    {
        "http://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class%203-Public-Primary-Certification-Authority-G5.pem",
        "http://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class-3-Public-Primary-Certification-Authority-G5.pem", /* (in case they correct above typo) */
        "http://www.verisign.com/repository/roots/root-certificates/PCA-3G5.pem", /* dead */
        "http://update.virtualbox.org/cacerts-symantec-PCA-3G5-pem-has-gone-missing-again" /* attention getter */
    }
};


/**
 * Details on the certificates we are after.
 * The pvUser member points to a UINetworkReplyPrivateThread::CERTINFO.
 */
/* static */ const RTCRCERTWANTED UINetworkReplyPrivateThread::s_aCerts[3] =
{
    /*[0] =*/   /* The reissued version with the SHA-1 signature. */
/** @todo r=bird: Why do we need this certificate? Neither update.virtualbox.org nor www.virtualbox.org uses it...  ElCapitan doesn't ship this. */
    {
        /*.pszSubject        =*/    "C=US, O=VeriSign, Inc., OU=Class 3 Public Primary Certification Authority",
        /*.cbEncoded         =*/    0x240,
        /*.Sha1Fingerprint   =*/    true,
        /*.Sha512Fingerprint =*/    true,
        /*.abSha1            =*/
        {
            0xa1, 0xdb, 0x63, 0x93, 0x91, 0x6f, 0x17, 0xe4, 0x18, 0x55,
            0x09, 0x40, 0x04, 0x15, 0xc7, 0x02, 0x40, 0xb0, 0xae, 0x6b
        },
        /*.abSha512          =*/
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
        /*.pvUser */ &UINetworkReplyPrivateThread::s_CertInfoPcaCls3Gen1Sha1
    },
    /*[1] =*/   /* The original version with the MD2 signature. */
    {
        /*.pszSubject        =*/    "C=US, O=VeriSign, Inc., OU=Class 3 Public Primary Certification Authority",
        /*.cbEncoded         =*/    0x240,
        /*.Sha1Fingerprint   =*/    true,
        /*.Sha512Fingerprint =*/    true,
        /*.abSha1            =*/
        {
            0x74, 0x2c, 0x31, 0x92, 0xe6, 0x07, 0xe4, 0x24, 0xeb, 0x45,
            0x49, 0x54, 0x2b, 0xe1, 0xbb, 0xc5, 0x3e, 0x61, 0x74, 0xe2
        },
        /*.abSha512          =*/
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
        /*.pvUser */ &UINetworkReplyPrivateThread::s_CertInfoPcaCls3Gen1Md2
    },
    /*[2] =*/
    {
        /*.pszSubject        =*/
        "C=US, O=VeriSign, Inc., OU=VeriSign Trust Network, OU=(c) 2006 VeriSign, Inc. - For authorized use only, "
        "CN=VeriSign Class 3 Public Primary Certification Authority - G5",
        /*.cbEncoded         =*/    0x4d7,
        /*.Sha1Fingerprint   =*/    true,
        /*.Sha512Fingerprint =*/    true,
        /*.abSha1            =*/
        {
            0x4e, 0xb6, 0xd5, 0x78, 0x49, 0x9b, 0x1c, 0xcf, 0x5f, 0x58,
            0x1e, 0xad, 0x56, 0xbe, 0x3d, 0x9b, 0x67, 0x44, 0xa5, 0xe5
        },
        /*.abSha512          =*/
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
        /*.pvUser */ &UINetworkReplyPrivateThread::s_CertInfoPcaCls3Gen5
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
     * Check the state of our CA certificate file, it's one of the following:
     *      - Missing, recreate from scratch (= refresh).
     *      - Everything is there and it is less than 28 days old, do nothing.
     *      - Everything is there but it's older than 28 days, refresh.
     *      - Missing certificates and is older than 1 min, refresh.
     *
     * Start by creating a store for loading the current state into, as we'll
     * be need that for the refresh.
     */
    RTCRSTORE hCurStore = NIL_RTCRSTORE;
    int rc = RTCrStoreCreateInMem(&hCurStore, 256);
    if (RT_SUCCESS(rc))
    {
        bool fRefresh    = true;
        bool afCertsFound[RT_ELEMENTS(s_aCerts)];
        RT_ZERO(afCertsFound);

        /*
         * Load the file if it exists.
         *
         * To effect regular updates, we need the modification date of the file,
         * so we use RTPathQueryInfoEx here and not RTFileExists.
         */
        RTFSOBJINFO Info;
        int rc = RTPathQueryInfoEx(pszCaCertFile, &Info, RTFSOBJATTRADD_NOTHING, RTPATH_F_FOLLOW_LINK);
        if (   RT_SUCCESS(rc)
            && RTFS_IS_FILE(Info.Attr.fMode))
        {
            RTERRINFOSTATIC StaticErrInfo;
            rc = RTCrStoreCertAddFromFile(hCurStore, RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR, pszCaCertFile,
                                          RTErrInfoInitStatic(&StaticErrInfo));
            if (RTErrInfoIsSet(&StaticErrInfo.Core))
                LogRel(("checkCertificates: %s\n", StaticErrInfo.Core.pszMsg));
            else
                AssertRC(rc);

            /*
             * Scan the store the for certificates we need, then see what we
             * need to do wrt file age.
             */
            rc = RTCrStoreCertCheckWanted(hCurStore, s_aCerts, RT_ELEMENTS(s_aCerts), afCertsFound);
            rc = adjustCertsFound(rc, afCertsFound);
            AssertRC(rc);
            RTTIMESPEC RefreshAge;
            uint32_t   cSecRefresh = rc == VINF_SUCCESS  ? 28 * RT_SEC_1DAY /* all found */ : 60 /* stuff missing */;
            fRefresh = RTTimeSpecCompare(&Info.ModificationTime, RTTimeSpecSubSeconds(RTTimeNow(&RefreshAge), cSecRefresh)) <= 0;
        }

        /*
         * Refresh the file if necessary.
         */
        if (fRefresh)
            refreshCertificates(m_hHttp, hCurStore, afCertsFound, pszCaCertFile);

        RTCrStoreRelease(hCurStore);

        /*
         * Final verdict.
         */
        if (areAllCertsFound(afCertsFound, true /*fOnlyMandatory*/))
            rc = VINF_SUCCESS;
        else
            rc = VERR_NOT_FOUND; /** @todo r=bird: Why not try and let RTHttpGet* bitch if the necessary certs are missing? */

        /*
         * Set our custom CA file.
         */
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
 * Adjusts the set of found certificates by marking all alternatives found if
 * one is.
 *
 * @returns Adjusted rc (VINF_SUCCESS instead of VWRN_NOT_FOUND if all found).
 * @param   rc                  The status code.
 * @param   pafFoundCerts       Array parallel to s_aCerts with the status of
 *                              each wanted certificate.
 */
/*static*/ int
UINetworkReplyPrivateThread::adjustCertsFound(int rc, bool *pafFoundCerts)
{
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
        if (pafFoundCerts[i])
        {
            uint8_t iAlt = i;
            for (;;)
            {
                const CERTINFO *pCertInfo = (const CERTINFO *)s_aCerts[iAlt].pvUser;
                iAlt = pCertInfo->iAlternativeTo;
                if (iAlt >= RT_ELEMENTS(s_aCerts) || iAlt == i)
                {
                    Assert(iAlt == UINT8_MAX || iAlt < RT_ELEMENTS(s_aCerts));
                    break;
                }
                if (!pafFoundCerts[iAlt])
                    pafFoundCerts[iAlt] = true;
            }
        }

    if (rc == VINF_SUCCESS || rc == VWRN_NOT_FOUND)
        rc = countCertsFound(pafFoundCerts) == RT_ELEMENTS(s_aCerts) ? VINF_SUCCESS : VWRN_NOT_FOUND;
    return rc;
}

/**
 * Counts the number of certificates found in a search result array.
 *
 * @returns Number of wanted certifcates we've found.
 * @param   pafFoundCerts       Array parallel to s_aCerts with the status of
 *                              each wanted certificate.
 */
/*static*/ unsigned
UINetworkReplyPrivateThread::countCertsFound(bool const *pafFoundCerts)
{
    unsigned cFound = 0;
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
        cFound += pafFoundCerts[i];
    return cFound;
}

/**
 * Checks if we've found all the necessary certificates or not.
 *
 * @returns true if we have, false if we haven't.
 * @param   pafFoundCerts       Array parallel to s_aCerts with the status of
 *                              each wanted certificate.
 * @param   fOnlyMandatory      Only require mandatory certificates to be
 *                              present.  If false, all certificates must be
 *                              found before we return true.
 */
/*static*/ bool
UINetworkReplyPrivateThread::areAllCertsFound(bool const *pafFoundCerts, bool fOnlyMandatory)
{
    if (fOnlyMandatory)
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
            if (   !pafFoundCerts[i]
                && ((const CERTINFO *)s_aCerts[i].pvUser)->fMandatory)
                return false;
    }
    else
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
            if (!pafFoundCerts[i])
                return false;
    return true;
}

/*static*/ void
UINetworkReplyPrivateThread::refreshCertificates(RTHTTP hHttp, RTCRSTORE hOldStore, bool *pafOldFoundCerts,
                                                 const char *pszCaCertFile)
{
    /*
     * Collect the standard assortment of SSL certificates.
     */
    uint32_t  cHint = RTCrStoreCertCount(hOldStore);
    RTCRSTORE hNewStore;
    int rc = RTCrStoreCreateInMem(&hNewStore, cHint > 32 && cHint < _32K ? cHint + 16 : 256);
    if (RT_SUCCESS(rc))
    {
        RTERRINFOSTATIC StaticErrInfo;
        rc = RTHttpGatherCaCertsInStore(hNewStore, 0 /*fFlags*/, RTErrInfoInitStatic(&StaticErrInfo));
        if (RTErrInfoIsSet(&StaticErrInfo.Core))
            LogRel(("refreshCertificates/#1: %s\n", StaticErrInfo.Core.pszMsg));
        else if (rc == VERR_NOT_FOUND)
            LogRel(("refreshCertificates/#1: No trusted SSL certs found on the system, will try download...\n"));
        else
            AssertLogRelRC(rc);
        if (RT_SUCCESS(rc) || rc == VERR_NOT_FOUND)
        {
            /*
             * Check and see what we've got.  If we haven't got all we desire,
             * try add it from the previous store.
             */
            bool afNewFoundCerts[RT_ELEMENTS(s_aCerts)];
            RT_ZERO(afNewFoundCerts); /* paranoia */

            rc = RTCrStoreCertCheckWanted(hNewStore, s_aCerts, RT_ELEMENTS(s_aCerts), afNewFoundCerts);
            rc = adjustCertsFound(rc, afNewFoundCerts);
            AssertLogRelRC(rc);
            Assert(rc != VINF_SUCCESS || areAllCertsFound(afNewFoundCerts, false /*fOnlyMandatory*/));
            if (rc != VINF_SUCCESS)
            {
                rc = RTCrStoreCertAddWantedFromStore(hNewStore,
                                                     RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR,
                                                     hOldStore, s_aCerts, RT_ELEMENTS(s_aCerts), afNewFoundCerts);
                rc = adjustCertsFound(rc, afNewFoundCerts);
                AssertLogRelRC(rc);
                Assert(rc != VINF_SUCCESS || areAllCertsFound(afNewFoundCerts, false /*fOnlyMandatory*/));
            }

            /*
             * If that didn't help, seek out certificates in more obscure places,
             * like java, mozilla and mutt.
             */
            if (rc != VINF_SUCCESS)
            {
                rc = RTCrStoreCertAddWantedFromFishingExpedition(hNewStore,
                                                                 RTCRCERTCTX_F_ADD_IF_NOT_FOUND
                                                                 | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR,
                                                                 s_aCerts, RT_ELEMENTS(s_aCerts), afNewFoundCerts,
                                                                 RTErrInfoInitStatic(&StaticErrInfo));
                rc = adjustCertsFound(rc, afNewFoundCerts);
                if (RTErrInfoIsSet(&StaticErrInfo.Core))
                    LogRel(("refreshCertificates/#2: %s\n", StaticErrInfo.Core.pszMsg));
                Assert(rc != VINF_SUCCESS || areAllCertsFound(afNewFoundCerts, false /*fOnlyMandatory*/));
            }

            /*
             * If that didn't help, try download the certificates.
             */
            if (rc != VINF_SUCCESS)
                downloadMissingCertificates(hNewStore, afNewFoundCerts, hHttp, &StaticErrInfo);

            /*
             * If we've got the same or better hit rate than the old store,
             * replace the CA certs file.
             */
            if (   areAllCertsFound(afNewFoundCerts, false /*fOnlyMandatory*/)
                || (   countCertsFound(afNewFoundCerts) >= countCertsFound(pafOldFoundCerts)
                    &&    areAllCertsFound(afNewFoundCerts, true /*fOnlyMandatory*/)
                       >= areAllCertsFound(pafOldFoundCerts, true /*fOnlyMandatory*/) ) )
            {
                rc = RTCrStoreCertExportAsPem(hNewStore, 0 /*fFlags*/, pszCaCertFile);
                if (RT_SUCCESS(rc))
                {
                    memcpy(pafOldFoundCerts, afNewFoundCerts, sizeof(afNewFoundCerts));
                    LogRel(("refreshCertificates/#3: Found %u/%u SSL certs we/you trust (previously %u/%u).\n",
                            countCertsFound(afNewFoundCerts), RTCrStoreCertCount(hNewStore),
                            countCertsFound(pafOldFoundCerts), RTCrStoreCertCount(hOldStore) ));
                }
                else
                {
                    RT_ZERO(pafOldFoundCerts);
                    LogRel(("refreshCertificates/#3: RTCrStoreCertExportAsPem unexpectedly failed with %Rrc\n", rc));
                }
            }
            else
                LogRel(("refreshCertificates/#3: Sticking with the old file, missing essential certs.\n"));
        }
        RTCrStoreRelease(hNewStore);
    }
}

/*static*/ void
UINetworkReplyPrivateThread::downloadMissingCertificates(RTCRSTORE hNewStore, bool *pafNewFoundCerts, RTHTTP hHttp,
                                                         PRTERRINFOSTATIC pStaticErrInfo)
{
    int rc;

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
                if (!pafNewFoundCerts[i])
                {
                    CERTINFO const *pInfo = (CERTINFO const *)s_aCerts[i].pvUser;
                    if (pInfo->pszZipFile)
                    {
                        void  *pvFile;
                        size_t cbFile;
                        rc = RTZipPkzipMemDecompress(&pvFile, &cbFile, pvRootsZip, cbRootsZip, pInfo->pszZipFile);
                        if (RT_SUCCESS(rc))
                        {
                            rc = convertVerifyAndAddPemCertificateToStore(hNewStore, pvFile, cbFile, &s_aCerts[i]);
                            RTMemFree(pvFile);
                            if (RT_SUCCESS(rc))
                            {
                                /*
                                 * Successfully added. Mark it as found and return if we've got them all.
                                 */
                                pafNewFoundCerts[i] = true;
                                if (adjustCertsFound(VWRN_NOT_FOUND, pafNewFoundCerts) == VINF_SUCCESS)
                                {
                                    RTHttpFreeResponse(pvRootsZip);
                                    return;
                                }
                            }
                        }
                    }
                }
            RTHttpFreeResponse(pvRootsZip);
        }
    }

    /*
     * Try download certificates separately.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aCerts); i++)
        if (!pafNewFoundCerts[i])
        {
            CERTINFO const *pInfo = (CERTINFO const *)s_aCerts[i].pvUser;
            for (uint32_t iUrl = 0; iUrl < RT_ELEMENTS(pInfo->apszUrls); i++)
                if (pInfo->apszUrls[iUrl])
                {
                    void  *pvResponse;
                    size_t cbResponse;
                    rc = RTHttpGetBinary(hHttp, pInfo->apszUrls[iUrl], &pvResponse, &cbResponse);
                    if (RT_SUCCESS(rc))
                    {
                        rc = convertVerifyAndAddPemCertificateToStore(hNewStore, pvResponse, cbResponse, &s_aCerts[i]);
                        RTHttpFreeResponse(pvResponse);
                        if (RT_SUCCESS(rc))
                        {
                            pafNewFoundCerts[i] = true;
                            adjustCertsFound(VWRN_NOT_FOUND, pafNewFoundCerts);
                            break;
                        }
                    }
                }
        }
}

/**
 * Converts a PEM certificate, verifies it against @a pCertInfo and adds it to
 * the given store.
 *
 * @returns IPRT status code.
 * @param   hStore              The store to add it to.
 * @param   pvResponse          The raw PEM certificate file bytes.
 * @param   cbResponse          The number of bytes.
 * @param   pWantedCert         The certificate info (we use hashes and encoded
 *                              size).
 */
/*static*/ int
UINetworkReplyPrivateThread::convertVerifyAndAddPemCertificateToStore(RTCRSTORE hStore,
                                                                      void const *pvResponse, size_t cbResponse,
                                                                      PCRTCRCERTWANTED pWantedCert)
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
            if (pCur->cbData == pWantedCert->cbEncoded)
            {
                if (   RTSha1Check(pCur->pbData, pCur->cbData, pWantedCert->abSha1)
                    && RTSha512Check(pCur->pbData, pCur->cbData, pWantedCert->abSha512))
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
                        pCur->cbData, pWantedCert->cbEncoded));

        RTCrPemFreeSections(pSectionHead);
    }
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

