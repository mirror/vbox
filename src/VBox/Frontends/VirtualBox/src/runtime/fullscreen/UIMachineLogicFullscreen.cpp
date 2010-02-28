/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicFullscreen class implementation
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
#include <QMenu>
#include <QDesktopWidget>

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include "UIFirstRunWzd.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"

#include "VBoxUtils.h"

UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Fullscreen)
{
    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Prepare action groups: */
    prepareActionGroups();

    /* Prepare action connections: */
    prepareActionConnections();

    /* Check the status of required features: */
    prepareRequiredFeatures();

    /* Prepare machine window: */
    prepareMachineWindow();

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMouseCapabilityChanged();
}

UIMachineLogicFullscreen::~UIMachineLogicFullscreen()
{
    /* Cleanup machine window: */
    cleanupMachineWindow();
}

void UIMachineLogicFullscreen::sltPrepareNetworkAdaptersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Network Adapters menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Simple_NetworkAdaptersDialog));
}

void UIMachineLogicFullscreen::sltPrepareSharedFoldersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Shared Folders menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Simple_SharedFoldersDialog));
}

void UIMachineLogicFullscreen::sltPrepareMouseIntegrationMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Mouse Integration Menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Toggle_MouseIntegration));
}

void UIMachineLogicFullscreen::prepareActionConnections()
{
    /* Base class connections: */
    UIMachineLogic::prepareActionConnections();

    /* This class connections: */
    connect(actionsPool()->action(UIActionIndex_Menu_NetworkAdapters)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareNetworkAdaptersMenu()));
    connect(actionsPool()->action(UIActionIndex_Menu_SharedFolders)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedFoldersMenu()));
    connect(actionsPool()->action(UIActionIndex_Menu_MouseIntegration)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareMouseIntegrationMenu()));
}

void UIMachineLogicFullscreen::prepareRequiredFeatures()
{
    // TODO_NEW_CORE
//    if (session().GetConsole().isAutoresizeGuestActive())
    {
//        QRect screen = QApplication::desktop()->screenGeometry(this);
//        ULONG64 availBits = session().GetMachine().GetVRAMSize() /* vram */
//                          * _1M /* mb to bytes */
//                          * 8; /* to bits */
//        ULONG guestBpp = mConsole->console().GetDisplay().GetBitsPerPixel();
//        ULONG64 usedBits = (screen.width() /* display width */
//                         * screen.height() /* display height */
//                         * guestBpp
//                         + _1M * 8) /* current cache per screen - may be changed in future */
//                         * session().GetMachine().GetMonitorCount() /**< @todo fix assumption that all screens have same resolution */
//                         + 4096 * 8; /* adapter info */
//        if (availBits < usedBits)
//        {
//            int result = vboxProblem().cannotEnterFullscreenMode(
//                   screen.width(), screen.height(), guestBpp,
//                   (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
//            if (result == QIMessageBox::Cancel)
//                sltClose();
//        }
    }
}

void UIMachineLogicFullscreen::prepareMachineWindow()
{
    /* Do not prepare window if its ready: */
    if (machineWindowWrapper())
        return;

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Create machine window: */
    setMachineWindowWrapper(UIMachineWindow::create(this, visualStateType()));

    /* If we are not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
    {
        /* Get current machine/console: */
        CMachine machine = session().GetMachine();
        CConsole console = session().GetConsole();

        /* Notify user about mouse&keyboard auto-capturing: */
        if (vboxGlobal().settings().autoCapture())
            vboxProblem().remindAboutAutoCapture();

        /* Shows first run wizard if necessary: */
        if (uisession()->isFirstTimeStarted())
        {
            UIFirstRunWzd wzd(machineWindowWrapper()->machineWindow(), machine);
            wzd.exec();
        }

        /* Start VM: */
        CProgress progress = vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled() ?
                             console.PowerUpPaused() : console.PowerUp();
        /* Check for an immediate failure */
        if (!console.isOk())
        {
            vboxProblem().cannotStartMachine(console);
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

        /* Disable auto-closure because we want to have a chance to show the error dialog on startup failure: */
        setPreventAutoClose(true);

        /* Show "Starting/Restoring" progress dialog: */
        if (uisession()->isSaved())
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindowWrapper()->machineWindow(), 0);
        else
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindowWrapper()->machineWindow());

        /* Check for an progress failure */
        if (progress.GetResultCode() != 0)
        {
            vboxProblem().cannotStartMachine(progress);
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

        /* Process pending events: */
        qApp->processEvents();

        /* Enable auto-closure again: */
        setPreventAutoClose(false);

        /* Check if we missed a really quick termination after successful startup, and process it if we did: */
        if (uisession()->isTurnedOff())
        {
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

#if 0 // TODO: Rework debugger logic!
# ifdef VBOX_WITH_DEBUGGER_GUI
        /* Open the debugger in "full screen" mode requested by the user. */
        else if (vboxGlobal().isDebuggerAutoShowEnabled())
        {
            /* console in upper left corner of the desktop. */
            QRect rct (0, 0, 0, 0);
            QDesktopWidget *desktop = QApplication::desktop();
            if (desktop)
                rct = desktop->availableGeometry(pos());
            move (QPoint (rct.x(), rct.y()));

            if (vboxGlobal().isDebuggerAutoShowStatisticsEnabled())
                sltShowDebugStatistics();
            if (vboxGlobal().isDebuggerAutoShowCommandLineEnabled())
                sltShowDebugCommandLine();

            if (!vboxGlobal().isStartPausedEnabled())
                machineWindowWrapper()->machineView()->pause (false);
        }
# endif
#endif

#ifdef VBOX_WITH_UPDATE_REQUEST
        /* Check for updates if necessary: */
        vboxGlobal().showUpdateDialog(false /* force request? */);
#endif
    }
}

void UIMachineLogicFullscreen::cleanupMachineWindow()
{
    /* Do not cleanup machine window if it is not present: */
    if (!machineWindowWrapper())
        return;

    /* Cleanup machine window: */
    UIMachineWindow::destroy(machineWindowWrapper());
    setMachineWindowWrapper(0);
}

