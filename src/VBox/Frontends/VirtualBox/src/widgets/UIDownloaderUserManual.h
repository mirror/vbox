/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

/* Global includes */
#include <QPointer>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "UIDownloader.h"

class UIMiniProcessWidgetUserManual : public QIWithRetranslateUI<UIMiniProcessWidget>
{
    Q_OBJECT;

public:

    UIMiniProcessWidgetUserManual(QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProcessWidget>(pParent)
    {
        retranslateUi();
    }

protected slots:

    void sltSetSource(const QString &strSource)
    {
        setSource(strSource);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setCancelButtonText(tr("Cancel"));
        setCancelButtonToolTip(tr("Cancel the VirtualBox User Manual download"));
        QString strProgressBarTip = source().isEmpty() ? tr("Downloading the VirtualBox User Manual") :
            tr("Downloading the VirtualBox User Manual <nobr><b>%1</b>...</nobr>").arg(source());
        setProgressBarToolTip(strProgressBarTip);
    }
};

class UIDownloaderUserManual : public UIDownloader
{
    Q_OBJECT;

public:

    static UIDownloaderUserManual* create();
    static UIDownloaderUserManual* current();
    static void destroy();

    void setSource(const QString &strSource);
    void addSource(const QString &strSource);

    void setParentWidget(QWidget *pParent);
    QWidget *parentWidget() const;

    UIMiniProcessWidgetUserManual* processWidget(QWidget *pParent = 0) const;
    void startDownload();

signals:

    void sigSourceChanged(const QString &strSource);
    void sigDownloadFinished(const QString &strFile);

private slots:

    void acknowledgeFinished(bool fError);
    void downloadFinished(bool fError);
    void suicide();

private:

    UIDownloaderUserManual();

    bool confirmDownload();
    void warnAboutError(const QString &strError);

    /* Private member variables: */
    static UIDownloaderUserManual *m_pInstance;

    /* We use QPointer here, cause these items could be deleted in the life of this object.
     * QPointer guarantees that the ptr itself is zero in that case. */
    QPointer<QWidget> m_pParent;

    /* List of sources to try to download from: */
    QList<QString> m_sourcesList;
};

#endif // __UIDownloaderUserManual_h__

