/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneGlobal class implementation.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
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
#include <QStackedLayout>
#ifndef VBOX_WS_MAC
# include <QStyle>
#endif
#include <QUuid>

/* GUI includes */
#include "UIActionPoolManager.h"
#include "UICloudProfileManager.h"
#include "UIHostNetworkManager.h"
#include "UIMediumManager.h"
#include "UIToolPaneGlobal.h"
#include "UIResourceMonitor.h"
#include "UIWelcomePane.h"

/* Other VBox includes: */
#include <iprt/assert.h>


UIToolPaneGlobal::UIToolPaneGlobal(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayout(0)
    , m_pPaneWelcome(0)
    , m_pPaneMedia(0)
    , m_pPaneNetwork(0)
    , m_pPaneCloud(0)
    , m_pPaneResourceMonitor(0)
{
    /* Prepare: */
    prepare();
}

UIToolPaneGlobal::~UIToolPaneGlobal()
{
    /* Cleanup: */
    cleanup();
}

UIToolType UIToolPaneGlobal::currentTool() const
{
    return   m_pLayout && m_pLayout->currentWidget()
           ? m_pLayout->currentWidget()->property("ToolType").value<UIToolType>()
           : UIToolType_Invalid;
}

bool UIToolPaneGlobal::isToolOpened(UIToolType enmType) const
{
    /* Search through the stacked widgets: */
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<UIToolType>() == enmType)
            return true;
    return false;
}

void UIToolPaneGlobal::openTool(UIToolType enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<UIToolType>() == enmType)
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
            case UIToolType_Welcome:
            {
                /* Create Desktop pane: */
                m_pPaneWelcome = new UIWelcomePane;
                if (m_pPaneWelcome)
                {
                    /* Configure pane: */
                    m_pPaneWelcome->setProperty("ToolType", QVariant::fromValue(UIToolType_Welcome));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneWelcome);
                    m_pLayout->setCurrentWidget(m_pPaneWelcome);
                }
                break;
            }
            case UIToolType_Media:
            {
                /* Create Virtual Media Manager: */
                m_pPaneMedia = new UIMediumManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneMedia);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneMedia->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneMedia->setProperty("ToolType", QVariant::fromValue(UIToolType_Media));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneMedia);
                    m_pLayout->setCurrentWidget(m_pPaneMedia);
                }
                break;
            }
            case UIToolType_Network:
            {
                /* Create Host Network Manager: */
                m_pPaneNetwork = new UIHostNetworkManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneNetwork);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneNetwork->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneNetwork->setProperty("ToolType", QVariant::fromValue(UIToolType_Network));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneNetwork);
                    m_pLayout->setCurrentWidget(m_pPaneNetwork);
                }
                break;
            }
            case UIToolType_Cloud:
            {
                /* Create Cloud Profile Manager: */
                m_pPaneCloud = new UICloudProfileManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneCloud);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneCloud->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneCloud->setProperty("ToolType", QVariant::fromValue(UIToolType_Cloud));
                    connect(m_pPaneCloud, &UICloudProfileManagerWidget::sigChange,
                            this, &UIToolPaneGlobal::sigCloudProfileManagerChange);

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneCloud);
                    m_pLayout->setCurrentWidget(m_pPaneCloud);
                }
                break;
            }
            case UIToolType_VMResourceMonitor:
            {
                m_pPaneResourceMonitor = new UIResourceMonitorWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneResourceMonitor);
                {
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneResourceMonitor->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Configure pane: */
                    m_pPaneResourceMonitor->setProperty("ToolType", QVariant::fromValue(UIToolType_VMResourceMonitor));
                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneResourceMonitor);
                    m_pLayout->setCurrentWidget(m_pPaneResourceMonitor);
                }

                break;
            }
            default:
                AssertFailedReturnVoid();
        }
    }
}

void UIToolPaneGlobal::closeTool(UIToolType enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<UIToolType>() == enmType)
            iActualIndex = iIndex;

    /* If widget with such type doesn't exist: */
    if (iActualIndex != -1)
    {
        /* Forget corresponding widget: */
        switch (enmType)
        {
            case UIToolType_Welcome: m_pPaneWelcome = 0; break;
            case UIToolType_Media:   m_pPaneMedia = 0; break;
            case UIToolType_Network: m_pPaneNetwork = 0; break;
            case UIToolType_Cloud:   m_pPaneCloud = 0; break;
            default: break;
        }
        /* Delete corresponding widget: */
        QWidget *pWidget = m_pLayout->widget(iActualIndex);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolPaneGlobal::prepare()
{
    /* Create stacked-layout: */
    m_pLayout = new QStackedLayout(this);

    /* Create desktop pane: */
    openTool(UIToolType_Welcome);
}

void UIToolPaneGlobal::cleanup()
{
    /* Remove all widgets prematurelly: */
    while (m_pLayout->count())
    {
        QWidget *pWidget = m_pLayout->widget(0);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}
