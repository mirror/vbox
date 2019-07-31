/* $Id$ */
/** @file
 * VBox Qt GUI - UIAudioControllerEditor class implementation.
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
#include "UIAudioControllerEditor.h"


UIAudioControllerEditor::UIAudioControllerEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_pLabel(0)
    , m_pCombo(0)
{
    prepare();
}

void UIAudioControllerEditor::setValue(KAudioControllerType enmValue)
{
    if (m_pCombo)
    {
        int iIndex = m_pCombo->findData(QVariant::fromValue(enmValue));
        if (iIndex != -1)
            m_pCombo->setCurrentIndex(iIndex);
    }
}

KAudioControllerType UIAudioControllerEditor::value() const
{
    return m_pCombo ? m_pCombo->itemData(m_pCombo->currentIndex()).value<KAudioControllerType>() : KAudioControllerType_AC97;
}

void UIAudioControllerEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Audio &Controller:"));
    if (m_pCombo)
    {
        for (int i = 0; i < m_pCombo->count(); ++i)
        {
            const KAudioControllerType enmType = m_pCombo->itemData(i).value<KAudioControllerType>();
            m_pCombo->setItemText(i, gpConverter->toString(enmType));
        }
    }
}

void UIAudioControllerEditor::sltHandleCurrentIndexChanged()
{
    if (m_pCombo)
        emit sigValueChanged(m_pCombo->itemData(m_pCombo->currentIndex()).value<KAudioControllerType>());
}

void UIAudioControllerEditor::prepare()
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
                        this, &UIAudioControllerEditor::sltHandleCurrentIndexChanged);
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

void UIAudioControllerEditor::populateCombo()
{
    /* Fill combo manually: */
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioControllerType_HDA));
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioControllerType_AC97));
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioControllerType_SB16));
}
