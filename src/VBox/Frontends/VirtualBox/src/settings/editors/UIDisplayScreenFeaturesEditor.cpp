/* $Id$ */
/** @file
 * VBox Qt GUI - UIDisplayScreenFeaturesEditor class implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>

/* GUI includes: */
#include "UIDisplayScreenFeaturesEditor.h"


UIDisplayScreenFeaturesEditor::UIDisplayScreenFeaturesEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fEnable3DAcceleration(false)
    , m_pLabel(0)
    , m_pCheckBoxEnable3DAcceleration(0)
{
    prepare();
}

void UIDisplayScreenFeaturesEditor::setEnable3DAcceleration(bool fOn)
{
    if (m_pCheckBoxEnable3DAcceleration)
    {
        /* Update cached value and
         * check-box if value has changed: */
        if (m_fEnable3DAcceleration != fOn)
        {
            m_fEnable3DAcceleration = fOn;
            m_pCheckBoxEnable3DAcceleration->setCheckState(m_fEnable3DAcceleration ? Qt::Checked : Qt::Unchecked);
        }
    }
}

bool UIDisplayScreenFeaturesEditor::isEnabled3DAcceleration() const
{
    return   m_pCheckBoxEnable3DAcceleration
           ? m_pCheckBoxEnable3DAcceleration->checkState() == Qt::Checked
           : m_fEnable3DAcceleration;
}

int UIDisplayScreenFeaturesEditor::minimumLabelHorizontalHint() const
{
    return m_pLabel ? m_pLabel->minimumSizeHint().width() : 0;
}

void UIDisplayScreenFeaturesEditor::setMinimumLayoutIndent(int iIndent)
{
    if (m_pLayout)
        m_pLayout->setColumnMinimumWidth(0, iIndent);
}

void UIDisplayScreenFeaturesEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Extended Features:"));
    if (m_pCheckBoxEnable3DAcceleration)
    {
        m_pCheckBoxEnable3DAcceleration->setText(tr("Enable &3D Acceleration"));
        m_pCheckBoxEnable3DAcceleration->setToolTip(tr("When checked, the virtual machine will be given access "
                                                       "to the 3D graphics capabilities available on the host."));
    }
}

void UIDisplayScreenFeaturesEditor::prepare()
{
    /* Prepare main layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        m_pLayout->setContentsMargins(0, 0, 0, 0);
        m_pLayout->setColumnStretch(1, 1);

        /* Prepare label: */
        m_pLabel = new QLabel(this);
        if (m_pLabel)
        {
            m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLayout->addWidget(m_pLabel, 0, 0);
        }
        /* Prepare 'enable 3D acceleration' check-box: */
        m_pCheckBoxEnable3DAcceleration = new QCheckBox(this);
        if (m_pCheckBoxEnable3DAcceleration)
            m_pLayout->addWidget(m_pCheckBoxEnable3DAcceleration, 0, 1);
    }

    /* Prepare connections: */
    connect(m_pCheckBoxEnable3DAcceleration, &QCheckBox::stateChanged,
            this, &UIDisplayScreenFeaturesEditor::sig3DAccelerationFeatureStatusChange);

    /* Apply language settings: */
    retranslateUi();
}
