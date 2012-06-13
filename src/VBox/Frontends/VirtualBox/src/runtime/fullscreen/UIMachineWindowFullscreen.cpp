/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class implementation
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

/* Qt includes: */
#include <QDesktopWidget>
#include <QMenu>
#include <QTimer>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* GUI includes: */
#include "UIDefs.h"
#include "VBoxMiniToolBar.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"

/* COM includes: */
#include "CMachine.h"
#include "CSnapshot.h"

UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
    , m_pMiniToolBar(0)
{
}

void UIMachineWindowFullscreen::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update mini-toolbar: */
    updateAppearanceOf(UIVisualElement_MiniToolBar);
}

void UIMachineWindowFullscreen::sltPlaceOnScreen()
{
    /* Get corresponding screen: */
    int iScreen = qobject_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* Calculate working area: */
    QRect workingArea = QApplication::desktop()->screenGeometry(iScreen);
    /* Move to the appropriate position: */
    move(workingArea.topLeft());
    /* Resize to the appropriate size: */
    resize(workingArea.size());
    /* Process pending move & resize events: */
    qApp->processEvents();
}

void UIMachineWindowFullscreen::sltPopupMainMenu()
{
    /* Popup main-menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltSelectFirstAction()));
    }
}

void UIMachineWindowFullscreen::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

    /* Prepare menu: */
#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar());
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu();
}

void UIMachineWindowFullscreen::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* The background has to go black: */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette(palette);
    centralWidget()->setAutoFillBackground(true);
    setAutoFillBackground(true);

    /* Prepare mini-toolbar: */
    prepareMiniToolbar();
}

void UIMachineWindowFullscreen::prepareMiniToolbar()
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
    m_pMiniToolBar = new VBoxMiniToolBar(centralWidget(),
                                         fIsAtTop ? VBoxMiniToolBar::AlignTop : VBoxMiniToolBar::AlignBottom,
                                         true, fIsAutoHide);
    m_pMiniToolBar->updateDisplay(true, true);
    QList<QMenu*> menus;
    QList<QAction*> actions = uisession()->newMenu()->actions();
    for (int i=0; i < actions.size(); ++i)
        menus << actions.at(i)->menu();
    *m_pMiniToolBar << menus;
    connect(m_pMiniToolBar, SIGNAL(minimizeAction()), this, SLOT(showMinimized()));
    connect(m_pMiniToolBar, SIGNAL(exitAction()),
            gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SLOT(trigger()));
    connect(m_pMiniToolBar, SIGNAL(closeAction()),
            gActionPool->action(UIActionIndexRuntime_Simple_Close), SLOT(trigger()));
}

void UIMachineWindowFullscreen::cleanupMiniToolbar()
{
    /* Make sure mini-toolbar was created: */
    if (!m_pMiniToolBar)
        return;

    /* Save mini-toolbar settings: */
    machine().SetExtraData(GUI_MiniToolBarAutoHide, m_pMiniToolBar->isAutoHide() ? QString() : "off");
    /* Delete mini-toolbar: */
    delete m_pMiniToolBar;
    m_pMiniToolBar = 0;
}

void UIMachineWindowFullscreen::cleanupVisualState()
{
    /* Cleanup mini-toolbar: */
    cleanupMiniToolbar();

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowFullscreen::cleanupMenu()
{
    /* Cleanup menu: */
    delete m_pMainMenu;
    m_pMainMenu = 0;

    /* Call to base-class: */
    UIMachineWindow::cleanupMenu();
}

void UIMachineWindowFullscreen::showInNecessaryMode()
{
    /* Show window if we have to: */
    if (uisession()->isScreenVisible(m_uScreenId))
    {
        /* Make sure the window is placed on valid screen
         * before we are show fullscreen window: */
        sltPlaceOnScreen();

        /* Show window fullscreen: */
        showFullScreen();

        /* Make sure the window is placed on valid screen again
         * after window is shown & window's decorations applied.
         * That is required due to X11 Window Geometry Rules. */
        sltPlaceOnScreen();

#ifdef Q_WS_MAC
        /* Make sure it is really on the right place (especially on the Mac): */
        QRect r = QApplication::desktop()->screenGeometry(qobject_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId));
        move(r.topLeft());
#endif /* Q_WS_MAC */
    }
    /* Else hide window: */
    else hide();
}

void UIMachineWindowFullscreen::updateAppearanceOf(int iElement)
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
            m_pMiniToolBar->setDisplayText(m.GetName() + strSnapshotName);
        }
    }
}

