/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineMenuBar class implementation.
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
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>

/* GUI includes: */
#include "UIMachineMenuBar.h"
#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIActionPoolRuntime.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIImageTools.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UINetworkManager.h"
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

/* COM includes: */
#include "CMachine.h"


QIMenu::QIMenu(QWidget *pParent /* = 0 */)
    : QMenu(pParent)
{
}

void QIMenu::sltHighlightFirstAction()
{
#ifdef Q_WS_WIN
    activateWindow();
#endif /* Q_WS_WIN */
    QMenu::focusNextChild();
}


UIMenuBar::UIMenuBar(QWidget *pParent /* = 0 */)
    : QMenuBar(pParent)
    , m_fShowBetaLabel(false)
{
    /* Check for beta versions: */
    if (vboxGlobal().isBeta())
        m_fShowBetaLabel = true;
}

void UIMenuBar::paintEvent(QPaintEvent *pEvent)
{
    QMenuBar::paintEvent(pEvent);
    if (m_fShowBetaLabel)
    {
        QPixmap betaLabel;
        const QString key("vbox:betaLabel");
        if (!QPixmapCache::find(key, betaLabel))
        {
            betaLabel = ::betaLabel();
            QPixmapCache::insert(key, betaLabel);
        }
        QSize s = size();
        QPainter painter(this);
        painter.setClipRect(pEvent->rect());
        painter.drawPixmap(s.width() - betaLabel.width() - 10, (height() - betaLabel.height()) / 2, betaLabel);
    }
}


UIMachineMenuBar::UIMachineMenuBar(UISession *pSession)
    : m_pSession(pSession)
{
}

QMenu* UIMachineMenuBar::createMenu(RuntimeMenuType fOptions /* = RuntimeMenuType_All */)
{
    /* Create empty menu: */
    QMenu *pMenu = new QIMenu;

    /* Fill menu with prepared items: */
    foreach (QMenu *pSubMenu, prepareSubMenus(fOptions))
        pMenu->addMenu(pSubMenu);

    /* Return filled menu: */
    return pMenu;
}

QMenuBar* UIMachineMenuBar::createMenuBar(RuntimeMenuType fOptions /* = RuntimeMenuType_All */)
{
    /* Create empty menubar: */
    QMenuBar *pMenuBar = new UIMenuBar;

    /* Fill menubar with prepared items: */
    foreach (QMenu *pSubMenu, prepareSubMenus(fOptions))
        pMenuBar->addMenu(pSubMenu);

    /* Return filled menubar: */
    return pMenuBar;
}

QList<QMenu*> UIMachineMenuBar::prepareSubMenus(RuntimeMenuType fOptions /* = RuntimeMenuType_All */)
{
    /* Create empty submenu list: */
    QList<QMenu*> preparedSubMenus;

    /* Machine submenu: */
    if (fOptions & RuntimeMenuType_Machine)
    {
        QMenu *pMenuMachine = gActionPool->action(UIActionIndexRuntime_Menu_Machine)->menu();
        prepareMenuMachine(pMenuMachine);
        preparedSubMenus << pMenuMachine;
    }

    /* View submenu: */
    if (fOptions & RuntimeMenuType_View)
    {
        QMenu *pMenuView = gActionPool->action(UIActionIndexRuntime_Menu_View)->menu();
        prepareMenuView(pMenuView);
        preparedSubMenus << pMenuView;
    }

    /* Devices submenu: */
    if (fOptions & RuntimeMenuType_Devices)
    {
        QMenu *pMenuDevices = gActionPool->action(UIActionIndexRuntime_Menu_Devices)->menu();
        prepareMenuDevices(pMenuDevices);
        preparedSubMenus << pMenuDevices;
    }

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug submenu: */
    if (fOptions & RuntimeMenuType_Debug)
    {
        if (vboxGlobal().isDebuggerEnabled())
        {
            QMenu *pMenuDebug = gActionPool->action(UIActionIndexRuntime_Menu_Debug)->menu();
            prepareMenuDebug(pMenuDebug);
            preparedSubMenus << pMenuDebug;
        }
    }
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Help submenu: */
    if (fOptions & RuntimeMenuType_Help)
    {
        QMenu *pMenuHelp = gActionPool->action(UIActionIndex_Menu_Help)->menu();
        prepareMenuHelp(pMenuHelp);
        preparedSubMenus << pMenuHelp;
    }

    /* Return a list of prepared submenus: */
    return preparedSubMenus;
}

