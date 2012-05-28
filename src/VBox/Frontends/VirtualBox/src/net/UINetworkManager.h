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

/* Local inludes: */
#include "UINetworkDefs.h"

/* Forward declarations: */
class QWidget;
class UINetworkRequest;
class UINetworkCustomer;
class UINetworkManagerDialog;

/* QNetworkAccessManager class reimplementation.
 * Providing network access for VirtualBox application purposes. */
class UINetworkManager : public QNetworkAccessManager
{
    Q_OBJECT;

signals:

    /* Ask listeners (network-requests) to cancel: */
    void sigCancelNetworkRequests();

public:

    /* Instance: */
    static UINetworkManager* instance() { return m_pInstance; }

    /* Create/destroy singleton: */
    static void create();
    static void destroy();

    /* Pointer to network-manager dialog: */
    UINetworkManagerDialog* window() const;

public slots:

    /* Show network-manager dialog: */
    void show();

protected:

    /* Allow UINetworkCustomer to create network-request: */
    friend class UINetworkCustomer;
    /* Network-request creation wrappers for UINetworkCustomer: */
    void createNetworkRequest(const QNetworkRequest &request, UINetworkRequestType type, const QString &strDescription,
                              UINetworkCustomer *pCustomer);
    void createNetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type, const QString &strDescription,
                              UINetworkCustomer *pCustomer);

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

    /* Network-manager dialog: */
    UINetworkManagerDialog *m_pNetworkManagerDialog;
};
#define gNetworkManager UINetworkManager::instance()

#endif // __UINetworkManager_h__

