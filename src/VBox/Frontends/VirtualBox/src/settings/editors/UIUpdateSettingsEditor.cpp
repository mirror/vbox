/* $Id$ */
/** @file
 * VBox Qt GUI - UIUpdateSettingsEditor class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>

/* GUI includes: */
#include "UIUpdateSettingsEditor.h"


UIUpdateSettingsEditor::UIUpdateSettingsEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pCheckBox(0)
    , m_pWidgetUpdateSettings(0)
    , m_pLabelUpdatePeriod(0)
    , m_pComboUpdatePeriod(0)
    , m_pLabelUpdateDate(0)
    , m_pFieldUpdateDate(0)
    , m_pLabelUpdateFilter(0)
    , m_pRadioButtonGroup(0)
{
    prepare();
}

void UIUpdateSettingsEditor::setValue(const VBoxUpdateData &guiValue)
{
    /* Update value if changed: */
    if (m_guiValue != guiValue)
    {
        /* Update value itself: */
        m_guiValue = guiValue;

        /* Update check-box if present: */
        if (m_pCheckBox)
        {
            m_pCheckBox->setChecked(!m_guiValue.isNoNeedToCheck());

            if (m_pCheckBox->isChecked())
            {
                /* Update period combo if present: */
                if (m_pComboUpdatePeriod)
                    m_pComboUpdatePeriod->setCurrentIndex(m_guiValue.periodIndex());

                /* Update branch radio-buttons if present: */
                if (m_mapRadioButtons.value(m_guiValue.branchIndex()))
                    m_mapRadioButtons.value(m_guiValue.branchIndex())->setChecked(true);
            }

            /* Update other related widgets: */
            sltHandleUpdateToggle(m_pCheckBox->isChecked());
        }
    }
}

VBoxUpdateData UIUpdateSettingsEditor::value() const
{
    return m_pCheckBox ? VBoxUpdateData(periodType(), branchType()) : m_guiValue;
}

void UIUpdateSettingsEditor::retranslateUi()
{
    /* Translate check-box: */
    if (m_pCheckBox)
    {
        m_pCheckBox->setToolTip(tr("When checked, the application will periodically connect to the VirtualBox "
                                   "website and check whether a new VirtualBox version is available."));
        m_pCheckBox->setText(tr("&Check for Updates"));
    }

    /* Translate period widgets: */
    if (m_pLabelUpdatePeriod)
        m_pLabelUpdatePeriod->setText(tr("&Once per:"));
    if (m_pComboUpdatePeriod)
    {
        m_pComboUpdatePeriod->setToolTip(tr("Selects how often the new version check should be performed."));
        const int iCurrenIndex = m_pComboUpdatePeriod->currentIndex();
        m_pComboUpdatePeriod->clear();
        VBoxUpdateData::populate();
        m_pComboUpdatePeriod->insertItems(0, VBoxUpdateData::list());
        m_pComboUpdatePeriod->setCurrentIndex(iCurrenIndex == -1 ? 0 : iCurrenIndex);
    }
    if (m_pLabelUpdateDate)
        m_pLabelUpdateDate->setText(tr("Next Check:"));
    if (m_pLabelUpdateFilter)
        m_pLabelUpdateFilter->setText(tr("Check for:"));

    /* Translate branch widgets: */
    if (m_mapRadioButtons.value(VBoxUpdateData::BranchStable))
    {
        m_mapRadioButtons.value(VBoxUpdateData::BranchStable)->setToolTip(tr("When chosen, you will be notified "
                                                                             "about stable updates to VirtualBox."));
        m_mapRadioButtons.value(VBoxUpdateData::BranchStable)->setText(tr("&Stable Release Versions"));
    }
    if (m_mapRadioButtons.value(VBoxUpdateData::BranchAllRelease))
    {
        m_mapRadioButtons.value(VBoxUpdateData::BranchAllRelease)->setToolTip(tr("When chosen, you will be notified "
                                                                                 "about all new VirtualBox releases."));
        m_mapRadioButtons.value(VBoxUpdateData::BranchAllRelease)->setText(tr("&All New Releases"));
    }
    if (m_mapRadioButtons.value(VBoxUpdateData::BranchWithBetas))
    {
        m_mapRadioButtons.value(VBoxUpdateData::BranchWithBetas)->setToolTip(tr("When chosen, you will be notified "
                                                                                "about all new VirtualBox releases and "
                                                                                "pre-release versions of VirtualBox."));
        m_mapRadioButtons.value(VBoxUpdateData::BranchWithBetas)->setText(tr("All New Releases and &Pre-Releases"));
    }
}

