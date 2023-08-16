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
#include "UIFileManager.h"

/* Other VBox includes: */
#include <iprt/assert.h>

UIFileManagerPanel::UIFileManagerPanel(QWidget *pParent, UIFileManagerOptions *pFileManagerOptions)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pTabWidget(0)
    , m_pListDirectoriesOnTopCheckBox(0)
    , m_pDeleteConfirmationCheckBox(0)
    , m_pHumanReabableSizesCheckBox(0)
    , m_pShowHiddenObjectsCheckBox(0)
    , m_pFileManagerOptions(pFileManagerOptions)
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

    if (m_pFileManagerOptions)
    {
        if (m_pListDirectoriesOnTopCheckBox)
            m_pListDirectoriesOnTopCheckBox->setChecked(m_pFileManagerOptions->fListDirectoriesOnTop);
        if (m_pDeleteConfirmationCheckBox)
            m_pDeleteConfirmationCheckBox->setChecked(m_pFileManagerOptions->fAskDeleteConfirmation);
        if (m_pHumanReabableSizesCheckBox)
            m_pHumanReabableSizesCheckBox->setChecked(m_pFileManagerOptions->fShowHumanReadableSizes);
        if (m_pShowHiddenObjectsCheckBox)
            m_pShowHiddenObjectsCheckBox->setChecked(m_pFileManagerOptions->fShowHiddenObjects);
    }

    connect(m_pListDirectoriesOnTopCheckBox, &QCheckBox::toggled,
            this, &UIFileManagerPanel::sltListDirectoryCheckBoxToogled);

    connect(m_pDeleteConfirmationCheckBox, &QCheckBox::toggled,
            this, &UIFileManagerPanel::sltDeleteConfirmationCheckBoxToogled);

    connect(m_pHumanReabableSizesCheckBox, &QCheckBox::toggled,
            this, &UIFileManagerPanel::sltHumanReabableSizesCheckBoxToogled);


    connect(m_pShowHiddenObjectsCheckBox, &QCheckBox::toggled,
            this, &UIFileManagerPanel::sltShowHiddenObjectsCheckBoxToggled);

    QGridLayout *pPreferencesLayout = new QGridLayout(m_pPreferencesTab);
    AssertReturnVoid(pPreferencesLayout);
    pPreferencesLayout->addWidget(m_pListDirectoriesOnTopCheckBox, 0, 0, 1, 1);
    pPreferencesLayout->addWidget(m_pDeleteConfirmationCheckBox,   1, 0, 1, 1);
    pPreferencesLayout->addWidget(m_pHumanReabableSizesCheckBox,   0, 1, 1, 1);
    pPreferencesLayout->addWidget(m_pShowHiddenObjectsCheckBox,    1, 1, 1, 1);

    m_pTabWidget->addTab(m_pPreferencesTab, QApplication::translate("UIFileManager", "Preferences"));
}

void UIFileManagerPanel::sltListDirectoryCheckBoxToogled(bool bChecked)
{
    if (!m_pFileManagerOptions)
        return;
    m_pFileManagerOptions->fListDirectoriesOnTop = bChecked;
    emit sigOptionsChanged();
}

void UIFileManagerPanel::sltDeleteConfirmationCheckBoxToogled(bool bChecked)
{
    if (!m_pFileManagerOptions)
        return;
    m_pFileManagerOptions->fAskDeleteConfirmation = bChecked;
    emit sigOptionsChanged();
}

void UIFileManagerPanel::sltHumanReabableSizesCheckBoxToogled(bool bChecked)
{
    if (!m_pFileManagerOptions)
        return;
    m_pFileManagerOptions->fShowHumanReadableSizes = bChecked;
    emit sigOptionsChanged();
}

void UIFileManagerPanel::sltShowHiddenObjectsCheckBoxToggled(bool bChecked)
{
    if (!m_pFileManagerOptions)
        return;
    m_pFileManagerOptions->fShowHiddenObjects = bChecked;
    emit sigOptionsChanged();
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

void UIFileManagerPanel::update()
{
    if (!m_pFileManagerOptions)
        return;

    if (m_pListDirectoriesOnTopCheckBox)
    {
        m_pListDirectoriesOnTopCheckBox->blockSignals(true);
        m_pListDirectoriesOnTopCheckBox->setChecked(m_pFileManagerOptions->fListDirectoriesOnTop);
        m_pListDirectoriesOnTopCheckBox->blockSignals(false);
    }

    if (m_pDeleteConfirmationCheckBox)
    {
        m_pDeleteConfirmationCheckBox->blockSignals(true);
        m_pDeleteConfirmationCheckBox->setChecked(m_pFileManagerOptions->fAskDeleteConfirmation);
        m_pDeleteConfirmationCheckBox->blockSignals(false);
    }

    if (m_pHumanReabableSizesCheckBox)
    {
        m_pHumanReabableSizesCheckBox->blockSignals(true);
        m_pHumanReabableSizesCheckBox->setChecked(m_pFileManagerOptions->fShowHumanReadableSizes);
        m_pHumanReabableSizesCheckBox->blockSignals(false);
    }

    if (m_pShowHiddenObjectsCheckBox)
    {
        m_pShowHiddenObjectsCheckBox->blockSignals(true);
        m_pShowHiddenObjectsCheckBox->setChecked(m_pFileManagerOptions->fShowHiddenObjects);
        m_pShowHiddenObjectsCheckBox->blockSignals(false);
    }
}


#include "UIFileManagerPanel.moc"