void UIMachineMenuBar::prepareMenuMachine(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;


    /* Separator #1? */
    bool fSeparator1 = false;

    /* Settings Dialog action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_SettingsDialog)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog)->setEnabled(false);
    /* Take Snapshot action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_TakeSnapshot &&
        m_pSession->isSnapshotOperationsAllowed())
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot)->setEnabled(false);
    /* Take Screenshot action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_TakeScreenshot)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot)->setEnabled(false);
    /* Information Dialog action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_InformationDialog)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog)->setEnabled(false);

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* Keyboard Settings action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_KeyboardSettings)
    {
//        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_KeyboardSettings));
//        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_KeyboardSettings)->setEnabled(false);

    /* Mouse Integration action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_MouseIntegration)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration)->setEnabled(false);

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


    /* Separator #3? */
    bool fSeparator3 = false;

    /* Type CAD action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_TypeCAD)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD));
        fSeparator3 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD)->setEnabled(false);
#ifdef Q_WS_X11
    /* Type CABS action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_TypeCABS)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS));
        fSeparator3 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS)->setEnabled(false);
#endif /* Q_WS_X11 */

    /* Separator #3: */
    if (fSeparator3)
        pMenu->addSeparator();


    /* Separator #4? */
    bool fSeparator4 = false;

    /* Pause action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_Pause)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Pause));
        fSeparator4 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_Pause)->setEnabled(false);
    /* Reset action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_Reset)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Reset));
        fSeparator4 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_Reset)->setEnabled(false);
    /* SaveState action: */
    if (!(m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_SaveState))
        gActionPool->action(UIActionIndexRuntime_Simple_Save)->setEnabled(false);
    /* Shutdown action: */
    if (m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_Shutdown)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown));
        fSeparator4 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_Shutdown)->setEnabled(false);
    /* PowerOff action: */
    if (!(m_pSession->allowedActionsMenuMachine() & RuntimeMenuMachineActionType_PowerOff))
        gActionPool->action(UIActionIndexRuntime_Simple_PowerOff)->setEnabled(false);

#ifndef Q_WS_MAC
    /* Separator #3: */
    if (fSeparator4)
        pMenu->addSeparator();
#endif /* !Q_WS_MAC */


    /* Close action: */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Close));
    if (m_pSession->isAllCloseActionsRestricted())
        gActionPool->action(UIActionIndexRuntime_Simple_Close)->setEnabled(false);
}

