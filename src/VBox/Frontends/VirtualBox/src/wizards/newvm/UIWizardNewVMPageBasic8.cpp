/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic8 class implementation.
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
#include <QFileInfo>
#include <QGridLayout>
#include <QMetaType>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UIMessageCenter.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic8.h"
#include "UIWizardNewVDPageBasic3.h"

/* COM includes: */
#include "CGuestOSType.h"

UIWizardNewVMPage8::UIWizardNewVMPage8()
    : m_pBaseMemoryEditor(0)
    , m_pVirtualCPUEditor(0)
    , m_pEFICheckBox(0)
{
}

int UIWizardNewVMPage8::baseMemory() const
{
    if (!m_pBaseMemoryEditor)
        return 0;
    return m_pBaseMemoryEditor->value();
}

int UIWizardNewVMPage8::VCPUCount() const
{
    if (!m_pVirtualCPUEditor)
        return 1;
    return m_pVirtualCPUEditor->value();
}

bool UIWizardNewVMPage8::EFIEnabled() const
{
    if (!m_pEFICheckBox)
        return false;
    return m_pEFICheckBox->isChecked();
}

void UIWizardNewVMPage8::retranslateWidgets()
{
    if (m_pEFICheckBox)
    {
        m_pEFICheckBox->setText(UIWizardNewVM::tr("Enable EFI (special OSes only)"));
        m_pEFICheckBox->setToolTip(UIWizardNewVM::tr("<p>When checked, the guest will support the "
                                                       "Extended Firmware Interface (EFI), which is required to boot certain "
                                                       "guest OSes. Non-EFI aware OSes will not be able to boot if this option is activated.</p>"));
    }
}

QWidget *UIWizardNewVMPage8::createHardwareWidgets()
{
    QWidget *pHardwareContainer = new QWidget;
    QGridLayout *pHardwareLayout = new QGridLayout(pHardwareContainer);

    m_pBaseMemoryEditor = new UIBaseMemoryEditor(0, true);
    m_pVirtualCPUEditor = new UIVirtualCPUEditor(0, true);
    m_pEFICheckBox      = new QCheckBox;
    pHardwareLayout->addWidget(m_pBaseMemoryEditor, 0, 0, 1, 4);
    pHardwareLayout->addWidget(m_pVirtualCPUEditor, 1, 0, 1, 4);
    pHardwareLayout->addWidget(m_pEFICheckBox, 2, 0, 1, 1);

    return pHardwareContainer;
}

UIWizardNewVMPageBasic8::UIWizardNewVMPageBasic8()
    : m_pLabel(0)
{
    prepare();
    qRegisterMetaType<CMedium>();
    registerField("baseMemory", this, "baseMemory");
    registerField("VCPUCount", this, "VCPUCount");
    registerField("EFIEnabled", this, "EFIEnabled");
}

void UIWizardNewVMPageBasic8::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(createHardwareWidgets());

    pMainLayout->addStretch();
    createConnections();
}

void UIWizardNewVMPageBasic8::createConnections()
{
}

void UIWizardNewVMPageBasic8::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Hardware"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can modify virtual machine's hardware by changing amount of RAM and "
                                            "virtual CPU count.</p>"));

    retranslateWidgets();
}

void UIWizardNewVMPageBasic8::initializePage()
{
    retranslateUi();

    if (!field("type").canConvert<CGuestOSType>())
        return;

    CGuestOSType type = field("type").value<CGuestOSType>();
    ULONG recommendedRam = type.GetRecommendedRAM();
    if (m_pBaseMemoryEditor)
        m_pBaseMemoryEditor->setValue(recommendedRam);

    KFirmwareType fwType = type.GetRecommendedFirmware();
    if (m_pEFICheckBox)
        m_pEFICheckBox->setChecked(fwType != KFirmwareType_BIOS);
}

void UIWizardNewVMPageBasic8::cleanupPage()
{
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic8::isComplete() const
{
    return true;
}

bool UIWizardNewVMPageBasic8::validatePage()
{
    bool fResult = true;

    const QString strMediumPath(fieldImp("mediumPath").toString());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return fResult;
    }

    startProcessing();

    SelectedDiskSource enmDiskSource = field("selectedDiskSource").value<SelectedDiskSource>();
    if (enmDiskSource == SelectedDiskSource_New)
    {
        fResult = qobject_cast<UIWizardNewVM*>(wizard())->createVirtualDisk();
        fResult = UIWizardNewVDPage3::checkFATSizeLimitation(fieldImp("mediumVariant").toULongLong(),
                                                             fieldImp("mediumPath").toString(),
                                                             fieldImp("mediumSize").toULongLong());
        if (!fResult)
        {
            msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
            return fResult;
        }
    }
    fResult = qobject_cast<UIWizardNewVM*>(wizard())->createVM();
    endProcessing();

    return fResult;
}
