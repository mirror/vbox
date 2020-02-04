/* $Id$ */
/** @file
 * VBox Qt GUI - UIBaseMemoryEditor class implementation.
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
#include "UIBaseMemoryEditor.h"
#include "UIBaseMemorySlider.h"


UIBaseMemoryEditor::UIBaseMemoryEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_pLabelMemory(0)
    , m_pSlider(0)
    , m_pLabelMemoryMin(0)
    , m_pLabelMemoryMax(0)
    , m_pSpinBox(0)
{
    prepare();
}

void UIBaseMemoryEditor::setValue(int iValue)
{
    if (m_pSlider)
        m_pSlider->setValue(iValue);
}

int UIBaseMemoryEditor::value() const
{
    return m_pSlider ? m_pSlider->value() : 0;
}

uint UIBaseMemoryEditor::maxRAMOpt() const
{
    return m_pSlider ? m_pSlider->maxRAMOpt() : 0;
}

uint UIBaseMemoryEditor::maxRAMAlw() const
{
    return m_pSlider ? m_pSlider->maxRAMAlw() : 0;
}

void UIBaseMemoryEditor::retranslateUi()
{
    if (m_pLabelMemory)
        m_pLabelMemory->setText(tr("Base &Memory:"));
    if (m_pLabelMemoryMin)
        m_pLabelMemoryMin->setText(tr("%1 MB").arg(m_pSlider->minRAM()));
    if (m_pLabelMemoryMax)
        m_pLabelMemoryMax->setText(tr("%1 MB").arg(m_pSlider->maxRAM()));
    if (m_pSpinBox)
        m_pSpinBox->setSuffix(QString(" %1").arg(tr("MB")));
}

void UIBaseMemoryEditor::sltHandleSliderChange()
{
    /* Apply spin-box value keeping it's signals disabled: */
    if (m_pSpinBox && m_pSlider)
    {
        m_pSpinBox->blockSignals(true);
        m_pSpinBox->setValue(m_pSlider->value());
        m_pSpinBox->blockSignals(false);
    }

    /* Revalidate to send signal to listener: */
    revalidate();
}

void UIBaseMemoryEditor::sltHandleSpinBoxChange()
{
    /* Apply slider value keeping it's signals disabled: */
    if (m_pSpinBox && m_pSlider)
    {
        m_pSlider->blockSignals(true);
        m_pSlider->setValue(m_pSpinBox->value());
        m_pSlider->blockSignals(false);
    }

    /* Revalidate to send signal to listener: */
    revalidate();
}

void UIBaseMemoryEditor::prepare()
{
    /* Create main layout: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        int iRow = 0;

        /* Create memory label: */
        if (m_fWithLabel)
            m_pLabelMemory = new QLabel(this);
        if (m_pLabelMemory)
            pMainLayout->addWidget(m_pLabelMemory, 0, iRow++, 1, 1);

        /* Create slider layout: */
        QVBoxLayout *pSliderLayout = new QVBoxLayout;
        if (pSliderLayout)
        {
            pSliderLayout->setContentsMargins(0, 0, 0, 0);

            /* Create memory slider: */
            m_pSlider = new UIBaseMemorySlider(this);
            if (m_pSlider)
            {
                m_pSlider->setMinimumWidth(150);
                connect(m_pSlider, &UIBaseMemorySlider::valueChanged,
                        this, &UIBaseMemoryEditor::sltHandleSliderChange);
                pSliderLayout->addWidget(m_pSlider);
            }

            /* Create legend layout: */
            QHBoxLayout *pLegendLayout = new QHBoxLayout;
            if (pLegendLayout)
            {
                pLegendLayout->setContentsMargins(0, 0, 0, 0);

                /* Create min label: */
                m_pLabelMemoryMin = new QLabel(this);
                if (m_pLabelMemoryMin)
                    pLegendLayout->addWidget(m_pLabelMemoryMin);

                /* Push labels from each other: */
                pLegendLayout->addStretch();

                /* Create max label: */
                m_pLabelMemoryMax = new QLabel(this);
                if (m_pLabelMemoryMax)
                    pLegendLayout->addWidget(m_pLabelMemoryMax);

                /* Add legend layout to slider layout: */
                pSliderLayout->addLayout(pLegendLayout);
            }

            /* Add slider layout to main layout: */
            pMainLayout->addLayout(pSliderLayout, 0, iRow++, 2, 1);
        }

        /* Create memory spin-box: */
        m_pSpinBox = new QSpinBox(this);
        if (m_pSpinBox)
        {
            setFocusProxy(m_pSpinBox);
            if (m_pLabelMemory)
                m_pLabelMemory->setBuddy(m_pSpinBox);
            m_pSpinBox->setMinimum(m_pSlider->minRAM());
            m_pSpinBox->setMaximum(m_pSlider->maxRAM());
            connect(m_pSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                    this, &UIBaseMemoryEditor::sltHandleSpinBoxChange);
            pMainLayout->addWidget(m_pSpinBox, 0, iRow++, 1, 1);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIBaseMemoryEditor::revalidate()
{
    if (m_pSlider)
        emit sigValidChanged(m_pSlider->value() < (int)m_pSlider->maxRAMAlw());
}
