/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolbarTools class implementation.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QAction>
# include <QButtonGroup>
# include <QHBoxLayout>
# include <QLabel>
# include <QStyle>
# include <QToolButton>

/* GUI includes: */
# include "UIActionPoolSelector.h"
# include "UITabBar.h"
# include "UIToolBar.h"
# include "UIToolbarTools.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolbarTools::UIToolbarTools(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pTabBarMachine(0)
    , m_pTabBarGlobal(0)
    , m_pLayoutMain(0)
{
    prepare();
}

void UIToolbarTools::switchToTabBar(TabBarType enmType)
{
    /* Handle known types: */
    switch (enmType)
    {
        case TabBarType_Machine:
        {
            if (m_pTabBarGlobal)
                m_pTabBarGlobal->setVisible(false);
            if (m_pTabBarMachine)
                m_pTabBarMachine->setVisible(true);
            break;
        }
        case TabBarType_Global:
        {
            if (m_pTabBarMachine)
                m_pTabBarMachine->setVisible(false);
            if (m_pTabBarGlobal)
                m_pTabBarGlobal->setVisible(true);
            break;
        }
    }
}

void UIToolbarTools::setTabBarEnabledMachine(bool fEnabled)
{
    /* Update Machine tab-bar availability: */
    m_pTabBarMachine->setEnabled(fEnabled);
}

void UIToolbarTools::setTabBarEnabledGlobal(bool fEnabled)
{
    /* Update Global tab-bar availability: */
    m_pTabBarGlobal->setEnabled(fEnabled);
}

QList<ToolTypeMachine> UIToolbarTools::tabOrderMachine() const
{
    QList<ToolTypeMachine> list;
    foreach (const QUuid &uuid, m_pTabBarMachine->tabOrder())
        list << m_mapTabIdsMachine.key(uuid);
    return list;
}

QList<ToolTypeGlobal> UIToolbarTools::tabOrderGlobal() const
{
    QList<ToolTypeGlobal> list;
    foreach (const QUuid &uuid, m_pTabBarGlobal->tabOrder())
        list << m_mapTabIdsGlobal.key(uuid);
    return list;
}

void UIToolbarTools::sltHandleOpenToolMachine()
{
    /* Acquire sender action: */
    const QAction *pAction = sender() ? qobject_cast<const QAction*>(sender()) : 0;
    /* Acquire action type: */
    const ToolTypeMachine enmType = pAction->property("ToolTypeMachine").value<ToolTypeMachine>();

    /* Get corresponding tab id: */
    QUuid idTab;
    if (m_mapTabIdsMachine.contains(enmType))
        idTab = m_mapTabIdsMachine.value(enmType);
    else
    {
        idTab = m_pTabBarMachine->addTab(pAction);
        m_mapTabIdsMachine[enmType] = idTab;
    }
    /* And make it active: */
    m_pTabBarMachine->setCurrent(idTab);

    /* Notify listeners: */
    emit sigToolOpenedMachine(enmType);
}

void UIToolbarTools::sltHandleOpenToolGlobal()
{
    /* Acquire sender action: */
    const QAction *pAction = sender() ? qobject_cast<const QAction*>(sender()) : 0;
    /* Acquire action type: */
    const ToolTypeGlobal enmType = pAction->property("ToolTypeGlobal").value<ToolTypeGlobal>();

    /* Get corresponding tab id: */
    QUuid idTab;
    if (m_mapTabIdsGlobal.contains(enmType))
        idTab = m_mapTabIdsGlobal.value(enmType);
    else
    {
        idTab = m_pTabBarGlobal->addTab(pAction);
        m_mapTabIdsGlobal[enmType] = idTab;
    }
    /* And make it active: */
    m_pTabBarGlobal->setCurrent(idTab);

    /* Notify listeners: */
    emit sigToolOpenedGlobal(enmType);
}

void UIToolbarTools::sltHandleCloseToolMachine(const QUuid &uuid)
{
    /* If the ID is registered: */
    if (m_mapTabIdsMachine.values().contains(uuid))
    {
        /* Notify listeners: */
        emit sigToolClosedMachine(m_mapTabIdsMachine.key(uuid));

        /* And remove the tab: */
        m_pTabBarMachine->removeTab(uuid);
        m_mapTabIdsMachine.remove(m_mapTabIdsMachine.key(uuid));
    }
}

void UIToolbarTools::sltHandleCloseToolGlobal(const QUuid &uuid)
{
    /* If the ID is registered: */
    if (m_mapTabIdsGlobal.values().contains(uuid))
    {
        /* Notify listeners: */
        emit sigToolClosedGlobal(m_mapTabIdsGlobal.key(uuid));

        /* And remove the tab: */
        m_pTabBarGlobal->removeTab(uuid);
        m_mapTabIdsGlobal.remove(m_mapTabIdsGlobal.key(uuid));
    }
}

