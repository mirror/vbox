/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkManagerIndicator stuff declaration.
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

#ifndef ___UINetworkManagerIndicator_h___
#define ___UINetworkManagerIndicator_h___

/* Qt includes: */
#include <QVector>
#include <QUuid>

/* GUI includes: */
#include "QIStatusBarIndicator.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class UINetworkRequest;


/** Network-manager status-bar indicator states. */
enum UINetworkManagerIndicatorState
{
    UINetworkManagerIndicatorState_Idle,
    UINetworkManagerIndicatorState_Loading,
    UINetworkManagerIndicatorState_Error
};


/** QIStateStatusBarIndicator extension for network-manager indicator. */
class SHARED_LIBRARY_STUFF UINetworkManagerIndicator : public QIWithRetranslateUI<QIStateStatusBarIndicator>
{
    Q_OBJECT;

public:

    /** Constructs network manager indicator. */
    UINetworkManagerIndicator();

    /** Updates appearance. */
    void updateAppearance();

public slots:

    /** Adds @a pNetworkRequest to network-manager state-indicators. */
    void sltAddNetworkManagerIndicatorDescription(UINetworkRequest *pNetworkRequest);
    /** Removes network-request with @a uuid from network-manager state-indicators. */
    void sldRemoveNetworkManagerIndicatorDescription(const QUuid &uuid);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Sets particular network-request @a uuid progress to 'started'. */
    void sltSetProgressToStarted(const QUuid &uuid);
    /** Sets particular network-request @a uuid progress to 'canceled'. */
    void sltSetProgressToCanceled(const QUuid &uuid);
    /** Sets particular network-request @a uuid progress to 'failed'. */
    void sltSetProgressToFailed(const QUuid &uuid, const QString &strError);
    /** Sets particular network-request @a uuid progress to 'finished'. */
    void sltSetProgressToFinished(const QUuid &uuid);
    /** Updates particular network-request @a uuid progress for @a iReceived amount of bytes among @a iTotal. */
    void sltSetProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal);

private:

    /** Network request data. */
    struct UINetworkRequestData
    {
        /** Constructs network request data. */
        UINetworkRequestData()
            : bytesReceived(0), bytesTotal(0), failed(false) {}
        /** Constructs network request data with @a strDescription, @a iBytesReceived and @a iBytesTotal. */
        UINetworkRequestData(const QString &strDescription, int iBytesReceived, int iBytesTotal)
            : description(strDescription), bytesReceived(iBytesReceived), bytesTotal(iBytesTotal), failed(false) {}
        /** Holds the description. */
        QString description;
        /** Holds the amount of bytes received. */
        int bytesReceived;
        /** Holds the amount of total bytes. */
        int bytesTotal;
        /** Holds whether request is failed. */
        bool failed;
    };

    /** Update stuff. */
    void recalculateIndicatorState();

    /** Holds the vector of network request IDs. */
    QVector<QUuid> m_ids;
    /** Holds the vector of network request data. */
    QVector<UINetworkRequestData> m_data;
};


#endif /* !___UINetworkManagerIndicator_h___ */

