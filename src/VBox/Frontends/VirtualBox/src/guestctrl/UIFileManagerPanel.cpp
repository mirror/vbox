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
#include <QCheckBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QSpinBox>
#include <QTabWidget>

/* GUI includes: */
#include "UIFileManagerPanel.h"


UIFileManagerPanel::UIFileManagerPanel(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pTabWidget(0)
    , m_pListDirectoriesOnTopCheckBox(0)
    , m_pDeleteConfirmationCheckBox(0)
    , m_pHumanReabableSizesCheckBox(0)
    , m_pShowHiddenObjectsCheckBox(0)
{
    prepare();
    retranslateUi();
}


void UIFileManagerPanel::prepare()
{
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);
    AssertReturnVoid(pMainLayout);

    m_pTabWidget = new QTabWidget;
    AssertReturnVoid(m_pTabWidget);
    pMainLayout->addWidget(m_pTabWidget);
    preparePreferencesTab();
}

void UIFileManagerPanel::preparePreferencesTab()
{
    m_pPreferencesTab = new QWidget;
    m_pListDirectoriesOnTopCheckBox = new QCheckBox;
    m_pDeleteConfirmationCheckBox = new QCheckBox;
    m_pHumanReabableSizesCheckBox = new QCheckBox;
    m_pShowHiddenObjectsCheckBox = new QCheckBox;

    AssertReturnVoid(m_pPreferencesTab);
    AssertReturnVoid(m_pListDirectoriesOnTopCheckBox);
    AssertReturnVoid(m_pDeleteConfirmationCheckBox);
    AssertReturnVoid(m_pHumanReabableSizesCheckBox);
    AssertReturnVoid(m_pShowHiddenObjectsCheckBox);

    connect(m_pListDirectoriesOnTopCheckBox, &QCheckBox::toggled, this, &UIFileManagerPanel::sigListDirectoryCheckBoxToogled);
    connect(m_pDeleteConfirmationCheckBox, &QCheckBox::toggled, this, &UIFileManagerPanel::sigDeleteConfirmationCheckBoxToogled);
    connect(m_pHumanReabableSizesCheckBox, &QCheckBox::toggled, this, &UIFileManagerPanel::sigHumanReabableSizesCheckBoxToogled);
    connect(m_pShowHiddenObjectsCheckBox, &QCheckBox::toggled, this, &UIFileManagerPanel::sigShowHiddenObjectsCheckBoxToggled);

    QGridLayout *pPreferencesLayout = new QGridLayout(m_pPreferencesTab);
    AssertReturnVoid(pPreferencesLayout);
    pPreferencesLayout->addWidget(m_pListDirectoriesOnTopCheckBox, 0, 0, 1, 1);
    pPreferencesLayout->addWidget(m_pDeleteConfirmationCheckBox,   1, 0, 1, 1);
    pPreferencesLayout->addWidget(m_pHumanReabableSizesCheckBox,   0, 1, 1, 1);
    pPreferencesLayout->addWidget(m_pShowHiddenObjectsCheckBox,    1, 1, 1, 1);

    m_pTabWidget->addTab(m_pPreferencesTab, QApplication::translate("UIFileManager", "Preferences"));
}



void UIFileManagerPanel::retranslateUi()
{
    if (m_pListDirectoriesOnTopCheckBox)
    {
        m_pListDirectoriesOnTopCheckBox->setText(QApplication::translate("UIFileManager", "List directories on top"));
        m_pListDirectoriesOnTopCheckBox->setToolTip(QApplication::translate("UIFileManager", "List directories before files"));
    }

    if (m_pDeleteConfirmationCheckBox)
    {
        m_pDeleteConfirmationCheckBox->setText(QApplication::translate("UIFileManager", "Ask before delete"));
        m_pDeleteConfirmationCheckBox->setToolTip(QApplication::translate("UIFileManager", "Show a confirmation dialog "
                                                                                "before deleting files and directories"));
    }

    if (m_pHumanReabableSizesCheckBox)
    {
        m_pHumanReabableSizesCheckBox->setText(QApplication::translate("UIFileManager", "Human readable sizes"));
        m_pHumanReabableSizesCheckBox->setToolTip(QApplication::translate("UIFileManager", "Show file/directory sizes in human "
                                                                                "readable format rather than in bytes"));
    }

    if (m_pShowHiddenObjectsCheckBox)
    {
        m_pShowHiddenObjectsCheckBox->setText(QApplication::translate("UIFileManager", "Show hidden objects"));
        m_pShowHiddenObjectsCheckBox->setToolTip(QApplication::translate("UIFileManager", "Show hidden files/directories"));
    }
}



#include "UIFileManagerPanel.moc"
