/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindow class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <QCloseEvent>
#include <QTimer>

#include <VBox/version.h>
#ifdef VBOX_BLEEDING_EDGE
# include <iprt/buildconfig.h>
#endif /* VBOX_BLEEDING_EDGE */

/* Local includes */
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

#include "UIKeyboardHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindow.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineWindowScale.h"
#include "UIMachineWindowSeamless.h"
#include "UIMouseHandler.h"
#include "UISession.h"
#include "UIVMCloseDialog.h"

#ifdef Q_WS_X11
# include <X11/Xlib.h>
#endif

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
        case UIVisualStateType_Scale:
            window = new UIMachineWindowScale(pMachineLogic, uScreenId);
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
     * Use a single shot with timeout 0 to allow the widgets to cleanly close and test then again.
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
    /* Close any opened modal & popup widgets: */
    while (QWidget *pWidget = QApplication::activeModalWidget() ? QApplication::activeModalWidget() :
                              QApplication::activePopupWidget() ? QApplication::activePopupWidget() : 0)
    {
        /* Set modal/popup window's parent to null early,
         * because deleteLater() is synchronous
         * and will be called later than this destructor: */
        pWidget->setParent(0);
        /* Close modal/popup window early to hide it
         * because deleteLater() is synchronous
         * and will be called later than this destructor: */
        pWidget->close();
        /* Delete modal/popup window synchronously (safe): */
        pWidget->deleteLater();
    }
}

UISession* UIMachineWindow::uisession() const
{
    return machineLogic()->uisession();
}

CSession& UIMachineWindow::session() const
{
    return uisession()->session();
}

void UIMachineWindow::setMask(const QRegion &region)
{
    machineWindow()->setMask(region);
}

void UIMachineWindow::retranslateUi()
{
    m_strWindowTitlePrefix = VBOX_PRODUCT;
#ifdef VBOX_BLEEDING_EDGE
    m_strWindowTitlePrefix += UIMachineLogic::tr(" EXPERIMENTAL build %1r%2 - %3")
                              .arg(RTBldCfgVersion())
                              .arg(RTBldCfgRevisionStr())
                              .arg(VBOX_BLEEDING_EDGE);
#endif
    updateAppearanceOf(UIVisualElement_WindowCaption);
}

