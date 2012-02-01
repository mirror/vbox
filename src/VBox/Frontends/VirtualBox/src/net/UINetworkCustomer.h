/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINetworkCustomer class declaration
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

#ifndef __UINetworkCustomer_h__
#define __UINetworkCustomer_h__

/* Global includes: */
#include <QObject>

/* Local includes: */
#include "UINetworkDefs.h"

/* Forward declarations: */
class QNetworkReply;
class QNetworkRequest;

/* Inheritable interface allowing to access UINetworkManager hidden functionality: */
class UINetworkCustomer : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UINetworkCustomer(QObject *pParent = 0);

    /* Network reply progress handler: */
    virtual void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal) = 0;
    /* Network reply cancel handler: */
    virtual void processNetworkReplyCanceled(QNetworkReply *pReply) = 0;
    /* Network reply finish handler: */
    virtual void processNetworkReplyFinished(QNetworkReply *pReply) = 0;

protected:

    /* Network request wrapper: */
    void createNetworkRequest(const QNetworkRequest &request, UINetworkRequestType type, const QString &strDescription);
    /* Network request wrapper (set): */
    void createNetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type, const QString &strDescription);

#if 0
    /* Downloader creation notification wrapper: */
    void notifyDownloaderCreated(UIDownloadType downloaderType);
#endif
};

#endif // __UINetworkCustomer_h__
