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

/* Qt includes: */
#include <QDir>
#include <QFile>
#include <QThread>
#include <QRegExp>

/* GUI includes: */
#include "UINetworkReply.h"
#include "UINetworkManager.h"
#include "VBoxGlobal.h"
#include "VBoxUtils.h"

/* Other VBox includes; */
#include <iprt/initterm.h>
#include <iprt/http.h>
#include <iprt/err.h>
#include <iprt/zip.h>

/* Our network-reply thread: */
class UINetworkReplyPrivateThread : public QThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UINetworkReplyPrivateThread(const QNetworkRequest &request);

    /* API: Read stuff: */
    const QByteArray& readAll() const { return m_reply; }

    /* API: Error stuff: */
    int error() const { return m_iError; }

    /* API: HTTP stuff: */
    void abort();

private:

    /* Helpers: HTTP stuff: */
    int applyProxyRules();
    int applyHttpsCertificates();
    int applyRawHeaders();
    int performMainRequest();

    /* Helper: Main thread runner: */
    void run();

    /* Static helper: File stuff: */
    static QString fullCertificateFileName();

    /* Static helpers: HTTP stuff: */
    static int abort(RTHTTP pHttp);
    static int applyProxyRules(RTHTTP pHttp, const QString &strHostName, int iPort);
    static int applyCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName);
    static int applyRawHeaders(RTHTTP pHttp, const QList<QByteArray> &headers, const QNetworkRequest &request);
    static int performGetRequestForText(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply);
    static int performGetRequestForBinary(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply);
    static int checkCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName);
    static int decompressCertificate(const QByteArray &package, QByteArray &certificate, const QString &strName);
    static int downloadCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName);
    static int downloadCertificatePca3G5(RTHTTP pHttp, QByteArray &certificate);
    static int downloadCertificatePca3(RTHTTP pHttp, QByteArray &certificate);
    static int verifyCertificatePca3G5(RTHTTP pHttp, QByteArray &certificate);
    static int verifyCertificatePca3(RTHTTP pHttp, QByteArray &certificate);
    static int verifyCertificate(RTHTTP pHttp, QByteArray &certificate, const QByteArray &sha1, const QByteArray &sha512);
    static int saveCertificates(const QString &strFullCertificateFileName, const QByteArray &certificatePca3G5, const QByteArray &certificatePca3);
    static int saveCertificate(QFile &file, const QByteArray &certificate);

    /* Variables: */
    QNetworkRequest m_request;
    int m_iError;
    RTHTTP m_pHttp;
    QByteArray m_reply;
    static const QString m_strCertificateFileName;
};

/* static */
const QString UINetworkReplyPrivateThread::m_strCertificateFileName = QString("vbox-ssl-cacertificate.crt");

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
    /* Prepare variables: */
    const QString strFullCertificateFileName(fullCertificateFileName());
    int rc = VINF_SUCCESS;

    /* Check certificates if present: */
    if (QFile::exists(strFullCertificateFileName))
        rc = checkCertificates(m_pHttp, strFullCertificateFileName);
    else
        rc = VERR_FILE_NOT_FOUND;

    /* Download certificates if necessary: */
    if (!RT_SUCCESS(rc))
        rc = downloadCertificates(m_pHttp, strFullCertificateFileName);

    /* Apply certificates: */
    if (RT_SUCCESS(rc))
        rc = applyCertificates(m_pHttp, strFullCertificateFileName);

    /* Return result-code: */
    return rc;
}

int UINetworkReplyPrivateThread::applyRawHeaders()
{
    /* Make sure we have a raw headers at all: */
    QList<QByteArray> headers = m_request.rawHeaderList();
    if (headers.isEmpty())
        return VINF_SUCCESS;

    /* Apply raw headers: */
    return applyRawHeaders(m_pHttp, headers, m_request);
}

