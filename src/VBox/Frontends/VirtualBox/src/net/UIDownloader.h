/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader class declaration
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

#ifndef __UIDownloader_h__
#define __UIDownloader_h__

/* Global includes: */
#include <QUrl>
#include <QList>

/* Local includes: */
#include "UINetworkDefs.h"
#include "UINetworkCustomer.h"

/* Forward declarations: */
class QNetworkReply;

/* Downloader interface.
 * UINetworkCustomer class extension which allows background http downloading. */
class UIDownloader : public UINetworkCustomer
{
    Q_OBJECT;

signals:

    /* Signal to start acknowledging: */
    void sigToStartAcknowledging();
    /* Signal to start downloading: */
    void sigToStartDownloading();

public:

    /* Starting routine: */
    void start();

protected slots:

    /* Acknowledging part: */
    void sltStartAcknowledging();
    /* Downloading part: */
    void sltStartDownloading();

protected:

    /* UIDownloader states: */
    enum UIDownloaderState
    {
        UIDownloaderState_Null,
        UIDownloaderState_Acknowledging,
        UIDownloaderState_Downloading
    };

    /* Constructor: */
    UIDownloader();

    /* Source stuff,
     * allows to set/get one or more sources to try to download from: */
    void addSource(const QString &strSource) { m_sources << QUrl(strSource); }
    void setSource(const QString &strSource) { m_sources.clear(); addSource(strSource); }
    const QList<QUrl>& sources() const { return m_sources; }
    const QUrl& source() const { return m_source; }

    /* Target stuff,
     * allows to set/get downloaded file destination: */
    void setTarget(const QString &strTarget) { m_strTarget = strTarget; }
    const QString& target() const { return m_strTarget; }

    /* Description stuff,
     * allows to set/get Network Customer description for Network Access Manager: */
    void setDescription(const QString &strDescription) { m_strDescription = strDescription; }

    /* Start delayed acknowledging: */
    void startDelayedAcknowledging() { emit sigToStartAcknowledging(); }
    /* Start delayed downloading: */
    void startDelayedDownloading() { emit sigToStartDownloading(); }

    /* Network-reply progress handler: */
    void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal);
    /* Network-reply cancel handler: */
    void processNetworkReplyCanceled(QNetworkReply *pNetworkReply);
    /* Network-reply finish handler: */
    void processNetworkReplyFinished(QNetworkReply *pNetworkReply);

    /* Handle acknowledging result: */
    virtual void handleAcknowledgingResult(QNetworkReply *pNetworkReply);
    /* Handle downloading result: */
    virtual void handleDownloadingResult(QNetworkReply *pNetworkReply);

    /* Pure virtual function to ask user about downloading confirmation: */
    virtual bool askForDownloadingConfirmation(QNetworkReply *pNetworkReply) = 0;
    /* Pure virtual function to handle downloaded object: */
    virtual void handleDownloadedObject(QNetworkReply *pNetworkReply) = 0;

private:

    /* Private variables: */
    UIDownloaderState m_state;
    QList<QUrl> m_sources;
    QUrl m_source;
    QString m_strTarget;
    QString m_strDescription;
};

#endif // __UIDownloader_h__

