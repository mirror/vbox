/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxNetworkFramework class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxNetworkFramework_h__
#define __VBoxNetworkFramework_h__

#include <HappyHttp.h>
#include <qobject.h>
#include <qthread.h>
//Added by qt3to4:
#include <QEvent>
#include <QDataStream>
typedef happyhttp::Connection HConnect;

/**
 *  QObject class reimplementation which is the target for different
 *  events reseived from network thread.
 */
class VBoxNetworkFramework : public QObject
{
    Q_OBJECT

public:

    VBoxNetworkFramework()
        : mDataStream (&mDataArray, QIODevice::ReadWrite)
        , mNetworkThread (0) {}

    ~VBoxNetworkFramework()
    {
        mNetworkThread->wait (1000);
        delete mNetworkThread;
    }

    void postRequest (const QString &aHost, const QString &aUrl);

signals:

    void netBegin (int aStatus);
    void netData (const QByteArray &aData);
    void netEnd (const QByteArray &aData);
    void netError (const QString &aData);

private:

    bool event (QEvent *aEvent);

    QByteArray   mDataArray;
    QDataStream  mDataStream;
    QThread     *mNetworkThread;
};

#endif // __VBoxNetworkFramework_h__

