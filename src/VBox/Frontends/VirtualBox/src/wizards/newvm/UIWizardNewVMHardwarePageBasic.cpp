/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMHardwarePageBasic class implementation.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMEditors.h"
#include "UIWizardNewVMHardwarePageBasic.h"

/* COM includes: */
#include "CGuestOSType.h"

UIWizardNewVMHardwarePageBasic::UIWizardNewVMHardwarePageBasic()
    : m_pLabel(0)
    , m_pHardwareWidgetContainer(0)
{
    prepare();
    qRegisterMetaType<CMedium>();
}

void UIWizardNewVMHardwarePageBasic::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    m_pHardwareWidgetContainer = new UINewVMHardwareContainer;
    AssertReturnVoid(m_pHardwareWidgetContainer);
    pMainLayout->addWidget(m_pHardwareWidgetContainer);

    pMainLayout->addStretch();
    createConnections();
}

void UIWizardNewVMHardwarePageBasic::createConnections()
{
    if (m_pHardwareWidgetContainer)
    {
        connect(m_pHardwareWidgetContainer, &UINewVMHardwareContainer::sigMemorySizeChanged,
                this, &UIWizardNewVMHardwarePageBasic::sltMemorySizeChanged);
        connect(m_pHardwareWidgetContainer, &UINewVMHardwareContainer::sigCPUCountChanged,
                this, &UIWizardNewVMHardwarePageBasic::sltCPUCountChanged);
        connect(m_pHardwareWidgetContainer, &UINewVMHardwareContainer::sigEFIEnabledChanged,
                this, &UIWizardNewVMHardwarePageBasic::sltEFIEnabledChanged);
    }
}

void UIWizardNewVMHardwarePageBasic::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Hardware"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can modify virtual machine's hardware by changing amount of RAM and "
                                            "virtual CPU count. Enabling EFI is also possible.</p>"));
}

void UIWizardNewVMHardwarePageBasic::initializePage()
{
    retranslateUi();

    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    if (pWizard && m_pHardwareWidgetContainer)
    {
        CGuestOSType type = pWizard->guestOSType();
        if (!type.isNull())
        {
            if (!m_userModifiedParameters.contains("MemorySize"))
            {
                ULONG recommendedRam = type.GetRecommendedRAM();
                m_pHardwareWidgetContainer->setMemorySize(recommendedRam);
            }
            if (!m_userModifiedParameters.contains("EFIEnabled"))
            {
                KFirmwareType fwType = type.GetRecommendedFirmware();
                m_pHardwareWidgetContainer->setEFIEnabled(fwType != KFirmwareType_BIOS);
            }
        }
    }
}

bool UIWizardNewVMHardwarePageBasic::isComplete() const
{
    return true;
}

void UIWizardNewVMHardwarePageBasic::sltMemorySizeChanged(int iValue)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setMemorySize(iValue);
    m_userModifiedParameters << "MemorySize";
}

void UIWizardNewVMHardwarePageBasic::sltCPUCountChanged(int iCount)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setCPUCount(iCount);
}

void UIWizardNewVMHardwarePageBasic::sltEFIEnabledChanged(bool fEnabled)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setEFIEnabled(fEnabled);
    m_userModifiedParameters << "EFIEnabled";
}
