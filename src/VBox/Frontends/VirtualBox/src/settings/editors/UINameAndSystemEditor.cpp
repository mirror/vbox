/* $Id$ */
/** @file
 * VBox Qt GUI - UINameAndSystemEditor class implementation.
 */

/*
 * Copyright (C) 2008-2023 Oracle and/or its affiliates.
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
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILineEdit.h"
#include "UICommon.h"
#include "UIIconPool.h"
#include "UIFilePathSelector.h"
#include "UINameAndSystemEditor.h"

/* COM includes: */
#include "CSystemProperties.h"
#include <VBox/com/VirtualBox.h> /* Need GUEST_OS_ID_STR_X86 and friends. */


UINameAndSystemEditor::UINameAndSystemEditor(QWidget *pParent,
                                             bool fChooseName /* = true */,
                                             bool fChoosePath /* = false */,
                                             bool fChooseImage /* = false */,
                                             bool fChooseEdition /* = false */,
                                             bool fChooseType /* = true */)
    : UIEditor(pParent, true /* show in basic mode? */)
    // options
    , m_fChooseName(fChooseName)
    , m_fChoosePath(fChoosePath)
    , m_fChooseImage(fChooseImage)
    , m_fChooseEdition(fChooseEdition)
    , m_fChooseType(fChooseType)
    // widgets
    , m_pLayout(0)
    // widgets: name
    , m_pLabelName(0)
    , m_pEditorName(0)
    // widgets: path
    , m_pLabelPath(0)
    , m_pSelectorPath(0)
    // widgets: image
    , m_pLabelImage(0)
    , m_pSelectorImage(0)
    // widgets: edition
    , m_pLabelEdition(0)
    , m_pComboEdition(0)
    // widgets/ family, subtype, type
    , m_pLabelFamily(0)
    , m_pComboFamily(0)
    , m_pLabelSubtype(0)
    , m_pComboSubtype(0)
    , m_pLabelType(0)
    , m_pComboType(0)
    , m_pIconType(0)
{
    prepare();
}

void UINameAndSystemEditor::setMinimumLayoutIndent(int iIndent)
{
    if (m_pLayout)
        m_pLayout->setColumnMinimumWidth(0, iIndent);
}

void UINameAndSystemEditor::setNameStuffEnabled(bool fEnabled)
{
    if (m_pLabelName)
        m_pLabelName->setEnabled(fEnabled);
    if (m_pEditorName)
        m_pEditorName->setEnabled(fEnabled);
}

void UINameAndSystemEditor::setPathStuffEnabled(bool fEnabled)
{
    if (m_pLabelPath)
        m_pLabelPath->setEnabled(fEnabled);
    if (m_pSelectorPath)
        m_pSelectorPath->setEnabled(fEnabled);
}

void UINameAndSystemEditor::setOSTypeStuffEnabled(bool fEnabled)
{
    if (m_pLabelFamily)
        m_pLabelFamily->setEnabled(fEnabled);
    if (m_pLabelType)
        m_pLabelType->setEnabled(fEnabled);
    if (m_pComboFamily)
        m_pComboFamily->setEnabled(fEnabled);
    if (m_pComboType)
        m_pComboType->setEnabled(fEnabled);
    if (m_pIconType)
        m_pIconType->setEnabled(fEnabled);
}

void UINameAndSystemEditor::setEditionSelectorEnabled(bool fEnabled)
{
    if (m_pLabelEdition)
        m_pLabelEdition->setEnabled(fEnabled);
    if (m_pComboEdition)
        m_pComboEdition->setEnabled(fEnabled);
}

bool UINameAndSystemEditor::isEditionsSelectorEmpty() const
{
    if (m_pComboEdition)
        return m_pComboEdition->count() == 0;
    return true;
}

void UINameAndSystemEditor::setName(const QString &strName)
{
    if (!m_pEditorName)
        return;
    m_pEditorName->setText(strName);
}