void UIMachineMenuBar::prepareMenuView(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;


    /* Mode flags: */
    bool fIsAllowedFullscreen = m_pSession->isVisualStateAllowed(UIVisualStateType_Fullscreen) &&
                                (m_pSession->allowedActionsMenuView() & RuntimeMenuViewActionType_Fullscreen);
    bool fIsAllowedSeamless = m_pSession->isVisualStateAllowed(UIVisualStateType_Seamless) &&
                              (m_pSession->allowedActionsMenuView() & RuntimeMenuViewActionType_Seamless);
    bool fIsAllowedScale = m_pSession->isVisualStateAllowed(UIVisualStateType_Scale) &&
                           (m_pSession->allowedActionsMenuView() & RuntimeMenuViewActionType_Scale);

    /* Fullscreen action: */
    gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen)->setEnabled(fIsAllowedFullscreen);
    if (fIsAllowedFullscreen)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen));

    /* Seamless action: */
    gActionPool->action(UIActionIndexRuntime_Toggle_Seamless)->setEnabled(fIsAllowedSeamless);
    if (fIsAllowedSeamless)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless));

    /* Scale action: */
    gActionPool->action(UIActionIndexRuntime_Toggle_Scale)->setEnabled(fIsAllowedScale);
    if (fIsAllowedScale)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Scale));

    /* Mode separator: */
    if (fIsAllowedFullscreen || fIsAllowedSeamless || fIsAllowedScale)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* Guest Autoresize action: */
    if (m_pSession->allowedActionsMenuView() & RuntimeMenuViewActionType_GuestAutoresize)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize));
        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->setEnabled(false);

    /* Adjust Window action: */
    if (m_pSession->allowedActionsMenuView() & RuntimeMenuViewActionType_AdjustWindow)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow));
        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setEnabled(false);

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


    /* Status-bar submenu: */
    if (m_pSession->allowedActionsMenuView() & RuntimeMenuViewActionType_StatusBar)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_StatusBar));
        gActionPool->action(UIActionIndexRuntime_Menu_StatusBar)->menu()->addAction(gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings));
        gActionPool->action(UIActionIndexRuntime_Menu_StatusBar)->menu()->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar));
    }
    else
    {
        gActionPool->action(UIActionIndexRuntime_Menu_StatusBar)->setEnabled(false);
        gActionPool->action(UIActionIndexRuntime_Simple_StatusBarSettings)->setEnabled(false);
        gActionPool->action(UIActionIndexRuntime_Toggle_StatusBar)->setEnabled(false);
    }
}

void UIMachineMenuBar::prepareMenuDevices(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;


    /* Separator #1? */
    bool fSeparator1 = false;

    /* Optical Devices submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_OpticalDevices)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->setEnabled(false);
    /* Floppy Devices submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_FloppyDevices)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->setEnabled(false);
    /* USB Devices submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_USBDevices)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_USBDevices));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->setEnabled(false);
    /* Web Cams submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_WebCams)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_WebCams));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Menu_WebCams)->setEnabled(false);
    /* Shared Clipboard submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_SharedClipboard)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_SharedClipboard));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Menu_SharedClipboard)->setEnabled(false);
    /* Drag&Drop submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_DragAndDrop)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_DragAndDrop));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Menu_DragAndDrop)->setEnabled(false);
    /* Network submenu: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_NetworkSettings)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Menu_Network));
        gActionPool->action(UIActionIndexRuntime_Menu_Network)->menu()->addAction(gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings));
        fSeparator1 = true;
    }
    else
    {
        gActionPool->action(UIActionIndexRuntime_Menu_Network)->setEnabled(false);
        gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings)->setEnabled(false);
    }
    /* Shared Folders Settings action: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_SharedFoldersSettings)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings)->setEnabled(false);

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* VRDE Server action: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_VRDEServer)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer));
        if (!m_pSession->isExtensionPackUsable())
            gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer)->setEnabled(false);
        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer)->setEnabled(false);
    /* Video Capture action: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_VideoCapture)
    {
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture));
        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture)->setEnabled(false);

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


    /* Install Guest Tools action: */
    if (m_pSession->allowedActionsMenuDevices() & RuntimeMenuDevicesActionType_InstallGuestTools)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InstallGuestTools));
    else
        gActionPool->action(UIActionIndexRuntime_Simple_InstallGuestTools)->setEnabled(false);
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineMenuBar::prepareMenuDebug(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Statistics action: */
    if (m_pSession->allowedActionsMenuDebugger() & RuntimeMenuDebuggerActionType_Statistics)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Statistics));
    else
        gActionPool->action(UIActionIndexRuntime_Simple_Statistics)->setEnabled(false);
    /* Command Line action: */
    if (m_pSession->allowedActionsMenuDebugger() & RuntimeMenuDebuggerActionType_CommandLine)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_CommandLine));
    else
        gActionPool->action(UIActionIndexRuntime_Simple_CommandLine)->setEnabled(false);
    /* Logging action: */
    if (m_pSession->allowedActionsMenuDebugger() & RuntimeMenuDebuggerActionType_Logging)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Logging));
    else
        gActionPool->action(UIActionIndexRuntime_Toggle_Logging)->setEnabled(false);
    /* Log Dialog action: */
    if (m_pSession->allowedActionsMenuDebugger() & RuntimeMenuDebuggerActionType_LogDialog)
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_LogDialog));
    else
        gActionPool->action(UIActionIndex_Simple_LogDialog)->setEnabled(false);
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineMenuBar::prepareMenuHelp(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;


    /* Separator #1? */
    bool fSeparator1 = false;

    /* Contents action: */
    if (m_pSession->allowedActionsMenuHelp() & RuntimeMenuHelpActionType_Contents)
    {
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Contents));
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Contents), SIGNAL(triggered()),
                            &msgCenter(), SLOT(sltShowHelpHelpDialog()));
        fSeparator1 = true;
    }
    else
        gActionPool->action(UIActionIndex_Simple_Contents)->setEnabled(false);
    /* Web Site action: */
    if (m_pSession->allowedActionsMenuHelp() & RuntimeMenuHelpActionType_WebSite)
    {
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_WebSite));
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_WebSite), SIGNAL(triggered()),
                            &msgCenter(), SLOT(sltShowHelpWebDialog()));
        fSeparator1 = true;
    }
    else
        gActionPool->action(RuntimeMenuHelpActionType_WebSite)->setEnabled(false);

    /* Separator #1: */
    if (fSeparator1)
        pMenu->addSeparator();


    /* Separator #2? */
    bool fSeparator2 = false;

    /* Reset Warnings action: */
    if (m_pSession->allowedActionsMenuHelp() & RuntimeMenuHelpActionType_ResetWarnings)
    {
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_ResetWarnings));
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                            &msgCenter(), SLOT(sltResetSuppressedMessages()));
        fSeparator2 = true;
    }
    else
        gActionPool->action(UIActionIndex_Simple_ResetWarnings)->setEnabled(false);

    /* Separator #2: */
    if (fSeparator2)
        pMenu->addSeparator();


