/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisualStateEditor class implementation.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>

/* GUI includes: */
#include "QIComboBox.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataManager.h"
#include "UIVisualStateEditor.h"


UIVisualStateEditor::UIVisualStateEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_enmValue(UIVisualStateType_Invalid)
    , m_pLabel(0)
    , m_pCombo(0)
{
    prepare();
}

void UIVisualStateEditor::setMachineId(const QUuid &uMachineId)
{
    m_uMachineId = uMachineId;
}

void UIVisualStateEditor::setValue(UIVisualStateType enmValue)
{
    if (m_pCombo)
    {
        /* Update cached value and
         * combo if value has changed: */
        if (m_enmValue != enmValue)
        {
            m_enmValue = enmValue;
            populateCombo();
        }

        /* Look for proper index to choose: */
        int iIndex = m_pCombo->findData(QVariant::fromValue(m_enmValue));
        if (iIndex != -1)
            m_pCombo->setCurrentIndex(iIndex);
    }
}

UIVisualStateType UIVisualStateEditor::value() const
{
    return m_pCombo ? m_pCombo->currentData().value<UIVisualStateType>() : m_enmValue;
}

void UIVisualStateEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Visual &State:"));
    if (m_pCombo)
    {
        for (int i = 0; i < m_pCombo->count(); ++i)
        {
            const UIVisualStateType enmType = m_pCombo->itemData(i).value<UIVisualStateType>();
            m_pCombo->setItemText(i, gpConverter->toString(enmType));
        }
    }
}

void UIVisualStateEditor::sltHandleCurrentIndexChanged()
{
    if (m_pCombo)
        emit sigValueChanged(m_pCombo->itemData(m_pCombo->currentIndex()).value<UIVisualStateType>());
}

void UIVisualStateEditor::prepare()
{
    /* Create main layout: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        int iRow = 0;

        /* Create label: */
        if (m_fWithLabel)
            m_pLabel = new QLabel(this);
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel, 0, iRow++, 1, 1);

        /* Create combo layout: */
        QHBoxLayout *pComboLayout = new QHBoxLayout;
        if (pComboLayout)
        {
            /* Create combo: */
            m_pCombo = new QIComboBox(this);
            if (m_pCombo)
            {
                setFocusProxy(m_pCombo->focusProxy());
                /* This is necessary since contents is dynamical now: */
                m_pCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                if (m_pLabel)
                    m_pLabel->setBuddy(m_pCombo->focusProxy());
                connect(m_pCombo, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
                        this, &UIVisualStateEditor::sltHandleCurrentIndexChanged);
                pComboLayout->addWidget(m_pCombo);
            }

            /* Add stretch: */
            pComboLayout->addStretch();

            /* Add combo-layout into main-layout: */
            pMainLayout->addLayout(pComboLayout, 0, iRow++, 1, 1);
        }
    }

    /* Populate combo: */
    populateCombo();

    /* Apply language settings: */
    retranslateUi();
}

void UIVisualStateEditor::populateCombo()
{
    if (m_pCombo)
    {
        /* Clear combo first of all: */
        m_supportedValues.clear();
        m_pCombo->clear();

        /* Get possible values: */
        QList<UIVisualStateType> possibleValues;
        possibleValues << UIVisualStateType_Normal
                       << UIVisualStateType_Fullscreen
                       << UIVisualStateType_Seamless
                       << UIVisualStateType_Scale;

        /* Load currently supported visual state types: */
        const UIVisualStateType enmRestrictedTypes = m_uMachineId.isNull()
                                                   ? UIVisualStateType_Invalid
                                                   : gEDataManager->restrictedVisualStates(m_uMachineId);
        foreach (const UIVisualStateType &enmPossibleValue, possibleValues)
            if (!(enmPossibleValue & enmRestrictedTypes))
                m_supportedValues << enmPossibleValue;

        /* Make sure requested value if sane is present as well: */
        if (   possibleValues.contains(m_enmValue)
            && !m_supportedValues.contains(m_enmValue))
            m_supportedValues.prepend(m_enmValue);

        /* Update combo with all the supported values: */
        foreach (const UIVisualStateType &enmType, m_supportedValues)
            m_pCombo->addItem(QString(), QVariant::fromValue(enmType));

        /* Retranslate finally: */
        retranslateUi();
    }
}
