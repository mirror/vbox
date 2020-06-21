/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicHardware class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QIntValidator>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QLabel>
#include <QSpinBox>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UIBaseMemorySlider.h"
#include "UICommon.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVMPageBasicHardware.h"
#include "UIWizardNewVM.h"


UIWizardNewVMPageHardware::UIWizardNewVMPageHardware()
    : m_pBaseMemoryEditor(0)
    , m_pVirtualCPUEditor(0)
{
}

int UIWizardNewVMPageHardware::baseMemory() const
{
    if (!m_pBaseMemoryEditor)
        return 0;
    return m_pBaseMemoryEditor->value();
}

int UIWizardNewVMPageHardware::VCPUCount() const
{
    if (!m_pVirtualCPUEditor)
        return 1;
    return m_pVirtualCPUEditor->value();
}

UIWizardNewVMPageBasicHardware::UIWizardNewVMPageBasicHardware()
    : m_pLabel(0)
{
    /* Create widget: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        QGridLayout *pMemoryLayout = new QGridLayout;
        {
            m_pBaseMemoryEditor = new UIBaseMemoryEditor(this, true);
            m_pVirtualCPUEditor = new UIVirtualCPUEditor(this, true);
            pMemoryLayout->addWidget(m_pBaseMemoryEditor, 0, 0, 1, 4);
            pMemoryLayout->addWidget(m_pVirtualCPUEditor, 1, 0, 1, 4);
        }
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);
        pMainLayout->addLayout(pMemoryLayout);
        pMainLayout->addStretch();
    }


    /* Register fields: */
    registerField("baseMemory", this, "baseMemory");
    registerField("VCPUCount", this, "VCPUCount");
}

void UIWizardNewVMPageBasicHardware::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Virtual Machine Settings"));

    /* Translate widgets: */
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can modify the virtual machine's hardware.</p>"));
}

void UIWizardNewVMPageBasicHardware::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Get recommended 'ram' field value: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    m_pBaseMemoryEditor->setValue(type.GetRecommendedRAM());
    m_pVirtualCPUEditor->setValue(1);

    /* 'Ram' field should have focus initially: */
    m_pBaseMemoryEditor->setFocus();
}

bool UIWizardNewVMPageBasicHardware::isComplete() const
{
    return UIWizardPage::isComplete();
}
