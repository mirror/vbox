/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkRequest class declaration.
 */

/*
 * Copyright (C) 2011-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networking_UINetworkRequest_h
#define FEQT_INCLUDED_SRC_networking_UINetworkRequest_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QPointer>
#include <QUuid>

/* GUI inludes: */
#include "UILibraryDefs.h"
#include "UINetworkDefs.h"
#include "UINetworkReply.h"

/* Forward declarations: */
class UINetworkRequestManager;
class UINetworkRequestManagerWindow;
class UINetworkRequestManagerIndicator;
class UINetworkRequestWidget;
class UINetworkCustomer;

/** QObject extension used as network-request container. */
class SHARED_LIBRARY_STUFF UINetworkRequest : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies common UINetworkRequestManager about progress with @a uId changed.
      * @param  iReceived  Holds the amount of bytes received.
      * @param  iTotal     Holds the amount of total bytes to receive. */
    void sigProgress(const QUuid &uId, qint64 iReceived, qint64 iTotal);
    /** Notifies UINetworkRequestManager about progress with @a uId started. */
    void sigStarted(const QUuid &uId);
    /** Notifies UINetworkRequestManager about progress with @a uId canceled. */
    void sigCanceled(const QUuid &uId);
    /** Notifies UINetworkRequestManager about progress with @a uId finished. */
    void sigFinished(const QUuid &uId);
    /** Notifies UINetworkRequestManager about progress with @a uId failed with @a strError. */
    void sigFailed(const QUuid &uId, const QString &strError);

    /** Notifies own UINetworkRequestWidget about progress changed.
      * @param  iReceived  Holds the amount of bytes received.
      * @param  iTotal     Holds the amount of total bytes to receive. */
    void sigProgress(qint64 iReceived, qint64 iTotal);
    /** Notifies own UINetworkRequestWidget about progress started. */
    void sigStarted();
    /** Notifies own UINetworkRequestWidget about progress finished. */
    void sigFinished();
    /** Notifies own UINetworkRequestWidget about progress failed with @a strError. */
    void sigFailed(const QString &strError);

public:

    /** Constructs network-request.
      * @param  enmType          Brings request type.
      * @param  urls             Brings request urls, there can be few of them.
      * @param  strTarget        Brings request target path.
      * @param  requestHeaders   Brings request headers in dictionary form.
      * @param  pCustomer        Brings customer this request ordered by.
      * @param  pNetworkManager  Brings network access manager this requests managed by. */
    UINetworkRequest(UINetworkRequestType enmType,
                     const QList<QUrl> &urls,
                     const QString &strTarget,
                     const UserDictionary &requestHeaders,
                     UINetworkCustomer *pCustomer,
                     UINetworkRequestManager *pNetworkManager);
    /** Destructs network-request. */
    virtual ~UINetworkRequest() /* override final */;

    /** Returns the request description. */
    const QString description() const;
    /** Returns the request customer. */
    UINetworkCustomer *customer() { return m_pCustomer; }
    /** Returns the request manager. */
    UINetworkRequestManager *manager() const { return m_pNetworkManager; }
    /** Returns unique request QUuid. */
    const QUuid &uuid() const { return m_uuid; }
    /** Returns the request reply. */
    UINetworkReply *reply() { return m_pReply; }

public slots:

    /** Initiates request retrying. */
    void sltRetry();
    /** Initiates request cancelling. */
    void sltCancel();

private slots:

    /** Handles reply about progress changed.
      * @param  iReceived  Brings the amount of bytes received.
      * @param  iTotal     Brings the amount of total bytes to receive. */
    void sltHandleNetworkReplyProgress(qint64 iReceived, qint64 iTotal);
    /** Handles reply about progress finished. */
    void sltHandleNetworkReplyFinish();

private:

    /** Prepares request. */
    void prepare();
    /** Prepares request's reply. */
    void prepareNetworkReply();

    /** Cleanups request's reply. */
    void cleanupNetworkReply();
    /** Cleanups request. */
    void cleanup();

    /** Holds the request type. */
    const UINetworkRequestType  m_enmType;
    /** Holds the request urls. */
    const QList<QUrl>           m_urls;
    /** Holds the request target. */
    const QString               m_strTarget;
    /** Holds the request headers. */
    const UserDictionary        m_requestHeaders;
    /** Holds the request customer. */
    UINetworkCustomer          *m_pCustomer;
    /** Holds the request manager. */
    UINetworkRequestManager    *m_pNetworkManager;
    /** Holds unique request QUuid. */
    const QUuid                 m_uuid;

    /** Holds current request url. */
    QUrl  m_url;
    /** Holds index of current request url. */
    int   m_iUrlIndex;
    /** Holds whether current request url is in progress. */
    bool  m_fRunning;

    /** Holds the request reply. */
    QPointer<UINetworkReply>  m_pReply;
};

#endif /* !FEQT_INCLUDED_SRC_networking_UINetworkRequest_h */
