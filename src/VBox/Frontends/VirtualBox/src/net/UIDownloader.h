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
#include <QWidget>
#include <QUrl>
#include <QPointer>
#include <QList>

/* Local includes: */
#include "UINetworkDefs.h"
#include "UINetworkCustomer.h"

/* Forward declarations: */
class QNetworkReply;

#if 0
/* Forward declarations: */
class QProgressBar;
class UIMiniCancelButton;

/**
 * The UIMiniProgressWidget class is QWidget class re-implementation which
 * embeds into the dialog's status-bar and reflects background http downloading.
 * This class is not supposed to be used itself and made for sub-classing only.
 */
class UIMiniProgressWidget : public QWidget
{
    Q_OBJECT;

signals:

    /* Signal to notify listeners about progress canceling: */
    void sigCancel();

protected:

    /* Constructor: */
    UIMiniProgressWidget(QWidget *pParent = 0);

    /* Source stuff: */
    QString source() const { return m_strSource; }

    /* Cancel-button stuff: */
    void setCancelButtonToolTip(const QString &strText);
    QString cancelButtonToolTip() const;

    /* Progress-bar stuff: */
    void setProgressBarToolTip(const QString &strText);
    QString progressBarToolTip() const;

protected slots:

    /* Slot to set source: */
    void sltSetSource(const QString &strSource);

    /* Slot to set progress: */
    void sltSetProgress(qint64 cDone, qint64 cTotal);

private:

    /* Private member vars: */
    QProgressBar *m_pProgressBar;
    UIMiniCancelButton *m_pCancelButton;
    QString m_strSource;
};
#endif

/**
 * The UIDownloader class is UINetworkCustomer class extension
 * which allows background http downloading.
 * This class is not supposed to be used itself and made for sub-classing only.
 */
class UIDownloader : public UINetworkCustomer
{
    Q_OBJECT;

signals:

    /* Signal to start acknowledging: */
    void sigToStartAcknowledging();
    /* Signal to start downloading: */
    void sigToStartDownloading();

#if 0
    /* Notifies listeners about source-change: */
    void sigSourceChanged(const QString &strNewSource);

    /* Notifies about downloading progress: */
    void sigDownloadProgress(qint64 cDone, qint64 cTotal);
#endif

public:

    /* Starting routine: */
    virtual void start();

private slots:

    /* Acknowledging part: */
    void sltStartAcknowledging();
    /* Downloading part: */
    void sltStartDownloading();

#if 0
    /* Common slots: */
    void sltCancel();
#endif

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

#if 0
    /* Pure virtual function to create UIMiniProgressWidget sub-class: */
    virtual UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const = 0;
    /* Create UIMiniProgressWidget for particular parent: */
    UIMiniProgressWidget* progressWidget(QWidget *pParent = 0) const;
#endif

private:

    /* Private variables: */
    UIDownloaderState m_state;
    QPointer<QWidget> m_pParent;
    QList<QUrl> m_sources;
    QUrl m_source;
    QString m_strTarget;
    QString m_strDescription;
};

#endif // __UIDownloader_h__

