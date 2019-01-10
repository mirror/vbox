/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoBrowserBase class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <QHeaderView>
#include <QGridLayout>
#include <QLabel>
#include <QSplitter>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIVisoBrowserBase.h"
#include "UIToolBar.h"

UIVisoBrowserBase::UIVisoBrowserBase(QWidget *pParent)
    : QWidget(pParent)
    , m_pTreeView(0)
    , m_pTitleLabel(0)
    , m_pRightContainerWidget(0)
    , m_pRightContainerLayout(0)
    , m_pVerticalToolBar(0)
    , m_pMainLayout(0)
    , m_pHorizontalSplitter(0)
{
}

UIVisoBrowserBase::~UIVisoBrowserBase()
{
}

void UIVisoBrowserBase::prepareObjects()
{
    m_pMainLayout = new QGridLayout;
    setLayout(m_pMainLayout);

    QWidget *pLeftContainerWidget = new QWidget;
    m_pRightContainerWidget = new QWidget;

    QVBoxLayout *pLeftContainerLayout = new QVBoxLayout;
    m_pRightContainerLayout = new QGridLayout;

    pLeftContainerLayout->setSpacing(1);
    pLeftContainerLayout->setContentsMargins(0, 0, 0, 0);
    m_pRightContainerLayout->setSpacing(1);
    m_pRightContainerLayout->setContentsMargins(0, 0, 0, 0);

    if (!m_pMainLayout || !pLeftContainerLayout || !m_pRightContainerLayout)
        return;
    if (!pLeftContainerWidget || !m_pRightContainerWidget)
        return;

    pLeftContainerWidget->setLayout(pLeftContainerLayout);
    m_pRightContainerWidget->setLayout(m_pRightContainerLayout);

    m_pHorizontalSplitter = new QSplitter;
    if (!m_pHorizontalSplitter)
        return;

    m_pMainLayout->addWidget(m_pHorizontalSplitter, 1, 0);
    m_pHorizontalSplitter->setOrientation(Qt::Horizontal);
    m_pHorizontalSplitter->setHandleWidth(2);

    m_pTitleLabel = new QLabel;
    if (m_pTitleLabel)
    {
        pLeftContainerLayout->addWidget(m_pTitleLabel);
    }

    m_pTreeView = new QTreeView;
    if (m_pTreeView)
    {
        pLeftContainerLayout->addWidget(m_pTreeView);
        m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
        m_pTreeView->setAlternatingRowColors(true);
        m_pTreeView->header()->hide();
        m_pTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    }


    m_pVerticalToolBar = new UIToolBar;
    if (m_pVerticalToolBar)
    {
        m_pVerticalToolBar->setOrientation(Qt::Vertical);
        m_pRightContainerLayout->addWidget(m_pVerticalToolBar, 0, 5, 6, 1);
    }

    m_pHorizontalSplitter->addWidget(pLeftContainerWidget);
    m_pHorizontalSplitter->addWidget(m_pRightContainerWidget);
    m_pHorizontalSplitter->setCollapsible(m_pHorizontalSplitter->indexOf(pLeftContainerWidget), false);
    m_pHorizontalSplitter->setCollapsible(m_pHorizontalSplitter->indexOf(m_pRightContainerWidget), false);
    m_pHorizontalSplitter->setStretchFactor(0, 1);
    m_pHorizontalSplitter->setStretchFactor(1, 3);
}

void UIVisoBrowserBase::prepareConnections()
{
    if (m_pTreeView)
    {
        connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoBrowserBase::sltHandleTreeSelectionChanged);
        connect(m_pTreeView, &QTreeView::clicked,
                this, &UIVisoBrowserBase::sltHandleTreeItemClicked);
    }
}

void UIVisoBrowserBase::sltHandleTableViewItemDoubleClick(const QModelIndex &index)
{
    tableViewItemDoubleClick(index);
}

void UIVisoBrowserBase::sltHandleTreeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList indices = selected.indexes();
    if (indices.empty())
        return;
    QModelIndex selectedIndex = indices[0];
    treeSelectionChanged(selectedIndex);
}


void UIVisoBrowserBase::sltHandleTreeItemClicked(const QModelIndex &modelIndex)
{
    if (!m_pTreeView)
        return;
    m_pTreeView->setExpanded(modelIndex, true);
}


//#include "UIVisoBrowserBase.moc"
