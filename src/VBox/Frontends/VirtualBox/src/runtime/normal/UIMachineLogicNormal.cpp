/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineLogicNormal class implementation
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

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include "UIFirstRunWzd.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"

#include "VBoxUtils.h"

UIMachineLogicNormal::UIMachineLogicNormal(QObject *pParent, UISession *pSession, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, pSession, pActionsPool, UIVisualStateType_Normal)
{
    /* Check the status of required features: */
    prepareRequiredFeatures();

    /* If required features are ready: */
    if (!isPreventAutoStart())
    {
        /* Prepare console connections: */
        prepareConsoleConnections();

        /* Prepare action groups: */
        prepareActionGroups();

        /* Prepare action connections: */
        prepareActionConnections();

        /* Prepare normal machine window: */
        prepareMachineWindows();

        /* Initialization: */
        sltMachineStateChanged();
        sltAdditionsStateChanged();
        sltMouseCapabilityChanged();
    }
}

UIMachineLogicNormal::~UIMachineLogicNormal()
{
    /* Cleanup normal machine window: */
    cleanupMachineWindow();
}

void UIMachineLogicNormal::sltPrepareNetworkAdaptersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Network Adapters menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Simple_NetworkAdaptersDialog));
}

void UIMachineLogicNormal::sltPrepareSharedFoldersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Shared Folders menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Simple_SharedFoldersDialog));
}

void UIMachineLogicNormal::sltPrepareMouseIntegrationMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Mouse Integration Menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Toggle_MouseIntegration));
}

void UIMachineLogicNormal::prepareActionConnections()
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

void UIMachineLogicNormal::prepareMachineWindows()
{
    /* Get monitor count: */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();

    /* Do not create window(s) if created already: */
    if ((ulong)machineWindows().size() == uMonitorCount)
        return;

    /* List should not be filled partially: */
    AssertMsg(machineWindows().size() == 0, ("Windows list is filled partially, something broken!\n"));

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Create machine window(s): */
    for (ulong uScreenId = 0; uScreenId < uMonitorCount; ++ uScreenId)
        addMachineWindow(UIMachineWindow::create(this, visualStateType(), uScreenId));

    /* Notify others about machine window(s) created: */
    setMachineWindowsCreated(true);

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
            UIFirstRunWzd wzd(mainMachineWindow()->machineWindow(), machine);
            wzd.exec();
        }

        /* Start VM: */
        CProgress progress = vboxGlobal().isStartPausedEnabled() || vboxGlobal().isDebuggerAutoShowEnabled() ?
                             console.PowerUpPaused() : console.PowerUp();

#if 0 // TODO: Check immediate failure!
        /* Check for an immediate failure: */
        if (!console.isOk())
        {
            vboxProblem().cannotStartMachine(console);
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

        /* Disable auto-closure because we want to have a chance to show the error dialog on startup failure: */
        setPreventAutoClose(true);
#endif

        /* Show "Starting/Restoring" progress dialog: */
        if (uisession()->isSaved())
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), mainMachineWindow()->machineWindow(), 0);
        else
            vboxProblem().showModalProgressDialog(progress, machine.GetName(), mainMachineWindow()->machineWindow());

#if 0 // TODO: Check immediate failure!
        /* Check for an progress failure */
        if (progress.GetResultCode() != 0)
        {
            vboxProblem().cannotStartMachine(progress);
            machineWindowWrapper()->machineWindow()->close();
            return;
        }

        /* Enable auto-closure again: */
        setPreventAutoClose(false);

        /* Check if we missed a really quick termination after successful startup, and process it if we did: */
        if (uisession()->isTurnedOff())
        {
            machineWindowWrapper()->machineWindow()->close();
            return;
        }
#endif

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

void UIMachineLogicNormal::cleanupMachineWindow()
{
    /* Do not cleanup machine window if it is not present: */
    if (!machineWindowsCreated())
        return;

    /* Cleanup normal machine window: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
}

