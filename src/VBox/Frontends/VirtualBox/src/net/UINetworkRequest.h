/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkRequest stuff declaration
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UINetworkRequest_h__
#define __UINetworkRequest_h__

/* Global includes: */
#include <QUuid>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>

/* Local inludes: */
#include "UINetworkDefs.h"

/* Forward declarations: */
class UINetworkManager;
class UINetworkManagerDialog;
class UINetworkRequestWidget;
class UINetworkCustomer;

/* Network-request contianer: */
class UINetworkRequest : public QObject
{
    Q_OBJECT;

signals:

    /* Notifications to UINetworkManager: */
    void sigProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal);
    void sigStarted(const QUuid &uuid);
    void sigCanceled(const QUuid &uuid);
    void sigFinished(const QUuid &uuid);
    void sigFailed(const QUuid &uuid, const QString &strError);

    /* Notifications to UINetworkRequestWidget: */
    void sigProgress(qint64 iReceived, qint64 iTotal);
    void sigStarted();
    void sigFinished();
    void sigFailed(const QString &strError);

public:

    /* Constructor/destructor: */
    UINetworkRequest(const QNetworkRequest &request, UINetworkRequestType type, const QString &strDescription,
                     UINetworkCustomer *pCustomer,
                     UINetworkManager *pNetworkManager);
    UINetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type, const QString &strDescription,
                     UINetworkCustomer *pCustomer,
                     UINetworkManager *pNetworkManager);
    ~UINetworkRequest();

    /* Getters: */
    const QUuid& uuid() const { return m_uuid; }
    const QString& description() const { return m_strDescription; }
    UINetworkCustomer* customer() { return m_pCustomer; }
    QNetworkReply* reply() { return m_pReply; }

private slots:

    /* Network-reply progress handler: */
    void sltHandleNetworkReplyProgress(qint64 iReceived, qint64 iTotal);
    /* Network-reply finish handler: */
    void sltHandleNetworkReplyFinish();

    /* Slot to retry network-request: */
    void sltRetry();
    /* Slot to cancel network-request: */
    void sltCancel();

private:

    /* Initialize: */
    void initialize();

    /* Prepare network-reply: */
    void prepareNetworkReply();
    /* Cleanup network-reply: */
    void cleanupNetworkReply();
    /* Abort network-reply: */
    void abortNetworkReply();

    /* Widgets: */
    UINetworkManagerDialog *m_pNetworkManagerDialog;

    /* Variables: */
    QUuid m_uuid;
    QList<QNetworkRequest> m_requests;
    QNetworkRequest m_request;
    int m_iCurrentRequestIndex;
    UINetworkRequestType m_type;
    QString m_strDescription;
    UINetworkCustomer *m_pCustomer;
    QPointer<QNetworkReply> m_pReply;
    bool m_fRunning;
};

#endif // __UINetworkRequest_h__
