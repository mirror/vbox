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
#include <QMenuBar>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>

/* GUI includes: */
#include "UIMachineMenuBar.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIImageTools.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UINetworkManager.h"
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

/* COM includes: */
#include "CMachine.h"


/**
 * QMenu sub-class with extended functionality.
 * Allows to highlight first menu item for popped up menu.
 */
class QIMenu : public QMenu
{
    Q_OBJECT;

public:

    /** Constructor. */
    QIMenu() : QMenu(0) {}

private slots:

    /** Highlights first menu action for popped up menu. */
    void sltHighlightFirstAction()
    {
#ifdef Q_WS_WIN
        activateWindow();
#endif /* Q_WS_WIN */
        QMenu::focusNextChild();
    }
};


/**
 * QMenuBar sub-class with extended functionality.
 * Reflects BETA label when necessary.
 */
class UIMenuBar: public QMenuBar
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIMenuBar(QWidget *pParent = 0)
        : QMenuBar(pParent)
        , m_fShowBetaLabel(false)
    {
        /* Check for beta versions: */
        if (vboxGlobal().isBeta())
            m_fShowBetaLabel = true;
    }

protected:

    /** Paint-event reimplementation. */
    void paintEvent(QPaintEvent *pEvent)
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

private:

    /** Reflects whether we should show BETA label or not. */
    bool m_fShowBetaLabel;
};


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
        CMachine machine = m_pSession->session().GetMachine();
        if (vboxGlobal().isDebuggerEnabled(machine))
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

    /* Machine submenu: */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SettingsDialog));
    if (m_pSession->isSnapshotOperationsAllowed())
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot));
    else
        gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot)->setEnabled(false);
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD));
#ifdef Q_WS_X11
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS));
#endif /* Q_WS_X11 */
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Pause));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Reset));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Close));
    if (m_pSession->isAllCloseActionsRestricted())
        gActionPool->action(UIActionIndexRuntime_Simple_Close)->setEnabled(false);
}

void UIMachineMenuBar::prepareMenuView(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* View submenu: */
    bool fIsAllowedFullscreen = m_pSession->isVisualStateAllowedFullscreen();
    bool fIsAllowedSeamless = m_pSession->isVisualStateAllowedSeamless();
    bool fIsAllowedScale = m_pSession->isVisualStateAllowedScale();
    gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen)->setEnabled(fIsAllowedFullscreen);
    if (fIsAllowedFullscreen)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen));
    gActionPool->action(UIActionIndexRuntime_Toggle_Seamless)->setEnabled(fIsAllowedSeamless);
    if (fIsAllowedSeamless)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless));
    gActionPool->action(UIActionIndexRuntime_Toggle_Scale)->setEnabled(fIsAllowedScale);
    if (fIsAllowedScale)
        pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Scale));
    if (fIsAllowedFullscreen || fIsAllowedSeamless || fIsAllowedScale)
        pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow));
}

void UIMachineMenuBar::prepareMenuDevices(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Devices submenu: */
    pMenu->addMenu(gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices)->menu());
    pMenu->addMenu(gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices)->menu());
    pMenu->addMenu(gActionPool->action(UIActionIndexRuntime_Menu_USBDevices)->menu());
    pMenu->addMenu(gActionPool->action(UIActionIndexRuntime_Menu_WebCams)->menu());
    pMenu->addMenu(gActionPool->action(UIActionIndexRuntime_Menu_SharedClipboard)->menu());
    pMenu->addMenu(gActionPool->action(UIActionIndexRuntime_Menu_DragAndDrop)->menu());
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_NetworkSettings));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersSettings));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VideoCapture));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InstallGuestTools));
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineMenuBar::prepareMenuDebug(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Debug submenu: */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Statistics));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_CommandLine));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Logging));
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_LogDialog));
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineMenuBar::prepareMenuHelp(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Help submenu: */
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Contents));
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_WebSite));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_ResetWarnings));
    pMenu->addSeparator();
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_NetworkAccessManager));
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_About));

    /* Prepare connections: */
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Contents), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_WebSite), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpWebDialog()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltResetSuppressedMessages()));
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_NetworkAccessManager), SIGNAL(triggered()),
                        gNetworkManager, SLOT(show()));
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpAboutDialog()));
}

#include "UIMachineMenuBar.moc"

