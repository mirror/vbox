/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkCustomer class declaration.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UINetworkCustomer_h___
#define ___UINetworkCustomer_h___

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UINetworkDefs.h"

/* Forward declarations: */
class UINetworkReply;
class QUrl;

/** Interface to access UINetworkManager protected functionality. */
class SHARED_LIBRARY_STUFF UINetworkCustomer : public QObject
{
    Q_OBJECT;

public:

    /** Constructs network customer passing @a pParent to the base-class.
      * @param  fForceCall  Brings whether this customer has forced privelegies. */
    UINetworkCustomer(QObject *pParent = 0, bool fForceCall = true);

    /** Returns whether this customer has forced privelegies. */
    bool isItForceCall() const { return m_fForceCall; }

    /** Handles network reply progress for @a iReceived amount of bytes among @a iTotal. */
    virtual void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal) = 0;
    /** Handles network reply canceling for a passed @a pReply. */
    virtual void processNetworkReplyCanceled(UINetworkReply *pReply) = 0;
    /** Handles network reply finishing for a passed @a pReply. */
    virtual void processNetworkReplyFinished(UINetworkReply *pReply) = 0;

    /** Returns description of the current network operation. */
    virtual const QString description() const { return QString(); }

protected:

    /** Creates network-request of the passed @a type on the basis of the passed @a urls and the @a requestHeaders. */
    void createNetworkRequest(UINetworkRequestType enmType, const QList<QUrl> urls,
                              const UserDictionary requestHeaders = UserDictionary());

private:

    /** Holds whether this customer has forced privelegies. */
    bool m_fForceCall;
};

#endif /* !___UINetworkCustomer_h___ */

