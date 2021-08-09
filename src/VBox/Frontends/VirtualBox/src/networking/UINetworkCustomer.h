/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkCustomer class declaration.
 */

/*
 * Copyright (C) 2012-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networking_UINetworkCustomer_h
#define FEQT_INCLUDED_SRC_networking_UINetworkCustomer_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QUrl>
#include <QUuid>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UINetworkDefs.h"

/* Forward declarations: */
class UINetworkReply;

/** Interface to access UINetworkRequestManager protected functionality. */
class SHARED_LIBRARY_STUFF UINetworkCustomer : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a pNetworkCustomer being destroyed. */
    void sigBeingDestroyed(UINetworkCustomer *pNetworkCustomer);

public:

    /** Constructs network customer passing @a pParent to the base-class. */
    UINetworkCustomer();
    /** Destructs network customer. */
    virtual ~UINetworkCustomer() /* override */;

    /** Returns description of the current network operation. */
    virtual QString description() const { return QString(); }

    /** Handles network reply progress for @a iReceived amount of bytes among @a iTotal. */
    virtual void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal) = 0;
    /** Handles network reply failed with specified @a strError. */
    virtual void processNetworkReplyFailed(const QString &strError) = 0;
    /** Handles network reply canceling for a passed @a pReply. */
    virtual void processNetworkReplyCanceled(UINetworkReply *pReply) = 0;
    /** Handles network reply finishing for a passed @a pReply. */
    virtual void processNetworkReplyFinished(UINetworkReply *pReply) = 0;

protected:

    /** Creates network-request.
      * @param  enmType         Brings request type.
      * @param  urls            Brings request urls, there can be few of them.
      * @param  strTarget       Brings request target path.
      * @param  requestHeaders  Brings request headers in dictionary form. */
    void createNetworkRequest(UINetworkRequestType enmType,
                              const QList<QUrl> urls,
                              const QString &strTarget = QString(),
                              const UserDictionary requestHeaders = UserDictionary());

    /** Aborts network-request. */
    void cancelNetworkRequest();

private:

    /** Holds the network-request ID. */
    QUuid  m_uId;
};

#endif /* !FEQT_INCLUDED_SRC_networking_UINetworkCustomer_h */