QString UINameAndSystemEditor::name() const
{
    if (!m_pEditorName)
        return QString();
    return m_pEditorName->text();
}

void UINameAndSystemEditor::setPath(const QString &strPath)
{
    if (!m_pSelectorPath)
        return;
    m_pSelectorPath->setPath(strPath);
}

QString UINameAndSystemEditor::path() const
{
    if (!m_pSelectorPath)
        return uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    return m_pSelectorPath->path();
}

void UINameAndSystemEditor::setISOImagePath(const QString &strPath)
{
    if (m_pSelectorImage)
        m_pSelectorImage->setPath(strPath);
    emit sigImageChanged(strPath);
}

QString UINameAndSystemEditor::ISOImagePath() const
{
    if (!m_pSelectorImage)
        return QString();
    return m_pSelectorImage->path();
}

bool UINameAndSystemEditor::setGuestOSTypeByTypeId(const QString &strTypeId)
{
    /* Sanity check: */
    AssertPtrReturn(m_pComboFamily, false);

    int iFamilyComboIndex = -1;
    /* We already have to have an item in the family combo box for this family id: */
    for (int i = 0; i < m_pComboFamily->count() && iFamilyComboIndex == -1; ++i)
    {
        QString strComboFamilyId = m_pComboFamily->itemData(i).toString();
        if (!strComboFamilyId.isEmpty() && strComboFamilyId == uiCommon().guestOSTypeManager().getFamilyId(strTypeId))
            iFamilyComboIndex = i;
    }
    /* Bail out if family combo has no such item: */
    if (iFamilyComboIndex == -1)
        return false;
    /* Set the family combo's index. This will cause subtype combo to be populated accordingly: */
    m_pComboFamily->setCurrentIndex(iFamilyComboIndex);

    /* If subtype is not empty then try to select correct index. This will populate type combo: */
    QString strSubtype = uiCommon().guestOSTypeManager().getSubtype(strTypeId);
    if (!strSubtype.isEmpty())
    {
        int index = -1;
        for (int i = 0; i < m_pComboSubtype->count() && index == -1; ++i)
        {
            if (strSubtype == m_pComboSubtype->itemText(i))
                index = i;
        }
        if (index != -1)
            m_pComboSubtype->setCurrentIndex(index);
        else
            return false;
    }

    /* At this point type combo should include the type we want to select: */
    int iTypeIndex = -1;
    for (int i = 0; i < m_pComboType->count() && iTypeIndex == -1; ++i)
    {
        if (strTypeId == m_pComboType->itemData(i))
            iTypeIndex = i;
    }
    if (iTypeIndex != -1)
        m_pComboType->setCurrentIndex(iTypeIndex);
    return true;
}

void UINameAndSystemEditor::markNameEditor(bool fError)
{
    if (m_pEditorName)
        m_pEditorName->mark(fError, fError ? tr("Invalid name") : QString("Name is valid"));
}

void UINameAndSystemEditor::markImageEditor(bool fError, const QString &strErrorMessage)
{
    if (m_pSelectorImage)
        m_pSelectorImage->mark(fError, strErrorMessage);
}

void UINameAndSystemEditor::setEditionNameAndIndices(const QVector<QString> &names, const QVector<ulong> &ids)
{
    AssertReturnVoid(m_pComboEdition && names.size() == ids.size());
    m_pComboEdition->clear();
    for (int i = 0; i < names.size(); ++i)
        m_pComboEdition->addItem(names[i], QVariant::fromValue(ids[i]) /* user data */);
}

