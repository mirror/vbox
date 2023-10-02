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


/** Defines the VM OS type ID. */
enum
{
    TypeID = Qt::UserRole + 1,
    FamilyID = Qt::UserRole + 2
};


UINameAndSystemEditor::UINameAndSystemEditor(QWidget *pParent,
                                             bool fChooseName /* = true */,
                                             bool fChoosePath /* = false */,
                                             bool fChooseImage /* = false */,
                                             bool fChooseEdition /* = false */,
                                             bool fChooseType /* = true */)
    : UIEditor(pParent)
    , m_fChooseName(fChooseName)
    , m_fChoosePath(fChoosePath)
    , m_fChooseImage(fChooseImage)
    , m_fChooseEdition(fChooseEdition)
    , m_fChooseType(fChooseType)
    , m_pLayout(0)
    , m_pLabelName(0)
    , m_pLabelPath(0)
    , m_pLabelImage(0)
    , m_pLabelEdition(0)
    , m_pLabelFamily(0)
    , m_pLabelType(0)
    , m_pIconType(0)
    , m_pLabelVariant(0)
    , m_pEditorName(0)
    , m_pSelectorPath(0)
    , m_pSelectorImage(0)
    , m_pComboEdition(0)
    , m_pComboFamily(0)
    , m_pComboType(0)
    , m_pComboVariant(0)
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
    if (m_pIconType)
        m_pIconType->setEnabled(fEnabled);
    if (m_pComboFamily)
        m_pComboFamily->setEnabled(fEnabled);
    if (m_pComboType)
        m_pComboType->setEnabled(fEnabled);
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
    AssertReturn(m_pComboFamily, false);

    int iFamilyComboIndex = -1;
    /* We already have to have an item in the family combo box for this family id: */
    for (int i = 0; i < m_pComboFamily->count() && iFamilyComboIndex == -1; ++i)
    {
        QString strComboFamilyId = m_pComboFamily->itemData(i, FamilyID).toString();
        if (!strComboFamilyId.isEmpty() && strComboFamilyId == uiCommon().guestOSTypeManager().getFamilyId(strTypeId))
            iFamilyComboIndex = i;
    }
    /* Bail out if family combo has no such item: */
    if (iFamilyComboIndex == -1)
        return false;
    /* Set the family combo's index. This will cause variant combo to be populated accordingly: */
    m_pComboFamily->setCurrentIndex(iFamilyComboIndex);

    /* If variant is not empty then try to select correct index. This will populate type combo: */
    QString strVariant = uiCommon().guestOSTypeManager().getVariant(strTypeId);
    if (!strVariant.isEmpty())
    {
        int index = -1;
        for (int i = 0; i < m_pComboVariant->count() && index == -1; ++i)
        {
            if (strVariant == m_pComboVariant->itemText(i))
                index = i;
        }
        if (index != -1)
            m_pComboVariant->setCurrentIndex(index);
        else
            return false;
    }

    /* At this point type combo should include the type we want to select: */
    int iTypeIndex = -1;
    for (int i = 0; i < m_pComboType->count() && iTypeIndex == -1; ++i)
    {
        if (strTypeId == m_pComboType->itemData(i, TypeID))
            iTypeIndex = i;
    }
    if (iTypeIndex != -1)
        m_pComboType->setCurrentIndex(iTypeIndex);
    return true;
}

QString UINameAndSystemEditor::typeId() const
{
    return m_strTypeId;
}