#ifdef Q_WS_X11
bool UIMachineWindow::x11Event(XEvent *pEvent)
{
    // TODO: Check if that is still required!
    /* Qt bug: when the machine-view grabs the keyboard,
     * FocusIn, FocusOut, WindowActivate and WindowDeactivate Qt events are
     * not properly sent on top level window deactivation.
     * The fix is to substiute the mode in FocusOut X11 event structure
     * to NotifyNormal to cause Qt to process it as desired. */
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
            /* Get the machine: */
            CMachine machine = session().GetMachine();

            /* Check if there is a close hock script defined. */
            const QString& strScript = machine.GetExtraData(VBoxDefs::GUI_CloseActionHook);
            if (!strScript.isEmpty())
            {
                QProcess::startDetached(strScript, QStringList() << machine.GetId());
                return;
            }
            /* Prepare close dialog: */
            UIVMCloseDialog dlg(machineWindow());

            /* Assign close-dialog pixmap: */
            dlg.pmIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(machine.GetOSTypeId()));

            /* Check which close actions are disallowed: */
            QStringList restictedActionsList = machine.GetExtraData(VBoxDefs::GUI_RestrictedCloseActions).split(',');
            bool fIsStateSavingAllowed = !restictedActionsList.contains("SaveState", Qt::CaseInsensitive);
            bool fIsACPIShutdownAllowed = !restictedActionsList.contains("Shutdown", Qt::CaseInsensitive);
            bool fIsPowerOffAllowed = !restictedActionsList.contains("PowerOff", Qt::CaseInsensitive);
            bool fIsPowerOffAndRestoreAllowed = fIsPowerOffAllowed && !restictedActionsList.contains("Restore", Qt::CaseInsensitive);

            /* Make Save State button visible/hidden depending on restriction: */
            dlg.mRbSave->setVisible(fIsStateSavingAllowed);
            dlg.mTxSave->setVisible(fIsStateSavingAllowed);
            /* Make Save State button enabled/disabled depending on machine state: */
            dlg.mRbSave->setEnabled(uisession()->machineState() != KMachineState_Stuck);

            /* Make ACPI shutdown button visible/hidden depending on restriction: */
            dlg.mRbShutdown->setVisible(fIsACPIShutdownAllowed);
            dlg.mTxShutdown->setVisible(fIsACPIShutdownAllowed);
            /* Make ACPI shutdown button enabled/disabled depending on ACPI state & machine state: */
            bool isACPIEnabled = session().GetConsole().GetGuestEnteredACPIMode();
            dlg.mRbShutdown->setEnabled(isACPIEnabled && uisession()->machineState() != KMachineState_Stuck);

            /* Make Power Off button visible/hidden depending on restriction: */
            dlg.mRbPowerOff->setVisible(fIsPowerOffAllowed);
            dlg.mTxPowerOff->setVisible(fIsPowerOffAllowed);

            /* Make the Restore Snapshot checkbox visible/hidden depending on snapshots count & restrictions: */
            dlg.mCbDiscardCurState->setVisible(fIsPowerOffAndRestoreAllowed && machine.GetSnapshotCount() > 0);
            if (!machine.GetCurrentSnapshot().isNull())
                dlg.mCbDiscardCurState->setText(dlg.mCbDiscardCurState->text().arg(machine.GetCurrentSnapshot().GetName()));

            /* Choice string tags for close-dialog: */
            QString strSave("save");
            QString strShutdown("shutdown");
            QString strPowerOff("powerOff");
            QString strDiscardCurState("discardCurState");

            /* Read the last user's choice for the given VM: */
            QStringList lastAction = machine.GetExtraData(VBoxDefs::GUI_LastCloseAction).split(',');

            /* Check which button should be initially chosen: */
            QRadioButton *pRadioButton = 0;

            /* If choosing 'last choice' is possible: */
            if (lastAction[0] == strSave && fIsStateSavingAllowed)
            {
                pRadioButton = dlg.mRbSave;
            }
            else if (lastAction[0] == strShutdown && fIsACPIShutdownAllowed && isACPIEnabled)
            {
                pRadioButton = dlg.mRbShutdown;
            }
            else if (lastAction[0] == strPowerOff && fIsPowerOffAllowed)
            {
                pRadioButton = dlg.mRbPowerOff;
                if (fIsPowerOffAndRestoreAllowed)
                    dlg.mCbDiscardCurState->setChecked(lastAction.count() > 1 && lastAction[1] == strDiscardCurState);
            }
            /* Else 'default choice' will be used: */
            else
            {
                if (fIsACPIShutdownAllowed && isACPIEnabled)
                    pRadioButton = dlg.mRbShutdown;
                else if (fIsPowerOffAllowed)
                    pRadioButton = dlg.mRbPowerOff;
                else if (fIsStateSavingAllowed)
                    pRadioButton = dlg.mRbSave;
            }

            /* If some radio button was chosen: */
            if (pRadioButton)
            {
                /* Check and focus it: */
                pRadioButton->setChecked(true);
                pRadioButton->setFocus();
            }
            /* If no one of radio buttons was chosen: */
            else
            {
                /* Just break and leave: */
                break;
            }

            /* This flag will keep the status of every further logical operation: */
            bool success = true;

            /* Pause before showing dialog if necessary: */
            bool fWasPaused = uisession()->isPaused() || uisession()->machineState() == KMachineState_Stuck;
            if (!fWasPaused)
                success = uisession()->pause();

            if (success)
            {
                /* Preventing auto-closure: */
                machineLogic()->setPreventAutoClose(true);

                /* If close dialog accepted: */
                if (dlg.exec() == QDialog::Accepted)
                {
                    /* Get current console: */
                    CConsole console = session().GetConsole();

                    success = false;

                    if (dlg.mRbSave->isChecked())
                    {
                        CProgress progress = console.SaveState();

                        if (!console.isOk())
                            msgCenter().cannotSaveMachineState(console);
                        else
                        {
                            /* Show the "VM saving" progress dialog: */
                            msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_state_save_90px.png", 0, true);
                            if (progress.GetResultCode() != 0)
                                msgCenter().cannotSaveMachineState(progress);
                            else
                                success = true;
                        }

                        if (success)
                            fCloseApplication = true;
                    }
                    else if (dlg.mRbShutdown->isChecked())
                    {
                        /* Unpause the VM to let it grab the ACPI shutdown event: */
                        uisession()->unpause();
                        /* Prevent the subsequent unpause request: */
                        fWasPaused = true;
                        /* Signal ACPI shutdown (if there is no ACPI device, the operation will fail): */
                        console.PowerButton();
                        if (!console.isOk())
                            msgCenter().cannotACPIShutdownMachine(console);
                        else
                            success = true;
                    }
                    else if (dlg.mRbPowerOff->isChecked())
                    {
                        CProgress progress = console.PowerDown();

                        if (!console.isOk())
                            msgCenter().cannotStopMachine(console);
                        else
                        {
                            /* Show the power down progress dialog: */
                            msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_poweroff_90px.png", 0, true);
                            if (progress.GetResultCode() != 0)
                                msgCenter().cannotStopMachine(progress);
                            else
                                success = true;
                        }

                        if (success)
                        {
                            /* Discard the current state if requested: */
                            if (dlg.mCbDiscardCurState->isChecked() && dlg.mCbDiscardCurState->isVisibleTo(&dlg))
                            {
                                CSnapshot snapshot = machine.GetCurrentSnapshot();
                                CProgress progress = console.RestoreSnapshot(snapshot);
                                if (!console.isOk())
                                    msgCenter().cannotRestoreSnapshot(console, snapshot.GetName());
                                else
                                {
                                    /* Show the progress dialog: */
                                    msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_discard_90px.png", 0, true);
                                    if (progress.GetResultCode() != 0)
                                        msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName());
                                }
                            }
                        }

                        if (success)
                            fCloseApplication = true;
                    }

                    if (success)
                    {
                        /* Read the last user's choice for the given VM: */
                        QStringList prevAction = machine.GetExtraData(VBoxDefs::GUI_LastCloseAction).split(',');
                        /* Memorize the last user's choice for the given VM: */
                        QString lastAction = strPowerOff;
                        if (dlg.mRbSave->isChecked())
                            lastAction = strSave;
                        else if ((dlg.mRbShutdown->isChecked()) ||
                                 (dlg.mRbPowerOff->isChecked() && prevAction[0] == strShutdown && !isACPIEnabled))
                            lastAction = strShutdown;
                        else if (dlg.mRbPowerOff->isChecked())
                            lastAction = strPowerOff;
                        else
                            AssertFailed();
                        if (dlg.mCbDiscardCurState->isChecked())
                            (lastAction += ",") += strDiscardCurState;
                        machine.SetExtraData(VBoxDefs::GUI_LastCloseAction, lastAction);
                    }
                }

                /* Restore the running state if needed: */
                if (success && !fCloseApplication && !fWasPaused && uisession()->machineState() == KMachineState_Paused)
                    uisession()->unpause();

                /* Allowing auto-closure: */
                machineLogic()->setPreventAutoClose(false);
            }

            break;
        }

        default:
            break;
    }

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

    /* Guest monitor change updater: */
    QObject::connect(uisession(), SIGNAL(sigGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)),
                     machineWindow(), SLOT(sltGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)));
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