int UINameAndSystemEditor::firstColumnWidth() const
{
    int iWidth = 0;
    if (m_pLabelName)
        iWidth = qMax(iWidth, m_pLabelName->width());
    if (m_pLabelPath)
        iWidth = qMax(iWidth, m_pLabelPath->width());
    if (m_pLabelImage)
        iWidth = qMax(iWidth, m_pLabelImage->width());
    if (m_pLabelEdition)
        iWidth = qMax(iWidth, m_pLabelEdition->width());
    if (m_pLabelFamily)
        iWidth = qMax(iWidth, m_pLabelFamily->width());
    if (m_pLabelType)
        iWidth = qMax(iWidth, m_pLabelType->width());
    return iWidth;
}

void UINameAndSystemEditor::retranslateUi()
{
    if (m_pLabelName)
        m_pLabelName->setText(tr("&Name:"));
    if (m_pLabelPath)
        m_pLabelPath->setText(tr("&Folder:"));
    if (m_pLabelImage)
        m_pLabelImage->setText(tr("&ISO Image:"));
    if (m_pLabelEdition)
        m_pLabelEdition->setText(tr("&Edition:"));
    if (m_pLabelFamily)
        m_pLabelFamily->setText(tr("&Type:"));
    if (m_pLabelSubtype)
        m_pLabelSubtype->setText(tr("&Subtype:"));
    if (m_pLabelType)
        m_pLabelType->setText(tr("&Version:"));

    if (m_pEditorName)
        m_pEditorName->setToolTip(tr("Holds the name for virtual machine."));
    if (m_pSelectorPath)
        m_pSelectorPath->setToolTip(tr("Selects the folder hosting virtual machine."));
    if (m_pComboFamily)
        m_pComboFamily->setToolTip(tr("Selects the operating system family that "
                                      "you plan to install into this virtual machine."));
    if (m_pComboType)
        m_pComboType->setToolTip(tr("Selects the operating system type that "
                                    "you plan to install into this virtual machine "
                                    "(called a guest operating system)."));
    if (m_pSelectorImage)
        m_pSelectorImage->setToolTip(tr("Selects an ISO image to be attached to the "
                                        "virtual machine or used in unattended install."));
}

void UINameAndSystemEditor::sltSelectedEditionsChanged(int)
{
    emit sigEditionChanged(selectedEditionIndex());
}

void UINameAndSystemEditor::sltFamilyChanged(int iIndex)
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pComboFamily);
    AssertPtrReturnVoid(m_pComboSubtype);

    /* Acquire new family ID: */
    m_strFamilyId = m_pComboFamily->itemData(iIndex).toString();
    AssertReturnVoid(!m_strFamilyId.isEmpty());

    m_pComboSubtype->blockSignals(true);
    m_pLabelSubtype->setEnabled(true);

    m_pComboSubtype->setEnabled(true);
    m_pComboSubtype->clear();

    const QStringList subtypeList = uiCommon().guestOSTypeManager().getSubtypeListForFamilyId(m_strFamilyId);

    if (subtypeList.isEmpty())
    {
        m_pComboSubtype->setEnabled(false);
        m_pLabelSubtype->setEnabled(false);
        /* If subtype list is empty the all the types of the family are added to typ selection combo: */
        populateTypeCombo(uiCommon().guestOSTypeManager().getTypeListForFamilyId(m_strFamilyId));
    }
    else
    {
        /* Populate subtype combo: */
        /* If family is Linux then select Oracle Linux as subtype: */
        int iOracleIndex = -1;
        foreach (const QString &strSubtype, subtypeList)
        {
            m_pComboSubtype->addItem(strSubtype);
            if (strSubtype.contains(QRegularExpression("Oracle.*Linux")))
                iOracleIndex = m_pComboSubtype->count() - 1;
        }
        if (iOracleIndex != -1)
            m_pComboSubtype->setCurrentIndex(iOracleIndex);

        populateTypeCombo(uiCommon().guestOSTypeManager().getTypeListForSubtype(m_pComboSubtype->currentText()));
    }
    m_pComboSubtype->blockSignals(false);

    /* Notify listeners about this change: */
    emit sigOSFamilyChanged(m_strFamilyId);
}

