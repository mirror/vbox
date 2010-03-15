/* $Id$ */
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
#include <QTimer>

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxCloseVMDlg.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineView.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineWindowSeamless.h"

UIMachineWindow* UIMachineWindow::create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType, ulong uScreenId)
{
    UIMachineWindow *window = 0;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            window = new UIMachineWindowNormal(pMachineLogic, uScreenId);
            break;
        case UIVisualStateType_Fullscreen:
            window = new UIMachineWindowFullscreen(pMachineLogic, uScreenId);
            break;
        case UIVisualStateType_Seamless:
            window = new UIMachineWindowSeamless(pMachineLogic, uScreenId);
            break;
    }
    return window;
}

void UIMachineWindow::destroy(UIMachineWindow *pWhichWindow)
{
    delete pWhichWindow;
}

void UIMachineWindow::sltTryClose()
{
    /* Do not try to close if restricted: */
    if (machineLogic()->isPreventAutoClose())
        return;

    /* First close any open modal & popup widgets.
     * Use a single shot with timeout 0 to allow the widgets to cleany close and test then again.
     * If all open widgets are closed destroy ourself: */
    QWidget *widget = QApplication::activeModalWidget() ?
                      QApplication::activeModalWidget() :
                      QApplication::activePopupWidget() ?
                      QApplication::activePopupWidget() : 0;
    if (widget)
    {
        widget->close();
        QTimer::singleShot(0, machineWindow(), SLOT(sltTryClose()));
    }
    else
        machineWindow()->close();
}

