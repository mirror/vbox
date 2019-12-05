/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsControllerEditor class implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include "UIConverter.h"
#include "UIGraphicsControllerEditor.h"


UIGraphicsControllerEditor::UIGraphicsControllerEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_enmValue(KGraphicsControllerType_Max)
    , m_pLabel(0)
    , m_pCombo(0)
{
    prepare();
}

void UIGraphicsControllerEditor::setValue(KGraphicsControllerType enmValue)
{
    if (m_pCombo)
    {
        /* Update cached value: */
        m_enmValue = enmValue;

        /* Look for proper index to choose: */
        int iIndex = m_pCombo->findData(QVariant::fromValue(m_enmValue));
        if (iIndex != -1)
            m_pCombo->setCurrentIndex(iIndex);
    }
}

KGraphicsControllerType UIGraphicsControllerEditor::value() const
{
    return m_pCombo ? m_pCombo->currentData().value<KGraphicsControllerType>() : m_enmValue;
}

void UIGraphicsControllerEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("&Graphics Controller:"));
    if (m_pCombo)
    {
        for (int i = 0; i < m_pCombo->count(); ++i)
        {
            const KGraphicsControllerType enmType = m_pCombo->itemData(i).value<KGraphicsControllerType>();
            m_pCombo->setItemText(i, gpConverter->toString(enmType));
        }
    }
}

void UIGraphicsControllerEditor::sltHandleCurrentIndexChanged()
{
    if (m_pCombo)
        emit sigValueChanged(m_pCombo->itemData(m_pCombo->currentIndex()).value<KGraphicsControllerType>());
}

void UIGraphicsControllerEditor::prepare()
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
                if (m_pLabel)
                    m_pLabel->setBuddy(m_pCombo->focusProxy());
                connect(m_pCombo, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
                        this, &UIGraphicsControllerEditor::sltHandleCurrentIndexChanged);
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

void UIGraphicsControllerEditor::populateCombo()
{
    for (int i = 0; i < KGraphicsControllerType_Max; ++i)
        m_pCombo->addItem(QString(), QVariant::fromValue(static_cast<KGraphicsControllerType>(i)));
}
