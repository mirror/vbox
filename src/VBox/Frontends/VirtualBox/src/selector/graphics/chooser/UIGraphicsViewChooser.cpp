/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGraphicsViewChooser class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <QVBoxLayout>
#include <QStatusBar>

/* GUI includes: */
#include "UIGraphicsViewChooser.h"
#include "UIGraphicsSelectorModel.h"
#include "UIGraphicsSelectorView.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIIconPool.h"
#include "UIActionPoolSelector.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

UIGraphicsViewChooser::UIGraphicsViewChooser(QWidget *pParent)
    : QWidget(pParent)
    , m_pMainLayout(0)
    , m_pSelectorModel(0)
    , m_pSelectorView(0)
    , m_pStatusBar(0)
{
    /* Create main-layout: */
    m_pMainLayout = new QVBoxLayout(this);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(0);

    /* Create selector-model: */
    m_pSelectorModel = new UIGraphicsSelectorModel(this);

    /* Create selector-view: */
    m_pSelectorView = new UIGraphicsSelectorView(this);
    m_pSelectorView->setFrameShape(QFrame::NoFrame);
    m_pSelectorView->setFrameShadow(QFrame::Plain);
    m_pSelectorView->setScene(m_pSelectorModel->scene());
    m_pSelectorView->show();
    setFocusProxy(m_pSelectorView);

    /* Add tool-bar into layout: */
    m_pMainLayout->addWidget(m_pSelectorView);

    /* Prepare connections: */
    prepareConnections();

    /* Load model: */
    m_pSelectorModel->load();
}

UIGraphicsViewChooser::~UIGraphicsViewChooser()
{
    /* Save model: */
    m_pSelectorModel->save();
}

void UIGraphicsViewChooser::setCurrentItem(int iCurrentItemIndex)
{
    m_pSelectorModel->setCurrentItem(iCurrentItemIndex);
}

UIVMItem* UIGraphicsViewChooser::currentItem() const
{
    return m_pSelectorModel->currentItem();
}

QList<UIVMItem*> UIGraphicsViewChooser::currentItems() const
{
    return m_pSelectorModel->currentItems();
}

void UIGraphicsViewChooser::setStatusBar(QStatusBar *pStatusBar)
{
    /* Old status-bar set? */
    if (m_pStatusBar)
       m_pSelectorModel->disconnect(m_pStatusBar);

    /* Connect new status-bar: */
    m_pStatusBar = pStatusBar;
    connect(m_pSelectorModel, SIGNAL(sigClearStatusMessage()), m_pStatusBar, SLOT(clearMessage()));
    connect(m_pSelectorModel, SIGNAL(sigShowStatusMessage(const QString&)), m_pStatusBar, SLOT(showMessage(const QString&)));
}

void UIGraphicsViewChooser::prepareConnections()
{
    /* Selector-model connections: */
    connect(m_pSelectorModel, SIGNAL(sigRootItemResized(const QSizeF&, int)),
            m_pSelectorView, SLOT(sltHandleRootItemResized(const QSizeF&, int)));
    connect(m_pSelectorModel, SIGNAL(sigSelectionChanged()), this, SIGNAL(sigSelectionChanged()));

    /* Selector-view connections: */
    connect(m_pSelectorView, SIGNAL(sigResized()), m_pSelectorModel, SLOT(sltHandleViewResized()));

    /* Global connections: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), m_pSelectorModel, SLOT(sltMachineStateChanged(QString, KMachineState)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QString)), m_pSelectorModel, SLOT(sltMachineDataChanged(QString)));
    connect(gVBoxEvents, SIGNAL(sigMachineRegistered(QString, bool)), m_pSelectorModel, SLOT(sltMachineRegistered(QString, bool)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), m_pSelectorModel, SLOT(sltSessionStateChanged(QString, KSessionState)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QString, QString)), m_pSelectorModel, SLOT(sltSnapshotChanged(QString, QString)));

    /* Context menu connections: */
    connect(gActionPool->action(UIActionIndexSelector_Simple_Machine_RemoveGroupDialog), SIGNAL(triggered()),
            m_pSelectorModel, SLOT(sltRemoveCurrentlySelectedGroup()));
}

