/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindow class implementation
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

/* Qt includes: */
#include <QCloseEvent>
#include <QTimer>
#include <QProcess>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIKeyboardHandler.h"
#include "UIMachineWindow.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineWindowSeamless.h"
#include "UIMachineWindowScale.h"
#include "UIMouseHandler.h"
#include "UISession.h"
#include "UIVMCloseDialog.h"
#include "UIConverter.h"

/* COM includes: */
#include "CConsole.h"
#include "CSnapshot.h"

/* Other VBox includes: */
#include <VBox/version.h>
#ifdef VBOX_BLEEDING_EDGE
# include <iprt/buildconfig.h>
#endif /* VBOX_BLEEDING_EDGE */

/* External includes: */
#ifdef Q_WS_X11
# include <X11/Xlib.h>
#endif /* Q_WS_X11 */

/* static */
UIMachineWindow* UIMachineWindow::create(UIMachineLogic *pMachineLogic, ulong uScreenId)
{
    /* Create machine-window: */
    UIMachineWindow *pMachineWindow = 0;
    switch (pMachineLogic->visualStateType())
    {
        case UIVisualStateType_Normal:
            pMachineWindow = new UIMachineWindowNormal(pMachineLogic, uScreenId);
            break;
        case UIVisualStateType_Fullscreen:
            pMachineWindow = new UIMachineWindowFullscreen(pMachineLogic, uScreenId);
            break;
        case UIVisualStateType_Seamless:
            pMachineWindow = new UIMachineWindowSeamless(pMachineLogic, uScreenId);
            break;
        case UIVisualStateType_Scale:
            pMachineWindow = new UIMachineWindowScale(pMachineLogic, uScreenId);
            break;
        default:
            AssertMsgFailed(("Incorrect visual state!"));
            break;
    }
    /* Prepare machine-window: */
    pMachineWindow->prepare();
    /* Return machine-window: */
    return pMachineWindow;
}

/* static */
void UIMachineWindow::destroy(UIMachineWindow *pWhichWindow)
{
    /* Cleanup machine-window: */
    pWhichWindow->cleanup();
    /* Delete machine-window: */
    delete pWhichWindow;
}

void UIMachineWindow::prepare()
{
    /* Prepare session-connections: */
    prepareSessionConnections();

    /* Prepare main-layout: */
    prepareMainLayout();

    /* Prepare menu: */
    prepareMenu();

    /* Prepare status-bar: */
    prepareStatusBar();

    /* Prepare machine-view: */
    prepareMachineView();

    /* Prepare visual-state: */
    prepareVisualState();

    /* Prepare handlers: */
    prepareHandlers();

    /* Load settings: */
    loadSettings();

    /* Retranslate window: */
    retranslateUi();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Show: */
    showInNecessaryMode();
}

void UIMachineWindow::cleanup()
{
    /* Save window settings: */
    saveSettings();

    /* Cleanup handlers: */
    cleanupHandlers();

    /* Cleanup visual-state: */
    cleanupVisualState();

    /* Cleanup machine-view: */
    cleanupMachineView();

    /* Cleanup status-bar: */
    cleanupStatusBar();

    /* Cleanup menu: */
    cleanupMenu();

    /* Cleanup main layout: */
    cleanupMainLayout();

    /* Cleanup session connections: */
    cleanupSessionConnections();
}

void UIMachineWindow::sltMachineStateChanged()
{
    /* Update window-title: */
    updateAppearanceOf(UIVisualElement_WindowTitle);
}

void UIMachineWindow::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect /* screenGeo */)
{
    /* Ignore change events for other screens: */
    if (uScreenId != m_uScreenId)
        return;
    /* Ignore KGuestMonitorChangedEventType_NewOrigin change event: */
    if (changeType == KGuestMonitorChangedEventType_NewOrigin)
        return;
    /* Ignore KGuestMonitorChangedEventType_Disabled event if there is only one window visible: */
    AssertMsg(uisession()->countOfVisibleWindows() > 0, ("All machine windows are hidden!"));
    if ((changeType == KGuestMonitorChangedEventType_Disabled) &&
        (uisession()->countOfVisibleWindows() == 1))
        return;

    /* Process KGuestMonitorChangedEventType_Enabled change event: */
    if (isHidden() && changeType == KGuestMonitorChangedEventType_Enabled)
        uisession()->setScreenVisible(m_uScreenId, true);
    /* Process KGuestMonitorChangedEventType_Disabled change event: */
    else if (!isHidden() && changeType == KGuestMonitorChangedEventType_Disabled)
        uisession()->setScreenVisible(m_uScreenId, false);

    /* Update screen visibility status: */
    showInNecessaryMode();
}

