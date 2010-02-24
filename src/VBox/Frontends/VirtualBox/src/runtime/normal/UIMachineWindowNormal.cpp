/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class implementation
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
#include <QDesktopWidget>
#include <QMenuBar>
#include <QTimer>
#include <QContextMenuEvent>

/* Local includes */
#include "VBoxGlobal.h"

#include "VBoxVMInformationDlg.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIIndicatorsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowNormal.h"

#include "QIStatusBar.h"
#include "QIStateIndicator.h"
#include "QIHotKeyEdit.h"

UIMachineWindowNormal::UIMachineWindowNormal(UIMachineLogic *pMachineLogic)
    : QIWithRetranslateUI<QIMainDialog>(0)
    , UIMachineWindow(pMachineLogic)
    , m_pIndicatorsPool(new UIIndicatorsPool(pMachineLogic->uisession()->session(), this))
    , m_pIdleTimer(0)
{
    /* "This" is machine window: */
    m_pMachineWindow = this;

    /* Prepare window icon: */
    prepareWindowIcon();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Prepare menu: */
    prepareMenu();

    /* Prepare status bar: */
    prepareStatusBar();

    /* Prepare connections: */
    prepareConnections();

    /* Prepare normal machine view: */
    prepareMachineView();

    /* Load normal window settings: */
    loadWindowSettings();

    /* Retranslate normal window finally: */
    retranslateUi();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

    /* Show window: */
    show();
}

UIMachineWindowNormal::~UIMachineWindowNormal()
{
    /* Save normal window settings: */
    saveWindowSettings();

    /* Cleanup status-bar: */
    cleanupStatusBar();
}

void UIMachineWindowNormal::sltTryClose()
{
    UIMachineWindow::sltTryClose();
}

void UIMachineWindowNormal::sltMachineStateChanged(KMachineState machineState)
{
    UIMachineWindow::sltMachineStateChanged(machineState);
}

void UIMachineWindowNormal::sltMediumChange(const CMediumAttachment &attachment)
{
    KDeviceType type = attachment.GetType();
    Assert(type == KDeviceType_DVD || type == KDeviceType_Floppy);
    updateAppearanceOf(type == KDeviceType_DVD ? UIVisualElement_CDStuff :
                       type == KDeviceType_Floppy ? UIVisualElement_FDStuff :
                       UIVisualElement_AllStuff);
}

void UIMachineWindowNormal::sltUSBControllerChange()
{
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowNormal::sltUSBDeviceStateChange()
{
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowNormal::sltNetworkAdapterChange()
{
    updateAppearanceOf(UIVisualElement_NetworkStuff);
}

void UIMachineWindowNormal::sltSharedFolderChange()
{
    updateAppearanceOf(UIVisualElement_SharedFolderStuff);
}

void UIMachineWindowNormal::sltPrepareMenuMachine()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu == machineLogic()->actionsPool()->action(UIActionIndex_Menu_Machine)->menu(),
              ("This slot should only be called on 'Machine' menu hovering!\n"));

    menu->clear();

    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_Fullscreen));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_Seamless));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_AdjustWindow));
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_MouseIntegration));
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_TypeCAD));
#ifdef Q_WS_X11
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_TypeCABS));
#endif
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_TakeSnapshot));
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_InformationDialog));
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_Pause));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_Reset));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_Shutdown));
#ifndef Q_WS_MAC
    menu->addSeparator();
#endif /* Q_WS_MAC */
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_Close));
}

