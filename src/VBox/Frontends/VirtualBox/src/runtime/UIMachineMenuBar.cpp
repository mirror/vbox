/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineMenuBar class implementation
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
#include "UIExtraDataEventHandler.h"
#include "UIImageTools.h"
#include "UINetworkManager.h"

/* COM includes: */
#include "CMachine.h"

/* Helper QMenu reimplementation which allows
 * to highlight first menu item for popped up menu: */
class QIMenu : public QMenu
{
    Q_OBJECT;

public:

    QIMenu() : QMenu(0) {}

private slots:

    void sltSelectFirstAction()
    {
#ifdef Q_WS_WIN
        activateWindow();
#endif
        QMenu::focusNextChild();
    }
};

class UIMenuBar: public QMenuBar
{
public:

    UIMenuBar(QWidget *pParent = 0)
      : QMenuBar(pParent)
      , m_fShowBetaLabel(false)
    {
        /* Check for beta versions */
        if (vboxGlobal().isBeta())
            m_fShowBetaLabel = true;
    }

protected:

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

    /* Private member vars */
    bool m_fShowBetaLabel;
};

UIMachineMenuBar::UIMachineMenuBar()
    /* On the Mac we add some items only the first time, cause otherwise they
     * will be merged more than once to the application menu by Qt. */
    : m_fIsFirstTime(true)
{
}

QMenu* UIMachineMenuBar::createMenu(UIMainMenuType fOptions /* = UIMainMenuType_All */)
{
    /* Create empty menu: */
    QMenu *pMenu = new QIMenu;

    /* Fill menu with prepared items: */
    foreach (QMenu *pSubMenu, prepareSubMenus(fOptions))
        pMenu->addMenu(pSubMenu);

    /* Return filled menu: */
    return pMenu;
}

QMenuBar* UIMachineMenuBar::createMenuBar(UIMainMenuType fOptions /* = UIMainMenuType_All */)
{
    /* Create empty menubar: */
    QMenuBar *pMenuBar = new UIMenuBar;

    /* Fill menubar with prepared items: */
    foreach (QMenu *pSubMenu, prepareSubMenus(fOptions))
        pMenuBar->addMenu(pSubMenu);

    /* Return filled menubar: */
    return pMenuBar;
}

QList<QMenu*> UIMachineMenuBar::prepareSubMenus(UIMainMenuType fOptions /* = UIMainMenuType_All */)
{
    /* Create empty submenu list: */
    QList<QMenu*> preparedSubMenus;

    /* Machine submenu: */
    if (fOptions & UIMainMenuType_Machine)
    {
        QMenu *pMenuMachine = gActionPool->action(UIActionIndexRuntime_Menu_Machine)->menu();
        prepareMenuMachine(pMenuMachine);
        preparedSubMenus << pMenuMachine;
    }

    /* View submenu: */
    if (fOptions & UIMainMenuType_View)
    {
        QMenu *pMenuView = gActionPool->action(UIActionIndexRuntime_Menu_View)->menu();
        prepareMenuView(pMenuView);
        preparedSubMenus << pMenuView;
    }

    /* Devices submenu: */
    if (fOptions & UIMainMenuType_Devices)
    {
        QMenu *pMenuDevices = gActionPool->action(UIActionIndexRuntime_Menu_Devices)->menu();
        prepareMenuDevices(pMenuDevices);
        preparedSubMenus << pMenuDevices;
    }

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* Debug submenu: */
    if (fOptions & UIMainMenuType_Debug)
    {
        CMachine machine; /** @todo we should try get the machine here. But we'll
                           *        probably be fine with the cached values. */
        if (vboxGlobal().isDebuggerEnabled(machine))
        {
            QMenu *pMenuDebug = gActionPool->action(UIActionIndexRuntime_Menu_Debug)->menu();
            prepareMenuDebug(pMenuDebug);
            preparedSubMenus << pMenuDebug;
        }
    }
#endif

    /* Help submenu: */
    if (fOptions & UIMainMenuType_Help)
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
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeSnapshot));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TakeScreenshot));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_InformationDialog));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_MouseIntegration));
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCAD));
#ifdef Q_WS_X11
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_TypeCABS));
#endif
    pMenu->addSeparator();
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Pause));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Reset));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Shutdown));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_Close));
}

void UIMachineMenuBar::prepareMenuView(QMenu *pMenu)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* View submenu: */
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_Scale));
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
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_NetworkAdaptersDialog));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Simple_SharedFoldersDialog));
    pMenu->addAction(gActionPool->action(UIActionIndexRuntime_Toggle_VRDEServer));
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

    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_NetworkAccessManager));
#ifdef VBOX_WITH_REGISTRATION
    pMenu->addAction(gActionPool->action(UIActionIndex_Simple_Register));
#endif

#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    if (m_fIsFirstTime)
# endif
        pMenu->addAction(gActionPool->action(UIActionIndex_Simple_About));

#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    /* Because this connections are done to VBoxGlobal, they are needed once only.
     * Otherwise we will get the slots called more than once. */
    if (m_fIsFirstTime)
    {
#endif
        VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                            &msgCenter(), SLOT(sltShowHelpAboutDialog()));
#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    }
#endif

    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Contents), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_WebSite), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpWebDialog()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltResetSuppressedMessages()));
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_NetworkAccessManager), SIGNAL(triggered()),
                        gNetworkManager, SLOT(show()));
#ifdef VBOX_WITH_REGISTRATION
    VBoxGlobal::connect(gActionPool->action(UIActionIndex_Simple_Register), SIGNAL(triggered()),
                        &vboxGlobal(), SLOT(showRegistrationDialog()));
    VBoxGlobal::connect(gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)),
                        gActionPool->action(UIActionIndex_Simple_Register), SLOT(setEnabled(bool)));
#endif /* VBOX_WITH_REGISTRATION */

    m_fIsFirstTime = false;
}

#include "UIMachineMenuBar.moc"

