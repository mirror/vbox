/* $Id$ */
/** @file
 * VBox Qt GUI - UIAudioHostDriverEditor class implementation.
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
#include "UIAudioHostDriverEditor.h"


UIAudioHostDriverEditor::UIAudioHostDriverEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_pLabel(0)
    , m_pCombo(0)
{
    prepare();
}

void UIAudioHostDriverEditor::setValue(KAudioDriverType enmValue)
{
    if (m_pCombo)
    {
        int iIndex = m_pCombo->findData(QVariant::fromValue(enmValue));
        if (iIndex != -1)
            m_pCombo->setCurrentIndex(iIndex);
    }
}

KAudioDriverType UIAudioHostDriverEditor::value() const
{
    return m_pCombo ? m_pCombo->itemData(m_pCombo->currentIndex()).value<KAudioDriverType>() : KAudioDriverType_Null;
}

void UIAudioHostDriverEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Host Audio &Driver:"));
    if (m_pCombo)
    {
        for (int i = 0; i < m_pCombo->count(); ++i)
        {
            const KAudioDriverType enmType = m_pCombo->itemData(i).value<KAudioDriverType>();
            m_pCombo->setItemText(i, gpConverter->toString(enmType));
        }
    }
}

void UIAudioHostDriverEditor::sltHandleCurrentIndexChanged()
{
    if (m_pCombo)
        emit sigValueChanged(m_pCombo->itemData(m_pCombo->currentIndex()).value<KAudioDriverType>());
}

void UIAudioHostDriverEditor::prepare()
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
                        this, &UIAudioHostDriverEditor::sltHandleCurrentIndexChanged);
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

void UIAudioHostDriverEditor::populateCombo()
{
    /* Fill combo manually: */
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_Null));
#ifdef Q_OS_WIN
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_WinMM));
# endif
#endif
#ifdef VBOX_WITH_AUDIO_OSS
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_OSS));
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_ALSA));
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_Pulse));
#endif
#ifdef Q_OS_MACX
    m_pCombo->addItem(QString(), QVariant::fromValue(KAudioDriverType_CoreAudio));
#endif
}