void UIMachineWindowNormal::sltPrepareMenuDevices()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu == machineLogic()->actionsPool()->action(UIActionIndex_Menu_Devices)->menu(),
              ("This slot should only be called on 'Devices' menu hovering!\n"));

    menu->clear();

    /* Devices submenu */
    menu->addMenu(machineLogic()->actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->menu());
    menu->addMenu(machineLogic()->actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->menu());
    menu->addMenu(machineLogic()->actionsPool()->action(UIActionIndex_Menu_USBDevices)->menu());
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_NetworkAdaptersDialog));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_SharedFoldersDialog));
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_VRDP));
    menu->addSeparator();
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_InstallGuestTools));
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineWindowNormal::sltPrepareMenuDebug()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu == machineLogic()->actionsPool()->action(UIActionIndex_Menu_Debug)->menu(),
              ("This slot should only be called on 'Debug' menu hovering!\n"));

    menu->clear();

    /* Debug submenu */
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_Statistics));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Simple_CommandLine));
    menu->addAction(machineLogic()->actionsPool()->action(UIActionIndex_Toggle_Logging));
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineWindowNormal::sltUpdateIndicators()
{
    CConsole console = session().GetConsole();
    QIStateIndicator *pStateIndicator = 0;

    pStateIndicator = indicatorsPool()->indicator(UIIndicatorIndex_HardDisks);
    if (pStateIndicator->state() != KDeviceActivity_Null)
    {
        int state = console.GetDeviceActivity(KDeviceType_HardDisk);
        if (pStateIndicator->state() != state)
            pStateIndicator->setState(state);
    }
    pStateIndicator = indicatorsPool()->indicator(UIIndicatorIndex_OpticalDisks);
    if (pStateIndicator->state() != KDeviceActivity_Null)
    {
        int state = console.GetDeviceActivity(KDeviceType_DVD);
        if (pStateIndicator->state() != state)
            pStateIndicator->setState(state);
    }
    pStateIndicator = indicatorsPool()->indicator(UIIndicatorIndex_USBDevices);
    if (pStateIndicator->state() != KDeviceActivity_Null)
    {
        int state = console.GetDeviceActivity(KDeviceType_USB);
        if (pStateIndicator->state() != state)
            pStateIndicator->setState(state);
    }
    pStateIndicator = indicatorsPool()->indicator(UIIndicatorIndex_NetworkAdapters);
    if (pStateIndicator->state() != KDeviceActivity_Null)
    {
        int state = console.GetDeviceActivity(KDeviceType_Network);
        if (pStateIndicator->state() != state)
            pStateIndicator->setState(state);
    }
    pStateIndicator = indicatorsPool()->indicator(UIIndicatorIndex_SharedFolders);
    if (pStateIndicator->state() != KDeviceActivity_Null)
    {
        int state = console.GetDeviceActivity(KDeviceType_SharedFolder);
        if (pStateIndicator->state() != state)
            pStateIndicator->setState(state);
    }
}

void UIMachineWindowNormal::sltShowIndicatorsContextMenu(QIStateIndicator *pIndicator, QContextMenuEvent *pEvent)
{
    if (pIndicator == indicatorsPool()->indicator(UIIndicatorIndex_OpticalDisks))
    {
        if (machineLogic()->actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->isEnabled())
            machineLogic()->actionsPool()->action(UIActionIndex_Menu_OpticalDevices)->menu()->exec(pEvent->globalPos());
    }
    else if (pIndicator == indicatorsPool()->indicator(UIIndicatorIndex_USBDevices))
    {
        if (machineLogic()->actionsPool()->action(UIActionIndex_Menu_USBDevices)->isEnabled())
            machineLogic()->actionsPool()->action(UIActionIndex_Menu_USBDevices)->menu()->exec(pEvent->globalPos());
    }
    else if (pIndicator == indicatorsPool()->indicator(UIIndicatorIndex_NetworkAdapters))
    {
        if (machineLogic()->actionsPool()->action(UIActionIndex_Menu_NetworkAdapters)->isEnabled())
            machineLogic()->actionsPool()->action(UIActionIndex_Menu_NetworkAdapters)->menu()->exec(pEvent->globalPos());
    }
    else if (pIndicator == indicatorsPool()->indicator(UIIndicatorIndex_SharedFolders))
    {
        if (machineLogic()->actionsPool()->action(UIActionIndex_Menu_SharedFolders)->isEnabled())
            machineLogic()->actionsPool()->action(UIActionIndex_Menu_SharedFolders)->menu()->exec(pEvent->globalPos());
    }
    else if (pIndicator == indicatorsPool()->indicator(UIIndicatorIndex_Mouse))
    {
        if (machineLogic()->actionsPool()->action(UIActionIndex_Menu_MouseIntegration)->isEnabled())
            machineLogic()->actionsPool()->action(UIActionIndex_Menu_MouseIntegration)->menu()->exec(pEvent->globalPos());
    }
}

void UIMachineWindowNormal::sltProcessGlobalSettingChange(const char * /* aPublicName */, const char * /* aName */)
{
    m_pNameHostkey->setText(QIHotKeyEdit::keyName(vboxGlobal().settings().hostKey()));
}

