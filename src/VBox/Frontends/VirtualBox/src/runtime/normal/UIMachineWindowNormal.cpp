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
#include "UIExtraDataManager.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIIndicatorsPool.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineView.h"
#include "QIStatusBar.h"
#include "QIStatusBarIndicator.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CMediumAttachment.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"

UIMachineWindowNormal::UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pIndicatorsPool(0)
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

void UIMachineWindowNormal::sltHandleIndicatorContextMenuRequest(IndicatorType indicatorType, const QPoint &position)
{
    /* Determine action depending on indicator-type: */
    UIAction *pAction = 0;
    switch (indicatorType)
    {
        case IndicatorType_HardDisks:     pAction = gActionPool->action(UIActionIndexRuntime_Menu_HardDisks);        break;
        case IndicatorType_OpticalDisks:  pAction = gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices);   break;
        case IndicatorType_FloppyDisks:   pAction = gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices);    break;
        case IndicatorType_USB:           pAction = gActionPool->action(UIActionIndexRuntime_Menu_USBDevices);       break;
        case IndicatorType_Network:       pAction = gActionPool->action(UIActionIndexRuntime_Menu_Network);          break;
        case IndicatorType_SharedFolders: pAction = gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders);    break;
        case IndicatorType_VideoCapture:  pAction = gActionPool->action(UIActionIndexRuntime_Menu_VideoCapture);     break;
        case IndicatorType_Mouse:         pAction = gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration); break;
        case IndicatorType_Keyboard:      pAction = gActionPool->action(UIActionIndexRuntime_Menu_Keyboard);         break;
        default: break;
    }
    /* Raise action's context-menu: */
    if (pAction && pAction->isEnabled())
        pAction->menu()->exec(position);
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
    RuntimeMenuType restrictedMenus = gEDataManager->restrictedRuntimeMenuTypes(vboxGlobal().managedVMUuid());
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    setMenuBar(uisession()->newMenuBar(allowedMenus));
#endif /* !Q_WS_MAC */
}

void UIMachineWindowNormal::prepareStatusBar()
{
    /* Call to base-class: */
    UIMachineWindow::prepareStatusBar();

    /* Create status-bar: */
    setStatusBar(new QIStatusBar);
    AssertPtrReturnVoid(statusBar());
    {
        /* Create indicator-pool: */
        m_pIndicatorsPool = new UIIndicatorsPool(machineLogic()->uisession());
        AssertPtrReturnVoid(m_pIndicatorsPool);
        {
            /* Configure indicator-pool: */
            connect(m_pIndicatorsPool, SIGNAL(sigContextMenuRequest(IndicatorType, const QPoint&)),
                    this, SLOT(sltHandleIndicatorContextMenuRequest(IndicatorType, const QPoint&)));
            /* Add indicator-pool into status-bar: */
            statusBar()->addPermanentWidget(m_pIndicatorsPool, 0);
        }
    }

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

#ifdef Q_WS_MAC
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* Q_WS_MAC */
}

void UIMachineWindowNormal::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Get machine: */
    CMachine m = machine();

    /* Load GUI customizations: */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
