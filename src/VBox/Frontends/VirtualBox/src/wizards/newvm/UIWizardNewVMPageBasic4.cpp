/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic4 class implementation.
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
#include <QGridLayout>
#include <QMetaType>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIBaseMemoryEditor.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UIMediumSelector.h"
#include "UIMessageCenter.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVD.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic4.h"

UIWizardNewVMPage4::UIWizardNewVMPage4()
    : m_pBaseMemoryEditor(0)
    , m_pVirtualCPUEditor(0)
{
}

int UIWizardNewVMPage4::baseMemory() const
{
    if (!m_pBaseMemoryEditor)
        return 0;
    return m_pBaseMemoryEditor->value();
}

int UIWizardNewVMPage4::VCPUCount() const
{
    if (!m_pVirtualCPUEditor)
        return 1;
    return m_pVirtualCPUEditor->value();
}

void UIWizardNewVMPage4::retranslateWidgets()
{
}

QWidget *UIWizardNewVMPage4::createHardwareWidgets()
{
    QWidget *pHardwareContainer = new QWidget;
    QGridLayout *pHardwareLayout = new QGridLayout(pHardwareContainer);

    m_pBaseMemoryEditor = new UIBaseMemoryEditor(0, true);
    m_pVirtualCPUEditor = new UIVirtualCPUEditor(0, true);
    pHardwareLayout->addWidget(m_pBaseMemoryEditor, 0, 0, 1, 4);
    pHardwareLayout->addWidget(m_pVirtualCPUEditor, 1, 0, 1, 4);
    return pHardwareContainer;
}

UIWizardNewVMPageBasic4::UIWizardNewVMPageBasic4()
    : m_pLabel(0)
{
    prepare();
    qRegisterMetaType<CMedium>();
    registerField("baseMemory", this, "baseMemory");
    registerField("VCPUCount", this, "VCPUCount");
}

void UIWizardNewVMPageBasic4::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(createHardwareWidgets());

    pMainLayout->addStretch();
    createConnections();
}

void UIWizardNewVMPageBasic4::createConnections()
{

}


void UIWizardNewVMPageBasic4::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Hardware"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can modify virtual machine's hardware by changing amount of RAM and "
                                            "virtual CPU count.</p>"));

    retranslateWidgets();
}

void UIWizardNewVMPageBasic4::initializePage()
{
    retranslateUi();

    if (!field("type").canConvert<CGuestOSType>())
        return;

    CGuestOSType type = field("type").value<CGuestOSType>();
    ULONG recommendedRam = type.GetRecommendedRAM();
    if (m_pBaseMemoryEditor)
        m_pBaseMemoryEditor->setValue(recommendedRam);

}

void UIWizardNewVMPageBasic4::cleanupPage()
{
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic4::isComplete() const
{
    return true;
}