void UIMachineWindowNormal::sltUpdateMouseState(int iState)
{
    if ((iState & UIMouseStateType_MouseAbsoluteDisabled) &&
        (iState & UIMouseStateType_MouseAbsolute) &&
        !(iState & UIMouseStateType_MouseCaptured))
    {
        indicatorsPool()->indicator(UIIndicatorIndex_Mouse)->setState(4);
    }
    else
    {
        indicatorsPool()->indicator(UIIndicatorIndex_Mouse)->setState(
            iState & (UIMouseStateType_MouseAbsolute | UIMouseStateType_MouseCaptured));
    }
}

void UIMachineWindowNormal::retranslateUi()
{
    /* Translate parent class: */
    UIMachineWindow::retranslateUi();

#ifdef Q_WS_MAC
    // TODO_NEW_CORE
//    m_pDockSettingsMenu->setTitle(tr("Dock Icon"));
//    m_pDockDisablePreview->setText(tr("Show Application Icon"));
//    m_pDockEnablePreviewMonitor->setText(tr("Show Monitor Preview"));
#endif /* Q_WS_MAC */

    m_pNameHostkey->setToolTip(
        tr("Shows the currently assigned Host key.<br>"
           "This key, when pressed alone, toggles the keyboard and mouse "
           "capture state. It can also be used in combination with other keys "
           "to quickly perform actions from the main menu."));
    m_pNameHostkey->setText(QIHotKeyEdit::keyName(vboxGlobal().settings().hostKey()));
}

void UIMachineWindowNormal::updateAppearanceOf(int iElement)
{
    /* Update parent-class window: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update that machine window: */
    if (iElement & UIVisualElement_HDStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_HardDisks)->updateAppearance();
    if (iElement & UIVisualElement_CDStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_OpticalDisks)->updateAppearance();
#if 0 /* TODO: Allow to setup status-bar! */
    if (iElement & UIVisualElement_FDStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_FloppyDisks)->updateAppearance();
#endif
    if (    iElement & UIVisualElement_USBStuff
        && !indicatorsPool()->indicator(UIIndicatorIndex_USBDevices)->isHidden())
            indicatorsPool()->indicator(UIIndicatorIndex_USBDevices)->updateAppearance();
    if (iElement & UIVisualElement_NetworkStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_NetworkAdapters)->updateAppearance();
    if (iElement & UIVisualElement_SharedFolderStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_SharedFolders)->updateAppearance();
#if 0 /* TODO: Allow to setup status-bar! */
    if (iElement & UIVisualElement_VRDPStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_VRDP)->updateAppearance();
#endif
    if (iElement & UIVisualElement_VirtualizationStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_Virtualization)->updateAppearance();
}

bool UIMachineWindowNormal::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = (QResizeEvent*)pEvent;

            if (!isMaximized())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                // TODO: Update debugger window size!
                //dbgAdjustRelativePos();
#endif
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximized())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                // TODO: Update debugger window position!
                //dbgAdjustRelativePos();
#endif
            }
            break;
        }
        default:
            break;
    }
    return QIWithRetranslateUI<QIMainDialog>::event(pEvent);
}

#ifdef Q_WS_X11
bool UIMachineWindowNormal::x11Event(XEvent *pEvent)
{
    /* Qt bug: when the console view grabs the keyboard, FocusIn, FocusOut,
     * WindowActivate and WindowDeactivate Qt events are not properly sent
     * on top level window (i.e. this) deactivation. The fix is to substiute
     * the mode in FocusOut X11 event structure to NotifyNormal to cause
     * Qt to process it as desired. */
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

void UIMachineWindowNormal::closeEvent(QCloseEvent *pEvent)
{
    return UIMachineWindow::closeEvent(pEvent);
}

void UIMachineWindowNormal::prepareConsoleConnections()
{
    /* Parent class connections: */
    UIMachineWindow::prepareConsoleConnections();
    /* Other console connections: */
    connect(machineLogic()->uisession(), SIGNAL(sigMediumChange(const CMediumAttachment &)),
            this, SLOT(sltMediumChange(const CMediumAttachment &)));
    connect(machineLogic()->uisession(), SIGNAL(sigUSBControllerChange()),
            this, SLOT(sltUSBControllerChange()));
    connect(machineLogic()->uisession(), SIGNAL(sigUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)),
            this, SLOT(sltUSBDeviceStateChange()));
    connect(machineLogic()->uisession(), SIGNAL(sigNetworkAdapterChange(const CNetworkAdapter &)),
            this, SLOT(sltNetworkAdapterChange()));
    connect(machineLogic()->uisession(), SIGNAL(sigSharedFolderChange()),
            this, SLOT(sltSharedFolderChange()));
}