#ifndef Q_WS_MAC
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
#endif /* !Q_WS_MAC */
        statusBar()->setHidden(settings.isFeatureActive("noStatusBar"));
        if (statusBar()->isHidden())
            m_pIndicatorsPool->setAutoUpdateIndicatorStates(false);
    }

    /* Load window geometry: */
    {
        /* Load extra-data: */
        QRect geo = gEDataManager->machineWindowGeometry(machineLogic()->visualStateType(),
                                                         m_uScreenId, vboxGlobal().managedVMUuid());

        /* If we do have proper geometry: */
        if (!geo.isNull())
        {
            /* If previous machine-state was SAVED: */
            if (m.GetState() == KMachineState_Saved)
            {
                /* Restore window geometry: */
                m_normalGeometry = geo;
                setGeometry(m_normalGeometry);
            }
            /* If previous machine-state was NOT SAVED: */
            else
            {
                /* Restore only window position: */
                m_normalGeometry = QRect(geo.x(), geo.y(), width(), height());
                setGeometry(m_normalGeometry);
                /* And normalize to the optimal-size: */
                normalizeGeometry(false);
            }

            /* Maximize (if necessary): */
            if (gEDataManager->machineWindowShouldBeMaximized(machineLogic()->visualStateType(),
                                                              m_uScreenId, vboxGlobal().managedVMUuid()))
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        /* If we do NOT have proper geometry: */
        else
        {
            /* Get available geometry, for screen with (x,y) coords if possible: */
            QRect availableGeo = !geo.isNull() ? QApplication::desktop()->availableGeometry(QPoint(geo.x(), geo.y())) :
                                                 QApplication::desktop()->availableGeometry(this);

            /* Normalize to the optimal size: */
            normalizeGeometry(true);
            /* Move newly created window to the screen-center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(availableGeo.center());
            setGeometry(m_normalGeometry);
        }

        /* Normalize to the optimal size: */
#ifdef Q_WS_X11
        QTimer::singleShot(0, this, SLOT(sltNormalizeGeometry()));
#else /* !Q_WS_X11 */
        normalizeGeometry(true);
#endif /* !Q_WS_X11 */
    }
}

void UIMachineWindowNormal::saveSettings()
{
    /* Save window geometry: */
    {
        gEDataManager->setMachineWindowGeometry(machineLogic()->visualStateType(),
                                                m_uScreenId, m_normalGeometry,
                                                isMaximizedChecked(), vboxGlobal().managedVMUuid());
    }

    /* Call to base-class: */
    UIMachineWindow::saveSettings();
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
    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return hide();

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Show in normal mode: */
    show();
}

/**
 * Adjusts machine-window size to correspond current guest screen size.
 * @param fAdjustPosition determines whether is it necessary to adjust position too.
 */
void UIMachineWindowNormal::normalizeGeometry(bool fAdjustPosition)
{
#ifndef VBOX_GUI_WITH_CUSTOMIZATIONS1
    /* Skip if maximized: */
    if (isMaximized())
        return;

    /* Calculate client window offsets: */
    QRect frameGeo = frameGeometry();
    QRect geo = geometry();
    int dl = geo.left() - frameGeo.left();
    int dt = geo.top() - frameGeo.top();
    int dr = frameGeo.right() - geo.right();
    int db = frameGeo.bottom() - geo.bottom();

    /* Get the best size w/o scroll-bars: */
    QSize s = sizeHint();

    /* Resize the frame to fit the contents: */
    s -= size();
    frameGeo.setRight(frameGeo.right() + s.width());
    frameGeo.setBottom(frameGeo.bottom() + s.height());

    /* Adjust position if necessary: */
    if (fAdjustPosition)
    {
        QRegion availableGeo;
        QDesktopWidget *dwt = QApplication::desktop();
        if (dwt->isVirtualDesktop())
            /* Compose complex available region: */
            for (int i = 0; i < dwt->numScreens(); ++i)
                availableGeo += dwt->availableGeometry(i);
        else
            /* Get just a simple available rectangle */
            availableGeo = dwt->availableGeometry(pos());

        frameGeo = VBoxGlobal::normalizeGeometry(frameGeo, availableGeo);
    }

    /* Finally, set the frame geometry: */
    setGeometry(frameGeo.left() + dl, frameGeo.top() + dt,
                frameGeo.width() - dl - dr, frameGeo.height() - dt - db);

#else /* !VBOX_GUI_WITH_CUSTOMIZATIONS1 */
    Q_UNUSED(fAdjustPosition);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */
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
            if (uisession()->isPaused())
                m_pIndicatorsPool->setAutoUpdateIndicatorStates(false);
            else if (uisession()->isRunning())
                m_pIndicatorsPool->setAutoUpdateIndicatorStates(true);
        }
    }
    if (iElement & UIVisualElement_HDStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_HardDisks);
    if (iElement & UIVisualElement_CDStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_OpticalDisks);
    if (iElement & UIVisualElement_FDStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_FloppyDisks);
    if (iElement & UIVisualElement_NetworkStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_Network);
    if (iElement & UIVisualElement_USBStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_USB);
    if (iElement & UIVisualElement_SharedFolderStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_SharedFolders);
    if (iElement & UIVisualElement_VideoCapture)
        m_pIndicatorsPool->updateAppearance(IndicatorType_VideoCapture);
    if (iElement & UIVisualElement_FeaturesStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_Features);
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