UIMachineWindow::UIMachineWindow(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : m_pMachineLogic(pMachineLogic)
    , m_pMachineWindow(0)
    , m_uScreenId(uScreenId)
    , m_pMachineViewContainer(0)
    , m_pTopSpacer(0)
    , m_pBottomSpacer(0)
    , m_pLeftSpacer(0)
    , m_pRightSpacer(0)
    , m_pMachineView(0)
{
}

UIMachineWindow::~UIMachineWindow()
{
}

UISession* UIMachineWindow::uisession() const
{
    return machineLogic()->uisession();
}

CSession& UIMachineWindow::session() const
{
    return uisession()->session();
}

void UIMachineWindow::retranslateUi()
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

void UIMachineWindow::closeEvent(QCloseEvent *pEvent)
{
    /* Always ignore close event: */
    pEvent->ignore();

    /* Should we close application? */
    bool fCloseApplication = false;

    switch (uisession()->machineState())
    {
        case KMachineState_Running:
        case KMachineState_Paused:
        case KMachineState_Stuck:
        case KMachineState_LiveSnapshotting:
        case KMachineState_Teleporting: // TODO: Test this!
        case KMachineState_TeleportingPausedVM: // TODO: Test this!
        {
            bool success = true;

            /* Get the ACPI status early before pausing VM: */
            bool isACPIEnabled = session().GetConsole().GetGuestEnteredACPIMode();

            bool fWasPaused = uisession()->isPaused() || uisession()->machineState() == KMachineState_Stuck;
            if (!fWasPaused)
            {
                /* Suspend the VM and ignore the close event if failed to do so.
                 * pause() will show the error message to the user: */
                success = uisession()->pause();
            }

            if (success)
            {
                success = false;

                /* Preventing closure right after we had paused: */
                machineLogic()->setPreventAutoClose(true);

                /* Get the machine: */
                CMachine machine = session().GetMachine();

                /* Prepare close dialog: */
                VBoxCloseVMDlg dlg(machineWindow());
                QString typeId = machine.GetOSTypeId();
                dlg.pmIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(typeId));

                /* Make the discard checkbox invisible if there are no snapshots: */
                dlg.mCbDiscardCurState->setVisible(machine.GetSnapshotCount() > 0);
                if (!machine.GetCurrentSnapshot().isNull())
                    dlg.mCbDiscardCurState->setText(dlg.mCbDiscardCurState->text().arg(machine.GetCurrentSnapshot().GetName()));

                /* Choice string tags for close-dialog: */
                QString strSave("save");
                QString strShutdown("shutdown");
                QString strPowerOff("powerOff");
                QString strDiscardCurState("discardCurState");

                if (uisession()->machineState() != KMachineState_Stuck)
                {
                    /* Read the last user's choice for the given VM: */
                    QStringList lastAction = machine.GetExtraData(VBoxDefs::GUI_LastCloseAction).split(',');
                    AssertWrapperOk(machine);
                    if (lastAction[0] == strSave)
                    {
                        dlg.mRbShutdown->setEnabled(isACPIEnabled);
                        dlg.mRbSave->setChecked(true);
                        dlg.mRbSave->setFocus();
                    }
                    else if (lastAction[0] == strPowerOff || !isACPIEnabled)
                    {
                        dlg.mRbShutdown->setEnabled(isACPIEnabled);
                        dlg.mRbPowerOff->setChecked(true);
                        dlg.mRbPowerOff->setFocus();
                    }
                    else /* The default is ACPI Shutdown: */
                    {
                        dlg.mRbShutdown->setChecked(true);
                        dlg.mRbShutdown->setFocus();
                    }
                    dlg.mCbDiscardCurState->setChecked(lastAction.count() > 1 && lastAction [1] == strDiscardCurState);
                }
                else
                {
                    /* The stuck VM can only be powered off; disable anything else and choose PowerOff: */
                    dlg.mRbSave->setEnabled(false);
                    dlg.mRbShutdown->setEnabled(false);
                    dlg.mRbPowerOff->setChecked(true);
                }

                bool fWasShutdown = false;

                /* If close dialog accepted: */
                if (dlg.exec() == QDialog::Accepted)
                {
                    CConsole console = session().GetConsole();

                    if (dlg.mRbSave->isChecked())
                    {
                        CProgress progress = console.SaveState();

                        if (console.isOk())
                        {
                            /* Show the "VM saving" progress dialog: */
                            vboxProblem().showModalProgressDialog(progress, machine.GetName(), 0, 0);
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
                        /* Unpause the VM to let it grab the ACPI shutdown event: */
                        uisession()->unpause();
                        /* Prevent the subsequent unpause request: */
                        fWasPaused = true;
                        /* Signal ACPI shutdown (if there is no ACPI device, the operation will fail): */
                        console.PowerButton();
                        fWasShutdown = console.isOk();
                        if (!fWasShutdown)
                            vboxProblem().cannotACPIShutdownMachine(console);
                        /* Success is always false because we never accept the close
                         * window action when doing ACPI shutdown: */
                        success = false;
                    }
                    else if (dlg.mRbPowerOff->isChecked())
                    {
                        CProgress progress = console.PowerDown();

                        if (console.isOk())
                        {
                            /* Show the power down progress dialog: */
                            vboxProblem().showModalProgressDialog(progress, machine.GetName(), 0);
                            if (progress.GetResultCode() != 0)
                                vboxProblem().cannotStopMachine(progress);
                            else
                                success = true;
                        }
                        else
                            vboxProblem().cannotStopMachine(console);

                        if (success)
                        {
                            /* Discard the current state if requested: */
                            if (dlg.mCbDiscardCurState->isChecked() && dlg.mCbDiscardCurState->isVisibleTo(&dlg))
                            {
                                CSnapshot snapshot = machine.GetCurrentSnapshot();
                                CProgress progress = console.RestoreSnapshot(snapshot);
                                if (console.isOk())
                                {
                                    /* Show the progress dialog: */
                                    vboxProblem().showModalProgressDialog(progress, machine.GetName(), 0);
                                    if (progress.GetResultCode() != 0)
                                        vboxProblem().cannotRestoreSnapshot(progress, snapshot.GetName());
                                }
                                else
                                    vboxProblem().cannotRestoreSnapshot(console, snapshot.GetName());
                            }
                        }
                    }

                    if (success || fWasShutdown)
                    {
                        /* Read the last user's choice for the given VM: */
                        QStringList prevAction = machine.GetExtraData(VBoxDefs::GUI_LastCloseAction).split(',');
                        /* Memorize the last user's choice for the given VM: */
                        QString lastAction = strPowerOff;
                        if (dlg.mRbSave->isChecked())
                            lastAction = strSave;
                        else if (dlg.mRbShutdown->isChecked() ||
                                 (dlg.mRbPowerOff->isChecked() && prevAction [0] == strShutdown && !isACPIEnabled))
                            lastAction = strShutdown;
                        else if (dlg.mRbPowerOff->isChecked())
                            lastAction = strPowerOff;
                        else
                            AssertFailed();
                        if (dlg.mCbDiscardCurState->isChecked())
                            (lastAction += ",") += strDiscardCurState;
                        machine.SetExtraData (VBoxDefs::GUI_LastCloseAction, lastAction);
                        AssertWrapperOk(machine);
                    }

                    fCloseApplication = success;
                }
            }

            if (!success)
            {
                /* Restore the running state if needed: */
                if (!fWasPaused && uisession()->machineState() == KMachineState_Paused)
                    uisession()->unpause();
            }
            break;
        }

        default:
            break;
    }

    /* Allowing closure: */
    machineLogic()->setPreventAutoClose(false);

    if (fCloseApplication)
    {
        /* VM has been powered off or saved. We must *safely* close the VM window(s): */
        QTimer::singleShot(0, uisession(), SLOT(sltCloseVirtualSession()));
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
    machineWindow()->setWindowIcon(vboxGlobal().vmGuestOSTypeIcon(session().GetMachine().GetOSTypeId()));
#endif
}

void UIMachineWindow::prepareConsoleConnections()
{
    /* Machine state-change updater: */
    QObject::connect(uisession(), SIGNAL(sigMachineStateChange()), machineWindow(), SLOT(sltMachineStateChanged()));
}

void UIMachineWindow::prepareAdditionsDownloader()
{
}

void UIMachineWindow::prepareMachineViewContainer()
{
    /* Create view container.
     * After it will be passed to parent widget of some mode,
     * there will be no need to delete it, so no need to cleanup: */
    m_pMachineViewContainer = new QGridLayout();
    m_pMachineViewContainer->setMargin(0);
    m_pMachineViewContainer->setSpacing(0);

    /* Create and add shifting spacers.
     * After they will be inserted into layout, it will get the parentness
     * of those spacers, so there will be no need to cleanup them. */
    m_pTopSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_pBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_pLeftSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pRightSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_pMachineViewContainer->addItem(m_pTopSpacer, 0, 1);
    m_pMachineViewContainer->addItem(m_pBottomSpacer, 2, 1);
    m_pMachineViewContainer->addItem(m_pLeftSpacer, 1, 0);
    m_pMachineViewContainer->addItem(m_pRightSpacer, 1, 2);
}

void UIMachineWindow::updateAppearanceOf(int iElement)
{
    CMachine machine = session().GetMachine();

    if (iElement & UIVisualElement_WindowCaption)
    {
        /* Get machine state: */
        KMachineState state = uisession()->machineState();
        /* Prepare full name: */
        QString strSnapshotName;
        if (machine.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = machine.GetCurrentSnapshot();
            strSnapshotName = " (" + snapshot.GetName() + ")";
        }
        QString strMachineName = machine.GetName() + strSnapshotName;
        if (state != KMachineState_Null)
            strMachineName += " [" + vboxGlobal().toString(state) + "] - ";
        strMachineName += m_strWindowTitlePrefix;
        if (machine.GetMonitorCount() > 1)
            strMachineName += QString(" : %1").arg(m_uScreenId + 1);
        machineWindow()->setWindowTitle(strMachineName);
    }
}

void UIMachineWindow::sltMachineStateChanged()
{
    updateAppearanceOf(UIVisualElement_WindowCaption);
}

