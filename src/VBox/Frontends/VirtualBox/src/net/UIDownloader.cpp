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

/* GUI includes: */
#include <UINetworkReply.h>
#include "UIDownloader.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"

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

/* Constructor: */
UIDownloader::UIDownloader()
    : m_state(UIDownloaderState_Null)
{
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
}

/* Network-reply canceled handler: */
void UIDownloader::processNetworkReplyCanceled(UINetworkReply *pNetworkReply)
{
    /* Unused variables: */
    Q_UNUSED(pNetworkReply);

    /* Delete downloader: */
    deleteLater();
}

/* Network-reply finished handler: */
void UIDownloader::processNetworkReplyFinished(UINetworkReply *pNetworkReply)
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
void UIDownloader::handleAcknowledgingResult(UINetworkReply *pNetworkReply)
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
void UIDownloader::handleDownloadingResult(UINetworkReply *pNetworkReply)
{
    /* Handle downloaded object: */
    handleDownloadedObject(pNetworkReply);

    /* Delete downloader: */
    deleteLater();
}