void UINameAndSystemEditor::sltSubtypeChanged(const QString &strSubtype)
{
    /* Save new subtype: */
    m_strSubtype = strSubtype;

    /* Populate type combo: */
    populateTypeCombo(uiCommon().guestOSTypeManager().getTypeListForSubtype(strSubtype));
}

void UINameAndSystemEditor::sltTypeChanged(int iIndex)
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pComboType);

    /* Acquire new type ID: */
    m_strTypeId = m_pComboType->itemData(iIndex).toString();
    AssertReturnVoid(!m_strTypeId.isEmpty());

    /* Update selected type pixmap: */
    m_pIconType->setPixmap(generalIconPool().guestOSTypePixmapDefault(m_strTypeId));

    /* Save the most recently used item: */
    m_currentIds[m_strFamilyId] = m_strTypeId;

    /* Notifies listeners about OS type change: */
    emit sigOsTypeChanged();
}

void UINameAndSystemEditor::prepare()
{
    prepareWidgets();
    prepareConnections();
    retranslateUi();
}

void UINameAndSystemEditor::prepareWidgets()
{
    /* Prepare main-layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        m_pLayout->setContentsMargins(0, 0, 0, 0);
        m_pLayout->setColumnStretch(0, 0);
        m_pLayout->setColumnStretch(1, 1);

        int iRow = 0;

        if (m_fChooseName)
        {
            /* Prepare name label: */
            m_pLabelName = new QLabel(this);
            if (m_pLabelName)
            {
                m_pLabelName->setAlignment(Qt::AlignRight);
                m_pLabelName->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelName, iRow, 0);
            }
            /* Prepare name editor: */
            m_pEditorName = new UIMarkableLineEdit(this);
            if (m_pEditorName)
            {
                m_pLabelName->setBuddy(m_pEditorName);
                m_pLayout->addWidget(m_pEditorName, iRow, 1, 1, 2);
            }
            ++iRow;
        }

        if (m_fChoosePath)
        {
            /* Prepare path label: */
            m_pLabelPath = new QLabel(this);
            if (m_pLabelPath)
            {
                m_pLabelPath->setAlignment(Qt::AlignRight);
                m_pLabelPath->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelPath, iRow, 0);
            }
            /* Prepare path selector: */
            m_pSelectorPath = new UIFilePathSelector(this);
            if (m_pSelectorPath)
            {
                m_pLabelPath->setBuddy(m_pSelectorPath->focusProxy());
                QString strDefaultMachineFolder = uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
                m_pSelectorPath->setPath(strDefaultMachineFolder);
                m_pSelectorPath->setDefaultPath(strDefaultMachineFolder);
                m_pLayout->addWidget(m_pSelectorPath, iRow, 1, 1, 2);
            }
            ++iRow;
        }

        if (m_fChooseImage)
        {
            /* Prepare image label: */
            m_pLabelImage = new QLabel(this);
            if (m_pLabelImage)
            {
                m_pLabelImage->setAlignment(Qt::AlignRight);
                m_pLabelImage->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelImage, iRow, 0);
            }
            /* Prepare image selector: */
            m_pSelectorImage = new UIFilePathSelector(this);
            if (m_pSelectorImage)
            {
                m_pLabelImage->setBuddy(m_pSelectorImage->focusProxy());
                m_pSelectorImage->setResetEnabled(false);
                m_pSelectorImage->setMode(UIFilePathSelector::Mode_File_Open);
                m_pSelectorImage->setFileDialogFilters("ISO Images(*.iso *.ISO)");
                m_pSelectorImage->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
                m_pSelectorImage->setRecentMediaListType(UIMediumDeviceType_DVD);
                m_pLayout->addWidget(m_pSelectorImage, iRow, 1, 1, 2);
            }
            ++iRow;
        }

        if (m_fChooseEdition)
        {
            /* Prepare edition label: */
            m_pLabelEdition = new QLabel(this);
            if (m_pLabelEdition)
            {
                m_pLabelEdition->setAlignment(Qt::AlignRight);
                m_pLabelEdition->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelEdition, iRow, 0);
            }
            /* Prepare edition combo: */
            m_pComboEdition = new QComboBox(this);
            if (m_pComboEdition)
            {
                m_pLabelEdition->setBuddy(m_pComboEdition);
                m_pLayout->addWidget(m_pComboEdition, iRow, 1, 1, 2);
            }
            ++iRow;
        }

        if (m_fChooseType)
        {
            const int iIconRow = iRow;

            /* Prepare VM OS family label: */
            m_pLabelFamily = new QLabel(this);
            if (m_pLabelFamily)
            {
                m_pLabelFamily->setAlignment(Qt::AlignRight);
                m_pLabelFamily->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelFamily, iRow, 0);
            }
            /* Prepare VM OS family combo: */
            m_pComboFamily = new QComboBox(this);
            if (m_pComboFamily)
            {
                m_pLabelFamily->setBuddy(m_pComboFamily);
                m_pLayout->addWidget(m_pComboFamily, iRow, 1);
            }
            ++iRow;

            /* Prepare VM OS subtype label: */
            m_pLabelSubtype = new QLabel(this);
            if (m_pLabelSubtype)
            {
                m_pLabelSubtype->setAlignment(Qt::AlignRight);
                m_pLabelSubtype->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelSubtype, iRow, 0);
            }
            /* Prepare VM OS subtype combo: */
            m_pComboSubtype = new QComboBox(this);
            if (m_pComboSubtype)
            {
                m_pLabelSubtype->setBuddy(m_pComboSubtype);
                m_pLayout->addWidget(m_pComboSubtype, iRow, 1);
            }
            ++iRow;

            /* Prepare VM OS type label: */
            m_pLabelType = new QLabel(this);
            if (m_pLabelType)
            {
                m_pLabelType->setAlignment(Qt::AlignRight);
                m_pLabelType->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                m_pLayout->addWidget(m_pLabelType, iRow, 0);
            }
            /* Prepare VM OS type combo: */
            m_pComboType = new QComboBox(this);
            if (m_pComboType)
            {
                m_pLabelType->setBuddy(m_pComboType);
                m_pLayout->addWidget(m_pComboType, iRow, 1);
            }
            ++iRow;

            /* Prepare sub-layout: */
            QVBoxLayout *pLayoutIcon = new QVBoxLayout;
            if (pLayoutIcon)
            {
                /* Prepare VM OS type icon: */
                m_pIconType = new QLabel(this);
                if (m_pIconType)
                {
                    m_pIconType->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                    pLayoutIcon->addWidget(m_pIconType);
                }

                /* Add stretch to sub-layout: */
                pLayoutIcon->addStretch();

                /* Add into layout: */
                m_pLayout->addLayout(pLayoutIcon, iIconRow, 2, 3, 1);
            }

            /* Initialize VM OS family combo
             * after all widgets were created: */
            populateFamilyCombo();
        }
    }
    /* Set top most widget of the 2nd column as focus proxy: */
    for (int i = 0; i < m_pLayout->rowCount(); ++i)
    {
        QLayoutItem *pItem = m_pLayout->itemAtPosition(i, 1);
        if (pItem && pItem->widget())
        {
            setFocusProxy(pItem->widget());
            break;
        }
    }
}