void UIMachineWindowNormal::prepareMenu()
{
    /* Machine submenu: */
    QMenu *pMenuMachine = machineLogic()->actionsPool()->action(UIActionIndex_Menu_Machine)->menu();
    menuBar()->addMenu(pMenuMachine);
    connect(pMenuMachine, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuMachine()));

    /* Devices submenu: */
    QMenu *pMenuDevices = machineLogic()->actionsPool()->action(UIActionIndex_Menu_Devices)->menu();
    menuBar()->addMenu(pMenuDevices);
    connect(pMenuDevices, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuDevices()));

#ifdef VBOX_WITH_DEBUGGER_GUI
    if (vboxGlobal().isDebuggerEnabled())
    {
        QMenu *pMenuDebug = machineLogic()->actionsPool()->action(UIActionIndex_Menu_Debug)->menu();
        menuBar()->addMenu(pMenuDebug);
        connect(pMenuDebug, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuDebug()));
    }
#endif
}

void UIMachineWindowNormal::prepareStatusBar()
{
    /* Common setup: */
    setStatusBar(new QIStatusBar(this));
    QWidget *pIndicatorBox = new QWidget;
    QHBoxLayout *pIndicatorBoxHLayout = new QHBoxLayout(pIndicatorBox);
    VBoxGlobal::setLayoutMargin(pIndicatorBoxHLayout, 0);
    pIndicatorBoxHLayout->setSpacing(5);

    /* Hard Disks: */
    pIndicatorBoxHLayout->addWidget(indicatorsPool()->indicator(UIIndicatorIndex_HardDisks));

    /* Optical Disks: */
    QIStateIndicator *pLedOpticalDisks = indicatorsPool()->indicator(UIIndicatorIndex_OpticalDisks);
    pIndicatorBoxHLayout->addWidget(pLedOpticalDisks);
    connect(pLedOpticalDisks, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
            this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));

    /* USB Devices: */
    QIStateIndicator *pLedUSBDevices = indicatorsPool()->indicator(UIIndicatorIndex_USBDevices);
    pIndicatorBoxHLayout->addWidget(pLedUSBDevices);
    connect(pLedUSBDevices, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
            this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));

    /* Network Adapters: */
    QIStateIndicator *pLedNetworkAdapters = indicatorsPool()->indicator(UIIndicatorIndex_NetworkAdapters);
    pIndicatorBoxHLayout->addWidget(pLedNetworkAdapters);
    connect(pLedNetworkAdapters, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
            this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));

    /* Shared Folders: */
    QIStateIndicator *pLedSharedFolders = indicatorsPool()->indicator(UIIndicatorIndex_SharedFolders);
    pIndicatorBoxHLayout->addWidget(pLedSharedFolders);
    connect(pLedSharedFolders, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
            this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));

    /* Virtualization: */
    pIndicatorBoxHLayout->addWidget(indicatorsPool()->indicator(UIIndicatorIndex_Virtualization));

    /* Separator: */
    QFrame *pSeparator = new QFrame;
    pSeparator->setFrameStyle(QFrame::VLine | QFrame::Sunken);
    pIndicatorBoxHLayout->addWidget(pSeparator);

    /* Mouse: */
    QIStateIndicator *pLedMouse = indicatorsPool()->indicator(UIIndicatorIndex_Mouse);
    pIndicatorBoxHLayout->addWidget(pLedMouse);
    connect(pLedMouse, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
            this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));

    /* Host Key: */
    m_pCntHostkey = new QWidget;
    QHBoxLayout *pHostkeyLedContainerLayout = new QHBoxLayout(m_pCntHostkey);
    VBoxGlobal::setLayoutMargin(pHostkeyLedContainerLayout, 0);
    pHostkeyLedContainerLayout->setSpacing(3);
    pIndicatorBoxHLayout->addWidget(m_pCntHostkey);
    pHostkeyLedContainerLayout->addWidget(indicatorsPool()->indicator(UIIndicatorIndex_Hostkey));
    m_pNameHostkey = new QLabel(QIHotKeyEdit::keyName(vboxGlobal().settings().hostKey()));
    pHostkeyLedContainerLayout->addWidget(m_pNameHostkey);

    /* Add to statusbar: */
    statusBar()->addPermanentWidget(pIndicatorBox, 0);

    /* Create & start timer to update LEDs: */
    m_pIdleTimer = new QTimer(this);
    connect(m_pIdleTimer, SIGNAL(timeout()), this, SLOT(sltUpdateIndicators()));
    m_pIdleTimer->start(50);

