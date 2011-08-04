/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class implementation
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
#include <QDesktopWidget>
#include <QMenuBar>
#include <QTimer>
#include <QContextMenuEvent>

/* Local includes */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"

#include "UISession.h"
#include "UIActionsPool.h"
#include "UIIndicatorsPool.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineView.h"
#include "UIDownloaderAdditions.h"
#include "UIDownloaderUserManual.h"
#ifdef Q_WS_MAC
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

#include "QIStatusBar.h"
#include "QIStateIndicator.h"
#include "UIHotKeyEditor.h"

UIMachineWindowNormal::UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI2<QMainWindow>(0, Qt::Window)
    , UIMachineWindow(pMachineLogic, uScreenId)
    , m_pIndicatorsPool(new UIIndicatorsPool(pMachineLogic->uisession()->session(), this))
    , m_pIdleTimer(0)
{
    /* "This" is machine window: */
    m_pMachineWindow = this;

    /* Set the main window in VBoxGlobal */
    if (uScreenId == 0)
        vboxGlobal().setMainWindow(this);

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

    /* Retranslate normal window finally: */
    retranslateUi();

    /* Prepare normal machine view container: */
    prepareMachineViewContainer();

    /* Prepare normal machine view: */
    prepareMachineView();

    /* Prepare handlers: */
    prepareHandlers();

    /* Load normal window settings: */
    loadWindowSettings();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

#ifdef Q_WS_MAC
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* Q_WS_MAC */

    /* Show window: */
    showSimple();
}

UIMachineWindowNormal::~UIMachineWindowNormal()
{
    /* Save normal window settings: */
    saveWindowSettings();

    /* Prepare handlers: */
    cleanupHandlers();

    /* Cleanup normal machine view: */
    cleanupMachineView();

    /* Cleanup status-bar: */
    cleanupStatusBar();
}

void UIMachineWindowNormal::sltMachineStateChanged()
{
    UIMachineWindow::sltMachineStateChanged();
    updateAppearanceOf(UIVisualElement_VirtualizationStuff);
}

void UIMachineWindowNormal::sltMediumChange(const CMediumAttachment &attachment)
{
    KDeviceType type = attachment.GetType();
    if (type == KDeviceType_HardDisk)
        updateAppearanceOf(UIVisualElement_HDStuff);
    if (type == KDeviceType_DVD)
        updateAppearanceOf(UIVisualElement_CDStuff);
    if (type == KDeviceType_Floppy)
        updateAppearanceOf(UIVisualElement_FDStuff);
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

void UIMachineWindowNormal::sltCPUExecutionCapChange()
{
    updateAppearanceOf(UIVisualElement_VirtualizationStuff);
}

void UIMachineWindowNormal::sltTryClose()
{
    UIMachineWindow::sltTryClose();
}

void UIMachineWindowNormal::sltDownloaderAdditionsEmbed()
{
    /* If there is an additions download running show the process bar: */
    if (UIDownloaderAdditions *pDl = UIDownloaderAdditions::current())
        statusBar()->addWidget(pDl->progressWidget(this), 0);
}

void UIMachineWindowNormal::sltDownloaderUserManualEmbed()
{
    /* If there is an additions download running show the process bar: */
    if (UIDownloaderUserManual *pDl = UIDownloaderUserManual::current())
        statusBar()->addWidget(pDl->progressWidget(this), 0);
}

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
    pStateIndicator = indicatorsPool()->indicator(UIIndicatorIndex_FloppyDisks);
    if (pStateIndicator->state() != KDeviceActivity_Null)
    {
        int state = console.GetDeviceActivity(KDeviceType_Floppy);
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
    else if (pIndicator == indicatorsPool()->indicator(UIIndicatorIndex_FloppyDisks))
    {
        if (machineLogic()->actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->isEnabled())
            machineLogic()->actionsPool()->action(UIActionIndex_Menu_FloppyDevices)->menu()->exec(pEvent->globalPos());
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
    m_pNameHostkey->setText(UIHotKeyCombination::toReadableString(vboxGlobal().settings().hostCombo()));
}

void UIMachineWindowNormal::retranslateUi()
{
    /* Translate parent class: */
    UIMachineWindow::retranslateUi();

    m_pNameHostkey->setToolTip(
        QApplication::translate("UIMachineWindowNormal", "Shows the currently assigned Host key.<br>"
           "This key, when pressed alone, toggles the keyboard and mouse "
           "capture state. It can also be used in combination with other keys "
           "to quickly perform actions from the main menu."));
    m_pNameHostkey->setText(UIHotKeyCombination::toReadableString(vboxGlobal().settings().hostCombo()));
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
    if (iElement & UIVisualElement_FDStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_FloppyDisks)->updateAppearance();
    if (iElement & UIVisualElement_USBStuff &&
        !indicatorsPool()->indicator(UIIndicatorIndex_USBDevices)->isHidden())
        indicatorsPool()->indicator(UIIndicatorIndex_USBDevices)->updateAppearance();
    if (iElement & UIVisualElement_NetworkStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_NetworkAdapters)->updateAppearance();
    if (iElement & UIVisualElement_SharedFolderStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_SharedFolders)->updateAppearance();
    if (iElement & UIVisualElement_VirtualizationStuff)
        indicatorsPool()->indicator(UIIndicatorIndex_Virtualization)->updateAppearance();
}

bool UIMachineWindowNormal::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            if (!isMaximizedChecked())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position */
                updateDbgWindows();
#endif
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximizedChecked())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position */
                updateDbgWindows();
#endif
            }
            break;
        }
        default:
            break;
    }
    return QIWithRetranslateUI2<QMainWindow>::event(pEvent);
}

