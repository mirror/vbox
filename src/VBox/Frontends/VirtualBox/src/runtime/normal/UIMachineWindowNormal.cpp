/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class implementation
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
#include <QDesktopWidget>
#include <QMenuBar>
#include <QTimer>
#include <QContextMenuEvent>
#include <QResizeEvent>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIIndicatorsPool.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineView.h"
#include "QIStatusBar.h"
#include "QIStateIndicator.h"
#include "UIHostComboEditor.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CMediumAttachment.h"
#include "CUSBController.h"

UIMachineWindowNormal::UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pIndicatorsPool(new UIIndicatorsPool(pMachineLogic->uisession()->session(), this))
    , m_pNameHostkey(0)
    , m_pIdleTimer(0)
{
}

void UIMachineWindowNormal::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update pause and virtualization stuff: */
    updateAppearanceOf(UIVisualElement_PauseStuff | UIVisualElement_FeaturesStuff);
}

void UIMachineWindowNormal::sltMediumChange(const CMediumAttachment &attachment)
{
    /* Update corresponding medium stuff: */
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
    /* Update USB stuff: */
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowNormal::sltUSBDeviceStateChange()
{
    /* Update USB stuff: */
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowNormal::sltNetworkAdapterChange()
{
    /* Update network stuff: */
    updateAppearanceOf(UIVisualElement_NetworkStuff);
}

void UIMachineWindowNormal::sltSharedFolderChange()
{
    /* Update shared-folders stuff: */
    updateAppearanceOf(UIVisualElement_SharedFolderStuff);
}

void UIMachineWindowNormal::sltVideoCaptureChange()
{
    /* Update video-capture stuff: */
    updateAppearanceOf(UIVisualElement_VideoCapture);
}

void UIMachineWindowNormal::sltCPUExecutionCapChange()
{
    /* Update virtualization stuff: */
    updateAppearanceOf(UIVisualElement_FeaturesStuff);
}

void UIMachineWindowNormal::sltUpdateIndicators()
{
    /* Update indicators: */
    if (indicatorsPool()->indicator(IndicatorType_HardDisks))
        updateIndicatorState(indicatorsPool()->indicator(IndicatorType_HardDisks), KDeviceType_HardDisk);
    if (indicatorsPool()->indicator(IndicatorType_OpticalDisks))
        updateIndicatorState(indicatorsPool()->indicator(IndicatorType_OpticalDisks), KDeviceType_DVD);
    if (indicatorsPool()->indicator(IndicatorType_FloppyDisks))
        updateIndicatorState(indicatorsPool()->indicator(IndicatorType_FloppyDisks), KDeviceType_Floppy);
    if (indicatorsPool()->indicator(IndicatorType_USB))
        updateIndicatorState(indicatorsPool()->indicator(IndicatorType_USB), KDeviceType_USB);
    if (indicatorsPool()->indicator(IndicatorType_Network))
        updateIndicatorState(indicatorsPool()->indicator(IndicatorType_Network), KDeviceType_Network);
    if (indicatorsPool()->indicator(IndicatorType_SharedFolders))
        updateIndicatorState(indicatorsPool()->indicator(IndicatorType_SharedFolders), KDeviceType_SharedFolder);
}

void UIMachineWindowNormal::sltShowIndicatorsContextMenu(QIStateIndicator *pIndicator, QContextMenuEvent *pEvent)
{
    /* Show optical-disks LED context menu: */
    if (pIndicator == indicatorsPool()->indicator(IndicatorType_OpticalDisks))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->menu()->exec(pEvent->globalPos());
    }
    /* Show floppy-disks LED context menu: */
    else if (pIndicator == indicatorsPool()->indicator(IndicatorType_FloppyDisks))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->menu()->exec(pEvent->globalPos());
    }
    /* Show usb LED context menu: */
    else if (pIndicator == indicatorsPool()->indicator(IndicatorType_USB))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->menu()->exec(pEvent->globalPos());
    }
    /* Show network LED context menu: */
    else if (pIndicator == indicatorsPool()->indicator(IndicatorType_Network))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_Network)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_Network)->menu()->exec(pEvent->globalPos());
    }
    /* Show shared-folders LED context menu: */
    else if (pIndicator == indicatorsPool()->indicator(IndicatorType_SharedFolders))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders)->menu()->exec(pEvent->globalPos());
    }
    /* Show video-capture LED context menu: */
    else if (pIndicator == indicatorsPool()->indicator(IndicatorType_VideoCapture))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_VideoCapture)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_VideoCapture)->menu()->exec(pEvent->globalPos());
    }
    /* Show mouse LED context menu: */
    else if (pIndicator == indicatorsPool()->indicator(IndicatorType_Mouse))
    {
        if (gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration)->isEnabled())
            gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration)->menu()->exec(pEvent->globalPos());
    }
}

