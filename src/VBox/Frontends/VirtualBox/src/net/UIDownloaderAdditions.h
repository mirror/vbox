/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class declaration
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

#ifndef __UIDownloaderAdditions_h__
#define __UIDownloaderAdditions_h__

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "UIDownloader.h"

class UIMiniProgressWidgetAdditions : public QIWithRetranslateUI<UIMiniProgressWidget>
{
    Q_OBJECT;

public:

    UIMiniProgressWidgetAdditions(const QString &strSource, QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
    {
        sltSetSource(strSource);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setCancelButtonToolTip(tr("Cancel the VirtualBox Guest Additions CD image download"));
        setProgressBarToolTip(tr("Downloading the VirtualBox Guest Additions CD image from <nobr><b>%1</b>...</nobr>")
                                .arg(source()));
    }
};

class UIDownloaderAdditions : public UIDownloader
{
    Q_OBJECT;

public:

    static UIDownloaderAdditions* create();
    static UIDownloaderAdditions* current();

    void setAction(QAction *pAction);
    QAction *action() const;

signals:

    void sigDownloadFinished(const QString &strFile);

private:

    UIDownloaderAdditions();
    ~UIDownloaderAdditions();

    UIMiniProgressWidget* createProgressWidgetFor(QWidget *pParent) const;
    bool askForDownloadingConfirmation(QNetworkReply *pReply);
    void handleDownloadedObject(QNetworkReply *pReply);
    void warnAboutNetworkError(const QString &strError);

    /* Private member variables: */
    static UIDownloaderAdditions *m_pInstance;

    /* Action to be blocked: */
    QPointer<QAction> m_pAction;
};

#endif // __UIDownloaderAdditions_h__

