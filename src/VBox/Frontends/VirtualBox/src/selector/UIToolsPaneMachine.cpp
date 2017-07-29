/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsPaneMachine class implementation.
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

/* GUI includes */
# include "UIActionPoolSelector.h"
# include "UIDesktopPane.h"
# include "UIGDetails.h"
# include "UISnapshotPane.h"
# include "UIToolsPaneMachine.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolsPaneMachine::UIToolsPaneMachine(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayout(0)
    , m_pPaneDesktop(0)
    , m_pPaneDetails(0)
    , m_pPaneSnapshots(0)
{
    /* Prepare: */
    prepare();
}

UIToolsPaneMachine::~UIToolsPaneMachine()
{
    /* Cleanup: */
    cleanup();
}

bool UIToolsPaneMachine::isToolOpened(ToolTypeMachine enmType) const
{
    /* Search through the stacked widgets: */
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeMachine>() == enmType)
            return true;
    return false;
}

void UIToolsPaneMachine::openTool(ToolTypeMachine enmType)
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
                m_pPaneDesktop = new UIDesktopPane(m_pActionPool->action(UIActionIndexST_M_Group_S_Refresh));
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
                m_pPaneDetails = new UIGDetails;
                AssertPtrReturnVoid(m_pPaneDetails);
                {
                    /* Configure pane: */
                    m_pPaneDetails->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Details));
                    connect(this, &UIToolsPaneMachine::sigSlidingStarted, m_pPaneDetails, &UIGDetails::sigSlidingStarted);
                    connect(this, &UIToolsPaneMachine::sigToggleStarted,  m_pPaneDetails, &UIGDetails::sigToggleStarted);
                    connect(this, &UIToolsPaneMachine::sigToggleFinished, m_pPaneDetails, &UIGDetails::sigToggleFinished);
                    connect(m_pPaneDetails, &UIGDetails::sigLinkClicked,  this, &UIToolsPaneMachine::sigLinkClicked);

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
                    /* Configure pane: */
                    m_pPaneSnapshots->setProperty("ToolType", QVariant::fromValue(ToolTypeMachine_Snapshots));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneSnapshots);
                    m_pLayout->setCurrentWidget(m_pPaneSnapshots);
                }
                break;
            }
            default:
                AssertFailedReturnVoid();
        }
    }
}

void UIToolsPaneMachine::closeTool(ToolTypeMachine enmType)
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
            default: break;
        }
        /* Delete corresponding widget: */
        QWidget *pWidget = m_pLayout->widget(iActualIndex);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolsPaneMachine::setDetailsText(const QString &strText)
{
    /* Update desktop pane: */
    AssertPtrReturnVoid(m_pPaneDesktop);
    m_pPaneDesktop->updateDetailsText(strText);
}

void UIToolsPaneMachine::setDetailsError(const QString &strError)
{
    /* Update desktop pane: */
    AssertPtrReturnVoid(m_pPaneDesktop);
    m_pPaneDesktop->updateDetailsError(strError);
}

void UIToolsPaneMachine::setItems(const QList<UIVMItem*> &items)
{
    /* Update details pane: */
    AssertPtrReturnVoid(m_pPaneDetails);
    m_pPaneDetails->setItems(items);
}

void UIToolsPaneMachine::setMachine(const CMachine &comMachine)
{
    /* Update snapshots pane: */
    AssertPtrReturnVoid(m_pPaneSnapshots);
    m_pPaneSnapshots->setMachine(comMachine);
}

void UIToolsPaneMachine::prepare()
{
    /* Create stacked-layout: */
    m_pLayout = new QStackedLayout(this);
    AssertPtrReturnVoid(m_pLayout);
    {
        /* Configure layout: */
        m_pLayout->setSpacing(0);
        m_pLayout->setContentsMargins(3, 4, 5, 0);
    }

    /* Create desktop pane: */
    openTool(ToolTypeMachine_Desktop);
}

void UIToolsPaneMachine::cleanup()
{
    /* Remove all widgets prematurelly: */
    while (m_pLayout->count())
    {
        QWidget *pWidget = m_pLayout->widget(0);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

