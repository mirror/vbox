/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDownloaderWgt class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxConsoleWnd.h"
#include "VBoxDownloaderWgt.h"

#include "qaction.h"
#include "qprogressbar.h"
#include "qtoolbutton.h"
#include "qlayout.h"
#include "qhttp.h"
#include "qstatusbar.h"
#include "qdir.h"
#include "qtimer.h"

VBoxDownloaderWgt::VBoxDownloaderWgt (QStatusBar *aStatusBar, QAction *aAction,
                                      const QString &aUrl, const QString &aTarget)
    : QWidget (0, "VBoxDownloaderWgt")
    , mStatusBar (aStatusBar)
    , mUrl (aUrl), mTarget (aTarget)
    , mHttp (0), mIsChecking (true)
    , mProgressBar (0), mCancelButton (0)
    , mAction (aAction), mStatus (0)
    , mConnectDone (false), mSuicide (false)
{
    /* Disable the associated action */
    mAction->setEnabled (false);

    /* Drawing itself */
    setFixedHeight (16);

    mProgressBar = new QProgressBar (this);
    mProgressBar->setFixedWidth (100);
    mProgressBar->setPercentageVisible (true);
    mProgressBar->setProgress (0);

    mCancelButton = new QToolButton (this);
    mCancelButton->setAutoRaise (true);
    mCancelButton->setFocusPolicy (TabFocus);
    QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Expanding,
                                                 QSizePolicy::Fixed);

    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    mainLayout->addWidget (mProgressBar);
    mainLayout->addWidget (mCancelButton);
    mainLayout->addItem (spacer);

    /* Select the product version */
    QString version = vboxGlobal().virtualBox().GetVersion();

#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
    /* Initialize url operator */
    mHttp = new QHttp (this, "mHttp");
    mHttp->setHost (mUrl.host());

    /* Setup connections */
    connect (mHttp, SIGNAL (dataReadProgress (int, int)),
             this, SLOT (processProgress (int, int)));
    connect (mHttp, SIGNAL (requestFinished (int, bool)),
             this, SLOT (processFinished (int, bool)));
    connect (mHttp, SIGNAL (responseHeaderReceived (const QHttpResponseHeader&)),
             this, SLOT (processResponse (const QHttpResponseHeader&)));
#endif /* !RT_OS_DARWIN */ /// @todo fix the qt build on darwin.
    connect (mCancelButton, SIGNAL (clicked()),
             this, SLOT (processAbort()));

    languageChange();
    mStatusBar->addWidget (this);

    /* Try to get the required file for the information */
    getFile();
}

void VBoxDownloaderWgt::languageChange()
{
    mCancelButton->setText (tr ("Cancel"));
    /// @todo the below title should be parametrized
    QToolTip::add (mProgressBar, tr ("Downloading the VirtualBox Guest Additions "
                                     "CD image from <nobr><b>%1</b>...</nobr>")
                                 .arg (mUrl.toString()));
    /// @todo the below title should be parametrized
    QToolTip::add (mCancelButton, tr ("Cancel the VirtualBox Guest "
                                      "Additions CD image download"));
}

/* This slot is used to handle the progress of the file-downloading
 * procedure. It also checks the downloading status for the file
 * presence verifying purposes. */
void VBoxDownloaderWgt::processProgress (int aRead, int aTotal)
{
    mConnectDone = true;
    if (aTotal != -1)
    {
        if (mIsChecking)
        {
#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
            mHttp->abort();
#endif
            if (mStatus == 404)
                abortDownload (tr ("Could not locate the file on "
                                   "the server (response: %1).")
                               .arg (mStatus));
            else
                processFile (aTotal);
        }
        else
            mProgressBar->setProgress (aRead, aTotal);
    }
    else
        abortDownload (tr ("Could not determine the file size."));
}

/* This slot is used to handle the finish signal of every operation's
 * response. It is used to display the errors occurred during the download
 * operation and for the received-buffer serialization procedure. */
