/* $Id$ */
/** @file
 * VBox Qt GUI - UIScaleFactorEditor class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QComboBox>
#include <QGridLayout>
#include <QSpinBox>
# include <QWidget>

/* GUI includes: */
#include "QIAdvancedSlider.h"
#include "UIScaleFactorEditor.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIScaleFactorEditor::UIScaleFactorEditor(QWidget *pParent)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pScaleSpinBox(0)
    , m_pMainLayout(0)
    , m_pMonitorComboBox(0)
{
    /* Prepare: */
    prepare();
}

void UIScaleFactorEditor::prepare()
{
    setStyleSheet("background-color:yellow;");

    m_pMainLayout = new QGridLayout;
    if (!m_pMainLayout)
        return;
    m_pMonitorComboBox = new QComboBox;
    if (m_pMonitorComboBox)
    {
        m_pMainLayout->addWidget(m_pMonitorComboBox, 0, 0);
    }
    m_pScaleSpinBox = new QSpinBox;
    if (m_pScaleSpinBox)
    {
        m_pMainLayout->addWidget(m_pScaleSpinBox, 0, 2);
        m_pScaleSpinBox->setMinimum(100);
        m_pScaleSpinBox->setMaximum(200);
        connect(m_pScaleSpinBox ,static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, &UIScaleFactorEditor::sltScaleSpinBoxValueChanged);
    }

    m_pScaleSlider = new QIAdvancedSlider;
    {
        m_pMainLayout->addWidget(m_pScaleSlider, 0, 1);
        m_pScaleSlider->setMinimum(100);
        m_pScaleSlider->setMaximum(200);
        m_pScaleSlider->setPageStep(10);
        m_pScaleSlider->setSingleStep(1);
        m_pScaleSlider->setTickInterval(10);
        m_pScaleSlider->setSnappingEnabled(true);
        connect(m_pScaleSlider, static_cast<void(QIAdvancedSlider::*)(int)>(&QIAdvancedSlider::valueChanged),
                this, &UIScaleFactorEditor::sltScaleSliderValueChanged);
    }
    setLayout(m_pMainLayout);
}

void UIScaleFactorEditor::setMonitorCount(int iMonitorCount)
{
    if (!m_pMonitorComboBox)
        return;
    if (iMonitorCount == m_pMonitorComboBox->count())
        return;

    m_pMonitorComboBox->blockSignals(true);
    m_pMonitorComboBox->clear();
    for (int i = 0; i < iMonitorCount; ++i)
    {
        m_pMonitorComboBox->addItem(QString("Monitor %1").arg(i));

    }
    m_pMonitorComboBox->blockSignals(false);
}

void UIScaleFactorEditor::sltScaleSpinBoxValueChanged(int value)
{
    if (m_pScaleSlider && value != m_pScaleSlider->value())
    {
        m_pScaleSlider->blockSignals(true);
        m_pScaleSlider->setValue(value);
        m_pScaleSlider->blockSignals(false);
    }
}

void UIScaleFactorEditor::sltScaleSliderValueChanged(int value)
{
    if (m_pScaleSpinBox && value != m_pScaleSpinBox->value())
    {
        m_pScaleSpinBox->blockSignals(true);
        m_pScaleSpinBox->setValue(value);
        m_pScaleSpinBox->blockSignals(false);
    }
}

void UIScaleFactorEditor::retranslateUi()
{
}
