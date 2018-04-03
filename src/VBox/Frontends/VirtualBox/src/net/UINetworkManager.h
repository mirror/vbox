/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkManager stuff declaration.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UINetworkManager_h___
#define ___UINetworkManager_h___

/* Qt includes: */
#include <QObject>
#include <QUuid>

/* GUI inludes: */
#include "UILibraryDefs.h"
#include "UINetworkDefs.h"

/* Forward declarations: */
class QUrl;
class QWidget;
class UINetworkCustomer;
class UINetworkManagerDialog;
class UINetworkManagerIndicator;
class UINetworkRequest;

/** QObject class extension.
  * Providing network access for VirtualBox application purposes. */
class SHARED_LIBRARY_STUFF UINetworkManager : public QObject
{
    Q_OBJECT;

signals:

    /** Asks listeners (network-requests) to cancel. */
    void sigCancelNetworkRequests();

    /** Requests to add @a pNetworkRequest to network-manager state-indicators. */
    void sigAddNetworkManagerIndicatorDescription(UINetworkRequest *pNetworkRequest);
    /** Requests to remove network-request with @a uuid from network-manager state-indicators. */
    void sigRemoveNetworkManagerIndicatorDescription(const QUuid &uuid);

public:

    /** Creates singleton instance. */
    static void create();
    /** Destroys singleton instance. */
    static void destroy();

    /** Returns the singleton instance. */
    static UINetworkManager *instance() { return s_pInstance; }

    /** Returns pointer to network-manager dialog. */
    UINetworkManagerDialog *window() const;

    /** Creates network-manager state-indicator.
      * @remarks To be cleaned up by the caller. */
    UINetworkManagerIndicator *createIndicator() const;

    /** Registers @a pNetworkRequest in network-manager. */
    void registerNetworkRequest(UINetworkRequest *pNetworkRequest);
    /** Unregisters network-request with @a uuid from network-manager. */
    void unregisterNetworkRequest(const QUuid &uuid);

public slots:

    /** Shows network-manager dialog. */
    void show();

protected:

    /** Allows UINetworkCustomer to create network-request. */
    friend class UINetworkCustomer;

    /** Creates network-request of the passed @a type
      * on the basis of the passed @a urls and the @a requestHeaders for the @a pCustomer specified. */
    void createNetworkRequest(UINetworkRequestType enmType, const QList<QUrl> &urls,
                              const UserDictionary &requestHeaders, UINetworkCustomer *pCustomer);

private:

    /** Constructs network manager. */
    UINetworkManager();
    /** Destructs network manager. */
    ~UINetworkManager();

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Prepares @a pNetworkRequest. */
    void prepareNetworkRequest(UINetworkRequest *pNetworkRequest);
    /** Cleanups network-request with passed @a uuid. */
    void cleanupNetworkRequest(QUuid uuid);
    /** Cleanups all network-requests. */
    void cleanupNetworkRequests();

private slots:

    /** Handles progress for @a iReceived amount of bytes among @a iTotal for request specified by @a uuid. */
    void sltHandleNetworkRequestProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal);
    /** Handles canceling of request specified by @a uuid. */
    void sltHandleNetworkRequestCancel(const QUuid &uuid);
    /** Handles finishing of request specified by @a uuid. */
    void sltHandleNetworkRequestFinish(const QUuid &uuid);
    /** Handles @a strError of request specified by @a uuid. */
    void sltHandleNetworkRequestFailure(const QUuid &uuid, const QString &strError);

private:

    /** Holds the singleton instance. */
    static UINetworkManager *s_pInstance;

    /** Holds the map of current requests. */
    QMap<QUuid, UINetworkRequest*> m_requests;

    /** Holds the network manager dialog instance. */
    UINetworkManagerDialog *m_pNetworkManagerDialog;
};

/** Singleton Network Manager 'official' name. */
#define gNetworkManager UINetworkManager::instance()

#endif /* !___UINetworkManager_h___ */

