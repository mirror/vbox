/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDownloaderWgt class declaration
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

#ifndef __VBoxDownloaderWgt_h__
#define __VBoxDownloaderWgt_h__

#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QUrl>
#include <QWidget>

class QIHttp;
class QHttpResponseHeader;
class QProgressBar;
class QToolButton;

/**
 * The VBoxDownloaderWgt class is QWidget class re-implementation which embeds
 * into the Dialog's status-bar and allows background http downloading.
 * This class is not supposed to be used itself and made for sub-classing only.
 *
 * This class has two parts:
 * 1. Acknowledging (getting information about target presence and size).
 * 2. Downloading (starting and handling file downloading process).
 * Every subclass can determine using or not those two parts and handling
 * the result of those parts itself.
 */
class VBoxDownloaderWgt : public QIWithRetranslateUI <QWidget>
{
    Q_OBJECT;

public:

    VBoxDownloaderWgt (const QString &aSource, const QString &aTarget);

    virtual void start();

protected slots:

    /* Acknowledging part */
    virtual void acknowledgeStart();
    virtual void acknowledgeProcess (const QHttpResponseHeader &aResponse);
    virtual void acknowledgeFinished (bool aError);

    /* Downloading part */
    virtual void downloadStart();
    virtual void downloadProcess (int aDone, int aTotal);
    virtual void downloadFinished (bool aError);

    /* Common slots */
    virtual void cancelDownloading();
    virtual void abortDownload (const QString &aError);
    virtual void suicide();

protected:

    /* In sub-class this function will show the user downloading object size
     * and ask him about downloading confirmation. Returns user response. */
    virtual bool confirmDownload() = 0;

    /* In sub-class this function will show the user which error happens
     * in context of downloading file and executing his request. */
    virtual void warnAboutError (const QString &aError) = 0;

    QUrl mSource;
    QString mTarget;
    QIHttp *mHttp;
    QProgressBar *mProgressBar;
    QToolButton *mCancelButton;
};

#endif // __VBoxDownloaderWgt_h__

