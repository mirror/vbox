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
 */

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxConsoleWnd.h"
#include "VBoxDownloaderWgt.h"

#include "qaction.h"
#include "qprogressbar.h"
#include "qtoolbutton.h"
#include "qlayout.h"
#include "qstatusbar.h"
#include "qdir.h"
#include "qtimer.h"

/* These notifications are used to notify the GUI thread about different
 * downloading events: Downloading Started, Downloading in Progress,
 * Downloading Finished. */
enum
{
    StartDownloadEventType = QEvent::User + 100,
    ProcessDownloadEventType,
    FinishDownloadEventType,
    ErrorDownloadEventType
};

class StartDownloadEvent : public QEvent
{
public:
    StartDownloadEvent (int aStatus, long aSize)
        : QEvent ((QEvent::Type) StartDownloadEventType)
        , mStatus (aStatus), mSize (aSize) {}

    int  mStatus;
    long mSize;
};

class ProcessDownloadEvent : public QEvent
{
public:
    ProcessDownloadEvent (const char *aData, ulong aSize)
        : QEvent ((QEvent::Type) ProcessDownloadEventType)
        , mData (QByteArray().duplicate (aData, aSize)) {}

    QByteArray mData;
};

class FinishDownloadEvent : public QEvent
{
public:
    FinishDownloadEvent()
        : QEvent ((QEvent::Type) FinishDownloadEventType) {}
};

class ErrorDownloadEvent : public QEvent
{
public:
    ErrorDownloadEvent (const QString &aInfo)
        : QEvent ((QEvent::Type) ErrorDownloadEventType)
        , mInfo (aInfo) {}

    QString mInfo;
};

/* This callback is used to handle the file-downloading procedure
 * beginning. It checks the downloading status for the file
 * presence verifying purposes. */
void OnBegin (const happyhttp::Response *aResponse, void *aUserdata)
{
    VBoxDownloaderWgt *loader = static_cast<VBoxDownloaderWgt*> (aUserdata);
    if (loader->isCheckingPresence())
    {
        int status = aResponse->getstatus();
        QString contentLength = aResponse->getheader ("Content-length");

        QApplication::postEvent (loader,
            new StartDownloadEvent (status, contentLength.toLong()));
    }
}

/* This callback is used to handle the progress of the file-downloading
 * procedure. It also checks the downloading status for the file
 * presence verifying purposes. */
void OnData (const happyhttp::Response*, void *aUserdata,
             const unsigned char *aData, int aSize)
{
    VBoxDownloaderWgt *loader = static_cast<VBoxDownloaderWgt*> (aUserdata);
    if (!loader->isCheckingPresence())
    {
        QApplication::postEvent (loader,
            new ProcessDownloadEvent ((const char*)aData, aSize));
    }
}

/* This callback is used to handle the finish signal of every operation's
 * response. It is used to display the errors occurred during the download
 * operation and for the received-buffer serialization procedure. */
void OnComplete (const happyhttp::Response*, void *aUserdata)
{
    VBoxDownloaderWgt *loader = static_cast<VBoxDownloaderWgt*> (aUserdata);
    if (!loader->isCheckingPresence())
        QApplication::postEvent (loader,
            new FinishDownloadEvent());
}

VBoxDownloaderWgt::VBoxDownloaderWgt (QStatusBar *aStatusBar, QAction *aAction,
                                      const QString &aUrl, const QString &aTarget)
    : QWidget (0, "VBoxDownloaderWgt")
    , mUrl (aUrl), mTarget (aTarget)
    , mStatusBar (aStatusBar), mAction (aAction)
    , mProgressBar (0), mCancelButton (0)
    , mIsChecking (true), mSuicide (false)
    , mConn (new HConnect (mUrl.host(), 80))
    , mRequestThread (0)
    , mDataStream (mDataArray, IO_WriteOnly)
    , mTimeout (new QTimer (this))
{
    /* Disable the associated action */
    mAction->setEnabled (false);
    connect (mTimeout, SIGNAL (timeout()),
             this, SLOT (processTimeout()));

    /* Drawing itself */
    setFixedHeight (16);

    mProgressBar = new QProgressBar (this);
    mProgressBar->setFixedWidth (100);
    mProgressBar->setPercentageVisible (true);
    mProgressBar->setProgress (0);

    mCancelButton = new QToolButton (this);
    mCancelButton->setAutoRaise (true);
    mCancelButton->setFocusPolicy (TabFocus);
    connect (mCancelButton, SIGNAL (clicked()),
             this, SLOT (processAbort()));

    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    mainLayout->addWidget (mProgressBar);
    mainLayout->addWidget (mCancelButton);
    mainLayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding,
                                                QSizePolicy::Fixed));

    /* Prepare the connection */
    mConn->setcallbacks (OnBegin, OnData, OnComplete, this);

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

/* This slot is used to control the connection timeout. */
void VBoxDownloaderWgt::processTimeout()
{
    abortDownload (tr ("Connection timed out."));
}

