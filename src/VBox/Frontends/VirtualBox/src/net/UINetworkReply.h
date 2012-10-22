/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkReply stuff declaration
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

#ifndef __UINetworkReply_h__
#define __UINetworkReply_h__

/* Qt includes: */
#include <QPointer>
#include <QThread>
#include <QNetworkReply>

/* GUI includes: */
#include "UINetworkDefs.h"

/* Forward declarations: */
class UINetworkReplyPrivateThread;

/* Network-reply interface: */
class UINetworkReply : public QObject
{
    Q_OBJECT;

signals:

    /* Notifiers: */
    void downloadProgress(qint64 iBytesReceived, qint64 iBytesTotal);
    void finished();

public:

    /* Constructor/destructor: */
    UINetworkReply(const QNetworkRequest &request, UINetworkRequestType requestType);
    ~UINetworkReply();

    /* API: */
    QVariant header(QNetworkRequest::KnownHeaders header) const;
    QVariant attribute(QNetworkRequest::Attribute code) const;
    void abort();
    QNetworkReply::NetworkError error() const;
    QString errorString() const;
    QByteArray readAll();
    QUrl url() const;

private:

    /* Variables: */
    UINetworkReplyType m_replyType;
    QPointer<QObject> m_pReply;
};

#endif // __UINetworkReply_h__
