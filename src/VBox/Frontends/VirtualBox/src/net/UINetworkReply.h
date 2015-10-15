/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkReply stuff declaration.
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

#ifndef ___UINetworkReply_h___
#define ___UINetworkReply_h___

/* Qt includes: */
#include <QPointer>
#include <QNetworkReply>

/* GUI includes: */
#include "UINetworkDefs.h"

/* Forward declarations: */
class UINetworkReplyPrivate;

/** QObject extension
  * used as network-reply interface. */
class UINetworkReply : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about reply progress change.
      * @param  iBytesReceived  Holds the current amount of bytes received.
      * @param  iBytesTotal     Holds the total amount of bytes to be received. */
    void downloadProgress(qint64 iBytesReceived, qint64 iBytesTotal);

    /** Notifies listeners about reply has finished processing. */
    void finished();

public:

    /** Constructs reply for the passed @a request of the passed @a type. */
    UINetworkReply(const QNetworkRequest &request, UINetworkRequestType type);
    /** Destructs reply. */
    ~UINetworkReply();

    /** Aborts reply. */
    void abort();

    /** Returns the URL of the reply. */
    QUrl url() const;

    /** Returns the last cached error of the reply. */
    QNetworkReply::NetworkError error() const;
    /** Returns the user-oriented string corresponding to the last cached error of the reply. */
    QString errorString() const;

    /** Returns binary content of the reply. */
    QByteArray readAll() const;
    /** Returns value for the cached reply header of the passed @a type. */
    QVariant header(QNetworkRequest::KnownHeaders header) const;
    /** Returns value for the cached reply attribute of the passed @a code. */
    QVariant attribute(QNetworkRequest::Attribute code) const;

private:

    /** Holds the reply private data instance. */
    UINetworkReplyPrivate *m_pReply;
};

#endif /* !___UINetworkReply_h___ */

