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
    UINetworkReplyPrivateThread(const QNetworkRequest &request)
        : m_request(request)
        , m_iError(VINF_SUCCESS)
    {
    }

    /* API: */
    const QByteArray& readAll() const { return m_reply; }
    int error() const { return m_iError; }

private:

    /* Thread runner: */
    void run()
    {
        /* Init: */
        RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB);

        /* Create: */
        RTHTTP hHttp;
        m_iError = RTHttpCreate(&hHttp);

        /* Setup proxy: */
        UIProxyManager proxyManager(vboxGlobal().settings().proxySettings());
        if (proxyManager.proxyEnabled())
        {
            RTHttpSetProxy(hHttp,
                           proxyManager.proxyHost().toAscii().constData(),
                           proxyManager.proxyPort().toUInt(), 0, 0);
        }

        /* Acquire: */
        if (RT_SUCCESS(m_iError))
        {
            char *pszBuf = 0;
            m_iError = RTHttpGet(hHttp,
                                 m_request.url().toString().toAscii().constData(),
                                 &pszBuf);
            m_reply = QByteArray(pszBuf);
            RTMemFree(pszBuf);
        }

        /* Destroy: */
        RTHttpDestroy(hHttp);
    }

    /* Variables: */
    QNetworkRequest m_request;
    int m_iError;
    QByteArray m_reply;
};

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
        m_pThread->wait();
        delete m_pThread;
        m_pThread = 0;
    }

    /* API: Abort reply: */
    void abort()
    {
        m_error = QNetworkReply::OperationCanceledError;
        emit finished();
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
                return tr("The server has not found anything matching the URI given");
                break;
            case QNetworkReply::ContentAccessDenied:
                return tr("The request is for something forbidden, authorization will not help");
                break;
            case QNetworkReply::ProtocolFailure:
                return tr("The server did not understand the request due to bad syntax");
                break;
            default:
                return tr("Unrecognized network error");
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