#ifdef Q_WS_MAC
    /* For the status bar on Cocoa: */
    setUnifiedTitleAndToolBarOnMac(true);
#endif
}

void UIMachineWindowNormal::prepareConnections()
{
    /* Setup global settings <-> indicators connections: */
    connect(&vboxGlobal().settings(), SIGNAL(propertyChanged(const char *, const char *)),
            this, SLOT(sltProcessGlobalSettingChange(const char *, const char *)));
}

void UIMachineWindowNormal::prepareMachineView()
{
    CMachine machine = session().GetMachine();

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = machine.GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    m_pMachineView = UIMachineView::create(  this
                                           , vboxGlobal().vmRenderMode()
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                           , machineLogic()->visualStateType());

    //qobject_cast<QGridLayout*>(centralWidget()->layout())->addWidget(m_pMachineView, 1, 1, Qt::AlignVCenter | Qt::AlignHCenter);
    setCentralWidget(m_pMachineView);

    /* Setup machine view <-> indicators connections: */
    connect(machineView(), SIGNAL(keyboardStateChanged(int)), indicatorsPool()->indicator(UIIndicatorIndex_Hostkey), SLOT(setState(int)));
    connect(machineView(), SIGNAL(mouseStateChanged(int)), this, SLOT(sltUpdateMouseState(int)));
}

void UIMachineWindowNormal::loadWindowSettings()
{
    /* Load parent class settings: */
    UIMachineWindow::loadWindowSettings();

    /* Load this class settings: */
    CMachine machine = session().GetMachine();

    /* Extra-data settings */
    {
        QString strPositionSettings = machine.GetExtraData(VBoxDefs::GUI_LastWindowPosition);

        bool ok = false, max = false;
        int x = 0, y = 0, w = 0, h = 0;
        x = strPositionSettings.section(',', 0, 0).toInt(&ok);
        if (ok)
            y = strPositionSettings.section(',', 1, 1).toInt(&ok);
        if (ok)
            w = strPositionSettings.section(',', 2, 2).toInt(&ok);
        if (ok)
            h = strPositionSettings.section(',', 3, 3).toInt(&ok);
        if (ok)
            max = strPositionSettings.section(',', 4, 4) == VBoxDefs::GUI_LastWindowPosition_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(machineWindow());

        if (ok /* if previous parameters were read correctly */)
        {
            m_normalGeometry = QRect(x, y, w, h);
            setGeometry(m_normalGeometry);

            /* Normalize view to the optimal size */
            if (machineView())
                machineView()->normalizeGeometry(true /* adjust position? */);

            /* Maximize if needed */
            if (max)
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        else
        {
            /* Normalize to the optimal size */
            if (machineView())
                machineView()->normalizeGeometry(true /* adjust position? */);

            /* Move newly created window to the screen center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(ar.center());
            setGeometry(m_normalGeometry);
        }
    }

    /* Availability settings */
    {
        /* USB Stuff: */
        CUSBController usbController = machine.GetUSBController();
        if (usbController.isNull())
        {
            /* Hide USB Menu: */
            indicatorsPool()->indicator(UIIndicatorIndex_USBDevices)->setHidden(true);
        }
        else
        {
            /* Toggle USB LED: */
            indicatorsPool()->indicator(UIIndicatorIndex_USBDevices)->setState(
                usbController.GetEnabled() ? KDeviceActivity_Idle : KDeviceActivity_Null);
        }
    }

    /* Global settings */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
        statusBar()->setHidden(settings.isFeatureActive("noStatusBar"));
    }
}

void UIMachineWindowNormal::saveWindowSettings()
{
    CMachine machine = session().GetMachine();

    /* Extra-data settings */
    {
        QString strWindowPosition = QString("%1,%2,%3,%4")
                                    .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                                    .arg(m_normalGeometry.width()).arg(m_normalGeometry.height());
        if (isMaximized())
            strWindowPosition += QString(",%1").arg(VBoxDefs::GUI_LastWindowPosition_Max);
        machine.SetExtraData(VBoxDefs::GUI_LastWindowPosition, strWindowPosition);
    }
}

void UIMachineWindowNormal::cleanupStatusBar()
{
    /* Stop LED-update timer: */
    m_pIdleTimer->stop();
    m_pIdleTimer->disconnect(SIGNAL(timeout()), this, SLOT(sltUpdateIndicators()));
}

