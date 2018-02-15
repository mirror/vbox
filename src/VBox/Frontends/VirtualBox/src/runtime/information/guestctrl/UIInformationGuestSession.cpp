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
# include <QApplication>
# include <QVBoxLayout>
# include <QSplitter>

/* GUI includes: */
# include "QITreeWidget.h"
# include "UIGuestControlConsole.h"
# include "UIGuestControlInterface.h"
# include "UIGuestSessionTreeItem.h"
# include "UIGuestSessionsEventHandler.h"
# include "UIInformationGuestSession.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CGuest.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationGuestSession::UIInformationGuestSession(QWidget *pParent, const CConsole &console)
    : QWidget(pParent)
    , m_comConsole(console)
    , m_pMainLayout(0)
    , m_pSplitter(0)
    , m_pGuestSessionsEventHandler(0)
    , m_pTreeWidget(0)
    , m_pConsole(0)
    , m_pControlInterface(0)
{
    prepareObjects();
    prepareConnections();

    // if(m_pControlInterface)
    //     m_pControlInterface->unitTest();
}

void UIInformationGuestSession::prepareObjects()
{
    m_pGuestSessionsEventHandler = new UIGuestSessionsEventHandler(this, m_comConsole.GetGuest());
    m_pControlInterface = new UIGuestControlInterface(this, m_comConsole.GetGuest());

    /* Create layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if(!m_pMainLayout)
        return;

    /* Configure layout: */
    m_pMainLayout->setSpacing(0);

    m_pSplitter = new QSplitter;

    if(!m_pSplitter)
        return;

    m_pSplitter->setOrientation(Qt::Vertical);

    m_pMainLayout->addWidget(m_pSplitter);


    m_pTreeWidget = new QITreeWidget;

    if (m_pTreeWidget)
        m_pSplitter->addWidget(m_pTreeWidget);

    m_pConsole = new UIGuestControlConsole;
    if(m_pConsole)
    {
        m_pSplitter->addWidget(m_pConsole);
        setFocusProxy(m_pConsole);
    }

    m_pSplitter->setStretchFactor(0, 9);
    m_pSplitter->setStretchFactor(1, 4);

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

void UIInformationGuestSession::prepareConnections()
{
    connect(m_pControlInterface, &UIGuestControlInterface::sigOutputString,
            this, &UIInformationGuestSession::sltConsoleOutputReceived);
    connect(m_pConsole, &UIGuestControlConsole::commandEntered,
            this, &UIInformationGuestSession::sltConsoleCommandEntered);
    connect(m_pGuestSessionsEventHandler, &UIGuestSessionsEventHandler::sigGuestSessionsUpdated,
            this, &UIInformationGuestSession::sltGuestSessionsUpdated);
}

void UIInformationGuestSession::sltGuestSessionsUpdated()
{
    updateTreeWidget();
}

void UIInformationGuestSession::sltConsoleCommandEntered(const QString &strCommand)
{
    if(m_pControlInterface)
    {
        m_pControlInterface->putCommand(strCommand);
    }
}

void UIInformationGuestSession::sltConsoleOutputReceived(const QString &strOutput)
{
    if(m_pConsole)
    {
        m_pConsole->putOutput(strOutput);
    }
}