UIMachineWindow::UIMachineWindow(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI2<QMainWindow>(0, windowFlags(pMachineLogic->visualStateType()))
    , m_pMachineLogic(pMachineLogic)
    , m_pMachineView(0)
    , m_uScreenId(uScreenId)
    , m_pMainLayout(0)
    , m_pTopSpacer(0)
    , m_pBottomSpacer(0)
    , m_pLeftSpacer(0)
    , m_pRightSpacer(0)
{
#ifndef Q_WS_MAC
    /* On Mac OS X application icon referenced in info.plist is used. */

    /* Set default application icon (will be changed to VM-specific icon little bit later): */
    setWindowIcon(QIcon(":/VirtualBox_48px.png"));

    /* Set VM-specific application icon: */
    setWindowIcon(vboxGlobal().vmGuestOSTypeIcon(machine().GetOSTypeId()));
#endif /* !Q_WS_MAC */

    /* Set the main application window for VBoxGlobal: */
    if (m_uScreenId == 0)
        vboxGlobal().setMainWindow(this);
}

UISession* UIMachineWindow::uisession() const
{
    return machineLogic()->uisession();
}

CSession& UIMachineWindow::session() const
{
    return uisession()->session();
}

CMachine UIMachineWindow::machine() const
{
    return session().GetMachine();
}

void UIMachineWindow::retranslateUi()
{
    /* Compose window-title prefix: */
    m_strWindowTitlePrefix = VBOX_PRODUCT;
#ifdef VBOX_BLEEDING_EDGE
    m_strWindowTitlePrefix += UIMachineWindow::tr(" EXPERIMENTAL build %1r%2 - %3")
                              .arg(RTBldCfgVersion())
                              .arg(RTBldCfgRevisionStr())
                              .arg(VBOX_BLEEDING_EDGE);
#endif /* VBOX_BLEEDING_EDGE */
    /* Update appearance of the window-title: */
    updateAppearanceOf(UIVisualElement_WindowTitle);
}

#ifdef Q_WS_X11
bool UIMachineWindow::x11Event(XEvent *pEvent)
{
    // TODO: Is that really needed?
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
#endif /* Q_WS_X11 */

void UIMachineWindow::closeEvent(QCloseEvent *pEvent)
{
    /* Always ignore close-event: */
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
            CMachine m = machine();

            /* Check if there is a close hook script defined. */
            const QString& strScript = m.GetExtraData(GUI_CloseActionHook);
            if (!strScript.isEmpty())
            {
                QProcess::startDetached(strScript, QStringList() << m.GetId());
                return;
            }

            /* Prepare close-dialog: */
            UIVMCloseDialog *pDlg = new UIVMCloseDialog(this);

            /* Assign close-dialog pixmap: */
            pDlg->pmIcon->setPixmap(vboxGlobal().vmGuestOSTypeIcon(m.GetOSTypeId()));

            /* Check which close actions are disallowed: */
            QStringList restictedActionsList = m.GetExtraData(GUI_RestrictedCloseActions).split(',');
            bool fIsStateSavingAllowed = !restictedActionsList.contains("SaveState", Qt::CaseInsensitive);
            bool fIsACPIShutdownAllowed = !restictedActionsList.contains("Shutdown", Qt::CaseInsensitive);
            bool fIsPowerOffAllowed = !restictedActionsList.contains("PowerOff", Qt::CaseInsensitive);
            bool fIsPowerOffAndRestoreAllowed = fIsPowerOffAllowed && !restictedActionsList.contains("Restore", Qt::CaseInsensitive);

            /* Make Save State button visible/hidden depending on restriction: */
            pDlg->mRbSave->setVisible(fIsStateSavingAllowed);
            pDlg->mTxSave->setVisible(fIsStateSavingAllowed);
            /* Make Save State button enabled/disabled depending on machine state: */
            pDlg->mRbSave->setEnabled(uisession()->machineState() != KMachineState_Stuck);

            /* Make ACPI shutdown button visible/hidden depending on restriction: */
            pDlg->mRbShutdown->setVisible(fIsACPIShutdownAllowed);
            pDlg->mTxShutdown->setVisible(fIsACPIShutdownAllowed);
            /* Make ACPI shutdown button enabled/disabled depending on ACPI state & machine state: */
            bool isACPIEnabled = session().GetConsole().GetGuestEnteredACPIMode();
            pDlg->mRbShutdown->setEnabled(isACPIEnabled && uisession()->machineState() != KMachineState_Stuck);

            /* Make Power Off button visible/hidden depending on restriction: */
            pDlg->mRbPowerOff->setVisible(fIsPowerOffAllowed);
            pDlg->mTxPowerOff->setVisible(fIsPowerOffAllowed);

            /* Make the Restore Snapshot checkbox visible/hidden depending on snapshots count & restrictions: */
            pDlg->mCbDiscardCurState->setVisible(fIsPowerOffAndRestoreAllowed && m.GetSnapshotCount() > 0);
            if (!m.GetCurrentSnapshot().isNull())
                pDlg->mCbDiscardCurState->setText(pDlg->mCbDiscardCurState->text().arg(m.GetCurrentSnapshot().GetName()));

            /* Choice string tags for close-dialog: */
            QString strSave("save");
            QString strShutdown("shutdown");
            QString strPowerOff("powerOff");
            QString strDiscardCurState("discardCurState");

            /* Read the last user's choice for the given VM: */
            QStringList lastAction = m.GetExtraData(GUI_LastCloseAction).split(',');

            /* Check which button should be initially chosen: */
            QRadioButton *pRadioButton = 0;

            /* If choosing 'last choice' is possible: */
            if (lastAction[0] == strSave && fIsStateSavingAllowed)
            {
                pRadioButton = pDlg->mRbSave;
            }
            else if (lastAction[0] == strShutdown && fIsACPIShutdownAllowed && isACPIEnabled)
            {
                pRadioButton = pDlg->mRbShutdown;
            }
            else if (lastAction[0] == strPowerOff && fIsPowerOffAllowed)
            {
                pRadioButton = pDlg->mRbPowerOff;
                if (fIsPowerOffAndRestoreAllowed)
                    pDlg->mCbDiscardCurState->setChecked(lastAction.count() > 1 && lastAction[1] == strDiscardCurState);
            }
            /* Else 'default choice' will be used: */
            else
            {
                if (fIsACPIShutdownAllowed && isACPIEnabled)
                    pRadioButton = pDlg->mRbShutdown;
                else if (fIsPowerOffAllowed)
                    pRadioButton = pDlg->mRbPowerOff;
                else if (fIsStateSavingAllowed)
                    pRadioButton = pDlg->mRbSave;
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
                delete pDlg;
                pDlg = 0;
                break;
            }

            /* This flag will keep the status of every further logical operation: */
            bool fSuccess = true;

            /* This flag is set if we must terminate the VM, even if server calls fail */
            bool fForce = false;

            /* Pause before showing dialog if necessary: */
            bool fWasPaused = uisession()->isPaused() || uisession()->machineState() == KMachineState_Stuck;
            if (!fWasPaused)
                fSuccess = uisession()->pause();

            if (fSuccess)
            {
                /* Preventing auto-closure: */
                machineLogic()->setPreventAutoClose(true);

                /* Show the close-dialog: */
                bool fDialogAccepted = pDlg->exec() == QDialog::Accepted;

                /* What was the decision? */
                enum DialogDecision { DD_Cancel, DD_Save, DD_Shutdown, DD_PowerOff };
                DialogDecision decision;
                if (!fDialogAccepted)
                    decision = DD_Cancel;
                else if (pDlg->mRbSave->isChecked())
                    decision = DD_Save;
                else if (pDlg->mRbShutdown->isChecked())
                    decision = DD_Shutdown;
                else
                    decision = DD_PowerOff;
                bool fDiscardCurState = pDlg->mCbDiscardCurState->isChecked();
                bool fDiscardCheckboxVisible = pDlg->mCbDiscardCurState->isVisibleTo(pDlg);

                /* Destroy the dialog early: */
                delete pDlg;
                pDlg = 0;

                /* Was dialog accepted? */
                if (fDialogAccepted)
                {
                    /* Process decision: */
                    CConsole console = session().GetConsole();
                    fSuccess = false;
                    switch (decision)
                    {
                        case DD_Save:
                        {
                            /* Prepare the saving progress: */
                            CProgress progress = console.SaveState();
                            if (console.isOk())
                            {
                                /* Show the saving progress dialog: */
                                msgCenter().showModalProgressDialog(progress, m.GetName(), ":/progress_state_save_90px.png", 0, true);
                                if (progress.GetResultCode() == 0)
                                    fSuccess = true;
                                else
                                    msgCenter().cannotSaveMachineState(progress);
                            }
                            else
                                msgCenter().cannotSaveMachineState(console);
                            if (fSuccess)
                                fCloseApplication = true;
                            break;
                        }
                        case DD_Shutdown:
                        {
                            /* Unpause the VM to let it grab the ACPI shutdown event: */
                            uisession()->unpause();
                            /* Prevent the subsequent unpause request: */
                            fWasPaused = true;
                            /* Signal ACPI shutdown (if there is no ACPI device, the operation will fail): */
                            console.PowerButton();
                            if (console.isOk())
                                fSuccess = true;
                            else
                                msgCenter().cannotACPIShutdownMachine(console);
                            break;
                        }
                        case DD_PowerOff:
                        {
                            /* Prepare the power down progress: */
                            CProgress progress = console.PowerDown();
                            if (console.isOk())
                            {
                                /* Show the power down progress: */
                                msgCenter().showModalProgressDialog(progress, m.GetName(), ":/progress_poweroff_90px.png", 0, true);
                                if (progress.GetResultCode() == 0)
                                    fSuccess = true;
                                else
                                    msgCenter().cannotStopMachine(progress);
                            }
                            else
                            {
                                COMResult res(console);
                                /* This can happen if VBoxSVC is not running */
                                if (FAILED_DEAD_INTERFACE(res.rc()))
                                    fForce = true;
                                else
                                    msgCenter().cannotStopMachine(console);
                            }
                            if (fSuccess)
                            {
                                /* Discard the current state if requested: */
                                if (fDiscardCurState && fDiscardCheckboxVisible)
                                {
                                    /* Prepare the snapshot discard progress: */
                                    CSnapshot snapshot = m.GetCurrentSnapshot();
                                    CProgress progress = console.RestoreSnapshot(snapshot);
                                    if (console.isOk())
                                    {
                                        /* Show the snapshot discard progress: */
                                        msgCenter().showModalProgressDialog(progress, m.GetName(), ":/progress_snapshot_discard_90px.png", 0, true);
                                        if (progress.GetResultCode() != 0)
                                            msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName());
                                    }
                                    else
                                        msgCenter().cannotRestoreSnapshot(console, snapshot.GetName());
                                }
                            }
                            if (fSuccess || fForce)
                                fCloseApplication = true;
                            break;
                        }
                        default:
                            break;
                    }

                    if (fSuccess)
                    {
                        /* Read the last user's choice for the given VM: */
                        QStringList prevAction = m.GetExtraData(GUI_LastCloseAction).split(',');
                        /* Memorize the last user's choice for the given VM: */
                        QString lastAction = strPowerOff;
                        switch (decision)
                        {
                            case DD_Save: lastAction = strSave; break;
                            case DD_Shutdown: lastAction = strShutdown; break;
                            case DD_PowerOff:
                            {
                                if (prevAction[0] == strShutdown && !isACPIEnabled)
                                    lastAction = strShutdown;
                                else
                                    lastAction = strPowerOff;
                                break;
                            }
                            default: break;
                        }
                        /* Memorize additional options for the given VM: */
                        if (fDiscardCurState)
                            (lastAction += ",") += strDiscardCurState;
                        m.SetExtraData(GUI_LastCloseAction, lastAction);
                    }
                }

                /* Restore the running state if needed: */
                if (fSuccess && !fCloseApplication && !fWasPaused && uisession()->machineState() == KMachineState_Paused)
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
        /* VM has been powered off or saved. We must *safely* close VM window(s): */
        QTimer::singleShot(0, uisession(), SLOT(sltCloseVirtualSession()));
    }
}

void UIMachineWindow::prepareSessionConnections()
{
    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));

    /* Guest monitor-change updater: */
    connect(uisession(), SIGNAL(sigGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)),
            this, SLOT(sltGuestMonitorChange(KGuestMonitorChangedEventType, ulong, QRect)));
}

