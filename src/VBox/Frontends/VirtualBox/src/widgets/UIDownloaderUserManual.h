/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

    UIMiniProcessWidgetUserManual(const QString &strSource, QWidget *pParent = 0)
        : QIWithRetranslateUI<UIMiniProcessWidget>(pParent)
    {
        setSource(strSource);
        retranslateUi();
    }

protected:

    void retranslateUi()
    {
        setCancelButtonText(tr("Cancel"));
        setCancelButtonToolTip(tr("Cancel the VirtualBox User Manual download"));
        setProgressBarToolTip((tr("Downloading the VirtualBox User Manual "
                                  "<nobr><b>%1</b>...</nobr>")
                               .arg(source())));
    }
};

class UIDownloaderUserManual : public UIDownloader
{
    Q_OBJECT;

public:

    static UIDownloaderUserManual* create();
    static UIDownloaderUserManual* current();
    static void destroy();

    void setParentWidget(QWidget *pParent);
    QWidget *parentWidget() const;

    UIMiniProcessWidgetUserManual* processWidget(QWidget *pParent = 0) const;
    void startDownload();

signals:

    void downloadFinished(const QString &strFile);

private slots:

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
};

#endif // __UIDownloaderUserManual_h__

