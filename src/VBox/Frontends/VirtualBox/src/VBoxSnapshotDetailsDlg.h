/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSnapshotDetailsDlg class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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


/** QWiget extension providing GUI with snapshot screenshot viewer widget. */
class UIScreenshotViewer : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs screenshow viewer passing @a pParent to the base-class.
      * @param  pixmapScreenshot  Brings the screenshot to show.
      * @param  strSnapshotName   Brings the snapshot name.
      * @param  strMachineName    Brings the machine name. */
    UIScreenshotViewer(const QPixmap &pixmapScreenshot,
                       const QString &strSnapshotName,
                       const QString &strMachineName,
                       QWidget *pParent = 0);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Handles mouse press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    /** Handles key press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Adjusts picture. */
    void adjustPicture();

    /** Holds whether this widget was polished. */
    bool m_fPolished;

    /** Holds the screenshot to show. */
    QPixmap m_pixmapScreenshot;
    /** Holds the snapshot name. */
    QString m_strSnapshotName;
    /** Holds the machine name. */
    QString m_strMachineName;

    /** Holds the scroll-area instance. */
    QScrollArea *m_pScrollArea;
    /** Holds the picture label instance. */
    QLabel *m_pLabelPicture;

    /** Holds whether we are in zoom mode. */
    bool m_fZoomMode;
};

#endif // __VBoxSnapshotDetailsDlg_h__

