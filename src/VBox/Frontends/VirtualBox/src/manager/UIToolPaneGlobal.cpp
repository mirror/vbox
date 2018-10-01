/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneGlobal class implementation.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
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
# include "UIHostNetworkManager.h"
# include "UIMediumManager.h"
# include "UIToolPaneGlobal.h"
# include "UIWelcomePane.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolPaneGlobal::UIToolPaneGlobal(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayout(0)
    , m_pPaneWelcome(0)
    , m_pPaneMedia(0)
    , m_pPaneNetwork(0)
{
    /* Prepare: */
    prepare();
}

UIToolPaneGlobal::~UIToolPaneGlobal()
{
    /* Cleanup: */
    cleanup();
}

ToolTypeGlobal UIToolPaneGlobal::currentTool() const
{
    return m_pLayout->currentWidget()->property("ToolType").value<ToolTypeGlobal>();
}

bool UIToolPaneGlobal::isToolOpened(ToolTypeGlobal enmType) const
{
    /* Search through the stacked widgets: */
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeGlobal>() == enmType)
            return true;
    return false;
}

void UIToolPaneGlobal::openTool(ToolTypeGlobal enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeGlobal>() == enmType)
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
            case ToolTypeGlobal_Welcome:
            {
                /* Create Desktop pane: */
                m_pPaneWelcome = new UIWelcomePane;
                if (m_pPaneWelcome)
                {
                    /* Configure pane: */
                    m_pPaneWelcome->setProperty("ToolType", QVariant::fromValue(ToolTypeGlobal_Welcome));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneWelcome);
                    m_pLayout->setCurrentWidget(m_pPaneWelcome);
                }
                break;
            }
            case ToolTypeGlobal_Media:
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
                    m_pPaneMedia->setProperty("ToolType", QVariant::fromValue(ToolTypeGlobal_Media));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneMedia);
                    m_pLayout->setCurrentWidget(m_pPaneMedia);
                }
                break;
            }
            case ToolTypeGlobal_Network:
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
                    m_pPaneNetwork->setProperty("ToolType", QVariant::fromValue(ToolTypeGlobal_Network));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneNetwork);
                    m_pLayout->setCurrentWidget(m_pPaneNetwork);
                }
                break;
            }
            default:
                AssertFailedReturnVoid();
        }
    }
}

void UIToolPaneGlobal::closeTool(ToolTypeGlobal enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeGlobal>() == enmType)
            iActualIndex = iIndex;

    /* If widget with such type doesn't exist: */
    if (iActualIndex != -1)
    {
        /* Forget corresponding widget: */
        switch (enmType)
        {
            case ToolTypeGlobal_Welcome: m_pPaneWelcome = 0; break;
            case ToolTypeGlobal_Media:   m_pPaneMedia = 0; break;
            case ToolTypeGlobal_Network: m_pPaneNetwork = 0; break;
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
    openTool(ToolTypeGlobal_Media);
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
