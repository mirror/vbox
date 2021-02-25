/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic5 class implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QMetaType>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic5.h"

/* COM includes: */
#include "CGuestOSType.h"

UIWizardNewVMPageBasic5::UIWizardNewVMPageBasic5(const QString &strDefaultName,
                                                 const QString &strDefaultPath,
                                                 qulonglong uDefaultSize)
    : UIWizardNewVDPage3(strDefaultName, strDefaultPath)
    , m_pLabel(0)
{
    Q_UNUSED(uDefaultSize);
    prepare();
    qRegisterMetaType<CMedium>();
    // registerField("baseMemory", this, "baseMemory");
    // registerField("VCPUCount", this, "VCPUCount");
    // registerField("EFIEnabled", this, "EFIEnabled");
}

void UIWizardNewVMPageBasic5::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    //pMainLayout->addWidget(createHardwareWidgets());

    pMainLayout->addStretch();
    createConnections();
}

void UIWizardNewVMPageBasic5::createConnections()
{
}

void UIWizardNewVMPageBasic5::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Hardware"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can modify virtual machine's hardware by changing amount of RAM and "
                                            "virtual CPU count.</p>"));

    //retranslateWidgets();
}

void UIWizardNewVMPageBasic5::initializePage()
{
    retranslateUi();

    // if (!field("type").canConvert<CGuestOSType>())
    //     return;

    // CGuestOSType type = field("type").value<CGuestOSType>();
    // ULONG recommendedRam = type.GetRecommendedRAM();
    // if (m_pBaseMemoryEditor)
    //     m_pBaseMemoryEditor->setValue(recommendedRam);

    // KFirmwareType fwType = type.GetRecommendedFirmware();
    // if (m_pEFICheckBox)
    //     m_pEFICheckBox->setChecked(fwType != KFirmwareType_BIOS);
}

void UIWizardNewVMPageBasic5::cleanupPage()
{
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic5::isComplete() const
{
    return true;
}
