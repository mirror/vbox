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

void UIScaleFactorEditor::setScaleFactors(const QList<double> &scaleFactors)
{
    if (m_scaleFactors == scaleFactors)
        return;
    m_scaleFactors = scaleFactors;

    /* Set the spinbox value for the currently selected monitor: */
    if (m_pMonitorComboBox)
    {
        int currentMonitorIndex = m_pMonitorComboBox->currentIndex();
        if (m_scaleFactors.size() > currentMonitorIndex && m_pScaleSpinBox)
            m_pScaleSpinBox->setValue(100 * m_scaleFactors.at(currentMonitorIndex));
    }
}

const QList<double>& UIScaleFactorEditor::scaleFactors() const
{
    return m_scaleFactors;
}

void UIScaleFactorEditor::retranslateUi()
{
}

void UIScaleFactorEditor::sltScaleSpinBoxValueChanged(int value)
{
    setSliderValue(value);
    if (m_pMonitorComboBox)
        setScaleFactor(m_pMonitorComboBox->currentIndex(), value);
}

void UIScaleFactorEditor::sltScaleSliderValueChanged(int value)
{
    setSpinBoxValue(value);
    if (m_pMonitorComboBox)
        setScaleFactor(m_pMonitorComboBox->currentIndex(), value);
}

void UIScaleFactorEditor::sltMonitorComboIndexChanged(int index)
{
    if (index >= m_scaleFactors.size())
        return;

    /* Update the slider and spinbox values without emitting signals: */
    int scaleFactor = 100 *m_scaleFactors[index];
    setSliderValue(scaleFactor);
    setSpinBoxValue(scaleFactor);
}

void UIScaleFactorEditor::prepare()
{
    m_pMainLayout = new QGridLayout;
    if (!m_pMainLayout)
        return;
    m_pMonitorComboBox = new QComboBox;
    if (m_pMonitorComboBox)
    {
        m_pMainLayout->addWidget(m_pMonitorComboBox, 0, 0);
        connect(m_pMonitorComboBox ,static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &UIScaleFactorEditor::sltMonitorComboIndexChanged);
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

void UIScaleFactorEditor::setScaleFactor(int iMonitorIndex, int iScaleFactor)
{
    /* Make sure we have the corresponding scale value for the @p iMonitorIndex: */
    if (iMonitorIndex >= m_scaleFactors.size())
    {
        for (int i = m_scaleFactors.size(); i <= iMonitorIndex; ++i)
            m_scaleFactors.append(1.0);
    }
    m_scaleFactors[iMonitorIndex] = iScaleFactor / 100.0;
}

void UIScaleFactorEditor::setSliderValue(int iValue)
{
    if (m_pScaleSlider && iValue != m_pScaleSlider->value())
    {
        m_pScaleSlider->blockSignals(true);
        m_pScaleSlider->setValue(iValue);
        m_pScaleSlider->blockSignals(false);
    }
}

void UIScaleFactorEditor::setSpinBoxValue(int iValue)
{
    if (m_pScaleSpinBox && iValue != m_pScaleSpinBox->value())
    {
        m_pScaleSpinBox->blockSignals(true);
        m_pScaleSpinBox->setValue(iValue);
        m_pScaleSpinBox->blockSignals(false);
    }
}
