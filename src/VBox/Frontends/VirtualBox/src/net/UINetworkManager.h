/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkManager stuff declaration
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

#ifndef __UINetworkManager_h__
#define __UINetworkManager_h__

/* Global includes: */
#include <QNetworkAccessManager>
#include <QUuid>
#include <QMap>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>

/* Local inludes: */
#include "UINetworkDefs.h"

/* Forward declarations: */
class UINetworkManager;
class UINetworkCustomer;
class UINetworkRequestWidget;
class UINetworkManagerDialog;

/* Network request contianer: */
class UINetworkRequest : public QObject
{
    Q_OBJECT;

signals:

    /* Notifications to UINetworkManager: */
    void sigProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal);
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
    UINetworkRequest(UINetworkManager *pNetworkManager,
                     UINetworkManagerDialog *pNetworkManagerDialog,
                     const QNetworkRequest &request, UINetworkRequestType type,
                     const QString &strDescription, UINetworkCustomer *pCustomer);
    UINetworkRequest(UINetworkManager *pNetworkManager,
                     UINetworkManagerDialog *pNetworkManagerDialog,
                     const QList<QNetworkRequest> &requests, UINetworkRequestType type,
                     const QString &strDescription, UINetworkCustomer *pCustomer);
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
    UINetworkRequestWidget *m_pNetworkRequestWidget;

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

/* QNetworkAccessManager class reimplementation providing
 * network access for the VirtualBox application purposes. */
class UINetworkManager : public QNetworkAccessManager
{
    Q_OBJECT;

signals:

    /* Notification to UINetworkRequest: */
    void sigCancelNetworkRequests();

#if 0
    /* Signal to notify listeners about downloader creation: */
    void sigDownloaderCreated(UIDownloadType downloaderType);
#endif

public:

    /* Instance: */
    static UINetworkManager* instance() { return m_pInstance; }

    /* Create/destroy singleton: */
    static void create();
    static void destroy();

    /* Network Access Manager GUI window: */
    QWidget* window() const;

public slots:

    /* Show Network Access Manager GUI: */
    void show();

protected:

    /* Allow UINetworkCustomer to implicitly use next mothods: */
    friend class UINetworkCustomer;
    /* Network-request creation wrapper for UINetworkCustomer: */
    void createNetworkRequest(const QNetworkRequest &request, UINetworkRequestType type,
                              const QString &strDescription, UINetworkCustomer *pCustomer);
    /* Network request (set) creation wrapper for UINetworkCustomer: */
    void createNetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type,
                              const QString &strDescription, UINetworkCustomer *pCustomer);

private:

    /* Constructor/destructor: */
    UINetworkManager();
    ~UINetworkManager();

    /* Prepare/cleanup: */
    void prepare();
    void cleanup();

    /* Network-request prepare helper: */
    void prepareNetworkRequest(UINetworkRequest *pNetworkRequest);
    /* Network-request cleanup helper: */
    void cleanupNetworkRequest(QUuid uuid);
    /* Network-requests cleanup helper: */
    void cleanupNetworkRequests();

#if 0
    /* Downloader creation notification: */
    void notifyDownloaderCreated(UIDownloadType downloaderType);
#endif

private slots:

    /* Slot to handle network-request progress: */
    void sltHandleNetworkRequestProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal);
    /* Slot to handle network-request cancel: */
    void sltHandleNetworkRequestCancel(const QUuid &uuid);
    /* Slot to handle network-request finish: */
    void sltHandleNetworkRequestFinish(const QUuid &uuid);
    /* Slot to handle network-request failure: */
    void sltHandleNetworkRequestFailure(const QUuid &uuid, const QString &strError);

private:

    /* Instance: */
    static UINetworkManager *m_pInstance;

    /* Network-request map: */
    QMap<QUuid, UINetworkRequest*> m_requests;

    /* Network manager UI: */
    UINetworkManagerDialog *m_pNetworkProgressDialog;
};
#define gNetworkManager UINetworkManager::instance()

#endif // __UINetworkManager_h__
