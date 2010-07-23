/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMPreviewWindow class declaration
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

#ifndef __UIVMPreviewWindow_h__
#define __UIVMPreviewWindow_h__

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

/* Global includes */
#include <QWidget>
#include <QHash>

/* Global forward declarations */
class QAction;
class QImage;
class QMenu;
class QTimer;

class UIVMPreviewWindow : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

    enum UpdateInterval
    {
        UpdateDisabled,
        Update1Sec,
        Update2Sec,
        Update5Sec,
        Update10Sec,
        UpdateEnd
    };

public:

    UIVMPreviewWindow(QWidget *pParent);
    ~UIVMPreviewWindow();

    void setMachine(const CMachine& machine);
    CMachine machine() const;

    QSize sizeHint() const;

protected:

    void retranslateUi();

    void resizeEvent(QResizeEvent *pEvent);
    void showEvent(QShowEvent *pEvent);
    void hideEvent(QHideEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);
    void contextMenuEvent(QContextMenuEvent *pEvent);

private slots:

    void sltMachineStateChange(QString strId, KMachineState state);
    void sltRecreatePreview();

private:

    void setUpdateInterval(UpdateInterval interval, bool fSave);
    void restart();
    void repaintBGImages();

    /* Private member vars */
    CSession m_session;
    CMachine m_machine;
    KMachineState m_machineState;
    QTimer *m_pUpdateTimer;
    QMenu *m_pUpdateTimerMenu;
    QHash<UpdateInterval, QAction*> m_actions;
    const int m_vMargin;
    QRect m_wRect;
    QRect m_vRect;
    QImage *m_pbgImage;
    QImage *m_pPreviewImg;
    QImage *m_pGlossyImg;
};

#endif /* !__UIVMPreviewWindow_h__ */

