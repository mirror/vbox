/* $Id$ */
/** @file
 * VBox Qt GUI - UIAccelerationFeaturesEditor class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIAccelerationFeaturesEditor.h"


UIAccelerationFeaturesEditor::UIAccelerationFeaturesEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fEnableVirtualization(false)
    , m_fEnableNestedPaging(false)
    , m_pLabel(0)
    , m_pCheckBoxEnableVirtualization(0)
    , m_pCheckBoxEnableNestedPaging(0)
{
    prepare();
}

void UIAccelerationFeaturesEditor::setEnableVirtualization(bool fOn)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fEnableVirtualization != fOn)
    {
        m_fEnableVirtualization = fOn;
        if (m_pCheckBoxEnableVirtualization)
            m_pCheckBoxEnableVirtualization->setCheckState(m_fEnableVirtualization ? Qt::Checked : Qt::Unchecked);
    }
}

bool UIAccelerationFeaturesEditor::isEnabledVirtualization() const
{
    return   m_pCheckBoxEnableVirtualization
           ? m_pCheckBoxEnableVirtualization->checkState() == Qt::Checked
           : m_fEnableVirtualization;
}

void UIAccelerationFeaturesEditor::setEnableVirtualizationAvailable(bool fAvailable)
{
    m_pCheckBoxEnableVirtualization->setEnabled(fAvailable);
}

void UIAccelerationFeaturesEditor::setEnableNestedPaging(bool fOn)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fEnableNestedPaging != fOn)
    {
        m_fEnableNestedPaging = fOn;
        if (m_pCheckBoxEnableNestedPaging)
            m_pCheckBoxEnableNestedPaging->setCheckState(m_fEnableNestedPaging ? Qt::Checked : Qt::Unchecked);
    }
}

bool UIAccelerationFeaturesEditor::isEnabledNestedPaging() const
{
    return   m_pCheckBoxEnableNestedPaging
           ? m_pCheckBoxEnableNestedPaging->checkState() == Qt::Checked
           : m_fEnableNestedPaging;
}

void UIAccelerationFeaturesEditor::setEnableNestedPagingAvailable(bool fAvailable)
{
    m_pCheckBoxEnableNestedPaging->setEnabled(fAvailable);
}

int UIAccelerationFeaturesEditor::minimumLabelHorizontalHint() const
{
    return m_pLabel ? m_pLabel->minimumSizeHint().width() : 0;
}

void UIAccelerationFeaturesEditor::setMinimumLayoutIndent(int iIndent)
{
    if (m_pLayout)
        m_pLayout->setColumnMinimumWidth(0, iIndent);
}

void UIAccelerationFeaturesEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Hardware Virtualization:"));
    if (m_pCheckBoxEnableVirtualization)
    {
        m_pCheckBoxEnableVirtualization->setText(tr("Enable &VT-x/AMD-V"));
        m_pCheckBoxEnableVirtualization->setToolTip(tr("When checked, the virtual machine will try to make use of the host "
                                                       "CPU's hardware virtualization extensions such as Intel VT-x and AMD-V."));
    }
    if (m_pCheckBoxEnableNestedPaging)
    {
        m_pCheckBoxEnableNestedPaging->setText(tr("Enable Nested Pa&ging"));
        m_pCheckBoxEnableNestedPaging->setToolTip(tr("When checked, the virtual machine will try to make use of the nested "
                                                     "paging extension of Intel VT-x and AMD-V."));
    }
}

void UIAccelerationFeaturesEditor::prepare()
{
    /* Prepare main layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        m_pLayout->setContentsMargins(0, 0, 0, 0);
        m_pLayout->setColumnStretch(1, 1);

        /* Prepare label layout: */
        QVBoxLayout *pLayoutLabel = new QVBoxLayout;
        if (pLayoutLabel)
        {
            /* Prepare virtualization label: */
            m_pLabel = new QLabel(this);
            if (m_pLabel)
            {
                m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutLabel->addWidget(m_pLabel);
            }
            /* Prepare placeholder: */
            QWidget *pWidgetPlaceholder = new QWidget(this);
            if (pWidgetPlaceholder)
            {
#ifndef VBOX_WITH_RAW_MODE
                /* Hide placeholder when raw-mode is not supported: */
                pWidgetPlaceholder->setVisible(false);
#endif
                pLayoutLabel->addWidget(pWidgetPlaceholder);
            }

            m_pLayout->addLayout(pLayoutLabel, 0, 0);
        }

        /* Prepare widget layout: */
        QVBoxLayout *pLayoutWidget = new QVBoxLayout;
        if (pLayoutWidget)
        {
            /* Prepare 'enable PAE' check-box: */
            m_pCheckBoxEnableVirtualization = new QCheckBox(this);
            if (m_pCheckBoxEnableVirtualization)
            {
#ifndef VBOX_WITH_RAW_MODE
                /* Hide check-box when raw-mode is not supported: */
                m_pCheckBoxEnableVirtualization->setVisible(false);
#endif
                connect(m_pCheckBoxEnableVirtualization, &QCheckBox::stateChanged,
                        this, &UIAccelerationFeaturesEditor::sigChangedVirtualization);
                pLayoutWidget->addWidget(m_pCheckBoxEnableVirtualization);
            }
            /* Prepare 'enable nested virtualization' check-box: */
            m_pCheckBoxEnableNestedPaging = new QCheckBox(this);
            if (m_pCheckBoxEnableNestedPaging)
            {
                connect(m_pCheckBoxEnableNestedPaging, &QCheckBox::stateChanged,
                        this, &UIAccelerationFeaturesEditor::sigChangedNestedPaging);
                pLayoutWidget->addWidget(m_pCheckBoxEnableNestedPaging);
            }

            m_pLayout->addLayout(pLayoutWidget, 0, 1);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}
