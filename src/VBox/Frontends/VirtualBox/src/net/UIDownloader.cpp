/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QProgressBar>
#include <QNetworkReply>

/* Local includes: */
#include "UIDownloader.h"
#include "UINetworkManager.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISpecialControls.h"
#include "VBoxUtils.h"

/* UIMiniProgressWidget stuff: */
UIMiniProgressWidget::UIMiniProgressWidget(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pProgressBar(new QProgressBar(this))
    , m_pCancelButton(new UIMiniCancelButton(this))
{
    /* Progress-bar setup: */
    m_pProgressBar->setFixedWidth(100);
    m_pProgressBar->setFormat("%p%");
    m_pProgressBar->setValue(0);

    /* Cancel-button setup: */
    m_pCancelButton->setFocusPolicy(Qt::NoFocus);
    m_pCancelButton->removeBorder();
    connect(m_pCancelButton, SIGNAL(clicked()), this, SIGNAL(sigCancel()));

    setContentsMargins(0, 0, 0, 0);
    setFixedHeight(16);

    /* Layout setup: */
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

void UIMiniProgressWidget::setCancelButtonToolTip(const QString &strText)
{
    m_pCancelButton->setToolTip(strText);
}

QString UIMiniProgressWidget::cancelButtonToolTip() const
{
    return m_pCancelButton->toolTip();
}

void UIMiniProgressWidget::setProgressBarToolTip(const QString &strText)
{
    m_pProgressBar->setToolTip(strText);
}

QString UIMiniProgressWidget::progressBarToolTip() const
{
    return m_pProgressBar->toolTip();
}

void UIMiniProgressWidget::sltSetSource(const QString &strSource)
{
    m_strSource = strSource;
}

void UIMiniProgressWidget::sltSetProgress(qint64 cDone, qint64 cTotal)
{
    m_pProgressBar->setMaximum(cTotal);
    m_pProgressBar->setValue(cDone);
}

/* UIDownloader stuff: */
UIMiniProgressWidget* UIDownloader::progressWidget(QWidget *pParent) const
{
    /* Create progress widget: */
    UIMiniProgressWidget *pWidget = createProgressWidgetFor(pParent);

    /* Connect the signal to notify about progress canceled: */
    connect(pWidget, SIGNAL(sigCancel()), this, SLOT(sltCancel()));
    /* Connect the signal to notify about source changed: */
    connect(this, SIGNAL(sigSourceChanged(const QString&)), pWidget, SLOT(sltSetSource(const QString&)));
    /* Connect the signal to notify about downloading progress: */
    connect(this, SIGNAL(sigDownloadProgress(qint64, qint64)), pWidget, SLOT(sltSetProgress(qint64, qint64)));
    /* Make sure the widget is destroyed when this class is deleted: */
    connect(this, SIGNAL(destroyed(QObject*)), pWidget, SLOT(deleteLater()));

    /* Return widget: */
    return pWidget;
}

void UIDownloader::start()
{
    startDelayedAcknowledging();
}

UIDownloader::UIDownloader()
{
    connect(this, SIGNAL(sigToStartAcknowledging()), this, SLOT(sltStartAcknowledging()), Qt::QueuedConnection);
    connect(this, SIGNAL(sigToStartDownloading()), this, SLOT(sltStartDownloading()), Qt::QueuedConnection);
}

/* Start acknowledging: */
void UIDownloader::sltStartAcknowledging()
{
    /* Setup HEAD request: */
    QNetworkRequest request;
    request.setUrl(m_source);
    QNetworkReply *pReply = gNetworkManager->head(request);
    connect(pReply, SIGNAL(sslErrors(QList<QSslError>)), pReply, SLOT(ignoreSslErrors()));
    connect(pReply, SIGNAL(finished()), this, SLOT(sltFinishAcknowledging()));
}

/* Finish acknowledging: */
void UIDownloader::sltFinishAcknowledging()
{
    /* Get corresponding network reply object: */
    QNetworkReply *pReply = qobject_cast<QNetworkReply*>(sender());
    /* And ask it for suicide: */
    pReply->deleteLater();

    /* Handle normal reply: */
    if (pReply->error() == QNetworkReply::NoError)
        handleAcknowledgingResult(pReply);
    /* Handle errors: */
    else
        handleError(pReply);
}

/* Handle acknowledging result: */
void UIDownloader::handleAcknowledgingResult(QNetworkReply *pReply)
{
    /* Check if redirection required: */
    QUrl redirect = pReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid())
    {
        /* Set new source: */
        UIDownloader::setSource(redirect.toString());
        /* Start redirecting: */
        startDelayedAcknowledging();
        return;
    }

    /* Ask for downloading confirmation: */
    if (askForDownloadingConfirmation(pReply))
    {
        /* Start downloading: */
        startDelayedDownloading();
        return;
    }

    /* Delete downloader: */
    deleteLater();
}

/* Start downloading: */
void UIDownloader::sltStartDownloading()
{
    /* Setup GET request: */
    QNetworkRequest request;
    request.setUrl(m_source);
    QNetworkReply *pReply = gNetworkManager->get(request);
    connect(pReply, SIGNAL(sslErrors(QList<QSslError>)), pReply, SLOT(ignoreSslErrors()));
    connect(pReply, SIGNAL(downloadProgress(qint64, qint64)), this, SIGNAL(sigDownloadProgress(qint64, qint64)));
    connect(pReply, SIGNAL(finished()), this, SLOT(sltFinishDownloading()));
}

/* Finish downloading: */
void UIDownloader::sltFinishDownloading()
{
    /* Get corresponding network reply object: */
    QNetworkReply *pReply = qobject_cast<QNetworkReply*>(sender());
    /* And ask it for suicide: */
    pReply->deleteLater();

    /* Handle normal reply: */
    if (pReply->error() == QNetworkReply::NoError)
        handleDownloadingResult(pReply);
    /* Handle errors: */
    else
        handleError(pReply);
}

/* Handle downloading result: */
void UIDownloader::handleDownloadingResult(QNetworkReply *pReply)
{
    /* Handle downloaded object: */
    handleDownloadedObject(pReply);

    /* Delete downloader: */
    deleteLater();
}

/* Handle simple errors: */
void UIDownloader::handleError(QNetworkReply *pReply)
{
    /* Show error message: */
    warnAboutNetworkError(pReply->errorString());

    /* Delete downloader: */
    deleteLater();
}

/* Cancel-button stuff: */
void UIDownloader::sltCancel()
{
    /* Delete downloader: */
    deleteLater();
}

