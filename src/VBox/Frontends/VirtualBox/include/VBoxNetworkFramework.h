/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxNetworkFramework class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxNetworkFramework_h__
#define __VBoxNetworkFramework_h__

#include <HappyHttp.h>
#include <qobject.h>
#include <qthread.h>
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
        : mDataStream (mDataArray, IO_ReadWrite)
        , mNetworkThread (0) {}

    ~VBoxNetworkFramework() { delete mNetworkThread; }

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