int UINetworkReplyPrivateThread::performMainRequest()
{
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
    const QDir homeDir(QDir::toNativeSeparators(vboxGlobal().virtualBox().GetHomeFolder()));
    return QDir::toNativeSeparators(homeDir.absoluteFilePath(m_strCertificateFileName));
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
int UINetworkReplyPrivateThread::applyCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Apply HTTPs certificates: */
    return RTHttpSetCAFile(pHttp, strFullCertificateFileName.toAscii().constData());
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

/* static */
int UINetworkReplyPrivateThread::checkCertificates(RTHTTP pHttp, const QString &strFullCertificateFileName)
{
    /* Open certificates file: */
    QFile file(strFullCertificateFileName);
    bool fFileOpened = file.open(QIODevice::ReadOnly);
    int rc = fFileOpened ? VINF_SUCCESS : VERR_OPEN_FAILED;

    /* Read certificates file: */
    if (RT_SUCCESS(rc))
    {
        /* Parse the file content: */
        QString strData(file.readAll());
#define CERT   "-{5}BEGIN CERTIFICATE-{5}[\\s\\S\\r{0,1}\\n]+-{5}END CERTIFICATE-{5}"
#define REOLD  "(" CERT ")\\r{0,1}\\n(" CERT ")\\r{0,1}\\n(" CERT ")"
#define RENEW  "(" CERT ")\\r{0,1}\\n(" CERT ")"
        /* First check if we have the old format with three certificates: */
        QRegExp regExp(REOLD);
        regExp.setMinimal(true);

        /* If so, fake an error to force re-downloading */
        if (regExp.indexIn(strData) != -1)
            rc = VERR_HTTP_CACERT_WRONG_FORMAT;

        /* Otherwise, check for two certificates: */
        if (RT_SUCCESS(rc))
        {
            regExp.setPattern(RENEW);
            regExp.setMinimal(true);
            if (regExp.indexIn(strData) == -1)
                rc = VERR_FILE_IO_ERROR;
        }

        /* Verify certificates: */
        if (RT_SUCCESS(rc))
        {
            QByteArray certificate = regExp.cap(1).toAscii();
            rc = verifyCertificatePca3G5(pHttp, certificate);
        }
        if (RT_SUCCESS(rc))
        {
            QByteArray certificate = regExp.cap(2).toAscii();
            rc = verifyCertificatePca3(pHttp, certificate);
        }
#undef CERT
#undef REOLD
#undef RENEW
    }

    /* Close certificates file: */
    if (fFileOpened)
        file.close();

    /* Return result-code: */
    return rc;
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
    const QNetworkRequest address(QUrl("http://www.verisign.com/support/roots.zip"));
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
    if (!RT_SUCCESS(rc))
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
        saveCertificates(strFullCertificateFileName, certificatePca3G5, certificatePca3);

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

/* static */
int UINetworkReplyPrivateThread::verifyCertificatePca3G5(RTHTTP pHttp, QByteArray &certificate)
{
    /* PCA 3G5 secure hash algorithm 1: */
    const unsigned char baSha1PCA3G5[] =
    {
        0x4e, 0xb6, 0xd5, 0x78, 0x49, 0x9b, 0x1c, 0xcf, 0x5f, 0x58,
        0x1e, 0xad, 0x56, 0xbe, 0x3d, 0x9b, 0x67, 0x44, 0xa5, 0xe5
    };
    /* PCA 3G5 secure hash algorithm 512: */
    const unsigned char baSha512PCA3G5[] =
    {
        0xd4, 0xf8, 0x10, 0x54, 0x72, 0x77, 0x0a, 0x2d,
        0xe3, 0x17, 0xb3, 0xcf, 0xed, 0x61, 0xae, 0x5c,
        0x5d, 0x3e, 0xde, 0xa1, 0x41, 0x35, 0xb2, 0xdf,
        0x60, 0xe2, 0x61, 0xfe, 0x3a, 0xc1, 0x66, 0xa3,
        0x3c, 0x88, 0x54, 0x04, 0x4f, 0x1d, 0x13, 0x46,
        0xe3, 0x8c, 0x06, 0x92, 0x9d, 0x70, 0x54, 0xc3,
        0x44, 0xeb, 0x2c, 0x74, 0x25, 0x9e, 0x5d, 0xfb,
        0xd2, 0x6b, 0xa8, 0x9a, 0xf0, 0xb3, 0x6a, 0x01
    };
    QByteArray pca3G5sha1 = QByteArray::fromRawData((const char *)baSha1PCA3G5, sizeof(baSha1PCA3G5));
    QByteArray pca3G5sha512 = QByteArray::fromRawData((const char *)baSha512PCA3G5, sizeof(baSha512PCA3G5));

    /* Verify certificate: */
    return verifyCertificate(pHttp, certificate, pca3G5sha1, pca3G5sha512);
}

/* static */
int UINetworkReplyPrivateThread::verifyCertificatePca3(RTHTTP pHttp, QByteArray &certificate)
{
    /* PCA 3 secure hash algorithm 1: */
    const unsigned char baSha1PCA3[] =
    {
        0xa1, 0xdb, 0x63, 0x93, 0x91, 0x6f, 0x17, 0xe4, 0x18, 0x55,
        0x09, 0x40, 0x04, 0x15, 0xc7, 0x02, 0x40, 0xb0, 0xae, 0x6b
    };
    /* PCA 3 secure hash algorithm 512: */
    const unsigned char baSha512PCA3[] =
    {
        0xbb, 0xf7, 0x8a, 0x19, 0x9f, 0x37, 0xee, 0xa2,
        0xce, 0xc8, 0xaf, 0xe3, 0xd6, 0x22, 0x54, 0x20,
        0x74, 0x67, 0x6e, 0xa5, 0x19, 0xb7, 0x62, 0x1e,
        0xc1, 0x2f, 0xd5, 0x08, 0xf4, 0x64, 0xc4, 0xc6,
        0xbb, 0xc2, 0xf2, 0x35, 0xe7, 0xbe, 0x32, 0x0b,
        0xde, 0xb2, 0xfc, 0x44, 0x92, 0x5b, 0x8b, 0x9b,
        0x77, 0xa5, 0x40, 0x22, 0x18, 0x12, 0xcb, 0x3d,
        0x0a, 0x67, 0x83, 0x87, 0xc5, 0x45, 0xc4, 0x99
    };
    QByteArray pca3sha1 = QByteArray::fromRawData((const char *)baSha1PCA3, sizeof(baSha1PCA3));
    QByteArray pca3sha512 = QByteArray::fromRawData((const char *)baSha512PCA3, sizeof(baSha512PCA3));

    /* Verify certificate: */
    return verifyCertificate(pHttp, certificate, pca3sha1, pca3sha512);
}

/* static */
int UINetworkReplyPrivateThread::verifyCertificate(RTHTTP pHttp, QByteArray &certificate, const QByteArray &sha1, const QByteArray &sha512)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Create digest: */
    uint8_t *abSha1;
    size_t  cbSha1;
    uint8_t *abSha512;
    size_t  cbSha512;
    int rc = RTHttpCertDigest(pHttp, certificate.data(), certificate.size(),
                              &abSha1, &cbSha1, &abSha512, &cbSha512);

    /* Verify digest: */
    if (cbSha1 != (size_t)sha1.size())
        rc = VERR_HTTP_CACERT_WRONG_FORMAT;
    else if (memcmp(sha1.constData(), abSha1, cbSha1))
        rc = VERR_HTTP_CACERT_WRONG_FORMAT;
    if (cbSha512 != (size_t)sha512.size())
        rc = VERR_HTTP_CACERT_WRONG_FORMAT;
    else if (memcmp(sha512.constData(), abSha512, cbSha512))
        rc = VERR_HTTP_CACERT_WRONG_FORMAT;

    /* Cleanup digest: */
    RTMemFree(abSha1);
    RTMemFree(abSha512);

    /* Return result-code: */
    return rc;
}

/* static */
int UINetworkReplyPrivateThread::saveCertificates(const QString &strFullCertificateFileName,
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
            case QNetworkReply::NoError:
                break;
            case QNetworkReply::HostNotFoundError:
                return tr("Host not found");
            case QNetworkReply::ContentAccessDenied:
                return tr("Content access denied");
            case QNetworkReply::ProtocolFailure:
                return tr("Protocol failure");
            case QNetworkReply::AuthenticationRequiredError:
                return tr("Wrong SSL certificate format");
            case QNetworkReply::SslHandshakeFailedError:
                return tr("SSL authentication failed");
            default:
                return tr("Unknown reason");
                break;
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
            case VINF_SUCCESS:
                m_error = QNetworkReply::NoError;
                break;
            case VERR_HTTP_ABORTED:
                m_error = QNetworkReply::OperationCanceledError;
                break;
            case VERR_HTTP_NOT_FOUND:
                m_error = QNetworkReply::HostNotFoundError;
                break;
            case VERR_HTTP_ACCESS_DENIED:
                m_error = QNetworkReply::ContentAccessDenied;
                break;
            case VERR_HTTP_BAD_REQUEST:
                m_error = QNetworkReply::ProtocolFailure;
                break;
            case VERR_HTTP_CACERT_WRONG_FORMAT:
                m_error = QNetworkReply::AuthenticationRequiredError;
                break;
            case VERR_HTTP_CACERT_CANNOT_AUTHENTICATE:
                m_error = QNetworkReply::SslHandshakeFailedError;
                break;
            default:
                m_error = QNetworkReply::UnknownNetworkError;
                break;
        }
        emit finished();
    }

private:

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