QString UINameAndSystemEditor::familyId() const
{
    if (!m_pComboFamily)
        return QString();
    return m_strFamilyId;
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

void UINameAndSystemEditor::setEditionSelectorEnabled(bool fEnabled)
{
    if (m_pComboEdition)
        m_pComboEdition->setEnabled(fEnabled);
    if (m_pLabelEdition)
        m_pLabelEdition->setEnabled(fEnabled);
}

bool UINameAndSystemEditor::isEditionsSelectorEmpty() const
{
    if (m_pComboEdition)
        return m_pComboEdition->count() == 0;
    return true;
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
    if (m_pLabelType)
        m_pLabelType->setText(tr("&Version:"));
    if (m_pLabelVariant)
        m_pLabelVariant->setText(tr("Kind:"));

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

void UINameAndSystemEditor::sltFamilyChanged(int index)
{
    AssertReturnVoid(m_pComboFamily);
    AssertReturnVoid(m_pComboVariant);

    m_strFamilyId = m_pComboFamily->itemData(index, FamilyID).toString();

    AssertReturnVoid(!m_strFamilyId.isEmpty());

    m_pComboVariant->blockSignals(true);
    m_pLabelVariant->setEnabled(true);

    m_pComboVariant->setEnabled(true);
    m_pComboVariant->clear();

    const QStringList variantList = uiCommon().guestOSTypeManager().getVariantListForFamilyId(m_strFamilyId);

    if (variantList.isEmpty())
    {
        m_pComboVariant->setEnabled(false);
        m_pLabelVariant->setEnabled(false);
        /* If variant list is empty the all the types of the family are added to typ selection combo: */
        populateTypeCombo(uiCommon().guestOSTypeManager().getTypeListForFamilyId(m_strFamilyId));
    }
    else
    {
        /* Populate variant combo: */
        /* If family is Linux then select Oracle Linux as variant: */
        int iOracleIndex = -1;
        foreach (const QString &strVariant, variantList)
        {
            m_pComboVariant->addItem(strVariant);
            if (strVariant.contains(QRegularExpression("Oracle.*Linux")))
                iOracleIndex = m_pComboVariant->count() - 1;
        }
        if (iOracleIndex != -1)
            m_pComboVariant->setCurrentIndex(iOracleIndex);

        populateTypeCombo(uiCommon().guestOSTypeManager().getTypeListForVariant(m_pComboVariant->currentText()));
    }
    m_pComboVariant->blockSignals(false);

    /* Notify listeners about this change: */
    emit sigOSFamilyChanged(m_strFamilyId);
}

void UINameAndSystemEditor::sltVariantChanged(const QString &strVariant)
{
    m_strVariant = strVariant;
    populateTypeCombo(uiCommon().guestOSTypeManager().getTypeListForVariant(strVariant));
}

void UINameAndSystemEditor::populateTypeCombo(const UIGuestOSTypeManager::UIGuestOSTypeInfo &typeList)
{
    AssertReturnVoid(m_pComboType);
    AssertReturnVoid(!typeList.isEmpty());

    m_pComboType->blockSignals(true);
    m_pComboType->clear();
    for (int i = 0; i < typeList.size(); ++i)
    {
        m_pComboType->addItem(typeList[i].second);
        m_pComboType->setItemData(i, typeList[i].first, TypeID);
    }
    m_pComboType->blockSignals(false);
    selectPreferredType();
    sltTypeChanged(m_pComboType->currentIndex());
}

void UINameAndSystemEditor::selectPreferredType()
{
    if (m_strFamilyId == "Windows")
    {
        QString strDefaultID = "Windows11_x64";
        const int iIndexWin = m_pComboType->findData(strDefaultID, TypeID);
        if (iIndexWin != -1)
        {
            m_pComboType->setCurrentIndex(iIndexWin);
            return;
        }
    }
    /* Or select Oracle Linux item for Linux family as default: */
    if (m_strVariant == "Oracle")
    {
        QString strDefaultID = "Oracle_x64";
        const int iIndexOracle = m_pComboType->findData(strDefaultID, TypeID);
        if (iIndexOracle != -1)
        {
            m_pComboType->setCurrentIndex(iIndexOracle);
            return;
        }
    }

    /* Else try to pick the first 64-bit one if it exists.: */
    QString strDefaultID = "_x64";
    const int iIndexAll = m_pComboType->findData(strDefaultID, TypeID, Qt::MatchContains);
    if (iIndexAll != -1)
        m_pComboType->setCurrentIndex(iIndexAll);
    else
        m_pComboType->setCurrentIndex(0);
}

void UINameAndSystemEditor::sltTypeChanged(int iIndex)
{
    AssertPtrReturnVoid(m_pComboType);

    /* Acquire type ID: */
    m_strTypeId = m_pComboType->itemData(iIndex, TypeID).toString();

    /* Update selected type pixmap: */
    m_pIconType->setPixmap(generalIconPool().guestOSTypePixmapDefault(m_strTypeId));

    /* Save the most recently used item: */
    m_currentIds[m_strFamilyId] = m_strTypeId;

    /* Notifies listeners about OS type change: */
    emit sigOsTypeChanged();
}

void UINameAndSystemEditor::sltSelectedEditionsChanged(int)
{
    emit sigEditionChanged(selectedEditionIndex());
}

void UINameAndSystemEditor::prepare()
{
    prepareThis();
    prepareWidgets();
    prepareConnections();
    retranslateUi();
}

void UINameAndSystemEditor::prepareThis()
{
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

            /* Prepare VM OS variant label: */
            m_pLabelVariant = new QLabel(this);
            if (m_pLabelVariant)
            {
                m_pLabelVariant->setAlignment(Qt::AlignRight);
                m_pLabelVariant->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

                m_pLayout->addWidget(m_pLabelVariant, iRow, 0);
            }
            /* Prepare VM OS variant combo: */
            m_pComboVariant = new QComboBox(this);
            if (m_pComboVariant)
            {
                m_pLabelFamily->setBuddy(m_pComboVariant);
                m_pLayout->addWidget(m_pComboVariant, iRow, 1);
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
            prepareFamilyCombo();
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

void UINameAndSystemEditor::prepareFamilyCombo()
{
    AssertPtrReturnVoid(m_pComboFamily);

    /* Acquire family IDs: */
    const UIGuestOSTypeManager::UIGuestOSTypeFamilyInfo &families = uiCommon().guestOSTypeManager().getFamilies();

    /* For each known family ID: */
    for (int i = 0; i < families.size(); ++i)
    {
        m_pComboFamily->addItem(families[i].second);
        m_pComboFamily->setItemData(i, families[i].first, FamilyID);
    }

    /* Choose the 1st item to be the current: */
    m_pComboFamily->setCurrentIndex(0);
    /* And update the linked widgets accordingly: */
    sltFamilyChanged(m_pComboFamily->currentIndex());
}

void UINameAndSystemEditor::prepareConnections()
{
    /* Prepare connections: */
    if (m_pEditorName)
        connect(m_pEditorName, &UIMarkableLineEdit::textChanged,
                this, &UINameAndSystemEditor::sigNameChanged);
    if (m_pSelectorPath)
        connect(m_pSelectorPath, &UIFilePathSelector::pathChanged,
                this, &UINameAndSystemEditor::sigPathChanged);
    if (m_pComboFamily)
        connect(m_pComboFamily, &QComboBox::currentIndexChanged,
                this, &UINameAndSystemEditor::sltFamilyChanged);
    if (m_pComboVariant)
        connect(m_pComboVariant, &QComboBox::currentTextChanged,
                this, &UINameAndSystemEditor::sltVariantChanged);
    if (m_pComboType)
        connect(m_pComboType, &QComboBox::currentIndexChanged,
                this, &UINameAndSystemEditor::sltTypeChanged);
    if (m_pSelectorImage)
        connect(m_pSelectorImage, &UIFilePathSelector::pathChanged,
                this, &UINameAndSystemEditor::sigImageChanged);
    if (m_pComboEdition)
        connect(m_pComboEdition, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &UINameAndSystemEditor::sltSelectedEditionsChanged);
}

ulong UINameAndSystemEditor::selectedEditionIndex() const
{
    if (!m_pComboEdition || m_pComboEdition->count() == 0)
        return 0;
    return m_pComboEdition->currentData().value<ulong>();
}
