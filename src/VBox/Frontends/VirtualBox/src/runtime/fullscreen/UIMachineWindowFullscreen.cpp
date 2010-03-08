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

#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowFullscreen.h"

UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI2<QIMainDialog>(0, Qt::FramelessWindowHint)
    , UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
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

    /* Retranslate fullscreen window finally: */
    retranslateUi();

    /* Prepare machine view container: */
    prepareMachineViewContainer();

    /* Prepare fullscreen machine view: */
    prepareMachineView();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Show window: */
    showFullScreen();

#ifdef Q_WS_MAC
    /* Make sure it is really on the right place (especially on the Mac) */
    move(0, 0);
#endif /* Q_WS_MAC */
}

UIMachineWindowFullscreen::~UIMachineWindowFullscreen()
{
    /* Cleanup normal machine view: */
    cleanupMachineView();

    /* Cleanup menu: */
    cleanupMenu();
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

#ifdef Q_WS_MAC
    // TODO_NEW_CORE
//    m_pDockSettingsMenu->setTitle(tr("Dock Icon"));
//    m_pDockDisablePreview->setText(tr("Show Application Icon"));
//    m_pDockEnablePreviewMonitor->setText(tr("Show Monitor Preview"));
#endif /* Q_WS_MAC */
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
#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar());
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu();
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
#ifdef Q_WS_MAC
    // Sync with UIMachineWindowFullscreen::prepareMenu()!
#else /* To NaN: Please uncomment below for mac too and test! */
    delete m_pMainMenu;
    m_pMainMenu = 0;
#endif /* Q_WS_MAC */
}

