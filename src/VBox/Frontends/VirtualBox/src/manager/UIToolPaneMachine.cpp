/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneMachine class implementation.
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
# include <QStackedLayout>
# ifndef VBOX_WS_MAC
#  include <QStyle>
# endif
# include <QUuid>

/* GUI includes */
# include "UIActionPoolSelector.h"
# include "UIWelcomePane.h"
# include "UIDetails.h"
# include "UIIconPool.h"
# include "UISnapshotPane.h"
# include "UIToolPaneMachine.h"
# include "UIVirtualMachineItem.h"
# include "UIVMLogViewerWidget.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolPaneMachine::UIToolPaneMachine(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pActionPool(pActionPool)
    , m_pItem(0)
    , m_pLayout(0)
    , m_pPaneDesktop(0)
    , m_pPaneDetails(0)
    , m_pPaneSnapshots(0)
    , m_pPaneLogViewer(0)
{
    /* Prepare: */
    prepare();
}

UIToolPaneMachine::~UIToolPaneMachine()
{
    /* Cleanup: */
    cleanup();
}

ToolTypeMachine UIToolPaneMachine::currentTool() const
{
    return m_pLayout->currentWidget()->property("ToolType").value<ToolTypeMachine>();
}

bool UIToolPaneMachine::isToolOpened(ToolTypeMachine enmType) const
{
    /* Search through the stacked widgets: */
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeMachine>() == enmType)
            return true;
    return false;
}

void UIToolPaneMachine::openTool(ToolTypeMachine enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeMachine>() == enmType)
            iActualIndex = iIndex;

    /* If widget with such type exists: */
    if (iActualIndex != -1)
    {
        /* Activate corresponding index: */
        m_pLayout->setCurrentIndex(iActualIndex);
    }
    /* Otherwise: */
    else
    {
        /* Create, remember, append corresponding stacked widget: */
        switch (enmType)
        {
            case ToolTypeMachine_Desktop:
            {
                /* Create Desktop pane: */
                m_pPaneDesktop = new UIWelcomePane(m_pActionPool->action(UIActionIndexST_M_Group_S_Refresh));
                AssertPtrReturnVoid(m_pPaneDesktop);
                {
                    /* Configure pane: */
                    m_pPaneDesktop->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Desktop));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneDesktop);
                    m_pLayout->setCurrentWidget(m_pPaneDesktop);
                }
                break;
            }
            case ToolTypeMachine_Details:
            {
                /* Create Details pane: */
                m_pPaneDetails = new UIDetails;
                AssertPtrReturnVoid(m_pPaneDetails);
                {
                    /* Configure pane: */
                    m_pPaneDetails->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Details));
                    connect(this, &UIToolPaneMachine::sigSlidingStarted, m_pPaneDetails, &UIDetails::sigSlidingStarted);
                    connect(this, &UIToolPaneMachine::sigToggleStarted,  m_pPaneDetails, &UIDetails::sigToggleStarted);
                    connect(this, &UIToolPaneMachine::sigToggleFinished, m_pPaneDetails, &UIDetails::sigToggleFinished);
                    connect(m_pPaneDetails, &UIDetails::sigLinkClicked,  this, &UIToolPaneMachine::sigLinkClicked);

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneDetails);
                    m_pLayout->setCurrentWidget(m_pPaneDetails);
                }
                break;
            }
            case ToolTypeMachine_Snapshots:
            {
                /* Create Snapshots pane: */
                m_pPaneSnapshots = new UISnapshotPane;
                AssertPtrReturnVoid(m_pPaneSnapshots);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneSnapshots->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneSnapshots->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Snapshots));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneSnapshots);
                    m_pLayout->setCurrentWidget(m_pPaneSnapshots);
                }
                break;
            }
            case ToolTypeMachine_LogViewer:
            {
                /* Create the Logviewer pane: */
                m_pPaneLogViewer = new UIVMLogViewerWidget(EmbedTo_Stack);
                AssertPtrReturnVoid(m_pPaneLogViewer);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneLogViewer->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneLogViewer->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_LogViewer));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneLogViewer);
                    m_pLayout->setCurrentWidget(m_pPaneLogViewer);
                }
                break;
            }
            default:
                AssertFailedReturnVoid();
        }
    }
}