#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# ifndef Q_WS_MAC
    /* Separator #3? */
    bool fSeparator3 = false;
# endif /* !Q_WS_MAC */

    /* Reset Warnings action: */
    if (m_pSession->allowedActionsMenuHelp() & RuntimeMenuHelpActionType_NetworkAccessManager)
    {
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_NetworkAccessManager));
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_NetworkAccessManager), SIGNAL(triggered()),
                            gNetworkManager, SLOT(show()));
# ifndef Q_WS_MAC
        fSeparator3 = true;
# endif /* !Q_WS_MAC */
    }
    else
        gActionPool->action(UIActionIndex_Simple_NetworkAccessManager)->setEnabled(false);

# ifndef Q_WS_MAC
    /* Separator #3: */
    if (fSeparator3)
        pMenu->addSeparator();
# endif /* !Q_WS_MAC */
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */


    /* About action: */
#ifdef Q_WS_MAC
    if (m_pSession->allowedActionsMenuApplication() & RuntimeMenuApplicationActionType_About)
#else /* !Q_WS_MAC */
    if (m_pSession->allowedActionsMenuHelp() & RuntimeMenuHelpActionType_About)
#endif /* Q_WS_MAC */
    {
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_About));
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                            &msgCenter(), SLOT(sltShowHelpAboutDialog()));
    }
    else
        gActionPool->action(UIActionIndex_Simple_About)->setEnabled(false);

    /* Preferences action: */
#ifdef Q_WS_MAC
    if (m_pSession->allowedActionsMenuApplication() & RuntimeMenuApplicationActionType_Preferences)
#else /* !Q_WS_MAC */
    if (m_pSession->allowedActionsMenuHelp() & RuntimeMenuHelpActionType_Preferences)
#endif /* Q_WS_MAC */
    {
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Preferences));
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Preferences), SIGNAL(triggered()),
                            m_pSession->machineLogic(), SLOT(sltShowGlobalPreferences()));
    }
    else
        gActionPool->action(UIActionIndex_Simple_Preferences)->setEnabled(false);
}

