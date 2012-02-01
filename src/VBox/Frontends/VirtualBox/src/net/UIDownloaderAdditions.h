/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class declaration
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

#ifndef __UIDownloaderAdditions_h__
#define __UIDownloaderAdditions_h__

/* Local includes: */
#include "UIDownloader.h"

#if 0
/* Local includes: */
# include "QIWithRetranslateUI.h"

/**
 * The UIMiniProgressWidgetAdditions class is UIMiniProgressWidget class re-implementation
 * which embeds into the dialog's status-bar and reflects background http downloading.
 */
class UIMiniProgressWidgetAdditions : public QIWithRetranslateUI<UIMiniProgressWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMiniProgressWidgetAdditions(const QString &strSource, QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
    {
        sltSetSource(strSource);
        retranslateUi();
    }

protected:

    /* Translating stuff: */
    void retranslateUi()
    {
        setCancelButtonToolTip(tr("Cancel the VirtualBox Guest Additions CD image download"));
        setProgressBarToolTip(tr("Downloading the VirtualBox Guest Additions CD image from <nobr><b>%1</b>...</nobr>")
                                .arg(source()));
    }
};
#endif

/**
 * The UIDownloaderAdditions class is UIDownloader class extension
 * which allows background http downloading.
 */
class UIDownloaderAdditions : public UIDownloader
{
    Q_OBJECT;

signals:

    /* Notifies listeners about file was downloaded: */
    void sigDownloadFinished(const QString &strFile);

public:

    /* Static stuff: */
    static UIDownloaderAdditions* create();
    static UIDownloaderAdditions* current();

    /* Starting routine: */
    void start();

private:

    /* Constructor/destructor: */
    UIDownloaderAdditions();
    ~UIDownloaderAdditions();

    /* Virtual stuff reimplementations: */
    bool askForDownloadingConfirmation(QNetworkReply *pReply);
    void handleDownloadedObject(QNetworkReply *pReply);
#if 0
    UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const;
#endif

    /* Static instance variable: */
    static UIDownloaderAdditions *m_spInstance;
};

#endif // __UIDownloaderAdditions_h__

