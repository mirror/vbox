/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader class declaration
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

#ifndef __UIDownloader_h__
#define __UIDownloader_h__

/* Global includes: */
#include <QWidget>
#include <QUrl>
#include <QPointer>

/* Forward declarations: */
class QProgressBar;
class UIMiniCancelButton;
class QNetworkReply;

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

/**
 * The UIDownloader class is QObject class re-implementation which
 * allows background http downloading.
 * This class is not supposed to be used itself and made for sub-classing only.
 *
 * This class has two parts:
 * 1. Acknowledging (getting information about target presence and size).
 * 2. Downloading (starting and handling file downloading process).
 * Every subclass can determine using or not those two parts and handling
 * the result of those parts itself.
 */
class UIDownloader : public QObject
{
    Q_OBJECT;

public:

    /* Parent stuff: */
    virtual void setParentWidget(QWidget *pParent) { m_pParent = pParent; }
    virtual QWidget* parentWidget() const { return m_pParent; }

    /* Source stuff: */
    virtual void setSource(const QString &strSource) { m_source = strSource; }
    virtual QString source() const { return m_source.toString(); }

    /* Target stuff: */
    virtual void setTarget(const QString &strTarget) { m_strTarget = strTarget; }
    virtual QString target() const { return m_strTarget; }

    /* Create UIMiniProgressWidget for particular parent: */
    UIMiniProgressWidget* progressWidget(QWidget *pParent = 0) const;

    /* Starting stuff: */
    virtual void start();

signals:

    /* Signal to notify listeners about source-change: */
    void sigSourceChanged(const QString &strNewSource);

    /* Signal to start acknowledging: */
    void sigToStartAcknowledging();
    /* Signal to start downloading: */
    void sigToStartDownloading();

    /* Notifies about downloading progress: */
    void sigDownloadProgress(qint64 cDone, qint64 cTotal);

protected:

    /* Constructor: */
    UIDownloader();

    /* Start delayed acknowledging: */
    void startDelayedAcknowledging() { emit sigToStartAcknowledging(); }
    /* Start delayed downloading: */
    void startDelayedDownloading() { emit sigToStartDownloading(); }

    /* Handle acknowledging result: */
    virtual void handleAcknowledgingResult(QNetworkReply *pReply);
    /* Handle downloading result: */
    virtual void handleDownloadingResult(QNetworkReply *pReply);
    /* Handle errors: */
    virtual void handleError(QNetworkReply *pReply);

    /* Pure virtual function to create UIMiniProgressWidget sub-class: */
    virtual UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const = 0;
    /* Pure virtual function to ask user about downloading confirmation: */
    virtual bool askForDownloadingConfirmation(QNetworkReply *pReply) = 0;
    /* Pure virtual function to handle downloaded object: */
    virtual void handleDownloadedObject(QNetworkReply *pReply) = 0;
    /* Pure virtual function to warn user about issues: */
    virtual void warnAboutNetworkError(const QString &strError) = 0;

private slots:

    /* Acknowledging part: */
    void sltStartAcknowledging();
    void sltFinishAcknowledging();

    /* Downloading part: */
    void sltStartDownloading();
    void sltFinishDownloading();

    /* Common slots: */
    void sltCancel();

private:

    /* Private variables: */
    QUrl m_source;
    QString m_strTarget;
    QPointer<QWidget> m_pParent;
};

#endif // __UIDownloader_h__

