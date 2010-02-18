/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindow class implementation
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
#include <QCloseEvent>

/* Local includes */
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxCloseVMDlg.h"

#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"
#include "UIMachineWindowNormal.h"
//#include "UIMachineWindowFullscreen.h"
//#include "UIMachineWindowSeamless.h"

UIMachineWindow* UIMachineWindow::create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType)
{
    UIMachineWindow *window = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            window = new UIMachineWindowNormal(pMachineLogic);
            break;
        case UIVisualStateType_Fullscreen:
            // window = new UIMachineWindowFullscreen(pMachineLogic);
            window = new UIMachineWindowNormal(pMachineLogic);
            break;
        case UIVisualStateType_Seamless:
            // window = new UIMachineWindowSeamless(pMachineLogic);
            window = new UIMachineWindowNormal(pMachineLogic);
            break;
    }
    return window;
}

UIMachineWindow::UIMachineWindow(UIMachineLogic *pMachineLogic)
    : m_pMachineLogic(pMachineLogic)
{
    /* Prepare window icon: */
    prepareWindowIcon();

    /* Load common window settings: */
    loadWindowSettings();

    /* Translate common window: */
    retranslateWindow();
}

UIMachineWindow::~UIMachineWindow()
{
    // Nothing for now!
}

void UIMachineWindow::retranslateWindow()
{
#ifdef VBOX_OSE
    m_strWindowTitlePrefix = UIMachineLogic::tr("VirtualBox OSE");
#else
    m_strWindowTitlePrefix = UIMachineLogic::tr("Sun VirtualBox");
#endif
#ifdef VBOX_BLEEDING_EDGE
    m_strWindowTitlePrefix += UIMachineLogic::tr(" EXPERIMENTAL build %1r%2 - %3")
                              .arg(RTBldCfgVersion())
                              .arg(RTBldCfgRevisionStr())
                              .arg(VBOX_BLEEDING_EDGE);
#endif
}

void UIMachineWindow::updateAppearanceOf(int iElement)
{
    CMachine machine = machineLogic()->session().GetMachine();

    if (iElement & UIVisualElement_WindowCaption)
    {
        QString strSnapshotName;
        if (machine.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = machine.GetCurrentSnapshot();
            strSnapshotName = " (" + snapshot.GetName() + ")";
        }
        machineWindow()->setWindowTitle(machine.GetName() + strSnapshotName + " [" +
                                        vboxGlobal().toString(machineLogic()->machineState()) + "] - " +
                                        m_strWindowTitlePrefix);

        // TODO: Move that to fullscreen/seamless update routine:
        // mMiniToolBar->setDisplayText(machine.GetName() + strSnapshotName);
    }
}