void UINameAndSystemEditor::prepareConnections()
{
    if (m_pEditorName)
        connect(m_pEditorName, &UIMarkableLineEdit::textChanged,
                this, &UINameAndSystemEditor::sigNameChanged);
    if (m_pSelectorPath)
        connect(m_pSelectorPath, &UIFilePathSelector::pathChanged,
                this, &UINameAndSystemEditor::sigPathChanged);
    if (m_pSelectorImage)
        connect(m_pSelectorImage, &UIFilePathSelector::pathChanged,
                this, &UINameAndSystemEditor::sigImageChanged);
    if (m_pComboEdition)
        connect(m_pComboEdition, &QComboBox::currentIndexChanged,
                this, &UINameAndSystemEditor::sltSelectedEditionsChanged);
    if (m_pComboFamily)
        connect(m_pComboFamily, &QComboBox::currentIndexChanged,
                this, &UINameAndSystemEditor::sltFamilyChanged);
    if (m_pComboSubtype)
        connect(m_pComboSubtype, &QComboBox::currentTextChanged,
                this, &UINameAndSystemEditor::sltSubtypeChanged);
    if (m_pComboType)
        connect(m_pComboType, &QComboBox::currentIndexChanged,
                this, &UINameAndSystemEditor::sltTypeChanged);
}