void UIMachineWindow::prepareMainLayout()
{
    /* Create central-widget: */
    setCentralWidget(new QWidget);

    /* Create main-layout: */
    m_pMainLayout = new QGridLayout(centralWidget());
    m_pMainLayout->setMargin(0);
    m_pMainLayout->setSpacing(0);

    /* Create shifting-spacers: */
    m_pTopSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_pBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_pLeftSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pRightSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);

    /* Add shifting-spacers into main-layout: */
    m_pMainLayout->addItem(m_pTopSpacer, 0, 1);
    m_pMainLayout->addItem(m_pBottomSpacer, 2, 1);
    m_pMainLayout->addItem(m_pLeftSpacer, 1, 0);
    m_pMainLayout->addItem(m_pRightSpacer, 1, 2);
}

void UIMachineWindow::prepareMachineView()
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = machine().GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /* Get visual-state type: */
    UIVisualStateType visualStateType = machineLogic()->visualStateType();

    /* Create machine-view: */
    m_pMachineView = UIMachineView::create(  this
                                           , m_uScreenId
                                           , visualStateType
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif /* VBOX_WITH_VIDEOHWACCEL */
                                           );

    /* Add machine-view into main-layout: */
    m_pMainLayout->addWidget(m_pMachineView, 1, 1, viewAlignment(visualStateType));
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

