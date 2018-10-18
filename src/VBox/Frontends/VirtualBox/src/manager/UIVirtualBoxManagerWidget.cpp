/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManagerWidget class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
# include <QHBoxLayout>
# include <QStackedWidget>
# include <QStyle>
# include <QVBoxLayout>

/* GUI includes: */
# include "QISplitter.h"
# include "UIActionPoolSelector.h"
# include "UIErrorString.h"
# include "UIExtraDataManager.h"
# include "UIChooser.h"
# include "UIVirtualBoxManager.h"
# include "UIVirtualBoxManagerWidget.h"
# include "UITabBar.h"
# include "UIToolBar.h"
# include "UIVirtualMachineItem.h"
# include "UITools.h"
# ifndef VBOX_WS_MAC
#  include "UIMenuBar.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVirtualBoxManagerWidget::UIVirtualBoxManagerWidget(UIVirtualBoxManager *pParent)
    : m_pActionPool(pParent->actionPool())
    , m_pSplitter(0)
    , m_pToolBar(0)
    , m_pPaneChooser(0)
    , m_pStackedWidget(0)
    , m_pPaneToolsGlobal(0)
    , m_pPaneToolsMachine(0)
    , m_pSlidingAnimation(0)
    , m_pPaneTools(0)
{
    prepare();
}

UIVirtualBoxManagerWidget::~UIVirtualBoxManagerWidget()
{
    cleanup();
}

bool UIVirtualBoxManagerWidget::isGroupItemSelected() const
{
    return m_pPaneChooser->isGroupItemSelected();
}

bool UIVirtualBoxManagerWidget::isGlobalItemSelected() const
{
    return m_pPaneChooser->isGlobalItemSelected();
}

bool UIVirtualBoxManagerWidget::isMachineItemSelected() const
{
    return m_pPaneChooser->isMachineItemSelected();
}

UIVirtualMachineItem *UIVirtualBoxManagerWidget::currentItem() const
{
    return m_pPaneChooser->currentItem();
}

QList<UIVirtualMachineItem*> UIVirtualBoxManagerWidget::currentItems() const
{
    return m_pPaneChooser->currentItems();
}

bool UIVirtualBoxManagerWidget::isGroupSavingInProgress() const
{
    return m_pPaneChooser->isGroupSavingInProgress();
}

bool UIVirtualBoxManagerWidget::isAllItemsOfOneGroupSelected() const
{
    return m_pPaneChooser->isAllItemsOfOneGroupSelected();
}

bool UIVirtualBoxManagerWidget::isSingleGroupSelected() const
{
    return m_pPaneChooser->isSingleGroupSelected();
}

ToolTypeMachine UIVirtualBoxManagerWidget::currentMachineTool() const
{
    return m_pPaneToolsMachine->currentTool();
}

ToolTypeGlobal UIVirtualBoxManagerWidget::currentGlobalTool() const
{
    return m_pPaneToolsGlobal->currentTool();
}

bool UIVirtualBoxManagerWidget::isToolOpened(ToolTypeMachine enmType) const
{
    return m_pPaneToolsMachine ? m_pPaneToolsMachine->isToolOpened(enmType) : false;
}

bool UIVirtualBoxManagerWidget::isToolOpened(ToolTypeGlobal enmType) const
{
    return m_pPaneToolsGlobal ? m_pPaneToolsGlobal->isToolOpened(enmType) : false;
}

