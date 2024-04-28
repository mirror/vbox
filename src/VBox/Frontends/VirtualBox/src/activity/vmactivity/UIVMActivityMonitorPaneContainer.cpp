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
#include <QColor>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>

/* GUI includes: */
#include "UITranslationEventListener.h"
#include "UIVMActivityMonitorPaneContainer.h"

/* Other includes: */
#include "iprt/assert.h"

UIVMActivityMonitorPaneContainer::UIVMActivityMonitorPaneContainer(QWidget *pParent,
                                                                   EmbedTo enmEmbedTo /* = EmbedTo_Stack */)
    : UIPaneContainer(pParent, enmEmbedTo, false /* detach not allowed */)
    , m_pColorLabel{0, 0}
    , m_pColorChangeButton{0, 0}
{
    prepare();
}

void UIVMActivityMonitorPaneContainer::prepare()
{
    QWidget *pContainerWidget = new QWidget(this);
    QVBoxLayout *pContainerLayout = new QVBoxLayout(pContainerWidget);
    insertTab(Tab_Preferences, pContainerWidget, "");

    for (int i = 0; i < 2; ++i)
    {
        QHBoxLayout *pColorLayout = new QHBoxLayout;
        m_pColorLabel[i] = new QLabel(this);
        m_pColorChangeButton[i] = new QPushButton(this);
        pColorLayout->addWidget(m_pColorLabel[i]);
        pColorLayout->addWidget(m_pColorChangeButton[i]);
        pColorLayout->addStretch();
        pContainerLayout->addLayout(pColorLayout);
        connect(m_pColorChangeButton[i], &QPushButton::pressed,
                this, &UIVMActivityMonitorPaneContainer::sltColorChangeButtonPressed);
    }

    pContainerLayout->addStretch();

    sltRetranslateUI();
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIVMActivityMonitorPaneContainer::sltRetranslateUI);
}

void UIVMActivityMonitorPaneContainer::sltRetranslateUI()
{
    setTabText(Tab_Preferences, QApplication::translate("UIVMActivityMonitorPaneContainer", "Preferences"));

    if (m_pColorLabel[0])
        m_pColorLabel[0]->setText(QApplication::translate("UIVMActivityMonitorPaneContainer", "Data Series 1 Color"));
    if (m_pColorLabel[1])
        m_pColorLabel[1]->setText(QApplication::translate("UIVMActivityMonitorPaneContainer", "Data Series 2 Color"));
}

void UIVMActivityMonitorPaneContainer::colorPushButtons(QPushButton *pButton, const QColor &color)
{
    AssertReturnVoid(pButton);
    int iSize = qApp->style()->pixelMetric(QStyle::PM_ButtonIconSize);
    QPixmap iconPixmap(iSize, iSize);
    QPainter painter(&iconPixmap);
    painter.setBrush(color);
    painter.drawRect(iconPixmap.rect());
    pButton->setIcon(QIcon(iconPixmap));
}

void UIVMActivityMonitorPaneContainer::setDataSeriesColor(int iIndex, const QColor &color)
{
    if (iIndex == 0 || iIndex == 1)
    {
        m_color[iIndex] = color;
        colorPushButtons(m_pColorChangeButton[iIndex], color);
    }
}

void UIVMActivityMonitorPaneContainer::sltColorChangeButtonPressed()
{
    int iIndex = -1;
    if (sender() == m_pColorChangeButton[0])
        iIndex = 0;
    else if (sender() == m_pColorChangeButton[1])
        iIndex = 1;
    else
        return;

    QColorDialog colorDialog(m_color[iIndex], this);
    if (colorDialog.exec() == QDialog::Rejected)
        return;
    QColor newColor = colorDialog.selectedColor();
    if (m_color[iIndex] == newColor)
        return;
    m_color[iIndex] = newColor;
    colorPushButtons(m_pColorChangeButton[iIndex], newColor);
    emit sigColorChanged(iIndex, newColor);
}
