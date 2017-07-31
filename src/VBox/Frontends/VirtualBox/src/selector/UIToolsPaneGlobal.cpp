/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsPaneGlobal class implementation.
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
# include "UIMediumManager.h"
# include "UIHostNetworkManager.h"
# include "UIToolsPaneGlobal.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIToolsPaneGlobal::UIToolsPaneGlobal(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayout(0)
    , m_pPaneDesktop(0)
    , m_pPaneMedia(0)
    , m_pPaneNetwork(0)
{
    /* Prepare: */
    prepare();
}

UIToolsPaneGlobal::~UIToolsPaneGlobal()
{
    /* Cleanup: */
    cleanup();
}

bool UIToolsPaneGlobal::isToolOpened(ToolTypeGlobal enmType) const
{
    /* Search through the stacked widgets: */
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<ToolTypeGlobal>() == enmType)
            return true;
    return false;
}

void UIToolsPaneGlobal::openTool(ToolTypeGlobal enmType)
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
            case ToolTypeGlobal_Desktop:
            {
                /* Create Desktop pane: */
                m_pPaneDesktop = new UIDesktopPane;
                AssertPtrReturnVoid(m_pPaneDesktop);
                {
                    /* Configure pane: */
                    m_pPaneDesktop->setProperty("ToolType", QVariant::fromValue(ToolTypeGlobal_Desktop));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneDesktop);
                    m_pLayout->setCurrentWidget(m_pPaneDesktop);
                }
                break;
            }
            case ToolTypeGlobal_VirtualMedia:
            {
                /* Create Virtual Media Manager: */
                m_pPaneMedia = new UIMediumManagerWidget(EmbedTo_Stack);
                AssertPtrReturnVoid(m_pPaneMedia);
                {
                    /* Configure pane: */
                    m_pPaneMedia->setProperty("ToolType", QVariant::fromValue(ToolTypeGlobal_VirtualMedia));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneMedia);
                    m_pLayout->setCurrentWidget(m_pPaneMedia);
                }
                break;
            }
            case ToolTypeGlobal_HostNetwork:
            {
                /* Create Host Network Manager: */
                m_pPaneNetwork = new UIHostNetworkManagerWidget(EmbedTo_Stack);
                AssertPtrReturnVoid(m_pPaneNetwork);
                {
                    /* Configure pane: */
                    m_pPaneNetwork->setProperty("ToolType", QVariant::fromValue(ToolTypeGlobal_HostNetwork));

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

void UIToolsPaneGlobal::closeTool(ToolTypeGlobal enmType)
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
            case ToolTypeGlobal_Desktop:      m_pPaneDesktop = 0; break;
            case ToolTypeGlobal_VirtualMedia: m_pPaneMedia = 0; break;
            case ToolTypeGlobal_HostNetwork:  m_pPaneNetwork = 0; break;
            default: break;
        }
        /* Delete corresponding widget: */
        QWidget *pWidget = m_pLayout->widget(iActualIndex);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolsPaneGlobal::setDetailsText(const QString &strText)
{
    /* Update desktop pane: */
    AssertPtrReturnVoid(m_pPaneDesktop);
    m_pPaneDesktop->updateDetailsText(strText);
}

void UIToolsPaneGlobal::setDetailsError(const QString &strError)
{
    /* Update desktop pane: */
    AssertPtrReturnVoid(m_pPaneDesktop);
    m_pPaneDesktop->updateDetailsError(strError);
}

void UIToolsPaneGlobal::prepare()
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
    openTool(ToolTypeGlobal_Desktop);
}

void UIToolsPaneGlobal::cleanup()
{
    /* Remove all widgets prematurelly: */
    while (m_pLayout->count())
    {
        QWidget *pWidget = m_pLayout->widget(0);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

