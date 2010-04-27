/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "UIDownloader.h"
#include "QIHttp.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxSpecialControls.h"

/* Global includes */
#include <QFile>
#include <QProgressBar>

UIMiniProcessWidget::UIMiniProcessWidget(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pProgressBar(new QProgressBar(this))
    , m_pCancelButton(new VBoxMiniCancelButton(this))
{
    /* Progress Bar setup */
    m_pProgressBar->setFixedWidth(100);
    m_pProgressBar->setFormat("%p%");
    m_pProgressBar->setValue(0);

    /* Cancel Button setup */
    m_pCancelButton->setFocusPolicy(Qt::NoFocus);
    m_pCancelButton->removeBorder();
    connect(m_pCancelButton, SIGNAL(clicked()),
            this, SIGNAL(sigCancel()));

    setContentsMargins(0, 0, 0, 0);
    setFixedHeight(16);

    /* Layout setup */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    VBoxGlobal::setLayoutMargin(pMainLayout, 0);

#ifdef Q_WS_MAC
    pMainLayout->setSpacing(2);
    m_pProgressBar->setFixedHeight(14);
    m_pCancelButton->setFixedHeight(11);
    pMainLayout->addWidget(m_pProgressBar, 0, Qt::AlignTop);
    pMainLayout->addWidget(m_pCancelButton, 0, Qt::AlignBottom);
#else /* Q_WS_MAC */
    pMainLayout->setSpacing(0);
    pMainLayout->addWidget(m_pProgressBar, 0, Qt::AlignCenter);
    pMainLayout->addWidget(m_pCancelButton, 0, Qt::AlignCenter);
#endif /* !Q_WS_MAC */

    pMainLayout->addStretch(1);

}

void UIMiniProcessWidget::setCancelButtonText(const QString &strText)
{
    m_pCancelButton->setText(strText);
}

QString UIMiniProcessWidget::cancelButtonText() const
{
    return m_pCancelButton->text();
}

void UIMiniProcessWidget::setCancelButtonToolTip(const QString &strText)
{
    m_pCancelButton->setToolTip(strText);
}

QString UIMiniProcessWidget::cancelButtonToolTip() const
{
    return m_pCancelButton->toolTip();
}

void UIMiniProcessWidget::setProgressBarToolTip(const QString &strText)
{
    m_pProgressBar->setToolTip(strText);
}

QString UIMiniProcessWidget::progressBarToolTip() const
{
    return m_pProgressBar->toolTip();
}

void UIMiniProcessWidget::setSource(const QString &strSource)
{
    m_strSource = strSource;
}

QString UIMiniProcessWidget::source() const
{
    return m_strSource;
}

void UIMiniProcessWidget::sltProcess(int cDone, int cTotal)
{
    m_pProgressBar->setMaximum(cTotal);
    m_pProgressBar->setValue(cDone);
}

UIDownloader::UIDownloader()
    : m_pHttp(0)
{
}

void UIDownloader::setSource(const QString &strSource)
{
    m_source = strSource;
}

QString UIDownloader::source() const
{
    return m_source.toString();
}

void UIDownloader::setTarget(const QString &strTarget)
{
    m_strTarget = strTarget;
}

QString UIDownloader::target() const
{
    return m_strTarget;
}

void UIDownloader::startDownload()
{
    /* By default we are not using acknowledging step, so
     * making downloading immediately */
    downloadStart();
}

/* This function is used to start acknowledging mechanism:
 * checking file presence & size */
void UIDownloader::acknowledgeStart()
{
    delete m_pHttp;
    m_pHttp = new QIHttp(this, m_source.host());
    connect(m_pHttp, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
            this, SLOT(acknowledgeProcess(const QHttpResponseHeader &)));
    connect(m_pHttp, SIGNAL(allIsDone(bool)),
            this, SLOT(acknowledgeFinished(bool)));
    m_pHttp->get(m_source.toEncoded());
}

/* This function is used to store content length */
void UIDownloader::acknowledgeProcess(const QHttpResponseHeader & /* response */)
{
    /* Abort connection as we already got all we need */
    m_pHttp->abortAll();
}

/* This function is used to ask the user about if he really want
 * to download file of proposed size if no error present or
 * abort download progress if error is present */
void UIDownloader::acknowledgeFinished(bool /* fError */)
{
    m_pHttp->disconnect(this);

    switch (m_pHttp->errorCode())
    {
        case QIHttp::NoError: /* full packet comes before aborting */
        case QIHttp::Aborted: /* part of packet comes before aborting */
        {
            /* Ask the user if he wish to download it */
            if (confirmDownload())
                QTimer::singleShot(0, this, SLOT(downloadStart()));
            else
                QTimer::singleShot(0, this, SLOT(suicide()));
            break;
        }
        case QIHttp::MovedPermanentlyError:
        case QIHttp::MovedTemporarilyError:
        {
            /* Restart downloading at new location */
            m_source = m_pHttp->lastResponse().value("location");
            QTimer::singleShot(0, this, SLOT(acknowledgeStart()));
            break;
        }
        default:
        {
            /* Show error happens during acknowledging */
            abortDownload(m_pHttp->errorString());
            break;
        }
    }
}

/* This function is used to start downloading mechanism:
 * downloading and saving the target */
void UIDownloader::downloadStart()
{
    delete m_pHttp;
    m_pHttp = new QIHttp(this, m_source.host());
    connect(m_pHttp, SIGNAL(dataReadProgress (int, int)),
            this, SLOT (downloadProcess(int, int)));
    connect(m_pHttp, SIGNAL(allIsDone(bool)),
            this, SLOT(downloadFinished(bool)));
    m_pHttp->get(m_source.toEncoded());
}

/* this function is used to observe the downloading progress through
 * changing the corresponding qprogressbar value */
void UIDownloader::downloadProcess(int cDone, int cTotal)
{
    emit sigDownloadProcess(cDone, cTotal);
}

/* This function is used to handle the 'downloading finished' issue
 * through saving the downloaded into the file if there in no error or
 * notifying the user about error happens */
void UIDownloader::downloadFinished(bool fError)
{
    m_pHttp->disconnect(this);

    if (fError)
    {
        /* Show information about error happens */
        if (m_pHttp->errorCode() == QIHttp::Aborted)
            abortDownload(tr("The download process has been canceled by the user."));
        else
            abortDownload(m_pHttp->errorString());
    }
    else
    {
        /* Trying to serialize the incoming buffer into the target, this is the
         * default behavior which have to be reimplemented in sub-class */
        QFile file(m_strTarget);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(m_pHttp->readAll());
            file.close();
        }
        QTimer::singleShot(0, this, SLOT(suicide()));
    }
}

/* This slot is used to process cancel-button clicking */
void UIDownloader::cancelDownloading()
{
    QTimer::singleShot(0, this, SLOT(suicide()));
}

/* This function is used to abort download by showing aborting reason
 * and calling the downloader's delete function */
void UIDownloader::abortDownload(const QString &strError)
{
    warnAboutError(strError);
    QTimer::singleShot(0, this, SLOT(suicide()));
}

/* This function is used to delete the downloader widget itself,
 * should be reimplemented to enhance necessary functionality in sub-class */
void UIDownloader::suicide()
{
    delete this;
}

