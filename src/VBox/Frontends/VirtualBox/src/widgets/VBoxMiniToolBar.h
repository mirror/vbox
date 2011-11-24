/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMiniToolBar class declaration & implementation. This is the toolbar shown on fullscreen mode.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxMiniToolBar_h__
#define __VBoxMiniToolBar_h__

/* Global includes */
#include <QBasicTimer>

/* Local includes */
#include "UIToolBar.h"

/* Global forwards */
class QLabel;
class QMenu;

/**
 *  The VBoxMiniToolBar class is a toolbar shown inside full screen mode or seamless mode.
 *  It supports auto hiding and animated sliding up/down.
 */
class VBoxMiniToolBar : public UIToolBar
{
    Q_OBJECT;

public:

    enum Alignment
    {
        AlignTop,
        AlignBottom
    };

    VBoxMiniToolBar(QWidget *pParent, Alignment alignment, bool fActive, bool fAutoHide);

    VBoxMiniToolBar& operator<<(QList<QMenu*> menus);

    void setSeamlessMode(bool fSeamless);
    void setDisplayText(const QString &strText);

    bool isAutoHide() const;

    void updateDisplay(bool fShow, bool fSetHideFlag);

signals:

    void minimizeAction();
    void exitAction();
    void closeAction();
    void geometryUpdated();

protected:

    bool eventFilter(QObject *pObj, QEvent *pEvent);
    void mouseMoveEvent(QMouseEvent *pEvent);
    void timerEvent(QTimerEvent *pEvent);
    void showEvent(QShowEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

private slots:

    void togglePushpin(bool fOn);

private:

    void initialize();
    void moveToBase();
    void setMouseTrackingEnabled(bool fEnabled);

    QAction *m_pAutoHideAction;
    QLabel *m_pDisplayLabel;
    QAction *m_pMinimizeAction;
    QAction *m_pRestoreAction;
    QAction *m_pCloseAction;

    QBasicTimer m_scrollTimer;
    QBasicTimer m_autoScrollTimer;

    bool m_fActive;
    bool m_fPolished;
    bool m_fSeamless;
    bool m_fAutoHide;
    bool m_fSlideToScreen;
    bool m_fHideAfterSlide;

    int m_iAutoHideCounter;
    int m_iPositionX;
    int m_iPositionY;

    /* Lists of used spacers */
    QList<QWidget*> m_Spacings;
    QList<QWidget*> m_LabelMargins;

    /* Menu insert position */
    QAction *m_pInsertPosition;

    /* Tool-bar alignment */
    Alignment m_alignment;

    /* Whether to animate showing/hiding the toolbar */
    bool m_fAnimated;

    /* Interval (in milli seconds) for scrolling the toolbar, default is 20 msec */
    int m_iScrollDelay;

    /* The wait time while the cursor is not over the window after this amount of time (in msec),
     * the toolbar will auto hide if autohide is on. The default is 100msec. */
    int m_iAutoScrollDelay;

    /* Number of total steps before hiding. If it is 10 then wait 10 (steps) * 100ms (m_iAutoScrollDelay) = 1000ms delay.
     * The default is 10. */
    int m_iAutoHideTotalCounter;
};

#endif // __VBoxMiniToolBar_h__

