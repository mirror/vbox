/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGMachinePreview class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGMachinePreview_h__
#define __UIGMachinePreview_h__

/* Qt includes: */
#include <QHash>

/* GUI includes: */
#include "UIGDetailsItem.h"
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
class UIGMachinePreview : public QIWithRetranslateUI4<QIGraphicsWidget>
{
    Q_OBJECT;

public:

    /* Graphics-item type: */
    enum { Type = UIGDetailsItemType_Preview };
    int type() const { return Type; }

    /* Constructor/destructor: */
    UIGMachinePreview(QIGraphicsWidget *pParent);
    ~UIGMachinePreview();

    /* API: Machine stuff: */
    void setMachine(const CMachine& machine);
    CMachine machine() const;

private slots:

    /* Handler: Global-event listener: */
    void sltMachineStateChange(QString strId, KMachineState state);

    /* Handler: Preview recreator: */
    void sltRecreatePreview();

private:

    /* Helpers: Event handlers: */
    void resizeEvent(QGraphicsSceneResizeEvent *pEvent);
    void showEvent(QShowEvent *pEvent);
    void hideEvent(QHideEvent *pEvent);
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);

    /* Helpers: Translate stuff; */
    void retranslateUi();

    /* Helpers: Layout stuff: */
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /* Helpers: Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);

    /* Helpers: Update stuff: */
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

#endif /* !__UIGMachinePreview_h__ */

