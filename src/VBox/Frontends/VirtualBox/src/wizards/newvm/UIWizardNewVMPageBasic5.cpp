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

UIWizardNewVMPageBasic5::UIWizardNewVMPageBasic5()
    : m_pLabel(0)
{
    prepare();
    qRegisterMetaType<CMedium>();
    registerField("mediumFormat", this, "mediumFormat");
    registerField("mediumVariant", this, "mediumVariant");
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");

    // fieldImp("machineBaseName").toString(),
    //     fieldImp("machineFolder").toString(),
    //     fieldImp("type").value<CGuestOSType>().GetRecommendedHDD(),
    QString strDefaultName = fieldImp("machineBaseName").toString();
    m_strDefaultName = strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName;
    m_strDefaultPath = fieldImp("machineFolder").toString();

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
