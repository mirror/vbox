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

/* Local includes */
#include "UIMachineMenuBar.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIExtraDataEventHandler.h"
#include "UIImageTools.h"

/* Global includes */
#include <QMenuBar>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>

/* Helper QMenu reimplementation which allows
 * to highlight first menu item for popped up menu: */
class UIMenu : public QMenu
{
    Q_OBJECT;

public:

    UIMenu() : QMenu(0) {}

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

QMenu* UIMachineMenuBar::createMenu(UIActionsPool *pActionsPool, UIMainMenuType fOptions /* = UIMainMenuType_All */)
{
    /* Create empty menu: */
    QMenu *pMenu = new UIMenu;

    /* Fill menu with prepared items: */
    foreach (QMenu *pSubMenu, prepareSubMenus(pActionsPool, fOptions))
        pMenu->addMenu(pSubMenu);

    /* Return filled menu: */
    return pMenu;
}

QMenuBar* UIMachineMenuBar::createMenuBar(UIActionsPool *pActionsPool, UIMainMenuType fOptions /* = UIMainMenuType_All */)
{
    /* Create empty menubar: */
    QMenuBar *pMenuBar = new UIMenuBar;

    /* Fill menubar with prepared items: */
    foreach (QMenu *pSubMenu, prepareSubMenus(pActionsPool, fOptions))
        pMenuBar->addMenu(pSubMenu);

    /* Return filled menubar: */
    return pMenuBar;
}

QList<QMenu*> UIMachineMenuBar::prepareSubMenus(UIActionsPool *pActionsPool, UIMainMenuType fOptions /* = UIMainMenuType_All */)
{
    /* Create empty submenu list: */
    QList<QMenu*> preparedSubMenus;

    /* Machine submenu: */
    if (fOptions & UIMainMenuType_Machine)
    {
        QMenu *pMenuMachine = pActionsPool->action(UIActionIndex_Menu_Machine)->menu();
        prepareMenuMachine(pMenuMachine, pActionsPool);
        preparedSubMenus << pMenuMachine;
    }

    /* View submenu: */
    if (fOptions & UIMainMenuType_View)
    {
        QMenu *pMenuView = pActionsPool->action(UIActionIndex_Menu_View)->menu();
        prepareMenuView(pMenuView, pActionsPool);
        preparedSubMenus << pMenuView;
    }

    /* Devices submenu: */
    if (fOptions & UIMainMenuType_Devices)
    {
        QMenu *pMenuDevices = pActionsPool->action(UIActionIndex_Menu_Devices)->menu();
        prepareMenuDevices(pMenuDevices, pActionsPool);
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
            QMenu *pMenuDebug = pActionsPool->action(UIActionIndex_Menu_Debug)->menu();
            prepareMenuDebug(pMenuDebug, pActionsPool);
            preparedSubMenus << pMenuDebug;
        }
    }
#endif

    /* Help submenu: */
    if (fOptions & UIMainMenuType_Help)
    {
        QMenu *pMenuHelp = pActionsPool->action(UIActionIndex_Menu_Help)->menu();
        prepareMenuHelp(pMenuHelp, pActionsPool);
        preparedSubMenus << pMenuHelp;
    }

    /* Return a list of prepared submenus: */
    return preparedSubMenus;
}

void UIMachineMenuBar::prepareMenuMachine(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Machine submenu: */
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_SettingsDialog));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_TakeSnapshot));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_InformationDialog));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_MouseIntegration));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_TypeCAD));
#ifdef Q_WS_X11
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_TypeCABS));
#endif
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Pause));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Reset));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Shutdown));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Close));
}

void UIMachineMenuBar::prepareMenuView(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* View submenu: */
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Fullscreen));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Seamless));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Scale));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_GuestAutoresize));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_AdjustWindow));
}

void UIMachineMenuBar::prepareMenuDevices(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Devices submenu: */
    pMenu->addMenu(pActionsPool->action(UIActionIndex_Menu_OpticalDevices)->menu());
    pMenu->addMenu(pActionsPool->action(UIActionIndex_Menu_FloppyDevices)->menu());
    pMenu->addMenu(pActionsPool->action(UIActionIndex_Menu_USBDevices)->menu());
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_NetworkAdaptersDialog));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_SharedFoldersDialog));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_VRDEServer));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_InstallGuestTools));
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMachineMenuBar::prepareMenuDebug(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Debug submenu: */
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Statistics));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_CommandLine));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Toggle_Logging));
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMachineMenuBar::prepareMenuHelp(QMenu *pMenu, UIActionsPool *pActionsPool)
{
    /* Do not prepare if ready: */
    if (!pMenu->isEmpty())
        return;

    /* Help submenu: */
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Help));
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Web));
    pMenu->addSeparator();
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_ResetWarnings));
    pMenu->addSeparator();

#ifdef VBOX_WITH_REGISTRATION
    pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Register));
#endif

#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    if (m_fIsFirstTime)
# endif
        pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_Update));
#ifndef Q_WS_MAC
    pMenu->addSeparator();
#endif /* !Q_WS_MAC */
#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    if (m_fIsFirstTime)
# endif
        pMenu->addAction(pActionsPool->action(UIActionIndex_Simple_About));


#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    /* Because this connections are done to VBoxGlobal, they are needed once only.
     * Otherwise we will get the slots called more than once. */
    if (m_fIsFirstTime)
    {
#endif
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_About), SIGNAL(triggered()),
                            &msgCenter(), SLOT(sltShowHelpAboutDialog()));
        VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Update), SIGNAL(triggered()),
                            &vboxGlobal(), SLOT(showUpdateDialog()));
#if defined(Q_WS_MAC) && (QT_VERSION < 0x040700)
    }
#endif

    VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Help), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpHelpDialog()));
    VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Web), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltShowHelpWebDialog()));
    VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_ResetWarnings), SIGNAL(triggered()),
                        &msgCenter(), SLOT(sltResetSuppressedMessages()));
#ifdef VBOX_WITH_REGISTRATION
    VBoxGlobal::connect(pActionsPool->action(UIActionIndex_Simple_Register), SIGNAL(triggered()),
                        &vboxGlobal(), SLOT(showRegistrationDialog()));
    VBoxGlobal::connect(gEDataEvents, SIGNAL(sigCanShowRegistrationDlg(bool)),
                        pActionsPool->action(UIActionIndex_Simple_Register), SLOT(setEnabled(bool)));
#endif /* VBOX_WITH_REGISTRATION */

    m_fIsFirstTime = false;
}

#include "UIMachineMenuBar.moc"