#ifdef Q_WS_X11
bool UIMachineWindowNormal::x11Event(XEvent *pEvent)
{
    return UIMachineWindow::x11Event(pEvent);
}
#endif

void UIMachineWindowNormal::closeEvent(QCloseEvent *pEvent)
{
    return UIMachineWindow::closeEvent(pEvent);
}

void UIMachineWindowNormal::prepareConsoleConnections()
{
    /* Base-class connections: */
    UIMachineWindow::prepareConsoleConnections();

    /* Medium change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigMediumChange(const CMediumAttachment &)),
            this, SLOT(sltMediumChange(const CMediumAttachment &)));

    /* USB controller change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigUSBControllerChange()),
            this, SLOT(sltUSBControllerChange()));

    /* USB device state-change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)),
            this, SLOT(sltUSBDeviceStateChange()));

    /* Network adapter change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigNetworkAdapterChange(const CNetworkAdapter &)),
            this, SLOT(sltNetworkAdapterChange()));

    /* Shared folder change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigSharedFolderChange()),
            this, SLOT(sltSharedFolderChange()));

    /* CPU execution cap change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigCPUExecutionCapChange()),
            this, SLOT(sltCPUExecutionCapChange()));

}

void UIMachineWindowNormal::prepareMenu()
{
    setMenuBar(uisession()->newMenuBar());
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

    /* Floppy Disks: */
    QIStateIndicator *pLedFloppyDisks = indicatorsPool()->indicator(UIIndicatorIndex_FloppyDisks);
    pIndicatorBoxHLayout->addWidget(pLedFloppyDisks);
    connect(pLedFloppyDisks, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
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
    m_pNameHostkey = new QLabel(UIHotKeyCombination::toReadableString(vboxGlobal().settings().hostCombo()));
    pHostkeyLedContainerLayout->addWidget(m_pNameHostkey);

    /* Add to statusbar: */
    statusBar()->addPermanentWidget(pIndicatorBox, 0);

    /* Add the additions downloader progress bar to the status bar,
     * if a download is actually running: */
    sltDownloaderAdditionsEmbed();

    /* Add the user manual progress bar to the status bar,
     * if a download is actually running: */
    sltDownloaderUserManualEmbed();

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
    /* Setup global settings change updater: */
    connect(&vboxGlobal().settings(), SIGNAL(propertyChanged(const char *, const char *)),
            this, SLOT(sltProcessGlobalSettingChange(const char *, const char *)));
    /* Setup additions downloader listener: */
    connect(machineLogic(), SIGNAL(sigDownloaderAdditionsCreated()), this, SLOT(sltDownloaderAdditionsEmbed()));
    /* Setup user manual downloader listener: */
    connect(&msgCenter(), SIGNAL(sigDownloaderUserManualCreated()), this, SLOT(sltDownloaderUserManualEmbed()));
}

void UIMachineWindowNormal::prepareMachineView()
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = session().GetMachine().GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    /* Set central widget: */
    setCentralWidget(new QWidget);

    /* Set central widget layout: */
    centralWidget()->setLayout(m_pMachineViewContainer);

    m_pMachineView = UIMachineView::create(  this
                                           , m_uScreenId
                                           , machineLogic()->visualStateType()
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                           );

    /* Add machine view into layout: */
    m_pMachineViewContainer->addWidget(m_pMachineView, 1, 1);

    /* Setup machine view connections: */
    if (machineView())
    {
        /* Keyboard state-change updater: */
        connect(machineLogic()->keyboardHandler(), SIGNAL(keyboardStateChanged(int)), indicatorsPool()->indicator(UIIndicatorIndex_Hostkey), SLOT(setState(int)));

        /* Mouse state-change updater: */
        connect(machineLogic()->mouseHandler(), SIGNAL(mouseStateChanged(int)), indicatorsPool()->indicator(UIIndicatorIndex_Mouse), SLOT(setState(int)));

        /* Early initialize required connections: */
        indicatorsPool()->indicator(UIIndicatorIndex_Hostkey)->setState(machineLogic()->keyboardHandler()->keyboardState());
        indicatorsPool()->indicator(UIIndicatorIndex_Mouse)->setState(machineLogic()->mouseHandler()->mouseState());
    }

#ifdef VBOX_GUI_WITH_CUSTOMIZATIONS1
    /* The background has to go black: */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette(palette);
    centralWidget()->setAutoFillBackground(true);
    setAutoFillBackground(true);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */
}

