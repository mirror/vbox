/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineMenuBar class implementation
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

/* Local includes */
#include "UIMachineMenuBar.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/* Global includes */
#include <QMenuBar>

UIMachineMenuBar::UIMachineMenuBar()
    /* On the Mac we add some items only the first time, cause otherwise they
     * will be merged more than once to the application menu by Qt. */
    : m_fIsFirstTime(true)
{
}

QMenuBar* UIMachineMenuBar::createMenuBar(UISession *pSession)
{
    QMenuBar *pMenuBar = new QMenuBar(0);

    UIActionsPool *pActionsPool = pSession->actionsPool();

    /* Machine submenu: */
    QMenu *pMenuMachine = pActionsPool->action(UIActionIndex_Menu_Machine)->menu();
    prepareMenuMachine(pMenuMachine, pActionsPool);
    pMenuBar->addMenu(pMenuMachine);

    /* Devices submenu: */
    QMenu *pMenuDevices = pActionsPool->action(UIActionIndex_Menu_Devices)->menu();
    prepareMenuDevices(pMenuDevices, pActionsPool);
    pMenuBar->addMenu(pMenuDevices);

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
    {
        QMenu *pMenuDebug = pActionsPool->action(UIActionIndex_Menu_Debug)->menu();
        prepareMenuDebug(pMenuDebug, pActionsPool);
        pMenuBar->addMenu(pMenuDebug);
    }
#endif

    /* Help submenu: */
    QMenu *pMenuHelp = pActionsPool->action(UIActionIndex_Menu_Help)->menu();
    prepareMenuHelp(pMenuHelp, pActionsPool);
    pMenuBar->addMenu(pMenuHelp);

    /* Because this connections are done to VBoxGlobal, they are needed once
     * only. Otherwise we will get the slots called more than once. */
    if (m_fIsFirstTime)
    {
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Help), SIGNAL(triggered()),
                            &vboxProblem(), SLOT(showHelpHelpDialog()));
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Web), SIGNAL(triggered()),
                            &vboxProblem(), SLOT(showHelpWebDialog()));
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                            &vboxProblem(), SLOT(resetSuppressedMessages()));
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Register), SIGNAL(triggered()),
                            &vboxGlobal(), SLOT(showRegistrationDialog()));
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Update), SIGNAL(triggered()),
                            &vboxGlobal(), SLOT(showUpdateDialog()));
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                            &vboxProblem(), SLOT(showHelpAboutDialog()));

        VBoxGlobal::connect(&vboxGlobal(), SIGNAL (canShowRegDlg (bool)),
                            pActionsPool->action(UIActionIndex_Simple_Register), SLOT(setEnabled(bool)));
        VBoxGlobal::connect(&vboxGlobal(), SIGNAL (canShowUpdDlg (bool)),
                            pActionsPool->action(UIActionIndex_Simple_Update), SLOT(setEnabled(bool)));

        m_fIsFirstTime = false;
    }

    return pMenuBar;
}

void UIMachineMenuBar::prepareMenuMachine(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Fullscreen));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Seamless));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_GuestAutoresize));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_AdjustWindow));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_MouseIntegration));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_TypeCAD));
#ifdef Q_WS_X11
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_TypeCABS));
#endif
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_TakeSnapshot));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_InformationDialog));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Pause));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Reset));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Shutdown));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#else /* Q_WS_MAC */
    if (m_fIsFirstTime)
#endif /* !Q_WS_MAC */
        pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Close));
}

void UIMachineMenuBar::prepareMenuDevices(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Devices submenu */
    pMenu->addMenu(pActionsPool->action(UIActionIndex_Menu_OpticalDevices)->menu());
    pMenu->addMenu(pActionsPool->action(UIActionIndex_Menu_FloppyDevices)->menu());
    pMenu->addMenu(pActionsPool->action(UIActionIndex_Menu_USBDevices)->menu());
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_NetworkAdaptersDialog));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_SharedFoldersDialog));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_VRDP));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_InstallGuestTools));
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineMenuBar::prepareMenuDebug(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Debug submenu */
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Statistics));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_CommandLine));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Logging));
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineMenuBar::prepareMenuHelp(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Help));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Web));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_ResetWarnings));
    pMenu->addSeparator();

#ifdef VBOX_WITH_REGISTRATION
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Register));
#endif

#ifdef Q_WS_MAC
    if (m_fIsFirstTime)
#endif /* Q_WS_MAC */
        pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Update));

#ifndef Q_WS_MAC
    pMenu->addSeparator();
#else /* Q_WS_MAC */
    if (m_fIsFirstTime)
#endif /* !Q_WS_MAC */
        pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_About));
}