void UIToolPaneMachine::closeTool(ToolTypeMachine enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeMachine>() == enmType)
            iActualIndex = iIndex;

    /* If widget with such type doesn't exist: */
    if (iActualIndex != -1)
    {
        /* Forget corresponding widget: */
        switch (enmType)
        {
            case ToolTypeMachine_Desktop:   m_pPaneDesktop = 0; break;
            case ToolTypeMachine_Details:   m_pPaneDetails = 0; break;
            case ToolTypeMachine_Snapshots: m_pPaneSnapshots = 0; break;
            case ToolTypeMachine_LogViewer: m_pPaneLogViewer = 0; break;
            default: break;
        }
        /* Delete corresponding widget: */
        QWidget *pWidget = m_pLayout->widget(iActualIndex);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolPaneMachine::setDetailsError(const QString &strError)
{
    /* Update desktop pane: */
    AssertPtrReturnVoid(m_pPaneDesktop);
    m_pPaneDesktop->updateDetailsError(strError);
}

void UIToolPaneMachine::setCurrentItem(UIVirtualMachineItem *pItem)
{
    if (m_pItem == pItem)
        return;

    /* Do we need translation after that? */
    const bool fTranslationRequired =  (!pItem &&  m_pItem)
                                    || ( pItem && !m_pItem)
                                    || (!pItem->accessible() &&  m_pItem->accessible())
                                    || ( pItem->accessible() && !m_pItem->accessible());

    /* Remember new item: */
    m_pItem = pItem;

    /* Retranslate if necessary: */
    if (fTranslationRequired)
        retranslateUi();
}

void UIToolPaneMachine::setItems(const QList<UIVirtualMachineItem*> &items)
{
    /* Update details pane: */
    AssertPtrReturnVoid(m_pPaneDetails);
    m_pPaneDetails->setItems(items);
}

void UIToolPaneMachine::setMachine(const CMachine &comMachine)
{
    /* Update snapshots pane is it is open: */
    if (isToolOpened(ToolTypeMachine_Snapshots))
    {
        AssertPtrReturnVoid(m_pPaneSnapshots);
        m_pPaneSnapshots->setMachine(comMachine);
    }
    /* Update logviewer pane is it is open: */
    if (isToolOpened(ToolTypeMachine_LogViewer))
    {
        AssertPtrReturnVoid(m_pPaneLogViewer);
        m_pPaneLogViewer->setMachine(comMachine);
    }
}

void UIToolPaneMachine::retranslateUi()
{
    /* Translate Machine Tools welcome screen: */
    if (!m_pItem || !m_pItem->accessible())
    {
        m_pPaneDesktop->setToolsPaneIcon(UIIconPool::iconSet(":/welcome_200px.png"));
        m_pPaneDesktop->setToolsPaneText(
            tr("<h3>Welcome to VirtualBox!</h3>"
               "<p>The left part of this window lists all virtual "
               "machines and virtual machine groups on your computer. "
               "The list is empty now because you haven't created any "
               "virtual machines yet.</p>"
               "<p>In order to create a new virtual machine, press the "
               "<b>New</b> button in the main tool bar located at the "
               "top of the window.</p>"
               "<p>You can press the <b>%1</b> key to get instant help, or visit "
               "<a href=https://www.virtualbox.org>www.virtualbox.org</a> "
               "for more information and latest news.</p>")
               .arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
    }
    else
    {
        m_pPaneDesktop->setToolsPaneIcon(UIIconPool::iconSet(":/tools_banner_machine_200px.png"));
        m_pPaneDesktop->setToolsPaneText(
            tr("<h3>Welcome to VirtualBox!</h3>"
               "<p>The left part of this window lists all virtual "
               "machines and virtual machine groups on your computer.</p>"
               "<p>The right part of this window represents a set of "
               "tools which are currently opened (or can be opened) for "
               "the currently chosen machine. For a list of currently "
               "available tools check the corresponding menu at the right "
               "side of the main tool bar located at the top of the window. "
               "This list will be extended with new tools in future releases.</p>"
               "<p>You can press the <b>%1</b> key to get instant help, or visit "
               "<a href=https://www.virtualbox.org>www.virtualbox.org</a> "
               "for more information and latest news.</p>")
               .arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
    }

    /* Wipe out the tool descriptions: */
    m_pPaneDesktop->removeToolDescriptions();

    /* Add tool descriptions: */
    if (m_pItem && m_pItem->accessible())
    {
        QAction *pAction1 = m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Details);
        m_pPaneDesktop->addToolDescription(pAction1,
                                           tr("Tool to observe virtual machine (VM) details. "
                                              "Reflects groups of <u>properties</u> for the currently chosen VM and allows "
                                              "basic operations on certain properties (like the machine storage devices)."));
        QAction *pAction2 = m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots);
        m_pPaneDesktop->addToolDescription(pAction2,
                                           tr("Tool to control virtual machine (VM) snapshots. "
                                              "Reflects <u>snapshots</u> created for the currently selected VM and allows "
                                              "snapshot operations like <u>create</u>, <u>remove</u>, "
                                              "<u>restore</u> (make current) and observe their properties. Allows to "
                                              "<u>edit</u> snapshot attributes like <u>name</u> and <u>description</u>."));
        QAction *pAction3 = m_pActionPool->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer);
        m_pPaneDesktop->addToolDescription(pAction3,
                                           tr("Tool to display  virtual machine (VM) logs. "));
    }
}

void UIToolPaneMachine::prepare()
{
    /* Create stacked-layout: */
    m_pLayout = new QStackedLayout(this);

    /* Create desktop pane: */
    openTool(ToolTypeMachine_Desktop);

    /* Apply language settings: */
    retranslateUi();
}

void UIToolPaneMachine::cleanup()
{
    /* Remove all widgets prematurelly: */
    while (m_pLayout->count())
    {
        QWidget *pWidget = m_pLayout->widget(0);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}
