/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoConfigurationPanel class implementation.
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
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>

/* GUI includes: */
#include "UIIconPool.h"
#include "QILabel.h"
#include "QILineEdit.h"
#include "QIToolButton.h"
#include "UIVisoConfigurationPanel.h"

UIVisoConfigurationPanel::UIVisoConfigurationPanel(QWidget *pParent /* =0 */)
    : UIDialogPanel(pParent)
    , m_pVisoNameLabel(0)
    , m_pCustomOptionsLabel(0)
    , m_pVisoNameLineEdit(0)
    , m_pCustomOptionsLineEdit(0)
    , m_pShowHiddenObjectsCheckBox(0)
    , m_pShowHiddenObjectsLabel(0)
{
    prepareObjects();
    prepareConnections();
}

UIVisoConfigurationPanel::~UIVisoConfigurationPanel()
{
}

QString UIVisoConfigurationPanel::panelName() const
{
    return "ConfigurationPanel";
}

void UIVisoConfigurationPanel::setVisoName(const QString& strVisoName)
{
    if (m_pVisoNameLineEdit)
        m_pVisoNameLineEdit->setText(strVisoName);
}

void UIVisoConfigurationPanel::setVisoCustomOptions(const QStringList& visoCustomOptions)
{
    if (!m_pCustomOptionsLineEdit)
        return;
    m_pCustomOptionsLineEdit->clear();
    m_pCustomOptionsLineEdit->setText(visoCustomOptions.join(";"));
}

void UIVisoConfigurationPanel::setShowHiddenbjects(bool fShow)
{
    if (m_pShowHiddenObjectsCheckBox)
        m_pShowHiddenObjectsCheckBox->setChecked(fShow);
}

void UIVisoConfigurationPanel::prepareObjects()
{
    if (!mainLayout())
        return;

    /* Name edit and and label: */
    m_pVisoNameLabel = new QILabel(QApplication::translate("UIVisoCreatorWidget", "VISO Name:"));
    m_pVisoNameLineEdit = new QILineEdit;
    if (m_pVisoNameLabel && m_pVisoNameLineEdit)
    {
        m_pVisoNameLabel->setBuddy(m_pVisoNameLineEdit);
        mainLayout()->addWidget(m_pVisoNameLabel, 0, Qt::AlignLeft);
        mainLayout()->addWidget(m_pVisoNameLineEdit, 0, Qt::AlignLeft);
    }

    addVerticalSeparator();

    /* Cutom Viso options stuff: */
    m_pCustomOptionsLabel = new QILabel(QApplication::translate("UIVisoCreatorWidget", "Custom VISO options:"));
    m_pCustomOptionsLineEdit = new QILineEdit;

    if (m_pCustomOptionsLabel && m_pCustomOptionsLineEdit)
    {
        m_pCustomOptionsLabel->setBuddy(m_pCustomOptionsLineEdit);

        mainLayout()->addWidget(m_pCustomOptionsLabel, 0, Qt::AlignLeft);
        mainLayout()->addWidget(m_pCustomOptionsLineEdit, Qt::AlignLeft);
    }


    m_pShowHiddenObjectsCheckBox = new QCheckBox;
    m_pShowHiddenObjectsLabel = new QILabel(QApplication::translate("UIVisoCreatorWidget", "Show Hidden Objects"));
    m_pShowHiddenObjectsLabel->setBuddy(m_pShowHiddenObjectsCheckBox);
    mainLayout()->addWidget(m_pShowHiddenObjectsCheckBox, 0, Qt::AlignLeft);
    mainLayout()->addWidget(m_pShowHiddenObjectsLabel, 0, Qt::AlignLeft);
    mainLayout()->addStretch(6);


    retranslateUi();
}

void UIVisoConfigurationPanel::prepareConnections()
{
    if (m_pVisoNameLineEdit)
        connect(m_pVisoNameLineEdit, &QILineEdit::editingFinished, this, &UIVisoConfigurationPanel::sltVisoNameChanged);
    if (m_pCustomOptionsLabel)
        connect(m_pCustomOptionsLineEdit, &QILineEdit::editingFinished, this, &UIVisoConfigurationPanel::sltCustomOptionsEdited);
    if (m_pShowHiddenObjectsCheckBox)
        connect(m_pShowHiddenObjectsCheckBox, &QCheckBox::stateChanged,
                this, &UIVisoConfigurationPanel::sltShowHiddenObjectsChange);
}

void UIVisoConfigurationPanel::retranslateUi()
{
    if (m_pVisoNameLabel)
        m_pVisoNameLabel->setText(QApplication::translate("UIVisoCreatorWidget", "VISO Name:"));
    if (m_pCustomOptionsLabel)
        m_pCustomOptionsLabel->setText(QApplication::translate("UIVisoCreatorWidget", "Custom VISO options:"));

    if (m_pVisoNameLineEdit)
        m_pVisoNameLineEdit->setToolTip(QApplication::translate("UIVisoCreatorWidget", "Holds the name of the VISO medium."));
    if (m_pCustomOptionsLineEdit)
        m_pCustomOptionsLineEdit->setToolTip(QApplication::translate("UIVisoCreatorWidget", "The list of suctom options delimited with ';'."));
    if (m_pShowHiddenObjectsLabel)
        m_pShowHiddenObjectsLabel->setText(QApplication::translate("UIVisoCreatorWidget", "Show Hidden Objects"));
    if (m_pShowHiddenObjectsCheckBox)
        m_pShowHiddenObjectsCheckBox->setToolTip(QApplication::translate("UIVisoCreatorWidget", "When checked, "
                                                                         "multiple hidden objects are shown in the file browser"));
}

void UIVisoConfigurationPanel::sltCustomOptionsEdited()
{
    if (!m_pCustomOptionsLineEdit)
        return;
    QStringList customVisoOptions = m_pCustomOptionsLineEdit->text().split(";");
    if (!customVisoOptions.isEmpty())
        emit sigCustomVisoOptionsChanged(customVisoOptions);
}

void UIVisoConfigurationPanel::sltVisoNameChanged()
{
    if (m_pVisoNameLineEdit)
        emit sigVisoNameChanged(m_pVisoNameLineEdit->text());
}

void UIVisoConfigurationPanel::sltShowHiddenObjectsChange(int iState)
{
    if (iState == static_cast<int>(Qt::Checked))
        sigShowHiddenObjects(true);
    else
        sigShowHiddenObjects(false);
}