void UIMachineWindow::prepareHandlers()
{
    /* Register keyboard-handler: */
    machineLogic()->keyboardHandler()->prepareListener(m_uScreenId, this);

    /* Register mouse-handler: */
    machineLogic()->mouseHandler()->prepareListener(m_uScreenId, this);
}

void UIMachineWindow::cleanupHandlers()
{
    /* Unregister mouse-handler: */
    machineLogic()->mouseHandler()->cleanupListener(m_uScreenId);

    /* Unregister keyboard-handler: */
    machineLogic()->keyboardHandler()->cleanupListener(m_uScreenId);
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
            strMachineName += " [" + vboxGlobal().toString(state) + "]";
        /* Unusual on the Mac. */
#ifndef Q_WS_MAC
        strMachineName += " - " + m_strWindowTitlePrefix;
#endif /* Q_WS_MAC */
        if (machine.GetMonitorCount() > 1)
            strMachineName += QString(" : %1").arg(m_uScreenId + 1);
        machineWindow()->setWindowTitle(strMachineName);
    }
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineWindow::updateDbgWindows()
{
    /* The debugger windows are bind to the main VM window. */
    if (m_uScreenId == 0)
        machineLogic()->dbgAdjustRelativePos();
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineWindow::sltMachineStateChanged()
{
    updateAppearanceOf(UIVisualElement_WindowCaption);
}

void UIMachineWindow::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect /* screenGeo */)
{
    /* Ignore change events for other screens: */
    if (uScreenId != m_uScreenId)
        return;

    /* Ignore KGuestMonitorChangedEventType_NewOrigin change event: */
    if (changeType == KGuestMonitorChangedEventType_NewOrigin)
        return;

    /* Process KGuestMonitorChangedEventType_Enabled change event: */
    if (machineWindow()->isHidden() && changeType == KGuestMonitorChangedEventType_Enabled)
        showInNecessaryMode();
    /* Process KGuestMonitorChangedEventType_Disabled change event: */
    else if (!machineWindow()->isHidden() && changeType == KGuestMonitorChangedEventType_Disabled)
        machineWindow()->hide();
}

