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
    , m_pColorChangeReset0(0)
    , m_pColorChangeReset1(0)
{
    prepare();
}

void UIVMActivityMonitorPaneContainer::prepare()
{
    sltRetranslateUI();

    QWidget *pContainerWidget = new QWidget(this);
    insertTab(0, pContainerWidget, m_strTabText);

    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIVMActivityMonitorPaneContainer::sltRetranslateUI);
}

void UIVMActivityMonitorPaneContainer::sltRetranslateUI()
{
    m_strTabText = QApplication::translate("UIVMActivityMonitorPaneContainer", "Preferences");
}
