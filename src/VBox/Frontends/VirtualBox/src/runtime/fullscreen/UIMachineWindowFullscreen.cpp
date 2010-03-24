/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class implementation
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

/* Global includes */
#include <QDesktopWidget>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxMiniToolBar.h"

#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineView.h"
#include "UIMachineWindowFullscreen.h"
#include "UISession.h"

UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI2<QMainWindow>(0, Qt::FramelessWindowHint)
    , UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
    , m_pMiniToolBar(0)
{
    /* "This" is machine window: */
    m_pMachineWindow = this;

    /* Set the main window in VBoxGlobal: */
    if (uScreenId == 0)
        vboxGlobal().setMainWindow(this);

    /* Prepare fullscreen window icon: */
    prepareWindowIcon();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Prepare fullscreen menu: */
    prepareMenu();

    /* Prepare machine view container: */
    prepareMachineViewContainer();

    /* Prepare fullscreen machine view: */
    prepareMachineView();

    /* Prepare mini tool-bar: */
    prepareMiniToolBar();

    /* Retranslate fullscreen window finally: */
    retranslateUi();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Make sure the window is placed on valid screen
     * before we are show fullscreen window: */
    sltPlaceOnScreen();

    /* Show fullscreen window: */
    showFullScreen();

    /* Make sure the window is placed on valid screen again
     * after window is shown & window's decorations applied.
     * That is required due to X11 Window Geometry Rules. */
    sltPlaceOnScreen();

#ifdef Q_WS_MAC
    /* Make sure it is really on the right place (especially on the Mac) */
    QRect r = QApplication::desktop()->screenGeometry(static_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId));
    move(r.topLeft());
#endif /* Q_WS_MAC */
}

UIMachineWindowFullscreen::~UIMachineWindowFullscreen()
{
    /* Save window settings: */
    saveWindowSettings();

    /* Cleanup machine view: */
    cleanupMachineView();

    /* Cleanup menu: */
    cleanupMenu();
}

void UIMachineWindowFullscreen::sltPlaceOnScreen()
{
    /* Get corresponding screen: */
    int iScreen = static_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* Calculate working area: */
    QRect workingArea = QApplication::desktop()->screenGeometry(iScreen);
    /* Move to the appropriate position: */
    move(workingArea.topLeft());
    /* Resize to the appropriate size: */
    resize(workingArea.size());
    /* Process pending move & resize events: */
    qApp->processEvents();
}

void UIMachineWindowFullscreen::sltMachineStateChanged()
{
    UIMachineWindow::sltMachineStateChanged();
}

void UIMachineWindowFullscreen::sltPopupMainMenu()
{
    /* Popup main menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
        m_pMainMenu->popup(machineWindow()->geometry().center());
}

void UIMachineWindowFullscreen::sltTryClose()
{
    UIMachineWindow::sltTryClose();
}

void UIMachineWindowFullscreen::retranslateUi()
{
    /* Translate parent class: */
    UIMachineWindow::retranslateUi();
}

#ifdef Q_WS_X11
bool UIMachineWindowFullscreen::x11Event(XEvent *pEvent)
{
    /* Qt bug: when the console view grabs the keyboard, FocusIn, FocusOut,
     * WindowActivate and WindowDeactivate Qt events are not properly sent
     * on top level window (i.e. this) deactivation. The fix is to substiute
     * the mode in FocusOut X11 event structure to NotifyNormal to cause
     * Qt to process it as desired. */
    if (pEvent->type == FocusOut)
    {
        if (pEvent->xfocus.mode == NotifyWhileGrabbed  &&
            (pEvent->xfocus.detail == NotifyAncestor ||
             pEvent->xfocus.detail == NotifyInferior ||
             pEvent->xfocus.detail == NotifyNonlinear))
        {
             pEvent->xfocus.mode = NotifyNormal;
        }
    }
    return false;
}
#endif

void UIMachineWindowFullscreen::closeEvent(QCloseEvent *pEvent)
{
    return UIMachineWindow::closeEvent(pEvent);
}

