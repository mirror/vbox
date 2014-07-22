/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineLogicNormal class implementation.
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
#include <QMenu>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineWindow.h"
#include "UIStatusBarEditorWindow.h"
#include "UIExtraDataManager.h"
#include "UIFrameBuffer.h"
#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CDisplay.h"

UIMachineLogicNormal::UIMachineLogicNormal(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Normal)
{
}

bool UIMachineLogicNormal::checkAvailability()
{
    /* Normal mode is always available: */
    return true;
}

void UIMachineLogicNormal::sltCheckForRequestedVisualStateType()
{
    /* Do not try to change visual-state type if machine was not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
        return;

    /* Check requested visual-state types: */
    switch (uisession()->requestedVisualState())
    {
        /* If 'seamless' visual-state type is requested: */
        case UIVisualStateType_Seamless:
        {
            /* And supported: */
            if (uisession()->isGuestSupportsSeamless())
            {
                LogRel(("UIMachineLogicNormal::sltCheckForRequestedVisualStateType: "
                        "Going 'seamless' as requested...\n"));
                uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
                uisession()->changeVisualState(UIVisualStateType_Seamless);
            }
            break;
        }
        default:
            break;
    }
}

void UIMachineLogicNormal::sltOpenStatusBarSettings()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Make sure status-bar is enabled: */
    const bool fEnabled = gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Prevent user from opening another one editor or toggle status-bar: */
    gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings)->setEnabled(false);
    gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar)->setEnabled(false);
    /* Create status-bar editor: */
    UIStatusBarEditorWindow *pStatusBarEditor = new UIStatusBarEditorWindow(activeMachineWindow());
    AssertPtrReturnVoid(pStatusBarEditor);
    {
        /* Configure status-bar editor: */
        connect(pStatusBarEditor, SIGNAL(destroyed(QObject*)),
                this, SLOT(sltStatusBarSettingsClosed()));
#ifdef Q_WS_MAC
        connect(this, SIGNAL(sigNotifyAbout3DOverlayVisibilityChange(bool)),
                pStatusBarEditor, SLOT(sltActivateWindow()));
#endif /* Q_WS_MAC */
        /* Show window: */
        pStatusBarEditor->show();
    }
}

void UIMachineLogicNormal::sltStatusBarSettingsClosed()
{
    /* Make sure status-bar is enabled: */
    const bool fEnabled = gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Allow user to open editor and toggle status-bar again: */
    gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings)->setEnabled(true);
    gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar)->setEnabled(true);
}

void UIMachineLogicNormal::sltToggleStatusBar()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Invert status-bar availability option: */
    const bool fEnabled = gEDataManager->statusBarEnabled(vboxGlobal().managedVMUuid());
    gEDataManager->setStatusBarEnabled(!fEnabled, vboxGlobal().managedVMUuid());
}

void UIMachineLogicNormal::sltPrepareMenuViewPopup()
{
    /* Make sure sender is valid: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsgReturnVoid(pMenu, ("This slot should be called only on View popup-menu show!\n"));

    /* Clear contents: */
    pMenu->clear();

    /* Add default contents: */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize));
    pMenu->addSeparator();

    /* Check if guest additions are enabled: */
    const bool fGAEnabled = uisession()->isGuestAdditionsActive() &&
                            uisession()->isGuestSupportsGraphics();

    /* For each the machine-window: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
    {
        /* Add 'Virtual Screen %1' menu: */
        const int iScreenID = pMachineWindow->screenId();
        QMenu *pSubMenu = pMenu->addMenu(QApplication::translate("UIMultiScreenLayout",
                                                                 "Virtual Screen %1").arg(iScreenID + 1));
        pSubMenu->setProperty("Screen ID", iScreenID);
        pSubMenu->setEnabled(fGAEnabled);
        connect(pSubMenu, SIGNAL(aboutToShow()), this, SLOT(sltPrepareMenuViewVirtualScreen()));
    }
}

void UIMachineLogicNormal::sltPrepareMenuViewVirtualScreen()
{
    /* Make sure sender is valid: */
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsgReturnVoid(pMenu, ("This slot should be called only on 'Virtual Screen %1' menu show!\n"));

    /* Clear contents: */
    pMenu->clear();

    /* Prepare new contents: */
    const QList<QSize> sizes = QList<QSize>()
                               << QSize(640, 480)
                               << QSize(800, 600)
                               << QSize(1024, 768)
                               << QSize(1280, 720)
                               << QSize(1280, 800)
                               << QSize(1366, 768)
                               << QSize(1440, 900)
                               << QSize(1600, 900)
                               << QSize(1680, 1050)
                               << QSize(1920, 1080)
                               << QSize(1920, 1200);

    /* Get corresponding screen ID and frame-buffer: */
    const int iScreenID = pMenu->property("Screen ID").toInt();
    const UIFrameBuffer *pFrameBuffer = uisession()->frameBuffer(iScreenID);

    /* Create exclusive action-group: */
    QActionGroup *pActionGroup = new QActionGroup(pMenu);
    AssertPtrReturnVoid(pActionGroup);
    {
        /* Configure exclusive action-group: */
        pActionGroup->setExclusive(true);
        /* For every available size: */
        foreach (const QSize &size, sizes)
        {
            /* Create exclusive action: */
            QAction *pAction = pActionGroup->addAction(UIActionPoolRuntime::tr("Resize to %1x%2", "Virtual Screen")
                                                                               .arg(size.width()).arg(size.height()));
            AssertPtrReturnVoid(pAction);
            {
                /* Configure exclusive action: */
                pAction->setProperty("Screen ID", iScreenID);
                pAction->setProperty("Requested Size", size);
                pAction->setCheckable(true);
                if (pFrameBuffer->width() == (ulong)size.width() &&
                    pFrameBuffer->height() == (ulong)size.height())
                {
                    pAction->setChecked(true);
                }
                /* Insert group actions into menu: */
                pMenu->addActions(pActionGroup->actions());
            }
        }
        /* Install listener for exclusive action-group: */
        connect(pActionGroup, SIGNAL(triggered(QAction*)),
                this, SLOT(sltHandleActionTriggerViewVirtualScreen(QAction*)));
    }
}

void UIMachineLogicNormal::sltHandleActionTriggerViewVirtualScreen(QAction *pAction)
{
    /* Make sure sender is valid: */
    AssertMsgReturnVoid(pAction, ("This slot should be called only on 'Virtual Screen %1' menu action trigger!\n"));

    /* Resize guest to required size: */
    const int iScreenID = pAction->property("Screen ID").toInt();
    const QSize size = pAction->property("Requested Size").toSize();
    CDisplay display = session().GetConsole().GetDisplay();
    display.SetVideoModeHint(iScreenID, uisession()->isScreenVisible(iScreenID),
                             false, 0, 0, size.width(), size.height(), 0);
}

void UIMachineLogicNormal::sltPrepareHardDisksMenu()
{
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should be called only on Hard Disks menu show!\n"));
    pMenu->clear();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_StorageSettings));
}

void UIMachineLogicNormal::sltPrepareSharedFoldersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Shared Folders menu show!\n"));
    menu->clear();
    menu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings));
}

void UIMachineLogicNormal::sltPrepareVideoCaptureMenu()
{
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should be called only on Video Capture menu show!\n"));
    pMenu->clear();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_VideoCaptureSettings));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture));
}

void UIMachineLogicNormal::sltPrepareKeyboardMenu()
{
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should be called only on Keyboard menu show!\n"));
    pMenu->clear();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_KeyboardSettings));
}

void UIMachineLogicNormal::sltPrepareMouseIntegrationMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Mouse Integration Menu show!\n"));
    menu->clear();
    menu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
}

void UIMachineLogicNormal::prepareActionConnections()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionConnections();

    /* "View" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Menu_ViewPopup)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareMenuViewPopup()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToFullscreen()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToSeamless()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToScale()));
    connect(gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings), SIGNAL(triggered(bool)),
            this, SLOT(sltOpenStatusBarSettings()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar), SIGNAL(triggered(bool)),
            this, SLOT(sltToggleStatusBar()));

    /* "Device" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Menu_HardDisks)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareHardDisksMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedFoldersMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_VideoCapture)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareVideoCaptureMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_Keyboard)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareKeyboardMenu()));
    connect(gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareMouseIntegrationMenu()));
}

void UIMachineLogicNormal::prepareMachineWindows()
{
    /* Do not create machine-window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC // TODO: Is that really need here?
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Get monitors count: */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();
    /* Create machine window(s): */
    for (ulong uScreenId = 0; uScreenId < uMonitorCount; ++ uScreenId)
        addMachineWindow(UIMachineWindow::create(this, uScreenId));
    /* Order machine window(s): */
    for (ulong uScreenId = uMonitorCount; uScreenId > 0; -- uScreenId)
        machineWindows()[uScreenId - 1]->raise();

    /* Mark machine-window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicNormal::cleanupMachineWindows()
{
    /* Do not destroy machine-window(s) if they destroyed already: */
    if (!isMachineWindowsCreated())
        return;

    /* Mark machine-window(s) destroyed: */
    setMachineWindowsCreated(false);

    /* Cleanup machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
}

void UIMachineLogicNormal::cleanupActionConnections()
{
    /* "View" actions disconnections: */
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToFullscreen()));
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToSeamless()));
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToScale()));
    disconnect(gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings), SIGNAL(triggered(bool)),
               this, SLOT(sltOpenStatusBarSettings()));
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar), SIGNAL(triggered(bool)),
               this, SLOT(sltToggleStatusBar()));

    /* Call to base-class: */
    UIMachineLogic::cleanupActionConnections();
}

void UIMachineLogicNormal::updateMenuView()
{
    /* Call to base-class: */
    UIMachineLogic::updateMenuView();

    /* Get corresponding menu: */
    QMenu *pMenu = gActionPool->action(UIActionIndexRuntime_Menu_View)->menu();
    AssertPtrReturnVoid(pMenu);

    /* Separator #1? */
    bool fSeparator1 = false;

    /* 'Adjust Window' action: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_AdjustWindow)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setEnabled(false);

    /* 'Guest Autoresize' action: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_GuestAutoresize)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setEnabled(false);

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();

    /* 'Status Bar' submenu: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_StatusBar)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_StatusBar));
    else
        gActionPool->action(UIActionIndexRuntime_Menu_StatusBar)->setEnabled(false);
    updateMenuViewStatusBar();
}

void UIMachineLogicNormal::updateMenuViewStatusBar()
{
    /* Get corresponding menu: */
    QMenu *pMenu = gActionPool->action(UIActionIndexRuntime_Menu_StatusBar)->menu();
    AssertPtrReturnVoid(pMenu);
    /* Clear contents: */
    pMenu->clear();

    /* 'Status Bar Settings' action: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_StatusBarSettings)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings));
    else
        gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings)->setEnabled(false);

    /* 'Toggle Status Bar' action: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_ToggleStatusBar)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar));
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar)->setEnabled(false);
}