void UIMachineWindowNormal::loadWindowSettings()
{
    /* Load normal window settings: */
    CMachine machine = session().GetMachine();

    /* Load extra-data settings: */
    {
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastNormalWindowPosition) :
                                     QString("%1%2").arg(VBoxDefs::GUI_LastNormalWindowPosition).arg(m_uScreenId);
        QStringList strPositionSettings = machine.GetExtraDataStringList(strPositionAddress);

        bool ok = !strPositionSettings.isEmpty(), max = false;
        int x = 0, y = 0, w = 0, h = 0;

        if (ok && strPositionSettings.size() > 0)
            x = strPositionSettings[0].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 1)
            y = strPositionSettings[1].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 2)
            w = strPositionSettings[2].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 3)
            h = strPositionSettings[3].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 4)
            max = strPositionSettings[4] == VBoxDefs::GUI_LastWindowState_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(machineWindow());

        /* If previous parameters were read correctly: */
        if (ok)
        {
            /* If previous machine state is SAVED: */
            if (machine.GetState() == KMachineState_Saved)
            {
                /* Restore window size and position: */
                m_normalGeometry = QRect(x, y, w, h);
                setGeometry(m_normalGeometry);
            }
            /* If previous machine state is not SAVED: */
            else
            {
                /* Restore only window position: */
                m_normalGeometry = QRect(x, y, width(), height());
                setGeometry(m_normalGeometry);
                if (machineView())
                    machineView()->normalizeGeometry(false /* adjust position? */);
            }
            /* Maximize if needed: */
            if (max)
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        else
        {
            /* Normalize view early to the optimal size: */
            if (machineView())
                machineView()->normalizeGeometry(true /* adjust position? */);
            /* Move newly created window to the screen center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(ar.center());
            setGeometry(m_normalGeometry);
        }

        /* Normalize view to the optimal size:
         * Note: Cause of the async behavior of X11 (at least GNOME) we have to delay this a little bit.
         * On Mac OS X and MS Windows this is not necessary and create even wrong resize events.
         * So there we set the geometry immediately. */
        if (machineView())
#ifdef Q_WS_X11
            QTimer::singleShot(0, machineView(), SLOT(sltNormalizeGeometry()));
#else /* Q_WS_X11 */
            machineView()->normalizeGeometry(true);
#endif /* !Q_WS_X11 */
    }

    /* Load availability settings: */
    {
        /* USB Stuff: */
        const CUSBController &usbController = machine.GetUSBController();
        if (    usbController.isNull()
            || !usbController.GetEnabled()
            || !usbController.GetProxyAvailable())
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

    /* Load global settings: */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
        statusBar()->setHidden(settings.isFeatureActive("noStatusBar"));
    }
}

void UIMachineWindowNormal::saveWindowSettings()
{
    CMachine machine = session().GetMachine();

    /* Save extra-data settings: */
    {
        QString strWindowPosition = QString("%1,%2,%3,%4")
                                    .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                                    .arg(m_normalGeometry.width()).arg(m_normalGeometry.height());
        if (isMaximizedChecked())
            strWindowPosition += QString(",%1").arg(VBoxDefs::GUI_LastWindowState_Max);
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastNormalWindowPosition) :
                                     QString("%1%2").arg(VBoxDefs::GUI_LastNormalWindowPosition).arg(m_uScreenId);
        machine.SetExtraData(strPositionAddress, strWindowPosition);
    }
}

void UIMachineWindowNormal::cleanupMachineView()
{
    /* Do not cleanup machine view if it is not present: */
    if (!machineView())
        return;

    UIMachineView::destroy(m_pMachineView);
    m_pMachineView = 0;
}

void UIMachineWindowNormal::cleanupStatusBar()
{
    /* Stop LED-update timer: */
    m_pIdleTimer->stop();
    m_pIdleTimer->disconnect(SIGNAL(timeout()), this, SLOT(sltUpdateIndicators()));
}

void UIMachineWindowNormal::showSimple()
{
    /* Just show window: */
    show();
}

bool UIMachineWindowNormal::isMaximizedChecked()
{
#ifdef Q_WS_MAC
    /* On the Mac the WindowStateChange signal doesn't seems to be delivered
     * when the user get out of the maximized state. So check this ourself. */
    return ::darwinIsWindowMaximized(this);
#else /* Q_WS_MAC */
    return isMaximized();
#endif /* !Q_WS_MAC */
}

