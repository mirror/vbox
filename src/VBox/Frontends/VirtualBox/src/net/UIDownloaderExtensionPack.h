/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderExtensionPack class declaration
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIDownloaderExtensionPack_h__
#define __UIDownloaderExtensionPack_h__

/* Local includes: */
#include "UIDownloader.h"

#if 0
/* Local includes: */
# include "QIWithRetranslateUI.h"

/**
 * The UIMiniProgressWidgetExtension class is UIMiniProgressWidget class re-implementation
 * which embeds into the dialog's status-bar and reflects background http downloading.
 */
class UIMiniProgressWidgetExtension : public QIWithRetranslateUI<UIMiniProgressWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMiniProgressWidgetExtension(const QString &strSource, QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
    {
        sltSetSource(strSource);
        retranslateUi();
    }

private:

    /* Translating stuff: */
    void retranslateUi()
    {
        setCancelButtonToolTip(tr("Cancel the <nobr><b>%1</b></nobr> download").arg(UI_ExtPackName));
        setProgressBarToolTip(tr("Downloading the <nobr><b>%1</b></nobr> from <nobr><b>%2</b>...</nobr>")
                                 .arg(UI_ExtPackName, source()));
    }
};
#endif

/**
 * The UIDownloaderExtensionPack class is UIDownloader class extension
 * which allows background http downloading.
 */
class UIDownloaderExtensionPack : public UIDownloader
{
    Q_OBJECT;

signals:

    /* Notify listeners about file was downloaded: */
    void sigDownloadFinished(const QString &strSource, const QString &strTarget, QString strHash);

public:

    /* Static stuff: */
    static UIDownloaderExtensionPack* create();
    static UIDownloaderExtensionPack* current();

    /* Starting routine: */
    void start();

private:

    /* Constructor/destructor: */
    UIDownloaderExtensionPack();
    ~UIDownloaderExtensionPack();

    /* Virtual stuff reimplementations: */
    bool askForDownloadingConfirmation(QNetworkReply *pReply);
    void handleDownloadedObject(QNetworkReply *pReply);
#if 0
    UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const;
#endif

    /* Static instance variable: */
    static UIDownloaderExtensionPack *m_spInstance;
};

#endif // __UIDownloaderExtensionPack_h__
