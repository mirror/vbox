/* $Id$ */
/** @file
 * VBox Qt GUI - UIVideoMemoryEditor class implementation.
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
#include "QIAdvancedSlider.h"
#include "UICommon.h"
#include "UIVideoMemoryEditor.h"

/* COM includes: */
#include "CSystemProperties.h"


UIVideoMemoryEditor::UIVideoMemoryEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_comGuestOSType(CGuestOSType())
    , m_cGuestScreenCount(1)
    , m_enmGraphicsControllerType(KGraphicsControllerType_Null)
#ifdef VBOX_WITH_3D_ACCELERATION
    , m_f3DAccelerationSupported(false)
    , m_f3DAccelerationEnabled(false)
#endif
    , m_iMinVRAM(0)
    , m_iMaxVRAM(0)
    , m_iMaxVRAMVisible(0)
    , m_iInitialVRAM(0)
    , m_pLabelMemory(0)
    , m_pSlider(0)
    , m_pLabelMemoryMin(0)
    , m_pLabelMemoryMax(0)
    , m_pSpinBox(0)
{
    prepare();
}

void UIVideoMemoryEditor::setValue(int iValue)
{
    if (m_pSlider)
    {
        m_iInitialVRAM = RT_MIN(iValue, m_iMaxVRAM);
        m_pSlider->setValue(m_iInitialVRAM);
    }
}

int UIVideoMemoryEditor::value() const
{
    return m_pSlider ? m_pSlider->value() : 0;
}

void UIVideoMemoryEditor::setGuestOSType(const CGuestOSType &comGuestOSType)
{
    /* Check if guest OS type really changed: */
    if (m_comGuestOSType == comGuestOSType)
        return;

    /* Remember new guest OS type: */
    m_comGuestOSType = comGuestOSType;

    /* Update requirements: */
    updateRequirements();
}

void UIVideoMemoryEditor::setGuestScreenCount(int cGuestScreenCount)
{
    /* Check if guest screen count really changed: */
    if (m_cGuestScreenCount == cGuestScreenCount)
        return;

    /* Remember new guest screen count: */
    m_cGuestScreenCount = cGuestScreenCount;

    /* Update requirements: */
    updateRequirements();
}

void UIVideoMemoryEditor::setGraphicsControllerType(const KGraphicsControllerType &enmGraphicsControllerType)
{
    /* Check if graphics controller type really changed: */
    if (m_enmGraphicsControllerType == enmGraphicsControllerType)
        return;

    /* Remember new graphics controller type: */
    m_enmGraphicsControllerType = enmGraphicsControllerType;

    /* Update requirements: */
    updateRequirements();
}

#ifdef VBOX_WITH_3D_ACCELERATION
void UIVideoMemoryEditor::set3DAccelerationSupported(bool fSupported)
{
    /* Check if 3D acceleration really changed: */
    if (m_f3DAccelerationSupported == fSupported)
        return;

    /* Remember new 3D acceleration: */
    m_f3DAccelerationSupported = fSupported;

    /* Update requirements: */
    updateRequirements();
}

void UIVideoMemoryEditor::set3DAccelerationEnabled(bool fEnabled)
{
    /* Check if 3D acceleration really changed: */
    if (m_f3DAccelerationEnabled == fEnabled)
        return;

    /* Remember new 3D acceleration: */
    m_f3DAccelerationEnabled = fEnabled;

    /* Update requirements: */
    updateRequirements();
}
#endif /* VBOX_WITH_3D_ACCELERATION */

void UIVideoMemoryEditor::retranslateUi()
{
    if (m_pLabelMemory)
        m_pLabelMemory->setText(tr("Video &Memory:"));
    if (m_pLabelMemoryMin)
        m_pLabelMemoryMin->setText(tr("%1 MB").arg(m_iMinVRAM));
    if (m_pLabelMemoryMax)
        m_pLabelMemoryMax->setText(tr("%1 MB").arg(m_iMaxVRAMVisible));
    if (m_pSpinBox)
        m_pSpinBox->setSuffix(QString(" %1").arg(tr("MB")));
}

void UIVideoMemoryEditor::sltHandleSliderChange()
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

void UIVideoMemoryEditor::sltHandleSpinBoxChange()
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

