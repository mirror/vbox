/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class declaration
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UIDownloaderAdditions_h__
#define __UIDownloaderAdditions_h__

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "UIDownloader.h"

/* Global includes */
#include <QPointer>

class UIMiniProcessWidgetAdditions : public QIWithRetranslateUI<UIMiniProcessWidget>
{
    Q_OBJECT;

public:

    UIMiniProcessWidgetAdditions(const QString &strSource, QWidget *pParent = 0)
      : QIWithRetranslateUI<UIMiniProcessWidget>(pParent)
    {
        setSource(strSource);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setCancelButtonText(tr("Cancel"));
        setCancelButtonToolTip(tr("Cancel the VirtualBox Guest "
                                  "Additions CD image download"));
        setProgressBarToolTip((tr("Downloading the VirtualBox Guest Additions "
                                  "CD image from <nobr><b>%1</b>...</nobr>")
                               .arg(source())));
    }
};

class UIDownloaderAdditions : public UIDownloader
{
    Q_OBJECT;

public:
    static UIDownloaderAdditions* create();
    static UIDownloaderAdditions* current();
    static void destroy();

    void setAction(QAction *pAction);
    QAction *action() const;

    void setParentWidget(QWidget *pParent);
    QWidget *parentWidget() const;

    UIMiniProcessWidgetAdditions* processWidget(QWidget *pParent = 0) const;
    void startDownload();

signals:
    void downloadFinished(const QString &strFile);

private slots:

    void downloadFinished(bool fError);
    void suicide();

private:

    UIDownloaderAdditions();

    bool confirmDownload();
    void warnAboutError(const QString &strError);

    /* Private member vars */
    static UIDownloaderAdditions *m_pInstance;

    /* We use QPointer here, cause these items could be deleted in the life of
     * this object. QPointer guarantees that the ptr itself is zero in that
     * case. */
    QPointer<QAction> m_pAction;
    QPointer<QWidget> m_pParent;
};

#endif // __UIDownloaderAdditions_h__

