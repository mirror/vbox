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
#include "UIExtraDataManager.h"

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
    : QIWithRetranslateUI2<QMainWindow>(0, pMachineLogic->windowFlags(uScreenId))
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
    /* On Mac OS X window icon referenced in info.plist is used. */

    /* Set default window icon (will be changed to VM-specific icon little bit later): */
    setWindowIcon(QIcon(":/VirtualBox_48px.png"));

    /* Set redefined machine-window icon if any: */
    QIcon *pMachineWidnowIcon = uisession()->machineWindowIcon();
    if (pMachineWidnowIcon)
        setWindowIcon(*pMachineWidnowIcon);
    /* Or set default machine-window icon: */
    else
        setWindowIcon(vboxGlobal().vmGuestOSTypeIcon(machine().GetOSTypeId()));
#endif /* !Q_WS_MAC */
}

UIActionPool* UIMachineWindow::actionPool() const
{
    return machineLogic()->actionPool();
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

void UIMachineWindow::adjustMachineViewSize()
{
    /* By default, the only thing we need is to
     * adjust guest-screen size if necessary: */
    machineView()->adjustGuestScreenSize();
}

#ifndef VBOX_WITH_TRANSLUCENT_SEAMLESS
void UIMachineWindow::setMask(const QRegion &region)
{
    /* Call to base-class: */
    QMainWindow::setMask(region);
}
#endif /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */

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
    /* Always ignore close-event first: */
    pEvent->ignore();

    /* Make sure machine is in one of the allowed states: */
    if (!uisession()->isRunning() && !uisession()->isPaused() && !uisession()->isStuck())
        return;

    /* Get machine: */
    CMachine m = machine();

    /* If there is a close hook script defined: */
    const QString strScript = gEDataManager->machineCloseHookScript(vboxGlobal().managedVMUuid());
    if (!strScript.isEmpty())
    {
        /* Execute asynchronously and leave: */
        QProcess::startDetached(strScript, QStringList() << m.GetId());
        return;
    }

    /* Choose the close action: */
    MachineCloseAction closeAction = MachineCloseAction_Invalid;

    /* If default close-action defined and not restricted: */
    MachineCloseAction defaultCloseAction = uisession()->defaultCloseAction();
    MachineCloseAction restrictedCloseActions = uisession()->restrictedCloseActions();
    if ((defaultCloseAction != MachineCloseAction_Invalid) &&
        !(restrictedCloseActions & defaultCloseAction))
    {
        switch (defaultCloseAction)
        {
            /* If VM is stuck, and the default close-action is 'save-state' or 'shutdown',
             * we should ask the user about what to do: */
            case MachineCloseAction_SaveState:
            case MachineCloseAction_Shutdown:
                closeAction = uisession()->isStuck() ? MachineCloseAction_Invalid : defaultCloseAction;
                break;
            /* Otherwise we just use what we have: */
            default:
                closeAction = defaultCloseAction;
                break;
        }
    }

    /* If the close-action still undefined: */
    if (closeAction == MachineCloseAction_Invalid)
    {
        /* Prepare close-dialog: */
        QWidget *pParentDlg = windowManager().realParentWindow(this);
        QPointer<UIVMCloseDialog> pCloseDlg = new UIVMCloseDialog(pParentDlg, m,
                                                                  session().GetConsole().GetGuestEnteredACPIMode(),
                                                                  restrictedCloseActions);

        /* Make sure close-dialog is valid: */
        if (pCloseDlg->isValid())
        {
            /* If VM is not paused and not stuck, we should pause it first: */
            bool fWasPaused = uisession()->isPaused() || uisession()->isStuck();
            if (fWasPaused || uisession()->pause())
            {
                /* Show close-dialog to let the user make the choice: */
                windowManager().registerNewParent(pCloseDlg, pParentDlg);
                closeAction = static_cast<MachineCloseAction>(pCloseDlg->exec());

                /* Make sure the dialog still valid: */
                if (!pCloseDlg)
                    return;

                /* If VM was not paused before but paused now,
                 * we should resume it if user canceled dialog or chosen shutdown: */
                if (!fWasPaused && uisession()->isPaused() &&
                    (closeAction == MachineCloseAction_Invalid ||
                     closeAction == MachineCloseAction_Shutdown))
                {
                    /* If we unable to resume VM, cancel closing: */
                    if (!uisession()->unpause())
                        closeAction = MachineCloseAction_Invalid;
                }
            }
        }
        else
        {
            /* Else user misconfigured .vbox file, we will reject closing UI: */
            closeAction = MachineCloseAction_Invalid;
        }

        /* Cleanup close-dialog: */
        delete pCloseDlg;
    }

    /* Depending on chosen result: */
    switch (closeAction)
    {
        case MachineCloseAction_SaveState:
        {
            /* Save VM state: */
            machineLogic()->saveState();
            break;
        }
        case MachineCloseAction_Shutdown:
        {
            /* Shutdown VM: */
            machineLogic()->shutdown();
            break;
        }
        case MachineCloseAction_PowerOff:
        case MachineCloseAction_PowerOff_RestoringSnapshot:
        {
            /* Power VM off: */
            machineLogic()->powerOff(closeAction == MachineCloseAction_PowerOff_RestoringSnapshot);
            break;
        }
        default:
            break;
    }
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

    /* Listen for frame-buffer resize: */
    connect(m_pMachineView, SIGNAL(sigFrameBufferResize()), this, SIGNAL(sigFrameBufferResize()));

    /* Add machine-view into main-layout: */
    m_pMainLayout->addWidget(m_pMachineView, 1, 1, viewAlignment(visualStateType));

    /* Install focus-proxy: */
    setFocusProxy(m_pMachineView);
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
        const QString strUserProductName = uisession()->machineWindowNamePostfix();
        strMachineName += " - " + (strUserProductName.isEmpty() ? defaultWindowTitle() : strUserProductName);
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

#ifdef Q_WS_MAC
void UIMachineWindow::handleNativeNotification(const QString &strNativeNotificationName, QWidget *pWidget)
{
    /* Handle arrived notification: */
    LogRel(("UIMachineWindow::handleNativeNotification: Notification '%s' received.\n",
            strNativeNotificationName.toAscii().constData()));
    if (UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(pWidget))
    {
        /* Redirect arrived notification: */
        LogRel(("UIMachineWindow::handleNativeNotification: Redirecting '%s' notification to corresponding machine-window...\n",
                strNativeNotificationName.toAscii().constData()));
        pMachineWindow->handleNativeNotification(strNativeNotificationName);
    }
}
#endif /* Q_WS_MAC */

