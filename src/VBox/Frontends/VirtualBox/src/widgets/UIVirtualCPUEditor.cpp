/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualCPUEditor class implementation.
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
#include <QSpinBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "UICommon.h"
#include "UIVirtualCPUEditor.h"
#include "QIAdvancedSlider.h"

/* COM includes */
#include "COMEnums.h"
#include "CSystemProperties.h"

UIVirtualCPUEditor::UIVirtualCPUEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pLabelVCPU(0)
    , m_pSlider(0)
    , m_pLabelVCPUMin(0)
    , m_pLabelVCPUMax(0)
    , m_pSpinBox(0)
    , m_uMaxVCPUCount(1)
    , m_uMinVCPUCount(1)
    , m_fWithLabel(fWithLabel)
{
    prepare();
}

void UIVirtualCPUEditor::setValue(int iValue)
{
    if (m_pSlider)
        m_pSlider->setValue(iValue);
}

int UIVirtualCPUEditor::value() const
{
    return m_pSlider ? m_pSlider->value() : 0;
}

void UIVirtualCPUEditor::retranslateUi()
{
    if (m_pLabelVCPU)
        m_pLabelVCPU->setText(tr("&Processor(s):"));
    if (m_pLabelVCPUMin)
        m_pLabelVCPUMin->setText(tr("%1 CPU", "%1 is 1 for now").arg(m_uMinVCPUCount));
    if (m_pLabelVCPUMax)
        m_pLabelVCPUMax->setText(tr("%1 CPUs", "%1 is host cpu count * 2 for now").arg(m_uMaxVCPUCount));
}

void UIVirtualCPUEditor::sltHandleSliderChange()
{
    if (m_pSpinBox && m_pSlider)
    {
        m_pSpinBox->blockSignals(true);
        m_pSpinBox->setValue(m_pSlider->value());
        m_pSpinBox->blockSignals(false);
    }
}

void UIVirtualCPUEditor::sltHandleSpinBoxChange()
{
    if (m_pSpinBox && m_pSlider)
    {
        m_pSlider->blockSignals(true);
        m_pSlider->setValue(m_pSpinBox->value());
        m_pSlider->blockSignals(false);
    }
}

void UIVirtualCPUEditor::prepare()
{
    const CSystemProperties properties = uiCommon().virtualBox().GetSystemProperties();
    const uint uHostCPUs = uiCommon().host().GetProcessorOnlineCoreCount();
    m_uMinVCPUCount = properties.GetMinGuestCPUCount();
    m_uMaxVCPUCount = qMin(2 * uHostCPUs, (uint)properties.GetMaxGuestCPUCount());

    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        int iRow = 0;

        if (m_fWithLabel)
            m_pLabelVCPU = new QLabel(this);
        if (m_pLabelVCPU)
            pMainLayout->addWidget(m_pLabelVCPU, 0, iRow++, 1, 1);

        QVBoxLayout *pSliderLayout = new QVBoxLayout;
        if (pSliderLayout)
        {
            pSliderLayout->setContentsMargins(0, 0, 0, 0);
            m_pSlider = new QIAdvancedSlider(this);
            if (m_pSlider)
            {
                m_pSlider->setMinimumWidth(150);
                m_pSlider->setMinimum(m_uMinVCPUCount);
                m_pSlider->setMaximum(m_uMaxVCPUCount);
                m_pSlider->setPageStep(1);
                m_pSlider->setSingleStep(1);
                m_pSlider->setTickInterval(1);
                m_pSlider->setOptimalHint(1, uHostCPUs);
                m_pSlider->setWarningHint(uHostCPUs, m_uMaxVCPUCount);

                connect(m_pSlider, &QIAdvancedSlider::valueChanged,
                        this, &UIVirtualCPUEditor::sltHandleSliderChange);
                pSliderLayout->addWidget(m_pSlider);
            }
            QHBoxLayout *pLegendLayout = new QHBoxLayout;
            if (pLegendLayout)
            {
                pLegendLayout->setContentsMargins(0, 0, 0, 0);
                m_pLabelVCPUMin = new QLabel(this);
                if (m_pLabelVCPUMin)
                    pLegendLayout->addWidget(m_pLabelVCPUMin);
                pLegendLayout->addStretch();
                m_pLabelVCPUMax = new QLabel(this);
                if (m_pLabelVCPUMax)
                    pLegendLayout->addWidget(m_pLabelVCPUMax);
                pSliderLayout->addLayout(pLegendLayout);
            }
            pMainLayout->addLayout(pSliderLayout, 0, iRow++, 2, 1);
        }
        m_pSpinBox = new QSpinBox(this);
        if (m_pSpinBox)
        {
            setFocusProxy(m_pSpinBox);
            if (m_pLabelVCPU)
                m_pLabelVCPU->setBuddy(m_pSpinBox);
            m_pSpinBox->setMinimum(m_uMinVCPUCount);
            m_pSpinBox->setMaximum(m_uMaxVCPUCount);
            connect(m_pSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                    this, &UIVirtualCPUEditor::sltHandleSpinBoxChange);
            pMainLayout->addWidget(m_pSpinBox, 0, iRow++, 1, 1);
        }
    }

    retranslateUi();
}