void UIMachineWindowFullscreen::prepareMenu()
{
    UIMainMenuType fMenus = UIMainMenuType_All;
    /* Remove the view menu in the case there is one screen only. */
    if (QApplication::desktop()->numScreens() == 1)
        fMenus = UIMainMenuType(fMenus ^ UIMainMenuType_View);
#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar(fMenus));
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu(fMenus);
}

void UIMachineWindowFullscreen::prepareMiniToolBar()
{
    /* Get current machine: */
    CMachine machine = session().GetConsole().GetMachine();
    /* Check if mini tool-bar should present: */
    bool fIsActive = machine.GetExtraData(VBoxDefs::GUI_ShowMiniToolBar) != "no";
    if (fIsActive)
    {
        /* Get the mini tool-bar alignment: */
        bool fIsAtTop = machine.GetExtraData(VBoxDefs::GUI_MiniToolBarAlignment) == "top";
        /* Get the mini tool-bar auto-hide feature availability: */
        bool fIsAutoHide = machine.GetExtraData(VBoxDefs::GUI_MiniToolBarAutoHide) != "off";
        m_pMiniToolBar = new VBoxMiniToolBar(centralWidget(),
                                             fIsAtTop ? VBoxMiniToolBar::AlignTop : VBoxMiniToolBar::AlignBottom,
                                             true, fIsAutoHide);
        m_pMiniToolBar->updateDisplay(true, true);
        QList<QMenu*> menus;
        menus << uisession()->actionsPool()->action(UIActionIndex_Menu_Machine)->menu();
        menus << uisession()->actionsPool()->action(UIActionIndex_Menu_Devices)->menu();
        *m_pMiniToolBar << menus;
        connect(m_pMiniToolBar, SIGNAL(exitAction()),
                uisession()->actionsPool()->action(UIActionIndex_Toggle_Fullscreen), SLOT(trigger()));
        connect(m_pMiniToolBar, SIGNAL(closeAction()),
                uisession()->actionsPool()->action(UIActionIndex_Simple_Close), SLOT(trigger()));
    }
}

void UIMachineWindowFullscreen::prepareMachineView()
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = session().GetMachine().GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    /* Set central widget: */
    setCentralWidget(new QWidget);

    /* Set central widget layout: */
    centralWidget()->setLayout(m_pMachineViewContainer);

    m_pMachineView = UIMachineView::create(  this
                                           , vboxGlobal().vmRenderMode()
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                           , machineLogic()->visualStateType()
                                           , m_uScreenId);

    /* Add machine view into layout: */
    m_pMachineViewContainer->addWidget(m_pMachineView, 1, 1, Qt::AlignVCenter | Qt::AlignHCenter);

    /* The background has to go black: */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette(palette);
    centralWidget()->setAutoFillBackground(true);
    setAutoFillBackground(true);
}

void UIMachineWindowFullscreen::saveWindowSettings()
{
    /* Get machine: */
    CMachine machine = session().GetConsole().GetMachine();

    /* Save extra-data settings: */
    {
        /* Save mini tool-bar settings: */
        if (m_pMiniToolBar)
            machine.SetExtraData(VBoxDefs::GUI_MiniToolBarAutoHide, m_pMiniToolBar->isAutoHide() ? QString() : "off");
    }
}

void UIMachineWindowFullscreen::cleanupMachineView()
{
    /* Do not cleanup machine view if it is not present: */
    if (!machineView())
        return;

    UIMachineView::destroy(m_pMachineView);
    m_pMachineView = 0;
}

void UIMachineWindowFullscreen::cleanupMenu()
{
    delete m_pMainMenu;
    m_pMainMenu = 0;
}

void UIMachineWindowFullscreen::updateAppearanceOf(int iElement)
{
    /* Base class update: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* If mini tool-bar is present: */
    if (m_pMiniToolBar)
    {
        /* Get machine: */
        CMachine machine = session().GetConsole().GetMachine();
        /* Get snapshot(s): */
        QString strSnapshotName;
        if (machine.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = machine.GetCurrentSnapshot();
            strSnapshotName = " (" + snapshot.GetName() + ")";
        }
        /* Update mini tool-bar text: */
        m_pMiniToolBar->setDisplayText(machine.GetName() + strSnapshotName);
    }
}

