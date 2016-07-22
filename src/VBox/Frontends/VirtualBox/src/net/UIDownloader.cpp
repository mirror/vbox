/* $Id$ */
/** @file
 * VBox Qt GUI - UIDownloader class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include <UINetworkReply.h>
# include "UIDownloader.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "VBoxUtils.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


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
    createNetworkRequest(UINetworkRequestType_HEAD, m_sources);
}

/* Downloading start: */
void UIDownloader::sltStartDownloading()
{
    /* Set state to acknowledging: */
    m_state = UIDownloaderState_Downloading;

    /* Send GET request: */
    createNetworkRequest(UINetworkRequestType_GET, QList<QUrl>() << m_source);
}

/* Constructor: */
UIDownloader::UIDownloader()
    : m_state(UIDownloaderState_Null)
{
    /* Connect listeners: */
    connect(this, SIGNAL(sigToStartAcknowledging()), this, SLOT(sltStartAcknowledging()), Qt::QueuedConnection);
    connect(this, SIGNAL(sigToStartDownloading()), this, SLOT(sltStartDownloading()), Qt::QueuedConnection);
}

/* virtual override */
const QString UIDownloader::description() const
{
    /* Look for known state: */
    switch (m_state)
    {
        case UIDownloaderState_Acknowledging: return tr("Looking for %1...");
        case UIDownloaderState_Downloading:   return tr("Downloading %1...");
        default:                              break;
    }
    /* Return null-string by default: */
    return QString();
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