/* This slot is used to process cancel-button clicking signal. */
void VBoxDownloaderWgt::processAbort()
{
    abortDownload (tr ("The download process has been cancelled "
                       "by the user."));
}

/* This slot is used to terminate the downloader, activate the
 * associated action and removing the downloader's
 * sub-widgets from the VM Console status-bar. */
void VBoxDownloaderWgt::suicide()
{
    delete mRequestThread;
    delete mConn;

    mAction->setEnabled (true);
    mStatusBar->removeWidget (this);
    delete this;
}

/* Used to process all the widget events */
bool VBoxDownloaderWgt::event (QEvent *aEvent)
{
    switch (aEvent->type())
    {
        case StartDownloadEventType:
        {
            StartDownloadEvent *e = static_cast<StartDownloadEvent*> (aEvent);

            mTimeout->stop();
            if (e->mStatus == 404)
                abortDownload (tr ("Could not locate the file on "
                    "the server (response: %1).").arg (e->mStatus));
            else
                processFile (e->mSize);

            return true;
        }
        case ProcessDownloadEventType:
        {
            ProcessDownloadEvent *e = static_cast<ProcessDownloadEvent*> (aEvent);

            mTimeout->start (20000, true);
            mProgressBar->setProgress (mProgressBar->progress() + e->mData.size());
            mDataStream.writeRawBytes (e->mData.data(), e->mData.size());

            return true;
        }
        case FinishDownloadEventType:
        {
            mTimeout->stop();

            /* Serialize the incoming buffer into the .iso image. */
            while (true)
            {
                QFile file (mTarget);
                if (file.open (IO_WriteOnly))
                {
                    file.writeBlock (mDataArray);
                    file.close();
                    /// @todo the below action is not part of the generic
                    //  VBoxDownloaderWgt functionality, so this class should just
                    //  emit a signal when it is done saving the downloaded file
                    //  (succeeded or failed).
                    if (vboxProblem()
                            .confirmMountAdditions (mUrl.toString(),
                                                    QDir::convertSeparators (mTarget)))
                        vboxGlobal().consoleWnd().installGuestAdditionsFrom (mTarget);
                    QTimer::singleShot (0, this, SLOT (suicide()));
                    break;
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
                    tr ("Select folder to save Guest Additions image to"), true);
                if (target.isNull())
                    QTimer::singleShot (0, this, SLOT (suicide()));
                else
                    mTarget = QDir (target).absFilePath (QFileInfo (mTarget).fileName());
            }

            return true;
        }
        case ErrorDownloadEventType:
        {
            ErrorDownloadEvent *e = static_cast<ErrorDownloadEvent*> (aEvent);

            abortDownload (e->mInfo);

            return true;
        }
        default:
            break;
    }

    return QWidget::event (aEvent);
}

/* This function is used to make a request to get a file */
void VBoxDownloaderWgt::getFile()
{
    /* Http request thread class */
    class Thread : public QThread
    {
    public:

        Thread (VBoxDownloaderWgt *aParent, HConnect *aConn,
                const QString &aPath, QMutex *aMutex)
            : mParent (aParent), mConn (aConn)
            , mPath (aPath), mMutex (aMutex) {}

        virtual void run()
        {
            try
            {
                mConn->request ("GET", mPath);
                while (mConn->outstanding())
                {
                    QMutexLocker locker (mMutex);
                    mConn->pump();
                }
            }
            catch (happyhttp::Wobbly &ex)
            {
                QApplication::postEvent (mParent,
                    new ErrorDownloadEvent (ex.what()));
            }
        }

    private:

        VBoxDownloaderWgt *mParent;
        HConnect *mConn;
        QString mPath;
        QMutex *mMutex;
    };

    if (!mRequestThread)
        mRequestThread = new Thread (this, mConn, mUrl.path(), &mMutex);
    mRequestThread->start();
    mTimeout->start (20000, true);
}

/* This function is used to ask the user about he wants to download the
 * founded Guest Additions image or not. It also shows the progress-bar
 * and Cancel-button widgets. */
void VBoxDownloaderWgt::processFile (int aSize)
{
    abortConnection();

    /// @todo the below action is not part of the generic
    //  VBoxDownloaderWgt functionality, so this class should just
    //  emit a signal when it is done detecting the file size.

    /* Ask user about GA image downloading */
    if (vboxProblem().confirmDownloadAdditions (mUrl.toString(), aSize))
    {
        mIsChecking = false;
        mProgressBar->setTotalSteps (aSize);
        getFile();
    }
    else
        abortDownload();
}

/* This wrapper displays an error message box (unless aReason is
 * QString::null) with the cause of the download procedure
 * termination. After the message box is dismissed, the downloader signals
 * to close itself on the next event loop iteration. */
void VBoxDownloaderWgt::abortDownload (const QString &aReason)
{
    abortConnection();

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

void VBoxDownloaderWgt::abortConnection()
{
    mMutex.lock();
    mConn->close();
    mMutex.unlock();
}

