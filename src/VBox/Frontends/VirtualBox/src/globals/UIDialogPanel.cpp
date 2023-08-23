/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
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
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QToolButton>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIDialogPanel.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Other VBox includes: */
#include <iprt/assert.h>

UIDialogPanelBase::UIDialogPanelBase(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pTabWidget(0)
    , m_pCloseButton(0)
{
    prepare();
}

//     m_pCloseButton = new QIToolButton;
//     if (m_pCloseButton)
//     {
//         m_pCloseButton->setIcon(UIIconPool::iconSet(":/close_16px.png"));
//         m_pMainLayout->addWidget(m_pCloseButton, 0, Qt::AlignLeft);
//     }
// }

// void UIDialogPanel::prepareConnections()
// {
//     if (m_pCloseButton)
//         connect(m_pCloseButton, &QIToolButton::clicked, this, &UIDialogPanel::hide);
// }

// void UIDialogPanel::retranslateUi()
// {
//     if (m_pCloseButton)
//         m_pCloseButton->setToolTip(tr("Close the pane"));
// }

void UIDialogPanelBase::prepare()
{
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    AssertReturnVoid(pLayout);
    pLayout->setContentsMargins(0, 0, 0, 0);
    m_pTabWidget = new QTabWidget();
    connect(m_pTabWidget, &QTabWidget::currentChanged, this, &UIDialogPanelBase::sigCurrentTabChanged);
    AssertReturnVoid(m_pTabWidget);
    pLayout->addWidget(m_pTabWidget);
    /* Add a button to close the tab widget: */
    m_pCloseButton = new QIToolButton;
    AssertReturnVoid(m_pCloseButton);
    m_pCloseButton->setIcon(UIIconPool::iconSet(":/close_16px.png"));
    m_pTabWidget->setCornerWidget(m_pCloseButton);

}

void UIDialogPanelBase::insertTab(int iIndex, QWidget *pPage, const QString &strLabel /* = QString() */)
{
    if (m_pTabWidget)
        m_pTabWidget->insertTab(iIndex, pPage, strLabel);
}

void UIDialogPanelBase::setTabText(int iIndex, const QString &strText)
{
    if (!m_pTabWidget || iIndex < 0 || iIndex >= m_pTabWidget->count())
        return;
    m_pTabWidget->setTabText(iIndex, strText);
}

void UIDialogPanelBase::setCurrentIndex(int iIndex)
{
    if (!m_pTabWidget || iIndex >= m_pTabWidget->count() || iIndex < 0)
        return;
    m_pTabWidget->setCurrentIndex(iIndex);
}

int UIDialogPanelBase::currentIndex() const
{
    if (!m_pTabWidget)
        return -1;
    return m_pTabWidget->currentIndex();
}
