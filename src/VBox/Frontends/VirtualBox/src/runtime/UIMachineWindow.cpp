/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindow class implementation
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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
#include "UIModalWindowManager.h"

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

void UIMachineWindow::setMask(const QRegion &region)
{
    /* Call to base-class: */
    QMainWindow::setMask(region);
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

    /* Should we close Runtime UI? */
    bool fCloseRuntimeUI = false;

    /* Make sure machine is in one of the allowed states: */
    KMachineState machineState = uisession()->machineState();
    if (   machineState != KMachineState_Running
        && machineState != KMachineState_Paused
        && machineState != KMachineState_Stuck
        && machineState != KMachineState_LiveSnapshotting
        && machineState != KMachineState_Teleporting
        && machineState != KMachineState_TeleportingPausedVM)
        return;

    /* Get the machine: */
    CMachine machineCopy = machine();

    /* If there is a close hook script defined: */
    const QString& strScript = machineCopy.GetExtraData(GUI_CloseActionHook);
    if (!strScript.isEmpty())
    {
        /* Execute asynchronously and leave: */
        QProcess::startDetached(strScript, QStringList() << machineCopy.GetId());
        return;
    }

    /* Prepare close-dialog: */
    QWidget *pParentDlg = mwManager().realParentWindow(this);
    QPointer<UIVMCloseDialog> pCloseDlg = new UIVMCloseDialog(pParentDlg, machineCopy, session());
    mwManager().registerNewParent(pCloseDlg, pParentDlg);

    /* Makes sure the dialog is valid: */
    if (!pCloseDlg->isValid())
    {
        /* Destroy and leave: */
        delete pCloseDlg;
        return;
    }

    /* This flag will keep the status of every further logical operation: */
    bool fSuccess = true;
    /* This flag determines if VM was paused before we called for close-event: */
    bool fWasPaused = uisession()->isPaused();

    if (fSuccess)
    {
        /* Pause VM if necessary: */
        if (!fWasPaused)
            fSuccess = uisession()->pause();
    }

    if (fSuccess)
    {
        /* Preventing auto-closure: */
        machineLogic()->setPreventAutoClose(true);

        /* Show the close-dialog: */
        UIVMCloseDialog::ResultCode dialogResult = (UIVMCloseDialog::ResultCode)pCloseDlg->exec();

        /* Destroy the dialog early: */
        delete pCloseDlg;

        /* Was the dialog accepted? */
        if (dialogResult != UIVMCloseDialog::ResultCode_Cancel)
        {
            switch (dialogResult)
            {
                case UIVMCloseDialog::ResultCode_Save:
                {
                    fSuccess = uisession()->saveState();
                    fCloseRuntimeUI = fSuccess;
                    break;
                }
                case UIVMCloseDialog::ResultCode_Shutdown:
                {
                    fSuccess = uisession()->shutDown();
                    if (fSuccess)
                        fWasPaused = true;
                    break;
                }
                case UIVMCloseDialog::ResultCode_PowerOff:
                case UIVMCloseDialog::ResultCode_PowerOff_With_Discarding:
                {
                    bool fServerCrashed = false;
                    fSuccess = uisession()->powerOff(dialogResult == UIVMCloseDialog::ResultCode_PowerOff_With_Discarding,
                                                     fServerCrashed);
                    fCloseRuntimeUI = fSuccess || fServerCrashed;
                    break;
                }
                default:
                    break;
            }
        }

        /* Restore the running state if needed: */
        if (fSuccess && !fCloseRuntimeUI && !fWasPaused && uisession()->machineState() == KMachineState_Paused)
            uisession()->unpause();

        /* Allowing auto-closure: */
        machineLogic()->setPreventAutoClose(false);
    }

    /* We've received a request to close Runtime UI: */
    if (fCloseRuntimeUI)
        uisession()->closeRuntimeUI();
}

void UIMachineWindow::prepareSessionConnections()
{
    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));
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

void UIMachineWindow::handleScreenCountChange()
{
    showInNecessaryMode();
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