void UIVirtualBoxManagerWidget::switchToTool(ToolTypeMachine enmType)
{
    /* Open corresponding tool: */
    m_pPaneToolsMachine->openTool(enmType);

    /* If that was 'Details' => pass there current items: */
    if (   enmType == ToolTypeMachine_Details
        && m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
        m_pPaneToolsMachine->setItems(currentItems());
    /* If that was 'Snapshot' or 'LogViewer' => pass there current or null machine: */
    if (enmType == ToolTypeMachine_Snapshots || enmType == ToolTypeMachine_Logs)
    {
        UIVirtualMachineItem *pItem = currentItem();
        m_pPaneToolsMachine->setMachine(pItem ? pItem->machine() : CMachine());
    }

    /* Let the parent know: */
    emit sigToolTypeChange();

    /* Update toolbar: */
    updateToolbar();
}

void UIVirtualBoxManagerWidget::switchToTool(ToolTypeGlobal enmType)
{
    /* Open corresponding tool: */
    m_pPaneToolsGlobal->openTool(enmType);

    /* Let the parent know: */
    emit sigToolTypeChange();

    /* Update toolbar: */
    updateToolbar();
}

void UIVirtualBoxManagerWidget::sltHandleContextMenuRequest(const QPoint &position)
{
    /* Populate toolbar actions: */
    QList<QAction*> actions;
    /* Add 'Show Toolbar Text' action: */
    QAction *pShowToolBarText = new QAction(tr("Show Toolbar Text"), 0);
    AssertPtrReturnVoid(pShowToolBarText);
    {
        /* Configure action: */
        pShowToolBarText->setCheckable(true);
        pShowToolBarText->setChecked(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);

        /* Add into action list: */
        actions << pShowToolBarText;
    }

    /* Prepare the menu position: */
    QPoint globalPosition = position;
    QWidget *pSender = static_cast<QWidget*>(sender());
    if (pSender)
        globalPosition = pSender->mapToGlobal(position);

    /* Execute the menu: */
    QAction *pResult = QMenu::exec(actions, globalPosition);

    /* Handle the menu execution result: */
    if (pResult == pShowToolBarText)
    {
        m_pToolBar->setToolButtonStyle(  pResult->isChecked()
                                       ? Qt::ToolButtonTextUnderIcon
                                       : Qt::ToolButtonIconOnly);
    }
}

void UIVirtualBoxManagerWidget::retranslateUi()
{
    /* Make sure chosen item fetched: */
    sltHandleChooserPaneIndexChange(false /* update details? */, false /* update snapshots? */, false /* update the logviewer? */);

#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    m_pToolBar->updateLayout();
#endif
}

void UIVirtualBoxManagerWidget::sltHandleChooserPaneIndexChange(bool fUpdateDetails /* = true */,
                                                                bool fUpdateSnapshots /* = true */,
                                                                bool fUpdateLogViewer /* = true */)
{
    /* Let the parent know: */
    emit sigChooserPaneIndexChange();

    /* If global item is selected and we are on machine tools pane => switch to global tools pane: */
    if (   isGlobalItemSelected()
        && m_pStackedWidget->currentWidget() != m_pPaneToolsGlobal)
    {
        /* Just start animation and return, do nothing else.. */
        m_pStackedWidget->setCurrentWidget(m_pPaneToolsGlobal); // rendering w/a
        m_pStackedWidget->setCurrentWidget(m_pSlidingAnimation);
        m_pSlidingAnimation->animate(SlidingDirection_Reverse);
        return;
    }

    else

    /* If machine or group item is selected and we are on global tools pane => switch to machine tools pane: */
    if (   (isMachineItemSelected() || isGroupItemSelected())
             && m_pStackedWidget->currentWidget() != m_pPaneToolsMachine)
    {
        /* Just start animation and return, do nothing else.. */
        m_pStackedWidget->setCurrentWidget(m_pPaneToolsMachine); // rendering w/a
        m_pStackedWidget->setCurrentWidget(m_pSlidingAnimation);
        m_pSlidingAnimation->animate(SlidingDirection_Forward);
        return;
    }

    /* If that was machine or group item selected: */
    if (isMachineItemSelected() || isGroupItemSelected())
    {
        /* Get current item: */
        UIVirtualMachineItem *pItem = currentItem();

        /* Update Tools-pane: */
        m_pPaneToolsMachine->setCurrentItem(pItem);

        /* Update Machine Tools availability: */
        m_pPaneTools->setToolsEnabled(UIToolsClass_Machine, pItem && pItem->accessible());

        /* If current item exists & accessible: */
        if (pItem && pItem->accessible())
        {
            /* If Error pane is chosen currently => open tool currently chosen in Tools-pane: */
            if (m_pPaneToolsMachine->currentTool() == ToolTypeMachine_Error)
                sltHandleToolsPaneIndexChange();

            /* Update Details-pane (if requested): */
            if (   fUpdateDetails
                && m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
                m_pPaneToolsMachine->setItems(currentItems());
            /* Update the Snapshots-pane or/and Logviewer-pane (if requested): */
            if (fUpdateSnapshots || fUpdateLogViewer)
                m_pPaneToolsMachine->setMachine(pItem->machine());
        }
        else
        {
            /* Make sure Error pane raised: */
            m_pPaneToolsMachine->openTool(ToolTypeMachine_Error);

            /* Note that the machine becomes inaccessible (or if the last VM gets
             * deleted), we have to update all fields, ignoring input arguments. */
            if (pItem)
            {
                /* The VM is inaccessible: */
                m_pPaneToolsMachine->setErrorDetails(UIErrorString::formatErrorInfo(pItem->accessError()));
            }

            /* Update Details-pane (in any case): */
            if (m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
                m_pPaneToolsMachine->setItems(currentItems());
            /* Update Snapshots-pane and Logviewer-pane (in any case): */
            m_pPaneToolsMachine->setMachine(CMachine());
        }
    }
}

void UIVirtualBoxManagerWidget::sltHandleSlidingAnimationComplete(SlidingDirection enmDirection)
{
    /* First switch the panes: */
    switch (enmDirection)
    {
        case SlidingDirection_Forward:
        {
            m_pPaneTools->setToolsClass(UIToolsClass_Machine);
            m_pStackedWidget->setCurrentWidget(m_pPaneToolsMachine);
            break;
        }
        case SlidingDirection_Reverse:
        {
            m_pPaneTools->setToolsClass(UIToolsClass_Global);
            m_pStackedWidget->setCurrentWidget(m_pPaneToolsGlobal);
            break;
        }
    }
    /* Then handle current item change (again!): */
    sltHandleChooserPaneIndexChangeDefault();
}

void UIVirtualBoxManagerWidget::sltHandleToolMenuRequested(UIToolsClass enmClass, const QPoint &position)
{
    /* Define current tools class: */
    m_pPaneTools->setToolsClass(enmClass);

    /* Move, resize and show: */
    m_pPaneTools->move(position);
    m_pPaneTools->show();
    // WORKAROUND:
    // Don't want even to think why, but for Qt::Popup resize to
    // smaller size often being ignored until it is actually shown.
    m_pPaneTools->resize(m_pPaneTools->minimumSizeHint());
}

void UIVirtualBoxManagerWidget::sltHandleToolsPaneIndexChange()
{
    switch (m_pPaneTools->toolsClass())
    {
        case UIToolsClass_Global:
        {
            ToolTypeGlobal enmType = ToolTypeGlobal_Invalid;
            if (!m_pPaneTools->areToolsEnabled(UIToolsClass_Global))
                enmType = ToolTypeGlobal_Welcome;
            else
            {
                switch (m_pPaneTools->toolsType())
                {
                    case UIToolsType_Welcome: enmType = ToolTypeGlobal_Welcome; break;
                    case UIToolsType_Media:   enmType = ToolTypeGlobal_Media; break;
                    case UIToolsType_Network: enmType = ToolTypeGlobal_Network; break;
                    case UIToolsType_Cloud:   enmType = ToolTypeGlobal_Cloud; break;
                    default: break;
                }
            }
            if (enmType != ToolTypeGlobal_Invalid)
                switchToTool(enmType);
            break;
        }
        case UIToolsClass_Machine:
        {
            ToolTypeMachine enmType = ToolTypeMachine_Invalid;
            if (!m_pPaneTools->areToolsEnabled(UIToolsClass_Machine))
                enmType = ToolTypeMachine_Details;
            else
            {
                switch (m_pPaneTools->toolsType())
                {
                    case UIToolsType_Details:   enmType = ToolTypeMachine_Details; break;
                    case UIToolsType_Snapshots: enmType = ToolTypeMachine_Snapshots; break;
                    case UIToolsType_Logs:      enmType = ToolTypeMachine_Logs; break;
                    default: break;
                }
            }
            if (enmType != ToolTypeMachine_Invalid)
                switchToTool(enmType);
            break;
        }
        default:
            break;
    }
}

void UIVirtualBoxManagerWidget::prepare()
{
    /* Configure palette: */
    setAutoFillBackground(true);
    QPalette pal = palette();
#ifdef VBOX_WS_MAC
    const QColor color = pal.color(QPalette::Active, QPalette::Mid).lighter(145);
#else
    const QColor color = pal.color(QPalette::Active, QPalette::Mid).lighter(160);
#endif
    pal.setColor(QPalette::Window, color);
    setPalette(pal);

    /* Prepare: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    retranslateUi();

    /* Make sure current Chooser-pane index fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIVirtualBoxManagerWidget::prepareWidgets()
{
    /* Create main-layout: */
    QHBoxLayout *pLayoutMain = new QHBoxLayout(this);
    if (pLayoutMain)
    {
        /* Configure layout: */
        pLayoutMain->setSpacing(0);
        pLayoutMain->setContentsMargins(0, 0, 0, 0);

        /* Create splitter: */
        m_pSplitter = new QISplitter(Qt::Horizontal, QISplitter::Flat);
        if (m_pSplitter)
        {
            /* Configure splitter: */
            m_pSplitter->setHandleWidth(1);

            /* Create Chooser-pane: */
            m_pPaneChooser = new UIChooser(this);
            if (m_pPaneChooser)
            {
                /* Add into splitter: */
                m_pSplitter->addWidget(m_pPaneChooser);
            }

            /* Create right widget: */
            QWidget *pWidgetRight = new QWidget;
            if (pWidgetRight)
            {
                /* Create right-layout: */
                QVBoxLayout *pLayoutRight = new QVBoxLayout(pWidgetRight);
                if(pLayoutRight)
                {
                    /* Configure layout: */
                    pLayoutRight->setSpacing(0);
                    pLayoutRight->setContentsMargins(0, 0, 0, 0);

                    /* Create Main toolbar: */
                    m_pToolBar = new UIToolBar;
                    if (m_pToolBar)
                    {
                        /* Configure toolbar: */
                        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
                        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
                        m_pToolBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                        m_pToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
                        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
#ifdef VBOX_WS_MAC
                        m_pToolBar->emulateMacToolbar();
#endif

                        /* Add tool-bar into layout: */
                        pLayoutRight->addWidget(m_pToolBar);
                    }

                    /* Create stacked-widget: */
                    m_pStackedWidget = new QStackedWidget;
                    if (m_pStackedWidget)
                    {
                        /* Create Global Tools-pane: */
                        m_pPaneToolsGlobal = new UIToolPaneGlobal(actionPool());
                        if (m_pPaneToolsGlobal)
                        {
                            /* Add into stack: */
                            m_pStackedWidget->addWidget(m_pPaneToolsGlobal);
                        }

                        /* Create Machine Tools-pane: */
                        m_pPaneToolsMachine = new UIToolPaneMachine(actionPool());
                        if (m_pPaneToolsMachine)
                        {
                            /* Add into stack: */
                            m_pStackedWidget->addWidget(m_pPaneToolsMachine);
                        }

                        /* Create sliding-widget: */
                        // Reverse initial animation direction if group or machine selected!
                        const bool fReverse = !m_pPaneChooser->isGlobalItemSelected();
                        m_pSlidingAnimation = new UISlidingAnimation(Qt::Vertical, fReverse);
                        if (m_pSlidingAnimation)
                        {
                            /* Add first/second widgets into sliding animation: */
                            m_pSlidingAnimation->setWidgets(m_pPaneToolsGlobal, m_pPaneToolsMachine);
                            connect(m_pSlidingAnimation, &UISlidingAnimation::sigAnimationComplete,
                                    this, &UIVirtualBoxManagerWidget::sltHandleSlidingAnimationComplete);

                            /* Add into stack: */
                            m_pStackedWidget->addWidget(m_pSlidingAnimation);
                        }

                        /* Choose which pane should be active initially: */
                        if (m_pPaneChooser->isGlobalItemSelected())
                            m_pStackedWidget->setCurrentWidget(m_pPaneToolsGlobal);
                        else
                            m_pStackedWidget->setCurrentWidget(m_pPaneToolsMachine);

                        /* Add into layout: */
                        pLayoutRight->addWidget(m_pStackedWidget, 1);
                    }
                }

                /* Add into splitter: */
                m_pSplitter->addWidget(pWidgetRight);
            }

            /* Adjust splitter colors according to main widgets it splits: */
            m_pSplitter->configureColor(palette().color(QPalette::Active, QPalette::Midlight).darker(110));
            /* Set the initial distribution. The right site is bigger. */
            m_pSplitter->setStretchFactor(0, 2);
            m_pSplitter->setStretchFactor(1, 3);

            /* Add into layout: */
            pLayoutMain->addWidget(m_pSplitter);
        }

        /* Create Tools-pane: */
        m_pPaneTools = new UITools(this);
        if (m_pPaneTools)
        {
            /* Choose which pane should be active initially: */
            if (m_pPaneChooser->isGlobalItemSelected())
                m_pPaneTools->setToolsClass(UIToolsClass_Global);
            else
                m_pPaneTools->setToolsClass(UIToolsClass_Machine);
        }
    }

    /* Update toolbar finally: */
    updateToolbar();

    /* Bring the VM list to the focus: */
    m_pPaneChooser->setFocus();
}

void UIVirtualBoxManagerWidget::prepareConnections()
{
    /* Tool-bar connections: */
    connect(m_pToolBar, &UIToolBar::sigResized,
            m_pPaneChooser, &UIChooser::sltHandleToolbarResize);
    connect(m_pToolBar, &UIToolBar::customContextMenuRequested,
            this, &UIVirtualBoxManagerWidget::sltHandleContextMenuRequest);

    /* Chooser-pane connections: */
    connect(m_pPaneChooser, &UIChooser::sigSelectionChanged,
            this, &UIVirtualBoxManagerWidget::sltHandleChooserPaneIndexChangeDefault);
    connect(m_pPaneChooser, &UIChooser::sigSlidingStarted,
            m_pPaneToolsMachine, &UIToolPaneMachine::sigSlidingStarted);
    connect(m_pPaneChooser, &UIChooser::sigToggleStarted,
            m_pPaneToolsMachine, &UIToolPaneMachine::sigToggleStarted);
    connect(m_pPaneChooser, &UIChooser::sigToggleFinished,
            m_pPaneToolsMachine, &UIToolPaneMachine::sigToggleFinished);
    connect(m_pPaneChooser, &UIChooser::sigGroupSavingStateChanged,
            this, &UIVirtualBoxManagerWidget::sigGroupSavingStateChanged);
    connect(m_pPaneChooser, &UIChooser::sigToolMenuRequested,
            this, &UIVirtualBoxManagerWidget::sltHandleToolMenuRequested);

    /* Details-pane connections: */
    connect(m_pPaneToolsMachine, &UIToolPaneMachine::sigLinkClicked,
            this, &UIVirtualBoxManagerWidget::sigMachineSettingsLinkClicked);

    /* Tools-pane connections: */
    connect(m_pPaneTools, &UITools::sigSelectionChanged,
            this, &UIVirtualBoxManagerWidget::sltHandleToolsPaneIndexChange);
}

void UIVirtualBoxManagerWidget::loadSettings()
{
    /* Restore splitter handle position: */
    {
        /* Read splitter hints: */
        QList<int> sizes = gEDataManager->selectorWindowSplitterHints();
        /* If both hints are zero, we have the 'default' case: */
        if (sizes[0] == 0 && sizes[1] == 0)
        {
            /* Propose some 'default' based on current dialog width: */
            sizes[0] = (int)(width() * .9 * (1.0 / 3));
            sizes[1] = (int)(width() * .9 * (2.0 / 3));
        }
        /* Pass hints to the splitter: */
        m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar settings: */
    {
        m_pToolBar->setToolButtonStyle(  gEDataManager->selectorWindowToolBarTextVisible()
                                       ? Qt::ToolButtonTextUnderIcon
                                       : Qt::ToolButtonIconOnly);
    }

    /* Open tool currently chosen in Tools-pane:  */
    sltHandleToolsPaneIndexChange();
}

void UIVirtualBoxManagerWidget::updateToolbar()
{
    /* Make sure toolbar exists: */
    AssertPtrReturnVoid(m_pToolBar);

    /* Clear initially: */
    m_pToolBar->clear();

    /* Basic action set: */
    switch (m_pPaneTools->toolsClass())
    {
        /* Global toolbar: */
        case UIToolsClass_Global:
        {
            switch (currentGlobalTool())
            {
                case ToolTypeGlobal_Welcome:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Add));
                    break;
                }
                case ToolTypeGlobal_Media:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_S_Add));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_S_Copy));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_S_Move));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_S_Remove));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_S_Release));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_T_Details));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Medium_S_Refresh));
                    break;
                }
                case ToolTypeGlobal_Network:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Network_S_Create));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Network_S_Remove));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Network_T_Details));
                    //m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Network_S_Refresh));
                    break;
                }
                case ToolTypeGlobal_Cloud:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Cloud_S_Add));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Cloud_S_Remove));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Cloud_T_Details));
                    //m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Cloud_S_Refresh));
                    break;
                }
                default:
                    break;
            }
            break;
        }
        /* Machine toolbar: */
        case UIToolsClass_Machine:
        {
            switch (currentMachineTool())
            {
                case ToolTypeMachine_Details:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
                    break;
                }
                case ToolTypeMachine_Snapshots:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Snapshot_S_Take));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Snapshot_S_Delete));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Snapshot_S_Restore));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Snapshot_T_Properties));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Snapshot_S_Clone));
                    break;
                }
                case ToolTypeMachine_Logs:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Save));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Find));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Filter));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Bookmark));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Refresh));
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // Actually Qt should do that itself but by some unknown reason it sometimes
    // forget to update toolbar after changing its actions on Cocoa platform.
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_New), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Discard), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));

    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    m_pToolBar->updateLayout();
#endif /* VBOX_WS_MAC */
}

void UIVirtualBoxManagerWidget::saveSettings()
{
    /* Save toolbar visibility: */
    {
        gEDataManager->setSelectorWindowToolBarVisible(!m_pToolBar->isHidden());
        gEDataManager->setSelectorWindowToolBarTextVisible(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);
    }

    /* Save splitter handle position: */
    {
        gEDataManager->setSelectorWindowSplitterHints(m_pSplitter->sizes());
    }
}

void UIVirtualBoxManagerWidget::cleanup()
{
    /* Save settings: */
    saveSettings();
}