ulong UINameAndSystemEditor::selectedEditionIndex() const
{
    /* Sanity check: */
    AssertPtrReturn(m_pComboEdition, 0);

    return m_pComboEdition->count() == 0 ? 0 : m_pComboEdition->currentData().value<ulong>();
}

void UINameAndSystemEditor::populateFamilyCombo()
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pComboFamily);

    /* Acquire family IDs: */
    const UIGuestOSTypeManager::UIGuestOSTypeFamilyInfo &families = uiCommon().guestOSTypeManager().getFamilies();

    /* Populate family combo: */
    for (int i = 0; i < families.size(); ++i)
    {
        m_pComboFamily->addItem(families[i].second);
        m_pComboFamily->setItemData(i, families[i].first);
    }

    /* Select 1st OS family: */
    m_pComboFamily->setCurrentIndex(0);

    /* Trigger family change handler manually: */
    sltFamilyChanged(m_pComboFamily->currentIndex());
}

void UINameAndSystemEditor::populateTypeCombo(const UIGuestOSTypeManager::UIGuestOSTypeInfo &types)
{
    /* Sanity check: */
    AssertPtrReturnVoid(m_pComboType);
    AssertReturnVoid(!types.isEmpty());

    /* Block signals initially and clear the combo: */
    m_pComboType->blockSignals(true);
    m_pComboType->clear();

    /* Populate type combo: */
    for (int i = 0; i < types.size(); ++i)
    {
        m_pComboType->addItem(types[i].second);
        m_pComboType->setItemData(i, types[i].first);
    }

    /* Unblock signals finally: */
    m_pComboType->blockSignals(false);

    /* Select preferred OS type: */
    selectPreferredType();

    /* Trigger type change handler manually: */
    sltTypeChanged(m_pComboType->currentIndex());
}

void UINameAndSystemEditor::selectPreferredType()
{
    /* Windows 11 for Windows family: */
    if (m_strFamilyId == "Windows")
    {
        const QString strDefaultID = GUEST_OS_ID_STR_X64("Windows11");
        const int iIndexWin11 = m_pComboType->findData(strDefaultID);
        if (iIndexWin11 != -1)
        {
            m_pComboType->setCurrentIndex(iIndexWin11);
            return;
        }
    }
    /* Oracle Linux for Oracle subtype: */
    else if (m_strSubtype == "Oracle")
    {
        const QString strDefaultID = GUEST_OS_ID_STR_X64("Oracle");
        const int iIndexOracle = m_pComboType->findData(strDefaultID);
        if (iIndexOracle != -1)
        {
            m_pComboType->setCurrentIndex(iIndexOracle);
            return;
        }
    }
    else
    {
        /* Else try to pick the first 64-bit one if it exists: */
        const QString strDefaultID = GUEST_OS_ID_STR_X64("");
        const int iIndexAll = m_pComboType->findData(strDefaultID, Qt::UserRole, Qt::MatchContains);
        if (iIndexAll != -1)
            m_pComboType->setCurrentIndex(iIndexAll);
        else
            m_pComboType->setCurrentIndex(0);
    }
}
