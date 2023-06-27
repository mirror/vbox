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
#include "QIToolButton.h"
#include "UIFileTableNavigationWidget.h"
#include "UIIconPool.h"
#include "UIVisoBrowserBase.h"


/*********************************************************************************************************************************
*   UIVisoBrowserBase implementation.                                                                                   *
*********************************************************************************************************************************/

UIVisoBrowserBase::UIVisoBrowserBase(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pNavigationWidget(0)
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


    m_pNavigationWidget = new UIFileTableNavigationWidget;
    m_pFileTableLabel = new QLabel;

    AssertReturnVoid(m_pNavigationWidget);
    AssertReturnVoid(m_pFileTableLabel);

    pTopLayout->addWidget(m_pFileTableLabel);
    pTopLayout->addWidget(m_pNavigationWidget);
    m_pMainLayout->addLayout(pTopLayout, 0, 0, 1, 4);

    m_pMainLayout->setRowStretch(1, 2);
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

void UIVisoBrowserBase::sltFileTableViewContextMenu(const QPoint &point)
{
    QWidget *pSender = qobject_cast<QWidget*>(sender());
    if (!pSender)
        return;
    emit sigCreateFileTableViewContextMenu(pSender, point);
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
}

void UIVisoBrowserBase::setFileTableLabelText(const QString &strText)
{
    if (m_pFileTableLabel)
        m_pFileTableLabel->setText(strText);
}