void UIToolbarTools::sltHandleToolChosenMachine(const QUuid &uuid)
{
    /* If the ID is registered => Notify listeners: */
    if (m_mapTabIdsMachine.values().contains(uuid))
        emit sigToolOpenedMachine(m_mapTabIdsMachine.key(uuid));
}

void UIToolbarTools::sltHandleToolChosenGlobal(const QUuid &uuid)
{
    /* If the ID is registered => Notify listeners: */
    if (m_mapTabIdsGlobal.values().contains(uuid))
        emit sigToolOpenedGlobal(m_mapTabIdsGlobal.key(uuid));
}

void UIToolbarTools::prepare()
{
    /* Prepare menu: */
    prepareMenu();
    /* Prepare widgets: */
    prepareWidgets();
}

void UIToolbarTools::prepareMenu()
{
    /* Configure 'Machine' menu: */
    UIMenu *pMenuMachine = m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine)->menu();
    if (pMenuMachine)
    {
        /* Add 'Details' action: */
        pMenuMachine->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Details));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Details), &UIAction::triggered,
                this, &UIToolbarTools::sltHandleOpenToolMachine);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Details)
            ->setProperty("ToolTypeMachine", QVariant::fromValue(ToolTypeMachine_Details));

        /* Add 'Snapshots' action: */
        pMenuMachine->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots), &UIAction::triggered,
                this, &UIToolbarTools::sltHandleOpenToolMachine);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots)
            ->setProperty("ToolTypeMachine", QVariant::fromValue(ToolTypeMachine_Snapshots));

        /* Add 'LogViewer' action: */
        pMenuMachine->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer), &UIAction::triggered,
                this, &UIToolbarTools::sltHandleOpenToolMachine);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer)
            ->setProperty("ToolTypeMachine", QVariant::fromValue(ToolTypeMachine_LogViewer));
    }

    /* Configure 'Machine' toggle action: */
    m_pActionPool->action(UIActionIndexST_M_Tools_T_Machine)->setMenu(pMenuMachine);

    /* Configure 'Global' menu: */
    UIMenu *pMenuGlobal = m_pActionPool->action(UIActionIndexST_M_Tools_M_Global)->menu();
    if (pMenuGlobal)
    {
        /* Add 'Virtual Media Manager' action: */
        pMenuGlobal->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager), &UIAction::triggered,
                this, &UIToolbarTools::sltHandleOpenToolGlobal);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager)
            ->setProperty("ToolTypeGlobal", QVariant::fromValue(ToolTypeGlobal_VirtualMedia));

        /* Add 'Host Network Manager' action: */
        pMenuGlobal->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager), &UIAction::triggered,
                this, &UIToolbarTools::sltHandleOpenToolGlobal);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager)
            ->setProperty("ToolTypeGlobal", QVariant::fromValue(ToolTypeGlobal_HostNetwork));
    }

    /* Configure 'Global' toggle action: */
    m_pActionPool->action(UIActionIndexST_M_Tools_T_Global)->setMenu(pMenuGlobal);
}

void UIToolbarTools::prepareWidgets()
{
    /* Create main layout: */
    m_pLayoutMain = new QHBoxLayout(this);
    if (m_pLayoutMain)
    {
        /* Configure layout: */
        m_pLayoutMain->setContentsMargins(0, 0, 0, 0);

        /* Create Machine tab-bar: */
        m_pTabBarMachine = new UITabBar(UITabBar::Align_Left);
        if (m_pTabBarMachine)
        {
            connect(m_pTabBarMachine, &UITabBar::sigTabRequestForClosing,
                    this, &UIToolbarTools::sltHandleCloseToolMachine);
            connect(m_pTabBarMachine, &UITabBar::sigCurrentTabChanged,
                    this, &UIToolbarTools::sltHandleToolChosenMachine);

            /* Add into layout: */
            m_pLayoutMain->addWidget(m_pTabBarMachine);
        }

        /* Create Global tab-bar: */
        m_pTabBarGlobal = new UITabBar(UITabBar::Align_Left);
        if (m_pTabBarGlobal)
        {
            connect(m_pTabBarGlobal, &UITabBar::sigTabRequestForClosing,
                    this, &UIToolbarTools::sltHandleCloseToolGlobal);
            connect(m_pTabBarGlobal, &UITabBar::sigCurrentTabChanged,
                    this, &UIToolbarTools::sltHandleToolChosenGlobal);

            /* Add into layout: */
            m_pLayoutMain->addWidget(m_pTabBarGlobal);
        }

        /* Let the tab-bars know our opinion: */
        switchToTabBar(TabBarType_Machine);
    }
}
