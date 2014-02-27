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

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindowSeamless.h"
#include "UIMachineView.h"
#ifndef Q_WS_MAC
# include "UIMachineDefs.h"
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
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltHighlightFirstAction()));
    }
}

void UIMachineWindowSeamless::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

    /* Prepare menu: */
    CMachine machine = session().GetMachine();
    RuntimeMenuType restrictedMenus = VBoxGlobal::restrictedRuntimeMenuTypes(machine);
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    m_pMainMenu = uisession()->newMenu(allowedMenus);
}

void UIMachineWindowSeamless::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* Make sure we have no background
     * until the first one paint-event: */
    setAttribute(Qt::WA_NoSystemBackground);

#ifdef VBOX_WITH_TRANSLUCENT_SEAMLESS
# ifdef Q_WS_MAC
    /* Using native API to enable translucent background for the Mac host.
     * - We also want to disable window-shadows which is possible
     *   using Qt::WA_MacNoShadow only since Qt 4.8,
     *   while minimum supported version is 4.7.1 for now: */
    ::darwinSetShowsWindowTransparent(this, true);
# else /* Q_WS_MAC */
    /* Using Qt API to enable translucent background:
     * - Under Win host Qt conflicts with 3D stuff (black seamless regions).
     * - Under Mac host Qt doesn't allows to disable window-shadows
     *   until version 4.8, but minimum supported version is 4.7.1 for now.
     * - Under x11 host Qt has it broken with KDE 4.9 (black background): */
    setAttribute(Qt::WA_TranslucentBackground);
# endif /* !Q_WS_MAC */
#else /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */
    /* Make sure we have no background
     * until the first one set-region-event: */
    setMask(m_maskRegion);
#endif /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */

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
    RuntimeMenuType restrictedMenus = VBoxGlobal::restrictedRuntimeMenuTypes(m);
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    QList<QAction*> actions = uisession()->newMenu(allowedMenus)->actions();
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
    /* Cleanup mini-toolbar: */
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
    /* Adjust guest screen size if necessary: */
    machineView()->maybeAdjustGuestScreenSize();
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
    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return hide();

    /* Make sure this window has seamless logic: */
    UIMachineLogicSeamless *pSeamlessLogic = qobject_cast<UIMachineLogicSeamless*>(machineLogic());
    if (!pSeamlessLogic)
        return hide();

    /* Make sure this window mapped to some host-screen: */
    if (!pSeamlessLogic->hasHostScreenForGuestScreen(m_uScreenId))
        return hide();

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Make sure this window is maximized and placed on valid screen: */
    placeOnScreen();

    /* Show in normal mode: */
    show();
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

#if defined(VBOX_WITH_TRANSLUCENT_SEAMLESS) && defined(Q_WS_WIN)
void UIMachineWindowSeamless::showEvent(QShowEvent*)
{
    /* Following workaround allows to fix the next Qt BUG:
     * https://bugreports.qt-project.org/browse/QTBUG-17548
     * https://bugreports.qt-project.org/browse/QTBUG-30974
     * Widgets with Qt::WA_TranslucentBackground attribute
     * stops repainting after minimizing/restoring, we have to call for single update. */
    QApplication::postEvent(this, new QEvent(QEvent::UpdateRequest), Qt::LowEventPriority);
}
#endif /* VBOX_WITH_TRANSLUCENT_SEAMLESS && Q_WS_WIN */

#ifndef VBOX_WITH_TRANSLUCENT_SEAMLESS
void UIMachineWindowSeamless::setMask(const QRegion &region)
{
    /* Prepare mask-region: */
    QRegion maskRegion(region);

    /* Shift region if left spacer width is NOT zero or top spacer height is NOT zero: */
    if (m_pLeftSpacer->geometry().width() || m_pTopSpacer->geometry().height())
        maskRegion.translate(m_pLeftSpacer->geometry().width(), m_pTopSpacer->geometry().height());

    /* Seamless-window for empty region should be empty too,
     * but the QWidget::setMask() wrapper doesn't allow this.
     * Instead, we have a full painted screen of seamless-geometry size visible.
     * Moreover, we can't just hide the empty seamless-window as 'hiding'
     * 1. will collide with the multi-screen layout behavior and
     * 2. will cause a task-bar flicker on moving window from one screen to another.
     * As a *temporary* though quite a dirty workaround we have to make sure
     * region have at least one pixel. */
    if (maskRegion.isEmpty())
        maskRegion += QRect(0, 0, 1, 1);
    /* Make sure mask-region had changed: */
    if (m_maskRegion != maskRegion)
    {
        /* Compose viewport region to update: */
        QRegion toUpdate = m_maskRegion + maskRegion;
        /* Remember new mask-region: */
        m_maskRegion = maskRegion;
        /* Assign new mask-region: */
        UIMachineWindow::setMask(m_maskRegion);
        /* Update viewport region finally: */
        m_pMachineView->viewport()->update(toUpdate);
    }
}
#endif /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */

