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
    const bool fEnabled = gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Prevent user from opening another one editor or toggle status-bar: */
    gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings)->setEnabled(false);
    gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->setEnabled(false);
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
    const bool fEnabled = gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Allow user to open editor and toggle status-bar again: */
    gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings)->setEnabled(true);
    gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->setEnabled(true);
}

void UIMachineLogicNormal::sltToggleStatusBar()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Invert status-bar availability option: */
    const bool fEnabled = gEDataManager->statusBarEnabled(vboxGlobal().managedVMUuid());
    gEDataManager->setStatusBarEnabled(!fEnabled, vboxGlobal().managedVMUuid());
}

void UIMachineLogicNormal::sltHandleActionTriggerViewResize(int iIndex, const QSize &size)
{
    /* Resize guest to required size: */
    CDisplay display = session().GetConsole().GetDisplay();
    display.SetVideoModeHint(iIndex, uisession()->isScreenVisible(iIndex),
                             false, 0, 0, size.width(), size.height(), 0);
}

void UIMachineLogicNormal::sltPrepareHardDisksMenu()
{
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should be called only on Hard Disks menu show!\n"));
    pMenu->clear();
    pMenu->addAction(gpActionPool->action(UIActionIndexRT_M_Devices_M_HardDrives_S_Settings));
}

void UIMachineLogicNormal::sltPrepareSharedFoldersMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Shared Folders menu show!\n"));
    menu->clear();
    menu->addAction(gpActionPool->action(UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings));
}

void UIMachineLogicNormal::sltPrepareVideoCaptureMenu()
{
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should be called only on Video Capture menu show!\n"));
    pMenu->clear();
    pMenu->addAction(gpActionPool->action(UIActionIndexRT_M_Devices_M_VideoCapture_S_Settings));
    pMenu->addAction(gpActionPool->action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start));
}

void UIMachineLogicNormal::sltPrepareKeyboardMenu()
{
    QMenu *pMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pMenu, ("This slot should be called only on Keyboard menu show!\n"));
    pMenu->clear();
    pMenu->addAction(gpActionPool->action(UIActionIndexRT_M_Machine_M_Keyboard_S_Settings));
}

void UIMachineLogicNormal::sltPrepareMouseIntegrationMenu()
{
    QMenu *menu = qobject_cast<QMenu*>(sender());
    AssertMsg(menu, ("This slot should be called only on Mouse Integration Menu show!\n"));
    menu->clear();
    menu->addAction(gpActionPool->action(UIActionIndexRT_M_Machine_M_Mouse_T_Integration));
}

void UIMachineLogicNormal::prepareActionConnections()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionConnections();

    /* "View" actions connections: */
    connect(gpActionPool, SIGNAL(sigNotifyAboutTriggeringViewResize(int, const QSize&)),
            this, SLOT(sltHandleActionTriggerViewResize(int, const QSize&)));
    connect(gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToFullscreen()));
    connect(gpActionPool->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToSeamless()));
    connect(gpActionPool->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToScale()));
    connect(gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings), SIGNAL(triggered(bool)),
            this, SLOT(sltOpenStatusBarSettings()));
    connect(gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility), SIGNAL(triggered(bool)),
            this, SLOT(sltToggleStatusBar()));

    /* "Device" actions connections: */
    connect(gpActionPool->action(UIActionIndexRT_M_Devices_M_HardDrives)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareHardDisksMenu()));
    connect(gpActionPool->action(UIActionIndexRT_M_Devices_M_SharedFolders)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareSharedFoldersMenu()));
    connect(gpActionPool->action(UIActionIndexRT_M_Devices_M_VideoCapture)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareVideoCaptureMenu()));
    connect(gpActionPool->action(UIActionIndexRT_M_Machine_M_Keyboard)->menu(), SIGNAL(aboutToShow()),
            this, SLOT(sltPrepareKeyboardMenu()));
    connect(gpActionPool->action(UIActionIndexRT_M_Machine_M_Mouse)->menu(), SIGNAL(aboutToShow()),
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

    /* Listen for frame-buffer resize: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        connect(pMachineWindow, SIGNAL(sigFrameBufferResize()),
                this, SIGNAL(sigFrameBufferResize()));

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
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToFullscreen()));
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToSeamless()));
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToScale()));
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings), SIGNAL(triggered(bool)),
               this, SLOT(sltOpenStatusBarSettings()));
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility), SIGNAL(triggered(bool)),
               this, SLOT(sltToggleStatusBar()));

    /* Call to base-class: */
    UIMachineLogic::cleanupActionConnections();
}

