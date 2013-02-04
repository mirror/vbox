/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMPreviewWindow class declaration
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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

/* Qt includes: */
#include <QWidget>
#include <QHash>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"
#include "CMachine.h"

/* Forward declarations: */
class QAction;
class QImage;
class QMenu;
class QTimer;

/* Update interval type: */
enum UpdateInterval
{
    UpdateInterval_Disabled,
    UpdateInterval_500ms,
    UpdateInterval_1000ms,
    UpdateInterval_2000ms,
    UpdateInterval_5000ms,
    UpdateInterval_10000ms,
    UpdateInterval_Max
};
typedef QMap<UpdateInterval, QString> UpdateIntervalMap;

/* Preview window class: */
class UIVMPreviewWindow : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

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
    void repaintBGImages();

    void restart();
    void stop();

    /* Variables: */
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

    static UpdateIntervalMap m_intervals;
};

#endif /* !__UIVMPreviewWindow_h__ */