void UIUpdateSettingsEditor::sltHandleUpdateToggle(bool fEnabled)
{
    /* Update activity status: */
    if (m_pWidgetUpdateSettings)
        m_pWidgetUpdateSettings->setEnabled(fEnabled);

    /* Update time of next check: */
    sltHandleUpdatePeriodChange();

    /* Choose stable branch if update enabled but branch isn't chosen: */
    if (   fEnabled
        && m_pRadioButtonGroup
        && !m_pRadioButtonGroup->checkedButton()
        && m_mapRadioButtons.value(VBoxUpdateData::BranchStable))
        m_mapRadioButtons.value(VBoxUpdateData::BranchStable)->setChecked(true);
}

void UIUpdateSettingsEditor::sltHandleUpdatePeriodChange()
{
    if (m_pFieldUpdateDate)
        m_pFieldUpdateDate->setText(VBoxUpdateData(periodType(), branchType()).date());
}

void UIUpdateSettingsEditor::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIUpdateSettingsEditor::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setContentsMargins(0, 0, 0, 0);
        pLayoutMain->setRowStretch(2, 1);

        /* Prepare update check-box: */
        m_pCheckBox = new QCheckBox(this);
        if (m_pCheckBox)
            pLayoutMain->addWidget(m_pCheckBox, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayoutMain->addItem(pSpacerItem, 1, 0);

        /* Prepare update settings widget: */
        m_pWidgetUpdateSettings = new QWidget(this);
        if (m_pWidgetUpdateSettings)
        {
            /* Prepare update settings widget layout: */
            QGridLayout *pLayoutUpdateSettings = new QGridLayout(m_pWidgetUpdateSettings);
            if (pLayoutUpdateSettings)
            {
                pLayoutUpdateSettings->setContentsMargins(0, 0, 0, 0);
                pLayoutUpdateSettings->setColumnStretch(2, 1);
                pLayoutUpdateSettings->setRowStretch(5, 1);

                /* Prepare update period label: */
                m_pLabelUpdatePeriod = new QLabel(m_pWidgetUpdateSettings);
                if (m_pLabelUpdatePeriod)
                {
                    m_pLabelUpdatePeriod->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutUpdateSettings->addWidget(m_pLabelUpdatePeriod, 0, 0);
                }
                /* Prepare update period combo: */
                m_pComboUpdatePeriod = new QComboBox(m_pWidgetUpdateSettings);
                if (m_pComboUpdatePeriod)
                {
                    if (m_pLabelUpdatePeriod)
                        m_pLabelUpdatePeriod->setBuddy(m_pComboUpdatePeriod);
                    m_pComboUpdatePeriod->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                    m_pComboUpdatePeriod->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                    pLayoutUpdateSettings->addWidget(m_pComboUpdatePeriod, 0, 1);
                }

                /* Prepare update date label: */
                m_pLabelUpdateDate = new QLabel(m_pWidgetUpdateSettings);
                if (m_pLabelUpdateDate)
                {
                    m_pLabelUpdateDate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutUpdateSettings->addWidget(m_pLabelUpdateDate, 1, 0);
                }
                /* Prepare update date field: */
                m_pFieldUpdateDate = new QLabel(m_pWidgetUpdateSettings);
                if (m_pFieldUpdateDate)
                    pLayoutUpdateSettings->addWidget(m_pFieldUpdateDate, 1, 1);

                /* Prepare update date label: */
                m_pLabelUpdateFilter = new QLabel(m_pWidgetUpdateSettings);
                if (m_pLabelUpdateFilter)
                {
                    m_pLabelUpdateFilter->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutUpdateSettings->addWidget(m_pLabelUpdateFilter, 2, 0);
                }
                /* Prepare radio-button group: */
                m_pRadioButtonGroup = new QButtonGroup(m_pWidgetUpdateSettings);
                if (m_pRadioButtonGroup)
                {
                    /* Prepare 'update to "stable"' radio-button: */
                    m_mapRadioButtons[VBoxUpdateData::BranchStable] = new QRadioButton(m_pWidgetUpdateSettings);
                    if (m_mapRadioButtons.value(VBoxUpdateData::BranchStable))
                    {
                        m_pRadioButtonGroup->addButton(m_mapRadioButtons.value(VBoxUpdateData::BranchStable));
                        pLayoutUpdateSettings->addWidget(m_mapRadioButtons.value(VBoxUpdateData::BranchStable), 2, 1);
                    }
                    /* Prepare 'update to "all release"' radio-button: */
                    m_mapRadioButtons[VBoxUpdateData::BranchAllRelease] = new QRadioButton(m_pWidgetUpdateSettings);
                    if (m_mapRadioButtons.value(VBoxUpdateData::BranchAllRelease))
                    {
                        m_pRadioButtonGroup->addButton(m_mapRadioButtons.value(VBoxUpdateData::BranchAllRelease));
                        pLayoutUpdateSettings->addWidget(m_mapRadioButtons.value(VBoxUpdateData::BranchAllRelease), 3, 1);
                    }
                    /* Prepare 'update to "with betas"' radio-button: */
                    m_mapRadioButtons[VBoxUpdateData::BranchWithBetas] = new QRadioButton(m_pWidgetUpdateSettings);
                    if (m_mapRadioButtons.value(VBoxUpdateData::BranchWithBetas))
                    {
                        m_pRadioButtonGroup->addButton(m_mapRadioButtons.value(VBoxUpdateData::BranchWithBetas));
                        pLayoutUpdateSettings->addWidget(m_mapRadioButtons.value(VBoxUpdateData::BranchWithBetas), 4, 1);
                    }
                }
            }

            pLayoutMain->addWidget(m_pWidgetUpdateSettings, 1, 1);
        }
    }
}

