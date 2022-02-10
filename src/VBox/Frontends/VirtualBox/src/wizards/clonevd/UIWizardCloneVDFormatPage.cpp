/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDFormatPage class implementation.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIWizardCloneVDFormatPage.h"
#include "UIWizardCloneVD.h"
#include "UIWizardDiskEditors.h"
#include "UICommon.h"
#include "QIRichTextLabel.h"

/* COM includes: */
#include "CSystemProperties.h"

UIWizardCloneVDFormatPage::UIWizardCloneVDFormatPage(KDeviceType enmDeviceType)
    : m_pLabel(0)
    , m_pFormatGroupBox(0)
{
    prepare(enmDeviceType);
}

void UIWizardCloneVDFormatPage::prepare(KDeviceType enmDeviceType)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    m_pLabel = new QIRichTextLabel(this);
    if (m_pLabel)
        pMainLayout->addWidget(m_pLabel);
    m_pFormatGroupBox = new UIDiskFormatsGroupBox(false /* expert mode */, enmDeviceType, 0);
    if (m_pFormatGroupBox)
    {
        pMainLayout->addWidget(m_pFormatGroupBox);
        connect(m_pFormatGroupBox, &UIDiskFormatsGroupBox::sigMediumFormatChanged,
                this, &UIWizardCloneVDFormatPage::sltMediumFormatChanged);
    }
    pMainLayout->addStretch();
    retranslateUi();
}

void UIWizardCloneVDFormatPage::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Virtual Hard disk file type"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardCloneVD::tr("Please choose the type of file that you would like to use "
                                          "for the destination virtual disk image. If you do not need to use it "
                                          "with other virtualization software you can leave this setting unchanged."));
}

void UIWizardCloneVDFormatPage::initializePage()
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVD>());
    /* Translate page: */
    retranslateUi();
    if (!m_userModifiedParameters.contains("MediumFormat"))
    {
        if (m_pFormatGroupBox)
            wizardWindow<UIWizardCloneVD>()->setMediumFormat(m_pFormatGroupBox->mediumFormat());
    }
}

bool UIWizardCloneVDFormatPage::isComplete() const
{
    if (m_pFormatGroupBox)
    {
        if (m_pFormatGroupBox->mediumFormat().isNull())
            return false;
    }
    return true;
}

void UIWizardCloneVDFormatPage::sltMediumFormatChanged()
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVD>());
    if (m_pFormatGroupBox)
        wizardWindow<UIWizardCloneVD>()->setMediumFormat(m_pFormatGroupBox->mediumFormat());
    m_userModifiedParameters << "MediumFormat";
    emit completeChanged();
}
