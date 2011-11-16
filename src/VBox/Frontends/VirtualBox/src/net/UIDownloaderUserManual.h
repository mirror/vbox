/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class declaration
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIDownloaderUserManual_h__
#define __UIDownloaderUserManual_h__

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIDownloader.h"

class UIMiniProcessWidgetUserManual : public QIWithRetranslateUI<UIMiniProgressWidget>
{
    Q_OBJECT;

public:

    UIMiniProcessWidgetUserManual(QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
    {
        retranslateUi();
    }

private slots:

    void sltSetSource(const QString &strSource)
    {
        UIMiniProgressWidget::sltSetSource(strSource);
        retranslateUi();
    }

private:

    void retranslateUi()
    {
        setCancelButtonToolTip(tr("Cancel the VirtualBox User Manual download"));
        setProgressBarToolTip(source().isEmpty() ? tr("Downloading the VirtualBox User Manual") :
                                                   tr("Downloading the VirtualBox User Manual <nobr><b>%1</b>...</nobr>")
                                                     .arg(source()));
    }
};

class UIDownloaderUserManual : public UIDownloader
{
    Q_OBJECT;

public:

    static UIDownloaderUserManual* create();
    static UIDownloaderUserManual* current();

    void setSource(const QString &strSource);
    void addSource(const QString &strSource);

    void start();

signals:

    void sigSourceChanged(const QString &strSource);
    void sigDownloadFinished(const QString &strFile);

private:

    UIDownloaderUserManual();
    ~UIDownloaderUserManual();

    void startDownloading();

    void handleError(QNetworkReply *pReply);

    UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const;
    bool askForDownloadingConfirmation(QNetworkReply *pReply);
    void handleDownloadedObject(QNetworkReply *pReply);
    void warnAboutNetworkError(const QString &strError);

    /* Private member variables: */
    static UIDownloaderUserManual *m_pInstance;

    /* List of sources to try to download from: */
    QList<QString> m_sourcesList;
};

#endif // __UIDownloaderUserManual_h__

