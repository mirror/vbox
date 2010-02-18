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
    /* Prepare action connections: */
    prepareActionConnections();

    /* Prepare normal machine window: */
    prepareMachineWindow();
}

UIMachineLogicNormal::~UIMachineLogicNormal()
{
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
    connect(actionsPool()->action(UIActionIndex_Menu_NetworkAdapters)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareNetworkAdaptersMenu()));
    connect(actionsPool()->action(UIActionIndex_Menu_SharedFolders)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedFoldersMenu()));
}

void UIMachineLogicNormal::prepareMachineWindow()
{
    if (machineWindowWrapper())
        return;

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    ::SetFrontProcess(&psn);
#endif /* Q_WS_MAC */

    m_pMachineWindowContainer = UIMachineWindow::create(this, visualStateType());

    /* Get the correct initial machineState() value */
    setMachineState(session().GetConsole().GetState());

    /* Update all the stuff: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    if (vboxGlobal().settings().autoCapture())
        vboxProblem().remindAboutAutoCapture();

    /* Notify the console scroll-view about the console-window is opened: */
    machineWindowWrapper()->machineView()->onViewOpened();

    bool saved = machineState() == KMachineState_Saved;

    CMachine machine = session().GetMachine();
    CConsole console = session().GetConsole();

    if (isFirstTimeStarted())
    {
        UIFirstRunWzd wzd(machineWindowWrapper()->machineWindow(), machine);
        wzd.exec();

        /* Remove GUI_FirstRun extra data key from the machine settings
         * file after showing the wizard once. */
        machine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);
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

    //machineWindowWrapper()->machineView()->attach();

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

#if 0 // TODO: Is it necessary now?
     * Checking if the fullscreen mode should be activated: */
    QString str = machine.GetExtraData (VBoxDefs::GUI_Fullscreen);
    if (str == "on")
        mVmFullscreenAction->setChecked (true);

    /* If seamless mode should be enabled then check if it is enabled
     * currently and re-enable it if seamless is supported: */
    if (mVmSeamlessAction->isChecked() && m_bIsSeamlessSupported && m_bIsGraphicsSupported)
        toggleFullscreenMode (true, true);

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

    connect(machineWindowWrapper()->machineView(), SIGNAL(machineStateChanged(KMachineState)), this, SLOT(sltUpdateMachineState(KMachineState)));
    connect(machineWindowWrapper()->machineView(), SIGNAL(additionsStateChanged(const QString&, bool, bool, bool)),
            this, SLOT(sltUpdateAdditionsState(const QString &, bool, bool, bool)));
    connect(machineWindowWrapper()->machineView(), SIGNAL(mouseStateChanged(int)), this, SLOT(sltUpdateMouseState(int)));

    /* Re-request all the static values finally after view is really opened and attached: */
    updateAppearanceOf(UIVisualElement_VirtualizationStuff);
}

void UIMachineLogicNormal::cleanupMachineWindow()
{
    if (!machineWindowWrapper())
        return;

    // TODO: What should be done on window destruction?
    //machineWindowWrapper()->machineView()->detach();
    //m_session.Close();
    //m_session.detach();
}
