/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationGuestSession class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include <QVBoxLayout>
# include <QApplication>

/* GUI includes: */
# include "QITreeWidget.h"
# include "UIInformationGuestSession.h"
# include "UIGuestSessionTreeItem.h"
# include "UIGuestSessionsEventHandler.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationGuestSession::UIInformationGuestSession(QWidget *pParent, const CConsole &console)
    : QWidget(pParent)
    , m_comConsole(console)
    , m_pMainLayout(0)
    , m_pGuestSessionsEventHandler(0)
    , m_pTreeWidget(0)
{
    prepareEventHandler();
    prepareLayout();
    prepareWidgets();
}

void UIInformationGuestSession::prepareLayout()
{
    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (m_pMainLayout)
    {
        /* Configure layout: */
        m_pMainLayout->setSpacing(0);
    }
}

void UIInformationGuestSession::prepareWidgets()
{
    m_pTreeWidget = new QITreeWidget();
    m_pMainLayout->addWidget(m_pTreeWidget);
    updateTreeWidget();
}

void UIInformationGuestSession::updateTreeWidget()
{
    if (!m_pTreeWidget)
        return;
    if (!m_pGuestSessionsEventHandler)
        return;

    m_pTreeWidget->clear();
    QVector<QITreeWidgetItem> treeItemVector;
    m_pGuestSessionsEventHandler->populateGuestSessionsTree(m_pTreeWidget);
    update();
}

void UIInformationGuestSession::prepareEventHandler()
{
    m_pGuestSessionsEventHandler = new UIGuestSessionsEventHandler(this, m_comConsole.GetGuest());
    connect(m_pGuestSessionsEventHandler, &UIGuestSessionsEventHandler::sigGuestSessionsUpdated,
            this, &UIInformationGuestSession::sltGuestSessionsUpdated);
}

void UIInformationGuestSession::sltGuestSessionsUpdated()
{
    updateTreeWidget();
}