void UIMachineWindow::closeEvent(QCloseEvent *pEvent)
{
    static const char *pstrSave = "save";
    static const char *pstrShutdown = "shutdown";
    static const char *pstrPowerOff = "powerOff";
    static const char *pstrDiscardCurState = "discardCurState";

    if (!machineView())
    {
        pEvent->accept();
        return;
    }

    switch (machineLogic()->machineState())
    {
        case KMachineState_PoweredOff:
        case KMachineState_Saved:
        case KMachineState_Teleported:
        case KMachineState_Aborted:
            /* The machine has been already powered off or saved or aborted -- close the window immediately. */
            pEvent->accept();
            break;

        default:
            /* The machine is in some temporary state like Saving or Stopping.
             * Ignore the close event. When it is Stopping, it will be soon closed anyway from sltUpdatmachineState().
             * In all other cases, an appropriate progress dialog will be shown within a few seconds. */
            pEvent->ignore();
            break;

        case KMachineState_Teleporting: /** @todo Live Migration: Test closing a VM that's being teleported or snapshotted. */
        case KMachineState_LiveSnapshotting:
        case KMachineState_Running:
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM: /** @todo Live Migration: Check out this. */
        case KMachineState_Stuck:
            /* Start with ignoring the close event */
            pEvent->ignore();

            bool isACPIEnabled = machineLogic()->session().GetConsole().GetGuestEnteredACPIMode();

            bool success = true;

            bool wasPaused = machineLogic()->machineState() == KMachineState_Paused ||
                             machineLogic()->machineState() == KMachineState_Stuck ||
                             machineLogic()->machineState() == KMachineState_TeleportingPausedVM;
            if (!wasPaused)
            {
                /* Suspend the VM and ignore the close event if failed to do so.
                 * pause() will show the error message to the user. */
                success = machineLogic()->pause(true);
            }

            if (success)
            {
                success = false;

                CMachine machine = machineLogic()->session().GetMachine();
                VBoxCloseVMDlg dlg(machineWindow());
                QString typeId = machine.GetOSTypeId();
                dlg.pmIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(typeId));

                /* Make the Discard checkbox invisible if there are no snapshots */
                dlg.mCbDiscardCurState->setVisible(machine.GetSnapshotCount() > 0);
                if (!machine.GetCurrentSnapshot().isNull())
                    dlg.mCbDiscardCurState->setText(dlg.mCbDiscardCurState->text().arg(machine.GetCurrentSnapshot().GetName()));

                if (machineLogic()->machineState() != KMachineState_Stuck)
                {
                    /* Read the last user's choice for the given VM */
                    QStringList lastAction = machine.GetExtraData(VBoxDefs::GUI_LastCloseAction).split(',');
                    AssertWrapperOk(machine);
                    if (lastAction[0] == pstrSave)
                    {
                        dlg.mRbShutdown->setEnabled(isACPIEnabled);
                        dlg.mRbSave->setChecked(true);
                        dlg.mRbSave->setFocus();
                    }
                    else if (lastAction[0] == pstrPowerOff || !isACPIEnabled)
                    {
                        dlg.mRbShutdown->setEnabled(isACPIEnabled);
                        dlg.mRbPowerOff->setChecked(true);
                        dlg.mRbPowerOff->setFocus();
                    }
                    else /* The default is ACPI Shutdown */
                    {
                        dlg.mRbShutdown->setChecked(true);
                        dlg.mRbShutdown->setFocus();
                    }
                    dlg.mCbDiscardCurState->setChecked(lastAction.count() > 1 && lastAction [1] == pstrDiscardCurState);
                }
                else
                {
                    /* The stuck VM can only be powered off; disable anything else and choose PowerOff */
                    dlg.mRbSave->setEnabled(false);
                    dlg.mRbShutdown->setEnabled(false);
                    dlg.mRbPowerOff->setChecked(true);
                }

                bool wasShutdown = false;

                if (dlg.exec() == QDialog::Accepted)
                {
                    /* Disable auto closure because we want to have a chance to show
                     * the error dialog on save state / power off failure. */
                    // TODO: process for multiple windows!
                    //m_bNoAutoClose = true;

                    CConsole console = machineLogic()->session().GetConsole();

                    if (dlg.mRbSave->isChecked())
                    {
                        CProgress progress = console.SaveState();

                        if (console.isOk())
                        {
                            /* Show the "VM saving" progress dialog */
                            vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindow(), 0);
                            if (progress.GetResultCode() != 0)
                                vboxProblem().cannotSaveMachineState(progress);
                            else
                                success = true;
                        }
                        else
                            vboxProblem().cannotSaveMachineState(console);
                    }
                    else if (dlg.mRbShutdown->isChecked())
                    {
                        /* Unpause the VM to let it grab the ACPI shutdown event */
                        machineLogic()->pause(false);
                        /* Prevent the subsequent unpause request */
                        wasPaused = true;
                        /* Signal ACPI shutdown (if there is no ACPI device, the
                         * operation will fail) */
                        console.PowerButton();
                        wasShutdown = console.isOk();
                        if (!wasShutdown)
                            vboxProblem().cannotACPIShutdownMachine(console);
                        /* Success is always false because we never accept the close
                         * window action when doing ACPI shutdown */
                        success = false;
                    }
                    else if (dlg.mRbPowerOff->isChecked())
                    {
                        CProgress progress = console.PowerDown();

                        if (console.isOk())
                        {
                            /* Show the power down progress dialog */
                            vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindow());
                            if (progress.GetResultCode() != 0)
                                vboxProblem().cannotStopMachine(progress);
                            else
                                success = true;
                        }
                        else
                            vboxProblem().cannotStopMachine(console);

                        if (success)
                        {
                            /* Note: leave success = true even if we fail to
                             * discard the current state later -- the console window
                             * will closed anyway */

                            /* Discard the current state if requested */
                            if (dlg.mCbDiscardCurState->isChecked() && dlg.mCbDiscardCurState->isVisibleTo(&dlg))
                            {
                                CSnapshot snapshot = machine.GetCurrentSnapshot();
                                CProgress progress = console.RestoreSnapshot(snapshot);
                                if (console.isOk())
                                {
                                    /* Show the progress dialog */
                                    vboxProblem().showModalProgressDialog(progress, machine.GetName(), machineWindow());
                                    if (progress.GetResultCode() != 0)
                                        vboxProblem().cannotRestoreSnapshot(progress, snapshot.GetName());
                                }
                                else
                                    vboxProblem().cannotRestoreSnapshot(console, snapshot.GetName());
                            }
                        }
                    }

                    if (success)
                    {
                        /* Accept the close action on success */
                        pEvent->accept();
                    }

                    if (success || wasShutdown)
                    {
                        /* Read the last user's choice for the given VM */
                        QStringList prevAction = machine.GetExtraData(VBoxDefs::GUI_LastCloseAction).split(',');
                        /* Memorize the last user's choice for the given VM */
                        QString lastAction = pstrPowerOff;
                        if (dlg.mRbSave->isChecked())
                            lastAction = pstrSave;
                        else if (dlg.mRbShutdown->isChecked() ||
                                 (dlg.mRbPowerOff->isChecked() && prevAction [0] == pstrShutdown && !isACPIEnabled))
                            lastAction = pstrShutdown;
                        else if (dlg.mRbPowerOff->isChecked())
                            lastAction = pstrPowerOff;
                        else
                            AssertFailed();
                        if (dlg.mCbDiscardCurState->isChecked())
                            (lastAction += ",") += pstrDiscardCurState;
                        machine.SetExtraData (VBoxDefs::GUI_LastCloseAction, lastAction);
                        AssertWrapperOk(machine);
                    }
                }
            }

            // TODO: process for multiple windows!
            //m_bNoAutoClose = false;

            if (machineLogic()->machineState() == KMachineState_PoweredOff ||
                machineLogic()->machineState() == KMachineState_Saved ||
                machineLogic()->machineState() == KMachineState_Teleported ||
                machineLogic()->machineState() == KMachineState_Aborted)
            {
                /* The machine has been stopped while showing the Close or the Pause
                 * failure dialog -- accept the close event immediately. */
                pEvent->accept();
            }
            else
            {
                if (!success)
                {
                    /* Restore the running state if needed */
                    if (!wasPaused && machineLogic()->machineState() == KMachineState_Paused)
                        machineLogic()->pause(false);
                }
            }
            break;
    }

    if (pEvent->isAccepted())
    {
#ifndef VBOX_GUI_SEPARATE_VM_PROCESS
        vboxGlobal().selectorWnd().show();
#endif

        /* Hide console window */
        machineWindow()->hide();

#ifdef VBOX_WITH_DEBUGGER_GUI
        /* Close & destroy the debugger GUI */
        // TODO: Check that logic!
        //dbgDestroy();
#endif

        /* Make sure all events are delievered */
        qApp->processEvents();

        /* Notify all the top-level dialogs about closing */
        // TODO: Notify about closing!
        // emit closing();
    }
}

void UIMachineWindow::prepareWindowIcon()
{
#if !(defined (Q_WS_WIN) || defined (Q_WS_MAC))
    /* The default application icon (will be changed to VM-specific icon little bit later):
     * 1. On Win32, it's built-in to the executable;
     * 2. On Mac OS X the icon referenced in info.plist is used. */
    machineWindow()->setWindowIcon(QIcon(":/VirtualBox_48px.png"));
#endif

#ifndef Q_WS_MAC
    /* Set the VM-specific application icon except Mac OS X: */
    CMachine machine = machineLogic()->session().GetMachine();
    machineWindow()->setWindowIcon(vboxGlobal().vmGuestOSTypeIcon(machine.GetOSTypeId()));
#endif
}

void UIMachineWindow::loadWindowSettings()
{
#ifdef Q_WS_MAC
    QString testStr = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_RealtimeDockIconUpdateEnabled).toLower();
    /* Default to true if it is an empty value */
    bool bIsDockIconEnabled = testStr.isEmpty() || testStr == "true";
    machineView()->setDockIconEnabled(bIsDockIconEnabled);
    machineView()->updateDockOverlay();
#endif
}
