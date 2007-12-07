/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDownloaderWgt class declaration
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

#ifndef __VBoxDownloaderWgt_h__
#define __VBoxDownloaderWgt_h__

#include "HappyHttp.h"
#include "qwidget.h"
#include "qurl.h"
#include "qmutex.h"
class QStatusBar;
class QAction;
class QProgressBar;
class QToolButton;
class QThread;
class QTimer;
typedef happyhttp::Connection HConnect;

/** class VBoxDownloaderWgt
 *
 *  The VBoxDownloaderWgt class is an QWidget class for Guest Additions
 *  http backgroung downloading. This class is also used to display the
 *  Guest Additions download state through the progress dialog integrated
 *  into the VM console status bar.
 */
class VBoxDownloaderWgt : public QWidget
{
    Q_OBJECT

public:

    VBoxDownloaderWgt (QStatusBar *aStatusBar, QAction *aAction,
                       const QString &aUrl, const QString &aTarget);

    void languageChange();

    bool isCheckingPresence() { return mIsChecking; }

private slots:

    /* This slot is used to control the connection timeout. */
    void processTimeout();

    /* This slot is used to process cancel-button clicking signal. */
    void processAbort();

    /* This slot is used to terminate the downloader, activate the
     * Install Guest Additions action and removing the downloader's
     * sub-widgets from the VM Console status-bar. */
    void suicide();

private:

    /* Used to process all the widget events */
    bool event (QEvent *aEvent);

    /* This function is used to make a request to get a file */
    void getFile();

    /* This function is used to ask the user about he wants to download the
     * founded Guest Additions image or not. It also shows the progress-bar
     * and Cancel-button widgets. */
    void processFile (int aSize);

    /* This wrapper displays an error message box (unless @aReason is
     * QString::null) with the cause of the download procedure
     * termination. After the message box is dismissed, the downloader signals
     * to close itself on the next event loop iteration. */
    void abortDownload (const QString &aReason = QString::null);

    void abortConnection();

    QUrl mUrl;
    QString mTarget;
    QStatusBar *mStatusBar;
    QAction *mAction;
    QProgressBar *mProgressBar;
    QToolButton *mCancelButton;
    bool mIsChecking;
    bool mSuicide;
    HConnect *mConn;
    QThread *mRequestThread;
    QMutex mMutex;
    QByteArray mDataArray;
    QDataStream mDataStream;
    QTimer *mTimeout;
};

#endif