void UIVideoMemoryEditor::prepare()
{
    /* Prepare common variables: */
    const CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    m_iMinVRAM = comProperties.GetMinGuestVRAM();
    m_iMaxVRAM = comProperties.GetMaxGuestVRAM();
    m_iMaxVRAMVisible = m_iMaxVRAM;

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
            m_pSlider = new QIAdvancedSlider(this);
            if (m_pSlider)
            {
                m_pSlider->setMinimum(m_iMinVRAM);
                m_pSlider->setMaximum(m_iMaxVRAMVisible);
                m_pSlider->setPageStep(calculatePageStep(m_iMaxVRAMVisible));
                m_pSlider->setSingleStep(m_pSlider->pageStep() / 4);
                m_pSlider->setTickInterval(m_pSlider->pageStep());
                m_pSlider->setSnappingEnabled(true);
                m_pSlider->setErrorHint(0, 1);
                m_pSlider->setMinimumWidth(150);
                connect(m_pSlider, &QIAdvancedSlider::valueChanged,
                        this, &UIVideoMemoryEditor::sltHandleSliderChange);
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
            m_pSpinBox->setMinimum(m_iMinVRAM);
            m_pSpinBox->setMaximum(m_iMaxVRAMVisible);
            connect(m_pSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                    this, &UIVideoMemoryEditor::sltHandleSpinBoxChange);
            pMainLayout->addWidget(m_pSpinBox, 0, iRow++, 1, 1);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIVideoMemoryEditor::updateRequirements()
{
    /* Make sure guest OS type is set: */
    if (m_comGuestOSType.isNull())
        return;

    /* Get monitors count and base video memory requirements: */
    quint64 uNeedMBytes = UICommon::requiredVideoMemory(m_comGuestOSType.GetId(), m_cGuestScreenCount) / _1M;

    /* Initial value: */
    m_iMaxVRAMVisible = m_cGuestScreenCount * 32;

    /* No more than m_iMaxVRAM: */
    if (m_iMaxVRAMVisible > m_iMaxVRAM)
        m_iMaxVRAMVisible = m_iMaxVRAM;

    /* No less than 128MB (if possible): */
    if (m_iMaxVRAMVisible < 128 && m_iMaxVRAM >= 128)
        m_iMaxVRAMVisible = 128;

    /* No less than initial VRAM size (wtf?): */
    if (m_iMaxVRAMVisible < m_iInitialVRAM)
        m_iMaxVRAMVisible = m_iInitialVRAM;

#ifdef VBOX_WITH_3D_ACCELERATION
    if (m_f3DAccelerationEnabled && m_f3DAccelerationSupported)
    {
        uNeedMBytes = qMax(uNeedMBytes, (quint64)128);
        /* No less than 256MB (if possible): */
        if (m_iMaxVRAMVisible < 256 && m_iMaxVRAM >= 256)
            m_iMaxVRAMVisible = 256;
    }
#endif

    if (m_pSpinBox)
        m_pSpinBox->setMaximum(m_iMaxVRAMVisible);
    if (m_pSlider)
    {
        m_pSlider->setMaximum(m_iMaxVRAMVisible);
        m_pSlider->setPageStep(calculatePageStep(m_iMaxVRAMVisible));
        m_pSlider->setWarningHint(1, qMin((int)uNeedMBytes, m_iMaxVRAMVisible));
        m_pSlider->setOptimalHint(qMin((int)uNeedMBytes, m_iMaxVRAMVisible), m_iMaxVRAMVisible);
    }
    if (m_pLabelMemoryMax)
        m_pLabelMemoryMax->setText(tr("%1 MB").arg(m_iMaxVRAMVisible));
}

void UIVideoMemoryEditor::revalidate()
{
    if (m_pSlider)
        emit sigValidChanged(   m_enmGraphicsControllerType == KGraphicsControllerType_Null
                             || m_pSlider->value() > 0);
}

/* static */
int UIVideoMemoryEditor::calculatePageStep(int iMax)
{
    /* Reasonable max. number of page steps is 32. */
    const uint uPage = ((uint)iMax + 31) / 32;
    /* Make it a power of 2: */
    uint uP = uPage, p2 = 0x1;
    while ((uP >>= 1))
        p2 <<= 1;
    if (uPage != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int)p2;
}