void UIMachineWindow::cleanupMachineView()
{
    /* Destroy machine-view: */
    UIMachineView::destroy(m_pMachineView);
    m_pMachineView = 0;
}

void UIMachineWindow::updateAppearanceOf(int iElement)
{
    /* Update window title: */
    if (iElement & UIVisualElement_WindowTitle)
    {
        /* Get machine: */
        const CMachine &m = machine();
        /* Get machine state: */
        KMachineState state = uisession()->machineState();
        /* Prepare full name: */
        QString strSnapshotName;
        if (m.GetSnapshotCount() > 0)
        {
            CSnapshot snapshot = m.GetCurrentSnapshot();
            strSnapshotName = " (" + snapshot.GetName() + ")";
        }
        QString strMachineName = m.GetName() + strSnapshotName;
        if (state != KMachineState_Null)
            strMachineName += " [" + gpConverter->toString(state) + "]";
        /* Unusual on the Mac. */
#ifndef Q_WS_MAC
        strMachineName += " - " + defaultWindowTitle();
#endif /* !Q_WS_MAC */
        if (m.GetMonitorCount() > 1)
            strMachineName += QString(" : %1").arg(m_uScreenId + 1);
        setWindowTitle(strMachineName);
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

/* static */
Qt::WindowFlags UIMachineWindow::windowFlags(UIVisualStateType visualStateType)
{
    switch (visualStateType)
    {
        case UIVisualStateType_Normal: return Qt::Window;
        case UIVisualStateType_Fullscreen: return Qt::FramelessWindowHint;
        case UIVisualStateType_Seamless: return Qt::FramelessWindowHint;
        case UIVisualStateType_Scale: return Qt::Window;
    }
    AssertMsgFailed(("Incorrect visual state!"));
    return 0;
}

/* static */
Qt::Alignment UIMachineWindow::viewAlignment(UIVisualStateType visualStateType)
{
    switch (visualStateType)
    {
        case UIVisualStateType_Normal: return 0;
        case UIVisualStateType_Fullscreen: return Qt::AlignVCenter | Qt::AlignHCenter;
        case UIVisualStateType_Seamless: return 0;
        case UIVisualStateType_Scale: return 0;
    }
    AssertMsgFailed(("Incorrect visual state!"));
    return 0;
}

