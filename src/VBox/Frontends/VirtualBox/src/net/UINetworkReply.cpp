/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkReply stuff implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UINetworkReply.h"
#include "UINetworkManager.h"
#include "VBoxGlobal.h"
#include "VBoxUtils.h"

/* Other VBox includes; */
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>

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

    /* Private API: HTTP stuff: */
    int applyProxyRules();
    int applyRawHeaders();
    int performMainRequest();

    /* Helper: Thread runner: */
    void run();

    /* Static helpers: HTTP stuff: */
    static int abort(RTHTTP pHttp);
    static int applyProxyRules(RTHTTP pHttp, const QString &strHostName, int iPort);
    static int applyRawHeaders(RTHTTP pHttp, const QList<QByteArray> &headers, const QNetworkRequest &request);
    static int performGetRequest(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply);

    /* Variables: */
    QNetworkRequest m_request;
    int m_iError;
    RTHTTP m_pHttp;
    QByteArray m_reply;
};

UINetworkReplyPrivateThread::UINetworkReplyPrivateThread(const QNetworkRequest &request)
    : m_request(request)
    , m_iError(VINF_SUCCESS)
    , m_pHttp(0)
{
}

void UINetworkReplyPrivateThread::abort()
{
    /* Call for HTTP abort: */
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
    return performGetRequest(m_pHttp, m_request, m_reply);
}

void UINetworkReplyPrivateThread::run()
{
    /* Init: */
    RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB);

    /* Create HTTP object: */
    m_iError = RTHttpCreate(&m_pHttp);

    /* Was HTTP created? */
    if (RT_SUCCESS(m_iError) && m_pHttp)
    {
        /* Simulate try-catch block: */
        do
        {
            /* Apply proxy-rules: */
            m_iError = applyProxyRules();
            if (!RT_SUCCESS(m_iError))
                break;

            /* Assign raw headers: */
            m_iError = applyRawHeaders();
            if (!RT_SUCCESS(m_iError))
                break;

            /* Perform main request: */
            m_iError = performMainRequest();
        }
        while (0);
    }

    /* Destroy HTTP object: */
    if (m_pHttp)
    {
        RTHttpDestroy(m_pHttp);
        m_pHttp = 0;
    }
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

    /* Assign HTTP proxy: */
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

    /* Assign HTTP headers: */
    return RTHttpSetHeaders(pHttp, formattedHeaderPointers.size(), ppFormattedHeaders);
}

/* static */
int UINetworkReplyPrivateThread::performGetRequest(RTHTTP pHttp, const QNetworkRequest &request, QByteArray &reply)
{
    /* Make sure HTTP is created: */
    if (!pHttp)
        return VERR_INVALID_POINTER;

    /* Perform blocking HTTP GET request: */
    char *pszBuf = 0;
    int rc = RTHttpGet(pHttp,
                       request.url().toString().toAscii().constData(),
                       &pszBuf);
    reply = QByteArray(pszBuf);
    RTMemFree(pszBuf);
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
                break;
            case QNetworkReply::ContentAccessDenied:
                return tr("Content access denied");
                break;
            case QNetworkReply::ProtocolFailure:
                return tr("Protocol failure");
                break;
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

