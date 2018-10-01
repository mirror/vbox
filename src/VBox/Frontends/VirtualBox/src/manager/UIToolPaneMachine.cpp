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
# include "UIErrorPane.h"
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
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pItem(0)
    , m_pLayout(0)
    , m_pPaneError(0)
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
            case ToolTypeMachine_Error:
            {
                /* Create Desktop pane: */
                m_pPaneError = new UIErrorPane(m_pActionPool->action(UIActionIndexST_M_Group_S_Refresh));
                if (m_pPaneError)
                {
                    /* Configure pane: */
                    m_pPaneError->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Error));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneError);
                    m_pLayout->setCurrentWidget(m_pPaneError);
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
                m_pPaneSnapshots = new UISnapshotPane(m_pActionPool, false /* show toolbar? */);
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
            case ToolTypeMachine_Logs:
            {
                /* Create the Logviewer pane: */
                m_pPaneLogViewer = new UIVMLogViewerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneLogViewer);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneLogViewer->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneLogViewer->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Logs));

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
            case ToolTypeMachine_Error:     m_pPaneError = 0; break;
            case ToolTypeMachine_Details:   m_pPaneDetails = 0; break;
            case ToolTypeMachine_Snapshots: m_pPaneSnapshots = 0; break;
            case ToolTypeMachine_Logs:      m_pPaneLogViewer = 0; break;
            default: break;
        }
        /* Delete corresponding widget: */
        QWidget *pWidget = m_pLayout->widget(iActualIndex);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolPaneMachine::setErrorDetails(const QString &strDetails)
{
    /* Update Error pane: */
    if (m_pPaneError)
        m_pPaneError->setErrorDetails(strDetails);
}

void UIToolPaneMachine::setCurrentItem(UIVirtualMachineItem *pItem)
{
    if (m_pItem == pItem)
        return;

    /* Remember new item: */
    m_pItem = pItem;
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
    if (isToolOpened(ToolTypeMachine_Logs))
    {
        AssertPtrReturnVoid(m_pPaneLogViewer);
        m_pPaneLogViewer->setMachine(comMachine);
    }
}

void UIToolPaneMachine::prepare()
{
    /* Create stacked-layout: */
    m_pLayout = new QStackedLayout(this);

    /* Create Details pane: */
    openTool(ToolTypeMachine_Details);
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