void UIMachineWindowNormal::sltProcessGlobalSettingChange(const char * /* aPublicName */, const char * /* aName */)
{
    /* Update host-combination status-bar label: */
    if (m_pNameHostkey)
        m_pNameHostkey->setText(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo()));
}

void UIMachineWindowNormal::prepareSessionConnections()
{
    /* Call to base-class: */
    UIMachineWindow::prepareSessionConnections();

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

    /* Video capture change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigVideoCaptureChange()),
            this, SLOT(sltVideoCaptureChange()));

    /* CPU execution cap change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigCPUExecutionCapChange()),
            this, SLOT(sltCPUExecutionCapChange()));
}

void UIMachineWindowNormal::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

#ifndef Q_WS_MAC
    /* Prepare application menu-bar: */
    CMachine machine = session().GetMachine();
    RuntimeMenuType restrictedMenus = VBoxGlobal::restrictedRuntimeMenuTypes(machine);
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    setMenuBar(uisession()->newMenuBar(allowedMenus));
#endif /* !Q_WS_MAC */
}

void UIMachineWindowNormal::prepareStatusBar()
{
    /* Call to base-class: */
    UIMachineWindow::prepareStatusBar();

    /* Setup: */
    setStatusBar(new QIStatusBar(this));
    QWidget *pIndicatorBox = new QWidget;
    QHBoxLayout *pIndicatorBoxHLayout = new QHBoxLayout(pIndicatorBox);
    VBoxGlobal::setLayoutMargin(pIndicatorBoxHLayout, 0);
    pIndicatorBoxHLayout->setSpacing(5);
    bool fAtLeastOneAddedToLeftSection = false;

    /* Hard Disks: */
    if (QIStateIndicator *pLedHardDisks = indicatorsPool()->indicator(IndicatorType_HardDisks))
    {
        pIndicatorBoxHLayout->addWidget(pLedHardDisks);
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Optical Disks: */
    if (QIStateIndicator *pLedOpticalDisks = indicatorsPool()->indicator(IndicatorType_OpticalDisks))
    {
        pIndicatorBoxHLayout->addWidget(pLedOpticalDisks);
        connect(pLedOpticalDisks, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Floppy Disks: */
    if (QIStateIndicator *pLedFloppyDisks = indicatorsPool()->indicator(IndicatorType_FloppyDisks))
    {
        pIndicatorBoxHLayout->addWidget(pLedFloppyDisks);
        connect(pLedFloppyDisks, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
        fAtLeastOneAddedToLeftSection = true;
    }

    /* USB: */
    if (QIStateIndicator *pLedUSB = indicatorsPool()->indicator(IndicatorType_USB))
    {
        pIndicatorBoxHLayout->addWidget(pLedUSB);
        connect(pLedUSB, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Network: */
    if (QIStateIndicator *pLedNetwork = indicatorsPool()->indicator(IndicatorType_Network))
    {
        pIndicatorBoxHLayout->addWidget(pLedNetwork);
        connect(pLedNetwork, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Shared Folders: */
    if (QIStateIndicator *pLedSharedFolders = indicatorsPool()->indicator(IndicatorType_SharedFolders))
    {
        pIndicatorBoxHLayout->addWidget(pLedSharedFolders);
        connect(pLedSharedFolders, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Video Capture: */
    if (QIStateIndicator *pLedVideoCapture = indicatorsPool()->indicator(IndicatorType_VideoCapture))
    {
        pIndicatorBoxHLayout->addWidget(pLedVideoCapture);
        connect(pLedVideoCapture, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Features: */
    if (QIStateIndicator *pLedFeatures = indicatorsPool()->indicator(IndicatorType_Features))
    {
        pIndicatorBoxHLayout->addWidget(pLedFeatures);
        fAtLeastOneAddedToLeftSection = true;
    }

    /* Separator: */
    if (fAtLeastOneAddedToLeftSection)
    {
        QFrame *pSeparator = new QFrame;
        pSeparator->setFrameStyle(QFrame::VLine | QFrame::Sunken);
        pIndicatorBoxHLayout->addWidget(pSeparator);
    }

    /* Mouse: */
    if (QIStateIndicator *pLedMouse = indicatorsPool()->indicator(IndicatorType_Mouse))
    {
        pIndicatorBoxHLayout->addWidget(pLedMouse);
        connect(pLedMouse, SIGNAL(contextMenuRequested(QIStateIndicator*, QContextMenuEvent*)),
                this, SLOT(sltShowIndicatorsContextMenu(QIStateIndicator*, QContextMenuEvent*)));
    }

    /* Keyboard: */
    if (QIStateIndicator *pLedKeyboard = indicatorsPool()->indicator(IndicatorType_Keyboard))
    {
        if (QWidget *pContainerWidgetHostkey = new QWidget)
        {
            if (QHBoxLayout *pContainerLayoutHostkey = new QHBoxLayout(pContainerWidgetHostkey))
            {
                VBoxGlobal::setLayoutMargin(pContainerLayoutHostkey, 0);
                pContainerLayoutHostkey->setSpacing(3);
                m_pNameHostkey = new QLabel(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo()));
                pContainerLayoutHostkey->addWidget(pLedKeyboard);
                pContainerLayoutHostkey->addWidget(m_pNameHostkey);
            }
            pIndicatorBoxHLayout->addWidget(pContainerWidgetHostkey);
        }
    }

    /* Add to status-bar: */
    statusBar()->addPermanentWidget(pIndicatorBox, 0);

    /* Create & start timer to update LEDs: */
    m_pIdleTimer = new QTimer(this);
    connect(m_pIdleTimer, SIGNAL(timeout()), this, SLOT(sltUpdateIndicators()));
    m_pIdleTimer->start(100);

#ifdef Q_WS_MAC
    /* For the status-bar on Cocoa: */
    setUnifiedTitleAndToolBarOnMac(true);
#endif /* Q_WS_MAC */
}

void UIMachineWindowNormal::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

#ifdef VBOX_GUI_WITH_CUSTOMIZATIONS1
    /* The background has to go black: */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette(palette);
    centralWidget()->setAutoFillBackground(true);
    setAutoFillBackground(true);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */

    /* Make sure host-combination LED will be updated: */
    connect(&vboxGlobal().settings(), SIGNAL(propertyChanged(const char *, const char *)),
            this, SLOT(sltProcessGlobalSettingChange(const char *, const char *)));

#ifdef Q_WS_MAC
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* Q_WS_MAC */
}

void UIMachineWindowNormal::prepareHandlers()
{
    /* Call to base-class: */
    UIMachineWindow::prepareHandlers();

    /* Connect keyboard state-change handler: */
    if (indicatorsPool()->indicator(IndicatorType_Keyboard))
        connect(machineLogic()->keyboardHandler(), SIGNAL(keyboardStateChanged(int)),
                indicatorsPool()->indicator(IndicatorType_Keyboard), SLOT(setState(int)));
    /* Connect mouse state-change handler: */
    if (indicatorsPool()->indicator(IndicatorType_Mouse))
        connect(machineLogic()->mouseHandler(), SIGNAL(mouseStateChanged(int)),
                indicatorsPool()->indicator(IndicatorType_Mouse), SLOT(setState(int)));
    /* Early initialize created connections: */
    if (indicatorsPool()->indicator(IndicatorType_Keyboard))
        indicatorsPool()->indicator(IndicatorType_Keyboard)->setState(machineLogic()->keyboardHandler()->keyboardState());
    if (indicatorsPool()->indicator(IndicatorType_Mouse))
        indicatorsPool()->indicator(IndicatorType_Mouse)->setState(machineLogic()->mouseHandler()->mouseState());
}

void UIMachineWindowNormal::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Get machine: */
    CMachine m = machine();

    /* Load extra-data settings: */
    {
        /* Load window position settings: */
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(GUI_LastNormalWindowPosition) :
                                     QString("%1%2").arg(GUI_LastNormalWindowPosition).arg(m_uScreenId);
        QStringList strPositionSettings = m.GetExtraDataStringList(strPositionAddress);
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
            max = strPositionSettings[4] == GUI_LastWindowState_Max;
        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(this);

        /* If previous parameters were read correctly: */
        if (ok)
        {
            /* If previous machine state is SAVED: */
            if (m.GetState() == KMachineState_Saved)
            {
                /* Restore window size and position: */
                m_normalGeometry = QRect(x, y, w, h);
                setGeometry(m_normalGeometry);
            }
            /* If previous machine state was not SAVED: */
            else
            {
                /* Restore only window position: */
                m_normalGeometry = QRect(x, y, width(), height());
                setGeometry(m_normalGeometry);
                if (machineView())
                    machineView()->normalizeGeometry(false);
            }
            /* Maximize if needed: */
            if (max)
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        else
        {
            /* Normalize view early to the optimal size: */
            if (machineView())
                machineView()->normalizeGeometry(true);
            /* Move newly created window to the screen center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(ar.center());
            setGeometry(m_normalGeometry);
        }

        /* Normalize view to the optimal size: */
        if (machineView())
#ifdef Q_WS_X11
            QTimer::singleShot(0, machineView(), SLOT(sltNormalizeGeometry()));
#else /* Q_WS_X11 */
            machineView()->normalizeGeometry(true);
#endif
    }

    /* Load availability settings: */
    {
        /* USB Stuff: */
        if (indicatorsPool()->indicator(IndicatorType_USB))
        {
            const CUSBController &usbController = m.GetUSBController();
            if (    usbController.isNull()
                || !usbController.GetEnabled()
                || !usbController.GetProxyAvailable())
            {
                /* Hide USB menu: */
                indicatorsPool()->indicator(IndicatorType_USB)->setHidden(true);
            }
            else
            {
                /* Toggle USB LED: */
                indicatorsPool()->indicator(IndicatorType_USB)->setState(
                    usbController.GetEnabled() ? KDeviceActivity_Idle : KDeviceActivity_Null);
            }
        }
    }

    /* Load global settings: */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
#ifndef Q_WS_MAC
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
#endif /* !Q_WS_MAC */
        statusBar()->setHidden(settings.isFeatureActive("noStatusBar"));
        if (statusBar()->isHidden())
            m_pIdleTimer->stop();
    }
}

void UIMachineWindowNormal::saveSettings()
{
    /* Get machine: */
    CMachine m = machine();

    /* Save extra-data settings: */
    {
        QString strWindowPosition = QString("%1,%2,%3,%4")
                                    .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                                    .arg(m_normalGeometry.width()).arg(m_normalGeometry.height());
        if (isMaximizedChecked())
            strWindowPosition += QString(",%1").arg(GUI_LastWindowState_Max);
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(GUI_LastNormalWindowPosition) :
                                     QString("%1%2").arg(GUI_LastNormalWindowPosition).arg(m_uScreenId);
        m.SetExtraData(strPositionAddress, strWindowPosition);
    }

    /* Call to base-class: */
    UIMachineWindow::saveSettings();
}

void UIMachineWindowNormal::cleanupStatusBar()
{
    /* Stop LED-update timer: */
    m_pIdleTimer->stop();
    m_pIdleTimer->disconnect(SIGNAL(timeout()), this, SLOT(sltUpdateIndicators()));

    /* Call to base-class: */
    UIMachineWindow::cleanupStatusBar();
}

void UIMachineWindowNormal::retranslateUi()
{
    /* Call to base-class: */
    UIMachineWindow::retranslateUi();

    /* Translate host-combo LED: */
    if (m_pNameHostkey)
    {
        m_pNameHostkey->setToolTip(
            QApplication::translate("UIMachineWindowNormal", "Shows the currently assigned Host key.<br>"
               "This key, when pressed alone, toggles the keyboard and mouse "
               "capture state. It can also be used in combination with other keys "
               "to quickly perform actions from the main menu."));
        m_pNameHostkey->setText(UIHostCombo::toReadableString(vboxGlobal().settings().hostCombo()));
    }
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
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximizedChecked())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        default:
            break;
    }
    return UIMachineWindow::event(pEvent);
}

void UIMachineWindowNormal::showInNecessaryMode()
{
    /* Show window if we have to: */
    if (uisession()->isScreenVisible(m_uScreenId))
        show();
    /* Else hide window: */
    else
        hide();
}

void UIMachineWindowNormal::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update machine window content: */
    if (iElement & UIVisualElement_PauseStuff)
    {
        if (!statusBar()->isHidden())
        {
            if (uisession()->isPaused() && m_pIdleTimer->isActive())
                m_pIdleTimer->stop();
            else if (uisession()->isRunning() && !m_pIdleTimer->isActive())
                m_pIdleTimer->start(100);
            sltUpdateIndicators();
        }
    }
    if ((iElement & UIVisualElement_HDStuff) &&
        indicatorsPool()->indicator(IndicatorType_HardDisks))
        indicatorsPool()->indicator(IndicatorType_HardDisks)->updateAppearance();
    if ((iElement & UIVisualElement_CDStuff) &&
        indicatorsPool()->indicator(IndicatorType_OpticalDisks))
        indicatorsPool()->indicator(IndicatorType_OpticalDisks)->updateAppearance();
    if ((iElement & UIVisualElement_FDStuff) &&
        indicatorsPool()->indicator(IndicatorType_FloppyDisks))
        indicatorsPool()->indicator(IndicatorType_FloppyDisks)->updateAppearance();
    if ((iElement & UIVisualElement_NetworkStuff) &&
        indicatorsPool()->indicator(IndicatorType_Network))
        indicatorsPool()->indicator(IndicatorType_Network)->updateAppearance();
    if ((iElement & UIVisualElement_USBStuff) &&
        indicatorsPool()->indicator(IndicatorType_USB) &&
        !indicatorsPool()->indicator(IndicatorType_USB)->isHidden())
        indicatorsPool()->indicator(IndicatorType_USB)->updateAppearance();
    if ((iElement & UIVisualElement_SharedFolderStuff) &&
        indicatorsPool()->indicator(IndicatorType_SharedFolders))
        indicatorsPool()->indicator(IndicatorType_SharedFolders)->updateAppearance();
    if ((iElement & UIVisualElement_VideoCapture) &&
        indicatorsPool()->indicator(IndicatorType_VideoCapture))
        indicatorsPool()->indicator(IndicatorType_VideoCapture)->updateAppearance();
    if ((iElement & UIVisualElement_FeaturesStuff) &&
        indicatorsPool()->indicator(IndicatorType_Features))
        indicatorsPool()->indicator(IndicatorType_Features)->updateAppearance();
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

void UIMachineWindowNormal::updateIndicatorState(QIStateIndicator *pIndicator, KDeviceType deviceType)
{
    /* Do NOT update indicators with NULL state: */
    if (pIndicator->state() == KDeviceActivity_Null)
        return;

    /* Paused VM have all indicator states set to IDLE: */
    bool fPaused = uisession()->isPaused();
    if (fPaused)
    {
        /* If state differs from IDLE => set IDLE one:  */
        if (pIndicator->state() != KDeviceActivity_Idle)
            pIndicator->setState(KDeviceActivity_Idle);
    }
    else
    {
        /* Get current indicator state: */
        int state = session().GetConsole().GetDeviceActivity(deviceType);
        /* If state differs => set new one:  */
        if (pIndicator->state() != state)
            pIndicator->setState(state);
    }
}