void UIUpdateSettingsEditor::prepareConnections()
{
    if (m_pCheckBox)
        connect(m_pCheckBox, &QCheckBox::toggled, this, &UIUpdateSettingsEditor::sltHandleUpdateToggle);
    if (m_pComboUpdatePeriod)
        connect(m_pComboUpdatePeriod, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
                this, &UIUpdateSettingsEditor::sltHandleUpdatePeriodChange);
}

VBoxUpdateData::PeriodType UIUpdateSettingsEditor::periodType() const
{
    const VBoxUpdateData::PeriodType enmResult = m_pCheckBox && m_pCheckBox->isChecked() && m_pComboUpdatePeriod
                                               ? (VBoxUpdateData::PeriodType)m_pComboUpdatePeriod->currentIndex()
                                               : VBoxUpdateData::PeriodNever;
    return enmResult == VBoxUpdateData::PeriodUndefined ? VBoxUpdateData::Period1Day : enmResult;
}

VBoxUpdateData::BranchType UIUpdateSettingsEditor::branchType() const
{
    QAbstractButton *pCheckedButton = m_pRadioButtonGroup ? m_pRadioButtonGroup->checkedButton() : 0;
    return pCheckedButton ? m_mapRadioButtons.key(pCheckedButton, VBoxUpdateData::BranchStable) : VBoxUpdateData::BranchStable;
}
