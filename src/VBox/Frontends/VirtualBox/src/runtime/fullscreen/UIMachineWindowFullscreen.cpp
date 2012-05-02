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

/* Global includes */
#include <QDesktopWidget>
#include <QTimer>
#ifdef Q_WS_MAC
# include <QMenuBar>
#endif /* Q_WS_MAC */

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxMiniToolBar.h"

#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineView.h"

UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
    , m_pMiniToolBar(0)
{
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

    /* Prepare handlers: */
    prepareHandlers();

    /* Prepare mini tool-bar: */
    prepareMiniToolBar();

    /* Retranslate fullscreen window finally: */
    retranslateUi();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Show fullscreen window: */
    showInNecessaryMode();
}

UIMachineWindowFullscreen::~UIMachineWindowFullscreen()
{
    /* Save window settings: */
    saveWindowSettings();

    /* Cleanup mini tool-bar: */
    cleanupMiniToolBar();

    /* Prepare handlers: */
    cleanupHandlers();

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

void UIMachineWindowFullscreen::sltPopupMainMenu()
{
    /* Popup main menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltSelectFirstAction()));
    }
}

void UIMachineWindowFullscreen::prepareMenu()
{
#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar());
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu();
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
                                           , m_uScreenId
                                           , machineLogic()->visualStateType()
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                           );

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

void UIMachineWindowFullscreen::cleanupMiniToolBar()
{
    if (m_pMiniToolBar)
    {
        delete m_pMiniToolBar;
        m_pMiniToolBar = 0;
    }
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

void UIMachineWindowFullscreen::showInNecessaryMode()
{
    /* Make sure we really have to show window: */
    BOOL fEnabled = true;
    ULONG guestOriginX = 0, guestOriginY = 0, guestWidth = 0, guestHeight = 0;
    session().GetMachine().QuerySavedGuestScreenInfo(m_uScreenId, guestOriginX, guestOriginY, guestWidth, guestHeight, fEnabled);
    if (fEnabled)
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
        QRect r = QApplication::desktop()->screenGeometry(static_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId));
        move(r.topLeft());
#endif /* Q_WS_MAC */
    }
}

