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
#include <QTimer>

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

#include "UIFirstRunWzd.h"

#include "UIActionsPool.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"

UIMachineLogicNormal::UIMachineLogicNormal(QObject *pParent, const CSession &session, UIActionsPool *pActionsPool)
    : UIMachineLogic(pParent, session, pActionsPool, UIVisualStateType_Normal)
{
    /* Prepare action groups: */
    prepareActionGroups();

    /* Prepare action connections: */
    prepareActionConnections();

    /* Check the status of required features: */
    prepareRequiredFeatures();

    /* Prepare normal machine window: */
    prepareMachineWindow();

    /* Load common logic settings: */
    loadLogicSettings();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);
}

UIMachineLogicNormal::~UIMachineLogicNormal()
{
    /* Save common logic settings: */
    saveLogicSettings();

    /* Cleanup normal machine window: */
    cleanupMachineWindow();
}

void UIMachineLogicNormal::sltPrepareNetworkAdaptersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Network Adapters Menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Simple_NetworkAdaptersDialog));
}

void UIMachineLogicNormal::sltPrepareSharedFoldersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Shared Folders Menu show!\n"));
    menu->clear();
    menu->addAction(actionsPool()->action(UIActionIndex_Simple_SharedFoldersDialog));
}

void UIMachineLogicNormal::updateAppearanceOf(int iElement)
{
    /* Update parent-class elements: */
    UIMachineLogic::updateAppearanceOf(iElement);
}

void UIMachineLogicNormal::prepareActionConnections()
{
    /* Parent class connections: */
    UIMachineLogic::prepareActionConnections();

    /* This class connections: */
    connect(actionsPool()->action(UIActionIndex_Menu_NetworkAdapters)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareNetworkAdaptersMenu()));
    connect(actionsPool()->action(UIActionIndex_Menu_SharedFolders)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedFoldersMenu()));
}

void UIMachineLogicNormal::prepareMachineWindow()
{
    /* Do not prepare window if its ready: */
    if (machineWindowWrapper())
        return;

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    ::SetFrontProcess(&psn);
#endif /* Q_WS_MAC */

    /* Create machine window: */
    m_pMachineWindowContainer = UIMachineWindow::create(this, visualStateType());

    /* Get the correct initial machineState() value */
    setMachineState(session().GetConsole().GetState());

    /* Notify user about mouse&keyboard auto-capturing: */
    if (vboxGlobal().settings().autoCapture())
        vboxProblem().remindAboutAutoCapture();

    bool saved = machineState() == KMachineState_Saved;

    CMachine machine = session().GetMachine();
    CConsole console = session().GetConsole();

    /* Shows first run wizard if necessary: */
    if (isFirstTimeStarted())
    {
        UIFirstRunWzd wzd(machineWindowWrapper()->machineWindow(), machine);
        wzd.exec();
        machine.SetExtraData(VBoxDefs::GUI_FirstRun, QString());
    }


    // TODO: Do not start VM yet!
    return;


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

    /* Disable auto closure because we want to have a chance to show the error dialog on startup failure: */
    setPreventAutoClose(true);

    /* Show "Starting/Restoring" progress dialog: */
    if (saved)
        vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindowWrapper()->machineView(), 0);
    else
        vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindowWrapper()->machineView());

    /* Check for an progress failure */
    if (progress.GetResultCode() != 0)
    {
        vboxProblem().cannotStartMachine(progress);
        machineWindowWrapper()->machineWindow()->close();
        return;
    }

    /* Enable auto closure again: */
    setPreventAutoClose(false);

    /* Check if we missed a really quick termination after successful startup, and process it if we did: */
    if (machineState() == KMachineState_PoweredOff || machineState() == KMachineState_Saved ||
        machineState() == KMachineState_Teleported || machineState() == KMachineState_Aborted)
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

    setOpenViewFinished(true);

#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Check for updates if necessary: */
    vboxGlobal().showUpdateDialog(false /* aForce */);
#endif

    /* Configure view connections: */
    if (machineWindowWrapper()->machineView())
    {
        connect(machineWindowWrapper()->machineView(), SIGNAL(machineStateChanged(KMachineState)),
                this, SLOT(sltUpdateMachineState(KMachineState)));
        connect(machineWindowWrapper()->machineView(), SIGNAL(additionsStateChanged(const QString&, bool, bool, bool)),
                this, SLOT(sltUpdateAdditionsState(const QString &, bool, bool, bool)));
        connect(machineWindowWrapper()->machineView(), SIGNAL(mouseStateChanged(int)),
                this, SLOT(sltUpdateMouseState(int)));
    }
}

void UIMachineLogicNormal::cleanupMachineWindow()
{
    /* Do not cleanup machine window if it is not present: */
    if (!machineWindowWrapper())
        return;

    /* Cleanup machine window: */
    UIMachineWindow::destroy(m_pMachineWindowContainer);
    m_pMachineWindowContainer = 0;

    // TODO: What should be done on window destruction?
    //machineWindowWrapper()->machineView()->detach();
    //m_session.Close();
    //m_session.detach();
}
