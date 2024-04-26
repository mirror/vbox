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
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

/* GUI includes: */
#include "UITranslationEventListener.h"
#include "UIVMActivityMonitorPaneContainer.h"

UIVMActivityMonitorPaneContainer::UIVMActivityMonitorPaneContainer(QWidget *pParent,
                                                                   EmbedTo enmEmbedTo /* = EmbedTo_Stack */)
    : UIPaneContainer(pParent, enmEmbedTo, false /* detach not allowed */)
    , m_pColorLabel0(0)
    , m_pColorLabel1(0)
    , m_pColorChangeButton0(0)
    , m_pColorChangeButton1(0)
{
    prepare();
}

void UIVMActivityMonitorPaneContainer::prepare()
{
    sltRetranslateUI();

    QWidget *pContainerWidget = new QWidget(this);
    QVBoxLayout *pContainerLayout = new QVBoxLayout(pContainerWidget);
    insertTab(0, pContainerWidget, m_strTabText);

    QHBoxLayout *pColorLayout0 = new QHBoxLayout(this);
    m_pColorLabel0 = new QLabel(this);
    m_pColorChangeButton0 = new QPushButton(this);
    pColorLayout0->addWidget(m_pColorLabel0);
    pColorLayout0->addWidget(m_pColorChangeButton0);

    QHBoxLayout *pColorLayout1 = new QHBoxLayout(this);
    m_pColorLabel1 = new QLabel(this);
    m_pColorChangeButton1 = new QPushButton(this);
    pColorLayout1->addWidget(m_pColorLabel1);
    pColorLayout1->addWidget(m_pColorChangeButton1);

    pContainerLayout->addLayout(pColorLayout0);
    pContainerLayout->addLayout(pColorLayout1);


    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIVMActivityMonitorPaneContainer::sltRetranslateUI);
}

void UIVMActivityMonitorPaneContainer::sltRetranslateUI()
{
    m_strTabText = QApplication::translate("UIVMActivityMonitorPaneContainer", "Preferences");

    if (m_pColorLabel0)
        m_pColorLabel0->setText(QApplication::translate("UIVMActivityMonitorPaneContainer", "Data Series 1 Color"));
    if (m_pColorLabel1)
        m_pColorLabel1->setText(QApplication::translate("UIVMActivityMonitorPaneContainer", "Data Series 1 Color"));
}
