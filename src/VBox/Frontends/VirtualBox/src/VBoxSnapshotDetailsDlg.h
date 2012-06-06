/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotDetailsDlg class declaration
 */

/*
 * Copyright (C) 2008-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxSnapshotDetailsDlg_h__
#define __VBoxSnapshotDetailsDlg_h__

/* GUI includes: */
#include "VBoxSnapshotDetailsDlg.gen.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "CSnapshot.h"

/* Forward declarations: */
class QScrollArea;

class VBoxSnapshotDetailsDlg : public QIWithRetranslateUI <QDialog>, public Ui::VBoxSnapshotDetailsDlg
{
    Q_OBJECT;

public:

    VBoxSnapshotDetailsDlg (QWidget *aParent);

    void getFromSnapshot (const CSnapshot &aSnapshot);
    void putBackToSnapshot();

protected:

    void retranslateUi();

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

private slots:

    void onNameChanged (const QString &aText);

private:

    CSnapshot mSnapshot;

    QPixmap mThumbnail;
    QPixmap mScreenshot;
};

class VBoxScreenshotViewer : public QIWithRetranslateUI2 <QWidget>
{
    Q_OBJECT;

public:

    VBoxScreenshotViewer (QWidget *aParent, const QPixmap &aScreenshot,
                          const QString &aSnapshotName, const QString &aMachineName);

private:

    void retranslateUi();

    void showEvent (QShowEvent *aEvent);
    void resizeEvent (QResizeEvent *aEvent);
    void mousePressEvent (QMouseEvent *aEvent);
    void keyPressEvent (QKeyEvent *aEvent);

    void adjustPicture();

    QScrollArea *mArea;
    QLabel *mPicture;

    QPixmap mScreenshot;
    QString mSnapshotName;
    QString mMachineName;

    bool mZoomMode;
};

#endif // __VBoxSnapshotDetailsDlg_h__