void VBoxDownloaderWgt::processFinished (int, bool aError)
{
#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
    if (aError && mHttp->error() != QHttp::Aborted)
    {
        mConnectDone = true;
        QString reason = mIsChecking ?
            tr ("Could not connect to the server (%1).") :
            tr ("Could not download the file (%1).");
        abortDownload (reason.arg (mHttp->errorString()));
    }
    else if (!aError && !mIsChecking)
    {
        mHttp->abort();
        /* Serialize the incoming buffer into the .iso image. */
        while (true)
        {
            QFile file (mTarget);
            if (file.open (IO_WriteOnly))
            {
                file.writeBlock (mHttp->readAll());
                file.close();
                /// @todo the below action is not part of the generic
                //  VBoxDownloaderWgt functionality, so this class should just
                //  emit a signal when it is done saving the downloaded file
                //  (succeeded or failed).
                int rc = vboxProblem().confirmMountAdditions (mUrl.toString(),
                                           QDir::convertSeparators (mTarget));
                if (rc == QIMessageBox::Yes)
                    vboxGlobal().consoleWnd().installGuestAdditionsFrom (mTarget);
                QTimer::singleShot (0, this, SLOT (suicide()));
                return;
            }
            else
            {
                vboxProblem().message (mStatusBar->topLevelWidget(),
                    VBoxProblemReporter::Error,
                    tr ("<p>Failed to save the downloaded file as "
                        "<nobr><b>%1</b>.</nobr></p>")
                    .arg (QDir::convertSeparators (mTarget)));
            }

            /// @todo read the todo above (probably should just parametrize
            /// the title)
            QString target = vboxGlobal().getExistingDirectory (
                QFileInfo (mTarget).dirPath(), this, "selectSaveDir",
                tr ("Select folder to save Guest Additions image as"), true);
            if (target.isNull())
            {
                QTimer::singleShot (0, this, SLOT (suicide()));
                return;
            }
            else
                mTarget = QDir (target).absFilePath (QFileInfo (mTarget).fileName());
        };
    }
#else
    NOREF (aError);
#endif /* !RT_OS_DARWIN */
}

/* This slot is used to handle the header responses about the
 * requested operations. Watches for the header's status-code. */
void VBoxDownloaderWgt::processResponse (const QHttpResponseHeader &aHeader)
{
#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
    mStatus = aHeader.statusCode();
#else
    NOREF(aHeader);
#endif
}

/* This slot is used to control the connection timeout. */
void VBoxDownloaderWgt::processTimeout()
{
    if (mConnectDone)
        return;
#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
    mHttp->abort();
    abortDownload (tr ("Connection timed out."));
#endif
}

/* This slot is used to process cancel-button clicking signal. */
void VBoxDownloaderWgt::processAbort()
{
#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
    mConnectDone = true;
    mHttp->abort();
    abortDownload (tr ("The download process has been cancelled "
                       "by the user."));
#endif
}

/* This slot is used to terminate the downloader, activate the
 * associated action and removing the downloader's
 * sub-widgets from the VM Console status-bar. */
void VBoxDownloaderWgt::suicide()
{
    mAction->setEnabled (true);
    mStatusBar->removeWidget (this);
    delete this;
}

/* This function is used to make a request to get a file */
void VBoxDownloaderWgt::getFile()
{
    mConnectDone = false;
#ifndef RT_OS_DARWIN /// @todo fix the qt build on darwin.
    mHttp->get (mUrl.path());
#endif
    QTimer::singleShot (5000, this, SLOT (processTimeout()));
}

/* This function is used to ask the user about he wants to download the
 * founded Guest Additions image or not. It also shows the progress-bar
 * and Cancel-button widgets. */
void VBoxDownloaderWgt::processFile (int aSize)
{
    /// @todo the below action is not part of the generic
    //  VBoxDownloaderWgt functionality, so this class should just
    //  emit a signal when it is done detecting the file size.

    /* Ask user about GA image downloading */
    int rc = vboxProblem().
        confirmDownloadAdditions (mUrl.toString(), aSize);
    if (rc == QIMessageBox::Yes)
    {
        mIsChecking = false;
        getFile();
    }
    else
        abortDownload();
}

/* This wrapper displays an error message box (unless @aReason is
 * QString::null) with the cause of the download procedure
 * termination. After the message box is dismissed, the downloader signals
 * to close itself on the next event loop iteration. */
void VBoxDownloaderWgt::abortDownload (const QString &aReason)
{
    /* Protect against double kill request. */
    if (mSuicide)
        return;
    mSuicide = true;

    /// @todo the below action is not part of the generic
    //  VBoxDownloaderWgt functionality, so this class should just emit a
    //  signal to display the error message when the download is aborted.

    if (!aReason.isNull())
        vboxProblem().cannotDownloadGuestAdditions (mUrl.toString(), aReason);

    /* Allows all the queued signals to be processed before quit. */
    QTimer::singleShot (0, this, SLOT (suicide()));
}

