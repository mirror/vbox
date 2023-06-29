/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoBrowserBase class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QHeaderView>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTreeView>

/* GUI includes: */
#include "QIToolBar.h"
#include "QIToolButton.h"
#include "UIActionPool.h"
#include "UIFileTableNavigationWidget.h"
#include "UIIconPool.h"
#include "UIVisoBrowserBase.h"

/* Other VBox includes: */
#include <iprt/assert.h>


UIVisoBrowserBase::UIVisoBrowserBase(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pToolBar(0)
    , m_pActionPool(pActionPool)
    , m_pGoHome(0)
    , m_pGoUp(0)
    , m_pGoForward(0)
    , m_pGoBackward(0)
    , m_pNavigationWidget(0)
    , m_pFileTableLabel(0)
{
}

UIVisoBrowserBase::~UIVisoBrowserBase()
{
}

void UIVisoBrowserBase::prepareObjects()
{
    m_pMainLayout = new QGridLayout;
    AssertPtrReturnVoid(m_pMainLayout);
    setLayout(m_pMainLayout);

    QHBoxLayout *pTopLayout = new QHBoxLayout;
    AssertPtrReturnVoid(pTopLayout);

    m_pToolBar = new QIToolBar;
    m_pNavigationWidget = new UIFileTableNavigationWidget;
    m_pFileTableLabel = new QLabel;

    AssertReturnVoid(m_pToolBar);
    AssertReturnVoid(m_pNavigationWidget);
    AssertReturnVoid(m_pFileTableLabel);

    pTopLayout->addWidget(m_pFileTableLabel);
    pTopLayout->addWidget(m_pNavigationWidget);

    m_pMainLayout->addWidget(m_pToolBar, 0, 0, 1, 4);
    m_pMainLayout->addLayout(pTopLayout, 1, 0, 1, 4);

    m_pMainLayout->setRowStretch(2, 2);
}

void UIVisoBrowserBase::prepareConnections()
{
    if (m_pNavigationWidget)
        connect(m_pNavigationWidget, &UIFileTableNavigationWidget::sigPathChanged,
                this, &UIVisoBrowserBase::sltNavigationWidgetPathChange);
}

void UIVisoBrowserBase::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
}

/* Close the tree view when it recieves focus-out and enter key press event: */
bool UIVisoBrowserBase::eventFilter(QObject *pObj, QEvent *pEvent)
{
    /* Call to base-class: */
    return QIWithRetranslateUI<QWidget>::eventFilter(pObj, pEvent);
}

void UIVisoBrowserBase::keyPressEvent(QKeyEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::keyPressEvent(pEvent);
}

void UIVisoBrowserBase::sltTableViewItemDoubleClick(const QModelIndex &index)
{
    tableViewItemDoubleClick(index);
}

void UIVisoBrowserBase::updateNavigationWidgetPath(const QString &strPath)
{
    if (!m_pNavigationWidget)
        return;
    m_pNavigationWidget->setPath(strPath);
}

void UIVisoBrowserBase::sltNavigationWidgetPathChange(const QString &strPath)
{
    setPathFromNavigationWidget(strPath);
    enableForwardBackwardActions();
}

void UIVisoBrowserBase::setFileTableLabelText(const QString &strText)
{
    if (m_pFileTableLabel)
        m_pFileTableLabel->setText(strText);
}

void UIVisoBrowserBase::sltGoForward()
{
    if (m_pNavigationWidget)
        m_pNavigationWidget->goForwardInHistory();
}

void UIVisoBrowserBase::sltGoBackward()
{
    if (m_pNavigationWidget)
        m_pNavigationWidget->goBackwardInHistory();
}

void UIVisoBrowserBase::enableForwardBackwardActions()
{
    AssertReturnVoid(m_pNavigationWidget);
    if (m_pGoForward)
        m_pGoForward->setEnabled(m_pNavigationWidget->canGoForward());
    if (m_pGoBackward)
        m_pGoBackward->setEnabled(m_pNavigationWidget->canGoBackward());
}
