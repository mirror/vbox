/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsToolbar class implementation.
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
#include <QAction>
#include <QButtonGroup>
#include <QLabel>
#include <QStackedLayout>
#include <QUuid>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIActionPoolSelector.h"
#include "UITabBar.h"
#include "UIToolsToolbar.h"

/* Other VBox includes: */
#include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolsToolbar::UIToolsToolbar(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayoutMain(0)
    , m_pLayoutStacked(0)
    , m_pLayoutButton(0)
    , m_pTabBarMachine(0)
    , m_pLabel(0)
{
    /* Prepare: */
    prepare();
}

void UIToolsToolbar::sltHandleOpenToolMachine()
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
        idTab = m_pTabBarMachine->addTab(pAction->icon(), pAction->text().remove('&'));
        m_mapTabIdsMachine[enmType] = idTab;
    }
    /* And make it active: */
    m_pTabBarMachine->setCurrent(idTab);

    /* Notify listeners: */
    emit sigToolOpenedMachine(enmType);
}

void UIToolsToolbar::sltHandleCloseToolMachine(const QUuid &uuid)
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

void UIToolsToolbar::sltHandleToolChosenMachine(const QUuid &uuid)
{
    /* If the ID is registered => Notify listeners: */
    if (m_mapTabIdsMachine.values().contains(uuid))
        emit sigToolOpenedMachine(m_mapTabIdsMachine.key(uuid));
}

void UIToolsToolbar::sltHandleButtonToggle()
{
    /* Acquire the sender: */
    QIToolButton *pButton = sender() ? qobject_cast<QIToolButton*>(sender()) : 0;
    if (!pButton)
        pButton = m_mapButtons.value(m_mapButtons.keys().first());

    /* Handle known buttons: */
    if (m_pLayoutStacked && m_mapButtons.values().contains(pButton))
    {
        switch (m_mapButtons.key(pButton))
        {
            case ActionType_Machine: m_pLayoutStacked->setCurrentWidget(m_pTabBarMachine); break;
        }
    }

    /* Update label's text: */
    m_pLabel->setText(pButton->text().remove('&'));
}

void UIToolsToolbar::prepare()
{
    /* Prepare menu: */
    prepareMenu();
    /* Prepare widgets: */
    prepareWidgets();
    /* Prepare connections: */
    prepareConnections();

    /* Initialize: */
    sltHandleButtonToggle();
}

void UIToolsToolbar::prepareMenu()
{
    /* Configure 'Machine' menu: */
    UIMenu *pMenuMachine = m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine)->menu();
    AssertPtrReturnVoid(pMenuMachine);
    {
        /* Add 'Details' action: */
        pMenuMachine->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_Details));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_Details), &UIAction::triggered,
                this, &UIToolsToolbar::sltHandleOpenToolMachine);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_Details)->setProperty("ToolTypeMachine", QVariant::fromValue(ToolTypeMachine_Details));

        /* Add 'Snapshots' action: */
        pMenuMachine->addAction(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_Snapshots));
        connect(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_Snapshots), &UIAction::triggered,
                this, &UIToolsToolbar::sltHandleOpenToolMachine);
        m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_Snapshots)->setProperty("ToolTypeMachine", QVariant::fromValue(ToolTypeMachine_Snapshots));
    }
}

void UIToolsToolbar::prepareWidgets()
{
    /* Create main layout: */
    m_pLayoutMain = new QGridLayout(this);
    AssertPtrReturnVoid(m_pLayoutMain);
    {
        /* Configure layout: */
        m_pLayoutMain->setContentsMargins(0, 0, 0, 0);
        m_pLayoutMain->setHorizontalSpacing(10);
        m_pLayoutMain->setVerticalSpacing(0);

        /* Create stacked layout: */
        m_pLayoutStacked = new QStackedLayout;
        AssertPtrReturnVoid(m_pLayoutStacked);
        {
            /* Create Machine tab-bar: */
            m_pTabBarMachine = new UITabBar;
            AssertPtrReturnVoid(m_pTabBarMachine);
            {
                /* Configure tab-bar: */
                connect(m_pTabBarMachine, &UITabBar::sigTabRequestForClosing,
                        this, &UIToolsToolbar::sltHandleCloseToolMachine);
                connect(m_pTabBarMachine, &UITabBar::sigCurrentTabChanged,
                        this, &UIToolsToolbar::sltHandleToolChosenMachine);

                /* Add into layout: */
                m_pLayoutStacked->addWidget(m_pTabBarMachine);
            }

            /* Add into layout: */
            m_pLayoutMain->addLayout(m_pLayoutStacked, 0, 0);
        }

        /* Create sub-layout: */
        m_pLayoutButton = new QHBoxLayout;
        AssertPtrReturnVoid(m_pLayoutButton);
        {
            /* Create exclusive button-group: */
            QButtonGroup *pButtonGroup = new QButtonGroup(this);
            AssertPtrReturnVoid(pButtonGroup);
            {
                /* Create button 'Machine': */
                m_mapButtons[ActionType_Machine] = prepareSectionButton(m_pActionPool->action(UIActionIndexST_M_Tools_T_Machine));
                QIToolButton *pButtonMachine = m_mapButtons.value(ActionType_Machine, 0);
                AssertPtrReturnVoid(pButtonMachine);
                {
                    /* Confgure button: */
                    pButtonMachine->setMenu(m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine)->menu());

                    /* Add into button-group / layout: */
                    pButtonGroup->addButton(pButtonMachine);
                    m_pLayoutButton->addWidget(pButtonMachine);
                }
            }

            /* Add into layout: */
            m_pLayoutMain->addLayout(m_pLayoutButton, 0, 1);
        }

        /* Create label: */
        m_pLabel = new QLabel;
        AssertPtrReturnVoid(m_pLabel);
        {
            /* Configure label: */
            m_pLabel->setAlignment(Qt::AlignTop | Qt::AlignCenter);

            /* Add into layout: */
            m_pLayoutMain->addWidget(m_pLabel, 1, 1);
        }
    }
}

/* static */
QIToolButton *UIToolsToolbar::prepareSectionButton(QAction *pAction)
{
    /* Create button: */
    QIToolButton *pButton = new QIToolButton;
    AssertPtrReturn(pButton, 0);
    {
        /* Confgure button: */
        pButton->setIconSize(QSize(32, 32));
        pButton->setPopupMode(QToolButton::InstantPopup);
        pButton->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

        /* Load action state to button: */
        pButton->setIcon(pAction->icon());
        pButton->setText(pAction->text());
        pButton->setToolTip(pAction->toolTip());
        pButton->setChecked(pAction->isChecked());

        /* Link button and action: */
        connect(pButton, &QIToolButton::toggled, pAction, &QAction::setChecked);
        connect(pAction, &QAction::toggled, pButton, &QIToolButton::setChecked);
    }
    /* Return button: */
    return pButton;
}

void UIToolsToolbar::prepareConnections()
{
    /* Prepare connections: */
    if (m_mapButtons.contains(ActionType_Machine))
        connect(m_mapButtons.value(ActionType_Machine), &QIToolButton::toggled, this, &UIToolsToolbar::sltHandleButtonToggle);
}

