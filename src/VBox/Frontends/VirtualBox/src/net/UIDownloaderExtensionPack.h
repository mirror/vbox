/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader for extension pack
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include "QIWithRetranslateUI.h"
#include "UIDownloader.h"

/* UIMiniProgressWidget reimplementation for the VirtualBox extension pack downloading: */
class UIMiniProgressWidgetExtension : public QIWithRetranslateUI<UIMiniProgressWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMiniProgressWidgetExtension(const QString &strSource, QWidget *pParent = 0);

private:

    /* Translating stuff: */
    void retranslateUi();
};

/* UIDownloader reimplementation for the VirtualBox Extension Pack updating: */
class UIDownloaderExtensionPack : public UIDownloader
{
    Q_OBJECT;

public:

    /* Create downloader: */
    static UIDownloaderExtensionPack* create();
    /* Return downloader: */
    static UIDownloaderExtensionPack* current();

    /* Starts downloading: */
    void start();

signals:

    /* Notify listeners about extension pack downloaded: */
    void sigNotifyAboutExtensionPackDownloaded(const QString &strSource, const QString &strTarget);

private:

    /* Constructor/destructor: */
    UIDownloaderExtensionPack();
    ~UIDownloaderExtensionPack();

    /* Virtual methods reimplementations: */
    UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const;
    bool askForDownloadingConfirmation(QNetworkReply *pReply);
    void handleDownloadedObject(QNetworkReply *pReply);
    void warnAboutNetworkError(const QString &strError);

    /* Variables: */
    static UIDownloaderExtensionPack *m_pInstance;
};

#endif // __UIDownloaderExtensionPack_h__
