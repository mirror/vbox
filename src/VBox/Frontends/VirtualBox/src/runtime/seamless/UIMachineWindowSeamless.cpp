/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowSeamless class implementation
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

/* Qt includes: */
#include <QDesktopWidget>
#include <QMenu>
#include <QTimer>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindowSeamless.h"
#include "UIMachineViewSeamless.h"
#ifndef Q_WS_MAC
# include "UIMiniToolBar.h"
#endif /* !Q_WS_MAC */
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CSnapshot.h"

UIMachineWindowSeamless::UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
#ifndef Q_WS_MAC
    , m_pMiniToolBar(0)
#endif /* !Q_WS_MAC */
{
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update mini-toolbar: */
    updateAppearanceOf(UIVisualElement_MiniToolBar);
}
#endif /* !Q_WS_MAC */

void UIMachineWindowSeamless::sltPopupMainMenu()
{
    /* Popup main-menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltSelectFirstAction()));
    }
}

void UIMachineWindowSeamless::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

    /* Prepare menu: */
    m_pMainMenu = uisession()->newMenu();
}

void UIMachineWindowSeamless::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* This might be required to correctly mask: */
    centralWidget()->setAutoFillBackground(false);

#ifdef Q_WS_MAC
    /* Please note: All the stuff below has to be done after the window has
     * switched to fullscreen. Qt changes the winId on the fullscreen
     * switch and make this stuff useless with the old winId. So please be
     * careful on rearrangement of the method calls. */
    ::darwinSetShowsWindowTransparent(this, true);
#endif /* Q_WS_MAC */

#ifndef Q_WS_MAC
    /* Prepare mini-toolbar: */
    prepareMiniToolbar();
#endif /* !Q_WS_MAC */
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::prepareMiniToolbar()
{
    /* Get machine: */
    CMachine m = machine();

    /* Make sure mini-toolbar is necessary: */
    bool fIsActive = m.GetExtraData(GUI_ShowMiniToolBar) != "no";
    if (!fIsActive)
        return;

    /* Get the mini-toolbar alignment: */
    bool fIsAtTop = m.GetExtraData(GUI_MiniToolBarAlignment) == "top";
    /* Get the mini-toolbar auto-hide feature availability: */
    bool fIsAutoHide = m.GetExtraData(GUI_MiniToolBarAutoHide) != "off";
    /* Create mini-toolbar: */
    m_pMiniToolBar = new UIRuntimeMiniToolBar(this,
                                              fIsAtTop ? Qt::AlignTop : Qt::AlignBottom,
                                              IntegrationMode_External,
                                              fIsAutoHide);
    m_pMiniToolBar->show();
    QList<QMenu*> menus;
    QList<QAction*> actions = uisession()->newMenu()->actions();
    for (int i=0; i < actions.size(); ++i)
        menus << actions.at(i)->menu();
    m_pMiniToolBar->addMenus(menus);
    connect(m_pMiniToolBar, SIGNAL(sigMinimizeAction()), this, SLOT(showMinimized()));
    connect(m_pMiniToolBar, SIGNAL(sigExitAction()),
            gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SLOT(trigger()));
    connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
            gActionPool->action(UIActionIndexRuntime_Simple_Close), SLOT(trigger()));
}
#endif /* !Q_WS_MAC */

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::cleanupMiniToolbar()
{
    /* Make sure mini-toolbar was created: */
    if (!m_pMiniToolBar)
        return;

    /* Save mini-toolbar settings: */
    machine().SetExtraData(GUI_MiniToolBarAutoHide, m_pMiniToolBar->autoHide() ? QString() : "off");
    /* Delete mini-toolbar: */
    delete m_pMiniToolBar;
    m_pMiniToolBar = 0;
}
#endif /* !Q_WS_MAC */

