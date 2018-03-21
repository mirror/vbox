/* $Id$ */
/** @file
 * VBox Qt GUI - UIDownloader class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDownloader_h___
#define ___UIDownloader_h___

/* Qt includes: */
#include <QUrl>
#include <QList>

/* GUI includes: */
#include "UINetworkCustomer.h"
#include "UINetworkDefs.h"

/* Forward declarations: */
class QString;
class UINetworkReply;

/** Downloader interface.
  * UINetworkCustomer class extension which allows background http downloading. */
class UIDownloader : public UINetworkCustomer
{
    Q_OBJECT;

signals:

    /** Signals to start acknowledging. */
    void sigToStartAcknowledging();
    /** Signals to start downloading. */
    void sigToStartDownloading();
    /** Signals to start verifying. */
    void sigToStartVerifying();

public:

    /** Starts the sequence. */
    void start();

protected slots:

    /** Performs acknowledging part. */
    void sltStartAcknowledging();
    /** Performs downloading part. */
    void sltStartDownloading();
    /** Performs verifying part. */
    void sltStartVerifying();

protected:

    /** UIDownloader states. */
    enum UIDownloaderState
    {
        UIDownloaderState_Null,
        UIDownloaderState_Acknowledging,
        UIDownloaderState_Downloading,
        UIDownloaderState_Verifying
    };

    /** Constructs downloader. */
    UIDownloader();

    /** Appends subsequent source to try to download from. */
    void addSource(const QString &strSource) { m_sources << QUrl(strSource); }
    /** Defines the only one source to try to download from. */
    void setSource(const QString &strSource) { m_sources.clear(); addSource(strSource); }
    /** Returns a list of sources to try to download from. */
    const QList<QUrl> &sources() const { return m_sources; }
    /** Returns a current source to try to download from. */
    const QUrl &source() const { return m_source; }

    /** Defines the @a strTarget file-path used to save downloaded file to. */
    void setTarget(const QString &strTarget) { m_strTarget = strTarget; }
    /** Returns the target file-path used to save downloaded file to. */
    const QString &target() const { return m_strTarget; }

    /** Defines the @a strPathSHA256SumsFile. */
    void setPathSHA256SumsFile(const QString &strPathSHA256SumsFile) { m_strPathSHA256SumsFile = strPathSHA256SumsFile; }
    /** Returns the SHA-256 sums file-path. */
    QString pathSHA256SumsFile() const { return m_strPathSHA256SumsFile; }

    /** Returns description of the current network operation. */
    virtual const QString description() const;

    /** Starts delayed acknowledging. */
    void startDelayedAcknowledging() { emit sigToStartAcknowledging(); }
    /** Starts delayed downloading. */
    void startDelayedDownloading() { emit sigToStartDownloading(); }
    /** Starts delayed verifying. */
    void startDelayedVerifying() { emit sigToStartVerifying(); }

    /** Handles network-reply progress for @a iReceived bytes of @a iTotal. */
    void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal);
    /** Handles network-reply cancel request for @a pReply. */
    void processNetworkReplyCanceled(UINetworkReply *pReply);
    /** Handles network-reply finish for @a pReply. */
    void processNetworkReplyFinished(UINetworkReply *pReply);

    /** Handles acknowledging result. */
    virtual void handleAcknowledgingResult(UINetworkReply *pReply);
    /** Handles downloading result. */
    virtual void handleDownloadingResult(UINetworkReply *pReply);
    /** Handles verifying result. */
    virtual void handleVerifyingResult(UINetworkReply *pReply);

    /** Asks user for downloading confirmation for passed @a pReply. */
    virtual bool askForDownloadingConfirmation(UINetworkReply *pReply) = 0;
    /** Handles downloaded object for passed @a pReply. */
    virtual void handleDownloadedObject(UINetworkReply *pReply) = 0;
    /** Handles verified object for passed @a pReply. */
    virtual void handleVerifiedObject(UINetworkReply *pReply) { Q_UNUSED(pReply); }

private:

    /** Holds the downloader state. */
    UIDownloaderState m_state;

    /** Holds the downloading sources. */
    QList<QUrl> m_sources;
    /** Holds the current downloading source. */
    QUrl        m_source;

    /** Holds the downloading target path. */
    QString m_strTarget;

    /** Holds the SHA-256 sums file path. */
    QString m_strPathSHA256SumsFile;
};

#endif /* !___UIDownloader_h___ */

