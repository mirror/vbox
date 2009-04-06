/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDownloaderWgt class implementation
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

#include "QIHttp.h"
#include "VBoxDownloaderWgt.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QFile>
#include <QHBoxLayout>
#include <QHttpResponseHeader>
#include <QProgressBar>
#include <QToolButton>

VBoxDownloaderWgt::VBoxDownloaderWgt (const QString &aSource, const QString &aTarget)
    : mSource (aSource), mTarget (aTarget), mHttp (0)
    , mProgressBar (new QProgressBar (this))
    , mCancelButton (new QToolButton (this))
{
    /* Progress Bar setup */
    mProgressBar->setFixedWidth (100);
    mProgressBar->setFormat ("%p%");
    mProgressBar->setValue (0);

    /* Cancel Button setup */
    mCancelButton->setAutoRaise (true);
    mCancelButton->setFocusPolicy (Qt::TabFocus);
    connect (mCancelButton, SIGNAL (clicked()), this, SLOT (cancelDownloading()));

    /* Downloader setup */
    setFixedHeight (16);
    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    mainLayout->setSpacing (0);
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
    mainLayout->addWidget (mProgressBar);
    mainLayout->addWidget (mCancelButton);
    mainLayout->addStretch (1);
}

void VBoxDownloaderWgt::start()
{
    /* By default we are not using acknowledging step, so
     * making downloading immediately */
    downloadStart();
}

/* This function is used to start acknowledging mechanism:
 * checking file presence & size */
void VBoxDownloaderWgt::acknowledgeStart()
{
    delete mHttp;
    mHttp = new QIHttp (this, mSource.host());
    connect (mHttp, SIGNAL (responseHeaderReceived (const QHttpResponseHeader &)),
             this, SLOT (acknowledgeProcess (const QHttpResponseHeader &)));
    connect (mHttp, SIGNAL (allIsDone (bool)), this, SLOT (acknowledgeFinished (bool)));
    mHttp->get (mSource.toEncoded());
}

/* This function is used to store content length */
void VBoxDownloaderWgt::acknowledgeProcess (const QHttpResponseHeader & /* aResponse */)
{
    /* Abort connection as we already got all we need */
    mHttp->abort();
}

/* This function is used to ask the user about if he really want
 * to download file of proposed size if no error present or
 * abort download progress if error is present */
void VBoxDownloaderWgt::acknowledgeFinished (bool aError)
{
    NOREF(aError);

    AssertMsg (aError, ("Error must be 'true' due to aborting.\n"));

    mHttp->disconnect (this);

    switch (mHttp->errorCode())
    {
        case QIHttp::Aborted:
        {
            /* Ask the user if he wish to download it */
            if (confirmDownload())
                QTimer::singleShot (0, this, SLOT (downloadStart()));
            else
                QTimer::singleShot (0, this, SLOT (suicide()));
            break;
        }
        case QIHttp::MovedPermanentlyError:
        case QIHttp::MovedTemporarilyError:
        {
            /* Restart downloading at new location */
            mSource = mHttp->lastResponse().value ("location");
            QTimer::singleShot (0, this, SLOT (acknowledgeStart()));
            break;
        }
        default:
        {
            /* Show error happens during acknowledging */
            abortDownload (mHttp->errorString());
            break;
        }
    }
}

/* This function is used to start downloading mechanism:
 * downloading and saving the target */
void VBoxDownloaderWgt::downloadStart()
{
    delete mHttp;
    mHttp = new QIHttp (this, mSource.host());
    connect (mHttp, SIGNAL (dataReadProgress (int, int)),
             this, SLOT (downloadProcess (int, int)));
    connect (mHttp, SIGNAL (allIsDone (bool)), this, SLOT (downloadFinished (bool)));
    mHttp->get (mSource.toEncoded());
}

/* this function is used to observe the downloading progress through
 * changing the corresponding qprogressbar value */
void VBoxDownloaderWgt::downloadProcess (int aDone, int aTotal)
{
    mProgressBar->setMaximum (aTotal);
    mProgressBar->setValue (aDone);
}

/* This function is used to handle the 'downloading finished' issue
 * through saving the downloaded into the file if there in no error or
 * notifying the user about error happens */
void VBoxDownloaderWgt::downloadFinished (bool aError)
{
    mHttp->disconnect (this);

    if (aError)
    {
        /* Show information about error happens */
        if (mHttp->errorCode() == QIHttp::Aborted)
            abortDownload (tr ("The download process has been cancelled by the user."));
        else
            abortDownload (mHttp->errorString());
    }
    else
    {
        /* Trying to serialize the incoming buffer into the target, this is the
         * default behavior which have to be reimplemented in sub-class */
        QFile file (mTarget);
        if (file.open (QIODevice::WriteOnly))
        {
            file.write (mHttp->readAll());
            file.close();
        }
        QTimer::singleShot (0, this, SLOT (suicide()));
    }
}

/* This slot is used to process cancel-button clicking */
void VBoxDownloaderWgt::cancelDownloading()
{
    QTimer::singleShot (0, this, SLOT (suicide()));
}

/* This function is used to abort download by showing aborting reason
 * and calling the downloader's delete function */
void VBoxDownloaderWgt::abortDownload (const QString &aError)
{
    warnAboutError (aError);
    QTimer::singleShot (0, this, SLOT (suicide()));
}

/* This function is used to delete the downloader widget itself,
 * should be reimplemented to enhanse necessary functionality in sub-class */
void VBoxDownloaderWgt::suicide()
{
    delete this;
}

