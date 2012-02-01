/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <QNetworkReply>

/* Local includes: */
#include "UIDownloader.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"

#if 0
/* Global includes: */
#include <QProgressBar>

/* Local includes: */
#include "UISpecialControls.h"

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
#endif

/* Starting routine: */
void UIDownloader::start()
{
    startDelayedAcknowledging();
}

/* Acknowledging start: */
void UIDownloader::sltStartAcknowledging()
{
    /* Set state to acknowledging: */
    m_state = UIDownloaderState_Acknowledging;

    /* Send HEAD requests: */
    QList<QNetworkRequest> requests;
    for (int i = 0; i < m_sources.size(); ++i)
        requests << QNetworkRequest(m_sources[i]);

    /* Create network request set: */
    createNetworkRequest(requests, UINetworkRequestType_HEAD, tr("Looking for %1...").arg(m_strDescription));
}

/* Downloading start: */
void UIDownloader::sltStartDownloading()
{
    /* Set state to acknowledging: */
    m_state = UIDownloaderState_Downloading;

    /* Send GET request: */
    QNetworkRequest request(m_source);

    /* Create network request: */
    createNetworkRequest(request, UINetworkRequestType_GET, tr("Downloading %1...").arg(m_strDescription));
}

#if 0
/* Cancel-button stuff: */
void UIDownloader::sltCancel()
{
    /* Delete downloader: */
    deleteLater();
}
#endif

/* Constructor: */
UIDownloader::UIDownloader()
{
    /* Choose initial state: */
    m_state = UIDownloaderState_Null;

    /* Connect listeners: */
    connect(this, SIGNAL(sigToStartAcknowledging()), this, SLOT(sltStartAcknowledging()), Qt::QueuedConnection);
    connect(this, SIGNAL(sigToStartDownloading()), this, SLOT(sltStartDownloading()), Qt::QueuedConnection);
}

/* Network-reply progress handler: */
void UIDownloader::processNetworkReplyProgress(qint64 iReceived, qint64 iTotal)
{
    /* Unused variables: */
    Q_UNUSED(iReceived);
    Q_UNUSED(iTotal);

#if 0
    emit sigDownloadProgress(iReceived, iTotal);
#endif
}

/* Network-reply canceled handler: */
void UIDownloader::processNetworkReplyCanceled(QNetworkReply *pNetworkReply)
{
    /* Unused variables: */
    Q_UNUSED(pNetworkReply);

    /* Delete downloader: */
    deleteLater();
}

/* Network-reply finished handler: */
void UIDownloader::processNetworkReplyFinished(QNetworkReply *pNetworkReply)
{
    /* Process reply: */
    switch (m_state)
    {
        case UIDownloaderState_Acknowledging:
        {
            handleAcknowledgingResult(pNetworkReply);
            break;
        }
        case UIDownloaderState_Downloading:
        {
            handleDownloadingResult(pNetworkReply);
            break;
        }
        default:
            break;
    }
}

/* Handle acknowledging result: */
void UIDownloader::handleAcknowledgingResult(QNetworkReply *pNetworkReply)
{
    /* Get the final source: */
    m_source = pNetworkReply->url();

    /* Ask for downloading: */
    if (askForDownloadingConfirmation(pNetworkReply))
    {
        /* Start downloading: */
        startDelayedDownloading();
    }
    else
    {
        /* Delete downloader: */
        deleteLater();
    }
}

/* Handle downloading result: */
void UIDownloader::handleDownloadingResult(QNetworkReply *pNetworkReply)
{
    /* Handle downloaded object: */
    handleDownloadedObject(pNetworkReply);

    /* Delete downloader: */
    deleteLater();
}

#if 0
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
#endif

