/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class declaration
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include "UIDownloader.h"

#if 0
/* Local includes: */
# include "QIWithRetranslateUI.h"

/**
 * The UIMiniProcessWidgetUserManual class is UIMiniProgressWidget class re-implementation
 * which embeds into the dialog's status-bar and reflects background http downloading.
 */
class UIMiniProcessWidgetUserManual : public QIWithRetranslateUI<UIMiniProgressWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMiniProcessWidgetUserManual(QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
    {
        retranslateUi();
    }

private slots:

    /* Source change stuff: */
    void sltSetSource(const QString &strSource)
    {
        UIMiniProgressWidget::sltSetSource(strSource);
        retranslateUi();
    }

private:

    /* Translating stuff: */
    void retranslateUi()
    {
        setCancelButtonToolTip(tr("Cancel the VirtualBox User Manual download"));
        setProgressBarToolTip(source().isEmpty() ? tr("Downloading the VirtualBox User Manual") :
                                                   tr("Downloading the VirtualBox User Manual <nobr><b>%1</b>...</nobr>")
                                                     .arg(source()));
    }
};
#endif

/**
 * The UIDownloaderUserManual class is UIDownloader class extension
 * which allows background http downloading.
 */
class UIDownloaderUserManual : public UIDownloader
{
    Q_OBJECT;

signals:

    /* Notifies listeners about file was downloaded: */
    void sigDownloadFinished(const QString &strFile);

public:

    /* Static stuff: */
    static UIDownloaderUserManual* create();
    static UIDownloaderUserManual* current();

    /* Starting routine: */
    void start();

private:

    /* Constructor/destructor: */
    UIDownloaderUserManual();
    ~UIDownloaderUserManual();

    /* Virtual stuff reimplementations: */
    bool askForDownloadingConfirmation(QNetworkReply *pReply);
    void handleDownloadedObject(QNetworkReply *pReply);
#if 0
    UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const;
#endif

    /* Static instance variable: */
    static UIDownloaderUserManual *m_spInstance;
};

#endif // __UIDownloaderUserManual_h__