void UIMachineWindowSeamless::cleanupVisualState()
{
#ifndef Q_WS_MAC
    /* Cleeanup mini-toolbar: */
    cleanupMiniToolbar();
#endif /* !Q_WS_MAC */

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowSeamless::cleanupMenu()
{
    /* Cleanup menu: */
    delete m_pMainMenu;
    m_pMainMenu = 0;

    /* Call to base-class: */
    UIMachineWindow::cleanupMenu();
}

void UIMachineWindowSeamless::placeOnScreen()
{
    /* Get corresponding screen: */
    int iScreen = qobject_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* Calculate working area: */
    QRect workingArea = vboxGlobal().availableGeometry(iScreen);
    /* Move to the appropriate position: */
    move(workingArea.topLeft());
    /* Resize to the appropriate size: */
    resize(workingArea.size());
#ifndef Q_WS_MAC
    /* Move mini-toolbar into appropriate place: */
    if (m_pMiniToolBar)
        m_pMiniToolBar->adjustGeometry();
#endif /* !Q_WS_MAC */
    /* Process pending move & resize events: */
    qApp->processEvents();
}

void UIMachineWindowSeamless::showInNecessaryMode()
{
    /* Should we show window?: */
    if (uisession()->isScreenVisible(m_uScreenId))
    {
        /* Do we have the seamless logic? */
        if (UIMachineLogicSeamless *pSeamlessLogic = qobject_cast<UIMachineLogicSeamless*>(machineLogic()))
        {
            /* Is this guest screen has own host screen? */
            if (pSeamlessLogic->hasHostScreenForGuestScreen(m_uScreenId))
            {
                /* Show manually maximized window: */
                placeOnScreen();

                /* Show normal window: */
                show();

#ifdef Q_WS_MAC
                /* Make sure it is really on the right place (especially on the Mac): */
                QRect r = vboxGlobal().availableGeometry(qobject_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId));
                move(r.topLeft());
#endif /* Q_WS_MAC */

                /* Return early: */
                return;
            }
        }
    }
    /* Hide in other cases: */
    hide();
}

#ifndef Q_WS_MAC
void UIMachineWindowSeamless::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update mini-toolbar: */
    if (iElement & UIVisualElement_MiniToolBar)
    {
        if (m_pMiniToolBar)
        {
            /* Get machine: */
            const CMachine &m = machine();
            /* Get snapshot(s): */
            QString strSnapshotName;
            if (m.GetSnapshotCount() > 0)
            {
                CSnapshot snapshot = m.GetCurrentSnapshot();
                strSnapshotName = " (" + snapshot.GetName() + ")";
            }
            /* Update mini-toolbar text: */
            m_pMiniToolBar->setText(m.GetName() + strSnapshotName);
        }
    }
}
#endif /* !Q_WS_MAC */

#ifdef Q_WS_MAC
bool UIMachineWindowSeamless::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Paint:
        {
            /* Clear the background */
            CGContextClearRect(::darwinToCGContextRef(this), ::darwinToCGRect(frameGeometry()));
            break;
        }
        default:
            break;
    }
    return UIMachineWindow::event(pEvent);
}
#endif /* Q_WS_MAC */

void UIMachineWindowSeamless::setMask(const QRegion &incomingRegion)
{
    /* Copy region: */
    QRegion region(incomingRegion);

#ifndef Q_WS_MAC
    /* Shift region if left spacer width is NOT zero or top spacer height is NOT zero: */
    if (m_pLeftSpacer->geometry().width() || m_pTopSpacer->geometry().height())
        region.translate(m_pLeftSpacer->geometry().width(), m_pTopSpacer->geometry().height());
#endif /* !Q_WS_MAC */

#ifdef VBOX_GUI_USE_QUARTZ2D
    if (vboxGlobal().vmRenderMode() == Quartz2DMode)
    {
        /* If we are using the Quartz2D backend we have to trigger a repaint only.
         * All the magic clipping stuff is done in the paint engine. */
        ::darwinWindowInvalidateShape(m_pMachineView->viewport());
    }
#else /* VBOX_GUI_USE_QUARTZ2D */
    /* Seamless-window for empty region should be empty too,
     * but native API wrapped by the QWidget::setMask() doesn't allow this.
     * Instead, we have a full painted screen of seamless-geometry size visible.
     * Moreover, we can't just hide the empty seamless-window as 'hiding'
     * 1. will collide with the multi-screen layout behavior and
     * 2. will cause a task-bar flicker on moving window from one screen to another.
     * As a temporary though quite a dirty workaround we have to make sure
     * region have at least one pixel. */
    if (region.isEmpty())
        region += QRect(0, 0, 1, 1);
    /* Make sure region had changed: */
    if (m_previousRegion != region)
    {
        /* Remember new region: */
        m_previousRegion = region;
        /* Assign new region: */
        UIMachineWindow::setMask(m_previousRegion);
        /* Update viewport contents: */
        m_pMachineView->viewport()->update();
    }
#endif /* !VBOX_GUI_USE_QUARTZ2D */
}

